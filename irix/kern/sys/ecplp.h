/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "sys/ecplp.h: $Revision: 1.6 $"

/*
 * ecplp.h - header for the IP32 (Moosehead)
 * IEEE1284 extended capabilities parallel port driver
 */

#include "sys/PCI/pciio.h"

#ifdef _LANGUAGE_C

#ifdef _K32U64
#define _IRIX6_3		/* for IP32 IRIX 6.3 (bonsai) */
#endif

#define	BYTE	unsigned char
#define	WORD	unsigned short
#define	DWORD	unsigned int

#define DCACHE_INVAL(a, b)	dki_dcache_inval(a, b)
#define DCACHE_WB(a, b)		dki_dcache_wb(a, b)

#define ONSPEC_BUG_RD_WAR	/* workaround for OnSpec chip bugs */
#ifdef ONSPEC_BUG_RD_WAR
#define NPFDRB		2
#define	PFD_RBUF_SZ	512*18*2	/* 512*18*2 bytes/trk */
#endif


/******************************************************************************
 *
 * Data Structure for Parallel Interface Control Registers
 *
 ******************************************************************************
 */

/* Centronics & EPP Control Register Structure */
typedef struct lptctrl_s {
	/* Data Register | ECP Address FIFO Register */
	union {
           volatile BYTE	_data;		/* data port */
           volatile BYTE	_ecpafifo;	/* ECP addr FIFO */
	} u1;
	#define data		u1._data
	#define ecpafifo	u1._ecpafifo

	/* Device Status Register bit definitions (R) */
        volatile BYTE		dsr;	/* device status register */

	#define DSR_BUSYZ	0x80	/* 0=BUSY */
	#define DSR_ACKZ	0x40	/* 0=ACK */
	#define DSR_PE		0x20	/* 1=PE */
	#define DSR_SLCT	0x10	/* 1=SLCT */
	#define DSR_ONLINE	0x10	/* 1=SLCT */
	#define DSR_FAULTZ	0x08	/* 0=error */
	#define DSR_ERRZ	0x08	/* 0=error */
	#define DSR_INTRZ	0x04	/* 0=printer interrupt */
	#define DSR_NOINKZ	0x04	/* 0=printer interrupt */
	#define DSR_TIMEOUT	0x02	/* 1=time out */

	/* Device Control Register bit definitions (R/W) */
        volatile BYTE		dcr;	/* device control register */

	#define DCR_DIR		0x20	/* 1=input;  0=output */
	#define DCR_IRQEN	0x10	/* 1=enable interrupt */
	#define DCR_SLCTIN	0x08	/* 1=active, 0=inactive SLINz */
	#define DCR_NINIT	0x04	/* 0=active, 1=inactive INITz */
	#define DCR_AFD		0x02	/* 1=active, 0=inactive AFDz */
	#define DCR_STB		0x01	/* 1=active, 0=inactive STBz */

	#define DCR_STD_DEFAULT	0xC4

	/* EPP Address Register */
        volatile BYTE		eppaddr; /* EPP addr port */

	/* EPP Data Register */
        volatile DWORD		eppdata; /* EPP data port */

} plp_lptctrl_t;


/* ECP Control Registers Structure */
typedef struct ecpctrl_s {

	/* ECP Data FIFO Register | ECP Configuration A Register */
	union {
       	   volatile BYTE	_ecpdfifo;	/* ECP data FIFO */
       	   volatile BYTE	_cnfgA;		/* ECP config register A */
	} u2;
	#define	ecpdfifo	u2._ecpdfifo
	#define cnfgA		u2._ecpcnfgA

	#define ECP_IMPL_ID_MASK 0x0F	/* implementation ID */

	/* ECP Configuration B Register */
        volatile BYTE		cnfgB;	/* ECP config register B */

	#define ECP_COMPRESS	0x80	/* compression */
	#define ECP_INTR_VALUE	0x40	/* ret value of IRQ conflicts */
	#define ECP_IRQ_LINE	0x3C	/* IRQ line */
	#define ECP_DMA_CHNL	0x03	/* DMA channel */

	/* ECP Extended Control Register bit definitions (R/W) */
        volatile BYTE		ecr;	/* extended control register */

	#define	PLP_OP_MODE	0xE0	/* operation mode: bit(7:5) */
	#define	PLP_AUTO_MODE	0x00	/* auto-negotiation mode */
	#define	PLP_CMPTBL_MODE	0x00	/* standard centronics mode */
	#define PLP_PIO_MODE    0x10    /* non-FIFO centronics mode */
	#define	PLP_BIDIR_MODE	0x20	/* bi-direction byte mode */
	#define	PLP_CDF_MODE	0x40	/* parallel port fifo mode */
	#define	PLP_ECP_MODE	0x60	/* extended capabilities mode */
	#define	PLP_EPP_MODE	0x80	/* enhanced parallel port mode*/
	#define	PLP_TEST_MODE	0xC0	/* FIFO test mode */
	#define	PLP_CFG_MODE	0xE0	/* configuration mode */
	#define	PLP_NULL_MODE	0xFF	/* configuration mode */

	#define ECR_FAULTZ_ENBL	0x10    /* ECP nFault intr enable: bit(4)
					 * 0=enable, 1=disable */
	#define	ECR_DMA_ENBL	0x08	/* EPP/ECP DMA enable: bit(3) */
	#define	ECR_SRVC_INTR	0x04	/* EPP/ECP DMA & service intr: bit(2) */
	#define	ECR_FIFO_FULL	0x02	/* ECP FIFO full: bit(1) */
	#define	ECR_FIFO_EMPTY	0x01	/* ECP FIFO empty: bit(0) */

} plp_ecpctrl_t;

	
/* Compatable Interface Signals */
#define CMP_NSLCTIN	DCR_SLCTIN	/* Comp: nSelectIn */
#define CMP_NINIT	DCR_NINIT	/* Comp: nInit */
#define CMP_NAUTOFD	DCR_AFD		/* Comp: nAutoFd */
#define CMP_NSTRB	DCR_STB		/* Comp: nStrobe */
#define CMP_BUSYZ	DSR_BUSYZ	/* Comp: Busy */
#define CMP_NACK	DSR_ACKZ	/* Comp: nAck */
#define CMP_PE		DSR_PE		/* Comp: PError */
#define CMP_SLCT	DSR_SLCT	/* Comp: Select */
#define CMP_NFAULT	DSR_FAULTZ	/* Comp: nFault */

/* EPP Interface Signals */
#define EPP_NASTRB	CMP_NSLCTIN	/* EPP: nAStrb     - Comp: nSelectIn */
#define EPP_NINIT	CMP_NINIT	/* EPP: nInit      - Comp: nInit */
#define EPP_NDSTRB	CMP_NAUTOFD	/* EPP: nDStrb     - Comp: nAutoFd */
#define EPP_NWRITE	CMP_NSTRB	/* EPP: nWrite     - Comp: nStrobe */
#define EPP_NWAIT	CMP_BUSYZ	/* EPP: nWait      - Comp: Busy */
#define EPP_INTR	CMP_NACK	/* EPP: Intr       - Comp: nAck */
#define EPP_USERDEF1	CMP_PE		/* EPP: User Def 1 - Comp: PError */
#define EPP_USERDEF3	CMP_SLCT	/* EPP: User Def 3 - Comp: Select */
#define EPP_USERDEF2	CMP_NFAULT	/* EPP: User Def 2 - Comp: nFault */

/* ECP Interface Signals */
#define ECP_ACTIVE	CMP_NSLCTIN	/* ECP: 1284Active  - Comp: nSelectIn */
#define ECP_NREVSREQ	CMP_NINIT	/* ECP: nReverseReq - Comp: nInit */
#define ECP_HOSTACK	CMP_NAUTOFD	/* ECP: HostAck     - Comp: nAutoFd */
#define ECP_HOSTCLK	CMP_NSTRB	/* ECP: HostClk     - Comp: nStrobe */
#define ECP_PERIPHACK	CMP_BUSYZ	/* ECP: PeriphAck   - Comp: Busy */
#define ECP_PERIPHCLK	CMP_NACK	/* ECP: PeriphClk   - Comp: nAck */
#define ECP_ACKZREVS	CMP_PE		/* ECP: nAckReserve - Comp: PError */
#define ECP_XFLAG	CMP_SLCT	/* ECP: XFlag       - Comp: Select */
#define ECP_NPERIPHREQ	CMP_NFAULT	/* ECP: nPeriphReq  - Comp: nFault */


/******************************************************************************
 *
 * Data Structure for MACE Parallel Interface DMA Control Registers
 *
 ******************************************************************************
 */
/* MACE Parallel Interface DMA Context Register */
typedef struct dma_cntxt_s {
	union {
	    __uint64_t	_cntxt;
	    /* __uint64_t	_fglast:1, _bcount:31, _dmaaddr:32; */
	    __uint64_t	_last:1, _filler:19, _bcount:12, _addr:32;
	} _dma_cntxt;

	#define dma_cntxt	_dma_cntxt._cntxt
	#define last_cntxt	_dma_cntxt._last
	#define dma_bcount	_dma_cntxt._bcount
	#define dma_addr	_dma_cntxt._addr
} dma_cntxt_t;


/* MACE Parallel Interface DMA Control Register */
typedef struct plp_dmactrl_s {
	__uint64_t		cntxt0;		/* DMA buffer context A */
	__uint64_t		cntxt1;		/* DMA buffer context B */

	#define PLP_DMA_CTX0	0		/* context A */
	#define PLP_DMA_CTX1	1		/* context B */

	/* DMA Control & Status Register */
	__uint64_t		ctrl;		/* control & status */

	#define MACE_DMA_CTX_VALID(x)	(0x20>>x) /* x=0:cntxtA; x=1:cntxtB */
	#define MACE_DMA_CTXS_VALID	0x18LL	/* cntxtA & cntxtB are valid */
	#define MACE_DMA_CTX0_VALID	0x10LL	/* 1:valid;  0:invalid */
	#define MACE_DMA_CTX1_VALID	0x08LL	/* 1:valid;  0:invalid */
	#define MACE_DMA_RESET		0x04LL	/* 1:reset;  0:active */
	#define MACE_DMA_ENBL		0x02LL	/* 1:enable; 0:disable */
	#define MACE_DMA_DIR		0x01LL	/* 1:input;  0:output */
	#define MACE_DMA_INPUT		0x01LL

	/* DMA Diagnostic Register */
	__uint64_t		diag;		/* diagnostic */

	#define MACE_DMA_ACTIVE		0x2	/* 1:active; 0:idle */
	#define MACE_DMA_INUSE_CTX	0x1	/* 1:context B; 0:context A */
	#define MACE_DMA_RESID		0x0FFF	/* bytes left in context */

} plp_dmactrl_t;


/******************************************************************************
 *
 * Data Structure for MACE ISA Peripheral Control Registers
 *
 ******************************************************************************
 */
typedef struct mace_isa_s {
	/* Ring Buffer Base Address & Reset Control Register */
	__uint64_t		dev_ctrl;
	#define	ISA_DEV_RESET	0x01LL

	/* Flash ROM Control Register */
	__uint64_t		flash_rom_ctrl;

	/* Interrupt Status Register */
	__uint64_t		intr_status;

	#define	PLP_DEV_IRQ	0x10000LL
	#define	PLP_CTX0_IRQ	0x20000LL
	#define	PLP_CTX1_IRQ	0x40000LL
	#define	PLP_CTX_IRQ	0x20000LL
	#define	PLP_CTXS_IRQ	0x60000LL
	#define	PLP_MEM_IRQ	0x80000LL
	#define	PLP_IRQS	0xf0000LL
	#define	PLP_CTXS_IRQ_SHIFT	17

	/* Interrupt Mask Register */
	__uint64_t		intr_mask;

} mace_isa_t;


/******************************************************************************
 *
 * Data Structure for Parallel Interface Input Buffer
 *
 ******************************************************************************
 */
typedef struct plp_rbuf_s {
    volatile uint_t		bcount;		/* byte count */
    volatile caddr_t		vaddr;		/* input buffer */
    volatile caddr_t		usrbuf;		/* user input buffer */
} plp_rbuf_t;


/******************************************************************************
 *
 * data structure for plp parallel port driver interface
 *
 ******************************************************************************
 */

typedef struct plp_s {
    uchar_t		state;		/* software state of port */
    uchar_t		sav_state;	/* saved software state of port */
#define PLP_ALIVE	0x1		/* parallel port exists */
#define PLP_WROPEN	0x2		/* open for writing */
#define PLP_RDOPEN	0x4		/* open for reading */
#define PLP_NBIO	0x8		/* set for non-blocking reads */
#define PLP_PLOCK	0x10		/* parallel port lock */
#define PLP_INUSE	0x20		/* parallel port is busy */
#define PLP_OCCUPIED    0x40		/* parallel port is occupied */
#define PLP_PFD		0x80		/* parallel floppy is in use */
#define PLP_PRN		0x00		/* parallel printer is in use */

    uchar_t		phase;		/* operating phase */
#define CMPTBL_IDLE_PHASE		0x00
#define CMPTBL_XFR_PHASE		0x01
#define EPP_IDLE_PHASE			0x02
#define NEGOTIATE_PHASE			0x04
#define SETUP_PHASE			0x05
#define ECP_IDLE_PHASE			0x06
#define FWD_IDLE_PHASE			0x06
#define FWD_XFR_PHASE			0x07
#define FWD_TO_RVS_PHASE		0x09
#define ECP_RVS_IDLE_PHASE		0x0A
#define RVS_XFR_PHASE			0x0B
#define RVS_TO_FWD_PHASE		0x0C
#define TERMINATE_PHASE			0x0F
#define	HOST_BUSY_DATA_READY_PHASE	0x10
#define	HOST_BUSY_DATA_NOT_READY_PHASE	0x11

    uchar_t		opmode;		/* operation mode */
    uchar_t		sav_opmode;	/* saved operation mode */

    dev_t		devt;		/* saved dev */
#ifndef _IRIX6_3
    vertex_hdl_t        cvhdl;		/* connect vertex */
    vertex_hdl_t        pvhdl;		/* prallel vertex */
    vertex_hdl_t        fvhdl;		/* prallel floppy vertex */
#endif

    volatile long long	*bufaddr;	/* base addr of MACE DMA ring */	

    uchar_t		rbufctrl;	/* status of input data in rbufs */
#define RBUFA_DATA_READY 0x1		/* data in rbuf A is ready to use */
#define RBUFB_DATA_READY 0x2		/* data in rbuf B is ready to use */
#define RBUFS_DATA_READY 0x3		/* data in rbuf A or B are ready */

    uchar_t		cntxts,		/* context available flag, A:0x1,B:0x2*/
			nxtcntxt,	/* next available context A:0, B:1 */
    			curcntxt;	/* first configurated context */
#define CTX_A           0x1             /* context A */
#define CTX_B           0x2             /* context B */
#define	ALL_CTXS	0x3		/* context A & B */

    uchar_t		iodir;		/* I/O direction */
#define	PLP_INPUT	0x1
#define	PLP_OUTPUT	0x0

    _macereg_t		irqmask;	/* interrupt mask */

#ifdef ONSPEC_BUG_RD_WAR
    caddr_t		pfdrbuf[2];	/* (two) pfd input check buffers */
#endif
    plp_rbuf_t		rbuf[2];	/* (two) 4k-page input buffers */
    buf_t		*bp;		/* pointer to user buffer */
    buf_t		fdcbuf;		/* buffer for fdc */

    mutex_t		mlock,		/* opmode and state lock */
    			plock,		/* protocol lock */
    			xlock,		/* context lock */
			tmoutlock;      /* timeout lock */
    sema_t
			rlock,		/* serialize read access */	
			wlock,		/* serialize write access */
			dlock;		/* serialize dma channel access */
						
    sv_t		dmaxfr,		/* dma xfer sync */
			sv_tmout;       /* timeout sync var */    

    sema_t		dmacntxts;    

    int			prlcks;		/* protocol lock counter */

#define PLP_BUSY	0x1
#define PLP_WANTED	0x2

    int			rtmout,		/* read timeout in secs */
    			wtmout,		/* write timeout in secs */
    			tmout,		/* read/write timeout in secs */
			tmoutid;	/* read/write timeout id */
#define	PLP_RTO		(1 * HZ)	/* 1 sec timeout on DMA read */
#define	PLP_WTO		0		/* never timeout on DMA write */

    time_t		dma_tmstamp;	/* value of lbolt at last DMA started */
    
    uint64_t            diag;           /* copy of PLP_DIAG_REG */
    int                 tmout_ctxts;    /* copy of cntxts saved for tmout */
    uint32_t            loadA,          /* byte count for context A */
                        loadB;          /* byte count for context B */
} plp_t;

#define NPLP		1	       /* maximum parallel ports on O2 */

#ifndef _IRIX6_3
/******************************************************************************
 *
 * Data Structure for Parallel Interfaces (Centronics, ECP, EPP) 
 *
 ******************************************************************************
 */
typedef struct plpif_s {
    plp_t		*plp;		/* pointer to plp struct */
    vertex_hdl_t	pvhdl;		/* hwgraph vertex */
    uchar_t		opmode;		/* operation mode */
} plpif_t;


#define	EDGE_LBL_NODE_IO "node/io"	/* /hw/node/io (connection point) */
#define	EDGE_LBL_PARALLEL "parallel"	/* /hw/parallel (alias edge) */
#define	EDGE_LBL_ECPLP	"ecplp"		/* /hw/node/io/ecplp */
#define	EDGE_LBL_PLP	"plp"		/* /hw/.../ecplp/plp (Centronics) */
#define	EDGE_LBL_EPP	"epp"		/* /hw/.../ecplp/epp (EPP) */
#ifndef EDGE_LBL_ECP
#define	EDGE_LBL_ECP	"ecp"		/* /hw/.../ecplp/ecp (ECP) */
#endif

#define	EDGE_LBL_PFD	"pfd"		/* /hw/.../ecplp/pfd */
#define	EDGE_LBL_PFD2HD	"pfd.2hd"	/* /hw/.../ecplp/pfd/pfd.2hd */
#define	EDGE_LBL_PFD2DD	"pfd.2dd"	/* /hw/.../ecplp/pfd/pfd.2dd */
#endif

#define	NEW(x, t)	(x = (t)kmem_zalloc(sizeof(*(x)), KM_SLEEP))
#define	FREE(x)		(kmem_free((x), sizeof(*(x))), (x) = 0)


/******************************************************************************
 *
 * MACE Address Mapping for Parallel Interface Control Registers
 *
 ******************************************************************************
 */
 
/* Address Map of MACE ISA Interface Registers
 * #define MACE_BASE			(             0x1F000000)
 * #define MACE_PERIF			(MACE_BASE  + 0x00300000)
 * #define MACE_ISA			(MACE_PERIF + 0x00010000)
 * #define ISA_PLP_BASE			(MACE_ISA   + 0x00004000)
 */
#define	MACE_ISA_CTRL_REG		PHYS_TO_K1(MACE_ISA+0x0000)
#define ISA_RING_RESET_REG		PHYS_TO_K1(ISA_RINGBASE)
#define ISA_IRQ_STS_REG			PHYS_TO_K1(ISA_INT_STS_REG)
#define ISA_IRQ_MSK_REG			PHYS_TO_K1(ISA_INT_MSK_REG) 
#define ISA_PLP_BASE			PHYS_TO_K1(MACE_ISA+0x4000)

#define	MACE_UST_REG			PHYS_TO_K1(MACE_UST_MSC)


/* Address Map of ISA Parallel Port Interface Registers
 */
#define	PLP_DMA_CTX0_REG		(ISA_PLP_BASE+0x0)
#define	PLP_DMA_CTX1_REG		(ISA_PLP_BASE+0x8)
#define	PLP_DMA_CTRL_REG		(ISA_PLP_BASE+0x10)
#define	PLP_DMA_DIAG_REG		(ISA_PLP_BASE+0x18)


/* Address Map of External ISA Parallel Interface Registers
 * #define MACE_BASE			(               0x1F000000)
 * #define MACE_ISA_EXT			(MACE_BASE    + 0x00380000)
 * #define ISA_EPP_BASE			(MACE_ISA_EXT + 0x00000000)
 * #define ISA_ECP_BASE			(MACE_ISA_EXT + 0x00008000)
 */
#define PLP_LPT_DATA_REG		PHYS_TO_K1(ISA_EPP_BASE + 0x000)	
#define PLP_ECP_AFIFO_REG		PHYS_TO_K1(ISA_EPP_BASE + 0x000)	
#define PLP_LPT_DSR_REG			PHYS_TO_K1(ISA_EPP_BASE + 0x100)	
#define PLP_LPT_DCR_REG			PHYS_TO_K1(ISA_EPP_BASE + 0x200)	
#define PLP_EPP_ADDR_REG		PHYS_TO_K1(ISA_EPP_BASE + 0x300)	
#define PLP_EPP_DATA_REG		PHYS_TO_K1(ISA_EPP_BASE + 0x400)	
#define PLP_EPP_DATA1_REG		PHYS_TO_K1(ISA_EPP_BASE + 0x500)	
#define PLP_EPP_DATA2_REG		PHYS_TO_K1(ISA_EPP_BASE + 0x600)	
#define PLP_EPP_DATA3_REG		PHYS_TO_K1(ISA_EPP_BASE + 0x700)	

#define PLP_ECP_DFIFO_REG		PHYS_TO_K1(ISA_ECP_BASE + 0x000)	
#define PLP_ECP_CFGA_REG		PHYS_TO_K1(ISA_ECP_BASE + 0x000)	
#define PLP_ECP_CFGB_REG		PHYS_TO_K1(ISA_ECP_BASE + 0x100)	
#define PLP_LPT_ECR_REG			PHYS_TO_K1(ISA_ECP_BASE + 0x200)	


/******************************************************************************
 *
 * Macro Definitions
 *
 ******************************************************************************
 */

#ifdef _IRIX6_3
#define	PLP_LOCK(m)	(m = spl5())
#define	PLP_UNLOCK(m)	splx(m)
#else
#define	PLP_LOCK(m)	mutex_lock(m, PZERO)		
#define	PLP_UNLOCK(m)	mutex_unlock(m)		
#endif

#define	PLP_NEXT_CTX(x)		(!(x))		/* next context */
#define PLP_DMA_INUSE_CTX(x)	(1 << ((x) & MACE_DMA_INUSE_CTX))
#define PLP_DMA_ACTIVE(x)	((x) & MACE_DMA_ACTIVE)
#define PLP_DMA_RESID(x)	(((x)>>2) & MACE_DMA_RESID)

#define	LAST_FLAG		(1LL << 63)

#define	LPT_DELAY(t)		delay(t)	/* 10 ms delay */
#define	MS_DELAY(t)		fasthz_delay(t)	/*  1 ms delay */
#define	NS_DELAY(t)		nano_delay(t)	/*  1 ns delay */
#define	US_DELAY(t)		us_delaybus(t)	/*  1 us delay */
#define	LPT_TM_SGNL_HOLD	4		/* 4 us */
#define	LPT_TM_40MS		4		/* 40 ms */
#define	LPT_TM_MAX_WAIT		LPT_TM_LONG_WAIT+4 /* 40 ms */
#define	LPT_TM_LONG_WAIT	200		/* 200 us */

/*
 * XXX:
 * The pciio_pio_* calls are part of the CRIME 1.1 PIO read bug workaround.
 * Don't change them unless you understand the implications.
 */
#define	LPT_CTRL_RD(reg)	pciio_pio_read8((volatile uint8_t *)(reg))
#define	LPT_CTRL_WR(reg, val)	pciio_pio_write8((uint8_t)(val), (volatile uint8_t *)(reg))
#define	LPT_CTRL_SET(reg, val)	LPT_CTRL_WR((reg), LPT_CTRL_RD((reg)) | (val))
#define	LPT_CTRL_CLR(reg, val)	LPT_CTRL_WR((reg), LPT_CTRL_RD((reg)) & ~(val))

	
#define MACE_ISA_RD(reg)	pciio_pio_read64((volatile uint64_t *)(reg))
#define MACE_ISA_WR(reg, val)	pciio_pio_write64((uint64_t)(val), (volatile uint64_t *)(reg))
#define MACE_ISA_SET(reg, val)	MACE_ISA_WR((reg), MACE_ISA_RD((reg)) | (val))
#define MACE_ISA_CLR(reg, val)	MACE_ISA_WR((reg), MACE_ISA_RD((reg)) & ~(val))


#define PLP_DMA_RESET(x) \
	{ MACE_ISA_SET(PLP_DMA_CTRL_REG, MACE_DMA_RESET); \
	  MACE_ISA_CLR(PLP_DMA_CTRL_REG, MACE_DMA_RESET); }

#define	PLP_DMA_ENBL(x) \
	{ LPT_CTRL_WR(PLP_LPT_ECR_REG, ((LPT_CTRL_RD(PLP_LPT_ECR_REG) & \
		~ECR_SRVC_INTR) | ECR_DMA_ENBL)); \
	  MACE_ISA_SET(PLP_DMA_CTRL_REG, MACE_DMA_ENBL); }

#define PLP_DMA_STOP(x) \
	{ MACE_ISA_CLR(PLP_DMA_CTRL_REG, MACE_DMA_ENBL); \
	  LPT_CTRL_WR(PLP_LPT_ECR_REG, (LPT_CTRL_RD(PLP_LPT_ECR_REG) & \
		~ECR_DMA_ENBL)); }

#define PLP_SET_INPUT_DIR(p) \
	{ (p)->iodir = PLP_INPUT; \
	  LPT_CTRL_SET(PLP_LPT_DCR_REG, DCR_DIR); \
	  MACE_ISA_SET(PLP_DMA_CTRL_REG, MACE_DMA_INPUT); }

#define PLP_CLR_INPUT_DIR(p) \
	{ (p)->iodir = PLP_OUTPUT; \
	  LPT_CTRL_CLR(PLP_LPT_DCR_REG, DCR_DIR); \
	  MACE_ISA_CLR(PLP_DMA_CTRL_REG, MACE_DMA_INPUT); }

#define PLP_SET_OPMODE(x, y) \
	{ LPT_CTRL_WR(PLP_LPT_ECR_REG, \
		((LPT_CTRL_RD(PLP_LPT_ECR_REG) & ~PLP_OP_MODE) | (y))); \
	  (x)->opmode = (y); }

#define PLP_CLR_OPMODE(x) \
	LPT_CTRL_CLR(PLP_LPT_ECR_REG, PLP_OP_MODE);

#define PLP_ISA_RESET(x) \
	{ MACE_ISA_SET(ISA_RING_RESET_REG, ISA_DEV_RESET); \
	  MACE_ISA_CLR(ISA_RING_RESET_REG, ISA_DEV_RESET); }

#ifdef _IRIX6_3
#define PLP_GET_OPMODE(x)	(((geteminor(x) >> 4) << 5)
#define PLP_DEV2PLP(x)		((x) ? (&plp_port[PLP_UNIT(x)]) : plpinfo)
#else
#define PLP_GET_OPMODE(x) \
	(((plpif_t *)device_info_get(dev_to_vhdl(x)))->opmode)
#define PLP_DEV2PLP(x) \
	((x) ? ((plp_t *)(((plpif_t *)device_info_get(dev_to_vhdl(x)))->plp)) \
	: plpinfo)
#endif


/******************************************************************************
 * Definitions for EPP
 ******************************************************************************
 */
#define	PLP_EPP_RD(reg) \
	( LPT_CTRL_WR(PLP_EPP_ADDR_REG, (reg)), \
	  LPT_CTRL_RD(PLP_EPP_DATA_REG) )

#define	PLP_EPP_WR(reg, byte) \
	( LPT_CTRL_WR(PLP_EPP_ADDR_REG, (reg)), \
	  LPT_CTRL_WR(PLP_EPP_DATA_REG, (byte)) )


/* Next three additions allow same parallel port to be used as
 * either output or bidirectional port.
 */
#define PLP_UNIT_MASK		0x0f    /* bit(3:0): unit number */
#define PLP_OPMODE_MASK		0xf0    /* bit(7:4): operqtion mode */
#define PLP_UNIT(dev)		(minor(dev) & PLP_UNIT_MASK)

/* modify these parameters for best performance
 */
#define NRPPP   	        2	/* number of read pages per port */
#define NRP     	        (NPLP * NRPPP)  /* read pages */

/*
 * IEEE P1284 Extensibility Request Value
 */
#define EXTREQ_NIBL_MODE	0x00
#define EXTREQ_BYTE_MODE	0x01
#define EXTREQ_ECP_MODE		0x10
#define EXTREQ_ECP_DEVID_MODE	0x14
#define EXTREQ_ECP_RLE_MODE	0x20
#define EXTREQ_EPP_MODE		0x40
#define EXTREQ_ANY_MODE		0x80
#define	EXTREQ_EXTLINK_MODE	0x00


/******************************************************************************
 * Definitions for OnSpec Egret Controller
 ******************************************************************************
 */
#define	PLP_ONSPEC_DATA_REG	0x0
#define	PLP_ONSPEC_CMD_REG	0x1
#define	PLP_ONSPEC_CFG1_REG	0x2
#define	PLP_ONSPEC_CFG2_REG	0x3
#define	PLP_ONSPEC_VER_REG	0x4
#define	PLP_ONSPEC_IFIFO_REG	0x5
#define	PLP_ONSPEC_FIFO_REG	0x6
#define	PLP_ONSPEC_REL_REG	0x7
#define	PLP_ONSPEC_LEVEL0_REG	0x8
#define	PLP_ONSPEC_LEVEL1_REG	0x9
#define	PLP_ONSPEC_LEVEL2_REG	0xa

#define	PLP_ONSPEC_EPP		0x1
#define	PLP_ONSPEC_DRVRESET	0x2
#define	PLP_ONSPEC_IDE8		0x4
#define	PLP_ONSPEC_IDE16	0x8

#define	PLP_ONSPEC_IFIFO_ENBL	0x1
#define	PLP_ONSPEC_FIFO_ENBL	0x2
#define	PLP_ONSPEC_FIFO_OFF	0x0
#define	PLP_ONSPEC_FIFO_HALT	0x8
#define	PLP_ONSPEC_FIFO_RD	0x10

#define	PLP_FDC_FIFO_CTRL_LEVEL	0x4
#define	PLP_FDC_FIFO_CTRL_ON	0x2
#define	PLP_FDC_FIFO_CTRL_OFF	0x1

#define PLP_ONSPEC_ENBL(id) \
         ( plpfdc_cpp(PLP_CPP_CMD_ASSGN, 0), \
           plpfdc_cpp(PLP_CPP_CMD_SLCT, id) )

#define	PLP_ONSPEC_REL(id) \
           plpfdc_cpp(PLP_CPP_CMD_DESLCT, id)

#define	PLP_ONSPEC_STD_ADDR(reg) \
	{ LPT_CTRL_WR(PLP_LPT_DATA_REG, (reg)); \
	  US_DELAY(LPT_TM_SGNL_HOLD); \
	  LPT_CTRL_WR(PLP_LPT_DCR_REG, (DCR_NINIT | DCR_STB)); \
	  US_DELAY(LPT_TM_SGNL_HOLD); \
	  LPT_CTRL_WR(PLP_LPT_DCR_REG, (DCR_SLCTIN | DCR_NINIT | DCR_STB)); \
	  US_DELAY(LPT_TM_SGNL_HOLD); \
	  LPT_CTRL_WR(PLP_LPT_DCR_REG, (DCR_NINIT | DCR_STB)); \
	  US_DELAY(LPT_TM_SGNL_HOLD); \
	  LPT_CTRL_WR(PLP_LPT_DCR_REG, DCR_NINIT); }

#define	PLP_ONSPEC_STD_DATA_WR(byte) \
	{ LPT_CTRL_WR(PLP_LPT_DATA_REG, (byte)); \
	  US_DELAY(LPT_TM_SGNL_HOLD); \
	  LPT_CTRL_WR(PLP_LPT_DCR_REG, (DCR_NINIT | DCR_STB)); \
	  US_DELAY(LPT_TM_SGNL_HOLD); \
	  LPT_CTRL_WR(PLP_LPT_DCR_REG, (DCR_NINIT | DCR_AFD | DCR_STB)); \
	  US_DELAY(LPT_TM_SGNL_HOLD); \
	  LPT_CTRL_WR(PLP_LPT_DCR_REG, (DCR_NINIT | DCR_STB)); \
	  US_DELAY(LPT_TM_SGNL_HOLD); \
	  LPT_CTRL_WR(PLP_LPT_DCR_REG, DCR_NINIT); }

#define	PLP_ONSPEC_STD_WR(reg, byte) \
	{ PLP_ONSPEC_STD_ADDR((reg)); \
	  PLP_ONSPEC_STD_DATA_WR((byte)); }

#define	PLP_ONSPEC_LEVEL() \
	( PLP_EPP_RD(PLP_ONSPEC_LEVEL0_REG) | \
	 (PLP_EPP_RD(PLP_ONSPEC_LEVEL1_REG) << 8) | \
	 (PLP_EPP_RD(PLP_ONSPEC_LEVEL2_REG) << 16) )


/******************************************************************************
 * Definitions for Parallel Floppy Controller
 ******************************************************************************
 */
#define	PLP_FDC_DSR		0x1d
#define	PLP_FDC_CCR		0x1d
#define	PLP_FDC_DATA		0x15
#define	PLP_FDC_MSR		0x11
#define	PLP_FDC_DOR		0x09
#define	PLP_FDC_DMATC		0x42
#define	PLP_FDC_AEN		0x02
#define	PLP_FDC_TC		0x00

#define	PLP_FDC_SCTRSZ		512
#define	PLP_FDC_TM_FIFOWAIT	500	/* 500 x 1 ms */

#define	PLP_FDC_HD_OUT		0x80	/* 2DD media is in */


/******************************************************************************
 * Definitions for IEEE-1284.P3 Command Packet Protocol (CPP)
 ******************************************************************************
 */
/* CPP commands */
#define	PLP_CPP_CMD_ASSGN	0x00
#define	PLP_CPP_CMD_SLCT	0x20
#define	PLP_CPP_CMD_DESLCT	0x30
#define	PLP_CPP_CMD_QUERY	0x08
#define	PLP_CPP_CMD_SET_IRQ	0x58
#define	PLP_CPP_CMD_CLR_IRQ	0x50

/* CPP status */
#define	PLP_CPP_CHIP_SLCT	DSR_FAULTZ
#define	PLP_CPP_LAST_DEV	DSR_ACKZ
#define	PLP_CPP_EPP_DEV		0x01
#define	PLP_CPP_INTR_IRQ	0x80
#define	PLP_CPP_EXTR_IRQ	0x08
#define	PLP_CPP_IRQS		0x70

#define	PLP_CPP_ALRDY_SLCT	0x02

#define	PLP_CPP_MAX_DEV		0x4

#endif /* _LANGUAGE_C */
