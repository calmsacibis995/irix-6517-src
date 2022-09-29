/*
 * Handle metrics for cluster getblkstats (22)
 *
 * Code built by newcluster on Thu Jun  9 15:22:59 EST 1994
 */

#ident "$Id: getblkstats.c,v 1.12 1998/06/15 06:14:14 tes Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/ksa.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"


static struct getblkstats	getblkstats;

static pmMeta		meta[] = {
/* irix.resource.buffer_cache.getblks */
  { (char *)&getblkstats.getblks, { PMID(1,22,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getblockmiss */
  { (char *)&getblkstats.getblockmiss, { PMID(1,22,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfound */
  { (char *)&getblkstats.getfound, { PMID(1,22,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getbchg */
  { (char *)&getblkstats.getbchg, { PMID(1,22,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getloops */
  { (char *)&getblkstats.getloops, { PMID(1,22,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfree */
  { (char *)&getblkstats.getfree, { PMID(1,22,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfreeempty */
  { (char *)&getblkstats.getfreeempty, { PMID(1,22,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfreehmiss */
  { (char *)&getblkstats.getfreehmiss, { PMID(1,22,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfreehmissx */
  { (char *)&getblkstats.getfreehmissx, { PMID(1,22,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfreealllck */
  { (char *)&getblkstats.getfreealllck, { PMID(1,22,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfreedelwri */
  { (char *)&getblkstats.getfreedelwri, { PMID(1,22,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.flush */
  { (char *)&getblkstats.flush, { PMID(1,22,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.flushloops */
  { (char *)&getblkstats.flushloops, { PMID(1,22,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfreeref */
  { (char *)&getblkstats.getfreeref, { PMID(1,22,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfreerelse */
  { (char *)&getblkstats.getfreerelse, { PMID(1,22,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getoverlap */
  { (char *)&getblkstats.getoverlap, { PMID(1,22,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.clusters */
  { (char *)&getblkstats.clusters, { PMID(1,22,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.clustered */
  { (char *)&getblkstats.clustered, { PMID(1,22,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfrag */
  { (char *)&getblkstats.getfrag, { PMID(1,22,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getpatch */
  { (char *)&getblkstats.getpatch, { PMID(1,22,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.trimmed */
  { (char *)&getblkstats.trimmed, { PMID(1,22,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.inserts */
  { (char *)&getblkstats.inserts, { PMID(1,22,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.irotates */
  { (char *)&getblkstats.irotates, { PMID(1,22,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.deletes */
  { (char *)&getblkstats.deletes, { PMID(1,22,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.drotates */
  { (char *)&getblkstats.drotates, { PMID(1,22,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.decomms */
  { (char *)&getblkstats.decomms, { PMID(1,22,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.flush_decomms */
  { (char *)&getblkstats.flush_decomms, { PMID(1,22,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.delrsv */
  { (char *)&getblkstats.delrsv, { PMID(1,22,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.delrsvclean */
  { (char *)&getblkstats.delrsvclean, { PMID(1,22,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.delrsvdirty */
  { (char *)&getblkstats.delrsvdirty, { PMID(1,22,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.delrsvfree */
  { (char *)&getblkstats.delrsvfree, { PMID(1,22,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.delrsvwait */
  { (char *)&getblkstats.delrsvwait, { PMID(1,22,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#if BEFORE_IRIX6_5
/* irix.resource.buffer_cache.sync_commits */
  { (char *)0, { PMID(1,22,33), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.resource.buffer_cache.commits */
  { (char *)0, { PMID(1,22,34), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.resource.buffer_cache.getfreecommit */
  { (char *)0, { PMID(1,22,35), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.resource.buffer_cache.inactive */
  { (char *)0, { PMID(1,22,36), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.resource.buffer_cache.active */
  { (char *)0, { PMID(1,22,37), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.resource.buffer_cache.force */
  { (char *)0, { PMID(1,22,38), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
#else
/* irix.resource.buffer_cache.sync_commits */
  { (char *)&getblkstats.sync_commits, { PMID(1,22,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.commits */
  { (char *)&getblkstats.commits, { PMID(1,22,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.getfreecommit */
  { (char *)&getblkstats.getfreecommit, { PMID(1,22,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.inactive */
  { (char *)&getblkstats.inactive, { PMID(1,22,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.active */
  { (char *)&getblkstats.active, { PMID(1,22,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.buffer_cache.force */
  { (char *)&getblkstats.force, { PMID(1,22,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	fetched;
static int 	direct_map = 1;

void getblkstats_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&getblkstats));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,22,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, "getblkstats_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void getblkstats_fetch_setup(void)
{
    fetched = 0;
}

int getblkstats_desc(pmID pmid, pmDesc *desc)
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
int getblkstats_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
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
	    if (meta[i].m_desc.type == PM_TYPE_NOSUPPORT) {
		vpcb->p_nval = PM_ERR_APPVERSION;
		return 0;
	    }
	    if (fetched == 0) {
                if (sysmp(MP_SAGET, MPSA_BUFINFO, &getblkstats, sizeof(struct getblkstats)) < 0) {
		    __pmNotifyErr(LOG_WARNING, "getblkstats_fetch: %s", pmErrStr(-errno));
		    return -errno;
		}
		fetched = 1;
	    }
	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    avp = (pmAtomValue *)&((char *)&getblkstats)[(ptrdiff_t)meta[i].m_offset];
	    return vpcb->p_valfmt = __pmStuffValue(avp, 0, vpcb->p_vp, meta[i].m_desc.type);
	}
    }
    return PM_ERR_PMID;
}
