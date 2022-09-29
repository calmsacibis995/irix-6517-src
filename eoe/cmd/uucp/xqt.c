/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 1.6 $"

#include "uucp.h"

static void euucico(char *);

/*
 * start up uucico for rmtname
 * return:
 *	none
 */
void
xuucico(rmtname)
char *rmtname;
{
	/*
	 * start uucico for rmtname system
	 */
	if (fork() == 0) {	/* can't vfork() */
		/*
		 * hide the uid of the initiator of this job so that he
		 * doesn't get notified about things that don't concern him.
		 */
		(void) setuid(geteuid());
		euucico(rmtname);
	}
	return;
}

static void
euucico(rmtname)
char	*rmtname;
{
	char opt[100];

	(void) close(0);
	(void) close(1);
	(void) close(2);
	(void) open("/dev/null", 0);
	(void) open("/dev/null", 1);
	(void) open("/dev/null", 1);
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
#ifdef sgi
	uucloselog();
#else
	closelog();
#endif
	if (rmtname[0] != '\0')
		(void) sprintf(opt, "-s%s", rmtname);
	else
		opt[0] = '\0';
	(void) execle(UUCICO, "UUCICO", "-r1", opt, (char *) 0, Env);
	exit(100);
}


/*
 * start up uuxqt
 * return:
 *	none
 */
void
xuuxqt(rmtname)
char	*rmtname;
{
	char	opt[100];

	if (rmtname && rmtname[0] != '\0')
		(void) sprintf(opt, "-s%s", rmtname);
	else
		opt[0] = '\0';
	/*
	 * start uuxqt
	 */
	if (vfork() == 0) {
		(void) close(0);
		(void) close(1);
		(void) close(2);
		(void) open("/dev/null", 2);
		(void) open("/dev/null", 2);
		(void) open("/dev/null", 2);
		(void) signal(SIGINT, SIG_IGN);
		(void) signal(SIGHUP, SIG_IGN);
		(void) signal(SIGQUIT, SIG_IGN);
#ifdef sgi
		uucloselog();
#else
		closelog();
#endif
		/*
		 * hide the uid of the initiator of this job so that he
		 * doesn't get notified about things that don't concern him.
		 */
		(void) setuid(geteuid());
		(void) execle(UUXQT, "UUXQT", opt, (char *) 0, Env);
		(void) _exit(100);
	}
	return;
}
