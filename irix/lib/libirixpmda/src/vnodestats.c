/*
 * Handle metrics for cluster vnodestats (23)
 *
 * Code built by newcluster on Thu Jun  9 15:23:12 EST 1994
 */

#ident "$Id: vnodestats.c,v 1.19 1997/07/23 04:23:16 chatz Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/ksa.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
#include "./kmemread.h"
#else
#include <sys/sysget.h>
#endif

static struct vnodestats	vnodestats;

#if (_MIPS_SZLONG == 64)
static __uint64_t		vn_vnumber;
#else
static __uint32_t		vn_vnumber;
#endif

static __int32_t		vn_epoch, vn_nfree;

static pmMeta		meta[] = {
/* irix.resource.vnodes.vnodes */
  { (char *)&vnodestats, { PMID(1,23,1), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.resource.vnodes.extant */
  { (char *)&vnodestats.vn_extant, { PMID(1,23,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.active */
  { (char *)&vnodestats.vn_active, { PMID(1,23,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.alloc */
  { (char *)&vnodestats.vn_alloc, { PMID(1,23,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.aheap */
  { (char *)&vnodestats.vn_aheap, { PMID(1,23,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.afree */
  { (char *)&vnodestats.vn_afree, { PMID(1,23,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.afreeloops */
  { (char *)&vnodestats.vn_afreeloops, { PMID(1,23,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.get */
  { (char *)&vnodestats.vn_get, { PMID(1,23,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.gchg */
  { (char *)&vnodestats.vn_gchg, { PMID(1,23,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.gfree */
  { (char *)&vnodestats.vn_gfree, { PMID(1,23,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.rele */
  { (char *)&vnodestats.vn_rele, { PMID(1,23,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.reclaim */
  { (char *)&vnodestats.vn_reclaim, { PMID(1,23,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.destroy */
  { (char *)&vnodestats.vn_destroy, { PMID(1,23,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.vnodes.afreemiss */
  { (char *)&vnodestats.vn_afreemiss, { PMID(1,23,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } }
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static int		direct_map = 1;

void vnodestats_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&vnodestats));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,23,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "vnodestats_init: direct map disabled @ meta[%d]", i);
	}	
    }
}

void vnodestats_fetch_setup(void)
{
    fetched = 0;
}

int vnodestats_desc(pmID pmid, pmDesc *desc)
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
int vnodestats_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		sts;
    pmAtomValue	av;
    pmAtomValue	*avp;

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
    __psunsigned_t	offset;
#else
    sgt_cookie_t ck;
#endif

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "vnodestats_fetch: direct mapping failed for %s (!= %s)\n",
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
	if (sysmp(MP_SAGET, MPSA_VOPINFO, &vnodestats, sizeof(struct vnodestats)) < 0) {
	    __pmNotifyErr(LOG_WARNING, "vnodestats_fetch: %s", pmErrStr(-errno));
	    return -errno;
	}

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
	offset = (__psunsigned_t)kernsymb[KS_VN_VNUMBER].n_value;
	sts = kmemread(offset, &vn_vnumber, sizeof(vn_vnumber));
#else
	SGT_COOKIE_INIT(&ck);
	SGT_COOKIE_SET_KSYM(&ck, KSYM_VN_VNUMBER);
	sts = sysget(SGT_KSYM, (char *)&vn_vnumber, sizeof(vn_vnumber),
		SGT_READ, &ck);
#endif
	if (sts != sizeof(vn_vnumber)) {
	    __pmNotifyErr(LOG_WARNING, "vnodestats.c: read vn_vnumber: %s", pmErrStr(-errno));
	    return -errno;
	}
#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
	offset = (__psunsigned_t)kernsymb[KS_VN_EPOCH].n_value;
	sts = kmemread(offset, &vn_epoch, sizeof(vn_epoch));
#else
	SGT_COOKIE_INIT(&ck);
	SGT_COOKIE_SET_KSYM(&ck, KSYM_VN_EPOCH);
	sts = sysget(SGT_KSYM, (char *)&vn_epoch, sizeof(vn_epoch),
		SGT_READ, &ck);
#endif
	if (sts != sizeof(vn_epoch)) {
	    __pmNotifyErr(LOG_WARNING, "vnodestats.c: read vn_epoch: %s", pmErrStr(-errno));
	    return -errno;
	}
#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
	offset = (__psunsigned_t)kernsymb[KS_VN_NFREE].n_value;
	sts = kmemread(offset, &vn_nfree, sizeof(vn_nfree));
#else
	SGT_COOKIE_INIT(&ck);
	SGT_COOKIE_SET_KSYM(&ck, KSYM_VN_NFREE);
	sts = sysget(SGT_KSYM, (char *)&vn_nfree, sizeof(vn_nfree),
		SGT_READ, &ck);
#endif
	if (sts != sizeof(vn_nfree)) {
	    __pmNotifyErr(LOG_WARNING, "vnodestats.c: read vn_nfree: %s", pmErrStr(-errno));
	    return -errno;
	}
	fetched = 1;
    }
    vpcb->p_nval = 1;
    vpcb->p_vp[0].inst = PM_IN_NULL;
    switch (pmid) {
	/* special case ones that are _not_ in vnodestats */
    case PMID(1,23,2):			/* irix.resource.vnodes.extant */
	av.l = (__int32_t)(vn_vnumber - vn_epoch);
	avp = &av;
	break;
    case PMID(1,23,3):			/* irix.resource.vnodes.active */
	av.l = (__int32_t)(vn_vnumber - vn_epoch - vn_nfree);
	avp = &av;
	break;
    default:
    	avp = (pmAtomValue *)&((char *)&vnodestats)[(ptrdiff_t)meta[i].m_offset];
	break;
    }
    if ((sts = __pmStuffValue(avp, 0, vpcb->p_vp, meta[i].m_desc.type)) < 0)
	return sts;
    vpcb->p_valfmt = sts;
    return 0;
}
