/*
 * pmstore [-h hostname ] [-i inst[,inst...]] [-n pmnsfile ] metric value
 *
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

#ident "$Id: pmstore.c,v 2.18 1998/11/15 08:35:24 kenmcd Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <values.h>
#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"
#include "pmda.h"

extern int	errno;

#define	IS_UNKNOWN	15
#define	IS_STRING	1
#define IS_INTEGER	2
#define IS_FLOAT	4
#define IS_HEX		8

static void
mkAtom(pmAtomValue *avp, int *nbyte, int type, char *buf)
{
    char	*p = buf;
    char	*endbuf;
    int		vtype = IS_UNKNOWN;
    int		seendot = 0;
    int		ndigit;
    double	d;
    void	*vp;

    if (strncmp(buf, "0x", 2) == 0 || strncmp(buf, "0X", 2) == 0) {
	p += 2;
    }
    else
	vtype &= ~IS_HEX;
	
    while (*p) {
	if (!isdigit(*p)) {
	    vtype &= ~IS_INTEGER;
	    if (*p != 'a' && *p != 'b' && *p != 'c' &&
		*p != 'd' && *p != 'e' && *p != 'f') {
		vtype &= ~IS_HEX;
		if (*p == '.')
		    seendot++;
	    }
	}
	p++;
    }
    if (seendot != 1)
	vtype &= ~IS_FLOAT;

    if (vtype & IS_HEX)
	ndigit = strlen(buf) - 2;

    endbuf = buf;
    switch (type) {
	case PM_TYPE_32:
		if (vtype & IS_HEX) {
		    if (ndigit > 8)
			errno = ERANGE;
		    sscanf(&buf[2], "%lx", &avp->ul);
		    endbuf = "";
		}
		else
		    avp->l = (int)strtol(buf, &endbuf, 10);
		*nbyte = sizeof(avp->l);
		break;

	case PM_TYPE_U32:
		if (vtype & IS_HEX) {
		    if (ndigit > 8)
			errno = ERANGE;
		    sscanf(&buf[2], "%lx", &avp->ul);
		    endbuf = "";
		}
		else
		    avp->ul = (unsigned int)strtoul(buf, &endbuf, 10);
		*nbyte = sizeof(avp->ul);
		break;

	case PM_TYPE_64:
		if (vtype & IS_HEX) {
		    if (ndigit > 16)
			errno = ERANGE;
		    sscanf(&buf[2], "%llx", &avp->ull);
		    endbuf = "";
		}
		else
		    avp->ll = strtoll(buf, &endbuf, 10);
		*nbyte = sizeof(avp->ll);
		break;

	case PM_TYPE_U64:
		if (vtype & IS_HEX) {
		    if (ndigit > 16)
			errno = ERANGE;
		    sscanf(&buf[2], "%llx", &avp->ull);
		    endbuf = "";
		}
		else
		    avp->ull = strtoull(buf, &endbuf, 10);
		*nbyte = sizeof(avp->ull);
		break;

	case PM_TYPE_FLOAT:
		d = strtod(buf, &endbuf);
		if (d < FLT_MIN || d > FLT_MAX)
		    errno = ERANGE;
		else {
		    avp->f = (float)d;
		    *nbyte = sizeof(avp->f);
		}
		break;

	case PM_TYPE_DOUBLE:
		avp->d = strtod(buf, &endbuf);
		*nbyte = sizeof(avp->d);
		break;

	case PM_TYPE_STRING:
		*nbyte = strlen(buf) + 1;
		avp->cp = (char *)malloc(*nbyte);
		if (avp->cp == NULL) {
		    __pmNoMem("pmstore", *nbyte, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		strcpy(avp->cp, buf);
		endbuf = "";
		break;

	case PM_TYPE_AGGREGATE:
		/* assume the worst, and scale back later */
		if ((vtype & IS_HEX) || (vtype & IS_INTEGER))
		    *nbyte = sizeof(__int64_t);
		else if (vtype & IS_FLOAT)
		    *nbyte = sizeof(double);
		else
		    *nbyte = strlen(buf);
		vp = (void *)malloc(*nbyte);
		if (vp == NULL) {
		    __pmNoMem("pmstore", *nbyte, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		if (vtype & IS_HEX) {
		    if (ndigit <= 8) {
			sscanf(&buf[2], "%lx", &avp->ul);
			*nbyte = sizeof(int);
			memcpy(vp, &avp->ul, *nbyte);
		    }
		    else if (ndigit <= 16) {
			sscanf(&buf[2], "%llx", &avp->ull);
			memcpy(vp, &avp->ull, *nbyte);
		    }
		    else
			errno = ERANGE;
		}
		else if (vtype & IS_INTEGER) {
		    avp->l = (int)strtol(buf, &endbuf, 10);
		    if (errno != ERANGE) {
			*nbyte = sizeof(long);
			memcpy(vp, &avp->l, *nbyte);
		    }
		    else {
			avp->ll = strtoll(buf, &endbuf, 10);
			if (errno != ERANGE)
			    memcpy(vp, &avp->ll, *nbyte);
		    }
		}
		else if (vtype & IS_FLOAT) {
		    d = strtod(buf, &endbuf);
		    if (errno != ERANGE) {
			if (FLT_MIN <= d && d <= FLT_MAX) {
			    *nbyte = sizeof(float);
			    avp->f = (float)d;
			    memcpy(vp, &avp->f, *nbyte);
			}
			else
			    memcpy(vp, &d, *nbyte);
		    }
		}
		else
		    strncpy(vp, buf, *nbyte);

		avp->vp = vp;
		endbuf = "";
		break;
    }
    if (*endbuf != '\0') {
	fprintf(stderr, "The value \"%s\" is incompatible with the data type (PM_TYPE_%s)\n",
			buf, pmTypeStr(type));
	exit(1);
    }
    if (errno == ERANGE) {
	fprintf(stderr, "The value \"%s\" is out of range for the data type (PM_TYPE_%s)\n",
			buf, pmTypeStr(type));
	exit(1);
    }
}

int
main(argc, argv)
int argc;
char *argv[];
{
    int		sts;
    int		n;
    int		c;
    int		i;
    int		j;
    char	*p;
    int		type = 0;
    char	*host;
    char	local[MAXHOSTNAMELEN];
    char	*pmnsfile = PM_NS_DEFAULT;
    int		errflag = 0;
    char	*namelist[1];
    pmID	pmidlist[1];
    pmResult	*result;
    char	**instnames = NULL;
    int		numinst = 0;
    pmDesc	desc;
    pmAtomValue	nav;
    int		aggr_len;
    pmValueSet	*vsp;
    char        *subopt;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    __pmSetAuthClient();

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "D:h:i:n:?")) != EOF) {
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

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'i':	/* list of instances */
#define WHITESPACE ", \t\n"
	    subopt = strtok(optarg, WHITESPACE);
	    while (subopt != NULL) {
		numinst++;
		instnames =
		    (char **)realloc(instnames, numinst * sizeof(char *));
		if (instnames == NULL) {
		    __pmNoMem("pmstore.instnames", numinst * sizeof(char *), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
	       instnames[numinst-1] = subopt;
	       subopt = strtok(NULL, WHITESPACE);
	    }
	    break;

	case 'n':	/* alternative namespace file */
	    pmnsfile = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc - 2) {
	fprintf(stderr,
"Usage: %s [options] metricname value\n"
"\n"
"Options:\n"
"  -h host       metrics source is PMCD on host\n"
"  -i instance   metric instance or list of instances. Elements in an\n"
"                instance list are separated by commas and/or newlines\n" 
"  -n pmnsfile   use an alternative PMNS\n",
			pmProgname);
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((n = pmLoadNameSpace(pmnsfile)) < 0) {
	    fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(n));
	    exit(1);
	}
    }

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
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    namelist[0] = argv[optind++];
    if ((n = pmLookupName(1, namelist, pmidlist)) < 0) {
	printf("%s: pmLookupName: %s\n", namelist[0], pmErrStr(n));
	exit(1);
    }
    if (pmidlist[0] == PM_ID_NULL) {
	printf("%s: unknown metric\n", namelist[0]);
	exit(1);
    }
    if ((n = pmLookupDesc(pmidlist[0], &desc)) < 0) {
	printf("%s: pmLookupDesc: %s\n", namelist[0], pmErrStr(n));
	exit(1);
    }
    if (instnames != NULL) {
	pmDelProfile(desc.indom, 0, NULL);
	for (i = 0; i < numinst; i++) {
	    if ((n = pmLookupInDom(desc.indom, instnames[i])) < 0) {
		printf("pmLookupInDom %s[%s]: %s\n",
		    namelist[0], instnames[i], pmErrStr(n));
		exit(1);
	    }
	    if ((sts = pmAddProfile(desc.indom, 1, &n)) < 0) {
		printf("pmAddProfile %s[%s]: %s\n",
		    namelist[0], instnames[i], pmErrStr(sts));
		exit(1);
	    }
	}
    }
    if ((n = pmFetch(1, pmidlist, &result)) < 0) {
	printf("%s: pmFetch: %s\n", namelist[0], pmErrStr(n));
	exit(1);
    }

    /* value is argv[optind] */
    mkAtom(&nav, &aggr_len, desc.type, argv[optind]);

    vsp = result->vset[0];
    if (vsp->numval == 0) {
	printf("%s: No value(s) available!\n", namelist[0]);
	exit(1);
    }
    else if (vsp->numval < 0) {
	printf("%s: Error: %s\n", namelist[0], pmErrStr(vsp->numval));
	exit(1);
    }
    for (j = 0; j < vsp->numval; j++) {
	pmValue	*vp = &vsp->vlist[j];
	printf("%s", namelist[0]);
	if (desc.indom != PM_INDOM_NULL) {
	    if ((n = pmNameInDom(desc.indom, vp->inst, &p)) < 0)
		printf(" inst [%d]", vp->inst);
	    else {
		printf(" inst [%d or \"%s\"]", vp->inst, p);
		free(p);
	    }
	}
	printf(" old value=");
	pmPrintValue(stdout, vsp->valfmt, desc.type, vp, 1);
	vsp->valfmt = __pmStuffValue(&nav, aggr_len, &vsp->vlist[j], desc.type);
	printf(" new value=");
	pmPrintValue(stdout, vsp->valfmt, desc.type, vp, 1);
	putchar('\n');
    }
    if ((n = pmStore(result)) < 0) {
	printf("%s: pmStore: %s\n", namelist[0], pmErrStr(n));
	exit(1);
    }
    pmFreeResult(result);

    exit(0);
    /* NOTREACHED */
}
