#ifndef __IDE_BRIDGE_H__
#define	__IDE_BRIDGE_H__
/*
 * d_bridge.h
 *
 *	bridge tests header
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
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

#ident "ide/godzilla/include/d_bridge.h: $Revision: 1.27 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/*
 * Bridge Ram test contants & macros
 */
#define RAM_ADDR_UNIQ	0	/* address uniqueness type of test */
#define RAM_WALKING	1	/* walking bit type of test */
#define BRIDGE_EXT_SSRAM_SIZE	0 	/* actually determined from ctrl reg */
/* ram funny addressing : Write Double Words and Read Single Words.
   If a word is written at 0x10000, half is read at 0x10004 (63:32), 
   half at 0x11004 (31:0) */
#define ATE_LSW_OFFSET	0x1000	/* ram ATE Least Significant Word Offset */
#define ATE_MSW_OFFSET	0x0000	/* ram ATE Most Significant Word Offset */
#define ATE_RD_OFFSET	0x4	/* ram ATE Read Offset */
#define LSWORD_ADDR(address)	(address+ATE_LSW_OFFSET+ATE_RD_OFFSET)	
#define MSWORD_ADDR(address)	(address+ATE_MSW_OFFSET+ATE_RD_OFFSET)	

/*
 * Bridge interrupt test constants (some also used in heart/bridge tests)
 */
#define CAUSE_LEVEL0_BIT	0x1<<10	/* position of H level0 intr bit */
					/* in processor cause register */
	/* NOTE: there's 8 interrupt bits in the cause register: */
	/*	bits 0 & 1 are for software generated interrupt, */
	/*	bits 2-6 are from HEART, bit 7 is from the proc internal timer*/
#define LEVEL0_LOW	3	/* 0 is IR and 1,2 are flow-control */
#define LEVEL0_HIGH	15
#define LEVEL1_LOW	16
#define LEVEL1_HIGH	31
#define LEVEL2_LOW	32
#define LEVEL2_HIGH	49
#define LEVEL3_LOW	50	/* Timer */
#define LEVEL3_HIGH	50
#define LEVEL4_LOW	51
#define LEVEL4_HIGH	58
#define LEVEL0		0	/* intr level */
#define LEVEL1		1
#define LEVEL2		2
#define LEVEL3		3
#define LEVEL4		4

/*
 * Bridge PCI Master Test constants 
 */
#define	PCI_RETRY_HLD_SHIFT	16	/* depends on the spec */
#define	PCI_RETRY_CNT_SHIFT	0
#define	PCI_RETRY_HLD		0x5
#define	PCI_RETRY_CNT		0x5
#define	PCI_ADD_OFF_MASK	0xfffff

/*
 * Bridge constants used in the heart-bridge tests
 */
#define D_BRIDGE_WID_REQ_TIMEOUT	0x000fffff	/* reset value */

/*
 * Bridge Register Read-Write Masks (32 bits)
 * NOTE: if a bit is undefined at power-up, it is masked
 */
#define	BRIDGE_WID_ID_MASK		0x0ffff001	
#define	BRIDGE_WID_STAT_MASK		0xffff001f	
#define	BRIDGE_WID_ERR_UPPER_MASK	0x00000000	
#define	BRIDGE_WID_ERR_LOWER_MASK	0x00000000	
#define	BRIDGE_WID_CONTROL_MASK		0xffffffff	
#define	BRIDGE_WID_REQ_TIMEOUT_MASK	0x000fffff	
#define	BRIDGE_WID_INT_UPPER_MASK	0x00000000	
#define	BRIDGE_WID_INT_LOWER_MASK	0x00000000	
#define	BRIDGE_WID_ERR_CMDWORD_MASK	0x00000000	
#define	BRIDGE_WID_LLP_MASK		0x03ffffff	
#define	BRIDGE_WID_TFLUSH_MASK		0x00000000	
#define	BRIDGE_WID_AUX_ERR_MASK		0x00000000	
#define	BRIDGE_WID_RESP_UPPER_MASK	0x0000ffff	
#define	BRIDGE_WID_RESP_LOWER_MASK	0xffffffff	
#define	BRIDGE_WID_TST_PIN_CTRL_MASK	0x00000fff	

#define	BRIDGE_DIR_MAP_MASK		0x00f7ffff	
#define	BRIDGE_ARB_MASK			0x0003ff7f	
/* NOTE: the NIC register does not retain the value written in its offset */
/*        field (9:2) but decrements it with time. Hence mask it */
#define	BRIDGE_NIC_MASK			0x000ff002	
#define	BRIDGE_PCI_BUS_TIMEOUT_MASK	0x001f13ff	
#define	BRIDGE_PCI_CFG_MASK		0x00fff800	
#define	BRIDGE_PCI_ERR_UPPER_MASK	0x00000000 /* all unknown at reset */
#define	BRIDGE_PCI_ERR_LOWER_MASK	0x00000000 /* all unknown at reset */
#define	BRIDGE_INT_STATUS_MASK		0xffffffff	
#define	BRIDGE_INT_ENABLE_MASK		0x7fffffff	
#define	BRIDGE_INT_RST_STAT_MASK	0x0000007f	
#define	BRIDGE_INT_MODE_MASK		0x000000ff	
#define	BRIDGE_INT_DEVICE_MASK		0x00ffffff	
#define	BRIDGE_INT_HOST_ERR_MASK	0x00000000	
#define	BRIDGE_INT_ADDR_MASK(x)		0x0003ffff	
#define	BRIDGE_DEVICE_MASK(x)		0x19fff000	
#define	BRIDGE_WR_REQ_BUF_MASK(x)	0x00000000	/* XXX */

/*
 * Bridge Register Default Values
 */
#define	D_BRIDGE_STATUS			0x60
#define	D_GIO_BRIDGE_STATUS		0x40

/*
 * other constants
 */
#define	BR_REGS_PATTERN_MAX	6	/* in br_regs */
#define	SUPER_IO_INTR		0x14	/* INTR[4] is on (super-io) */

/* 
 * 	register access macros depend bridge_wid_no to be set to
 *	the correct xbow port id. Default is XBOW_PORT_F
 */
#define	BR_REG_WR_32(address, mask, value) \
	PIO_REG_WR_32((address+MAIN_WIDGET(bridge_xbow_port)), mask, value);

#define	BR_REG_RD_32(address, mask, value) \
	PIO_REG_RD_32((address+MAIN_WIDGET(bridge_xbow_port)), mask, value);

/* for Internal ATE: need to to double word writes and single word reads */
#define	BR_ATE_DW_WR_64(address, mask, value) \
	PIO_REG_WR_64((address+MAIN_WIDGET(bridge_xbow_port)), mask, value);

extern int bridge_xbow_port;
extern void get_bridge_port(int, char **);
#define SET_BRIDGE_XBOW_PORT(_argc, _argv)		\
	get_bridge_port((_argc), (_argv))

/*
 * structure definitions
 */
typedef struct	_Bridge_Regs {
	char		*name;  	/* name of the register */
	__uint64_t	address;	/* address */
	bridgereg_t	mask;	    	/* read-write mask */
	unsigned char	mode;	    	/* read / write only or read & write */
} Bridge_Regs;


/* used in the bridge ram tests */
typedef struct	_Bridge_Ram {
	char		*ram_name;  	/* name of the ram to be tested */
	__uint64_t	start_address;	/* start address of ram */
	__uint32_t	ram_size;	/* ram size in bytes */
	__uint32_t	test_type;	/* type of ram test to be run */
} Bridge_Ram;

#endif /* C || C++ */

#endif	/* __IDE_BRIDGE_H__ */

