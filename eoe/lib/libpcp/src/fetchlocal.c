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

#ident "$Id: fetchlocal.c,v 1.13 1997/07/25 07:03:33 markgw Exp $"

#include <stdio.h>
#include <malloc.h>
#include <sys/time.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

extern int	errno;

int
__pmFetchLocal(int numpmid, pmID pmidlist[], pmResult **result)
{
    int		sts;
    int		ctx;
    int		j;
    int		k;
    __pmContext	*ctxp;
    pmResult	*ans;
    __pmDSO	*dp;
    int		need;

    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    if ((ctx = pmWhichContext()) < 0)
	return ctx;
    ctxp = __pmHandleToPtr(ctx);

    /*
     * this is very ugly ... the DSOs have a high-water mark
     * allocation algorithm for the result skeleton, but the
     * code that calls us assumes it has freedom to retain
     * this result structure for as long as it wishes, and
     * then to call pmFreeResult
     *
     * we make another skeleton, selectively copy and return that
     *
     * (numpmid - 1) because there's room for one valueSet
     * in a pmResult
     */
    need = (int)sizeof(pmResult) + (numpmid - 1) * (int)sizeof(pmValueSet *);
    if ((ans = (pmResult *)malloc(need)) == NULL)
	return -errno;

    ans->numpmid = numpmid;
    gettimeofday(&ans->timestamp, NULL);
    for (j = 0; j < numpmid; j++)
	ans->vset[j] = NULL;

    for (j = 0; j < numpmid; j++) {

	if (ans->vset[j] != NULL)
	    /* picked up in a previous fetch */
	    continue;

	sts = 0;
	if ((dp = __pmLookupDSO(((__pmID_int *)&pmidlist[j])->domain)) == NULL)
	    /* based on domain, unknown PMDA */
	    sts = PM_ERR_NOAGENT;
	else {
	    if (ctxp->c_sent != dp->domain) {
		/*
		 * current profile is _not_ already cached at other end of
		 * IPC, so send get current profile ...
		 * Note: trickier than the non-local case, as no per-PMDA
		 *	 caching at the PMCD end, so need to remember the
		 *	 last domain to receive a profile
		 */
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_FETCH)
		    fprintf(stderr, "__pmFetchLocal: calling ???_profile(domain: %d), context: %d\n", dp->domain, ctx);
#endif
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    sts = dp->dispatch.version.one.profile(ctxp->c_instprof);
		else
		    sts = dp->dispatch.version.two.profile(ctxp->c_instprof,
					      dp->dispatch.version.two.ext);
		if (sts >= 0)
		    ctxp->c_sent = dp->domain;
	    }
	}

	if (sts >= 0) {
	    if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		sts = dp->dispatch.version.one.fetch(numpmid, pmidlist, result);
	    else
		sts = dp->dispatch.version.two.fetch(numpmid, pmidlist, result,
						  dp->dispatch.version.two.ext);
	}

	for (k = j; k < numpmid; k++) {
	    if (((__pmID_int *)&pmidlist[j])->domain == ((__pmID_int *)&pmidlist[k])->domain) {
		if (sts < 0) {
		    ans->vset[k] = (pmValueSet *)malloc(sizeof(pmValueSet));
		    if (ans->vset[k] == NULL)
			return -errno;
		    ans->vset[k]->numval = sts;
		}
		else {
		    ans->vset[k] = (*result)->vset[k];
		}
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_FETCH) {
		    fprintf(stderr, "__pmFetchLocal: [%d] PMID=%s nval=",
			    k, pmIDStr(pmidlist[k]));
		    if (ans->vset[k]->numval < 0)
			fprintf(stderr, "%s\n", pmErrStr(ans->vset[k]->numval));
		    else
			fprintf(stderr, "%d\n", ans->vset[k]->numval);
		}
#endif
	    }
	}
    }
    *result = ans;

    return 0;
}
