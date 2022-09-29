/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstdio/fgetwc.c	1.1.1.1"

#ifdef __STDC__
	#pragma weak fgetwc = _fgetwc
	#pragma weak getwc = _fgetwc
#else
	#pragma weak getwc = fgetwc
#endif

#include "synonyms.h"
#include <limits.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

wint_t
fgetwc(FILE *iop)
{
	register int i, c;
	char str[MB_LEN_MAX];
	wchar_t wc;

	for(i=0; i<MB_CUR_MAX; i++) {
		if ((c = getc(iop)) == EOF)
			return EOF;
		str[i] = c;
		if(mbtowc(&wc, (const char *)str, i+1) != (size_t)-1)
			return wc;
	}
	while (i >= 0) 
		ungetc(str[i--], iop);
	setoserror(EILSEQ);
	iop->_flag |= _IOERR;
	return EOF;
}
