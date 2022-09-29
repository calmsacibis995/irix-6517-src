/*
 * Handle metrics for cluster igetstats (24)
 *
 * Code built by newcluster on Thu Jun  9 15:23:24 EST 1994
 */

#ident "$Id: igetstats.c,v 1.12 1997/07/23 04:19:56 chatz Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/ksa.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

static struct igetstats	igetstats;

static pmMeta		meta[] = {
/* irix.resource.efs.attempts */
  { (char *)&igetstats.ig_attempts, { PMID(1,24,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.found */
  { (char *)&igetstats.ig_found, { PMID(1,24,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.frecycle */
  { (char *)&igetstats.ig_frecycle, { PMID(1,24,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.missed */
  { (char *)&igetstats.ig_missed, { PMID(1,24,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.dup */
  { (char *)&igetstats.ig_dup, { PMID(1,24,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.reclaims */
  { (char *)&igetstats.ig_reclaims, { PMID(1,24,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.itobp */
  { (char *)&igetstats.ig_itobp, { PMID(1,24,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.itobpf */
  { (char *)&igetstats.ig_itobpf, { PMID(1,24,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.iupdat */
  { (char *)&igetstats.ig_iupdat, { PMID(1,24,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.iupacc */
  { (char *)&igetstats.ig_iupacc, { PMID(1,24,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.iupupd */
  { (char *)&igetstats.ig_iupupd, { PMID(1,24,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.iupchg */
  { (char *)&igetstats.ig_iupchg, { PMID(1,24,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.iupmod */
  { (char *)&igetstats.ig_iupmod, { PMID(1,24,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.iupunk */
  { (char *)&igetstats.ig_iupunk, { PMID(1,24,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.iallocrd */
  { (char *)&igetstats.ig_iallocrd, { PMID(1,24,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.iallocrdf */
  { (char *)&igetstats.ig_iallocrdf, { PMID(1,24,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.ialloccoll */
  { (char *)&igetstats.ig_ialloccoll, { PMID(1,24,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.bmaprd */
  { (char *)&igetstats.ig_bmaprd, { PMID(1,24,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.bmapfbm */
  { (char *)&igetstats.ig_bmapfbm, { PMID(1,24,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.bmapfbc */
  { (char *)&igetstats.ig_bmapfbc, { PMID(1,24,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.dirupd */
  { (char *)&igetstats.ig_dirupd, { PMID(1,24,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.truncs */
  { (char *)&igetstats.ig_truncs, { PMID(1,24,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.icreat */
  { (char *)&igetstats.ig_icreat, { PMID(1,24,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.attrchg */
  { (char *)&igetstats.ig_attrchg, { PMID(1,24,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.efs.readcancel */
  { (char *)&igetstats.ig_readcancel, { PMID(1,24,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	fetched;
static int 	direct_map = 1;

void igetstats_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&igetstats));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,24,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "igetstats_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void igetstats_fetch_setup(void)
{
    fetched = 0;
}

int igetstats_desc(pmID pmid, pmDesc *desc)
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

/*ARGSUSED*/
int igetstats_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    pmAtomValue	*avp;

    if (direct_map) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	i = pmidp->item - 1;
	if (i < nmeta)
	    goto doit;
    }
    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
doit:
	    if (fetched == 0) {
                if (sysmp(MP_SAGET, MPSA_EFS, &igetstats, sizeof(struct igetstats)) < 0) {
		    __pmNotifyErr(LOG_WARNING, "igetstats_fetch: %s", pmErrStr(-errno));
		    return -errno;
		}
		fetched = 1;
	    }
	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    avp = (pmAtomValue *)&((char *)&igetstats)[(ptrdiff_t)meta[i].m_offset];
	    return vpcb->p_valfmt = __pmStuffValue(avp, 0, vpcb->p_vp, meta[i].m_desc.type);
	}
    }
    return PM_ERR_PMID;
}




