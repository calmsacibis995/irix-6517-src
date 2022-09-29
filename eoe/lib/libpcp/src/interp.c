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

#ident "$Id: interp.c,v 2.38 1999/04/30 01:44:04 kenmcd Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <limits.h>
#include "pmapi.h"
#include "impl.h"

#define UPD_MARK_NONE	0
#define UPD_MARK_FORW	1
#define UPD_MARK_BACK	2

#if defined(HAVE_CONST_LONGLONG)
#define SIGN_64_MASK 0x8000000000000000LL
#else
#define SIGN_64_MASK 0x8000000000000000
#endif

extern int	errno;

typedef union {				/* value from pmResult */
    pmValueBlock	*pval;
    int			lval;
} value;

typedef struct instcntl {		/* metric-instance control */
    struct instcntl	*next;		/* next for this metric control */
    struct instcntl	*want;		/* ones of interest */
    struct instcntl	*unbound;	/* not yet bound above [or below] */
    int			search;		/* looking for found this one? */
    int			inst;		/* instance identifier */
    int			inresult;	/* will be in this result */
    double		t_prior;
    int			m_prior;	/* mark, not value at t_prior */
    value		v_prior;
    double		t_next;
    int			m_next;		/* mark, not value at t_next */
    value		v_next;
    double		t_first;	/* no records before this */
    double		t_last;		/* no records after this */
    struct pmidcntl	*metric;	/* back to metric control */
} instcntl_t;

static instcntl_t	*want_head;
static instcntl_t	*unbound_head;

typedef struct pmidcntl {		/* metric control */
    pmDesc		desc;
    int			valfmt;		/* used to build result */
    int			numval;		/* number of instances in this result */
    struct instcntl	*first;		/* first metric-instace control */
} pmidcntl_t;

#ifdef PCP_DEBUG
static void
printstamp(__pmTimeval *tp)
{
    static struct tm	*tmp;
    time_t t = (time_t)tp->tv_sec;

    tmp = localtime(&t);
    fprintf(stderr, "%02d:%02d:%02d.%03d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, tp->tv_usec/1000);
}
#endif

typedef struct {
    pmResult	*rp;		/* cached pmResult from __pmLogRead */
    int		sts;		/* from __pmLogRead */
    FILE	*mfp;		/* log stream */
    int		vol;		/* log volume */
    long	head_posn;	/* posn in file before forwards __pmLogRead */
    long	tail_posn;	/* posn in file after forwards __pmLogRead */
    int		mode;		/* PM_MODE_FORW or PM_MODE_BACK */
    int		used;		/* used count for LFU replacement */
} cache_t;

#define NUMCACHE 4
static cache_t		cache[NUMCACHE];

static int
cache_read(__pmArchCtl *acp, int mode, pmResult **rp)
{
    long	posn;
    cache_t	*cp;
    cache_t	*lfup;
    int		save_curvol;
    static int	round_robin = -1;

    if (acp->ac_vol == acp->ac_log->l_curvol)
	posn = ftell(acp->ac_log->l_mfp);
    else
	posn = 0;

    if (round_robin == -1) {
	/* cache initialization */
	for (cp = cache; cp < &cache[NUMCACHE]; cp++) {
	    cp->rp = NULL;
	    cp->mfp = NULL;
	}
	round_robin = 0;
    }

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_LOG) && (pmDebug & DBG_TRACE_INTERP)) {
	fprintf(stderr, "cache_read: fd=%d mode=%s vol=%d (curvol=%d) %s_posn=%ld ",
	    fileno(acp->ac_log->l_mfp),
	    mode == PM_MODE_FORW ? "forw" : "back",
	    acp->ac_vol, acp->ac_log->l_curvol,
	    mode == PM_MODE_FORW ? "head" : "tail",
	    (long)posn);
    }
#endif

    round_robin = (round_robin + 1) % NUMCACHE;
    lfup = &cache[round_robin];
    for (cp = cache; cp < &cache[NUMCACHE]; cp++) {
	if (cp->mfp == acp->ac_log->l_mfp && cp->vol == acp->ac_vol &&
	    ((mode == PM_MODE_FORW && cp->head_posn == posn) ||
	     (mode == PM_MODE_BACK && cp->tail_posn == posn)) &&
	    cp->rp != NULL) {
	    *rp = cp->rp;
	    cp->used++;
	    if (mode == PM_MODE_FORW)
		fseek(acp->ac_log->l_mfp, cp->tail_posn, SEEK_SET);
	    else
		fseek(acp->ac_log->l_mfp, cp->head_posn, SEEK_SET);
#ifdef PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_LOG) && (pmDebug & DBG_TRACE_INTERP))
		fprintf(stderr, "hit cache[%d]\n", (int)(cp - cache));
#endif
	    return cp->sts;
	}
#if 0
	if (cp->used < lfup->used)
	    lfup = cp;
#endif
    }

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_LOG) && (pmDebug & DBG_TRACE_INTERP))
	fprintf(stderr, "miss\n");
#endif

    if (lfup->rp != NULL)
	pmFreeResult(lfup->rp);

    save_curvol = acp->ac_log->l_curvol;

    lfup->sts = __pmLogRead(acp->ac_log, mode, NULL, &lfup->rp);
    if (lfup->sts < 0)
	lfup->rp = NULL;
    *rp = lfup->rp;

    if (posn == 0 || save_curvol != acp->ac_log->l_curvol) {
	/*
	 * vol switch since last time, or vol switch in __pmLogRead() ...
	 * new vol, stdio stream and we don't know where we started from
	 * ... don't cache
	 */
	lfup->mfp = NULL;
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_LOG) && (pmDebug & DBG_TRACE_INTERP))
	    fprintf(stderr, "cache_read: reload vol switch, mark cache[%d] unused\n",
		(int)(lfup - cache));
#endif
    }
    else {
	lfup->mode = mode;
	lfup->vol = acp->ac_vol;
	lfup->mfp = acp->ac_log->l_mfp;
	lfup->used = 1;
	if (mode == PM_MODE_FORW) {
	    lfup->head_posn = posn;
	    lfup->tail_posn = ftell(acp->ac_log->l_mfp);
	}
	else {
	    lfup->tail_posn = posn;
	    lfup->head_posn = ftell(acp->ac_log->l_mfp);
	}
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_LOG) && (pmDebug & DBG_TRACE_INTERP)) {
	    fprintf(stderr, "cache_read: reload cache[%d] vol=%d (curvol=%d) head=%ld tail=%ld ",
		(int)(lfup - cache), lfup->vol, acp->ac_log->l_curvol,
		(long)lfup->head_posn, (long)lfup->tail_posn);
	    if (lfup->sts == 0)
		fprintf(stderr, "sts=%d\n", lfup->sts);
	    else
		fprintf(stderr, "sts=%s\n", pmErrStr(lfup->sts));
	}
#endif
    }

    return lfup->sts;
}

void
__pmLogCacheClear(FILE *mfp)
{
    cache_t	*cp;

    for (cp = cache; cp < &cache[NUMCACHE]; cp++) {
	if (cp->mfp == mfp) {
	    if (cp->rp != NULL)
		pmFreeResult(cp->rp);
	    cp->rp = NULL;
	    cp->mfp = NULL;
	    cp->used = 0;
	}
    }
}

static void
update_bounds(__pmContext *ctxp, double t_req, pmResult *logrp, int do_mark, int *done_prior, int *done_next)
{
    /*
     * for every metric in the result from the log
     *   for every instance in the result from the log
     *     if we have ever asked for this metric and instance, update the
     *        range bounds, if necessary
     */
    int		k;
    int		i;
    __pmHashCtl	*hcp = &ctxp->c_archctl->ac_pmid_hc;
    __pmHashNode	*hp;
    pmidcntl_t	*pcp;
    instcntl_t	*icp;
    double	t_this;
    __pmTimeval	tmp;
    int		changed;

    tmp.tv_sec = (__int32_t)logrp->timestamp.tv_sec;
    tmp.tv_usec = (__int32_t)logrp->timestamp.tv_usec;
    t_this = __pmTimevalSub(&tmp, &ctxp->c_archctl->ac_log->l_label.ill_start);

    if (logrp->numpmid == 0 && do_mark != UPD_MARK_NONE) {
	/* mark record, discontinuity in log */
	for (icp = want_head; icp != NULL; icp = icp->want) {
	    if (t_this <= t_req &&
		(t_this >= icp->t_prior || icp->t_prior > t_req)) {
		/* <mark> is closer than best lower bound to date */
		icp->t_prior = t_this;
		icp->m_prior = 1;
		if (icp->v_prior.pval != NULL)
		    __pmUnpinPDUBuf((void *)icp->v_prior.pval);
		icp->v_prior.pval = NULL;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INTERP) {
		    fprintf(stderr, "pmid %s inst %d <mark> t_prior=%.3f\n",
			pmIDStr(icp->metric->desc.pmid), icp->inst, t_this);
		}
#endif
		if (icp->search && done_prior != NULL) {
		    icp->search = 0;
		    (*done_prior)++;
		}
	    }
	    if (t_this >= t_req &&
		((t_this <= icp->t_next || icp->t_next < 0) ||
		  icp->t_next < t_req)) {
		/* <mark> is closer than best upper bound to date */
		icp->t_next = t_this;
		icp->m_next = 1;
		if (icp->v_next.pval != NULL)
		    __pmUnpinPDUBuf((void *)icp->v_next.pval);
		icp->v_next.pval = NULL;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INTERP) {
		    fprintf(stderr, "pmid %s inst %d <mark> t_next=%.3f\n",
			pmIDStr(icp->metric->desc.pmid), icp->inst, t_this);
		}
#endif
		if (icp->search && done_next != NULL) {
		    icp->search = 0;
		    (*done_next)++;
		}
	    }
	}
	return;
    }

    changed = 0;
    for (k = 0; k < logrp->numpmid; k++) {
	hp = __pmHashSearch((int)logrp->vset[k]->pmid, hcp);
	if (hp == NULL)
	    continue;
	pcp = (pmidcntl_t *)hp->data;
	if (pcp->valfmt == -1 && logrp->vset[k]->numval > 0)
	    pcp->valfmt = logrp->vset[k]->valfmt;
	for (icp = pcp->first; icp != NULL; icp = icp->next) {
	    for (i = 0; i < logrp->vset[k]->numval; i++) {
		if (logrp->vset[k]->vlist[i].inst == icp->inst ||
		    icp->inst == PM_IN_NULL) {
		    /* matched on instance */
		    if (t_this <= t_req && 
			(icp->t_prior > t_req || t_this > icp->t_prior)) {
			/*
			 * at or before the requested time, and this is the
			 * closest-to-date lower bound
			 */
			changed = 1;
			if (icp->t_prior < icp->t_next && icp->t_prior >= t_req) {
			    /* shuffle prior to next */
			    icp->t_next = icp->t_prior;
			    if (pcp->valfmt == PM_VAL_INSITU)
				icp->v_next.lval = icp->v_prior.lval;
			    else {
				if (icp->v_next.pval != NULL)
				    __pmUnpinPDUBuf((void *)icp->v_next.pval);
				icp->v_next.pval = icp->v_prior.pval;
			    }
			}
			icp->t_prior = t_this;
			icp->m_prior = 0;
			if (pcp->valfmt == PM_VAL_INSITU)
			    icp->v_prior.lval = logrp->vset[k]->vlist[i].value.lval;
			else {
			    if (icp->v_prior.pval != NULL)
				__pmUnpinPDUBuf((void *)icp->v_prior.pval);
			    icp->v_prior.pval = logrp->vset[k]->vlist[i].value.pval;
			    __pmPinPDUBuf((void *)icp->v_prior.pval);
			}
			if (icp->search && done_prior != NULL) {
			    /* one we were looking for */
			    changed |= 2;
			    icp->search = 0;
			    (*done_prior)++;
			}
		    }
		    if (t_this >= t_req &&
			(icp->t_next < t_req || t_this < icp->t_next)) {
			/*
			 * at or after the requested time, and this is the
			 * closest-to-date upper bound
			 */
			changed |= 1;
			if (icp->t_prior < icp->t_next && icp->t_next <= t_req) {
			    /* shuffle next to prior */
			    icp->t_prior = icp->t_next;
			    icp->m_prior = icp->m_next;
			    if (pcp->valfmt == PM_VAL_INSITU)
				icp->v_prior.lval = icp->v_next.lval;
			    else {
				if (icp->v_prior.pval != NULL)
				    __pmUnpinPDUBuf((void *)icp->v_prior.pval);
				icp->v_prior.pval = icp->v_next.pval;
			    }
			}
			icp->t_next = t_this;
			icp->m_next = 0;
			if (pcp->valfmt == PM_VAL_INSITU)
			    icp->v_next.lval = logrp->vset[k]->vlist[i].value.lval;
			else {
			    if (icp->v_next.pval != NULL)
				__pmUnpinPDUBuf((void *)icp->v_next.pval);
			    icp->v_next.pval = logrp->vset[k]->vlist[i].value.pval;
			    __pmPinPDUBuf((void *)icp->v_next.pval);
			}
			if (icp->search && done_next != NULL) {
			    /* one we were looking for */
			    changed |= 2;
			    icp->search = 0;
			    (*done_next)++;
			}
		    }
#ifdef PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_INTERP) && changed) {
			fprintf(stderr, "update%s pmid %s inst %d prior: t=%.3f",
			    changed & 2 ? "+search" : "",
			    pmIDStr(logrp->vset[k]->pmid), icp->inst, icp->t_prior);
			if (icp->m_prior)
			    fprintf(stderr, " <mark>");
			else
			    fprintf(stderr, " v=%d", icp->v_prior.lval);
			fprintf(stderr, " next: t=%.3f", icp->t_next);
			if (icp->m_next)
			    fprintf(stderr, " <mark>");
			else
			    fprintf(stderr, " v=%d", icp->v_next.lval);
			fputc('\n', stderr);
		    }
#endif
		    goto next_inst;
		}
	    }
next_inst:
	    ;
	}
    }

    return;
}

static void
do_roll(__pmContext *ctxp, double t_req)
{
    pmResult	*logrp;
    __pmTimeval	tmp;
    double	t_this;

    /*
     * now roll forwards in the direction of log reading
     * to make sure we are up to t_req
     */
    if (ctxp->c_delta > 0) {
	while (cache_read(ctxp->c_archctl, PM_MODE_FORW, &logrp) >= 0) {
	    tmp.tv_sec = (__int32_t)logrp->timestamp.tv_sec;
	    tmp.tv_usec = (__int32_t)logrp->timestamp.tv_usec;
	    t_this = __pmTimevalSub(&tmp, &ctxp->c_archctl->ac_log->l_label.ill_start);
	    if (t_this > t_req)
		break;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_INTERP)
		fprintf(stderr, "roll forw to t=%.3f%s\n",
		    t_this, logrp->numpmid == 0 ? " <mark>" : "");
#endif
	    ctxp->c_archctl->ac_offset = ftell(ctxp->c_archctl->ac_log->l_mfp);
	    ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;
	    update_bounds(ctxp, t_req, logrp, UPD_MARK_FORW, NULL, NULL);
	}
    }
    else {
	while (cache_read(ctxp->c_archctl, PM_MODE_BACK, &logrp) >= 0) {
	    tmp.tv_sec = (__int32_t)logrp->timestamp.tv_sec;
	    tmp.tv_usec = (__int32_t)logrp->timestamp.tv_usec;
	    t_this = __pmTimevalSub(&tmp, &ctxp->c_archctl->ac_log->l_label.ill_start);
	    if (t_this < t_req)
		break;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_INTERP)
		fprintf(stderr, "roll back to t=%.3f%s\n",
		    t_this, logrp->numpmid == 0 ? " <mark>" : "");
#endif
	    ctxp->c_archctl->ac_offset = ftell(ctxp->c_archctl->ac_log->l_mfp);
	    ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;
	    update_bounds(ctxp, t_req, logrp, UPD_MARK_BACK, NULL, NULL);
	}
    }
}

int
__pmLogFetchInterp(__pmContext *ctxp, int numpmid, pmID pmidlist[], pmResult **result)
{
    int		i;
    int		j;
    int		sts;
    double	t_req;
    double	t_this;
    pmResult	*rp;
    pmResult	*logrp;
    __pmHashCtl	*hcp = &ctxp->c_archctl->ac_pmid_hc;
    __pmHashNode	*hp;
    pmidcntl_t	*pcp;
    instcntl_t	*icp;
    int		back = 0;
    int		forw = 0;
    int		done;
    int		done_roll;
    static double	t_end = 0;
    static int	dowrap = -1;
    __pmTimeval	tmp;
    struct timeval delta_tv;

    if (dowrap == -1) {
	/* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
	if (getenv("PCP_COUNTER_WRAP") == NULL)
	    dowrap = 0;
	else
	    dowrap = 1;
    }

    t_req = __pmTimevalSub(&ctxp->c_origin, &ctxp->c_archctl->ac_log->l_label.ill_start);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_INTERP) {
	fprintf(stderr, "__pmLogFetchInterp @ ");
	printstamp(&ctxp->c_origin);
	fprintf(stderr, " t_req=%.6f curvol=%d posn=%ld (vol=%d) serial=%d\n",
	    t_req, ctxp->c_archctl->ac_log->l_curvol,
	    (long)ctxp->c_archctl->ac_offset, ctxp->c_archctl->ac_vol,
	    ctxp->c_archctl->ac_serial);
    }
#endif

    /*
     * the 0.001 is magic slop for 1 msec, which is about as accurate
     * as we can expect any of this timing stuff to be ...
     */
    if (t_req < -0.001) {
	sts = PM_ERR_EOL;
	goto all_done;
    }

    if (t_req > t_end + 0.001) {
	struct timeval	end;
	__pmTimeval	tmp;

	/*
	 * past end of archive ... see if it has grown since we last looked
	 */
	if (pmGetArchiveEnd(&end) >= 0)
	    tmp.tv_sec = (__int32_t)end.tv_sec;
	    tmp.tv_usec = (__int32_t)end.tv_usec;
	    t_end = __pmTimevalSub(&tmp, &ctxp->c_archctl->ac_log->l_label.ill_start);
	if (t_req > t_end) {
	    sts = PM_ERR_EOL;
	    goto all_done;
	}
    }

    if ((rp = (pmResult *) malloc(sizeof(pmResult) + (numpmid - 1) * sizeof(pmValueSet *))) == NULL)
	return -errno;

    rp->timestamp.tv_sec = ctxp->c_origin.tv_sec;
    rp->timestamp.tv_usec = ctxp->c_origin.tv_usec;
    rp->numpmid = numpmid;

    /* zeroth pass ... clear search and inresult flags */
    for (j = 0; j < hcp->hsize; j++) {
	for (hp = hcp->hash[j]; hp != NULL; hp = hp->next) {
	    pcp = (pmidcntl_t *)hp->data;
	    for (icp = pcp->first; icp != NULL; icp = icp->next) {
		icp->search = icp->inresult = 0;
		icp->unbound = icp->want = NULL;
	    }
	}
    }

    /*
     * first pass ... scan all metrics, establish which ones are in
     * the log, and which instances are being requested ... also build
     * the skeletal pmResult
     */
    want_head = NULL;
    for (j = 0; j < numpmid; j++) {
	if (pmidlist[j] == PM_ID_NULL)
	    continue;
	hp = __pmHashSearch((int)pmidlist[j], hcp);
	if (hp == NULL) {
	    /* first time we've been asked for this one in this context */
	    if ((pcp = (pmidcntl_t *)malloc(sizeof(pmidcntl_t))) == NULL) {
		__pmNoMem("__pmLogFetchInterp.pmidcntl_t", sizeof(pmidcntl_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    pcp->valfmt = -1;
	    pcp->first = NULL;
	    sts = __pmHashAdd((int)pmidlist[j], (void *)pcp, hcp);
	    if (sts < 0) {
		rp->numpmid = j;
		pmFreeResult(rp);
		return sts;
	    }
	    sts = __pmLogLookupDesc(ctxp->c_archctl->ac_log, pmidlist[j], &pcp->desc);
	    if (sts < 0)
		/* not in the archive log */
		pcp->desc.type = -1;
	    else {
		/* enumerate all the instances from the domain underneath */
		int		*instlist;
		char		**namelist;
		instcntl_t	*lcp;
		if (pcp->desc.indom == PM_INDOM_NULL) {
		    sts = 1;
		    if ((instlist = (int *)malloc(sizeof(int))) == NULL) {
			__pmNoMem("__pmLogFetchInterp.instlist", sizeof(int), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    instlist[0] = PM_IN_NULL;
		}
		else {
		    sts = pmGetInDomArchive(pcp->desc.indom, &instlist, &namelist);
		}
		lcp = NULL;
		for (i = 0; i < sts; i++) {
		    if ((icp = (instcntl_t *)malloc(sizeof(instcntl_t))) == NULL) {
			__pmNoMem("__pmLogFetchInterp.instcntl_t", sizeof(instcntl_t), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    if (lcp)
			lcp->next = icp;
		    else
			pcp->first = icp;
		    lcp = icp;
		    icp->metric = pcp;
		    icp->inresult = icp->search = 0;
		    icp->next = icp->want = icp->unbound = NULL;
		    icp->inst = instlist[i];
		    icp->t_prior = icp->t_next = icp->t_first = icp->t_last = -1;
		    icp->m_prior = icp->m_next = 1;
		    icp->v_prior.pval = icp->v_next.pval = NULL;
		}
		if (sts > 0) {
		    free(instlist);
		    if (pcp->desc.indom != PM_INDOM_NULL)
			free(namelist);
		}
	    }
	}
	else
	    /* seen this one before */
	    pcp = (pmidcntl_t *)hp->data;

	pcp->numval = 0;
	if (pcp->desc.type == -1) {
	    pcp->numval = PM_ERR_PMID_LOG;
	}
	else if (pcp->desc.indom != PM_INDOM_NULL) {
	    /* use the profile to filter the instances to be returned */
	    for (icp = pcp->first; icp != NULL; icp = icp->next) {
		if (__pmInProfile(pcp->desc.indom, ctxp->c_instprof, icp->inst)) {
		    icp->inresult = 1;
		    icp->want = want_head;
		    want_head = icp;
		    pcp->numval++;
		}
		else
		    icp->inresult = 0;
	    }
	}
	else {
	    pcp->first->inresult = 1;
	    pcp->first->want = want_head;
	    want_head = pcp->first;
	    pcp->numval = 1;
	}
    }

    if (ctxp->c_archctl->ac_serial == 0) {
	/* need gross positioning from temporal index */
	__pmLogSetTime(ctxp);
	ctxp->c_archctl->ac_offset = ftell(ctxp->c_archctl->ac_log->l_mfp);
	ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;

	/*
	 * and now fine-tuning ...
	 * back-up (relative to the direction we are reading the log)
	 * to make sure
	 */
	if (ctxp->c_delta > 0) {
	    while (cache_read(ctxp->c_archctl, PM_MODE_BACK, &logrp) >= 0) {
		tmp.tv_sec = (__int32_t)logrp->timestamp.tv_sec;
		tmp.tv_usec = (__int32_t)logrp->timestamp.tv_usec;
		t_this = __pmTimevalSub(&tmp, &ctxp->c_archctl->ac_log->l_label.ill_start);
		if (t_this <= t_req) {
		    break;
		}
		ctxp->c_archctl->ac_offset = ftell(ctxp->c_archctl->ac_log->l_mfp);
		ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;
		update_bounds(ctxp, t_req, logrp, UPD_MARK_NONE, NULL, NULL);
	    }
	}
	else {
	    while (cache_read(ctxp->c_archctl, PM_MODE_FORW, &logrp) >= 0) {
		tmp.tv_sec = (__int32_t)logrp->timestamp.tv_sec;
		tmp.tv_usec = (__int32_t)logrp->timestamp.tv_usec;
		t_this = __pmTimevalSub(&tmp, &ctxp->c_archctl->ac_log->l_label.ill_start);
		if (t_this > t_req) {
		    break;
		}
		ctxp->c_archctl->ac_offset = ftell(ctxp->c_archctl->ac_log->l_mfp);
		ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;
		update_bounds(ctxp, t_req, logrp, UPD_MARK_NONE, NULL, NULL);
	    }
	}
	ctxp->c_archctl->ac_serial = 1;
    }

    /* get to the last remembered place */
    __pmLogChangeVol(ctxp->c_archctl->ac_log, ctxp->c_archctl->ac_vol);
    fseek(ctxp->c_archctl->ac_log->l_mfp, ctxp->c_archctl->ac_offset, SEEK_SET);

    /*
     * optimization to supress roll forwards unless really needed ...
     * if the sample interval is much shorter than the time between log
     * records, then do not roll forwards unless some scanning is
     * required ... and if scanning is required in the "forwards"
     * direction, no need to roll forwards
     */
    done_roll = 0;

    /*
     * second pass ... see which metrics are not currently bounded below
     */
    unbound_head = NULL;
    for (j = 0; j < numpmid; j++) {
	if (pmidlist[j] == PM_ID_NULL)
	    continue;
	hp = __pmHashSearch((int)pmidlist[j], hcp);
	pcp = (pmidcntl_t *)hp->data;
	if (pcp->numval > 0) {
	    for (icp = pcp->first; icp != NULL; icp = icp->next) {
		if (!icp->inresult)
		    continue;
		if (icp->t_first >= 0 && t_req < icp->t_first)
		    /* before earliest, don't bother */
		    continue;
retry_back:
		if (icp->t_prior < 0 || icp->t_prior > t_req ||
		    (icp->m_next && icp->t_next < t_req)) {
		    if (back == 0 && !done_roll) {
			done_roll = 1;
			if (ctxp->c_delta > 0)  {
			    /* forwards before scanning back */
			    do_roll(ctxp, t_req);
			    goto retry_back;
			}
		    }
		    back++;
		    icp->search = 1;
		    icp->unbound = unbound_head;
		    unbound_head = icp;
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_INTERP)
			fprintf(stderr, "search back for inst %d and pmid %s\n",
			    icp->inst, pmIDStr(pmidlist[j]));
#endif
		}
	    }
	}
    }

    if (back) {
	/*
	 * at least one metric requires a bound from earlier in the log ...
	 * position ourselves, ... and search
	 */
	__pmLogChangeVol(ctxp->c_archctl->ac_log, ctxp->c_archctl->ac_vol);
	fseek(ctxp->c_archctl->ac_log->l_mfp, ctxp->c_archctl->ac_offset, SEEK_SET);
	done = 0;

	while (done < back) {
	    if (cache_read(ctxp->c_archctl, PM_MODE_BACK, &logrp) < 0) {
		/* ran into start of log */
		break;
	    }
	    tmp.tv_sec = (__int32_t)logrp->timestamp.tv_sec;
	    tmp.tv_usec = (__int32_t)logrp->timestamp.tv_usec;
	    t_this = __pmTimevalSub(&tmp, &ctxp->c_archctl->ac_log->l_label.ill_start);
	    if (ctxp->c_delta < 0 && t_this >= t_req) {
		/* going backwards, and not up to t_req yet */
		ctxp->c_archctl->ac_offset = ftell(ctxp->c_archctl->ac_log->l_mfp);
		ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;
	    }
	    update_bounds(ctxp, t_req, logrp, UPD_MARK_BACK, &done, NULL);

	    /*
	     * forget about those that can never be found from here
	     * in this direction
	     */
	    for (icp = unbound_head; icp != NULL; icp = icp->unbound) {
		if (icp->search && t_this <= icp->t_first) {
		    icp->search = 0;
		    done++;
		}
	    }
	}
	/* end of search, trim t_first as required */
	for (icp = unbound_head; icp != NULL; icp = icp->unbound) {
	    if ((icp->t_prior == -1 || icp->t_prior > t_req) &&
		icp->t_first < t_req) {
		icp->t_first = t_req;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INTERP) {
		    fprintf(stderr, "pmid %s inst %d t_first=%.3f\n",
			pmIDStr(icp->metric->desc.pmid), icp->inst, icp->t_first);
		}
#endif
	    }
	    icp->search = 0;
	}
    }

    /*
     * third pass ... see which metrics are not currently bounded above
     */
    unbound_head = NULL;
    for (j = 0; j < numpmid; j++) {
	if (pmidlist[j] == PM_ID_NULL)
	    continue;
	hp = __pmHashSearch((int)pmidlist[j], hcp);
	pcp = (pmidcntl_t *)hp->data;
	if (pcp->numval > 0) {
	    for (icp = pcp->first; icp != NULL; icp = icp->next) {
		if (!icp->inresult)
		    continue;
		if (icp->t_last >= 0 && t_req > icp->t_last)
		    /* after latest, don't bother */
		    continue;
retry_forw:
		if (icp->t_next < 0 || icp->t_next < t_req ||
		    (icp->m_prior && icp->t_prior > t_req)) {
		    if (forw == 0 && !done_roll) {
			done_roll = 1;
			if (ctxp->c_delta < 0)  {
			    /* backwards before scanning forwards */
			    do_roll(ctxp, t_req);
			    goto retry_forw;
			}
		    }
		    forw++;
		    icp->search = 1;
		    icp->unbound = unbound_head;
		    unbound_head = icp;
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_INTERP)
			fprintf(stderr, "search forw for inst %d and pmid %s\n",
			    icp->inst, pmIDStr(pmidlist[j]));
#endif
		}
	    }
	}
    }

    if (forw) {
	/*
	 * at least one metric requires a bound from later in the log ...
	 * position ourselves ... and search
	 */
	__pmLogChangeVol(ctxp->c_archctl->ac_log, ctxp->c_archctl->ac_vol);
	fseek(ctxp->c_archctl->ac_log->l_mfp, ctxp->c_archctl->ac_offset, SEEK_SET);
	done = 0;

	while (done < forw) {
	    if (cache_read(ctxp->c_archctl, PM_MODE_FORW, &logrp) < 0) {
		/* ran into end of log */
		break;
	    }
	    tmp.tv_sec = (__int32_t)logrp->timestamp.tv_sec;
	    tmp.tv_usec = (__int32_t)logrp->timestamp.tv_usec;
	    t_this = __pmTimevalSub(&tmp, &ctxp->c_archctl->ac_log->l_label.ill_start);
	    if (ctxp->c_delta > 0 && t_this <= t_req) {
		/* going forwards, and not up to t_req yet */
		ctxp->c_archctl->ac_offset = ftell(ctxp->c_archctl->ac_log->l_mfp);
		ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;
	    }
	    update_bounds(ctxp, t_req, logrp, UPD_MARK_FORW, NULL, &done);

	    /*
	     * forget about those that can never be found from here
	     * in this direction
	     */
	    for (icp = unbound_head; icp != NULL; icp = icp->unbound) {
		if (icp->search && icp->t_last >= 0 && t_this >= icp->t_last) {
		    icp->search = 0;
		    done++;
		}
	    }
	}
	/* end of search, trim t_last as required */
	for (icp = unbound_head; icp != NULL; icp = icp->unbound) {
	    if (icp->t_next < t_req &&
		(icp->t_last < 0 || t_req < icp->t_last)) {
		icp->t_last = t_req;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INTERP) {
		    fprintf(stderr, "pmid %s inst %d t_last=%.3f\n",
			pmIDStr(icp->metric->desc.pmid), icp->inst, icp->t_last);
		}
#endif
	    }
	    icp->search = 0;
	}
    }

    /*
     * check to see how many qualifying values there are really going to be
     */
    for (j = 0; j < numpmid; j++) {
	if (pmidlist[j] == PM_ID_NULL)
	    continue;
	hp = __pmHashSearch((int)pmidlist[j], hcp);
	pcp = (pmidcntl_t *)hp->data;
	for (icp = pcp->first; icp != NULL; icp = icp->next) {
	    if (!icp->inresult)
		continue;
	    if (pcp->desc.sem == PM_SEM_DISCRETE) {
		if (icp->m_prior || icp->t_prior == -1 ||
		    icp->t_prior > t_req) {
		    /* no earlier value, so no value */
		    pcp->numval--;
		    icp->inresult = 0;
		}
	    }
	    else {
		/* assume COUNTER or INSTANT */
		if (icp->m_prior || icp->t_prior == -1 ||
		    icp->t_prior > t_req ||
		    icp->m_next || icp->t_next == -1 || icp->t_next < t_req) {
		    /* in mid-range, and no bound, so no value */
		    pcp->numval--;
		    icp->inresult = 0;
		}
		else if (pcp->desc.sem == PM_SEM_COUNTER) {
		    /*
		     * for counters, has to be arithmetic also,
		     * else cannot interpolate ...
		     */
		    if (pcp->desc.type != PM_TYPE_32 &&
			pcp->desc.type != PM_TYPE_U32 &&
			pcp->desc.type != PM_TYPE_64 &&
			pcp->desc.type != PM_TYPE_U64 &&
			pcp->desc.type != PM_TYPE_FLOAT &&
			pcp->desc.type != PM_TYPE_DOUBLE)
			    pcp->numval = PM_ERR_VALUE;
		}
	    }
	}
    }

    for (j = 0; j < numpmid; j++) {
	if (pmidlist[j] == PM_ID_NULL) {
	    rp->vset[j] = (pmValueSet *)malloc(sizeof(pmValueSet) -
					    sizeof(pmValue));
	}
	else {
	    hp = __pmHashSearch((int)pmidlist[j], hcp);
	    pcp = (pmidcntl_t *)hp->data;

	    if (pcp->numval == 1)
		rp->vset[j] = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	    else if (pcp->numval > 1)
		rp->vset[j] = (pmValueSet *)malloc(sizeof(pmValueSet) +
						(pcp->numval - 1)*sizeof(pmValue));
	    else
		rp->vset[j] = (pmValueSet *)malloc(sizeof(pmValueSet) -
						sizeof(pmValue));
	}

	if (rp->vset[j] == NULL) {
	    __pmNoMem("__pmLogFetchInterp.vset", sizeof(pmValueSet), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}

	rp->vset[j]->pmid = pmidlist[j];
	if (pmidlist[j] == PM_ID_NULL) {
	    rp->vset[j]->numval = 0;
	    continue;
	}
	rp->vset[j]->numval = pcp->numval;
	rp->vset[j]->valfmt = pcp->valfmt;

	i = 0;
	if (pcp->numval > 0) {
	    for (icp = pcp->first; icp != NULL; icp = icp->next) {
		if (!icp->inresult)
		    continue;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INTERP && done_roll) {
		    fprintf(stderr, "pmid %s inst %d prior: t=%.6f",
			    pmIDStr(pmidlist[j]), icp->inst, icp->t_prior);
		    if (icp->m_prior)
			fprintf(stderr, " <mark>");
		    else
			fprintf(stderr, " v=%d", icp->v_prior.lval);
		    fprintf(stderr, " next: t=%.6f", icp->t_next);
		    if (icp->m_next)
			fprintf(stderr, " <mark>");
		    else
			fprintf(stderr, " v=%d", icp->v_next.lval);
		    fputc('\n', stderr);
		}
#endif
		rp->vset[j]->vlist[i].inst = icp->inst;
		if (pcp->desc.type == PM_TYPE_32 || pcp->desc.type == PM_TYPE_U32) {
		    if (icp->t_prior == t_req)
			rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
		    else if (icp->t_next == t_req)
			rp->vset[j]->vlist[i++].value.lval = icp->v_next.lval;
		    else {
			if (pcp->desc.sem == PM_SEM_DISCRETE) {
			    if (icp->t_prior >= 0)
				rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
			}
			else if (pcp->desc.sem == PM_SEM_INSTANT) {
			    if (icp->t_prior >= 0 && icp->t_next >= 0)
				rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
			}
			else {
			    /* assume COUNTER */
			    if (icp->t_prior >= 0 && icp->t_next >= 0) {
				if (pcp->desc.type == PM_TYPE_32) {
				    if (icp->v_next.lval >= icp->v_prior.lval ||
					dowrap == 0) {
					rp->vset[j]->vlist[i++].value.lval = 0.5 +
					    icp->v_prior.lval + (t_req - icp->t_prior) *
					    (icp->v_next.lval - icp->v_prior.lval) /
					    (icp->t_next - icp->t_prior);
				    }
				    else {
					/* not monotonic increasing and want wrap */
					rp->vset[j]->vlist[i++].value.lval = 0.5 +
					    (t_req - icp->t_prior) *
					    (__int32_t)(UINT_MAX - icp->v_prior.lval + 1 + icp->v_next.lval) /
					    (icp->t_next - icp->t_prior);
					rp->vset[j]->vlist[i].value.lval += icp->v_prior.lval;
				    }
				}
				else {
				    pmAtomValue     av;
				    pmAtomValue     *avp_prior = (pmAtomValue *)&icp->v_prior.lval;
				    pmAtomValue     *avp_next = (pmAtomValue *)&icp->v_next.lval;
				    if (avp_next->ul >= avp_prior->ul) {
					av.ul = 0.5 + avp_prior->ul +
						(t_req - icp->t_prior) *
						(avp_next->ul - avp_prior->ul) /
						(icp->t_next - icp->t_prior);
				    }
				    else {
					/* not monotonic increasing */
					if (dowrap) {
					    av.ul = 0.5 +
						    (t_req - icp->t_prior) *
						    (__uint32_t)(UINT_MAX - avp_prior->ul + 1 + avp_next->ul ) /
						    (icp->t_next - icp->t_prior);
					    av.ul += avp_prior->ul;
					}
					else {
					    __uint32_t	tmp;
					    tmp = avp_prior->ul - avp_next->ul;
					    av.ul = 0.5 + avp_prior->ul -
						    (t_req - icp->t_prior) * tmp /
						    (icp->t_next - icp->t_prior);
					}
				    }
				    rp->vset[j]->vlist[i++].value.lval = av.ul;
				}
			    }
			}
		    }
		}
		else if (pcp->desc.type == PM_TYPE_FLOAT && icp->metric->valfmt == PM_VAL_INSITU) {
		    /* OLD style FLOAT insitu */
		    if (icp->t_prior == t_req)
			rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
		    else if (icp->t_next == t_req)
			rp->vset[j]->vlist[i++].value.lval = icp->v_next.lval;
		    else {
			if (pcp->desc.sem == PM_SEM_DISCRETE) {
			    if (icp->t_prior >= 0)
				rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
			}
			else if (pcp->desc.sem == PM_SEM_INSTANT) {
			    if (icp->t_prior >= 0 && icp->t_next >= 0)
				rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
			}
			else {
			    /* assume COUNTER */
			    pmAtomValue	av;
			    pmAtomValue	*avp_prior = (pmAtomValue *)&icp->v_prior.lval;
			    pmAtomValue	*avp_next = (pmAtomValue *)&icp->v_next.lval;
			    if (icp->t_prior >= 0 && icp->t_next >= 0) {
				av.f = avp_prior->f + (t_req - icp->t_prior) *
					(avp_next->f - avp_prior->f) /
					(icp->t_next - icp->t_prior);
				/* yes this IS correct ... */
				rp->vset[j]->vlist[i++].value.lval = av.l;
			    }
			}
		    }
		}
		else if (pcp->desc.type == PM_TYPE_FLOAT) {
		    /* NEW style FLOAT in pmValueBlock */
		    int			need;
		    pmValueBlock	*vp;
		    int			ok = 1;

		    need = PM_VAL_HDR_SIZE + sizeof(float);
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			sts = -errno;
			goto bad_alloc;
		    }
		    vp->vlen = need;
		    vp->vtype = PM_TYPE_FLOAT;
		    rp->vset[j]->valfmt = PM_VAL_DPTR;
		    rp->vset[j]->vlist[i++].value.pval = vp;
		    if (icp->t_prior == t_req)
			memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(float));
		    else if (icp->t_next == t_req)
			memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(float));
		    else {
			if (pcp->desc.sem == PM_SEM_DISCRETE) {
			    if (icp->t_prior >= 0)
				memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(float));
			    else
				ok = 0;
			}
			else if (pcp->desc.sem == PM_SEM_INSTANT) {
			    if (icp->t_prior >= 0 && icp->t_next >= 0)
				memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(float));
			    else
				ok = 0;
			}
			else {
			    /* assume COUNTER */
			    if (icp->t_prior >= 0 && icp->t_next >= 0) {
				pmAtomValue	av;
				pmAtomValue	*avp_prior = (pmAtomValue *)icp->v_prior.pval->vbuf;
				pmAtomValue	*avp_next = (pmAtomValue *)icp->v_next.pval->vbuf;
				float	f_prior;
				float	f_next;

				memcpy((void *)&f_prior, (void *)&avp_prior->f, sizeof(float));
				memcpy((void *)&f_next, (void *)&avp_next->f, sizeof(float));

				av.f = f_prior + (t_req - icp->t_prior) *
					(f_next - f_prior) /
					(icp->t_next - icp->t_prior);
				memcpy((void *)vp->vbuf, (void *)&av.f, sizeof(av.f));
			    }
			    else
				ok = 0;
			}
		    }
		    if (!ok) {
			i--;
			free(vp);
		    }
		}
		else if (pcp->desc.type == PM_TYPE_64 || pcp->desc.type == PM_TYPE_U64) {
		    int			need;
		    pmValueBlock	*vp;
		    int			ok = 1;

		    need = PM_VAL_HDR_SIZE + sizeof(__int64_t);
		    if ((vp = (pmValueBlock *)__pmPoolAlloc(need)) == NULL) {
			sts = -errno;
			goto bad_alloc;
		    }
		    vp->vlen = need;
		    if (pcp->desc.type == PM_TYPE_64)
			vp->vtype = PM_TYPE_64;
		    else
			vp->vtype = PM_TYPE_U64;
		    rp->vset[j]->valfmt = PM_VAL_DPTR;
		    rp->vset[j]->vlist[i++].value.pval = vp;
		    if (icp->t_prior == t_req)
			memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(__int64_t));
		    else if (icp->t_next == t_req)
			memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(__int64_t));
		    else {
			if (pcp->desc.sem == PM_SEM_DISCRETE) {
			    if (icp->t_prior >= 0)
				memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(__int64_t));
			    else
				ok = 0;
			}
			else if (pcp->desc.sem == PM_SEM_INSTANT) {
			    if (icp->t_prior >= 0 && icp->t_next >= 0)
				memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(__int64_t));
			    else
				ok = 0;
			}
			else {
			    /* assume COUNTER */
			    if (icp->t_prior >= 0 && icp->t_next >= 0) {
				pmAtomValue	av;
				pmAtomValue	*avp_prior = (pmAtomValue *)icp->v_prior.pval->vbuf;
				pmAtomValue	*avp_next = (pmAtomValue *)icp->v_next.pval->vbuf;
				if (pcp->desc.type == PM_TYPE_64) {
				    __int64_t	ll_prior;
				    __int64_t	ll_next;
				    memcpy(&ll_prior, &avp_prior->ll, sizeof(ll_prior));
				    memcpy(&ll_next, &avp_next->ll, sizeof(ll_next));
				    if (ll_next >= ll_prior || dowrap == 0)
					av.ll = ll_next - ll_prior;
				    else
					/* not monotonic increasing and want wrap */
					av.ll = (__int64_t)(ULONGLONG_MAX - ll_prior + 1 +  ll_next);
				    av.ll = (__int64_t)(0.5 + (double)ll_prior +
					    (t_req - icp->t_prior) * (double)av.ll / (icp->t_next - icp->t_prior));
				    memcpy((void *)vp->vbuf, (void *)&av.ll, sizeof(av.ll));
				}
				else {
				    __int64_t	ull_prior;
				    __int64_t	ull_next;
				    memcpy(&ull_prior, &avp_prior->ull, sizeof(ull_prior));
				    memcpy(&ull_next, &avp_next->ull, sizeof(ull_next));
				    if (ull_next >= ull_prior) {
					av.ull = ull_next - ull_prior;
#if !defined(HAVE_CAST_U64_DOUBLE)
					{
					    double tmp;

					    if (SIGN_64_MASK & av.ull)
						tmp = (double)(__int64_t)(av.ull & (~SIGN_64_MASK)) + (__uint64_t)SIGN_64_MASK;
					    else
						tmp = (double)(__int64_t)av.ull;

					    av.ull = (__uint64_t)(0.5 + (double)ull_prior +
						    (t_req - icp->t_prior) * tmp /
						    (icp->t_next - icp->t_prior));
					}
#else
					av.ull = (__uint64_t)(0.5 + (double)ull_prior +
						(t_req - icp->t_prior) * (double)av.ull /
						(icp->t_next - icp->t_prior));
#endif
				    }
				    else {
					/* not monotonic increasing */
					if (dowrap) {
					    av.ull = ULONGLONG_MAX - ull_prior + 1 +
						     ull_next;
#if !defined(HAVE_CAST_U64_DOUBLE)
					    {
						double tmp;

						if (SIGN_64_MASK & av.ull)
						    tmp = (double)(__int64_t)(av.ull & (~SIGN_64_MASK)) + (__uint64_t)SIGN_64_MASK;
						else
						    tmp = (double)(__int64_t)av.ull;

						av.ull = (__uint64_t)(0.5 + (double)ull_prior +
							(t_req - icp->t_prior) * tmp /
							(icp->t_next - icp->t_prior));
					    }
#else
					    av.ull = (__uint64_t)(0.5 + (double)ull_prior +
						    (t_req - icp->t_prior) * (double)av.ull /
						    (icp->t_next - icp->t_prior));
#endif
					}
					else {
					    __uint64_t	tmp;
					    tmp = ull_prior - ull_next;
#if !defined(HAVE_CAST_U64_DOUBLE)
					    {
						double xtmp;

						if (SIGN_64_MASK & av.ull)
						    xtmp = (double)(__int64_t)(tmp & (~SIGN_64_MASK)) + (__uint64_t)SIGN_64_MASK;
						else
						    xtmp = (double)(__int64_t)tmp;
							
						av.ull = (__uint64_t)(0.5 + (double)ull_prior -
							(t_req - icp->t_prior) * xtmp /
							(icp->t_next - icp->t_prior));
					    }
#else
					    av.ull = (__uint64_t)(0.5 + (double)ull_prior -
						    (t_req - icp->t_prior) * (double)tmp /
						    (icp->t_next - icp->t_prior));
#endif
					}
				    }
				    memcpy((void *)vp->vbuf, (void *)&av.ull, sizeof(av.ull));
				}
			    }
			    else
				ok = 0;
			}
		    }
		    if (!ok) {
			i--;
			free(vp);
		    }
		}
		else if (pcp->desc.type == PM_TYPE_DOUBLE) {
		    int			need;
		    pmValueBlock	*vp;
		    int			ok = 1;

		    need = PM_VAL_HDR_SIZE + sizeof(double);
		    if ((vp = (pmValueBlock *)__pmPoolAlloc(need)) == NULL) {
			sts = -errno;
			goto bad_alloc;
		    }
		    vp->vlen = need;
		    vp->vtype = PM_TYPE_DOUBLE;
		    rp->vset[j]->valfmt = PM_VAL_DPTR;
		    rp->vset[j]->vlist[i++].value.pval = vp;
		    if (icp->t_prior == t_req)
			memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(double));
		    else if (icp->t_next == t_req)
			memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(double));
		    else {
			if (pcp->desc.sem == PM_SEM_DISCRETE) {
			    if (icp->t_prior >= 0)
				memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(double));
			    else
				ok = 0;
			}
			else if (pcp->desc.sem == PM_SEM_INSTANT) {
			    if (icp->t_prior >= 0 && icp->t_next >= 0)
				memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(double));
			    else
				ok = 0;
			}
			else {
			    /* assume COUNTER */
			    if (icp->t_prior >= 0 && icp->t_next >= 0) {
				pmAtomValue	av;
				pmAtomValue	*avp_prior = (pmAtomValue *)icp->v_prior.pval->vbuf;
				pmAtomValue	*avp_next = (pmAtomValue *)icp->v_next.pval->vbuf;
				double	d_prior;
				double	d_next;

				memcpy((void *)&d_prior, (void *)&avp_prior->d, sizeof(double));
				memcpy((void *)&d_next, (void *)&avp_next->d, sizeof(double));

				av.d = d_prior + (t_req - icp->t_prior) *
					(d_next - d_prior) /
					(icp->t_next - icp->t_prior);
				memcpy((void *)vp->vbuf, (void *)&av.d, sizeof(av.d));
			    }
			    else
				ok = 0;
			}
		    }
		    if (!ok) {
			i--;
			free(vp);
		    }
		}
		else if ((pcp->desc.type == PM_TYPE_AGGREGATE ||
			  pcp->desc.type == PM_TYPE_STRING) &&
			 icp->t_prior >= 0) {
		    int		need;
		    pmValueBlock	*vp;

		    need = icp->v_prior.pval->vlen;

		    if (need == PM_VAL_HDR_SIZE + sizeof(__int64_t))
			vp = (pmValueBlock *)__pmPoolAlloc(need);
		    else
			vp = (pmValueBlock *)malloc(need);
		    if (vp == NULL) {
			sts = -errno;
			goto bad_alloc;
		    }
		    rp->vset[j]->valfmt = PM_VAL_DPTR;
		    rp->vset[j]->vlist[i++].value.pval = vp;
		    memcpy((void *)vp, icp->v_prior.pval, need);
		}
	    }
	}
    }

    *result = rp;
    sts = 0;

all_done:
    pmXTBdeltaToTimeval(ctxp->c_delta, ctxp->c_mode, &delta_tv);
    ctxp->c_origin.tv_sec += delta_tv.tv_sec;
    ctxp->c_origin.tv_usec += delta_tv.tv_usec;
    while (ctxp->c_origin.tv_usec > 1000000) {
	ctxp->c_origin.tv_sec++;
	ctxp->c_origin.tv_usec -= 1000000;
    }
    while (ctxp->c_origin.tv_usec < 0) {
	ctxp->c_origin.tv_sec--;
	ctxp->c_origin.tv_usec += 1000000;
    }

    return sts;

bad_alloc:
    /*
     * leaks a little (vlist[] stuff) ... but does not really matter at
     * this point, chance of anything good happening from here on are
     * pretty remote
     */
    rp->vset[j]->numval = i;
    while (++j < numpmid)
	rp->vset[j]->numval = 0;
    pmFreeResult(rp);

    return sts;

}

void
__pmLogResetInterp(__pmContext *ctxp)
{
    __pmHashCtl	*hcp = &ctxp->c_archctl->ac_pmid_hc;
    double	t_req;
    __pmHashNode	*hp;
    int		k;
    pmidcntl_t	*pcp;
    instcntl_t	*icp;

    if (hcp->hsize == 0)
	return;

    t_req = __pmTimevalSub(&ctxp->c_origin, &ctxp->c_archctl->ac_log->l_label.ill_start);

    for (k = 0; k < hcp->hsize; k++) {
	for (hp = hcp->hash[k]; hp != NULL; hp = hp->next) {
	    pcp = (pmidcntl_t *)hp->data;
	    for (icp = pcp->first; icp != NULL; icp = icp->next) {
		if (icp->t_prior > t_req || icp->t_next < t_req) {
		    icp->t_prior = icp->t_next = -1;
		    if (pcp->valfmt != PM_VAL_INSITU) {
			if (icp->v_prior.pval != NULL)
			    __pmUnpinPDUBuf((void *)icp->v_prior.pval);
			if (icp->v_next.pval != NULL)
			    __pmUnpinPDUBuf((void *)icp->v_next.pval);
		    }
		    icp->v_prior.pval = icp->v_next.pval = NULL;
		}
	    }
	}
    }
}
