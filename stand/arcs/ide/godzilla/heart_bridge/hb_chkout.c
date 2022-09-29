/*
 * hb_chkout.c
 *
 * 	Checks whether some important registers in HEART & BRIDGE
 *	have the right (or decent) values. This function must be called
 *	before actually performing the test.
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

#ident "ide/heart_bridge/hb_chkout.c: $Revision: 1.19 $"

/*
 * hb_chkout.c - checks whether some important registers in HEART & BRIDGE
 *		 have the right (or decent) values
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
 * Forward References : in d_prototypes.h
 */

/*
 * Name:	hb_chkout
 * Input:	whether to check heart and/or bridge (-h -b options)
 * Description: call to _hb_chkout as ide command
 */
bool_t
hb_chkout(__uint32_t argc, char **argv)
{
	bool_t	check_heart = FALSE;
	bool_t	check_bridge = FALSE;

	/* "-h" arg for heart, "-b" for bridge */
	/*  default is "check both " */
	if (argc == 1) {
		check_heart = TRUE;
		check_bridge = TRUE;
	}
	else  {
		argc--; argv++;
		if (argc && argv[0][0]=='-' && argv[0][1]=='h')
			check_heart = TRUE;
		else if (argc && argv[0][0]=='-' && argv[0][1]=='b')
		check_bridge = TRUE;
		if (argc != 0) {
			argc--; argv++;
			if (argc && argv[0][0]=='-' && argv[0][1]=='b')
				check_bridge = TRUE;
			else if (argc && argv[0][0]=='-' && argv[0][1]=='h')
				check_heart = TRUE;
		}
	}

	if (_hb_chkout(check_heart, check_bridge)) /* computes d_errors */
		msg_printf(DBG,"_hb_chkout Failed\n");

	if (check_heart && !check_bridge)
	    REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HEART_0005], d_errors );
#ifdef NOTNOW
	    REPORT_PASS_OR_FAIL(d_errors, "Heart Check Out", D_FRU_IP30);
#endif
	else if (!check_heart && check_bridge)
	    REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_BRIDGE_0004], d_errors );
#ifdef NOTNOW
	    REPORT_PASS_OR_FAIL(d_errors, "Bridge Check Out", D_FRU_IP30);
#endif
	else
	    REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HAB_0000], d_errors );
#ifdef NOTNOW
	    REPORT_PASS_OR_FAIL(d_errors, "Heart & Bridge Check Out", D_FRU_IP30);
#endif
}

/*
 * Name:        _hb_chkout
 * Description: checks whether some important registers in HEART & BRIDGE
 *		have the right (or decent) values
 * Input:       whether to check heart and/or bridge
 * Output:      1 if fails, 0 if succeeds
 * Error Handling: 
 * Side Effects: none
 * Remarks:
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_hb_chkout(bool_t chk_heart, bool_t chk_bridge)
{
	bridgereg_t	br_mask = ~0x0;
	bridgereg_t	br_id, br_status, br_int_status;
	bridgereg_t	br_err_upper, br_err_lower;
	bridgereg_t	br_status_default = D_BRIDGE_STATUS;
	heartreg_t	hr_mask = ~0x0;
	heartreg_t	hr_id, hr_status, hr_int_status;
	heartreg_t	hr_err_upper, hr_err_lower;
	heartreg_t	hr_berr_addr, hr_berr_misc;
	heartreg_t	hr_cause, hr_wid_err_type;

    if (chk_heart && !chk_bridge)
        msg_printf(DBG,"Calling _hb_chkout (heart only)...\n");
    else if (!chk_heart && chk_bridge)
        msg_printf(DBG,"Calling _hb_chkout (bridge only)...\n");
    else msg_printf(DBG,"Calling _hb_chkout...\n");

    d_errors = 0x0;

    if (chk_heart) {
	/* HEART */
	/* Read Heart Identification register */
	PIO_REG_RD_64(HEART_WID_ID, hr_mask, hr_id);
	msg_printf(DBG, "Heart ID = 0x%x\n",hr_id);

	/* Read Heart Status register */
	/*  mask off LLP retry counters and # of pending requests */
	PIO_REG_RD_64(HEART_WID_STAT, 
		~(WIDGET_LLP_REC_CNT | WIDGET_LLP_TX_CNT | WIDGET_PENDING), 
		hr_status);
	/* Verify Heart Widget Status register */
	if (hr_status != D_HEART_STATUS) {
	    msg_printf(ERR, "Heart Widget Status Register Mismatch\n");
	    msg_printf(DBG, "Heart Widget Status = 0x%x, should be 0x%x\n", 
						hr_status, D_HEART_STATUS);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}

	/* Read INT_STATUS register */
	PIO_REG_RD_64(HEART_ISR, hr_mask, hr_int_status);
	/* Verify INT_STATUS register */
	/* NOTE: ignore bit 50 (timer) which can be on/off arbitrarily */
	hr_int_status &= ~HEART_INT_TIMER;
	if (hr_int_status) {
	    msg_printf(ERR, "Heart Interrupt Status Register is not 0\n");
	    msg_printf(DBG, "Heart Interrupt Status = 0x%x\n", 
						hr_int_status);
	    if (hr_int_status & (HEART_INT_EXC << HEART_INT_L4SHIFT)) {
		PIO_REG_RD_64(HEART_CAUSE, hr_mask, hr_cause);
		msg_printf(DBG, "Heart Cause = 0x%x\n",
						hr_cause);
		if (hr_cause & HC_WIDGET_ERR) {
		    PIO_REG_RD_64(HEART_WID_ERR_TYPE, hr_mask, hr_wid_err_type);
		    msg_printf(DBG, "Heart Widget Error Type = 0x%x\n",
						hr_wid_err_type);
		}
	    }
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}

	/* Read WIDGET ERROR UPPER ADDRESS register */
	PIO_REG_RD_64(HEART_WID_ERR_UPPER, hr_mask, hr_err_upper);
	/* Verify error upper register */
	if (hr_err_upper) {
	    msg_printf(ERR, "Heart Widget Error Upper Register is not 0\n");
	    msg_printf(DBG, "Heart Widget Error Upper Register = 0x%x\n", 
					hr_err_upper);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}

	/* Read WIDGET ERROR LOWER ADDRESS register */
	PIO_REG_RD_64(HEART_WID_ERR_LOWER, hr_mask, hr_err_lower);
	/* Verify error lower register */
	if (hr_err_lower) {
	    msg_printf(ERR, "Heart Widget Error Lower Register is not 0\n");
	    msg_printf(DBG, "Heart Widget Error Lower Register = 0x%x, \n", 
					hr_err_lower);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}
	/* it seems that the error registers are not reset but keep
		the last value written to them:
			BUS ERROR ADDRESS register 
			BUS ERROR MISC register */
    } /* chk_heart */

    if (chk_bridge) {
	/* BRIDGE */

	/* TJS - 04/28/1997
	 *   Need to mask off the FLASH_SELECT - Bit 6
	 */
	br_status_default &= ~(1 << 6);

	if ((bridge_xbow_port != BRIDGE_ID) &&
	    (bridge_xbow_port != 13)) { 
		/* GIO bridge */
		br_status_default = D_GIO_BRIDGE_STATUS;
	}
	/* Read Bridge Identification register */
	BR_REG_RD_32(BRIDGE_WID_ID, br_mask, br_id);
	msg_printf(DBG, "Bridge ID = 0x%x\n",br_id);

	/* Read Bridge Status register */
	BR_REG_RD_32(BRIDGE_WID_STAT, br_mask, br_status);
	/* Verify Bridge Status register */
	if ((br_status & br_status_default) != br_status_default) {
	    msg_printf(ERR, "Bridge Status Register Mismatch\n");
	    msg_printf(DBG, "Bridge Status = 0x%x, should be 0x%x\n", 
					br_status, br_status_default);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}
	/* Read INT_STATUS register */
	BR_REG_RD_32(BRIDGE_INT_STATUS, br_mask, br_int_status);
	/* Verify INT_STATUS register */
	if (br_int_status & ~SUPER_IO_INTR) {
	    msg_printf(ERR, "Bridge Interrupt Status Register is not 0\n");
	    msg_printf(DBG, "Bridge Interrupt Status = 0x%x\n", br_int_status);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}

	/* it seems that the error registers are not reset but keep
		the last value written to them:
			ERROR UPPER ADDRESS register 
			ERROR LOWER ADDRESS register */

    } /* chk_bridge */

_error:
	if (d_errors) return(1);
	else return(0);

}

