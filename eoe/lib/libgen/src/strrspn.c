/*
 * strrspn.c
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

#ident "$Revision: 1.3 $"


#ifdef __STDC__
	#pragma weak strrspn = _strrspn
#endif
#include "synonyms.h"

/*
	Trim trailing characters from a string.
	Returns pointer to the first character in the string
	to be trimmed (tc).
*/

#include	<string.h>


char *
strrspn(const char *string, const char *tc)	/* tc: characters to trim */
{
	char	*p;

	p = (char *)string + strlen( string );  
	while( p != (char *)string )
		if( !strchr( tc, *--p ) )
			return  ++p;
		
	return  p;
}
