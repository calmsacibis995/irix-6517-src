/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"$Revision: 1.5 $"

#include <stdio.h>
#include <ctype.h>
#include "libadm.h"

int
getinput(char *s)
{
	char input[128];
	char *copy, *pt;

	if(!gets(input))
		return(1);

	copy = s;
	pt = input;

	while(isspace(*pt))
		++pt;

	while(*pt)
		*copy++ = *pt++;
	*copy = '\0';

	if(copy != s) {
		copy--;
		while(isspace(*copy))
			*copy-- = '\0';
	}
	return(0);
}
