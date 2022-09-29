/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.3 $"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "signal.h"
#include "termios.h"
#include "stdarg.h"
#include "getopt.h"
#include "errno.h"
#include "stress.h"

char *Cmd;

int
main(int argc, char **argv)
{
	pid_t pgrp, dpid, sspid, ppid;
	int c;
	int slptime = 0;

	Cmd = errinit(argv[0]);

	while((c = getopt(argc, argv, "t:")) != EOF)
	switch (c) {
	case 't':
		slptime = atoi(optarg);
		break;
	default:
		fprintf(stderr, "%s: Usage [-t ticks]\n", Cmd);
		exit(-1);
	}

	/* make us a process group */
	if (setpgid(0, 0) != 0) {
		errprintf(1, "setpgid failed");
		/* NOTREACHED */
	}
	pgrp = getpgrp();
	ppid = getpid();
	printf("%s: pgrp %d\n", Cmd, pgrp);
		
	/* make us foreground process group */
	if (tcsetpgrp(1, pgrp) != 0) {
		errprintf(1, "tcsetpgrp failed");
		/* NOTREACHED */
	}
	if ((dpid = fork()) < 0) {
		errprintf(1, "fork of drain child failed");
		/* NOTREACHED */
	} else if (dpid == 0) {
		/* child */
		for (;;) {
			if (tcdrain(1)) {
				errprintf(3, "tcdrain failed");
				if (sspid)
					kill(sspid, SIGKILL);
				if (ppid)
					kill(ppid, SIGKILL);
				abort();
				/* NOTREACHED */
			}
		}
	}
	if ((sspid = fork()) < 0) {
		errprintf(3, "fork of start-stop child failed");
		if (dpid)
			kill(dpid, SIGKILL);
		abort();
		/* NOTREACHED */
	} else if (sspid == 0) {
		/* start and stop others */
		/* move to another pgrp */
		if (setpgid(0, 0) != 0) {
			errprintf(3, "setpgid of ss child failed");
			goto doerr;
		}

		/* now send STOP and CONT to others */
		for (;;) {
			if (kill(-pgrp, SIGSTOP) != 0) {
				if (oserror() != ESRCH)
					errprintf(3, "kill STOP failed");
				goto doerr;
			}
			if (kill(-pgrp, SIGCONT) != 0) {
				if (oserror() != ESRCH)
					errprintf(3, "kill CONT failed");
				goto doerr;
			}
			if (slptime)
				sginap(slptime);
		}
doerr:
		if (dpid)
			kill(dpid, SIGKILL);
		if (ppid)
			kill(ppid, SIGKILL);
		if (oserror() != ESRCH)
			abort();
		else
			exit(0);
		/* NOTREACHED */
	}

	/* last but not least - have parent output some stuff */
	for (;;) {
		if (printf("Mary Had A Little Lamb\n") == EOF) {
			errprintf(2, "printf failed");
			if (dpid)
				kill(dpid, SIGKILL);
			if (sspid)
				kill(sspid, SIGKILL);
			exit(0);
		}
	}
	/* NOTREACHED */
}
