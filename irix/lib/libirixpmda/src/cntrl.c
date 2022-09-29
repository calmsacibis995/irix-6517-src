/*
 * Handle metrics for cluster disk, aggregated by controller (81)
 *
 * Code stolen from disk.c
 */

#ident "$Id: cntrl.c,v 1.14 1998/08/13 02:57:53 tes Exp $"

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
/* irix.disk.ctl.total */
  { 0, { PMID(1,81,9), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.ctl.read */
  { 0, { PMID(1,81,10), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.ctl.write */
  { 0, { PMID(1,81,11), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.ctl.active */
  { 0, { PMID(1,81,12), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.disk.ctl.response */
  { 0, { PMID(1,81,13), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.disk.ctl.blktotal */
  { 0, { PMID(1,81,14), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.ctl.blkread */
  { 0, { PMID(1,81,15), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.ctl.blkwrite */
  { 0, { PMID(1,81,16), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.ctl.bytes */
  { 0, { PMID(1,81,17), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.disk.ctl.read_bytes */
  { 0, { PMID(1,81,18), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.disk.ctl.write_bytes */
  { 0, { PMID(1,81,19), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.disk.ctl.avg_disk.active */
  { 0, { PMID(1,81,20), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.disk.ctl.avg_disk.response */
  { 0, { PMID(1,81,21), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* hinv.nctl */
  { 0, { PMID(1,81,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,PM_COUNT_ONE} } },
/* hinv.ctl.ndisk */
  { 0, { PMID(1,81,23), PM_TYPE_U32, PM_INDOM_CNTRL, PM_SEM_DISCRETE, {0,0,0,0,0,PM_COUNT_ONE} } },
#if BEFORE_IRIX6_5
/* irix.disk.ctl.qlen */
  { 0, { PMID(1,81,24), PM_TYPE_NOSUPPORT, 0, 0, {0,0,0,0,0,0} } },
/* irix.disk.ctl.sum_qlen */
  { 0, { PMID(1,81,25), PM_TYPE_NOSUPPORT, 0, 0, {0,0,0,0,0,0} } },
#else
/* irix.disk.ctl.qlen */
  { 0, { PMID(1,81,24), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.disk.ctl.sum_qlen */
  { 0, { PMID(1,81,25), PM_TYPE_U64, PM_INDOM_CNTRL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static int		*cntrl_inclusion;
static int		direct_map = 1;
controller		*cntrl_stats = (controller *)0;

void cntrl_init(int reset)
{
    int		i;
    int		cntrl_count = indomtab[PM_INDOM_CNTRL].i_numinst;
    extern int	errno;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	if (meta[i].m_desc.indom != PM_INDOM_NULL)
	    meta[i].m_desc.indom = indomtab[PM_INDOM_CNTRL].i_indom;
	if (direct_map && meta[i].m_desc.pmid != PMID(1,81,i+9)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "cntrl_init: direct map disabled @ meta[%d]", i);
	}
    }

    if ((cntrl_inclusion = (int *)malloc(cntrl_count * sizeof(int))) == (int *)0) {
	__pmNotifyErr(LOG_WARNING, "disk_init: malloc: %s", pmErrStr(-errno));
	nmeta = 0;
	return;
    }

    for (i=0; i < cntrl_count; i++)
	/* fetch stats for all controllers by default */
	cntrl_inclusion[i] = 1;

    cntrl_stats = controller_stats((int *)0);	/* get them all */

    if (cntrl_count && cntrl_stats == (controller *)0) {
	__pmNotifyErr(LOG_WARNING, "cntrl_init: count (%d) botch", cntrl_count);
	nmeta = 0;
	return;
    }
}

void cntrl_fetch_setup(void)
{
    fetched = 0;
}

int cntrl_desc(pmID pmid, pmDesc *desc)
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

int cntrl_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		c;
    int		sts;
    int		nval;
    pmAtomValue	av;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 9;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "cntrl_fetch: direct mapping failed for %s (!= %s)\n",
		     pmIDStr(pmid), pmIDStr(meta[i].m_desc.pmid));
	direct_map = 0;
	for (i = 0; i < nmeta; i++)
	    if (pmid == meta[i].m_desc.pmid)
		break;
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

    if (meta[i].m_desc.indom != PM_INDOM_NULL) {
	if (fetched == 0) {
	    /*
	     * Iterate over all the controller instances
	     * and exclude those instances not included by the profile.
	     */
	    for (c=0; c < indomtab[PM_INDOM_CNTRL].i_numinst; c++) {
		cntrl_inclusion[c] =
		    __pmInProfile(indomtab[PM_INDOM_CNTRL].i_indom, profp, cntrl_stats[c].c_id);
	    }
	    /* Get the stats for non-excluded controllers */
	    cntrl_stats = controller_stats(cntrl_inclusion);
	    fetched = 1;
	}
	vpcb->p_nval = 0;
	for (c=0; c < indomtab[PM_INDOM_CNTRL].i_numinst; c++)
	    if (cntrl_inclusion[c])
		vpcb->p_nval++;

	sizepool(vpcb);
	
	nval = 0;
	for (c=0; c < indomtab[PM_INDOM_CNTRL].i_numinst; c++) {
	    if (cntrl_inclusion[c] == 0)
		/* no stats collected */
		continue;

	    /* special case ones? */
	    switch (pmid) {
	    case PMID(1,81,9):		/* irix.disk.ctl.total */
		av.ull = cntrl_stats[c].c_ops;
		break;
	    case PMID(1,81,10):		/* irix.disk.ctl.read */
		av.ull = cntrl_stats[c].c_ops - cntrl_stats[c].c_wops;
		break;
	    case PMID(1,81,11):		/* irix.disk.ctl.write */
		av.ull = cntrl_stats[c].c_wops;
		break;
	    case PMID(1,81,12):		/* irix.disk.ctl.active */
		av.ull = (1000 * cntrl_stats[c].c_act) / HZ;
		break;
	    case PMID(1,81,13):		/* irix.disk.ctl.response */
		av.ull = (1000 * cntrl_stats[c].c_resp) / HZ;
		break;
	    case PMID(1,81,14):		/* irix.disk.ctl.blktotal */
		av.ull = cntrl_stats[c].c_bops;
		break;
	    case PMID(1,81,15):		/* irix.disk.ctl.blkread */
		av.ull = cntrl_stats[c].c_bops - cntrl_stats[c].c_wbops;
		break;
	    case PMID(1,81,16):		/* irix.disk.ctl.blkwrite */
		av.ull = cntrl_stats[c].c_wbops;
		break;
	    case PMID(1,81,17):		/* irix.disk.ctl.bytes */
		av.ull = cntrl_stats[c].c_bops >> 1;
		break;
	    case PMID(1,81,18):		/* irix.disk.ctl.read_bytes */
		av.ull = (cntrl_stats[c].c_bops - cntrl_stats[c].c_wbops) >> 1;
		break;
	    case PMID(1,81,19):		/* irix.disk.ctl.write_bytes */
		av.ull =  cntrl_stats[c].c_wbops >> 1;
		break;
	    case PMID(1,81,20):		/* irix.disk.ctl.avg_disk.active */
		av.ull = (1000 * cntrl_stats[c].c_act) / 
		    (HZ * cntrl_stats[c].c_ndrives);
		break;
	    case PMID(1,81,21):		/* irix.disk.ctl.avg_disk.response */
		av.ull = (1000 * cntrl_stats[c].c_resp) / 
		    (HZ * cntrl_stats[c].c_ndrives);
		break;
	    case PMID(1,81,23):		/* hinv.ctl.ndisk */
		av.ul = cntrl_stats[c].c_ndrives;
		break;
#if !BEFORE_IRIX6_5
	    case PMID(1,81,24):		/* irix.disk.ctl.qlen */
		av.ull = cntrl_stats[c].c_qlen;
		break;
	    case PMID(1,81,25):		/* irix.disk.ctl.sum_qlen */
		av.ull = cntrl_stats[c].c_sum_qlen;
		break;
#endif
	    default:
		return PM_ERR_PMID;
	    }
	    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
		return sts;
	    vpcb->p_vp[nval].inst = cntrl_stats[c].c_id;
	    if (nval == 0)
		vpcb->p_valfmt = sts;
	    nval++;
	}
    }
    else {
	if (pmid == PMID(1,81,22)) {	/* hinv.nctl */
	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    av.ul = indomtab[PM_INDOM_CNTRL].i_numinst;
	    sts = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].m_desc.type);
	    vpcb->p_valfmt = sts;
	}
	else
	    return PM_ERR_PMID;
    }
    return 0;
}

