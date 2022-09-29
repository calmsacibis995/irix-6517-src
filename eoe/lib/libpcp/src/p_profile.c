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

#ident "$Id: p_profile.c,v 2.6 1998/11/15 08:35:24 kenmcd Exp $"

#include "pmapi.h"
#include "impl.h"

extern int	errno;

/*
 * PDU used to transmit __pmProfile prior to pmFetch (PDU_PROFILE)
 */
typedef struct {
    pmInDom	indom;
    int		state;		/* include/exclude */
    int		numinst;	/* no. of instances to follow */
    int		pad;		/* protocol backward compatibility */
} instprof_t;

typedef struct {
    __pmPDUHdr	hdr;
    int		ctxnum;
    int		g_state;	/* global include/exclude */
    int		numprof;	/* no. of elts to follow */
    int		pad;		/* protocol backward compatibility */
} profile_t;

int
__pmSendProfile(int fd, int mode, int ctxnum, __pmProfile *instprof)
{
    __pmInDomProfile	*prof, *p_end;
    profile_t		*pduProfile;
    instprof_t		*pduInstProf;
    __pmPDU		*p;
    size_t		need;
    __pmPDU		*pdubuf;
#ifndef HAVE_NETWORK_BYTEORDER
    int			j;
#endif

    if (mode == PDU_BINARY) {
	/* work out how much space we need and then alloc a pdu buf */
	need = sizeof(profile_t) + instprof->profile_len * sizeof(instprof_t);
	for (prof=instprof->profile, p_end=prof+instprof->profile_len; prof < p_end; prof++)
	    need += prof->instances_len * sizeof(int);

	if ((pdubuf = __pmFindPDUBuf((int)need)) == NULL)
	    return -errno;

	p = (__pmPDU *)pdubuf;

	/* First the profile itself */
	pduProfile = (profile_t *)p;
	pduProfile->hdr.len = (int)need;
	pduProfile->hdr.type = PDU_PROFILE;
	pduProfile->ctxnum = htonl(ctxnum);
	pduProfile->g_state = htonl(instprof->state);
	pduProfile->numprof = htonl(instprof->profile_len);
	pduProfile->pad = 0;

	p += sizeof(profile_t) / sizeof(__pmPDU);

	if (instprof->profile_len) {
	    /* Next all the profile entries (if any) in one block */
	    for (prof=instprof->profile, p_end=prof+instprof->profile_len; prof < p_end; prof++) {
		pduInstProf = (instprof_t *)p;
		pduInstProf->indom = __htonpmInDom(prof->indom);
		pduInstProf->state = htonl(prof->state);
		pduInstProf->numinst = htonl(prof->instances_len);
		pduInstProf->pad = 0;
		p += sizeof(instprof_t) / sizeof(__pmPDU);
	    }

	    /* and then all the instances */
	    for (prof=instprof->profile, p_end=prof+instprof->profile_len; prof < p_end; prof++) {
		/* and then the instances themselves (if any) */
#ifdef HAVE_NETWORK_BYTEORDER
		memcpy(p, prof->instances, prof->instances_len * sizeof(int));
		p += prof->instances_len * (sizeof(int) / sizeof(__pmPDU));
#else
		for (j = 0; j < prof->instances_len; j++, p++)
		    *p = htonl(prof->instances[j]);
#endif
	    }
	}

	return __pmXmitPDU(fd, pdubuf);
    }
    else {
	/* assume PDU_ASCII */
	int			i, j, sts;
	__psint_t		nbytes;
	__pmInDomProfile		*idp;
	char			*q;

	sprintf(__pmAbuf, "PROFILE %d %d %d\n", ctxnum, instprof->state, instprof->profile_len);
	nbytes = (int)strlen(__pmAbuf);
	sts = __pmXmitAscii(fd, __pmAbuf, (int)nbytes);
	if (sts < 0)
	    return sts;
	for (i = 0; i < instprof->profile_len; i++) {
	    idp = &instprof->profile[i];
	    sprintf(__pmAbuf, ". %d %d %d", idp->indom, idp->state, idp->instances_len);
	    q = &__pmAbuf[strlen(__pmAbuf)];
	    for (j = 0; j < idp->instances_len; j++) {
		if (j % 10 == 9) {
		    *q++ = ' ';
		    *q++ = '\\';
		    *q++ = '\n';
		    nbytes = q - __pmAbuf;
		    sts = __pmXmitAscii(fd, __pmAbuf, (int)nbytes);
		    if (sts < 0)
			return sts;
		    q = __pmAbuf;
		}
		sprintf(q, " %d", idp->instances[j]);
		while (*q) q++;
	    }
	    *q++ = '\n';
	    nbytes = q - __pmAbuf;
	    sts = __pmXmitAscii(fd, __pmAbuf, (int)nbytes);
	    if (sts < 0)
		return sts;
	}
	return 0;
    }
}

int
__pmDecodeProfile(__pmPDU *pdubuf, int mode, int *ctxnum, __pmProfile **result)
{
    __pmProfile		*instprof;
    __pmInDomProfile	*prof, *p_end;
    profile_t		*pduProfile;
    instprof_t		*pduInstProf;
    __pmPDU		*p;
#ifndef HAVE_NETWORK_BYTEORDER
    int			j;
#endif

    if (mode == PDU_BINARY) {
	p = (__pmPDU *)pdubuf;

	/* First the profile */
	pduProfile = (profile_t *)p;
	*ctxnum = ntohl(pduProfile->ctxnum);
	if ((instprof = (__pmProfile *)malloc(sizeof(__pmProfile))) == NULL)
	    return -errno;
	instprof->state = ntohl(pduProfile->g_state);
	instprof->profile_len = ntohl(pduProfile->numprof);
	p += sizeof(profile_t) / sizeof(__pmPDU);

	if (instprof->profile_len > 0) {
	    if ((instprof->profile = (__pmInDomProfile *)malloc(
		instprof->profile_len * sizeof(__pmInDomProfile))) == NULL)
		return -errno;

	    /* Next the profiles (if any) all together */
	    for (prof=instprof->profile, p_end=prof+instprof->profile_len; prof < p_end; prof++) {
		pduInstProf = (instprof_t *)p;
		prof->indom = __ntohpmInDom(pduInstProf->indom);
		prof->state = ntohl(pduInstProf->state);
		prof->instances_len = ntohl(pduInstProf->numinst);
		p += sizeof(instprof_t) / sizeof(__pmPDU);
	    }

	    /* Finally, all the instances for all profiles (if any) together */
	    for (prof=instprof->profile, p_end=prof+instprof->profile_len; prof < p_end; prof++) {
		prof->instances = (int *)malloc(prof->instances_len * sizeof(int));
		if (prof->instances == NULL)
		    return -errno;
#ifdef HAVE_NETWORK_BYTEORDER
		memcpy(prof->instances, p, prof->instances_len * sizeof(int));
		p += prof->instances_len * (sizeof(int) / sizeof(__pmPDU));
#else
		for (j = 0; j < prof->instances_len; j++, p++)
		    prof->instances[j] = ntohl(*p);
#endif
	    }
	}
	else
	    instprof->profile = NULL;

	*result = instprof;
	return 0;
    }
    else {
	/* Incoming ASCII request PDUs not supported */
	return PM_ERR_NOASCII;
    }
}
