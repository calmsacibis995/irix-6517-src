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

#ident "$Id: p_datax.c,v 1.6 1998/11/15 08:35:24 kenmcd Exp $"

#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

/*
 * PDU for arbitrary binary data exchange (PDU_DATA_X)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		subtype;
    char	value[sizeof(int)];
} data_x_t;

int
__pmSendDataX(int fd, int mode, int subtype, int vlen, const void *vp)
{
    data_x_t	*pp;
    int		need;

    if (mode != PDU_BINARY) {
	/* ASCII mode is not supported */
	return PM_ERR_NOASCII;
    }

    need = (int)sizeof(data_x_t) - (int)sizeof(pp->value) + vlen;
    if ((pp = (data_x_t *)__pmFindPDUBuf(need)) == NULL)
	return -errno;
    pp->hdr.len = need;
    pp->hdr.type = PDU_DATA_X;
    pp->subtype = htonl(subtype);

    /* note: the caller must swab the value */
    if (vlen > 0)
	memmove(pp->value, vp, vlen);

    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeDataX(__pmPDU *pdubuf, int mode, int *subtype, int *vlen, void **vp)
{
    data_x_t	*pp;

    if (mode != PDU_BINARY) {
	/* ASCII mode is not supported */
	return PM_ERR_NOASCII;
    }

    pp = (data_x_t *)pdubuf;
    *subtype = ntohl(pp->subtype);
    *vlen = pp->hdr.len - (int)sizeof(pp->hdr) - (int)sizeof(pp->subtype);

    /* note: the caller must swab the value */
    *vp = pp->value;
    __pmPinPDUBuf(pdubuf);

    return 0;
}
