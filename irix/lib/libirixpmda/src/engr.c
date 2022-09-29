/*
 * Handle metrics for SGI engineering cluster engr (34)
 */

#ident "$Id: engr.c,v 1.7 1997/07/23 04:19:08 chatz Exp $"

#if !defined(IRIX6_2) && !defined(IRIX6_3) && !defined(IRIX6_4)

#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

/*ARGSUSED*/
void
engr_init(int reset)
{
}

void
engr_fetch_setup(void)
{
}

static pmDesc dummy = {
    0, PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0}
};

int
engr_desc(pmID pmid, pmDesc *desc)
{
    *desc = dummy;
    desc->pmid = pmid;
    return 0;
}

/*ARGSUSED*/
int
engr_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    vpcb->p_nval = PM_ERR_APPVERSION;
    return 0;
}

#else /* IRIX6_2 || IRIX 6_3 || IRIX6_3 */

#include <sys/types.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./kmemread.h"

/*
 * replace this with the structure you wish to export ...
 */
typedef struct {
    int		one;
    int		two;
    int		three;
    int		four;
} engr_t;
static engr_t	engr;

/*
 * add one initialization for each element in the structure you wish
 * to export
 */
static pmMeta		meta[] = {
/* irix.engr.one */
  { (char *)&engr.one, { PMID(1,34,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.engr.two */
  { (char *)&engr.two, { PMID(1,34,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.engr.three */
  { (char *)&engr.three, { PMID(1,34,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.engr.four */
  { (char *)&engr.four, { PMID(1,34,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;

void engr_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++)
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&engr));
}

void engr_fetch_setup(void)
{
    fetched = 0;
}

int engr_desc(pmID pmid, pmDesc *desc)
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
int engr_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		sts;
    int		*ip;
    pmAtomValue	atom;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    if (fetched == 0) {
		/*
		 * need method here to refresh the exported structure ...
		 * code in #if 0 suggests how for /dev/kmem reader
		 */
#if 0
		/*
		__psunsigned_t	offset;
		 * KS_ENGR would be defined in kmemread.h ...
		 * see also kmemread.c
		 */
		offset = (__psunsigned_t)kernsymb[KS_ENGR].n_value;
		sts = kmemread(offset, &engr, sizeof(engr));
		if (sts != sizeof(engr)) {
		    __pmNotifyErr(LOG_WARNING, "engr.c: read engr: %s", pmErrStr(-errno));
		    return -errno;
		}
		fetched = 1;
#endif
	    }
	    if (fetched) {
		vpcb->p_nval = 1;
		vpcb->p_vp[0].inst = PM_IN_NULL;
		ip = (int *)&((char *)&engr)[(ptrdiff_t)meta[i].m_offset];
		atom.l = *ip;
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

#endif /* IRIX6_2 || IRIX6_3 || IRIX6_4 */
