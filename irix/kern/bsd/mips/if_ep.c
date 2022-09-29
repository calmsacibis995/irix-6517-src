/* mezzanine 8-port Ethernet "E-Plex"
 *
 * Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.45 $"


#ifdef EVEREST				/* compile only for EVEREST */

#include <tcp-param.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/reg.h>
#include <sys/immu.h>
#include <sys/sbd.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/invent.h>
#include <sys/capability.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#define OLD_STYLE
#include <sys/EVEREST/dang.h>

#include <ether.h>
#include <sys/if_ep.h>

#include <net/soioctl.h>
#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/udp_var.h>
#include <netinet/tcpip.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <net/rsvp.h>


int if_epdevflag = D_MP;

extern int ep_cksum;
extern int ep_low_latency;
extern int ep_reset_limit;

int ep_force_reset;

/* -DEP_DEBUG to generate a lot of debugging code */
/* #define EP_DEBUG */

#ifdef EP_DEBUG
#define SHOWCONFIG 1
#else
#define SHOWCONFIG showconfig
#endif


static char *ep_name = "ep";

/* check the validity of ep and epb pointers
 */
#define CK_EP(ep,epb) {					\
	ASSERT((ep)->ep_if.if_name == ep_name);		\
	ASSERT((ep) == &(epb)->epb_ep[(ep)->ep_port]);	\
}

/* given an ifp, get and check the corresponding ep and epb pointers
 */
#define GET_CK_EIF(eif,ep,epb) {			\
	(ep) = (struct ep_info*)eif->eif_private;	\
	(epb) = (ep)->ep_epb;				\
	CK_EP(ep,epb);					\
	CK_EP_LOCK(ep);					\
}


#define CK_EP_LOCK(ep) ASSERT(IFNET_ISLOCKED(&(ep)->ep_if))
#define CK_EPB_LOCK(epb) ASSERT(IFNET_ISLOCKED(&(epb)->epb_ep[0].ep_if))

/* Lock the first port when doing anything to the board as a whole */
#define LOCK_BD(ep,epb) {				\
	CK_EP(ep,epb);					\
	if ((ep)->ep_port != 0) {			\
		IFNET_UNLOCK(&(ep)->ep_if);		\
		IFNET_LOCK(&(epb)->epb_ep[0].ep_if);	\
	} else {					\
		CK_EP_LOCK(ep);				\
	}						\
}

#define UNLOCK_BD(ep,epb) {				\
	CK_EP(ep,epb);					\
	if ((ep)->ep_port != 0) {			\
		IFNET_UNLOCK(&(epb)->epb_ep[0].ep_if);	\
		IFNET_LOCK(&(ep)->ep_if);		\
	} else {					\
		CK_EP_LOCK(ep);				\
	}						\
}

/* (un)lock a port when the board is already locked */
#define LOCK_PORT(ep) IFNET_LOCK(&(ep)->ep_if)
#define UNLOCK_PORT(ep) IFNET_UNLOCK(&(ep)->ep_if)



/* limits on mbufs given the board
 */
#ifdef EP_DEBUG
int epb_num_big = EPB_MAX_BIG;
int epb_num_sml = EPB_MAX_SML;
#else
#define epb_num_big EPB_MAX_BIG
#define epb_num_sml EPB_MAX_SML
#endif


/* DMA parameters
 */
#define MAX_PKT_DELAY (USEC_PER_SEC/500)    /* try to limit delay to this */

int epb_h2b_dma_delay = MAX_PKT_DELAY;	/* DMA control info this often. */
int epb_int_delay = MAX_PKT_DELAY;	/* delay board-to-host interrupts */
int epb_h2b_sleep = USEC_PER_SEC/10;	/* keep H2B DMA running this long */

#if _MIPS_SIM == _ABI64
#define ADDR_HI(a) ((a) >> 32)
#else
#define ADDR_HI(a) 0
#endif


#define EP_UNIT(ep) ((ep)->ep_if.if_unit)
#define EPB_UNIT(epb) ((epb)->epb_ep[0].ep_if.if_unit)

/* label printf for one port */
#define PF_L	"ep%d slot %d adapter %d: "
#define PF_P(ep) EP_UNIT(ep),(ep)->ep_epb->epb_slot,(ep)->ep_epb->epb_adapter

/* label printf for entire board */
#define PF_L2	"ep%d-ep%d slot %d adapter %d: "
#define PF_P2(epb) EPB_UNIT(epb), EPB_UNIT(epb)+EP_PORTS-1, (epb)->epb_slot, (epb)->epb_adapter


#define EPB_RESET_DELAY(epb) ((void)EV_GET_REG((epb)->epb_swin \
					       + DANG_INTR_ENABLE))

#define ZAPBD(epb) {					\
	EV_SET_REG((epb)->epb_swin+DANG_GIORESET, 0);	\
	(epb)->epb_state |= EPB_ST_ZAPPED;		\
	EPB_RESET_DELAY(epb);				\
	EV_SET_REG((epb)->epb_swin+DANG_GIORESET, 1);	\
}

#define BAD_BD(epb,ex,bail) if (!(ex)) {		\
	printf(PF_L2 "failure \"ex\"\n", PF_P2(epb));	\
	ZAPBD(epb);					\
	bail;						\
}

/* delays required by board firmware
 */
#define DELAY_EEPROM	(160*1000)	/* time to copy EEPROM */
#define DELAY_NIBBLE	100		/* usec delay for each nibble */
#define START_CHECK	DELAY_NIBBLE
#define OP_CHECK	15		/* check command done this often */

#define DELAY_SIGNAL	(DELAY_EEPROM/100)  /* delay for ioctl signalling */

#define DELAY_RESET_DBG (5*1000*1000)	/* 5 sec for emulator */
#ifdef EP_DEBUG
#define DELAY_RESET	DELAY_RESET_DBG
#else
#define DELAY_RESET	DELAY_EEPROM	/* delay after reset */
#endif
#define DELAY_OP	1500		/* most operations */


#define EPB_TIMER (1/IFNET_SLOWHZ)	/* get counters every second */


#define	OUT_ALIGNED(p) (0 == ((__psunsigned_t)(p) % sizeof(__uint32_t)))


/* host counters for each board
 */
struct epb_dbg {
	uint	out_copy;		/* had to copy mbufs on output */
	uint	out_alloc;		/* had to alloc mbuf for header */
	uint	pg_cross;		/* buffer crossed page */

	uint	ints;			/* total interrupts */

	uint	board_dead;		/* gave up waiting for the board */
	uint	board_waits;		/* waited for the board */
	uint	board_poke;		/* had to wake up the board */

	uint	in_cksums;		/* board offered us a checksum */
	uint	in_cksum_frag;		/* packet was an IP fragment */
	uint	in_cksum_bad;		/* bad checksum */
	uint	in_cksum_proto;		/* wrong protocol */

	uint	out_cksum_udp;		/* output UDP checksums by board */
	uint	out_cksum_tcp;		/* output TCP checksums by board */
	uint	out_cksum_encap;	/* encapsulate checksum by board */

	uint	in_active;		/* input recursion prevented */

	uint	no_sml;			/* ran out of this size mbuf */
	uint	no_big;
};


/* all there is to know about a single unit or port of a single board */
struct ep_info {
	struct etherif	ep_eif;

	struct epb_info *ep_epb;	/* outer structure */
	int		ep_port;	/* unit number relative to board */
	u_long		ep_state;	/* useful state bits */
	u_char		ep_ps_enabled;	/* 1=RSVP scheduling on */

	time_t		ep_link_lbolt;	/* recent error message */
	time_t		ep_carrier_lbolt;

	u_char		ep_hash[MMODE_HLEN];	/* DA filter */
	uint		ep_nmulti;	/* # active multicast addresses */
	mval_t		ep_hash_bcast;	/* hash value of broadcast address */

	int		ep_prev_opackets;
	int		ep_prev_ipackets;

	/* This is locked with the interface, not the output queue. */
	int		ep_job_cnt;
	struct ep_job {
	    struct epb_h2b  job_dma[EP_MAX_DMA_ELEM+1];
#	 define MAX_JOBS 201
	} *ep_job_head, *ep_job_tail, ep_jobs[MAX_JOBS];
};
#define	ep_ac		ep_eif.eif_arpcom
#define	ep_if		ep_ac.ac_if
#define	ep_rawif	ep_eif.eif_rawif

/* bits in ep_state */
#define EP_ST_BADMAC	    0x0001	/* bad MAC address in EEPROM */
#define EP_ST_OUT_DONE	    0x0002	/* board said it finished output */
#define EP_ST_OUT_WAITING   0x0004	/* output queue was non-empty */
#define EP_ST_OUT_ILL	    0x0008	/* output seems ill */
#define EP_ST_IN_ILL	    0x0010	/* input seems ill */


/* all there is to know about a board */
struct epb_info {
	struct ep_info	epb_ep[EP_PORTS];

	struct epb_stats epb_prev_stats;
	struct epb_stats epb_new_stats;

	uint		epb_adapter;
	uint		epb_slot;
	__psunsigned_t	epb_swin;
	volatile __uint32_t *epb_gio;	/* look for the board here */

	__int32_t	epb_cmd_id;	/* ID of last command given board */
	int		epb_cmd_line;

	__int32_t	epb_vers;

	u_long		epb_state;	/* useful state bits */

	short		epb_in_active;	/* input recursion preventer */
	short		epb_rs_timer;	/* reset limiter */
	short		epb_peek_cnt;	/* limit input peeking */
	time_t		epb_peek_lbolt;

	struct epb_dbg	epb_dbg;	/* debugging counters */

	struct mbuf	*epb_in_sml_m[EPB_MAX_SML];  /* input buffer pool */
	union epb_in	*epb_in_sml_d[EPB_MAX_SML];
	int		epb_in_sml_num;
	int		epb_in_sml_h;
	int		epb_in_sml_t;
	struct mbuf	*epb_in_big_m[EPB_MAX_BIG];
	union epb_in	*epb_in_big_d[EPB_MAX_BIG];
	int		epb_in_big_num;
	int		epb_in_big_h;
	int		epb_in_big_t;

	/* This should be less than about 30(mod 128) to keep most of it
	 * in one cache line. */
	struct epb_hc	epb_hc;

	volatile struct epb_b2h epb_b2h[EPB_B2H_LEN];
	volatile struct epb_b2h *epb_b2hp;
	volatile struct epb_b2h *epb_b2h_lim;
	u_char		epb_b2h_sernum;

	/* These are volatile not because the board writes them,
	 * but to ensure the compiler stores them in the right order.
	 * Align
	 */
#if _MIPS_SIM == _ABI64
	long		pad;
#else
	long long	pad;
#endif
	volatile struct epb_h2b	epb_h2b[EPB_H2B_LEN];
	volatile struct epb_h2b	*epb_h2bp;
	volatile struct epb_h2b	*epb_h2b_lim;
} *epb_infop[EP_MAXBD];

/* bits in epb_state */
#define EPB_ST_BAD_DANG	0x0001		/* rev.B DANG chip */
#define EPB_ST_DEBUG	0x0002		/* at least 1 interface debugging */
#define EPB_ST_UP	0x0004		/* at least 1 interface up */
#define EPB_ST_WHINE	0x0008		/* have whined about sick board */
#define EPB_ST_ZAPPED	0x0010		/* turned off */
#define EPB_ST_NO_RESET	0x0020		/* board failed to reset */
#define EPB_ST_NEED_RS	0x0040		/* board needs to be reset */
#define EPB_ST_SLEEP	0x0080		/* H2B DMA is asleep */
#define EPB_ST_NOPOKE	0x0100		/* skipped a board wakeup */
#define EPB_ST_GOTMAC	0x0200		/* MAC address known */
#define EPB_ST_STATS	0x0400		/* epb_new_stats are waiting */

#define EPB_RES	epb->epb_hc.res
#define EPB_ARG	epb->epb_hc.arg


static void epb_b2h(struct epb_info*, u_long ps_ok);
static int epb_fillin(struct epb_info*);
static void ep_snd_out(struct ep_info *, struct epb_info *);


#define epb_op(epb,c) ((epb)->epb_hc.cmd = (c),				\
		       (epb)->epb_cmd_line = __LINE__,			\
		       (epb)->epb_hc.cmd_id = ++(epb)->epb_cmd_id,	\
		       *(epb)->epb_gio = EPB_GIO_H2B_INTR)


#define epb_wait(epb) epb_wait_usec(epb,DELAY_OP,__LINE__)

#ifdef EP_DEBUG
int epb_complain_reset = 0;
#endif


static void
epb_wait_usec(struct epb_info *epb,
	     int wait,			/* delay in usec */
	     int lineno)
{
	__int32_t cmd_ack;

	for (;;) {
		if ((cmd_ack = epb->epb_hc.cmd_ack) == epb->epb_cmd_id) {
			epb->epb_state &= ~EPB_ST_WHINE;
			break;
		}
		if (wait < 0) {
			METER(epb->epb_dbg.board_dead++);
			if (!(epb->epb_state & EPB_ST_WHINE)) {
				printf(PF_L2 "board asleep at %d with 0x%x"
				       " not 0x%x after 0x%x at %d\n",
				       PF_P2(epb), lineno,
				       (u_long)cmd_ack,
				       (u_long)epb->epb_cmd_id,
				       (u_long)epb->epb_hc.cmd,
				       (u_long)epb->epb_cmd_line);
				epb->epb_state |= (EPB_ST_WHINE
						   | EPB_ST_NEED_RS);
				epb->epb_state &= ~EPB_ST_STATS;
#ifdef EP_DEBUG
				if (epb_complain_reset)
					ZAPBD(epb);
#endif
			}
			epb->epb_hc.cmd = ECMD_NOP;
			break;
		}
		METER(epb->epb_dbg.board_waits++);
		wait -= OP_CHECK;
		DELAY(OP_CHECK);
	}
}


/* get the MAC address of one port
 */
static void
ep_mac_addr(struct ep_info *ep,
	    struct epb_info *epb,
	    struct etheraddr *eaddr)
{
	ep->ep_state &= ~EP_ST_BADMAC;

	epb_wait(epb);
	if (epb->epb_hc.cmd_ack != epb->epb_cmd_id)
		ep->ep_state |= EP_ST_BADMAC;
	bzero((char*)&EPB_RES, sizeof(EPB_RES.dwn_l));
	EPB_ARG.dwn.addr = (EP_MACS_ADDR + ep->ep_port*EP_MAC_SIZE);
	EPB_ARG.dwn.cnt = (EP_MAC_SIZE+3)/4;
	epb_op(epb, ECMD_FET);
	epb_wait(epb);
	if (epb->epb_hc.cmd_ack != epb->epb_cmd_id) {
		printf(PF_L "failed to get MAC address\n", PF_P(ep));
		ep->ep_state |= EP_ST_BADMAC;
	} else {
		__uint32_t cksum;

		EP_MAC_SUM(cksum,EPB_RES.dwn_l);
		if (cksum != 0) {
			printf(PF_L "bad MAC address checksum\n", PF_P(ep));
			ep->ep_state |= EP_ST_BADMAC;
		}

		if (EPB_RES.dwn_c[0] != SGI_MAC_SA0
		    || EPB_RES.dwn_c[1] != SGI_MAC_SA1
		    || EPB_RES.dwn_c[2] != SGI_MAC_SA2) {
			printf(PF_L "bad MAC address %x:%x:%x:%x:%x:%x\n",
			       PF_P(ep),
			       EPB_RES.dwn_c[0],
			       EPB_RES.dwn_c[1],
			       EPB_RES.dwn_c[2],
			       EPB_RES.dwn_c[3],
			       EPB_RES.dwn_c[4],
			       EPB_RES.dwn_c[5]);
		}
	}

	bcopy((void*)EPB_RES.dwn_c, eaddr, sizeof(*eaddr));
}


/* Set the MAC multicast mode, address filters, and so on
 *	The board must be locked.
 */
static void
ep_set_mmode(struct ep_info *ep,
	     struct epb_info *epb)
{
	int i;

	CK_EPB_LOCK(epb);

	epb_wait(epb);
	if (epb->epb_hc.cmd_ack != epb->epb_cmd_id)
		return;

	bzero(&EPB_ARG.mac, sizeof(EPB_ARG.mac));

	EPB_ARG.mac.port = ep->ep_port;

	if (ep->ep_if.if_flags & IFF_ALLMULTI)
		EPB_ARG.mac.mode |= MMODE_AMULTI;

	if (ep->ep_if.if_flags & IFF_PROMISC) {
		/* fill the DA hash filter table with ones in
		 * promiscuous mode.
		 */
		EPB_ARG.mac.mode |= MMODE_PROM | MMODE_AMULTI;
	}
	if (0 != (EPB_ARG.mac.mode & MMODE_AMULTI)) {
		for (i = 0;
		     i < sizeof(EPB_ARG.mac.hash)/sizeof(EPB_ARG.mac.hash[0]);
		     i++)
			EPB_ARG.mac.hash[i] = -1;
	} else {
		bcopy(ep->ep_hash, EPB_ARG.mac.hash,
		      sizeof(EPB_ARG.mac.hash));
	}

	EPB_ARG.mac.addr[0] = ep->ep_ac.ac_enaddr[1];
	EPB_ARG.mac.addr[1] = ep->ep_ac.ac_enaddr[0];
	EPB_ARG.mac.addr[2] = ep->ep_ac.ac_enaddr[3];
	EPB_ARG.mac.addr[3] = ep->ep_ac.ac_enaddr[2];
	EPB_ARG.mac.addr[4] = ep->ep_ac.ac_enaddr[5];
	EPB_ARG.mac.addr[5] = ep->ep_ac.ac_enaddr[4];

	epb_op(epb, ECMD_MAC_PARMS);

	/* ensure the port is alive before giving it output */
	epb_wait(epb);
}


/* compute DA hash value
 */
static int
ep_dahash(u_char *addr)			/* MAC address */
{
	int hv;

	/* This is not called often, and so clarity is more important than
	 *	speed.
	 */
	hv = addr[0] ^ addr[1] ^ addr[2] ^ addr[3] ^ addr[4] ^ addr[5];

	return (hv & 0xff);
}


/* turn on a DA hash bit
 */
static void
ep_hash1(struct ep_info *ep,
	 mval_t b)
{
	u_char *hp = &ep->ep_hash[0];

	hp[b/8] |= (((unsigned char)0x80) >> (b % 8));
}



/* turn off a DA hash bit
 */
static void
ep_hash0(struct ep_info *ep,
	 mval_t b)
{
	u_char *hp = &ep->ep_hash[0];

	hp[b/8] &= ~(((unsigned char)0x80) >> (b % 8));
}


/* Empty output queue for one port.
 *	Things should already be sufficently locked.
 */
static void
ep_clrsnd(struct ep_info *ep)
{
	struct mbuf *m;

	for (;;) {
		IF_DEQUEUE(&ep->ep_if.if_snd, m);
		if (!m)
			break;
		m_freem(m);
	}
	ep->ep_job_head = ep->ep_job_tail = ep->ep_jobs;
	ep->ep_job_cnt = 0;
}


/* Clear the DMA structures
 * The entire board and all interfaces must be shut down before calling here.
 */
static void
epb_clrdma(struct epb_info *epb)
{
	struct ep_info *ep;
	int i;


	while (epb->epb_in_sml_num != 0) {
		m_freem(epb->epb_in_sml_m[epb->epb_in_sml_h]);
		epb->epb_in_sml_h = (epb->epb_in_sml_h+1) % EPB_MAX_SML;
		epb->epb_in_sml_num--;
	}
	while (epb->epb_in_big_num != 0) {
		m_freem(epb->epb_in_big_m[epb->epb_in_big_h]);
		epb->epb_in_big_h = (epb->epb_in_big_h+1) % EPB_MAX_BIG;
		epb->epb_in_big_num--;
	}

	/* clear all output queues */
	for (ep = &epb->epb_ep[0]; ep < &epb->epb_ep[EP_PORTS]; ep++)
		ep_clrsnd(ep);

	epb->epb_state |= EPB_ST_SLEEP;

	epb->epb_h2bp = &epb->epb_h2b[0];
	epb->epb_h2b_lim = epb->epb_h2bp + EPB_H2B_LEN - 1;
	for (i = 0; i < EPB_H2B_LEN; i++) {
		epb->epb_h2b[i].epb_h2b_op = EPB_H2B_EMPTY;
		epb->epb_h2b[i].epb_h2b_addr_hi = ~i;
		epb->epb_h2b[i].epb_h2b_len = i;
		epb->epb_h2b[i].epb_h2b_addr = i;
	}

	epb->epb_b2hp = &epb->epb_b2h[0];
	epb->epb_b2h_lim = &epb->epb_b2h[EPB_B2H_LEN];
	bzero((void*)epb->epb_b2hp, sizeof(epb->epb_b2h));
	epb->epb_b2h_sernum = 1;
	for (i = 0; i < EPB_B2H_LEN; i++)
		epb->epb_b2h[i].b2h_sernum = (i+1+EPB_B2H_LEN*255)%256;
}


/* send a word to the board through the GIO bus slave interface
 */
static void
epb_nibble(struct epb_info *epb,
	  __uint32_t w)
{
	register int i;

	for (i = 0; i < 8; i++) {
		/* Send a nibble.
		 */
		*epb->epb_gio = (((w * EPB_GIO_FLAG0) & EPB_GIO_FLAG_MASK)
				 | EPB_GIO_H2B_INTR);
		w >>= 4;
		DELAYBUS(DELAY_NIBBLE);
	}
}


/* signal an address and an operation number to the board
 */
static int				/* 0=no news from the board */
epb_signal(struct epb_info *epb,
	   int dlay,			/* try this long in usec */
	   u_short op)			/* send this to the board */
{
	inventory_t *pinv;

	/* reset the board
	 */
	if (0 != (op & EP_SIGNAL_RESET)) {
		/* reset the board using the DANG chip. */
		op &= ~EP_SIGNAL_RESET;
		EV_SET_REG(epb->epb_swin+DANG_GIORESET, 0);

		/* Resetting the board turns things off.
		 * So mark everything off.
		 */
		epb_clrdma(epb);

		bzero(&epb->epb_prev_stats, sizeof(epb->epb_prev_stats));
		epb->epb_state &= ~EPB_ST_STATS;
		bzero(&epb->epb_dbg, sizeof(epb->epb_dbg));

		bzero(&epb->epb_hc, sizeof(epb->epb_hc));
		epb->epb_cmd_id = 0;

		EPB_RESET_DELAY(epb);
		EV_SET_REG(epb->epb_swin+DANG_GIORESET, 1);
	}

	epb->epb_hc.cmd = ECMD_NOP;
	epb->epb_cmd_line = __LINE__;
	epb->epb_hc.cmd_id = ++epb->epb_cmd_id;
	epb->epb_hc.sign = 0;

	do {
		epb_nibble(epb, EP_GIO_DL_PREAMBLE);
		epb_nibble(epb, op);
		epb_nibble(epb, kvtophys(&epb->epb_hc));
		epb_nibble(epb, ~(op ^ kvtophys(&epb->epb_hc)));
		DELAYBUS(START_CHECK);
		if (epb->epb_hc.sign == EPB_SIGN) {
			if (epb->epb_vers != epb->epb_hc.vers) {
				epb->epb_vers = epb->epb_hc.vers;
				pinv = find_inventory(0, INV_NETWORK,
						      INV_NET_ETHER,
						      INV_ETHER_EP,
						      EPB_UNIT(epb), -1);
				/* if our inventory entry exists, then
				 * change the firmware date code without
				 * changing the slot or adapter numbers.
				 */
				if (pinv) {
					pinv->inv_state &= (EP_VERS_SLOT_M
							    | EP_VERS_ADAP_M);
					pinv->inv_state |=(epb->epb_vers
							   & EP_VERS_DATE_M);
				}
			}
			/* It is alive.
			 * Ensure that the board always, not just sometimes,
			 * has an extra interrupt.
			 */
			*epb->epb_gio = EPB_GIO_H2B_INTR;
			epb->epb_cmd_line = __LINE__;
			return 1;
		}
	} while (0 <= (dlay -= (DELAY_NIBBLE*4*8+START_CHECK)));
	/* We failed to awaken the board. */

#ifdef NOTDEF
	/* bang on the host-to-board interrupt to help debug the hardware */
	{
		static int first = 0;
		time_t limit = lbolt+HZ*60*5;
		int i;

		if (++first >= 5) {
			do {
				*epb->epb_gio =  EPB_GIO_H2B_INTR;
				for (i = 0; i < 1000; i++) {
					if (EV_GET_REG(epb->epb_swin
						       + DANG_INTR_STATUS)
					    & DANG_ISTAT_GIO2)
						break;
				}
				if (i > 20)
					printf(".");
				*epb->epb_gio = (EPB_GIO_FLAG0
						 | EPB_GIO_FLAG1
						 | EPB_GIO_FLAG2
						 | EPB_GIO_FLAG3);
				for (i = 0; i < 1000; i++) {
					if (EV_GET_REG(epb->epb_swin
						       + DANG_INTR_STATUS)
					    & DANG_ISTAT_GIO2)
						break;
				}
				if (i > 20)
					printf("+");
			} while (limit > lbolt);
		}
	}
#endif
	return 0;
}


static int				/* 0=bad board */
epb_do_reset(struct epb_info *epb,
	     char *msg)
{
#define GET_DANG(nm) EV_GET_REG(epb->epb_swin+(nm))

	if (epb_signal(epb, ((epb->epb_state & EPB_ST_DEBUG)
			     ? DELAY_RESET_DBG
			     : DELAY_RESET),
		       EP_SIGNAL_RESET))
		return 1;

	printf(PF_L2 "failed to reset %s\n"
	       "    PIO_ERR=0x%x IGNORE_GIO_TIMEOUT=%x"
	       " MAX_GIO_TIMEOUT=%x\n"
	       "    DMAS_STATUS=%x DMAS_ERR=%x"
	       " INTR_STATUS=%x INTR_ENABLE=%x\n",
	       PF_P2(epb), msg,
	       GET_DANG(DANG_PIO_ERR),
	       GET_DANG(DANG_IGNORE_GIO_TIMEOUT),
	       GET_DANG(DANG_MAX_GIO_TIMEOUT),
	       GET_DANG(DANG_DMAS_STATUS),
	       GET_DANG(DANG_DMAS_ERR),
	       GET_DANG(DANG_INTR_STATUS),
	       GET_DANG(DANG_INTR_ENABLE));

	/* try to reset a DANG timeout */
	EV_SET_REG(epb->epb_swin+DANG_CLR_TIMEOUT, 0);

	epb->epb_state |= (EPB_ST_ZAPPED | EPB_ST_WHINE);

	return 0;
}


/* tell the board about the host communication area
 */
static int				/* 0=bad board */
epb_run(struct epb_info *epb)
{
	if (!epb_do_reset(epb, "")) {
		if (epb->epb_state & EPB_ST_NO_RESET)
			return 0;
		epb->epb_state |= EPB_ST_NO_RESET;
		if (!epb_do_reset(epb, "after DANG timeout reset"))
			return 0;
	}

	/* Tell the board how to do its work, regardless of its version,
	 * to ensure it has a good place from which to fetch H2B commands.
	 */
	epb_wait(epb);
	EPB_ARG.init.b2h_buf = svirtophys(&epb->epb_b2h[0]);
	EPB_ARG.init.b2h_len = EPB_B2H_LEN;

	EPB_ARG.init.max_bufs = epb_num_big + epb_num_sml;

	EPB_ARG.init.h2b_buf = svirtophys(&epb->epb_h2b[0]);

	if (ep_low_latency) {
		EPB_ARG.init.h2b_dma_delay = MAX_PKT_DELAY/4;
		EPB_ARG.init.int_delay = 1;
		EPB_ARG.init.h2b_sleep = 1;
	} else {
		EPB_ARG.init.h2b_dma_delay = epb_h2b_dma_delay;
		EPB_ARG.init.int_delay = epb_int_delay;
		EPB_ARG.init.h2b_sleep = epb_h2b_sleep;
	}

	EPB_ARG.init.sml_size = EPB_SML_SIZE;

	epb_op(epb, ECMD_INIT);
	epb_wait(epb);

	if (0 != (EP_VERS_CKSUM & epb->epb_vers)) {
		printf(PF_L2 "bad firmware checksum\n", PF_P2(epb));
		epb->epb_state |= (EPB_ST_NO_RESET | EPB_ST_ZAPPED);
		return 0;
	}
	if ((epb->epb_vers & EP_VERS_DATE_M) < EP_VERS_MIN_DATE
	    || (epb->epb_vers & EP_VERS_FAM_M) != EP_VERS_FAM) {
		printf(PF_L2 "firmware too old or new\n", PF_P2(epb));
		epb->epb_state |= (EPB_ST_NO_RESET | EPB_ST_ZAPPED);
		return 0;
	}
	epb->epb_state &= ~(EPB_ST_NO_RESET | EPB_ST_ZAPPED | EPB_ST_NEED_RS
			    | EPB_ST_WHINE);
	epb->epb_rs_timer = 0;

	/* See if the board is really alive.
	 */
	return (epb->epb_hc.cmd_ack == epb->epb_cmd_id);

#undef GET_DANG
}


/* Reset all of a board because we do not want to play any more at all.
 */
static void
epb_reset(struct epb_info *epb)
{
	struct ep_info *ep;
	struct etheraddr eaddr;

	epb->epb_state &= ~EPB_ST_WHINE;	/* allow more complaints */

	/* quit if it does not awaken */
	if (!epb_run(epb))
		return;

	for (ep = &epb->epb_ep[0]; ep < &epb->epb_ep[EP_PORTS]; ep++) {
		/* Set output checksumming.
		 *	It would be nice to do this in ep_init(), but
		 *	ether_start() does not allow the init() function to
		 *	change the flags.
		 */
		if (ep_cksum & 2)
			ep->ep_if.if_flags |= IFF_CKSUM;
		else
			ep->ep_if.if_flags &= ~IFF_CKSUM;

		/* get possibly new MAC addresses */
		if (!(epb->epb_state & EPB_ST_GOTMAC)) {
			ep_mac_addr(ep,epb,&eaddr);
			if (!bcmp(ep->ep_ac.ac_enaddr, &ep->ep_eif.eif_addr,
				  sizeof(ep->ep_eif.eif_addr)))
				bcopy(&eaddr, ep->ep_ac.ac_enaddr,
				      sizeof(ep->ep_ac.ac_enaddr));
			bcopy(&eaddr, &ep->ep_eif.eif_addr,
			      sizeof(ep->ep_eif.eif_addr));
		}
	}
	epb->epb_state |= EPB_ST_GOTMAC;
}



/* Get the interface ready to turn on.
 *	The board must be locked.
 */
static int
ep_init_sub(struct ep_info *ep,
	    struct epb_info *epb,
	    int flags)
{
	/* fail if bad EEPROM MAC address or otherwise sick board */
	if ((ep->ep_state & EP_ST_BADMAC)
	    || (epb->epb_state & EPB_ST_ZAPPED))
		return EIO;

	if (flags & IFF_DEBUG)
		epb->epb_state |= EPB_ST_DEBUG;

	/* Assume that the port is being turned on, and tell it so.
	 * Unfortunately that is not necessarily true, there is and
	 * no way to tell from here.  ether_start() calls here only with
	 * the flags it is given by the user program, and not with the
	 * IFF_UP and IFF_RUNNING bits that it sets after.  There are other
	 * calls to the init() function that may leave the interface off.
	 */
	epb->epb_state |= EPB_ST_UP;

	ep->ep_if.if_flags = ((ep->ep_if.if_flags & (IFF_UP|IFF_RUNNING))
			      | flags);

	/* start watchdog on the first port */
	if (epb->epb_ep[0].ep_if.if_timer == 0)
		epb->epb_ep[0].ep_if.if_timer = EPB_TIMER;

	ep->ep_state &= ~(EP_ST_OUT_DONE | EP_ST_OUT_WAITING | EP_ST_OUT_ILL
			  | EP_ST_IN_ILL);
	ep->ep_prev_ipackets = ep->ep_if.if_ipackets;
	ep->ep_prev_opackets = ep->ep_if.if_opackets;

	ep_hash1(ep, (ep->ep_hash_bcast = ep_dahash(etherbroadcastaddr)));
	ep_set_mmode(ep,epb);		/* tell SONIC MAC address & mode */

	epb_fillin(epb);
	return 0;
}


static int				/* 0 or errno */
ep_init(struct etherif *eif,
	int flags)
{
	register struct ep_info *ep;
	register struct epb_info *epb;
	int res;

	GET_CK_EIF(eif,ep,epb);
	LOCK_BD(ep,epb);

	res = ep_init_sub(ep, epb, flags);

	UNLOCK_BD(ep,epb);
	return res;
}


/* See if any interface is alive, and maybe reset the board.
 *	The board must be locked.
 */
static int				/* # of active interfaces */
ep_ck_reset(struct epb_info *epb,
	    int force)			/* 1=reset board if 0 interfaces */
{
	register struct ep_info *ep1;
	register int i;

	/* While counting live ports, update the debugging flag. */
	i = 0;
	epb->epb_state &= ~EPB_ST_DEBUG;
	for (ep1 = &epb->epb_ep[0]; ep1 < &epb->epb_ep[EP_PORTS]; ep1++) {
		if (ep1->ep_if.if_flags & IFF_DEBUG)
			epb->epb_state |= EPB_ST_DEBUG;
		if (iff_alive(ep1->ep_if.if_flags))
			i++;
	}

	/* If one or more interfaces is alive, then do not reset the board.
	 * If no interfaces are alive, then reset the board only if asked.
	 */
	if (force && i == 0) {
		epb->epb_state &= ~EPB_ST_UP;
		epb_reset(epb);
	}

	return i;
}


/* turn off an interface
 *	The board must be locked.
 */
static void
ep_reset_sub(struct ep_info *ep,
	     struct epb_info *epb)
{
	epb_wait(epb);
	bzero(&EPB_ARG.mac, sizeof(EPB_ARG.mac));
	EPB_ARG.mac.port = ep->ep_port;
	epb_op(epb, ECMD_MAC_OFF);

	bzero(&epb->epb_prev_stats.cnts[ep->ep_port],
	      sizeof(epb->epb_prev_stats.cnts[ep->ep_port]));
	epb->epb_state &= ~EPB_ST_STATS;

	ep_clrsnd(ep);
}


/* Turn off an interface for external callers.
 */
static void
ep_reset(struct etherif *eif)
{
	struct ep_info *ep;
	struct epb_info *epb;

	GET_CK_EIF(eif,ep,epb);
	LOCK_BD(ep,epb);

	ep_reset_sub(ep,epb);
	(void)ep_ck_reset(epb, 1);

	UNLOCK_BD(ep,epb);
}


/* Process an ioctl request.
 */
static int				/* return 0 or error number */
ep_ioctl(struct etherif *eif,
	 int cmd,
	 void *data)
{
	register struct ep_info *ep;
	register struct epb_info *epb;
	int error = 0;


	GET_CK_EIF(eif,ep,epb);
	LOCK_BD(ep,epb);

	switch (cmd) {

#define MFR ((struct mfreq *)data)
	case SIOCADDMULTI:
		++ep->ep_nmulti;
		ep->ep_if.if_flags |= IFF_FILTMULTI;
		MFR->mfr_value = ep_dahash(MFR->mfr_key->mk_dhost);
		ep_hash1(ep, MFR->mfr_value);
		ep_set_mmode(ep,epb);
		break;

	case SIOCDELMULTI:
		if (!--ep->ep_nmulti)
			ep->ep_if.if_flags &= ~IFF_FILTMULTI;
		/* Remove bit from the table if no longer in use.
		 */
		if (!mfhasvalue(&eif->eif_mfilter, MFR->mfr_value)
		    && MFR->mfr_value != ep->ep_hash_bcast) {
			ep_hash0(ep, MFR->mfr_value);
		}
		ep_set_mmode(ep,epb);
		break;
#undef MFR


	case SIOC_EP_STO:
	case SIOC_EP_POKE:
#define DWN ((struct ep_dwn*)data)
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		if ((cmd == SIOC_EP_STO && ep_ck_reset(epb, 0) != 0)
		    || DWN->cnt == 0
		    || DWN->cnt > EP_DWN_LEN) {
			error = EINVAL;
			break;
		}
		epb_wait(epb);
		bcopy(DWN, &EPB_ARG.dwn, sizeof(EPB_ARG.dwn));
		epb_op(epb, ECMD_STO);
		epb->epb_state |= EPB_ST_ZAPPED;
		epb->epb_state &= ~EPB_ST_GOTMAC;
		/* The first request to store is not finished until the board
		 *	has copied all of EEPROM to SRAM.
		 */
		epb_wait_usec(epb,DELAY_RESET,__LINE__);
		if (epb->epb_hc.cmd_ack != epb->epb_cmd_id)
			error = EIO;
		break;

	case SIOC_EP_EXEC:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		if (ep_ck_reset(epb, 0)) {
			error = EINVAL;
			break;
		}
		epb_wait(epb);
		if (epb->epb_hc.cmd_ack != epb->epb_cmd_id)
			error = EIO;	/* continue as we complain */
		bcopy(DWN, &EPB_ARG.dwn, sizeof(EPB_ARG.dwn));
		epb->epb_state |= EPB_ST_ZAPPED;
		epb->epb_hc.sign = 0;
		epb_op(epb, ECMD_EXEC);
		epb_wait(epb);
		if (epb->epb_hc.cmd_ack != epb->epb_cmd_id)
			error = EIO;
		epb->epb_hc.cmd = ECMD_NOP;
		break;

	case SIOC_EP_FET:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		if (DWN->cnt == 0
		    || DWN->cnt > EP_DWN_LEN) {
			error = EINVAL;
			break;
		}
		epb_wait(epb);
		bcopy(DWN, &EPB_ARG.dwn, sizeof(EPB_ARG.dwn));
		epb_op(epb, ECMD_FET);
		epb_wait(epb);
		if (epb->epb_hc.cmd_ack != epb->epb_cmd_id)
			error = EIO;
		bcopy((void*)&EPB_RES.dwn_c, &DWN->val, DWN->cnt*4);
#undef DWN
		break;

	case SIOC_EP_VERS:
#define VERS ((struct ep_vers*)data)
		epb_wait(epb);		/* check that the board is alive */
		VERS->vers = epb->epb_vers & ~EP_VERS_BAD_DANG;
		if (epb->epb_state & EPB_ST_BAD_DANG)
			VERS->vers |= EP_VERS_BAD_DANG;
#undef VERS
		break;

	case SIOC_EP_SIGNAL:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		if (ep_ck_reset(epb, 0)
		    || *(__uint32_t*)data > 0xffff) {
			error = EINVAL;
			break;
		}
		if (!epb_signal(epb,DELAY_SIGNAL,*(__uint32_t*)data)) {
			error = EIO;
			break;
		}
		break;

	default:
		error = EINVAL;
	}

	UNLOCK_BD(ep,epb);
	return error;
}



static void
ep_watchdog(struct ifnet *ifp)
{
	struct ep_info *ep, *need_rs;
	struct epb_info *epb;
	struct epb_counts *ncnt, *pcnt;
	struct ifnet *l_ifp;
	int port, j, out_ok, in_ok;
	short flags[EP_PORTS];

	ASSERT(ifp->if_name == ep_name);
	GET_CK_EIF(ifptoeif(ifp),ep,epb);
	ASSERT(ep->ep_port == 0);	/* only on the first port */
	if (ep->ep_port != 0)
		return;
	CK_EPB_LOCK(epb);

	/* Quit if the board is entirely off. */
	if (!(epb->epb_state & EPB_ST_UP)) {
		epb->epb_state &= ~EPB_ST_STATS;
		return;
	}

	/* another second has passed since the last reset */
	epb->epb_rs_timer++;

	/* at least restart the watchdog. */
	ifp->if_timer = EPB_TIMER;

	/* Fetch counters from the board, if they are not already waiting.
	 */
	epb_wait(epb);
	if (!(epb->epb_state & EPB_ST_NEED_RS)
	    && !(epb->epb_state & EPB_ST_STATS)) {
		EPB_ARG.cntptr = kvtophys(&epb->epb_new_stats);
		epb_op(epb,ECMD_FET_CNT);
		epb_wait(epb);
	}

	/* For each counter for each port, note any differences */
	if ((epb->epb_state & EPB_ST_NEED_RS)
	    || ep_force_reset) {
		need_rs = ep;
	} else {
		need_rs = 0;

		for (port = 0; port < EP_PORTS; port++, ep++, ifp=&ep->ep_if) {
			if (iff_dead(ifp->if_flags))
				continue;
			ncnt = &epb->epb_new_stats.cnts[port];
			pcnt = &epb->epb_prev_stats.cnts[port];

			j = ncnt->tx_error - pcnt->tx_error;
			ifp->if_oerrors += j;
			ifp->if_collisions += ncnt->tx_col - pcnt->tx_col;

			/* If we had solid output errors during the last
			 * second, assume the port is ill.  If the
			 * condition persists, reset the board.
			 */
			out_ok = ifp->if_opackets - ep->ep_prev_opackets;
			ep->ep_prev_opackets = ifp->if_opackets;
			if (out_ok > 0) {
				if (j >= out_ok) {
					if (ep->ep_state & EP_ST_OUT_ILL) {
					    if (!need_rs)
						need_rs = ep;
					}
					ep->ep_state |= EP_ST_OUT_ILL;
				} else {
					ep->ep_state &= ~EP_ST_OUT_ILL;
				}
			}

			/* display some complaints only occassionally
			 */
			if (ncnt->tx_crsl != pcnt->tx_crsl
			    && ep->ep_carrier_lbolt < lbolt) {
				printf(PF_L
				       "no carrier: check Ethernet cable\n",
				       PF_P(ep));
				ep->ep_carrier_lbolt = lbolt + (HZ*30);
			}
			if (0 == (epb->epb_new_stats.cnts[0].links
				  & (1<<port))
			    && ep->ep_link_lbolt < lbolt) {
				printf(PF_L
				       "no LINK: check Ethernet cable\n",
				       PF_P(ep));
				ep->ep_link_lbolt = lbolt + (HZ*60*60*24);
			}

			/* display other complaints when they occur
			 */
			in_ok = ifp->if_ipackets - ep->ep_prev_ipackets;
			ep->ep_prev_ipackets = ifp->if_ipackets;
			if (0 != (j = ncnt->rx_mp - pcnt->rx_mp)) {
				ifp->if_ierrors += j;
				if (ifp->if_flags & IFF_DEBUG)
					printf(PF_L "missed %d packets\n",
					       PF_P(ep),j);
				/* If no valid packets were received in the
				 * last second, but some were missed, assume
				 * the port is sick.
				 */
				if (in_ok == 0) {
					if (ep->ep_state & EP_ST_IN_ILL) {
					    if (!need_rs)
						need_rs = ep;
					}
					ep->ep_state |= EP_ST_IN_ILL;
				} else {
					ep->ep_state &= ~EP_ST_IN_ILL;
				}
			} else if (in_ok != 0) {
				ep->ep_state &= ~EP_ST_IN_ILL;
			}

#			define TST(nm) (0 != (j = ncnt->nm - pcnt->nm))
#			define TST_DBG(nm) (TST(nm)			\
					    && (ifp->if_flags & IFF_DEBUG))
#			define MSG(msg) printf(PF_L"%d "msg"\n",PF_P(ep),j)
#			define RS_IN_MSG(msg) {MSG(msg);		\
				if (j > in_ok && !need_rs) need_rs = ep;}
#			define RS_OUT_MSG(msg) {MSG(msg);		\
				if (j > out_ok && !need_rs) need_rs = ep;}
			if (TST(rx_babble))
				RS_IN_MSG("packets too long");
			if (TST(rx_rfo))
				RS_IN_MSG("FIFO overruns");
			if (TST_DBG(rx_rde))
				RS_IN_MSG("no receive descriptors");
			if (TST_DBG(rx_rbe))
				RS_IN_MSG("no receive buffers");
			if (TST_DBG(rx_rbae))
				RS_IN_MSG("no receive buffer areas");
			if (TST(tx_exd))
				MSG("excessive deferrals");
			if (TST_DBG(tx_exc))
				MSG("excessive collisions");
			if (TST(tx_owc))
				MSG("late collisions");
			if (TST(tx_pmb))
				RS_OUT_MSG("transmission errors");
			if (TST(tx_fu))
				RS_OUT_MSG("FIFO underruns");
			if (TST(tx_bcm))
				RS_OUT_MSG("byte count mismatches");
			if (TST_DBG(br))
				RS_OUT_MSG("bus retries");
#			undef TST
#			undef TST_DBG
#			undef MSG

			/* If the port has gone dead, reset the interface.
			 * It must be dead if the output queue was not empty
			 * at the start of the previous second, but the
			 * board never said it completed any output.
			 */
			if ((ep->ep_state & EP_ST_OUT_WAITING)
			    && !(ep->ep_state & EP_ST_OUT_DONE)
			    && !need_rs)
				need_rs = ep;
			ep->ep_state &= ~(EP_ST_OUT_DONE | EP_ST_OUT_WAITING);
			if (ep->ep_if.if_snd.ifq_len != 0)
				ep->ep_state |= EP_ST_OUT_WAITING;
		}
	}

	/* If we are not going to reset the board,
	 * start fetching the counters for next time.
	 */
	if (!need_rs
	    || epb->epb_rs_timer <= ep_reset_limit
	    || (epb->epb_state & EPB_ST_NO_RESET)) {
		bcopy(&epb->epb_new_stats, &epb->epb_prev_stats,
		      sizeof(epb->epb_prev_stats));

		EPB_ARG.cntptr = kvtophys(&epb->epb_new_stats);
		epb_op(epb,ECMD_FET_CNT);
		epb->epb_state |= EPB_ST_STATS;

	} else {
		ep_force_reset = 0;

		/* lock and turn off all of the ports on the board.
		 */
		ifp = &(ep = epb->epb_ep)->ep_if;
		if (ifp->if_flags & IFF_DEBUG)
			printf(PF_L"automatic reset\n", PF_P(need_rs));

		l_ifp = ifp;
		for (port = 0; port < EP_PORTS; port++, ep++, ifp=&ep->ep_if) {
			flags[port] = ifp->if_flags;
			if (iff_alive(flags[port])) {
				/* lock a port to fiddle with its state */
				if (l_ifp != ifp) {
					IFNET_UNLOCK(l_ifp);
					IFNET_LOCK(ifp);
					l_ifp = ifp;
				}
				ifp->if_flags &= ~IFF_ALIVE;
				ep_reset_sub(ep,epb);
			}
		}
		ifp = &(ep = epb->epb_ep)->ep_if;
		/* lock the board (port 0) before resetting it */
		if (l_ifp != ifp) {
			IFNET_UNLOCK(l_ifp);
			IFNET_LOCK(ifp);
			l_ifp = ifp;
		}
		/* reset the board */
		(void)ep_ck_reset(epb, 1);

		/* restore the ports that were on before they were turned off
		 */
		for (port = 0; port < EP_PORTS; port++, ep++, ifp=&ep->ep_if) {
			if (iff_alive(flags[port])) {
				/* lock a port to fiddle with its state */
				if (l_ifp != ifp) {
					IFNET_UNLOCK(l_ifp);
					IFNET_LOCK(ifp);
					l_ifp = ifp;
				}
				ifp->if_flags |= IFF_ALIVE;
				ep_init_sub(ep,epb,flags[port]);
			}
		}
		/* leave the board locked */
		if (l_ifp != (ifp = &epb->epb_ep[0].ep_if)) {
			IFNET_UNLOCK(l_ifp);
			IFNET_LOCK(ifp);
		}
	}
}



/* E-Plex output.
 *	If the destination is this machine or broadcast, send the packet
 *	to the loop-back device.  We cannot talk to ourself.
 */
static int				/* 0 or error # */
ep_transmit(struct etherif *eif,
	    struct etheraddr *edst,
	    struct etheraddr *esrc,
	    u_short type,
	    struct mbuf *m0)
{
	struct mbuf *m1, *m2;
	struct mbuf *mloop = 0;
	struct ep_out *bp;
	int blen1, blen2, blen3;
	struct ep_info *ep;
	struct epb_info *epb;
	struct ep_job *job;
	int inx;


	GET_CK_EIF(eif,ep,epb);

	if (iff_dead(ep->ep_if.if_flags)
	    || (epb->epb_state & EPB_ST_ZAPPED)) {
		IF_DROP(&ep->ep_if.if_snd);
		m_freem(m0);
		return EHOSTDOWN;
	}

	if (IF_QFULL(&ep->ep_if.if_snd)) {
		LOCK_BD(ep,epb);
		epb_b2h(epb, 0);
		UNLOCK_BD(ep,epb);
		if (IF_QFULL(&ep->ep_if.if_snd)) {
			IF_DROP(&ep->ep_if.if_snd);
			m_freem(m0);
			return ENOBUFS;
		}
	}

	/* Get room for the Ethernet header.
	 * Use what we were given if possible.
	 */
	if (!M_HASCL(m0)
	    && m0->m_off >= MMINOFF+sizeof(*bp)
	    && (bp = mtod(m0, struct ep_out*), OUT_ALIGNED(bp))) {
		ASSERT(m0->m_off <= MSIZE);
		m1 = 0;
		--bp;
	} else {
		METER(epb->epb_dbg.out_alloc++);
		m1 = m_get(M_DONTWAIT, MT_DATA);
		if (0 == m1) {
			IF_DROP(&ep->ep_if.if_snd);
			m_freem(m0);
			return ENOBUFS;
		}
		bp = mtod(m1, struct ep_out*);
		m1->m_len = sizeof(*bp);
	}

	bp->ep_out_dst = *(struct etheraddr*)edst;
	bp->ep_out_src = *(struct etheraddr*)esrc;
	bp->ep_out_type = type;
	bp->ep_out_pad = 0;		/* keep noise out of checksum */

	job = ep->ep_job_head;

	/* Let the board compute the TCP or UDP checksum.
	 * Start the checksum with the checksum of the psuedo-header
	 * and everthing else in the UDP or TCP header up to but
	 * not including the checksum.
	 */
	if (!(m0->m_flags & M_CKSUMMED)) {
		job->job_dma[0].epb_h2b_offsum = 0;
		job->job_dma[0].epb_h2b_cksum = 1;
	} else {
		struct ip *ip;
		u_long cksum;
		ushort *unsum;
		int hlen, startsum;

		/* Find the UDP or TCP header after IP header, and
		 * the checksum within.
		 */
		unsum = mtod(m0, ushort*);
		ip = (struct ip*)unsum;
		hlen = ip->ip_hl<<2;
		startsum = hlen+sizeof(*bp);
again:
		if (&ip->ip_p >= (u_char*)unsum+m0->m_len) {
			printf(PF_L "bad output checksum offset\n", PF_P(ep));
			m_freem(m1);
			m_freem(m0);
			return EPROTONOSUPPORT;
		}
		switch (ip->ip_p) {
		case IPPROTO_TCP:
			METER(epb->epb_dbg.out_cksum_tcp++);
			job->job_dma[0].epb_h2b_offsum = startsum+8*2;
			break;

		case IPPROTO_UDP:
			METER(epb->epb_dbg.out_cksum_udp++);
			job->job_dma[0].epb_h2b_offsum = startsum+3*2;
			break;

		case IPPROTO_ENCAP:
			METER(epb->epb_dbg.out_cksum_encap++);
			startsum += hlen;
			ip = (struct ip*)((char*)ip+hlen);
			hlen = ip->ip_hl<<2;
			goto again;

		default:
			printf(PF_L "impossible output checksum\n", PF_P(ep));
			m_freem(m1);
			m_freem(m0);
			return EPROTONOSUPPORT;
		}

		/* compute checksum of the psuedo-header.
		 */
		cksum = (ip->ip_len-hlen
			 + htons((ushort)ip->ip_p)
			 + ((ip->ip_src.s_addr >> 16) & 0xffff)
			 + (ip->ip_src.s_addr & 0xffff)
			 + ((ip->ip_dst.s_addr >> 16) & 0xffff)
			 + (ip->ip_dst.s_addr & 0xffff));

		/* Adjust checksum by MAC and IP headers.
		 * Do it in one glup if we were able to put the MAC header
		 * in the first mbuf.
		 */
		if (0 == m1) {
			cksum += ~in_cksum_sub((ushort*)bp, startsum,
					       0);
		} else {
			cksum += ~in_cksum_sub((ushort*)bp, sizeof(*bp),
					       in_cksum_sub(unsum,
							startsum-sizeof(*bp),
							0));
		}

		cksum = (cksum & 0xffff) + (cksum >> 16);
		job->job_dma[0].epb_h2b_cksum = cksum + (cksum>>16);
	}

	if (0 == m1) {
		m0->m_off -= sizeof(*bp);
		m0->m_len += sizeof(*bp);
	} else {
		m1->m_next = m0;
		m0 = m1;
	}


	/* Compute DMA list for the message.
	 *
	 * If the chain of mbufs is too messy, copy it to a cluster
	 * and start over.
	 */
restart:;
	blen1 = 0;
	m2 = m0;
	inx = 1;
	do {
		void *dp;
		paddr_t physp;

		if (0 == m2->m_len)
			continue;
		dp = mtod(m2, void*);
		if (!OUT_ALIGNED(dp)
		    || !OUT_ALIGNED(blen1)
		    || inx >= EP_MAX_DMA_ELEM) {
			/* We must copy the chain.
			 */
copy_chain:;
			METER(epb->epb_dbg.out_copy++);
			blen1 = m_length(m0);
			ASSERT(blen1 <= VCL_MAX);

			m1 = m_vget(M_DONTWAIT,blen1,MT_DATA);
			if (0 == m1) {
				ep->ep_if.if_odrops++;
				IF_DROP(&ep->ep_if.if_snd);
				m_freem(mloop);
				m_freem(m0);
				return ENOBUFS;
			}
			(void)m_datacopy(m0, 0,blen1, mtod(m1,caddr_t));
			m_freem(m0);
			m0 = m1;
			bp = mtod(m0, struct ep_out*);
			goto restart;
		}

		blen3 = m2->m_len;
		physp = kvtophys(dp);
		/* request 2nd DMA if it spans a physical page
		 */
		if (io_pnum(dp) != io_pnum((__psunsigned_t)dp + blen3-1)) {
			ASSERT(blen3<IO_NBPP*2);
			METER(epb->epb_dbg.pg_cross++);
			if (inx >= EP_MAX_DMA_ELEM-1)
				goto copy_chain;
			blen2 = IO_NBPP - (physp & IO_POFFMASK);
			job->job_dma[inx].epb_h2b_addr = physp;
			job->job_dma[inx].epb_h2b_addr_hi = ADDR_HI(physp);
			job->job_dma[inx].epb_h2b_outlen = blen2;
			job->job_dma[inx].epb_h2b_op = EPB_H2B_O_CONT;
			inx++;
			blen1 += blen2;
			blen3 -= blen2;
			dp = (void*)((__psunsigned_t)dp + blen2);
			physp = kvtophys(dp);
		}
		job->job_dma[inx].epb_h2b_addr = physp;
		job->job_dma[inx].epb_h2b_addr_hi = ADDR_HI(physp);
		job->job_dma[inx].epb_h2b_outlen = blen3;
		job->job_dma[inx].epb_h2b_op = EPB_H2B_O_CONT;
		inx++;
		blen1 += blen3;
	} while (0 != (m2 = m2->m_next));
	ASSERT(blen1 <= ETHERMTU+sizeof(*bp));

	blen1 -= sizeof(bp->ep_out_pad);
	ep->ep_if.if_obytes += blen1;

	job->job_dma[inx-1].epb_h2b_op = EPB_H2B_O_END;
	job->job_dma[0].epb_h2b_totlen = blen1;
	job->job_dma[0].epb_h2b_op = inx;

	if (++job >= &ep->ep_jobs[MAX_JOBS])
		job = ep->ep_jobs;
	ep->ep_job_head = job;

	/* Check whether snoopers want to copy this packet.
	 */
	if (RAWIF_SNOOPING(&ep->ep_rawif)
	    && snoop_match(&ep->ep_rawif, (caddr_t)&bp->ep_out_dst, m0->m_len))
		ether_selfsnoop(eif,
				(struct ether_header*)&bp->ep_out_dst, m0,
				sizeof(*bp),
				blen1-(sizeof(*bp)-sizeof(bp->ep_out_pad)));

	IF_ENQUEUE(&ep->ep_if.if_snd, m0);


	/* Tell the board about the new job.
	 */
	if (ep->ep_job_cnt < EP_MAX_OUTQ) {
		LOCK_BD(ep,epb);
		ep_snd_out(ep,epb);
		UNLOCK_BD(ep,epb);
	}

	return 0;
}


/* Tell the board about work it is supposed to do.
 *	The board must be locked.
 */
static void
ep_snd_out(struct ep_info *ep,
	   struct epb_info *epb)
{
	int i;
	struct ep_job *job;
	volatile struct epb_h2b *h2bp;

	job = ep->ep_job_tail;
	while (ep->ep_job_cnt < EP_MAX_OUTQ
	       && job != ep->ep_job_head) {
		i = job->job_dma[0].epb_h2b_op;
		h2bp = epb->epb_h2bp;
		if (h2bp+i >= epb->epb_h2b_lim) {
			/* Wrap to start of host-to-board region if there
			 * is not enough room.
			 */
			epb->epb_h2b[0].epb_h2b_op = EPB_H2B_EMPTY;
			h2bp->epb_h2b_op = EPB_H2B_WRAP;
			h2bp = epb->epb_h2b;
		}
		h2bp += i;
		epb->epb_h2bp = h2bp;

		/* Install the request backwards so the board
		 * never sees an incomplete request.
		 */
		job->job_dma[0].epb_h2b_op = EPB_H2B_O+ep->ep_port;
		job->job_dma[0].epb_h2b_addr_hi = epb->epb_h2b-h2bp;
		h2bp->epb_h2b_op = EPB_H2B_EMPTY;
		do {
			*--h2bp = job->job_dma[--i];
		} while (i != 0);

		ep->ep_job_cnt++;
		if (++job >= &ep->ep_jobs[MAX_JOBS])
			job = ep->ep_jobs;
		--epb->epb_peek_cnt;
	}
	if (job == ep->ep_job_tail)
		return;			/* done if did nothing */
	ep->ep_job_tail = job;

	/* Ensure that the board knows about the packet.
	 */
	if (epb->epb_peek_cnt <= 0 || epb->epb_peek_lbolt != lbolt)
		epb_b2h(epb, 0);
	if ((epb->epb_state & EPB_ST_SLEEP)
	    || ep->ep_ps_enabled
	    || ep_low_latency) {
		epb->epb_state &= ~(EPB_ST_SLEEP|EPB_ST_NOPOKE);
		METER(epb->epb_dbg.board_poke++);
		epb_wait(epb);
		epb_op(epb,ECMD_WAKEUP);
	} else {
		epb->epb_state |= EPB_ST_NOPOKE;
	}
}


/* send tell the board about an input mbuf
 *	The board must be locked.
 */
static void
ep_add_mbuf(struct epb_info *epb,
	    int op,
	    __psunsigned_t addr)
{
	volatile struct epb_h2b *h2bp;

	/* install things in the right order, to resolve race with board
	 */
	h2bp = epb->epb_h2bp;
	if (h2bp >= epb->epb_h2b_lim) {
		epb->epb_h2b[0].epb_h2b_op = EPB_H2B_EMPTY;
		h2bp->epb_h2b_op = EPB_H2B_WRAP;
		h2bp = &epb->epb_h2b[0];
	}

	(epb->epb_h2bp = h2bp+1)->epb_h2b_op = EPB_H2B_EMPTY;

	h2bp->epb_h2b_addr = addr;
	h2bp->epb_h2b_addr_hi = ADDR_HI(addr);
	h2bp->epb_h2b_op = op;
}


/* Fill up the board stocks of empty buffers
 *	Interrupts must be off.
 */
static int				/* 1=did some filling */
epb_fillin(struct epb_info *epb)
{
	struct mbuf *m;
	union epb_in *p;
	int worked = 0;


	/* Do not bother reserving buffers if not turned on.
	 */
	if (!(epb->epb_state & EPB_ST_UP))
		return worked;

#ifdef OS_METER
	if (epb->epb_in_sml_num <= 1)
		epb->epb_dbg.no_sml++;
	if (epb->epb_in_big_num <= 1)
		epb->epb_dbg.no_big++;
#endif

	while (epb->epb_in_big_num < epb_num_big) {
		m = m_vget(M_DONTWAIT,
			   EPB_RPKT_SIZE+sizeof(struct epb_in_raw),
			   MT_DATA);
		if (!m)
			break;
		p = mtod(m,union epb_in*);
		epb->epb_in_big_m[epb->epb_in_big_t] = m;
		epb->epb_in_big_d[epb->epb_in_big_t] = p;
#ifdef WBCACHE
		dki_dcache_wbinval(&p->epb_in_start, EPB_RPKT_SIZE+EPB_PAD);
#endif
		ep_add_mbuf(epb, EPB_H2B_BIG, kvtophys(&p->epb_in_start));
		epb->epb_in_big_t = (epb->epb_in_big_t+1) % EPB_MAX_BIG;
		++epb->epb_in_big_num;
		worked = 1;
	}
	while (epb->epb_in_sml_num < epb_num_sml) {
		m = m_get(M_DONTWAIT,MT_DATA);
		if (!m)
			break;
		/* A subsequent mtod() would put the mbuf header into the
		 * cache which would undo the cache invalidation.
		 */
		p = mtod(m,union epb_in*);
		epb->epb_in_sml_m[epb->epb_in_sml_t] = m;
		epb->epb_in_sml_d[epb->epb_in_sml_t] = p;
#ifdef WBCACHE
		dki_dcache_wbinval(&p->epb_in_start, MLEN);
#endif
		ep_add_mbuf(epb, EPB_H2B_SML, kvtophys(&p->epb_in_start));
		epb->epb_in_sml_t = (epb->epb_in_sml_t+1) % EPB_MAX_SML;
		++epb->epb_in_sml_num;
		worked = 1;
	}

	return worked;
}



/* Deal with a complete input packet in a string of mbufs.
 *	The board must be locked.
 */
static void
epb_input(struct epb_info *epb,
	  struct ep_info *ep,
	  int op,
	  int len)
{
	struct mbuf *m;
	union epb_in *bp;
	int snoopflags;
	u_long cksum;
	struct ip *ip;
	int hlen, cklen;


	if (op == EPB_B2H_IN_SML) {
		BAD_BD(epb,epb->epb_in_sml_num > 0, return);
		m = epb->epb_in_sml_m[epb->epb_in_sml_h];
		bp = epb->epb_in_sml_d[epb->epb_in_sml_h];
		epb->epb_in_sml_h = (epb->epb_in_sml_h+1) % EPB_MAX_SML;
		epb->epb_in_sml_num--;
		dki_dcache_inval(&bp->epb_in_start,len);
		/* must flush the cache before fiddling with mbuf header */
		m->m_len = len+sizeof(bp->epb_in_raw);
	} else {
		BAD_BD(epb,epb->epb_in_big_num > 0, return);
		m = epb->epb_in_big_m[epb->epb_in_big_h];
		bp = epb->epb_in_big_d[epb->epb_in_big_h];
		epb->epb_in_big_h = (epb->epb_in_big_h+1) % EPB_MAX_BIG;
		epb->epb_in_big_num--;
		dki_dcache_inval(&bp->epb_in_start,len);
		/* must flush the cache before fiddling with mbuf header */
		m_adj(m, len-EPB_RPKT_SIZE);
	}
	if (iff_dead(ep->ep_if.if_flags)) {
		m_freem(m);
		return;
	}

	UNLOCK_BD(ep,epb);

	ep->ep_if.if_ibytes += len;
	ep->ep_if.if_ipackets++;

	snoopflags = 0;
	if (bp->epb_in_stat & EPB_IN_CRC)
		snoopflags |= SN_ERROR|SNERR_FRAME;
	if (len < ETHERMIN+sizeof(struct ether_header)) {
		snoopflags |= SN_ERROR|SNERR_TOOSMALL;
		/* if we have less than 14 bytes, then extend with
		 * zeros to keep snoop_input() from crashing.
		 */
		if (m->m_len < sizeof(*bp)) {
			bzero(&((char*)bp)[m->m_len], sizeof(*bp) - m->m_len);
			m->m_len = sizeof(*bp);
		}
	}
	if (len > ETHERMTU+sizeof(struct ether_header))
		snoopflags |= SN_ERROR|SNERR_TOOBIG;
	if (snoopflags != 0)
		ep->ep_if.if_ierrors++;

	/* Finish TCP or UDP checksum on IP non-fragments.
	 */
	if ((ep_cksum & 1)
	    && snoopflags == 0
	    && bp->epb_in_eh.ether_type == ETHERTYPE_IP) {
		METER(epb->epb_dbg.in_cksums++);

		ip = (struct ip*)(bp+1);
		hlen = ip->ip_hl<<2;
		cklen = len - (hlen+sizeof(*bp));

		/* Do not bother if it is an IP fragment or not TCP nor UDP.
		 */
		if (0 != (ip->ip_off & ~IP_DF)) {
			METER(epb->epb_dbg.in_cksum_frag++);
		} else if (ip->ip_p != IPPROTO_TCP
			   && ip->ip_p != IPPROTO_UDP) {
			METER(epb->epb_dbg.in_cksum_proto++);
		} else {
			/* compute checksum of the psuedo-header
			 */
			cksum = (bp->epb_in_cksum
				 + ip->ip_len-hlen
				 + htons((ushort)ip->ip_p)
				 + (ip->ip_src.s_addr >> 16)
				 + (ip->ip_src.s_addr & 0xffff)
				 + (ip->ip_dst.s_addr >> 16)
				 + (ip->ip_dst.s_addr & 0xffff));
			if (cklen > 0) {
				cksum = in_cksum_sub((ushort*)((char*)ip
							       + hlen),
						     cklen, cksum);
			} else {
				cksum = (cksum & 0xffff) + (cksum >> 16);
				cksum = 0xffff & (cksum + (cksum >> 16));
			}
			if (cksum == 0 || cksum == 0xffff) {
				m->m_flags |= M_CKSUMMED;
			} else {
				/* The TCP input code will check it again
				 * and do any complaining.
				 */
				METER(epb->epb_dbg.in_cksum_bad++);
			}
		}
	}

	IF_INITHEADER(bp, &ep->ep_if, sizeof(*bp));

	ether_input(&ep->ep_eif, snoopflags, m);

	LOCK_BD(ep,epb);
}


/* process the board-to-host ring
 *	The board must be locked, and no other ports must be locked by
 *	the caller.
 */
static void
epb_b2h(struct epb_info *epb, u_long ps_ok)
{
	struct ep_info *ep;
	unsigned int blen, op, port;
	u_char sernum;
	int pokebd;
	volatile struct epb_b2h *b2hp;


	CK_EPB_LOCK(epb);

	/* If recursing, for example output->here->arp->output->here,
	 * then quit
	 */
	if (epb->epb_in_active) {
		METER(epb->epb_dbg.in_active++);
		return;
	}
	epb->epb_in_active = 1;

	epb->epb_peek_cnt = 8;
	epb->epb_peek_lbolt = lbolt;

	if ((epb->epb_state & EPB_ST_NOPOKE)) {
		epb->epb_state &= ~EPB_ST_NOPOKE;
		pokebd = 1;
	} else {
		pokebd = 0;
	}

	b2hp = epb->epb_b2hp;
	for (;;) {
		sernum = b2hp->b2h_sernum;
		blen = b2hp->b2h_val;
		port = b2hp->b2h_port;
		op = b2hp->b2h_op;
		if (sernum != epb->epb_b2h_sernum) {    /* no more new work */
			epb->epb_b2hp = b2hp;

			if (epb->epb_b2h_sernum
			    != (u_char)(sernum+(u_char)EPB_B2H_LEN)) {
				ZAPBD(epb);
				epb->epb_state |= EPB_ST_NEED_RS;
				printf(PF_L2 "bad B2H sernum=0x%x!=0x%x"
				       " at 0x%x op=%d port=%d val=0x%x\n",
				       PF_P2(epb),
				       sernum,epb->epb_b2h_sernum,
				       b2hp,(long)op,(long)port,(long)blen);
			} else {
				/* Fix race in board going to sleep
				 * as host is giving it buffers.
				 */
				if (epb_fillin(epb)) {
					pokebd = 1;
					continue;
				}

				if (pokebd
				    && (epb->epb_state & EPB_ST_SLEEP)) {
					epb->epb_state &= ~EPB_ST_SLEEP;
					METER(epb->epb_dbg.board_poke++);
					epb_wait(epb);
					epb_op(epb,ECMD_WAKEUP);
				}
			}
			epb->epb_in_active = 0;
			return;
		}

		epb->epb_b2h_sernum = sernum+1;
		if (port >= EP_PORTS) {
			if ((epb->epb_state & EPB_ST_DEBUG)
			    && !(epb->epb_state & EPB_ST_WHINE)) {
				printf(PF_L2 "invalid B2H at 0x%x sernum=0x%x"
				       " op=%d port=%d val=0x%x\n",PF_P2(epb),
				       b2hp,sernum,op,port,blen);
				epb->epb_state |= EPB_ST_WHINE;
			}
			continue;
		}
		ep = &epb->epb_ep[port];

		switch (op) {
		case EPB_B2H_SLEEP:	/* port is going to sleep */
			epb->epb_state |= EPB_ST_SLEEP;
			break;

		case EPB_B2H_ODONE:	/* free output mbuf chains */
			if (port != 0)
				LOCK_PORT(ep);
			ep->ep_state |= EP_ST_OUT_DONE;
			ep->ep_job_cnt -= blen;
			do {
				struct mbuf *m;
				IF_DEQUEUE(&ep->ep_if.if_snd, m);
				m_freem(m);
			} while (--blen > 0);
			if (port != 0)
				UNLOCK_PORT(ep);
			ep_snd_out(ep,epb);
			if ((ep->ep_ps_enabled & ps_ok) != 0) {
				UNLOCK_BD(ep,epb);
				ps_txq_stat(&ep->ep_if,
					    MAX_JOBS-1 - ep->ep_if.if_snd.ifq_len);
				LOCK_BD(ep,epb);
			}
			break;

		case EPB_B2H_IN_SML:
		case EPB_B2H_IN_BIG:
			epb->epb_state &= ~EPB_ST_SLEEP;
			epb_input(epb,ep, op, blen);
			break;

		default:		/* sick board */
			if ((epb->epb_state & EPB_ST_DEBUG)
			    && (epb->epb_state & EPB_ST_WHINE)) {
				printf(PF_L2 "invalid B2H at 0x%x sernum=0x%x"
				       " op=%d port=%d val=0x%x\n",PF_P2(epb),
				       b2hp, sernum,op,port,blen);
				epb->epb_state |= EPB_ST_WHINE;
			}
			break;
		}

		if (++b2hp >= epb->epb_b2h_lim)
			b2hp = &epb->epb_b2h[0];
	}
}



/* handle interrupts
 */
/* ARGSUSED */
void
if_epintr(eframe_t *ep, void *p)
{
#define EPB ((struct epb_info*)p)

	ASSERT(EPB->epb_ep[0].ep_if.if_name == ep_name);

	LOCK_PORT(&EPB->epb_ep[0]);

	METER(EPB->epb_dbg.ints++);
	*EPB->epb_gio = EPB_GIO_B2H_CLR;
	epb_b2h(EPB, 1);		/* check the DMA pipeline */

	UNLOCK_PORT(&EPB->epb_ep[0]);
#undef EPB
}


/* Called by packet scheduling functions to determine length of driver queue.
 */
static int
ep_txfree_len(struct ifnet *ifp)
{
	struct ep_info *ep;
	struct epb_info *epb;

	GET_CK_EIF(ifptoeif(ifp),ep,epb);

	LOCK_BD(ep, epb);
	epb_b2h(epb, 0);
	UNLOCK_BD(ep, epb);

	return (MAX_JOBS-1 - ifp->if_snd.ifq_len);
}


/* Called by packet scheduling functions to notify driver about queueing state.
 * If setting is not zero then there are queues and driver should
 * update the packet scheduler by calling ps_txq_stat() when the txq length
 * changes.  If setting is 0, then don't call packet scheduler.
 */
static void
ep_setstate(struct ifnet *ifp, int setting)
{
	register struct ep_info *ep;
	register struct epb_info *epb;

	ASSERT(ifp->if_unit < (EP_MAXBD * EP_PORTS_OCT));
	GET_CK_EIF(ifptoeif(ifp),ep,epb);

	/* do not need to lock the port because the write to the byte
	 * is atomic
	 */
	ep->ep_ps_enabled = setting;
}


static struct etherifops epops = {
	ep_init,
	ep_reset,
	ep_watchdog,
	ep_transmit,
	ep_ioctl,
};


/* probe for a single board, and attach if present.
 */
/* ARGSUSED */
void
if_epedtinit(struct edt *edtp)
{
	static repeat = 0;
	struct ep_info *ep;
	struct epb_info *epb;
	__psunsigned_t swin;
	volatile __uint32_t *gio;
	uint unit, slot, adapter;
	int i;
	pgno_t pg;
	uint dang_indx, dang_vers;
	struct etheraddr eaddr;
	struct ps_parms ps_params;
	graph_error_t	rc;
	vertex_hdl_t	io4vhdl, epvhdl;
	int s;
	int state;


	/* keep serial number sane */
	ASSERT((EPB_B2H_LEN % 256 ) != 0);

	/* the watchdog timer must run */
	ASSERT(EPB_TIMER != 0);

	/* keep ether_input() sane */
	ASSERT(sizeof(struct etherbufhead) == sizeof(union epb_in));

	ASSERT(sizeof(struct ether_header)+ETHERHDRPAD \
	       == sizeof(struct ep_out));

	/* Probe for boards only once */
	s = splimp();
	if (repeat) {
		splx(s);
		return;
	}
	repeat = 1;

	/* default reset limit of 30 seconds */
	if (ep_reset_limit == 0)
		ep_reset_limit = 30;

	/* look for all boards */
	unit = 0;
	for (dang_indx = 0; dang_indx < DANG_MAX_INDX; dang_indx++) {
		if ((dang_id[dang_indx] & GIO_SID_MASK) != GIO_ID_EP)
			continue;

		slot = DANG_INX2SLOT(dang_indx);
		adapter = DANG_INX2ADAP(dang_indx);
		swin = DANG_INX2BASE(dang_indx);
		gio = (volatile __uint32_t*)DANG_INX2GIOBASE(dang_indx,
							     0x1f400000);
		if (badaddr(gio, 4)) {
			printf(PF_L2 "bad probe\n",
			       unit, unit+EP_PORTS-1, slot, adapter);
			unit += EP_PORTS_OCT;
			continue;
		}
		i = *gio;
		if ((i & GIO_SID_MASK) != GIO_ID_EP) {
			printf(PF_L2 "bad GIO ID 0x%x\n",
			       unit, unit+EP_PORTS-1, slot, adapter, i);
			unit += EP_PORTS_OCT;
			continue;
		}

		if (unit >= EP_MAXBD*EP_PORTS_OCT) {
			printf(PF_L2 "extra board\n",
			       unit, unit+EP_PORTS-1, slot, adapter);
			unit += EP_PORTS_OCT;
			continue;
		}

		pg = contig_memalloc((sizeof(*epb)+IO_NBPP-1)/IO_NBPP,
				     sizeof(epb),   /* align to a pointer */
				     VM_DIRECT);
		if (pg == 0) {
			printf(PF_L2 "no memory\n",
			       unit, unit+EP_PORTS-1, slot, adapter);
			unit += EP_PORTS_OCT;
			continue;
		}
		epb = (struct epb_info*)small_pfntova_K0(pg);

		/* The board assumes it is aligned. */
		ASSERT(0 == ((__psunsigned_t)epb->epb_h2b
			     % sizeof(epb->epb_h2b[0])));

		bzero(epb,sizeof(*epb));
		epb->epb_swin = swin;
		epb->epb_gio = gio;
		epb->epb_slot = slot;
		epb->epb_adapter = adapter;
		for (i = 0; i < EP_PORTS; i++) {
			epb->epb_ep[i].ep_port = i;
			epb->epb_ep[i].ep_if.if_unit = unit+i;
			epb->epb_ep[i].ep_epb = epb;
		}

#ifdef EP_DEBUG
		printf(PF_L2 "swin=0x%x gio=0x%x\n", PF_P2(epb),swin,gio);
#endif

		/* allocate the interrupt
		 */
		i = dang_intr_conn((evreg_t*)(swin+DANG_INTR_GIO_2),
				   EVINTR_ANY, if_epintr, epb);
		if (i == 0) {
			printf(PF_L2 "failed to allocate interrupt\n",
			       PF_P2(epb));
			unit += EP_PORTS_OCT;
			continue;
		}

		/* we are committed now */
		epb_infop[unit/EP_PORTS] = epb;

		dang_vers = (EV_GET_REG(swin+DANG_VERSION_REG)
			     & DANG_VERSION_MASK) >> DANG_VERSION_SHFT;
		if (dang_vers <= DANG_VERSION_B)
			epb->epb_state |= EPB_ST_BAD_DANG;

		/* enable the interrupt in the DANG */
		EV_SET_REG(swin+DANG_INTR_ENABLE,
			   DANG_IRS_GIO2 | DANG_IRS_ENABLE);

		bzero(&eaddr,sizeof(eaddr));
		/* create ('attach') the interfaces */
		for (i = 0; i < EP_PORTS; i++) {
			ep = &epb->epb_ep[i];
			state = ((epb->epb_slot << EP_VERS_SLOT_S)
				| (epb->epb_adapter << EP_VERS_ADAP_S)
				| (epb->epb_vers & EP_VERS_DATE_M));

			ether_attach(&ep->ep_eif, ep_name, unit+i,
				     (caddr_t)ep, &epops, &eaddr,
				     INV_ETHER_EP, state);
			add_to_inventory(INV_NETWORK, INV_NET_ETHER,
					 INV_ETHER_EP, unit+i, state);
			rc = io4_hwgraph_lookup(slot, &io4vhdl);
			if (rc == GRAPH_SUCCESS) {
				char loc_str[8];
				char alias_name[8];
				sprintf(loc_str, "%s/%d", EDGE_LBL_EP,
					unit + i);
				sprintf(alias_name, "%s%d", EDGE_LBL_EP,
					unit + i);
				rc = if_hwgraph_add(io4vhdl, loc_str, "if_ep",
					alias_name, &epvhdl);
			}

			ep->ep_if.if_snd.ifq_maxlen = MAX_JOBS-1;

			/* Only support admission control for now */
			ps_params.bandwidth = 10000000;
			ps_params.txfree = MAX_JOBS-1;
			ps_params.txfree_func = ep_txfree_len;
			ps_params.state_func = ep_setstate;
			ps_params.flags = 0;
			(void)ps_init(&ep->ep_if, &ps_params);
#ifdef EP_DEBUG
			ep->ep_if.if_flags |= IFF_DEBUG;
#endif
		}
		epb_reset(epb);		/* reset the board */

		unit += EP_PORTS_OCT;

		if (epb->epb_state & EPB_ST_BAD_DANG)
			printf(PF_L2 "has rev.%c DANG chip\n",
			       PF_P2(epb),'A'-1+dang_vers);
		if (SHOWCONFIG)
			printf(PF_L2 "present\n", PF_P2(epb));
	}

	splx(s);
	if (unit == 0)
		printf("ep0: missing\n");
}

#endif /* EVEREST */
