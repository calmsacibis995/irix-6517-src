/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident	"ide/godzilla/include/d_xbow.h:  $Revision: 1.11 $"

/*
 * IDE xbow tests header
 */

/*
 */

#ifndef __IDE_XBOW_H__
#define __IDE_XBOW_H__

/*
 * Xbow Register Read-Write Masks
 *	(values from working specs, Jan 96)
 */
#define	XBOW_WID_ID_MASK		XBOWCONST 0xffffffff
#define	XBOW_WID_STAT_MASK		XBOWCONST 0xff800027
#define	XBOW_WID_ERR_UPPER_MASK		XBOWCONST 0x0000ffff
#define	XBOW_WID_ERR_LOWER_MASK		XBOWCONST 0xffffffff
#define	XBOW_WID_CONTROL_MASK		XBOWCONST 0x00000026 /* bits1,2,5 only*/
#define	XBOW_WID_REQ_TO_MASK		XBOWCONST 0x000fffff /* packet timeout*/
#define	XBOW_WID_INT_UPPER_MASK		XBOWCONST 0xff0fffff
#define	XBOW_WID_INT_LOWER_MASK		XBOWCONST 0xffffffff
#define	XBOW_WID_ERR_CMDWORD_MASK	XBOWCONST 0xffffffff
#define	XBOW_WID_LLP_MASK		XBOWCONST 0x03ffffff
#define	XBOW_WID_ARB_RELOAD_MASK	XBOWCONST 0x0000003f
#define	XBOW_WID_PERF_CTR_A_MASK	XBOWCONST 0x00700000 /* mask undef val*/
#define	XBOW_WID_PERF_CTR_B_MASK	XBOWCONST 0x00700000 /* mask undef val*/
#define	XBOW_WID_NIC_MASK		XBOWCONST 0x000ffffe /* NIC-DATA=x*/
#define	XBOW_WID_NIC_RW_MASK		XBOWCONST 0x000ffc00 /* NIC RW bit mask*/

/* link values: same for all 8 */
#define	XB_LINK_IBUF_FLUSH_MASK		XBOWCONST 0xffffffff
#define	XB_LINK_CTRL_MASK		XBOWCONST 0xbfff01ff
#define	XB_LINK_STATUS_MASK		XBOWCONST 0xffffffff /* mask bit 31 that varies from link to link ("link alive" bit) */
#define	XB_LINK_AUX_STATUS_MASK		XBOWCONST 0x0000006f /* mask bit 4 that varies from link to link ("16/8 bit link" bit) */
#define	XB_LINK_ARB_UPPER_MASK		XBOWCONST 0xffffffff
#define	XB_LINK_ARB_LOWER_MASK		XBOWCONST 0xffffffff

#define	XBOW_WID_STAT_DEFAULT		XBOWCONST 0x00000000
#define	XBOW_WID_REQ_TO_DEFAULT		XBOWCONST 0x000fffff
#define	XB_LINK_STATUS_DEFAULT		XBOWCONST 0x80000000

/* misc masks */
#define	D_XBOW_WID_ID_REV_NUM_MASK	XBOWCONST 0xf0000000	
#define	D_XBOW_WID_INT_UPPER_ADDR	XBOWCONST 0x0000ffff	
#define	D_XBOW_WID_INT_UPPER_TARGID	XBOWCONST 0x000f0000	
#define	D_XBOW_WID_INT_UPPER_INTVECT	XBOWCONST 0xff000000	

/* other constants */
#define XBOW_ACC_REGS_MAX      0x3 /* # of cases to generate an access error */
#define	D_UPPER_ADDR_SHIFT	32 /* to shift address to get upper address */
#define D_XBOW_WID_REQ_TO XBOW_WID_REQ_TO_DEFAULT /* set to default for now */

#define	D_XBOW_ID			XBOWCONST 0x00000001 /* w/o rev # */
#define	D_XBOW_WID_STAT			XBOW_WID_STAT_DEFAULT

#define XBOW_WID0_BITS_CLR	( XB_WID_STAT_WIDGET0_INTR 		\
				| XB_WID_STAT_REG_ACC_ERR 		\
				| XB_WID_STAT_XTALK_ERR)

/*
 * macro definitions
 *	Read/Write like from/to XBOW.
 */
#define XB_REG_WR_32(address, mask, value) \
        PIO_REG_WR_32((address+XBOW_BASE), mask, value);

#define XB_REG_RD_32(address, mask, value) \
        PIO_REG_RD_32((address+XBOW_BASE), mask, value);

/* 
 * register structure for Xbow register tests
 */
typedef struct	_Xbow_Regs {
	char		*name;  	/* name of the register */
	__uint32_t	address;	/* address (really offset,can be 32b) */
	int		mode;	    	/* read / write only or read & write */
	__uint32_t	mask;	    	/* read-write mask */
	__uint32_t	def;    	/* default value */
} Xbow_Regs;

typedef	struct	 _Xbow_Regs_Access {
	char		*name;          /* name of the register */
	__uint64_t	register_addr;  /* register physical address */
        __uint32_t	access_mode;    /* access mode on the register */
} Xbow_Regs_Access;

#endif	/* __IDE_XBOW_H__ */
