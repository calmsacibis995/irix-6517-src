/*
 */

#ident "$Id: minfo.c,v 1.21 1997/10/07 06:09:41 chatz Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

extern int	_pm_pagesz;		/* defined and initialized in irix.c */


static struct minfo	minfo;

static pmMeta	meta[] = {
/* irix.mem.fault.prot.total */
  { (char *)&minfo.pfault, { PMID(1,11,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.fault.prot.cow */
  { (char *)&minfo.cw, { PMID(1,11,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.fault.prot.steal */
  { (char *)&minfo.steal, { PMID(1,11,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.fault.addr.total */
  { (char *)&minfo.vfault, { PMID(1,11,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.fault.addr.cache */
  { (char *)&minfo.cache, { PMID(1,11,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.fault.addr.demand */
  { (char *)&minfo.demand, { PMID(1,11,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.fault.addr.file */
  { (char *)&minfo.file, { PMID(1,11,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.fault.addr.swap */
  { (char *)&minfo.swap, { PMID(1,11,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.halloc */
  { (char *)&minfo.halloc, { PMID(1,11,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.heapmem */
  { (char *)&minfo.heapmem, { PMID(1,11,10), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.hfree */
  { (char *)&minfo.hfree, { PMID(1,11,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.hovhd */
  { (char *)&minfo.hovhd, { PMID(1,11,12), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.hunused */
  { (char *)&minfo.hunused, { PMID(1,11,13), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)

/* irix.mem.zfree */
  { (char *)&minfo.zfree, { PMID(1,11,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.zonemem */
  { (char *)&minfo.zonemem, { PMID(1,11,15), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.zreq */
  { (char *)&minfo.zreq, { PMID(1,11,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#else /* IRIX 6.5 and later */

/* irix.mem.zfree */
  { 0, { PMID(1,11,14), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.mem.zonemem */
  { 0, { PMID(1,11,15), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.mem.zreq */
  { 0, { PMID(1,11,16), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },

#endif

/* irix.mem.iclean */
  { (char *)&minfo.iclean, { PMID(1,11,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.freeswap */
  { (char *)&minfo.freeswap, { PMID(1,11,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.bsdnet */
  { (char *)&minfo.bsdnet, { PMID(1,11,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.tlb.flush */
  { (char *)&minfo.tlbflush, { PMID(1,11,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.tlb.idnew */
  { (char *)&minfo.tlbpids, { PMID(1,11,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.tlb.invalid */
  { (char *)&minfo.tdirt, { PMID(1,11,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.tlb.idwrap */
  { (char *)&minfo.twrap, { PMID(1,11,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.tlb.rfault */
  { (char *)&minfo.rfault, { PMID(1,11,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.tlb.sync */
  { (char *)&minfo.tlbsync, { PMID(1,11,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.tlb.tfault */
  { (char *)&minfo.tfault, { PMID(1,11,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.tlb.purge */
  { (char *)&minfo.tphys, { PMID(1,11,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.tlb.kvmwrap */
  { (char *)&minfo.tvirt, { PMID(1,11,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.paging.reclaim */
  { (char *)&minfo.freedpgs, { PMID(1,11,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.palloc */
  { (char *)&minfo.palloc, { PMID(1,11,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.unmodfl */
  { (char *)&minfo.unmodfl, { PMID(1,11,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.unmodsw */
  { (char *)&minfo.unmodsw, { PMID(1,11,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.mem.system.sptalloc */
  { (char *)&minfo.sptalloc, { PMID(1,11,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.system.sptfree */
  { (char *)&minfo.sptfree, { PMID(1,11,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.system.sptclean */
  { (char *)&minfo.sptclean, { PMID(1,11,35), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.system.sptdirty */
  { (char *)&minfo.sptdirty, { PMID(1,11,36), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.system.sptintrans */
  { (char *)&minfo.sptintrans, { PMID(1,11,37), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.system.sptaged */
  { (char *)&minfo.sptaged, { PMID(1,11,38), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.system.sptbp */
  { (char *)&minfo.sptbp, { PMID(1,11,39), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.system.sptheap */
  { (char *)&minfo.sptheap, { PMID(1,11,40), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.system.sptzone */
  { (char *)&minfo.sptzone, { PMID(1,11,41), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.system.sptpt */
  { (char *)&minfo.sptpt, { PMID(1,11,42), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	fetched;
static int 	direct_map = 1;

void minfo_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&minfo));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,11,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, "minfo_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void minfo_fetch_setup(void)
{
    fetched = 0;
}

int minfo_desc(pmID pmid, pmDesc *desc)
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
int minfo_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    pmAtomValue		av;
    void		*vp;

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
		if (sysmp(MP_SAGET, MPSA_MINFO, &minfo, sizeof(minfo)) < 0)
		    return -errno;
		fetched = 1;
	    }
	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    vp = (void *)&((char *)&minfo)[(ptrdiff_t)meta[i].m_offset];
	    avcpy(&av, vp, meta[i].m_desc.type);

	    /* special case ones? */
	    switch (pmid) {
		case PMID(1,11,18):	/* irix.mem.freeswap */
		    av.ul /= 2;		/* disk pages to Kbytes */
		    break;
		case PMID(1,11,15):	/* irix.mem.zonemem */
		case PMID(1,11,35):	/* irix.mem.system.sptclean */
		case PMID(1,11,36):	/* irix.mem.system.sptdirty */
		case PMID(1,11,37):	/* irix.mem.system.sptintrans */
		case PMID(1,11,38):	/* irix.mem.system.sptaged */
		case PMID(1,11,39):	/* irix.mem.system.sptbp */
		case PMID(1,11,40):	/* irix.mem.system.sptheap */
		case PMID(1,11,41):	/* irix.mem.system.sptzone */
		case PMID(1,11,42):	/* irix.mem.system.sptpt */
		    			/* mem pages to Kbytes */
		    av.l = (__int32_t)((av.l * (__int64_t)_pm_pagesz) + 512) / 1024;
		    break;
		case PMID(1,11,10):	/* irix.mem.heapmem */
		case PMID(1,11,12):	/* irix.mem.hovhd */
		case PMID(1,11,13):	/* irix.mem.hunused */
		case PMID(1,11,19):	/* irix.mem.bsdnet */
		    			/* bytes to Kbytes */
		    av.l = (av.l + 512) / 1024;
		    break;
		case PMID(1,11,33):	/* irix.mem.system.sptalloc */
		case PMID(1,11,34):	/* irix.mem.system.sptfree */
		    			/* mem pages to Kbytes */
		    av.ul = (__uint32_t)((av.ul * (__uint64_t)_pm_pagesz) + 512) / 1024;
		    break;
	    }

	    return vpcb->p_valfmt = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].m_desc.type);
	}
    }
    return PM_ERR_PMID;
}
