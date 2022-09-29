/*
 * copylist.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.4 $"


/*
	copylist copies a file into a block of memory, replacing newlines
	with null characters, and returns a pointer to the copy.
*/

#ifdef __STDC__
	#pragma weak copylist = _copylist
#endif
#include "synonyms.h"

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<stdio.h>
#include	<malloc.h>

static long	linecount;

char *
copylist(const char *filenm, off_t *szptr)
{
	FILE		*strm;
	struct	stat	stbuf;
	register int	c;
	register char	*ptr, *p;

	/* get size of file */
	if (stat(filenm, &stbuf) == -1) {
		return(NULL);
	}
	*szptr = stbuf.st_size;

	/* get block of memory */
	if((ptr = malloc((unsigned) *szptr)) == NULL) {
		return(NULL);
	}

	/* copy contents of file into memory block, replacing newlines
	with null characters */
	if ((strm = fopen(filenm, "r")) == NULL) {
		return(NULL);
	}
	linecount = 0;
	for (p = ptr; p < ptr + *szptr  &&  (c = getc(strm)) != EOF; p++) {
		if (c == '\n') {
			*p = '\0';
			linecount++;
		}
		else
			*p = (char) c;
	}
	(void)fclose(strm);

	return(ptr);
}
