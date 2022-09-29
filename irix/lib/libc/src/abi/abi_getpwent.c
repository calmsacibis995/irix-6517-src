/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Id: abi_getpwent.c,v 1.2 1994/10/11 21:22:58 jwag Exp $"
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak endpwent = _endpwent
	#pragma weak fgetpwent = _fgetpwent
	#pragma weak getpwent = _getpwent
	#pragma weak setpwent = _setpwent
#endif
#include "synonyms.h"
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <errno.h>

static const char *PASSWD = "/etc/passwd";
static FILE *pwf = NULL;

static char line[BUFSIZ+1];
static struct passwd save_passwd;

struct passwd *fgetpwent(FILE *f);

void
setpwent(void)
{
	if(pwf == NULL)
		pwf = fopen(PASSWD, "r");
	else
		rewind(pwf);
}

void
endpwent(void)
{
	if(pwf != NULL) {
		(void) fclose(pwf);
		pwf = NULL;
	}
}

static char *
pwskip(register char *p)
{
	while(*p && *p != ':' && *p != '\n')
		++p;
	if(*p == '\n')
		*p = '\0';
	else if(*p)
		*p++ = '\0';
	return(p);
}

struct passwd *
getpwent(void)
{

	if(pwf == NULL) {
		if((pwf = fopen(PASSWD, "r")) == NULL)
			return(NULL);
	}
	return (fgetpwent(pwf));
}

struct passwd *
fgetpwent(FILE *f)
{
	register struct passwd *passwd;
	register char *p;
	char *end;
	long	x;

	passwd = &save_passwd;

	p = fgets(line, BUFSIZ, f);
	if(p == NULL)
		return(NULL);
	passwd->pw_name = p;
	p = pwskip(p);
	passwd->pw_passwd = p;
	p = pwskip(p);
	if (p == NULL || *p == ':') {
		/* check for non-null uid */
		errno = EINVAL;
		return (NULL);
	}
	x = strtol(p, &end, 10);	
	if (end != memchr(p, ':', strlen(p))){
		/* check for numeric value */
		errno = EINVAL;
		return (NULL);
	}
	p = pwskip(p);
	passwd->pw_uid = (x < 0 || x > MAXUID)? (UID_NOBODY): x;
	if (p == NULL || *p == ':') {
		/* check for non-null uid */
		errno = EINVAL;
		return (NULL);
	}
	x = strtol(p, &end, 10);	
	if (end != memchr(p, ':', strlen(p))) {
		/* check for numeric value */
		errno = EINVAL;
		return (NULL);
	}
	p = pwskip(p);
	passwd->pw_gid = (x < 0 || x > MAXUID)? (UID_NOBODY): x;
	passwd->pw_comment = p;
	passwd->pw_gecos = p;
	p = pwskip(p);
	passwd->pw_dir = p;
	p = pwskip(p);
	passwd->pw_shell = p;
	(void) pwskip(p);

	p = passwd->pw_passwd;
	while(*p && *p != ',')
		p++;
	if(*p)
		*p++ = '\0';
	passwd->pw_age = p;
	return(passwd);
}
