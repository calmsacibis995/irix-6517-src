/*
 * Everest IO4 EPC SEEQ/EDLC ethernet driver
 */
#ident "$Revision: 1.74 $"

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * This driver is 99.44% straightforward.  KNOW and UNDERSTAND the
 * EPC, EDLC and LXT901 and the IO4 workaround PALS before even dreaming
 * of "fixing" ee_carefully_bump_rtop().
 *
 * ANY modification to this driver and/or the IO4/EPC/EDLC/LXT901 h/w
 * should be tested VERY CAREFULLY and THOROUGHLY.  For example, put
 * the Everest on an isolated net and use ping -l from an IP17, IP12,
 * and at least one more machine for 24 to 48 hours before considering
 * your modification complete and correct.  1 or 2 hours IS NOT ENOUGH!
 *
 * ANY modification.
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * Stuff to do yet:
 *   XXX tune TBUFS, RBUFS, intr hold-off, intr poll nloops
 */

#define	EE_METER	/* Gather EE xmit/recv Q statistics */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/reg.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/invent.h>
#include <sys/immu.h>			/* VM_DIRECT */
#include <sys/param.h>			/* DELAY() */
#include <sys/cmn_err.h>
#include <net/soioctl.h>
#include <ether.h>
#include <seeq.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/debug.h>			/* ASSERT() */
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <net/rsvp.h>

static int lmap[8] = { 2, 4, 8, 16, 32, 64, 128, 256 };	/* for EPC_LIMTON */

int if_eedevflag = D_MP;

#define	TBUFS	EPC_256BUFS	/* XXX master.d or tuneable??? */
#define	RBUFS	EPC_256BUFS

#ifdef DEBUG
extern int kdbx_on;
#endif

#define	EPC_DEBUG	1
typedef struct ee_info {
	struct seeqif	ei_sif;		/* etherif is 0x270 bytes */

	int		ei_rbufs;	/* # of receive bufs (RLIMIT) */
	caddr_t		ei_rbase;	/* base addr (k0seg) of rbufs */
	int		ei_orindex;	/* prev RINDEX */
	__psunsigned_t	ei_epcbase;	/* base of EPC register set */

	int		ei_tbufs;	/* # of transmit bufs (TLIMIT) */
	caddr_t		ei_tbase;	/* base addr (k0seg) of tbufs */
	int		ei_nttop;	/* how far to push TTOP */
	int		ei_ottop;	/* S/W Copy of TTOP */
	int		ei_otindex;	/* prev TINDEX */

	int		ei_resets;	/* count of prevented rcv hangs */

	int		ei_num;		/* logical interface number */
	int		ei_adap;	/* Adapter no of this entry */
	int		ei_inuse;	/* Already configured 	*/
	unsigned	ei_ototcol;	/* last value of (h/w) coll. counter */
	unsigned	ei_olatecol;	/* last value of late coll. counter */
	unsigned	ei_oearlycol;	/* last value of early coll. counter */
	unsigned	ei_accumlate;	/* Total number of real late colls */

} eeinfo_t;

/* Static space for all possible ethernet interfaces */
static eeinfo_t  EEInfo[EV_MAX_IO4S];

#define	ei_eif		ei_sif.sif_eif
#define	ei_ac		ei_eif.eif_arpcom
#define	ei_if		ei_ac.ac_if
#define	ei_addr		ei_ac.ac_enaddr
#define	ei_rawif	ei_eif.eif_rawif

#ifdef	EE_METER
static uint ee_tmeter[256];
static uint ee_pmeter[256];
#endif
#define DOG   (2*IFNET_SLOWHZ)        /* watchdog duration in seconds */

extern int epc_xadap2slot(int, int*, int*);	/* imported */

/*
 * Since EV_MAX_IO4S is 8, we only use a char.
 */
static char ps_enabled;
#define PS_ENABLED(ei)  (ps_enabled & (1 << (ei)->ei_num))

void if_eeintr(eframe_t *, void *);
static void ee_reset(struct etherif *eif);
static void ee_poll(eeinfo_t*);
static void ee_carefully_bump_rtop(eeinfo_t*, int);
static void ee_reset_edlc(eeinfo_t *ei);
static void ee_gather_tstat(eeinfo_t*);
static void ee_setup_bufs(eeinfo_t*);
static void ee_nomem(eeinfo_t*);
static void ee_arm_intr(eeinfo_t*);
static void ee_kick_tdma(eeinfo_t*);
static void ee_set_loopback(eeinfo_t *);
static void ee_clear_loopback(eeinfo_t *);

/* WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING */
/*
 * XXX SEQ_RC_OFLOW works but has not had the same stress testing
 * XXX as the interrupts selected
 */
#define	EE_RCMD	(SEQ_RC_INTGOOD | SEQ_RC_INTEND | \
		SEQ_RC_INTSHORT | SEQ_RC_INTDRBL | SEQ_RC_INTCRC)

/* WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING */

/* Debug printfs use epc_printf */
#ifdef	EPC_DEBUG
#define	epc_printf	printf
#else
#define	epc_printf
#endif

u_long
ee_reconnect(struct etherif *eif, u_long rcmd)
{
	eeinfo_t *ei = (eeinfo_t*) eif->eif_private;

	return(EPC_SETW(EPC_RCMD, rcmd));
}
	

u_long
ee_disconnect(struct etherif *eif)
{
	eeinfo_t *ei = (eeinfo_t*) eif->eif_private;
	unsigned long	oldrcmd;
	extern void evintr_stray (eframe_t* ep, void *arg);

	oldrcmd = EPC_GETW(EPC_RCMD);
	EPC_SETW(EPC_RCMD, 0);	/* be absolutely sure we're not receiving */
	evintr_connect((evreg_t *)(ei->ei_epcbase + EPC_IIDENET),
			EVINTR_LEVEL_EPC_ENET + ei->ei_num,
			SPLDEV,
			EVINTR_DEST_EPC_ENET,
			evintr_stray,
			(void *)(__psint_t)ei->ei_num);
	return(oldrcmd);
}

u_long
ee_get_tbase(struct etherif *eif)
{
	eeinfo_t *ei = (eeinfo_t*) eif->eif_private;

	return(EPC_GETW(EPC_TBASELO));
}

u_long
ee_get_rbase(struct etherif *eif)
{
	eeinfo_t *ei = (eeinfo_t*) eif->eif_private;

	return(EPC_GETW(EPC_RBASELO));
}


/* for kdbx */
int ee_initialized = 0;

/* etherifops init() entry point -- called once at startup */
static int
ee_init(struct etherif *eif, int flags)
{
	int i;
	u_int match;
	eeinfo_t *ei = (eeinfo_t*) eif->eif_private;

	ASSERT(IFNET_ISLOCKED(&ei->ei_if) || kdbx_on);	
	if (ei->ei_rbufs == -1 || ei->ei_tbufs == -1)
		return ENOMEM;

	EPC_SETW(EPC_RCMD, 0);	/* be absolutely sure we're not receiving */

	/* program intr vector -- if_eeintr() */

	evintr_connect((evreg_t *)(ei->ei_epcbase + EPC_IIDENET),
			EVINTR_LEVEL_EPC_ENET + ei->ei_num,
			SPLDEV,
			EVINTR_DEST_EPC_ENET,
			if_eeintr,
			(void *)(__psint_t)ei->ei_num);

	/* 
	 * snapshot h/w state.  EPC transmit and receive DMA engines can
	 * NOT be reset.  We take the [TR]INDEX values as they stand.
	 * Not a prob unless the PROM decides to use more buffers
	 * than the kernel.
	 */
	while (EPC_GETW(EPC_TINDEX) != EPC_GETW(EPC_TTOP)) 
		;	/* drain out tmit DMA's */
			/* XXX make sure we're not stuck -- collisions, etc */
	ei->ei_ottop = ei->ei_nttop = ei->ei_otindex = EPC_GETW(EPC_TINDEX);
	ei->ei_olatecol  = EPC_GETW(EPC_LATE_COL)  & EPC_LATE_COL_MASK;
	ei->ei_ototcol = EPC_GETW(EPC_TOT_COL) & EPC_TOT_COL_MASK;
	ei->ei_oearlycol = EPC_GETW(EPC_EARLY_COL) & EPC_EARLY_COL_MASK;
	ei->ei_accumlate = 0;
	
	/*	- set enet addr */	
#define	EV_PAD	8
	for (i = 0; i < 6; i++)
		EPC_SETW(EPC_EADDR_BASE + EV_PAD * i, ei->ei_addr[i]);

	/* 	- set transmit condition "intr" */
#define	ENET_TCMD \
	( SEQ_XC_INTGOOD | SEQ_XC_INT16TRY | SEQ_XC_INTCOLL | SEQ_XC_INTUFLOW )

	EPC_SETW(EPC_TCMD, ENET_TCMD);

	/* start EPC receive dma engine -- open all (-1) receive buffers */
	/* start EPC dma _before_ EDLC recv'er */
	ei->ei_orindex = EPC_GETW(EPC_RINDEX); /* also resets rcv intr. timer */
	EPC_SETW(EPC_RTOP, EPC_RBN_DECR(ei->ei_orindex));

	/* start EDLC receiver */
	/* Do _NOT_ permit rcv interrupts if RINDEX == RTOP! */
	ASSERT(EPC_GETW(EPC_RINDEX) != EPC_GETW(EPC_RTOP));
	if (flags & IFF_PROMISC)
		match = SEQ_RC_RALL;
	else if (flags & (IFF_ALLMULTI | IFF_FILTMULTI))
		match = SEQ_RC_RSMB;
	else
		match = SEQ_RC_RSB;

	/* First, set the control of SEEQ register to drop packets < 14bytes */
#define	EPCSEEQ_CTL	(EPC_EADDR_BASE + (EV_PAD*3))

	EPC_SETW(EPC_TCMD, ENET_TCMD|SEQ_XC_REGBANK2); /* Point to reg bank 2 */
	EPC_SETW(EPCSEEQ_CTL, SEQ_CTL_SHORT);
	EPC_SETW(EPC_TCMD, ENET_TCMD);	/* Reset to SEEQ regbank 0 */

#undef	EPCSEEQ_CTL
#undef	ENET_TCMD

	EPC_SETW(EPC_RCMD, EE_RCMD | match);

	/* program intr hold-off timer duration */
	EPC_SETENETHOLD(EPC_ENETHOLD_0_8);

	ee_arm_intr(ei);

	ei->ei_if.if_timer = DOG;	/* turn on watchdog timer */

	ee_initialized = 1;
	return 0;
}

void
ee_setparams(struct etherif *eif, int flags)
{
	u_int match;
	eeinfo_t *ei = (eeinfo_t*) eif->eif_private;

	if (flags & IFF_PROMISC)
		match = SEQ_RC_RALL;
	else if (flags & (IFF_ALLMULTI | IFF_FILTMULTI))
		match = SEQ_RC_RSMB;
	else
		match = SEQ_RC_RSB;
	EPC_SETW(EPC_RCMD, EE_RCMD | match);
}



/* XXX what is higher level s/w expect a "reset" to do? (EPC can't be reset) */
/* etherifops reset() entry point */
static void
ee_reset(struct etherif *eif)
{
	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)) || kdbx_on);
	eiftoifp(eif)->if_timer = 0;	/* turn off watchdog */
}

/* h/w reset of EDLC chip */
static void
ee_reset_edlc(eeinfo_t *ei)
{
	/* reset EDLC -- the EPC cannot be reset! (except by sclr) */
	EPC_SETW(EPC_PRSTSET, EPC_PRST_EDLC);
	EPC_GETW(EPC_PRST);	/* sync before starting delay */
	ASSERT(EPC_GETW(EPC_PRST) & EPC_PRST_EDLC);
	DELAY(10);	/* assert reset for at least 10us */
	EPC_SETW(EPC_PRSTCLR, EPC_PRST_EDLC);
	EPC_GETW(EPC_PRST);	/* sync before continuing */
	ASSERT(!(EPC_GETW(EPC_PRST) & EPC_PRST_EDLC));
}



/* etherifops watchdog() entry point */
/* ARGSUSED */
static void
ee_watchdog(struct ifnet *ifp)
{
	eeinfo_t *ei = (eeinfo_t*) ifptoeif(ifp)->eif_private;

	ASSERT(IFNET_ISLOCKED(ifp));

	ee_poll(ei);
	ifp->if_timer = DOG;
}


/*
 * Ethernet interface interrupt.
 */
/* ARGSUSED */
void
if_eeintr(eframe_t *ep, void *arg)
{
	int npoll = 7; 
	eeinfo_t *ei;
	int devnm = (__psint_t)arg;

	ASSERT(devnm >= 0 && devnm < EV_MAX_IO4S);
	ei = &(EEInfo[devnm]);
	IFNET_LOCK(&ei->ei_if);

	ee_arm_intr(ei);	

	/*
	 * spin because intr beats DMA, 7 because there
	 * can be 5 DMA requests outstanding on the ibus
	 * plus 2 for good measure 
	 */
	while (npoll--)
		ee_poll(ei);

	/* RSVP.  Let packet scheduler know we saw an interrupt. */
	if (PS_ENABLED(ei)) {
		int tindex, ttop, qlen;
		tindex = EPC_GETW(EPC_TINDEX);
		ttop = EPC_GETW(EPC_TTOP);
		qlen = (ttop < tindex) ? (tindex - ttop) :
			(ei->ei_tbufs - (ttop - tindex));
		ps_txq_stat(eiftoifp(&ei->ei_eif), qlen);
	}

	IFNET_UNLOCK(&ei->ei_if);
}

static void
ee_arm_intr(eeinfo_t *ei)
{
	/* arm timer */
	EPC_SETW(EPC_PRSTCLR, EPC_ENETHOLD_ARM);	/* inverted logic */
	EPC_SETW(EPC_PRSTSET, EPC_ENETHOLD_ARM);

	/* arm EPC (IO4 actually) enet intrs */
	EPC_SETW(EPC_IMSET, EPC_INTR_ENET);
}

static void
ee_poll(eeinfo_t *ei)
{
	int didwork = 0;

	/*
	 * for each new packet
	 *	examine rcv status and length
	 *	copy rbuf to mbuf
	 *	call ether_input
	 */
#ifdef	EE_METER
	{
	int rindex = EPC_GETW(EPC_RINDEX);
	int diff = rindex >= ei->ei_orindex 
		? rindex - ei->ei_orindex
		: rindex + ei->ei_rbufs - ei->ei_orindex;
	if (diff > 0 && diff < EPC_LIMTON(RBUFS))
		ee_pmeter[diff]++;
	}
#endif	/* EE_METER */
	while (EPC_GETW(EPC_RINDEX) != ei->ei_orindex) {
		int rstat, rlen;
		caddr_t bp;
		struct mbuf *m0;
		int snoopflags = 0;

		didwork++;

		bp = EPC_ENET_RBP(ei->ei_orindex);
		rstat = *(u_int*)(bp + EPC_ENET_STAT);
		rlen = rstat & EPC_ENET_LENMASK;
		rstat = (rstat >> EPC_ENET_STSHFT) & EPC_ENET_STMASK;

		if (!(rstat & SEQ_RS_GOOD)){
			snoopflags = SN_ERROR;

			if (rstat & SEQ_RS_OFLOW)	/* XXX reset EDLC? */
				snoopflags |= SNERR_OVERFLOW;

			if (rstat & SEQ_RS_SHORT ||
		    	rlen < ETHERMIN + sizeof(struct ether_header)){
				snoopflags |= SN_ERROR|SNERR_TOOSMALL;
				if(rlen < sizeof(struct ether_header))
					rlen = sizeof(struct ether_header);
			}

			if (rstat & SEQ_RS_DRBL)
				snoopflags |= SNERR_FRAME;

			if (rstat & SEQ_RS_CRC)
				snoopflags |= SNERR_CHECKSUM;

			if (rlen > ETHERMTU + sizeof(struct ether_header)){
				snoopflags |= SN_ERROR|SNERR_TOOBIG;
				rlen = ETHERMTU + sizeof(struct ether_header);
			}

		}

		/* record the next rbuf to visit */
		ei->ei_orindex = EPC_RBN_INCR(ei->ei_orindex);

		/* If this interface has been turned off toss packet */
		if (iff_dead(ei->ei_if.if_flags)) {
			ether_stop(&ei->ei_eif, ei->ei_if.if_flags);
			continue;
		}

		ei->ei_if.if_ipackets++;
		ei->ei_if.if_ibytes += rlen;

		m0 = m_vget(M_DONTWAIT,
			    rlen + sizeof(struct etherbufhead) -
				sizeof(struct ether_header),
			    MT_DATA);
		if (m0 == 0) {
			if (ei->ei_if.if_flags & IFF_DEBUG)
				printf("et%d: out of mbufs for rcv packet\n",
					ei->ei_num);
			return;
		}
		ASSERT(m0->m_next == 0); /* XXX someone racing us? */
	
		IF_INITHEADER(mtod(m0, struct etherbufhead*),
			&ei->ei_if,
			sizeof(struct etherbufhead));

		bcopy(bp + EPC_ENET_DATA,
		      &mtod(m0, struct etherbufhead *)->ebh_ether,
		      rlen);

		ether_input(&ei->ei_eif, snoopflags, m0);

	}

	if (didwork)
		ee_carefully_bump_rtop(ei, EPC_RBN_DECR(ei->ei_orindex));

	/* while we're in town... process any work on the transmit side */
	ee_gather_tstat(ei);
	ee_kick_tdma(ei);
}




/*
 * Some facts about transmit buffers:
 *
 * [nttop, otindex)     - free
 * [otindex, TINDEX)    - dma complete
 * [TINDEX, nttop)      - awaiting dma
 *
 * if (EPC_GETW(EPC_TTOP) != EPC_GETW(EPC_TINDEX))
 *						- transmit DMA active
 * if (ei->ei_nttop == ei->ei_otindex &&
				 EPC_GETW(EPC_TTOP) != EPC_GETW(EPC_TINDEX))
 *						- all tbufs owned by h/w
 *
 */

/* "owner" of nttop */
/* XXX snoop support ??? ee hears itself when promiscuous... */
static int
ee_transmit(struct etherif *eif, struct etheraddr *edst,
		struct etheraddr *esrc, u_short type, struct mbuf *m0)
{
	int tthis, len;
	caddr_t bp;
	struct ether_header *eh;
	eeinfo_t *ei = (eeinfo_t*) eif->eif_private;

	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)) || kdbx_on);
#ifdef	EE_METER
	{
	int tindex = EPC_GETW(EPC_TINDEX), ttop = EPC_GETW(EPC_TTOP);
	int diff = ttop < tindex ? ttop + ei->ei_tbufs - tindex : ttop - tindex;
	/* Remove this line */
	if ((diff > 0) && (diff < EPC_LIMTON(TBUFS)))
		ee_tmeter[diff]++;
	}
#endif	/* EE_METER */


	/* Wait for possible rcv unhang to complete -- ee_watchdog() */
	ee_gather_tstat(ei);
	
	/* If ttop+1 equal to tindex,all TBUFS of EPC are used up.  */
	if(EPC_TBN_INCR(ei->ei_nttop) == ei->ei_otindex  &&
				 EPC_GETW(EPC_TTOP) != EPC_GETW(EPC_TINDEX)){
		ei->ei_if.if_snd.ifq_drops++;
		m_freem(m0);
#ifdef	DEBUG
		printf("et%d: dropped packet. No EPC transmit buffer\n",
					ei->ei_num);
#endif
		return ENOBUFS;
	}

	tthis = ei->ei_nttop;
	ei->ei_nttop = EPC_TBN_INCR(ei->ei_nttop);

	bp = EPC_ENET_TBP(tthis);

	/* Copy from the mbuf chain */
	len = m_datacopy(m0, 0, ETHERMTU,
			bp + EPC_ENET_DATA + sizeof(struct ether_header));

	if (len < ETHERMIN)
		len = ETHERMIN;

	/* Fill in the enet header */
	eh = (struct ether_header *)(bp + EPC_ENET_DATA);
	bcopy((caddr_t)edst, eh->ether_dhost, sizeof(*edst));
	bcopy((caddr_t)esrc, eh->ether_shost, sizeof(*esrc));
	eh->ether_type = type;	/* already in network byte order */

	/* Send info to snoopers */
	if ((RAWIF_SNOOPING(&ei->ei_rawif))
	    && snoop_match(&ei->ei_rawif, (caddr_t)eh, len))
		ether_selfsnoop(&ei->ei_eif, eh, m0, 0, len);

	/* Add header length to the data to be sent */
	len += sizeof(struct ether_header);

	/* Write in packet len for EPC transmit DMA engine */
	bp[0] = len & 0xff;
	bp[1] = (len >> 8) & 0xff;

	ei->ei_if.if_obytes += len;

	ee_kick_tdma(ei);
	
	m_freem(m0);

	return 0;
}

/* "owner" of TTOP -- call anytime */
static void
ee_kick_tdma(eeinfo_t *ei)
{
	/* Following logic is too expensive. It sets the EPC_TTOP if it's
	 * not same as ei_nttop OR (which implies it's same as nttop
	 * and TINDEX is not same as ei_nttop. Instead, we can keep a
	 * copy of the value set to TTOP (ei_ottop), and use that to
	 * compare. Each PIO read on Everest could cost upto a usec.
	 */
#if	0
	if (EPC_GETW(EPC_TTOP) == ei->ei_nttop &&
					 EPC_GETW(EPC_TINDEX) == ei->ei_nttop)
		return;

	EPC_SETW(EPC_TTOP, ei->ei_nttop);
#else
	if (ei->ei_ottop != ei->ei_nttop){
		EPC_SETW(EPC_TTOP, ei->ei_nttop);;
		ei->ei_ottop = ei->ei_nttop;
	}
#endif
}

/* "owner" of otindex -- call anytime */
static void
ee_gather_tstat(eeinfo_t *ei)
{
	/* compute accumulated collision counts from delta of h/w regs */
	unsigned late  = EPC_GETW(EPC_LATE_COL) & EPC_LATE_COL_MASK;
	unsigned early = EPC_GETW(EPC_EARLY_COL) & EPC_EARLY_COL_MASK;
	unsigned tot   = EPC_GETW(EPC_TOT_COL) & EPC_TOT_COL_MASK;
	unsigned deltaearly, deltalate, deltatot;

#define	DIFF(old,new,mask)	(new < old ? mask - old + new + 1 : new - old)

	/* Calculate the deltas from the last attempt */
	deltaearly = DIFF(ei->ei_oearlycol, early, EPC_EARLY_COL_MASK);
	deltalate  = DIFF(ei->ei_olatecol,  late,  EPC_LATE_COL_MASK);
	deltatot   = DIFF(ei->ei_ototcol,    tot,   EPC_TOT_COL_MASK);

	/* update s/w collision count -- for netstat, etc*/
	ei->ei_if.if_collisions += deltaearly;

	if (deltalate && (deltaearly < deltatot)) {
		ei->ei_accumlate += (deltatot - deltaearly);
		if ((ei->ei_accumlate > 0) && ((ei->ei_accumlate % 100) == 0))
			printf("et%d: late collision\n", ei->ei_num);
	}

	/* Update the old values */
        ei->ei_oearlycol = early;
	ei->ei_olatecol  = late;
	ei->ei_ototcol   = tot;
 
	/* rip through transmitted tbufs looking for errors */
	while (EPC_GETW(EPC_TINDEX) != ei->ei_otindex) {
		caddr_t bp = EPC_ENET_TBP(ei->ei_otindex);
		uint tstat = *(uint*)(bp + EPC_ENET_STAT);
		tstat = (tstat >> EPC_ENET_STSHFT) & EPC_ENET_STMASK;
		if (tstat & SEQ_XS_UFLOW)
			ei->ei_if.if_oerrors++;
#ifdef	DEBUG
		*(uint*)(bp + EPC_ENET_STAT) = 0x1fabbabe;
#endif
		/* free the tbuf */
		ei->ei_otindex = EPC_TBN_INCR(ei->ei_otindex);
	}
}

/* etherifops ioctl() entry point */
/* ARGSUSED */
static int
ee_ioctl(struct etherif *eif, int cmd, void *data)
{
	struct seeqif *sif = (struct seeqif *)eif;

	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)));
	switch (cmd) {
	case SIOCADDMULTI:
		if (sif->sif_nmulti == 0) {
			eiftoifp(eif)->if_flags |= IFF_FILTMULTI;
			if (!(eiftoifp(eif)->if_flags
			    & (IFF_ALLMULTI | IFF_PROMISC)))
				ee_setparams(eif, eiftoifp(eif)->if_flags);
		}
		sif->sif_nmulti++;
		break;

	case SIOCDELMULTI:
		--sif->sif_nmulti;
		if (sif->sif_nmulti == 0) {
			eiftoifp(eif)->if_flags &= ~IFF_FILTMULTI;
			if (!(eiftoifp(eif)->if_flags
			    & (IFF_ALLMULTI | IFF_PROMISC)))
				ee_setparams(eif, eiftoifp(eif)->if_flags);
		}
		break;

	default:
		return EINVAL;
	}
	return 0;
}

/*
 * Called by packet scheduling functions to determine length of driver queue.
 */
static int
ee_txfree_len(struct ifnet *ifp)
{
	eeinfo_t *ei;
	int tindex, ttop;

	ei = (eeinfo_t *)ifptoeif(ifp)->eif_private;
	tindex = EPC_GETW(EPC_TINDEX);
	ttop = EPC_GETW(EPC_TTOP);
	return ((ttop < tindex) ? (tindex - ttop) :
	  (ei->ei_tbufs - (ttop - tindex)));
}

/*
 * Called by packet scheduling functions to notify driver about queueing state.
 * If setting is 1 then there are queues and driver should try to minimize
 * delay (ie take intr per packet).  If 0 then driver can optimize for
 * throughput (ie. take as few intr as possible).
 */
static void
ee_setstate(struct ifnet *ifp, int setting)
{
	eeinfo_t *ei;

	ei = (eeinfo_t *)ifptoeif(ifp)->eif_private;
	ASSERT(ei->ei_num < sizeof(ps_enabled)*8);
	if (setting)
		ps_enabled |= (1 << ei->ei_num);
	else
		ps_enabled &= ~(1 << ei->ei_num);
}

static struct etherifops eeops =
		{ ee_init, ee_reset, ee_watchdog, ee_transmit, ee_ioctl };

/*
 * if_eeedtinit
 *	called once by edtinit.  This routine must probe for all working
 *	EPC ethernet devices and set up the necessary data structures.
 *	It then calls ether_attach for each EPC ethernet.
 */

#define UNUSED 255
 
/* ARGSUSED */
void
if_eeedtinit(struct edt *edtp)
{
	int slot, adap, i;	/* Index variables */
	int	epcno;
	eeinfo_t *ei;		/* Pointer to current eeinfo structure */
	struct etheraddr ea;	/* Ethernet address of current enet */
	ibus_t	*iadap;		/* Pointer to relavent ibus adap structure */
	int	inum;
	vertex_hdl_t io4vhdl;
	vertex_hdl_t eevhdl;
	graph_error_t rc;
	unsigned char map_usage[EV_MAX_SLOTS];
	struct ps_parms ps_params;

	for (i = 0; i < EV_MAX_SLOTS; i++)
		map_usage[i] = UNUSED;

	for (inum=0; inum < ibus_numadaps; inum++){

		if (ibus_adapters[inum].ibus_module != EPC_ETHER)
			continue;

		iadap = &ibus_adapters[inum];
		if (epc_xadap2slot(iadap->ibus_adap, &slot, &adap)){
			printf(
			"epcether: could not map unit %d to slot %d ioa %d\n",
			iadap->ibus_unit, slot, adap);
		    	continue;
		}

		/* Make sure that the ethernet port isn't already configured */
		if (map_usage[slot] != UNUSED) {
			printf("epcether: slot %d ioa %d already configured "
			       "as unit %d\n", slot, adap, map_usage[slot]);
			continue;
		} else {
			map_usage[slot] = iadap->ibus_unit;
		}

		/* Find the EPC array matching given adapter */
		for (epcno=0; epcno < numepcs; epcno++){
			if((epc_slot(epcno)==slot)&&(epc_adap(epcno) == adap))
			    break;
		}
		if (epcno >= numepcs){
			printf("et%d not configured. No EPC at adap %d\n",
				iadap->ibus_unit, iadap->ibus_adap);
			continue;
		}

#ifdef	CHECK_ET0
		/* Following code needs to be enabled if it is essential
		 * to allow only Master EPC to use et0, and no one else.
		 * For now, No restrictions
		 */

#define	EMSG1 "EPC in Master IO4 slot %d should be unit 0(%d now)."
#define	EMSG2 "Unit 0 reserved for Master IO4 slot:%d. Used with IO4 slot:%d"

		/* Check if Primary interface is on Master Io4 or not */
		if ((epcno == 0) && (iadap->ibus_unit != 0)){
			cmn_err_tag(262,CE_WARN, EMSG1, slot, iadap->ibus_unit);
			cmn_err_tag(263,CE_WARN,"Reconfigure the Kernel");
			return;
		}

		/* Check if Primary Interface is used for non-master IO4 */
		if ((iadap->ibus_unit == 0) && (epcno != 0)){
			cmn_err_tag(264,CE_WARN, EMSG2, epc_slot(0), slot);
			cmn_err_tag(265,CE_WARN,"Reconfigure the kernel");
			return;
		}
#endif	/* CHECK_ET0 */


#define	EMSG3 "if_ee: adapter %d using unit %d, already in use by adapter %d"
#define	EMSG4 "if_ee: adapter %d not configured"

		ei = &EEInfo[iadap->ibus_unit];

		if (ei->ei_inuse){
			cmn_err_tag(266,CE_WARN, EMSG3, iadap->ibus_adap,
				iadap->ibus_unit, ei->ei_adap);
			cmn_err_tag(267,CE_WARN, EMSG4, iadap->ibus_adap);
			continue;
		}

		bzero(ei, sizeof(eeinfo_t));
		ei->ei_num = iadap->ibus_unit;
		ei->ei_adap = iadap->ibus_adap;
		ei->ei_inuse = 1;
	
		ei->ei_epcbase = epc_swin(epcno);

#define PAD 16
		for (i = 0; i < ETHERADDRLEN; i++)
		    ea.ea_vec[i] = EPC_GETW(EPC_ENETPROM + (5 - i) * PAD);
		if (ea.ea_vec[0] != 0x08){
		    cmn_err_tag(268,CE_WARN,"et%d: bad enet addr (Check enet prom)\n", 
				ei->ei_num);
		}

		/* By convention, et0 is always on the master IO4 */
		if (iadap->ibus_unit != 0)
			ee_reset_edlc(ei); /* Done only for non-master EPCs */

		printf("Configuring EPC in IO4 slot %d padap %d as et%d\n", 
			slot, adap, ei->ei_num);

		ether_attach(&(ei->ei_eif), "et", ei->ei_num, (caddr_t)ei, 
				 &eeops, &ea, INV_ETHER_EE, slot);
		add_to_inventory(INV_NETWORK, INV_NET_ETHER, INV_ETHER_EE, ei->ei_num, slot);

		rc = io4_hwgraph_lookup(slot, &io4vhdl);
		if (rc == GRAPH_SUCCESS) {
			char alias_name[5];
			sprintf(alias_name, "%s%d", EDGE_LBL_ET, ei->ei_num);
			(void) if_hwgraph_add(io4vhdl, EDGE_LBL_ET, "if_ee", 
				alias_name, &eevhdl);
		}
		
		/* RSVP.  Support admission control and packet scheduling. */
                ps_params.bandwidth = 10000000;
                ps_params.txfree = EPC_LIMTON(TBUFS);
                ps_params.txfree_func = ee_txfree_len;
                ps_params.state_func = ee_setstate;
                ps_params.flags = 0;
                (void)ps_init(eiftoifp(&ei->ei_eif), &ps_params);

		/* allocate bufs now before mem is too frag'ed */
		ee_setup_bufs(ei);

	} 
}

#define EBUFSZ	2048
#define	ETOP(n)	(n/(NBPP/EBUFSZ))	/* bufs in a page */
static void
ee_setup_bufs(eeinfo_t *ei)
{
	pgno_t tmp;

	/* program EPC enet dma engines */
	EPC_SETW(EPC_TLIMIT, TBUFS);			/* no. transmit bufs */
	ei->ei_tbufs = EPC_LIMTON(TBUFS);

	EPC_SETW(EPC_RLIMIT, RBUFS);			/* no. receive bufs */
	ei->ei_rbufs = EPC_LIMTON(RBUFS);

	ASSERT(EPC_GETW(EPC_RINDEX) < ei->ei_rbufs);

	/* allocate k0seg memory for transmit and receive buffers */
	if ((tmp=contig_memalloc(ETOP(ei->ei_tbufs), ETOP(ei->ei_tbufs), VM_DIRECT)) == NULL) {
		ee_nomem(ei);
		return;
	}

	ei->ei_tbase = small_pfntova_K0(tmp);
	EPC_SETW(EPC_TBASEHI, 0); /* assumes k0seg! */
	tmp = svirtophys(ei->ei_tbase);
	EPC_SETW(EPC_TBASELO, tmp);

	if ((tmp=contig_memalloc(ETOP(ei->ei_rbufs), ETOP(ei->ei_rbufs), VM_DIRECT)) == NULL) {
		ee_nomem(ei);
		return;
	}

	ei->ei_rbase = small_pfntova_K0(tmp);
	EPC_SETW(EPC_RBASEHI, 0); /* assumes k0seg! */
	tmp = svirtophys(ei->ei_rbase);
	EPC_SETW(EPC_RBASELO, tmp);
}

static void
ee_nomem(eeinfo_t *ei)
{
	printf("ee: failed to allocate buffers... marking interface down\n");
	ei->ei_tbufs = -1;
	ei->ei_rbufs = -1;
}

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * KNOW the IO4, EPC, EDLC, LXT901 and workarea PALs before
 * touching _ANYTHING_ in the following code.  There are tight corner
 * cases which are not seen under normal or even heavy stress network
 * conditions... WHICH DEADLOCK THE EPC!  Only a system reset will
 * clear the deadlock.  The following code detects and _prevents_
 * the deadlock conditions.
 *
 * The EPC will deadlock internally if the EDLC overflows due to
 * s/w getting behind in servicing rcv'ed buffers.  This routine is
 * called whenever we notice RINDEX approaching RTOP.
 *
 * If there was no deadlock danger in the EPC this routine could
 * be replaced by: EPC_SETW(EPC_RTOP, rbuf);
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 */

static void
ee_carefully_bump_rtop(eeinfo_t *ei, int rbuf)
{
	int rtop, rindex, i, match;

	ASSERT(IFNET_ISLOCKED(&(ei)->ei_if));
	/*
	 * Prevent receiver overflow hang.  If RINDEX catches RTOP the EPC
	 * will hang if we then bump RTOP while the EDLC is asserting
	 * any recv interrupt to the EPC.  Before bumping RTOP make sure
	 * RINDEX hasn't caught up (or won't before we bump RTOP).
	 */

	rindex = EPC_GETW(EPC_RINDEX);
	rtop = EPC_GETW(EPC_RTOP);

#define	LEADPKTS 64 /* if RINDEX far enough away we're safe in bumping RTOP */
	if (rtop < rindex)
		rtop += ei->ei_rbufs;	/* keep subtraction positive */
	if (rtop - rindex > LEADPKTS) {
		EPC_SETW(EPC_RTOP, rbuf);
		return;
	}

	/* Okay, we're behind: initiate hang avoidance sequence captain! */
	
	/*
	 * Step 0	Transmit side must be quiescent.  Hold off ee_transmit
	 */

	/*
	 * Step 1	EDLC must be isolated from the wire when setting
	 *		its registers: set loopback mode on the LXT901.
	 */
	ee_set_loopback(ei);

	/*
	 * Step 2	Shut off transmitter by turning off the EPC
 	 *		transmit DMA engine.  Yes, this tosses packets
	 * 		already queued for output... we're here to fix
	 *		the rcv side. XXX  (We _could_ spin waiting for
	 *		TINDEX to reach TTOP if we properly bullet-proof
	 *		such a loop).
	 */
	EPC_SETW(EPC_TTOP, EPC_GETW(EPC_TINDEX));	/* XXX racy ? */

	/* 
	 * Step 3	Hardware reset of EDLC chip.
	 */
	ee_reset_edlc(ei);


	/*
	 * Step 4	Turn on transmit side of the world.  Snapshot state
	 *		of EPC transmit DMA engine, turn on transmit
	 *		interrupts (EDLC->EPC).
	 */
	ei->ei_nttop = ei->ei_otindex = EPC_GETW(EPC_TINDEX);
	EPC_SETW(EPC_TCMD, SEQ_XC_INTGOOD | SEQ_XC_INT16TRY | SEQ_XC_INTCOLL |
		SEQ_XC_INTUFLOW);

	/* 
	 * Step 5	Program and turn on receive side of the world. 
	 *		Reprogram station address. Note that we're still
	 *		in LXT901 loopback.. i.e., we're disconnected
	 *		from the wire and inactive.
	 */
#define	EV_PAD	8
	for (i = 0; i < 6; i++)
		EPC_SETW(EPC_EADDR_BASE + EV_PAD * i, ei->ei_addr[i]);

	if (ei->ei_if.if_flags & IFF_PROMISC)
		match = SEQ_RC_RALL;
	else if (ei->ei_if.if_flags & (IFF_ALLMULTI | IFF_FILTMULTI))
		match = SEQ_RC_RSMB;
	else
		match = SEQ_RC_RSB;
	EPC_SETW(EPC_RCMD, EE_RCMD | match);

	ei->ei_orindex = EPC_GETW(EPC_RINDEX);
	EPC_SETW(EPC_RTOP, EPC_RBN_DECR(ei->ei_orindex));
	EPC_GETW(EPC_RTOP);	/* sync up the write to h/w */

	/*
	 * Step 6	Disable loopback.  (Patch us back out to the wire).
	 */
	ee_clear_loopback(ei);

	ei->ei_resets++;
}

static void
ee_set_loopback(eeinfo_t *ei)
{
	EPC_SETB(EPC_LXT_LOOP, 1); /* this pbus "dev" must be byte-accessed */
	EPC_GETW(EPC_LXT_LOOP);	/* sync to h/w */
}

static void
ee_clear_loopback(eeinfo_t *ei)
{
	EPC_SETB(EPC_LXT_LOOP, 0);
}
