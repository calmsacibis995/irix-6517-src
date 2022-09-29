/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.35 $"

/*
 * SCSI command interface internal data structures.
 */

#define	PSCSI		PZERO-1

#define	MAXWAIT		4000000	/* Max wait for non-timeout IO
					   operations */
#define		SC_MAXRETRY	4	/* Max number of retries on
					   parity errs */

typedef struct scsidev		scsidev_t;


/*
 * Host adapter interface record.  Records our state, his info, any
 * other nits needed to properly run the adapter.  Could be generalized
 * by adding a different kind of structure for each device
 */
struct scsidev {
	u_char d_flags;	/* Flags, see below */
	u_char d_ctrlreg;	/* value to put in ctrl register */
	u_char d_tdata;	/* temporary holder for data reg on some intr */
	u_char d_adap;	/* controller # */
	u_char d_ucode;	/* ucode revision; 93B only */
	struct dmamap	*d_map;		/* DMA map */
	scsisubchan_t	*d_consubchan;	/* Connected subchannel */
	u_int		d_retry;	/* Retry count */
	scsisubchan_t	*d_subchan[SC_MAXTARG][SC_MAXLUN];
					/* Table of subchannels */
	scsisubchan_t	*d_waithead;	/* Async request waitint list head */
	scsisubchan_t	*d_waittail;	/* Async request waitint list tail */
	sema_t		d_sleepsem;	/* Synchronous request waiting sem */
	sema_t		d_onlysem;	/* 'only' request waiting sem */
	short		d_sleepcount;	/* Synchronous request waiter count */
	short		d_onlycount;	/* Synchronous request waiter count */
	lock_t		d_qlock;	/* Lock for structure flags & queues */
	lock_t		d_reglock;	/* Lock for hardware registers */
	volatile u_char	*d_addr;	/* chip address register */
	volatile u_char	*d_data;	/* chip 'data' register */
};

#define	D_BUSY	0x1	/* host adapter in use */
#define D_WD93	0x2	/* is a 93, don't try burst mode dma, etc. */
#define D_WD93A	0x4	/* is a 93A, so we can use burst mode dma */
#define D_WD93B	0x8	/* is a 93B, superset of 93A, fast scsi, queues, etc. */
#define D_ONLY	0x10	/* only onlysem waiters can acquire */
#define D_MAPRETAIN 0x20 /*Set via scuzzy struct d_initflags for systems
			 * where the DMA map retained across disconnects;
			 * i.e., a DMA MAP per SCSI channel in use */
#define D_PROMSYNC 0x40 /* Set via scuzzy struct d_initflags for systems
			 * whose PROM understands sync scsi */

/* Chip registers.  */
#define	WD93ID		0	/* own id register */
#define	WD93CTRL	1	/* control register */
#define	WD93TOUT	2	/* timeout register */
#define	WD93C1		3	/* command byte 1 */
#define	WD93C2		4	/* command byte 1 */
#define	WD93C3		5	/* command byte 1 */
#define	WD93C4		6	/* command byte 1 */
#define	WD93C5		7	/* command byte 1 */
#define	WD93C6		8	/* command byte 1 */
#define	WD93C7		9	/* command byte 1 */
#define	WD93C8		10	/* command byte 1 */
#define	WD93C9		11	/* command byte 1 */
#define	WD93C10		12	/* command byte 1 */
#define	WD93C11		13	/* command byte 1 */
#define	WD93C12		14	/* command byte 1 */
#define	WD93LUN		15	/* target LUN */
#define	WD93PHASE	16	/* command phase */
#define	WD93SYNC	17	/* synchronous transfer */
#define	WD93CNTHI	18	/* MSB transfer count */
#define	WD93CNTMD	19	/* transfer count */
#define	WD93CNTLO	20	/* LSB transfer count */
#define	WD93DESTID	21	/* destination ID */
#define	WD93SRCID	22	/* source ID */
#define	WD93STAT	23	/* SCSI Status */
#define	WD93CMD		24	/* command register */
#define	WD93DATA	25	/* data register */
#define	WD93QTAG	26	/* queue tag register (93B only) */

/* Timeout values.  */
#if IP20 || IP22 || IP26	/* runs at 20 Mhz */
#define	T93SATN		64	/* ~250 ms @ 4 ms per tick (for 20MHz) */
#define	T93SATN_SHORT	 8	/*  ~30 ms during hinv and diags */
#define WD93FREQVAL	0x80	/* bit for chip frequency (ORed into 93ID) */
#else	/* all others run at 10 MHz */
#define	T93SATN		32	/* ~250 ms @ 8 ms per tick (for 10MHz) */
#define	T93SATN_SHORT	 4	/*  ~30 ms during hinv and diags */
#define WD93FREQVAL	0	/* bit for chip frequency (ORed into 93ID) */
#endif	/* IP20 || IP22 */

#define	T93SRETRY	3	/* times to attempt selection; same as most
	drives will do on re-selects. */

/* Chip commands.  */
#define	C93RESET	0x00	/* reset the bus and chip */
#define	C93ABORT	0x01	/* abort current operation */
#define	C93ATN		0x02	/* assert ATN on the bus */
#define	C93NACK		0x03	/* negate ACK line */
#define	C93DISC		0x04	/* disconnect from bus */
#define	C93RESEL	0x05	/* reselect */
#define	C93SELATN	0x06	/* select with ATN */
#define	C93SELNATN	0x07	/* select with no ATN */
#define	C93SELATNTR	0x08	/* select with ATN and transfer */
#define	C93SELNATNTR	0x09	/* select without ATN and transfer */
#define	C93RESELREC	0x0a	/* reselect and receive */
#define	C93RESELSND	0x0b	/* reselect and send */
#define	C93WSELREC	0x0c	/* wait for select and receive */
#define	C93SENDMSG	0x16	/* send message to target */
#define	C93TRADDR	0x18	/* translate address */
#define	C93TRINFO	0x20	/* transfer info */
#define	C93TRPAD	0x21	/* transfer pad */

/* States of interest */
#define	ST_RESET	0x00	/* Reset by command or power-up (MR-) */
#define	ST_SELECT	0x11	/* Selection complete (after C93SELATN) */
#define	ST_SATOK	0x16	/* Select-And-Transfer completed successfully */
#define ST_TR_DATAOUT	0x18	/* transfer cmd done, target requesting data */
#define ST_TR_DATAIN	0x19	/* transfer cmd done, target sending data */
#define	ST_TR_STATIN	0x1b	/* Target is sending status in */
#define ST_TR_MSGIN	0x1f	/* transfer cmd done, target sending msg */
#define ST_TRANPAUSE	0x20	/* transfer cmd has paused with ACK */
#define	ST_SAVEDP	0x21	/* Save Data Pointers message during SAT */
#define	ST_A_RESELECT	0x27	/* reselected after disc (93A) */
#define	ST_UNEXPDISC	0x41	/* An unexpected disconnect */
#define	ST_PARITY	0x43	/* cmd terminated due to parity error */
#define	ST_PARITY_ATN	0x44	/* cmd terminated due to parity error
	(ATN is asserted) */
#define	ST_TIMEOUT	0x42	/* Time-out during Select or Reselect */
#define	ST_INCORR_DATA	0x47	/* incorrect message or status byte */
#define	ST_UNEX_RDATA	0x48	/* Unexpected receive data phase */
#define	ST_UNEX_SDATA	0x49	/* Unexpected send data phase */
#define	ST_UNEX_CMDPH	0x4a	/* Unexpected cmd phase */
#define	ST_UNEX_SSTATUS	0x4b	/* Unexpected send status phase */
#define	ST_UNEX_RMESGOUT	0x4e	/* Unexpected request msg out phase */
#define	ST_UNEX_SMESGIN	0x4f	/* Unexpected send message in phase */
#define	ST_RESELECT	0x80	/* WD33C93 has been reselected */
#define	ST_93A_RESEL	0x81	/* reselected while idle (93A) */
#define	ST_DISCONNECT	0x85	/* Disconnect has occurred */
#define ST_NEEDCMD	0x8a	/* Target is ready for a cmd */
#define	ST_REQ_SMESGOUT	0x8e	/* REQ signal for send message out */
#define	ST_REQ_SMESGIN	0x8f	/* REQ signal for send message in */

/* Phases during a Select and Transfer command */
#define	PH_NOSELECT	0x00	/* Selection not successful */
#define	PH_SELECT	0x10	/* Selection successful */
#define	PH_IDENTSEND	0x20	/* Identify message sent */
/* phase 30 indicates none of the cmd bytes have yet been sent; every
 * cmd byte sent increments that by one. */
#define	PH_CDB_START	0x30	/* Start of CDC transfers */
#define	PH_CDB_6	0x36	/* 6th cmd byte sent */
#define	PH_CDB_10	0x3a	/* 0xAth cmd byte sent */
#define	PH_CDB_12	0x3c	/* 0xCth cmd byte sent */
#define	PH_SAVEDP	0x41	/* Save data pointers */
#define	PH_DISCRECV	0x42	/* Disconnect message received */
#define	PH_DISCONNECT	0x43	/* Target disconnected */
#define	PH_RESELECT	0x44	/* Original target reselected */
#define	PH_IDENTRECV	0x45	/* Correct identify (right LUN) message rcv'd */
#define	PH_DATA		0x46	/* Data transfer completed */
#define	PH_STATUSRECV	0x50	/* Status byte received */
#define	PH_COMPLETE	0x60	/* Command complete message received */

/* Auxiliary status register bits */
#define	AUX_INT		0x80	/* interrupt pending */
#define	AUX_LCI		0x40	/* last command ignored */
#define	AUX_BSY		0x20	/* busy -- level II command being performed */
#define	AUX_CIP		0x10	/* command in progress */
#define	AUX_FFE		0x04	/* WD FIFO full/empty (93B only) */
#define	AUX_PE		0x02	/* parity error */
#define	AUX_DBR		0x01	/* data buffer ready */

