/*
 * sh(1) ping PMDA 
 *
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

#ident "$Id: pmda.c,v 1.2 1999/05/11 00:28:03 kenmcd Exp $"

#include <stdio.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <syslog.h>

#include "shping.h"
#include "domain.h"

__uint32_t	cycletime = 120;	/* default cycle time, 2 minutes */
__uint32_t	timeout = 20;		/* response timeout in seconds */

cmd_t		*cmdlist = NULL;

/*ARGSUSED*/
void
onchld(int s)
{
    int done;
    int waitStatus;
    int	die = 0;

    while ((done = waitpid(-1, &waitStatus, WNOHANG)) > 0) {

	if (done != sprocpid)
	    continue;

	die = 1;

	if (WIFEXITED(waitStatus)) {

	    if (WEXITSTATUS(waitStatus) == 0)
		logmessage(LOG_INFO, 
			   "Sproc (pid=%d) exited normally\n",
			   done);
	    else
		logmessage(LOG_CRIT, 
			   "Sproc (pid=%d) exited with status = %d\n",
			   done, WEXITSTATUS(waitStatus));
	}
	else if (WIFSIGNALED(waitStatus)) {
	    
	    if (WCOREDUMP(waitStatus))
		logmessage(LOG_CRIT, 
			   "Sproc (pid=%d) received signal = %d and dumped core\n",
			   done, WTERMSIG(waitStatus));
	    else
		logmessage(LOG_CRIT, 
			   "Sproc (pid=%d) received signal = %d\n",
			   done, WTERMSIG(waitStatus));
	}
	else {
	    logmessage(LOG_CRIT, 
		       "Sproc (pid=%d) died, reason unknown\n", done);
	}
    }

    if (die) {
	logmessage(LOG_INFO, "Main process exiting\n");
	exit(0);
    }
}

void
usage(void)
{
    fprintf(stderr, 
"Usage: %s [options] configfile\n\n\
Options:\n\
  -C           parse configuration file and exit\n\
  -d domain    use domain (numeric) for metrics domain of PMDA\n\
  -I interval  cycle time in seconds between subsequent executions of each\n\
               command [default 120 seconds]\n\
  -l logfile   write log into logfile rather than using the default log\n\
  -t timeout   time in seconds before aborting the wait for individual\n\
               commands to complete [default 20 seconds]\n",
	pmProgname);
    exit(1);
}

int
main(int argc, char **argv)
{
    pmdaInterface	dispatch;
    int			n = 0;
    int			i;
    int			err = 0;
    int			line;
    int                 numcmd = 0;
    int                 parseonly = 0;
    char		*configfile;
    FILE		*conf;
    char		*endnum;
    char		*p;
    char		*tag;
    char		lbuf[256];

    extern char		*optarg;
    extern int		optind;

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    pmdaDaemon(&dispatch, PMDA_INTERFACE_2, pmProgname, SHPING, 
		"shping.log", "/var/pcp/pmdas/shping/help");

    while ((n = pmdaGetOpt(argc, argv,"CD:d:I:l:t:?",
			&dispatch, &err)) != EOF) {
	switch (n) {

	    case 'C':
		parseonly = 1;
		break;

	    case 'I':
		cycletime = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, 
		    	    "%s: -I requires number of seconds as argument\n",
			    pmProgname);
		    err++;
		}
		break;

	    case 't':
		timeout = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, 
		    	    "%s: -t requires number of seconds as argument\n",
			    pmProgname);
		    err++;
		}
		break;

	    case '?':
		err++;
	}
    }

    if (err || optind != argc -1) {
    	usage();
	/*NOTREACHED*/
    }

    configfile = argv[optind];
    if ((conf = fopen(configfile, "r")) == NULL) {
	fprintf(stderr, "%s: Unable to open config file \"%s\": %s\n",
	    pmProgname, configfile, strerror(errno));
	exit(1);
    }
    line = 0;

    for ( ; ; ) {
	if (fgets(lbuf, sizeof(lbuf), conf) == NULL)
	    break;

	line++;
	p = lbuf;
	while (*p && isspace(*p))
	    p++;
	if (*p == '\0' || *p == '\n' || *p == '#')
	    continue;
	tag = p++;
	while (*p && !isspace(*p))
	    p++;
	if (*p)
	    *p++ = '\0';
	while (*p && isspace(*p))
	    p++;
	if (*p == '\0' || *p == '\n') {
	    fprintf(stderr, "[%s:%d] missing command after tag \"%s\"\n",
		configfile, line, tag);
	    exit(1);
	}

	numcmd++;
	if (parseonly)
	    continue;
	if ((cmdlist = (cmd_t *)realloc(cmdlist, numcmd * sizeof(cmd_t))) == NULL) {
	    __pmNoMem("main:cmdlist", numcmd * sizeof(cmd_t), 
		     PM_FATAL_ERR);
	    /*NOTREACHED*/
	}

	cmdlist[numcmd-1].tag = strdup(tag);
	cmdlist[numcmd-1].cmd = strdup(p);
	cmdlist[numcmd-1].status = STATUS_NOTYET;
	cmdlist[numcmd-1].error = 0;
	cmdlist[numcmd-1].real = cmdlist[numcmd-1].usr = cmdlist[numcmd-1].sys = -1;

	/* trim trailing newline */
	p = cmdlist[numcmd-1].cmd;
	while (*p && *p != '\n')
	    p++;
	*p = '\0';
    }

    fclose(conf);

    if (numcmd == 0) {
	fprintf(stderr, "%s: No commands in config file \"%s\"?\n",
	    pmProgname, configfile);
	exit(1);
    }
    else if (parseonly)
	exit(0);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	fprintf(stderr, "Parsed %d commands\n", numcmd);
	fprintf(stderr, "Tag\tCommand\n");
	for (i = 0; i < numcmd; i++) {
	    fprintf(stderr, "[%s]\t%s\n", cmdlist[i].tag, cmdlist[i].cmd);
	}
    }
#endif

    /* set up indom description */
    indomtab.it_numinst = numcmd;
    if ((indomtab.it_set = (pmdaInstid *)malloc(numcmd*sizeof(pmdaInstid))) == NULL) {
	__pmNoMem("main.indomtab", numcmd * sizeof(pmdaInstid), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    for (i = 0; i < numcmd; i++) {
	indomtab.it_set[i].i_inst = i;
	indomtab.it_set[i].i_name = cmdlist[i].tag;
    }

    signal(SIGCHLD, onchld);

    pmdaOpenLog(&dispatch);
    shping_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
    /*NOTREACHED*/
}
