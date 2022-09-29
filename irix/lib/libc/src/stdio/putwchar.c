/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstdio/putwchar.c	1.1"
#ifdef __STDC__
	#pragma weak putwchar = _putwchar
#endif

/*
 * A subroutine version of the macro putchar
 */
#include <synonyms.h>
#include <stdio.h>
#include <wchar.h>
#undef putwchar

wint_t
_putwchar(wint_t c)
{
	return(putwc(c, stdout));
}
