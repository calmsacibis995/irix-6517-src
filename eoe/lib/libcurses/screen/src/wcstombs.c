/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)curses:screen/wcstombs.c	1.2"
/*LINTLIBRARY*/

#include <widec.h>
#include "synonyms.h"
#include <limits.h>
#include "curses_inc.h"

size_t
_curs_wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
	size_t	val = 0;
	size_t	total = 0;
	char	temp[MB_LEN_MAX];
	size_t	i;

	for (;;) {
		if (*pwcs == 0) {
			*s = '\0';
			break;
		}
		if ((val = (size_t) _curs_wctomb(temp, *pwcs++)) == (size_t)-1)
			return(val);
		if ((total += val) > n) {
			total -= val;
			break;
		}
		for (i=0; i<val; i++)
			*s++ = temp[i];
	}
	return(total);
}
