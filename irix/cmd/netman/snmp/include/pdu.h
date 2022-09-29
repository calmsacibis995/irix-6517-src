#ifndef __PDU_H__
#define __PDU_H__
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	PDU Definitions
 *
 *	$Revision: 1.7 $
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

#include <snmp.h>

/*
 * An SNMP variable binding list class.
 */
class varBind : public asnObject {
public:
	varBind(void);
	varBind(asnObjectIdentifier *o, asnObject *a = 0);

	// varBind(char *) will create a new object identifier.
	varBind(char *s, asnObject *a = 0);

	// If a new object identifier or objectValue is created by
	// varBind(char *), setObject(char *), or decode() methods, it
	// will be deleted in the destructor. 
	virtual ~varBind(void);

	// setObject(char *) will create a new object identifier.
	void setObject(char *s);
	void setObject(asnObjectIdentifier *o, int delLater = 0);

	void setObjectValue(asnObject *a, int delLater = 0);

	inline asnObjectIdentifier *getObject(void);
	inline asnObjectIdentifier *moveObject(void);

	inline asnObject *getObjectValue(void);
	inline asnObject *moveObjectValue(void);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);

	// decode() will create a new object identifier and the objectValue
	// class instance corresponding to the ASN.1 tag decoded if neither
	// is provided by previous setObject(), setObjectValue() calls.
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// getString(void) will create and return a new char[] that will not be
	// deleted on destruction of the class instance.  The form of the
	// string is "object = objectValue".
	virtual char *getString(void);

private:
	asnObjectIdentifier *object;
	asnObject *objectValue;
	unsigned short newObject;
	unsigned short newObjectValue;
};

/*
 * A list of class varBind.
 */
class varBindList : public asnObject {
public:
	varBindList(void);
	virtual ~varBindList(void);

	// Append the varBind to the end of the list.
	void appendVar(varBind *v);

	// Remove the varBind from the list.  Set destroy to delete varBind.
	void removeVar(varBind *v, int destroy = 1);

	// Clear the rest of the varBindList.  Set destroy to delete varBinds.
	void clear(int destroy = 1);

	inline varBind *getVar(void);
	inline varBindList *getNext(void);

	// Encodes the varBind list starting with next.
	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);

	// Decodes the varBind list starting with next.  decode() will
	// create a new varBind for each variable and a new varBindList
	// if necessary.  Neither will be deleted on the destruction of
	// this varBindList instance.
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// getString(void) will create and return a new char[] that will not be
	// deleted on destruction of the class instance.  The form of the
	// string is lines of "object = objectValue;".
	virtual char *getString(void);

private:
	varBind *var;
	varBindList *next;
};

/*
 * An SNMP PDU.
 */
class snmpPDU : public asnObject {
public:
	snmpPDU(void);

	// The destructor will call varList.clear() to remove and delete
	// any variable that remain on the list.  If these variables should
	// not be deleted, remove them from the list before destroying the PDU.

	virtual ~snmpPDU(void);

	inline void setRequestId(int i);
	inline int getRequestId(void);

	inline void setErrorStatus(int i);
	inline int getErrorStatus(void);

	inline void setErrorIndex(int i);
	inline int getErrorIndex(void);

	inline void setType(unsigned int t);
	inline unsigned int getType(void);

	inline void appendVar(varBind *v);
	inline varBindList *getVarList(void);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);

	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// getString(void) will create and return a new char[] that will not be
	// deleted on destruction of the class instance.
	virtual char *getString(void);

private:
	unsigned int type;
	asnInteger requestId;
	asnInteger errorStatus;
	asnInteger errorIndex;
	varBindList varList;
};

/*
 * An SNMP GetRequest PDU.
 */
class getRequestPDU : public snmpPDU {
public:
	getRequestPDU(void);
	virtual ~getRequestPDU(void);
};

/*
 * An SNMP GetNextRequest PDU.
 */
class getNextRequestPDU : public snmpPDU {
public:
	getNextRequestPDU(void);
	virtual ~getNextRequestPDU(void);
};

/*
 * An SNMP GetResponse PDU.
 */
class getResponsePDU : public snmpPDU {
public:
	getResponsePDU(void);
	virtual ~getResponsePDU(void);
};

/*
 * An SNMP SetRequest PDU.
 */
class setRequestPDU : public snmpPDU {
public:
	setRequestPDU(void);
	virtual ~setRequestPDU(void);
};

/*
 * An SNMP Trap PDU.
 */
class trapPDU : public asnObject {
private:
	asnObjectIdentifier	enterprise;
	snmpIPaddress		agentAddress;
	asnInteger		trapType;
	asnInteger		specificTrap;
	snmpTimeticks		timeStamp;
	varBindList		varList;

public:
	trapPDU(void);

	// The destructor will call varList.clear() to remove and delete
	// any variable that remain on the list.  If this variables should
	// not be deleted, remove them from the list before destroying the PDU.

	virtual ~trapPDU(void);

	void		setEnterprise(char *e)
				{ enterprise.setValue(e); }
	void		setAgentAddress(unsigned int a)
				{ agentAddress.setValue(a); }
	void		setTrapType(int t)
				{ trapType.setValue(t); }
	void		setSpecificTrap(int t)
				{ specificTrap.setValue(t); }
	void		setTimeStamp(int t)
				{ timeStamp.setValue(t); }

	asnObjectIdentifier *	getEnterprise(void)
					{ return &enterprise; }
	unsigned int	getAgentAddress(void)
				{ return agentAddress.getValue(); }
	int		getTrapType(void)
				{ return trapType.getValue(); }
	int		getSpecificTrap(void)
				{ return specificTrap.getValue(); }
	int		getTimeStamp(void)
				{ return timeStamp.getValue(); }
	void		appendVar(varBind *v)
				{ varList.appendVar(v); }

	virtual int	encode(void);
	virtual int	encode(char *a, unsigned int len);

	virtual int	decode(void);
	virtual int	decode(char *a, unsigned int len);

	// getString(void) will create and return a new char[] that will not be
	// deleted on destruction of the class instance.

	virtual char *	getString(void);
};

// Inline functions
asnObjectIdentifier *
varBind::getObject(void)
{
    return object;
}

asnObjectIdentifier *
varBind::moveObject(void) {
    newObject = 0;
    asnObjectIdentifier *tmp = object;
    object= 0;
    return tmp;
}

asnObject *
varBind::moveObjectValue(void) {
    newObjectValue = 0;
    asnObject *tmp = objectValue;
    objectValue = 0;
    return tmp;
}

asnObject *
varBind::getObjectValue(void)
{
    return objectValue;
}

varBind *
varBindList::getVar(void)
{
    return var;
}

varBindList *
varBindList::getNext(void)
{
    return next;
}

void
snmpPDU::setRequestId(int i)
{
    requestId.setValue(i);
}

void
snmpPDU::setErrorStatus(int i)
{
    errorStatus.setValue(i);
}

void
snmpPDU::setErrorIndex(int i)
{
    errorIndex.setValue(i);
}

void
snmpPDU::setType(unsigned int t)
{
    type = t;
}

void
snmpPDU::appendVar(varBind *v)
{
    varList.appendVar(v);
}

int
snmpPDU::getRequestId(void)
{
    return requestId.getValue();
}

int
snmpPDU::getErrorStatus(void)
{
    return errorStatus.getValue();
}

int
snmpPDU::getErrorIndex(void)
{
    return errorIndex.getValue();
}

varBindList *
snmpPDU::getVarList(void)
{
    return &varList;
}

unsigned int
snmpPDU::getType(void)
{
    return type;
}
#endif /* !__PDU_H__ */
