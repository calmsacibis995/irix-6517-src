/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/

#ident	"@(#)acct:common/cmd/acct/lib/uidtonam.c	1.6.1.3"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/lib/RCS/uidtonam.c,v 1.4 1993/11/16 20:49:25 jwag Exp $"
/*
 * convert uid to login name; interface to getpwuid that keeps up to USIZE1
 * names to avoid unnecessary accesses to passwd file
 * returns ptr to NSZ-byte name (not necessarily null-terminated)
 * returns ptr to "?" if cannot convert
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "acctdef.h"
#include <pwd.h>

static	usize1;
static struct ulist {
	char	uname[NSZ];
	uid_t	uuid;
} ul[USIZE1];

char *strncpy();

char *
uidtonam(uid)
uid_t	uid;
{
	register struct ulist *up;
	struct passwd *getpwuid();
	register struct passwd *pp;
	extern int _getpwent_no_shadow;

	for (up = ul; up < &ul[usize1]; up++)
		if (uid == up->uuid)
			return(up->uname);
	_getpwent_no_shadow = 1;	/* XXX no one in accting wants paswds */
	setpwent();
	if ((pp = getpwuid(uid)) == NULL)
		return("?");
	else {
		if (usize1 < USIZE1) {
			up->uuid = uid;
			CPYN(up->uname, pp->pw_name);
			usize1++;
		}
		return(pp->pw_name);
	}
}
