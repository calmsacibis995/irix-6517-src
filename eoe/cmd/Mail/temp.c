/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)temp.c	5.2 (Berkeley) 6/21/85";
#endif /* not lint */

#include <limits.h>
#include "glob.h"


/*
 * Mail -- a mail program
 *
 * Give names to all the temporary files that we will need.
 */

char	tempMail[PATH_MAX];
char	tempQuit[PATH_MAX];
char	tempEdit[PATH_MAX];
char	tempSet[PATH_MAX];
char	tempResid[PATH_MAX];
char	tempMesg[PATH_MAX];
char	tempRoot[PATH_MAX];

tinit()
{
	register char *cp, *cp2;
	char str[PATH_MAX];
	register int err = 0;
	register int pid;


	if (strlen(myname) != 0) {
		uid = getuserid(myname);
		if (uid == -1) {
			printf("\"%s\" is not a user of this system\n",
			    myname);
			safe_exit(1);
		}
	}
	else {
		uid = getuid() & UIDMASK;
		if (username(uid, str) < 0) {
			copy("ubluit", myname, PATHSIZE);
			err++;
			if (rcvmode) {
				printf("Who are you!?\n");
				safe_exit(1);
			}
		}
		else
			copy(str, myname, PATHSIZE);
	}
	cp = value("HOME");
	if (cp == NOSTR) {
		cp = str;
		if (getwd(cp) == 0)
			safe_exit(1);
	}
	copy(cp, homedir, PATHSIZE);
	findmail();
	cp = copy(homedir, mbox, PATHSIZE);
	copy("/mbox", cp, PATHSIZE - strlen(mbox));
	cp = copy(homedir, mailrc, PATHSIZE);
	copy("/.mailrc", cp, PATHSIZE - strlen(mailrc));
	cp = copy(homedir, deadletter, PATHSIZE);
	copy("/dead.letter", cp, PATHSIZE - strlen(deadletter));
	if (debug) {
		printf("uid = %d\nuser = %s\nmailname = %s\n",
		    uid, myname, mailname);
		printf("deadletter = %s\nmailrc = %s\nmbox = %s\n",
		    deadletter, mailrc, mbox);
	}

	if (!nosrc)
		load(MASTER);
	load(mailrc);

	if ((cp = value("TMPDIR")) != NOSTR)
		strncpy(tempRoot, cp, PATH_MAX);
	else
		strncpy(tempRoot, TMPDIR, PATH_MAX);
	tempRoot[PATH_MAX - 1] = '\0';
	pid = getpid();
	snprintf(tempMail, PATH_MAX, "%s/Rs%05d", tempRoot, pid);
	snprintf(tempResid, PATH_MAX, "%s/Rq%05d", tempRoot, pid);
	snprintf(tempQuit, PATH_MAX, "%s/Rm%05d", tempRoot, pid);
	snprintf(tempEdit, PATH_MAX, "%s/Re%05d", tempRoot, pid);
	snprintf(tempSet, PATH_MAX, "%s/Rx%05d", tempRoot, pid);
	snprintf(tempMesg, PATH_MAX, "%s/Rx%05d", tempRoot, pid);

}
