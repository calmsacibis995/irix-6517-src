/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/strtows.c	1.1.1.1"

#include <stdlib.h>
#include <limits.h>

wchar_t *
strtows(register wchar_t *s1, register char *s2)
{
	if(mbstowcs(s1, (const char *)s2, SSIZE_MAX) != (size_t)-1)
		return s1;
	else 
		return 0;
}
