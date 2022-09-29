/*
 * rtnetd.c
 *
 * 	Daemon to enable preemptable network packet processing.
 *
 *
 * Copyright 1990, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/syssgi.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>


main(int argc, char **argv)
{
	int pri = NDPHIMIN;
	char *prog;

	if ((prog = strrchr(argv[0], '/')) != NULL)
		prog++;
	else
		prog = argv[0];

	openlog(prog, LOG_PID, LOG_DAEMON);
	if (argc > 2) {
		syslog(LOG_ERR, "usage: %s [pri]\n", prog);
		exit(1);
	}
	if (argc == 2) {
		pri = atoi(argv[1]);
		if (pri < NDPHIMAX || pri > NDPHIMIN) {
			syslog(LOG_ERR, "invalid priority %d\n", pri);
			exit(1);
		}
	}
	if (schedctl(NDPRI, 0, pri) < 0)
		exit(1);

	if (fork())
		exit(0);

	closelog();
	(void) close(0);
	(void) close(1);
	(void) close(2);

	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGTSTP, SIG_IGN);

	(void) syssgi(SGI_NETPROC);

	_exit(errno);
}
