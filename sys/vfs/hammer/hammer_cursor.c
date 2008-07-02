/*
 * Copyright (c) 2007-2008 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $DragonFly: src/sys/vfs/hammer/hammer_cursor.c,v 1.35 2008/07/02 21:57:54 dillon Exp $
 */

/*
 * HAMMER B-Tree index - cursor support routines
 */
#include "hammer.h"

static int hammer_load_cursor_parent(hammer_cursor_t cursor, int try_exclusive);

/*
 * Initialize a fresh cursor using the B-Tree node cache.  If the cache
 * is not available initialize a fresh cursor at the root of the filesystem.
 */
int
hammer_init_cursor(hammer_transaction_t trans, hammer_cursor_t cursor,
		   hammer_node_cache_t cache, hammer_inode_t ip)
{
	hammer_volume_t volume;
	hammer_node_t node;
	int error;

	bzero(cursor, sizeof(*cursor));

	cursor->trans = trans;

	/*
	 * If the cursor operation is on behalf of an inode, lock
	 * the inode.
	 */
	if ((cursor->ip = ip) != NULL) {
		++ip->cursor_ip_refs;
		if (trans->type == HAMMER_TRANS_FLS)
			hammer_lock_ex(&ip->lock);
		else
			hammer_lock_sh(&ip->lock);
	}

	/*
	 * Step 1 - acquire a locked node from the cache if possible
	 */
	if (cache && cache->node) {
		node = hammer_ref_node_safe(trans->hmp, cache, &error);
		if (error == 0) {
			hammer_lock_sh_lowpri(&node->lock);
			if (node->flags & HAMMER_NODE_DELETED) {
				hammer_unlock(&node->lock);
				hammer_rel_node(node);
				node = NULL;
			}
		}
	} else {
		node = NULL;
	}

	/*
	 * Step 2 - If we couldn't get a node from the cache, get
	 * the one from the root of the filesystem.
	 */
	while (node == NULL) {
		volume = hammer_get_root_volume(trans->hmp, &error);
		if (error)
			break;
		node = hammer_get_node(trans->hmp,
				       volume->ondisk->vol0_btree_root,
				       0, &error);
		hammer_rel_volume(volume, 0);
		if (error)
			break;
		hammer_lock_sh_lowpri(&node->lock);

		/*
		 * If someone got in before we could lock the node, retry.
		 */
		if (node->flags & HAMMER_NODE_DELETED) {
			hammer_unlock(&node->lock);
			hammer_rel_node(node);
			node = NULL;
			continue;
		}
		if (volume->ondisk->vol0_btree_root != node->node_offset) {
			hammer_unlock(&node->lock);
			hammer_rel_node(node);
			node = NULL;
			continue;
		}
	}

	/*
	 * Step 3 - finish initializing the cursor by acquiring the parent
	 */
	cursor->node = node;
	if (error == 0)
		error = hammer_load_cursor_parent(cursor, 0);
	KKASSERT(error == 0);
	/* if (error) hammer_done_cursor(cursor); */
	return(error);
}

/*
 * Normalize a cursor.  Sometimes cursors can be left in a state
 * where node is NULL.  If the cursor is in this state, cursor up.
 */
void
hammer_normalize_cursor(hammer_cursor_t cursor)
{
	if (cursor->node == NULL) {
		KKASSERT(cursor->parent != NULL);
		hammer_cursor_up(cursor);
	}
}


/*
 * We are finished with a cursor.  We NULL out various fields as sanity
 * check, in case the structure is inappropriately used afterwords.
 */
void
hammer_done_cursor(hammer_cursor_t cursor)
{
	hammer_inode_t ip;

	if (cursor->parent) {
		hammer_unlock(&cursor->parent->lock);
		hammer_rel_node(cursor->parent);
		cursor->parent = NULL;
	}
	if (cursor->node) {
		hammer_unlock(&cursor->node->lock);
		hammer_rel_node(cursor->node);
		cursor->node = NULL;
	}
        if (cursor->data_buffer) {
                hammer_rel_buffer(cursor->data_buffer, 0);
                cursor->data_buffer = NULL;
        }
        if (cursor->record_buffer) {
                hammer_rel_buffer(cursor->record_buffer, 0);
                cursor->record_buffer = NULL;
        }
	if ((ip = cursor->ip) != NULL) {
		hammer_mem_done(cursor);
                KKASSERT(ip->cursor_ip_refs > 0);
                --ip->cursor_ip_refs;
#if 1
		hammer_unlock(&ip->lock);
#endif
                cursor->ip = NULL;
        }


	/*
	 * If we deadlocked this node will be referenced.  Do a quick
	 * lock/unlock to wait for the deadlock condition to clear.
	 */
	if (cursor->deadlk_node) {
		hammer_lock_ex_ident(&cursor->deadlk_node->lock, "hmrdlk");
		hammer_unlock(&cursor->deadlk_node->lock);
		hammer_rel_node(cursor->deadlk_node);
		cursor->deadlk_node = NULL;
	}
	if (cursor->deadlk_rec) {
		hammer_wait_mem_record_ident(cursor->deadlk_rec, "hmmdlr");
		hammer_rel_mem_record(cursor->deadlk_rec);
		cursor->deadlk_rec = NULL;
	}

	cursor->data = NULL;
	cursor->leaf = NULL;
	cursor->left_bound = NULL;
	cursor->right_bound = NULL;
	cursor->trans = NULL;
}

/*
 * Upgrade cursor->node and cursor->parent to exclusive locks.  This
 * function can return EDEADLK.
 *
 * The lock must already be either held shared or already held exclusively
 * by us.
 *
 * If we fail to upgrade the lock and cursor->deadlk_node is NULL, 
 * we add another reference to the node that failed and set
 * cursor->deadlk_node so hammer_done_cursor() can block on it.
 */
int
hammer_cursor_upgrade(hammer_cursor_t cursor)
{
	int error;

	error = hammer_lock_upgrade(&cursor->node->lock);
	if (error && cursor->deadlk_node == NULL) {
		cursor->deadlk_node = cursor->node;
		hammer_ref_node(cursor->deadlk_node);
	} else if (error == 0 && cursor->parent) {
		error = hammer_lock_upgrade(&cursor->parent->lock);
		if (error && cursor->deadlk_node == NULL) {
			cursor->deadlk_node = cursor->parent;
			hammer_ref_node(cursor->deadlk_node);
		}
	}
	return(error);
}

int
hammer_cursor_upgrade_node(hammer_cursor_t cursor)
{
	int error;

	error = hammer_lock_upgrade(&cursor->node->lock);
	if (error && cursor->deadlk_node == NULL) {
		cursor->deadlk_node = cursor->node;
		hammer_ref_node(cursor->deadlk_node);
	}
	return(error);
}

/*
 * Downgrade cursor->node and cursor->parent to shared locks.  This
 * function can return EDEADLK.
 */
void
hammer_cursor_downgrade(hammer_cursor_t cursor)
{
	if (hammer_lock_excl_owned(&cursor->node->lock, curthread))
		hammer_lock_downgrade(&cursor->node->lock);
	if (cursor->parent &&
	    hammer_lock_excl_owned(&cursor->parent->lock, curthread)) {
		hammer_lock_downgrade(&cursor->parent->lock);
	}
}

/*
 * Seek the cursor to the specified node and index.
 *
 * The caller must ref the node prior to calling this routine and release
 * it after it returns.  If the seek succeeds the cursor will gain its own
 * ref on the node.
 */
int
hammer_cursor_seek(hammer_cursor_t cursor, hammer_node_t node, int index)
{
	int error;

	hammer_cursor_downgrade(cursor);
	error = 0;

	if (cursor->node != node) {
		hammer_unlock(&cursor->node->lock);
		hammer_rel_node(cursor->node);
		cursor->node = node;
		hammer_ref_node(node);
		hammer_lock_sh(&node->lock);
		KKASSERT ((node->flags & HAMMER_NODE_DELETED) == 0);

		if (cursor->parent) {
			hammer_unlock(&cursor->parent->lock);
			hammer_rel_node(cursor->parent);
			cursor->parent = NULL;
			cursor->parent_index = 0;
		}
		error = hammer_load_cursor_parent(cursor, 0);
	}
	cursor->index = index;
	return (error);
}

/*
 * Load the parent of cursor->node into cursor->parent.
 */
static
int
hammer_load_cursor_parent(hammer_cursor_t cursor, int try_exclusive)
{
	hammer_mount_t hmp;
	hammer_node_t parent;
	hammer_node_t node;
	hammer_btree_elm_t elm;
	int error;
	int parent_index;

	hmp = cursor->trans->hmp;

	if (cursor->node->ondisk->parent) {
		node = cursor->node;
		parent = hammer_btree_get_parent(node, &parent_index,
						 &error, try_exclusive);
		if (error == 0) {
			elm = &parent->ondisk->elms[parent_index];
			cursor->parent = parent;
			cursor->parent_index = parent_index;
			cursor->left_bound = &elm[0].internal.base;
			cursor->right_bound = &elm[1].internal.base;
		}
	} else {
		cursor->parent = NULL;
		cursor->parent_index = 0;
		cursor->left_bound = &hmp->root_btree_beg;
		cursor->right_bound = &hmp->root_btree_end;
		error = 0;
	}
	return(error);
}

/*
 * Cursor up to our parent node.  Return ENOENT if we are at the root of
 * the filesystem.
 */
int
hammer_cursor_up(hammer_cursor_t cursor)
{
	int error;

	hammer_cursor_downgrade(cursor);

	/*
	 * If the parent is NULL we are at the root of the B-Tree and
	 * return ENOENT.
	 */
	if (cursor->parent == NULL)
		return (ENOENT);

	/*
	 * Set the node to its parent. 
	 */
	hammer_unlock(&cursor->node->lock);
	hammer_rel_node(cursor->node);
	cursor->node = cursor->parent;
	cursor->index = cursor->parent_index;
	cursor->parent = NULL;
	cursor->parent_index = 0;

	error = hammer_load_cursor_parent(cursor, 0);
	return(error);
}

/*
 * Special cursor up given a locked cursor.  The orignal node is not
 * unlocked and released and the cursor is not downgraded.  If we are
 * unable to acquire and lock the parent, EDEADLK is returned.
 */
int
hammer_cursor_up_locked(hammer_cursor_t cursor)
{
	hammer_node_t save;
	int error;

	/*
	 * If the parent is NULL we are at the root of the B-Tree and
	 * return ENOENT.
	 */
	if (cursor->parent == NULL)
		return (ENOENT);

	save = cursor->node;

	/*
	 * Set the node to its parent. 
	 */
	cursor->node = cursor->parent;
	cursor->index = cursor->parent_index;
	cursor->parent = NULL;
	cursor->parent_index = 0;

	/*
	 * load the new parent, attempt to exclusively lock it.  Note that
	 * we are still holding the old parent (now cursor->node) exclusively
	 * locked.  This can return EDEADLK.
	 */
	error = hammer_load_cursor_parent(cursor, 1);
	if (error) {
		cursor->parent = cursor->node;
		cursor->parent_index = cursor->index;
		cursor->node = save;
		cursor->index = 0;
	}
	return(error);
}


/*
 * Cursor down through the current node, which must be an internal node.
 *
 * This routine adjusts the cursor and sets index to 0.
 */
int
hammer_cursor_down(hammer_cursor_t cursor)
{
	hammer_node_t node;
	hammer_btree_elm_t elm;
	int error;

	/*
	 * The current node becomes the current parent
	 */
	hammer_cursor_downgrade(cursor);
	node = cursor->node;
	KKASSERT(cursor->index >= 0 && cursor->index < node->ondisk->count);
	if (cursor->parent) {
		hammer_unlock(&cursor->parent->lock);
		hammer_rel_node(cursor->parent);
	}
	cursor->parent = node;
	cursor->parent_index = cursor->index;
	cursor->node = NULL;
	cursor->index = 0;

	/*
	 * Extract element to push into at (node,index), set bounds.
	 */
	elm = &node->ondisk->elms[cursor->parent_index];

	/*
	 * Ok, push down into elm.  If elm specifies an internal or leaf
	 * node the current node must be an internal node.  If elm specifies
	 * a spike then the current node must be a leaf node.
	 */
	switch(elm->base.btype) {
	case HAMMER_BTREE_TYPE_INTERNAL:
	case HAMMER_BTREE_TYPE_LEAF:
		KKASSERT(node->ondisk->type == HAMMER_BTREE_TYPE_INTERNAL);
		KKASSERT(elm->internal.subtree_offset != 0);
		cursor->left_bound = &elm[0].internal.base;
		cursor->right_bound = &elm[1].internal.base;
		node = hammer_get_node(cursor->trans->hmp,
				       elm->internal.subtree_offset, 0, &error);
		if (error == 0) {
			KASSERT(elm->base.btype == node->ondisk->type, ("BTYPE MISMATCH %c %c NODE %p\n", elm->base.btype, node->ondisk->type, node));
			if (node->ondisk->parent != cursor->parent->node_offset)
				panic("node %p %016llx vs %016llx\n", node, node->ondisk->parent, cursor->parent->node_offset);
			KKASSERT(node->ondisk->parent == cursor->parent->node_offset);
		}
		break;
	default:
		panic("hammer_cursor_down: illegal btype %02x (%c)\n",
		      elm->base.btype,
		      (elm->base.btype ? elm->base.btype : '?'));
		break;
	}
	if (error == 0) {
		hammer_lock_sh(&node->lock);
		KKASSERT ((node->flags & HAMMER_NODE_DELETED) == 0);
		cursor->node = node;
		cursor->index = 0;
	}
	return(error);
}

