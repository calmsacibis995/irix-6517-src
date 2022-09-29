/*
 * Copyright 1995 Silicon Graphics, Inc.  All rights reserved.
 *
 * Driver for Silicon Graphics IP32 PCI 10/100 fast ethernet controller
 * using Texas Instruments ThunderLAN ethernet control chip
 * (supports 10BaseT, 100BaseTX, 100BaseT4, and 100VG-AnyLAN protocols).
 */
#ident "$Revision: 1.10 $"

#if IP32 || IP30	/* for IP32(Moosehead) and IP30(SpeedRacer) */

#ifndef _IF_ECF_H
#define _IF_ECF_H

#ifdef _K32U64
#define _IRIX6_3	/* for IP32 IRIX 6.3 (bonsai) */
#endif

/******************************************************************************
 * Generic Definitions
 ******************************************************************************
 */
#define LWORD		__uint64_t	/* 64 bit */
#define DWORD		__uint32_t	/* 32 bit */
#define WORD		unsigned short	/* 16 bit */
#define BYTE		unsigned char	/* 8 bit */

#define DWMSK		0	/* dword offset mask */
#define WMSK		0x2	/* word offset mask */
#define BMSK		0x3	/* byte offset mask */
#define LSB_MSK		0xff
#define MSW_MSK		0xffff0000


/******************************************************************************
 * TLAN Control Information and Data Structures
 ******************************************************************************
 */

/*
 * transmit & receive mbufs saving queue
 */
typedef struct ecf_mbq_s {
    struct mbuf		*m_head;	/* first mbuf */
    struct mbuf		*m_tail;	/* last mbuf */
    uint_t		m_count;	/* number of mbufs */
} ecf_mbq_t;


/*
 * TI TLAN100 Tx/Rx Frame Frgament Structure
 */
typedef struct tlan_frame_frgm_s {
    volatile DWORD	f_bcnt;		/* fragment size */
    volatile DWORD	f_buf;		/* fragment addr */
} tlan_frgm_t;


/* tx may use all 10 fragments but rx will use only one fragment */
#define TLAN_TX_FRGMS	10
#define TLAN_RX_FRGMS	1

/*
 * Tx Frame Descriptor Structure
 */
typedef struct tlan_txfd_s {
    volatile DWORD	d_nxtf;		/* next frame */
    volatile WORD	d_fsize;	/* frame size */
    volatile WORD	d_cstat;	/* command & status */
    tlan_frgm_t		d_frgm[TLAN_TX_FRGMS];	/* fragment */

    struct tlan_txfd_s	*d_nxtd;	/* next tx frame descriptor */
    struct mbuf		*d_mbuf;	/* tx mbuf */
} tlan_txfd_t;

#define TLAN_TXDSZ	(24*4)

/*
 * Rx Frame Descriptor Structure
 */
typedef struct tlan_rxfd_s {
    volatile DWORD	d_nxtf;		/* next frame */
    volatile WORD	d_fsize;	/* frame size */
    volatile WORD	d_cstat;	/* command & status */
    tlan_frgm_t		d_frgm[TLAN_RX_FRGMS];	/* fragment */

    struct tlan_rxfd_s	*d_nxtd;	/* next rx frame descriptor */
    struct mbuf		*d_mbuf;	/* rx mbuf */
} tlan_rxfd_t;

#define TLAN_RXDSZ	(6*4)


/*
 * Tx/Rx Descriptors Ring Structure
 */
typedef struct tlan_dr_s {
    volatile void	*r_head;	/* first descriptor in the list */
    volatile void	*r_tail;	/* last descriptor in the list */
    volatile void	*r_actd;	/* first descriptor of active window */
    volatile void	*r_rotd;	/* first descriptor of used window */
    volatile void	*r_nxtd;	/* first descriptor of standby window */
    volatile void	*r_lastd;	/* last descriptor of standby window */
    volatile void	*r_freed;	/* first descriptor of free window */
    uint_t		r_ndesc;	/* number of free descriptors */
    uint_t		r_busy;		/* channel busy */
    int			r_drlock;	/* descriptor lock */
    lock_t		r_slock;	/* spin lock */
    mutex_t		r_mlock;	/* spin lock */
} tlan_dr_t;


/*
 * PHY status information
 */
typedef struct tlan_physts_s {
    WORD		sts;		/* phy status */
    uchar_t		brkn;		/* no. of broken link checking */
    uchar_t		led;		/* LED control byte */
} tlan_physts_t;


/*
 * Statistics information
 */
typedef struct tlan_stats_s {
    uint_t		goodtxfrm;
    uint_t		goodrxfrm;
    uint_t		txundrrun;
    uint_t		rxovrrun;
    uint_t		dfrtxfrm;
    uint_t		crcerr;
    uint_t		codeerr;
    uint_t		multcolli;
    uint_t		snglcolli;
    uint_t		excscolli;
    uint_t		latecolli;
    uint_t		nocarrier;
    uint_t		adptcmmit;
    uint_t		txdrp;
    uint_t		rxdrp;
} tlan_stats_t;


/*
 * tlan 100/10 tx information
 */
typedef struct tlan_ctrl_s {
    uint_t		phyid;		/* PHY ID */
    uint_t		phydev;		/* PHY device no. */
    uint_t		loopbk;		/* loopback/wire test */
    uint_t		speed;		/* fast 100mb */
    uint_t		fduplx;		/* full duplex */
    uint_t		rxeoc;		/* rx eoc count */
    uint_t		ready;		/* ready flag */
} tlan_ctrl_t;


/******************************************************************************
 * PCI Information
 ******************************************************************************
 */
typedef struct tlan_pci_s {
    __psunsigned_t	p_cfgaddr;	/* config space address */
    __psunsigned_t	p_baseaddr;	/* memory space address */
    pciio_device_id_t	p_dev;		/* device id */
    pciio_vendor_id_t	p_vndr;		/* vendor id */
    pciio_bus_t		p_bus;		/* bus number */
    pciio_slot_t	p_slot;		/* slot number */
    int			p_rev;		/* revision number */
    pciio_intr_t	p_intr;		/* interrupt handle */
} tlan_pci_t;


#define TLAN_TI_VNDID		0x104c	/* TI/Racore vendor ID */
#define TLAN_TI_DEVID		0x0500	/* TI/Racore device ID */
#define TLAN_CPQ_VNDID		0x0e11	/* COMPAQ vendor ID */
#define TLAN_CPQ_DEVID		0xae32	/* COMPAQ device ID */

#define PCI_LATENCY_16		0x10
#define PCI_LATENCY_64		0x30	/* Moosehead needs at least 0x20 */
#define PCI_CACHELINE_128_BYTE	0x20	/* 32*4 bytes */


/******************************************************************************
 * TI TLAN TNETE100 Register Definitions
 ******************************************************************************
*/

/* last fragment indicator */
#define F_LASTFRGM		0x80000000L

/* Receive CSTAT bits */
#define FRAME_COMPLETE          0x4000

#define TLAN_TX_FRAME_COMPLETE	0x4000
#define TLAN_RX_FRAME_ERROR	0x0400
#define TLAN_RX_FRAME_COMPLETE	0x4000

/* Transmit CSTAT bits */
#define CSTAT_REQ		0x3000
#define TXCSTAT_REQ             CSTAT_REQ
#define RXCSTAT_REQ             CSTAT_REQ

#define TLAN_TX			0x01
#define TLAN_RX			0x00

/******************************************************************************
 * TLAN Adaptor Host Register Definitions
 ******************************************************************************
 */
/* AHR_HOST_CMD_REG: */
#define AHR_HOST_CMD_REG	0x00

#define HOST_CMD_GO		0x80000000
#define HOST_CMD_STOP		0x40000000
#define HOST_CMD_ACK		0x20000000
#define HOST_CMD_CHSE		0x1fe00000
#define HOST_CMD_EOC		0x00100000
#define HOST_CMD_RT		0x00080000
#define HOST_CMD_NES		0x00040000

#define HOST_CMD_AD_RST		0x00008000
#define HOST_CMD_LD_TMR		0x00004000
#define HOST_CMD_LD_THR		0x00002000
#define HOST_CMD_REQ_INTR	0x00001000
#define HOST_CMD_INTR_OFF	0x00000800
#define HOST_CMD_INTR_ON	0x00000400
#define HOST_CMD_ACKS		0x000000ff

#define LD_TMR_200US		0x32
#define LD_TMR_4US		0x1

/* AHR_CH_PARM_REG: */
#define AHR_CH_PARM_REG		0x04

#define	TLAN_CHK_DATA_PART	0x1
#define	TLAN_CHK_ADDR_PART	0x2
#define	TLAN_CHK_MSTR_ABRT	0x3
#define	TLAN_CHK_TRGT_ABRT	0x4
#define	TLAN_CHK_LST_ERR	0x5
#define	TLAN_CHK_ACK_ERR	0x6
#define	TLAN_CHK_IOV_ERR	0x7
#define	TLAN_CHK_LD		0x00100000
#define	TLAN_CHK_RT		0x00080000
#define	TLAN_CHK_RW		0x00040000

/* AHR_HOST_INTR_REG: */
#define AHR_HOST_INTR_REG	0x0a

#define TLAN_HOST_INTR_VEC(x)	(((x) >> 5) & 0xff)
#define TLAN_HOST_INTR_TYPE(x)	(((x) >> 2) & 0x07)

#define NO_INTR			0x00	/* no interrupt */
#define TX_EOF			0x01	/* tx end-of-frame */
#define STAT_OVRFLOW		0x02	/* statistics overflow */
#define RX_EOF			0x03	/* rx end-of-frame */
#define DUMMY_INTR		0x04	/* created by Req_Int cmd bit */
#define TX_EOC			0x05	/* tx end-of-channel */
#define CHK_STATUS		0x06	/* check/network status */
#define RX_EOC			0x07	/* rx end-of-channel */

#define REQ_TX_INTRS		0x1	/* threshold  of # of tx eof intr */

#define ERR_ACK_INT             (HOST_CMD_ACK|HOST_CMD_EOC|HOST_CMD_RT)
#define ACK_STATISTICS_INT      (HOST_CMD_ACK|HOST_CMD_RT)

/* AHR_DIO_ADDR_REG: */
/* It will be used directly in big-endian format. 
#define AHR_DIO_ADDR_REG	0x08
*/
#define AHR_DIO_ADDR_REG	0x0a

#define DIO_ADDR_INC		0x8000	/* address increment */
#define DIO_RAM_ADDR_SEL	0x4000	/* RAM address select */
#define DIO_ADDR_MASK		0x3fff	/* mask of address select */

/* AHR_DIO_DATA_REG: */
#define AHR_DIO_DATA_REG	0x0c


/******************************************************************************
 * TLAN DIO Internal Register Definitions
 ******************************************************************************
 */
/* DIO Network Command & Status Registers */
#define DIO_NET_CMDSTS_REG	0x0000

#define DIO_NET_CMD_REG		0x0000
#define DIO_NET_SIO_REG		0x0001
#define DIO_NET_STS_REG		0x0002
#define DIO_NET_MSK_REG		0x0003

/* DIO_NET_CMD_REG: Net Command */
#define NRESET			0x80
#define NWRAP			0x40
#define CSF			0x20
#define CAF			0x10
#define NOBRX			0x08
#define DUPLEX			0x04
#define TRFRAM			0x02
#define TXPACE			0x01

/* NET_SIO: Net Serial I/O */
#define MIINTEN			0x80
#define ECLK			0x40
#define ETXEN			0x20
#define EDATA			0x10
#define NMRST			0x08
#define MCLK			0x04
#define MTXEN			0x02
#define MDATA			0x01

/* NET_STS: Net Status */
#define MIIRQ			0x80
#define HBEAT			0x40
#define TXSTOP			0x20
#define RXSTOP			0x10

/* NET_MASK: Net Error Mask */
#define MIIRQMSK		0x80
#define HBEATMSK		0x40
#define TXSTOPMSK		0x20
#define RXSTOPMSK		0x10


/* DIO Network Manufacture Test & Configuration Registers */
#define DIO_NET_TESTCFG_REG	0x0004

#define DIO_NET_CFG_REG		0x0004
#define DIO_MAN_TEST_REG	0x0006

/* NET_CFG: Net Error Mask */
#define RCLK_TEST		0x8000
#define TCLK_TEST		0x4000
#define BIT_RATE		0x2000
#define RX_CRC			0x1000
#define PEF			0x0800
#define ONE_FRAG		0x0400
#define ONE_CHN			0x0200
#define MTEST			0x0100
#define PHY_ENBL		0x0080

/* NET_CFG: MAC Select */
#define MACP_ONCHIP_PHY		0x0080	/* internal 10Mb PHY */
#define MAC_SELECT_MASK		0x007f
#define MACP_CSMACD		0x0000	/* 10/100Tx CSMA/CD */
#define MACP_100VG_CHNPRI	0x0001	/* 100VG channel priority */
#define MACP_100VG_CSTSPRI	0x0002	/* 100VG cstat priority */
#define MACP_100VG_EXT		0x0003

/* DIO Default PCI Parameter Registers */
#define DIO_NET_ID_REG		0x0008
#define DIO_NET_LGCR_REG	0x000c

/* DIO General Address Registers */
#define DIO_NET_ADDR_REG	0x0010
#define DIO_NET_ADDR0_REG	0x0010
#define DIO_NET_ADDR1_REG	0x0014
#define DIO_NET_ADDR2_REG	0x0018
#define DIO_NET_ADDR3_REG	0x001c
#define DIO_NET_ADDR4_REG	0x0020
#define DIO_NET_ADDR5_REG	0x0024

/* DIO Hash Address Registers */
#define DIO_NET_HASH0_REG	0x0028
#define DIO_NET_HASH1_REG	0x002C

/* DIO Tx/Rx Statistics Registers */
#define DIO_NETSTS_REG	0x0030

#define NET_STS_GOOD_FRAMES(x)	((x) & 0xffffff)
#define NET_STS_UNDER_RUNS(x)	(((x) >> 24) & 0xff)
#define NET_STS_OVER_RUNS(x)	(((x) >> 24) & 0xff)

/* DIO Network Error Statistics Registers */
#define NET_STS_TXDFR_FRAMES(x)	 ((x) & 0xffff)
#define NET_STS_CRCERR_FRAMES(x) (((x) >> 16) & 0xff)
#define NET_STS_CODERR_FRAMES(x) (((x) >> 24) & 0xff)

/* DIO Network Collision Statistics Registers */
#define NET_STS_SNGLCOLLI_FRAMES(x)	(((x) >> 16) & 0xffff)
#define NET_STS_MULTCOLLI_FRAMES(x)	((x) & 0xffff)

/* DIO Network Miscellaneous Statistics Registers */
#define NET_STS_EXCSCOLLIS(x)	((x) & 0xff) /* Excessive Collisions */
#define NET_STS_LATECOLLIS(x)	(((x) >> 8) & 0xff) /* Late Collisions */
#define NET_STS_CARRIER_LOST(x)	(((x) >> 16) & 0xff) /* Carrier Loss Errors */
#define NET_STS_ADPTR_COMMIT(x)	(((x) >> 24) & 0xff) /* Adapter Commit */

#define NET_TX_COMMIT_LEVEL(x)	((x) >> 4) /* Adapter PCI Commit Size */
#define TX_COMMIT_WHOLE_FRAME	0x70

#define NET_PHY_OPTION(x)	((x) & 0xf)
#define PHYOPT_HALF_DUPLEX	0x08
#define PHYOPT_LOOPBACK		0x04
#define PHYOPT_AUI		0x02
#define PHYOPT_UNDEFINED	0x01

/* DIO Rx Frame Size, Burst Size, LED Registers */
#define DIO_NET_LED_REG	0x0044
#define DIO_NET_BSIZE_REG	0x0045
#define DIO_NET_RX_MAXFRAME_REG	0x0046

#define LED_LNK			0x01
#define LED_ACT			0x10

#define NET_STS_LED(x)	((x) & 0xff)	/* LED Control Reg */
#define NET_STS_BURST_SIZE(x)	(((x) >> 8) & 0xff) /* Tx/Rx Burst Size Reg */
#define NET_STS_RX_MAXFRAME(x)	(((x) >> 16) & 0xffff) /* Rx Frame Size Reg */

#define NET_TX_BURSTSZ(x)	(((x) >> 4) 0xf)
#define NET_RX_BURSTSZ(x)	((x) & 0xf)

/* Tx/Rx burst size:
 * 0: 16 bytes     1: 32 bytes     2: 64 bytes (default)    3: 128 bytes
 * 4: 256 bytes    5-7: 512 bytes  8-f: reserved
 */
#define TX_BURST_16		0x00
#define TX_BURST_32		0x01
#define TX_BURST_64		0x02
#define TX_BURST_128		0x03
#define TX_BURST_256		0x04
#define TX_BURST_512		0x05
#define RX_BURST_16		0x00
#define RX_BURST_32		0x10
#define RX_BURST_64		0x20
#define RX_BURST_128		0x30
#define RX_BURST_256		0x40
#define RX_BURST_512		0x50

/******************************************************************************
 * TLAN MII Register Definitions
 ******************************************************************************
 */
#define MII_READ		0x10	/* MII read operation code */
#define MII_WRITE		0x01	/* MII write operation code */
#define EE_READ			0x01	/* EEPROM read operation code */
#define EE_WRITE		0x00	/* EEPROM write operation code */

/* PHY Generic Control Register */
#define MII_GEN_CTRL_REG	0x00

#define PHYRESET		0x8000
#define LOOPBK			0x4000
#define FAST100Mb		0x2000
#define AUTOENBL		0x1000
#define PDOWN			0x0800
#define ISOLATE			0x0400
#define AUTORSRT		0x0200
#define FDUPLEX			0x0100
#define COLTEST			0x0080

/* PHY Generic Status Register */
#define MII_GEN_STS_REG		0x01

#define GEN_STS_100TX_T4	0x8000
#define GEN_STS_100TX_FD	0x4000
#define GEN_STS_100TX_HD	0x2000
#define GEN_STS_10TX_FD		0x1000
#define GEN_STS_10TX_HD		0x0800
#define GEN_STS_AN_CMPLT	0x0020
#define GEN_STS_RFLT		0x0010        
#define GEN_STS_AN_ABLY		0x0008
#define GEN_STS_LINK		0x0004
#define GEN_STS_JABBER		0x0002
#define GEN_STS_EXTCAP		0x0001
#define GEN_STS_NO_LINK		0x0004

/* PHY Generic identifier (high) */
#define MII_GEN_ID_HI_REG	0x02

/* PHY Generic identifier (low) */
#define MII_GEN_ID_LO_REG	0x03

/* PHY Auto-neg. advertisement */
#define MII_AN_ADVS_REG		0x04
#define T_FDUPLEX		0x40
#define T_HDUPLEX		0x20

#define AN_ABILITY_MSK		0x1fe0
#define AN_100TX_T4		0x0200
#define AN_100TX_FD		0x0100
#define AN_100TX_HD		0x0080
#define AN_10TX_FD		0x0040
#define AN_10TX_HD		0x0020


/* Auto-neg. Link-Partner ability */
#define MII_AN_LKPTNR_REG	0x05

/* Auto-neg. expansion */
#define MII_AN_EXPN_REG		0X06

/* TLAN PHY identifier */
#define MII_TLPHY_ID_REG	0x10

/* TLAN PHY control register */
#define MII_TLPHY_CTRL_REG	0x11

#define PHY_CTRL_IGLINK		0x8000  
#define PHY_CTRL_INTEN		0x0002
#define PHY_CTRL_TINT		0x0001

/* ICS PHY quickpoll detailed status register */
#define MII_QPOLL_STS_REG	0x11

#define MII_QPOLL_FAST100Mb	0x8000
#define MII_QPOLL_FDUPLEX	0x4000

/* TLAN PHY status register */
#define MII_TLPHY_STS_REG	0x12

#define MINT			0x8000
#define PHOK			0x4000
#define POLOK			0x2000
#define TPENERGY		0x1000


#define MAX_MIISYNCS		32	/* maximum MII sync cycles */

/* user option bit flags */
#define	OPT_NOMGTPHY		0x0002	/* use non-MII managed PHY */
#define OPT_AUI			0x0004	/* use AUI on TLAN internal PHY */
#define OPT_NOAUTODETECT	0x0008	/* use first PHY found; no link test */

/* PHY descriptor bit flags */
#define PHY_MII_IF		0x01	/* PHY has an MII interface */
#define PHY_10TX		0x02	/* PHY is a 10Tx only */
#define PHY_NS_100TX		0x04	/* PHY is a National Semi. 10/100Tx */
#define PHY_TI_100VG		0x08	/* PHY is a 100VG-AnyLAN PHY */
#define PHY_TI_INTRL		0x10	/* PHY is the internal PHY */
#define PHY_BIT_LEVEL		0x20	/* PHY is a bit-level mode PHY */
#define PHY_MII_POLL		0x40	/* PHY requires MII poll */
#define PHY_FSDEL		0x80	/* PHY supports a fast SDEL */

/* generic PHY ID */
#define TLAN_PHYID_TI_INTRL	0x40005010	/* TI internal 10Tx */
#define TLAN_PHYID_TI_100VG	0x40005020	/* TI 100VG */
#define TLAN_PHYID_NS_100TX	0x20005c00	/* National Semi. DP83840 */
#define TLAN_PHYID_ICS_100TX	0x0015f420	/* ICS 1890 */

#define TLAN_MAX_PHYS_INDEX	32	/* maximum PHY devices */

/******************************************************************************
 * 100VG MII Register Definitions
 ******************************************************************************
 */
/* TLAN PHY identifier */
#define VGPHY_ID_REG	0x10

/* TLAN PHY control register */
#define VGPHY_CTRL_REG	0x11

#define TRIDLE	0x0010
#define TRFAIL	0x0020

/* TLAN PHY status register */
#define VGPHY_STS_REG	0x12

#define RETRAIN	0x0040
#define LSTATE	0x0030
#define TRFRTO	0x0008
#define RTRIDL	0x0004

/******************************************************************************
 * EEPROM Constants
 ******************************************************************************
 */
#define EACOMMIT    0x70
#define EBURST      0x71
#define ETLAN_CTL   0x72
#define EINTPACE    0x73
#define ELATTIME    0x74
#define EMAXFRLSB   0x75
#define EMAXFRMSB   0x76
#define EMINFRLSB   0x77
#define EMINFRMSB   0x78
#define ETESTMODE   0x79
#define EFRTYPE     0x7A
#define ETSTTYPE    0x7B
#define EPIPEDEP    0x7C
#define EPHYID      0x7D
#define EMACLSB     0x7E
#define EMACMSB     0x7F
 
/******************************************************************************
 * miscellaneous constants
 ******************************************************************************
 */
#define TRUE    1
#define FALSE   0


/******************************************************************************
 * TLAN_PCI_CFG macros definitions
 ******************************************************************************
 */
/*
 * Description:
 * read/write TLAN PCI Configuration Register (8/16/32-bit)
 */

#if IP32
#define PCI_CFG_ADDR     ((volatile DWORD *)(PHYS_TO_K1(PCI_CONFIG_ADDR)))
#define PCI_CFG_DATA     ((volatile DWORD *)(PHYS_TO_K1(PCI_CONFIG_DATA)))

#define PCI_CFG_RD(pci, reg) \
	(*(PCI_CFG_ADDR) = (DWORD)((pci)->p_cfgaddr + ((reg) & 0xfc)),\
	(DWORD)*(PCI_CFG_DATA))

#define PCI_CFG_WR(pci, reg, val) \
	(*(PCI_CFG_ADDR) = (DWORD)((pci)->p_cfgaddr + ((reg) & 0xfc)),\
	 *(PCI_CFG_DATA) = (val))
#else
#define PCI_CFG_RD(pci, reg) \
	(DWORD)PCI_INW(((pci)->p_cfgaddr + ((reg) & 0xfc)))

#define PCI_CFG_WR(pci, reg, val) \
	PCI_OUTW(((pci)->p_cfgaddr + ((reg) & 0xfc)), (val))
#endif


/******************************************************************************
 * TLAN_AHR macros definitions
 ******************************************************************************
 */
/*
 * TLAN_AHR_RD_*()
 * TLAN_AHR_WR_*()
 *
 * Description:
 * read/write TLAN Adapter Host Register (16/32-bit)
 */

#define TLAN_AHR_RD_DWORD(ei, reg, msk) \
	(pciio_pio_read32((volatile uint32_t *)((ei)->ei_io + ((reg) & 0xfc) + ((msk) - ((reg) & (msk))))))

#define TLAN_AHR_RD_WORD(ei, reg, msk) \
	(pciio_pio_read16((volatile uint16_t *)((ei)->ei_io + ((reg) & 0xfc) + ((msk) - ((reg) & (msk))))))

#define TLAN_AHR_WR_DWORD(ei, reg, val, msk) \
	(pciio_pio_write32((uint32_t)(val), (volatile uint32_t *)((ei)->ei_io + (((reg) & 0xfc) + ((msk) - ((reg) & (msk)))))))

#define TLAN_AHR_WR_WORD(ei, reg, val, msk) \
	(pciio_pio_write16((uint16_t)(val), (volatile uint16_t *)((ei)->ei_io + (((reg) & 0xfc) + ((msk) - ((reg) & (msk)))))))

#define TLAN_AHR_SET_DWORD(ei, reg, b, msk) \
	TLAN_AHR_WR_DWORD((ei), (reg), (TLAN_AHR_RD_DWORD((ei), (reg), msk) | (b)), msk)

#define TLAN_AHR_CLR_DWORD(ei, reg, b, msk) \
	TLAN_AHR_WR_DWORD((ei), (reg), (TLAN_AHR_RD_DWORD((ei), (reg), msk) & ~(b)))

#define TLAN_RESET(ei) \
	TLAN_AHR_WR_DWORD((ei), AHR_HOST_CMD_REG, HOST_CMD_AD_RST, DWMSK)


/******************************************************************************
 * TLAN_DIO macros definitions
 ******************************************************************************
 */
/*
 * TLAN_DIO_RD_*()
 * TLAN_DIO_WR_*()
 *
 * Description:
 * indirectly read/write TLAN Internal Register (8/16/32-bit)
 */
#define TLAN_DIO_RD_BYTE(ei, reg, msk) \
	(pciio_pio_write16((uint16_t)(reg), (volatile uint16_t *)((ei)->ei_io + AHR_DIO_ADDR_REG)),\
	 pciio_pio_read8((volatile uint8_t *)((ei)->ei_io + AHR_DIO_DATA_REG + ((msk) - ((reg) & (msk))))))

#define TLAN_DIO_RD_DWORD(ei, reg, msk) \
	(pciio_pio_write16((uint16_t)(reg), (volatile uint16_t *)((ei)->ei_io + AHR_DIO_ADDR_REG)),\
	 pciio_pio_read32((volatile uint32_t *)((ei)->ei_io + AHR_DIO_DATA_REG + ((msk) - ((reg) & (msk))))))

#define TLAN_DIO_WR_BYTE(ei, reg, val, msk) \
	(pciio_pio_write16((uint16_t)(reg), (volatile uint16_t *)((ei)->ei_io + AHR_DIO_ADDR_REG)),\
	 pciio_pio_write8((uint8_t)(val), (volatile uint8_t *)((ei)->ei_io + AHR_DIO_DATA_REG + ((msk) - ((reg) & (msk))))))
	
#define TLAN_DIO_WR_DWORD(ei, reg, val, msk) \
	(pciio_pio_write16((uint16_t)(reg), (volatile uint16_t *)((ei)->ei_io + AHR_DIO_ADDR_REG)),\
	 pciio_pio_write32((uint32_t)(val), (volatile uint32_t *)((ei)->ei_io + AHR_DIO_DATA_REG + ((msk) - ((reg) & (msk))))))
	
#define TLAN_DIO_SET_BYTE(ei, reg, b, msk) \
	(pciio_pio_write16((uint16_t)(reg), (volatile uint16_t *)((ei)->ei_io + AHR_DIO_ADDR_REG)),\
	 pciio_pio_write8((uint8_t)(pciio_pio_read8((volatile uint8_t *)((ei)->ei_io + AHR_DIO_DATA_REG + ((msk) - ((reg) & (msk))))) | (b)), (volatile uint8_t *)((ei)->ei_io + AHR_DIO_DATA_REG + ((msk) - ((reg) & (msk))))))

#define TLAN_DIO_CLR_BYTE(ei, reg, b, msk) \
	(pciio_pio_write16((uint16_t)(reg), (volatile uint16_t *)((ei)->ei_io + AHR_DIO_ADDR_REG)),\
	 pciio_pio_write8((uint8_t)(pciio_pio_read8((volatile uint8_t *)((ei)->ei_io + AHR_DIO_DATA_REG + ((msk) - ((reg) & (msk))))) & ~(b)), (volatile uint8_t *)((ei)->ei_io + AHR_DIO_DATA_REG + ((msk) - ((reg) & (msk))))))

#define TLAN_DIO_RD_DATA_DWORD(ei) \
	 pciio_pio_read32((volatile uint32_t *)((ei)->ei_io + AHR_DIO_DATA_REG))
	 

/******************************************************************************
 * TLAN_NETSIO macros definitions
 ******************************************************************************
 */
/*
 * TLAN_NETSIO_RD()
 * TLAN_NETSIO_WR()
 * TLAN_NETSIO_SET()
 * TLAN_NETSIO_CLR()
 * TLAN_NETSIO_CHK()
 *
 * Description:
 * indirectly read/write TLAN Internal Register NETSIO (8-bit)
 */
#define TLAN_NETSIO_RD(ei) \
	TLAN_DIO_RD_BYTE((ei), DIO_NET_SIO_REG, BMSK)

#define TLAN_NETSIO_WR(ei, val) \
	TLAN_DIO_WR_BYTE((ei), DIO_NET_SIO_REG, (val), BMSK)

#define TLAN_NETSIO_SET(ei, val) \
	TLAN_DIO_SET_BYTE((ei), DIO_NET_SIO_REG, (val), BMSK)

#define TLAN_NETSIO_CLR(ei, val) \
	TLAN_DIO_CLR_BYTE((ei), DIO_NET_SIO_REG, (val), BMSK)

#define TLAN_NETSIO_CHK(ei, val) \
	(TLAN_DIO_RD_BYTE((ei), DIO_NET_SIO_REG, BMSK) & (val))

#define TLAN_NETSIO_MCLK(ei, s) \
	(TLAN_NETSIO_WR(ei, ((s) &= ~MCLK)), TLAN_NETSIO_WR(ei, ((s) |= MCLK)))

#define TLAN_LED_SET(ei, val) \
	TLAN_DIO_SET_BYTE((ei), DIO_LED_REG, (val), BMSK)

#define TLAN_LED_CLR(ei, val) \
	TLAN_DIO_CLR_BYTE((ei), DIO_LED_REG, (val), BMSK)


/******************************************************************************
 * TLAN_MII macros definitions
 ******************************************************************************
 */
#define TLAN_MII_RD(ei, dev, reg)	\
	tlan_mii_io(MII_READ, (ei), (dev), (reg), 0)

#define TLAN_MII_WR(ei, dev, reg, data)	\
	tlan_mii_io(MII_WRITE, (ei), (dev), (reg), (data))

#define TLAN_MII_ISOLATE_PHY(ei, phy) \
	TLAN_MII_WR((ei), (phy), MII_GEN_CTRL_REG, (PDOWN | LOOPBK | ISOLATE))
	
#define TLAN_MII_MDCLK(ei, sio, val) \
	((sio)=(val), TLAN_NETSIO_WR((ei),(sio)), TLAN_NETSIO_MCLK((ei),(sio)))


/******************************************************************************
 * TLAN_EEPROM macros definitions
 ******************************************************************************
 */
/*
 * TLAN_EEPROM_RD()
 * TLAN_EEPROM_WR()
 *
 * Description:
 * indirectly read/write TLAN Internal Register NETSIO (8-bit)
 */
#define TLAN_EEPROM_RD(ei, i, ack) \
	  tlan_eeprom_io(EE_READ, ei, (i), 0, 1, ack)

#define TLAN_EEPROM_WR(ei, i, data, n) \
	  tlan_eeprom_io(EE_WRITE, ei, (i), data, n, &n)

#define TLAN_NETSIO_EDCLK(ei) \
	{TLAN_NETSIO_SET(ei, ECLK); TLAN_NETSIO_CLR(ei, ECLK);}

/*
 * TLAN_EEPROM_START_SEQ()
 * TLAN_EEPROM_STOP_SEQ()
 * TLAN_EEPROM_START_ACCESS_SEQ()
 * TLAN_EEPROM_STOP_ACCESS_SEQ()
 *
 * Description:
 */
#define TLAN_EEPROM_START_SEQ(ei) \
	{ TLAN_NETSIO_SET(ei, ECLK);  TLAN_NETSIO_SET(ei, EDATA); \
	  TLAN_NETSIO_SET(ei, ETXEN); TLAN_NETSIO_CLR(ei, EDATA); \
	  TLAN_NETSIO_CLR(ei, ECLK); }

#define TLAN_EEPROM_STOP_SEQ(ei) \
	{ TLAN_NETSIO_SET(ei, ETXEN); TLAN_NETSIO_CLR(ei, EDATA); \
	  TLAN_NETSIO_SET(ei, ECLK);  TLAN_NETSIO_SET(ei, ETXEN); \
	  TLAN_NETSIO_SET(ei, EDATA); TLAN_NETSIO_CLR(ei, ETXEN); }

#define TLAN_EEPROM_START_ACCESS_SEQ(ei) \
	{ TLAN_NETSIO_SET(ei, ETXEN); TLAN_NETSIO_SET(ei, ECLK);  \
	  TLAN_NETSIO_SET(ei, EDATA); TLAN_NETSIO_SET(ei, ETXEN); \
	  TLAN_NETSIO_CLR(ei, EDATA); TLAN_NETSIO_CLR(ei, ECLK);  }

#define TLAN_EEPROM_STOP_ACCESS_SEQ(ei) \
	{ TLAN_NETSIO_CLR(ei, EDATA); TLAN_NETSIO_SET(ei, ECLK);  \
	  TLAN_NETSIO_SET(ei, EDATA); TLAN_NETSIO_CLR(ei, ETXEN); }

#define TLAN_EEPROM_EADDR	0x83	/* burned in ethernet address */


/******************************************************************************
 * PCI pciio Memory Address Conversion macros
 ******************************************************************************
 */
#if IP32
#ifdef _IRIX6_3
#define PCIIO_CMD		PCIIO_PIOMAP_BIGEND
#define PCIIO_DATA		PCIIO_PIOMAP_LITTLEEND
#else
#define PCIIO_CMD		PCIIO_WORD_VALUES
#define PCIIO_DATA		PCIIO_BYTE_STREAM
#endif
#else
#define PCIIO_CMD		PCIIO_DMA_CMD
#define PCIIO_DATA		PCIIO_DMA_DATA
#endif
#define KVTOPCIIO(ei, addr, flag) \
	pciio_dmatrans_addr((ei)->ei_vhdl, 0, kvtophys((caddr_t)(addr)), \
		sizeof(int), (flag))


/******************************************************************************
 * TLAN Tx/Rx macros
 ******************************************************************************
 */
#if IP32
#define PCI_FLUSH_WBUF(x)	(*(DWORD *)PHYS_TO_K1(PCI_FLUSH_W) = 1)
#else
#define PCI_FLUSH_WBUF(x)
#endif

#define TLAN_TX_GO(ei, list) \
	{ \
	PCI_FLUSH_WBUF(ei); \
	TLAN_AHR_WR_DWORD((ei), AHR_CH_PARM_REG, \
		KVTOPCIIO(ei, list, PCIIO_CMD), DWMSK); \
	TLAN_AHR_WR_DWORD((ei), AHR_HOST_CMD_REG, \
		(HOST_CMD_EOC | HOST_CMD_GO), DWMSK); \
	}

#define TLAN_RX_GO(ei, list) \
	{ \
	TLAN_AHR_WR_DWORD((ei), AHR_CH_PARM_REG, \
		KVTOPCIIO(ei, list, PCIIO_CMD), DWMSK); \
	TLAN_AHR_WR_DWORD((ei), AHR_HOST_CMD_REG, \
		(HOST_CMD_GO | HOST_CMD_RT), DWMSK); \
	}


/******************************************************************************
 * Multicast macros
 ******************************************************************************
 */
/* multicast logical address filter
 */
typedef struct lafilter_s {
	DWORD	laf_vec[2];	/* for hash address register */
	DWORD	laf_coll;	/* laf hash collisions */
} lafilter_t;

#define LAF_TSTBIT(ei, bit) \
	((ei)->ei_laf.laf_vec[((bit) >> 5)] & (1L << ((bit) & 0x1f)))
#define LAF_SETBIT(ei, bit) \
	((ei)->ei_laf.laf_vec[((bit) >> 5)] |= (1L << ((bit) & 0x1f)))
#define LAF_CLRBIT(ei, bit) \
	((ei)->ei_laf.laf_vec[((bit) >> 5)] &= ~(1L << ((bit) & 0x1f)))
#define LAF_SETALL(ei) \
	((ei)->ei_laf.laf_vec[1] = (ei)->ei_laf.laf_vec[0] = 0xffffffffL)


/******************************************************************************
 * 
 ******************************************************************************
 */

/* maximum & minimum packet sizes
 */
#define EHDR_LEN	14
#define MAX_TPKT	(ETHERMTU+EHDR_LEN)
#define MIN_TPKT	(ETHERMIN+EHDR_LEN)
#define MAX_RPKT	(ETHERMTU+EHDR_LEN)
#define MIN_RPKT	(ETHERMIN+EHDR_LEN)
#define MAX_RDAT	ETHERMTU

/* Receive buffer structure and initializer.
 */
struct rcvbuf {
	struct etherbufhead	ebh;
	char			data[MAX_RDAT];
};
#define RCVBUF_SIZE	(sizeof(struct rcvbuf))


/******************************************************************************
 *  ecf infor structure
 ******************************************************************************
 */

/*
 * Each interface is represented by a network interface structure, ec_info,
 * which the routing code uses to locate the interface.
 */
typedef struct ecf_info_s {
    struct etherif	ei_eif;		/* common ethernet interface */

    tlan_pci_t		ei_pci;		/* pci config reg */
    tlan_ctrl_t		ei_tlan;	/* tlan phy/mii ctrl information */
    tlan_dr_t		ei_txdr;	/* transmit descriptor ring */
    tlan_dr_t		ei_rxdr;	/* receive descriptor ring */
    tlan_stats_t	ei_stats;	/* statistics information */
    tlan_physts_t	ei_physts;	/* PHY status */

    int			ei_flags;	/* flag bits */
    __psunsigned_t	ei_io;		/* I/O base addr */
    struct etheraddr    ei_ea;		/* burned in ethernet address */

    int			ei_rxpks;	/* no. of pending input packets */
    int			ei_txpks;	/* no. of pending output packets */
    int			ei_netype;	/* type of network */

    int			ei_unicast_cnt;	/* unicast to self counter */
    uint_t		ei_unicast_id;	/* unicast watchdog timeout id */
    uint_t		ei_nmulti;	/* count of multicast enabled */
    lafilter_t		ei_laf;		/* multicast logical address filter */
    uint_t		ei_lafcoll;	/* laf hash collisions */
#ifdef _IRIX6_3
    struct ecf_info_s	*ei_nxtif;	/* next ecf info */
#else
    vertex_hdl_t	ei_ecfvhdl;	/* ecf vertex */
#endif
    vertex_hdl_t	ei_vhdl;	/* PCI connect vertex */
    device_desc_t	ei_devdesc;	/* device descriptor */
lock_t              lock;

} ecf_info_t;

/* ei_flag bits */
#define EIF_PSENABLED	0x0001		/* RSVP packet scheduling enabled */

#ifdef _IRIX6_3
static ecf_info_t *ecf_list = NULL;	/* pointers to ecf adapters */
#endif

#define ei_ac		ei_eif.eif_arpcom	/* common arp stuff */
#define ei_if		ei_ac.ac_if		/* network-visible interface */
#define ei_curaddr	ei_ac.ac_enaddr		/* current ethernet address */
#define ei_rawif	ei_eif.eif_rawif	/* raw domain interface */
#define ei_lostintrs	ei_eif.eif_lostintrs	/* count lost interrupts */
#define ei_resets	ei_eif.eif_resets	/* count board resets */
#define ei_sick		ei_eif.eif_sick		/* 2 => reset on next bark */
#define eiftoei(eif)	((ecf_info_t *)(eif)->eif_private)
#define ei_unit		ei_ac.ac_if.if_unit
#define ei_name		ei_ac.ac_if.if_name
#define ei_enaddr	ei_ac.ac_enaddr		/* current ethernet address */

#define IF_ECF	"ec"

/* definitions for cache operations.
 */
#if IP32
/*extern void	__vm_dcache_inval(caddr_t, int);
extern void	__vm_dcache_wb_inval(caddr_t, int);
#ifndef _IRIX6_3
extern void	__dcache_wb(caddr_t, int);
#endif*/

#define DCACHE_INVAL(a, b)	__vm_dcache_inval(a, b)
#define DCACHE_WB(a, b)		__dcache_wb(a, b)
#define DCACHE_WBINVAL(a, b)	__vm_dcache_wb_inval(a, b)
#elif IP30
#define DCACHE_INVAL(a, b)	dki_dcache_inval(a, b)
#ifdef HEART_COHERENCY_WAR
#define DCACHE_WB(a, b)		heart_dcache_wb_inval(a, b)
#define DCACHE_WBINVAL(a, b)	heart_dcache_wb_inval(a, b)
#else
#define DCACHE_WB(a, b)		dki_dcache_wb(a, b)
#define DCACHE_WBINVAL(a, b)	dki_dcache_wbinval(a, b)
#endif
#endif

#define ETHER_HDRLEN	(sizeof (struct ether_header))
#define SPACE_FOR_EHDR	(EHDR_LEN + RAW_HDRPAD(EHDR_LEN))
#define ECFDOG		(2*IFNET_SLOWHZ)	/* watchdog duration in sec */

#define IFF_DBG_PRINT(a) if (ei->ei_if.if_flags & IFF_DEBUG) printf a

#ifdef _IRIX6_3
#define INV_ETHER_ECF	INV_ETHER_EF
#endif

#define ECF_LOCK(lck)	\
	(!(lck) && (atomicAddInt(&(lck), 1) == 1 || !atomicAddInt(&(lck), -1)))

#define ECF_UNLOCK(lck)	\
	(void)atomicAddInt(&(lck), -1)

#endif /* _IF_ECF_H */
#endif /* IP32 */
