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

#ident "$Id: check.c,v 2.7 1997/09/12 03:06:24 markgw Exp $"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include "pmapi_dev.h"
#include "./logger.h"

extern int		pmDebug;

char *chk_emess[] = {
    "No error",
    "Request for (advisory) ON conflicts with current (mandatory) ON state",
    "Request for (advisory) OFF conflicts with current (mandatory) ON state",
    "Request for (advisory) ON conflicts with current (mandatory) OFF state",
    "Request for (advisory) OFF conflicts with current (mandatory) OFF state",
};

static void
undo(task_t *tp, optreq_t *rqp, int inst)
{
    int 	j;
    int		k;

    if (rqp->r_numinst >= 1) {
	/* remove instance from list of instance */
	for (k =0, j = 0; j < rqp->r_numinst; j++) {
	    if (rqp->r_instlist[j] != inst)
		rqp->r_instlist[k++] = rqp->r_instlist[j];
	}
	rqp->r_numinst = k;
	__pmOptFetchDel(&tp->t_fetch, rqp);

	if (rqp->r_numinst == 0) {
	    /* no more instances, remove specification */
	    if (tp->t_fetch == NULL) {
		/* no more specifications, remove task */
		task_t	*xtp;
		task_t	*ltp = NULL;
		for (xtp = tasklist; xtp != NULL; xtp = xtp->t_next) {
		    if (xtp == tp) {
			if (ltp == NULL)
			    tasklist = tp->t_next;
			else
			    ltp->t_next = tp->t_next;
			break;
		    }
		    ltp = xtp;
		}
	    }
	    __pmHashDel(rqp->r_desc->pmid, (void *)rqp, &pm_hash);
	    free(rqp);
	}
	else
	    /* re-insert modified specification */
	    __pmOptFetchAdd(&tp->t_fetch, rqp);
    }
    else {
	/*
	 * TODO ... current specification is for all instances,
	 * need to remove this instance from the set ...
	 * this requires some enhancement to optFetch
	 *
	 * pro tem, this metric-instance pair may continue to get
	 * logged, even though the logging state is recorded as
	 * OFF (this is the worst thing that can happen here)
	 */
    }
}

int
chk_one(task_t *tp, pmID pmid, int inst)
{
    optreq_t	*rqp;
    task_t	*ctp;

    rqp = findoptreq(pmid, inst);
    if (rqp == NULL)
	return 0;

    ctp = rqp->r_fetch->f_aux;
    if (ctp == NULL)
	/*
	 * can only happen if same metric+inst appears more than once
	 * in the same group ... this can never be a conflict
	 */
	return 0;

    if (PMLC_GET_MAND(ctp->t_state)) {
	if (PMLC_GET_ON(ctp->t_state)) {
	    if (PMLC_GET_MAND(tp->t_state) == 0 && PMLC_GET_MAYBE(tp->t_state) == 0) {
		if (PMLC_GET_ON(tp->t_state))
		    return -1;
		else
		    return -2;
	    }
	}
	else {
	    if (PMLC_GET_MAND(tp->t_state) == 0 && PMLC_GET_MAYBE(tp->t_state) == 0) {
		if (PMLC_GET_ON(tp->t_state))
		    return -3;
		else
		    return -4;
	    }
	}
	/*
	 * new mandatory, over-rides the old mandatory
	 */
	undo(ctp, rqp, inst);
    }
    else {
	/*
	 * new anything, over-rides the old advisory
	 */
	undo(ctp, rqp, inst);
    }

    return 0;
}

int
chk_all(task_t *tp, pmID pmid)
{
    optreq_t	*rqp;
    task_t	*ctp;

    rqp = findoptreq(pmid, 0);	/*TODO, not right!*/
    if (rqp == NULL)
	return 0;

    ctp = rqp->r_fetch->f_aux;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	printf("chk_all: pmid=%s state=%s%s%s%s delta=%d.%06d\n",
		pmIDStr(pmid),
		PMLC_GET_INLOG(tp->t_state) ? " " : "N",
		PMLC_GET_AVAIL(tp->t_state) ? " " : "N",
		PMLC_GET_MAND(tp->t_state) ? "M" : "A",
		PMLC_GET_ON(tp->t_state) ? "Y" : "N",
		tp->t_delta.tv_sec, tp->t_delta.tv_usec);
	printf("compared to: state=%s%s%s%s delta=%d.%06d\n",
		PMLC_GET_INLOG(ctp->t_state) ? " " : "N",
		PMLC_GET_AVAIL(ctp->t_state) ? " " : "N",
		PMLC_GET_MAND(ctp->t_state) ? "M" : "A",
		PMLC_GET_ON(ctp->t_state) ? "Y" : "N",
		ctp->t_delta.tv_sec, ctp->t_delta.tv_usec);
    }
#endif
    return 0;
}
