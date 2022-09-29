/*
 * Handle metrics for cluster 44
 */

#ident "$Id: hubstat.c,v 1.3 1997/10/07 06:16:35 chatz Exp $"

/* #if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4) || defined(IRIX6_5) */
#if defined(IRIX6_2) || defined(IRIX6_3)

#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

/*ARGSUSED*/
void
hub_init(int reset)
{
}

void
hub_fetch_setup(void)
{
}

static pmDesc dummy = {
    0, PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0}
};

int
hub_desc(pmID pmid, pmDesc *desc)
{
    dummy.pmid = pmid;
    *desc = dummy;
    return 0;
}

/*ARGSUSED*/
int
hub_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    vpcb->p_nval = PM_ERR_APPVERSION;
    return 0;
}

#else /* 6.4 or later */

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <invent.h>
#include <paths.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./numa_node.h"

static hubstat_t	hinfo;

static pmMeta	meta[] = {
/* hw.hub.ni.retry */
  { (char *)&hinfo.hs_ni_retry_errors, { PMID(1,44,1), PM_TYPE_U64, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* hw.hub.ni.sn_errors */
  { (char *)&hinfo.hs_ni_sn_errors, { PMID(1,44,2), PM_TYPE_U64, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* hw.hub.ni.cb_errors; */
  { (char *)&hinfo.hs_ni_cb_errors, { PMID(1,44,3), PM_TYPE_U64, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* hw.hub.ni.overflows */
  { (char *)&hinfo.hs_ni_overflows, { PMID(1,44,4), PM_TYPE_32, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* hw.hub.ii.sn_errors */
  { (char *)&hinfo.hs_ii_sn_errors, { PMID(1,44,5), PM_TYPE_U64, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* hw.hub.ii.cb_errors */
  { (char *)&hinfo.hs_ii_cb_errors, { PMID(1,44,6), PM_TYPE_U64, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* hw.hub.ii.overflows */
  { (char *)&hinfo.hs_ii_overflows, { PMID(1,44,7), PM_TYPE_U64, PM_INDOM_NODE, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* hw.hub.nasid */
  { (char *)&hinfo.hs_nasid, { PMID(1,44,8), PM_TYPE_32, PM_INDOM_NODE, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static char	       	*fetched = 0;
static int		direct_map = 1;

extern int		n_nodes;
extern int		_pm_has_nodes;
extern _pm_node		*nodes;

int
reload_hubs(void)
{
    /* walk /hw and find all hubs */
    char		buf[MAXPATHLEN];
    int			i;

    for (i = 0; i < n_nodes; i++)
	nodes[i].hubfd = -1;

    for (i = 0; i < n_nodes; i++) {
	sprintf(buf, "%s/hub/mon", nodes[i].hwgname);
	nodes[i].hubfd = open(buf, O_RDONLY);
	if (nodes[i].hubfd < 0) {
	    __pmNotifyErr(LOG_ERR,
			  "reload_hubs: unable to open hub %d \"%s\": %s\n",
			  i, buf, strerror(errno));
	}
#ifdef PCP_DEBUG
	else if (pmIrixDebug & DBG_IRIX_NODE) {
	    __pmNotifyErr(LOG_DEBUG,
			  "reload_hubs: hub %d mapped to \"%s\"\n",
			  i, buf);
	}
#endif
	
    }

    return i;
}

/* ARGSUSED */
void
hub_init(int reset)
{
    int			i;
    static int		offset_done = 0;

    (void)reload_hubs();

    fetched = (char *)realloc(fetched, n_nodes * sizeof(char));
	
    if (!offset_done) {
	for (i = 0; i < nmeta; i++) {
	    meta[i].m_desc.indom = indomtab[PM_INDOM_NODE].i_indom;
	    if (meta[i].m_offset != (char *)0)
		meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - 
							(char *)&hinfo));
	    if (direct_map && meta[i].m_desc.pmid != PMID(1,44,i+1)) {
		direct_map = 0;
		__pmNotifyErr(LOG_WARNING, 
			     "hub_init: direct map disabled @ meta[%d]", i);
	    }
	}
	offset_done=1;

	if (! _pm_has_nodes) {
	    for (i = 0; i < nmeta; i++) {
		meta[i].m_desc.type = PM_TYPE_NOSUPPORT;
	    }
	}
    }
}

int
hub_desc(pmID pmid, pmDesc *desc)
{
    int		i;

    if (direct_map) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	i = pmidp->item - 1;
	if (i < nmeta)
	    goto doit;
    }
    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
doit:
	    *desc = meta[i].m_desc;	/* struct assignment */
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

void
hub_fetch_setup(void)
{
    int i;
    for (i = 0; i < n_nodes; i++)
	fetched[i] = 0;
}

int
hub_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    int			d;
    int			sts;
    int			nval;
    pmAtomValue		av;
    void		*vp;
    hubstat_t		*hub;
    _pm_node		*np;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "hub_fetch: direct mapping failed for %s (!= %s)\n",
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
	    if (fetched[d] == 0 && np->hubfd >= 0) {
		if ((sts = ioctl(np->hubfd, SN0DRV_GET_HUBINFO, &(np->hub))) < 0)
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
	
	hub = &(np->hub);

	if (pmid != PMID(1,44,8)) {
	    vp = (void *)&((char *)hub)[(ptrdiff_t)meta[i].m_offset];
	    avcpy(&av, vp, meta[i].m_desc.type);
	}
	else {	/* hw.hub.hasid */
	    av.l = (__int32_t)(hub->hs_nasid);
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
