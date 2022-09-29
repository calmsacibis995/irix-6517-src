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

#ident "$Id: logcontrol.c,v 1.5 1997/09/05 01:12:52 markgw Exp $"

#include "pmapi_dev.h"

int
__pmControlLog(int fd, const pmResult *request, int control, int state, int delta, pmResult **status)
{
    int         n;
    __pmPDU       *pb;
    __pmIPC	*ipc;

    if (request->numpmid < 1)
        return PM_ERR_TOOSMALL;

    __pmFdLookupIPC(fd, &ipc);

    if (ipc == NULL || ipc->version == LOG_PDU_VERSION1) {
	/* send a PCP 1.x control request */
	n = __pmSendControlReq(fd, PDU_BINARY, request, control, state, delta);
    }
    else {
	/* send a PCP 2.0 log control request */
	n = __pmSendLogControl(fd, request, control, state, delta);
    }

    if (n < 0)
	n = __pmMapErrno(n);
    else {
	/* get the reply */
	n = __pmGetPDU(fd, PDU_BINARY, TIMEOUT_NEVER, &pb);
	if (n == PDU_RESULT) {
	    n = __pmDecodeResult(pb, PDU_BINARY, status);
	}
	else if (n == PDU_ERROR)
	    __pmDecodeError(pb, PDU_BINARY, &n);
	else if (n != PM_ERR_TIMEOUT)
	    n = PM_ERR_IPC; /* unknown reply type */
    }

    return n;
}
