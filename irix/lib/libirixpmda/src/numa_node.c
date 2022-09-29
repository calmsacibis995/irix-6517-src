/*
 * Handle metrics for cluster numa_node (39)
 */

#ident "$Id: numa_node.c,v 1.22 1997/10/07 06:16:49 chatz Exp $"

#if defined(IRIX6_2) || defined(IRIX6_3)

#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

/*ARGSUSED*/
void
numa_init(int reset)
{
}

void
numa_fetch_setup(void)
{
}

static pmDesc dummy = {
    0, PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0}
};

int
numa_desc(pmID pmid, pmDesc *desc)
{
    dummy.pmid = pmid;
    *desc = dummy;
    return 0;
}

/*ARGSUSED*/
int
numa_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    vpcb->p_nval = PM_ERR_APPVERSION;
    return 0;
}

#else /* 6.4 or later */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include <paths.h>

#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./numa_node.h"

#define NODE_EXT_LEN	32		/* number of characters required for external name */

static numa_stats_t	numastats;	/* used for offset calculations */

/* descriptors for metrics below irix.numa */
static pmMeta meta[] = {

/* irix.numa.routerload */
{ (char *)&numastats.traffic_local_router,
	{ PMID(1,39,1), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,0,0,0,0} } },

/* irix.numa.migr.threshold */
{ (char *)&numastats.migr_threshold,
	{ PMID(1,39,2), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,0,0,0,0} } },

#if defined(IRIX6_4)
/* irix.numa.migr.intr.total */
{ (char *)&numastats.migr_interrupt_number,
	{ PMID(1,39,3), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#else
/* irix.numa.migr.intr.total */
{ (char *)&numastats.refcnt_interrupt_number,
	{ PMID(1,39,3), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif

/* irix.numa.migr.intr.failstate */
{ (char *)&numastats.migr_interrupt_failstate,
	{ PMID(1,39,4), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.intr.failenabled */
{ (char *)&numastats.migr_interrupt_failenabled,
	{ PMID(1,39,5), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.intr.failfrozen */
{ (char *)&numastats.migr_interrupt_failfrozen,
	{ PMID(1,39,6), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.auto.total */
{ (char *)NULL, /* derived */
	{ PMID(1,39,7), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.auto.in */
{ (char *)&numastats.migr_auto_number_in,
	{ PMID(1,39,8), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.auto.out */
{ (char *)&numastats.migr_auto_number_out,
	{ PMID(1,39,9), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.auto.fail */
{ (char *)&numastats.migr_auto_number_fail,
	{ PMID(1,39,10), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.auto.queue_len */
{ (char *)&numastats.migr_auto_number_queued,
	{ PMID(1,39,11), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.user.total */
{ (char *)NULL, /* derived */
	{ PMID(1,39,12), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.user.in */
{ (char *)&numastats.migr_user_number_in,
	{ PMID(1,39,13), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.user.out */
{ (char *)&numastats.migr_user_number_out,
	{ PMID(1,39,14), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.user.fail */
{ (char *)&numastats.migr_user_number_fail,
	{ PMID(1,39,15), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.user.queue_len */
{ (char *)&numastats.migr_user_number_queued,
	{ PMID(1,39,16), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.queue.total */
{ (char *)NULL, /* derived */
	{ PMID(1,39,17), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.queue.in */
{ (char *)&numastats.migr_queue_number_in,
	{ PMID(1,39,18), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.queue.out */
{ (char *)&numastats.migr_queue_number_out,
	{ PMID(1,39,19), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.queue.fail.total */
{ (char *)&numastats.migr_queue_number_fail,
	{ PMID(1,39,20), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.queue.fail.overflow */
{ (char *)&numastats.migr_queue_number_overflow,
	{ PMID(1,39,21), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.queue.fail.state */
{ (char *)&numastats.migr_queue_number_failstate,
	{ PMID(1,39,22), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.queue.fail.failq */
{ (char *)&numastats.migr_queue_number_failq,
	{ PMID(1,39,23), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.coalesc.total */
{ (char *)NULL, /* derived */
	{ PMID(1,39,24), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.coalesc.in */
{ (char *)&numastats.migr_coalescingd_number_in,
	{ PMID(1,39,25), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.coalesc.out */
{ (char *)&numastats.migr_coalescingd_number_out,
	{ PMID(1,39,26), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.coalesc.fail */
{ (char *)&numastats.migr_coalescingd_number_fail,
	{ PMID(1,39,27), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.triggers.capacity */
{ (char *)&numastats.migr_queue_capacity_trigger_count,
	{ PMID(1,39,28), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.triggers.time */
{ (char *)&numastats.migr_queue_time_trigger_count,
	{ PMID(1,39,29), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.migr.triggers.traffic */
{ (char *)&numastats.migr_queue_traffic_trigger_count,
	{ PMID(1,39,30), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.memory.lent */
{ (char *)&numastats.pagealloc_lent_memory,
	{ PMID(1,39,31), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },

/* irix.numa.memory.replicated.page_count */
{ (char *)&numastats.repl_pagecount,
	{ PMID(1,39,32), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.memory.replicated.page_dest */
{ (char *)&numastats.repl_page_destination,
	{ PMID(1,39,33), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.memory.replicated.page_reuse */
{ (char *)&numastats.repl_page_reuse,
	{ PMID(1,39,34), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.memory.replicated.page_notavail */
{ (char *)&numastats.repl_page_notavail,
	{ PMID(1,39,35), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.memory.replicated.control_refuse */
{ (char *)&numastats.repl_control_refuse,
	{ PMID(1,39,36), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.unpegger.calls */
{ (char *)&numastats.migr_unpegger_calls,
	{ PMID(1,39,37), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.unpegger.npages */
{ (char *)&numastats.migr_unpegger_npages,
	{ PMID(1,39,38), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.unpegger.last_npages */
{ (char *)&numastats.migr_unpegger_lastnpages,
	{ PMID(1,39,39), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.bouncectl.calls */
{ (char *)&numastats.migr_bouncecontrol_calls,
	{ PMID(1,39,40), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.bouncectl.frozen_pages */
{ (char *)&numastats.migr_bouncecontrol_frozen_pages,
	{ PMID(1,39,41), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.bouncectl.melt_pages */
{ (char *)&numastats.migr_bouncecontrol_melt_pages,
	{ PMID(1,39,42), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.bouncectl.last_melt_pages */
{ (char *)&numastats.migr_bouncecontrol_last_melt_pages,
	{ PMID(1,39,43), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.bouncectl.dampened_pages */
{ (char *)&numastats.migr_bouncecontrol_dampened_pages,
	{ PMID(1,39,44), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.memoryd.activations */
{ (char *)&numastats.memoryd_total_activations,
	{ PMID(1,39,45), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* irix.numa.memoryd.periodic */
{ (char *)&numastats.memoryd_periodic_activations,
	{ PMID(1,39,46), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

/* *** fillers *** */
{ (char *)0,
	{ PMID(1,39,47), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
{ (char *)0,
	{ PMID(1,39,48), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },

/* hinv.map.node */
 { (char *)0,
       { PMID(1,39,49), PM_TYPE_STRING, PM_INDOM_NODE, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static char		*fetched = (char *)0;
static int		direct_map = 1;

extern int		n_nodes;
extern _pm_node		*nodes;

static int
refresh_numa_stats(_pm_node *node)
{
    int err = 0;

    err = (int)syssgi(SGI_NUMA_STATS_GET, node->hwgname, &node->numa);
#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_DEBUG)
	__pmNotifyErr(LOG_DEBUG, "syssgi(SGI_NUMA_STATS_GET, node->hwgname=%s, &node->numa=%p) -> %d\n", node->hwgname, &node->numa, err);
#endif
    if (err < 0) {
	__pmNotifyErr(LOG_ERR, "refresh_numa_stats: syssgi failed: %s\n", strerror(errno));
	return -errno;
    }
    return 0;
}

void
numa_init(int reset)
{
    int		i;
    static int	offset_done = 0;
    extern int	errno;

    fetched = (char *)realloc(fetched, n_nodes * sizeof(char));

    if (reset)
	return;

    if (n_nodes == 0) {
	if (_pm_has_nodes)
	    __pmNotifyErr(LOG_WARNING, 
			 "numa_init: No nodes found in hardware graph\n");
    }

    if (!offset_done) {
	for (i = 0; i < nmeta; i++) {
	    meta[i].m_desc.indom = indomtab[PM_INDOM_NODE].i_indom;
	    if (meta[i].m_offset != (char *)0)
		meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&numastats));
	    if (direct_map && meta[i].m_desc.pmid != PMID(1,39,i+1)) {
		direct_map = 0;
		__pmNotifyErr(LOG_WARNING, 
			     "numa_init: direct map disabled @ meta[%d]", i);
	    }
	}
	offset_done=1;
    }
}

void
numa_fetch_setup(void)
{
    int i;

    for (i=0; i < n_nodes; i++)
	fetched[i] = 0;
}

int
numa_desc(pmID pmid, pmDesc *desc)
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

int
numa_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    int			d;
    int			sts;
    int			nval;
    pmAtomValue		av;
    void		*vp;
    numa_stats_t	*npnuma;
    _pm_node		*np;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "numa_fetch: direct mapping failed for %s (!= %s)\n",
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

    vpcb->p_nval = 0;
    if (meta[i].m_desc.type == PM_TYPE_NOSUPPORT || n_nodes == 0)
	return 0;
    
    /*
     * Iterate over all the numa node instances,
     * exclude those instances not included by the profile,
     * and fetch those that are included but not yet fetched.
     */
    for (d=0; d < n_nodes; d++) {
	np = &nodes[d];
	if (__pmInProfile(indomtab[PM_INDOM_NODE].i_indom, profp, NODE_ID(np))) {
	    if (fetched[d] == 0) {
		if ((sts = refresh_numa_stats(np)) < 0)
		    return sts;
		fetched[d]++;
	    }
	    vpcb->p_nval++;
	}
    }
    
    sizepool(vpcb);
    
    nval = 0;
    for (d=0; d < n_nodes; d++) {

	np = &nodes[d];

	if (fetched[d] == 0) {
	    /* not in profile */
	    continue;
	}

	npnuma = &np->numa;

	/* special case ones? */
	switch (pmid) {
	case PMID(1,39,1):		/* irix.numa.routerload */
	    /* field is an instantaneous uchar value */
	    av.ul = (unsigned int)npnuma->traffic_local_router;
	    break;

	case PMID(1,39,2):		/* irix.numa.migr.threshold */
	    /* field is an instantaneous uchar value */
	    av.ul = (unsigned int)npnuma->traffic_local_router;
	    break;

	case PMID(1,39,7):		/* irix.numa.migr.auto.total */
	    av.ul = npnuma->migr_auto_number_in + npnuma->migr_auto_number_out;
	    break;

	case PMID(1,39,12):		/* irix.numa.migr.user.total */
	    av.ul = npnuma->migr_user_number_in + npnuma->migr_user_number_out;
	    break;

	case PMID(1,39,17):		/* irix.numa.migr.queue.total */
	    av.ul = npnuma->migr_queue_number_in + npnuma->migr_queue_number_out;
	    break;

	case PMID(1,39,24):		/* irix.numa.migr.coalesc.total */
	    av.ul = npnuma->migr_coalescingd_number_in + npnuma->migr_coalescingd_number_out;
	    break;

	case PMID(1,39,49):		/* hinv.map.node */
	    av.cp = np->hwgname;
	    break;
	    /*
	     * add any other derived metrics here
	     */

	default:			/* all remaining fields in numa_stats_t */
	    vp = (void *)&((char *)npnuma)[(ptrdiff_t)meta[i].m_offset];
	    avcpy(&av, vp, meta[i].m_desc.type);
	    break;
	}

	if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
	    return sts;
	vpcb->p_vp[nval].inst = NODE_ID(np);
	if (nval == 0)
	    vpcb->p_valfmt = sts;
	nval++;
    }
    return 0;
}

#endif /* defined(IRIX6_2) || defined(IRIX6_3) */

