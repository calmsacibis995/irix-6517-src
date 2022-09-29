/*
 * FDDIXPRESS, 6U VME FDDI board
 * Copyright 1990-1998 Silicon Graphics, Inc.  All rights reserved.
 */
#ident "$Revision: 1.153 $"

/* Do not compile for IP20/IP22 (no VME).
 */
#if !defined(IP20) && !defined(IP22)

/* define IPG_DEBUG  1 */
/* -DIPG_DEBUG to generate debugging code */

#ifdef IPG_DEBUG
#undef DEBUG
#define DEBUG	1		/* turn on all other debugging */
#endif /* IPG_DEBUG */

/* -DIPG_DEBUG_EXTRA to enable additional debugging code for mbufs, etc */
/* define IPG_DEBUG_EXTRA  1 */

#include <tcp-param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/vmereg.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/edt.h>
#include <sys/kopt.h>
#include <sys/invent.h>
#include <sys/capability.h>
#include <sys/pio.h>
#include <sys/dmamap.h>

#ifdef IPG_DEBUG_EXTRA
#include <sys/sema_private.h>
#endif

#if EVEREST
#include <sys/EVEREST/io4.h>
#endif
#include <bstring.h>

#include <net/soioctl.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/raw.h>
#include <net/multi.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <sys/dlsap_register.h>
#include <ether.h>
#include <sys/ddi.h>
#include <sys/fddi.h>
#include <sys/smt.h>
#include <sys/if_ipg.h>
#include <sys/iograph.h>
#include <sys/hwgraph.h>
#include <net/rsvp.h>

extern time_t lbolt;
extern struct ifnet loif;

#ifdef EVEREST
#pragma weak io4ia_war
#pragma weak io4_flush_cache
extern int io4ia_war;			/* fight the IO4 IA chip bug */
extern int io4_flush_cache(paddr_t);
extern void fchip_tlbflush(dmamap_t *);
#endif

extern int ipg_max_board;
extern int ipg_low_latency;
extern int ipg_prom_mode;
extern u_char ipg_cmt[];
extern int ipg_cksum;
extern int ipg_lat_void;
extern int ipg_mtu;

extern int ifcksum, udpcksum;

int if_ipgdevflag = D_MP;

#define DEF_T_REQ (FDDI_DEF_T_REQ & ~31)
#define MIN_T_REQ MIN(FDDI_MIN_T_MIN & ~31, -32)
#define MAX_T_REQ (-0xffffe0)
#define DEF_T_MAX MIN_T_MAX
#define MIN_T_MAX (FDDI_MIN_T_MAX & ~31)
#define MAX_T_MAX MAX(MAX_T_REQ & ~31, -0xffff*32)
#define DEF_TVX (((FDDI_DEF_TVX-254)/255)*255)
#define MIN_TVX MIN(((FDDI_MIN_TVX-254)/255)*255, -255*1)
#define MAX_TVX MAX(((FDDI_MAX_TVX-254)/255)*255, -255*255)


#ifdef IPG_DEBUG_EXTRA
#define IPG_BOGUS1 ((struct mbuf *)0x5a5a5a5a5a5a5a5a)
#define IPG_BOGUS2 ((struct mbuf *)0x4141414141414141)

#define CK_IFNET(pi) ipg_check_ifnet(pi)
#define IPG_METER(x) x

#else
#define CK_IFNET(pi) ASSERT(IFNET_ISLOCKED(&(pi)->pi_if))
#define IPG_METER(x)
#endif /* IPG_DEBUG_EXTRA */

/* DMA parameters
 */
#ifdef EVEREST
int ipg_vme_block = 1;
int ipg_vme_am = VME_A32NPBLOCK + ((0xff & -256)<<8);
#define FRAME_SEC 2000
#endif

#ifndef FRAME_SEC			/* unknown */
int ipg_vme_block = 0;
int ipg_vme_am = VME_A32NPAMOD + ((0xff & -256)<<8);
#define FRAME_SEC 1250
#endif

/* Assume the ratio of data to ack frames is about this
 */
#define ACKRATE 8

/* DMA control info at this rate
 */
int ipg_h2b_dma_delay = USEC_PER_SEC/FRAME_SEC;
int ipg_b2h_dma_delay = USEC_PER_SEC/FRAME_SEC/2;

/* Handling an ordinary input frame makes the host busy for this long.
 * Writing a frame implies the host will be busy that long.
 */
#define BTODELAY(b) (((b)*((USEC_PER_SEC*IPG_BD_MHZ*16)/ \
			   (IPG_BIG_SIZE*FRAME_SEC))) /16)

/* limit interrupts to this many equivalent bytes
 */
int ipg_int_delay = (IPG_BIG_SIZE*ACKRATE/2);

/* Do not let the interrupt delay exceed this.
 */
int ipg_max_host_work = USEC_PER_SEC/FRAME_SEC*ACKRATE/2;

/* Turn off host-to-board DMA after this much idleness.
 * There are races which are prevented by assuming that the worst case
 * host input acknowledgement/processing latency is less than this.  It should
 * be generous.  However, it should not be indefinite, because there is
 * no profit in doing 1000 DMA operations/second without any network traffic.
 * This value delays interrupts from the board, and so should not be too long.
 */
int ipg_h2b_sleep = MAX(USEC_PER_SEC/25, (USEC_PER_SEC/FRAME_SEC)*ACKRATE*8);


/* limits on mbufs given the board
 */
int ipg_num_big = IPG_MAX_BIG*3/4;
int ipg_num_med = IPG_MAX_MED*3/4;
int ipg_num_sml = IPG_MAX_SML;

/* how busy the board should be doing checksums
 */
int ipg_cksum_max = -1;

static char *ipg_name = "ipg";

/* When this driver is dual-MAC, we consider the "primary" board
 *	to be the even unit number.
 *
 * Dual attach stations split PHYs accross ENDECs.  The general rule
 *	is that the PHY associated with the output port of the ENDEC
 *	owns the whole ENDEC.  The input portion of the ENDEC of a
 *	DAS-DM board is tracked by the data structures of the other board.
 *
 *	The first ENDEC of a DAS-SM board is on the primary ring, so its
 *	input is part of PHY-A while its output is part of PHY-B.
 *
 * The SMT data structures owned by the primary board of a dual-MAC pair
 *	are for PHY B.
 */

#define PRIMARY_PI(pi)	(0 == ((pi)->pi_if.if_unit & 1))
#define OTHER_PI(pi)	(PRIMARY_PI(pi) ? ((pi)+1) : ((pi)-1))

#define SMT1(smt,pi)	(&(pi)->pi_smt1 == (smt))
#define OTHER_SMT(pi,smt) (SMT1(smt,pi) ? &(pi)->pi_smt2 : &(pi)->pi_smt1)

#define IS_DASDM(pi)	((pi)->pi_smt1.smt_conf.type == DM_DAS)
#define IS_DASSM(pi)	((pi)->pi_smt1.smt_conf.type == SM_DAS)



/* talk to the board */

#define BAD_BD(ex,bail) if (!(ex)) {			\
	printf("ipg%d: ex\n", pi->pi_if.if_unit);	\
	zapbd(pi,pi->pi_sio);				\
	bail;						\
}


#define BD_DELAY_RESET 30000		/* may take 20 msec to reset */
#define BD_DELAY_RESET_DBG (5*1000*1000)	/* for emulator */
#define BD_DELAY_START 10000		/* may take 20 msec to start */
#define BD_DELAY 3500			/* board should be 10 times faster,
					 *	but let it be slow */

#define IPG_NVRAM_RD_DELAY 500		/* usec delay to read NVRAM */
#define IPG_NVRAM_WR_DELAY 11000	/* usec delay to write NVRAM */

#define BD_DELAY_CHECK 50		/* check this often in usec */


#define ipg_wait(pi,sio) ipg_wait_usec(BD_DELAY,pi,sio,__LINE__)
#define ipg_op(sio,cmd) ((sio)->mailbox = (cmd))



extern char *smt_pc_label[];
static char *str_ls[] = PCM_LS_LABELS;	/* printf labels for line states */

extern int smt_debug_lvl;

#define DEBUGPRINT(pi,a) {if ((pi)->pi_if.if_flags & IFF_DEBUG) printf a;}

#define	ALIGNED(p) (0 == ((__psunsigned_t)(p) % sizeof(__uint32_t)))


extern void bitswapcopy(void *src, void *dst, int len);
extern int looutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);


/* the start of an mbuf containing an output frame */
typedef struct {
	struct {
		u_char	dlen;		/* always OBF_PAD_DLEN */
#define OBF_PAD_LEN 3
		u_char	pad;
		u_short len;		/* -(data bytes) */
	} desc;
	struct fddi fddi;
} IPG_OBUF;


/* minimum input frame */
#define MININ (FDDI_MIN_LLC + sizeof(FDDI_IBUF))

#define MAXIN (FDDI_MAXIN+256)		/* allow others to babble a little */
#define ISIZEOF(x) ((int)sizeof(x))	/* make lint happy */


/* counters for each board
 */
struct ipg_dbg {
	uint	out_copy;		/* had to copy mbufs on output */
	uint	out_alloc;		/* had to alloc mbuf for header */
	uint	out_pullup;		/* had to use m_pullup() */
	uint	own_out;		/* heard our own output */
	uint	pg_cross;		/* buffer crossed page */

	uint	waste_page;		/* input used a cluster but no flip */

	uint	ints;			/* total interrupts */

	uint	board_dead;		/* gave up waiting for the board */
	uint	board_waits;		/* waited for the board */
	uint	board_poke;		/* had to wake up the board */
	uint	drown_on;		/* started drowning */
	uint	drown_off;

	uint	bd_cksums;		/* board offered us a checksum */
	uint	bd_cksum_frag;		/* packet was an IP fragment */
	uint	bd_cksum_bad;		/* bad checksum */
	uint	bd_cksum_proto;		/* wrong protocol */
	uint	bd_cksum_missing;	/* unexpected missing checksum */
	uint	bd_cksum_odd;		/* no checksum because odd buffer */

	uint	no_sml;
	uint	no_med;			/* ran out of this size */
	uint	no_big;

#ifdef IPG_DEBUG_EXTRA
	uint	smlmap_inx_wrap;
	uint	medmap_inx_wrap;
	uint	bigmap_inx_wrap;
	uint	outmap_inx_wrap;
	uint	outmap_inx_restart;
#endif
};

/* info per board
 */
struct ipg_info {
	struct arpcom	pi_ac;		/* common interface structures */
	IPG_OBUF	pi_hdr;		/* prototype MAC & LLC header */
	struct mfilter	pi_filter;
	struct rawif	pi_rawif;

	u_char		pi_macaddr[6];	/* MAC address from the board */

	IPG_SIO		*pi_sio;	/* look for the board here */

	u_char		pi_vector;	/* VME interrupt vector */
	u_char		pi_brl;		/* VME bus priority level */
	int		pi_vme_adapter;	/* which VME adapater */

	ushort		pi_mac_mode;

	uint		pi_state;	/* useful state bits */

	struct ipg_dbg	pi_dbg;		/* debugging counters */
	int		pi_in_active;	/* input recursion preventer */

	struct mbuf	*pi_in;		/* growing input chain */
	int		pi_in_len;
	int		pi_byte_busy;	/* accumulated interrupt delay */

	struct mbuf	*pi_in_smls[IPG_MAX_SML];   /* input buffer pool */
	int		pi_in_sml_num;
	int		pi_in_sml_h;
	int		pi_in_sml_t;
	struct mbuf	*pi_in_meds[IPG_MAX_MED];
	int		pi_in_med_num;
	int		pi_in_med_h;
	int		pi_in_med_t;
	struct mbuf	*pi_in_bigs[IPG_MAX_BIG];
	int		pi_in_big_num;
	int		pi_in_big_h;
	int		pi_in_big_t;

	u_char		pi_hash[256/8]; /* DA filter */
	uint		pi_nuni;	/* # active unicast addresses */
	mval_t		pi_hash_mac;	/* hash value of our MAC address */
	mval_t		pi_hash_bcast;	/* hash value of broadcast address */

	struct sio_counts pi_counts;
	IPG_CNT		pi_old_noise1;	/* previous values to generate */
	IPG_CNT		pi_old_noise2;	/*	SMT events */

	/* smoother for ring latency */
	struct {
		u_long		avg;
		int		age;
		int		newavg;
		int		newage;
	}		pi_rnglat;

	struct timeval	pi_time;	/* free running timer */
	struct timeval	pi_tpc1;
	struct timeval	pi_tpc2;

	struct pi_lem {
		IPG_CNT	usec4_prev;	/* usecs*4 when last measured */
		IPG_CNT	ct_start;	/* short term LEM started */
		IPG_CNT	ct_cur;		/* recent raw LEM count */
		struct timeval time2;	/* duration for current value */
		IPG_CNT	ct2;		/* counts since LEM turned on */
		struct timeval time;	/* total duration */
		IPG_CNT	ct;		/* total count */
	} pi_lem1, pi_lem2;		/* 1st ENDEC, 2nd ENDEC LEM */

	struct smt	pi_smt1;
	struct smt	pi_smt2;	/* PHY-A for DAS-SM */

#ifdef EVEREST
	iamap_t		*pi_iamap;	/* i/o address map for h2b and b2h */
	dmamap_t	*pi_dmamap;	/* h2b and b2h DMA map */

	dmamap_t	*pi_sml_map;	/* input DMA map for small mbufs */
	dmamap_t	*pi_med_map;	/* input DMA map for 2k cluster mbufs*/
	dmamap_t	*pi_big_map;    /* input DMA map for page mbufs */
	dmamap_t	*pi_out_map;	/* output DMA map */

	int		pi_sml_map_inx;
	int		pi_med_map_inx;
	int		pi_big_map_inx;
	int		pi_out_map_inx;
#endif /* EVEREST */

	volatile struct ipg_b2h	*pi_b2hp;
	u_char		pi_b2h_sn;
	volatile struct ipg_b2h	*pi_b2h_buf;
	volatile struct ipg_b2h	*pi_b2h_lim;

	struct ipg_h2b	*pi_h2bp;
	struct ipg_h2b	*pi_h2b_buf;
	struct ipg_h2b	*pi_h2b_lim;

	volatile struct ipg_b2h	pi_b2h0[IPG_B2H_LEN];

	struct ipg_h2b	pi_h2b0[IPG_H2B_LEN];

} ipg_info[IPG_MAXBD];

#define pi_if		pi_ac.ac_if     /* network-visible interface */

/* bits in pi_state */
#define PI_ST_BCHANGE	0x0001		/* optical bypass changed */
#define PI_ST_MAC	0x0002		/* MAC is on */
#define PI_ST_LEM1	0x0004		/* 1st LEM is on */
#define PI_ST_LEM2	0x0008		/* 2nd LEM is on */
#define PI_ST_ADDRSET	0x0010		/* MAC address set by ioctl */
#define PI_ST_ADDRFET	0x0020		/* MAC address fetched from board */
#define PI_ST_ARPWHO	0x0040		/* IP address announcement needed */
#define PI_ST_ZAPPED	0x0080		/* board has been naughty */
#define PI_ST_WHINE	0x0100		/* have whined about sick board */
#define PI_ST_DROWN	0x0200		/* drowning in input--promiscuous? */
#define PI_ST_SLEEP	0x0400		/* H2B DMA is asleep */
#define PI_ST_NOPOKE	0x0800		/* skipped a board wakeup */
#define PI_ST_D_BEC	0x1000		/* board using directed beacon */
#define PI_ST_PSENABLED 0x2000		/* RSVP packet sched. enabled */

#ifdef IPG_DEBUG_EXTRA
#define PI_ST_DRVRINTR  0x4000		/* Entered device driver interrupt */
#endif

static void ipg_alarm(struct smt *smt, struct ipg_info *pi, u_long alarm);
static void ipg_cfm(struct smt *smt, struct ipg_info *pi);
static void ipg_start(struct ipg_info *pi, int flags);
static int ipg_fillin(struct ipg_info*);
static void ipg_b2h(struct ipg_info*, u_long ps_ok);


#ifdef EVEREST
extern iamap_t *iamap_get(int, int);
extern uint ev_kvtophyspnum(caddr_t);

/*
 * The F chip fetches-ahead the second TLB entry of each even/odd pair.
 * That requires that each job consist of an even number of TLB entries.
 * The VMECC and F chip read data ahead and so can require an extra TLB entry.
 * The combination of those requires implies that each job can use two
 * extra TLB entries.
 *
 * If we could be sure that no mbufs cross a 4K boundary, we could
 * reduce the size of the input maps.
 *
 * The DMA map lengths below are in units of number of 4K IO pages required
 * for the various DMA maps
 */
#define OUT_MAP_LEN ((IPG_MAX_DMA_ELEM*2+2)*(IFQ_MAXLEN+1))

#define SML_MAP_LEN (IPG_MAX_SML*4)
#define MED_MAP_LEN (IPG_MAX_MED*4)
#define BIG_MAP_LEN (IPG_MAX_BIG*4)

/* Round a map index to the next even number.
 * Even indeces must be increased by 2.
 */
#define RND_MAP_INX(m) (((m)+2) & ~1)

/* buffer to catch excessively long DMA by the board
 */
#define IPG_MAX_SLOPSIZE ((IO_NBPC*4)/sizeof(u_long))

u_long *ipg_slopp, ipg_slopbuf[IPG_MAX_SLOPSIZE];

/*
 * get and set up a DMA map
 */
static dmamap_t *
ipg_mapalloc(struct ipg_info *pi, int maplen)
{
	dmamap_t *dmamap;
	int index;
	register uint *te;

	if ((dmamap = dma_mapalloc(DMA_A32VME, pi->pi_vme_adapter,
				   maplen, VM_NOSLEEP)) == NULL) { /* failed */
		return NULL;
	}
	/*
	 * Doing only one iamap_map() requires the maps with entries that
	 * are reused be big enough to force the TLB to flush itself.
	 */
	ASSERT(maplen >= 32);
	iamap_map(pi->pi_iamap, dmamap);

	/*
	 * Using address of I/O page within slop buffer
	 * Initialize the map to keep TLB fetch-aheads from trashing things
	 */
	te = pi->pi_iamap->table + dmamap->dma_index;
	for (index = 0; index < maplen; index++) {
		*te++ = SYSTOIOPFN(ev_kvtophyspnum((caddr_t)ipg_slopp),
				   io_btoct(ipg_slopp));
	}
	return dmamap;
}

/*
 * Put an mbuf address into EVEREST DMA map.
 * This function clears it, but the caller must reserve the
 * extra entry used to keep the hardware fetch-ahead from messing up.
 *
 * This assumes that all of the maps are at least 16 entries long
 * and that entries are used consecutively, so that no flushing
 * of F-chip TLB's is necessary.
 */
/* ARGSUSED */
static __uint32_t
ipg_dmamap(void *addr,			/* starting addresses */
	   int len,			/* bytes to map */
	   dmamap_t *dmamap,		/* DMA map */
	   iamap_t *iamap,
	   int *indexp,			/* start using this entry */
	   int maplen			/* length of entire map */
#ifdef IPG_DEBUG_EXTRA
	   , struct ipg_info *pi
#endif /* IPG_DEBUG_EXTRA */
	   )
{
	register uint *te;
	register __psunsigned_t paddr;
	register int index;
	__uint32_t vme_addr;


	/* compute # of pages mapped */
	len = (((__psunsigned_t)addr + len + IO_NBPC-1)/IO_NBPC
	       - (__psunsigned_t)addr/IO_NBPC);

	/* Wrap to the start if not enough entries for both the target
	 * and the pad.
	 */
	index = *indexp;
	if (index+len >= maplen) { /* Do f-chip flush on wrap-around */
		index = 0;
		fchip_tlbflush(dmamap);

#ifdef LATER /* IPG_DEBUG_EXTRA */
		/*
		 * Note: This extra statistics counts caused one
		 * of the VME timing bugs to disappear, hence it's
		 * ifdef'ed special
		 */
		if (dmamap == pi->pi_sml_map) { /* IPG_SML_SIZE */
			IPG_METER(pi->pi_dbg.smlmap_inx_wrap++);
		} else {
			if (dmamap == pi->pi_med_map) { /* IPG_MED_SIZE */
				IPG_METER(pi->pi_dbg.medmap_inx_wrap++);
			} else {
				if (dmamap == pi->pi_big_map) {
				  IPG_METER(pi->pi_dbg.bigmap_inx_wrap++);
				} else {
				  IPG_METER(pi->pi_dbg.outmap_inx_wrap++);
				}
			}
		}
#endif /* LATER IPG_DEBUG_EXTRA */
	}

	/* return with the address as seen on the VME bus
	 */
	vme_addr = (dmamap->dma_addr + index*IO_NBPC
		    + (__psunsigned_t)addr % IO_NBPC);

	te = iamap->table + dmamap->dma_index + index;

	/*
	 * use enough map entries to cover the entire buffer
	 */
	do {
		paddr = SYSTOIOPFN(ev_kvtophyspnum((caddr_t)addr),
			io_btoct(addr));
		*te++ = paddr;
		addr = (char*)addr + IO_NBPC;
		index++;
	} while (--len > 0);

	*indexp = index;
	return vme_addr;
}
#endif /* EVEREST */

#ifdef IPG_DEBUG_EXTRA
/*
 * Check if ifnet lock is held
 */
void
ipg_check_ifnet(struct ipg_info *pi)
{
	if (!(mutex_mine(&(pi->pi_ac.ac_if.if_mutex)))) {
		cmn_err_tag(231,CE_PANIC, "ipg%d: ifnet 0x%x IFNET LOCK NOT HELD\n",
		 pi->pi_if.if_unit, &(pi->pi_ac.ac_if));
	}
}
#endif /* IPG_DEBUG_EXTRA */

/*
 * RSVP
 * Called by packet scheduling functions to determine length of driver queue.
 */
static int
ipg_txfree_len(struct ifnet *ifp)
{
	struct ipg_info *pi = (struct ipg_info *)ifp;
	/*
	 * must do garbage collection here, otherwise, fewer free
	 * spaces may be reported, which would cause packet scheduling
	 * to not send down packets.
	 */
	ipg_b2h(pi, 0);
	return (IFQ_MAXLEN - pi->pi_if.if_snd.ifq_len);
}

/*
 * RSVP
 * Called by packet scheduling functions to notify driver about queueing
 * state.  If setting is not zero then there are queues and driver should
 * update the packet scheduler by calling ps_txq_stat() when the txq length
 * changes. If setting is 0, then don't call packet scheduler.
 */
static void
ipg_setstate(struct ifnet *ifp, int setting)
{
	struct ipg_info *pi = (struct ipg_info *)ifp;

	if (setting)
		pi->pi_state |= PI_ST_PSENABLED;
	else
		pi->pi_state &= ~PI_ST_PSENABLED;
}

static void
zapbd(struct ipg_info *pi, IPG_SIO *sio)
{
	static int ipg_reset_zap = 1;

	pi->pi_state |= PI_ST_ZAPPED;
	if (ipg_reset_zap) {
		sio->mailbox = IPG_SIO_RESET;
		DELAYBUS(BD_DELAY);
		sio->mailbox = 0;
	}
}

static void
ipg_wait_usec(long usec, struct ipg_info *pi, IPG_SIO *sio, ushort lineno)
{
	static int ipg_complain_reset = 0;
	int mailbox;
	long waited;

#if defined(DEBUG) || defined(METER)
#define WAIT_LOG_LEN 32
	static struct wait_log {
		ushort	line;
		ushort	max;
		int	tot;
	} ipg_wait_log[WAIT_LOG_LEN];
	struct wait_log *logp;
	long wcnt;

	logp = &ipg_wait_log[sio->mailbox % WAIT_LOG_LEN];
#endif

	waited = usec;
	for (;;) {
		if ((mailbox = sio->mailbox) == ACT_DONE) {
			pi->pi_state &= ~PI_ST_WHINE;
			break;
		}
		if (waited < 0) {
			IPG_METER(pi->pi_dbg.board_dead++);
			if (!(pi->pi_state & PI_ST_WHINE)) {
				printf("ipg%d: board asleep at %d"
				       " with 0x%x\n",
				       pi->pi_if.if_unit, lineno, mailbox);
				pi->pi_state |= PI_ST_WHINE;
				ipg_alarm(&pi->pi_smt1,pi,SMT_ALARM_SICK);
				if (ipg_complain_reset)
					zapbd(pi,sio);
			}
			break;
		}
		IPG_METER(pi->pi_dbg.board_waits++);
		waited -= BD_DELAY_CHECK;
		DELAY(BD_DELAY_CHECK);
	}

#if defined(DEBUG) || defined(METER)
	wcnt = (usec - waited) / BD_DELAY_CHECK;
	if (wcnt > 0) {
		if (logp->max < wcnt) {
			logp->line = lineno;
			logp->max = (ushort)wcnt;
		}
		logp->tot += wcnt;
	}
#endif
}


/* set the FORMAC mode
 */
static void
ipg_set_mmode(struct ipg_info *pi, IPG_SIO *sio)
{
	int i;

	ipg_wait(pi,sio);
	sio->cmd.mac.multi = (0 != (pi->pi_if.if_flags & IFF_ALLMULTI));

	pi->pi_mac_mode &= ~FORMAC_MADET_PROM;
	if (0 != (pi->pi_if.if_flags & IFF_PROMISC)) {
		pi->pi_mac_mode |= FORMAC_MADET_PROM;
		for (i = 0;
		     i<sizeof(sio->cmd.mac.hash)/sizeof(sio->cmd.mac.hash[0]);
		     i++)
			sio->cmd.mac.hash[i] = -1;
	} else {
		if (0 != pi->pi_nuni || ipg_prom_mode)
			pi->pi_mac_mode |= FORMAC_MADET_PROM;
		hwcpout(&pi->pi_hash[0], &sio->cmd.mac.hash[0],
			sizeof(sio->cmd.mac.hash));
	}

	sio->cmd.mac.mode = pi->pi_mac_mode;

	ipg_op(sio, ACT_MAC_MODE);
}


/* make a FORMAC quiet to set its mode
 */
static void
ipg_quiet_mac(struct ipg_info *pi,
	      IPG_SIO *sio)
{
	ipg_wait(pi,sio);
	sio->cmd.mac_state = MAC_IDLE;
	ipg_op(sio, ACT_MAC_STATE);

	ipg_set_mmode(pi,sio);

	ipg_wait(pi,sio);
	sio->cmd.mac_state = MAC_IDLE;
	ipg_op(sio, ACT_MAC_STATE);
}


/* compute DA hash value
 */
static int
ipg_dahash(u_char *addr)		/* MAC address in FDDI order */
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
ipg_hash1(struct ipg_info *pi,
	  mval_t b)
{
	pi->pi_hash[b/8] |= (((unsigned char)0x80) >> (b % 8));
}

/* turn off a DA hash bit
 */
static void
ipg_hash0(struct ipg_info *pi,
	  mval_t b)
{
	pi->pi_hash[b/8] &= ~(((unsigned char)0x80) >> (b % 8));
}

/* set the MAC address
 */
static void
ipg_set_addr(struct ipg_info *pi,
	     char *addr)
{
	bcopy(addr,pi->pi_ac.ac_enaddr,sizeof(pi->pi_ac.ac_enaddr));

	if (pi->pi_hash_mac != pi->pi_hash_bcast
	    && !mfhasvalue(&pi->pi_filter, pi->pi_hash_mac)) {
		ipg_hash0(pi, pi->pi_hash_mac);
	}

	bitswapcopy(addr, pi->pi_smt1.smt_st.addr.b,
		    sizeof(pi->pi_smt1.smt_st.addr));

	ipg_hash1(pi,
		  pi->pi_hash_mac = ipg_dahash(pi->pi_smt1.smt_st.addr.b));

	ipg_hash1(pi,
		  pi->pi_hash_bcast = ipg_dahash(etherbroadcastaddr));
}

/*
 * Add a destination address.
 */
static int
ipg_add_da(struct ipg_info *pi, union mkey *key, int ismulti)
{
	struct mfreq mfr;

	if (mfmatchcnt(&pi->pi_filter, 1, key, 0))
		return 0;

	mfr.mfr_key = key;
	mfr.mfr_value = ipg_dahash(key->mk_dhost);
	if (!mfadd(&pi->pi_filter, key, mfr.mfr_value)) {
		return ENOMEM;
	}

	ipg_hash1(pi, mfr.mfr_value);	/* tell the board */

	if (!ismulti) {
		++pi->pi_nuni;
	}

	ipg_set_mmode(pi,pi->pi_sio);

	return 0;
}


/* Delete an address filter.  If key is unassociated, do nothing.
 * Otherwise delete software filter first, then hardware filter.
 */
static int
ipg_del_da(struct ipg_info *pi, union mkey *key, int ismulti)
{
	struct mfreq mfr;

	if (!mfmatchcnt(&pi->pi_filter, -1, key, &mfr.mfr_value))
		return 0;

	mfdel(&pi->pi_filter, key);

	/* if no other address has this bit, then tell the board.
	 */
	if (!mfhasvalue(&pi->pi_filter, mfr.mfr_value)
	    && mfr.mfr_value != pi->pi_hash_mac
	    && mfr.mfr_value != pi->pi_hash_bcast) {
		ipg_hash0(pi, mfr.mfr_value);
	}

	if (!ismulti) {
		--pi->pi_nuni;
	}

	ipg_set_mmode(pi,pi->pi_sio);

	return 0;
}

/*
 * Set the output line state
 *	Interrupts must be off
 * Configurations:
 *			SAS		DAS-SM		DAS-DM
 *			--------	--------	--------
 * repeat:		ENDEC0=repeat	ENDEC0=repeat	ENDEC0=repeat
 *  iff_dead()				ENDEC1=repeat
 *			MAC=xx		MAC=xx		MAC=xx
 *							MUX1=xx
 *							MUX2=xx
 *
 * THRU			ENDEC0=TA	ENDEC0=TA	ENDEC0=TA
 *  THRU=1				ENDEC1=repeat
 *  JOIN=1		MAC=RA		MAC=RA		MAC=RA
 *					MUX=xx		MUX1=xx
 *							MUX2=xx
 *
 * WRAP_(A/this)			ENDEC0=TB	ENDEC0=TA
 *  THRU=0				ENDEC1=TA
 *  JOIN=1				MAC=??		MAC=RA
 *					MUX=ENDEC2	MUX1=xx
 *							MUX2=FORMAC
 *
 * ISOLATE (WRAP_B/other)		ENDEC0=TA	ENDEC0=TA
 *  THRU=0				ENDEC1=TB
 *  JOIN=0				MAC=??		MAC=RB
 *					MUX=ENDEC2	MUX1=xx
 *							MUX2=ENDEC R bus
 */
static void
ipg_setls(struct smt *smt,
	  struct ipg_info *pi,
	  enum rt_ls ls)
{
	struct ipg_info *pi2;
	IPG_SIO *sio, *sio2;
	uint what;

	CK_IFNET(pi);
	sio = pi->pi_sio;
	what = (SMT1(smt,pi) ? ACT_PHY1_CMD : ACT_PHY2_CMD);
	ipg_wait(pi,sio);

	switch (ls) {
	case R_PCM_QLS:
		ls = T_PCM_QLS;
	case T_PCM_QLS:
		if (ls == smt->smt_st.tls)
			return;
		sio->cmd.phy = ENDEC_QLS;
		ipg_op(sio,what);
		break;

	case R_PCM_MLS:
		ls = T_PCM_MLS;
	case T_PCM_MLS:
		if (ls == smt->smt_st.tls)
			return;
		sio->cmd.phy = ENDEC_MLS;
		ipg_op(sio,what);
		break;

	case R_PCM_HLS:
		ls = T_PCM_HLS;
	case T_PCM_HLS:
		if (ls == smt->smt_st.tls)
			return;
		sio->cmd.phy = ENDEC_HLS;
		ipg_op(sio,what);
		break;

	case R_PCM_ILS:
		ls = T_PCM_ILS;
	case T_PCM_ILS:
		if (ls == smt->smt_st.tls)
			return;
		sio->cmd.phy = ENDEC_ILS;
		ipg_op(sio,what);
		break;

	case T_PCM_REP:			/* shut down and just repeat */
		if (ls == smt->smt_st.tls)
			return;
		ipg_op(sio,ACT_PHY_REP);
		pi->pi_mac_mode &= ~FORMAC_MMODE_ONL;
		pi->pi_state &= ~PI_ST_MAC;
		break;


	case T_PCM_THRU:
		/* Put this PHY into the ring.
		 */
		if (ls == smt->smt_st.tls)
			return;
		pi->pi_mac_mode |= FORMAC_MSELRA;
		ipg_quiet_mac(pi,sio);

		if (IS_DASSM(pi)) {
			ipg_wait(pi,sio);
			sio->cmd.phy = ENDEC_REPEAT;
			ipg_op(sio, ACT_PHY2_CMD);

			ipg_wait(pi,sio);
			sio->cmd.phy = ENDEC_FILTER;
			ipg_op(sio, ACT_PHY2_CMD);

		} else if (IS_DASDM(pi)) {
			ipg_wait(pi,sio);
			pi2 = OTHER_PI(pi);
			sio2 = pi2->pi_sio;
			pi2->pi_mac_mode |= FORMAC_MSELRA;
			ipg_quiet_mac(pi2,sio2);

			ipg_wait(pi2,sio2);
			sio2->cmd.phy = ENDEC_TA | ENDEC_THRU;
			ipg_op(sio2,ACT_PHY1_CMD);

			ipg_wait(pi2,sio2);
			sio2->cmd.phy = ENDEC_FILTER;
			ipg_op(sio2,ACT_PHY1_CMD);

			ipg_wait(pi2,sio2);
			sio2->cmd.mac_state = MAC_CLAIMING;
			ipg_op(sio2, ACT_MAC_STATE);
		}

		ipg_wait(pi,sio);
		sio->cmd.phy = ENDEC_TA | ENDEC_THRU;
		ipg_op(sio,ACT_PHY1_CMD);

		ipg_wait(pi,sio);
		sio->cmd.phy = ENDEC_FILTER;
		ipg_op(sio,ACT_PHY1_CMD);

		ipg_wait(pi,sio);
		sio->cmd.mac_state = MAC_CLAIMING;
		ipg_op(sio, ACT_MAC_STATE);
		break;


	case T_PCM_WRAP:
		/* Put this MAC and PHY in the ring alone.
		 */
		if (ls == smt->smt_st.tls)
			return;
		if (IS_DASDM(pi)) {
			pi->pi_mac_mode |= FORMAC_MSELRA;
			ipg_quiet_mac(pi,sio);

			pi2 = OTHER_PI(pi);
			sio2 = pi2->pi_sio;
			pi2->pi_mac_mode &= ~FORMAC_MSELRA;
			ipg_quiet_mac(pi2,sio2);

			ipg_wait(pi,sio);
			sio->cmd.mux = MUX_FORMAC;
			ipg_op(sio, ACT_MUX);

			ipg_wait(pi2,sio2);
			sio2->cmd.phy = ENDEC_TA | ENDEC_THRU;
			ipg_op(sio2, ACT_PHY1_CMD);

			ipg_wait(pi2,sio2);
			sio2->cmd.phy = ENDEC_FILTER;
			ipg_op(sio2,ACT_PHY1_CMD);

		} else {
			if (SMT1(smt,pi)) {
				pi->pi_mac_mode &= ~FORMAC_MSELRA;
			} else {
				pi->pi_mac_mode |= FORMAC_MSELRA;
			}
			ipg_quiet_mac(pi,sio);

			ipg_wait(pi,sio);
			sio->cmd.phy = ENDEC_TA | ENDEC_THRU;
			ipg_op(sio,what);

			if (SMT1(smt,pi)) {
				ipg_wait(pi,sio);
				sio->cmd.phy = ENDEC_TB | ENDEC_THRU;
				ipg_op(sio, ACT_PHY2_CMD);
			}

			ipg_wait(pi,sio);
			sio->cmd.phy = ENDEC_FILTER;
			ipg_op(sio,what);
		}
		ipg_wait(pi,sio);
		sio->cmd.mac_state = MAC_CLAIMING;
		ipg_op(sio, ACT_MAC_STATE);
		break;


	case T_PCM_WRAP_AB:
		ASSERT(IS_DASDM(pi));
		if (ls == smt->smt_st.tls)
			return;
		pi->pi_mac_mode &= ~FORMAC_MSELRA;
		ipg_quiet_mac(pi,sio);

		pi2 = OTHER_PI(pi);
		sio2 = pi2->pi_sio;
		pi2->pi_mac_mode &= ~FORMAC_MSELRA;
		ipg_quiet_mac(pi2,sio2);

		ipg_wait(pi,sio);
		sio->cmd.phy = ENDEC_TA | ENDEC_THRU;
		ipg_op(sio, ACT_PHY1_CMD);

		ipg_wait(pi2,sio2);
		sio2->cmd.phy = ENDEC_TA | ENDEC_THRU;
		ipg_op(sio2, ACT_PHY1_CMD);

		ipg_wait(pi,sio);
		sio->cmd.phy = ENDEC_FILTER;
		ipg_op(sio,ACT_PHY1_CMD);

		ipg_wait(pi2,sio2);
		sio2->cmd.phy = ENDEC_FILTER;
		ipg_op(sio2,ACT_PHY1_CMD);

		ipg_wait(pi,sio);
		sio->cmd.mux = MUX_ENDEC;
		ipg_op(sio, ACT_MUX);

		ipg_wait(pi2,sio2);
		sio2->cmd.mux = MUX_ENDEC;
		ipg_op(sio2, ACT_MUX);

		ipg_wait(pi,sio);
		sio->cmd.mac_state = MAC_CLAIMING;
		ipg_op(sio, ACT_MAC_STATE);

		ipg_wait(pi2,sio2);
		sio2->cmd.mac_state = MAC_CLAIMING;
		ipg_op(sio2, ACT_MAC_STATE);

		pi2->pi_smt1.smt_st.tls = ls;
		SMT_LSLOG(&pi2->pi_smt1, T_PCM_WRAP_AB);
		break;


	case T_PCM_LCT:
		if (ls == smt->smt_st.tls)
			return;
		if (IS_DASSM(pi)) {
			sio->cmd.mux = MUX_ENDEC2;
			ipg_op(sio, ACT_MUX);
			ipg_wait(pi,sio);

			sio->cmd.phy = ENDEC_TB | ENDEC_THRU;
			ipg_op(sio, what);
		} else {
			ASSERT(IS_DASDM(pi));
			pi2 = OTHER_PI(pi);
			sio2 = pi2->pi_sio;

			ipg_wait(pi2,sio2);
			sio2->cmd.mux = MUX_ENDEC;
			ipg_op(sio2,ACT_MUX);

			sio->cmd.phy = ENDEC_TB | ENDEC_THRU;
			ipg_op(sio, what);
		}

		ipg_wait(pi,sio);
		sio->cmd.phy = ENDEC_FILTER;
		ipg_op(sio,what);
		break;

	case T_PCM_LCTOFF:
		if (ls == smt->smt_st.tls)
			return;
		sio->cmd.phy = ENDEC_ILS;
		ipg_op(sio,what);
		break;

#ifdef DEBUG
	default:
		cmn_err_tag(232,CE_PANIC, "ipg: unkown line state\n");
#endif
	}

	smt->smt_st.tls = ls;
	SMT_LSLOG(smt, ls);
}

/*
 * accumulate Link Error Monitor experience
 * This must be called at least every 500 seconds or the symbol count
 * will be wrong.
 */
static void
ipg_lem_accum(struct smt *smt,
	      struct ipg_info *pi,
	      struct pi_lem *lp,
	      __int32_t lem_usec4,	/* usec*4 */
	      IPG_CNT lem_ct)		/* raw error count */
{
	struct timeval lem_time;
	uint new_ler2;
	IPG_CNT ct;


	ct = lem_ct - lp->ct_start;
	ct = ((long)ct > 2) ? (ct-2) : 0;	/* forgive two errors */

	lp->ct2 = ct;
	lem_usec4 = (lem_usec4 - lp->usec4_prev)/4;
	lp->time2.tv_sec += lem_usec4 / USEC_PER_SEC;
	lp->time2.tv_usec += lem_usec4 % USEC_PER_SEC;
	RNDTIMVAL(&lp->time2);
	new_ler2 = smt_ler(&lp->time2,ct);  /* short-term LER */
	if (new_ler2 <= smt->smt_conf.ler_cut
	    && new_ler2 < smt->smt_st.ler2)
		ipg_alarm(smt,pi,SMT_ALARM_LEM);
	smt->smt_st.ler2 = new_ler2;

	ct += lp->ct;
	if (ct >= 0x80000000) {
		lp->ct /= 2;
		lp->time.tv_usec /= 2;
		lp->time.tv_sec /= 2;
	}
	lem_time.tv_sec = lp->time.tv_sec + lp->time2.tv_sec;
	lem_time.tv_usec = lp->time.tv_usec + lp->time2.tv_usec;
	RNDTIMVAL(&lem_time);
	smt->smt_st.ler = smt_ler(&lem_time, ct);	/* long-term LER */

	smt->smt_st.lem_count = ct;

	/* Age the short term estimate
	 *	This assumes the deamon will ask at least once a minute.
	 */
	if (lp->time2.tv_sec > 2*LC_EXTENDED/USEC_PER_SEC) {
		time_t secs;

		/* age but avoid rounding problems
		 */
		secs = lp->time2.tv_sec/4;
		lp->time2.tv_sec -= secs;
		lp->time.tv_sec += secs;
		ct = lp->ct2 - (lp->ct2 / 4);	/* round up */
		lp->ct += ct;
		lp->ct_start += ct;
	}
}


/* Get LER and other counters
 *	come with interrupts off.
 */
static int
ipg_st_get(struct smt *macsmt,		/* not used */
	   struct ipg_info *pi)
{
#define CK_CNT(c) (pi->pi_counts.c != counts.c)
#define UP_CNT(s,a,c) (s->smt_st.a.hi += (counts.c < pi->pi_counts.c), \
		       s->smt_st.a.lo += (counts.c - pi->pi_counts.c))
	IPG_SIO *sio;
	struct smt *in1smt;
	struct sio_counts counts;
	struct timeval ntime;
	IPG_CNT ticks, delta;

	CK_IFNET(pi);
	sio = pi->pi_sio;

	/* start gettting the counters
	 */
	ipg_wait(pi,sio);
	ipg_op(sio, ACT_FET_CT);

	macsmt = &pi->pi_smt1;
	if (IS_DASSM(pi)) {
		in1smt = &pi->pi_smt2;
	} else {
		in1smt = &OTHER_PI(pi)->pi_smt1;
	}

	ntime.tv_sec = lbolt/HZ;
	ntime.tv_usec = (lbolt%HZ)*(USEC_PER_SEC/HZ);

	/* if the board is dead, pretend some time has passed so that PCM
	 *	will stop trying.
	 */
	ipg_wait(pi,sio);
	if (ACT_DONE != sio->mailbox) {
		pi->pi_time = ntime;
		return EIO;
	}
	hwcpin(&sio->cmd.counts, &counts, sizeof(counts));

	/* Use the board counter only to increase the resolution beyond
	 *	the system HZ counter.  Time must advance, so fake it
	 *	if necessary.
	 */
	ticks = counts.ticks - pi->pi_counts.ticks;
	ntime.tv_usec += ticks % (USEC_PER_SEC/HZ);
	if (ntime.tv_sec <= pi->pi_time.tv_sec
	    && ntime.tv_usec <= pi->pi_time.tv_usec) {
		ntime.tv_sec = pi->pi_time.tv_sec;
		ntime.tv_usec = pi->pi_time.tv_usec + 100;
		RNDTIMVAL(&ntime);
	}
	pi->pi_time = ntime;

	if (0 != (pi->pi_state & PI_ST_LEM1)) {
		ipg_lem_accum(in1smt,pi, &pi->pi_lem1,
			      counts.lem1_usec4, counts.lem1_ct);
	}
	pi->pi_lem1.ct_cur = counts.lem1_ct;
	pi->pi_lem1.usec4_prev = counts.lem1_usec4;

	UP_CNT(in1smt,eovf,eovf1);
	UP_CNT(in1smt,noise,noise1);

	if (IS_DASSM(pi)) {
		if (0 != (pi->pi_state & PI_ST_LEM2)) {
			ipg_lem_accum(macsmt,pi, &pi->pi_lem2,
				      counts.lem2_usec4, counts.lem2_ct);
		}
		pi->pi_lem2.ct_cur = counts.lem2_ct;
		pi->pi_lem2.usec4_prev = counts.lem2_usec4;

		UP_CNT(macsmt,eovf,eovf2);
		UP_CNT(macsmt,noise,noise2);
	}


	if (CK_CNT(rngop)
	    || CK_CNT(rngbroke)) {
		ipg_op(sio, ACT_FET_TNEG);  /* start fetching T_Neg */

		ipg_alarm(macsmt,pi,SMT_ALARM_RNGOP);	/* tell daemon */
		UP_CNT(macsmt,rngop,rngop);
		UP_CNT(macsmt,rngbroke,rngbroke);
		/* It is always true that
		 *	counts.rngop >= counts.rngbroke
		 *	counts.rngop <= counts.rngbroke+1
		 * Or, that the ring starts broken, and no counts are
		 * missed (because rngbroke is forced).
		 */
		if (counts.rngop != counts.rngbroke) {
			macsmt->smt_st.flags |= PCM_RNGOP;
		} else {
			macsmt->smt_st.flags &= ~PCM_RNGOP;
		}

		ipg_wait(pi,sio);
		macsmt->smt_st.t_neg = ((sio->cmd.t_neg[0]<<16)
					+ sio->cmd.t_neg[1]);
	}


	UP_CNT(macsmt,pos_dup,pos_dup);
	UP_CNT(macsmt,misfrm,misfrm);
	UP_CNT(macsmt,xmtabt,xmtabt);
	UP_CNT(macsmt,tkerr,tkerr);
	UP_CNT(macsmt,clm,clm);
	UP_CNT(macsmt,bec,bec);
	UP_CNT(macsmt,tvxexp,tvxexp);
	UP_CNT(macsmt,trtexp,trtexp);
	UP_CNT(macsmt,tkiss,tkiss);
	UP_CNT(macsmt,myclm,myclm);
	UP_CNT(macsmt,loclm,loclm);
	UP_CNT(macsmt,hiclm,hiclm);
	UP_CNT(macsmt,mybec,mybec);
	UP_CNT(macsmt,otrbec,otrbec);

	UP_CNT(macsmt,frame_ct,frame_ct);
	UP_CNT(macsmt,tok_ct,tok_ct);
	UP_CNT(macsmt,err_ct,err_ct);
	UP_CNT(macsmt,lost_ct,lost_ct);

	UP_CNT(macsmt,flsh,flsh);
	UP_CNT(macsmt,abort,abort);
	UP_CNT(macsmt,miss,miss);
	UP_CNT(macsmt,error,error);
	UP_CNT(macsmt,e,e);
	UP_CNT(macsmt,a,a);
	UP_CNT(macsmt,c,c);
	UP_CNT(macsmt,tot_junk,tot_junk);
	UP_CNT(macsmt,junk_void,junk_void);
	UP_CNT(macsmt,junk_bec,junk_bec);
	UP_CNT(macsmt,longerr,longerr);
	UP_CNT(macsmt,shorterr,shorterr);
	if (CK_CNT(rx_ovf)) {
		UP_CNT(macsmt,rx_ovf,rx_ovf);
		pi->pi_if.if_ierrors += (counts.rx_ovf
					 - pi->pi_counts.rx_ovf);
	}
	UP_CNT(macsmt,buf_ovf,buf_ovf);
	pi->pi_if.if_iqdrops += counts.buf_ovf - pi->pi_counts.buf_ovf;
	if (CK_CNT(tx_under)) {
		DEBUGPRINT(pi,("ipg%d: %u TX underrun\n", pi->pi_if.if_unit,
			(unsigned)(counts.tx_under - pi->pi_counts.tx_under)));
		UP_CNT(macsmt,tx_under,tx_under);
		pi->pi_if.if_oerrors += (counts.tx_under
					 - pi->pi_counts.tx_under);
	}
	UP_CNT(macsmt,dup_mac,dup_mac);

	macsmt->smt_st.mac_state = counts.mac_state;


	/* Ignore wildly different values for a while and until
	 * they are self-consistent.  Average the good values.
	 */
	if (counts.rnglatency < IPG_MAX_RNG_LATENCY) {
# define LAT_SCALE	8192
# define LAT_AGE	256
# define LAT_NEWAGE	5
# define LAT_JITTER	(5*IPG_BD_MHZ/2 * LAT_SCALE/IPG_BD_MHZ)

		ticks = counts.rnglatency * (LAT_SCALE/IPG_BD_MHZ);
		delta = ticks - pi->pi_rnglat.avg;
		if (delta < -LAT_JITTER || delta > LAT_JITTER) {
			delta = ticks - pi->pi_rnglat.newavg;
			if (delta >= -LAT_JITTER*2 && delta <= LAT_JITTER*2) {
				delta /= ++pi->pi_rnglat.newage;
				pi->pi_rnglat.newavg += delta;
				if (pi->pi_rnglat.newage >= LAT_NEWAGE) {
				    pi->pi_rnglat.age = 1;
				    pi->pi_rnglat.avg = pi->pi_rnglat.newavg;
				}
			} else {
				pi->pi_rnglat.newage = 1;
				pi->pi_rnglat.newavg = ticks;
			}
		} else {
			if (pi->pi_rnglat.age < LAT_AGE)
				++pi->pi_rnglat.age;
			pi->pi_rnglat.avg += delta / pi->pi_rnglat.age;
			pi->pi_rnglat.newavg = 0;
			pi->pi_rnglat.newage = 0;
		}

		/* convert scaled board ticks to 80 ns symbol clock */
		macsmt->smt_st.ring_latency = (pi->pi_rnglat.avg
					       / (LAT_SCALE*10/125));
	}

	bcopy(&counts,&pi->pi_counts,sizeof(pi->pi_counts));
#undef CK_CNT
#undef UP_CNT

	return 0;
}


/* Check a proposed configuration
 *	This only checks special restrictions imposed by this driver
 *	and hardware.  It does not do general sanity checks, which should
 *	be done by the SMT demon.
 */
/* ARGSUSED */
static int				/* 0, EINVAL or some other error # */
ipg_st_set(struct smt *smt,
	   struct ipg_info *pi,
	   struct smt_conf *conf)
{
	struct ipg_info *pi2 = OTHER_PI(pi);
	int recon;			/* need to reconnect */

	CK_IFNET(pi);

	ipg_wait(pi,pi->pi_sio);
	ipg_op(pi->pi_sio, ACT_FET_BOARD);
	ipg_wait(pi,pi->pi_sio);

	if (conf->type == DM_DAS) {
		if (!(pi->pi_sio->cmd.bd_type & IPG_BD_TYPE_MAC2)) {
			return EINVAL;
		}

		if (0 == pi2->pi_sio) {	/* dual-MAC requires 2 boards */
			return EINVAL;
		}

		if (iff_alive(pi2->pi_if.if_flags)
		    && pi2->pi_smt1.smt_conf.type != DM_DAS) {
			return EINVAL;	/* the boards must agree */
		}
	}


	recon = 0;

	/* do the easy stuff first
	 */
	bcopy(&conf->mnfdat,
	      &smt->smt_conf.mnfdat,
	      sizeof(smt->smt_conf.mnfdat));
	smt->smt_conf.debug = conf->debug;

	/* clamp & round TVX for the FORMAC.  It wants a negative multiple
	 * of 255*BCLKs between 1 and 255.  Round away from 0.
	 */
	conf->tvx = ((conf->tvx-254)/255)*255;
	if (conf->tvx > MIN_TVX)
		conf->tvx = MIN_TVX;
	else if (conf->tvx < MAX_TVX)
		conf->tvx = MAX_TVX;

	conf->t_req &= ~31;
	if (conf->t_req > MIN_T_REQ)
		conf->t_req = MIN_T_REQ;
	else if (conf->t_req < MAX_T_REQ)
		conf->t_req = MAX_T_REQ;

	/* clamp and round T_Max for the FORMAC.  It wants a
	 * multiple of 32 BCLKs.
	 */
	if (conf->t_max > conf->t_req)
		conf->t_max = conf->t_req;
	conf->t_max &= ~31;
	if (conf->t_max < MAX_T_MAX)
		conf->t_max = MAX_T_MAX;
	else if (conf->t_max > MIN_T_MAX)
		conf->t_max = MIN_T_MAX;

	/* do not change T_MIN */
	conf->t_min = smt->smt_conf.t_min;

	if (smt->smt_conf.t_max != conf->t_max
	    || smt->smt_conf.t_req != conf->t_req
	    || smt->smt_conf.tvx != conf->tvx) {
#ifdef IPG_DEBUG
		printf("ipg%d: change t_max=%x/%x, t_req=%x/%x, tvx=%x/%x\n",
		       pi->pi_if.if_unit,
		       smt->smt_conf.t_max, conf->t_max,
		       smt->smt_conf.t_req, conf->t_req,
		       smt->smt_conf.tvx, conf->tvx);
#endif
		recon = 1;
		smt->smt_conf.t_max = conf->t_max;
		smt->smt_conf.t_req = conf->t_req;
		smt->smt_conf.tvx = conf->tvx;
	}

	if (smt->smt_conf.ler_cut != conf->ler_cut) {
		if (smt->smt_st.ler2 > smt->smt_conf.ler_cut
		    && smt->smt_st.ler2 <= conf->ler_cut)
			ipg_alarm(smt,pi,SMT_ALARM_LEM);
		smt->smt_conf.ler_cut = conf->ler_cut;
	}

	if (smt->smt_conf.type != conf->type
	    || smt->smt_conf.pc_type != conf->pc_type
	    || smt->smt_conf.pcm_tgt != conf->pcm_tgt) {
#ifdef IPG_DEBUG
		printf("ipg%d: change type=%x/%x, pc_type=%x/%x, "
		       "pcm_tgt=%x/%x\n",
		       pi->pi_if.if_unit,
		       smt->smt_conf.type, conf->type,
		       smt->smt_conf.pc_type, conf->pc_type,
		       smt->smt_conf.pcm_tgt, conf->pcm_tgt);
#endif
		recon = 1;
		smt->smt_conf.type = conf->type;
		smt->smt_conf.pc_type = conf->pc_type;
		smt->smt_conf.pcm_tgt = conf->pcm_tgt;
	}

	if (smt->smt_conf.ip_pri != conf->ip_pri) {
#ifdef IPG_DEBUG
		printf("ipg%d: change ip_pri=%x/%x\n",
		       pi->pi_if.if_unit,
		       smt->smt_conf.ip_pri, conf->ip_pri);
#endif
		recon = 1;
		smt->smt_conf.ip_pri = conf->ip_pri;
	}

	if (recon) {
		/* force the MAC(s) to be reset */
		smt_off(smt);
		if (IS_DASSM(pi)) {
			smt_off(OTHER_SMT(pi,smt));
		} else if (IS_DASDM(pi)) {
			smt_off(&pi2->pi_smt1);
		}
	}
	return 0;
}



/* drain a fake frame to awaken the SMT deamon
 */
static void
ipg_alarm(struct smt *smt,
	  struct ipg_info *pi,
	  u_long alarm)
{
	struct mbuf *m;
	FDDI_IBUFLLC *fhp;

	CK_IFNET(pi);

	if (IS_DASSM(pi)) {		/* use the only MAC we have */
		smt = &pi->pi_smt1;
	}
	smt->smt_st.alarm |= alarm;
	if ((m = smt->smt_alarm_mbuf)) {
		smt->smt_alarm_mbuf = 0;
		ASSERT(SMT_LEN_ALARM <= MLEN);
		m->m_len = SMT_LEN_ALARM;
		fhp = mtod(m,FDDI_IBUFLLC*);
		bzero(fhp, SMT_LEN_ALARM);
		IF_INITHEADER(fhp, &pi->pi_if, SMT_LEN_ALARM);
		if (RAWIF_DRAINING(&pi->pi_rawif)) {
			drain_input(&pi->pi_rawif, FDDI_PORT_SMT,
				    (caddr_t)&fhp->fbh_fddi.fddi_mac.mac_sa,m);
		} else {
			m_free(m);
		}
	}
}



/* Build the beacon frame and use if if in the ring
 */
static void
ipg_d_bec(struct smt *smt,
	  struct ipg_info *pi,
	  struct d_bec_frame *info,
	  int len)
{
	IPG_SIO	*sio = pi->pi_sio;
	struct {
		ushort	pad;
		ushort	len;
		struct d_bec_frame msg;
	} frame;

	CK_IFNET(pi);
	bzero(&frame, sizeof(frame));

	if (len >= D_BEC_SIZE-sizeof(info->una)) {
		/* use daemon supplied DA and INFO fields, if supplied */
		bcopy(info,&frame.msg,len);
		frame.len = len-FDDI_FILLER;
		pi->pi_state |= PI_ST_D_BEC;
	} else {
		/* otherwise use 0 DA and INFO fields */
		frame.len = sizeof(struct lmac_frame)-FDDI_FILLER;
		pi->pi_state &= ~PI_ST_D_BEC;
	}

	/* always use the standard FC and SA */
	frame.msg.hdr.mac_fc = MAC_FC_BEACON;
	bcopy(&pi->pi_hdr.fddi.fddi_mac.mac_sa,
	      &frame.msg.hdr.mac_sa,
	      sizeof(frame.msg.hdr.mac_sa));

	ipg_wait(pi,sio);
	hwcpout(&frame.len, &sio->cmd.frame, sizeof(frame));
	ipg_op(sio, ACT_BEACON);

	if (PCM_SET(smt, PCM_CF_JOIN)
	    && (pi->pi_state & PI_ST_D_BEC)) {
		ipg_wait(pi,sio);
		sio->cmd.mac_state = MAC_BEACONING;
		ipg_op(sio, ACT_MAC_STATE);
	}
}



/* start PC-Trace
 *	Interrupts must be off.
 */
static void
ipg_trace_req(struct smt *smt,
	      struct ipg_info *pi)
{
	CK_IFNET(pi);
	TPC_RESET(smt);
	SMT_USEC_TIMER(smt, SMT_TRACE_MAX);
	smt->smt_st.flags &= ~PCM_PC_DISABLE;
	smt_new_st(smt, PCM_ST_TRACE);

	ipg_setls(smt, pi, R_PCM_MLS);
}



/* start the clock ticking for PCM
 */
static void
ipg_reset_tpc(struct smt *smt,
	      struct ipg_info *pi)
{
	(void)ipg_st_get(0,pi);

	if (SMT1(smt,pi)) {
		pi->pi_tpc1 = pi->pi_time;
	} else {
		pi->pi_tpc2 = pi->pi_time;
	}
}


/* Decide what time it is
 */
static time_t				/* elapsed time in usec */
ipg_get_tpc(struct smt *smt,
	    struct ipg_info *pi)
{
	struct timeval *tp;
	time_t tpc;

	(void)ipg_st_get(0,pi);

	if (SMT1(smt,pi)) {
		tp = &pi->pi_tpc1;
	} else {
		tp = &pi->pi_tpc2;
	}
	tpc = pi->pi_time.tv_sec - tp->tv_sec;
	if (tpc >= 0x7fffffff/USEC_PER_SEC)
		return 0x7fffffff;

	return (tpc * USEC_PER_SEC) + pi->pi_time.tv_usec-tp->tv_usec;
}


/* turn off LEM on one ENDEC
 */
static void
ipg_lemshut(struct ipg_info *pi,
	    struct pi_lem *lp,
	    uint st)
{
	if (0 != (pi->pi_state & st)) {

		/* accumulate experience
		 */
		lp->time.tv_sec += lp->time2.tv_sec;
		lp->time.tv_usec += lp->time2.tv_usec;
		RNDTIMVAL(&lp->time);
		lp->ct += lp->ct2;
		if (lp->ct >= 0x80000000) {
			lp->ct /= 2;
			lp->time.tv_sec /= 2;
		}

		pi->pi_state &= ~st;
	}
}


/* Turn LEM off for a PCM
 */
static void
ipg_lemoff(struct smt *smt,
	   struct ipg_info *pi)
{
	if (IS_DASSM(pi)) {
		(void)ipg_st_get(0,pi);
		if (SMT1(smt,pi)) {		/* smt #1 = ENDEC #2 input */
			ipg_lemshut(pi,
				    &pi->pi_lem2,
				    PI_ST_LEM2);
		} else {
			ipg_lemshut(pi,
				    &pi->pi_lem1,
				    PI_ST_LEM1);
		}
	} else {
		ASSERT(IS_DASDM(pi));
		pi = OTHER_PI(pi);
		(void)ipg_st_get(0,pi);
		ipg_lemshut(pi,
			    &pi->pi_lem1,
			    PI_ST_LEM1);
	}
}


/* Turn LEM on
 */
static void
ipg_lemon(struct smt *smt,
	  struct ipg_info *pi)
{
	struct pi_lem *lp;
	uint st;

	if (IS_DASSM(pi)) {
		if (SMT1(smt,pi)) {
			lp = &pi->pi_lem2;
			st = PI_ST_LEM2;
		} else {
			lp = &pi->pi_lem1;
			st = PI_ST_LEM1;
		}
	} else {
		ASSERT(IS_DASDM(pi));
		pi = OTHER_PI(pi);
		lp = &pi->pi_lem1;
		st = PI_ST_LEM1;
	}

	if (0 == (pi->pi_state & st)) {
		(void)ipg_st_get(0,pi);

		lp->time2.tv_sec = 0;
		lp->time2.tv_usec = 0;
		lp->ct_start = lp->ct_cur;
		pi->pi_state |= st;
	}
}


/* Clear the DMA structures
 * The board must be shut down before calling here.
 */
static void
ipg_clrdma(struct ipg_info *pi)
{
	struct mbuf *m;
	int i;

	for (;;) {
		IF_DEQUEUE_NOLOCK(&pi->pi_if.if_snd, m);
		if (!m)
			break;
		m_freem(m);
	}

#ifdef EVEREST
	/* flush the IO4 cache in case the board has written into
	 * any of its mbufs.
	 */
	if (io4_flush_cache != 0 && io4ia_war)
		io4_flush_cache((paddr_t)pi->pi_sio);
#endif
	m_freem(pi->pi_in);
	pi->pi_in = NULL;

	while (pi->pi_in_sml_num != 0) {
#ifdef IPG_DEBUG_EXTRA
		m = pi->pi_in_smls[pi->pi_in_sml_h];
		pi->pi_in_smls[pi->pi_in_sml_h] = NULL;
		m_freem(m);
#else
		m_freem(pi->pi_in_smls[pi->pi_in_sml_h]);
#endif
		pi->pi_in_sml_h = (pi->pi_in_sml_h+1) % IPG_MAX_SML;
		pi->pi_in_sml_num--;
	}

	while (pi->pi_in_med_num != 0) {
#ifdef IPG_DEBUG_EXTRA
		m = pi->pi_in_meds[pi->pi_in_med_h];
		pi->pi_in_meds[pi->pi_in_med_h] = NULL;
		m_freem(m);
#else
		m_freem(pi->pi_in_meds[pi->pi_in_med_h]);
#endif
		pi->pi_in_med_h = (pi->pi_in_med_h+1) % IPG_MAX_MED;
		pi->pi_in_med_num--;
	}

	while (pi->pi_in_big_num != 0) {
#ifdef IPG_DEBUG_EXTRA
		m = pi->pi_in_bigs[pi->pi_in_big_h];
		pi->pi_in_bigs[pi->pi_in_big_h] = NULL;
		m_freem(m);
#else
		m_freem(pi->pi_in_bigs[pi->pi_in_big_h]);
#endif
		pi->pi_in_big_h = (pi->pi_in_big_h+1) % IPG_MAX_BIG;
		pi->pi_in_big_num--;
	}

	pi->pi_h2bp = pi->pi_h2b_buf;
	pi->pi_h2b_lim = pi->pi_h2b_buf + (IPG_H2B_LEN - IPG_MAX_DMA_ELEM-1);
	bzero((char*)pi->pi_h2b_buf, sizeof(pi->pi_h2b0));

	pi->pi_b2hp = pi->pi_b2h_buf;
	pi->pi_b2h_lim = pi->pi_b2h_buf + IPG_B2H_LEN;
	bzero((char*)pi->pi_b2h_buf, sizeof(pi->pi_b2h0));

	pi->pi_b2h_sn = 1;
	for (i = 0; i < IPG_B2H_LEN; i++)
		pi->pi_b2h_buf[i].b2h_sn = (i+1+IPG_B2H_LEN*255)%256;

	pi->pi_state |= PI_ST_SLEEP;
}


/* Reset a MAC because we want to shut everything off or because we want
 *	to join the ring.
 *
 * The caller must worry about zapping the PHY. Interrupts must already be
 *	off.
 */
static void
ipg_macreset(struct ipg_info *pi)
{
	IPG_SIO *sio = pi->pi_sio;

	ipg_wait(pi,sio);
	sio->cmd.mac_state = MAC_OFF;
	ipg_op(sio, ACT_MAC_STATE);
	pi->pi_mac_mode &= ~FORMAC_MMODE_ONL;
	pi->pi_state &= ~PI_ST_MAC;

	ipg_wait(pi,sio);		/* reset fiber DMA */
	ipg_op(sio, ACT_FLUSH);
	ipg_wait(pi,sio);
}



/* initialize the MAC
 *	Interrupts must already be off.
 */
static void
ipg_macinit(struct smt *smt,
	    struct ipg_info *pi)
{
	IPG_SIO	*sio = pi->pi_sio;
	struct {
		ushort	pad;
		ushort	len;
		struct lmac_frame msg;
	} frame;


	ipg_macreset(pi);

	/* install 48-bit source address in prototype header
	 */
	pi->pi_hdr.desc.dlen = OBF_PAD_LEN;
	pi->pi_hdr.fddi.fddi_mac.filler[0] = 0;
	pi->pi_hdr.fddi.fddi_mac.filler[1] = 0;
	pi->pi_hdr.fddi.fddi_mac.mac_bits = 0;
	pi->pi_hdr.fddi.fddi_mac.mac_fc = MAC_FC_LLC(pi->pi_smt1
						     .smt_conf.ip_pri);
	bcopy(&pi->pi_smt1.smt_st.addr,
	      &pi->pi_hdr.fddi.fddi_mac.mac_sa,
	      sizeof(pi->pi_hdr.fddi.fddi_mac.mac_sa));
	pi->pi_hdr.fddi.fddi_llc.llc_c1 = RFC1042_C1;
	pi->pi_hdr.fddi.fddi_llc.llc_c2 = RFC1042_C2;
	pi->pi_hdr.fddi.fddi_llc.llc_etype = htons(ETHERTYPE_IP);


	/* create claim frame.
	 */
	bzero(&frame, sizeof(frame));
	frame.len = sizeof(frame.msg)-FDDI_FILLER;
	frame.msg.macf_hdr.mac_fc = MAC_FC_CLAIM;
	bcopy(&pi->pi_hdr.fddi.fddi_mac.mac_sa,
	      &frame.msg.macf_hdr.mac_sa,
	      sizeof(frame.msg.macf_hdr.mac_sa));
	bcopy(&pi->pi_hdr.fddi.fddi_mac.mac_sa,
	      &frame.msg.macf_hdr.mac_da,
	      sizeof(frame.msg.macf_hdr.mac_da));
	frame.msg.macf_info.info[0] = pi->pi_smt1.smt_conf.t_req;
	ipg_wait(pi,sio);
	hwcpout(&frame.len, &sio->cmd.frame, sizeof(frame));
	ipg_op(sio, ACT_CLAIM);

	/* create standard beacon frame
	 */
	ipg_d_bec(smt, pi, 0, 0);

	/* send MAC address to the board
	 */
	ipg_wait(pi,sio);
	sio->cmd.adrs[0] = -1;		/* SAID */
	hwcpout(&pi->pi_hdr.fddi.fddi_mac.mac_sa.b[0],
		&sio->cmd.adrs[1], 6);
	sio->cmd.adrs[4] = -1;		/* SAGP */
	sio->cmd.adrs[5] = 0;		/* reserved */
	sio->cmd.adrs[6] = pi->pi_smt1.smt_conf.t_req>>16;
	sio->cmd.adrs[7] = pi->pi_smt1.smt_conf.t_req;
	ipg_op(sio, ACT_MAC_ARAM);


	ipg_wait(pi,sio);
	sio->cmd.parms.t_max = -pi->pi_smt1.smt_conf.t_max>>5;
	sio->cmd.parms.tvx = pi->pi_smt1.smt_conf.tvx / 255;
	ipg_op(sio, ACT_MAC_PARMS);

	pi->pi_mac_mode |= FORMAC_MMODE_ONL;
	ipg_set_mmode(pi,sio);
	pi->pi_state |= PI_ST_MAC;

	(void)ipg_fillin(pi);
}



/* Initialize the ENDEC and turn on PHY and PCM interrupts
 *	This must be called with interrupts off.
 */
static void
ipg_phyinit(struct smt *smt, struct ipg_info *pi, IPG_SIO *sio)
{
	smt_clear(smt);

	/* start line state interrupts
	 */
	ipg_wait(pi,sio);
	if (!SMT1(smt,pi)) {		/* smt #2 for DAS-SM */
		ASSERT(IS_DASSM(pi));

		sio->cmd.phy = ENDEC_RESET;	/* use ENDEC #2 outout */
		ipg_op(sio,ACT_PHY2_CMD);
		smt->smt_st.tls = T_PCM_QLS;
		SMT_LSLOG(smt, T_PCM_QLS);

		bzero(&pi->pi_lem1, sizeof(pi->pi_lem1));
		ipg_lem_accum(smt,pi, &pi->pi_lem1, 0, 0);
		pi->pi_state &= ~PI_ST_LEM1;

		ipg_wait(pi,sio);		/* use ENDEC #1 input */
		ipg_op(sio, ACT_PHY_ARM);

		ipg_wait(pi,sio);
		ipg_op(sio, ACT_FET_LS1);
		ipg_wait(pi,sio);
		smt->smt_st.rls = (enum pcm_ls)sio->cmd.ls;

		(void)ipg_st_get(0,pi);
		pi->pi_old_noise1 = smt->smt_st.noise.lo;

	} else if (IS_DASSM(pi)) {		/* smt #1 for DAS-SM */
		sio->cmd.phy = ENDEC_RESET;
		ipg_op(sio,ACT_PHY1_CMD);	/* use ENDEC #1 output */
		smt->smt_st.tls = T_PCM_QLS;
		SMT_LSLOG(smt, T_PCM_QLS);

		bzero(&pi->pi_lem2, sizeof(pi->pi_lem2));
		ipg_lem_accum(smt,pi, &pi->pi_lem2, 0, 0);
		pi->pi_state &= ~PI_ST_LEM2;

		ipg_wait(pi,sio);		/* use ENDEC #2 input */
		ipg_op(sio, ACT_PHY_ARM);

		ipg_wait(pi,sio);
		ipg_op(sio, ACT_FET_LS2);
		ipg_wait(pi,sio);
		smt->smt_st.rls = (enum pcm_ls)sio->cmd.ls;

		(void)ipg_st_get(0,pi);
		pi->pi_old_noise2 = smt->smt_st.noise.lo;

	} else {				/* DAS-DM */
		struct ipg_info *pi2 = OTHER_PI(pi);
		IPG_SIO *sio2 = pi2->pi_sio;
		ASSERT(IS_DASDM(pi) && IS_DASDM(pi2));

		sio->cmd.phy = ENDEC_RESET;	/* output on this ENDEC */
		ipg_op(sio,ACT_PHY1_CMD);
		smt->smt_st.tls = T_PCM_QLS;
		SMT_LSLOG(smt, T_PCM_QLS);

		bzero(&pi2->pi_lem1, sizeof(pi2->pi_lem1));
		ipg_lem_accum(smt,pi, &pi->pi_lem1, 0, 0);
		pi2->pi_state &= ~PI_ST_LEM1;

		ipg_wait(pi2,sio2);		/* input on other board */
		ipg_op(sio2, ACT_PHY_ARM);

		ipg_wait(pi2,sio2);
		ipg_op(sio2, ACT_FET_LS1);
		ipg_wait(pi2,sio2);
		smt->smt_st.rls = (enum pcm_ls)sio2->cmd.ls;

		(void)ipg_st_get(0,pi2);
		pi2->pi_old_noise1 = smt->smt_st.noise.lo;
	}
	SMTDEBUGPRINT(smt,4,("%sR=%s ", PC_LAB(smt),
			     str_ls[(int)smt->smt_st.rls]));
	SMT_LSLOG(smt,smt->smt_st.rls);
}


/* shut down the PCM
 *	This is also the first step in starting. If we are dual-attached and
 *	not wrapped, then killing one PHY zaps both rings.
 *
 * Interrupts must already be off
 */
static void
ipg_pcm_off(struct smt *smt, struct ipg_info *pi)
{
	struct smt *smt2;
	struct ipg_info *pi2;
	IPG_SIO	*sio, *sio2;
	u_long bflag;

	CK_IFNET(pi);
	if (IS_DASDM(pi)) {
		pi2 = OTHER_PI(pi);
		smt2 = &pi2->pi_smt1;
	} else {
		ASSERT(IS_DASSM(pi));
		pi2 = pi;
		smt2 = OTHER_SMT(pi,smt);
	}
	sio = pi->pi_sio;
	sio2 = pi2->pi_sio;

	ipg_lemoff(smt,pi);

	smt->smt_st.flags &= ~(PCM_CF_JOIN | PCM_THRU_FLAG);

	/* just sit tight while the other side is wait for a response
	 * to trace
	 */
	if (smt2->smt_st.pcm_state == PCM_ST_TRACE
	    && !PCM_SET(smt2, PCM_SELF_TEST)
	    && iff_alive(pi->pi_if.if_flags)) {
		if (!PCM_SET(smt, PCM_PC_DISABLE))
			ipg_setls(smt, pi,R_PCM_QLS);
		smt_new_st(smt, PCM_ST_OFF);
		return;
	}

	/* Reset PHY A if it had been withheld and this is PHY B
	 * being shut off.  (see CEM Isolated_Actions)
	 */
	if (SMT1(smt,pi) && IS_DASSM(pi)) {
		if (PCM_SET(smt, PCM_WA_FLAG)) {
			smt->smt_st.flags &= ~(PCM_WA_FLAG | PCM_WAT_FLAG);
			smt2->smt_st.flags &= ~(PCM_WA_FLAG | PCM_WAT_FLAG);
			smt_off(smt2);
		} else if (PCM_SET(smt, PCM_WAT_FLAG)) {
			smt->smt_st.flags &= ~PCM_WAT_FLAG;
			smt2->smt_st.flags &= ~PCM_WAT_FLAG;
			if (PC_MODE_T(smt2))
				smt_off(smt2);
		}
	}

	/* If we are dual-attached and THRU or WRAP_AB, then we have to WRAP.
	 */
	if (PCM_SET(smt2, PCM_CF_JOIN)) {
		smt2->smt_st.flags &= ~PCM_THRU_FLAG;
		if (iff_alive(pi->pi_if.if_flags)
		    && iff_alive(pi2->pi_if.if_flags)) {
			ipg_setls(smt2,pi2,T_PCM_WRAP);
		}
	} else {
		/* if neither PHY is inserted, shut off the MACs
		 */
		ipg_macreset(pi);
		if (pi != pi2)
			ipg_macreset(pi2);
	}


	/* compute new state for the optical bypass
	 */
	bflag = 0;
	if (iff_alive(pi->pi_if.if_flags)
	    && iff_alive(pi2->pi_if.if_flags)
	    && !PCM_SET(smt,PCM_SELF_TEST)
	    && !PCM_SET(smt2,PCM_SELF_TEST)) {
		bflag = PCM_INS_BYPASS;
	}

	/* fiddle with the optical bypasses on the two boards of a
	 * dual-MAC station.
	 * Note that this function is not the only code that changes
	 * the optical bypass.
	 */
	if (bflag != (smt->smt_st.flags & PCM_INS_BYPASS)) {
		ipg_wait(pi,sio);
		ipg_op(sio,
		       ((bflag & PCM_INS_BYPASS) ? ACT_BYPASS_INS
			: ACT_BYPASS_REM));
		pi->pi_state |= PI_ST_BCHANGE;
		smt->smt_st.flags = ((smt->smt_st.flags & ~PCM_INS_BYPASS)
				     | bflag);
	}
	if (bflag != (smt2->smt_st.flags & PCM_INS_BYPASS)) {
		ipg_wait(pi2,sio2);
		ipg_op(sio2,
		       ((bflag & PCM_INS_BYPASS) ? ACT_BYPASS_INS
			: ACT_BYPASS_REM));
		pi2->pi_state |= PI_ST_BCHANGE;
		smt2->smt_st.flags = ((smt2->smt_st.flags & ~PCM_INS_BYPASS)
				      | bflag);
	}

	/* If needed, wait for the bypass to settle.
	 */
	if (0 != (pi->pi_state & PI_ST_BCHANGE)) {
		pi->pi_state &= ~PI_ST_BCHANGE;
		pi2->pi_state &= ~PI_ST_BCHANGE;
		/* this leaves the ENDECs outputting QLS to meet TD_Min */
		ipg_phyinit(smt,pi,sio);
		if (smt2 != smt)
			ipg_phyinit(smt2,pi2,sio2);
		if (bflag & PCM_INS_BYPASS) {
			SMTDEBUGPRINT(smt,0, ("ipg%d: inserted\n",
					      pi->pi_if.if_unit));
			if (PCM_NONE == smt->smt_conf.pcm_tgt)
				ipg_cfm(smt,pi);
		} else {
			SMTDEBUGPRINT(smt,0, ("ipg%d: bypassed\n",
					      pi->pi_if.if_unit));
		}

		if (PCM_CMT == smt->smt_conf.pcm_tgt) {
			smt->smt_st.flags &= ~PCM_BS_FLAG;
			smt_new_st(smt, PCM_ST_BYPASS);
			TPC_RESET(smt);
			if (smt != smt2) {
				smt2->smt_st.flags &= ~PCM_BS_FLAG;
				smt_new_st(smt2, PCM_ST_BYPASS);
				TPC_RESET(smt2);
			}
		}
	}

	if (PCM_CMT == smt->smt_conf.pcm_tgt
	    && !PCM_SET(smt, PCM_PC_DISABLE)) {
		if (PCM_ST_BYPASS == smt->smt_st.pcm_state) {
			/* If we were waiting for the bypass to settle,
			 * then wait some more.
			 */
			SMT_USEC_TIMER(smt,SMT_I_MAX);
			if (smt != smt2)
				SMT_USEC_TIMER(smt2,SMT_I_MAX);
		} else {
			if (!(bflag & PCM_INS_BYPASS)) {
				/* If dead, stay dead.
				 * Put the ENDECs in repeat-mode,
				 * in case there is no optical bypass.
				 */
				smt_new_st(smt, PCM_ST_OFF);
				ipg_setls(smt,pi,T_PCM_REP);
				ipg_setls(smt2,pi2,T_PCM_REP);
			} else {
				/* if up, enter BREAK */
				smt->smt_st.flags &= ~PCM_BS_FLAG;
				smt_new_st(smt, PCM_ST_BREAK);
				ipg_setls(smt, pi,R_PCM_QLS);
				TPC_RESET(smt);
				SMT_USEC_TIMER(smt,SMT_TB_MIN);
			}
		}
	}
}



/* Start Link Confidence Test
 *	Interrupts must be off
 */
static void
ipg_lct_on(struct smt *smt, struct ipg_info *pi)
{
	ipg_setls(smt,pi,T_PCM_LCT);

	ipg_lemon(smt,pi);
}


/* Stop LCT and return results
 *	Interrupts must be off
 */
static int				/* 1==LCT failed */
ipg_lct_off(struct smt *smt, struct ipg_info *pi)
{
	ipg_lemoff(smt,pi);		/* update LER counter */

	/* unhook PHY */
	ipg_setls(smt, pi,T_PCM_LCTOFF);
	return (smt->smt_st.ler2	/* fail if LER bad */
		<= smt->smt_conf.ler_cut);
}



/* Join the ring
 *	Interrupts must be off
 *	This implements much of the Configuration Element Management state
 *	machine.
 */
static void
ipg_cfm(struct smt *smt, struct ipg_info *pi)
{
	struct ipg_info *pi2;
	struct smt *smt2;
	int st;

	CK_IFNET(pi);
	if (PCM_SET(smt, PCM_CF_JOIN))
		return;

	if (IS_DASDM(pi)) {
		pi2 = OTHER_PI(pi);
		smt2 = &pi2->pi_smt1;
	} else {
		ASSERT(IS_DASSM(pi));
		pi2 = pi;
		smt2 = OTHER_SMT(pi,smt);
	}

	/* build claim and beacon frames */
	if (!(pi->pi_state & PI_ST_MAC))
		ipg_macinit(smt,pi);
	if (!(pi2->pi_state & PI_ST_MAC))
		ipg_macinit(smt2,pi2);

	/* restore beacon frame after directed beacon */
	if (pi->pi_state & PI_ST_D_BEC)
		ipg_d_bec(smt, pi, 0, 0);
	if (pi2->pi_state & PI_ST_D_BEC)
		ipg_d_bec(smt2, pi2, 0, 0);


	/* If we are a DAS-DM going from WRAP to THRU, then we have to change
	 *	both MACs.  Since we are dual-MAC and will go into both
	 *	rings claiming, we do not have to strip.
	 *
	 * If we are dual-attached and the other PHY is wrapped, then
	 *	we have to unwrap it.  If the other PHY is not alive,
	 *	then we must wrap.
	 *
	 * Zap PHY A if this is B and we are in a tree and not
	 * single attached or dual-MAC.  (See CEM Insert_Actions)
	 */
	st = 0;
	if (SMT1(smt,pi) && IS_DASSM(pi)) {
		if (PC_MODE_T(smt)) {
			smt->smt_st.flags |= PCM_WA_FLAG;
			smt2->smt_st.flags |= PCM_WA_FLAG;
			st = 1;
		} else {
			smt->smt_st.flags |= PCM_WAT_FLAG;
			smt2->smt_st.flags |= PCM_WAT_FLAG;
			if (PC_MODE_T(smt2))
				st = 1;
		}
	}

	smt->smt_st.flags |= PCM_CF_JOIN;
	if (st) {
		ipg_setls(smt, pi, T_PCM_WRAP);
		smt_off(smt2);
	} else if (PCM_SET(smt2, PCM_CF_JOIN)) {
		smt->smt_st.flags |= PCM_THRU_FLAG;
		ipg_setls(smt, pi, T_PCM_THRU);
		smt2->smt_st.flags |= PCM_THRU_FLAG;
		ipg_setls(smt2, pi, T_PCM_THRU);
	} else {
		ipg_setls(smt, pi, T_PCM_WRAP);
	}

	ipg_lemon(smt,pi);

	if (pi->pi_state & PI_ST_ARPWHO) {
		pi->pi_state &= ~PI_ST_ARPWHO;
		ARP_WHOHAS(&pi->pi_ac, &pi->pi_ac.ac_ipaddr);
	}
}


/* help propagate TRACE upstream by finding the other PCM state machine
 */
static struct smt*
ipg_osmt(struct smt *smt, struct ipg_info *pi)
{
	if (IS_DASDM(pi)) {
		return &OTHER_PI(pi)->pi_smt1;
	} else {
		return OTHER_SMT(pi,smt);
	}
}

#include "ipg/ipg.firm"


/* download a single word to the board
 */
static int				/* 0=bad board */
ipg_downone(struct ipg_info *pi,
	    IPG_SIO *sio,
	    paddr_t addr,
	    ushort val0, ushort val1)
{
	int dwn_cnt;

	dwn_cnt = BD_DELAY/1;
	while (sio->dwn_ad != IPG_DWN_RDY) {
		if (--dwn_cnt == 0) {	/* give up if the board dies */
			printf("ipg%d: download failed with %x,%x,%x\n",
			       pi->pi_if.if_unit,
			       sio->dwn_val[0],
			       sio->dwn_val[1],
			       sio->dwn_ad);
			return 0;
		}
		DELAYBUS(1);
	}

	sio->dwn_val[0] = val0;
	sio->dwn_val[1] = val1;
	sio->dwn_ad = addr;

	return 1;
}


/* give the board a personality
 */
static int				/* 0=bad board */
ipg_download(struct ipg_info *pi)
{
	IPG_SIO	*sio = pi->pi_sio;
	paddr_t addr;
	u_char *fp;
#define SADDR0 ((ipg_txt[0]<<8) + ipg_txt[1])
#define SADDR1 ((ipg_txt[2]<<8) + ipg_txt[3])
#define FIRMSTART 4
	u_char op;
	ushort v0,v1;
	int i, j;

	if (0 != (pi->pi_state & (PI_ST_LEM1 | PI_ST_LEM2))) {
		(void)ipg_st_get(0,pi);
		ipg_lemshut(pi,&pi->pi_lem1,PI_ST_LEM1);
		ipg_lemshut(pi,&pi->pi_lem2,PI_ST_LEM2);
	}

	/* reset the board
	 */
	sio->sign = (ushort)~IPG_SGI_SIGN;
	sio->mailbox = IPG_SIO_RESET;
	DELAYBUS(BD_DELAY);
	sio->mailbox = 0;
	DELAYBUS(BD_DELAY);

	pi->pi_state &= ~PI_ST_ZAPPED;

	/* Resetting the board turns things off.  So mark everything off.
	 */
	ipg_clrdma(pi);
	pi->pi_state &= ~PI_ST_MAC;
	pi->pi_smt1.smt_st.flags &= (~(PCM_INS_BYPASS | PCM_HAVE_BYPASS)
				     & PCM_SAVE_FLAGS);
	pi->pi_smt2.smt_st.flags &= (~(PCM_INS_BYPASS | PCM_HAVE_BYPASS)
				     & PCM_SAVE_FLAGS);
	pi->pi_state |= PI_ST_BCHANGE;
	bzero(&pi->pi_counts, sizeof(pi->pi_counts));
	bzero(&pi->pi_rnglat, sizeof(pi->pi_rnglat));

	i =  (0 != ((pi)->pi_if.if_flags & IFF_DEBUG)
	      ? (BD_DELAY_RESET_DBG/BD_DELAY_CHECK)
	      : (BD_DELAY_RESET    /BD_DELAY_CHECK));
	for (;;) {
		v0 = sio->sign;
		v1 = sio->dwn_ad;
		if (v0 == IPG_SGI_SIGN && v1 == IPG_DWN_RDY)
			break;
		if (--i <= 0) {
			printf("ipg%d: failed to reset: sig=%x flag=%x\n",
			       pi->pi_if.if_unit, v0, v1);
			return 0;
		}
		DELAYBUS(BD_DELAY_CHECK);
	}

	/* download the "firmware"
	 */
	fp = &ipg_txt[FIRMSTART];
	addr = 1;
	while (fp < &ipg_txt[sizeof(ipg_txt)]) {
#define DDATA 0
		op = *fp++;
		if (op < ipg_DZERO) {
			j = op-DDATA+1;
			while (j-- != 0) {
				v0 = *fp++ << 8;
				v0 += *fp++;
				v1 = *fp++ << 8;
				v1 += *fp++;
				if (!ipg_downone(pi,sio, addr++, v0,v1)) {
					return 0;
				}
			}
		} else {
			if (op < ipg_DNOP) {
				j = op-ipg_DZERO+1;
				v0 = 0;
				v1 = 0;
			} else {
				j = op-ipg_DNOP+1;
				v0 = 0x7040;
				v1 = 0x0101;
			}
			while (j-- != 0) {
				if (!ipg_downone(pi,sio, addr++, v0,v1)) {
					return 0;
				}
			}
		}
	}
	if (!ipg_downone(pi,sio, IPG_DWN_GO,	/* start it */
			 SADDR0,SADDR1)) {
		return 0;
	}
	i = BD_DELAY_START/BD_DELAY_CHECK;
	for (;;) {
		v0 = sio->mailbox;
		if (v0 == ACT_DONE)
			break;
		if (--i <= 0) {
			printf("ipg%d: firmware failed to start: flag=%x\n",
			       pi->pi_if.if_unit, v0);
			return 0;
		}
		DELAYBUS(BD_DELAY_CHECK);
	}


	sio->cmd.vme.block = ipg_vme_block;
	sio->cmd.vme.brl = pi->pi_brl;
	sio->cmd.vme.vector = pi->pi_vector;
	sio->cmd.vme.am = ipg_vme_am;

	sio->cmd.vme.flags = (ipg_lat_void ? IPG_LAT : 0);

	if (!ipg_cksum) {
		i = 0;
	} else if (ipg_cksum_max < 0) {
		i = 8;
	} else {
		i = ipg_cksum_max;
	}
	sio->cmd.vme.cksum_max[0] = i >> 16;
	sio->cmd.vme.cksum_max[1] = i;

#ifdef EVEREST
	addr = dma_mapaddr(pi->pi_dmamap, (caddr_t)pi->pi_b2h_buf);
#else
	addr = kvtophys((void*)pi->pi_b2h_buf);
#endif
	sio->cmd.vme.b2h_buf[0] = addr >> 16;
	sio->cmd.vme.b2h_buf[1] = addr;

	sio->cmd.vme.b2h_len = IPG_B2H_LEN;
	sio->cmd.vme.max_bufs = ipg_num_big + ipg_num_med + ipg_num_sml;
	sio->cmd.vme.sml_size = IPG_SML_SIZE;
	sio->cmd.vme.pad_in_dma = sizeof(FDDI_IBUF);

#ifdef EVEREST
	addr = dma_mapaddr(pi->pi_dmamap, (caddr_t)pi->pi_h2b_buf);
#else
	addr = kvtophys(pi->pi_h2b_buf);
#endif
	sio->cmd.vme.h2b_buf[0] = addr >> 16;
	sio->cmd.vme.h2b_buf[1] = addr;

	if (ipg_low_latency) {
		sio->cmd.vme.h2b_dma_delay[0] = 0;
		sio->cmd.vme.h2b_dma_delay[1] = 1;
		sio->cmd.vme.b2h_dma_delay[0] = 0;
		sio->cmd.vme.b2h_dma_delay[1] = 1;
		sio->cmd.vme.int_delay[0] = 0;
		sio->cmd.vme.int_delay[1] = 1;
		sio->cmd.vme.max_host_work[0] = 0;
		sio->cmd.vme.max_host_work[1] = 0;
		sio->cmd.vme.h2b_sleep[0] = 0;
		sio->cmd.vme.h2b_sleep[1] = 1;
	} else {
		sio->cmd.vme.h2b_dma_delay[0] = ipg_h2b_dma_delay >> 16;
		sio->cmd.vme.h2b_dma_delay[1] = ipg_h2b_dma_delay;
		sio->cmd.vme.b2h_dma_delay[0] = ipg_b2h_dma_delay >> 16;
		sio->cmd.vme.b2h_dma_delay[1] = ipg_b2h_dma_delay;
		i = BTODELAY(ipg_int_delay);
		sio->cmd.vme.int_delay[0] = i >> 16;
		sio->cmd.vme.int_delay[1] = i;
		sio->cmd.vme.max_host_work[0] = ipg_max_host_work >> 16;
		sio->cmd.vme.max_host_work[1] = ipg_max_host_work;
		sio->cmd.vme.h2b_sleep[0] = ipg_h2b_sleep >> 16;
		sio->cmd.vme.h2b_sleep[1] = ipg_h2b_sleep;
	}

	ipg_op(sio, ACT_VME);

	/* See if the board is really alive.
	 */
	ipg_wait(pi,sio);
	i = (ACT_DONE == sio->mailbox);

	return i;
}


/* reset a board because we do not want to play any more at all, because
 *	a chip just paniced, or because we need to change the MAC address.
 *
 *	If we are playing dual-attachment, then both PHYs must be reset.
 *	Interrupts must already be off.
 */
static void
ipg_reset(struct ipg_info *pi, int frc)
{
	u_char dasdm;
	int unit = pi->pi_if.if_unit;
	IPG_SIO	*sio = pi->pi_sio;
	struct ipg_info *pi2;
	u_char cksum;
	union {
		IPG_NVRAM ram;
		ushort align;		/* align for hwcpin() */
	} nv;
	int i;


	pi2 = OTHER_PI(pi);

	pi->pi_state &= ~PI_ST_WHINE;	/* allow more complaints */

	/* Reset the board if turning it off.
	 *	See if it is alive if turning it on, and only reset it if
	 *	dead.
	 */
	if (0 != (pi->pi_state & PI_ST_ZAPPED)
	    || 0 == sio->mailbox
	    || IPG_SGI_SIGN != sio->sign
	    || IPG_DWN_RDY != sio->dwn_ad) {
		frc = 1;

	} else if (!frc) {
		ipg_wait(pi,sio);
		if (ACT_DONE == sio->mailbox) {
			ipg_op(sio, ACT_WAKEUP);
			ipg_wait(pi,sio);
		}
		frc = (ACT_DONE != sio->mailbox);
	}
	if (frc && !ipg_download(pi)) {
		pi->pi_if.if_flags &= ~IFF_ALIVE;
		if_down(&pi->pi_if);
		return;
	}

	/* get the MAC address
	 */
	if (0 == (pi->pi_state & PI_ST_ADDRFET)) {
		ipg_wait(pi,sio);
		i = sio->mailbox;
		ipg_op(sio, ACT_FET_NVRAM);
		ipg_wait_usec(IPG_NVRAM_RD_DELAY+BD_DELAY,pi,sio,__LINE__);
		hwcpin(&sio->cmd.nvram, &nv.ram,
		       sizeof(nv.ram));
		IPG_NVRAM_SUM(cksum,&nv.ram);
		if (cksum != IPG_NVRAM_CKSUM
		    || 0 != nv.ram.zeros[0]
		    || bcmp(&nv.ram.zeros[0],&nv.ram.zeros[1],
			    sizeof(nv.ram.zeros)-1)) {
			printf("ipg%d: bad NVRAM contents\n",
			       pi->pi_if.if_unit);
			pi->pi_if.if_flags &= ~IFF_ALIVE;
		} else if (ACT_DONE != sio->mailbox
			   || ACT_DONE != i) {
			printf("ipg%d: failed to get NVRAM\n",
			       pi->pi_if.if_unit);
			pi->pi_if.if_flags &= ~IFF_ALIVE;
		} else {
			pi->pi_state |= PI_ST_ADDRFET;
			bcopy(nv.ram.addr, pi->pi_macaddr,
			      sizeof(pi->pi_macaddr));
			if (pi->pi_macaddr[0] != SGI_MAC_SA0
			    || pi->pi_macaddr[1] != SGI_MAC_SA1
			    || pi->pi_macaddr[2] != SGI_MAC_SA2) {
				printf("ipg%d: unlikely NVRAM MAC address\n",
				       pi->pi_if.if_unit);
			}
			if (0 == (pi->pi_state & PI_ST_ADDRSET)) {
				ipg_set_addr(pi, nv.ram.addr);
			}
		}
	}


	pi->pi_smt1.smt_conf.debug = smt_debug_lvl;
	pi->pi_smt2.smt_conf.debug = smt_debug_lvl;

	ipg_wait(pi,sio);
	ipg_op(sio, ACT_FET_BOARD);
	ipg_wait(pi,sio);
	if (sio->mailbox != ACT_DONE) {
		pi->pi_if.if_flags &= ~IFF_ALIVE;
		if_down(&pi->pi_if);
		return;
	}

	if (0 != (sio->cmd.bd_type & IPG_BD_TYPE_BYPASS)) {
		pi->pi_smt1.smt_st.flags |= PCM_HAVE_BYPASS;
		pi->pi_smt2.smt_st.flags |= PCM_HAVE_BYPASS;
	} else {
		pi->pi_smt1.smt_st.flags &= ~PCM_HAVE_BYPASS;
		pi->pi_smt2.smt_st.flags &= ~PCM_HAVE_BYPASS;
	}

	if (pi->pi_smt1.smt_conf.pcm_tgt == PCM_DEFAULT) {
		/* Dual-attachment without CMT is nonsense.
		 *	There is no telling in which order a pair of boards
		 *	will be probed or turned on.
		 */
		dasdm = (0 != (sio->cmd.bd_type & IPG_BD_TYPE_MAC2));
		if (unit >= ipg_max_board || ipg_cmt[unit]) {
			if (!dasdm
			    || (ipg_cmt[unit^1] && 0 != pi2->pi_sio)) {
				pi->pi_smt1.smt_conf.pcm_tgt = PCM_CMT;
				pi->pi_smt2.smt_conf.pcm_tgt = PCM_CMT;
			} else {
				dasdm = 0;
			}
		} else {
			dasdm = 0;
			pi->pi_smt1.smt_conf.pcm_tgt = PCM_NONE;
			pi->pi_smt2.smt_conf.pcm_tgt = PCM_NONE;
		}
		if (dasdm) {
			pi->pi_smt1.smt_conf.type = DM_DAS;
			if (PRIMARY_PI(pi)) {
				pi->pi_smt1.smt_conf.pc_type = PC_B;
			} else {
				pi->pi_smt1.smt_conf.pc_type = PC_A;
			}
		} else {
			pi->pi_smt1.smt_conf.type = SM_DAS;
			pi->pi_smt2.smt_conf.type = SM_DAS;
			pi->pi_smt1.smt_conf.pc_type = PC_B;
			pi->pi_smt2.smt_conf.pc_type = PC_A;
		}
	}


	/* Do not do multicast on the secondary ring to avoid confusing
	 *	single MAC destinations.
	 */
	if (IS_DASDM(pi) && !PRIMARY_PI(pi))
		pi->pi_if.if_flags &= ~IFF_MULTICAST;
	else
		pi->pi_if.if_flags |= IFF_MULTICAST;

	/* Zap the other board if it does not agree to be married or
	 *	divorced.  Start the other board if it was waiting for us.
	 *
	 * Do not assume the other board is correctly configured yet.
	 *	It may be neither single-MAC nor dual-MAC because it
	 *	has not yet been spoken to.
	 *
	 * While it is true that ipg_start() calls ipg_reset(), there
	 *	is no infinite recursion here.
	 */
	if (IS_DASDM(pi)) {
		if (!IS_DASDM(pi2)) {
			pi2->pi_if.if_flags &= ~IFF_RUNNING;
			ipg_start(pi2, pi2->pi_if.if_flags);
		} else if (iff_alive(pi->pi_if.if_flags)
			     && iff_dead(pi2->pi_if.if_flags)
			     && 0 != (pi2->pi_if.if_flags & IFF_UP)) {
			ipg_start(pi2, pi2->pi_if.if_flags);
		}
		smt_off(&pi2->pi_smt1);
	} else {
		smt_off(&pi->pi_smt2);
	}
	smt_off(&pi->pi_smt1);

	if (iff_dead(pi->pi_if.if_flags)) {
		if_down(&pi->pi_if);
	}
}



/* initialize interface if it has not been initialized before.
 */
static void
ipg_start(struct ipg_info *pi, int flags)
{
	int dflags;

	CK_IFNET(pi);

	if (0 != (flags & IFF_UP)) {
		flags |= IFF_ALIVE;
	} else {
		flags &= ~IFF_ALIVE;
	}

	if (pi->pi_if.in_ifaddr == 0)
		flags &= ~IFF_ALIVE;	/* dead if no address */
	if (IS_DASDM(OTHER_PI(pi)) != IS_DASDM(pi))
		flags &= ~IFF_RUNNING;	/* dual MAC must be mutual */

	dflags = flags ^ pi->pi_if.if_flags;
	pi->pi_if.if_flags = flags;	/* set here to allow overrides */
	if (0 != (dflags & IFF_ALIVE)) {
		pi->pi_state |= PI_ST_ARPWHO;
		ipg_reset(pi,1);
		if (iff_alive(pi->pi_if.if_flags)) {
			host_pcm(&pi->pi_smt1);	/* start joining the ring */
			if (IS_DASSM(pi))
				host_pcm(&pi->pi_smt2);
		}
	}

	if (0 != (dflags & IFF_PROMISC)) {
		ipg_set_mmode(pi,pi->pi_sio);
	}

	if (ipg_mtu > 128 && ipg_mtu < FDDIMTU)
		pi->pi_if.if_mtu = ipg_mtu;
	else
		pi->pi_if.if_mtu = FDDIMTU;
}


/* take a breath after drowning
 */
static void
ipg_breath(struct ipg_info *pi)
{
	IFNET_LOCK(&pi->pi_if);
	/* ASSERT(pi->pi_if.if_name == ipg_name); */

	if (pi->pi_state & PI_ST_DROWN) {
		pi->pi_state &= ~PI_ST_DROWN;
		IPG_METER(pi->pi_dbg.drown_off++);
	}

	ipg_b2h(pi, PI_ST_PSENABLED);
	IFNET_UNLOCK(&pi->pi_if);
}


/* start holding our breath */
static void
ipg_drown(struct ipg_info *pi)
{
#define DROWN_TICKS ((HZ*2)/(FRAME_SEC/IPG_MAX_SML))
#if DROWN_TICKS == 0
#undef DROWN_TICKS
#define DROWN_TICKS 1
#endif

	if (!(pi->pi_state & PI_ST_DROWN)) {
		pi->pi_state |= PI_ST_DROWN;

		(void)dtimeout(ipg_breath, pi, DROWN_TICKS, plbase, cpuid());
		IPG_METER(pi->pi_dbg.drown_on++);
	}
#undef DROWN_TICKS
}

/*
 * Process an ioctl request.
 * return 0 or error number
 */
static int
ipg_ioctl(struct ifnet *ifp, int cmd, void *data)
{
#define pi ((struct ipg_info*)ifp)
	IPG_SIO *sio = pi->pi_sio;
	struct ipg_info *pi2;
	int flags;
	int error = 0;

	/* ASSERT(pi->pi_if.if_name == ipg_name); */
	CK_IFNET(pi);

	switch (cmd) {
	case SIOCAIFADDR: {		/* Add interface alias address */
		struct sockaddr *addr = _IFADDR_SA(data);
#ifdef INET6
		/*
		 * arp is only for v4.  So we only call arpwhohas if there is
		 * an ipv4 addr.
		 */
		if (ifp->in_ifaddr != 0)
			arprequest(&pi->pi_ac, 
				&((struct sockaddr_in*)addr)->sin_addr,
				&((struct sockaddr_in*)addr)->sin_addr,
				(&pi->pi_ac)->ac_enaddr);
#else
		arprequest(&pi->pi_ac, 
			&((struct sockaddr_in*)addr)->sin_addr,
			&((struct sockaddr_in*)addr)->sin_addr,
			(&pi->pi_ac)->ac_enaddr);
#endif
		
		break;
	}

	case SIOCDIFADDR:		/* Delete interface alias address */
		/* currently nothing needs to be done */
		break;

	case SIOCSIFADDR: {
		struct sockaddr *addr = _IFADDR_SA(data);

		switch (addr->sa_family) {
		case AF_INET:
			pi->pi_ac.ac_ipaddr = ((struct sockaddr_in*)addr
					       )->sin_addr;
			pi->pi_if.if_flags &= ~IFF_RUNNING;
			ipg_start(pi, pi->pi_if.if_flags|IFF_UP);
			break;

		case AF_RAW:
			/* It is not safe to change the address while the
			 * board is alive.
			 */
			if (!iff_dead(pi->pi_if.if_flags)) {
				error = EINVAL;
			} else {
				pi->pi_state |= PI_ST_ADDRSET;
				ipg_set_addr(pi, addr->sa_data);
			}
			break;

		default:
			error = EINVAL;
			break;
		}
		} break;


	case SIOCSIFFLAGS:
		flags = ((struct ifreq *)data)->ifr_flags;

		if ((flags & IFF_UP) != 0) {

			if (pi->pi_if.in_ifaddr == 0)
				return EINVAL;	/* quit if no address */
			ipg_start(pi, flags);
		} else {
			flags &= ~IFF_RUNNING;
			pi->pi_if.if_flags = flags;
			ipg_reset(pi,iff_dead(flags));
			pi2 = OTHER_PI(pi);
			if (IS_DASDM(pi)
			    && iff_alive(pi2->pi_if.if_flags)) {
				pi2->pi_if.if_flags &= ~IFF_RUNNING;
				ipg_reset(pi2,iff_dead(flags));
			}
		}
		if (0 != (pi->pi_if.if_flags & IFF_DEBUG)) {
			pi->pi_smt1.smt_st.flags |= PCM_DEBUG;
			pi->pi_smt2.smt_st.flags |= PCM_DEBUG;
		} else {
			pi->pi_smt1.smt_st.flags &= ~PCM_DEBUG;
			pi->pi_smt2.smt_st.flags &= ~PCM_DEBUG;
		}
		if (0 != ((flags ^ pi->pi_if.if_flags) & IFF_ALIVE)) {
			error = ENETDOWN;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
#define MKEY ((union mkey*)data)
		int allmulti;
		error = ether_cvtmulti((struct sockaddr *)data, &allmulti);
		if (0 == error) {
			if (allmulti) {
				if (SIOCADDMULTI == cmd) {
					pi->pi_if.if_flags |= IFF_ALLMULTI;
				} else {
					pi->pi_if.if_flags &= ~IFF_ALLMULTI;
				}
				ipg_set_mmode(pi,sio);
			} else {
				bitswapcopy(MKEY->mk_dhost, MKEY->mk_dhost,
					    sizeof(MKEY->mk_dhost));
				if (SIOCADDMULTI == cmd) {
					error = ipg_add_da(pi, MKEY, 1);
				} else {
					error = ipg_del_da(pi, MKEY, 1);
				}
			}
		}
#undef MKEY
		} break;

	  case SIOCADDSNOOP:
	  case SIOCDELSNOOP: {
#define	SF(nm) ((struct lmac_hdr*)&(((struct snoopfilter *)data)->nm))
		u_char *a;
		union mkey key;

		a = &SF(sf_mask[0])->mac_da.b[0];
		if (!FDDI_ISBROAD_SW(a)) {
			/* cannot filter on board unless the mask is trivial.
			 */
			error = EINVAL;

		} else {
			/* Filter individual destination addresses.
			 *	Use a different address family to avoid
			 *	damaging an ordinary multi-cast filter.
			 */
			a = &SF(sf_match[0])->mac_da.b[0];
			key.mk_family = AF_RAW;
			bcopy(a, key.mk_dhost, sizeof(key.mk_dhost));
			if (cmd == SIOCADDSNOOP) {
				error = ipg_add_da(pi, &key,
						   FDDI_ISGROUP_SW(a));
			} else {
				error = ipg_del_da(pi, &key,
						   FDDI_ISGROUP_SW(a));
			}
		}
#undef SF
		} break;

	case SIOC_IPG_NVRAM_GET: {
#define		nvramp ((IPG_NVRAM *)data)

		pi->pi_state &= ~PI_ST_WHINE;	/* allow more complaints */
		ipg_wait(pi,sio);
		ipg_op(sio, ACT_FET_NVRAM);
		ipg_wait_usec(IPG_NVRAM_RD_DELAY+BD_DELAY,pi,sio,__LINE__);
		if (sio->mailbox != ACT_DONE)
			error = EIO;
		hwcpin(&sio->cmd.nvram,
		       nvramp, sizeof(*nvramp));
#undef		nvramp
		} break;

	case SIOC_IPG_NVRAM_SET: {
#define nvramp ((IPG_NVRAM *)data)

		if (!_CAP_ABLE(CAP_SYSINFO_MGT))
			return EPERM;
		pi->pi_state &= ~PI_ST_WHINE;	/* allow more complaints */
		ipg_wait(pi,sio);
		hwcpout(nvramp, &sio->cmd.nvram,
			sizeof(sio->cmd.nvram));
		ipg_op(sio, ACT_STO_NVRAM);
		ipg_wait_usec(IPG_NVRAM_WR_DELAY+BD_DELAY,pi,sio,__LINE__);
		if (sio->mailbox != ACT_DONE)
			error = EIO;
		pi->pi_state &= ~PI_ST_ADDRFET;
#undef		nvramp
		} break;

	default:
		error = smt_ioctl(&pi->pi_if, &pi->pi_smt1, &pi->pi_smt2,
				  cmd, (struct smt_sioc*)data);
		break;
	}

	return error;
#undef pi
}


/* Ensure there is room in front of a packet
 */
static struct mbuf*
ipg_mbuf_pad(struct ipg_info *pi,
	     struct mbuf *m0,
	     uint padlen,		/* prepend this much space */
	     uint minlen)		/* must have this much in 1st mbuf */
{
	struct mbuf *m1;
	caddr_t p;

	/* Extend first, since m_pullup() destroys any spare space
	 */
	if (padlen != 0) {
		if (!M_HASCL(m0)
		    && m0->m_off >= MMINOFF+padlen) {
			m0->m_len += padlen;
			m0->m_off -= padlen;
		} else {
			IPG_METER(pi->pi_dbg.out_alloc++);
			m1 = m_get(M_DONTWAIT, MT_DATA);
			if (!m1) {
				m_freem(m0);
				pi->pi_if.if_odrops++;
				IF_DROP(&pi->pi_if.if_snd);
				return 0;
			}
			m1->m_len = padlen;
			m1->m_next = m0;
			m0 = m1;
		}
	}

	p = mtod(m0, caddr_t);
	if (m0->m_len < minlen || M_HASCL(m0) || !ALIGNED(p)) {

		IPG_METER(pi->pi_dbg.out_pullup++);
		m0 = m_pullup(m0, minlen);
		if (!m0) {
			pi->pi_if.if_odrops++;
			IF_DROP(&pi->pi_if.if_snd);
			return 0;
		}
	}
	return m0;
}

/*
 * FDDI output.
 *	If destination is this address or broadcast, send packet to
 *	loop device since we cannot talk to ourself.
 */
ipg_output(struct ifnet *ifp,
	 struct mbuf *m0,
	 struct sockaddr *dst,
	 struct rtentry *rte)
{
#define pi ((struct ipg_info*)ifp)
	struct mbuf *m1, *m2;
	struct mbuf *mloop = 0;
	IPG_OBUF *fp;
	struct ipg_h2b *h2bp;
	int blen1, blen2, blen3, error, inx;
	int map_inx;
	void *dp;
	paddr_t vme_addr;

	ASSERT(pi->pi_if.if_name == ipg_name);
	CK_IFNET(pi);

	if (IF_QFULL(&pi->pi_if.if_snd)) {

		ipg_b2h(pi, 0);		/* try to empty the queue */

		if (IF_QFULL(&pi->pi_if.if_snd)) {
			m_freem(m0);
			pi->pi_if.if_odrops++;
			IF_DROP(&pi->pi_if.if_snd);
			return ((pi->pi_smt1.smt_st.mac_state == MAC_ACTIVE)
				? ENOBUFS : ENETDOWN);

		}
	}

	switch (dst->sa_family) {
	case AF_INET:
		/* get room for the FDDI headers; use what we were
		 *	given if possible.
		 */
		fp = mtod(m0, IPG_OBUF*);
		if (!M_HASCL(m0) && m0->m_off >= MMINOFF+sizeof(*fp) && ALIGNED(fp)) {
			ASSERT(m0->m_off <= MSIZE);
			m1 = 0;
			--fp;
		} else {
			IPG_METER(pi->pi_dbg.out_alloc++);
			m1 = m_get(M_DONTWAIT, MT_DATA);
			if (0 == m1) {
				m_freem(m0);
				pi->pi_if.if_odrops++;
				IF_DROP(&pi->pi_if.if_snd);
				return ENOBUFS;
			}
			fp = mtod(m1, IPG_OBUF*);
			m1->m_len = sizeof(*fp);
		}

#if _MIPS_SIM == _ABI64
		/* The 64-bit compilers mistakenly try to use longword
		 * operations here.
		 */
		bcopy(&pi->pi_hdr, fp, sizeof(*fp));
#else
		*fp = pi->pi_hdr;
#endif
		if (!arpresolve(&pi->pi_ac, rte, m0,
				&((struct sockaddr_in *)dst)->sin_addr,
				(u_char*)&fp->fddi.fddi_mac.mac_da, &error)) {
			m_freem(m1);

			ipg_b2h(pi, 0);		/* try to empty the queue */
			return error;	/* just wait if not yet resolved */
		}

		if (0 == m1) {
			m0->m_off -= sizeof(*fp);
			m0->m_len += sizeof(*fp);
		} else {
			m1->m_next = m0;
			m0 = m1;
		}
		bitswapcopy(&fp->fddi.fddi_mac.mac_da,
			    &fp->fddi.fddi_mac.mac_da,
			    sizeof(fp->fddi.fddi_mac.mac_da));

		/* Listen to ourself, if we are supposed to.
		 */
		if (FDDI_ISBROAD_SW(&fp->fddi.fddi_mac.mac_da.b[0])) {
			mloop = m_copy(m0, sizeof(*fp), M_COPYALL);
			if (0 == mloop) {
				m_freem(m0);
				pi->pi_if.if_odrops++;
				IF_DROP(&pi->pi_if.if_snd);
				return ENOBUFS;
			}
		}
		break;


	case AF_UNSPEC:
#define EP ((struct ether_header *)&dst->sa_data[0])
		m0 = ipg_mbuf_pad(pi, m0, sizeof(*fp),
				  sizeof(*fp)+sizeof(struct ether_arp));
		if (!m0)
			return ENOBUFS;
		fp = mtod(m0, IPG_OBUF*);

#if _MIPS_SIM == _ABI64
		/* The 64-bit compilers mistakenly try to use longword
		 * operations here.
		 */
		bcopy(&pi->pi_hdr, fp, sizeof(*fp));
#else
		*fp = pi->pi_hdr;
#endif
		fp->fddi.fddi_llc.llc_etype = EP->ether_type;
		bitswapcopy(&EP->ether_dhost[0],
			    &fp->fddi.fddi_mac.mac_da,
			    sizeof(fp->fddi.fddi_mac.mac_da));
# undef EP
		break;

	case AF_RAW:
		/* The mbuf chain contains most of the frame, including the
		 *	MAC header but not the board header.
		 */
		m0 = ipg_mbuf_pad(pi, m0, sizeof(*fp)-sizeof(fp->fddi),
				  sizeof(*fp));
		if (!m0)
			return ENOBUFS;
		fp = mtod(m0, IPG_OBUF*);
		fp->desc.dlen = OBF_PAD_LEN;
		fp->fddi.fddi_mac.mac_bits = 0;	/* clear frame status */
		break;

	case AF_SDL:
		/* send an 802 packet for DLPI
		 */
#define SCKTP ((struct sockaddr_sdl *)dst)
#define SCKTP_HLEN (sizeof(*fp) - sizeof(fp->fddi.fddi_llc))

		/* choke if the MAC address has a silly length */
		if (SCKTP->ssdl_addr_len != sizeof(fp->fddi.fddi_mac.mac_da)) {
			m_freem(m0);
			return EAFNOSUPPORT;
		}
		/* The mbuf chain contains most of the frame, including the
		 *	LLC header but not the board or MAC headers.
		 */
		m0 = ipg_mbuf_pad(pi, m0, SCKTP_HLEN, SCKTP_HLEN);
		if (!m0)
			return ENOBUFS;
		fp = mtod(m0, IPG_OBUF*);
		bcopy(&pi->pi_hdr, fp, SCKTP_HLEN);
		bitswapcopy(SCKTP->ssdl_addr, &fp->fddi.fddi_mac.mac_da,
			    sizeof(fp->fddi.fddi_mac.mac_da));
#undef SCKTP
#undef SCKTP_HLEN
		break;

	default:
		printf("ipg%d: cannot handle address family 0x%x\n",
		       pi->pi_if.if_unit, dst->sa_family);
		m_freem(m0);
		return EAFNOSUPPORT;
	}
	/*
	 * each of the preceding cases leave fp pointing at the FDDI header
	 * Start DMA on the message, unless the board is dead.
	 */
	if (iff_dead(pi->pi_if.if_flags)
	    || 0 != (pi->pi_state & PI_ST_ZAPPED)) {
		pi->pi_if.if_oerrors++;
		IF_DROP(&pi->pi_if.if_snd);
		m_freem(mloop);
		m_freem(m0);
		return EHOSTDOWN;
	}


	/* If the chain of mbufs is too messy, copy it to a couple of clusters
	 *	and start over.
	 */
restart:;

	/* be certain there is enough room for the request
	 */
	h2bp = pi->pi_h2bp;
	if (h2bp >= pi->pi_h2b_lim) {
		pi->pi_h2b_buf->h2b_op = IPG_H2B_EMPTY;
		h2bp->h2b_op = IPG_H2B_WRAP;
		h2bp = pi->pi_h2b_buf;
		pi->pi_h2bp = h2bp;
	}

	blen1 = 0;
	m2 = m0;
	inx = 0;

	map_inx = pi->pi_out_map_inx;

	do {
		if (0 == m2->m_len)
			continue;
		dp = mtod(m2, void*);

		if (!ALIGNED(dp)
		    || !ALIGNED(blen1)
		    || inx >= IPG_MAX_DMA_ELEM-1
		    || (IS_KSEG2(dp) && inx >= IPG_MAX_DMA_ELEM-2)) {

			  /*
			   * We must copy the chain.
			   * Reserve an extra DMA slot so we do not have to
			   * worry about running out when handling a last mbuf
			   * in kernel virtual space which crosses a
			   * page boundary.
			   */
			IPG_METER(pi->pi_dbg.out_copy++);
			blen1 = m_length(m0);

			if (blen1 <= VCL_MAX) {
				m2 = 0;
			} else {
				blen2 = blen1 - VCL_MAX;
				blen1 = VCL_MAX;
				m2 = m_vget(M_DONTWAIT, blen2, MT_DATA);
				if (0 == m2) {
					pi->pi_if.if_odrops++;
					IF_DROP(&pi->pi_if.if_snd);
					m_freem(mloop);
					m_freem(m0);
					return ENOBUFS;
				}

				(void)m_datacopy(m0,VCL_MAX,blen2,
						 mtod(m2, caddr_t));
			}
			m1 = m_vget(M_DONTWAIT,blen1,MT_DATA);
			if (0 == m1) {
				pi->pi_if.if_odrops++;
				IF_DROP(&pi->pi_if.if_snd);
				m_freem(m2);
				m_freem(mloop);
				m_freem(m0);
				return ENOBUFS;
			}
			(void)m_datacopy(m0, 0,blen1, mtod(m1,caddr_t));
			m1->m_next = m2;
			m_freem(m0);
			m0 = m1;
			fp = mtod(m0, IPG_OBUF*);

			IPG_METER(pi->pi_dbg.outmap_inx_restart++);
			goto restart;
		}

		blen3 = m2->m_len;

		/*
		 * The ipg_dmamap uses enough map entries so that we
		 * do not need to worry about page crossings.
		 */
		vme_addr = ipg_dmamap(dp,
				   blen3,
				   pi->pi_out_map,
				   pi->pi_iamap,
				   &map_inx,
				   OUT_MAP_LEN
#ifdef IPG_DEBUG_EXTRA
				   , pi
#endif /* IPG_DEBUG_EXTRA */
				   );
	
		h2bp->h2b_addr = vme_addr;
		h2bp->h2b_len = blen3;
		++h2bp;
		++inx;
		blen1 += blen3;
	} while (0 != (m2 = m2->m_next));

	ASSERT(blen1 >= FDDI_MINOUT+sizeof(fp->desc));
	ASSERT(blen1 <= FDDI_MAXOUT+sizeof(fp->desc));

	/* finish DRAM-to-fiber DMA header */
	fp->desc.len = sizeof(fp->desc)+FDDI_FILLER-blen1;

#ifdef EVEREST
	/*
	 * Adjust/allocate an extra map entry to foil the read-ahead by
	 * the hardware.
	 * NOTE: We also update the pi structure with the new rounded value
	 * after the restart case has been handled.
	 */
	pi->pi_out_map_inx = RND_MAP_INX(map_inx);
#endif /* EVEREST */

	/*
	 * Give the DMA elements to the board, with the first element
	 * last so the board does not start before we are finished
	 */
	h2bp->h2b_op = IPG_H2B_EMPTY;
	pi->pi_h2bp = h2bp;
	(--h2bp)->h2b_op = IPG_H2B_OUT_FIN;
	while (--inx > 0)
		(--h2bp)->h2b_op = IPG_H2B_OUT;

	pi->pi_if.if_obytes += blen1;
	pi->pi_if.if_opackets++;

	IF_ENQUEUE_NOLOCK(&pi->pi_if.if_snd, m0);

	/* Check whether snoopers want to copy this packet.
	 *	Do it now, before we might call ipg_b2h and free
	 *	the chain after the board has been very fast and
	 *	already transmitted the frame.
	 */
	if (RAWIF_SNOOPING(&pi->pi_rawif)
	    && snoop_match(&pi->pi_rawif, mtod(m0,caddr_t), m0->m_len)) {
		struct mbuf *ms, *mt;
		int len;		/* m0 bytes to copy */
		int lenoff;
		int curlen;

		len = m_length(m0)-sizeof(fp->desc);
		lenoff = sizeof(fp->desc);
		curlen = len+sizeof(FDDI_IBUF);
		if (curlen > VCL_MAX)
			curlen = VCL_MAX;
		ms = m_vget(M_DONTWAIT,
			    MAX(curlen,sizeof(FDDI_IBUFLLC)), MT_DATA);
		if (0 != ms) {
			IF_INITHEADER(mtod(ms,caddr_t),
				      &pi->pi_if,
				      sizeof(FDDI_IBUFLLC));
			curlen = m_datacopy(m0, lenoff,
					    curlen-sizeof(FDDI_IBUF),
					    mtod(ms,caddr_t)
					    + sizeof(FDDI_IBUF));
			mt = ms;
			for (;;) {
				lenoff += curlen;
				len -= curlen;
				if (len <= 0)
					break;
				curlen = MIN(len,VCL_MAX);
				m1 = m_vget(M_DONTWAIT,curlen,MT_DATA);
				if (0 == m1) {
					m_freem(ms);
					ms = 0;
					break;
				}
				mt->m_next = m1;
				mt = m1;
				curlen = m_datacopy(m0, lenoff, curlen,
						    mtod(m1, caddr_t));
			}
		}
		if (!ms) {
			snoop_drop(&pi->pi_rawif, SN_PROMISC,
				   mtod(m0,caddr_t), m0->m_len);
		} else {
			(void)snoop_input(&pi->pi_rawif, SN_PROMISC,
					  mtod(m0, caddr_t),
					  ms,
					  (lenoff > sizeof(*fp)
					  ? lenoff-sizeof(*fp) : 0));
		}
	}

	/* Ensure that the board knows about the frame.
	 */
	ipg_b2h(pi, 0);

	if ((pi->pi_state & PI_ST_SLEEP) || ipg_low_latency) {

		pi->pi_state &= ~(PI_ST_SLEEP | PI_ST_NOPOKE);
		IPG_METER(pi->pi_dbg.board_poke++);
		ipg_wait(pi,pi->pi_sio);
		ipg_op(pi->pi_sio,ACT_WAKEUP);
	} else {
		pi->pi_state |= PI_ST_NOPOKE;
	}

	/* finally send to ourself */
	if (mloop != 0) {
		pi->pi_if.if_omcasts++;
		(void)looutput(&loif, mloop, dst, rte);
	} else if (FDDI_ISGROUP_SW(fp->fddi.fddi_mac.mac_da.b)) {
		pi->pi_if.if_omcasts++;
	}

	return 0;
#undef pi
}

/*
 * send a DMA command to the board
 * NOTE:
 * Interrupts MUST be off AND
 * IFNET lock protecting the ipg_info structure must be held.
 */
static void
ipg_send_h2b(struct ipg_info *pi, ushort op, ushort len, __uint32_t addr)
{
	struct ipg_h2b *h2bp, *n_h2bp;
        union {
		struct ipg_h2b ts;
		uint64_t td;
	} tmp;

	h2bp = pi->pi_h2bp;
	/*
	 * install things in the right order, to resolve race with board
	 */
	n_h2bp = h2bp+1;
	if (n_h2bp >= pi->pi_h2b_lim) {
		pi->pi_h2bp = pi->pi_h2b_buf;
		pi->pi_h2b_buf->h2b_op = IPG_H2B_EMPTY;
		n_h2bp->h2b_op = IPG_H2B_WRAP;
	} else {
		n_h2bp->h2b_op = IPG_H2B_EMPTY;
		pi->pi_h2bp = n_h2bp;
	}
	/*
	 * Store entire 64-bit message as one operation to prevent
	 * cache interventions with the VME controller for the message.
	 * This is problem is particularly egreious with the R10K since
	 * the uncached writes are not handled properly, so doing this
	 * operation entirely uncached isn't possible. So do a cached write
	 * operation below to prevent a partial message from being read by
	 * the off-board VME FDDI controller.
	 */
        tmp.ts.h2b_addr = addr;
        tmp.ts.h2b_len = len;
        tmp.ts.h2b_op = op;
        *((uint64_t *) h2bp) = tmp.td;
}

/*
 * fill up the board stocks of empty buffers
 * MUST be called with the interrupts must be off.
 */
static int				/* 1=did some filling */
ipg_fillin(struct ipg_info *pi)
{
	struct mbuf *m;
	caddr_t p;
	int i;
	int worked = 0;

	CK_IFNET(pi);

	/* Do not bother reserving buffers if not turned on.
	 * Take it easy while drowning in input frames.
	 */
	if (iff_dead(pi->pi_if.if_flags)
	    || !(pi->pi_state & PI_ST_MAC)
	    || (pi->pi_state & PI_ST_DROWN)) {
		return worked;
	}

#ifdef IPG_DEBUG_EXTRA
	if (pi->pi_in_sml_num <= 1)
		pi->pi_dbg.no_sml++;
	if (pi->pi_in_med_num == 0)
		pi->pi_dbg.no_med++;
	if (pi->pi_in_big_num <= 1)
		pi->pi_dbg.no_big++;
#endif
	/* delay interrupts while we handle input
	 */
	if ((pi->pi_byte_busy != 0) && !ipg_low_latency) {

		i = BTODELAY(pi->pi_byte_busy);
		if (i > 0)
			ipg_send_h2b(pi, IPG_H2B_IDELAY, 0, i);
		pi->pi_byte_busy = 0;
	}

	while (pi->pi_in_sml_num < ipg_num_sml) {
#ifdef IPG_DEBUG_EXTRA
		pi->pi_in_smls[pi->pi_in_sml_t] = m =
			m_get(M_DONTWAIT, MT_DATA);
		if (m == NULL)
			goto exit;
#else
		m = m_get(M_DONTWAIT,MT_DATA);
		if (!m)
			goto exit;
		pi->pi_in_smls[pi->pi_in_sml_t] = m;
#endif /* IPG_DEBUG_EXTRA */
		/*
		 * A subsequent mtod() would put the mbuf header into the
		 * cache which would undo the cache invalidation.
		 */
		p = mtod(m,caddr_t);
#ifdef EVEREST
		ipg_send_h2b(pi,
			     IPG_H2B_SMLBUF,
			     0,
			     ipg_dmamap(p,
				IPG_SML_SIZE,
				pi->pi_sml_map,
				pi->pi_iamap,
				&pi->pi_sml_map_inx,
				SML_MAP_LEN
#ifdef IPG_DEBUG_EXTRA
				, pi
#endif
				));
		pi->pi_sml_map_inx = RND_MAP_INX(pi->pi_sml_map_inx);
#else
		dki_dcache_inval(p, IPG_SML_SIZE);
		ipg_send_h2b(pi, IPG_H2B_SMLBUF,0,kvtophys(p));
#endif /* EVEREST */
		pi->pi_in_sml_t = (pi->pi_in_sml_t+1) % IPG_MAX_SML;
		++pi->pi_in_sml_num;
		worked = 1;
	}

	while (pi->pi_in_med_num < ipg_num_med) {
#ifdef IPG_DEBUG_EXTRA
		pi->pi_in_meds[pi->pi_in_med_t] = m =
			m_vget(M_DONTWAIT, IPG_MED_SIZE, MT_DATA);
		if (m == NULL)
			break;
#else
		m = m_vget(M_DONTWAIT, IPG_MED_SIZE, MT_DATA);
		if (!m)
			break;
		pi->pi_in_meds[pi->pi_in_med_t] = m;
#endif /* IPG_DEBUG_EXTRA */

		p = mtod(m,caddr_t);

#ifdef EVEREST
		ipg_send_h2b(pi,
			     IPG_H2B_MEDBUF,
			     0,
			     ipg_dmamap(p,
				IPG_MED_SIZE,
				pi->pi_med_map,
				pi->pi_iamap,
				&pi->pi_med_map_inx,
				MED_MAP_LEN
#ifdef IPG_DEBUG_EXTRA
				, pi
#endif
				));

		pi->pi_med_map_inx = RND_MAP_INX(pi->pi_med_map_inx);
#else
		dki_dcache_inval(p, IPG_MED_SIZE);
		ipg_send_h2b(pi, IPG_H2B_MEDBUF,0,kvtophys(p));
#endif /* EVEREST */
		pi->pi_in_med_t = (pi->pi_in_med_t+1) % IPG_MAX_MED;
		++pi->pi_in_med_num;
		worked = 1;
	}

	while (pi->pi_in_big_num < ipg_num_big) {
#ifdef IPG_DEBUG_EXTRA
		pi->pi_in_bigs[pi->pi_in_big_t] = m =
			m_vget(M_DONTWAIT, IPG_BIG_SIZE, MT_DATA);
		if (m == NULL)
			break;
#else
		m = m_vget(M_DONTWAIT, IPG_BIG_SIZE, MT_DATA);
		if (!m)
			break;
		pi->pi_in_bigs[pi->pi_in_big_t] = m;
#endif /* IPG_DEBUG_EXTRA */

		p = mtod(m,caddr_t);
#ifdef EVEREST
		ipg_send_h2b(pi,
			     IPG_H2B_BIGBUF,
			     0,
			     ipg_dmamap(p,
					IPG_BIG_SIZE,
					pi->pi_big_map,
					pi->pi_iamap,
					&pi->pi_big_map_inx,
					BIG_MAP_LEN
#ifdef IPG_DEBUG_EXTRA
					, pi
#endif
					));
		pi->pi_big_map_inx = RND_MAP_INX(pi->pi_big_map_inx);
#else
		dki_dcache_inval(p, IPG_BIG_SIZE);
		ipg_send_h2b(pi, IPG_H2B_BIGBUF,0,kvtophys(p));
#endif /* EVEREST */
		pi->pi_in_big_t = (pi->pi_in_big_t+1) % IPG_MAX_BIG;
		++pi->pi_in_big_num;
		worked = 1;
	}

exit:;
	return worked;
}



/* process SMT and CMT interrupts
 */
static void
ipg_phy_intr(struct ipg_info *pi, u_char ls1, u_char ls2)
{
	struct smt *smt, *smtx, *smty;


	(void)ipg_st_get(0,pi);		/* see what it is about */

	smt = &pi->pi_smt1;
	smtx = 0;
	if (ls2 != PCM_ULS) {
		smt->smt_st.rls = (enum pcm_ls)ls2;
		if (IS_DASSM(pi)
		    && iff_alive(pi->pi_if.if_flags)) {
			if (ls2 == PCM_NLS) {
				smt->smt_st.flags |= (PCM_NE
						      | PCM_LEM_FAIL);
				SMTDEBUGPRINT(smt,0,
					      ("%sCMT overrun\n",
					       PC_LAB(smt)));
			} else {
				SMTDEBUGPRINT(smt,4,
					      ("%sR=%s ",PC_LAB(smt),
					       str_ls[ls2]));
			}
			SMT_LSLOG(smt,ls2);
			smtx = smt;
		}
	}

	if (pi->pi_old_noise2 != smt->smt_st.noise.lo) {
		pi->pi_old_noise2 = smt->smt_st.noise.lo;
		if (iff_alive(pi->pi_if.if_flags)) {
			smt->smt_st.flags |= (PCM_NE | PCM_LEM_FAIL);
			if (IS_DASSM(pi)) {
				SMTDEBUGPRINT(smt,1,
					      ("%sNoise Event\n",
					       PC_LAB(smt)));
				smtx = smt;
			}
		}
	}


	if (IS_DASDM(pi)) {
		smt = &OTHER_PI(pi)->pi_smt1;
	} else {
		smt = &pi->pi_smt2;
	}
	smty = 0;

	if (ls1 != PCM_ULS) {
		smt->smt_st.rls = (enum pcm_ls)ls1;
		if (iff_alive(pi->pi_if.if_flags)) {
			if (ls1 == PCM_NLS) {
				smt->smt_st.flags |= (PCM_NE | PCM_LEM_FAIL);
				SMTDEBUGPRINT(smt,0,
					      ("%sCMT overrun\n",
					       PC_LAB(smt)));
			} else {
				SMTDEBUGPRINT(smt,4,
					      ("%sR=%s ",PC_LAB(smt),
					       str_ls[ls1]));
			}
			SMT_LSLOG(smt,ls1);
			smty = smt;
		}
	}

	if (pi->pi_old_noise1 != smt->smt_st.noise.lo) {
		pi->pi_old_noise1 = smt->smt_st.noise.lo;
		if (iff_alive(pi->pi_if.if_flags)) {
			smt->smt_st.flags |= (PCM_NE | PCM_LEM_FAIL);
			SMTDEBUGPRINT(smt,1, ("%sNoise Event\n",
					      PC_LAB(smt)));
			smty = smt;
		}
	}


	ipg_send_h2b(pi,IPG_H2B_PHY_ACK,0,0);   /* re-arm PHY interrupts */

	if (0 != smtx
	    && PCM_ST_BYPASS != smtx->smt_st.pcm_state
	    && PCM_ST_OFF != smtx->smt_st.pcm_state
	    && PCM_CMT == smtx->smt_conf.pcm_tgt)
		host_pcm(smtx);

	if (0 != smty
	    && PCM_ST_BYPASS != smty->smt_st.pcm_state
	    && PCM_ST_OFF != smty->smt_st.pcm_state
	    && PCM_CMT == smty->smt_conf.pcm_tgt)
		host_pcm(smty);
}


/* See if a DLPI function wants a frame.
 */
static int
ipg_dlp(struct ipg_info *pi, int port, int encap, struct mbuf *m, int len)
{
	dlsap_family_t *dlp;
	struct mbuf *m2;
	FDDI_IBUFLLC *fhp;

	dlp = dlsap_find(port, encap);
	if (!dlp)
		return 0;

	/* The DLPI code wants to get the entire MAC and LLC headers.
	 * It needs the total length of the mbuf chain to reflect
	 * the actual data length, not to be extended to contain
	 * a fake, zeroed LLC header which keeps the snoop code from
	 * crashing.
	 */
	m2 = m_copy(m, 0, len+sizeof(FDDI_IBUF));
	if (!m2)
		return 0;
	if (M_HASCL(m2)) {
		m2 = m_pullup(m2, sizeof(*fhp));
		if (!m2)
			return 0;
	}
	fhp = mtod(m2, FDDI_IBUFLLC*);

	/* The DLPI code wants the MAC addresses in cannonical bit order.
	 */
	bitswapcopy(&fhp->fbh_fddi.fddi_mac.mac_da,
		    &fhp->fbh_fddi.fddi_mac.mac_da,
		    sizeof(fhp->fbh_fddi.fddi_mac.mac_da)
		    +sizeof(fhp->fbh_fddi.fddi_mac.mac_sa));

	/* The DLPI code wants the LLC header to not be hidden with the
	 * MAC header.
	 */
	fhp->fbh_ifh.ifh_hdrlen -= sizeof(struct llc);

	if ((*dlp->dl_infunc)(dlp, &pi->pi_if, m2,
			      &fhp->fbh_fddi.fddi_mac)) {
		m_freem(m);
		return 1;
	}
	m_freem(m2);
	return 0;
}


/* deal with a complete input frame in a string of mbufs
 */
static void
ipg_input(struct ipg_info *pi, int op, int totlen)
{
	struct mbuf *m;
	FDDI_IBUFLLC *fhp;
	int snoopflags = 0;
	u_long cksum;
	uint port;

	if (totlen <= IPG_SML_SIZE) {

		BAD_BD(pi->pi_in_sml_num > 0, return);
		m = pi->pi_in_smls[pi->pi_in_sml_h];

#ifdef IPG_DEBUG_EXTRA
		pi->pi_in_smls[pi->pi_in_sml_h] = NULL;
		if (m == NULL) {
			printf("ipg%d: ipg_input pi_in_smls NULL; ix %d\n",
			       pi->pi_if.if_unit, pi->pi_in_sml_h);
			zapbd(pi, pi->pi_sio);
			return;
		}
		if (m->m_type == MT_FREE) { /* bogus */
	cmn_err_tag(233,CE_PANIC, "ipg%d: ifnet 0x%x, MT_FREE small mbuf 0x%x\n",
			pi->pi_if.if_unit, &(pi->pi_ac.ac_if), m);
		}
#endif
		pi->pi_in_sml_h = (pi->pi_in_sml_h+1) % IPG_MAX_SML;
		pi->pi_in_sml_num--;
		m->m_next = 0;
		m->m_len = totlen;
	} else if (totlen <= IPG_MED_SIZE) {

		BAD_BD(pi->pi_in_med_num > 0, return);
		m = pi->pi_in_meds[pi->pi_in_med_h];
#ifdef IPG_DEBUG_EXTRA
		pi->pi_in_meds[pi->pi_in_med_h] = NULL;
		if (m == NULL) {
			printf("ipg%d: ipg_input pi_in_meds NULL; ix %d\n",
			       pi->pi_if.if_unit, pi->pi_in_med_h);
			zapbd(pi, pi->pi_sio);
			return;
		}
		if (m->m_type == MT_FREE) { /* bogus */
		cmn_err_tag(234,CE_PANIC, "ipg%d: ifnet 0x%x, MT_FREE med mbuf 0x%x\n",
			pi->pi_if.if_unit, &(pi->pi_ac.ac_if), m);
		}
#endif
		pi->pi_in_med_h = (pi->pi_in_med_h+1) % IPG_MAX_MED;
		pi->pi_in_med_num--;
		m->m_next = 0;
		m_adj(m, totlen - IPG_MED_SIZE);
	} else {
		BAD_BD(pi->pi_in_big_num > 0, return);
		BAD_BD(totlen <= IPG_BIG_SIZE, return);

		m = pi->pi_in_bigs[pi->pi_in_big_h];
#ifdef IPG_DEBUG_EXTRA
		pi->pi_in_bigs[pi->pi_in_big_h] = NULL;
		if (m == NULL) {
			printf("ipg%d: ipg_input pi_in_bigs NULL; ix %d\n",
			       pi->pi_if.if_unit, pi->pi_in_big_h);
			zapbd(pi, pi->pi_sio);
			return;
		}
		if (m->m_type == MT_FREE) { /* bogus */
		cmn_err_tag(235,CE_PANIC, "ipg%d: ifnet 0x%x, MT_FREE big mbuf 0x%x\n",
			pi->pi_if.if_unit, &(pi->pi_ac.ac_if), m);
		}
#endif
		pi->pi_in_big_h = (pi->pi_in_big_h+1) % IPG_MAX_BIG;
		pi->pi_in_big_num--;

		if (totlen != IPG_BIG_SIZE)
			IPG_METER(pi->pi_dbg.waste_page++);
		m->m_next = 0;
		m->m_len = totlen;
	}

	/* If not finished, wait for more.
	 */
	if (op != IPG_B2H_IN_DONE) {
		pi->pi_in = m;
		pi->pi_in_len = totlen;
		return;
	}
	if (pi->pi_in != 0) {
		totlen += pi->pi_in_len;
		pi->pi_in->m_next = m;
		m = pi->pi_in;
		pi->pi_in = 0;
	}

	totlen -= sizeof(FDDI_IBUF);

	/* keep the mbuf large enough to keep the snoop code from
	 * calling panic or going crazy.
	 */
	if (totlen < FDDI_MIN_LLC) {
		bzero(mtod(m,char*)+totlen+sizeof(FDDI_IBUF),
		      FDDI_MIN_LLC - totlen);
		m->m_len = FDDI_MIN_LLC + sizeof(FDDI_IBUF);
	}

	fhp = mtod(m, FDDI_IBUFLLC*);
	IF_INITHEADER(fhp, &pi->pi_if, ISIZEOF(FDDI_IBUFLLC));

	/* Predict activity based on received data.
	 */
	pi->pi_byte_busy += totlen;

	pi->pi_if.if_ibytes += totlen;
	pi->pi_if.if_ipackets++;

	if (0 != (fhp->fbh_fddi.fddi_mac.mac_bits
		  & (MAC_ERROR_BIT | MAC_E_BIT))) {
		pi->pi_if.if_ierrors++;
		snoopflags = (SN_ERROR|SNERR_FRAME);

	} else if (totlen < sizeof(struct fddi)) {
		if (totlen < sizeof(struct lmac_hdr)) {
			pi->pi_if.if_ierrors++;
			snoopflags = (SN_ERROR|SNERR_TOOSMALL);
		}
		port = FDDI_PORT_LLC;

	} else {
#define FSA fddi_mac.mac_sa.b
#define FDA fddi_mac.mac_da.b
#define OUR_SA() (fhp->fbh_fddi.FSA[sizeof(LFDDI_ADDR)-1] \
				 == pi->pi_hdr.fddi.FSA[sizeof(LFDDI_ADDR)-1]\
				 && !bcmp(fhp->fbh_fddi.FSA,		 \
					  pi->pi_hdr.fddi.FSA,		 \
					  sizeof(LFDDI_ADDR)))

#define OTR_DA() (fhp->fbh_fddi.FDA[sizeof(LFDDI_ADDR)-1] \
				 != pi->pi_hdr.fddi.FSA[sizeof(LFDDI_ADDR)-1]\
				 || bcmp(fhp->fbh_fddi.FDA,		 \
					 pi->pi_hdr.fddi.FSA,		 \
					 sizeof(LFDDI_ADDR)))

		/* notice if it is a broadcast or multicast frame.
		 *	If it is, and we sent it, then drop it since we are
		 *	probably seeing it only because the ring latency is
		 *	high.
		 *
		 * XXX this might indicate a duplicate address.
		 */
		if (FDDI_ISGROUP_SW(fhp->fbh_fddi.FDA)) {
			if (OUR_SA()) {
				/* XXX this could mean a duplicate address */
				IPG_METER(pi->pi_dbg.own_out++);
				m_freem(m);
				return;
			}
			/* get rid of imperfectly filtered multicasts
			 */
			if (FDDI_ISBROAD_SW(fhp->fbh_fddi.FDA)) {
				m->m_flags |= M_BCAST;
			} else {
				if (0 == (pi->pi_ac.ac_if.if_flags
					  & IFF_ALLMULTI)
				    && !mfethermatch(&pi->pi_filter,
						     fhp->fbh_fddi.FDA, 0)) {
					if (RAWIF_SNOOPING(&pi->pi_rawif)
					    && snoop_match(&pi->pi_rawif,
						     (caddr_t)&fhp->fbh_fddi,
							   totlen)) {
						snoopflags = SN_PROMISC;
					} else {
						m_freem(m);
						return;
					}
				}
				m->m_flags |= M_MCAST;
			}
			pi->pi_if.if_imcasts++;

		} else if ((pi->pi_mac_mode & FORMAC_MADET_PROM)
			   && OTR_DA()) {
			/* Check for a packet from ourself.
			 */
			if (OUR_SA()) {
				IPG_METER(pi->pi_dbg.own_out++);
				m_freem(m);
				return;
			}

			if (RAWIF_SNOOPING(&pi->pi_rawif)
			    && snoop_match(&pi->pi_rawif,
					   (caddr_t)&fhp->fbh_fddi,
					   totlen)) {
				snoopflags = SN_PROMISC;
			} else {
				m_freem(m);
				return;
			}
		}


		if (MAC_FC_IS_LLC(fhp->fbh_fddi.fddi_mac.mac_fc)) {
			if (totlen >= FDDI_MIN_LLC
			    && fhp->fbh_fddi.fddi_llc.llc_c1 == RFC1042_C1
			    && fhp->fbh_fddi.fddi_llc.llc_c2 == RFC1042_C2) {
				port= ntohs(fhp->fbh_fddi.fddi_llc.llc_etype);
			} else {
				port = FDDI_PORT_LLC;
			}
		} else if (MAC_FC_IS_SMT(fhp->fbh_fddi.fddi_mac.mac_fc)) {
			port = FDDI_PORT_SMT;
		} else if (MAC_FC_IS_MAC(fhp->fbh_fddi.fddi_mac.mac_fc)) {
			port = FDDI_PORT_MAC;
		} else if (MAC_FC_IS_IMP(fhp->fbh_fddi.fddi_mac.mac_fc)) {
			port = FDDI_PORT_IMP;
		} else {
			snoopflags = SN_PROMISC;
		}
#undef FSA
#undef FDA
#undef OUR_SA
#undef OTR_DA
	}

	/* do raw snooping
	 */
	if (RAWIF_SNOOPING(&pi->pi_rawif)) {
		if (!snoop_input(&pi->pi_rawif, snoopflags,
				 (caddr_t)&fhp->fbh_fddi,
				 m,
				 (totlen>sizeof(struct fddi)
				  ? totlen-sizeof(struct fddi) : 0))) {
			ipg_drown(pi);
		}
		if (0 != snoopflags)
			return;

	} else if (0 != snoopflags) {
		/* account for input errors which snoopers might care about.
		 */
		m_freem(m);
		if (RAWIF_SNOOPING(&pi->pi_rawif))
			snoop_drop(&pi->pi_rawif, snoopflags,
				   (caddr_t)&fhp->fbh_fddi, totlen);
		if (RAWIF_DRAINING(&pi->pi_rawif))
			drain_drop(&pi->pi_rawif, port);
		return;
	}

	/* If it is a frame we understand, then give it to the
	 *	correct protocol code.
	 * It must be big enough to have RFC-1103 SNAP, have a simple LLC
	 *	frame code, and have the magic SNAP.
	 */
	switch (port) {
	case ETHERTYPE_IP:
		/* Finish TCP or UDP checksum on non-fragments.
		 * This knows at most 2 mbufs are allocated.  It knows
		 * the board offers checksums only if the frame was big.
		 */
		cksum = *(ushort*)&fhp->fbh_fddi.fddi_mac.filler[0];
		if (cksum != 0) {
		    struct ip *ip;
		    int hlen, cklen;

		    IPG_METER(pi->pi_dbg.bd_cksums++);

		    ip = (struct ip*)(fhp+1);
		    hlen = ip->ip_hl<<2;
		    cklen = totlen - (hlen+sizeof(struct fddi)
				      +IPG_IN_CKSUM_LEN);

		    /* Do not bother if it is an IP fragment or if
		     * not all of the uncheck data is in the 1st mbuf.
		     */
		    if (0 != (ip->ip_off & ~IP_DF)
			|| cklen + sizeof(FDDI_IBUFLLC) > m->m_len) {
			IPG_METER(pi->pi_dbg.bd_cksum_frag++);
		    } else if (ip->ip_p == IPPROTO_TCP
			       || ip->ip_p == IPPROTO_UDP) {

			/* compute checksum of the psuedo-header
			 */
			cksum += (ip->ip_len-hlen
				  + htons((ushort)ip->ip_p)
				  + (ip->ip_src.s_addr >> 16)
				  + (ip->ip_src.s_addr & 0xffff)
				  + (ip->ip_dst.s_addr >> 16)
				  + (ip->ip_dst.s_addr & 0xffff));
			if (cklen > 0) {
			    cksum = in_cksum_sub((ushort*)((char*)ip + hlen),
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
			    IPG_METER(pi->pi_dbg.bd_cksum_bad++);
			}
		    } else {
			IPG_METER(pi->pi_dbg.bd_cksum_proto++);
		    }
#ifdef IPG_DEBUG_EXTRA
		/* METER */
		} else if(totlen>(4096+sizeof(struct ip)+sizeof(struct fddi))){
			if ((totlen & 3) != 0)
				IPG_METER(pi->pi_dbg.bd_cksum_odd++);
			else
				IPG_METER(pi->pi_dbg.bd_cksum_missing++);
#endif
		}
		/* Predict activity based on probable TCP/IP acks,
		 * which tend to imply a lot of work.
		 */
		if (totlen == 40+sizeof(struct fddi))
			pi->pi_byte_busy += ACKRATE*IPG_BIG_SIZE/2;

		if (network_input(m, AF_INET, 0)) {
			ipg_drown(pi);
			pi->pi_if.if_iqdrops++;
			pi->pi_if.if_ierrors++;
		}
		return;

	case ETHERTYPE_ARP:
		/* Assume that if_ether.c can understand arp_hrd
		 *	if we strip the LLC header.
		 */
		arpinput(&pi->pi_ac, m);
		return;

	case FDDI_PORT_LLC:
		if (totlen >= sizeof(struct lmac_hdr)+2
		    && ipg_dlp(pi, fhp->fbh_fddi.fddi_llc.llc_dsap,
			       DL_LLC_ENCAP, m, totlen))
			return;
		break;

	default:
		if (ipg_dlp(pi, port, DL_SNAP0_ENCAP, m, totlen))
			return;
		break;
	}

	/* if we cannot find a protocol queue, then flush it down the
	 *	drain, if it is open.
	 */
	if (RAWIF_DRAINING(&pi->pi_rawif)) {
		drain_input(&pi->pi_rawif, port,
			    (caddr_t)&fhp->fbh_fddi.fddi_mac.mac_sa, m);
	} else {
		pi->pi_if.if_noproto++;
		m_freem(m);
	}
}

/*
 * process the board-to-host ring
 * Must be called with interrupts off and the driver ifnet lock held.
 */
static void
ipg_b2h(struct ipg_info *pi, u_long ps_ok)
{
	int blen, op, pokebd;
	u_char sn;
	struct mbuf *m;
	volatile struct ipg_b2h *b2hp;

#ifdef EVEREST
	volatile struct ipg_b2h *b2h_warp;
	u_char war_sn;
	int war_flush;

	war_flush = (io4_flush_cache != 0 && io4ia_war) ? 0 : 0x7fffffff;
#endif

	/* 
	 * The following is really a compile time assertion NOT a run-time one.
	 *
	 * ASSERT((IPG_B2H_LEN % 256 ) != 0);
	 */
	CK_IFNET(pi);

#ifdef EVEREST
	if (*ipg_slopp != 0) {

		zapbd(pi, pi->pi_sio);
		cmn_err_tag(236,CE_PANIC, "ipg%d DMA data corruption\n",
			pi->pi_if.if_unit);
	}
#endif /* EVEREST */

	/* if recursing, for example here->arp->output->here, then quit
	 */
	if (pi->pi_in_active)
		return;

	pi->pi_in_active = 1;

	if ((pi->pi_state & PI_ST_NOPOKE) && pi->pi_if.if_snd.ifq_len != 0) {
		pokebd = 1;
	} else {
		pokebd = 0;
	}
	pi->pi_state &= ~PI_ST_NOPOKE;

	b2hp = pi->pi_b2hp;

	for (;;) {

		sn = b2hp->b2h_sn;
		blen = b2hp->b2h_val;
		op = b2hp->b2h_op;

		if (sn != pi->pi_b2h_sn) {  /* no more new work */
			pi->pi_b2hp = b2hp;

			if (pi->pi_b2h_sn != (sn+IPG_B2H_LEN)%256
			    && !(pi->pi_state & PI_ST_WHINE)) {

				DEBUGPRINT(pi, ("ipg%d bad B2H sn=0x%x !=0x%x"
						" at 0x%x op=%d val=0x%x\n",
						pi->pi_if.if_unit,
						sn,pi->pi_b2h_sn,
						b2hp,op,blen));
				pi->pi_state |= PI_ST_WHINE;
			}
			if (ipg_fillin(pi)) {	/* restock input queues */
				pokebd = 1;
				continue;
			}
			/* Fix race between board going to sleep as host is
			 * giving it buffers.
			 */
			if (pokebd && (pi->pi_state & PI_ST_SLEEP)) {
				pi->pi_state &= ~PI_ST_SLEEP;
				IPG_METER(pi->pi_dbg.board_poke++);
				ipg_wait(pi,pi->pi_sio);
				ipg_op(pi->pi_sio,ACT_WAKEUP);
			}
			pi->pi_in_active = 0;
			return;
		}

		pi->pi_b2h_sn = sn+1;

		switch (op) {

		case IPG_B2H_SLEEP:	/* board is going to sleep */
			pi->pi_state |= PI_ST_SLEEP;
			break;

		case IPG_B2H_ODONE:
			/* free next output mbuf chains
			 */
#ifdef IPG_DEBUG_EXTRA
			if (blen <= 0) { /* error */
				zapbd(pi, pi->pi_sio);
				cmn_err_tag(237,CE_PANIC,
				"ipg_b2h: BOGUS blen %d, ipg%d: ifnet 0x%x\n",
				 blen, pi->pi_if.if_unit, &(pi->pi_ac.ac_if));
			}
#endif /* IPG_DEBUG_EXTRA */


			do {
#ifdef IPG_DEBUG_EXTRA
				m = pi->pi_if.if_snd.ifq_head;

				if ((m == IPG_BOGUS1) || (m == IPG_BOGUS2)) {
					zapbd(pi, pi->pi_sio);
					cmn_err_tag(238,CE_PANIC,
	"ipg_b2h: ipg%d, BOGUS ifq_head; m 0x%x, sn %d, blen %d, ifnet 0x%x\n",
		 pi->pi_if.if_unit, m, sn, blen, &(pi->pi_ac.ac_if));
				}

				if (m && ((m->m_act == IPG_BOGUS1) ||
					  (m->m_act == IPG_BOGUS2))) {
					zapbd(pi, pi->pi_sio);
					cmn_err_tag(239,CE_PANIC,
	"ipg_b2h: ipg%d, BOGUS m_act; m 0x%x, sn %d, blen %d, ifnet 0x%x\n",
			  pi->pi_if.if_unit, m, sn, blen, &(pi->pi_ac.ac_if));
				}
#endif /* IPG_DEBUG_EXTRA */
				IF_DEQUEUE_NOLOCK(&pi->pi_if.if_snd, m);
				m_freem(m);

			} while (--blen > 0);

			if ((pi->pi_state & ps_ok) != 0)
				ps_txq_stat(&pi->pi_if,
				    (IFQ_MAXLEN - pi->pi_if.if_snd.ifq_len));
			break;

		case IPG_B2H_IN_DONE:
#ifdef EVEREST
			if (--war_flush == -1) {
				war_flush = 0;
				io4_flush_cache((paddr_t)pi->pi_sio);
				b2h_warp = b2hp;
				war_sn = sn;
				for (;;) {
					if (++b2h_warp >= pi->pi_b2h_lim)
						b2h_warp = pi->pi_b2h_buf;
					if (++war_sn != b2h_warp->b2h_sn)
						break;
					if (b2h_warp->b2h_op==IPG_B2H_IN_DONE)
						war_flush++;
				}
			}
			/* fall into initial buffer case */
#endif
		case IPG_B2H_IN:
			pi->pi_state &= ~PI_ST_SLEEP;
			ipg_input(pi, op, blen);
			break;

		case IPG_B2H_PHY:
			ipg_phy_intr(pi, blen, blen>>8);
			pokebd = 1;
			break;

		default:		/* sick board */
			if (!(pi->pi_state & PI_ST_WHINE))
				DEBUGPRINT(pi, ("ipg%d: bad B2H"
						" sn=%d op=%d val=0x%x\n",
						pi->pi_if.if_unit,
						sn, op, blen));
			pi->pi_state |= PI_ST_WHINE;
			break;
		}

		if (++b2hp >= pi->pi_b2h_lim)
			b2hp = pi->pi_b2h_buf;
	}
}

/*
 * handle interrupts
 * Returns 1 if found a reason to interrupt and ZERO on stray interrupt
 */
int
if_ipgintr(int unit)
{
	IPG_SIO *sio;
	struct ipg_info *pi = &ipg_info[unit];

	ASSERT(unit >= 0 && unit < IPG_MAXBD);

	/* If the board is not up, forget it.
	 *	This can happen while we are bootstrapping the system,
	 *	beforing resetting and probing the board.  The board
	 *	may still be active from PROM requests, or even for
	 *	requests made before the system last went down.
	 */
	sio = pi->pi_sio;
	if (sio == 0) {
		printf("ipg%d: stray interrupt\n", unit);
		return 0;
	}

	IFNET_LOCK(&pi->pi_if);
	IPG_METER(pi->pi_dbg.ints++);

#ifdef IPG_DEBUG_EXTRA
	if (pi->pi_state & PI_ST_DRVRINTR) { /* die as driver is re-entered */
		cmn_err_tag(240,CE_PANIC, "ipg%d: ifnet 0x%x Driver re-entered\n",
			pi->pi_if.if_unit, &(pi->pi_ac.ac_if));
	}
	pi->pi_state |= PI_ST_DRVRINTR;
#endif

	/* account for interrupt delay already done by the board */
	pi->pi_byte_busy -= ipg_int_delay;

	ipg_b2h(pi, PI_ST_PSENABLED);	/* check the DMA pipeline */

#ifdef IPG_DEBUG_EXTRA
	pi->pi_state &= ~PI_ST_DRVRINTR;
#endif
	IFNET_UNLOCK(&pi->pi_if);

	return 1;			/* do not worry about stray ints */
}

/* probe for a single board, and attach if present.
 */
void
if_ipgedtinit(struct edt *edtp)
{
	struct ipg_info *pi;
	IPG_SIO *sio;
	unsigned int unit;
	struct vme_intrs *intp;
	graph_error_t rc;
	vertex_hdl_t vmevhdl;
	vertex_hdl_t ipgvhdl;
	char loc_str[32];

	static struct smt_ops ipg_ops = {
#define SOP struct smt*, void*
		(void (*)(SOP))			ipg_reset_tpc,
		(time_t (*)(SOP))		ipg_get_tpc,
		(void (*)(SOP))			ipg_pcm_off,
		(void (*)(SOP, enum rt_ls))	ipg_setls,
		(void (*)(SOP))			ipg_lct_on,
		(int (*)(SOP))			ipg_lct_off,
		(void (*)(SOP))			ipg_cfm,
		(int (*)(SOP))			ipg_st_get,
		(int (*)(SOP, struct smt_conf*))ipg_st_set,
		(void (*)(SOP, u_long))		ipg_alarm,
		(void (*)(SOP, struct d_bec_frame*, int))ipg_d_bec,
		(struct smt* (*)(SOP))		ipg_osmt,
		(void (*)(SOP))			ipg_trace_req,
		0,
		0,
		(void (*)(struct smt*))		host_pcm,
#undef SOP
	};
	static SMT_MANUF_DATA mnfdat = SGI_FDDI_MNFDATA(ipgx);

#define MANUF_INDEX (sizeof(SGI_FDDI_MNFDATA_STR(ipgx))-2)
	piomap_t	*piomap;

	int s;
	int pi_vme_adapter;
	struct ps_parms ps_params;

	intp = (vme_intrs_t *)edtp->e_bus_info;
	unit = edtp->e_ctlr;
	if (IPG_MAXBD <= unit) {
		printf("ipg%u: bad EDT entry\n", unit);
		return;
	}
	ASSERT(IPG_BIG_SIZE <= MCLBYTES);
	ASSERT(IPG_SML_SIZE <= MLEN);
	ASSERT(sizeof(IPG_CNT) == 4);

	/* zero slop area */
	bzero(ipg_slopbuf, IPG_MAX_SLOPSIZE);

	/* compute starting address of I/O page within slop buffer */
	ipg_slopp = &ipg_slopbuf[(IO_NBPC
	 - ((__psunsigned_t)ipg_slopbuf) % IO_NBPC)/ sizeof(ipg_slopbuf[0])];

	pi = &ipg_info[unit];

	if (0 != pi->pi_sio) {
		printf("ipg%u: duplicate EDT entry\n", unit);
		return;
	}
	pi->pi_if.if_unit = unit;
	pi->pi_vme_adapter = pi_vme_adapter = edtp->e_adap;

	piomap = pio_mapalloc(edtp->e_bus_type,
			      pi_vme_adapter,
			      &edtp->e_space[0],
			      PIOMAP_FIXED,
			      "ipg");
	if (!piomap) {
		printf("ipg%u: PIO map failed\n", unit);
		return;
	}
	sio = (IPG_SIO*)pio_mapaddr(piomap, edtp->e_iobase);

	if (badaddr(&sio->sign, sizeof(sio->sign))) {
		pio_mapfree(piomap);
		printf("ipg%u: sio bad address\n", unit);
		return;
	}

#ifdef EVEREST
	pi->pi_iamap = iamap_get(pi->pi_vme_adapter, DMA_A32VME);

	pi->pi_h2b_buf = &pi->pi_h2b0[0];
	pi->pi_b2h_buf = &pi->pi_b2h0[0];

	pi->pi_dmamap = dma_mapalloc(DMA_A32VME,
			pi_vme_adapter,
			io_btoc(sizeof(pi->pi_b2h0) + sizeof(pi->pi_h2b0)),
			VM_NOSLEEP);

	if (!pi->pi_dmamap ||
	    !dma_map(pi->pi_dmamap,
		     (caddr_t)pi->pi_b2h_buf,
		     sizeof(pi->pi_b2h0) + sizeof(pi->pi_h2b0))) {

		printf("ipg%u: dma_map failed\n", unit);
		return;
	}
	iamap_map(pi->pi_iamap, pi->pi_dmamap);

	pi->pi_sml_map = ipg_mapalloc(pi, SML_MAP_LEN+2);
	pi->pi_med_map = ipg_mapalloc(pi, MED_MAP_LEN+2);
	pi->pi_big_map = ipg_mapalloc(pi, BIG_MAP_LEN+2);
	pi->pi_out_map = ipg_mapalloc(pi, OUT_MAP_LEN+2);

	if (!pi->pi_sml_map ||
	    !pi->pi_med_map ||
	    !pi->pi_big_map ||
	    !pi->pi_out_map) {

		printf("ipg%u: DMA mapping failed\n", unit);
		return;
	}
#else
	pi->pi_h2b_buf = (struct ipg_h2b*)K0_TO_K1(&pi->pi_h2b0[0]);
	pi->pi_b2h_buf = (struct ipg_b2h*)K0_TO_K1(&pi->pi_b2h0[0]);
#endif /* EVEREST */

	if (!intp->v_vec)
		intp->v_vec = vme_ivec_alloc(pi_vme_adapter);
	if (intp->v_vec == (unsigned char)-1) {
		printf("ipg%u: no interrupt vector\n", unit);
		return;
	}
	vme_ivec_set(pi_vme_adapter, intp->v_vec, if_ipgintr, unit);

	/* things looks ok, so mark the interface present */
	pi->pi_sio = sio;

	pi->pi_vector = intp->v_vec;
	pi->pi_brl = intp->v_brl;

	s = splimp();

	pi->pi_smt1.smt_pdata = (caddr_t)pi;
	pi->pi_smt1.smt_ops = &ipg_ops;
	pi->pi_smt2.smt_pdata = (caddr_t)pi;
	pi->pi_smt2.smt_ops = &ipg_ops;

	pi->pi_smt1.smt_conf.tvx = ((FDDI_DEF_TVX - 254)/255)*255;
	pi->pi_smt2.smt_conf.tvx = ((FDDI_DEF_TVX - 254)/255)*255;
	pi->pi_smt1.smt_conf.t_max = DEF_T_MAX;
	pi->pi_smt2.smt_conf.t_max = DEF_T_MAX;
	pi->pi_smt1.smt_conf.t_req = DEF_T_REQ;
	pi->pi_smt2.smt_conf.t_req = DEF_T_REQ;
	pi->pi_smt1.smt_conf.t_min = FDDI_MIN_T_MIN;
	pi->pi_smt2.smt_conf.t_min = FDDI_MIN_T_MIN;

	pi->pi_smt1.smt_conf.ip_pri = 1;
	pi->pi_smt2.smt_conf.ip_pri = 1;

	pi->pi_smt1.smt_conf.ler_cut = SMT_LER_CUT_DEF;
	pi->pi_smt2.smt_conf.ler_cut = SMT_LER_CUT_DEF;

	mnfdat.manf_data[MANUF_INDEX] = unit+'0';
	bcopy(&mnfdat, &pi->pi_smt1.smt_conf.mnfdat,
	      sizeof(mnfdat));

	pi->pi_if.if_name = ipg_name;	/* create ('attach') the interface */
	pi->pi_if.if_type = IFT_FDDI;
	pi->pi_if.if_baudrate.ifs_value = 1000*1000*100;
	pi->pi_if.if_flags = IFF_BROADCAST|IFF_MULTICAST|IFF_IPALIAS;

#ifdef IPG_DEBUG
	pi->pi_if.if_flags |= IFF_DEBUG;
	pi->pi_smt1.smt_st.flags |= PCM_DEBUG;
	pi->pi_smt2.smt_st.flags |= PCM_DEBUG;
#endif
	pi->pi_if.if_output = ipg_output;
	pi->pi_if.if_ioctl = ipg_ioctl;
	pi->pi_if.if_rtrequest = arp_rtrequest;

	if_attach(&pi->pi_if);		/* must get lock initialized 1st */

	IFNET_LOCK(&pi->pi_if);
	ipg_reset(pi,1);		/* reset the board */
	IFNET_UNLOCK(&pi->pi_if);

	if (showconfig)
		printf("ipg%u: present\n", unit);


	/* Allocate a multicast filter table with an initial size of 10.
	 */
	if (!mfnew(&pi->pi_filter, 10)) {
		cmn_err_tag(241,CE_PANIC, "ipg%d: no memory for frame filter\n",
			unit);
	}

	/* Initialize the raw ethernet socket interface.
	 *	tell the snoop stuff to watch for our address in FDDI bit
	 *	order.
	 */
	rawif_attach(&pi->pi_rawif, &pi->pi_if,
		     (caddr_t)&pi->pi_smt1.smt_st.addr,
		     (caddr_t)&etherbroadcastaddr[0],
		     sizeof(pi->pi_smt1.smt_st.addr),
		     sizeof(struct fddi),
		     structoff(fddi, fddi_mac.mac_sa),
		     structoff(fddi, fddi_mac.mac_da));

	add_to_inventory(INV_NETWORK, INV_NET_FDDI, INV_FDDI_IPG,
			 unit, sio->vers);

	rc = vme_hwgraph_lookup(pi_vme_adapter, &vmevhdl);

	if (rc == GRAPH_SUCCESS) {
		char alias_name[8];
		sprintf(loc_str, "%s/%d", EDGE_LBL_IPG, unit);
		sprintf(alias_name, "%s%d", EDGE_LBL_IPG, unit);
		rc = if_hwgraph_add(vmevhdl, loc_str, "if_ipg", alias_name,
			&ipgvhdl);
	}

	/* RSVP.  Support both admission control and packet scheduling. */
	ps_params.bandwidth = 100000000;
	ps_params.txfree = IFQ_MAXLEN;
	ps_params.txfree_func = ipg_txfree_len;
	ps_params.state_func = ipg_setstate;
	ps_params.flags = 0;
	(void)ps_init(&pi->pi_if, &ps_params);

	splx(s);
}
#endif /* !IP20 && !IP22 */
