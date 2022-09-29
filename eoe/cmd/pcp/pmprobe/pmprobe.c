/*
 * pmprobe - light-weight pminfo for configuring monitor apps
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

#ident "$Id: pmprobe.c,v 1.24 1999/05/11 19:25:55 kenmcd Exp $"

#include <unistd.h>
#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"

static char	**namelist;
static pmID	*pmidlist;
static int	listsize;
static int	numpmid;
static int	numinst = -1;
static int	*instlist;
static char	**instnamelist;

static void
dometric(const char *name)
{
    if (numpmid >= listsize) {
	listsize = listsize == 0 ? 16 : listsize * 2;
	if ((pmidlist = (pmID *)realloc(pmidlist, listsize * sizeof(pmidlist[0]))) == NULL) {
	    __pmNoMem("realloc pmidlist", listsize * sizeof(pmidlist[0]), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	if ((namelist = (char **)realloc(namelist, listsize * sizeof(namelist[0]))) == NULL) {
	    __pmNoMem("realloc namelist", listsize * sizeof(namelist[0]), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
    }

    namelist[numpmid]= strdup(name);
    if (namelist[numpmid] == NULL) {
	__pmNoMem("strdup name", strlen(name), PM_FATAL_ERR);
	/*NOTREACHED*/
    }

    numpmid++;
}

static void
lookup(pmInDom indom)
{
    static pmInDom	last = PM_INDOM_NULL;

    if (indom != last) {
	if (numinst > 0) {
	    free(instlist);
	    free(instnamelist);
	}
	numinst = pmGetInDom(indom, &instlist, &instnamelist);
	last = indom;
    }
}

extern int optind;

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		fetch_sts;
    int		iflag = 0;			/* -i for instance numbers */
    int		Iflag = 0;			/* -I for instance names */
    int		vflag = 0;			/* -v for values */
    int		Vflag = 0;			/* -V for verbose */
    char	*p;
    int		errflag = 0;
    int		type = 0;
    char	*host;
    pmLogLabel	label;				/* get hostname for archives */
    char	local[MAXHOSTNAMELEN];
    char	*pmnsfile = PM_NS_DEFAULT;
    int		i;
    int		j;
    pmResult	*result;
    pmValueSet	*vsp;
    pmDesc	desc;
#ifdef PM_USE_CONTEXT_LOCAL
    char        *opts = "a:D:h:IiLn:Vv?";
#else
    char        *opts = "a:D:h:Iin:Vv?";
#endif
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    __pmSetAuthClient();

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, opts)) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
#ifdef PM_USE_CONTEXT_LOCAL
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

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

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
#ifdef PM_USE_CONTEXT_LOCAL
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'i':	/* report internal instance numbers */
	    if (vflag) {
		fprintf(stderr, "%s: at most one of -i and -v allowed\n", pmProgname);
		errflag++;
	    }
	    iflag++;
	    break;

	case 'I':	/* report external instance names */
	    if (vflag) {
		fprintf(stderr, "%s: at most one of -I and -v allowed\n", pmProgname);
		errflag++;
	    }
	    Iflag++;
	    break;

#ifdef PM_USE_CONTEXT_LOCAL
	case 'L':	/* LOCAL, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    break;
#endif

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'V':	/* verbose */
	    Vflag++;
	    break;

	case 'v':	/* cheap values */
	    if (iflag || Iflag) {
		fprintf(stderr, "%s: at most one of -v and (-i or -I) allowed\n", pmProgname);
		errflag++;
	    }
	    vflag++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] [metricname ...]\n\
\n\
Options:\n\
  -a archive   metrics source is a PCP log archive\n\
  -h host      metrics source is PMCD on host\n\
  -I           list external instance names\n\
  -i           list internal instance numbers\n"
#ifdef PM_USE_CONTEXT_LOCAL
"  -L           use local context instead of PMCD\n"
#endif
"  -n pmnsfile  use an alternative PMNS\n\
  -V           report PDU operations (verbose)\n\
  -v           list metric values\n",
		pmProgname);
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

#ifdef MALLOC_AUDIT
    _malloc_reset_();
    atexit(_malloc_audit_);
#endif

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	(void)gethostname(local, MAXHOSTNAMELEN);
	local[MAXHOSTNAMELEN-1] = '\0';
	host = local;
    }
    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
#ifdef PM_USE_CONTEXT_LOCAL
	else if (type == PM_CONTEXT_LOCAL)
	    fprintf(stderr, "%s: Cannot make standalone connection on localhost: %s\n",
		    pmProgname, pmErrStr(sts));
#endif
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }

    if (optind >= argc) {
	pmTraversePMNS("", dometric);
    }
    else {
	int	a;
	for (a = optind; a < argc; a++) {
	    sts = pmTraversePMNS(argv[a], dometric);
	    if (sts < 0)
		printf("%s %d %s\n", argv[a], sts, pmErrStr(sts));
	}
    }

    /* Lookup names.
     * Cull out names that were unsuccessfully looked up.
     * However, it is unlikely to fail because names come from a traverse PMNS.
     */
    if ((sts = pmLookupName(numpmid, namelist, pmidlist)) < 0) {
        int i, j = 0;
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL) {
		printf("%s %d %s\n", namelist[i], sts, pmErrStr(sts));
		free(namelist[i]);
	    }
	    else {
		/* assert(j <= i); */
		pmidlist[j] = pmidlist[i];
		namelist[j] = namelist[i];
		j++;
	    }	
	}
	numpmid = j;
    }

    if (type != PM_CONTEXT_ARCHIVE)
	/*
	 * merics from archives are fetched one at a time, otherwise
	 * get them all at once
	 */
	fetch_sts = pmFetch(numpmid, pmidlist, &result);

    for (i = 0; i < numpmid; i++) {
	if (type == PM_CONTEXT_ARCHIVE) {
	    if ((sts = pmSetMode(PM_MODE_FORW, &label.ll_start, 0)) < 0) {
		fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
		exit(1);
	    }
	    fetch_sts = pmFetch(1, &pmidlist[i], &result);
	}

	printf("%s ", namelist[i]);
	if (fetch_sts < 0) {
	    printf("%d %s", fetch_sts, pmErrStr(fetch_sts));
	}
	else {
	    if (type == PM_CONTEXT_ARCHIVE)
	    	vsp = result->vset[0];
	    else
	    	vsp = result->vset[i];
	    printf("%d", vsp->numval);

	    if (vsp->numval < 0) {
		printf(" %s\n", pmErrStr(vsp->numval));
		continue;
	    }
	    else if (vsp->numval == 0) {
		putchar('\n');
		continue;
	    }
	    else if (vsp->numval > 0) {
		if (iflag || Iflag) {
		    if (pmLookupDesc(pmidlist[i], &desc) < 0) {
			/* report numeric instances from pmResult */
			for (j = 0; j < vsp->numval; j++) {
			    pmValue	*vp = &vsp->vlist[j];
			    if (vp->inst == PM_IN_NULL) {
				if (iflag)
				    printf(" PM_IN_NULL");
				if (Iflag)
				    printf(" PM_IN_NULL");
			    }
			    else {
				if (iflag)
				    printf(" ?%d", vp->inst);
				if (Iflag)
				    printf(" ?%d", vp->inst);
			    }
			}
		    }
		    else if (desc.indom == PM_INDOM_NULL) {
			if (iflag)
			    printf(" PM_IN_NULL");
			if (Iflag)
			    printf(" PM_IN_NULL");
		    }
		    else {
			lookup(desc.indom);
			for (j = 0; j < vsp->numval; j++) {
			    int		k;
			    pmValue	*vp = &vsp->vlist[j];
			    if (iflag)
				printf(" ?%d", vp->inst);
			    for (k = 0; k < numinst; k++) {
				if (instlist[k] == vp->inst)
				    break;
			    }
			    if (Iflag) {
				if (k < numinst)
				    printf(" \"%s\"", instnamelist[k]);
				else
				    printf(" ?%d", vp->inst);
			    }
			}
		    }
		}
		else if (vflag) {
		    /* really need to get pmDesc */
		    sts = pmLookupDesc(pmidlist[i], &desc);
		    for (j = 0; j < vsp->numval; j++) {
			pmValue	*vp = &vsp->vlist[j];
			putchar(' ');
			pmPrintValue(stdout, vsp->valfmt, desc.type, vp, 1);
		    }
		}
	    }
	}
	putchar('\n');
    }

    if (Vflag) {
	extern unsigned int	__pmPDUCntIn[];
	extern unsigned int	__pmPDUCntOut[];

	/* Warning: 16 is magic and from libpcp/src/pdu.c */
	printf("PDUs send");
	j = 0;
	for (i = 0; i < 16; i++) {
	    printf(" %3d", __pmPDUCntOut[i]);
	    j += __pmPDUCntOut[i];
	}
	printf("\nTotal: %d\n", j);

	printf("PDUs recv");
	j = 0;
	for (i = 0; i < 16; i++) {
	    printf(" %3d",__pmPDUCntIn[i]);
	    j += __pmPDUCntIn[i];
	}
	printf("\nTotal: %d\n", j);
    }

    exit(0);
    /*NOTREACHED*/
}
