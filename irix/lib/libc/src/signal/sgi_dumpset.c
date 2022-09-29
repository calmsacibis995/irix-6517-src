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

#ident "$Revision: 1.5 $"
#ifdef __STDC__
	#pragma weak sgi_dumpset = _sgi_dumpset
#endif
#include "synonyms.h"

#include <stdio.h>
#include <signal.h>

/*
 * sgi_dumpset (added by Silicon Graphics, Inc, not a part of
 * the POSIX standard) displays as bit-fields the signals which
 * are pending in the sigset pointed to by *set*.
 */
int
sgi_dumpset(sigset_t *set)
{
	int sig;

	printf("\n");
	for (sig = NUMSIGS; sig > 0; sig--) {
		if (sigismember(set, sig))
			printf("%c",'1');
		else
			printf("%c",'0');
	}
	printf("\n");
	return(0);

} /* end sgi_dumpset() */
