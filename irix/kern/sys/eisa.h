/**************************************************************************
 *									  *
 *		Copyright ( C ) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _SYS_EISA_H
#define _SYS_EISA_H

#ident "$Revision: 1.10 $"

#include <sys/kmem.h>

/*
 * EISA register set
 */
#define   DmaCh0Address		    0x00
#define   DmaCh0Count		    0x01
#define   DmaCh1Address		    0x02
#define   DmaCh1Count		    0x03
#define   DmaCh2Address		    0x04
#define   DmaCh2Count		    0x05
#define   DmaCh3Address		    0x06
#define   DmaCh3Count		    0x07
#define   Dma03StatusCommand	    0x08
#define   Dma03Request		    0x09
#define   Dma03SingleMask	    0x0a
#define   Dma03Mode		    0x0b
#define   Dma03ClearBytePointer	    0x0c
#define   Dma03MasterClear	    0x0d
#define   Dma03ClearMask	    0x0e
#define   Dma03AllMask		    0x0f

#define   Int1Control		    0x20
#define   Int1Mask		    0x21

#define   IntTimer1SystemClock	    0x40
#define   IntTimer1RefreshRequest   0x41
#define   IntTimer1SpeakerTone	    0x42
#define   IntTimer1CommandMode	    0x43

#define   IntTimer2FailsafeClock    0x48
#define   IntTimer2Reserved	    0x49
#define   IntTimer2CPUSpeeedCtrl    0x4a
#define   IntTimer2CommandMode	    0x4b

#define   NMIStatus		    0x61
#define   NMIEnable		    0x70

#define   DmaCh2LowPage             0x81
#define   DmaCh3LowPage             0x82
#define   DmaCh1LowPage             0x83
#define   DmaCh0LowPage             0x87
#define   DmaCh6LowPage             0x89
#define   DmaCh7LowPage             0x8a
#define   DmaCh5LowPage             0x8b
#define   DmaLowRefreshPage         0x8f

#define   Int2Control		    0xa0
#define   Int2Mask		    0xa1

#define   DmaCh4Address		    0xc0
#define   DmaCh4Count		    0xc2
#define   DmaCh5Address		    0xc4
#define   DmaCh5Count		    0xc6
#define   DmaCh6Address		    0xc8
#define   DmaCh6Count		    0xca
#define   DmaCh7Address		    0xcc
#define   DmaCh7Count		    0xce
#define   Dma47StatusCommand	    0xd0
#define   Dma47Request		    0xd2
#define   Dma47SingleMask	    0xd4
#define   Dma47Mode		    0xd6
#define   Dma47ClearBytePointer	    0xd8
#define   Dma47MasterClear	    0xda
#define   Dma47ClearMask	    0xdc
#define   Dma47AllMask		    0xde

#define   DmaCh0HighCount	    0x401
#define   DmaCh1HighCount	    0x403
#define   DmaCh2HighCount	    0x405
#define   DmaCh3HighCount	    0x407

#define   Dma03IntStatusChain	    0x40a
#define   Dma03ExtMode		    0x40b
#define   DmaChainExpire	    0x40c

#define   ExtNMIReset		    0x461
#define   NMIIntPort		    0x462
#define   LastEISAMaster	    0x464

#define   DmaCh2HighPage            0x481
#define   DmaCh3HighPage            0x482
#define   DmaCh1HighPage            0x483
#define   DmaCh0HighPage            0x487
#define   DmaCh6HighPage            0x489
#define   DmaCh7HighPage            0x48a
#define   DmaCh5HighPage            0x48b
#define   DmaHighRefreshPage        0x48f

#define   DmaCh5HighCount	    0x4c6
#define   DmaCh6HighCount	    0x4ca
#define   DmaCh7HighCount	    0x4ce

#define   Int1TrigMode		    0x4d0
#define   Int2TrigMode		    0x4d1

#define   Dma47IntStatusChain	    0x4d4
#define   Dma47ExtMode		    0x4d6

#define   DmaCh0StopA		    0x4e0
#define   DmaCh0StopB		    0x4e1
#define   DmaCh0StopC		    0x4e2
#define   DmaCh1StopA		    0x4e4
#define   DmaCh1StopB		    0x4e5
#define   DmaCh1StopC		    0x4e6
#define   DmaCh2StopA		    0x4e8
#define   DmaCh2StopB		    0x4e9
#define   DmaCh2StopC		    0x4ea
#define   DmaCh3StopA		    0x4ec
#define   DmaCh3StopB		    0x4ed
#define   DmaCh3StopC		    0x4ee
#define   DmaCh5StopA		    0x4e8
#define   DmaCh5StopB		    0x4e9
#define   DmaCh5StopC		    0x4ea
#define   DmaCh6StopA		    0x4f8
#define   DmaCh6StopB		    0x4f9
#define   DmaCh6StopC		    0x4f9
#define   DmaCh7StopA		    0x4fc
#define   DmaCh7StopB		    0x4fd
#define   DmaCh7StopC		    0x4fe

/* Per slot register register offsets */
#define   EisaId		    0xc80	/* 4 one byte regs */
#define   EisaCtl		    0xc84

/*
 * eisa dma buffer descriptor structure
 */
struct eisa_dma_buf {
	uint_t			count;		/* size of block */
	paddr_t			address;	/* phys addr of data block */
	struct eisa_dma_buf	*next_buf;	/* next buffer descriptor */
	paddr_t			stopval;	/* ring buf stop */
};

/*
 * eisa dma command block structure
 */
struct eisa_dma_cb {
	struct eisa_dma_buf *reqrbufs;	/* list of reqr data bufs */
	union modes {
	    struct {
	      ushort_t
		 mreqr_ringstop:1,	/* use channel's stop reg? */
		 mreqr_eop:1,		/* is EOP input/output */
		 mreqr_timing:2,	/* A,B,C,ISA compat */
		 mreqr_path:2,		/* 8/16/32 */
		 :2,			/* reserved */
		 mtrans_type:2,		/* Single/Demand/Blck/Cascade */
		 mtarg_step:1,		/* Inc/Dec */
		 :1,
		 mcommand:2,		/* Read/Write/Translate/Verify*/
		 :2;			/* reserved */
	    } m;
	    ushort_t mode;
	} mu;
	uchar_t	bufprocess;		/* single/chain/autoinit */
	uchar_t	reqr_bswap;		/* byte swap data on/off */
	void	*procparam;		/* parameter buf for app call */
	int	(*proc)(void*);		/* addr of app call routine */
};

#define reqr_ringstop	mu.m.mreqr_ringstop
#define reqr_eop	mu.m.mreqr_eop
#define reqr_timing	mu.m.mreqr_timing
#define reqr_path	mu.m.mreqr_path
#define trans_type	mu.m.mtrans_type
#define targ_step	mu.m.mtarg_step
#define cb_cmd		mu.m.mcommand
#define moderegs	mu.mode

/*
 * Command block defines (conforms to eisa reg values,
 * do not modify)
 */

/* reqr_ringstop */
#define EISA_DMA_RING_OFF	0	/* don't use stop reg */
#define EISA_DMA_RING_ON	1	/* use stop reg */

/* reqr_eop */
#define EISA_DMA_EOP_OUTPUT	0	/* EOP is output */
#define EISA_DMA_EOP_INPUT	1	/* EOP is input */

/* reqr_timing */
#define EISA_DMA_TIME_ISA	0	/* ISA compat mode timing */
#define EISA_DMA_TIME_A		1	/* type A timing */
#define EISA_DMA_TIME_B		2	/* type B timing */
#define EISA_DMA_TIME_C		3	/* type C (burst) timing */

/* reqr_path */
#define EISA_DMA_PATH_8		0	/* 8 bit xfer */
#define EISA_DMA_PATH_16	3	/* 16 bit xfer */
#define EISA_DMA_PATH_32	2	/* 32 bit xfer */

/* trans_type */
#define EISA_DMA_TRANS_DMND	0	/* variety of block, reqr flow ctl */
#define EISA_DMA_TRANS_SNGL	1	/* single xfer */
#define EISA_DMA_TRANS_BLCK	2	/* block xfer must use if sw initiated*/

/* targ_step */
#define EISA_DMA_STEP_INC	0	/* increm targ addr on xfer */
#define EISA_DMA_STEP_DEC	1	/* decrem targ addr on xfer */

/* command */
#define EISA_DMA_CMD_WRITE	1	/* write from io dev to mem */
#define EISA_DMA_CMD_READ	2	/* read from mem to io dev */

/* bufprocess */
#define EISA_DMA_BUF_SNGL	0	/* memory has a single DMA buffer */
#define EISA_DMA_BUF_CHAIN	1	/* memory has a chain of DMA buffers */

/* reqr_bswap */
#define EISA_DMA_BSWAP_OFF	0	/* don't do byte swapping */
#define EISA_DMA_BSWAP_ON	1	/* byte swap */

/* Chaining Mode Register defns */
#define	EISA_CHAIN_ENABLE	(1 << 2)
#define EISA_CHAIN_PROGDONE	(1 << 3)

/* Mask Reg defns */
#define EISA_DMAMASK_CLEAR	0
#define EISA_DMAMASK_SET	(1 << 2)

/* Request Reg defns */
#define EISA_REQUEST_CLEAR	0
#define EISA_REQUEST_SET	(1 << 2)

/* buffer alloc flags
 */
#define EISA_DMA_SLEEP		KM_SLEEP
#define EISA_DMA_NOSLEEP	KM_NOSLEEP

/*
 * interrupt vector alloc flags
 */
#define EISA_EDGE_IRQ		0
#define EISA_LEVEL_IRQ		1
#define EISA_RESERVED_IRQ	0x80

/*
 * bus master flags
 */
#define EISA_MASTER_BSWAP_OFF	0
#define EISA_MASTER_BSWAP_ON	1
#define EISA_MASTER_ZEROWAIT	2

#define spleisa		spl5

extern int eisa_ivec_alloc(uint_t, ushort_t, uchar_t);
extern int eisa_ivec_set(uint_t, int, void (*)(long), long);

extern int eisa_dmachan_alloc(uint_t, uchar_t);

extern struct eisa_dma_buf *eisa_dma_get_buf(uchar_t);
extern struct eisa_dma_cb  *eisa_dma_get_cb(uchar_t);

extern void eisa_dma_free_buf(struct eisa_dma_buf *);
extern void eisa_dma_free_cb(struct eisa_dma_cb *);

extern int  eisa_dma_prog(uint_t, struct eisa_dma_cb *, int, uchar_t);
extern void eisa_dma_disable(uint_t, int);
extern void eisa_dma_stop(uint_t, int);
extern void eisa_dma_enable(uint_t, struct eisa_dma_cb *, int, uchar_t);
extern void eisa_dma_swstart(uint_t, int, uchar_t);

#endif
