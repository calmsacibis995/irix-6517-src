/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. 	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   	*/
/*	  All Rights Reserved                                   	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1990, 1991, 1992 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1985, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstdio/ungetwc.c	1.1.2.2"

/*
 * Ungetwc saves the process code c into the one character buffer
 * associated with an input stream "iop". That character, c,
 * will be returned by the next getwc call on that stream.
 */
#pragma weak ungetwc = _ungetwc

#include "synonyms.h"
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>

wint_t
ungetwc(wint_t c, FILE *iop)
{
	char str[MB_LEN_MAX + 1];
	int i, len;

	if (c == WEOF)
		return WEOF;

	len = wctomb(str, c);
	if (len == -1)  {
		setoserror(EILSEQ);
		return WEOF;
	} else {
		for(i=len-1; i>=0; i--) {
			if (ungetc(str[i], iop) == EOF)
				return WEOF;
		}
	}
	return c;
}
