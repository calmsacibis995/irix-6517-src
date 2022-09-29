/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, 1990 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 3.51 $ $Author: joecd $"

#ifdef __STDC__
	#pragma weak spin_destroy = _spin_destroy
	#pragma weak spin_init = _spin_init
	#pragma weak spin_mode = _spin_mode
	#pragma weak spin_print = _spin_print
	#pragma weak spin_test = _spin_test
#endif

#include "synonyms.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <ulocks.h>
#include <time.h>
#include <strings.h>
#include <sys/usioctl.h>
#include <sys/usync.h>
#include <sync_internal.h>
#include <pthread.h>
#include "shlib.h"
#include "us.h"
#include "mp_extern.h"

/*
 * This file implements the POSIX and Arena spin lock *support* code,
 * including:
 *
 * 1) Uni-processor block/unblock routines
 * 2) Debug implementations of spin locks (UP & MP)
 * 3) Arena spin lock allocation, initialization and destruction routines
 * 4) Spin lock attribute control
 *
 * The real (non-debug) spin lock routines are coded in r4k.s.
 *
 * For an explanation of what the real spin lock routines do (asside from
 * looking at asm code), take a look at the debug locking algorithms below.
 */

extern struct timespec __usnano;

/* default # of spins waiting for a lock before yielding the processor */
int _ushlockdefspin = _SPINDEFSPINC;

/* default # of times to yield processor before 'napping' */
int _usyieldcnt = _SPINDEFSLEEPC;

static int _spin_init_common(spinlock_t *, int);

/*
 * spin_init: Initializes a POSIX spin lock
 */
int
spin_init(spinlock_t *slock)
{
	return (_spin_init_common(slock, SPIN_FLAGS_POSIX));
}

/*
 * _spin_arena_init: Initializes an Arena spin lock
 *
 * NOTE: All calls to usinitlock are routed directly to this function.
 */		
int
_spin_arena_init(ulock_t l)
{
	return (_spin_init_common((spinlock_t *) l, SPIN_FLAGS_ARENA));
}
  
static int
_spin_init_common(spinlock_t *slock, int type)
{
	slock->spin_members = 0;

	if (type == SPIN_FLAGS_POSIX) {
		slock->spin_flags = SPIN_FLAGS_POSIX;
		slock->spin_usptr = NULL;
		slock->spin_trace = NULL;

		if (_us_systype == 0)
			_us_systype = _getsystype();
	}

	if (slock->spin_flags & SPIN_FLAGS_DEBUG) {
		slock->spin_trace->st_spins = 0;
		slock->spin_trace->st_tries = 0;
		slock->spin_trace->st_hits = 0;
		slock->spin_trace->st_unlocks = 0;
		slock->spin_trace->st_opid = -1;
		slock->spin_trace->st_otid = -1;
	}

	if (_us_systype & US_UP)
		slock->spin_flags |= SPIN_FLAGS_UP;

	return 0;
}

/*
 * _spin_arena_alloc: allocates a spin lock from the specified arena.
 *
 * NOTE: All calls to "usnewlock" are routed directly to this function.
 *
 * Returns:	ulock_t when successful
 *		-1 if unable to alloc lock
 */
#define CACHELINESIZE 128
ulock_t
_spin_arena_alloc(usptr_t *us_ptr)
{
	register ushdr_t *header = us_ptr;
	register spinlock_t *slock;
	int pad;

	/*
	 * Arena spin locks are padded to prevent multiple
	 * locks from falling on the same cache line.
	 */
	pad = CACHELINESIZE - sizeof(spinlock_t);

	if ((slock = usmalloc(sizeof(spinlock_t)+pad, us_ptr)) == NULL) {
		setoserror(ENOMEM);
		return(NULL);
	}

	slock->spin_usptr = us_ptr;
	slock->spin_flags = SPIN_FLAGS_ARENA;

	if (header->u_locktype == US_NODEBUG)
		slock->spin_trace = NULL;
	else {
		slock->spin_trace = usmalloc(sizeof(spinlock_trace_t), us_ptr);
		if (slock->spin_trace == NULL) {
			usfree(slock, us_ptr);
			setoserror(ENOMEM);
			return(NULL);
		}
		slock->spin_flags |= SPIN_FLAGS_DEBUG|SPIN_FLAGS_METER;
	}

	_spin_init_common(slock, SPIN_FLAGS_ARENA);
	return((ulock_t) slock);
}

/*
 * spin_destroy: Destroys a POSIX or Arena spin lock
 *
 * NOTE: All calls to usfreelock are routed directly to this function.
 */
int
spin_destroy(spinlock_t *slock)
{
	if (slock->spin_flags & SPIN_FLAGS_ARENA) {
		if (slock->spin_flags & SPIN_FLAGS_DEBUG)
			usfree(slock->spin_trace, slock->spin_usptr);

		usfree(slock, slock->spin_usptr);
	} else {
		slock->spin_flags = 0;
		slock->spin_trace = NULL;
	}

	return 0;
}

/*
 * spin_test: tests the spin lock, returning a snapshot of the lock state.
 *
 * NOTE: All calls to ustestlock are routed directly to this function.
 *
 * Returns:	1 if the lock is locked,
 * 		0 if the lock is free
 */
int
spin_test(spinlock_t *slock)
{
	return (slock->spin_members ? 1 : 0);
}

/*
 * spin_blocker: "spin" lock block routine for uni-processor systems
 */
int
_spin_blocker(spinlock_t *slock)
{
	usync_arg_t uarg;

	uarg.ua_version = USYNC_VERSION_2;
	uarg.ua_addr = (__uint64_t) slock;
	uarg.ua_flags = 0;

	if (slock->spin_flags & SPIN_FLAGS_PRIO)
		uarg.ua_policy = USYNC_POLICY_PRIORITY;
	else
		uarg.ua_policy = USYNC_POLICY_FIFO;

	while (usync_cntl(USYNC_INTR_BLOCK, &uarg)) {
		/*
		 * Spin if we returned due to an interrupt
		 */
	  	if (oserror() != EINTR) {
			if (_uerror) {
				_uprint(1,
				"%s:ERROR:lock 0x%p pid %d tid %d error %d",
				"spin_blocker", slock, get_pid(),
				pthread_self(), oserror());
			}
			return -1;
		}
	}

	/*
	 * Adjust success return code
	 *
	 * Arena locks return 1 on success, while
	 * POSIX locks return 0 on success.
	 */ 
	return (slock->spin_flags & SPIN_FLAGS_ARENA);
}

/*
 * spin_unblocker: "spin" lock unblock routine for uni-processor systems
 */
int
_spin_unblocker(spinlock_t *slock)
{
	usync_arg_t uarg;

	uarg.ua_version = USYNC_VERSION_2;
	uarg.ua_addr = (__uint64_t) slock;
	uarg.ua_flags = 0;
	return (usync_cntl(USYNC_UNBLOCK, &uarg));
}

/*
 * spin_mode: controls various spin lock attribute and trace modes
 *
 * Note: All calls to usctllock are routed directly to this function.
 *
 * Returns:	0 Success
 *		-1 error (errno set)
 */
int
spin_mode(spinlock_t *slock, int cmd, ...)
{
	va_list ap;
	int rval = 0;
	lockmeter_t *met;
	lockdebug_t *deb;
	spinlock_trace_t *stp;

	va_start(ap, cmd);
	switch(cmd) {

	case SPIN_MODE_TRACEINIT:
		stp = va_arg(ap, spinlock_trace_t *);

		if (stp == NULL) {
			setoserror(EINVAL);
			return -1;
		}

		slock->spin_trace = stp;
		/* fall through */

	case SPIN_MODE_TRACERESET:

		if (slock->spin_trace == NULL) {
			setoserror(EINVAL);
			return -1;
		}
	    
		slock->spin_trace->st_spins = 0;
		slock->spin_trace->st_tries = 0;
		slock->spin_trace->st_hits = 0;
		slock->spin_trace->st_unlocks = 0;
		slock->spin_trace->st_opid = -1;
		slock->spin_trace->st_otid = -1;
		break;

	case SPIN_MODE_TRACEON:

		if (slock->spin_trace == NULL) {
			setoserror(EINVAL);
			return -1;
		}

		slock->spin_flags |= SPIN_FLAGS_DEBUG|SPIN_FLAGS_METER;
		break;

	case SPIN_MODE_TRACEPLUSON:
		if (slock->spin_trace == NULL) {
			setoserror(EINVAL);
			return -1;
		}

		slock->spin_flags |= SPIN_FLAGS_DEBUG|SPIN_FLAGS_DPLUS;
		break;

	case SPIN_MODE_TRACEOFF:

		slock->spin_flags &=
			~(SPIN_FLAGS_DEBUG|SPIN_FLAGS_METER|SPIN_FLAGS_DPLUS);
		break;

	case SPIN_MODE_UP:

		slock->spin_flags |= SPIN_FLAGS_UP;
		break;

	case SPIN_MODE_QUEUEFIFO:

		slock->spin_flags &= ~SPIN_FLAGS_PRIO;
		break;

	case SPIN_MODE_QUEUEPRIO:

		slock->spin_flags |= SPIN_FLAGS_PRIO;
		break;


	/*
	 * Arena specific commands
	 */

	case CL_METERFETCH:
		if (slock->spin_flags & SPIN_FLAGS_METER) {
			met = va_arg(ap, lockmeter_t *);
			met->lm_spins = slock->spin_trace->st_spins;
			met->lm_tries = slock->spin_trace->st_tries;
			met->lm_hits  = slock->spin_trace->st_hits;
		} else {
			setoserror(EINVAL);
			rval = -1;
		}
		break;

	case CL_METERRESET:
		if (slock->spin_flags & SPIN_FLAGS_METER) {
			slock->spin_trace->st_spins = 0;
			slock->spin_trace->st_tries = 0;
			slock->spin_trace->st_hits = 0;
			slock->spin_trace->st_unlocks = 0;
		}
		break;

	case CL_DEBUGFETCH:
		if (slock->spin_flags & SPIN_FLAGS_DEBUG) {
			deb = va_arg(ap, lockdebug_t *);
			deb->ld_owner_pid = (pid_t) slock->spin_opid;
		} else {
			setoserror(EINVAL);
			rval = -1;
		}
		break;

	case CL_DEBUGRESET:
		if (slock->spin_flags & SPIN_FLAGS_DEBUG) {
			slock->spin_trace->st_opid = -1;
			slock->spin_trace->st_otid = -1;
		}
		break;

	default:
		setoserror(EINVAL);
		rval = -1;
		break;
	};

	return (rval);
}

/*
 * DEBUG lock routines
 *
 * When debugging is enabled for a lock, the debug routines below are invoked,
 * substituting the functionality of the faster assembly versions in r4k.s.
 * 
 */

/*
 * DEBUG: spin_lock & ussetlock
 */
int
_spin_lock_debug(spinlock_t *slock, char *callpc)
{
	register unsigned spinc = _ushlockdefspin;
	register unsigned sleepc = _usyieldcnt;
	int ret;
	int pid;
	int tid;
	int spunout = 0;
	int flags = slock->spin_flags;

	pid = get_pid();
	tid = pthread_self();

	if (flags & SPIN_FLAGS_DPLUS) {
		if (slock->spin_members && slock->spin_opid == pid &&
		    slock->spin_otid == tid)
			_uprint(0,
		"Double tripped on lock @ 0x%p by pid %d:%d called from 0x%p",
				slock, pid, tid, callpc);
	}

	if (flags & SPIN_FLAGS_UP) {
		if (ret = _r4k_dbg_spin_lock(slock)) {
			/*
			 * Lock not available, block
			 */
			ret = _spin_blocker(slock);
			_r4k_dbg_spin_rewind(slock);

			spunout++;

			if (ret == -1)
				return ret;
		}
	} else {
		while (ret = _r4k_dbg_spin_trylock(slock)) {
			/*
			 * Lock not available, spin
			 */
		  	if (--spinc == 0) {
				if (--sleepc == 0) {
					sleepc = _usyieldcnt;
					nanosleep(&__usnano, NULL);
				} else
					sginap(0);
				spinc = _ushlockdefspin;
				spunout++;
			}
		}
	}

	/*
	 * We got it!
	 */

	slock->spin_opid = pid;
	slock->spin_otid = tid;

	if (flags & SPIN_FLAGS_METER) {
		slock->spin_trace->st_tries++;
		slock->spin_trace->st_hits++;
		slock->spin_trace->st_spins += spunout;
	}

	/*
	 * Adjust success return code
	 *
	 * Arena locks return 1 on success, while
	 * POSIX locks return 0 on success.
	 */ 
	return (flags & SPIN_FLAGS_ARENA);
}

/*
 * DEBUG: spin_unlock & usunsetlock
 */
int
_spin_unlock_debug(spinlock_t *slock, char *callpc)
{
	int ret;
	int flags = slock->spin_flags;

	if (flags & SPIN_FLAGS_METER)
		slock->spin_trace->st_unlocks++;

	if (flags & SPIN_FLAGS_DPLUS) {
		int pid = get_pid();
		int tid = pthread_self();
		int opid = slock->spin_opid;
		int otid = slock->spin_otid;

		if (slock->spin_members == 0) {
			  _uprint(0,
				  "%s lock @ 0x%p pid %d:%d called from 0x%p",
				  "Unset lock, but lock not locked.",
				  slock, pid, tid, callpc);
			  return 0;
		} else if (opid != pid || otid != tid) {
			_uprint(0,
		"%s %d:%d locked. lock @ 0x%p pid %d:%d called from 0x%p",
				"Unlocking lock that process",
				opid, otid, slock, pid, tid, callpc);
		}
	}

	/*
	 * The owner fields can only be modified while the lock is held.
	 * We clear ownership (-1) here because r4k_dbg_spin_unlock() releases
	 * the lock when no waiters are present.
	 *
	 * In the event waiters are present, the lock isn't actually dropped.
	 * This is because we are handing-off ownership. In this case, we mark
	 * the owner state transitional (0).
	 */
	slock->spin_opid = -1;
	slock->spin_otid = -1;

	if (ret = _r4k_dbg_spin_unlock(slock)) {
		/*
		 * There are waiters present.  Wake one up.
		 */

		slock->spin_opid = 0;
		slock->spin_otid = 0;

		ret = _spin_unblocker(slock);
	}

	return ret;
}

/*
 * DEBUG: uswsetlock & spin_lockw
 */
int
_spin_lockw_debug(spinlock_t *slock, unsigned int spinc, char *callpc)
{
	int ret;
	int pid;
	int tid;
	int spunout = 0;
	int flags = slock->spin_flags;
	unsigned int cnt = spinc;

	pid = get_pid();
	tid = pthread_self();

	if (flags & SPIN_FLAGS_DPLUS) {
		if (slock->spin_members && slock->spin_opid == pid &&
		    slock->spin_otid == tid)
			_uprint(0,
		"Double tripped on lock @ 0x%p by pid %d:%d called from 0x%p",
				slock, pid, tid, callpc);
	}

	if (flags & SPIN_FLAGS_UP) {
		if (ret = _r4k_dbg_spin_lock(slock)) {
			/*
			 * Lock not available, block
			 */
			ret = _spin_blocker(slock);
			_r4k_dbg_spin_rewind(slock);

			spunout++;

			if (ret == -1)
				return ret;
		}
	} else {
		while (ret = _r4k_dbg_spin_trylock(slock)) {
			/*
			 * Lock not available, spin
			 */
			if (--cnt == 0) {
				sginap(0);
				cnt = spinc;
				spunout++;
			}
		}
	}

	/*
	 * We got it!
	 */

	slock->spin_opid = pid;
	slock->spin_otid = tid;

	if (flags & SPIN_FLAGS_METER) {
		slock->spin_trace->st_tries++;
		slock->spin_trace->st_hits++;
		slock->spin_trace->st_spins += spunout;
	}

	/*
	 * Adjust success return code
	 *
	 * Arena locks return 1 on success, while
	 * POSIX locks return 0 on success.
	 */ 
	return (flags & SPIN_FLAGS_ARENA);
}

/*
 * DEBUG: uscsetlock & spin_lockc
 */
int
_spin_lockc_debug(spinlock_t *slock, unsigned int spinc, char *callpc)
{
	int pid;
	int tid;
	int spunout = 0;
	int flags = slock->spin_flags;

	pid = get_pid();
	tid = pthread_self();

retry:

	if (_r4k_dbg_spin_trylock(slock) != 0) {
		/*
		 * lock is not availble
		 */
		if (flags & SPIN_FLAGS_DPLUS && (spunout == 0)) {
			if (slock->spin_opid == pid && slock->spin_otid == tid)
				_uprint(0,
		"Double tripped on lock @ 0x%p by pid %d:%d called from 0x%p",
					slock, pid, tid, callpc);
		}

		if (!(flags & SPIN_FLAGS_UP)) {
			/*
			 * Multi-processor system
			 */
		  	if (--spinc > 0)
				goto retry;
		}

		spunout++;

		if (flags & SPIN_FLAGS_METER) {
		 	slock->spin_trace->st_tries++;
			slock->spin_trace->st_spins += spunout;
		}

		if (flags & SPIN_FLAGS_ARENA)
			return 0;

		setoserror(EBUSY);
		return -1;
	}

	/*
	 * We got it!
	 */

	slock->spin_opid = pid;
	slock->spin_otid = tid;

	if (flags & SPIN_FLAGS_METER) {
		slock->spin_trace->st_tries++;
		slock->spin_trace->st_hits++;
		slock->spin_trace->st_spins += spunout;
	}

	/*
	 * Adjust success return code
	 *
	 * Arena locks return 1 on success, while
	 * POSIX locks return 0 on success.
	 */ 
	return (flags & SPIN_FLAGS_ARENA);
}

/*
 * spin_print:	 prints the state of the specified spin lock
 *	  	 to the specified file.
 */
int
spin_print(spinlock_t *slock, FILE *fd, const char *n)
{
	char iobuf[300];
	char stype[40];
	int members;

	if (slock->spin_flags & SPIN_FLAGS_ARENA) {
		if (!(slock->spin_flags & SPIN_FLAGS_UP))
			sprintf(stype, "ARENA with no queuing");
		else if (slock->spin_flags & SPIN_FLAGS_PRIO)
			sprintf(stype, "ARENA with priority queuing");
		else
			sprintf(stype, "ARENA with FIFO queuing");

	} else if (slock->spin_flags & SPIN_FLAGS_POSIX) {
		if (!(slock->spin_flags & SPIN_FLAGS_UP))
			sprintf(stype, "POSIX with no queuing");
		else if (slock->spin_flags & SPIN_FLAGS_PRIO)
			sprintf(stype, "POSIX with priority queuing");
		else
			sprintf(stype, "POSIX with FIFO queuing");

	} else {
		setoserror(EINVAL);
		return -1;
	}

	members = (int) slock->spin_members;

	sprintf(iobuf, "Spinlock at 0x%p is %s with %d waiters\n",
		slock, members ? "HELD" : "FREE",
		members ? members-1 : 0);

	sprintf(&iobuf[strlen(iobuf)], "  Type : %s\n", stype);

	if (slock->spin_flags & SPIN_FLAGS_METER) {
		sprintf(&iobuf[strlen(iobuf)],
		"  Meter: tries %d spins %d hits %d unlocks %d\n", 
			slock->spin_trace->st_tries, 
			slock->spin_trace->st_spins,
			slock->spin_trace->st_hits,
			slock->spin_trace->st_unlocks);
	}

	if (slock->spin_flags & SPIN_FLAGS_DEBUG) {
		sprintf(&iobuf[strlen(iobuf)],
			"  Debug: current owner %d:%d\n", 
			slock->spin_opid, slock->spin_otid);
	}

	fprintf(fd, "%s: %s", n, iobuf);

	return 0;
}
