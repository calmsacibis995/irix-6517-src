#ifndef __IDE_HEART_H__
#define	__IDE_HEART_H__

/*
 * d_heart.h
 *
 * IDE heart tests header
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

#ident	"ide/godzilla/include/d_heart.h:  $Revision: 1.23 $"

/*
 * Heart Register Read-Write Masks
 */
#define	HEART_WID_ID_MASK		HEARTCONST 0x0000000000000001
#define	HEART_WID_STAT_MASK		HEARTCONST 0x00000000ffff001f
#define	HEART_WID_ERR_UPPER_MASK	HEARTCONST (((HEARTCONST 1)<<16) - 1)
#define	HEART_WID_ERR_LOWER_MASK	HEARTCONST (((HEARTCONST 1)<<32) - 1)
#define	HEART_WID_CONTROL_MASK		HEARTCONST (((HEARTCONST 1)<<32) - 1)
#define	HEART_WID_REQ_TO_MASK		HEARTCONST 0x00000000000fffff
#define	HEART_WID_ERR_CMDWORD_MASK	HEARTCONST (((HEARTCONST 1)<<32) - 1)
#define	HEART_WID_LLP_MASK		HEARTCONST (((HEARTCONST 1)<<26) - 1)
#define	HEART_WID_TARG_FLUSH_MASK	HEARTCONST (((HEARTCONST 1)<<32) - 1)
#define	HEART_WID_ERR_TYPE_MASK		HEARTCONST 0x000000003ff7fc0f
#define	HEART_WID_ERR_MASK_MASK		HEARTCONST 0x00000000ffffffff
#define	HEART_WID_PIO_ERR_UPPER_MASK	HEARTCONST 0x00000000fff7ffff
#define	HEART_WID_PIO_ERR_LOWER_MASK	HEARTCONST 0x00000000ffffffff
#define	HEART_WID_PIO_RTO_ADDR_MASK	HEARTCONST 0x00000000000fffff
#define	HEART_MODE_MASK			HEARTCONST 0xffff0fff3fffffff
#define	HEART_SDRAM_MODE_MASK		HEARTCONST 0x00000000000fffff
#define	HEART_MEM_REF_MASK		HEARTCONST 0x00000000000fffff
#define	HEART_MEM_ARB_MASK		HEARTCONST 0x00000000001fffff
/* no access to the memory config reg. */
#define	HEART_MEMCFG0_MASK		HEARTCONST 0x803f01ff803f01ff 
#define	HEART_MEMCFG1_MASK		HEARTCONST 0x803f01ff803f01ff
#define	HEART_MEMCFG2_MASK		HEARTCONST 0x803f01ff803f01ff
#define	HEART_MEMCFG3_MASK		HEARTCONST 0x803f01ff803f01ff
#define	HEART_FC_MODE_MASK		HEARTCONST (((HEARTCONST 1)<<14) - 1)
#define	HEART_FC_LIMIT_MASK		HEARTCONST (((HEARTCONST 1)<< 26) - 1)
#define	HEART_FC0_ADDR_MASK		HEARTCONST 0xffffffffffffffe0
#define	HEART_FC1_ADDR_MASK		HEARTCONST 0xffffffffffffffe0
#define	HEART_FC0_COUNT_MASK		HEARTCONST (((HEARTCONST 1)<<12) - 1)
#define	HEART_FC1_COUNT_MASK		HEARTCONST (((HEARTCONST 1)<<12) - 1)
#define	HEART_FC0_TIMER_MASK		HEARTCONST (((HEARTCONST 1)<<26) - 1)
#define	HEART_FC1_TIMER_MASK		HEARTCONST (((HEARTCONST 1)<<26) - 1)
#if SABLE
#define	HEART_STATUS_MASK		HEARTCONST (((HEARTCONST 1)<<32) - 1)
#else
#define	HEART_STATUS_MASK		HEARTCONST 0x00000000ffff00ff	/* mask off UsedCrdCnt */
#endif /* SABLE */
#define	HEART_BERR_ADDR_MASK		HEARTCONST (((HEARTCONST 1)<<40) - 1)
#define	HEART_BERR_MISC_MASK		HEARTCONST (((HEARTCONST 1)<<24) - 1)
#if SABLE
#define	HEART_MEM_ERR_MASK		HEARTCONST (((HEARTCONST 1)<<46) - 1)
#else
#define	HEART_MEM_ERR_MASK		HEARTCONST 0x0  /* this register could contain anything */
#endif /* SABLE */
#define	HEART_BAD_MEM_MASK		HEARTCONST 0x0	/* this register could contain anything */
#define	HEART_PIU_ERR_MASK		HEARTCONST (((HEARTCONST 1)<<23) - 1)
#define	HEART_MLAN_CLK_DIV_MASK		HEARTCONST 0x0000000000007f7f
/* NOTE: the HEART_MLAN_CTL register does not retain the value written in its offset */
/*	  field (9:2) but decrements it with time. Hence mask it */
#define	HEART_MLAN_CTL_MASK		HEARTCONST 0x00000000000ffc02
#define	HEART_IMR0_MASK			HEARTCONST ~0x0
#define	HEART_IMR1_MASK			HEARTCONST ~0x0
#define	HEART_IMR2_MASK			HEARTCONST ~0x0
#define	HEART_IMR3_MASK			HEARTCONST ~0x0
#define	HEART_CAUSE_MASK		HEARTCONST 0xf0000ffffff100f7
#define	HEART_COMPARE_MASK		HEARTCONST 0x0000000000ffffff

/*
 * Index of interrupt registers (used in functionality tests)
 */
#define	HEART_REGS_PIU_MAX	0x4 /* # of cases to generate a PIU */
                                            /*  access error */
/* these values are consistent with the Access type bit in PIU Acc Error Reg */
#define	DO_A_READ	0x0	/* used for PIU Access error tests */
#define	DO_A_WRITE	0x1	/* used for PIU Access error tests */

/*
 * constants used in hr_regs.c
 */
#define	HR_REGS_PATTERN_MAX	6

/*
 * constants used in hr_intr.c
 */
#define	INT_STAT_VECT_EXC	63	/* in ISR */
#define	INT_STAT_VECT_FC_TO_1	2	/* 2 in ISR */
/* to mask immune bits: HEART_Exc, PPErr(3:0), Timer_IP, FC_TimeOut(1:0), IR. */
#define INT_STAT_IMMUNE_MSK	0x07fbfffffffffff8 /* immune to set&clear */

/*
 * constants used in hb_flow_ctrl.c
 */
#define	FC_PASSED	0
#define	FC_FAILED	1
#define	HEAD_0		0
#define	HEAD_1		1

/*
 * Heart Register Default Values
 */
#define	D_HEART_STATUS		HEARTCONST 0x00000060	/* XXX */

typedef struct	_Heart_Regs {
	char		*name;  	/* name of the register */
	__psunsigned_t	address;	/* address */
	__uint64_t	mask;	    	/* read-write mask */
	int		mode;		/* read / write only or read & write */
} Heart_Regs;

typedef struct  _Heart_Regs_PIU_Access {
        char            *name;          /* name of the register */
        __uint64_t      register_addr;  /* PIU register physical address */
        __uint32_t      access_mode;    /* access mode on the PIU register */
} Heart_Regs_PIU_Access;

#endif	/* __IDE_HEART_H__ */

