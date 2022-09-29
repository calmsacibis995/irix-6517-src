/*
 * Moosehead MACE 10/100 Mbit/s Fast Ethernet Interface Driver
 *
 * Copyright 1995, Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.28 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/sysmacros.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/edt.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/mbuf.h"
#include "sys/sbd.h"
#include "sys/socket.h"
#include "sys/cpu.h"
#include "net/if.h"
#include "net/raw.h"
#include "net/soioctl.h"
#include "net/rsvp.h"
#include "misc/ether.h"
#include "sys/kmem.h"
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/if_ether.h"
#include "netinet/ip.h"
#include "string.h"
#include "sys/idbgentry.h"
#include "sys/if_me.h"
#include "sys/mii.h"
#include "sys/atomic_ops.h"
#include "sys/ddi.h"
#include "ksys/cacheops.h"
#include "sys/hwgraph.h"
#include "sys/iograph.h"
#include "sys/timers.h"
#include "sys/PCI/pciio.h"

/* config from master.d */
extern int me_hdwrcksum_enable;
extern int me_rxdelay;
extern int me_hdwrgather_enable;
extern int me_fullduplex_ipg[];
extern int me_halfduplex_ipg[];
extern struct phyerrata me_phyerrata[];

int if_medevflag = D_MP;

/* General MACE ethernet hardware defines */
#define	RCVBUF_SIZE			4096
#define	RCVDMA_SIZE			(1536+128)
#define	TX_RING_SIZE			128
#define RX_VALID_PACKET                 0x80000000
#define	MSGCL_FIFO_SIZE			16
#define	ETHER_HDRLEN			14
#define	CRCLEN				4
#define	ETHERMAXLEN			1518
#define	ETHERMINLEN			64
#define	DMA_PADDING			2
#define	DMA_OFFSET			(mif->rx_boffset + DMA_PADDING)
#define	DEADPACKET			0x7E0001
#define	DEADMAN_TIMEOUT			2

#define	BLOCKROUND(x, blk) 		(((x) + (blk) - 1) & ~((blk) - 1))
#define	FIFOINDEX(x,y)			((x) & ((y) - 1))
#define	RXRINGINDEX(x)			((x) & (MSGCL_FIFO_SIZE - 1))
#define	RXFIFOINDEX(x)			((x) & ((MSGCL_FIFO_SIZE * 2) - 1))
#define	TXFIFOINDEX(x)			((x) & (TX_RING_SIZE - 1))

#define	ei_ac		eif.eif_arpcom	/* common arp stuff */
#define	ei_if		ei_ac.ac_if	/* network-visible interface */
#define	ei_rawif	eif.eif_rawif	/* raw domain interface */

#define rx_control	receive_alias
#define tx_control	transmit_alias

/* TX fifo entry */
typedef union{
  unsigned long long TXCmd;
  unsigned long long TXStatus;
  unsigned long long TXConcatPtr[4];
  unsigned long long TXData[16];
  char               buf[128];
} TXfifo;

/* RX and TX buffers */
typedef union{
  unsigned long long RXStatus;
  char               buf[2048];
} RXbuf;

typedef union{
  unsigned long long TXStatus;
  char               buf[2048];
} TXbuf;

#define	DMA_RX_PAD	2+8		/* RX padding at front of buffer */

#define	PHYS_INIT	0x01
#define	PHYS_LOCK	0x02
#define	PHYS_START	0x04
#define	PHYS_WASUP	0x08
#define	PHYS_WASDOWN	0x10
#define	PHYS_UPDATE	0x20
#define	PHYS_JABBER	0x40
#define	PHYS_FAULT	0x80

static struct maceif {
	/* Common Ethernet interface */
	struct etherif		eif;

	/* Multicast control */
	u_int			lafcoll;
	u_int			nmulti;
	long long		mlaf;

	/* Hardware structures */
	volatile struct mac110	*mac;

	/* Operations */
	u_int			mode;
	char			revision;

	/* Phy xcvr info */
	signed char		phyaddr;
	char			phyrev;
	char			phystatus;
	int			phytype;

	/* Transmit ring buffer */
	short			tx_rptr, tx_wptr;
	int			tx_free_space;
	struct mbuf		*tx_mfifo[TX_RING_SIZE];
	volatile TXfifo		*tx_fifo;

	/* Transmit watchdog */
	short			tx_timer;
	short			tx_timeout;
	int			tx_watchdog_resets;

	/* Performance */
	int			tcase[8];

	/* Receive message cluster FIFO */
	short			rx_rptr, rx_rlen;
	struct mbuf		*rx_mfifo[MSGCL_FIFO_SIZE];
	int			rx_boffset;

	/* Statistics */
	int			tx_ring_errors;
	int			rx_fifo_errors;
	int			rx_underflow_count;
	int			rx_getisr_value_changed;
	int			rx_getisr_bad_value;

	/* TX link stats */
	int			tx_late_collisions;
	int			tx_crc_error;
	int			tx_deferred;
	int			tx_aborted_too_long;
	int			tx_aborted_underrun;
	int			tx_dropped_collisions;
	int			tx_canceled_deferral;
	int			tx_dropped_late_collision;
	
	/* RX link stats */
	int			rx_multicast;
	int			rx_broadcast;
	int			rx_hdwr_cksum;
	int			rx_code_violation;
	int			rx_dribble_nibble;
	int			rx_crc_error;
        int                     rx_not_valid;

	/* RSVP */
	short			rsvp_intfreq;
	short			rsvp_flags;

	/* Control variables */
	char			ctl_console_messages;

	/* Recovery timestamp */
	int			rt_in_intr;
	int			rt_intr_not_valid_base;
	int			rt_intr_not_valid_limit;
	int			rt_not_valid_resets;
} mace_ether;

#define MIF_PSENABLED		0x10
#define MIF_PSINITED		0x20

static int mace_ether_init(struct etherif *, int);
static void mace_ether_reset(struct etherif *);
static void mace_ether_watchdog(struct ifnet *);
static int mace_ether_output(struct etherif *, struct etheraddr *,
	struct etheraddr *, u_short, struct mbuf *);
static int mace_ether_ioctl(struct etherif *, int, void *);
static void mace_ether_timer(struct maceif *);

static struct etherifops meops = {
	mace_ether_init, mace_ether_reset, mace_ether_watchdog,
	mace_ether_output,
	(int (*)(struct etherif *, int, void *))mace_ether_ioctl
};

static void mace_ether_intr(int);
static int mace_ether_receive(struct maceif *, int);
static void mace_ether_transmit_complete(struct maceif *);
static void mace_ether_transmit_stats(struct maceif *, unsigned);
static void mace_ether_dump(int);


/*
 * RSVP
 * Called by packet scheduling functions to determine length of driver queue.
 */
static int
me_txfree_len(struct ifnet *ifp)
{
	struct maceif *mif = (struct maceif *) (ifptoeif(ifp))->eif_private;
	/*
	 * must do garbage collection here, otherwise, fewer free
	 * spaces may be reported, which would cause packet scheduling
	 * to not send down packets.
	 */
	mace_ether_transmit_complete(mif);
	return (mif->tx_free_space);
}

/*
 * RSVP
 * Called by packet scheduling functions to notify driver about queueing state.
 * state.  If setting is 1 then there are queues and driver should update
 * the packet scheduler by calling ps_txq_stat() when the txq length changes.
 * If setting is 0, then don't call packet scheduler.
 */
/*ARGSUSED*/
static void
me_setstate(struct ifnet *ifp, int setting)
{
	if (setting)
		mace_ether.rsvp_flags |= MIF_PSENABLED;
	else
		mace_ether.rsvp_flags &= ~MIF_PSENABLED;
}

/*
 * Read a phy register over the MDIO bus
 */
static int
mace_ether_mdio_rd(register struct maceif *mif, int fireg)
{
	volatile struct mac110 *mac = mif->mac;
	volatile int rval;

	while ((rval = pciio_pio_read32(&mac->phy_dataio)) & MDIO_BUSY) {/* PIO RD 32 */
		us_delay(25);
	}
	pciio_pio_write32((mif->phyaddr << 5) | (fireg & 0x1f), 
		&mac->phy_address); /* PIO WR 32 */
	pciio_pio_write32(fireg, &mac->phy_read_start);	/* PIO WR 32 */
	us_delay(25);
	while ((rval = pciio_pio_read32(&mac->phy_dataio)) & MDIO_BUSY) {/* PIO RD 32 */
		us_delay(25);
	}

	return rval;
}

/*
 * Write a phy register over the MDIO bus
 */
static int
mace_ether_mdio_wr(register struct maceif *mif, int fireg, int val)
{
	volatile struct mac110 *mac = mif->mac;
	volatile int rval;

	while ((rval = pciio_pio_read32(&mac->phy_dataio)) & MDIO_BUSY) {/* PIO RD 32 */
		us_delay(25);
	}
	pciio_pio_write32((mif->phyaddr << 5) | (fireg & 0x1f), 
		&mac->phy_address); /* PIO WR 32 */
	pciio_pio_write32(val, &mac->phy_dataio);	/* PIO WR 32 */
	us_delay(25);
	while ((rval = pciio_pio_read32(&mac->phy_dataio)) & MDIO_BUSY) {/* PIO RD 32 */
		us_delay(25);
	}

	return val;
}

/*
 * Modify phy register using given mask and value
 */
static void
mace_ether_mdio_rmw(register struct maceif *mif, int fireg, int mask, int val)
{
	register int rval, wval;

	rval = mace_ether_mdio_rd(mif, fireg);
	wval = (rval & ~mask) | (val & mask);
	if (mif->ei_if.if_flags & IFF_DEBUG) {
		printf("!ec0: errata mdio addr=0x%x rval=0x%x wval=0x%x\n" +
		       mif->ctl_console_messages,
			fireg, rval, wval);
	}
	mace_ether_mdio_wr(mif, fireg, wval);
}

/*
 * Process ERRATA data for the PHY found on the MDIO bus
 */
static void
mace_ether_mdio_errata(register struct maceif *mif)
{
	register struct phyerrata *pe;

	for (pe = me_phyerrata; pe->type != 0; ++pe) {
		if (pe->type != mif->phytype)
			continue;
		if (pe->rev != mif->phyrev)
			continue;
		mace_ether_mdio_rmw(mif, pe->reg, pe->mask, pe->val);
	}
}

/*
 * Probe the management interface for PHYs
 */
static int
mace_ether_mdio_probe(register struct maceif *mif)
{
	register int i, val, p2, p3;

	/* already found the phy? */
	if ((mif->phyaddr >= 0) && (mif->phyaddr < 32))
		return mif->phytype;

	/* probe all 32 slots for a known phy */
	for (i = 0; i < 32; ++i) {
		mif->phyaddr = (char)i;
		p2 = mace_ether_mdio_rd(mif, 2);
		p3 = mace_ether_mdio_rd(mif, 3);
		val = (p2 << 12) | (p3 >> 4);
		switch (val) {
		    case PHY_QS6612X:
		    case PHY_ICS1889:
		    case PHY_ICS1890:
		    case PHY_DP83840:
			mif->phyrev = p3 & 0xf;
			mif->phytype = val;
			return val;
		}
	}
	mif->phyaddr = -1;

	return -1;
}

/*
 * Update our mode to match external xvcr.  We really only care about full
 * versus half duplex here.  Our speed select is a no-op in non-loopback
 * test modes.  So we really don't care about speed here.
 *
 * Note: if the partner doesn't support fast-link pulse auto-negotiation,
 *    we just assume half-duplex mode to be safe.  turns out that works as
 *    expected since most equipment that doesn't support FLP isn't capable
 *    of full duplex operation either.
 */
static int
mace_ether_link_update(register struct maceif *mif, int msr)
{
	register int mode, p5 = 0, p6, p17, p19, p31, val = 0;
	struct ps_parms ps_params;

	/*
	 * Only do once per link bounce
	 */
	if ((mif->phystatus & (PHYS_LOCK|PHYS_UPDATE)) != 0) {
		return (0);
	}

	/*
	 * Wait for auto-negotiation to succeed or fail
	 */
	if ((msr & MII_R1_AUTODONE) == 0) {
		return (EBUSY);
	}

	/*
	 * Setup defaults
	 */
	mode = mif->mode;
	mode &= ~(MAC_IPG | MAC_100MBIT | MAC_FULL_DUPLEX);

	/*
	 * Auto-negotiation completed, pick up result and
	 * set our operating mode accordingly.
	 */

	/*
	 * partner supports auto-negotiation?
	 */
	p6 = mace_ether_mdio_rd(mif, 6);
	if ((p6 & MII_R6_LPNWABLE) == 0) {
		switch (mif->phytype) {
		    case PHY_DP83840:
			/* National ERRATA, read reserved word on DP83840 */
			p19 = mace_ether_mdio_rd(mif, 25);
			if ((p19 & 0x40) == 0) {		/* 100Mb-TX */
				val |= MAC_100MBIT;
			}
			break;

		    case PHY_ICS1889:
		    case PHY_ICS1890:
			/* ICS speed/duplex detection register */
			p17 = mace_ether_mdio_rd(mif, 17);
			if (p17 & 0x8000) {			/* 100Mb-TX */
				val |= MAC_100MBIT;
			}
			if (p17 & 0x4000) {
				val |= MAC_FULL_DUPLEX;
			}
			break;

		    case PHY_QS6612X:
			/* Quality speed/duplex detection register */
			p31 = mace_ether_mdio_rd(mif, 31);
			if (p31 & 0x8) {			/* 100Mb-TX */
				val |= MAC_100MBIT;
			}
			if (p31 & 0x10) {
				val |= MAC_FULL_DUPLEX;
			}
			break;

		    default:
			goto anlpar;
		}
	} else {
anlpar:
		/*
		 * Pick up auto-negotiation status from	ANLPAR registers
		 */
		p5 = mace_ether_mdio_rd(mif, 4) & mace_ether_mdio_rd(mif, 5);
		if (p5 & MII_R5_T4) {				/* 100Mb-T4 */
			val |= MAC_100MBIT;
		} else if (p5 & MII_R4_TXFD) {			/* 100Mb-TX */
			val |= MAC_100MBIT | MAC_FULL_DUPLEX;
		} else if (p5 & MII_R4_TX) {			/* 100Mb-HD */
			val |= MAC_100MBIT;
		} else if (p5 & MII_R4_10FD) {			/*  10Mb-FD */
			val |= MAC_FULL_DUPLEX;
		}
	}

	/* Print info about negotiated link type */
	if (mif->ei_if.if_flags & IFF_DEBUG) {
		printf("!ec0: %d Mbits/s %s%s-duplex\n" +
		       mif->ctl_console_messages,
			(val & MAC_100MBIT) ? 100 : 10,
			(p5 & MII_R5_T4) ? "T4 " : "",
			(val & MAC_FULL_DUPLEX) ? "full": "half");
	}

	/* update mac mode */
	mode |= val;
		
	/* set ipg based on full or half duplex */
	if (val & MAC_FULL_DUPLEX) {
		(&(mif->eif.eif_arpcom.ac_if))->if_flags |= IFF_LINK0;
		if (me_fullduplex_ipg[0]) {
			mode |= me_fullduplex_ipg[0] << MAC_IPGT_SHIFT;
			mode |= me_fullduplex_ipg[1] << MAC_IPGR1_SHIFT;
			mode |= me_fullduplex_ipg[2] << MAC_IPGR2_SHIFT;
		} else {
			mode |= MAC_DEFAULT_IPG;
		}
	} else {
		(&(mif->eif.eif_arpcom.ac_if))->if_flags &= ~IFF_LINK0;
		if (me_halfduplex_ipg[0]) {
			mode |= me_halfduplex_ipg[0] << MAC_IPGT_SHIFT;
			mode |= me_halfduplex_ipg[1] << MAC_IPGR1_SHIFT;
			mode |= me_halfduplex_ipg[2] << MAC_IPGR2_SHIFT;
		} else {
			mode |= MAC_DEFAULT_IPG;
		}
	}
	mif->mode = mode;
	pciio_pio_write32(mode, &mif->mac->mac_control);	/*PIO WR 32 */
	pciio_pio_write32(ETHER_TX_DMA_ENABLE |
		pciio_pio_read32(&mif->mac->dma_control), 
		&mif->mac->dma_control);	/* PIO RD/WR 32 */

	/* done */
	mif->phystatus |= PHYS_UPDATE;

	if (mode & MAC_100MBIT) {
		(&(mif->eif.eif_arpcom.ac_if))->if_baudrate.ifs_value
			= 1000*1000*100;
		ps_params.bandwidth = 100000000;
		mif->rsvp_intfreq = 0xf;
	} else {
		(&(mif->eif.eif_arpcom.ac_if))->if_baudrate.ifs_value =
			1000*1000*10;
		ps_params.bandwidth = 10000000;
		mif->rsvp_intfreq = 1;
	}
	ps_params.flags = 0;
	ps_params.txfree = TX_RING_SIZE; 
	ps_params.txfree_func = me_txfree_len;
	ps_params.state_func = me_setstate;
 	ps_reinit(&(mif->eif.eif_arpcom.ac_if), &ps_params);

	return (0);
}

#define GETISR_TIME_LIMIT_HZ     100 /* 10 ms. */
#define GETISR_TIME_LIMIT_MIN    (100000000 / GETISR_TIME_LIMIT_HZ)
extern int fastick;
extern int fasthz;

/*ARGSUSED*/
void
if_meedtinit(struct edt *edtp)
{
	struct maceif *mif = &mace_ether;
	struct etheraddr ea;
	register int pfn, unit = 0;
	static int mainit;
	extern char eaddr[];
	graph_error_t rc;
	extern graph_error_t mace_hwgraph_lookup(vertex_hdl_t *);
	vertex_hdl_t mace_vhdl, ec_vhdl;
	char str[25];

	extern int network_intr_pri;

	/* once only */
	if (mainit) {
		return;
	}
	mainit = 1;

	/* should we bother to probe? */
	mif->mac = (volatile struct mac110 *)MACE_ETHER_ADDRESS;

	/* get ethernet address from system */
	bcopy(eaddr, ea.ea_vec, ETHERADDRLEN);
	
	/* print BSD style device present message on console? */
	mif->ctl_console_messages = (showconfig >= 2);
	if (showconfig) {
		printf("!ec%d: hardware ethernet address %s\n" +
		       mif->ctl_console_messages,
			unit, ether_sprintf(ea.ea_vec));
	}

	/* init filters */
	pciio_pio_write64(0, &mif->mac->physaddr);	/* PIO WR 64 */
	pciio_pio_write64(0, &mif->mac->secphysaddr);	/* PIO WR 64 */
	pciio_pio_write64(0, &mif->mac->mlaf);	/* PIO WR 64 */
	mif->mlaf = 0;

	/* get MACE ethernet hardware revision */
	mif->revision = pciio_pio_read32(&mif->mac->mac_control) >> MAC_REV_SHIFT;	/*PIO RD 32 */

	/* don't know phy address */
	mif->phyaddr = -1;

	/* register ethernet interface */
	mif->eif.eif_private = (caddr_t)mif;
	ether_attach(&mif->eif, "ec", unit, (caddr_t)mif,
		&meops, &ea, INV_ETHER_EC, mif->revision);

	/*
	 * In 6.3, ether_attach() would do this; not true in 6.5.
	 */
	add_to_inventory(INV_NETWORK, INV_NET_ETHER, INV_ETHER_EC, unit, 
		mif->revision);
	
	rc = mace_hwgraph_lookup(&mace_vhdl);

	if (rc == GRAPH_SUCCESS) {
		char alias[8];
		sprintf(str, "%s", EDGE_LBL_EC);
		sprintf(alias, "%s%d", EDGE_LBL_EC, unit);
		(void)if_hwgraph_add(mace_vhdl, str, "if_me", alias, &ec_vhdl);
	}
	idbg_addfunc("me_dump", (void (*)())mace_ether_dump);

	/* Force off CRIME interrupts */
	pciio_pio_write64(1 << (MACE_ETHERNET + + 16),
		&mif->mac->irltv.sintr_request);/* PIO WR 64 */

	/* attach to IP32 interrupt dispatch core */
	if (setcrimevector(MACE_INTR(MACE_ETHERNET), SPL5,
			(void(*)())mace_ether_intr, (int)mif, 0))
		cmn_err_tag(269,CE_ALERT, "ec0: could not set interrupt vector");

	/* start timer */
	timeout_pri(mace_ether_timer, mif, HZ, network_intr_pri+1);

	/* create transmit fifo: (16K) 128 entries */
	pfn = contmemall(4, 4, VM_DIRECT|VM_NOSLEEP);
	mif->tx_fifo = (TXfifo *)small_pfntova_K1(pfn);

	/* initialize time stamp */
	mif->rt_in_intr = 0;
	mif->rt_intr_not_valid_base = 0;
	mif->rt_intr_not_valid_limit = 100;
}

static void
mace_hdwrether_init(struct maceif *mif)
{
	register struct mbuf *m;
	register int boffset, i;
	struct timeval tv;

	/* reset the ethernet */
	pciio_pio_write32(MAC_RESET, &mif->mac->mac_control);	/* PIO WR 32 */
	pciio_pio_write32(0, &mif->mac->mac_control);	/* PIO WR 32 */

	/* set operating mode */
	pciio_pio_write32(mif->mode, &mif->mac->mac_control);	/* PIO WR 32 */

	/* initialize transmit fifo */
	bzero((void *)mif->tx_fifo, TX_RING_SIZE * sizeof (TXfifo));
	pciio_pio_write64(kvtophys((void *)mif->tx_fifo), 
		&mif->mac->tx_ring_base);/* PIO WR 64 */
	mif->tx_rptr = mif->tx_wptr = 0;
	mif->tx_free_space = TX_RING_SIZE;

	/* free any previously queued tx buffers (toss-em) */
	for (i = 0; i < TX_RING_SIZE; i++) {
		m_freem(mif->tx_mfifo[i]);
		mif->tx_mfifo[i] = NULL;
	}

	/* setup watchdog */
	mif->ei_if.if_timer = IFNET_SLOWHZ;
	mif->tx_timeout = 0;

	/* calculate proper receive fill offset */
	boffset = (sizeof (struct etherbufhead) -
		   sizeof (struct ether_header)) / sizeof (long long);

	/* setup receive message cluster list */
	mif->rx_rptr = 0;
	mif->rx_rlen = MSGCL_FIFO_SIZE;
	mif->rx_boffset = boffset * sizeof (long long);
	mif->tx_timer = me_rxdelay;
	pciio_pio_write32(me_rxdelay, &mif->mac->timer);	/* PIO WR 32 */

	/* free any previously used rx buffers */
	for (i = 0; i < mif->rx_rlen; i++) {
		m_freem(mif->rx_mfifo[i]);
		mif->rx_mfifo[i] = NULL;
	}
	for (i = 0; i < mif->rx_rlen; i++) {
		if ((m = m_vget(M_DONTWAIT, RCVBUF_SIZE, MT_DATA)) == NULL) {
			/* This should be a rare and serious enough event to print to console */
			if (!(mif->rx_rlen = i))
				printf("ec0: WARNING: not enough memory to allocate receive buffers\n");	
			break;
		}
		__vm_dcache_inval(mtod(m, caddr_t), RCVBUF_SIZE);
		pciio_pio_write32(kvtophys(mtod(m, caddr_t)),
			&mif->mac->rx_fifo);	/* PIO WR 32 */
		mif->rx_mfifo[i] = m;
	}

	/* set random seed register for backoff */
	microtime(&tv);
	pciio_pio_write32(tv.tv_usec, &mif->mac->backoff);	/*PIO WR 32 */

	/* set DMA control bits */
	pciio_pio_write32(
		ETHER_TX_DMA_ENABLE | DMA_TX_16K |
		ETHER_RX_DMA_ENABLE |
		ETHER_RX_INTR_ENABLE |
		(boffset << ETHER_RX_OFFSET_SHIFT) |
		(mif->rx_rlen << ETHER_RX_THRESH_SHIFT),
			&mif->mac->dma_control); /* PIO WR 32 */
}

static int
mace_ether_init(struct etherif *eif, int flags)
{
	register struct maceif *mif = (struct maceif *)eif->eif_private;
	register int mode = MAC_NORMAL, ctl = 0, error = 0;
	union {
	    char eaddr[8];
	    long long laddr;
	} eau;
	char *ec0mode;
	static char beenhere;
	struct ps_parms ps_params;

	/* reset the ethernet */
	pciio_pio_write32(MAC_RESET, &mif->mac->mac_control);	/*PIO WR 32 */
	pciio_pio_write32(0, &mif->mac->mac_control);	/*PIO WR 32 */
	mif->phystatus = 0;

	/* store station address in RAM */
	eau.laddr = 0;
	bcopy((caddr_t)mif->ei_ac.ac_enaddr,
		(caddr_t)&eau.eaddr[2], ETHERADDRLEN);
	pciio_pio_write64(eau.laddr, &mif->mac->physaddr);/*PIO WR 64 */

	/* get ec0mode from environment */
	ec0mode = kopt_find("ec0mode");

	/* valid station address? */
	if (ETHER_ISGROUP(mif->ei_ac.ac_enaddr)) {
		++error;
		cmn_err_tag(270,CE_ALERT, "ec0: invalid ethernet station address");
	}

	/* probe for the external transceiver */
	if (mace_ether_mdio_probe(mif) < 0) {
		++error;
		cmn_err_tag(271,CE_ALERT, "ec0: phy device not found, probe failed");
	}

	/* Load errata work-arounds into PHY */
	mace_ether_mdio_errata(mif);

	/* Check KOPT for ec0mode */
	if (ec0mode && ec0mode[0]) {
		/* note: if the user miss types, who cares? */
		if ((ec0mode[0] == 't') || (ec0mode[0] == 'T')) {
			/* external phy loopback test mode */
			ctl |= MII_R0_LOOPBACK | MII_R0_DUPLEX;
			mode |= MAC_FULL_DUPLEX;
		} else if ((ec0mode[0] == 'f') || (ec0mode[0] == 'F')) {
			/* full-duplex operating mode */
			ctl |= MII_R0_DUPLEX;
			mode |= MAC_FULL_DUPLEX;
		}
		if (((ec0mode[1] == '1') && (ec0mode[3] == '0')) ||
		    ((ec0mode[0] == '1') && (ec0mode[2] == '0'))) {
			/* 100Mbit operating speed */
			ctl |= MII_R0_SPEEDSEL;
			mode |= MAC_100MBIT;
		}
		mace_ether_mdio_wr(mif, 0, ctl);
		mif->phystatus |= PHYS_LOCK;
		if (!beenhere) {
			printf("!ec0: %s %d Mbits/s %s-duplex\n" +
			       mif->ctl_console_messages,
				"PROM ec0mode override set to",
				(mode & MAC_100MBIT) ? 100 : 10,
				(mode & MAC_FULL_DUPLEX) ? "full": "half");
			beenhere = 1;
		}

	/* MACE rev 0, force 100Base-TX */
	} else if (!mif->revision) {
		mode = MAC_100MBIT;			/* 100Mb-TX */
		mace_ether_mdio_wr(mif, 0, MII_R0_SPEEDSEL | MII_R0_DUPLEX);
		mif->phystatus |= PHYS_LOCK;
		if (!beenhere) {
			printf("!ec0: MACE1.0 - forcing 100Mb operation\n" +
			       mif->ctl_console_messages);
			beenhere = 1;
		}

	/* Autonegotiation enabled, default to 100mbit half duplex */
	} else {
		mace_ether_mdio_wr(mif, 0, MII_R0_AUTOEN);
		mode |= MAC_100MBIT;
	}

	/* Turn on promiscuous flag if needed for snoop */
	if (flags & IFF_PROMISC)
		mode |= MAC_PROMISCUOUS;

	/* Turn on all multicast flag if requested by mrouted */
	if (flags & IFF_ALLMULTI)
		mode |= MAC_ALL_MULTICAST;

	/* set ipg based on full or half duplex */
	if (mode & MAC_FULL_DUPLEX) {
		(&(mif->eif.eif_arpcom.ac_if))->if_flags |= IFF_LINK0;
		if (me_fullduplex_ipg[0]) {
			mode |= me_fullduplex_ipg[0] << MAC_IPGT_SHIFT;
			mode |= me_fullduplex_ipg[1] << MAC_IPGR1_SHIFT;
			mode |= me_fullduplex_ipg[2] << MAC_IPGR2_SHIFT;
		} else {
			mode |= MAC_DEFAULT_IPG;
		}
	} else {
		(&(mif->eif.eif_arpcom.ac_if))->if_flags &= ~IFF_LINK0;
		if (me_halfduplex_ipg[0]) {
			mode |= me_halfduplex_ipg[0] << MAC_IPGT_SHIFT;
			mode |= me_halfduplex_ipg[1] << MAC_IPGR1_SHIFT;
			mode |= me_halfduplex_ipg[2] << MAC_IPGR2_SHIFT;
		} else {
			mode |= MAC_DEFAULT_IPG;
		}
	}

	/* store away new mode, but don't load into hardware */
	mif->mode = mode;

	/* hardware init */
	mace_hdwrether_init(mif);

	/* warn off use of IFF_DEBUG */
	if (mif->ei_if.if_flags & IFF_DEBUG) {
		printf("ec0: WARNING: hardware debug mode enabled, do not use for normal operation!\n");
	}

	/* Discard current latched contents of PHY register 1 */
	if (mif->revision) {
		(void) mace_ether_mdio_rd(mif, 1);
	}

	/* init ok? */
	if (!error) {
		mif->phystatus |= PHYS_INIT;
		/*
		 * RSVP
		 * On O2, there is only 1 MACE ethernet.  The unit number
		 * should have been initialized from the edtinit routine.
		 */
		if (eif->eif_arpcom.ac_if.if_unit != -1) {
			if (mode & MAC_100MBIT) {
				(&(mif->eif.eif_arpcom.ac_if))->if_baudrate.ifs_value =
					1000*1000*100;
				ps_params.bandwidth = 100000000;
				mif->rsvp_intfreq = 0xf;
			} else {
				(&(mif->eif.eif_arpcom.ac_if))->if_baudrate.ifs_value =
					1000*1000*10;
				ps_params.bandwidth = 10000000;
				mif->rsvp_intfreq = 1;
			}
			ps_params.flags = 0;
			ps_params.txfree = TX_RING_SIZE;
			ps_params.txfree_func = me_txfree_len;
			ps_params.state_func = me_setstate;
			if (mif->rsvp_flags & MIF_PSINITED) {
				ps_reinit(&(eif->eif_arpcom.ac_if),
					  &ps_params);
			} else {
				ps_init(&(eif->eif_arpcom.ac_if), &ps_params);
				mif->rsvp_flags |= MIF_PSINITED;
			}
		} else {
			printf("!if_me: unit number not assigned\n" +
			       mif->ctl_console_messages);
		}
 
	} else {
		mif->ei_if.if_flags &= ~IFF_RUNNING;
		mif->phystatus |= PHYS_WASDOWN;
	}

	return 0;
}

static void
mace_ether_rx_fifo_error(register struct maceif *mif, int status)
{
	if (status & INTR_RX_MSGS_UNDERFLOW) {
		if (mif->ei_if.if_flags & IFF_DEBUG) {
			printf("!ec0: RX msg cluster list empty\n" +
			       mif->ctl_console_messages);
		}
		if (mif->tx_timer > 0) {
			pciio_pio_write32(--mif->tx_timer, &mif->mac->timer);	/* PIO WR 32 */
		}
		mif->ei_if.if_ierrors++;
		mif->rx_underflow_count++;
	}
	if (status & INTR_RX_FIFO_OVERFLOW) {
		cmn_err_tag(272,CE_ALERT, "ec0: RX error, data FIFO overflow");
		mace_hdwrether_init(mif);
	}
	mif->rx_fifo_errors++;
}

static char *
mace_ether_tx_emsg(unsigned status)
{
	if (status & TX_VEC_ABORTED_UNDERRUN)
		return "fifo underrun";
	else if (status & TX_VEC_ABORTED_TOO_LONG)
		return "giant pkt";
	else if (status & TX_VEC_DROPPED_COLLISIONS)
		return "excess collisions";
	else if (status & TX_VEC_CANCELED_DEFERRAL)
		return "excess deferrals";
	else if (status & TX_VEC_DROPPED_LATE_COLLISION)
		return "late collision";
	else
		return "???";
}

static void
mace_ether_tx_purge(register struct maceif *mif)
{
	register int i;

	/* turn off transmit ring dma */
	pciio_pio_write32(~ETHER_TX_DMA_ENABLE &
		pciio_pio_read32(&mif->mac->dma_control),
		&mif->mac->dma_control);	/* PIO RD/WR 32 */

	/* free any previously queued buffers (toss-em) */
	for (i = 0; i < TX_RING_SIZE; i++) {
		m_freem(mif->tx_mfifo[i]);
		mif->tx_mfifo[i] = NULL;
	}
	mif->tx_rptr = mif->tx_wptr = pciio_pio_read16(&mif->mac->tx_ring_rptr);	/* PIO RD 16 */
	pciio_pio_write16(mif->tx_wptr, &mif->mac->tx_ring_wptr);/*PIO WR 16 */
	mif->tx_free_space = TX_RING_SIZE;
}

static void
mace_ether_tx_error(register struct maceif *mif, int status)
{
	register unsigned vstatus = 
		pciio_pio_read64(&mif->mac->irltv.last_transmit_vector); /* PIO RD 64*/

	/* statistics */
	if ((status & INTR_TX_LINK_FAIL) && (mif->phystatus & PHYS_WASUP)) {
		cmn_err_tag(273,CE_ALERT, "ec0: no carrier: check Ethernet cable");
		mace_ether_tx_purge(mif);
	}
	if (status & INTR_TX_MEMORY_ERROR) {
		cmn_err_tag(274,CE_ALERT, "ec0: TX memory read ecc error");
		mif->phystatus &= ~PHYS_WASUP;
		mif->phystatus |= PHYS_WASDOWN;
		mace_ether_tx_purge(mif);
	}
	if (status & INTR_TX_ABORTED) {
		if (mif->ei_if.if_flags & IFF_DEBUG) {
			printf("!ec0: TX aborted, %s (0x%08X)\n" +
			       mif->ctl_console_messages,
				mace_ether_tx_emsg(vstatus), vstatus);
		}
		mace_ether_transmit_stats(mif, vstatus);
		if (mif->revision < 2) {
			mace_ether_reset(&mif->eif);
			mace_ether_init(&mif->eif, 0);
		} else {
			pciio_pio_write32(ETHER_TX_DMA_ENABLE |
				pciio_pio_read32(&mif->mac->dma_control),
				&mif->mac->dma_control); /* PIO RD/WR 32 */
		}
	}
	mif->tx_ring_errors++;
}

static void
mace_ether_reset(struct etherif *eif)
{
	register struct maceif *mif = (struct maceif *)eif->eif_private;
	register int i;

	/* put mac through global reset state */
	pciio_pio_write32(MAC_RESET, &mif->mac->mac_control);	/* PIO WR 32 */
	pciio_pio_write32(0, &mif->mac->mac_control);	/* PIO WR 32 */
	mif->phystatus = 0;

	/* turn off watchdog */
	mif->ei_if.if_timer = 0;

	/* reset both DMA channels */
	mif->rx_rptr = 0;

	/* free any receive buffers */
	for (i = 0; i < MSGCL_FIFO_SIZE; i++) {
		m_freem(mif->rx_mfifo[i]);
		mif->rx_mfifo[i] = NULL;
	}

	/* cleanup old TX buffers */
	mace_ether_tx_purge(mif);

	/* Force off CRIME interrupts */
	pciio_pio_write64(1 << (MACE_ETHERNET + 16), 
		&mif->mac->irltv.sintr_request);/*PIO WR 64 */
}

/*ARGSUSED*/
static void
mace_ether_watchdog(struct ifnet *ifp)
{
	register struct maceif *mif = &mace_ether;
	register int rval;
	static int qsflg;

	ASSERT(IFNET_ISLOCKED(ifp));
	/* Pick up status and free fifo space */
	mace_ether_transmit_complete(mif);

	/* Pick up any receive messages */
	(void) mace_ether_receive(mif, pciio_pio_read8(&mif->mac->rx_fifo_rptr));/*PIO RD 8 */

	/* Watch link status in the xcvr, broken for MACE rev 0 */
	if (mif->phystatus & PHYS_START) {
		if (((rval = pciio_pio_read32(&mif->mac->phy_dataio)) & MDIO_BUSY) == 0) {/*PIO RD 32 */
			/* Got response */
			mif->phystatus &= ~PHYS_START;

			/* Link up/down */
			if (rval & MII_R1_LINKSTAT) {
				if (!mace_ether_link_update(mif, rval)) {
					mif->phystatus &= ~PHYS_WASDOWN;
					mif->phystatus |= PHYS_WASUP;
				}
			} else {
				if (mif->phystatus & PHYS_WASUP) {
					mace_ether_tx_error(mif,
						INTR_TX_LINK_FAIL);
					mif->phystatus |= PHYS_WASDOWN;
					mif->phystatus &=
						~(PHYS_UPDATE|PHYS_WASUP);
				}

				/* Quality PHY hardware bug */
				if (mif->phytype == PHY_QS6612X) {
					/* Tickle scrambler */
					qsflg ^= 1;
					mace_ether_mdio_rmw(mif, 31, 1, qsflg);
				}
			}

			/* Jabber condition */
			if ((rval & MII_R1_JABBER) &&
			    (rval & MII_R1_LINKSTAT) &&
			    ((mif->mode & MAC_100MBIT) == 0)) {
				if ((mif->phystatus & PHYS_JABBER) == 0) {
					cmn_err_tag(275,CE_ALERT,
						"ec0: jabber detected");
				}
				mif->phystatus |= PHYS_JABBER;
			} else {
				mif->phystatus &= ~PHYS_JABBER;
			}

			/* Remote fault condition */
			if (rval & MII_R1_REMFAULT) {
				if ((mif->phystatus & PHYS_FAULT) == 0) {
					cmn_err_tag(276,CE_ALERT,
						"ec0: remote fault detected");
				}
				mif->phystatus |= PHYS_FAULT;
			} else {
				mif->phystatus &= ~PHYS_FAULT;
			}
		}
	}
	if (mif->revision > 0) {
		if ((mif->phystatus & (PHYS_INIT|PHYS_START)) == PHYS_INIT) {
			pciio_pio_write32((mif->phyaddr << 5) | 0x1,
				&mif->mac->phy_address);	/*PIO WR 32 */
			pciio_pio_write32(1, &mif->mac->phy_read_start);	/*PIO WR 32 */
			mif->phystatus |= PHYS_START;
		}
	}

	/* Ramp timer back up at 1HZ interval */
	if (mif->tx_timer < me_rxdelay) {
		pciio_pio_write32(++mif->tx_timer, &mif->mac->timer);	/* PIO WR 32 */
	}

	/* keep watchdog running */
	mif->ei_if.if_timer = IFNET_SLOWHZ;
}

static void
mace_ether_timer(struct maceif *mif)
{
	IFNET_LOCK(&mif->ei_if);
	/* Pick up status and free fifo space */
	mace_ether_transmit_complete(mif);

	/* Watch for stuck transmitter */
	if ((mif->phystatus & PHYS_WASUP) &&
	    (mif->tx_free_space != TX_RING_SIZE)) {
		if (++mif->tx_timeout > DEADMAN_TIMEOUT) {
			if (mif->ei_if.if_flags & IFF_DEBUG) {
				printf("!ec0: watchdog reset transmitter\n" +
				       mif->ctl_console_messages);
			}
			++mif->tx_watchdog_resets;
			mace_ether_reset(&mif->eif);
			mace_ether_init(&mif->eif, 0);
		}
	}

	/* Post next call */
	(void) itimeout(mace_ether_timer, mif, HZ/5, plbase);
	IFNET_UNLOCK(&mif->ei_if);
}

#define GETISR_ZERO_MASK      0x8000e000      /* should be zeros */


static __uint32_t
mace_ether_getisr(struct maceif *mif)
{
	register union me_isr esr;
#ifdef _PIO_EXTRA_SAFE
	register union me_isr esr2;
#endif
	esr.lsr = pciio_pio_read64(&mif->mac->isr.lsr);		/*PIO RD 64 */
again:
#ifdef _PIO_EXTRA_SAFE
	esr2.lsr = pciio_pio_read64(&mif->mac->isr.lsr);	/*PIO RD 64 */
	if (esr2.lsr != esr.lsr) {
		esr = esr2;
		mif->rx_getisr_value_changed++;
		wbflush();
		goto again;
	}
#endif
	if ((esr.s.__ispd != 0) || (esr.s.sr & GETISR_ZERO_MASK)) {
		if (mif->ei_if.if_flags & IFF_DEBUG) {
			printf("!ec0: getisr(): 0x%016llX\n" +
			       mif->ctl_console_messages, esr.lsr);
		}
		mif->rx_getisr_bad_value++;
		wbflush();
		goto again;
	}

	return esr.s.sr;
}

/*ARGSUSED*/
static void
mace_ether_intr(int unit)
{
	register struct maceif *mif = &mace_ether;
	union {
		__uint32_t      all;
		struct {
			__uint32_t      :2,
					rxseqnum:5,
					txrptr:9,
					rxrptr:8,
					isf:8;
		} comp;
	} status;

	IFNET_LOCK(&mif->ei_if);

	/* Initialize error limit */
	mif->rt_in_intr = 1;
	mif->rt_intr_not_valid_base = mif->rx_not_valid;

	/* Read interrupt status from dispatch register in MACE */
	while (((status.all = mace_ether_getisr(mif)) & 0xff) != 0) {

	    /*
	     * Need to reclaim tx packets first until the NFS client
	     * clntkudp_callit() routine is fixed to not be brain dead
	     * in serializing all requests into the same private buffer.
	     */
	    if (mif->tx_rptr != status.comp.txrptr) {
		mace_ether_transmit_complete(mif);
 	    }

	    /* Process received packets */
	    if (mif->rx_rptr != status.comp.rxrptr) {
		if (mace_ether_receive(mif, (int)status.comp.rxrptr))
		    continue;
	    }

	    /* Check for receive errors */
	    if (status.comp.isf & ETHER_RX_ERRORS) {
		mace_ether_rx_fifo_error(mif, status.comp.isf);
		pciio_pio_write32(ETHER_RX_ERRORS,
			&mif->mac->interrupt_status);/*PIO WR 32 */
	    }

	    /* Check for transmit errors */
	    if (status.comp.isf & ETHER_TX_ERRORS) {
		mace_ether_tx_error(mif, status.comp.isf);
		pciio_pio_write32(ETHER_TX_ERRORS,
			&mif->mac->interrupt_status);/*PIO WR 32 */
	    }

	    /* Check transmit complete status */
	    if (status.comp.isf & INTR_TX_PKT_REQ) {
		/*
		 * RSVP.  This interrupt is caused by setting the
		 * TX_CMD_SENT_INT_EN in the txcmd
		 */
		if (mif->rsvp_flags & MIF_PSENABLED)
			ps_txq_stat(&(mif->eif.eif_arpcom.ac_if),
				    mif->tx_free_space);
		pciio_pio_write32(INTR_TX_PKT_REQ, &mif->mac->interrupt_status);/*PIO WR 32 */
	    }

	    /* Check transmit fifo empty status */
	    if (status.comp.isf & INTR_TX_DMA_REQ) {
		/* should be nothing left?, turn off drain interrupt */
		pciio_pio_write32(0, &mif->mac->tx_control);	/* PIO WR 32 */
	    }

	}

	mif->rt_in_intr = 0;
	IFNET_UNLOCK(&mif->ei_if);
	return;
}

/*
 * Hardware internet checksum support
 */
static void
mace_ether_hdwrcksum(
	struct maceif *mif,
	struct mbuf *m0,
	int rlen,
	__uint32_t cksum)
{
	struct etherbufhead *ebh;
	struct ether_header *eh;
	__uint32_t x;
	struct ip *ip;
	char *crc;
	int hlen;

	/*
	 * Finish TCP or UDP checksum on non-fragments.
	 */
	ebh = mtod(m0, struct etherbufhead *);
	eh = &ebh->ebh_ether;
	ip = (struct ip *)(ebh + 1);
	hlen = ip->ip_hl << 2;
	if ((ntohs(eh->ether_type) == ETHERTYPE_IP) &&
	    ((ip->ip_off & (IP_OFFMASK|IP_MF)) == 0) &&
	    ((ip->ip_p == IPPROTO_TCP) || (ip->ip_p == IPPROTO_UDP))) {

		/*
		 * compute checksum of the pseudo-header
		 */
		cksum += (ip->ip_len - hlen) +
			 htons((ushort)ip->ip_p) +
			 (ip->ip_src.s_addr >> 16) +
			 (ip->ip_src.s_addr & 0xffff) +
			 (ip->ip_dst.s_addr >> 16) +
			 (ip->ip_dst.s_addr & 0xffff);

		/*
		 * Subtract the ether header from the checksum.
		 * The IP header will sum to logical zero if it's correct
		 * so we don't need to include it here.  This is safe since,
		 * if it's incorrect, ip input code will toss it anyway.
		 */
		x = ((u_short*)eh)[0] + ((u_short*)eh)[1] +
		    ((u_short*)eh)[2] + ((u_short*)eh)[3] +
		    ((u_short*)eh)[4] + ((u_short*)eh)[5] +
		    ((u_short*)eh)[6];
		x = (x & 0xffff) + (x >> 16);
		x = 0xffff & (x + (x >> 16));
		cksum += 0xffff ^ x;

		/*
		 * subtract CRC portion that is not part of checksum
		 */
		crc = &(((char *)ip)[rlen - (ETHER_HDRLEN + CRCLEN)]);
		if (rlen & 1) {	/* odd */
			cksum += 0xffff ^ (u_short) ((crc[1] << 8) | crc[0]);
			cksum += 0xffff ^ (u_short) ((crc[3] << 8) | crc[2]);
		} else { /* even */
			cksum += 0xffff ^ (u_short) ((crc[0] << 8) | crc[1]);
			cksum += 0xffff ^ (u_short) ((crc[2] << 8) | crc[3]);
		}

		/*
		 * fold in carries
		 */
		cksum = (cksum & 0xffff) + (cksum >> 16);
		cksum = 0xffff & (cksum + (cksum >> 16));

		/*
		 * valid iff all 1's
		 */
		if (cksum == 0xffff) {
			m0->m_flags |= M_CKSUMMED;
			mif->rx_hdwr_cksum++;
		}
	}
}

/*
 * Record statistics and send up to protocol level
 */
static void
mace_ether_input(
	struct maceif *mif,
	struct mbuf *m0,
	int length,
	unsigned stats,
	unsigned cksum)
{
	register int rlen = length, error = 0, snoopflags = 0;
	register void *rbp;

	/* save statistics info */
	if (length > (RCVBUF_SIZE / 2) || (length < ETHERMINLEN))
		stats |= RX_VEC_BAD_PACKET;
	if (stats & RX_VEC_MULTICAST)
		mif->rx_multicast++;
	if (stats & RX_VEC_BROADCAST)
		mif->rx_broadcast++;
	if (stats & RX_VEC_BAD_PACKET) {
		/* packet is bad, record error and set snoopflags */
		mif->ei_if.if_ierrors++;
		error = 1;
		snoopflags |= SN_ERROR;
		if (mif->ei_if.if_flags & IFF_DEBUG) {
			printf("!ec0: rcv error: length=%d, status=0x%x\n" +
			       mif->ctl_console_messages,
				length, stats);
		}
		if (length < ETHERMINLEN) {
			snoopflags |= SNERR_TOOSMALL;
			length = ETHERMINLEN;
		}
		if (length > ETHERMAXLEN) {
			snoopflags |= SNERR_TOOBIG;
			length = ETHERMAXLEN;
		}
		if (stats & RX_VEC_CODE_VIOLATION) {
			snoopflags |= SNERR_FRAME;
			mif->rx_code_violation++;
		}
		if (stats & RX_VEC_DRIBBLE_NIBBLE)
			mif->rx_dribble_nibble++;
		if (stats & RX_VEC_CRC_ERROR) {
			snoopflags |= SNERR_CHECKSUM;
			mif->rx_crc_error++;
		}
	}
	length -= ETHER_HDRLEN + CRCLEN;
	mif->ei_if.if_ibytes += length;
	mif->ei_if.if_ipackets++;
	length += sizeof (struct etherbufhead);
	m0->m_len = length;

	/* ifheader at the front of the received buffer */
	rbp = mtod(m0, void *);
	IF_INITHEADER(rbp, &mif->ei_if, sizeof(struct etherbufhead));

	/* test for promiscuous packets */
	if ((stats & RX_PROMISCUOUS) == 0) {
		snoopflags |= SN_PROMISC;
	}

	/* hardware internet checksum checker */
	if (!error && (rlen > ETHERMINLEN) && me_hdwrcksum_enable) {
		mace_ether_hdwrcksum(mif, m0, rlen, cksum & 0xffff);
	}

	/* call input function to deliver the packet */
	(void) ether_input(&mif->eif, snoopflags, m0);
}

/*
 * Force flush PIO write all the way out to the device
 * workaround for MCL fifo write & incoming message queue hardware bug
 * need to complete a MCL fifo write before another PIO can be queued.
 */
static __uint32_t
pioflush(void)
{
	volatile __uint32_t *ptr = (__uint32_t *)(0xBF304018);

	wbflush();
	*ptr = 0;
	*ptr = 0;
	return *ptr;
}

/*
 * Basic procedure that checks to see how many message cluster buffers the
 * hardware has used and replaces them.  Note that if we can't replace a
 * buffer we must resubmit the old one and drop the received data.
 * 
 * Note: this could be done at a lower software interrupt priority.
 */
static int
mace_ether_receive(register struct maceif *mif, int nptr)
{
	register int length, optr, rlen = mif->rx_rlen;
	register struct mbuf *m, *m0;
	register unsigned *ps, cksum, stats;

	/* RX msgs packet collection, also gather statistics */
	nptr = RXFIFOINDEX(nptr);
	optr = mif->rx_rptr;
	while (optr != nptr) {
		/* Message cluster being processed */
		m = mif->rx_mfifo[RXRINGINDEX(optr)];

		/* R10K, remap the i/o page (no flush, do that below) */
		xdki_dcache_validate(mtod(m, void *), RCVDMA_SIZE);

		/* Invalidate cache before reading new buffer */
		__dcache_inval(mtod(m, caddr_t), MSIZE);

		/* Get status & length from receive status vector */
		ps = mtod(m, __uint32_t *);
		cksum = ps[0], stats = ps[1];
		length = stats & RX_VEC_LENGTH;

		/* Not a valid packet? ...spurious interrupt... */
		if ((cksum & RX_VALID_PACKET) == 0) {
			/* Gack! R10K work around, re-unmap i/o page */
			__vm_dcache_inval(mtod(m, caddr_t), RCVDMA_SIZE);
			pciio_pio_write32(INTR_RX_DMA_REQ,&mif->mac->interrupt_status);/*PIO WR 32*/
			wbflush();
			mif->rx_not_valid++;
			if (mif->rt_in_intr &&
			    ((mif->rx_not_valid - mif->rt_intr_not_valid_base) >
			     mif->rt_intr_not_valid_limit)) {
				if (mif->ei_if.if_flags & IFF_DEBUG) {
					printf("!ec0: excessive rx_not_valid reset\n" +
					       mif->ctl_console_messages);
				}
				++mif->rt_not_valid_resets;
				mace_ether_reset(&mif->eif);
				mace_ether_init(&mif->eif, 0);
				mif->rt_intr_not_valid_base = mif->rx_not_valid;
				return(1);
			}
			break;
		}

		/* For short msgs, copy data into mbuf and reuse cluster */
		if (length <= (MLEN - DMA_OFFSET)) {
			m0 = m;
			if ((m = m_get(M_DONTWAIT, MT_DATA)) != NULL) {
				bcopy(mtod(m0, caddr_t), mtod(m, caddr_t),
					length + DMA_OFFSET);
				goto deliver;
			}

		/* For long msgs, must have replacement cluster buffer */
		} else if ((m0 = m_vget(M_DONTWAIT, RCVBUF_SIZE, MT_DATA)) != NULL) {
			/* Invalidate cache for remaining buffer length */
			__dcache_inval(mtod(m, caddr_t) + MSIZE,
				RCVDMA_SIZE - MSIZE);

deliver:
			/* Deliver old mbuf to networking stack */
			mace_ether_input(mif, m, length, stats, cksum);

		} else {
			m0 = m;

		}

		/* Clear semaphore in new buffer and flush to memory */
		*(mtod(m0, __uint64_t *)) = 0L;
		__vm_dcache_wb_inval(mtod(m0, caddr_t), sizeof (__uint64_t));

		/* Invalidate cache contents for new buffer */
		__vm_dcache_inval(mtod(m0, caddr_t), RCVDMA_SIZE);
		pciio_pio_write32(kvtophys(mtod(m0, caddr_t)),&mif->mac->rx_fifo);/* PIO WR 32 */
		(void) pioflush();				/* XXX */

		/* New message cluster is at end of queue */
		mif->rx_mfifo[RXRINGINDEX(optr + rlen)] = m0;

		/* Update pointer */
		optr = RXFIFOINDEX(optr + 1);
	}
	mif->rx_rptr = optr;

	return(0);
}

/*
 * Transmit vector statistics
 */
static void
mace_ether_transmit_stats(struct maceif *mif, unsigned stats)
{
	register int collisions = (stats & TX_VEC_COLLISIONS) >>
				   TX_VEC_COLLISION_SHIFT;

	mif->ei_if.if_obytes += stats & TX_VEC_LENGTH;
	mif->ei_if.if_collisions += collisions;
	if (stats & TX_VEC_DEFERRED) {
		mif->tx_deferred++;
	}
	if ((stats & TX_VEC_COMPLETED_SUCCESSFULLY) == 0) {
		if (stats & TX_VEC_LATE_COLLISION)
			mif->tx_late_collisions++;
		if (stats & TX_VEC_CRC_ERROR)
			mif->tx_crc_error++;
		if (stats & TX_VEC_ABORTED_TOO_LONG)
			mif->tx_aborted_too_long++;
		if (stats & TX_VEC_ABORTED_UNDERRUN)
			mif->tx_aborted_underrun++;
		if (stats & TX_VEC_DROPPED_COLLISIONS)
			mif->tx_dropped_collisions++;
		if (stats & TX_VEC_CANCELED_DEFERRAL)
			mif->tx_canceled_deferral++;
		if (stats & TX_VEC_DROPPED_LATE_COLLISION)
			mif->tx_dropped_late_collision++;
		if (stats & (TX_VEC_CRC_ERROR |
			     TX_VEC_ABORTED_TOO_LONG |
			     TX_VEC_ABORTED_UNDERRUN |
			     TX_VEC_CANCELED_DEFERRAL)) {
			mif->ei_if.if_oerrors++;
		}
	}
}

/*
 * pick up status vectors and rack up free space, can be called either from
 * interrupt time or output time.
 */
void
mace_ether_transmit_complete(register struct maceif *mif)
{
	register int tx_rptr = mif->tx_rptr, tx_wptr = mif->tx_wptr, cnt = 0;
	register volatile TXfifo *f = &mif->tx_fifo[tx_rptr];
	register long long status;

	/* Empty? */
	if (tx_rptr == tx_wptr)
		return;

	/* TX FIFO garbage collection, also gather statistics */
	while ((status = f->TXStatus) & TX_VEC_FINISHED) {
		/* Gather info and record done */
		mace_ether_transmit_stats(mif, (unsigned)status);
		m_freem(mif->tx_mfifo[tx_rptr]);
		mif->tx_mfifo[tx_rptr] = NULL;
		f->TXStatus = DEADPACKET;
		cnt++;
		mif->tx_timeout = 0;
		tx_rptr = TXFIFOINDEX(tx_rptr + 1);
		if (tx_rptr == tx_wptr)
			break;
		f = &mif->tx_fifo[tx_rptr];
	}

	/* Record new ring complete position */
	mif->tx_rptr = tx_rptr;
	(void) atomicAddInt(&mif->tx_free_space, cnt);
}

/*
 * DEBUG ONLY
 */
static void
mace_valid_txcmd(volatile TXfifo *f)
{
	register u_int ec, i, ccnt, len;
	register __uint64_t dword;

#define	MACE_ECODE(num)	{ ec = (num); goto bad; }

	/* Header */
	len = (dword = f->TXCmd) & 0xFFFF;
	if (len > 1513)
		MACE_ECODE(0);
	if (len < 15)
		MACE_ECODE(1);
	len = (dword >> 16) & 0x7F;
	if (len < 0x8)
		MACE_ECODE(2);
	if (len > 0x72)
		MACE_ECODE(3);
	if ((dword >> 28) != 0LL)
		MACE_ECODE(4);
	for (ccnt = 0, i = 25; i < 32; ++i) {
		if ((dword >> i) & 1)
			++ccnt;
	}
	if (ccnt > TX_CMD_NUM_CATS)
		MACE_ECODE(5);

	/* Concatenation buffers */
	for (i = 1; i <= ccnt; ++i) {
		dword = f->TXConcatPtr[i];
		if ((dword & 7) != 0)
			MACE_ECODE(6);
		if ((dword >> 43) != 0LL)
			MACE_ECODE(7);
		if (((dword >> 32) & 0x3FF) > 1513)
			MACE_ECODE(8);
	}

	return;

bad:
	printf("mace_valid_txcmd: bad cmd blk detected (%d)\n", ec);
	printf("\tTXCmd          = 0x%016llX\n", f->TXCmd);
	printf("\tTXConcatPtr[1] = 0x%016llX\n", f->TXConcatPtr[1]);
	printf("\tTXConcatPtr[2] = 0x%016llX\n", f->TXConcatPtr[2]);
	printf("\tTXConcatPtr[3] = 0x%016llX\n", f->TXConcatPtr[3]);
	for (i = 4; i < 16; i++) {
		printf("\tTXData[%d]      = 0x%016llX\n", i, f->TXData[i]);
	}
}

/*
 * Try to build a concatenation list for the supplied mbuf chain.  We
 * don't try to optimize the failure case.
 */
static int
mace_tx_catlist(
	struct maceif *mif,
	volatile TXfifo *f, 
	struct mbuf *m,
	int *plen)
{
	register int rev = mif->revision, ccnt = 1, remain, hlen, flen, len;
	register paddr_t vaddr;

#define IO_PGSIZE	4096
#define IO_PGOFFSET	(IO_PGSIZE - 1)
#define IO_PGMASK	~(IO_PGSIZE - 1)

	/* Do nothing if scatter/gather disabled */
	if (!me_hdwrgather_enable)
		return -1;

	/* Count protocol header bytes, usually one mbuf */
	for (hlen = 0; m != NULL; m = m->m_next) {
		if (m->m_type != MT_HEADER) {
			break;
		}			
		if ((hlen + m->m_len) > 96) {
			break;
		}
		hlen += m->m_len;
	}

	/* Process all remaining mbufs */
	for (ccnt = 1; m != NULL; m = m->m_next) {
		/* Get data length, skip empty buffers */
		if ((len = m->m_len) == 0)
			continue;

		/* Valid aligned starting address */
		if ((vaddr = mtod(m, paddr_t)) & 7) {
			/* Pullup bytes in first buffer to align? */
			if (ccnt == 1) {
				flen = 8 - (int)(vaddr & 7);
				if (flen >= len) {
					/* short buf, pullup */
					flen = len;
					hlen += flen;
					continue;
				} else {
					/* clip bytes off front */
					hlen += flen;
					vaddr += flen;
					len -= flen;
				}
			} else {
				++mif->tcase[4];
				return -1;
			}
		}

		/* Force dword length (for MACE ethernet rev 0 only) */
		if (!rev && m->m_next && ((len & 7) != 0)) {
			++mif->tcase[5];
			return -1;
		}

		/* Writeback cache contents over virtual length */
		dki_dcache_wb((void *)vaddr, len);

		/* Create gather elements (check for page crossing) */
		if ((vaddr & IO_PGMASK) != ((vaddr + len - 1) & IO_PGMASK)) {
			remain = IO_PGSIZE - (int)(vaddr & IO_PGOFFSET);

			/* Set concatenation pointer address & length */
			f->TXConcatPtr[ccnt++] =
				((long long)(remain - 1) << 32) |
				kvtophys((void *)vaddr);

			/* Whats left on the following virtual page? */
			vaddr += remain;
			len -= remain;
		}

		/* Set concatenation pointer address & length */
		f->TXConcatPtr[ccnt] = ((long long)(len - 1) << 32) |
			kvtophys((void *)vaddr);

		/* Advance and fail if at concatenation list limit */
		if (ccnt++ > TX_CMD_NUM_CATS) {
			++mif->tcase[6];
			return -1;
		}
	}

	/* tell caller how much we want to copy */
	*plen = hlen;
	hlen += ETHER_HDRLEN;

	/* will the copied data and concatenation list overlap? */
	if (hlen > (sizeof(TXfifo) - ccnt * sizeof(long long))) {
		++mif->tcase[7];
		return -1;
	}

	return ccnt - 1;
}

/*
 * High level ethernet output entry point, called by upper level protocol
 * stack to place a new message buffer chain into the output queue.
 *
 * Each 128byte MACE tx descriptor describes a single packet that contains
 * up to 120 bytes of data locally, plus up to three pointers to noncontiguous
 * data to be concatenated onto the end of the packet by the hardware.
 *
 * The MACE tx pointers have the following restrictions:
 * - addresses must start on dword aligned boundaries
 * - iff MACE1.0 is used, block lengths must be dword multiples
 */
static int
mace_ether_output(
	struct etherif *eif,		/* on this interface */
	struct etheraddr *edst,		/* with these addresses */
	struct etheraddr *esrc,
	u_short type,			/* of this type */
	struct mbuf *m0)		/* send this chain */
{
	register struct maceif *mif = (struct maceif *)eif->eif_private;
	register volatile TXfifo *f = &mif->tx_fifo[mif->tx_wptr];
	register int m0save = 0, ccnt, tlen, len, nwptr, txcmd = 0;
	register struct mbuf *m;
        struct ether_header ehdr;
	struct mehdr {
		struct etheraddr dst, src;
		char htype, ltype;
	} *eh;
	int plen;
	static int cmdmap[] = {
		0, 
		TX_CMD_CONCAT_1,
		TX_CMD_CONCAT_1 | TX_CMD_CONCAT_2,
		TX_CMD_CONCAT_1 | TX_CMD_CONCAT_2 | TX_CMD_CONCAT_3,
	};

	/*
	 * Link is down, don't queue new packets
	 */
	if (mif->phystatus & PHYS_WASDOWN) {
		mif->ei_if.if_odrops++;
		m_freem(m0);
		return EIO;
	}

	/*
	 * Garbage collection in transmit ring
	 */
	mace_ether_transmit_complete(mif);

	/*
	 * Space in TX ring?  (note: don't use last entry, fix this)
	 */
	if (mif->tx_free_space <= 1) {
		IF_DROP(&mif->ei_if.if_snd);
		m_freem(m0);
		return ENOBUFS;
	}

	/*
	 * Calculate length of packet
	 */
	if ((len = m_length(m0)) > ETHERMTU) {
		m_freem(m0);
		return E2BIG;
	}
	tlen = len + ETHER_HDRLEN;

	++mif->tcase[0];

	/*
	 * Init TX cmd header, interrupt every 1/4 ring
	 */
	if (mif->ei_if.if_flags & IFF_DEBUG)
		txcmd = TX_CMD_TERM_DMA;
	nwptr = TXFIFOINDEX(mif->tx_wptr + 1);
	if ((nwptr & ((TX_RING_SIZE / 4) - 1)) == 0) {
		txcmd |= TX_CMD_SENT_INT_EN;
	} else if (mif->rsvp_flags & MIF_PSENABLED) {
		/* RSVP.
		 * If packet scheduling is on, interrupt more frequently
		 */
		if ((nwptr & mif->rsvp_intfreq) == 0)
			txcmd |= TX_CMD_SENT_INT_EN;
	}

	/*
	 * CASE #1
	 * If it's a short packet, just put directly in the TX ring.
	 */
	if (tlen < (sizeof(TXfifo) - sizeof(long long))) {
		/* Set up the fifo block cmd header */
		f->TXCmd = txcmd | 
				((sizeof(TXfifo) - tlen) <<
				 TX_CMD_OFFSET_SHIFT) | (tlen - 1);

		/* Ethernet header insertion point */
		eh = (struct mehdr *)&f->buf[sizeof(TXfifo) - tlen];

		/* Copy the packet into the TX ring */
		m_datacopy(m0, 0, len,
			(void *)&f->buf[sizeof(TXfifo) - len]);
		mif->tx_mfifo[mif->tx_wptr] = NULL;

		++mif->tcase[1];

	/*
	 * CASE #2
	 * Pull up all the protocol headers, then check if we can DMA
	 * directly out of the remaining mbufs using the concatenation
	 * buffers.  If not, copy the rest of the data into a cluster
	 * and dma directly out of that instead.  Turns out that this
	 * works out ok 99.9% of the time (very few bcopys).
	 */
	} else if ((ccnt = mace_tx_catlist(mif, f, m0, &plen)) > 0) {
		/* Data insertion point */
		eh = (struct mehdr *)&f->buf[sizeof(TXfifo) - plen];

		/* Pullup plen data bytes of mbuf chain? */
		m_datacopy(m0, 0, plen, (void *)eh);

		/* Backup for ethernet header insertion point */
		plen += ETHER_HDRLEN;
		eh--;

		/* Set up the fifo block cmd header */
		f->TXCmd = txcmd | cmdmap[ccnt] |
			((sizeof(TXfifo) - plen) << TX_CMD_OFFSET_SHIFT) |
			 (tlen - 1);

		/* Need to save packet until dma is complete */
		mif->tx_mfifo[mif->tx_wptr] = m0;
		m0save = 1;

		++mif->tcase[2];
	/*
	 * CASE #3
	 * Just copy the packet into a single cluster and DMA out of it.
	 */
	} else {
		/* Need a message buffer cluster to hold the entire packet */
		if ((m = m_vget(M_DONTWAIT, len, MT_DATA)) == NULL) {
			mif->ei_if.if_odrops++;
			m_freem(m0);
			return ENOBUFS;
		}
		mif->tx_mfifo[mif->tx_wptr] = m;

		/* Set up the fifo block cmd header */
		f->TXCmd = txcmd | TX_CMD_CONCAT_1 |
				((sizeof(TXfifo) - ETHER_HDRLEN)
					<< TX_CMD_OFFSET_SHIFT) |
				(tlen - 1);

		/* Ethernet header insertion point */
		eh = (struct mehdr *)&f->buf[sizeof(TXfifo) - ETHER_HDRLEN];

		/* Copy the packet into a contiguous buffer */
		m_datacopy(m0, 0, m->m_len, mtod(m, caddr_t));

		/* Writeback cache contents over needed length */
		dki_dcache_wb(mtod(m, void *), m->m_len);

		/* Set concatenation pointer #1 physical address & length */
		f->TXConcatPtr[1] = ((long long)(m->m_len - 1) << 32) |
			kvtophys(mtod(m, caddr_t));

		++mif->tcase[3];
	}

	/* Create ether header at front of the packet (unaligned) */
	eh->dst = *edst;
	eh->src = *esrc;
	eh->htype = type >> 8;
	eh->ltype = type;

	/* Check whether snoopers want to copy this packet */
	if (RAWIF_SNOOPING(&mif->ei_rawif)) {
		/* Can't filter here, let snoop_input() handle it */
		bcopy((void *)eh, (void *)&ehdr, sizeof ehdr);
		ether_selfsnoop(&mif->eif, &ehdr, m0, 0, len);
	}

	/* Free the original mbuf list, no longer needed */
	if (!m0save) {
		m_freem(m0);
	}

	/* Check if transmit command is valid (DEBUG ONLY) */
	if (mif->ei_if.if_flags & IFF_DEBUG) {
		mace_valid_txcmd(f);
	}

	/* Place the packet into the TX hardware output queue */
	(void) atomicAddInt(&mif->tx_free_space, -1);
	wbflush();
	mif->tx_wptr = nwptr;
	pciio_pio_write16(nwptr, &mif->mac->tx_ring_wptr); /* PIO WR 16 */

	return 0;
}

/*
 * DEC Poly routine
 */
static unsigned
CrcGen(unsigned char *Bytes, int BytesLength)
{
	unsigned Crc = 0xFFFFFFFF;
	unsigned const Poly = 0x04c11db7;
	unsigned Msb;
	unsigned char CurrentByte;
	int Bit;

	while (BytesLength-- > 0) {
		CurrentByte = *Bytes++;
		for (Bit = 0; Bit < 8; Bit++) {
			Msb = Crc >> 31;
			Crc <<= 1;
			if (Msb ^ (CurrentByte & 1)) {
				Crc ^= Poly;
				Crc |= 1;
			}
			CurrentByte >>= 1;
		}
	}

	return Crc;
}

/*
 * Given a multicast ethernet address, this routine calculates the
 * address's bit index in the logical address filter mask
 */
static int
mace_laf_hash(u_char *addr, int len)
{
	return CrcGen(addr, len) >> 26;
}

static int
mace_ether_ioctl(
	struct etherif *eif,
	int cmd,
	void *data)
{
	register struct maceif *mif = (struct maceif *)eif->eif_private;
	struct mfreq *mfr;
	union mkey *key;

	mfr = (struct mfreq *)data;
	key = mfr->mfr_key;

	switch (cmd) {
	    /* Enable one of the multicast filter flags */
	    case SIOCADDMULTI:
		mfr->mfr_value =
			mace_laf_hash(key->mk_dhost, sizeof (key->mk_dhost));
		if (LAF_TSTBIT(mif->mlaf, mfr->mfr_value)) {
			mif->lafcoll++;
			break;
		}
		LAF_SETBIT(mif->mlaf, mfr->mfr_value);
		pciio_pio_write64(mif->mlaf, &mif->mac->mlaf);	/*PIO WR 64 */
		if (mif->nmulti == 0)
			eiftoifp(eif)->if_flags |= IFF_FILTMULTI;
		mif->nmulti++;
		break;

	    /* Disable one of the multicast filter flags */
	    case SIOCDELMULTI:
		if (mfr->mfr_value !=
			mace_laf_hash(key->mk_dhost, sizeof(key->mk_dhost)))
			return EINVAL;
		if (mfhasvalue(&eif->eif_mfilter, mfr->mfr_value)) {
			/* Forget about this collision. */
			--mif->lafcoll;
			break;
		}

		/*
		 * If this multicast address is the last one to map
		 * the bit numbered by mfr->mfr_value in the filter,
		 * clear that bit and update the chip.
		 */
		LAF_CLRBIT(mif->mlaf, mfr->mfr_value);
		pciio_pio_write64(mif->mlaf, &mif->mac->mlaf);	/* PIO WR 64 */
		--mif->nmulti;
		if (mif->nmulti == 0)
			eiftoifp(eif)->if_flags &= ~IFF_FILTMULTI;
		break;

	    default:
		return EINVAL;
	}

	return (0);
}

static char *
mace_hexconv(char *cp, int len, int spaces)
{
	register int idx;
	register int qqq;
	int maxi = 32;
	static const char digits[] = "0123456789abcdef";
	static char qstr[100];

	if (len < maxi)
		maxi = len;
	for (idx = 0, qqq = 0; qqq<maxi; qqq++) {
		if (((qqq%16) == 0) && (qqq != 0))
			qstr[idx++] = '\n';
		qstr[idx++] = digits[cp[qqq] >> 4];
		qstr[idx++] = digits[cp[qqq] & 0xf];
		if (spaces)
			qstr[idx++] = ' ';
	}
	qstr[idx] = 0;

	return qstr;
}

static void
mace_hexdump(char *msg, char *cp, int len)
{
	qprintf("%s: %s\n", msg, mace_hexconv(cp, len, 1));
}

static void
mace_dumpif(struct ifnet *ifp)
{
	qprintf("if_name \"%s\" if_unit %d if_mtu %d if_flags 0x%x if_timer %d\n",
		ifp->if_name, ifp->if_unit, ifp->if_mtu, ifp->if_flags,
		ifp->if_timer);
	qprintf("ifq_len %d ifq_maxlen %d ifq_drops %d\n",
		ifp->if_snd.ifq_len, ifp->if_snd.ifq_maxlen,
		ifp->if_snd.ifq_drops);
	qprintf("if_ipackets %d if_ierrors %d if_opackets %d if_oerrors %d\n",
		ifp->if_ipackets, ifp->if_ierrors,
		ifp->if_opackets, ifp->if_oerrors);
	qprintf("if_collisions %d if_ibytes %d if_obytes %d if_odrops %d\n",
		ifp->if_collisions, ifp->if_ibytes,
		ifp->if_obytes, ifp->if_odrops);
}

static char *
mace_phystr(int phytype)
{
	register char *pname = "";

	switch(phytype) {
	    case PHY_QS6612X:
		pname = "QS6612";
		break;
	    case PHY_ICS1889:
		pname = "ICS1889";
		break;
	    case PHY_ICS1890:
		pname = "ICS1890";
		break;
	    case PHY_DP83840:
		pname = "DP83840";
		break;
	}

	return pname;
}

static void
mace_dumpei(struct maceif *mif)
{
	qprintf("ac_enaddr %s\n", ether_sprintf(mif->ei_ac.ac_enaddr));
	qprintf("eif_rawif 0x%x\n", &mif->ei_rawif);
	qprintf("nmulti %d lafcoll %d ", mif->nmulti, mif->lafcoll);
	mace_hexdump("laf", (char *)&mif->mlaf, sizeof (long long));
	qprintf("phyaddr %d phyrev %d phystatus %x phytype %x %s\n",
		mif->phyaddr, mif->phyrev, mif->phystatus,
		mif->phytype, mace_phystr(mif->phytype));
	qprintf("tx_fifo 0x%08X mode 0x%08X\n", mif->tx_fifo, mif->mode);
	qprintf("tx_ring_errors %d tx_watchdog_resets %d rx_fifo_errors %d\n",
		mif->tx_ring_errors, mif->tx_watchdog_resets,
		mif->rx_fifo_errors);
	qprintf("rx_underflow_count %d rx_getisr_value_changed %d rx_getisr_bad_value %d\n",
		mif->rx_underflow_count, mif->rx_getisr_value_changed, 
		mif->rx_getisr_bad_value);
	qprintf("tx_rptr %d tx_wptr %d tx_free_space %d\n",
		mif->tx_rptr, mif->tx_wptr, mif->tx_free_space);
	qprintf("tx_late_collisions %d tx_crc_error %d tx_deferred %d\n",
		mif->tx_late_collisions, mif->tx_crc_error, mif->tx_deferred);
	qprintf("tx_aborted_too_long %d tx_aborted_underrun %d\n",
		mif->tx_aborted_too_long, mif->tx_aborted_underrun);
	qprintf("tx_dropped_collisions %d tx_canceled_deferral %d\n",
		mif->tx_dropped_collisions, mif->tx_canceled_deferral);
	qprintf("tx_dropped_late_collision %d\n",
		mif->tx_dropped_late_collision);
	qprintf("txcases: %d %d %d %d / %d %d %d %d\n",
		mif->tcase[0], mif->tcase[1], mif->tcase[2], mif->tcase[3],
		mif->tcase[4], mif->tcase[5], mif->tcase[6], mif->tcase[7]);
	qprintf("rx_rptr %d rx_rlen %d rx_boffset %d rx_not_valid %d\n",
		mif->rx_rptr, mif->rx_rlen, mif->rx_boffset,
		mif->rx_not_valid);
	qprintf("rx_code_violation %d rx_dribble_nibble %d rx_crc_error %d\n",
		mif->rx_code_violation, mif->rx_dribble_nibble,
		mif->rx_crc_error);
	qprintf("rx_hdwr_cksum %d rx_multicast %d rx_broadcast %d\n",
		mif->rx_hdwr_cksum, mif->rx_multicast, mif->rx_broadcast);
	qprintf("rt_not_valid_resets %d\n",
		mif->rt_not_valid_resets);
}

static void
mace_dumpregs(struct maceif *mif)
{
	register long long *regs = (long long *)mif->mac;
	register int i;

	qprintf("intf regs: base=0x%08X\n", regs);
	for (i = 0; i < 32; i += 4, regs += 4) {
		qprintf("0x%s%x: ", (i < 16) ? "0" : "", i);
		qprintf("%s ", mace_hexconv((char *)&regs[0], 8, 0));
		qprintf("%s ", mace_hexconv((char *)&regs[1], 8, 0));
		qprintf("%s ", mace_hexconv((char *)&regs[2], 8, 0));
		qprintf("%s\n", mace_hexconv((char *)&regs[3], 8, 0));
	}
}

static void
mace_dumpphy(struct maceif *mif)
{
	register int i, nl = 0;
	short pval;

	mif->phystatus &= ~PHYS_START;
	qprintf("phy regs:\n");
	for (i = 0; i < 32; i++) {
		if (nl == 0) {
			qprintf("0x%s%x: ", (i < 16) ? "0" : "", i);
		}
		pval = mace_ether_mdio_rd(mif, i);
		qprintf("0x%s ", mace_hexconv((char *)&pval, 2, 0));
		if (++nl >= 8) {
			qprintf("\n");
			nl = 0;
		}
	}
	if (nl != 0)
		qprintf("\n");
}

/*ARGSUSED*/
static void
mace_ether_dump(int unit)
{
	struct maceif *mif = &mace_ether;

	qprintf("$Revision: 1.28 $\n");
	mace_dumpif(&mif->ei_if);
	qprintf("\n");
	mace_dumpei(mif);
	qprintf("\n");
	mace_dumpregs(mif);
	qprintf("\n");
	mace_dumpphy(mif);
}



