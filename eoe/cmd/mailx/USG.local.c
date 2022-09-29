/*
 * Mail -- a mail program
 *
 * System 5
 *
 * Local routines that are installation dependent.
 */

#include "glob.h"
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>


/*
 * Locate the user's mailbox file (ie, the place where new, unread
 * mail is queued).  In System V, it is in /var/mail/name.
 */

findmail()
{
	register char *cp;

	/* Let the user redefine the mailbox location. */
	if ((cp = value("MAIL")) != NOSTR && cp[0] != '\0') {
		strncpy(mailname, cp, PATHSIZE);
		mailname[PATHSIZE - 1] = '\0';
		return;
	}
	cp = copy("/var/mail/", mailname, PATHSIZE);
	copy(myname, cp, PATHSIZE - strlen(mailname));
	if (isdir(mailname)) {
		if(strlen(mailname) < PATHSIZE - 2)
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
	setgid(savedegid);
	if (value("keep") != NOSTR)
		close(creat(mailname, 0666));
	else {
		if (m_remove(mailname) < 0)
			close(creat(mailname, 0666));
	}
	setgid(getgid());
}

/*
 * Discover user login name.
 */

username(uid, namebuf)
	char namebuf[];
{
	register char *np;

	if (uid == getuid() && (np = getenv("LOGNAME")) != NOSTR) {
		strncpy(namebuf, np, PATHSIZE);
		namebuf[PATHSIZE - 1] = '\0';
		return(0);
	}
	return(getname(uid, namebuf));
}
