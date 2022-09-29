#ifndef __IDE_duart_H__
#define	__IDE_duart_H__

/*
 * d_duart.h
 *
 * IDE duart tests header 
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

#ident	"ide/godzilla/include/d_duart.h:  $Revision: 1.6 $"

#include "d_ioregs.h"

typedef unsigned char              duartregisters_t;

/*
 * duart Register Default Values
 * 
 * NOTE: these values are not all "after-power-on" values, as the prom init
 *	 is already run by the time ide is run.
 */
/*Parallal port*/

#define DU_SIO_UARTC_MASK		0xFF
#define DU_SIO_UARTC_DEFAULT		0x0

#define DU_SIO_KBDCG_MASK		0xFF
#define DU_SIO_KBDCG_DEFAULT		0x0

#define DU_SIO_PP_DATA_MASK		0xFF
#define DU_SIO_PP_DATA_DEFAULT		0x0

#define DU_SIO_PP_DSR_MASK		0xFF
#define DU_SIO_PP_DSR_DEFAULT		0x0

#define DU_SIO_PP_DCR_MASK		0x0F
#define DU_SIO_PP_DCR_DEFAULT		0x0

#define DU_SIO_PP_FIFA_MASK		0xFF
#define DU_SIO_PP_FIFA_DEFAULT		0x0

#define DU_SIO_PP_CFGB_MASK		0xFF
#define DU_SIO_PP_CFGB_DEFAULT		0x0

#define DU_SIO_PP_ECR_MASK		0xFF
#define DU_SIO_PP_ECR_DEFAULT		0x0

#define DU_SIO_RTCAD_MASK		0xFF
#define DU_SIO_RTCAD_DEFAULT		0x0

#define DU_SIO_RTCDAT_MASK		0xFF
#define DU_SIO_RTCDAT_DEFAULT		0x0

#define DU_SIO_UB_THOLD_MASK		0xFF
#define DU_SIO_UB_THOLD_DEFAULT		0x0

#define DU_SIO_UB_RHOLD_MASK		0xFF
#define DU_SIO_UB_RHOLD_DEFAULT		0x0

#define DU_SIO_UB_DIV_LSB_MASK		0xFF
#define DU_SIO_UB_DIV_LSB_DEFAULT	0x0

#define DU_SIO_UB_DIV_MSB_MASK		0xFF
#define DU_SIO_UB_DIV_MSB_DEFAULT	0x0

#define DU_SIO_UB_IENB_MASK		0xFF
#define DU_SIO_UB_IENB_DEFAULT		0x0

#define DU_SIO_UB_IIDENT_MASK		0xFF
#define DU_SIO_UB_IIDENT_DEFAULT	0x0

#define DU_SIO_UB_FIFOC_MASK		0xFF
#define DU_SIO_UB_FIFOC_DEFAULT		0x0

#define DU_SIO_UB_LINEC_MASK		0xFF
#define DU_SIO_UB_LINEC_DEFAULT		0x0

#define DU_SIO_UB_MODEMC_MASK		0xFF
#define DU_SIO_UB_MODEMC_DEFAULT	0x0

#define DU_SIO_UB_LINES_MASK		0xFF
#define DU_SIO_UB_LINES_DEFAULT		0x0

#define DU_SIO_UB_MODEMS_MASK		0xFF
#define DU_SIO_UB_MODEMS_DEFAULT	0x0

#define DU_SIO_UB_SCRPAD_MASK		0xFF
#define DU_SIO_UB_SCRPAD_DEFAULT	0x0

#define DU_SIO_UA_THOLD_MASK		0xFF
#define DU_SIO_UA_THOLD_DEFAULT		0x0

#define DU_SIO_UA_RHOLD_MASK		0xFF
#define DU_SIO_UA_RHOLD_DEFAULT		0x0

#define DU_SIO_UA_DIV_LSB_MASK		0xFF
#define DU_SIO_UA_DIV_LSB_DEFAULT	0x0

#define DU_SIO_UA_DIV_MSB_MASK		0xFF
#define DU_SIO_UA_DIV_MSB_DEFAULT	0x0

#define DU_SIO_UA_IENB_MASK		0xFF
#define DU_SIO_UA_IENB_DEFAULT		0x0

#define DU_SIO_UA_IIDENT_MASK		0xFF
#define DU_SIO_UA_IIDENT_DEFAULT	0x0

#define DU_SIO_UA_FIFOC_MASK		0xFF
#define DU_SIO_UA_FIFOC_DEFAULT		0x0

#define DU_SIO_UA_LINEC_MASK		0xFF
#define DU_SIO_UA_LINEC_DEFAULT		0x0

#define DU_SIO_UA_MODEMC_MASK		0xFF
#define DU_SIO_UA_MODEMC_DEFAULT	0x0

#define DU_SIO_UA_LINES_MASK		0xFF
#define DU_SIO_UA_LINES_DEFAULT		0x0

#define DU_SIO_UA_MODEMS_MASK		0xFF
#define DU_SIO_UA_MODEMS_DEFAULT	0x0

#define DU_SIO_UA_SCRPAD_MASK		0xFF
#define DU_SIO_UA_SCRPAD_DEFAULT	0x0
/* 
 * macro definitions: yes, the BR_xxxx_32 Reads and Writes are really 32 bits...
 * 	register access macros depend bridge_wid_no to be set to
 *	the correct xbow port id. Default is XBOW_PORT_F
 *	PIO_REG_WR_32(address, mask, value);
 *	PIO_REG_RD_32(address, mask, value);
 */	
                                         
/*
 * constants used in duart_regs.c
 */
#define	DUART_REGS_PATTERN_MAX	6

typedef struct	_duart_Registers {
	char		*name;  	/* name of the register */
	__uint32_t	address;	/* address */
	__uint32_t	mode;	    	/* read / write only or read & write */
	unsigned char	mask;	    	/* read-write mask */
	unsigned char	def;    	/* default value */
} duart_Registers;

#endif	/* __IDE_duart_H__ */

