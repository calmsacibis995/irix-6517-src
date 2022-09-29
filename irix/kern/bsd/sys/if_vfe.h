/*
 * Include file for if_vfe VME Fast Ethernet Driver for IP 19/21/25
 *
 * Copyright 1996-7, Silicon Graphics, Inc.  All Rights Reserved.
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
/* tlan ioctls */
#define TLIOC_GETPHYREG		(('e' << 8) | 1)
#define TLIOC_VMEDMASET		(('e' << 8) | 2)
#define TLIOC_UNIVERSEREAD	(('e' << 8) | 3)
#define TLIOC_UNIVERSEWRITE	(('e' << 8) | 4)
#define TLIOC_PCIREAD		(('e' << 8) | 5)
#define TLIOC_PCIWRITE		(('e' << 8) | 6)
#define TLIOC_SETDMABUF		(('e' << 8) | 7)
#define TLIOC_GETDMABUF		(('e' << 8) | 8)
#define TLIOC_QUIT		(('e' << 8) | 9)

#define MAX_COUNT		32

/* structure for ioctl read and write */
typedef struct tlioc_arg {
	int	count;
	__uint32_t *vme_addr;
	uint_t	val[MAX_COUNT];
} tlioc_arg_t;

/* TLAN Host command Register - See TLAN design spec. Page 28-31 */

#define  CMD_GO		0x80000000		/* Network channel go command */
#define  CMD_STP	0x40000000	 	/* Network channel stop command */
#define  CMD_ACK	0x00000020		/* Interrupt Acknowledge */	 
#define  CMD_CH0_SEL	0x00000000		/* Channel 0 select */
#define  CMD_CH1_SEL	0x00200000		/* Channel 1 select */
#define  CMD_EOC	0x00100000		/* End of Channel Select */	
#define  CMD_RTX	0x00080000		/* Rx/Tx Select */
#define  CMD_NES	0x00040000		/* Not Error/Statistics */
#define  CMD_ADPRST	0x00008000		/* Adapter Reset command */
#define  CMD_LDTMR	0x00004000		/* Load Interrupt Timer command */
#define  CMD_LDTHR	0x00002000		/* Load Tx Interrupt Threshold */
#define  CMD_REQINT	0x00001000		/* Request host interrupt command */
#define  CMD_INTOFF	0x00000800		/* PCI interrupt OFF */
#define  CMD_INTON	0x00000400		/* PCI interrupt ON */

/* TLAN Host Interrupt Register - See TLAN design spec. Page 33 */

#define  INT_TXEOF	0x0400		/* Tx Eof interrupt */
#define  INT_STATOV	0x0800		/* Statistics overflow interrupt */
#define  INT_RXEOF	0x0c00		/* Rx Eof interrupt */
#define  INT_DUMMY	0x1000		/* Dummy interrupt */
#define  INT_TXEOC	0x1400		/* Tx Eoc interrupt */
#define  INT_ADPCHK	0x1800		/* Adapter check interrupt */
#define  INT_RXEOC	0x1c00		/* Rx Eoc interrupt */

/*----------------------------------------------------------------------
//  TLAN Related                                                        
//----------------------------------------------------------------------
*/
#define TI_VENDORID             0x104C  /*pci vendor id for TI */
#define TLAN_DEVICEID           0x0500  /*pci tlan device ID */

#define F_LASTFRAG              0x80000000 /*last fragment indicator */

/*Receive CSTAT bits */
#define FRAME_COMPLETE          0x0040
#define RX_ERROR          	0x0400

/* Transmit CSTAT bits          */
#define TXCSTAT_REQ             0x0030
#define RXCSTAT_REQ             0x0030

/*----------------------------------------------------------------------
//  TLAN Internal register definitions
//----------------------------------------------------------------------
*/
#define DIO_BYTE		0x01
#define DIO_WORD		0x02
#define ADR_INC 		0x8000	    /* Address Increment mode */

#define NET_CMD                 0x00        /*Net Command */
#define     NRESET                  0x80
#define     NWRAP                   0x40
#define     CSF                     0x20
#define     CAF                     0x10
#define     NOBRX                   0x08
#define     NC_DUPLEX               0x04
#define     TRFRAM                  0x02
#define     TXPACE                  0x01
#define NET_SIO                 0x01        /*Net Serial I/O */
#define     MDATA                   0x01
#define     MTXEN                   0x02
#define     MCLK                    0x04
#define     NMRST                   0x08
#define     EDATA                   0x10
#define     ETXEN                   0x20
#define     ECLOK                   0x40
#define     MINTEN                  0x80
#define NET_STS                 0x02        /*Net Status */
#define     MIRQ                    0x80    /*Mii interrupt request */
#define     HBEAT                   0x40    /*Heart Beat Error    */ 
#define     TXSTOP                  0x20
#define     RXSTOP                  0x10

#define NET_MASK                0x03        /*Net Error Mask */
#define     MIIMASK                 0x80
#define     HEARTBEATMASK           0x40
#define     TRANSMITSTOPMASK        0x20
#define     RECEIVESTOPMASK         0x10
#define NET_CONFIG                0x04	      /*Net Config */
#define     RCLKTEST                0x8000
#define     TCLKTEST                0x4000
#define     BITRATE                 0x2000
#define     RXCRC                   0x1000
#define     PEF                     0x0800
#define     ONEFRAG                 0x0400
#define     ONECHN                  0x0200
#define     MTEST                   0x0100
#define     PHY_EN                  0x0080
#define     MAC_SELECT_MASK         0x007F
#define     MACP_CSMACD             0x0000         /*10/100mbit ethernet csma/cd */
#define     MACP_100VG              0x0001         /*100mbit channel priority */
#define     MACP_100CSTAT           0x0002         /*100mbit cstat priority */
#define     MACP_EXT                0x0003
#define NET_DEFREV              0x0C        /*default revision reg */
#define NET_AREG0               0x10        /*General address */
#define NET_AREG1               0x16        /*General address */
#define NET_AREG2               0x1C        /*General address */
#define NET_AREG3               0x22        /*General address */
#define NET_HASH1               0x28        /*Hash address */
#define NET_HASH2               0x2C        /*Hash address */
#define NET_STATISTICS          0x30        /*Statistics registers */
#define NET_SGOODTX             0x30
#define NET_STXUNDERRUN         0x33
#define NET_SGOODRX             0x34
#define NET_SRXOVERRUN          0x37
#define NET_SDEFERTX            0x38
#define NET_SCRCERR             0x3a
#define NET_SCODEERR            0x3b
#define NET_SMULTICOLLTX        0x3c
#define NET_SINGLECOLLTX        0x3e
#define NET_SEXCESSCOLL         0x40
#define NET_SLATECOLL           0x41
#define NET_SCARRIERLOST        0x42
#define ACOMMIT                 0x43        /*adapter PCI commit size */
#define     BLP_UNDEFINED           0x01
#define     BLP_AUI                 0x02
#define     BLP_LOOPBACK            0x04
#define     BLP_HDUPLEX             0x08
#define     TRANSWHOLEFRAME         0x70
#define LED                     0x44        
#define BSIZE                   0x45        /*Burst size (tx=4msbs,rx=4lsbs) */
#define MAXRX                   0X46        /*Maximum Rx frame size */

#define RAM_SIZE        0x400

/*----------------------------------------------------------------------
//  MII Registers
//----------------------------------------------------------------------
*/

#define GEN_CTL                 0x00        /* Generic Control Register */
#define     PHYRESET    0x8000
#define     LOOPBK      0x4000
#define	    SPEED100MB	0x2000	
#define     AUTOENB     0x1000
#define     PDOWN       0x0800
#define     ISOLATE     0x0400
#define     AUTORSRT    0x0200
#define     DUPLEX      0x0100
#define     COLTEST     0x0080
#define GEN_STS                 0x01        /* Generic Status Register */
#define     AUTOCMPLT           0x0020
#define     RFLT                0x0010        
#define     LINK                0x0004
#define     AUTO_NEG    0x0008	     
#define     EXTCAP      0x0001	     
#define     JABBER	0x0002

#define     TFOUR       0x8000	     
#define     HTX_FD      0x4000	     
#define     HTX_HD      0x2000	     
#define     TENT_FD     0x1000	     
#define     TENT_HD     0x0800	     

/* Register 4 - Auto-Negotiation Advertisement Register (RW) */
#define R4_NP               0x8000          /* next page bit */
#define R4_ACK              0x4000          /* ack link partner data */
#define R4_REMFAULT         0x2000          /* signal rem. fault to partner  */
#define R4_T4               0x0200          /* 100BASE-T4 cap. */
#define R4_TXFD             0x0100          /* 100BASE-TX full duplex */
#define R4_TX               0x0080          /* 100BASE-TX cap. */
#define R4_10FD             0x0040          /* 10BASE-T full duplex */
#define R4_10               0x0020          /* 10BASE-T cap. */
#define R4_SELECTOR         0x001f          /* encoded selector cap. */

/* Register 5 - Auto-Negotiation Link Partner Ability Register (RO) */
#define R5_NP               0x8000          /* next page capable */
#define R5_ACK              0x4000          /* ack link parter data */
#define R5_REMFAULT         0x2000          /* rem. fault received */
#define R5_T4               0x0200          /* 100BASE-T4 cap. */
#define R5_TXFD             0x0100          /* 100BASE-TX full duplex */
#define R5_TX               0x0080          /* 100BASE-TX cap. */
#define R5_10FD             0x0040          /* 10BASE-T full duplex */
#define R5_10               0x0020          /* 10BASE-T cap. */
#define R5_SELECTOR         0x001f          /* encoded selector cap. */
#define R5_8023             0x1             /* IEEE 802.3 link tech */
#define R5_8029A            0x2             /* IEEE 802.9a link tech */

/* mii phy register 6 */
#define R6_MLF              0x0010          /* mult. link fault */
#define R6_LPNPABLE         0x0008          /* partner nextpage cap. */
#define R6_NPABLE           0x0004          /* nextpage cap. */
#define R6_PAGERECVD        0x0002          /* code word received. */
#define R6_LPNWABLE         0x0001          /* partner is NWay cap. */

#define PHYTYPE_DP83840         1
#define ICS1890_R17_SPEED_100   0x8000  /* 100 Mbits/s */
#define ICS1890_R17_FULL_DUPLEX 0x4000  /* full duplex */
#define DP83840_R25_SPEED_10    0x0040  /* 10 Mbits/s */


#define GEN_ID_HI               0x02        /* Generic identifier (high) */
#define GEN_ID_LO               0x03        /* Generic identifier (lo) */
#define AN_ADVERTISE            0x04        /* Auto-neg. advertisement */
#define     TA_TFOUR    0x0200	    /* from IEEE spec xxxxx */
#define     TA_HTX_FD   0x0100	    /* from IEEE spec xxxxx */
#define     TA_HTX_HD   0x0080	    /* from IEEE spec xxxxx */
#define     TA_TENT_FD  0x0040	    /* from IEEE spec xxxxx and Ours */
#define     TA_TENT_HD  0x0020        
#define     FDUPLEX             0x40
#define     HDUPLEX             0x20
#define AN_LPABILITY            0x05        /* Auto-neg. Link-Partner ability */
#define AN_EXPANSION            0X06        /* Auto-neg. expansion */
#define TLPHY_ID                0x10        /* TLAN PHY identifier */
#define TLPHY_CTL               0x11        /* TLAN PHY control register */
#define     IGLINK      0x8000  
#define     INTEN       0x0002
#define     TINT        0x0001
#define TLPHY_STS               0x12        /* TLAN PHY status register */
#define     MINT                0x8000
#define     PHOK                0x4000
#define     POLOK               0x2000
#define     TPENERGY            0x1000

/*----------------------------------------------------------------------
//  100VG MII Registers
//----------------------------------------------------------------------
*/
#define VGPHY_ID                0x10        /* TLAN PHY identifier */
#define VGPHY_CTL               0x11        /* TLAN PHY control register */
#define     TRIDLE                  0x0010
#define     TRFAIL                  0x0020
#define VGPHY_STS               0x12        /* TLAN PHY status register */
#define     RETRAIN                 0x0040
#define     LSTATE                  0x0030
#define     TRFRTO                  0x0008
#define     RTRIDL                  0x0004

/*----------------------------------------------------------------------
    Macros
  ---------------------------------------------------------------------*/

#define ADPTCHK_TYPE(x) ( x & 0x00100000 ) ? "List Op" : "Data Transfer" 
#define ADPTCHK_CHOP(x) ( x & 0x00080000 ) ? "RX Ch." : "TX Ch."
#define ADPTCHK_RWOP(x) ( x & 0x00040000 ) ? "PCI RD" : "PCI WR"

#define TLAN_MII_CLR(pmc,bits) { \
	diodata = tl_dio_readB(pmc,NET_SIO); diodata &= ~(bits); \
	tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE); }

#define TLAN_MII_SET(pmc,bits) { \
	diodata = tl_dio_readB(pmc,NET_SIO); diodata |=  (bits); \
        tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE); }

#define TLAN_MII_CLCK(pmc) { \
	TLAN_MII_CLR(pmc,MCLK); TLAN_MII_SET(pmc,MCLK); }

#define TLAN_EEPROM_CLR(pmc,bits) { \
	diodata = tl_dio_readB(pmc,NET_SIO); diodata &= ~(bits); \
	tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE); }

#define TLAN_EEPROM_SET(pmc,bits) { \
	diodata = tl_dio_readB(pmc,NET_SIO); diodata |=  (bits); \
        tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE); }

#define TLAN_EEPROM_CLCK(pmc) { \
	TLAN_EEPROM_SET(pmc,ECLOK); TLAN_EEPROM_CLR(pmc,ECLOK); }

#define TLAN_EEPROM_START_SEQ(pmc)  { \
	TLAN_EEPROM_SET(pmc, ETXEN);  \
	TLAN_EEPROM_SET(pmc, ECLOK);  \
	TLAN_EEPROM_SET(pmc, EDATA);  \
	TLAN_EEPROM_SET(pmc, ETXEN);  \
	TLAN_EEPROM_CLR(pmc, EDATA);  \
	TLAN_EEPROM_CLR(pmc, ECLOK); }

#define TLAN_EEPROM_STOP_SEQ(pmc)  { \
	TLAN_EEPROM_CLR(pmc, EDATA); \
	TLAN_EEPROM_SET(pmc, ECLOK);  \
	TLAN_EEPROM_SET(pmc, EDATA); \
	TLAN_EEPROM_CLR(pmc, ETXEN); }

#define TLAN_SET_MACADDR(pmc,reg,addr) { \
	tl_dio_write(pmc,NET_AREG0+(6 * reg)  ,*(((u_char *)(addr))+0),DIO_BYTE); \
	tl_dio_write(pmc,NET_AREG0+(6 * reg)+1,*(((u_char *)(addr))+1),DIO_BYTE); \
	tl_dio_write(pmc,NET_AREG0+(6 * reg)+2,*(((u_char *)(addr))+2),DIO_BYTE); \
	tl_dio_write(pmc,NET_AREG0+(6 * reg)+3,*(((u_char *)(addr))+3),DIO_BYTE); \
	tl_dio_write(pmc,NET_AREG0+(6 * reg)+4,*(((u_char *)(addr))+4),DIO_BYTE); \
	tl_dio_write(pmc,NET_AREG0+(6 * reg)+5,*(((u_char *)(addr))+5),DIO_BYTE); }
	


/* tupedefs for external addresses, don't confuse them with internal
 * 64 bit addresses
 */
typedef __uint32_t pciaddr_t;
typedef __uint32_t vmebusaddr_t;

/*----------------------------------------------------------------------
 * TLAN PCI Configuration Registers
 *---------------------------------------------------------------------*/
typedef struct TLAN_CONFIG_REG
{
    ushort	vend_id;		/* Vendor ID Register. Required */
    ushort	dev_id;			/* Device ID Register. Required */
    ushort	cmd;			/* Command Register. Required */
#define IOENABLE	0x0001
#define MEMENABLE	0x0002
#define MSTENABLE	0x0004
#define SPECRECOG	0x0008
#define MEMINVENABLE	0x0010
#define VGAENABLE	0x0020
#define PARERRRSP	0x0040
#define WAITCYLEN	0x0080
#define SYSERREN	0x0100
#define FBBENABLE	0x0200
    ushort	status;			/* PCI status Register */
#define MHZ66ABLE	0x0020
#define UDFABLE		0x0040
#define FBBABLE		0x0080
#define DPARITY		0x0100
#define DEVSEL_F	0x0000
#define DEVSEL_M	0x0200
#define DEVSEL_S	0x0400
#define SIGTRGABT	0x0800
#define RCVTRGABT	0x1000
#define RCVMSTABT	0x2000
#define SIGSYSERR	0x4000
#define PARITYERR	0x8000
    u_char	rev;			/* Revision Register */
    u_char	Prog_if;		/* Program interface Register */
    u_char	subclass;		/* Sub-Class Register */
    u_char	baseclass;		/* Base Class Register */
    u_char	chahesize;		/* Cache line size */
    u_char	latencytmr;		/* PCI bus latency timer */
    ushort	reserve1;		/* Header type/BIST register-not used*/
    uint	baseaddr_io;		/* I/O base address */
    uint	baseaddr_mem;		/* Memory base address */
    uint	reserve2[6];		/* base addresses, not used */
    uint	bios_romaddr;		/* BIOS ROM Base Address */
    u_char	nvram;			/* PCI NVRAM extern EEPROM access */
    u_char	reserve3[7];		/* Not used, reserved */
    u_char	int_line;		/* INterrupt line register */
    u_char	int_pin;		/* Interrupt pin REgister */
    u_char	min_gnt;		/* Min_Gnt Register */
    u_char	max_lat;		/* Max_Lat Register */
    u_char	reset_ctrl;		/* PCI reset control Register */
    u_char	reserve4[15];		/* not used, reserved. */
} tlan_conf_t;


/*----------------------------------------------------------------------
 * TLAN Adapter Host Registers structure 
 *---------------------------------------------------------------------*/

typedef struct HOST_REG
{
     uint        host_cmd;      /* Host command register */
     uint        chan_parm;     /* channel parameter register */
     ushort      dio_addr;      /* TLAN internal DIO register addr. */
     ushort      host_intr;     /* Host interrupt register */
     uint        dio_data;      /* DIO data register */
     uint        padd[8*1024 - 4];/* This struct MUST be 0x8000 aligned */
} host_reg_t;



/*----------------------------------------------------------------------
 * TLAN Transmit/Receive list Buffer Descriptor structure 
 *---------------------------------------------------------------------*/
typedef struct TLANLIST_RXBD
{
     pciaddr_t	 bd_next;    /* Forward pointer of BD,  PCI ADDRESS */
     ushort      bd_cstat;      /* CSTAT field of BD */
     short       bd_frame_sz;   /* size of frame in BD */
     uint        bd_data_cnt;   /* byte count in buffer */
     pciaddr_t	 bd_bufp;       /* pointer to buffer PCI ADDRESS */
} tlan_rxbd_t;

typedef struct BD_BUF
{
    uint        bd_data_cnt;   /* byte count in buffer */
    pciaddr_t	bd_bufp;       /* pointer to buffer PCI ADDRESS*/
} tlan_bdbuf_t;


typedef struct TLANLIST_TXBD
{
     pciaddr_t	 bd_next;    /* Forward pointer of BD, PCI ADDRESS */
     ushort      bd_cstat;      /* CSTAT field of BD */
     short       bd_frame_sz;   /* size of frame in BD */
     tlan_bdbuf_t bdbuf[9];	/* Transmit BD has 9 buffer segments */
} tlan_txbd_t;



typedef struct TLAN_NETSTAT
{
     u_int        tx_good_frm;
     u_int        tx_underrun_frm;
     u_int        tx_deferred_frm;
     u_int        tx_multicollision_frm;
     u_int        rx_good_frm;
     u_int        rx_overrun_frm;
     u_int        crc_frm;
     u_int        codeerr_frm;
     u_int        singlecollision_frm;
     u_int        excesscollision_frm;
     u_int        latecollision_frm;
     u_int        carrierlost_frm;
} tlan_stat_t;


typedef union DIO_REG
{
     u_char	byte;
     ushort     word;
} dio_reg;



/* Starting address in PCI space for Universe DMA command list as well as
 * TLAN transmit/receive BD lists
 */

#define MAXBOARDS 	4
/* #define DMA_LIST_NO	32 */
#define DMA_LIST_NO	18
#define DMA_IMAGE	l

#define TX_QUEUE_NO	128
#define RBD_LIST_NO	98
#define TBD_LIST_NO	32
#define BD_PAD_SIZE	16
#define BUFFSIZE	1600

#define CRC_LEN 4
#define EHDR_LEN 14
#define MIN_TPKT 60
#define MAX_TPKT (EHDR_LEN+ETHERMTU)	/* 1550 bytes */
#define MIN_RPKT (MIN_TPKT+CRC_LEN)
#define MAX_RPKT (EHDR_LEN+ETHERMTU)

#define  UNIVERSE_ID    0x000010e3
#define  TLAN_VEND_ID   0x107e          /* InterPhase Vender ID */
#define  TLAN_DEV_ID    0x0006          /* InterPhase 100BaseTX device ID */
#define  TLAN_MACADDR   0x83            /* EEPROM Addr. where MAC stored */
#define  PHY83840_IDHI  0x2000
#define  PHY83840_IDLO  0x5c00
#define  UV_PCI_MEM     0x80000000      /* IP-6200 PCI RAM starting address */
#define  TLAN0_REG_MEM  0x20000000      /* PMC TLAN Host register PCI address */
#define  TLAN1_REG_MEM  0x20008000      /* PMC TLAN Host register PCI address */

#define  MBUF_SEG       9               /* frag. mbuf limit for cluster mbuf */
/*
#define  INV_ETHER_IP6200 24
*/

/* 
 * Universe Register Map definitions
 */

#define PCI_ID 		0x000		/* PCI config. space ID reg. */
#define PCI_CSR		0x004		/* PCI Control and Status register */
/* 0x040 - 0x0FF Universe PCI autoconfig reg. Not used. */

#define LSI0_CTL	0x100		/* PCI Slave Image 0 Control */
#define LSI0_BS		0x104		/* slave Image 0 Base address */
#define LSI0_BD		0x108		/* slave Image 0 bound Address */
#define LSI0_TO		0x10c		/* slave Image 0 translate offset */
#define LS_OFFSET	0x014		/* offset to next image reg. set */

#define SCYC_CTL	0x170		/* Special Cycle Control reg. */
#define SCYC_ADDR	0x174		/* Special Cycle  PCI addr reg. */
#define SCYC_EN		0x178		/* Swap/comppare enable reg. */
#define SCYC_CMP	0x17c		/* Special Cycle compare data reg. */
#define SCYC_SWP	0x180		/* Special Cycle swap data reg. */

#define LMISC		0x184		/* PCI misc. register */
#define SLSI		0x188		/* special PCI slave image */
#define L_CMDERR	0x18c		/* PCI command error log reg */
#define LAERR		0x190		/* PCI address error log reg. */

#define DCTL		0x200		/* DMA transfer control reg. */
#define DTBC		0x204		/* DMA transfer byte count reg. */
#define DLA		0x208		/* DMA PCI bus addr. reg. */
#define DVA		0x210		/* DMA VME bus addr. reg. */
#define DCPP		0x218		/* DMA command packet pointer */
#define DGCS		0x220		/* DMA control and status reg. */
#define D_LLUE		0x224		/* DMA linked list update enable */

/* 0x300 - 0x30c PCI interrupt register set. Not used */

#define VINT_EN		0x310		/* VME bus interrupt enable */
#define VINT_STAT	0x314		/* VME bus interrupt status */
#define VINT_MAP0	0x318           /* VME bus interrupt Map 0 */
#define VINT_MAP1	0x31c           /* VME bus interrupt Map 1 */
#define VINT_ID  	0x320           /* VME bus interrupt Vector */
#define VINT_STATID  	0x320           /* VME bus interrupt stat reg. */

#define MAST_CTL	0x400		/* Master control */
#define MISC_CTL	0x404		/* Misc. control */
#define MISC_STAT	0x408		/* Misc. status */
#define USER_AM		0x40c		/* User AM code reg. */

#define VSI0_CTL	0xf00		/* VME Slave Image 0 Control */
#define VSI0_BS		0xf04		/* slave Image 0 Base address */
#define VSI0_BD		0xf08		/* slave Image 0 bound Address */
#define VSI0_TO		0xf0c		/* slave Image 0 translate offset */
#define VSI1_CTL	0xf14		/* VME Slave Image 1 Control */
#define VSI1_BS		0xf18		/* slave Image 1 Base address */
#define VSI1_BD		0xf1c		/* slave Image 1 bound Address */
#define VSI1_TO		0xf20		/* slave Image 1 translate offset */
#define VSI2_CTL	0xf28		/* VME Slave Image 2 Control */
#define VSI2_BS		0xf2c		/* slave Image 2 Base address */
#define VSI2_BD		0xf30		/* slave Image 2 bound Address */
#define VSI2_TO		0xf34		/* slave Image 2 translate offset */
#define VSI3_CTL	0xf3c		/* VME Slave Image 3 Control */
#define VSI3_BS		0xf40		/* slave Image 3 Base address */
#define VSI3_BD		0xf44		/* slave Image 3 bound Address */
#define VSI3_TO		0xf48		/* slave Image 3 translate offset */

#define VRAL_CTL	0xf70		/* VME access image control reg. */
#define VRAL_BS		0xf74		/* VME access image base address */

#define VCSR_CTL	0xf80		/* VMEbus CSR control reg. */
#define VCSR_TO		0xf84		/* VMEbus CSR Translation offset */
#define V_AMERR		0xf88		/* VMEbus AM code error log */
#define VAERR		0xf8c		/* VMEbus Address error log */

#define VCSR_CLR	0xff4		/* VMEbus CSR bit clear reg. */
#define VCSR_SET	0xff8		/* VMEbus CSR bit set reg. */
#define VCSR_BS		0xffc		/* VMEbus CSR base Address */


/* 
 * Universe specific Macro defines
 */

/* 
 * Macro to swap 4-byte integer for Big and Little Endian machines
 * X[0][1][2][3]  <==> X[3][2][1][0]
 */
#ifdef JC_XXX
#define EN_SWAP(x)   { \
        * ((u_char *)&(x))     = (*((u_char *)&(x)) ^ *(((u_char *)&(x))+3)); \
        * (((u_char *)&(x))+3) = (*((u_char *)&(x)) ^ *(((u_char *)&(x))+3)); \
        * ((u_char *)&(x))     = (*((u_char *)&(x)) ^ *(((u_char *)&(x))+3)); \
        * (((u_char *)&(x))+1) = (*(((u_char *)&(x))+1) ^ *(((u_char *)&(x))+2)); \        * (((u_char *)&(x))+2) = (*(((u_char *)&(x))+1) ^ *(((u_char *)&(x))+2)); \        * (((u_char *)&(x))+1) = (*(((u_char *)&(x))+1) ^ *(((u_char *)&(x))+2)); \}

#define SWAP_2(x)   { \
        * ((u_char *)&(x))     = (*((u_char *)&(x)) ^ *(((u_char *)&(x))+1)); \
        * (((u_char *)&(x))+1) = (*((u_char *)&(x)) ^ *(((u_char *)&(x))+1)); \
        * ((u_char *)&(x))     = (*((u_char *)&(x)) ^ *(((u_char *)&(x))+1));}
}

#endif

#define EN_SWAP(x)   { \
	u_char tmp; \
          tmp                  = *((u_char *)&(x)); \
	* ((u_char *)&(x))     = *(((u_char *)&(x))+3); \
  	* (((u_char *)&(x))+3) =  tmp; \
	  tmp                  = *(((u_char *)&(x))+2); \
	* (((u_char *)&(x))+2) = *(((u_char *)&(x))+1); \
	* (((u_char *)&(x))+1) = tmp; \
}

#define D_SWAP(x) ((x)>>24 | ((x)>>8)&0xff00 | (x)<<24 | ((x)<<8)&0xff0000)

#define SWAP_2(x)   { \
        u_char tmp; \
          tmp                  = *((u_char *)&(x)); \
        * ((u_char *)&(x))     = *(((u_char *)&(x))+1); \
        * (((u_char *)&(x))+1) =  tmp; \
}


/*
 * given the address of a VME slave image, return physical address in
 * the PCIbus memory space
 */
#define  PCIADDR(fe,vmeaddr) ((__psunsigned_t)vmeaddr - (__psunsigned_t)fe->VME_SLAVE +(__psunsigned_t) fe->SYNC_RAM )

/* - not used */
/*#define  VMEADDR(fe,pciaddr) ((__psunsigned_t)pciaddr + (__psunsigned_t)fe->VME_SLAVE -(__psunsigned_t) fe->SYNC_RAM ) */


/* 
 * Macro to increment a BD pointer of a (transmit or receive)list
 */
#define  INC_BD(base,ptr,limit) { \
		ptr ++; \
		ptr = (ptr > limit) ? base : ptr; \
	 }

/* 
 * Macro to decrement a BD pointer of a (transmit or receive)list
 */
#define  DEC_BD(base,ptr,limit) { \
		ptr --; \
		ptr = (ptr < base) ? limit : ptr; \
	 }

/* 
 * Macro to get alternate PMC for DMA access
 */
#define OTHERPMC(x) ((x->feinfo->pmc == x) ? &x->feinfo->pmc[1] : &x->feinfo->pmc[0])
#define NEXTPMC(fe) ((fe->uv_lastpmc == &fe->pmc[0]) ? &fe->pmc[1] : &fe->pmc[0])
/* 
 * Macro to get address of a Universe CSR register
 */
#define  UREG(devp,reg) ((__psunsigned_t)(devp->uv_regp) + (__psunsigned_t)reg)

#define  UVREG_WR(devp,reg,val) { \
        *(__uint32_t *)(UREG(devp,reg)) = D_SWAP(val); \
}

/* old:
	EN_SWAP(val); \
	*(__uint32_t *)(UREG(devp,reg)) = val; \
} 
*/

#define  UVREG_RD(devp,reg,val) { \
        val = (__uint32_t)D_SWAP(*((__uint32_t *)(UREG(devp,reg)))); \
}

/* old:
	val = (__uint32_t)*((__uint32_t *)(UREG(devp,reg))); \
	EN_SWAP(val);	\
}
*/

/*----------------------------------------------------------------------
 * Universe PCI_CSR Reg. definition (off. 0x004)
 *---------------------------------------------------------------------*/
#define  CSR_DPE	0x80000000	/* Parity Error detect set */
#define  CSR_SERR	0x40000000	/* SERR# asserted */
#define  CSR_RMA	0x20000000	/* Received Master-Abort */
#define  CSR_RTA	0x10000000	/* Received Target-Abort */
#define  CSR_STA	0x08000000	/* Singalled Target-Abort */
#define  CSR_DEVSEL	0x02000000	/* Device Select Med. speed */
#define  CSR_DPD	0x01000000	/* Data Parity Detected */
#define  CSR_TFBBC	0x00800000	/* Target Fast Back-to-back capable */
#define  CSR_MFBBC	0x00000200	/* Master Fast back to back enable */
#define  CSR_SERREN	0x00000100	/* SERR# enable */
#define  CSR_WAIT	0x00000080	/* Wait cycle control */
#define  CSR_PERESP	0x00000040	/* Parity Error Response */
#define  CSR_VGAPS	0x00000020	/* VGA Palette snoop */
#define  CSR_MWIEN	0x00000010 	/* Memory Write and invalid. enable */
#define  CSR_SC		0x00000008	/* Special Cycle */
#define  CSR_BM		0x00000004	/* Master Enable */
#define  CSR_MS		0x00000002	/* Target Memory Enable */
#define  CSR_IOS	0x00000001	/* Target IO emable */
 
/*----------------------------------------------------------------------
 * Universe DCTL Reg. definition (off. 0x200)
 *---------------------------------------------------------------------*/
#define  DCTL_V2L	0x00000000	/* DMA direction - VME-to-PCI */
#define  DCTL_L2V	0x80000000	/* DMA direction - PCI-to-VME */
#define  DCTL_VDW_8	0x00000000	/* VME Max. Data width - 8 bits */
#define  DCTL_VDW_16	0x00400000	/* VME Max. Data width - 16 bits */
#define  DCTL_VDW_32	0x00800000	/* VME Max. Data width - 32 bits */
#define  DCTL_VDW_64	0x00c00000	/* VME Max. Data width - 64 bits */
#define  DCTL_VAS_A16   0x00000000	/* VME address space - A16 */
#define  DCTL_VAS_A24   0x00010000	/* VME address space - A24 */
#define  DCTL_VAS_A32   0x00020000	/* VME address space - A32 */
#define  DCTL_VAS_U1    0x00060000	/* VME address space - User1 */
#define  DCTL_VAS_U2    0x00070000	/* VME address space - User2 */
#define  DCTL_PGM_D 	0x00000000	/* Program/Data mode - Data */
#define  DCTL_PGM_G	0x00004000	/* Program/Data mode - Program */
#define  DCTL_SUP_N	0x00000000	/* Supervisor/User AM code - Non super */
#define  DCTL_SUP_S	0x00001000	/* Supervisor/User AM code - Non super */
#define  DCTL_VCT_SNG	0x00000000	/* VMEbus Cycle Type - Single Cycle */
#define  DCTL_VCT_BLK	0x00000100	/* VMEbus Cycle Type - block transfer*/
#define  DCTL_LD64EN	0x00000080	/* Enable 64bit PCI bus transaction */

/*----------------------------------------------------------------------
 * Universe DGCS Reg. definition (off. 0x220)
 *---------------------------------------------------------------------*/
#define  DGCS_GO	0x80000000	/* DMA Go bit */
#define  DGCS_STOP_REQ	0x40000000	/* Stop DMA when all data is done */
#define  DGCS_HALT_REQ	0x20000000	/* Stop DMA at end of current packet */
#define  DGCS_CHAIN	0x08000000	/* DMA linked list mode */
#define  DGCS_ACT	0x00008000	/* DMA Active Flag */
#define  DGCS_STOP	0x00004000	/* DMA Stopped flag */
#define  DGCS_HALT	0x00002000	/* DMA Halted flag */
#define  DGCS_DONE	0x00000800	/* DMA Transfer complete */
#define  DGCS_LERR	0x00000400	/* DMA PCI Bus error */
#define  DGCS_VERR	0x00000200	/* DMA VME bus error */
#define  DGCS_PERR	0x00000100	/* DMA Protocol error */
#define  DGCS_ISTP	0x00000040	/* Interrupt when stopped */
#define  DGCS_IHLT	0x00000020	/* Interrupt when halted */
#define  DGCS_IDONE	0x00000008	/* interrupt when done */
#define  DGCS_ILERR	0x00000004	/* Interrupt when LERR */
#define  DGCS_IVERR	0x00000002	/* Interrupt when VERR */
#define  DGCS_IMERR	0x00000001	/* Interrupt when Protocol error */


/*----------------------------------------------------------------------
 * Universe VINT_EN Reg. definition (off. 0x310)
 *---------------------------------------------------------------------*/
#define  EN_LINT0	0x00000001	/* Enable LINT0 */
#define  EN_LINT1	0x00000002	/* Enable LINT1 */
#define  EN_LINT2	0x00000004	/* Enable LINT2 */
#define  EN_LINT3	0x00000008	/* Enable LINT3 */
#define  EN_LINT4	0x00000010	/* Enable LINT4 */
#define  EN_LINT5	0x00000020	/* Enable LINT5 */
#define  EN_LINT6	0x00000040	/* Enable LINT6 */
#define  EN_LINT7	0x00000080	/* Enable LINT7 */
#define  EN_DMA  	0x00000100	/* Enable DMA   */
#define  EN_LERR	0x00000200	/* Enable LERR */
#define  EN_VERR	0x00000400	/* Enable VERR */
#define  EN_SWIACK	0x00001000	/* Enable SWIACK */

/*----------------------------------------------------------------------
 * Universe MISC_CTL Reg. definition (off. 0x404)
 *---------------------------------------------------------------------*/
#define  SW_LRST	0x00800000	/* Software PCI Reset */
#define  SW_SRST	0x00400000	/* Software VMEbus SYSRESET */


/*----------------------------------------------------------------------
 * Universe VSI_CTL Reg. definition (off. 0xf00,0xf14,0xf28,0xf3c)
 *---------------------------------------------------------------------*/
#define  VSI_EN  	0x80000000	/* Image Enable */
#define  VSI_PWNE  	0x40000000	/* Posted Write enable */
#define  VSI_PREN  	0x20000000	/* Prefetch Read Enable */
#define  VSI_BOTH  	0x00C00000	/* Program/Data AM Mode */
#define  VSI_PGM  	0x00800000	/* Program AM Mode */
#define  VSI_DATA  	0x00400000	/* DATA AM Mode */
#define  VSI_SUPUR  	0x00300000	/* Super/User Mode */
#define  VSI_SUPER  	0x00200000	/* Super Mode */
#define  VSI_USER  	0x00100000	/* User Mode */
#define  VSI_VAS16  	0x00000000	/* VME A16 Mode */
#define  VSI_VAS24  	0x00010000	/* VME A24 Mode */
#define  VSI_VAS32  	0x00020000	/* VME A32 Mode */
#define  VSI_LD64  	0x00000080	/* Enable 64bit PCI bus transaction */
#define  VSI_LLRMW  	0x00000040	/* Enable PCI bus lock */
#define  VSI_LAS_CFG  	0x00000002	/* PCI bus Configuration space */
#define  VSI_LAS_IO  	0x00000001	/* PCI bus I/O space */
#define  VSI_LAS_MEM  	0x00000000	/* PCI bus Memory space */



/*----------------------------------------------------------------------
 * Define Universe DMA command packet structure 
 *---------------------------------------------------------------------*/
typedef struct DMA_CMD_PACKET
{
    __uint32_t	dctl;		/* DMA transfer control */
    __uint32_t	dtbc;		/* DMA transfer byte count */
    pciaddr_t	dla;		/* DMA PCIbus Address */
    __uint32_t	resv1;		
    vmebusaddr_t dva;		/* DMA VMEbus Address */
    __uint32_t	resv2;	
    pciaddr_t	dcpp;		/* DMA command packet Pointer */
#define PROCESSED  0x02000000
#define NULLBIT	   0x01000000
    __uint32_t	resv3;		/* pad it for 8 byte alignment */
} uv_dmacmd_t;


typedef struct DMA_OPTABLE
{
    struct PMC_DEV   * pmc;	/* PMC port associated with the DMA op */
    volatile
    tlan_txbd_t	* bd;		/* BD associated with this DMA op */
    struct mbuf * mb;		/* Either Tx or Rx message */
    pciaddr_t     dbuf;		/* address of data buffer */
    int	 	  event;	/* DMA event code */
#define	RX_CMP  1
#define	TX_FRST 2
#define	TX_MORE 3
#define	TX_CMP  4
} uv_dmatbl_t;
    

/*----------------------------------------------------------------------
 * Define memory map for the VME Slave images
 * Note: each slave image to be created MUST be 4K page size aligned 
 *---------------------------------------------------------------------*/
typedef struct VME_SLAEVMAP
{
    union
    /* both slave image #0 and #1 uses the same VME memory space for mapping 
     * because once the PMC devices are autoconfig, there will be no use
     * for the slave image any more and thus may be destroyed. The VME mem
     * space once used by slave image #0 can be reused by slave image #1
     */
    {
        /* SLAVE IMAGE #0 - 
	 *    Shared by: PMC PCI Autoconfig memory mapping area and
         *               UNIVERSE DMA, TLAN BD list PCI Memory mapping area
	 */
        tlan_conf_t	tlan_conf_reg;
	uint_t		page_align[16*1024];
        uv_dmacmd_t	uv_dmalist[DMA_LIST_NO];
    } share_map;
    /*SLAVE IMAGE #1 - TLAN 0 & 1 HOST REG. PCI Memory mapping area */
    host_reg_t	tlan_hostreg[2];
} vme_map_t;

struct pmcdbg {
    int out_alloc;
    int out_copy;
    int aggr_out;
    int aggr_in;
    int o_packet;
    int i_packet;
};


/* Per Fast Ethernet port Device(PMC) data structure */
typedef struct PMC_DEV
{
    struct etherif    pmcif;   		/* Ethernet network interface struct */
    struct FENET_DEV *feinfo;		/* backward pointer to its master */
    /* TLAN Host Register Slave Image Base Address */
    volatile
    host_reg_t  *tlan_reg;		/* TLAN host register slave image */
    /* PMC Network interface transmit mbuf queue */
    struct mbuf *txq_list[TX_QUEUE_NO];
    int		 phy;			/* PHY device number */
    int 	 txq_geti;		/* Tx mbuf get queue index */
    int          txq_puti;		/* Tx mbuf put queue index */
    short	 pend_reset;
    short	 pend_init;
    volatile
    tlan_rxbd_t	*tl_rxbdlist[5];	/* RX BD List pointer array */
    volatile
    tlan_txbd_t	*tl_txbdlist[5];	/* TX BD List pointer array */
#define RX_BASE 0			/* Rx list base pointer */
#define RX_LIMT 1			/* Rx list limit pointer */
#define RX_CURR 2			/* Rx list curr pointer */
#define RX_LAST 3			/* Rx list last pointer */
#define RX_NEXT 4			/* Rx list next pointer */
#define TX_BASE 0			/* Tx list base pointer */
#define TX_LIMT 1			/* Tx list limit pointer */
#define TX_CURR 2			/* Tx list curr pointer */
#define TX_LAST 3			/* Tx list last pointer */
#define TX_NEXT 4			/* Tx list next pointer */
    pciaddr_t	 rx_bufp;		/* Receive buffer begins */
    pciaddr_t	 tx_bufp;		/* Receive buffer begins */
    int		 pmc_opstat;		/* PMC operational states */
#define PMC_NOP   0x01			/* PMC not operational */
#define PMC_INIT  0x02			/* PMC in init state */ 
#define PMC_RESET 0x04			/* PMC in reset state */
#define PMC_DOWN  0x08			/* PMC in down state */
#define PMC_UP    0x10			/* PMC in up state */
#define PMC_RXRDY 0x20			/* PMC in down state */
#define PMC_TXBSY 0x40			/* PMC in down state */
#define PMC_RXOVRUN 0x100			/* PMC in down state */
#define PMC_PENDOVR 0x200			/* PMC in down state */
    volatile
    tlan_conf_t auto_config;		/* PCI autoconfig contents */
    tlan_stat_t net_stat;		/* Network statistic data */
    u_char macaddr[6];			/* MAC address string */
    struct pmcdbg dbg_stat;		/* debug stats structure */
    int  vfe_num;               /* interface number */
    int  speed;                 /* current running speed */
    int  duplex;                        /* full or half-duplex */
    int  rem_fault;                     /* remote fault? */
    int  jabbering;                     /* jabbering? */
    int  link_down;                     /* link_down? */
/* RSVP support */
#define VFE_PSENABLED 0x01 
#define VFE_PSINITIALIZED 0x02
    int  vfe_rsvp_state;
    int vfe_nmulti;
    int txfull;
} pmc_dev_t;

#define pmc_ac	pmcif.eif_arpcom	/* struct for ARP stuff */
#define pmc_if	pmc_ac.ac_if		/* network-visible interface */
#define pmc_rawif pmcif.eif_rawif       /* struct for snoop stuff */

typedef struct uvdmamap
{
    dmamap_t    *mapp;
    int          map_idx;
} uv_dmamap_t;

#define RND_MAP_INX(m) ((m+1) & ~1)
#define MAX_DMA_ELEM 6
#define OUT_MAP_LEN ((MAX_DMA_ELEM*2+2)*(IFQ_MAXLEN+1))


/* Per IP-6200 board data structure */
typedef struct FENET_DEV
{
    volatile
    __uint32_t  *uv_boardaddr;		/* VME Base address */
    iamap_t     *uv_iamap;		/* IA map */
    dmamap_t    *uv_dmamap[DMA_LIST_NO+1];
    volatile
    __uint32_t	*uv_regp;		/* pointer to Universe reg space */
    __int32_t    uv_adapter;		/* VME adapter number */
    volatile
    uv_dmacmd_t	*uv_dmalistp;		/* Universe DMA command list ptr */
    pmc_dev_t   *uv_lastpmc;		/* last PMC using the DMA channel */
    caddr_t	VME_SLAVE;		/* slave image of IP-6200 Sync. RAM (VME)*/
    vmebusaddr_t VME_ADDR;		/* IP-6200 VME address(as seen in A32 space) */
    pciaddr_t	SYNC_RAM;		/* true memory map of IP-6200 Sync. RAM (PCI)*/
    u_char	 uv_intvec;		/* assigned Universe Interrupt vector */
    u_char	 uv_intbrl;		/* Interrupt VME bus request level */
    uv_dmatbl_t  uv_dmarec[DMA_LIST_NO];/* record of DMA requests */
    int		 uv_stat;		/* operation status of the Universe chip */
#define UV_DMARDY   0x01		/* DMA channel is busy */
    pmc_dev_t    pmc[2];		/* PMC specific data struct */
    uv_dmacmd_t  uv_dmacmd[DMA_LIST_NO]; /* in-core copy of dma list */
    lock_t	 vfedma_lock;	
    __int32_t    uv_unit;		/* VME unit number - board number */
} fenet_dev_t;


union
{
    uint_t    u4;
    ushort    u2;
    u_char    u1;
} pci_reg;





/*
 * data structures and macro defines for the Network interface (if)
 */


/* Receive buffer structure */
struct rcvbuf {
        struct etherbufhead     ebh;
        char                    data[MAX_RPKT];
};

/* Defines and macros that replace struct VME_SLAEVMAP, VME
   slave image memory mapping,  used in 5.3.   
*/

#define VMEMAP_BASE	0
#define VMEMAP_TLAN_HOST0	(VMEMAP_BASE+0x10000) /* == 0x10000 */
#define VMEMAP_TLAN_HOST1	(VMEMAP_TLAN_HOST0+0x8000) /* == 0x8000 */
#define VMEMAP_AUTOCONFIG	VMEMAP_BASE	
#define VMEMAP_END	        (VMEMAP_TLAN_HOST1+0x8000) /* == 0x20000 */

#define VMEMAP_DMALIST_BASE	VMEMAP_BASE

#define GET_TLANHOST0(fe)	((vmebusaddr_t)fe->VME_ADDR + (__uint32_t)VMEMAP_TLAN_HOST0)
#define GET_MAPEND(fe)	        ((vmebusaddr_t)fe->VME_ADDR + (__uint32_t)VMEMAP_END)

#define GET_TLANHOST0_SLAVE(fe)	(fe->VME_SLAVE + (__uint32_t)VMEMAP_TLAN_HOST0)
#define GET_TLANHOST1_SLAVE(fe)	(fe->VME_SLAVE + (__uint32_t)VMEMAP_TLAN_HOST1)

#define GET_DMALIST(fe)		((vmebusaddr_t)fe->VME_ADDR + (__uint32_t)VMEMAP_DMALIST_BASE)
#define GET_DMALIST_SLAVE(fe)	(fe->VME_SLAVE + (__uint32_t)VMEMAP_DMALIST_BASE)
