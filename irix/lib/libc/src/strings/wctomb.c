/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#if 0 && ! _PIC

#ident	"@(#)libc-port:stdio/wctomb.c	1.11"
/*LINTLIBRARY*/

#include "synonyms.h"
#include <ctype.h>
#include <stdlib.h>
#include "_wchar.h"

int
wctomb(s, wchar)
	register char *s;
	register wchar_t wchar;
{
	register int n, length, cs1;

	if (s == 0)
		return 0;	/* stateless encoding */
	switch (wchar & EUCMASK)
	{
	default:
		if (wchar >= 0400 || multibyte && wchar >= 0240)
			return -1;
		*s = (char)wchar;
		return 1;
	case P11:
		if ((n = eucw1) == 0)
			return -1;
		length = n;
		cs1 = 1;
		break;
	case P01:
		if ((n = eucw2) == 0)
			return -1;
		*s = SS2;
		length = n + 1;
		cs1 = 0;
		break;
	case P10:
		if ((n = eucw3) == 0)
			return -1;
		*s = SS3;
		length = n + 1;
		cs1 = 0;
		break;
	}
	s += length;	/* fill in array backwards */
	do
	{
		*--s = (char)((wchar | 0200) & 0377);	/* hope "& 0377" is tossed */
		wchar >>= 7;
	} while (--n != 0);
	if (cs1 && (unsigned char)*s < 0240)	/* C1 byte cannot be first */
		return -1;
	return length;
}
                
#endif /* ! _PIC */
