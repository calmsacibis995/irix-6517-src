#ident "$Id: rpc_client.c,v 1.13 1997/07/23 04:21:46 chatz Exp $"

/*
 * Handle metrics for cluster rpc_client (14)
 *
 * Code built by newcluster on Fri Jun  3 12:45:17 EST 1994
 */

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/fs/nfs_stat.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

static struct rcstat	rcstat;

static pmMeta		meta[] = {
/* irix.rpc.client.badcalls */
  { (char *)&rcstat.rcbadcalls, { PMID(1,14,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.client.badxid */
  { (char *)&rcstat.rcbadxids, { PMID(1,14,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.client.calls */
  { (char *)&rcstat.rccalls, { PMID(1,14,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.client.newcred */
  { (char *)&rcstat.rcnewcreds, { PMID(1,14,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.client.retrans */
  { (char *)&rcstat.rcretrans, { PMID(1,14,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.client.timeout */
  { (char *)&rcstat.rctimeouts, { PMID(1,14,6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.client.wait */
  { (char *)&rcstat.rcwaits, { PMID(1,14,7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.rpc.client.badverfs */
  { (char *)&rcstat.rcbadverfs, { PMID(1,14,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } }
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static int		direct_map = 1;

void rpc_client_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&rcstat));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,14,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "rpc_client_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void rpc_client_fetch_setup(void)
{
    fetched = 0;
}

int rpc_client_desc(pmID pmid, pmDesc *desc)
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
int rpc_client_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
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
		     "rpc_client_fetch: direct mapping failed for %s (!= %s)\n",
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
#ifdef MPSA_RCSTAT
	if (sysmp(MP_SAGET, MPSA_RCSTAT, &rcstat, sizeof(rcstat)) < 0) {
#else
	errno = PM_ERR_PMID;
	if (1) {
#endif
	    __pmNotifyErr(LOG_WARNING, "rpc_client_fetch: %s", pmErrStr(-errno));
	    return -errno;
	}
	fetched = 1;
    }

    vpcb->p_nval = 1;
    vpcb->p_vp[0].inst = PM_IN_NULL;
    avp = (pmAtomValue *)&((char *)&rcstat)[(ptrdiff_t)meta[i].m_offset];
    if ((sts = __pmStuffValue(avp, 0, vpcb->p_vp, meta[i].m_desc.type)) < 0)
	return sts;
    vpcb->p_valfmt = sts;
    return 0;
}
