/*
 * Copyright 1997, Silicon Graphics, Inc.
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

#ident "$Id: p_lrequest.c,v 1.9 1998/11/15 08:35:24 kenmcd Exp $"

#include <ctype.h>
#include <syslog.h>
#include "pmapi_dev.h"

extern int      errno;

/*
 * PDU for general pmlogger notification (PDU_LOG_REQUEST)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		type;		/* notification type */
} notify_t;

int
__pmSendLogRequest(int fd, int type)
{
    notify_t	*pp;

    if ((pp = (notify_t *)__pmFindPDUBuf(sizeof(notify_t))) == NULL)
	return -errno;
    pp->hdr.len = sizeof(notify_t);
    pp->hdr.type = PDU_LOG_REQUEST;
    pp->type = htonl(type);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	__pmIPC	*ipc;
	__pmFdLookupIPC(fd, &ipc);
	fprintf(stderr, "_pmSendRequest: sending PDU (type=%d, version=%d)\n",
		pp->type, (ipc)?(ipc->version):(LOG_PDU_VERSION));
    }
#endif
    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeLogRequest(const __pmPDU *pdubuf, int *type)
{
    const notify_t	*pp;

    pp = (const notify_t *)pdubuf;
    *type = ntohl(pp->type);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	__pmIPC	*ipc;
	__pmLookupIPC(&ipc);
	fprintf(stderr, "__pmDecodeLogRequest: got PDU (type=%d, version=%d)\n",
		*type, (ipc)?(ipc->version):(LOG_PDU_VERSION));
    }
#endif
    return 0;
}
