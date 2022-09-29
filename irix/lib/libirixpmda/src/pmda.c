/*
 * Handle metrics for the cluster pmda (0)
 */

#ident "$Id: pmda.c,v 1.7 1997/07/23 04:21:41 chatz Exp $"

#include <sys/types.h>
#include <syslog.h>
#include <sys/utsname.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "pmda_version.h"

/*
 * add one initialization for each element in the structure you wish
 * to export
 */
static pmMeta		meta[] = {
/* irix.pmda.reset */
  { (char *)0, { PMID(1,0,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
/* irix.pmda.version */
  { (char *)0, { PMID(1,0,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
/* irix.pmda.uname */
  { (char *)0, { PMID(1,0,3), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
/* irix.pmda.debug */
  { (char *)0, { PMID(1,0,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
int		pmIrixDebug;

/*ARGSUSED*/
void pmda_init(int reset)
{
}

void pmda_fetch_setup(void)
{
}

int pmda_desc(pmID pmid, pmDesc *desc)
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
int pmda_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		sts;
    pmAtomValue	atom;
    int		len;
    char	ubuf[5 * (_SYS_NMLN+1)];

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    if (pmid == PMID(1,0,1)) {
		/* irix.pmda.reset is always 1 */
		atom.l = 1;
		len = 0;
	    }
	    else if (pmid == PMID(1,0,2)) {
		/* irix.pmda.version */
		atom.cp = buildversion;
		len = (int)strlen(atom.cp);
	    }
	    else if (pmid == PMID(1,0,3)) {
		/* irix.pmda.uname */
		struct utsname	us;
		uname(&us);
		sprintf(ubuf, "%s %s %s %s %s", us.sysname, us.nodename,
			us.release, us.version, us.machine);
		atom.cp = ubuf;
		len = (int)strlen(atom.cp);
	    }
	    else {
		/* irix.pmda.debug */
		atom.l = pmIrixDebug;
		len = 0;
	    }
	    if ((sts = __pmStuffValue(&atom, len, vpcb->p_vp, meta[i].m_desc.type)) < 0)
		return sts;
	    vpcb->p_valfmt = sts;
	    return 0;
	}
    }
    return PM_ERR_PMID;
}
