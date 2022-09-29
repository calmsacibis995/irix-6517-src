/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	PDU Definitions
 *
 *	$Revision: 1.2 $
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
#include <libc.h>
#include <string.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"

/*
 * Variable Binding class
 */
varBind::varBind(void)
{
    newObject = 0;
    newObjectValue = 0;
    object = 0;
    objectValue = 0;
}

varBind::varBind(asnObjectIdentifier *o, asnObject *a)
{
    newObject = 0;
    newObjectValue = 0;
    object = o;
    objectValue = a;
}

varBind::varBind(char *s, asnObject *a)
{
    newObject = 0;
    newObjectValue = 0;
    setObject(s);
    objectValue = a;
}

varBind::~varBind()
{
    if (newObject)
	delete object;
    if (newObjectValue)
	delete objectValue;
}

void
varBind::setObject(char *s)
{
    if (newObject)
	delete object;

    object = new asnObjectIdentifier(s);
    newObject = 1;
}

void
varBind::setObject(asnObjectIdentifier *o, int delLater)
{
    if (newObject)
	delete object;

    object = o;
    newObject = delLater;
}

void
varBind::setObjectValue(asnObject *a, int delLater)
{
    if (newObjectValue)
	delete objectValue;

    objectValue = a;
    newObjectValue = delLater;
}

int
varBind::encode(void)
{
    // Shortcut the common case of a two byte sequence (tag and length)
    const unsigned int offset = 2;
    const unsigned int buflen = 8;
    char buf[buflen];

    if (object == 0)
	return 0;

    // Encode into buffer leaving room for initial sequence
    char *a = asn + offset;
    unsigned int alen = asnLength - offset;
    int length = object->encode(a, alen);
    if (length == 0)
	return 0;
    int totalLength = length;

    // If objectValue is 0, send a null
    if (objectValue == 0) {
	objectValue = new asnNull;
	newObjectValue = 1;
    }

    length = objectValue->encode(a + totalLength, alen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;
    if (totalLength >= alen)
	return 0;

    // Encode sequence into a temporary buffer
    asnSequence s(totalLength);
    length = s.encode(buf, buflen);
    if (length == 0 || totalLength + length > asnLength)
	return 0;

    // Correct if shortcut failed
    if (length != offset)
	bcopy(asn + offset, asn + length, totalLength);

    // Copy in sequence
    bcopy(buf, asn, length);
    return totalLength + length;
}

int
varBind::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
varBind::decode(void)
{
    asnSequence s;
    int length = s.decode(asn, asnLength);
    if (length == 0 || asnLength - length < s.getValue())
	return 0;
    int totalLength = length;
    
    // If object is 0, allocate one
    if (object == 0) {
	object = new asnObjectIdentifier;
	newObject = 1;
    }

    length = object->decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    if (newObjectValue) {
	delete objectValue;
	objectValue = 0;
    }

    if (objectValue == 0) {
	asnObject a;
	a.setAsn(asn + totalLength, asnLength - totalLength);
	switch (a.getTag()) {
	    case TAG_INTEGER:
		objectValue = new asnInteger;
		break;
	    case TAG_OCTET_STRING:
		objectValue = new asnOctetString;
		break;
	    case TAG_NULL:
		objectValue = new asnNull;
		break;
	    case TAG_OBJECT_IDENTIFIER:
		objectValue = new asnObjectIdentifier;
		break;
	    case TAG_NETADDR_IP:
		objectValue = new snmpNetworkAddress;
		break;
	    case TAG_IPADDRESS:
		objectValue = new snmpIPaddress;
		break;
	    case TAG_COUNTER:
		objectValue = new snmpCounter;
		break;
	    case TAG_GAUGE:
		objectValue = new snmpGauge;
		break;
	    case TAG_TIMETICKS:
		objectValue = new snmpTimeticks;
		break;
	    default:
		return 0;
	}
	newObjectValue = 1;
    }
    length = objectValue->decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    return totalLength;
}

int
varBind::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

char *
varBind::getString(void)
{
    if (object == 0)
	return asnObject::getString();

    char *os = object->getString();
    char *vs;
    if (objectValue == 0) {
	asnNull n;
	vs = n.getString();
    } else
	vs = objectValue->getString();

    unsigned int l = strlen(os);
    char *s = new char[l + strlen(vs) + 4];
    bcopy(os, s, l);
    strcpy(s + l, " = ");
    strcpy(s + l + 3, vs);
    delete os;
    delete vs;
    return s;
}

/*
 * Variable Binding List
 */
varBindList::varBindList(void)
{
    var = 0;
    next = 0;
}

varBindList::~varBindList(void)
{
}

void
varBindList::appendVar(varBind *v)
{
    for (varBindList **vbl = &next; *vbl != 0; vbl = &((*vbl)->next) )
	;

    *vbl = new varBindList;
    (*vbl)->var = v;
}

void
varBindList::removeVar(varBind *v, int destroy)
{
    for (varBindList **vbl = &next;
		*vbl != 0 && (*vbl)->var != v; vbl = &((*vbl)->next) )
	;

    if (*vbl != 0) {
	varBindList *vl = *vbl;
	*vbl = vl->next;
	delete vl;
    }

    if (destroy != 0)
	delete v;
}

void
varBindList::clear(int destroy)
{
    while (next != 0) {
	varBindList *vl = next;
	next = vl->next;
	if (destroy != 0)
	    delete vl->var;
	delete vl;
    }
}

int
varBindList::encode(void)
{
    // Shortcut the common case of a two byte sequence (tag and length)
    const unsigned int offset = 2;
    const unsigned int buflen = 8;
    char buf[buflen];

    // Encode into buffer leaving room for initial sequence
    char *a = asn + offset;
    unsigned int alen = asnLength - offset;
    int length, totalLength = 0;

    // Encode the list
    for (varBindList *v = next; v != 0; v = v->next) {
	length = v->var->encode(a + totalLength, alen - totalLength);
	if (length == 0)
	    return 0;
	totalLength += length;
    }

    // Encode sequence into a temporary buffer
    asnSequence s(totalLength);
    length = s.encode(buf, buflen);
    if (length == 0 || totalLength + length > asnLength)
	return 0;

    // Correct if shortcut failed
    if (length != offset)
	memmove(asn + length, asn + offset, totalLength);

    // Copy in sequence
    bcopy(buf, asn, length);
    return totalLength + length;
}

int
varBindList::encode(char* a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
varBindList::decode(void)
{
    // Decode the sequence
    asnSequence s;
    int length = s.decode(asn, asnLength);
    if (length == 0 || asnLength - length < s.getValue())
	return 0;
    int totalLength = length;
 
    // Decode the list
    varBindList **vbl = &next;
    for (length = 0; totalLength < asnLength; totalLength += length) {
	varBind *vb = new varBind;
	length = vb->decode(asn + totalLength, asnLength - totalLength);
	if (length == 0) {
	    delete vb;
	    return totalLength;
	}
	if (*vbl == 0)
	    *vbl = new varBindList;
	else
	    (*vbl)->next = 0;
	(*vbl)->var = vb;
	vbl = &((*vbl)->next);
    }

    return totalLength;
}

int
varBindList::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

char *
varBindList::getString(void)
{
    const int stringlen = 4096;
    char *s = new char[stringlen];
    char *sp = s;
    strcpy(sp, "{\n");
    sp += strlen(sp);

    for (varBindList **vbl = &next;
		*vbl != 0 && (*vbl)->var != 0; vbl = &((*vbl)->next) ) {
	*sp++ = '\t';
	char *vs = (*vbl)->var->getString();
	strcpy(sp, vs);
	delete vs;
	sp += strlen(sp);
	*sp++ = ';';
	*sp++ = '\n';
    }
    *sp++ = '}';
    *sp++ = ';';
    *sp = '\0';
    return s;
}

/*
 * An SNMP PDU.
 */
snmpPDU::snmpPDU(void)
{
}

snmpPDU::~snmpPDU(void)
{
    varList.clear();
}

int
snmpPDU::encode(void)
{
    const int buflen = 4096;
    char buf[buflen];

    int length = requestId.encode(buf, buflen);
    if (length == 0)
	return 0;
    int totalLength = length;

    length = errorStatus.encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = errorIndex.encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = varList.encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    // Now encode choice and copy the buffer
    asnChoice c(type, totalLength);
    length = c.encode(asn, asnLength);
    if (length == 0 || totalLength + length > asnLength)
	return 0;
    bcopy(buf, asn + length, totalLength);
    return totalLength + length;
}

int
snmpPDU::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
snmpPDU::decode(void)
{
    // Decode the choice
    asnChoice c;
    int length = c.decode(asn, asnLength);
    if (length == 0 || asnLength - length < c.getValue())
	return 0;
    if (c.getTag() != type)
	return 0;
    int totalLength = length;
 
    length = requestId.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = errorStatus.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = errorIndex.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = varList.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;

    return totalLength + length;
}

int
snmpPDU::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

char *
snmpPDU::getString(void)
{
    const int stringlen = 4096;
    char *s = new char[stringlen];
    char *sp = s;
    sprintf(sp, "Request Id = %u;\n", requestId.getValue());
    sp += strlen(sp);
    sprintf(sp, "Error Status = %u;\n", errorStatus.getValue());
    sp += strlen(sp);
    sprintf(sp, "Error Index = %u;\n", errorIndex.getValue());
    sp += strlen(sp);
    sprintf(sp, "Variable List = ");
    sp += strlen(sp);

    char *vs = varList.getString();
    strcpy(sp, vs);
    delete vs;
    return s;
}

/*
 * GetRequest PDU
 */
getRequestPDU::getRequestPDU(void)
{
    setType(SNMP_GetRequest);
}

getRequestPDU::~getRequestPDU(void)
{
}

/*
 * GetNextRequest PDU
 */
getNextRequestPDU::getNextRequestPDU(void)
{
    setType(SNMP_GetNextRequest);
}

getNextRequestPDU::~getNextRequestPDU(void)
{
}

/*
 * GetResponse PDU
 */
getResponsePDU::getResponsePDU(void)
{
    setType(SNMP_GetResponse);
}

getResponsePDU::~getResponsePDU(void)
{
}

/*
 * SetRequest PDU
 */
setRequestPDU::setRequestPDU(void)
{
    setType(SNMP_SetRequest);
}

setRequestPDU::~setRequestPDU(void)
{
}

/*
 * Trap PDU
 */
trapPDU::trapPDU(void)
{
}

trapPDU::~trapPDU()
{
    varList.clear();
}

int
trapPDU::encode(void)
{
    const int buflen = 4096;
    char buf[buflen];

    int length = enterprise.encode(buf, buflen);
    if (length == 0)
	return 0;
    int totalLength = length;

    length = agentAddress.encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = trapType.encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = specificTrap.encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = timeStamp.encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = varList.encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    // Now encode choice and copy the buffer
    asnChoice c(SNMP_Trap, totalLength);
    length = c.encode(asn, asnLength);
    if (length == 0 || totalLength + length > asnLength)
	return 0;
    bcopy(buf, asn + length, totalLength);
    return totalLength + length;
}

int
trapPDU::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
trapPDU::decode(void)
{
    // Decode the choice
    asnChoice c;
    int length = c.decode(asn, asnLength);
    if (length == 0 || asnLength - length < c.getValue())
	return 0;
    if (c.getTag() != SNMP_Trap)
	return 0;
    int totalLength = length;
 
    length = enterprise.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = agentAddress.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = trapType.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = specificTrap.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = timeStamp.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = varList.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;

    return totalLength + length;
}

int
trapPDU::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

char *
trapPDU::getString(void)
{
    const int stringlen = 4096;
    char *s = new char[stringlen];
    char *sp = s;

    char *gs = enterprise.getString();
    sprintf(sp, "Enterprise = %s;\n", gs);
    sp += strlen(sp);
    delete gs;

    gs = agentAddress.getString();
    sprintf(sp, "Agent Address = %s;\n", gs);
    sp += strlen(sp);
    delete gs;

    sprintf(sp, "Trap Type = %u;\n", trapType.getValue());
    sp += strlen(sp);

    sprintf(sp, "Specific Trap = %u;\n", specificTrap.getValue());
    sp += strlen(sp);

    sprintf(sp, "Time Stamp = %u;\n", timeStamp.getValue());
    sp += strlen(sp);

    sprintf(sp, "Variable List = ");
    sp += strlen(sp);

    char *vs = varList.getString();
    for (char *p = vs; *p != 0; *sp++ = *p++) {
	if (*p == '\n') {
	    *sp++ = '\n';
	    *p = '\t';
	}
    }
    delete vs;
    return s;
}
