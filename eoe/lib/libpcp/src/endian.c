/*
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/* $Id: endian.c,v 1.1 1998/11/15 08:35:24 kenmcd Exp $ */

/*
 * Bit field typedefs for endian translations to support little endian
 * hosts.
 *
 * For a typedef __X_foo, the big endian version will be "foo" or
 * "__foo" in one of the other headers, ie. pmapi.h, pmapi_dev.h or
 * impl.h
 * The only structures that appear here are ones that
 * (a) may be encoded within a PDU, and/or
 * (b) may appear in a PCP archive
 */

#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"

#if !defined(HAVE_NETWORK_BYTEORDER)

typedef struct {
    int pad : 8;
    int	scaleCount : 4;	/* one of PM_COUNT_* below */
    int	scaleTime : 4;	/* one of PM_TIME_* below */
    int	scaleSpace : 4;	/* one of PM_SPACE_* below */
    int	dimCount : 4;	/* event dimension */
    int	dimTime : 4;	/* time dimension */
    int	dimSpace : 4;	/* space dimension */
} __X_pmUnits;			/* dimensional units and scale of value */

typedef struct {
    unsigned int	vlen : 24;	/* bytes for vtype/vlen + vbuf */
    unsigned int	vtype : 8;	/* value type */
    char		vbuf[1];	/* the value */
} __X_pmValueBlock;

typedef struct {
	unsigned int	item : 10;
	unsigned int	cluster : 12;
	unsigned int	domain : 8;
	int		pad : 2;
} __X_pmID_int;

typedef struct {
	unsigned int	serial : 22;		/* unique within PMD */
	unsigned int	domain : 8;		/* the administrative PMD */
	int		pad : 2;
} __X_pmInDom_int;

typedef struct {
    unsigned int	authorize : 16;
    unsigned int	licensed : 8;
    unsigned int	version : 7;	/* PDU_VERSION */
    unsigned int	zero : 1;	/* ensure first bit is zero for 1.x compatibility */
} __X_pmPDUInfo;

typedef struct {
    unsigned int	c_valc: 8;
    unsigned int	c_valb: 8;
    unsigned int	c_vala: 8;
    unsigned int	c_type: 8;
} __X_pmCred;

pmUnits
__htonpmUnits(pmUnits units)
{
    pmUnits		result;
    pmUnits		*inp = (pmUnits *)&units;
    __X_pmUnits		*outp = (__X_pmUnits *)&result;
    unsigned int	*ip = (unsigned int *)&result;

    outp->pad = inp->pad;
    outp->scaleCount = inp->scaleCount;
    outp->scaleTime = inp->scaleTime;
    outp->scaleSpace = inp->scaleSpace;
    outp->dimCount = inp->dimCount;
    outp->dimTime = inp->dimTime;
    outp->dimSpace = inp->dimSpace;

    *ip = htonl(*ip);

    return result;
}

pmUnits
__ntohpmUnits(pmUnits units)
{
    pmUnits		result;
    pmUnits		*outp = (pmUnits *)&result;
    unsigned int	tmp;
    __X_pmUnits	*inp = (__X_pmUnits *)&tmp;

    tmp = ntohl(*(unsigned int *)&units);

    outp->pad = inp->pad;
    outp->scaleCount = inp->scaleCount;
    outp->scaleTime = inp->scaleTime;
    outp->scaleSpace = inp->scaleSpace;
    outp->dimCount = inp->dimCount;
    outp->dimTime = inp->dimTime;
    outp->dimSpace = inp->dimSpace;

    return result;
}

pmID
__htonpmID(pmID pmid)
{
    pmID		result;
    __pmID_int		*inp = (__pmID_int *)&pmid;
    __X_pmID_int	*outp = (__X_pmID_int *)&result;
    unsigned int	*ip = (unsigned int *)&result;

    outp->pad = inp->pad;
    outp->item = inp->item;
    outp->cluster = inp->cluster;
    outp->domain = inp->domain;

    *ip = htonl(*ip);

    return result;
}

pmID
__ntohpmID(pmID pmid)
{
    pmID		result;
    __pmID_int		*outp = (__pmID_int *)&result;
    unsigned int	tmp;
    __X_pmID_int	*inp = (__X_pmID_int *)&tmp;

    tmp = ntohl(*(unsigned int *)&pmid);

    outp->pad = inp->pad;
    outp->item = inp->item;
    outp->cluster = inp->cluster;
    outp->domain = inp->domain;

    return result;
}

pmValueBlock
__htonpmValueBlock(pmValueBlock vb)
{
    pmValueBlock	result;
    pmValueBlock	*inp = &vb;
    __X_pmValueBlock	*outp = (__X_pmValueBlock *)&result;
    unsigned int	*ip = (unsigned int *)&result;

    outp->vtype = inp->vtype;
    outp->vlen = inp->vlen;

    *ip = htonl(*ip);

    return result;
}

pmValueBlock
__ntohpmValueBlock(pmValueBlock vb)
{
    pmValueBlock	result;
    pmValueBlock	*outp = (pmValueBlock *)&result;
    unsigned int	tmp;
    __X_pmValueBlock	*inp = (__X_pmValueBlock *)&tmp;

    tmp = ntohl(*(unsigned int *)&vb);

    outp->vtype = inp->vtype;
    outp->vlen = inp->vlen;

    return result;
}

pmInDom
__htonpmInDom(pmInDom indom)
{
    pmInDom		result;
    __pmInDom_int	*inp = (__pmInDom_int *)&indom;
    __X_pmInDom_int	*outp = (__X_pmInDom_int *)&result;
    unsigned int	*ip = (unsigned int *)&result;

    outp->serial = inp->serial;
    outp->domain = inp->domain;
    outp->pad = inp->pad;

    *ip = htonl(*ip);

    return result;
}

pmInDom
__ntohpmInDom(pmInDom indom)
{
    pmInDom		result;
    __pmInDom_int	*outp = (__pmInDom_int *)&result;
    unsigned int	tmp;
    __X_pmInDom_int	*inp = (__X_pmInDom_int *)&tmp;

    tmp = ntohl(*(unsigned int *)&indom);

    outp->serial = inp->serial;
    outp->domain = inp->domain;
    outp->pad = inp->pad;

    return result;
}


__pmPDUInfo
__htonpmPDUInfo(__pmPDUInfo info)
{
    __pmPDUInfo		result;
    __pmPDUInfo		*inp = (__pmPDUInfo *)&info;
    __X_pmPDUInfo	*outp = (__X_pmPDUInfo *)&result;
    unsigned int	*ip = (unsigned int *)&result;

    outp->authorize = inp->authorize;
    outp->licensed = inp->licensed;
    outp->version = inp->version;
    outp->zero = inp->zero;

    *ip = htonl(*ip);

    return result;
}

__pmPDUInfo
__ntohpmPDUInfo(__pmPDUInfo info)
{
    __pmPDUInfo		result;
    __pmPDUInfo		*outp = (__pmPDUInfo *)&result;
    unsigned int	tmp;
    __X_pmPDUInfo	*inp = (__X_pmPDUInfo *)&tmp;

    tmp = ntohl(*(unsigned int *)&info);

    outp->authorize = inp->authorize;
    outp->licensed = inp->licensed;
    outp->version = inp->version;
    outp->zero = inp->zero;

    return result;
}

__pmCred
__htonpmCred(__pmCred cred)
{
    __pmCred		result;
    __pmCred		*inp = (__pmCred *)&cred;
    __X_pmCred		*outp = (__X_pmCred *)&result;
    unsigned int	*ip = (unsigned int *)&result;

    outp->c_valc = inp->c_valc;
    outp->c_valb = inp->c_valb;
    outp->c_vala = inp->c_vala;
    outp->c_type = inp->c_type;

    *ip = htonl(*ip);

    return result;
}

__pmCred
__ntohpmCred(__pmCred cred)
{
    __pmCred		result;
    __pmCred		*outp = (__pmCred *)&result;
    unsigned int	tmp;
    __X_pmCred		*inp = (__X_pmCred *)&tmp;

    tmp = ntohl(*(unsigned int *)&cred);

    outp->c_valc = inp->c_valc;
    outp->c_valb = inp->c_valb;
    outp->c_vala = inp->c_vala;
    outp->c_type = inp->c_type;

    return result;
}


void
__htonf(char *p)
{
    char 	c;
    int		i;

    for (i = 0; i < 2; i++) {
	c = p[i];
	p[i] = p[3-i];
	p[3-i] = c;
    }
}

void
__htonll(char *p)
{
    char 	c;
    int		i;

    for (i = 0; i < 4; i++) {
	c = p[i];
	p[i] = p[7-i];
	p[7-i] = c;
    }
}

#endif /* ! HAVE_NETWORK_BYTEORDER */
