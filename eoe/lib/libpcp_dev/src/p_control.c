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

#ident "$Id: p_control.c,v 1.6 1997/09/12 03:31:06 markgw Exp $"

#include <syslog.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "pmapi_dev.h"

/*
 * This is a deprecated PDU, which is maintained for backward compatability
 * with PCP 1.x
 */

extern int	errno;

/*
 * PDU for __pmControlLogging request (PDU_CONTROL_REQ)
 */

typedef struct {
    pmID		v_pmid;
    int			v_numval;	/* no. of vlist els to follow */
    __pmValue_PDU	v_list[1];	/* one or more */
} vlist_t;

typedef struct {
    __pmPDUHdr		c_hdr;
    int			c_control;	/* mandatory or advisory */
    int			c_state;	/* off, maybe or on */
    int			c_delta;	/* requested logging interval (msec) */
    int			c_numpmid;	/* no. of vlist_ts to follow */
    __pmPDU		c_data[1];	/* one or more */
} control_req_t;

int
__pmSendControlReq(int fd, int mode, const pmResult *request, int control, int state, int delta)
{
    pmValueSet		*vsp;
    int			i;
    int			j;
    control_req_t	*pp;
    int			need;
    vlist_t		*vp;

    if (mode == PDU_ASCII) {
	/*
	 * Sending ASCII request PDUs is not supported, since this is now below
	 * the PMAPI.
	 */
	return PM_ERR_NOASCII;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU)
	__pmDumpResult(stderr, request);
#endif

    /* advisory+maybe logging and retrospective logging (delta < 0) are not
     *permitted
     */
    if (delta < 0 ||
	(control == PM_LOG_ADVISORY && state == PM_LOG_MAYBE))
	return -EINVAL;

    /* PDU header, control, state and count of metrics */
    need = sizeof(control_req_t) - sizeof(pp->c_data);
    for (i = 0; i < request->numpmid; i++) {
	/* plus PMID and count of values */
	if (request->vset[i]->numval > 0)
	    need += sizeof(vlist_t) + (request->vset[i]->numval - 1)*sizeof(__pmValue_PDU);
	else
	    need += sizeof(vlist_t) - sizeof(__pmValue_PDU);
    }
    if ((pp = (control_req_t *)__pmFindPDUBuf(need)) == NULL)
	return -errno;
    pp->c_hdr.len = need;
    pp->c_hdr.type = PDU_CONTROL_REQ;
    pp->c_control = htonl(control);
    pp->c_state = htonl(state);
    pp->c_delta = htonl(delta);
    pp->c_numpmid = htonl(request->numpmid);
    vp = (vlist_t *)pp->c_data;
    for (i = 0; i < request->numpmid; i++) {
	vsp = request->vset[i];
	vp->v_pmid = htonl(vsp->pmid);
	vp->v_numval = htonl(vsp->numval);
	/*
	 * Note: spec says only PM_VAL_INSITU can be used ... we don't
	 * check vsp->valfmt -- is this OK, because the "value" is never
	 * looked at!
	 */
	for (j = 0; j < vsp->numval; j++) {
	    vp->v_list[j].inst = htonl(vsp->vlist[j].inst);
	    vp->v_list[j].value.lval = htonl(vsp->vlist[j].value.lval);
	}
	if (vsp->numval > 0)
	    vp = (vlist_t *)((__psint_t)vp + sizeof(*vp) + (vsp->numval-1)*sizeof(__pmValue_PDU));
	else
	    vp = (vlist_t *)((__psint_t)vp + sizeof(*vp) - sizeof(__pmValue_PDU));
    }
    
    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeControlReq(const __pmPDU *pdubuf, int mode, pmResult **request, int *control, int *state, int *delta)
{
    int			sts;
    int			i;
    int			j;
    const control_req_t	*pp;
    int			numpmid;
    int			numval;
    size_t		need;
    pmResult		*req;
    pmValueSet		*vsp;
    vlist_t		*vp;

    if (mode == PDU_ASCII) {
	/* Incoming ASCII request PDUs not supported */
	return PM_ERR_NOASCII;
    }

    pp = (const control_req_t *)pdubuf;
    *control = ntohl(pp->c_control);
    *state = ntohl(pp->c_state);
    *delta = ntohl(pp->c_delta);
    numpmid = ntohl(pp->c_numpmid);
    vp = (vlist_t *)pp->c_data;

    need = sizeof(pmResult) + (numpmid - 1) * sizeof(pmValueSet *);
    if ((req = (pmResult *)malloc(need)) == NULL) {
	__pmNoMem("__pmDecodeControlReq.req", need, PM_RECOV_ERR);
	pmFreeResult(req);
	return -errno;
    }
    req->numpmid = numpmid;
    req->timestamp.tv_sec = 0;
    req->timestamp.tv_usec = 0;

    for (i = 0; i < numpmid; i++) {
	numval = ntohl(vp->v_numval);
	if (numval > 0)
	    need = sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue);
	else
	    need = sizeof(pmValueSet) - sizeof(pmValue);
	if ((vsp = (pmValueSet *)malloc(need)) == NULL) {
	    __pmNoMem("__pmDecodeControlReq.vsp", need, PM_RECOV_ERR);
	    sts = -errno;
	    i--;
	    while (i)
		free(req->vset[i--]);
	    free(req);
	    return sts;
	}
	req->vset[i] = vsp;
	vsp->pmid = ntohl(vp->v_pmid);
	vsp->numval = numval;
	vsp->valfmt  = PM_VAL_INSITU;
	for (j = 0; j < vsp->numval; j++) {
	    vsp->vlist[j].inst = ntohl(vp->v_list[j].inst);
	    vsp->vlist[j].value.lval = ntohl(vp->v_list[j].value.lval);
	}
	if (numval > 0)
	    vp = (vlist_t *)((__psint_t)vp + sizeof(*vp) + (numval-1)*sizeof(__pmValue_PDU));
	else
	    vp = (vlist_t *)((__psint_t)vp + sizeof(*vp) - sizeof(__pmValue_PDU));
    }

    *request = req;
    return 0;
}
