/*
 * Moosehead internal fast ethernet interface
 */

#ifndef _mace_ec_h_
#define _mace_ec_h_

/*
 * mac110
 */
typedef volatile struct _mac110 {
  long long macControl;		/* 00 Control an config */
  long long interruptStatus; 	/* 08 Interrupt status */
  long long dmaCtrl;		/* 10 Tx and rx DMA control */
  long long rxIntrTimerCtrl; 	/* 18 Rx interrupt delay timer */
  long long txIntrAlias;	/* 20 Alias of tx interrupt bits */
  long long rxIntrAlias;	/* 28 Alias of rx interrupt bits */
  struct {
       long reserved;
       long   	rptr:16;     /* ...ring buffer read pointer */
       long   	wptr:16;     /* ...ring buffer write pointer */
  } _txInfo;			/* 30 Tx DMA ring buffer ptrs */
#define txRingBufReadPtr _txInfo.rptr
#define txRingBufWritePtr _txInfo.wptr
  long long txIalias;		/* 38 ...alias of above register */
  struct {
       long     r1;
       long         r2:8;
       long        wptr:8,      /* MCL fifo write pointer */
                   rptr:8,      /* MCL fifo read pointer */
                  depth:8;      /* MCL fifo depth */
  } _rxInfo;			/* 40 Rx DMA ring buffer ptrs */
#define rxMclFIFOReadPtr _rxInfo.rptr
#define rxMclFIFOWritePtr _rxInfo.wptr
#define rxMclFIFODepth _rxInfo.depth
  long long rxIalias1;		/* 48 ...alias of above register */
  long long rxIalias2;		/* 50 ...alias of above register */
  long long intReq_lastTxVec;	/* 58 diag */
  long long phyDataInOut;	/* 60 PHY data in/out register */
  long long phyAddress;		/* 68 PHY address register */
  long long phyReadStart;	/* 70 PHY read initiate */
  long long backoff;		/* 78 Random number seed for backoff */
  long long imh1;		/* 80 diag */
  long long imd1;		/* 88 diag */
  long long imh2;		/* 90 diag */
  long long imd2;		/* 98 diag */
  long long physicalAddress;	/* a0 Ethernet physical address */
  long long secphysicalAddress;	/* a8 Secondary ethernet physical address */
  long long multicastFilter;	/* b0 Multicast logical address mask */
  long long txRingBase;		/* b8 Transmit ring base addr (phys) */
  long long txPkt1CmdHdr;	/* c0 diag */
  long long txPkt1CatBuf1;	/* c8 diag */
  long long txPkt1CatBuf2;	/* d0 diag */
  long long txPkt1CatBuf3;	/* d8 diag */
  long long txPkt2CmdHdr;	/* e0 diag */
  long long txPkt2CatBuf1;	/* e8 diag */
  long long txPkt2CatBuf2;	/* f0 diag */
  long long txPkt2CatBuf3;	/* f8 diag */
  long long mclRxFifo;		/* 100 */
} Mac110;


/* Definitions for Mac110.macControl */
#define MAC_RESET 	(0x0001) /* 1=> Reset MAC */
#define MAC_DUPLEX	(0x0002) /* 1=> Full duplex */
#define MAC_LOOPBACK    (0x0004) /* 1=> Internal loopback */
#define MAC_100MB       (0x0008) /* 1=> 100 Mb, 0=> 10 Mb */
#define MAC_MII		(0x0010) /* 1=> MII bus selected */
#define MAC_FILTER      (0x0060) /* Filter bits */
#define   MAC_PHYS      (0x0000)   /* Accept phy. packets only  */
#define   MAC_NORMAL    (0x0020)   /* Accept phy., bcast., mcast packets */
#define   MAC_ALL_MCAST (0x0040)   /* Accept phy., bcast., all mcast packets */
#define   MAC_PROMISCOUS (0x0060)  /* Accept all packets */
#define MAC_IPGT        (0x3F80) /* Inter packet gap ctr */
#define MAC_IPGR1     (0x1fc000) /* Inter packet gap ctr */
#define MAC_IPGR2    (0x7e00000) /* Inter packet gap ctr */

#define MAC_MASK        (0x006F) /* Mask of all of non-counter bits */

/* Definitions for Mac110.interruptStatus */
#define INTR_TX_THRSHD	(0x0001) /* 1=> TX threshold intr set */
#define INTR_TX_PKTREQ  (0x0002) /* 1=> TX packet intr request set */
#define INTR_TX_DMAFIFO (0x0004) /* 1=> TX dma fifo overflow intr set */
#define INTR_TX_MEMERR  (0x0008) /* 1=> Memory error during tx intr set*/
#define INTR_TX_ABORT   (0x0010) /* 1=> TX abort intr set */
#define INTR_RX_THRSHD  (0x0020) /* 1=> RX threshold intr set */
#define INTR_RX_MCLFIFO (0x0040) /* 1=> RX mcl fifo overflow intr set */
#define INTR_RX_DMAFIFO (0x0080) /* 1=> RX dma fifo overflow intr set */
#define INTR_RX_MCLRPTR (0xff00) /* Alias of rx mcl fifo read ptr */
#define INTR_TX_RPTR (0x1ff0000) /* Alias of tx ring buffer read ptr */
#define INTR_RX_SEQ (0x3e000000) /* Receive sequence number */

/* Definitions for Mac110.dmaCtrl */
#define DMA_TX_INTENAB  (0x0001) /* 1=> TX empty interrupt enable */
#define DMA_TX_DMAENAB  (0x0002) /* 1=> TX DMA enable */
#define DMA_TX_RINGMSK  (0x000c) /* TX ring size mask */
#define   DMA_TX_8K       (0x0000) /* 8KB ring */
#define   DMA_TX_16K      (0x0004) /* 16KB ring */
#define   DMA_TX_32K      (0x0008) /* 32KB ring */
#define   DMA_TX_64K      (0x000c) /* 64KB ring */
#define DMA_RX_THRSHD   (0x01f0) /* Receive interrupt threshold */
#define DMA_RX_INTENAB  (0x0200) /* 1=> RX Threshold intr enable */
#define DMA_RX_RUNTENAB (0x0400) /* 1=> RX receive runt packets enable */
#define DMA_RX_GATHENAB (0x0800) /* 1=> RX gather timer enable */
#define DMA_RX_OFFSET   (0x7000) /* RX DMA starting offset */
#define DMA_RX_OFFSET_SHF (12)	 /* Shift amount of offset  */
#define DMA_RX_DMAENAB  (0x8000) /* 1=> RX DMA enable */


/*
 * MAC110 Transmit fifo entry
 *
 * Each entry is 128 bytes:
 *
 *   +-----------------+
 *   | tx command      | 
 *   +-----------------+
 *   | concat. buf. ptr|  -> [optional] buffer for longer packets
 *   +-----------------+
 *   | concat. buf. ptr|  -> [optional] buffer for longer packets
 *   +-----------------+
 *   | concat. buf. ptr|  -> [optional] buffer for longer packets
 *   +-----------------+
 *          data
 *            :        
 *   |                 |
 *   +-----------------+
 *
 * Following transmission, the tx-command is overwritten by transmit
 * status data.
 */
typedef volatile union{
  struct{
    union{
      struct{
	unsigned long r1;	/* TX control data */
	unsigned long r2:  4,
	  concatPtr2	:  1,	/* Concatenated buffer ptr is valid */
	  concatPtr1	:  1,
	  concatPtr0	:  1,
	  genTxIntr	:  1,	/* Generate TX intr when pkt sent */
	  termOnAbrt	:  1,	/* Terminate TX on abort */
	  byteOffset	:  7,	/* Byte offset of start of data */
	  requestLength	: 16;	/* Number of data bytes minus 1 */
      }cmd;
      struct{
	unsigned long 		/* TX status data */
	  finished	:  1,	/* Transmit finished */
	  r1	 	: 31,
	  r2		:  2,
	  lateCollisions:  1,	/* Transmit dropped due to late collision */
	  excessDeferral:  1,	/* Transmit canceled due to excess deferrals */
	excessCollisions:  1,	/* Transmit dropped due to excess collisions */
	  underrun	:  1,	/* Transmit aborted due to underrun */
	  excessLength	:  1,	/* Transmit aborted due to excess length */
	  deferred	:  1,	/* Packet deferred at least once */
	  broadcast	:  1,	/* Broadcast packet */
	  multicast	:  1,	/* Multicast packet */
	  CRC		:  1,	/* CRC error seen on at least one tx attempt */
	  lateCollision	:  1,	/* At least one late collision seen */
	  collisions	:  4,	/* Collision retry count */
	  transmitLength	: 16;	/* Number of data bytes sent */
      }sts;
      unsigned long long ll;	/* Simple access to first long long */
    } hdr;
  }x;
  unsigned long long ptr[4];	/* Concatenated buffer ptr */
  char fifoBuf[128];
} TXfifo;



/*
 * MAC110 receive status
 */
typedef struct _rxBuf{
  unsigned long long
    finished        	: 1,
    _mbz0           	: 15,
    ipChecksum      	: 16,
    seqNumber       	: 5,
    physaddrMatch   	: 1,
    multicastMatch  	: 1,
    carrierEventSeen 	: 1,
    receivedBadPacket   : 1,
    longEventSeen   	: 1,
    invalidPreamble 	: 1,
    broadcastPkt    	: 1,
    multicastPkt    	: 1,
    CRCerror        	: 1,
    dribbleNibble   	: 1,
    codeViolation   	: 1,
    receiveLength   	: 16;	/* # of bytes received */
} RXbuf;


/*
 * Define the FIFO size
 *
 * The TX FIFO can be 64, 128, 256, or 512 entries.
 * The RX FIFO is 32 entries.
 */
#define TXFIFOSMALL   64
#define TXFIFOMEDIUM 128
#define TXFIFOLARGE  512
#define RXFIFOSIZE    32


#endif
