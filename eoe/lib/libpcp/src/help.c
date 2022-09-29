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

#ident "$Id: help.c,v 2.15 1998/11/15 08:35:24 kenmcd Exp $"

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

static int
lookuptext(int ident, int type, char **buffer)
{
    int		n;
    __pmPDU	*pb;
    __pmContext	*ctxp;
    __pmDSO	*dp;
    int		x_ident;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type == PM_CONTEXT_HOST) {

again:
	    n = __pmSendTextReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, ident, type);
	    if (n < 0)
		n = __pmMapErrno(n);
	    else {
		n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
		if (n == PDU_TEXT) {
		    n = __pmDecodeText(pb, PDU_BINARY, &x_ident, buffer);
		    /*
		     * Note: __pmDecodeText does not swab ident because it does not
		     * know whether it's a pmID or a pmInDom. So x_ident is still in
		     * network byte order, but we don't use it anyway.
		     */

		    if (n == 0 && (*buffer)[0] == '\0' && (type & PM_TEXT_HELP)) {
			/* fall back to oneline, if possible */
			free(*buffer);
			type &= ~PM_TEXT_HELP;
			type |= PM_TEXT_ONELINE;
			goto again;
		    }
		}
		else if (n == PDU_ERROR)
		    __pmDecodeError(pb, PDU_BINARY, &n);
		else if (n != PM_ERR_TIMEOUT)
		    n = PM_ERR_IPC;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmID_int *)&ident)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
again_local:
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    n = dp->dispatch.version.one.text(ident, type, buffer);
		else
		    n = dp->dispatch.version.two.text(ident, type, buffer, dp->dispatch.version.two.ext);
		if (n == 0 && (*buffer)[0] == '\0' && (type & PM_TEXT_HELP)) {
		    /* fall back to oneline, if possible */
		    free(*buffer);
		    type &= ~PM_TEXT_HELP;
		    type |= PM_TEXT_ONELINE;
		    goto again_local;
		}
		if (n < 0 &&
		    dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			n = XLATE_ERR_1TO2(n);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE -- this is an error */
	    return PM_ERR_NOTHOST;
	}
    }

    return n;
}

int
pmLookupText(pmID pmid, int level, char **buffer)
{
    return lookuptext((int)pmid, level | PM_TEXT_PMID, buffer);
}

int
pmLookupInDomText(pmInDom indom, int level, char **buffer)
{
    return lookuptext((int)indom, level | PM_TEXT_INDOM, buffer);
}
