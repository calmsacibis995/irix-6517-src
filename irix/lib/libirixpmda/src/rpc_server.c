#ident "$Id: rpc_server.c,v 1.19 1999/10/14 07:21:40 tes Exp $"

/*
 * Handle metrics for cluster rpc_server (15)
 *
 * Code built by newcluster on Mon Jun  6 12:51:08 EST 1994
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/fs/nfs_stat.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./xpc.h"

static struct rsstat	rsstat;

static pmMeta		meta[] = {
/* irix.rpc.server.badcalls */
  { (char *)&rsstat.rsbadcalls, { PMID(1,15,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.server.badlen */
  { (char *)&rsstat.rsbadlen, { PMID(1,15,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.server.calls */
  { (char *)&rsstat.rscalls, { PMID(1,15,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.server.dupage */
  { (char *)&rsstat.rsdupage, { PMID(1,15,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.rpc.server.duphits */
  { (char *)&rsstat.rsduphits, { PMID(1,15,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.server.nullrecv */
  { (char *)&rsstat.rsnullrecv, { PMID(1,15,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.server.xdrcall */
  { (char *)&rsstat.rsxdrcall, { PMID(1,15,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } }
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static int		direct_map = 1;

void rpc_server_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&rsstat));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,15,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "rpc_server_init: direct map disabled @ meta[%d]", i);
	}
    }	
}

void rpc_server_fetch_setup(void)
{
    fetched = 0;
}

int rpc_server_desc(pmID pmid, pmDesc *desc)
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

/*ARGSUSED*/
int rpc_server_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		sts;
    pmAtomValue	*avp;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "rpc_server_fetch: direct mapping failed for %s (!= %s)\n",
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
#ifdef MPSA_RSSTAT
	if (sysmp(MP_SAGET, MPSA_RSSTAT, &rsstat, sizeof(rsstat)) < 0) {
#else
	errno = PM_ERR_PMID;
	if (1) {
#endif
	    __pmNotifyErr(LOG_WARNING, "rpc_server_fetch: %s", pmErrStr(-errno));
	    return -errno;
	}
	fetched = 1;
    }

    vpcb->p_nval = 1;
    vpcb->p_vp[0].inst = PM_IN_NULL;
    avp = (pmAtomValue *)&((char *)&rsstat)[(ptrdiff_t)meta[i].m_offset];
	
    switch (pmid) {
    case PMID(1,15,4):		/* irix.rpc.server.dupage */
	/* convert from ticks to milliseconds */
	avp->ull = (__uint64_t)avp->ul * 1000 / HZ;
	break;
    default:
	break;
    }
    
    if ((sts = __pmStuffValue(avp, 0, vpcb->p_vp, meta[i].m_desc.type)) < 0)
	return sts;
    vpcb->p_valfmt = sts;
    return 0;
}

