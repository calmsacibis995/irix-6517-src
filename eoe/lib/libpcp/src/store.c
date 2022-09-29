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

#ident "$Id: store.c,v 2.16 1997/08/21 04:56:52 markgw Exp $"

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

int
pmStore(const pmResult *result)
{
    int		n;
    int		sts;
    __pmPDU	*pb;
    __pmContext	*ctxp;
    __pmDSO	*dp;

    if (result->numpmid < 1)
        return PM_ERR_TOOSMALL;

    for (n = 0; n < result->numpmid; n++) {
	if (result->vset[n]->numval < 1) {
	    return PM_ERR_VALUE;
	}
    }

    if ((sts = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(sts);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    sts = __pmSendResult(ctxp->c_pmcd->pc_fd, PDU_BINARY, result);
	    if (sts < 0)
		sts = __pmMapErrno(sts);
	    else {
		sts = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
		if (sts == PDU_ERROR)
		    __pmDecodeError(pb, PDU_BINARY, &sts);
		else if (sts != PM_ERR_TIMEOUT)
		    sts = PM_ERR_IPC;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    /*
	     * have to do them one at a time in case different DSOs
	     * involved ... need to copy each result->vset[n]
	     */
	    pmResult	tmp;
	    pmValueSet	tmpvset;
	    sts = 0;
	    for (n = 0; sts == 0 && n < result->numpmid; n++) {
		if ((dp = __pmLookupDSO(((__pmID_int *)&result->vset[n]->pmid)->domain)) == NULL)
		    sts = PM_ERR_NOAGENT;
		else {
		    tmp.numpmid = 1;
		    tmp.vset[0] = &tmpvset;
		    tmpvset.numval = 1;
		    tmpvset.pmid = result->vset[n]->pmid;
		    tmpvset.valfmt = result->vset[n]->valfmt;
		    tmpvset.vlist[0] = result->vset[n]->vlist[0];
		    if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
			sts = dp->dispatch.version.one.store(&tmp);
		    else
			sts = dp->dispatch.version.two.store(&tmp,
						dp->dispatch.version.two.ext);
		    if (sts < 0 &&
			dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			sts = XLATE_ERR_1TO2(sts);
		}
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE -- this is an error */
	    return PM_ERR_NOTHOST;
	}
    }

    return sts;
}
