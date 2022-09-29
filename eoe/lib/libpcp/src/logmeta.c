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

#ident "$Id: logmeta.c,v 2.24 1998/11/15 08:35:24 kenmcd Exp $"

#include <stdio.h>
#include "pmapi.h"
#include "impl.h"
#include <string.h>
#include <syslog.h>
#include <errno.h>

extern int	errno;

/* bytes for a length field in a header/trailer, or a string length field */
#define LENSIZE	4

#ifdef PCP_DEBUG
static char *
StrTimeval(__pmTimeval *tp)
{
    if (tp == NULL) {
	static char *null_timeval = "<null timeval>";
	return null_timeval;
    }
    else {
	static char		sbuf[13];
	static struct tm	*tmp;
	time_t 			t = tp->tv_sec;
	tmp = localtime(&t);
	sprintf(sbuf, "%02d:%02d:%02d.%03d",
	    tmp->tm_hour, tmp->tm_min, tmp->tm_sec, tp->tv_usec/1000);
	return sbuf;
    }
}
#endif

__pmHashNode *
__pmHashSearch(unsigned int key, __pmHashCtl *hcp)
{
    __pmHashNode	*hp;

    if (hcp->hsize == 0)
	return NULL;

    for (hp = hcp->hash[key % hcp->hsize]; hp != NULL; hp = hp->next) {
	if (hp->key == key)
	    return hp;
    }
    return NULL;
}

int
__pmHashAdd(unsigned int key, void *data, __pmHashCtl *hcp)
{
    __pmHashNode    *hp;
    int		k;

    hcp->nodes++;

    if (hcp->hsize == 0) {
	hcp->hsize = 1;	/* arbitrary number */
	if ((hcp->hash = (__pmHashNode **)calloc(hcp->hsize, sizeof(__pmHashNode *))) == NULL) {
	    hcp->hsize = 0;
	    return -errno;
	}
    }
    else if (hcp->nodes / 4 > hcp->hsize) {
	__pmHashNode	*tp;
	__pmHashNode	**old = hcp->hash;
	int		oldsize = hcp->hsize;

	hcp->hsize *= 2;
	if (hcp->hsize % 2) hcp->hsize++;
	if (hcp->hsize % 3) hcp->hsize += 2;
	if (hcp->hsize % 5) hcp->hsize += 2;
	if ((hcp->hash = (__pmHashNode **)calloc(hcp->hsize, sizeof(__pmHashNode *))) == NULL) {
	    hcp->hsize = oldsize;
	    hcp->hash = old;
	    return -errno;
	}
	/*
	 * re-link chains
	 */
	while (oldsize) {
	    for (hp = old[--oldsize]; hp != NULL; ) {
		tp = hp;
		hp = hp->next;
		k = tp->key % hcp->hsize;
		tp->next = hcp->hash[k];
		hcp->hash[k] = tp;
	    }
	}
	free(old);
    }

    if ((hp = (__pmHashNode *)malloc(sizeof(__pmHashNode))) == NULL)
	return -errno;

    k = key % hcp->hsize;
    hp->key = key;
    hp->data = data;
    hp->next = hcp->hash[k];
    hcp->hash[k] = hp;

    return 1;
}

int
__pmHashDel(unsigned int key, void *data, __pmHashCtl *hcp)
{
    __pmHashNode    *hp;
    __pmHashNode    *lhp = NULL;

    if (hcp->hsize == 0)
	return 0;

    for (hp = hcp->hash[key % hcp->hsize]; hp != NULL; hp = hp->next) {
	if (hp->key == key && hp->data == data) {
	    if (lhp == NULL)
		hcp->hash[key % hcp->hsize] = hp->next;
	    else
		lhp->next = hp->next;
	    free(hp);
	    return 1;
	}
	lhp = hp;
    }

    return 0;
}

static int
addindom(__pmLogCtl *lcp, pmInDom indom, const __pmTimeval *tp, int numinst, 
         int *instlist, char **namelist, int *indom_buf, int allinbuf)
{
    __pmLogInDom	*idp;
    __pmHashNode	*hp;
    int		sts;

    if ((idp = (__pmLogInDom *)malloc(sizeof(__pmLogInDom))) == NULL)
	return -errno;
    idp->stamp = *tp;		/* struct assignment */
    idp->numinst = numinst;
    idp->instlist = instlist;
    idp->namelist = namelist;
    idp->buf = indom_buf;
    idp->allinbuf = allinbuf;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOGMETA)
	fprintf(stderr, "addindom( ..., %s, %s, numinst=%d)\n",
	    pmInDomStr(indom), StrTimeval((__pmTimeval *)tp), numinst);
#endif


    if ((hp = __pmHashSearch((unsigned int)indom, &lcp->l_hashindom)) == NULL) {
	idp->next = NULL;
	sts = __pmHashAdd((unsigned int)indom, (void *)idp, &lcp->l_hashindom);
    }
    else {
	idp->next = (__pmLogInDom *)hp->data;
	hp->data = (void *)idp;
	sts = 0;
    }
    return sts;
}

/*
 * load _all_ of the hashed pmDesc and __pmLogInDom structures from the metadata
 * log file -- used at the initialization (NewContext) of an archive
 * If version 2 then
 * load all the names from the meta data and create l_pmns.
 */
int
__pmLogLoadMeta(__pmLogCtl *lcp)
{
    int		rlen;
    int		check;
    pmDesc	*dp;
    int		sts = 0;
    __pmLogHdr	h;
    FILE	*f = lcp->l_mdfp;
    int         version2 = ((lcp->l_label.ill_magic & 0xff) == PM_LOG_VERS02);
    int		numpmid = 0;
    int		n;
    
    if (version2) {
       if ((sts = __pmNewPMNS(&(lcp->l_pmns))) < 0) {
          sts = -errno;
          goto end;
       }
    }

    fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);
    for ( ; ; ) {
	n = (int)fread(&h, 1, sizeof(__pmLogHdr), f);

	/* swab hdr */
	h.len = ntohl(h.len);
	h.type = ntohl(h.type);

	if (n != sizeof(__pmLogHdr) || h.len <= 0) {
            if (feof(f)) {
		clearerr(f);
                sts = 0;
		goto end;
            }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOGMETA) {
		fprintf(stderr, "__pmLogLoadMeta: header read -> %d: expected: %d\n",
			n, (int)sizeof(__pmLogHdr));
	    }
#endif
	    if (ferror(f)) {
		clearerr(f);
		sts = -errno;
	    }
	    else
		sts = PM_ERR_LOGREC;
	    goto end;
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOGMETA) {
	    fprintf(stderr, "__pmLogLoadMeta: record len=%d, type=%d @ offset=%d\n",
		h.len, h.type, (int)(ftell(f) - sizeof(__pmLogHdr)));
	}
#endif
	rlen = h.len - (int)sizeof(__pmLogHdr) - (int)sizeof(int);
	if (h.type == TYPE_DESC) {
            numpmid++;
	    if ((dp = (pmDesc *)malloc(sizeof(pmDesc))) == NULL) {
		sts = -errno;
		goto end;
	    }
	    if ((n = (int)fread(dp, 1, sizeof(pmDesc), f)) != sizeof(pmDesc)) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOGMETA) {
		    fprintf(stderr, "__pmLogLoadMeta: pmDesc read -> %d: expected: %d\n",
			    n, (int)sizeof(pmDesc));
		}
#endif
		if (ferror(f)) {
		    clearerr(f);
		    sts = -errno;
		}
		else
		    sts = PM_ERR_LOGREC;
		goto end;
	    }
	    else {
		/* swab desc */
		dp->type = ntohl(dp->type);
		dp->sem = ntohl(dp->sem);
		dp->indom = __ntohpmInDom(dp->indom);
		dp->units = __ntohpmUnits(dp->units);
		dp->pmid = __ntohpmID(dp->pmid);
	    }

	    if ((sts = __pmHashAdd((int)dp->pmid, (void *)dp, &lcp->l_hashpmid)) < 0)
		goto end;

            if (version2) {
                char name[MAXPATHLEN];
                int numnames;
		int i;
		int len;

                /* read in the names & store in PMNS tree ... */
		if ((n = (int)fread(&numnames, 1, sizeof(numnames), f)) != 
                     sizeof(numnames)) {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_LOGMETA) {
			fprintf(stderr, "__pmLogLoadMeta: numnames read -> %d: expected: %d\n",
				n, (int)sizeof(numnames));
		    }
#endif
		    if (ferror(f)) {
			clearerr(f);
			sts = -errno;
		    }
		    else
			sts = PM_ERR_LOGREC;
		    goto end;
		}
		else {
		    /* swab numnames */
		    numnames = ntohl(numnames);
		}

 		for (i = 0; i < numnames; i++) {
		    if ((n = (int)fread(&len, 1, sizeof(len), f)) != 
			 sizeof(len)) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_LOGMETA) {
			    fprintf(stderr, "__pmLogLoadMeta: len name[%d] read -> %d: expected: %d\n",
				    i, n, (int)sizeof(len));
			}
#endif
			if (ferror(f)) {
			    clearerr(f);
			    sts = -errno;
			}
			else
			    sts = PM_ERR_LOGREC;
			goto end;
		    }
		    else {
			/* swab len */
			len = ntohl(len);
		    }

		    if ((n = (int)fread(name, 1, len, f)) != len) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_LOGMETA) {
			    fprintf(stderr, "__pmLogLoadMeta: name[%d] read -> %d: expected: %d\n",
				    i, n, len);
			}
#endif
			if (ferror(f)) {
			    clearerr(f);
			    sts = -errno;
			}
			else
			    sts = PM_ERR_LOGREC;
			goto end;
		    }
                    name[len] = '\0';
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_LOGMETA) {
			fprintf(stderr, "__pmLogLoadMeta: PMID: %s name: %s\n",
				pmIDStr(dp->pmid), name);
		    }
#endif

                    if ((sts = __pmAddPMNSNode(lcp->l_pmns, dp->pmid, name)) < 0) {
			goto end;
		    } 
		}/*for*/
            }/*version2*/
	}
	else if (h.type == TYPE_INDOM) {
	    int			*tbuf;
	    pmInDom		indom;
	    __pmTimeval		*when;
	    int			numinst;
	    int			*instlist;
	    char		**namelist;
	    char		*namebase;
	    int			*stridx;
	    int			i;
	    int			k;
	    int			allinbuf;

	    if ((tbuf = (int *)malloc(rlen)) == NULL) {
		sts = -errno;
		goto end;
	    }
	    if ((n = (int)fread(tbuf, 1, rlen, f)) != rlen) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOGMETA) {
		    fprintf(stderr, "__pmLogLoadMeta: indom read -> %d: expected: %d\n",
			    n, rlen);
		}
#endif
		if (ferror(f)) {
		    clearerr(f);
		    sts = -errno;
		}
		else
		    sts = PM_ERR_LOGREC;
		goto end;
	    }

	    k = 0;
	    when = (__pmTimeval *)&tbuf[k];
	    when->tv_sec = ntohl(when->tv_sec);
	    when->tv_usec = ntohl(when->tv_usec);
	    k += sizeof(*when)/sizeof(int);
	    indom = __ntohpmInDom((unsigned int)tbuf[k++]);
	    numinst = ntohl(tbuf[k++]);
	    if (numinst > 0) {
		instlist = &tbuf[k];
		k += numinst;
		stridx = &tbuf[k];
#if _MIPS_SZPTR == 32
		namelist = (char **)stridx;
		allinbuf = 1; /* allocation is all in tbuf */
#else
		allinbuf = 0; /* allocation for namelist + tbuf */
		/* need to allocate to hold the pointers */
		namelist = (char **)malloc(numinst*sizeof(char*));
		if (namelist == NULL) {
		    sts = -errno;
		    goto end;
		}
#endif
		k += numinst;
		namebase = (char *)&tbuf[k];
	        for (i = 0; i < numinst; i++) {
		    instlist[i] = ntohl(instlist[i]);
	            namelist[i] = &namebase[ntohl(stridx[i])];
		}
	    }
	    else {
		/* no instances, or an error */
		instlist = NULL;
		namelist = NULL;
	    }
	    if ((sts = addindom(lcp, indom, when, numinst, instlist, namelist, tbuf, allinbuf)) < 0)
		goto end;
	}
	else
	    fseek(f, (long)rlen, SEEK_CUR);
	n = (int)fread(&check, 1, sizeof(check), f);
	check = ntohl(check);
	if (n != sizeof(check) || h.len != check) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOGMETA) {
		fprintf(stderr, "__pmLogLoadMeta: trailer read -> %d or len=%d: expected %d @ offset=%d\n",
		    n, check, h.len, (int)(ftell(f) - sizeof(check)));
	    }
#endif
	    if (ferror(f)) {
		clearerr(f);
		sts = -errno;
	    }
	    else
		sts = PM_ERR_LOGREC;
	    goto end;
	}
    }/*for*/
end:
    
    fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);

    if (version2 && sts == 0) {
        __pmFixPMNSHashTab(lcp->l_pmns, numpmid, 1);
    }
    return sts;
}

/*
 * scan the hashed data structures to find a pmDesc, given a pmid
 */
int
__pmLogLookupDesc(__pmLogCtl *lcp, pmID pmid, pmDesc *dp)
{
    __pmHashNode	*hp;
    pmDesc	*tp;

    if ((hp = __pmHashSearch((unsigned int)pmid, &lcp->l_hashpmid)) == NULL)
	return PM_ERR_PMID_LOG;

    tp = (pmDesc *)hp->data;
    *dp = *tp;			/* struct assignment */
    return 0;
}

/*
 * Add a new pmDesc into the metadata log, and to the hashed data structures
 * If numnames is positive, then write out any associated PMNS names.
 */
int
__pmLogPutDesc(__pmLogCtl *lcp, const pmDesc *dp, int numnames, char **names)
{
    __pmLogHdr	h;
    FILE	*f = lcp->l_mdfp;
    pmDesc	*tdp;
    pmDesc	*odp;		/* pmDesc to write out */
    int		onumnames;	/* numnames to write out */
    int		olen;		/* length to write out */
    int		i;
    int		len;
#ifndef HAVE_NETWORK_BYTEORDER
    pmDesc	tmp;
#endif

    h.type = htonl(TYPE_DESC);
    len = sizeof(__pmLogHdr) + sizeof(pmDesc) + LENSIZE;

#ifdef HAVE_NETWORK_BYTEORDER
    odp = (pmDesc *)dp;
#else
    tmp.type = htonl(dp->type);
    tmp.sem = htonl(dp->sem);
    tmp.indom = __htonpmInDom(dp->indom);
    tmp.units = __htonpmUnits(dp->units);
    tmp.pmid = __htonpmID(dp->pmid);
    odp = &tmp;
#endif

    if (numnames > 0) {
        len += sizeof(numnames);
        for (i = 0; i < numnames; i++)
            len += LENSIZE + (int)strlen(names[i]);

	h.len = htonl(len);
	onumnames = htonl(numnames);
	if (fwrite(&h, 1, sizeof(__pmLogHdr), f) != sizeof(__pmLogHdr) ||
	    fwrite(odp, 1, sizeof(pmDesc), f) != sizeof(pmDesc) ||
            fwrite(&onumnames, 1, sizeof(numnames), f) != sizeof(numnames))
		return -errno;

        /* write out the names */
        for (i = 0; i < numnames; i++) {
	    int slen = (int)strlen(names[i]);
	    olen = htonl(slen);
            if (fwrite(&olen, 1, LENSIZE, f) != LENSIZE)
                return -errno;
            if (fwrite(names[i], 1, slen, f) != slen)
                return -errno;
        }

	olen = htonl(len);
	if (fwrite(&olen, 1, LENSIZE, f) != LENSIZE)
	    return -errno;
    }
    else {
	h.len = htonl(len);
	olen = htonl(len);
	if (fwrite(&h, 1, sizeof(__pmLogHdr), f) != sizeof(__pmLogHdr) ||
	    fwrite(odp, 1, sizeof(pmDesc), f) != sizeof(pmDesc) ||
	    fwrite(&olen, 1, LENSIZE, f) != LENSIZE)
		return -errno;
    }

    /*
     * need to make a copy of the pmDesc, and add this, since caller
     * may re-use *dp
     */
    if ((tdp = (pmDesc *)malloc(sizeof(pmDesc))) == NULL)
	return -errno;
    *tdp = *dp;		/* struct assignment */
    return __pmHashAdd((int)dp->pmid, (void *)tdp, &lcp->l_hashpmid);
}

static __pmLogInDom *
searchindom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp)
{
    __pmHashNode	*hp;
    __pmLogInDom	*idp;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOGMETA)
	fprintf(stderr, "searchindom( ..., %s, %s)\n",
	    pmInDomStr(indom), StrTimeval(tp));
#endif

    if ((hp = __pmHashSearch((unsigned int)indom, &lcp->l_hashindom)) == NULL)
	return NULL;

    idp = (__pmLogInDom *)hp->data;
    if (tp != NULL) {
	for ( ; idp != NULL; idp = idp->next) {
	    /*
	     * need first one at or earlier than the requested time
	     */
	    if (__pmTimevalSub(&idp->stamp, tp) <= 0)
		break;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOGMETA)
		fprintf(stderr, "too early for indom @ %s\n",
		    StrTimeval(&idp->stamp));
#endif
	}
	if (idp == NULL)
	    return NULL;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOGMETA)
	fprintf(stderr, "success for indom @ %s\n",
	    StrTimeval(&idp->stamp));
#endif
    return idp;
}

/*
 * for the given indom retrieve the instance domain that is correct
 * as of the latest time (tp == NULL) or at a designated
 * time
 */
int
__pmLogGetInDom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp, int **instlist, char ***namelist)
{
    __pmLogInDom	*idp = searchindom(lcp, indom, tp);

    if (idp == NULL)
	return PM_ERR_INDOM_LOG;

    *instlist = idp->instlist;
    *namelist = idp->namelist;

    return idp->numinst;
}

int
__pmLogLookupInDom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp, 
		   const char *name)
{
    __pmLogInDom	*idp = searchindom(lcp, indom, tp);
    int		i;

    if (idp == NULL)
	return PM_ERR_INDOM_LOG;

    if (idp->numinst < 0)
	return idp->numinst;

    /* full match */
    for (i = 0; i < idp->numinst; i++) {
	if (strcmp(name, idp->namelist[i]) == 0)
	    return idp->instlist[i];
    }

    /* half-baked match to first space */
    for (i = 0; i < idp->numinst; i++) {
	char	*p = idp->namelist[i];
	while (*p && *p != ' ')
	    p++;
	if (*p == ' ') {
	    if (strncmp(name, idp->namelist[i], p - idp->namelist[i]) == 0)
		return idp->instlist[i];
	}
    }

    return PM_ERR_INST_LOG;
}

int
__pmLogNameInDom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp, int inst, char **name)
{
    __pmLogInDom	*idp = searchindom(lcp, indom, tp);
    int		i;

    if (idp == NULL)
	return PM_ERR_INDOM_LOG;

    if (idp->numinst < 0)
	return idp->numinst;

    for (i = 0; i < idp->numinst; i++) {
	if (inst == idp->instlist[i]) {
	    *name = idp->namelist[i];
	    return 0;
	}
    }

    return PM_ERR_INST_LOG;
}

int
__pmLogPutInDom(__pmLogCtl *lcp, pmInDom indom, const __pmTimeval *tp, 
		int numinst, int *instlist, char **namelist)
{
    int		sts = 0;
    __pmLogHdr	h;
    int		i;
    int		wlen;
    int		strsize;
    int		*stridx;
    int		real_numinst;
    int		onuminst;
    pmInDom	oindom;
    __pmTimeval	otp;

    /*
     * Note: this routine depends upon the names pointed to by namelist[]
     *       being packed, and starting at namelist[0] ... this is exactly
     *       the format returned by pmGetInDom and __pmLogLoadMeta, so it
     *       should be OK
     */

    real_numinst = numinst > 0 ? numinst : 0;
    if ((stridx = (int *)malloc(real_numinst * sizeof(stridx[0]))) == NULL)
	return -errno;

    h.len = (int)sizeof(__pmLogHdr) + (int)sizeof(*tp) + (int)sizeof(indom) +
	    (int)sizeof(numinst) +
	    real_numinst * ((int)sizeof(instlist[0]) + (int)sizeof(stridx[0])) +
	    (int)sizeof(h.len);
    strsize = 0;
    for (i = 0; i < numinst; i++) {
	strsize += (int)strlen(namelist[i]) + 1;
	/* see Note */
	stridx[i] = (int)((ptrdiff_t)namelist[i] - (ptrdiff_t)namelist[0]);
	stridx[i] = htonl(stridx[i]); /* swab */
	instlist[i] = htonl(instlist[i]); /* swab: this is changed back after writing */
    }
    h.len += strsize;

    /* swab all output buffers */
    h.len = htonl(h.len);
    h.type = htonl(TYPE_INDOM);
    otp.tv_sec = htonl(tp->tv_sec);
    otp.tv_usec = htonl(tp->tv_usec);
    oindom = __htonpmInDom(indom);
    onuminst = htonl(numinst);

    wlen = (int)fwrite(&h, 1, sizeof(__pmLogHdr), lcp->l_mdfp);
    wlen += fwrite(&otp, 1, sizeof(otp), lcp->l_mdfp);
    wlen += fwrite(&oindom, 1, sizeof(oindom), lcp->l_mdfp);
    wlen += fwrite(&onuminst, 1, sizeof(onuminst), lcp->l_mdfp);
    if (numinst > 0) {
	wlen += fwrite(instlist, 1, real_numinst * sizeof(instlist[0]), lcp->l_mdfp);
	wlen += fwrite(stridx, 1, real_numinst * sizeof(stridx[0]), lcp->l_mdfp);
	/* see Note */
	wlen += fwrite(namelist[0], 1, strsize, lcp->l_mdfp);
    }
    wlen += fwrite(&h.len, 1, sizeof(h.len), lcp->l_mdfp);
    free(stridx);

    if (wlen != ntohl(h.len)) {
	pmprintf("__pmLogPutInDom: wrote %d, expected %d: %s\n",
	    wlen, ntohl(h.len), strerror(errno));
	pmflush();
	return -errno;
    }

    sts = addindom(lcp, indom, tp, numinst, instlist, namelist, NULL, 0);

#ifndef HAVE_NETWORK_BYTEORDER
    /* swab instance list back again so as to not upset the caller */
    for (i = 0; i < numinst; i++) {
	instlist[i] = ntohl(instlist[i]);
    }
#endif

    return sts;
}

int
pmLookupInDomArchive(pmInDom indom, const char *name)
{
    int		n;
    int		j;
    __pmHashNode	*hp;
    __pmLogInDom	*idp;
    __pmContext	*ctxp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type != PM_CONTEXT_ARCHIVE)
	    return PM_ERR_NOTARCHIVE;

	if ((hp = __pmHashSearch((unsigned int)indom, &ctxp->c_archctl->ac_log->l_hashindom)) == NULL)
	    return PM_ERR_INDOM_LOG;

	for (idp = (__pmLogInDom *)hp->data; idp != NULL; idp = idp->next) {
	    /* full match */
	    for (j = 0; j < idp->numinst; j++) {
		if (strcmp(name, idp->namelist[j]) == 0) {
		    return idp->instlist[j];
		}
	    }
	    /* half-baked match to first space */
	    for (j = 0; j < idp->numinst; j++) {
		char	*p = idp->namelist[j];
		while (*p && *p != ' ')
		    p++;
		if (*p == ' ') {
		    if (strncmp(name, idp->namelist[j], p - idp->namelist[j]) == 0)
			return idp->instlist[j];
		}
	    }
	}
	n = PM_ERR_INST_LOG;
    }

    return n;
}

int
pmNameInDomArchive(pmInDom indom, int inst, char **name)
{
    int		n;
    int		j;
    __pmHashNode	*hp;
    __pmLogInDom	*idp;
    __pmContext	*ctxp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type != PM_CONTEXT_ARCHIVE)
	    return PM_ERR_NOTARCHIVE;

	if ((hp = __pmHashSearch((unsigned int)indom, &ctxp->c_archctl->ac_log->l_hashindom)) == NULL)
	    return PM_ERR_INDOM_LOG;

	for (idp = (__pmLogInDom *)hp->data; idp != NULL; idp = idp->next) {
	    for (j = 0; j < idp->numinst; j++) {
		if (idp->instlist[j] == inst) {
		    if ((*name = strdup(idp->namelist[j])) == NULL)
			n = -errno;
		    else
			n = 0;
		    return n;
		}
	    }
	}
	n = PM_ERR_INST_LOG;
    }

    return n;
}

int
pmGetInDomArchive(pmInDom indom, int **instlist, char ***namelist)
{
    int			n;
    int			i;
    int			j;
    char		*p;
    __pmContext		*ctxp;
    __pmHashNode		*hp;
    __pmLogInDom		*idp;
    int			numinst = 0;
    int			strsize = 0;
    int			*ilist = NULL;
    char		**nlist = NULL;
    char		**olist;

    /* avoid ambiguity when no instances or errors */
    *instlist = NULL;
    *namelist = NULL;
    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type != PM_CONTEXT_ARCHIVE)
	    return PM_ERR_NOTARCHIVE;

	if ((hp = __pmHashSearch((unsigned int)indom, &ctxp->c_archctl->ac_log->l_hashindom)) == NULL)
	    return PM_ERR_INDOM_LOG;

	for (idp = (__pmLogInDom *)hp->data; idp != NULL; idp = idp->next) {
	    for (j = 0; j < idp->numinst; j++) {
		for (i = 0; i < numinst; i++) {
		    if (idp->instlist[j] == ilist[i])
			break;
		}
		if (i == numinst) {
		    numinst++;
		    if ((ilist = (int *)realloc(ilist, numinst*sizeof(ilist[0]))) == NULL) {
			__pmNoMem("pmGetInDomArchive: ilist", numinst*sizeof(ilist[0]), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    if ((nlist = (char **)realloc(nlist, numinst*sizeof(nlist[0]))) == NULL) {
			__pmNoMem("pmGetInDomArchive: nlist", numinst*sizeof(nlist[0]), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    ilist[numinst-1] = idp->instlist[j];
		    nlist[numinst-1] = idp->namelist[j];
		    strsize += strlen(idp->namelist[j])+1;
		}
	    }
	}
	if ((olist = (char **)malloc(numinst*sizeof(olist[0]) + strsize)) == NULL) {
	    __pmNoMem("pmGetInDomArchive: olist", numinst*sizeof(olist[0]) + strsize, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	p = (char *)olist;
	p += numinst * sizeof(olist[0]);
	for (i = 0; i < numinst; i++) {
	    olist[i] = p;
	    strcpy(p, nlist[i]);
	    p += strlen(nlist[i]) + 1;
	}
	free(nlist);
	*instlist = ilist;
	*namelist = olist;
	n = numinst;
    }

    return n;
}
