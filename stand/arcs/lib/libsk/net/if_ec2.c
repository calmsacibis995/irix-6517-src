#if IP20 || IP22 || IP26 || IP28

#ident	"lib/libsk/net/if_ec2.c:  $Revision: 1.90 $"

/* 
 * if_ec2.c - driver for IP20/IP22/IP26/IP28 onboard ethernet controller
 */

#include <arcs/errno.h>
#include <arcs/hinv.h>		/* NBPP */

#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <net/in.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/ei.h>
#include <net/mbuf.h>
#include <net/seeq.h>
#include <saio.h>
#include <saioctl.h>
#include <libsk.h>
#include <libsc.h>

#if IP22 || IP26 || IP28
#define	reset_enet_carrier()
#endif	/* IP22 || IP26 || IP28 */

#if IP20
#define	ENET_READ(reg)	(reg)
#endif	/* IP20 */
#if IP22 || IP26 || IP28
#define	ENET_READ(reg)	(hio->piocfg, ((__psunsigned_t)reg) & ~0xf)
#endif	/* IP22 || IP26 || IP28 */

#if _MIPS_SIM == _ABI64
#define tclean_dcache(x,y) if ((__psunsigned_t)(x) >= K0BASE_NONCOH) clear_cache(x,y)
#else
#define tclean_dcache(x,y) if ((__psunsigned_t)(x) < K1BASE) clear_cache(x,y)
#endif

#define cei(x)		((struct ether_info *)(x->DevPtr))

#define SGI_ETHER_ADDR 0x08		/* 1st byte of SGI enet addresses */
#ifdef	VERBOSE
extern int Verbose;
#define	VPRINTF if (Verbose) printf
#else
#define	VPRINTF
#endif
#ifdef	DEBUG
extern int Debug;
#define	DPRINTF2(arg)	if (Debug>1) printf arg
#define	DPRINTF(arg)	if (Debug) printf arg
#else
#define	DPRINTF2(arg)
#define	DPRINTF(arg)
#endif

/*
 * Each interface is represented by a network interface structure, eh_info,
 * which the routing code uses to locate the interface.
 */
struct eh_info {
	struct		arpcom ei_ac;	/* common ethernet structures */

	rd_desc		*ei_ract;	/* next active receive buffer */
	rd_desc		*ei_rtail;	/* end of available receive buffers */
	int		ei_rnum;	/* number of unused buffers */

	xd_desc		*ei_tact;	/* transmit chain */
	xd_desc		*ei_ttail;	/* end of transmit chain */
	int		ei_tnum;
} eh_info, *ei;

#define ei_if           ei_ac.ac_if		/* network-visible interface */
#define ei_addr		ei_ac.ac_enaddr		/* current ethernet address */

#define	EDLC_NEW_FEATURES	(SEQ_CTL_XCOLL | SEQ_CTL_ACOLL | SEQ_CTL_SQE | \
					SEQ_CTL_SHORT | SEQ_CTL_NOCARR)
#define NUM_TMD		(NSO_TABLE * MAXMBUFS)
#define NUM_RMD		(NSO_TABLE * MAXMBUFS)

/* the descriptor chains must not be cached */
char _align0[128];		/* cache alignment */
xd_desc eh_xd[NUM_TMD+2];	/* transmit/receive descriptor arrays, +1 */
rd_desc eh_rd[NUM_RMD+2];	/* for alignment, +1 for sentinal */
char _align1[128];		/* cache alignment */

EHIO hio = (EHIO)PHYS_TO_K1(0x1fb80000); /* hpc registers map into mem here */

extern int cachewrback;
static int carr_detect;
static int new_edlc;

#if IP22 || IP26 || IP28
static u_int xid;
#endif

int _eclose(void);
static int eioctl(IOBLOCK *);
static int eoutput(struct ifnet *, struct mbuf *, struct sockaddr *);
static void epoll(IOBLOCK *);
static int eput(struct mbuf *);
static void ereset(void);
static int einit(void);
static void seeq_init(u_int);
static void eput_finish(void);
static void eread(void);
static void e_frwrd_pkt(struct mbuf *);

/*
 * _einit -- cleanup globals
 */
int
_einit(void)
{
	ei = &eh_info;
	bzero(ei, sizeof(eh_info));
	ereset();

/* set up ethernet PIO and DMA timing */
#if IP22 || IP26 || IP28
#define	DMA_CFG_25MHZ	0x1e015
#define	DMA_CFG_33MHZ	0x1e026
#define	PIO_CFG_25MHZ	0x51
#define	PIO_CFG_33MHZ	0x161

	if (ip22_gio_is_33MHZ()) {
		*(volatile u_int *)PHYS_TO_K1(HPC3_ETHER_PIO_CFG_ADDR) = 
			PIO_CFG_33MHZ;
		*(volatile u_int *)PHYS_TO_K1(HPC3_ETHER_DMA_CFG_ADDR) = 
			DMA_CFG_33MHZ;
		DPRINTF(("GIO clock rate is 33 MHz\n"));
	} else {
		*(volatile u_int *)PHYS_TO_K1(HPC3_ETHER_PIO_CFG_ADDR) = 
			PIO_CFG_25MHZ;
		*(volatile u_int *)PHYS_TO_K1(HPC3_ETHER_DMA_CFG_ADDR) = 
			DMA_CFG_25MHZ;
		DPRINTF(("GIO clock rate is 25 MHz\n"));
	}
#endif	/* IP22 || IP26 || IP28 */

	new_edlc = (hio->seq_coll_xmit[0] & 0xff) == 0;
	if (new_edlc) {				/* new EDLC */
		hio->seq_csx = SEQ_XC_REGBANK2;
		hio->seq_ctl = EDLC_NEW_FEATURES;
		hio->seq_csx = 0x00;
		DPRINTF(("Using the new EDLC\n"));
	}

#if IP20
	carr_detect = 1;
#endif	/* IP20 */
#if IP22 || IP26 || IP28
	carr_detect = 2;
#endif	/* IP22 || IP26 || IP28 */

	return 0;
}

/*
 *	eprobe -- returns -1 on invalid unit number
 */
int
_eprobe(int unit)
{
	return (unit == 0 ? 1 : -1);
}

static COMPONENT ectmpl = {
	ControllerClass,		/* Class */
	NetworkController,		/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	4,				/* IdentifierLength */
	"ec0"				/* Identifier */
};

static COMPONENT ecptmpl = {
	PeripheralClass,		/* Class */
	NetworkPeripheral,		/* Type */
	Input|Output,			/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentiferLength */
	NULL				/* Identifier */
};

/*
 *	ec_install -- manages ARCS configuration for ec0
 */
void
ec_install(COMPONENT *p)
{
	p = AddChild(p,&ectmpl,0);
	if (p == (COMPONENT *)NULL) cpu_acpanic("ec0");
	p = AddChild(p,&ecptmpl,(void *)0);
	if (p == (COMPONENT *)NULL) cpu_acpanic("ec0");
	RegisterDriverStrategy(p,_if_strat);
}	

/*
 * _eopen -- setup seeq and initialize network driver data structs
 */
int
_eopen(IOBLOCK *iob)
{
	if (ei->ei_if.if_flags & (IFF_UP|IFF_RUNNING|IFF_BROADCAST))
		panic("ec0: trying to open the driver twice\n");

	cei(iob)->ei_registry = -1;	/* mark not bound yet */

	cpu_get_eaddr(ei->ei_addr);

	if (ei->ei_addr[0] != SGI_ETHER_ADDR) {
		printf("ec0: bad ethernet address %s\n",
			(char *)ether_sprintf(ei->ei_addr));
		return(iob->ErrorNumber = EINVAL);
	}

	if (einit() < 0)
		return(iob->ErrorNumber = EIO);

	ei->ei_if.if_unit = 0;
	ei->ei_if.if_mtu = ETHERMTU;
	ei->ei_if.if_output = eoutput;
	ei->ei_if.if_ioctl = eioctl;
	ei->ei_if.if_poll = epoll;
	cei(iob)->ei_acp = &ei->ei_ac;	/* iob ptr to arpcom */

	ei->ei_if.if_flags = IFF_UP|IFF_RUNNING|IFF_BROADCAST;

	return(ESUCCESS);
}

static void
ereset(void)
{
	hio->rcvstat = 0;	/* shut off dma before doing the reset */
	hio->trstat = 0;

	hio->ctl = HPC_MODNORM | HPC_ERST;
	delay(15);
	hio->ctl = HPC_MODNORM;
}

/*
 * eioctl -- device specific ioctls
 */
static int
eioctl(IOBLOCK *iob)
{

	switch ((__psint_t)(iob->IoctlCmd)) {
	case NIOCRESET:
		(void) ereset();
		break;

	default:
		printf("ec0: invalid ioctl (cmd %d)\n",
			iob->IoctlCmd);
		return iob->ErrorNumber = EINVAL;
	}
	return(ESUCCESS);
}

static int
einit(void)
{
	rd_desc *rd_prev, *rd;
	xd_desc *xd;
	struct mbuf *m;
	int i;

	/*
	 * allocate new mbufs for the seeq to receive into
	 *
	 */
#if IP20
	rd = ei->ei_ract = (rd_desc *)PHYS_TO_K1(KDM_TO_PHYS(((u_char *)eh_rd +
		(NBPP - poff(eh_rd)) % sizeof(rd_desc))));
#endif	/* IP20 */
#if IP22 || IP26 || IP28
	/* quadword alignment */
	rd = ei->ei_ract =
		(rd_desc *)PHYS_TO_K1(KDM_TO_PHYS(((__psunsigned_t)eh_rd + 0xf) & ~0xf));
#endif	/* IP22 || IP26 || IP28 */
	bzero((void *)rd, sizeof(rd_desc) * (NUM_RMD + 1));

	rd_prev = rd + NUM_RMD;		/* extra desc is sentinal */

	for (i = 0; i < NUM_RMD; i++) {

		if ((m = _m_get(M_DONTWAIT, MT_DATA)) == 0) {
			rd = ei->ei_ract;
			while (rd && rd->r_mbuf) {
				_m_freem(rd->r_mbuf);
				rd = (rd_desc *)PHYS_TO_K1(rd->r_nrdesc);
			}
			printf("ec0: Not enough mbufs to initialize receiver.\n");
			return -1;
		}

		/* initialize new receive descriptor
		 */
		rd->r_rbcnt = MAX_RPKT;
		rd->r_rown = 1;
		rd->r_rbufptr = KDM_TO_MCPHYS( mtod(m, char *) );
#ifdef	DEBUG
		if (Debug > 1) {
			u_int *dp = ((u_int *)PHYS_TO_K1(rd->r_rbufptr));
			*dp++ = 0;
			*dp++ = 0;
			*dp++ = 0;
			*dp   = 0;
		}
#endif
		m->m_off = 2;
		rd->r_mbuf = m;
		rd->r_eor = 0;
#if IP22 || IP26 || IP28
		rd->r_eorp = 1;
		rd->r_xie = 0;
#endif	/* IP22 || IP26 || IP28 */

		rd_prev->r_nrdesc = KDM_TO_MCPHYS(rd);
		rd_prev = rd++;

		tclean_dcache(m, sizeof(struct mbuf));
	}

	/* XXX DO WE REALLY NEED A SETINEL ? XXX */
	rd->r_mbuf = 0;
	rd->r_rbufptr = 0xEBAD;
	rd->r_rown = 1;

	rd_prev->r_eor = 1;
	rd_prev->r_nrdesc = KDM_TO_MCPHYS(rd);
	ei->ei_rtail = rd_prev;
	ei->ei_rnum = NUM_RMD;

	/*
	 * Initialize transmit descriptors.
	 */

	/* align xmit desc */
#if IP20
	xd = ei->ei_tact = (xd_desc *)PHYS_TO_K1(KDM_TO_PHYS(((u_char *)eh_xd +
		(NBPP - poff(eh_xd)) % sizeof(xd_desc))));
#endif	/* IP20  */
#if IP22 || IP26 || IP28
	/* quadword alignment */
	xd = ei->ei_tact =
		(xd_desc *)PHYS_TO_K1(KDM_TO_PHYS(((__psunsigned_t)eh_xd + 0xf) & ~0xf));
#endif	/* IP22 || IP26 || IP28 */
	bzero((void *)xd, sizeof(xd_desc) * (NUM_TMD + 1));

	ei->ei_ttail = xd + NUM_TMD;
	ei->ei_ttail->x_nxdesc = KDM_TO_MCPHYS(xd);

	for (i = 0; i < NUM_TMD; i++) {
		xd->x_nxdesc = KDM_TO_MCPHYS(xd + 1);
		xd++;
	}
	ei->ei_tnum = 0;

	/*
	 * setup seeq address and mode
	 */
	seeq_init( SEQ_RC_RSB );

	hio->nrbdp = hio->crbdp = KDM_TO_MCPHYS(ei->ei_ract);
	hio->nxbdp = hio->cpfxbdp = hio->ppfxbdp = KDM_TO_MCPHYS(ei->ei_tact);

#if IP20
	hio->rcvstat = HPC_STRCVDMA;
#endif	/* IP20 */
#if IP22 || IP26 || IP28
#ifdef _MIPSEB
	hio->rcvstat = HPC_STRCVDMA;
#else
	hio->rcvstat = HPC_STRCVDMA | HPC_RCV_ENDIAN_LITTLE;
#endif	/* _MIPSEB */
	DPRINTF2(("Starting receive DMA in einit()\n"));
#endif	/* IP22 || IP26 || IP28 */

	return 0;
}

/*
 * initialize seeq chip
 */
static void
seeq_init(u_int rmode)
{
	int i;

	ereset();

	for (i = 0; i < 6; i++)
		hio->seq_eaddr[i] = ei->ei_addr[i];

	hio->seq_csx = SEQ_XC_INTGOOD | SEQ_XC_INT16TRY | SEQ_XC_INTCOLL |
		   SEQ_XC_INTUFLOW | SEQ_XC_REGBANK2;

	hio->seq_csr = rmode | SEQ_RC_INTGOOD | SEQ_RC_INTEND |
		SEQ_RC_INTSHORT | SEQ_RC_INTDRBL | SEQ_RC_INTCRC;

#if IP20
	hio->intdelay = 0;
#endif	/* IP20 */
}

/*
 * eoutput -- raw access to transmit a packet
 */
/*ARGSUSED*/
static int
eoutput(struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst)
{
	ETHADDR edst;
	struct in_addr idst;
	struct ether_header *eh;
	int type;

#ifdef notdef
	DPRINTF(("sending to %d 0x%x %d, cc=%d\n", dst->sa_family, ((struct sockaddr_in *)dst)->sin_addr, nuxi_s((((struct sockaddr_in *)dst)->sin_port), m0->m_len)));
#endif

	switch (dst->sa_family) {

	case AF_INET:
		idst = ((struct sockaddr_in *)dst)->sin_addr;
		_arpresolve(&ei->ei_ac, &idst, (u_char *)&edst);
		type = ETHERTYPE_IP;
		break;

	case AF_UNSPEC:
		eh = (struct ether_header *)dst->sa_data;
		bcopy(eh->ether_dhost, &edst, sizeof(edst));
		type = eh->ether_type;
		break;

	default:
		/* EAFNOSUPPORT */
		_m_freem(m0);
		printf("ec0: no support for address family %d\n",
		    (int)dst->sa_family);
		return(-1);
	}

	/*
         * Add local net header.  If no space in mbuf, drop it (shouldn't
	 * happen!)
         */
	if (m0->m_off < sizeof(struct ether_header)
	    || m0->m_off + m0->m_len > MLEN) {
		DPRINTF(("ec0: transmit -- no space for hdr\n"));
		_m_freem(m0);
		return(-1);
	}

	m0->m_off -= sizeof(struct ether_header);
	m0->m_len += sizeof(struct ether_header);
	eh = mtod(m0, struct ether_header *);
	eh->ether_type = htons((u_short)type);
	bcopy(&edst, eh->ether_dhost, sizeof(edst));
	bcopy(ei->ei_addr, eh->ether_shost, sizeof(ETHADDR));

#ifdef	_OLDHPC
	if (needs_dma_swap)
	    swapl(m0->m_dat, (((m0->m_off + m0->m_len) + 3) & ~3) >> 2);
#endif	/* _OLDHPC */
	/*
	 * Queue message on interface
	 */
	return( eput(m0) );
}


static int
eput(struct mbuf *m)
{
	xd_desc *xd_build;
	caddr_t cp;

	/*
	 * must have a descriptor to use
	 */
	if (ei->ei_tnum >= NUM_TMD) {
		DPRINTF(("ec0: dropped transmit packet\n"));
		_m_freem(m);
		return -1;
	}

	xd_build = (xd_desc *)PHYS_TO_K1(ei->ei_ttail->x_nxdesc);
	ei->ei_tnum++;

	cp = mtod(m, caddr_t);
	xd_build->x_xbufptr = KDM_TO_MCPHYS(cp);
	xd_build->x_xbcnt = _max(MIN_TPKT, m->m_len);

	xd_build->x_xie = 0;
	xd_build->x_eoxp = 1;
	xd_build->x_eox = 1;
#if IP22 || IP26 || IP28
	xd_build->x_txd = 0;
	xd_build->x_xid = xid++;
#endif	/* IP22 || IP26 || IP28 */
	xd_build->x_mbuf = m;

	if (cachewrback)
		tclean_dcache(cp, xd_build->x_xbcnt);

	/*
	 * starting xmit is sort of a hack because there is a small
	 * window in which STTRDMA can be high but not notice that
	 * x_eox has become zero, which will cause the packet to not
	 * go out until dma is restarted later in eput_finish
	 */
	ei->ei_ttail->x_eox = 0;
	ei->ei_ttail = xd_build;

	if (!(hio->trstat & HPC_STTRDMA)) {
#if IP22 || IP26 || IP28
		xd_desc *xd;
		xd_desc *xd0;
		xd_desc *xd1;
#endif

		if (carr_detect == 1)
			reset_enet_carrier();
		else if (carr_detect == 2) {
			hio->seq_ctl = EDLC_NEW_FEATURES & 
				~(SEQ_CTL_NOCARR | SEQ_CTL_XCOLL);
			hio->seq_ctl = EDLC_NEW_FEATURES;
		}

		/* need to kickstart the hpc */
#if IP20
		/* XXX NEED TO CHECK FOR RETRANSMISSION XXX */
		hio->ppfxbdp = hio->cpfxbdp = ENET_READ(hio->nxbdp); 
		hio->trstat = HPC_STTRDMA;
#endif	/* IP20 */
#if IP22 || IP26 || IP28
		xd0 = (xd_desc *)PHYS_TO_K1(ENET_READ(hio->ppfxbdp));
		xd1 = (xd_desc *)PHYS_TO_K1(ENET_READ(hio->cpfxbdp));

		if (xd0->x_txd && xd1->x_txd)
			xd = (xd_desc *)ENET_READ(hio->nxbdp);
		else if (!xd0->x_txd && !xd1->x_txd) {
			if (((xd1->x_xid - xd0->x_xid) & XID_MASK) != XID_MASK)
				xd = xd0;
			else
				xd = xd1;
		} else if (!xd0->x_txd)
			xd = xd0;
		else if (!xd1->x_txd)
			xd = xd1;

		hio->ppfxbdp = hio->cpfxbdp = hio->nxbdp = KDM_TO_MCPHYS(xd);
#ifdef _MIPSEB
		hio->trstat = HPC_STTRDMA;
#else
		hio->trstat = HPC_STTRDMA | HPC_TR_ENDIAN_LITTLE;
#endif	/* _MIPSEB */
		DPRINTF2(("Starting transmit DMA in eput()\n"));
#endif	/* IP22 || IP26 || IP28 */
	}

	return 0;
}

/*
 * eput_finish -- lower half of eput.  Recover mbufs, check for
 * errors.  Called while in scandevs()
 */
static void
eput_finish(void)
{
	xd_desc *xd_chain;
	xd_desc *xd0, *xd1;
	u_int xstat;

	xd_chain = ei->ei_tact;
	xd0 = (xd_desc *)ENET_READ(hio->ppfxbdp);
	xd1 = (xd_desc *)ENET_READ(hio->cpfxbdp);

	while ((xd_desc *)KDM_TO_PHYS(xd_chain) != xd0
	    && (xd_desc *)KDM_TO_PHYS(xd_chain) != xd1) {
		_m_freem(xd_chain->x_mbuf);
		xd_chain->x_eox = 1;
		xd_chain->x_xbufptr = 0;
		xd_chain = (xd_desc *)PHYS_TO_K1(xd_chain->x_nxdesc);
		ei->ei_tnum--;
	}

	xstat = hio->trstat;
#if IP20
	if (!(xstat & HPC_STTRDMA)) {
		xd0 = (xd_desc *)(ENET_READ(hio->nxbdp));
		while ((xd_desc *)KDM_TO_PHYS(xd_chain) != xd0) {
			_m_freem(xd_chain->x_mbuf);
			xd_chain->x_eox = 1;
			xd_chain->x_xbufptr = 0;
			xd_chain = (xd_desc *)PHYS_TO_K1(xd_chain->x_nxdesc);
			ei->ei_tnum--;
		}

		if (xstat & SEQ_XS_UFLOW) {
			DPRINTF(("ec0: transmit underflow\n"));
		} else if (xstat & SEQ_XS_16TRY) {
			DPRINTF(("ec0: 16 xmit collisions\n"));
		} else if (ei->ei_tnum) {
			DPRINTF(("ec0: late hpc dma start\n"));
		}

		if (ei->ei_tnum) {
			/* need to kickstart the hpc */
			hio->ppfxbdp = hio->cpfxbdp = ENET_READ(hio->nxbdp);
			hio->trstat = HPC_STTRDMA;
		} else if (carr_detect && !enet_carrier_on())
			printf("ec0: ethernet cable problem\n");
	}
#endif	/* IP20 */
#if IP22 || IP26 || IP28
	if (!(xstat & HPC_STTRDMA)) {
		u_int xd;

		xd0 = (xd_desc *)PHYS_TO_K1(ENET_READ(hio->ppfxbdp));
		xd1 = (xd_desc *)PHYS_TO_K1(ENET_READ(hio->cpfxbdp));

		if (xd0->x_txd && xd1->x_txd)
			xd = ENET_READ(hio->nxbdp);
		else if (!xd0->x_txd && !xd1->x_txd) {
			if (((xd1->x_xid - xd0->x_xid) & XID_MASK) != XID_MASK)
				xd = KDM_TO_MCPHYS(xd0);
			else
				xd = KDM_TO_MCPHYS(xd1);
		} else if (!xd0->x_txd)
			xd = KDM_TO_MCPHYS(xd0);
		else if (!xd1->x_txd)
			xd = KDM_TO_MCPHYS(xd1);

		while (KDM_TO_PHYS(xd_chain) != xd) {
			_m_freem(xd_chain->x_mbuf);
			xd_chain->x_eox = 1;
			xd_chain->x_xbufptr = 0;
			xd_chain = (xd_desc *)PHYS_TO_K1(xd_chain->x_nxdesc);
			ei->ei_tnum--;
		}

		if (xstat & SEQ_XS_UFLOW) {
			DPRINTF(("ec0: transmit underflow\n"));
		} else if (xstat & SEQ_XS_16TRY) {
			DPRINTF(("ec0: 16 xmit collision\n"));
		} else if (xstat & SEQ_XS_LATE_COLL) {
			DPRINTF(("ec0: late xmit collision\n"));
		} else if (ei->ei_tnum) {
			DPRINTF(("ec0: late hpc dma start\n"));
		}

		if (ei->ei_tnum) {
			/* need to kickstart the hpc */
			hio->ppfxbdp = hio->cpfxbdp = hio->nxbdp = xd;
#ifdef _MIPSEB
			hio->trstat = HPC_STTRDMA;
#else
			hio->trstat = HPC_STTRDMA | HPC_TR_ENDIAN_LITTLE;
#endif	/* _MIPSEB */
			DPRINTF2(("Starting transmit DMA in eput_finish()\n"));
		} else if (hio->seq_flags & SEQ_XS_NO_CARRIER) {
			if (((hio->seq_coll_xmit[0] & 0xff) == 0)
			    && ((hio->seq_coll_xmit[1] & 0x7) == 0))
				printf("ec0: ethernet cable problem\n");
		}
	}
#endif	/* IP22 || IP26 || IP28 */
	ei->ei_tact = xd_chain;
}


static void
eread(void)
{
	int bcnt;
	rd_desc *rd_chain;
	rd_desc *rd_tail, *rd_crbdp;
	u_char rstat, *status;
	u_int ircvstat;
	struct mbuf *m;

	bcnt = ei->ei_rnum;
	rd_chain = ei->ei_ract;		/* from beginning of chain */

	while (!rd_chain->r_rown) {

		struct ether_header *eh;
		int rem, rlen;

		if (Verbose > 1) printf (".");
		m = rd_chain->r_mbuf;
		eh = mtod(m, struct ether_header *);

#ifdef IP28	/* R10000_DMA_READ_WAR */
		rlen = MAX_RPKT - rd_chain->r_rbcnt - HPC_RSPACE;
		__dcache_inval(eh,rlen);
#endif

#ifdef	DEBUG
		if (Debug > 1) {
		    char *htoe();
		    printf("packet in: type %x", ntohs(eh->ether_type));
		    printf(" tgt %s",   htoe(eh->ether_dhost));
		    printf(" src %s\n", htoe(eh->ether_shost));
		}
#endif

#ifdef	_OLDHPC
		/*
		 * the cache needs to be flushed before byte swapping is
		 * done, but byte swapping needs to be done before this
		 * ddm stuff needs to be done.  This flushes and swaps 
		 * all of the data in the mbuf whether the packet is
		 * valid or not, and whether the packet is smaller than
		 * the mbuf data.  
		 */
		if (needs_dma_swap)
		    swapl(m->m_dat, MLEN >> 2);
#endif
		/*
		 * the DDM memory controller chip has a bug which
		 * allows the remaining byte count to occasionally
		 * be written into the bufptr field.  This is a
		 * heuristic fix.
		 */
		rem = rd_chain->r_rbcnt;

		rlen = MAX_RPKT - rem - HPC_RSPACE;
		if (rlen <= 0) {
			DPRINTF(("eread(): rec'd >MAX_RPKT packet: %d bytes\n",
			    rem));
			_m_freem(m);
			goto nextpkt;
		}

		rstat = *((caddr_t)eh + rlen);

		if (!(rstat & SEQ_RS_GOOD)) {/* map error state */
#if IP22 || IP26 || IP28
			if ((rstat & SEQ_RS_LATE_RXDC)
			    && !(rstat & SEQ_RS_STATUS_5_0)) {
				DPRINTF(("ec0: late receive discard.\n"));
			}
			if ((rstat & SEQ_RS_TIMEOUT)
			    && (rstat & SEQ_RS_STATUS_5_0 == SEQ_RS_END)) {
				DPRINTF(("ec0: receive timeout.\n"));
			}
#endif	/* IP22 || IP26 || IP28 */
			if (rstat & SEQ_RS_SHORT) {
				DPRINTF(("ec0: packet too small (%d).\n",rlen));
			} else if (rstat & SEQ_RS_DRBL) {
				DPRINTF(("ec0: packet framing error\n"));
			} else if (rstat & SEQ_RS_CRC) {
				DPRINTF(("ec0: CRC error\n"));
			} else if (rstat & SEQ_RS_OFLOW) {
				DPRINTF(("ec0: FIFO overflow error\n"));
			}

			_m_freem(m);
			goto nextpkt;
		}

		m->m_len = rlen;

		e_frwrd_pkt(m);

nextpkt:	bcnt--;

		rd_chain->r_rown = 1;
		if (rd_chain->r_eor)		/* stay sane */
			((rd_desc *)PHYS_TO_K1(rd_chain->r_nrdesc))->r_rown = 1;

		rd_chain = (rd_desc *)PHYS_TO_K1(rd_chain->r_nrdesc);
	}
#ifdef	DEBUG
	if (Debug > 1) {	/* check first 16 bytes to see if written */
		u_int *dp = (u_int *)PHYS_TO_K1(rd_chain->r_rbufptr);
		if (*dp++ || *dp++ || *dp++ || *dp)
			DPRINTF(("eread(): buffer written by seeq without "
				 "ownership bit cleared\n"));
	}
#endif
	/*
	 * allocate new mbufs for the seeq to receive into
	 */
	rd_tail = ei->ei_rtail;
	while (bcnt < NUM_RMD) {

		rd_desc *rd;

		if ((m = _m_get(M_DONTWAIT, MT_DATA)) == 0) {
			printf("ec0: eread out of mbufs\n");
			break;
		}

		rd = (rd_desc *)PHYS_TO_K1(rd_tail->r_nrdesc);
		bcnt++;

		rd->r_rbcnt = MAX_RPKT;
		rd->r_rbufptr = KDM_TO_MCPHYS(mtod(m, char *));
#ifdef	DEBUG
		if (Debug > 1) {
			u_int *dp = (u_int *)PHYS_TO_K1(rd->r_rbufptr);
			*dp++ = 0;
			*dp++ = 0;
			*dp++ = 0;
			*dp   = 0;
		}
#endif
		rd->r_mbuf = m;
		m->m_off = 2;		/* hpc offsets rcv packets 2 bytes */

		((rd_desc *)PHYS_TO_K1(rd->r_nrdesc))->r_rown = 1;
		rd->r_eor = 1;

#if IP22 || IP26 || IP28
		rd->r_eorp = 1;
		rd->r_xie = 0;
#endif	/* IP22 || IP26 || IP28 */

		rd_tail->r_eor = 0;
		rd_tail = rd;

		tclean_dcache(m, sizeof(struct mbuf));
	}
	ei->ei_rtail = rd_tail;
	ei->ei_rnum = bcnt;
	ei->ei_ract = rd_chain; 		/* from beginning of chain */

	ircvstat = hio->rcvstat;
	if (!(ircvstat & HPC_STRCVDMA)) {		/* stopped. why? */

		rstat = ircvstat >> RCVSTAT_SHIFT;
		rd_crbdp = (rd_desc *)PHYS_TO_K1(ENET_READ(hio->crbdp));

		/*
		 * on the 2 error conditions for which we could be stopped,
		 * patch up the mbuf so that it will get tossed when the
		 * chain is received
		 */
#if IP20
		if (hio->ctl & HPC_RBO) {
			hio->ctl = HPC_MODNORM | HPC_RBO;
#endif	/* IP20 */
#if IP22 || IP26 || IP28
		if (rstat & HPC_RBO) {
#endif	/* IP22 || IP26 || IP28 */
			DPRINTF(("ec0: packet too big"));
			rd_crbdp->r_rbcnt = MAX_RPKT-HPC_RSPACE;

                        if (rd_crbdp->r_nrdesc != (__uint32_t)KDM_TO_MCPHYS(rd_chain))
				rd_crbdp->r_rown = 0;

			m = rd_crbdp->r_mbuf;
			status = mtod(m, u_char *);
			*status = 0;	/* clear SEQ_RS_GOOD */
		}

		if (rd_crbdp != ei->ei_rtail) {	/* restart if buffers avail */
			hio->nrbdp = rd_crbdp->r_nrdesc;
			hio->rcvstat = HPC_STRCVDMA;
		}
	}
}

/*
 * e_frwrd_pkt -- forward packet to appropriate protocol handler
 */
static void
e_frwrd_pkt(struct mbuf *m)
{
	struct ether_header *eh;
	int len1, len2, off;
	u_short type;
	char *cp, *resid;

	/*
	 * Get input data length.
	 * Get pointer to ethernet header (in input buffer).
	 * Deal with trailer protocol: if type is PUP trailer
	 * get true type from first 16-bit word past data.
	 * Remember that type was trailer by setting off.
	 */

	eh = mtod(m, struct ether_header *);
	cp = (char *)eh;

	m->m_off += sizeof(struct ether_header);
	m->m_len -= sizeof(struct ether_header);
	cp += sizeof(struct ether_header);
	len1 = m->m_len;
#ifdef	_MIPSEL
	type = nuxi_s((u_short)eh->ether_type);
#else	/*_MIPSEB*/
	type = eh->ether_type;
#endif	/*_MIPSEL*/

	if (type >= ETHERTYPE_TRAIL
	    && type < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) {

		off = (type - ETHERTYPE_TRAIL) * 512;
		if (off >= ETHERMTU) {
		    DPRINTF(("e_frwrd_pkt(): discarding, off(%d) >= ETHERMTU\n",
			    off));
			_m_freem(m);
			return;			/* sanity */
		}

		resid = cp + off;
#ifdef	_MIPSEL
		type = nuxi_s(*(u_short *)resid);
#else
		type = *(u_short *)resid;
#endif	/* _MIPSEL */

		resid += sizeof(u_short);
#ifdef	_MIPSEL
		len2 = nuxi_s(*(u_short *)resid);
#else
		len2 = *(u_short *)resid;
#endif	/* _MIPSEL */
		resid += sizeof(u_short);

		if (off + len2 > len1) {
			DPRINTF(("e_frwrd_pkt(): discarding, off(%d) + "
				"len2(%d) > len1(%d)\n", off, len2, len1));
			_m_freem(m);
			return;			/* sanity */
		}
		len2 -= 2*sizeof(u_short);
		len1 = off;

	} else
		off = 0;

	if( len1 == 0 ) {
		DPRINTF(("e_frwrd_pkt(): discarding, len1 == 0\n"));
		_m_freem(m);
		return;
	}

	if (off) {	/* trailer protocol */

		struct mbuf *n;

		if ((n = _m_get(M_DONTWAIT, MT_DATA)) == 0) {
#ifdef	DEBUG
			DPRINTF(("e_frwrd_pkt(): discarding, no mbuf\n"));
#endif
			_m_freem(m);
			return;
		}

		n->m_off = MMAXOFF;
		n->m_len = len1 + len2;

		bcopy(resid, mtod(n, char *), len2);
		bcopy(cp, mtod(n, char *)+len2, len1);

		_m_freem(m);
		m = n;
	}

	switch (type) {
	case ETHERTYPE_IP:
		_ip_input(&ei->ei_if, m);
		break;

	case ETHERTYPE_ARP:
		_arpinput(&ei->ei_ac, m);
		break;

	default:
		DPRINTF(("e_frwrd_pkt(): discarding, unknown type %x\n", type));
		_m_freem(m);
	}
}

int
_eclose(void)
{
	rd_desc *rd;
	xd_desc *xd;
	int bcnt;

	while (hio->trstat & HPC_STTRDMA)
		/* empty */;

	ereset();

	bcnt = ei->ei_rnum;
	rd = ei->ei_ract;
	while (bcnt--) {
		_m_freem(rd->r_mbuf);
		rd = (rd_desc *)PHYS_TO_K1(rd->r_nrdesc);
	}

	bcnt = ei->ei_tnum;
	xd = ei->ei_tact;
	while (bcnt--) {
		_m_freem(xd->x_mbuf);
		xd = (xd_desc *)PHYS_TO_K1(xd->x_nxdesc);
	}
	ei->ei_rnum = 0;
	ei->ei_tnum = 0;
	ei->ei_if.if_flags &= ~(IFF_UP|IFF_RUNNING|IFF_BROADCAST);

	return 0;
}

/*ARGSUSED*/
static void
epoll(IOBLOCK *io)
{
	eread();
	if (ei->ei_tnum)
		eput_finish();
}

#endif	/* IP20 || IP22 || IP26 || IP28 */
