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

#ident "$Id: pmobjstyle.c,v 1.2 1997/07/23 02:36:17 markgw Exp $"

#include <stdio.h>
#include <ctype.h>

#include "pmapi.h"
#include "impl.h"

int
main(int argc, char **argv)
{
    int		sts;
    int		user_style;
    int		kernel_style;
    char	*p;
    const char	*name;
    char	*pmProgname;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
        if (*p == '/')
            pmProgname = p+1;
    }

    if (argc != 1) {
	fprintf(stderr, "Usage: %s\n", pmProgname);
	fprintf(stderr, "Prints the object style of the running kernel.\n");
	exit(1);
    }

    if ((sts = __pmGetObjectStyle(&user_style, &kernel_style)) != 0) {
	fprintf(stderr, "%s: Error: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    name = __pmNameObjectStyle(kernel_style);
    printf("%s\n", (name == NULL) ? "unknown" : name);
    exit(0);
    /*NOTREACHED*/
}
