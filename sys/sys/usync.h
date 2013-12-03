#ifndef _SYS_USYNC_H_
#define _SYS_USYNC_H_

#ifndef _KERNEL
#error "This file should not be included by userland programs."
#else

int usync_sleep(volatile const int *uptr, int timeout_us,
		int (*should_sleep)(volatile const int *kptr, int arg),
		int arg, const char *wchan);
int usync_wakeup(volatile const int *uptr, int count);

#endif /* _KERNEL */

#endif
