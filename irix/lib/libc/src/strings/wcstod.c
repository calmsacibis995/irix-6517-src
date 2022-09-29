#ident "$Revision: 1.1 $"
/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

/*	"@(#)libc-port:str/wcstod.c	1.2"				*/
/*LINTLIBRARY*/

#include "synonyms.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "mkflt.h"

extern	char *wstostr(char *, wchar_t *);

#ifdef __STDC__
	#pragma weak wcstod = _wcstod
#endif

double
#ifdef __STDC__
wcstod(const wchar_t *wcs, wchar_t **eptr)
#else
wcstod(wcs, eptr)const wchar_t *wcs; wchar_t **eptr;
#endif
{
	char	*_strb;
	size_t	_n;
	double	_x;
	MkFlt mf;

	mf.wcs = wcs;
	_mf_wcs(&mf);
	if (eptr != 0)
		*eptr = (wchar_t *)mf.wcs;

	_n = (size_t)strlen((char *)wcs);
	if (!_n) {
		_n = 100;
	}
	_strb = (char *)calloc((size_t)1, (_n + 4));
	if (!_strb) {
		return (0);
	}
	if (wstostr(_strb, (wchar_t *)wcs) != _strb) {
		(void)free(_strb);
		return (0);
	}
	_x = (double)strtod((char *)_strb, (char **)0);

	(void)free(_strb);
	return (_x);
}
