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

#ident "$Id: p_fetch.c,v 2.6 1998/11/15 08:35:24 kenmcd Exp $"

#include "pmapi.h"
#include "impl.h"

extern int	errno;

/*
 * PDU for pmFetch request (PDU_FETCH)
 */
typedef struct {
    __pmPDUHdr		hdr;
    int			ctxnum;		/* context no. */
    __pmTimeval      	when;		/* desired time */
    int			numpmid;	/* no. PMIDs to follow */
    pmID		pmidlist[1];	/* one or more */
} fetch_t;

int
__pmSendFetch(int fd, int mode, int ctxnum, __pmTimeval *when, int numpmid, pmID *pmidlist)
{
    size_t	need;
    fetch_t	*pp;
    int		j;

    if (mode == PDU_BINARY) {
	need = sizeof(fetch_t) + (numpmid-1) * sizeof(pmID);
	if ((pp = (fetch_t *)__pmFindPDUBuf((int)need)) == NULL)
	    return -errno;
	pp->hdr.len = (int)need;
	pp->hdr.type = PDU_FETCH;
	pp->ctxnum = htonl(ctxnum);
	if (when == NULL)
	    memset((void *)&pp->when, 0, sizeof(pp->when));
	else {
#if _MIPS_SZLONG == 32
	    pp->when.tv_sec = htonl(when->tv_sec);
	    pp->when.tv_usec = htonl(when->tv_usec);
#else
	    pp->when.tv_sec = htonl((__int32_t)when->tv_sec);
	    pp->when.tv_usec = htonl((__int32_t)when->tv_usec);
#endif
	}
	pp->numpmid = htonl(numpmid);
	for (j = 0; j < numpmid; j++)
	    pp->pmidlist[j] = __htonpmID(pmidlist[j]);
	return __pmXmitPDU(fd, (__pmPDU *)pp);
    }
    else {
	/* assume PDU_ASCII */
	int		i, nbytes, sts;

	sprintf(__pmAbuf, "FETCH %d %d %d %d\n", ctxnum, numpmid, when->tv_sec, when->tv_usec);
	nbytes = (int)strlen(__pmAbuf);
	sts = __pmXmitAscii(fd, __pmAbuf, nbytes);
	if (sts < 0)
	    return sts;
	for (i = 0; i < numpmid; i++) {
	    sprintf(__pmAbuf, ". %d\n", pmidlist[i]);
	    nbytes = (int)strlen(__pmAbuf);
	    sts = __pmXmitAscii(fd, __pmAbuf, nbytes);
	    if (sts < 0)
		return sts;
	}
	return 0;
    }
}

int
__pmDecodeFetch(__pmPDU *pdubuf, int mode, int *ctxnum, __pmTimeval *when, int *numpmid, pmID **pmidlist)
{
    fetch_t	*pp;
    int		j;

    if (mode == PDU_ASCII) {
	/* Incoming ASCII request PDUs not supported */
	return PM_ERR_NOASCII;
    }

    pp = (fetch_t *)pdubuf;
    *ctxnum = ntohl(pp->ctxnum);
    when->tv_sec = ntohl(pp->when.tv_sec);
    when->tv_usec = ntohl(pp->when.tv_usec);
    *numpmid = ntohl(pp->numpmid);
    for (j = 0; j < *numpmid; j++)
	pp->pmidlist[j] = __ntohpmID(pp->pmidlist[j]);
    *pmidlist = pp->pmidlist;
    __pmPinPDUBuf((void *)pdubuf);

    return 0;
}
