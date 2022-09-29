/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/

#ident	"@(#)acct:common/cmd/acct/lib/namtouid.c	1.7.3.3"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/lib/RCS/namtouid.c,v 1.4 1993/11/16 20:49:07 jwag Exp $"
/*
 *	namtouid converts login names to uids
 *	maintains ulist for speed only
 */
#include <stdio.h>
#include <sys/types.h>
#include "acctdef.h"
#include <pwd.h>
static	usize;
static	struct ulist {
	char	uname[NSZ];
	uid_t	uuid;
} ul[A_USIZE];
char	ntmp[NSZ+1];

char	*strncpy();

uid_t
namtouid(name)
char	name[NSZ];
{
	register struct ulist *up;
	register uid_t tuid;
	struct passwd *getpwnam(), *pp;
	extern int _getpwent_no_shadow;

	for (up = ul; up < &ul[usize]; up++)
		if (strncmp(name, up->uname, NSZ) == 0)
			return(up->uuid);
	strncpy(ntmp, name, NSZ);
	_getpwent_no_shadow = 1;	/* XXX no one in accting wants paswds */
	(void) setpwent();
	if ((pp = getpwnam(ntmp)) == NULL)
		tuid = -1;
	else {
		tuid = pp->pw_uid;
		if (usize < A_USIZE) {
			CPYN(up->uname, name);
			up->uuid = tuid;
			usize++;
		}
	}
	return(tuid);
}
