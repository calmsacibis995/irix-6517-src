/*
 * hr_pio_wcr.c   
 *
 *	runs PIO WCR (Widget Configuration Register) access tests
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

#ident "ide/godzilla/heart_bridge/hr_pio_wcr.c:  $Revision: 1.17 $"

/*
 * hr_pio_wcr.c - runs PIO WCR access tests
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h" 	/* all function prototypes */
#include "sys/xtalk/xwidget.h"

/*
 * Name:	hr_pio_wcr
 * Description:	runs PIO WCR (Widget Configuration Register) access tests
 * Input:	none
 * Output:	Returns d_errors
 * Error Handling: no interrupt, using polling
 * Side Effects: 
 * Remarks:     This is more a heart test that involves widget registers
 * Debug Status: compiles, simulated, not emulated, not debugged
 */
bool_t
hr_pio_wcr(void)
{
	heartreg_t	heart_w_err;
	heartreg_t	heart_addr_lower, heart_addr_upper;
	heartreg_t tmp_hr_buf[2] = {HEART_WID_ERR_MASK, D_EOLIST};
	heartreg_t tmp_hr_save[2];

	msg_printf(DBG,"Heart ""PIO access"" Test... Begin\n");

	_hb_disable_proc_intr(); /* disable the coprocessor0 intr */

	/* reset the diag error cntr */
	d_errors = 0;
	
	/* heart and Bridge check out */
	if (_hb_chkout(CHK_HEART, DONT_CHK_BRIDGE)) {
		msg_printf(ERR, "Heart/Bridge Check Out Error\n");
		d_errors++;
		goto _error;
	}

	/* save all readable H registers that this function writes to */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);

	/* Set PIO_WCR_AccessErr bit in the Widget Error Mask Register */
	PIO_REG_WR_64(HEART_WID_ERR_MASK, ~0x0, ERRTYPE_PIO_WCR_ACC_ERR);

	/* write to  a Read/Only  Widget Config. register */
	/*		(i.e Widget Identification Register) */
	/* OR read from a Write/Only Widget Config. register */
	/*		(NOTE: could not find any WO reg XXX)	*/
	msg_printf(DBG,"Heart ""PIO access"" Test... writing to a R/O register (0x%x)\n", HEART_WID_ID);
	PIO_REG_WR_64(HEART_WID_ID, ~0x0, 0x0);

	/* wait for WidgetErr in H cause register */
	if(_hb_wait_for_widgeterr()) {
		msg_printf(ERR, "*** error in _hb_wait_for_widgeterr ***\n");
		d_errors++;
		if (d_exit_on_error) goto _error;
	}

	/* check PIO_WCR_AccessErr in the Widget Error Type Register */
	PIO_REG_RD_64(HEART_WID_ERR_TYPE, ERRTYPE_PIO_WCR_ACC_ERR, heart_w_err);
	msg_printf(DBG," PIO_WCR_AccessErr in the Widget Error Type Register = 0x%x\n", heart_w_err);
	if (heart_w_err == 0) {
		msg_printf(ERR, "*** PIO_WCR_AccessErr not set ***\n");
		d_errors++;
		if (d_exit_on_error) goto _error;
	}

	/* check the error information in the Widget PIO Error */
	/*	Address Lower/Upper Registers. */
	PIO_REG_RD_64(HEART_WID_PIO_ERR_UPPER, MASK_32, heart_addr_upper);
	/* print only for now the SysCmd and UncAttr fields XXX */
	msg_printf(DBG,"\t\tWidget PIO Error Address Upper Register:\n");
	msg_printf(DBG,"\t\t\tProcNum  %1x\n",
		(heart_addr_upper & HW_PIO_ERR_PROC_ID) >> 22);
	/* XXX revise for mp */
	if ((heart_addr_upper & HW_PIO_ERR_PROC_ID) != 0) { 
		msg_printf(ERR, "*** ProcId is not 0  ***\n");
		d_errors++;
		if (d_exit_on_error) goto _error;
	}
	msg_printf(DBG,"\t\t\tSysCmd  %3x (only printed for now XXX)\n",
		(heart_addr_upper & HW_PIO_ERR_SYSCMD) >> 8);
	msg_printf(DBG,"\t\t\tUncAttr  %1x (only printed for now XXX)\n",
		(heart_addr_upper & HW_PIO_ERR_UNC_ATTR) >> 20);
	msg_printf(DBG,"\t\t\tPhysAddr  %2x\n",
		(heart_addr_upper & HW_PIO_ERR_ADDR) >> 0);
	if ( (heart_addr_upper & HW_PIO_ERR_ADDR) != 
		(heartreg_t)HEART_WID_ID >> 32 ) {
		msg_printf(ERR, "*** Upper Phys addresses (32-39) don't match ***\n");	
		d_errors++;
		if (d_exit_on_error) goto _error;
	}
	PIO_REG_RD_64(HEART_WID_PIO_ERR_LOWER, HW_PIO_ERR_LOWER_ADDR, 
		heart_addr_lower);
	msg_printf(DBG,"\t\tWidget PIO Error Address Lower Register: %8x\n",
		heart_addr_lower );
	if ( (HEART_WID_ID & HW_PIO_ERR_LOWER_ADDR) != heart_addr_lower ) {
		msg_printf(ERR, "*** Lower Phys addresses (0-31) don't match ***\n");  
		d_errors++;
		if (d_exit_on_error) goto _error;
	}

	/* reset the heart and bridge */
	if (_hb_reset(RES_HEART, DONT_RES_BRIDGE)) {
		d_errors++;
		if (d_exit_on_error) goto _error;
	} 

_error:
	/* restore all readable H registers that this function writes to */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);

	/* top level function must report the pass/fail status */
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Heart ""PIO Access""", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HEART_0002], d_errors );
}
