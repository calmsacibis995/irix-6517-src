/*
 * strecpy.c
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


#ifdef __STDC__
	#pragma weak strecpy = _strecpy
	#pragma weak streadd = _streadd
#endif
#include "synonyms.h"

/*
	strecpy(output, input, except)
	strecpy copys the input string to the output string expanding
	any non-graphic character with the C escape sequence.
	Esacpe sequences produced are those defined in "The C Programming
	Language" pages 180-181.
	Characters in the except string will not be expanded.
	Returns the first argument.

	streadd( output, input, except )
	Identical to strecpy() except returns address of null-byte at end
	of output.  Useful for concatenating strings.
*/

#include	<ctype.h>
#include	<string.h>
#include	<stdio.h>
char *streadd(char *, const char *, const char *);


char *
strecpy(char *pout, const char *pin, const char *except)
{
	(void)streadd( pout, pin, except );
	return  pout;
}


char *
streadd(register char *pout, register const char *pin, const char *except)
{
	register unsigned	c;

	while( c = *pin++ ) {
		if( !isprint(c) && (!except || !strchr(except, (int) c)) ) {
			*pout++ = '\\';
			switch( c ) {
			case '\n':
				*pout++ = 'n';
				continue;
			case '\t':
				*pout++ = 't';
				continue;
			case '\b':
				*pout++ = 'b';
				continue;
			case '\r':
				*pout++ = 'r';
				continue;
			case '\f':
				*pout++ = 'f';
				continue;
			case '\v':
				*pout++ = 'v';
				continue;
			case '\007':
				*pout++ = 'a';
				continue;
			case '\\':
				continue;
			default:
				(void)sprintf( pout, "%.3o", c );
				pout += 3;
				continue;
			}
		}
		if ( c == '\\' && (!except || !strchr(except, (int) c)) )
			*pout++ = '\\';
		*pout++ = (char) c;
	}
	*pout = '\0';
	return  (pout);
}
