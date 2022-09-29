/* if_med.h - Matrix DB-ETH ethernet driver header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
02f,22sep92,rrr  added support for c++
02e,26may92,rrr  the tree shuffle
02d,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
02c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02b,26jun90,hjb  upgrade to use cluster and if_subr routines;
	   +dab  integrated in the new distribution from Matrix.
02a,05may90,dab  cleanup of Matrix version 01g.
01a,24sep89,srm  written.
*/

#ifndef __INCif_medh
#define __INCif_medh

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DB_ETH definitions
 */

/* DB-ETH local memory map. */

#define  dbeth_NIC_MEM                   0x00000
#define  dbeth_DBETH_CPU_BFR             0x10000
#define  dbeth_ETH_ADDR_ROM              0x20000
#define  dbeth_NIC_ADDR                  0x30000

/* transmit and receive buffer parameters */

#define  dbeth_NIC_MEM_SIZE	 0x10000
#define  dbeth_NIC_RECV_BFR	 (dbeth_NIC_MEM+0)
#define  dbeth_NIC_RECV_BFR_SIZE ((dbeth_NIC_MEM_SIZE*7)/8)
#define  dbeth_NIC_RECV_BFR_END	 (dbeth_NIC_RECV_BFR+dbeth_NIC_RECV_BFR_SIZE)
#define  dbeth_NIC_XMIT_BFR	 (dbeth_NIC_MEM+dbeth_NIC_RECV_BFR_SIZE)
#define  dbeth_NIC_XMIT_BFR_SIZE (dbeth_NIC_MEM_SIZE-dbeth_NIC_RECV_BFR_SIZE)

/*
 * NIC definitions
 */

#define  nic_PAGE_SIZE               256
#define  nic_CPU_TO_NIC_ADRS(Adrs)   ((u_char)((u_long)Adrs >> 8))

/*
 * NIC received packet header.
 */

typedef struct
    {
    u_char nextPkt;
    u_char recvStatus;
    u_char byteCntL;
    u_char byteCntH;
    struct ether_header eh;
    } nic_RecvHdr_s;

/*
 * All NIC registers are accessable at odd byte locations.
 */

typedef struct
    {
    u_char pad;      /* pad even (most significant) byte */
    u_char reg;      /* the actual NIC register          */
    } NIC_REG;

typedef	union
    {
    NIC_REG reg [16];
    struct
	{
	NIC_REG cr;
	NIC_REG pstart;
	NIC_REG pstop;
	NIC_REG bnry;
	NIC_REG tpsr;
	NIC_REG tbcr0;
	NIC_REG tbcr1;
	NIC_REG isr;
	NIC_REG rsar0;
	NIC_REG	rsar1;
	NIC_REG rbcr0;
	NIC_REG rbcr1;
	NIC_REG rcr;
	NIC_REG tcr;
	NIC_REG dcr;
	NIC_REG imr;
	} nic_pg0;
    struct
	{
	NIC_REG cr;
	NIC_REG clda0;
	NIC_REG clda1;
	NIC_REG bnry;
	NIC_REG tsr;
	NIC_REG ncr;
	NIC_REG fifo;
	NIC_REG isr;
	NIC_REG crda0;
	NIC_REG	crda1;
	NIC_REG reserved0;
	NIC_REG reserved1;
	NIC_REG rsr;
	NIC_REG cntr0;
	NIC_REG cntr1;
	NIC_REG cntr2;
	} nic_pg0_read;
    struct
	{
	NIC_REG cr;
	NIC_REG par0;
	NIC_REG par1;
	NIC_REG par2;
	NIC_REG par3;
	NIC_REG par4;
	NIC_REG par5;
	NIC_REG curr;
	NIC_REG mar0;
	NIC_REG mar1;
	NIC_REG mar2;
	NIC_REG mar3;
	NIC_REG mar4;
	NIC_REG mar5;
	NIC_REG mar6;
	NIC_REG mar7;
	} nic_pg1;
    struct
	{
	NIC_REG cr;
	NIC_REG clda0;
	NIC_REG clda1;
	NIC_REG rnpp;
	NIC_REG res;
	NIC_REG lnpp;
	NIC_REG upper;
	NIC_REG lower;
	NIC_REG reg[4];          /* reserved */
	NIC_REG rcr_read;
	NIC_REG tcr_read;
	NIC_REG dcr_read;
	NIC_REG imr_read;
	} nic_pg2;
    } NIC_DEVICE;

#define Cr	nic_pg0.cr.reg
#define Pstart	nic_pg0.pstart.reg
#define Pstop	nic_pg0.pstop.reg
#define Bnry	nic_pg0.bnry.reg
#define Tpsr	nic_pg0.tpsr.reg
#define Tsr	nic_pg0_read.tsr.reg
#define Tbcr0	nic_pg0.tbcr0.reg
#define Tbcr1	nic_pg0.tbcr1.reg
#define Isr	nic_pg0.isr.reg
#define Rsar0	nic_pg0.rsar0.reg
#define Rsar1	nic_pg0.rsar1.reg
#define Rbcr0	nic_pg0.rbcr0.reg
#define Rbcr1	nic_pg0.rbcr1.reg
#define Rcr	nic_pg0.rcr.reg
#define Tcr	nic_pg0.tcr.reg
#define FaeErr	nic_pg0_read.cntr0.reg
#define Dcr	nic_pg0.dcr.reg
#define CrcErr	nic_pg0_read.cntr1.reg
#define Imr	nic_pg0.imr.reg
#define MspErr	nic_pg0_read.cntr2.reg

#define Par0	nic_pg1.par0.reg
#define Par1	nic_pg1.par1.reg
#define Par2	nic_pg1.par2.reg
#define Par3	nic_pg1.par3.reg
#define Par4	nic_pg1.par4.reg
#define Par5	nic_pg1.par5.reg
#define Curr	nic_pg1.curr.reg

#define ImrRD	nic_pg2.imr_read.reg

/*
 * CR Register bits - Command Register
 */

#define STP	0x1
#define STA	0x2
#define TXP	0x4
#define RREAD	0x8
#define RWRITE	0x10
#define SPKT	0x18
#define ABORT	0x20
#define RPAGE0	0x00
#define RPAGE1	0x40
#define RPAGE2	0x80

#define nic_XMTR_BUSY  0x4

/*
 * ISR Register - Interrupt Status Register
 */

#define PRX	0x1
#define PTX	0x2
#define RXE	0x4
#define TXE	0x8
#define OVW	0x10
#define CNT	0x20
#define RDC	0x40
#define RST	0x80

#define  nic_STOP_MODE    RST

/*
 * IMR - Interrupt Mask Register - 0F (write-only)
 */

#define  PRXE	0x1
#define  PTXE	0x2
#define  RXEE	0x4
#define  TXEE	0x8
#define  OVWE	0x10
#define  CNTE	0x20
#define  RDCE	0x40

#define  nic_PKT_RECV_NO_ERRORS         PRXE
#define  nic_PKT_RECV_ERROR             RXEE
#define  nic_PKT_XMIT_NO_ERRORS         PTXE
#define  nic_PKT_XMIT_ERROR             TXEE
#define  nic_RECV_BFR_OVF_WARN          OVWE
#define  nic_NET_STATS_CNTR_OVF         CNTE

/*
 * DCR
 */

#define WTS	0x1        /* word-wide DMA transfers        */
#define BOS	0x2        /* 680x0-style byte ordering      */
#define LAS	0x4        /* remote DMA long address select */
#define BMS	0x8        /* transfer FIFO threshold bytes  */
#define ARM	0x10       /* autoinitialize remote DMA      */
#define FT1	0x00       /* FIFO threshold, 1 word         */
#define FT2  	0x20
#define FT4  	0x40
#define FT6   	0x60

/*
 * TCR - Transmit Configuration Register - 0D ( write-only)
 */

#define CRC 	0x1
#define MODE0	0x0
#define MODE1	0x2
#define MODE2	0x4
#define MODE3	0x6
#define ATD	0x8
#define OFST	0x10

#define  nic_NORMAL_XMIT_CTRL     MODE0

/*
 * TSR
 */

#define TPTX	0x1
#define COL	0x4
#define ABT	0x8
#define CRS	0x10
#define FU	0x20
#define CDH	0x40
#define OWC	0x80

/*
 * RCR
 */

#define	SEP	0x1
#define AR	0x2
#define AB	0x4
#define AM	0x8
#define PRO	0x10
#define MON	0x20

/*
 * RSR
 */

#define PRX	0x1
#define CRCR	0x2
#define FAE	0x4
#define FO	0x8
#define MPA	0x10
#define PHY	0x20
#define DIS	0x40
#define DFR	0x80

#define nic_RECV_NO_ERROR  PRX
#define nic_RCVR_DISABLED  DIS
#define nic_RCVR_DEFERRING DFR

#define  NNIC	           1    /* max number of nic controllers */

/*
 * Overflow buffer. This buffer is used when received packets are
 * "wrapped around" in the receive buffer ring by the NIC.
 * Type nic_RecvHdr_s includes "struct ether_header".
 */

#define  OVERFLOW_BFR_SIZE (sizeof (nic_RecvHdr_s) + ETHERMTU)

/* driver statistics, for debug and performance monitoring */

typedef struct
    {
    u_long   noXmitBfrs;
    u_long   intrCount;
    u_long   PRX_intrCount;
    u_long   PTX_intrCount;
    u_long   RXE_intrCount;
    u_long   TXE_intrCount;
    u_long   OVW_intrCount;
    u_long   CNT_intrCount;
    u_long   carrierLost;    /* transmission error, not aborted           */
    u_long   outOfWndColl;   /* transmission error, not aborted           */
    u_long   FIFO_UdrRun;    /* fatal transmission error, aborted         */
    u_long   excessColl;     /* fatal transmission error, aborted         */
    u_long   frameAlign;     /* frame alignment error on packet reception */
    u_long   misMatchCRC;    /* CRC error on packet reception             */
    u_long   missedPkt;      /* packet missed, no receive buffers avail.  */
    } med_Stats_s;

/*_____________________________________________________________________
 * Transmit Queue (xmitQ) definition.
 *_____________________________________________________________________
 *
 *  Transmit Queue - showing transmit buffer control block (xmitBCB_s)
 *                   structure
 *
 *    +-------------------------------------------------------+
 *    |                                                       |
 *    |   +-----------+    +-----------+  //   +-----------+  |
 *    +-->|    next   |--->|    next   |--\\-->|    next   |--+
 *        +-----------+    +-----------+  //   +-----------+
 *        |  bPktRdy  |    |  bPktRdy  |  \\   |  bPktRdy  |
 *        +-----------+    +-----------+  //   +-----------+
 *        |   pktLen  |    |   pktLen  |  \\   |   pktLen  |
 *        +-----------+    +-----------+  //   +-----------+
 *        |  pPktBfr  |    |  pPktBfr  |  \\   |   pPktBfr |
 *        +-----------+    +-----------+  //   +-----------+
 *              |                |                   |
 *              |                |                   |
 *          +---+                |                   |
 *          |    +---------------+                   |
 *          |    |                                   |
 *          |    |                                   |
 *          |    |        NIC accessable memory      |
 *          |    |   +----------------------------+  |  <----+
 *          |    |   |                            |  |       |
 *          +------->|      packet buffer 0       |  |       |
 *               |   |                            |  |       |
 *               |   +----------------------------+  |  <----+
 *               |   |                            |  |       |
 *               +-->|      packet buffer 1       |  |       |- NIC page
 *                   |                            |  |       |  boundaries
 *                   +----------------------------+  |  <----+
 *                   |                            |  |       |
 *                  /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
 *                   |                            |  |       |
 *                   +----------------------------+  |  <----+
 *                   |                            |  |
 *                   |      packet buffer n       |<-+
 *                   |                            |
 *                   +----------------------------+
 *
 *
 *  Transmit Queue - showing buffer indexing
 *
 *    +-----------------------------------------------------------+
 *    |                                                           |
 *    |   +---+    +---+    +---+    +---+    +---+  //    +---+  |
 *    |   |   |    |###|    |###|    |###|    |   |  \\    |   |  |
 *    +-->|   +--->|###+--->|###+--->|###+--->|   +--// -->|   |--+
 *        |   |    |###|    |###|    |###|    |   |  \\    |   |
 *        +---+    +---+    +---+    +---+    +---+  //    +---+
 *                   |                          |
 *                   |                          |
 *                   +-- head                   +-- tail
 *
 *  tail (tail buffer index)
 *    Unless the xmitQ is full, Tail always indexes the next empty
 *    buffer into which a packet to be transmitted may be written.
 *
 *  head (head buffer index)
 *    Unless the xmitQ is empty, Head always indexes the next data
 *    buffer (containing a packet) to be transmitted by the NIC.
 *___________________________________________________________________*/

/*
 * A 'lock' is used as the mutual exclusion mechanism for the NIC transmitter.
 */
typedef unsigned long lockID_t;

#define LOCKED		((lockID_t) 0)
#define UNLOCKED	((lockID_t) 1)

/* Transmit Buffer Control Blocks are referenced by an index.  */

typedef unsigned long  XBIdx_t;       /* Transmit Buffer Index */

/* Transmit Buffer Control Block (xmitBCB) */

typedef struct
    {
    XBIdx_t      next;           /* index of the next xmitBfrContolBlock */
    BOOL         bPktRdy;        /* data is available in packet buffer   */
    u_short      pktLen;
    u_short      dummy;          /* pad to longword */
    char        *pPktBfr;
    } xmitBCB_s;

/*
 * Calculate the maximum transmit buffer size, and round-up to the nearest
 * NIC page size...
 */
#define  XMIT_BFR_SIZE         (((ETHERMTU + sizeof (struct ether_header)) \
                                + (nic_PAGE_SIZE - 1))                      \
                                & ~(nic_PAGE_SIZE - 1))

#define  NUM_XMIT_BFRS         (dbeth_NIC_XMIT_BFR_SIZE / XMIT_BFR_SIZE)

/* Transmit Queue Control Block (xmitQCB). */

typedef struct
    {
    XBIdx_t        head;
    XBIdx_t        tail;
    BOOL           bEmpty;
    BOOL           bFull;
    lockID_t       lockNIC_Xmtr;
    xmitBCB_s      xmitBCB [NUM_XMIT_BFRS];
    } xmitQCB_s;


/*
 * MED definitions
 */

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * es_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */

struct med_softc
    {
    struct arpcom  es_ac;         /* common ethernet structures               */
    NIC_DEVICE    *nicAddr;       /* address of nic Chip                      */
    int            nicIntLevel;   /* interrupt level                          */
    int            nicIntVec;     /* interrupt vector                         */
    BOOL           bPromisc;      /* if TRUE, NIC retrieves all packets       */
    nic_RecvHdr_s *pOvfRecvBfr;   /* buffer for wrapped receive packets       */
    BOOL           bxmitBfrBusy;  /* for initial version, single xmit buffer  */
    BOOL           bRecvBfrOvfl;  /* when TRUE, receive packets not processed */
    BOOL           bFirstInit;    /* TRUE when NIC/driver first initialized   */
    xmitQCB_s      xmitQCB;       /* transmit queue control block             */
    u_char         pktNextToRead; /* next received packet to read             */
    u_char         currentIMR;    /* NIC IMR shadow register                  */
    };

#define es_if		es_ac.ac_if     /* network-visible interface */
#define es_enaddr       es_ac.ac_enaddr /* hardware ethernet address */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_medh */
