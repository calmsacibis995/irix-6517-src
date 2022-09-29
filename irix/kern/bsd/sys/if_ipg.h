/* FDDIXPress, 6U FDDI board, definitions
 *
 * Copyright 1990-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 * This contents of this file must match the corresponding definitions
 *	in ipg/ipg.h.
 */

#ifndef __IF_IPG_H__
#define __IF_IPG_H__
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.37 $"


#define IPG_MAXBD   4			/* allow this many boards */

typedef __int32_t IPG_CNT;		/* typical, board counter */

typedef struct ipg_nvram {		/* contents of non-volatile RAM */
	char	addr[6];
	u_char	cksum;
	u_char	zeros[32-6-1];
} IPG_NVRAM;

#define IPG_NVRAM_SUM(cksum,nvramp) { int nvbyte;		\
	(cksum) = 0;						\
	for (nvbyte=0; nvbyte<sizeof(*nvramp); nvbyte++)	\
		(cksum) += ((u_char*)(nvramp))[nvbyte];		\
}
#define IPG_NVRAM_CKSUM ((u_char)-2)



/* The command interface uses a control scheme consisting of a
 * series of commands, using a single union with an initial
 * discriminant.
 *
 * Most commands are expected to be completed by the board within
 * 2 or 4 microseconds.  The exceptions are ACT_FET_CT which may
 * take a couple dozen microseconds and ACT_FET_NVRAM and
 * ACT_STO_NVRAM which may take 512 microseconds.
 */
enum ipg_mail {
	ACT_NOP=0,

	ACT_DONE,			/* previous operation finished */

	ACT_VME,			/* set VME parameters */

	ACT_FLUSH,			/* flush MAC & DMA queues */

	ACT_PHY1_CMD,			/* send command to ENDEC #1 */
	ACT_PHY2_CMD,			/* send command to ENDEC #2 */
	ACT_PHY_REP,			/* repeat line states & disarm PHY */

	ACT_MUX,			/* set configuration MUX */

	ACT_BYPASS_INS,			/* "insert" the bypass */
	ACT_BYPASS_REM,			/* turn off the bypass */

	ACT_MAC_ARAM,			/* set contents of FORMAC "ARAM" */
	ACT_MAC_MODE,			/* set FORMAC mode */
	ACT_MAC_PARMS,			/* set MAC parameters */
	ACT_MAC_STATE,			/* force MAC state or FORMAC reset */

	ACT_CLAIM,			/* set claim frame */
	ACT_BEACON,			/* set beacon frame */

	ACT_PHY_ARM,			/* arm PHY interrupts */
	ACT_WAKEUP,			/* wake up H2B DMA */

	ACT_FET_TNEG,			/* fetch T_Neg */
	ACT_FET_LS1,			/* fetch line state */
	ACT_FET_LS2,
	ACT_FET_BOARD,			/* discover board type */

	ACT_FET_CT,			/* fetch counters */

	ACT_FET_NVRAM,			/* fetch NVRAM contents */
	ACT_STO_NVRAM			/* store NVRAM contents */
};


/* the shape of the "short I/O" board RAM, as seen by the host
 */
typedef volatile struct ipg_sio {
	ushort	mailbox;		/* reset and "mailbox" interrupt */
	ushort	sign;			/* board signature */
	ushort	vers;			/* board/eprom version */

	ushort	dwn_val[2];		/* download value */
	ushort	dwn_ad;			/* download address */

	union {
		struct {		/* ACT_VME */
			ushort	block;	/* 1=enable VME block mode */
			ushort	brl;
			ushort	vector;	/* interrupt vector */
			ushort	am;	/* address modifier & burst count */
			ushort	flags;
#			define	IPG_LAT 1   /* enable latency measures */
			ushort	cksum_max[2];
			ushort	b2h_buf[2]; /* B2H buffer */
			ushort	b2h_len;    /* B2H_LEN */
			ushort	max_bufs;   /* maximum total mbufs */
			ushort	h2b_buf[2];
			ushort	h2b_dma_delay[2];
			ushort	b2h_dma_delay[2];
			ushort	int_delay[2];
			ushort	max_host_work[2];
			ushort	h2b_sleep[2];
			ushort	sml_size;   /* MLEN */
			ushort	pad_in_dma; /* sizeof(FDDI_IBUF) */
		} vme;

		ushort	phy;		/* ACT_PHY*_CMD */

		ushort	mux;		/* ACT_MUX */

		ushort	adrs[8];	/* ACT_MAC_ARAM */

		struct {		/* ACT_MAC_MODE */
			ushort	mode;	/* FORMAC mode value */
			ushort	multi;	/* 0=normal, 1=all multicasts */
			ushort	hash[256/16];
		} mac;

		struct {		/* ACT_MAC_PARMS */
			ushort	t_max;
			ushort	tvx;
		} parms;

		ushort	mac_state;	/* ACT_MAC_STATE */

		struct {		/* ACT_CLAIM or _BEACON */
			ushort	len;	/* length in bytes */
			ushort	b[16];
		} frame;

		IPG_NVRAM nvram;	/* ACT_*_NVRAM */

		ushort	t_neg[2];	/* ACT_FET_TNEG */

		ushort	ls;		/* ACT_FET_LS* */
		ushort	bd_type;	/* ACT_FET_BOARD */


		/* Many events are represented by counters incremented by
		 * the board. On each FORMAC status interrupt, the
		 * corresponding counter is incremented, except for
		 * RNGOP and NOT_RNGOP which are synthesized from the
		 * SRNGOP bit and a private flag on the board.
		 *
		 * These counters must be arranged to prevent padding.
		 */
		struct sio_counts {	/* ACT_FET_CT */
		    IPG_CNT ticks;   /* free running usec timer */

/* i */		    IPG_CNT rngop;	/* ring went non-RingOP to RingOP */
/* i */		    IPG_CNT rngbroke;	/* ring went RingOP to non-RingOP */
		    IPG_CNT pos_dup;
		    IPG_CNT misfrm;
		    IPG_CNT xmtabt;
		    IPG_CNT tkerr;
/* i */		    IPG_CNT clm;
/* i */		    IPG_CNT bec;
/* i */		    IPG_CNT tvxexp;
/* i */		    IPG_CNT trtexp;
		    IPG_CNT tkiss;	/* tokens issued */
		    IPG_CNT myclm;
		    IPG_CNT loclm;
		    IPG_CNT hiclm;
		    IPG_CNT mybec;
		    IPG_CNT otrbec;

		    IPG_CNT frame_ct;	/* total frames seen */
		    IPG_CNT tok_ct;	/* token count */
		    IPG_CNT err_ct;	/* FORMAC error pulses */
		    IPG_CNT lost_ct;	/* FORMAC lost pulses */

		    IPG_CNT lem1_ct;	/* LEM "events" */
		    IPG_CNT lem1_usec4;	/* usec*4 of elapsed measuring time */
		    IPG_CNT lem2_ct;
		    IPG_CNT lem2_usec4;

		    IPG_CNT eovf1;	/* elasticity overflows */
		    IPG_CNT eovf2;

		    IPG_CNT flsh;	/* flushed frames */
		    IPG_CNT abort;	/* aborted frames */
		    IPG_CNT miss;	/* gap too small */
		    IPG_CNT error;	/* error detected by local MAC */
		    IPG_CNT e;		/* received E bit */
		    IPG_CNT a;		/* received A bit */
		    IPG_CNT c;		/* received C bit */
		    IPG_CNT rx_ovf;	/* receive FIFO overrun */
		    IPG_CNT buf_ovf;	/* out of buffers */
		    IPG_CNT tx_under;	/* transmit FIFO underrun */

/* i */		    IPG_CNT noise1;	/* noise events */
/* i */		    IPG_CNT noise2;

		    IPG_CNT tot_junk;	/* discarded frames */
		    IPG_CNT junk_void;	/* discarded void frames */
		    IPG_CNT junk_bec;	/* discarded beacon frames */
		    IPG_CNT shorterr;	/* frame too short */
		    IPG_CNT longerr;	/* frame too long */

		    enum mac_states mac_state;	/* current MAC state */

		    IPG_CNT dup_mac;	/* likely duplicate address */

		    IPG_CNT rnglatency;	/* measured ring latency in ticks */
# define IPG_MAX_RNG_LATENCY (2*2661*IPG_BD_MHZ)    /* >2*D_Max */

/* i: interesting event which causes an interrupt.
 *	When an interrupt is required by a change in one of these counters,
 *	the interrupt will occur the next time the board interrupt is enabled,
 *	regardless of whether the host has since asked that the counters
 *	be updated.
 */
		} counts;
	} cmd;
} IPG_SIO;



#define IPG_BD_MHZ 16			/* board CPU clock speed */

#define IPG_SGI_SIGN 0xfdd1		/* firmware signature */

/* values for "mailbox" interrupt word */
#define IPG_SIO_RESET	0x8000
#define IPG_SIO_MAILBOX	ACT_DONE

/* download values */
#define IPG_DWN_RDY	0xfffe
#define IPG_DWN_GO	0xffff


/* board type bits */
#define IPG_BD_TYPE_MAC2	0x01	/* connected to 2nd board */
#define IPG_BD_TYPE_BYPASS	0x02	/* seem to have an optical bypass */


/* ENDEC commands */
#define	ENDEC_SEL_CR0	    0x0000
#define	ENDEC_SEL_CR1	    0x0100
#define	ENDEC_SEL_CR2	    0x0200
#define	ENDEC_SEL_STATUS    0x0300
#define	ENDEC_SEL_CR3	    0x0400

/* ENDEC CR0 modes */
#define	ENDEC_QLS	(0x00 + ENDEC_SEL_CR0)
#define	ENDEC_MLS	(0x01 + ENDEC_SEL_CR0)
#define	ENDEC_HLS	(0x02 + ENDEC_SEL_CR0)
#define	ENDEC_ILS	(0x03 + ENDEC_SEL_CR0)
#define	ENDEC_FILTER    (0x04 + ENDEC_SEL_CR0)
#define	ENDEC_JKILS	(0x05 + ENDEC_SEL_CR0)
#define	ENDEC_IQ	(0x06 + ENDEC_SEL_CR0)
#define	ENDEC_UNFILTER  (0x07 + ENDEC_SEL_CR0)

/* ENDEC CR1 modes */
#define	ENDEC_TA	(0x04 + ENDEC_SEL_CR1)  /* TA-bus */
#define	ENDEC_TB	(0x00 + ENDEC_SEL_CR1)  /* TB-bus */
#define	ENDEC_THRU	(0x00 + ENDEC_SEL_CR1)  /* 'through' mode */
#define	ENDEC_LOOPBACK  (0x01 + ENDEC_SEL_CR1)  /* loopback mode */
#define	ENDEC_SHORTBACK (0x02 + ENDEC_SEL_CR1)  /* short-loopback */
#define	ENDEC_REPEAT    (0x03 + ENDEC_SEL_CR1)  /* repeat mode */

/* ENDEC CR2 modes */
#define	ENDEC_RESET	(0x04 + ENDEC_SEL_CR2)  /* reset */


/* FORMAC modes */
#define FORMAC_MMODE		0xE000	/* mask MMODE bits */
#define FORMAC_MMODE_INIT	0x0000	/* initialize mode */
#define FORMAC_MMODE_ONL	0x6000	/* online */
#define FORMAC_MMODE_IL1	0x8000	/* internal loopback 1 */
#define FORMAC_MMODE_IL2	0xA000	/* internal loopback 2 */
#define FORMAC_MMODE_EL1	0xC000	/* external loopback 1 */
#define FORMAC_MMODE_EL2	0xE000	/* external loopback 2 */
#define FORMAC_MUSESA		0x1000	/* use short addresses */
#define FORMAC_MDELADET		0x0800	/* delayed address detection */
#define FORMAC_MODE_DISRCV	0x0400	/* disable reception */
#define FORMAC_MADET_MASK	0x0300
#define FORMAC_MADET_DA		0x0000	/* receive if DA=MA */
#define FORMAC_MADET_DASA	0x0100	/* receive if DA=MA or SA=MA */
#define FORMAC_MADET_PROM	0x0200	/* promiscuous */
#define FORMAC_MNFCS		0x0080	/* no frame check sequence */
#define FORMAC_MSELRA		0x0020	/* 1=select RA or 0=RB input */
#define FORMAC_MDSCRY		0x0010	/* must be 0 */


/* MUX values
 *	These values correspond to bits in the board command register
 */
#define MUX_EXT		0x8000		/* RB/TB bus */
#define MUX_ENDEC	0x4000		/* ENDEC R bus */
#define MUX_FORMAC	0x2000		/* X/TA bus */
#define MUX_ENDEC2	0x0100		/* second ENDEC (set DATACH) */



/* board-to-host DMA
 */
struct ipg_b2h {
	u_char	b2h_sn;
	u_char	b2h_op;
	ushort	b2h_val;
};
#define IPG_B2H_SLEEP	1		/* board has run out of work */
#define IPG_B2H_ODONE	2		/* output DMA commands finished */
#define IPG_B2H_IN	3		/* input DMA */
#define IPG_B2H_IN_DONE	4
#define IPG_B2H_PHY	5		/* PHY interrupt */


#define	IPG_IN_CKSUM_LEN 4096		/* input bytes checksummed */

#define IPG_MAX_DMA_ELEM 6		/* max segments/data output DMA */
#define IPG_PHY_INT_NUM	12

#define IPG_B2H_LEN	((IPG_MAX_SML+IPG_MAX_MED+IPG_MAX_BIG)*2 \
			 + IFQ_MAXLEN + IPG_PHY_INT_NUM + 2)

#define	IPG_PADMAX_IN_DMA 32		/* max input padding added by board */


#define IPG_MAX_BIG	32		/* clusters in pool */
#define IPG_MAX_MED	32		/* medium sized mbufs in pool */
#define IPG_MAX_SML	64		/* little mbufs in pool */
#define IPG_BIG_SIZE	IPG_IN_CKSUM_LEN    /* size of big mbufs */
#define IPG_MED_SIZE	(1514+(24-14)+IPG_PADMAX_IN_DMA)   /* (Ethernet) */
#define IPG_SML_SIZE	MLEN

/* host-to-board DMA
 */
struct ipg_h2b {
	ushort	h2b_op;
	ushort	h2b_len;
	__int32_t	h2b_addr;
};
#define IPG_H2B_EMPTY	(0*4)
#define IPG_H2B_SMLBUF	(1*4)		/* add little mbuf to pool */
#define IPG_H2B_MEDBUF	(2*4)		/* add medium mbuf to pool */
#define IPG_H2B_BIGBUF	(3*4)		/* add cluster to pool */
#define IPG_H2B_OUT	(4*4)		/* start output DMA chain */
#define IPG_H2B_OUT_FIN	(5*4)		/* end output DMA chain */
#define IPG_H2B_PHY_ACK	(6*4)		/* finished PHY interrupt */
#define	IPG_H2B_WRAP	(7*4)		/* back to start of buffer */
#define	IPG_H2B_IDELAY	(8*4)		/* delay interrupts */

/* big enough to never be short of space
 *	max output queue + max input queue + max buffer pool
 *		+ N*PHY int acks + sleep note + marker
 *
 *	max output queue=50+unused slot at end of table
 *
 */
#define IPG_H2B_LEN	(IPG_MAX_SML + IPG_MAX_MED + IPG_MAX_BIG \
			 + IPG_MAX_DMA_ELEM*(IFQ_MAXLEN+1)	\
			 + IPG_PHY_INT_NUM			\
			 + 1 + 1)
#if IFQ_MAXLEN != 50
? error ? error ?			/* fix the firmware */
#endif


/* special IOCTLs
 */
#define SIOC_IPG_NVRAM_GET	_IOR('P', 1, IPG_NVRAM)
#define SIOC_IPG_NVRAM_SET	_IOW('P', 2, IPG_NVRAM)

#ifdef __cplusplus
}
#endif
#endif /* __IF_IPG_H__ */
