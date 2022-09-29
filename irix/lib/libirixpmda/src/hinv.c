/*
 * Handle metrics for cluster inv (20)
 *
 * Code built by newcluster on Wed Jun  8 15:54:57 EST 1994
 */

#ident "$Id: hinv.c,v 1.22 1997/10/07 06:09:31 chatz Exp $"

#define _KMEMUSER

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <syslog.h>
#include <invent.h>
#include "pmapi.h"
#include "impl.h"
#include "./cpu.h"
#include "./cluster.h"

static inventory_t	*hinv;

static pmMeta		meta[] = {
/* hinv.cpuclock */
  { NULL, { PMID(1,26,1), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_DISCRETE, {0,-1,1,0,PM_TIME_SEC,6} } },
/* hinv.dcache */
  { NULL, { PMID(1,26,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* hinv.icache */
  { NULL, { PMID(1,26,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* hinv.secondarycache */
  { NULL, { PMID(1,26,4), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_DISCRETE, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* hinv.physmem */
  { NULL, { PMID(1,26,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {1,0,0,PM_SPACE_MBYTE,0,0} } },
/* hinv.pmeminterleave */
  { NULL, { PMID(1,26,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,PM_COUNT_ONE} } },
/* hinv.ndisk */
  { NULL, { PMID(1,26,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,PM_COUNT_ONE} } },
/* hinv.nnode */
  { NULL, { PMID(1,26,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,PM_COUNT_ONE} } },
#if defined(IRIX6_2) || defined(IRIX6_3)
/* hinv.map.cpu */
  { NULL, { PMID(1,26,9), PM_TYPE_NOSUPPORT, PM_INDOM_CPU, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
#else
/* hinv.map.cpu */
  { NULL, { PMID(1,26,9), PM_TYPE_STRING, PM_INDOM_CPU, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
#endif
/* hinv.mincpuclock */
  { NULL, { PMID(1,26,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,-1,1,0,PM_TIME_SEC,6} } },
/* hinv.maxcpuclock */
  { NULL, { PMID(1,26,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,-1,1,0,PM_TIME_SEC,6} } },
/* hinv.machine */
  { NULL, { PMID(1,26,12), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* hinv.cputype */
  { NULL, { PMID(1,26,13), PM_TYPE_STRING, PM_INDOM_CPU, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* hinv.ncell */
  { NULL, { PMID(1,26,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,PM_COUNT_ONE} } },
/* hinv.pagesize */
  { NULL, { PMID(1,26,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {1,0,0,PM_SPACE_BYTE,0,0} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static char		*fetched = NULL;
static struct utsname	_uname;
static int		direct_map = 1;

extern int		n_cpus;
extern int		max_cpuclock;
extern int		min_cpuclock;
extern _pm_cpu		*cpus;
extern int		_pm_pagesz;

/*ARGSUSED*/
void
hinv_init(int reset)
{
    int	i;

    if (reset && fetched != NULL) {
	free(fetched);
	fetched = NULL;
    }

    for (i = 0; i < nmeta; i++) {
	if (meta[i].m_desc.indom == PM_INDOM_CPU)
	    meta[i].m_desc.indom = indomtab[PM_INDOM_CPU].i_indom;
	if (direct_map && meta[i].m_desc.pmid != PMID(1,26,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "hinv_init: direct map disabled @ meta[%d]", i);
	}	
    }

    uname(&_uname);

}

void
hinv_fetch_setup(void)
{
    int	i;

/*
 * delay this check to here, instead of hinv_init, as indomtab is set in
 * percpu_init which is called after hinv_init 
 */

    if (fetched == NULL)
	fetched = (char *)malloc(indomtab[PM_INDOM_CPU].i_numinst * sizeof(char));
    if (fetched == NULL) {
	__pmNotifyErr(LOG_ERR, "hinv_init: fetched malloc failed (%d bytes)\n",
		     indomtab[PM_INDOM_CPU].i_numinst * sizeof(char));
	nmeta = 0;
	return;
    }

    for (i = 0; i < indomtab[PM_INDOM_CPU].i_numinst; i++)
	fetched[i] = 0;
}

int
hinv_desc(pmID pmid, pmDesc *desc)
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
hinv_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    int			sts;
    int			n;
    int			kval;
    pmAtomValue		av;

    extern int		n_cpus;
    extern _pm_cpu	*cpus;

    extern int		_pmSpindleCount;
    extern int		n_nodes;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "hinv_fetch: direct mapping failed for %s (!= %s)\n",
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

    kval = 0;
    /* do these metrics have an instance domain? */
    if (meta[i].m_desc.indom != PM_INDOM_NULL) {
	for (n = 0; n < indomtab[PM_INDOM_CPU].i_numinst; n++) {
	    if (__pmInProfile(indomtab[PM_INDOM_CPU].i_indom, profp, cpus[n].id)) {
		if (pmid == PMID(1,26,9) && cpus[n].hwgname == NULL) {
		    /* hinv.map.cpu entry not available */
		    kval = 0;
		    break;
		}
		fetched[n] = 1;
		kval++;
	    }
	}
	if (kval == 0)
	    return 0;

	vpcb->p_nval = kval;
	sizepool(vpcb);   

	for (kval = 0, n = 0; n < indomtab[PM_INDOM_CPU].i_numinst; n++) {

	    switch (pmid) {
	    case PMID(1,26,1):		/* hinv.cpuclock */
		av.ul = (__uint32_t) cpus[n].freq;
		break;
	    case PMID(1,26,4):		/* hinv.secondarycache */
		av.ul = (__uint32_t) cpus[n].sdcache;
		break;
	    case PMID(1,26,9):		/* hinv.map.cpu */
		av.cp = cpus[n].hwgname;
		break;
	    case PMID(1,26,13):		/* hinv.cputype */
		av.cp = cpus[n].type;
		break;
	    default:
		return PM_ERR_PMID;
	    }
	    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[kval], meta[i].m_desc.type)) < 0)
		return sts;
	    vpcb->p_vp[kval++].inst = cpus[n].id;
	}
	vpcb->p_valfmt = sts;
	return sts;
    }

    vpcb->p_nval = 1;
    vpcb->p_vp[0].inst = PM_IN_NULL;

    setinvent();

    switch (pmid) {
    case PMID(1,26,2):		/* hinv.dcache */
	while ((hinv = getinvent()) != NULL) {
	    if ((hinv->inv_class == INV_MEMORY) && (hinv->inv_type == INV_DCACHE)) {
		av.ul = (__uint32_t) hinv->inv_state / 1024;
		break;
	    }
	}
	break;
    case PMID(1,26,3):		/* hinv.icache */
	while ((hinv = getinvent()) != NULL) {
	    if ((hinv->inv_class == INV_MEMORY) && (hinv->inv_type == INV_ICACHE)) {
		av.ul = (__uint32_t) hinv->inv_state / 1024;
		break;
	    }
	}
	break;
    case PMID(1,26,5):		/* hinv.physmem */
	while ((hinv = getinvent()) != NULL) {
	    if ((hinv->inv_class == INV_MEMORY) && (hinv->inv_type == INV_MAIN_MB)) {
		av.ul = (__uint32_t) hinv->inv_state;
		break;
	    }
	}
	break;
    case PMID(1,26,6):		/* hinv.pmeminterleave */
	while ((hinv = getinvent()) != NULL) {
	    if ((hinv->inv_class == INV_MEMORY) && (hinv->inv_type == INV_MAIN_MB)) {
		av.ul = (__uint32_t) hinv->inv_unit;
		break;
	    }
	}
	break;
    case PMID(1,26,7):		/* hinv.ndisk */
	if (_pmSpindleCount == 0) {
	    /* the hard way, only when spindle_stats_init() cannot hack it */
	    n = 0;
	    while ((hinv = getinvent()) != NULL) {
		if ((hinv->inv_class == INV_DISK) && (hinv->inv_type == INV_SCSIDRIVE))
		    n++;
	    }
	}
	else
	    n = _pmSpindleCount;
	av.ul = (__uint32_t) n;
	break;
    case PMID(1,26,8):		/* hinv.nnode */
	av.ul = (__uint32_t) n_nodes;
	break;
    case PMID(1,26,10):		/* hinv.maxcpuclock */
	if (max_cpuclock < min_cpuclock) {
	    vpcb->p_nval = 0;
	    endinvent();
	    return 0;
	}
	else
	    av.ul = (__uint32_t) min_cpuclock;
	break;
    case PMID(1,26,11):		/* hinv.maxcpuclock */
	if (max_cpuclock < min_cpuclock) {
	    vpcb->p_nval = 0;
	    endinvent();
	    return 0;
	}
	else
	    av.ul = (__uint32_t) max_cpuclock;
	break;
    case PMID(1,26,12):		/* hinv.machine */
	av.cp = _uname.machine;
	break;
    case PMID(1,26,14):		/* hinv.ncell */
	av.ul = (__uint32_t) _pm_numcells;
	break;
    case PMID(1,26,15):		/* hinv.pagesize */
	av.ul = (__uint32_t) _pm_pagesz;
	break;
    default: 
	endinvent();
	return PM_ERR_PMID;
    }
    endinvent();

    sts = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].m_desc.type);
    vpcb->p_valfmt = sts;
    return sts;
}
