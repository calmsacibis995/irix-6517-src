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

#ident "$Id: p_text.c,v 2.7 1998/11/15 08:35:24 kenmcd Exp $"

#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

/*
 * PDU for pmLookupText request (PDU_TEXT_REQ)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		ident;
    int		type;		/* one line or help, PMID or InDom */
} text_req_t;

int
__pmSendTextReq(int fd, int mode, int ident, int type)
{
    text_req_t	*pp;

    if (mode == PDU_BINARY) {

	if ((pp = (text_req_t *)__pmFindPDUBuf(sizeof(text_req_t))) == NULL)
	    return -errno;
	pp->hdr.len = sizeof(text_req_t);
	pp->hdr.type = PDU_TEXT_REQ;

	if (type & PM_TEXT_PMID) {
	    pp->ident = __htonpmID((pmID)ident);
	}
	else
	if (type & PM_TEXT_INDOM) {
	    pp->ident = __htonpmInDom((pmInDom)ident);
	}

	pp->type = htonl(type);

	return __pmXmitPDU(fd, (__pmPDU *)pp);
    }
    else {
	/* assume PDU_ASCII */
	int		nbytes, sts;

	sprintf(__pmAbuf, "TEXT_REQ %d %d\n", ident, type);
	nbytes = (int)strlen(__pmAbuf);
	sts = __pmXmitAscii(fd, __pmAbuf, nbytes);
	if (sts < 0)
	    return sts;
	return 0;
    }
}

int
__pmDecodeTextReq(__pmPDU *pdubuf, int mode, int *ident, int *type)
{
    text_req_t	*pp;

    if (mode == PDU_ASCII) {
	/* Incoming ASCII request PDUs not supported */
	return PM_ERR_NOASCII;
    }

    pp = (text_req_t *)pdubuf;
    *type = ntohl(pp->type);

    if ((*type) & PM_TEXT_PMID)
	*ident = __ntohpmID(pp->ident);
    else
    if ((*type) & PM_TEXT_INDOM)
	*ident = __ntohpmInDom(pp->ident);

    return 0;
}

/*
 * PDU for pmLookupText result (PDU_TEXT)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		ident;
    int		buflen;			/* no. of chars following */
    char	buffer[sizeof(int)];	/* desired text */
} text_t;

int
__pmSendText(int fd, int mode, int ident, const char *buffer)
{
    text_t	*pp;
    size_t	need;

    if (mode == PDU_ASCII) {
	/* Outgoing ASCII result PDUs not supported */
	return PM_ERR_NOASCII;
    }

    need = sizeof(text_t) - sizeof(pp->buffer) + sizeof(__pmPDU)*((strlen(buffer) - 1 + sizeof(__pmPDU))/sizeof(__pmPDU));
    if ((pp = (text_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -errno;
    pp->hdr.len = (int)need;
    pp->hdr.type = PDU_TEXT;
    /*
     * Note: ident argument must already be in network byte order.
     * The caller has to do this because the type of ident is not
     * part of the transmitted PDU_TEXT pdu; ident may be either
     * a pmID or pmInDom, and so the caller must use either
     * __htonpmID or __htonpmInDom (respectfully).
     */
    pp->ident = ident;

    pp->buflen = (int)strlen(buffer);
    memcpy((void *)pp->buffer, (void *)buffer, pp->buflen);
    pp->buflen = htonl(pp->buflen);

    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeText(__pmPDU *pdubuf, int mode, int *ident, char **buffer)
{
    text_t	*pp;

    if (mode == PDU_BINARY) {
	int		buflen;
	pp = (text_t *)pdubuf;
	/*
	 * Note: ident argument is returned in network byte order.
	 * The caller has to convert it to host byte order because
	 * the type of ident is not part of the transmitted PDU_TEXT
	 * pdu (ident may be either a pmID or a pmInDom. The caller
	 * must use either __ntohpmID() or __ntohpmInDom(), respectfully.
	 */
	*ident = pp->ident;

	buflen = ntohl(pp->buflen);
	if ((*buffer = (char *)malloc(buflen+1)) == NULL)
	    return -errno;
	strncpy(*buffer, pp->buffer, buflen);
	(*buffer)[buflen] = '\0';
    }
    else {
	/* assume PDU_ASCII */
	int		n;
	int		len;
	int		type;
	int		nbyte;
	char		*p;

	n = __pmRecvLine(pdubuf, ABUFSIZE, __pmAbuf);
	if (n <= 0)
	    return n;
	if ((n = sscanf(__pmAbuf, "TEXT %d %d %d", ident, &type, &nbyte)) != 3) {
	    __pmNotifyErr(LOG_WARNING, "__pmDecodeText: ASCII botch %d values from: \"%s\"\n", n, __pmAbuf);
	    return PM_ERR_IPC;
	}
	if ((*buffer = (char *)malloc(nbyte+1)) == NULL)
	    return -errno;
	p = *buffer;
	len = nbyte;
	while (len) {
	    n = __pmRecvLine(pdubuf, ABUFSIZE, __pmAbuf);
	    if (n <= 0) {
		free(*buffer);
		return n;
	    }
	    /* -1 here as follows: -2 for leading ". " +1 for stripped \n */
	    n = (int)strlen(__pmAbuf) - 1;
	    len -= n;
	    if (len < 0) {
		__pmNotifyErr(LOG_WARNING, "__pmDecodeText: ASCII botch, more than %d bytes of text, ...\nat or after: \"%s\"\n", nbyte, __pmAbuf);
		free(*buffer);
		return PM_ERR_IPC;
	    }
	    strcpy(p, &__pmAbuf[2]);
	    p += n;
	    p[-1] = '\n';	/* put back what was stripped by __pmRecvLine */
	}
	if (p > *buffer && (type & PM_TEXT_ONELINE))
	    /* no trailing \n for non-null one-line fellows */
	    p--;
	*p = '\0';
    }

    return 0;
}
