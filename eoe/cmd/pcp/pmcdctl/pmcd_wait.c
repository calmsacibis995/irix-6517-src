/*
 * Copyright 1998, Silicon Graphics, Inc.
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

#ident "$Id: pmcd_wait.c,v 1.2 1999/02/12 03:17:23 kenmcd Exp $"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "pmapi_dev.h" /* for __pmSetAuthClient() */

/* possible exit states */
#define EXIT_STS_SUCCESS 	0
#define EXIT_STS_USAGE 		1
#define EXIT_STS_TIMEOUT 	2
#define EXIT_STS_UNIXERR 	3
#define EXIT_STS_PCPERR 	4

extern int	errno;
extern int 	optind;

static char     localhost[MAXHOSTNAMELEN];
static char	*hostname = NULL;
static long	delta = 60;
static int	verbose = 0;


static void
PrintUsage(void)
{
    fprintf(stderr,
"Usage: %s [options] \n\
\n\
Options:\n\
  -h host	wait for PMCD on host\n\
  -t interval   maximum interval to wait for PMCD [default 60 seconds]\n\
  -v		turn on output messages\n",
		pmProgname);
}

static void
ParseOptions(int argc, char *argv[])
{
    int		c;
    int		sts;
    int		errflag = 0;
    extern char	*optarg;
    extern int	pmDebug;
    char	*msg;
    struct 	timeval delta_time;

    while ((c = getopt(argc, argv, "D:h:t:v?")) != EOF) {
	switch (c) {

	    case 'D':	/* debug flag */
		sts = __pmParseDebug(optarg);
		if (sts < 0) {
		    fprintf(stderr, "%s: Unrecognized debug flag specification (%s)\n",
			    pmProgname, optarg);
		    errflag++;
		}
		else
		    pmDebug |= sts;
		break;


	    case 'h':	/* contact PMCD on this hostname */
		hostname = optarg;
		break;


	    case 't':   /* delta to wait */
		if (pmParseInterval(optarg, &delta_time, &msg) < 0) {
		    fprintf(stderr, "%s: Illegal -t argument (%s)\n", 
			pmProgname, optarg);
		    fputs(msg, stderr);
		    free(msg);
		    errflag++;
		}
		delta = delta_time.tv_sec;
		if (delta <= 0) {
		    fprintf(stderr, "%s: -t argument must be at least 1 second\n",
			pmProgname);
		    errflag++;
		}
		break;

	    case 'v':
		verbose = 1;
		break;

	    default:
	    case '?':
		PrintUsage();
		exit(EXIT_STS_SUCCESS);
		/*NOTREACHED*/
	}
    }

    if (optind < argc) {
	fprintf(stderr, "%s: Too many arguments\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	PrintUsage();
	exit(EXIT_STS_USAGE);
    }

}

void 
PrintTimeout(void)
{
    if (verbose) {
	fprintf(stderr, "%s: Failed to connect to PMCD on host \"%s\""
		" in %d seconds\n",
		pmProgname, hostname, delta);
    }
}

int
main(int argc, char **argv)
{
    int		sts;
    char	*p;
    int		onesec = CLK_TCK; /* really syscall */
    char	env[256];
    long	delta_count;

    __pmSetAuthClient();

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; pmProgname && *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    ParseOptions(argc, argv);

    if (hostname == NULL) {
	(void)gethostname(localhost, MAXHOSTNAMELEN);
	localhost[MAXHOSTNAMELEN-1] = '\0';
	hostname = localhost;
    }

    sts = sprintf(env, "%s=%d", "PMCD_CONNECT_TIMEOUT", delta);
    if (sts < 0) {
	if (verbose) {
	    fprintf(stderr, "%s: Failed to create env string: %s\n",
		pmProgname, strerror(errno));
        }
	exit(EXIT_STS_UNIXERR);
    }
    sts = putenv(env);
    if (sts != 0) {
	if (verbose) {
	    fprintf(stderr, "%s: Failed to set PMCD_CONNECT_TIMEOUT: %s\n",
		pmProgname, strerror(errno));
        }
	exit(EXIT_STS_UNIXERR);
    }

    delta_count = delta;
    for(;;) {
        sts = pmNewContext(PM_CONTEXT_HOST, hostname);

        if (sts >= 0) {
	    (void)pmDestroyContext(sts);
	    exit(EXIT_STS_SUCCESS);
	}
	if (sts == -ECONNREFUSED || sts == PM_ERR_IPC) {
	    delta_count--;
	    if (delta_count < 0) {
		PrintTimeout();	
		exit(EXIT_STS_TIMEOUT);
	    }
	    sginap(onesec); 
        }
	else if (sts == PM_ERR_TIMEOUT) {
	    PrintTimeout();	
	    exit(EXIT_STS_TIMEOUT);
	}
	else {
	    if (verbose) {
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
			pmProgname, hostname, pmErrStr(sts));
	    }
	    if (sts > PM_ERR_BASE)
		exit(EXIT_STS_UNIXERR);
	    else
		exit(EXIT_STS_PCPERR);

	}
    }

    /*NOTREACHED*/
}
