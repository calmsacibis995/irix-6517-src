/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*  cuexit.c $Revision: 7.13 $ */


#ifdef __STDC__
	#pragma weak exit = __exit
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/prctl.h>
#include <rld_interface.h>
#include <unistd.h>
#include "gen_extern.h"
#include "mplib.h"

/* used by libfpe and Fortran libF77/main.c to talk to each other.
   unfortunately, both Fortran and libfpe are optional in a compile,
   so this needs to be someplace common to all languages.
 */
int __trap_fpe_override = 0;

void
__exit(int code)
{
	long	ret;	/* in case PR_LASTSHEXIT not supported */

#ifdef SPECIAL_EXIT
	/* NOTE: this section is now obsolete and the libraries that
	** still use it should instead be changed to use __ateachexit
	** (or atexit) for their cleanup/termination needs.
	*/

	/* 
	   Call the various cleanup routines for SGI task cleanup.
	   The stubs for these routines are in the non-shared part of libc.
	   Note that these routines are NOT called in the libc (or libc_s)
	   version of exit (if you got the version out of libc, then you
	   must not be doing anything that needs a special exit).
	*/
	__checktraps(); /* fpe's */
#if !(_COMPILER_VERSION >= 400)
	/* this isn't used in 6.x and hopefully forever.
	 * We only call __mp_cleanup for older libs
	 */
	__mp_cleanup(); /* auto mp processes */
#endif
	__mpc_cleanup(); /* auto mp processes for C */
	__prof_cleanup(); /* pc-sample profiling */
#endif

	/* Multi-threaded exit (not sprocs).
	 * Does not return.
	 */
	MTLIB_INSERT( (MTCTL_EXIT, code) );

	/*
	 * Call Thread Shutdown - this will clear the fact
	 * that the thread has any locks inside rld.
	 * This is useful if a signal handler needs to exit, but
	 * the signal came in when the mainline thread code was in rld
	 * getting something done
	 * We call this before doing anything since if we do have the lock
	 * and any exit function needs to resolve anything, we'll hang.
	 */
	if (_DYNAMIC_LINK != 0) _rld_new_interface(_RLD_SHUTDOWN_THREAD);

	/* Call registered (via __ateachexit) per-thread exit routines */
	__eachexithandle();

	/* Only call atexit and RLD shutdown if we are the last thread.
	 * Default to old racy PR_GETNSHARE if PR_LASTSHEXIT not supported.
	 */
	if ((ret = _prctl(PR_LASTSHEXIT)) == 1
	    || ret == -1 && _prctl(PR_GETNSHARE) <= 1) {
		/* ANSI - call functions registered with atexit() */
		_exithandle();
		if (_DYNAMIC_LINK != 0) _rld_new_interface(_SHUT_DOWN);
	}
	_cleanup();
	_exit(code);
}
