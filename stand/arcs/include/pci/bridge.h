/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef IP32		/* whole file */
#ifndef __PCI_BRIDGE_H__
#define __PCI_BRIDGE_H__

#ident "$Revision: 1.2 $"

/*
 * bridge.h - bridge chip header file
 */

#include <sys/xtalk/xwidget.h>		/* generic widget header */

/* Start: Random stuff */

/* Widget part number of bridge */
#define BRIDGE_WIDGET_PART_NUM	0xc002     

/*
 * I/O page size
 */
#if _PAGESZ == 4096
#define IOPFNSHIFT		12		/* 4K per mapped page */
#else
#define	IOPFNSHIFT		14		/* 16K per mapped page */
#endif	/* _PAGESZ */
#define IOPGSIZE		(1<<IOPFNSHIFT)
#define IOPG(x)			(x>>IOPFNSHIFT)
/* End: Random stuff */

/*
 * Bridge address map 
 */

/* Bridge internal registers
 */

#define BRIDGE_WID_ID		WIDGET_ID
#define BRIDGE_WID_STAT		WIDGET_STATUS
#define BRIDGE_WID_ERR_UPPER	WIDGET_ERR_UPPER_ADDR
#define BRIDGE_WID_ERR_LOWER	WIDGET_ERR_LOWER_ADDR
#define BRIDGE_WID_CONTROL	WIDGET_CONTROL
#define BRIDGE_WID_REQ_TIMEOUT	WIDGET_REQ_TIMEOUT
#define BRIDGE_WID_INT_UPPER	WIDGET_INTDEST_UPPER_ADDR
#define BRIDGE_WID_INT_LOWER	WIDGET_INTDEST_LOWER_ADDR
#define BRIDGE_WID_ERR_CMDWORD	WIDGET_ERR_CMD_WORD
#define BRIDGE_WID_LLP		WIDGET_LLP_CFG
#define BRIDGE_WID_TARG_FLUSH	WIDGET_TFLUSH

#define	BRIDGE_WID_AUX_ERR	0x00005C	/* Aux Error Command Word */
#define	BRIDGE_WID_RESP_UPPER	0x000064	/* Response Buf Upper Addr */
#define	BRIDGE_WID_RESP_LOWER	0x00006C	/* Response Buf Lower Addr */
#define	BRIDGE_WID_TST_PIN_CTRL	0x000074	/* Test pin control */

#define BRIDGE_DIR_MAP		0x000084	/* Direct Map reg */

#define BRIDGE_RAM_PERR		0x000094	/* SSRAM Parity Error */

#define BRIDGE_ARB		0x0000A4	/* Arbitration Priority reg */

#define	BRIDGE_NIC		0x0000B4	/* Number In A Can */

#define BRIDGE_PCI_BUS_TIMEOUT	0x0000C4	/* Bus Timeout Register */
#define BRIDGE_PCI_CFG		0x0000CC	/* PCI Type 1 Config reg */
#define BRIDGE_PCI_ERR_UPPER	0x0000D4	/* PCI error Upper Addr */
#define BRIDGE_PCI_ERR_LOWER	0x0000DC	/* PCI error Lower Addr */

#define BRIDGE_INT_STATUS	0x000104	/* Interrupt Status */
#define BRIDGE_INT_ENABLE	0x00010C	/* Interrupt Enables */
#define BRIDGE_INT_RST_STAT	0x000114	/* Reset Intr Status */
#define BRIDGE_INT_MODE		0x00011C	/* Interrupt Mode */
#define BRIDGE_INT_DEVICE	0x000124	/* Interrupt Device */
#define	BRIDGE_INT_HOST_ERR	0x00012C	/* Host Error Field */

#define BRIDGE_INT_ADDR0 	0x000134	/* Host Address Reg */
#define	BRIDGE_INT_ADDR_OFF	0x000008	/* Host Addr offset (1..7) */
#define	BRIDGE_INT_ADDR(x)	(BRIDGE_INT_ADDR0+(x)*BRIDGE_INT_ADDR_OFF)

#define BRIDGE_DEVICE0 		0x000204	/* Device 0 */
#define	BRIDGE_DEVICE_OFF	0x000008	/* Device offset (1..7) */
#define	BRIDGE_DEVICE(x)	(BRIDGE_DEVICE0+(x)*BRIDGE_DEVICE_OFF)

#define BRIDGE_WR_REQ_BUF0	0x000244	/* Write Request Buffer 0 */
#define	BRIDTE_WR_REQ_BUF_OFF	0x000008	/* Buffer Offset (1..7) */
#define	BRIDGE_WR_REQ_BUF(x)	(BRIDGE_WR_REQ_BUF0+(x)*BRIDTE_WR_REQ_BUF_OFF)

#define	BRIDGE_EVEN_RESP	0x000284	/* Even Device Response Buf */
#define	BRIDGE_ODD_RESP		0x00028C	/* Odd Device Response Buf */

#define	BRIDGE_RESP_STATUS	0x000294	/* Read Response Status reg */
#define	BRIDGE_RESP_CLEAR	0x00029C	/* Read Response Clear reg */

/* Bridge I/O space */
#define BRIDGE_ATE_RAM		0x00010000	/* Internal Addr Xlat Ram */
#define BRIDGE_RRB_RAM		0x00018000	/* Read Response Buffer */

#define BRIDGE_TYPE0_CFG_DEV0	0x00020000	/* Type 0 Cfg, Device 0 */
#define	BRIDGE_TYPE0_CFG_OFF	0x00001000	/* Type 0 Cfg Offset (1..7) */
#define	BRIDGE_TYPE0_CFG_DEV(x)	(BRIDGE_TYPE0_CFG_DEV0+(x)*BRIDGE_TYPE0_CFG_OFF)

#define BRIDGE_PCI_IACK		0x00030000	/* PCI Interrupt Ack */
#define BRIDGE_EXT_SSRAM	0x00080000	/* Extern SSRAM (ATE) */

/* Bridge device IO spaces */
#define BRIDGE_DEV_CNT		8		/* Up to 8 devices per bridge */
#define BRIDGE_DEVIO0		0x00200000	/* Device IO 0 Addr */
#define BRIDGE_DEVIO1		0x00400000	/* Device IO 1 Addr */
#define BRIDGE_DEVIO2		0x00600000	/* Device IO 2 Addr */
#define	BRIDGE_DEVIO_OFF	0x00100000      /* Device IO Offset (3..7) */

#define	BRIDGE_DEVIO_2MB	0x00200000	/* Device IO Offset (0..1) */
#define	BRIDGE_DEVIO_1MB	0x00100000	/* Device IO Offset (2..7) */
#define	BRIDGE_DEVIO(x)		((x)<=1 ? BRIDGE_DEVIO0+(x)*BRIDGE_DEVIO_2MB : BRIDGE_DEVIO2+((x)-2)*BRIDGE_DEVIO_1MB)

#define	BRIDGE_EXTERNAL_FLASH	0x00C00000	/* External Flash PROMS */

/* Bridge RAM sizes */
#define BRIDGE_ATE_RAM_SIZE	0x00000400	/* 1kB ATE RAM */
#define BRIDGE_RRB_RAM_SIZE	0x00000800      /* 2kB Read Response Buffer */
		/* the SSRAM size is determined from the control register */
#define BRIDGE_SSRAM_512K 	0x00080000	/* 512kB */
#define BRIDGE_SSRAM_128K 	0x00020000	/* 128kB */
#define BRIDGE_SSRAM_64K 	0x00010000	/* 64kB */
#define BRIDGE_SSRAM_0K 	0x00000000	/* 0kB */


/* Bridge widget control register bits definition */
#define	BRIDGE_CTRL_FLASH_WR_EN		(0x1 << 31)
#define	BRIDGE_CTRL_EN_CLK50		(0x1 << 30)
#define	BRIDGE_CTRL_EN_CLK40		(0x1 << 29)
#define	BRIDGE_CTRL_EN_CLK33		(0x1 << 28)
#define	BRIDGE_CTRL_RST(n)		((n) << 24)
#define	BRIDGE_CTRL_RST_MASK		(BRIDGE_CTRL_RST(0xF))
#define	BRIDGE_CTRL_RST_PIN(x)		(BRIDGE_CTRL_RST(0x1 << (x)))
#define	BRIDGE_CTRL_IO_SWAP		(0x1 << 23)
#define	BRIDGE_CTRL_MEM_SWAP		(0x1 << 22)
#define	BRIDGE_CTRL_PAGE_SIZE		(0x1 << 21)
#define	BRIDGE_CTRL_SS_PAR_BAD		(0x1 << 20)
#define	BRIDGE_CTRL_SS_PAR_EN		(0x1 << 19)
#define	BRIDGE_CTRL_SSRAM_SIZE(n)	((n) << 17)
#define	BRIDGE_CTRL_SSRAM_SIZE_MASK	(BRIDGE_CTRL_SSRAM_SIZE(0x3))
#define	BRIDGE_CTRL_SSRAM_512K		(BRIDGE_CTRL_SSRAM_SIZE(0x3))
#define	BRIDGE_CTRL_SSRAM_128K		(BRIDGE_CTRL_SSRAM_SIZE(0x2))
#define	BRIDGE_CTRL_SSRAM_64K		(BRIDGE_CTRL_SSRAM_SIZE(0x1))
#define	BRIDGE_CTRL_SSRAM_1K		(BRIDGE_CTRL_SSRAM_SIZE(0x0))
#define	BRIDGE_CTRL_F_BAD_PKT		(0x1 << 16)
#define	BRIDGE_CTRL_LLP_XBAR_CRD(n)	((n) << 12)
#define	BRIDGE_CTRL_LLP_XBAR_CRD_MASK	(BRIDGE_CTRL_LLP_XBAR_CRD(0xf))
#define	BRIDGE_CTRL_CLR_RLLP_CNT	(0x1 << 11)
#define	BRIDGE_CTRL_CLR_TLLP_CNT	(0x1 << 10)
#define	BRIDGE_CTRL_SYS_END		(0x1 << 9)
#define	BRIDGE_CTRL_MAX_TRANS(n)	((n) << 4)
#define	BRIDGE_CTRL_MAX_TRANS_MASK	(BRIDGE_CTRL_MAX_TRANS(0x1f))
#define	BRIDGE_CTRL_WIDGET_ID(n)	((n) << 0)
#define	BRIDGE_CTRL_WIDGET_ID_MASK	(BRIDGE_CTRL_WIDGET_ID(0xf))

/* Bridge direct mapping register bits definition */
#define	BRIDGE_DIRMAP_W_ID_SHFT		20
#define	BRIDGE_DIRMAP_W_ID		(0xf << BRIDGE_DIRMAP_W_ID_SHFT)
#define	BRIDGE_DIRMAP_RMF_64		(0x1 << 18)
#define	BRIDGE_DIRMAP_ADD512		(0x1 << 17)
#define	BRIDGE_DIRMAP_OFF		(0x1ffff << 0)

/* Bridge interrupt status register bits definition */
#define	BRIDGE_ISR_MULTI_ERR		(0x1 << 31)
#define	BRIDGE_ISR_PMU_ESIZE_FAULT	(0x1 << 30)
#define	BRIDGE_ISR_UNEXP_RESP		(0x1 << 29)
#define	BRIDGE_ISR_BAD_XRESP_PKT	(0x1 << 28)
#define	BRIDGE_ISR_BAD_XREQ_PKT		(0x1 << 27)
#define	BRIDGE_ISR_RESP_XTLK_ERR	(0x1 << 26)
#define	BRIDGE_ISR_REQ_XTLK_ERR		(0x1 << 25)
#define	BRIDGE_ISR_INVLD_ADDR		(0x1 << 24)
#define	BRIDGE_ISR_UNSUPPORTED_XOP	(0x1 << 23)
#define	BRIDGE_ISR_XREQ_FIFO_OFLOW	(0x1 << 22)
#define	BRIDGE_ISR_LLP_REC_SNERR	(0x1 << 21)
#define	BRIDGE_ISR_LLP_REC_CBERR	(0x1 << 20)
#define	BRIDGE_ISR_LLP_RCTY		(0x1 << 19)
#define	BRIDGE_ISR_LLP_TX_RETRY		(0x1 << 18)
#define	BRIDGE_ISR_LLP_TCTY		(0x1 << 17)
#define	BRIDGE_ISR_SSRAM_PERR		(0x1 << 16)
#define	BRIDGE_ISR_PCI_ABORT		(0x1 << 15)
#define	BRIDGE_ISR_PCI_PARITY		(0x1 << 14)
#define	BRIDGE_ISR_PCI_SERR		(0x1 << 13)
#define	BRIDGE_ISR_PCI_PERR		(0x1 << 12)
#define	BRIDGE_ISR_PCI_MST_TIMEOUT	(0x1 << 11)
#define	BRIDGE_ISR_PCI_RETRY_CNT	(0x1 << 10)
#define	BRIDGE_ISR_XREAD_REQ_TIMEOUT	(0x1 << 9)
#define	BRIDGE_ISR_GIO_B_ENBL_ERR	(0x1 << 8)
#define	BRIDGE_ISR_INT_MSK		(0xff << 0)
#define	BRIDGE_ISR_INT(x)		(0x1 << (x))

/* Bridge interrupt enable register bits definition */
#define	BRIDGE_IMR_UNEXP_RESP		BRIDGE_ISR_UNEXP_RESP
#define	BRIDGE_IMR_PMU_ESIZE_FAULT	BRIDGE_ISR_PMU_ESIZE_FAULT
#define	BRIDGE_IMR_BAD_XRESP_PKT	BRIDGE_ISR_BAD_XRESP_PKT
#define	BRIDGE_IMR_BAD_XREQ_PKT		BRIDGE_ISR_BAD_XREQ_PKT
#define	BRIDGE_IMR_RESP_XTLK_ERR	BRIDGE_ISR_RESP_XTLK_ERR
#define	BRIDGE_IMR_REQ_XTLK_ERR		BRIDGE_ISR_REQ_XTLK_ERR
#define	BRIDGE_IMR_INVLD_ADDR		BRIDGE_ISR_INVLD_ADDR
#define	BRIDGE_IMR_UNSUPPORTED_XOP	BRIDGE_ISR_UNSUPPORTED_XOP
#define	BRIDGE_IMR_XREQ_FIFO_OFLOW	BRIDGE_ISR_XREQ_FIFO_OFLOW
#define	BRIDGE_IMR_LLP_REC_SNERR	BRIDGE_ISR_LLP_REC_SNERR
#define	BRIDGE_IMR_LLP_REC_CBERR	BRIDGE_ISR_LLP_REC_CBERR
#define	BRIDGE_IMR_LLP_RCTY		BRIDGE_ISR_LLP_RCTY
#define	BRIDGE_IMR_LLP_TX_RETRY		BRIDGE_ISR_LLP_TX_RETRY
#define	BRIDGE_IMR_LLP_TCTY		BRIDGE_ISR_LLP_TCTY
#define	BRIDGE_IMR_SSRAM_PERR		BRIDGE_ISR_SSRAM_PERR
#define	BRIDGE_IMR_PCI_ABORT		BRIDGE_ISR_PCI_ABORT
#define	BRIDGE_IMR_PCI_PARITY		BRIDGE_ISR_PCI_PARITY
#define	BRIDGE_IMR_PCI_SERR		BRIDGE_ISR_PCI_SERR
#define	BRIDGE_IMR_PCI_PERR		BRIDGE_ISR_PCI_PERR
#define	BRIDGE_IMR_PCI_MST_TIMEOUT	BRIDGE_ISR_PCI_MST_TIMEOUT
#define	BRIDGE_IMR_PCI_RETRY_CNT	BRIDGE_ISR_PCI_RETRY_CNT
#define	BRIDGE_IMR_XREAD_REQ_TIMEOUT	BRIDGE_ISR_XREAD_REQ_TIMEOUT
#define	BRIDGE_IMR_GIO_B_ENBL_ERR	BRIDGE_ISR_GIO_B_ENBL_ERR
#define	BRIDGE_IMR_INT_MSK		BRIDGE_ISR_INT_MSK
#define	BRIDGE_IMR_INT(x)		BRIDGE_ISR_INT(x)

/* Bridge interrupt reset register bits definition */
#define	BRIDGE_IRR_MULTI_CLR		(0x1 << 6)
#define	BRIDGE_IRR_CRP_GRP_CLR		(0x1 << 5)
#define	BRIDGE_IRR_RESP_BUF_GRP_CLR	(0x1 << 4)
#define	BRIDGE_IRR_REQ_DSP_GRP_CLR	(0x1 << 3)
#define	BRIDGE_IRR_LLP_GRP_CLR		(0x1 << 2)
#define	BRIDGE_IRR_SSRAM_GRP_CLR	(0x1 << 1)
#define	BRIDGE_IRR_PCI_GRP_CLR		(0x1 << 0)
#define	BRIDGE_IRR_ALL_CLR		0x7f

#define	BRIDGE_IRR_CRP_GRP		(BRIDGE_ISR_UNEXP_RESP | \
			 		 BRIDGE_ISR_XREQ_FIFO_OFLOW)
#define	BRIDGE_IRR_RESP_BUF_GRP		(BRIDGE_ISR_BAD_XRESP_PKT | \
			 		 BRIDGE_ISR_RESP_XTLK_ERR | \
			 		 BRIDGE_ISR_XREAD_REQ_TIMEOUT)
#define	BRIDGE_IRR_REQ_DSP_GRP		(BRIDGE_ISR_UNSUPPORTED_XOP | \
			 		 BRIDGE_ISR_BAD_XREQ_PKT | \
			 		 BRIDGE_ISR_REQ_XTLK_ERR | \
			 		 BRIDGE_ISR_INVLD_ADDR)
#define	BRIDGE_IRR_LLP_GRP		(BRIDGE_ISR_LLP_REC_SNERR | \
			 		 BRIDGE_ISR_LLP_REC_CBERR | \
			 		 BRIDGE_ISR_LLP_RCTY | \
			 		 BRIDGE_ISR_LLP_TX_RETRY | \
			 		 BRIDGE_ISR_LLP_TCTY)
#define	BRIDGE_IRR_SSRAM_GRP		(BRIDGE_ISR_SSRAM_PERR | \
					 BRIDGE_ISR_PMU_ESIZE_FAULT)
#define	BRIDGE_IRR_PCI_GRP		(BRIDGE_ISR_PCI_ABORT | \
			 		 BRIDGE_ISR_PCI_PARITY | \
			 		 BRIDGE_ISR_PCI_SERR | \
			 		 BRIDGE_ISR_PCI_PERR | \
			 		 BRIDGE_ISR_PCI_MST_TIMEOUT | \
			 		 BRIDGE_ISR_PCI_RETRY_CNT | \
			 		 BRIDGE_ISR_GIO_B_ENBL_ERR)

/* Bridge INT_DEV register bits definition */
#define	BRIDGE_INT_DEV_SHFT(n)		((n)*3)
#define	BRIDGE_INT_DEV_MASK(n)		(0x7 << BRIDGE_INT_DEV_SHFT(n))

/* Bridge interrupt(x) register bits definition */
#define	BRIDGE_INT_ADDR_HOST		0x0001FF00
#define	BRIDGE_INT_ADDR_FLD		0x000000FF

/* Bridge device(x) register bits definition */
#define	BRIDGE_DEV_ERR_LOCK_EN		0x10000000
#define	BRIDGE_DEV_PAGE_CHK_EN		0x08000000
#define	BRIDGE_DEV_FORCE_PCI_PAR	0x04000000
#define BRIDGE_DEV_VIRTUAL_EN		0x02000000
#define BRIDGE_DEV_PMU_WRGA_EN		0x01000000
#define BRIDGE_DEV_DIR_WRGA_EN		0x00800000
#define BRIDGE_DEV_DEV_SIZE		0x00400000
#define BRIDGE_DEV_RT			0x00200000
#define BRIDGE_DEV_SWAP_PMU 		0x00100000
#define BRIDGE_DEV_SWAP_DIR 		0x00080000
#define BRIDGE_DEV_PREF			0x00040000
#define BRIDGE_DEV_PRECISE		0x00020000
#define BRIDGE_DEV_COH			0x00010000
#define BRIDGE_DEV_BARRIER		0x00008000
#define BRIDGE_DEV_GBR			0x00004000
#define BRIDGE_DEV_DEV_SWAP		0x00002000
#define BRIDGE_DEV_DEV_IO_MEM		0x00001000
#define BRIDGE_DEV_OFF_MASK		0x00000fff

/* Bridge interrupt mode register bits definition */
#define BRIDGE_INTMODE_CLR_PKT_EN(x)	(0x1 << (x))

/* this should be written to the xbow's link_control(x) register */
#define	BRIDGE_CREDIT	3

/* Ranges of PCI bus space that can be accessed via PIO from xtalk */
#define BRIDGE_MIN_PIO_ADDR_MEM		0x00000000  /* 1G PCI memory space */
#define BRIDGE_MAX_PIO_ADDR_MEM		0x3fffffff
#define BRIDGE_MIN_PIO_ADDR_IO		0x00000000  /* 4G PCI IO space */
#define BRIDGE_MAX_PIO_ADDR_IO		0xffffffff

#define PCI32_LOCAL_BASE		0
#define PCI32_MAPPED_BASE		0x40000000
#define PCI32_DIRECT_BASE		0x80000000
#define PCI64_BASE			0x100000000


#if LANGUAGE_C
#define IS_PCI32_LOCAL(x)	((ulong_t)(x) < PCI32_MAPPED_BASE)
#define IS_PCI32_MAPPED(x)	((ulong_t)(x) < PCI32_DIRECT_BASE && \
					(ulong_t)(x) >= PCI32_MAPPED_BASE)
#define IS_PCI32_DIRECT(x)	((ulong_t)(x) >= PCI32_MAPPED_BASE)
#define IS_PCI64(x)		((ulong_t)(x) >= PCI64_BASE)
#endif /* LANGUAGE_C */


/* The GIO address space.
 */
#define GIO_LOCAL_BASE			0x00000000
#define GIO_MAPPED_BASE			0x40000000
#define GIO_DIRECT_BASE			0x80000000
#define GIO64_BASE			0x100000000

#if LANGUAGE_C
#define IS_GIO_LOCAL(x)		((ulong_t)(x) < GIO_MAPPED_BASE)
#define IS_GIO_MAPPED(x)	((ulong_t)(x) < GIO_DIRECT_BASE && \
					(ulong_t)(x) >= GIO_MAPPED_BASE)
#define IS_GIO_DIRECT(x)	((ulong_t)(x) >= GIO_MAPPED_BASE)
#define IS_GIO64(x)		((ulong_t)(x) >= GIO64_BASE)
#endif /* LANGUAGE_C */

/* PCI to xtalk mapping
 */

/* given a DIR_OFF value and a pci/gio 32 bits direct address, determine
 * which xtalk address is accessed
 */
#define	BRIDGE_DIRECT_32_SEG_SIZE	0x80000000
#define BRIDGE_DIRECT_32_TO_XTALK(dir_off,adr)		\
	((dir_off) * BRIDGE_DIRECT_32_SEG_SIZE +	\
		((adr) & (BRIDGE_DIRECT_32_SEG_SIZE - 1)) + PHYS_RAMBASE)


/* 64-bit address attribute masks */
#define PCI64_ATTR_TARG		0xf000000000000000
#define PCI64_ATTR_PREF		0x0800000000000000
#define PCI64_ATTR_PREC		0x0400000000000000
#define PCI64_ATTR_VIRTUAL	0x0200000000000000
#define PCI64_ATTR_BAR		0x0100000000000000
#define PCI64_ATTR_RMF		0x00ff000000000000

#if LANGUAGE_C
/* Address translation entry for mapped pci32 accesses */
typedef struct ate_s {
    union {
    __uint64_t	rmf:16,
	addr:36,
	targ:4,
	reserved:3,
	barrier:1,
	prefetch:1,
	precise:1,
	coherent:1,
	valid:1;
    int ate_bits[2];
    } x;
} ate_t;
#endif /* LANGUAGE_C */

#define ATE_V		0x01
#define ATE_CO		0x02
#define ATE_PREC	0x04
#define ATE_PREF	0x08
#define ATE_BAR		0x10
#define ATE_SWAP	0x20

#define ATE_PFNSHIFT		12
#define ATE_TIDSHIFT		8
#define	ATE_RMFSHIFT		48

/* Create an ATE directed at local memory */
#define mkate(mode,pfn)		\
	(mode | pfn << ATE_PFNSHIFT | TID_MEMORY << ATE_TIDSHIFT)

/* Create an ATE directed at tid */
#define mkate_tid(mode,pfn,tid)		\
	(mode | pfn << ATE_PFNSHIFT | tid << ATE_TIDSHIFT)

/* Create an ATE directed at tid/rmf */
#define mkate_rmf(mode,pfn,tid,rmf)		\
	(mode | pfn << ATE_PFNSHIFT | rmf << ATE_RMFSHIFT | tid << ATE_TIDSHIFT)

#define BRIDGE_INTERNAL_ATES	128
#define	BRIDGE_ATE_VPN_MASK	0x3ffff000

#if LANGUAGE_C
/* XXX does any code use this ? */
#define ate_dropin(ba,pate)	\
	(ba & BRIDGE_ATE_VPN_MASK >> IOPFNSHIFT) >= BRIDGE_INTERNAL_ATES ? \
		atemem[ba & BRIDGE_ATE_VPN_MASK >> IOPFNSHIFT] = *pate :   \
		atebridge[ba & BRIDGE_ATE_VPN_MASK >> IOPFNSHIFT] = *pate

/* Bridge chip registers are 32 bits wide.  32-bits access only */

typedef __uint32_t bridgereg_t;

/* Bridge chip registers */
typedef volatile struct bridge_s {
    widget_cfg_t b_widget;			/* 0x000 */

    bridgereg_t	b_pad_0;
    bridgereg_t	b_aux_err_cmd_word;		/* 0x05c */
    bridgereg_t	b_pad_1;
    bridgereg_t	b_resp_buf_upp_addr;		/* 0x064 */
    bridgereg_t	b_pad_2;
    bridgereg_t	b_resp_buf_low_addr;		/* 0x06c */
    bridgereg_t	b_pad_3[5];
    bridgereg_t b_dir_map;			/* 0x084 */
    bridgereg_t	b_pad_4[3];
    bridgereg_t b_ssram_perr;			/* 0x094 */
    bridgereg_t b_pad_5[3];
    bridgereg_t b_arb_pri;			/* 0x0a4 */
    bridgereg_t b_pad_6[3];
    bridgereg_t b_nic;				/* 0x0b4 */
    bridgereg_t b_pad_7[3];
    bridgereg_t b_pci_timeout;			/* 0x0c4 */
    bridgereg_t	b_pad_8;
    bridgereg_t b_pci_type1_cfg;		/* 0x0cc */
    bridgereg_t	b_pad_9;
    bridgereg_t b_pci_err_upp_addr;		/* 0x0d4 */
    bridgereg_t	b_pad_10;
    bridgereg_t b_pci_err_low_addr;		/* 0x0dc */
    bridgereg_t b_pad_11[9];
    bridgereg_t b_int_status;			/* 0x104 */
    bridgereg_t	b_pad_12;
    bridgereg_t b_int_enable;			/* 0x10c */
    bridgereg_t	b_pad_13;
    bridgereg_t b_reset_int_status;		/* 0x114 */
    bridgereg_t	b_pad_14;
    bridgereg_t b_int_mode;			/* 0x11c */
    bridgereg_t	b_pad_15;
    bridgereg_t b_int_dev;			/* 0x124 */
    bridgereg_t	b_pad_16;
    bridgereg_t b_host_err_field;		/* 0x12c */

    struct {					/* 0x130 */
	bridgereg_t b_pad_17;
	bridgereg_t addr;
    } b_int_host[8];

    bridgereg_t	b_pad_18[36];

    struct {					/* 0x200 */
	bridgereg_t b_pad_19;
	bridgereg_t reg;
    } b_dev[8];

    struct {					/* 0x240 */
	bridgereg_t b_pad_20;
	bridgereg_t reg;
    } b_wr_req_buf[8];

    bridgereg_t b_pad_21;
    bridgereg_t b_even_dev_resp_buf;		/* 0x284 */
    bridgereg_t b_pad_22;
    bridgereg_t b_odd_dev_resp_buf;		/* 0x28c */
    bridgereg_t b_pad_23;
    bridgereg_t b_rd_resp_buf_status;		/* 0x294 */
    bridgereg_t b_pad_24;
    bridgereg_t b_rd_resp_buf_clear;		/* 0x29c */

    char b_pad_25[BRIDGE_EXT_SSRAM-0x2A0];

    __uint64_t b_ext_ssram[256<<10];		/* 0x80000 */

} bridge_t;

#define	BRIDGE_K1PTR		((bridge_t *)PHYS_TO_COMPATK1(BRIDGE_BASE))

#define BRIDGE_CLR_INT(b,d) (((volatile bridge_t *)b)->b_clr_int = 1 << (d))

typedef struct bridge_dev_cfg_s {
    uint devicecfg;		/* values for device(x) register */
    char intr;			/* which interrupt (0-7) */
    char intmode;		/* interrupt modes */
    uint hostaddr;		/* values for interrupt(x) host register */
    uint pcivec;		/* pci vector used by this device */
} bridge_dev_cfg_t;

extern void init_bridge(bridge_t *);

#endif /* LANGUAGE_C */

#endif /* __PCI_BRIDGE_H__ */
#endif /*ndef IP32 */
