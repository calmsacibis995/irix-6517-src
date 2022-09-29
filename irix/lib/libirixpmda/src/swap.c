/*
 * Handle metrics for cluster swap (16)
 *
 * Code built by newcluster on Mon Jun  6 15:55:48 EST 1994
 */

#ident "$Id: swap.c,v 1.17 1997/11/14 03:08:58 chatz Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/immu.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

swaptbl_t 	*swaptable = (swaptbl_t *)NULL;
extern int	_pm_pagesz; /* declared and initialised in irix.c */

static pmMeta		meta[] = {
/* irix.swapdev.free */
  { 0, { PMID(1,16,1), PM_TYPE_U32, PM_INDOM_SWAP, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swapdev.length */
  { 0, { PMID(1,16,2), PM_TYPE_U32, PM_INDOM_SWAP, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swapdev.maxswap */
  { 0, { PMID(1,16,3), PM_TYPE_U32, PM_INDOM_SWAP, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swapdev.vlength */
  { 0, { PMID(1,16,4), PM_TYPE_U32, PM_INDOM_SWAP, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swapdev.priority */
  { 0, { PMID(1,16,5), PM_TYPE_32, PM_INDOM_SWAP, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.swap.free */
  { 0, { PMID(1,16,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swap.length */
  { 0, { PMID(1,16,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swap.maxswap */
  { 0, { PMID(1,16,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swap.vlength */
  { 0, { PMID(1,16,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swap.alloc */
  { 0, { PMID(1,16,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swap.reserve */
  { 0, { PMID(1,16,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swap.used */
  { 0, { PMID(1,16,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.swap.unused */
  { 0, { PMID(1,16,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static int		*interesting = (int *)NULL;
static int		direct_map = 1;

void swap_init(int reset)
{
    int		i;
    int		n = indomtab[PM_INDOM_SWAP].i_numinst;
    int 	indomtag; /* Constant from descr in form */

    if (reset)
	return;

    /* 
     * TODO: check indomtab[PM_INDOM_SWAP].i_numinst
     * equals the _current_ number of logical swap areas
     */

    for (i = 0; i < nmeta; i++) {
        indomtag = meta[i].m_desc.indom;
	if (direct_map && meta[i].m_desc.pmid != PMID(1,16,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "swap_init: direct map disabled @ meta[%d]", i);
	}
        if (indomtag == PM_INDOM_NULL)
            continue;
        if (indomtag < 0 || indomtag >= PM_INDOM_NEXT) {
            __pmNotifyErr(LOG_ERR, "swap_init: bad instance domain (%d) for metric %s\n",
                         indomtag, pmIDStr(meta[i].m_desc.pmid));
            continue;
        }
        /* Replace tag with it's indom */
        meta[i].m_desc.indom = indomtab[indomtag].i_indom;
    }


    swaptable = (swaptbl_t *)malloc(sizeof(swaptbl_t) + n * sizeof(struct swapent));
    if (swaptable == (swaptbl_t *)0) {
	__pmNotifyErr(LOG_WARNING, "swap_init: swaptable malloc: %s", pmErrStr(-errno));
	nmeta = 0;
	return;
    }
    swaptable->swt_n = n;
    interesting = (int *)malloc(n * sizeof(int));
    if (interesting == (int *)0) {
	__pmNotifyErr(LOG_WARNING, "swap_init: interesting malloc: %s", pmErrStr(-errno));
	nmeta = 0;
	return;
    }

    for (i=0; i < n; i++)
	swaptable->swt_ent[i].ste_path = (char *)malloc(PATH_MAX);
    swapctl(SC_LIST, swaptable);
}

void swap_fetch_setup(void)
{
    fetched = 0;
}

int swap_desc(pmID pmid, pmDesc *desc)
{
    int		i;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    *desc = meta[i].m_desc;	/* struct assignment */
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

int swap_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		k;
    int		nval;
    static int	n_swap;
    int		sts;
    pmAtomValue	av;
    swapent_t	aggregate;
    off_t	s_tot, s_free, s_resv, s_lmax;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "swap_fetch: direct mapping failed for %s (!= %s)\n",
		     pmIDStr(pmid), pmIDStr(meta[i].m_desc.pmid));
	direct_map = 0;
    }

    for (i = 0; i < nmeta; i++)
	if (pmid == meta[i].m_desc.pmid)
	    break;

    if (i >= nmeta) {
	vpcb->p_nval = 0;
	return PM_ERR_PMID;
    }

 doit:

    if (fetched == 0) {
	while ((sts = swapctl(SC_LIST, swaptable)) < 0) {
	    switch (errno) {
	    case ENOMEM:
		/* number of swap entries has increased */
		sts = swapctl(SC_GETNSWP, (int *)NULL);
		indomtab[PM_INDOM_SWAP].i_numinst = sts;
		for (k=0; k < swaptable->swt_n; k++)
		    free(swaptable->swt_ent[k].ste_path);
		swaptable = (swaptbl_t *)realloc(swaptable,
						 sizeof(swaptbl_t) + sts * sizeof(struct swapent));
		swaptable->swt_n = sts;
		interesting = (int *)realloc(interesting, swaptable->swt_n * sizeof(int));
		if (swaptable == (swaptbl_t *)NULL || interesting == (int *)NULL) {
		    __pmNotifyErr(LOG_WARNING, "swap_fetch: %s", pmErrStr(-errno));
		    return -errno;
		}
		for (k=0; k < swaptable->swt_n; k++)
		    swaptable->swt_ent[k].ste_path = (char *)malloc(PATH_MAX);
		break;
	    default:
		__pmNotifyErr(LOG_WARNING, "swap_fetch: %s", pmErrStr(-errno));
		return -errno;
	    }
	}
	n_swap = sts;
	fetched = 1;
    }
    
    if (meta[i].m_desc.indom != PM_INDOM_NULL) {
	/*
	 * metrics with multiple instances
	 */
	vpcb->p_nval = 0;
	for (k=0; k < n_swap; k++) {
	    /* which values are interesting ? */
	    if (interesting[k] = __pmInProfile(meta[i].m_desc.indom, 
					      profp, 
					      swaptable->swt_ent[k].ste_lswap))
		vpcb->p_nval++;
	}
	sizepool(vpcb);
	nval = 0;
	sts = 0;
	for (k=0; k < n_swap; k++) {
	    if (interesting[k] == 0)
		continue;
	    switch (pmid) {
	    case PMID(1,16,1):		/* irix.swapdev.free */
		av.ul = (__uint32_t)(swaptable->swt_ent[k].ste_free * _pm_pagesz / 1024);
		break;
	    case PMID(1,16,2):		/* irix.swapdev.length */
		av.ul = (__uint32_t)(swaptable->swt_ent[k].ste_pages * _pm_pagesz / 1024);
		break;
	    case PMID(1,16,3):		/* irix.swapdev.maxswap */
		av.ul = (__uint32_t)(swaptable->swt_ent[k].ste_maxpages * _pm_pagesz / 1024);
		break;
	    case PMID(1,16,4):		/* irix.swapdev.vlength */
		av.ul = (__uint32_t)(swaptable->swt_ent[k].ste_vpages * _pm_pagesz / 1024);
		break;
	    case PMID(1,16,5):		/* irix.swapdev.priority */
		av.l = swaptable->swt_ent[k].ste_pri;
		break;
	    default:
		return PM_ERR_PMID;
	    }

	    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
		break;

	    vpcb->p_vp[nval++].inst = swaptable->swt_ent[k].ste_lswap;
	}
	vpcb->p_valfmt = sts;
	vpcb->p_nval = nval;
	return sts;
    }
    else {
	/*
	 * single instance metrics
	 * (i.e. aggregated totals)
	 */
	vpcb->p_nval = 1;
	vpcb->p_vp[0].inst = PM_IN_NULL;

	if (pmid >= PMID(1,16,6) && pmid <= PMID(1,16,9)) {
	    /* akin to swap -l */
	    memset((void *)&aggregate, 0, sizeof(swapent_t));

	    for (k=0; k < n_swap; k++) {
		aggregate.ste_free += swaptable->swt_ent[k].ste_free;
		aggregate.ste_pages += swaptable->swt_ent[k].ste_pages;
		aggregate.ste_maxpages += swaptable->swt_ent[k].ste_maxpages;
		aggregate.ste_vpages += swaptable->swt_ent[k].ste_vpages;
	    }
	}
	else {
	    /* akin to swap -s */
	    swapctl(SC_GETSWAPTOT, &s_tot);
	    swapctl(SC_GETFREESWAP, &s_free);
	    swapctl(SC_GETRESVSWAP, &s_resv);
	    swapctl(SC_GETLSWAPTOT, &s_lmax);
	}

	switch (pmid) {
	case PMID(1,16,6):		/* irix.swap.free */
	    av.l = (__int32_t)(aggregate.ste_free * _pm_pagesz / 1024);
	    break;
	case PMID(1,16,7):		/* irix.swap.length */
	    av.l = (__int32_t)(aggregate.ste_pages * _pm_pagesz / 1024);
	    break;
	case PMID(1,16,8):		/* irix.swap.maxswap */
	    av.l = (__int32_t)(aggregate.ste_maxpages * _pm_pagesz / 1024);
	    break;
	case PMID(1,16,9):		/* irix.swap.vlength */
	    av.l = (__int32_t)(aggregate.ste_vpages * _pm_pagesz / 1024);
	    break;
	case PMID(1,16,10):		/* irix.swap.alloc ... 2 == 1024/512 */
	    av.ul = (__uint32_t)((s_tot - s_free) / 2);
	    break;
	case PMID(1,16,11):		/* irix.swap.reserve ... 2 == 1024/512 */
	    av.ul = (__uint32_t)((s_resv - s_tot + s_free) / 2);
	    break;
	case PMID(1,16,12):		/* irix.swap.used ... 2 == 1024/512 */
	    av.ul = (__uint32_t)(s_resv / 2);
	    break;
	case PMID(1,16,13):		/* irix.swap.unused ... 2 == 1024/512 */
	    av.ul = (__uint32_t)((s_lmax - s_resv) / 2);
	    break;
	default:
	    return PM_ERR_PMID;
	}

	sts = __pmStuffValue(&av, 0, &vpcb->p_vp[0], meta[i].m_desc.type);
	vpcb->p_valfmt = sts;
	return sts;
    }
}
