/*
 * hr_regs.c : 
 *	Heart Register read/write Tests
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

#ident "ide/godzilla/heart/hr_regs.c:  $Revision: 1.29 $"

/*
 * hr_regs.c - Heart Register read/write Tests
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"

/*
 * Forward References: all in d_prototypes.h 
 */

/* heart registers */
Heart_Regs	gz_heart_regs[] = {
{"HEART_WID_ID",		HEART_WID_ID,
  HEART_WID_ID_MASK,		GZ_READ_ONLY },

{"HEART_WID_STAT",		HEART_WID_STAT,
  HEART_WID_STAT_MASK,		GZ_READ_ONLY },

{"HEART_WID_ERR_UPPER",		HEART_WID_ERR_UPPER,
  HEART_WID_ERR_UPPER_MASK,	GZ_READ_ONLY },

{"HEART_WID_ERR_LOWER",		HEART_WID_ERR_LOWER,
  HEART_WID_ERR_LOWER_MASK,	GZ_READ_ONLY },

{"HEART_WID_CONTROL",		HEART_WID_CONTROL,
  HEART_WID_CONTROL_MASK,	GZ_NO_ACCESS },

{"HEART_WID_REQ_TIMEOUT",	HEART_WID_REQ_TIMEOUT,
  HEART_WID_REQ_TO_MASK,	GZ_READ_WRITE },

/* the register is R/W but writing to it only clears the register */
{"HEART_WID_ERR_CMDWORD",	HEART_WID_ERR_CMDWORD,
  HEART_WID_ERR_CMDWORD_MASK,	GZ_READ_ONLY },

/* should this be written to?? XXX emulation exception: Data Bus error. */
{"HEART_WID_LLP",		HEART_WID_LLP,
  HEART_WID_LLP_MASK,		GZ_READ_ONLY },

/* this register is only readable by other widgets, not by the uP */
{"HEART_WID_TARG_FLUSH",	HEART_WID_TARG_FLUSH,
  HEART_WID_TARG_FLUSH_MASK,	GZ_NO_ACCESS },

{"HEART_WID_ERR_TYPE",		HEART_WID_ERR_TYPE,
  HEART_WID_ERR_TYPE_MASK,	GZ_READ_ONLY },

{"HEART_WID_ERR_MASK",		HEART_WID_ERR_MASK,
  HEART_WID_ERR_MASK_MASK,	GZ_READ_WRITE },

{"HEART_MODE",			HEART_MODE,
  HEART_MODE_MASK,		GZ_READ_ONLY },

{"HEART_WID_PIO_ERR_UPPER",	HEART_WID_PIO_ERR_UPPER,
  HEART_WID_PIO_ERR_UPPER_MASK,	GZ_READ_ONLY },

{"HEART_WID_PIO_ERR_LOWER",	HEART_WID_PIO_ERR_LOWER,
  HEART_WID_PIO_ERR_LOWER_MASK,	GZ_READ_ONLY },

{"HEART_WID_PIO_RTO_ADDR",	HEART_WID_PIO_RTO_ADDR,
  HEART_WID_PIO_RTO_ADDR_MASK,	GZ_READ_ONLY },

{"HEART_SDRAM_MODE",		HEART_SDRAM_MODE,
  HEART_SDRAM_MODE_MASK,	GZ_READ_ONLY },

{"HEART_MEM_REF",		HEART_MEM_REF,
  HEART_MEM_REF_MASK,		GZ_READ_ONLY },

{"HEART_MEM_ARB",		HEART_MEM_REQ_ARB,
  HEART_MEM_ARB_MASK,		GZ_READ_ONLY },

{"HEART_MEMCFG0",		HEART_MEMCFG(0),
  HEART_MEMCFG0_MASK,		GZ_NO_ACCESS },

/* the other mem config reg can be modified as they are unused (sable only) XXX */
#ifdef SABLE
{"HEART_MEMCFG1",		HEART_MEMCFG(1),
  HEART_MEMCFG1_MASK,		GZ_READ_WRITE },

{"HEART_MEMCFG2",		HEART_MEMCFG(2),
  HEART_MEMCFG2_MASK,		GZ_READ_WRITE },

{"HEART_MEMCFG3",		HEART_MEMCFG(3),
  HEART_MEMCFG3_MASK,		GZ_READ_WRITE },
#else
{"HEART_MEMCFG1",		HEART_MEMCFG(1),
  HEART_MEMCFG1_MASK,		GZ_NO_ACCESS },

{"HEART_MEMCFG2",		HEART_MEMCFG(2),
  HEART_MEMCFG2_MASK,		GZ_NO_ACCESS },

{"HEART_MEMCFG3",		HEART_MEMCFG(3),
  HEART_MEMCFG3_MASK,		GZ_NO_ACCESS },
#endif

{"HEART_FC_MODE",		HEART_FC_MODE,
  HEART_FC_MODE_MASK,		GZ_READ_ONLY },

{"HEART_FC_LIMIT",		HEART_FC_TIMER_LIMIT,
  HEART_FC_LIMIT_MASK,		GZ_READ_ONLY },

{"HEART_FC0_ADDR",		HEART_FC_ADDR(0),
  HEART_FC0_ADDR_MASK,		GZ_READ_WRITE },

{"HEART_FC1_ADDR",		HEART_FC_ADDR(1),
  HEART_FC1_ADDR_MASK,		GZ_READ_WRITE },

{"HEART_FC0_COUNT",		HEART_FC_CR_CNT(0),
  HEART_FC0_COUNT_MASK,		GZ_READ_ONLY },

{"HEART_FC1_COUNT",		HEART_FC_CR_CNT(1),
  HEART_FC1_COUNT_MASK,		GZ_READ_ONLY },

{"HEART_FC0_TIMER",		HEART_FC_TIMER(0),
  HEART_FC0_TIMER_MASK,		GZ_READ_ONLY },

{"HEART_FC1_TIMER",		HEART_FC_TIMER(1),
  HEART_FC1_TIMER_MASK,		GZ_READ_ONLY },

{"HEART_STATUS",		HEART_STATUS,
  HEART_STATUS_MASK,		GZ_READ_ONLY },

{"HEART_BERR_ADDR",		HEART_BERR_ADDR,
  HEART_BERR_ADDR_MASK,		GZ_READ_ONLY },

{"HEART_BERR_MISC",		HEART_BERR_MISC,
  HEART_BERR_MISC_MASK,		GZ_READ_ONLY },

{"HEART_MEM_ERR",		HEART_MEMERR_ADDR,
  HEART_MEM_ERR_MASK,		GZ_READ_ONLY },

{"HEART_BAD_MEM",		HEART_MEMERR_DATA,
  HEART_BAD_MEM_MASK,		GZ_READ_ONLY },

{"HEART_PIU_ERR",		HEART_PIUR_ACC_ERR,
  HEART_PIU_ERR_MASK,		GZ_READ_ONLY },

{"HEART_MLAN_CLK_DIV",		HEART_MLAN_CLK_DIV,
  HEART_MLAN_CLK_DIV_MASK,	GZ_READ_WRITE },

/* theoretically WRITABLE but does not retain value: made RO */
{"HEART_MLAN_CTL",		HEART_MLAN_CTL,
  HEART_MLAN_CTL_MASK,		GZ_READ_ONLY },

{"HEART_IMR(0)",		HEART_IMR(0),
  HEART_IMR0_MASK,		GZ_READ_WRITE },

{"HEART_IMR(1)",		HEART_IMR(1),
  HEART_IMR1_MASK,		GZ_READ_WRITE },

{"HEART_IMR(2)",		HEART_IMR(2),
  HEART_IMR2_MASK,		GZ_READ_WRITE },

{"HEART_IMR(3)",		HEART_IMR(3),
  HEART_IMR3_MASK,		GZ_READ_WRITE },

{"HEART_CAUSE",			HEART_CAUSE,
  HEART_CAUSE_MASK,		GZ_READ_ONLY },

{"HEART_COMPARE",		HEART_COMPARE,
  HEART_COMPARE_MASK,		GZ_READ_WRITE },

{"", -1, -1, -1 }
};

/*
 * Name:	hr_regs.c
 * Description:	tests registers in the Heart
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
/*ARGSUSED*/
bool_t
hr_regs(__uint32_t argc, char **argv)
{
	Heart_Regs	*regs_ptr = gz_heart_regs;

	d_errors = 0; /* init : it is incremented here, 
				not in the called subroutines */

	while (regs_ptr->name[0] != NULL) {
	   if (regs_ptr->mode == GZ_READ_WRITE) {
		if (_hr_regs_read_write(regs_ptr)) {
		    d_errors++;
	   	    if (d_exit_on_error) goto _error;
		}
	    }
	    /* GZ_READ_ONLY/GZ_WRITE_ONLY/GZ_NO_ACCESS are nops here */
	   regs_ptr++;
	}
	if (_special_hr_cases()) { 
		d_errors++;
	}

_error:
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Heart Read-Write Register", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HEART_0004], d_errors );
}

/*
 * Name:	_hr_regs_read_write.c
 * Description:	Performs Write/Reads on the Heart Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_hr_regs_read_write(Heart_Regs *regs_ptr)
{
	heartreg_t	saved_reg_val, write_val, read_val;
	heartreg_t	pattern[HR_REGS_PATTERN_MAX] = {
					0x5a5a5a5a5a5a5a5a, 0xa5a5a5a5a5a5a5a5,
					0xcccccccccccccccc, 0x3333333333333333,
					0xf0f0f0f0f0f0f0f0, 0x0f0f0f0f0f0f0f0f};
	char 		pattern_index;
					
    /* give register info if it was not given in default checking */
    msg_printf(DBG, "\t%s (0x%x) ", regs_ptr->name, regs_ptr->address);

    /* Save the current value */
    PIO_REG_RD_64(regs_ptr->address, (heartreg_t)~0x0, saved_reg_val);

    for (pattern_index=0; pattern_index<HR_REGS_PATTERN_MAX; pattern_index++) {
	write_val = pattern[pattern_index];

	/* Write the test value */
	PIO_REG_WR_64(regs_ptr->address, regs_ptr->mask, write_val);

	/* Read back the register */
	PIO_REG_RD_64(regs_ptr->address, regs_ptr->mask, read_val);

	/* Compare the expected and received values */
	if ((write_val & regs_ptr->mask) != read_val) {
	   msg_printf(INFO, "\nERROR Write/Read: %s; Addr = 0x%x; Mask = 0x%x; \
\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
		regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
		(write_val&regs_ptr->mask), read_val);
	   /* Restore the register with old value */
	   PIO_REG_WR_64(regs_ptr->address, (heartreg_t)~0x0, saved_reg_val);
	   return(1);	/* exit on first error */
	}
	else if (pattern_index == HR_REGS_PATTERN_MAX -1) {
		msg_printf(DBG, "mask= 0x%x; ", regs_ptr->mask);
		msg_printf(DBG, "Write/Read OK.\n");
	}
    } /* pattern_index loop */

    /* Restore the register with old value */
    PIO_REG_WR_64(regs_ptr->address, (heartreg_t)~0x0, saved_reg_val);

    return(0);
}

/*
 * Name:	_special_hr_cases
 * Description:	handles special information about some registers
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: none
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_special_hr_cases()
{
	heartreg_t	heart_wid_id;

	msg_printf(INFO,"Misc heart register info:\n");

	PIO_REG_RD_64(HEART_WID_ID, ~0x0, heart_wid_id);
	/* per working specs, on 3/27/96 */
	if ((heart_wid_id & HW_ID_REV_NUM_MSK) && (heart_wid_id & 1 == 1))
	    msg_printf(INFO,"\t\twidget revision number %d\n\t\twidget part number 0x%x\n\t\twidget manufacturer number %d\n", 
		(heart_wid_id & HW_ID_REV_NUM_MSK) >> HW_ID_REV_NUM_SHFT,
		(heart_wid_id & HW_ID_PART_NUM_MSK) >> HW_ID_PART_NUM_SHFT,
		(heart_wid_id & HW_ID_MFG_NUM_MSK) >> HW_ID_MFG_NUM_SHFT);
	else return (1);
	return (0);

}
