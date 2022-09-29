/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstdio/fputwc.c	1.1.1.3"


/*
 * Fputwc transforms the process code c into the EUC,
 * and writes it onto the output stream "iop".
 */
#ifdef __STDC__
	#pragma weak fputwc = _fputwc
	#pragma weak putwc = _fputwc
#else
	#pragma weak putwc = fputwc
#endif

#include "synonyms.h"
#include <stdlib.h>
#include <limits.h>
#include <wchar.h>
#include <errno.h>

#define putbyte(x,p)    { if (putc((x),(p)) == EOF) return(EOF); }

wint_t
fputwc(wint_t c, register FILE *iop)
{
	char str[MB_LEN_MAX + 1]; 
	int i, len;

	len = wctomb(str, c);
	if (len == -1)  {
		setoserror(EILSEQ);
		return EOF;
	} else {
		for (i=0; i<len; i++)
			putbyte((int)str[i], iop);
	}
	return c;
}
