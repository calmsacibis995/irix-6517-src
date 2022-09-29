/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Query Handler
 *
 *	$Revision: 1.1 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <bstring.h>
#include <string.h>
#include <osfcn.h>
#include <ctype.h>
#include <alloca.h>
#include <fcntl.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"
#include "message.h"
#include "query.h"

const unsigned int defaultBufferSize = 8192;
extern char *defaultCommunity;

int
snmpQueryHandler::vget(const struct in_addr *a, const varBindList *arg,
		      varBindList **result, int *ErrIdx)
{
    getRequestPDU pdu;
    return vquery(a, arg, result, &pdu, ErrIdx);
}

int
snmpQueryHandler::vgetNext(const struct in_addr *a, const varBindList *arg,
			  varBindList **result, int *ErrIdx)
{
   getNextRequestPDU pdu;
   return vquery(a, arg, result, &pdu, ErrIdx); 
}

/*
** This is a very limited vectorized query function.  It will send
** the same request to a list of machines, and then wait for all
** of their responses.  It is limited to one try.
*/
int
snmpQueryHandler::vquery(const struct in_addr *a, const varBindList *arg,
			varBindList **result, snmpPDU *pdu, int *ErrIdx)
{
    snmpPacket request;
    snmpPacket *response;
    int rc, first, count=0, tries, len, i;
    char *status;
    int rstatus=SNMP_ERR_noError;

    for (i=0; a[i].s_addr != 0; i++)
	ErrIdx[i] = QUERY_ERR_timeout;
    status = (char *)alloca(i);
    bzero(status, i);

    if (requestBuffer == 0)
	requestBuffer = new char[defaultBufferSize];
    if (responseBuffer == 0)
	responseBuffer = new char[defaultBufferSize];

    request.setCommunity(community == 0 ? (char *) defaultCommunity
					: community);
    request.setPDU(pdu);

    // Use varBindList given.  This is ugly.
    varBindList *pvl = pdu->getVarList();
    bcopy(arg, pvl, sizeof *pvl);

    // Initialize the request
    pdu->setErrorStatus(0);
    pdu->setErrorIndex(0);

    // Set up the timeval
    struct timeval tv;
    tv.tv_sec = interval;
    tv.tv_usec = 0;

    // Flush receive socket.
    fcntl(sock, F_SETFL, FNDELAY);
    char rbuf[128];
    for (i=0; (recv(sock, rbuf, 128, 0) > 0); i++) {}
    fcntl(sock, F_SETFL, 0);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);

    // For each host we encode the request, and send the packet.
    first = reqid;
    reqid += count;
    tries = 3;

retry:
    for (i=0; a[i].s_addr != 0; i++) {
	if (status[i] == 1)
		continue;

	pdu->setRequestId(first+i);

	len = request.encode(requestBuffer, defaultBufferSize);
	if (len == 0) {
	    bzero(pvl, sizeof *pvl);
	    return QUERY_ERR_encodeError;
	}
	
	rc = mh.sendPacket(&a[i], &request, len);
	count++;

	// Check for send error
	if (rc != len) {
		status[i] = 1;
		ErrIdx[i] = QUERY_ERR_sendError;
	}
    }

    // Wait for replys
    for (;;) {
	    rc = select(sock + 1, &fds, 0, 0, &tv);
	    // Break out of loop on timeout.
	    if (rc == 0) {
		if (--tries > 0) {
			count = 0;
			goto retry;
		} else {
			bzero(pvl, sizeof *pvl);
			return QUERY_ERR_timeout;
		}
	    }

	    // Check for error in select
	    if (rc < 0)
		continue;
	    
	    // Receive the response
	    response = new snmpPacket;

	    response->setAsn(responseBuffer, defaultBufferSize);
	    rc = mh.recv(0, response, defaultBufferSize);
	    if (rc < 0) {
		delete response;
		bzero(pvl, sizeof *pvl);
		return QUERY_ERR_recvError;
	    }

	    // Decode the response
	    len = response->decode();
	    if (len == 0) {
		delete response;
		continue;
	    }

	    // Pull out response PDU
	    snmpPDU *rpdu = (snmpPDU *) response->getPDU();
	    i = rpdu->getRequestId() - first;

	    // Continue if this was from a previous call, or duplicate.
	    if ((i < 0) || status[i]) {
		delete response;
		continue;
	    }

	    // Fill in response values
	    for (varBindList *vbl = rpdu->getVarList()->getNext();
		vbl != 0; vbl = rpdu->getVarList()->getNext()) {
		varBind *v = vbl->getVar();
		rpdu->getVarList()->removeVar(v, 0);
		result[i]->appendVar(v);
	    }

	    rc = rpdu->getErrorStatus();
	    if (rc) {
		ErrIdx[i] = rc;
		rstatus = SNMP_ERR_genErr;
	    } else {
		ErrIdx[i] = SNMP_ERR_noError;
	    }

	    delete response;
	    
	    status[i] = 1;
	    if (--count <= 0)
		break;
    }

    bzero(pvl, sizeof *pvl);
    return rstatus;
}
