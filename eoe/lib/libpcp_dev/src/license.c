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
 *
 *      Programs linked with this module must also be linked
 *      with liblmsgi ... this is hidden in the linking of libpcp_dev.a
 *
 *      Compiling this file requires the "-dollar" flag and the flag
 *      -I/usr/include/idl/c to the C compiler.
 */

#ident "$Id: license.c,v 1.20 1999/08/18 15:49:07 kenmcd Exp $"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include "pmapi.h"
#include "impl.h"
#include "pmapi_dev.h"

#if !defined(HAVE_FLEXLM)
int
__pmGetLicense(int request, const char *prog, int showme)
{
    /* TODO - all licenses available for now */
    return request;
}
#else

#include <lmsgi.h>

LM_CODE(code, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2,
	VENDOR_KEY1, VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);

/*
 * Other FLEXlm constants
 */

static char* flexServer = "sgifd";

/* license types */
#define	T_DEF		0
#define T_NETLS		1
#define T_FLEXLM	2

struct {
    int		type;
    int		productID;
    char	*productName;
    char	*version;
    int		bits;
    int		checked_out;
} lic_tab[] = {
    { T_FLEXLM,	0,	"pcpmon",	"1.0",	PM_LIC_PCP | PM_LIC_MON, 0 },
    { T_FLEXLM,	0,	"pcpcol",	"1.0",	PM_LIC_PCP | PM_LIC_COL, 0 },
    { T_FLEXLM, 0,	"webmeter",	"1.0",	PM_LIC_WEB | PM_LIC_COL | PM_LIC_MON, 0 },
};

int	nnlt = sizeof(lic_tab)/sizeof(lic_tab[0]);

static const unsigned int warnOfExpiration = 30;	/* licensed days left */

/*
 * Get a license or bust
 */

int
__pmGetLicense(int request, const char *prog, int showme)
{
    long		status;
    time_t		flexExpDate;
    int			haveFLEXlm = 0;
    unsigned int	remains = UINT_MAX;
				/* days to go before license expires */
    unsigned int	check;
    int			i;
    struct stat		sbuf;
    int			result = 0;
    struct timeval	now;
    SIG_PF		onpipe;
    SIG_PF		onalrm;
    char		*lf = NULL;

    /*
     * Strange but true, FLEXlm libraries install signal handlers for
     * SIGPIPE and SIGALRM and do not cleanup ...
     * remember disposition on entry
     */
    onpipe = signal(SIGPIPE, SIG_DFL);
    onalrm = signal(SIGALRM, SIG_DFL);

    if (request == 0)
	/* strange sort of no-op */
	goto done;

    /* Initialize FLEXlm */
    status = license_init(&code, flexServer, B_TRUE);
    if (status >= 0)
	haveFLEXlm = 1;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "__pmGetLicense: want: 0x%x license_init() -> %ld (%s)\n",
		request, status, status >= 0 ? "OK" : "FAILED");
	lf = getenv("LM_LICENSE_FILE");
	if (lf == NULL)
	    fprintf(stderr, "__pmGetLicense: $LM_LICENSE_FILE is not set\n");
	else
	    fprintf(stderr, "__pmGetLicense: $LM_LICENSE_FILE=\"%s\"\n", lf);

	fprintf(stderr, "__pmGetLicense: ls -l /var/flexlm/license.dat ...\n");
	system("ls -l /var/flexlm/license.dat 2>&1");
    }
#endif

    /*
     * Ensure that we can talk to one of the servers
     */
    if (haveFLEXlm == 0) {
	if (showme & (GET_LICENSE_SHOW_NOLIC | GET_LICENSE_DIE)) {
	    pmprintf("%s: Error: Could not initialize FLEXlm, license_init() -> %ld\n", prog, status);
	    lf = getenv("LM_LICENSE_FILE");
	    if (lf != NULL)
		pmprintf("Hint: $LM_LICENSE_FILE=\"%s\"\n", lf);
	    if (stat("/var/flexlm/license.dat", &sbuf) < 0)
		pmprintf("Hint: Cannot stat /var/flexlm/license.dat\n");
	    pmflush();
	}
	if (showme & GET_LICENSE_DIE)
	    exit(1);

	/* caller handles this; e.g. pmcd continues unlicensed */
	goto done;
    }

    /* Turn off the default messages */
    license_set_attr(LMSGI_NO_SUCH_FEATURE, NULL);
    license_set_attr(LMSGI_30_DAY_WARNING, NULL);
    license_set_attr(LMSGI_60_DAY_WARNING, NULL);
    license_set_attr(LMSGI_90_DAY_WARNING, NULL);

    gettimeofday(&now, NULL);

    /* Get a nodelocked license */
    for (i = 0; i < nnlt; i++) {
	if ((lic_tab[i].bits & request) == 0) {
	    /* none of the bits we want are enabled by this license */
	    continue;
	}

	switch (lic_tab[i].type) {

	case T_DEF:
	    result |= (lic_tab[i].bits & request);
	    break;

	case T_FLEXLM:
	    if ((status = license_chk_out(&code, lic_tab[i].productName, lic_tab[i].version)) == 0) {
		/*
		 * if we get this far, we have a valid license for
		 * lic_tab[i].productName
		 */
		lic_tab[i].checked_out = 1;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_CONTEXT)
		    fprintf(stderr, "__pmGetLicense: license_chk_out(... %s, %s) OK",
			    lic_tab[i].productName, lic_tab[i].version);
#endif
		flexExpDate = license_expdate(lic_tab[i].productName);
		if (flexExpDate) {
		    /*
		     * I think this means it is a temporary license
		     */
		    check = (int)((flexExpDate - now.tv_sec) / (60 * 60 * 24));
		    if (check < remains)
			remains = check;
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_CONTEXT)
			fprintf(stderr, " (exp in %d days)", check);
#endif
		}
#ifdef PCP_DEBUG
		else {
		    if (pmDebug & DBG_TRACE_CONTEXT)
			fprintf(stderr, " (permanent)");
		}
#endif
		result |= (lic_tab[i].bits & request);
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_CONTEXT)
		    fprintf(stderr, " result: 0x%x\n", result);
#endif
	    }
#ifdef PCP_DEBUG
	    else {
		if (pmDebug & DBG_TRACE_CONTEXT)
		    fprintf(stderr, "__pmGetLicense: license_chk_out(... %s, %s) -> %d (FAILED)\n",
			    lic_tab[i].productName, lic_tab[i].version, (int)status);
	    }
#endif
	    break;
	}
    }

    if ((result & request) != request) {
	if (showme & (GET_LICENSE_SHOW_NOLIC | GET_LICENSE_DIE)) {
	    pmprintf("Error: %s: Could not find a valid Performance Co-Pilot license\n", prog);
	    pmflush();
	}
	if (showme & GET_LICENSE_DIE) {
	    license_free();
	    exit(1);
	}
    }
    else if (remains <= warnOfExpiration) {
	if (showme & GET_LICENSE_SHOW_EXP) {
	    /* Warn if expiration date is approaching */
	    if ((getenv("PCP_LICENCE_NOWARNING") == NULL) &&
		(getenv("PCP_LICENSE_NOWARNING") == NULL)) {
		pmprintf("Warning: %s: Performance Co-Pilot license expires ", prog);
		switch (remains) {
		case 0:
		    pmprintf("today\n");
		    break;
		case 1:
		    pmprintf("tomorrow\n");
		    break;
		default:
		    pmprintf("in %d days\n", remains);
		}
		pmflush();
	    }
	}
    }

done:
    if (haveFLEXlm) {
	/*
	 * check in any licenses just checked out
	 */
	for (i = 0; i < nnlt; i++) {
	    if (lic_tab[i].checked_out) {
		lic_tab[i].checked_out = 0;
		status = license_chk_in(lic_tab[i].productName, 1);
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_CONTEXT) {
		    fprintf(stderr, "__pmGetLicense: license_chk_in(%s, 1) -> %d (%s)\n",
			lic_tab[i].productName, (int)status, status >= 0 ? "OK" : "FAILED");
		}
#endif
	    }
	}
	license_free();
    }

    /*
     * Strange but true, FLEXlm libraries install signal handlers for
     * SIGPIPE and SIGALRM and do not cleanup ...
     * restore disposition at entry
     */
    if (onpipe != SIG_ERR) signal(SIGPIPE, onpipe);
    if (onalrm != SIG_ERR) signal(SIGALRM, onalrm);

    return result;
}

#endif /* HAVE_FLEXLM */

