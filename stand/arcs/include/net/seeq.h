#ident	"include/net/seeq.h:  $Revision: 1.13 $"

/*
 * Copyright 1989 Silicon Graphics, Inc. All rights reserved.
 *
 * Definitions for the SEEQ 8003 chip when interfaced to the HPC1/HPC3.
 */


/* maximum & minimum packet sizes */
#define EHDR_LEN	14
#define MAX_TPKT	(EHDR_LEN+1500)	    /* max xmit len (seeq adds crc) */
#define MIN_TPKT	60		    /* min xmit len, unchained pkts */
#define HPC_RSPACE	3		    /* 2 byte offset + 1 status */
#define MAX_RPKT	(MAX_TPKT+HPC_RSPACE)	/* max receive length */

/* SEEQ 8003 external register definitions */

/* receive status register */
#define SEQ_RS_OLD	0x80		/* old/new status, set on reg read */
#define SEQ_RS_GOOD	0x20		/* good frame */
#define SEQ_RS_END	0x10		/* received end of frame */
#define SEQ_RS_SHORT	0x08		/* received short frame */
#define SEQ_RS_DRBL	0x04		/* dribble error */
#define SEQ_RS_CRC	0x02		/* CRC error */
#define SEQ_RS_OFLOW	0x01		/* overflow error */

#if IP22 || IP26 || IP28
#define RCVSTAT_SHIFT		0
#define HPC_RBO			0x800	/* receive buffer overflow */
#define HPC_STRCVDMA_MASK	0x400	/* receive channel active mask */
#define HPC_STRCVDMA		0x200	/* receive dma started */
#define	HPC_RCV_ENDIAN_LITTLE	0x100	/* receive dma is little endian */
#define	SEQ_RS_LATE_RXDC	0x40	/* combine with status_5_0 (all 0's)
					   to signify late receive */
#define	SEQ_RS_TIMEOUT		0x40	/* combine with the status_4 (1) to 
					   signify timeout */
#define	SEQ_RS_STATUS_5_0	0x3f	/* mask for the 6 lowest status bits */ 
#else
#define RCVSTAT_SHIFT		8
#define HPC_STRCVDMA		0x4000	/* receive dma started */
#endif	/* IP22 || IP26 || IP28 */

/* receive command register */
#define SEQ_RC_RNONE	0x0		/* disable seeq receive */
#define SEQ_RC_RSMB	0xc0		/* recv station/broadcast/multicast */
#define SEQ_RC_RSB	0x80		/* recv station/broadcast */
#define SEQ_RC_RALL	0x40		/* receive all frames */
#define SEQ_RC_INTGOOD	0x20	 	/* interrupt on good frames */
#define SEQ_RC_INTEND	0x10		/* interrupt on end of frame */
#define SEQ_RC_INTSHORT	0x08		/* interrupt on short frame */
#define SEQ_RC_INTDRBL	0x04		/* interrupt on dribble error */
#define SEQ_RC_INTCRC	0x02		/* interrupt on CRC error */
#define SEQ_RC_OFLOW	0x01		/* interrupt on overflow error */

/* xmit status register */
#if IP22 || IP26 || IP28
#define HPC_STTRDMA_MASK	0x400	/* xmit channel active mask */
#define HPC_STTRDMA		0x200	/* xmit dma started */
#define	HPC_TR_ENDIAN_LITTLE	0x100	/* transmit dma is little endian */
#define SEQ_XS_OLD		0x80	/* old/new status, set on reg read */
#define	SEQ_XS_LATE_COLL	0x10	/* late collision */
#define SEQ_XS_SUCCESS		0x08	/* xmit success */
#define SEQ_XS_16TRY		0x04	/* 16 retries */
#define SEQ_XS_COLL		0x02	/* collision */
#define SEQ_XS_UFLOW		0x01	/* underflow */
#else
#define SEQ_XS_OLD	(0x80 << 16)	/* old/new status, set on reg read */
#define HPC_STTRDMA	(0x40 << 16)	/* xmit dma started */
#define SEQ_XS_SUCCESS	(0x08 << 16)	/* xmit success */
#define SEQ_XS_16TRY	(0x04 << 16)	/* 16 retries */
#define SEQ_XS_COLL	(0x02 << 16)	/* collision */
#define SEQ_XS_UFLOW	(0x01 << 16)	/* underflow */
#endif	/* IP22 || IP26 || IP28 */

/* on new EDLC only */
#define	SEQ_XS_NO_SQE		0x01
#define	SEQ_XS_NO_CARRIER	0x02

/* xmit command register */
#define	SEQ_XC_REGBANK0	0x00	/* access reg bank 0 incl station addr */
#define	SEQ_XC_REGBANK1	0x20	/* access reg bank 1 incl mcast-lsb */
#define	SEQ_XC_REGBANK2	0x40	/* access reg bank 2 incl mcast-msb & ctl */
#define SEQ_XC_INTGOOD	0x08	/* interrupt on good xmit */
#define SEQ_XC_INT16TRY	0x04	/* interrupt on 16 retries */
#define SEQ_XC_INTCOLL	0x02	/* interrupt on collision */
#define SEQ_XC_INTUFLOW	0x01	/* interrupt on underflow */

/* control register */
#define	SEQ_CTL_NOCARR	0x20	/* enable/clear no carrier on xmit flag */
#define	SEQ_CTL_SHORT	0x10	/* enable/disable recv frame < 14 bytes */
#define	SEQ_CTL_MULTI	0x08	/* enable/disable hash multicast */
#define	SEQ_CTL_SQE	0x04	/* enable/clear sqe flag */
#define	SEQ_CTL_ACOLL	0x02	/* enable/clear all coll counter */
#define	SEQ_CTL_XCOLL	0x01	/* enable/clear xmit coll counter */

/* HPC1/3 ethernet reg definitions */

/* HPC enet control register */
#if IP22 || IP26 || IP28
#define HPC_MODNORM	0x0		/* mode: 0=normal, 1=loopback */
#define	HPC_FIX_INTR	0x8000		/* start timeout counter after */
#define	HPC_FIX_EOP	0x4000		/* rcv_eop_intr/eop_in_chip is set */ 
#define	HPC_FIX_RXDC	0x2000		/* clear eop status upon rxdc */
#define	HPC_INTERGAP	0x1000		/* enable writing of intergap reg */
#else
#define HPC_RBO		0x08		/* receive buffer overflow */
#define HPC_MODNORM	0x04		/* mode: 0=loopback, 1=normal */
#endif	/* IP22 || IP26 || IP28 */

#define HPC_INTPEND	0x02		/* interrupt pending, write 1 clears */
#define HPC_ERST	0x01		/* ethernet channel reset */

#if IP20
typedef volatile struct xd {
#ifdef	_MIPSEB
	volatile u_int
		x_eoxp:1,
		:15,
		x_xie:1,
		:2,
		x_xbcnt:13;
	volatile u_int
		x_eox:1,
		:3,
		x_xbufptr:28;
#else	/* _MIPSEL */
	volatile u_int
		x_xbcnt:13,
		:2,
		x_xie:1,
		:15,
		x_eoxp:1;
	volatile u_int
		x_xbufptr:28,
		:3,
		x_eox:1;
#endif	/* _MIPSEL */
	volatile u_int x_nxdesc;		/* (physical) struct xd * */
	__uint32_t x_nxpkt;
#if _MIPS_SIM == _ABI64
	__uint32_t x_pad0;
	struct mbuf *x_mbuf;
	__uint32_t x_pad1[2];
#else
	struct mbuf *x_mbuf;
#endif
} xd_desc;

typedef volatile struct rd {
#ifdef	_MIPSEB
	volatile u_int
		r_rown:1,
		:18,
		r_rbcnt:13;
	volatile u_int
		r_eor:1,
		:3,
		r_rbufptr:28;
#else	/* _MIPSEL */
	volatile u_int
		r_rbcnt:13,
		:18,
		r_rown:1;
	volatile u_int
		r_rbufptr:28,
		:3,
		r_eor:1;
#endif	/* _MIPSEL */
	volatile u_int r_nrdesc;	/* (physical) struct rd * */
#if _MIPS_SIM == _ABI64
	__uint32_t r_pad0;
	struct mbuf *r_mbuf;
	__uint32_t r_pad1[2];
#else
	struct mbuf *r_mbuf;
#endif
} rd_desc;

/*
 * HPC enet register mapping
 */
typedef struct {
	u_int	fill1;
	u_int	fill2;

	volatile u_int	xcount;		/* hpc debug reg */
	volatile u_int	cxbp;		/* current xmit buffer pointer */
	volatile u_int	nxbdp;		/* next xmit buffer desc. pointer */
	volatile u_int	xbc;		/* xmit byte count */

	volatile u_int	xpntr;		/* hpc debug reg */
	volatile u_int	xfifo;		/* hpc debug reg */

	volatile u_int	cxbdp;		/* current xmit buf desc ptr */
	volatile u_int	cpfxbdp;	/* curr packet 1st xmit buf desc. ptr */
	volatile u_int	ppfxbdp;	/* prev pckt 1st xmit buf desc ptr */

	volatile u_int	intdelay;	/* interrupt delay count */

	u_int	fill3;			/* hpc debug reg (data 31 to 24) */

	volatile u_int	trstat;		/* xmit status */
	volatile u_int	rcvstat;	/* receive status */
	volatile u_int	ctl;		/* intrpt, channel reset, buf oflow */

	u_int	fill4;
	volatile u_int	rcount;		/* hpc debug reg */

	volatile u_int	rbc;		/* receive byte count */
	volatile u_int	crbp;		/* current receive buf ptr */
	volatile u_int	nrbdp;		/* next receive buf desc ptr */
	volatile u_int	crbdp;		/* current receive buf desc ptr */

	volatile u_int	rpntr;		/* hpc debug reg */
	volatile u_int	rfifo;		/* hpc debug reg */

	u_int	fill5[40];

	/*
	 * seeq external registers
	 */
	struct {

		union {
			volatile u_int	eaddr[6];

			volatile u_int	mcast_lsb[6];

			struct {
				volatile u_int mcast_msb[2];
				volatile u_int pktgap;
				volatile u_int ctl;
			} seq_write;

			/* read regs */
			struct {
				volatile u_int	coll_xmit[2];
				volatile u_int	coll_total[2];
				volatile u_int	fill6;
				volatile u_int	flags;
			} seq_read;
		} sr;
			
		volatile u_int	csr;		/* seeq receive com/stat reg */
		volatile u_int	csx;		/* seeq xmit cmd/stat reg */
	} seq_reg;

} *EHIO;
#endif	/* IP20 */

#if IP22 || IP26 || IP28
#define	XID_MASK	((1 << 5) - 1)

typedef struct xd {
	u_int x_xbufptr;
#ifdef	_MIPSEB
	u_int	x_eox:1,
		x_eoxp:1,
		x_xie:1,
		x_xid:5,
		x_ipg:8,
		x_txd:1,
		:1,
		x_xbcnt:14;
#else	/* _MIPSEL */
	u_int	x_xbcnt:14,
		:1,
		x_txd:1,
		x_ipg:8,
		x_xid:5,
		x_xie:1,
		x_eoxp:1,
		x_eox:1;
#endif	/* _MIPSEL */
	__uint32_t x_nxdesc;			/* (physical) struct xd * */
#if _MIPS_SIM == _ABI64
	__uint32_t x_pad0;
	struct mbuf *x_mbuf;
	__uint32_t x_pad1[2];
#else
	struct mbuf *x_mbuf;
#endif
} xd_desc;

typedef struct rd {
	u_int r_rbufptr;
#ifdef	_MIPSEB
	u_int	r_eor:1,
		r_eorp:1,
		r_xie:1,
		:14,
		r_rown:1,
		r_rbcnt:14;
#else	/* _MIPSEL */
	u_int	r_rbcnt:14,
		r_rown:1,
		:14,
		r_xie:1,
		r_eorp:1,
		r_eor:1;
#endif	/* _MIPSEL */
	__uint32_t r_nrdesc;		/* (physical) struct rd * */
#if _MIPS_SIM == _ABI64
	__uint32_t r_pad0;
	struct mbuf *r_mbuf;
	__uint32_t r_pad1[2];
#else
	struct mbuf *r_mbuf;
#endif
} rd_desc;

/*
 * HPC enet register mapping
 */
typedef struct {
	u_int		fill1[20480];

	volatile u_int	crbp;		/* current receive buf ptr */
	volatile u_int	nrbdp;		/* next receive buf desc ptr */
	u_int		fill2[1022];
	volatile u_int	rbc;		/* receive byte count */
	volatile u_int	rcvstat;	/* receive status */
	volatile u_int	rgio;		/* receive gio fifo ptr */
	volatile u_int	rdev;		/* receive device fifo ptr */

	u_int		fill3;

	volatile u_int	ctl;		/* intrpt, channel reset, buf oflow */
	volatile u_int	dmacfg;		/* dma configuration */
	volatile u_int	piocfg;		/* pio configuration */

	u_int		fill4[1016];

	volatile u_int	cxbp;		/* current xmit buffer ptr */
	volatile u_int	nxbdp;		/* next xmit buffer desc ptr */
	u_int		fill5[1022];
	volatile u_int	xbc;		/* xmit byte count */
	volatile u_int	trstat;		/* xmit status */
	volatile u_int	xgio;		/* xmit gio fifo ptr */
	volatile u_int	xdev;		/* xmit device fifo ptr */

	u_int		fill6[1020];

	volatile u_int	crbdp;		/* current receive desc ptr */

	u_int		fill7[2047];

	/* these 2 are ping pong ptrs and are not in order */
	volatile u_int	cpfxbdp;	/* current/previous packet 1st xmit */
	volatile u_int	ppfxbdp;	/* desc ptr */

	u_int		fill8[59390];

	/*
	 * seeq external registers
	 */
	struct {

		union {
			volatile u_int	eaddr[6];

			volatile u_int	mcast_lsb[6];

			struct {
				volatile u_int mcast_msb[2];
				volatile u_int pktgap;
				volatile u_int ctl;
			} seq_write;

			/* read regs */
			struct {
				volatile u_int	coll_xmit[2];
				volatile u_int	coll_total[2];
				volatile u_int	fill6;
				volatile u_int	flags;
			} seq_read;
		} sr;
			
		volatile u_int	csr;		/* seeq receive com/stat reg */
		volatile u_int	csx;		/* seeq xmit cmd/stat reg */
	} seq_reg;

} *EHIO;
#endif	/* IP22 || IP26 || IP28 */

#define	seq_csr		seq_reg.csr
#define	seq_csx		seq_reg.csx
#define	seq_eaddr	seq_reg.sr.eaddr
#define	seq_mcast_lsb	seq_reg.sr.mcast_lsb
#define	seq_mcast_msb	seq_reg.sr.seq_write.mcast_msb
#define	seq_pktgap	seq_reg.sr.seq_write.pktgap
#define	seq_ctl		seq_reg.sr.seq_write.ctl
#define	seq_coll_xmit	seq_reg.sr.seq_read.coll_xmit
#define	seq_coll_total	seq_reg.sr.seq_read.coll_total
#define	seq_flags	seq_reg.sr.seq_read.flags

