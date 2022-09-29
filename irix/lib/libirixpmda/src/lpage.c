/*
 * Handle metrics for cluster lpage (41)
 */

#ident "$Id: lpage.c,v 1.10 1997/11/11 00:12:05 curt Exp $"

#if defined(IRIX6_2) || defined(IRIX6_3)
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

/*ARGSUSED*/
void
irixpmda_lpage_init(int reset)
{
}

void
irixpmda_lpage_fetch_setup(void)
{
}

static pmDesc dummy = {
    0, PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0}
};

int
irixpmda_lpage_desc(pmID pmid, pmDesc *desc)
{
    dummy.pmid = pmid;
    *desc = dummy;
    return 0;
}

/*ARGSUSED*/
int
irixpmda_lpage_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    vpcb->p_nval = PM_ERR_APPVERSION;
    return 0;
}
#else /* 6.4 or later */

#define _KMEMUSER
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/lpage.h>
#include <unistd.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

static lpg_stat_info_t	lpage;

/*
 * add one initialization for each element in the structure you wish
 * to export
 */
static pmMeta		meta[] = {
/* irix.mem.lpage.coalesce.scans */
  { (char *)&lpage.coalesce_atts, { PMID(1,41,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.mem.lpage.coalesce.success */
  { (char *)&lpage.coalesce_succ, { PMID(1,41,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.mem.lpage.faults */
  { (char *)&lpage.num_lpg_faults, { PMID(1,41,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.mem.lpage.allocs */
  { (char *)&lpage.num_lpg_allocs, { PMID(1,41,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.mem.lpage.downgrade */
  { (char *)&lpage.num_lpg_downgrade, { PMID(1,41,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.mem.lpage.page_splits */
  { (char *)&lpage.num_page_splits, { PMID(1,41,6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.mem.lpage.basesize */
  { 0, { PMID(1,41,7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.lpage.maxsize */
  { 0, { PMID(1,41,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.lpage.maxenabled */
  { 0, { PMID(1,41,9), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.lpage.enabled */
  { 0, { PMID(1,41,10), PM_TYPE_32, PM_INDOM_LPAGESIZE, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
#if 0
/* commented out as no clean interface available yet */
/* irix.mem.lpage.reserved */
  { 0, { PMID(1,41,11), PM_TYPE_32, PM_INDOM_LPAGESIZE, PM_SEM_DISCRETE, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.mem.lpage.coalesce.limits */
  { 0, { PMID(1,41,12), PM_TYPE_32, PM_INDOM_LPAGESIZE, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
#endif
};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	fetched;
static int	basesize;
static int	inst[6] = {16, 64, 256, 1024, 4*1024, 16*1024};

void
irixpmda_lpage_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&lpage));
	if (meta[i].m_desc.indom == PM_INDOM_LPAGESIZE)
	    meta[i].m_desc.indom = indomtab[PM_INDOM_LPAGESIZE].i_indom;
    }
}

void
irixpmda_lpage_fetch_setup(void)
{
    fetched = 0;
}

int
irixpmda_lpage_desc(pmID pmid, pmDesc *desc)
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

/*
 *	fetch_enabled - fetches values for irix.mem.lpage.enabled
 */
static int
fetch_enabled(__pmProfile *profp, pmVPCB *vpcb, int type)
{
    int			i;
    int			s;
    int			nval;
    pmAtomValue         av;

    nval = 0;
    for (i = 0; i < 6; i++) {
	if (__pmInProfile(indomtab[PM_INDOM_LPAGESIZE].i_indom, profp, inst[i])) {
	    vpcb->p_vp[nval].inst = inst[i];
	    switch (i) {
		case 0:  av.l = (lpage.enabled_16k == 1); break;
		case 1:  av.l = (lpage.enabled_64k == 1); break;
		case 2:  av.l = (lpage.enabled_256k == 1); break;
		case 3:  av.l = (lpage.enabled_1m == 1); break;
		case 4:  av.l = (lpage.enabled_4m == 1); break;
		case 5:  av.l = (lpage.enabled_16m == 1); break;
	    }
	    if ((s = __pmStuffValue(&av, 0, &vpcb->p_vp[nval++], type)) < 0)
		break;
	}
    }

    vpcb->p_nval = nval;
    return vpcb->p_valfmt = s;  /* Use last valfmt returned by __pmStuffValue */
}

#if 0
/* commented out as no clean interface available yet */
/*
 *	fetch_limits - fetches irix.mem.lpage.limits
 */
static int
fetch_limits(__pmProfile *profp, pmVPCB *vpcb, int type)
{
    int			i;
    int			s;
    int			nval;
    pmAtomValue         av;

    nval = 0;
    for (i = 0; i < 6; i++) {
	if (__pmInProfile(indomtab[PM_INDOM_LPAGESIZE].i_indom, profp, inst[i])) {
	    vpcb->p_vp[nval].inst = inst[i];
	    switch (i) {
		case 0:  av.l = 0; break;	/* not yet implemented */
		case 1:  av.l = 0; break;	/* not yet implemented */
		case 2:  av.l = 0; break;	/* not yet implemented */
		case 3:  av.l = 0; break;	/* not yet implemented */
		case 4:  av.l = 0; break;	/* not yet implemented */
		case 5:  av.l = 0; break;	/* not yet implemented */
	    }
	    if ((s = __pmStuffValue(&av, 0, &vpcb->p_vp[nval++], type)) < 0)
		break;
	}
    }

    vpcb->p_nval = nval;        /* pool is at least 16, no check needed (?) */
    return vpcb->p_valfmt = s;  /* Use last valfmt returned by __pmStuffValue */
}

/* commented out as no clean interface available yet */
/*
 *	fetch_reserved - fetches irix.mem.lpage.reserved
 */
static int
fetch_reserved(__pmProfile *profp, pmVPCB *vpcb, int type)
{
    int			i;
    int			s;
    int			nval;
    pmAtomValue         av;

    nval = 0;
    for (i = 0; i < 6; i++) {
	if (__pmInProfile(indomtab[PM_INDOM_LPAGESIZE].i_indom, profp, inst[i])) {
	    vpcb->p_vp[nval].inst = inst[i];
	    switch (i) {
		case 0:  av.l = 0; break;	/* 16k pages cannot be reserved */
		case 1:  av.l = 0; break;	/* not yet implemeted */
		case 2:  av.l = 0; break;	/* not yet implemeted */
		case 3:  av.l = 0; break;	/* not yet implemeted */
		case 4:  av.l = 0; break;	/* not yet implemeted */
		case 5:  av.l = 0; break;	/* not yet implemeted */
	    }
	    if ((s = __pmStuffValue(&av, 0, &vpcb->p_vp[nval++], type)) < 0)
		break;
	}
    }

    vpcb->p_nval = nval;        /* pool is at least 16, no check needed (?) */
    return vpcb->p_valfmt = s;  /* Use last valfmt returned by __pmStuffValue */
}
#endif

/*ARGSUSED*/
int
irixpmda_lpage_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		sts;
    int		*ip;
    pmAtomValue	atom;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    if (fetched == 0) {
		if ((sts = (int)sysmp(MP_SAGET, MPSA_LPGSTATS, &lpage, sizeof(lpage))) < 0) {
		    return -errno;
		}
		basesize = getpagesize() / 1024;
		fetched = 1;
	    }
	    if (fetched) {
		if (pmid == PMID(1,41,10))		/* irix.mem.lpage.enabled  */
		    return fetch_enabled(profp, vpcb, meta[i].m_desc.type);
#if 0
		else if (pmid == PMID(1,41,11))		/* irix.mem.lpage.reserved */
		    return fetch_reserved(profp, vpcb, meta[i].m_desc.type);
		else if (pmid == PMID(1,41,12))		/* irix.mem.lpage.coalesce.limits */
		    return fetch_limits(profp, vpcb, meta[i].m_desc.type);
#endif
		vpcb->p_nval = 1;
		vpcb->p_vp[0].inst = PM_IN_NULL;
		if (pmid == PMID(1,41,7))		/* irix.mem.lpage.basesize */
		    atom.l = basesize;
		else if (pmid == PMID(1,41,8))		/* irix.mem.lpage.maxsize */
		    atom.l = MAX_PAGE_SIZE / 1024;
		else if (pmid == PMID(1,41,9)) {	/* irix.mem.lpage.maxenabled */
		    if (lpage.enabled_16m == 1) atom.l = 16*1024;
		    else if (lpage.enabled_4m == 1) atom.l = 4*1024;
		    else if (lpage.enabled_1m == 1) atom.l = 1024;
		    else if (lpage.enabled_256k == 1) atom.l = 256;
		    else if (lpage.enabled_64k == 1) atom.l = 64;
		    else if (lpage.enabled_16k == 1) atom.l = 16;
		    else atom.l = 0;
		}
		else {					/* other irix.mem.lpage */
		    ip = (int *)&((char *)&lpage)[(ptrdiff_t)meta[i].m_offset];
		    atom.l = *ip;
		}
		if ((sts = __pmStuffValue(&atom, 0, vpcb->p_vp, meta[i].m_desc.type)) < 0)
		    return sts;
		vpcb->p_valfmt = sts;
	    }
	    else
		vpcb->p_nval = 0;
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

#endif /* defined(IRIX6_2) || defined(IRIX6_3) */
