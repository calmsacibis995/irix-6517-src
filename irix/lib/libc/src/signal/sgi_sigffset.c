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

#ident "$Revision: 1.6 $"

#ifdef __STDC__
	#pragma weak sgi_sigffset = _sgi_sigffset
#endif
#include "synonyms.h"
#include <stdio.h>
#include <signal.h>

/*
 * sgi_sigffset (added by Silicon Graphics, Inc, not a part of
 * the POSIX standard), returns the signal number of the first (i.e. 
 * lowest numbered) signal which is set in the sigset_t struct pointed 
 * to by *set*. If *clear* is TRUE (1) it clears that signal in the set
 * before returning.  If no signals are set, 0 is returned.
 */

int
sgi_sigffset(sigset_t *set, int clear)
{
	register int signo;

	for (signo = 1; signo <= NUMSIGS; signo++) {
		if (sigismember(set, signo)) {
			if (clear)
				sigdelset(set, signo);
			return(signo);
		}
	}
	return(0);

} /* end sgi_sigffset() */
