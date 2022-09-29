/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/mktemp.c	1.18"
/*LINTLIBRARY*/
/****************************************************************
 *	Routine expects a string of length at least 6, with
 *	six trailing 'X's.  These will be overlaid with a
 *	letter and the last (5) symbols of the proccess ID.
 *	If every letter (a thru z) thus inserted leads to
 *	an existing file name, your string is shortened to
 *	length zero upon return (first character set to '\0').
 ***************************************************************/
#define XCNT  6
#ifdef __STDC__
	#pragma weak mktemp = _mktemp
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include "shlib.h"
#include "mplib.h"

#define PERMITTED	0

char *
mktemp(char *as)
{
	register char *s=as;
	register pid_t pid;
	register unsigned mod;
	register unsigned xcnt=0; /* keeps track of number of X's seen */
	LOCKDECLINIT(l, LOCKOPEN);

	pid = getpid();
	s += strlen(as);	/* point at the terminal null */
	while(*--s == 'X' && ++xcnt <= XCNT) {
		mod = (unsigned int)(pid & 077);	/* use radix-64 arithmetic */
		if (mod > 35)
			*s = (char)(mod + '_' - 36);
		else if(mod > 9)
			*s = (char)(mod + 'A' - 10);
		else	*s = (char)(mod + '0');
		if (*s == '\140') *s = '-';
		pid >>= 6;
	}
	if(*++s) {		/* maybe there were no 'X's */
		*s = 'a';
		while(access(as, F_OK) == PERMITTED) {
			if(++*s > 'z') {
				*as = '\0';
				break;
			}
		}
	} else
		if(access(as, F_OK) == PERMITTED)
			*as = '\0';
	UNLOCKOPEN(l);
	return(as);
}
