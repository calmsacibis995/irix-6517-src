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

#ident "$Id: fetch.c,v 2.19 1999/04/30 01:44:04 kenmcd Exp $"

#include <stdio.h>
#include <malloc.h>
#include <sys/time.h>
#include "pmapi.h"
#include "impl.h"

extern int __pmFetchLocal(int, pmID *, pmResult **);

int
pmFetch(int numpmid, pmID pmidlist[], pmResult **result)
{
    int		n;
    __pmPDU	*pb;
    __pmContext	*ctxp;
    int		ctxp_mode;

    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	ctxp_mode = (ctxp->c_mode & __PM_MODE_MASK);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if (ctxp->c_sent == 0) {
		/*
		 * current profile is _not_ already cached at other end of
		 * IPC, so send get current profile
		 */
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PROFILE) {
		    fprintf(stderr, "pmFetch: calling __pmSendProfile, context: %d\n", n);
		    __pmDumpProfile(stderr, PM_INDOM_NULL, ctxp->c_instprof);
		}
#endif
		if ((n = __pmSendProfile(ctxp->c_pmcd->pc_fd, PDU_BINARY, n, ctxp->c_instprof)) < 0)
		    n = __pmMapErrno(n);
		else
		    ctxp->c_sent = 1;
	    }
	    if (n >= 0) {
		n = __pmSendFetch(ctxp->c_pmcd->pc_fd, PDU_BINARY,
			pmWhichContext(), &ctxp->c_origin, numpmid, pmidlist);
		if (n < 0)
		    n = __pmMapErrno(n);
		else {
		    int		changed = 0;
		    do {
			n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
			if (n == PDU_RESULT) {
			    n = __pmDecodeResult(pb, PDU_BINARY, result);
			}
			else if (n == PDU_ERROR) {
			    __pmDecodeError(pb, PDU_BINARY, &n);
			    if (n > 0)
				/* PMCD state change protocol */
				changed = n;
			}
			else if (n != PM_ERR_TIMEOUT)
			    n = PM_ERR_IPC;
		    } while (n > 0);
		    if (n == 0)
			n |= changed;
		}
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    n = __pmFetchLocal(numpmid, pmidlist, result);
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    n = __pmLogFetch(ctxp, numpmid, pmidlist, result);
	    if (n >= 0 && ctxp_mode != PM_MODE_INTERP) {
		ctxp->c_origin.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
		ctxp->c_origin.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
	    }
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_FETCH) {
	fprintf(stderr, "pmFetch returns ...\n");
	if (n > 0) {
	    fprintf(stderr, "PMCD state changes: agent(s)");
	    if (n & PMCD_ADD_AGENT) fprintf(stderr, " added");
	    if (n & PMCD_RESTART_AGENT) fprintf(stderr, " restarted");
	    if (n & PMCD_DROP_AGENT) fprintf(stderr, " dropped");
	    fputc('\n', stderr);
	}
	if (n >= 0)
	    __pmDumpResult(stderr, *result);
	else
	    fprintf(stderr, "Error: %s\n", pmErrStr(n));
    }
#endif

    return n;
}

int
pmFetchArchive(pmResult **result)
{
    int		n;
    __pmContext	*ctxp;
    int		ctxp_mode;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	ctxp_mode = (ctxp->c_mode & __PM_MODE_MASK);
	if (ctxp->c_type != PM_CONTEXT_ARCHIVE)
	    n = PM_ERR_NOTARCHIVE;
	else if (ctxp_mode == PM_MODE_INTERP)
	    /* makes no sense! */
	    n = PM_ERR_MODE;
	else {
	    /* assume PM_CONTEXT_ARCHIVE and BACK or FORW */
	    n = __pmLogFetch(ctxp, 0, NULL, result);
	    if (n >= 0) {
		ctxp->c_origin.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
		ctxp->c_origin.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
	    }
	}
    }

    return n;
}

int
pmSetMode(int mode, const struct timeval *when, int delta)
{
    int		n;
    __pmContext	*ctxp;
    int		l_mode = (mode & __PM_MODE_MASK);

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if (l_mode != PM_MODE_LIVE)
		return PM_ERR_MODE;

	    ctxp->c_origin.tv_sec = ctxp->c_origin.tv_usec = 0;
	    ctxp->c_mode = mode;
	    ctxp->c_delta = delta;
	    return 0;
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
		return PM_ERR_MODE;
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    if (l_mode == PM_MODE_INTERP ||
		l_mode == PM_MODE_FORW || l_mode == PM_MODE_BACK) {
		if (when != NULL) {
		    /*
		     * special case of NULL for timestamp
		     * => do not update notion of "current" time
		     */
		    ctxp->c_origin.tv_sec = (__int32_t)when->tv_sec;
		    ctxp->c_origin.tv_usec = (__int32_t)when->tv_usec;
		}
		ctxp->c_mode = mode;
		ctxp->c_delta = delta;
		__pmLogSetTime(ctxp);
		__pmLogResetInterp(ctxp);
		return 0;
	    }
	    else
		return PM_ERR_MODE;
	}
    }
    else
	return n;
}
