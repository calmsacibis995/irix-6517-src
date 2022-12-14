/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)v7.local.c	5.2 (Berkeley) 6/21/85";
#endif /* lint */

/*
 * Mail -- a mail program
 *
 * Version 7
 *
 * Local routines that are installation dependent.
 */

#include "glob.h"

/*
 * Locate the user's mailbox file (ie, the place where new, unread
 * mail is queued).  In Version 7, it is in /var/spool/mail/name.
 */

findmail()
{
	register char *cp;

	cp = copy("/var/spool/mail/", mailname, PATHSIZE);
	copy(myname, cp, PATHSIZE - strlen(mailname));
	if (isdir(mailname)) {
		if(strlen(mailname) < (PATHSIZE - 2))
			strcat(mailname, "/");
		strncat(mailname, myname, PATHSIZE - strlen(mailname));
		mailname[PATHSIZE - 1] = '\0';
	}
}

/*
 * Get rid of the queued mail.
 */

demail()
{

	if (value("keep") != NOSTR)
		close(creat(mailname, 0666));
	else {
		if (m_remove(mailname) < 0)
			close(creat(mailname, 0666));
	}
}

/*
 * Discover user login name.
 */

username(uid, namebuf)
	char namebuf[];
{
	register char *np;

	if (uid == getuid() && (np = getenv("USER")) != NOSTR) {
		strncpy(namebuf, np, PATHSIZE);
		namebuf[PATHSIZE - 1] = '\0';
		return(0);
	}
	return(getname(uid, namebuf));
}
