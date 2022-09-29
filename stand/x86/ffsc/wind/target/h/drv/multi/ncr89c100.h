/* ncr89c100.h - National Cash Register NCR89C100 Master I/O Chip header. */

/* Copyright 1994-1995 Wind River Systems, Inc. */

/* Copyright 1994-1995 FORCE COMPUTERS */

/*
modification history
--------------------
01b,15mar95,vin  integrated with 5.2.
01a,28jun93,tkj  written
*/

/* _PFN = 24 bit Page frame number = Physical address >> PAGE_SHIFT */
/* This is needed because the full physical address is 36 bits. */

/* _OFS = Offset from preceeding _PFN entry. */
/* This can be used to compute both the virtual and physical addresses. */

#ifndef __INCncr89c100h
#define __INCncr89c100h

#ifndef	_ASMLANGUAGE

/* #include	"drv/dma/l64854.h"	 * DMA2 header */
#include	"drv/netif/if_ln.h"	/* Ethernet header */
#include	"drv/scsi/ncr5390.h"	/* SCSI header */
/* There should also be a parallel header */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WARNING: The NCR89C100 Chip Specification says that the
 * DMA/Ethernet/SCSI/Parallel registers are at SBus address 0x08xxxxxx.
 * In the SS10 they are at 0x00xxxxxx and the DBRI chip is at 0x08xxxxxx.
 *
 * In the SS10 use SS10_OFS() to correct the offset as above.
 */

#define	SS10_OFS(OFS) ((OFS) - 0x08000000)

/* Offsets of functional blocks from address of this chip (pp. 2-25) */
#define	NCR89C100_DMA_OFS	   0x08000000 /* DMA2 */
#define	NCR89C100_DMA_PP_OFS	   0x0c800000 /* DMA: Parallel Port */
#define NCR89C100_DMA_SCSI_OFS     0x08400000 /* DMA: SCSI */
#define NCR89C100_DMA_ETHERNET_OFS 0x08400010 /* DMA: Ethernet */
#define	NCR89C100_SCSI_OFS	   0x08800000 /* SCSI */
#define	NCR89C100_ETHERNET_OFS	   0x08c00000 /* Ethernet */

/* Extra #defines not include in the include files above. */

/*
 * DMA2 Ethernet registers
 */

/* DMA2 Ethernet register E_CSR */
				/* Offset from NCR89C100_DMA_ETHERNET_OFS */
#define	NCR89C100_E_CSR_OFS		0

#define	NCR89C100_E_CSR_DEV_ID_MASK		0xff000000
#define	NCR89C100_E_CSR_DEV_ID_SHIFT		28

#define	NCR89C100_E_CSR_TP_AUI_MASK		0x00400000

#define	NCR89C100_E_CSR_LOOP_TEST_MASK		0x00200000

#define	NCR89C100_E_CSR_BURST_SIZE_MASK		0x000c0000
#define	NCR89C100_E_CSR_BURST_SIZE_SHIFT	18

#define	NCR89C100_E_CSR_DSBL_WR_INVAL_MASK	0x00020000

#define	NCR89C100_E_CSR_DSBL_BUF_WR_MASK	0x00010000

#define NCR89C100_E_CSR_ILACC_MASK		0x00008000

#define	NCR89C100_E_CSR_DSBL_RD_DRN_MASK	0x00001000

#define	NCR89C100_E_CSR_DSBL_WR_DRN_MASK	0x00000800

#define	NCR89C100_E_CSR_DRAIN_MASK		0x00000400

#define	NCR89C100_E_CSR_RESET_MASK		0x00000080

#define	NCR89C100_E_CSR_SLAVE_ERR_MASK		0x00000040

#define	NCR89C100_E_CSR_INVALIDATE_MASK		0x00000020

#define	NCR89C100_E_CSR_INT_EN_MASK		0x00000010

#define	NCR89C100_E_CSR_DRAINING_MASK		0x0000000c
#define	NCR89C100_E_CSR_DRAINING_SHIFT		2

#define	NCR89C100_E_CSR_ERR_PEND_MASK		0x00000002

#define	NCR89C100_E_CSR_INT_PEND_MASK		0x00000001

/* DMA2 Ethernet register E_TST_CSR */
				/* Offset from NCR89C100_DMA_ETHERNET_OFS */
#define	NCR89C100_E_TST_CSR_OFS		4

#define	NCR89C100_E_TST_CSR_LD_TAG_MASK		0x80000000	/* W */

#define	NCR89C100_E_TST_CSR_RD_LINE_MASK	0x40000000	/* W */
#define	NCR89C100_E_TST_CSR_REQ_OUT_MASK	0x40000000	/* R */

#define	NCR89C100_E_TST_CSR_HIT_MASK		0x20000000	/* R */

#define	NCR89C100_E_TST_CSR_LRU_MASK		0x10000000	/* R */

#define	NCR89C100_E_TST_CSR_DRAIN1_MASK		0x08000000	/* R/W */

#define	NCR89C100_E_TST_CSR_DRAIN0_MASK		0x04000000	/* R/W */

#define	NCR89C100_E_TST_CSR_DIRTY1_MASK		0x02000000	/* R/W */

#define	NCR89C100_E_TST_CSR_DIRTY0_MASK		0x01000000	/* R/W */

#define	NCR89C100_E_TST_CSR_E_ADDR_MASK		0x00ffffff	/* R/W */
#define	NCR89C100_E_TST_CSR_E_ADDR_SHIFT	0

/* DMA2 Ethernet register E_VLD */
				/* Offset from NCR89C100_DMA_ETHERNET_OFS */
#define	NCR89C100_E_VLD_OFS		8

/* DMA2 Ethernet register E_BASE_ADDR */
				/* Offset from NCR89C100_DMA_ETHERNET_OFS */
#define	NCR89C100_E_BASE_ADDR_OFS	0xc

/*
 * Ethernet registers
 */

#define	NCR89C100_RDP_OFS		0	/* Register data port */
#define	NCR89C100_RAP_OFS		2	/* Register address port */

/* Control Status Register 0 */
#define	NCR89C100_CSR0_ERR_MASK		0x8000
#define	NCR89C100_CSR0_BAB_MASK		0x4000
#define	NCR89C100_CSR0_CE_MASK		0x2000
#define	NCR89C100_CSR0_MISS_MASK	0x1000
#define	NCR89C100_CSR0_ME_MASK		0x0800
#define	NCR89C100_CSR0_RINT_MASK	0x0400
#define	NCR89C100_CSR0_TINT_MASK	0x0200
#define	NCR89C100_CSR0_IFIN_MASK	0x0100
#define	NCR89C100_CSR0_INT_MASK		0x0080
#define	NCR89C100_CSR0_IEN_MASK		0x0040
#define	NCR89C100_CSR0_RON_MASK		0x0020
#define	NCR89C100_CSR0_TON_MASK		0x0010
#define	NCR89C100_CSR0_TD_MASK		0x0008
#define	NCR89C100_CSR0_STP_MASK		0x0004
#define	NCR89C100_CSR0_STR_MASK		0x0002
#define	NCR89C100_CSR0_INIT_MASK	0x0001

/* Control Status Register 3 */
#define	NCR89C100_CSR3_IPGS_MASK	0x8000

#define	NCR89C100_CSR3_RX_TX_IPG_MASK	0x7000
#define	NCR89C100_CSR3_RX_TX_IPG_SHIFT	12

#define	NCR89C100_CSR3_TX_TX_IPG_MASK	0x0e00
#define	NCR89C100_CSR3_TX_TX_IPG_SHIFT	9

#define	NCR89C100_CSR3_SWAP_MASK	0x0004
#define	NCR89C100_CSR3_ACON_MASK	0x0002
#define	NCR89C100_CSR3_BCON_MASK	0x0001

#ifndef	_ASMLANGUAGE
#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCncr89c100h */
