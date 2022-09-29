/*
 * Handle metrics for cluster ncstats (21)
 *
 * Code built by newcluster on Thu Jun  9 15:22:34 EST 1994
 */

#ident "$Id: ncstats.c,v 1.10 1997/07/23 04:20:57 chatz Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

#include <sys/dnlc.h>

static struct ncstats	ncstats;

static pmMeta		meta[] = {
/* irix.resource.name_cache.hits */
  { (char *)&ncstats.hits, { PMID(1,21,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.misses */
  { (char *)&ncstats.misses, { PMID(1,21,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.enters */
  { (char *)&ncstats.enters, { PMID(1,21,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.dbl_enters */
  { (char *)&ncstats.dbl_enters, { PMID(1,21,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.long_enter */
  { (char *)&ncstats.long_enter, { PMID(1,21,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.long_look */
  { (char *)&ncstats.long_look, { PMID(1,21,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.lru_empty */
  { (char *)&ncstats.lru_empty, { PMID(1,21,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.purges */
  { (char *)&ncstats.purges, { PMID(1,21,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.vfs_purges */
  { (char *)&ncstats.vfs_purges, { PMID(1,21,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.removes */
  { (char *)&ncstats.removes, { PMID(1,21,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.searches */
  { (char *)&ncstats.searches, { PMID(1,21,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.stale_hits */
  { (char *)&ncstats.stale_hits, { PMID(1,21,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.name_cache.steps */
  { (char *)&ncstats.steps, { PMID(1,21,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } }
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static int		direct_map = 1;

void
ncstats_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&ncstats));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,21,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "ncstats_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void
ncstats_fetch_setup(void)
{
    fetched = 0;
}

int
ncstats_desc(pmID pmid, pmDesc *desc)
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
int
ncstats_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
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
		     "ncstats_fetch: direct mapping failed for %s (!= %s)\n",
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
	if (sysmp(MP_SAGET, MPSA_NCSTATS, &ncstats, sizeof(struct ncstats)) < 0) {
	    __pmNotifyErr(LOG_WARNING, "ncstats_fetch: %s", pmErrStr(-errno));
	    return -errno;
	}
	fetched = 1;
    }
    vpcb->p_nval = 1;
    vpcb->p_vp[0].inst = PM_IN_NULL;
    avp = (pmAtomValue *)&((char *)&ncstats)[(ptrdiff_t)meta[i].m_offset];
    if ((sts = __pmStuffValue(avp, 0, vpcb->p_vp, meta[i].m_desc.type)) < 0)
	return sts;
    vpcb->p_valfmt = sts;
    return 0;
}
