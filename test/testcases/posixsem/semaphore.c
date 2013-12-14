#include <machine/atomic.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>

// TTT assert
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int _sem_value(sem_t *sem)
{
	return (int)atomic_load_acq_int(&sem->value);
}

/* Atomically decrement semaphore and return original value. */
static inline int _sem_postdec(sem_t *s)
{
	return (int)atomic_fetchadd_int(&s->value, (unsigned)-1);
}

/* Atomically increment semaphore and return original value. */
static inline int _sem_postinc(sem_t *s)
{
	return (int)atomic_fetchadd_int(&s->value, 1);
}

/*
 * Atomically increment semaphore and wake up waiters if possible
 * Return 0 on success, otherwise -1 and set errno.
 *
 * Must be async-signal safe.
 */
static int _sem_inc_wakeup(sem_t *s)
{
	int error = 0;
	if (_sem_postinc(s) == -1)
		error = usem_wakeup(&s->value, 0);
	return error;
}

/*
 * Subtract timespecs.  vvp -= uvp.
 * XXX stolen from kernel part of <sys/time.h>
 */
#define timespecsub(vvp, uvp)						\
	do {								\
		(vvp)->tv_sec -= (uvp)->tv_sec;				\
		(vvp)->tv_nsec -= (uvp)->tv_nsec;			\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)

// TTT address all occurrences
#define ASSERT_NO_ERROR(error) assert_no_error((error), __FILE__, __LINE__)

static void
assert_no_error(int error, const char *file, int line)
{
	if (error) {
		fprintf(stderr,
		    "%s:%d: not implemented yet: error: %d errno: %s\n",
		    file, line, error, strerror(errno));
		abort();
	}
}

/*
 * We keep per-process data for named semaphores as the standard require that
 * when a process calls sem_open() several times, all calls shall return the
 * same semaphore.
 */
static pthread_mutex_t named_sem_lock = PTHREAD_MUTEX_INITIALIZER;

static LIST_HEAD(named_sem_head, named_sem) named_sem_head = 
    LIST_HEAD_INITIALIZER(named_sem_head);

struct named_sem {
	LIST_ENTRY(named_sem) link;
	int	open_count;
	sem_t	*sem;
	char	name[NAME_MAX+1];
	bool	unlinked;
};

/*
 * Named semaphores are regular files in this directory which must be
 * rwx'able by everyone because it will be created by a process with 
 * random credentials.  We set the sticky bits to avoid malicious
 * semaphore destruction.
 */
static const char _sem_dir[] = "/tmp/posix_semaphores";
#define _SEM_DIR_PERMS (S_IRWXU|S_IRWXG|S_IRWXO|S_ISTXT)

static pthread_once_t _sem_proc_init_once;
static int _sem_proc_init_errno;

/* Initialize per-process data structures. */
static void
_sem_proc_init(void)
{
	int error;

	/*
	 * Create the directory storing named semaphores if not already done by
	 * another process.
	 */
	error = mkdir(_sem_dir, _SEM_DIR_PERMS);
	if (error) {
		if (errno != EEXIST)
			_sem_proc_init_errno = errno;
		return;
	}

	/*
	 * mkdir(2) does not allow to set all mode bits and is subject to
	 * umask so let's set the wanted mode.
	 */
	error = chmod(_sem_dir, _SEM_DIR_PERMS);
	if (error) {
		_sem_proc_init_errno = errno;
		rmdir(_sem_dir);
		return;
	}
}

int
sem_init(sem_t *sem, int pshared, unsigned int value)
{
	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return -1;
	}

	sem->type = SEM_UNNAMED;
	atomic_store_rel_int(&sem->value, value);

	return 0;
}

int
sem_destroy(sem_t *s)
{
	if (s->type != SEM_UNNAMED) {
		errno = EINVAL;
		return -1;
	}
	if (_sem_value(s) < 0) {
		errno = EBUSY;
		return -1;
	}
	return 0;
}

sem_t*
sem_open(const char *name, int flags, ...)
{
	char path[PATH_MAX+1];
	va_list args;
	sem_t *sem = SEM_FAILED;
	struct named_sem *ns;
	mode_t mode;
	unsigned int init_value;
	int fd;
	int error;

	/*
	 * Supporting O_RDONLY and O_RDWR is not required by standard.  We
	 * ignore them for compatibility with fbsd and potentially other OSes.
	 */
	flags &= ~O_RDONLY|O_RDWR;
	if (flags & ~(O_CREAT|O_EXCL)) {
		errno = EINVAL;
		return SEM_FAILED;
	}


	if (name[0] != '/') {
		errno = EINVAL;
		return SEM_FAILED;
	}

	pthread_once(&_sem_proc_init_once, _sem_proc_init);
	if (_sem_proc_init_errno) {
		errno = _sem_proc_init_errno;
		return SEM_FAILED;
	}

	error = pthread_mutex_lock(&named_sem_lock);
	if (error)
		return SEM_FAILED;

	/*
	 * If semaphore with same name already opened by this process,
	 * return it.
	 */
	LIST_FOREACH(ns, &named_sem_head, link) {
		// TTT cmp using inode num
		if (strcmp(ns->name, name) == 0 && !ns->unlinked) {
			ns->open_count++;
			sem = ns->sem;
			goto unlock;
		}
	}

	// TTT handle error
	snprintf(path, sizeof(path), "%s%s", _sem_dir, name);
	strlcpy(path, _sem_dir, sizeof(path));
	strlcat(path, _sem_dir, sizeof(path));

	/*
	 * Create or open the semaphore file.
	 * Take an exclusive lock on the file to prevent a race between
	 * a thread creating a semaphore and another one opening it before
	 * the creator had time to initialize the sem_t object.
	 */
	if (flags & O_CREAT) {
		// TTT validate these args
		va_start(args, flags);
		mode = va_arg(args, int); /* mode_t */
		init_value = va_arg(args, unsigned int);
		va_end(args);

		// TTT close
		// TTT how to handle O_CREAT w/o O_EXCL?
		// TTT check flags
		fd = open(path, flags|O_RDWR|O_EXLOCK, mode);
		// TTT rollback
		if (fd < 0)
			goto unlock;

		error = ftruncate(fd, sizeof(sem_t));
		ASSERT_NO_ERROR(error);
	} else {
		// TTT close
		fd = open(path, O_RDWR|O_EXLOCK, 0);
		// TTT rollback
		if (fd < 0)
			goto unlock;
	}

	// TTT unmap
	sem = mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE,
	    MAP_SHARED|MAP_NOSYNC, fd, 0);
	assert(sem != MAP_FAILED); // TTT

	if (flags & O_CREAT) {
		// TTT use sem_init_common()
		error = sem_init(sem, 1, init_value);
		if (error)
			goto unmap_file;
		sem->type = SEM_NAMED;
	}

	// TTT free
	ns = malloc(sizeof(struct named_sem));
	assert(ns != NULL); // TTT

	/* open() would have failed earlier if name were too long. */
	strlcpy(ns->name, name, sizeof(ns->name));
	ns->open_count = 1;
	ns->sem = sem;
	ns->unlinked = false;
	LIST_INSERT_HEAD(&named_sem_head, ns, link);

	goto close_file;

unmap_file:
	munmap(sem, sizeof(sem_t));
	sem = SEM_FAILED;
close_file:
	/* This also releases lock taken at open-time. */
	close(fd);
unlock:
	pthread_mutex_unlock(&named_sem_lock);

	return sem;
}

int
sem_close(sem_t *sem)
{
	struct named_sem *ns;
	int error;

	if (sem->type != SEM_NAMED) {
		errno = EINVAL;
		return -1;
	}

	error = pthread_mutex_lock(&named_sem_lock);
	ASSERT_NO_ERROR(error);

	LIST_FOREACH(ns, &named_sem_head, link) {
		if (ns->sem == sem) {
			if (--ns->open_count == 0) {
				LIST_REMOVE(ns, link);
				pthread_mutex_unlock(&named_sem_lock);
				free(ns);
				error = munmap(sem, sizeof(sem_t));
				ASSERT_NO_ERROR(error);
			} else {
				pthread_mutex_unlock(&named_sem_lock);
			}
			return 0;
		}
	}

	pthread_mutex_unlock(&named_sem_lock);

	return 0;
}

int
sem_unlink(const char *name)
{
	char path[PATH_MAX];
	struct named_sem *ns;
	int error;

	error = pthread_mutex_lock(&named_sem_lock);
	ASSERT_NO_ERROR(error);

	LIST_FOREACH(ns, &named_sem_head, link) {
		if (strcmp(ns->name, name) == 0 && !ns->unlinked) {
			ns->unlinked = true;
			break;
		}
	}

	error = pthread_mutex_unlock(&named_sem_lock);
	ASSERT_NO_ERROR(error);

	// TTT handle error
	snprintf(path, sizeof(path), "%s%s", _sem_dir, name);

	return unlink(path);
}

int
sem_wait(sem_t *s)
{
	return sem_timedwait(s, NULL);
}

// TTT use atomic magic to dec iff positive to save wakeup syscall
int
sem_trywait(sem_t *s)
{
	if (_sem_postdec(s) <= 0) {
		if (_sem_inc_wakeup(s) == 0)
			errno = EAGAIN;
		return -1;
	}

	return 0;
}

int
sem_timedwait(sem_t * __restrict sem,
    const struct timespec * __restrict abs_timeout)
{
	struct timespec rel_timeout;
	struct timespec now;
	int error;
	int timeout_us;

	if (_sem_postdec(sem) <= 0) {
		// recompute timeout if early exit
		if (abs_timeout == NULL) {
			timeout_us = 0;
		} else {
			// TTT check abs_timeout bounds
			error = clock_gettime(CLOCK_REALTIME, &now);
			ASSERT_NO_ERROR(error);
			rel_timeout = *abs_timeout;
			timespecsub(&rel_timeout, &now);
			if (rel_timeout.tv_sec < 0) {
				errno = ETIMEDOUT;
				return -1;
			}
			// TTT overflow?
			timeout_us = rel_timeout.tv_sec * 1000000 +
					(rel_timeout.tv_nsec + 999) / 1000;
		}

		while (_sem_value(sem) < 0) {
			// TTT recompute timeout
			// TTT handle errors
			error = usem_sleep(&sem->value, timeout_us);
			if (error) {
				switch (errno) {
				case EINTR:
					// TTT do not always restart?
					continue;
				case EBUSY:
					continue;
				case EWOULDBLOCK:
					if (_sem_inc_wakeup(sem) == 0)
						errno = ETIMEDOUT;
					return -1;
				}
			}
			ASSERT_NO_ERROR(error);
		}
	}

	return 0;
}

/*
 * Must be async-signal safe.
 */
int
sem_post(sem_t *s)
{
	return _sem_inc_wakeup(s);
}

int
sem_getvalue(sem_t * __restrict sem, int * __restrict value)
{
	*value = _sem_value(sem);
	return 0;
}
