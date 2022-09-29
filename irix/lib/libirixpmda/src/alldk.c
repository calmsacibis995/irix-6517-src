/*
 * Handle metrics for cluster disk, aggregated over all disks (82)
 *
 * Code stolen from cntrl.c
 */

#ident "$Id: alldk.c,v 1.13 1998/08/13 02:57:53 tes Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./spindle.h"

static pmMeta		meta[] = {
/* irix.disk.all.total */
  { 0, { PMID(1,82,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.all.read */
  { 0, { PMID(1,82,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.all.write */
  { 0, { PMID(1,82,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.all.active */
  { 0, { PMID(1,82,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.disk.all.response */
  { 0, { PMID(1,82,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.disk.all.blktotal */
  { 0, { PMID(1,82,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.all.blkread */
  { 0, { PMID(1,82,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.all.blkwrite */
  { 0, { PMID(1,82,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.all.bytes */
  { 0, { PMID(1,82,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.disk.all.read_bytes */
  { 0, { PMID(1,82,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.disk.all.write_bytes */
  { 0, { PMID(1,82,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.disk.all.avg_disk.active */
  { 0, { PMID(1,82,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.disk.all.avg_disk.response */
  { 0, { PMID(1,82,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
#if BEFORE_IRIX6_5
/* irix.disk.all.qlen */
  { 0, { PMID(1,82,22), PM_TYPE_NOSUPPORT, 0, 0, {0,0,0,0,0,0} } },
/* irix.disk.all.sum_qlen */
  { 0, { PMID(1,82,23), PM_TYPE_NOSUPPORT, 0, 0, {0,0,0,0,0,0} } },
#else
/* irix.disk.all.qlen */
  { 0, { PMID(1,82,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.all.sum_qlen */
  { 0, { PMID(1,82,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static spindle		*disk_stats = (spindle *)0;

extern int		_pmSpindleCount;

void alldk_init(int reset)
{
    int		disk_count = indomtab[PM_INDOM_DISK].i_numinst;
    extern int	errno;

    if (reset)
	return;

    disk_stats = spindle_stats((int *)0);

    if (disk_count <= 0 || disk_stats == (spindle *)0) {
	__pmNotifyErr(LOG_WARNING, "alldk_init: no disks detected\n");
	disk_count = 0;
    }
}

void alldk_fetch_setup(void)
{
    fetched = 0;
}

int alldk_desc(pmID pmid, pmDesc *desc)
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
int alldk_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		d;
    int		sts;
    pmAtomValue	av;

    if (fetched == 0) {
	disk_stats = spindle_stats((int *)0);
	fetched = 1;
    }

    if (_pmSpindleCount == 0) {
        vpcb->p_nval = PM_ERR_VALUE;
	return 0;
    }

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {

	    if (meta[i].m_desc.type == PM_TYPE_NOSUPPORT) {
		vpcb->p_nval = PM_ERR_APPVERSION;
		return 0;
	    }

	    av.ull = 0;
	    switch (pmid) {
		case PMID(1,82,9):	/* irix.disk.all.total */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_ops;
		    break;
		case PMID(1,82,10):	/* irix.disk.all.read */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_ops - disk_stats[d].s_wops;
		    break;
		case PMID(1,82,11):	/* irix.disk.all.write */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_wops;
		    break;
		case PMID(1,82,12):	/* irix.disk.all.active */
		case PMID(1,82,20):	/* irix.disk.all.avg_disk.active */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_act;
		    av.ull = (1000 * av.ull) / HZ;
		    if (pmid == PMID(1,82,20))
			av.ull /= _pmSpindleCount;
		    break;
		case PMID(1,82,13):	/* irix.disk.all.response */
		case PMID(1,82,21):	/* irix.disk.all.avg_disk.response */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_resp;
		    av.ull = (1000 * av.ull) / HZ;
		    if (pmid == PMID(1,82,21))
			av.ull /= _pmSpindleCount;
		    break;
		case PMID(1,82,14):	/* irix.disk.all.blktotal */
		case PMID(1,82,17):	/* irix.disk.all.bytes */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_bops;
		    if (meta[i].m_desc.units.scaleSpace == PM_SPACE_KBYTE)
			av.ull >>= 1;
		    break;
		case PMID(1,82,15):	/* irix.disk.all.blkread */
		case PMID(1,82,18):	/* irix.disk.all.read_bytes */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_bops - disk_stats[d].s_wbops;
		    if (meta[i].m_desc.units.scaleSpace == PM_SPACE_KBYTE)
			av.ull >>= 1;
		    break;
		case PMID(1,82,16):	/* irix.disk.all.blkwrite */
		case PMID(1,82,19):	/* irix.disk.all.write_bytes */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_wbops;
		    if (meta[i].m_desc.units.scaleSpace == PM_SPACE_KBYTE)
			av.ull >>= 1;
		    break;
#if !BEFORE_IRIX6_5
		case PMID(1,82,22):	/* irix.disk.all.qlen */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_qlen;
		    break;
		case PMID(1,82,23):	/* irix.disk.all.sum_qlen */
		    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
			av.ull += disk_stats[d].s_sum_qlen;
		    break;
#endif
		default:
		    return PM_ERR_PMID;
	    }
	    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[0], meta[i].m_desc.type)) < 0)
		return sts;
	    vpcb->p_valfmt = sts;
	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    return 0;
	}
    }
    return PM_ERR_PMID;
}
