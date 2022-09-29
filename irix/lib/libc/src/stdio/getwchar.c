/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstdio/getwchar.c	1.1"
#ifdef __STDC__
	#pragma weak getwchar = _getwchar
#endif

/*
 * A subroutine version of the macro getwchar.
 */

#include <synonyms.h>
#include <stdio.h>
#include <wchar.h>
#undef getwchar

wint_t
_getwchar(void)
{
	return(getwc(stdin));
}
