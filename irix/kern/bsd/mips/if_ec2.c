/*
 * seeq/hpc1 or hpc3 integral/gio ethernet driver for IP20/IP22/IP26
 *
 * Copyright 1995, Silicon Graphics, Inc.  All Rights Reserved.
 */
#ident "$Revision: 1.130 $"

#if IP20 || IP22 || IP26 || IP28	/* compile for IP2[0268] */

#include "tcp-param.h"
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
#include "ether.h"
#include "sys/kmem.h"
#include "sys/hwgraph.h"
#include "sys/iograph.h"
#include "sys/giobus.h"
#if IP24_GIO
#undef	IP22
#define	IP20 1
#define IP24_NO_INTEGRAL_EC 1
#define if_ecedtinit if_egedtinit
#define if_ecdevflag if_egdevflag
#define ec2_dump eg2_dump
#define idbg_ec2_dump idbg_eg2_dump
#endif
#include "sys/seeq.h"
#include "sys/ddi.h"
#include "sys/pda.h"
#include "string.h"
#include <net/rsvp.h>

#if CONSISTANCY_CHECK
#define	CCEFIXUP		0
#define	CCELINES_LIMIT		500
#define	CCF_REQUIRE_NOHIT	0x01
#define	CCF_REQUIRE_NODIRTY	0x02

extern void consistancy_check(unsigned pa, unsigned ct, char *where, unsigned flags);
#else
#define consistancy_check(a,l,w,f)
#endif

#if CONSISTANCY_CHECK_ETH
#define consistancy_check_eth(a,l,w,f)		consistancy_check(a,l,w,f)
#else
#define consistancy_check_eth(a,l,w,f)
#endif

#define HPC_NBPP IO_NBPP
#define HPC_POFFMASK IO_POFFMASK
#define hpc_pnum(a) io_pnum(a)
#define hpc_poff(a) io_poff(a)

#define BYTES_PER_DMA 2048
#define DMA_PAGES (_PAGESZ / BYTES_PER_DMA)

#if IP20
#define	MAX_NUM_EDLC	3
#define	ENET_READ(reg)	(reg)
#endif	/* IP20 */
#if IP22 || IP26 || IP28
#if IP22
#define	MAX_NUM_EDLC	4	/* unit 3 used on the Challenge S */
#else
#define	MAX_NUM_EDLC	1
#endif
/* Hardware BUG - PIO readback of DMA descriptors bad unless another
 * register is read immediately before */
#define	ENET_READ(reg)	(ei->_s = spl7(), ei->_reg = hio->piocfg, ei->_reg = (reg), splx(ei->_s), ei->_reg) 
#endif	/* IP22 || IP26 || IP28 */

int if_ecdevflag = 0;

#ifdef DEBUG	/* dwong */
int _ecintr_is_last[MAX_NUM_EDLC];
__psunsigned_t _ecintr_xd_cur0[MAX_NUM_EDLC];
__psunsigned_t _ecintr_xd_prev0[MAX_NUM_EDLC];
__psunsigned_t _ecintr_xd_tact0[MAX_NUM_EDLC];
__psunsigned_t _ecintr_xd_cur1[MAX_NUM_EDLC];
__psunsigned_t _ecintr_xd_prev1[MAX_NUM_EDLC];
__psunsigned_t _ecintr_xd_tact1[MAX_NUM_EDLC];
__psunsigned_t _xmit_xd_cur0[MAX_NUM_EDLC];
__psunsigned_t _xmit_xd_prev0[MAX_NUM_EDLC];
__psunsigned_t _xmit_xd_tact0[MAX_NUM_EDLC];
__psunsigned_t _xmit_xd_cur1[MAX_NUM_EDLC];
__psunsigned_t _xmit_xd_prev1[MAX_NUM_EDLC];
__psunsigned_t _xmit_xd_tact1[MAX_NUM_EDLC];
#endif	/* DEBUG */

#ifdef OS_METER
static u_int ec_istat_crc[MAX_NUM_EDLC];
static u_int ec_istat_frame[MAX_NUM_EDLC];
static u_int ec_istat_oflow[MAX_NUM_EDLC];
static u_int ec_istat_small[MAX_NUM_EDLC];
#if IP22 || IP26 || IP28
static u_int ec_istat_rxdc[MAX_NUM_EDLC];
static u_int ec_istat_timeout[MAX_NUM_EDLC];
static u_int ec_ostat_late_coll[MAX_NUM_EDLC];
#endif	/* IP22 || IP26 || IP28 */
static u_int ec_ostat_uflow[MAX_NUM_EDLC];
static u_int ec_trdma_on[MAX_NUM_EDLC];
#endif	/* OS_METER */

#define EDOG		(2*IFNET_SLOWHZ)  /* watchdog duration in seconds */
#define EMSG_DELAY	(30*HZ)

/* XXX these probably all belong elsewhere */

/* first 3 bytes of SGI enet addresses */
#define SGI_ETHER_ADDR_0 0x08
#define SGI_ETHER_ADDR_1 0x00
#define SGI_ETHER_ADDR_2 0x69

/* first 3 bytes of Cabletron's assigned ethernet addresses
 * Necessary because HPLC (Hollywood light) uses cabletron's addresses.
 */
#define CABLETRON_ETHER_ADDR_0 0x00
#define CABLETRON_ETHER_ADDR_1 0x00
#define CABLETRON_ETHER_ADDR_2 0x1d

#define K2_TO_PHYS(x)	(ctob(kvtokptbl(x)->pte.pg_pfn) + poff(x))
#define K_TO_PHYS(x)	(IS_KSEG2(x) ? K2_TO_PHYS(x) : K0_TO_PHYS(x))
#define	SPACE_FOR_EHDR		(EHDR_LEN + RAW_HDRPAD(EHDR_LEN))
#define HPC_DELAY_EXPIRED	(1 << 24)
#define HPC_DELAY_SHIFT		4

/*
 * Each interface is represented by a network interface structure, ec_info,
 * which the routing code uses to locate the interface.
 */
static struct ec_info {
	struct seeqif	ei_sif;		/* seeq network interface */
					/* must be the first field in struct */
	rd_desc		*ei_ract;	/* next active receive buffer */
	rd_desc		*ei_rtail;	/* end of receive buffers */
	int		ei_rnum;	/* number of unused buffers */
	int		ei_num_rxd;	/* allocate this many rcv buffers */

	xd_desc		*ei_tact;	/* 1st active transmit buffer */
	xd_desc		*ei_ttail;	/* end of transmit chain */
	int		ei_tfreecnt;
	struct mbuf	*ei_tmbuf_head;
	struct mbuf	*ei_tmbuf_tail;

	u_int		ei_carr_msg_lbolt;
	u_char		ei_mode;
	u_char		ei_seeq_ctl;

	int		ei_unit;	/* EDLC unit number */
	u_short		ei_eaddr_a;	/* 2 MSBs of this EDLC's eaddr */
	u_int 		ei_eaddr_b;	/* 4 LSBs of this EDLC's eaddr */
	u_int 		ei_c_detect;	/* 0 -> no carrier detection
				   	   1 -> detection by wire on board
				   	   2 -> detection by EDLC */
	u_int 		ei_carrier_was_up;
	u_int 		ei_sgi_edlc;	/* non-seeq EDLC */
	u_int 		ei_sqe_on;	/* sqe is turned on */
	u_int		ei_coll_adj;	/* collision adjustment due to SQE */
	int		ei_efix;
	int		ei_oflow_lbolt;
	int		ei_oflow_cnt;
	u_int		ei_oflow_id;
	toid_t		ei_hang_id;	/* 'enet hang' timer id */
#ifdef DEBUG
	int		ei_hang_cnt;	/* 'enet hang' count */
	int		ei_xmit_sync_cnt;	/* desc out of sync count */
	int		ei_watchdog_cnt;	/* watchdog count */
#endif	/* DEBUG */

	caddr_t		ei_tmdvec;	/* start of the transmit and receive */
	caddr_t		ei_rmdvec;	/* descriptors */

#if IP20
	caddr_t		ei_xmit_buf;	/* if_ec2's own DMA-able xmit buffer */
	int		ei_xmit_cp_cnt;	/* xmit buf copy count */
	caddr_t		ei_rcv_buf;	/* if_ec2's own DMA-able rcv buffer */
#endif	/* IP20 */

#if IP22 || IP26 || IP28
	u_int		ei_srcaddr_a;	/* 4 MSBs of this EDLC's eaddr */
	u_short		ei_srcaddr_b;	/* 2 LSBs of this EDLC's eaddr */
	int		ei_unicast_counter;	/* unicast to self counter */
	toid_t		ei_unicast_id;	/* unicast watchdog timeout id */
#endif	/* IP22 || IP26 || IP28 */
	u_int		ei_carr_opkts;	/* keep track of xmits w/ no carrier */
	u_int		ei_xd16coll;	/* xmit desc on 16 collision retry */
	int		ei_try16coll;	/* remaining 16 collision retries */

	u_int		ei_no_xd_cnt;	/* For bug 321135: looping no tx-desc */
	int		_s;		/* Saved spl */
	uint		_reg;		/* Placeholder for register read */
	int		ei_collisions;	/* Collisions reported by chip */
	
	EHIO		ei_io;		/* I/O addresses */
} ec_info[MAX_NUM_EDLC];

#define	ei_eif		ei_sif.sif_eif		/* common ethernet stuff */
#define	ei_ac		ei_eif.eif_arpcom	/* common arp stuff */
#define ei_if           ei_ac.ac_if		/* network-visible interface */
#define ei_curaddr      ei_ac.ac_enaddr		/* current ethernet address */
#define	ei_addr		ei_eif.eif_addr		/* board's official address */
#define	ei_rawif	ei_eif.eif_rawif	/* raw domain interface */
#define	ei_lostintrs	ei_eif.eif_lostintrs	/* count lost interrupts */
#define	ei_resets	ei_eif.eif_resets	/* count board resets */
#define	ei_sick		ei_eif.eif_sick		/* 2 => reset on next bark */
#define	ei_opackets	ei_if.if_opackets	/* output packet count */
#define	ei_oerrors	ei_if.if_oerrors	/* output error count */
#define	eiftoei(eif)	((struct ec_info *)(eif)->eif_private)

/*
 * Receive buffer structure and initializer.
 */
struct rcvbuf {
	struct etherbufhead	ebh;
	char			data[MAX_RDAT];
};

/* defined in master.d/if_ec2 */
extern int	ec_oflow_limit;
extern int	ec_oflow_period;
extern int	ec_promisc_time;
extern int	ec_rcv_promisc;
extern int	ec_rcvint_delay;
extern int	ec_rcv_desc;
/* For bug 321638: more tx-desc; it decides the number of tx descriptors.  */
extern int	ec_tx_desc;		
/* For bug 321135: looping no tx-desc: threshold to reset the interface. */
extern int	ec_max_no_xd_cnt;	

/* tunable parameters defined in mtune/if_ec2 */
extern int	ec_ipg_delay;
extern int	ec_xpg_delay;
extern int	ec_16coll_retry;

extern u_int	enet_collision;

#ifdef notdef /* never used any more */
static int	ec_oflow_check(struct ec_info *);
static void	ec_oflow(struct ec_info *);
static void	ec_recover(struct ec_info *);
#endif

static void	ec_hang_recovery(struct ec_info *);
static void	ec_reset(struct etherif *eif);
static void	gio_ecintr(__psint_t, struct eframe_s *);
static int	if_ecintr(struct ec_info *);
static void	integral_ecintr(void);
static int	laf_hash(u_char *, int);
static void	seeq_init(struct seeqif *, int);
static int	seeq_ioctl(struct seeqif *, int, caddr_t);
#ifdef DEBUG
void	ec2_dump(struct ec_info *);
#endif
#ifdef IP22
static void	giogfx_ecintr(__psint_t, struct eframe_s *); /* Challenge S */
#endif

extern int	board_rev(void);
extern int	enet_carrier_on(void);
extern ushort	get_nvreg(int);
extern int	is_hp1(void);
extern void	qprintf(char *, ...);
extern void	reset_enet_carrier(void);

static int ps_enabled;
#define PS_ENABLED(ei)	(ps_enabled & (1 << (ei)->ei_unit))

#define MAX_NUM_RXD	63	/* max number of rcv buffer descriptors */

/* the following three defines are for bug 321638: more tx-desc */
#if _NO_UNCACHEDMEM_WAR && IP26
/* Do not need so many descriptors (which blow up to 128 bytes with war), but
 * eat the difference on IP28 as it's a more mainline product than IP26.
 */
#define	DEFAULT_NUM_TXD	63	/* number of xmit buffer descriptors */
#define	MAX_NUM_TXD	DEFAULT_NUM_TXD		/* same */
#define	MIN_NUM_TXD	DEFAULT_NUM_TXD		/* same */
#else
#define	DEFAULT_NUM_TXD	255	/* number of xmit buffer descriptors */
#define	MAX_NUM_TXD	((DEFAULT_NUM_TXD << 1) | 1) 	/* double */
#define	MIN_NUM_TXD	(DEFAULT_NUM_TXD >> 1)		/* half */
#endif

#if IP20
static char *nodma_mem = "ec%d: cannot allocate DMA'able memory for %s\n";
#define	DMA_MEM	0x10000000	/* DMA-able memory boundary */
#endif	/* IP20 */

#ifdef DEBUG
static u_int edbg = 0;		/* debugging flag */
#endif	/* DEBUG */

static int dma_copy = 0;	/* need copying before/after DMA, true on IP20
				   with > 128M of memory */

#define DBG_PRINT(a)	if (ei->ei_if.if_flags & IFF_DEBUG) \
				printf a

#if IP26 || IP28
#define RDDESC(rd)		((rd_desc *)K1_TO_K0(rd))
#define XDDESC(xd)		((xd_desc *)K1_TO_K0(xd))
#define FLUSHDESC(d)		__dcache_line_wb_inval(d)
#define PHYS_TO_KD(addr)	PHYS_TO_K0(addr)
#else
#define RDDESC(rd)		rd
#define XDDESC(xd)		xd
#define FLUSHDESC(d)
#define PHYS_TO_KD(addr)	PHYS_TO_K1(addr)
#endif

#if IP26
#define	GETRDDESC(k1,t)		((t) = *RDDESC(k1))
#define	GETXDDESC(k1,t)		((t) = *XDDESC(k1))
#else
#define	GETRDDESC(k1,t)		((t) = *(k1))
#define	GETXDDESC(k1,t)		((t) = *(k1))
#endif

#if IP26 || IP28
#define	PUTRDDESC(k1,t)		do { *RDDESC(k1) = t; FLUSHDESC(k1); } while (0)
#define	PUTXDDESC(k1,t)		do { *XDDESC(k1) = t; FLUSHDESC(k1); } while (0)
#else
#define	PUTRDDESC(k1,t)		(*(k1) = (t))
#define	PUTXDDESC(k1,t)		(*(k1) = (t))
#endif

#if IP28
#define	PUTRDDESCFIELD(k1,f,e)	do { rd_desc _t; GETRDDESC(k1,_t); _t.f = (e); PUTRDDESC(k1,_t); } while (0);
#define	PUTXDDESCFIELD(k1,f,e)	do { xd_desc _t; GETXDDESC(k1,_t); _t.f = (e); PUTXDDESC(k1,_t); } while (0);
#elif IP26
#define	PUTRDDESCFIELD(k1,f,e)	do { (RDDESC(k1)->f = (e)); FLUSHDESC(k1); } while (0);
#define	PUTXDDESCFIELD(k1,f,e)	do { (XDDESC(k1)->f = (e)); FLUSHDESC(k1); } while (0);
#else /* IP20/IP22 */
#define	PUTRDDESCFIELD(k1,f,e)	((k1)->f = (e))
#define	PUTXDDESCFIELD(k1,f,e)	((k1)->f = (e))
#endif


#ifdef notdef
#define START_RCV()	\
	if (ei->ei_rnum) { \
		hio->seeq_csr = ei->ei_mode | SEQ_RC_INTGOOD | SEQ_RC_INTEND | \
			SEQ_RC_INTSHORT | SEQ_RC_INTDRBL | SEQ_RC_INTCRC; \
		if (ei->ei_oflow_id) \
			untimeout(ei->ei_oflow_id); \
		ei->ei_oflow_id = dtimeout(ec_oflow, ei, HZ/100, plbase, cpuid()); \
		hio->rcvstat = HPC_STRCVDMA; \
	}
#endif
#define START_RCV()	\
	if (ei->ei_rnum) { \
		hio->seeq_csr = ei->ei_mode | SEQ_RC_INTGOOD | SEQ_RC_INTEND | \
			SEQ_RC_INTSHORT | SEQ_RC_INTDRBL | SEQ_RC_INTCRC; \
		hio->rcvstat = HPC_STRCVDMA; \
		hio->seeq_csr = ei->ei_mode | SEQ_RC_INTGOOD | SEQ_RC_INTEND | \
		SEQ_RC_INTSHORT | SEQ_RC_INTDRBL | SEQ_RC_INTCRC | SEQ_RC_OFLOW; \
	}
	
#define CHECK_DMA() \
	if (!(hio->rcvstat & HPC_STRCVDMA) \
	    && ei->ei_if.if_flags & IFF_DEBUG) { \
		printf("ec%d: CHECK_DMA() receive dma stopped, rstat = 0x%x, crbdp = 0x%x\n", \
			ei->ei_unit, (hio->rcvstat >> RCVSTAT_SHIFT) & 0xff, \
			PHYS_TO_K1(ENET_READ(hio->crbdp))); \
	}

/*
 * Initialize the chip.  Interrupts must already be off.
 */
static int				/* return 0 or an errno */
ec_init(
	struct etherif *eif,
	int flags)
{
	rd_desc *rd_tail;
	register struct ec_info *ei;
	EHIO hio;
	int unit;

	ei = eiftoei(eif);
	hio = ei->ei_io;
	unit = ei->ei_unit;

	if (eif->eif_resets == 0) {
		if (unit == 0) {
			setlclvector(VECTOR_ENET, (void(*)())integral_ecintr, 0);
		}
#ifdef IP22
		else if (unit == 3)
			setgiogfxvec(2, giogfx_ecintr, 0);
#endif /* IP22 */
		else
			setgiovector(GIO_INTERRUPT_1, unit - 1, 
				gio_ecintr, 0);
	}

	if (++eif->eif_resets == 0)
		++eif->eif_resets;

	if (ei->ei_efix || flags & IFF_PROMISC)
		ei->ei_num_rxd = ec_rcv_promisc;
	else
		ei->ei_num_rxd = ec_rcv_desc;

	ec_reset(eif);
	ei->ei_sick = 0;

        rd_tail = (rd_desc *)K1_TO_K0(ei->ei_rtail);
        while (ei->ei_rnum < ei->ei_num_rxd) {
                struct rcvbuf *newrb;
		struct mbuf *m;
                rd_desc *rd;

                m = m_vget(M_DONTWAIT, sizeof(struct rcvbuf), MT_DATA);
                if (m == 0) {
                        DBG_PRINT(("ec%d: out of mbufs, cnt = %d\n", ei->ei_unit, ei->ei_rnum));
                        break;
                }
                ei->ei_rnum++;

                /*
                 * initialize new receive descriptor and receive
                 * buffer with an ifheader at the front of it.
                 */
                newrb = mtod(m, struct rcvbuf *);
                IF_INITHEADER(newrb, &ei->ei_if, sizeof(struct etherbufhead));

                rd = (rd_desc *)PHYS_TO_K0(rd_tail->r_nrdesc);
                rd->r_rbcnt = MAX_RPKT;
		if (!dma_copy) {
                	rd->r_rbufptr = K_TO_PHYS(newrb->ebh.ebh_pad);
		}
		rd->r_mbuf = m;
		((rd_desc *)PHYS_TO_K0(rd->r_nrdesc))->r_rown = 1;
		rd->r_eor = 1;
		rd_tail->r_eor = 0;

		dki_dcache_wbinval((void *)newrb->ebh.ebh_pad, MAX_RPKT);

                rd_tail = rd;
        }
        ei->ei_rtail = (rd_desc *)K0_TO_K1(rd_tail);
	dki_dcache_wbinval((void *)K1_TO_K0(ei->ei_rmdvec),
		sizeof(rd_desc_layout) * (MAX_NUM_RXD + 1));

	/*
	 * setup seeq address and mode
	 */
	seeq_init(&ei->ei_sif, flags);

#if IP20
	hio->cxbdp = K1_TO_PHYS(ei->ei_ttail);	/* for freeing xmit desc. */
#endif	/* IP20 */
	hio->nxbdp = K1_TO_PHYS(ei->ei_tact);

	hio->crbdp = K1_TO_PHYS(ei->ei_rtail);	/* for ec_watchdog */
	hio->nrbdp = K1_TO_PHYS(ei->ei_ract);

	START_RCV();

	ei->ei_if.if_timer = EDOG;	/* turn on watchdog */

	/* RSVP.  Update the packet scheduler with our txqlen status */
	if (PS_ENABLED(ei))
		ps_txq_stat(eiftoifp(&ei->ei_eif), ei->ei_tfreecnt >> 2);

	return 0;
}

#ifdef notdef	/* never used any more */
static void
ec_oflow(register struct ec_info *ei)
{
	IFNET_LOCK(eiftoifp(&ei->ei_eif));
	ei->ei_oflow_id = 0;
	ei->ei_io->seeq_csr = ei->ei_mode | SEQ_RC_INTGOOD | SEQ_RC_INTEND |
		SEQ_RC_INTSHORT | SEQ_RC_INTDRBL | SEQ_RC_INTCRC | SEQ_RC_OFLOW;
	IFNET_UNLOCK(eiftoifp(&ei->ei_eif));
}
#endif

static void
seeq_init(struct seeqif *sif, int flags)
{
	int i;
	register struct ec_info *ei;
	EHIO hio;

	ei = (struct ec_info *)sif;
	hio = ei->ei_io;
	hio->seeq_csx = SEQ_XC_REGBANK0;

#ifdef IP22
	/* If Challenge S SCSI/Ethernet card, initialize dmacfg and piocfg */
	if (ei->ei_unit == 3) {
		hio->dmacfg = (HPC_DMA_D1 << HPC_DMA_D1_SHIFT)
				| (HPC_DMA_D2 << HPC_DMA_D2_SHIFT)
				| (HPC_DMA_D3 << HPC_DMA_D3_SHIFT)
				| (HPC_DMA_TIMEOUT << HPC_DMA_TIMEOUT_SHIFT);
		hio->piocfg = (HPC_PIO_P1 << HPC_PIO_P1_SHIFT)
				| (HPC_PIO_P2 << HPC_PIO_P2_SHIFT)
				| (HPC_PIO_P3 << HPC_PIO_P3_SHIFT);
	}
#endif
#define	sif_eaddr	sif->sif_eif.eif_arpcom.ac_enaddr

	for (i = 0; i < 6; i++)
		hio->seeq_eaddr[i] = sif_eaddr[i];

	ei->ei_eaddr_a = (sif_eaddr[0] << 8) | sif_eaddr[1];
	ei->ei_eaddr_b = (sif_eaddr[2] << 24) | (sif_eaddr[3] << 16) |
		(sif_eaddr[4] << 8) | sif_eaddr[5];

#if IP22 || IP26 || IP28
	ei->ei_srcaddr_a = (sif_eaddr[0] << 24) | (sif_eaddr[1] << 16) |
		(sif_eaddr[2] << 8) | sif_eaddr[3];
	ei->ei_srcaddr_b = (sif_eaddr[4] << 8) | sif_eaddr[5];
#endif	/* IP22 || IP26 || IP28 */

#undef	sif_eaddr

#if IP20
	if (ei->ei_efix)
		ei->ei_mode = SEQ_RC_RALL;
	else		/* next line must be following if stmt */
#endif
	    if (flags & IFF_PROMISC)
		ei->ei_mode = SEQ_RC_RALL;
	else if (flags & (IFF_ALLMULTI | IFF_FILTMULTI)) {
		ei->ei_mode = SEQ_RC_RSMB;
		if (ei->ei_sgi_edlc) {
			if (flags & IFF_ALLMULTI) {
				/* disable hash filtering */
				ei->ei_seeq_ctl &= ~SEQ_CTL_MULTI;
			} else {
				/* enable hash filtering */
				ei->ei_seeq_ctl |= SEQ_CTL_MULTI;
				hio->seeq_csx = SEQ_XC_REGBANK1;
				for (i = 0; i < 6; i++)
					hio->seeq_mcast_lsb[i] =
						sif->sif_laf.laf_vec[i];
				hio->seeq_csx = SEQ_XC_REGBANK2;
				hio->seeq_mcast_msb[0] =
					sif->sif_laf.laf_vec[6];
				hio->seeq_mcast_msb[1] =
					sif->sif_laf.laf_vec[7];
			}
		}
	} else
		ei->ei_mode = SEQ_RC_RSB;
		
	if (ei->ei_sgi_edlc) { 		/* enable new features */
		u_int ipgval;

		hio->seeq_csx = SEQ_XC_INTGOOD | SEQ_XC_INT16TRY |
			SEQ_XC_INTCOLL | SEQ_XC_INTUFLOW | SEQ_XC_REGBANK2;
		hio->seeq_ctl = ei->ei_seeq_ctl;
		
		if (ec_ipg_delay <= 0)		/* default, "normal" */
			ipgval = 0;
		else {
			/* ipg goes up in 1.6us intvl, add 8 to rnd */
			ec_ipg_delay = (ec_ipg_delay * 10 + 8) / 16;
			ec_ipg_delay = (ec_ipg_delay > 15) ? 15 : ec_ipg_delay;
			ipgval = ((~ec_ipg_delay + 1) << 4) & 0xff;
		}
		hio->seeq_pktgap = ipgval;

	} else
		hio->seeq_csx = SEQ_XC_INTGOOD | SEQ_XC_INT16TRY |
			SEQ_XC_INTCOLL | SEQ_XC_INTUFLOW;

#if IP20
	hio->intdelay = HPC_DELAY_EXPIRED;
#endif	/* IP20 */
#if IP22 || IP26 || IP28
	/* enable new features in HPC3 */
	hio->dmacfg |= HPC_FIX_INTR | HPC_FIX_EOP | HPC_FIX_RXDC;
#endif	/* IP22 || IP26 || IP28 */
}

/*
 * Reset the interface. Must be done at splimp level.
 */
static void
ec_reset(struct etherif *eif)
{
	int i;
	struct mbuf *m;
	struct mbuf *t_mbuf;
	rd_desc_layout *rdl;
	xd_desc_layout *xdl;
	rd_desc *rd;
	register struct ec_info *ei;
	EHIO hio;
	
	eiftoifp(eif)->if_timer = 0;	/* turn off watchdog */
	ei = eiftoei(eif);
	hio = ei->ei_io;

	if (ei->ei_hang_id) {
		untimeout(ei->ei_hang_id);
		ei->ei_hang_id = 0;
	}

	ei->ei_no_xd_cnt = 0;		/* Bug 321135: looping no tx-desc */

	ei->ei_xd16coll = 0;
	ei->ei_try16coll = 0;

	/* reset seeq & hpc */
	hio->rcvstat = 0;
	hio->trstat = 0;

	hio->ctl = HPC_MODNORM | HPC_ERST | HPC_INTPEND;
	DELAY(10);
	hio->ctl = HPC_MODNORM;

	/* reset receive descriptor ring */
	if (ei->ei_rnum) {
		struct rcvbuf *rb;

		rd = (rd_desc *)K1_TO_K0(ei->ei_ract);
		for (i = 0; i < ei->ei_rnum; i++) {
			m = rd->r_mbuf;
			GOODMP(m);
			GOODMT(m->m_type);
			rd->r_rown = 1;
			rd->r_rbcnt = MAX_RPKT;
			rb = mtod(m, struct rcvbuf *);
			if (!dma_copy) {
                		rd->r_rbufptr = K_TO_PHYS(rb->ebh.ebh_pad);
			}
			rd->r_eor = 0;
			rd = (rd_desc *)PHYS_TO_K0(rd->r_nrdesc);
		}
		ASSERT(rd == (rd_desc *)PHYS_TO_K0(ei->ei_rtail->r_nrdesc));
		rd->r_rown = 1;
		((rd_desc *)K1_TO_K0(ei->ei_rtail))->r_eor = 1;

	} else {
		ei->ei_ract = (rd_desc *)ei->ei_rmdvec;
		rdl = (rd_desc_layout *)K1_TO_K0(ei->ei_ract);

		for (i = 0; i <= MAX_NUM_RXD; i++) {
			rdl->rd.r_rown = 1;
			rdl->rd.r_eor = 1;
#if IP22 || IP26 || IP28
			rdl->rd.r_eorp = 1;
			rdl->rd.r_xie = 1;
#endif	/* IP22 || IP26 || IP28 */
			rdl++;
		}

        	ei->ei_rtail = (rd_desc *)K0_TO_K1(--rdl);   /* includes sentinal */
	}

	dki_dcache_wbinval((void *)K1_TO_K0(ei->ei_rmdvec),
		sizeof(rd_desc_layout) * (MAX_NUM_RXD + 1));

	/* free the transmit mbufs */
	t_mbuf = ei->ei_tmbuf_head;
	while (t_mbuf) {
		m = t_mbuf;
		GOODMP(m);
		t_mbuf = m->m_act;
		m_freem(m);
	}
	ei->ei_tmbuf_head = ei->ei_tmbuf_tail = 0;

	/* create xmit descriptor ring */
	ei->ei_tact = (xd_desc *)ei->ei_tmdvec;
	xdl = (xd_desc_layout *)K1_TO_K0(ei->ei_tact);

	for (i = 0; i <= ec_tx_desc; i++) {
		xdl->xd.x_inuse = 0;
		xdl->xd.x_mbuf = 0;
		xdl->xd.x_eox = 1;
#if IP22 || IP26 || IP28
		xdl->xd.x_txd = 1;
#endif	/* IP22 || IP26 || IP28 */
		xdl++;
	}

	ei->ei_ttail = (xd_desc *)K0_TO_K1(--xdl);
	ei->ei_tfreecnt = ec_tx_desc;	

	dki_dcache_wbinval((void *)K1_TO_K0(ei->ei_tmdvec),
                sizeof(xd_desc_layout) * (MAX_NUM_TXD + 1));

        consistancy_check_eth((__psunsigned_t) K1_TO_K0(ei->ei_rmdvec), (__psunsigned_t) sizeof(rd_desc_layout) * (MAX_NUM_RXD + 1), "ec_resetr", CCF_REQUIRE_NODIRTY);

}

/*
 * Periodically poll the controller for input packets in case
 * an interrupt gets lost.
 */
/* ARGSUSED */
static void
ec_watchdog(struct ifnet *ifp)
{
#ifdef DEBUG
	rd_desc *rd;
#endif
	struct ec_info *ei = eiftoei(ifptoeif(ifp));
	EHIO hio;

	hio = ei->ei_io;

        ASSERT(IFNET_ISLOCKED(ifp));

#ifdef DEBUG
	ei->ei_watchdog_cnt++;

	/* this happens only when we have bad hardware */
	rd = ei->ei_ract;
	if (ei->ei_rnum &&
	    (K1_TO_PHYS(rd) != (ENET_READ(hio->nrbdp) & ~0xf)) &&
	    (K1_TO_PHYS(rd) != (ENET_READ(hio->crbdp) & ~0xf)) &&
	    rd->r_rown) {
		DBG_PRINT(("ec%d: enet recovered A\n", ei->ei_unit));
		(void) ec_init(&ei->ei_eif, ifp->if_flags);
	}
#endif	/* DEBUG */

	if (!(hio->rcvstat & HPC_STRCVDMA) 
	    && ((rd_desc *)PHYS_TO_K1(ENET_READ(hio->crbdp) & ~0xf) == ei->ei_rtail)
	    && ei->ei_rnum) {
		DBG_PRINT(("ec%d: enet recovered B\n", ei->ei_unit));
		(void) ec_init(&ei->ei_eif, ifp->if_flags);
	}

	/*
	 * ifdef out this code since this may happen under normal condition -
	 * start transmit DMA right before the watchdog timer expires and
	 * collisions occur on the ethernet when we try to transmit the
	 * first packet in the chain
	 */
#ifdef NOTDEF
	if ((ei->ei_tfreecnt < NUM_TMD) &&
	    (K1_TO_PHYS(ei->ei_tact) == (ENET_READ(hio->nxbdp) & ~0xf))) {
		DBG_PRINT(("ec%d: enet recovered C\n", ei->ei_unit));
		(void) ec_init(&ei->ei_eif, ifp->if_flags);
	}
#endif	/* NOTDEF */

	if (!(hio->rcvstat & HPC_STRCVDMA)) {
		DBG_PRINT(("ec%d: enet recovered D\n", ei->ei_unit));
#ifdef DEBUG
		if (edbg)
			ec2_dump(ei);
#endif	/* DEBUG */
		(void) ec_init(&ei->ei_eif, ifp->if_flags);
	}

	if (if_ecintr(ei))
		ei->ei_lostintrs++;

	ifp->if_timer = EDOG;
}

/*
 * If possible, start transmit DMA
 */
static void 
kickstart_dma(struct ec_info *ei, EHIO hio)
{
        xd_desc *xd_cur;
        u_int xstat;
	
        xstat = hio->trstat;
	if (xstat & HPC_STTRDMA)
		return;
	if (!(xstat & SEQ_XS_SUCCESS)) {        /* error */
		#pragma mips_frequency_hint NEVER
		if (xstat & SEQ_XS_16TRY) {
#if IP22 || IP26 || IP28
			u_int xdphys_cur;
			xdphys_cur = ENET_READ(hio->cpfxbdp) & ~0xf;
			xd_cur = (xd_desc *)PHYS_TO_K1(xdphys_cur);
			if (xdphys_cur != ei->ei_xd16coll) {
                                ei->ei_try16coll = ec_16coll_retry;
                                ei->ei_xd16coll = xdphys_cur;
			}
			if (ei->ei_try16coll-- > 0 &&
			    xd_cur->x_inuse && !xd_cur->x_txd) {
                                hio->nxbdp = xdphys_cur;
                                DBG_PRINT(("ec%d: 16 xmit collisions, retry %d\n", ei->ei_unit, ec_16coll_retry - ei->ei_try16coll));
			} else
				/* next stmt is DBG_PRINT */
#endif  /* IP22 || IP26 || IP28 */
				DBG_PRINT(("ec%d: 16 xmit collisions\n", ei->ei_unit));
                }
		if (xstat & SEQ_XS_UFLOW) {
			ei->ei_if.if_oerrors++;
                        METER(ec_ostat_uflow[ei->ei_unit]++);
                        DBG_PRINT(("ec%d: transmit underflow\n", ei->ei_unit));
                }
#if IP22 || IP26 || IP28
                if (xstat & SEQ_XS_LATE_COLL) {
                        METER(ec_ostat_late_coll[ei->ei_unit]++);
                        DBG_PRINT(("ec%d: late xmit collision\n", ei->ei_unit));
                }
#endif  /* IP22 || IP26 || IP28 */
	}

#if IP20
	xd_cur = (xd_desc *)PHYS_TO_K1(hio->cxbdp & ~0xf);
	if (xd_cur->x_inuse) {
		while (!xd_cur->x_eoxp)
			xd_cur = (xd_desc *)PHYS_TO_K1(xd_cur->x_nxdesc);
	}
	xd_cur = (xd_desc *)PHYS_TO_K1(xd_cur->x_nxdesc);
	while (!xd_cur->x_inuse && xd_cur != ei->ei_ttail)
		xd_cur = (xd_desc *)PHYS_TO_K1(xd_cur->x_nxdesc);
	if (!xd_cur->x_inuse) 
		xd_cur = (xd_desc *)PHYS_TO_K1(ei->ei_ttail->x_nxdesc);

	hio->nxbdp = K1_TO_PHYS(xd_cur);
        hio->cxbdp = K1_TO_PHYS(xd_cur);

	if (xd_cur->x_inuse) {  /* need to kickstart the hpc */
		hio->cpfxbdp = K1_TO_PHYS(xd_cur);

		if (ec_xpg_delay > 0)
			us_delay(ec_xpg_delay);

		hio->trstat = HPC_STTRDMA;
	}
#endif  /* IP20 */
	
#if IP22 || IP26 || IP28
	xd_cur = (xd_desc *)PHYS_TO_K1(ENET_READ(hio->nxbdp) & ~0xf);
	while (xd_cur->x_inuse && xd_cur->x_txd)
		xd_cur = (xd_desc *)PHYS_TO_K1(xd_cur->x_nxdesc);
	
	if (xd_cur->x_inuse) {  /* need to kickstart the hpc */
		hio->nxbdp = K1_TO_PHYS(xd_cur);
		
		if (ec_xpg_delay > 0)           /* hack for customer */
			us_delay(ec_xpg_delay);

		hio->trstat = HPC_STTRDMA;
	}
#endif  /* IP22 || IP26 || IP28 */
}

/*
 * Check carrier state
 */
static void
check_carrier(struct ec_info *ei, EHIO hio)
{
	int no_carrier;
	switch (ei->ei_c_detect) {
	case 0:
		no_carrier = 0;
		break;
	case 1:
		no_carrier = !enet_carrier_on();
		break;
	case 2:
		no_carrier= hio->seeq_flags & SEQ_XS_NO_CARRIER;
		if ( no_carrier )
			no_carrier = no_carrier && !ei->ei_collisions;
		break;
	default:
		panic("impossible enet configuration");
	}
	
	if (no_carrier) {
		#pragma mips_frequency_hint NEVER
		if (ei->ei_carr_msg_lbolt <= lbolt) {
			cmn_err_tag(211,CE_ALERT, "ec%d: no carrier: check Ethernet cable\n", ei->ei_unit);
			ei->ei_carr_msg_lbolt = lbolt + EMSG_DELAY;
		}
		
		if (ei->ei_carrier_was_up) {
			ei->ei_carr_opkts = ei->ei_opackets;
			ei->ei_oerrors++;
			ei->ei_carrier_was_up = 0;
			
		} else {
			ei->ei_oerrors += ei->ei_opackets - ei->ei_carr_opkts;
			ei->ei_carr_opkts = ei->ei_opackets;
		}
		
		if (ei->ei_c_detect == 1)
			reset_enet_carrier();
		else {          /* == 2 */
			hio->seeq_ctl = ei->ei_seeq_ctl &
				~SEQ_CTL_NOCARR;
			hio->seeq_ctl = ei->ei_seeq_ctl;
		}
	} else
		ei->ei_carrier_was_up = 1;
	
	/*
	 * XXX SQE collision counting hack.  if ever the number
	 * of collisions after the adjustment is negative, then
	 * SQE must be off.  we check the SQE here since we
	 * know the transmit queue is completely empty and,
	 * thus, the result should be more reliable.
	 */
	if (!ei->ei_sgi_edlc && ei->ei_sqe_on
	    && (ei->ei_if.if_collisions - ei->ei_coll_adj) <= 0)
		ei->ei_sqe_on = 0;
}


/*
 * Clean any transmitted packets of the xd ring
 * returns 1 if some descriptors made available
 * returns >= 2 if all descriptors were made available
 */
static int 
clean_xd_chain(struct ec_info *ei, EHIO hio)
{
        xd_desc *xd_cur, *xd_prev;
        xd_desc *xd_chain;
        int status = 0;
        struct mbuf *mhead, *n;
	u_int xstat;

        if (ei->ei_tfreecnt == ec_tx_desc)
                return 0;

	xstat = hio->trstat;

	if (xstat & HPC_STTRDMA) {
        	xd_cur = (xd_desc *)PHYS_TO_K1(ENET_READ(hio->cpfxbdp) & ~0xf);
        	xd_prev = (xd_desc *)PHYS_TO_K1(ENET_READ(hio->ppfxbdp) & ~0xf);
	}
	else
		xd_cur = xd_prev = (xd_desc *)PHYS_TO_K1(ENET_READ(hio->nxbdp) & ~0xf);

        mhead = ei->ei_tmbuf_head;
        xd_chain = ei->ei_tact;

#ifdef DEBUG    /* dwong */
        _ecintr_xd_cur1[ei->ei_unit] = (__psunsigned_t)xd_cur;
        _ecintr_xd_prev1[ei->ei_unit] = (__psunsigned_t)xd_prev;
        _ecintr_xd_tact1[ei->ei_unit] = (__psunsigned_t)xd_chain;
#endif  /* DEBUG */

        while (xd_chain != xd_cur && xd_chain != xd_prev) {
                xd_desc _xd;
		
                if (!xd_chain->x_inuse) {
#ifdef DEBUG
                        if (xstat & HPC_STTRDMA) {
                                printf("ec0: transmit DMA left on with no packets!\n");
                                METER(ec_trdma_on[ei->ei_unit]++);
                        }
#endif
                        hio->trstat = 0;
                        hio->nxbdp = K1_TO_PHYS(xd_chain);
                        break;
                }
		
                n = mhead;
                if (xd_chain->x_mbuf != n) {
			#pragma mips_frequency_hint NEVER
                        (void)ec_init(&ei->ei_eif, ei->ei_if.if_flags);
#ifdef DEBUG
                        printf("ec0: mbuf heads out of sync on xd ring!\n");
                        ei->ei_xmit_sync_cnt++;
#endif  /* DEBUG */
                        return 0;
                }
		
                PUTXDDESCFIELD(xd_chain,x_mbuf,0);
                mhead = mhead->m_act;
		
                if (n == ei->ei_tmbuf_tail) {
			
                        /* For bug 331579: reset ei_tmbuf_tail.
                         * no more packets so mhead should be NULL
                         */
                        if( mhead != NULL ){
				#pragma mips_frequency_hint NEVER
                                DBG_PRINT( ("!ec%d: if_ecintr(): !mhead(0x%x).\n",
					    ei->ei_unit, mhead));
                        }
			
                        mhead = NULL;
                        ei->ei_tmbuf_tail = NULL;
			status |= 2;
			
                }
		
                m_freem(n);             /* free the mbufs */
                while (xd_chain) {
                        GETXDDESC(xd_chain,_xd);
                        ASSERT(_xd.x_inuse);
                        _xd.x_inuse = 0;
                        _xd.x_mbuf = 0;
                        _xd.x_eox = 1;
#if IP22 || IP26 || IP28
                        _xd.x_txd = 1;

                        if ((u_int)K1_TO_PHYS(xd_chain) == ei->ei_xd16coll)
                                ei->ei_xd16coll = ei->ei_try16coll = 0;
#endif  /* IP22 || IP26 || IP28 */
                        PUTXDDESC(xd_chain,_xd);
                        ei->ei_tfreecnt++;
                        ASSERT(ei->ei_tfreecnt <= ec_tx_desc);
                        if (xd_chain->x_eoxp)
                                break;
                        xd_chain = (xd_desc *)PHYS_TO_K1(_xd.x_nxdesc);
                }
                xd_chain = (xd_desc *)PHYS_TO_K1(_xd.x_nxdesc);
                status |= 1;
        }

#ifdef DEBUG    /* dwong */
        _xmit_xd_cur0[ei->ei_unit] = _xmit_xd_cur1[ei->ei_unit];
        _xmit_xd_prev0[ei->ei_unit] = _xmit_xd_prev1[ei->ei_unit];
        _xmit_xd_tact0[ei->ei_unit] = _xmit_xd_tact1[ei->ei_unit];
#endif  /* DEBUG */

        ei->ei_tact = xd_chain;
        ei->ei_tmbuf_head = mhead;
#ifdef NOTDEF
        __dcache_wb_inval((void *)K1_TO_K0(ei->ei_tmdvec),
                sizeof(xd_desc_layout) * (MAX_NUM_TXD + 1));
#endif

        return status;
}

static void
integral_ecintr(void)
{
	IFNET_LOCK(eiftoifp(&ec_info[0].ei_eif));
	if_ecintr(&ec_info[0]);
	IFNET_UNLOCK(eiftoifp(&ec_info[0].ei_eif));
}

/* ARGSUSED */
static void
gio_ecintr(__psint_t x, struct eframe_s *y)
{
	if (ec_info[1].ei_io && (ec_info[1].ei_io->ctl & HPC_INTPEND)) {
		IFNET_LOCK(eiftoifp(&ec_info[1].ei_eif));
		if_ecintr(&ec_info[1]);
		IFNET_UNLOCK(eiftoifp(&ec_info[1].ei_eif));
	}

	if (ec_info[2].ei_io && (ec_info[2].ei_io->ctl & HPC_INTPEND)) {
		IFNET_LOCK(eiftoifp(&ec_info[2].ei_eif));
		if_ecintr(&ec_info[2]);
		IFNET_UNLOCK(eiftoifp(&ec_info[2].ei_eif));
	}
}

#ifdef IP22
/*
 * Support the Challenge S Ethernet as unit number 3
 */
/* ARGSUSED */
static void
giogfx_ecintr(__psint_t x, struct eframe_s *y)
{
	IFNET_LOCK(eiftoifp(&ec_info[3].ei_eif));
	if_ecintr(&ec_info[3]);
	IFNET_UNLOCK(eiftoifp(&ec_info[3].ei_eif));
}
#endif /* IP22 */

/*
 * generic ethernet interface interrupt.  Come here at splimp().
 */
static int
if_ecintr(register struct ec_info *ei)
{
	int bcnt, found = 0, obcnt;
	rd_desc *rd_chain, *rd_tail, *rd_crbdp, *save_rtail;
	u_char rstat;
	struct mbuf *m;
	EHIO hio = ei->ei_io;
	rd_desc *rd, _rd;
#if IP22 || IP26 || IP28
	u_int xdphys_cur;
#endif
	u_int save_rstatus;
	int ps_more;

        ASSERT(IFNET_ISLOCKED(eiftoifp(&ei->ei_eif)));
	/* ignore early interrupts */
	if (ei->ei_curaddr[0] == 0 && ei->ei_curaddr[1] == 0 &&
	    ei->ei_curaddr[2] == 0 && ei->ei_curaddr[3] == 0 &&
	    ei->ei_curaddr[4] == 0 && ei->ei_curaddr[5] == 0 ) {
		#pragma mips_frequency_hint NEVER
		ether_stop(&ei->ei_eif, ei->ei_if.if_flags);	
		DBG_PRINT(("ec%d: early interrupt\n", ei->ei_unit));
		return 0;
	}

	if (iff_dead(ei->ei_if.if_flags)) {	/* if down don't process */
		#pragma mips_frequency_hint NEVER
		ether_stop(&ei->ei_eif, ei->ei_if.if_flags);
		DBG_PRINT(("ec%d: interrupt during down\n", ei->ei_unit));
		return 0;
	}

#ifdef notdef
	CHECK_DMA();
#endif

	if (hio->ctl & HPC_INTPEND)			/* intr pending? */
		hio->ctl = HPC_MODNORM | HPC_INTPEND;

	/*
	 * process receive packets
	 */

	bcnt = ei->ei_rnum;
	rd_chain = ei->ei_ract;

	while (!rd_chain->r_rown && bcnt) {
		struct ether_header *eh;
		int rem, rlen, snoopflags = 0;
		struct rcvbuf *rbuf;

		ei->ei_if.if_ipackets++;

		m = rd_chain->r_mbuf;
		GOODMP(m);
		GOODMT(m->m_type);
#ifndef _NO_UNCACHEDMEM_WAR
		/* r_mbuf == 0 is never checked and cacheop is be expensive */
		PUTRDDESCFIELD(rd_chain,r_mbuf,0);
#endif
		rbuf = mtod(m, struct rcvbuf *);
		eh = &rbuf->ebh.ebh_ether;

		rem = rd_chain->r_rbcnt;
		rlen = MAX_RPKT - rem - 3;
		consistancy_check_eth(rd_chain->r_rbufptr, rlen+3,
				      "if_ecintr", CCF_REQUIRE_NODIRTY);
		if (rlen <= 0) {
			#pragma mips_frequency_hint NEVER
			DBG_PRINT(("ec%d: receive indicates impossible length packet\n", ei->ei_unit));
			m_freem(m);
			goto next_pkt;
		}

#if R10000_SPECULATION_WAR
		/* Ensure the mbuf data is not speculatively cached */
		__dcache_inval(rbuf, MAX_RPKT);
#endif

#if IP20
		if (dma_copy) {
			bcopy((void *)PHYS_TO_K0(rd_chain->r_rbufptr),
				rbuf->ebh.ebh_pad, rlen + 3);
			dki_dcache_inval((void *)PHYS_TO_K0(rd_chain->r_rbufptr),
				rlen + 3);
		}
#endif	/* IP20 */

		/* XXX - promiscuous hack
		 * For efficiency reasons, this code has a dependency on the
		 * packet alignment.  Fortunately, it is also enforced by hpc.
		 */
		if (ei->ei_efix && rlen > 5
		    && !(ei->ei_if.if_flags & IFF_PROMISC)) {
			if (!((*(u_int *)((char *)eh + 2) == ei->ei_eaddr_b)
			    && (*(u_short *)eh == ei->ei_eaddr_a))
			    && !ETHER_ISBROAD(eh->ether_dhost)
			    && !((ei->ei_if.if_flags & (IFF_FILTMULTI|IFF_ALLMULTI))
				  && ETHER_ISMULTI(eh->ether_dhost))) {

				m_freem(m);
				goto next_pkt;
			}
		}

#if IP22 || IP26 || IP28
		/* filter out unicast/broadcast/multicast packet from self */
		{
			if ((ETHER_ISBROAD(eh->ether_dhost)
			    || ETHER_ISMULTI(eh->ether_dhost)
			    || ei->ei_unicast_counter)
			    && (*(u_int *)((char *)eh + 6) == ei->ei_srcaddr_a
			    && *(u_short *)((char *)eh + 10) == ei->ei_srcaddr_b)) {
				m_freem(m);
				if (ei->ei_unicast_counter) {
					ei->ei_unicast_counter--;
					if (!ei->ei_unicast_counter)
						untimeout(ei->ei_unicast_id);
				}
				goto next_pkt;
			}
		}
#endif	/* IP22 || IP26 || IP28 */

		rstat = (rlen > 2) ? *((caddr_t)eh + rlen) : SEQ_RS_SHORT;
			
		if (rlen < EHDR_LEN) {
			int i;

			snoopflags = SN_ERROR|SNERR_TOOSMALL;
			for (i = rlen; i < EHDR_LEN; i++)
				*((char *)eh + i) = 0;
			rlen = EHDR_LEN;
		}

		if (!(rstat & SEQ_RS_GOOD)) {	/* map error state */
			#pragma mips_frequency_hint NEVER
#if IP22 || IP26 || IP28
			if ((rstat & SEQ_RS_LATE_RXDC)
			    && !(rstat & SEQ_RS_STATUS_5_0)) {
				DBG_PRINT(("ec%d: late receive discard, rstat = 0x%x\n", ei->ei_unit, rstat));
				METER(ec_istat_rxdc[ei->ei_unit]++);
				m_freem(m);
				goto next_pkt;
			}
			if ((rstat & SEQ_RS_TIMEOUT)
			    && ((rstat & SEQ_RS_STATUS_5_0) == SEQ_RS_END)) {
				DBG_PRINT(("ec%d: receive timeout, rstat = 0x%x\n", ei->ei_unit, rstat));
				METER(ec_istat_timeout[ei->ei_unit]++);
				m_freem(m);
				goto next_pkt;
			}
#endif	/* IP22 || IP26 || IP28 */

			snoopflags = SN_ERROR;
			ei->ei_if.if_ierrors++;

			if (rstat & SEQ_RS_SHORT) {
				snoopflags |= SNERR_TOOSMALL;
				METER(ec_istat_small[ei->ei_unit]++);
				ei->ei_if.if_ierrors--;	/* don't count as err */
#ifdef notdef
				DBG_PRINT(("ec%d: packet too small(%d)\n", ei->ei_unit, rlen));
#endif	/* notdef */

			} else if (rstat & SEQ_RS_DRBL) {
				snoopflags |= SNERR_FRAME;
				METER(ec_istat_frame[ei->ei_unit]++);
				DBG_PRINT(("ec%d: packet framing error\n", ei->ei_unit));

			} else if (rstat & SEQ_RS_CRC)  {
				snoopflags |= SNERR_CHECKSUM;
				METER(ec_istat_crc[ei->ei_unit]++);
				DBG_PRINT(("ec%d: CRC error\n", ei->ei_unit));

			} else if (rstat & SEQ_RS_OFLOW)  {
				snoopflags |= SNERR_OVERFLOW;
				METER(ec_istat_oflow[ei->ei_unit]++);
				DBG_PRINT(("ec%d: receive packet overflow\n", ei->ei_unit));

			}
#ifdef notdef
			  else
				printf("ec%d: unknown recv error packet len = %d stat = 0x%x\n", ei->ei_unit, rlen, rstat);
#endif	/* notdef */
		}

		ei->ei_if.if_ibytes += rlen;

		m_adj(m, rlen - MAX_RPKT);
		ether_input(&ei->ei_eif, snoopflags, m);

next_pkt:
		bcnt--;
#ifdef NOTDEF
		rd_chain->r_rown = 1;
		if (rd_chain->r_eor)		/* sanity check */
			PUTRDDESCFIELD(rd_chain->r_nrdesc,r_rown,1);
#endif	/* NOTDEF */

		rd_chain = (rd_desc *)PHYS_TO_K1(rd_chain->r_nrdesc);
	}
	ei->ei_ract = rd_chain;			/* from beginning of chain */

	/* recvd a pckt, delay next intrpt */
	if (bcnt < ei->ei_rnum) {
		ei->ei_if.if_timer = EDOG;	/* postpone watchdog */
		if (ei->ei_hang_id) {		/* enet still alive */
			untimeout(ei->ei_hang_id);
			ei->ei_hang_id = 0;
		}
		found = 1;
		ei->ei_sick = 0;
#if IP20
		if (ei->ei_efix)
			hio->intdelay = ec_rcvint_delay;
#endif	/* IP20 */
	}
	ei->ei_rnum = bcnt;

	/*
	 * save stopped state for use below
	 */
	if (!((save_rstatus = hio->rcvstat) & HPC_STRCVDMA)) {
		rd_crbdp = (rd_desc *)PHYS_TO_K1(ENET_READ(hio->crbdp) & ~0xf);
		save_rtail = ei->ei_rtail;
	}

	/*
	 * allocate new mbufs for the seeq to receive into
	 */
	obcnt = bcnt;
	rd_tail = ei->ei_rtail;

	rd = (rd_desc *)PHYS_TO_K1(rd_tail->r_nrdesc);
        while (bcnt < ei->ei_num_rxd) {
		struct rcvbuf *newrb;

		m = m_vget(M_DONTWAIT, sizeof(struct rcvbuf), MT_DATA);
		if (m == 0) {
			#pragma mips_frequency_hint NEVER
			DBG_PRINT(("ec%d: out of mbufs, bcnt = %d\n", ei->ei_unit, bcnt));
			break;
		}
		bcnt++;

		/*
		 * initialize new receive descriptor and receive
		 * buffer with an ifheader at the front of it.
		 */
		newrb = mtod(m, struct rcvbuf *);
		IF_INITHEADER(newrb, &ei->ei_if, sizeof(struct etherbufhead));

		GETRDDESC(rd,_rd);
		_rd.r_rbcnt = MAX_RPKT;
		if (!dma_copy) {
                	_rd.r_rbufptr = K_TO_PHYS(newrb->ebh.ebh_pad);
		}
		_rd.r_mbuf = m;
		_rd.r_rown = 1;
		_rd.r_eor = 0;
		PUTRDDESC(rd,_rd);
		rd_tail = rd;
		rd = (rd_desc *)PHYS_TO_K1(_rd.r_nrdesc);

		/*
		 * Make sure the mbuf header and data are flushed because
		 * the receive code above will invalidate after the DMA.
		 */
		dki_dcache_wbinval((void *)m, MMINOFF);
		dki_dcache_wbinval((void *)newrb->ebh.ebh_pad, MAX_RPKT);
        }
	if (obcnt != bcnt) {
		PUTRDDESCFIELD(rd_tail,r_eor,1);
		PUTRDDESCFIELD(ei->ei_rtail,r_eor,0);
	}

	ei->ei_rtail = rd_tail;
	ei->ei_rnum = bcnt;

	/*
	 * Has receive stopped?
	 */
	if (save_rstatus & HPC_STRCVDMA &&
	   !((save_rstatus = hio->rcvstat) & HPC_STRCVDMA)) {
		rd_crbdp = (rd_desc *)PHYS_TO_K1(ENET_READ(hio->crbdp) & ~0xf);
		save_rtail = ei->ei_rtail;
	}

	if (!(save_rstatus & HPC_STRCVDMA)) {	
		#pragma mips_frequency_hint NEVER

		rstat = (save_rstatus >> RCVSTAT_SHIFT) & 0xff;

		if (rd_crbdp == save_rtail) {
			DBG_PRINT(("ec%d: receive dma stopped   --   out of receive descriptors\n", ei->ei_unit));

#if IP20
		} else if (hio->ctl & HPC_RBO) {
			hio->ctl = HPC_MODNORM | HPC_RBO;
#elif IP22 || IP26 || IP28
		} else if (rstat & HPC_RBO) {
#endif	/* IP22 || IP26 || IP28 */
			DBG_PRINT(("ec%d: rcv packet too long\n", ei->ei_unit));

		} else {

			/* appear to have stopped for abnormal reason */

			DBG_PRINT(("ec%d: receive dma stopped may be out of receive descriptors, rstat = 0x%x\n", ei->ei_unit, rstat));

#if notdef /* IP20 */
			if (!(ei->ei_efix = ec_oflow_check(ei)))
				return(found);
#elif IP20 || IP22 || IP26 || IP28
			(void) ec_init(&ei->ei_eif, ei->ei_if.if_flags);
			return found;
#endif
		}

		hio->nrbdp = K1_TO_PHYS(rd_crbdp->r_nrdesc);
		START_RCV();
	}

	/*
	 * Process completed transmissions.
	 */

	if (ei->ei_tfreecnt == ec_tx_desc)	/* nothing to do */
		return found;

	if (!ei->ei_sgi_edlc) {
		if (!ei->ei_sqe_on)
			ei->ei_if.if_collisions += enet_collision;
		else {
			/*
			 * we don't want to ever decrement if_collision since
			 * this may cause 'netstat -i 1' to show negative
			 * collision between consecutive updates
	 		 */
			if (enet_collision >= ei->ei_coll_adj) {
				ei->ei_if.if_collisions +=
					enet_collision - ei->ei_coll_adj;
				ei->ei_coll_adj = 0;
			} else {
				ei->ei_coll_adj -= enet_collision;
			}
		}
		enet_collision = 0;
	} else {
		ei->ei_collisions = (hio->seeq_coll_xmit[0] & 0xff) +
			((hio->seeq_coll_xmit[1] & 0x07) << 8);
		ei->ei_if.if_collisions += ei->ei_collisions;

		/* clear collision counter */
		hio->seeq_ctl = ei->ei_seeq_ctl & ~SEQ_CTL_XCOLL;
		hio->seeq_ctl = ei->ei_seeq_ctl;
	}

	/*
	 * ifdef out this portion of the code for now since there may be a
	 * bug that make the EDLC think the SQE is being enabled/disabled
	 * every time it gets a collision too early
	 */
#ifdef NOTDEF
	if (ei->ei_sgi_edlc) {
		if ((ei->ei_sqe_on && (hio->seeq_flags & SEQ_XS_NO_SQE)) 
	            || (!ei->ei_sqe_on && !(hio->seeq_flags & SEQ_XS_NO_SQE))) {
			ei->ei_sqe_on = !ei->ei_sqe_on;
			DBG_PRINT(("ec%d: SQE is %s\n",
				ei->ei_unit, ei->ei_sqe_on ? "on" : "off"));
		}
		hio->seeq_ctl = ei->ei_seeq_ctl & ~SEQ_CTL_SQE;
		hio->seeq_ctl = ei->ei_seeq_ctl;
	}
#endif	/* NOTDEF */

	ps_more = clean_xd_chain(ei,hio);
	if (ps_more >= 2) 
		check_carrier(ei,hio);
	kickstart_dma(ei,hio);

	/* RSVP.  Update the packet scheduler with our txqlen status */
	if (PS_ENABLED(ei) && ps_more)
		ps_txq_stat(eiftoifp(&ei->ei_eif), ei->ei_tfreecnt >> 2);

	return found;
}

#ifdef notdef /* never used any more */
#if IP20
static int
ec_oflow_check(register struct ec_info *ei)
{
	if (ei->ei_efix)	/* already changed modes? */
		return 1;
		
	if (ei->ei_oflow_lbolt <= lbolt) {
		ei->ei_oflow_lbolt = lbolt +
			ec_oflow_period * HZ;
		ei->ei_oflow_cnt = 1;
	} else
		ei->ei_oflow_cnt++;

	/*
	 * enter promisc mode if stopped by "too many" oflows
	 */
	if (ei->ei_oflow_cnt > ec_oflow_limit) {
		DBG_PRINT(("ec%d: too many oflows, changing mode\n", ei->ei_unit));
		ei->ei_efix = 1;
		(void) ec_init(&ei->ei_eif, ei->ei_if.if_flags);
		(void)dtimeout(ec_recover, ei, ec_promisc_time * HZ,
					 plbase, cpuid());
		return 1;
	}

	return 0;
}
#endif

static void
ec_recover(register struct ec_info *ei)
{

        IFNET_LOCK(eiftoifp(&ei->ei_eif));
	ei->ei_efix = 0;
	(void) ec_init(&ei->ei_eif, ei->ei_if.if_flags);
        IFNET_UNLOCK(eiftoifp(&ei->ei_eif));
	DBG_PRINT(("ec%d: changing back to normal mode\n", ei->ei_unit));
}
#endif /* never used any more */

static void
ec_hang_recovery(register struct ec_info *ei)
{

        IFNET_LOCK(eiftoifp(&ei->ei_eif));
	(void)ec_init(&ei->ei_eif, ei->ei_if.if_flags);
	ei->ei_hang_id = 0;
#ifdef DEBUG
	ei->ei_hang_cnt++;
#endif	/* DEBUG */
        IFNET_UNLOCK(eiftoifp(&ei->ei_eif));
}

static void
ec_init_interface(register struct ec_info *ei)
{

        IFNET_LOCK(eiftoifp(&ei->ei_eif));
        (void) ec_init(&ei->ei_eif, ei->ei_if.if_flags);
        IFNET_UNLOCK(eiftoifp(&ei->ei_eif));
}


#if IP22 || IP26 || IP28
/*
 * if we sent something to oursleves and did not receive it within a set
 * amount of time, we probably not going to receive it.  so reset the
 * counter and forget about the timeout routine
 */
static void
ec_unicast_watchdog(register struct ifnet *ifp)
{
	struct ec_info *ei;

	ei = eiftoei(ifptoeif(ifp));
        IFNET_LOCK(ifp);
	ei->ei_unicast_counter = 0;
	ei->ei_unicast_id = 0;
        IFNET_UNLOCK(ifp);
}
#endif	/* IP22 || IP26 || IP28 */

/*
 * Called by packet scheduling functions to determine length of driver queue.
 */
static int
ec_txfree_len(struct ifnet *ifp)
{
	struct ec_info *ei;

	ei = eiftoei(ifptoeif(ifp));
	return (ei->ei_tfreecnt >> 2);
}

/*
 * RSVP
 * Called by packet scheduling functions to notify driver about queueing
 * state.  If setting is 1 then there are queues and driver should update
 * the packet scheduler by calling ps_txq_stat() when the txq length changes.
 * If setting is 0, then don't call packet scheduler.
 */
static void
ec_setstate(struct ifnet *ifp, int setting)
{
	struct ec_info *ei;

	ei = eiftoei(ifptoeif(ifp));
	if (setting)
		ps_enabled |= (1 << ei->ei_unit);
	else
		ps_enabled &= ~(1 << ei->ei_unit);
}


/*
 * Queue the mbuf chain to be transmitted via the seeq interface
 */
static int				/* 0 or errno */
ec_transmit(
	struct etherif *eif,		/* on this interface */
	struct etheraddr *edst,		/* with these addresses */
	struct etheraddr *esrc,
	u_short type,			/* of this type */
	struct mbuf *m)			/* send this chain */
{
	struct mbuf *n;
	int mlen, mcnt;
	struct ether_header *eh;
	xd_desc *xd_build, *xd_build2;
	caddr_t cp;
	register struct ec_info *ei;
	EHIO hio;
#if IP20
	int non_dma_mem = 0;
#endif	/* IP20 */
	xd_desc _xd;
#if IP22 || IP26 || IP28
	u_int xdphys_cur;
#endif

	ei = eiftoei(eif);
	hio = ei->ei_io;
#ifndef REMOTEDEBUG
        ASSERT(IFNET_ISLOCKED(eiftoifp(&ei->ei_eif)));
#endif

	clean_xd_chain(ei, hio);

	/* Count min number of outgoing buffers for packet */
	mlen = mcnt = 0;
	for (n = m; n; n = n->m_next) {
		if (!n->m_len)
			continue;
		mlen += n->m_len;
		mcnt++;

#if IP20
		if (!non_dma_mem) {
			caddr_t p;
			caddr_t q;

			p = mtod(n, caddr_t);
			q = p + n->m_len - 1;
			p = (caddr_t) K_TO_PHYS(p);
			q = (caddr_t) K_TO_PHYS(q);
			if ((__psunsigned_t)p >= DMA_MEM ||
			    (__psunsigned_t)q >= DMA_MEM)
				non_dma_mem = 1;
		}
#endif	/* IP20 */
	}
	ASSERT(mlen <= MAX_TPKT - EHDR_LEN);

	/* Check for sufficient xmit descriptors.  Need max of twice
	 * mcnt if all mbufs cross page boundaries + header
	 */
	if (mcnt*2+1 > ei->ei_tfreecnt) {
		#pragma mips_frequency_hint NEVER
	
		/* For bug 321135: looping no tx-desc: */

		/* report the status every 32 */
		if( !(ei->ei_no_xd_cnt & 0x1f) ){
			DBG_PRINT(("ec%d: transmit dropped packet.  No xd_desc(%d > %d).\n", ei->ei_unit, mcnt*2+1, ei->ei_tfreecnt));
		}

		m_freem(m);
		IF_DROP(&ei->ei_if.if_snd);
		
		/* is reset required? */
		if( ++ei->ei_no_xd_cnt >= ec_max_no_xd_cnt ){
			DBG_PRINT(("ec%d: reset becasue of ei_no_xd_cnt(%d)!",
				ei->ei_unit, ei->ei_no_xd_cnt) );
			(void)dtimeout(ec_init_interface, ei, 0,
					 plbase, cpuid());
		}
		return ENOBUFS;
	}
	ei->ei_no_xd_cnt = 0;

	/* make room for ethernet header */
	if (M_HASCL(m) || m->m_off <= MMINOFF + SPACE_FOR_EHDR) {
		n = m_get(M_DONTWAIT, MT_DATA);
		if (n == 0) {
			#pragma mips_frequency_hint NEVER
			DBG_PRINT(("ec%d: transmit ehdr alloc failed.\n", ei->ei_unit));
			m_freem(m);
			IF_DROP(&ei->ei_if.if_snd);
			return ENOBUFS;
		}
#if IP20
		if (!non_dma_mem) {
			caddr_t p;
			caddr_t q;

			p = mtod(n, caddr_t);
			q = p + SPACE_FOR_EHDR - 1;
			p = (caddr_t) K_TO_PHYS(p);
			q = (caddr_t) K_TO_PHYS(q);
			if ((__psunsigned_t)p >= DMA_MEM ||
			    (__psunsigned_t)q >= DMA_MEM)
				non_dma_mem = 1;
		}
#endif	/* IP20 */
		n->m_len = SPACE_FOR_EHDR;
		n->m_next = m;
		m = n;
		mcnt++;
	} else {
		m->m_off -= SPACE_FOR_EHDR;
		m->m_len += SPACE_FOR_EHDR;
	}

	/* create ether header */
	cp = mtod(m, caddr_t);
	eh = RAW_HDR(cp, struct ether_header);
	*(struct etheraddr *)eh->ether_dhost = *edst;
	*(struct etheraddr *)eh->ether_shost = *esrc;
	eh->ether_type = type;

	/* skip over the raw header padding */
	m->m_off += RAW_HDRPAD(EHDR_LEN);
	m->m_len -= RAW_HDRPAD(EHDR_LEN);
	cp += RAW_HDRPAD(EHDR_LEN);

	/* Check whether snoopers want to copy this packet.
	 */
	if (RAWIF_SNOOPING(&ei->ei_rawif)
	    && snoop_match(&ei->ei_rawif, cp, mlen)) {
		ether_selfsnoop(&ei->ei_eif, eh, m, sizeof *eh, mlen);
	}

	if (!ei->ei_sgi_edlc && ei->ei_sqe_on)
		ei->ei_coll_adj++;

	/* put mbuf into savings
	 */
	m->m_act = 0;
	if (ei->ei_tmbuf_tail == 0)
		ei->ei_tmbuf_tail = ei->ei_tmbuf_head = m;
	else {
		ei->ei_tmbuf_tail->m_act = m;
		ei->ei_tmbuf_tail = m;
	}

#if IP22 || IP26 || IP28
	/*
	 * if we try to send something to ourselves, set the counter such that
	 * the receiver know to ignore it
	 */
	if (strncmp((void *)ei->ei_curaddr, (void *)edst, ETHERADDRLEN) == 0) {
		ei->ei_unicast_counter++;
		if (ei->ei_unicast_id)
			untimeout(ei->ei_unicast_id);
		ei->ei_unicast_id = dtimeout(ec_unicast_watchdog, eiftoifp(&ei->ei_eif), HZ * 5,
						plbase, cpuid());
	}
#endif	/* IP22 || IP26 || IP28 */

#if IP20
	if (non_dma_mem) {
		int	i;
		int	len;
		caddr_t	p;

		i = (xd_desc *)PHYS_TO_K1(ei->ei_ttail->x_nxdesc) -
			(xd_desc *)ei->ei_tmdvec;
		p = ei->ei_xmit_buf + (i * BYTES_PER_DMA);
		ASSERT(mlen + EHDR_LEN < BYTES_PER_DMA);
		len = m_datacopy(m, 0, mlen + EHDR_LEN, p); 
		ASSERT(len == mlen + EHDR_LEN);
		ei->ei_xmit_cp_cnt++;

		ei->ei_tfreecnt--;
		ASSERT(ei->ei_tfreecnt >= 0);

		xd_build = (xd_desc *)PHYS_TO_K1(ei->ei_ttail->x_nxdesc);
		_xd = *xd_build;
		ASSERT(!_xd.x_inuse);
		_xd.x_inuse = 1;

		_xd.x_mbuf = m;
		_xd.x_xbufptr = KDM_TO_PHYS(p);
		_xd.x_xbcnt = len;

		*xd_build = _xd;

		dki_dcache_wbinval(p, len);

		goto done;
	}
#endif	/* IP20 */

	/* create HPC descriptor chain
	 */

	ei->ei_tfreecnt -= mcnt;
	ASSERT(ei->ei_tfreecnt >= 0);
	xd_build = (xd_desc *)PHYS_TO_K1(ei->ei_ttail->x_nxdesc);
	GETXDDESC(xd_build,_xd);
	_xd.x_mbuf = m;
#if IP22 || IP26 || IP28
	_xd.x_txd = 0;	/* 1st descriptor in the packet */
#endif	/* IP22 || IP26 || IP28 */
	for (;;) {
		char	*cp2;

		GOODMP(m);
		GOODMT(m->m_type);
		ASSERT(m->m_len > 0);

		ASSERT(!_xd.x_inuse);
		_xd.x_inuse = 1;

		_xd.x_xbufptr = K_TO_PHYS(cp);

		_xd.x_eoxp = 0;
		_xd.x_eox = 0;
		_xd.x_xie = 0;

		cp2 = cp + m->m_len - 1;
		if (hpc_pnum(cp) != hpc_pnum(cp2)) { /* does buf span pages? */
			xd_desc _xd2;

			xd_build2 = (xd_desc *)PHYS_TO_K1(_xd.x_nxdesc);
			ei->ei_tfreecnt--;
			ASSERT(ei->ei_tfreecnt >= 0);

			GETXDDESC(xd_build2,_xd2);

			ASSERT(!_xd2.x_inuse);
			_xd2.x_inuse = 1;
			ASSERT(!_xd2.x_mbuf);

			_xd2.x_xbufptr = K_TO_PHYS(cp2) & ~HPC_POFFMASK;

			_xd2.x_eoxp = 0;
			_xd2.x_eox = 0;
			_xd2.x_xie = 0;

			_xd.x_xbcnt = m->m_len - hpc_poff(cp2) - 1;
			_xd2.x_xbcnt = hpc_poff(cp2) + 1;

			PUTXDDESC(xd_build,_xd);
			_xd = _xd2; 

			xd_build = xd_build2;
		} else {
			_xd.x_xbcnt = m->m_len;
		}

		if (cachewrback)
			dki_dcache_wbinval(cp, m->m_len);

		do {
			m = m->m_next;
			if (m == 0)
				goto done;
		} while (m->m_len == 0);

		cp = mtod(m, caddr_t);
		PUTXDDESC(xd_build,_xd);
		xd_build = (xd_desc *)PHYS_TO_K1(_xd.x_nxdesc);
		GETXDDESC(xd_build,_xd);
		_xd.x_mbuf = 0;
	}

done:
	 /* mark end of chain */
	if (mlen < MIN_TPKT - EHDR_LEN) {
		_xd.x_xbcnt += (MIN_TPKT - EHDR_LEN) - mlen;
		ei->ei_if.if_obytes += MIN_TPKT;
	} else {
		ei->ei_if.if_obytes += mlen;
	}

	_xd.x_eoxp = 1;
	_xd.x_eox = 1;
	_xd.x_xie = 1;
	PUTXDDESC(xd_build,_xd);

	/* connect new chain to hpc transmit chain */
	if (ec_xpg_delay <= 0) {	/* "normal", streaming mode */
		GETXDDESC(ei->ei_ttail,_xd);
		_xd.x_eox = 0;
		if (!PS_ENABLED(ei) || (ei->ei_opackets & 1))
			_xd.x_xie = 0;
		PUTXDDESC(ei->ei_ttail,_xd);
	}

	kickstart_dma(ei,hio);

	ei->ei_ttail = xd_build;

	if (!ei->ei_hang_id)	/* start 'enet hang' timer */
		ei->ei_hang_id = dtimeout(ec_hang_recovery, ei, 5 * HZ,
						plbase, cpuid());

	return 0;
}

static int
seeq_ioctl(
	struct seeqif *sif,
	int cmd,
	caddr_t data)
{
	struct etherif *eif;

	eif = &sif->sif_eif;

	if (eiftoei(eif)->ei_sgi_edlc) {
		struct mfreq *mfr;
		union mkey *key;

		mfr = (struct mfreq *)data;
		key = mfr->mfr_key;

		switch (cmd) {
		case SIOCADDMULTI:
			mfr->mfr_value = laf_hash(key->mk_dhost,
				sizeof(key->mk_dhost));
			if (LAF_TSTBIT(&sif->sif_laf, mfr->mfr_value)) {
				ASSERT(mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
				sif->sif_lafcoll++;
				break;
			}
			ASSERT(!mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
			LAF_SETBIT(&sif->sif_laf, mfr->mfr_value);
			if (!(eiftoifp(eif)->if_flags 
			      & (IFF_ALLMULTI | IFF_PROMISC)))
				(void)ec_init(eif,
					eif->eif_arpcom.ac_if.if_flags);
			if (sif->sif_nmulti == 0)
				eiftoifp(eif)->if_flags |= IFF_FILTMULTI;
			sif->sif_nmulti++;
			break;

		case SIOCDELMULTI:
			if (mfr->mfr_value 
			    != laf_hash(key->mk_dhost, sizeof(key->mk_dhost)))
				return EINVAL;
			if (mfhasvalue(&eif->eif_mfilter, mfr->mfr_value)) {
				/* Forget about this collision. */
				--sif->sif_lafcoll;
				break;
			}
			/*
		 	 * If this multicast address is the last one to map
			 * the bit numbered by mfr->mfr_value in the filter,
			 * clear that bit and update the chip.
		 	 */
			LAF_CLRBIT(&sif->sif_laf, mfr->mfr_value);
			if (!(eiftoifp(eif)->if_flags 
			      & (IFF_ALLMULTI | IFF_PROMISC)))
				(void)ec_init(eif,
					eif->eif_arpcom.ac_if.if_flags);
			--sif->sif_nmulti;
			if (sif->sif_nmulti == 0)
				eiftoifp(eif)->if_flags &= ~IFF_FILTMULTI;
			break;

		default:
			return EINVAL;
		}
	} else {
		switch (cmd) {
		case SIOCADDMULTI:
			if (sif->sif_nmulti == 0) {
				eiftoifp(eif)->if_flags |= IFF_FILTMULTI;
				if (!(eiftoifp(eif)->if_flags 
				      & (IFF_ALLMULTI | IFF_PROMISC)))
					(void)ec_init(eif,
						eif->eif_arpcom.ac_if.if_flags);
			}
			
			sif->sif_nmulti++;
			break;

		case SIOCDELMULTI:
			--sif->sif_nmulti;

			if (sif->sif_nmulti == 0) {
				eiftoifp(eif)->if_flags &= ~IFF_FILTMULTI;
				if (!(eiftoifp(eif)->if_flags 
				      & (IFF_ALLMULTI | IFF_PROMISC)))
					(void)ec_init(eif,
						eif->eif_arpcom.ac_if.if_flags);
			}
			break;

		default:
			return EINVAL;
		}
	}

	return 0;
}



/*
 * Given a multicast ethernet address, this routine calculates the 
 * address's bit index in the logical address filter mask for the SGI/EDLC
 *
 * copied from bsd/misc/lance.c
 */
#define CRC_MASK	0xEDB88320

static int
laf_hash(u_char *addr, int len)
{
	u_int crc;
	u_char byte;
	int bits;

	for (crc = ~0; --len >= 0; addr++) {
		byte = *addr;
		for (bits = 8; --bits >= 0; ) {
			if ((byte ^ crc) & 1)
				crc = (crc >> 1) ^ CRC_MASK;
			else
				crc >>= 1;
			byte >>= 1;
		}
	}
	return (crc >> 26);
}



static struct etherifops ecops =
	{ ec_init, ec_reset, ec_watchdog, ec_transmit,
	  (int (*)(struct etherif*,int,void*))seeq_ioctl };

static __psunsigned_t hpc_base[] = {
	PHYS_TO_K1(HPC_0_ID_ADDR),
#if IP20
	PHYS_TO_K1(HPC_1_ID_ADDR),	/* optional GIO ethernet controller */
	PHYS_TO_K1(HPC_2_ID_ADDR),	/* optional GIO ethernet controller */
#endif	/* IP20 */
#ifdef IP22
	PHYS_TO_K1(HPC_1_ID_ADDR),	/* optional GIO ethernet controller */
	PHYS_TO_K1(HPC_2_ID_ADDR),	/* optional GIO ethernet controller */
	PHYS_TO_K1(HPC_1_ID_ADDR),	/* Challenge S SCSI/Enet controller */
#endif
};

#define	BASE_ADDR	(hpc_base[unit])
#define	ENDIAN_ADDR	(hpc_base[unit] + 0xc0)
#define	PROBE_ADDR	(hpc_base[unit] + 0x11c)
#define	RESET_ADDR	(hpc_base[unit] + 0x3c)
#ifdef _MIPSEB
#define	CPUAUXCTL_ADDR	(hpc_base[unit] + 0x1bf)
#else
#define	CPUAUXCTL_ADDR	(hpc_base[unit] + 0x1bc)
#endif	/* _MIPSEB */

#ifdef IP22
#define PROBE2_ADDR	(hpc_base[unit] + 0x5d000)
#define RESET2_ADDR	(hpc_base[unit] + 0x15014)
#ifdef _MIPSEB
#define CPUAUXCTL2_ADDR	(hpc_base[unit] + 0x3000b)
#else
#define CPUAUXCTL2_ADDR	(hpc_base[unit] + 0x30008)
#endif
#endif

static int
eaddr_ok (u_char *ether_vec)
{
	/* Test for either SGI or Cabletron complient ethernet address.
	 * The last is necessary since the HPLC (Indigo Light) will often
	 * have Cabletron ethernet addresses put on it.
	 */
	return (ether_vec[0] == SGI_ETHER_ADDR_0 &&
		ether_vec[1] == SGI_ETHER_ADDR_1 &&
		ether_vec[2] == SGI_ETHER_ADDR_2
		||
		ether_vec[0] == CABLETRON_ETHER_ADDR_0 &&
		ether_vec[1] == CABLETRON_ETHER_ADDR_1 &&
		ether_vec[2] == CABLETRON_ETHER_ADDR_2);
}

/* ARGSUSED */
void
if_ecedtinit(struct edt *edtp)
{
	struct etheraddr ea;
	int unit;
	int ongio = 0;
	vertex_hdl_t giovhdl;
	vertex_hdl_t ecvhdl;
	graph_error_t rc;
	char loc_str[32];
	int ec_rbuf_total = 0;
	int probe_value;
	extern char eaddr[];
	int i;
	rd_desc_layout *rdl;
	xd_desc_layout *xdl;
	struct ps_parms ps_params;
#if IP24_GIO
	static int ecinit = 0;

	/*
	 * Only come through this code once
	 */
	if (ecinit) {
		return;
	}
	ecinit = 1;
#endif


	for (unit = 0; unit < MAX_NUM_EDLC; unit++) {
		register struct ec_info *ei = &ec_info[unit];

		if (unit != 0) {
#ifdef IP22
		    if (unit != 3)
			continue;
		    else {
			/* Look at pbus.cfgpio(0) */
			if (badaddr_val((void *)PROBE2_ADDR, sizeof(u_int),
			    &probe_value))
			    continue;
			if (probe_value != 0x3ffff)
			    continue;

			/* reset the ethernet state machine */
			*(volatile int *)RESET2_ADDR = HPC_ERST;
			DELAY(10);
			*(volatile int *)RESET2_ADDR = 0x0;
		    }
#else
/*
 * work around a possible HPC1.5 bug that causes probing of BASE_ADDR to
 * succeed some of the times even when there is no GIO card
 */
#if IP20
#define	PATTERN_0	0xaaaa
#define	PATTERN_1	0x5555
#define	PROBE_0		(BASE_ADDR + 0x10)	/* NXBDP */
#define	PROBE_1		(BASE_ADDR + 0x50)	/* NRBDP */

			probe_value = PATTERN_0;
			if (wbadaddr_val((void *)PROBE_0, sizeof(u_int),
			    &probe_value))
				continue;
			probe_value = PATTERN_1;
			if (wbadaddr_val((void *)PROBE_1, sizeof(u_int),
			    &probe_value))
				continue;
			if (badaddr_val((void *)PROBE_0, sizeof(u_int),
			    &probe_value))
				continue;
			if (probe_value != PATTERN_0)
				continue;
#endif	/* IP20 */
			/* check for HPC */
			if (badaddr((void *)BASE_ADDR, sizeof(u_int)))
				continue;

			/* reset the ethernet state machine */
			*(volatile int *)RESET_ADDR = HPC_ERST;
			DELAY(10);
			*(volatile int *)RESET_ADDR = 0x0;

			/* check for ethernet controller */
			if (badaddr_val((void *)PROBE_ADDR, sizeof(u_int), 
			    &probe_value))
				continue;
			if ((probe_value & 0xff) != 0x80)
				continue;

#ifdef _MIPSEB
			*(volatile u_int *)ENDIAN_ADDR = 0x0;
#else
			*(volatile u_int *)ENDIAN_ADDR = HPC_ALL_LITTLE;
#endif	/* _MIPSEB */
#endif	/* IP22 */
		}

#ifdef	IP24_NO_INTEGRAL_EC
		else
		    continue;		/* XXX HACK - for testing */
#endif

		if (ei->ei_resets) {
			dri_printf("ec%d: ec_info structure non-zero\n", unit);
			continue;
		}

		if (unit == 0)
			bcopy(eaddr, ea.ea_vec, 6);
		else {
			extern volatile unchar *cpu_auxctl;
			u_short _eaddr;

#ifdef IP22
			if (unit == 3)
			    cpu_auxctl = (volatile unchar *)CPUAUXCTL2_ADDR;
			else
#endif
			cpu_auxctl = (volatile unchar *)CPUAUXCTL_ADDR;

/*
 * don't want to use the define NVOFF_ENET in IP{12,20}nvram.h because they
 * have different values and using it means we have to burn NVRAM on different
 * machines.  always use IP20 to burn the NVRAM for the GIO ethernet card
 */
#define	NVOFF_EADDR	250
			_eaddr = get_nvreg(NVOFF_EADDR / 2);
			ea.ea_vec[0] = _eaddr >> 8;
			ea.ea_vec[1] = _eaddr & 0xff;
			_eaddr = get_nvreg(NVOFF_EADDR / 2 + 1);
			ea.ea_vec[2] = _eaddr >> 8;
			ea.ea_vec[3] = _eaddr & 0xff;
			_eaddr = get_nvreg(NVOFF_EADDR / 2 + 2);
			ea.ea_vec[4] = _eaddr >> 8;
			ea.ea_vec[5] = _eaddr & 0xff;
		}

		if (showconfig)
			printf("ec%d: hardware ethernet address %s\n",
			   	unit, ether_sprintf(ea.ea_vec));

		if (!eaddr_ok(ea.ea_vec)) {
			dri_printf("ec%d: machine has bad ethernet address: %s\n",
				unit, ether_sprintf(ea.ea_vec));
			continue;
		}

		/* allocate 1 page each for receive/transmit descriptors */
		ei->ei_rmdvec = kmem_alloc(sizeof(rd_desc_layout) *
			(MAX_NUM_RXD + 1),
			VM_NOSLEEP | VM_DIRECT | VM_CACHEALIGN);
		ei->ei_rmdvec = (caddr_t)K0_TO_K1(ei->ei_rmdvec);
		if (!ei->ei_rmdvec) {
			dri_printf("ec%d: cannot allocate space for receive descriptors\n", unit);
			continue;
		}

		if( ec_tx_desc > MAX_NUM_TXD || ec_tx_desc < MIN_NUM_TXD ){
			dri_printf("ec%d: ec_tx_desc: %d -> %d!\n",
				unit, ec_tx_desc, DEFAULT_NUM_TXD);
			ec_tx_desc = DEFAULT_NUM_TXD;
		}
		ei->ei_tmdvec = kmem_alloc(sizeof(xd_desc_layout) *
			(ec_tx_desc + 1),
			VM_NOSLEEP | VM_DIRECT | VM_CACHEALIGN);
		ei->ei_tmdvec = (caddr_t)K0_TO_K1(ei->ei_tmdvec);
		if (!ei->ei_tmdvec) {
			dri_printf("ec%d: cannot allocate space for transmit descriptors\n", unit);
			kmem_free(ei->ei_rmdvec,
				sizeof(rd_desc) * (MAX_NUM_RXD + 1));
			continue;
		}

		ec_rcv_promisc = ec_rcv_promisc > MAX_NUM_RXD ?
			MAX_NUM_RXD : ec_rcv_promisc;
		ec_rbuf_total += ec_rcv_promisc;
		ei->ei_num_rxd = ec_rcv_desc;

#if IP20
		if ((ctob(pmem_getfirstclick() + physmem)) > DMA_MEM) {
			uint addr;
			int pages;

			if (KDM_TO_PHYS(ei->ei_tmdvec) > DMA_MEM
			    || KDM_TO_PHYS(ei->ei_rmdvec) > DMA_MEM) {
				dri_printf(nodma_mem, unit, "descriptors");
				kmem_free(ei->ei_rmdvec,
					sizeof(rd_desc_layout) * (MAX_NUM_RXD + 1));
				kmem_free(ei->ei_tmdvec,
					sizeof(xd_desc_layout) * (ec_tx_desc + 1));
				continue;
			}
			dma_copy = 1;

			pages = (ec_tx_desc + (DMA_PAGES - 1)) / DMA_PAGES;
			ei->ei_xmit_buf = kvpalloc(pages,
				VM_NOSLEEP | VM_DIRECT, 0);
			if (!ei->ei_xmit_buf
			    || (KDM_TO_PHYS(ei->ei_xmit_buf + ctob(pages)) > DMA_MEM)) {
				dri_printf(nodma_mem, unit, "transmit buffers");
				kmem_free(ei->ei_rmdvec,
					sizeof(rd_desc_layout) * (MAX_NUM_RXD + 1));
				kmem_free(ei->ei_tmdvec,
					sizeof(xd_desc_layout) * (ec_tx_desc + 1));
				if (ei->ei_xmit_buf)
					kvpfree(ei->ei_xmit_buf, pages);
				continue;
			}

			pages = (MAX_NUM_RXD + (DMA_PAGES - 1)) / DMA_PAGES;
			ei->ei_rcv_buf = kvpalloc(pages,
				VM_NOSLEEP | VM_DIRECT, 0);
			if (!ei->ei_rcv_buf
			    || (KDM_TO_PHYS(ei->ei_rcv_buf + ctob(pages)) > DMA_MEM)) {
				dri_printf(nodma_mem, unit, "receive buffers");
				kmem_free(ei->ei_rmdvec,
					sizeof(rd_desc_layout) * (MAX_NUM_RXD + 1));
				kmem_free(ei->ei_tmdvec,
					sizeof(xd_desc_layout) * (ec_tx_desc + 1));
				kvpfree(ei->ei_xmit_buf, (ec_tx_desc + DMA_PAGES - 1) / DMA_PAGES);
				if (ei->ei_rcv_buf)
					kvpfree(ei->ei_rcv_buf, pages);
				continue;
			}

			addr = KDM_TO_PHYS(ei->ei_rcv_buf);
			rdl = (rd_desc_layout *)K1_TO_K0(ei->ei_rmdvec);
			for (i = 0; i <= MAX_NUM_RXD;
			     i++, rdl++, addr += BYTES_PER_DMA)
				rdl->rd.r_rbufptr = addr;

		}
#endif	/* IP20 */

		rdl = (rd_desc_layout *)K1_TO_K0(ei->ei_rmdvec);
		for (i = 0; i < MAX_NUM_RXD; i++, rdl++)
			rdl->rd.r_nrdesc = KDM_TO_PHYS(rdl + 1);
		rdl->rd.r_nrdesc = KDM_TO_PHYS(ei->ei_rmdvec);
		dki_dcache_wbinval((void *)K1_TO_K0(ei->ei_rmdvec),
			sizeof(rd_desc_layout) * (MAX_NUM_RXD + 1));

		xdl = (xd_desc_layout *)K1_TO_K0(ei->ei_tmdvec);
		for (i = 0; i < ec_tx_desc; i++, xdl++)
			xdl->xd.x_nxdesc = KDM_TO_PHYS(xdl + 1);
		xdl->xd.x_nxdesc = KDM_TO_PHYS(ei->ei_tmdvec);
		dki_dcache_wbinval((void *)K1_TO_K0(ei->ei_tmdvec),
			sizeof(xd_desc_layout) * (MAX_NUM_TXD + 1));

		if (ec_rcvint_delay == 0)
			ec_rcvint_delay = HPC_DELAY_EXPIRED;
		else
			ec_rcvint_delay <<= HPC_DELAY_SHIFT;

		ei->ei_unit = unit;
		ei->ei_io = (EHIO)BASE_ADDR;
	
		ei->ei_io->ctl = HPC_MODNORM | HPC_ERST | HPC_INTPEND;
		DELAY(10);
		ei->ei_io->ctl = HPC_MODNORM;

		if (unit == 0) {
#if IP20
			ei->ei_c_detect = 1;
#elif IP22 || IP26 || IP28
			ei->ei_c_detect = 2;
#endif	/* IP22 || IP26 || IP28 */
		}
#ifdef IP22
		else if (unit == 3)
			ei->ei_c_detect = 2;
#endif
		else	/* can't do carrier detection because of HPC bug */
			ei->ei_c_detect = 0;

#if IP20
		/* XXX! need to run more tests before shipping this */
		ei->ei_c_detect = 0;
#endif	/* IP20 */

		/* determine ethernet part, SEEQ or ASIC EDLC */
		ei->ei_sgi_edlc = (ei->ei_io->seeq_coll_xmit[0] & 0xff) == 0;

		if (unit == 0)	/* integral ethernet controller */
			ether_attach(&ei->ei_eif, "ec", unit, (caddr_t)ei,
				&ecops, &ea, INV_ETHER_EC, ei->ei_sgi_edlc);
#ifdef IP22
		else if (unit == 3) { /* Ethernet/SCSI board */
			ether_attach(&ei->ei_eif, "ec", unit, (caddr_t)ei,
				&ecops, &ea, INV_ETHER_EC, ei->ei_sgi_edlc);
			ongio = 1;
		}
#endif
		else {		/* optional GIO ethernet controller */
			ether_attach(&ei->ei_eif, "ec", unit, (caddr_t)ei,
				&ecops, &ea, INV_ETHER_GIO, ei->ei_sgi_edlc);
			ongio = 1;
		}

		if (ongio) {
			rc = gio_hwgraph_lookup(edtp->e_base, &giovhdl);
		} else {
			rc = hpc_hwgraph_lookup(&giovhdl);
		}
		if (rc == GRAPH_SUCCESS) {
			char alias_name[8];
			sprintf(loc_str, "%s/%d", EDGE_LBL_EC, unit);
			sprintf(alias_name, "%s%d", EDGE_LBL_EC, unit);
			(void)if_hwgraph_add(giovhdl, loc_str, "if_ec", 
				alias_name, &ecvhdl);
		}
		add_to_inventory(INV_NETWORK, INV_NET_ETHER, INV_ETHER_EC, unit, ei->ei_sgi_edlc);
		ps_params.bandwidth = 10000000;
		ps_params.txfree = ec_tx_desc >> 2;
		ps_params.txfree_func = ec_txfree_len;
		ps_params.state_func = ec_setstate;
		ps_params.flags = 0;
		(void)ps_init(eiftoifp(&ei->ei_eif), &ps_params);

		if (ei->ei_sgi_edlc) {
			ei->ei_if.if_collisions = 0;
			ei->ei_sqe_on = 0;

			ei->ei_seeq_ctl = SEQ_CTL_XCOLL | SEQ_CTL_ACOLL |
				SEQ_CTL_SQE | SEQ_CTL_SHORT | SEQ_CTL_NOCARR;
		} else {
			/*
	 	 	 * XXX sqe collision count hack.  See above.
			 * Assume some small initial number of collisions
			 * to get us started.
		 	 */
			ei->ei_if.if_collisions = 5;
			ei->ei_sqe_on = 1;
		}

		if (ei->ei_c_detect == 1)
			reset_enet_carrier();
		else if (ei->ei_c_detect == 2) {
			ei->ei_io->seeq_ctl = ei->ei_seeq_ctl & ~SEQ_CTL_NOCARR;
			ei->ei_io->seeq_ctl = ei->ei_seeq_ctl;
		}
		ei->ei_carrier_was_up = 1;
		ei->ei_carr_msg_lbolt = 0;

#ifdef IP20
		if (unit != 0) {
			u_int cpuctrl1;

			setgioconfig(unit - 1,
				GIO64_ARB_EXP0_RT | GIO64_ARB_EXP0_MST);
			cpuctrl1 = *(volatile u_int *)PHYS_TO_K1(CPUCTRL1);
#ifdef _MIPSEB
			if (unit == 1) {
				cpuctrl1 &= ~CPUCTRL1_EXP0_LITTLE;
				cpuctrl1 |= CPUCTRL1_EXP0_FX;
			} else {
				cpuctrl1 &= ~CPUCTRL1_EXP1_LITTLE;
				cpuctrl1 |= CPUCTRL1_EXP1_FX;
			}
#else
			if (unit == 1)
				cpuctrl1 |=
					CPUCTRL1_EXP0_FX | CPUCTRL1_EXP0_LITTLE;
			else
				cpuctrl1 |=
					CPUCTRL1_EXP1_FX | CPUCTRL1_EXP1_LITTLE;
#endif	/* _MIPSEB */
			writemcreg(CPUCTRL1, cpuctrl1);
		}
#endif	/* IP20 */
	}

}

#ifdef DEBUG
void
ec2_dump(register struct ec_info *ei)
{
	register int unit = ei->ei_unit;
	register EHIO hio = ei->ei_io;

	cmn_err(CE_CONT, "ec%d data structures dump:\n", unit);
#ifdef OS_METER
	cmn_err(CE_CONT, "ec_istat_frame: %d\n", ec_istat_frame[unit]);
	cmn_err(CE_CONT, "ec_istat_crc: %d\n", ec_istat_crc[unit]);
	cmn_err(CE_CONT, "ec_istat_oflow: %d\n", ec_istat_oflow[unit]);
	cmn_err(CE_CONT, "ec_istat_small: %d\n", ec_istat_small[unit]);
#if IP22 || IP26 || IP28
	cmn_err(CE_CONT, "ec_istat_rxdc: %d\n", ec_istat_rxdc[unit]);
	cmn_err(CE_CONT, "ec_istat_timeout: %d\n", ec_istat_timeout[unit]);
	cmn_err(CE_CONT, "ec_ostat_late_coll: %d\n", ec_ostat_late_coll[unit]);
#endif	/* IP22 || IP26 || IP28 */
	cmn_err(CE_CONT, "ec_ostat_uflow: %d\n", ec_ostat_uflow[unit]);
	cmn_err(CE_CONT, "ec_trdma_on: %d\n", ec_trdma_on[unit]);
#endif	/* OS_METER */
	cmn_err(CE_CONT, "ec_oflow_limit: %d\n", ec_oflow_limit);
	cmn_err(CE_CONT, "ec_oflow_period: %d\n", ec_oflow_period);
	cmn_err(CE_CONT, "ec_promisc_time: %d\n", ec_promisc_time);
	cmn_err(CE_CONT, "ec_rcvint_delay: %d\n", ec_rcvint_delay);
	cmn_err(CE_CONT, "ec_rbuf_num: %d\n", ec_rcv_promisc);

	cmn_err(CE_CONT, "ei_ract: 0x%x\n", (__psunsigned_t)ei->ei_ract);
	cmn_err(CE_CONT, "ei_rtail: 0x%x\n", (__psunsigned_t)ei->ei_rtail);
	cmn_err(CE_CONT, "ei_rnum: %d\n", ei->ei_rnum);
	cmn_err(CE_CONT, "ei_num_rxd: %d\n", ei->ei_num_rxd);
	cmn_err(CE_CONT, "ei_tact: 0x%x\n", (__psunsigned_t)ei->ei_tact);
	cmn_err(CE_CONT, "ei_ttail: 0x%x\n", (__psunsigned_t)ei->ei_ttail);
	cmn_err(CE_CONT, "ei_tfreecnt: %d\n", ei->ei_tfreecnt);
	cmn_err(CE_CONT, "ei_tmbuf_head: 0x%x\n",
		(__psunsigned_t)ei->ei_tmbuf_head);
	cmn_err(CE_CONT, "ei_tmbuf_tail: 0x%x\n", (__psunsigned_t)ei->ei_tmbuf_tail);
	cmn_err(CE_CONT, "ei_mode: 0x%x\n", ei->ei_mode);
	cmn_err(CE_CONT, "ei_seeq_ctl: 0x%x\n", ei->ei_seeq_ctl);
	cmn_err(CE_CONT, "ei_sgi_edlc: %d\n", ei->ei_sgi_edlc);
	cmn_err(CE_CONT, "ei_efix: %d\n", ei->ei_efix);
	cmn_err(CE_CONT, "ei_tmdvec: 0x%x\n", (__psunsigned_t)ei->ei_tmdvec);
	cmn_err(CE_CONT, "ei_rmdvec: 0x%x\n", (__psunsigned_t)ei->ei_rmdvec);
	cmn_err(CE_CONT, "ei_io: 0x%x\n", (__psunsigned_t)ei->ei_io);
#ifdef DEBUG
	cmn_err(CE_CONT, "ei_hang_cnt: %d\n", ei->ei_hang_cnt);
	cmn_err(CE_CONT, "ei_xmit_sync_cnt: %d\n", ei->ei_xmit_sync_cnt);
	cmn_err(CE_CONT, "ei_watchdog_cnt: %d\n", ei->ei_watchdog_cnt);
#endif	/* DEBUG */
#if IP20
	cmn_err(CE_CONT, "ei_xmit_cp_cnt: %d\n", ei->ei_xmit_cp_cnt);
	cmn_err(CE_CONT, "ei_xmit_buf: 0x%x\n",(__psunsigned_t)ei->ei_xmit_buf);
	cmn_err(CE_CONT, "ei_rcv_buf: 0x%x\n",(__psunsigned_t)ei->ei_rcv_buf);
#endif	/* IP20 */

	cmn_err(CE_CONT, "ethernet address: %x:%x:%x:%x:%x:%x\n",
		ei->ei_curaddr[0], ei->ei_curaddr[1], ei->ei_curaddr[2],
		ei->ei_curaddr[3], ei->ei_curaddr[4], ei->ei_curaddr[5]);

	{
		register struct mbuf *m = ei->ei_tmbuf_head;

		cmn_err(CE_CONT, "\ntransmit mbuf chain:\n");
		while (m) {
			cmn_err(CE_CONT, "\t0x%x: 0x%x\n",
				(__psunsigned_t)m, (__psunsigned_t)m->m_act);
			m = m->m_act;
		}
	}

	{
		register int i;
		register uint *p;

		cmn_err(CE_CONT, "\nreceive descriptor ring:\n");
		p = (uint *)ei->ei_rmdvec;
		for (i = 0; i <= MAX_NUM_RXD; i++, p += 4)
			cmn_err(CE_CONT, "\t0x%x(%d): 0x%x 0x%x 0x%x 0x%x\n",
				(__psunsigned_t)p, i, p[0], p[1], p[2], p[3]);

		cmn_err(CE_CONT, "\ntransmit descriptor ring:\n");
		p = (uint *)ei->ei_tmdvec;
		for (i = 0; i <= ec_tx_desc; i++, p += 4)
			cmn_err(CE_CONT, "\t0x%x(%d): 0x%x 0x%x 0x%x 0x%x\n",
				(__psunsigned_t)p, i, p[0], p[1], p[2], p[3]);
	}

	cmn_err(CE_CONT, "\nhardware registers:\n");
#if IP20
	cmn_err(CE_CONT, "\txcount: 0x%x\n", hio->xcount);
#endif	/* IP20 */
	cmn_err(CE_CONT, "\tcxbp: 0x%x\n", ENET_READ(hio->cxbp));
	cmn_err(CE_CONT, "\tnxbdp: 0x%x\n", ENET_READ(hio->nxbdp));
	cmn_err(CE_CONT, "\txbc: 0x%x\n", hio->xbc);
#if IP20
	cmn_err(CE_CONT, "\txpntr: 0x%x\n", hio->xpntr);
	cmn_err(CE_CONT, "\txfifo: 0x%x\n", hio->xfifo);
	cmn_err(CE_CONT, "\tcxbdp: 0x%x\n", hio->cxbdp);
#endif	/* IP20 */
	cmn_err(CE_CONT, "\tcpfxbdp: 0x%x\n", ENET_READ(hio->cpfxbdp));
	cmn_err(CE_CONT, "\tppfxbdp: 0x%x\n", ENET_READ(hio->ppfxbdp));
#if IP20
	cmn_err(CE_CONT, "\tintdelay: 0x%x\n", hio->intdelay);
#endif	/* IP20 */
	cmn_err(CE_CONT, "\ttrstat: 0x%x\n", hio->trstat);
	cmn_err(CE_CONT, "\trcvstat: 0x%x\n", hio->rcvstat);
	cmn_err(CE_CONT, "\tctl: 0x%x\n", hio->ctl);
#if IP20
	cmn_err(CE_CONT, "\trcount: 0x%x\n", hio->rcount);
#endif /* IP 20 */
	cmn_err(CE_CONT, "\trbc: 0x%x\n", hio->rbc);
	cmn_err(CE_CONT, "\tcrbp: 0x%x\n", ENET_READ(hio->crbp));
	cmn_err(CE_CONT, "\tnrbdp: 0x%x\n", ENET_READ(hio->nrbdp));
	cmn_err(CE_CONT, "\tcrbdp: 0x%x\n", ENET_READ(hio->crbdp));
#if IP20
	cmn_err(CE_CONT, "\trpntr: 0x%x\n", hio->rpntr);
	cmn_err(CE_CONT, "\trfifo: 0x%x\n", hio->rfifo);
#endif	/* IP20 */
	cmn_err(CE_CONT, "\tcsr: 0x%x\n", hio->seeq_csr);
	cmn_err(CE_CONT, "\tcsx: 0x%x\n", hio->seeq_csx);
#if IP22 || IP26 || IP28
	cmn_err(CE_CONT, "\trgio: 0x%x\n", hio->rgio);
	cmn_err(CE_CONT, "\trdev: 0x%x\n", hio->rdev);
	cmn_err(CE_CONT, "\tdmacfg: 0x%x\n", hio->dmacfg);
	cmn_err(CE_CONT, "\tpiocfg: 0x%x\n", hio->piocfg);
	cmn_err(CE_CONT, "\txgio: 0x%x\n", hio->xgio);
	cmn_err(CE_CONT, "\txdev: 0x%x\n", hio->xdev);
#endif	/* IP22 || IP26 || IP28 */
}
#endif	/* DEBUG */

void
idbg_ec2_dump(register int unit, register int showchain)
{
	register struct ec_info *ei = &ec_info[unit];
	register EHIO hio = ei->ei_io;

	if (unit < 0 || unit >= MAX_NUM_EDLC || !hio) {
		qprintf("no ethernet controller %d\n", unit);
		return;
	}

	qprintf("ec%d data structures dump:\n", unit);
#ifdef OS_METER
	qprintf("ec_istat_frame: %d\n", ec_istat_frame[unit]);
	qprintf("ec_istat_crc: %d\n", ec_istat_crc[unit]);
	qprintf("ec_istat_oflow: %d\n", ec_istat_oflow[unit]);
	qprintf("ec_istat_small: %d\n", ec_istat_small[unit]);
#if IP22 || IP26 || IP28
	qprintf("ec_istat_rxdc: %d\n", ec_istat_rxdc[unit]);
	qprintf("ec_istat_timeout: %d\n", ec_istat_timeout[unit]);
	qprintf("ec_ostat_late_coll: %d\n", ec_ostat_late_coll[unit]);
#endif	/* IP22 || IP26 || IP28 */
	qprintf("ec_ostat_uflow: %d\n", ec_ostat_uflow[unit]);
	qprintf("ec_trdma_on: %d\n", ec_trdma_on[unit]);
#endif	/* OS_METER */
	qprintf("ec_oflow_limit: %d\n", ec_oflow_limit);
	qprintf("ec_oflow_period: %d\n", ec_oflow_period);
	qprintf("ec_promisc_time: %d\n", ec_promisc_time);
	qprintf("ec_rcvint_delay: %d\n", ec_rcvint_delay);
	qprintf("ec_rcv_promisc: %d\n", ec_rcv_promisc);

	qprintf("ei_ract: 0x%x\n", ei->ei_ract);
	qprintf("ei_rtail: 0x%x\n", ei->ei_rtail);
	qprintf("ei_rnum: %d\n", ei->ei_rnum);
	qprintf("ei_num_rxd: %d\n", ei->ei_num_rxd);
	qprintf("ei_tact: 0x%x\n", ei->ei_tact);
	qprintf("ei_ttail: 0x%x\n", ei->ei_ttail);
	qprintf("ei_tfreecnt: %d\n", ei->ei_tfreecnt);
	qprintf("ei_tmbuf_head: 0x%x\n", ei->ei_tmbuf_head);
	qprintf("ei_tmbuf_tail: 0x%x\n", ei->ei_tmbuf_tail);
	qprintf("ei_mode: 0x%x\n", ei->ei_mode);
	qprintf("ei_seeq_ctl: 0x%x\n", ei->ei_seeq_ctl);
	qprintf("ei_sgi_edlc: %d\n", ei->ei_sgi_edlc);
	qprintf("ei_efix: %d\n", ei->ei_efix);
	qprintf("ei_tmdvec: 0x%x\n", ei->ei_tmdvec);
	qprintf("ei_rmdvec: 0x%x\n", ei->ei_rmdvec);
	qprintf("ei_io: 0x%x\n", ei->ei_io);
#ifdef DEBUG
	qprintf("ei_hang_cnt: %d\n", ei->ei_hang_cnt);
	qprintf("ei_xmit_sync_cnt: %d\n", ei->ei_xmit_sync_cnt);
	qprintf("ei_watchdog_cnt: %d\n", ei->ei_watchdog_cnt);
#endif	/* DEBUG */
#ifdef IP20
	qprintf("ei_xmit_cp_cnt: %d\n", ei->ei_xmit_cp_cnt);
	qprintf("ei_xmit_buf: 0x%x\n", ei->ei_xmit_buf);
	qprintf("ei_rcv_buf: 0x%x\n", ei->ei_rcv_buf);
#endif	/* IP20 */

	qprintf("ethernet address: %x:%x:%x:%x:%x:%x\n",
		ei->ei_curaddr[0], ei->ei_curaddr[1], ei->ei_curaddr[2],
		ei->ei_curaddr[3], ei->ei_curaddr[4], ei->ei_curaddr[5]);

	if(showchain) {
		register struct mbuf *m = ei->ei_tmbuf_head;

		qprintf("\ntransmit mbuf chain:\n");
		while (m) {
			qprintf("\t0x%x: 0x%x\n", m, m->m_act);
			m = m->m_act;
		}
	}

	if(showchain) {
		register int i;
		register uint *p;

		qprintf("\nreceive descriptor ring:\n");
		p = (uint *)ei->ei_rmdvec;
		for (i = 0; i <= MAX_NUM_RXD; i++, p += 4)
			qprintf("\t0x%x(%d): 0x%x 0x%x 0x%x 0x%x\n",
				p, i, p[0], p[1], p[2], p[3]);

		qprintf("\ntransmit descriptor ring:\n");
		p = (uint *)ei->ei_tmdvec;
		for (i = 0; i <= ec_tx_desc; i++, p += 4)
			qprintf("\t0x%x(%d): 0x%x 0x%x 0x%x 0x%x\n",
				p, i, p[0], p[1], p[2], p[3]);
	}

	qprintf("\nhardware registers:\n");
#if IP20
	qprintf("\txcount: 0x%x\n", hio->xcount);
#endif	/* IP20 */
	qprintf("\tcxbp: 0x%x\n", ENET_READ(hio->cxbp));
	qprintf("\tnxbdp: 0x%x\n", ENET_READ(hio->nxbdp));
	qprintf("\txbc: 0x%x\n", hio->xbc);
#if IP20
	qprintf("\txpntr: 0x%x\n", hio->xpntr);
	qprintf("\txfifo: 0x%x\n", hio->xfifo);
	qprintf("\tcxbdp: 0x%x\n", hio->cxbdp);
#endif	/* IP20 */
	qprintf("\tcpfxbdp: 0x%x\n", ENET_READ(hio->cpfxbdp));
	qprintf("\tppfxbdp: 0x%x\n", ENET_READ(hio->ppfxbdp));
#if IP20
	qprintf("\tintdelay: 0x%x\n", hio->intdelay);
#endif	/* IP20 */
	qprintf("\ttrstat: 0x%x\n", hio->trstat);
	qprintf("\trcvstat: 0x%x\n", hio->rcvstat);
	qprintf("\tctl: 0x%x\n", hio->ctl);
#if IP20
	qprintf("\trcount: 0x%x\n", hio->rcount);
#endif /* IP 20 */
	qprintf("\trbc: 0x%x\n", hio->rbc);
	qprintf("\tcrbp: 0x%x\n", ENET_READ(hio->crbp));
	qprintf("\tnrbdp: 0x%x\n", ENET_READ(hio->nrbdp));
	qprintf("\tcrbdp: 0x%x\n", ENET_READ(hio->crbdp));
#if IP20
	qprintf("\trpntr: 0x%x\n", hio->rpntr);
	qprintf("\trfifo: 0x%x\n", hio->rfifo);
#endif	/* IP20 */
	qprintf("\tcsr: 0x%x\n", hio->seeq_csr);
	qprintf("\tcsx: 0x%x\n", hio->seeq_csx);
#if IP22 || IP26 || IP28
	qprintf("\trgio: 0x%x\n", hio->rgio);
	qprintf("\trdev: 0x%x\n", hio->rdev);
	qprintf("\tdmacfg: 0x%x\n", hio->dmacfg);
	qprintf("\tpiocfg: 0x%x\n", hio->piocfg);
	qprintf("\txgio: 0x%x\n", hio->xgio);
	qprintf("\txdev: 0x%x\n", hio->xdev);
#endif	/* IP22 || IP26 || IP28 */
}
#endif /* IP20 || IP22 || IP26 || IP28 */
