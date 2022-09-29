/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wsntostr.c	1.1.1.3"

#include <limits.h>
#include <stdlib.h>

char *
_wsntostr(register char *s1, wchar_t *s2, int n, wchar_t **p)
{
	register wchar_t wchar;
	register int i, length;
	char str[MB_LEN_MAX + 1]; 
	char *os1 = s1;

	if (n > 0 && (wchar = *s2) != 0)
	{
		do
		{
			length = wctomb(str, wchar);
			if( length == -1 ) 
			{
				if (p != 0)
					*p = s2;
				*s1 = '\0';
				return 0;
			}
			if ((n -= length) < 0)  /* not enough room */
                        {
                                *s1 = '\0';
                                break;
                        }
			for (i=0; i<length; i++)
				*s1++ = str[i];
		} while ((wchar = *++s2) != 0 && n > 0);
	}
	if (p != 0)
		*p = s2;
	return os1;
}
