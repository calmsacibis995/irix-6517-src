/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ident "$Id"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include "evctr_util.h"

extern int errno;
extern char *sys_errlist[];

static char quant[] = { HZ/10, 10, 10, 6, 10, 6, 10, 10, 10 };
static char *pad  = "000      ";
static char *sep  = "\0\0.\0:\0:\0\0";
static char *nsep = "\0\0.\0 \0 \0\0";

void
printt(char *s, time_t	a)
{
	char	digit[9];
	register int	i;
	char	c;
	int	nonzero;

	for (i=0; i<9; i++) {
		digit[i] = (char)a % quant[i];
		a /= quant[i];
	}
	fprintf(stderr,s);
	nonzero = 0;
	while (--i>0) {
		c = digit[i]!=0 ? digit[i]+'0':
		    nonzero ? '0':
		    pad[i];
		if (c != '\0') putc(c,stderr);
		nonzero |= digit[i];
		c = nonzero?sep[i]:nsep[i];
		if (c != '\0') putc(c,stderr);
	}
	fprintf(stderr,"%c",digit[0]*100/HZ+'0');
	fprintf(stderr,"\n");
}

/*
** hmstime() sets current time in hh:mm:ss string format in stime;
*/

void
hmstime(char *stime)
{
	char	*ltime;
	time_t tme;

	tme = time((time_t *)0);
	ltime = ctime(&tme);
	strncpy(stime, ltime+11, 8);
	stime[8] = '\0';
}
 
void
diag(char *s)
{
	fprintf(stderr,"%s\n",s);
	exit(1);
}

/* ARGSUSED */
static void
onintr(int dummy)
{
}

void main(argc, argv)
int argc;
char **argv;
{
    struct tms buffer, obuffer;
    int		status;
    register int	p;
    int	c;
    time_t	before, after;
    /*    char	stime[9], etime[9];*/
    /*    char	cmd[80];*/
    extern	char	*optarg;
    extern	int	optind;
    int		i;
    char	*op;
    int		tflg = 0;
    int		delta;
    int		eflg = 1;
    pid_t	pid = -1;		/* for -p pid */
    int		excp = 0;		/* -x (exception level ctrs) */
    int		iter = -1;		/* -r (repeat) */

    evctr_init(argv[0]);

    /* check options; */
    while ((c = getopt(argc, argv, "e:p:r:t:x?")) != EOF) {
	switch(c) {

	case 'e':
		    evctr_opt(optarg, 1);
		    break;

	case 'p':
		    pid = (int)strtol(optarg, &op, 10);
		    if (*op != '\0')
			/* not a number */
			diag("-p requires numeric pid argument");
		    break;

	case 'r':
		    iter = (int)strtol(optarg, &op, 10);
		    if (*op != '\0')
			/* not a number */
			diag("-r requires numeric argument");
		    break;

	case 't':
		    delta = (int)strtol(optarg, &op, 10);
		    if (*op != '\0')
			/* not a number */
			diag("-t requires numeric argument");
		    break;

	case 'x':
		    excp = 1;
		    break;


	case '?':  diag("Usage: evctr [-e event,[event]...] [-x] cmd [args]\n       evctr [-e event,[event]...] [-r iter] [-t delta] [-x] -p pid");
			break;
	}
    }

    if (pid == -1 && optind >= argc) diag("Missing command");
    if (tflg && pid == -1) diag("-t option requires -p pid");
    if (iter != -1 && !tflg) diag("-r option requires -t delta");

    /* TODO - if pid != -1, this is not correct */
    before = times(&obuffer);

    if (eflg) {
	pid_t	lpid = pid;
	if (lpid == -1)
	    lpid = getpid();
	if (!evctr_start(lpid, excp)) {
	    if (pid != -1)
		exit(1);
	    fprintf(stderr, "evctr: Warning: failed to initialize event counters: %s\n", strerror(errno));
	    eflg = 0;
	}
    }

    if (pid == -1) {
	if ((p = fork()) == -1) diag("Try again.\n");
	if (p == 0) {
		setgid(getgid());
		execvp(*(argv+optind),(argv+optind));
		fprintf(stderr, "%s: %s\n", *(argv+optind), sys_errlist[errno]);
		exit(1);
	}
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	while (wait(&status) != p);

	if ((status&0377) != 0)
		fprintf(stderr,"Command terminated abnormally.\n");
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
    }
    else {
	if (!tflg) {
	    fprintf(stderr, "Watching process %d ...", pid);
	    signal(SIGINT, onintr);
	    pause();
	    fprintf(stderr, " done watching\n");
	}
	else {
	    i = 0;
	    for (i = 0; iter == -1 || i < iter; i++) {
		sleep(delta);
		if (iter == -1 || i+1 < iter)
		    break;
		if (!evctr_sample()) {
		    eflg = 0;
		    break;
		}
		fprintf(stderr,"\n");
		evctr_report(stderr);
	    }
	}
    }

    if (eflg) {
	if (evctr_sample()) {
	    fprintf(stderr,"\n");
	    evctr_report(stderr);
	}
    }

    if (pid != -1)
	evctr_release();

    after = times(&buffer);

    fprintf(stderr,"\n");
    printt("real", (after-before));
    if (pid != -1)
	fprintf(stderr, "Warning: CPU times are bogus at the moment\n");
    printt("user", buffer.tms_cutime - obuffer.tms_cutime);
    printt("sys ", buffer.tms_cstime - obuffer.tms_cstime);
    fprintf(stderr,"\n");

    exit(status>>8);
}
