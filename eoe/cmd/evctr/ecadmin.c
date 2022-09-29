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
#ident "$Id: ecadmin.c,v 1.5 1998/12/16 23:03:45 kenmcd Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/times.h>
#include <sys/capability.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include "evctr_util.h"

void
main(int argc, char **argv)
{
    int		done_init = 0;
    int		sts = 0;
    int		c;
    int		rflag = 0;
    int		lflag = 0;
    int		mixed = 0;
    int		setflag = 0;
    char	*cmd = basename(argv[0]);
    extern char	*optarg;
    extern int	optind;
    extern int	evctr_debug;

    /* check options; */
    while ((c = getopt(argc, argv, "ad:De:lMrT?")) != EOF)
	    switch(c)  {

	    case 'a':   setflag = 1;
			if (!done_init) {
			    evctr_init(cmd);
			    done_init = 1;
			}
			evctr_opt("*", 1);
			break;

	    case 'd':   setflag = 1;
			if (!done_init) {
			    evctr_init(cmd);
			    done_init = 1;
			}
			evctr_opt(optarg, -1);
			break;

	    case 'D':   evctr_debug = 1;
			break;

	    case 'e':   setflag = 1;
			if (!done_init) {
			    evctr_init(cmd);
			    done_init = 1;
			}
			evctr_opt(optarg, 1);
			break;

	    case 'l':   lflag = 1;
			break;

	    case 'M':	/* mixed R10K and R12K is OK */
			fprintf(stderr, "%s: Warning: -M allows statistically questionable counter combinations\n", cmd);
			mixed = 1;
			break;

	    case 'r':   rflag = 1;
			break;

	    case 'T':	/* trust me */
			fprintf(stderr, "%s: Warning: -T allows semantically questionable counter combinations\n", cmd);
			evctr_trustme();
			break;

	    case '?':   goto use;
	    }

    if (optind <= argc - 1)
	goto use;

    if (rflag + setflag + lflag == 0)
	goto use;

    if (!done_init) {
	evctr_init(cmd);
	done_init = 1;
    }

    if (cpucount.r10k > 0 && cpucount.r12k > 0 && mixed == 0) {
	fprintf(stderr, "ecadmin: Error: counters for mixed R10000 and R12000 systems is not supported\n");
	exit(1);
    }

    if (lflag)
	evctr_global_status(stdout);

    if (rflag + setflag != 0) {
	if (cap_envl(0, CAP_DEVICE_MGT, (cap_value_t) 0) == -1) {
	    fprintf(stderr, "ecadmin: Error: cannot change counter setup: %s\n", strerror(errno));
	    exit(1);
	}
    }

    if (rflag) {
	if (evctr_global_release() == 0)
	    sts = 1;
    }

    if (setflag) {
	if (evctr_global_set() == 0)
	    sts = 1;
    }

    exit(sts);

use:
    fprintf(stderr, "Usage: ecadmin [-aDlrT] [-d event,[event]...] [-e event,[event]...]\n");
    exit(1);
}
