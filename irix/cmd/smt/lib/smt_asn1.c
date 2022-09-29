/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.8 $
 */

/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

		      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#include <sys/types.h>
#include <sm_log.h>
#include <smt_asn1.h>

#ifndef NULL
#define NULL	0
#endif

/*
 * asn_parse_int - pulls a long out of an ASN int type.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
char *
asn_parse_int(
	register char	    *data,	/* IN - pointer to start of object */
	register int	    *datalength,/* IN/OUT - number of bytes in buffer */
	char		    *type,	/* OUT - asn type of object */
	long		    *intp,	/* IN/OUT - ptr to output buffer */
	int		    intsize)    /* IN - size of output buffer */
{
	/*
	 * ASN.1 integer ::= 0x02 asnlength byte {byte}*
	 */
	register char *bufp = data;
	u_long	    asn_length;
	register long   value = 0;

	if (intsize != sizeof(long)) {
		ERROR("not long");
		return NULL;
	}
	*type = *bufp++;
	bufp = asn_parse_length(bufp, &asn_length);
	if (bufp == NULL) {
		ERROR("bad length");
		return NULL;
	}
	if (asn_length + (bufp - data) > *datalength) {
		ERROR("overflow of message");
		return NULL;
	}
	if (asn_length > intsize) {
		ERROR("I don't support such large integers");
		return NULL;
	}
	*datalength -= (int)asn_length + (bufp - data);
	if (*bufp & 0x80)
		value = -1; /* integer is negative */
	while(asn_length--)
		value = (value << 8) | *bufp++;
	*intp = value;
	return bufp;
}


/*
 * asn_build_int - builds an ASN object containing an integer.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
char *
asn_build_int(
	register char *data,		/* IN - ptr to start of output buf */
	register int    *datalength,	/* IN/OUT - num of valid bytes in buf */
	char	    type,		/* IN - asn type of object */
	register long   *intp,		/* IN - ptr to start of long integer */
	register int    intsize)	/* IN - size of *intp */
{
	/*
	 * ASN.1 integer ::= 0x02 asnlength byte {byte}*
	 */

	register long integer;
	register u_long mask;

	if (intsize != sizeof (long))
		return NULL;
	integer = *intp;

	/*
	 * Truncate "unnecessary" bytes off of the most significant end of
	 * this 2's complement integer.
	 * There should be no sequence of 9 consecutive 1's or 0's
	 * at the most significant end of the integer.
	 */

	/* mask is 0xFF800000 on a big-endian machine */
	mask = 0x1FF << ((8 * (sizeof(long) - 1)) - 1);
	while ((((integer & mask) == 0) || ((integer & mask) == mask))
		&& intsize > 1) {
		intsize--;
		integer <<= 8;
	}
	data = asn_build_header(data, datalength, type, intsize);
	if (data == NULL)
		return NULL;
	if (*datalength < intsize)
		return NULL;
	*datalength -= intsize;
	mask = 0xFF << (8 * (sizeof(long) - 1));

	/* mask is 0xFF000000 on a big-endian machine */
	while (intsize--){
		*data++ = (integer&mask) >> (8 * (sizeof(long)-1));
		integer <<= 8;
	}
	return data;
}


/*
 * asn_parse_string - pulls an octet string out of an ASN octet string type.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  "string" is filled with the octet string.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
char *
asn_parse_string(data, datalength, type, string, strlength)
    char	    *data;	    /* IN - pointer to start of object */
    register int    *datalength;    /* IN/OUT - number of valid bytes left in buffer */
    char	    *type;	    /* OUT - asn type of object */
    char	    *string;	    /* IN/OUT - pointer to start of output buffer */
    register int    *strlength;     /* IN/OUT - size of output buffer */
{
/*
 * ASN.1 octet string ::= primstring | cmpdstring
 * primstring ::= 0x04 asnlength byte {byte}*
 * cmpdstring ::= 0x24 asnlength string {string}*
 * This doesn't yet support the compound string.
 */
    register char *bufp = data;
    u_long	    asn_length;

    *type = *bufp++;
    bufp = asn_parse_length(bufp, &asn_length);
    if (bufp == NULL)
	return NULL;
    if (asn_length + (bufp - data) > *datalength){
	ERROR("overflow of message");
	return NULL;
    }
    if (asn_length > *strlength){
	ERROR("I don't support such long strings");
	return NULL;
    }
    bcopy((char *)bufp, (char *)string, (int)asn_length);
    *strlength = (int)asn_length;
    *datalength -= (int)asn_length + (bufp - data);
    return bufp + asn_length;
}


/*
 * asn_build_string - Builds an ASN octet string object containing the input string.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
char *
asn_build_string(
    char	    *data,	    /* IN - pointer to start of object */
    register int    *datalength,    /* IN/OUT-num of valid bytes left in buf */
    char	    type,	    /* IN - ASN type of string */
    char	    *string,	    /* IN - pointer to start of input buffer */
    register int    strlength)	    /* IN - size of input buffer */
{
/*
 * ASN.1 octet string ::= primstring | cmpdstring
 * primstring ::= 0x04 asnlength byte {byte}*
 * cmpdstring ::= 0x24 asnlength string {string}*
 * This code will never send a compound string.
 */
    data = asn_build_header(data, datalength, type, strlength);
    if (data == NULL)
	return NULL;
    if (*datalength < strlength)
	return NULL;
    bcopy((char *)string, (char *)data, strlength);
    *datalength -= strlength;
    return data + strlength;
}


/*
 * asn_parse_header - interprets the ID and length of the current object.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   in this object following the id and length.
 *
 *  Returns a pointer to the first byte of the contents of this object.
 *  Returns NULL on any error.
 */
char *
asn_parse_header(char	*data,		/* IN - pointer to start of object */
		 int	*datalength,    /* IN/OUT - # valid bytes in buffer */
		 char	*type)		/* OUT - ASN type of object */
{
	register char *bufp = data;
	register int header_len;
	u_long	 asn_length;

	/* this only works on data types < 30, i.e. no extension octets */
	if (IS_EXTENSION_ID(*bufp)) {
		sm_log(LOG_DEBUG, 0, "can't process ID(%d) >= 30", (int)*bufp);
		return(NULL);
	}
	*type = *bufp;
	bufp = asn_parse_length(bufp + 1, &asn_length);
	if (bufp == NULL) {
		sm_log(LOG_DEBUG, 0, "parse_header: bad length");
		return(NULL);
	}
	header_len = bufp - data;
	if (header_len + asn_length > *datalength) {
		sm_log(LOG_DEBUG, 0, "header_len(%d)+asn_length(%d) > %d",
			header_len, asn_length, *datalength);
		sm_hexdump(LOG_DEBUG, data, *datalength);
		return(NULL);
	}
	*datalength = (int)asn_length;
	return bufp;
}

/*
 * asn_build_header - builds an ASN header for an object with the ID and
 * length specified.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   in this object following the id and length.
 *
 *  This only works on data types < 30, i.e. no extension octets.
 *  The maximum length is 0xFFFF;
 *
 *  Returns a pointer to the first byte of the contents of this object.
 *  Returns NULL on any error.
 */
char *
asn_build_header(
	register char *data,	/* IN - pointer to start of object */
	int	 *datalength,	/* IN/OUT-num of valid bytes left in buf */
	char	 type,		/* IN - ASN type of object */
	int	 length)	/* IN - length of object */
{
	if (*datalength < 1)
		return(NULL);
	*data++ = type;
	(*datalength)--;
	return(asn_build_length(data, datalength, length));

}

/*
 * asn_parse_length - interprets the length of the current object.
 *  On exit, length contains the value of this length field.
 *
 *  Returns a pointer to the first byte after this length
 *  field (aka: the start of the data field).
 *  Returns NULL on any error.
 */
char *
asn_parse_length(
	char  *data,		/* IN - pointer to start of length field */
	u_long  *length)	/* OUT - value of length field */
{
	register char lengthbyte = *data;

	if (lengthbyte & ASN_LONG_LEN) {
		lengthbyte &= ~ASN_LONG_LEN;	/* turn MSb off */
		if (lengthbyte == 0) {
			ERROR("We don't support indefinite lengths");
			return(NULL);
		}
		if (lengthbyte > sizeof(long)) {
			ERROR("we can't support data lengths that long");
			return(NULL);
		}
		bcopy((char *)data + 1, (char *)length, (int)lengthbyte);
#ifdef CMU
		*length = ntohl(*length);
#endif
		*length >>= (8 * ((sizeof *length) - lengthbyte));
		return(data + lengthbyte + 1);
	}
	/* short asnlength */
	*length = (long)lengthbyte;
	return(data + 1);
}

char *
asn_build_length(
		 register char *data,	/* IN - pointer to start of object */
		 int	*datalength,	/* IN/OUT -  valid bytes in buf */
		 register u_long length)    /* IN - length of object */
{
	char    *start_data = data;

	/* no indefinite lengths sent */
	if (length < 0x80) {
		*data++ = length;
	} else if (length <= 0xFF) {
		*data++ = 0x01 | ASN_LONG_LEN;
		*data++ = length;
	} else {			/* 0xFF < length <= 0xFFFF */
		*data++ = 0x02 | ASN_LONG_LEN;
		*data++ = length >> 8;
		*data++ = length & 0xFF;
	}

	if (*datalength < (data - start_data)) {
		ERROR("build_length");
		return(NULL);
	}
	*datalength -= (data - start_data);
	return(data);

}

/*
 * asn_parse_objid - pulls an object indentifier out of an ASN object identifier type.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  "objid" is filled with the object identifier.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
char *
asn_parse_objid(
    char	    *data,	    /* IN - pointer to start of object */
    int		    *datalength,    /* IN/OUT-num of valid bytes left in buf */
    char	    *type,	    /* OUT - ASN type of object */
    oid		    *objid,	    /* IN/OUT - ptr to start of output buf */
    int		    *objidlength)   /* IN/OUT - number of sub-id's in objid */
{
/*
 * ASN.1 objid ::= 0x06 asnlength subidentifier {subidentifier}*
 * subidentifier ::= {leadingbyte}* lastbyte
 * leadingbyte ::= 1 7bitvalue
 * lastbyte ::= 0 7bitvalue
 */
    register char *bufp = data;
    register oid *oidp = objid + 1;
    register u_long subidentifier;
    register long   length;
    u_long	    asn_length;

    *type = *bufp++;
    bufp = asn_parse_length(bufp, &asn_length);
    if (bufp == NULL)
	return NULL;
    if (asn_length + (bufp - data) > *datalength){
	ERROR("overflow of message");
	return NULL;
    }
    *datalength -= (int)asn_length + (bufp - data);

    length = asn_length;
    (*objidlength)--;	/* account for expansion of first byte */
    while (length > 0 && (*objidlength)-- > 0){
	subidentifier = 0;
	do {	/* shift and add in low order 7 bits */
	    subidentifier = (subidentifier << 7) + (*bufp & ~ASN_BIT8);
	    length--;
	} while (*bufp++ & ASN_BIT8);	/* last byte has high bit clear */
	if (subidentifier > MAX_SUBID){
	    ERROR("subidentifier too long");
	    return NULL;
	}
	*oidp++ = (oid)subidentifier;
    }

    /*
     * The first two subidentifiers are encoded into the first component
     * with the value (X * 40) + Y, where:
     *	X is the value of the first subidentifier.
     *  Y is the value of the second subidentifier.
     */
    subidentifier = (u_long)objid[1];
    objid[1] = (u_char)(subidentifier % 40);
    objid[0] = (u_char)((subidentifier - objid[1]) / 40);

    *objidlength = (int)(oidp - objid);
    return bufp;
}

/*
 * asn_build_objid - Builds an ASN object identifier object containing the input string.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
char *
asn_build_objid(
    register char *data,	    /* IN - pointer to start of object */
    int		    *datalength,    /* IN/OUT-num of valid bytes left in buf */
    char	    type,	    /* IN - ASN type of object */
    oid		    *objid,	    /* IN - pointer to start of input buffer */
    int		    objidlength)    /* IN - number of sub-id's in objid */
{
/*
 * ASN.1 objid ::= 0x06 asnlength subidentifier {subidentifier}*
 * subidentifier ::= {leadingbyte}* lastbyte
 * leadingbyte ::= 1 7bitvalue
 * lastbyte ::= 0 7bitvalue
 */
    char buf[MAX_OID_LEN];
    char *bp = buf;
    oid	objbuf[MAX_OID_LEN];
    oid *op = objbuf;
    register int    asnlength;
    register u_long subid, mask, testmask;
    register int bits, testbits;

    bcopy((char *)objid, (char *)objbuf, objidlength * sizeof(oid));
    /* transform size in bytes to size in subid's */
    /* encode the first two components into the first subidentifier */
    op[1] = op[1] + (op[0] * 40);
    op++;
    objidlength--;

    while(objidlength-- > 0){
	subid = *op++;
	mask = 0x7F; /* handle subid == 0 case */
	bits = 0;
	/* testmask *MUST* !!!! be of an unsigned type */
	for(testmask = 0x7F, testbits = 0; testmask != 0; testmask <<= 7, testbits += 7){
	    if (subid & testmask){	/* if any bits set */
		mask = testmask;
		bits = testbits;
	    }
	}
	/* mask can't be zero here */
	for(;mask != 0x7F; mask >>= 7, bits -= 7){
	    *bp++ = ((subid & mask) >> bits) | ASN_BIT8;
	}
	*bp++ = subid & mask;
    }
    asnlength = bp - buf;
    data = asn_build_header(data, datalength, type, asnlength);
    if (data == NULL)
	return NULL;
    if (*datalength < asnlength)
	return NULL;
    bcopy((char *)buf, (char *)data, asnlength);
    *datalength -= asnlength;
    return data + asnlength;
}

/*
 * asn_parse_null - Interprets an ASN null type.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
char *
asn_parse_null(data, datalength, type)
    char	    *data;	    /* IN - pointer to start of object */
    int		    *datalength;    /* IN/OUT-num of valid bytes left in buf */
    char	    *type;	    /* OUT - ASN type of object */
{
/*
 * ASN.1 null ::= 0x05 0x00
 */
    register char   *bufp = data;
    u_long	    asn_length;

    *type = *bufp++;
    bufp = asn_parse_length(bufp, &asn_length);
    if (bufp == NULL)
	return NULL;
    if (asn_length != 0){
	ERROR("Malformed NULL");
	return NULL;
    }
    *datalength -= (bufp - data);
    return bufp + asn_length;
}


/*
 * asn_build_null - Builds an ASN null object.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
char *
asn_build_null(
	char	    *data,	    /* IN - pointer to start of object */
	int	    *datalength,    /* IN/OUT-num of valid bytes left in buf */
	char	    type)	    /* IN - ASN type of object */
{
	/* ASN.1 null ::= 0x05 0x00 */
	return(asn_build_header(data, datalength, type, 0));
}
