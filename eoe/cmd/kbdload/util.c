/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)kbdload:util.c	1.1"			*/

#ident  "$Revision: 1.1 $"


#include <stdio.h>
#include <sys/types.h>
#include <sys/kbdm.h>

/*
 * Copy an ALIGNER to destination.
 */

cpalign(where, con)
	ALIGNER *where;
	ALIGNER con;
{
	*where = con;
}

/*
 * Copy a bunch of chars that are really a structure.  The target
 * is ALIGN aligned.
 */

cpchar(where, what, size)
	char *where, *what;
	int size;
{
	while (size--)
		*where++ = *what++;
}
