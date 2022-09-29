/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

# ident	"@(#)saf:log.c	1.5.6.1"

# include <stdio.h>
# include <unistd.h>
# include <sys/types.h>
# include <sac.h>
#ifndef sgi
# include <priv.h>
#endif
# include "extern.h"
# include "misc.h"
# include "msgs.h"
# include "structs.h"

static	FILE	*Lfp;	/* log file */
# ifdef DEBUG
static	FILE	*Dfp;	/* debug file */
# endif


/*
 * Procedure: openlog - open log file, sets global file pointer Lfp
 *
 *
 * Restrictions:
                 fprintf: none
                 lockf: none
 *               fopen: none
 */


void
openlog()
{
	FILE *fp;		/* scratch file pointer for problems */

	Lfp = fopen(LOGFILE, "a+");
	if (Lfp == NULL) {
		fp = fopen("/dev/console", "w");
		if (fp) {
			(void) fprintf(fp, "SAC: could not open logfile\n");
		}
		exit(1);
	}

/*
 * lock logfile to indicate presence
 */

	if (lockf(fileno(Lfp), F_LOCK, 0) < 0) {
		fp = fopen("/dev/console", "w");
		if (fp) {
			(void) fprintf(fp, "SAC: could not lock logfile\n");
		}
		exit(1);
	}
}


/*
 * Procedure: log - put a message into the log file
 *
 * Args:	msg - message to be logged
 *
 * Restrictions:
                 ctime: none
                 sprintf: none
                 fprintf: none
                 fflush: none
 */


void
log(msg)
char *msg;
{
	char *timestamp;	/* current time in readable form */
	long clock;		/* current time in seconds */
	char buf[SIZE];		/* scratch buffer */

	(void) time(&clock);
	timestamp = ctime(&clock);
	*(strchr(timestamp, '\n')) = '\0';
	(void) sprintf(buf, "%s; %ld; %s\n", timestamp, getpid(), msg);
	(void) fprintf(Lfp, buf);
	(void) fflush(Lfp);
}


/*
 * Procedure: error - put an error message into the log file and exit if indicated
 *
 * Args:	msgid - id of message to be output
 *		action - action to be taken (EXIT or not)
 */


void
error(msgid, action)
int msgid;
int action;
{
	if (msgid < 0 || msgid > N_msgs)
		return;
	log(Msgs[msgid].e_str);
	if (action == EXIT) {
		log("*** SAC exiting ***");
		exit(Msgs[msgid].e_exitcode);
	}
}


# ifdef DEBUG

/*
 * Procedure: opendebug - open debugging file, sets global file pointer Dfp
 */


void
opendebug()
{
	FILE *fp;	/* scratch file pointer for problems */

	Dfp = fopen(DBGFILE, "a+");
	if (Dfp == NULL) {
		fp = fopen("/dev/console", "w");
		if (fp) {
			(void) fprintf(fp, "SAC: could not open debugfile\n");
		}
		exit(1);
	}
}


/*
 * Procedure: debug - put a message into debug file
 *
 * Args:	msg - message to be output
 */


void
debug(msg)
char *msg;
{
	char *timestamp;	/* current time in readable form */
	long clock;		/* current time in seconds */
	char buf[SIZE];		/* scratch buffer */

	(void) time(&clock);
	timestamp = ctime(&clock);
	*(strchr(timestamp, '\n')) = '\0';
	(void) sprintf(buf, "%s; %ld; %s\n", timestamp, getpid(), msg);
	(void) fprintf(Dfp, buf);
	(void) fflush(Dfp);
}

# endif
