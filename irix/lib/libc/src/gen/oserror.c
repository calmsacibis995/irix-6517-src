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
#ident "$Revision: 1.8 $ $Author: jwag $"


#ifdef __STDC__
	#pragma weak oserror = _oserror
	#pragma weak setoserror = _setoserror
	#pragma weak goserror = _goserror
#endif
#include "synonyms.h"
#include "errno.h"
#include "ulocks.h"
#include "sys/prctl.h"
#include "priv_extern.h"

/* 
	Undefine "errno" because, libc makefiles define _SGI_MP_SOURCE.
	With _SGI_MP_SOURCE defined errno.h defines "errno" to be :

	#if defined(_SGI_MP_SOURCE)
	#define errno   (*__oserror())
	 [ stuff deleted ]
	#endif

	This will cause the wrong definition of "errno" to be used in this routine.
*/

/*
 * errno handling:
 * PREMISE1: we believe that POSIX/ABI require that by default errno
 *	must be a global int
 * GOAL1: plain non-threaded programs should just work - accessing
 *	the global errno.
 * GOAL2: threaded programs, compiled with the feature test macro _SGI_MP_SOURCE
 *	should get per-thread errno
 * GOAL3: 'mistakes' such as compiling a threaded program w/o _SGI_MP_SOURCE
 *	should fail softly - if only 1 thread executes a function that sets
 *	errno, it should get the correct errno.
 * GOAL4: we can't really require that all libraries that set or look at
 *	errno be modified immediately
 *
 * SOLUTION:
 *	1) there is a global errno and a global errno address pointer.
 *		for single threaded jobs __errnoaddr = &errno.
 *	2) upon becoming multi-threaded, __errnoaddr is changed to point to
 *		a per-thread variable.o
 * PROBLEM1: libc now calls sproc implcitly for aio and system thus
 *	even 'normal' programs call sproc.
 * SOLUTION1: for safety sake - sproc, when called with PR_NOLIBC as
 *	both aio and system/pcreate do, will NOT point __errnoaddr to
 *	the per-thread errno
 *
 * SCENARIO1: app not compiled with _SGI_MP_SOURCE but calls sproc and
 *		explicitly references errno.
 *	--> when app references errno it is referencing the global
 *	THUS - ALL setting of errno within libraries must at least set the
 *		global errno and preferably BOTH errno and *__errnoaddr
 * SCENARIO2: app compiled with _SGI_MP_SOURCE, calls sproc, references errno
 *		and links with some libraries that set errno but have NOT
 *		been modified to be 'good'
 *	--> when the library errs and sets errno - it will set the global.
 *	--> when the app references errno it will get the per-thread WRONG!!
 *	--> if the app calls oserror/perror these will also
 *	    grab the per-thread value WRONG!.
 *	The workaround for the above 2 errors is to either use goserror()
 *		to reference the global error value OR not
 *		compile with _SGI_MP_SOURCE set in files that reference
 *		these non-converted libraries.
 *
 * RULES:
 *	If a library wishes to be MP safe it must call setoserror() to
 *	set errno. If it reads errno it must either:
 *		1) use oserror() OR
 *		2) compile with _SGI_MP_SOURCE set.
 *	
 */
#undef errno
extern int errno;
int *__errnoaddr = &errno;

/*
 * oserror - return system errno
 * setoserror - set os error
 * goserror - return global errno always.
 */

int
oserror(void)
{
	return(*__errnoaddr);
}

int
setoserror(int err)
{
	/*
	 * always set global errno in case someone hasn't
	 * compiled with -D_SGI_MP_SOURCE
	 */
	errno = err;
	*__errnoaddr = err;
	return(err);
}

int
goserror(void)
{
	return(errno);
}

/*
 * A function (defined as errno in errno.h) that returns an
 * lval for errno
 */
int *
__oserror(void)
{
	return(__errnoaddr);
}

void
__initperthread_errno(void)
{
	__errnoaddr = (int *)&PRDALIB->t_errno;
}

