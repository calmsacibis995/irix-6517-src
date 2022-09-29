/*
 * strccpy.c
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
	#pragma weak strccpy = _strccpy
	#pragma weak strcadd = _strcadd
#endif
#include "synonyms.h"

/*
	strccpy(output, input)
	strccpy copys the input string to the output string compressing
	any C-like escape sequences to the real character.
	Esacpe sequences recognized are those defined in "The C Programming
	Language" pages 180-181.
	strccpy returns the output argument.

	strcadd(output, input)
	Identical to strccpy() except returns address of null-byte at end
	of output.  Useful for concatenating strings.
*/

char *strcadd(char *, const char *);

char *
strccpy(char *pout, const char *pin)
{
	(void)strcadd( pout, pin );
	return  (pout);
}


char *
strcadd(register char *pout, register const char *pin)
{
	register char c;
	int count;
	int wd;

	while (c = *pin++) {
		if (c == '\\')
			switch (c = *pin++) {
			case 'n':
				*pout++ = '\n';
				continue;
			case 't':
				*pout++ = '\t';
				continue;
			case 'b':
				*pout++ = '\b';
				continue;
			case 'r':
				*pout++ = '\r';
				continue;
			case 'f':
				*pout++ = '\f';
				continue;
			case 'v':
				*pout++ = '\v';
				continue;
			case 'a':
				*pout++ = '\007';
				continue;
			case '\\':
				*pout++ = '\\';
				continue;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				wd = c - '0';
				count = 0;
				while ((c = *pin++) >= '0' && c <= '7') {
					wd <<= 3;
					wd |= (c - '0');
					if (++count > 1) {   /* 3 digits max */
						pin++;
						break;
					}
				}
				*pout++ = (char) wd;
				--pin;
				continue;
			default:
				*pout++ = c;
				continue;
		}
		*pout++ = c;
	}
	*pout = '\0';
	return (pout);
}
