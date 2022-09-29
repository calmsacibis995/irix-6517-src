/*
 * Handle socket state metrics
 */

#ident "$Id: socket.c,v 1.4 1998/03/17 21:13:53 sca Exp $"

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)

#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

/*ARGSUSED*/
void
socket_init(int reset)
{
}

void
socket_fetch_setup(void)
{
}

static pmDesc dummy = {
    0, PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0}
};

int
socket_desc(pmID pmid, pmDesc *desc)
{
    dummy.pmid = pmid;
    *desc = dummy;
    return 0;
}

/*ARGSUSED*/
int
socket_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    vpcb->p_nval = PM_ERR_APPVERSION;
    return 0;
}

#else /* 6.5 or later */

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysget.h>
#include <sys/tcpipstats.h>
#include <errno.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "cluster.h"

static pmMeta	meta[] = {
/* irix.network.socket.type */
 { 0, { PMID(1,43,1), PM_TYPE_U64, PM_INDOM_SOCKTYPE, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
/* irix.network.socket.state */
 { 0, { PMID(1,43,2), PM_TYPE_U64, PM_INDOM_SOCKSTATE, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched = 0;
static int		typeFlag[TCPSTAT_NUM_SOCKTYPES];
static int		stateFlag[TCPSTAT_NUM_TCPSTATES];
static struct sockstat	sockinfo;


extern int		errno;

/* ARGSUSED */
void
socket_init(int reset)
{
    static int	offset_done = 0;
    int		i = 0;

    if (indomtab[PM_INDOM_SOCKTYPE].i_numinst != TCPSTAT_NUM_SOCKTYPES) {
	__pmNotifyErr(LOG_WARNING,
		     "socket_init: indomtab[PM_INDOM_SOCKTYPE].i_numinst(%d) != TCPSTAT_NUM_SOCKTYPES(%d)\n",
		     indomtab[PM_INDOM_SOCKTYPE].i_numinst,
		     TCPSTAT_NUM_SOCKTYPES);
    }
    if (indomtab[PM_INDOM_SOCKSTATE].i_numinst != TCPSTAT_NUM_TCPSTATES) {
	__pmNotifyErr(LOG_WARNING,
		     "socket_init: indomtab[PM_INDOM_SOCKSTATE].i_numinst(%d) != TCPSTAT_NUM_TCPSTATES(%d)\n",
		     indomtab[PM_INDOM_SOCKSTATE].i_numinst,
		     TCPSTAT_NUM_TCPSTATES);
    }

    if (!offset_done) {
	for (i = 0; i < nmeta; i++) {
	    if (meta[i].m_desc.indom == PM_INDOM_SOCKTYPE)
		meta[i].m_desc.indom = indomtab[PM_INDOM_SOCKTYPE].i_indom;
	    else if (meta[i].m_desc.indom == PM_INDOM_SOCKSTATE)
	    	meta[i].m_desc.indom = indomtab[PM_INDOM_SOCKSTATE].i_indom;
	}
	offset_done = 1;
    }
}

int
socket_desc(pmID pmid, pmDesc *desc)
{
    int         i;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    *desc = meta[i].m_desc;     /* struct assignment */
	    return 0;
	    /*NOTREACHED*/
	}
    }
    return PM_ERR_PMID;
}

void
socket_fetch_setup(void)
{
    int i;

    fetched = 0;
    for (i = 0; i < TCPSTAT_NUM_SOCKTYPES; i++)
	typeFlag[i] = 0;
    for (i = 0; i < TCPSTAT_NUM_TCPSTATES; i++)
	stateFlag[i] = 0;
    return;
}

int
socket_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int                 i;
    int			nval = 0;
    int                 sts = 0;
    pmAtomValue         av;

    switch(pmid) {
    case PMID(1,43,1):		/* irix.network.socket.type */
	for (i = 0; i < TCPSTAT_NUM_SOCKTYPES; i++) {
	    if (__pmInProfile(indomtab[PM_INDOM_SOCKTYPE].i_indom, profp, i))
		vpcb->p_nval++;
		typeFlag[i]++;
	}
	break;
    case PMID(1,43,2):		/* irix.network.socket.state */
	for (i = 0; i < TCPSTAT_NUM_TCPSTATES; i++) {
	    if (__pmInProfile(indomtab[PM_INDOM_SOCKSTATE].i_indom, profp, i))
		vpcb->p_nval++;
		stateFlag[i]++;
	}
	break;
    default:
	return PM_ERR_PMID;
	/*NOTREACHED*/
    }

    sizepool(vpcb);
    
    if (!fetched) {
	sts = (int)sysmp(MP_SAGET1, MPSA_SOCKSTATS, &sockinfo, 
		sizeof(sockinfo), 0);
	if (sts < 0) {
	    __pmNotifyErr(LOG_WARNING,
			 "socket_fetch: sysmp(MPSA_SOCKSTATS) failed: %s\n",
			 strerror(errno));
	    return -errno;
	    /*NOTREACHED*/
	}
    }

    switch(pmid) {
    case PMID(1,43,1):		/* irix.network.socket.type */
	for (i = 0; i < TCPSTAT_NUM_SOCKTYPES; i++) {
	    if (typeFlag[i])
		av.ull = sockinfo.open_sock[i];
	    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], 
				  meta[0].m_desc.type)) < 0)
		return sts;
	    vpcb->p_vp[nval].inst = i;
	    if (nval == 0)
		vpcb->p_valfmt = sts;
	    nval++;
	}
	break;
    case PMID(1,43,2):		/* irix.network.socket.state */
	for (i = 0; i < TCPSTAT_NUM_TCPSTATES; i++) {
	    if (stateFlag[i])
		av.ull = sockinfo.tcp_sock[i];
	    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], 
				  meta[1].m_desc.type)) < 0)
		return sts;
	    vpcb->p_vp[nval].inst = i;
	    if (nval == 0)
		vpcb->p_valfmt = sts;
	    nval++;
	}
	break;
    default:
	return PM_ERR_PMID;
	/*NOTREACHED*/
    }

    return sts;
}

#endif /* 6.5 or later */
