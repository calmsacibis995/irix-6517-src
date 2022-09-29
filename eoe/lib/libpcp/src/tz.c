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

#ident "$Id: tz.c,v 2.13 1999/05/11 00:28:03 kenmcd Exp $"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#if defined(sgi)
#define _SGI_REENTRANT_FUNCTIONS
#endif
#include <time.h>

#include "pmapi.h"
#include "impl.h"

static char	*envtz = NULL;		/* buffer in env */
static int	envtzlen = 0;

static char	*savetz = NULL;		/* real $TZ from env */
static char	**savetzp;

static int	nzone;				/* table of zones */
static int	curzone = -1;
static char	**zone = NULL;

#if !defined(HAVE_UNDERBAR_ENVIRON)
#define _environ environ
#endif

extern char **_environ;

static void
_pushTZ(void)
{
    char	**p;

    savetzp = NULL;
    for (p = _environ; *p != NULL; p++) {
	if (strncmp(*p, "TZ=", 3) == 0) {
	    savetz = *p;
	    *p = envtz;
	    savetzp = p;
	    break;
	}
    }
    if (*p == NULL)
	putenv(envtz);
    tzset();
}

static void
_popTZ(void)
{
    if (savetzp != NULL)
	*savetzp = savetz;
    else
	putenv("TZ=");
    tzset();
}

/*
 * __pmTimezone: work out local timezone
 *
 *               NOTE that this is an optional symbol for IRIX6_5
 */
char *
__pmTimezone(void)
{
    static char tzbuffer[80] = "";
    char envbuffer[83] = "";
    char *tz = NULL;

    tz = getenv("TZ");
    if (tz == NULL) {
        tzset();

	if(timezone != 0) {
            if(strlen(tzname[daylight]) > 70)
                return NULL;
            sprintf(tzbuffer, "%s%+d", tzname[daylight], timezone / 3600);
            tz = tzbuffer;
        }
        else {
            tz = tzname[daylight];
        }
        sprintf(envbuffer, "TZ=%s", tz);
        putenv(envbuffer);
    }

    return tz;
}
#if defined(IRIX6_5) 
#pragma optional __pmTimezone
#endif


int
pmUseZone(const int tz_handle)
{
    if (tz_handle < 0 || tz_handle >= nzone)
	return -1;
    
    curzone = tz_handle;
    strcpy(&envtz[3], zone[curzone]);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmUseZone(%d) tz=%s\n", curzone, zone[curzone]);
#endif

    return 0;
}

int
pmNewZone(const char *tz)
{
    extern int	errno;
    int		len;
    int		hack = 0;

    len = (int)strlen(tz);
    if (len == 3) {
	/*
	 * things like TZ=GMT may be broken in libc, particularly
	 * in _ltzset() of time_comm.c, where changes to TZ are
	 * sometimes not properly reflected.
	 * TZ=GMT+0 avoids the problem.
	 */
	len += 2;
	hack = 1;
    }

    if (len+4 > envtzlen) {
	/* expand buffer for env */
	if (envtz != NULL)
	    free(envtz);
	envtzlen = len+4;
	envtz = (char *)malloc(envtzlen);
	strcpy(envtz, "TZ=");
    }
    strcpy(&envtz[3], tz);
    if (hack)
	/* see above */
	strcpy(&envtz[6], "+0");

    curzone = nzone++;
    zone = (char **)realloc(zone, nzone * sizeof(char *));
    if (zone == NULL) {
	__pmNoMem("pmNewZone", nzone * sizeof(char *), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    zone[curzone] = strdup(&envtz[3]);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmNewZone(%s) -> %d\n", zone[curzone], curzone);
#endif

    return curzone;
}

int
pmNewContextZone(void)
{
    __pmContext	*ctxp;
    int		sts;
    static pmID	pmid = 0;
    pmResult	*rp;

    if ((ctxp = __pmHandleToPtr(pmWhichContext())) == NULL)
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type == PM_CONTEXT_ARCHIVE)
	sts = pmNewZone(ctxp->c_archctl->ac_log->l_label.ill_tz);
    else if (ctxp->c_type == PM_CONTEXT_LOCAL)
	/* from env, not PMCD */
	sts = pmNewZone(__pmTimezone());
    else {
	/* assume PM_CONTEXT_HOST */
	if (pmid == 0) {
	    char	*name = "pmcd.timezone";
	    if ((sts = pmLookupName(1, &name, &pmid)) < 0)
		return sts;
	}
	if ((sts = pmFetch(1, &pmid, &rp)) >= 0) {
	    if (rp->vset[0]->numval == 1 && 
		(rp->vset[0]->valfmt == PM_VAL_DPTR || rp->vset[0]->valfmt == PM_VAL_SPTR))
		sts = pmNewZone((char *)rp->vset[0]->vlist[0].value.pval->vbuf);
	    else
		sts = PM_ERR_VALUE;
	    pmFreeResult(rp);
	}
    }
    
    return sts;
}

char *
pmCtime(const time_t *clock, char *buf)
{
    static struct tm	tbuf;
    if (curzone >= 0) {
	_pushTZ();
#ifdef IRIX5_3
	asctime_r(localtime_r(clock, &tbuf), buf, 26);
#else
	asctime_r(localtime_r(clock, &tbuf), buf);
#endif
	_popTZ();
    }
    else
#ifdef IRIX5_3
	asctime_r(localtime_r(clock, &tbuf), buf, 26);
#else
	asctime_r(localtime_r(clock, &tbuf), buf);
#endif

    return buf;
}

struct tm *
pmLocaltime(const time_t *clock, struct tm *result)
{
    if (curzone >= 0) {
	_pushTZ();
	localtime_r(clock, result);
	_popTZ();
    }
    else
	localtime_r(clock, result);

    return result;
}

time_t
__pmMktime(struct tm *timeptr)
{
    time_t	ans;

    if (curzone >= 0) {
	_pushTZ();
	ans = mktime(timeptr);
	_popTZ();
    }
    else
	ans = mktime(timeptr);

    return ans;
}

int
pmWhichZone(char **tz)
{
    if (curzone >= 0)
	*tz = zone[curzone];

    return curzone;
}
