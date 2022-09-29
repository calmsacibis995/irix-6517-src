/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wstostr.c	1.1.1.3"

#include "synonyms.h"
#include <widec.h>
#include <stdlib.h>
#include <limits.h>

#pragma weak wstostr = _wstostr

char *
wstostr(register char *s1, wchar_t *s2)
{
	if(wcstombs(s1, (const wchar_t *)s2, SSIZE_MAX) != (size_t)-1)
	 	return s1;
	else 
		return 0;
}
