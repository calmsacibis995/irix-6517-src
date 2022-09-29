 /******************************************************************
 *
 *  SpiderX25 - LLC2 Multiplexer
 *
 *  Copyright 1991 Spider Systems Limited
 *
 *  LLC2SERVE.C
 *
 *    Service routines for LLC2 Multiplexer.
 *
 ******************************************************************/

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/sys/llc2/86/s.llc2serve.c
 *	@(#)llc2serve.c	1.56
 *
 *	Last delta created	16:54:41 5/28/93
 *	This file extracted	16:55:19 5/28/93
 *
 */

#ident "@(#)llc2serve.c	1.56 (Spider) 5/28/93"

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
#include <sys/sysmacros.h>
#include <sys/strlog.h>
#include <sys/snet/uint.h>
#include <sys/snet/system.h>
#include <sys/snet/ll_proto.h>
#include <sys/snet/ll_control.h>
#include <sys/dlpi.h>
#include <sys/snet/dl_proto.h>
#include <sys/snet/timer.h>
/* #include "sys/snet/llc2match_if.h" */
#include "sys/snet/llc2.h"


#ifdef SVR4
extern mblk_t *getq(), *copyb(), *dupmsg();
#else
extern mblk_t *getq(), *allocb(), *copyb(), *dupmsg();
#endif

#ifdef SGI
static fillmsgc(struct ll_msgc FAR *,caddr_t,uint8 *,uint8 *,int);
static int getstock(llc2dn_t*,mblk_t*,int);
static int nobuf(llc2dn_t *, int);
extern int llc2_rxframe(register llc2cn_t*);
extern int llc2_control_req(register llc2cn_t*,uint8);
extern llc2up_t *llc2_findup(llc2up_t*, uint8, int, int);
extern llc2cn_t *llc2_connectup(llc2up_t*);
extern llc2cn_t *llc2_schnext(llc2dn_t*);
extern int llc2_expired(register llc2cn_t*, unchar);
extern int dlokack(queue_t *, mblk_t *, int);
extern int llc2_sendxid(llc2cn_t*);
extern int llc2_sendinfo(register llc2cn_t *);
extern int llc2_txack(register llc2cn_t *);
extern int llc2_sendudata(llc2cn_t *);
/* extern from llc2dlpi.c */
extern int llc2_dlpi_dtecpy(llc2cn_t *, llc2up_t *, mblk_t *, uint8 *,int);
extern void reply_uderror(queue_t *, mblk_t *, int);
extern int dlerrorack(queue_t*, mblk_t*, dlu32_t, dlu32_t, dlu32_t);
extern void llc2_dlpi_tonet(llc2cn_t *, int, int, mblk_t *);

#else
extern llc2up_t *llc2_findup();
extern llc2cn_t *llc2_connectup();
extern llc2cn_t *llc2_schnext();
extern int dlerrorack(), dlokack();
#endif


/* Counts from 'space.c' */
extern uint         llc2_rqhwm;
extern uint         llc2_rqlwm;

/* Bufcall control */
/* static nostock; */

/* Stock buffer pointers */
mblk_t *llc2_upmp;
mblk_t *llc2_dnmp;
mblk_t *llc2_rxmp;

/*
 *  'llc2_macinsrv' services messages from the Ethernet.
 */
llc2_macinsrv(dnp)
llc2dn_t *dnp;
{
    register queue_t *rq = RD(dnp->dn_downq);

    strlog(LLC_STID,dnp->dn_snid,8,SL_TRACE,"LLC - llc2_macinsrv");
    DLP(("Entering llc2_macinsrv, dn_mactype=%d\n",dnp->dn_mactype));
    for (;;)
    {
	/* Frames have already been adjusted for length, and marked as  */
	/* having either a direct connection handle or else a DTE key.  */

	register llc2cn_t *lp = (llc2cn_t *)0;
	register mblk_t *mp;
	bufp_t fp;
        unsigned ucmd;
	uint8 FAR *dte;
	llc2cn_t **hashp;
	int upsize;
	S_DATAL_RX *dlrx;
	uint8 *macsrc, *macdst;
#if DL_VERSION >= 2
	uint8 *route;
#endif

	if (dnp->dn_rxqcnt)
	{   /* Take from head of private queue first */
	    dnp->dn_rxqcnt--;
	    mp = dnp->dn_rxqueue;
	    dnp->dn_rxqueue = mp->b_next;
	    mp->b_next = (mblk_t *)0;
	    strlog(LLC_STID,dnp->dn_snid,6,SL_TRACE,
		"LLC - get mp from rxqueue");
	}
	else
	{   /* Try public queue instead: break if none left */
	    if (!(mp = getq(rq)))
		break;
	}

	dlrx = ((struct datal_rx FAR *)mp->b_rptr);
	DLP(("\tdlrx=%x, prim_type=%d,maclen=%d\n",
		dlrx->prim_type,dlrx->type,dnp->dn_maclen));
	if (dlrx->prim_type == DATAL_RX)
	{
	    strlog(LLC_STID,dnp->dn_snid,9,SL_TRACE,
		"LLC - it's a DATAL_RX");
		DLP(("\tIt is a DATAL_RX\n"));
		macsrc = dlrx->src;
#if DL_VERSION >= 2
		macdst = dlrx->src + dnp->dn_maclen;
		route = macdst + dnp->dn_maclen;
#else
		macdst = dnp->dn_macaddr;
#endif
	}
	else
	{
		DLP(("\tIt is NOT a DATAL_RX\n"));
		macsrc = dnp->dn_macaddr;
		macdst = ((struct datal_tx FAR *)mp->b_rptr)->dst;
#if DL_VERSION >= 2
		route = macdst + dnp->dn_maclen;
#endif
	}
	if (dlrx->type > 1500 && (dnp->dn_mactype == DL_ETHER ||
		dnp->dn_mactype == DL_CSMACD))
	{
		/* an Ethernet packet */
		uint8 type[2];
		queue_t *uq;
		dl_unitdata_ind_t *data_ind;
		int size;
		mblk_t *ump;

		DLP(("\tGot an Ethernet packet\n"));
		/* put the type in network order */
		type[0] = dlrx->type >> 8;
		type[1] = dlrx->type & 0xFF;

		if (mp->b_cont->b_cont && pullupmsg(mp->b_cont, -1) == 0)
		{
		    LLC2_PRINTF("llc_macinsrv: pullupmsg failed (1)\n", 0);
		    /* pull-up failed */
		    freemsg(mp);
		    continue;
		}
			
		uq = lookup_sap(dnp->eth_match, (uint16 *)0, type, 2, 
			mp->b_cont->b_rptr,(unsigned short)(mp->b_cont->b_wptr-
			mp->b_cont->b_rptr));

		if (uq == (queue_t *)0)
		{
			/* no matching entry */
			LLC2_PRINTF("llc_macinsrv: ether sap not found\n", 0);
		        freemsg(mp);
		        continue;
		}

#if DL_VERSION >= 2
		if (dlrx->addr_length != dnp->dn_maclen)
		{
		    LLC2_PRINTF("llc_macinsrv: bad addr len (%d) (1)\n",
			dlrx->addr_length);
		    freemsg(mp);
		    continue;
		}
#endif
		size = (int)sizeof(dl_unitdata_ind_t) +
#if DL_VERSION >= 2
		    dlrx->route_length +
#endif
		    (dnp->dn_maclen + 2) * 2;

		ump = allocb(size, BPRI_MED);
		if (ump == (mblk_t *)0)
		{
		    LLC2_PRINTF("llc_macinsrv: allocb (%d) failed (1)\n",
			size);
		    freemsg(mp);
		    continue;
	        }
	        /*
	         * Send DL_UNITDATA_IND message upstream.
	         */
	        ump->b_datap->db_type = M_PROTO;
	        ump->b_wptr += size;
	        data_ind = (dl_unitdata_ind_t *) ump->b_rptr;
	        data_ind->dl_primitive = DL_UNITDATA_IND;

	        /*
	         * Set destination addr.
	         */
	        data_ind->dl_dest_addr_length = dnp->dn_maclen + 2;
	        data_ind->dl_dest_addr_offset = sizeof(dl_unitdata_ind_t);

	        /* copy the MAC address */
	        bcopy((char *)macdst,
		    (char *)data_ind + data_ind->dl_dest_addr_offset,
		    dnp->dn_maclen);

	        /* copy the Ethernet type */
	        bcopy((char *) type, (char *)data_ind +
		    data_ind->dl_dest_addr_offset + dnp->dn_maclen, 2);
			
	        /*
	         * Set source addr.
	         */
#if DL_VERSION >= 2
		data_ind->dl_src_addr_length = dnp->dn_maclen + 2 +
		    dlrx->route_length;
#else
		data_ind->dl_src_addr_length = dnp->dn_maclen + 2;
#endif
		data_ind->dl_src_addr_offset = 
		    data_ind->dl_dest_addr_offset +
			data_ind->dl_dest_addr_length;

		/* copy the MAC address */
		bcopy((char *)macsrc,
		    (char *)data_ind + data_ind->dl_src_addr_offset,
		    dnp->dn_maclen);

		/* copy the Ethernet type */
		bcopy((char *) type, (char *)data_ind +
		    data_ind->dl_src_addr_offset + dnp->dn_maclen, 2);

#if DL_VERSION >= 2
		if (dlrx->route_length)
		{
		    /* copy the source routing information */
		    bcopy((char *)route,
		        (char *)data_ind + data_ind->dl_src_addr_offset +
		        dnp->dn_maclen + 2, dlrx->route_length);
		}
#endif

		data_ind->dl_group_address = macsrc[0] & 0x01;

		ump->b_cont = mp->b_cont;
		mp->b_cont = (mblk_t *)0;
		freeb(mp);
		putnext(uq, ump);
		continue;
	}
#ifdef SGI
	strlog(LLC_STID,dnp->dn_snid,9,SL_TRACE, "LLC - Not ethernet");
	if (mp->b_cont->b_cont && pullupmsg(mp->b_cont, -1) == 0) {
	    LLC2_PRINTF("llc_macinsrv: pullupmsg failed (1)\n", 0);
	    strlog(LLC_STID,dnp->dn_snid,2,SL_TRACE,
		"llc_macinsrv: pullupmsg failed (1)");
	    DLP(("\tNon-ether: pullupmsg failed\n"));
	    /* pull-up failed */
	    freemsg(mp);
	    continue;
	}
#endif
	dte = ((struct datal_rx FAR *)mp->b_rptr)->src - 2;
#ifdef SGI
	if (dnp->dn_mactype == DL_TPR) {
	    mp->b_cont->b_rptr += (dlrx->route_length+dlrx->addr_length*2 + 2);
	}
#endif
	fp = mp->b_cont->b_rptr;

	/*  Check frames for minimum length  */
	if (mp->b_cont->b_wptr - fp < 3)
	{
	    freemsg(mp);
	    strlog(LLC_STID,dnp->dn_snid,2,SL_TRACE,
		"LLC - bad length, freemsg");
	    DLP(("\tBad length %d, freemsg\n",mp->b_cont->b_wptr - fp));
	    continue;
	}


	/* 'ucmd' is set so that it is equal to 'SABME' or 'UI' */
	/*   only when the control value is correct for the     */
	/*   corresponding U-command.                           */
	ucmd = ~(fp[1] & 1) & (fp[2] & ~UPF); 

	/* Make up complete connection key using SAPs from frame */
	dte[0] = fp[0];
	dte[1] = fp[1] & ~1;

	if (ucmd == UI ||
	    (dte[0] == 0xff && dte[1] == 0xfe))
	{   /* LLC1 data: send it upwards */
	    /* (0xff is Netware "raw" SAP, which must be LLC1.) */

	    llc2up_t *upp;
	    mblk_t *ump = (mblk_t *)0;
	    struct ll_msgc FAR *pp;
	    queue_t *urq;
	    /* netware mode has a 6 byte address */
	    uint8 addrlen = dnp->dn_maclen + (dte[0] != 0xff);
#ifdef ETH_PACKET
            struct eth_packet FAR *ep = (struct eth_packet FAR *)mp->b_rptr;
#endif /* ETH_PACKET */

	   if (mp->b_cont->b_cont && pullupmsg(mp->b_cont, -1) == 0)
	   {
		LLC2_PRINTF("llc_macinsrv: pullupmsg failed (1)\n", 0);
		/* pull-up failed */
		freemsg(mp);
		continue;
	    }
			
	    /*
	     * do a look-up, skipping over the 3 byte header
	     */
	    urq = lookup_sap(dnp->llc1_match, (uint16 *)0, dte, 1, 
		mp->b_cont->b_rptr + 3, (ushort)(mp->b_cont->b_wptr -
		mp->b_cont->b_rptr - 3));

	    upp = (llc2up_t *) (urq ? WR(urq)->q_ptr : 0);
	    upsize = (int)(sizeof(dl_unitdata_ind_t)) + 2 * addrlen;
#if DL_VERSION >= 2
	    upsize += dlrx->route_length;
#endif
	    if (!upp ||
	        !(ump = allocb(upsize, BPRI_MED)) ||
		!canput((urq = upp->up_readq)->q_next))
	    {   /* Can't get rid of it, so discard */
		if (ump) freeb(ump);
	    }
	    else
	    {   /* Fill in message and post upwards */
		if (upp->up_interface == LLC_IF_DLPI) {
		    dl_unitdata_ind_t *data_ind;

		    /*
		     * Send DL_UNITDATA_IND message upstream.
		     */
		    ump->b_datap->db_type = M_PROTO;
		    ump->b_wptr += upsize;
		    data_ind = (dl_unitdata_ind_t *) ump->b_rptr;
		    data_ind->dl_primitive = DL_UNITDATA_IND;

		    /*
		     * Set destination (or local) addr.
		     */
		    data_ind->dl_dest_addr_length = addrlen;
		    data_ind->dl_dest_addr_offset = sizeof(*data_ind);
		    {
			uint8 *addr = ump->b_rptr +
				data_ind->dl_dest_addr_offset;

			bcopy(macdst, addr, dnp->dn_maclen);
			if (dte[0] != 0xFF)
				addr[dnp->dn_maclen] = dte[0];
			data_ind->dl_group_address = addr[0] & 0x01;
		    }

		    /*
		     * Set calling (or remote) addr.
		     */
		    data_ind->dl_src_addr_length = addrlen;
		    data_ind->dl_src_addr_offset = (uint)(sizeof(*data_ind)) +
							addrlen;
		    {
			uint8 *addr = ump->b_rptr +
				data_ind->dl_src_addr_offset;

			bcopy(macsrc, addr, dnp->dn_maclen);
			if (fp[1] != 0xFF)
				addr[dnp->dn_maclen] = dte[1];
#if DL_VERSION >= 2
			if (dlrx->route_length)
			{
			    bcopy(route, addr + addrlen, dlrx->route_length);
			    data_ind->dl_src_addr_length += dlrx->route_length;
			}
#endif
		    }
		    ump->b_cont = mp->b_cont;
		    mp->b_cont = NULL;
		    /* The frame is passed up untouched if it's addressed
		     * to the Netware "raw" mode SAP (0xff). */
		    if (dte[0] != 0xff)
			adjmsg(ump->b_cont, 3); /* remove 3 UI bytes */
		    putnext(upp->up_readq, ump);

		}
		else
		{
		    bzero((BUFP_T)ump->b_wptr, sizeof(struct ll_msgc));
		    pp = (struct ll_msgc FAR *)ump->b_wptr;
		    ump->b_wptr += sizeof(struct ll_msgc);
		    ump->b_datap->db_type = M_PROTO;
		    pp->ll_type = LL_DAT;
		    pp->ll_command = LC_UDATA;
#if DL_VERSION >= 2
		    fillmsgc(pp, (caddr_t)0, dte, macdst, dnp->dn_maclen);
#else
#ifdef ETH_PACKET
		    if (ep->prim_type == ETH_PACKET)
			fillmsgc(pp, (caddr_t)0, dte, ep->eth_dst,
				dnp->dn_maclen);
		    else
			fillmsgc(pp, (caddr_t)0, dte, dnp->dn_macaddr, 
				dnp->dn_maclen);
#else /* ETH_PACKET */
		    fillmsgc(pp, (caddr_t)0, dte, dnp->dn_macaddr, 
				dnp->dn_maclen);
#endif /* ETH_PACKET */
#endif /* DL_VERSION >= 2 */
		    adjmsg(ump->b_cont = mp->b_cont, 3); /* remove 3 UI bytes */
		    mp->b_cont = (mblk_t *)0;
		    putnext(urq, ump);
		}
	    }
	    /* Free unused parts of message */
	    freemsg(mp);
	    continue;
	}

	/*
	 * LLC2 or oddball LLC1. Default size NORMAL_UPSIZE unless SABME
	 * which may generate an 'll_msgc'/DL_CONNECT_IND message
	 */
	if (ucmd == SABME)
	    upsize = CONNECT_UPSIZE;
	else
	    upsize = NORMAL_UPSIZE;
#ifdef SGI
	dte[2] &= 0x7f;	/* make sure source routing bit is off */
	HEXDUMP(dte, dnp->dn_maclen + 2);
#endif

	/* Look up in hash table */
	strlog(LLC_STID,dnp->dn_snid,9,SL_TRACE,
	    "LLC - Look up in hash table");
	hashp = dnp->dn_hashtab + (HASH(dte));
	DLP(("\tLook up in hash table %p, index=%d\n",dnp->dn_hashtab,
		(HASH(dte)) ));
	for (lp = *hashp; lp; lp = lp->hnext)
	{
	    if (lp->state >= ADM  &&
		(bcmp((BUFP_T)dte, (BUFP_T)lp->dte, dnp->dn_maclen + 2) == 0))
		    /* Found */
		    break;
	    HEXDUMP(lp->dte, dnp->dn_maclen + 2);
	}

	if (!lp && ucmd == SABME)
	{   /* SABME command to new connection => incoming CONNECT */
	    llc2up_t *upp;

	    strlog(LLC_STID,dnp->dn_snid,8,SL_TRACE,
		"LLC - incoming SABME w/o llc2cn");
	    DLP(("macin: incoming SABME w/o llc2cn\n"));
	    if (upp = llc2_findup(dnp->dn_uplist, dte[0], LC_LLC2, 0))
	    {   /* Found an upward stream for the DSAP */
		DLP(("\tfound an upward stream for the DSAP\n"));
		if (lp = llc2_connectup(upp))
		{   /* Got a new connection block */
		    DLP(("\tGot a new connection block\n"));
		    /* Put into hash table */
		    lp->hnext = *hashp;
		    *hashp = lp;

		    /* Tell ADM state it's a good new connect */
		    lp->sflag = 1;

		    /* Loopback if looped back by 'llc2_tomac' */
		    lp->loopback = (((struct datal_rx FAR *)mp->b_rptr)
		    				->prim_type == DATAL_TX);
		}
	    }
	}

	if (!lp)
	{
	    /* Not SABME, SAP not found (or 0), no structure, or dying */
	    /* Use the station component (with sflag=0) */
	    strlog(LLC_STID,dnp->dn_snid,9,SL_TRACE,
		"LLC - failed to find matched llc2cn");
	    DLP(("\tFailed to find mactched llc2cn\n"));
	    lp = &dnp->dn_station;
	    lp->sflag = 0;
	    /* Loopback if looped back by 'llc2_tomac' */
	    lp->loopback = (uint8)(((union datal_proto FAR *)
				mp->b_rptr)->type == DATAL_TX);
	}
 
	/* Copy address information into connection structure */
	strlog(LLC_STID,dnp->dn_snid,7,SL_TRACE,
	    "LLC - Copy address info");
	DLP(("\tCopy address info to llc2cn %p\n",lp));
	bcopy((BUFP_T)dte, (BUFP_T)lp->dte, dnp->dn_maclen + 2);
#if DL_VERSION >= 2
	/* copy routing information into connection structure */
	if (dlrx->route_length >= MINRTLEN && dlrx->route_length <= MAXRTLEN)
	{
	    lp->routelen = dlrx->route_length;
	    bcopy(route, lp->route, lp->routelen);
	    TR_COMPLEMENT_DBIT(lp->route);
	}
	else
	    lp->routelen = 0;
#endif

	if (getstock(dnp, (mblk_t *)0, upsize))
	{   /* Not blocked, and got everything we need */

	    /* Detach frame data from message */
	    llc2_rxmp = mp->b_cont;
	    mp->b_cont = (mblk_t *)0;
	    freeb(mp);

	    strlog(LLC_STID,dnp->dn_snid,7,SL_TRACE,
		"LLC - crank state machine");
	    DLP(("\tcrank state machine, lp=%p\n",lp));
	    if (llc2_rxmp)
	    {   /* Crank state machine */
		llc2_rxframe(lp);
	    }

	    /* Free frame data if unused */
	    if (mp = llc2_rxmp)
	    {
		freemsg(mp);
		llc2_rxmp = (mblk_t *)0;
	    }

	    /* Back for next frame */
	    continue;
	}

	strlog(LLC_STID,dnp->dn_snid,7,SL_TRACE,
	    "LLC - put unused msg back");
	/* Put unused message back on private queue */
	if (dnp->dn_rxqcnt++ == 0)
	{   /* First entry */
	    dnp->dn_rxqueue = dnp->dn_rxqtail = mp;
	}
	else
	{   /* Put back at queue head */
	    mp->b_next = dnp->dn_rxqueue;
	    dnp->dn_rxqueue = mp;
	}

	/* If private queue at LWM, fill from public queue */
	if (dnp->dn_rxqcnt < llc2_rqlwm)
	{
	    while (dnp->dn_rxqcnt < llc2_rqhwm)
	    {
		if (mp = getq(rq))
		{
		    dnp->dn_rxqcnt++;
		    dnp->dn_rxqtail->b_next = mp;
		    dnp->dn_rxqtail = mp;
		    continue;
		}

		/* Public queue now empty: leave loop */
		break;
	    }
	}

	/* Leave loop */
	break;
    }
    return 0;
}

/*
 *  'llc2_schedsrv' services scheduled connections.
 */
llc2_schedsrv(dnp)
llc2dn_t *dnp;
{
    register llc2cn_t *lp;
    register mblk_t *mp;

    while (lp = llc2_schnext(dnp))
    {   /* Go through scheduled components */
	x25_timer_t *tp;

	/* First, deal with control commands */
	while (mp = lp->ctlq)
	{   /* Work through control queue */
	    struct ll_msg FAR *pp = (struct ll_msg FAR *)mp->b_rptr;
	    int		size;

	    /* Make sure flow is OK and we have all the buffers we need */
	    size = (pp->ll_command == LC_CONNECT) ?
			CONNECT_UPSIZE : NORMAL_UPSIZE;
	    if (!getstock(dnp, (mblk_t *)0, size)) goto stop;

	    /* Call state M/C */
	    llc2_control_req(lp, pp->ll_command);

	    /* Remove message from queue */
	    lp->ctlq = mp->b_next;
	    mp->b_next = (mblk_t *)0;
	    freemsg(mp);
	}

	/* Next, deal with expired timers */
	while (tp = lp->thead.th_expired)
	{   /* Timer on expired list */

	    /* Make sure flow is OK and we have all the buffers we need */
	    if (!getstock(dnp, (mblk_t *)0, NORMAL_UPSIZE)) goto stop;

	    /* Remove timer from expired list, then call state M/C */
	    set_timer(tp, (ushort)0);
	    llc2_expired(lp, tp->tm_id);
	}

	/* Next, if station, deal with messages from the LAN driver
	 * (registration stage messages and acks to DL_ENAB/DISAB_MULTI). */
	if (lp == &dnp->dn_station)
	{
	    int skip_reg_check = 0;

	    /* Check for ack to DL_ENAB/DISAB_MULTI_REQ. */
	    if (dnp->dn_dlpi_ack)
	    {
		llc2up_t *upp;
		mblk_t *bp;
		int n;
		extern llc2up_t *llc2_uptab;
		extern unsigned int llc2_nups;

		/* Look for an up stream waiting for an ack from the LAN
		 * driver.  If there isn't one, drop the message. */
		for (n=llc2_nups, upp = llc2_uptab; n--; upp++)
		{
		    if (!upp->up_readq || upp->up_dnp != dnp)
			continue;

		    if (upp->up_waiting_ack == DL_PHYS_ADDR_REQ)
		    {
			dl_error_ack_t  	*err_ack;
			dl_phys_addr_ack_t	*phys_ack;
			struct datal_address    *datal_ack;
			mblk_t          	*ea;

			bp = dnp->dn_dlpi_ack;
			dnp->dn_dlpi_ack = NULL;
			datal_ack = (struct datal_address *) bp->b_rptr;
			if (datal_ack->error_field)
                        {   /* Construct error_ack */
			    ea = allocb((int)sizeof(dl_error_ack_t), BPRI_HI);
                            if (!ea)
                            {
				dnp->dn_dlpi_ack = bp;
                                goto stop;
                            }
			    err_ack = (dl_error_ack_t *) ea->b_rptr;
			    ea->b_wptr += sizeof(dl_error_ack_t);
			    ea->b_datap->db_type = M_PCPROTO;

			    /* Construct error_ack */	
			    err_ack->dl_primitive = DL_ERROR_ACK;
			    err_ack->dl_error_primitive = DL_PHYS_ADDR_REQ;
			    err_ack->dl_errno = DL_SYSERR;
			    err_ack->dl_unix_errno = datal_ack->error_field;

			    freemsg(bp);
			    upp->up_waiting_ack = 0;
			    putnext(upp->up_readq, ea);			
			}
			else
			{   /* Construct dl_phys_addr_ack */
			    ea = allocb((int)sizeof(dl_phys_addr_ack_t) +
				 datal_ack->addr_len, BPRI_HI);
			    if (!ea)
			    {
				dnp->dn_dlpi_ack = bp;
				goto stop;
			    }
			    phys_ack = (dl_phys_addr_ack_t *) ea->b_rptr;
			    ea->b_datap->db_type = M_PCPROTO;

			    /* Construct phys_ack */
			    phys_ack->dl_primitive = DL_PHYS_ADDR_ACK;
			    phys_ack->dl_addr_length = datal_ack->addr_len;
			    phys_ack->dl_addr_offset =sizeof(dl_phys_addr_ack_t);
			    bcopy (datal_ack->addr, 
				   ea->b_rptr + sizeof(dl_phys_addr_ack_t), 
				   datal_ack->addr_len);
			    ea->b_wptr += sizeof(dl_phys_addr_ack_t) + 
					  datal_ack->addr_len;

			    freemsg(bp);
			    upp->up_waiting_ack = 0;
			    putnext(upp->up_readq, ea);			
			}
		    }
		    else if (upp->up_waiting_ack)
		    {
			dl_error_ack_t	*err_ack;
			dl_ok_ack_t	*ok_ack;
			struct datal_address	*datal_ack;
			mblk_t		*ea;

	
			bp = dnp->dn_dlpi_ack;
			dnp->dn_dlpi_ack = NULL;

			datal_ack = (struct datal_address *) bp->b_rptr;
			if (datal_ack->error_field)
			{
			    ea = allocb(sizeof(dl_error_ack_t), BPRI_HI);
			    if (!ea) 
			    {
				dnp->dn_dlpi_ack = bp;
				goto stop;
			    }
			    err_ack = (dl_error_ack_t *) ea->b_rptr;
			    ea->b_wptr += sizeof(dl_error_ack_t);
			    ea->b_datap->db_type = M_PCPROTO;
			    if (datal_ack->prim_type == DATAL_ENAB_MULTI)
				upp->up_multi_count--;

			    /* Construct error_ack */	
			    err_ack->dl_primitive = DL_ERROR_ACK;
			    err_ack->dl_error_primitive = upp->up_waiting_ack;
			    err_ack->dl_unix_errno = 0;
			    if (datal_ack->error_field == EINVAL)
			        err_ack->dl_errno = DL_BADADDR;
			    else if (datal_ack->error_field == ENOSPC)
			        err_ack->dl_errno = DL_TOOMANY;
			    else
			    {
			        err_ack->dl_errno = DL_SYSERR;
				err_ack->dl_unix_errno = datal_ack->error_field;
			    }

			    upp->up_waiting_ack = 0;
			    putnext(upp->up_readq, ea);			
			    freemsg(bp);
			}
			else
		    	{
			    /* Construct ok_ack */
			    ok_ack = (dl_ok_ack_t *) bp->b_rptr;
			    ok_ack->dl_correct_primitive = upp->up_waiting_ack;
			    ok_ack->dl_primitive = DL_OK_ACK;

			    bp->b_datap->db_type = M_PCPROTO;
			    bp->b_wptr = bp->b_rptr + sizeof(*ok_ack);
			    upp->up_waiting_ack = 0;
			    putnext(upp->up_readq, bp);			
			}
		    }
		}
		if (dnp->dn_dlpi_ack)
		{
		    freemsg(dnp->dn_dlpi_ack);
		    dnp->dn_dlpi_ack = NULL;
		}

		/*
		 * skip registration check (otherwise may discover a message
		 * on the up stream's write Q other than LL_REG/DL_BIND_ACK)
		 */
		skip_reg_check = 1;
	    }

	  if (!skip_reg_check)
	  {
	    /* Check registration stage */
	    if (dnp->dn_regstage == REGFROMMAC)
	    {
		llc2tune_t *tunep = &dnp->dn_tunetab;

		if (tunep->xid_Ndup && tunep->xid_Tdup)
		{   /* XID duplicate address check wanted */
		    lp->retry_count = tunep->xid_Ndup;
		    dnp->dn_regstage = REGDUPWAIT;

		    /* Request XID command */
		    lp->xidflag = 2;
		}
		else
		    dnp->dn_regstage = REGDONEOK;
	    }

	    if (dnp->dn_regstage >= REGDONEOK)
	    {   /* XID check (if any) completed */
		llc2up_t *upp;
		mblk_t *bp;
		extern llc2up_t *llc2_uptab;
		extern uint	llc2_nups;
		int		n;

		/* Deal with delayed network registrations */
		for (n = llc2_nups, upp = llc2_uptab; n--;  upp++)
		if (upp->up_readq && upp->up_dnp == dnp)
		{   /* Go through upper streams */
		    if (bp = getq(WR(upp->up_readq)))
		    if (upp->up_interface == LLC_IF_DLPI)
		    {
			if (dnp->dn_regstage == REGDUPFAIL) {
			    /* Duplicate MAC address.  No reasonable DLPI
			     * error code for this, so return DL_ACCESS for
			     * now. */
			    upp->up_dlpi_state = DL_UNATTACHED;
			    dlerrorack(WR(upp->up_readq), bp, DL_ATTACH_REQ,
					DL_ACCESS, 0);
			} else {
			    /*
			     * Send an OK ACK msg upstream now.
			     */
			    upp->up_dlpi_state = DL_UNBOUND;
			    dlokack(WR(upp->up_readq), bp, DL_ATTACH_REQ);
			}
		    } else {  /* up_interface == LLC_IF_SPIDER */
			struct ll_reg FAR *rp =
				(struct ll_reg FAR *)bp->b_rptr;
			llc2_maccpy((bufp_t)rp->ll_mymacaddr,
					(bufp_t)dnp->dn_macaddr,
					dnp->dn_maclen);
			if (dnp->dn_regstage == REGDUPFAIL)
			    rp->ll_regstatus = LS_DUPLICATED;
			putnext(upp->up_readq, bp);
		    }
		}
	    }
	  }
	}

	if (lp->xidflag == 2)
	{   /* Send XID command */

	    /* Check flow and buffers */
	    if (!getstock(dnp, (mblk_t *)0, NORMAL_UPSIZE)) goto stop;

	    /* If station, set to own address for DUP check XID */
	    if (lp == &dnp->dn_station)
	    {
		llc2_maccpy((bufp_t)(&lp->dte[2]), (bufp_t)dnp->dn_macaddr,
				dnp->dn_maclen);
		lp->dte[0] = lp->dte[1] = 0;
		lp->loopback = 0;
		lp->upp = (struct llc2up *)0;
	    }

	    /* Fire off XID */
	    llc2_sendxid(lp);
	}

	/* Now, deal with info packets if not stopped */
	while ((mp = lp->datq) && !lp->txstopped)
	{   /* Send info if we can */

	    /*
	     * Make sure flow is OK and we have all the buffers we need
	     * (including duplicate of data).
	     */
	    if (!getstock(dnp, mp, NORMAL_UPSIZE)) goto stop;

	    /* Remove message from queue (leaving on the ackq) */
	    lp->datq = mp->b_next;

	    /* Operate the state M/C */
	    llc2_sendinfo(lp);
	}

	/* If an ack we need hasn't already been piggy-backed, send it now. */
	if (lp->need_ack)
	{
	    /* Make sure flow is OK and we have all the buffers we need */
	    if (!getstock(dnp, (mblk_t *)0, NORMAL_UPSIZE)) goto stop;
	    llc2_txack(lp);
	}

	/* Finally, check for disconnection */
	if (lp->state == OFF)
	    llc2_disconnect(lp);
	else
	if (lp->state == STOP)
	    lp->state = OFF;
    }

    /* All scheduled connections serviced OK */
    return 0;

stop:
    /* Failed 'canput' or buffer shortage: requeue component then exit */
    llc2_schedule(lp);
    return 0;
}


/*
 *  'llc2_udatasrv' services LLC1 data requests, and XID/TEST requests.
 */
llc2_udatasrv(dnp)
llc2dn_t *dnp;
{
    llc2up_t *upp;

    /* Deal with LLC1 data requests */
    while (upp = dnp->dn_udatq)
    {
	queue_t *uq = WR(upp->up_readq);
	register llc2cn_t *lp;
	register mblk_t *mp;

	if (upp->up_interface == LLC_IF_DLPI)
	{
	    while (mp = getq(uq))
	    {
		union DL_primitives *dlp;
		int error;

		/*
		 * Copy address info from DLPI message into station component.
		 */
		dlp = (union DL_primitives *) mp->b_rptr;
		error = llc2_dlpi_dtecpy((lp = &dnp->dn_station), upp, mp,
					 dnp->dn_macaddr,
					 (dlp->dl_primitive ==
					   DL_UNITDATA_REQ ? 0 : 1));
		if (!error)
		{
		    lp->upp = upp;

		    if (!getstock(dnp, mp->b_cont, NORMAL_UPSIZE))
		    {   /* No resources: try again later */
			putbq(uq, mp);
			return 0;
		    }
		    /* Got everything we need: send it off */
		    if (dlp->dl_primitive == DL_UNITDATA_REQ)
		    {
			llc2_sendudata(lp);
			lp->loopback = 0;
			freemsg(mp);
			continue;
		    }
		    lp->need_fbit = (dlp->test_req.dl_flag == DL_POLL_FINAL);
		    switch (dlp->dl_primitive)
		    {
		    case DL_TEST_REQ:
			send_Ucmd(lp, TEST);
			/* Check this - may want msgc for routing info */
			break;
		    case DL_TEST_RES:
			send_Ursp(lp, TEST);
			break;
		    case DL_XID_REQ:
			send_Ucmd(lp, XID);
			/* Check this - may want msgc for routing info */
			break;
		    case DL_XID_RES:
			send_Ursp(lp, XID);
			break;
		    }
		    if (mp)
			freemsg(mp);
		} else {
		    switch (dlp->dl_primitive)
		    {
		    case DL_UNITDATA_REQ:
			reply_uderror(uq, mp, error);
			break;
		    case DL_TEST_REQ:
		    case DL_XID_REQ:
			dlerrorack(uq, mp, dlp->dl_primitive, error, 0);
			break;
		    default:
			freemsg(mp);
		    }
		}
	    }
	}
	/*
	 * Handle Spider Interface LLC1 data requests.
	 */
	else while (mp = getq(uq))
	{
	    struct ll_msgc FAR *pp = (struct ll_msgc FAR *)mp->b_rptr;

	    /* Copy address info into station component */
	    if (!llc2_dtecpy((lp = &dnp->dn_station), upp, pp, dnp->dn_macaddr))
	    {	/* Legal address */

		lp->upp = upp;

		if (!getstock(dnp, mp->b_cont, NORMAL_UPSIZE))
		{   /* No resources: try again later */
		    putbq(uq, mp);
		    return 0;
		}

		/* Got everything we need: send it off */
		llc2_sendudata(lp);
		lp->loopback = 0;
	    }
	    freemsg(mp);
	}
	dnp->dn_udatq = upp->up_udatqnext;
	upp->up_udatqnext = NULL;
    }
    return 0;
}

static void gotbuf(arg)
caddr_t arg;
{
    register llc2dn_t *dnp = (llc2dn_t *)arg;
/*  nostock = 0; */
    dnp->dn_bufcall = 0;
    if (dnp->dn_downq)
	qenable(dnp->dn_downq);
}

static int nobuf(dnp, sz)
llc2dn_t *dnp;
{
    dnp->dn_bufcall = bufcall(sz, BPRI_MED, gotbuf, (long)dnp);
/*  nostock = 1; */
    return 0;
}

/*
 * 'getstock' is called before cranking the state M/C to get blocks
 * for all STREAMS messages that might be needed in advance.
 *
 * For the upward (net) interface, just a top-level protocol header
 * is normally needed.  Its size is given by 'upsize'.
 *
 * For downward (MAC) messages two blocks are normally required,
 * a protocol block and a block to hold the PDU header.
 *
 * Data requests messages have duplicates of the information part
 * attached to them.  The original message remains in the
 * transmit/acknowledgement queue until acknowledged by the
 * remote end, at which point it is freed.
 */
static int getstock(dnp, infop, upsize)
llc2dn_t *dnp;
mblk_t *infop;
int upsize;
{   /* Gets all the message blocks that might be needed by the state M/C */
    register mblk_t *bp;
    register dblk_t *dp;

    /* Wait for back-enable if MAC busy */
    if (!canput(dnp->dn_downq->q_next)) return 0;

    upsize += MAXRTLEN;

    /* Make sure existing up (network) message is big enough */
    if (llc2_upmp && (dp = llc2_upmp->b_datap) &&
    	(dp->db_lim - dp->db_base) < upsize)
    {   /* Not big enough: throw it away */
	freemsg(llc2_upmp);
	llc2_upmp = (mblk_t *)0;
    }

    /* Make sure we have an up (network) message of suitable size */
    if (!llc2_upmp)
    {   /* None in stock: get one */
	if (!(llc2_upmp = allocb(upsize, BPRI_MED)))
	    return nobuf(dnp, upsize);
    }

    /* Now make sure we have a complete MAC message */
#if DL_VERSION >= 2
    if (!(bp = llc2_dnmp) && !(llc2_dnmp = bp =
                allocb(sizeof(struct datal_tx)+MAXHWLEN+MAXRTLEN, BPRI_MED)))
        return nobuf(dnp, sizeof(struct datal_tx)+MAXHWLEN+MAXRTLEN);
#else /* DL_VERSION >= 2 */
    if (!(bp = llc2_dnmp) && !(llc2_dnmp = bp =
    			allocb(sizeof(struct datal_tx), BPRI_MED)))
	return nobuf(dnp, sizeof(struct datal_tx));
#endif /* DL_VERSION >= 2 */

    /* Make sure MAC message has a PDU block */
    if (!(bp->b_cont) && !(bp->b_cont = allocb(PDU_SIZE, BPRI_MED)))
	return nobuf(dnp, PDU_SIZE);

    /* Make sure MAC message does NOT have an old information part */
    if ((bp = bp->b_cont)->b_cont)
    {   /* Free old (unused) information part of down message, if any */
	freemsg(bp->b_cont);
	bp->b_cont = (mblk_t *)0;
    }

    /* Add duplicate of new information part (if any) to MAC message */
    if (infop && !(bp->b_cont = dupmsg(infop)))
	return nobuf(dnp, sizeof(mblk_t));

    /* Success */
    return 1;
}

static fillmsgc(pp, myhandle, dte, macaddr, maclen)
struct ll_msgc FAR *pp;
caddr_t myhandle;
uint8 *dte, *macaddr;
{   /* Fill in 'll_msgc' address information */
    pp->ll_myhandle = myhandle;
    pp->ll_service_class = 0;
    pp->ll_remsize = (maclen + 1)*2;
    pp->ll_locsize = (maclen + 1)*2;
    llc2_maccpy((bufp_t)pp->ll_locaddr, (bufp_t)macaddr, maclen);
    pp->ll_locaddr[maclen] = dte[0];
    llc2_maccpy((bufp_t)pp->ll_remaddr, (bufp_t)(dte+2), maclen);
    pp->ll_remaddr[maclen] = dte[1];
    return 0;
}

llc2_tomac(lp, plen)
llc2cn_t *lp;
{   /* Send packet to MAC with PDU header length 'plen' */
    mblk_t *mp;
    register struct datal_tx FAR *pp;
    register queue_t *macq = lp->dnp->dn_downq;
    uint8 up_mode = (lp->upp)? lp->upp->up_mode : LLC_MODE_NORMAL;

    if (!(mp = llc2_dnmp))
    {
    	strlog(LLC_STID,0,0,SL_ERROR,"LLC2_tomac - No DOWN message");
	return 0;
    }

    llc2_dnmp = (mblk_t *)0;

    /* Fill in Tx message */
    mp->b_datap->db_type = M_PROTO;
    pp = (struct datal_tx FAR *)mp->b_wptr;
    pp->prim_type = DATAL_TX;
    DLP(("llc2_tomac: llc2cn=%p, maclen=%d\n",lp,lp->dnp->dn_maclen));
    HEXDUMP(&lp->dte[0], 8);
    llc2_maccpy((bufp_t)pp->dst, (bufp_t)(lp->yrmac), lp->dnp->dn_maclen);

#if DL_VERSION >= 2
    pp->addr_length = lp->dnp->dn_maclen;
    strlog(LLC_STID,0,9,SL_TRACE,"LLC - tomac: lp->routelen=%d", 
	lp->routelen);
    if (pp->route_length = lp->routelen)
    {
        /*
         * copy in routing info if given - assumes space is allocated
         */
        bcopy (lp->route, pp->dst + pp->addr_length, pp->route_length);
    }

    mp->b_wptr += sizeof(struct datal_tx) + lp->dnp->dn_maclen - 1 +
                        pp->route_length;
#else /* DL_VERSION >= 2 */
    mp->b_wptr += sizeof(struct datal_tx);
#endif /* DL_VERSION >= 2 */

    if ((lp->dte[0] & 0xfe) == 0) {	/* NULL SAP */
	DLP(("\tNULL SAP, up_mode=%d\n",up_mode));
	up_mode = LLC_MODE_NORMAL;
    }
    if (up_mode == LLC_MODE_NORMAL)
    {
	    mp->b_cont->b_wptr += plen;

	    pp->type = msgdsize(mp->b_cont);
    }
    else if (up_mode == LLC_MODE_ETHERNET)
    {
	mblk_t *headerp;
	/*
	 * no LLC2 header required, so free it
	 */
	headerp = mp->b_cont;
	mp->b_cont = headerp->b_cont;
	headerp->b_cont = NULL;
	freeb(headerp);

	/* fill in the Ethernet type, previously stored in dte[0-1]  */
	pp->type = lp->dte[1] + (lp->dte[0] << 8);
    }
    else /* up_mode == LLC_MODE_NETWARE) */
    {
	mblk_t *headerp;

	pp->type = msgdsize(mp->b_cont);

	/*
	 * Netware "raw mode" -- there is no LLC2 header.  Detach the msg
	 * block intended to hold the header, for possible reuse below in
	 * padding.
	 */
	headerp = mp->b_cont;
	mp->b_cont = headerp->b_cont;
	headerp->b_cont = NULL;

	/*
	 * If the 802.3 packet length will be greater than ETHERMIN, and the
	 * packet length is odd, pad the packet to an even length.
	 * (802.3 header length = 14 = 6 byte dest MAC + 6 byte src MAC +
	 * 2 byte length
	 */
#ifndef ETHERMIN
#define ETHERMIN	60
#endif
	if ((int) pp->type > ETHERMIN - 14 && (pp->type & 1))
	{
	    mblk_t *bp;

	    pp->type++;
	    bp = mp->b_cont;
	    while (bp->b_cont)
		bp = bp->b_cont;
	    if (bp->b_wptr < bp->b_datap->db_lim)
		*bp->b_wptr++ = 0;
	    else
	    {
		bp->b_cont = headerp;
		headerp->b_wptr = headerp->b_rptr;
		*headerp->b_wptr++ = 0;
		headerp = NULL;
	    }
	}
	if (headerp)
	    freeb(headerp);
    }

    if ( lp->dnp->dn_trace_active ) llc2_trace(lp->dnp,mp,0);

#if defined (NO_TRANS_LOOP)
	if (lp->loopback) {
		/* Trace this message as received, since it's looped back here */
		if (lp->dnp->dn_trace_active)
			llc2_trace(lp->dnp,mp,1);

		/* Put original message on MAC Rx queue (with type ETH_TX) */
		llc2_rxputq(lp->dnp, mp);

		return 0;
	}
#else
    if (lp->loopback)
    {   /* Sending to own MAC address: send back up read side */
	mblk_t *lbmp = mp;

	/* Replace MAC message if possible */
	if ((mp = copyb(lbmp)) && !(mp->b_cont = dupmsg(lbmp->b_cont)))
	{   /* Incomplete, so give back bits we have */
	    freemsg(mp);
	    mp = (mblk_t *)0;
	}

	/* Trace this message as received, since it's looped back here */ 
        if ( lp->dnp->dn_trace_active ) llc2_trace(lp->dnp,lbmp,1);

	/* Put original message on MAC Rx queue (with type ETH_TX) */
	llc2_rxputq(lp->dnp, lbmp);

	/* Send duplicate to MAC Tx (unless failed to get one) */
	if (!mp) return 0;
    }
#endif

    /* Pass to MAC level */
    putnext(macq, mp);
    return 0;
}

llc2_tonet(lp, command, status)
llc2cn_t *lp;
{   /* Send indication or report to network layer */
    mblk_t *mp;
    dblk_t *dp;
    register queue_t *netq;
    register union ll_un FAR *pp;

    if ( (!lp->upp) || (!lp->upp->up_readq) )
    {
    	strlog(LLC_STID,0,0,SL_ERROR,"LLC2_tonet - No UP queue");
	return 0;
    }

    if (!(mp = llc2_upmp))
    {   /* getstock mechanism has failed !!?? */
    	strlog(LLC_STID,0,0,SL_ERROR,"LLC2 - No UP message");
	return 0;
    }

    /* Take message from stock */
    llc2_upmp = (mblk_t *)0;

    if (lp->upp->up_interface == LLC_IF_DLPI) {
	llc2_dlpi_tonet(lp, command, status, mp);
	return 0;
    }

    netq = lp->upp->up_readq;

    pp = (union ll_un FAR *)mp->b_wptr;
    dp = mp->b_datap;

    pp->ll_type = command==LC_DATA || command==LC_UDATA ? LL_DAT : LL_CTL;
    pp->msg.ll_connID = lp->connID;
    pp->msg.ll_yourhandle = lp->nethandle;

    switch (pp->msg.ll_command = command)
    {
    case LC_CONNECT:
    case LC_CONOK:
	/* Check message size is OK */
	if (dp->db_lim - dp->db_base < sizeof(struct ll_msgc))
	{   /* getstock 'connect' mechanism has failed !!?? */
	    strlog(LLC_STID,0,0,SL_ERROR,"LLC2 - UP message too small");
	    return 0;
	}

	/* Fill in rest of proto block */
	dp->db_type = (command == LC_CONOK ? M_PCPROTO : M_PROTO);
        mp->b_wptr += sizeof(struct ll_msgc);
	fillmsgc(&pp->msgc, (caddr_t)lp, lp->dte, lp->dnp->dn_macaddr,
		 lp->dnp->dn_maclen);  
	break;

    default:
	/* Commands other than LC_CONNECT and LC_CONOK have 'status' */
        mp->b_wptr += sizeof(struct ll_msg);
	dp->db_type = M_PROTO;
	pp->msg.ll_status = status;
	break;
    }

    if ( command == LC_CONCNF && status == LS_SUCCESS)
	strlog(LLC_STID,(short)lp->dnp->dn_snid,1,SL_TRACE,
	       "LINK  Up  : '%x' Id %03x",(short)lp->dnp->dn_snid,lp->connID);
	       
    else if ( command == LC_DISC ||
             (command == LC_REPORT && status == LS_RST_FAILED))
	strlog(LLC_STID,(short)lp->dnp->dn_snid,1,SL_TRACE,
	       "LINK  Dwn : '%x' Id %03x Remote",lp->dnp->dn_snid,lp->connID);
	       
    else if ( command == LC_RESET)
	strlog(LLC_STID,(short)lp->dnp->dn_snid,2,SL_TRACE,
	       "LINK  Rst : '%x' Id %03x Remote",lp->dnp->dn_snid,lp->connID);

    putnext(netq, mp);
    return 0;
}

llc2_flushstock()
{
    /* Throw away any unused messages got by 'getstock' */
    if (llc2_upmp)
    {
	freemsg(llc2_upmp);
	llc2_upmp = (mblk_t *)0;
    }

    if (llc2_dnmp)
    {
	freemsg(llc2_dnmp);
	llc2_dnmp = (mblk_t *)0;
    }

    if (llc2_rxmp)
    {
	freemsg(llc2_rxmp);
	llc2_rxmp = (mblk_t *)0;
    }
    return 0;
}

