/*
 * Handle metrics for cluster syserr (20)
 *
 * Code built by newcluster on Wed Jun  8 15:54:57 EST 1994
 */

#ident "$Id: syserr.c,v 1.9 1997/07/23 04:22:30 chatz Exp $"

#define _KMEMUSER

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

static struct syserr	syserr;

static pmMeta		meta[] = {
/* irix.resource.fileovf */
  { (char *)&syserr.fileovf, { PMID(1,20,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.resource.procovf */
  { (char *)&syserr.procovf, { PMID(1,20,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } }
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;

void syserr_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++)
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&syserr));
}

void syserr_fetch_setup(void)
{
    fetched = 0;
}

int syserr_desc(pmID pmid, pmDesc *desc)
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
int syserr_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		sts;
    pmAtomValue	*avp;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    if (fetched == 0) {
		if (sysmp(MP_SAGET, MPSA_SERR, &syserr, sizeof(struct syserr)) < 0) {
		    __pmNotifyErr(LOG_WARNING, "syserr_fetch: %s", pmErrStr(-errno));
		    return -errno;
		}
		fetched = 1;
	    }
	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    avp = (pmAtomValue *)&((char *)&syserr)[(ptrdiff_t)meta[i].m_offset];
	    if ((sts = __pmStuffValue(avp, 0, vpcb->p_vp, meta[i].m_desc.type)) < 0)
		return sts;
	    vpcb->p_valfmt = sts;
	    return 0;
	}
    }
    return PM_ERR_PMID;
}
