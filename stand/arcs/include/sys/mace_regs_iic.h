/*****************************************************************************
 * $Id: mace_regs_iic.h,v 1.1 1996/01/18 17:27:54 montep Exp $
 *
 * Copyright 1993, Silicon Graphics, Inc.
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
 * Moosehead MACE I2C Registers
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

/* === */

#ifdef __cplusplus
/*extern "C" {*/
#endif

#ifndef	__MACE_REG_T
#define	__MACE_REG_T
typedef unsigned long long reg_t;
#endif	/* __MACE_REG_T */

/*
 * Control and Status Register (CSR) Definitions
 */

/* CSR bit shift counts */
#define CSR_MASK	0xff	/* mask for CSR */
#define CSR_FISC_SHIFT	0	/* force idle state control */
#define CSR_BDC_SHIFT	1	/* bus direction control */
#define CSR_LBC_SHIFT	2	/* last byte control */
#define CSR_RSVD_SHIFT	3	/* reserved */
#define CSR_TS_SHIFT	4	/* transfer status (read only) */
#define CSR_AS_SHIFT	5	/* acknowledge status (read only) */
#define CSR_RSVD1_SHIFT	6	/* reserved */
#define CSR_BES_SHIFT	7	/* bus error status (read only) */

/* CSR reset values for each bit */
#define CSR_FISC_RESET_VALUE	0	/* force idle state control */
#define CSR_BDC_RESET_VALUE	0	/* bus direction control */
#define CSR_LBC_RESET_VALUE	0	/* last byte control */
#define CSR_RSVD_RESET_VALUE	0	/* reserved */
#define CSR_TS_RESET_VALUE	0	/* transfer status */
#define CSR_AS_RESET_VALUE	0	/* acknowledge status */
#define CSR_RSVD1_RESET_VALUE	0	/* reserved */
#define CSR_BES_RESET_VALUE	0	/* bus error status */

/*
 * Configuration and Reset Register (CRR) Definitions
 */

/* CRR bit shift counts */
#define CRR_MASK	0x3f	/* mask for CSR */
#define CRR_RESET_SHIFT	0	/* RESET */
#define CRR_FME_SHIFT	1	/* Fast mode enable */
#define CRR_DPO_SHIFT	2	/* Data pin override */
#define CRR_CPO_SHIFT	3	/* Clock pin override */
#define CRR_DICV_SHIFT	4	/* Data input current value (read only) */
#define CRR_CICV_SHIFT	5	/* Clock input current value (read only) */

/* CRR reset values for each bit */
#define CRR_RESET_RESET_VALUE	0	/* RESET */
#define CRR_FME_RESET_VALUE	0	/* Fast mode enable */
#define CRR_DPO_RESET_VALUE	0	/* Data pin override */
#define CRR_CPO_RESET_VALUE	0	/* Clock pin override */
#define CRR_DICV_RESET_VALUE	X	/* Data input current value */
					/* no reset value */
#define CRR_CICV_RESET_VALUE	X	/* Clock input current value */
					/* no reset value */

typedef struct cont_stat_reg_s {
    reg_t	i2cpad:56;				/* 63:8 */
    reg_t	i2c_bus_err_status:1;		/* 7 */
    reg_t	i2c_rsvd1:1;			/* 6 */
    reg_t	i2c_ack_status:1;		/* 5 */
    reg_t	i2c_xfer_status:1;		/* 4 */
    reg_t	i2c_rsvd2:1;			/* 3 */
    reg_t	i2c_last_byte_control:1;	/* 2 */
    reg_t	i2c_bus_dir_control:1;		/* 1 */
    reg_t	i2c_force_idle_control:1;	/* 0 */
} cont_stat_reg_s;

typedef struct config_reset_reg_s {
    reg_t	i2cpad1:58;			/* 63:6 */
    reg_t	i2c_clk_input_current:1;	/* 5 */
    reg_t	i2c_data_input_current:1;	/* 4 */
    reg_t	i2c_clk_pin_ovrrde:1;		/* 3 */
    reg_t	i2_data_pin_ovrrde:1;		/* 2 */
    reg_t	i2c_fast_mode_enable:1;		/* 1 */
    reg_t	i2c_reset:1;			/* 0 */
} config_reset_reg_s;

typedef union config_reset_reg_u {
    reg_t		word;
    config_reset_reg_s	bits;
} config_reset_reg_t;

#define	I2C_MACE_ADDR	(void *)((unsigned long)(MACE_OFFSET) | \
				(unsigned long)(MACE_OFFSET_IIC))
#define	I2C_PHYS_ADDR	(void *)((unsigned long)(I2C_MACE_ADDR) | \
				(unsigned long)(0xA0000000))

typedef struct {
	reg_t	i2c_config_reset;
	reg_t	i2c_rsvd1;
	reg_t	i2c_cont_status;
	reg_t	i2c_data;
} MaceI2CAddr;

#ifdef __cplusplus
/*}*/
#endif

/* === */
