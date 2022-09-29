/*
 * chkhelp helpfile metric-name ...
 *
 * check help files build by newhelp
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

#ident "$Id: chkhelp.c,v 2.12 1999/02/12 03:33:50 kenmcd Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

extern int	errno;

#define VERSION 2
static int	version = VERSION;

/*
 * these two are from libpcp/gettext.c
 */
typedef struct {
    pmID	pmid;
    __uint32_t	off_oneline;
    __uint32_t	off_text;
} help_idx_t;

typedef struct {
    int		dir_fd;
    int		pag_fd;
    int		numidx;
    help_idx_t	*index;
    char	*text;
    int		textlen;
} help_t;

static int
next(int *ident, int *type)
{
    static help_t	*hp = NULL;
    static int		nextidx;
    pmID		pmid;
    __pmID_int		*pi = (__pmID_int *)&pmid;
    extern void		*__pmdaHelpTab(void);

    if (hp == NULL) {
	hp = (help_t *)__pmdaHelpTab();
	/*
	 * Note, skip header and version info at index[0]
	 */
	nextidx = 1;
    }

    if (nextidx > hp->numidx)
	return 0;

    pmid = hp->index[nextidx].pmid;
    nextidx++;

    if (pi->pad == 0) {
	*ident = (int)pmid;
	*type = 1;
    }
    else {
	pi->pad = 0;
	*ident = (int)pmid;
	*type = 2;
    }

    return 1;
}

int
main(argc, argv)
int argc;
char *argv[];
{
    int		sts;
    int		c;
    int		help = 0;
    int		oneline = 0;
    char	*pmnsfile = PM_NS_DEFAULT;
    char	*p;
    int		errflag = 0;
    int		aflag = 0;
    int		allpmid = 0;
    int		allindom = 0;
    char	*filename;
    int		handle;
    char	*tp;
    char	*name;
    int		id;
    int		next_type;
    char	*endnum;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "D:Hin:Opv:?")) != EOF) {
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

	case 'H':	/* help text */
	    help = 1;
	    break;
	
	case 'i':
	    aflag++;
	    allindom = 1;
	    break;

	case 'n':	/* alternative namespace file */
	    pmnsfile = optarg;
	    break;

	case 'O':	/* oneline text */
	    oneline = 1;
	    break;

	case 'p':
	    aflag++;
	    allpmid = 1;
	    break;

	case 'v':	/* version 1 or 2 */
	    version = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -v requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    if (version < 1 || version > 2) {
		fprintf(stderr, "%s: version must be 1 or 2\n", pmProgname);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (aflag && optind != argc - 1) {
	fprintf(stderr, "%s: metric-name arguments cannot be used with -i or -p\n\n",
	    pmProgname);
	errflag = 1;
    }

    if (aflag == 0 && optind == argc-1 && oneline+help != 0) {
	fprintf(stderr, "%s: -O or -H require metric-name arguments or -i or -p\n\n",
	    pmProgname);
	errflag = 1;
    }

    if (errflag || optind >= argc) {
	fprintf(stderr,
"Usage: %s helpfile\n\
       %s [options] helpfile [metricname ...]\n\
\n\
Options:\n\
  -H           display verbose help text\n\
  -i           process all the instance domains\n\
  -n pmnsfile  use an alternative PMNS\n\
  -O           display the one line help summary\n\
  -p           process all the metrics (PMIDs)\n\
  -v version   version 1 or 2 [default 2]\n\
\n\
No options implies silently check internal integrity of the helpfile.\n",
		pmProgname, pmProgname);
	exit(1);
    }

    if (version == 1) {
	int	i;
	char	**pargv;

	pargv = (char **)malloc((argc+1)*sizeof(*pargv));

	if (pargv != NULL) {
	    for (i = 1; i < argc; i++)
	       pargv[i] = argv[i];
	    pargv[argc] = NULL;

	    execvp("/usr/pcp/bin/chkhelp_v1", argv);
	}

	fprintf(stderr, "%s: failed to exec \"/usr/pcp/bin/chkhelp_v1\": %s\n",
		pmProgname, strerror(errno));
	fprintf(stderr, "pcp.sw.compat must be installed to check version 1 help text files.\n");
	exit(1);
    }

    filename = argv[optind++];
    if ((handle = pmdaOpenHelp(filename)) < 0) {
	fprintf(stderr, "pmdaOpenHelp: failed to open \"%s\": ", filename);
	if (handle == -EINVAL)
	    fprintf(stderr, "Bad format, not version %d PCP help text\n", version);
	else
	    fprintf(stderr, "%s\n", pmErrStr(handle));
	exit(1);
    }

    if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(sts));
	exit(1);
    }

    if (help + oneline == 0 && (optind < argc || aflag))
	/* if metric names, -p or -i => -O is default */
	oneline = 1;

    if (optind == argc && aflag == 0)
	/* no metric names, process all entries */
	aflag = 1;

    while (optind < argc || aflag) {
	if (aflag) {
	    if (next(&id, &next_type) == 0)
		break;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0 && allindom+allpmid == 0)
		fprintf(stderr, "next_type=%d id=0x%x\n", next_type, id);
#endif
	    if (next_type == 2) {
		if (!allindom)
		    continue;
		printf("\nInDom %s:", pmInDomStr((pmInDom)id));
	    }
	    else {
		char		*p;
		if (!allpmid)
		    continue;

		printf("\nPMID %s", pmIDStr((pmID)id));
		sts = pmNameID(id, &p);
		if (sts == 0) {
		    printf(" %s", p);
		    free(p);
		}
		putchar(':');
	    }
	}
	else {
	    next_type = 1;
	    name = argv[optind++];
	    printf("\nPMID");
	    if ((sts = pmLookupName(1, &name, (pmID *)&id)) < 0) {
		printf(" %s: %s\n", name, pmErrStr(sts));
		continue;
	    }
	    printf(" %s %s:", pmIDStr((pmID)id), name);
	    if (id == PM_ID_NULL) {
		printf(" unknown metric\n");
		continue;
	    }
	}

	if (oneline) {
	    if (next_type == 1)
		tp = pmdaGetHelp(handle, (pmID)id, PM_TEXT_ONELINE);
	    else
		tp = pmdaGetInDomHelp(handle, (pmInDom)id, PM_TEXT_ONELINE);
	    if (tp != NULL)
		printf(" %s\n", tp);
	    else
		putchar('\n');
	}
	else
	    putchar('\n');

	if (help) {
	    if (next_type == 1)
		tp = pmdaGetHelp(handle, (pmID)id, PM_TEXT_HELP);
	    else
		tp = pmdaGetInDomHelp(handle, (pmInDom)id, PM_TEXT_HELP);
	    if (tp != NULL && *tp)
		printf("%s\n", tp);
	}
    }

    return 0;
}
