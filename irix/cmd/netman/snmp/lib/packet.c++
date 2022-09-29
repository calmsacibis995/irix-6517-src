/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Packet Class
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
#include <libc.h>
#include <string.h>
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"


snmpPacket::snmpPacket(void) : version(SNMP_version)
{
    pdu = 0;
    newPdu = 0;
}

snmpPacket::snmpPacket(char *c, asnObject *a) : version(SNMP_version)
{
    pdu = 0;
    newPdu = 0;
    setCommunity(c);
    setPDU(a);
}

snmpPacket::~snmpPacket(void)
{
    if (newPdu)
	delete pdu;
}

void
snmpPacket::setPDU(asnObject *a)
{
    if (newPdu)
	delete pdu;
    pdu = a;
    newPdu = 0;
}

int
snmpPacket::encode(void)
{
    if (pdu == 0)
	return 0;

    const int buflen = 4096;
    char buf[buflen];

    int totalLength = version.encode(buf, buflen);
    if (totalLength == 0)
	return 0;

    int length = community.encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    length = pdu->encode(buf + totalLength, buflen - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    // Now encode sequence and copy the buffer
    asnSequence s(totalLength);
    length = s.encode(asn, asnLength);
    if (length == 0 || totalLength + length > asnLength)
	return 0;
    bcopy(buf, asn + length, totalLength);
    return totalLength + length;
}

int
snmpPacket::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
snmpPacket::decode(void)
{
    // Decode the sequence
    asnSequence s;
    int length = s.decode(asn, asnLength);
    if (length == 0 || asnLength - length < s.getValue())
	return 0;
    int totalLength = length;
 
    length = version.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    if (version.getValue() != SNMP_version)
	return 0;
    totalLength += length;

    length = community.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    // Find which PDU type
    asnChoice c;
    length = c.decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;

    if (newPdu)
	delete pdu;

    switch (c.getTag()) {
	case SNMP_GetRequest:
	    pdu = new getRequestPDU();
	    break;
	case SNMP_GetNextRequest:
	    pdu = new getNextRequestPDU();
	    break;
	case SNMP_GetResponse:
	    pdu = new getResponsePDU();
	    break;
	case SNMP_SetRequest:
	    pdu = new setRequestPDU();
	    break;
	case SNMP_Trap:
	    pdu = new trapPDU();
	    break;
	default:
	    return 0;
    }
    newPdu = 1;

    length = pdu->decode(asn + totalLength, asnLength - totalLength);
    if (length == 0)
	return 0;
    totalLength += length;

    return totalLength + length;
}

int
snmpPacket::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

char *
snmpPacket::getString(void)
{
    const int stringlen = 4096;
    char *s = new char[stringlen];
    char *sp = s;

    char *vs = version.getString();
    char *cs = community.getString();
    sprintf(sp, "{\n\tversion = %s;\n\tcommunity = \"%s\";\n", vs, cs);
    delete vs;
    delete cs;
    sp += strlen(sp);
    if (pdu != 0) {
	// XXX - broken for trap
	switch (((snmpPDU *)pdu)->getType()) {
	    case SNMP_GetRequest:
		vs = "\tGetRequest = {\n\t\t";
		break;
	    case SNMP_GetNextRequest:
		vs = "\tGetNextRequest = {\n\t\t";
		break;
	    case SNMP_SetRequest:
		vs = "\tSetRequest = {\n\t\t";
		break;
	    case SNMP_GetResponse:
		vs = "\tGetResponse = {\n\t\t";
		break;
	    default:
		vs = "\tPDU = {\n\t\t";
		break;
	}
	strcpy(sp, vs);
	sp += strlen(vs);
	char *ps = pdu->getString();
	for (char *p = ps; *p != 0; *sp++ = *p++) {
	    if (*p == '\n') {
		*sp++ = '\n';
		*sp++ = '\t';
		*p = '\t';
	    }
	}
	strcpy(sp, "\n\t};\n");
	sp += strlen(sp);
	delete ps;
    }
    strcpy(sp, "\n};");
    return s;
}
