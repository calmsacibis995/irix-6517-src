#ident "$Revision: 1.4 $"
/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

/*	"@(#)libc-port:str/wcstok.c	1.1"				*/
#ifdef __STDC__
	#pragma weak wcstok = _wcstok
	#pragma weak wcstok_r = _wcstok_r
#endif

/*
 * uses wcspbrk and wcsspn to break string into tokens on
 * sequentially subsequent calls. returns NULL when no
 * non-separator characters remain.
 * 'subsequent' calls are calls with first argument NULL.
 *
 * Note: The version below is the ISO MSE version with three
 * arguments. The XPG4 version with two arguments is handled
 * in the header file and the #pragma below.
 */

#include "synonyms.h"
#include <wchar.h>

wchar_t *
wcstok_r(wchar_t *string, const wchar_t *sepset, wchar_t **savept)
{
	register wchar_t *p, *q, *r;

	/* first or subsequent call */
	p = ((string == 0) ? *savept: string);

	/* return if no tokens remaining */
	if (p == 0)
		return(0);

	/* skip leading separators */
	q = p + wcsspn(p, sepset);

	/* return if no tokens remaining */
	if (*q == 0)
		return(0);

	/* move past token */
	if ((r = wcspbrk(q, sepset)) == 0)
		*savept = 0; /* indicate this is last token */
	else {
		*r = 0;
		*savept = ++r;
	}

	return(q);
}

/* move out of function scope so we get a global symbol for use with data cording */
static wchar_t *__p;

wchar_t *
wcstok(wchar_t *string, const wchar_t *sepset)
{
        return wcstok_r(string, sepset, &__p);
}

