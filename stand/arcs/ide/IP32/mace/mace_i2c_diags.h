/*****************************************************************************
 * $Id: mace_i2c_diags.h,v 1.1 1996/01/24 18:50:37 montep Exp $
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 *****************************************************************************/

/*
 * Moosehead MACE I2C Diagnostic Defines
 *
 */

#ifdef __cplusplus
/*extern "C" {*/
#endif

/*
#include "quirks.h"
*/
#include <sys/mace_regs.h>


/* added from cv.h */
#define SUCCESS	0
#define FAILURE	-1

#define MAX_DELAY_LOOP 5
#define PAGES_TO_ALLOCATE 1

/*
 *  Miscellaneous type definitions
 */

typedef volatile unsigned long vol_ulong;

#define MACE_REG_READ(vinohw, reg) \
	    MACE_REG_READ_PHYS(&(vinohw->reg))
	    
#define MACE_REG_WRITE(vinohw, reg, val) \
	    MACE_REG_WRITE_PHYS(&(vinohw->reg), (val))

/*
 *  Macros that take a register address that is on 64 bit offsets.
 *  To access the low word of the 64 bit double word we add 4 to the
 *  address and access the register.  If we have to do double word
 *  accesses then an assembly function will be called with the 64 bit
 *  address directly.
 */

#define MACE_REG_READ_PHYS(addr)	\
	    (*((ulong *)((char *)addr + 4)))

#define MACE_REG_WRITE_PHYS(addr, val)	\
	    (*((ulong *)((char *)addr + 4)) = (val))



/*
 *  MACE ASIC - I2C Control and Status Register
 */

#define I2C_FORCE_IDLE  (0 << 0)

#define I2C_IS_IDLE(status)     (((status) & I2C_NOT_IDLE) == 0)
#define I2C_NOT_IDLE    	(1 << 0)
#define I2C_WRITE       	(0 << 1)
#define I2C_READ        	(1 << 1)
#define I2C_RELEASE_BUS 	(0 << 2)
#define I2C_HOLD_BUS    	(1 << 2)
#define I2C_XFER_DONE   	(0 << 4)
#define I2C_XFER_BUSY   	(1 << 4)
#define I2C_ACK         	(0 << 5)
#define I2C_NACK        	(1 << 5)
#define I2C_BUS_OK      	(0 << 7)
#define I2C_BUS_ERR     	(1 << 7)

/*
 * I2C bus slave addresses
 */

#define DMSD_ADDR	0x8A
#define CDMC_ADDR_ASIC	0x56
#define CDMC_ADDR_FPGA	0xAE

#define I2C_WRITE_ADDR	0x0
#define I2C_READ_ADDR	0x1

#define I2C_OK			0
#define I2C_ERROR		-1

/*
 *  MaceI2cWriteBlock() definitions
 */

#define	END_BLOCK		-1	/* flag to mark the end of reg list */
#define SKIP_RO_REG		-2	/* flag to mark a read only reg */

#ifdef __cplusplus
/*}*/
#endif

/* === */
