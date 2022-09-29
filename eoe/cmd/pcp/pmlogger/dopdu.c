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

#ident "$Id: dopdu.c,v 2.31 1999/05/11 00:28:03 kenmcd Exp $"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"
#include "./logger.h"

#if defined(IRIX6_5)
#include <optional_sym.h>
#endif

extern int		pmDebug;
extern int		clientfd;

/* return one of these when a status request is made from a PCP 1.x pmlc */
typedef struct {
    __pmTimeval  ls_start;	/* start time for log */
    __pmTimeval  ls_last;	/* last time log written */
    __pmTimeval  ls_timenow;	/* current time */
    int		ls_state;	/* state of log (from __pmLogCtl) */
    int		ls_vol;		/* current volume number of log */
    __int64_t	ls_size;	/* size of current volume */
    char	ls_hostname[PM_LOG_MAXHOSTLEN];
				/* name of pmcd host */
    char	ls_tz[40];      /* $TZ at collection host */
    char	ls_tzlogger[40]; /* $TZ at pmlogger */
} __pmLoggerStatus_v1;

#ifdef PCP_DEBUG
/* This crawls over the data structure looking for weirdness */
void
reality_check(void)
{
    __pmHashNode		*hp;
    task_t		*tp;
    task_t		*tp2;
    fetchctl_t		*fp;
    optreq_t		*rqp;
    pmID		pmid;
    int			i = 0, j, k;

    /* check that all fetch_t's f_aux point back to their parent task */
    for (tp = tasklist; tp != NULL; tp = tp->t_next, i++) {
	if (tp->t_fetch == NULL)
	    fprintf(stderr, "task[%d] @0x%p has no fetch group\n", i, tp);
	j = 0;
	for (fp = tp->t_fetch; fp != NULL; fp = fp->f_next) {
	    if (fp->f_aux != (void *)tp)
		fprintf(stderr, "task[%d] fetch group[%d] has invalid task pointer\n",
			i, j);
	    j++;
	}

	/* check that all optreq_t's in hash list have valid r_fetch->f_aux
	 * pointing to a task in the task list.
	 */
	for (j = 0; j < tp->t_numpmid; j++) {
	    pmid = tp->t_pmidlist[j];
	    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL; hp = hp->next) {
		if (pmid != (pmID)hp->key)
		continue;
		rqp = (optreq_t *)hp->data;
		for (tp2 = tasklist; tp2 != NULL; tp2 = tp2->t_next)
		    if (rqp->r_fetch->f_aux == (void *)tp2)
			break;
		if (tp2 == NULL) {
		    fprintf(stderr, "task[%d] pmid %s optreq %p for [",
			    i, pmIDStr(pmid), rqp);
		    if (rqp->r_numinst == 0)
			fputs("`all instances' ", stderr);
		    else
			for (k = 0; k < rqp->r_numinst; k++)
			    fprintf(stderr, "%d ", rqp->r_instlist[k]);
		    fputs("] bad task pointer\n", stderr);
		}
	    }
	}
    }

    
}

/* Call this in dbx to dump the task list (dbx doesn't know about stdout) */
void
dumpit(void)
{
    int		i;
    task_t	*tp;

    reality_check();
    for (tp = tasklist, i = 0; tp != NULL; tp = tp->t_next, i++) {
	
	fprintf(stderr,
		"\ntask[%d] @%p: %s %s \ndelta = %f\n", i, tp,
		PMLC_GET_MAND(tp->t_state) ? "mandatory " : "advisory ",
		PMLC_GET_ON(tp->t_state) ? "on " : "off ",
		tp->t_delta.tv_sec + (float)tp->t_delta.tv_usec / 1.0e6);
	__pmOptFetchDump(stderr, tp->t_fetch);
    }
}

/*
 * stolen from __pmDumpResult
 */
static void
dumpcontrol(FILE *f, const pmResult *resp, int dovalue)
{
    int		i;
    int		j;

    fprintf(f,"LogControl dump from 0x%p", resp);
    fprintf(f, " numpmid: %d\n", resp->numpmid);
    for (i = 0; i < resp->numpmid; i++) {
	pmValueSet	*vsp = resp->vset[i];
	fprintf(f,"  %s :", pmIDStr(vsp->pmid));
	if (vsp->numval == 0) {
	    fprintf(f, " No values!\n");
	    continue;
	}
	else if (vsp->numval < 0) {
	    fprintf(f, " %s\n", pmErrStr(vsp->numval));
	    continue;
	}
	fprintf(f, " numval: %d", vsp->numval);
	fprintf(f, " valfmt: %d", vsp->valfmt);
	for (j = 0; j < vsp->numval; j++) {
	    pmValue	*vp = &vsp->vlist[j];
	    if (vsp->numval > 1 || vp->inst != PM_INDOM_NULL) {
		fprintf(f," inst [%d]", vp->inst);
	    }
	    else
		fprintf(f, " singular");
	    if (dovalue) {
		fprintf(f, " value ");
		pmPrintValue(f, vsp->valfmt, PM_TYPE_U32, vp, 1); 
	    }
	    fputc('\n', f);
	}
    }
}

#endif

/* Called when optFetch or _pmHash routines fail.  This is terminal. */
void
die(char *name, int sts)
{
    __pmNotifyErr(LOG_ERR, "%s error unrecoverable: %s\n", name, pmErrStr(sts));
    exit(1);
}

static void
linkback(task_t *tp)
{
    fetchctl_t	*fcp;

    /* link the new fetch groups back to their task */
    for (fcp = tp->t_fetch; fcp != NULL; fcp = fcp->f_next)
	fcp->f_aux = (void *)tp;
}

optreq_t *
findoptreq(pmID pmid, int inst)
{
    __pmHashNode	*hp;
    optreq_t	*rqp;
    optreq_t	*all_rqp = NULL;
    int		j;

    /*
     * Note:
     * The logic here assumes that for each metric-inst pair, there is
     * at most one optreq_t structure, corresponding to the logging
     * state of ON (mandatory or advisory) else OFF (mandatory).  Other
     * requests change the data structures, but do not leave optreq_t
     * structures lying about, i.e. MAYBE (mandatory) is the default,
     * and does not have to be explicitly stored, while OFF (advisory)
     * reverts to MAYBE (mandatory).
     * There is one execption to the above assumption, namely for
     * cases where the initial specification includes "all" instances,
     * then some later concurrent specification may refer to specific
     * instances ... in this case, the specific optreq_t structure is
     * the one that applies.
     */
    
    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL; hp = hp->next) {
	if (pmid != (pmID)hp->key)
	    continue;
	rqp = (optreq_t *)hp->data;
	if (rqp->r_numinst == 0) {
	    all_rqp = rqp;
	    continue;
	}
	for (j = 0; j < rqp->r_numinst; j++)
	    if (inst == rqp->r_instlist[j])
		return rqp;
    }

    if (all_rqp != NULL)
	return all_rqp;
    else
	return NULL;
}

/* Determine whether a metric is currently known.  Returns
 *	-1 if metric not known
 *	inclusive OR of the flags below if it is known
 */
#define MF_HAS_INDOM	0x1		/* has an instance domain */
#define MF_HAS_ALL	0x2		/* has an "all instances" */
#define MF_HAS_INST	0x4		/* has specific instance(s) */
#define MF_HAS_MAND	0x8		/* has at least one inst with mandatory */
					/* logging (or is mandatory if no indom) */
static int
find_metric(pmID pmid)
{
    __pmHashNode	*hp;
    optreq_t	*rqp;
    int		result = 0;
    int		found = 0;

    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL; hp = hp->next) {
	if (pmid != (pmID)hp->key)
	    continue;
	rqp = (optreq_t *)hp->data;
	if (found++ == 0)
	    if (rqp->r_desc->indom != PM_INDOM_NULL) {
		result |= MF_HAS_INDOM;
		if (rqp->r_numinst == 0)
		    result |= MF_HAS_ALL;
		else
		    result |= MF_HAS_INST;
	    }
	if (PMLC_GET_MAND(((task_t *)(rqp->r_fetch->f_aux))->t_state))
	    result |= MF_HAS_MAND;
    }
    return found ? result : -1;
}

/* Find an optreq_t suitable for adding a new instance */

/* Add a new metric (given a pmValueSet and a pmDesc) to the specified task.
 * Allocate and return a new task_t if the specified task pointer is nil.
 *
 * Note that this should only be called for metrics not currently in the
 * logging data structure.  All instances in the pmValueSet are added!
 */
static int
add_metric(pmValueSet *vsp, task_t **result)
{
    pmID	pmid = vsp->pmid;
    task_t	*tp = *result;
    optreq_t	*rqp;
    pmDesc	*dp;
    int		sts, need = 0;

    dp = (pmDesc *)malloc(sizeof(pmDesc));
    if (dp == NULL) {
	__pmNoMem("add_metric: new pmDesc malloc", sizeof(pmDesc), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    if ((sts = pmLookupDesc(pmid, dp)) < 0)
	return sts;

    /* allocate a new task if null task pointer passed in */
    if (tp == NULL) {
	tp = calloc(1, sizeof(task_t));
	if (tp == NULL) {
	    __pmNoMem("add_metric: new task calloc", sizeof(task_t), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	*result = tp;
    }

    /* add metric (and any instances specified) to task */
    need = ++tp->t_numpmid * sizeof(pmID);
    tp->t_pmidlist = (pmID *)realloc(tp->t_pmidlist, need);
    if (tp->t_pmidlist == NULL) {
	__pmNoMem("add_metric: new task pmID realloc", need, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    tp->t_pmidlist[tp->t_numpmid-1] = pmid;
    rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
    if (rqp == NULL) {
	__pmNoMem("add_metric: new task optreq calloc", need, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    rqp->r_desc = dp;

    /* Now copy instances if required.  Remember that metrics with singular
     * values actually have one instance specified to distinguish them from the
     * "all instances" case (which has no instances).  Use the pmDesc to check
     * for this.
     */
    if (dp->indom != PM_INDOM_NULL)
	need = rqp->r_numinst = vsp->numval;
    if (need) {
	int	i;

	need *= sizeof(rqp->r_instlist[0]);
	rqp->r_instlist = (int *)malloc(need);
	if (rqp->r_instlist == NULL) {
	    __pmNoMem("add_metric: new task optreq instlist malloc", need,
		     PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	for (i = 0; i < vsp->numval; i++)
	    rqp->r_instlist[i] = vsp->vlist[i].inst;
    }

    /* Add new metric to task's fetchgroup(s) and global hash table */
    if ((sts = __pmOptFetchAdd(&tp->t_fetch, rqp)) < 0)
	die("add_metric: __pmOptFetchAdd", sts);
    linkback(tp);
    if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0)
	die("add_metric: __pmHashAdd", sts);
    return 0;
}

/* Return true if a request for a new logging state (newstate) will be honoured
 * when current state is curstate.
 */
static int
update_ok(int curstate, int newstate)
{
    /* If new state is advisory and current is mandatory, reject request.
     * Any new mandatory state is accepted.  If the new state is advisory
     * and the current state is advisory, it is accepted.
     * Note that a new state of maybe (mandatory maybe) counts as mandatory
     */
    if (PMLC_GET_MAND(newstate) == 0 && PMLC_GET_MAYBE(newstate) == 0 &&
	PMLC_GET_MAND(curstate))
	return 0;
    else
	return 1;
}

/* Given a task and a pmID, find an optreq_t associated with the task suitable
 * for inserting a new instance into.
 * The one with the smallest number of instances is chosen.  We could also
 * have just used the first, but smallest-first gives a more even distribution.
 */
static optreq_t *
find_instoptreq(task_t *tp, pmID pmid)
{
    optreq_t	*result = NULL;
    optreq_t	*rqp;
    int		ni = 0;
    __pmHashNode	*hp;

    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL;
	 hp = hp->next) {
	if (pmid != (pmID)hp->key)
	    continue;
	rqp = (optreq_t *)hp->data;
	if ((task_t *)rqp->r_fetch->f_aux != tp)
	    continue;
	if (rqp->r_numinst == 0)
	    continue;			/* don't want "all instances" cases */
	if (ni == 0 || rqp->r_numinst < ni) {
	    result = rqp;
	    ni = rqp->r_numinst;
	}
    }
    return result;
}

/* Delete an optreq_t from its task, free it and remove it from the hash list.
 */
static void
del_optreq(optreq_t *rqp)
{
    int		sts;
    task_t	*tp = (task_t *)rqp->r_fetch->f_aux;

    if ((sts = __pmOptFetchDel(&tp->t_fetch, rqp)) < 0)
	die("del_optreq: __pmOptFetchDel", sts);
    if ((sts = __pmHashDel(rqp->r_desc->pmid, (void *)rqp, &pm_hash)) < 0)
	die("del_optreq: __pmHashDel", sts);
    free(rqp->r_desc);
    if (rqp->r_numinst)
	free(rqp->r_instlist);
    free(rqp);
    /* TODO: remove pmid from task if that was the last optreq_t for it */
    /* TODO: remove task if last pmid removed */
}

/* Delete every instance of a given metric from the data structure.
 * The pmid is deleted from the pmidlist of every task containing an instance.
 * Return a pointer to the first pmDesc found (the only thing salvaged from the
 * smoking ruins), or nil if no instances were found.
 */
static pmDesc *
del_insts(pmID pmid)
{
    optreq_t	*rqp;
    __pmHashNode	*hp;
    task_t	*tp;
    pmDesc	*dp = NULL;
    int		i, sts, keep;

    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL;
	 hp = hp->next) {
	if (pmid != (pmID)hp->key)
	    continue;
        rqp = (optreq_t *)hp->data;
	tp = (task_t *)rqp->r_fetch->f_aux;
	if ((sts = __pmOptFetchDel(&tp->t_fetch, rqp)) < 0)
	    die("del_insts: __pmOptFetchDel", sts);
	if ((sts = __pmHashDel(pmid, (void *)rqp, &pm_hash)) < 0)
	    die("del_insts: __pmHashDel", sts);
	/* save the first pmDesc pointer for return and subsequent re-use, but
	 * free all the others
	 */
	if (dp != NULL)
	    free(rqp->r_desc);
	else
	    dp = rqp->r_desc;
	if (rqp->r_numinst)
	    free(rqp->r_instlist);
	free(rqp);

	/* remove pmid from the task's pmid list */
	for (i = 0; i < tp->t_numpmid; i++)
	    if (tp->t_pmidlist[i] == pmid)
		break;
	keep = (tp->t_numpmid - 1 - i) * sizeof(tp->t_pmidlist[0]);
	if (keep)
	    memmove(&tp->t_pmidlist[i], &tp->t_pmidlist[i+1], keep);
	/* don't bother shrinking the pmidlist */
	tp->t_numpmid--;
	if (tp->t_numpmid == 0) {
	    /* TODO: nuke the task if that was the last pmID */
	}
    }
    return dp;
}

/* Update an exisiting metric (given a pmValueSet) adding it to the specified
 * task. Allocate and return a new task_t if the specified task pointer is nil.
 */
static int
update_metric(pmValueSet *vsp, int reqstate, int mflags, task_t **result)
{
    pmID	pmid = vsp->pmid;
    task_t	*ntp = *result;		/* pointer to new task */
    task_t	*ctp;			/* pointer to current task */
    optreq_t	*rqp;
    pmDesc	*dp;
    int		i, j, inst;
    int		sts, need = 0;
    int		addpmid = 0;

    /* allocate a new task if null task pointer passed in */
    if (ntp == NULL) {
	ntp = calloc(1, sizeof(task_t));
	if (ntp == NULL) {
	    __pmNoMem("update_metric: new task calloc", sizeof(task_t),
		     PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	*result = ntp;
    }

    if ((mflags & MF_HAS_INDOM) == 0) {
	rqp = findoptreq(pmid, 0);
	ctp = (task_t *)(rqp->r_fetch->f_aux);
	if (!update_ok(ctp->t_state, reqstate))
	    return 1;

	/* if the new state is advisory off, just remove the metric */
	if ((PMLC_GET_MAYBE(reqstate)) ||
	    (PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_ON(reqstate) == 0))
	    del_optreq(rqp);
	else {
	    /* update the optreq.  For single valued metrics there are no
	     * instances involved so the sole optreq can just be re-used.
	     */
	    if ((sts = __pmOptFetchDel(&ctp->t_fetch, rqp)) < 0)
		die("update_metric: 1 metric __pmOptFetchDel", sts);
	    if ((sts = __pmOptFetchAdd(&ntp->t_fetch, rqp)) < 0)
		die("update_metric: 1 metric __pmOptFetchAdd", sts);
	    linkback(ntp);
	    addpmid = 1;
	}
    }
    else {
	/* metric has an instance domain */
	if (vsp->numval > 0) {
	    /* tricky: since optFetch can't handle instance profiles of the
	     * form "all except these specific instances", and managing it
	     * manually is just too hard, reject requests for specific
	     * metric instances if "all instances" of the metric are already
	     * being logged.
	     * Note: advisory off "all instances" is excepted since ANY request
	     * overrides and advisory off.  E.g. "advisory off all" followed by
	     * "advisory on someinsts" turns on advisory logging for
	     * "someinsts".  mflags will be zero for "advisory off" metrics.
	     */
	    if (mflags & MF_HAS_ALL)
		return 1;		/* can't turn "all" into specific insts */

	    for (i = 0; i < vsp->numval; i++) {
		int	freedp = 0;
		pmDesc	*dp = NULL;

		inst = vsp->vlist[i].inst;
		rqp = findoptreq(pmid, inst);
		if (rqp != NULL) {
		    dp = rqp->r_desc;
		    ctp = (task_t *)(rqp->r_fetch->f_aux);
		    /* no work required if new task and current are the same */
		    if (ntp == ctp)
			continue;
		    if (!update_ok(ctp->t_state, reqstate))
			continue;

		    /* remove inst's group from current task */
		    if ((sts = __pmOptFetchDel(&ctp->t_fetch, rqp)) < 0)
			die("update_metric: instance add __pmOptFetchDel", sts);

		    /* put group back if there are any instances left */
		    if (rqp->r_numinst > 1) {
			/* remove inst from group */
			for (j = 0; j < rqp->r_numinst; j++)
			    if (inst == rqp->r_instlist[j])
				break;
			/* don't call memmove to move zero bytes */
			if (j < rqp->r_numinst - 1)
			    memmove(&rqp->r_instlist[j], &rqp->r_instlist[j+1],
				    (rqp->r_numinst - 1 - j) *
				    sizeof(rqp->r_instlist[0]));
			rqp->r_numinst--;
			/* (don't bother realloc-ing the instlist to a smaller size) */

			if ((sts = __pmOptFetchAdd(&ctp->t_fetch, rqp)) < 0)
			    die("update_metric: instance del __pmOptFetchAdd", sts);
			linkback(ctp);
			/* no need to update hash list, rqp already there */
		    }
		    /* if that was the last instance, free the group */
		    else {
			freedp = 1;
			free(rqp->r_instlist);
			free(rqp);
			if (( sts = __pmHashDel(pmid, (void *)rqp, &pm_hash)) < 0)
			    die("update_metric: instance __pmHashDel", sts);
		    }
		}

		/* advisory off (mandatory maybe) metrics don't get put into
		 * the data structure
		 */
		if (PMLC_GET_MAYBE(reqstate) ||
		    (PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_ON(reqstate) == 0)) {
		    if (freedp)
			free(dp);
		    continue;
		}
		addpmid = 1;

		/* try to find an existing optreq_t for the instance */
		rqp = find_instoptreq(ntp, pmid);
		if (rqp != NULL) {
		    if ((sts = __pmOptFetchDel(&ntp->t_fetch, rqp)) < 0)
			die("update_metric: instance add __pmOptFetchDel", sts);
		}
		/* no existing optreq_t found, allocate & populate a new one */
		else {
		    rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
		    if (rqp == NULL) {
			__pmNoMem("update_metric: optreq calloc",
				 sizeof(optreq_t), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    /* if the metric existed but the instance didn't, we don't
		     * have a valid pmDesc (dp), so find one.
		     */
		    if (dp == NULL)  {
			/* find metric and associated pmDesc */
			__pmHashNode	*hp;

			for (hp = __pmHashSearch(pmid, &pm_hash);
			     hp != NULL; hp = hp->next)
			    if (pmid == (pmID)hp->key)
				break;
			dp = ((optreq_t *)hp->data)->r_desc;
		    }
		    /* recycle pmDesc from the old group, if possible */
		    if (freedp) {
			rqp->r_desc = dp;
			freedp = 0;
		    }
		    /* otherwise allocate & copy a new pmDesc via dp */
		    else {
			need = sizeof(pmDesc);
			rqp->r_desc = (pmDesc *)malloc(need);
			if (rqp->r_desc == NULL) {
			    __pmNoMem("update_metric: new inst pmDesc malloc",
				     need, PM_FATAL_ERR);
			    /*NOTREACHED*/
			}
			memcpy(rqp->r_desc, dp, need);
		    }
		    if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0)
			die("update_metric: __pmHashAdd", sts);
		}
		    
		need = (rqp->r_numinst + 1) * sizeof(rqp->r_instlist[0]);
		rqp->r_instlist = (int *)realloc(rqp->r_instlist, need);
		if (rqp->r_instlist == NULL) {
		    __pmNoMem("update_metric: inst list resize", need,
			     PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		rqp->r_instlist[rqp->r_numinst++] = inst;
		if ((sts = __pmOptFetchAdd(&ntp->t_fetch, rqp)) < 0)
		    die("update_metric: instance add __pmOptFetchAdd", sts);
		linkback(ntp);
		if (freedp)
		    free(dp);
	    }
	}
	/* the vset has numval == 0, a request for "all instances" */
	else {
	    /* if the metric is a singular instance that has mandatory logging
	     * or has at least one instance with mandatory logging on, a
	     * request for advisory logging cannot be honoured
	     */
	    if ((mflags & MF_HAS_MAND) &&
		PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_MAYBE(reqstate) == 0)
		return 1;

	    if (mflags & MF_HAS_ALL) {
		/* if there is an "all instances" for the metric, it will be
		 * the only optreq_t for the metric
		 */
		rqp = findoptreq(pmid, 0);
		ctp = (task_t *)rqp->r_fetch->f_aux;

		/* if the metric is "advisory on, all instances"  and the
		 * request is for "mandatory maybe, all instances" the current
		 * advisory logging state of the metric is retained
		 */
		if (PMLC_GET_MAND(ctp->t_state) == 0 && PMLC_GET_MAYBE(reqstate))
		    return 0;

		/* advisory off & mandatory maybe metrics don't get put into
		 * the data structure
		 */
		if (PMLC_GET_MAYBE(reqstate) ||
		    (PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_ON(reqstate) == 0)) {
		    del_optreq(rqp);
		    return 0;
		}

		addpmid = 1;
		if ((sts = __pmOptFetchDel(&ctp->t_fetch, rqp)) < 0)
		    die("update_metric: all inst __pmOptFetchDel", sts);
		/* don't delete from hash list, rqp re-used */
		if ((sts = __pmOptFetchAdd(&ntp->t_fetch, rqp)) < 0)
		    die("update_metric: all inst __pmOptFetchAdd", sts);
		linkback(ntp);
	    }
	    else {
		/* there are one or more specific instances for the metric.
		 * The metric cannot have an "all instances" at the same time.
		 *
		 * if the request is for "mandatory maybe, all instances" and
		 * the only instances of the metric all have advisory logging
		 * on, retain the current advisory semantics.
		 */
		if (PMLC_GET_MAYBE(reqstate) &&
		    (mflags & MF_HAS_INST) && !(mflags & MF_HAS_MAND))
		    return 0;

		dp = del_insts(pmid);

		/* advisory off (mandatory maybe) metrics don't get put into
		 * the data structure
		 */
		if (PMLC_GET_MAYBE(reqstate) ||
		    (PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_ON(reqstate) == 0)) {
		    free(dp);
		    return 0;
		}

		addpmid = 1;
		rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
		if (rqp == NULL) {
		    __pmNoMem("update_metric: all inst calloc",
			     sizeof(optreq_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		rqp->r_desc = dp;
		if ((sts = __pmOptFetchAdd(&ntp->t_fetch, rqp)) < 0)
		    die("update_metric: all inst __pmOptFetchAdd", sts);
		linkback(ntp);
		if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0)
		    die("update_metric: all inst __pmHashAdd", sts);
	    }
	}
    }

    if (!addpmid)
	return 0;

    /* add pmid to new task if not already there */
    for (i = 0; i < ntp->t_numpmid; i++)
	if (pmid == ntp->t_pmidlist[i])
	    break;
    if (i >= ntp->t_numpmid) {
	int	need;

	need = (ntp->t_numpmid + 1) * sizeof(pmID);
	ntp->t_pmidlist = (pmID *)realloc(ntp->t_pmidlist, need);
	if (ntp->t_pmidlist == NULL) {
	    __pmNoMem("update_metric: grow task pmidlist", need, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	ntp->t_pmidlist[ntp->t_numpmid] = pmid;
	ntp->t_numpmid++;
    }
    return 0;
}

/* Given a state and a delta, return in result the first matching task.
 * Return true if a matching task was found
 */
static int
find_task(int state, int delta, task_t **result)
{
    task_t	*tp;
    int		tdelta;

    /* Never return a "once only" task, it may have gone off already and just
     * be hanging around
     * TODO: cleanup once only tasks after callback invoked
     */
    if (delta == 0)
	return 0;

    for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	tdelta = tp->t_delta.tv_sec * 1000 + (tp->t_delta.tv_usec / 1000);
	if (state == (tp->t_state & 0x3) && delta == tdelta)
	    break;
    }
    *result = tp;
    return tp != NULL;
}

/* Return a mask containing the history flags for a given metric/instance.
 * the history flags indicate whether the metric/instance is in the log at all
 * and whether the last fetch of the metric/instance was successful.
 *
 * The result is suitable for ORing into the result returned by a control log
 * request.
 */
static int
gethistflags(pmID pmid, int inst)
{
    extern __pmHashCtl	hist_hash;
    __pmHashNode		*hp;
    pmidhist_t		*php;
    insthist_t		*ihp;
    int			i, found;
    int			val;

    for (hp = __pmHashSearch(pmid, &hist_hash); hp != NULL; hp = hp->next)
	if ((pmID)hp->key == pmid)
	    break;
    if (hp == NULL)
	return 0;
    php = (pmidhist_t *)hp->data;
    ihp = &php->ph_instlist[0];
    val = 0;
    if (php->ph_indom != PM_INDOM_NULL) {
	for (i = 0; i < php->ph_numinst; i++, ihp++)
	    if (ihp->ih_inst == inst)
		break;
	found = i < php->ph_numinst;
    }
    else
	found = php->ph_numinst > 0;
    if (found) {
	PMLC_SET_INLOG(val, 1);
	val |= ihp->ih_flags;		/* only "available flag" is ever set */
    }
    return val;
}

/* take a pmResult (from a control log request) and half-clone it: return a
 * pointer to a new pmResult struct which shares the pmValueSets in the
 * original that have numval > 0, and has null pointers for the pmValueSets
 * in the original with numval <= 0
 */
static pmResult *
siamise_request(pmResult *request)
{
    int		i, need;
    pmValueSet	*vsp;
    pmResult	*result;

    need = sizeof(pmResult) + (request->numpmid - 1) * sizeof(pmValueSet *);
    result = (pmResult *)malloc(need);
    if (result == NULL) {
	__pmNoMem("siamise_request: malloc pmResult", need, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    for (i = 0; i < request->numpmid; i++) {
	vsp = request->vset[i];
	if (vsp->numval > 0)
	    result->vset[i] = request->vset[i];
	else
	    result->vset[i] = NULL;
    }
    result->timestamp = request->timestamp; /* structure assignment */
    result->numpmid = request->numpmid;

    return result;
}

/* Temporarily borrow a bit in the metric/instance history to indicate that
 * the instance currently exists in the instance domain.  The macros below
 * set and get the bit, which is cleared after we are finished with it here.
 */

#define PMLC_SET_USEINDOM(val, flag) (val = (val & ~0x1000) | (flag << 12 ))
#define PMLC_GET_USEINDOM(val) ((val & 0x1000) >> 12)

/* create a pmValueSet large enough to contain the union of the current
 * instance domain of the specified metric and any previous instances from
 * the history list.
 */
static pmValueSet *
build_vset(pmID pmid, int usehist)
{
    __pmHashNode		*hp;
    pmidhist_t		*php = NULL;
    insthist_t		*ihp;
    int			need = 0;
    int			i, numindom = 0;
    pmDesc		desc;
    int			have_desc;
    int			*instlist;
    char		**namelist;
    pmValueSet		*vsp;

   if (usehist) {
	/* find the number of instances of the metric in the history (1 if
	 * single-valued metric)
	 */
	for (hp = __pmHashSearch(pmid, &hist_hash); hp != NULL; hp = hp->next)
	    if ((pmID)hp->key == pmid)
		break;
	if (hp != NULL) {
	    php = (pmidhist_t *)hp->data;
	    need = php->ph_numinst;
	}
    }
    /*
     * get the current instance domain, so that if the metric hasn't been
     * logged yet a sensible result is returned.
     */
    if ((have_desc = pmLookupDesc(pmid, &desc)) < 0)
	goto no_info;
    if (desc.indom == PM_INDOM_NULL)
	need = 1;			/* will be same in history */
    else {
	int	j;
	    
	if ((numindom = pmGetInDom(desc.indom, &instlist, &namelist)) < 0) {
	    have_desc = numindom;
	    goto no_info;
	}
	/* php will be null if usehist is false or there is no history yet */
	if (php == NULL)
	    need = numindom;		/* no history => use indom */
	else
	    for (i = 0; i < numindom; i++) {
		int	inst = instlist[i];
		
		for (j = 0; j < php->ph_numinst; j++)
		    if (inst == php->ph_instlist[j].ih_inst)
			break;
		/*
		 * if instance is in history but not instance domain, leave
		 * extra space for it in vset, otherwise use the USEINDOM
		 * flag to avoid another NxM comparison when building the vset
		 * instances later.
		 */
		if (j >= php->ph_numinst)
		    need++;
		else
		    PMLC_SET_USEINDOM(php->ph_instlist[j].ih_flags, 1);
	    }
    }

no_info:

    need = sizeof(pmValueSet) + (need - 1) * sizeof(pmValue);
    vsp = (pmValueSet *)malloc(need);
    if (vsp == NULL) {
	__pmNoMem("build_vset for control/enquire", need, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    vsp->pmid = pmid;
    if (have_desc < 0) {
	vsp->numval = have_desc;
    }
    else if (desc.indom == PM_INDOM_NULL) {
	vsp->vlist[0].inst = PM_IN_NULL;
	vsp->numval = 1;
    }
    else {
	int	j;
	
	i = 0;
	/* get instances out of instance domain first */
	if (numindom > 0)
	    for (j = 0; j < numindom; j++)
		vsp->vlist[i++].inst = instlist[j];

	/* then any not in instance domain from history */
	if (php != NULL) {
	    ihp = &php->ph_instlist[0];
	    for (j = 0; j < php->ph_numinst; j++, ihp++)
		if (PMLC_GET_USEINDOM(ihp->ih_flags))
		    /* it's already in the indom */
		    PMLC_SET_USEINDOM(ihp->ih_flags, 0);
		else
		    vsp->vlist[i++].inst = ihp->ih_inst;
	}
	vsp->numval = i;
	free(instlist);
	free(namelist);
    }
    
    return vsp;
}

static void
do_control(__pmPDU *pb)
{
    int			sts;
    int			control;
    int			state;
    int			delta;
    pmResult		*request;
    pmResult		*result;
    int			siamised = 0;	/* the verb from siamese (as in twins) */
    int			i;
    int			j;
    int			val;
    pmValueSet		*vsp;
    optreq_t		*rqp;
    task_t		*tp;
    time_t		now;
    int			reqstate = 0;
    extern unsigned int	clientops;	/* access control (deny ops) */

    /*
     * TODO	- encoding for logging interval in requests and results?
     */
    if ((sts = __pmDecodeLogControl(pb, &request, &control, &state, &delta)) < 0)
	return;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "do_control: control=%d state=%d delta=%d request ...\n",
		control, state, delta);
	dumpcontrol(stderr, request, 0);
    }
#endif

    if (control == PM_LOG_MANDATORY || control == PM_LOG_ADVISORY) {
	time(&now);
	fprintf(stderr, "\n%s", ctime(&now));
	fprintf(stderr, "pmlc request from %s: %s",
	    pmlc_host, control == PM_LOG_MANDATORY ? "mandatory" : "advisory");
	if (state == PM_LOG_ON) {
	    if (delta == 0)
		fprintf(stderr, " on once\n");
	    else
		fprintf(stderr, " on %.1f sec\n", (float)delta/1000);
	}
	else if (state == PM_LOG_OFF)
	    fprintf(stderr, " off\n");
	else
	    fprintf(stderr, " maybe\n");
    }

    /*
     * access control checks
     */
    sts = 0;
    switch (control) {
	case PM_LOG_MANDATORY:
	    if (clientops & OP_LOG_MAND)
		sts = PM_ERR_PERMISSION;
	    break;

	case PM_LOG_ADVISORY:
	    if (clientops & OP_LOG_ADV)
		sts = PM_ERR_PERMISSION;
	    break;

	case PM_LOG_ENQUIRE:
	    if (clientops & OP_LOG_ENQ)
		sts = PM_ERR_PERMISSION;
	    break;

	default:
	    fprintf(stderr, "Bad control PDU type %d\n", control);
	    sts = PM_ERR_IPC;
	    break;
    }
    if (sts < 0) {
	if (control == PM_LOG_MANDATORY || control == PM_LOG_ADVISORY)
	    fprintf(stderr, "Error: %s\n", pmErrStr(sts));
	if ((sts = __pmSendError(clientfd, PDU_BINARY, sts)) < 0)
	    __pmNotifyErr(LOG_ERR,
			 "do_control: error sending Error PDU to client: %s\n",
			 pmErrStr(sts));
	pmFreeResult(request);
	return;
    }

    /* handle everything except PM_LOG_ENQUIRE */
    if (control == PM_LOG_MANDATORY || control == PM_LOG_ADVISORY) {
	/* update the logging status of metrics */

	task_t		*newtp = NULL; /* task for metrics/insts in request */
	int		newtask;
	int		mflags;

	/* convert state and control to the bitmask used in pmlogger and values
	 * returned in results.  Remember that reqstate starts with nothing on.
	 */
	PMLC_SET_ON(reqstate, (state == PM_LOG_ON) ? 1 : 0);
	if (control == PM_LOG_MANDATORY)
	    if (state == PM_LOG_MAYBE)
		/* mandatory+maybe => maybe+advisory+off  */
		PMLC_SET_MAYBE(reqstate, 1);
	    else
		PMLC_SET_MAND(reqstate, 1);

	/* try to find an existing task for the request */
	newtask = !find_task(reqstate, delta, &newtp);

	for (i = 0; i < request->numpmid; i++) {
	    vsp = request->vset[i];
	    mflags = find_metric(vsp->pmid);
	    if (mflags < 0) {
		/* only add new metrics if they are ON or MANDATORY OFF
		 * Careful: mandatory+maybe is mandatory+maybe+off
		 */
		if (PMLC_GET_ON(reqstate) ||
		    (PMLC_GET_MAND(reqstate) && !PMLC_GET_MAYBE(reqstate)))
		    add_metric(vsp, &newtp);
	    }
	    else
		/* already a specification for this metric */
		update_metric(vsp, reqstate, mflags, &newtp);
	}

	/* schedule new logging task if new metric(s) specified */
	if (newtask && newtp != NULL) {
	    if (newtp->t_fetch == NULL) {
		/* the new task ended up with no fetch groups, throw it away */
		if (newtp->t_pmidlist != NULL)
		    free(newtp->t_pmidlist);
		free(newtp);
	    }
	    else {
		/* link new task into tasklist */
		newtp->t_next = tasklist;
		tasklist = newtp;

		/* use only the MAND/ADV and ON/OFF bits of reqstate */
		newtp->t_state = reqstate & 0x3;
		if (PMLC_GET_ON(reqstate)) {
		    newtp->t_delta.tv_sec = delta / 1000;
		    newtp->t_delta.tv_usec = (delta % 1000) * 1000;
		    newtp->t_afid = __pmAFregister(&newtp->t_delta, (void *)newtp,
					       log_callback);
		}
		else
		    newtp->t_delta.tv_sec = newtp->t_delta.tv_usec = 0;
		linkback(newtp);	/* TODO: really needed? (no) */
	    }
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	dumpit();
#endif

    /* just ignore advisory+maybe---the returned pmResult will have the metrics
     * in their original state indicating that the request could not be
     * satisfied.
     */

    result = request;
    result->timestamp.tv_sec = result->timestamp.tv_usec = 0;	/* for purify */
    /* write the current state of affairs into the result _pmResult */
    for (i = 0; i < request->numpmid; i++) {

	if (control == PM_LOG_MANDATORY || control == PM_LOG_ADVISORY) {
	    char	*p;

	    sts = pmNameID(request->vset[i]->pmid, &p);
	    if (sts < 0)
		fprintf(stderr, "  metric: %s", pmIDStr(request->vset[i]->pmid));
	    else {
		fprintf(stderr, "  metric: %s", p);
		free(p);
	    }
	}

	if (request->vset[i]->numval <= 0 && !siamised) {
	    result = siamise_request(request);
	    siamised = 1;
	}
	/*
	 * pmids with numval <= 0 in the request have a null vset ptr in the
	 * in the corresponding place in the siamised result.
	 */
	if (result->vset[i] != NULL)
	    vsp = result->vset[i];
	else {
	    /* the result should also contain the history for an all instances
	     * enquire request.  Control requests just get the current indom
	     * since the user of pmlc really wants to see what's being logged
	     * now rather than in the past.
	     */
	    vsp = build_vset(request->vset[i]->pmid, control == PM_LOG_ENQUIRE);
	    result->vset[i] = vsp;
	}
	vsp->valfmt = PM_VAL_INSITU;
	for (j = 0; j < vsp->numval; j++) {
	    rqp = findoptreq(vsp->pmid, vsp->vlist[j].inst);
	    val = 0;
	    if (rqp == NULL) {
		PMLC_SET_STATE(val, 0);
		PMLC_SET_DELTA(val, 0);
	    }
	    else {
		tp = rqp->r_fetch->f_aux;
		PMLC_SET_STATE(val, tp->t_state);
		PMLC_SET_DELTA(val, tp->t_delta.tv_sec*1000 + tp->t_delta.tv_usec/1000);
	    }

	    val |= gethistflags(vsp->pmid, vsp->vlist[j].inst);
	    vsp->vlist[j].value.lval = val;

	    if (control == PM_LOG_MANDATORY || control == PM_LOG_ADVISORY) {
		int	expstate = 0;
		int	statemask = 0;
		int	expdelta;
		if (rqp != NULL && rqp->r_desc->indom != PM_INDOM_NULL) {
		    char	*p;
		    if (j == 0)
			fputc('\n', stderr);
		    if (pmNameInDom(rqp->r_desc->indom, vsp->vlist[j].inst, &p) >= 0) {
			fprintf(stderr, "    instance: %s", p);
			free(p);
		    }
		    else
			fprintf(stderr, "    instance: #%d", vsp->vlist[j].inst);
		}
		else {
		    /* no pmDesc ... punt */
		    if (vsp->numval > 1 || vsp->vlist[j].inst != PM_IN_NULL) {
			if (j == 0)
			    fputc('\n', stderr);
			fprintf(stderr, "    instance: #%d", vsp->vlist[j].inst);
		    }
		}
		if (state != PM_LOG_MAYBE) {
		    PMLC_SET_MAND(expstate, (control == PM_LOG_MANDATORY) ? 1 : 0);
		    PMLC_SET_ON(expstate, (state == PM_LOG_ON) ? 1 : 0);
		    PMLC_SET_MAND(statemask, 1);
		    PMLC_SET_ON(statemask, 1);
		}
		else {
		    PMLC_SET_MAND(expstate, 0);
		    PMLC_SET_MAND(statemask, 1);
		}
		expdelta = PMLC_GET_ON(expstate) ? delta : 0;
		if ((PMLC_GET_STATE(val) & statemask) != expstate ||
		    PMLC_GET_DELTA(val) != expdelta)
			fprintf(stderr, " [request failed]");
		fputc('\n', stderr);
	    }
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	__pmDumpResult(stderr, result);
    }
#endif

    if ((sts = __pmSendResult(clientfd, PDU_BINARY, result)) < 0)
		__pmNotifyErr(LOG_ERR,
			     "do_control: error sending Error PDU to client: %s\n",
			     pmErrStr(sts));

    if (siamised) {
	for (i = 0; i < request->numpmid; i++)
	    if (request->vset[i]->numval <= 0)
		free(result->vset[i]);
	free(result);
    }
    pmFreeResult(request);
    return;
}

/*
 * sendstatus
 * (data_x send is kept for backwards compatability with PCP 1.x)
 */
static int
sendstatus(void)
{
    int				end;
    __pmIPC			*ipc;
    static int			firsttime = 1;
    static char			*tzlogger;
    extern struct timeval	last_stamp;
    struct timeval		now;
    struct hostent		*hep = NULL;

    if (firsttime) {
#if defined(IRIX6_5)
        if (_MIPS_SYMBOL_PRESENT(__pmTimezone))
            tzlogger = __pmTimezone();
        else
            tzlogger = getenv("TZ");
#else
        tzlogger = __pmTimezone();
#endif
	firsttime = 0;
    }

    if ((end = __pmFdLookupIPC(clientfd, &ipc)) < 0)
	return end;

    if (ipc->version >= LOG_PDU_VERSION2) {
	__pmLoggerStatus		ls;

	if ((ls.ls_state = logctl.l_state) == PM_LOG_STATE_NEW)
	    ls.ls_start.tv_sec = ls.ls_start.tv_usec = 0;
	else
	    memcpy(&ls.ls_start, &logctl.l_label.ill_start, sizeof(ls.ls_start));
	memcpy(&ls.ls_last, &last_stamp, sizeof(ls.ls_last));
	gettimeofday(&now, NULL);
	ls.ls_timenow.tv_sec = (__int32_t)now.tv_sec;
	ls.ls_timenow.tv_usec = (__int32_t)now.tv_usec;
	ls.ls_vol = logctl.l_curvol;
	ls.ls_size = ftell(logctl.l_mfp);

	/* be careful of buffer size mismatches when copying strings */
	end = sizeof(ls.ls_hostname) - 1;
	strncpy(ls.ls_hostname, logctl.l_label.ill_hostname, end);
	ls.ls_hostname[end] = '\0';
	end = sizeof(ls.ls_fqdn) - 1;
	hep = gethostbyname(logctl.l_label.ill_hostname);
	if (hep != NULL)
	    /* send the fully qualified domain name */
	    strncpy(ls.ls_fqdn, hep->h_name, end);
	else
	    /* use the hostname from -h ... on command line */
	    strncpy(ls.ls_fqdn, logctl.l_label.ill_hostname, end);
	ls.ls_fqdn[end] = '\0';

	end = sizeof(ls.ls_tz) - 1;
	strncpy(ls.ls_tz, logctl.l_label.ill_tz, end);
	ls.ls_tz[end] = '\0';
	end = sizeof(ls.ls_tzlogger) - 1;
	if (tzlogger != NULL)
	    strncpy(ls.ls_tzlogger, tzlogger, end);
	else
	    end = 0;
	ls.ls_tzlogger[end] = '\0';

	return __pmSendLogStatus(clientfd, &ls);
    }

    else {
	__pmLoggerStatus_v1		ls_v1;

	if ((ls_v1.ls_state = logctl.l_state) == PM_LOG_STATE_NEW)
	    ls_v1.ls_start.tv_sec = ls_v1.ls_start.tv_usec = 0;
	else
	    memcpy(&ls_v1.ls_start, &logctl.l_label.ill_start, sizeof(ls_v1.ls_start));
	memcpy(&ls_v1.ls_last, &last_stamp, sizeof(ls_v1.ls_last));
	gettimeofday(&now, NULL);
	ls_v1.ls_timenow.tv_sec = (__int32_t)now.tv_sec;
	ls_v1.ls_timenow.tv_usec = (__int32_t)now.tv_usec;
	ls_v1.ls_vol = logctl.l_curvol;
	ls_v1.ls_size = ftell(logctl.l_mfp);

	/* be careful of buffer size mismatches when copying strings */
	end = sizeof(ls_v1.ls_hostname) - 1;
	strncpy(ls_v1.ls_hostname, logctl.l_label.ill_hostname, end);
	ls_v1.ls_hostname[end] = '\0';

	end = sizeof(ls_v1.ls_tz) - 1;
	strncpy(ls_v1.ls_tz, logctl.l_label.ill_tz, end);
	ls_v1.ls_tz[end] = '\0';
	end = sizeof(ls_v1.ls_tzlogger) - 1;
	if (tzlogger != NULL)
	    strncpy(ls_v1.ls_tzlogger, tzlogger, end);
	else
	    end = 0;
	ls_v1.ls_tzlogger[end] = '\0';

	return __pmSendDataX(clientfd, PDU_BINARY, PMLC_PDU_STATUS, sizeof(ls_v1), &ls_v1);
    }
}

/*
 * do_data_x
 * (function kept for backwards compatability with PCP 1.x)
 */
static int
do_data_x(__pmPDU *pb)
{
    int			sts;
    int			subtype, vlen;
    void		*vp;

    if ((sts = __pmDecodeDataX(pb, PDU_BINARY, &subtype, &vlen, &vp)) < 0) {
	__pmNotifyErr(LOG_ERR,
		     "do_data_x: error decoding PDU: %s\n", pmErrStr(sts));
	sts = PM_ERR_IPC;
    }
    else {
	switch (subtype) {
	    case PMLC_PDU_STATUS_REQ:
		sts = sendstatus();
		break;

	    case PMLC_PDU_NEWVOLUME:
		sts = newvolume(VOL_SW_PMLC);
		if (sts >= 0)
		    sts = logctl.l_label.ill_vol;
		__pmSendError(clientfd, PDU_BINARY, sts);
		break;

	    case PMLC_PDU_SYNC:
		sts = 0;
		if (fflush(logctl.l_tifp) != 0)
		    sts = errno;
		if (fflush(logctl.l_mdfp) != 0 && sts == 0)
		    sts = errno;
		if (fflush(logctl.l_mfp) != 0 && sts == 0)
		    sts = errno;
		__pmSendError(clientfd, PDU_BINARY, sts);
		break;

	    default:
		sts = PM_ERR_IPC;
		break;
	}
	__pmUnpinPDUBuf(pb);
    }
    return sts;
}

static int
do_request(__pmPDU *pb)
{
    int		sts;
    int		type;

    if ((sts = __pmDecodeLogRequest(pb, &type)) < 0) {
	__pmNotifyErr(LOG_ERR, "do_request: error decoding PDU: %s\n", pmErrStr(sts));
	return PM_ERR_IPC;
    }

    switch (type) {
	case LOG_REQUEST_STATUS:
	    sts = sendstatus();
	    break;

	case LOG_REQUEST_NEWVOLUME:
	    sts = newvolume(VOL_SW_PMLC);
	    if (sts >= 0)
		sts = logctl.l_label.ill_vol;
	    __pmSendError(clientfd, PDU_BINARY, sts);
	    break;

	case LOG_REQUEST_SYNC:
	    sts = 0;
	    if (fflush(logctl.l_tifp) != 0)
		sts = errno;
	    if (fflush(logctl.l_mdfp) != 0 && sts == 0)
		sts = errno;
	    if (fflush(logctl.l_mfp) != 0 && sts == 0)
		sts = errno;
	    __pmSendError(clientfd, PDU_BINARY, sts);
	    break;

	default:
	    sts = PM_ERR_IPC;
	    break;
    }
    return sts;
}

static int
do_creds(__pmPDU *pb)
{
    int		i;
    int		sts;
    int		credcount;
    int		sender;
    __pmCred	*credlist = NULL;
    __pmIPC	ipc = { UNKNOWN_VERSION, NULL };

    if ((sts = __pmDecodeCreds(pb, PDU_BINARY, &sender, &credcount, &credlist)) < 0) {
	__pmNotifyErr(LOG_ERR, "do_creds: error decoding PDU: %s\n", pmErrStr(sts));
		return PM_ERR_IPC;
    }

    for (i = 0; i < credcount; i++) {
	if (credlist[i].c_type == CVERSION) {
	    ipc.version = credlist[i].c_vala;
	    if ((sts = __pmAddIPC(clientfd, ipc)) < 0)
		return sts;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	fprintf(stderr, "do_creds: pmlc version=%d\n", ipc.version);
#endif

    return sts;
}

/*
 * Service a request from the pmlogger client.
 * Return non-zero if the client has closed the connection.
 */
int
client_req(void)
{
    int		sts;
    __pmPDU	*pb;
    __pmIPC	*ipc;
    __pmPDUHdr	*php;

more:
    if ((sts = __pmGetPDU(clientfd, PDU_BINARY, TIMEOUT_DEFAULT, &pb)) <= 0) {
	if (sts != 0)
	    fprintf(stderr, "client_req: %s\n", pmErrStr(sts));
	__pmNoMoreInput(clientfd);
	__pmResetIPC(clientfd);
	close(clientfd);
	clientfd = -1;
	return 1;
    }
    php = (__pmPDUHdr *)pb;
    if ((sts = __pmFdLookupIPC(clientfd, &ipc)) < 0) {
	fprintf(stderr, "client_req: %s\n", pmErrStr(sts));
	__pmNoMoreInput(clientfd);
	__pmResetIPC(clientfd);
	close(clientfd);
	clientfd = -1;
	return 1;
    }
    sts = 0;

    switch (php->type) {
	case PDU_CREDS:		/* version 2 PDU */
	    do_creds(pb);
	    break;
	case PDU_LOG_REQUEST:	/* version 2 PDU */
	    do_request(pb);
	    break;
	case PDU_LOG_CONTROL:	/* version 2 PDU */
	    do_control(pb);
	    break;
	case PDU_DATA_X:	/* version 1 PDU */
	    ipc->version = LOG_PDU_VERSION1;
	    sts = do_data_x(pb);
	    break;
	case PDU_CONTROL_REQ:	/* version 1 PDU */
	    if (ipc->version == UNKNOWN_VERSION)
		ipc->version = LOG_PDU_VERSION1;
	    do_control(pb);
	    break;
	default:		/*  unknown PDU  */
	    fprintf(stderr, "client_req: bad PDU type 0x%x\n", php->type);
	    sts = PM_ERR_IPC;
	    break;
    }
    
    if (sts >= 0 && __pmMoreInput(clientfd))
	goto more;

    if (sts >= 0)
	return 0;
    else {
	/* the client isn't playing by the rules; disconnect it */
	__pmSendError(clientfd, PDU_BINARY, sts);
	__pmNoMoreInput(clientfd);
	__pmResetIPC(clientfd);
	close(clientfd);
	clientfd = -1;
	return 1;
    }
}
