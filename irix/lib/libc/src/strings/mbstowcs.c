/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#if 0 && ! _PIC

#ident	"@(#)libc-port:gen/mbstowcs.c	1.4"

/*LINTLIBRARY*/

#include "widec.h"
#include "synonyms.h"
#include <stdlib.h>
#include <errno.h>

size_t
mbstowcs(pwcs, s, n)
wchar_t	*pwcs;
const char *s;
size_t n;
{
        int     i =0, val = 0;

        if (pwcs == 0) {
                /*
                 *  XPG4 feature:
                 *
                 * If 'pwcs' is a NULL pointer, 'mbstowcs()' returns the length
                 * required to convert the entire array regardless of the value
                 * of 'n', but no values are stored.
                 */
                while ((val = mbtowc(pwcs, s, (size_t)MB_CUR_MAX)) != 0) {
                        if (val == -1) {
                                setoserror(EILSEQ);
                                return((size_t)val);
                        }
                        s += val;
                        i++;
                }
	} else {
		for (i = 0; (unsigned int)i < n; i++) {
			if ((val = mbtowc(pwcs++, s, (size_t)MB_CUR_MAX)) == -1)
			{
				setoserror(EILSEQ);
				return((size_t)val);
			}
			if (val == 0)
				break;
			s += val;
		}
	}
	return((size_t)i);
}

#endif /* ! _PIC */

