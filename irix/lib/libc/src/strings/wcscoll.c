#ident "$Revision: 1.3 $"
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
#ifdef __STDC__
	#pragma weak wcscoll = _wcscoll
#endif

#include "synonyms.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <wchar.h>
#include <errno.h>

/* ARGSUSED */
int 
wcscoll(const wchar_t *ws1, const wchar_t *ws2)
{
        VOID    *local_buf_1,
                *local_buf_2;
        char    *s1, *s2;
        size_t  i,j;
	int 	result = 0;

        i = ((wcslen(ws2)*MB_CUR_MAX)*sizeof(wchar_t)) +1;
        if ((local_buf_2 = malloc((size_t)i)) == NULL) goto err2;

        j = ((wcslen(ws1)*MB_CUR_MAX)*sizeof(wchar_t)) +1;
        if ((local_buf_1 = malloc((size_t)j)) == NULL) goto err1;

        s2 = (char *) local_buf_2;
        s1 = (char *) local_buf_1;

	if(( wcstombs(s1, ws1, j)) == -1) goto bogus;
	if(( wcstombs(s2, ws2, i)) == -1) goto bogus;

	result = (strcoll(s1,s2));
	goto happy;

err1:   free (local_buf_2);
err2:   setoserror(EINVAL);
        return(result);

bogus:	setoserror(EINVAL);
happy:	free(local_buf_2);
	free(local_buf_1);
	return (result);
}

