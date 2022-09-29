/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
/* 	Portions Copyright(c) 1988, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

#ident	"@(#)ucblibc:port/gen/ualarm.c	1.1"

#ifdef __STDC__
	#pragma weak ualarm = _ualarm
#endif
#include "synonyms.h"

#include <unistd.h>
#include <sys/time.h>

#define	USPS	1000000		/* # of microseconds in a second */

/*
 * Generate a SIGALRM signal in ``usecs'' microseconds.
 * If ``reload'' is non-zero, keep generating SIGALRM
 * every ``reload'' microseconds after the first signal.
 */
useconds_t
ualarm(useconds_t usecs, useconds_t reload)
{
	struct itimerval new, old;

	new.it_interval.tv_usec = (long) reload % USPS;
	new.it_interval.tv_sec = (time_t) reload / USPS;
	
	new.it_value.tv_usec = (long) usecs % USPS;
	new.it_value.tv_sec = (time_t) usecs / USPS;

	if (setitimer(ITIMER_REAL, &new, &old) == 0)
		return ((useconds_t)(old.it_value.tv_sec * USPS + old.it_value.tv_usec));
	return ((useconds_t) -1);
}
