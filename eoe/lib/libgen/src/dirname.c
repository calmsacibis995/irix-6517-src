/*
 * dirname.c
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


/*
	Return pointer to the directory name, stripping off the last
	component of the path.
	Works similar to /bin/dirname
*/

#ifdef __STDC__
	#pragma weak dirname = _dirname
#endif
#include "synonyms.h"

#include	<string.h>

char *
dirname(char *s)
{
	register char	*p;

	if( !s  ||  !*s )			/* zero or empty argument */
		return  ".";

	p = s + strlen( s );
	while( p != s  &&  *--p == '/' )	/* trim trailing /s */
		;
	
	if ( p == s && *p == '/' )
		return "/";

	while( p != s )
		if( *--p == '/' ) {
			if ( p == s )
				return "/";
			while ( *p == '/' )
				p--;
			*++p = '\0';
			return  s;
		}
	
	return  ".";
}
