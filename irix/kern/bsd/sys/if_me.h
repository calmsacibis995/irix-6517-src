/*
 * Moosehead internal fast ethernet interface
 *
 * Copyright 1995, Silicon Graphics, Inc.  All rights reserved.
 */

#ifndef SYS_MACE_ETHER_H
#define SYS_MACE_ETHER_H

#define MACE_ETHER_ADDRESS	0xBF280000

/* Ethernet interface registers */
struct mac110 {
	__uint32_t	__mcpd;
	__uint32_t	mac_control;		/* MAC mode setup */
	union me_isr {
		__uint64_t	lsr;
		struct {
		    __uint32_t	__ispd;
		    __uint32_t	sr;		/* Interrupt status */
		} s;
	} isr;
#define interrupt_status isr.s.sr
	__uint32_t	__dcpd;
	__uint32_t	dma_control;		/* DMA control */
	__uint32_t	__trpd;
	__uint32_t	timer;			/* Timer */
	__uint32_t	__tapd;
	__uint32_t 	transmit_alias;		/* Transmit interrupt (WO) */
	__uint32_t	__rapd;
	__uint32_t	receive_alias;		/* Receive interrupt (WO) */
	struct {
		__uint32_t	_tpd;
		union me_tir_u {
		    __uint32_t	td;
		    struct me_tir {
			ushort_t    rptr,	/* ring buffer read pointer */
				    wptr;	/* ring buffer write pointer */
		    } bd;
		} u;
	} tx_info;
#define tx_ring_rptr tx_info.u.bd.rptr
#define tx_ring_wptr tx_info.u.bd.wptr
#define tx_ring_regs tx_info.u.td
	__uint64_t	pad1;
	struct {
		__uint32_t	_rpd1;
		union me_rir_u {
		    __uint32_t	rd;
		    struct me_rir {
			u_char     _rpd2,
				    wptr,	/* MCL fifo write pointer */
				    rptr,	/* MCL fifo read pointer */
				    depth;	/* MCL fifo depth */
		    } bd;
		} u;
	} rx_info;
#define rx_fifo_rptr rx_info.u.bd.rptr
#define rx_fifo_wptr rx_info.u.bd.wptr
#define rx_fifo_depth rx_info.u.bd.depth
#define rx_fifo_regs rx_info.u.rd
	__uint64_t	pad2;
	__uint64_t	pad3;
	union {
		__uint64_t	sintr_request;
		__uint64_t	last_transmit_vector;
	} irltv;
	__uint32_t	__ppd1;
	__uint32_t	phy_dataio;		/* PHY data r/w */
	__uint32_t	__ppd2;
	__uint32_t	phy_address;		/* PHY fadr & radr */
	__uint32_t	__ppd3;
	__uint32_t	phy_read_start;		/* PHY read start */
	__uint32_t	__ppd4;
	__uint32_t	backoff;		/* Backoff LFSR */

	/* 64-bit DP-RAM locations in MACE */
	__uint64_t	msgqueue[4];		/* read-only diag */
	__uint64_t	physaddr;		/* Physical address */
	__uint64_t	secphysaddr;		/* Physical address #2 */
	__uint64_t	mlaf;			/* Multicast filter */
	__uint64_t	tx_ring_base;		/* Transmit ring base */
	__uint64_t	tx1_cmd_hdr;		/* read-only diag */
	__uint64_t	tx1_cat_ptr1;		/* read-only diag */
	__uint64_t	tx1_cat_ptr2;		/* read-only diag */
	__uint64_t	tx1_cat_ptr3;		/* read-only diag */
	__uint64_t	tx2_cmd_hdr;		/* read-only diag */
	__uint64_t	tx2_cat_ptr1;		/* read-only diag */
	__uint64_t	tx2_cat_ptr2;		/* read-only diag */
	__uint64_t	tx2_cat_ptr3;		/* read-only diag */

	/* 64-bit DP-RAM FIFO locations in MACE */
	__uint32_t	__rpd;
	__uint32_t	rx_fifo;
	__uint64_t	reserved5[31];
};

/* Multicast Logical Address Filter Macros */
#define LAF_TSTBIT(laf, bit) \
	((laf) & (1LL << ((bit) & 0x3F)))
#define LAF_SETBIT(laf, bit) \
	((laf) |= (1LL << ((bit) & 0x3F)))
#define LAF_CLRBIT(laf, bit) \
	((laf) &= ~(1LL << ((bit) & 0x3F)))

/* MAC Control Register */
#define MAC_RESET		0x0001
#define MAC_FULL_DUPLEX		0x0002
#define MAC_LOOPBACK		0x0004
#define MAC_100MBIT		0x0008
#define MAC_SIA			0x0010
#define MAC_FILTER		0x0060
#define		MAC_PHYSICAL		0x0000
#define		MAC_NORMAL		0x0020
#define		MAC_ALL_MULTICAST	0x0040
#define		MAC_PROMISCUOUS		0x0060
#define	MAC_LINKF		0x0080
#define MAC_IPG			0x1FFFF00
#define MAC_IPGT_SHIFT		8
#define MAC_IPGR1_SHIFT		15
#define MAC_IPGR2_SHIFT		22
#define MAC_DEFAULT_IPG		0x54A9500	/* 21, 21, 21 */
#define MAC_REV_SHIFT           29

/* Interrupt Status Register */
#define INTR_TX_DMA_REQ		0x01
#define INTR_TX_PKT_REQ		0x02
#define INTR_TX_LINK_FAIL	0x04
#define INTR_TX_MEMORY_ERROR	0x08
#define INTR_TX_ABORTED		0x10
#define ETHER_TX_ERRORS		(INTR_TX_LINK_FAIL | \
				 INTR_TX_MEMORY_ERROR | \
				 INTR_TX_ABORTED)
#define INTR_RX_DMA_REQ		0x20
#define INTR_RX_MSGS_UNDERFLOW	0x40
#define INTR_RX_FIFO_OVERFLOW	0x80
#define ETHER_RX_ERRORS		(INTR_RX_MSGS_UNDERFLOW | \
				 INTR_RX_FIFO_OVERFLOW)

/* DMA control register */
#define	DMA_TX_INT_EN		0x0001
#define	DMA_TX_DMA_EN		0x0002
#define	DMA_TX_RINGMSK		0x000c
#define   DMA_TX_8K			0x0000
#define   DMA_TX_16K			0x0004
#define   DMA_TX_32K			0x0008
#define   DMA_TX_64K			0x000c
#define	DMA_TX_RINGMSK_SHIFT	2
#define	DMA_RX_THRSHD		0x01f0
#define	DMA_RX_INT_EN		0x0200
#define	DMA_RX_RUNTS_EN		0x0400
#define	DMA_RX_GATHER_EN	0x0800
#define	DMA_RX_OFFSET		0x7000
#define	DMA_RX_OFFSET_SHIFT	12
#define	DMA_RX_DMA_EN		0x8000

/* Phy MDIO interface busy flag */
#define MDIO_BUSY		0x10000

/* Statistics vector format */
typedef __uint64_t		statistics_vector_t;

/* Receive message cluster FIFO control */
#define ETHER_RX_DMA_ENABLE	0x8000
#define ETHER_RX_DMA_OFFSET	0x7000
#define ETHER_RX_OFFSET_SHIFT	12
#define ETHER_RX_MERGE_ENABLE	0x0800
#define ETHER_RX_RUNT_ENABLE	0x0400
#define ETHER_RX_INTR_ENABLE	0x0200
#define ETHER_RX_THRESHOLD	0x01F0
#define ETHER_RX_THRESH_SHIFT	4

/* Transmit message cluster FIFO control */
#define ETHER_TX_RING_SIZE	0x000C
#define ETHER_TX_DMA_ENABLE	0x0002
#define ETHER_TX_INTR_ENABLE	0x0001

/* Receive status vector */
#define RX_VEC_LENGTH                   0x00007FFF
#define RX_VEC_CODE_VIOLATION           0x00010000
#define RX_VEC_DRIBBLE_NIBBLE           0x00020000
#define RX_VEC_CRC_ERROR                0x00040000
#define RX_VEC_MULTICAST                0x00080000
#define RX_VEC_BROADCAST                0x00100000
#define RX_VEC_INVALID_PREAMBLE         0x00200000
#define RX_VEC_LONG_EVENT               0x00400000
#define	RX_VEC_BAD_PACKET		0x00800000
#define RX_VEC_CARRIER_EVENT            0x01000000
#define RX_VEC_MULTICAST_MATCH          0x02000000
#define RX_VEC_PHYSICAL_MATCH           0x04000000
#define RX_VEC_RECEIVE_SEQNUM		0xF8000000
#define RX_VEC_RECEIVE_SEQNUM_SHIFT	27
#define RX_VEC_FINISHED			0x8000000000000000ll
#define RX_PROMISCUOUS                  \
	(RX_VEC_BROADCAST|RX_VEC_MULTICAST_MATCH|RX_VEC_PHYSICAL_MATCH)
#define	RX_VEC_CKSUM_SHIFT		32

/* Transmit command header */
#define	TX_CMD_LENGTH			0x00007FFF
#define	TX_CMD_OFFSET			0x007F0000
#define	TX_CMD_OFFSET_SHIFT		16
#define	TX_CMD_TERM_DMA			0x00800000
#define	TX_CMD_SENT_INT_EN		0x01000000
#define	TX_CMD_CONCAT_1			0x02000000
#define	TX_CMD_CONCAT_2			0x04000000
#define	TX_CMD_CONCAT_3			0x08000000
#define	TX_CMD_NUM_CATS			3

/* Transmit status vector */
#define TX_VEC_LENGTH                   0x00007FFF
#define TX_VEC_COLLISIONS               0x000F0000
#define TX_VEC_COLLISION_SHIFT          16
#define TX_VEC_LATE_COLLISION           0x00100000
#define TX_VEC_CRC_ERROR                0x00200000
#define TX_VEC_DEFERRED                 0x00400000
#define TX_VEC_COMPLETED_SUCCESSFULLY	0x00800000
#define TX_VEC_ABORTED_TOO_LONG         0x01000000
#define TX_VEC_ABORTED_UNDERRUN         0x02000000
#define TX_VEC_DROPPED_COLLISIONS       0x04000000
#define TX_VEC_CANCELED_DEFERRAL        0x08000000
#define TX_VEC_DROPPED_LATE_COLLISION   0x10000000
#define TX_VEC_FINISHED			0x8000000000000000ll

/* PHY defines */
#define	PHY_QS6612X			0x0181440	/* Quality TX */
#define PHY_ICS1889			0x0015F41	/* ICS FX */
#define PHY_ICS1890			0x0015F42	/* ICS TX */
#define PHY_DP83840			0x20005C0	/* National TX */

/* PHY errata structure */
struct phyerrata {
	__uint32_t type;
	unsigned short rev, reg, mask, val;
};

#endif


