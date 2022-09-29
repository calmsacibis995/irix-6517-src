/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#ident "$Revision: 1.1 $"

/*---------------------------------------------------------------------------*/
/* amtickerd.c                                                               */
/* amtickerd is a light weight daemon that is run by availmon at startup     */
/* depending upon whether 'tickerd' flag is set or not.  amtickerd monitors  */
/* system uptime by constantly writing to a file the last time system is up. */
/* The frequency of writing to the file is driven by command line arguments. */
/* amtickerd also takes another command line option, statusinterval which    */
/* represents the interval at which status reports need to be sent.          */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>
#include <sys/file.h>
#include "amdefs.h"
#include "oamdefs.h"

char cmd[MAXNAMELEN];
extern int errno;
void ticker();

/*---------------------------------------------------------------------------*/
/* Name    : main                                                            */
/* Purpose : amtickerd daemon                                                */
/* Inputs  : command line arguments, all mandatory                           */
/* Outputs : None, runs as a daemon                                          */
/*---------------------------------------------------------------------------*/

main(int argc, char **argv)
{
    unsigned int sleeptime;
    int fd;
    pid_t pid;

    strcpy(cmd, basename(argv[0]));
    if (argc != 4) {
	fprintf(stderr, "usage: %s <tick-file> <tick-duration> <statusinterval>\n", cmd);
	exit(1);
    }
    if ((pid = fork()) != (pid_t) 0 ) {
	/* parent  -- just return */
	if ( pid == (pid_t) -1 ) {
	    perror("fork");
	    exit(1);
	}
	exit(0);
    } else {
	/* child -- go into ticker loop */
	chdir("/");
	ticker(argv[1], argv[2], argv[3]);
    }
    /* NOT REACHED */
}

/*---------------------------------------------------------------------------*/
/* Name    : ticker                                                          */
/* Purpose : main function that keeps track of uptime of the machine         */
/* Inputs  : tickfile, sleeptime and statusinterval values                   */
/* Outputs : none                                                            */
/*---------------------------------------------------------------------------*/

void
ticker(char *tickfile, char *s_sleeptime, char *s_statusint)
{
    struct tms t; /* not used */
    register time_t uptime, lastuptime;
    register int len;
    register int fd;
    register unsigned int sleeptime;
    int	statusinterval, statusint, statusduration;
    char	buf[32];	/* has to at most hold maxint */
    FILE	*fp;

    fclose(stdin);
    fclose(stdout);

    /*-----------------------------------------------------------------------*/
    /* Open the tickfile.  If we can't open it, then exit.  Lock the file    */
    /*-----------------------------------------------------------------------*/

    if ((fd = open(tickfile, O_WRONLY|O_CREAT|O_SYNC|O_TRUNC, 0644)) < 0) {
	fprintf(stderr, "%s:cannot open file %s\n", cmd, tickfile);
	perror(tickfile);
	exit(1);
    }
    if (flock(fd, LOCK_EX|LOCK_NB) == -1) {
	fprintf(stderr, "%s: cannot get lock on %s\n", cmd, tickfile);
	if (errno == EWOULDBLOCK)
	    fprintf(stderr, "... are you running another instance of %s?\n", cmd);
	perror(tickfile);
	exit(1);
    }

    sleeptime = atoi(s_sleeptime);
    statusinterval = atoi(s_statusint);

    if (statusinterval > 0) {
	lastuptime = (times(&t) / HZ) / 60 ;
	statusint = statusinterval * 1440;
	statusduration = lastuptime % statusint;
    }

    /*-----------------------------------------------------------------------*/
    /* Go into an infinite loop.  Wake up after every sleeptime seconds and  */
    /* write to tickfile. check whether statusinterval days have passed since*/
    /* we started.                                                           */
    /*-----------------------------------------------------------------------*/

    while (1) {
	sprintf(buf, "%d\n", time(0));
	len = strlen(buf);
	/* go to begining of file  to rewrite previous value */
	if (lseek(fd, 0L, SEEK_SET) == -1) {
	    fprintf(stderr, "%s: lseek on file %s failed\n",
		    cmd, tickfile);
	    perror(tickfile);
	    exit(1);
	}
	if (write(fd, buf, len) != len) {
	    fprintf(stderr, "%s: write to file %s failed\n", cmd, tickfile);
	    perror(tickfile);
	    exit(1);
	}

        /*-------------------------------------------------------------------*/
        /* If we meet the threshold of statusinterval, then invoke amdiag    */
	/* to send status update report                                      */
        /*-------------------------------------------------------------------*/

	if (statusinterval > 0) {
	    uptime = (times(&t) / HZ) / 60; /* time in minutes */
	    statusduration += uptime - lastuptime;
	    lastuptime = uptime;
	    if (statusduration >= statusint) {
		statusduration -= statusint;
		system("/usr/etc/amdiag STATUS &");
	    }
	}
	sleep(sleeptime);
    }
}
