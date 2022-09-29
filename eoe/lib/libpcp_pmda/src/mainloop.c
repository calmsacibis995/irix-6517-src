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

#ident "$Id"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "./libdefs.h"

extern int __pmdaSetupPDU(int, int, char *);


int
__pmdaInFd(pmdaInterface *dispatch)
{
    if (!HAVE_V_TWO(dispatch->comm.pmda_interface)) {
	__pmNotifyErr(LOG_CRIT, "PMDA interface version %d not supported",
		     dispatch->comm.pmda_interface);
        return -1;
    }
    return dispatch->version.two.ext->e_infd;
}

int
__pmdaMainPDU(pmdaInterface *dispatch)
{
    __pmPDU		*pb;
    int			sts;
    pmID		pmid;
    pmDesc		desc;
    int			npmids;
    pmID		*pmidlist;
    pmResult		*result;
    int			ctxnum;
    __pmTimeval		when;
    int			ident;
    int			type;
    pmInDom		indom;
    int			inst;
    char		*iname;
    __pmInResult		*inres;
    char		*buffer;
    __pmProfile		*profile;
    __pmProfile		*saveprofile = NULL;
    static int		first_time = 1;
    static pmdaExt	*pmda = NULL;

    /* Initial version checks */
    if (first_time) {
	if (dispatch->status != 0) {
	    __pmNotifyErr(LOG_ERR, "PMDA Initialisation Failed");
	    return -1;
	}
	if (!HAVE_V_TWO(dispatch->comm.pmda_interface)) {
	    __pmNotifyErr(LOG_CRIT, "PMDA interface version %d not supported",
			 dispatch->comm.pmda_interface);
	    return -1;
	}
	dispatch->comm.pmapi_version = PMAPI_VERSION;
	pmda = dispatch->version.two.ext; /* interface Ver 2 Extensions */
	first_time = 0;
    }

    do {
	sts = __pmGetPDU(pmda->e_infd, PDU_BINARY, TIMEOUT_NEVER, &pb);
	if (sts == 0)
	    /* End of File */
	    return PM_ERR_EOF;
	if (sts < 0) {
	    __pmNotifyErr(LOG_ERR, "IPC Error: %s\n", pmErrStr(sts));
	    return sts;
	}

	/*
	 * if defined, callback once per PDU to check availability, etc.
	 */
	if (pmda->e_checkCallBack) {
	    int		i;
	    i = (*(pmda->e_checkCallBack))();
	    if (i < 0) {
		if (sts != PDU_PROFILE)
		    /* all other PDUs expect an ACK */
		    __pmSendError(pmda->e_outfd, PDU_BINARY, i);
		return 0;
	    }
	}

	switch (sts) {

	    case PDU_PROFILE:
		/*
		 * can ignore ctxnum, since pmcd has already used this to send
		 * the correct profile, if required
		 */

    #ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    __pmNotifyErr(LOG_DEBUG, "Received PDU_PROFILE\n");
		}
    #endif

		if ((sts = __pmDecodeProfile(pb, PDU_BINARY, &ctxnum, &profile)) >= 0) {
		    sts = dispatch->version.two.profile(profile, pmda);
		}

		if (sts < 0)
		    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
		else {
		    if (saveprofile != NULL)
			__pmFreeProfile(saveprofile);
		    /*
		     * need to keep the last valid one around, as the DSO
		     * routine just remembers the address
		     */
		    saveprofile = profile;
		}
		break;

	    case PDU_FETCH:
		/*
		 * can ignore ctxnum, since pmcd has already used this to send
		 * the correct profile, if required
		 */

    #ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    __pmNotifyErr(LOG_DEBUG, "Received PDU_FETCH\n");
		}
    #endif

		sts = __pmDecodeFetch(pb, PDU_BINARY, &ctxnum, &when, &npmids, &pmidlist);

		/* Ignore "when"; pmcd should intercept archive log requests */
		if (sts >= 0) {
		    sts = dispatch->version.two.fetch(npmids, pmidlist, &result, pmda);

		    __pmUnpinPDUBuf(pmidlist);
		}
		if (sts < 0)
		    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
		else {
    /* this is for PURIFY to prevent a UMR in __pmXmitPDU */
		    result->timestamp.tv_sec = 0;
		    result->timestamp.tv_usec = 0;
		    __pmSendResult(pmda->e_outfd, PDU_BINARY, result);
		    (pmda->e_resultCallBack)(result);
		}
		break;

	    case PDU_DESC_REQ:

    #ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    __pmNotifyErr(LOG_DEBUG, "Received PDU_DESC_REQ\n");
		}
    #endif

		if ((sts = __pmDecodeDescReq(pb, PDU_BINARY, &pmid)) >= 0) {
		    sts = dispatch->version.two.desc(pmid, &desc, pmda);
		}
		if (sts < 0)
		    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
		else
		    __pmSendDesc(pmda->e_outfd, PDU_BINARY, &desc);
		break;

	    case PDU_INSTANCE_REQ:

    #ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    __pmNotifyErr(LOG_DEBUG, "Received PDU_INSTANCE_REQ\n");
		}
    #endif

		if ((sts = __pmDecodeInstanceReq(pb, PDU_BINARY, &when, &indom, &inst, 
						&iname)) >= 0) {
		    /*
		     * Note: when is ignored.
		     *		If we get this far, we are _only_ dealing
		     *		with current data (pmcd handles the other
		     *		cases).
		     */

		    sts = dispatch->version.two.instance(indom, inst, iname, &inres, 
							 pmda);
		}
		if (sts < 0)
		    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
		else {
		    __pmSendInstance(pmda->e_outfd, PDU_BINARY, inres);
		    __pmFreeInResult(inres);
		}
		break;

	    case PDU_TEXT_REQ:

    #ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    __pmNotifyErr(LOG_DEBUG, "Received PDU_TEXT_REQ\n");
		}
    #endif

		if ((sts = __pmDecodeTextReq(pb, PDU_BINARY, &ident, &type)) >= 0) {
		    sts = dispatch->version.two.text(ident, type, &buffer, pmda);
		}
		if (sts < 0)
		    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
		else
		    __pmSendText(pmda->e_outfd, PDU_BINARY, ident, buffer);
		break;

	    case PDU_RESULT:

    #ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    __pmNotifyErr(LOG_DEBUG, "Received PDU_RESULT\n");
		}
    #endif

		if ((sts = __pmDecodeResult(pb, PDU_BINARY, &result)) >= 0) {
		    sts = dispatch->version.two.store(result, pmda);
		}
		__pmSendError(pmda->e_outfd, PDU_BINARY, sts);
		pmFreeResult(result);
		break;

	    case PDU_CONTROL_REQ:
		 /*
		  * The protocol followed by the PMCD is to send these as
		  * advisory notices, but not to expect any response, and
		  * in particular no response PDU is required.
		  */

    #ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    __pmNotifyErr(LOG_DEBUG, "Received PDU_CONTROL_REQ\n");
		}
    #endif

#if 0
		/*
		 * this function moved to libpcp_dev.a - this isn't being used
		 * by any PMDA, so its out of 2.0 libpcp.
		 */
		if ((sts = __pmDecodeControlReq(pb, PDU_BINARY, &result, &control, &state, &delta)) >= 0) {
		    __pmNotifyErr(LOG_ERR, "PDU_CONTROL_REQ not supported");
		}
#endif
		break;

	    default:
		__pmNotifyErr(LOG_ERR,
			     "%s: Unrecognised pdu type: 0x%0x?\n",
			     pmda->e_name, sts);
		break;
	}

	/*
	 * if defined, callback once per PDU to do termination checks,
	 * stats, etc
	 */
	if (pmda->e_doneCallBack)
	    (*(pmda->e_doneCallBack))();

    } while(__pmMoreInput(pmda->e_infd));

    return 0;
}


void 
pmdaMain(pmdaInterface *dispatch)
{
    for ( ; ; ) {
	if (__pmdaMainPDU(dispatch) < 0)
	    break;
    }
}


/* TODO: this function serves no useful purpose and should be removed. */
/*ARGSUSED*/
void
pmdaMainLoopFreeResultCallback(void (*callback)(pmResult *res))
{
    __pmNotifyErr(LOG_CRIT, 
		 "pmdaMainLoopFreeResultCallback() not supported. Use pmdaSetResultCallBack().");
    exit(1);
}

void
pmdaSetResultCallBack(pmdaInterface *dispatch, pmdaResultCallBack callback)
{
    if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	dispatch->version.two.ext->e_resultCallBack = callback;
    else {
	__pmNotifyErr(LOG_CRIT, "Unable to set result callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetFetchCallBack(pmdaInterface *dispatch, pmdaFetchCallBack callback)
{
    if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	dispatch->version.two.ext->e_fetchCallBack = callback;
    else {
	__pmNotifyErr(LOG_CRIT, "Unable to set fetch callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetCheckCallBack(pmdaInterface *dispatch, pmdaCheckCallBack callback)
{
    if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	dispatch->version.two.ext->e_checkCallBack = callback;
    else {
	__pmNotifyErr(LOG_CRIT, "Unable to set check callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetDoneCallBack(pmdaInterface *dispatch, pmdaDoneCallBack callback)
{
    if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	dispatch->version.two.ext->e_doneCallBack = callback;
    else {
	__pmNotifyErr(LOG_CRIT, "Unable to set done callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}
