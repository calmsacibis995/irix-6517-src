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

#ident "$Id: pmlc.c,v 2.28 1997/12/19 04:18:11 markgw Exp $"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#if defined(sgi)
#include <bstring.h>
#endif
#include <sys/select.h>
#include "pmapi_dev.h"
#include "./pmlc.h"
#include "./gram.h"

char		*configfile = NULL;
__pmLogCtl	logctl;
int		parse_done = 0;
int		pid = PM_LOG_NO_PID;
int		port = PM_LOG_NO_PORT;
int		zflag = 0;		/* for -z */
char 		*tz = NULL;	/* for -Z timezone */
int		tztype = TZ_LOCAL;	/* timezone for status cmd */

int		eflag;
int		iflag;

extern int	parse_stmt;
extern int	tzchange;

static char title[] = "Performance Co-Pilot Logger Control (pmlc), Version %s\n\n%s\n";
static char menu[] =
"pmlc commands\n\n"
"  show loggers [@<host>]             display <pid>s of running pmloggers\n"
"  connect _logger_id [@<host>]       connect to designated pmlogger\n"
"  status                             information about connected pmlogger\n"
"  query metric-list                  show logging state of metrics\n"
"  new volume                         start a new log volume\n"
"  flush                              flush the log buffers to disk\n"
"\n"
"  log { mandatory | advisory } on <interval> _metric-list\n"
"  log { mandatory | advisory } off _metric-list\n"
"  log mandatory maybe _metric-list\n"
"\n"
"  timezone local|logger|'<timezone>' change reporting timezone\n"
"  help                               print this help message\n"
"  quit                               exit from pmlc\n"
"\n"
"  _logger_id   is  primary | <pid> | port <n>\n"
"  _metric-list is  _metric-spec | { _metric-spec ... }\n"
"  _metric-spec is  <metric-name> | <metric-name> [ <instance> ... ]\n";


int
main(int argc, char **argv)
{
    int			c;
    int			sts;
    char		*p;
    int			errflag = 0;
    char		*host = NULL;
    char		local[MAXHOSTNAMELEN];
    char		*pmnsfile = PM_NS_DEFAULT;
    char		*endnum;
    int			primary;
    extern char		*optarg;
    extern int		optind;

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    __pmSetAuthClient();

    iflag = isatty(0);

    while ((c = getopt(argc, argv, "D:eh:in:Pp:zZ:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'e':		/* echo input */
	    eflag++;
	    break;

	case 'h':		/* hostname for PMCD to contact */
	    host = optarg;
	    break;

	case 'i':		/* be interactive */
	    iflag++;
	    break;

	case 'n':		/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'P':		/* connect to primary logger */
	    if (port != PM_LOG_NO_PORT) {
		fprintf(stderr, "%s: at most one of -P and/or -p and/or a pid may be specified\n", pmProgname);
		errflag++;
	    }
	    else {
		port = PM_LOG_PRIMARY_PORT;
	    }
	    break;

	case 'p':		/* connect via port */
	    if (port != PM_LOG_NO_PORT) {
		fprintf(stderr, "%s: at most one of -P and/or -p and/or a pid may be specified\n", pmProgname);
		errflag++;
	    }
	    else {
		port = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0' || port <= PM_LOG_PRIMARY_PORT) {
		    fprintf(stderr, "%s: port must be numeric and greater than %d\n", pmProgname, PM_LOG_PRIMARY_PORT);
		    errflag++;
		}
	    }
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    zflag++;
	    tztype = TZ_LOGGER;
	    tzchange = 1;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    if ((tz = strdup(optarg)) == NULL) {
		__pmNoMem("initialising timezone", strlen(optarg), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    tztype  = TZ_OTHER;
	    tzchange = 1;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (optind < argc-1)
	errflag++;
    else if (optind == argc-1) {
	/* pid was specified */
	if (port != PM_LOG_NO_PORT) {
	    fprintf(stderr, "%s: at most one of -P and/or -p and/or a pid may be specified\n", pmProgname);
	    errflag++;
	}
	else {
	    pid = (int)strtol(argv[optind], &endnum, 10);
	    if (*endnum != '\0' || pid <= PM_LOG_PRIMARY_PID) {
		fprintf(stderr, "%s: pid must be a numeric process id and greater than %d\n", pmProgname, PM_LOG_PRIMARY_PID);
		errflag++;
	    }
	}
    }

    if (errflag == 0 && host != NULL && pid == PM_LOG_NO_PID && port == PM_LOG_NO_PORT) {
	fprintf(stderr, "%s: -h may not be used without -P or -p or a pid\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
		"Usage: %s [options] [pid]\n\n"
		"Options:\n"
		"  -e           echo input\n"
		"  -h host      connect to pmlogger on host\n"
		"  -i           be interactive and prompt\n"
		"  -n pmnsfile  use an alternative PMNS\n"
		"  -P           connect to primary pmlogger\n"
		"  -p port      connect to pmlogger on this TCP/IP port\n"
		"  -Z timezone  set reporting timezone\n"
		"  -z           set reporting timezone to local time for pmlogger\n",
		pmProgname);
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n",
		    pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

    if (host == NULL) {
	(void)gethostname(local, MAXHOSTNAMELEN);
	local[MAXHOSTNAMELEN-1] = '\0';
	host = local;
    }

    primary = 0;
    if (port == PM_LOG_PRIMARY_PORT || pid == PM_LOG_PRIMARY_PID)
	primary = 1;

    if (pid != PM_LOG_NO_PID || port != PM_LOG_NO_PORT)
	sts = ConnectLogger(host, &pid, &port);

    if (iflag)
	printf(title, PCP_VERSION, menu);

    if (pid != PM_LOG_NO_PID || port != PM_LOG_NO_PORT) {
	if (sts < 0) {
	    if (primary) {
		fprintf(stderr, "Unable to connect to primary pmlogger at %s: ",
			host);
		if (still_connected(sts))
		    fprintf(stderr, "%s\n", pmErrStr(sts));
	    }
	    else if (port != PM_LOG_NO_PORT) {
		fprintf(stderr, "Unable to connect to pmlogger on port %d at %s: ",
			port, host);
		if (still_connected(sts))
		    fprintf(stderr, "%s\n", pmErrStr(sts));
	    }
	    else {
		fprintf(stderr, "Unable to connect to pmlogger pid %d at %s: ",
			pid, host);
		if (still_connected(sts))
		    fprintf(stderr, "%s\n", pmErrStr(sts));
	    }
	}
	else {
	    if (primary)
		printf("Connected to primary pmlogger at %s\n", host);
	    else if (port != PM_LOG_NO_PORT)
		printf("Connected to pmlogger on port %d at %s\n", port, host);
	    else
		printf("Connected to pmlogger pid %d at %s\n", pid, host);
	}
    }

    for ( ; ; ) {
	char		*realhost;
	extern int	logfreq;
	extern void	ShowLoggers(char *);
	extern void	Query(void);
	extern void	LogCtl(int, int, int);
	extern void	Status(int, int);
	extern void	NewVolume(void);
	extern void	Sync(void);
	extern int	yywrap(void);

	parse_stmt = -1;
	metric_cnt = 0;
	yyparse();
	if (yywrap()) {
	    if (iflag)
		putchar('\n');
	    break;
	}
	if (metric_cnt < 0)
	    continue;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    printf("stmt=%d, state=%d, control=%d, hostname=%s, pid=%d, port=%d, mynumb=%d \n",
		   parse_stmt, state, control, hostname, pid, port, mynumb);
#endif

	realhost = (hostname == NULL) ? host : hostname;
	switch (parse_stmt) {

	    case SHOW:
		ShowLoggers(realhost);
		break;

	    case CONNECT:
		primary = 0;
		if (port == PM_LOG_PRIMARY_PORT || pid == PM_LOG_PRIMARY_PID)
		    primary = 1;
		if ((sts = ConnectLogger(realhost, &pid, &port)) < 0) {
		    if (primary) {
			fprintf(stderr, "Unable to connect to primary pmlogger at %s: ",
				realhost);
		    }
		    else if (port != PM_LOG_NO_PORT) {
			fprintf(stderr, "Unable to connect to pmlogger on port %d at %s: ",
				port, realhost);
		    }
		    else {
			fprintf(stderr, "Unable to connect to pmlogger pid %d at %s: ",
				pid, realhost);
		    }
		    if (still_connected(sts))
			fprintf(stderr, "%s\n", pmErrStr(sts));
		}
		else
		    /* if the timezone is "logger time", it has changed
		     * because the logger may be in a different zone
		     * (note that tzchange may already be set (e.g. -Z and
		     * this connect is the first).
		     */
		    tzchange |= (tztype == TZ_LOGGER);
		break;		

	    case HELP:
		puts(menu);
		break;

	    case LOG:
		if (state == PM_LOG_ENQUIRE) {
		    Query();
		    break;
		}
		if (logfreq == -1)
		    logfreq = 0;
		if (state == PM_LOG_ON) {
		    if (logfreq < 0) {
fprintf(stderr, "Logging delta (%d msec) must be positive\n", logfreq);
			break;
		    }
		    else if (logfreq > PMLC_MAX_DELTA) {
fprintf(stderr, "Logging delta (%d msec) cannot be bigger than %d msec\n", logfreq, PMLC_MAX_DELTA);
			break;
		    }
		}
		LogCtl(control, state, logfreq);
		break;

	    case QUIT:
		printf("Goodbye\n");
		exit(0);
		/*NOTREACHED*/
		break;

	    case STATUS:
		Status(pid, primary);
		break;

	    case NEW:
		NewVolume();
		break;

	    case TIMEZONE:
		tzchange = 1;
		break;

	    case SYNC:
		Sync();
		break;
	}

	if (hostname != NULL) {
	    free(hostname);
	    hostname = NULL;
	}

    }

    exit(0);
    /*NOTREACHED*/
}
