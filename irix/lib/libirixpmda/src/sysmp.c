/*
 * Handle metrics for cluster sysmp (18)
 *
 * Code built by newcluster on Mon Jun  6 17:58:47 EST 1994
 */

#ident "$Id: sysmp.c,v 1.26 1997/10/13 01:55:10 kenmcd Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <utmp.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
#include "./kmemread.h"
#else
#include <sys/sysget.h>
#endif

static pmMeta		meta[] = {
/* irix.mem.freemem */
  { 0, { PMID(1,18,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* hinv.ncpu */
  { 0, { PMID(1,18,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* irix.kernel.all.load */
  { 0, { PMID(1,18,3), PM_TYPE_FLOAT, PM_INDOM_LOADAV, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
/* irix.kernel.all.users */
  { 0, { PMID(1,18,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
/* irix.mem.availsmem */
  { 0, { PMID(1,18,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.availrmem */
  { 0, { PMID(1,18,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.bufmem */
  { 0, { PMID(1,18,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.physmem */
  { 0, { PMID(1,18,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.dchunkpages */
  { 0, { PMID(1,18,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.pmapmem */
  { 0, { PMID(1,18,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.strmem */
  { 0, { PMID(1,18,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.chunkpages */
  { 0, { PMID(1,18,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.dpages */
  { 0, { PMID(1,18,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
#if !defined(IRIX6_2) && !defined(IRIX6_3)
/* irix.mem.emptymem */
  { 0, { PMID(1,18,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
#else
/* irix.mem.emptymem */
  { 0, { PMID(1,18,14), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
#endif
/* irix.mem.util.kernel */
  { 0, { PMID(1,18,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.util.fs_ctl */
  { 0, { PMID(1,18,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.util.fs_dirty */
  { 0, { PMID(1,18,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.util.fs_clean */
  { 0, { PMID(1,18,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.util.free */
  { 0, { PMID(1,18,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.mem.util.user */
  { 0, { PMID(1,18,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	rm_fetched;
static int	direct_map = 1;
extern int	_pm_pagesz; /* defined and initialized in irix.c */

void sysmp_init(int reset)
{
    int	i;
    int	indomtag;		/* Constant from descr in form */

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	indomtag = meta[i].m_desc.indom;
	if (direct_map && meta[i].m_desc.pmid != PMID(1,18,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "sysmp_init: direct map disabled @ meta[%d]", i);
	}
	if (indomtag == PM_INDOM_NULL)
	    continue;
	if (indomtag < 0 || indomtag >= PM_INDOM_NEXT) {
	    __pmNotifyErr(LOG_ERR, "sysmp_init: bad instance domain (%d) for metric %s\n",
			 indomtag, pmIDStr(meta[i].m_desc.pmid));
	    continue;
	}
	/* Replace tag with it's indom */
	meta[i].m_desc.indom = indomtab[indomtag].i_indom;
    }
}

void sysmp_fetch_setup(void)
{
    rm_fetched = 0;
}

int sysmp_desc(pmID pmid, pmDesc *desc)
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

static int
fetch_loadav(__pmProfile *profp, pmVPCB *vpcb, int type)
{
    int			i;
    int			s;
    int			nval;
    static int		inst[3] = {1, 5, 15};
    __int32_t		load[sizeof(inst)/sizeof(inst[0])];
    pmAtomValue		av;

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
    static off_t	addr = -1;
#else
    sgt_cookie_t        ck;
#endif

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
    if (addr == -1) {
	if ((addr = sysmp(MP_KERNADDR, MPKA_AVENRUN)) == -1)
	    return -errno;
    }
    if ((s = kmemread(addr, &load, sizeof(load))) < 0)
	return s;
#else /* IRIX6_5 and later */

    /* Calculate an average from all cells (if there are more than one) */

    SGT_COOKIE_INIT(&ck);
    SGT_COOKIE_SET_KSYM(&ck, KSYM_AVENRUN);
    if (sysget(SGT_KSYM, (char *)load, sizeof(load), SGT_READ | SGT_SUM, &ck) < 0) {
	return -errno;
    }

    if (_pm_numcells > 1) {
	for (i = 0; i < 3; i++) {
	    load[i] /= _pm_numcells;
	}
    }
#endif

    nval = 0;
    for (i = 0; i < 3; i++) {
	if (__pmInProfile(indomtab[PM_INDOM_LOADAV].i_indom, profp, inst[i])) {
	    vpcb->p_vp[nval].inst = inst[i];
	    av.f = load[i] / 1024.0;
	    if ((s = __pmStuffValue(&av, 0, &vpcb->p_vp[nval++], type)) < 0)
		break;
	}
    }

    vpcb->p_nval = nval;	/* pool is at least 16, no check needed */
    return vpcb->p_valfmt = s;	/* Use last valfmt returned by __pmStuffValue */
}

int
sysmp_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int				i;
    int				sts;
    static int			ncpu = -1;
    pmAtomValue			av;
    struct utmp			*utmp;
    static struct rminfo	rminfo;
    int				nusers;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "sysmp_fetch: direct mapping failed for %s (!= %s)\n",
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

    if ((pmid == PMID(1,18,1) || pmid >= PMID(1,18,5)) && !rm_fetched) {
	if (sysmp(MP_SAGET, MPSA_RMINFO, &rminfo, sizeof(rminfo)))
	    return -errno;
	rm_fetched = 1;
    }
    
    switch (pmid) {
    case PMID(1,18,1):			/* irix.mem.freemem */
	av.ul = rminfo.freemem * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,2):			/* hinv.ncpu */
	if (ncpu == -1) {
	    if ((ncpu = (int)sysmp(MP_NPROCS)) == -1)
		return -errno;
	}
	av.ul = ncpu;
	break;

    case PMID(1,18,3):			/* irix.kernel.all.load */
	return fetch_loadav(profp, vpcb, meta[i].m_desc.type);

    case PMID(1,18,4):			/* irix.kernel.all.users */
	nusers = 0;
	setutent();
	while ((utmp = getutent()) != (struct utmp *)0) {
	    if (utmp->ut_type == USER_PROCESS)
		nusers++;
	}
	av.ul = nusers;
	break;

    case PMID(1,18,5):			/* irix.mem.availsmem */
	av.ul = rminfo.availsmem * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,6):			/* irix.mem.availrmem */
	av.ul = rminfo.availrmem * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,7):			/* irix.mem.bufmem */
	av.ul = rminfo.bufmem * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,8):			/* irix.mem.physmem */
	av.ul = rminfo.physmem * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,9):			/* irix.mem.dchunkpages */
	av.ul = rminfo.dchunkpages * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,10):			/* irix.mem.pmapmem */
	av.ul = rminfo.pmapmem * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,11):			/* irix.mem.strmem */
	av.ul = rminfo.strmem * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,12):			/* irix.mem.chunkpages */
	av.ul = rminfo.chunkpages * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,13):			/* irix.mem.dpages */
	av.ul = rminfo.dpages * (_pm_pagesz / 1024);
	break;

#if !defined(IRIX6_2) && !defined(IRIX6_3)
    case PMID(1,18,14):			/* irix.mem.emptymem */
	av.ul = rminfo.emptymem * (_pm_pagesz / 1024);
	break;
#endif

    case PMID(1,18,15):			/* irix.mem.util.kernel */
    	if ((rminfo.availrmem + rminfo.bufmem) > rminfo.physmem) {
	    vpcb->p_nval = PM_ERR_VALUE;
	    return 0;
	}
    	av.ul = (rminfo.physmem - (rminfo.availrmem + rminfo.bufmem))
		* (_pm_pagesz / 1024);
	break;

    case PMID(1,18,16):			/* irix.mem.util.fs_ctl */
    	av.ul = rminfo.bufmem * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,17):			/* irix.mem.util.fs_dirty */
    	av.ul = (rminfo.dchunkpages + rminfo.dpages) * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,18):			/* irix.mem.util.fs_clean */
    	if (rminfo.dchunkpages > rminfo.chunkpages) {
	    vpcb->p_nval = PM_ERR_VALUE;
	    return 0;
	}
    	av.ul = (rminfo.chunkpages - rminfo.dchunkpages) * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,19):			/* irix.mem.util.free */
    	av.ul = rminfo.freemem * (_pm_pagesz / 1024);
	break;

    case PMID(1,18,20):			/* irix.mem.util.user */
    	if ((rminfo.chunkpages + rminfo.dpages + rminfo.freemem) >
	    rminfo.availrmem) {
	    vpcb->p_nval = PM_ERR_VALUE;
	    return 0;
	}
    	av.ul = (rminfo.availrmem - 
		 (rminfo.chunkpages + rminfo.dpages + rminfo.freemem))
		* (_pm_pagesz / 1024);
	break;

    default:
	__pmNotifyErr(LOG_WARNING, 
		     "sysmp_fetch: no case for PMID %s\n", pmIDStr(pmid));
	vpcb->p_nval = 0;
	return PM_ERR_PMID;
    }
    
    vpcb->p_nval = 1;
    vpcb->p_vp[0].inst = PM_IN_NULL;
    sts = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].m_desc.type);
    if (sts >= 0)
	vpcb->p_valfmt = sts;
    return sts;
}
