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

#ident "$Id: desc.c,v 2.13 1997/07/23 02:52:14 markgw Exp $"

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

int
pmLookupDesc(pmID pmid, pmDesc *desc)
{
    int		n;
    __pmPDU	*pb;
    __pmContext	*ctxp;
    __pmDSO	*dp;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    n = __pmSendDescReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, pmid);
	    if (n < 0)
		n = __pmMapErrno(n);
	    else {
		n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
		if (n == PDU_DESC)
		    n = __pmDecodeDesc(pb, PDU_BINARY, desc);
		else if (n == PDU_ERROR)
		    __pmDecodeError(pb, PDU_BINARY, &n);
		else if (n != PM_ERR_TIMEOUT)
		    n = PM_ERR_IPC;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmID_int *)&pmid)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    n = dp->dispatch.version.one.desc(pmid, desc);
		else
		    n = dp->dispatch.version.two.desc(pmid, desc, dp->dispatch.version.two.ext);
		if (n < 0 &&
		    dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			n = XLATE_ERR_1TO2(n);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    n = __pmLogLookupDesc(ctxp->c_archctl->ac_log, pmid, desc);
	}
    }

    return n;
}

