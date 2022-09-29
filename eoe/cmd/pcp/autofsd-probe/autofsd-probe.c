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

#include <stdio.h>
#include <sys/time.h>
#include <libgen.h>
#include <rpc/rpc.h>
#include "pmapi.h"
#include "impl.h"

#define AUTOFSD_PROGRAM 100099UL
#define AUTOFSD_VERSION 1UL

/*
 * probe autofsd(1M)
 */

main(int argc, char **argv)
{
    struct timeval	tv = { 10, 0 };
    CLIENT		*clnt;
    enum clnt_stat	stat;
    int			c;
    char		*host = "localhost";
    char		*p;
    int			errflag = 0;
    extern char		*optarg;
    extern int		optind;

    pmProgname = basename(argv[0]);

    while ((c = getopt(argc, argv, "h:t:?")) != EOF) {
	switch (c) {

	case 'h':	/* contact autofsd on this hostname */
	    host = optarg;
	    break;

	case 't':	/* change timeout interval */
	    if (pmParseInterval(optarg, &tv, &p) < 0) {
		fprintf(stderr, "%s: illegal -t argument\n", pmProgname);
		fputs(p, stderr);
		free(p);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    fprintf(stderr, "Usage: %s [-h host] [-t timeout]\n", pmProgname);
	    errflag++;
	    break;
	}
    }

    if (errflag)
	exit(4);

    if ((clnt = clnt_create(host, AUTOFSD_PROGRAM, AUTOFSD_VERSION, "udp")) == NULL) {
	clnt_pcreateerror("clnt_create");
	exit(2);
    }

    /*
     * take control of the timeout algorithm
     */
    clnt_control(clnt, CLSET_TIMEOUT, (char *)&tv);
    clnt_control(clnt, CLSET_RETRY_TIMEOUT, (char *)&tv);

    stat = clnt_call(clnt, NULLPROC, xdr_void, (char *)0, xdr_void, (char *)0, tv);

    if (stat != RPC_SUCCESS) {
	clnt_perror(clnt, "clnt_call");
	exit(1);
    }

    exit(0);
}
