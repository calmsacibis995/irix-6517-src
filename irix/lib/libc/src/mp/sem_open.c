/*************************************************************************
*                                                                        *
*               Copyright (C) 1995 Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident "$Id: sem_open.c,v 1.32 1998/12/02 22:10:23 jph Exp $"

#ifdef __STDC__
        #pragma weak sem_close = _sem_close
	#pragma weak sem_destroy = _sem_destroy
	#pragma weak sem_getvalue = _sem_getvalue
	#pragma weak sem_init = _sem_init
	#pragma weak sem_mode = _sem_mode
	#pragma weak sem_open = _sem_open
	#pragma weak sem_post = _sem_post
	#pragma weak sem_print = _sem_print
	#pragma weak sem_unlink = _sem_unlink
	#pragma weak sem_wait = _sem_wait
	#pragma weak sem_trywait = _sem_trywait
#endif

#include <synonyms.h>
#include <semaphore_internal.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys.s>
#include <stdarg.h>
#include <sys/usync.h>
#include <sys/psema_cntl.h>
#include <ulocks.h>
#include <strings.h>
#include <sys/stat.h>
#include "mp_extern.h"
#include "debug.h"
#include "mplib.h"

/*
 * Posix.1b-1993 Semaphore Interfaces
 */

typedef struct nsem_s {
	sem_t ns_sem;	/* ns_sem must be first field */
	ino_t ns_ino;
	dev_t ns_dev;
	struct nsem_s *ns_next;
} nsem_t;

static nsem_t *_ns_list;

/*
 * sem_open:	open/create a named semaphore
 *
 * Returns:	pointer to a semaphore on success, else -1
 */
/* sem_open(const char *name, int oflag, mode_t mode, unsigned int value)
 */
sem_t *
sem_open(const char *name, int oflag, ...)
{
        mode_t	mode;
        uint	value;
        va_list	ap;
	nsem_t	*ns;
	sem_t   *semp;
	int	fd;
	int	error;
	struct	stat info;
	LOCKDECL(l);

        va_start(ap, oflag);
        mode = va_arg(ap, mode_t);
        value = va_arg(ap, unsigned int);
        va_end(ap);

	if (value > (unsigned int) SEM_SGI_VALUE_MAX) {
		setoserror(EINVAL);
		return (SEM_FAILED);
	}

        if ((fd = psema_cntl(PSEMA_OPEN, name, oflag, mode, value)) == -1) {
		/*
		 * If we ran out of resources, check to see if the semaphore
		 * was already opened by the process before failing.
		 */
		error = oserror();
		if (error != EMFILE && error != ENFILE && error != ENOSPC)
			return (SEM_FAILED);
	}

	if (stat(name, &info)) {
		if (fd != -1)
			(void) psema_cntl(PSEMA_CLOSE, fd);
		else
			setoserror(error);
		return (SEM_FAILED);
	}

	LOCKINIT(l, LOCKOPEN);

	/*
	 * Scan list for previous open
	 */
	for (ns = _ns_list; ns != NULL; ns = ns->ns_next) {
		if ((ns->ns_ino == info.st_ino) &&
		    (ns->ns_dev == info.st_dev))
			break;
	}

	if (ns && !(ns->ns_sem.sem_flags & SEM_FLAGS_UNLINKED)) {
		/*
		 * This semaphore has already been opened, and it hasn't
		 * been unlinked. Use the previously allocated descriptor.
		 */
		semp = &ns->ns_sem;
		UNLOCKOPEN(l);
		if (fd != -1)
			(void) psema_cntl(PSEMA_CLOSE, fd);
		return (semp);
	}

	if (fd == -1) {
		/*
		 * The open wasn't successful and the semaphore
		 * wasn't previously opened by this process.
		 */
		UNLOCKOPEN(l);
		return (SEM_FAILED);
	}

	/*
	 * This is the first open.
	 * Create and initialize a descriptor for it.
	 */
	if ((ns = (nsem_t *) malloc(sizeof(nsem_t))) == 0) {
		UNLOCKOPEN(l);
		(void) psema_cntl(PSEMA_CLOSE, fd);
		setoserror(ENOSPC);
		return (SEM_FAILED);
	}

	ns->ns_sem.sem_nfd = fd;
	ns->ns_sem.sem_flags = SEM_FLAGS_NAMED | SEM_FLAGS_PRIO;
	ns->ns_sem.sem_rlevel = 0;
	ns->ns_sem.sem_xtrace = NULL;

	ns->ns_ino = info.st_ino;
	ns->ns_dev = info.st_dev;

	/*
	 * Insert at head of list
	 */
	ns->ns_next = _ns_list;
	_ns_list = ns;
	
	UNLOCKOPEN(l);
	return (&ns->ns_sem);
}

/*
 * sem_close:	Closes a named semaphore.  Attempts to close an
 *		unnamed semaphore will result in failure.		
 *
 * Returns:	0 on success, else -1
 * Errors:	EINVAL 
 */
int
sem_close(sem_t *semp)
{
	nsem_t *ns = (nsem_t *) semp;
	nsem_t *ps;
	nsem_t *prev = NULL;
	int fd;
	LOCKDECLINIT(l, LOCKOPEN);

	/*
	 * Make sure this is a valid named semaphore
	 */
	for (ps = _ns_list; ps != NULL; ps = ps->ns_next) {
		if (ps == ns)
			break;
		prev = ps;
	}

	if (ps == NULL || !(semp->sem_flags & SEM_FLAGS_NAMED)) {
		UNLOCKOPEN(l);
		setoserror(EINVAL);
		return -1;
	}

	/*
	 * Remove from list
	 */
	if (prev)
		prev->ns_next = ns->ns_next;
	else
		_ns_list = ns->ns_next;

	UNLOCKOPEN(l);

	semp->sem_flags = 0;
	fd = semp->sem_nfd;

	free(ns);

	return (psema_cntl(PSEMA_CLOSE, fd));
}

int
sem_unlink(const char *name)
{
	struct stat info;
	nsem_t *ns;
	LOCKDECL(l);
 
	if (stat(name, &info))
		return -1;

	LOCKINIT(l, LOCKOPEN);

	for (ns = _ns_list; ns != NULL; ns = ns->ns_next) {
		if ((ns->ns_ino == info.st_ino) &&
		    (ns->ns_dev == info.st_dev)) {
			ns->ns_sem.sem_flags |= SEM_FLAGS_UNLINKED;
			break;
		}
	}

	UNLOCKOPEN(l);

	return (psema_cntl(PSEMA_UNLINK, name));
}

/*
 * sem_init:	Initialize an unnamed semaphore.  Initialization
 *		assumes semp points to a valid unnamed semaphore
 *		structure, as there is no way to verify this.
 *	
 * Returns:	0 on success, else -1
 * Errors:	EINVAL, ENOSPC, EPERM
 */
int
sem_init(sem_t *semp, int pshared, unsigned int value)
{
	if (value > SEM_SGI_VALUE_MAX) {
		setoserror(EINVAL);
		return -1;
	}

	semp->sem_value = (int) value;
	semp->sem_flags = SEM_FLAGS_UNNAMED | SEM_FLAGS_PRIO;
	semp->sem_rlevel = 0;
	semp->sem_xtrace = NULL;

	if (pshared == 0 && MTLIB_ACTIVE()) {

		/* Avoid setting SEM_FLAGS_NOSHARE unless mtlib
		 * is active otherwise if it is loaded later we
		 * may call back.
		 */
		semp->sem_flags |= SEM_FLAGS_NOSHARE;
		MTLIB_RETURN( (MTCTL_SEM_ALLOC, semp) );
	}

	return 0;
}

/*
 * sem_destroy:	destroy an unnamed semaphore
 *
 * Returns:	0 on success, else -1
 * Errors:	EBUSY, EINVAL
 */
int
sem_destroy(sem_t *semp)
{
	/*
	 * Check if semp is a valid, unnamed semaphore
	 */
	if (semp->sem_flags & SEM_FLAGS_UNNAMED) {
		if (semp->sem_value < 0) {
			setoserror(EBUSY);
			return -1;
		}

		/* Let the multithreaded version clean up - may fail
		 */
		if (semp->sem_flags & SEM_FLAGS_NOSHARE) {
			if (MTLIB_VAL( (MTCTL_SEM_FREE, semp), 0 )) {
				return (-1);
			}
		}

		semp->sem_flags = 0;
		return 0;
	}

	setoserror(EINVAL);
	return -1;
}

/*
 * sem_wait:	wait (p) operation on a semaphore
 *
 * Returns:	0 on success, else -1
 * Errors:	EINVAL, EINTR
 */
int
sem_wait(sem_t *semp)
{
	usync_arg_t sarg;
	int ret = 0;
	int spins;

	MTLIB_CNCL_TEST();

	if (semp->sem_flags & SEM_FLAGS_UNNAMED) {

		SEM_METER(semp, SM_WAIT, 1);

		if (semp->sem_flags & SEM_FLAGS_SPINNER)
			spins = semp->sem_spins;
		else
			spins = 0;

		if (_r4k_sem_wait(semp, spins)) {
			/*
			 * Semaphore unavailable, block!
			 */
			SEM_METER(semp, SM_BLOCK, 1);
			SEM_DEBUG(semp, SD_LAST);

			/* Call back for threaded version if not shared
			 */
			if (semp->sem_flags & SEM_FLAGS_NOSHARE) {
				ret = MTLIB_VAL( (MTCTL_SEM_WAIT, semp), -1 );

			} else {
				extern int __usync_cntl(int, void *);

				sarg.ua_version = USYNC_VERSION_2;
				sarg.ua_addr = (__uint64_t) semp;
				sarg.ua_policy = USYNC_POLICY_PRIORITY;

				sarg.ua_userpri = MTLIB_VAL( (MTCTL_PRI_HACK),
							     -1 );
				if (sarg.ua_userpri >= 0)
					sarg.ua_flags = USYNC_FLAGS_USERPRI;
				else
					sarg.ua_flags = 0;

				MTLIB_BLOCK_CNCL_VAL(
					ret,
					__usync_cntl(USYNC_INTR_BLOCK, &sarg) );
			}

			_r4k_sem_rewind(semp);

			SEM_METER(semp, SM_BLOCK, -1);
		} else
			SEM_METER(semp, SM_HIT, 1);

		SEM_DEBUG(semp, SD_OWNER);
		return ret;
	}

	if (semp->sem_flags & SEM_FLAGS_NAMED) {
		int userpri;
		int cmd;
		extern int __psema_cntl(int, ...);

		userpri = MTLIB_VAL( (MTCTL_PRI_HACK), -1 );
		if (userpri >= 0)
			cmd = PSEMA_WAIT_USERPRI;
		else
			cmd = PSEMA_WAIT;

		MTLIB_BLOCK_CNCL_RET(
			int,
			__psema_cntl(cmd, semp->sem_nfd, userpri) );
	}

	setoserror(EINVAL);
	return -1;
}

/*
 * sem_trywait:	trywait operation on a semaphore
 *
 * Returns:	0 on success, else -1
 * Errors:	EINVAL, EAGAIN
 */
int
sem_trywait(sem_t *semp)
{
	usync_arg_t sarg;

	if (semp->sem_flags & SEM_FLAGS_UNNAMED) {
		if (_r4k_sem_trywait(semp)) {
			/*
			 * Semaphore is not immediately available.
			 * Check if a prepost is pending.  If so,
			 * consume it.
			 */
			if (semp->sem_flags & SEM_FLAGS_NOSHARE) {
				if (MTLIB_VAL( (MTCTL_SEM_TRYWAIT, semp), 0 )){
					return (-1);
				}
			} else { /* fall into default */ 

				sarg.ua_version = USYNC_VERSION_2;
				sarg.ua_addr = (__uint64_t) semp;
				sarg.ua_flags = USYNC_FLAGS_PREPOST_CONSUME;
				if (usync_cntl(USYNC_GET_STATE, &sarg) <= 0) {
					/*
					 * A prepost was not pending.
					 * Semaphore is not available.
					 */
					setoserror(EAGAIN);
					return -1;
				}
			}
		}

		/*
		 * Got it
		 */
		if (SEM_TRACE_ENABLED(semp)) {
			SEM_METER(semp, SM_WAIT, 1);
			SEM_METER(semp, SM_HIT, 1);
			SEM_DEBUG(semp, SD_OWNER | SD_LAST);
		}

		return 0;
	}

	if (semp->sem_flags & SEM_FLAGS_NAMED)
    		return (psema_cntl(PSEMA_TRYWAIT, semp->sem_nfd));

	setoserror(EINVAL);
	return -1;
}

/*
 * sem_post:	post (v) operation on a semaphore
 *
 * Returns:	0 on success, else -1
 * Errors:	EINVAL
 */
int
sem_post(sem_t *semp)
{
	usync_arg_t sarg;
	int ret = 0;

	if (semp->sem_flags & SEM_FLAGS_UNNAMED) {

		if (SEM_TRACE_ENABLED(semp)) {
			SEM_METER(semp, SM_POST, 1);
			SEM_DEBUG(semp, SD_LAST);
		}

		if (_r4k_sem_post(semp)) {
			/*
			 * Wakeup a waiting process
			 */
			if (semp->sem_flags & SEM_FLAGS_NOSHARE) {
				ret = MTLIB_VAL( (MTCTL_SEM_POST, semp), -1 );
			} else {
				sarg.ua_version = USYNC_VERSION_2;
				sarg.ua_addr = (__uint64_t) semp;
				sarg.ua_flags = 0;
				ret = usync_cntl(USYNC_UNBLOCK, &sarg);
			}
		} else
			SEM_METER(semp, SM_NOWAITERS, 1);

		return ret;
	}

	if (semp->sem_flags & SEM_FLAGS_NAMED)
		return (psema_cntl(PSEMA_POST, semp->sem_nfd));

	setoserror(EINVAL);
	return -1;
}

/*
 * sem_getvalue:  obtain the semaphore count value.  If the returned
 *                sval value is negative, the absolute value of sval
 *                indicates the number of processes currently blocked
 *                on the semaphore.
 *
 * Returns:	0 on success, else -1
 * Errors:	EINVAL
 */
int
sem_getvalue(sem_t *semp, int *sval)
{
  	usync_arg_t sarg;
	int tmp;

	if (semp->sem_flags & (SEM_FLAGS_UNNAMED | SEM_FLAGS_ARENA)) {

		if (semp->sem_flags & SEM_FLAGS_NOSHARE) {
			MTLIB_RETURN( (MTCTL_SEM_GETVALUE, semp, sval) );
		}

		/*
		 * Computing the value of an unnamed semaphore is a little
		 * tricky because the semaphore state is split between libc
		 * and the kernel.
		 * 
		 * Calling this routine is inherently racy, so it's mainly
		 * used for debugging and speed isn't essential.
		 *
		 * We always have to check the kernel maintained state to
		 * find-out how many waiters are actually blocked, since
		 * the the semaphore value is not incremented (when waking-up
		 * a process) until after the *waiter* has run and rewound the
		 * value during the backend of sem_wait.
		 *
		 * USYNC_GET_STATE Returns
		 *
		 *	<= 0 : number of waiters
		 *	>  0 : prepost value
		 */
		sarg.ua_version = USYNC_VERSION_2;
		sarg.ua_addr = (__uint64_t) semp;
		sarg.ua_flags = 0;
		if ((tmp = usync_cntl(USYNC_GET_STATE, &sarg)) >= 0) {
			/* No waiters. Use sem_value and add the preposts */
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

	if (semp->sem_flags & SEM_FLAGS_NAMED)
		return (psema_cntl(PSEMA_GETVALUE, semp->sem_nfd, sval));

	setoserror(EINVAL);
	return -1;
}

int
sem_mode(sem_t *semp, int cmd, ...)
{
	va_list ap;
	sem_trace_t *xtrace;
	semameter_t *met;
	semadebug_t *dbg;

	if (semp->sem_flags & SEM_FLAGS_NAMED) {
		setoserror(EINVAL);
		return -1;
	}

	if (semp == NULL) {
		setoserror(EINVAL);
		return -1;
	}

	va_start(ap, cmd);	

	switch(cmd) {
	case SEM_MODE_TRACEINIT:
		xtrace = va_arg(ap, sem_trace_t *);
		if (xtrace == NULL) {
			setoserror(EINVAL);
			return -1;
		}

		semp->sem_xtrace = xtrace;

		xtrace->sem_waithits	= 0;
		xtrace->sem_waits	= 0;
		xtrace->sem_posts	= 0;
		xtrace->sem_posthits	= 0;
		xtrace->sem_waiters	= 0;
		xtrace->sem_maxwaiters	= SEM_SGI_VALUE_MAX;

		xtrace->sem_owner_pid	= -1;
		xtrace->sem_owner_tid	= -1;
		xtrace->sem_owner_pc	= (char *) -1L;
		xtrace->sem_last_pid	= -1;
		xtrace->sem_last_tid	= -1;
		xtrace->sem_last_pc	= (char *) -1L;

		return (spin_init((spinlock_t*) &xtrace->__st_l));

	case SEM_MODE_METERON:
		if (semp->sem_xtrace)
			semp->sem_flags |= SEM_FLAGS_METER;
		else {
			setoserror(EINVAL);
			return -1;
		}
		break;

	case SEM_MODE_METEROFF:
		semp->sem_flags &= ~SEM_FLAGS_METER;
		break;

	case SEM_MODE_METERRESET:
		if (semp->sem_xtrace) {
			xtrace = semp->sem_xtrace;
			xtrace->sem_waithits = 0;
			xtrace->sem_waits    = 0;
			xtrace->sem_posts    = 0;
			xtrace->sem_posthits = 0;
			xtrace->sem_waiters  = 0;
			if (semp->sem_flags & SEM_FLAGS_POLLED) {
				/*
				 * The arena limit only applys to polled semas
				 */
				ushdr_t *h = (ushdr_t *) semp->sem_usptr;
				xtrace->sem_maxwaiters = h->u_maxtidusers;
			} else
				xtrace->sem_maxwaiters = SEM_SGI_VALUE_MAX;
		} else {
			setoserror(EINVAL);
			return -1;
		}
		break;

	case SEM_MODE_DEBUGON:
		if (semp->sem_xtrace)
			semp->sem_flags |= SEM_FLAGS_DEBUG;
		else {
			setoserror(EINVAL);
			return -1;
		}
		break;

	case SEM_MODE_DEBUGOFF:
		semp->sem_flags &= ~SEM_FLAGS_DEBUG;
		break;

	case SEM_MODE_DEBUGRESET:
		if (semp->sem_xtrace) {
			xtrace = semp->sem_xtrace;
			xtrace->sem_owner_pid = -1;
			xtrace->sem_owner_tid = -1;
			xtrace->sem_owner_pc  = (char *) -1L;
			xtrace->sem_last_pid  = -1;
			xtrace->sem_last_tid  = -1;
			xtrace->sem_last_pc   = (char *) -1L;
		}
		break;

	case SEM_MODE_METERFETCH:
		if (semp->sem_xtrace) {
			xtrace = semp->sem_xtrace;
			met = va_arg(ap, semameter_t *);
			met->sm_phits    = xtrace->sem_waithits;
			met->sm_psemas   = xtrace->sem_waits;
			met->sm_vsemas   = xtrace->sem_posts;
			met->sm_vnowait  = xtrace->sem_posthits;
			met->sm_nwait    = xtrace->sem_waiters;
			met->sm_maxnwait = xtrace->sem_maxwaiters;
		} else {
			setoserror(EINVAL);
			return -1;
		}
		break;

	case SEM_MODE_DEBUGFETCH:
		if (semp->sem_xtrace) {
			xtrace = semp->sem_xtrace;
			dbg = va_arg(ap, semadebug_t *);
			dbg->sd_owner_pid = xtrace->sem_owner_pid;
			dbg->sd_owner_pc  = xtrace->sem_owner_pc;
			dbg->sd_last_pid  = xtrace->sem_last_pid;
			dbg->sd_last_pc   = xtrace->sem_last_pc;
		} else {
			setoserror(EINVAL);
			return -1;
		}
		break;

	case SEM_MODE_SPINSET:
		if (_us_systype == 0)
			_us_systype = _getsystype();

		if (_us_systype & US_MP) {
			int spins = va_arg(ap, int);
			if (spins < 0) {
				setoserror(EINVAL);
				return -1;
			}

			semp->sem_spins = spins;

			if (spins == 0)
				semp->sem_flags &= ~SEM_FLAGS_SPINNER;
			else
				semp->sem_flags |= SEM_FLAGS_SPINNER;
		}
		break;

	case SEM_MODE_NOCNCL:
		semp->sem_flags |= SEM_FLAGS_NOCNCL;
		break;

	default:
		setoserror(EINVAL);
		return -1;
	};

	return 0;
}

/*
 * Semaphore print
 */
int
sem_print(sem_t *semp, FILE *fd, const char *n)
{
	char iobuf[350];
	char stype[40];
	int value;

	if (semp->sem_flags & SEM_FLAGS_NAMED) {
		setoserror(EINVAL);
		return -1;
	}

	if (semp->sem_flags & SEM_FLAGS_UNNAMED)
		sprintf(stype, "POSIX unnamed with priority queuing");
	else if (semp->sem_flags & SEM_FLAGS_POLLED)
		sprintf(stype, "ARENA polled with FIFO queuing");
	else
		sprintf(stype, "ARENA standard with FIFO queuing");

	(void) sem_getvalue(semp, &value);

	if (!(semp->sem_flags & SEM_FLAGS_SPINNER))
		semp->sem_spins = 0;

	sprintf(iobuf, "Semaphore at %p vbase %d depth %d spin %d",
		semp, value, semp->sem_rlevel, semp->sem_spins);

	if (semp->sem_flags & SEM_FLAGS_POLLED)
		sprintf(&iobuf[strlen(iobuf)],
			" polling dev %d\n", semp->sem_dev);
	else
		sprintf(&iobuf[strlen(iobuf)], "\n");

	sprintf(&iobuf[strlen(iobuf)], "  Type : %s\n", stype);

	if (semp->sem_flags & SEM_FLAGS_METER) {
		sprintf(&iobuf[strlen(iobuf)],
			"  Meter: value %d waiters %d maxwaiters %d\n", 
			ustestsema(semp),
			semp->sem_xtrace->sem_waiters,
			semp->sem_xtrace->sem_maxwaiters);

		sprintf(&iobuf[strlen(iobuf)],
			"         waits %d waithits %d posts %d posthits %d\n",
			semp->sem_xtrace->sem_waits,
			semp->sem_xtrace->sem_waithits, 
			semp->sem_xtrace->sem_posts, 
			semp->sem_xtrace->sem_posthits);
	}

	if (semp->sem_flags & SEM_FLAGS_DEBUG) {
		sprintf(&iobuf[strlen(iobuf)],
			"  Debug: owner pid %d:%d called from 0x%p\n", 
			semp->sem_xtrace->sem_owner_pid,
			semp->sem_xtrace->sem_owner_tid,
			semp->sem_xtrace->sem_owner_pc);
		sprintf(&iobuf[strlen(iobuf)],
			"         last  pid %d:%d called from 0x%p\n",
			semp->sem_xtrace->sem_last_pid,
			semp->sem_xtrace->sem_last_tid,
			semp->sem_xtrace->sem_last_pc);
	}

	if (fprintf(fd, "%s: %s", n, iobuf) < 0) {
		setoserror(EBADF);
		return -1;
	}

	return(0);
}
