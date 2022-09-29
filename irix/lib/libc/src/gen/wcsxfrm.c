
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
#include <errno.h>

#pragma weak wcsxfrm = _wcsxfrm

/* ARGSUSED */
size_t 
wcsxfrm(wchar_t *ws1, const wchar_t *ws2, size_t n)
{
	VOID 	*local_buf_1,
		*local_buf_2;
	size_t 	i, j;
	char	*s1, *s2;
	ssize_t j1 = MB_CUR_MAX;

	i = ((wcslen(ws2)* j1) * sizeof(char)) +1;
	j = ((n*j1) * sizeof(char)) +1;

	if ((local_buf_2 = malloc((size_t)i)) == NULL) goto err2;
	if ((local_buf_1 = malloc((size_t)j)) == NULL) goto err1;

	s2 = (char *) local_buf_2;
	s1 = (char *) local_buf_1;

	if((i = wcstombs(s2, ws2, n)) == -1 ) goto bogus;
	if (i == (size_t) 0) goto happy;

	if ((j = strxfrm (s1, s2, n)) == -1) goto bogus;
	j1 = (ssize_t) j;
	if ( j1 > n) {
		i=n+1;
		goto happy; 
	}

	if (( i = mbstowcs(ws1, s1, n)) == -1) goto bogus;
	goto happy;

err1:	free (local_buf_2);
err2:	setoserror(EINVAL);
	return(-1);

bogus:	i= -1;
happy:	free(local_buf_2);
	free(local_buf_1);
	return (i);
}
