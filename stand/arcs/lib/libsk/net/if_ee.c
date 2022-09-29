#ident  "lib/libsk/net/if_ee.c:  $Revision: 1.24 $"

#if defined(EVEREST)

/*
 * if_ee.c -- ARCS driver for Everest EPC/EDLC IO4 board ethernet
 * 
 * NOTE -- no rcv hang workaround code herein (see the kernel driver)
 * XXX support non-master IO4s
 */


#include <sys/types.h>
#include <sys/sbd.h>

#include <arcs/errno.h>
#include <arcs/hinv.h>		/* COMPONENT */

#include <saioctl.h>
#include <net/socket.h>
#include <net/in.h>
#include <net/arp.h>
#include <net/mbuf.h>
#include <net/ei.h>

#include <arcs/eiob.h>

#include <sys/EVEREST/addrs.h>	/* ENETBUFS_BASE */
#include <sys/EVEREST/epc.h>
#include <net/seeq.h>
#include <libsc.h>
#include <libsk.h>

static int lmap[8] = { 2, 4, 8, 16, 32, 64, 128, 256 };

/* 
 * The EPC cannot be reset w/o resetting the whole system (ebus sclr)
 * this makes it tricky to _decrease_ R/TLIMIT in the kernel especially
 * if RTOP, TTOP are currently larger than the new limit...  Why 16?
 * I just made that up.  1 or 2 would probably work fine with bootp.
 * 16's probably the smallest value for the kernel.
 */

#ifdef NETDBX
static int TBUFS = EPC_16BUFS;
#define	RBUFS		TBUFS
#define	MAXBUFS		256
static unsigned long 	TBASE_PHYS = ENETBUFS_BASE;
static unsigned long 	RBASE_PHYS = (ENETBUFS_BASE + MAXBUFS * EPC_ENET_BUFSZ);
#else
#define	TBUFS		EPC_16BUFS
#define	RBUFS		EPC_16BUFS
#define	MAXBUFS		256
#define	TBASE_PHYS	ENETBUFS_BASE
#define	RBASE_PHYS	(ENETBUFS_BASE + MAXBUFS * EPC_ENET_BUFSZ)
#endif /* NETDBX */

struct ee_info {
	struct arpcom	ei_ac;

	int		ei_rbufs;	/* # receive bufs (RLIMIT) */
	caddr_t		ei_rbase;	/* base of rbufs */
	int		ei_orindex;	/* prev RINDEX */
	caddr_t		ei_epcbase;

	int		ei_tbufs;	/* # transmit bufs (TLIMIT) */
	caddr_t		ei_tbase;	/* base of tbufs */
	int		ei_tfree;	/* 1st open tbuf */
	int		ei_tdmawait;	/* dirty tbuf not queued for tmit */
} ee_info, *ei;

static __psint_t eedebug;

#define	ei_if	ei_ac.ac_if
#define	ei_addr	ei_ac.ac_enaddr

#define	cei(x)	((struct ether_info *)(x->DevPtr))

static int ee_output(struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst);
static void ee_poll(IOBLOCK *iob);
static int ee_ioctl(IOBLOCK *iob);
static int ee_reset(void);
static void ee_tkick(void);

#ifdef NETDBX
void
ee_config(unsigned long tbase, int ntbufs, unsigned long rbase)
{
	TBASE_PHYS = tbase;
	TBUFS = ntbufs;
	if (rbase != 0)
		RBASE_PHYS = rbase;
	else
		RBASE_PHYS = TBASE_PHYS + MAXBUFS * EPC_ENET_BUFSZ;

	printf("ee_config:: tbasephys = 0x%llx\n", TBASE_PHYS);
	printf("ee_config:: rbasephys = 0x%llx\n", RBASE_PHYS);
}
#endif /* NETDBX */


int
ee_init(void)
{
	/* init driver globals */
	ei = &ee_info;
	bzero(&ee_info, sizeof ee_info);

	EPC_SETBASE(EPC_REGION, EPC_ADAPTER);

	return 0;
}

int
ee_probe(int unit)
{
	return (unit == 0 ? 1 : -1);	/* one unit in standalone */
}

/*
 * ARCS gunk
 */

static COMPONENT eetmpl = {
	ControllerClass,		/* Class */
	NetworkController,		/* Type */
	(IDENTIFIERFLAG)(Input|Output),	/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	4,				/* IdentifierLength */
	"et0",				/* Identifier */
};

static COMPONENT eeptmpl = {
	PeripheralClass,		/* Class */
	NetworkPeripheral,		/* Type */
	(IDENTIFIERFLAG)(Input|Output),	/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV, 			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	NULL,				/* Identifier */
};

void
ee_install(COMPONENT *p)
{
	p = AddChild(p, &eetmpl, 0);
	if (p == NULL)
		cpu_acpanic("et0");
	p = AddChild(p, &eeptmpl, 0);
	if (p == NULL)
		cpu_acpanic("et0");
	RegisterDriverStrategy(p, _if_strat);
}

int
ee_open(IOBLOCK *iob)
{
	int i;
	char *cp;

	eedebug = ((__psint_t)(cp = getenv("EEDEBUG")) ? atoi(cp) : 0);

	if (ei->ei_if.if_flags & IFF_RUNNING)
		ee_close();

	cei(iob)->ei_registry = -1;
	cpu_get_eaddr(ei->ei_addr);

	if (ei->ei_addr[0] != 0x08) {
		printf("WARNING: non-SGI enet addr! (check enet prom)\n");
		return iob->ErrorNumber = EINVAL;
	}

	/* wait for any DMA's to complete -- they can't be stopped! :-) */
	while (EPC_GETW(EPC_TINDEX) != EPC_GETW(EPC_TTOP))
		;
	ei->ei_tfree = EPC_GETW(EPC_TINDEX);

	/* 	- set ethernet address */
#define	EV_PAD	8
	for (i = 0; i < 6; i++)
		EPC_SETW(EPC_EADDR_BASE + EV_PAD * i, ei->ei_addr[i]);

	/* Turn the SEEQ on... */
	/*	- notify EPC of any possible transmit condition */
	EPC_SETW(EPC_TCMD, SEQ_XC_INTGOOD | SEQ_XC_INT16TRY | SEQ_XC_INTCOLL |
		SEQ_XC_INTUFLOW);

	/*
	 *	- notify EPC of any possible receive condition
	 *	- enable reception of station addressed and broadcast frames
	 */
	EPC_SETW(EPC_RCMD, SEQ_RC_INTGOOD | SEQ_RC_INTEND | SEQ_RC_INTSHORT |
		SEQ_RC_INTDRBL | SEQ_RC_INTCRC | SEQ_RC_RSB);

	/* EPC DMA engine initialization */
	/*	- set address and number of transmit buffers */
	EPC_SETW(EPC_TBASEHI, 0);
	EPC_SETW(EPC_TBASELO, TBASE_PHYS);
	ei->ei_tbase = (caddr_t)PHYS_TO_K0(TBASE_PHYS);
	EPC_SETW(EPC_TLIMIT, TBUFS);
	ei->ei_tbufs = EPC_LIMTON(TBUFS);

	/*	- set address and number of receive buffers */
	EPC_SETW(EPC_RBASEHI, 0);
	EPC_SETW(EPC_RBASELO, RBASE_PHYS);
	ei->ei_rbase = (caddr_t)PHYS_TO_K0(RBASE_PHYS);
	EPC_SETW(EPC_RLIMIT, RBUFS);
	ei->ei_rbufs = EPC_LIMTON(RBUFS);

	/* turn _off_ self recv mode -- don't want to hear our own broadcasts */
	EPC_SETW(EPC_EDLC_SELF, 0);

	/* make sure loopback mode is off */
	EPC_SETW(EPC_LXT_LOOP, 0);

	/* 	- turn on rcv dma engine, open max # rbufs (RLIMIT - 1) */
	/* snapshot h/w state */
	ei->ei_orindex = EPC_GETW(EPC_RINDEX);
	EPC_SETW(EPC_RTOP, EPC_RBN_DECR(ei->ei_orindex));

	/* set if_ routines: output, ioctl, poll */
	ei->ei_if.if_unit = 0;
	ei->ei_if.if_mtu = ETHERMTU;
	ei->ei_if.if_output = ee_output;
	ei->ei_if.if_ioctl = ee_ioctl;
	ei->ei_if.if_poll = ee_poll;
	cei(iob)->ei_acp = &ei->ei_ac;

	ei->ei_if.if_flags = IFF_UP|IFF_RUNNING|IFF_BROADCAST;

	return ESUCCESS;
}


int
ee_close(void)
{
	EPC_SETW(EPC_RCMD, 0);/* stop listening to the EDLC */
	ei->ei_if.if_flags &= ~(IFF_UP|IFF_RUNNING|IFF_BROADCAST);
	return 0;
}

static int
ee_reset()
{
	/* XXX make sure DMA engines are off */
	/*     warning: "decrementing" R/TINDEX could hang dma engine(s) */

	/* assert reset to EDLC... */
	EPC_SETW(EPC_PRSTSET, EPC_PRST_EDLC);

	/* ...for 10us... */
	us_delay(10);

	/* ...then clear */
	EPC_SETW(EPC_PRSTCLR, EPC_PRST_EDLC);

	return 0;
}

/*ARGSUSED*/
static int
ee_output(struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst)
{
	u_char edst[6], *ep;
	struct in_addr idst;
	struct ether_header *eh;
	int type;
	int tindex;
	char *bp;
	int elen;
	int tthis;

	/*
	 * find enet addr as appropriate for address family of this packet
	 */
	switch (dst->sa_family) {
	case AF_INET:
		idst = ((struct sockaddr_in *)dst)->sin_addr;
		ep = edst;
		_arpresolve(&ei->ei_ac, &idst, ep);
		type = ETHERTYPE_IP;
		break;
	case AF_UNSPEC:
		eh = (struct ether_header *)dst->sa_data;
		ep = eh->ether_dhost;
		type = eh->ether_type;
		break;
	default:
		_m_freem(m0);
		printf("et0: no support for address family %d\n",
			(int)dst->sa_family);
		return -1;
	}

	/*
	 * Find the next available EPC transmit buffer, and advance
	 * (and properly wrap) pointer to next (potentially) open tbuf
	 */
	tthis = ei->ei_tfree;
	ei->ei_tfree = EPC_TBN_INCR(ei->ei_tfree);

	/* 	- make sure we're not getting ahead of the hardware */
	tindex = EPC_GETW(EPC_TINDEX);

	/*	- ("tindex != ttop" implies "transmit dma active") */
	if (tindex == tthis && tindex != EPC_GETW(EPC_TTOP)) {
		if (eedebug)
			printf("et0: dropped transmit packet\n");
		_m_freem(m0);
		return -1;
	}
	

	/*
	 * Copy the packet into the buffer
	 */
	bp = EPC_ENET_TBP(tthis);

	/*
	 * 	- write in packet len (1st 2 bytes of tbuf)
	 */
	elen = sizeof(struct ether_header) + m0->m_len;
	if (elen < ETHERMIN + sizeof(struct ether_header))
		elen = ETHERMIN + sizeof(struct ether_header);
	else if (elen > ETHERMTU) {
		printf("et0: transmit packet too long: %d (truncating)\n",elen);
		elen = ETHERMTU + sizeof(struct ether_header); /*XXX toss? */
	}
	bp[0] = elen & 0xff;		/* LSB */
	bp[1] = (elen >> 8) & 0xff;	/* MSB */

	/*
	 * 	- fill in the ethernet header (in the tbuf)
	 */
	eh = (struct ether_header *)(bp + EPC_ENET_DATA);
	bcopy(ep, eh->ether_dhost, sizeof(edst));
	bcopy(ei->ei_addr, eh->ether_shost, sizeof(ETHADDR));
	eh->ether_type = htons(type);

	/*
	 * 	- fill in the rest from the mbuf
	 * 	- XXX assumes m_len <= ethernet packet
	 */
	bcopy(mtod(m0, char *),
		bp + EPC_ENET_DATA + sizeof(struct ether_header), m0->m_len);
	

	/*
	 * Kick tmit dma engine.  Sample TINDEX again to get the latest
	 * h/w state.
	 */
	if (ei->ei_tfree == tindex)
		tindex = EPC_GETW(EPC_TINDEX);
	if (ei->ei_tfree != tindex) {
		EPC_SETW(EPC_TTOP, ei->ei_tfree);
		ei->ei_tdmawait = 0;
	} else {	/* ei->ei_tfree == TINDEX */
		/*
		 * We've just copied to TTOP-1, but bumping TTOP would set
		 * it equal to TINDEX which shuts down the dma engine...
		 * so..  kick this one out later (via scandevs)
		 * Meanwhile push TTOP as far as possible
		 */
		if (ei->ei_tdmawait) {
			EPC_SETW(EPC_TTOP, tthis);
		} else
			ei->ei_tdmawait++;
	}

	/*
	 * free the mbuf
	 */
	_m_freem(m0);

	return 0;
}

static void
ee_tkick()
{
	int tindex;

	if (ei->ei_tdmawait) {
		tindex = EPC_GETW(EPC_TINDEX);
		if (ei->ei_tfree != tindex) {
			EPC_SETW(EPC_TTOP, ei->ei_tfree);
			ei->ei_tdmawait = 0;
		}
	}
}

static int
ee_ioctl(IOBLOCK *iob)
{
	switch ((__psint_t)(iob->IoctlCmd)) {
	case NIOCRESET:
		(void) ee_reset();
		break;
	default:
		printf("et0: invalid ioctl (cmd %d)\n",
			iob->IoctlCmd);
		return iob->ErrorNumber = EINVAL;
	}
	return ESUCCESS;
}

/*ARGSUSED*/
static void
ee_poll(IOBLOCK *iob)
{
	int rlen, type;
	char *bp;
	u_int rstat;
	struct ether_header *eh;
	struct mbuf *m0;

	ee_tkick();

	/*
	 * There's stuff to read if EPC's RINDEX has changed.
	 * (RINDEX can't wrap to ei->ei_orindex because only RLIMIT - 1
	 * rbufs can be open at once)
	 *
	 * for each new packet (first new is old copy of RINDEX)
	 *	copy out of rbuf to mbuf
	 *	pass up to appropriate protocol handler
	 *	bump RTOP to open this buffer for input
	 * 	bump shadow of rindex
	 */
	while (EPC_GETW(EPC_RINDEX) != ei->ei_orindex) {
		bp = EPC_ENET_RBP(ei->ei_orindex);
		rstat = *(u_int*)(bp + EPC_ENET_STAT); /* endian neutral */
		rlen = rstat & EPC_ENET_LENMASK;
                rstat = (rstat >> EPC_ENET_STSHFT) & EPC_ENET_STMASK;

		/* take anything greater than 14 and less than 1501 */
		if (rstat & SEQ_RS_GOOD && rlen > ETHERMIN &&
		    rlen < ETHERMTU + sizeof(struct ether_header)) {
			eh = (struct ether_header *)(bp + EPC_ENET_DATA);
			type = ntohs(eh->ether_type);
			if (type == ETHERTYPE_IP || type == ETHERTYPE_ARP) {
				m0 = _m_get(0, 0);
				m0->m_len = rlen - sizeof(struct ether_header);
				m0->m_off = 0;
				bcopy(bp + EPC_ENET_DATA +
						sizeof(struct ether_header),
						mtod(m0, caddr_t), m0->m_len);
	
				switch (type) {
				case ETHERTYPE_IP:
					_ip_input((struct ifnet*)&ei->ei_ac, m0);
					break;
				case ETHERTYPE_ARP:
					_arpinput(&ei->ei_ac, m0);
					break;
				default:
					break;
				}
			} else {
				if (eedebug == 1)
printf("et0: ok rstat, bad type rbuf=%d rstat=0x%x len=%d type=0x%x\n",
					ei->ei_orindex, rstat, rlen, type);
				/* endian-ness problem? :-) */
			}
		} else {
			if (eedebug == 1)
printf("et0: bad rstat rbuf=%d rstat=0x%x len=%d\n",
						 ei->ei_orindex, rstat, rlen);
		}

		/* indicate we've visited -- for debugging */
		if (eedebug) {
			*(bp + EPC_ENET_DATA) = 0;
			*(u_int *)(bp + EPC_ENET_STAT) = 0;
		}

		/* Open the copy'ed-out rbuf for more input packets */
		EPC_SETW(EPC_RTOP, ei->ei_orindex);

		ei->ei_orindex = EPC_RBN_INCR(ei->ei_orindex);
	}
}

#endif /* EVEREST */
