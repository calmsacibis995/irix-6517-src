/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/t_error.c	1.2"
#ifdef __STDC__
        #pragma weak t_error	= _t_error
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
#include "sys/stropts.h"
#include "sys/tiuser.h"
#include "errno.h"
#include "_import.h"

extern int t_errno, write();
#ifdef _BUILDING_LIBXNET
extern  char *t_errlist(int);
#else
extern  char *t_errlist[];
#endif
extern int t_nerr;

#ifdef _BUILDING_LIBXNET
int
#else
void
#endif
t_error(s)
char	*s;
{
	register char *c;
	register int n;

	c = "Unknown error";
	if(t_errno <= t_nerr)
#ifdef _BUILDING_LIBXNET
		c = t_errlist(t_errno);
#else
		c = t_errlist[t_errno];
#endif
	if(s) {
		n = strlen(s);
		if(n) {
			(void) write(2, s, (unsigned)n);
			(void) write(2, ": ", 2);
		}
	}
	(void) write(2, c, (unsigned)strlen(c));
	if (t_errno == TSYSERR) {
		(void) write(2, ": ", 2);
		perror("");
	} else
		(void) write(2, "\n", 1);

#ifdef _BUILDING_LIBXNET
	return(0);
#endif
}
