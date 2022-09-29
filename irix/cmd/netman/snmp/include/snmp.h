#ifndef __SNMP_H__
#define __SNMP_H__
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Definitions
 *
 *	$Revision: 1.5 $
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

#include <asn1.h>

#define TAG_IPADDRESS		(TAG_APPLICATION | 0x00)
#define TAG_COUNTER		(TAG_APPLICATION | 0x01)
#define TAG_GAUGE		(TAG_APPLICATION | 0x02)
#define TAG_TIMETICKS		(TAG_APPLICATION | 0x03)
#define TAG_OPAQUE		(TAG_APPLICATION | 0x04)

#define TAG_NETADDR_IP		(TAG_CHOICE | 0x00)

#define SNMP_GetRequest		(TAG_CHOICE | 0x00)
#define SNMP_GetNextRequest	(TAG_CHOICE | 0x01)
#define SNMP_GetResponse	(TAG_CHOICE | 0x02)
#define SNMP_SetRequest		(TAG_CHOICE | 0x03)
#define SNMP_Trap		(TAG_CHOICE | 0x04)

#define SNMP_ERR_noError	0x00
#define SNMP_ERR_tooBig		0x01
#define SNMP_ERR_noSuchName	0x02
#define SNMP_ERR_badValue	0x03
#define SNMP_ERR_readOnly	0x04
#define SNMP_ERR_genErr		0x05

class snmpNetworkAddress : public asnObject {
public:
	snmpNetworkAddress(void);
	snmpNetworkAddress(const char *s);
	snmpNetworkAddress(unsigned long a);
	virtual ~snmpNetworkAddress(void);

	void setValue(unsigned long a);
	void setValue(const char *s);
#if _MIPS_SZLONG == 64
	inline snmpNetworkAddress &operator=(unsigned int a);
#else
	inline snmpNetworkAddress &operator=(unsigned long a);
#endif
	inline snmpNetworkAddress &operator=(const char *s);
	inline snmpNetworkAddress &operator=(snmpNetworkAddress &i);
	unsigned long getValue(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// dup() will return a new asnObject that is a copy of this object.
	// Only the tag and value are copied.  The new object will not be
	// deleted on destruction of this class instance.
	virtual asnObject *dup(void);

	// getString will return a new char[] that will not be deleted
	// on destruction of the class instance.
	char *getString(void);
};

class snmpIPaddress : public asnObject {
public:
	snmpIPaddress(void);
	snmpIPaddress(const char *s);
	snmpIPaddress(unsigned long a);
	virtual ~snmpIPaddress(void);

	void setValue(unsigned long a);
	void setValue(const char *s);
#if _MIPS_SZLONG == 64
	inline snmpIPaddress &operator=(unsigned int a);
#else
	inline snmpIPaddress &operator=(unsigned long a);
#endif
	inline snmpIPaddress &operator=(const char *s);
	inline snmpIPaddress &operator=(snmpIPaddress &i);
	unsigned long getValue(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// dup() will return a new asnObject that is a copy of this object.
	// Only the tag and value are copied.  The new object will not be
	// deleted on destruction of this class instance.
	virtual asnObject *dup(void);

	// getString will return a new char[] that will not be deleted
	// on destruction of the class instance.
	char *getString(void);
};

class snmpCounter : public asnObject {
public:
	snmpCounter(void);
	snmpCounter(unsigned int i);
	virtual ~snmpCounter(void);

	void setValue(unsigned int i);
	inline snmpCounter &operator=(unsigned int i);
	inline snmpCounter &operator=(snmpCounter &c);
	unsigned int getValue(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// dup() will return a new asnObject that is a copy of this object.
	// Only the tag and value are copied.  The new object will not be
	// deleted on destruction of this class instance.
	virtual asnObject *dup(void);

	// getString will return a new char[] that will not be deleted
	// on destruction of the class instance.
	char *getString(void);
};

class snmpGauge : public asnObject {
public:
	snmpGauge(void);
	snmpGauge(unsigned int i);
	virtual ~snmpGauge(void);

	void setValue(unsigned int i);
	inline snmpGauge &operator=(unsigned int i);
	inline snmpGauge &operator=(snmpGauge &g);
	unsigned int getValue(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// dup() will return a new asnObject that is a copy of this object.
	// Only the tag and value are copied.  The new object will not be
	// deleted on destruction of this class instance.

	virtual asnObject *dup(void);

	// getString will return a new char[] that will not be deleted
	// on destruction of the class instance.

	char *getString(void);
};

class snmpTimeticks : public asnObject {
public:
	snmpTimeticks(void);
	snmpTimeticks(int i);
	virtual ~snmpTimeticks(void);

	void setValue(int i);
	inline snmpTimeticks &operator=(int i);
	inline snmpTimeticks &operator=(snmpTimeticks &t);
	int getValue(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// dup() will return a new asnObject that is a copy of this object.
	// Only the tag and value are copied.  The new object will not be
	// deleted on destruction of this class instance.
	virtual asnObject *dup(void);

	// getString will return a new char[] that will not be deleted
	// on destruction of the class instance.
	char *getString(void);
};

// Inline functions
snmpNetworkAddress &
#if _MIPS_SZLONG == 64
snmpNetworkAddress::operator=(unsigned int a)
#else
snmpNetworkAddress::operator=(unsigned long a)
#endif
{
    setValue(a);
    return *this;
}

snmpNetworkAddress &
snmpNetworkAddress::operator=(const char *s)
{
    setValue(s);
    return *this;
}

snmpNetworkAddress &
snmpNetworkAddress::operator=(snmpNetworkAddress &i)
{
    return *this = i.getValue();
}

snmpIPaddress &
#if _MIPS_SZLONG == 64
snmpIPaddress::operator=(unsigned int a)
#else
snmpIPaddress::operator=(unsigned long a)
#endif
{
    setValue(a);
    return *this;
}

snmpIPaddress &
snmpIPaddress::operator=(const char *s)
{
    setValue(s);
    return *this;
}

snmpIPaddress &
snmpIPaddress::operator=(snmpIPaddress &i)
{
    return *this = i.getValue();
}

snmpCounter &
snmpCounter::operator=(unsigned int i)
{
    setValue(i);
    return *this;
}

snmpCounter &
snmpCounter::operator=(snmpCounter &c)
{
    return *this = c.getValue();
}

snmpGauge &
snmpGauge::operator=(unsigned int i)
{
    setValue(i);
    return *this;
}

snmpGauge &
snmpGauge::operator=(snmpGauge &g)
{
    return *this = g.getValue();
}

snmpTimeticks &
snmpTimeticks::operator=(int i)
{
    setValue(i);
    return *this;
}

snmpTimeticks &
snmpTimeticks::operator=(snmpTimeticks &t)
{
    return *this = t.getValue();
}
#endif /* !__SNMP_H__ */
