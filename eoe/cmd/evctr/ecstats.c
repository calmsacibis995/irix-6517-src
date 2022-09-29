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
#ident "$Id: ecstats.c,v 1.5 1998/12/06 23:07:24 kenmcd Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "evctr_util.h"

int		Cflag;

void main(int argc, char **argv)
{
    int		c;
    char	*p;
    int		samples = 0;
    int		delta = 5;
    int		fflag = 0;
    int		rflag = 1;
    extern char	*optarg;
    extern int	optind;
    extern int	evctr_debug;
    extern void	disp(int, int, int, int);
    extern void	report(int, int, int);

    evctr_init(argv[0]);

    /* check options; */
    while ((c = getopt(argc, argv, "aCDefrs:?")) != EOF) {
	switch(c)  {

	    case 'a':	rflag = 0;
			break;

	    case 'C':	Cflag = 1;	/* full screen */
			break;

	    case 'D':	evctr_debug = 1;
			break;

	    case 'e':	fflag = 0;
			break;

	    case 'f':	fflag = 1;	/* full (all) events */
			break;

	    case 'r':	rflag = 1;	/* counter rate */
			break;

	    case 's':			/* limited samples */
			samples = (int)strtol(optarg, &p, 10);
			if (*p != '\0' || samples <= 0) {
			    fprintf(stderr, "ecstats: \"samples\" (%s) illegal, must be a positive integer\n",
				    optarg);
			    exit(1);
			}
			break;

	    case '?':
			goto use;
	}
    }

    if (optind < argc-1)
	goto use;
    
    if (optind == argc-1) {
	delta = strtol(argv[optind], &p, 10);
	if (*p != '\0' || delta <= 0) {
	    fprintf(stderr, "ecstats: \"interval\" (%s) illegal, must be a positive integer\n",
		    argv[optind]);
	    exit(1);
	}
    }

    if (samples != 0) {
	if (rflag == 1)
	    /* rates need one more iteration */
	    samples++;
    }

    if (Cflag) {
	disp(fflag, rflag, delta, samples);
	/*NOTREACHED*/
    }
	
    for ( ; ; ) {
	evctr_global_sample(1);
	report(fflag, rflag, delta);
	if (samples) {
	    if (samples == 1)
		/*
		 * all done, so exit
		 */
		exit(0);
	    samples--;
	}
	sleep(delta);
    }

use:
    fprintf(stderr, "Usage: ecstats [-aCDefr] [-s samples] [interval]\n");
    exit(1);
}
