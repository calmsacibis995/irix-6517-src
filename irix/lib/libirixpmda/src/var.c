/*
 * Handle metrics for cluster var (19)
 *
 * Code built by newcluster on Tue Jun  7 16:39:51 EST 1994
 */

#ident "$Id: var.c,v 1.14 1997/07/23 04:23:10 chatz Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/var.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
#include "./kmemread.h"
#else
#include <sys/sysget.h>
#endif

static struct var	var;
#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
static __psint_t	kaddr;	/* kernel address of struct var */
#endif
static int		pgsz;	/* system page size in bytes */
static pmMeta		meta[] = {
/* irix.resource.nproc */
  { (char *)&var.v_proc, { PMID(1,19,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.resource.nbuf */
  { (char *)&var.v_buf, { PMID(1,19,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.resource.hbuf */
  { (char *)&var.v_hbuf, { PMID(1,19,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.resource.syssegsz */
  { (char *)&var, { PMID(1,19,4), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.resource.maxpmem */
  { (char *)&var.v_maxpmem, { PMID(1,19,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.resource.maxdmasz */
  { (char *)&var.v_maxdmasz, { PMID(1,19,6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.resource.dquot */
  { (char *)&var.v_dquot, { PMID(1,19,7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.nstream_queue */
  { (char *)&var.v_nqueue, { PMID(1,19,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.resource.nstream_head */
  { (char *)&var.v_nstream, { PMID(1,19,9), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } }
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static int		direct_map = 1;

void var_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&var));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,19,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "var_init: direct map disabled @ meta[%d]", i);
	}
    }

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
    kaddr = sysmp(MP_KERNADDR, MPKA_VAR);
#endif
    pgsz = (int)sysmp(MP_PGSIZE);
}

void var_fetch_setup(void)
{
    fetched = 0;
}

int var_desc(pmID pmid, pmDesc *desc)
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
int var_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		 i;
    int		 sts;
    pmAtomValue	 av;
    pmAtomValue	 *avp;
#if !defined(IRIX6_2) && !defined(IRIX6_3) && !defined(IRIX6_4)
    sgt_cookie_t ck;
#endif

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "var_fetch: direct mapping failed for %s (!= %s)\n",
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
#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
	if (kmemread(kaddr, &var, sizeof(struct var))
#else
	SGT_COOKIE_INIT(&ck);
	SGT_COOKIE_SET_KSYM(&ck, KSYM_VAR);
	if (sysget(SGT_KSYM, (char *)&var, sizeof(struct var),
		   SGT_READ, &ck)
#endif
	    != sizeof(struct var)) {
	    __pmNotifyErr(LOG_WARNING,"var_fetch: %s",pmErrStr(-errno));
	    return -errno;
	}
	fetched = 1;
    }

    vpcb->p_nval = 1;
    vpcb->p_vp[0].inst = PM_IN_NULL;
    avp = (pmAtomValue *)&((char *)&var)[(ptrdiff_t)meta[i].m_offset];
    av.ul = avp->ul;
    
    /* convert pages to kbytes */
    if ((pmid == PMID(1,19,4)) ||
	(pmid == PMID(1,19,5)) ||
	(pmid == PMID(1,19,6)))
	av.ul = av.ul * pgsz / 1024;
    
    if ((sts = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].m_desc.type)) < 0)
	return sts;
    vpcb->p_valfmt = sts;
    return 0;
}
