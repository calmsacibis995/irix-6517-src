/***************************************************************************
 *
 * Copyright 1991, Silicon Graphics, Inc.
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
 *  vino_i2c.c
 *
 *  VINO Video driver
 *
 *  $Revision: 1.7 $
 */

/*
 *  Externally visible functions and global variables start with a 
 *  prefix of vino.  Static functions start with a prefix of i2c.
 */

#include "sys/types.h"
#include "sys/immu.h"
#include "sys/cpu.h"
#include <libsk.h>

#include "uif.h"

#include "./vino_diags.h"
#include "vino.h"

/* Empirically derived i2c bus timeout constants */

#define ACK_WAIT_TIMEOUT	1000
#define XFER_WAIT_TIMEOUT	200
#define IDLE_WAIT_TIMEOUT	200

/* global variables */

int	vino_i2c_debug = 0;

extern	ulong	CDMC_addr;


/*
 *  i2cWaitForAck
 *
 *  Waits for the slave chip on the i2c bus to acknowledge a data byte
 *  transfer from the vino asic which is the i2c bus master.  An error
 *  is returned if the ack is not received in a specified time.
 */
  
int
i2cWaitForAck (VinoRegs vino_regs)
{
    int ack_wait = ACK_WAIT_TIMEOUT;

    while (VINO_REG_READ(vino_regs, i2c_control) & I2C_NACK && ack_wait) {
	ack_wait--;
	DELAY(1);
    }

    /* did the ack timeout occur, if so report an error */

    if (!ack_wait){
	if (vino_i2c_debug)
	    msg_printf (DBG, "i2cWaitForAck: ACK timeout ERROR\n");
	return (I2C_ERROR);
    }
    
    msg_printf(DBG, "ACK RCVD\n");
    return (I2C_OK);
}

/*
 *  i2cWaitForXferDone
 *
 *  Waits for the vino asic to indicate that a data byte read transfer
 *  from a slave device to the vino asic master chip has completed.
 *  An error is returned if the read transfer exceeds a specified time.
 */
  
int
i2cWaitForXferDone (VinoRegs vino_regs)
{
    int xfer_wait = XFER_WAIT_TIMEOUT;

    while (VINO_REG_READ(vino_regs,i2c_control) & I2C_XFER_BUSY && xfer_wait){
	xfer_wait--;
	DELAY(1);
    }

    /* did the xfer done timeout occur, if so report an error */

    if (!xfer_wait) {
	if (vino_i2c_debug)
	    msg_printf(DBG, "i2cWaitForXferDone: Xfer Done timeout ERROR\n");
	return (I2C_ERROR);
    }

    return (I2C_OK);
}

/*
 *  i2cForceBusIdle
 *
 *  If the i2c bus is not idle then force it to an idle state.  Then
 *  loop waiting for the control/status reg to indicate that the i2c bus
 *  has become idle.  If the bus does not become idle within a timeout 
 *  period or if a bus error occurs then an error value is returned.
 */

int
i2cForceBusIdle (VinoRegs vino_regs)
{
    int idle_wait = IDLE_WAIT_TIMEOUT;

    /* check to see if the i2c bus is already idle, if so then just return */

    if ( I2C_IS_IDLE(VINO_REG_READ(vino_regs, i2c_control)) ) {
	msg_printf(DBG, "BUS WAS IDLE\n");
	return (I2C_OK);
    }

    /* i2c is not idle so force it to be idle */

    VINO_REG_WRITE(vino_regs, i2c_control, I2C_FORCE_IDLE);

    /* wait until it is idle or a timeout occurs. */

    while (VINO_REG_READ(vino_regs, i2c_control) & I2C_NOT_IDLE && idle_wait) {
	idle_wait--;
	DELAY(1); 
    }

    /* did a bus idle timeout occur, if so then report an error */

    if (!idle_wait) {
	if (vino_i2c_debug)
	    msg_printf (DBG, "i2cForceBusIdle: Timeout ERROR\n");
	return (I2C_ERROR);
    }

    /* report an error if an i2c bus error occurred */

    if (VINO_REG_READ(vino_regs, i2c_control) & I2C_BUS_ERR) {
	if (vino_i2c_debug)
	    msg_printf(DBG, "i2cForceBusIdle: Bus ERROR\n");
	return (I2C_ERROR);
    }

    /* everything is ok and the bus is idle */
    msg_printf(DBG, "BUS WAS FORCED IDLE\n");
    return (I2C_OK);
}

/*
 *  vinoI2cGetStatus
 *
 *  Do the appropriate i2c bus handshake to read the status register in 
 *  the specified i2c chip.  Return I2C_FAILURE if any errors are detected. 
 *  If no errors are detected then return the status byte.
 */

int
vinoI2cReadStatus (VinoBoard *vino_bd, int i2c_chip_addr)
{
    int reg_addr;

    if (i2c_chip_addr != DMSD_ADDR && i2c_chip_addr != CDMC_addr)
	return (FAILURE);

    if (i2c_chip_addr == DMSD_ADDR)
	reg_addr = DMSD_REG_STATUS;
    else
	reg_addr = CDMC_REG_CTRL_STATUS;

    return ( vinoI2cReadReg ( vino_bd, i2c_chip_addr, reg_addr) );
}

/*
 *  vinoI2cReadReg
 *
 *  Read the specified register in the specified i2c chip.
 *  If any i2c bus handshake errors occur then report an error.
 */

int
vinoI2cReadReg (VinoBoard *vino_bd, int i2c_chip_addr, int reg_addr)
{
    VinoRegs vino_regs = vino_bd->hw_addr;
    ulong   val;
    
    msg_printf(DBG, "in vinoI2cReadReg\n");
    /* Verify the i2c chip address is legal */

    if (i2c_chip_addr != DMSD_ADDR && i2c_chip_addr != CDMC_addr)
	return (FAILURE);

    /*
     *  The 7191 DMSD chip is write only except for the status reg.
     *  If the i2c chip is the 7191 and the reg address is not the
     *  status reg then get the value from a shadow reg array and
     *  return it.
     */

    if (i2c_chip_addr == DMSD_ADDR && reg_addr != DMSD_REG_STATUS)
	if (reg_addr >= 0 && reg_addr < SHADOW_7191_REG_SIZE)
	    return (vino_bd->shadow_7191_regs [reg_addr]);
	else
	    return (FAILURE);

    if (i2c_chip_addr == CDMC_addr && reg_addr > CDMC_REG_READ_ADDR_MAX)
	if (reg_addr != CDMC_REG_VERSION)
	    return (FAILURE);

    /* The reg addr is a readable register so do the i2c reg read */

    if (i2cForceBusIdle (vino_regs) == I2C_ERROR)
	return (FAILURE);

    /* Put the vino i2c master in the write state */

    VINO_REG_WRITE(vino_regs, i2c_control,  I2C_HOLD_BUS | I2C_WRITE_ADDR | I2C_NOT_IDLE);	

    /*
     *  If the i2c chip is the camera controller (CDMC) then we have
     *  to set the reg subaddress before initiating the read.  The
     *  7191 status register does not require a subaddress to be set.
     */

    if (i2c_chip_addr == CDMC_addr) {

        /* Write out the i2c chip address and specify a write operation */

	VINO_REG_WRITE(vino_regs, i2c_data, i2c_chip_addr | I2C_WRITE_ADDR);
	
	if (i2cWaitForXferDone (vino_regs) == I2C_ERROR)
	    return (FAILURE);
	    
	if (i2cWaitForAck (vino_regs) == I2C_ERROR)
	    return (FAILURE);

	/* write the register subaddress */

	VINO_REG_WRITE(vino_regs, i2c_data, reg_addr);
	
	if (i2cWaitForXferDone (vino_regs) == I2C_ERROR)
	    return (FAILURE);
	    
	if (i2cWaitForAck(vino_regs) == I2C_ERROR)
	    return (FAILURE);
	    
	VINO_REG_WRITE(vino_regs, i2c_control, I2C_RELEASE_BUS);

	if (i2cForceBusIdle (vino_regs) == I2C_ERROR)
	    return (FAILURE);
    
	/* Put the vino i2c master in the write state */
    
	VINO_REG_WRITE(vino_regs, i2c_control, I2C_HOLD_BUS | I2C_NOT_IDLE);	
    }

    /* Write out the i2c chip address and specify a read operation */

    VINO_REG_WRITE(vino_regs, i2c_data, i2c_chip_addr | I2C_READ_ADDR);
    
    if (i2cWaitForXferDone (vino_regs) == I2C_ERROR)
	return (FAILURE);
	
    if (i2cWaitForAck (vino_regs) == I2C_ERROR)
	return (FAILURE);

    /* Tell vino to do an i2c bus READ operation and wait for the data */

    VINO_REG_WRITE(vino_regs, i2c_control, I2C_READ | I2C_NOT_IDLE);	

    if (i2cWaitForXferDone (vino_regs) == I2C_ERROR)
	return (FAILURE);

    val = VINO_REG_READ(vino_regs, i2c_data);
    
    VINO_REG_WRITE(vino_regs, i2c_control, I2C_RELEASE_BUS | I2C_FORCE_IDLE | I2C_WRITE);

    /* return the value read form the i2c register */

    return (val);
}

/*
 *  i2cWriteAddrSubAddr
 *
 *  Write out the i2c chip address in write mode and the reg subaddress.
 *  If any i2c bus handshake errors are detected then report an error.
 */

int
i2cWriteAddrSubAddr (VinoRegs vino_regs, int i2c_chip_addr, int reg_addr)
{
    /* Verify the i2c chip address is legal */
    msg_printf(DBG, "in i2cWriteAddrSubAddr\n");
    if (i2c_chip_addr != DMSD_ADDR && i2c_chip_addr != CDMC_addr)
	return (FAILURE);

    /* Verify the reg subaddress is legal */

    if (i2c_chip_addr == DMSD_ADDR && reg_addr >= SHADOW_7191_REG_SIZE)
	return (FAILURE);

    if (i2c_chip_addr == CDMC_addr && reg_addr > CDMC_REG_WRITE_ADDR_MAX) {

	/* Above the CDMC reg max value is the master reset reg */

	if (reg_addr != CDMC_REG_MASTER_RESET)
	    return (FAILURE);
    }

    /* The camera brightness reg is read only so don't allow writes to it */

    if (i2c_chip_addr == CDMC_addr && reg_addr == CDMC_REG_BRIGHTNESS)
	return (FAILURE);

    if (i2cForceBusIdle (vino_regs) == I2C_ERROR)
	return (FAILURE);

    /* Write out the i2c chip address and specify a write operation */

    VINO_REG_WRITE(vino_regs, i2c_control, I2C_HOLD_BUS | I2C_WRITE_ADDR | I2C_NOT_IDLE);	
    VINO_REG_WRITE(vino_regs, i2c_data, i2c_chip_addr | I2C_WRITE_ADDR);

    if (i2cWaitForXferDone (vino_regs) == I2C_ERROR)
	return (FAILURE);

    if (i2cWaitForAck (vino_regs) == I2C_ERROR)
	return (FAILURE);

    /* write the register subaddress */
    VINO_REG_WRITE(vino_regs, i2c_data, reg_addr);		

    if (i2cWaitForXferDone (vino_regs) == I2C_ERROR)
	return (FAILURE);

    if (i2cWaitForAck(vino_regs) == I2C_ERROR)
	return (FAILURE);

    return (SUCCESS);
}

/*
 *  vinoI2cWriteReg
 *
 *  Write out the specified value to the register in the specified
 *  i2c chip.  If any i2c bus handshake errors occur then report an
 *  error.
 */

int
vinoI2cWriteReg (VinoBoard *vino_bd, int i2c_chip_addr, int reg_addr, int val)
{
    VinoRegs vino_regs = vino_bd->hw_addr;
    msg_printf(DBG, "in vinoI2cWriteReg\n");
    if (i2cWriteAddrSubAddr(vino_regs, i2c_chip_addr, reg_addr) == FAILURE)
	return (FAILURE);

    /* write the data value to the register in the i2c chip */
    VINO_REG_WRITE(vino_regs, i2c_data, val);		

    if (i2cWaitForXferDone (vino_regs) == I2C_ERROR)
	return (FAILURE);

    if (i2cWaitForAck(vino_regs) == I2C_ERROR)
	return (FAILURE);

    /* since the 7191 does not support register reads, shadow the value */

    if (i2c_chip_addr == DMSD_ADDR)
	vino_bd->shadow_7191_regs [reg_addr] = val;

    return (SUCCESS);
}

/*
 *  vinoI2cWriteBlock
 *
 *  Write a block of i2c registers from a specified array of register values.
 *  The array is of type int and is terminated by a -1.
 *
 *  Only registers in one i2c chip may be in the block and no discontinuous
 *  gaps in the registers are allowed.  This function is intended to be
 *  used to initialize an i2c chip to a known state.
 *
 *  A success or failure status value is returned.
 */

int
vinoI2cWriteBlock (VinoBoard *vino_bd, int i2c_chip_addr,
		   int initial_reg_addr, int *pData)
{
    VinoRegs vino_regs = vino_bd->hw_addr;
    int i = initial_reg_addr;
    msg_printf(DBG, "in vinoI2cWriteBlock\n");
    /* Only the 7191 supports autoincrement, so do the CDMC one at a time */

    if (i2c_chip_addr == CDMC_addr) {
	msg_printf(DBG, "in WriteBlock of CDMC\n");
	while (*pData != END_BLOCK) {
	    if (*pData != SKIP_RO_REG)
		vinoI2cWriteReg (vino_bd, i2c_chip_addr, i++, *pData);
	    else
		i++;	/* am i being stupid? i think this was missing */
	    pData++;
	}
    }
    else if (i2c_chip_addr == DMSD_ADDR) {
	if (i2cWriteAddrSubAddr(vino_regs, i2c_chip_addr, i) == FAILURE)
	    return (FAILURE);

	while (*pData != END_BLOCK) {
	    /* write the data value to the register in the i2c chip */
	    
	    VINO_REG_WRITE(vino_regs, i2c_data, *pData);
	    		
	    if (i2cWaitForXferDone (vino_regs) == I2C_ERROR)
		return (FAILURE);

	    if (i2cWaitForAck(vino_regs) == I2C_ERROR)
		return (FAILURE);

	    vino_bd->shadow_7191_regs [i++] = *pData;
	    pData++;
	}
    }
    else
	return (FAILURE);

    return (SUCCESS);
}
