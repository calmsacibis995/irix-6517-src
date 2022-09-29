/*
 * Copyright 1995, Silicon Graphics, Inc.
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

#ident "$Id: p_instance.c,v 2.10 1998/11/15 08:35:24 kenmcd Exp $"

#include <ctype.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

/*
 * PDU for pm*InDom request (PDU_INSTANCE_REQ)
 */
typedef struct {
    __pmPDUHdr		hdr;
    pmInDom		indom;
    __pmTimeval		when;			/* desired time */
    int			inst;			/* may be PM_IN_NULL */
    int			namelen;		/* chars in name[], may be 0 */
    char		name[sizeof(int)];	/* may be missing */
} instance_req_t;

int
__pmSendInstanceReq(int fd, int mode, const __pmTimeval *when, pmInDom indom, 
		    int inst, const char *name)
{
    instance_req_t	*pp;
    int			need;

    if (mode == PDU_BINARY) {
	need = sizeof(instance_req_t) - sizeof(int);
	if (name != NULL)
	    need += sizeof(__pmPDU)*((strlen(name) - 1 + sizeof(__pmPDU))/sizeof(__pmPDU));
	if ((pp = (instance_req_t *)__pmFindPDUBuf(sizeof(need))) == NULL)
	    return -errno;
	pp->hdr.len = need;
	pp->hdr.type = PDU_INSTANCE_REQ;
	pp->when.tv_sec = htonl((__int32_t)when->tv_sec);
	pp->when.tv_usec = htonl((__int32_t)when->tv_usec);
	pp->indom = __htonpmInDom(indom);
	pp->inst = htonl(inst);
	if (name == NULL)
	    pp->namelen = 0;
	else {
	    pp->namelen = (int)strlen(name);
	    memcpy((void *)pp->name, (void *)name, pp->namelen);
#ifdef PCP_DEBUG
	    if ((pp->namelen % sizeof(__pmPDU)) != 0) {
		/* for Purify */
		int	pad;
		char	*padp = pp->name + pp->namelen;
		for (pad = sizeof(__pmPDU) - 1; pad >= (pp->namelen % sizeof(__pmPDU)); pad--)
		    *padp++ = '~';	/* buffer end */
	    }
#endif
	    pp->namelen = htonl(pp->namelen);
	}

	return __pmXmitPDU(fd, (__pmPDU *)pp);
    }
    else {
	/* assume PDU_ASCII */
	int		nbytes, sts;

	if (name == NULL) {
	    if (inst == PM_IN_NULL)
		sprintf(__pmAbuf, "INSTANCE_REQ %d ? ?\n", indom);
	    else
		sprintf(__pmAbuf, "INSTANCE_REQ %d %d ?\n", indom, inst);
	}
	else
	    sprintf(__pmAbuf, "INSTANCE_REQ %d ? %s\n", indom, name);
	nbytes = (int)strlen(__pmAbuf);
	sts = __pmXmitAscii(fd, __pmAbuf, nbytes);
	if (sts < 0)
	    return sts;
	return 0;
    }
}

int
__pmDecodeInstanceReq(__pmPDU *pdubuf, int mode, __pmTimeval *when, pmInDom *indom, int *inst, char **name)
{
    instance_req_t	*pp;

    if (mode == PDU_ASCII) {
	/* Incoming ASCII request PDUs not supported */
	return PM_ERR_NOASCII;
    }

    pp = (instance_req_t *)pdubuf;
    when->tv_sec = ntohl(pp->when.tv_sec);
    when->tv_usec = ntohl(pp->when.tv_usec);
    *indom = __ntohpmInDom(pp->indom);
    *inst = ntohl(pp->inst);
    pp->namelen = ntohl(pp->namelen);
    if (pp->namelen) {
	if ((*name = (char *)malloc(pp->namelen+1)) == NULL)
	    return -errno;
	strncpy(*name, pp->name, pp->namelen);
	(*name)[pp->namelen] = '\0';
    }
    else
	*name = NULL;

    return 0;
}

/*
 * PDU for pm*InDom result (PDU_INSTANCE)
 */
typedef struct {
    int		inst;			/* internal instance id */
    int		namelen;		/* chars in name[], may be 0 */
    char	name[sizeof(int)];	/* may be missing */
} instlist_t;

typedef struct {
    __pmPDUHdr	hdr;
    pmInDom	indom;
    int		numinst;	/* no. of elts to follow */
    __pmPDU	rest[1];	/* array of instlist_t */
} instance_t;

int
__pmSendInstance(int fd, int mode, __pmInResult *result)
{
    instance_t	*rp;
    instlist_t		*ip;
    int			need;
    int			i;
    int			j;

    if (mode == PDU_ASCII) {
	/* Outgoing ASCII result PDUs not supported */
	return PM_ERR_NOASCII;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_INDOM)
	__pmDumpInResult(stderr, result);
#endif

    need = sizeof(*rp) - sizeof(rp->rest);
    /* instlist_t + name rounded up to a __pmPDU boundary */
    for (i = 0; i < result->numinst; i++) {
	need += sizeof(*ip) - sizeof(ip->name);
	if (result->namelist != NULL)
	    need += sizeof(__pmPDU)*((strlen(result->namelist[i]) - 1 + sizeof(__pmPDU))/sizeof(__pmPDU));
    }

    if ((rp = (instance_t *)__pmFindPDUBuf(need)) == NULL)
	return -errno;
    rp->hdr.len = need;
    rp->hdr.type = PDU_INSTANCE;
    rp->indom = __htonpmInDom(result->indom);
    rp->numinst = htonl(result->numinst);
    j = 0;
    for (i = 0; i < result->numinst; i++) {
	ip = (instlist_t *)&rp->rest[j/sizeof(__pmPDU)];
	if (result->instlist != NULL)
	    ip->inst = htonl(result->instlist[i]);
	else
	    /* weird, but this is going to be ignored at the other end */
	    ip->inst = htonl(PM_IN_NULL);
	if (result->namelist != NULL) {
	    ip->namelen = (int)strlen(result->namelist[i]);
	    memcpy((void *)ip->name, (void *)result->namelist[i], ip->namelen);
#ifdef PCP_DEBUG
	    if ((ip->namelen % sizeof(__pmPDU)) != 0) {
		/* for Purify */
		int	pad;
		char	*padp = ip->name + ip->namelen;
		for (pad = sizeof(__pmPDU) - 1; pad >= (ip->namelen % sizeof(__pmPDU)); pad--)
		    *padp++ = '~';	/* buffer end */
	    }
#endif
	    j += sizeof(*ip) - sizeof(ip->name) +
		sizeof(__pmPDU)*((ip->namelen - 1 + sizeof(__pmPDU))/sizeof(__pmPDU));
	    ip->namelen = htonl(ip->namelen);
	}
	else {
	    ip->namelen = 0;
	    j += sizeof(*ip) - sizeof(ip->name);
	}
    }

    return __pmXmitPDU(fd, (__pmPDU *)rp);
}

int
__pmDecodeInstance(__pmPDU *pdubuf, int mode, __pmInResult **result)
{
    int			i;
    int			j;
    instance_t		*rp;
    instlist_t		*ip;
    __pmInResult		*res = NULL;
    int			sts;

    if ((res = (__pmInResult *)malloc(sizeof(*res))) == NULL)
	return -errno;
    res->instlist = NULL;
    res->namelist = NULL;

    if (mode == PDU_BINARY) {
	rp = (instance_t *)pdubuf;
	res->indom = __ntohpmInDom(rp->indom);
	res->numinst = ntohl(rp->numinst);
    }
    else {
	/* assume PDU_ASCII */
	if ((sts = __pmRecvLine(pdubuf, ABUFSIZE, __pmAbuf)) <= 0) goto badsts;
	if (sscanf(__pmAbuf, "INSTANCE %d %d", &res->indom, &res->numinst) != 2) goto bad;
    }

    if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	sts = -errno;
	goto badsts;
    }
    if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	sts = -errno;
	goto badsts;
    }
    /*
     * seems paranoid, but required for __pmFreeInResult() in the event of
     * a later error
     */
    for (i = 0; i < res->numinst; i++)
	res->namelist[i] = NULL;

    if (mode == PDU_BINARY) {
	char	*p;
	int	keep_instlist;
	int	keep_namelist;

	if (res->numinst == 1)
	    keep_instlist = keep_namelist = 0;
	else
	    keep_instlist = keep_namelist = 1;
	j = 0;
	for (i = 0; i < res->numinst; i++) {
	    ip = (instlist_t *)&rp->rest[j/sizeof(__pmPDU)];
	    res->instlist[i] = ntohl(ip->inst);
	    if (res->instlist[i] != PM_IN_NULL)
		keep_instlist = 1;
	    ip->namelen = ntohl(ip->namelen);
	    if (ip->namelen > 0)
		keep_namelist = 1;
	    if ((p = (char *)malloc(ip->namelen + 1)) == NULL) {
		sts = -errno;
		goto badsts;
	    }
	    memcpy((void *)p, (void *)ip->name, ip->namelen);
	    p[ip->namelen] = '\0';
	    res->namelist[i] = p;
	    j += sizeof(*ip) - sizeof(ip->name) +
		    sizeof(__pmPDU)*((ip->namelen - 1 + sizeof(__pmPDU))/sizeof(__pmPDU));
	}
	if (keep_instlist == 0) {
	    free(res->instlist);
	    res->instlist = NULL;
	}
	if (keep_namelist == 0) {
	    free(res->namelist[0]);
	    free(res->namelist);
	    res->namelist = NULL;
	}
    }
    else {
	char	*q;
	char	*qbase;

	for (i = 0; i <res->numinst; i++) {
	    if ((sts = __pmRecvLine(pdubuf, ABUFSIZE, __pmAbuf)) <= 0) goto badsts;
	    q = &__pmAbuf[2];
	    while (*q && isspace(*q)) q++;
	    if (sscanf(q, "%d", &res->instlist[i]) != 1) goto bad;
	    while (*q && !isspace(*q)) q++;
	    while (*q && isspace(*q)) q++;
	    qbase = q;
	    while (*q && !isspace(*q)) q++;
	    if ((res->namelist[i] = (char *)malloc(q - qbase + 1)) == NULL) {
		sts = -errno;
		goto badsts;
	    }
	    strncpy(res->namelist[i], qbase, q - qbase);
	    res->namelist[i][q-qbase] = '\0';
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_INDOM)
	__pmDumpInResult(stderr, res);
#endif
    *result = res;
    return 0;

bad:
    sts = PM_ERR_IPC;
    __pmNotifyErr(LOG_WARNING, "__pmDecodeInstance: ASCII botch @ \"%s\"\n", __pmAbuf);
    /* FALLTHRU */

badsts:
    __pmFreeInResult(res);
    /* FALLTHRU */

    return sts;
}
