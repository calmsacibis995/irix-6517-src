/*
 * Handle metrics for cluster node (40)
 */

#ident "$Id: nodeinfo.c,v 1.14 1997/10/09 19:43:23 chatz Exp $"

#if defined(IRIX6_2) || defined(IRIX6_3)
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

/*ARGSUSED*/
void
node_init(int reset)
{
}

void
node_fetch_setup(void)
{
}

static pmDesc dummy = {
    0, PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0}
};

int
node_desc(pmID pmid, pmDesc *desc)
{
    dummy.pmid = pmid;
    *desc = dummy;
    return 0;
}

/*ARGSUSED*/
int
node_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    vpcb->p_nval = PM_ERR_APPVERSION;
    return 0;
}

#else /* 6.4 or later */

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include <errno.h>
#include <invent.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./numa_node.h"

static struct nodeinfo	ninfo;

static pmMeta	meta[] = {
/* irix.node.physmem */
  { (char *)&ninfo.totalmem, { PMID(1,40,1), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.node.free.total */
  { (char *)&ninfo.freemem, { PMID(1,40,2), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.node.free.pages_64k */
  { (char *)&ninfo.num64kpages, { PMID(1,40,3), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.node.free.pages_256k */
  { (char *)&ninfo.num256kpages, { PMID(1,40,4), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.node.free.pages_1m */
  { (char *)&ninfo.num1mpages, { PMID(1,40,5), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.node.free.pages_4m */
  { (char *)&ninfo.num4mpages, { PMID(1,40,6), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.node.free.pages_16m */
  { (char *)&ninfo.num16mpages, { PMID(1,40,7), PM_TYPE_U32, PM_INDOM_NODE, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
};

int			n_nodes = 0;
int			max_nodes = 0;
_pm_node		*nodes = NULL;

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static char	       	*fetched = 0;
static int		collected = 0;
static int		direct_map = 1;
static int		node_infosz;
static nodeinfo_t	*node_info = (nodeinfo_t *)0;

int
reload_nodes(void)
{
    char	buf[MAXPATHLEN];
    char	*str = NULL;
    int		len;
    int		i;
    int		module;
    int		slot;
    _pm_node_id	id;

    if (n_nodes > 0 && nodes != NULL) {
	for (i=0; i < n_nodes; i++) {
	    if (nodes[i].hwgname != NULL) {
		free(nodes[i].extname);
		free(nodes[i].hwgname);
	    }
	    if (nodes[i].hubfd >= 0)
		close(nodes[i].hubfd);
	}
	memset(nodes, 0, max_nodes * sizeof(_pm_node));
	for (i=0; i < n_nodes; i++)
	    nodes[i].hubfd = -1;
    }

    if (node_info != (nodeinfo_t *)0) {
	free(node_info);
	node_info = NULL;
    }

    if (!_pm_has_nodes) {
	/* no Lego nodes on this platform */
	n_nodes = 0;
    }
    else {
	n_nodes = (unsigned int)sysmp(MP_NUMNODES);

	if (n_nodes <= 0) {
	    __pmNotifyErr(LOG_ERR,
			  "reload_nodeinfo: No nodes found via sysmp(): %s\n", 
			  strerror(errno));
	    n_nodes = 0;
	}
#if PCP_DEBUG
	else if (pmIrixDebug & DBG_IRIX_NODE)
	    __pmNotifyErr(LOG_DEBUG, 
			  "reload_nodeinfo: should be %d nodes\n", n_nodes);
#endif
    }

    if (n_nodes == 0)
	return 0;

    nodes = (_pm_node *)realloc(nodes, n_nodes * sizeof(_pm_node));
    if (nodes == NULL) {
	__pmNotifyErr(LOG_ERR, "reload_nodes: Unable to allocate %d bytes\n",
		      n_nodes * sizeof(_pm_node));
	goto failed;
    }

    node_infosz = (int)sysmp(MP_SASZ, MPSA_NODE_INFO);    
    node_info = calloc(n_nodes, node_infosz);

    if (node_info == NULL) {
	__pmNotifyErr(LOG_ERR, "reload_nodes: Unable to allocate %d bytes\n",
		      n_nodes * node_infosz);
	goto failed;
    }

    if (sysmp(MP_SAGET, MPSA_NODE_INFO, (char *)node_info, 
	      n_nodes * node_infosz) < 0) {
	__pmNotifyErr(LOG_ERR, 
		      "reload_nodes: sysmp(MP_SAGET, MPSA_NODE_INFO) failed: %s\n",
		      strerror(errno));
	goto failed;
    }

    for (i = 0; i < n_nodes; i++) {
	len = sizeof(buf);
	str = dev_to_devname(node_info[i].node_device, buf, &len);

	if (str == NULL || buf[0] == '\0') {
	    __pmNotifyErr(LOG_ERR,
			  "reload_nodes: Unable to find pathname for node %d\n",
			  i);
	    goto failed;

	}
	nodes[i].hwgname = strdup(buf);
	nodes[i].extname = __hwg2inst(PM_INDOM_NODE, nodes[i].hwgname);
	nodes[i].hubfd = -1;

	if (sscanf(nodes[i].extname, "node:%d.%d", &module, &slot) != 2) {
	    __pmNotifyErr(LOG_ERR, 
			  "reload_nodes: Unable to determine module/slot number for node %d = \"%s\"\n",
			  i, nodes[i].hwgname);
	    nodes[i].id = i;
	    free(nodes[i].hwgname);
	    sprintf(nodes[i].hwgname, "node%d", i);
	}
	else {
	    id.module = module;
	    id.slot = slot;
	    id.pad = 0;
	    nodes[i].id = *(int *)&(id);
	}
	nodes[i].mem = &node_info[i];

#ifdef PCP_DEBUG
	if (pmIrixDebug & DBG_IRIX_NODE)
	    __pmNotifyErr(LOG_DEBUG, 
			  "reload_nodes: %d has id = %d, extname = \"%s\" for hw = \"%s\"\n",
			  i, nodes[i].id, nodes[i].extname, nodes[i].hwgname);
#endif
    }

    return n_nodes;

 failed:

    if (nodes)
	free(nodes);
    nodes = NULL;
    if (node_info)
	free(node_info);
    node_info = NULL;
    n_nodes = 0;
    return 0;
}

/* ARGSUSED */
void
node_init(int reset)
{
    int			i;
    static int		offset_done = 0;

    fetched = (char *)realloc(fetched, n_nodes * sizeof(char));

    if (reset)
	return;

    if (!offset_done) {
	for (i = 0; i < nmeta; i++) {
	    meta[i].m_desc.indom = indomtab[PM_INDOM_NODE].i_indom;
	    meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&ninfo));
	    if (direct_map && meta[i].m_desc.pmid != PMID(1,40,i+1)) {
		direct_map = 0;
		__pmNotifyErr(LOG_WARNING, "node_init: direct map disabled @ meta[%d]", i);
	    }
	}
	offset_done = 1;
	if (! _pm_has_nodes) {
	    for (i = 0; i < nmeta; i++) {
		meta[i].m_desc.type = PM_TYPE_NOSUPPORT;
	    }
	}
    }
}

int
node_desc(pmID pmid, pmDesc *desc)
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
node_fetch_setup(void)
{
    int i;
    for (i = 0; i < n_nodes; i++)
	fetched[i] = 0;
    collected = 0;
}

/*ARGSUSED*/
int
node_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    int			d;
    int			nval;
    int			sts;
    pmAtomValue		av;
    void		*vp;
    _pm_node		*np;

    if (direct_map) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	i = pmidp->item - 1;
	if (i < nmeta)
	    goto doit;
    }
    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
doit:
	    vpcb->p_nval = 0;
	    if (meta[i].m_desc.type == PM_TYPE_NOSUPPORT || n_nodes == 0)
		return 0;
	    if (collected == 0) {
		if (sysmp(MP_SAGET, MPSA_NODE_INFO, (char *)node_info, 
			  n_nodes * node_infosz) < 0) {
		    __pmNotifyErr(LOG_ERR, 
				 "node_fetch: sysmp(MP_SAGET, MPSA_NODE_INFO) failed: %s\n",
				 strerror(errno));
		    return -errno;
		}
		collected = 1;
	    }

	    for (d = 0; d < n_nodes; d++) {
		nodes[d].mem = &node_info[d];
		np = &nodes[d];
		if (__pmInProfile(indomtab[PM_INDOM_NODE].i_indom, profp, NODE_ID(np))) {
		    fetched[d]++;
		    vpcb->p_nval++;
		}
	    }

	    sizepool(vpcb);
	    nval = 0;

	    for (d=0; d < n_nodes; d++) {

		if (fetched[d] == 0) {
		    /* not in profile */
		    continue;
		}

		np = &nodes[d];
		vp = (void *)&((char *)(np->mem))[(ptrdiff_t)meta[i].m_offset];
		avcpy(&av, vp, meta[i].m_desc.type);

		if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
		    break;
		vpcb->p_vp[nval++].inst = NODE_ID(np);
	    }
	    vpcb->p_valfmt = sts;
	    return sts;
	}
    }
    return PM_ERR_PMID;
}

#endif /* defined(IRIX6_2) || defined(IRIX6_3) */
