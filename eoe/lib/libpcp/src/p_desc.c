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

#ident "$Id: p_desc.c,v 2.7 1998/11/15 08:35:24 kenmcd Exp $"

#include <syslog.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

/*
 * PDU for pmLookupDesc request (PDU_DESC_REQ)
 */
typedef struct {
    __pmPDUHdr	hdr;
    pmID	pmid;
} desc_req_t;

int
__pmSendDescReq(int fd, int mode, pmID pmid)
{
    desc_req_t	*pp;

    if (mode == PDU_BINARY) {
	if ((pp = (desc_req_t *)__pmFindPDUBuf(sizeof(desc_req_t))) == NULL)
	    return -errno;
	pp->hdr.len = sizeof(desc_req_t);
	pp->hdr.type = PDU_DESC_REQ;
	pp->pmid = __htonpmID(pmid);

#ifdef DESPERATE
	fprintf(stderr, "__pmSendDescReq: converted 0x%08x (%s) to 0x%08x\n", pmid, pmIDStr(pmid), pp->pmid);
#endif

	return __pmXmitPDU(fd, (__pmPDU *)pp);
    }
    else {
	int		nbytes, sts;

	sprintf(__pmAbuf, "DESC_REQ %d\n", pmid);
	nbytes = (int)strlen(__pmAbuf);
	sts = __pmXmitAscii(fd, __pmAbuf, nbytes);
	if (sts < 0)
	    return sts;
	return 0;
    }

}

int
__pmDecodeDescReq(__pmPDU *pdubuf, int mode, pmID *pmid)
{
    desc_req_t	*pp;

    if (mode == PDU_ASCII) {
	/* Incoming ASCII request PDUs not supported */
	return PM_ERR_NOASCII;
    }

    pp = (desc_req_t *)pdubuf;
    *pmid = __ntohpmID(pp->pmid);

    return 0;
}

/*
 * PDU for pmLookupDesc result (PDU_DESC)
 */
typedef struct {
    __pmPDUHdr	hdr;
    pmDesc	desc;
} desc_t;

int
__pmSendDesc(int fd, int mode, pmDesc *desc)
{
    desc_t		*pp;

    if (mode == PDU_ASCII) {
	/* Outgoing ASCII result PDUs not supported */
	return PM_ERR_NOASCII;
    }

    if ((pp = (desc_t *)__pmFindPDUBuf(sizeof(desc_t))) == NULL)
	return -errno;

    pp->hdr.len = sizeof(desc_t);
    pp->hdr.type = PDU_DESC;

    pp->desc.type = htonl(desc->type);
    pp->desc.sem = htonl(desc->sem);

    pp->desc.indom = __htonpmInDom(desc->indom);
    pp->desc.units = __htonpmUnits(desc->units);
    pp->desc.pmid = __htonpmID(desc->pmid);

    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeDesc(__pmPDU *pdubuf, int mode, pmDesc *desc)
{
    desc_t		*pp;

    if (mode == PDU_BINARY) {
	pp = (desc_t *)pdubuf;
	desc->type = ntohl(pp->desc.type);
	desc->sem = ntohl(pp->desc.sem);

	desc->indom = __ntohpmInDom(pp->desc.indom);
	desc->units = __ntohpmUnits(pp->desc.units);
	desc->pmid = __ntohpmID(pp->desc.pmid);
	return 0;
    }
    else {
	/* assume PDU_ASCII */
	int		n;
	int		dimSpace, dimTime, dimCount;
	int		scaleSpace, scaleTime, scaleCount;

	n = __pmRecvLine(pdubuf, ABUFSIZE, __pmAbuf);
	if (n <= 0)
	    return n;
	if ((n = sscanf(__pmAbuf, "DESC %d %d %d %d %d %d %d %d %d %d",
	    &desc->pmid, &desc->type, &desc->indom, &desc->sem,
	    &dimSpace, &dimTime, &dimCount,
	    &scaleSpace, &scaleTime, &scaleCount)) != 10) {
	    __pmNotifyErr(LOG_WARNING, "__pmDecodeDesc: ASCII botch %d values from: \"%s\"\n", n, __pmAbuf);
	    return PM_ERR_IPC;
	}
	desc->units.dimSpace = dimSpace;
	desc->units.dimTime = dimTime;
	desc->units.dimCount = dimCount;
	desc->units.scaleSpace = scaleSpace;
	desc->units.scaleTime = scaleTime;
	desc->units.scaleCount = scaleCount;
	return 0;
    }
}
