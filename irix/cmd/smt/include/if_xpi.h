/* GIO bus FDDI board
 *
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 * This contents of this file must match the corresponding definitions
 *	in cmd/smt/apps/xpi/.../if_xpi_s.h.
 */

#ifndef __IF_XPI_H__
#define __IF_XPI_H__
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.32 $"


#define XPI_MAX_OUTQ 64			/* max output queue length */

/* shape of the board GIO bus as seen by the host
 */
#define	XPI_GIO_RESET	    0x0100	/* set to reset board (latch) */
#define	    XPI_GIO_RESET_DELAY 200	/*	usec delay for reset */
#define	XPI_GIO_FLAG0	    0x0200
#define	XPI_GIO_FLAG1	    0x0400
#define	XPI_GIO_FLAG2	    0x0800
#define	XPI_GIO_FLAG3	    0x1000
#define XPI_GIO_FLAG_MASK   0x1e00
#define	XPI_GIO_H2B_INTR    0x2000	/* write to create intr on board */

#define	XPI_GIO_B2H_CLR	    XPI_GIO_FLAG3   /* clear board->host intr */

/* initial 32 bits of message specifying the host communications area */
#define XPI_GIO_DL_PREAMBLE    0x7ffffffe


/* shape of the board memory as far as the host knows
 */
#define XPI_EPROM	0
#define XPI_EPROM_SIZE	(128*1024)
#define XPI_SRAM	0x400000

/* use SRAM starting here for downloading firmware */
#define	XPI_DOWN	(XPI_SRAM + XPI_EPROM_SIZE)

#define	XPI_FIRM	0
#define	XPI_FIRM_SIZE	(16*1024)

/* the MAC address lives here */
#define	XPI_MAC_SIZE	16
#define	XPI_MAC_ADDR	(XPI_EPROM_SIZE - XPI_MAC_SIZE)

/* the host driver is stored here */
#define	XPI_DRV		(XPI_FIRM + XPI_FIRM_SIZE)
#define	XPI_DRV_SIZE	(XPI_EPROM_SIZE - XPI_FIRM_SIZE - XPI_MAC_SIZE)

/* the EEPROM programming code is put here */
#define XPI_PROG_SIZE	(1024*2)
#define XPI_PROG	(XPI_DOWN-XPI_PROG_SIZE)


/* Motorola FDDI chip definitions.
 *	These should be moved to another file if there is every any
 *	other code that uses them.
 */
#define FSI_CAM_LEN	32		/* 32 entries */
#define	FSI_CAM_CONST	0xb900		/* 48-bit */
#define FSI_CAM_VALID	0x0080		/* valid entry */
struct fsi_cam {
	ushort	fsi_id;
	ushort	fsi_entry[3];
};

/* MAC packet request headers
 *	These bits match the PRH_* bits defined in the firmware.
 */
#define MAC_DATA_HDR0	0x30		/* any token */
#define MAC_DATA_HDR1	0x28		/* append CRC, unrestricted token */
#define MAC_DATA_HDR2	0x00

#define MAC_BEACON_HDR0	0x01		/* BCN_FRAME */
#define MAC_BEACON_HDR1	0x20		/* append CRC */
#define MAC_BEACON_HDR2	0x00



/* The host and board communicate mostly with a common area in host memory.
 * The board reads the first part and writes the second part while the host
 * does the opposite.
 *
 */


/* host-to-board commands
 *	values for xpi_hc.cmd
 */
enum xpi_cmd {
	XCMD_NOP=0,			/* 00	do nothing */
	XCMD_INIT,			/* 01	set parameters */
	XCMD_STO,			/* 02	download to the board */
	XCMD_EXEC,			/* 03	execute downloaded code */
	XCMD_FET,			/* 04	upload from the board */

	XCMD_REP,			/* 05	repeat states & disarm PHY */
	XCMD_SETLS0,			/* 06	ELM0 transmitted line state */
	XCMD_SETLS1,			/* 07 */
	XCMD_PC_OFF0,			/* 08	clear ELM PCM */
	XCMD_PC_OFF1,			/* 09 */
	XCMD_PC_START0,			/* 0a	start ELM PCM */
	XCMD_PC_START1,			/* 0b */
	XCMD_SETVEC0,			/* 0c	signal some bits */
	XCMD_SETVEC1,			/* 0d */
	XCMD_LCT0,			/* 0e	start LCT */
	XCMD_LCT1,			/* 0f */
	XCMD_PC_TRACE0,			/* 10	start PC_TRACE */
	XCMD_PC_TRACE1,			/* 11 */
	XCMD_JOIN0,			/* 12	finish signaling */
	XCMD_JOIN1,			/* 13 */
	XCMD_WRAPA,			/* 14 */
	XCMD_WRAPB,			/* 15 */
	XCMD_THRU,			/* 16 */

	XCMD_BYPASS_INS,		/* 17	"insert" the bypass */
	XCMD_BYPASS_REM,		/* 18	turn off the bypass */

	XCMD_MAC_MODE,			/* 19	set MAC mode */
	XCMD_CAM,			/* 1a	set FSI CAM */
	XCMD_MAC_PARMS,			/* 1b	set MAC parameters */
	XCMD_MAC_OFF,			/* 1c	turn off the MAC */
	XCMD_D_BEC,			/* 1d	start directed beacon */

	XCMD_PINT_ARM,			/* 1e	arm PHY interrupts */
	XCMD_WAKEUP,			/* 1f	wake up C2B DMA */

	XCMD_FET_LS0,			/* 20	fetch line ELM0 state */
	XCMD_FET_LS1,			/* 21 */
	XCMD_FET_CNT			/* 22	fetch counters */
};


struct xpi_counts {			/* XCMD_FET_CNT */
    __uint32_t	ticks;			/* free running usec timer */

    __uint32_t	rngop;			/* I	non-RingOP to RingOP */
    __uint32_t	rngbroke;		/* I	RingOP to non-RingOP */
    __uint32_t	pos_dup;		/*	our DA with A bit set */
    __uint32_t	misfrm;
    __uint32_t	xmtabt;
    __uint32_t	tkerr;			/*	duplicate token */
    __uint32_t	clm;			/* I */
    __uint32_t	bec;			/* I */
    __uint32_t	tvxexp;			/* I */
    __uint32_t	trtexp;			/* I */
    __uint32_t	tkiss;			/*	tokens issued */
    __uint32_t	myclm;
    __uint32_t	loclm;
    __uint32_t	hiclm;
    __uint32_t	mybec;
    __uint32_t	otrbec;

    __uint32_t	frame_ct;		/*	total frames seen */
    __uint32_t	tok_ct;			/*	token count */
    __uint32_t	err_ct;			/*	locatable errors */
    __uint32_t	lost_ct;		/*	format errors */

    __uint32_t	lem0_ct;		/*	LEM "events" from ELM0 */
    __uint32_t	lem1_ct;

    __uint32_t	vio0;			/*	violation symbols from ELM0 */
    __uint32_t	vio1;
    __uint32_t	eovf0;			/*	elasticity buffer overflows */
    __uint32_t	eovf1;
    __uint32_t	elm0st;			/*	ELM status bits */
    __uint32_t	elm1st;

    __uint32_t	error;			/*	error detected by local MAC */
    __uint32_t	e;			/*	received E bit */
    __uint32_t	a;			/*	received A bit */
    __uint32_t	c;			/*	received C bit */
    __uint32_t	rx_ovf;			/*	receive FIFO overrun */
    __uint32_t	tx_under;		/*	transmit FIFO underrun */

    __uint32_t	noise0;			/* I	noise events from ELM0 */
    __uint32_t	noise1;			/* I */

    __uint32_t	tot_junk;		/*	discarded frames */
    __uint32_t	shorterr;		/*	frame too short */
    __uint32_t	longerr;		/*	frame too long */

    enum mac_states mac_state:32;	/*	current MAC state */

    __uint32_t	dup_mac;		/*	likely duplicate address */

    __uint32_t	rnglatency;		/*	measured ring latency */
    __uint32_t	t_neg;			/*	recent T_Neg */

    __uint32_t	mac_bug;		/* I	firmware bugs */
    __uint32_t	elm_bug;		/* I */
    __uint32_t	fsi_bug;		/* I */
    __uint32_t	mac_cnt_ovf;		/*	too much interrupt latency */

    __uint32_t	early_rels;		/*	released token early */

/* I: interesting event which causes an interrupt.
 *	When an interrupt is required by a change in one of these counters,
 *	the interrupt will occur the next time the board interrupt is enabled,
 *	regardless of whether the host has since asked that the counters
 *	be updated.
 */
};



/* host-written/board-read portion of the communcations area
 */
struct xpi_hcw {

    __uint32_t	cmd;			/* one of the xpi_cmd enum */
    __uint32_t	cmd_id;			/* ID used by board to ack cmd */

    union {
	char	    cmd_data[64];	/* minimize future version problems */

	struct xpi_dwn {		/* XCMD_STO, XCMD_EXEC, XCMD_FET */
	    __uint32_t	addr;		/* board address */
	    __uint32_t	cnt;		/* number of values */
	    union {			/* values */
#		define XPI_DWN_LEN 8
		__uint32_t  l[XPI_DWN_LEN];
		u_char	    c[XPI_DWN_LEN*4];
	    } val;
	} dwn;

	__uint32_t  exec;		/* XCMD_EXEC, goto downloaded code */

	struct {			/* XCMD_INIT */
	    __uint32_t	flags;
#		define	    XPI_IN_CKSUM 1  /* board IP cksums */
#		define	    XPI_IN_CKSUM_SIZE 4096
#		define	    XPI_LAT_VOID 2  /* enable latency measures */
#		define	    XPI_ZAPRING  4  /* mess up the ring */
#		define	    XPI_IO4IA_WAR 8	/* combat IO4 IA chip bug */
	    __uint32_t  b2h_buf;	/* B2H buffer */
	    __uint32_t  b2h_len;	/* B2H_LEN */
	    __uint32_t  max_bufs;	/* maximum total mbufs */
	    __uint32_t  d2b_buf;
	    __uint32_t  d2b_buf_lim;
	    __uint32_t  c2b_buf;
	    __uint32_t  c2b_dma_delay;
	    __uint32_t  int_delay;
	    __uint32_t  max_host_work;
	    __uint32_t  c2b_sleep;
	    __uint32_t  ctptr;		/* counters for XCMD_FET_CNT */
	    ushort	rel_tok;	/* early token release rate */
	    ushort	sml_size;	/* MLEN */
	    ushort	pad_in_dma;	/* sizeof(FDDI_IBUF) */
	} init;

	__uint32_t  ls;			/* XCMD_SETLS* */

	__uint32_t  vec_n;		/* XCMD_SETVEC* */

	struct {			/* XCMD_MAC_MODE */
	    __uint32_t  mode;
#		define	MMODE_PROM  0x1	/*	promiscuous */
#		define	MMODE_AMULTI 0x2    /*	all multicasts for routing */
#		define	MMODE_HMULTI 0x4    /*	CAM is full or want all */
#		define	MAX_MULTI_CAM 32    /*	max before all-multi */
#		define	MMODE_HLEN  (256/8)
	    u_char  hash[MMODE_HLEN];	/* multicast hash bits */
	} mac;

	struct {			/* XCMD_MAC_PARMS */
	    u_char	addr[6];	/*	long address */
	    __uint32_t  t_req;
	    __uint32_t	r15;		/*	TVX and T_Max */
	} mac_parms;

	__uint32_t  camp;		/* XCMD_CAM FSI CAM entries */

	struct {			/* XCMD_D_BEC */
	    __uint32_t	len;
	    struct d_bec_frame msg;
	} d_bec;
    } arg;
};


/* host-read/board-written part of the communications area
 */
struct xpi_hcr {
    volatile __uint32_t	sign;		/* board signature */
#define XPI_SIGN	0xfdd1f00d
    volatile __uint32_t	vers;		/* board/eprom version */
    /*			  ((((yy-92)*13 + m)*32 +  d)*24 + hr)*60 + minute */
#define XPI_VERS_MIN	  ((((96-92)*13 + 1)*32 +  9)*24 +  0)*60 + 0
#define XPI_VERS_M	0x00ffffff
#define XPI_VERS_FAM_M	0x07000000	/* detect incompatibility */
#define XPI_VERS_FAM	0x02000000	/* January, 1996 */
#define XPI_VERS_DANG	0x08000000	/* using DANG chip */
#define XPI_VERS_PHY2	0x10000000	/* DAS */
#define XPI_VERS_NO_PHY	0x20000000	/* incomplete hardware */
#define XPI_VERS_BYPASS	0x40000000	/* have an optical bypass */
#define XPI_VERS_CKSUM	0x80000000	/*	bad checksum */

    /* These values are not in the word from the board.  They are in
     * the firmware version put into the hardware inventory table.
     */
#define XPI_VERS_MEZ_M	0x0003ffff
#define XPI_VERS_MEZ_S	60
#define XPI_VERS_SLOT_S 18
#define XPI_VERS_SLOT_M	0x003c0000	/* slot containing IO4 */
#define XPI_VERS_ADAP_S 22
#define XPI_VERS_ADAP_M 0x01c00000


    volatile __uint32_t inum;		/* ++ on board-to-host interrupt */

    volatile __uint32_t	cmd_ack;	/* ID of last completed command */

    volatile union {
	__uint32_t  ls;			/* XCMD_FET_LS* */
	u_char	    dwn[XPI_DWN_LEN*4];	/* XCMD_FET */
    } res;
};


#define XPI_MAC_SUM(cksum,macp) { int macn;		\
    (cksum) = 0;					\
	for (macn = 0; macn < XPI_MAC_SIZE/4; macn++)	\
	    (cksum) += ((__uint32_t*)(macp))[macn];	\
}

/* board-to-host DMA
 */
struct xpi_b2h {
	u_char	b2h_sn;
	u_char	b2h_op;
	ushort	b2h_val;
};
#define XPI_B2H_SLEEP	1		/* board has run out of work */
#define XPI_B2H_ODONE	2		/* output DMA commands finished */
#define XPI_B2H_IN	3		/* input DMA */
#define XPI_B2H_IN_DONE	4
#define XPI_B2H_PHY	5		/* PHY interrupt */

/* received line states, a superset of pcm_ls
 */
enum xi_pcm_ls {
    RCV_TRACE_PROP  =8,
    RCV_SELF_TEST   =9,
    RCV_BREAK	    =10,
    RCV_PCM_CODE    =11
};

#define XPI_MAX_DMA_ELEM 7		/* max segments/data output DMA */
#define XPI_PHY_INT_NUM	12		/* max queued line state changes */

/* big enough for
 *	all available mbufs to be filled with frames.
 *	all pending output to be completed and reported separately, times
 *		two for historical reasons and since we cannot change it
 *		now without being unable to update the firmware on old
 *		boards.
 *	full list of PHY interrupts.
 *	some complaints about going to sleep.
 *	padding to an odd number to ensure the length is not a multiple
 *		of the period of the serial number so the serial number
 *		always changes.
 */
#define XPI_B2H_LEN	((XPI_MAX_SML + XPI_MAX_MED + XPI_MAX_BIG	    \
			  + 1 + XPI_MAX_OUTQ*2+1 + XPI_PHY_INT_NUM + 2) | 1)

#define	XPI_PADMAX_IN_DMA 32		/* input padding added by board */


#define XPI_MAX_BIG	32		/* clusters in pool */
#define XPI_MAX_MED	64		/* medium sized mbufs in pool */
#define XPI_MAX_SML	64		/* little mbufs in pool */
#define XPI_BIG_SIZE	4096		/* size of big mbufs */
#define XPI_MED_SIZE	(1514+(24-14)+XPI_PADMAX_IN_DMA)   /* (Ethernet) */
#define XPI_SML_SIZE	MLEN

/* host-control-to-board DMA
 */
struct xpi_c2b {
#ifdef EVEREST
    u_char	c2b_addr_hi;		/* 8 bits of EBus address */
    u_char	pad;
    ushort	c2b_op;
#else
    __uint32_t  c2b_op;
#endif
    __uint32_t	c2b_addr;
};
#define XPI_C2B_EMPTY   (0*4)
#define XPI_C2B_SML	(1*4)		/* add little mbuf to pool */
#define XPI_C2B_MED	(2*4)		/* add medium mbuf to pool */
#define XPI_C2B_BIG	(3*4)		/* add cluster to pool */
#define XPI_C2B_PHY_ACK (4*4)		/* finished PHY interrupt */
#define	XPI_C2B_WRAP    (5*4)		/* back to start of buffer */
#define	XPI_C2B_IDELAY  (6*4)		/* delay interrupts */

/* big enough to never be short of space
 *	max buffer pool + N*PHY int acks + sleep note + marker
 */
#define XPI_C2B_LEN	(XPI_MAX_SML + XPI_MAX_MED + XPI_MAX_BIG \
			 + XPI_PHY_INT_NUM + 1 + 1)

/* host-data-to-board DMA
 */
union xpi_d2b {
    struct {
#ifdef EVEREST
	u_short	    addr_hi;		/* 8 bits of EBus address */
	u_short	    len;		/* length of chunk */
#else
	__uint32_t  len;		/* length of chunk */
#endif
	__uint32_t  addr;		/* host address */
    } sg;
    struct {
	u_short start;			/* start-of-request flag */
	u_short	totlen;			/* total frame length */
	u_char	offsum;			/* put it here or 0 */
	u_char	startsum;		/* start checksum here or 0 */
	u_short cksum;			/* start with this value or 1 */
    } hd;
};
#define XPI_D2B_RDY	0x8000		/* in first hd.start of DMA string */
#define XPI_D2B_BAD	0xc000		/* start of un-ready DMA string */

#define XPI_D2B_LEN	(XPI_MAX_OUTQ*(XPI_MAX_DMA_ELEM+1))

#if XPI_MAX_OUTQ != 64
? error ? error ?			/* fix the firmware */
#endif

/* values for SIOC_XPI_VERS */
struct xpi_vers {
    __uint32_t	vers;			/* timestamp from the firmware */
    u_short	type;			/* board type */
};
#define XPI_TYPE_LC	0		/* boards that use "LC" firmware */
#define XPI_TYPE_MEZ_S	2		/* single-port Everest mezzanine */
#define XPI_TYPE_MEZ_D	3		/* dual-port Everest mezzanine */


/* special IOCTLs
 */
#define SIOC_XPI_STO	_IOW('P', 1, struct xpi_dwn)
#define SIOC_XPI_EXEC	_IOW('P', 2, struct xpi_dwn)
#define SIOC_XPI_FET	_IOWR('P',3, struct xpi_dwn)
#define SIOC_XPI_VERS	_IOR('P', 4, struct xpi_vers)
#define SIOC_XPI_SIGNAL	_IOW('P', 5, __uint32_t)
#define XPI_SIGNAL_RESET 0x8000

#ifdef __cplusplus
}
#endif
#endif /* __IF_XPI_H__ */
