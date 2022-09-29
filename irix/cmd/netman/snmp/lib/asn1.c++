/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	ASN.1
 *
 *	$Revision: 1.9 $
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
#include <ctype.h>
#include "oid.h"
#include "asn1.h"
// The definition of MAXCOMMNAMELEN was taken from agent.c++
#define MAXCOMMNAMELEN 256


/*
 * ASN object base class
 */
asnObject::asnObject(void)
{
    asn = 0;
    asnLength = 0;
    setValue();
}

asnObject::~asnObject(void)
{
}

void
asnObject::setValue(void)
{
    value = 0;
    tag = TAG_NULL;
}

void
asnObject::setValue(asnObject *a)
{
    tag = a->tag;
    value = a->value;
}

void
asnObject::getValue(void)
{
}

unsigned int
asnObject::getTag(void)
{
    if (asn != 0 && tag == TAG_NULL)
	tag = *asn;
    return tag;
}

int
asnObject::encode(void)
{
    return 0;
}

int
asnObject::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
asnObject::decode(void)
{
    return 0;
}

int
asnObject::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
asnObject::dup(void)
{
    asnObject *a = new asnObject();
    *a = *this;
    return a;
}

char *
asnObject::getString(void)
{
    char *s = new char[4];
    s[0] = 0;
    return s;
}

/*
 * Length encoding rules:
 *
 *    A length of 0 to 127 may be encoded as a single byte; that is,
 *	bit 8 is zero and bits 1 to 7 are the value.
 *
 *    A length longer that 127 comprises of a bytes to say how many bytes make
 *	up the length, followed by those bytes.  The first byte has bit 8 set
 *	and bits 1 to 7 as the byte count.  The subsequent bytes contain the
 *	length, most significant byte first.
 */
int
asnObject::encodeLength(char **p, unsigned int len)
{
    // Try to just send 1 byte
    if (len < LENGTH_LONG_FORMAT) {
	*(*p)++ = len;
	return 1;
    }

    // Have to do it the long way
    char *llen = *p;
    unsigned int size = sizeof(int);
    unsigned int shift = 8 * (size - 1);
    unsigned int mask = 0xFF << shift;
    while ((mask & len) == 0) {
	len <<= 8;
	size--;
    }
    if (size >= asnLength)
	return 0;
    for (*(*p)++ = 0; size != 0; len <<= 8, (*llen)++, size--)
	*(*p)++ = len >> shift;

    *llen |= LENGTH_LONG_FORMAT;
    unsigned long tmp = (unsigned long)*p - (unsigned long)llen;
    return (unsigned int) tmp;
}

int
asnObject::decodeLength(char **p, unsigned int& len)
{
    char *start = *p;
    len = *(*p)++;
    if (!(len & LENGTH_LONG_FORMAT))
	return 1;

    len &= ~LENGTH_LONG_FORMAT;
    for (unsigned int size = sizeof(int); len > size; len--) {
	if (*(*p)++ != 0)
	    return 0;		// Too big!
    }
    for (unsigned int result = 0; len != 0; len--)
	result = (result << 8) | *(*p)++;
    len = result;

    unsigned long tmp = (unsigned long) *p - (unsigned long) start;
    return (unsigned int) tmp;
}

/*
 * ASN long class - for 64 bit
 */
asnLong::asnLong(void)
{
}

asnLong::asnLong(long i)
{
    setValue(i);
}

asnLong::~asnLong(void)
{
}

void
asnLong::setValue(asnObject *a)
{
    *this = *((asnLong *) a);
}

int
asnLong::encode(char *a, unsigned int len)
{
    printf("Error: encode called for asnLong\n");
    exit(1);
}

int
asnLong::decode(char *a, unsigned int len)
{
    printf("Error: decode called for asnLong\n");
    exit(1);
}

/*
 * ASN integer class
 */
asnInteger::asnInteger(void)
{
}

asnInteger::asnInteger(int i)
{
    setValue(i);
}

asnInteger::~asnInteger(void)
{
}

void
asnInteger::setValue(asnObject *a)
{
    *this = *((asnInteger *) a);
}

int
asnInteger::encode(void)
{
    if (asn == 0 || asnLength < 2)
	return 0;

    char *p = asn;
    *p++ = tag;				// Fill in the tag value

    /*
     * Send the least number of bytes necessary to hold a signed number
     * most significant byte first.  Bit 8 of the first byte must
     * be zero if the number is positive.
     */
    unsigned long tmp = (unsigned long)value;
    unsigned int v = (unsigned int) tmp;
    unsigned int len = sizeof(int);
    unsigned int shift = 8 * (len - 1);
    unsigned int mask = 0x1FF << (shift - 1);
    while ( ((mask & v) == 0 || (mask & v) == mask) && len > 1) {
	v <<= 8;
	len--;
    }

    int llen = encodeLength(&p, len);
    if (llen == 0)
	return 0;			// Bad length
    if (len > asnLength - llen)
	return 0;			// End of ASN string

    while (len-- != 0) {
	*p++ = v >> shift;
	v <<= 8;
    }
    tmp = (unsigned long) p - (unsigned long) asn;
    return (unsigned int) tmp;
}

int
asnInteger::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
asnInteger::decode(void)
{
    if (asn == 0 || asnLength == 0)
	return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_INTEGER)
	return 0;			// Wrong tag

    if (asnLength == 1)
	return 0;

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0 )
	return 0;			// Bad length
    if (len > asnLength - llen)
	return 0;			// End of ASN string

    for (unsigned int size = sizeof(int); len > size; len--) {
	if (*p++ != 0)
	    return 0;			// Too big!
    }

    if (len > 0) {
	unsigned int result = (*p & 0x80) ? -1 : 0;
	for ( ; len != 0; len--)
	    result = (result << 8) | *p++;
        unsigned long tmp = result;
	value = (void *) tmp;
    } else
	value = 0;

    unsigned long tmp = (unsigned long) p - (unsigned long) asn;
    return (unsigned int) tmp;
}

int
asnInteger::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
asnInteger::dup(void)
{
    return new asnInteger(getValue());
}

char *
asnInteger::getString(void)
{
    char *s = new char[12];
    sprintf(s, "%d", value);
    return s;
}

/*
 * ASN Octet String class
 */
asnOctetString::asnOctetString(void)
{
    value = 0;
    stringLength = 0;
    newString = 0;
}

asnOctetString::asnOctetString(const char *s)
{
    newString = 0;
    setValue(s);
}

asnOctetString::asnOctetString(const char *s, unsigned int len)
{
    newString = 0;
    setValue(s, len);
}

asnOctetString::asnOctetString(asnOctetString &a)
{
    newString = 0;
    *this = a;
}

asnOctetString::~asnOctetString(void)
{
    if (newString)
	delete (char *) value;
}

void
asnOctetString::setValue(const char *s)
{
    if (newString)
	delete (char *) value;

    newString = 0;
    value = (void *) s;
    tag = TAG_OCTET_STRING;
    if (s == 0)
	stringLength = 0;
    else
	stringLength = strlen(s);
}

void
asnOctetString::setValue(const char *s, unsigned int len)
{
    if (newString)
	delete (char *) value;

    newString = 0;
    value = (void *) s;
    tag = TAG_OCTET_STRING;
    stringLength = len;
}

void
asnOctetString::setValue(asnObject *a)
{
    *this = *((asnOctetString *) a);
}

asnOctetString &
asnOctetString::operator=(asnOctetString &a)
{
    if (newString)
	delete (char *) value;

    value = 0;
    newString = 0;
    tag = a.getTag();
    stringLength = a.getLength();
    if (stringLength != 0) {
	newString = 1;
	value = new char[stringLength];
	bcopy(a.getValue(), value, stringLength);
    }
    return *this;
}

int
asnOctetString::encode(void)
{
    if (asn == 0 || asnLength < 2)
	return 0;

    char *p = asn;
    *p++ = tag;				// Fill in the tag value

    int len = stringLength;
    int llen = encodeLength(&p, len);
    if (llen == 0)
	return 0;			// Bad length
    if (len > asnLength - llen)
	return 0;			// End of ASN string

    if (len > 0)
	bcopy(value, p, len);
    return llen + len + 1;
}

int
asnOctetString::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
asnOctetString::decode(void)
{
    if (asn == 0 || asnLength == 0)
	return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_OCTET_STRING)
	return 0;			// Wrong tag

    if (asnLength == 1)
	return 0;

    int llen = decodeLength(&p, stringLength);
    if (llen == 0)
	return 0;			// Bad length
    if (stringLength > asnLength - llen)
	return 0;			// End of ASN string

    if (stringLength != 0) {
	if (value == 0) {
	    newString = 1;
	    value = new char[MAXCOMMNAMELEN];
        }
	bcopy(p, value, stringLength);
    }
    return llen + stringLength + 1;
}

int
asnOctetString::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
asnOctetString::dup(void)
{
    return new asnOctetString(*this);
}

char *
asnOctetString::getString(void)
{
    if (value == 0)
	return asnObject::getString();

    /*
     * Check if the string is printable.
     */
    char *v = (char *) value;
    char *s;
    for (int i = 0; i < stringLength; i++) {
	if (!isprint(v[i]) && !isspace(v[i])) {
	    /*
	     * The string is not printable, so dump it.
	     */
	    // Allow a trailing '\0' for Cabletron bug.
	    if (v[i] == 0 && i == stringLength - 1)
		continue;
	    s = new char[stringLength * 3];
	    char *p = s;
	    for (*p = '\0', i = 0; i < stringLength; i++) {
		if (p != s)
			*p++ = ' ';
		sprintf(p, "%02X", v[i]);
		p += 2;
	    }
	    return s;
	}
    }

    /*
     * The string is printable, so print it.
     */
    s = new char[stringLength + 1];
    if (stringLength != 0)
	bcopy(value, s, stringLength);
    s[stringLength] = '\0';
    return s;
}

/*
 * ASN Null class
 */
asnNull::~asnNull(void)
{
}

void
asnNull::setValue(void)
{
    value = 0;
    tag = TAG_NULL;
}

void
asnNull::setValue(asnObject *)
{
}

int
asnNull::encode(void)
{
    if (asn == 0 || asnLength < 2)
	return 0;

    char *p = asn;
    *p++ = tag;				// Fill in the tag value
    *p = 0;
    return 2;
}

int
asnNull::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
asnNull::decode(void)
{
    if (asn == 0 || asnLength == 0)
	return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_NULL)
	return 0;			// Wrong tag

    if (asnLength == 1)
	return 0;

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0 )
	return 0;			// Bad length

    value = 0;
    return llen + 1;
}

int
asnNull::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
asnNull::dup(void)
{
    return new asnNull;
}

char *
asnNull::getString(void)
{
    char *s = new char[8];
    strcpy(s, "null");
    return s;
}

/*
 * ASN Object Identifier class
 */
asnObjectIdentifier::asnObjectIdentifier(void)
{
    newOid = 0;
}

asnObjectIdentifier::asnObjectIdentifier(const char *s)
{
    newOid = 0;
    setValue(s);
}

asnObjectIdentifier::asnObjectIdentifier(oid *o)
{
    newOid = 0;
    setValue(o);
}

asnObjectIdentifier::asnObjectIdentifier(asnObjectIdentifier &a)
{
    newOid = 0;
    *this = a;
}

asnObjectIdentifier::~asnObjectIdentifier(void)
{
    if (newOid)
	delete (oid *) value;
}

void
asnObjectIdentifier::setValue(oid *o)
{
    if (newOid)
	delete (oid *) value;

    newOid = 0;
    value = o;
    tag = TAG_OBJECT_IDENTIFIER;
}

void
asnObjectIdentifier::setValue(const char *s)
{
    if (newOid)
	delete (oid *) value;

    value = new oid(s);
    newOid = 1;
    tag = TAG_OBJECT_IDENTIFIER;
}

void
asnObjectIdentifier::setValue(asnObject *a)
{
    *this = *((asnObjectIdentifier *) a);
}

asnObjectIdentifier &
asnObjectIdentifier::operator=(asnObjectIdentifier &a)
{
    if (newOid)
	delete (oid *) value;

    value = 0;
    newOid = 0;
    tag = a.getTag();
    oid *oi = a.getValue();
    if (oi != 0) {
	newOid = 1;
	value = new oid(*oi);
    }
    return *this;
}

int
asnObjectIdentifier::encode(void)
{
    if (asn == 0 || asnLength < 2)
	return 0;

    char *p = asn;
    *p++ = tag;				// Fill in the tag value

    /*
     * Combine the first sub-ID with the second using
     * b[0] = s[0] * 40 + s[1].  Encode the rest of the sub-IDs in
     * bits 1 to 7.  Bit 8 is 1 in all but the last byte of
     * each sub-ID, in which it is 0.  For example, 1.3.5.129 encodes
     * as 2B 05 81 01.
     */
    oid *objid = (oid *) value;
    int objlen = objid->length;
    if (objlen == 0)
	return 0;

    char buf[256];
    unsigned int len = 1;
    unsigned int sid = 2;
    unsigned int size = (8 * sizeof(subID)) / 7 + 1;
    unsigned int shift = 7 * (size - 1);
    unsigned int mask = 0x7F << shift;

    // Combine first to sub-IDs
    buf[0] = (objlen == 1) ? objid->subid[0] * 40 :
			    objid->subid[0] * 40 + objid->subid[1];

    // Encode in bits 1 to 7
    for (objlen -= 2; objlen > 0; objlen--, sid++) {
	unsigned int s = objid->subid[sid];
	unsigned int z = size;
	while ((s & mask) == 0 && z > 1) {
	    s <<= 7;
	    z--;
	}
	for ( ; z != 0; z--, s <<= 7, len++)
	    buf[len] = (s >> shift) | 0x80;
	    buf[len-1] &= 0x7F;
    }

    // Encode the length now that we know it and copy the bytes
    int llen = encodeLength(&p, len);
    if (llen == 0)
	return 0;			// Bad length
    if (len > asnLength - llen)
	return 0;			// End of ASN string

    bcopy(buf, p, len);
    return llen + len + 1;
}

int
asnObjectIdentifier::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
asnObjectIdentifier::decode(void)
{
    if (asn == 0 || asnLength == 0)
	return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_OBJECT_IDENTIFIER)
	return 0;			// Wrong tag

    if (asnLength == 1)
	return 0;

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0 || len <= 0)
	return 0;			// Bad length
    if (len > asnLength - llen)
	return 0;			// End of ASN string

    subID tmp[OID_MAX_LENGTH];
    unsigned int s = 0;
    tmp[s++] = *p / 40;
    tmp[s] = *p++ % 40;
    unsigned int next = 1;
    for (len--; len != 0; len--, next = !(*p++ & 0x80)) {
	if (next)
	    tmp[++s] = 0;
	tmp[s] = (tmp[s] << 7) | (*p & 0x7F);
    }

    if (value == 0) {
	value = new oid;
	newOid = 1;
    }
    oid *dst = (oid *) value;
    dst->length = s + 1;
    if (dst->subid == 0)
	dst->subid = new subID[dst->length];
    bcopy(tmp, dst->subid, dst->length * sizeof(subID));
    unsigned long temp = (unsigned long) p - (unsigned long) asn;
    return (unsigned int) temp;
}

int
asnObjectIdentifier::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
asnObjectIdentifier::dup(void)
{
    return new asnObjectIdentifier(*this);
}

char *
asnObjectIdentifier::getString(void)
{
    if (value == 0) {
	char *s = new char[8];
	strcpy(s, "null");
	return s;
    }
    return ((oid *) value)->getString();
}

/*
 * ASN Sequence class
 */
asnSequence::asnSequence(void)
{
}

asnSequence::asnSequence(unsigned int l)
{
    setValue(l);
}

asnSequence::~asnSequence(void)
{
}

void
asnSequence::setValue(unsigned int l)
{
    unsigned long tmp = l;
    value = (void *) tmp;
    tag = TAG_SEQUENCE;
}

void
asnSequence::setValue(asnObject *a)
{
    asnObject::setValue(a);
}

int
asnSequence::encode(void)
{
    if (asn == 0 || asnLength < 2)
	return 0;

    char *p = asn;
    *p++ = tag;				// Fill in the tag value

    unsigned long tmp = (unsigned long) value;
    int llen = encodeLength(&p, (unsigned int) tmp);
    if (llen == 0)
	return 0;

    return llen + 1;
}

int
asnSequence::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
asnSequence::decode(void)
{
    if (asn == 0 || asnLength == 0)
	return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_SEQUENCE)
	return 0;			// Wrong tag

    if (asnLength == 1)
	return 0;

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0)
	return 0;			// Bad length

    unsigned long tmp = len;
    value = (void *) tmp;
    return llen + 1;
}

int
asnSequence::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

char *
asnSequence::getString(void)
{
    char *s = new char[12];
    sprintf(s, "%u", value);
    return s;
}

/*
 * ASN Choice class
 */
asnChoice::asnChoice(void)
{
}

asnChoice::asnChoice(unsigned int c, unsigned int l)
{
    setValue(c, l);
}

asnChoice::~asnChoice(void)
{
}

void
asnChoice::setValue(unsigned int c, unsigned int l)
{
    unsigned long tmp = l;
    value = (void *) tmp;
    setTag(c);
}

void
asnChoice::setValue(asnObject *a)
{
    asnObject::setValue(a);
}

void
asnChoice::setTag(unsigned int c)
{
    tag = c;
}

int
asnChoice::encode(void)
{
    if (asn == 0 || asnLength < 2)
	return 0;

    char *p = asn;
    *p++ = tag;				// Fill in the tag value

    unsigned long tmp = (unsigned long) value;
    int llen = encodeLength(&p, (unsigned int) tmp);
    if (llen == 0)
	return 0;

    return llen + 1;
}

int
asnChoice::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
asnChoice::decode(void)
{
    if (asn == 0 || asnLength == 0)
	return 0;

    char *p = asn;
    tag = *p++;
    if ((tag & 0xF0) != TAG_CHOICE)
	return 0;			// Wrong tag

    if (asnLength == 1)
	return 0;

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0)
	return 0;			// Bad length

    unsigned long tmp = len;
    value = (void *) tmp;
    return llen + 1;
}

int
asnChoice::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

char *
asnChoice::getString(void)
{
    char *s = new char[12];
    sprintf(s, "%u", value);
    return s;
}
