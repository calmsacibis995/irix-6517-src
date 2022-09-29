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

#ident "$Revision: 1.7 $"
#ifdef __STDC__
	#pragma weak sgi_altersigs = _sgi_altersigs
#endif
#include "synonyms.h"

#include <stdio.h>
#include <errno.h>
#include <signal.h>

/*
 * sgi_altersigs (added by Silicon Graphics, Inc, not a part
 * of the POSIX standard) adds or deletes (depending on the value
 * of *Action*) the set of signals in array *sigarray* to those already
 * in the sigset_t struct pointed to by *set*.  The array must be 
 * terminated by an empty (0) entry.  Returns the number of signals 
 * that are successfully altered.  If the *action* parameter isn't 
 * ADDSIGS or DELSIGS, returns -1 and errno is set to EINVAL.
 */
int
sgi_altersigs(int action, sigset_t *set, int sigarray[])
{
	register int index = 0;
	register int count = 0;
	register int signo;

	if ((action != ADDSIGS) && (action != DELSIGS))
	{
	  setoserror(EINVAL);
	  return(-1);
	}
	while ((sigarray[index] != 0) && (index < NUMSIGS))
	{
		signo = (sigarray[index++]);
		if ((signo <= 0) || (signo > NUMSIGS))	/* sigs 1-->NUMSIGS */
			continue;	/* invalid signal num, skip this one */
		if (action == ADDSIGS)
			sigaddset(set, signo);
		else
			sigdelset(set, signo);
		count++;
	}
	return(count);

} /* end sgi_altersigs */
