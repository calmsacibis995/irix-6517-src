/*
 * Handle metrics for cluster disk (80)
 *
 * Code built by newcluster on Fri Jun  3 10:09:19 EST 1994
 */

#ident "$Id: disk.c,v 1.21 1999/08/26 07:31:48 tes Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./spindle.h"
#include "./xpc.h"

static pmMeta		meta[] = {
/* irix.disk.dev.read */
  { 0, { PMID(1,80,1), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.dev.write */
  { 0, { PMID(1,80,2), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.dev.active */
  { 0, { PMID(1,80,3), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.disk.dev.blktotal */
  { 0, { PMID(1,80,4), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.dev.blkread */
  { 0, { PMID(1,80,5), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.dev.blkwrite */
  { 0, { PMID(1,80,6), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.dev.total */
  { 0, { PMID(1,80,7), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.dev.response */
  { 0, { PMID(1,80,8), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/*
 * xpc metrics
 */
/* irix.xpc.disk.dev.read */
  { 0, { PMID(1,80,9), PM_TYPE_U64, PM_INDOM_DISK, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xpc.disk.dev.active */
  { 0, { PMID(1,80,10), PM_TYPE_U64, PM_INDOM_DISK, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.disk.dev.blkread */
  { 0, { PMID(1,80,11), PM_TYPE_U64, PM_INDOM_DISK, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xpc.disk.dev.response */
  { 0, { PMID(1,80,12), PM_TYPE_U64, PM_INDOM_DISK, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/*
 * hinv.map metrics
 */
/* hinv.map.disk */
  { 0, { PMID(1,80,13), PM_TYPE_STRING, PM_INDOM_DISK, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
 /* hinv.disk_sn */
  { 0, { PMID(1,80,14), PM_TYPE_NOSUPPORT, 0, 0, {0,0,0,0,0,0} } },

/*
 * Export as KBytes
 */
/* irix.disk.dev.bytes */
  { 0, { PMID(1,80,15), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.disk.dev.read_bytes */
  { 0, { PMID(1,80,16), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.disk.dev.write_bytes */
  { 0, { PMID(1,80,17), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.xpc.disk.dev.bytes */
  { 0, { PMID(1,80,18), PM_TYPE_U64, PM_INDOM_DISK, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.xpc.disk.dev.read_bytes */
  { 0, { PMID(1,80,19), PM_TYPE_U64, PM_INDOM_DISK, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.xpc.disk.dev.write_bytes */
  { 0, { PMID(1,80,20), PM_TYPE_U64, PM_INDOM_DISK, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.disk.dev.qlen */
  { 0, { PMID(1,80,21), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.dev.sum_qlen */
  { 0, { PMID(1,80,22), PM_TYPE_U32, PM_INDOM_DISK, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static int		direct_map = 1;
static int		*disk_inclusion;

spindle			*disk_stats = (spindle *)0;

void
disk_init(int reset)
{
    int		i;
    int		disk_count = indomtab[PM_INDOM_DISK].i_numinst;
    extern int	errno;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_desc.indom = indomtab[PM_INDOM_DISK].i_indom;
	if (direct_map && meta[i].m_desc.pmid != PMID(1,80,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "disk_init: direct map disabled @ meta[%d]", i);
	}
    }

    if ((disk_inclusion = (int *)malloc(disk_count * sizeof(int))) == (int *)0) {
	__pmNotifyErr(LOG_WARNING, "disk_init: malloc: %s", pmErrStr(-errno));
	nmeta = 0;
	return;
    }

    for (i=0; i < disk_count; i++)
	/* fetch stats for all drives by default */
	disk_inclusion[i] = 1;

    disk_stats = spindle_stats(disk_inclusion);

    if (disk_count <=0 || disk_stats == (spindle *)0) {
	__pmNotifyErr(LOG_WARNING, "disk_init: no disks detected\n");
	disk_count = 0;
	return;
    }
}

void
disk_fetch_setup(void)
{
    fetched = 0;
}

int
disk_desc(pmID pmid, pmDesc *desc)
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
disk_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		d;
    int		sts;
    int		nval;
    __uint64_t	*cp;
    __uint64_t	*cp2;
    pmAtomValue	av;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "disk_fetch: direct mapping failed for %s (!= %s)\n",
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

    if (meta[i].m_desc.type == PM_TYPE_NOSUPPORT) {
	vpcb->p_nval = PM_ERR_APPVERSION;
	return 0;
    }
    
    if (fetched == 0) {
	/*
	 * Iterate over all the disk drive instances
	 * and exclude those instances not included by the profile.
	 */
	for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++) {
	    disk_inclusion[d] =
		__pmInProfile(indomtab[PM_INDOM_DISK].i_indom, profp, disk_stats[d].s_id);
	}
	/* Get the stats for non-excluded drives */
	disk_stats = spindle_stats(disk_inclusion);
	fetched = 1;
    }
    vpcb->p_nval = 0;
    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++)
	if (disk_inclusion[d])
	    vpcb->p_nval++;
    sizepool(vpcb);
    
    nval = 0;
    for (d=0; d < indomtab[PM_INDOM_DISK].i_numinst; d++) {
	if (disk_inclusion[d] == 0)
	    /* no stats collected */
	    continue;

	/* special case ones? */
	switch (pmid) {
	case PMID(1,80,1):		/* irix.disk.dev.read */
	case PMID(1,80,9):		/* irix.xpc.disk.dev.read */
	    av.ul = disk_stats[d].s_ops;
	    cp = XPCincr(PMID(1,80,1), d, av.ul);
	    av.ul = disk_stats[d].s_wops;
	    cp2 = XPCincr(PMID(1,80,9), d, av.ul);
	    if (meta[i].m_desc.type == PM_TYPE_U32)
		av.ul = (__uint32_t)(*cp - *cp2);
	    else
		av.ull = *cp - *cp2;
	    break;
	case PMID(1,80,2):		/* irix.disk.dev.write */
	    av.ul = disk_stats[d].s_wops;
	    break;
	case PMID(1,80,3):		/* irix.disk.dev.active */
	case PMID(1,80,10):		/* irix.xpc.disk.dev.active */
	    av.ul = disk_stats[d].s_act;
	    cp = XPCincr(pmid, d, av.ul);
	    if (meta[i].m_desc.type == PM_TYPE_U32)
		av.ul = (__uint32_t)((1000 * (*cp)) / HZ);
	    else
		av.ull = (1000 * (*cp)) / HZ;
	    break;
	case PMID(1,80,4):		/* irix.disk.dev.blktotal */
	case PMID(1,80,15):		/* irix.disk.dev.bytes */
	case PMID(1,80,18):		/* irix.xpc.disk.dev.bytes */
	    av.ul = disk_stats[d].s_bops;
	    cp = XPCincr(PMID(1,80,4), d, av.ul);
	    if (meta[i].m_desc.units.scaleSpace == PM_SPACE_KBYTE)
		if (meta[i].m_desc.type == PM_TYPE_U32)
		    av.ul = (__uint32_t)(*cp >> 1);
	        else
		    av.ull = *cp >> 1;
	    else
		av.ul = (__uint32_t)*cp;	    
	    break;
	case PMID(1,80,5):		/* irix.disk.dev.blkread */
	case PMID(1,80,11):		/* irix.xpc.disk.dev.blkread */
	case PMID(1,80,16):		/* irix.disk.dev.read_bytes */
	case PMID(1,80,19):		/* irix.xpc.disk.dev.read_bytes */
	    av.ul = disk_stats[d].s_bops;
	    cp = XPCincr(PMID(1,80,5), d, av.ul);
	    av.ul = disk_stats[d].s_wbops;
	    cp2 = XPCincr(PMID(1,80,11), d, av.ul);
	    if (meta[i].m_desc.type == PM_TYPE_U32)
		if (meta[i].m_desc.units.scaleSpace == PM_SPACE_KBYTE)
		    av.ul = ((__uint32_t)(*cp - *cp2)) >> 1;
	        else
		    av.ul = (__uint32_t)(*cp - *cp2);
	    else if (meta[i].m_desc.units.scaleSpace == PM_SPACE_KBYTE)
		av.ull = (*cp - *cp2) >> 1;
	    else
		av.ull = *cp - *cp2;
	    break;
	case PMID(1,80,6):		/* irix.disk.dev.blkwrite */
	case PMID(1,80,17):		/* irix.disk.dev.write_bytes */
	case PMID(1,80,20):		/* irix.xpc.disk.dev.write_bytes */
	    av.ul = disk_stats[d].s_wbops;
	    cp = XPCincr(PMID(1,80,16), d, av.ul);
	    if (meta[i].m_desc.units.scaleSpace == PM_SPACE_KBYTE)
		if (meta[i].m_desc.type == PM_TYPE_U32)
		    av.ul = (__uint32_t)(*cp >> 1);
	        else
		    av.ull = *cp >> 1;
	    else
		av.ul = (__uint32_t)*cp;
	    break;
	case PMID(1,80,7):		/* irix.disk.dev.total */
	    av.ul = disk_stats[d].s_ops;
	    break;
	case PMID(1,80,8):		/* irix.disk.dev.response */
	case PMID(1,80,12):		/* irix.xpc.disk.dev.response */
	    av.ul = disk_stats[d].s_resp;
	    cp = XPCincr(pmid, d, av.ul);
	    if (meta[i].m_desc.type == PM_TYPE_U32)
		av.ul = (__uint32_t)((1000 * (*cp)) / HZ);
	    else
		av.ull = (1000 * (*cp)) / HZ;
	    break;

	case PMID(1,80,13):		/* hinv.map.disk */
	    av.cp = disk_stats[d].s_hwgname;
	    break;

	case PMID(1,80,21):		/* irix.disk.dev.qlen */
	    av.ul = disk_stats[d].s_qlen;
	    break;
	case PMID(1,80,22):		/* irix.disk.dev.sum_qlen */
	    av.ul = disk_stats[d].s_sum_qlen;
	    break;
	}
	if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
	    return sts;
	vpcb->p_vp[nval].inst = disk_stats[d].s_id;
	if (nval == 0)
	    vpcb->p_valfmt = sts;
	nval++;
    }
    return 0;
}

