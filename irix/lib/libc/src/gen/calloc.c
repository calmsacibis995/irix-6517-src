/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/calloc.c	1.14"
/*LINTLIBRARY*/
/*	calloc - allocate and clear memory block
*/
#ifdef __STDC__
	#pragma weak calloc = _calloc
#endif
#include "synonyms.h"

#define NULL 0
#include "shlib.h"
#include <stdlib.h>
#include <memory.h>

VOID * 
_calloc(num, size)
size_t num, size;
{
	register char *mp;
	unsigned long total;

	if (num == 0 || size == 0 ) 
		total = 0;
	else {
		total = (unsigned long) num * size;

		/* check for overflow */
		if (total / num != size)
		    return(NULL);
	}
	if((mp = malloc(total)) != NULL)
		memset(mp, '\0', total);
	return(mp);
}
