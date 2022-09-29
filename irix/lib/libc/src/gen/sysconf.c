/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/sysconf.c	1.7"

/* sysconf(3C) - returns system configuration information
*/

#ifdef __STDC__
	#pragma weak sysconf = _sysconf
#endif
#include "synonyms.h"
#include "mplib.h"
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syssgi.h>
#include <sys/sysmp.h>
#include <limits.h>
#include <ttymap.h>
#include <time.h>
#include <aio.h>
#include <semaphore_internal.h>
#include <mqueue.h>
#include <sys/uio.h>

/* 
 * These are here for binary compatibility. The ABI moved the SC numbers
 * and changed values for AIO max
 */
#define _SC_ASYNCHRONOUS_IO_OLD	22
#define _SC_AIO_MAX_OLD		18
#define _SC_AIO_LISTIO_MAX_OLD	19	

#define _OLD_AIO_MAX		4	/* from aio/old_aio.h */
#define _OLD_AIO_LISTIO_MAX	255	/* from aio/old_aio.h */
extern long __pthread_sysconf(int);

long
sysconf(int name)
{
	char *xpg_env;

	switch(name) {
		case _SC_JOB_CONTROL:
			return(_POSIX_JOB_CONTROL);	/* unistd.h */
		case _SC_SAVED_IDS:
			return(_POSIX_SAVED_IDS);	/* unistd.h */
		case _SC_PASS_MAX:
			return(PASS_MAX);
		case _SC_TZNAME_MAX:
			return(TZNAME_MAX);
		case _SC_NACLS_MAX:
			return(0);
		case _SC_NPROC_CONF:
			return(sysmp(MP_NPROCS));
		case _SC_NPROC_ONLN:
			return(sysmp(MP_NAPROCS));
		case _SC_LOGNAME_MAX:
			return(LOGNAME_MAX);
		case _SC_STREAM_MAX:
			return(syssgi(SGI_SYSCONF,_SC_OPEN_MAX));
		/*
		 * 1003.1b additions
		 */
		/* limits */
		case _SC_AIO_LISTIO_MAX:
			return(_AIO_SGI_LISTIO_MAX);	/* aio.h */
		case _SC_AIO_LISTIO_MAX_OLD:
			return(_OLD_AIO_LISTIO_MAX);	/* aio.h */
		case _SC_AIO_MAX:
			return(_AIO_SGI_MAX);		/* aio.h */
	        case _SC_AIO_MAX_OLD:
			return(_OLD_AIO_MAX);		/* aio.h */
		case _SC_AIO_PRIO_DELTA_MAX:
			return(_AIO_SGI_PRIO_DELTA_MAX);
		case _SC_DELAYTIMER_MAX:
			return(_POSIX_DELAYTIMER_MAX);
		case _SC_PAGESIZE:
			return(getpagesize());
		case _SC_MQ_OPEN_MAX:
			return(syssgi(SGI_SYSCONF,_SC_MQ_OPEN_MAX));
		case _SC_MQ_PRIO_MAX:
			return(_MQ_SGI_PRIO_MAX);
		case _SC_RTSIG_MAX:
			return(SIGRTMAX-SIGRTMIN+1);	/* signal.h */
		case _SC_SIGQUEUE_MAX:
			return(syssgi(SGI_SYSCONF,_SC_SIGQUEUE_MAX));
		case _SC_SEM_NSEMS_MAX:
			return(SEM_SGI_NSEMS_MAX);
		case _SC_SEM_VALUE_MAX:
			return(SEM_SGI_VALUE_MAX);
		case _SC_TIMER_MAX:
			return(_POSIX_TIMER_MAX);
		/* 1003.1b options */
		case _SC_ASYNCHRONOUS_IO:
		case _SC_ABI_ASYNCHRONOUS_IO: /* ABI and POSIX are same  */
	        case _SC_ASYNCHRONOUS_IO_OLD:
			return(_POSIX_ASYNCHRONOUS_IO); /* unistd.h */
	    	case _SC_FSYNC:
		    	return(_POSIX_FSYNC);
		case _SC_MAPPED_FILES:
			return(_POSIX_MAPPED_FILES);
		case _SC_MEMLOCK:
			return(_POSIX_MEMLOCK);
		case _SC_MEMLOCK_RANGE:
			return(_POSIX_MEMLOCK_RANGE);
		case _SC_MEMORY_PROTECTION:
			return(_POSIX_MEMORY_PROTECTION);
		case _SC_MESSAGE_PASSING:
			return(_POSIX_MESSAGE_PASSING);
		case _SC_PRIORITIZED_IO:
#ifdef _POSIX_PRIORITIZED_IO
			return 1;
#else
			return -1;
#endif
		case _SC_PRIORITY_SCHEDULING:
#ifdef _POSIX_PRIORITY_SCHEDULING
			return 1;
#else
			return -1;
#endif
		case _SC_REALTIME_SIGNALS:
			return(_POSIX_REALTIME_SIGNALS); /* unistd.h */
		case _SC_SEMAPHORES:
			return(_POSIX_SEMAPHORES);
		case _SC_SHARED_MEMORY_OBJECTS:
			return(_POSIX_SHARED_MEMORY_OBJECTS);
	    	case _SC_SYNCHRONIZED_IO:
		    	return(_POSIX_SYNCHRONIZED_IO);
		case _SC_TIMERS:
			return(_POSIX_TIMERS);

	/*
	 * The following variables are new to XPG4.
	 */
                case _SC_XOPEN_VERSION:
			return(_XOPEN_VERSION);
  		case _SC_BC_BASE_MAX:	
			return(_POSIX2_BC_BASE_MAX);
  		case _SC_BC_DIM_MAX:	
			return(_POSIX2_BC_DIM_MAX);
  		case _SC_BC_SCALE_MAX:  
			return(_POSIX2_BC_SCALE_MAX);
  		case _SC_BC_STRING_MAX: 
			return(_POSIX2_BC_STRING_MAX);
  		case _SC_COLL_WEIGHTS_MAX: 
			return(_POSIX2_COLL_WEIGHTS_MAX);
  		case _SC_EXPR_NEST_MAX: 
			return(_POSIX2_EXPR_NEST_MAX);
  		case _SC_RE_DUP_MAX:	
			return(_POSIX2_RE_DUP_MAX);
  		case _SC_LINE_MAX:	
			return(_POSIX2_LINE_MAX);

		case _SC_XOPEN_SHM:
			/*
			 * We assume that noone configs out shm anymore.
			 * Note that just calling a shm routine and checking
			 * for ENOSYS isn't so hot since that also generates
			 * a SIGSYS ..
			 */
			return 1L;
			
		case _SC_XOPEN_CRYPT:
			/*
			 * The encryption routines crypt(), encrypt() and
                         * setkey() are always provided, and hence the setting
			 * for this X/Open Feature Group is to return true.
			 * Note that in certain markets
                         * the decryption algorithm may not be exported
                         * and in that case, encrypt() returns ENOSYS for
                         * the decryption operation.
			 */
			return (1L);

  		case _SC_2_C_BIND:
			return(_POSIX2_C_BIND);

  		case _SC_2_LOCALEDEF:
			return(_POSIX2_LOCALEDEF);

  		case _SC_2_C_VERSION:
			return(_POSIX2_C_VERSION);

  		case _SC_2_VERSION:
                        if ((xpg_env = getenv("_XPG")) != NULL && atoi(xpg_env) > 0) {
                                return(_POSIX2_VERSION);
                        } else {
                                return -1;   /* don't touch errno */
                        }

  		case _SC_2_C_DEV:
			return(_POSIX2_C_DEV);

  		case _SC_2_CHAR_TERM:
			return(_POSIX2_CHAR_TERM);

  		case _SC_2_FORT_DEV:
			return(_POSIX2_FORT_DEV);

  		case _SC_2_FORT_RUN:
			return(_POSIX2_FORT_RUN);

  		case _SC_2_SW_DEV:
			return(_POSIX2_SW_DEV);

  		case _SC_2_UPE:
			return(_POSIX2_UPE);

		case _SC_XOPEN_ENH_I18N:
			return(_XOPEN_ENH_I18N);

  		case _SC_XOPEN_UNIX:
			return(_XOPEN_UNIX);

  		case _SC_XOPEN_XCU_VERSION:
			return(_XOPEN_XCU_VERSION);

  		case _SC_ATEXIT_MAX:
			/*
			 * hard coded in libc/atexit.c: define MAXEXITFNS 37
			 *
			 * XXX - when this hard coded value changes to be
			 *	 dynamic, this return will need to be
			 *	 changed to return the dynamic value.
			 */
			return(37);

		/*
		 * POSIX1C - pthreads
		 */
		case _SC_GETGR_R_SIZE_MAX:
		case _SC_GETPW_R_SIZE_MAX:
			/* BOGUS .. */
			return 1024;
		case _SC_LOGIN_NAME_MAX:
			return LOGNAME_MAX+1;
		case _SC_TTY_NAME_MAX:
			return MAX_DEV_PATH;
		case _SC_THREAD_SAFE_FUNCTIONS:
			return 1;

		/*
		 * Allow these to be overridden by thread lib
		 */
		case _SC_THREAD_DESTRUCTOR_ITERATIONS:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
			return _POSIX_THREAD_DESTRUCTOR_ITERATIONS;
		case _SC_THREAD_KEYS_MAX:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
			return _POSIX_THREAD_KEYS_MAX;
		case _SC_THREAD_STACK_MIN:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
			return 0;
		case _SC_THREAD_THREADS_MAX:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
			return _POSIX_THREAD_THREADS_MAX;
		case _SC_THREADS:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
#ifdef _POSIX_THREADS
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_ATTR_STACKADDR:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
#ifdef _POSIX_THREAD_ATTR_STACKADDR
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_ATTR_STACKSIZE:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_PRIORITY_SCHEDULING:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_PRIO_INHERIT:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
#ifdef _POSIX_THREAD_PRIO_INHERIT
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_PRIO_PROTECT:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
#ifdef _POSIX_THREAD_PRIO_PROTECT
			return 1;
#else
			return -1;
#endif
		case _SC_THREAD_PROCESS_SHARED:
			MTLIB_RETURN( (MTCTL_SYSCONF, name) );
#ifdef _POSIX_THREAD_PROCESS_SHARED
			return 1;
#else
			return -1;
#endif

		/*
		 * XPG5
		 */
		case _SC_XBS5_ILP32_OFF32:
			return _XBS5_ILP32_OFF32;
		case _SC_XBS5_ILP32_OFFBIG:
			return _XBS5_ILP32_OFFBIG;
		case _SC_XOPEN_LEGACY:
			/* always include LEGACY features */
			return 1L;
		case _SC_XOPEN_REALTIME:
			return _XOPEN_REALTIME;

		default:
			return(syssgi(SGI_SYSCONF,name));
	}
}
