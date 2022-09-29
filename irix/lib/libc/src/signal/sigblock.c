/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.10 $"

/*
 * Library routine supplying the entry point for BSD-compatible sigblock()
 * sys call.  Sigblock() uses POSIX sigprocmask() system call
 */

#define _BSD_SIGNALS

#ifdef __STDC__
	#pragma weak sigblock = _sigblock
#endif
#include "synonyms.h"

#include <stdio.h>
#undef signal			/* redefined to BSDsignal in signal.h */
#undef sigpause			/* redefined to BSDsigpause in signal.h */
#include <signal.h>
#include "sigcompat.h"

int
sigblock(mask)
	int mask;
{
	sigset_t oset;
	sigset_t nset;

	sigemptyset(&nset);
	mask2set(mask, &nset);
	sigdelset(&nset, SIGCONT);
	sigdelset(&nset, SIGKILL);
	sigdelset(&nset, SIGSTOP);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);
	return ((int)set2mask(&oset));

} /* end sigblock() */
