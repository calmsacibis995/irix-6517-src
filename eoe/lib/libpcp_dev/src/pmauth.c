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

#include "pmapi.h"
#include "impl.h"
#include "pmapi_dev.h"

/*
 * assume 8-bit bytes
 */
#define BYTEMASK  0xFF		/* mask for one byte		*/
#define BYTESHFT  8		/* shift for one byte		*/

extern int		errno;

/*
 * Generates magic number used to provide authentication between
 * pmcd and unlicensed (but trusted) clients.
 * 
 * Note:
 * o  "authNumber" is the low 16-bits from lrand48() generated by pmcd
 *    and sent to the client in an extended error PDU
 * o  "clientId" is the pid of the client
 * o  the 0xffffff mask is needed because only 24 bits are transmitted
 *    across the credentials protocols
 */
unsigned int
__pmMakeAuthCookie(unsigned int authNumber, pid_t clientId)
{
    return( (authNumber^(unsigned int)clientId) & 0xffffff );
}

/*
 * authorized client connection protocol ... alternate to
 * scheme in __pmConnectPMCD() of libpcp
 */
static int
pcp_dev_trustme(int fd, int what)
{
    __pmPDUInfo	*pduinfo;
    __pmCred	handshake[2];
    unsigned	int auth;

    pduinfo = (__pmPDUInfo *)&what;
    auth = __pmMakeAuthCookie(pduinfo->authorize, getpid());
    handshake[0].c_type = CVERSION;
    handshake[0].c_vala = PDU_VERSION;
    handshake[0].c_valb = 0;
    handshake[0].c_valc = 0;
    handshake[1].c_type = CAUTH;
    handshake[1].c_vala = auth & BYTEMASK;
    handshake[1].c_valb = (auth >> BYTESHFT) & BYTEMASK;
    handshake[1].c_valc = (auth >> BYTESHFT >> BYTESHFT) & BYTEMASK;

    return __pmSendCreds(fd, PDU_BINARY, 2, handshake);
}

void
__pmSetAuthClient(void)
{
    __pmConnectHostMethod = pcp_dev_trustme;
}
