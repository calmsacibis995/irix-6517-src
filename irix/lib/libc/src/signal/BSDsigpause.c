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

#ident "$Revision: 1.8 $"

/*
 * Library routine supplying the entry point for BSD-compatible sigpause()
 * sys call.  BSDsigpause() uses POSIX sigsuspend() sys call
 */

#define _BSD_SIGNALS

#ifdef __STDC__
	#pragma weak BSDsigpause = _BSDsigpause
#endif
#include "synonyms.h"

#include <stdio.h>
#undef signal			/* redefined to BSDsignal in signal.h */
#undef sigpause			/* redefined to BSDsigpause in signal.h */
#include <signal.h>
#include "sigcompat.h"

int
BSDsigpause(mask)
	int mask;
{
	sigset_t set;

	(void) sigprocmask(SIG_NOP, (sigset_t *)0, &set);
	mask2set(mask, &set);
	return (sigsuspend(&set));
} /* end BSDsigpause() */

