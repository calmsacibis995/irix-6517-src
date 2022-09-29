/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP
 *
 *	$Revision: 1.3 $
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <libc.h>
#include <string.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"


/*
 * SNMP network address class
 */
snmpNetworkAddress::snmpNetworkAddress(void)
{
}

snmpNetworkAddress::snmpNetworkAddress(unsigned long a)
{
    setValue(a);
}

snmpNetworkAddress::snmpNetworkAddress(const char *s)
{
    setValue(s);
}

snmpNetworkAddress::~snmpNetworkAddress(void)
{
}

void
snmpNetworkAddress::setValue(unsigned long a)
{
    value = (void *) a;
    tag = TAG_IPADDRESS;	// XXX - tag = TAG_NETADDR_IP;
}

void
snmpNetworkAddress::setValue(const char *s)
{
    value = (void *) inet_addr(s);
    tag = TAG_NETADDR_IP;
}

void
snmpNetworkAddress::setValue(asnObject *a)
{
    *this = *((snmpNetworkAddress *) a);
}

unsigned long
snmpNetworkAddress::getValue(void)
{
    return (unsigned long) value;
}

int
snmpNetworkAddress::encode(void)
{
    if (asn == 0 || asnLength < 6)
	return 0;

    char *p = asn;
    *p++ = tag;		// Fill in the tag value

    int llen = encodeLength(&p, 4);
    if (llen == 0)
	return 0;
    unsigned long temp = (unsigned long) value;
    unsigned int  ii = (unsigned int) temp;
    bcopy(&ii, p, 4);       
    p += 4;
    return llen + 5;
}

int
snmpNetworkAddress::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
snmpNetworkAddress::decode(void)
{
    if (asn == 0 || asnLength < 6)
	return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_IPADDRESS)		// XXX - TAG_NETADDR_IP?
	return 0;			// Wrong tag

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0 || len != 4)
	return 0;			// Bad length

    unsigned int ii;
    bcopy(p, &ii, 4);
    unsigned long temp = (unsigned long) ii;
    value = (void *) temp;
    //unsigned long tmp = (unsigned long)p + 4;
    return llen + 5;
}

int
snmpNetworkAddress::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
snmpNetworkAddress::dup(void)
{
    return new snmpNetworkAddress(getValue());
}

char *
snmpNetworkAddress::getString(void)
{
    struct in_addr i;
    i.s_addr = (unsigned long) value;
    char *a = inet_ntoa(i);
    char *s = new char[strlen(a) + 1];
    strcpy(s, a);
    return s;
}

/*
 * SNMP IP address class
 */
snmpIPaddress::snmpIPaddress(void)
{
}

snmpIPaddress::snmpIPaddress(unsigned long a)
{
    setValue(a);
}

snmpIPaddress::snmpIPaddress(const char *s)
{
    setValue(s);
}

snmpIPaddress::~snmpIPaddress(void)
{
}

void
snmpIPaddress::setValue(unsigned long a)
{
    value = (void *) a;
    tag = TAG_IPADDRESS;
}

void
snmpIPaddress::setValue(const char *s)
{
    value = (void *) inet_addr(s);
    tag = TAG_IPADDRESS;
}

void
snmpIPaddress::setValue(asnObject *a)
{
    *this = *((snmpIPaddress *) a);
}

unsigned long
snmpIPaddress::getValue(void)
{
    return (unsigned long) value;
}

int
snmpIPaddress::encode(void)
{
    if (asn == 0 || asnLength < 6)
	return 0;

    char *p = asn;
    *p++ = tag;		// Fill in the tag value

    int llen = encodeLength(&p, 4);
    if (llen == 0)
	return 0;

    unsigned long temp = (unsigned long) value;
    unsigned int  ii = (unsigned int) temp;
    bcopy(&ii, p, 4);
    p += 4;
    return llen + 5;
}

int
snmpIPaddress::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
snmpIPaddress::decode(void)
{
    if (asn == 0 || asnLength < 6)
	return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_IPADDRESS)
	return 0;			// Wrong tag

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0 || len != 4)
	return 0;			// Bad length

    unsigned int ii;
    bcopy(p, &ii, 4);
    unsigned long temp = (unsigned long) ii;
    value = (void *) temp;
    return llen + 5;
}

int
snmpIPaddress::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
snmpIPaddress::dup(void)
{
    return new snmpIPaddress(getValue());
}

char *
snmpIPaddress::getString(void)
{
    struct in_addr i;
    i.s_addr = (unsigned long) value;
    char *a = inet_ntoa(i);
    char *s = new char[strlen(a) + 1];
    strcpy(s, a);
    return s;
}

/*
 * SNMP Counter class
 */
snmpCounter::snmpCounter(void)
{
}

snmpCounter::snmpCounter(unsigned int i)
{
    setValue(i);
}

snmpCounter::~snmpCounter(void)
{
}

void
snmpCounter::setValue(unsigned int i)
{
    unsigned long tmp = i;
    value = (void *) tmp;
    tag = TAG_COUNTER;
}

void
snmpCounter::setValue(asnObject *a)
{
    *this = *((snmpCounter *) a);
}

unsigned int
snmpCounter::getValue(void)
{
    unsigned long tmp = (unsigned long) value;
    return (unsigned int) tmp;
}

int
snmpCounter::encode(void)
{
    if (asn == 0 || asnLength < 2)
	return 0;

    char *p = asn;
    *p++ = tag;		// Fill in the tag value

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
        return 0;                       // Bad length
    if (len > asnLength - llen)
        return 0;                       // End of ASN string

    while (len-- != 0) {
	*p++ = v >> shift;
	v <<= 8;
    }
    tmp = (unsigned long)p - (unsigned long)asn;
    return (int) tmp;
}

int
snmpCounter::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
snmpCounter::decode(void)
{
    if (asn == 0 || asnLength == 0)
        return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_COUNTER)
        return 0;                       // Wrong tag

    if (asnLength == 1)
	return 0;

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0)
        return 0;                       // Bad length
    if (len > asnLength - llen)
        return 0;                       // End of ASN string

    for (unsigned int size = sizeof(int); len > size; len--) {
        if (*p++ != 0)
            return 0;                   // Too big!
    }

    if (len > 0) {
        unsigned int result = (*p & 0x80) ? -1 : 0;
        for ( ; len != 0; len--)
            result = (result << 8) | *p++;
	unsigned long tmp = result;
        value = (void *) tmp;
    } else
        value = 0;
    unsigned long tmp = (unsigned long)p - (unsigned long)asn;
    return (int) tmp;
}

int
snmpCounter::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
snmpCounter::dup(void)
{
    return new snmpCounter(getValue());
}

char *
snmpCounter::getString(void)
{
    char *s = new char[12];
    sprintf(s, "%lu", value);
    return s;
}

/*
 * SNMP Gauge class
 */
snmpGauge::snmpGauge(void)
{
}

snmpGauge::snmpGauge(unsigned int i)
{
    setValue(i);
}

snmpGauge::~snmpGauge(void)
{
}

void
snmpGauge::setValue(unsigned int i)
{
    unsigned long tmp = i;
    value = (void *) tmp;
    tag = TAG_GAUGE;
}

void
snmpGauge::setValue(asnObject *a)
{
    *this = *((snmpGauge *) a);
}

unsigned int
snmpGauge::getValue(void)
{
    unsigned long tmp = (unsigned long) value;
    return (unsigned int) tmp;
}

int
snmpGauge::encode(void)
{
    if (asn == 0 || asnLength < 2)
	return 0;

    char *p = asn;
    *p++ = tag;		// Fill in the tag value

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
        return 0;                       // Bad length
    if (len > asnLength - llen)
        return 0;                       // End of ASN string

    while (len-- != 0) {
	*p++ = v >> shift;
	v <<= 8;
    }

    tmp = (unsigned long)p - (unsigned long)asn;
    return (int)tmp;
}

int
snmpGauge::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
snmpGauge::decode(void)
{
    if (asn == 0 || asnLength == 0)
        return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_GAUGE)
        return 0;                       // Wrong tag

    if (asnLength == 1)
	return 0;

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0)
        return 0;                       // Bad length
    if (len > asnLength - llen)
        return 0;                       // End of ASN string

    for (unsigned int size = sizeof(int); len > size; len--) {
        if (*p++ != 0)
            return 0;                   // Too big!
    }

    if (len > 0) {
        unsigned int result = (*p & 0x80) ? -1 : 0;
        for ( ; len != 0; len--)
            result = (result << 8) | *p++;
        unsigned long tmp = result;
        value = (void *) tmp;
    } else
        value = 0;
    unsigned long tmp = (unsigned long)p - (unsigned long)asn;
    return (int) tmp;
}

int
snmpGauge::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
snmpGauge::dup(void)
{
    return new snmpGauge(getValue());
}

char *
snmpGauge::getString(void)
{
    char *s = new char[12];
    sprintf(s, "%lu", value);
    return s;
}

/*
 * SNMP Timeticks class
 */
snmpTimeticks::snmpTimeticks(void)
{
}

snmpTimeticks::snmpTimeticks(int i)
{
    setValue(i);
}

snmpTimeticks::~snmpTimeticks(void)
{
}

void
snmpTimeticks::setValue(int i)
{
    long tmp = i;
    value = (void *) tmp;
    tag = TAG_TIMETICKS;
}

void 
snmpTimeticks::setValue(asnObject *a)
{
    *this = *((snmpTimeticks *) a);
}

int
snmpTimeticks::getValue(void)
{
    unsigned long tmp = (unsigned long) value;
    return (unsigned int) tmp;
}

int
snmpTimeticks::encode(void)
{
    if (asn == 0 || asnLength < 2)
	return 0;

    char *p = asn;
    *p++ = tag;		// Fill in the tag value

    /*
     * Send the least number of bytes necessary to hold a signed number
     * most significant byte first.  Bit 8 of the first byte must
     * be zero if the number is positive.
     */
    unsigned long tmp = (unsigned long) value;
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
        return 0;                       // Bad length
    if (len > asnLength - llen)
        return 0;                       // End of ASN string

    while (len-- != 0) {
	*p++ = v >> shift;
	v <<= 8;
    }

    tmp = (unsigned long)p - (unsigned long)asn;
    return (int) tmp;
}

int
snmpTimeticks::encode(char *a, unsigned int len)
{
    setAsn(a, len);
    return encode();
}

int
snmpTimeticks::decode(void)
{
    if (asn == 0 || asnLength == 0)
        return 0;

    char *p = asn;
    tag = *p++;
    if (tag != TAG_TIMETICKS)
        return 0;                       // Wrong tag

    if (asnLength == 1)
	return 0;

    unsigned int len;
    int llen = decodeLength(&p, len);
    if (llen == 0)
        return 0;                       // Bad length
    if (len > asnLength - llen)
        return 0;                       // End of ASN string

    for (unsigned int size = sizeof(int); len > size; len--) {
        if (*p++ != 0)
            return 0;                   // Too big!
    }

    if (len > 0) {
        unsigned int result = (*p & 0x80) ? -1 : 0;
        for ( ; len != 0; len--)
            result = (result << 8) | *p++;
        unsigned long tmp = result;
        value = (void *) tmp;
    } else
        value = 0;
    unsigned long tmp = (unsigned long)p -(unsigned long)asn;
    return (int) tmp;
}
int
snmpTimeticks::decode(char *a, unsigned int len)
{
    setAsn(a, len);
    return decode();
}

asnObject *
snmpTimeticks::dup(void)
{
    return new snmpTimeticks(getValue());
}

char *
snmpTimeticks::getString(void)
{
    char *s = new char[32];
    unsigned long tmp = (unsigned long)value;
    unsigned int t = (unsigned int) tmp / 100;
    unsigned int days = t / (24 * 60 * 60);
    unsigned int hours = t % (24 * 60 * 60) / (60 * 60);
    unsigned int minutes = t % (60 * 60) / 60;
    unsigned int seconds = t % 60;
    unsigned int hundreths = (unsigned int) tmp % 100;

    if (days > 0)
	sprintf(s, "%d day%s, %02d:%02d:%02d.%02d",
	    days, (days == 1) ? "" : "s", hours, minutes, seconds, hundreths);
    else
	sprintf(s, "%02d:%02d:%02d.%02d", hours, minutes, seconds, hundreths);

    return s;
}
