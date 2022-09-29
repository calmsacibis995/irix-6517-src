#ident "$Id: nfs3_server.c,v 1.7 1997/07/23 04:21:08 chatz Exp $"

/*
 * Handle metrics for cluster nfs3_server (37)
 *
 * Code built by newcluster on Mon Jun  6 12:54:30 EST 1994
 */

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/fs/nfs_stat.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

static struct svstat	svstat;
#define N_NFS3REQS 22

static pmMeta		meta[] = {
/* irix.nfs3.server.badcalls */
  { (char *)&svstat.nbadcalls, { PMID(1,37,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.nfs3.server.calls */
  { (char *)&svstat.ncalls, { PMID(1,37,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.nfs3.server.reqs */
  { (char *)&svstat.reqs[0], { PMID(1,37,3), PM_TYPE_32, PM_INDOM_NFS3REQ, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } }
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;

void nfs3_server_init(int reset)
{
    int		i;
    int		indomtag;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
        indomtag = meta[i].m_desc.indom;
        if (indomtag == PM_INDOM_NULL)
            continue;
        if (indomtag < 0 || indomtag >= PM_INDOM_NEXT) {
            __pmNotifyErr(LOG_ERR, "nfs3_server_init: bad instance domain (%d) for metric %s\n",
                         indomtag, pmIDStr(meta[i].m_desc.pmid));
            continue;
        }
        /* Replace tag with it's indom */
        meta[i].m_desc.indom = indomtab[indomtag].i_indom;
    }

    for (i = 0; i < nmeta; i++)
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&svstat));
}

void nfs3_server_fetch_setup(void)
{
    fetched = 0;
}

int nfs3_server_desc(pmID pmid, pmDesc *desc)
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

int nfs3_server_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		k;
    int		sts;
    pmAtomValue	av;
    pmAtomValue	*avp;
    int         nval;
    static int  interesting[N_NFS3REQS];

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    if (fetched == 0) {
#ifdef MPSA_SVSTAT3
                if (sysmp(MP_SAGET, MPSA_SVSTAT3, &svstat, sizeof(svstat)) < 0) {
#else
                errno = PM_ERR_PMID;
                if (1) {
#endif
                    __pmNotifyErr(LOG_WARNING, "nfs3_server_fetch: %s", pmErrStr(-errno));
                    return -errno;
                }
                fetched = 1;
	    }

            switch (pmid) {
            case PMID(1,37,3): /* irix.nfs3.server.reqs */
                vpcb->p_nval = 0;
                for (k=0; k < N_NFS3REQS; k++) {
                    if (interesting[k] = __pmInProfile(meta[i].m_desc.indom, profp, k))
                        vpcb->p_nval++;
                }
                sizepool(vpcb);
                nval = 0;
                for (k=0; k < N_NFS3REQS; k++) {
                    if (interesting[k] == 0)
                        continue;
                    av.ul = svstat.reqs[k];
                    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
                        break;
                    vpcb->p_vp[nval++].inst = k;
                }
		vpcb->p_valfmt = sts;
                return sts;

            default: /* irix.nfs3.server.anything_other_than_reqs */
                vpcb->p_nval = 1;
		vpcb->p_vp[0].inst = PM_IN_NULL;
                avp = (pmAtomValue *)&((char *)&svstat)[(ptrdiff_t)meta[i].m_offset];
                sts = __pmStuffValue(avp, 0, vpcb->p_vp, meta[i].m_desc.type);
                vpcb->p_valfmt = sts;
		return sts;
            }
	}
    }
    return PM_ERR_PMID;
}
