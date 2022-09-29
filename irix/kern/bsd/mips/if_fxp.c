/*
 * Driver for the CMC FXP-130 ethernet board.
 *
 * Copyright 1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

/* do not compile on machines without VME buses */
#if !defined(IP20) && !defined(IP22)

#ident "$Revision: 1.77 $"

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/edt.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/kopt.h"
#include "sys/mbuf.h"
#include "sys/sbd.h"
#include "sys/socket.h"
#include "sys/invent.h"
#include "sys/par.h"
#include "sys/vmereg.h"
#include "sys/cpu.h"
#include "misc/ether.h"
#include "misc/lance.h"
#include "net/if.h"
#include "net/if_types.h"
#include "net/netisr.h"
#include "net/multi.h"
#include "net/raw.h"
#include "net/soioctl.h"
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/ip.h"
#include "netinet/if_ether.h"
#include "sys/hwgraph.h"
#include "sys/iograph.h"
#include "mclance.h"
#ifndef _IRIX4
#include "sys/pio.h"
#include "sys/dmamap.h"
#if EVEREST
#include "sys/EVEREST/io4.h"
#include "sys/EVEREST/everest.h"
#endif
#endif /* _IRIX4 */
#include "bstring.h"
#include <net/rsvp.h>

extern  struct ifnet loif;
extern struct ifqueue ipintrq;		/* ip packet input queue */

#ifdef EVEREST
/*#define NOREADBYPIO	1*/
/*#define DOPREFET	1*/
/*#define SPRAYDBG	1*/
#endif
#define ONECHAINXMIT	1
#ifdef ONECHAINXMIT
/*#define ALIGNEDXMIT	1*/
#endif
/*#define BOUNDDEBUG 1*/

#if defined(R4000) || defined(TFP) || defined(R10000)
#define	VMEIN(s,t,l)	hwcpin(s,t,l)
#define	VMEOUT(s,t,l)	hwcpout(s,t,l)
#else
#define	VMEIN(s,t,l)	bcopy(s,t,l)
#define	VMEOUT(s,t,l)	bcopy(s,t,l)
#endif

int if_fxpdevflag = D_MP;

#ifdef BOUNDDEBUG
struct dlbounds {
	int bufs;
	int frags;
	int max_frags;
	int ch_frags;
	int hw_frags;
	int lw_frags;
	int ch_len;
	int hw_len;
	int lw_len;
};
struct dlbounds fxp_rcvd_bounds;
struct dlbounds fxp_xmit_bounds;
#endif

/*
 * Debugging levels
 */
#define DBG_LVL_PRINT	  1		/* enable most printfs */
#if DBG_LVL >= DBG_LVL_PRINT
# define DP(a)		printf a
#else
# define DP(a)
#endif

#if defined(lint)
#define WB() wbflush()			/* flush to preserve order */
#else
#define WB()
#endif

/*
 * redabilities
 */
#define K		*1024

/*
 * Configuable Literals
 */
#define MAXBD		4		/* allow 4 boards */
#define NUM_RECV_BUFS	32		/* number of recv dma bcbs */
#define	FXPWAIT		10000		/* init delay delta */

#undef SPL_INTR
/*
#if defined(EVEREST)
#define SPL_INTR 1
#endif
*/

/*
 * These macros convert between kernel addresses and "FXP addresses" which are
 *	just 20-bit addresses right-aligned in a 32-bit word.
 */

#if _MIPS_SIM == _ABI64

#define FXP_ADDRMASK	0xffffffffUL
#define FXP_BOARDMASK	~FXP_ADDRMASK

#define FXP_TO_KPHYS(_r, _x)  (void *)(((_x) == 0) ? 0 : 		\
				(((_x) & FXP_ADDRMASK) |		\
				 ((__psunsigned_t)(_r) & FXP_BOARDMASK)))
#define KPHYS_TO_FXP(_x)      ((__uint32_t)(K1_TO_PHYS(_x) & FXP_ADDRMASK))

#else

#define	FXP_TO_KPHYS(_r, _x)	((void *)(_x))
#define KPHYS_TO_FXP(_x)	((__uint32_t)(_x))

#endif /* _ABI64 */

#define	NEXTI(i, mod)	(((i) + 1) % (mod))

/*
 * DMA parameters
 */
#if defined(EVEREST)
#	define	FXP_RAM		VME_A32NPBLOCK
#	define	FXP_XAM		VME_A32NPBLOCK
#else
#	define	FXP_RAM		VME_A32NPAMOD
#	define	FXP_XAM		VME_A32NPAMOD
#endif

/*
 * Watch-dog timers
 */
#define DOG	(2*IFNET_SLOWHZ)	/* check this often after alive */
/* XXX move this to some header file, for all drivers */
extern time_t	lbolt;
#define EMSG_DELAY	(HZ*60*1)	/* suppress messages this long */

/*
 * Receive buffer padding, to ether header and to data.
 */
struct rcvbuf {
	struct etherbufhead	ebh;
	char			data[MAX_RDAT];
};

#define	RCVDATAPAD sizeof(struct etherbufhead)
#define	RCVPAD (RCVDATAPAD - sizeof(struct ether_header))

/* ---------------------------------------------------------------------- */
typedef struct ethlist {
#define EL_ADD_ADDR	0	/* use e_addrs to add addr to laf */
#define EL_DEL_ADDR	1	/* use e_addrs to delete addr from laf */
#define EL_SET_LAF	2	/* use e_laf to set our Mcast_Filter */
	short	e_mode;			/* add addr, del addr, or set laf */
	short	e_listsize;		/* number of active addr entries */
	struct etheraddr e_baseaddr;	/* our ethernet address */
	union {				/* rest of struct is either */
	    struct etheraddr e_a[ 16 ];	/*   ethernet multicast addresses, */
	    Mcast_Filter     e_l;	/*   or encoded LAF, */
	} e_u;				/* depending on e_mode value */
} ETHLIST;
#define e_addrs	e_u.e_a
#define e_laf	e_u.e_l

#define EAC_PROMISC	0x1
#define EAC_SET_ADDR	0x2
#define EAC_SET_FILTER	0x4
#define EAC_ALL	(EAC_PROMISC|EAC_SET_ADDR|EAC_SET_FILTER)
#define EAC_SET_ALLMULTI 0x20

/* ---------------------------------------------------------------------- */


#define RING_SIZE	256	/* entries/ring */
typedef volatile struct {
	u_short	r_rdidx;
	u_short	r_wrtidx;
	u_short	r_size;
	u_short	r_pad;
	__uint32_t	r_slot[RING_SIZE];
} RING;

#if _MIPS_SIM == _ABI64
/*
 * Needed if pointers are different sizes between the host and board
 */
typedef volatile struct {
	u_short	r_rdidx;
	u_short	r_wrtidx;
	u_short	r_size;
	u_short	r_pad;
	__psunsigned_t	r_slot[RING_SIZE];
} CPU_RING;
#else
#define CPU_RING RING
#endif

#define ROM_SIZE (4 K)		/* size of visible ROM */
#define RAM_SIZE (250 K)	/* size of visible RAM */
#define REG_SIZE (256)		/* size of visible control registers */
#define RIW_SIZE (2 K)		/* size of visible reset/interrupt window */


/*
 * FXP Ram data layout
 */
typedef volatile struct {
	u_char	fxp_ram_rom[ROM_SIZE];		/* either RAM or ROM */
	union {
		u_char	fxp_ram[RAM_SIZE];	/* RAM */
		struct {
#define FXP_KICK	0x8080
			u_short reg_go;		/* 0x8080 to start board */
/* fxp_u.regs.reg_csr bits */
#define	FXPCSR_ONLINE	0x2		/* entered application firmware */
#define	FXPCSR_RDY	0x4		/* reset complete */
#define	FXPCSR_OVPROM	0x8		/* don't transfer to prom on reset */
#define	FXPCSR_IE	0x40		/* interrupt enable for bus debug */
#define	FXPCSR_ERR	0x8000		/* fxp detected error */
			u_short reg_csr;	/* board control/status reg */
			__uint32_t reg_pstart;
		} regs;
		struct {			/* link-level structures */
			u_char	pad1[REG_SIZE];
/* state bits in fxp_state */
#define S_FXPRESET	01		/* fxp is in reset state */
#define S_FXPRUN	02		/* fxp is in run state */
#define S_FXPRIP	03		/* fxp's Response is In Progress */
			u_short	e_fxpstate;	/* =S_FXPRUN after init */
/* mode bits in fxp_mode */
#define E_SWAP16	0x1		/* swap two octets within 16 */
#define E_SWAP32	0x2		/* swap 16s within 32 */
#define E_SWAPRD	0x4		/* swap on read */
#define E_SWAPWRT	0x8		/* swap on write */
#define E_DMA		0x10		/* fxp does data moving */
#define E_XMIT_DMA	E_DMA
#define E_ARP		0x20		/* Reserved for Future Use */
#define E_DONTINT	0x40		/* do not interrupt host */
#define E_NOTERM	0x80		/* don't use serial port */
#define E_RCV_DMA	0x100		/* fxp moves rcv data */
#define E_DO_TRAILERS	0x200		/* Reserved for Future Use */
#define E_UNDO_TRAILERS	0x400		/* de-encapusulate trailers */
#define E_NO_SRC_ADDR	0x800		/* bd won't set enet src addr */
#define E_PROMISCUOUS	0x1000		/* accept everything */
#define E_RBUF_SHWD	0x2000		/* align rcv buf on short wd */
#define E_RESET_STATS	0x4000		/* reset lance statistics */
#define E_EXAM_LIST	0x8000		/* fxp should examine addr list */
			u_short	e_fxpmode;
			__uint32_t e_fxpbase;	/* =&fxp_u.fxp_ram_rom[0] */
			u_short	e_fxprun;	/* 'I am alive' counter */
			u_short	e_intvector;	/* alternate interrupt */

			RING	h_tofxp;
			RING	h_hostfree;
			RING	e_tohost;
			RING	e_fxpfree;
			RING	e_rcvdma;
			RING	h_rcv_d;

			__uint32_t e_xmit_ok;	/* Successful xmits */
			__uint32_t e_mult_retry;	/* multiple retries on xmit */
			__uint32_t e_one_retry;	/* single retries */

			__uint32_t e_fail_retry;	/* too many retries */
			__uint32_t e_deferrals;	/* xmit delayed--cable busy */
			__uint32_t e_xmit_buff_err;	/* impossible */
			__uint32_t e_silo_underrun;	/* transmit underrun */
			__uint32_t e_late_coll;	/* collision after xmit */
			__uint32_t e_lost_carrier;
			__uint32_t e_babble;	/* xmit length > 1518 */
			__uint32_t e_no_heartbeat;	/* transceiver mismatch */
			__uint32_t e_xmit_mem_err;

			__uint32_t e_rcv_ok;	/* good receptions */
			__uint32_t e_rcv_missed;	/* no recv buff available */
			__uint32_t e_crc_err;	/* checksum failed */
			__uint32_t e_frame_err;	/* surely implies CRC error */
			__uint32_t e_rcv_buff_err;	/* 'impossible' */
			__uint32_t e_silo_overrun;	/* receive overrun */
			__uint32_t e_rcv_mem_err;

			ETHLIST			e_addr;
		} iface;
	} fxp_u;

	/* Top 2K space is for control functions */
	u_char	fxp_interrupt;		/* R or W interrupts FXP */
	u_char	pad2[1 K -1];
	u_char	fxp_reset;		/* R or W resets FXP */
	u_char	pad3[1 K -1];
} FXPDEVICE;
#define fxp_go		fxp_u.regs.reg_go
#define	fxp_csr		fxp_u.regs.reg_csr
#define	fxp_prog_start	fxp_u.regs.reg_pstart
#define fxp_state	fxp_u.iface.e_fxpstate
#define fxp_mode	fxp_u.iface.e_fxpmode
#define fxp_base	fxp_u.iface.e_fxpbase
#define fxp_run		fxp_u.iface.e_fxprun
#define fxp_intvector	fxp_u.iface.e_intvector
#define fxp_tofxp	fxp_u.iface.h_tofxp
#define fxp_hostfree	fxp_u.iface.h_hostfree
#define fxp_tohost	fxp_u.iface.e_tohost
#define fxp_fxpfree	fxp_u.iface.e_fxpfree
#define fxp_rcvdma	fxp_u.iface.e_rcvdma
#define fxp_rcv_d	fxp_u.iface.h_rcv_d
#define fxp_baseaddr	fxp_u.iface.e_addr.e_baseaddr
#define fxp_eaddrs	fxp_u.iface.e_addr.e_addrs
#define fxp_ethlist	fxp_u.iface.e_addr

#define ADDR_CHECK(ei, foo) \
	if ((__psunsigned_t)foo < (__psunsigned_t)&ei->ei_boardaddr->fxp_u.fxp_ram[0]) { \
		cmn_err_tag(223,CE_PANIC, \
		    "fxp%d: ring address %x below FXP ram (%x) at line %d\n", \
		    ei->ei_if.if_unit, foo, \
		    &ei->ei_boardaddr->fxp_u.fxp_ram[0], __LINE__); \
	} else if((__psunsigned_t)foo > (__psunsigned_t)&ei->ei_boardaddr->fxp_u.fxp_ram[RAM_SIZE-1]){\
		cmn_err_tag(224,CE_PANIC,\
		    "fxp%d: ring address %x above FXP ram (%x) at line %d\n", \
		    ei->ei_if.if_unit, foo, \
		    &ei->ei_boardaddr->fxp_u.fxp_ram[RAM_SIZE-1], __LINE__); \
	}

/*
 *	The bus description structure
 */
#ifdef EVEREST
#if 0
#define	SG_ENTRIES	5		/* max entries for sg-list */
#else
#define	SG_ENTRIES	3		/* max entries for sg-list */
#endif
#else
#define	SG_ENTRIES	8		/* max entries for sg-list */
#endif
typedef volatile struct {
	__uint32_t	bd_link;
	__uint32_t	bd_addr;		/* Physical bus address */
	__uint32_t	bd_len;			/* length in bytes */
	u_short bd_mode;		/* For VME, address modifier */
	u_short bd_pad;
} BUS_D;

/*
 * The FXP Data Buffer Structure
 */
typedef volatile struct bcb {
	__uint32_t	b_link;
/* BCB b_stat field defines */
#define	BCB_DONE	0x8000		/* buffer complete */
#define	BCB_ERR		0x4000		/* error summary */
#define	BCB_FRAME	0x2000		/* framing error */
#define	BCB_OFLO	0x1000		/* silo overflow */
#define	BCB_CRC		0x0800		/* crc error */
#define	BCB_RXBUF	0x0400		/* rx buffer err */
#define	BCB_STP		0x0200		/* start of packet */
#define	BCB_FXP		0x0100		/* end of packet */
#define	BCB_MEMERR	0x0002		/* memory error */
#define	BCB_MISSED	0x0001		/* missed packet */
#define	BCB_ALLERRS	0x3c03		/* all error bits */
	u_short	b_stat;
	u_short	b_len;
	__uint32_t	b_addr;
	u_short	b_msglen;
	u_short	b_reserved;

	/* The next two fields are reserved for host use */
	__int32_t	b_mbufi;	/* index into ei_tm[], ei_rm[], or -1 */
	__uint32_t	b_bus_descr;

	/* This field is NOT available for host use! */
	__uint32_t	b_rsrvd2[2];

} BCB;


/*
 * Ethernet info per interface.
 */
struct fxp_info {
	struct lanceif	ei_lif;		/* lance network interface */

	struct {			/* error statistics */
		__uint32_t e_mult_retry;	/* These track board counts */
		__uint32_t e_one_retry;
		__uint32_t e_deferrals;

		__uint32_t e_fail_retry;
		__uint32_t e_xmit_buff_err;
		__uint32_t e_silo_underrun;
		__uint32_t e_late_coll;
		__uint32_t e_lost_carrier;
		__uint32_t e_babble;
		__uint32_t e_xmit_mem_err;

		__uint32_t e_rcv_missed;
		__uint32_t e_crc_err;
		__uint32_t e_frame_err;
		__uint32_t e_rcv_buff_err;
		__uint32_t e_silo_overrun;
		__uint32_t e_rcv_mem_err;
	} ei_stat;

	FXPDEVICE	*ei_boardaddr;	/* physical address */

	CPU_RING	ei_free;		/* free xmit ring */
	CPU_RING	ei_rcv_free;		/* free recv ring */
	CPU_RING	tmpring;		/* tmp ring workarea */

	int	ei_tmi;				/* index into xmit mbuf table */
	struct	mbuf	*ei_tm[RING_SIZE];	/* active xmit mbufs */
	int	ei_rmi;				/* index into recv mbuf table */
	struct	mbuf	*ei_rm[NUM_RECV_BUFS];	/* active (posted) recv mbufs */

	char	*ei_terr;		/* recent transmit error message */
	time_t	ei_terr_lbolt;		/* when to forget it */

	int	ei_vme_adapter;		/* which VME adapater */
	u_char	ei_vec;			/* VME interrupt vector */
	u_char	ei_brl;			/* VME bus request level */

	u_char	ei_enaddrInitialized;	/* TRUE if ei_enaddr is valid */
	u_char	ei_inPromiscMode;	/* TRUE=LANCE chip is promiscuous */

	u_short	ei_heartbit;		/* recent output ring index */
	u_short	ei_addrCmds;		/* multicast or promiscuous set */

#ifdef EVEREST
	iamap_t		*ei_iamap;
	dmamap_t	*ei_in_map;	/* input DMA map */
	dmamap_t	*ei_out_map;	/* output DMA map */
	int		ei_in_map_n;
	int		ei_out_map_n;
#endif /* EVEREST */

} fxp_info[MAXBD];
#define	ei_eif		ei_lif.lif_eif		/* common ethernet stuff */
#define	ei_laf		ei_lif.lif_laf		/* logical address filter */
#define	ei_ac		ei_eif.eif_arpcom	/* common arp stuff */
#define ei_if           ei_ac.ac_if		/* network-visible interface */
#define ei_enaddr       ei_ac.ac_enaddr		/* hardware ethernet address */
#define	ei_addr		ei_eif.eif_addr		/* board's official address */
#define	ei_rawif	ei_eif.eif_rawif	/* raw domain interface */
#define	ei_lostintrs	ei_eif.eif_lostintrs	/* count lost interrupts */
#define	ei_resets	ei_eif.eif_resets	/* count board resets */
#define	ei_sick		ei_eif.eif_sick		/* 2 => reset on next bark */

static void UpdateBoardParams(int, struct fxp_info *);
static int  fxp_read(struct fxp_info *, BCB *);
static int  fxp_reset(struct fxp_info *);
static void fxp_ifreset(struct etherif *);
static int  fxp_ioctl(struct lanceif *, int, caddr_t);
static int  waitfor_fxp(FXPDEVICE *);
static int  fxp_bdinit(struct fxp_info *);


#ifdef EVEREST
iamap_t *iamap_get(int, int);

#define IN_MAP_LEN	(RING_SIZE)
#define OUT_MAP_LEN	((RING_SIZE+1)*(SG_ENTRIES+2))

/* put input mbuf address into EVEREST DMA map
 */
static __uint32_t
fxp_dmamap(dmamap_t *dmamap,
	   iamap_t *iamap,
	   int index,
	   void *addr)
{
#ifdef _IRIX5_1
	register ulong *te;
	register ulong paddr;
#else
	register uint *te;
	register __psunsigned_t paddr;
#endif

__psunsigned_t mapped_addr;

	/*
	 * On systems where IO_NBPP != NBPP, we need to convert before
	 * dropping a pfn into the mapping array.
	 */
	paddr = SYSTOIOPFN(ev_kvtophyspnum((caddr_t)addr), io_btoct(addr));

	/* Let a TLB-flush happen if needed when we wrap around the
	 * array of mbuf pointers.  (or the first time)
	 */
	if (!index)
		iamap_map(iamap, dmamap);

	te = iamap->table + dmamap->dma_index + index;
	*te = paddr;

	/* clean up the next entri[es] for the pre-fetch.
	 * for even index - just one more.
	 * for odd index - next 2 more.
	 */
	te[1] = paddr;
	if (index&0x1)
		te[2] = paddr;

	/* return with the address as seen on the VME bus
	 */
	mapped_addr = (dmamap->dma_addr + index*IO_NBPC +
			   (__psunsigned_t)(addr)%IO_NBPC) &
			   0xffffffffUL;
	return (__uint32_t)mapped_addr;
}
#endif /* EVEREST */

/*
 * handle FXP-130 'rings'
 */
#define ringempty(rp) ((rp)->r_rdidx == (rp)->r_wrtidx)
#define ringfull(rp) ((((rp)->rwrtidx+1) & ((rp)->r_size-1)) == (rp)->r_rdidx)


static void
ringinit(register CPU_RING *rp, int size)
{
	rp->r_rdidx = rp->r_wrtidx = 0; WB();
	rp->r_size = size; WB();
}

static void
ringput(RING *rp, BCB *bcbp)
{
	register int idx;

	idx = (rp->r_wrtidx + 1) & (rp->r_size-1);
	if (idx != rp->r_rdidx) {
ASSERT((bcbp == 0) || (KPHYS_TO_FXP(bcbp) != 0));
		rp->r_slot[rp->r_wrtidx] = KPHYS_TO_FXP(bcbp);
		rp->r_wrtidx = (u_short)idx; WB();
		if ((idx -= rp->r_rdidx) < 0)
			idx += rp->r_size;
	}
}

static BCB *
ringget(register RING *rp)
{
	if (rp->r_rdidx != rp->r_wrtidx) {
		register BCB *p = FXP_TO_KPHYS(rp, rp->r_slot[rp->r_rdidx]);
		rp->r_rdidx = (rp->r_rdidx+1)&(rp->r_size-1); WB();
		return(p);
	}
	return(0);
}

static void
fxp_clearring(RING *rp)
{
	register BCB *bp;

	while (bp = ringget(rp))
		bp->b_mbufi = -1;
}

#if _MIPS_SIM == _ABI64

#define	CPU_RINGPUT		ringput64
#define	CPU_RINGGET		ringget64
#define	CPU_RINGPEEK		ringpeek64
#define	CPU_CLEARRING		fxp_clearring64

static void
ringput64(CPU_RING *rp, BCB *bcbp) {
	register int idx;

	idx = (rp->r_wrtidx + 1) & (rp->r_size-1);
	if (idx != rp->r_rdidx) {
		rp->r_slot[rp->r_wrtidx] = (__psunsigned_t)bcbp;
		rp->r_wrtidx = (u_short)idx; WB();
		if ((idx -= rp->r_rdidx) < 0)
			idx += rp->r_size;
	}
}


static BCB *
ringget64(register CPU_RING *rp)
{
	if (rp->r_rdidx != rp->r_wrtidx) {
		register BCB *p = (BCB *)rp->r_slot[rp->r_rdidx];
		rp->r_rdidx = (rp->r_rdidx+1)&(rp->r_size-1); WB();
		return(p);
	}
	return(0);
}


static BCB *
ringpeek64(register CPU_RING *rp)
{
	if (rp->r_rdidx != rp->r_wrtidx)
		return((BCB *)rp->r_slot[rp->r_rdidx]);
	return(0);
}

static void
fxp_clearring64(CPU_RING *rp)
{
	register BCB *bp;

	while (bp = ringget64(rp))
		bp->b_mbufi = -1;
}

#else

/*
 * In the 32-bit model, pointers are the same size on both the board
 * and the host.
 */
static BCB *
ringpeek(register RING *rp)
{
	if (rp->r_rdidx != rp->r_wrtidx)
		return((BCB *)FXP_TO_KPHYS(rp, rp->r_slot[rp->r_rdidx]));
	return(0);
}

#define CPU_RINGPUT		ringput
#define CPU_RINGGET		ringget
#define CPU_RINGPEEK		ringpeek
#define	CPU_CLEARRING		fxp_clearring

#endif /* _ABI64 */

/*
 * reset the board
 *
 *	Must be invoked in splimp.
 */
static void
fxp_ifreset(struct etherif *eif)
{
	struct fxp_info *ei = (struct fxp_info *)eif->eif_private;

	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)));
	DP(("fxp%d: IFRESET\n", ei->ei_if.if_unit));
	ei->ei_addrCmds |= EAC_SET_ADDR;
	UpdateBoardParams(ei->ei_addrCmds, ei);
}

static int
fxp_reset(register struct fxp_info *ei)
{
	int i;
	register FXPDEVICE *addr;

	DP(("fxp_reset%d:\n", ei->ei_if.if_unit));
	addr = ei->ei_boardaddr;
	ASSERT(0 != addr);

	addr->fxp_u.regs.reg_csr = 0; WB();
	addr->fxp_reset = 1; WB();

	ei->ei_sick = 0;		/* stop watchdog resets */
	ei->ei_terr_lbolt = 0;		/* force error messages */
	ei->ei_if.if_timer = 0;		/* turn off watchdog */

	NETPAR(NETSCHED, NETWAKINGTKN, NETPID_NULL,
		 NETEVENT_LINKDOWN, NETCNT_NULL, NETRES_BUSY);

	/*
	 * provide hardware settle time for this host environment
	 */
	DELAY(1000);
	i = 0;
	while (i++ < FXPWAIT
	    && (addr->fxp_csr & (FXPCSR_RDY|FXPCSR_ERR)) == 0)
		DELAY(1000);

	if (addr->fxp_csr & FXPCSR_ERR) {
		printf("fxp_reset%d: detected error on reset\n",
		       ei->ei_if.if_unit);
		NETPAR(NETSCHED, NETWAKINGTKN, NETPID_NULL,
			 NETEVENT_LINKDOWN, NETCNT_NULL, NETRES_BUSY);
		return(1);
	}

	if (i >= FXPWAIT) {
		printf("fxp_reset%d: timed-out waiting for reset\n",
		       ei->ei_if.if_unit);
		NETPAR(NETSCHED, NETWAKINGTKN, NETPID_NULL,
			 NETEVENT_LINKDOWN, NETCNT_NULL, NETRES_BUSY);
	}

	/*
	 * Let FXP settle.
	 */
	DELAY(100000);

	if ((addr->fxp_csr & FXPCSR_RDY) == 0)
		return(1);

	/*
	 * save mbuf-ptrs lent if any
	 * But freeing mbufs must be done after
	 * hardware reset to avoid race condition.
	 */
	if (ei->ei_enaddrInitialized) {
		CPU_CLEARRING(&ei->ei_free);
		fxp_clearring(&addr->fxp_hostfree);
		CPU_CLEARRING(&ei->ei_rcv_free);
		fxp_clearring(&addr->fxp_rcvdma);
		fxp_clearring(&addr->fxp_rcv_d);
		fxp_clearring(&addr->fxp_tofxp);
	}

	/*
	 * Clear xmit and recv mbuf tables.
	 */
	for (i = 0; i < RING_SIZE; i++)
		if (ei->ei_tm[i]) {
			m_freem(ei->ei_tm[i]);
			ei->ei_tm[i] = NULL;
		}
	for (i = 0; i < NUM_RECV_BUFS; i++)
		if (ei->ei_rm[i]) {
			m_freem(ei->ei_rm[i]);
			ei->ei_rm[i] = NULL;
		}
	ei->ei_tmi = ei->ei_rmi = 0;

	DP(("fxp_reset: csr = %x\n", (int)addr->fxp_csr));
	return(0);
}

/*
 *	alloc_xbcb - Allocate a transmit BCB for use as control structures.
 *		   - Must be called at splimp() priority.
 *
 *	For FXP architecture boards, will only use BCBs which contain a full
 *	1K memory segment.
 */
static BCB *
alloc_xbcb(register CPU_RING *rp)
{
	register __uint32_t bp_start, bp_end;
	register BCB	*bcbp;
	register BCB	*obcbp = 0;

	for (bcbp = CPU_RINGGET(rp); bcbp != obcbp; bcbp = CPU_RINGGET(rp)) {
		bp_start = bcbp->b_addr;
		bp_end = bp_start + bcbp->b_len;
		bp_end &= (~(1 K -1));
		bp_start = (bp_start + (1 K -1)) & (~(1 K -1));

		if (bp_start < bp_end) {
			bcbp->b_addr = bp_start;
			bcbp->b_len = bp_end - bp_start;
			return(bcbp);
		}

		/*
		 * if there is no middle 1K, can't use this bcb.
		 */
		if (!obcbp)
			obcbp = bcbp;
		CPU_RINGPUT(rp, bcbp);
	}
	return(0);
}

static void
fxp_hang(struct fxp_info *ei)
{
	register struct mbuf *m;
	register BCB *bp;
	register BUS_D *bdp;
	register char *va = 0;
	register FXPDEVICE *addr = (FXPDEVICE *)ei->ei_boardaddr;
	int i;

#ifdef EVEREST
	CPU_RING *tmpring = &ei->tmpring;
	ringinit(tmpring, RING_SIZE);
#endif
	ASSERT(addr != 0);
	for (;;) {
		bp = CPU_RINGPEEK(&ei->ei_rcv_free);
		if (!bp)
			break;

		i = bp->b_mbufi;
		ASSERT(((i >= 0) && (i < NUM_RECV_BUFS)) || (i == -1));
		if (i >= 0) {
			m = ei->ei_rm[i];
			ASSERT(m);
		} else
			m = NULL;

		if (!m) {
			m = m_vget(M_DONTWAIT,sizeof(struct rcvbuf),MT_DATA);
			if (m == 0)
				return;
			IF_INITHEADER(mtod(m, caddr_t),&ei->ei_if,RCVDATAPAD);
		}

		bp = CPU_RINGGET(&ei->ei_rcv_free);

		/* allocate next recv mbuf pointer entry and increment index */
		i = ei->ei_rmi;
		ASSERT(ei->ei_rm[i] == NULL);
		ei->ei_rm[i] = m;
		bp->b_mbufi = i;
		ei->ei_rmi = NEXTI(i, NUM_RECV_BUFS);

		/* Paranoid */
		bdp = (BUS_D *)FXP_TO_KPHYS(ei->ei_boardaddr,bp->b_bus_descr);
ASSERT(IS_KSEG1(bdp));
ADDR_CHECK(ei, bdp);
		ASSERT(bdp != 0);
		bp->b_addr = KPHYS_TO_FXP(bdp);

		/* Fill in the DMA control information */
		bdp->bd_len = m->m_len - RCVPAD;
		va = mtod(m, char *) + RCVPAD;
#ifdef EVEREST
		bdp->bd_addr = fxp_dmamap(ei->ei_in_map,ei->ei_iamap,
					ei->ei_in_map_n, va);
		ei->ei_in_map_n = (ei->ei_in_map_n+1) % IN_MAP_LEN;
#else
		bdp->bd_addr = kvtophys(va);
#endif

#ifdef EVEREST
		CPU_RINGPUT(tmpring, bp);
#else
		ringput(&addr->fxp_rcv_d, bp);
#endif
	}

#ifdef EVEREST
	if (CPU_RINGPEEK(tmpring)) {
		/* SKIP dummy dma map entries for pre-fetch */
		/* for even index - just one more. - is odd here now */
		/* for odd index - next 2 more. - is even here now */
		ei->ei_in_map_n += (ei->ei_in_map_n&0x1) ? 1 : 2;
		ei->ei_in_map_n %= IN_MAP_LEN;

		while (bp = CPU_RINGGET(tmpring)) {
			ringput(&addr->fxp_rcv_d, bp);
		}
	}
#endif
}

static int
fxp_setup(register struct fxp_info *ei)
{
	register int i, j;
	register BCB *bp;
	register BCB *bcbs;
	register BUS_D *dp;
	register int xbs;
	register int xbps;
	CPU_RING *tmpring = &ei->tmpring;
	register FXPDEVICE *addr = ei->ei_boardaddr;

	ringinit(tmpring, RING_SIZE);

	/*
	 * Get all the xmit bcbs.
	 */
	xbs = 0;
	while ((bp = ringget(&addr->fxp_hostfree)) != 0) {
		CPU_RINGPUT(tmpring, bp);
		xbs++;
	}
	DP(("\nfxp_setup%d: Xmit bcbs = %d\n", ei->ei_if.if_unit, xbs));

	/*
	 * Setup RECV BCB's for DMA
	 *
	 *	sizeof(BCB) = 32
	 *	sizeof(DSC) = 16
	 *	b_len = 1024
	 *	NUM_RECV_BUFS = 32
	 *
	 *	1 xbcb per DMA_RCV_BCBs = 32 * 32 = 1024 <= 1024
	 *	1 dsc per rcv-buf = 32 dscs = 32 * 16 = 512 <= 1024
	 *	1 xbcb for rcv-dscs
	 *
	 */
	if ((bp = alloc_xbcb(tmpring)) == 0) {
		printf("fxp_setup%d: recv dma desc alloc failed.\n",
				ei->ei_if.if_unit);
		return(-1);
	}
	dp = (BUS_D *)FXP_TO_KPHYS(ei->ei_boardaddr, bp->b_addr);
	ASSERT(NUM_RECV_BUFS <= bp->b_len / sizeof(BUS_D));
	DP(("RDSC: start=%x, len=%d\n", (__psint_t)dp, (int)bp->b_len));

	if ((bp = alloc_xbcb(tmpring)) == 0) {
		printf("fxp_setup%d: recv dma ring alloc failed.\n",
				ei->ei_if.if_unit);
		return(-1);
	}
	ASSERT(NUM_RECV_BUFS <= bp->b_len / sizeof(BCB));
	DP(("RBCB: start=%x, len=%d\n",
				(__int32_t)bp->b_addr, (__int32_t)bp->b_len));
	bp = (BCB *)(FXP_TO_KPHYS(ei->ei_boardaddr, bp->b_addr));
ADDR_CHECK(ei, bp);

	xbs -= 2;
	i = 0;
	do {
		dp[i].bd_link = 0;
		dp[i].bd_addr = 0;		/* Physical bus address */
		dp[i].bd_len  = 0;		/* length in bytes */
		dp[i].bd_mode = FXP_RAM;	/* For VME, Ext normal data */
		dp[i].bd_pad  = 0;

		bp[i].b_link = 0;
		bp[i].b_addr = KPHYS_TO_FXP(&dp[i]);
ADDR_CHECK(ei, &dp[i]);
		bp[i].b_mbufi = -1;
ASSERT(IS_KSEG1(&dp[i]));
		bp[i].b_bus_descr = KPHYS_TO_FXP(&dp[i]);
		CPU_RINGPUT(&ei->ei_rcv_free, &bp[i]);

		i++;
	} while (i < NUM_RECV_BUFS);

	/*
	 * Setup XMIT BCB's for DMA
	 */
	xbps = (xbs * SG_ENTRIES) / ((1 K / sizeof(BUS_D)) + SG_ENTRIES);
	DP(("fxp_setup%d: %d out of %d used for descs.\n",
			ei->ei_if.if_unit, xbps, xbs));

	for (i = 0, bcbs = 0; i < xbps; i++) {
		if ((bp = alloc_xbcb(tmpring)) == 0) {
			printf("fxp_setup%d: WARNING: xmit dsc alloc failed.\n",
				ei->ei_if.if_unit);
			return(-1);
		}
		bp->b_link = KPHYS_TO_FXP(bcbs);
		bcbs = bp;
	};

	for (; bcbs != 0; bcbs = (BCB *)FXP_TO_KPHYS(ei->ei_boardaddr,
						     bcbs->b_link)) {
		dp = (BUS_D *)FXP_TO_KPHYS(ei->ei_boardaddr, bcbs->b_addr);
		for (i = (1 K / sizeof(BUS_D) / SG_ENTRIES); i; i--) {
			if ((bp = CPU_RINGGET(tmpring)) == 0)
				panic("fxp_setup%d: INTERNAL1: xmit ring\n",
						ei->ei_if.if_unit);
			bp->b_link = 0;
			bp->b_addr = KPHYS_TO_FXP(dp);
			bp->b_mbufi = -1;
ADDR_CHECK(ei, dp);
			bp->b_bus_descr = KPHYS_TO_FXP(dp);
#ifdef DEBUG
{
BUS_D *test;
test = (BUS_D *)FXP_TO_KPHYS(ei->ei_boardaddr,bp->b_bus_descr);
if (test != dp)
printf("1:wrote 0x%x, read 0x%x\n", dp, test);
ASSERT(test == dp);
}
#endif

			j = 0;
			do {
				dp[j].bd_link = 0;
				dp[j].bd_addr = 0;
				dp[j].bd_len  = 0;
				dp[j].bd_mode = FXP_XAM;
				dp[j].bd_pad  = 0;
				j++;
			} while (j < SG_ENTRIES);

			CPU_RINGPUT(&ei->ei_free, bp);
			dp = &dp[SG_ENTRIES];
		}
	}

	/*
	 * Purge non-dma xmit bcbs.
	 */
	i = 0;
	while (bp = CPU_RINGGET(tmpring))
		i++;
	DP(("fxp_setup%d: %d xmit bufs purged\n", ei->ei_if.if_unit, i));

	return(0);
}

/*
 * initialize the board
 */
static int
fxp_ifinit(struct etherif *eif, int flags)
{
	int err;
	struct fxp_info *ei = (struct fxp_info *)eif->eif_private;

	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)));
	err = fxp_bdinit(ei);
	if (err)
		return(err);

	ei->ei_if.if_timer = DOG;	/* turn on watchdog */

	ei->ei_addrCmds &= ~(EAC_SET_ALLMULTI|EAC_SET_FILTER);

	if (flags & IFF_ALLMULTI)
		ei->ei_addrCmds |= EAC_SET_ALLMULTI;
	else if (flags & IFF_FILTMULTI)
		ei->ei_addrCmds |= EAC_SET_FILTER;

	if (( (flags & IFF_PROMISC) && !ei->ei_inPromiscMode) ||
	    (!(flags & IFF_PROMISC) &&  ei->ei_inPromiscMode)) {
		ei->ei_inPromiscMode = !ei->ei_inPromiscMode;
		ei->ei_addrCmds |= EAC_PROMISC;
	}

	ei->ei_addrCmds |= EAC_SET_ADDR;
	UpdateBoardParams(ei->ei_addrCmds, ei);
	return(err);
}

static int
fxp_bdinit(register struct fxp_info *ei)
{
	register FXPDEVICE *addr;
	register __uint32_t i;
	register int err;
#ifdef SPL_INTR
	register int s = splimp();
#endif

	err = fxp_reset(ei);
	if (err)
		goto bdinitret;

	addr = ei->ei_boardaddr;
	addr->fxp_mode =
		E_UNDO_TRAILERS|E_DONTINT|E_RCV_DMA|E_XMIT_DMA|E_NO_SRC_ADDR;
	WB();

	/*
	 * let controller know where its "all_ram" region
	 * begins in our address space so it can present
	 * addresses correctly to us.
	 */
/* This wouldn't work as written.  Our real address is 64-bits.  We
 * need to process the address before storing it here.
 */
	addr->fxp_base = KPHYS_TO_FXP(&addr->fxp_ram_rom[0]); WB();
	addr->fxp_intvector = ei->ei_vec; WB();
	addr->fxp_state = 0; WB();
	if (ei->ei_enaddrInitialized) {
		DP(("fxp%d: SET_ADDR: en_addr = %x:%x:%x:%x:%x:%x\n",
				ei->ei_if.if_unit,
				(u_int)ei->ei_enaddr[0],
				(u_int)ei->ei_enaddr[1],
				(u_int)ei->ei_enaddr[2],
				(u_int)ei->ei_enaddr[3],
				(u_int)ei->ei_enaddr[4],
				(u_int)ei->ei_enaddr[5]));
		for (i = 0; i < 6; i++) {
			addr->fxp_baseaddr.ea_vec[i]=ei->ei_enaddr[i]; WB();
		}
		(void) waitfor_fxp(addr);
		addr->fxp_mode |= E_EXAM_LIST; WB();
	}
	addr->fxp_go = FXP_KICK; WB();
	i = 0;
	while (i++ < FXPWAIT && addr->fxp_state != S_FXPRUN)
		DELAY(1000);
	DP(("fxp_bdinit: state = %x(run=%x)\n",
			(int)addr->fxp_state,
			(int)S_FXPRUN));
	if (err = (i >= FXPWAIT)) {
		printf("fxp%d: firmware failed to start\n",ei->ei_if.if_unit);
		goto bdinitret;
	}

	if (err = fxp_setup(ei))
		goto bdinitret;

	/* clear error counters	*/
	bzero(&ei->ei_stat, sizeof(ei->ei_stat));

	/* get ethernet address */
	if (!ei->ei_enaddrInitialized) {
		VMEIN((void*)&addr->fxp_baseaddr, &ei->ei_addr,
			sizeof(ei->ei_addr));
		bcopy((char*)&ei->ei_addr, ei->ei_enaddr,
			sizeof(ei->ei_enaddr));
		ei->ei_enaddrInitialized = 1;
	}

	DP(("fxp%d: ei_addr = %x:%x:%x:%x:%x:%x\n",
				ei->ei_if.if_unit,
				(u_int)ei->ei_addr.ea_vec[0],
				(u_int)ei->ei_addr.ea_vec[1],
				(u_int)ei->ei_addr.ea_vec[2],
				(u_int)ei->ei_addr.ea_vec[3],
				(u_int)ei->ei_addr.ea_vec[4],
				(u_int)ei->ei_addr.ea_vec[5]));
	DP(("fxp%d: en_addr = %x:%x:%x:%x:%x:%x\n",
				ei->ei_if.if_unit,
				(u_int)ei->ei_enaddr[0],
				(u_int)ei->ei_enaddr[1],
				(u_int)ei->ei_enaddr[2],
				(u_int)ei->ei_enaddr[3],
				(u_int)ei->ei_enaddr[4],
				(u_int)ei->ei_enaddr[5]));

	/* Now, enable interrupt */
	addr->fxp_mode &= ~E_DONTINT; WB();

	/*
	 * hang initial DMA recvs
	 */
	fxp_hang(ei);

bdinitret:
#ifdef SPL_INTR
	splx(s);
#endif
	return(err);
}

/*
 * Periodically poll the controller in case an interrupt gets lost.
 */
static void
fxp_watchdog(struct ifnet *ifp)
{
	struct fxp_info *ei = (struct fxp_info *)ifptoeif(ifp)->eif_private;
	FXPDEVICE *addr;
	u_short tmp;
#ifdef SPL_INTR
	int s;
#endif
	ASSERT(IFNET_ISLOCKED(ifp));

	addr = ei->ei_boardaddr;
	ASSERT(0 != addr);

#ifdef SPL_INTR
	s = splimp();
#endif
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING))
		== (IFF_UP|IFF_RUNNING)) {

		tmp = addr->fxp_state;
		if (ei->ei_heartbit != (u_char)tmp) {
			ei->ei_sick = 0;	/* notice successful output */
		} else if (ei->ei_sick >= 2) {
			printf("fxp%d: died and restarted(%x)\n",
				ifp->if_unit, addr->fxp_u.regs.reg_csr);
			(void)fxp_bdinit(ei);
			ifp->if_oerrors++;
		}
		ei->ei_heartbit = (u_char)tmp;

		/* collect error statistics */
#define FET_STAT(nm) i = (int)addr->fxp_u.iface.nm;
#define MUL_STAT(nm,cnt,mul) { \
		int i; \
		FET_STAT(nm); \
		ifp->cnt += (i - ei->ei_stat.nm)*(mul); \
		ei->ei_stat.nm = i; \
		}
#define ADD_STAT(nm,cnt) MUL_STAT(nm,cnt,1)
#define CMN_ERR_STAT(nm,cnt,mul,msg) { \
		int i,j; \
		static char *emsg = msg; \
		FET_STAT(nm); \
		j = (i - ei->ei_stat.nm)*(mul); \
		ifp->cnt += j; \
		ei->ei_stat.nm = i; \
		if (j > 0 \
			&& (emsg != ei->ei_terr \
			|| ei->ei_terr_lbolt <= lbolt)) { \
			cmn_err(CE_ALERT, "fxp%d: %s\n", ifp->if_unit,emsg); \
			ei->ei_terr_lbolt = lbolt+EMSG_DELAY; \
			ei->ei_terr = emsg; \
		} }
#define PRINT_STAT(nm,cnt,mul,msg) { \
		int i,j; \
		static char *emsg = msg; \
		FET_STAT(nm); \
		j = (i - ei->ei_stat.nm)*(mul); \
		ifp->cnt += j; \
		ei->ei_stat.nm = i; \
		if (j > 0 \
		    && (emsg != ei->ei_terr \
			|| ei->ei_terr_lbolt <= lbolt)) { \
			printf("fxp%d: %s\n", ifp->if_unit,emsg); \
			ei->ei_terr_lbolt = lbolt+EMSG_DELAY; \
			ei->ei_terr = emsg; \
		} }

		MUL_STAT(e_mult_retry,	if_collisions,2);
		ADD_STAT(e_one_retry,	if_collisions);

		ADD_STAT(e_fail_retry,	if_oerrors);
		ADD_STAT(e_xmit_buff_err,if_oerrors);
		ADD_STAT(e_silo_underrun,if_oerrors);
		PRINT_STAT(e_late_coll,	if_oerrors,1, "late collision");
		CMN_ERR_STAT(e_lost_carrier,if_oerrors,1,"no carrier: check Ethernet cable");
		ADD_STAT(e_babble,	if_oerrors);
		ADD_STAT(e_xmit_mem_err,if_oerrors);

		ADD_STAT(e_rcv_missed,	if_ierrors);
		ADD_STAT(e_crc_err,	if_ierrors);
		ADD_STAT(e_frame_err,	if_ierrors);
		ADD_STAT(e_rcv_buff_err,if_ierrors);
		ADD_STAT(e_silo_overrun,if_ierrors);
		ADD_STAT(e_rcv_mem_err,	if_ierrors);
	}

	ifp->if_timer = DOG;
#ifdef SPL_INTR
	splx(s);
#endif
}

#ifdef SPRAYDBG
long fxp_rdmas = 0;
long fxp_rpios = 0;
int  nyield = 1;
#endif
/*
 * Ethernet interface interrupt.
 *	Called at splvme(), returns 0 if no work found, 1 otherwise.
 *
 *		splvme	splimp	splenet(local)
 *
 *	IP6(ESD)     3	     5	     5
 *	IP5(ASD)     3	     5	     3
 *
 *	Thus explicit splimp required for IP6
 */
int					/* 0=did nothing, 1=found work */
if_fxpintr(int unit)
{
	register BCB *bcbp;
	register struct fxp_info *ei;
	register FXPDEVICE *addr;
	register __uint32_t running;
	register int found = 0;
	register int yield_len = 0;
	int i;
#if SPL_INTR
	int s = splimp();
#define INT_SPLX(s) splx(s); WB();
#else
#define INT_SPLX(s)
#endif
	int owrtidx;
	RING *rp;

	ASSERT(unit >= 0 && unit < MAXBD);
	NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
		 NETEVENT_LINKUP, NETCNT_NULL, NETRES_INT);

	ei = &fxp_info[unit];
	IFNET_LOCK(&ei->ei_if);
	addr = ei->ei_boardaddr;
	running = ((ei->ei_if.if_flags & (IFF_UP|IFF_RUNNING))
		    == (IFF_UP|IFF_RUNNING));
	if ((!running) || (!addr)) {
		ei->ei_terr_lbolt = 0;
		DP(("fxp%d: stray interrupt\n", unit));
		IFNET_UNLOCK(&ei->ei_if);
		INT_SPLX(s);
		return(found);
	}

	/* free up the xmit queue first */
	while ((bcbp = ringget(&addr->fxp_hostfree)) != (BCB *)0) {
		found++;

ADDR_CHECK(ei, bcbp);

		i = bcbp->b_mbufi;
		ASSERT((i >= 0) && (i < RING_SIZE));
		m_freem(ei->ei_tm[i]);
		ei->ei_tm[i] = NULL;
 		bcbp->b_mbufi = -1;
		CPU_RINGPUT(&ei->ei_free, bcbp);
	}

#ifdef EVEREST
	/*
	 * If IA workaround enabled, process received packets in batches
	 * flushing the IA cache each time we notice wrtidx has been updated.
	 */
	if (io4ia_war) {
		rp = &addr->fxp_rcvdma;
		owrtidx = rp->r_wrtidx;
		io4_flush_cache((paddr_t) addr);
	}
#endif

	running = 0;
	while (bcbp = (BCB *)ringget(&addr->fxp_rcvdma)) {
		found++;
		ASSERT(bcbp->b_mbufi >= 0);

#ifdef EVEREST
		/*
		 * If IA workaround enabled, and we're beginning a new batch
		 * of input packets, and wrtidx has been updated by the board
		 * since the last time we checked, then save the value of
		 * wrtidx and flush the IO4 cache.
		 */
		if (io4ia_war && (rp->r_rdidx == owrtidx) && (rp->r_wrtidx != owrtidx)) {
			owrtidx = rp->r_wrtidx;
			io4_flush_cache((paddr_t) addr);
		}
#endif

		(void)fxp_read(ei, bcbp);
		ASSERT(bcbp->b_mbufi == -1);

#ifdef DEBUG
if (!IS_KSEG1(FXP_TO_KPHYS(ei->ei_boardaddr,bcbp->b_bus_descr)))
printf("fxpintr: FXP_TO_KPHYS(b_bus_descr) == 0x%x\n", FXP_TO_KPHYS(ei->ei_boardaddr,bcbp->b_bus_descr));
ASSERT(!FXP_TO_KPHYS(ei->ei_boardaddr,bcbp->b_bus_descr) || IS_KSEG1(FXP_TO_KPHYS(ei->ei_boardaddr,bcbp->b_bus_descr)));
#endif

		CPU_RINGPUT(&ei->ei_rcv_free, bcbp);
		running++;
#ifdef SPRAYDBG
		fxp_rdmas++;
		if (running > nyield) {
#else
		if (running > 1) {
#endif
			INT_SPLX(s);
			vme_yield(ei->ei_brl, ei->ei_vme_adapter);
#if SPL_INTR
			s = splimp();
#endif
 			running = 0;
		}
	}

#ifndef NOREADBYPIO
	while ((bcbp = ringget(&addr->fxp_tohost)) != 0) {
		ADDR_CHECK(ei, bcbp);
		found++;
		ASSERT(bcbp->b_mbufi <= 0);
		bcbp->b_mbufi = -1;
		yield_len += fxp_read(ei, bcbp);
		bcbp->b_msglen = 0;	/* XXX just paranoia */
		ringput(&addr->fxp_fxpfree, bcbp);

#ifdef SPRAYDBG
		fxp_rpios++;
		if (running > nyield) {
#else
		if (running > 1) {
#endif
			IFNET_UNLOCK(&ei->ei_if);
			INT_SPLX(s);
			vme_yield(ei->ei_brl, ei->ei_vme_adapter);
#if SPL_INTR
			s = splimp();
#endif
			IFNET_LOCK(&ei->ei_if);
			running = 0;
		}
	}
#endif /* !NOREADBYPIO */

	fxp_hang(ei);

	IFNET_UNLOCK(&ei->ei_if);
	INT_SPLX(s);

	return(found);
}

/*
 * receive packet from interface
 */
static int
fxp_read(struct fxp_info *ei, BCB *bcbp)
{
	register u_char *cp;
	register int len;
	int snoopflags = 0;
	struct mbuf *m;
	int i;
	static struct snoopflagmap errorMap[] = {
		{ BCB_FRAME,	SNERR_FRAME },
		{ BCB_OFLO,	SNERR_OVERFLOW },
		{ BCB_CRC,	SNERR_CHECKSUM },
		{ BCB_RXBUF,	SNERR_NOBUFS },
		{ BCB_MEMERR,	SNERR_MEMORY },
		{ BCB_MISSED,	SNERR_NOBUFS },
		{ 0, }
	};

	ASSERT(IFNET_ISLOCKED(&ei->ei_if));
	ei->ei_if.if_ipackets++;

	/*
	 * Use b_mbufi to index into the recv mbuf pointer table.
	 */
	i = bcbp->b_mbufi;
	ASSERT((i == -1) || ((i >= 0) && (i < NUM_RECV_BUFS)));
	if (i >= 0) {
		m = ei->ei_rm[i];
		ei->ei_rm[i] = NULL;
		ASSERT(m);
	} else
		m = NULL;

	/*
	 * Check bad packet
	 */
	if (bcbp->b_stat & BCB_ERR) {
		DP(("fxp_read%d: BCB_ERR = %x\n",
				ei->ei_if.if_unit, bcbp->b_stat));
		if ((bcbp->b_stat & BCB_ALLERRS) != BCB_MISSED)
			snoopflags = snoop_mapflags(errorMap, (u_short)bcbp->b_stat);
	}

	len = bcbp->b_msglen;
	if (len < MIN_RPKT) {
		ei->ei_terr_lbolt = 0;
		printf("fxp%d: packet too small (length = %d)\n",
			   ei->ei_if.if_unit, len);
		snoopflags |= SNERR_TOOSMALL|SN_ERROR;
		if (len < (CRC_LEN + sizeof(struct ether_header)))
			len = CRC_LEN + sizeof(struct ether_header);
	} else if (len > MAX_RPKT) {
		ei->ei_terr_lbolt = 0;
		printf("fxp%d: packet too large (length = %d)\n",
			   ei->ei_if.if_unit, len);
		snoopflags |= SNERR_TOOBIG|SN_ERROR;
		len = MAX_RPKT;
	}
	ei->ei_if.if_ibytes += len;

	/*
	 * Get the ethernet header from the input buffer.
	 */
	if (m) {
		cp = mtod(m, u_char *) + RCVPAD;
		dki_dcache_inval(cp, len);
		m_adj(m, ((RCVPAD+len-CRC_LEN)-sizeof(struct rcvbuf)));
		bcbp->b_mbufi = -1;
	} else {
		cp = (u_char *)FXP_TO_KPHYS(ei->ei_boardaddr, bcbp->b_addr);
		ADDR_CHECK(ei, cp);
		m = m_vget(M_DONTWAIT, RCVPAD+len-CRC_LEN, MT_DATA);
 		if (!m) {
			DP(("fxpget%d: no buff\n",ei->ei_if.if_unit));
			return(0);
		}

		IF_INITHEADER(mtod(m, caddr_t), &ei->ei_if, RCVDATAPAD);
		VMEIN(cp, mtod(m,u_char*)+RCVPAD, len-CRC_LEN);
#ifdef UNROLL
		if (((__psunsigned_t)cp&0x3) == 2) {
			register __uint32_t *dst;
			register int ilen = ((len+2+3)/4);
			register __uint32_t *src = (__uint32_t *)(cp-2);
			dst = (__uint32_t *)(mtod(m,u_char*)+RCVPAD-2);

			/* unroll 64 byte blocks */
			while (ilen >= 16) {
				dst[0] =src[0]; dst[1] =src[1];
				dst[2] =src[2]; dst[3] =src[3];
				dst[4] =src[4]; dst[5] =src[5];
				dst[6] =src[6]; dst[7] =src[7];
				dst[8] =src[8]; dst[9] =src[9];
				dst[10]=src[10];dst[11]=src[11];
				dst[12]=src[12];dst[13]=src[13];
				dst[14]=src[14];dst[15]=src[15];
				src += 16; dst += 16; ilen -= 16;
			}

			/* we have < 64 bytes remaining */
			if (0 != (ilen&8)) {
				dst[0]=src[0];dst[1]=src[1];
				dst[2]=src[2];dst[3]=src[3];
				dst[4]=src[4];dst[5]=src[5];
				dst[6]=src[6];dst[7]=src[7];
				src += 8; dst += 8;
			}
			if (0 != (ilen&4)) {
				dst[0]=src[0];dst[1]=src[1];
				dst[2]=src[2];dst[3]=src[3];
				src += 4; dst += 4;
			}
			if (0 != (ilen&2)) {
				dst[0]=src[0];dst[1]=src[1];
				src += 2; dst += 2;
			}
			if (0 != (ilen&1)) {
				dst[0]=src[0];
			}
		} else {
			VMEIN(cp, mtod(m,u_char*)+RCVPAD, len-CRC_LEN);
		}
#endif
	}
#ifdef BOUNDDEBUG
{
	__uint32_t tlen = len;
	u_char *ba = (u_char *)(mtod(m,u_char*)+RCVPAD);
	fxp_rcvd_bounds.bufs++;
	fxp_rcvd_bounds.frags++;
	fxp_rcvd_bounds.max_frags = 1;
	if ((__uint32_t)ba & 0x1)
		fxp_rcvd_bounds.ch_frags++;
	else if ((__uint32_t)ba & 0x2)
		fxp_rcvd_bounds.hw_frags++;
	else
		fxp_rcvd_bounds.lw_frags++;

	if (tlen & 0x1)
		fxp_rcvd_bounds.ch_len++;
	else if ((__uint32_t)tlen & 0x2)
		fxp_rcvd_bounds.hw_len++;
	else
		fxp_rcvd_bounds.lw_len++;
}
#endif

	/*
	 * Pass m to the ethernet decapsulator.
	 */
#ifdef SPRAYDBG
	if (IF_QFULL(&ipintrq)) {
/*
		printf("fxp%d: IP-Q full\n", ei->ei_if.if_unit);
*/
		IF_DROP(&ipintrq);
		ei->ei_if.if_idrops++;
		ei->ei_if.if_ierrors++;
		m_freem(m);
		return(len);
	}
#endif

	ether_input(&ei->ei_eif, snoopflags, m);
	return(len);
}

/*
 * copy from mbuf chain to transmitter buffer in FXP memory.
 */
static int				/* 0 or errno */
fxp_transmit(struct etherif *eif,       /* on this interface */
	struct etheraddr *edst,		/* with these addresses */
	struct etheraddr *esrc,
	u_short type,			/* of this type */
	struct mbuf *m)			/* send this chain */
{
	register int i;
	register BCB *bcbp;
	register BUS_D *bdp;
	register caddr_t cp;
	register struct mbuf *n;
	register u_int len, mlen;
	register struct ether_header *eh;
	register struct fxp_info *ei = (struct fxp_info *)eif->eif_private;
	register FXPDEVICE *addr = ei->ei_boardaddr;
	int oddlist = 0;
	int ti;

	ASSERT(IFNET_ISLOCKED(eiftoifp(eif)));
	ASSERT(addr != 0);
	bcbp = CPU_RINGGET(&ei->ei_free);
	if (!bcbp) {
		DP(("fxp_transmit%d: !bcbp\n", ei->ei_if.if_unit));
		NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
			 NETEVENT_LINKDOWN, NETCNT_NULL, NETRES_NULL);
		m_freem(m);
		return(ENOBUFS);
	}
	ADDR_CHECK(ei, bcbp);

	/*
	 * Count the number of buffers to go out in the packet.  Conservative
	 * estimate (may be fewer if multiple mbufs span MIN_TPKT_U).
	 */
	for (n = m, len = 0, i = 0; n != 0; n = n->m_next) {
		if ((mlen = n->m_len) == 0)
			continue;
		len += mlen;

		cp = mtod(n, caddr_t);
		if ((__psunsigned_t)cp & 0x3) {
			oddlist = 1;
#ifdef BOUNDDEBUG
			if ((__psunsigned_t)cp & 0x1)
				fxp_xmit_bounds.ch_frags++;
			else
				fxp_xmit_bounds.hw_frags++;
#endif
		} else if ((mlen & 0x3) && (n->m_next != 0)) {
			oddlist = 1;
#ifdef BOUNDDEBUG
		if (mlen & 0x1)
			fxp_xmit_bounds.ch_len++;
		else
			fxp_xmit_bounds.hw_len++;
#endif
		} else if (io_btoct(cp) != io_btoct(cp+mlen-1)) {
#ifdef BOUNDDEBUG
			fxp_xmit_bounds.lw_frags++;
			fxp_xmit_bounds.lw_len++;
#endif
			if ((IO_NBPP-io_poff(cp))&0x3) {
				oddlist = 1;
#ifdef BOUNDDEBUG
			if (mlen & 0x1)
				fxp_xmit_bounds.ch_len++;
			else
				fxp_xmit_bounds.hw_len++;
#endif
			}
			/* We may need multiple buffers per mbuf. */
			i += (io_btoct(cp+mlen-1) - io_btoct(cp));
		}
#ifdef BOUNDDEBUG
		else {
			fxp_xmit_bounds.lw_frags++;
			fxp_xmit_bounds.lw_len++;
		}
#endif
		i++;
	};
#ifdef BOUNDDEBUG
	fxp_xmit_bounds.bufs++;
	fxp_xmit_bounds.frags += i;
	if (i > fxp_xmit_bounds.max_frags)
		fxp_xmit_bounds.max_frags = i;
#endif
	if (len > ETHERMTU) {
		DP(("fxp_transmit%d: too large(%d)\n",
				ei->ei_if.if_unit, (int)len));
		goto xmitret1;
	}

	/* make room for ethernet header */
	if (!M_HASCL(m) && m->m_off > MMINOFF+(EHDR_LEN+ETHERHDRPAD)) {
		m->m_off -= sizeof(*eh);
		m->m_len += sizeof(*eh);
	} else {
		n = m_get(M_DONTWAIT, MT_DATA);
		if (n == 0) {
			DP(("fxp_transmit%d: m_get=0\n",
						ei->ei_if.if_unit));
			goto xmitret1;
		}
		n->m_len = sizeof(*eh);
		n->m_off += ETHERHDRPAD;
		n->m_next = m;
		m = n;
	}

	eh = mtod(m, struct ether_header *);
	*(struct etheraddr *)eh->ether_dhost = *edst;
	*(struct etheraddr *)eh->ether_shost = *esrc;
	eh->ether_type = type;
	len += sizeof(*eh);

#ifdef ONECHAINXMIT
	oddlist = 1;
#endif /* ONECHAINXMIT */

	if ((oddlist) || (len < MIN_TPKT_U) || (i >= SG_ENTRIES-1)) {
#ifdef ALIGNEDXMIT
		n = m_vget(M_DONTWAIT, len, MT_DATA);
#else /* ALIGNEDXMIT */
		n = m_vget(M_DONTWAIT, len+ETHERHDRPAD, MT_DATA);
#endif /* ALIGNEDXMIT */
		if (!n) {
			DP(("fxp_transmit%d: !mbuf\n", ei->ei_if.if_unit));
			goto xmitret1;
		}
#ifndef ALIGNEDXMIT
		n->m_off += ETHERHDRPAD;
#endif /* !ALIGNEDXMIT */
		if (len != m_datacopy(m,0,len,mtod(n,caddr_t))) {
			DP(("fxp_transmit%d: m_datacopy failed.\n",
					ei->ei_if.if_unit));
			m_freem(n);
			goto xmitret1;
		}
		m_freem(m);
		m = n;
		if (len < MIN_TPKT_U)
			len = MIN_TPKT_U;
		m->m_len = len;
	}

	/*
	 * self-snoop must be done prior to dma bus desc. build because
	 * bus-desc. may change mbuf len and off.
	 */
	if (RAWIF_SNOOPING(&ei->ei_rawif)
	    && snoop_match(&ei->ei_rawif, mtod(m, caddr_t), m->m_len)) {
		    ether_selfsnoop(&ei->ei_eif,
				    mtod(m, struct ether_header *),
				    m, sizeof(*eh), len - EHDR_LEN);
	}

#ifdef EVEREST
	/* prepare to build DMA map entries
	 * - avoid TLB flush within a frame.
	 */
	if (ei->ei_out_map_n >= (OUT_MAP_LEN-SG_ENTRIES))
		ei->ei_out_map_n = 0;
#endif /* EVEREST */

	bdp = (BUS_D *)FXP_TO_KPHYS(ei->ei_boardaddr,bcbp->b_bus_descr);
#ifdef DEBUG
if (!IS_KSEG1(bdp))
	printf("bdp=%x, bcbp=%x\n", bdp, bcbp);
#endif
ASSERT(IS_KSEG1(bdp));
ADDR_CHECK(ei, bdp);

	i = 0; n = m;
	do {
		register singlepage;

		singlepage = 1;
		mlen = n->m_len;
		if (mlen) {
			cp = mtod(n, caddr_t);
			if (io_btoct(cp) != io_btoct(cp + mlen - 1)) {
				singlepage = 0;
				mlen = IO_NBPP - io_poff(cp);
				n->m_len -= mlen;
				n->m_off += mlen;
			}

			bdp[i].bd_link = KPHYS_TO_FXP(&bdp[i+1]);
#ifdef EVEREST
			bdp[i].bd_addr = fxp_dmamap(ei->ei_out_map,ei->ei_iamap,
					   ei->ei_out_map_n, cp);
			ei->ei_out_map_n = (ei->ei_out_map_n+1)%OUT_MAP_LEN;
#else
			bdp[i].bd_addr= kvtophys(cp);
#endif
			bdp[i].bd_len = mlen;
			i++;
		}
		if (singlepage)
			n = n->m_next;
	} while (n);

	ASSERT((i > 0) && (i <= SG_ENTRIES));
	bdp[i-1].bd_link = 0;
	bcbp->b_link = 0;

	/* allocate next mbuf pointer table entry and increment the index */
	ti = ei->ei_tmi;
	ASSERT(ei->ei_tm[ti] == NULL);
	ei->ei_tm[ti] = m;
	bcbp->b_mbufi = ti;
	ei->ei_tmi = NEXTI(ti, RING_SIZE);

	bcbp->b_addr = KPHYS_TO_FXP(&bdp[0]);
	bcbp->b_len = len;
ADDR_CHECK(ei, &bdp[0]);

	NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
		 NETEVENT_LINKDOWN, len, NETRES_NULL);

	ASSERT((bcbp->b_mbufi >= 0) && (bcbp->b_mbufi < RING_SIZE));
ASSERT(IS_KSEG1(FXP_TO_KPHYS(ei->ei_boardaddr,bcbp->b_bus_descr)));
	ringput(&addr->fxp_tofxp, bcbp);
	ei->ei_if.if_obytes += len;

#ifdef EVEREST
	/* SKIP dummy dma map entries for pre-fetch */
	/* for even index - just one more. - is odd here now */
	/* for odd index - next 2 more. - is even here now */
	ei->ei_out_map_n += (ei->ei_out_map_n&0x1) ? 1 : 2;
	ei->ei_out_map_n %= OUT_MAP_LEN;
#endif
	return(0);

xmitret1:
ADDR_CHECK(ei, bcbp);
	m_freem(m);
	bcbp->b_mbufi = -1;
ASSERT(!FXP_TO_KPHYS(ei->ei_boardaddr,bcbp->b_bus_descr) || IS_KSEG1(FXP_TO_KPHYS(ei->ei_boardaddr,bcbp->b_bus_descr)));
	CPU_RINGPUT(&ei->ei_free, bcbp);
	return(ENOBUFS);
}

/*
 * Process an ioctl request.
 * 	should be in splimp.
 */
static int
fxp_ioctl(struct lanceif *lif, int cmd, caddr_t data)
{
	int error = 0;
	struct etherif *eif = &lif->lif_eif;
	struct ifnet *ifp = eiftoifp(eif);
	struct mfreq *mfr = (struct mfreq *)data;
	union mkey *key = mfr->mfr_key;
	struct fxp_info *ei = (struct fxp_info *)eif->eif_private;

	ASSERT(IFNET_ISLOCKED(ifp));
	switch (cmd) {
	case SIOCADDMULTI:
		mfr->mfr_value = CalcIndex(key->mk_dhost, sizeof(key->mk_dhost));
		if (LAF_TSTBIT(&ei->ei_laf, mfr->mfr_value)) {
			ASSERT(mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
			break;
		}
		ASSERT(!mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
		LAF_SETBIT(&ei->ei_laf, mfr->mfr_value);
		if (ei->ei_lif.lif_nmulti == 0)
			ei->ei_if.if_flags |= IFF_FILTMULTI;
		ei->ei_lif.lif_nmulti++;
		if ((ifp->if_flags & IFF_ALLMULTI) == 0)
			UpdateBoardParams(EAC_SET_FILTER, ei);
		break;

	case SIOCDELMULTI:
		if (mfr->mfr_value
		    != CalcIndex(key->mk_dhost, sizeof(key->mk_dhost)))
			return (EINVAL);
		if (mfhasvalue(&eif->eif_mfilter, mfr->mfr_value))
			break;
		/*
	 	 * If this multicast address is the last one to map
		 * the bit numbered by mfr->mfr_value in the filter,
		 * clear that bit and update the chip.
	 	 */
		LAF_CLRBIT(&ei->ei_laf, mfr->mfr_value);
		ei->ei_lif.lif_nmulti--;
		if (ei->ei_lif.lif_nmulti == 0)
			ei->ei_if.if_flags &= ~IFF_FILTMULTI;
		if ((ifp->if_flags & IFF_ALLMULTI) == 0)
			UpdateBoardParams(EAC_SET_FILTER, ei);
		break;

	default:
		error = EINVAL;
	}

	return(error);
}

/*
 * probe and attach a single board
 */
static struct etherifops fxpops =
    { fxp_ifinit, fxp_ifreset, fxp_watchdog, fxp_transmit,
      (int (*)(struct etherif *, int, void*))fxp_ioctl };

/* ARGSUSED */
void
if_fxpedtinit(struct edt *edtp)
{
	u_int unit;
	struct fxp_info *ei;
	struct vme_intrs *intp;
	graph_error_t	rc;
	vertex_hdl_t	vmevhdl, fxpvhdl;
	FXPDEVICE *addr;
	struct etheraddr ea;
	struct ps_parms ps_params;
#ifndef _IRIX4
	piomap_t	*piomap;
#endif

	ASSERT(sizeof(BCB) == 32);
	ASSERT(sizeof(RING) == 1032);
	ASSERT(sizeof(BUS_D) == 16);

#ifdef _IRIX4
	if ((intp = edtp->e_intr_info) == 0)
		unit = MAXBD+1;
	else
		unit = intp->v_unit;
#else
	intp = (vme_intrs_t *)edtp->e_bus_info;
	unit = edtp->e_ctlr;
#endif
	if (unit >= MAXBD) {
		printf("fxp%u: bad EDT entry\n", unit);
		return;
	}

	ei = &fxp_info[unit];
	if (0 != ei->ei_boardaddr) {
		printf("fxp%u: duplicate EDT entry\n", unit);
		return;
	}

#ifdef _IRIX4
	addr = (FXPDEVICE*)edtp->e_base;
	ei->ei_vme_adapter = vme_adapter(addr);
#else
	DP(("fxp%u: allocating pio map\n", unit));
	ei->ei_vme_adapter = edtp->e_adap;
	piomap = pio_mapalloc(edtp->e_bus_type, ei->ei_vme_adapter,
			      &edtp->e_space[0], PIOMAP_FIXED, "fxp");
	if (!piomap) {
		printf("fxp%u: PIO map failed\n", unit);
		return;
	}
	DP(("fxp%u: mapping pio map=%x, e_base=%x\n", unit, piomap, edtp->e_base));
	DP(("fxp%u: _type==0x%x, iopaddr==0x%x, size==0x%x, vaddr==0x%x\n",unit,
		edtp->e_space[0].ios_type, edtp->e_space[0].ios_iopaddr,
		edtp->e_space[0].ios_size, edtp->e_space[0].ios_vaddr));
	addr = (FXPDEVICE*)pio_mapaddr(piomap, edtp->e_iobase);
#endif /* _IRIX4 */

	DP(("fxp%u: boardaddr=%x\n", unit, addr));
	DP(("fxp%u: badaddr(0x%x, %d)\n", unit, &addr->fxp_u.fxp_ram[0], sizeof(short)));
	if (badaddr(&addr->fxp_u.fxp_ram[0], sizeof(short))) {
#ifndef _IRIX4
		pio_mapfree(piomap);
#endif
		printf("fxp%u: missing\n", unit);
		return;
	}
	ei->ei_boardaddr = addr;	/* say the board is here */

#ifdef EVEREST
	ei->ei_out_map = dma_mapalloc(DMA_A32VME, ei->ei_vme_adapter,
				      OUT_MAP_LEN,VM_NOSLEEP);
	DP(("fxp%u: out map=%x\n", unit, ei->ei_out_map));
	ei->ei_in_map = dma_mapalloc(DMA_A32VME, ei->ei_vme_adapter,
				      IN_MAP_LEN,VM_NOSLEEP);
	DP(("fxp%u: in map=%x\n", unit, ei->ei_in_map));
	if (!ei->ei_out_map || !ei->ei_in_map) {
		printf("fxp%u: DMA mapping failed\n", unit);
		return;
	}
	ei->ei_iamap = iamap_get(ei->ei_vme_adapter, DMA_A32VME);
	ei->ei_in_map_n = 0;
	ei->ei_out_map_n = 0;
	DP(("fxp%u: IA map=%x\n", unit, ei->ei_iamap));
#endif /* EVEREST */

#ifndef _IRIX4
	if (!intp->v_vec) {
		DP(("fxp%u: allocating IVEC\n", unit));
		intp->v_vec = vme_ivec_alloc(ei->ei_vme_adapter);
		if (!intp->v_vec) {
			DP(("fxp%u: waisting NULL-IVEC\n", unit));
			intp->v_vec = vme_ivec_alloc(ei->ei_vme_adapter);
			if (!intp->v_vec) {
				DP(("fxp%u: COMMOM GIMMEA BREAK NULL-IVEC\n", unit));
				return;
			}
		}
	}
	DP(("fxp%u: IVEC=%x\n", unit, intp->v_vec));

	if (intp->v_vec == (unsigned char)-1) {
		printf("fxp%u: no interrupt vector\n", unit);
		return;
	}
	vme_ivec_set(ei->ei_vme_adapter, intp->v_vec, if_fxpintr, unit);
	DP(("fxp%u: IVEC set!\n", unit));
#endif /* !_IRIX4 */
	ei->ei_vec = intp->v_vec;	/* remember interrupt vector */
	ei->ei_brl = intp->v_brl;	/* and VME bus request level */

	ei->ei_enaddrInitialized = 0;
	ei->ei_inPromiscMode = 0;
	ei->ei_addrCmds = 0;
	ringinit(&ei->ei_free, RING_SIZE);
	ringinit(&ei->ei_rcv_free, RING_SIZE);

	/* setup xmit and recv mbuf pointer arrays */
	bzero(&ei->ei_tm, sizeof (ei->ei_tm));
	bzero(&ei->ei_rm, sizeof (ei->ei_rm));
	ei->ei_tmi = ei->ei_rmi = 0;

	/* get ethernet address */
	VMEIN((void*)&addr->fxp_baseaddr, (u_char *)&ea, sizeof(ea));

	ether_attach(&ei->ei_eif, "fxp", unit, (caddr_t)ei, &fxpops,
			&ea, INV_ETHER_FXP, 0);
	add_to_inventory(INV_NETWORK, INV_NET_ETHER, INV_ETHER_FXP, unit, 0);

	rc = vme_hwgraph_lookup(ei->ei_vme_adapter, &vmevhdl);
	if (rc == GRAPH_SUCCESS) {
		char loc_str[16];
		char alias_name[8];
		sprintf(loc_str, "%s/%d", EDGE_LBL_FXP, unit);
		sprintf(alias_name, "%s%d", EDGE_LBL_FXP, unit);
		rc = if_hwgraph_add(vmevhdl, loc_str, "if_fxp", alias_name,
			&fxpvhdl);
	}

	/* Only support admission control for now */
	ps_params.bandwidth = 10000000;
	ps_params.txfree = 0;
	ps_params.txfree_func = NULL;
	ps_params.state_func = NULL;
	ps_params.flags = PS_DISABLED;
	(void)ps_init(eiftoifp(&ei->ei_eif), &ps_params);

	if (fxp_bdinit(ei))
		return;

	ei->ei_enaddrInitialized = 1;

	if (showconfig) {
		printf("fxp%u: ethernet address %s,  firmware version ",
			   unit, ether_sprintf(ei->ei_enaddr));
		printf("0 (CMC)\n");
	}

#ifdef EVEREST
	if (io4ia_war)
		(void) vme_ivec_ioflush_disable(ei->ei_vme_adapter, intp->v_vec);
#endif
}

/*
 * Wait for FXP to process any previous mode-altering requests before we
 * write to the mode word.  Since the board resets some bits when the operation
 * is complete, waiting insures that we don't write the mode word the same
 * time as the board.
 *
 * FXP_MODE_BITS is defined to be the collection of all bits that cause the
 * board to write the mode word.  The board will change its state to FXPRIP
 * when it's processing the request; it is changed back to FXPRUN when the
 * request has been completed.
 */

#define FXP_MODE_BITS	(E_EXAM_LIST|E_RESET_STATS)
#define	FXP_WAITDELAY	5		/* # microseconds to delay between wait checks */
#define	FXP_WAITMAX	1000000		/* max # waitdelays before returning error */

static int
waitfor_fxp(FXPDEVICE *addr)
{
	int wcount = 0;

	while ((addr->fxp_mode & FXP_MODE_BITS) || (addr->fxp_state == S_FXPRIP)) {
		if (++wcount >= FXP_WAITMAX)
			return (1);
		DELAY(FXP_WAITDELAY);
	}

	return (0);
}

/*
 * UpdateBoardParams --
 *
 * This routine set/reset Promiscuous, ether_addr, and Mcast addr.
 */

static void
UpdateBoardParams(register int cmd, register struct fxp_info *ei)
{
	register int i;
	register FXPDEVICE *addr = ei->ei_boardaddr;

top:

	/*
	 * If we're in promiscous mode, always tell the board in case
	 * it got reset.
	 * Also, tell the board if cmd has the EAC_PROMISC bit set.
	 */
	if (cmd & EAC_PROMISC) {
		if (waitfor_fxp(addr)) {
			fxp_ifinit(&ei->ei_eif, ei->ei_if.if_flags);
			goto top;
		}
		if (ei->ei_inPromiscMode) {
			addr->fxp_mode |= E_PROMISCUOUS;
			DP(("SET PROMISC\n"));
		} else {
			addr->fxp_mode &= ~E_PROMISCUOUS;
			DP(("RESET PROMISC\n"));
		}
		WB();
	}

#ifdef STASH
	Not supported yet.(prom v4.5.2)
	/*
	 * Change the board's ethernet address.
	 */
	if (cmd & EAC_SET_ADDR) {
		DP(("fxp%d: SET_ADDR: en_addr = %x:%x:%x:%x:%x:%x\n",
				ei->ei_if.if_unit,
				(u_int)ei->ei_enaddr[0],
				(u_int)ei->ei_enaddr[1],
				(u_int)ei->ei_enaddr[2],
				(u_int)ei->ei_enaddr[3],
				(u_int)ei->ei_enaddr[4],
				(u_int)ei->ei_enaddr[5]));
		(void) waitfor_fxp(addr);
		addr->fxp_mode |= E_NO_SRC_ADDR; WB();
		for (i = 0; i < 6; i++) {
			addr->fxp_baseaddr.ea_vec[i]=ei->ei_enaddr[i]; WB();
		}
		(void) waitfor_fxp(addr);
		addr->fxp_mode |= E_EXAM_LIST; WB();
	}
#endif

	/*
	 * Change the logical multicast address filter on the board.
	 */
	if (cmd & EAC_SET_FILTER) {
		DP(("SETFILTER %x:%x:%x:%x\n",
				ei->ei_laf.laf_vec[0],
				ei->ei_laf.laf_vec[1],
				ei->ei_laf.laf_vec[2],
				ei->ei_laf.laf_vec[3]));
		if (waitfor_fxp(addr)) {
			fxp_ifinit(&ei->ei_eif, ei->ei_if.if_flags);
			goto top;
		}
		addr->fxp_ethlist.e_mode = EL_SET_LAF;
		for (i = 0; i < 4; i++) {
			addr->fxp_ethlist.e_laf[i] = ei->ei_laf.laf_vec[i];
			WB();
		}
		addr->fxp_mode |= E_EXAM_LIST; WB();
	}

	/*
	 * Special case of EAC_SET_FILTER: cause reception of all
	 * multicast pkts.
	 */
	if (cmd & EAC_SET_ALLMULTI) {
		DP(("ALLMULTI\n"));
		if (waitfor_fxp(addr)) {
			fxp_ifinit(&ei->ei_eif, ei->ei_if.if_flags);
			goto top;
		}
		addr->fxp_ethlist.e_mode = EL_SET_LAF;
		for (i = 0; i < 4; i++) {
			addr->fxp_ethlist.e_laf[i] = 0xffff; WB();
		}
		addr->fxp_mode |= E_EXAM_LIST; WB();
	}

	ei->ei_addrCmds |= cmd;
}
#endif /* !IP20 && !IP22 */
