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

#ident "$Id: fetch.c,v 2.17 1998/03/26 17:19:22 kenmcd Exp $"

#include <stdio.h>
#include <malloc.h>
#include <sys/time.h>
#include "pmapi.h"
#include "impl.h"
#include "./logger.h"

int
myFetch(int numpmid, pmID pmidlist[], __pmPDU **pdup)
{
    int			n = 0;
    int			ctx;
    __pmPDU		*pb;
    __pmContext		*ctxp;

    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    if ((ctx = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp->c_type != PM_CONTEXT_HOST)
	    return PM_ERR_NOTHOST;
    }
    else
	return PM_ERR_NOCONTEXT;

#if CAN_RECONNECT
    if (ctxp->c_pmcd->pc_fd == -1) {
	/* lost connection, try to get it back */
	n = reconnect();
	if (n < 0)
	    return n;
    }
#endif

    if (ctxp->c_sent == 0) {
	/*
	 * current profile is _not_ already cached at other end of
	 * IPC, so send current profile
	 */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PROFILE)
	    fprintf(stderr, "myFetch: calling __pmSendProfile, context: %d\n", ctx);
#endif
	if ((n = __pmSendProfile(ctxp->c_pmcd->pc_fd, PDU_BINARY, ctx, ctxp->c_instprof)) >= 0)
	    ctxp->c_sent = 1;
    }

    if (n >= 0) {
	n = __pmSendFetch(ctxp->c_pmcd->pc_fd, PDU_BINARY, ctx, &ctxp->c_origin, numpmid, pmidlist);
	if (n >= 0){
	    int		changed = 0;
	    do {
		n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
		/*
		 * expect PDU_RESULT or
		 *        PDU_ERROR(changed > 0)+PDU_RESULT or
		 *        PDU_ERROR(real error < 0 from PMCD) or
		 *        0 (end of file)
		 *        < 0 (local error or IPC problem)
		 *        other (bogus PDU)
		 */
		if (n == PDU_RESULT)
		    /* success */
		    *pdup = pb;
		else if (n == PDU_ERROR) {
		    __pmDecodeError(pb, PDU_BINARY, &n);
		    if (n > 0) {
			/* PMCD state change protocol */
			changed = n;
			n = 0;
		    }
		    else {
			fprintf(stderr, "myFetch: ERROR PDU: %s\n", pmErrStr(n));
			disconnect(PM_ERR_IPC);
			/*NOTREACHED*/
		    }
		}
		else if (n == 0) {
		    fprintf(stderr, "myFetch: End of File: PMCD exited?\n");
		    disconnect(PM_ERR_IPC);
		    /*NOTREACHED*/
		}
		else if (n < 0) {
		    fprintf(stderr, "myFetch: __pmGetPDU: Error: %s\n", pmErrStr(n));
		    disconnect(PM_ERR_IPC);
		    /*NOTREACHED*/
		}
		else {
		    fprintf(stderr, "myFetch: Unexpected %s PDU from PMCD\n", __pmPDUTypeStr(n));
		    disconnect(PM_ERR_IPC);
		    /*NOTREACHED*/
		}
	    } while (n == 0);

	    if (changed & PMCD_ADD_AGENT) {
		/*
		 * PMCD_DROP_AGENT does not matter, no values are returned.
		 * Trying to restart (PMCD_RESTART_AGENT) is less interesting
		 * than when we actually start (PMCD_ADD_AGENT) ... the latter
		 * is also set when a successful restart occurs, but more
		 * to the point the sequence Install-Remove-Install does
		 * not involve a restart ... it is the second Install that
		 * generates the second PMCD_ADD_AGENT that we need to be
		 * particularly sensitive to, as this may reset counter
		 * metrics ...
		 */
		int	sts;
		if ((sts = putmark()) < 0) {
		    fprintf(stderr, "putmark: %s\n", pmErrStr(sts));
		    exit(1);
		}
	    }
	}
    }

    if (n < 0 && ctxp->c_pmcd->pc_fd != -1) {
	disconnect(n);
	/*NOTREACHED*/
    }

    return n;
}
