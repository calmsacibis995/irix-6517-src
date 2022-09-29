/*
 * chkhelp helpfile metric-name ...
 *
 * check's ndbm help files build by newhelp
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

#ident "$Id: chkhelp_v1.c,v 1.5 1999/02/07 20:20:57 kenmcd Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <ndbm.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

extern void pass0(DBM *);
extern int pass1(int *, int *);

static int	version = 1;

int
main(argc, argv)
int argc;
char *argv[];
{
    int		sts;
    int		c;
    int		help = 0;
    int		oneline = 0;
    char	*namespace = PM_NS_DEFAULT;
    char	*p;
    int		errflag = 0;
    int		aflag = 0;
    int		allpmid = 0;
    int		allindom = 0;
    char	*endnum;
    char	*filename;
    DBM		*dbf;
    char	*tp;
    char	*namelist[1];
    int		idlist[1];
    int		type;
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

#ifdef PCP_DEBUG
	case 'D':	/* debug flag */
	    pmDebug = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -D requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;
#endif

	case 'H':	/* help text */
	    help = 1;
	    break;
	
	case 'i':
	    aflag++;
	    allindom = 1;
	    break;

	case 'n':	/* alternative namespace file */
	    namespace = optarg;
	    break;

	case 'O':	/* oneline text */
	    oneline = 1;
	    break;

	case 'p':
	    aflag++;
	    allpmid = 1;
	    break;

	case 'v':	/* version 1 only! */
	    version = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -v requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    if (version != 1) {
		fprintf(stderr, "%s: version must be 1\n", pmProgname);
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
       %s [options] helpfile [metricname ..]\n\
\n\
Options\n\
  -H		  display verbose help text\n\
  -i		  process all the instance domains\n\
  -n namespace    use an alternative PMNS\n\
  -O 		  display the one line help summary\n\
  -p		  process all the metrics (PMIDs)\n\
  -v version      version, must be 1\n\
\n\
No options implies silently check internal integrity of the helpfile.\n",
		pmProgname, pmProgname);
	exit(1);
    }

    filename = argv[optind++];
    if ((dbf = dbm_open(filename, O_RDONLY, 0)) == NULL) {
	fprintf(stderr, "dbm_open for %s.*: %s\n", filename, strerror(errno));
	exit(1);
    }

    if ((sts = pmLoadNameSpace(namespace)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(sts));
	exit(1);
    }

    if (help + oneline == 0 && (optind < argc || aflag))
	/* if metric names, -p or -i => -O is default */
	oneline = 1;

    if (optind == argc && aflag == 0)
	/* no metric names, process all entries */
	aflag = 1;

    if (aflag)
	pass0(dbf);

    while (optind < argc || aflag) {
	if (aflag) {
	    if (pass1(idlist, &type) == -1)
		break;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0 && allindom+allpmid == 0)
		fprintf(stderr, "type=%d id=0x%x\n", type, idlist[0]);
#endif
	    if (type == PM_TEXT_INDOM) {
		if (!allindom)
		    continue;
		printf("\nInDom %s:", pmInDomStr((pmInDom)idlist[0]));
	    }
	    else {
		char		*p;
		if (!allpmid)
		    continue;

		printf("\nPMID %s", pmIDStr((pmID)idlist[0]));
		sts = pmNameID(idlist[0], &p);
		if (sts == 0) {
		    printf(" %s", p);
		    free(p);
		}
		putchar(':');
	    }
	}
	else {
	    type = PM_TEXT_PMID;
	    namelist[0] = argv[optind++];
	    printf("\nPMID");
	    if ((sts = pmLookupName(1, namelist, (pmID *)idlist)) < 0) {
		printf(" %s: %s\n", namelist[0], pmErrStr(sts));
		continue;
	    }
	    printf(" %s %s:", pmIDStr((pmID)idlist[0]), namelist[0]);
	    if (idlist[0] == PM_ID_NULL) {
		printf(" unknown metric\n");
		continue;
	    }
	}
	if (oneline) {
	    tp = _pmGetText(dbf, idlist[0], type | PM_TEXT_ONELINE);
	    if (tp != NULL) {
		printf(" %s\n", tp);
		free(tp);
	    }
	    else
		putchar('\n');
	}
	else
	    putchar('\n');
	if (help) {
	    tp = _pmGetText(dbf, idlist[0], type | PM_TEXT_HELP);
	    if (tp != NULL) {
		printf("%s", tp);
		free(tp);
	    }
	}
    }

    exit(0);
    /*NOTREACHED*/
}
