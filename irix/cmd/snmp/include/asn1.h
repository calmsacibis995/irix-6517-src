#ifndef __ASN1_H__
#define __ASN1_H__
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	ASN.1 Definitions
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

#include <oid.h>

#define TAG_INTEGER		0x02
#define TAG_OCTET_STRING	0x04
#define TAG_NULL		0x05
#define TAG_OBJECT_IDENTIFIER	0x06
#define TAG_SEQUENCE		0x30
#define TAG_APPLICATION		0x40
#define TAG_CHOICE		0xA0

#define LENGTH_LONG_FORMAT	0x80


/*
 * The base ASN.1 object class.
 */
class asnObject {
public:
	asnObject(void);
	virtual ~asnObject(void);

	// Encode and decode an ASN.1 length field.  Both return the number
	// of bytes of the ASN.1 string used.
	int encodeLength(char **p, unsigned int len);
	int decodeLength(char **p, unsigned int &len);

	// Set a pointer to the ASN.1 encoding (or buffer for decoding), with
	// a maximum length of len.
	inline void setAsn(char *a, unsigned int len);

	// Get a pointer to the ASN.1 encoding
	inline char *getAsn(void);

	// Return the ASN.1 tag
	unsigned int getTag(void);

	// Set/Get the value of the object
	void setValue(void);
	void getValue(void);

	// Assignment of asnObjects.  Only the tag and value are assigned.
	inline asnObject &operator=(asnObject &);

	// Set from an asnObject.
	virtual void setValue(asnObject *a);

	// Encode the object into ASN.1 in the buffer pointer to by a with a
	// length of len, or in the space passed by a previous setAsn() call.
	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);

	// Decode the object from the ASN.1 string pointer to by a with a
	// length of len, or from the string passed by a previous setAsn() call.
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// dup() will return a new asnObject that is a copy of this object.
	// Only the tag and value are copied.  The new object will not be
	// deleted on destruction of this class instance.
	virtual asnObject *dup(void);

	// getString() will return a new char[] that will not be deleted
	// on destruction of the class instance.
	virtual char *getString(void);

	// Public class variables
	char *asn;
	unsigned int asnLength;
	unsigned int tag;
	void *value;
};

class asnInteger : public asnObject {
public:
	asnInteger(void);
	asnInteger(int i);
	virtual ~asnInteger(void);

	inline void setValue(int i);
	inline asnInteger &operator=(asnInteger &a);
	inline asnInteger &operator=(int i);

	inline int getValue(void);
	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);
	virtual asnObject *dup(void);

	// getString() will return a new char[] that will not be deleted
	// on destruction of the class instance.
	virtual char *getString(void);
};

// This is for 64 bit, since sizes of Integer and Long are not same
//
class asnLong : public asnObject {
public:
	asnLong(void);
	asnLong(long i);
	virtual ~asnLong(void);

	inline void setValue(long i);
	inline asnLong &operator=(asnInteger &a);
	inline asnLong &operator=(asnLong &a);
	inline asnLong &operator=(long i);

	inline long getValue(void);
	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);
};

class asnOctetString : public asnObject {
public:
	asnOctetString(void);
	asnOctetString(const char *s);
	asnOctetString(const char *s, unsigned int len);
	asnOctetString(asnOctetString &a);

	virtual ~asnOctetString(void);

	// setValue() will delete any char[] that was allocated by
	// another method of the class.
	void setValue(const char *s);
	void setValue(const char *s, unsigned int len);
	asnOctetString &operator=(asnOctetString &a);
	inline asnOctetString &operator=(const char *s);

	inline char *getValue(void);
	inline unsigned int getLength(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);

	// decode() will create a new char[] if none is provided from a
	// previous setValue() call.  It will be deleted by a subsequent
	// call to setValue() or by the class destructor.
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// dup() will create a new char[] for the new class instance.
	// It will be deleted by a subsequent call to setValue or by
	// the class destructor.
	virtual asnObject *dup(void);

	// getString() will return a new char[] that will not be deleted
	// on destruction of the class instance.
	virtual char *getString(void);

private:
	unsigned int	stringLength;
	unsigned int	newString;
};

class asnNull : public asnObject {
public:
	virtual ~asnNull(void);

	void setValue(void);
	inline void getValue(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);
	virtual asnObject *dup(void);

	// getString() will return a new char[] that will not be deleted
	// on destruction of the class instance.
	virtual char *getString(void);
};

class oid;

class asnObjectIdentifier : public asnObject {
public:
	asnObjectIdentifier(void);
	asnObjectIdentifier(const char *s);
	asnObjectIdentifier(oid *o);
	asnObjectIdentifier(asnObjectIdentifier &a);

	virtual ~asnObjectIdentifier(void);

	// setValue() will delete any char[] that was allocated by another
	// method of the class.  setValue(char *) will create a new oid that
	// will be deleted by a subsequent call to setValue() or by the
	// class destructor.
	void setValue(const char *s);
	void setValue(oid *o);
	inline asnObjectIdentifier &operator=(const char *s);
	inline asnObjectIdentifier &operator=(oid *o);
	asnObjectIdentifier &operator=(asnObjectIdentifier &a);

	inline oid *getValue(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);

	// decode() will create a new oid if none is provided from a
	// previous setValue() call.  It will be deleted by a subsequent
	// call to setValue() or by the class destructor.
	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// dup() will create a new oid for the new class instance.
	// It will be deleted by a subsequent call to setValue() or by
	// the class destructor.
	virtual asnObject *dup(void);

	// getString() will return a new char[] that will not be deleted
	// on destruction of the class instance.
	virtual char *getString(void);

private:
	unsigned int newOid;
};

class asnSequence : public asnObject {
public:
	asnSequence(void);
	asnSequence(unsigned int l);
	virtual ~asnSequence(void);

	void setValue(unsigned int l);
	inline unsigned int getValue(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);

	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// getString() will return a new char[] that will not be deleted
	// on destruction of the class instance.
	virtual char *getString(void);
};

class asnChoice : public asnObject {
public:
	asnChoice(void);
	asnChoice(unsigned int c, unsigned int l);
	virtual ~asnChoice(void);

	// Set the ASN.1 tag for the choice
	void setTag(unsigned int c);

	// Set the tag and the value
	void setValue(unsigned int c, unsigned int l);
	inline unsigned int getValue(void);

	virtual void setValue(asnObject *a);

	virtual int encode(void);
	virtual int encode(char *a, unsigned int len);

	virtual int decode(void);
	virtual int decode(char *a, unsigned int len);

	// getString() will return a new char[] that will not be deleted
	// on destruction of the class instance.
	virtual char *getString(void);
};

// Inline Function
void
asnObject::setAsn(char *a, unsigned int len)
{
    asn = a;
    asnLength = len;
}

char *
asnObject::getAsn(void)
{
    return asn;
}

asnObject &
asnObject::operator=(asnObject &a)
{
    tag = a.tag;
    value = a.value;
    return *this;
}

asnInteger &
asnInteger::operator=(asnInteger &a)
{
    tag = a.tag;
    value = a.value;
    return *this;
}

asnInteger &
asnInteger::operator=(int i)
{
    long tmp = (long) i;
    value = (void *) tmp;
    tag = TAG_INTEGER;
    return *this;
}

int
asnInteger::getValue(void)
{
    long tmp = (long) value;
    return (int) tmp;
}

void
asnInteger::setValue(int i)
{
    long tmp = (long) i;
    value = (void *) tmp;
    tag = TAG_INTEGER;
}


asnLong &
asnLong::operator=(asnInteger &a)
{
    tag = a.tag;
    value = a.value;
    return *this;
}

asnLong &
asnLong::operator=(asnLong &a)
{
    tag = a.tag;
    value = a.value;
    return *this;
}

asnLong &
asnLong::operator=(long i)
{
    value = (void *) i;
    tag = TAG_INTEGER;
    return *this;
}

long
asnLong::getValue(void)
{
    return (long) value;
}

void
asnLong::setValue(long i)
{
    value = (void *) i;
    tag = TAG_INTEGER;
}

asnOctetString &
asnOctetString::operator=(const char *s)
{
    setValue(s);
    return *this;
}

char *
asnOctetString::getValue(void)
{
    return (char *) value;
}

unsigned int
asnOctetString::getLength(void)
{
    return stringLength;
}

void
asnNull::getValue(void)
{
}

asnObjectIdentifier &
asnObjectIdentifier::operator=(const char *s)
{
    setValue(s);
    return *this;
}

asnObjectIdentifier &
asnObjectIdentifier::operator=(oid *o)
{
    setValue(o);
    return *this;
}

oid *
asnObjectIdentifier::getValue(void)
{
    return (oid *) value;
}

unsigned int
asnSequence::getValue(void)
{
    unsigned long tmp = (unsigned long) value;
    return (unsigned int) tmp;
}

unsigned int
asnChoice::getValue(void)
{
    unsigned long tmp = (unsigned long) value;
    return (unsigned int) tmp;
}
#endif /* !__ASN1_H__ */
