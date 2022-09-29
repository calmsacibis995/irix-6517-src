/*
 * bufsplit.c
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
	split buffer into fields delimited by tabs and newlines.
	Fill pointer array with pointers to fields.
	Return the number of fields assigned to the array[].
	The remainder of the array elements point to the end of the buffer.
  Note:
	The delimiters are changed to null-bytes in the buffer and array of
	pointers is only valid while the buffer is intact.
*/

#ifdef __STDC__
	#pragma weak bufsplit = _bufsplit
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <string.h>

static char	*bsplitchar = "\t\n";	/* characters that separate fields */

size_t
bufsplit(register char *buf, size_t dim, char *array[])
  /* register char	*buf;		-- input buffer */
  /* size_t		dim;		-- dimension of the array */
{
	register size_t numsplit;
	register size_t i;

	if( !buf )
		return 0;
	if ( !dim ^ !array)
		return 0;
	if( buf  &&  !dim  &&  !array ) {
		bsplitchar = buf;
		return 1;
	}
	numsplit = 0;
	while ( numsplit < dim ) {
		array[numsplit] = buf;
		numsplit++;
		buf = strpbrk(buf, bsplitchar);
		if (buf)
			*(buf++) = '\0';
		else
			break;
		if (*buf == '\0') {
			break;
		}
	}
	buf = strrchr( array[numsplit-1], '\0' );
	for (i=numsplit; i < dim; i++)
		array[i] = buf;
	return numsplit;
}
