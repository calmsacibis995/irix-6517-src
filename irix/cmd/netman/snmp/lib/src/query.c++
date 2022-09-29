/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Query Handler
 *
 *	$Revision: 1.4 $
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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <bstring.h>
#include <string.h>
#include <osfcn.h>
#include <ctype.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"
#include "message.h"
#include "query.h"

const unsigned int defaultBufferSize = 8192;
const unsigned int defaultTries = 3;
const unsigned int defaultInterval = 3;
const char *defaultCommunity = "public";

extern "C" {
char *ether_aton(const char *);
}


snmpQueryHandler::snmpQueryHandler(void)
{
    sock = mh.open();
    requestBuffer = 0;
    responseBuffer = 0;
    community = 0;
    tries = defaultTries;
    interval = defaultInterval;
    reqid = 0;
}

snmpQueryHandler::~snmpQueryHandler(void)
{
    (void) mh.close();
    if (requestBuffer != 0)
	delete requestBuffer;
    if (responseBuffer != 0)
	delete responseBuffer;
}

int
snmpQueryHandler::get(const char *dst, const varBindList *arg,
		      varBindList *result)
{
    struct hostent *he = gethostbyname(dst);
    if (he == 0)
	return QUERY_ERR_hostnameError;

    return get((struct in_addr *) he->h_addr, arg, result);
}

int
snmpQueryHandler::get(const struct in_addr *a, const varBindList *arg,
		      varBindList *result)
{
    getRequestPDU pdu;
    return query(a, arg, result, &pdu);
}

int
snmpQueryHandler::get(const struct in_addr *a, const varBindList *arg,
		      varBindList *result, int *reqIdx,  int reqID)
{
    getRequestPDU pdu;
    return query(a, arg, result, &pdu, reqID, reqIdx);
}

int
snmpQueryHandler::getNext(const char *dst, const varBindList *arg,
			  varBindList *result)
{
    struct hostent *he = gethostbyname(dst);
    if (he == 0)
	return QUERY_ERR_hostnameError;

    return getNext((struct in_addr *) he->h_addr, arg, result);
}

int
snmpQueryHandler::getNext(const struct in_addr *a, const varBindList *arg,
			  varBindList *result)
{
   getNextRequestPDU pdu;
   return query(a, arg, result, &pdu); 
}

int
snmpQueryHandler::getNext(const struct in_addr *a, const varBindList *arg,
			  varBindList *result, int *reqIdx, int reqID)
{
    getNextRequestPDU pdu;
    return query(a, arg, result, &pdu, reqID, reqIdx);
}

int
snmpQueryHandler::set(const char *dst, const varBindList *arg,
		      varBindList *result)
{
    struct hostent *he = gethostbyname(dst);
    if (he == 0)
	return QUERY_ERR_hostnameError;

    return set((struct in_addr *) he->h_addr, arg, result);
}

int
snmpQueryHandler::set(const struct in_addr *a, const varBindList *arg,
		      varBindList *result)
{
    setRequestPDU pdu;
    return query(a, arg, result, &pdu);
}

int
snmpQueryHandler::set(const struct in_addr *a, const varBindList *arg,
		      varBindList *result, int *reqIdx, int reqID)
{
    setRequestPDU pdu;
    return query(a, arg, result, &pdu, reqID, reqIdx);
}

int
snmpQueryHandler::get(const char *dst, const char *objid, varBind **value)
{
    struct hostent *he = gethostbyname(dst);
    if (he == 0)
	return QUERY_ERR_hostnameError;

    return get((struct in_addr *) he->h_addr, objid, value);
}

int
snmpQueryHandler::get(const struct in_addr *a, const char *objid,
		      varBind **value)
{
    getRequestPDU pdu;
    return queryOne(a, objid, 0, value, &pdu);
}

int
snmpQueryHandler::getNext(const char *dst, const char *objid, varBind **value)
{
    struct hostent *he = gethostbyname(dst);
    if (he == 0)
	return QUERY_ERR_hostnameError;

    return getNext((struct in_addr *) he->h_addr, objid, value);
}

int
snmpQueryHandler::getNext(const struct in_addr *a, const char *objid,
			  varBind **value)
{
    getNextRequestPDU pdu;
    return queryOne(a, objid, 0, value, &pdu);
}

int
snmpQueryHandler::set(const char *dst, const char *objid, const char *objval,
		      varBind **value)
{
    struct hostent *he = gethostbyname(dst);
    if (he == 0)
	return QUERY_ERR_hostnameError;

    return set((struct in_addr *) he->h_addr, objid, objval, value);
}

int
snmpQueryHandler::set(const struct in_addr *a, const char *objid,
		      const char *objval, varBind **value)
{
    setRequestPDU pdu;
    return queryOne(a, objid, objval, value, &pdu);
}

int
snmpQueryHandler::set(const char *dst, const char *objid,
		      const asnObject *objval, varBind **value)
{
    struct hostent *he = gethostbyname(dst);
    if (he == 0)
	return QUERY_ERR_hostnameError;

    return set((struct in_addr *) he->h_addr, objid, objval, value);
}

int
snmpQueryHandler::set(const struct in_addr *a, const char *objid,
		      const asnObject *objval, varBind **value)
{
    setRequestPDU pdu;
    varBindList arg, result;

    // Create argument varBindList
    varBind *v = new varBind((char *) objid, (asnObject *) objval);
    arg.appendVar(v);

    // Query
    int rc = query(a, &arg, &result, &pdu);

    // Clear argument varBindList
    arg.clear();

    // On query error, don't touch result
    if (rc <= SNMP_ERR_genErr) {
	if (result.getNext() == 0)
	    *value = 0;
	else {
	    *value = result.getNext()->getVar();
	    result.removeVar(*value, 0);
	}
    }

    result.clear();
    return rc;
}

int
snmpQueryHandler::queryOne(const struct in_addr *a, const char *objid,
			   const char *objval, varBind **value, snmpPDU *pdu)
{
    asnObject *val;
    varBindList arg, result;

    // Create value object
    if (objval == 0 || strlen(objval) == 0)
	val = new asnNull;
    else {
	val = 0;
	for (const char *s = objval; *s != 0; s++) {
	    if (!isdigit(*s)) {
		if (*s == '.') {
		    if (s == objval)
			val = new asnObjectIdentifier(objval + 1);
		    else
			val = new snmpIPaddress(objval);
		} else if (*s == ':')
		    val = new asnOctetString(ether_aton(objval), 6);
		else
		    val = new asnOctetString(objval);
		break;
	    }
	}
	if (val == 0)
	    val = new asnInteger(atoi(objval));
    }

    // Create argument varBindList
    varBind *v = new varBind((char *) objid, val);
    arg.appendVar(v);

    // Query
    int rc = query(a, &arg, &result, pdu);

    // Clear argument varBindList
    arg.clear();
    delete val;

    // On query error, don't touch result
    if (rc <= SNMP_ERR_genErr) {
	if (result.getNext() == 0)
	    *value = 0;
	else {
	    *value = result.getNext()->getVar();
	    result.removeVar(*value, 0);
	}
    }

    result.clear();
    return rc;
}

int
snmpQueryHandler::query(const struct in_addr *a, const varBindList *arg,
			varBindList *result, snmpPDU *pdu, int reqID,
			int *errIdx)
{
    snmpPacket request;
    snmpPacket response;
    int rc;

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
    if (reqID == 0)
	pdu->setRequestId(reqid++);
    else
	pdu->setRequestId(reqID);
    pdu->setErrorStatus(0);
    pdu->setErrorIndex(0);

    // Encode the request
    int len = request.encode(requestBuffer, defaultBufferSize);
    if (len == 0)
	return QUERY_ERR_encodeError;

    // Remove varBindList from PDU
    bzero(pvl, sizeof *pvl);

    // Set up the timeval
    struct timeval tv;
    tv.tv_sec = interval;
    tv.tv_usec = 0;

    for (int i = tries; i != 0; i--) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	// Send the message
	rc = mh.sendPacket(a, &request, len);

	// Check for send error
	if (rc != len)
	    return QUERY_ERR_sendError;

	// Wait for reply
	rc = select(sock + 1, &fds, 0, 0, &tv);
	if (rc > 0)
	    break;

	// Check for error in select
	if (rc < 0)
	    return QUERY_ERR_recvError;

	tv.tv_sec *= 2;
    }

    // Check for timeout
    if (i == 0)
	return QUERY_ERR_timeout;

    // Receive the response
    response.setAsn(responseBuffer, defaultBufferSize);
    rc = mh.recv(0, &response, defaultBufferSize);
    if (rc < 0)
	return QUERY_ERR_recvError;

    // Decode the response
    len = response.decode();
    if (len == 0)
	return QUERY_ERR_decodeError;

    // Changed the pdu to rpdu below. The 3.15 compiler does not let
    // me re-declare it here. BUBBA
    
    // Pull out response PDU
    snmpPDU *rpdu = (snmpPDU *) response.getPDU();

    // Fill in response values
    for (varBindList *vbl = rpdu->getVarList()->getNext();
		vbl != 0; vbl = rpdu->getVarList()->getNext()) {
	varBind *v = vbl->getVar();
	rpdu->getVarList()->removeVar(v, 0);
	result->appendVar(v);
    }
    if (errIdx)
	*errIdx = rpdu->getErrorIndex();
    return rpdu->getErrorStatus();
}

void
snmpQueryHandler::setCommunity(const char *c)
{
    if (community != 0)
	delete community;
    community = new char[strlen(c) + 1];
    strcpy(community, c);
}

void
snmpQueryHandler::setTries(unsigned int t)
{
    tries = t;
}

void
snmpQueryHandler::setInterval(unsigned int i)
{
    interval = i;
}
