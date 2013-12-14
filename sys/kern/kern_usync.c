/*
 * (MPSAFE)
 *
 * Copyright (c) 2003,2004,2010, 2013 The DragonFly Project.  All rights
 * reserved.
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
 * This module implements helper functions for system calls implementing
 * userland synchronization primitives (mutexes and semaphores).
 *
 * Userland represents such primitives as a user-space memory word manipulated
 * atomically and handles uncontested acquisitions (locking an unlocked mutex,
 * decrementing a positive semaphore) and releases without kernel support.
 *
 * Userland calls a primitive-specific syscall when acquiring a contented
 * primitive for blocking the calling lwp.  Similarly, userland calls another
 * syscall when releasing a contended primitive to wake up one or more waiters.
 *
 * These syscalls differ only by how contention is encoded in the atomic memory
 * word.  Contention must be checked kernel-side too to avoid missed-wakeup
 * races.  This module implements the format-agnostic part of these syscalls.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/sysunion.h>
#include <sys/sysent.h>
#include <sys/syscall.h>
#include <sys/usync.h>
#include <sys/module.h>
#include <sys/lock.h>

#include <cpu/lwbuf.h>

#include <machine/vmm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page2.h>

static void usync_sleep_page_action_cow(vm_page_t m, vm_page_action_t action);

/*
 * If this page is being copied it may no longer represent the page
 * underlying our virtual address.  Wake up any usync_sleep()'s
 * that were waiting on its physical address to force them to retry.
 */
static void
usync_sleep_page_action_cow(vm_page_t m, vm_page_action_t action)
{
    wakeup_domain(action->data, PDOMAIN_USYNC);
}

/*
 * Block the calling lwp if there is contention for the  user synchronization
 * primitive.
 *
 * If should_sleep() returns non-zero, enter an interruptable sleep for up to
 * timeout_us microseconds (or forever if timeout_us == 0).  If should_sleep()
 * returns zero then return immediately.
 *
 * The should_sleep() predicate is passed a kernel-side pointer matching uptr
 * and the caller-specific arg.  It returns whether there is still contention for
 * the synchronization primitive.  It may be called multiple times and so should
 * be idempotent.
 *
 * Return 0 if we slept and were woken up, EWOULDBLOCK if we slept and timed
 * out, EBUSY if should_sleep() returned zero, EINTR if the call was interrupted
 * by a signal (even if the signal specifies that the system call should
 * restart).
 *
 * This function interlocks against call to usync_wakeup.  It does NOT interlock
 * against changes in *uptr.  However, it does not have to as long as the
 * standard use described in the top comment is adhered to.  We can safely race
 * against changes in *uptr as long as we are properly interlocked against the
 * usync_wakeup() call.
 *
 * The VM page associated with uptr is held in an attempt to keep its physical
 * address consistent, allowing usync_sleep() and usync_wakeup() to use the
 * physical address as their rendezvous.  BUT situations can arise where the
 * physical address may change, particularly if a threaded program fork()'s and
 * the user sync primitive memory becomes copy-on-write.  We register an event
 * on the VM page to catch COWs.
 */
int
usync_sleep(volatile const int *uptr, int timeout_us,
		int (*should_sleep)(volatile const int *kptr, int arg),
		int arg, const char *wchan)
{
    struct lwbuf lwb_cache;
    struct lwbuf *lwb;
    struct vm_page_action action;
    vm_page_t m;
    void *waddr;
    volatile const int *kptr;
    int offset;
    int timeout_clk;
    int error = EBUSY;

    if (timeout_us < 0)
	return (EINVAL);

    if (curthread->td_vmm) {
	register_t gpa;
	vmm_vm_get_gpa(curproc, &gpa, (register_t) uptr);
	uptr = (const int *)gpa;
    }

    if ((vm_offset_t)uptr & (sizeof(int) - 1))
	return (EFAULT);

    /*
     * When faulting in the page, force any COW pages to be resolved.
     * Otherwise the physical page we sleep on my not match the page
     * being woken up.
     */
    m = vm_fault_page_quick((vm_offset_t)uptr,
			    VM_PROT_READ|VM_PROT_WRITE, &error);
    if (m == NULL) {
	error = EFAULT;
	goto done;
    }
    lwb = lwbuf_alloc(m, &lwb_cache);
    offset = (vm_offset_t)uptr & PAGE_MASK;
    kptr = (volatile int *)(lwbuf_kva(lwb) + offset);

    /*
     * The critical section is required to interlock the tsleep against
     * a wakeup from another cpu.  The lfence forces synchronization.
     */
    if (should_sleep(kptr, arg)) {
	if ((timeout_clk = timeout_us) != 0) {
	    timeout_clk = (timeout_clk / 1000000) * hz +
		      ((timeout_clk % 1000000) * hz + 999999) / 1000000;
	}
	waddr = (void *)((intptr_t)VM_PAGE_TO_PHYS(m) + offset);
	crit_enter();
	tsleep_interlock(waddr, PCATCH | PDOMAIN_USYNC);
	if (should_sleep(kptr, arg)) {
	    vm_page_init_action(m, &action, usync_sleep_page_action_cow, waddr);
	    vm_page_register_action(&action, VMEVENT_COW);
	    if (should_sleep(kptr, arg)) {
		    error = tsleep(waddr, PCATCH | PINTERLOCKED | PDOMAIN_USYNC,
				    wchan, timeout_clk);
	    } else {
		    error = EBUSY;
	    }
	    vm_page_unregister_action(&action);
	} else {
	    error = EBUSY;
	}
	crit_exit();
	/* Always break out in case of signal, even if restartable */
	if (error == ERESTART)
		error = EINTR;
    } else {
	error = EBUSY;
    }

    lwbuf_free(lwb);
    /*vm_page_dirty(m); we don't actually dirty the page */
    vm_page_unhold(m);
done:
    return(error);
}

/*
 * Wakeup the specified number of lwps held in usync_sleep() on the
 * specified user address.  A count of 0 wakes up all waiting processes.
 *
 * XXX assumes that the physical address space does not exceed the virtual
 * address space.
 */
int
usync_wakeup(volatile const int *uptr, int count)
{
    vm_page_t m;
    int offset;
    int error;
    void *waddr;

    if (curthread->td_vmm) {
	register_t gpa;
	vmm_vm_get_gpa(curproc, &gpa, (register_t) uptr);
	uptr = (const int *)gpa;
    }

    cpu_mfence();
    if ((vm_offset_t)uptr & (sizeof(int) - 1))
	return (EFAULT);
    m = vm_fault_page_quick((vm_offset_t)uptr, VM_PROT_READ, &error);
    if (m == NULL) {
	error = EFAULT;
	goto done;
    }
    offset = (vm_offset_t)uptr & PAGE_MASK;
    waddr = (void *)((intptr_t)VM_PAGE_TO_PHYS(m) + offset);

    if (count == 1) {
	wakeup_domain_one(waddr, PDOMAIN_USYNC);
    } else {
	/* XXX wakes them all up for now */
	wakeup_domain(waddr, PDOMAIN_USYNC);
    }
    vm_page_unhold(m);
    error = 0;
done:
    return(error);
}
