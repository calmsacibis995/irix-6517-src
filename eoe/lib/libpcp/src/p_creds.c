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

#ident "$Id: p_creds.c,v 1.10 1998/11/15 08:35:24 kenmcd Exp $"

#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

/*
 * PDU for process credentials (PDU_CREDS)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		numcreds;
    __pmCred	credlist[1];
} creds_t;

int
__pmSendCreds(int fd, int mode, int credcount, const __pmCred *credlist)
{
    size_t	need = 0;
    creds_t	*pp = NULL;
    int		i;

    if ((credcount <= 0) || (credlist == NULL))
	return PM_ERR_IPC;

    if (mode == PDU_BINARY) {
	need = sizeof(creds_t) + ((credcount-1) * sizeof(__pmCred));
	if ((pp = (creds_t *)__pmFindPDUBuf((int)need)) == NULL)
	    return -errno;
	pp->hdr.len = (int)need;
	pp->hdr.type = PDU_CREDS;
	pp->numcreds = htonl(credcount);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    for (i = 0; i < credcount; i++)
		fprintf(stderr, "__pmSendCreds: #%d = %x\n", i, *(unsigned int*)&(credlist[i]));
#endif
	/* swab and fix bitfield order */
	for (i = 0; i < credcount; i++) {
	    pp->credlist[i] = __htonpmCred(credlist[i]);
	}

	return __pmXmitPDU(fd, (__pmPDU *)pp);
    }
    else {
	/* Outgoing ASCII credentials PDUs not supported */
	return PM_ERR_NOASCII;
    }
}

int
__pmDecodeCreds(__pmPDU *pdubuf, int mode, int *sender, int *credcount, __pmCred **credlist)
{
    creds_t	*pp;
    int		i;
    int		numcred;
    __pmCred	*list;

    if (mode == PDU_ASCII) {
	/* Incoming ASCII credentials PDUs not supported */
	return PM_ERR_NOASCII;
    }

    pp = (creds_t *)pdubuf;
    numcred = ntohl(pp->numcreds);
    if (pp == NULL || numcred < 0) return PM_ERR_IPC;
    *sender = pp->hdr.from;	/* ntohl() converted already in __pmGetPDU() */
    if ((list = (__pmCred *)malloc(sizeof(__pmCred) * numcred)) == NULL)
	return -errno;

    /* swab and fix bitfield order */
    for (i = 0; i < numcred; i++) {
	list[i] = __ntohpmCred(pp->credlist[i]);
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	for (i = 0; i < numcred; i++)
	    fprintf(stderr, "__pmDecodeCreds: #%d = { type=0x%x a=0x%x b=0x%x c=0x%x }\n",
		i, list[i].c_type, list[i].c_vala,
		list[i].c_valb, list[i].c_valc);
#endif

    *credlist = list;
    *credcount = numcred;

    return 0;
}
