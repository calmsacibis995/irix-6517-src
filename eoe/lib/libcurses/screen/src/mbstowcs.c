/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)curses:screen/mbstowcs.c	1.2"
/*LINTLIBRARY*/

#include <widec.h>
#include "synonyms.h"
#include "curses_inc.h"

size_t
_curs_mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
	size_t	i, val;

	for (i = 0; i < n; i++) {
		if ((val = (size_t) _curs_mbtowc(pwcs++, s, (size_t) MB_CUR_MAX)) == (size_t) -1)
			return(val);
		if (val == 0)
			break;
		s += val;
	}
	return(i);
}
