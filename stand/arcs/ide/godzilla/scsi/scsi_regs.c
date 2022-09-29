/*  
 * scsi_reg.c
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S GOVERNMENT RESTRICTED RIGHTS LEGEND:
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

#ident "ide/godzilla/scsi/scsi_regs.c:  $Revision: 1.5 $"

enum {EightBits, SixteenBits, ThirtyTwoBits, SixtyFourBits};
/*
 * scsi_regs.c - scsi Register read/write Tests for configuration space
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_scsiregs.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"

/*
 * Forward References: all in d_prototypes.h 
 */

/*::
 * Global Variables
 */
bool_t		exit_on_scsi_error = 0;
__uint32_t	scsi_error = 0;

scsi_Registers	gd_scsi_regs[] = {

{"SCSI_VENDOR_ID",			SCSI_VENDOR_ID,		GZ_READ_ONLY,
SCSI_VENDOR_ID_MASK,			SCSI_VENDOR_ID_DEFAULT,		SixteenBits},

{"SCSI_DEVICE_ID",			SCSI_DEVICE_ID,		GZ_READ_ONLY,
SCSI_DEVICE_ID_MASK,			SCSI_DEVICE_ID_DEFAULT, 	SixteenBits},

{"SCSI_REVISION_ID",			SCSI_REVISION_ID,		GZ_READ_ONLY,
SCSI_REVISION_ID_MASK,			SCSI_REVISION_ID_DEFAULT, EightBits},

{"SCSI_CLASS_CODE",			SCSI_CLASS_CODE,		GZ_READ_ONLY,
SCSI_CLASS_CODE_MASK,			SCSI_CLASS_CODE_DEFAULT, EightBits},
/*CNTL
{"SCSI_CACHE_LINE_SIZE",		SCSI_CACHE_LINE_SIZE,		GZ_READ_WRITE,
SCSI_CACHE_LINE_SIZE_MASK,		SCSI_CACHE_LINE_SIZE_DEFAULT, EightBits},

{"SCSI_LATENCY_TIMER",			SCSI_LATENCY_TIMER,		GZ_READ_WRITE,
SCSI_LATENCY_TIMER_MASK,		SCSI_LATENCY_TIMER_DEFAULT, EightBits},
*/
{"SCSI_IO_BASE_ADDRESS",		SCSI_IO_BASE_ADDRESS,		GZ_READ_WRITE,
SCSI_IO_BASE_ADDRESS_MASK,		SCSI_IO_BASE_ADDRESS_DEFAULT, ThirtyTwoBits},

{"SCSI_MEMORY_BASE_ADDRESS",		SCSI_MEMORY_BASE_ADDRESS,	GZ_READ_WRITE,
SCSI_MEMORY_BASE_ADDRESS_MASK,		SCSI_MEMORY_BASE_ADDRESS_DEFAULT, ThirtyTwoBits},
/*CNTL
{"SCSI_CONF_1",				SCSI_CONF_1,			GZ_READ_WRITE,
SCSI_CONF_1_MASK,			SCSI_CONF_1_DEFAULT, SixteenBits},
*/

/*SCSI 0 DEVICE IO SPACE REGISTERS*/
{"SCSI_SEMAPHORE",			SCSI_SEMAPHORE,			GZ_READ_WRITE,
SCSI_SEMAPHORE_MASK,			SCSI_SEMAPHORE_DEFAULT, SixteenBits},
/*CNTL
{"SCSI_NVRAM_INTF",			SCSI_NVRAM_INTF,		GZ_READ_WRITE,
SCSI_NVRAM_INTF_MASK,			SCSI_NVRAM_INTF_DEFAULT, SixteenBits},
*/
/* NOT USED development on hold
{"SCSI_DMA_CONF",			SCSI_DMA_CONF,			GZ_READ_WRITE,
SCSI_DMA_CONF_MASK,			SCSI_DMA_CONF_DEFAULT, SixteenBits},

{"SCSI_DMA_TFR_COUNTER_LOW",		SCSI_DMA_TFR_COUNTER_LOW,	GZ_READ_WRITE,
SCSI_DMA_TFR_COUNTER_LOW_MASK,		SCSI_DMA_TFR_COUNTER_LOW_DEFAULT, SixteenBits},

{"SCSI_DMA_ADDR_WORD_CNT_0",		SCSI_DMA_ADDR_WORD_CNT_0,	GZ_READ_WRITE,
SCSI_DMA_ADDR_WORD_CNT_0_MASK,		SCSI_DMA_ADDR_WORD_CNT_0_DEFAULT, SixteenBits},

{"SCSI_DMA_ADDR_WORD_CNT_1",		SCSI_DMA_ADDR_WORD_CNT_1,	GZ_READ_WRITE,
SCSI_DMA_ADDR_WORD_CNT_1_MASK,		SCSI_DMA_ADDR_WORD_CNT_1_DEFAULT, SixteenBits},

{"SCSI_DMA_ADDR_WORD_CNT_2",		SCSI_DMA_ADDR_WORD_CNT_2,	GZ_READ_WRITE,
SCSI_DMA_ADDR_WORD_CNT_2_MASK,		SCSI_DMA_ADDR_WORD_CNT_2_DEFAULT, SixteenBits},

{"SCSI_DMA_ADDR_WORD_CNT_3",		SCSI_DMA_ADDR_WORD_CNT_3,	GZ_READ_WRITE,
SCSI_DMA_ADDR_WORD_CNT_3_MASK,		SCSI_DMA_ADDR_WORD_CNT_3_DEFAULT, SixteenBits},

{"SCSI_SXP_PART_ID",			SCSI_SXP_PART_ID,		GZ_READ_ONLY,
SCSI_SXP_PART_ID_MASK,			SCSI_SXP_PART_ID_DEFAULT, SixteenBits},

{"SCSI_SXP_CONF_1",			SCSI_SXP_CONF_1,		GZ_READ_WRITE,
SCSI_SXP_CONF_1_MASK,			SCSI_SXP_CONF_1_DEFAULT, SixteenBits},

{"SCSI_SXP_CONF_2",			SCSI_SXP_CONF_2,		GZ_READ_WRITE,
SCSI_SXP_CONF_2_MASK,			SCSI_SXP_CONF_2_DEFAULT, SixteenBits},

{"SCSI_SXP_CONF_3",			SCSI_SXP_CONF_3,		GZ_READ_WRITE,
SCSI_SXP_CONF_3_MASK,			SCSI_SXP_CONF_3_DEFAULT, SixteenBits},

{"SCSI_SXP_RETURN_ADDR",		SCSI_SXP_RETURN_ADDR,		GZ_READ_WRITE,
SCSI_SXP_RETURN_ADDR_MASK,		SCSI_SXP_RETURN_ADDR_DEFAULT, SixteenBits},

{"SCSI_SXP_SEQUENCE",			SCSI_SXP_SEQUENCE,		GZ_READ_WRITE,
SCSI_SXP_SEQUENCE_MASK,			SCSI_SXP_SEQUENCE_DEFAULT, SixteenBits},

{"SCSI_SXP_EXCEPTION",			SCSI_SXP_EXCEPTION,		GZ_READ_WRITE,
SCSI_SXP_EXCEPTION_MASK,		SCSI_SXP_EXCEPTION_DEFAULT, SixteenBits},

{"SCSI_SXP_OVERRIDE",			SCSI_SXP_OVERRIDE,		GZ_READ_WRITE,
SCSI_SXP_OVERRIDE_MASK,			SCSI_SXP_OVERRIDE_DEFAULT, SixteenBits},

{"SCSI_SXP_LITTERAL_BASE",		SCSI_SXP_LITTERAL_BASE,		GZ_READ_WRITE,
SCSI_SXP_LITTERAL_BASE_MASK,		SCSI_SXP_LITTERAL_BASE_DEFAULT, SixteenBits},

{"SCSI_SXP_USER_FLAGS",			SCSI_SXP_USER_FLAGS,		GZ_READ_WRITE,
SCSI_SXP_USER_FLAGS_MASK,		SCSI_SXP_USER_FLAGS_DEFAULT, SixteenBits},

{"SCSI_SXP_USER_EXCEPTION",		SCSI_SXP_USER_EXCEPTION,	GZ_READ_WRITE,
SCSI_SXP_USER_EXCEPTION_MASK,		SCSI_SXP_USER_EXCEPTION_DEFAULT, SixteenBits},

{"SCSI_SXP_BREAKPOINT",			SCSI_SXP_BREAKPOINT,		GZ_READ_WRITE,
SCSI_SXP_BREAKPOINT_MASK,		SCSI_SXP_BREAKPOINT_DEFAULT, SixteenBits},

{"SCSI_SXP_SCSI_BUS_ID",		SCSI_SXP_SCSI_BUS_ID,		GZ_READ_WRITE,
SCSI_SXP_SCSI_BUS_ID_MASK,		SCSI_SXP_SCSI_BUS_ID_DEFAULT, SixteenBits},

{"SCSI_SXP_DEV_CONF_1",			SCSI_SXP_DEV_CONF_1,		GZ_READ_WRITE,
SCSI_SXP_DEV_CONF_1_MASK,		SCSI_SXP_DEV_CONF_1_DEFAULT, SixteenBits},

{"SCSI_SXP_DEV_CONF_2",			SCSI_SXP_DEV_CONF_2,		GZ_READ_WRITE,
SCSI_SXP_DEV_CONF_2_MASK,		SCSI_SXP_DEV_CONF_2_DEFAULT, SixteenBits},

{"SCSI_SXP_PHASE_POINTER",		SCSI_SXP_PHASE_POINTER,		GZ_READ_WRITE,
SCSI_SXP_PHASE_POINTER_MASK,		SCSI_SXP_PHASE_POINTER_DEFAULT, SixteenBits},

{"SCSI_SXP_BUFFER_POINTER",		SCSI_SXP_BUFFER_POINTER,	GZ_READ_WRITE,
SCSI_SXP_BUFFER_POINTER_MASK,		SCSI_SXP_BUFFER_POINTER_DEFAULT, SixteenBits},

{"SCSI_SXP_BUFFER_COUNTER",		SCSI_SXP_BUFFER_COUNTER,	GZ_READ_WRITE,
SCSI_SXP_BUFFER_COUNTER_MASK,		SCSI_SXP_BUFFER_COUNTER_DEFAULT, SixteenBits},

{"SCSI_SXP_BUFFER",			SCSI_SXP_BUFFER,		GZ_READ_WRITE,
SCSI_SXP_BUFFER_MASK,			SCSI_SXP_BUFFER_DEFAULT, SixteenBits},

{"SCSI_SXP_BUFFER_BYTE",		SCSI_SXP_BUFFER_BYTE,		GZ_READ_WRITE,
SCSI_SXP_BUFFER_BYTE_MASK,		SCSI_SXP_BUFFER_BYTE_DEFAULT, SixteenBits},

{"SCSI_SXP_BUFFER_WORD",		SCSI_SXP_BUFFER_WORD,		GZ_READ_WRITE,
SCSI_SXP_BUFFER_WORD_MASK,		SCSI_SXP_BUFFER_WORD_DEFAULT, SixteenBits},

{"SCSI_SXP_BUFFER_WORD_TRANS",		SCSI_SXP_BUFFER_WORD_TRANS,	GZ_READ_WRITE,
SCSI_SXP_BUFFER_WORD_TRANS_MASK,	SCSI_SXP_BUFFER_WORD_TRANS_DEFAULT, SixteenBits},

{"SCSI_SXP_FIFO",			SCSI_SXP_FIFO,			GZ_READ_WRITE,
SCSI_SXP_FIFO_MASK,			SCSI_SXP_FIFO_DEFAULT, SixteenBits},

{"SCSI_SXP_FIFO_TOP_RESIDUE",		SCSI_SXP_FIFO_TOP_RESIDUE,	GZ_READ_WRITE,
SCSI_SXP_FIFO_TOP_RESIDUE_MASK,		SCSI_SXP_FIFO_TOP_RESIDUE_DEFAULT, SixteenBits},

{"SCSI_SXP_FIFO_BOTTOM_RESIDUE",	SCSI_SXP_FIFO_BOTTOM_RESIDUE,	GZ_READ_WRITE,
SCSI_SXP_FIFO_BOTTOM_RESIDUE_MASK,	SCSI_SXP_FIFO_BOTTOM_RESIDUE_DEFAULT, SixteenBits},

{"SCSI_SXP_TFR_REGISTER",		SCSI_SXP_TFR_REGISTER,		GZ_READ_WRITE,
SCSI_SXP_TFR_REGISTER_MASK,		SCSI_SXP_TFR_REGISTER_DEFAULT, SixteenBits},

{"SCSI_SXP_TFR_COUNT_LOW",		SCSI_SXP_TFR_COUNT_LOW,		GZ_READ_WRITE,
SCSI_SXP_TFR_COUNT_LOW_MASK,		SCSI_SXP_TFR_COUNT_LOW_DEFAULT, SixteenBits},

{"SCSI_SXP_TFR_COUNT_HIGH",		SCSI_SXP_TFR_COUNT_HIGH,	GZ_READ_WRITE,
SCSI_SXP_TFR_COUNT_HIGH_MASK,		SCSI_SXP_TFR_COUNT_HIGH_DEFAULT, SixteenBits},

{"SCSI_SXP_TFR_COUNTER_LOW",		SCSI_SXP_TFR_COUNTER_LOW,	GZ_READ_WRITE,
SCSI_SXP_TFR_COUNTER_LOW_MASK,		SCSI_SXP_TFR_COUNTER_LOW_DEFAULT, SixteenBits},

{"SCSI_SXP_TFR_COUNTER_HIGH",		SCSI_SXP_TFR_COUNTER_HIGH,	GZ_READ_WRITE,
SCSI_SXP_TFR_COUNTER_HIGH_MASK,		SCSI_SXP_TFR_COUNTER_HIGH_DEFAULT, SixteenBits},
*/

/*SCSI 1 DEVICE IO SPACE REGISTERS*/
{"SCSI1_SEMAPHORE",			SCSI1_SEMAPHORE,			GZ_READ_WRITE,
SCSI1_SEMAPHORE_MASK,			SCSI1_SEMAPHORE_DEFAULT, SixteenBits},

{"", -1, -1, -1, -1}

};

void initSCSI(void) {

    /*
        SCSI initialization.
    */
    PIO_REG_WR_8(SCSI_LATENCY_TIMER,~0x0,0x40);
    PIO_REG_WR_8(SCSI_CACHE_LINE_SIZE,~0x0,0x40);
    PIO_REG_WR_16(SCSI_COMMAND,~0x0,MEMORY_SPACE_ENABLE | BUS_MASTER_ENABLE);

    PIO_REG_WR_16(SCSI_INTF_CONTROL, ~0x0, ICR_ENABLE_RISC_INT | ICR_SOFT_RESET);
    DELAY(10);
    PIO_REG_WR_16(SCSI_HOST_COMM_CNTL, ~0x0, HCCR_CMD_RESET);
    PIO_REG_WR_16(SCSI_HOST_COMM_CNTL, ~0x0, HCCR_CMD_RELEASE);
    PIO_REG_WR_16(SCSI_HOST_COMM_CNTL, ~0x0, HCCR_WRITE_BIOS_ENABLE);
    PIO_REG_WR_16(SCSI_CONF_1, ~0x0, CONF_1_BURST_ENABLE);
    PIO_REG_WR_16(SCSI_SEMAPHORE, ~0x0, 0x0);

   /*access SPX scsi processor registers*/
   /*PIO_REG_WR_16(SCSI_CONF_1,~0x0,0x8);*/
   /*access risc processor registers*/
   /*PIO_REG_WR_16(SCSI_CONF_1,~0x0,0x0);*/

   /*parse risk processor*/
   PIO_REG_WR_16(SCSI_HOST_COMM_CNTL, ~0x0, HCCR_CMD_PAUSE);

}

void restoreSCSI(void) {
    /*get out of pause mode*/
    PIO_REG_WR_16(SCSI_HOST_COMM_CNTL, ~0x0, HCCR_CMD_RESET);
    PIO_REG_WR_16(SCSI_HOST_COMM_CNTL, ~0x0, HCCR_CMD_RELEASE);
    PIO_REG_WR_16(SCSI_HOST_COMM_CNTL, ~0x0, HCCR_WRITE_BIOS_ENABLE);
}

/*
 * Name:	scsi_regs.c
 * Description:	tests registers in the scsi
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
scsi_regs(__uint32_t argc, char **argv)
{
	scsi_Registers	*regs_ptr = gd_scsi_regs;
	bool_t		check_defaults;

	msg_printf(INFO,"SCSI Register Test\n");
	initSCSI(); /*required scsi setup*/

	scsi_error = 0; /* init : it is incremented here, 
				not in the called subroutines */
	/*test
	test();
	*/

	while (regs_ptr->name[0] != NULL) {

	   switch (regs_ptr->mode) {
	      case  GZ_READ_ONLY:
	        if (_scsi_regs_check_default(regs_ptr)) {
	        	msg_printf( DBG,"\n");
			scsi_error++;
	  		if (exit_on_scsi_error) goto _error;
	  	        msg_printf( DBG,"\n");
		}
		break;
	      case  GZ_READ_WRITE: 
		if (_scsi_regs_read_write(regs_ptr)) {
		    scsi_error++;
	   	    if (exit_on_scsi_error) goto _error;
		}
		break;
	      case  GZ_WRITE_ONLY:
		break;
	      case  GZ_NO_ACCESS:
		break;
	   }
	   regs_ptr++;
	}
	/*
	if (_special_scsi_cases()) { 
		scsi_error++;
	}
	*/

_error:
	restoreSCSI();
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(scsi_error, "SCSI Read-Write Register", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_SCSI_0001], scsi_error );
}

/*
 * Name:	_scsi_regs_check_default.c
 * Description:	Tests the default values of scsi Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_scsi_regs_check_default(scsi_Registers *regs_ptr)
{
	scsiregisters_t	reg_val;
	unsigned char mask_char;
	unsigned char reg_val_char;
	unsigned short mask_short;
	unsigned short reg_val_short;

	switch (regs_ptr->size) {
		case EightBits:
			msg_printf(DBG, "EightBits\n");
			mask_char = regs_ptr->mask;
     			PIO_REG_RD_8(regs_ptr->address, mask_char, reg_val_char);
			if (reg_val_char != (regs_ptr->def & mask_char)) {
	   			msg_printf(INFO, "ERROR Default:    %s; Addr = 0x%x; Mask = 0x%x; \
				\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
				regs_ptr->name, regs_ptr->address, mask_char, 
				(regs_ptr->def & mask_char), (reg_val_char & mask_char));
	   		if (exit_on_scsi_error) {
			return(1);
	   		}
			}
        		else {
	   		msg_printf(DBG, "Configuration Register Test Passed\n");
	   		msg_printf(DBG, "\t%s (0x%x) mask= 0x%x; default OK= 0x%x;\n",
				regs_ptr->name, regs_ptr->address, mask_char,
				(regs_ptr->def & mask_char));
			}
		break;
		case SixteenBits:
			msg_printf(DBG, "SixteenBits\n");
			mask_short = regs_ptr->mask;
		       	PIO_REG_RD_16(regs_ptr->address, mask_short, reg_val_short);
			if (reg_val_short != (regs_ptr->def & mask_short)) {
	   			msg_printf(INFO, "ERROR Default:    %s; Addr = 0x%x; Mask = 0x%x; \
				\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
				regs_ptr->name, regs_ptr->address, mask_short, 
				(regs_ptr->def & mask_short), (reg_val_short & mask_short));
	   		if (exit_on_scsi_error) {
			return(1);
	   		}
			}
        		else {
	   		msg_printf(DBG, "Configuration Register Test Passed\n");
	   		msg_printf(DBG, "\t%s (0x%x) mask= 0x%x; default OK= 0x%x;\n",
				regs_ptr->name, regs_ptr->address, regs_ptr->mask,
				(regs_ptr->def & regs_ptr->mask));
			}
		break;
		case ThirtyTwoBits:
			msg_printf(DBG, "ThirtyTwoBits\n");
		       	PIO_REG_RD_32(regs_ptr->address, regs_ptr->mask, reg_val);
			if (reg_val != (regs_ptr->def & regs_ptr->mask)) {
	   			msg_printf(INFO, "ERROR Default:    %s; Addr = 0x%x; Mask = 0x%x; \
				\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
				regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
				(regs_ptr->def & regs_ptr->mask), (reg_val & regs_ptr->mask));
	   		if (exit_on_scsi_error) {
			return(1);
	   		}
			}
        		else {
	   		msg_printf(DBG, "Configuration Register Test Passed\n");
	   		msg_printf(DBG, "\t%s (0x%x) mask= 0x%x; default OK= 0x%x;\n",
				regs_ptr->name, regs_ptr->address, regs_ptr->mask,
				(regs_ptr->def & regs_ptr->mask));
			}
		break;
		default:
		msg_printf(INFO, "SW error\n");
	}
	
	return(0);
}

/*
 * Name:	_scsi_regs_read_write.c
 * Description:	Performs Write/Reads on the scsi Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_scsi_regs_read_write(scsi_Registers *regs_ptr)
{
	scsiregisters_t	saved_reg_val, write_val, read_val;
	scsiregisters_t	pattern[SCSIREGS_REGS_PATTERN_MAX] = {
					0x5a5a5a5a, 0xa5a5a5a5,
					0xcccccccc, 0x33333333,
					0xf0f0f0f0, 0x0f0f0f0f};
	char 		pattern_index;
	unsigned char mask_char;
	unsigned char read_val_char;
	unsigned short mask_short;
	unsigned short read_val_short;
	unsigned char saved_reg_val_char;
	unsigned short saved_reg_val_short;
	unsigned char write_val_char;
	unsigned short write_val_short;	

    	/* Save the current value */
	switch (regs_ptr->size) {
		case EightBits:
			msg_printf(DBG, "EightBits\n");
    			PIO_REG_RD_8(regs_ptr->address, (scsiregisters_t)~0x0,saved_reg_val_char);
		break;
		case SixteenBits:
			msg_printf(DBG, "SixteeenBits\n");
    			PIO_REG_RD_16(regs_ptr->address, (scsiregisters_t)~0x0, saved_reg_val_short);
		break;
		case ThirtyTwoBits:
			msg_printf(DBG, "ThirtyttwoBits\n");
    			PIO_REG_RD_32(regs_ptr->address, (scsiregisters_t)~0x0, saved_reg_val);
		break;
		default:
		msg_printf(INFO, "SW error\n");
	}
    for (pattern_index=0; pattern_index<HR_REGS_PATTERN_MAX; pattern_index++) {
	write_val = pattern[pattern_index];

	/* Write the test value */
	switch (regs_ptr->size) {
		case EightBits:
			msg_printf(DBG, "EightBits\n");
			mask_char = regs_ptr->mask;
			write_val_char = write_val;
			PIO_REG_WR_8(regs_ptr->address, mask_char, write_val_char);
		break;
		case SixteenBits:
			msg_printf(DBG, "SixteeenBits\n");
			mask_short = regs_ptr->mask;
			write_val_short = write_val;
			PIO_REG_WR_16(regs_ptr->address, mask_short, write_val_short);
		break;
		case ThirtyTwoBits:
			msg_printf(DBG, "ThirtyTwoBits\n");
			PIO_REG_WR_32(regs_ptr->address, regs_ptr->mask, write_val);
		break;
		default:
		msg_printf(INFO, "SW error\n");
	}

	/* Read back the register */
	switch (regs_ptr->size) {
		case EightBits:
			/* Compare the expected and received values */
			if ((write_val_char & mask_char) != read_val_char) {
	   			msg_printf(INFO, "\nERROR Write/Read: %s; Addr = 0x%x; Mask = 0x%x; \
				\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
				regs_ptr->name, regs_ptr->address, mask_char, 
				(write_val_char&mask_char), read_val_char);
	   		/* Restore the register with old value */
	   		PIO_REG_WR_8(regs_ptr->address, (scsiregisters_t)~0x0, saved_reg_val_char);
	   		return(1);	/* exit on first error */
			}
			else if (pattern_index == HR_REGS_PATTERN_MAX -1) {
			msg_printf(DBG, "Write/Read Test OK.\n");
			}
		break;
		case SixteenBits:
			PIO_REG_RD_16(regs_ptr->address, mask_short, read_val_short);
			/* Compare the expected and received values */
			if ((write_val_short & mask_short) != read_val_short) {
	   			msg_printf(INFO, "\nERROR Write/Read: %s; Addr = 0x%x; Mask = 0x%x; \
				\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
				regs_ptr->name, regs_ptr->address, mask_short, 
				(write_val_short&mask_short), read_val_short);
	   		/* Restore the register with old value */
	   		PIO_REG_WR_16(regs_ptr->address, (scsiregisters_t)~0x0, saved_reg_val_short);
	   		return(1);	/* exit on first error */
			}
			else if (pattern_index == HR_REGS_PATTERN_MAX -1) {
			msg_printf(DBG, "Write/Read Test OK.\n");
			}
		break;
		case ThirtyTwoBits:
			PIO_REG_RD_32(regs_ptr->address, regs_ptr->mask, read_val);
			/* Compare the expected and received values */
			if ((write_val & regs_ptr->mask) != read_val) {
	   			msg_printf(INFO, "\nERROR Write/Read: %s; Addr = 0x%x; Mask = 0x%x; \
				\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
				regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
				(write_val&regs_ptr->mask), read_val);
	   		/* Restore the register with old value */
	   		PIO_REG_WR_32(regs_ptr->address, (scsiregisters_t)~0x0, saved_reg_val);
	   		return(1);	/* exit on first error */
			}
			else if (pattern_index == HR_REGS_PATTERN_MAX -1) {
			msg_printf(DBG, "Write/Read Test OK.\n");
			}
		break;
		default:
		msg_printf(INFO, "SW error\n"); 
	}

			
    } /* pattern_index loop */

    /* Restore the register with old value */
	switch (regs_ptr->size) {
		case EightBits:
			PIO_REG_WR_8(regs_ptr->address, (scsiregisters_t)~0x0, saved_reg_val_char);
		break;
		case SixteenBits:
			PIO_REG_WR_16(regs_ptr->address, (scsiregisters_t)~0x0, saved_reg_val_short);
		break;
		case ThirtyTwoBits:
			PIO_REG_WR_32(regs_ptr->address, (scsiregisters_t)~0x0, saved_reg_val);
		break;
		default:
		msg_printf(INFO, "SW error\n");
	}

    return(0);
}

/*
 * Name:	_special_scsi_cases
 * Description:	handles special information about some registers
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: none
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_special_scsi_cases()
{
	return (0);
 
}

