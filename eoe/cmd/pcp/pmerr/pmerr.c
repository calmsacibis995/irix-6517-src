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

#ident "$Id: pmerr.c,v 2.7 1997/10/24 06:15:45 markgw Exp $"

#include <stdio.h>
#include <ctype.h>
#include "pmapi.h"
#include <ctype.h>

extern void __pmDumpErrTab(FILE *);

int
main(int argc, char **argv)
{
    int		code;
    int		sts;
    char	*p;
    char	*q;

    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
	__pmDumpErrTab(stdout); 
	exit(1);
    }
    else if (argc > 1 && strcmp(argv[1], "-?") == 0) {
	argc = 0;
    }
    else if (argc > 1 && argv[1][0] == '-' && !isxdigit(argv[1][1])) { 
	fprintf(stderr, "Illegal option -- %s\n", &argv[1][1]);
	argc = 0;
    }

    if (argc == 0) {
	fprintf(stderr,
"Usage: pmerr [options] [code]\n\n"
"  -l   causes all known error codes to be listed\n");
	exit(1);
    }

    while (argc > 1) {
	sts = 0;
	p = argv[1];
	if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
	    p = &p[2];
	    for (q = p; isxdigit(*q); q++)
		;
	    if (*q == '\0')
		sts = sscanf(p, "%x", &code);
	}
	if (sts < 1)
	    sts = sscanf(argv[1], "%d", &code);
	if (sts != 1) {
	    printf("Cannot decode \"%s\" - neither decimal nor hexadecimal\n", argv[1]);
	    goto next;
	}

	if (code > 0) {
	    code = -code;
	    printf("Code is positive, assume you mean %d\n", code);
	}

	if (-1100 <= code && code <= -1000)
	    printf("Code: %d 0x%x [PCP 1.x?] Text: %s\n", code, code, pmErrStr(XLATE_ERR_1TO2(code)));
	else
	    printf("Code: %d 0x%x Text: %s\n", code, code, pmErrStr(code));

next:
	argc--;
	argv++;
    }

    return 0;
}
