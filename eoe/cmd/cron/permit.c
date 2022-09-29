/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)cron:permit.c	1.3" */
#ident	"$Revision: 1.6 $"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <paths.h>
#include <string.h>
#include "cron.h"
#include "permit.h"

int per_errno;	/* status info from getuser */

char *
getuser(uid_t uid)
{
	struct passwd *pw;

	if ((pw = getpwuid(uid)) == NULL) {
		per_errno = 1;
		return(NULL);
	}
	if (strcmp(pw->pw_shell, SHELL) != 0 && *(pw->pw_shell) != '\0') {
		per_errno = 2;
#if 0
		/*
		 * Return NULL if you want crontab and at to abort
                 * when the users login shell is not /bin/sh
		 */
		return(NULL);
#endif
	}
	return(pw->pw_name);
}

static int
within(const char *username, const char *filename)
{
	char buf[UNAMESIZE], *name, *trailers;
	FILE *fp;

	if ((fp = fopen(filename, "r")) == NULL)
		return(0);
	while (fgets(name = buf, sizeof(buf), fp) != NULL) {
		/* ignore leading whitespace */
		while (isspace(*name))
			name++;

		/* ignore empty lines */
		if (*name == '\0')
			continue;

		/* remove trailing whitespace */
		trailers = name + strlen(name) - 1;
		while (trailers != name && isspace(*trailers))
			*trailers-- = '\0';

		/* match against specified username */
		if (strcmp(name, username) == 0) {
			fclose(fp);
			return(1);
		}
	}
	fclose(fp);
	return(0);
}

int
allowed(const char *user, const char *allow, const char *deny)
{
	struct stat st;

	if (stat(allow, &st) == 0)
		return(within(user, allow));
	if (stat(deny, &st) == 0)
		return(!within(user, deny));
	return(strcmp(user, "root") == 0);
}
