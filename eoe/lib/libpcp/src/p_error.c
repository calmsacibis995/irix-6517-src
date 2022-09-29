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

#ident "$Id: p_error.c,v 2.23 1998/11/15 08:35:24 kenmcd Exp $"

#include <ctype.h>
#include <syslog.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "pmapi.h"
#include "impl.h"

extern int      errno;

/*
 * PDU for general error reporting (PDU_ERROR)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		code;		/* error code */
} error_t;

/*
 * and the extended variant, with a second datum word
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		code;		/* error code */
    int		datum;		/* additional information */
} x_error_t;

int
__pmSendError(int fd, int mode, int code)
{
    __pmIPC	*ipc = NULL;
    error_t	*pp;
    int		sts;

    if (mode == PDU_ASCII) {
	/* Outgoing ASCII result PDUs not supported */
	return PM_ERR_NOASCII;
    }

    if ((pp = (error_t *)__pmFindPDUBuf(sizeof(error_t))) == NULL)
	return -errno;
    pp->hdr.len = sizeof(error_t);
    pp->hdr.type = PDU_ERROR;

    if ((sts = __pmFdLookupIPC(fd, &ipc)) < 0)
	return sts;

    if (ipc && ipc->version == PDU_VERSION1)
	pp->code = htonl(XLATE_ERR_2TO1(code));
    else
	pp->code = htonl(code);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmSendError: sending error PDU (code=%d, toversion=%d)\n",
		pp->code, (ipc)?(ipc->version):(PDU_VERSION));
#endif

    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmSendXtendError(int fd, int mode, int code, int datum)
{
    x_error_t	*pp;

    if (mode == PDU_ASCII)
	/* ASCII PDUs not supported */
	return PM_ERR_NOASCII;

    if ((pp = (x_error_t *)__pmFindPDUBuf(sizeof(x_error_t))) == NULL)
	return -errno;
    pp->hdr.len = sizeof(x_error_t);
    pp->hdr.type = PDU_ERROR;

    pp->code = htonl(XLATE_ERR_2TO1(code));
    pp->datum = datum; /* NOTE: caller must swab this */

    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeError(__pmPDU *pdubuf, int mode, int *code)
{
    __pmIPC	*ipc;
    error_t	*pp;
    int		sts;

    if ((sts = __pmLookupIPC(&ipc)) < 0)
	return sts;
    if (mode == PDU_BINARY) {
	pp = (error_t *)pdubuf;
	if (ipc && ipc->version == PDU_VERSION1)
	    *code = XLATE_ERR_1TO2((int)ntohl(pp->code));
	else
	    *code = ntohl(pp->code);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmDecodeError: got error PDU (code=%d, fromversion=%d)\n",
		*code, (ipc)?(ipc->version):(PDU_VERSION));
#endif
    }
    else {
	/* assume PDU_ASCII */
	int	n;

	n = __pmRecvLine(pdubuf, ABUFSIZE, __pmAbuf);
	if (n <= 0)
	    return n;
	if ((n = sscanf(__pmAbuf, "ERROR %d", code)) != 1) {
	    __pmNotifyErr(LOG_WARNING, "__pmDecodeError: ASCII botch @ \"%s\"\n", __pmAbuf);
	    return PM_ERR_IPC;
	}
	if (ipc && ipc->version == PDU_VERSION1)
	    *code = XLATE_ERR_1TO2(*code);
    }
    return 0;
}

int
__pmDecodeXtendError(__pmPDU *pdubuf, int mode, int *code, int *datum)
{
    x_error_t	*pp = (x_error_t *)pdubuf;
    int		sts;

    if (mode == PDU_ASCII)
	/* ASCII PDUs not supported */
	return PM_ERR_NOASCII;

    /*
     * is is ALWAYS a PCP 1.x error code here
     */
    *code = XLATE_ERR_1TO2((int)ntohl(pp->code));

    if (pp->hdr.len == sizeof(x_error_t)) {
	/* really version 2 extended error PDU */
	sts = PDU_VERSION2;
	*datum = pp->datum; /* NOTE: caller must swab this */
    }
    else {
	/* assume a vanilla 1.x error PDU ... has saame PDU type */
	sts = PDU_VERSION1;
	*datum = 0;
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmDecodeXtendError: got error PDU (code=%d, datum=%d, version=%d)\n",
		*code, *datum, sts);
#endif

    return sts;
}
