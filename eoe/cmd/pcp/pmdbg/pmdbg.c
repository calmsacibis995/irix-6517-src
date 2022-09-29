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

#ident "$Id: pmdbg.c,v 2.16 1997/09/25 06:48:36 markgw Exp $"

/*
 * pmdbg - help for PCP debug flags
 *
 * If you add a flag here, you should also add the flag to dbpmda.
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"

static struct {
    int		flag;
    char	*name;
    char	*text;
} foo[] = {
    { DBG_TRACE_PDU,	"PDU",
		"Trace PDU traffic at the Xmit and Recv level" },
    { DBG_TRACE_FETCH,	"FETCH",
		"Dump results from pmFetch" },
    { DBG_TRACE_PROFILE,	"PROFILE",
		"Trace changes and xmits for instance profile" },
    { DBG_TRACE_VALUE,		"VALUE",
		"Diags for metric value extraction and conversion" },
    { DBG_TRACE_CONTEXT,	"CONTEXT",
		"Trace changes to contexts" },
    { DBG_TRACE_INDOM,		"INDOM",
		"Low-level instance profile xfers" },
    { DBG_TRACE_PDUBUF,		"PDUBUF",
		"Trace pin/unpin ops for PDU buffers" },
    { DBG_TRACE_LOG,		"LOG",
		"Diags for archive log manipulations" },
    { DBG_TRACE_LOGMETA,	"LOGMETA",
		"Diags for meta-data operations on archive logs" },
    { DBG_TRACE_OPTFETCH,	"OPTFETCH",
		"Trace optFetch magic" },
    { DBG_TRACE_AF,		"AF",
		"Trace asynchronous event scheduling" },
    { DBG_TRACE_APPL0,		"APPL0",
		"Application-specific flag 0" },
    { DBG_TRACE_APPL1,		"APPL1",
		"Application-specific flag 1" },
    { DBG_TRACE_APPL2,		"APPL2",
		"Application-specific flag 2" },
    { DBG_TRACE_PMNS,		"PMNS",
		"Diags for PMNS manipulations" },
    { DBG_TRACE_LIBPMDA,        "LIBPMDA",
	        "Trace PMDA callbacks in libpcp_pmda" },
    { DBG_TRACE_TIMECONTROL,	"TIMECONTROL",
    		"Trace Time Control API" },
    { DBG_TRACE_PMC,		"PMC",
    		"Trace metrics class operations" },
    { DBG_TRACE_INTERP,		"INTERP",
		"Diags for value interpolation in archives" },
};

static int	nfoo = sizeof(foo) / sizeof(foo[0]);

static char	*fmt = "DBG_TRACE_%-11.11s %7d  %s\n";

int
main(int argc, char **argv)
{
    int		i;
    int		c;
    int		code;
    char	*p;
    int		errflag = 0;
    extern char	*optarg;
    extern int	optind;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "l?")) != EOF) {
	switch (c) {

	case 'l':	/* list all flags */
	    printf("Performance Co-Pilot Debug Flags\n");
	    printf("#define              Value  Meaning\n");
	    for (i = 0; i < nfoo; i++)
		printf(fmt, foo[i].name, foo[i].flag, foo[i].text);
	    exit(0);
	    /*NOTREACHED*/

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind >= argc) {
	fprintf(stderr,
"Usage: %s [options] [code ..]\n\
\n\
Options:\n\
  -l              displays mnemonic and decimal values of all PCP bit fields\n",
		pmProgname);
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (optind < argc) {
	char	*p = argv[optind];
	for (p = argv[optind]; *p && isdigit(*p); p++)
	    ;
	if (*p == '\0')
	    sscanf(argv[optind], "%d", &code);
	else {
	    char	*q;
	    p = argv[optind];
	    if (*p == '0' && (p[1] == 'x' || p[1] == 'X'))
		p = &p[2];
	    for (q = p; isxdigit(*q); q++)
		;
	    if (*q != '\0' || sscanf(p, "%x", &code) != 1) {
		printf("Cannot decode \"%s\" - neither decimal nor hexadecimal\n", argv[optind]);
		goto next;
	    }
	}
	printf("Performance Co-Pilot -- pmDebug value = %d (0x%x)\n", code, code);
	printf("#define                 Value  Meaning\n");
	for (i = 0; i < nfoo; i++) {
	    if (code & foo[i].flag)
		printf(fmt, foo[i].name, foo[i].flag, foo[i].text);
	}

next:
	optind++;
    }

    return 0;
}
