/*
 * Include file for if_gfe GIO Fast Ethernet Driver for IP 22
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
#ident "$Revision: 1.1 $"

/* Host command Register */
#define  CMD_GO         0x80000000              /* Network channel go command */
#define  CMD_STP        0x40000000              /* Network channel stop command */
#define  CMD_ACK        0x20000000              /* Interrupt Acknowledge */
#define  CMD_CH0_SEL    0x00000000              /* Channel 0 select */
#define  CMD_CH1_SEL    0x00200000              /* Channel 1 select */
#define  CMD_EOC        0x00100000              /* End of Channel Select */
#define  CMD_RTX        0x00080000              /* Rx/Tx Select */
#define  CMD_NES        0x00040000              /* Not Error/Statistics */
#define  CMD_ADPRST     0x00008000              /* Adapter Reset command */
#define  CMD_LDTMR      0x00004000              /* Load Interrupt Timer command */
#define  CMD_LDTHR      0x00002000              /* Load Tx Interrupt Threshold */
#define  CMD_REQINT     0x00001000              /* Request host interrupt command */
#define  CMD_INTOFF     0x00000800              /* PCI interrupt OFF */
#define  CMD_INTON      0x00000400              /* PCI interrupt ON */

#define  TX_INT_THR	0x5			/* threshold # */
#define  INT_TIMER	0x1

/* Host Interrupt Register */
#define  TXEOF	0x0004		/* Tx Eof interrupt */
#define  STATOV	0x0008		/* Statistics overflow interrupt */
#define  RXEOF	0x000c		/* Rx Eof interrupt */
#define  DUMMY	0x0010		/* Dummy interrupt */
#define  TXEOC	0x0014		/* Tx Eoc interrupt */
#define  ADPCHK	0x0038		/* Adapter check interrupt */
#define  NSTAT  0x0018		/* Network status interrupt */
#define  RXEOC	0x001c		/* Rx Eoc interrupt */

/* config IDs */
#define TL_ID			0x0500104c

/* Internal register definitions */
#define NET_CMD                 0x03        /*Net Command */
#define NET_SIO                 0x02        /*Net Serial I/O */
#define NET_STS                 0x01        /*Net Status */
#define NET_MASK                0x00        /*Net Error Mask */
#define NET_CONFIG                0x06	      /*Net Config */
#define NET_AREG0               0x10        /*General address */
#define NET_AREG1               0x16        /*General address */
#define NET_AREG2               0x1C        /*General address */
#define NET_AREG3               0x22        /*General address */
#define NET_HASH1               0x28        /*Hash address */
#define NET_HASH2               0x2C        /*Hash address */
#define NET_SGOODTX             0x30
#define NET_SGOODRX             0x34
#define NET_SDEFERTX            0x38
#define NET_SMULTICOLLTX        0x3c
#define NET_SEXCESSCOLL         0x40
#define ACOMMIT                 0x40        /*adapter PCI commit size */
#define LED                     0x47
#define BSIZE                   0x46        /*Burst size (tx=4msbs,rx=4lsbs) */
#define MAXRX                   0X44        /*Maximum Rx frame size */

	/* net_cmd */
#define     NRESET                  0x80
#define     NWRAP                   0x40
#define     CSF                     0x20
#define     CAF                     0x10
#define     NOBRX                   0x08
#define     NC_DUPLEX               0x04
#define     TRFRAM                  0x02
#define     TXPACE                  0x01

	/* net_sio */
#define     MDATA                   0x01
#define     MTXEN                   0x02
#define     MCLK                    0x04
#define     NMRST                   0x08
#define     EDATA                   0x10
#define     ETXEN                   0x20
#define     ECLK 	            0x40
#define     MINTEN                  0x80

	/* net_sts */
#define     MIRQ                    0x80    /*Mii interrupt request */
#define     HBEAT                   0x40    /*Heart Beat Error    */ 
#define     TXSTOP                  0x20
#define     RXSTOP                  0x10

	/* net_mask */
#define     MIIMASK                 0x80
#define     HEARTBEATMASK           0x40
#define     TRANSMITSTOPMASK        0x20
#define     RECEIVESTOPMASK         0x10

	/* net_config */
#define     PHY_EN                  0x0080
#define     MAC_SELECT_MASK         0x007F
#define     MACP_10MB               0         /*10mbit ethernet csma/cd */
#define     MACP_100CHANNEL         1         /*100mbit channel priority */
#define     MACP_100CSTAT           2         /*100mbit cstat priority */
#define     MACP_EXT                3
#define     RCLKTEST                0x8000
#define     TCLKTEST                0x4000
#define     BITRATE                 0x2000
#define     RXCRC                   0x1000
#define     PEF                     0x0800
#define     ONEFRAG                 0x0400
#define     ONECHN                  0x0200
#define     MTEST                   0x0100

	/* acommit */
#define     BLP_UNDEFINED           0x01
#define     BLP_AUI                 0x02
#define     BLP_LOOPBACK            0x04
#define     BLP_HDUPLEX             0x08
#define     TRANSWHOLEFRAME         0x70

/* MII Registers */
#define GEN_CTL                 0x00        /* Generic Control Register */
#define GEN_STS                 0x01        /* Generic Status Register */
#define GEN_ID_HI               0x02        /* Generic identifier (high) */
#define GEN_ID_LO               0x03        /* Generic identifier (lo) */
#define AN_ADVERTISE            0x04        /* Auto-neg. advertisement */
#define AN_LPABILITY            0x05        /* Auto-neg. Link-Partner ability */
#define AN_EXPANSION            0X06        /* Auto-neg. expansion */
#define TLPHY_ID                0x10        /* TLAN PHY identifier */
#define TLPHY_CTL               0x11        /* TLAN PHY control register */
#define TLPHY_STS               0x12        /* TLAN PHY status register */

	/* gen_ctl */
#define     PHYRESET    0x8000
#define     LOOPBK      0x4000
#define     SPEED       0x2000
#define     AUTOENB     0x1000
#define     PDOWN       0x0800
#define     ISOLATE     0x0400
#define     AUTORSRT    0x0200
#define     DUPLEX      0x0100
#define     COLTEST     0x0080

	/* gen_sts */
#define     AUTOCMPLT           0x20
#define     RFLT                0x10        
#define     LINK                0x04
#define     AUTO_NEG    0x08	     
#define     EXTCAP      0x01	     
#define     JABBER	0x02

#define     TFOUR       0x8000	     
#define     HTX_FD      0x4000	     
#define     HTX_HD      0x2000	     
#define     TENT_FD     0x1000	     
#define     TENT_HD     0x0800	     

#define     TA_TFOUR    0x0200	    /* from IEEE spec xxxxx */
#define     TA_HTX_FD   0x0100	    /* from IEEE spec xxxxx */
#define     TA_HTX_HD   0x0080	    /* from IEEE spec xxxxx */
#define     TA_TENT_FD  0x0040	    /* from IEEE spec xxxxx and Ours */
#define     TA_TENT_HD  0x0020        
#define     FDUPLEX             0x40
#define     HDUPLEX             0x20

	/* tlphy_ctl */
#define     IGLINK      0x8000  
#define     INTEN       0x0002
#define     TINT        0x0001

	/* tlphy_sts */
#define     MINT                0x8000
#define     PHOK                0x4000
#define     POLOK               0x2000
#define     TPENERGY            0x1000

#define  PHY83840_IDHI  0x2000
#define  PHY83840_IDLO  0x5c00

/* 100VG MII Registers */
#define VGPHY_ID                0x10        /* TLAN PHY identifier */
#define VGPHY_CTL               0x11        /* TLAN PHY control register */
#define VGPHY_STS               0x12        /* TLAN PHY status register */

	/* vgphy_ctl */
#define     TRIDLE                  0x0010
#define     TRFAIL                  0x0020

	/* vgphy_sts */
#define     RETRAIN                 0x0040
#define     LSTATE                  0x0030
#define     TRFRTO                  0x0008
#define     RTRIDL                  0x0004


/* swapping macros 'x' stands for address of data */
#define	Master_SwapInt(x) \
	((x)>>24 | ((x)>>8)&0xff00 | (x)<<24 | ((x)<<8)&0xff0000)

#define Master_Swap2B(x) 	((x)>>8 | (x)<<8)
#define SwapInt(x)	x
#define Swap2B(x)	x

#define Gfe_Netsio_Clr(bits) { \
        sio = Gfe_Dio_Read(u_char,NET_SIO); sio &= ~(bits); \
        Gfe_Dio_Write(u_char, NET_SIO, sio); }

#define Gfe_Netsio_Set(bits) { \
        sio = Gfe_Dio_Read(u_char,NET_SIO); sio |=  (bits); \
        Gfe_Dio_Write(u_char,NET_SIO,sio); }

#define Gfe_Mii_Clck { \
        Gfe_Netsio_Clr(MCLK); Gfe_Netsio_Set(MCLK); }

#define Gfe_Dio_Read(t, r) \
        (DELAYBUS(10), ba->dio_addr = r, DELAYBUS(10), *(volatile t *)(ba->dio_data + (r&0x3)))

#define Gfe_Dio_Write(t, r, v) \
        (DELAYBUS(10), ba->dio_addr = r, DELAYBUS(10), *(volatile t *)(ba->dio_data + (r&0x3)) = v)

#define Gfe_Eeprom_Start_Seq \
        { Gfe_Netsio_Set(ECLK);  Gfe_Netsio_Set(EDATA); \
          Gfe_Netsio_Set(ETXEN); Gfe_Netsio_Clr(EDATA); \
          Gfe_Netsio_Clr(ECLK); }

#define Gfe_Eeprom_Stop_Seq \
        { Gfe_Netsio_Set(ETXEN); Gfe_Netsio_Clr(EDATA); \
          Gfe_Netsio_Set(ECLK);  Gfe_Netsio_Set(ETXEN); \
          Gfe_Netsio_Set(EDATA); Gfe_Netsio_Clr(ETXEN); }

#define Gfe_Eeprom_Start_Access_Seq \
        { Gfe_Netsio_Set(ETXEN); Gfe_Netsio_Set(ECLK);  \
          Gfe_Netsio_Set(EDATA); Gfe_Netsio_Set(ETXEN); \
          Gfe_Netsio_Clr(EDATA); Gfe_Netsio_Clr(ECLK);  }

#define Gfe_Eeprom_Stop_Access_Seq \
        { Gfe_Netsio_Clr(EDATA); Gfe_Netsio_Set(ECLK);  \
          Gfe_Netsio_Set(EDATA); Gfe_Netsio_Clr(ETXEN); }

#define Gfe_Eeprom_Clck { \
        Gfe_Netsio_Set(ECLK); Gfe_Netsio_Clr(ECLK); }
	
/* useful */
#define MAX_MIISYNCS 	32
#define EADDR		0x83
#define READ_OP		0
#define WRITE_OP		1

#define TL_ERR_INIT     1500
#define EHDR_LEN		14
#define EDAT_LEN		1500
#define MAX_RPKT		(EHDR_LEN+1500)
#define SWAP_MAX_RPKT		0xEA05
#define BUF_SIZE		1500+sizeof(struct etherbufhead)
#define RCVPAD			sizeof(struct etherbufhead)-sizeof(struct ether_header)-ETHERHDRPAD
#define MIN_TPKT_U		60
#define GIO_INTERRUPT_1		1	

/* Faste */
#define FE_BC_OFF		0x20000
#define FE_STAT_OFF		0x40000
#define DMA_BC			0x1000
#define LT_BIT			0x10000000
#define FE_CONF_OFF		0x80000
#define FE_ADDR_OFF		0x100000

/* PCI Configuration Registers */
struct gfe_conf {
    union {
	    struct {
	    ushort      vend_id;                /* Vendor ID Register. Required */
	    ushort      dev_id;                 /* Device ID Register. Required */
	    ushort      cmd;                    /* Command Register. Required */
	    ushort      status;                 /* PCI status Register */
	    u_char      rev;                    /* Revision Register */
	    u_char      Prog_if;                /* Program interface Register */
	    u_char      subclass;               /* Sub-Class Register */
	    u_char      baseclass;              /* Base Class Register */
	    u_char      chahesize;              /* Cache line size */
	    u_char      latencytmr;             /* PCI bus latency timer */
	    ushort      reserve1;               /* Header type/BIST register-not used*/
	    uint        baseaddr_io;            /* I/O base address */
	    uint        baseaddr_mem;           /* Memory base address */
	    uint        reserve2[6];            /* base addresses, not used */
	    uint        bios_romaddr;           /* BIOS ROM Base Address */
	    u_char      nvram;                  /* PCI NVRAM extern EEPROM access */
	    u_char      reserve3[7];            /* Not used, reserved */
	    u_char      int_line;               /* INterrupt line register */
	    u_char      int_pin;                /* Interrupt pin REgister */
	    u_char      min_gnt;                /* Min_Gnt Register */
	    u_char      max_lat;                /* Max_Lat Register */
	    u_char      reset_ctrl;             /* PCI reset control Register */
	    u_char      reserve4[15];           /* not used, reserved. */
	    } conf1;

	    struct conf2 {
	    u_int	id;
	    u_int	cmd;
	    u_int	class;
	    u_int	oth[8];
	    } conf2;
	} conf;
};

        /* config values */
        /* cmd */
#define IOENABLE        0x01
#define MEMENABLE       0x02
#define MSTENABLE       0x04
#define SIGSYSERR       0x40
#define PARITYERR       0x80

	/* stat */
#define MHZ66ABLE       0x0020
#define DPARITY         0x0100
#define SIGTRGABT       0x0800
#define RCVTRGABT       0x1000
#define RCVMSTABT       0x2000

/* Adapter Host Registers structure */
struct gfe_addr {
     u_int        host_cmd;      /* Host command register */
     u_int        chan_parm;     /* channel parameter register */
     u_short      host_intr;     /* Host interrupt register */
     u_short      dio_addr;      /* TLAN internal DIO register addr. */
     u_char	dio_data[4];      /* DIO data register */
};


/* Transmit/Receive dma buffer */
/* CSTAT  Master_Swap */
#define FRAME_COMPLETE          0x0040
#define RX_ERROR          	0x0004
#define TXCSTAT_REQ             0x0030  
#define RXCSTAT_REQ             0x0030

#define MAX_DMA			10
#define TX_DMA_AV		0
#define RV_DMA_NOT		0
#define RDY			0x1
#define LIST_CNT		18
#define EOF_LIST		0
#define EOF_FAKE_LIST		0
#define TRX_NOT			0
#define END_BIT			0x80			/* Master_Swap */

struct dma_bd {
	u_int		next;		/* forward pointer */
	u_short		cstat;		/* CSTAT */
	u_short 	size;		/* frame size */

	/* buffer count and pointer */
	struct {
		u_int	count;
		u_int	addr;
	} list[MAX_DMA];
};


struct fake_dma_bd {
        struct dma_bd   dma_bd;
        struct fake_dma_bd	*next;
        struct mbuf     *info;          /* alloc'd buffer: inuse/free/end */
};


/* network statistics */
struct gfe_net_stat {
     int        tx_good;
     int        tx_underrun;
     int        tx_deferred;
     int        tx_multicollision;
     int        rx_good;
     int        rx_overrun;
     int        crc;
     int        codeerr;
     int        singlecollision;
     int        excesscollision;
     int        latecollision;
     int        carrierlost;
};

/* tlan ioctls */
#define GFEIOC_INFOGET		(('e' << 8) | 1)
#define GFEIOC_VMEDMASET		(('e' << 8) | 2)
#define GFEIOC_UNIVERSEREAD	(('e' << 8) | 3)
#define GFEIOC_UNIVERSEWRITE	(('e' << 8) | 4)
#define GFEIOC_PCIREAD		(('e' << 8) | 5)
#define GFEIOC_PCIWRITE		(('e' << 8) | 6)
#define GFEIOC_SETDMABUF		(('e' << 8) | 7)
#define GFEIOC_GETDMABUF		(('e' << 8) | 8)
#define GFEIOC_MEMREAD		(('e' << 8) | 9)
#define GFEIOC_MEMWRITE		(('e' << 8) | 10)
#define GFEIOC_EEPROM_READ	(('e' << 8) | 11)
#define GFEIOC_EEPROM_WRITE	(('e' << 8) | 12)
#define GFEIOC_GET_DBGFLAG	(('e' << 8) | 13)
#define GFEIOC_SET_DBGFLAG	(('e' << 8) | 14)
#define GFEIOC_PHYREAD		(('e' << 8) | 15)
#define GFEIOC_PHYWRITE		(('e' << 8) | 16)

#define DBG_SNOOP_OUT		1
#define DBG_PROMISC		2
#define DBG_PHY_WRAP		4
#define DBG_PRINT		8

#define MAX_COUNT		20

#define UINT	0
#define USHORT	1
#define UCHAR	2

#define DBLEN	88

/* structure for ioctl read and write */
typedef struct gfeioc_arg {
	int	count;
	int	type;
	u_int	*gio_addr;
	uint_t	val[MAX_COUNT];
} gfeioc_arg_t;
