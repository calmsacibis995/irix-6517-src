/* CHALLENGE 8-port Ethernet "E-Plex"
 *
 * Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
 *
 * This contents of this file must match the corresponding definitions
 *	in cmd/bsd/epfirm/firm/if_ep_s.h.
 */

#ifndef __IF_EP_H__
#define __IF_EP_H__
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.11 $"


/* EPB* and epb* have something to do with the entire board, while
 * EP* and ep* concern a single interface.
 * Except for user code, where EP and ep are used
 *
 * There is only one input stream, and it is associated with the
 * board instead of any single interface.
 */


#define EP_PORTS	8		/* 8 Ethernets/board */
#define EP_PORTS_OCT	10		/* to make unit numbers look octal */
#define EP_MAXBD	8		/* as many as 8 boards/system */

/* SGI IEEE ID, 8:0:69 */
#define SGI_MAC_SA0 0x08
#define SGI_MAC_SA1 0x00
#define SGI_MAC_SA2 0x69

/* checksum on MAC address in EEPROM */
#define EP_MAC_SUM(cksum,macp) { int macn;		\
    (cksum) = 0;					\
    for (macn = 0; macn < EP_MAC_SIZE/4; macn++)	\
	(cksum) += (macp)[macn];	\
}


/* The addresses of packets are 48 bits long, but addresses of control
 * structures are only 32 bits long.  It is good enough provided
 * the control structures are always allocated in the first 4 GByte.
 */
#define EP_CADDR   __uint32_t


/* initial 32 bits of message specifying the host communications area */
#define EP_GIO_DL_PREAMBLE    0x7ffffffe


/* shape of the board memory as far as the host knows
 */
#define EP_EPROM	0
#define EP_EPROM_SIZE	(128*1024)
#define EP_GIOSRAM	0x080000

/* use GIOSRAM starting here for downloading firmware */
#define	EP_DOWN		(EP_GIOSRAM + EP_EPROM_SIZE)

#define	EP_FIRM		0		/* the operational firmware */
#define	EP_FIRM_SIZE	(16*1024)

#define EP_DIAG		(EP_FIRM+EP_FIRM_SIZE)	/* diagnostics */
#define EP_DIAG_SIZE	(64*1024)

#define	EP_SPARE	(EP_DIAG+EP_DIAG_SIZE)	/* spare */
#define EP_SPARE_SIZE	(EP_MACS_ADDR-EP_SPARE)

#define	EP_MAC_SIZE	16		/* the MAC addresses live here */
#define EP_MACS_SIZE	(EP_MAC_SIZE*EP_PORTS)
#define	EP_MACS_ADDR	(EP_EPROM_SIZE - EP_MACS_SIZE)

/* the EEPROM programming code is put here */
#define EP_PROG_SIZE	(1024*2)
#define EP_PROG		(EP_DOWN-EP_PROG_SIZE)


/* The version number from the board has several parts:
 *	- date when firmware was built for backward compatiblity checks.
 *	- flag indicating bad EEPROM checksum.
 *
 *			    (((yy-92)*13 + m)*32 +  d)*24 + hr */
#define EP_VERS_MIN_DATE    (((95-92)*13 +10)*32 +  1)*24 +  0
#define EP_VERS_DATE_M	    0x0003ffff
#define EP_VERS_FAM	    0x10000000	/* detect incompatibility in future */
#define EP_VERS_FAM_M	    0x70000000
#define EP_VERS_FAM_S	    0x10000000
#define EP_VERS_CKSUM	    0x80000000	/* bad checksum */

/* These values are not in the version number from the board.  They are in
 * the firmware version put into the hardware inventory table.  Also
 * put into the hardware inventory table are the bits covered by
 * EP_VERS_DATE_M.
 */
#define EP_VERS_SLOT_S	    18
#define EP_VERS_SLOT_M	    0x003c0000
#define EP_VERS_ADAP_S	    22
#define EP_VERS_ADAP_M	    0x03c00000
#define EP_VERS_BAD_DANG    0x04000000

/* values for SIOC_EP_VERS */
struct ep_vers {
    __uint32_t	vers;			/* timestamp from the firmware */
};


/* special IOCTLs
 */
struct ep_dwn {
    __uint32_t  addr;			/* board address */
    __uint32_t	cnt;			/* number of values */
#define EP_DWN_LEN 8
#define EP_DWN_CLEN (EP_DWN_LEN*4)
    union {				/* values */
	__uint32_t  l[EP_DWN_LEN];
	u_char	    c[EP_DWN_CLEN];
    } val;
};
#define SIOC_EP_STO	_IOW('P', 1, struct ep_dwn)
#define SIOC_EP_EXEC	_IOW('P', 2, struct ep_dwn)
#define SIOC_EP_FET	_IOWR('P',3, struct ep_dwn)
#define SIOC_EP_VERS	_IOR('P', 4, struct ep_vers)
#define SIOC_EP_SIGNAL	_IOW('P', 5, __uint32_t)
#define SIOC_EP_POKE	_IOW('P', 6, struct ep_dwn)

#define EP_SIGNAL_RESET 0x8000


#ifdef _KERNEL

#define EP_MAX_OUTQ 64			/* max output queue length */

/* shape of the board GIO bus as seen by the host
 */
#define	EPB_GIO_FLAG0	    0x0200
#define	EPB_GIO_FLAG1	    (2*EPB_GIO_FLAG0)
#define	EPB_GIO_FLAG2	    (4*EPB_GIO_FLAG0)
#define	EPB_GIO_FLAG3	    (8*EPB_GIO_FLAG0)
#define EPB_GIO_FLAG_MASK   (0xf*EPB_GIO_FLAG0)
#define	EPB_GIO_H2B_INTR    (16*EPB_GIO_FLAG0)	/* to interrupt on board */

#define	EPB_GIO_B2H_CLR	    EPB_GIO_FLAG3   /* clear board->host intr */

struct ep_out {				/* an output packet */
    ushort	    ep_out_pad;
    struct etheraddr ep_out_dst;
    struct etheraddr ep_out_src;
    ushort	    ep_out_type;
};

union epb_in {				/* start of an input packet */
	struct epb_in_raw {
	    u_char	    epb_raw_pad[sizeof(struct ifheader)
					+sizeof(struct snoopheader)
					-4];	/* overlap epb_raw_ctl */
	    struct epb_raw_ctl {
		ushort	    epb_ctl_pad;
		ushort	    epb_ctl_cksum;
		ushort	    epb_ctl_stat;
	    } epb_raw_ctl;
	} epb_in_raw;
	struct {
	    struct ifheader ep_data_ifh;
	    struct snoopheader ep_data_snoop;
	    ushort	    ep_data_pad;
	    struct ether_header ep_data_eh;
	} epb_in_data;
};
#define epb_in_start	epb_in_raw.epb_raw_ctl
#define epb_in_cksum	epb_in_raw.epb_raw_ctl.epb_ctl_cksum
#define epb_in_stat	epb_in_raw.epb_raw_ctl.epb_ctl_stat
#define epb_in_ifh	epb_in_data.ep_data_ifh
#define epb_in_snoop	epb_in_data.ep_data_snoop
#define epb_in_eh	epb_in_data.ep_data_eh

/* bits in epb_in_stat */
#define EPB_IN_CRC	0x08		/* CRC error in received packet */

#define EPB_PAD	sizeof(struct epb_raw_ctl)  /* input padding by board */
#define EPB_RBUF_SIZE	(1514+8)	/* packet size used by board */
#define EPB_RPKT_NET_SIZE EPB_RBUF_SIZE	/* normal packet without CRC */
#define EPB_RPKT_SIZE	(EPB_RBUF_SIZE+4)   /* babble packet */


/* The host and board communicate mostly with a common area in host memory.
 * The board reads the first part and writes the second part while the host
 * does the opposite.
 *
 */

/* host-to-board commands
 *	values for epb_hc.cmd
 */
enum epb_cmd {
	ECMD_NOP=0,			/* 00	do nothing */
	ECMD_INIT,			/* 01	set parameters */

	ECMD_STO,			/* 02	download to the board */
	ECMD_EXEC,			/* 03	execute downloaded code */
	ECMD_FET,			/* 04	upload from the board */

	ECMD_MAC_PARMS,			/* 05	set MAC  */
	ECMD_MAC_OFF,			/* 06	turn off the SONIC */

	ECMD_WAKEUP,			/* 07	wake up C2B DMA */

	ECMD_FET_CNT,			/* 08	fetch counters */

	ECMD_POKE			/* 09	poke memory on the board */
};


struct epb_stats {			/* ECMD_FET_CNT */
    struct epb_counts {
	__uint32_t  rx_mp;		/* missed packets */
	__uint32_t  rx_hash;		/* failed to match hash */
	__uint32_t  rx_col;		/* receive collisions */
	__uint32_t  rx_babble;		/* packet too long */
	__uint32_t  rx_rfo;		/* receive FIFO overrun */
	__uint32_t  rx_rde;		/* receive descriptors exhausted */
	__uint32_t  rx_rbe;		/* receive buffers exhausted */
	__uint32_t  rx_rbae;		/* receive buffer area exhausted */
	__uint32_t  tx_col;		/* transmit collisions */
	__uint32_t  tx_hbl;		/* heartbeat missing */
	__uint32_t  tx_exd;		/* excessive deferrals */
	__uint32_t  tx_def;		/* deferrals */
	__uint32_t  tx_crsl;		/* no carrier */
	__uint32_t  tx_exc;		/* excessive collisions */
	__uint32_t  tx_owc;		/* late collisions */
	__uint32_t  tx_pmb;		/* transmission error */
	__uint32_t  tx_fu;		/* FIFO underrun */
	__uint32_t  tx_bcm;		/* byte count mismatch */
	__uint32_t  tx_error;		/* any transmitt error */
	__uint32_t  links;		/* LINK bits */
	__uint32_t  br;			/* bus retries */
	__uint32_t  pad21;
	__uint32_t  pad22;
	__uint32_t  pad23;
	__uint32_t  pad24;
	__uint32_t  pad25;
	__uint32_t  pad26;
	__uint32_t  pad27;
	__uint32_t  pad28;
	__uint32_t  pad29;
	__uint32_t  pad30;
	__uint32_t  pad31;
    } cnts[EP_PORTS];
};



struct epb_hc {
    /* host-written/board-read portion of the communcations area
     */
    __uint32_t	cmd;			/* one of the epb_cmd enum */
    __uint32_t	cmd_id;			/* ID used by board to ack cmd */

    union {
	char	    cmd_data[4*16];	/* minimize future version problems */

	struct ep_dwn dwn;		/* ECMD_STO, ECMD_EXEC, ECMD_FET */

	__uint32_t  exec;		/* ECMD_EXEC, goto downloaded code */

	struct {			/* ECMD_INIT */
	    EP_CADDR	b2h_buf;	/* B2H buffer */
	    __uint32_t	b2h_len;	/* B2H_LEN */
	    EP_CADDR	h2b_buf;
	    __uint32_t	h2b_dma_delay;	/* ticks between H2B DMA */
	    __uint32_t  h2b_sleep;	/* ticks to keep H2B DMA awake */
	    __uint32_t  int_delay;	/* ticks b2h interrupt hold-off */
	    ushort	max_bufs;	/* maximum total mbufs */
	    ushort	sml_size;	/* EPB_SML_SIZE */
	} init;

	struct {			/* ECMD_MAC_PARMS */
	    __uint32_t	port;
#		define	MMODE_HLEN  (256/8)
	    u_char	hash[MMODE_HLEN];   /* multicast hash bits */
	    ushort	mode;
#		define	MMODE_PROM   0x1    /*	promiscuous */
#		define	MMODE_AMULTI 0x2    /*	all multicasts for routing */
	    u_char	addr[6];	/* MAC address */
	} mac;

	EP_CADDR    cntptr;		/* ECMD_FET_CT, addr of counters */
    } arg;



    /* host-read/board-written part of the communications area
     */

    volatile __uint32_t	sign;		/* board signature */
#define EPB_SIGN	0xe8e8f00d

    volatile __uint32_t	vers;		/* board/eprom version */

    volatile __uint32_t	cmd_ack;	/* ID of last completed command */

    volatile union {
	__uint32_t  dwn_l[EP_DWN_LEN];	/* ECMD_FET */
	u_char	    dwn_c[EP_DWN_CLEN];
    } res;
};


/* board-to-host control
 */
struct epb_b2h {
    unsigned	b2h_sernum:8;
    unsigned	b2h_op:4;
    unsigned	b2h_port:4;
    ushort	b2h_val:16;
};
#define EPB_B2H_SLEEP	1		/* board has run out of work */
#define EPB_B2H_ODONE	2		/* output DMA commands finished */
#define EPB_B2H_IN_SML	3		/* input small packet */
#define EPB_B2H_IN_BIG	4		/* input big packet */

/* big enough for
 *	all available mbufs to be filled with frames
 *	all pending output to be completed and reported separately
 *	some complaints about going to sleep
 *	padding to ensure the length is not a multiple of the period of
 *	    the serial number so the serial number always changes
 */
#define EPB_B2H_LEN	((EPB_MAX_SML + EPB_MAX_BIG    \
			  + EP_MAX_OUTQ*EP_PORTS+1 + 2) | 1)

#define EPB_MAX_SML	(32*EP_PORTS)	/* little mbufs in pool */
#define EPB_MAX_BIG	(32*EP_PORTS)	/* clusters in pool */

#define EPB_SML_SIZE	(MLEN-sizeof(struct epb_in_raw))


/* host-to-board control
 */
struct epb_h2b {
    u_char  epb_h2b_op;
    u_char  epb_h2b_addr_hi;		/* 8 bits of EBus address */
    u_short epb_h2b_len;
    union {
	__uint32_t addr;
	struct {
	    u_short off;
	    u_short sum;
	} ck;
    } epb_h2b_u;
};
#define epb_h2b_addr epb_h2b_u.addr
#define epb_h2b_totlen epb_h2b_len	/* total length of output */
#define epb_h2b_offsum epb_h2b_u.ck.off	/* put it here or 0 */
#define epb_h2b_cksum epb_h2b_u.ck.sum	/* start with this value or 1 */
#define epb_h2b_outlen epb_h2b_len	/* length of this chunk */

#define EPB_H2B_EMPTY	0
#define	EPB_H2B_WRAP    1		/* back to start of buffer */
#define EPB_H2B_SML	2		/* add little mbuf to pool */
#define EPB_H2B_BIG	3		/* add cluster to pool */
#define EPB_H2B_O	4		/* start output (dup EP_PORTS) */
#define EPB_H2B_O_CONT	(EPB_H2B_O+EP_PORTS)	/* continue output */
#define EPB_H2B_O_END	(EPB_H2B_O_CONT+1)  /* last of output */

#define EP_MAX_DMA_ELEM 3		/* max segments/data output DMA */

/* big enough to never be short of space
 *	max input buffer pool
 *	+ max output buffer queues
 *	+ sleep note
 *	+ marker
 *	+ spare when wrapping
 */
#define EPB_H2B_LEN	(EPB_MAX_SML + EPB_MAX_BIG	    \
			 + EP_MAX_OUTQ*(EP_MAX_DMA_ELEM+1)*EP_PORTS  \
			 + 1 + 1 + 1)

#if EP_MAX_OUTQ > 64
? error ? error ?			/* fix the firmware */
#endif

#endif /* _KERNEL */
#ifdef __cplusplus
}
#endif
#endif /* __IF_EP_H__ */
