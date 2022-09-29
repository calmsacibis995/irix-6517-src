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

#ident "$Id: p_lstatus.c,v 1.8 1999/02/12 04:49:46 kenmcd Exp $"

#include <ctype.h>
#include <syslog.h>
#include "pmapi_dev.h"

extern int      errno;

/*
 * PDU for logger status information transfer (PDU_LOG_STATUS)
 */
typedef struct {
    __pmPDUHdr		hdr;
    int                 pad;            /* force status to be double word aligned */
    __pmLoggerStatus	status;
} logstatus_t;

int
__pmSendLogStatus(int fd, __pmLoggerStatus *status)
{
    logstatus_t	*pp;

    if ((pp = (logstatus_t *)__pmFindPDUBuf(sizeof(logstatus_t))) == NULL)
	return -errno;
    pp->hdr.len = sizeof(logstatus_t);
    pp->hdr.type = PDU_LOG_STATUS;
    memcpy(&pp->status, status, sizeof(__pmLoggerStatus));
#ifndef HAVE_NETWORK_BYTEORDER
    pp->status.ls_state = htonl(pp->status.ls_state);
    pp->status.ls_vol = htonl(pp->status.ls_vol);
    __htonll((char *)&pp->status.ls_size);
    pp->status.ls_start.tv_sec = htonl(pp->status.ls_start.tv_sec);
    pp->status.ls_start.tv_usec = htonl(pp->status.ls_start.tv_usec);
    pp->status.ls_last.tv_sec = htonl(pp->status.ls_last.tv_sec);
    pp->status.ls_last.tv_usec = htonl(pp->status.ls_last.tv_usec);
    pp->status.ls_timenow.tv_sec = htonl(pp->status.ls_timenow.tv_sec);
    pp->status.ls_timenow.tv_usec = htonl(pp->status.ls_timenow.tv_usec);
#endif

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	__pmIPC	*ipc;
	__pmFdLookupIPC(fd, &ipc);
	fprintf(stderr, "__pmSendLogStatus: sending PDU (toversion=%d)\n",
		(ipc)?(ipc->version):(LOG_PDU_VERSION));
    }
#endif
    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeLogStatus(__pmPDU *pdubuf, __pmLoggerStatus **status)
{
    logstatus_t	*pp;

    pp = (logstatus_t *)pdubuf;
#ifndef HAVE_NETWORK_BYTEORDER
    pp->status.ls_state = ntohl(pp->status.ls_state);
    pp->status.ls_vol = ntohl(pp->status.ls_vol);
    __ntohll((char *)&pp->status.ls_size);
    pp->status.ls_start.tv_sec = ntohl(pp->status.ls_start.tv_sec);
    pp->status.ls_start.tv_usec = ntohl(pp->status.ls_start.tv_usec);
    pp->status.ls_last.tv_sec = ntohl(pp->status.ls_last.tv_sec);
    pp->status.ls_last.tv_usec = ntohl(pp->status.ls_last.tv_usec);
    pp->status.ls_timenow.tv_sec = ntohl(pp->status.ls_timenow.tv_sec);
    pp->status.ls_timenow.tv_usec = ntohl(pp->status.ls_timenow.tv_usec);
#endif
    *status = &pp->status;
    __pmPinPDUBuf(pdubuf);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	__pmIPC	*ipc;
	__pmLookupIPC(&ipc);
	fprintf(stderr, "__pmDecodeLogStatus: got PDU (fromversion=%d)\n",
		(ipc)?(ipc->version):(LOG_PDU_VERSION));
    }
#endif
    return 0;
}
