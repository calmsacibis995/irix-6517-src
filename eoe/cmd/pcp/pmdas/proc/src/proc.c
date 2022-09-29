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

#ident "$Id: proc.c,v 2.31 1999/08/17 04:13:41 kenmcd Exp $"

#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/procfs.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <pwd.h>
#include <grp.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "proc.h"
#include "proc_aux.h"
#include "ctltab.h"

/* local stuff */
#include "./cluster.h"

#define MAX_CLUSTER	6	/* highest pmid cluster id for this pmd */

extern int		errno;

static __pmProfile	*profp;	/* last received profile */

/* handle on /proc */
static DIR *procdir;

/* the pid list */
static pid_t *proc_list = NULL;
static int n_proc_list = 0;
static int max_proc_list = 0;

/* the list of interesting processes from the profile */
static int *profile_list = NULL;
static int n_profile_list = 0;
static int max_profile_list = 0;

static int proc_domain = 3; /* set by proc_init */

/* format of an entry in /proc */
char proc_fmt[8]; /* export for procfs fname conversions */
int proc_entry_len;

/*
 * Set proc_fmt and proc_entry_len
 */
static int
set_proc_fmt(void)
{
    struct dirent *directp;	/* go thru /proc directory */

    for (rewinddir(procdir); directp=readdir(procdir);) {
	if (!isdigit(directp->d_name[0]))
	    continue;
	proc_entry_len = (int)strlen(directp->d_name);
	(void)sprintf(proc_fmt, "%%0%dd", proc_entry_len);
        break;
    }

    /* make sure entry length hasn't exceeded our max size */
    /* possible internal error */
    if (proc_entry_len + 1 > PROC_FNAME_SIZE) {
	__pmNotifyErr(LOG_ERR, "proc: PROC_FNAME_SIZE(%d) is smaller than "
		"proc_entry_len(%d) + 1\n", PROC_FNAME_SIZE, proc_entry_len);
	return PM_ERR_GENERIC;
    }

    return 0;
}

static void
refresh_proc_list(void)
{
    pid_t pid;
    struct dirent *directp;

    for (n_proc_list=0, rewinddir(procdir); directp=readdir(procdir);) {
	if (!isdigit(directp->d_name[0]))
	    continue;
	if (++n_proc_list >= max_proc_list) {
	    max_proc_list += 16;
	    proc_list = (pid_t *)realloc(proc_list, max_proc_list * sizeof(pid_t));
#ifdef MALLOC_AUDIT
	    _persistent_(proc_list);
#endif
	}
	(void)sscanf(directp->d_name, proc_fmt, &pid);
	proc_list[n_proc_list-1] = pid;
    }
}

/*ARGSUSED*/
static int
proc_profile(__pmProfile *prof, pmdaExt *pmda)
{
    int i;

    /* save the profile locally */
    profp = prof;	

    /*
     * Search for the PROC instance domain in the profile.
     * This instance domain is ``not enumerable'', hence
     * we can use the explicit list of instances directly
     * rather than having to search the profile w.r.t. the proc list.
     */
    profile_list = NULL;
    n_profile_list = 0;
    for (i=0; i < profp->profile_len; i++) {
	if (profp->profile[i].indom != proc_indom)
	    continue;
	if (profp->profile[i].state != PM_PROFILE_EXCLUDE ||
	    profp->profile[i].instances_len == 0) {
	    /*EMPTY*/
	    /* there is no explicit list => only global metrics are available */
	}
	else {
	    /* use these pointers directly: mustn't free them! */
	    profile_list = profp->profile[i].instances;
	    n_profile_list = profp->profile[i].instances_len;
        }
        break;
    }/*for*/
    return 0;
}

/*ARGSUSED*/
static int
proc_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    __pmInResult		*res;
    int			j;
    int			sts;

    if (indom != proc_indom)
	return PM_ERR_INDOM;

    if ((res = (__pmInResult *)malloc(sizeof(*res))) == NULL)
	return -errno;
    res->indom = indom;
    res->instlist = NULL;
    res->namelist = NULL;
    sts = 0;

    if (name == NULL && inst == PM_IN_NULL) {
	/* get everything ... how many? */
	refresh_proc_list();
	res->numinst = n_proc_list;
    }
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -errno;
	}
    }

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -errno;
	}
	for (j = 0; j < res->numinst; j++)
	    res->namelist[j] = NULL;
    }

    if (name == NULL && inst == PM_IN_NULL) {
	/* find all instance ids and names for the PROC indom */
	for (j=0; j < n_proc_list; j++) {
	    res->instlist[j] = proc_list[j];
	    if ((sts = proc_id_to_mapname(proc_list[j], &res->namelist[j])) < 0)
		   break; 
	}
    }
    else if (name == NULL) {
        sts = proc_id_to_name(inst, &res->namelist[0]);
    }
    else {
	/* find the instance for the given indom/name */
	sts = proc_name_to_id(name, &res->instlist[0]);
    }

    if (sts == 0)
	*result = res;
    else
	__pmFreeInResult(res);

    return sts;
}

/*ARGSUSED*/
static int
proc_desc(pmID pmid, pmDesc *desc, pmdaExt *pmda)
{
    int pmidErr;
    __pmID_int	*pmidp = (__pmID_int *)&pmid;

    if (pmidp->domain != proc_domain || pmidp->cluster > MAX_CLUSTER)
	pmidErr = PM_ERR_PMID;
    else
	pmidErr = ctltab[pmidp->cluster].getdesc(pmid, desc);

    return pmidErr;
}

/*ARGSUSED*/
static int
proc_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int			i;		/* over pmidlist[] */
    int			j;
    int			n;
    int			numval;
    int			sts;
    size_t		need;
    static pmResult	*res = NULL;
    static int		maxnpmids = 0;
    pmValueSet		*vset;
    pmDesc		dp;
    __pmID_int		*pmidp;
    pmAtomValue		atom;
    int			aggregate_len;
    static int 		*fetched[MAX_CLUSTER+1] = {NULL};


    /* allocate for result structure */
    if (numpmid > maxnpmids) {
	if (res != NULL) {
	    /*
	     * don't worry about res->vset[] and pmValues ... handled
	     * using __pmFreeResultValues() in pmcd's DoFetch() or in
	     * error recovery below here.
	     */
	    free(res);
	}
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = sizeof(pmResult) + (numpmid - 1) * sizeof(pmValueSet *);
	if ((res = (pmResult *) malloc(need)) == NULL)
	    return -errno;
	maxnpmids = numpmid;
    }
    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;


    /* fix up allocations for profile size */
    for (i=0; i <= MAX_CLUSTER; i++) {
        if (n_profile_list > max_profile_list) {
	    int *f = (int*)realloc(fetched[i], n_profile_list * sizeof(int));
	    if (f == NULL) {
		max_profile_list = 0;
    		for (j=0; j <= MAX_CLUSTER; i++) {
		    if (fetched[j])
			free(fetched[j]);
		    fetched[j] = NULL;
		}
		return -errno;
            }
	    fetched[i] = f;
        }
        (void)memset(fetched[i], 0, n_profile_list * sizeof(int));
	if ((sts = ctltab[i].allocbuf(n_profile_list)) < 0)
	    return sts;
    }/*for*/
    if (n_profile_list > max_profile_list) {
        max_profile_list = n_profile_list;
    }



    sts = 0;
    for (i = 0; i < numpmid; i++) {
	int	pmidErr = 0;

	pmidp = (__pmID_int *)&pmidlist[i];
	pmidErr = proc_desc(pmidlist[i], &dp, NULL);

        /* create a vset with the error code in it */
	if (pmidErr < 0) {
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) - sizeof(pmValue));
	    if (vset == NULL) {
		if (i) {
		    res->numpmid = i;
		    __pmFreeResultValues(res);
		}
		return -errno;
	    }
	    vset->pmid = pmidlist[i];
	    vset->numval = pmidErr;
	    continue;
	}

	if (pmidp->cluster == 0) {
	    /* global metrics, singular instance domain */
	    res->vset[i] = vset = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	    if (vset == NULL) {
		if (i) {
		    res->numpmid = i;
		    __pmFreeResultValues(res);
		}
		return -errno;
	    }
	    vset->pmid = pmidlist[i];
	    vset->numval = 1;

	    switch (pmidp->item) {
	    case 0:		/* proc.nprocs */
		refresh_proc_list();
		atom.l = n_proc_list;
		break;
	    }

	    sts = __pmStuffValue(&atom, 0, &vset->vlist[0], dp.type);
	    vset->valfmt = sts;
	    vset->vlist[0].inst = PM_IN_NULL;
	}
	else {
	    /*
	     * All remaining metrics are in the PROC instance domain
	     */
	    if (!ctltab[pmidp->cluster].supported) {
		numval = PM_ERR_APPVERSION;
	    }
	    else {
		for (numval=j=0; j < n_profile_list; j++) {
		    if (fetched[pmidp->cluster][j] == 0) {
			if (ctltab[pmidp->cluster].getinfo(profile_list[j], j) == 0)
			    fetched[pmidp->cluster][j] = 1;
		    }
		    if (fetched[pmidp->cluster][j] == 1)
			numval++;
		}
	    }

	    if (numval == 1)
		vset = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	    else
		vset = (pmValueSet *)malloc(sizeof(pmValueSet) + (numval-1)*sizeof(pmValue));
	    if (vset == NULL) {
		if (i > 0) { /* not the first malloc failing */
		    res->numpmid = i;
		    __pmFreeResultValues(res);
		}
		return -errno;
	    }
	    res->vset[i] = vset;
	    vset->pmid = pmidlist[i];
	    vset->numval = numval;
	    if (numval > 0)
		vset->valfmt = PM_VAL_INSITU;

	    if (ctltab[pmidp->cluster].supported) {
		if (numval == 0) {
		    if (n_profile_list == 0)
			vset->numval = PM_ERR_PROFILE;
		}
		else {
		    for (n=j=0; j < n_profile_list; j++) {

			if (fetched[pmidp->cluster][j] == 0)
			    continue;

			aggregate_len = ctltab[pmidp->cluster].setatom(pmidp->item, &atom, j);
			sts = __pmStuffValue(&atom, aggregate_len, &vset->vlist[n], dp.type);
			vset->valfmt = sts;
			vset->vlist[n].inst = profile_list[j];
			n++;
		    }
		}/*if*/
	    }/*if*/
	}/*if*/
    }/*for*/

    *resp = res;
    return 0;
}/*proc_fetch*/

void
proc_init(pmdaInterface *dp)
{
    int sts;

    pmdaDSO(dp, PMDA_INTERFACE_2, "proc", "/usr/pcp/pmdas/proc/help");

    dp->version.two.profile = proc_profile;
    dp->version.two.fetch = proc_fetch;
    dp->version.two.desc = proc_desc;
    dp->version.two.instance = proc_instance;

    proc_domain = dp->domain;
    init_tables(dp->domain);

    if ((procdir = opendir(PROCFS_INFO)) == NULL) {
	dp->status = -errno;
	perror(PROCFS);
	return;
    }

    if ((sts = set_proc_fmt()) < 0) {
	dp->status = sts;
	return;
    }

    pmdaInit(dp, NULL, 0, NULL, 0);

    return;
}
