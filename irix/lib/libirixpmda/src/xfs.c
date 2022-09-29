/*
 * Handle metrics for cluster xfs (32)
 *
 * Code built by newcluster Fri Feb  3 10:44:59 EST 1995
 */

#ident "$Id: xfs.c,v 1.14 1999/03/02 07:25:43 tes Exp $"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/ksa.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

static struct xfsstats xfsstats;

static pmMeta	meta[] = {
/* irix.xfs.allocx */
  { (char *)&xfsstats.xs_allocx, { PMID(1,32,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.allocb */
  { (char *)&xfsstats.xs_allocb, { PMID(1,32,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.freex */
  { (char *)&xfsstats.xs_freex, { PMID(1,32,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.freeb */
  { (char *)&xfsstats.xs_freeb, { PMID(1,32,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.abt_lookup */
  { (char *)&xfsstats.xs_abt_lookup, { PMID(1,32,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.abt_compare */
  { (char *)&xfsstats.xs_abt_compare, { PMID(1,32,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.abt_insrec */
  { (char *)&xfsstats.xs_abt_insrec, { PMID(1,32,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.abt_delrec */
  { (char *)&xfsstats.xs_abt_delrec, { PMID(1,32,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.blk_mapr */
  { (char *)&xfsstats.xs_blk_mapr, { PMID(1,32,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.blk_mapw */
  { (char *)&xfsstats.xs_blk_mapw, { PMID(1,32,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.blk_unmap */
  { (char *)&xfsstats.xs_blk_unmap, { PMID(1,32,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.add_exlist */
  { (char *)&xfsstats.xs_add_exlist, { PMID(1,32,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.del_exlist */
  { (char *)&xfsstats.xs_del_exlist, { PMID(1,32,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.look_exlist */
  { (char *)&xfsstats.xs_look_exlist, { PMID(1,32,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.cmp_exlist */
  { (char *)&xfsstats.xs_cmp_exlist, { PMID(1,32,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.bmbt_lookup */
  { (char *)&xfsstats.xs_bmbt_lookup, { PMID(1,32,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.bmbt_compare */
  { (char *)&xfsstats.xs_bmbt_compare, { PMID(1,32,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.bmbt_insrec */
  { (char *)&xfsstats.xs_bmbt_insrec, { PMID(1,32,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.bmbt_delrec */
  { (char *)&xfsstats.xs_bmbt_delrec, { PMID(1,32,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.dir_lookup */
  { (char *)&xfsstats.xs_dir_lookup, { PMID(1,32,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.dir_create */
  { (char *)&xfsstats.xs_dir_create, { PMID(1,32,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.dir_remove */
  { (char *)&xfsstats.xs_dir_remove, { PMID(1,32,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.dir_getdents */
  { (char *)&xfsstats.xs_dir_getdents, { PMID(1,32,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.trans_sync */
  { (char *)&xfsstats.xs_trans_sync, { PMID(1,32,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.trans_async */
  { (char *)&xfsstats.xs_trans_async, { PMID(1,32,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.trans_empty */
  { (char *)&xfsstats.xs_trans_empty, { PMID(1,32,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.ig_attempts */
  { (char *)&xfsstats.xs_ig_attempts, { PMID(1,32,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.ig_found */
  { (char *)&xfsstats.xs_ig_found, { PMID(1,32,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.ig_frecycle */
  { (char *)&xfsstats.xs_ig_frecycle, { PMID(1,32,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.ig_missed */
  { (char *)&xfsstats.xs_ig_missed, { PMID(1,32,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.ig_dup */
  { (char *)&xfsstats.xs_ig_dup, { PMID(1,32,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.ig_reclaims */
  { (char *)&xfsstats.xs_ig_reclaims, { PMID(1,32,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.ig_attrchg */
  { (char *)&xfsstats.xs_ig_attrchg, { PMID(1,32,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.log_writes */
  { (char *)&xfsstats.xs_log_writes, { PMID(1,32,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.log_blocks */
  { (char *)&xfsstats.xs_log_blocks, { PMID(1,32,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.xfs.log_noiclogs */
  { (char *)&xfsstats.xs_log_noiclogs, { PMID(1,32,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.xfsd_bufs */
  { (char *)&xfsstats.xs_xfsd_bufs, { PMID(1,32,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.xstrat_bytes */
  { (char *)&xfsstats.xs_xstrat_bytes, { PMID(1,32,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.xfs.xstrat_quick */
  { (char *)&xfsstats.xs_xstrat_quick, { PMID(1,32,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.xstrat_split */
  { (char *)&xfsstats.xs_xstrat_split, { PMID(1,32,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.write_calls */
  { (char *)&xfsstats.xs_write_calls, { PMID(1,32,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.write_bytes */
  { (char *)&xfsstats.xs_write_bytes, { PMID(1,32,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.xfs.write_bufs */
  { (char *)&xfsstats.xs_write_bufs, { PMID(1,32,43), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.read_calls */
  { (char *)&xfsstats.xs_read_calls, { PMID(1,32,44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.read_bytes */
  { (char *)&xfsstats.xs_read_bytes, { PMID(1,32,45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.xfs.read_bufs */
  { (char *)&xfsstats.xs_read_bufs, { PMID(1,32,46), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.attr_get */
  { (char *)&xfsstats.xs_attr_get, { PMID(1,32,47), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.attr_set */
  { (char *)&xfsstats.xs_attr_set, { PMID(1,32,48), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.attr_remove */
  { (char *)&xfsstats.xs_attr_remove, { PMID(1,32,49), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.attr_list */
  { (char *)&xfsstats.xs_attr_list, { PMID(1,32,50), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#if BEFORE_IRIX6_5
/* irix.xfs.log_force */
  { (char *)0, { PMID(1,32,51), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.log_force_sleep */
  { (char *)0, { PMID(1,32,52), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.sleep_logspace */
  { (char *)0, { PMID(1,32,53), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.try_logspace */
  { (char *)0, { PMID(1,32,54), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.tail_push.npush */
  { (char *)0, { PMID(1,32,55), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.tail_push.flush */
  { (char *)0, { PMID(1,32,56), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.tail_push.flushing */
  { (char *)0, { PMID(1,32,57), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.tail_push.locked */
  { (char *)0, { PMID(1,32,58), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.tail_push.pinned */
  { (char *)0, { PMID(1,32,59), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.tail_push.pushbuf */
  { (char *)0, { PMID(1,32,60), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.tail_push.restarts */
  { (char *)0, { PMID(1,32,61), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.tail_push.success */
  { (char *)0, { PMID(1,32,62), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,1,0,0,0} } },
#else
/* irix.xfs.log_force */
  { (char *)&xfsstats.xs_log_force, { PMID(1,32,51), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.log_force_sleep */
  { (char *)&xfsstats.xs_log_force_sleep, { PMID(1,32,52), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.sleep_logspace */
  { (char *)&xfsstats.xs_sleep_logspace, { PMID(1,32,53), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.try_logspace */
  { (char *)&xfsstats.xs_try_logspace, { PMID(1,32,54), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.tail_push.npush */
  { (char *)&xfsstats.xs_push_ail, { PMID(1,32,55), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.tail_push.flush */
  { (char *)&xfsstats.xs_push_ail_flush, { PMID(1,32,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.tail_push.flushing */
  { (char *)&xfsstats.xs_push_ail_flushing, { PMID(1,32,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.tail_push.locked */
  { (char *)&xfsstats.xs_push_ail_locked, { PMID(1,32,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.tail_push.pinned */
  { (char *)&xfsstats.xs_push_ail_pinned, { PMID(1,32,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.tail_push.pushbuf */
  { (char *)&xfsstats.xs_push_ail_pushbuf, { PMID(1,32,60), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.tail_push.restarts */
  { (char *)&xfsstats.xs_push_ail_restarts, { PMID(1,32,61), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.tail_push.success */
  { (char *)&xfsstats.xs_push_ail_success, { PMID(1,32,62), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
#if defined(IRIX5_3) || defined(IRIX6_2)
/* irix.xfs.quota.cachehits */
  { (char *)0, { PMID(1,32,63), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.quota.cachemisses */
  { (char *)0, { PMID(1,32,64), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.quota.inact_reclaims */
  { (char *)0, { PMID(1,32,65), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.quota.reclaim_misses */
  { (char *)0, { PMID(1,32,66), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.quota.reclaims */
  { (char *)0, { PMID(1,32,67), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.quota.shake_reclaims */
  { (char *)0, { PMID(1,32,68), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.quota.dquot_dups */
  { (char *)0, { PMID(1,32,69), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.quota.wants */
  { (char *)0, { PMID(1,32,70), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
#else
/* irix.xfs.quota.cachehits */
  { (char *)&xfsstats.xs_qm_dqcachehits, { PMID(1,32,63), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.quota.cachemisses */
  { (char *)&xfsstats.xs_qm_dqcachemisses, { PMID(1,32,64), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.quota.inact_reclaims */
  { (char *)&xfsstats.xs_qm_dqinact_reclaims, { PMID(1,32,65), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.quota.reclaim_misses */
  { (char *)&xfsstats.xs_qm_dqreclaim_misses, { PMID(1,32,66), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.quota.reclaims */
  { (char *)&xfsstats.xs_qm_dqreclaims, { PMID(1,32,67), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.quota.shake_reclaims */
  { (char *)&xfsstats.xs_qm_dqshake_reclaims, { PMID(1,32,68), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.quota.dquot_dups */
  { (char *)&xfsstats.xs_qm_dquot_dups, { PMID(1,32,69), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.quota.wants */
  { (char *)&xfsstats.xs_qm_dqwants, { PMID(1,32,70), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
#if BEFORE_IRIX6_5
/* irix.xfs.iflush_count */
  { (char *)0, { PMID(1,32,71), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.xfs.cluster_flushzero */
  { (char *)0, { PMID(1,32,72), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
#else
/* irix.xfs.iflush_count */
  { (char *)&xfsstats.xs_iflush_count, { PMID(1,32,71), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xfs.icluster_flushzero */
  { (char *)&xfsstats.xs_icluster_flushzero, { PMID(1,32,72), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif

};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	direct_map = 1;
static int	fetched;

void xfs_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&xfsstats));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,32,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, "xfs_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void xfs_fetch_setup(void)
{
    fetched = 0;
}

int xfs_desc(pmID pmid, pmDesc *desc)
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
int xfs_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    pmAtomValue	av;
    void	*vp;

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
		if (sysmp(MP_SAGET, MPSA_XFSSTATS, &xfsstats, sizeof(xfsstats)) < 0) {
		    __pmNotifyErr(LOG_WARNING, "xfs_fetch: %s", pmErrStr(-errno));
		    return -errno;
		}
		fetched = 1;
	    }
	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    vp = (void *)&((char *)&xfsstats)[(ptrdiff_t)meta[i].m_offset];
	    avcpy(&av, vp, meta[i].m_desc.type);

	    /* Do any special case value conversions */
	    if (pmid == PMID(1,32,35)) {
		/* convert 512-byte blocks to Kbytes */
		av.ul /= 2;
	    }

	    return vpcb->p_valfmt = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].m_desc.type);
	}
    }
    return PM_ERR_PMID;
}
