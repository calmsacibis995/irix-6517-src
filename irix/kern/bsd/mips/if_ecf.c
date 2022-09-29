/*
 * Copyright 1995 Silicon Graphics, Inc.  All rights reserved.
 *
 * Driver for Silicon Graphics IP32 PCI 100/10 fast ethernet controller
 * using Texas Instruments ThunderLAN ethernet control chip
 * (supports 10BaseT, 100BaseTX, 100BaseT4, and 100VG-AnyLAN protocols).
 * (Racore/Compaq PCI Fast Ethernet Adaptor)
 *
 */
#ident "$Revision: 1.21 $"

#if IP32 || IP30

#include "sys/tcp-param.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/edt.h"
#include "sys/sysmacros.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/kmem.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/mbuf.h"
#include "sys/sbd.h"
#include "sys/socket.h"
#include "sys/cpu.h"
#include "sys/ksynch.h"
#include "sys/systm.h"

#include "sys/PCI/pciio.h"
#include "sys/PCI/PCI_defs.h"

#include "net/if.h"
#include "net/raw.h"
#include "net/soioctl.h"
#include <net/rsvp.h>
#include "ether.h"
#include "sys/atomic_ops.h"
#include "sys/if_ecf.h"
#include "ksys/cacheops.h"

#include <sys/mload.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>

/* configurable variables in master.d/if_ecf
 */
extern uint_t	ecf_max_txds;
extern uint_t	ecf_max_rxds;
extern int	ecf_no;
extern int	ecf_spdpx;
extern int	ecf_txipp;

/* global variables for loadable driver.
 */
char		*if_ecfmversion = M_VERSION;
int		if_ecfdevflag = D_MP;

/* ecf network fucntions prototypes
 */
static int	ecf_init(struct etherif *, int);
static void	ecf_reset(struct etherif *);
static void	ecf_reset_fdr(ecf_info_t *, int);
#if IP32
static void	if_ecfintr(eframe_t *, intr_arg_t);
#else
static void	if_ecfintr(void *);
#endif
static void	ecf_watchdog(struct ifnet *);
static int	ecf_transmit(struct etherif *, struct etheraddr *,
			struct etheraddr *, u_short, struct mbuf *);
static int	ecf_new_rcvbuf(ecf_info_t *, tlan_rxfd_t *, uint_t);
static int	ecf_ioctl(struct etherif *, int, caddr_t);
static u_char	ecf_lafhash(u_char *);

/* TI ThunderLan control chip functions prototypes.
 */
static void	tlan_init(ecf_info_t *);
static void	tlan_mii_sync(ecf_info_t *);
static void	tlan_mii_init(ecf_info_t *);
static WORD	tlan_mii_io (BYTE, ecf_info_t *, BYTE, BYTE, WORD);
static int	tlan_phydev(ecf_info_t *, int);
static void	tlan_setspdpx(ecf_info_t *);
static void	tlan_autonego(ecf_info_t *);
static void	tlan_phystatus(ecf_info_t *);
static void	tlan_statistics(ecf_info_t *);
static BYTE	tlan_eeprom_io(BYTE, ecf_info_t *, BYTE, DWORD, BYTE, int*);

/* PCI infra structure interface functions prototypes.
 */
int		if_ecfattach(vertex_hdl_t);
int		if_ecfdetach(vertex_hdl_t);
int		if_ecferror(vertex_hdl_t, int, ioerror_mode_t, ioerror_t *);

/* RSVP function prototypes */
static int	ecf_txfree_len(struct ifnet *);
static void	ecf_setstate(struct ifnet *, int);


/**************************************************************************n
 *  Ethernet Controller Fast-ethernet (Network) Related Functions           *
 *  (Ethernet Interface Operations)                                         *
 ****************************************************************************
 */

static struct etherifops ecfops = {
    ecf_init, ecf_reset, ecf_watchdog, ecf_transmit,
    (int (*)(struct etherif*,int,void *))ecf_ioctl
};


/*
 * ecf_init()
 *
 * Description:
 *    Init Fast Ethernet Controller interface.
 *       1. Call ecf_reset() to free stale mbufs and reset descriptors.
 *       2. Create new Rx mbufs.
 *       3. Call tlan_init() to init TLAN and start receiving.
 *
 */
/* ARGSUSED */
static int
ecf_init (
    struct etherif *eif,
    int flags)
{
    ecf_info_t *ei = eiftoei(eif);
    struct ps_parms ps_params;

    /* reset TLAN chip and ecf interface
     * kernel init() has just called it but ifconfig() will need it.
     */
    ecf_reset(eif);

    /* init rx mbuf for each receive descriptor */
    if (ecf_new_rcvbuf(ei, (tlan_rxfd_t *)ei->ei_rxdr.r_head, ecf_max_rxds))
		return (ENOBUFS);

    /* init TLAN AHR and DIO registers */
    tlan_init(ei);

    ei->ei_sick = 0;
    ei->ei_if.if_timer = ECFDOG;	/* turn on watchdog */

    if (ei->ei_tlan.speed & FAST100Mb)
	    ps_params.bandwidth = 100000000;
    else
	    ps_params.bandwidth = 10000000;
    ps_params.flags = 0;
    ps_params.txfree = ecf_max_txds;
    ps_params.txfree_func = ecf_txfree_len;
    ps_params.state_func = ecf_setstate;
    (void)ps_reinit(eiftoifp(&ei->ei_eif), &ps_params);

    return (0);
}


/*
 * ecf_reset()
 *
 * Description:
 *    Reset ethernet interface. Must be done at splimp level.
 *       1. Reset TLAN
 *       2. Free Rx mbuf and reset Rx descriptors
 *       3. Free Tx mbuf and reset Tx descriptors
 *
 * Assumes ei_if lock held on entry.
 */
static void
ecf_reset (
    struct etherif *eif)
{
    ecf_info_t *ei = eiftoei(eif);

    /* stop watchdog */
    ei->ei_if.if_timer = 0;

    /* reset TLAN ethernet chip */
    TLAN_RESET(ei);
    
    /* free all stale mbufs and reset the frame descriptor ring. */
    ecf_reset_fdr(ei, TLAN_RX);
    ecf_reset_fdr(ei, TLAN_TX);
}

/*
 * ecf_reset_fdr()
 *
 * Description:
 *    Free mbuf and reset frame descriptor ring.
 *
 */
static void
ecf_reset_fdr (
    ecf_info_t *ei,
    int flag)
{
    tlan_dr_t *dr;
    struct mbuf *m;
    uint_t i, j;

    if (flag == TLAN_TX) {
	tlan_txfd_t *txfd;
	dr = &ei->ei_txdr;
 
	/* reset tx descriptors ring and clean all tx mbufs */
	for (txfd = (tlan_txfd_t *)dr->r_head, i = 0; i < ecf_max_txds;
	     txfd++, i++) {
		txfd->d_nxtd = (i == ecf_max_txds - 1) ?
			(tlan_txfd_t *)dr->r_head : txfd + 1;
		txfd->d_nxtf = KVTOPCIIO(ei, txfd->d_nxtd, PCIIO_CMD);
		txfd->d_cstat = CSTAT_REQ;
		txfd->d_fsize = 0;
		for (j = 0; j < TLAN_TX_FRGMS; j++) {
			txfd->d_frgm[j].f_bcnt = 0;
			txfd->d_frgm[j].f_buf = 0;
		}
		/* free stale mbuf; make sure mbuf is valid */
		if (dr->r_tail && (m = txfd->d_mbuf))
			m_freem(m);
		txfd->d_mbuf = 0;
	}
	txfd--;
	txfd->d_nxtf = 0;
	txfd->d_nxtd = (tlan_txfd_t *)dr->r_head;
	dr->r_tail = (void *)txfd;
	dr->r_ndesc = ecf_max_txds;
    }
    else { /* TLAN_RX */
	tlan_rxfd_t *rxfd;
	dr = &ei->ei_rxdr;
  
	/* reset rx descriptors ring and clean all rx mbufs */
	for (rxfd = (tlan_rxfd_t *)dr->r_head, i = 0; i < ecf_max_rxds;
	     rxfd++, i++) {
		rxfd->d_nxtd = (i == ecf_max_rxds - 1) ?
			(tlan_rxfd_t *)dr->r_head : rxfd + 1;
		rxfd->d_nxtf = KVTOPCIIO(ei, rxfd->d_nxtd, PCIIO_CMD);
		rxfd->d_cstat = CSTAT_REQ;
		rxfd->d_fsize = 0;
		rxfd->d_frgm[0].f_bcnt = 0;
		rxfd->d_frgm[0].f_buf = 0;

		/* free stale mbuf; make sure mbuf is valid */
		if (dr->r_tail && (m = rxfd->d_mbuf))
			m_freem(m);
		rxfd->d_mbuf = 0;
	}
	rxfd--;
	rxfd->d_nxtf = 0;
	rxfd->d_nxtd = (tlan_rxfd_t *)dr->r_head;
	dr->r_tail = (void *)rxfd;
	dr->r_ndesc = ecf_max_rxds;
    }

    dr->r_freed = dr->r_head;
    dr->r_actd = dr->r_rotd = dr->r_nxtd = dr->r_lastd = 0;
    dr->r_busy = 0;
}


/*
 * if_ecfintr()
 *
 * Description:
 *    Generic ethernet interface interrupt.  Come here at splimp().
 *
 */
/* ARGSUSED */
static void
if_ecfintr (
#if IP32
    eframe_t *ef, intr_arg_t info)
#else
    void *info)
#endif
{
    ecf_info_t *ei = (ecf_info_t *)info;
    u_int32_t intrreg, status;
    char intrtype, intrmask, acks = 0;

    IFNET_LOCK(&ei->ei_if);
    /* check if down or early interrupts */
    if ((iff_dead(ei->ei_if.if_flags)) || !ei->ei_txdr.r_head) {
	ether_stop(&ei->ei_eif, ei->ei_if.if_flags);	
        IFNET_UNLOCK(&ei->ei_if);
	return;
    }

    /* disable TLAN interrupt and get intrreg */
    TLAN_AHR_WR_DWORD(ei, AHR_HOST_CMD_REG, HOST_CMD_INTR_OFF, DWMSK);
    intrreg = TLAN_AHR_RD_WORD(ei, AHR_HOST_INTR_REG, WMSK);

    if (!(intrtype = TLAN_HOST_INTR_TYPE(intrreg))) {
	TLAN_AHR_SET_DWORD(ei, AHR_HOST_CMD_REG, HOST_CMD_INTR_ON, DWMSK);
	IFNET_UNLOCK(&ei->ei_if);
	return;
    }
    /* write intrreg back to disable PCI interrupt */
    TLAN_AHR_WR_WORD(ei, AHR_HOST_INTR_REG, intrreg, WMSK);

    switch (intrtype) {
	case TX_EOC:	/* tx end-of-channel */
	     {
		tlan_dr_t *txdr = &ei->ei_txdr;
		tlan_txfd_t *txd;
		int s, tx_busy = 0;

		acks++;
		if (!txdr->r_rotd)
			txdr->r_rotd = txdr->r_actd;

		/* try to transmit next standby tx list */
		if (txdr->r_nxtd && (s = mutex_spintrylock(&txdr->r_slock))) {
			if (txdr->r_nxtd) {
				txdr->r_actd = txdr->r_nxtd;
				txdr->r_nxtd = 0;
				tx_busy = 1;

				/* ack interrupts and enable TLAN interrupt */
				TLAN_AHR_WR_DWORD(ei, AHR_HOST_CMD_REG,
					(((intrreg << 16) + acks) |
					HOST_CMD_ACK), DWMSK);
				TLAN_AHR_SET_DWORD(ei, AHR_HOST_CMD_REG,
					HOST_CMD_INTR_ON, DWMSK);

				/* start tx as early as possible */
				TLAN_TX_GO(ei, txdr->r_actd);
			}
			mutex_spinunlock(&txdr->r_slock, s);
		}
		txdr->r_busy = tx_busy;

		/* clean up the old tx list */
		for (txd = (tlan_txfd_t *)txdr->r_rotd; txd &&
		     (txd->d_cstat & FRAME_COMPLETE);
		     txd = txd->d_nxtd, txdr->r_rotd = (void *)txd) {

			/* free tx mbufs and adjust descriptors */
			txd->d_cstat = 0;
			m_freem(txd->d_mbuf);
			txd->d_mbuf = 0;
			txdr->r_ndesc++;
			ei->ei_if.if_opackets++;

			/* check if all done */
			if (!txd->d_nxtf &&
			    (!(txd->d_nxtd->d_cstat & FRAME_COMPLETE) ||
			      (txd->d_nxtd == (tlan_txfd_t *)txdr->r_actd))) {
				txdr->r_rotd = 0;
				break;
			}
		}

		/* RSVP - let packet scheduler know there's more room in the q */
		if (ei->ei_flags & EIF_PSENABLED) 
			ps_txq_stat(eiftoifp(&ei->ei_eif), txdr->r_ndesc);

		if (tx_busy) { /* ack & intr has been done */
			IFNET_UNLOCK(&ei->ei_if);
			return;
		}
		break;
	     }

	case RX_EOF:	/* rx end-of-frame */
	     {
		/* process received packets */
		tlan_dr_t *rxdr = &ei->ei_rxdr;
		tlan_rxfd_t *rxd;
		struct mbuf *m;
		int rlen, snoopflags = 0;
		u_short cstat;

		ei->ei_physts.led |= LED_ACT;
		TLAN_DIO_WR_BYTE(ei, DIO_NET_LED_REG, ei->ei_physts.led, BMSK);

		/* process received packets and collect all statistics */
		for (rxd = (tlan_rxfd_t *)rxdr->r_actd, acks = 0;
		     (rxd->d_cstat & FRAME_COMPLETE);
		     rxd = rxd->d_nxtd, acks++) {

			/* get the received packet */
			m = rxd->d_mbuf;
#ifdef IP32
			/* for R10K, avoid pg_vr setting overhead */
			xdki_dcache_validate(mtod(m, void *), RCVBUF_SIZE);
			__dcache_inval(mtod(m, caddr_t), RCVBUF_SIZE);
#endif
			rlen = rxd->d_fsize;
			cstat = rxd->d_cstat;

			/* make sure another new mbuf can be allocated
			 * otherwise drop the received packet
			 */
			if (ecf_new_rcvbuf(ei, rxd, 1)) {
				ei->ei_stats.rxdrp++;
				goto rx_req;
			}
			
			ei->ei_if.if_ipackets++;

			/* make sure the packet length is valid. */
			if (rlen > MAX_RPKT) {
				snoopflags |= SN_ERROR | SNERR_TOOBIG;
				rlen = MAX_RPKT;
			}
			else if (rlen < MIN_RPKT) {
				int padsz;

				snoopflags |= SN_ERROR | SNERR_TOOSMALL;

				/* pad with 0 so snoop_input() won't crash. */
				if ((padsz = EHDR_LEN - rlen) > 0) {
					struct ether_header *eh = &(mtod(m,
					       struct rcvbuf *))->ebh.ebh_ether;

					bzero((char *)(eh + rlen), padsz);
					rlen = EHDR_LEN;
				}
			}
			ei->ei_if.if_ibytes += rlen;
			m->m_len = (rlen - ETHER_HDRLEN) +
				sizeof(struct etherbufhead);

			/* check error frame: CRC,alignment,and coding errors */
			if (cstat & TLAN_RX_FRAME_ERROR)
				snoopflags |= SNERR_FRAME;

			/* ready to pass this mbuf to the upper layers  */
			ether_input(&ei->ei_eif, snoopflags, m);

			/* append rxd back to the rx queue */
			rx_req:
			((tlan_rxfd_t *)rxdr->r_tail)->d_nxtf =
				KVTOPCIIO(ei, rxd, PCIIO_CMD);
			rxdr->r_tail = (void *)rxd;
		}
		/* advance to the next receiving descriptor */
		rxdr->r_actd = (void *)rxd;

		ei->ei_physts.led &= ~LED_ACT;
		TLAN_DIO_WR_BYTE(ei, DIO_NET_LED_REG, ei->ei_physts.led, BMSK);
		break;
	     }

	case RX_EOC:	/* rx end-of-channel */
	     {
		/* this should be avoid during normal receive process */
		tlan_dr_t  *rxdr = &ei->ei_rxdr;

		/* pick up head of rx list and restart receiving quickly */
		TLAN_AHR_WR_DWORD(ei, AHR_CH_PARM_REG,
			KVTOPCIIO(ei, rxdr->r_actd, PCIIO_CMD), DWMSK);
		TLAN_AHR_WR_DWORD(ei, AHR_HOST_CMD_REG, (((intrreg << 16) |
			0x1) | HOST_CMD_ACK | HOST_CMD_GO | HOST_CMD_RT |
			HOST_CMD_INTR_ON), DWMSK);
		ei->ei_tlan.rxeoc++;
		IFNET_UNLOCK(&ei->ei_if);
		return;
	     }

	case TX_EOF:	/* tx end-of-frame */
		/* since Ld_Thr=0 there should be no eof */
		acks++;
		break;

	case CHK_STATUS:
		/* adapter check interrupt */
		if (TLAN_HOST_INTR_VEC(intrreg)) {
			u_int32_t chkcode = TLAN_AHR_RD_DWORD(ei,
						AHR_CH_PARM_REG, DWMSK);

			/* read CH_PARM to get adapter check code */
			if (chkcode & 0xff)
				cmn_err_tag(194,CE_WARN, "%s%d: adapter check interrupt - %s while %s %s %s",
					IF_ECF, ei->ei_unit,
					((chkcode & 0x7) == TLAN_CHK_DATA_PART)?
					" data parity error" :
					((chkcode &0x7) == TLAN_CHK_ADDR_PART)?
					" address parity error" :
					((chkcode & 0x7) == TLAN_CHK_MSTR_ABRT)?
					" master abort" :
					((chkcode & 0x7) == TLAN_CHK_TRGT_ABRT)?
					"target abort" :
					((chkcode & 0x7) == TLAN_CHK_LST_ERR) ?
					"list error" :
					((chkcode & 0x7) == TLAN_CHK_ACK_ERR) ?
					"ack error" :
					((chkcode & 0x7) == TLAN_CHK_IOV_ERR) ?
					"intr overflow" : " unknown error",
					(chkcode & TLAN_CHK_RT) ?
					"receive channel" : "transmit channel",
					(chkcode & TLAN_CHK_RW) ?
					"read" : "write",
					(chkcode & TLAN_CHK_LD) ?
					"list" : "data");
			
			/* save statistic registers */
			tlan_statistics(ei);

			/* reset TLAN ethernet chip */
			TLAN_RESET(ei);

			/* ??? */
			ecf_init(&ei->ei_eif, ei->ei_if.if_flags);

			/* do not Ack as Ad_rst clears interrupt */
			IFNET_UNLOCK(&ei->ei_if);
			return;
		}
		/* network status interrupt */
		else {
			/* read NetSts and clear the status bits */
			status = TLAN_DIO_RD_BYTE(ei, DIO_NET_STS_REG, BMSK);
			TLAN_DIO_WR_BYTE(ei, DIO_NET_STS_REG, status, BMSK);

			/* work around for Racore: disable MIIRQ */
			intrmask = TLAN_DIO_RD_BYTE(ei, DIO_NET_MSK_REG, BMSK);
			TLAN_DIO_WR_BYTE(ei, DIO_NET_MSK_REG, 
				(intrmask & ~MIIRQ), BMSK);
		}
		break;

	case STAT_OVRFLOW:
		/* read all statistics and thereby clearing them */
		tlan_statistics(ei);

		cmn_err_tag(195,CE_WARN, "%s%d: statistics overflow", IF_ECF,
			ei->ei_unit);
		break;
		
	case DUMMY_INTR:
		break;

	default:
		break;
    }
    /* ack interrupts and enable TLAN interrupt */
    TLAN_AHR_WR_DWORD(ei, AHR_HOST_CMD_REG, (((intrreg << 16) + acks) |
	HOST_CMD_ACK), DWMSK);
    TLAN_AHR_SET_DWORD(ei, AHR_HOST_CMD_REG, HOST_CMD_INTR_ON, DWMSK);

    IFNET_UNLOCK(&ei->ei_if);
}


/*
 * ecf_new_rcvbuf()
 *
 * Description:
 *    Allocate and init a new receive mbuf for TLAN to receive packet into.
 *
 */
static int
ecf_new_rcvbuf (
    ecf_info_t *ei,
    tlan_rxfd_t *rxd,
    uint_t nrxds)
{
    tlan_rxfd_t *rxd0 = NULL;
    struct rcvbuf *nrb;
    struct mbuf	*m;
    uint_t mcnt = nrxds;
    int reuse = 0;

    for (; nrxds; nrxds--, rxd = rxd->d_nxtd) {
	if ((!(m = rxd->d_mbuf) || mcnt == 1) &&
	    !(rxd->d_mbuf = m_vget(M_DONTWAIT, RCVBUF_SIZE, MT_DATA))) {
		if (mcnt == 1) {
			rxd->d_mbuf = m;
			reuse = 1;
		}
		else
			return (ENOBUFS);
	}
	nrb = mtod(rxd->d_mbuf, struct rcvbuf *);

	/* init mbuf with an ifheader */
	IF_INITHEADER(nrb, &ei->ei_if, sizeof(struct etherbufhead));

	/* one fragment per packet frame */
	rxd->d_cstat = CSTAT_REQ;
	rxd->d_fsize = MAX_RPKT;
	rxd->d_frgm[0].f_bcnt = (MAX_RPKT | F_LASTFRGM);
	rxd->d_frgm[0].f_buf = KVTOPCIIO(ei, &nrb->ebh.ebh_ether, PCIIO_DATA);

	/* Writeback-invalidate cache contents over needed length */
	DCACHE_WBINVAL((void *)nrb, sizeof(struct etherbufhead));
	DCACHE_INVAL((void *)nrb->data, MAX_RDAT);

	if (rxd0)
		rxd0->d_nxtf = KVTOPCIIO(ei, rxd, PCIIO_CMD);
	rxd0 = rxd;
    }
    rxd0->d_nxtf = 0;

    return ((reuse) ? ENOBUFS : 0);
}


/*
 * ecf_transmit()
 *
 * Description:
 *    Queue the mbuf chain to transmit descripor ring for transmition
 */
static int
ecf_transmit (
    struct etherif *eif,		/* on this interface */
    struct etheraddr *edst,		/* destination address */
    struct etheraddr *esrc,		/* source address */
    u_short type,			/* of this type */
    struct mbuf *m0)			/* send this chain */
{
    ecf_info_t *ei = eiftoei(eif);
    tlan_dr_t  *txdr = &ei->ei_txdr;
    tlan_txfd_t *txd;
    struct mbuf *m, *mp;
    struct ether_header	*eh;
    caddr_t cp0, cp;
    tlan_frgm_t *txd_frgm, *last_frgm; 
    uint_t len, mlen, mbcnt, nfrgm;
    int	s, padsz;

    ASSERT(IFNET_ISLOCKED(&ei->ei_if));

    /* get next free tx descriptor */
    if (!txdr->r_ndesc) {
	ei->ei_stats.txdrp++;
	m_freem(m0);
	return (ENOBUFS);
    }
    txd = (tlan_txfd_t *)txdr->r_freed;

    /* add space for ethernet header */
    if (!M_HASCL(m0) && m0->m_off >= MMINOFF + SPACE_FOR_EHDR) {
	m0->m_off -= SPACE_FOR_EHDR;
	m0->m_len += SPACE_FOR_EHDR;
    }
    else { /* need a new mbuf to hold etherheader */
	if (!(m = m_get(M_DONTWAIT, MT_DATA))) {
		m_freem(m0);
		return (ENOBUFS);
	}
	m->m_len = SPACE_FOR_EHDR;
	m->m_next = m0;
	m0 = m;
    }

    /* create ether header for the packet */
    cp0 = mtod(m0, caddr_t);
    eh = RAW_HDR(cp0, struct ether_header);
    *(struct etheraddr *)eh->ether_dhost = *edst;
    *(struct etheraddr *)eh->ether_shost = *esrc;
    eh->ether_type = type;

    /* skip over the raw header padding */
    m0->m_off += RAW_HDRPAD(EHDR_LEN);
    m0->m_len -= RAW_HDRPAD(EHDR_LEN);
    cp0 += RAW_HDRPAD(EHDR_LEN);

    /* create a transmit frame descriptor */
    for (txd_frgm = txd->d_frgm, nfrgm = mbcnt = 0, m = m0; m; m = m->m_next) {

	/* skip empty mbuf */
	if (!(mlen = m->m_len))
		continue;

	/* copy rest of mbufs into a new big mbuf if almost out of fragments */
	if (nfrgm > TLAN_TX_FRGMS - 2 && m->m_next) {
		struct mbuf *mx;

		for (mx = m, mlen = 0; m; m = m->m_next)
			mlen += m->m_len;
		if (!(m = m_vget(M_DONTWAIT, mlen, MT_DATA))) {
			m_freem(m0);
			return (ENOBUFS);
		}
		(void)m_datacopy(mx, 0, mlen, mtod(m, caddr_t));
		mp->m_next = m;
		m_freem(mx);
	}
	/* split mbuf if it spans pages */
	for (cp = (m == m0)? cp0 : mtod(m, caddr_t); mlen;
	     cp += len, mlen -= len) {
		len = min(mlen, CLBYTES - poff(cp));

		/* set up tx fragment bcount & buf address */
		txd_frgm->f_bcnt = len;
		txd_frgm->f_buf = KVTOPCIIO(ei, cp, PCIIO_DATA);

		/* set up next fragment */
		last_frgm = txd_frgm;
		txd_frgm++;
		nfrgm++;
	}
	/* writeback-invalidate cache contents */
	DCACHE_WB(mtod(m, caddr_t), m->m_len);
	mbcnt += m->m_len;
	mp = m;
    }

    /* snoopers may want to copy this packet. */
    if (RAWIF_SNOOPING(&ei->ei_rawif) &&
	snoop_match(&ei->ei_rawif, cp0, m0->m_len))
	ether_selfsnoop(&ei->ei_eif, eh, m0, sizeof *eh, mbcnt);

    /* drop the huge packet */
    if (mbcnt > MAX_TPKT) {
	m_freem(m0);
	return (ENOBUFS);
    }
    /* pad small packet */
    if ((padsz = MIN_TPKT - mbcnt) > 0) {
	last_frgm->f_bcnt += padsz;
	mbcnt = MIN_TPKT;
    }

    /* save mbuf list */
    txd->d_mbuf = m0;

    /* set up tx descriptor */
    txd->d_fsize = mbcnt;
    txd->d_cstat = TXCSTAT_REQ;
    last_frgm->f_bcnt |= F_LASTFRGM;
    txd->d_nxtf = 0;

    txdr->r_freed = (void *)txd->d_nxtd;
    txdr->r_ndesc--;

    ei->ei_if.if_obytes += mbcnt;

    /* acquire tx lock and update the standby tx list */ 
    s = mutex_spinlock(&txdr->r_slock);
    if (txdr->r_nxtd)
	((tlan_txfd_t *)txdr->r_lastd)->d_nxtf = KVTOPCIIO(ei, txd, PCIIO_CMD);
    else
	txdr->r_nxtd = (void *)txd;
    txdr->r_lastd = (void *)txd;
    mutex_spinunlock(&txdr->r_slock, s);

    /* transmit the packet if tx channel is ready */
    if (!txdr->r_busy && (s = mutex_spintrylock(&txdr->r_slock))) {
	if (txdr->r_nxtd) {
		txdr->r_actd = txdr->r_nxtd;
		txdr->r_nxtd = 0;
		txdr->r_busy = 1;

		/* start tx */
		TLAN_TX_GO(ei, txdr->r_actd);
	}
	mutex_spinunlock(&txdr->r_slock, s);
    }

    return (0);
}


/*
 * ecf_lafhash()
 *
 * Description:
 *    Given a multicast ethernet address, this routine calculates the 
 *    address's bit index in the logical address filter mask
 */
static u_char
ecf_lafhash (
    u_char *addr)
{
    return ((addr[5] & 0x3f) ^
    	    (((addr[4] & 0xf) << 2) | (addr[5] >> 6)) ^
	    (((addr[3] & 0x3) << 4) | (addr[4] >> 4)) ^
	    (addr[3] >> 2) ^
    	    (addr[2] & 0x3f) ^
    	    (((addr[1] & 0xf) << 2) | (addr[2] >> 6)) ^
	    (((addr[0] & 0x3) << 4) | (addr[1] >> 4)) ^
	    (addr[0] >> 2));
}


/*
 * ecf_ioctl()
 *
 * Description:
 *    ecf ioctl
 */
static int
ecf_ioctl (
    struct etherif *eif,
    int cmd,
    caddr_t data)
{
    ecf_info_t *ei = eiftoei(eif);
    struct mfreq *mfr = (struct mfreq *) data;
    union mkey *key = mfr->mfr_key;

    switch (cmd) {
	/* enable one of the multicast filter flags */
	case SIOCADDMULTI:
		mfr->mfr_value = ecf_lafhash(key->mk_dhost);
		if (LAF_TSTBIT(ei, mfr->mfr_value)) {
			ASSERT(mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
			ei->ei_lafcoll++;
			break;
		}
		ASSERT(!mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
		LAF_SETBIT(ei, mfr->mfr_value);
		if (ei->ei_nmulti == 0)
			ei->ei_if.if_flags |= IFF_FILTMULTI;
		ei->ei_nmulti++;
		if (!(ei->ei_if.if_flags & (IFF_ALLMULTI | IFF_PROMISC)))
			(void)ecf_init(eif, eif->eif_arpcom.ac_if.if_flags);
		break;

	/* disable one of the multicast filter flags */
	case SIOCDELMULTI:
		if (mfr->mfr_value != ecf_lafhash(key->mk_dhost))
			return (EINVAL);
		if (mfhasvalue(&eif->eif_mfilter, mfr->mfr_value)) {
			/* forget about this collision. */
			ei->ei_lafcoll--;
			break;
		}
		/*
	 	 * If this multicast address is the last one to map
		 * the bit numbered by mfr->mfr_value in the filter,
		 * clear that bit and update the chip.
	 	 */
		LAF_CLRBIT(ei, mfr->mfr_value);
		ei->ei_nmulti--;
		if (ei->ei_nmulti == 0)
			ei->ei_if.if_flags &= ~IFF_FILTMULTI;
		if (!(ei->ei_if.if_flags & (IFF_ALLMULTI | IFF_PROMISC)))
			(void)ecf_init(eif, eif->eif_arpcom.ac_if.if_flags);
		break;

	default:
		return (EINVAL);
    }

    return (0);
}


/*
 * ecf_watchdog()
 *
 * Description:
 *    Periodically poll the controller for input packets
 *    in case of losing any interrupt.
 *
 */

static void
ecf_watchdog (struct ifnet *ifp)
{
    ecf_info_t *ei;

    ei = eiftoei(ifptoeif(ifp));
    ASSERT(IFNET_ISLOCKED(ifp));
		    
    /* collect statistics */
    tlan_statistics(ei);

    /* check PHY status */
    tlan_phystatus(ei);

    /* check and reset rxeoc count */
    if (ei->ei_tlan.rxeoc > 1)
	cmn_err_tag(196,CE_ALERT, "%s%d: need more ecf_max_rxds (%d)",
		IF_ECF, ei->ei_unit, ei->ei_tlan.rxeoc);
    ei->ei_tlan.rxeoc = 0;

    /* check for lost interrupt */
    IFNET_UNLOCK(ifp);
#if IP32
    if_ecfintr(0, ei);
#else
    if_ecfintr(ei);
#endif
    IFNET_LOCK(ifp);

    /* try to recover enet if it is sick */
    if (ei->ei_sick > 1)
	(void) ecf_init(&ei->ei_eif, ei->ei_if.if_flags);
    else if (ei->ei_txpks != 0)	/* still have pending output? */
	ei->ei_sick++;

    ei->ei_if.if_timer = ECFDOG;

}


/****************************************************************************
 *  TI ThunderLAN 100 (Network Adaptor) Related Functions                   *
 ****************************************************************************
 */

/*
 * tlan_init()
 *
 * Description:
 *    Init TLAN AHR and DIO registers
 */
static void
tlan_init (
    ecf_info_t *ei)
{
    DWORD addr3210, addr5432, addr1054;
    WORD areg;
    BYTE macp;
    int i;

    /* make sure TLAN is in reset state */
    TLAN_RESET(ei);
    TLAN_AHR_SET_DWORD(ei, AHR_HOST_CMD_REG, HOST_CMD_INTR_OFF, DWMSK);

    /* avoid early broken link message from watchdog */
    ei->ei_tlan.ready = 0;

    /* detect and init MII/PHY, default 100tx */
    if ((ei->ei_tlan.phydev = tlan_phydev(ei, 0)) >= TLAN_MAX_PHYS_INDEX) {
	cmn_err_tag(197,CE_PANIC, "%s%d: no PHY (0x%x) was found!", IF_ECF, ei->ei_unit,
		ei->ei_tlan.phyid);
    }
    else if (ei->ei_tlan.phyid == TLAN_PHYID_TI_INTRL) {
	cmn_err_tag(198,CE_WARN, "%s%d: only 10Mb on-chip PHY was found!", IF_ECF,
		ei->ei_unit);

	ei->ei_tlan.speed = !FAST100Mb;
	ei->ei_tlan.fduplx = !FDUPLEX;
	macp = MACP_ONCHIP_PHY;

	/* only 100vg & on-chip phy need to turn on MII interrupt request */
	TLAN_DIO_SET_BYTE(ei, DIO_NET_SIO_REG, MIINTEN, BMSK);
	TLAN_DIO_SET_BYTE(ei, DIO_NET_MSK_REG, MIIRQMSK, BMSK);

	/* enable MII interrupt for internal 10Mb */
	TLAN_MII_WR(ei, ei->ei_tlan.phydev, MII_TLPHY_CTRL_REG, (PHY_CTRL_INTEN
		| TLAN_MII_RD(ei, ei->ei_tlan.phydev, MII_TLPHY_CTRL_REG)));

	/* set up 10Mb, half duplex, and no auto-negotiation */
	TLAN_MII_WR(ei, ei->ei_tlan.phydev, MII_GEN_CTRL_REG, 0);
    }
    else { /* TLAN_PHYID_NS_100TX, TLAN_PHYID_ICS_100TX */
	macp = MACP_CSMACD;

	/* auto-negotiation */
	tlan_autonego(ei);
    }
    /* set MAC protocol */
    TLAN_DIO_WR_BYTE(ei, DIO_NET_CFG_REG, (macp | ONE_FRAG | ONE_CHN), BMSK);

    /* enable network status interrupts */
    TLAN_DIO_SET_BYTE(ei, DIO_NET_MSK_REG, (HBEAT | TXSTOP | RXSTOP), BMSK);

    /* setup NetCmd register: no wrap, copy short frame & routed frame,
     * promiscuous mode, half duplex, broadcast, and tx pacing
     */
    TLAN_DIO_WR_BYTE(ei, DIO_NET_CMD_REG, (NWRAP | CSF |
	((ei->ei_if.if_flags & IFF_PROMISC) ? CAF : 0) |
	((ecf_txipp) ? TXPACE : 0)), BMSK);

    /* set up all multicast hash regs */
    if (ei->ei_if.if_flags & IFF_ALLMULTI)
	ei->ei_laf.laf_vec[0] = ei->ei_laf.laf_vec[1] = 0xffffffff;

    /* set up multicast hash regs */
    TLAN_DIO_WR_DWORD(ei, DIO_NET_HASH0_REG, ei->ei_laf.laf_vec[0], DWMSK);
    TLAN_DIO_WR_DWORD(ei, DIO_NET_HASH1_REG, ei->ei_laf.laf_vec[1], DWMSK);

    /* set up tx/rx burst size */
    TLAN_DIO_WR_BYTE(ei, DIO_NET_BSIZE_REG, (TX_BURST_256 | RX_BURST_256),
	BMSK);

    /* set tx interrupt hold time and turn on PCI interrupt */
#if 0
    TLAN_AHR_WR_DWORD(ei, AHR_HOST_CMD_REG, (HOST_CMD_INTR_ON |
	(HOST_CMD_LD_TMR | LD_TMR_4US)), DWMSK);
#endif
    TLAN_AHR_WR_DWORD(ei, AHR_HOST_CMD_REG, HOST_CMD_INTR_ON, DWMSK);

    /* enable TLAN */
    TLAN_DIO_SET_BYTE(ei, DIO_NET_CMD_REG, NRESET, BMSK);
 
   /* setup ethernet address */
    addr3210 = (ei->ei_ea.ea_vec[3] << 24) | (ei->ei_ea.ea_vec[2] << 16) |
	       (ei->ei_ea.ea_vec[1] << 8 ) | (ei->ei_ea.ea_vec[0]);
    addr5432 = (ei->ei_ea.ea_vec[5] << 24) | (ei->ei_ea.ea_vec[4] << 16) |
	       (ei->ei_ea.ea_vec[3] << 8 ) | (ei->ei_ea.ea_vec[2]);
    addr1054 = (addr3210 << 16) | (addr5432 >> 16);

    for (areg = DIO_NET_ADDR_REG, i = 0; i < 2; areg += 0xc, i++) { 
	TLAN_DIO_WR_DWORD(ei, areg, addr3210, DWMSK);
	TLAN_DIO_WR_DWORD(ei, areg + 0x4, addr1054, DWMSK);
	TLAN_DIO_WR_DWORD(ei, areg + 0x8, addr5432, DWMSK);
    }

    /* start receiving packet */
    ei->ei_rxdr.r_actd = ei->ei_rxdr.r_head;
    TLAN_RX_GO(ei, ei->ei_rxdr.r_actd);
}


/****************************************************************************
 *  TI ThunderLAN - MII (Media Independent Interface) Functions             *
 ****************************************************************************
 */
/*
 * tlan_mii_sync()
 *
 * Description:
 *    Send 32 cycles as MII synchronization pattern to MII interface
 */
static void
tlan_mii_sync (
    ecf_info_t *ei)
{
    int i;
    BYTE sio = TLAN_NETSIO_RD(ei);

    TLAN_NETSIO_WR(ei, (sio &= ~MTXEN));
    TLAN_NETSIO_WR(ei, (sio &= ~MCLK));

    /* delay a while */
    (void)TLAN_NETSIO_RD(ei);

    TLAN_NETSIO_WR(ei, (sio |= MCLK));

    /* delay a while again */
    (void)TLAN_NETSIO_RD(ei);

    TLAN_NETSIO_WR(ei, (sio |= (NMRST | MDATA)));

    /* generate 32 sync cycles */
    for (i = 0; i < MAX_MIISYNCS; i++)
	TLAN_NETSIO_MCLK(ei, sio);
}


/*
 * tlan_mii_init()
 *
 * Description:
 *    Initialize all possible MII interfaces
 */
static void
tlan_mii_init (
    ecf_info_t *ei)
{
    int phy;

    /* send synchronization sequence to MII interface */
    tlan_mii_sync(ei);

    /* turn all off */
    for (phy = 0; phy < 32; phy++)
	TLAN_MII_ISOLATE_PHY(ei, phy);

    /* put each potential MII interface in a known state */
    for (phy = 0; phy < 32; phy++) {

	/* Reset and keep isolated */
	TLAN_MII_WR(ei, phy, MII_GEN_CTRL_REG,
		(PHYRESET | PDOWN | LOOPBK | ISOLATE));

	/* need to sync again after reset */
	tlan_mii_sync(ei);

	/* isolate it again, just in case */
	TLAN_MII_ISOLATE_PHY(ei, phy);
    }
    tlan_mii_sync(ei);
}


/*
 * tlan_mii_io()
 *
 * Description:
 *    Read word from PHY MII
 *    MII frame read format:
 *
 *   start    operation  PHY      reg      turn          Data
 * delimiter    code     addr     addr    around
 * ----------------------------------------------------------------------
 * |   01   |   10   |  AAAAA  |  RRRRR  |  Z0  |  DDDD.DDDD.DDDD.DDDD  |
 * ----------------------------------------------------------------------
 *
 *
 * write word to PHY MII
 * MII frame write format:
 *
 *   start    operation  PHY      reg      turn          Data
 * delimiter    code     addr     addr    around
 * ----------------------------------------------------------------------
 * |   01   |   01   |  AAAAA  |  RRRRR  |  10  |  DDDD.DDDD.DDDD.DDDD  |
 * ----------------------------------------------------------------------
 */
static WORD
tlan_mii_io (
    BYTE op,
    ecf_info_t *ei,
    BYTE phydev,
    BYTE reg,
    WORD data)
{
    int i;
    WORD val;
    BYTE sio = TLAN_NETSIO_RD(ei);

    /* ICS requires preamble for each transaction */
    for (sio |= MDATA, i = 0; i < MAX_MIISYNCS; i++)
	TLAN_NETSIO_MCLK(ei, sio);

    /* disable MII interrupt, reset MDATA, and set MDCLK idle */
    sio &= ~(MIINTEN | MDATA);
    TLAN_NETSIO_WR(ei, (sio |= MCLK));

    /* enable sio tx */
    TLAN_NETSIO_WR(ei, (sio |= MTXEN));

    /* send start delimiter */
    TLAN_NETSIO_MCLK(ei, sio);			/* MDIO = 0 */

    TLAN_NETSIO_WR(ei, (sio &= ~MCLK));		/* latch fast SDEL into phy */

    TLAN_NETSIO_WR(ei, (sio |= MDATA));
    TLAN_NETSIO_WR(ei, (sio |= MCLK));		/* MDIO = 1 */

    /* send read/write operation code */
    if (op == MII_READ) {
	TLAN_MII_MDCLK(ei, sio, (sio | MDATA));	/* MDIO = 1 */
	TLAN_MII_MDCLK(ei, sio, (sio & ~MDATA));/* MDIO = 0 */
    }
    else {
	TLAN_MII_MDCLK(ei, sio, (sio & ~MDATA));	/* MDIO = 0 */
	TLAN_MII_MDCLK(ei, sio, (sio | MDATA));	/* MDIO = 1 */
    }

    /* send 5-bit phy dev#, MSB first, Internal=0x1f, External=0x00 */
    for (i = 0x10; i; i >>= 1)
	TLAN_MII_MDCLK(ei, sio, ((phydev & i)? (sio | MDATA) : (sio & ~MDATA)));

    /* send 5-bit register#, MSB first */
    for (i = 0x10; i; i >>= 1)
	TLAN_MII_MDCLK(ei, sio, ((reg & i) ? (sio | MDATA) : (sio & ~MDATA)));

    if (op == MII_READ) {	/* MII read */
	/* 802.3u specifies an idle bit time after the register
	 * address is sent.  This and the following zero bit are
	 * designated as "turn-around" cycles.
	 */
	/* enable sio rx and send turn around */
	TLAN_MII_MDCLK(ei, sio, (sio & ~MTXEN));	/* MDIO = Z */

	/* get PHY ack and read 16-bit data at MDCLK low */
	TLAN_NETSIO_WR(ei, (sio &= ~MCLK));		/* MDIO = 0 */
	if (!(TLAN_NETSIO_RD(ei) & MDATA)) {
		TLAN_NETSIO_WR(ei, (sio |= MCLK));
		for (val = 0, i = 0x8000; i; i >>= 1) {
			TLAN_NETSIO_WR(ei, (sio &= ~MCLK));
			if (TLAN_NETSIO_RD(ei) & MDATA)
				val |= i;
			TLAN_NETSIO_WR(ei, (sio |= MCLK));
		}
    	}
	else {	/* clock out invalid 16-bit data */
		for (i = 0; i < 17; i++)
			TLAN_NETSIO_MCLK(ei, sio);
		val = 0xffff;
	}
	/* quiescent cycle? */
	TLAN_NETSIO_MCLK(ei, sio);
    }
    else {	/* MII write */
	/* turn around */
	TLAN_MII_MDCLK(ei, sio, (sio | MDATA));	/* MDIO = 1 */
	TLAN_MII_MDCLK(ei, sio, (sio & ~MDATA));/* MDIO = 0 */

	/* write data to MII, MSB first */
	for (i = 0x8000; i; i >>= 1)
		TLAN_MII_MDCLK(ei, sio,
			((data & i) ? (sio | MDATA) : (sio & ~MDATA)));

	/* disable sio tx */
	TLAN_MII_MDCLK(ei, sio, (sio & ~MTXEN));
    }

    /* re-enable MII interrupt */
    TLAN_NETSIO_SET(ei, MIINTEN);

    return (val);
}


/****************************************************************************
 *  TI ThunderLAN - EEPROM Functions                                        *
 ****************************************************************************
 */
/*
 * tlan_eeprom_seldev()
 * select EEPROM device to access (see Excel XL24C02 device specification)
 */
static void
tlan_eeprom_seldev (
    ecf_info_t *ei,
    BYTE op)
{
    int	i;
    BYTE bit, id = 0x50; /* { 1,0,1,0,0,0,0 }
			  * 1010: Exel XL24C02 device type id
			  * 000:  specifies first device on the bus
			  */
    /* write device id */
    for (i = 7, bit = 0x40; i; i--, bit >>= 1) {
	if (id & bit)
		TLAN_NETSIO_SET(ei, EDATA);
	else
		TLAN_NETSIO_CLR(ei, EDATA);
	    
	TLAN_NETSIO_EDCLK(ei);
    }
    /* read/write operation */
    if (op == EE_READ)
	TLAN_NETSIO_SET(ei, EDATA);
    else
	TLAN_NETSIO_CLR(ei, EDATA);
    TLAN_NETSIO_EDCLK(ei);
}


/*
 * tlan_eeprom_ack()
 * check for ack from eeprom
 */
static int
tlan_eeprom_ack (
    ecf_info_t *ei)
{
    int ack;

    TLAN_NETSIO_CLR(ei, ETXEN);
    TLAN_NETSIO_SET(ei, ECLK);
    ack = (TLAN_NETSIO_CHK(ei, EDATA) == 0);
    TLAN_NETSIO_CLR(ei, ECLK);

    return (ack);
}


/*
 * tlan_eeprom_sendbyte()
 * clock one byte out to eeprom
 */
static void
tlan_eeprom_sendbyte (
    ecf_info_t *ei,
    BYTE data)
{
    int i, bit;

    TLAN_NETSIO_SET(ei,ETXEN);
    for (i = 8, bit = 0x80; i; i--, bit >>= 1) {
	if (data & bit)
    		TLAN_NETSIO_SET(ei, EDATA);
	else
    		TLAN_NETSIO_CLR(ei, EDATA);
	TLAN_NETSIO_EDCLK(ei);
    }
}


/*
 * tlan_ee_io()
 *
 * Description:
 *    Read byte /write byte, word, dword from/to EEPROM
 */
static BYTE
tlan_eeprom_io (
    BYTE op,
    ecf_info_t *ei,
    BYTE addr,
    DWORD data,
    BYTE bcount,
    int	*acked)
{
    int i;
    BYTE bit, rdata;

    *acked = 0;

    /* send eeprom start sequence */
    TLAN_EEPROM_START_SEQ(ei);

    /* put eeprom device identifier, address and write command on bus */
    tlan_eeprom_seldev(ei, EE_WRITE);
    if (!tlan_eeprom_ack(ei))
	goto noack;

    /* send addr to eeprom to read from or write to */
    tlan_eeprom_sendbyte(ei, addr);
    if (!tlan_eeprom_ack(ei))
	goto noack;

    if (op == EE_READ) {
	/* send eeprom start access sequence */
	TLAN_EEPROM_START_ACCESS_SEQ(ei);

	/* change to eeprom read */
	tlan_eeprom_seldev(ei, EE_READ);
	if (!tlan_eeprom_ack(ei))
		goto noack;

	/* clock byte data in from eeprom */
	for (i = 8, rdata = 0, bit = 0x80; i; i--, bit >>= 1) {
		TLAN_NETSIO_SET(ei, ECLK);
		if (TLAN_NETSIO_CHK(ei, EDATA))
			rdata |= bit;
		TLAN_NETSIO_CLR(ei, ECLK);
	}
	/* send eeprom stop access sequence */
	TLAN_EEPROM_STOP_ACCESS_SEQ(ei);
    }
    else { /* op == EE_WRITE */

	/* clock bytes out to eeprom */
	for (i = 0; i < bcount; i++) {
		tlan_eeprom_sendbyte(ei, (BYTE)((data >> (8 * i)) & 0xff));
		if (!tlan_eeprom_ack(ei))
			goto noack;
    	}
	rdata = 1;	/* write complete */

	/* send eeprom stop sequence */
	TLAN_EEPROM_STOP_SEQ(ei);

	/* wait until eeprom writes the data, 
	 * when eeprom responds, internal write is complete
	 */
	do {
		TLAN_EEPROM_START_SEQ(ei);
		tlan_eeprom_seldev(ei, EE_WRITE);
	} while (!tlan_eeprom_ack(ei));
    }
    *acked = 1;

    noack:
    return (*acked ? rdata : 0);
}


/****************************************************************************
 *  TI ThunderLAN - PHY Functions                                           *
 ****************************************************************************
 */
/*
 * tlan_phydev()
 *
 * Description:
 *    Check if the given PHY exist
 */
static int
tlan_phydev (
    ecf_info_t *ei,
    int	n)
{
    int i;
    WORD idlo, idhi;
    DWORD id;

    /* initialize all potential PHYs on the MII interface
     * to insure PHYs are in a known state.
     */
    tlan_mii_init(ei);

    for (i = n; i < TLAN_MAX_PHYS_INDEX; i++) {
	/* enable PHY and sync MII interface before checking internal PHY */
	if (i == 31) {
		TLAN_DIO_SET_BYTE(ei, DIO_NET_CFG_REG, PHY_ENBL, BMSK);
		tlan_mii_sync(ei); /* or use tlan_mii_init(ei); */
	}
	if ((idlo = TLAN_MII_RD(ei, i, MII_GEN_ID_LO_REG)) &&
	    (idhi = TLAN_MII_RD(ei, i, MII_GEN_ID_HI_REG))) {

		/* construct phyid without rev# */
		id = (idhi << 16) | (idlo & 0xfff0);
		if (id == TLAN_PHYID_ICS_100TX ||
		    id == TLAN_PHYID_NS_100TX ||
		    id == TLAN_PHYID_TI_INTRL) {
			ei->ei_tlan.phyid = id;
			return (i);
		}
	}
    }
    return (i);
}


/*
 * tlan_autonego()
 *
 * Description:
 *    Auto-negotiate the network speed and duplex mode
 */
static void
tlan_autonego (
    ecf_info_t *ei)
{
    int i = 0;

    /* check any enforce setting: 0:auto; 1:10h, 2:10f, 3:100h, 4:100f */
    if (ecf_spdpx) {
	ei->ei_tlan.speed = (ecf_spdpx < 3) ? !FAST100Mb : FAST100Mb;
	ei->ei_tlan.fduplx = (ecf_spdpx % 2) ? !FDUPLEX : FDUPLEX;
	TLAN_MII_WR(ei, ei->ei_tlan.phydev, MII_GEN_CTRL_REG,
		(ei->ei_tlan.speed | ei->ei_tlan.fduplx));
        /* turn on link LED */
        ei->ei_physts.led |= LED_LNK;
        TLAN_DIO_WR_BYTE(ei, DIO_NET_LED_REG, ei->ei_physts.led, BMSK);
        IFF_DBG_PRINT(("%s%d: forced %s, %s-duplex.\n", IF_ECF, ei->ei_unit,
	  (ei->ei_tlan.speed) ? "100Mbps" : "10Mbps",
	  (ei->ei_tlan.fduplx) ? "full" : "half"));
	if (ei->ei_tlan.speed == FAST100Mb) {
		eiftoifp(&(ei->ei_eif))->if_baudrate.ifs_value = 1000*1000*100;
	} else {
		eiftoifp(&(ei->ei_eif))->if_baudrate.ifs_value = 1000*1000*10;
	}
	if (ei->ei_tlan.fduplx == FDUPLEX) {
		eiftoifp(&ei->ei_eif)->if_flags |= IFF_LINK0;
	} else {
		eiftoifp(&ei->ei_eif)->if_flags &= ~IFF_LINK0;
	}
	return;
    }

    /* start auto-negotiation */
    TLAN_MII_WR(ei, ei->ei_tlan.phydev, MII_GEN_CTRL_REG, AUTOENBL | AUTORSRT);

    /* wait for auto-negotiation complete */
    while (!(TLAN_MII_RD(ei, ei->ei_tlan.phydev, MII_GEN_STS_REG) &
	   GEN_STS_AN_CMPLT)) {

	if (++i > 300000) { /* max 3 sec. */
		cmn_err_tag(199,CE_ALERT, "%s%d: no carrier: check Ethernet cable",
			IF_ECF, ei->ei_unit);

		/* disable auto-negotiation, set speed and duplex mode */
		ei->ei_tlan.speed = !FAST100Mb;
		ei->ei_tlan.fduplx = !FDUPLEX;
		TLAN_MII_WR(ei, ei->ei_tlan.phydev, MII_GEN_CTRL_REG,
			(ei->ei_tlan.speed | ei->ei_tlan.fduplx));
		return;
	}
	DELAYBUS(10);
    }
    /* setup speed and duplex mode */
    tlan_setspdpx(ei);
}


/*
 * tlan_setspdpx()
 *
 * Description:
 *    Set up network speed, duplex mode, and turn on link led
 */
static void
tlan_setspdpx (
    ecf_info_t *ei)
{
    WORD sts, ablty;
    int t4 = FALSE;

    /* turn on link LED */
    ei->ei_physts.led |= LED_LNK;
    TLAN_DIO_WR_BYTE(ei, DIO_NET_LED_REG, ei->ei_physts.led, BMSK);

    /* National PHY: AN_LKPTNR may be 0; 100Tx: jabber; 10Tx: !jabber */
    if (ei->ei_tlan.phyid == TLAN_PHYID_NS_100TX) {
	/* read twice since the link bit is latched */
	sts = TLAN_MII_RD(ei, ei->ei_tlan.phydev, MII_GEN_STS_REG);
	ei->ei_tlan.speed = (sts & GEN_STS_JABBER) ? FAST100Mb : !FAST100Mb;
	ei->ei_tlan.fduplx = !FDUPLEX;
    }
    /* other PHY: get advertisement and link partner ability */
    else {
	ablty = TLAN_MII_RD(ei, ei->ei_tlan.phydev, MII_AN_ADVS_REG) &
		TLAN_MII_RD(ei, ei->ei_tlan.phydev, MII_AN_LKPTNR_REG);

	if (ablty & AN_100TX_T4) {
		ei->ei_tlan.speed = FAST100Mb;
		ei->ei_tlan.fduplx = FDUPLEX;
		t4 = TRUE;
	}
	else if (ablty & AN_100TX_FD) {
		ei->ei_tlan.speed = FAST100Mb;
#if 0
		ei->ei_tlan.fduplx = FDUPLEX;
#else
		cmn_err_tag(200,CE_WARN," %s%d: full-duplex not currently supported",
			IF_ECF, ei->ei_unit);
		ecf_spdpx = 3;
		ei->ei_tlan.fduplx = !FDUPLEX;
		tlan_autonego(ei);
#endif
	}
	else if (ablty & AN_100TX_HD) {
		ei->ei_tlan.speed = FAST100Mb;
		ei->ei_tlan.fduplx = !FDUPLEX;
	}
	else if (ablty & AN_10TX_FD) {
		ei->ei_tlan.speed = !FAST100Mb;
		ei->ei_tlan.fduplx = FDUPLEX;
	}
	else if (ablty & AN_10TX_HD) {
		ei->ei_tlan.speed = !FAST100Mb;
		ei->ei_tlan.fduplx = !FDUPLEX;
	}
	else {
		cmn_err_tag(201,CE_WARN, "%s%d: auto-negotiation failed", IF_ECF,
			ei->ei_unit);
		ei->ei_tlan.speed = !FAST100Mb;
		ei->ei_tlan.fduplx = !FDUPLEX;
	}
    }

    IFF_DBG_PRINT(("%s%d: %s%s, %s-duplex.\n", IF_ECF, ei->ei_unit,
	(ei->ei_tlan.speed) ? "100Mbps" : "10Mbps",
	(t4) ? " T4" : "", (ei->ei_tlan.fduplx) ? "full" : "half"));
}


/*
 * tlan_phystatus()
 *
 * Description:
 *    Check TLAN PHY status
 */
static void
tlan_phystatus (
    ecf_info_t *ei)
{
    WORD sts;

    /* check link status */
    if (!((sts = TLAN_MII_RD(ei, ei->ei_tlan.phydev, MII_GEN_STS_REG)) &
	GEN_STS_LINK)) {
	/* turn off link LED */
	if (!(ei->ei_physts.sts & GEN_STS_NO_LINK)) {
		ei->ei_physts.sts |= GEN_STS_NO_LINK;
		if (ei->ei_tlan.ready) {
		    cmn_err_tag(202,CE_ALERT, "%s%d: no carrier: check Ethernet cable",
			IF_ECF, ei->ei_unit);
		}
		ei->ei_physts.led &= ~LED_LNK;
		TLAN_DIO_WR_BYTE(ei, DIO_NET_LED_REG, ei->ei_physts.led, BMSK);
	}
	/* for ICS PHY re-start auto-negotiation every 4 ECFDOG */
	if (!ecf_spdpx /*&& ei->ei_tlan.ready*/ &&
	    !(ei->ei_physts.brkn = (ei->ei_physts.brkn + 1) % 4) &&
	    ei->ei_tlan.phyid == TLAN_PHYID_ICS_100TX) {
		tlan_mii_init(ei);
		TLAN_MII_WR(ei, ei->ei_tlan.phydev, MII_GEN_CTRL_REG,
			(AUTOENBL | AUTORSRT));
	}
    }
    else {
	if (ei->ei_physts.sts & GEN_STS_NO_LINK) { /* turn on link LED */
		ei->ei_physts.sts &= ~GEN_STS_NO_LINK;
		ei->ei_physts.brkn = 0;

		/* setup speed and duplex mode */
		if (!ecf_spdpx)
			tlan_setspdpx(ei);

		if (ei->ei_tlan.ready)
			cmn_err_tag(203,CE_ALERT,
				"%s%d: network link restored. %s, %s-duplex.",
				IF_ECF, ei->ei_unit,
				(ei->ei_tlan.speed) ? "100Mbps" : "10Mbps",
				(ei->ei_tlan.fduplx) ? "full" : "half");
	}
	ei->ei_tlan.ready = 1;
    }

    /* check remote fault */
    if (sts & GEN_STS_RFLT) {
	if (!(ei->ei_physts.sts & GEN_STS_RFLT)) {
		ei->ei_physts.sts |= GEN_STS_RFLT;
		cmn_err_tag(204,CE_ALERT, "%s%d: remote fault detected",
			IF_ECF, ei->ei_unit);
	}
    }
    else
	ei->ei_physts.sts &= ~GEN_STS_RFLT;

    /* check jabber condition */
    if ((sts & GEN_STS_JABBER) && ei->ei_tlan.speed != FAST100Mb) {
	if (!(ei->ei_physts.sts & GEN_STS_JABBER)) {
		ei->ei_physts.sts |= GEN_STS_JABBER;
		cmn_err_tag(205,CE_ALERT, "%s%d: jabber detected", IF_ECF, ei->ei_unit);
	}
    }
    else
	ei->ei_physts.sts &= ~GEN_STS_JABBER;
}


/*
 * tlan_statistics()
 *
 * Description:
 *    Collect TLAN statistics
 */
static void
tlan_statistics (
    ecf_info_t *ei)
{
    DWORD sts;
    uint txundrrun, rxovrrun, crcerr, codeerr,
	 multcolli, snglcolli, excscolli, latecolli;

    /* get tx good frames & under-run */
    sts = TLAN_DIO_RD_DWORD(ei, (DIO_NETSTS_REG | DIO_ADDR_INC), DWMSK);
    ei->ei_stats.goodtxfrm += NET_STS_GOOD_FRAMES(sts);
    ei->ei_stats.txundrrun += (txundrrun = NET_STS_UNDER_RUNS(sts));
 
    /* get rx good frames & over-run */
    sts = TLAN_DIO_RD_DATA_DWORD(ei);
    ei->ei_stats.goodrxfrm += NET_STS_GOOD_FRAMES(sts);
    ei->ei_stats.rxovrrun += (rxovrrun = NET_STS_OVER_RUNS(sts));

    /* get deferred tx frames, crc error frames, and code error frames */
    sts = TLAN_DIO_RD_DATA_DWORD(ei);
    ei->ei_stats.dfrtxfrm += NET_STS_TXDFR_FRAMES(sts);
    ei->ei_stats.crcerr += (crcerr = NET_STS_CRCERR_FRAMES(sts));
    ei->ei_stats.codeerr += (codeerr = NET_STS_CODERR_FRAMES(sts));

    /* get multi-collision & single-collision tx frames */
    sts = TLAN_DIO_RD_DATA_DWORD(ei);
    ei->ei_stats.multcolli += (multcolli = NET_STS_MULTCOLLI_FRAMES(sts));
    ei->ei_stats.snglcolli += (snglcolli = NET_STS_SNGLCOLLI_FRAMES(sts));

    /* excessive-collision, late-collision, carrier loss, & adapter commit */
    sts = TLAN_DIO_RD_DATA_DWORD(ei);
    ei->ei_stats.excscolli += (excscolli = NET_STS_EXCSCOLLIS(sts));
    ei->ei_stats.latecolli += (latecolli = NET_STS_LATECOLLIS(sts));
    ei->ei_stats.nocarrier += NET_STS_CARRIER_LOST(sts);

    ei->ei_if.if_oerrors += txundrrun;
    ei->ei_if.if_ierrors = rxovrrun + crcerr + codeerr;
    ei->ei_if.if_collisions += multcolli + snglcolli + excscolli + latecolli;
    
    ei->ei_if.if_odrops = ei->ei_stats.txdrp;

    if (ei->ei_if.if_flags & IFF_DEBUG) {
	printf("%s%d: network statistics\n", IF_ECF, ei->ei_unit);
	printf("     tx pkg: good: %d, underrun: %d, deferred: %d, dropped: %d\n",
		ei->ei_stats.goodtxfrm, ei->ei_stats.txundrrun,
		ei->ei_stats.dfrtxfrm, ei->ei_stats.txdrp);
	printf("     rx pkg: good: %d, overrun: %d, crc error: %d, code error: %d, dropped: %d\n",
		ei->ei_stats.goodrxfrm, ei->ei_stats.rxovrrun,
		ei->ei_stats.crcerr, ei->ei_stats.codeerr, ei->ei_stats.rxdrp);
	printf("     collision: multiple: %d, single: %d, excess: %d, late: %d\n",
		ei->ei_stats.multcolli, ei->ei_stats.snglcolli,
		ei->ei_stats.excscolli, ei->ei_stats.latecolli);
	printf("     carrier: lost: %d\n", ei->ei_stats.nocarrier);
    }
}


/****************************************************************************
 *  PCI Infrastructure Functions                                            *
 ****************************************************************************
 */

/*
 * if_ecfinit()
 *
 * Description:
 *    Called once by edtinit() at system startup time.
 *    It must probe for all working PCI ethernet devices and set up the
 *    necessary data structures and then calls ether_attach for each ethernet.
 *    Only support one adapter at this point.
 */
void
if_ecfinit (void)
{
    /* register all supported adapters */
    pciio_driver_register(TLAN_CPQ_VNDID, TLAN_CPQ_DEVID, "if_ecf", 0);
    pciio_driver_register(TLAN_TI_VNDID, TLAN_TI_DEVID, "if_ecf", 0);
}


/*
 * if_ecfattach()
 *
 * Description:
 *    Called by pci_edtinit to register TLAN controller to PCI infra structure
 *    which takes care of PCI initialization.
 */
int
if_ecfattach (
    vertex_hdl_t vhdl)
{
    vertex_hdl_t ecf_vhdl = vhdl;
    ecf_info_t *ei;
    tlan_pci_t *pci;
    pciio_info_t pci_info;
    inventory_t *inv_ptr = NULL;
    tlan_dr_t *txdr, *rxdr;
    int pfn, epaddr, i, pn, erc = 0, acked = 0;
    char alias[8];
    struct ps_parms ps_params;

    /* set up hwgraph
     * add a new ecf vertex to the hwgraph
     */
    if ((erc = hwgraph_char_device_add(vhdl, EDGE_LBL_ECF, "if_ecf", &ecf_vhdl))
	!= GRAPH_SUCCESS) {
	cmn_err_tag(206,CE_WARN, "%s%d: cannot create private hwgraph vertex", IF_ECF,
		ecf_no);
	return (erc);
    }

    /* allocate ecf info struct */
    if (!(ei = (ecf_info_t *)kmem_zalloc(sizeof(ecf_info_t), KM_SLEEP))) {
	cmn_err_tag(207,CE_ALERT, "%s%d: no memory for new interface", IF_ECF, ecf_no);
	return (ENOMEM);
    }

    /* save ecf device information */
    device_info_set(ecf_vhdl, (void *)ei);
    ei->ei_ecfvhdl = ecf_vhdl;
    ei->ei_vhdl = vhdl;
    ei->ei_unit = ecf_no++;

    /* set up device descriptor */
    ei->ei_devdesc = device_desc_dup(ecf_vhdl);
    device_desc_intr_name_set(ei->ei_devdesc, EDGE_LBL_ECF);
    device_desc_intr_swlevel_set(ei->ei_devdesc, (ilvl_t)splimp);
    device_desc_default_set(ecf_vhdl, ei->ei_devdesc);

    /* set up PCI configuration */
    pci = &ei->ei_pci;

    /* get configuration space */
    pci->p_cfgaddr =
	(__psunsigned_t)pciio_piotrans_addr(
		vhdl, 0,		/* device and (override) dev_info */
		PCIIO_SPACE_CFG,	/* select config address space */
		0,			/* from the start of space */
		PCI_CFG_VEND_SPECIFIC,	/* vendor specific stuff */
		0);			/* flag */

    /* get memory base address */
    ei->ei_io = pci->p_baseaddr =
	(__psunsigned_t)pciio_piotrans_addr(
		vhdl, 0,		/* device and (override) dev_info */
		PCIIO_SPACE_WIN(1),	/* select memory base address */
		0,			/* from the start of space, */
		1024,			/* PCI_32MB + 100, byte count */
		0);			/* flag */

#if 1
    if (pciio_endian_set(vhdl, PCIDMA_ENDIAN_LITTLE, PCIDMA_ENDIAN_BIG) != 
	PCIDMA_ENDIAN_BIG)
	cmn_err_tag(208,CE_WARN, "%s%d: cannot set to big endian", IF_ECF, ei->ei_unit);
#endif

    /* get slot no., vender id, device id, and revision */
    pci_info = pciio_info_get(vhdl);
    pci->p_dev = pciio_info_device_id_get(pci_info);
    pci->p_vndr = pciio_info_vendor_id_get(pci_info);
    pci->p_bus = pciio_info_bus_get(pci_info);
    pci->p_slot = pciio_info_slot_get(pci_info);
    pci->p_rev = PCI_CFG_RD(pci, PCI_CFG_REV_ID) & LSB_MSK;

    /* enable bus master and memory space mapping */
    PCI_CFG_WR(pci, PCI_CFG_COMMAND, ((PCI_CFG_RD(pci, PCI_CFG_COMMAND) &
	MSW_MSK) | (PCI_CMD_BUS_MASTER | PCI_CMD_MEM_SPACE)));

    /* set up the cache line and latency timer */
    PCI_CFG_WR(pci, PCI_CFG_CACHE_LINE, ((PCI_CFG_RD(pci, PCI_CFG_CACHE_LINE) &
	MSW_MSK) | ((PCI_LATENCY_64 << 8) | PCI_CACHELINE_128_BYTE)));

    /* set up ecf */

    /* get the burned in ethernet address from eeprom. */
    for (epaddr = TLAN_EEPROM_EADDR, i = 0; i < 6; i++, epaddr++)
	ei->ei_curaddr[i] = ei->ei_ea.ea_vec[i] =
		TLAN_EEPROM_RD(ei, epaddr, &acked);

    /* allocate memory for tx/rx descriptors. */
    rxdr = &ei->ei_rxdr;
    pn = (ecf_max_rxds * TLAN_RXDSZ - 1) / NBPP + 1;
    if (!(pfn = contig_memalloc(pn, 1, (VM_NOSLEEP | VM_DIRECT | VM_UNCACHED))))
	return (ENOMEM);
    rxdr->r_head = small_pfntova_K1(pfn);
    rxdr->r_tail = 0;
    
    txdr = &ei->ei_txdr;
    pn = (ecf_max_txds * TLAN_TXDSZ - 1) / NBPP + 1;
    if (!(pfn = contig_memalloc(pn, 1, (VM_NOSLEEP | VM_DIRECT | VM_UNCACHED))))
	return (ENOMEM);
    txdr->r_head = small_pfntova_K1(pfn);
    txdr->r_tail = 0;

    /* init tx spin lock */
    spinlock_init(&txdr->r_slock, NULL);

    /* attach ethernet interface but don't do add_to_inventory (-1) now */
    ether_attach(&ei->ei_eif, IF_ECF, ei->ei_unit, (caddr_t)ei, &ecfops,
    	&ei->ei_ea, INV_ETHER_ECF, -1);

    /* set up interrupt handler
     * after ether_attach to avoid immature interrupt
     */
    if (!(pci->p_intr = pciio_intr_alloc(vhdl, ei->ei_devdesc,
	PCIIO_INTR_LINE_A, ecf_vhdl))) {
	cmn_err_tag(209,CE_WARN, "%s%d: interrupt setup failed", IF_ECF, ei->ei_unit);
	erc = EIO;
	goto done;
    }
    pciio_intr_connect(pci->p_intr, (intr_func_t)if_ecfintr, (intr_arg_t)ei,
	(void *)0);

    /* set up hinv database.
     * when the bus was scanned during boot up, out card was added automaticly.
     * now find that record and update it with whatever info we want.
     */
    if (!(inv_ptr = find_inventory(inv_ptr, INV_NETWORK, INV_NET_ETHER,
	INV_ETHER_ECF, ei->ei_unit, pci->p_dev)))
	device_inventory_add(vhdl, INV_NETWORK, INV_NET_ETHER, INV_ETHER_ECF,
		((ei->ei_unit << 24) | (pci->p_bus << 16) | (pci->p_slot << 8) |
		pci->p_rev), ((pci->p_vndr << 16) | pci->p_dev));
    else
	replace_in_inventory(inv_ptr, INV_NETWORK, INV_NET_ETHER, INV_ETHER_ECF,
		((ei->ei_unit << 24) | (pci->p_bus << 16) | (pci->p_slot << 8) |
		pci->p_rev), ((pci->p_vndr << 16) | pci->p_dev));

    /* add ecf alias at /hw/net */
    sprintf(alias, "%s%d", EDGE_LBL_ECF, ei->ei_unit);
    (void)if_hwgraph_alias_add(ecf_vhdl, alias);

    if (ei->ei_tlan.speed & FAST100Mb)
	    ps_params.bandwidth = 100000000;
    else
	    ps_params.bandwidth = 10000000;
    ps_params.flags = 0;
    ps_params.txfree = ecf_max_txds;
    ps_params.txfree_func = ecf_txfree_len;
    ps_params.state_func = ecf_setstate;
    (void)ps_init(eiftoifp(&ei->ei_eif), &ps_params);

    done:
    return (erc);
}


/*
 * if_ecfdetach()
 *
 * Description:
 *    Called by pciio_driver_unregister to unregister TLAN controller
 *    to PCI infra structure
 */
int
if_ecfdetach (
    vertex_hdl_t vhdl)
{
    ecf_info_t *ei;
    vertex_hdl_t ecf_vhdl;
    char alias[8];

    if (hwgraph_traverse(vhdl, EDGE_LBL_ECF, &ecf_vhdl) == GRAPH_NOT_FOUND ||
	!(ei = (ecf_info_t *)device_info_get(ecf_vhdl))) {
	cmn_err_tag(210,CE_WARN,"ecf: cannot get ecf vertex or device information");
	return (EIO);
    }

    /* unregister the error handler */
    pciio_error_register(vhdl, NULL, NULL);

#ifdef DEBUG
    printf("if_ecfdetach: vhdl=0x%x\n", vhdl);
#endif

    /* shutdown the network */
    IFNET_LOCK(&ei->ei_if);

    if_down(ifnet);
    ei->ei_if.if_flags &= ~IFF_ALIVE;
    ei->ei_if.if_timer = 0;
    ei->ei_if.if_output = (int (*)(struct ifnet*, struct mbuf*,struct sockaddr*,
#ifndef IP30
	struct rtentry *
#endif
	))nodev;
    ei->ei_if.if_ioctl = (int (*)(struct ifnet *, int, void *))nodev;
    ei->ei_if.if_reset = (int (*)(int, int))nodev;
    ei->ei_if.if_watchdog = (void(*)(struct ifnet *))nodev;

    /* disconnect edge */
    hwgraph_edge_remove(vhdl, EDGE_LBL_ECF, 0);
    hwgraph_vertex_destroy(ecf_vhdl);

    /* remove ecf alias from /hw/net */
    sprintf(alias, "%s%d", EDGE_LBL_ECF, ei->ei_unit);
    (void)if_hwgraph_alias_remove(alias);

    /* destroy spin lock */
    spinlock_destroy(&ei->ei_txdr.r_slock);

    /* unregister interrupt */
    pciio_intr_disconnect(ei->ei_pci.p_intr);
    pciio_intr_free(ei->ei_pci.p_intr);

    /* free device descriptor */
    device_desc_free(ei->ei_devdesc);

    /* free all tx/rx descriptors */
    ecf_reset(&ei->ei_eif);

    IFNET_UNLOCK(&ei->ei_if);

    /* free ei info */
    kmem_free(ei, sizeof(ecf_info_t));

    return (0);
}

/*
 * RSVP functions 
 */

/*
 * Called by packet scheduling functions to determine length of driver queue.
 */
static int
ecf_txfree_len(struct ifnet *ifp)
{
	ecf_info_t *ei = (ecf_info_t *) ifp;

	return (ei->ei_txdr.r_ndesc);
}

/*
 * Called by packet scheduling functions to notify driver about queueing state.
 * If setting is 1 then there are queues and driver should try to minimize
 * delay (ie take intr per packet).  If 0 then driver can optimize for
 * throughput (ie. take as few intr as possible).
 */
static void
ecf_setstate(struct ifnet *ifp, int setting)
{
	ecf_info_t *ei = (ecf_info_t *) ifp;

	if (setting)
		ei->ei_flags |= EIF_PSENABLED;
	else
		ei->ei_flags &= ~EIF_PSENABLED;
}


#endif /* IP32 */
