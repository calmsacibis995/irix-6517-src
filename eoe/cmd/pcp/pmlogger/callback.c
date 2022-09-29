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

#ident "$Id: callback.c,v 2.25 1999/05/06 21:25:15 kenmcd Exp $"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"
#include "./logger.h"

/*
 * pro tem, we have a single context with the pmcd providing the
 * results, hence need to send the profile each time
 */
static int one_context = 1;

struct timeval	last_stamp;
__pmHashCtl	hist_hash;

/*
 * These structures allow us to keep track of the _last_ fetch
 * for each fetch in each AF group ... needed to track changes in
 * instance availability.
 */
typedef struct _lastfetch {
    struct _lastfetch	*lf_next;
    fetchctl_t		*lf_fp;
    pmResult		*lf_resp;
    __pmPDU		*lf_pb;
} lastfetch_t;

typedef struct _AFctl {
    struct _AFctl	*ac_next;
    int			ac_afid;
    lastfetch_t		*ac_fetch;
} AFctl_t;

static AFctl_t		*achead = (AFctl_t *)0;

/* clear the "metric/instance was available at last fetch" flag for each metric
 * and instance in the specified fetchgroup.
 */
static void
clearavail(fetchctl_t *fcp)
{
    indomctl_t	*idp;
    pmID	pmid;
    pmidctl_t	*pmp;
    pmidhist_t	*php;
    insthist_t	*ihp;
    __pmHashNode	*hp;
    int		i, inst;
    int		j;

    for (idp = fcp->f_idp; idp != (indomctl_t *)0; idp = idp->i_next) {
	for (pmp = idp->i_pmp; pmp != (pmidctl_t *)0; pmp = pmp->p_next) {
	    /* find the metric if it's in the history hash table */
	    pmid = pmp->p_pmid;
	    for (hp = __pmHashSearch(pmid, &hist_hash); hp != (__pmHashNode *)0; hp = hp->next)
		if (pmid == (pmID)hp->key)
		    break;
	    if (hp == (__pmHashNode *)0)
		/* not in history, no flags to update */
		continue;
	    php = (pmidhist_t *)hp->data;

	    /* now we have the metric's entry in the history */

	    if (idp->i_indom != PM_INDOM_NULL) {
		/*
		 * for each instance in the profile for this metric, find
		 * the history entry for the instance if it exists and
		 * reset the "was available at last fetch" flag
		 */
		if (idp->i_numinst)
		    for (i = 0; i < idp->i_numinst; i++) {
			inst = idp->i_instlist[i];
			ihp = &php->ph_instlist[0];
			for (j = 0; j < php->ph_numinst; j++, ihp++)
			    if (ihp->ih_inst == inst) {
				PMLC_SET_AVAIL(ihp->ih_flags, 0);
				break;
			    }
		    }
		else
		    /*
		     * if the profile specifies "all instances" clear EVERY
		     * instance's "available" flag
		     * NOTE: even instances that don't exist any more
		     */
		    for (i = 0; i < php->ph_numinst; i++)
			PMLC_SET_AVAIL(php->ph_instlist[i].ih_flags, 0);
	    }
	    /* indom is PM_INDOM_NULL */
	    else {
		/* if the single-valued metric is in the history it will have 1
		 * instance */
		ihp = &php->ph_instlist[0];
		PMLC_SET_AVAIL(ihp->ih_flags, 0);
	    }
	}
    }
}

static void
setavail(pmResult *resp)
{
    int			i;

    for (i = 0; i < resp->numpmid; i++) {
	pmID		pmid;
	pmValueSet	*vsp;
	__pmHashNode	*hp;
	pmidhist_t	*php;
	insthist_t	*ihp;
	int		j;
	
	vsp = resp->vset[i];
	pmid = vsp->pmid;
	for (hp = __pmHashSearch(pmid, &hist_hash); hp != (__pmHashNode *)0; hp = hp->next)
	    if (pmid == (pmID)hp->key)
		break;

	if (hp != (__pmHashNode *)0)
	    php = (pmidhist_t *)hp->data;
	else {
	    /* add new pmid to history if it's pmValueSet is OK */
	    if (vsp->numval <= 0)
		continue;
	    /*
	     * use the OTHER hash list to find the pmid's desc and thereby its
	     * indom
	     */
	    for (hp = __pmHashSearch(pmid, &pm_hash); hp != (__pmHashNode *)0; hp = hp->next)
		if (pmid == (pmID)hp->key)
		    break;
	    if (hp == (__pmHashNode *)0 ||
		((optreq_t *)hp->data)->r_desc == (pmDesc *)0)
		/* not set up properly yet, not much we can do ... */
		continue;
	    php = (pmidhist_t *)calloc(1, sizeof(pmidhist_t));
	    if (php == (pmidhist_t *)0) {
		__pmNoMem("setavail: new pmid hist entry calloc",
			 sizeof(pmidhist_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    php->ph_pmid = pmid;
	    php->ph_indom = ((optreq_t *)hp->data)->r_desc->indom;
	    /*
	     * now create a new insthist list for all the instances in the
	     * pmResult and we're done
	     */
	    php->ph_numinst = vsp->numval;
	    ihp = (insthist_t *)calloc(vsp->numval,
				       vsp->numval * sizeof(insthist_t));
	    if (ihp == (insthist_t *)0) {
		__pmNoMem("setavail: inst list calloc",
			 vsp->numval * sizeof(insthist_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    php->ph_instlist = ihp;
	    for (j = 0; j < vsp->numval; j++, ihp++) {
		ihp->ih_inst = vsp->vlist[j].inst;
		PMLC_SET_AVAIL(ihp->ih_flags, 1);
	    }
	    if ((j = __pmHashAdd(pmid, (void *)php, &hist_hash)) < 0) {
		extern void die(char *, int);
		die("setavail: __pmHashAdd(hist_hash)", j);
		/*NOTREACHED*/
	    }
	    
	    return;
	}

	/* update an existing pmid history entry, adding any previously unseen
	 * instances
	 */
	for (j = 0; j < vsp->numval; j++) {
	    int		inst = vsp->vlist[j].inst;
	    int		k;

	    for (k = 0; k < php->ph_numinst; k++)
		if (inst == php->ph_instlist[k].ih_inst)
		    break;

	    if (k < php->ph_numinst)
		ihp = &php->ph_instlist[k];
	    else {
		/* allocate new instance if required */
		int	need = (k + 1) * sizeof(insthist_t);

		php->ph_instlist = (insthist_t *)realloc(php->ph_instlist, need);
		if (php->ph_instlist == (insthist_t *)0) {
		    __pmNoMem("setavail: inst list realloc", need, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		ihp = &php->ph_instlist[k];
		ihp->ih_inst = inst;
		ihp->ih_flags = 0;
		php->ph_numinst++;
	    }
	    PMLC_SET_AVAIL(ihp->ih_flags, 1);
	}
    }
}

/* 
 * This has been taken straight from logmeta.c in libpcp. It is required
 * here to get the timestamp of the indom. 
 * Note that the tp argument is used to return the timestamp of the indom.
 * It is a merger of __pmLogGetIndom and searchindom.
 */
int
__localLogGetInDom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp, int **instlist, char ***namelist)
{
    __pmHashNode	*hp;
    __pmLogInDom	*idp;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOGMETA)
	fprintf(stderr, "__localLogGetInDom( ..., %s)\n",
	    pmInDomStr(indom));
#endif

    if ((hp = __pmHashSearch((unsigned int)indom, &lcp->l_hashindom)) == NULL)
	return NULL;

    idp = (__pmLogInDom *)hp->data;

    if (idp == NULL)
	return PM_ERR_INDOM_LOG;

    *instlist = idp->instlist;
    *namelist = idp->namelist;
    *tp = idp->stamp;

    return idp->numinst;
}


/*
 * compare pmResults for a particular metric, and return 1 if
 * the set of instances has changed.
 */
static int
check_inst(pmValueSet *vsp, int hint, pmResult *lrp)
{
    int		i;
    int		j;
    pmValueSet	*lvsp;

    /* Make sure vsp->pmid exists in lrp's result */
    /* and find which value set in lrp it is. */
    if (hint < lrp->numpmid && lrp->vset[hint]->pmid == vsp->pmid)
	i = hint;
    else {
	for (i = 0; i < lrp->numpmid; i++) {
	    if (lrp->vset[i]->pmid == vsp->pmid)
		break;
	}
	if (i == lrp->numpmid) {
	    fprintf(stderr, "check_inst: cannot find PMID %s in last result ...\n",
		pmIDStr(vsp->pmid));
	    __pmDumpResult(stderr, lrp);
	    return 0;
	}
    }

    lvsp = lrp->vset[i];

    if (lvsp->numval != vsp->numval)
	return 1;

    /* compare instances */
    for (i = 0; i < lvsp->numval; i++) {
	if (lvsp->vlist[i].inst != vsp->vlist[i].inst) {
	    /* the hard way */
	    for (j = 0; j < vsp->numval; j++) {
		if (lvsp->vlist[j].inst == vsp->vlist[i].inst)
		    break;
	    }
	    if (j == vsp->numval)
		return 1;
	}
    }

    return 0;
}


void
log_callback(int afid, void *data)
{
    int			i;
    int			j;
    int			k;
    int			sts;
    task_t		*tp = (task_t *)data;
    fetchctl_t		*fp;
    indomctl_t		*idp;
    pmResult		*resp;
    __pmPDU		*pb;
    AFctl_t		*acp;
    lastfetch_t		*lfp;
    lastfetch_t		*free_lfp;
    int			needindom;
    int			needti;
    static int		flushsize = 100000;
    long		old_offset;
    long		old_meta_offset;
    long		new_offset;
    long		new_meta_offset;
    int			pdu_bytes = 0;
    int			pdu_metrics = 0;
    pmID		pdu_first_pmid;
    pmID		pdu_last_pmid;
    int			numinst;
    int			*instlist;
    char		**namelist;
    __pmTimeval		tmp;
    __pmTimeval		resp_tval;
    extern int		exit_samples;
    extern int		vol_switch_samples;
    extern long		vol_switch_bytes;
    extern int		vol_samples_counter;
    extern int		parse_done;
    extern long		exit_bytes;
    extern long		vol_bytes;
    extern int		archive_version;
    extern int		linger;
    extern int		rflag;

    if (!parse_done)
	/* ignore callbacks until all of the config file has been parsed */
	return;

    /* find AFctl_t for this afid */
    for (acp = achead; acp != (AFctl_t *)0; acp = acp->ac_next) {
	if (acp->ac_afid == afid)
	    break;
    }
    if (acp == (AFctl_t *)0) {
	acp = (AFctl_t *)calloc(1, sizeof(AFctl_t));
	if (acp == (AFctl_t *)0) {
	    __pmNoMem("log_callback: new AFctl_t entry calloc",
		     sizeof(AFctl_t), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	acp->ac_afid = afid;
	acp->ac_next = achead;
	achead = acp;
    }
    else {
	/* cleanup any fetchgroups that have gone away */
	for (lfp = acp->ac_fetch; lfp != (lastfetch_t *)0; lfp = lfp->lf_next) {
	    for (fp = tp->t_fetch; fp != (fetchctl_t *)0; fp = fp->f_next) {
		if (fp == lfp->lf_fp)
		    break;
	    }
	    if (fp == (fetchctl_t *)0) {
		lfp->lf_fp = (fetchctl_t *)0;	/* mark lastfetch_t as free */
		if (lfp->lf_resp != (pmResult *)0) {
		    /* see below to understand this */
		    free(lfp->lf_resp);
		    __pmUnpinPDUBuf((void *)lfp->lf_pb);
		    lfp->lf_resp =(pmResult *)0;
		}
	    }
	}
    }

    for (fp = tp->t_fetch; fp != (fetchctl_t *)0; fp = fp->f_next) {
	
	/* find lastfetch_t for this fetch group, else make a new one */
	free_lfp = (lastfetch_t *)0;
	for (lfp = acp->ac_fetch; lfp != (lastfetch_t *)0; lfp = lfp->lf_next) {
	    if (lfp->lf_fp == fp)
		break;
	    if (lfp->lf_fp == (fetchctl_t *)0 && free_lfp == (lastfetch_t *)0)
		    free_lfp = lfp;
	}
	if (lfp == (lastfetch_t *)0) {
	    /* need new one */
	    if (free_lfp != (lastfetch_t *)0)
		lfp = free_lfp;				/* lucky */
	    else {
		lfp = (lastfetch_t *)calloc(1, sizeof(lastfetch_t));
		if (lfp == (lastfetch_t *)0) {
		    __pmNoMem("log_callback: new lastfetch_t entry calloc",
			     sizeof(lastfetch_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		lfp->lf_next = acp->ac_fetch;
		acp->ac_fetch = lfp;
	    }
	    lfp->lf_fp = fp;
	}

	if (one_context || fp->f_state & OPT_STATE_PROFILE) {
	    /* profile for this fetch group has changed */
	    pmAddProfile(PM_INDOM_NULL, 0, (int *)0);
	    for (idp = fp->f_idp; idp != (indomctl_t *)0; idp = idp->i_next) {
		if (idp->i_indom != PM_INDOM_NULL && idp->i_numinst != 0)
		    pmAddProfile(idp->i_indom, idp->i_numinst, idp->i_instlist);
	    }
	    fp->f_state &= ~OPT_STATE_PROFILE;
	}

	clearavail(fp);

	if ((sts = myFetch(fp->f_numpmid, fp->f_pmidlist, &pb)) < 0) {
	    disconnect(sts);
	    /*NOTREACHED*/
	}

	if (archive_version != PM_LOG_VERS02)
	    pb = rewrite_pdu(pb, archive_version);


	if (rflag) {
	    /* bytes = PDU len + trailer len */
	    pdu_bytes += ((__pmPDUHdr *)pb)->len + sizeof(int);
	    pdu_metrics += fp->f_numpmid;
	    if (fp == tp->t_fetch)
		pdu_first_pmid = fp->f_pmidlist[0];
	    pdu_last_pmid = fp->f_pmidlist[fp->f_numpmid-1];
	}

	/*
	 * would prefer to save this up until after any meta data and/or
	 * temporal index writes, but __pmDecodeResult changes the pointers
	 * in the pdu buffer for the non INSITU values ... sigh
	 */
	old_offset = ftell(logctl.l_mfp);
	if ((sts = __pmLogPutResult(&logctl, pb)) < 0) {
	    fprintf(stderr, "__pmLogPutResult: %s\n", pmErrStr(sts));
	    exit(1);
	}

	__pmOverrideLastFd(fileno(logctl.l_mfp));
	if ((sts = __pmDecodeResult(pb, PDU_BINARY, &resp)) < 0) {
	    fprintf(stderr, "__pmDecodeResult: %s\n", pmErrStr(sts));
	    exit(1);
	}
	setavail(resp);
	resp_tval.tv_sec = resp->timestamp.tv_sec;
	resp_tval.tv_usec = resp->timestamp.tv_usec;

	needti = 0;
	old_meta_offset = ftell(logctl.l_mdfp);
	for (i = 0; i < resp->numpmid; i++) {
	    pmValueSet	*vsp = resp->vset[i];
	    pmDesc	desc;
	    char	**names = NULL;
	    int		numnames = 0;
	    sts = __pmLogLookupDesc(&logctl, vsp->pmid, &desc);
	    if (sts < 0) {
		if (archive_version == PM_LOG_VERS02) {
		    if ((numnames = pmNameAll(vsp->pmid, &names)) < 0) {
		        fprintf(stderr, "pmNameAll: %s\n", pmErrStr(numnames));
		        exit(1);
		    }
		}
		if ((sts = pmLookupDesc(vsp->pmid, &desc)) < 0) {
		    fprintf(stderr, "pmLookupDesc: %s\n", pmErrStr(sts));
		    exit(1);
		}
		if ((sts = __pmLogPutDesc(&logctl, &desc, numnames, names)) < 0) {
		    fprintf(stderr, "__pmLogPutDesc: %s\n", pmErrStr(sts));
		    exit(1);
		}
		if (names != NULL) {
		    free(names);
                }
	    }
	    if (desc.indom != PM_INDOM_NULL && vsp->numval > 0) {
		/*
		 * __pmLogGetInDom has been replaced by __localLogGetInDom so that
		 * the timestamp of the retrieved indom is also returned. The timestamp
		 * is then used to decide if the indom needs to be refreshed.
		 */
		__pmTimeval indom_tval;
		numinst = __localLogGetInDom(&logctl, desc.indom, &indom_tval, &instlist, &namelist);
		if (numinst < 0)
		    needindom = 1;
		else {
		    needindom = 0;
		    /* Need to see if result's insts all exist
		     * somewhere in the hashed/cached insts.
		     * Thus a potential numval^2 search.
                     */
		    for (j = 0; j < vsp->numval; j++) {
			for (k = 0; k < numinst; k++) {
			    if (vsp->vlist[j].inst == instlist[k])
				break;
			}
			if (k == numinst) {
			    needindom = 1;
			    break;
			}
		    }
		}
		/* 
		 * Check here that the instance domain has not been changed
		 * by a previous iteration of this loop.
		 * So, the timestamp of resp must be after the update timestamp
		 * of the target instance domain.
		 */
		if (needindom == 0 && lfp->lf_resp != (pmResult *)0 &&
		    __pmTimevalSub(&resp_tval, &indom_tval) < 0 )
		    needindom = check_inst(vsp, i, lfp->lf_resp);

		if (needindom) {
		    /*
		     * Note.  We do NOT free() instlist and namelist allocated
		     *	  here ... look for magic below log{Put,Get}InDom ...
		     */
		    if ((numinst = pmGetInDom(desc.indom, &instlist, &namelist)) < 0) {
			fprintf(stderr, "pmGetInDom(%s): %s\n", pmInDomStr(desc.indom), pmErrStr(numinst));
			exit(1);
		    }
		    tmp.tv_sec = (__int32_t)resp->timestamp.tv_sec;
		    tmp.tv_usec = (__int32_t)resp->timestamp.tv_usec;
		    if ((sts = __pmLogPutInDom(&logctl, desc.indom, &tmp, numinst, instlist, namelist)) < 0) {
			fprintf(stderr, "__pmLogPutInDom: %s\n", pmErrStr(sts));
			exit(1);
		    }
		    needti = 1;
		}
	    }
	}

	if (ftell(logctl.l_mfp) > flushsize)
	    needti = 1;

	if (old_offset == 0 || old_offset == sizeof(__pmLogLabel)+2*sizeof(int))
	    /* first result in this volume */
	    needti = 1;

	if (needti)
	    fflush(logctl.l_mdfp);

	if (needti) {
	    /*
	     * need to unwind seek pointer to start of most recent
	     * result (but if this is the first one, skip the label
	     * record, what a crock), ... ditto for the meta data
	     */
	    if (old_offset == 0)
		old_offset = sizeof(__pmLogLabel)+2*sizeof(int);
	    new_offset = ftell(logctl.l_mfp);
	    new_meta_offset = ftell(logctl.l_mdfp);
	    fseek(logctl.l_mfp, old_offset, SEEK_SET);
	    fseek(logctl.l_mdfp, old_meta_offset, SEEK_SET);
	    tmp.tv_sec = (__int32_t)resp->timestamp.tv_sec;
	    tmp.tv_usec = (__int32_t)resp->timestamp.tv_usec;
	    __pmLogPutIndex(&logctl, &tmp);
	    /*
	     * ... and put them back
	     */
	    fseek(logctl.l_mfp, new_offset, SEEK_SET);
	    fseek(logctl.l_mdfp, new_meta_offset, SEEK_SET);
	    flushsize = ftell(logctl.l_mfp) + 100000;
	}

	last_stamp = resp->timestamp;	/* struct assignment */

	if (lfp->lf_resp != (pmResult *)0) {
	    /* release memory */
	    __pmUnpinPDUBuf((void *)lfp->lf_pb);		/* pmValueSets */
	    free(lfp->lf_resp);		/* allocated in __pmDecodeResult */
	}
	lfp->lf_resp = resp;
	lfp->lf_pb = pb;
    }

    if (rflag && tp->t_size == 0 && pdu_metrics > 0) {
	char	**names = NULL;

	tp->t_size = pdu_bytes;

	if (pdu_metrics > 1)
	    fprintf(stderr, "\nGroup [%d metrics] {\n\t", pdu_metrics);
	else
	    fprintf(stderr, "\nMetric ");
	if (archive_version == PM_LOG_VERS02) {
	    if (pmNameAll(pdu_first_pmid, &names) < 0)
		names = NULL;
	}
	if (names != NULL) {
	    fprintf(stderr, "%s", names[0]);
	    free(names);
	}
	else
	    fprintf(stderr, "%s", pmIDStr(pdu_first_pmid));
	if (pdu_metrics > 1) {
	    fprintf(stderr, "\n\t");
	    if (pdu_metrics > 2)
		fprintf(stderr, "...\n\t");
	    if (archive_version == PM_LOG_VERS02) {
		if (pmNameAll(pdu_last_pmid, &names) < 0)
		    names = NULL;
	    }
	    if (names != NULL) {
		fprintf(stderr, "%s", names[0]);
		free(names);
	    }
	    else
		fprintf(stderr, "%s", pmIDStr(pdu_last_pmid));
	    fprintf(stderr, "\n}");
	}
	fprintf(stderr, " logged ");

	if (tp->t_delta.tv_sec == 0 && tp->t_delta.tv_usec == 0)
	    fprintf(stderr, "once: %d bytes\n", pdu_bytes);
	else {
	    if (tp->t_delta.tv_usec == 0) {
		fprintf(stderr, "every %d sec: %d bytes ",
		    tp->t_delta.tv_sec, pdu_bytes);
	    }
	    else
		fprintf(stderr, "every %d.%03d sec: %d bytes ",
		    tp->t_delta.tv_sec, tp->t_delta.tv_usec / 1000, pdu_bytes);
	    fprintf(stderr, "or %.2f Mbytes/day\n",
		((double)pdu_bytes * 24 * 60 * 60) /
		(1024 * 1024 * (tp->t_delta.tv_sec + (double)tp->t_delta.tv_usec / 1000000)));
	}
    }

    if (exit_samples > 0)
	exit_samples--;

    if (exit_samples == 0)
	/* run out of samples in sample counter, so stop logging */
	run_done(0, "Sample limit reached");

    if (exit_bytes != -1 && 
        (vol_bytes + ftell(logctl.l_mfp) >= exit_bytes)) 
        /* reached exit_bytes limit, so stop logging */
        run_done(0, "Byte limit reached");

    if (vol_switch_samples > 0 &&
	++vol_samples_counter == vol_switch_samples) {
        (void)newvolume(VOL_SW_COUNTER);
    }

    if (vol_switch_bytes > 0 &&
        (ftell(logctl.l_mfp) >= vol_switch_bytes)) {
        (void)newvolume(VOL_SW_BYTES);
    }
}

int
putmark(void)
{
    struct {
	__pmPDU		hdr;
	__pmTimeval	timestamp;	/* when returned */
	int		numpmid;	/* zero PMIDs to follow */
	__pmPDU		tail;
    } mark;
    extern int	errno;

    if (last_stamp.tv_sec == 0 && last_stamp.tv_usec == 0)
	/* no earlier pmResult, no point adding a mark record */
	return 0;

    mark.hdr = htonl((int)sizeof(mark));
    mark.tail = mark.hdr;
    mark.timestamp.tv_sec = last_stamp.tv_sec;
    mark.timestamp.tv_usec = last_stamp.tv_usec + 1000;	/* + 1msec */
    if (mark.timestamp.tv_usec > 1000000) {
	mark.timestamp.tv_usec -= 1000000;
	mark.timestamp.tv_sec++;
    }
    mark.timestamp.tv_sec = htonl(mark.timestamp.tv_sec);
    mark.timestamp.tv_usec = htonl(mark.timestamp.tv_usec);
    mark.numpmid = htonl(0);

    if (fwrite(&mark, 1, sizeof(mark), logctl.l_mfp) != sizeof(mark))
	return -errno;
    else
	return 0;
}
