#ifndef __PACKET_H__
#define __PACKET_H__
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Message Definitions
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

#include <pdu.h>

#define	SNMP_version	0


class snmpPacket : public asnObject {
public:
	snmpPacket(void);
	snmpPacket(char *c, asnObject *a);

	// The destructor will delete the PDU created by another method of
	// the class.
	virtual ~snmpPacket(void);

	inline int getVersion(void);

	inline void setCommunity(char *c);
	inline void setCommunity(asnOctetString *o);
	inline asnOctetString *getCommunity(void);

	void setPDU(asnObject *a);
	inline asnObject *getPDU(void);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);

	// decode() will create a new PDU of the appropriate type.
	// This PDU will be deleted by subsequent calls to decode(),
	// setPDU(), or the class destructor.
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// getString(void) will create and return a new char[] that will not be
	// deleted on destruction of the class instance.
	virtual char *getString(void);

private:
	asnInteger version;
	asnOctetString community;
	asnObject *pdu;
	unsigned int newPdu;
};

// Inline functions
void
snmpPacket::setCommunity(char *c)
{
    community = c;
}

void
snmpPacket::setCommunity(asnOctetString *o)
{
    community = *o;
}

int
snmpPacket::getVersion(void)
{
    return version.getValue();
}

asnOctetString *
snmpPacket::getCommunity(void)
{
    return &community;
}

asnObject *
snmpPacket::getPDU(void)
{
    return pdu;
}
#endif /* !__PACKET_H__ */
