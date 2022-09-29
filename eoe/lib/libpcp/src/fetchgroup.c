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

#ident "$Id: fetchgroup.c,v 1.13 1999/08/18 15:49:07 kenmcd Exp $"

#include <sys/param.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"

typedef struct {
	int		lastIndex;
	double		*current;
	double		*previous;
	struct timeval	*currentTime;
	struct timeval	*previousTime;
	int		*currentFetchErr;
	int		*previousFetchErr;
} fetchMetric;

typedef struct {
	char		*hostOrArchive;
	int		contextMode;
	fetchctl_t	*root;
	int		ncontexts;
	fetchctl_t	**roots;
	int		*contexts;
	pmResult	**results;
	int		*fetchErr;
} fetchGroup;

static fetchGroup *fetchGroups = NULL;
static int nfetchGroups = 0;

static optreq_t **reqlist = NULL;
static int nreqlist = 0;

void
__pmFetchGroupStats(void)
{
    fetchGroup *group;
    int r, i, c, d;

    for (r=i=0; i < nreqlist; i++)
	if (reqlist[i] != NULL)
	    r++;

    for (c=i=0; i < nfetchGroups; i++) {
	group = &fetchGroups[i];
	for (d=0; d < group->ncontexts; d++) {
	    if (group->contexts[d] >= 0)
		c++;
	}
    }
    pmprintf("__pmFetchGroupStats: %d reqs managed by %d groups using %d contexts.\n", r, nfetchGroups, c);
    pmflush();
}

static void
fetchGroupReconfigure(fetchGroup *group)
{
    fetchctl_t *fp;
    indomctl_t *idp;
    int i, found;
    int curcontext = pmWhichContext();

    /*
     * destroy contexts for roots that have gone away
     */
    for (i=0; i < group->ncontexts; i++) {
	for (found=0, fp=group->root; fp != NULL; fp=fp->f_next) {
	    if (group->roots[i] == fp) {
		found++;
		break;
	    }
	}
	if (!found) {
	    if (group->contexts[i] >= 0)
		pmDestroyContext(group->contexts[i]);
	    group->contexts[i] = -1;
	    group->roots[i] = NULL;
	    group->results[i] = NULL;
	}
    }

    /*
     * create new contexts for new optfetch groups
     */
    for (fp=group->root; fp != NULL; fp=fp->f_next) {
	for (found=0, i=0; i < group->ncontexts; i++) {
	    if (group->roots[i] == fp) {
		found++;
		break;
	    }
	}
	if (!found) {
	    /* add a new optfetch root */
	    group->ncontexts++;
	    group->roots = (fetchctl_t **)realloc(group->roots, group->ncontexts * sizeof(fetchctl_t *));
	    group->roots[group->ncontexts-1] = fp;
	    group->contexts = (int *)realloc(group->contexts, group->ncontexts * sizeof(int));
	    group->contexts[group->ncontexts-1] = pmNewContext(group->contextMode, group->hostOrArchive);
	    if (group->contexts[group->ncontexts-1] < 0) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
		fprintf(stderr, "__pmFetchGroupAdd: failed to create new context : %s\n",
		pmErrStr(group->contexts[group->ncontexts-1]));
}
#endif
		return;
	    }
	    group->results = (pmResult **)realloc(group->results, group->ncontexts * sizeof(pmResult *));
	    group->results[group->ncontexts-1] = NULL;
	    group->fetchErr = (int *)realloc(group->fetchErr, group->ncontexts * sizeof(int));
	    group->fetchErr[group->ncontexts-1] = 0;
	}
    }

    /*
     * recreate the profile for each context
     */
    for (i=0; i < group->ncontexts; i++) {
	if (group->contexts[i] < 0)
	    continue;
	pmUseContext(group->contexts[i]);
	fp = group->roots[i];
	for (idp=fp->f_idp; idp != NULL; idp=idp->i_next) {
	    if (idp->i_indom != PM_INDOM_NULL) {
		pmDelProfile(idp->i_indom, 0, NULL); /* exclude all */
		pmAddProfile(idp->i_indom, idp->i_numinst, idp->i_instlist);
	    }
	}
    }

    if (curcontext >= 0)
	pmUseContext(curcontext);
}


int
__pmFetchGroupAdd(int contextMode, char *hostOrArchive, pmDesc *desc, int instance,
    double *current, double *previous,
    struct timeval *currentTime, struct timeval *previousTime,
    int *currentFetchErr, int *previousFetchErr)
{
    int newGroup = 1;
    fetchGroup *group;
    int groupIndex = -1;
    optreq_t *req;
    fetchMetric *metric;
    int i;
    int curcontext = pmWhichContext();

    for (i=0; i < nfetchGroups; i++) {
	group = &fetchGroups[i];
	if (group->contextMode == contextMode && strcmp(group->hostOrArchive, hostOrArchive) == 0) {
	    newGroup = 0;
	    groupIndex = i;
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
	    fprintf(stderr, "__pmFetchGroupAdd: mode=%d using existing group=%d for host %s\n",
	    contextMode, groupIndex, hostOrArchive);
}
#endif
	    break;
	}
    }

    if (newGroup) {
	groupIndex = nfetchGroups++;
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
	fprintf(stderr, "__pmFetchGroupAdd: mode=%d new group=%d for host %s\n",
	contextMode, groupIndex, hostOrArchive);
}
#endif
	fetchGroups = (fetchGroup *)realloc(fetchGroups, nfetchGroups * sizeof(fetchGroup));
	group = &fetchGroups[groupIndex];
	memset(group, 0, sizeof(fetchGroup));
	group->hostOrArchive = strdup(hostOrArchive);
	group->contextMode = contextMode;
	group->contexts = (int *)malloc(sizeof(int));
	group->contexts[0] = pmNewContext(contextMode, hostOrArchive);
	if (group->contexts[0] < 0) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
	    fprintf(stderr, "__pmFetchGroupAdd: failed to create new context : %s\n", pmErrStr(group->contexts[0]));
}
#endif
	    return group->contexts[0];
	}
	group->ncontexts = 1;
	group->results = (pmResult **)malloc(group->ncontexts * sizeof(pmResult *));
	group->fetchErr = (int *)malloc(group->ncontexts * sizeof(int));
	group->roots = (fetchctl_t **)malloc(group->ncontexts * sizeof(fetchctl_t *));
	memset(group->results, 0, group->ncontexts * sizeof(pmResult *));
	memset(group->fetchErr, 0, group->ncontexts * sizeof(int));
	memset(group->roots, 0, group->ncontexts * sizeof(fetchctl_t *));
    }

    req = (optreq_t *)malloc(sizeof(optreq_t));
    memset(req, 0, sizeof(optreq_t));
    req->r_desc = (pmDesc *)malloc(sizeof(pmDesc));
    memcpy(req->r_desc, desc, sizeof(pmDesc));
    if (req->r_desc->indom == PM_INDOM_NULL) {
	req->r_numinst = 0;
	req->r_instlist = NULL;
    }
    else {
	req->r_numinst = 1;
	req->r_instlist = (int *)malloc(2 * sizeof(int));
	req->r_instlist[0] = instance;
	req->r_instlist[1] = PM_IN_NULL;
    }

    metric = (fetchMetric *)malloc(sizeof(fetchMetric));
    if (metric == NULL) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
	perror("__pmFetchGroupAdd: malloc failed!!");
}
#endif
	return -errno;
    }
    req->r_aux = (void *)metric;
    metric->current = current;
    metric->previous = previous;
    metric->currentTime = currentTime;
    metric->previousTime = previousTime;
    metric->currentFetchErr = currentFetchErr;
    metric->previousFetchErr = previousFetchErr;
    metric->lastIndex = 0;

    /*
     * add the req to this group, then reconfigure 
     */
    __pmOptFetchAdd(&group->root, req);
    fetchGroupReconfigure(group);

    /* append the request list */
    nreqlist++;
    reqlist = (optreq_t **)realloc(reqlist, nreqlist * sizeof(optreq_t *));
    reqlist[nreqlist-1] = req;

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
    fprintf(stderr, "__pmFetchGroupAdd: ");
    __pmFetchGroupStats();
}
#endif

    if (curcontext >= 0)
	pmUseContext(curcontext);
    return nreqlist-1;
}

int
__pmFetchGroupDel(int reqNumber)
{
    optreq_t *req;
    fetchGroup *group;
    fetchMetric *metricPointers;
    int fg, cn;

    if (reqNumber >= 0 && reqNumber < nreqlist) {
	if ((req = reqlist[reqNumber]) != NULL) {
	    /* set the req pointer to NULL, but DO NOT REORDER THE LIST */
	    reqlist[reqNumber] = NULL;
	    metricPointers = (fetchMetric *)req->r_aux;
	    free(metricPointers);
	    req->r_aux = NULL;
	    /*
	     * find the fetch group root containing req
	     * and reconfigure;
	     */
	    for (fg=0; fg < nfetchGroups; fg++) {
		group = &fetchGroups[fg];
		for (cn=0; cn < group->ncontexts; cn++) {
		    if (group->contexts[cn] >= 0) {
			if (group->roots[cn] == req->r_fetch) {
			    /* NOTE: apparently __pmOptFetchDel will free(req) */
			    __pmOptFetchDel(&group->root, req);
			    fetchGroupReconfigure(group);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
			    fprintf(stderr, "__pmFetchGroupDel: ");
			    __pmFetchGroupStats();
}
#endif
			    /* success */
			    return 1;
			}
		    }
		}
	    }
	}
    }

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
    fprintf(stderr, "__pmFetchGroupDel: failed to find req#=%d\n", reqNumber);
}
#endif

    /* fail */
    return 0;
}


void
__pmFetchGroupArchiveMode(int mode, const struct timeval *when, int interval)
{
    int fg;
    int c;
    fetchGroup *group;
    int curcontext = pmWhichContext();

    for (fg=0; fg < nfetchGroups; fg++) {
	group = &fetchGroups[fg];
	for (c=0; c < group->ncontexts; c++) {
	    if (group->contexts[c] >= 0) {
		pmUseContext(group->contexts[c]);
		pmSetMode(mode, when, interval);
	    }
	}
    }

    if (curcontext >= 0)
	pmUseContext(curcontext);
}

static double
tv2d(struct timeval *tv)
{
    return((double)tv->tv_sec + (double)tv->tv_usec / 1000000.0);
}

static struct timeval *
d2tv(double tm, struct timeval *tv)
{
    tv->tv_sec = (int)tm;
    tv->tv_usec = (int)(1000000 * (tm - (int)tm));
    return tv;
}

int
__pmFetchGroupArchiveBounds(struct timeval *start, struct timeval *finish)
{
    fetchGroup	*group;
    int		i, j;
    double	s, f, ss, ff;
    struct timeval tf;
    pmLogLabel	lab;
    int curcontext = pmWhichContext();

    s = f = 0.0;

    for (i=0; i < nfetchGroups; i++) {
	group = &fetchGroups[i];
	for (j=0; j < group->ncontexts; j++) {
	    if (group->contexts[j] >= 0) {
		pmUseContext(group->contexts[j]);
		if (pmGetArchiveLabel(&lab) >= 0) {
		    if (pmGetArchiveEnd(&tf) < 0) {
			/* assume realtime */
			gettimeofday(&tf, NULL);
		    }
		    ss = tv2d(&lab.ll_start);
		    ff = tv2d(&tf);
		    if (s == f && s == 0.0) {
			s = ss;
			f = ff;
		    }
		    else {
			if (ss < s)
			    s = ss;
			if (ff > f)
			    f = ff;
		    }
		}
	    }
	}
    }

    if (curcontext >= 0)
	pmUseContext(curcontext);

    if (s == f && s == 0.0)
	/* fail */
	return -1;

    /* success */
    d2tv(s, start);
    d2tv(f, finish);
    return 0;
}

void
__pmFetchGroupPointers(int reqnum, double *current, double *previous,
    struct timeval *currentTime, struct timeval *previousTime,
    int *currentFetchErr, int *previousFetchErr)
{
    optreq_t *req = reqlist[reqnum];
    fetchMetric *metric = (fetchMetric *)req->r_aux;

    if (reqnum < 0 || reqnum >= nreqlist) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
	fprintf(stderr, "__pmFetchGroupPointers: BAD reqnum=%d\n", reqnum);
}
#endif
	return;
    }

    if (metric == NULL) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
	fprintf(stderr, "__pmFetchGroupPointers: BAD reqnum=%d r_aux=0x%p\n", reqnum, metric);
}
#endif
	return;
    }

    metric->current = current;
    metric->previous = previous;
    metric->currentTime = currentTime;
    metric->previousTime = previousTime;
    metric->currentFetchErr = currentFetchErr;
    metric->previousFetchErr = previousFetchErr;
}

int
__pmFetchGroupFetch(void)
{
    int r, fg, cn;
    pmAtomValue atom;
    pmValue *value;
    int pmid, inst;
    int found, x, y;
    pmResult *result;
    fetchctl_t *fp;
    indomctl_t *ip;
    pmidctl_t *pp;
    optreq_t *rp;
    fetchGroup *group;
    fetchMetric *metric;
    int e;
    int curcontext = pmWhichContext();

    /*
     * fetch each group
     */
    for (fg=0; fg < nfetchGroups; fg++) {
	group = &fetchGroups[fg];

	/*
	 * for each optFetch root (context)
	 */
	for (cn=0; cn < group->ncontexts; cn++) {
	    if (group->contexts[cn] < 0) {
		group->fetchErr[cn] = group->contexts[cn];
		continue;
	    }

	    pmUseContext(group->contexts[cn]);

	    if (group->results[cn] != NULL)
		pmFreeResult(group->results[cn]);
	    group->results[cn] = NULL;

	    fp = group->roots[cn];
	    if (fp->f_numpmid > 0 && group->fetchErr[cn] != PM_ERR_IPC) {
		group->fetchErr[cn] = pmFetch(fp->f_numpmid, fp->f_pmidlist, &group->results[cn]);
#ifdef PCP_DEBUG
if (group->fetchErr[cn] < 0 && (pmDebug & DBG_TRACE_OPTFETCH)) {
		    fprintf(stderr, "pmFetchGroupFetch(host = %s) FAILED: %s\n",
		    group->hostOrArchive, pmErrStr(group->fetchErr[cn]));
}
#endif
		if (group->fetchErr[cn] == PM_ERR_TIMEOUT)
		    /* treat PM_ERR_TIMEOUT like PM_ERR_IPC */
		    group->fetchErr[cn] = PM_ERR_IPC;
	    }

	    /*
	     * if any fetch in the group fails due to IPC or TIMEOUT error
	     * then attempt to reconnect
	     */
	    if (group->fetchErr[cn] == PM_ERR_IPC) {
		if ((e = pmReconnectContext(group->contexts[cn])) >= 0)
		    /* back on the air */
		    group->fetchErr[cn] = 0;
		else {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
		    fprintf(stderr, "pmReconnectContext(host = %s) FAILED: %s\n",
		    group->hostOrArchive, pmErrStr(e));
}
#endif
		    /* give up on the rest of this group for this fetch */
		    for (r=cn; r < group->ncontexts; r++) {
			group->fetchErr[r] = PM_ERR_IPC;
		    }
		    break;
		}
	    }
	}
    }

    if (curcontext >= 0)
	pmUseContext(curcontext);

    /*
     * We are done fetching; now search and extract results
     */
    for (fg=0; fg < nfetchGroups; fg++) {
	group = &fetchGroups[fg];
	for (cn=0; cn < group->ncontexts; cn++) {
	    if (group->contexts[cn] < 0)
		continue;
	    fp = group->roots[cn];
	    for (ip=fp->f_idp; ip != NULL; ip=ip->i_next) {
		for (pp=ip->i_pmp; pp != NULL; pp=pp->p_next) {
		    for (rp=pp->p_rqp; rp != NULL; rp=rp->r_next) {
			/*
			 * search for the result value
			 */
			if (rp->r_aux == NULL) {
			    /* the request may have been deleted during an update */
			    rp = NULL;
			    break;
			}
			metric = (fetchMetric *)rp->r_aux;
			*metric->previous = *metric->current;
			*metric->current = 0.0;
			*metric->previousTime = *metric->currentTime; /* struct cpy */
			*metric->previousFetchErr = *metric->currentFetchErr;

			pmid = rp->r_desc->pmid;
			if (rp->r_desc->indom != PM_INDOM_NULL)
			    inst = rp->r_instlist[0];
			found = 0;

			for (r=0; r < group->ncontexts && !found; r++) {
			    result = group->results[r];
			    /*
			     * error values are tri-state
			     * < 0 means an error occured
			     * = 0 means no values available
			     * > 0 means success.
			     */
			    if ((*metric->currentFetchErr = group->fetchErr[r]) == 0) {
				*metric->currentFetchErr = 1;
			    }
			    if (group->fetchErr[r] == PM_ERR_EOL) {
				break;
			    }
			    if (result != NULL && group->fetchErr[r] >= 0) {
				for (x=0; x < result->numpmid && !found; x++) {
				    if (result->vset[x]->pmid == pmid) {
					if (result->vset[x]->numval <= 0) {
					    *metric->currentFetchErr = result->vset[x]->numval;
					    found++;
					    break;
					}

					if (metric->lastIndex >= 0 && metric->lastIndex < result->vset[x]->numval &&
					   (rp->r_desc->indom == PM_INDOM_NULL || result->vset[x]->vlist[metric->lastIndex].inst == inst)) {
					    value = &result->vset[x]->vlist[metric->lastIndex];
					    found++;
					}
					else {
					    /* The search for Spock ... */
					    value = &result->vset[x]->vlist[0];
					    for (y=0; y < result->vset[x]->numval; y++, value++) {
						if (rp->r_desc->indom == PM_INDOM_NULL || value->inst == inst) {
						    /* my god Henry! */
						    metric->lastIndex = y;
						    found++;
						    break;
						}
					    }
					}
					
					if (found) {
					    pmExtractValue(result->vset[x]->valfmt, value, rp->r_desc->type, &atom, PM_TYPE_DOUBLE);
					    *metric->current = atom.d;
					    *metric->currentTime = result->timestamp; /* struct cpy */
					}
				    }
				}
			    }
			}
			if (!found) {
			    if (*metric->currentFetchErr > 0)
				*metric->currentFetchErr = 0; /* no values available */
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_OPTFETCH) {
			    fprintf(stderr, "__pmFetchGroupFetch: failed to find result for req\n"); 
}
#endif
			}
		    }
		}
	    }
	}
    }

    return 0;
}

