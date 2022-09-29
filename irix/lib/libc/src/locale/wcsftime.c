

#ident "$Revision: 1.5 $"
/*
*
* Copyright 1995, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

#include "synonyms.h"
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#pragma weak wcsftime = _wcsftime

size_t 
wcsftime(wchar_t *wcs, size_t maxsize, const char *format, const struct tm *timptr)
{
        char  *p;
        wchar_t *wp;
	ssize_t s_maxsize = (ssize_t)  maxsize ;
        size_t n = (maxsize * MB_CUR_MAX) +1; 
	ssize_t len_test;

	if ((p = malloc(n)) == NULL) return (0);
        p[0] = '\0';

	if ((wp = malloc(n * sizeof(wchar_t))) == NULL) {
		n = 0;
		goto no_wp;
	}
        wp[0] = '\0';
	wcscpy (wcs, wp); 	/* initialize wcs to null wchar_t string */

        if ((n = strftime(p, maxsize, format, timptr)) == 0) {
                if (((len_test = (ssize_t) strlen(p)) == 0) ||
			((len_test+1) >= s_maxsize) ) goto bye;
        }
        /*
         * Convert byte string to wide characters.  
         */
        n = mbstowcs(wp, p, maxsize);
	len_test = (ssize_t) n; 
	if (len_test <= s_maxsize)  wcsncat(wcs, wp,n);
	else n = 0;
	
bye:	free (wp);
no_wp:	free (p);
	return (n);
}
