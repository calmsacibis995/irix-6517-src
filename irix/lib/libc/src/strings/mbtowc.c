/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#if 0 && ! _PIC

#ident	"@(#)libc-port:stdio/mbtowc.c	1.10"
/*LINTLIBRARY*/

#include "synonyms.h"
#include <ctype.h>
#include <wchar.h>
#include "_wchar.h"
#include <stdlib.h>

int
mbtowc(wchar_t *wchar, const char *s, size_t n)
{
	register int length;
	register wchar_t intcode;
	register int c;
	int retval;
	wchar_t mask;
	
	if (s == 0)
		return 0;	/* stateless encoding */
	if (n == 0)
		return -1;	/* valid character requires at least 1 byte */
	c = (unsigned char)*s;
	if (!multibyte || c < 0200)
	{
invalid_prefix:
		if (wchar != 0)
			*wchar = c;
		return c != '\0';
	}
	if (c == SS2)
	{
		if ((length = eucw2) == 0)
			goto invalid_prefix;
		mask = P01;
		intcode = 0;
	}
	else if (c == SS3)
	{
		if ((length = eucw3) == 0)
			goto invalid_prefix;
		mask = P10;
		intcode = 0;
	}
	else if (c < 0240)	/* C1 (or metacontrol) byte */
		goto invalid_prefix;
	else
	{
		if ((length = (int)eucw1 - 1) < 0)
			return -1;
		mask = P11;
		intcode = c & 0177;
	}
	if ((size_t)(retval = length + 1) > n)	/* need "length" more bytes */
		return -1;
	while (--length >= 0)
	{
		if ((c = (unsigned char)*++s) < 0200)
			return -1;
		intcode <<= 7;
		intcode |= c & 0177;
	}
	if (wchar != 0)
		*wchar = intcode | mask;
	return retval;
}	

#undef mblen

int
mblen(s, n)
const char *s;
size_t n;
{
	return(mbtowc((wchar_t *)0, s, n));
}

#endif /* ! _PIC */
