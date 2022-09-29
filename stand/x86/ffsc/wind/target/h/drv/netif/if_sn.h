/* if_sn.h - structures and defines for the DP83932B SONIC device */

/* Copyright 1991-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01h,18oct93,cd   added extra field to TX_DESC to hold associated mbuf,
		 defined EXBUS, added timer support.
01g,22sep92,rrr  added support for c++
01f,04jun92,ajm  defined appropriate regs volatile
01e,26may92,rrr  the tree shuffle
01d,03feb92,rfs  minor changes to TX_DESC, moved tunables out to driver
01c,08nov91,rfs  adapted from Ironics stuff
01b,29jul91,kjl  cleanup in preparation for release
01a,25mar91,kjl  adapted from if_ln.h 01j,05oct90,shl
*/

#ifndef __INCif_snh
#define __INCif_snh

#ifdef __cplusplus
extern "C" {
#endif


/* DATA STRUCTURES */

typedef struct rra_desc                    /* Receive Resource Descriptor */
    {
    unsigned long buff_ptr0;
    unsigned long buff_ptr1;
    unsigned long buff_wc0;
    unsigned long buff_wc1;
    } RRA_DESC;
#define RRA_DESC_SIZ  sizeof(RRA_DESC)

/* define NUM_RRA_DESC in the driver source file */
#define RRA_SIZE    (NUM_RRA_DESC * RRA_DESC_SIZ)      /* size of area */

typedef struct Rx_desc                           /* Receive Descriptor */
    {
    unsigned long status;
    unsigned long byte_count;
    unsigned long pkt_ptr0;
    unsigned long pkt_ptr1;
    unsigned long seq_no;
    struct Rx_desc *link;
    unsigned long in_use;
    } RX_DESC;
#define RX_DESC_SIZ  sizeof(RX_DESC)

/* define NUM_RX_DESC in the driver source file */
#define RDA_SIZE    (NUM_RX_DESC * RX_DESC_SIZ)             /* Total Bytes */

/* values for in_use field */
#define IN_USE      (0)
#define NOT_IN_USE  (1)

/* end-of-list marker for link field */
#define RX_EOL      (RX_DESC *)(1)

#define CAM_COUNT   (16)
#define CAM_SIZE    (((CAM_COUNT * 4) * 4) + 4)

typedef struct Tx_desc                            /* Transmit Descriptor */
    {
    unsigned long status;
    unsigned long config;
    unsigned long pkt_size;
    unsigned long frag_count;
#ifdef MAX_TX_FRAGS
    struct frag_desc
        {
        unsigned long frag_ptr0;
        unsigned long frag_ptr1;
        unsigned long frag_size;
        } frag [MAX_TX_FRAGS]; /* define MAX_TX_FRAGS in driver source file */
#endif
    struct Tx_desc *pLink;						/* OUR real link ptr */
    unsigned long flag;							/* OUR state flags */
    struct mbuf *pMbuf;		/* OUR mbuf associated with descriptor  */
    } TX_DESC;
#define TX_DESC_SIZ  sizeof(TX_DESC)

/* define NUM_TX_DESC in the driver source file */
#define TDA_SIZE    (NUM_TX_DESC * TX_DESC_SIZ)

/* end-of-list marker for link field */
#define TX_EOL      (1)

/* SONIC registers, defined as data structure */

typedef struct sonic
    {
    volatile unsigned long cr;
    volatile unsigned long dcr;
    volatile unsigned long rcr;
    volatile unsigned long tcr;
    volatile unsigned long imr;
    volatile unsigned long isr;
    volatile unsigned long utda;
    volatile unsigned long ctda;
    volatile unsigned long fill_20;
    volatile unsigned long fill_24;
    volatile unsigned long fill_28;
    volatile unsigned long fill_2c;
    volatile unsigned long fill_30;
    volatile unsigned long urda;
    volatile unsigned long crda;
    volatile unsigned long fill_3c;
    volatile unsigned long fill_40;
    volatile unsigned long fill_44;
    volatile unsigned long fill_48;
    volatile unsigned long eobc;
    volatile unsigned long urra;
    volatile unsigned long rsa;
    volatile unsigned long rea;
    volatile unsigned long rrp;
    volatile unsigned long rwp;
    volatile unsigned long fill_64;
    volatile unsigned long fill_68;
    volatile unsigned long fill_6c;
    volatile unsigned long fill_70;
    volatile unsigned long fill_74;
    volatile unsigned long fill_78;
    volatile unsigned long fill_7c;
    volatile unsigned long fill_80;
    volatile unsigned long cep;
    volatile unsigned long cap2;
    volatile unsigned long cap1;
    volatile unsigned long cap0;
    volatile unsigned long ce;
    volatile unsigned long cdp;
    volatile unsigned long cdc;
    volatile unsigned long sr;
    volatile unsigned long wt0;
    volatile unsigned long wt1;
    volatile unsigned long rsc;
    volatile unsigned long fill_b0;
    volatile unsigned long fill_b4;
    volatile unsigned long fill_b8;
    volatile unsigned long fill_bc;
    volatile unsigned long fill_c0;
    volatile unsigned long fill_c4;
    volatile unsigned long fill_c8;
    volatile unsigned long fill_cc;
    volatile unsigned long fill_d0;
    volatile unsigned long fill_d4;
    volatile unsigned long fill_d8;
    volatile unsigned long fill_dc;
    volatile unsigned long fill_e0;
    volatile unsigned long fill_e4;
    volatile unsigned long fill_e8;
    volatile unsigned long fill_ec;
    volatile unsigned long fill_f0;
    volatile unsigned long fill_f4;
    volatile unsigned long fill_f8;
    volatile unsigned long dcr2;
    } SONIC;
#define SONIC_SIZ  sizeof(SONIC)


/* SONIC registers, defined as direct ptrs.  The constant SONIC_BASE_ADDR
 * should be defined elsewhere; in board specific header file.
 * These defines are NOT used in the VxWorks SONIC driver, but are left
 * here for reference.
 */

#if 0
#define SONIC_CR   (volatile unsigned long *) (SONIC_BASE_ADDR + 0x00)
#define SONIC_DCR  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x04)
#define SONIC_RCR  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x08)
#define SONIC_TCR  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x0c)
#define SONIC_IMR  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x10)
#define SONIC_ISR  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x14)
#define SONIC_UTDA (volatile unsigned long *) (SONIC_BASE_ADDR + 0x18)
#define SONIC_CTDA (volatile unsigned long *) (SONIC_BASE_ADDR + 0x1c)

#define SONIC_URDA (volatile unsigned long *) (SONIC_BASE_ADDR + 0x34)
#define SONIC_CRDA (volatile unsigned long *) (SONIC_BASE_ADDR + 0x38)
#define SONIC_EOBC (volatile unsigned long *) (SONIC_BASE_ADDR + 0x4c)
#define SONIC_URRA (volatile unsigned long *) (SONIC_BASE_ADDR + 0x50)
#define SONIC_RSA  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x54)
#define SONIC_REA  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x58)
#define SONIC_RRP  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x5c)
#define SONIC_RWP  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x60)
#define SONIC_CEP  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x84)
#define SONIC_CAP2 (volatile unsigned long *) (SONIC_BASE_ADDR + 0x88)
#define SONIC_CAP1 (volatile unsigned long *) (SONIC_BASE_ADDR + 0x8c)
#define SONIC_CAP0 (volatile unsigned long *) (SONIC_BASE_ADDR + 0x90)
#define SONIC_CE   (volatile unsigned long *) (SONIC_BASE_ADDR + 0x94)
#define SONIC_CDP  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x98)
#define SONIC_CDC  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x9c)
#define SONIC_RSC  (volatile unsigned long *) (SONIC_BASE_ADDR + 0xac)
#define SONIC_SR   (volatile unsigned long *) (SONIC_BASE_ADDR + 0xa0)
#define SONIC_WT0  (volatile unsigned long *) (SONIC_BASE_ADDR + 0xa4)
#define SONIC_WT1  (volatile unsigned long *) (SONIC_BASE_ADDR + 0xa8)
#define SONIC_CRCT (volatile unsigned long *) (SONIC_BASE_ADDR + 0xb0)
#define SONIC_FAET (volatile unsigned long *) (SONIC_BASE_ADDR + 0xb4)
#define SONIC_MPT  (volatile unsigned long *) (SONIC_BASE_ADDR + 0xb8)

#define SONIC_CRBA0  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x3c)
#define SONIC_CRBA1  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x40)
#define SONIC_RBWC0  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x44)
#define SONIC_RBWC1  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x48)

#define SONIC_TPS  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x20)
#define SONIC_TFC  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x24)
#define SONIC_TSA0 (volatile unsigned long *) (SONIC_BASE_ADDR + 0x28)
#define SONIC_TSA1 (volatile unsigned long *) (SONIC_BASE_ADDR + 0x2c)
#define SONIC_TFS  (volatile unsigned long *) (SONIC_BASE_ADDR + 0x30)
#define SONIC_TTDA (volatile unsigned long *) (SONIC_BASE_ADDR + 0x80)
#endif

/* miscellaneous stuff */
#define LMASK  (0xffff0000)
#define UMASK  (0x0000ffff)

/* Command Register Commands */
#define LCAM    (0x0200)      /* Load CAM */
#define RRRA    (0x0100)      /* Read RRA */
#define RST     (0x0080)      /* Software Reset */
#define RST_OFF (0x0000)      /* Software Reset Off */
#define ST      (0x0020)      /* Start Timer */
#define STP     (0x0010)      /* Stop Timer */
#define RXEN    (0x0008)      /* Receiver Enable */
#define RXDIS   (0x0004)      /* Receiver Disable */
#define TXP     (0x0002)      /* Transmit Packet */
#define HTX     (0x0001)      /* Halt Transmission */

/* Data Configuration Register */
#define EXBUS	(0x8000)      /* Extended Bus Mode (BVF) */
#define LBR     (0x2000)      /* Latched Mode */
#define PO1     (0x1000)      /* Programmable Output 1 */
#define PO0     (0x0800)      /* Programmable Output 0 */
#define STERM   (0x0400)      /* Synchronous Transmission */
#define USR1    (0x0200)      /* User Definable Pin 1 */
#define USR0    (0x0100)      /* User Definable Pin 0 */
#define WC1     (0x0080)      /* Wait State Control 1 */
#define WC0     (0x0040)      /* Wait State Control 0 */
#define WAIT0   (0)           /* 0 Wait state */
#define WAIT1   (WC0)         /* 1 Wait state */
#define WAIT2   (WC1)         /* 2 Wait state */
#define WAIT3   (WC1 | WC0)   /* 3 Wait state */
#define DW      (0x0020)      /* Data Width Control */
#define DW_32   (0x0020)      /* Data Width 32 bits */
#define DW_16   (0x0000)      /* Data Width 16 bits */
#define BMS     (0x0010)      /* Block Mode Select for DMA */
#define RFT1    (0x0008)      /* Receive FIFO threshold 1 */
#define RFT0    (0x0004)      /* Receive FIFO threshold 0 */
#define TFT1    (0x0002)      /* Transmit FIFO Threshold 1 */
#define TFT0    (0x0001)      /* Transmit FIFO Threshold 0 */

/* Receive Control Register */
#define ERR      (0x8000)     /* Accept packets with errors */
#define RNT      (0x4000)     /* Accept Runt packets */
#define BRD      (0x2000)     /* Accept broadcast packets */
#define PRO      (0x1000)     /* Physical Promiscuous packets */
#define AMC      (0x0800)     /* Accept all multicast packets */
#define LB1      (0x0400)     /* Loopback bit 1 */
#define LB0      (0x0200)     /* Loopback bit 0 */
#define NO_LB    (0x0000)     /* No Loopback */
#define MAC_LB   (0x0200)     /* MAC loopback */
#define ENDEC_LB (0x0400)     /* ENDEC loopback */
#define XCVR_LB  (0x0600)     /* Tranceiver loopback */
#define MC       (0x0100)     /* Multicast packet received */
#define BC       (0x0080)     /* Broadcast packet received */
#define LPKT     (0x0040)     /* Last packet in RBA */
#define CRS      (0x0020)     /* Carrier Sense Activity */
#define COL      (0x0010)     /* Collision activity */
#define CRCR     (0x0008)     /* CRC Error */
#define FAER     (0x0004)     /* Frame alignment error */
#define LBK      (0x0002)     /* Loopback packet revceived */
#define PRX      (0x0001)     /* Packet received OK */

/* Transmit Control Register */
#define PINT  (0x8000)        /* Programmable Interrupt */
#define POWC  (0x4000)        /* Programmed out of window collision timer */
#define CRCI  (0x2000)        /* CRC Inhibit */
#define EXDIS (0x1000)        /* Disable Excessive Deferal Timer */
#define EXD   (0x0400)        /* Excessive Deferal */
#define DEF   (0x0200)        /* Defered transmission */
#define NCRS  (0x0100)        /* No CRS */
#define CRSL  (0x0080)        /* CRS Lost */
#define EXC   (0x0040)        /* Excessive Collisions */
#define OWC   (0x0020)        /* Out of window collision */
#define PMB   (0x0008)        /* Packet monitored bad */
#define FU    (0x0004)        /* FIFO underrun */
#define BCM   (0x0002)        /* Byte count mismatch */
#define PTX   (0x0001)        /* Packet transmitted OK */

/* Interrupt Mask Register */
#define BREN   (0x4000)       /* Bus Retry Occured Enable */
#define HBLEN  (0x2000)       /* Heartbeat lost Enable */
#define LCDEN  (0x1000)       /* Load Cam Done Interrupt Enable */
#define PINTEN (0x0800)       /* Programmable Interrupt Enable */
#define PRXEN  (0x0400)       /* Packet Received Enable */
#define PTXEN  (0x0200)       /* Packet Transmitted OK Enable */
#define TXEREN (0x0100)       /* Transmit Error Enable */
#define TCEN   (0x0080)       /* Timer Complete Enable */
#define RDEEN  (0x0040)       /* Receive Discriptors Enable */
#define RBEEN  (0x0020)       /* Receive Buffers Exhausted Enable */
#define RBAEEN (0x0010)       /* Receive Buffer Area Exceeded Enable */
#define CRCEN  (0x0008)       /* CRC Tally counter warning Enable */
#define FAEEN  (0x0004)       /* FAE Tally counter warning Enable */
#define MPEN   (0x0002)       /* MP Tally counter warning Enable */
#define RFOEN  (0x0001)       /* Receive FIFO Overrun Enable */

/* Interrupt Status Register */
#define CLEAR_ISR (0xffff)    /* Clear all status */
#define BR        (0x4000)    /* Bus Retry Occured  */
#define HBL       (0x2000)    /* Heartbeat lost  */
#define LCD       (0x1000)    /* Load Cam Done Interrupt  */
#define PINTS     (0x0800)    /* Programmable Interrupt Status  */
#define PKTRX     (0x0400)    /* Packet Received  */
#define TXDN      (0x0200)    /* Transmition Done */
#define TXER      (0x0100)    /* Transmit Error  */
#define TC        (0x0080)    /* Timer Complete  */
#define RDE       (0x0040)    /* Receive Discriptors  */
#define RBE       (0x0020)    /* Receive Buffers Exhausted  */
#define RBAE      (0x0010)    /* Receive Buffer Area Exceeded  */
#define CRC       (0x0008)    /* CRC Tally counter warning  */
#define FAE       (0x0004)    /* FAE Tally counter warning  */
#define MP        (0x0002)    /* MP Tally counter warning  */
#define RFO       (0x0001)    /* Receive FIFO Overrun  */

/* End of Buffer Count Register */
#define MAX_PACKET_SIZE (0x02f8)  /* 760 words */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_snh */
