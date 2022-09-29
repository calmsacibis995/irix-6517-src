/*
 * FILE: eoe/cmd/miser/cmd/miser_kill/miser_kill.c
 *
 * DESCRIPTION:
 *	miser_kill - kill a miser job.
 *
 *	The miser_kill command is used to terminate a miser job. The 
 *	command sends a SIGKILL to the job and contacts miser in order 
 *	to free the currently committed resources. The resources become 
 *	available immediately.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include <sys/syssgi.h>		/* SGI_GETGRPPID, syssgi */
#include <sys/param.h>		/* MAXPID */
#include <errno.h>		/* errno, EPERM, ESRCH */
#include <fcntl.h>		/* O_RDONLY */
#include "libmiser.h"


#define PIDDIGITS	10	/* digits in pid for string conversion */
#define PIDLISTSIZE	1000	/* hold pid list returned by syssgi call */


int
usage(const char* pname)
/*
 * Print an usage message with a brief description of each possible
 * option and quit.
 */
{
	fprintf(stderr, "\nUsage: %s [-j] jid | -h\n\n"
                "Valid Arguments:\n"
                "\t[-j] jid\tKill miser job with pid = jid.\n" 
                "\t-h\t\tPrint the command's usage message.\n\n", 
		pname);
        return 1;       /* error */

} /* usage */


int
main(int argc, char ** argv)
{
	int	c;			/* command line option character */
	int	fd;			/* file descriptor for procinfo */
	int	j_flag	= 0;		/* j_flag initialized to 0 */
	pid_t	jid	= 0;		/* job id from argment */
	uid_t	uid;			/* user id for kill permission check */
	pid_t	pidlist[PIDLISTSIZE];	/* array of pids in pgrp */
	char	*path;			/* path of the pid proc file */
	prpsinfo_t j;			/* job's proc structure */

	/* Check if miser is running. */
        if (miser_init()) {
		fprintf(stderr, "NOTE: you may use kill/killall to kill "
			"a miser job, when miser is not running.\n\n");
                return 1;       /* error */
        }

	/* Parse command line arguments and set appropriate Flags. */
	while ((c = getopt(argc, argv, ":j:h")) != -1) {

		switch(c) {

		/* Get job id to be killed. */
		case 'j':
			j_flag++;
			jid = atoi(optarg);
			break;

		/* Get job ids for the specified queue. */
		case 'h':
			return usage(argv[0]);

		/* Getopt reports required argument missing from -j option. */
		case ':':
			/* -j option requires an argument. */
			if (strcmp(argv[optind -1], "-j") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-j' option "
					"requires an argument\n", argv[0]);
				return usage(argv[0]);
			}

			break;

		/* getopt reports invalid option in command line. */
		case '?':
			fprintf(stderr, "\n%s: ERROR - invalid command "
				"line option '%s'\n", 
				argv[0], argv[optind -1]);
			return usage(argv[0]);

                } /* switch */

        } /* while */

	/* j_flag option not set */
	if (j_flag == 0) {
		/* An argument is passed in without the '-j' option */
		if (argc == 2) {
			jid = atoi(argv[1]);
		} else {
			return usage(argv[0]);
		}
	}

	/* Make sure entered jid is valid. */ 
	if (jid <= 0 || jid > MAXPID) {
		fprintf(stderr, "\n%s: ERROR - invalid jid.\n\n", argv[0]);
		return 1;
	}

	/* 
	 * Get the first proc in the proc group. The proc group leader (jid) 
	 * may not exist, but there are other members in the group.
	 */
	if (syssgi(SGI_GETGRPPID, jid, pidlist, PIDLISTSIZE) <= 0) {
		fprintf(stderr, "\n%s: ERROR - syssgi error (%s).\n\n", 
			argv[0], strerror(errno));
		return 1;	/* error */
	}

	/* Open one of the proc in the group to make sure it is a batch job. */
	path = malloc(strlen("/proc/pinfo/") + 1 + PIDDIGITS);

       	memset(path, 0, strlen("/proc/pinfo/") + 1 + PIDDIGITS);
       	sprintf(path, "/proc/pinfo/%010d", pidlist[0]);

	fd = open(path, O_RDONLY);

	if ((fd == -1) || (ioctl(fd, PIOCPSINFO, &j) == -1)) {
		fprintf(stderr, "\n%s: ERROR - process structure jid is "
			"missing.\n\n", argv[0]);
		return 1;	/* error */
	}

	/* Check and make sure only miser jobs are being killed. */
	if ((strcmp(j.pr_clname, "B")) && (strcmp(j.pr_clname, "BC"))) {
		fprintf(stderr, "\n%s: ERROR - attempting to kill a non miser "
			"job.\n\n", argv[0]);
		return 1;	/* error */
	}

	uid = getuid();
	setuid(uid);

	/* Send a SIGKILL signal to the jid */
	if (kill (-jid, SIGKILL) != 0) {

		if (errno == EPERM) {
			fprintf(stderr, "\n%s: ERROR - do not have permission "
				"to kill this miser job.\n\n", argv[0]);

		} else if (errno == ESRCH) {
			fprintf(stderr, "\n%s: ERROR - miser job not found."
				"\n\n", argv[0]);
		} 

		return 1;	/* error */
	}

	fprintf(stdout, "\n%s: miser job [%d] - killed.\n\n", argv[0], jid);

	return 0;	/* success */

} /* main */
