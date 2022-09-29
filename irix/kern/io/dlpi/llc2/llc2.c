/******************************************************************
 *
 *  SpiderX25 - LLC2 Multiplexer
 *
 *  Copyright 1991 Spider Systems Limited
 *
 *  LLC2.C
 *
 *    Top-level STREAMS driver for LLC2.
 *
 ******************************************************************/

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/sys/llc2/86/s.llc2.c
 *	@(#)llc2.c	1.63
 *
 *	Last delta created	16:53:13 5/28/93
 *	This file extracted	16:55:16 5/28/93
 *
 */


#ident "@(#)llc2.c	1.63 (Spider) 5/28/93"

#ifdef ENP
#define FAR
#endif

#ifdef DEVMT
#define FAR far
#endif


#define FAR
#define STATIC static
typedef unsigned char *bufp_t;
#define BUFP_T bufp_t

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#ifndef SVR4
#include <sys/sysmacros.h>
#endif
#include <sys/systm.h>
#include <sys/strlog.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/dlpi.h>
#include <sys/snet/uint.h>
#include <sys/snet/ll_proto.h>
#include <sys/snet/ll_control.h>
#include <sys/snet/dl_proto.h>
#include <sys/snet/timer.h>
#include <sys/snet/x25trace.h>
#include <sys/snet/system.h>
/* #include "sys/snet/llc2match_if.h" */
#include "sys/snet/llc2.h"
#ifdef SVR4
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/proc.h>
#endif /* SVR4 */

#ifdef SVR4
extern clock_t lbolt;
#endif

/* Default tuning table from 'space.c' */
extern llc2tune_t   llc2_tdflt;

/* Counts from master.d/llc2 */
extern unsigned int	llc2_ndevs;
extern unsigned int	llc2_ndns;
extern unsigned int	llc2_nups;
extern unsigned int	llc2_ncns;
/* Counts from 'Space.c' */
extern uint_t	    llc2_rqhwm;

/* Various tables to be allocated in llc2open() */
uchar_t		*llc2_devtab;
llc2up_t	*llc2_uptab;
llc2dn_t	*llc2_dntab;
llc2cn_t	*llc2_cntab;

/* Local static variables */
STATIC llc2cn_t *llc2_cnfree;

/* Functions */
#ifdef SGI
static int close_llc2_tracestrm(queue_t*);
static closedn(llc2dn_t*);
static flushrx(llc2dn_t*);
static disable_all_multicast(llc2up_t*);

extern int closeup(register llc2up_t*, int);
extern int llc2_flushstock(void);
extern int merror(queue_t *, mblk_t *, int);
extern int llc2_cninit(llc2cn_t *);
extern int llc2wput_dlpi(queue_t *, mblk_t *);
extern int llc2_macinsrv(llc2dn_t*);
extern int llc2_schedsrv(llc2dn_t*);
extern int llc2_udatasrv(llc2dn_t*);
extern void unregister_q(caddr_t, queue_t*, queue_t*, uint16);
extern void deregister_q(caddr_t, queue_t*);
extern int llc2_stop(llc2cn_t*);
#else
llc2up_t *llc2_findup();
llc2cn_t *llc2_connectup();
#endif

#ifdef LLCDBG
int	llc2_debug = 1;
#else
int	llc2_debug = 0;
#endif

STATIC int llc2open(), llc2close(), llc2wput(), llc2rput(), llc2wsrv();
STATIC int llc2upwsrv();

STATIC struct module_info uprinfo = { LLC_STID, "llc2 UR", 0, INFPSZ, 0x2000, 0x800 };
STATIC struct module_info upwinfo = { LLC_STID, "llc2 UW", 0, INFPSZ, 0x2000, 0x800 };
STATIC struct module_info dnrinfo = { LLC_STID, "llc2 DR", 0, INFPSZ, 1, 0 };
STATIC struct module_info dnwinfo = { LLC_STID, "llc2 DW", 0, INFPSZ, 0, 0 };

STATIC struct qinit uprinit =
    { 0, 0, llc2open, llc2close, 0, &uprinfo, 0 };

STATIC struct qinit upwinit =
    { llc2wput, llc2upwsrv, 0, 0, 0, &upwinfo, 0 };

STATIC struct qinit dnrinit =
    { llc2rput, 0, 0, 0, 0, &dnrinfo, 0 };

STATIC struct qinit dnwinit =
    { 0, llc2wsrv, 0, 0, 0, &dnwinfo, 0 };

struct streamtab llc2info = { &uprinit, &upwinit, &dnrinit, &dnwinit };

#ifdef SGI
void
llc2_hexdump(uint8 *cp, int len)
{
    register int idx;
    register int qqq;
    char qstr[512];
    static char digits[] = "0123456789abcdef";

    for (idx = 0, qqq = 0; qqq<len;) {
        qstr[idx++] = digits[cp[qqq] >> 4];
        qstr[idx++] = digits[cp[qqq] & 0xf];
        qstr[idx++] = ' ';
        if ((++qqq%16) == 0)
            qstr[idx++] = '\n';
    }
    qstr[idx] = 0;
    if (qqq%16)
        printf("%s\n", qstr);
}
#endif

int llc2devflag = D_MP;

/* ARGSUSED */
STATIC int 
llc2open(q, dev, flag, sflag, credp)
queue_t *q;
dev_t   *dev;
int flag, sflag;
cred_t  *credp;
{
    unsigned char *cp;
    static int initdone = 0;
    dev_t   device = *dev;

    if (!initdone) {
	if (!(llc2_devtab=kmem_zalloc(sizeof(uchar_t)*llc2_ndevs,KM_NOSLEEP))) {
	    goto open_fail;
	}
	if (!(llc2_dntab=kmem_zalloc(sizeof(llc2dn_t)*llc2_ndns,KM_NOSLEEP))) {
	    kmem_free(llc2_devtab,sizeof(uchar_t)*llc2_ndevs);
	    goto open_fail;
	}
	if (!(llc2_uptab=kmem_zalloc(sizeof(llc2up_t)*llc2_nups,KM_NOSLEEP))) {
	    kmem_free(llc2_devtab,sizeof(uchar_t)*llc2_ndevs);
	    kmem_free(llc2_dntab,sizeof(llc2dn_t)*llc2_ndns);
	    goto open_fail;
	}
	if (!(llc2_cntab=kmem_zalloc(sizeof(llc2cn_t)*llc2_ncns,KM_NOSLEEP))) {
	    kmem_free(llc2_devtab,sizeof(uchar_t)*llc2_ndevs);
	    kmem_free(llc2_dntab,sizeof(llc2dn_t)*llc2_ndns);
	    kmem_free(llc2_uptab,sizeof(llc2up_t)*llc2_nups);
	    goto open_fail;
	}
	bzero(llc2_devtab,sizeof(uchar_t)*llc2_ndevs);
	bzero(llc2_dntab,sizeof(llc2dn_t)*llc2_ndns);
	bzero(llc2_uptab,sizeof(llc2up_t)*llc2_nups);
	bzero(llc2_cntab,sizeof(llc2cn_t)*llc2_ncns);
	initdone=1;
    }
    if (sflag == CLONEOPEN)
    {   /* Opened as clone */
        for (device = 0; device<llc2_ndevs; device++)
	{
            if (!llc2_devtab[device])
	    {
        	*dev = makedevice(getemajor(*dev), device);
		break;
	    }
	}
    }
    else
        device = getminor(*dev);

    if (device >= llc2_ndevs)
        return ENXIO;

    if (!*(cp = &llc2_devtab[device]))
    {   /* New minor device, so record as opened */
	q->q_ptr = (caddr_t)cp;
	*cp = 1;
    }
    return 0;
open_fail:
    cmn_err(CE_WARN,"llc2: llc2open failed to allocate internal tables\n");
    return ENOMEM;
}

/* ARGSUSED */
STATIC int 
llc2close(q, flags, credp)
queue_t *q;
int      flags;
cred_t   *credp;
{
    llc2up_t *upp;

    (void)close_llc2_tracestrm(q);

    if (upp = (llc2up_t *)WR(q)->q_ptr)
    {   /* Write side has control block: registered stream */
#ifdef SGI
	closeup(upp,1);
#else
	closeup(upp);
#endif
	bzero((BUFP_T)upp, sizeof(llc2up_t));
	WR(q)->q_ptr = (caddr_t)0;
    }

    /* Mark device as closed (using read side queue pointer) */
    *(unchar *)q->q_ptr = 0;
    q->q_ptr = (caddr_t)0;

    /* Flush stock messages */
    llc2_flushstock();
    return 0;
}

STATIC int llc2wput(q, mp)
queue_t *q;
mblk_t  *mp;
{   /* This is the upper (write) put procedure */
    llc2up_t *upp = (llc2up_t *)q->q_ptr; /* NULL if not registered */
    register llc2dn_t *dnp;
    uint n;

    switch (mp->b_datap->db_type)
    {
    case M_DATA:
    {
	llc2cn_t *lp;

	/*
	 * M_DATA = "send LLC2 data" in the DLPI interface.  (Is equivalent
	 * to the LC_DATA message in the Spider interface.)
	 */
	if (!upp) {
	    /*
	     * Stream is not attached yet, so the data was received while
	     * in the wrong state.
	     */
	    merror(q, mp, EPROTO);
	    return 0;
	}
	if (upp->up_interface != LLC_IF_DLPI) {
	    /*
	     * freemsg(mp) is called after breaking out of the switch.
	     */
	    break;
	}
	if (upp->up_dlpi_state != DL_DATAXFER) {
	    /*
	     * Data received while out of state.  If in idle state or
	     * awaiting reset, drop the data, otherwise reply with an
	     * M_ERROR message (as per the DLPI spec).
	     */
	    if (upp->up_dlpi_state == DL_IDLE ||
	        upp->up_dlpi_state == DL_PROV_RESET_PENDING) {
		/*
		 * This case includes trying to send M_DATA msgs on an
		 * LLC1 stream.
		 */
		break;
	    } else {
		merror(q, mp, EPROTO);
		return 0;
	    }
	}
	lp = upp->up_cnlist;

	/* At one customer site, lp could be NULL here. Efforts of trying to
	 * understand the cause failed since we could not reproduce here
	 * and after a long period of time of being able to reproduce at
	 * the customer site (at least twice a week), it did not happen 
	 * for a month.
	 * Put in this check for defense reason to prevent from kernel
	 * panic.	jlan 03/03/95
	 */
	if (!lp) {
	    printf("llc2_wput: lp is NULL, q=%p, up=%p, up_dlpi_state=%d\n",
		    q, upp, upp->up_dlpi_state);
	    merror(q,mp,EIO);
	    return 0;
	}

	/* XXX Need to test lp->discard ? */
	/*
	 * Attach data to ack/dat queue, and start up 'dat' queue if it
	 * was empty.
	 */
	if (lp->ackq)
	    lp->adqtail->b_next = mp;
	else
	    lp->ackq = mp;
	lp->adqtail = mp;

	if (!lp->datq) {
	    /* Data queue was empty */
	    lp->datq = mp;
	    if (!lp->txstopped)
		llc2_schedule(lp);
	}
	return 0;
    }  /* case M_DATA */

    case M_IOCTL:
	{
	    struct iocblk FAR *iocp = (struct iocblk FAR *)mp->b_rptr;
	    mblk_t *bp;
	    int command;
	    unchar error = EINVAL;

	    switch (command = (int)iocp->ioc_cmd)
	    {
	    case I_LINK:
	    case I_UNLINK:
		{
		    struct linkblk FAR *linkp =
				(struct linkblk FAR *) mp->b_cont->b_rptr;
		    queue_t *downq = linkp->l_qbot;

		    if (command == I_LINK)
		    {   /* LINK action */

			/* Error if no free structure left */
			error = EAGAIN;

			/* Find free structure */
			for (n = llc2_ndns, dnp = llc2_dntab; n--; dnp++)
			{
			    if (!dnp->dn_downq)
			    {   /* Found a free down control block */

				/* Interconnect with lower stream head */
				dnp->dn_downq = downq;
				dnp->dn_index = linkp->l_index;
				downq->q_ptr = RD(downq)->q_ptr =
							    (caddr_t)dnp;
				noenable(RD(downq));

				/* Initialise tuning values */
				bcopy((BUFP_T)&llc2_tdflt,
				      (BUFP_T)&dnp->dn_tunetab,
				      sizeof(llc2tune_t));

				/* Initialise station component */
				llc2_cninit(&dnp->dn_station);
				dnp->dn_station.dnp = dnp;

				/* No error */
				error = 0;

				/* Leave search loop */
				break;
			    }
			}
		    }
		    else
		    {   /* UNLINK action */
			if (dnp = (llc2dn_t *)downq->q_ptr)
			{   /* Was successfully linked */

			    /* Close connections */
			    closedn(dnp);

			    if (dnp->llc1_match)
			    {
				end_match(dnp->llc1_match);
				dnp->llc1_match = (caddr_t) 0;
			    }
			    if (dnp->eth_match)
			    {
				end_match(dnp->eth_match);
				dnp->eth_match = (caddr_t) 0;
			    }
#ifdef SVR4
			    if (dnp->dn_bufcall)
			    {
				(void) unbufcall(dnp->dn_bufcall);
				dnp->dn_bufcall = 0;
			    }
#endif

			    /* Stop station component timers  */
			    stop_timers(&dnp->dn_station.thead,
					 dnp->dn_station.tmtab, NTIMERS);

			    /* Discard private queue */
			    flushrx(dnp);

			    /* Disconnect from lower stream head */
			    dnp->dn_downq = (queue_t *)0;
			    downq->q_ptr = RD(downq)->q_ptr = (caddr_t)0;
			    bzero((BUFP_T)dnp, sizeof(llc2dn_t));
			}

			/* No error */
			error = 0;
		    }
		}

		/* No data back from I_[UN]LINK */
		iocp->ioc_count = 0;
		break;

	    case L_SETSNID:	/* and L_SETPPA */
		if ((bp = mp->b_cont) &&
			bp->b_wptr - bp->b_rptr >= sizeof(struct ll_snioc))
		{
		    struct ll_snioc FAR *pp =
			    (struct ll_snioc FAR *)bp->b_rptr;

		    /* Sanity checks */
		    if (pp->lli_type != LI_SNID ||
			pp->lli_index == 0 ||
			pp->lli_snid == 0)
			break;

		    /* Error if not found */
		    error = ENODEV;

		    /* Find downward stream */
		    for (n = llc2_ndns, dnp = llc2_dntab; n--; dnp++)
		    {   /* Scan structures */

			if (dnp->dn_index == pp->lli_index)
			{   /* Found: set SNID */
			    dnp->dn_snid = pp->lli_snid;
#ifdef SGI
			    /* Set default MAC Addr Length. */
			    dnp->dn_maclen = 6;
#endif
			    error = 0;
			    break;
			}
		    }
		}
		break;

	    case L_GETSNID:	/* and L_GETPPA */
		if ((bp = mp->b_cont) &&
			bp->b_wptr - bp->b_rptr >= sizeof(struct ll_snioc))
		{
		    struct ll_snioc FAR *pp =
			    (struct ll_snioc FAR *)bp->b_rptr;

		    /* Sanity check */
		    if (pp->lli_type != LI_SNID)
			break;

		    if (upp)
		    {   /* Registered */
			dnp = upp->up_dnp;
			pp->lli_snid = dnp->dn_snid;
			pp->lli_index = dnp->dn_index;
			error = 0;
		    }
		    else
			error = ENODEV;
		}
		break;

	    case L_GETTUNE:  /* Interrogate dnp->dn_tunetab	*/
	    case L_SETTUNE:  /* (Re-)initialise dnp->dn_tunetab */
		if ((bp = mp->b_cont) &&
			bp->b_wptr - bp->b_rptr >= sizeof(struct llc2_tnioc))
		{
		    struct llc2_tnioc FAR *pp =
			    (struct llc2_tnioc FAR *)bp->b_rptr;

		    /* Sanity check */
		    if (pp->lli_type!=LI_LLC2TUNE)
			break;

		    /* Error if no down stream found */
		    error = ENODEV;

		    /* Find appropriate down stream(s) */
		    for (n = llc2_ndns, dnp = llc2_dntab; n--; dnp++)
		    {
			if (dnp->dn_downq && (pp->lli_snid == dnp->dn_snid ||
				command == L_SETTUNE && pp->lli_snid == (uint32)'*'))
			{   /* Match: transfer in appropriate direction */
			    if (iocp->ioc_cmd == L_SETTUNE)
				bcopy((BUFP_T)&pp->llc2_tune,
				      (BUFP_T)&dnp->dn_tunetab,
				      sizeof(llc2tune_t));
			    else
				bcopy((BUFP_T)&dnp->dn_tunetab,
				      (BUFP_T)&pp->llc2_tune,
				      sizeof(llc2tune_t));

			    /* No error */
			    error = 0;
			}
		    }
		}
		break;

	    case L_GETSTATS:
	    case L_ZEROSTATS:
		if ((bp = mp->b_cont) && bp->b_wptr - bp->b_rptr >=
			(command == L_GETSTATS ? sizeof(struct llc2_stioc)
					       : sizeof(struct ll_hdioc)))
		{
		    lliun_t FAR *pp = (lliun_t FAR *)bp->b_rptr;

		    error = ENODEV;
		    for (n = llc2_ndns, dnp = llc2_dntab; n--; dnp++)
		    {   /* Scan 'down's to find SNID */
			if (dnp->dn_downq &&
			    (pp->ll_hd.lli_snid == dnp->dn_snid ||
				command == L_ZEROSTATS &&
						pp->ll_hd.lli_snid == (uint32)'*'))
			{
			    if (command == L_GETSTATS)
				bcopy((BUFP_T)&dnp->dn_statstab,
				      (BUFP_T)&pp->llc2_st.lli_stats,
				      sizeof(llc2stats_t));
			    else
			    {
				bzero((BUFP_T)&dnp->dn_statstab,
				      sizeof(llc2stats_t));
			    }
			    error = 0;
			}
		    }
		}
		break;

	    case L_TRACEON:
	    case L_TRACEOFF:
	    {

		struct trc_regioc  FAR *reg;
		int snid_found, snid_busy;

		reg = (struct trc_regioc  FAR *)mp->b_cont->b_rptr;

		/*
		    Check the privilege
		*/
		if ( iocp->ioc_uid != 0 )
		{
		    error = EACCES;
		    break;
		}

		if ( iocp->ioc_count != sizeof(struct trc_regioc) )
		{
		    error = EINVAL;
		    break;
		}

		snid_found = snid_busy = 0;
	    
		if ( command == L_TRACEOFF )
		{
		    snid_found = close_llc2_tracestrm(RD(q));

		    if ( snid_found )
		    {
			reg->all_snworks = snid_found;
			error  = 0;
		    }
		    else
		    {
			error = ENODEV;
		    }
		}
		else
		{
		    if ( reg->all_snworks )
		    {
			/*
			    All subnetworks to trace level specified
			*/
			for (n = llc2_ndns, dnp = llc2_dntab; n--; dnp++)
			{
			    if ( dnp->dn_trace_active == 0 )
			    {
				dnp->dn_trace_active = (reg->level) ? reg->level : 1;
				dnp->dn_trace_queue  = RD(q);
				reg->active[snid_found++] = dnp->dn_snid;
			    }
			    else
			    	snid_busy++;
			}
		    }
		    else
		    {
			/*
			    Subnetwork and trace level specified.
			*/
			for (n = llc2_ndns, dnp = llc2_dntab; n--; dnp++)
		        {
			    if ( dnp->dn_snid == reg->snid )
			    {
				if ( dnp->dn_trace_active == 0 )
				{
				    dnp->dn_trace_active = (reg->level) ? reg->level : 1;
				    dnp->dn_trace_queue  = RD(q);
				    reg->active[snid_found++] = dnp->dn_snid;
				}
				else
				    snid_busy++;

				break;
			    }
			}
		    }

		    if ( snid_found )
		    {
			if ( reg->all_snworks )
			{
		    	    reg->all_snworks      = snid_found;
			    reg->active[snid_found] = '\0';
			}
			error = 0;
		    }
		    else
		    {
			if ( snid_busy )
			   error = EEXIST;
			else
			   error = ENODEV;
		    }
		}
	    }
	    break;

	    default:
		break;
	    }

	    mp->b_datap->db_type = error ? M_IOCNAK : M_IOCACK;
	    iocp->ioc_error = error;
	    qreply(q, mp);
	    return 0;
	}

    case M_FLUSH:
	if (*mp->b_rptr & FLUSHW)
	{
	    *mp->b_rptr &= ~FLUSHW;

	    /* Flush write queue */
	    flushq(q, FLUSHALL);
	}

	if (*mp->b_rptr & FLUSHR)
	{
	    /* Flush read queue */
	    flushq(RD(q), FLUSHALL);

	    /* Send back up read side */
	    qreply(q, mp);
	    return 0;
	}
	break;

    case M_PROTO:
    case M_PCPROTO:
	{
	    register union ll_un FAR *pp = (union ll_un FAR *)mp->b_rptr;

	    if (pp->ll_type == LL_REG)
	    {
		uint8 regstatus = LS_SUCCESS;
		uint8 class = pp->reg.ll_class;

		DLP(("llc2wput: LL_REG\n"));
		/* Sanity checks */
		if (upp ||
		    pp->reg.ll_snid == 0 ||
		    (class != LC_LLC2 && class != LC_LLC1) ||
		    pp->reg.ll_normalSAP == 0) regstatus = LS_EXHAUSTED;

		if (regstatus == LS_SUCCESS)
		{
		    /* (loopback SAP == 0) => (loopback SAP == normal SAP) */
		    if (pp->reg.ll_loopbackSAP == 0)
			pp->reg.ll_loopbackSAP = pp->reg.ll_normalSAP;

		    /* Find network */
		    regstatus = LS_EXHAUSTED;

		    for (n = llc2_ndns, dnp = llc2_dntab; n--; dnp++)
		    {   /* Scan 'down's to find SNID */
			if (dnp->dn_snid == pp->reg.ll_snid)
			{   /* Found */
			    regstatus = LS_SUCCESS;
			    break;
			}
		    }
		}

		if (regstatus == LS_SUCCESS)
		{
		    for (upp = dnp->dn_uplist; upp; upp = upp->up_dnext)
		    {   /* Scan existing registrations */
			if (pp->reg.ll_normalSAP == upp->up_nmlsap ||
			    pp->reg.ll_normalSAP == upp->up_lpbsap ||
			    pp->reg.ll_loopbackSAP == upp->up_nmlsap ||
			    pp->reg.ll_loopbackSAP == upp->up_lpbsap)
			{   /*
			     * upp is registered on the same
			     * network, and its normal SAP or its
			     * loopback SAP (if it has one)
			     * is the same as either the new normal
			     * SAP or the new loopback SAP
			     * (if there is one)
			     */
			    regstatus = LS_SSAPINUSE;
			    break;
			}
		    }
		}

		if (regstatus == LS_SUCCESS)
		{   /* Find an up control block */
		    regstatus = LS_EXHAUSTED;

		    for (n = llc2_nups, upp = llc2_uptab; n--; upp++)
		    {   /* Scan table */
			if (!upp->up_readq)
			{   /* Free block found */
			    if (dnp->llc1_match == (caddr_t) 0)
			    {
				    /*
				     * There is currently no LLC match table,
				     * so create one.
				     */
				    dnp->llc1_match = init_match();
				    if (dnp->llc1_match == (caddr_t) 0)
					break;
			    }

			    if (register_sap(dnp->llc1_match, RD(q),
				    &(pp->reg.ll_normalSAP), 1) < 0)
			    {
			        regstatus = LS_SSAPINUSE;
				break;
			    }

			    /* Attach to write side only */
			    q->q_ptr = (caddr_t)upp;
			    upp->up_readq = RD(q);

			    /* Fill in class and SAPs */
			    upp->up_class = class;
			    upp->up_nmlsap = pp->reg.ll_normalSAP;
			    upp->up_lpbsap = pp->reg.ll_loopbackSAP;

			    /* Put in down block's up list */
			    upp->up_dnext = dnp->dn_uplist;
			    dnp->dn_uplist = upp;
			    upp->up_dnp = dnp;

			    /*
			     * LL_REG is the first message received on a
			     * stream supporting Spider's interface.
			     */
			    upp->up_interface = LLC_IF_SPIDER;
			    upp->up_mode = LLC_MODE_NORMAL;

			    regstatus = LS_SUCCESS;
			    break;
			}
		    }
		}

		if (regstatus == LS_SUCCESS &&
				dnp->dn_regstage == REGNONE)
		{   /* Send off MAC registration */
		    mblk_t *mmp;
		    struct datal_type FAR *mpp;

		    DLP(("llc2wput: Send off MAC registration\n"));
#if DL_VERSION >= 2
                    if (!(mmp = allocb(sizeof(struct datal_type) + MAXHWLEN,
                        BPRI_MED)))
#else /* DL_VERSION >= 2 */

		    if (!(mmp = allocb(sizeof(struct datal_type), BPRI_MED)))
#endif /* DL_VERSION >= 2 */
		    {
		        strlog(LLC_STID,0,0,SL_ERROR,"LLC2: No buffer for Ethernet Registration");
			regstatus = LS_EXHAUSTED;
		    }
		    else
		    {
			/* Assemble MAC registration message */
			mpp = (struct datal_type FAR *)mmp->b_wptr;
#ifdef ETH_PTYPE
			mpp->prim_type = ETH_PTYPE;
#else
			mpp->prim_type = DATAL_TYPE;
#endif /* ETH_PTYPE */
			mpp->version = DL_VERSION;
#if defined (sgi)
			mpp->lwb = pp->reg.ll_normalSAP;
			mpp->upb = pp->reg.ll_normalSAP;
#else
			mpp->lwb = 0;
			mpp->upb = 1500;
#endif
			mmp->b_datap->db_type = M_PROTO;
#if DL_VERSION >= 2
                        mmp->b_wptr += sizeof(struct datal_type) + MAXHWLEN;
#else /* DL_VERSION >= 2 */
			mmp->b_wptr += sizeof(struct datal_type);
#endif /* DL_VERSION >= 2 */

			/* Registration now in hand */
			dnp->dn_regstage = REGTOMAC;

			/* Send MAC registration off */
			putnext(dnp->dn_downq, mmp);
		    }
		}

		if ((pp->reg.ll_regstatus = regstatus) == LS_SUCCESS)
		{   /* OK so far: rest is handled by state M/C server */
		    putq(q, mp);
		    llc2_schedule(&dnp->dn_station);
		    return 0;
		}

		qreply(q, mp);
		return 0;
	    }

	    /* Not a registration message: sanity check */
	    if (!upp) {
		dlu32_t	prim;

		/*
		 * Maybe it's a DLPI message?
		 */
		if (MBLKL(mp) < sizeof(dlu32_t)) {
		    /*
		     * Not long enough to be a DLPI (or Spider) message,
		     * so free it.
		     */
		    break;
		}
		prim = ((union DL_primitives *)pp)->dl_primitive;
		if (prim <= DL_MAXPRIM) {
		    llc2wput_dlpi(q, mp);
		    return 0;
		} else {
		    /*
		     * Default action (for Spider interface) is to free the
		     * invalid message and send no nak upstream.
		     */
		    break;
		}
	    }

	    if (upp->up_interface == LLC_IF_DLPI) {
		llc2wput_dlpi(q, mp);
		return 0;
	    }

	    /* Spider interface: switch on non-LL_REG type */
	    if (!(dnp = upp->up_dnp)) break;

	    /* Switch on non-LL_REG type */
	    switch (pp->ll_type)
	    {
	    case LL_DAT:
		switch (pp->msg.ll_command)
		{
		case LC_DATA:
		    {
			register llc2cn_t *lp = 
				(llc2cn_t *)pp->msg.ll_yourhandle;

			if (upp->up_class == LC_LLC2 &&
			    lp->connID == pp->msg.ll_connID && !lp->discard)
			{   /* LLC2, same component and not discarding */

			    /* Detach info part from incoming message */
			    register mblk_t *bp = mp->b_cont;
			    mp->b_cont = (mblk_t *)0;

			    /* Attach info part to ack/dat queue */
			    if (lp->ackq)
				lp->adqtail->b_next = bp;
			    else
				lp->ackq = bp;
			    lp->adqtail = bp;

			    /* Start up 'dat' queue if it was empty */
			    if (!lp->datq)
			    {   /* Data queue was empty */
				lp->datq = bp;
				if (!lp->txstopped)
				    llc2_schedule(lp);
			    }
			    /* Run into 'break' to discard header ... */
			}
		    }
		    break;

		case LC_UDATA:
		    if (upp->up_class == LC_LLC1 &&
			pp->msgc.ll_connID == 0 &&
			pp->msgc.ll_yourhandle == 0)
		    {
			putq(q, mp);

			/* Schedule this stream for service */
			if (dnp->dn_udatq)
			{   /* There is a stream already scheduled */
			    if (!(upp->up_udatqnext ||
					    dnp->dn_udatqtail == upp))
			    {   /* This stream is not on the chain */
				dnp->dn_udatqtail->up_udatqnext = upp;
				dnp->dn_udatqtail = upp;
			    }
			}
			else
			{   /* Chain was empty, so start with this component and enable queue */
			    dnp->dn_udatq = upp;
			    dnp->dn_udatqtail = upp;
			    qenable(dnp->dn_downq);
			}
			return 0;
		    }
		    break;

		default:
		    /* Shouldn't happen ... */
		    break;
		}
		break;

	    case LL_CTL:
		{
		    register llc2cn_t *lp;

		    if (pp->msgc.ll_command == LC_CONNECT)
		    {
			uint16 status;

			if (dnp->dn_regstage != REGDONEOK ||
			    upp->up_class != LC_LLC2 ||
			    !(lp = llc2_connectup(upp)))
			{   /* No good! */
			    status = LS_EXHAUSTED;
			}
			else
			{   /* Registered and got a structure OK */
			    register llc2cn_t *xlp;
			    llc2cn_t **hashp;

			    /* Copy address info into component */
			    if (!(status =
				llc2_dtecpy(lp, upp, &pp->msgc, dnp->dn_macaddr)))
			    {   /* Legal addresses */

				/* Check unique: if so, add to hash */
				hashp = upp->up_dnp->dn_hashtab +
							(HASH(lp->dte));
				for (xlp = *hashp;; xlp = xlp->hnext)
				{	/* Scan hash chain */
				    if (!xlp)
				    {   /* Unique, so add to hash */
					lp->hnext = *hashp;
					*hashp = lp;
					break;
				    }
				    if (*(uint32 *)&xlp->dte[0] ==
					    *(uint32 *)&lp->dte[0] &&
					*(uint32 *)&xlp->dte[4] ==
					    *(uint32 *)&lp->dte[4])
				    {   /* Found, so fail */
					status = LS_LSAPINUSE;
					break;
				    }
				}
			    }
			}

			if (status)
			{   /* Failed: tell the network layer */
			    if (lp)
				llc2_disconnect(lp);
			    pp->msg.ll_command = LC_CONNAK;
			    mp->b_datap->db_type = M_PCPROTO;
			    pp->msg.ll_yourhandle = pp->msgc.ll_myhandle;
			    pp->msg.ll_status = status;
			    qreply(q, mp);
			    return 0;
			}
		    }
		    else
		    {   /* Not connect: get lp value from message */
			lp = (llc2cn_t *)pp->msg.ll_yourhandle;
		    }

		    /* Now switch on command (including OK connect) */
		    switch (pp->msgc.ll_command)
		    {
		    case LC_PRGBRA:
			if (lp->connID != pp->msg.ll_connID) lp->discard++;
			break;

		    case LC_PRGKET:
			if (lp->connID != pp->msg.ll_connID) lp->discard--;
			break;

		    case LC_CONNECT:
		    case LC_CONOK:
			lp->nethandle = pp->msgc.ll_myhandle;
			/* Run into LC_CONNAK ... */

		    case LC_CONNAK:
			lp->connID = pp->msg.ll_connID;
			/* Run into default ... */

		    default:
			if (lp->connID == pp->msg.ll_connID)
			{
			    /* Attach whole message to control queue */
			    if (lp->ctlq)
				lp->ctlqtail->b_next = mp;
			    else
			    {   /* First message, so schedule connection */
				lp->ctlq = mp;
				llc2_schedule(lp);
			    }
			    lp->ctlqtail = mp;
			    return 0;
			}
			break;
		    }
		}
		break;

	    default:
		/* Shouldnt happen ... */
		break;
	    }
	}
	break;

    default:
	/* Shouldn't happen ... */
	break;
    }

    /* We will have obeyed 'return' if whole message has been passed on */
    freemsg(mp);
    return 0;
}


STATIC int llc2rput(q, mp)
queue_t *q;
mblk_t  *mp;
{   /* This is the lower (read) put procedure */
    llc2dn_t *dnp = (llc2dn_t *)q->q_ptr;

    switch (mp->b_datap->db_type)
    {
#ifdef SGI
    case M_ERROR:
	DLP(("M_ERROR received\n"));
	goto llc2_r_err;
    case M_HANGUP:
	DLP(("M_HANGUP received\n"));
llc2_r_err:
	if (dnp)
	{
	    struct llc2up	*up = dnp->dn_uplist;
	    while (up) 
	    {
		if (up->up_readq)
		{
		    DLP(("send M_HANGUP\n"));
		    putctl(up->up_readq->q_next, M_HANGUP);
		}
		up = up->up_dnext;
	    }
	}
	break;
#endif

    case M_FLUSH:
	if (*mp->b_rptr & FLUSHR)
	{
	    *mp->b_rptr &= ~FLUSHR;

	    /* Flush public Rx queue */
	    flushq(q, FLUSHALL);

	    /* Flush private Rx queue */
	    if (dnp) flushrx(dnp);
	}

	if (*mp->b_rptr & FLUSHW)
	{
	    /* Flush write queue */
	    flushq(WR(q), FLUSHALL);

	    /* Flush stock messages */
	    llc2_flushstock();

	    /* Send back down write side */
	    qreply(q, mp);
	    return 0;
	}
	break;

    case M_PROTO:
    case M_PCPROTO:
	{   /* Registration reply or received MAC frame */
	    register union datal_proto FAR *pp =
				(union datal_proto FAR *)mp->b_rptr;

#ifdef SGI
	    if (!dnp)
		break;
#endif
	    switch (pp->type)
	    {
#if DL_VERSION >= 1
	    case DATAL_RESPONSE:
	        strlog(LLC_STID,0,9,SL_TRACE,"LLC2 - rput rcv DATAL_RESPONSE");
	        DLP(("LLC2 - rput rcv DATAL_RESPONSE\n"));
		/* MAC registration reply */

		/* Copy MAC details to 'down' structure */
		dnp->dn_mactype = pp->dlresp.mac_type;
		dnp->dn_maclen = pp->dlresp.addr_len;
		dnp->dn_frgsz = pp->dlresp.frgsz;
		llc2_maccpy((bufp_t)dnp->dn_macaddr,
			    (bufp_t)pp->dlresp.addr, pp->dlresp.addr_len);

		/* Schedule XID duplicate address check or skip it */
		dnp->dn_regstage = REGFROMMAC;
		llc2_schedule(&dnp->dn_station);

		break;
#else /* DL_VERSION */

	    case DATAL_VERSION0:
		/* MAC registration reply */
#ifdef ETH_PTYPE
            case ETH_PTYPE:
#endif
	        strlog(LLC_STID,0,9,SL_TRACE,"LLC2 - rput rcv ETH_PTYPE");
		/* Copy MAC address to 'down' structure */
		dnp->dn_mactype = DL_CSMACD;
		dnp->dn_maclen = DEFHWLEN;
		dnp->dn_frgsz = pp->dltype.frgsz;
		llc2_maccpy((bufp_t)dnp->dn_macaddr,
			    (bufp_t)pp->dltype.ethaddr, dnp->dn_maclen);

		/* Schedule XID duplicate address check or skip it */
		dnp->dn_regstage = REGFROMMAC;
		llc2_schedule(&dnp->dn_station);

		break;
#endif /* DL_VERSION */

	    case DATAL_RX:
#ifdef ETH_PACKET
            case ETH_PACKET:
#endif
	        strlog(LLC_STID,0,9,SL_TRACE,"LLC2 - rput rcv DATAL_RX");
		if (dnp->dn_trace_active)
			llc2_trace(dnp,mp,1);
		llc2_rxputq(dnp, mp);
		return 0;
	    case DATAL_ENAB_MULTI:
	    case DATAL_DISAB_MULTI:
	    case DATAL_GET_PHYS_ADDR:
	    case DATAL_GET_CUR_ADDR:
	    case DATAL_SET_CUR_ADDR:

	        strlog(LLC_STID,0,9,SL_TRACE,"LLC2 - rput rcv ADDR/MULTI");
		if (dnp)
		{
		    if (dnp->dn_dlpi_ack)
			freemsg(dnp->dn_dlpi_ack);
		    dnp->dn_dlpi_ack = mp;
		    llc2_schedule(&dnp->dn_station);
		    return 0;
		}
		break;
	    default:
	        strlog(LLC_STID,pp->type,0,SL_TRACE,"LLC2 - rput rcv Unknown pp type");
		/* Shouldn't happen ... */
		break;
	    }
	}
	break;

    default:
	strlog(LLC_STID,mp->b_datap->db_type,1,SL_ERROR,"LLC2 - rput rcv Unknown M type");
	/* Shouldn't happen ... */
	break;
    }

    /* Free message which has not been queued */
    freemsg(mp);
    return 0;
}

STATIC int llc2upwsrv(q)
queue_t *q;
{   /* Upper write queue service procedure */
    llc2up_t *upp = (llc2up_t *)q->q_ptr;
    mblk_t 	*mp;

    /* Check if we are waiting for buffers */
    if (upp && (mp = upp->up_waiting_buf))
    {
	upp->up_waiting_buf = NULL;
	(void) llc2wput_dlpi(q, mp);
    }
    return 0;
}

STATIC int llc2wsrv(q)
queue_t *q;
{   /* Lower write queue service procedure: operates state machine */

    /* The main work is done by functions in 'llc2serve.c' */
    llc2dn_t *dnp = (llc2dn_t *)q->q_ptr;

    if (dnp == NULL)
	return 0;

    /* Deal with input from LAN */
    llc2_macinsrv(dnp);

    /* Deal with sheduled connections */
    llc2_schedsrv(dnp);

    /* Deal with LLC1 data */
    llc2_udatasrv(dnp);
    return 0;
}

llc2_rxputq(dnp, mp)
register llc2dn_t *dnp;
mblk_t *mp;
{   /* Put message on receive queue */

    /* Check private queue high-water mark */
    if (dnp->dn_rxqcnt > llc2_rqhwm)
    {   /* Put message on public read-side queue to block flow */
	strlog(LLC_STID,0,9,SL_TRACE,"LLC2 - rxputq putq");
	putq(RD(dnp->dn_downq), mp);
	return 0;
    }

    /* Otherwise put on private queue */
    if (dnp->dn_rxqcnt++ == 0)
    {   /* First one: wake up state machine */
	strlog(LLC_STID,0,9,SL_TRACE,"LLC2 - rxputq qenable");
	dnp->dn_rxqueue = dnp->dn_rxqtail = mp;
	qenable(dnp->dn_downq);
	return 0;
    }

    /* Not first: add to tail */
    dnp->dn_rxqtail->b_next = mp;
    dnp->dn_rxqtail = mp;
    return 0;
}

void llc2_schedule(lp)
llc2cn_t *lp;
{   /* Schedule connection component for service by lower write queue */
    int s = splclock(); /* Interlock with clock ISR */
    register llc2dn_t *dnp = lp->dnp;

    if (dnp->dn_schq)
    {   /* There is at least one component already scheduled */
	if (!(lp->schqnext || dnp->dn_schqtail == lp))
	{   /* This component is not on the chain, so chain it on now */
	    dnp->dn_schqtail->schqnext = lp;
	    dnp->dn_schqtail = lp;
	}
    }
    else
    {   /* Chain was empty, so start with this component and enable queue */
	dnp->dn_schq = lp;
	dnp->dn_schqtail = lp;
	qenable(dnp->dn_downq);
    }

    /* Release interlock with clock ISR */
    splx(s);
}

/* Yield next connection in 'dnp's schedule queue */
llc2cn_t *llc2_schnext(dnp)
llc2dn_t *dnp;
{
    int s = splclock(); /* Interlock with clock ISR */
    register llc2cn_t *lp;

    if (lp = dnp->dn_schq)
    {   /* Queue is not empty: unchain top entry */
	dnp->dn_schq = lp->schqnext;
	lp->schqnext = (llc2cn_t *)0;
    }

    /* Release interlock with clock ISR */
    splx(s);

    /* Return pointer to top entry, or NULL if none */
    return lp;
}


#ifdef SGI
llc2up_t *llc2_findup(llc2up_t *uplist, uint8 sap, int class, int xid_or_test)
#else
llc2up_t *llc2_findup(uplist, sap, class, xid_or_test)
llc2up_t *uplist;
uint8 sap;
#endif
{   /* Look for network stream with right SAP */

    register llc2up_t *upp;

    /* DLPI "connection management" streams have sap == 0, so make sure
     * we don't accidentally find one. */
    if (sap == 0)
	return (llc2up_t *)0;

    for (upp = uplist; upp; upp = upp->up_dnext)
    {   /* Search through upper (network) streams */
	if (upp->up_nmlsap == sap ||
	    upp->up_lpbsap == sap)
	{
	    if (upp->up_class == class)
	    {
		if (class == LC_LLC1)
		    return upp;

		/* If this is an LLC2 stream, and we just need an 'upp'
		 * to handle an incoming XID or TEST, skip all the checking
		 * below for incoming SABMEs. */
		if (xid_or_test)
		    return upp;

		/* If this is an LLC2 stream, and its interface is DLPI,
		 * it can only accept an incoming connect if it is a listen
		 * stream (max_conind > 0), the number of outstanding
		 * connect indications (num_conind) is less than
		 * max_conind, and it is still listening for new
		 * connections (it hasn't accepted a connection).
		 */
		if (upp->up_interface != LLC_IF_DLPI ||
		    (upp->up_max_conind > 0 &&
		     upp->up_num_conind < upp->up_max_conind &&
		     (upp->up_dlpi_state == DL_IDLE ||
		      upp->up_dlpi_state == DL_INCON_PENDING)))
		    return upp;
	    }
	}
    }

    /* If haven't found a stream with the right SAP, and we need an LLC2
     * stream, look for a DLPI "connection management" up stream. */
    if (class == LC_LLC2)
	for (upp = uplist; upp; upp = upp->up_dnext)
	    if (upp->up_interface == LLC_IF_DLPI && upp->up_conn_mgmt)
		return upp;

    return (llc2up_t *)0;
}

llc2cn_t *llc2_connectup(upp)
llc2up_t *upp;
{
    register llc2cn_t *lp = (llc2cn_t *)0;
    register llc2dn_t *dnp;

    if (dnp = upp->up_dnp)
    {   /* Registered OK */

	/* Remove lp from spare list */
	if (lp = llc2_cnfree)
	{   /* Re-use one form free list */
	    llc2_cnfree = lp->next;
	}
	else
	{   /* No connection structure on free list */
	    static int nconn;

	    if (nconn < llc2_ncns)
	    {   /* Take a new one */
		lp = llc2_cntab + nconn++;
	    }
	}
    }

    if (lp)
    {   /* Got one */

	/* Clear and initialise the new connection structure */
	bzero((BUFP_T)lp, sizeof(llc2cn_t));
	llc2_cninit(lp);
	lp->dnp = dnp;

	/* Default window */
	llc2_settxwind(lp, dnp->dn_tunetab.tx_window);

	/* Add to 'upp's connection list */
	lp->upp = upp;
	lp->next = upp->up_cnlist;
	upp->up_cnlist = lp;
    }
    return lp;
}

llc2_disconnect(lp)
register llc2cn_t *lp;
{
    register llc2cn_t **hp;
    register mblk_t *mp;

    if (!lp->upp)
    {
	strlog(LLC_STID,0,0,SL_ERROR,"LLC2 - No UP pointer");
	return 0;
    }

    if (lp->upp->up_dnp)
    {
	/* Remove component from hash table if it's there */
	for (hp = lp->upp->up_dnp->dn_hashtab + (HASH(lp->dte)); *hp;
						    hp = &(*hp)->hnext)
	{
	    if (*hp == lp)
	    {
		*hp = lp->hnext;
		break;
	    }
	}
    }

    if (lp->upp->up_cnlist)
    {
	/* Remove component from up list if it's there */
	for (hp = &lp->upp->up_cnlist; *hp; hp = &(*hp)->next)
	{
	    if (*hp == lp)
	    {
		*hp = lp->next;
		break;
	    }
	}
    }

    /* Stop timers */
    stop_timers(&lp->thead, lp->tmtab, NTIMERS);

    /* Throw away any messages on the control queue */
    while (mp = lp->ctlq)
    {
	lp->ctlq = mp->b_next;
	mp->b_next = (mblk_t *)0;
	freemsg(mp);
    }

    /*  Reset the connection ID  */
    lp->connID = 0;

    /* Return to spare chain */
    lp->next = llc2_cnfree;
    llc2_cnfree = lp;
    return 0;
}

llc2_xidend(lp, xidin)
llc2cn_t *lp;
{
    if (lp == &lp->dnp->dn_station && lp->dte[0] == 0)
	lp->dnp->dn_regstage = xidin ? REGDUPFAIL : REGDONEOK;
    else
    {   /* Start Tx now that XID done */
	if (lp->state == RUN && --lp->txstopped == 0)
	    llc2_schedule(lp);
    }
    return 0;
}

#ifdef SGI
llc2_maccpy(BUFP_T d, BUFP_T s, uint8 len)
#else
llc2_maccpy(d, s, len)
BUFP_T d, s;
uint8 len;
#endif
{
    /* Copy MAC address from *s to *d */
    bcopy (s, d, len);
    return 0;
}

int llc2_dtecpy(lp, upp, pp, dnmac)
register llc2cn_t *lp;
llc2up_t *upp;
register struct ll_msgc FAR *pp;
uint8 dnmac[];
{
    /* Check local LSAP */
    if (pp->ll_locsize != 14)
    {	/* If length zero, default to normal listen SAP */
    	if (pp->ll_locsize != 0) return LS_LSAPWRONG;
    	pp->ll_locsize = 14;
    	pp->ll_locaddr[6] = upp->up_nmlsap;
    }

    /* Check remote LSAP */
    if (pp->ll_remsize != 14)
    {	/* If length zero, default to loopback SAP */
    	if (pp->ll_remsize != 0) return LS_LSAPWRONG;
    	pp->ll_remaddr[6] = upp->up_lpbsap;
    }

    /* Copy SAPs (but zero SAPs are not allowed) */
    if ((lp->dte[0] = pp->ll_locaddr[6]) == 0 ||
    	(lp->dte[1] = pp->ll_remaddr[6]) == 0) return LS_LSAPWRONG;

    /* Check for missing or zero MAC address */
    if (pp->ll_remsize == 0 ||
	*(uint16 FAR *)(pp->ll_remaddr+0) == (uint16)0 &&
	*(uint16 FAR *)(pp->ll_remaddr+2) == (uint16)0 &&
	*(uint16 FAR *)(pp->ll_remaddr+4) == (uint16)0 )
    {	/* Mac address is zero: use station MAC address */
    	*(uint16 FAR *)(pp->ll_remaddr+0) = *(uint16 *)(dnmac+0);
    	*(uint16 FAR *)(pp->ll_remaddr+2) = *(uint16 *)(dnmac+2);
	*(uint16 FAR *)(pp->ll_remaddr+4) = *(uint16 *)(dnmac+4);
	pp->ll_remsize = 14;
    }

    /* Override (ignore) local MAC address */
    *(uint16 FAR *)(pp->ll_locaddr+0) = *(uint16 *)(dnmac+0);
    *(uint16 FAR *)(pp->ll_locaddr+2) = *(uint16 *)(dnmac+2);
    *(uint16 FAR *)(pp->ll_locaddr+4) = *(uint16 *)(dnmac+4);

    /* Copy in destination MAC address */
    *(uint16 *)(lp->dte+2) = *(uint16 FAR *)(pp->ll_remaddr+0);
    *(uint16 *)(lp->dte+4) = *(uint16 FAR *)(pp->ll_remaddr+2);
    *(uint16 *)(lp->dte+6) = *(uint16 FAR *)(pp->ll_remaddr+4);

    /*
     * clear out any source route
     */
    lp->routelen = 0;

    /* Check for loopback */
    if (*(uint16 FAR *)(pp->ll_remaddr+0) == *(uint16 *)(dnmac+0) &&
	*(uint16 FAR *)(pp->ll_remaddr+2) == *(uint16 *)(dnmac+2) &&
	*(uint16 FAR *)(pp->ll_remaddr+4) == *(uint16 *)(dnmac+4))
    {   /* Loopback: SAPs must be different - unless it's LLC1 */
	if ( (lp->dte[0] == lp->dte[1]) && (upp->up_class != LC_LLC1) )
	    return LS_LSAPINUSE;
	lp->loopback = 1;
    }
    else
    {
	lp->loopback = 0;
    }

    /* OK: format OK and not loopback with SSAP == DSAP */
    return 0;
}

#ifdef SGI
closeup(upp, disable_multi)
int disable_multi;
#else
closeup(upp)
#endif
register llc2up_t *upp;
{
    llc2up_t *lupp, **hup;
    llc2dn_t *dnp;

    if (upp && (dnp = upp->up_dnp))
    {   /* Upper stream was registered OK: close all associated connections */
	llc2cn_t *lp;

#ifdef SGI
	if ((disable_multi) && (upp->up_multi_count))
	    disable_all_multicast(upp);
#else
	if (upp->up_multi_count)
	    disable_all_multicast(upp);
#endif

	if (dnp->llc1_match)
	{
#ifdef SGI
		unregister_q(dnp->llc1_match, upp->up_readq, dnp->dn_downq, 1);
#endif
		deregister_q(dnp->llc1_match, upp->up_readq);
	}
	if (dnp->eth_match)
	{
#ifdef SGI
		unregister_q(dnp->eth_match, upp->up_readq, dnp->dn_downq, 2);
#endif
		deregister_q(dnp->eth_match, upp->up_readq);
	}
	if (upp->up_xid_msg)
	{
	    freemsg(upp->up_xid_msg);
	    upp->up_xid_msg = NULL;
	}

	if (upp->up_timeout)
	{
		(void) untimeout(upp->up_timeout);
		upp->up_timeout = 0;
	}
#ifdef SVR4
	if (upp->up_bufcall)
	{
		(void) unbufcall(upp->up_bufcall);
		upp->up_bufcall = 0;
	}
#endif
	while (lp = upp->up_cnlist)
	{
	    llc2_stop(lp);
	    llc2_disconnect(lp);
	}
#ifdef SGI
	upp->up_dlpi_state = DL_UNBOUND;
#endif

	for (hup = &dnp->dn_uplist; lupp = *hup; hup = &lupp->up_dnext)
	{   /* remove from down's uplist */
	    if (lupp == upp)
	    {
		*hup = upp->up_dnext;
		break;
	    }
	}
    }
    return 0;
}


/*
 * Disable all multicast addresses that were enabled for the stream.
 * Send a DL_DISABLMULTI_REQ message down to the LAN driver for every
 * registered multicast address.
 */
static
disable_all_multicast(upp)
llc2up_t *upp;
{
	int 			i;
	mblk_t			*bp;
	struct datal_address	*dis_req;
	int			len;

	len = upp->up_dnp->dn_maclen;
	for (i = 0; i < (int)upp->up_multi_count; i++) {
		bp  = allocb(len + (int)sizeof(*dis_req), BPRI_HI);
		if (!bp)
			return 0;
		bp->b_wptr += len + sizeof(*dis_req);
		bp->b_datap->db_type = M_PROTO;
		dis_req = (struct datal_address*)bp->b_rptr;
		dis_req->prim_type = DATAL_DISAB_MULTI;
		dis_req->addr_len = len;

		bcopy (&upp->up_multi_tab[i], dis_req->addr,
			upp->up_dnp->dn_maclen);
		putnext(upp->up_dnp->dn_downq, bp);
	}
	upp->up_multi_count = 0;
	return 0;
}


/* Close all connections on dependent UP streams */
static closedn(dnp)
llc2dn_t *dnp;
{
    llc2up_t *upp;
    while (upp = dnp->dn_uplist) 
    {
#ifdef SGI
	if (upp->up_readq)
	{
	    DLP(("closedn: Send M_ERROR\n"));
	    putctl1(upp->up_readq->q_next,M_ERROR,EIO);
	}
#endif
    	closeup(upp,1);
    }
    return 0;
}


static flushrx(dnp)
llc2dn_t *dnp;
{
    while (dnp->dn_rxqcnt)
    {
	mblk_t *mp;

	dnp->dn_rxqcnt--;
	mp = dnp->dn_rxqueue;
	dnp->dn_rxqueue = mp->b_next;
	mp->b_next = (mblk_t *)0;
	freemsg(mp);
    }
    return 0;
}


/*
Deactivates tracing on any subnetwork whose tracestream matches
that which is closing down.  Returns number of subnetworks processed.
*/

static close_llc2_tracestrm(q)
queue_t  *q;
{
    llc2dn_t *dnp;
    int n;
    int snid_found = 0;
			
    for (n = llc2_ndns, dnp = llc2_dntab; n--; dnp++)
    {
	if (dnp->dn_trace_active != 0)
	{
	    if (dnp->dn_trace_queue == q)
	    {
		snid_found++;
		dnp->dn_trace_active = 0;
		dnp->dn_trace_queue  = 0;
	    }
	}
    }
    return(snid_found);
}

			
llc2_trace(dnp,mp,received)
llc2dn_t *dnp;
mblk_t *mp;
int received;
{
    register mblk_t *cmp;
    mblk_t *dmp;
    
    trc_types FAR *trc_msg;
    union datal_proto FAR *pp = (union datal_proto FAR *)mp->b_rptr;
    unsigned int size = sizeof(trc_types);

    static unsigned int trace_seq = 0;

    trace_seq++;

    if (dnp->dn_maclen > sizeof(trc_msg->trc_llc2dat.trc_rmt))
	size += dnp->dn_maclen - sizeof(trc_msg->trc_llc2dat.trc_rmt);
    
    if (canput(dnp->dn_trace_queue->q_next))
    {
	if ((cmp = allocb(size, BPRI_LO)))
	{
	    if (dmp = copymsg(mp->b_cont))
	    {
	    	cmp->b_datap->db_type = M_PROTO;
	        cmp->b_wptr += size;

		trc_msg = (trc_types FAR *)cmp->b_rptr;

		trc_msg->trc_prim = TR_LLC2_DAT;
		trc_msg->trc_llc2dat.trc_seq  = trace_seq;
		trc_msg->trc_llc2dat.trc_snid = dnp->dn_snid;
		trc_msg->trc_llc2dat.trc_time = lbolt;
		trc_msg->trc_llc2dat.trc_rcv  = received;
		trc_msg->trc_llc2dat.trc_mid  = LLC_STID;

		bcopy( (received) ? (BUFP_T)pp->dlrx.src
			          : (BUFP_T)pp->dltx.dst,
		                    (BUFP_T)trc_msg->trc_llc2dat.trc_rmt,
				    dnp->dn_maclen);
		
		cmp->b_cont = dmp;
		
	        putnext(dnp->dn_trace_queue,cmp);
	    }
	    else
	        freemsg(cmp);
	}
    }
    return 0;
}

int
llc2unload()
{
	int	device;

	for (device = 0; device < llc2_ndevs; device++)
		if (llc2_devtab[device])
			return(EBUSY);
	return(0);
}
