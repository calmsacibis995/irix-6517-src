/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/tempnam.c	1.7.1.9"
/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak tempnam = _tempnam
#endif
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>	/* prototype for access() */

#define max(A,B) (((A)<(B))?(B):(A))

static char *pcopy(char *, const char *);

static char seed[] = "AAA";

char *
tempnam(
const char *dir,		/* use this directory please (if non-NULL) */
const char *pfx)		/* use this (if non-NULL) as filename prefix */
{
	register char *p, *q, *tdir;
	int x=0, y=0, z;
	LOCKDECL(l);

	z=sizeof(P_tmpdir) - 1;
	if((tdir = getenv("TMPDIR")) != NULL) {
		x = (int)strlen(tdir);
	}
	if(dir != NULL) {
		y= (int)strlen(dir);
	}
	if((p=malloc((unsigned)(max(max(x,y),z)+16))) == NULL)
		return(NULL);
	LOCKINIT(l, LOCKOPEN);
	if(x > 0 && access(pcopy(p, tdir), 3) == 0)
		goto OK;
	if(y > 0 && access(pcopy(p, dir), 3) == 0)
		goto OK;
	if(access(pcopy(p, P_tmpdir), 3) == 0)
		goto OK;
	if(access(pcopy(p, "/tmp"), 3) != 0) {
		UNLOCKOPEN(l);
		free(p);
		return(NULL);
	}
OK:
	(void)strcat(p, "/");
	if(pfx) {
		*(p+strlen(p)+5) = '\0';
		(void)strncat(p, pfx, 5);
	}
	(void)strcat(p, seed);
	(void)strcat(p, "XXXXXX");
	q = &seed[0];
	while(*q == 'Z')
		*q++ = 'A';
	if (*q != '\0')
		++*q;
	if(*mktemp(p) == '\0') {
		UNLOCKOPEN(l);
		free(p);
		return(NULL);
	}
	UNLOCKOPEN(l);
	return(p);
}

static char*
pcopy(char *space, const char *arg)
{
	char *p;

	if(arg) {
		(void)strcpy(space, arg);
		p = space-1+strlen(space);
		if(*p == '/')
			*p = '\0';
	}
	return(space);
}
