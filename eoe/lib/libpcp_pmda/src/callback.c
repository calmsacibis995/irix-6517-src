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

#ident "$Id: callback.c,v 2.7 1999/04/29 23:22:55 kenmcd Exp $"

#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "./libdefs.h"

/*
 * count the number of instances in an instance domain
 */

int
__pmdaCntInst(pmInDom indom, pmdaExt *pmda)
{
    int		i;

    if (indom == PM_INDOM_NULL)
	return 1;
    for (i = 0; i < pmda->e_nindoms; i++) {
	if (pmda->e_indoms[i].it_indom == indom)
	    return pmda->e_indoms[i].it_numinst;
    }
    __pmNotifyErr(LOG_WARNING, "cntinst: unknown pmInDom 0x%x", indom);
    return 0;
}

/*
 * commence a new round of instance selection
 */

void
__pmdaStartInst(pmInDom indom, pmdaExt *pmda)
{
    int		i;

    pmda->e_ordinal = pmda->e_singular = -1;
    if (indom == PM_INDOM_NULL) {
	/* singular value */
	pmda->e_singular = 0;
    }
    else { 
	for (i = 0; i < pmda->e_nindoms; i++) {
	    if (pmda->e_indoms[i].it_indom == indom) {
		/* multiple values are possible */
		pmda->e_idp = &pmda->e_indoms[i];
		pmda->e_ordinal = 0;
		break;
	    }
	}
    }
    return;
}

/* 
 * select the next instance
 */

int
__pmdaNextInst(int *inst, pmdaExt *pmda)
{
    int		j;

    if (pmda->e_singular == 0) {
	/* PM_INDOM_NULL ... just the one value */
	*inst = 0;
	pmda->e_singular = -1;
	return 1;
    }
    if (pmda->e_ordinal >= 0) {
	/* scan for next value in the profile */
	for (j = pmda->e_ordinal; j < pmda->e_idp->it_numinst; j++) {
	    if (__pmInProfile(pmda->e_idp->it_indom, 
	    		     pmda->e_prof, 
			     pmda->e_idp->it_set[j].i_inst)) {
		*inst = pmda->e_idp->it_set[j].i_inst;
		pmda->e_ordinal = j+1;
		return 1;
	    }
	}
	pmda->e_ordinal = -1;
    }
    return 0;
}

/*
 * store the profile away for the next fetch
 */

int
pmdaProfile(__pmProfile *prof, pmdaExt *pmda)
{
    pmda->e_prof = prof;	
    return 0;
}

/*
 * return desciption of an instance or instance domain
 */

int
pmdaInstance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    int			i;
    int			err = 0;
    __pmInResult  	*res;
    pmdaIndom		*idp;

    /*
     * check this is an instance domain we know about -- code below
     * assumes this test is complete
     */
    for (i = 0; i < pmda->e_nindoms; i++) {
	if (pmda->e_indoms[i].it_indom == indom)
	    break;
    }
    if (i >= pmda->e_nindoms)
	return PM_ERR_INDOM;

    idp = &pmda->e_indoms[i];
    if ((res = (__pmInResult *)malloc(sizeof(*res))) == NULL)
        return -errno;
    res->indom = indom;

    if (name == NULL && inst == PM_IN_NULL)
	res->numinst = __pmdaCntInst(indom, pmda);
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    free(res);
	    return -errno;
	}
    }
    else
	res->instlist = NULL;

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -errno;
	}
	for (i = 0; i < res->numinst; i++)
	    res->namelist[0] = NULL;
    }
    else
	res->namelist = NULL;

    if (name == NULL && inst == PM_IN_NULL) {
	/* return inst and name for everything */
	for (i = 0; i < res->numinst; i++) {
	    res->instlist[i] = idp->it_set[i].i_inst;
	    if ((res->namelist[i] = strdup(idp->it_set[i].i_name)) == NULL) {
		__pmFreeInResult(res);
		return -errno;
	    }
	}
    }
    else if (name == NULL) {
	/* given an inst, return the name */
	for (i = 0; i < idp->it_numinst; i++) {
	    if (inst == idp->it_set[i].i_inst) {
		if ((res->namelist[0] = strdup(idp->it_set[i].i_name)) == NULL) {
		    __pmFreeInResult(res);
		    return -errno;
		}
		break;
	    }
	}
	if (i == idp->it_numinst)
	    err = 1;
    }
    else if (inst == PM_IN_NULL) {
	/* given a name, return an inst */
	for (i = 0; i < idp->it_numinst; i++) {
	    if (strcmp(name, idp->it_set[i].i_name) == 0) {
		res->instlist[0] = idp->it_set[i].i_inst;
		break;
	    }
	}
	if (i == idp->it_numinst)
	    err = 1;
    }
    else
	err = 1;
    if (err == 1) {
	/* bogus arguments or instance id/name */
	__pmFreeInResult(res);
	return PM_ERR_INST;
    }

    *result = res;
    return 0;
}

/*
 * resize the pmResult and call the e_callback for each metric instance
 * required in the profile.
 */

int
pmdaFetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    static int		maxnpmids = 0;
    static pmResult	*res = NULL;

    int			i;		/* over pmidlist[] */
    int			j;		/* over metatab and vset->vlist[] */
    int			sts;
    int			need;
    int			inst;
    int			numval;
    pmValueSet		*vset;
    pmDesc		*dp;
    __pmID_int		*pmidp;
    pmdaMetric		*metap;
    pmAtomValue		atom;
    int			type;

    if (numpmid > maxnpmids) {
	if (res != NULL)
	    free(res);
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = (int)sizeof(pmResult) + (numpmid - 1) * (int)sizeof(pmValueSet *);
	if ((res = (pmResult *) malloc(need)) == NULL)
	    return -errno;
	maxnpmids = numpmid;
    }

    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;

    for (i = 0; i < numpmid; i++) {

    	dp = NULL;
	metap = NULL;
	pmidp = (__pmID_int *)&pmidlist[i];

	if (pmda->e_direct) {
/*
 * pmidp->domain is correct ... PMCD gurantees this, but
 * next to check the cluster
 */
	    if (pmidp->item < pmda->e_nmetrics &&
		pmidlist[i] == pmda->e_metrics[pmidp->item].m_desc.pmid) {
/* 
 * pmidp->item is unsigned, so must be >= 0 
 */
		metap = &pmda->e_metrics[pmidp->item];
		dp = &(metap->m_desc);
	    }
	}
	else {
	    for (j = 0; j < pmda->e_nmetrics; j++) {
		if (pmidlist[i] == pmda->e_metrics[j].m_desc.pmid) {
/*
 * found the hard way 
 */
		    metap = &pmda->e_metrics[j];
		    dp = &(metap->m_desc);
		    break;
		}
	    }
	}
	
	if (dp == NULL) {
	    __pmNotifyErr(LOG_ERR, "pmdaFetch: Requested metric %s is not defined",
			 pmIDStr(pmidlist[i]));
	    numval = PM_ERR_PMID;
	}
	else {

	    if (dp->indom != PM_INDOM_NULL) {
		/* count instances in the profile */
		numval = 0;
		/* count instances in indom */
		__pmdaStartInst(dp->indom, pmda);
		while (__pmdaNextInst(&inst, pmda)) {
		    numval++;
		}
	    }
	    else {
		/* singular instance domains */
		numval = 1;
	    }
	}

	/* Must use individual malloc()s because of pmFreeResult() */
	if (numval == 1)
	    res->vset[i] = vset = (pmValueSet *)
	    				__pmPoolAlloc(sizeof(pmValueSet));
	else if (numval > 1)
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) +
					    (numval - 1)*sizeof(pmValue));
	else
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) -
					    sizeof(pmValue));
	if (vset == NULL) {
	    sts = -errno;
	    goto error;
	}
	vset->pmid = pmidlist[i];
	vset->numval = numval;
	vset->valfmt = PM_VAL_INSITU;
	if (vset->numval <= 0)
	    continue;

	if (dp->indom == PM_INDOM_NULL)
	    inst = PM_IN_NULL;
	else {
	    __pmdaStartInst(dp->indom, pmda);
	    __pmdaNextInst(&inst, pmda);
	}
	type = dp->type;
	j = 0;
	do {
	    if (j == numval) {
		/* more instances than expected! */
		numval++;
		res->vset[i] = vset = (pmValueSet *)realloc(vset,
			    sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue));
		if (vset == NULL) {
		    sts = -errno;
		    goto error;
		}
	    }
	    vset->vlist[j].inst = inst;

	    if ((sts = (*(pmda->e_fetchCallBack))(metap, inst, &atom)) < 0) {

		if (sts == PM_ERR_PMID)
		    __pmNotifyErr(LOG_ERR, 
		        "pmdaFetch: PMID %s not handled by fetch callback\n",
				 pmIDStr(dp->pmid));
		else if (sts == PM_ERR_INST) {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_LIBPMDA) {
			__pmNotifyErr(LOG_ERR,
			    "pmdaFetch: Instance %d of PMID %s not handled by fetch callback\n",
				     inst, pmIDStr(dp->pmid));
		    }
#endif
		}
		else
		    __pmNotifyErr(LOG_ERR,
				 "pmdaFetch: Fetch callback error: %s\n",
				 pmErrStr(sts));
	    }
	    else {
		/*
		 * PMDA_INTERFACE_2
		 *	>= 0 => OK
		 * PMDA_INTERFACE_3
		 *	== 0 => no values
		 *	> 0  => OK
		 */
		e_ext_t *extp = (e_ext_t *)pmda->e_ext;

		if (extp->pmda_interface == PMDA_INTERFACE_2 ||
		    (extp->pmda_interface == PMDA_INTERFACE_3 && sts > 0)) {

		    if ((sts = __pmStuffValue(&atom, 0, &vset->vlist[j], 
					     type)) == PM_ERR_GENERIC) {
			__pmNotifyErr(LOG_ERR, 
				     "pmdaFetch: Descriptor type (%s) for metric %s is bad",
				     pmTypeStr(dp->type), pmIDStr(dp->pmid));
		    }
		    else if (sts >= 0) {
			vset->valfmt = sts;
			j++;
		    }
		}
	    }
	} while (dp->indom != PM_INDOM_NULL && __pmdaNextInst(&inst, pmda));

	if (j == 0)
	    vset->numval = sts;
	else
	    vset->numval = j;

    }
    *resp = res;
    return 0;

 error:

    if (i) {
	res->numpmid = i;
	__pmFreeResultValues(res);
    }
    return sts;
}

/*
 * Return the metric description
 */

int
pmdaDesc(pmID pmid, pmDesc *desc, pmdaExt *pmda)
{
    int			j;
    int			sts = 0;

    if (pmda->e_direct) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	/*
	 * pmidp->domain is correct ... PMCD gurantees this, but
	 * pmda->e_direct only works for a single cluster
	 */
	if (pmidp->item < pmda->e_nmetrics && 
	    pmidp->cluster == 
	     	((__pmID_int *)&pmda->e_metrics[pmidp->item].m_desc.pmid)->cluster) {
	    /* pmidp->item is unsigned, so must be >= 0 */
	    *desc = pmda->e_metrics[pmidp->item].m_desc;
	}
	else {
	    __pmNotifyErr(LOG_ERR, "Requested metric %s is not defined",
			 pmIDStr(pmid));
	    sts = PM_ERR_PMID;
	}
    }
    else {
	for (j = 0; j < pmda->e_nmetrics; j++) {
	    if (pmda->e_metrics[j].m_desc.pmid == pmid) {
		/* found the hard way */
		*desc = pmda->e_metrics[j].m_desc;
		break;
	    }
	}
	if (j == pmda->e_nmetrics) {
	    __pmNotifyErr(LOG_ERR, "Requested metric %s is not defined",
			 pmIDStr(pmid));
	    sts = PM_ERR_PMID;
	}
    }
    return sts;
}

/*
 * Return the help text for a metric
 */

int
pmdaText(int ident, int type, char **buffer, pmdaExt *pmda)
{

    if (pmda->e_help >= 0) {
	if ((type & PM_TEXT_PMID) == PM_TEXT_PMID)
	    *buffer = pmdaGetHelp(pmda->e_help, (pmID)ident, type);
	else
	    *buffer = pmdaGetInDomHelp(pmda->e_help, (pmInDom)ident, type);
    }
    else
	*buffer = NULL;

    return (*buffer == NULL) ? PM_ERR_TEXT : 0;
}

/*
 * Tell PMCD there is nothing to store
 */

/*ARGSUSED*/
int
pmdaStore(pmResult *result, pmdaExt *pmda)
{
    return -EACCES;
}








