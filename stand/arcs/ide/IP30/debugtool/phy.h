
#ifndef __IDE_phy_
#define	__IDE_phy_

/*
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

#ident	"ide/godzilla/include/d_phyregs.h:  $Revision: 1.1 $"

#define PHY_CONTROL			0x0
#define PHY_STATUS			0x1	
#define PHY_ID1				0x2
#define PHY_ID2				0x3
#define PHY_AUTO_NEG_ADD		0x4
#define PHY_AUTO_NEG_LINK_PTNR		0x5
#define PHY_AUTO_NEG_EXP		0x6
#define PHY_EXT_CONTROL			0x16
#define PHY_QUICKPOLL_STATUS		0x17
#define PHY_10BASET			0x18
#define PHY_EXT_CONTROL2		0x19

#define PHY_CONTROL_MASK			0xFFFF
#define PHY_CONTROL_DEFAULT			0x0

#define PHY_STATUS_MASK				0xFFFF
#define PHY_STATUS_DEFAULT			0x0

#define PHY_ID1_MASK				0xFFFF
#define PHY_ID1_DEFAULT				0x15

#define PHY_ID2_MASK				0xFFFF
#define PHY_ID2_DEFAULT				0xF422

#define PHY_AUTO_NEG_ADD_MASK			0xFFFF
#define PHY_AUTO_NEG_ADD_DEFAULT		0x1e1

#define PHY_AUTO_NEG_LINK_PTNR_MASK		0xFFFF
#define PHY_AUTO_NEG_LINK_PTNR_DEFAULT		0x0

#define PHY_AUTO_NEG_EXP_MASK			0xFFFF
#define PHY_AUTO_NEG_EXP_DEFAULT		0x0

#define PHY_EXT_CONTROL_MASK			0xFFFF
#define PHY_EXT_CONTROL_DEFAULT			0x0

#define PHY_QUICKPOLL_STATUS_MASK		0xFFFF
#define PHY_QUICKPOLL_STATUS_DEFAULT		0x0

#define PHY_10BASET_MASK			0x3F
#define PHY_10BASET_DEFAULT			0x0

#define PHY_EXT_CONTROL2_MASK			0xFFFF
#define PHY_EXT_CONTROL2_DEFAULT		0x0

#define PHY_BUSY 0x800
#define PHY_ADDR 0x020
#define PHY_READ 0x400
#define PHY_WRITE 0x000

typedef __uint32_t              phyregisters_t;

/*phy PCI configuration space*/

#define PHY_AUTO_NEG_AD			0x4  ADD here:wq			

/*
 * constants used in phy_regs.c
 */
#define	PHY_REGS_PATTERN_MAX	6

typedef struct	_phy_Registers {
	char		*name;  	/* name of the register */
	__uint32_t	address;	/* address */
	__uint32_t	mode;	    	/* read / write only or read & write */
	__uint32_t	mask;	    	/* read-write mask */
	__uint32_t	def;    	/* default value */
} phy_Registers;

#endif	/* __IDE_phy_ */
