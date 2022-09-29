/*
 * hr_misc.c : 
 *	Heart Miscellaneous Register Tests
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

#ident "ide/godzilla/heart/hr_misc.c:  $Revision: 1.12 $"

/*
 * hr_misc.c - Heart Miscellaneous Register Tests
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

/*
 * Name:	hr_misc.c
 * Description:	tests miscellaneous registers in the Heart
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
hr_misc()
{
	heartreg_t	mask = ~0x0;
	heartreg_t	rd_init_count;
	heartreg_t	count_incr;
	heartreg_t	timerIP_mask; 
	heartreg_t	compare_value = 0xf; /* XXX */
	__uint64_t	time_out = 0xffff; /* XXX */
	__uint64_t	i, delay = 100;  /* XXX */
	heartreg_t	isr_value, prid_value;
	heartreg_t	sync_value, rd_count;
	heartreg_t	tmp_hr_buf[2] = {HEART_COMPARE, D_EOLIST};
	heartreg_t	tmp_hr_save[2];

	/*
	 * Test Miscellaneous Registers
	 * 1. Read the current value of the Fixed Time Base Register 
	 * 2. After a brief delay, test if the counter has changed
	 * 3. Load the compare register with a known value
	 * 4. Poll on the Timer_IP bit in the interrupt status register
	 * 5. Read Processor ID register
	 * 6. Read Sync register and verify that it's zero
	 */

	msg_printf(DBG, "Heart Miscellaneous Registers Test... Begin\n");

	/* clear the error count */
	d_errors = 0; 

	/* save registers first */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);

	/* 1. First read the current value of the Fixed Time Base Register */
	PIO_REG_RD_64(HEART_COUNT, mask, rd_init_count);

	/* 2. delay */
	for (i = 0; i < delay; i++) {}
	/* read back the counter */
	PIO_REG_RD_64(HEART_COUNT, mask, rd_count);
	msg_printf(DBG, "\t\tFixed Time Base Register:\n \t\t\tinitial value = 0x%x; later value = 0x%x\n", rd_init_count, rd_count);
	/* just checking if the counter has changed */
	if (rd_count == rd_init_count) {
		d_errors++;
		msg_printf(ERR, "Fixed Time Base Reg Read/Write Test FAILED\n");
		if (d_exit_on_error) goto _error;
	} 

	/* 3. Load the compare register with a known value */
	/* 	put the value far enough ahead of where it is *now* */
	count_incr = 100 * (rd_count - rd_init_count);
	PIO_REG_RD_64(HEART_COUNT, mask, rd_count);
	compare_value = rd_count + count_incr;
	PIO_REG_WR_64(HEART_COMPARE, mask, compare_value);

	/* 4. Poll on the Timer_IP bit in the interrupt status register */
	timerIP_mask = ((heartreg_t) HEART_INT_L3MASK << HEART_INT_L3SHIFT);
	do {
		PIO_REG_RD_64(HEART_ISR, timerIP_mask, isr_value);
		time_out--;
	} while ((isr_value != timerIP_mask) && time_out);
	if (!time_out) {
		msg_printf(ERR, "Fixed Time Base Register Write & Interrupt Poll Test FAILED\n");
		d_errors++;
		if (d_exit_on_error) goto _error;
	}
	/* if we got a timer int, clear it out. */
	if (isr_value & timerIP_mask)
		PIO_REG_WR_64(HEART_CLR_ISR, mask, timerIP_mask);

	/* 5. Read Processor ID register */
	PIO_REG_RD_64(HEART_PRID, mask, prid_value);
	msg_printf(SUM, "Heart Miscellaneous Registers Test... Processor ID is 0x%x\n", prid_value);

	/* 6. Read Sync register and verify that it's zero */
	PIO_REG_RD_64(HEART_SYNC, mask, sync_value);
	msg_printf(DBG, "Heart Miscellaneous Registers Test... Sync value = 0x%x\n", sync_value);
	if (sync_value) {
		msg_printf(ERR, "Sync Register Test FAILED\n");
		d_errors++;
		if (d_exit_on_error) goto _error;
	}

_error:
	msg_printf(DBG, "Heart Miscellaneous Registers Test... End\n");

	/* restore all H registers that are written to in this module */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);

#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Heart Miscellaneous Register", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HEART_0001], d_errors );
}

