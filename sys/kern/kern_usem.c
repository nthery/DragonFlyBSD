#include <sys/sysproto.h>
#include <sys/usync.h>

static int
usem_should_sleep(volatile const int *kptr, int dummy)
{
	return *kptr < 0;
}

int
sys_usem_sleep(struct usem_sleep_args *uap)
{
	return usync_sleep(uap->ptr, uap->timeout, usem_should_sleep, 0,
			"usemsl");
}

int
sys_usem_wakeup(struct usem_wakeup_args *uap)
{
	return usync_wakeup(uap->ptr, uap->count);
}
