/* GIO bus FDDI board
 *
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.107 $"

#if defined(IP20) || defined(IP22) || defined(IP26) || defined(IP28) || defined(EVEREST)

#include <tcp-param.h>
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/reg.h>
#include <sys/vmereg.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/ddi.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/edt.h>
#include <sys/kopt.h>
#include <sys/invent.h>
#include <sys/giobus.h>
#include <bstring.h>
#include <sys/capability.h>

#ifdef EVEREST
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#define OLD_STYLE
#include <sys/EVEREST/dang.h>
#if _MIPS_SIM == _ABI64
#define ADDR_HI(a) ((a) >> 32)
#else
#define ADDR_HI(a) 0
#endif
#endif /* EVEREST */

#include <net/soioctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/raw.h>
#include <net/multi.h>

#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/udp_var.h>
#include <netinet/tcpip.h>

#include <ether.h>
#include <sys/dlsap_register.h>
#include <sys/fddi.h>
#include <sys/smt.h>
#include <sys/if_xpi.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <net/rsvp.h>

#ifdef EVEREST
#pragma weak io4ia_war
#pragma weak io4_flush_cache
extern int io4ia_war;			/* fight the IO4 IA chip bug */
extern int io4_flush_cache(paddr_t);
#endif

extern time_t lbolt;
extern struct ifnet loif;

extern int xpi_cksum;
extern int xpi_lat_void;
extern int xpi_mtu;
extern int xpi_low_latency;
extern int xpi_early_release;

int xpi_fake_in_cksum = 0;
int xpi_zapring = 0;

/* -DXPI_DEBUG to generate a lot of debugging code */

#ifdef XPI_DEBUG
#define SHOWCONFIG 1
#else
#define SHOWCONFIG showconfig
#endif

/* Allow this many boards. */
#ifdef EVEREST
#define XPI_MAXBD   (12*2)		/* really # of ports, 2 ports/board */
#else
#define XPI_MAXBD   3
#endif

static char *xpi_name = "xpi";

#define DEF_T_REQ (FDDI_DEF_T_REQ & ~0xff)
#define MIN_T_REQ MIN(FDDI_MIN_T_MIN & ~0xff, -0x100)
#define MAX_T_REQ (-0xffff00)
#define DEF_T_MAX MIN_T_MAX
#define MIN_T_MAX (FDDI_MIN_T_MAX & ~0xffff)
#define MAX_T_MAX (MAX_T_REQ & ~0xffff)
#define DEF_TVX (FDDI_DEF_TVX & ~0xff)
#define MIN_TVX (FDDI_MIN_TVX & ~0xff)
#define MAX_TVX MAX(FDDI_MAX_TVX & ~0xff, -0xff00)

#undef WBCACHE
#if defined(IP20) || defined(IP22) || defined(IP26) || defined(IP28)
#define WBCACHE				/* IP17 style R4000 cache */
#endif

#define CK_IFNET(xi) ASSERT(IFNET_ISLOCKED(&(xi)->xi_if))

int if_xpidevflag = D_MP;


/* DMA parameters
 */
#define FRAME_SEC 2800			/* system instataneous speed */

/* Assume the ratio of data to ack frames is about this
 */
#define ACKRATE 4

/* DMA control info at this rate
 */
int xpi_c2b_dma_delay = (USEC_PER_SEC/FRAME_SEC)*(ACKRATE-1);

/* Handling an ordinary input frame makes the host busy for this long.
 * Writing a frame implies the host will be busy that long.
 */
#define BTODELAY(b) (((b)*((USEC_PER_SEC*16)/(XPI_BIG_SIZE*FRAME_SEC)) +7) /16)

/* limit interrupts to this many equivalent bytes
 */
int xpi_byte_int_delay = XPI_BIG_SIZE;

/* Do not let the interrupt delay exceed this.
 */
int xpi_max_host_work = (USEC_PER_SEC/FRAME_SEC);


/* Turn off host control-to-board DMA after this much idleness.
 * There are races which are prevented by assuming that the worst case
 * host input acknowledgement/processing latency is less than this.  It should
 * be generous.  However, it should not be indefinite, because there is
 * no profit in doing 1000 DMA operations/second without any network traffic.
 */
int xpi_c2b_sleep = MAX(USEC_PER_SEC/50, (USEC_PER_SEC/FRAME_SEC)*ACKRATE*8);


/* limits on mbufs given the board
 */
#ifdef XPI_DEBUG
int xpi_num_big = (XPI_MAX_BIG*3/4);
int xpi_num_med = (XPI_MAX_MED*3/4);
int xpi_num_sml = (XPI_MAX_SML);
#else
#define xpi_num_big (XPI_MAX_BIG*3/4)
#define xpi_num_med (XPI_MAX_MED*3/4)
#define xpi_num_sml (XPI_MAX_SML)
#endif


/* When this driver is dual-MAC, we consider the "primary" board
 *	to be the even unit number.
 */
#define PRIMARY_XI(xi)	(0 == ((xi)->xi_if.if_unit & 1))
#define OTHER_XI(xi)	(PRIMARY_XI(xi) ? ((xi)+1) : ((xi)-1))

#define SMT1(smt,xi)	(&(xi)->xi_smt1 == (smt))
#define OTHER_SMT(xi,smt) (SMT1(smt,xi) ? &(xi)->xi_smt2 : &(xi)->xi_smt1)

#define ELM(smt,zero,one) (((smt)->smt_conf.pc_type == PC_B) ? (one) : (zero))

#define IS_DASSM(xi)	((xi)->xi_smt1.smt_conf.type == SM_DAS)
#define IS_SA(xi)	((xi)->xi_smt1.smt_conf.type == SAS)


#ifdef EVEREST
#define PF_L		"xpi%d slot %d adapter %d: "
#define PF_P(xi)	(xi)->xi_if.if_unit, (xi)->xi_slot, (xi)->xi_adapter
#else
#define PF_L		"xpi%d: "
#define PF_P(xi)	(xi)->xi_if.if_unit
#endif

#define ZAPBD(xi) {			\
	*(xi)->xi_gio = XPI_GIO_RESET;	\
	(xi)->xi_state |= XI_ST_ZAPPED;	\
	DELAYBUS(XPI_GIO_RESET_DELAY);	\
	*(xi)->xi_gio = 0;		\
}

#define BAD_BD(xi,ex,bail) if (!(ex)) {			\
	printf(PF_L "failure \"ex\"\n", PF_P(xi));	\
	ZAPBD(xi);					\
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
#ifdef XPI_DEBUG
#define DELAY_RESET	DELAY_RESET_DBG
#else
#define DELAY_RESET	DELAY_EEPROM
#endif
#define DELAY_OP	5000		/* delay for most operations */



extern char *smt_pc_label[];
static char *str_ls[] = PCM_LS_LABELS;	/* printf labels for line states */

extern int smt_debug_lvl;

#define DEBUGPRINT(xi,a) {if ((xi)->xi_if.if_flags&IFF_DEBUG) printf a;}

#define	OUT_ALIGNED(p) (0 == ((__psunsigned_t)(p) % sizeof(__uint32_t)))



extern void bitswapcopy(void *, void *, int);
extern int looutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);


struct xpi_info;			/* forward reference */

static void xpi_setvec(struct smt*, struct xpi_info*, int);
static void xpi_join(struct smt*, struct xpi_info*);

static void xpi_start(struct xpi_info*, int);
static void xpi_macinit(struct xpi_info*);
static int xpi_fillin(struct xpi_info*);
static void xpi_b2h(struct xpi_info*, u_long);

static void xpi_reset_tpc(struct smt*, struct xpi_info*);
static time_t xpi_get_tpc(struct smt*, struct xpi_info*);
static void xpi_pcm_off(struct smt*, struct xpi_info*);
static void xpi_setls(struct smt*, struct xpi_info*, enum rt_ls);
static void xpi_lct_on(struct smt*, struct xpi_info*);
static int  xpi_lct_off(struct smt*, struct xpi_info*);
static void xpi_cfm(struct smt*, struct xpi_info*);
static int xpi_st_get(struct smt*, struct xpi_info*);
static int  xpi_st_set(struct smt*, struct xpi_info*, struct smt_conf*);
static void xpi_alarm(struct smt*, struct xpi_info*, u_long);
static void xpi_d_bec(struct smt*, struct xpi_info*, struct d_bec_frame*,int);
static struct smt* xpi_osmt(struct smt*, struct xpi_info*);
static void xpi_trace_req(struct smt*, struct xpi_info*);


static struct smt_ops xpi_ops = {
#define SOP struct smt*, void*
	(void (*)(SOP))			xpi_reset_tpc,
	(time_t (*)(SOP))		xpi_get_tpc,
	(void (*)(SOP))			xpi_pcm_off,
	(void (*)(SOP, enum rt_ls))	xpi_setls,
	(void (*)(SOP))			xpi_lct_on,
	(int (*)(SOP))			xpi_lct_off,
	(void (*)(SOP))			xpi_cfm,
	(int (*)(SOP))			xpi_st_get,
	(int (*)(SOP, struct smt_conf*))xpi_st_set,
	(void (*)(SOP, u_long))		xpi_alarm,
	(void (*)(SOP, struct d_bec_frame*, int))xpi_d_bec,
	(struct smt* (*)(SOP))		xpi_osmt,
	(void (*)(SOP))			xpi_trace_req,

	(void (*)(SOP, int))		xpi_setvec,
	(void (*)(SOP))			xpi_join,
	(void (*)(struct smt*))		moto_pcm,
#undef SOP
};



/* counters for each board
 */
struct xpi_dbg {
	uint	out_copy;		/* had to copy mbufs on output */
	uint	out_alloc;		/* had to alloc mbuf for header */
	uint	out_pullup;		/* had to use m_pullup() */
	uint	own_out;		/* heard our own output */
	uint	pg_cross;		/* buffer crossed page */

	uint	waste_page;		/* input used a cluster but no flip */

	uint	ints;			/* total interrupts */
#ifndef EVEREST
	uint	noint;			/* false alarm */
#endif

	uint	board_dead;		/* gave up waiting for the board */
	uint	board_waits;		/* waited for the board */
	uint	board_poke;		/* had to wake up the board */
#ifdef DODROWN
	uint	drown_on;		/* started drowning */
	uint	drown_off;
#endif

	uint	in_cksums;		/* board offered us a checksum */
	uint	in_cksum_frag;		/* packet was an IP fragment */
	uint	in_cksum_bad;		/* bad checksum */
	uint	in_cksum_proto;		/* wrong protocol */
	uint	in_cksum_missing;	/* unexpected missing checksum */
	uint	in_cksum_odd;		/* no checksum because odd buffer */

	uint	out_cksum_udp;		/* output UDP checksums by board */
	uint	out_cksum_tcp;		/* output TDP checksums by board */
	uint	out_cksum_encap;	/* encapsulate checksum by board */

	uint	in_active;		/* input recursion prevented */

	uint	no_sml;
	uint	no_med;			/* ran out of this size */
	uint	no_big;
};




/* info per board
 */
struct xpi_info {
	struct arpcom	xi_ac;		/* common interface structures */
	struct fddi	xi_hdr;		/* prototype MAC & LLC header */
	struct mfilter	xi_filter;
	struct rawif	xi_rawif;

	u_char		xi_macaddr[6];	/* MAC address from the board */

#ifdef EVEREST
	struct xpi_info *xi_xi2;	/* sister board */
	uint		xi_adapter;
#endif
	uint		xi_slot;
	volatile __uint32_t *xi_gio;	/* look for the board here */

	__uint32_t	xi_inum;	/* most recently seen interrupt # */
	__uint32_t	xi_cmd_id;	/* ID of last command given board */
	int		xi_cmd_line;

	__uint32_t	xi_vers;	/* firmware version from board */

	u_long		xi_state;	/* useful state bits */

	struct xpi_dbg	xi_dbg;		/* debugging counters */

	int		xi_in_active;	/* input recursion preventer */

	struct mbuf	*xi_in;		/* growing input chain */
	int		xi_in_len;

	int		xi_byte_busy;	/* accumulated int. delay in bytes */

	struct mbuf	*xi_in_sml_m[XPI_MAX_SML];  /* input buffer pool */
	FDDI_IBUFLLC	*xi_in_sml_d[XPI_MAX_SML];
	int		xi_in_sml_num;
	int		xi_in_sml_h;
	int		xi_in_sml_t;
	struct mbuf	*xi_in_med_m[XPI_MAX_MED];
	FDDI_IBUFLLC	*xi_in_med_d[XPI_MAX_MED];
	int		xi_in_med_num;
	int		xi_in_med_h;
	int		xi_in_med_t;
	struct mbuf	*xi_in_big_m[XPI_MAX_BIG];
	FDDI_IBUFLLC	*xi_in_big_d[XPI_MAX_BIG];
	int		xi_in_big_num;
	int		xi_in_big_h;
	int		xi_in_big_t;

	u_char		xi_hash[MMODE_HLEN];	/* DA filter */
	uint		xi_nuni;	/* # active unicast addresses */
	mval_t		xi_hash_mac;	/* hash value of our MAC address */
	mval_t		xi_hash_bcast;	/* hash value of broadcast address */

	struct xpi_counts xi_prev_counts;
	__uint32_t	xi_old_noise0;	/* previous values to generate */
	__uint32_t	xi_old_noise1;	/*	SMT events */

	struct timeval	xi_time;	/* free running timer */
	struct timeval	xi_tpc1;	/* PCM timer for smt1 */
	struct timeval	xi_tpc2;	/* PCM timer for smt2 */

	int		xi_smt1_n;	/* bits being signaled */
	int		xi_smt2_n;
	int		xi_elm1_st;	/* recent ELM state */
	int		xi_elm2_st;

	struct xi_lem {
	    u_long	usec_prev;	/* usecs when last measured */
	    __uint32_t	ct_start;	/* short term LEM started */
	    __uint32_t	ct_cur;		/* recent raw LEM count */
	    __uint32_t  ct2;		/* counts since LEM turned on */
	    __uint32_t  ct;		/* total count */
	    struct timeval time2;	/* duration for current value */
	    struct timeval time;	/* total duration */
	} xi_lem0, xi_lem1;		/* ELM0 LEM, ELM1 LEM */

	struct smt	xi_smt1;	/* PHY-S or PHY-B for DAS-SM */
	struct smt	xi_smt2;	/* PHY-A for DAS-SM */

	/* DMAed control structures */
#ifdef _NO_UNCACHED_MEM_WAR
	struct xpi_dma1	*xi_dma1_u;	/* uncached */
	struct xpi_dma1	*xi_dma1_c;	/* cached */
#else
	struct xpi_dma1	*xi_dma1;
#endif
	struct xpi_dma2	*xi_dma2;

	volatile struct xpi_b2h	*xi_b2hp;
	volatile struct xpi_b2h	*xi_b2h_lim;
	u_char		xi_b2h_sn;

	/* (These are volatile not because the board writes them,
	 * but to ensure the compiler stores them in the right order.)
	 */
	volatile union xpi_d2b *xi_d2bp;
	volatile union xpi_d2b *xi_d2b_lim;

	volatile struct xpi_c2b	*xi_c2bp;
	volatile struct xpi_c2b	*xi_c2b_lim;
} *xpi_infop[XPI_MAXBD];

#define xi_if		xi_ac.ac_if     /* network-visible interface */

/* bits in XI_STATE */
#define XI_ST_BCHANGE	0x0001		/* optical bypass changed */
#define XI_ST_MAC	0x0002		/* MAC is on */
#define XI_ST_LEM0	0x0004		/* ELM0 LEM is on */
#define XI_ST_LEM1	0x0008		/* ELM1 LEM is on */
#define XI_ST_ADDRSET	0x0010		/* MAC address set by ioctl */
#define XI_ST_ARPWHO	0x0020		/* IP address announcement needed */
#define XI_ST_ZAPPED	0x0040		/* board has been naughty */
#ifdef DODROWN
#define XI_ST_DROWN	0x0080		/* drowning in input--promiscuous? */
#endif
#define XI_ST_SLEEP	0x0100		/* C2B DMA is asleep */
#define XI_ST_WHINE	0x0200		/* have whined about sick board */
#define XI_ST_NOPOKE	0x0400		/* skipped a board wakeup */
#define XI_ST_PROM	0x0800		/* MAC is promiscuous */
#define XI_ST_D_BEC	0x1000		/* board using directed beacon */
#define XI_ST_PSENABLED 0x2000		/* RSVP packet sched. enabled */
#ifdef EVEREST
#define XI_ST_BAD_DANG	0x4000		/* mez. card with rev.B DANG chip */
#endif


/* These structures are read and written by the board, and must not
 * cross a page boundary or be above 4GB.
 */
struct xpi_dma1 {
	/* This should be one Everest cache line for speed.
	 * It must be spread accross two IP26/28 cache lines so the first part
	 * can be cached so less than 8-bytes at a time can be written to it
	 * and so the second part can be uncached for reasonable access.
	 * The IP26/28 does not allow any write to uncached memory when running
	 * normally, which makes it impossible to make the communication areas
	 * uncached.
	 */
	struct {
		struct xpi_hcw	w;
		struct xpi_hcr	r;
	} dma_hc;

	volatile struct xpi_b2h	dma_b2h[XPI_B2H_LEN];
#ifdef EVEREST
	/* padding for 4 IO4 cache lines for Everest IO4 IA chip bug
	 * work-around.
	 */
	volatile char dma_b2h_io4ia[128*4];
#endif
	volatile struct xpi_counts dma_new_counts;
#ifdef _NO_UNCACHED_MEM_WAR
	/* The preceding are uncached on the IP26 so that accessing them
	 * is not so painful.
	 */
	char		dma_pad2[CACHE_SLINE_SIZE];
	/* The following pair are cached on the IP26 to meet its
	 * 8-byte-write restrictions.
	 */
#endif
	struct fsi_cam	dma_cam[FSI_CAM_LEN];
	struct xpi_c2b	dma_c2b[XPI_C2B_LEN];
};

struct xpi_dma2 {
	union xpi_d2b	dma_d2b[XPI_D2B_LEN];
};

#ifdef _NO_UNCACHED_MEM_WAR
#define xi_hcw		xi_dma1_c->dma_hc.w
#define xi_hcr		xi_dma1_u->dma_hc.r
#define xi_new_counts	xi_dma1_u->dma_new_counts
#define xi_b2h		xi_dma1_u->dma_b2h
#define xi_cam		xi_dma1_c->dma_cam
#define xi_c2b		xi_dma1_c->dma_c2b
#else
#define xi_hcw		xi_dma1->dma_hc.w
#define xi_hcr		xi_dma1->dma_hc.r
#define xi_new_counts	xi_dma1->dma_new_counts
#define xi_b2h		xi_dma1->dma_b2h
#define xi_cam		xi_dma1->dma_cam
#define xi_c2b		xi_dma1->dma_c2b
#endif
#define xi_d2b		xi_dma2->dma_d2b

#define XI_ARG		xi->xi_hcw.arg
#define XI_RES		xi->xi_hcr.res


/*
 * RSVP
 * Called by packet scheduling functions to determine length of driver queue.
 */
static int
xpi_txfree_len(struct ifnet *ifp)
{
	struct xpi_info *xi = (struct xpi_info *)ifp;
	/*
	 * must do garbage collection here, otherwise, fewer free
	 * spaces may be reported, which would cause packet scheduling
	 * to not send down packets.
	 */
	xpi_b2h(xi, 0);
	return (XPI_MAX_OUTQ - xi->xi_if.if_snd.ifq_len);
}


/*
 * RSVP
 * Called by packet scheduling functions to notify driver about queueing
 * state.  If setting is not zero then there are queues and driver should
 * update the packet scheduler by calling ps_txq_stat() when the txq length
 * changes. If setting is 0, then don't call packet scheduler.
 */
static void
xpi_setstate(struct ifnet *ifp, int setting)
{
	struct xpi_info *xi = (struct xpi_info *)ifp;

	if (setting)
		xi->xi_state |= XI_ST_PSENABLED;
	else
		xi->xi_state &= ~XI_ST_PSENABLED;
}



#ifdef _NO_UNCACHED_MEM_WAR
/* cannot use uncached memory with IP26/28 */
#define xpi_op(xi,c) ((xi)->xi_hcw.cmd = (c),				\
		      (xi)->xi_hcw.cmd_id = ++(xi)->xi_cmd_id,		\
		      dcache_wb(&(xi)->xi_hcw,sizeof((xi)->xi_hcw)),	\
		      (xi)->xi_cmd_line = __LINE__,			\
		      *(xi)->xi_gio = XPI_GIO_H2B_INTR)
#else
#define xpi_op(xi,c) ((xi)->xi_hcw.cmd = (c),				\
		      (xi)->xi_hcw.cmd_id = ++(xi)->xi_cmd_id,		\
		      (xi)->xi_cmd_line = __LINE__,			\
		      *(xi)->xi_gio = XPI_GIO_H2B_INTR)
#endif


#define xpi_wait(xi) xpi_wait_usec(xi,DELAY_OP,__LINE__)

static void
xpi_wait_usec(struct xpi_info *xi,
	      int wait,			/* delay in usec */
	      int lineno)
{
	__uint32_t cmd_ack;
#ifdef XPI_DEBUG
	static int xpi_complain_reset = 0;
	static int xpi_continue_on = 0;
	static int xpi_cmd_log_i = 0;
#	define CMD_LOG_LEN 128
	static int xpi_cmd_log[CMD_LOG_LEN];

	xpi_cmd_log[xpi_cmd_log_i] = lineno;
	xpi_cmd_log_i = (xpi_cmd_log_i + 1) % CMD_LOG_LEN;
#endif

	for (;;) {
		if ((cmd_ack = xi->xi_hcr.cmd_ack) == xi->xi_cmd_id) {
			xi->xi_state &= ~XI_ST_WHINE;
			break;
		}
		if (wait < 0) {
			METER(xi->xi_dbg.board_dead++);
			if (!(xi->xi_state & XI_ST_WHINE)) {
				printf(PF_L "board asleep at %d with 0x%x "
				       "not 0x%x after 0x%x at %d\n",
				       PF_P(xi), lineno,
				       (long)cmd_ack, (long)xi->xi_cmd_id,
				       (long)xi->xi_hcw.cmd,
				       (long)xi->xi_cmd_line);
				xi->xi_state |= XI_ST_WHINE;
#ifdef XPI_DEBUG
				if (xpi_continue_on) {
					xpi_continue_on = 0;
					return;
				}
				if (xpi_complain_reset)
					ZAPBD(xi);
#endif
				xpi_alarm(&xi->xi_smt1,xi,SMT_ALARM_SICK);
			}
			xi->xi_hcw.cmd = XCMD_NOP;
			break;
		}
		METER(xi->xi_dbg.board_waits++);
		wait -= OP_CHECK;
		DELAY(OP_CHECK);
	}
}


/* Set the MAC multicast mode, address filters, and so on
 *	Interrupts must be off.
 */
static void
xpi_set_mmode(struct xpi_info *xi)
{
	struct mfent *mfe;
	struct fsi_cam cam[FSI_CAM_LEN];
	int i;

	xpi_wait(xi);
	XI_ARG.mac.mode = 0;

	if (xi->xi_if.if_flags & IFF_ALLMULTI)
		XI_ARG.mac.mode |= (MMODE_HMULTI|MMODE_AMULTI);

	/* fill the DA hash filter table with ones in promiscuous mode.
	 */
	xi->xi_state &= ~XI_ST_PROM;
	if (0 != (xi->xi_if.if_flags & IFF_PROMISC)) {
		xi->xi_state |= XI_ST_PROM;
		XI_ARG.mac.mode |= MMODE_PROM;
		for (i = 0; i < sizeof(XI_ARG.mac.hash); i++)
			XI_ARG.mac.hash[i] = -1;
	} else {
		/* We cannot use the CAM to snoop on individual addresses,
		 * because it would cause the A bit to be set.
		 */
		if (0 != xi->xi_nuni) {
			xi->xi_state |= XI_ST_PROM;
			XI_ARG.mac.mode |= MMODE_PROM;
		}
		bcopy(xi->xi_hash, XI_ARG.mac.hash,
		      sizeof(XI_ARG.mac.hash));
	}

	/* Copy addresses from the multicast filter to a list of commands
	 * to set the FSI CAM.  If there are too many addresses,
	 * tell the MAC to receive all multicasts.  They will always be
	 * filtered them with the hash table.
	 */
	i = 0;
	mfe = xi->xi_filter.mf_base;
	for (;;) {
		if ((mfe->mfe_flags & MFE_ACTIVE)
		    && FDDI_ISGROUP_SW(mfe->mfe_key.mk_dhost)) {
			if (i >= FSI_CAM_LEN) {
				XI_ARG.mac.mode |= MMODE_HMULTI;
				break;
			}

			bcopy(&mfe->mfe_key.mk_dhost[0],
			      &cam[i].fsi_entry[0],
			      sizeof(cam[i].fsi_entry));
			cam[i].fsi_id = (FSI_CAM_CONST | FSI_CAM_VALID | i);
			i++;
		}
		if (++mfe >= xi->xi_filter.mf_limit) {
			while (i < FSI_CAM_LEN) {
				cam[i].fsi_id = (FSI_CAM_CONST | i);
				i++;
			}
			break;
		}
	}

	/* Tell the board how to program the MAC now that we know if all
	 * multicast packets must be received.
	 */
	xpi_op(xi, XCMD_MAC_MODE);

	/* bother the board only if necessary, because changing the FSI
	 * CAM requires a MAC reset.
	 */
	if (bcmp(cam, xi->xi_cam, sizeof(xi->xi_cam))) {
		bcopy(cam,xi->xi_cam,sizeof(xi->xi_cam));
#ifdef _NO_UNCACHED_MEM_WAR
		dcache_wb(xi->xi_cam,sizeof(xi->xi_cam));
#endif
		xpi_wait(xi);
		XI_ARG.camp = svirtophys(&xi->xi_cam[0]);
		xpi_op(xi, XCMD_CAM);
	}
}


/* compute DA hash value
 */
static int
xpi_dahash(u_char *addr)		/* MAC address in FDDI order */
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
xpi_hash1(struct xpi_info *xi,
	  mval_t b)
{
	u_char *hp = &xi->xi_hash[0];

	hp[b/8] |= (((unsigned char)0x80) >> (b % 8));
}



/* turn off a DA hash bit
 */
static void
xpi_hash0(struct xpi_info *xi,
	  mval_t b)
{
	u_char *hp = &xi->xi_hash[0];

	hp[b/8] &= ~(((unsigned char)0x80) >> (b % 8));
}


/* set the MAC address
 */
static void
xpi_set_addr(struct xpi_info *xi,
	     u_char *addr)
{
	bcopy(addr,xi->xi_ac.ac_enaddr,sizeof(xi->xi_ac.ac_enaddr));

	if (xi->xi_hash_mac != xi->xi_hash_bcast
	    && !mfhasvalue(&xi->xi_filter, xi->xi_hash_mac)) {
		xpi_hash0(xi, xi->xi_hash_mac);
	}

	bitswapcopy(addr, xi->xi_smt1.smt_st.addr.b,
		    sizeof(xi->xi_smt1.smt_st.addr));

	xpi_hash1(xi,
		  xi->xi_hash_mac = xpi_dahash(xi->xi_smt1.smt_st.addr.b));

	xpi_hash1(xi,
		  xi->xi_hash_bcast = xpi_dahash(etherbroadcastaddr));
}



/* Add a destination address.
 */
static int
xpi_add_da(struct xpi_info *xi,
	   union mkey *key,
	   int ismulti)
{
	struct mfreq mfr;

	if (mfmatchcnt(&xi->xi_filter, 1, key, 0))
		return 0;

	mfr.mfr_key = key;
	mfr.mfr_value = xpi_dahash(key->mk_dhost);
	if (!mfadd(&xi->xi_filter, key, mfr.mfr_value)) {
		return ENOMEM;
	}

	xpi_hash1(xi, mfr.mfr_value);	/* tell the board */

	if (!ismulti) {
		++xi->xi_nuni;
	}

	xpi_set_mmode(xi);

	return 0;
}


/* Delete an address filter.  If key is unassociated, do nothing.
 * Otherwise delete software filter first, then hardware filter.
 */
static int
xpi_del_da(struct xpi_info *xi,
	   union mkey *key,
	   int ismulti)
{
	struct mfreq mfr;

	if (!mfmatchcnt(&xi->xi_filter, -1, key, &mfr.mfr_value))
		return 0;

	mfdel(&xi->xi_filter, key);

	/* Remove bit from the table if no longer in use.
	 */
	if (!mfhasvalue(&xi->xi_filter, mfr.mfr_value)
	    && mfr.mfr_value != xi->xi_hash_mac
	    && mfr.mfr_value != xi->xi_hash_bcast) {
		xpi_hash0(xi, mfr.mfr_value);
	}

	if (!ismulti) {
		--xi->xi_nuni;
	}

	xpi_set_mmode(xi);

	return 0;
}



/* accumulate Link Error Monitor experience
 *	This must be called at least every 500 seconds or the symbol count
 *	will be wrong.
 */
static void
xpi_lem_accum(struct smt *smt,
	      struct xpi_info *xi,
	      struct xi_lem *lp,
	      __uint32_t lem_usec,	/* usec */
	      __uint32_t lem_ct)	/* raw error count */
{
	struct timeval lem_time;
	uint new_ler2;
	__uint32_t ct;


	ct = lem_ct - lp->ct_start;
	ct = ((long)ct > 2) ? (ct-2) : 0;   /* forgive two errors */

	lp->ct2 = ct;
	lem_usec = (lem_usec - lp->usec_prev);
	lp->time2.tv_sec += lem_usec / USEC_PER_SEC;
	lp->time2.tv_usec += lem_usec % USEC_PER_SEC;
	RNDTIMVAL(&lp->time2);
	new_ler2 = smt_ler(&lp->time2,ct);  /* short-term LER */
	if (new_ler2 <= smt->smt_conf.ler_cut
	    && new_ler2 < smt->smt_st.ler2)
		xpi_alarm(smt,xi,SMT_ALARM_LEM);
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


/* Turn LEM off for a PCM
 */
static void
xpi_lemoff(struct smt *smt,
	   struct xpi_info *xi)
{
	struct xi_lem *lp;
	uint st;

	if (ELM(smt,0,1) == 0) {
		lp = &xi->xi_lem0;
		st = XI_ST_LEM0;
	} else {
		lp = &xi->xi_lem1;
		st = XI_ST_LEM1;
	}

	if (0 != (xi->xi_state & st)) {
		/* accumulate experience
		 */
		(void)xpi_st_get(0,xi);

		lp->time.tv_sec += lp->time2.tv_sec;
		lp->time.tv_usec += lp->time2.tv_usec;
		RNDTIMVAL(&lp->time);
		lp->ct += lp->ct2;
		if (lp->ct >= 0x80000000) {
			lp->ct /= 2;
			lp->time.tv_sec /= 2;
		}

		xi->xi_state &= ~st;
	}
}


/* Turn LEM on
 */
static void
xpi_lemon(struct smt *smt,
	  struct xpi_info *xi)
{
	struct xi_lem *lp;
	uint st;

	if (ELM(smt,0,1) == 0) {
		lp = &xi->xi_lem0;
		st = XI_ST_LEM0;
	} else {
		lp = &xi->xi_lem1;
		st = XI_ST_LEM1;
	}

	if (0 == (xi->xi_state & st)) {
		(void)xpi_st_get(0,xi);

		lp->time2.tv_sec = 0;
		lp->time2.tv_usec = 0;
		lp->ct_start = lp->ct_cur;
		xi->xi_state |= st;
	}
}


/* Set the output line state
 * The ELM that is upstream of the MAC when a DAS system is in THRU
 * must be PHY-A.  Because there are no MUXes on the board, there
 * is no choice in which ELM is upstream of the MAC.  This means
 * that ELM0 must be PHY-S or PHY-A.  It would be simpler to
 * always associate the smt1 structure with ELM0.  Unfortunately,
 * the rest of the system thinks smt #1 is corresponds to PHY-B.
 * This forces us to associate ELM0 with either smt1 or smt1.
 *
 *
 * Configurations:
 *			SAS		DAS-SM		DAS-DM
 *			--------	--------	--------
 * repeat:		ELM0=quiet	ELM0=thru	ELM0=???
 *  iff_dead()				ELM1=thru
 *			MAC=xx		MAC=repeating	MAC=???
 *
 * THRU			ELM0=thru	ELM0=thru	ELM0=???
 *  THRU=1				ELM1=thru
 *  JOIN=1
 *
 * WRAP_A				ELM0=thru	ELM0=???
 *  THRU=0				ELM1=loop
 *  JOIN=1
 *
 * WRAP_B				ELM0=loop	ELM0=???
 *  THRU=0				ELM1=thru
 *  JOIN=0
 */
static void
xpi_setls(struct smt *smt,
	  struct xpi_info *xi,
	  enum rt_ls ls)
{
	register u_int elm_num;

	CK_IFNET(xi);
	elm_num = ELM(smt,0,1);
	xpi_wait(xi);

	switch (ls) {
	case R_PCM_QLS:			/* send QLS */
		ls = T_PCM_QLS;
	case T_PCM_QLS:
		if (ls == smt->smt_st.tls)
			return;
		XI_ARG.ls = PCM_QLS;
		xpi_op(xi,XCMD_SETLS0+elm_num);
		break;

	case R_PCM_MLS:			/* send MLS */
		ls = T_PCM_MLS;
	case T_PCM_MLS:
		if (ls == smt->smt_st.tls)
			return;
		XI_ARG.ls = PCM_MLS;
		xpi_op(xi,XCMD_SETLS0+elm_num);
		break;

	case R_PCM_HLS:			/* send HLS */
		ls = T_PCM_HLS;
	case T_PCM_HLS:
		if (ls == smt->smt_st.tls)
			return;
		XI_ARG.ls = PCM_HLS;
		xpi_op(xi,XCMD_SETLS0+elm_num);
		break;

	case R_PCM_ILS:			/* send ILS */
		ls  = T_PCM_ILS;
	case T_PCM_ILS:
		if (ls == smt->smt_st.tls)
			return;
		XI_ARG.ls = PCM_ILS;
		xpi_op(xi,XCMD_SETLS0+elm_num);
		break;

	case T_PCM_REP:			/* shut down and just repeat */
		if (ls == smt->smt_st.tls)
			return;
		xpi_op(xi,XCMD_REP);
		xi->xi_state &= ~XI_ST_MAC;
		break;

	case T_PCM_THRU:		/* Put this PHY into the ring. */
		if (ls == smt->smt_st.tls)
			return;
		break;

	case T_PCM_WRAP:		/* Put MAC & PHY in ring alone. */
		if (ls == smt->smt_st.tls)
			return;
		break;

	case T_PCM_LCT:			/* start Link Confidence Test */
		if (ls == smt->smt_st.tls)
			return;
		xpi_op(xi, XCMD_LCT0+elm_num);
		xpi_lemon(smt,xi);
		break;

	case T_PCM_LCTOFF:		/* stop Link Confidence Test */
		xpi_lemoff(smt,xi);	/* update LER counter */
		break;

	case T_PCM_SIG:
		ls = T_PCM_QLS;
		xpi_op(xi, ELM(smt,XCMD_PC_OFF0,XCMD_PC_OFF1));
		break;

#ifdef DEBUG
	default:
		panic("unknown line state");
#endif
	}

	smt->smt_st.tls = ls;
	SMT_LSLOG(smt, ls);
}



/* Get PCM state bits from ELM status
 */
static void
xpi_elmst(struct smt *smt,
	  struct xpi_info *xi,
	  __uint32_t st)
{
/* bits in elm0st or elm1st
 */
#define ELM_PCM_STATE	0x0080
#define ELM_PCM_STATE_MASK 0x0780
#define ELM_SIGNALING	0x0040
#define ELM_LS_FLAG	0x0020
#define ELM_RC_FLAG	0x0010
#define ELM_TC_FLAG	0x0008
#define ELM_BREAK_MASK	0x0007
#define ELM_PC_START	0x0001

	enum pcm_state pcm_state;
	int *np;

	SMTDEBUGPRINT(smt,2,("%sPC%d,st=%x ",
			     PC_LAB(smt), smt->smt_st.pcm_state, (int)st));

	np = SMT1(smt,xi) ? &xi->xi_smt1_n : &xi->xi_smt2_n;
	smt->smt_st.r_val &= (1 << smt->smt_st.n) - 1;
	smt->smt_st.r_val |= ((st & 0xffff0000) >> (16-smt->smt_st.n));
	smt->smt_st.r_val &= (1 << (smt->smt_st.n+*np)) - 1;

	smt->smt_st.flags &= ~(PCM_LS_FLAG
			       | PCM_RC_FLAG
			       | PCM_TC_FLAG);
	if (st & ELM_LS_FLAG)
		smt->smt_st.flags |= PCM_LS_FLAG;
	if (st & ELM_RC_FLAG)
		smt->smt_st.flags |= PCM_RC_FLAG;
	if (st & ELM_TC_FLAG)
		smt->smt_st.flags |= PCM_TC_FLAG;

	pcm_state = (enum pcm_state)((st & ELM_PCM_STATE_MASK)
				     / ELM_PCM_STATE);

	if (st & ELM_SIGNALING) {
		smt->smt_st.flags |= PCM_PHY_BUSY;
	} else if (pcm_state == PCM_ST_NEXT
		   && smt->smt_st.n < 10) {
		smt->smt_st.n += *np;
		*np = 0;
	} else if (pcm_state == PCM_ST_MAINT
		   && smt->smt_st.pcm_state == PCM_ST_OFF) {
		pcm_state = PCM_ST_OFF;
	}

	if (smt->smt_st.pcm_state != PCM_ST_BYPASS) {
		smt_new_st(smt, ((pcm_state == PCM_ST_OFF
				  && PCM_SET(smt, PCM_PC_START))
				 ? PCM_ST_BREAK
				 : pcm_state));

		np = SMT1(smt,xi) ? &xi->xi_elm1_st : &xi->xi_elm2_st;
		st &= ELM_BREAK_MASK;
		if (pcm_state == PCM_ST_BREAK
		    && st != *np) {
			if (st > ELM_PC_START)
				SMT_LSLOG(smt, SMT_LOG_X+st);
			*np = st;
		}
	}
}


/* Get LER and other counters
 *	come here with interrupts off.
 */
static int
xpi_st_get(struct smt *smt1,	/* caller value not used */
	   struct xpi_info *xi)
{
#define NC xi->xi_new_counts
#define PC xi->xi_prev_counts
#define CK_CNT(c) (PC.c != NC.c)
#define UP_CNT0(s,a,c) ((s)->smt_st.a.hi += (NC.c < PC.c), \
			(s)->smt_st.a.lo += (NC.c - PC.c))
#define UP_CNT(s,c) UP_CNT0(s,c,c)
	struct timeval ntime;
	__uint32_t ticks;
	int error = 0;


	CK_IFNET(xi);
	if (xi->xi_state & XI_ST_ZAPPED) {
		/* no work if the board is sick
		 */
		error = EIO;

	} else {
		xpi_wait(xi);
		xpi_op(xi, XCMD_FET_CNT);
		xpi_wait(xi);
		if (xi->xi_hcr.cmd_ack != xi->xi_cmd_id)
			return EIO;
	}

	smt1 = &xi->xi_smt1;
	ntime.tv_sec = lbolt/HZ;
	ntime.tv_usec = (lbolt%HZ)*(USEC_PER_SEC/HZ);

	/* Use the board counter only to increase the resolution beyond
	 *	the system HZ counter.  Time must advance, so fake it
	 *	if necessary.
	 */
	ticks = NC.ticks - PC.ticks;
	ntime.tv_usec += ticks % (USEC_PER_SEC/HZ);
	if (ntime.tv_sec <= xi->xi_time.tv_sec
	    && ntime.tv_usec <= xi->xi_time.tv_usec) {
		ntime.tv_sec = xi->xi_time.tv_sec;
		ntime.tv_usec = xi->xi_time.tv_usec + 100;
		RNDTIMVAL(&ntime);
	}
	xi->xi_time = ntime;

	/* If the board is dead, do no more than pretend some time has
	 * passed so that PCM will stop trying.
	 */
	if (error)
		return error;

	if (IS_SA(xi)) {
		if (0 != (xi->xi_state & XI_ST_LEM0)) {
			xpi_lem_accum(smt1, xi, &xi->xi_lem0,
				      NC.ticks, NC.lem0_ct);
		}
		xpi_elmst(smt1,xi, NC.elm0st);
		UP_CNT0(smt1,eovf,eovf0);
		UP_CNT0(smt1,noise,noise0);
		UP_CNT0(smt1,viol,vio0);

	} else {
		struct smt *smt2 = &xi->xi_smt2;

		if (0 != (xi->xi_state & XI_ST_LEM0)) {
			xpi_lem_accum(smt2, xi, &xi->xi_lem0,
				      NC.ticks, NC.lem0_ct);
		}

		xpi_elmst(smt2,xi, NC.elm0st);
		UP_CNT0(smt2,eovf,eovf0);
		UP_CNT0(smt2,noise,noise0);
		UP_CNT0(smt2,viol,vio0);

		if (0 != (xi->xi_state & XI_ST_LEM1)) {
			xpi_lem_accum(smt1,xi, &xi->xi_lem1,
				      NC.ticks, NC.lem1_ct);
		}

		xpi_elmst(smt1,xi, NC.elm1st);
		UP_CNT0(smt1,eovf,eovf1);
		UP_CNT0(smt1,noise,noise1);
		UP_CNT0(smt1,viol,vio1);
	}
	xi->xi_lem0.ct_cur = NC.lem0_ct;
	xi->xi_lem0.usec_prev = NC.ticks;
	xi->xi_lem1.ct_cur = NC.lem1_ct;
	xi->xi_lem1.usec_prev = NC.ticks;

	if (NC.rngop > NC.rngbroke) {
		if (!PCM_SET(&xi->xi_smt1, PCM_RNGOP)) {
			xi->xi_smt1.smt_st.flags |= PCM_RNGOP;
			xpi_alarm(smt1,xi,SMT_ALARM_RNGOP);
		}
	} else {
		if (PCM_SET(&xi->xi_smt1, PCM_RNGOP)) {
			xi->xi_smt1.smt_st.flags &= ~PCM_RNGOP;
			xpi_alarm(smt1,xi,SMT_ALARM_RNGOP);
		}
	}
	UP_CNT(smt1,rngop);
	UP_CNT(smt1,rngbroke);
	smt1->smt_st.t_neg = NC.t_neg;


	UP_CNT(smt1,pos_dup);
	UP_CNT(smt1,misfrm);
	UP_CNT(smt1,xmtabt);
	UP_CNT(smt1,tkerr);
	UP_CNT(smt1,clm);
	UP_CNT(smt1,bec);
	UP_CNT(smt1,tvxexp);
	UP_CNT(smt1,trtexp);
	UP_CNT(smt1,tkiss);
	UP_CNT(smt1,myclm);
	UP_CNT(smt1,loclm);
	UP_CNT(smt1,hiclm);
	UP_CNT(smt1,mybec);
	UP_CNT(smt1,otrbec);

	UP_CNT(smt1,frame_ct);
	UP_CNT(smt1,tok_ct);
	UP_CNT(smt1,err_ct);
	UP_CNT(smt1,lost_ct);

	UP_CNT(smt1,error);
	UP_CNT(smt1,e);
	UP_CNT(smt1,a);
	UP_CNT(smt1,c);
	UP_CNT(smt1,tot_junk);
	UP_CNT(smt1,longerr);
	UP_CNT(smt1,shorterr);
	if (CK_CNT(rx_ovf)) {
		xi->xi_if.if_ierrors += (NC.rx_ovf-PC.rx_ovf);
		UP_CNT(smt1,rx_ovf);
	}
	if (CK_CNT(tx_under)) {
		DEBUGPRINT(xi,(PF_L "%u TX underrun\n", PF_P(xi),
			       (unsigned)(NC.tx_under - PC.tx_under)));
		xi->xi_if.if_oerrors += (NC.tx_under - PC.tx_under);
		UP_CNT(smt1,tx_under);
	}
	UP_CNT(smt1,dup_mac);

	smt1->smt_st.mac_state = NC.mac_state;

	smt1->smt_st.ring_latency = NC.rnglatency;

	if (CK_CNT(elm_bug)) {
		printf(PF_L "%d ELM programming error(s)\n", PF_P(xi),
		       (int)(NC.elm_bug - PC.elm_bug));
	}

	if (CK_CNT(mac_bug)) {
		printf(PF_L "MAC programming error 0x%x\n", PF_P(xi),
		       (int)NC.mac_bug);
	}

	if (CK_CNT(fsi_bug)) {
		DEBUGPRINT(xi,(PF_L "FSI internal error 0x%x\n", PF_P(xi),
			       (int)NC.fsi_bug));
		if ((NC.fsi_bug >> 16) == 0)
			xi->xi_if.if_oerrors++;
	}

	if (CK_CNT(mac_cnt_ovf)) {
		DEBUGPRINT(xi,(PF_L "MAC %d count overflow(s)\n", PF_P(xi),
			       (int)(NC.mac_cnt_ovf - PC.mac_cnt_ovf)));
	}

	bcopy((void*)&NC, &PC, sizeof(PC));

	return 0;
#undef CK_CNT
#undef UP_CNT
#undef UP_CNT0
#undef NC
#undef PC
}


/* Check a proposed configuration
 *	This only checks special restrictions imposed by this driver
 *	and hardware.  It does not do general sanity checks, which should
 *	be done by the SMT demon.
 */
/* ARGSUSED */
static int				/* 0, EINVAL or some other error # */
xpi_st_set(struct smt *smt,
	   struct xpi_info *xi,
	   struct smt_conf *conf)
{
	int recon;			/* need to reconnect */

	CK_IFNET(xi);

	if (conf->type == SM_DAS	/* require needed DAS hardware */
	    && 0 == (xi->xi_vers & XPI_VERS_PHY2))
		return EINVAL;

	if (conf->type == DM_DAS)
		return EINVAL;		/* not implemented */

	recon = 0;

	/* do the easy stuff first
	 */
	bcopy(&conf->mnfdat,
	      &smt->smt_conf.mnfdat,
	      sizeof(smt->smt_conf.mnfdat));
	smt->smt_conf.debug = conf->debug;

	/* clamp & round TVX for the MAC.  It wants a negative multiple
	 * of 256*BCLKs between 1 and 255.
	 */
	conf->tvx = conf->tvx & ~0xff;
	if (conf->tvx > MIN_TVX)
		conf->tvx = MIN_TVX;
	else if (conf->tvx < MAX_TVX)
		conf->tvx = MAX_TVX;

	/* The MAC wants a multiple of 256 for T_REQ
	 */
	conf->t_req &= ~0xff;
	if (conf->t_req > MIN_T_REQ)
		conf->t_req = MIN_T_REQ;
	else if (conf->t_req < MAX_T_REQ)
		conf->t_req = MAX_T_REQ;

	/* Clamp and round T_Max.  The MAC wants a positive
	 * multiple of 2**16 BCLKs.
	 */
	if (conf->t_max > conf->t_req)
		conf->t_max = conf->t_req;
	conf->t_max &= ~0xffff;
	if (conf->t_max < MAX_T_MAX)
		conf->t_max = MAX_T_MAX;
	if (conf->t_max > MIN_T_MAX)
		conf->t_max = MIN_T_MAX;

	/* do not change T_MIN */
	conf->t_min = smt->smt_conf.t_min;

	if (smt->smt_conf.t_max != conf->t_max
	    || smt->smt_conf.t_req != conf->t_req
	    || smt->smt_conf.tvx != conf->tvx) {
		recon = 1;
		smt->smt_conf.t_max = conf->t_max;
		smt->smt_conf.t_req = conf->t_req;
		smt->smt_conf.tvx = conf->tvx;
	}

	if (smt->smt_conf.ler_cut != conf->ler_cut) {
		if (smt->smt_st.ler2 > smt->smt_conf.ler_cut
		    && smt->smt_st.ler2 <= conf->ler_cut)
			xpi_alarm(smt,xi,SMT_ALARM_LEM);
		smt->smt_conf.ler_cut = conf->ler_cut;
	}

	if (smt->smt_conf.type != conf->type
	    || smt->smt_conf.pc_type != conf->pc_type) {
		recon = 1;
#ifdef XPI_DEBUG
		printf("t_max=%x/%x, t_req=%x/%x, tvx=%x/%x\n",
		       smt->smt_conf.t_max, conf->t_max,
		       smt->smt_conf.t_req, conf->t_req,
		       smt->smt_conf.tvx, conf->tvx);
#endif
		smt->smt_conf.type = conf->type;
		smt->smt_conf.pc_type = conf->pc_type;
	}

	if (smt->smt_conf.ip_pri != conf->ip_pri) {
		recon = 1;
		smt->smt_conf.ip_pri = conf->ip_pri;
	}

	if (recon) {
		/* force the MAC(s) to be reset */
		smt_off(smt);
		if (IS_DASSM(xi)) {
			smt_off(OTHER_SMT(xi,smt));
		}
	}
	return 0;
}



/* drain a fake frame to awaken the SMT deamon
 */
static void
xpi_alarm(struct smt *smt,
	  struct xpi_info *xi,
	  u_long alarm)
{
	struct mbuf *m;
	FDDI_IBUFLLC *fhp;

	CK_IFNET(xi);

	smt = &xi->xi_smt1;		/* use the only MAC we have */

	smt->smt_st.alarm |= alarm;
	m = smt->smt_alarm_mbuf;
	if (0 != m) {
		smt->smt_alarm_mbuf = 0;
		ASSERT(SMT_LEN_ALARM <= MLEN);
		m->m_len = SMT_LEN_ALARM;
		fhp = mtod(m,FDDI_IBUFLLC*);
		bzero(fhp, SMT_LEN_ALARM);
		IF_INITHEADER(fhp, &xi->xi_if, SMT_LEN_ALARM);
		if (RAWIF_DRAINING(&xi->xi_rawif)) {
			drain_input(&xi->xi_rawif, FDDI_PORT_SMT,
				    (caddr_t)&fhp->fbh_fddi.fddi_mac.mac_sa,m);
		} else {
			m_free(m);
		}
	}
}



/* start directed beacon
 */
/* ARGSUSED */
static void
xpi_d_bec(struct smt *smt,
	  struct xpi_info *xi,
	  struct d_bec_frame *info,
	  int len)
{
	CK_IFNET(xi);

	xpi_wait(xi);
	bzero(&XI_ARG.d_bec, sizeof(XI_ARG.d_bec));

	if (len >= D_BEC_SIZE-sizeof(info->una)) {
		bcopy(info, &XI_ARG.d_bec.msg, len);
		XI_ARG.d_bec.len = len;
		xi->xi_state |= XI_ST_D_BEC;

	} else {
		/* Let the MAC beacon normally if the daemon does not want
		 * a special beacon.
		 */
		XI_ARG.d_bec.len = 0;
		xi->xi_state &= ~XI_ST_D_BEC;
	}

	XI_ARG.d_bec.msg.hdr.filler[0] = MAC_BEACON_HDR0;
	XI_ARG.d_bec.msg.hdr.filler[1] = MAC_BEACON_HDR1;
	XI_ARG.d_bec.msg.hdr.mac_bits = MAC_BEACON_HDR2;
	XI_ARG.d_bec.msg.hdr.mac_fc = MAC_FC_BEACON;
	bcopy(&xi->xi_hdr.fddi_mac.mac_sa,
	      &XI_ARG.d_bec.msg.hdr.mac_sa,
	      sizeof(XI_ARG.d_bec.msg.hdr.mac_sa));
	xpi_op(xi, XCMD_D_BEC);
}



/* start PC-Trace
 *	Interrupts must be off.
 */
static void
xpi_trace_req(struct smt *smt,
	      struct xpi_info *xi)
{
	CK_IFNET(xi);
	TPC_RESET(smt);
	SMT_USEC_TIMER(smt, SMT_TRACE_MAX);
	smt->smt_st.flags &= ~PCM_PC_DISABLE;
	smt_new_st(smt, PCM_ST_TRACE);

	xpi_wait(xi);
	xpi_op(xi, ELM(smt,XCMD_PC_TRACE0,XCMD_PC_TRACE1));
}



/* start the clock ticking for PCM
 */
static void
xpi_reset_tpc(struct smt *smt,
	      struct xpi_info *xi)
{
	(void)xpi_st_get(0,xi);

	if (SMT1(smt,xi)) {
		xi->xi_tpc1 = xi->xi_time;
	} else {
		xi->xi_tpc2 = xi->xi_time;
	}
}


/* Decide what time it is
 */
static time_t				/* elapsed time in usec */
xpi_get_tpc(struct smt *smt,
	    struct xpi_info *xi)
{
	struct timeval *tp;
	time_t tpc;

	(void)xpi_st_get(0,xi);

	tp = SMT1(smt,xi) ? &xi->xi_tpc1 : &xi->xi_tpc2;

	tpc = xi->xi_time.tv_sec - tp->tv_sec;
	if (tpc >= 0x7fffffff/USEC_PER_SEC)
		return 0x7fffffff;

	return (tpc * USEC_PER_SEC) + xi->xi_time.tv_usec-tp->tv_usec;
}


/* Clear the DMA structures
 * The board must be shut down before calling here.
 */
static void
xpi_clrdma(struct xpi_info *xi)
{
	int i;
	volatile struct xpi_b2h* b2hp;

	while (xi->xi_in_sml_num != 0) {
		m_freem(xi->xi_in_sml_m[xi->xi_in_sml_h]);
		xi->xi_in_sml_h = (xi->xi_in_sml_h+1) % XPI_MAX_SML;
		xi->xi_in_sml_num--;
	}
	while (xi->xi_in_med_num != 0) {
		m_freem(xi->xi_in_med_m[xi->xi_in_med_h]);
		xi->xi_in_med_h = (xi->xi_in_med_h+1) % XPI_MAX_MED;
		xi->xi_in_med_num--;
	}
	while (xi->xi_in_big_num != 0) {
		m_freem(xi->xi_in_big_m[xi->xi_in_big_h]);
		xi->xi_in_big_h = (xi->xi_in_big_h+1) % XPI_MAX_BIG;
		xi->xi_in_big_num--;
	}

	xi->xi_d2bp = &xi->xi_d2b[0];
	xi->xi_d2bp->hd.start = XPI_D2B_BAD;
#ifdef _NO_UNCACHED_MEM_WAR
	dcache_wb((void*)xi->xi_d2bp, sizeof(*xi->xi_d2bp));
#endif
	xi->xi_d2b_lim = xi->xi_d2bp + XPI_D2B_LEN - (XPI_MAX_DMA_ELEM+1);

	xi->xi_c2bp = &xi->xi_c2b[0];
	xi->xi_c2b_lim = xi->xi_c2bp + XPI_C2B_LEN - 1;
	bzero((void*)xi->xi_c2bp, sizeof(xi->xi_c2b));
	xi->xi_c2b_lim->c2b_op = XPI_C2B_WRAP;
#ifdef _NO_UNCACHED_MEM_WAR
	dcache_wb((void*)xi->xi_c2bp, sizeof(xi->xi_c2b));
#endif

	b2hp = &xi->xi_b2h[0];
	xi->xi_b2hp = b2hp;
	xi->xi_b2h_lim = b2hp+XPI_B2H_LEN;
#ifdef _NO_UNCACHED_MEM_WAR
	b2hp = (volatile struct xpi_b2h*)K1_TO_K0(b2hp);
#endif
	bzero((char*)b2hp, sizeof(xi->xi_b2h));
	xi->xi_b2h_sn = 1;
	for (i = 0; i < XPI_B2H_LEN; i++, b2hp++)
		b2hp->b2h_sn = (i+1+XPI_B2H_LEN*255)%256;
#ifdef _NO_UNCACHED_MEM_WAR
	dki_dcache_wbinval((void*)(b2hp-XPI_B2H_LEN), sizeof(xi->xi_b2h));
#endif

	xi->xi_state |= XI_ST_SLEEP;
}


/* Reset a MAC because we want to shut everything off or because we want
 *	to join the ring.
 */
static void
xpi_macreset(struct xpi_info *xi)
{
	xpi_wait(xi);
	xpi_op(xi, XCMD_MAC_OFF);
	xi->xi_state &= ~XI_ST_MAC;
}


/* initialize the MAC
 *	Interrupts must already be off.
 */
static void
xpi_macinit(struct xpi_info *xi)
{
	xpi_macreset(xi);

	xpi_set_mmode(xi);

	/* install 48-bit source address in prototype header
	 */
	xi->xi_hdr.fddi_mac.filler[0] = MAC_DATA_HDR0;
	xi->xi_hdr.fddi_mac.filler[1] = MAC_DATA_HDR1;
	xi->xi_hdr.fddi_mac.mac_bits = MAC_DATA_HDR2;
	xi->xi_hdr.fddi_mac.mac_fc = MAC_FC_LLC(xi->xi_smt1.smt_conf.ip_pri);
	bcopy(&xi->xi_smt1.smt_st.addr,
	      &xi->xi_hdr.fddi_mac.mac_sa,
	      sizeof(xi->xi_hdr.fddi_mac.mac_sa));
	xi->xi_hdr.fddi_llc.llc_c1 = RFC1042_C1;
	xi->xi_hdr.fddi_llc.llc_c2 = RFC1042_C2;
	xi->xi_hdr.fddi_llc.llc_etype = htons(ETHERTYPE_IP);

	/* send MAC address and other parameters to the board
	 */
	xpi_wait(xi);
	bcopy(&xi->xi_hdr.fddi_mac.mac_sa.b[0],
	      XI_ARG.mac_parms.addr,
	      sizeof(XI_ARG.mac_parms.addr));

	XI_ARG.mac_parms.t_req = xi->xi_smt1.smt_conf.t_req >> 8;
	XI_ARG.mac_parms.r15 = ((xi->xi_smt1.smt_conf.tvx & ~0xff)
				| (xi->xi_smt1.smt_conf.t_max>>16 & 0xff));
	xpi_op(xi, XCMD_MAC_PARMS);
	xi->xi_state |= XI_ST_MAC;
	xi->xi_state &= ~XI_ST_D_BEC;

	(void)xpi_fillin(xi);
}



/* Initialize the LEM and turn on PHY and PCM interrupts
 */
static void
xpi_phyinit(struct smt *smt,
	    struct xpi_info *xi)
{
	smt_clear(smt);

	xpi_setls(smt, xi, T_PCM_SIG);	/* start line state interrupts */
	xpi_wait(xi);
	xpi_op(xi, XCMD_PINT_ARM);

	if (IS_SA(xi) || !SMT1(smt,xi)) {    /* SAS or DAS_DM */
		bzero(&xi->xi_lem0, sizeof(xi->xi_lem0));
		xpi_lem_accum(smt,xi, &xi->xi_lem0, 0, 0);
		xi->xi_state &= ~XI_ST_LEM0;

		xpi_wait(xi);
		xpi_op(xi, XCMD_FET_LS0);
		xpi_wait(xi);
		smt->smt_st.rls = (enum pcm_ls)XI_RES.ls;

		(void)xpi_st_get(0,xi);
		xi->xi_old_noise0 = smt->smt_st.noise.lo;

	} else {			/* smt #2, ELM #2 for DAS-SM */
		bzero(&xi->xi_lem1, sizeof(xi->xi_lem1));
		xpi_lem_accum(smt,xi, &xi->xi_lem1, 0, 0);
		xi->xi_state &= ~XI_ST_LEM1;

		xpi_wait(xi);
		xpi_op(xi, XCMD_FET_LS1);
		xpi_wait(xi);
		smt->smt_st.rls = (enum pcm_ls)XI_RES.ls;

		(void)xpi_st_get(0,xi);
		xi->xi_old_noise1 = smt->smt_st.noise.lo;
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
xpi_pcm_off(struct smt *smt,
	    struct xpi_info *xi)
{
	struct smt *smt2;
	struct xpi_info *xi2;
	u_long bflag;


	CK_IFNET(xi);
	if (IS_SA(xi)) {
		ASSERT(smt == &xi->xi_smt1);
		xi2 = xi;
		smt2 = smt;
	} else {
		xi2 = xi;
		smt2 = OTHER_SMT(xi,smt);
	}

	xpi_lemoff(smt,xi);
	smt->smt_st.flags &= ~(PCM_CF_JOIN | PCM_THRU_FLAG
			       | PCM_PHY_BUSY | PCM_PC_START);

	/* just sit tight while the other side is wait for a response
	 * to trace
	 */
	if (smt2->smt_st.pcm_state == PCM_ST_TRACE
	    && !PCM_SET(smt2, PCM_SELF_TEST)
	    && iff_alive(xi2->xi_if.if_flags)) {
		if (!PCM_SET(smt, PCM_PC_DISABLE))
			xpi_setls(smt, xi, R_PCM_QLS);
		smt_new_st(smt, PCM_ST_OFF);
		return;
	}

	/* Reset PHY A if it had been withheld and this is PHY B
	 * being shut off.  (see CEM Isolated_Actions)
	 */
	if (SMT1(smt,xi) && IS_DASSM(xi)) {
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

	/* If we are dual-attached and THRU or WRAP_AB, then we must WRAP.
	 */
	if (PCM_SET(smt2, PCM_CF_JOIN)) {
		smt2->smt_st.flags &= ~PCM_THRU_FLAG;
		if (iff_alive(xi->xi_if.if_flags)
		    && iff_alive(xi2->xi_if.if_flags)) {
			xpi_setls(smt2,xi2,T_PCM_WRAP);
		}
	} else {
		/* if neither PHY is inserted, shut off the MACs
		 */
		xpi_macreset(xi);
		if (xi != xi2)
			xpi_macreset(xi2);
	}


	/* compute new state for the optical bypass
	 */
	bflag = 0;
	if (iff_alive(xi->xi_if.if_flags)
	    && iff_alive(xi2->xi_if.if_flags)
	    && !PCM_SET(smt,PCM_SELF_TEST)
	    && !PCM_SET(smt2,PCM_SELF_TEST)) {
		bflag = PCM_INS_BYPASS;
	}

	/* Fiddle with the optical bypasses on the two boards of a
	 * dual-MAC station.
	 * Note that this function is not the only code that changes
	 * the optical bypass.
	 */
	if (bflag != (smt->smt_st.flags & PCM_INS_BYPASS)) {
		xpi_wait(xi);
		xpi_op(xi,
		       ((bflag & PCM_INS_BYPASS) ? XCMD_BYPASS_INS
			: XCMD_BYPASS_REM));
		xi->xi_state |= XI_ST_BCHANGE;
		smt->smt_st.flags = ((smt->smt_st.flags & ~PCM_INS_BYPASS)
				     | bflag);
	}
	if (bflag != (smt2->smt_st.flags & PCM_INS_BYPASS)) {
		xpi_wait(xi2);
		xpi_op(xi2,
		       ((bflag & PCM_INS_BYPASS) ? XCMD_BYPASS_INS
			: XCMD_BYPASS_REM));
		xi2->xi_state |= XI_ST_BCHANGE;
		smt2->smt_st.flags = ((smt2->smt_st.flags & ~PCM_INS_BYPASS)
				      | bflag);
	}

	/* If needed, wait for the bypass to settle.
	 * Note that this function is not the only code that changes
	 * the optical bypass.
	 */
	if (0 != (xi->xi_state & XI_ST_BCHANGE)) {
		xi->xi_state &= ~XI_ST_BCHANGE;
		xi2->xi_state &= ~XI_ST_BCHANGE;
		if (bflag & PCM_INS_BYPASS) {
			SMTDEBUGPRINT(smt,0, (PF_L "inserted\n", PF_P(xi)));
			xpi_phyinit(smt,xi);
			if (!IS_SA(xi)) {
				xpi_phyinit(smt2,xi2);
			}
		} else {
			SMTDEBUGPRINT(smt,0, (PF_L "bypassed\n", PF_P(xi)));
		}
		/* send QLS to meet the TD_Min requirement */
		xpi_setls(smt,xi,R_PCM_QLS);
		xpi_setls(smt2,xi2,R_PCM_QLS);

		/* Wait for the bypass to settle, and be sure
		 * something wakes up.
		 */
		smt_new_st(smt, PCM_ST_BYPASS);
		TPC_RESET(smt);
		if (smt != smt2) {
			smt_new_st(smt2,PCM_ST_BYPASS);
			TPC_RESET(smt2);
		}
		return;
	}

	if (!PCM_SET(smt, PCM_PC_DISABLE)) {
		if (PCM_ST_BYPASS == smt->smt_st.pcm_state) {
			SMT_USEC_TIMER(smt,SMT_I_MAX);
			if (smt != smt2)
				SMT_USEC_TIMER(smt2,SMT_I_MAX);
		} else {
			if (!(bflag & PCM_INS_BYPASS)) {
				/* If dead, stay dead.
				 * Put the ELMs of a DAS in repeat-mode in
				 * case there is no optical bypass and
				 * dual-attached.
				 */
				if (!IS_SA(xi)) {
					xpi_setls(smt,xi,T_PCM_REP);
					xpi_setls(smt2,xi2,T_PCM_REP);
				}
				smt_new_st(smt, PCM_ST_OFF);
			} else {
				/* let the board start */
				xpi_wait(xi);
				xpi_op(xi,ELM(smt,XCMD_PC_OFF0,XCMD_PC_OFF1));
				smt->smt_st.tls = T_PCM_QLS;
				smt->smt_st.flags |= PCM_PC_START;
				TPC_RESET(smt);
				SMT_USEC_TIMER(smt,SMT_TB_MIN);
			}
		}
	}
}



/* tell the ELM to signal some bits
 */
static void
xpi_setvec(struct smt *smt,
	   struct xpi_info *xi,
	   int n)
{
	smt->smt_st.flags |= PCM_PHY_BUSY;
	if (SMT1(smt,xi)) {
		xi->xi_smt1_n = n;
	} else {
		xi->xi_smt2_n = n;
	}
	xpi_wait(xi);
	XI_ARG.vec_n = (((smt->smt_st.t_val << (16-smt->smt_st.n))
			 & 0xffff0000)
			| (n-1));
	xpi_op(xi, ELM(smt,XCMD_SETVEC0,XCMD_SETVEC1));

	if (smt->smt_st.n == 0) {
		xpi_wait(xi);
		xpi_op(xi, ELM(smt,XCMD_PC_START0,XCMD_PC_START1));
	}
}


/* Start Link Confidence Test
 *	Interrupts must be off
 */
static void
xpi_lct_on(struct smt *smt,
	   struct xpi_info *xi)
{
	xpi_setls(smt,xi,T_PCM_LCT);
}


/* Stop LCT and return results
 *	Interrupts must be off
 */
static int				/* 1==LCT failed */
xpi_lct_off(struct smt *smt,
	    struct xpi_info *xi)
{
	xpi_setls(smt, xi,T_PCM_LCTOFF);

	return (smt->smt_st.ler2	/* fail if LER bad */
		<= smt->smt_conf.ler_cut);
}


/* Tell the ELM to finish the PCM signaling
 *	Interrupts must be off
 */
static void
xpi_join(struct smt *smt,
	 struct xpi_info *xi)
{
	CK_IFNET(xi);
	smt->smt_st.flags |= PCM_PHY_BUSY;
	xpi_wait(xi);
	xpi_op(xi, ELM(smt,XCMD_JOIN0,XCMD_JOIN1));
}



/* Join the ring
 *	Interrupts must be off
 *	This implements much of the Configuration Element Management state
 *	machine.
 */
static void
xpi_cfm(struct smt *smt,
	 struct xpi_info *xi)
{
	struct xpi_info *xi2;
	struct smt *smt2;
	int st;


	CK_IFNET(xi);
	if (PCM_SET(smt, PCM_CF_JOIN))
		return;

	if (IS_SA(xi)) {
		ASSERT(smt == &xi->xi_smt1);
		xi2 = xi;
		smt2 = smt;
	} else {
		ASSERT(IS_DASSM(xi));
		xi2 = xi;
		smt2 = OTHER_SMT(xi,smt);
	}

	/* tell the MAC about itself */
	if (!(xi->xi_state & XI_ST_MAC))
		xpi_macinit(xi);
	if (!(xi2->xi_state & XI_ST_MAC))
		xpi_macinit(xi2);

	/* stop directed beacon */
	if (xi->xi_state & XI_ST_D_BEC)
		xpi_d_bec(smt, xi, 0, 0);
	if (xi2->xi_state & XI_ST_D_BEC)
		xpi_d_bec(smt2, xi2, 0, 0);


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
	if (SMT1(smt,xi) && IS_DASSM(xi)) {
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
		xpi_setls(smt, xi, T_PCM_WRAP);
		smt_off(smt2);
	} else if (PCM_SET(smt2, PCM_CF_JOIN)
		   && (IS_SA(xi)
		       || (!PC_MODE_T(smt) && !PC_MODE_T(smt2)))) {
		smt->smt_st.flags |= PCM_THRU_FLAG;
		xpi_setls(smt, xi, T_PCM_THRU);
		smt2->smt_st.flags |= PCM_THRU_FLAG;
		xpi_setls(smt2, xi, T_PCM_THRU);
	} else {
		xpi_setls(smt, xi, T_PCM_WRAP);
	}

	xpi_lemon(smt,xi);

	if (xi->xi_state & XI_ST_ARPWHO) {
		xi->xi_state &= ~XI_ST_ARPWHO;
		ARP_WHOHAS(&xi->xi_ac, &xi->xi_ac.ac_ipaddr);
	}
}


/* help propagate TRACE upstream by finding the other PCM state machine
 */
static struct smt*
xpi_osmt(struct smt *smt,
	  struct xpi_info *xi)
{
	if (IS_DASSM(xi))
		return OTHER_SMT(xi,smt);
	return smt;
}



/* send a word to the board through the GIO bus slave interface
 */
static void
xpi_nibble(volatile __uint32_t *gio,
	   __uint32_t w)
{
	int i;

	for (i = 0; i < 8; i++) {
		/* Send a nibble.
		 */
		*gio = (((w * XPI_GIO_FLAG0) & XPI_GIO_FLAG_MASK)
			| XPI_GIO_H2B_INTR);
		w >>= 4;
		DELAYBUS(DELAY_NIBBLE);
	}
}


/* signal an address and an operation number to the board
 */
static int				/* 0=no news from the board */
xpi_signal(struct xpi_info *xi,
	   int dlay,			/* try this long in usec */
	   u_short op)			/* send this to the board */
{
#ifdef _NO_UNCACHED_MEM_WAR
	long ucmem_previous;

#endif
	/* reset the board, if asked
	 */
	if (0 != (op & XPI_SIGNAL_RESET)) {
		*xi->xi_gio = XPI_GIO_RESET;

		/* Resetting the board turns things off.
		 * So mark everything off.
		 */
#if defined(IP20) || defined(IP22) || defined(IP26) || defined(IP28)
		setgioconfig(xi->xi_slot,
			     GIO64_ARB_EXP0_RT | GIO64_ARB_EXP0_MST);
#endif
		bzero(&xi->xi_hcw, sizeof(xi->xi_hcw));
#ifdef _NO_UNCACHED_MEM_WAR
		dcache_wb(&xi->xi_hcw, sizeof(xi->xi_hcw));
#endif

		xpi_clrdma(xi);
		bzero(xi->xi_cam, sizeof(xi->xi_cam));

		xi->xi_state &= ~(XI_ST_LEM0 | XI_ST_LEM1);

		if (IS_SA(xi)) {
			xi->xi_smt1.smt_st.tls = R_PCM_QLS;
			xi->xi_smt2.smt_st.tls = R_PCM_QLS;
		} else {
			xi->xi_smt1.smt_st.tls = T_PCM_REP;
			xi->xi_smt2.smt_st.tls = T_PCM_REP;
		}
		xi->xi_state &= ~XI_ST_MAC;

		xi->xi_smt1.smt_st.flags &= (~(PCM_INS_BYPASS|PCM_HAVE_BYPASS)
					     & PCM_SAVE_FLAGS);
		xi->xi_smt2.smt_st.flags &= (~(PCM_INS_BYPASS|PCM_HAVE_BYPASS)
					     & PCM_SAVE_FLAGS);
		xi->xi_state |= XI_ST_BCHANGE;
		bzero(&xi->xi_prev_counts, sizeof(xi->xi_prev_counts));
		bzero(&xi->xi_dbg, sizeof(xi->xi_dbg));

		DELAYBUS(XPI_GIO_RESET_DELAY);
		op &= ~XPI_SIGNAL_RESET;
		*xi->xi_gio = 0;
	}

	xi->xi_hcw.cmd = XCMD_NOP;
	xi->xi_hcw.cmd_id = ++xi->xi_cmd_id;
	xi->xi_cmd_line = __LINE__;
#ifdef _NO_UNCACHED_MEM_WAR
	dcache_wb(&xi->xi_hcw, sizeof(xi->xi_hcw));
	ucmem_previous= ip26_enable_ucmem();
	bzero(&xi->xi_hcr,sizeof(xi->xi_hcr));
	ip26_return_ucmem(ucmem_previous);
#else
	bzero(&xi->xi_hcr,sizeof(xi->xi_hcr));
#endif

	for (;;) {
		xpi_nibble(xi->xi_gio, XPI_GIO_DL_PREAMBLE);
		xpi_nibble(xi->xi_gio, op);
		xpi_nibble(xi->xi_gio, kvtophys(&xi->xi_hcw));
		xpi_nibble(xi->xi_gio, ~(op ^ kvtophys(&xi->xi_hcw)));
		DELAYBUS(START_CHECK);
		if (xi->xi_hcr.sign == XPI_SIGN) {
			/* It is alive.
			 * Ensure that the board always, not just sometimes,
			 * has an extra interrupt.
			 */
			*xi->xi_gio = XPI_GIO_H2B_INTR;
			xi->xi_cmd_line = __LINE__;
			return 1;
		}
		dlay -= DELAY_NIBBLE*4*8+START_CHECK;
		if (dlay < 0)
			return 0;
	}
}


/* tell the board about the host communication area
 */
static int				/* 0=bad board */
xpi_run(struct xpi_info *xi)
{
	inventory_t *pinv;
	long l;

	for (;;) {
		struct mbuf *m;
		IF_DEQUEUE_NOLOCK(&xi->xi_if.if_snd, m);
		if (!m)
			break;
		m_freem(m);
	}
	m_freem(xi->xi_in);
	xi->xi_in = 0;

	if (!xpi_signal(xi, ((xi->xi_if.if_flags & IFF_DEBUG)
			     ? DELAY_RESET_DBG
			     : DELAY_RESET),
			XPI_SIGNAL_RESET)) {
		xi->xi_state |= XI_ST_ZAPPED;
		printf(PF_L "failed to reset\n", PF_P(xi));
		return 0;
	}

	/* make the new firmware revision number visible to `hinv`
	 */
	if (xi->xi_vers != xi->xi_hcr.vers) {
		xi->xi_vers = xi->xi_hcr.vers;
		pinv = find_inventory(0, INV_NETWORK,
				      INV_NET_FDDI,
				      INV_FDDI_XPI,
				      xi->xi_if.if_unit, -1);
		if (pinv) {
#ifdef EVEREST
			pinv->inv_state &= (XPI_VERS_DANG
					    |XPI_VERS_SLOT_M
					    |XPI_VERS_ADAP_M);
			pinv->inv_state |= ((xi->xi_vers & XPI_VERS_M)
					    /XPI_VERS_MEZ_S);
#else
			pinv->inv_state = xi->xi_vers & XPI_VERS_M;
#endif
			pinv->inv_state |= xi->xi_vers & (XPI_VERS_PHY2
							   |XPI_VERS_BYPASS);
		}
	}

	/* Tell the board how to do its work, regardless of its version,
	 * to ensure it has a good place from which to fetch C2B commands.
	 */
	xpi_wait(xi);
	XI_ARG.init.flags = (xpi_lat_void ? XPI_LAT_VOID : 0);
	if (xpi_cksum & 1)
		XI_ARG.init.flags |= XPI_IN_CKSUM;
	if (xpi_zapring)
		XI_ARG.init.flags |= XPI_ZAPRING;
#ifdef EVEREST
	if (io4_flush_cache != 0 && io4ia_war)
		XI_ARG.init.flags |= XPI_IO4IA_WAR;
#endif

	XI_ARG.init.b2h_buf = svirtophys(&xi->xi_b2h[0]);
	XI_ARG.init.b2h_len = XPI_B2H_LEN;

	XI_ARG.init.max_bufs = xpi_num_big + xpi_num_med + xpi_num_sml;

	XI_ARG.init.d2b_buf = svirtophys(&xi->xi_d2b[0]);
	XI_ARG.init.d2b_buf_lim = svirtophys(xi->xi_d2b_lim);

	XI_ARG.init.c2b_buf = svirtophys(&xi->xi_c2b[0]);

	if (xpi_low_latency) {
		XI_ARG.init.c2b_dma_delay = (USEC_PER_SEC/FRAME_SEC)/2;
		XI_ARG.init.int_delay = 1;
		XI_ARG.init.max_host_work = 0;
		XI_ARG.init.c2b_sleep = 1;
	} else {
		XI_ARG.init.c2b_dma_delay = xpi_c2b_dma_delay;
		XI_ARG.init.int_delay = BTODELAY(xpi_byte_int_delay);
		XI_ARG.init.max_host_work = xpi_max_host_work;
		XI_ARG.init.c2b_sleep = xpi_c2b_sleep;
	}

	XI_ARG.init.ctptr = svirtophys(&xi->xi_new_counts);

	l = xpi_early_release;
	if (l <= 0) {
		l = 1024*1024;
	} else if (l == 1) {
		l = (60*1024)/3;
	}
	XI_ARG.init.rel_tok = l;

	XI_ARG.init.sml_size = XPI_SML_SIZE;
	XI_ARG.init.pad_in_dma = sizeof(FDDI_IBUF);

	xpi_op(xi, XCMD_INIT);
	xpi_wait(xi);

	if (0 != (XPI_VERS_CKSUM & xi->xi_vers)) {
		printf(PF_L "bad firmware checksum\n", PF_P(xi));
		xi->xi_state |= XI_ST_ZAPPED;
		return 0;
	}
	if ((xi->xi_vers & XPI_VERS_FAM_M) != XPI_VERS_FAM
	    || (xi->xi_vers & XPI_VERS_M) < XPI_VERS_MIN) {
		printf(PF_L "firmware too old or new;"
			    " smtconfig will update it\n", PF_P(xi));
		xi->xi_state |= XI_ST_ZAPPED;
		return 0;
	}
	xi->xi_state &= ~XI_ST_ZAPPED;

	/* See if the board is really alive.
	 */
	return (xi->xi_hcr.cmd_ack == xi->xi_cmd_id);
}


/* reset a board because we do not want to play any more at all, because
 *	a chip just paniced, or because we need to change the MAC address.
 *
 *	If we are playing dual-attachment, then both PHYs must be reset.
 *	Interrupts must already be off.
 */
static void
xpi_reset(struct xpi_info *xi,
	  int frc)
{
	__uint32_t cksum;
	u_char das;

	CK_IFNET(xi);

	xi->xi_state &= ~XI_ST_WHINE;	/* allow more complaints */

	/* Reset the board if turning it off.
	 *	See if it is alive if turning it on, and only reset it if
	 *	dead.
	 */
	if ((xi->xi_state & XI_ST_ZAPPED)
	    || !iff_alive(xi->xi_if.if_flags)) {
		frc = 1;

	} else if (!frc) {
		xpi_wait(xi);
		if (xi->xi_hcr.cmd_ack == xi->xi_cmd_id) {
			xpi_op(xi, XCMD_WAKEUP);
			xpi_wait(xi);
		}
		frc = (xi->xi_hcr.cmd_ack != xi->xi_cmd_id);
	}
	if (frc && !xpi_run(xi)) {
		xi->xi_if.if_flags &= ~IFF_ALIVE;
		if_down(&xi->xi_if);
		return;
	}

	/* get the MAC address
	 */
	xpi_wait(xi);
	XI_ARG.dwn.addr = XPI_MAC_ADDR;
	XI_ARG.dwn.cnt = XPI_MAC_SIZE/4;
	xpi_op(xi, XCMD_FET);
	xpi_wait(xi);
	if (xi->xi_hcr.cmd_ack != xi->xi_cmd_id) {
		printf(PF_L "failed to get MAC address\n", PF_P(xi));
		xi->xi_if.if_flags &= ~IFF_ALIVE;
		if_down(&xi->xi_if);
		return;
	}
	XPI_MAC_SUM(cksum,XI_RES.dwn);
	if (cksum != 0) {
		printf(PF_L "bad MAC address checksum\n", PF_P(xi));
		xi->xi_if.if_flags &= ~IFF_ALIVE;
		if_down(&xi->xi_if);
	} else {
		bcopy((void*)XI_RES.dwn, &xi->xi_macaddr[0],
		      sizeof(xi->xi_macaddr));
		if (0 == (xi->xi_state & XI_ST_ADDRSET))
			xpi_set_addr(xi, xi->xi_macaddr);
		if (xi->xi_macaddr[0] != SGI_MAC_SA0
		    || xi->xi_macaddr[1] != SGI_MAC_SA1
		    || xi->xi_macaddr[2] != SGI_MAC_SA2) {
			printf(PF_L "bad MAC address %x:%x:%x:%x:%x:%x\n",
			       PF_P(xi),
			       xi->xi_macaddr[0], xi->xi_macaddr[1],
			       xi->xi_macaddr[2], xi->xi_macaddr[3],
			       xi->xi_macaddr[4], xi->xi_macaddr[5]);
			xi->xi_if.if_flags &= ~IFF_ALIVE;
			if_down(&xi->xi_if);
		}
	}

	/* Figure out the configuration.
	 */
	xi->xi_smt1.smt_conf.debug = smt_debug_lvl;
	xi->xi_smt2.smt_conf.debug = smt_debug_lvl;

	if (0 != (xi->xi_vers & XPI_VERS_BYPASS)) {
		xi->xi_smt1.smt_st.flags |= PCM_HAVE_BYPASS;
		xi->xi_smt2.smt_st.flags |= PCM_HAVE_BYPASS;
	} else {
		xi->xi_smt1.smt_st.flags &= ~PCM_HAVE_BYPASS;
		xi->xi_smt2.smt_st.flags &= ~PCM_HAVE_BYPASS;
	}

	xi->xi_smt1.smt_conf.pcm_tgt = PCM_CMT;
	xi->xi_smt2.smt_conf.pcm_tgt = PCM_CMT;

	das = (0 != (xi->xi_vers & XPI_VERS_PHY2));
	if (das) {
		xi->xi_smt1.smt_conf.type = SM_DAS;
		xi->xi_smt2.smt_conf.type = SM_DAS;
		xi->xi_smt1.smt_conf.pc_type = PC_B;
		xi->xi_smt2.smt_conf.pc_type = PC_A;
	} else {
		xi->xi_smt1.smt_conf.type = SAS;
		xi->xi_smt1.smt_conf.pc_type = PC_S;
	}


	xi->xi_if.if_flags |= IFF_MULTICAST;

	/* Reset the MAC, send QLS, and restart things.
	 */
	if (IS_DASSM(xi)) {
		smt_off(&xi->xi_smt2);
	}
	smt_off(&xi->xi_smt1);

	if (iff_dead(xi->xi_if.if_flags)) {
		if_down(&xi->xi_if);
	}
}



/* initialize interface
 */
static void
xpi_start(struct xpi_info *xi,
	  int flags)
{
	int dflags;

	CK_IFNET(xi);

	if (flags & IFF_UP)
		flags |= IFF_ALIVE;
	else
		flags &= ~IFF_ALIVE;
	if (xpi_cksum & 2)
		flags |= IFF_CKSUM;
	else
		flags &= ~IFF_CKSUM;

	if (xi->xi_if.in_ifaddr == 0)
		flags &= ~IFF_ALIVE;	/* dead if no address */

	dflags = flags ^ xi->xi_if.if_flags;
	xi->xi_if.if_flags = flags;	/* set here to allow overrides */
	if (0 != (dflags & IFF_ALIVE)) {
		xi->xi_state |= XI_ST_ARPWHO;
		xpi_reset(xi,1);
		if (iff_alive(xi->xi_if.if_flags)) {
			moto_pcm(&xi->xi_smt1);	/* start joining ring */
			if (IS_DASSM(xi))
				moto_pcm(&xi->xi_smt2);
		}
	}

	if (0 != (dflags & IFF_PROMISC))
		xpi_set_mmode(xi);

	if (xpi_mtu > 128 && xpi_mtu < FDDIMTU)
		xi->xi_if.if_mtu = xpi_mtu;
	else
		xi->xi_if.if_mtu = FDDIMTU;

	xi->xi_if.if_snd.ifq_maxlen = XPI_MAX_OUTQ;
}


#ifdef DODROWN
/* take a breath after drowning
 */
static void
xpi_breath(struct xpi_info *xi)
{
	IFNET_LOCK(&xi->xi_if);

	ASSERT(xi->xi_if.if_name == xpi_name);

	if (xi->xi_state & XI_ST_DROWN) {
		xi->xi_state &= ~XI_ST_DROWN;
		METER(xi->xi_dbg.drown_off++);
	}
	xpi_b2h(xi, XI_ST_PSENABLED);
	IFNET_UNLOCK(&xi->xi_if);
}


/* start holding our breath */
static void
xpi_drown(struct xpi_info *xi)
{
#define DROWN_TICKS ((HZ*2)/(FRAME_SEC/XPI_MAX_SML))
#if DROWN_TICKS == 0
#undef DROWN_TICKS
#define DROWN_TICKS 1
#endif

	if (!(xi->xi_state & XI_ST_DROWN)) {
		xi->xi_state |= XI_ST_DROWN;
		(void) dtimeout(xpi_breath, xi, DROWN_TICKS, plbase, cpuid());
		METER(xi->xi_dbg.drown_on++);
	}
#undef DROWN_TICKS
}
#endif /* DODROWN */


/* Process an ioctl request.
 */
static int				/* return 0 or error number */
xpi_ioctl(struct ifnet *ifp,
	  int cmd,
	  void *data)
{
#define xi ((struct xpi_info*)ifp)
	int flags;
	int error = 0;


	ASSERT(xi->xi_if.if_name == xpi_name);
	CK_IFNET(xi);

	switch (cmd) {
	case SIOCAIFADDR: {		/* Add interface alias address */
		struct sockaddr *addr = _IFADDR_SA(data);
#ifdef INET6
		/*
		 * arp is only for v4.  So we only call arpwhohas if there is
		 * an ipv4 addr.
		 */
		if (ifp->in_ifaddr != 0)
			arprequest(&xi->xi_ac, 
				&((struct sockaddr_in*)addr)->sin_addr,
				&((struct sockaddr_in*)addr)->sin_addr,
				(&xi->xi_ac)->ac_enaddr);
#else
		arprequest(&xi->xi_ac, 
			&((struct sockaddr_in*)addr)->sin_addr,
			&((struct sockaddr_in*)addr)->sin_addr,
			(&xi->xi_ac)->ac_enaddr);
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
			xi->xi_ac.ac_ipaddr = ((struct sockaddr_in*)addr
					       )->sin_addr;
			xi->xi_if.if_flags &= ~IFF_RUNNING;
			xpi_start(xi, xi->xi_if.if_flags|IFF_UP);
			break;

		case AF_RAW:
			/* It is not safe to change the address while the
			 * board is alive.
			 */
			if (!iff_dead(xi->xi_if.if_flags)) {
				error = EINVAL;
			} else {
				xi->xi_state |= XI_ST_ADDRSET;
				xpi_set_addr(xi, (u_char*)addr->sa_data);
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
			if (xi->xi_if.in_ifaddr == 0)
				return EINVAL;	/* quit if no address */
			xpi_start(xi, flags);
		} else {
			flags &= ~IFF_RUNNING;
			xi->xi_if.if_flags = flags;
			xpi_reset(xi,iff_dead(flags));
		}
		if (0 != (xi->xi_if.if_flags & IFF_DEBUG)) {
			xi->xi_smt1.smt_st.flags |= PCM_DEBUG;
			xi->xi_smt2.smt_st.flags |= PCM_DEBUG;
		} else {
			xi->xi_smt1.smt_st.flags &= ~PCM_DEBUG;
			xi->xi_smt2.smt_st.flags &= ~PCM_DEBUG;
		}
		if (0 != ((flags ^ xi->xi_if.if_flags) & IFF_ALIVE)) {
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
					xi->xi_if.if_flags |= IFF_ALLMULTI;
				} else {
					xi->xi_if.if_flags &= ~IFF_ALLMULTI;
				}
				xpi_set_mmode(xi);
			} else {
				bitswapcopy(MKEY->mk_dhost, MKEY->mk_dhost,
					    sizeof(MKEY->mk_dhost));
				if (SIOCADDMULTI == cmd) {
					error = xpi_add_da(xi, MKEY, 1);
				} else {
					error = xpi_del_da(xi, MKEY, 1);
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
				error = xpi_add_da(xi, &key,
						   FDDI_ISGROUP_SW(a));
			} else {
				error = xpi_del_da(xi, &key,
						   FDDI_ISGROUP_SW(a));
			}
		}
#undef SF
		} break;

	case SIOC_XPI_STO:
#define DWN ((struct xpi_dwn*)data)
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		if (!iff_dead(xi->xi_if.if_flags)
		    || DWN->cnt == 0
		    || DWN->cnt > XPI_DWN_LEN)
			return EINVAL;
		xpi_wait(xi);
		bcopy(DWN, &XI_ARG.dwn, sizeof(XI_ARG.dwn));
		xpi_op(xi, XCMD_STO);
		xi->xi_state |= XI_ST_ZAPPED;
		/* The first request to store is not finished until the board
		 *	has copied all of EEPROM to SRAM.
		 */
		xpi_wait_usec(xi,DELAY_RESET,__LINE__);
		if (xi->xi_hcr.cmd_ack != xi->xi_cmd_id)
			error = EIO;
		break;

	case SIOC_XPI_EXEC:
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		if (!iff_dead(xi->xi_if.if_flags))
			return EINVAL;
		xpi_wait(xi);
		if (xi->xi_hcr.cmd_ack != xi->xi_cmd_id)
			error = EIO;	/* continue as we complain */
		bcopy(DWN, &XI_ARG.dwn, sizeof(XI_ARG.dwn));
		xi->xi_state |= XI_ST_ZAPPED;
		xpi_op(xi, XCMD_EXEC);
		xpi_wait(xi);
		if (xi->xi_hcr.cmd_ack != xi->xi_cmd_id)
			error = EIO;
		break;

	case SIOC_XPI_FET:
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		if (DWN->cnt == 0
		    || DWN->cnt > XPI_DWN_LEN)
			return EINVAL;
		xpi_wait(xi);
		bcopy(DWN, &XI_ARG.dwn, sizeof(XI_ARG.dwn));
		xpi_op(xi, XCMD_FET);
		xpi_wait(xi);
		if (xi->xi_hcr.cmd_ack != xi->xi_cmd_id)
			error = EIO;
		bcopy((void*)&XI_RES.dwn, &DWN->val, DWN->cnt*4);
#undef DWN
		break;

	case SIOC_XPI_VERS:
#define VERS ((struct xpi_vers*)data)
		/* check that the board is alive
		 */
		xpi_wait(xi);
		VERS->vers = xi->xi_vers;
#ifdef EVEREST
		VERS->type = ((xi->xi_state & XI_ST_BAD_DANG)
			      ? XPI_TYPE_MEZ_S
			      : XPI_TYPE_MEZ_D);
#else
		VERS->type = XPI_TYPE_LC;
#endif
		break;

	case SIOC_XPI_SIGNAL:
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		if (!iff_dead(xi->xi_if.if_flags)
		    || *(__uint32_t*)data > 0xffff)
			return EINVAL;
		if (!xpi_signal(xi, DELAY_SIGNAL, *(__uint32_t*)data))
			error = EIO;
		break;

	default:
		error = smt_ioctl(&xi->xi_if, &xi->xi_smt1, &xi->xi_smt2,
				  cmd, (struct smt_sioc*)data);
		break;
	}

	return error;
#undef xi
}



/* Ensure there is room in front of a packet
 */
static struct mbuf*
xpi_mbuf_pad(struct xpi_info *xi,
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
			METER(xi->xi_dbg.out_alloc++);
			m1 = m_get(M_DONTWAIT, MT_DATA);
			if (!m1) {
				m_freem(m0);
				xi->xi_if.if_odrops++;
				IF_DROP(&xi->xi_if.if_snd);
				return 0;
			}
			m1->m_len = padlen;
			m1->m_next = m0;
			m0 = m1;
		}
	}

	p = mtod(m0, caddr_t);
	if (m0->m_len < minlen
	    || M_HASCL(m0)
	    || !OUT_ALIGNED(p)) {
		METER(xi->xi_dbg.out_pullup++);
		m0 = m_pullup(m0, minlen);
		if (!m0) {
			xi->xi_if.if_odrops++;
			IF_DROP(&xi->xi_if.if_snd);
			return 0;
		}
	}
	return m0;
}


/* FDDI output.
 *	If the destination is this machine or broadcast, send the packet
 *	to the loop-back device.  We cannot talk to ourself.
 */
static int				/* 0 or error # */
xpi_output(struct ifnet *ifp,
	   struct mbuf *m0,
	   struct sockaddr *dst,
	   struct rtentry *rte)
{
#define xi ((struct xpi_info*)ifp)
	struct mbuf *m1, *m2;
	struct mbuf *mloop = 0;
	struct fddi *fp;
	int blen1, blen2, blen3;
	int inx;
	int offsum = 0;
	int startsum = 0;
	u_long cksum = 1;
	volatile union xpi_d2b *d2bp, *d2bp0;
	int error = 0;


	ASSERT(xi->xi_if.if_name == xpi_name);
	CK_IFNET(xi);

	if (IF_QFULL(&xi->xi_if.if_snd)) {
		xpi_b2h(xi, 0);		/* try to empty the queue */
		if (IF_QFULL(&xi->xi_if.if_snd)) {
			m_freem(m0);
			xi->xi_if.if_odrops++;
			IF_DROP(&xi->xi_if.if_snd);
			return ((xi->xi_smt1.smt_st.mac_state == MAC_ACTIVE)
				? ENOBUFS : ENETDOWN);
		}
	}

	switch (dst->sa_family) {
	case AF_INET:
		/* get room for the FDDI headers; use what we were
		 *	given if possible.
		 */
		if (!M_HASCL(m0)
		    && m0->m_off >= MMINOFF+sizeof(*fp)
		    && (fp = mtod(m0, struct fddi*), OUT_ALIGNED(fp))) {
			ASSERT(m0->m_off <= MSIZE);
			m1 = 0;
			--fp;
		} else {
			METER(xi->xi_dbg.out_alloc++);
			m1 = m_get(M_DONTWAIT, MT_DATA);
			if (0 == m1) {
				m_freem(m0);
				xi->xi_if.if_odrops++;
				IF_DROP(&xi->xi_if.if_snd);
				return ENOBUFS;
			}
			fp = mtod(m1, struct fddi*);
			m1->m_len = sizeof(*fp);
		}

		bcopy(&xi->xi_hdr, fp, sizeof(*fp));
		if (!arpresolve(&xi->xi_ac, rte, m0,
				   &((struct sockaddr_in *)dst)->sin_addr,
				   (u_char*)&fp->fddi_mac.mac_da, &error)) {
			m_freem(m1);
			return error;	/* just wait if not yet resolved */
		}

		/* Let the board compute the TCP or UDP checksum.
		 * Start the checksum with the checksum of the psuedo-header
		 * and everthing else in the UDP or TCP header up to but
		 * not including the checksum.
		 */
		if (m0->m_flags & M_CKSUMMED) {
			struct ip *ip;
			uint hlen;

			/* Find the UDP or TCP header after IP header, and
			 * the checksum within.
			 */
			ip = mtod(m0, struct ip*);
			hlen = ip->ip_hl<<2;
			startsum = sizeof(*fp)+hlen;
again:
			if (&ip->ip_p >= mtod(m0,u_char*)+m0->m_len) {
				printf(PF_L "bad output checksum offset\n",
				       PF_P(xi));
				m_freem(m1);
				m_freem(m0);
				return EPROTONOSUPPORT;
			}
			switch (ip->ip_p) {
			case IPPROTO_TCP:
				METER(xi->xi_dbg.out_cksum_tcp++);
				offsum = startsum+8*2;
				break;

			case IPPROTO_UDP:
				METER(xi->xi_dbg.out_cksum_udp++);
				offsum = startsum+3*2;
				break;

			case IPPROTO_ENCAP:
				METER(xi->xi_dbg.out_cksum_encap++);
				startsum += hlen;
				ip = (struct ip*)((char*)ip+hlen);
				hlen = ip->ip_hl<<2;
				goto again;

			default:
				printf(PF_L "impossible output checksum\n",
				       PF_P(xi));
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
			cksum = (cksum & 0xffff) + (cksum >> 16);
			cksum = 0xffff & (cksum + (cksum >> 16));
		}

		if (0 == m1) {
			m0->m_off -= sizeof(*fp);
			m0->m_len += sizeof(*fp);
		} else {
			m1->m_next = m0;
			m0 = m1;
		}
		bitswapcopy(&fp->fddi_mac.mac_da, &fp->fddi_mac.mac_da,
			    sizeof(fp->fddi_mac.mac_da));

		/* Listen to ourself, if we are supposed to.
		 */
		if (FDDI_ISBROAD_SW(&fp->fddi_mac.mac_da.b[0])) {
			mloop = m_copy(m0, sizeof(*fp), M_COPYALL);
			if (0 == mloop) {
				m_freem(m0);
				xi->xi_if.if_odrops++;
				IF_DROP(&xi->xi_if.if_snd);
				return ENOBUFS;
			}
		}
		break;


	case AF_UNSPEC:
#		define EP ((struct ether_header *)&dst->sa_data[0])
		m0 = xpi_mbuf_pad(xi, m0, sizeof(*fp),
				  sizeof(*fp)+sizeof(struct ether_arp));
		if (!m0)
			return ENOBUFS;
		fp = mtod(m0, struct fddi*);
		bcopy(&xi->xi_hdr, fp, sizeof(*fp));
		fp->fddi_llc.llc_etype = EP->ether_type;
		bitswapcopy(&EP->ether_dhost[0],
			    &fp->fddi_mac.mac_da,
			    sizeof(fp->fddi_mac.mac_da));
# undef EP
		break;

	case AF_RAW:
		/* The mbuf chain contains most of the frame, including the
		 *	MAC header but not the board header.
		 *	The board header is hidden in the start of the
		 *	supposed MAC header.
		 */
		m0 = xpi_mbuf_pad(xi, m0, 0, sizeof(*fp));
		if (!m0)
			return ENOBUFS;
		fp = mtod(m0, struct fddi*);
		fp->fddi_mac.filler[0] = MAC_DATA_HDR0;
		fp->fddi_mac.filler[1] = MAC_DATA_HDR1;
		fp->fddi_mac.mac_bits = MAC_DATA_HDR2;
		break;

	case AF_SDL:
		/* send an 802 packet for DLPI
		 */
#		define SCKTP ((struct sockaddr_sdl *)dst)
#		define SCKTP_HLEN (sizeof(*fp) - sizeof(fp->fddi_llc))

		/* choke if the MAC address is silly */
		if (SCKTP->ssdl_addr_len != sizeof(fp->fddi_mac.mac_da)) {
			m_freem(m0);
			return EAFNOSUPPORT;
		}
		/* The mbuf chain contains most of the frame, including the
		 *	LLC header but not the board or MAC headers.
		 */
		m0 = xpi_mbuf_pad(xi, m0, SCKTP_HLEN, SCKTP_HLEN);
		if (!m0)
			return ENOBUFS;
		fp = mtod(m0, struct fddi*);
		bcopy(&xi->xi_hdr, fp, SCKTP_HLEN);
		bitswapcopy(SCKTP->ssdl_addr, &fp->fddi_mac.mac_da,
			    sizeof(fp->fddi_mac.mac_da));
# undef SCKTP
# undef SCKTP_HLEN
		break;

	default:
		printf(PF_L "cannot handle address family %u\n",
		       PF_P(xi), dst->sa_family);
		m_freem(m0);
		return EAFNOSUPPORT;
	}
	/* each of the preceding cases leave fp pointing at the FDDI header
	 */


	/* Start DMA on the message, unless the board is dead.
	 */
	if (iff_dead(xi->xi_if.if_flags)
	    || (xi->xi_state & XI_ST_ZAPPED)) {
		xi->xi_if.if_oerrors++;
		IF_DROP(&xi->xi_if.if_snd);
		m_freem(mloop);
		m_freem(m0);
		return EHOSTDOWN;
	}


	/* If the chain of mbufs is too messy, copy it to 1 or 2 clusters
	 *	and start over.
	 */
restart:;

	d2bp0 = xi->xi_d2bp;
	d2bp0->hd.offsum = offsum;
	d2bp0->hd.startsum = startsum;
	d2bp0->hd.cksum = cksum;

	d2bp = d2bp0+1;
	blen1 = 0;
	m2 = m0;
	inx = 0;
	do {
		void *dp;
		paddr_t physp;

		if (0 == m2->m_len)
			continue;
		dp = mtod(m2, void*);
		if (!OUT_ALIGNED(dp)
		    || !OUT_ALIGNED(blen1)
		    || inx >= XPI_MAX_DMA_ELEM-3) {
/* We must copy the chain.
 *	Reserve extra DMA slots so we do not have to worry about running
 *	out when handling a last mbuf which crosses a page boundary or two.
 */
			METER(xi->xi_dbg.out_copy++);
			blen1 = m_length(m0);

			if (blen1 <= VCL_MAX) {
				m2 = 0;
			} else {
				blen2 = blen1 - VCL_MAX;
				blen1 = VCL_MAX;
				m2 = m_vget(M_DONTWAIT, blen2, MT_DATA);
				if (0 == m2) {
					xi->xi_if.if_odrops++;
					IF_DROP(&xi->xi_if.if_snd);
					m_freem(mloop);
					m_freem(m0);
					return ENOBUFS;
				}

				(void)m_datacopy(m0,VCL_MAX,blen2,
						 mtod(m2, caddr_t));
			}
			m1 = m_vget(M_DONTWAIT,blen1,MT_DATA);
			if (0 == m1) {
				xi->xi_if.if_odrops++;
				IF_DROP(&xi->xi_if.if_snd);
				m_freem(m2);
				m_freem(mloop);
				m_freem(m0);
				return ENOBUFS;
			}
			(void)m_datacopy(m0, 0,blen1, mtod(m1,caddr_t));
			m1->m_next = m2;
			m_freem(m0);
			m0 = m1;
			fp = mtod(m0, struct fddi*);
			goto restart;
		}

		blen3 = m2->m_len;
#ifdef WBCACHE
		/* write-back data in the cache for the DMA to find it */
		dcache_wb(dp, blen3);
#endif
		physp = kvtophys(dp);
		/* Request 2nd or 3rd DMA if it spans a physical I/O page.
		 */
		while (io_pnum(dp) != io_pnum((__psunsigned_t)dp + blen3-1)) {
			ASSERT(blen3<IO_NBPP*2);
			METER(xi->xi_dbg.pg_cross++);
			blen2 = IO_NBPP - (physp & IO_POFFMASK);
			d2bp->sg.addr = physp;
#ifdef ADDR_HI
			d2bp->sg.addr_hi = ADDR_HI(physp);
#endif
			d2bp->sg.len = blen2;
			++d2bp;
			inx++;
			blen1 += blen2;
			blen3 -= blen2;
			dp = (void*)((__psunsigned_t)dp + blen2);
			physp = kvtophys(dp);
		}
		d2bp->sg.addr = physp;
#ifdef ADDR_HI
		d2bp->sg.addr_hi = ADDR_HI(physp);
#endif
		d2bp->sg.len = blen3;
		++d2bp;
		inx++;
		blen1 += blen3;
	} while (0 != (m2 = m2->m_next));
	ASSERT(blen1 >= FDDI_MINOUT);
	ASSERT(blen1 <= FDDI_MAXOUT);

	/* Install the end flag so the board does not run off the end.
	 */
	d2bp->hd.start = XPI_D2B_BAD;
#ifdef _NO_UNCACHED_MEM_WAR
	if (((__psunsigned_t)d2bp) / CACHE_SLINE_SIZE
	    != ((__psunsigned_t)d2bp0) / CACHE_SLINE_SIZE)
		dcache_wb((void*)d2bp,sizeof(*d2bp));
#endif
	if (d2bp >= xi->xi_d2b_lim) {
		d2bp = &xi->xi_d2b[0];
		d2bp->hd.start = XPI_D2B_BAD;
#ifdef _NO_UNCACHED_MEM_WAR
		dcache_wb((void*)d2bp,sizeof(*d2bp));
#endif
	}
	xi->xi_d2bp = d2bp;

	/* Tell the board about the new job.
	 */
	d2bp0->hd.totlen = blen1;
	d2bp0->hd.start = XPI_D2B_RDY;
#ifdef _NO_UNCACHED_MEM_WAR
	dcache_wb((void*)d2bp0,sizeof(*d2bp)*(inx+1));
#endif

	xi->xi_if.if_obytes += blen1;
	xi->xi_if.if_opackets++;
	/* This may dirty the cache line containing the mbuf header, but
	 * since the board only reads while sending to the fiber, this is
	 * ok.
	 */
	IF_ENQUEUE_NOLOCK(&xi->xi_if.if_snd, m0);

	/* Check whether snoopers want to copy this packet.
	 *	Do it now, before we might call xpi_b2h() and free
	 *	the chain after the board has been very fast and
	 *	already transmitted the frame.
	 */
	if (RAWIF_SNOOPING(&xi->xi_rawif)
	    && snoop_match(&xi->xi_rawif, (caddr_t)fp, m0->m_len)) {
		struct mbuf *ms, *mt;
		int len;		/* m0 bytes to copy */
		int lenoff;
		int curlen;

		len = m_length(m0);
		lenoff = 0;
		curlen = len+sizeof(FDDI_IBUF);
		if (curlen > VCL_MAX)
			curlen = VCL_MAX;
		ms = m_vget(M_DONTWAIT,
			    MAX(curlen,sizeof(FDDI_IBUFLLC)), MT_DATA);
		if (0 != ms) {
			IF_INITHEADER(mtod(ms,caddr_t),
				      &xi->xi_if,
				      sizeof(FDDI_IBUFLLC));
			curlen = m_datacopy(m0, 0,
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
			snoop_drop(&xi->xi_rawif, SN_PROMISC,
				   mtod(m0,caddr_t), m0->m_len);
		} else {
			(void)snoop_input(&xi->xi_rawif, SN_PROMISC,
					  mtod(m0, caddr_t),
					  ms,
					  (lenoff > sizeof(*fp)
					  ? lenoff-sizeof(*fp) : 0));
		}
	}


	/* Ensure that the board knows about the frame.
	 */
	xpi_b2h(xi, 0);
	if ((xi->xi_state & XI_ST_SLEEP)
	    || xpi_low_latency) {
		xi->xi_state &= ~(XI_ST_SLEEP | XI_ST_NOPOKE);
		METER(xi->xi_dbg.board_poke++);
		xpi_wait(xi);
		xpi_op(xi,XCMD_WAKEUP);
	} else {
		xi->xi_state |= XI_ST_NOPOKE;
	}


	/* finally send to ourself */
	if (mloop != 0) {
		xi->xi_if.if_omcasts++;
		(void)looutput(&loif, mloop, dst, rte);
	} else if (FDDI_ISGROUP_SW(fp->fddi_mac.mac_da.b)) {
		xi->xi_if.if_omcasts++;
	}

	return 0;
#undef xi
}


/* send a DMA command to the board
 *	interrupts must be off
 */
static void
xpi_send_c2b(struct xpi_info *xi,
	     int op,
	     __psunsigned_t addr)
{
	register volatile struct xpi_c2b *c2bp, *n_c2bp;
#ifdef _NO_UNCACHED_MEM_WAR
	register int needflush;
#endif

	/* install things in the right order, to resolve race with board
	 */
	c2bp = xi->xi_c2bp;
	n_c2bp = c2bp+1;
	if (n_c2bp >= xi->xi_c2b_lim)
		n_c2bp = &xi->xi_c2b[0];

	/* Ensure the board can always see an end flag, so always get
	 * the next end flag out before we change the current entry.
	 */
	n_c2bp->c2b_op = XPI_C2B_EMPTY;
#ifdef _NO_UNCACHED_MEM_WAR
	/* Because the callers of xpi_send_c2b() always flush after
	 * their last request, we need only flush at the end
	 * of each cache block.
	 */
	needflush = (((__psunsigned_t)n_c2bp) / CACHE_SLINE_SIZE
		     != ((__psunsigned_t)c2bp) / CACHE_SLINE_SIZE);
	if (needflush)
		dcache_wb((void*)n_c2bp,sizeof(*n_c2bp));
#endif
	xi->xi_c2bp = n_c2bp;
	c2bp->c2b_addr = addr;
#ifdef ADDR_HI
	c2bp->c2b_addr_hi = ADDR_HI(addr);
#endif
	c2bp->c2b_op = op;
#ifdef _NO_UNCACHED_MEM_WAR
	if (needflush)
		dcache_wb((void*)c2bp,sizeof(*c2bp));
#endif
}


/* fill up the board stocks of empty buffers
 */
static int				/* 1=did some filling */
xpi_fillin(struct xpi_info *xi)
{
	struct mbuf *m;
	FDDI_IBUFLLC *p;
	int i;
	int worked = 0;

	CK_IFNET(xi);

	/* Do not bother reserving buffers if not turned on.
	 * Take it easy while drowning in input frames.
	 */
	if (iff_dead(xi->xi_if.if_flags)
#ifdef DODROWN
	    || (xi->xi_state & XI_ST_DROWN)
#endif
	    || !(xi->xi_state & XI_ST_MAC))
		return worked;

#ifdef OS_METER
	if (xi->xi_in_sml_num <= 1)
		xi->xi_dbg.no_sml++;
	if (xi->xi_in_med_num == 0)
		xi->xi_dbg.no_med++;
	if (xi->xi_in_big_num <= 1)
		xi->xi_dbg.no_big++;
#endif
	/* delay interrupts while we handle input
	 */
	if (xi->xi_byte_busy != 0
	    && !xpi_low_latency) {
		i = BTODELAY(xi->xi_byte_busy);
		if (i > 0) {
			xpi_send_c2b(xi, XPI_C2B_IDELAY,i);
#ifdef _NO_UNCACHED_MEM_WAR
			dcache_wb((void*)xi->xi_c2bp,sizeof(*xi->xi_c2bp));
#endif
		}
		xi->xi_byte_busy = 0;
	}

	while (xi->xi_in_sml_num < xpi_num_sml) {
#if R10000_SPECULATION_WAR
		/*
		 * IP28 R10K systems corrupt packets DMAed into regular
		 * mbufs.  We suspect mbuf management code or some mbuf
		 * user speculatively stores into the mbuf, perhaps when
		 * bcopying an adjacent mbuf.  The dki_dcache_inval here
		 * and in xpi_input are not enough to prevent the
		 * corruption, but using clustered mbufs seems to fix it.
		 */
		m = m_vget(M_DONTWAIT, XPI_MED_SIZE, MT_DATA);
		if (!m)
			goto exit;
#else /* !R10000_SPECULATION_WAR */
		m = m_get(M_DONTWAIT,MT_DATA);
		if (!m)
			goto exit;
		m->m_len = MLEN;
#endif /* R10000_SPECULATION_WAR */
		/* A subsequent mtod() would put the mbuf header into the
		 * cache which would undo the cache invalidation.
		 */
		p = mtod(m,FDDI_IBUFLLC*);
		xi->xi_in_sml_m[xi->xi_in_sml_t] = m;
		xi->xi_in_sml_d[xi->xi_in_sml_t] = p;
#ifdef WBCACHE
		dki_dcache_inval(p, XPI_SML_SIZE);
#endif
		xpi_send_c2b(xi, XPI_C2B_SML, kvtophys(p));
		xi->xi_in_sml_t = (xi->xi_in_sml_t+1) % XPI_MAX_SML;
		++xi->xi_in_sml_num;
		worked = 1;
	}

	while (xi->xi_in_med_num < xpi_num_med) {
		m = m_vget(M_DONTWAIT, XPI_MED_SIZE, MT_DATA);
		if (!m)
			break;
		p = mtod(m,FDDI_IBUFLLC*);
		xi->xi_in_med_m[xi->xi_in_med_t] = m;
		xi->xi_in_med_d[xi->xi_in_med_t] = p;
#ifdef WBCACHE
		dki_dcache_inval(p, XPI_MED_SIZE);
#endif
		xpi_send_c2b(xi, XPI_C2B_MED, kvtophys(p));
		xi->xi_in_med_t = (xi->xi_in_med_t+1) % XPI_MAX_MED;
		++xi->xi_in_med_num;
		worked = 1;
	}

	while (xi->xi_in_big_num < xpi_num_big) {
		m = m_vget(M_DONTWAIT, XPI_BIG_SIZE, MT_DATA);
		if (!m)
			break;
		p = mtod(m,FDDI_IBUFLLC*);
		xi->xi_in_big_m[xi->xi_in_big_t] = m;
		xi->xi_in_big_d[xi->xi_in_big_t] = p;
#ifdef WBCACHE
		dki_dcache_inval(p, XPI_BIG_SIZE);
#endif
		xpi_send_c2b(xi, XPI_C2B_BIG, kvtophys(p));
		xi->xi_in_big_t = (xi->xi_in_big_t+1) % XPI_MAX_BIG;
		++xi->xi_in_big_num;
		worked = 1;
	}

exit:;
#ifdef _NO_UNCACHED_MEM_WAR
	/* Flush last cache line that xpi_send_c2b() did not */
	if (worked)
		dcache_wb((void*)xi->xi_c2bp,sizeof(*xi->xi_c2bp));
#endif
	return worked;
}



/* if needed, then run the PCM state machines after a PHY interrupt
 */
static void
xpi_phy_pcm(struct xpi_info *xi,
	    int need_smt)
{
	struct smt *smta;

	if (!iff_alive(xi->xi_if.if_flags))
		return;

	(void)xpi_st_get(0,xi);

	smta = IS_SA(xi) ? &xi->xi_smt1 : &xi->xi_smt2;
	if (xi->xi_old_noise0 != smta->smt_st.noise.lo) {
		xi->xi_old_noise0 = smta->smt_st.noise.lo;
		/* avoid bogus noise as the light fades
		 */
		smta->smt_st.flags |= (PCM_NE | PCM_LEM_FAIL);
		SMT_LSLOG(smta,PCM_NLS);
		SMTDEBUGPRINT(smta,1, ("%sNoise Event\n",
				       PC_LAB(smta)));
		if (IS_SA(xi))
			need_smt |= 1;
		else
			need_smt |= 2;
	}

	if (IS_DASSM(xi)
	    && xi->xi_old_noise1 != xi->xi_smt1.smt_st.noise.lo) {
		xi->xi_old_noise1 = xi->xi_smt1.smt_st.noise.lo;
		/* avoid bogus noise as the light fades
		 */
		xi->xi_smt1.smt_st.flags |= (PCM_NE | PCM_LEM_FAIL);
		SMT_LSLOG(&xi->xi_smt1,PCM_NLS);
		SMTDEBUGPRINT(&xi->xi_smt1,1, ("%sNoise Event\n",
					       PC_LAB(&xi->xi_smt1)));
		need_smt |= 1;
	}

	if (need_smt & 1)
		moto_pcm(&xi->xi_smt1);

	if (need_smt & 2)
		moto_pcm(&xi->xi_smt2);
}


/* process a new "line state"
 */
static void
xpi_new_rls(struct smt *smt,
	    struct xpi_info *xi,
	    unchar ls)
{
	struct smt *osmt;

	switch (ls) {
	case RCV_TRACE_PROP:
		/* PC(88c) and PC(82)
		 *
		 * If we are the MAC that is supposed to
		 * respond to PC_Trace, then do so.
		 * Otherwise, propagate it.
		 *
		 * If we are are single-MAC, dual-attached, "THRU,"
		 * on the primary ring, then we are not the
		 * PHY that is being "traced," and should
		 * ask the board to propagate the TRACE.
		 * Otherwise, we are the target and should
		 * respond with QLS.
		 *
		 * When the destination MAC responds with QLS,
		 * we will recycle.
		 *
		 * Delay the restart to allow the ring to
		 * do something useful in case we are the
		 * cause, and instead of a path test.
		 */

		if (smt->smt_st.pcm_state != PCM_ST_ACTIVE
		    || PCM_SET(smt, PCM_TRACE_OTR))
			break;

		osmt = xpi_osmt(smt, xi);
		if (smt->smt_conf.type == SM_DAS
		    && smt->smt_conf.pc_type == PC_A
		    && PCM_SET(smt,PCM_THRU_FLAG)) {
			SMTDEBUGPRINT(smt,0,
				      ("%sMLS--trace prop\n", PC_LAB(smt)));
			smt->smt_st.flags |= PCM_TRACE_OTR;

			TPC_RESET(osmt);
			SMT_USEC_TIMER(osmt, SMT_TRACE_MAX);
			smt_new_st(osmt, PCM_ST_TRACE);
			osmt->smt_st.flags &= ~PCM_PC_DISABLE;
			xpi_wait((struct xpi_info*)osmt->smt_pdata);
			xpi_op((struct xpi_info*)osmt->smt_pdata,
			       ELM(osmt,XCMD_PC_TRACE0,XCMD_PC_TRACE1));

		} else {
			SMTDEBUGPRINT(smt,0, ("%sMLS--traced\n",PC_LAB(smt)));
			smt_fin_trace(smt,osmt);
		}
		break;

	case RCV_SELF_TEST:
		if (smt->smt_st.pcm_state == PCM_ST_TRACE)
			smt->smt_st.flags |= PCM_SELF_TEST;
		break;

	case RCV_BREAK:
		if (SMT1(smt,xi))
			xi->xi_elm1_st = -1;
		else
			xi->xi_elm2_st = -1;
		smt_off(smt);
		break;

	case RCV_PCM_CODE:
		smt->smt_st.flags &= ~PCM_PHY_BUSY;
		break;

	case PCM_NLS:
		smt->smt_st.flags |= (PCM_NE | PCM_LEM_FAIL);
		SMT_LSLOG(smt,PCM_ULS);
		SMTDEBUGPRINT(smt,0, ("%sCMT overrun\n", PC_LAB(smt)));
		smt_off(smt);
		break;

	case PCM_LSU:			/* ignore it */
		break;

	default:
		smt->smt_st.rls = (enum pcm_ls)ls;
		SMT_LSLOG(smt,ls);
		SMTDEBUGPRINT(smt,4, ("%sR=%s ",PC_LAB(smt), str_ls[ls]));
		break;
	}
}


/* process SMT and CMT interrupts
 */
static int
xpi_phy_intr(struct xpi_info *xi,
	     unchar ls0,
	     unchar ls1)
{
	int need_smt;

	if (!iff_alive(xi->xi_if.if_flags))
		return 0;

	need_smt = 4;

	/* see what the interrupt is about */
	if (ls0 != PCM_ULS) {
		if (IS_SA(xi)) {
			xpi_new_rls(&xi->xi_smt1,xi,ls0);
			need_smt |= 1;
		} else {
			xpi_new_rls(&xi->xi_smt2,xi,ls0);
			need_smt |= 2;
		}
	}
	if (ls1 != PCM_ULS && IS_DASSM(xi)) {
		xpi_new_rls(&xi->xi_smt1,xi,ls1);
		need_smt |= 1;
	}

	return need_smt;
}


/* See if a DLPI function wants a frame.
 */
static int
xpi_dlp(struct xpi_info *xi,
	int port,
	int encap,
	struct mbuf *m,
	int len)
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

	if ((*dlp->dl_infunc)(dlp, &xi->xi_if, m2,
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
xpi_input(struct xpi_info *xi,
	  int op,
	  int totlen)
{
	struct mbuf *m;
	FDDI_IBUFLLC *fhp;
	int snoopflags = 0;
	u_long cksum;
	uint port;

	if (totlen <= XPI_SML_SIZE) {
		BAD_BD(xi,xi->xi_in_sml_num > 0, return);
		m = xi->xi_in_sml_m[xi->xi_in_sml_h];
		fhp = xi->xi_in_sml_d[xi->xi_in_sml_h];
		xi->xi_in_sml_h = (xi->xi_in_sml_h+1) % XPI_MAX_SML;
		xi->xi_in_sml_num--;
	} else if (totlen <= XPI_MED_SIZE) {
		BAD_BD(xi,xi->xi_in_med_num > 0, return);
		m = xi->xi_in_med_m[xi->xi_in_med_h];
		fhp = xi->xi_in_med_d[xi->xi_in_med_h];
		xi->xi_in_med_h = (xi->xi_in_med_h+1) % XPI_MAX_MED;
		xi->xi_in_med_num--;
	} else {
		BAD_BD(xi,xi->xi_in_big_num > 0, return);
		m = xi->xi_in_big_m[xi->xi_in_big_h];
		fhp = xi->xi_in_big_d[xi->xi_in_big_h];
		xi->xi_in_big_h = (xi->xi_in_big_h+1) % XPI_MAX_BIG;
		xi->xi_in_big_num--;
#ifndef lint
		if (totlen != XPI_BIG_SIZE)
			METER(xi->xi_dbg.waste_page++);
#endif
	}

/* It would be better to write-back and invalidate the cache when
 * the mbufs are allocated, in a single operation.  Unfortunately,
 * there are too many unknown places in the kernel that dereference
 * wild pointers and pollute the cache.  They necessitate an additional
 * invalidation here.
 */
	dki_dcache_inval(mtod(m, char *), m->m_len);
	m->m_next = 0;
	m->m_len = totlen;

	/* If not finished, wait for more.
	 */
	if (op != XPI_B2H_IN_DONE) {
		xi->xi_in = m;
		xi->xi_in_len = totlen;
		return;
	}
	if (xi->xi_in != 0) {
		totlen += xi->xi_in_len;
		xi->xi_in->m_next = m;
		m = xi->xi_in;
		fhp = mtod(m, FDDI_IBUFLLC*);
		xi->xi_in = 0;
	}

	totlen -= sizeof(FDDI_IBUF);

	/* keep the mbuf large enough to keep the snoop code from
	 * calling panic or going crazy.
	 */
	if (totlen < FDDI_MIN_LLC) {
		bzero((char*)fhp+totlen+sizeof(FDDI_IBUF),
		      FDDI_MIN_LLC - totlen);
		m->m_len = FDDI_MIN_LLC + sizeof(FDDI_IBUF);
	}

	IF_INITHEADER(fhp, &xi->xi_if, (int)sizeof(FDDI_IBUFLLC));

	/* Predict activity.  TCP/IP ACKs predict a lot of work.
	 */
	xi->xi_byte_busy += totlen;
	if (totlen == 40+sizeof(struct fddi))
		xi->xi_byte_busy += ACKRATE*XPI_BIG_SIZE;

	xi->xi_if.if_ibytes += totlen;
	xi->xi_if.if_ipackets++;

	if (0 != (fhp->fbh_fddi.fddi_mac.mac_bits
		  & (MAC_ERROR_BIT | MAC_E_BIT))) {
		xi->xi_if.if_ierrors++;
		snoopflags = (SN_ERROR|SNERR_FRAME);

	} else if (totlen < sizeof(struct fddi)) {
		if (totlen < sizeof(struct lmac_hdr)) {
			xi->xi_if.if_ierrors++;
			snoopflags = (SN_ERROR|SNERR_TOOSMALL);
		}
		port = FDDI_PORT_LLC;

	} else {
#		define FSA fddi_mac.mac_sa.b
#		define FDA fddi_mac.mac_da.b
#		define MY_SA() (fhp->fbh_fddi.FSA[sizeof(LFDDI_ADDR)-1] \
				== xi->xi_hdr.FSA[sizeof(LFDDI_ADDR)-1]\
				&& !bcmp(fhp->fbh_fddi.FSA,		 \
					 xi->xi_hdr.FSA,		 \
					 sizeof(LFDDI_ADDR)))
#		define OTR_DA() (fhp->fbh_fddi.FDA[sizeof(LFDDI_ADDR)-1] \
				 != xi->xi_hdr.FSA[sizeof(LFDDI_ADDR)-1]\
				 || bcmp(fhp->fbh_fddi.FDA,		 \
					 xi->xi_hdr.FSA,		 \
					 sizeof(LFDDI_ADDR)))

		/* Notice if it is a broadcast or multicast frame.
		 * Get rid of imperfectly filtered multicasts
		 */
		if (FDDI_ISGROUP_SW(fhp->fbh_fddi.FDA)) {
			/* Check for a packet from ourself.
			 */
			if (MY_SA()) {
				METER(xi->xi_dbg.own_out++);
				m_freem(m);
				return;
			}
			if (FDDI_ISBROAD_SW(fhp->fbh_fddi.FDA)) {
				m->m_flags |= M_BCAST;
			} else {
				if (0 == (xi->xi_ac.ac_if.if_flags
					  & IFF_ALLMULTI)
				    && !mfethermatch(&xi->xi_filter,
						     fhp->fbh_fddi.FDA, 0)) {
					if (RAWIF_SNOOPING(&xi->xi_rawif)
					    && snoop_match(&xi->xi_rawif,
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
			xi->xi_if.if_imcasts++;

		} else if ((xi->xi_state & XI_ST_PROM)
			   && OTR_DA()) {
			if (MY_SA()) {
				METER(xi->xi_dbg.own_out++);
				m_freem(m);
				return;
			}

			if (RAWIF_SNOOPING(&xi->xi_rawif)
			    && snoop_match(&xi->xi_rawif,
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
#undef MY_SA
	}

	/* do raw snooping
	 */
	if (RAWIF_SNOOPING(&xi->xi_rawif)) {
		if (!snoop_input(&xi->xi_rawif, snoopflags,
				 (caddr_t)&fhp->fbh_fddi,
				 m,
				 (totlen>sizeof(struct fddi)
				  ? totlen-sizeof(struct fddi) : 0))) {
#ifdef DODROWN
			xpi_down(xi);
#endif
		}
		if (0 != snoopflags)
			return;

	} else if (0 != snoopflags) {
		/* if bad, count and skip it, and let snoopers know. */
		m_freem(m);
		if (RAWIF_SNOOPING(&xi->xi_rawif))
			snoop_drop(&xi->xi_rawif, snoopflags,
				   (caddr_t)&fhp->fbh_fddi, totlen);
		if (RAWIF_DRAINING(&xi->xi_rawif))
			drain_drop(&xi->xi_rawif, port);
		return;
	}

	/* If it is a frame we understand, then give it to the
	 *	correct protocol code.
	 */
	switch (port) {
	case ETHERTYPE_IP:
		/* Finish TCP or UDP checksum on non-fragments.
		 * This knows at most 2 mbufs are allocated.  It knows
		 * the board offers checksums only if the frame is big
		 * and an even number of bytes.
		 */
		cksum = *(ushort*)&fhp->fbh_fddi.fddi_mac.filler[0];
		if (cksum != 0) {
		    struct ip *ip;
		    int hlen, cklen;

		    METER(xi->xi_dbg.in_cksums++);

		    ip = (struct ip*)(fhp+1);
		    hlen = ip->ip_hl<<2;
		    cklen = totlen - (hlen+sizeof(struct fddi)
				      + XPI_IN_CKSUM_SIZE);

		    /* Do not bother if it is an IP fragment or if
		     * not all of the unchecked data is in the 1st mbuf.
		     */
		    if (0 != (ip->ip_off & ~IP_DF)
			|| cklen + sizeof(FDDI_IBUFLLC) > m->m_len) {
			    METER(xi->xi_dbg.in_cksum_frag++);
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
			if (cksum == 0 || cksum == 0xffff
			    || xpi_fake_in_cksum) {
			    m->m_flags |= M_CKSUMMED;
			} else {
			    /* The TCP input code will check it again
			     * and do any complaining.
			     */
				METER(xi->xi_dbg.in_cksum_bad++);
			}
		    } else {
			    METER(xi->xi_dbg.in_cksum_proto++);
		    }
#ifdef METER
		} else if (totlen > (4096+sizeof(struct ip)
				     +sizeof(struct fddi))) {
			if ((totlen & 3) != 0)
				METER(xi->xi_dbg.in_cksum_odd++);
			else
				METER(xi->xi_dbg.in_cksum_missing++);
#endif
		}
		if (network_input(m, AF_INET, 0)) {
#ifdef DODROWN
			xpi_drown(xi);
#endif
			xi->xi_if.if_iqdrops++;
			xi->xi_if.if_ierrors++;
		}
		return;

	case ETHERTYPE_ARP:
		/* Assume that if_ether.c can understand arp_hrd
		 *	if we strip the LLC header.
		 */
		arpinput(&xi->xi_ac, m);
		return;

	case FDDI_PORT_LLC:
		if (totlen >= sizeof(struct lmac_hdr)+2
		    && xpi_dlp(xi, fhp->fbh_fddi.fddi_llc.llc_dsap,
			       DL_LLC_ENCAP, m, totlen))
			return;
		break;

	default:
		if (xpi_dlp(xi, port, DL_SNAP0_ENCAP, m, totlen))
			return;
		break;
	}

	/* if we cannot find a protocol queue, then flush it down the
	 *	drain, if it is open.
	 */
	if (RAWIF_DRAINING(&xi->xi_rawif)) {
		drain_input(&xi->xi_rawif, port,
			    (caddr_t)&fhp->fbh_fddi.fddi_mac.mac_sa, m);
	} else {
		xi->xi_if.if_noproto++;
		m_freem(m);
	}
}


/* process the board-to-host ring
 *	interrupts must be off.
 */
static void
xpi_b2h(struct xpi_info *xi, u_long ps_ok)
{
	int blen, op;
	u_char sn;
	int pokebd;
	int need_smt = 0;
	volatile struct xpi_b2h *b2hp;

	ASSERT((XPI_B2H_LEN % 256 ) != 0);
	CK_IFNET(xi);

	/* if recursing, for example output->here->arp->output->here,
	 * then quit
	 */
	if (xi->xi_in_active) {
		METER(xi->xi_dbg.in_active++);
		return;
	}
	xi->xi_in_active = 1;

	if ((xi->xi_state & XI_ST_NOPOKE)
	    && xi->xi_if.if_snd.ifq_len != 0) {
		pokebd = 1;
	} else {
		pokebd = 0;
	}
	xi->xi_state &= ~XI_ST_NOPOKE;

	b2hp = xi->xi_b2hp;
	for (;;) {
		sn = b2hp->b2h_sn;
		blen = b2hp->b2h_val;
		op = b2hp->b2h_op;
		if (sn != xi->xi_b2h_sn) {  /* no more new work */
			xi->xi_b2hp = b2hp;

			if (xi->xi_b2h_sn != (sn+XPI_B2H_LEN)%256
			    && !(xi->xi_state & XI_ST_WHINE)) {
				DEBUGPRINT(xi, (PF_L "bad B2H sn=0x%x !=0x%x"
						" at 0x%x op=%d val=0x%x\n",
						PF_P(xi), sn,xi->xi_b2h_sn,
						b2hp,op,blen));
				xi->xi_state |= XI_ST_WHINE;
			}
			if (xpi_fillin(xi)) {
				/* Fix race in board going to sleep as host
				 * is giving it buffers.
				 */
				pokebd = 1;
				continue;
			}
			if (need_smt)
				xpi_phy_pcm(xi, need_smt);

			if (pokebd && (xi->xi_state & XI_ST_SLEEP)) {
				xi->xi_state &= ~XI_ST_SLEEP;
				METER(xi->xi_dbg.board_poke++);
				xpi_wait(xi);
				xpi_op(xi,XCMD_WAKEUP);
			}
			xi->xi_in_active = 0;
			return;
		}

		xi->xi_b2h_sn = sn+1;
		switch (op) {
		case XPI_B2H_SLEEP:	/* board is going to sleep */
			xi->xi_state |= XI_ST_SLEEP;
			break;

		case XPI_B2H_ODONE:	/* free output mbuf chains
			 */
			do {
				struct mbuf *m;
				IF_DEQUEUE_NOLOCK(&xi->xi_if.if_snd, m);
				m_freem(m);
			} while (--blen > 0);

			if ((xi->xi_state & ps_ok) != 0)
				ps_txq_stat(&xi->xi_if,
					    (XPI_MAX_OUTQ
					     - xi->xi_if.if_snd.ifq_len));
			break;

		case XPI_B2H_IN:
		case XPI_B2H_IN_DONE:
			xi->xi_state &= ~XI_ST_SLEEP;
			xpi_input(xi, op, blen);
			break;

		case XPI_B2H_PHY:
			xpi_send_c2b(xi,XPI_C2B_PHY_ACK,0);
#ifdef _NO_UNCACHED_MEM_WAR
			dcache_wb((void*)xi->xi_c2bp,sizeof(*xi->xi_c2bp));
#endif
			need_smt |= xpi_phy_intr(xi, blen>>8, blen);
			pokebd = 1;
			break;

		default:		/* sick board */
			if (!(xi->xi_state & XI_ST_WHINE))
				DEBUGPRINT(xi, (PF_L "invalid B2H at 0x%x"
						" sn=0x%x op=%d val=0x%x\n",
						PF_P(xi), b2hp,sn,op,blen));
			xi->xi_state |= XI_ST_WHINE;
			break;
		}

		if (++b2hp >= xi->xi_b2h_lim)
			b2hp = &xi->xi_b2h[0];
	}
}



/* handle interrupts
 */
#ifdef EVEREST
/* ARGSUSED */
void
if_xpiintr(eframe_t *ep, void *p)
{
#define XI ((struct xpi_info*)p)

	ASSERT(XI->xi_if.if_name == xpi_name);

	IFNET_LOCK(&XI->xi_if);
	METER(XI->xi_dbg.ints++);
	*XI->xi_gio = XPI_GIO_B2H_CLR;
	xpi_b2h(XI, XI_ST_PSENABLED);		/* check the DMA pipeline */
	IFNET_UNLOCK(&XI->xi_if);

	/* check the other device on the board */
	p = XI->xi_xi2;
	if (XI != 0) {
		IFNET_LOCK(&XI->xi_if);
		*XI->xi_gio = XPI_GIO_B2H_CLR;
		xpi_b2h(XI, XI_ST_PSENABLED);
		IFNET_UNLOCK(&XI->xi_if);
	}
#undef XI
}
#else /* !EVEREST */
/* ARGSUSED */
void
if_xpiintr(__psint_t unit, struct eframe_s *ep)
{
	struct xpi_info *xi;
	static time_t last_int;
	static int noint;
	__uint32_t inum;


	ASSERT(unit >= 0 && unit < XPI_MAXBD);
	xi = xpi_infop[unit];
	ASSERT(xi != 0);
	ASSERT(xi->xi_if.if_name == xpi_name);

	IFNET_LOCK(&xi->xi_if);

	inum = xi->xi_hcr.inum;
	if (inum == xi->xi_inum) {
		METER(++xi->xi_dbg.noint);
		if (last_int == lbolt) {
			if (++noint > 500) {
				printf(PF_L "%d false interrupts\n", PF_P(xi),
				       noint);
				noint = 0;
			} else {
				IFNET_UNLOCK(&xi->xi_if);
				return;	/* false alarm */
			}
		} else {
			last_int = lbolt;
			noint = 0;
			IFNET_UNLOCK(&xi->xi_if);
			return;		/* false alarm */
		}
	}

	xi->xi_inum = inum;
	*xi->xi_gio = XPI_GIO_B2H_CLR;
	METER(xi->xi_dbg.ints++);
	xpi_b2h(xi, XI_ST_PSENABLED);	/* check the DMA pipeline */
	IFNET_UNLOCK(&xi->xi_if);
}
#endif /* !EVEREST */


/* add a single board
 *	Interrupts must be off or not yet turned on.
 */
static void
xpi_init(struct xpi_info *xi)
{
	struct ps_parms ps_params;
	static SMT_MANUF_DATA mnfdat = SGI_FDDI_MNFDATA(xpix);
#	define MANUF_INDEX (sizeof(SGI_FDDI_MNFDATA_STR(xpix))-2)


	xi->xi_smt1.smt_pdata = (caddr_t)xi;
	xi->xi_smt1.smt_ops = &xpi_ops;
	xi->xi_smt2.smt_pdata = (caddr_t)xi;
	xi->xi_smt2.smt_ops = &xpi_ops;

	xi->xi_smt1.smt_conf.tvx = DEF_TVX;
	xi->xi_smt2.smt_conf.tvx = DEF_TVX;
	xi->xi_smt1.smt_conf.t_max = DEF_T_MAX;
	xi->xi_smt2.smt_conf.t_max = DEF_T_MAX;
	xi->xi_smt1.smt_conf.t_req = DEF_T_REQ;
	xi->xi_smt2.smt_conf.t_req = DEF_T_REQ;
	xi->xi_smt1.smt_conf.t_min = FDDI_MIN_T_MIN;
	xi->xi_smt2.smt_conf.t_min = FDDI_MIN_T_MIN;

	xi->xi_smt1.smt_conf.ip_pri = 1;
	xi->xi_smt2.smt_conf.ip_pri = 1;

	xi->xi_smt1.smt_conf.ler_cut = SMT_LER_CUT_DEF;
	xi->xi_smt2.smt_conf.ler_cut = SMT_LER_CUT_DEF;

	bcopy(&mnfdat, &xi->xi_smt1.smt_conf.mnfdat,
	      sizeof(mnfdat));
	if (xi->xi_if.if_unit >= 10) {
		xi->xi_smt1.smt_conf.mnfdat.manf_data[MANUF_INDEX] = (xi->xi_if.if_unit/10)+'0';
		xi->xi_smt1.smt_conf.mnfdat.manf_data[MANUF_INDEX+1] = (xi->xi_if.if_unit%10)+'0';
	} else {
		xi->xi_smt1.smt_conf.mnfdat.manf_data[MANUF_INDEX] = xi->xi_if.if_unit+'0';
	}

	xi->xi_if.if_name = xpi_name;	/* create ('attach') the interface */
	xi->xi_if.if_type = IFT_FDDI;
	xi->xi_if.if_baudrate.ifs_value = 1000*1000*100;
	xi->xi_if.if_flags = IFF_BROADCAST|IFF_MULTICAST|IFF_IPALIAS;
	xi->xi_if.if_output = xpi_output;
	xi->xi_if.if_ioctl = xpi_ioctl;
	xi->xi_if.if_rtrequest = arp_rtrequest;

	if_attach(&xi->xi_if);

	/* Allocate a multicast filter table with an initial size of 10.
	 */
	if (!mfnew(&xi->xi_filter, 10))
		cmn_err_tag(230,CE_PANIC, PF_L "no memory for frame filter\n",
			    PF_P(xi));

	/* Initialize the raw ethernet socket interface.
	 *	Tell the snoop stuff to watch for our address in FDDI bit
	 *	order.
	 */
	rawif_attach(&xi->xi_rawif, &xi->xi_if,
		     (caddr_t)&xi->xi_smt1.smt_st.addr,
		     (caddr_t)&etherbroadcastaddr[0],
		     sizeof(xi->xi_smt1.smt_st.addr),
		     sizeof(struct fddi),
		     structoff(fddi, fddi_mac.mac_sa),
		     structoff(fddi, fddi_mac.mac_da));

	/* RSVP.  Support admission control and packet scheduling */
	ps_params.bandwidth = 100000000;
	ps_params.txfree = XPI_MAX_OUTQ;
	ps_params.txfree_func = xpi_txfree_len;
	ps_params.state_func = xpi_setstate;
	ps_params.flags = 0;
	(void)ps_init(&xi->xi_if, &ps_params);

	IFNET_LOCK(&xi->xi_if);
	xpi_reset(xi,1);		/* reset the board */
	IFNET_UNLOCK(&xi->xi_if);
}


/* probe for a single board, and attach if present.
 */
#ifdef EVEREST
static struct xpi_info*
xpi_allocxpi(uint unit,
	     volatile __uint32_t *gio,
	     uint slot,
	     uint adapter)
{
	struct xpi_info *xi;

	xi = xpi_infop[unit];
	if (xi == 0) {
		struct xpi_dma1 *dma1;
		struct xpi_dma2 *dma2;

		xi = (struct xpi_info*)kern_malloc(sizeof(struct xpi_info));

		/* get buffers for non-page crossing DMA structures
		 */
		ASSERT(NBPC >= sizeof(struct xpi_dma1));
		ASSERT(XPI_BIG_SIZE <= MCLBYTES);
		ASSERT(XPI_SML_SIZE <= MLEN);
		ASSERT(NBPC >= sizeof(struct xpi_dma2));

		dma1 = (struct xpi_dma1*)kvpalloc(1, VM_DIRECT|VM_STALE, 0);
		dma2 = (struct xpi_dma2*)kvpalloc(1, VM_DIRECT|VM_STALE, 0);
		if (!xi || !dma1 || !dma2) {
			printf(PF_L "no memory\n", unit, slot, adapter);
			if (xi)
				kern_free(xi);
			if (dma1)
				kvpfree(dma1,1);
			if (dma2)
				kvpfree(dma2,1);
			return 0;
		}

		bzero(xi,sizeof(*xi));
		bzero(dma1, sizeof(*dma1));
		bzero(dma2, sizeof(*dma2));

		xi->xi_dma1 = dma1;
		xi->xi_dma2 = dma2;

		xpi_infop[unit] = xi;
	}

	xi->xi_if.if_unit = unit;
	xi->xi_gio = gio;
	xi->xi_slot = slot;
	xi->xi_adapter = adapter;

	return xi;
}

/* ARGSUSED */
void
if_xpiedtinit(struct edt *edtp)
{
	static repeat = 0;
	struct xpi_info *xi, *xi2;
	__psunsigned_t swin;
	volatile __uint32_t *gio;
	uint unit, slot, adapter;
	uint dang_indx;
	graph_error_t rc;
	vertex_hdl_t io4vhdl, xpivhdl;
	int i, id, dang_vers;
	int s;

	s = splimp();
	if (repeat) {
		splx(s);
		return;
	}
	repeat = 1;

	/* look for all boards */
	unit = 0;
	for (dang_indx = 0; dang_indx < DANG_MAX_INDX; dang_indx++) {
		id = dang_id[dang_indx] & GIO_SID_MASK;
		if (id != GIO_ID_XPI_M0 && id != GIO_ID_XPI_M1)
			continue;

		slot = DANG_INX2SLOT(dang_indx);
		adapter = DANG_INX2ADAP(dang_indx);
		swin = DANG_INX2BASE(dang_indx);
		gio = (volatile __uint32_t*)DANG_INX2GIOBASE(dang_indx,
							     0x1f400000);
#ifdef XPI_DEBUG
		printf(PF_L "swin=0x%x gio=0x%x\n",
		       unit, slot,adapter,swin,gio);
#endif

		if (badaddr(gio, 4)) {
			printf("xpi slot %d adapter %d: bad probe\n",
			       slot, adapter);
			continue;
		}
		if (id != (*gio & GIO_SID_MASK)) {
			printf("xpi slot %d adapter %d: bad GIO ID 0x%x\n",
			       slot, adapter, i);
			continue;
		}

		if (XPI_MAXBD <= unit) {
			printf(PF_L "extra board\n",
			       unit, slot, adapter);
			break;
		}

		xi = xpi_allocxpi(unit,gio,slot,adapter);
		if (!xi)
			break;

		/* allocate the interrupt
		 */
		i = dang_intr_conn((evreg_t*)(swin+DANG_INTR_GIO_0),
				   EVINTR_ANY, if_xpiintr, xi);
		if (i == 0) {
			printf(PF_L "failed to allocate interrupt\n",
			       unit, slot, adapter);
			break;
		}

		if (xpi_signal(xi, DELAY_RESET, XPI_SIGNAL_RESET)
		    && (xi->xi_hcr.vers & XPI_VERS_NO_PHY) != 0) {
			printf("xpi slot %d adapter %d: "
			       "missing first PHY card\n",
			       slot, adapter);
		}
		/* we are committed now */
		unit++;
		xi->xi_vers = xi->xi_hcr.vers;
		dang_vers = (EV_GET_REG(swin+DANG_VERSION_REG)
			     & DANG_VERSION_MASK) >> DANG_VERSION_SHFT;
		if (dang_vers <= DANG_VERSION_B)
			xi->xi_state |= XI_ST_BAD_DANG;

		xpi_init(xi);
		EV_SET_REG(swin+DANG_INTR_ENABLE,
			   DANG_IRS_GIO0 | DANG_IRS_ENABLE);

		/* See about the 2nd interface.
		 */
		if (xi->xi_state & XI_ST_BAD_DANG) {
			printf(PF_L "single port with rev.%c DANG chip\n",
			       PF_P(xi), 'A'-1+dang_vers);
			continue;
		}
		if (0 != (EV_GET_REG(swin+DANG_INTR_STATUS)
			  & DANG_ISTAT_GIOSTAT)) {
			if (SHOWCONFIG)
				printf(PF_L "single port present\n",
				       PF_P(xi));
			continue;
		}
		if (SHOWCONFIG)
			printf(PF_L "present\n", PF_P(xi));
		gio++;
		if (badaddr(gio, 4)) {
			printf("xpi slot %d adapter %d: "
			       "second probe failed\n",
			       slot, adapter);
			continue;
		}
		i = *gio;
		if ((i & GIO_SID_MASK) != GIO_ID_XPI_M1) {
			printf(PF_L "bad second GIO ID 0x%x\n",
			       PF_P(xi));
			continue;
		}

		xi2 = xpi_allocxpi(unit,gio,slot,adapter);
		if (!xi2)
			break;

		if (xpi_signal(xi2, DELAY_RESET, XPI_SIGNAL_RESET)
		    && (xi2->xi_hcr.vers & XPI_VERS_NO_PHY) != 0) {
			printf(PF_L "missing second PHY card\n", PF_P(xi));
			continue;
		}
		xi->xi_xi2 = xi2;
		xi2->xi_xi2 = xi;
		unit++;
		xi->xi_vers = xi->xi_hcr.vers;
		if (SHOWCONFIG)
			printf(PF_L "present\n", PF_P(xi2));

		xpi_init(xi2);

		/* use the same interrupt on both ports */
		i = EV_GET_REG(swin+DANG_INTR_GIO_0);
		EV_SET_REG(swin + DANG_INTR_GIO_1, i);
		EV_SET_REG(swin + DANG_INTR_ENABLE,
			   (DANG_IRS_GIO1 | DANG_IRS_ENABLE));
	}

	splx(s);
	if (unit == 0) {
		printf("xpi0: missing\n");
		return;
	}

	/* Announce the devices in reverse order so that they are listed
	 * in the right order by hinv.
	 */
	while (unit-- != 0) {
		xi = xpi_infop[unit];
		add_to_inventory(INV_NETWORK, INV_NET_FDDI, INV_FDDI_XPI,
				 unit,
				 (XPI_VERS_DANG
				  | (xi->xi_slot << XPI_VERS_SLOT_S)
				  | (xi->xi_adapter << XPI_VERS_ADAP_S)
				  | ((xi->xi_vers & XPI_VERS_M)
				     / XPI_VERS_MEZ_S)
				  | (xi->xi_vers & ~XPI_VERS_M)));
		rc = io4_hwgraph_lookup(slot, &io4vhdl);
		if (rc == GRAPH_SUCCESS) {
			char loc_str[16];
			char edge_name[8];
			sprintf(loc_str, "%s/%d", EDGE_LBL_XPI, unit);
			sprintf(edge_name, "%s%d", EDGE_LBL_XPI, unit);
			(void) if_hwgraph_add(io4vhdl, loc_str, "if_xpi",
				edge_name, &xpivhdl);
		}
	}
}
#else /* !EVEREST */
void
if_xpiedtinit(struct edt *edtp)
{
	struct xpi_info *xi;
	struct xpi_dma1 *dma1;
	struct xpi_dma2 *dma2;
	volatile __uint32_t *gio;
	int slot;
	static int unit = 0;
	int id;
	int s;
	graph_error_t rc;
	char loc_str[32];
	vertex_hdl_t giovhdl, xpivhdl;

	gio = (volatile __uint32_t*)edtp->e_base;
	slot = edtp->e_ctlr;

	if (XPI_MAXBD <= unit) {
		printf(PF_L "extra board in slot %d\n", unit, slot);
		return;
	}

	if (badaddr(gio, 4)) {
		printf(PF_L "missing from slot %d\n", unit, slot);
		return;
	}

	id = *gio;
	if ((id & GIO_SID_MASK) != GIO_ID_XPI) {
		if (SHOWCONFIG)
			printf(PF_L "id=%x in slot %d"
			       " not for an FDDI board\n",
		       unit, id, slot);
		return;
	}

	xi = (struct xpi_info*)kern_malloc(sizeof(struct xpi_info));

	/* get buffers for non-page crossing DMA structures
	 */
	ASSERT(NBPC >= sizeof(struct xpi_dma1));
	ASSERT(XPI_BIG_SIZE <= MCLBYTES);
	ASSERT(XPI_SML_SIZE <= MLEN);
	ASSERT(NBPC >= sizeof(struct xpi_dma2));

	dma1 = (struct xpi_dma1*)kvpalloc(1, VM_DIRECT|VM_STALE, 0);
	dma2 = (struct xpi_dma2*)kvpalloc(1, VM_DIRECT|VM_STALE, 0);
	if (!xi || !dma1 || !dma2) {
		printf(PF_L "no memory for slot %d\n", unit, slot);
		if (xi)
			kern_free(xi);
		if (dma1)
			kvpfree(dma1,1);
		if (dma2)
			kvpfree(dma2,1);
		return;
	}

#ifdef _NO_UNCACHED_MEM_WAR
	/* Adjust start of communications area so that the
	 * host-written portion can be cached and board written
	 * part can be uncached for the problems on the IP26/IP28.
	 */
	ASSERT(NBPC >= sizeof(struct xpi_dma1)+CACHE_SLINE_SIZE);
	ASSERT(sizeof(dma1->dma_hc.w) % 8 == 0);
	dma1 = (struct xpi_dma1*)((__psunsigned_t)dma1
				  + ((NBPC - sizeof(dma1->dma_hc.w))
				     % CACHE_SLINE_SIZE));
	ASSERT(sizeof(dma1->dma_hc.r) % 8 == 0);    /* so bzero() is safe */
#endif

	bzero(xi,sizeof(*xi));
	bzero(dma1, sizeof(*dma1));
	bzero(dma2, sizeof(*dma2));
	dki_dcache_wbinval(dma1, sizeof(*dma1));
	dki_dcache_wbinval(dma2, sizeof(*dma2));

#ifdef _NO_UNCACHED_MEM_WAR
	xi->xi_dma1_u = (struct xpi_dma1*)K0_TO_K1(dma1);
	xi->xi_dma1_c = dma1;
	xi->xi_dma2 = dma2;
#else
	xi->xi_dma1 = (struct xpi_dma1*)K0_TO_K1(dma1);
	xi->xi_dma2 = (struct xpi_dma2*)K0_TO_K1(dma2);
#endif
	xi->xi_if.if_unit = unit;
	xi->xi_gio = gio;
	xi->xi_slot = slot;

	s = splimp();
	xpi_infop[unit] = xi;

	xpi_init(xi);
	setgiovector(GIO_INTERRUPT_1, slot, if_xpiintr, unit);
	add_to_inventory(INV_NETWORK, INV_NET_FDDI, INV_FDDI_XPI, unit,
			 xi->xi_vers);

	rc = gio_hwgraph_lookup(edtp->e_base, &giovhdl);
	if (rc == GRAPH_SUCCESS) {
		char alias_name[8];
		sprintf(alias_name, "%s%d", EDGE_LBL_XPI, unit);
		sprintf(loc_str, "%s/%d", EDGE_LBL_XPI, unit);
		(void) if_hwgraph_add(giovhdl, loc_str, "if_xpi", alias_name,
			&xpivhdl);
	}

	if (SHOWCONFIG)
		printf(PF_L "present\n", PF_P(xi));

	unit++;
	splx(s);
}
#endif /* !EVEREST */
#endif /* IP20 || IP22 || IP26 || IP28 || EVEREST */
