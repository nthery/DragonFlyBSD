/*
 * (MPSAFE)
 *
 * Copyright (c) 2003,2004,2010 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com> and David Xu <davidxu@freebsd.org>
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
 */

/*
 * This module implements userland mutex helper functions.  umtx_sleep()
 * handling blocking and umtx_wakeup() handles wakeups.  The sleep/wakeup
 * functions operate on user addresses.
 */

#include <sys/sysproto.h>
#include <sys/usync.h>

/*
 * Is the user-side mutex pointed to by kptr locked?
 */
static int
umtx_should_sleep(volatile const int *kptr, int value)
{
	return *kptr == value;
}

/*
 * umtx_sleep { const int *ptr, int value, int timeout }
 *
 * Block calling lwp if userland mutex held.
 */
int
sys_umtx_sleep(struct umtx_sleep_args *uap)
{
	return usync_sleep(uap->ptr, uap->timeout, umtx_should_sleep, 0,
			"umtxsl");
}

/*
 * umtx_wakeup { const int *ptr, int count }
 *
 * Wakeup the specified number of lwps waiting for the mutex pointed to by ptr.
 * A count of 0 wakes up all waiting processes.
 *
 * XXX assumes that the physical address space does not exceed the virtual
 * address space.
 */
int
sys_umtx_wakeup(struct umtx_wakeup_args *uap)
{
	return usync_wakeup(uap->ptr, uap->count);
}
