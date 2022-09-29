/*
 * Copyright (c) 1994 Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND
 * Use, duplication or disclosure by the Government is subject to
 * restrictions as set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software clause
 * at DFARS 252.227-7013 and/or similar or successor clauses in the FAR,
 * or the DOD or NASA FAR Supplement.  Contractor/manufacturer is Silicon
 * Graphics, Inc., 2011 N. Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * THIS SOFTWARE CONTAINS CONFIDENTIAL AND PROPRIETARY INFORMATION OF
 * SILICON GRAPHICS, INC.  ANY DUPLICATION, MODIFICATION, DISTRIBUTION, OR
 * DISCLOSURE IS STRICTLY PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN
 * PERMISSION OF SILICON GRAPHICS, INC.
 */

#ident "$Id: irix.c,v 1.69 1999/10/14 07:21:40 tes Exp $"

/*
 * The Irix Performance Metrics Domain Agent
 */

#include <fcntl.h>
#include <sys/param.h>
#include <errno.h>
#include <syslog.h>
#include <sys/systeminfo.h>
#include <sys/sysmp.h>
#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "./cluster.h"

/* Include kmemread until all readers are removed */
#include "./kmemread.h"

#if defined(IRIX6_5)
#include <sys/sysget.h>
#endif

#include "./xpc.h"
#include "./xbow.h"

extern int	errno;

static int	maxcluster;	/* initialized in irix_init() */
static int	*clmap;		/* ditto */
static int	helpHndl = -1;	/* handle to mmap help text */
static int	helpInit = 0;	/* = 1 if attempted to open help */

#define IRIX 1			/* Irix PMD Domain Id */
#define HELP_TEXT_PATH "/var/pcp/pmdas/irix/help"

/*
 * pool of pmValues to hold multiple instances
 * This is reused for each call (possibly made bigger but not free'ed)
 */
static pmVPCB		valpool;

static struct {
    int		cluster;
    void	(*init)(int reset);
    int		(*desc)(pmID pmid, pmDesc *desc);
    void	(*fetch_setup)(void);
    int		(*fetch)(pmID pmid, __pmProfile *profp, pmVPCB *vpcb);
    int		done;
} cltab[] = {
    { 0, pmda_init, pmda_desc, pmda_fetch_setup, pmda_fetch },
    { 10, sysinfo_init, sysinfo_desc, sysinfo_fetch_setup, sysinfo_fetch },
    { 11, minfo_init, minfo_desc, minfo_fetch_setup, minfo_fetch },
    { 12, nfs_client_init, nfs_client_desc, nfs_client_fetch_setup, nfs_client_fetch },
    { 13, nfs_server_init, nfs_server_desc, nfs_server_fetch_setup, nfs_server_fetch },
    { 14, rpc_client_init, rpc_client_desc, rpc_client_fetch_setup, rpc_client_fetch },
    { 15, rpc_server_init, rpc_server_desc, rpc_server_fetch_setup, rpc_server_fetch },
    { 16, swap_init, swap_desc, swap_fetch_setup, swap_fetch },
    { 17, kna_init, kna_desc, kna_fetch_setup, kna_fetch },
    { 18, sysmp_init, sysmp_desc, sysmp_fetch_setup, sysmp_fetch },
    { 19, var_init, var_desc, var_fetch_setup, var_fetch },
    { 20, syserr_init, syserr_desc, syserr_fetch_setup, syserr_fetch },
    { 21, ncstats_init, ncstats_desc, ncstats_fetch_setup, ncstats_fetch },
    { 22, getblkstats_init, getblkstats_desc, getblkstats_fetch_setup, getblkstats_fetch },
    { 23, vnodestats_init, vnodestats_desc, vnodestats_fetch_setup, vnodestats_fetch },
    { 24, igetstats_init, igetstats_desc, igetstats_fetch_setup, igetstats_fetch },
    { 25, if_init, if_desc, if_fetch_setup, if_fetch },
    { 26, hinv_init, hinv_desc, hinv_fetch_setup, hinv_fetch },
    { 27, filesys_init, filesys_desc, filesys_fetch_setup, filesys_fetch },
    { 28, percpu_init, percpu_desc, percpu_fetch_setup, percpu_fetch },
    { 29, shm_init, shm_desc, shm_fetch_setup, shm_fetch },
    { 30, msg_init, msg_desc, msg_fetch_setup, msg_fetch },
    { 31, irixpmda_sem_init, irixpmda_sem_desc, irixpmda_sem_fetch_setup, irixpmda_sem_fetch },
    { 32, xfs_init, xfs_desc, xfs_fetch_setup, xfs_fetch },
    { 33, nfs3_client_init, nfs3_client_desc, nfs3_client_fetch_setup, nfs3_client_fetch },
    { 34, engr_init, engr_desc, engr_fetch_setup, engr_fetch },
    { 35, evctr_init, evctr_desc, evctr_fetch_setup, evctr_fetch },
    { 36, irixpmda_aio_init, irixpmda_aio_desc, irixpmda_aio_fetch_setup, irixpmda_aio_fetch },
    { 37, nfs3_server_init, nfs3_server_desc, nfs3_server_fetch_setup, nfs3_server_fetch },
/*
 * Warning: the order below is important ... numa_init() must be called
 *	    before node_init()
 */
    { 38, hwrouter_init, hwrouter_desc, hwrouter_fetch_setup, hwrouter_fetch },
    { 39, numa_init, numa_desc, numa_fetch_setup, numa_fetch },
    { 40, node_init, node_desc, node_fetch_setup, node_fetch },
    { 41, irixpmda_lpage_init, irixpmda_lpage_desc, irixpmda_lpage_fetch_setup, irixpmda_lpage_fetch },
    { 42, xbow_init, xbow_desc, xbow_fetch_setup, xbow_fetch },
    { 43, socket_init, socket_desc, socket_fetch_setup, socket_fetch },
    { 44, hub_init, hub_desc, hub_fetch_setup, hub_fetch },
    { 45, xlv_init, xlv_desc, xlv_fetch_setup, xlv_fetch },
    { 46, stream_init, stream_desc, stream_fetch_setup, stream_fetch },
/*
 * Warning: the order below is important ... disk_init() must be called first
 */
    { 80, disk_init, disk_desc, disk_fetch_setup, disk_fetch },
    { 81, cntrl_init, cntrl_desc, cntrl_fetch_setup, cntrl_fetch },
    { 82, alldk_init, alldk_desc, alldk_fetch_setup, alldk_fetch },
};

static int		nclust = sizeof(cltab)/sizeof(cltab[0]);
static __pmProfile	*cur_prof = (__pmProfile *)0;
int			_pm_pagesz;
int			_pm_has_nodes = 1;
int			_pm_numcells = 0;

extern int irix_instance(pmInDom, int, char *, __pmInResult **);
extern int init_indom(int, int);

static int
irix_profile(__pmProfile *prof)
{
    /*
     * With non-DSO PMDs, this will receive the profile pdu from pmcd
     * but for the Irix pmd, we can just receive a pointer to the profile
     * and save it away in a local/static place.
     */
    cur_prof = prof;
    return 0;
}

static int
irix_fetch(int numpmid, pmID pmidlist[], pmResult **resp)
{
    register int	i;
    register int	j;
    register pmValueSet	*vsp;
    int			sts;
    static pmResult	*result = (pmResult *)0;
    static int          maxnpmids = 0;
    int			need;
    __pmID_int		*pmidp;

    for (j = 0; j < nclust; j++)
	cltab[j].done = 0;

    if (numpmid > maxnpmids) {
	if (result != (pmResult *)0) {
	    free(result);
	}
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = (int)sizeof(pmResult) + (numpmid - 1) * (int)sizeof(pmValueSet *);
	if ((result = (pmResult *)malloc(need)) == (pmResult *)0)
	    return -errno;
	maxnpmids = numpmid;
    }

    for (i = 0; i < numpmid; i++) {
	valpool.p_nval = 0;
	pmidp = (__pmID_int *)&pmidlist[i];
	if (pmidp->domain == IRIX && pmidp->cluster <= maxcluster) {
	    if ((j = clmap[pmidp->cluster]) != -1) {
		if (cltab[j].done == 0) {
		    cltab[j].fetch_setup();
		    cltab[j].done = 1;
		}
		if ((sts = cltab[j].fetch(pmidlist[i], cur_prof, &valpool)) < 0) {
		    /* error is encoded in numval of pmResult */
		    valpool.p_nval = sts;
		}
	    }
	}

	/*
	 * Build the result for the i'th pmid
	 */
	if (valpool.p_nval == 1)
	    vsp = result->vset[i] = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	else if (valpool.p_nval > 1)
	    vsp = result->vset[i] = (pmValueSet *)malloc(
			sizeof(pmValueSet) + (valpool.p_nval-1) * sizeof(pmValue));
	else
	    vsp = result->vset[i] = (pmValueSet *)malloc(
			sizeof(pmValueSet) - sizeof(pmValue));
	if (vsp == (pmValueSet *)0) {
	    __pmNotifyErr(LOG_WARNING, "irix_fetch: vset malloc x %d: %s",
		valpool.p_nval, pmErrStr(-errno));
	    vsp->numval = 0;
	}
	else
	    vsp->numval = valpool.p_nval;

	vsp->pmid = pmidlist[i];
	vsp->valfmt = valpool.p_valfmt;

	if (vsp->numval == 1)
	    vsp->vlist[0] = valpool.p_vp[0];	/* struct asignment */
	else if (vsp->numval > 1)
	    memcpy((void *)vsp->vlist, (void *)valpool.p_vp, vsp->numval * sizeof(vsp->vlist[0]));
    }

    result->numpmid = numpmid;
    gettimeofday(&result->timestamp);

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_FETCH) || (pmIrixDebug & DBG_IRIX_FETCH))
	__pmDumpResult(stderr, result);
#endif
    *resp = result;
    return 0;
}

static int
irix_desc(pmID pmid, pmDesc *desc)
{
    __pmID_int	*pmidp;
    int		j;

    pmidp = (__pmID_int *)&pmid;
    if (pmidp->domain == IRIX && pmidp->cluster <= maxcluster)
	if ((j = clmap[pmidp->cluster]) != -1)
	    return cltab[j].desc(pmid, desc);

    return PM_ERR_PMID;
}

static int
irix_text(int ident, int type, char **buffer)
{
    char *mybuf;

    if (!helpInit) {
    	helpHndl = pmdaOpenHelp(HELP_TEXT_PATH);
	if (helpHndl < 0) {
	    __pmNotifyErr(LOG_WARNING, "Help text \"%s\": %s\n",
			  HELP_TEXT_PATH, pmErrStr(helpHndl));
	}
	helpInit = 1; 
    }
    
    if (helpHndl >= 0) {
	if ((type & PM_TEXT_PMID) == PM_TEXT_PMID)
	    mybuf = pmdaGetHelp(helpHndl, (pmID)ident, type);
	else
	    mybuf = pmdaGetInDomHelp(helpHndl, (pmInDom)ident, type);
	if (mybuf == (char *)0)
	    *buffer = (char *)0;
	else
	    *buffer = strdup(mybuf);
    }
    else
    	*buffer = (char *)0;

    return (*buffer == (char *)0) ? PM_ERR_TEXT : 0;
}

/*ARGSUSED*/
static int
irix_control(pmResult *request, int state, int control, int rate)
{
    /*
     * nothing tricky we can do.
     * this is all handled in the PMCD, and Irix instrumentation is not
     * yet in a state where dynamic activation/deactivation is possible.
     */
    return 0;
}

/*ARGSUSED*/
static int
irix_store(pmResult *result)
{
    int 	m;
    __pmID_int	*pmidp;
    pmAtomValue	av;
    pmValueSet	*vsp;
    int		sts;

    for (m = 0; m < result->numpmid; m++) {
	vsp = result->vset[m];
	pmidp = (__pmID_int *)&vsp->pmid;
	if (pmidp->domain == IRIX && pmidp->cluster == 0 && pmidp->item == 1) {
	    int		i;
	    /*
	     * irix.pmda.reset
	     * reset the cluster-specific code
	     */
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_INDOM || pmIrixDebug & DBG_IRIX_INDOM)
		__pmNotifyErr(LOG_WARNING, "irix_store: reset for all indoms and clusters\n");
#endif
	    init_indom(IRIX, 1);

	    for (i = 0; i < nclust; i++)
		(cltab[i].init)(1);

	    /* Close help if we opened it */
	    if (helpInit && helpHndl >= 0)
		pmdaCloseHelp(helpHndl);
	    helpHndl = -1;
	    helpInit = 0;

	    __pmNotifyErr(LOG_INFO, "irix_store: irix.pmda.reset completed\n");
	}
	else if (pmidp->domain == IRIX && pmidp->cluster == 0 && pmidp->item == 4) {
	    /* irix.pmda.debug */
	    if (vsp->numval != 1 || vsp->valfmt != PM_VAL_INSITU)
		return PM_ERR_CONV;
	    if ((sts = pmExtractValue(vsp->valfmt, \
				      &vsp->vlist[0], 
				      PM_TYPE_32, &av, 
				      PM_TYPE_32)) < 0)
		return sts;
#ifdef PCP_DEBUG
	    if (av.l < 0) {
/*
 * storing some magic negative numbers here triggers dump/diagnostic routines
 *
 *	-10	XPCdump();
 */
		switch (av.l) {
		    case -10:
			XPCdump();
			break;

		    default:
			return -EINVAL;
		}
		return 0;
	    }
#endif
	    pmIrixDebug = av.l;
	}
	else if (pmidp->domain == IRIX && 
		 pmidp->cluster == 42 && 
		 pmidp->item == 5) {
	    /* irix.xbow.map.switch */
	    if (vsp->valfmt != PM_VAL_INSITU)
		return PM_ERR_CONV;
	    if ((sts = xbow_store(vsp)) < 0)
		return sts;
	}
	else 
	    /* storing anything else in the irix PMD is not allowed */
	    return -EACCES;
    }

    return 0;
}

void
irix_init(pmdaInterface *dp)
{
    int		i;
#if defined(IRIX6_5)
    sgt_info_t  info;
    sgt_cookie_t ck;
#endif

    pmIrixDebug = pmDebug;		/* before anything bad can happen! */

    dp->domain = IRIX;
    dp->version.one.profile = irix_profile;
    dp->version.one.fetch = irix_fetch;
    dp->version.one.desc = irix_desc;
    dp->version.one.instance = irix_instance;
    dp->version.one.text = irix_text;
    dp->version.one.control = irix_control;
    dp->version.one.store = irix_store;

    /*
     * some global, platform stuff ...
     */
    _pm_pagesz = getpagesize();

    /* independent of the state of the HWG, this is the definitive test */

#if defined(IRIX6_2) || defined(IRIX6_3)
    _pm_has_nodes = 0;

#else
    if (sysmp(MP_NUMNODES) <= 0)
	_pm_has_nodes = 0;
#endif

#if defined(IRIX6_5)

    /* Figure out number of cells */

    SGT_COOKIE_INIT(&ck);
    if (sysget(SGT_SINFO, (char *)&info, sizeof(info), SGT_INFO, &ck) < 0) {
	info.si_num = 0;
    }
    _pm_numcells = info.si_num > 1 ? info.si_num : 0;

#endif

    /* Tell PMCD that we are an Interface version 1 PMDA */
    dp->comm.pmda_interface = PMDA_INTERFACE_1;
    dp->comm.pmapi_version = PMAPI_VERSION;

    kmeminit();		/* needs to be early on, as others depend on it */

    init_indom(dp->domain, 0);

    /*
     * initialize the cluster-specific code
     */
    maxcluster = 0;
    for (i = 0; i < nclust; i++) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_INDOM || pmIrixDebug & DBG_IRIX_INDOM)
	    __pmNotifyErr(LOG_WARNING, 
			  "irix_init: calling init for cluster %d @ %p\n", 
			  i, cltab[i].init);
#endif
	(cltab[i].init)(0);
	if (cltab[i].cluster > maxcluster)
	    maxcluster = cltab[i].cluster;
    }

    if ((clmap = (int *)malloc((maxcluster+1) * sizeof(clmap[0]))) == (int *)0) {
	__pmNotifyErr(LOG_WARNING, "irix_init: clmap malloc: %s", pmErrStr(-errno));
	maxcluster = -1;
	
    }
    else {
	for (i = 0; i <= maxcluster; i++)
	    clmap[i] = -1;
	for (i = 0; i < nclust; i++)
	    clmap[cltab[i].cluster] = i;
    }

    valpool.p_nval = 1;		/* force the initial allocation of 16 elts */
    sizepool(&valpool);

    dp->status = 0;
    return;
}

#define VPCB_INC 16		/* expand by this number each time */

int
sizepool(pmVPCB *vpcb)
{
    if (vpcb->p_nval <= vpcb->p_maxnval)
	return 0;

    vpcb->p_maxnval = (1 + vpcb->p_nval / VPCB_INC) * VPCB_INC;
    if ((vpcb->p_vp = (pmValue *)realloc((void *)vpcb->p_vp, vpcb->p_maxnval * sizeof(pmValue))) == (pmValue *)0)
	return -errno;
    
    return 0;
}

int
avcpy(pmAtomValue *avp, void *data, int type)
{
    switch(type) {
    case PM_TYPE_32:
	avp->l = *(__int32_t *)data;
	break;
    case PM_TYPE_U32:
	avp->ul = *(__uint32_t *)data;
	break;
    case PM_TYPE_64:
	avp->ll = *(__int64_t *)data;
	break;
    case PM_TYPE_U64:
	avp->ull = *(__uint64_t *)data;
	break;
    case PM_TYPE_FLOAT:
	avp->f = *(float *)data;
	break;
    case PM_TYPE_DOUBLE:
	avp->d = *(double *)data;
	break;
    default:
	return PM_ERR_UNIT;
    }
    return 0;
}

#if !defined(IRIX6_2) && !defined(IRIX6_3)

char *
__hwg2inst(const pmInDom indom, char *path)
{
    char	buf[MAXPATHLEN];
    char	*module = (char *)0;
    char	*slot = (char *)0;
    char	*unit = (char *)0;
    char	*p;
    char	*pend = &path[strlen(path)];
    pmInDom	myindom = PM_INDOM_NULL;
    int		metarouter = 0;

    p = strtok(path, "/");
    if (p == NULL) goto cleanup;
    if (strcmp(p, "hw") != 0) goto cleanup;

    p = strtok(NULL, "/");
    if (p == NULL) goto cleanup;
    if (strcmp(p, "module") != 0) goto cleanup;

    module = strtok(NULL, "/");
    if (module == NULL) goto cleanup;

    p = strtok(NULL, "/");
    if (p == NULL) goto cleanup;
    if (strcmp(p, "slot") != 0) goto cleanup;

    /* /hw/module/<module>/slot/ ... so far */

    slot = strtok(NULL, "/");
    if (slot == NULL) goto cleanup;

    if (slot[0] == 'n' && (indom == PM_INDOM_NULL || 
			   indom == PM_INDOM_NODE || 
			   indom == PM_INDOM_CPU  ||
			   indom == PM_INDOM_XBOW)) {
        slot++;

	p = strtok(NULL, "/");
	if (p == NULL) goto cleanup;
	if (strcmp(p, "node") != 0) goto cleanup;

	/* /hw/module/<module>/slot/n<node-slot>/node/ ... so far */

	p = strtok(NULL, "/");
	if (p == NULL) {
	    myindom = PM_INDOM_NODE;
	    goto cleanup;
	}

	if (strcmp(p, "xtalk") == 0) {
	    p = strtok(NULL, "/");
	    if (p == NULL) goto cleanup;

	    if (strcmp(p, "0") != 0) goto cleanup;

	    p = strtok(NULL, "/");
	    if (p == NULL) goto cleanup;

	    /* /hw/module/<module>/slot/n<node-slot>/node/xtalk/0/ ... so far */

	    if (strcmp(p, "mon") == 0)
		myindom = PM_INDOM_XBOW;
	    goto cleanup;
	}
	else if (strcmp(p, "cpu") != 0) goto cleanup;

	unit = strtok(NULL, "/");
	if (unit == NULL) goto cleanup;

	p = strtok(NULL, "/");
	if (p == NULL)
	    myindom = PM_INDOM_CPU;
        goto cleanup;
    }
    else
    if (slot[0] == 'r' && (indom == PM_INDOM_NULL || 
			   indom == PM_INDOM_ROUTER || 
			   indom == PM_INDOM_ROUTERPORT)) {
	slot++;

	p = strtok(NULL, "/");
	if (p == NULL) goto cleanup;

	if (strcmp(p, "metarouter") == 0) {

	    /* /hw/module/<module>/slot/r<router-slot>/metarouter/ ... so far */
	    
	    metarouter = 1;
	}
	else if (strcmp(p, "router") != 0) goto cleanup;

	/* /hw/module/<module>/slot/r<router-slot>/router/ ... so far */

	unit = strtok(NULL, "/");
	if (unit == NULL) {
	    myindom = PM_INDOM_ROUTER;
	    goto cleanup;
	}

	for (p = unit; *p; p++) {
	    if (!isascii(*p) || !isdigit(*p)) goto cleanup;
	}
        myindom = PM_INDOM_ROUTERPORT;
    }
    else if (strcmp(slot, "MotherBoard") == 0) {

    	/* Fix for #507603, o200 node and CPU slot numbers are the
	   same as module numbers */

	slot = module;
    	p = strtok(NULL, "/");
	if (p == NULL) goto cleanup;
	if (strcmp(p, "node") != 0) goto cleanup;

	/* /hw/module/<module>/slot/MotherBoard/node/... so far */

	unit = strtok(NULL, "/");
	if (unit == NULL) {
	    myindom = PM_INDOM_NODE;
	    goto cleanup;
	}
	else if (strcmp(unit, "cpu") != 0) goto cleanup;

	unit = strtok(NULL, "/");
	if (unit == NULL) goto cleanup;

	/* /hw/module/<module>/slot/MotherBoard/node/cpu/{a,b} */

	if (strcmp(unit, "a") != 0 && strcmp(unit, "b") != 0) 
	    goto cleanup;

	myindom = PM_INDOM_CPU;
	goto cleanup;
		
    }
    else goto cleanup;


cleanup:

    switch (myindom) {
    case PM_INDOM_NODE:
	sprintf(buf, "node:%s.%s", module, slot);
	break;

    case PM_INDOM_CPU:
	sprintf(buf, "cpu:%s.%s.%s", module, slot, unit);
	break;

    case PM_INDOM_ROUTER:
	if (metarouter)
	    sprintf(buf, "mrouter:%s.%s", module, slot);
	else
	    sprintf(buf, "router:%s.%s", module, slot);
	break;

    case PM_INDOM_ROUTERPORT:
	if (metarouter)
	    sprintf(buf, "mrport:%s.%s.%s", module, slot, unit);
	else
	    sprintf(buf, "rport:%s.%s.%s", module, slot, unit);
	break;

    case PM_INDOM_XBOW:
	sprintf(buf, "xbow:%s.%s", module, slot);
	break;

    default:
	myindom = PM_INDOM_NULL;
	break;
    }

    for (p = path; p < pend; p++) {
	if (*p == '\0') *p = '/';
    }

    if (myindom == PM_INDOM_NULL)
	return strdup(path);
    else
	return strdup(buf);
}

#endif /* !defined(IRIX6_2) && !defined(IRIX6_3) */

#ifdef PCP_DEBUG

/*
 * Simple malloc audit ... use the real routines here
 */

#ifdef malloc
#undef malloc
#endif
#ifdef realloc
#undef realloc
#endif
#ifdef calloc
#undef calloc
#endif
#ifdef valloc
#undef valloc
#endif
#ifdef strdup
#undef strdup
#endif
#ifdef free
#undef free
#endif

void *
__pmMalloc(const char *file, int line, size_t bytes)
{
    void *res;

    res = malloc(bytes);
    if (pmIrixDebug & DBG_IRIX_MALLOC)
	fprintf(stderr, "audit: %s %d malloc 0x%p %d\n", 
		file, line, res, (int)bytes);
    return res;
}

void *
__pmRealloc(const char *file, int line, void *ptr, size_t bytes)
{
    void *res;

    res = realloc(ptr, bytes);
    if (pmIrixDebug & DBG_IRIX_MALLOC)
	fprintf(stderr, "audit: %s %d realloc 0x%p 0x%p %d\n", 
		file, line, ptr, res, (int)bytes);
    return res;
}

void *
__pmCalloc(const char *file, int line, size_t num, size_t bytes)
{
    void *res;

    res = calloc(num, bytes);
    if (pmIrixDebug & DBG_IRIX_MALLOC)
	fprintf(stderr, "audit: %s %d calloc 0x%p %d\n", file, line, res,
		(int)num * (int)bytes);
    return res;
}

void *
__pmValloc(const char *file, int line, size_t bytes)
{
    void *res;

    res = valloc(bytes);
    if (pmIrixDebug & DBG_IRIX_MALLOC)
	fprintf(stderr, "audit: %s %d valloc 0x%p %d\n", 
		file, line, res, (int)bytes);
    return res;
}

void
__pmFree(const char *file, int line, void *ptr)
{
    if (pmIrixDebug & DBG_IRIX_MALLOC)
	fprintf(stderr, "audit: %s %d free 0x%p\n", file, line, ptr);
    free(ptr);
}

char *
__pmStrdup(const char *file, int line, const char *str)
{
    char *res;

    res = strdup(str);
    if (pmIrixDebug & DBG_IRIX_MALLOC)
	fprintf(stderr, "audit: %s %d strdup 0x%p %d\n", 
		file, line, res, (int)strlen(res));
    return res;
}

#endif /* PCP_DEBUG */

/*
 * Warning do not add source after this, due to the malloc-family
 * #undef's above
 */
