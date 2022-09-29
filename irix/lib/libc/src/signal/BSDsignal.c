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

#ident "$Revision: 1.14 $"

/*
 * Library routine supplying the entry points for BSD-compatible signal()
 * routine.  BSDsignal() uses POSIX sigaction() system call
 */

#define _BSD_SIGNALS

#ifdef __STDC__
#if !_LIBC_ABI
	#pragma weak BSDsignal = _BSDsignal
#endif
	#pragma weak bsd_signal = _bsd_signal
#endif
#include "synonyms.h"
#include <stdio.h>
#include <errno.h>
#undef signal			/* redefined to BSDsignal in signal.h */
#undef sigpause			/* redefined to BSDsigpause in signal.h */
#include <signal.h>

static void (*
bsd_sig_common(int sig, void (*func)(int sig), int flag))(int sig)
{
	sigaction_t sa;		/* signal actions from/to sigaction() syscall */
	void (*oldact)();

	/* note that signals come in--and are passed to sigaction()--numbering
	   from 1 --> NUMBSDSIGS, not as they are stored in bitmasks */
	if ((sig <= 0) || (sig > NUMBSDSIGS)) {		/* invalid sig # */
		_setoserror(EINVAL);
		return((void (*)())-1L);
	}
	/* SIGCONT is ignored by default; error if request to do so */
	if ((sig == SIGCONT) && (func == SIG_IGN)) {
		_setoserror(EINVAL);
		return((void (*)())-1L);
	}

	if (sigaction(sig,0,&sa))	/* get current handling for signal */
		return((void (*)())-1L);
	oldact = sa.sa_handler;	/* return the previous handling */
	sa.sa_handler = func;
	/* sa_mask is not altered except to ensure SIGCONT is not blocked */
	sigdelset(&sa.sa_mask, SIGCONT);
	sa.sa_flags |= _SA_BSDCALL | flag;
	sa.sa_flags &= ~SA_NOCLDSTOP;
	if (sigaction(sig,&sa,0))
		return((void (*)())-1L);
	return(oldact);
}

void (*
bsd_signal(int sig, void (*func)(int sig)))(int sig)
{
	return(bsd_sig_common(sig, func, SA_RESTART));
}

void (*
BSDsignal(int sig, void (*func)()))()
{
	return(bsd_sig_common(sig, func, 0));
}
