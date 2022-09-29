/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:setbrk.c	1.8.4.1"
/*
 *	UNIX shell
 */

#include	"defs.h"

#ifdef __STDC__
extern void *sbrk();
#else
extern char 	*sbrk();
#endif

unsigned char*
setbrk(incr)
{

	register unsigned char *a = (unsigned char *)sbrk(incr);

	brkend = a + incr;
	return(a);
}
