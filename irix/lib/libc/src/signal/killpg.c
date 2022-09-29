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
 * Library routine supplying the entry point for BSD-compatible killpg()
 * sys call.  Killpg() uses SysV's multi-talented kill() sys call.
 */

#define _BSD_SIGNALS

#ifdef __STDC__
	#pragma weak killpg = _killpg
#endif
#include "synonyms.h"

#include <stdio.h>
#include <errno.h>
#undef signal			/* redefined to BSDsignal in signal.h */
#undef sigpause			/* redefined to BSDsigpause in signal.h */
#include <signal.h>

/*
 * BSD kill(pid,sig) performs ==>NEARLY<== identically to a subset of our
 * SYSV kill(), so no library routine is necessary.  By "nearly", I mean
 * that when *pid* is -1, the calling routine does NOT receive a signal
 * (in both cases--caller is and is not superuser).  Our SYSV kill() 
 * does include the caller in the recipient group.  For now, this is as
 * close to perfect compatibility as we will get.
 */

int
killpg(pid_t pgrp,	/* deliver signal to this process group */
	int sig)	/* signal number to deliver		*/
{
	/* note that signals come in--and are passed to kill()--numbering
	   from 1 --> NUMBSDSIGS, not as they are stored in bitmasks */
	if ((sig <= 0) || (sig > NUMBSDSIGS)) {		/* invalid sig # */
		_setoserror(EINVAL);
		return(-1);
	}
	if (pgrp <= 1) {	/* invalid pgrp number */
		_setoserror(EINVAL);
		return(-1);
	}
	return(kill(-pgrp,sig));

} /* end killpg() */
