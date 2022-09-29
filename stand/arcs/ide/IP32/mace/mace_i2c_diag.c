/***************************************************************************
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
 ***************************************************************************/

/*
 *  mace_i2c_diag.c
 *
 *  mace I2C diagnostic
 *
 *  $Revision: 1.2 $
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
/*
#include <stdio.h>
*/
#include <sys/mman.h>
#include <sys/mace.h>
#include <sys/IP32.h>
#include "mace_i2c_diags.h"
#include "uif.h"


/* Global variables */
volatile MaceI2CAddr *I2C_addr;
unsigned mace_i2c_diag_debug = 0;
unsigned err_status = 0;
unsigned int first_time = 0; 	/* can't set first_time, ide may run after unix */

i2c_reset()
{
	reg_t config_reset;
	reg_t cont_status;

	msg_printf(DBG, "i2c_reset\n");
	msg_printf(DBG, "Reading Bus Configuration and Reset Register (CRR)\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "CRR at 0x%x == 0x%llx\n",
		&(I2C_addr->i2c_config_reset), (config_reset&CRR_MASK));
	if (first_time && 
	    (((config_reset>>CRR_RESET_SHIFT)&0x1) != CRR_RESET_RESET_VALUE)) {
		msg_printf(ERR, "CRR RESET incorrect reset value,\n");
		msg_printf(ERR, "CRR RESET was 0x%llx, should be 0x%x\n", 
			((config_reset>>CRR_RESET_SHIFT) & 0x1),
			CRR_RESET_RESET_VALUE);
		err_status++;
	} else {
		if ( first_time ) {
			msg_printf(DBG, "CRR RESET correct reset value\n");
		} else {
		/* need to set it back to zero to reset */
		WRITE_REG64((long long)0, &I2C_addr->i2c_config_reset,
								long long);
		config_reset = READ_REG64( &I2C_addr->i2c_config_reset,
								long long);
		msg_printf(DBG, "RESET CRR at 0x%x == 0x%llx\n",
			&(I2C_addr->i2c_config_reset), (config_reset&CRR_MASK));
		}
	}
	first_time = 0;
	msg_printf(DBG, "Reading Control and Status Register (CSR)\n");
	cont_status = READ_REG64(&I2C_addr->i2c_cont_status, long long);
	msg_printf(DBG, "CSR at 0x%x == 0x%llx\n",
		&(I2C_addr->i2c_cont_status), (cont_status&CSR_MASK));
	config_reset |= (1<<CRR_RESET_SHIFT);
	msg_printf(DBG, "Writing CRR with 0x%llx\n", config_reset);
	WRITE_REG64(config_reset, &I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-read CRR value == 0x%llx\n", config_reset);
	msg_printf(DBG, "Re-reading CSR\n");
	cont_status = READ_REG64(&I2C_addr->i2c_cont_status, long long);
	msg_printf(DBG, "Re-read CSR value == 0x%llx\n",
					(cont_status&CSR_MASK));
	if ( !(config_reset & (1<<CRR_RESET_SHIFT)) ) {
		msg_printf(ERR, "CRR RESET cannot be set\n");
		err_status++;
	}
}

chk_reset_crr()
{
	reg_t config_reset;

	msg_printf(DBG, "chk_reset_crr\n");
	msg_printf(DBG, "Reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "CRR at 0x%x == 0x%llx\n",
		&(I2C_addr->i2c_config_reset), (config_reset&CRR_MASK));
	if (((config_reset>>CRR_FME_SHIFT) & 0x1) != CRR_FME_RESET_VALUE) {
		msg_printf(ERR,
		   "CRR Fast Mode Enable incorrect reset value,\n");
		msg_printf(ERR,
		   "CRR Fast Mode Enable was 0x%llx, should be 0x%x\n", 
			((config_reset>>CRR_FME_SHIFT) & 0x1),
			CRR_FME_RESET_VALUE);
		err_status++;
	} else {
		msg_printf(DBG, "CRR Fast Mode Enable correct reset value\n");
	}
	if (((config_reset>>CRR_DPO_SHIFT) & 0x1) != CRR_DPO_RESET_VALUE) {
		msg_printf(ERR,
		   "CRR Data Pin Override incorrect reset value,\n");
		msg_printf(ERR,
		   "CRR Data Pin Override was 0x%llx, should be 0x%x\n", 
			((config_reset>>CRR_DPO_SHIFT) & 0x1),
			CRR_DPO_RESET_VALUE);
		err_status++;
	} else {
		msg_printf(DBG, "CRR Data Pin Override correct reset value\n");
	}
	if (((config_reset>>CRR_CPO_SHIFT) & 0x1) != CRR_CPO_RESET_VALUE) {
		msg_printf(ERR,
		   "CRR Clock Pin Override incorrect reset value,\n");
		msg_printf(ERR,
		   "CRR Clock Pin Override was 0x%llx, should be 0x%x\n", 
			((config_reset>>CRR_CPO_SHIFT) & 0x1),
			CRR_CPO_RESET_VALUE);
		err_status++;
	} else {
		msg_printf(DBG, "CRR Clock Pin Override correct reset value\n");
	}
}

chk_reset_csr()
{
	reg_t cont_status;

	msg_printf(DBG, "chk_reset_csr\n");
	msg_printf(DBG, "Reading Control and Status Register (CSR)\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "CSR at 0x%x == 0x%llx\n",
			&(I2C_addr->i2c_cont_status), (cont_status&CSR_MASK));
	if ( ((cont_status>>CSR_FISC_SHIFT) & 0x1) != CSR_FISC_RESET_VALUE ) {
		msg_printf(ERR,
		   "CSR Force Idle State Control incorrect reset value,\n");
		msg_printf(ERR,
		   "CSR Force Idle State Control was 0x%llx, should be 0x%x\n", 
			((cont_status>>CSR_FISC_SHIFT) & 0x1),
			CSR_FISC_RESET_VALUE);
		err_status++;
	} else {
		msg_printf(DBG,
		   "CSR Force Idle State Control correct reset value\n");
	}
	if ( ((cont_status>>CSR_BDC_SHIFT) & 0x1) != CSR_BDC_RESET_VALUE ) {
		msg_printf(ERR,
		   "CSR Bus Direction Control incorrect reset value,\n");
		msg_printf(ERR,
		   "CSR Bus Direction Control was 0x%llx, should be 0x%x\n", 
			((cont_status>>CSR_BDC_SHIFT) & 0x1),
			CSR_BDC_RESET_VALUE);
		err_status++;
	} else {
		msg_printf(DBG,
		   "CSR Bus Direction Control correct reset value\n");
	}
	if ( ((cont_status>>CSR_LBC_SHIFT) & 0x1) != CSR_LBC_RESET_VALUE ) {
		msg_printf(ERR,
		   "CSR Last Byte Control incorrect reset value,\n");
		msg_printf(ERR,
		   "CSR Last Byte Control was 0x%llx, should be 0x%x\n", 
			((cont_status>>CSR_LBC_SHIFT) & 0x1),
			CSR_LBC_RESET_VALUE);
		err_status++;
	} else {
		msg_printf(DBG, "CSR Last Byte Control correct reset value\n");
	}
	if ( ((cont_status>>CSR_TS_SHIFT) & 0x1) != CSR_TS_RESET_VALUE ) {
		msg_printf(ERR, "CSR Transfer Status incorrect reset value,\n");
		msg_printf(ERR,
			"CSR Transfer Status was 0x%llx, should be 0x%x\n", 
			((cont_status>>CSR_TS_SHIFT) & 0x1),
			CSR_TS_RESET_VALUE);
		err_status++;
	} else {
		msg_printf(DBG, "CSR Transfer Status correct reset value\n");
	}
	if ( ((cont_status>>CSR_AS_SHIFT) & 0x1) != CSR_AS_RESET_VALUE ) {
		msg_printf(ERR,
		   "CSR Acknowledge Status incorrect reset value,\n");
		msg_printf(ERR,
		   "CSR Acknowledge Status was 0x%llx, should be 0x%x\n", 
			((cont_status>>CSR_AS_SHIFT) & 0x1),
			CSR_AS_RESET_VALUE);
		err_status++;
	} else {
		msg_printf(DBG, "CSR Acknowledge Status correct reset value\n");
	}
	if ( ((cont_status>>CSR_BES_SHIFT) & 0x1) != CSR_BES_RESET_VALUE ) {
		msg_printf(ERR,
		   "CSR Bus Error Status incorrect reset value,\n");
		msg_printf(ERR,
		   "CSR Bus Error Status was 0x%llx, should be 0x%x\n", 
			((cont_status>>CSR_BES_SHIFT) & 0x1),
			CSR_BES_RESET_VALUE);
		err_status++;
	} else {
		msg_printf(DBG, "CSR Bus Error Status correct reset value\n");
	}
}

chk_toggle_csr()
{
	reg_t config_reset;
	reg_t cont_status;

	msg_printf(DBG, "chk_toggle_csr\n");
	/* toggle Bus Direction Control bit */
	msg_printf(DBG, "Reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "CRR at 0x%x == 0x%llx\n",
		&(I2C_addr->i2c_config_reset), (config_reset&CRR_MASK));
	msg_printf(DBG, "Reading CSR\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "CSR at 0x%x == 0x%llx\n",
		&(I2C_addr->i2c_cont_status), (cont_status&CSR_MASK));
	cont_status |= (1<<CSR_BDC_SHIFT);
	msg_printf(DBG, "Writing CSR with 0x%llx\n", cont_status);
	WRITE_REG64(cont_status, &I2C_addr->i2c_cont_status, long long);
	msg_printf(DBG, "Re-reading CSR\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "Re-read CSR value == 0x%llx\n",
					(cont_status&CSR_MASK));
	if ( !(cont_status & (1<<CSR_BDC_SHIFT)) ) {
		msg_printf(ERR, "CSR BDC cannot be set\n");
		err_status++;
	}
	cont_status &= ~(1<<CSR_BDC_SHIFT);
	msg_printf(DBG, "Writing CSR with 0x%llx\n", cont_status);
	WRITE_REG64(cont_status, &I2C_addr->i2c_cont_status, long long);
	msg_printf(DBG, "Re-reading CSR\n");
	cont_status = READ_REG64(&I2C_addr->i2c_cont_status, long long);
	msg_printf(DBG, "Re-read CSR value == 0x%llx\n",
					(cont_status&CSR_MASK));
	if ( cont_status & (1<<CSR_BDC_SHIFT) ) {
		msg_printf(ERR, "CSR BDC cannot be cleared\n");
		err_status++;
		cont_status &= ~(1<<CSR_BDC_SHIFT);
	}
	/* toggle Last Byte Control bit */
	msg_printf(DBG, "Reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "CRR at 0x%x == 0x%llx\n",
		&(I2C_addr->i2c_config_reset), (config_reset&CRR_MASK));
	msg_printf(DBG, "Reading CSR\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "CSR at 0x%x == 0x%llx\n",
		&(I2C_addr->i2c_cont_status), (cont_status&CSR_MASK));
	cont_status |= (1<<CSR_LBC_SHIFT);
	msg_printf(DBG, "Writing CSR with 0x%llx\n", cont_status);
	WRITE_REG64(cont_status, &I2C_addr->i2c_cont_status, long long);
	msg_printf(DBG, "Re-reading CSR\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "Re-read CSR value == 0x%llx\n",
					(cont_status&CSR_MASK));
	if ( !(cont_status & (1<<CSR_LBC_SHIFT)) ) {
		msg_printf(ERR, "CSR LBC cannot be set\n");
		err_status++;
	}
	cont_status &= ~(1<<CSR_LBC_SHIFT);
	msg_printf(DBG, "Writing CSR with 0x%llx\n", cont_status);
	WRITE_REG64(cont_status, &I2C_addr->i2c_cont_status, long long);
	msg_printf(DBG, "Re-reading CSR\n");
	cont_status = READ_REG64(&I2C_addr->i2c_cont_status, long long);
	msg_printf(DBG, "Re-read CSR value == 0x%llx\n",
					(cont_status&CSR_MASK));
	if ( cont_status & (1<<CSR_LBC_SHIFT) ) {
		msg_printf(ERR, "CSR LBC cannot be cleared\n");
		err_status++;
		cont_status &= ~(1<<CSR_LBC_SHIFT);
	}
}

chk_toggle_crr()
{
	reg_t config_reset;
	reg_t cont_status;

	msg_printf(DBG, "chk_toggle_crr\n");
	/* clear CRR (set all bits 0, except RESET) */
	config_reset = (~((1<<CRR_FME_SHIFT)|(1<<CRR_DPO_SHIFT)|
				(1<<CRR_CPO_SHIFT))) & CRR_MASK;
	msg_printf(DBG, "Writing CRR with 0x%llx\n", config_reset);
	WRITE_REG64(config_reset, &I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-read CRR value == 0x%llx\n", config_reset);
	msg_printf(DBG, "Re-reading CSR\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "Re-read CSR value == 0x%llx\n",
					(cont_status&CSR_MASK));
	/* toggle Fast Mode Enable bit */
	msg_printf(DBG, "Reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "CRR at 0x%x == 0x%llx\n",
		&(I2C_addr->i2c_config_reset), (config_reset&CRR_MASK));
	msg_printf(DBG, "Reading CSR\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "CSR at 0x%x == 0x%llx\n",
		&(I2C_addr->i2c_cont_status), (cont_status&CSR_MASK));
	config_reset |= (1<<CRR_FME_SHIFT);
	msg_printf(DBG, "Writing CRR with 0x%llx\n", config_reset);
	WRITE_REG64(config_reset, &I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-read CRR value == 0x%llx\n", config_reset);
	msg_printf(DBG, "Re-reading CSR\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "Re-read CSR value == 0x%llx\n",
					(cont_status&CSR_MASK));
	if ( !(config_reset & (1<<CRR_FME_SHIFT)) ) {
		msg_printf(ERR, "CRR FME cannot be set\n");
		err_status++;
	}
	config_reset &= ~(1<<CRR_FME_SHIFT);
	msg_printf(DBG, "Writing CRR with 0x%llx\n", config_reset);
	WRITE_REG64(config_reset, &I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	if ( config_reset & (1<<CRR_FME_SHIFT) ) {
		msg_printf(ERR, "CRR FME cannot be cleared\n");
		err_status++;
		config_reset &= ~(1<<CRR_FME_SHIFT);
	}
	config_reset |= (1<<CRR_DPO_SHIFT);
	msg_printf(DBG, "Writing CRR with 0x%llx\n", config_reset);
	WRITE_REG64(config_reset, &I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-read CRR value == 0x%llx\n", config_reset);
	msg_printf(DBG, "Re-reading CSR\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "Re-read CSR value == 0x%llx\n",
					(cont_status&CSR_MASK));
	if ( !(config_reset & (1<<CRR_DPO_SHIFT)) ) {
		msg_printf(ERR, "CRR DPO cannot be set\n");
		err_status++;
	}
	config_reset &= ~(1<<CRR_DPO_SHIFT);
	msg_printf(DBG, "Writing CRR with 0x%llx\n", config_reset);
	WRITE_REG64(config_reset, &I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "mace_i2c_diag: Re-reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	if ( config_reset & (1<<CRR_DPO_SHIFT) ) {
		msg_printf(ERR, "CRR DPO cannot be cleared\n");
		err_status++;
		config_reset &= ~(1<<CRR_DPO_SHIFT);
	}
	config_reset |= (1<<CRR_CPO_SHIFT);
	msg_printf(DBG, "Writing CRR with 0x%llx\n", config_reset);
	WRITE_REG64(config_reset, &I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-read CRR value == 0x%llx\n", config_reset);
	msg_printf(DBG, "Re-reading CSR\n");
	cont_status = I2C_addr->i2c_cont_status;
	msg_printf(DBG, "Re-read CSR value == 0x%llx\n",
					(cont_status&CSR_MASK));
	if ( !(config_reset & (1<<CRR_CPO_SHIFT)) ) {
		msg_printf(ERR, "CRR CPO cannot be set\n");
		err_status++;
	}
	config_reset &= ~(1<<CRR_CPO_SHIFT);
	msg_printf(DBG, "Writing CRR with 0x%llx\n", config_reset);
	WRITE_REG64(config_reset, &I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-reading CRR\n");
	config_reset = READ_REG64(&I2C_addr->i2c_config_reset, long long);
	msg_printf(DBG, "Re-read CRR value == 0x%llx\n", config_reset);
	if ( config_reset & (1<<CRR_CPO_SHIFT) ) {
		msg_printf(ERR, "CRR CPO cannot be cleared\n");
		err_status++;
		config_reset &= ~(1<<CRR_CPO_SHIFT);
	}
}

unsigned int i2c_regtest()
{
	int i;
	int c;
	ulong checksum;
	volatile void *physAddr;
	extern int optind;

	msg_printf(VRB, "MACE I2C Register Test\n");
	msg_printf(DBG, "i2c_regtest\n");
	I2C_addr = (MaceI2CAddr *) PHYS_TO_K1(MACE_I2C);
	msg_printf(DBG, "mace_i2c_diag: I2C_addr=0x%x\n", I2C_addr);

	err_status = 0; /* 0 = pass, non-zero = fail */  

	i2c_reset();
	chk_reset_csr();
	chk_reset_crr();
	chk_toggle_csr();
	chk_toggle_crr();

	if ( err_status ) {
		msg_printf(DBG, "i2c_reg FAILED ( %d errors)\n", err_status);
		sum_error("MACE I2C Register");
	} else { 
		msg_printf(DBG, "i2c_reg PASSED\n");
		okydoky();
	}
	return(err_status);
}
