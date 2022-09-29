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
#ident "$Id: report.c,v 1.3 1998/12/16 23:03:45 kenmcd Exp $"

#include <stdio.h>
#include "evctr_util.h"

void
report(int fflag, int rflag, int interval)
{
    int		ctr;
    int		done;

    if (evctr_debug) {
	printf("Counters:");
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++ )
	    printf(" %llu", Count[ctr]);
	putchar('\n');
    }
    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++ ) {
	if (Active[ctr]) {
	    done = 0;
	    if (evctr_debug > 1)
		printf("new=%llu - old=%llu x mux=%d / interval=%d\n",
		    Count[ctr], OldCount[ctr], Mux[ctr], interval);
	    if (rflag) {
		if (OldCount[ctr] != -1) {
		    printf("%18.2f", ((double)((Count[ctr] - OldCount[ctr]) * Mux[ctr])) / interval);
		    done = 1;
		}
	    }
	    else {
		printf("%18llu", Count[ctr] * Mux[ctr]);
		done = 1;
	    }
	    if (done) {
		putchar(' ');
		if (Sem[ctr] != EVCTR_SEM_OK)
		    putchar('?');
		else if (Mux[ctr] > 1)
		    putchar('*');
		else
		    putchar(' ');
	    }
	    OldCount[ctr] = Count[ctr];
	}
	else if (fflag)
	    printf("%18s  ", "<inactive>");
	if ((Active[ctr] && done) || fflag)
	    printf(" [%2d] %s\n", ctr, EventDesc[ctr].text);
    }
    putchar('\n');
}
