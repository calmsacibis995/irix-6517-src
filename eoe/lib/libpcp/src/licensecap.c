/*
 * pmGetLicenseCap - fastpath to get license capabilities
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

#ident "$Id: licensecap.c,v 2.15 1999/02/12 04:49:46 kenmcd Exp $"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"

#define BRAND	"/var/pcp/pmns/Brand"

#if defined(BUILDTOOL)

int
__pmGetLicenseCap(void)
{
    /* for building, turn all of the bits on, please */
    return ~0;
}

#elif defined(HAVE_FLEXLM)

#include <sys/syssgi.h>

static char	*license[] = { "/var/flexlm/license.dat" };
static int	numlic = sizeof(license)/sizeof(license[0]);

/*
 * global to make sure \0 is the default initial value, like in
 * pmbrand(1)
 */
static char	identbrand[MAXSYSIDSIZE];

int
__pmGetLicenseCap(void)
{
    int		bits;
    char	ident[MAXSYSIDSIZE];
    FILE	*f = NULL;
    __int32_t	sum;
    int		i;
    int		l;
    int		try;
    char	*reason = "?";
    struct stat	bstat;
    struct stat lstat;

    for (try = 0; try < 2; try++) {
	if (stat(BRAND, &bstat) < 0) {
	    reason = "stat(brand)";
	    goto fail;
	}

	/*
	 * out of date checks depend on which flavour of licenses
	 * we have this week
	 */
	for (l = 0; l < numlic; l++) {
	    if (stat(license[l], &lstat) < 0)
		continue;
	    if (bstat.st_mtime < lstat.st_mtime) {
		/* license newer than brand, out of date */
		reason = "out of date";
		goto fail;
	    }
	}

	if ((f = fopen(BRAND, "r")) == NULL) {
	    reason = "no brand";
	    goto fail;
	}

	if (fread(&sum, sizeof(sum), 1, f) != 1) {
	    reason = "read(sum)";
	    goto fail;
	}

	if (sum != __pmCheckSum(f)) {
	    reason = "checksum";
	    goto fail;
	}

	fseek(f, (long)(sizeof(sum)), SEEK_SET);
	if (fread(&bits, sizeof(bits), 1, f) != 1) {
	    reason = "read(bits)";
	    goto fail;
	}

	syssgi(SGI_SYSID, identbrand);
	if (fread(ident, MAXSYSIDSIZE, 1, f) != 1) {
	    reason = "syssgi";
	    goto fail;
	}

	for (i = 0; i < MAXSYSIDSIZE; i++)
	    if (identbrand[i] != ident[i]) {
		reason = "sysid";
		goto fail;
	    }

	fclose(f);
	return bits;

fail:
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS)
	    fprintf(stderr, "__pmGetLicenseCap: try=%d reason=%s\n", try, reason);
#endif
	if (f != NULL)
	    fclose(f);
	if (try == 0)
	    system("/usr/pcp/bin/pmbrand");
    }
    return 0;
}

#else

int
__pmGetLicenseCap(void)
{
    return ~0;
}

#endif
