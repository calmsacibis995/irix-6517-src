#include <unistd.h>
#include <errno.h>
#include <limits.h>

#pragma weak __pthread_sysconf = ___pthread_sysconf
/*
 * sysconf for pthreads - this version will likely be preempted by
 * a more up to date one in libpthread.
 */

long
___pthread_sysconf(int name)
{
	switch(name) {
		/*
		 * POSIX1C - pthreads
		 */
		case _SC_THREAD_DESTRUCTOR_ITERATIONS:
			return _POSIX_THREAD_DESTRUCTOR_ITERATIONS;
		case _SC_THREAD_KEYS_MAX:
			return _POSIX_THREAD_KEYS_MAX;
		case _SC_THREAD_STACK_MIN:
			return 0;
		case _SC_THREAD_THREADS_MAX:
			return _POSIX_THREAD_THREADS_MAX;
		case _SC_THREADS:
#ifdef _POSIX_THREADS
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_ATTR_STACKADDR:
#ifdef _POSIX_THREAD_ATTR_STACKADDR
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_ATTR_STACKSIZE:
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_PRIORITY_SCHEDULING:
#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_PRIO_INHERIT:
#ifdef _POSIX_THREAD_PRIO_INHERIT
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_PRIO_PROTECT:
#ifdef _POSIX_THREAD_PRIO_PROTECT
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_PROCESS_SHARED:
#ifdef _POSIX_THREAD_PROCESS_SHARED
			return 1;
#else
			return -1;
#endif
	}
	return EINVAL;
}
