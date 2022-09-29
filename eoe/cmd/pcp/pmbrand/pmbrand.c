/*
 * pmbrand - build licence capabilities file for PMNS personalities
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

#ident "$Id: pmbrand.c,v 2.22 1997/10/07 02:39:57 markgw Exp $"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syssgi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "pmapi_dev.h"

#define BRAND	"/var/pcp/pmns/Brand"
#define BRANDNEW	"/var/pcp/pmns/Brand.XXXXXX"

static int	_bits;
static char	_ident[MAXSYSIDSIZE];
static char	*_brandnew;

static void
_check(void)
{
    FILE	*f;
    __int32_t	sum;
    int		bits;
    char	ident[MAXSYSIDSIZE];
    int		sts;
    int		i;

    if ((f = fopen(BRAND, "r")) == NULL) {
	fprintf(stderr, "%s: cannot open \"%s\": %s\n",
	    pmProgname, BRAND, strerror(errno));
	exit(1);
    }
    if ((sts = (int)fread(&sum, sizeof(sum), 1, f)) != 1) {
	if (sts < 0)
	    fprintf(stderr, "%s: fread chksum: %s\n", pmProgname, strerror(errno));
	else
	    fprintf(stderr, "%s: fread chksum: short read\n", pmProgname);
	exit(2);
    }
    if (sum != __pmCheckSum(f))
	exit(3);	/* checksum failure */
    fseek(f, (long)(sizeof(sum)), SEEK_SET);
    if ((sts = (int)fread(&bits, sizeof(bits), 1, f)) != 1) {
	if (sts < 0)
	    fprintf(stderr, "%s: fread capabilities: %s\n", pmProgname, strerror(errno));
	else
	    fprintf(stderr, "%s: fread capabilities: short read\n", pmProgname);
	unlink(_brandnew);
	exit(2);
    }
    if (bits != _bits)
	exit(4);	/* licence capabilities mismatch */
    if ((sts = (int)fread(ident, MAXSYSIDSIZE, 1, f)) != 1) {
	if (sts < 0)
	    fprintf(stderr, "%s: fread sysid: %s\n", pmProgname, strerror(errno));
	else
	    fprintf(stderr, "%s: fread sysid: short read\n", pmProgname);
	exit(2);
    }
    for (i = 0; i < MAXSYSIDSIZE; i++) {
	if (_ident[i] != ident[i])
	    exit(5);	/* sysid mismatch */
    }

    exit(0);
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char	*p;
    int		errflag = 0;
    int		list = 0;
    int		remake = 1;
    int		vflag = 0;
#if QA
    int         xbits = 0;
#endif
    __int32_t	sum;
    FILE	*f;
    extern char	*optarg;
    extern int	optind;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

#if QA
#define OPTSPEC "D:lqvx:?"
#else
#define OPTSPEC "D:lqv?"
#endif

    while ((c = getopt(argc, argv, OPTSPEC)) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		exit(1);
		/*NOTREACHED*/
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'l':	/* list */
	    list++;
	    break;

	case 'q':	/* query */
	    list++;
	    remake = 0;
	    break;

	case 'v':	/* verify */
	    if (list) {
		errflag++;
		fprintf(stderr, "%s: -v may not be used with any other options\n",
		    pmProgname);
	    }
	    else
		vflag++;
	    break;

#if QA
	case 'x':	/* pretend, not documented! */
	    {
		char *endnum;
		_bits = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "%s: -x requires numeric argument\n", pmProgname);
		    exit(1);
		    /*NOTREACHED*/
		}
		xbits = 1;
	    }
	    break;
#endif

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s [-lqv]\n", pmProgname);
	exit(1);
    }

    syssgi(SGI_SYSID, _ident);
#if QA
    if (xbits == 0)
#endif
	_bits = __pmGetLicense(~0, pmProgname, GET_LICENSE_SILENT);

    if (vflag) {
	_check();
	/*NOTREACHED*/
    }

    if (remake) {
	if ((_brandnew = mktemp(BRANDNEW)) == NULL) {
	    fprintf(stderr, "%s: mktemp(%s): %s\n", pmProgname, BRANDNEW, strerror(errno));
	    exit(1);
	}

	umask(022);

	if ((f = fopen(_brandnew, "w+")) == NULL) {
	    fprintf(stderr, "%s: cannot create \"%s\": %s\n",
		pmProgname, _brandnew, strerror(errno));
	    exit(1);
	}
    }

    if (list) {
	int	i;
	fprintf(stderr, "Licenses for system ");
	for (i = 0; i < MAXSYSIDSIZE; i++)
	    fprintf(stderr, "%02x", _ident[i]);
	fputc('\n', stderr);
	if (_bits & PM_LIC_PCP) {
	    if (_bits & PM_LIC_COL)
		fprintf(stderr, "  PCP Collector\n");
	    if (_bits & PM_LIC_MON)
		fprintf(stderr, "  PCP Monitor\n");
	}
	if (_bits & PM_LIC_WEB) {
	    if (_bits & PM_LIC_COL)
		fprintf(stderr, "  WebMeter Collector\n");
	    if (_bits & PM_LIC_MON)
		fprintf(stderr, "  WebMeter Monitor\n");
	}
    }

    if (remake == 0)
	exit(0);

    sum = 0;
    if (fwrite(&sum, sizeof(sum), 1, f) != 1) {
	fprintf(stderr, "%s: fwrite: %s\n", pmProgname, strerror(errno));
	unlink(_brandnew);
	exit(1);
    }
    if (fwrite(&_bits, sizeof(_bits), 1, f) != 1) {
	fprintf(stderr, "%s: fwrite: %s\n", pmProgname, strerror(errno));
	unlink(_brandnew);
	exit(1);
    }
    if (fwrite(_ident, MAXSYSIDSIZE, 1, f) != 1) {
	fprintf(stderr, "%s: fwrite: %s\n", pmProgname, strerror(errno));
	unlink(_brandnew);
	exit(1);
    }
    fseek(f, (long)(sizeof(sum)), SEEK_SET);
    sum = __pmCheckSum(f);
    fseek(f, 0L, SEEK_SET);
    if (fwrite(&sum, sizeof(sum), 1, f) != 1) {
	fprintf(stderr, "%s: fwrite: %s\n", pmProgname, strerror(errno));
	unlink(_brandnew);
	exit(1);
    }

    unlink(BRAND);
    if (rename(_brandnew, BRAND) < 0) {
	fprintf(stderr, "%s: rename(%s, %s): %s\n",
	    pmProgname, _brandnew, BRAND, strerror(errno));
	exit(1);
    }
    exit(0);
    /* NOTREACHED */
}
