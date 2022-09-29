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
#ident "$Revision: 1.15 $"

/*
 * Library routine supplying the entry point for BSD-compatible sigvec()
 * system call.  Sigvec() uses POSIX sigaction() system call
 */

#define _BSD_SIGNALS

#ifdef __STDC__
	#pragma weak sigvec = _sigvec
#endif
#include "synonyms.h"

#include <errno.h>
#include <stdio.h>
#undef signal			/* redefined to BSDsignal in signal.h */
#undef sigpause			/* redefined to BSDsigpause in signal.h */
#include <signal.h>
#include "sigcompat.h"

/* 
 * The POSIX sigaction has more information than sigvec() and signal()
 * (sa_flags for both, sa_mask for signal), so the previous values are
 * fetched to fill the proper fields of the structs which will now set 
 * the new vals for the signal.
 */

int
sigvec(sig,vec,ovec)
	int sig;		/* the signal number we are concerned with */
	struct sigvec *vec;	/* handle the signal in this way	   */
	struct sigvec *ovec;	/* previously signal was handled like this */
{
	sigaction_t osa, sa;	/* signal actions from/to sigaction() syscall */
	int rv;

	/* note that signals come in--& are passed to sigaction()--numbering
	   from 1 --> NUMBSDSIGS, not as they are stored in bitmasks */
	if ((sig <= 0) || (sig > NUMBSDSIGS)) {		/* invalid sig # */
		_setoserror(EINVAL);
		return(-1);
	}

	/*
	 * SIGCONT is ignored by default; error if request to do so
	 * Since sigaction allows this, we must error check here
	 */
	if ((vec) && (sig == SIGCONT) && (vec->sv_handler == SIG_IGN)) {
		_setoserror(EINVAL);
		return(-1);
	}

	if (vec != NULL) {	/* user wants to change handling */
		sigaction(sig, (struct sigaction *)0, &sa);	/* get old */
		sa.sa_handler = vec->sv_handler;
		mask2set(vec->sv_mask, &sa.sa_mask);
		sigdelset(&sa.sa_mask, SIGCONT);
		sigdelset(&sa.sa_mask, SIGKILL);
		sigdelset(&sa.sa_mask, SIGSTOP);
		sa.sa_flags = (0 | _SA_BSDCALL);    /* clrs SA_NOCLDSTOP bit */
		if (vec->sv_flags & SV_ONSTACK) 
			sa.sa_flags |= SA_ONSTACK; 
		rv = sigaction(sig, &sa, &osa);
	} else
		rv = sigaction(sig, NULL, &osa);
	if (rv)
		return(-1);
	if (ovec != NULL) {	/* user wants current handling info returned */
		ovec->sv_handler = osa.sa_handler;
		ovec->sv_mask = (int)set2mask(&osa.sa_mask);
		ovec->sv_flags = 0;
		if (osa.sa_flags & SA_ONSTACK) 
			ovec->sv_flags |= SV_ONSTACK; 
	}

	return(0);

}
