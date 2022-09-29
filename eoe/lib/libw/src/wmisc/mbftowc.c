/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wmisc/mbftowc.c	1.1.1.2"
#include <limits.h>
#include <wchar.h>
#include <stdlib.h>

/* returns number of bytes read by *f */
int
mbftowc(char *s, wchar_t *wchar, int (*f)(void), int *peekc)
{
	register int i, c;
	char str[MB_LEN_MAX];

	if ((c = (*f)()) < 0)
		return 0;
	for(i=0; i<MB_CUR_MAX; i++) {
		if(i!=0 && (c = (*f)()) < 0)
			return 0;
		*s++ = str[i] = c;
		if(mbtowc(wchar, (const char *)str, i+1) > 0) 
			return i+1;
	}
	*peekc = c;
	return -1;
}
