/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)param.h	8.3 (Berkeley) 4/4/95
 * $FreeBSD: src/sys/sys/param.h,v 1.61.2.38 2003/05/22 17:12:01 fjoe Exp $
 * $DragonFly: src/sys/sys/param.h,v 1.53 2008/11/11 00:55:49 pavalos Exp $
 */

#ifndef _SYS_PARAM_H_
#define _SYS_PARAM_H_

#define BSD	200708		/* XXX kern.osrevision */
#define BSD4_3	1		/* XXX obsolete */
#define BSD4_4	1		/* XXX obsolete */

/*
 * __DragonFly_version number.  The number doesn't really meaningfully
 * translate to a version number.
 *
 * 170000 - base development version after 1.6 branch
 * 170001 - base development version before 1.8 branch
 * 190000 - base development version after 1.8 branch
 * 195000 - base development version after 1.10 branch
 * 197500 - base development version before 1.12 branch
 * 197600 - 1.12 branch
 * 197700 - base development version after 1.12 branch
 * 199900 - base development version before 2.0 branch
 * 200000 - 2.0 branch
 * 200100 - base development version after 2.0 branch
 * 200101 - lchflags syscall
 * 200200 - 2.2 branch
 * 200201 - base development version after 2.2 branch
 * 200202 - major changes to libc
 * 200203 - introduce PCI domain
 * 200204 - suser() & suser_cred() removal
 * 200205 - devfs import
 * 200206 - *sleep() renames
 * 200400 - 2.4 branch
 * 200500 - 2.5 master
 * 200600 - 2.6 branch
 * 200700 - 2.7 master
 */
#undef __DragonFly_version
#define __DragonFly_version 200600	/* propagated to newvers */

#ifdef __FreeBSD__
/* 
 * __FreeBSD_version numbers for source compatibility.  This is temporary
 * along with the __FreeBSD__ define in gcc2.  Note that gcc3 does not 
 * define __FreeBSD__ by default, only __DragonFly__.
 */
#undef __FreeBSD_version
#define __FreeBSD_version 480101
#endif

#include <sys/_null.h>

#ifndef LOCORE
#include <sys/types.h>
#endif

/*
 * Machine-independent constants (some used in following include files).
 * Redefined constants are from POSIX 1003.1 limits file.
 *
 * MAXCOMLEN should be >= sizeof(ac_comm) (see <acct.h>)
 * MAXLOGNAME should be == UT_NAMESIZE+1 (see <utmp.h>)
 */
#include <sys/syslimits.h>

#define MAXCOMLEN	16		/* max command name remembered */
#define MAXINTERP	32		/* max interpreter file name length */
#define MAXLOGNAME	17		/* max login name length (incl. NUL) */
#define MAXUPRC		CHILD_MAX	/* max simultaneous processes */
#define NCARGS		ARG_MAX		/* max bytes for an exec function */
#define NGROUPS		NGROUPS_MAX	/* max number groups */
#define NOFILE		OPEN_MAX	/* max open files per process */
#define NOGROUP		65535		/* marker for empty group set member */
#define MAXHOSTNAMELEN	256		/* max hostname size */

/* More types and definitions used throughout the kernel. */
#ifdef _KERNEL
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/time.h>

#define FALSE	0
#define TRUE	1
#endif

#ifndef _KERNEL
/* Signals. */
#include <sys/select.h>
#include <sys/signal.h>
#endif

/* Machine type dependent parameters. */
#include <machine/param.h>
#ifndef _KERNEL
#include <machine/limits.h>
#endif

#define PCATCH		0x00000100	/* tsleep checks signals */
#define PUSRFLAG1	0x00000200	/* Subsystem specific flag */
#define PINTERLOCKED	0x00000400	/* Interlocked tsleep */
#define PWAKEUP_CPUMASK	0x00003FFF	/* start cpu for chained wakeups */
#define PWAKEUP_MYCPU	0x00004000	/* wakeup on current cpu only */
#define PWAKEUP_ONE	0x00008000	/* argument to wakeup: only one */
#define PDOMAIN_MASK	0xFFFF0000	/* address domains for wakeup */
#define PDOMAIN_UMTX	0x00010000	/* independant domain for UMTX */
#define PWAKEUP_ENCODE(domain, cpu)	((domain) | (cpu))
#define PWAKEUP_DECODE(domain)		((domain) & PWAKEUP_CPUMASK)

#define NZERO	0		/* default "nice" */

#define NBPW	sizeof(int)	/* number of bytes per word (integer) */

#define CMASK	022		/* default file mask: S_IWGRP|S_IWOTH */
#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)
#define NOUDEV	(udev_t)(-1)	/* non-existent device */
#define NOMAJ	256		/* non-existent device */
#endif

#ifndef _KERNEL
#define NODEV	(dev_t)(-1)	/* non-existent device */
#endif

/*
 * File system parameters and macros.
 *
 * MAXBSIZE -	Filesystems are made out of blocks of at most MAXBSIZE bytes
 *		per block.  MAXBSIZE may be made larger without effecting
 *		any existing filesystems as long as it does not exceed MAXPHYS,
 *		and may be made smaller at the risk of not being able to use
 *		filesystems which require a block size exceeding MAXBSIZE.
 *
 * BKVASIZE -	Nominal buffer space per buffer, in bytes.  BKVASIZE is the
 *		minimum KVM memory reservation the kernel is willing to make.
 *		Filesystems can of course request smaller chunks.  Actual 
 *		backing memory uses a chunk size of a page (PAGE_SIZE).
 *
 *		If you make BKVASIZE too small you risk seriously fragmenting
 *		the buffer KVM map which may slow things down a bit.  If you
 *		make it too big the kernel will not be able to optimally use 
 *		the KVM memory reserved for the buffer cache and will wind 
 *		up with too-few buffers.
 *
 *		The default is 16384, roughly 2x the block size used by a
 *		normal UFS filesystem.
 */
#define MAXBSIZE	65536	/* must be power of 2 */
#define BKVASIZE	16384	/* must be power of 2 */
#define BKVAMASK	(BKVASIZE-1)
#define MAXFRAG 	8

/*
 * MAXPATHLEN defines the longest permissible path length after expanding
 * symbolic links. It is used to allocate a temporary buffer from the buffer
 * pool in which to do the name expansion, hence should be a power of two,
 * and must be less than or equal to MAXBSIZE.  MAXSYMLINKS defines the
 * maximum number of symbolic links that may be expanded in a path name.
 * It should be set high enough to allow all legitimate uses, but halt
 * infinite loops reasonably quickly.
 */
#define MAXPATHLEN	PATH_MAX
#define MAXSYMLINKS	32

/* Bit map related macros. */
#define setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

/* Macros for counting and rounding. */
#ifndef howmany
#define howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define rounddown(x, y)	(((x)/(y))*(y))
#define roundup(x, y)	((((x)+((y)-1))/(y))*(y))  /* to any y */
#define roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */
#define powerof2(x)	((((x)-1)&(x))==0)

/*
 * VM objects can be larger than the virtual address space, make sure
 * we don't cut-off the mask.
 */
#define trunc_page64(x)           ((x) & ~(int64_t)PAGE_MASK)
#define round_page64(x)           (((x) + PAGE_MASK) & ~(int64_t)PAGE_MASK)

/* Macros for min/max. */
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/*
 * Constants for setting the parameters of the kernel memory allocator.
 *
 * 2 ** MINBUCKET is the smallest unit of memory that will be
 * allocated. It must be at least large enough to hold a pointer.
 *
 * Units of memory less or equal to MAXALLOCSAVE will permanently
 * allocate physical memory; requests for these size pieces of
 * memory are quite fast. Allocations greater than MAXALLOCSAVE must
 * always allocate and free physical memory; requests for these
 * size allocations should be done infrequently as they will be slow.
 *
 * Constraints: PAGE_SIZE <= MAXALLOCSAVE <= 2 ** (MINBUCKET + 14), and
 * MAXALLOCSIZE must be a power of two.
 */
#define MINBUCKET	4		/* 4 => min allocation of 16 bytes */
#define MAXALLOCSAVE	(2 * PAGE_SIZE)

/*
 * Scale factor for scaled integers used to count %cpu time and load avgs.
 *
 * The number of CPU `tick's that map to a unique `%age' can be expressed
 * by the formula (1 / (2 ^ (FSHIFT - 11))).  The maximum load average that
 * can be calculated (assuming 32 bits) can be closely approximated using
 * the formula (2 ^ (2 * (16 - FSHIFT))) for (FSHIFT < 15).
 *
 * For the scheduler to maintain a 1:1 mapping of CPU `tick' to `%age',
 * FSHIFT must be at least 11; this gives us a maximum load avg of ~1024.
 */
#define FSHIFT	11		/* bits to right of fixed binary point */
#define FSCALE	(1<<FSHIFT)

#define dbtoc(db)			/* calculates devblks to pages */ \
	((db + (ctodb(1) - 1)) >> (PAGE_SHIFT - DEV_BSHIFT))
 
#define ctodb(db)			/* calculates pages to devblks */ \
	((db) << (PAGE_SHIFT - DEV_BSHIFT))

#define MJUMPAGESIZE	PAGE_SIZE	/* jumbo cluster 4k */
#define MJUM9BYTES	(9 * 1024)	/* jumbo cluster 9k */
#define MJUM16BYTES	(16 * 1024)	/* jumbo cluster 16k */
/*
 * Make this available for most of the kernel.  There were too many
 * things that included sys/systm.h just for panic().
 */
#ifdef _KERNEL
void	panic (const char *, ...) __dead2 __printflike(1, 2);
#endif

#ifndef htonl
#define	htonl(x)	__htonl(x)
#endif
#ifndef htons
#define	htons(x)	__htons(x)
#endif
#ifndef ntohl
#define	ntohl(x)	__ntohl(x)
#endif
#ifndef ntohs
#define	ntohs(x)	__ntohs(x)
#endif

#endif	/* _SYS_PARAM_H_ */
