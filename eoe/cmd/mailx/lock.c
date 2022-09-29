/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include "glob.h"

#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define GOT_LOCK	0
#define TRY_AGAIN	1
#define SYS_ERROR	2
#define LOCK_EXISTS	3

#define PHONY_PID	"0"
#define WATCHDOG	30

#define RO_LOCK_QUERY "\nThe mailfile is read-only locked because it is either in use by another\nprocess or because another process died without unlocking it.\n\nPlease select one of:\n\n\tR|r: Honor the lock and open the mailfile read-only.\n\tW|w: Forcibly acquire the lock and open the mailfile read-write.\n\nEnter selection [R]: "

static jmp_buf  CtxLockTimeout;
static void (*savesig)();

void
locktimeout()
{
	sigset(SIGALRM, savesig);
        longjmp(CtxLockTimeout, 1);
}

void
setalarm()
{
	savesig = sigset(SIGALRM, locktimeout);
	alarm(WATCHDOG);
}

void
clearalarm()
{
	sigset(SIGALRM, savesig);
	alarm(0);
}

int
makelock(char *name, char *suffix, char *lockname, int *lockfd, int timeout)
{
	char tmpname[PATHSIZE];
	int fd;
	struct stat sbuf;
	time_t lcktime, tmptime;

	/* Manufacture lock name and temp name */
	strcpy(lockname, name);
	strcat(lockname, suffix);
	strcpy(tmpname, lockname);
	strcat(tmpname, ".temp");

	if (debug)
		printf("makelock: attempting to lock %s...\n", name);

	if (setjmp(CtxLockTimeout) != 0) {
		printf("Timeout waiting to create %s\n", lockname);
		clearalarm();
		return SYS_ERROR;
	}

	setalarm();

	/* Create temp file */
	if ((fd = open(tmpname, O_CREAT|O_TRUNC|O_RDWR, 0660)) < 0) {
		clearalarm();
		if (debug)
			printf("makelock: error opening %s (%s)\n",
			       tmpname, strerror(errno));
		return SYS_ERROR;
	}

	/* Write an always-good pid into it for backwards compatibility */
	if (write(fd, PHONY_PID, strlen(PHONY_PID)) != strlen(PHONY_PID)) {
		if (debug)
			printf("makelock: error writing to %s (%s)\n",
			       tmpname, strerror(errno));
		unlink(tmpname);
		close(fd);
		clearalarm();
		return SYS_ERROR;
	}

	/* Link to lock name */
	if (link(tmpname, lockname) == 0) {
		/* Lock is ours! */
		if (debug)
			printf("makelock: got lock %s\n", lockname);
		unlink(tmpname);
		*lockfd = fd;
		clearalarm();
		return GOT_LOCK;
	}

	if (errno != EEXIST) {
		if (debug)
			printf("makelock: error linking %s to %s (%s)\n",
			       tmpname, lockname, strerror(errno));
		if (errno == ENOENT) {
			/*
			 * We must have raced with another process
			 * creating the temp file.  Try again
			 * immediately.
			 */
			close(fd);
			clearalarm();
			return TRY_AGAIN;
		}
		unlink(tmpname);
		close(fd);
		clearalarm();
		return SYS_ERROR;
	}

	/* Lock file exists. */

	if (!timeout) { /* Lock is good indefinitely */
		if (debug)
			printf("makelock: lock %s exists\n", lockname);
		unlink(tmpname);
		close(fd);
		clearalarm();
		return LOCK_EXISTS;
	}

	/* See if lock file is "too old" */
	if (stat(tmpname, &sbuf) < 0) {
		if (debug)
			printf("makelock: error stating %s (%s)\n",
			       tmpname, strerror(errno));
		if (errno == ENOENT) {
			close(fd);
			clearalarm();
			return TRY_AGAIN;
		}
		unlink(tmpname);
		close(fd);
		clearalarm();
		return SYS_ERROR;
	}
	tmptime = sbuf.st_ctime;
	unlink(tmpname);
	close(fd);
	if (stat(lockname, &sbuf) < 0) {
		clearalarm();
		if (debug)
			printf("makelock: error stating %s (%s)\n",
			       lockname, strerror(errno));
		if (errno == ENOENT)
			return TRY_AGAIN;
		return SYS_ERROR;
	}
	clearalarm();
	lcktime = sbuf.st_ctime;
	if (tmptime > lcktime && (tmptime - lcktime) > timeout) {
		if (debug)
			printf("makelock: lock %s is older than %d sec\n",
			       lockname, timeout);
		unlink(lockname);
		return TRY_AGAIN;
	}
	if (debug)
		printf("makelock: lock %s exists\n", lockname);
	return LOCK_EXISTS;
}

/*
 * Ask user if he wants to forcibly acquire  the lock.  If he does, attempt
 * to remove the lockfile and return 1.  Return 0 otherwise.
 */

int
fix_lock(char *lockname, char *querystr)
{
	char replybuf[BUFSIZ];
	char *cp;

	cp = value("noaskrolock");
	for (;;) {
		if (cp == NOSTR) {
			printf(querystr);
			replybuf[0] = '\0';
			savesig = sigset(SIGINT, SIG_DFL);
			fgets(replybuf, BUFSIZ, stdin);
			sigset(SIGINT, savesig);
			cp = replybuf;
		}
		while (*cp && isspace(*cp))
			cp++;
		switch(*cp) {
		case 'r':
		case 'R':
		case '\0':
			printf("\nOpening the mailfile read-only.\n");
			return(0);
		case 'w':
		case 'W':
			printf("\nAttempting to forcibly acquire the lock.\n");
			m_remove(lockname);
			return(1);
		default:
			printf("\n\'%c\' is not a valid selection.\n\n", *cp);
			cp = NOSTR;
			break;
		}
	}
	/* NOTREACHED */
}


char maillock[]	= ".lock";		/* Lock suffix for mailname */
static char curlock[PATHSIZE];		/* Last used name of lock */
static int curlockfd = -1;		/* Lock file descriptor */

/*
 * AT&T-style locking.  Sun-compatible algorithm.
 */

lockit(file)
char *file;
{
	int lckstat, trys;

	if (nolock) {
		if (debug)
			printf("lockit: -F flag given.  No locking.\n");
		return 0;
	}
	if (file == NOSTR) {
		if (debug)
			printf("lockit: no filename supplied.\n");
		return 0;
	}

	if (curlockfd != -1) {
		if (debug)
			printf("lockit: already holding %s\n", curlock);
		return -1;
	}

	if (debug)
		printf("lockit: attempting to lock %s\n", file);

	setgid(savedegid);
	for (trys = 0; trys < 100; trys++) {

		lckstat = makelock(file, maillock, &curlock[0],
				   &curlockfd, 300);

		switch(lckstat) {

		case GOT_LOCK:
			setgid(getgid());
			return 0;

		case LOCK_EXISTS:
			sleep(5);
			/* fall through... */

		case TRY_AGAIN:
			if (debug)
				printf("lockit: retrying lock...\n");
			continue;

		case SYS_ERROR:
		default:
			printf(
		             "Warning: unexpected error trying to lock %s.\n",
			       file);
			setgid(getgid());
			return -1;
		}
	}
	setgid(getgid());
	printf("Warning: could not lock %s after 100 trys.\n", file);
	return -1;
}

/*
 * Remove the mail lock, and note that we no longer
 * have it locked.
 */

unlock()
{
	if (nolock) {
		if (debug)
			printf("unlock: -F flag given.  No locking.\n");
		return;
	}
	if (curlockfd == -1) {
		if (debug)
			printf("unlock: no lock held?!?\n");
		return;
	}
	if (debug)
		printf("unlock: %s\n", curlock);
	setgid(savedegid);
	close(curlockfd);
	curlockfd = -1;
	m_remove(curlock);
	setgid(getgid());
}


/*
 * SGI-style read-only locking.
 */

char rolock[] = ".rolock";	/* lock suffix */
static int rolockfd = -1;	/* file descriptor for .rolock file */
static char rolockfile[PATHSIZE];

/*
 * If .rolock file exists, go into read-only mode.
 *
 * Since .rolocking is advisory in nature go into read-write mode on most
 * errors.
 */
void
rolockit(name, roflag)
	char *name;
	int *roflag;
{
	int trys, lckstat;

	if (nolock) {
		if (debug)
			printf("rolockit: -F flag given.  No locking.\n");
		return;
	}
	if (rolockfd != -1) {
		if (debug)
			printf("rolockit: already holding %s\n", rolockfile);
		*roflag = NOT_READONLY;
		return;
	}
	/*
	 * If we don't have write access to the mail file,
	 * set the read only flag and get out of here.  We
	 * wouldn't want to create a read only lock file in
	 * this case, or we might lock out someone else who
	 * HAS write access to the file.
	 */
	if (access(name, 2) < 0) {
		*roflag = NO_ACCESS;
		if (debug)
			printf("rolockit: no write access to %s (%s)\n",
			       name, strerror(errno));
		return;
	}

	setgid(savedegid);
	if (debug)
		printf("rolockit: attempting to lock %s\n", name);
	for (trys = 0; trys < 100; trys++) {

		lckstat = makelock(name, rolock, &rolockfile[0], &rolockfd, 0);

		switch(lckstat) {

		case GOT_LOCK:
			*roflag = NOT_READONLY;
			setgid(getgid());
			return;

		case LOCK_EXISTS:
			if (!trys && fix_lock(rolockfile, RO_LOCK_QUERY)) {
				continue;
			} else if (trys) {
				printf("Cannot acquire %s\n", rolockfile);
			}
			*roflag = RO_LOCKED;
			setgid(getgid());
			return;

		case TRY_AGAIN:
			if (debug)
				printf("rolockit: retrying lock...\n");
			continue;

		case SYS_ERROR:
		default:
			*roflag = NOT_READONLY;
			setgid(getgid());
			return;
		}
	}
	if (debug)
		printf("lockit: giving up after 100 trys.\n");
	*roflag = NOT_READONLY;
	setgid(getgid());
	return;
}


void
rounlock()
{
	int lpid;

	if (nolock) {
		if (debug)
			printf("rounlock: -F flag given.  No locking.\n");
		return;
	}
	if (rolockfd == -1) {
		if (debug)
			printf("rounlock: no lock held?!?\n");
		return;
	}
	if (debug)
		printf("rounlock: %s\n", rolockfile);
	setgid(savedegid);
	close(rolockfd);
	rolockfd = -1;
	m_remove(rolockfile);
	setgid(getgid());
	return;
}

