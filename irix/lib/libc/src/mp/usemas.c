/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 3.66 $ $Author: joecd $"

#ifdef __STDC__
	#pragma weak uspsema = _uspsema
	#pragma weak uscpsema = _uscpsema
	#pragma weak usvsema = _usvsema
	#pragma weak ustestsema = _ustestsema
	#pragma weak usnewsema = _usnewsema
	#pragma weak usinitsema = _usinitsema
	#pragma weak usfreesema = _usfreesema
	#pragma weak usctlsema = _usctlsema
	#pragma weak usdumpsema = _usdumpsema
	#pragma weak usdumphist = _usdumphist
	#pragma weak usopenpollsema = _usopenpollsema
	#pragma weak usclosepollsema = _usclosepollsema
	#pragma weak usnewpollsema = _usnewpollsema
	#pragma weak usfreepollsema = _usfreepollsema
#endif

#include "synonyms.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <shlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <ulocks.h>
#include <sys/usioctl.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/usync.h>
#include <pthread.h>
#include "mp_extern.h"
#include "us.h"
#include "debug.h"

#define US_CACHEPAD	64	/* XXX assumes cache <= 128 bytes */
#define US_REFSLEEPTIME	500000

static void hlog(sem_t *, int, pid_t, pid_t, char *, int);
static short _usfindspfd(sem_t *, short);
static sem_t * _usallocsema(usptr_t *, int);
static int _uspoll_block(sem_t *);
static int _uspoll_unblock(sem_t *);
static int _uspoll_getvalue(sem_t *, int *);

/*
 * Arena User level semaphores
 *
 * These are full counting P/V semaphores
 * logging/history is maintained
 * versions that are 'select'able are available
 *
 */

/*
 * usnewsema - allocate a new sharable semaphore and initialize it to
 * 	     the given value, returning a pointer to the semaphore.
 */
usema_t *
usnewsema(usptr_t *us_ptr, int num)
{
	register sem_t *semp;

	if ((semp = _usallocsema(us_ptr, 0)) == NULL) {
		if (_uerror)
			_uprint(0, "%s:ERROR:no mem in arena 0x%x",
				"usnewsema", us_ptr);
		setoserror(ENOMEM);
		return((usema_t *) NULL);
	}

	if (usinitsema((usema_t *)semp, num) < 0) {
		usfreesema((usema_t *)semp, us_ptr);
		return((usema_t *) NULL);
	}

	return((usema_t *)semp);
}

/*
 * Polled (selectable) semaphores
 *
 * User must call usnewpollsema & usopenpollsema before calling
 * standard uspsema.
 *
 * Each semaphore has minor device from /dev/usema - it stores the
 * minor device in the semaphore struct so that unrelated processes
 * can ask the driver for a file descriptor to it.
 */
usema_t *
usnewpollsema(register usptr_t *us_ptr, int num)
{
	register sem_t *semp;

	if ((semp = _usallocsema(us_ptr, 1)) == NULL) {
		if (_uerror)
			_uprint(0, "%s:ERROR:no mem in arena 0x%x",
				"usnewpollsema", us_ptr);
		setoserror(ENOMEM);
		return((usema_t *) NULL);
	}

	if (usinitsema((usema_t *)semp, num) < 0) {
		usfreepollsema((usema_t *)semp, us_ptr);
		return((usema_t *) NULL);
	}

	return ((usema_t *)semp);
}

sem_t *
_usallocsema(usptr_t *us_ptr, int polled)
{
	ushdr_t *header = (ushdr_t *) us_ptr;
	sem_t *semp;
	size_t size;
	int i;

	/*
	 * Arena semaphores are padded to prevent multiple
	 * semaphores from falling on the same cache line.
	 */
	if ((semp = usmalloc((sizeof(sem_t)+US_CACHEPAD), header)) == NULL) {
		setoserror(ENOMEM);
		return(NULL);
	}

	semp->sem_flags = SEM_FLAGS_ARENA;
	semp->sem_xtrace = NULL;
	semp->sem_usptr = us_ptr;
	semp->sem_value = 0;
	semp->sem_refs = 0;

	if (header->u_histon & _USSEMAHIST)
		semp->sem_flags |= SEM_FLAGS_HISTORY;

	if (polled) {
		size = sizeof(short) * (unsigned int) header->u_maxtidusers;
		if ((semp->sem_pfd = usmalloc(size, header)) == NULL) {
			usfreesema(semp, header);
			if (_uerror)
				_uprint(0, "%s:ERROR:no mem in arena 0x%x",
					"_usallocsema", header);
			setoserror(ENOMEM);
			return(NULL);
		}

		for (i = 0; i < header->u_maxtidusers; i++)
			semp->sem_pfd[i] = -1;

		semp->sem_flags |= SEM_FLAGS_POLLED;
		semp->sem_dev = 0;
	} else
		semp->sem_pfd = NULL;

	return (semp);
}

/*
 * usinitsema - initialize a semaphore to the given value.  Destroys
 *	      any previous information associated with the semaphore.
 */
int
usinitsema(usema_t *s, int num)
{
	register sem_t *semp = (sem_t *) s;

	if (num > 30000 || !(semp->sem_flags & SEM_FLAGS_ARENA)) {
		setoserror(EINVAL);
		return -1;
	}

	/*
	 * Don't reinitialize the semaphore until all
	 * pending references have been dropped.
	 */
	while (SEM_REF_PENDING(semp)) {
		struct timespec nsleep = {0, US_REFSLEEPTIME};
		nanosleep(&nsleep, NULL);
	}

	if (_uerror && (semp->sem_value < 0))
		_uprint(0,
		"%s:WARNING:pid %d:%d reinitializing semp 0x%p with %d waiters",
			"usinitsema", get_pid(), pthread_self(),
			semp, semp->sem_value * -1);

	semp->sem_value = num;
	semp->sem_rlevel = 0;
	semp->sem_opid = -1;
	semp->sem_otid = -1;

	if (semp->sem_flags & (SEM_FLAGS_METER | SEM_FLAGS_DEBUG))
		(void) sem_mode(semp, SEM_MODE_TRACEINIT, &semp->sem_xtrace);

	return 0;
}

/*
 * usfreesema - free a previously allocated semaphore.
 */
/* ARGSUSED1 */
void
usfreesema(usema_t *s, usptr_t *us_ptr)
{
	register sem_t *semp = (sem_t *) s;
	register usptr_t *header = semp->sem_usptr;

	if (_uerror && header != us_ptr)
		_uprint(0,
		"usfreesema:ERROR:semp 0x%x bad usptr_t 0x%x pid %d:%d\n",
			semp, header, get_pid(), pthread_self());

	if (_uerror && !(semp->sem_flags & SEM_FLAGS_ARENA))
		_uprint(0,
		"usfreesema:ERROR:pid %d:%d - semp 0x%x is not an arena sema\n",
			get_pid(), pthread_self(), semp);

	/*
	 * Don't free the semaphore until all
	 * pending references have been dropped.
	 */
	while (SEM_REF_PENDING(semp)) {
		struct timespec nsleep = {0, US_REFSLEEPTIME};
		nanosleep(&nsleep, NULL);
	}

	if (semp->sem_flags & (SEM_FLAGS_METER | SEM_FLAGS_DEBUG))
		usfree(semp->sem_xtrace, header);

	if (semp->sem_flags & SEM_FLAGS_POLLED)
		usfree(semp->sem_pfd, header);

	semp->sem_usptr = NULL;
	usfree(semp, header);
}

/*
 * usfreepollsema - free a previously allocated pollable semaphore.
 */
int
usfreepollsema(usema_t *ps, usptr_t *us_ptr)
{
	usfreesema((usema_t *)ps, us_ptr);
	return 0;
}

/*
 * uspsema - get a resource
 *
 * Returns:
 *	1 - semaphore acquired
 *	0 - semaphore not available (polled semaphores only)
 *     -1 - errors (errno set)
 */
int
uspsema(usema_t *s)
{
	sem_t *semp = (sem_t *) s;
	usync_arg_t sarg;
	int ret;

	if (!(semp->sem_flags & SEM_FLAGS_ARENA)) {
		setoserror(EINVAL);
		return -1;
	}

retry:

	ret = 0;

	if (SEM_TRACE_ENABLED(semp)) {
		SEM_METER(semp, SM_WAIT, 1);
		SEM_DEBUG(semp, SD_LAST);
	}

	if (_r4k_sem_wait(semp, 0)) {
		/*
		 * Didn't get it, check for recursion
		 */
		if (semp->sem_flags & SEM_FLAGS_RECURSE) {
			if (semp->sem_opid == get_pid() &&
			    semp->sem_otid == pthread_self()) {
				/*
				 * It is recursive and we are
				 * the owner...  carry on!
				 */
				semp->sem_rlevel++;
				return 1;
			}
		}
  
		/*
		 * Semaphore unavailable, block!
		 */

		if (SEM_TRACE_ENABLED(semp)) {
			SEM_METER(semp, SM_BLOCK, 1);
			SEM_HISTORY(semp, HOP_PSLEEP, 0);
		}

		if (!(semp->sem_flags & SEM_FLAGS_POLLED)) {
			sarg.ua_version = USYNC_VERSION_2;
			sarg.ua_addr = (__uint64_t) semp;
			sarg.ua_policy = USYNC_POLICY_FIFO;
			sarg.ua_flags = 0;
			ret = usync_cntl(USYNC_INTR_BLOCK, &sarg);
			_r4k_sem_rewind(semp);
		} else {
			if ((ret = _uspoll_block(semp)) == 1) {
				/*
				 * The current process has registered with the
				 * kernel for sema acquistion. The sema may be
				 * acquired via a call to poll/select.
				 */
				return 0;
			} else {
				/*
				 * One of two things happened:
				 *
				 * 1) The usvsema operation beat us into the
				 *    kernel, preposting the sema and granting
				 *    us ownership now.
				 *
				 * 2) An error occurred.
				 *
				 * Either way, we didn't register for acquistion
				 * and the sema state needs to be rewound.
				 */
				_r4k_sem_rewind(semp);
			}
		}
		SEM_METER(semp, SM_BLOCK, -1);
	} else {
		if (SEM_TRACE_ENABLED(semp)) {
			SEM_METER(semp, SM_HIT, 1);
			SEM_HISTORY(semp, HOP_PHIT, 0);
		}
	}

	if (ret) {
		if (oserror() == EINTR)
			goto retry;

		if (_uerror)
			_uprint(0, "%s:ERROR:semp 0x%x pid %d:%d error %d\n",
				"uspsema", semp, get_pid(), pthread_self(),
				oserror());

		return -1;
	}

	/*
	 * If this is a recursive semaphore, mark the new owner
	 */
	if (semp->sem_flags & SEM_FLAGS_RECURSE) {
		semp->sem_opid = get_pid();
		semp->sem_otid = pthread_self();
	}

	SEM_DEBUG(semp, SD_OWNER);

	return 1;
}

/*
 * usvsema - release a resource
 *
 * Returns:
 *	0 on success
 *     -1 on failure
 */
int
usvsema(usema_t *s)
{
	sem_t *semp = (sem_t *) s;
	usync_arg_t sarg;
	int opid;
	int otid;
	int ret = 0;

	if (!(semp->sem_flags & SEM_FLAGS_ARENA)) {
		setoserror(EINVAL);
		return -1;
	}

	if (SEM_TRACE_ENABLED(semp)) {
		SEM_METER(semp, SM_POST, 1);
		SEM_DEBUG(semp, SD_LAST);
	}

	if (semp->sem_flags & SEM_FLAGS_RECURSE) {
		/*
		 * Clear ownership
		 *
		 * Note: This is done in advance to prevent
		 * ownership field maintenance races.
		 */
		opid = semp->sem_opid;
		otid = semp->sem_otid;
		semp->sem_opid = -1;
		semp->sem_otid = -1;
	}

	if (_r4k_sem_post(semp)) {
		/*
		 * Before we wake anyone up, check for recursion
		 */
		if (semp->sem_flags & SEM_FLAGS_RECURSE) {
			if (opid == get_pid() && otid == pthread_self() &&
			    semp->sem_rlevel) {
				/*
				 * Decrement recursion level
				 */
				semp->sem_rlevel--;
				_r4k_sem_rewind(semp);

				/*
				 * Reset ownership and return
				 */
				semp->sem_opid = opid;
				semp->sem_otid = otid;
				return 0;
			}
		}

		SEM_HISTORY(semp, HOP_VWAKE, -1);

		/*
		 * Wakeup a waiting process
		 */
		if (!(semp->sem_flags & SEM_FLAGS_POLLED)) {
			sarg.ua_version = USYNC_VERSION_2;
			sarg.ua_addr = (__uint64_t) semp;
			sarg.ua_flags = 0;
			ret = usync_cntl(USYNC_UNBLOCK, &sarg);
		} else {
			SEM_REF_GRAB(semp);
			if ((ret = _uspoll_unblock(semp)) == 1) {
				/*
				 * Only rewind the semaphore count if a
				 * process was actually registered with
				 * the kernel for acquisition. Otherwise,
				 * let the uspsema prepost case handle
				 * the rewind operation.
				 */
				_r4k_sem_rewind(semp);
				SEM_METER(semp, SM_BLOCK, -1);

				/*
				 * XXX SEM_DEBUG(semp, ...)
				 * XXX Need a way to set this on behalf
				 * XXX of the real owner (select).
				 *
				 * Perhaps _uspoll_unblock should return the
				 * pid/tid of the process we just woke-up.
				 */
				ret = 0;
			}
			SEM_REF_DROP(semp);
		}
	} else {
		if (SEM_TRACE_ENABLED(semp)) {
			SEM_METER(semp, SM_NOWAITERS, 1);
			SEM_HISTORY(semp, HOP_VHIT, 0);
		}
	}

	if (ret && _uerror)
		_uprint(0, "%s:ERROR:semp 0x%x pid %d:%d error %d\n",
			"usvsema", semp, get_pid(), pthread_self(), oserror());

	return ret;
}

/*
 * uscpsema - get a resource conditionally
 *
 * Returns:
 *	1 - semaphore acquired
 *	0 - semaphore not available
 */
int
uscpsema(usema_t *s)
{
	sem_t *semp = (sem_t *) s;
	usync_arg_t sarg;

	if (!(semp->sem_flags & SEM_FLAGS_ARENA)) {
		setoserror(EINVAL);
		return -1;
	}

	if (_r4k_sem_trywait(semp)) {
		/*
		 * Didn't get it, check for recursion
		 */
		if (semp->sem_flags & SEM_FLAGS_RECURSE) {
			if (semp->sem_opid == get_pid() &&
			    semp->sem_otid == pthread_self()) {
				/*
				 * It is recursive and we are
				 * the owner...  carry on!
				 */
				semp->sem_rlevel++;
				SEM_DEBUG(semp, SD_LAST);
				return 0;
			}
		}

		/*
		 * Semaphore is not immediately available.
		 * Check if a prepost is pending.  If so,
		 * consume it.
		 */
		sarg.ua_version = USYNC_VERSION_2;
		sarg.ua_addr = (__uint64_t) semp;
		sarg.ua_flags = USYNC_FLAGS_PREPOST_CONSUME;
		if (usync_cntl(USYNC_GET_STATE, &sarg) <= 0) {
			/*
			 * A prepost was not pending.
			 * Semaphore is not available.
			 */
			return 0;
		}
	}

	/*
	 * Got it
	 */

	if (semp->sem_flags & SEM_FLAGS_RECURSE) {
		semp->sem_opid = get_pid();
		semp->sem_otid = pthread_self();
	}

	if (SEM_TRACE_ENABLED(semp)) {
		SEM_METER(semp, SM_WAIT, 1);
		SEM_METER(semp, SM_HIT, 1);
		SEM_DEBUG(semp, SD_OWNER | SD_LAST);
		SEM_HISTORY(semp, HOP_PHIT, 0);
	}

	return 1;
}

/*
 * ustestsema - returns the value of a sema without changing its state
 */
int
ustestsema(usema_t *s)
{
	register sem_t *semp = (sem_t *) s;
	int val;

	if (semp->sem_flags & SEM_FLAGS_POLLED)
		(void) _uspoll_getvalue(semp, &val);
	else
		(void) sem_getvalue(semp, &val);

	return val;
}

/*
 * _uspoll_block: polled semaphore block routine
 *
 * Returns:
 *      0 = acquired semaphore by consuming pending prepost
 *      1 = not available, but obtainable via select() or poll()
 *     -1 = error
 */
static int
_uspoll_block(sem_t *semp)
{
	register ushdr_t *header = semp->sem_usptr;
	short fd;
	short tid;

	if ((tid = __usfastadd(header)) == -1) {
		/* XXX sema/lock is scrogged */
		if (_uerror)
			_uprint(0, "%s:ERROR:no tid!for process %d:%d",
				"uspoll_block", get_pid(), pthread_self());
		setoserror(EBADF);
		return -1;
	}

	if ((fd = semp->sem_pfd[tid]) == -1)
		fd = _usfindspfd(semp, tid);

	if (ioctl(fd, UIOCABLOCKQ) != 0) {
		if (oserror() == EAGAIN)  /* preposted */
			return 0;

		if (_uerror)
			_uprint(1, "%s:ERROR:pid %d:%d tid %d fd %d",
				"uspoll_block", get_pid(), pthread_self(),
				tid, fd);

		return -1;
	}

	return 1;
}

/*
 * _uspoll_unblock - utility function for unblocking polled semaphores
 *
 * Returns:
 *	 0 = no waiters were present so the semphore was preposted
 *	 1 = unblocked a waiting process
 *      -1 = error
 */
static int
_uspoll_unblock(sem_t *semp)
{
	register ushdr_t *header = semp->sem_usptr;
	short fd;
	short tid;
	int recursive = 0;

	if (semp->sem_flags & SEM_FLAGS_RECURSE)
		recursive = 1;

	if ((tid = __usfastadd(header)) == -1) {
		/* XXX sema/lock is scrogged */
		if (_uerror)
			_uprint(0,
				"%s:ERROR:no tid!for process %d:%d",
				"uspoll_unblock", get_pid(), pthread_self());
		return -1;
	}

	if ((fd = semp->sem_pfd[tid]) == -1)
		fd = _usfindspfd(semp, tid);

	if (ioctl(fd, UIOCAUNBLOCKQ, recursive) != 0) {
		if (oserror() == EAGAIN)  /* preposted */
			return 0;

		if (_uerror)
			_uprint(1,
				"%s:ERROR:unblock:pid %d:%d tid %d fd %d",
				"uspoll_unblock", get_pid(), pthread_self(),
				tid, fd);
		return -1;
	}

	return 1;
}

static int
_uspoll_getvalue(sem_t *semp, int *sval)
{
	register ushdr_t *header = semp->sem_usptr;
	short fd;
	short tid;
	int tmp;

	if ((tid = __usfastadd(header)) == -1) {
		/* XXX sema/lock is scrogged */
		if (_uerror)
			_uprint(0,
				"%s:ERROR:no tid!for process %d:%d",
				"uspoll_getvalue", get_pid(), pthread_self());
		return -1;
	}

	if ((fd = semp->sem_pfd[tid]) == -1)
		fd = _usfindspfd(semp, tid);

	/*
	 * Computing the value of a polled semaphore is a little
	 * tricky because the semaphore state is split between libc
	 * and the kernel.
	 * 
	 * Calling this routine is inherently racy, so it's mainly
	 * used for debugging and speed isn't essential.
	 *
	 * We always have to check the kernel maintained state to
	 * find-out how many waiters are actually blocked, since
	 * the poster (often) increments the semaphore value *after*
	 * it wakes-up a process.  A waiter may wake-up, run, and
	 * then call this function before the semaphore value
	 * (sem_value) gets updated.
	 *
	 * UIOCGETCOUNT Returns
	 *
	 *	<= 0 : number of waiters
	 *	>  0 : prepost value
	 */

	if ((tmp = ioctl(fd, UIOCGETCOUNT)) >= 0) {
		/* No waiters. Use sem_value and add preposts */
		tmp += semp->sem_value;
		/* Take recursion level into account */
		tmp += semp->sem_rlevel;
		if (tmp < 0) {
			/*
			 * If the result is negative, return zero
			 * and call it a race.
			 */
			tmp = 0;
		}
	}

	*sval = tmp;
	return 0;
}

/*
 * usctlsema - ctl function for user semaphores
 *
 * Returns:
 *	0 Success
 *	-1 error (errno set)
 */
int
usctlsema(usema_t *s, int cmd, ...)
{
	va_list ap;
	int rval = 0;
	sem_trace_t *xtrace;
	semameter_t *met;
	semadebug_t *deb;
	sem_t *semp = (sem_t *) s;

	if (!(semp->sem_flags & SEM_FLAGS_ARENA)) {
		setoserror(EINVAL);
		return -1;
	}

	va_start(ap, cmd);
	switch(cmd) {
	case CS_METERON:
		if (sem_mode(semp, SEM_MODE_METERRESET) < 0) {
			xtrace = usmalloc(sizeof(sem_trace_t), semp->sem_usptr);
			if (xtrace == NULL) {
				setoserror(ENOMEM);
				return(-1);
			}

			if (sem_mode(semp, SEM_MODE_TRACEINIT, xtrace) < 0) {
				usfree(xtrace, semp->sem_usptr);
				return(-1);
			}
		}
		rval = sem_mode(semp, SEM_MODE_METERON);
		break;

	case CS_METEROFF:
		rval = sem_mode(semp, SEM_MODE_METEROFF);
		break;

	case CS_METERFETCH:
		met = va_arg(ap, semameter_t *);
		rval = sem_mode(semp, SEM_MODE_METERFETCH, met);
		break;

	case CS_METERRESET:
		rval = sem_mode(semp, SEM_MODE_METERRESET);
		break;

	case CS_DEBUGON:
		if (sem_mode(semp, SEM_MODE_DEBUGRESET) < 0) {
			xtrace = usmalloc(sizeof(sem_trace_t), semp->sem_usptr);
			if (xtrace == NULL) {
				setoserror(ENOMEM);
				return(-1);
			}

			if (sem_mode(semp, SEM_MODE_TRACEINIT, xtrace) < 0) {
				usfree(xtrace, semp->sem_usptr);
				return(-1);
			}
		}
		rval = sem_mode(semp, SEM_MODE_DEBUGON);
		break;

	case CS_DEBUGOFF:
		rval = sem_mode(semp, SEM_MODE_DEBUGOFF);
		break;

	case CS_DEBUGFETCH:
		deb = va_arg(ap, semadebug_t *);
		rval = sem_mode(semp, SEM_MODE_DEBUGFETCH, deb);
		break;

	case CS_DEBUGRESET:
		rval = sem_mode(semp, SEM_MODE_DEBUGRESET);
		break;

	case CS_HISTON:
		semp->sem_flags |= SEM_FLAGS_HISTORY;
		break;

	case CS_HISTOFF:
		semp->sem_flags &= ~SEM_FLAGS_HISTORY;
		break;

	case CS_RECURSIVEON:
		if (semp->sem_value == 1) {
			semp->sem_flags |= SEM_FLAGS_RECURSE;
			semp->sem_opid = -1;
			semp->sem_otid = -1;
			return 0;
		}
 		setoserror(EINVAL);  
		rval = -1;
		break;

	case CS_RECURSIVEOFF:
		semp->sem_flags &= ~SEM_FLAGS_RECURSE;
		break;

	default:
		setoserror(EINVAL);
		rval = -1;
	};

	return (rval);
}

/*
 * usopenpollsema - establish a file descriptor that can it attached to the
 *	argument semaphore.
 * Cases:
 *	1) we are first open - we need to get a semaphore device and
 *		attach to it
 *	2) we are a share group member and are calling open even though
 *		someone else in our share group has already done so
 *		- in this case we do NOT want to open another descriptor
 *	3) we are a share group member not sharing fd's OR any other
 *		unrelated process - we may need to open one of our own
 *	4) we are a forked child which has same fd - we do NOT want
 *		to open another file descriptor
 */
int
usopenpollsema(usema_t *ps, mode_t acc)
{
	register sem_t *semp = (sem_t *) ps;
	ushdr_t *header = semp->sem_usptr;
	register int i, nfd, fd;
	int error;
	short tid;
	struct stat sb;
	usattach_t at;

	if ((tid = __usfastadd(header)) == -1)
		return -1;

	/* single thread opening/closing */
	if (uspsema(header->u_openpollsema) < 0)
		return(-1);

	/*
	 * first see if by some chance we already have an fd for this semaphore:
	 * 1) we are a forked child (and sema set up before fork)
	 * 2) we are a share group member sharing fds
	 * 3) we are a share group member NOT sharing fds
	 *		(and sema set up before sproc)
	 */
	if (semp->sem_dev != 0) {
		for (i = 0; i < header->u_maxtidusers; i++) {
			if (_usget_pid(header, (short)i) == -1)
				continue;
			fd = semp->sem_pfd[i];
			if (fd == -1)
				continue;

			if (fstat(fd, &sb) == 0 &&
					(sb.st_mode & S_IFMT) == S_IFCHR &&
					sb.st_rdev == semp->sem_dev) {
				/* we got one! */
				nfd = fd;
				goto out;
			}
		}
	}

	if (semp->sem_dev == 0) {
alloc:
		/* don't have minor yet */
		if ((nfd = open(USEMACLNDEV, O_RDWR)) < 0) {
			error = oserror();
			if (_uerror) {
				_uprint(1,
		"usopenpollsema:ERROR:pid %d:%d cannot get new minor",
					get_pid(), pthread_self());
			}
			goto err;
		}
		if (fstat(nfd, &sb) != 0 || fchmod(nfd, acc) < 0) {
			error = oserror();
			close(nfd);
			if (_uerror)
				_uprint(1,
		"usopenpollsema:ERROR:pid %d:%d cannot chg mode on new minor",
					get_pid(), pthread_self());
			goto err;
		}
		if (_utrace)
			_uprint(0,
		"TRACE: usopenpollsema:tid %d pollsema @ 0x%x creates device 0x%x fd %d",
				tid, semp, sb.st_rdev, nfd);
		semp->sem_dev = (int) sb.st_rdev;

		/* Register address of semaphores ID field */
		at.us_dev = NULL;
		at.us_handle = &semp->sem_opid;
		if (ioctl(nfd, UIOCIDADDR, &at) < 0) {
			error = oserror();
			close(nfd);
			if (_uerror)
				_uprint(1,
		"usopenpollsema:ERROR:pid %d:%d cannot register handle",
					get_pid(), pthread_self());
			goto err;
		}		
	} else {
		/*
		 * someone else has already established minor num - just grab
		 * a copy for us
		 */
		if ((fd = open(USEMADEV, O_RDWR)) < 0) {
			error = oserror();
			goto err;
		}

		at.us_dev = semp->sem_dev;
		at.us_handle = &semp->sem_opid;

		if ((nfd = ioctl(fd, UIOCATTACHSEMA, &at)) < 0) {
			error = oserror();
			close(fd);
			if (error == EINVAL) {
				/*
				 * Perhaps the originator died without
				 * calling closepollsema?? Lets just
				 * clear out the dev and alloc a new one
				 */
				semp->sem_dev = 0;
				goto alloc;
			}
			if (_uerror)
				_uprint(1, "usopenpollsema:ERROR:cannot attach to existing minor 0x%x", semp->sem_dev);
			goto err;
		}
		if (_utrace)
			_uprint(0, "TRACE: usopenpollsema:tid %d pollsema @ 0x%x attaches device 0x%x fd %d",
				tid, semp, semp->sem_dev, nfd);
		close(fd);
	}
	fcntl(nfd, F_SETFD, FD_CLOEXEC);
out:
	usvsema(header->u_openpollsema);
	/* put fd into our slot */
	semp->sem_pfd[tid] = (short)nfd;
	return(nfd);

err:
	usvsema(header->u_openpollsema);
	setoserror(error);
	return(-1);
}

/*
 * XXX is there a race when setting another tid's fd?? and they're blocking/
 *	unblocking?
 * We don't zap sp_dev since we don't really know when the last opener is
 * gone.
 */
int
usclosepollsema(usema_t *ps)
{
	register sem_t *semp = (sem_t *) ps;
	register ushdr_t *header = semp->sem_usptr;
	int tid;
	register int mask, i;
	register int didclose = 0;
	struct stat sb;

	if ((tid = _usgettid(header)) == -1) {
		setoserror(EINVAL);
		return(-1);
	}

	/* the caller may not have it open (have gotton an auto-open) */
	if (semp->sem_pfd[tid] != -1) {
		if (close(semp->sem_pfd[tid]) != 0)
			return(-1);
		didclose++;
		semp->sem_pfd[tid] = -1;
	}

	if (prctl(PR_GETSHMASK, 0) < 0) {
		/* we're not in a share group - bag it */
		return(0);
	}

	/* single thread opening/closing */
	if (uspsema(header->u_openpollsema) < 0)
		return(-1);

	/*
	 * now go through and and invalid all other auto-opens
	 * these correspond to share group members who
	 * didn't explicitly usopenpollsema()
	 */
	for (i = 0; i < header->u_maxtidusers; i++) {
		if (semp->sem_pfd[i] == -1)
			continue;
		mask = (int)prctl(PR_GETSHMASK, _usget_pid(header, (short)i));
		if (mask >= 0) {
			if ((mask & PR_SFDS) == 0)
				/* not sharing file descriptors */
				continue;
			if (!didclose) {
				if (close(semp->sem_pfd[i]) != 0)
					goto bad;
				didclose++;
			}
			semp->sem_pfd[i] = -1;
		}
		/*
		 * dead or not in our share group
		 */
		if (oserror() == ESRCH) {
			/*
			 * dead - clear out its fd!
			 * IF it WAS in callers share group AND
			 * caller didn't yet have auto-open then we may
			 * not know enough to properly close.
			 * So we see if caller has fd at same position
			 * corresponding to the semaphore of interest
			 */
			if (!didclose) {
				if (fstat(semp->sem_pfd[i], &sb) == 0 &&
					   (sb.st_mode & S_IFMT) == S_IFCHR &&
					   sb.st_rdev == semp->sem_dev) {
					/* Yes! */
					if (close(semp->sem_pfd[i]) != 0)
						goto bad;
					didclose++;
				}
			}
			semp->sem_pfd[i] = -1;
		}
	}
	usvsema(header->u_openpollsema);
	return(0);
bad:
	usvsema(header->u_openpollsema);
	return(-1);
}

/*
 * _usfindspfd - find an suitable fd for a process to use for this
 * this semaphore - this is so that share group members don't have
 * to each issue an usopenpollsema()
 */
static short
_usfindspfd(sem_t *semp, short tid)
{
	register int i, mask;
	register ushdr_t *header = semp->sem_usptr;
	register int fd;
	struct stat sb;

	if (prctl(PR_GETSHMASK, 0) < 0) {
		/* we're not in a share group - bag it */
		return(-1);
	}

	for (i = 0; i < header->u_maxtidusers; i++) {
		if ((fd = semp->sem_pfd[i]) == -1)
			continue;

		/* see if process is sharings fds with us */
		mask = (int)prctl(PR_GETSHMASK, _usget_pid(header, (short)i));

		if (mask < 0) {
			if (errno == ESRCH) {
				/* process is dead, see if fd is still usable */
				if (fstat(fd, &sb) == 0 &&
				    (sb.st_mode & S_IFMT) == S_IFCHR &&
				    sb.st_rdev == semp->sem_dev)
					goto found_fd;
			}
			/* process is not in our share group */
			continue;
		}

		if ((mask & PR_SFDS) == 0) {
			/* not sharing file descriptors */
			continue;
		}

	found_fd:

		semp->sem_pfd[tid] = (short)fd;

		if (_uerror)
			if (fstat(fd, &sb) != 0 ||
					(sb.st_mode & S_IFMT) != S_IFCHR ||
					sb.st_rdev != semp->sem_dev) {
				_uprint(0,
"usfindspfd:ERROR:pid %d sharing fd %d with pid %d but dev %x doesn't match!\n",
					get_pid(), fd,
					_usget_pid(header, (short)i),
					sb.st_rdev);
				return(-1);
			}

		if (_utrace)
			_uprint(0,
"TRACE: giving auto-open to tid %d pollsema @ 0x%x gets fd %d",
				tid, semp, fd);
		return((short)fd);
	}
	return(-1);
}

/*
 * return amount of space needed per semaphore
 */
int
_ussemaspace(void)
{
	int semasize;

	/*
	 * Note: Arena semaphores are padded a bit to disallow
	 * 	 multiple semaphores to fall on the same cache line.
	 */
	semasize = (int)sizeof(sem_t) + (int)sizeof(sem_trace_t) + US_CACHEPAD;

	return (semasize);
}

int
usdumpsema(usema_t *s, FILE *fd, const char *n)
{
	return (sem_print((sem_t *) s, fd, n));
}

/*
 * Semaphore logging.
 */
void
hlog(sem_t *semp, int op, pid_t own, pid_t wake, char *cpc, int count)
{
	ushdr_t	*header = semp->sem_usptr;
	hist_t *hp;

	/*
	 * if we have a entry limit or cannot get more memory then
	 * re-use oldest history entry
	 */
	if ((header->u_histsize && (unsigned int)(header->u_histcount) > header->u_histsize) ||
	   ((hp = (hist_t *) usmalloc(sizeof(hist_t), header)) == NULL)) {
		ussetlock(header->u_histlock);
		header->u_histerrors++;
		if (header->u_histcount == 0) {
			usunsetlock(header->u_histlock);
			return;
		}
		/* use first log entry */
		hp = header->u_histfirst;
		if (hp->h_next)
			hp->h_next->h_last = NULL;
		else
			header->u_histlast = NULL;
		header->u_histfirst = hp->h_next; 
	} else {
		ussetlock(header->u_histlock);
		header->u_histcount++;
	}

	hp->h_sem = (usema_t *)semp;
	hp->h_op = op;
	hp->h_pid = own;
	hp->h_scnt = count;
	hp->h_wpid = wake;
	hp->h_cpc = cpc;
	hp->h_next = NULL;

	if (header->u_histfirst == NULL) {
		header->u_histfirst = hp; 
		header->u_histlast = hp; 
		hp->h_last = NULL;
	} else {
		header->u_histlast->h_next = hp; 
		hp->h_last = header->u_histlast;	
		header->u_histlast = hp;
	}
	usunsetlock(header->u_histlock);
}

#define OPSNMLEN 8
static const char ops[][OPSNMLEN] = {
	"UNK",
	"PHIT  ",
	"PSLEEP",
	"PWOKE ",
	"VHIT  ",
	"VWAKE ",
	"PINTR ",
	"PMISS ",
};

void
usdumphist(usema_t *s, FILE *fd)
{
	hist_t *h, *oh = NULL;
	histptr_t hp;
	register sem_t *semp = (sem_t *)s;
	register ushdr_t *header = semp->sem_usptr;

	if (!fd)
		fd = stderr;

	if (usconfig(CONF_HISTFETCH, header, &hp) != 0)
		return;

	fprintf(fd, "History current 0x%p entries %d errors %d\n",
		hp.hp_current, hp.hp_entries, hp.hp_errors);

	fprintf(fd, "Backward in time\n");
	for (h = hp.hp_current; h; h = h->h_last) {
		if (semp && semp != (sem_t *) h->h_sem)
			continue;
		fprintf(fd, "%s sema @ 0x%p pid %d wpid %d cpc 0x%p scnt %d\n",
			ops[h->h_op], h->h_sem, h->h_pid, h->h_wpid,
			h->h_cpc, h->h_scnt);
		oh = h;
	}

	fprintf(fd, "Forward in time\n");
	for (h = oh; h; h = h->h_next) {
		if (semp && semp != (sem_t *) h->h_sem)
			continue;
		fprintf(fd, "%s sema @ 0x%p pid %d wpid %d cpc 0x%p scnt %d\n",
			ops[h->h_op], h->h_sem, h->h_pid, h->h_wpid,
			h->h_cpc, h->h_scnt);
	}
}
