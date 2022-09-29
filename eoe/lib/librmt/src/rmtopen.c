#ident	"$Revision: 1.6 $"

#include <errno.h>
#include <stdio.h>
#include <sys/signal.h>

#include "rmtlib.h"

static int _rmt_open(char *, int, int);

int _rmt_Ctp[MAXUNIT][2] = { -1, -1, -1, -1, -1, -1, -1, -1 };
int _rmt_Ptc[MAXUNIT][2] = { -1, -1, -1, -1, -1, -1, -1, -1 };
int server_version = -1;	/* not set yet */

/*
 *	Open a local or remote file.  Looks just like open(2) to
 *	caller.
 */
 
int rmtopen (path, oflag, mode)
char *path;
int oflag;
int mode;
{
	if (_rmt_dev (path))
	{
		return (_rmt_open (path, oflag, mode) | REM_BIAS);
	}
	else
	{
		return (open (path, oflag, mode));
	}
}

/*
 *	_rmt_open --- open a magtape device on system specified, as given user
 *
 *	file name has the form system[.user]:/dev/????
 */

#define MAXHOSTLEN	257	/* BSD allows very long host names... */

/* ARGSUSED */
static int _rmt_open (char *path, int oflag, int mode)
{
	int i, rc;
	char buffer[BUFMAGIC];
	char system[MAXHOSTLEN];
	char device[BUFMAGIC];
	char login[BUFMAGIC];
	int  failed_once = 0;
	char *sys, *dev, *user;

	sys = system;
	dev = device;
	user = login;

/*
 *	first, find an open pair of file descriptors
 */

	for (i = 0; i < MAXUNIT; i++)
		if (READ(i) == -1 && WRITE(i) == -1)
			break;

	if (i == MAXUNIT)
	{
		setoserror( EMFILE );
		return(-1);
	}

/*
 *	pull apart system and device, and optional user
 *	don't munge original string
 */
	while (*path != '@' && *path != ':') {
		*user++ = *path++;
	}
	*user = '\0';
	path++;

	if (*(path - 1) == '@')
	{
		while (*path != ':') {
			*sys++ = *path++;
		}
		*sys = '\0';
		path++;
	}
	else
	{
		for ( user = login; *sys = *user; user++, sys++ )
			;
		user = login;
	}

	while (*path) {
		*dev++ = *path++;
	}
	*dev = '\0';

/*
 *	setup the pipes for the 'rsh' command and fork
 */
again:
	if (pipe(_rmt_Ptc[i]) == -1 || pipe(_rmt_Ctp[i]) == -1)
		return(-1);

	if ((rc = fork()) == -1)
		return(-1);

	if (rc == 0)
	{
		close(0);
		dup(_rmt_Ptc[i][0]);
		close(_rmt_Ptc[i][0]); close(_rmt_Ptc[i][1]);
		close(1);
		dup(_rmt_Ctp[i][1]);
		close(_rmt_Ctp[i][0]); close(_rmt_Ctp[i][1]);
		(void) setuid (getuid ());
		(void) setgid (getgid ());
		if (user != login)
		{
			execl(RSH_PATH, "rsh", system, "-l", login,
				RMT_PATH, (char *) 0);
			execl("/usr/bin/remsh", "remsh", system, "-l", login,
				"/etc/rmt", (char *) 0);
		}
		else
		{
			execl(RSH_PATH, "rsh", system,
				RMT_PATH, (char *) 0);
			execl("/usr/bin/remsh", "remsh", system,
				"/etc/rmt", (char *) 0);
			execl("/bin/remsh", "remsh", system,
				"/etc/rmt", (char *) 0);
		}

/*
 *	bad problems if we get here
 */

		perror("can't find remote shell program");
		exit(1);
	}

	close(_rmt_Ptc[i][0]); close(_rmt_Ctp[i][1]);

/*
 *	now attempt to open the tape device
 */

	sprintf(buffer, "O%s\n%d\n", device, oflag);
	if (_rmt_command(i, buffer) == -1 || _rmt_status(i) == -1)
		return(-1);

	/*
	 * old version of /etc/rmt does not understand 'V'
	 */
	if (failed_once == 0) {
		int rv;

		sprintf(buffer, "V%d\n", LIBRMT_VERSION);
		if (_rmt_command(i, buffer) == -1 || (rv=_rmt_status(i)) == -1 )
		{
			if (failed_once++)
				return(-1);
			else {
				close(READ(i));
				close(WRITE(i));
				READ(i) = -1;
				WRITE(i) = -1;
				/* XXX need to kill child??? */
				if (kill(rc, SIGKILL)) 
					fprintf(stderr,"remote shell program that invoked /etc/rmt does not exist\n");
				goto again;
			}
		}
		if ( rv != LIBRMT_VERSION ) {
			setoserror( EPROTONOSUPPORT );
			fprintf (stderr, "Remote tape protocol version mismatch (/etc/rmt)\n");
			exit(1);
		}
		server_version = rv;
	}
		

	return(i);
}
