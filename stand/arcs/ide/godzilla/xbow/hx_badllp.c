/*
 * hx_badllp.c
 *
 * 	runs bad llp tests between Heart and Xbow  (using F_BAD_LLP bit in both)
 * 	This test exercises the retry and squash logic on both heart and xbow.
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

/*
 * NOTE: H=heart, X=crossbow
 */
#ident "ide/godzilla/xbow/hx_badllp.c:  $Revision: 1.8 $"

/*
 * hx_badllp.c - runs bad llp tests between Heart and Xbow  
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/xtalk/xwidget.h"
#include "sys/RACER/heart.h"
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
 * Name:	hx_badllp
 * Description:	runs bad llp tests between Heart and Xbow
 * Input:	none
 * Output:	Returns d_errors
 * Error Handling: no interrupt, using polling
 * Side Effects: 
 * Remarks:     Both H and B interrupts come up during both tests;
 *		I chose to focus on the H interrupt (bad_llp_from_heart) 
 *		and on the B interrupt (bad_llp_from_xbow) 
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
hx_badllp(void)
{
	heartreg_t tmp_hr_buf[3] = {HEART_WID_ERR_MASK, HEART_IMR(0), D_EOLIST};
	heartreg_t tmp_hr_save[3];
	xbowreg_t tmp_xr_buf[11] = {XBOW_WID_REQ_TO, XBOW_WID_CONTROL, \
			XB_LINK_CTRL(XBOW_PORT_8), XB_LINK_CTRL(XBOW_PORT_9), \
			XB_LINK_CTRL(XBOW_PORT_A), XB_LINK_CTRL(XBOW_PORT_B), \
			XB_LINK_CTRL(XBOW_PORT_C), XB_LINK_CTRL(XBOW_PORT_D), \
			XB_LINK_CTRL(XBOW_PORT_E), XB_LINK_CTRL(XBOW_PORT_F), \
								 D_EOLIST};
	xbowreg_t tmp_xr_save[11];
	d_errors = 0;

	/* save all readable/writable H and X registers that are written to */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);
	_xr_save_regs(tmp_xr_buf, tmp_xr_save);
	
	/* reset whole system */
	if (hxb_reset()) {
		d_errors++;
		return(1);
	}

	/* heart, crossbow and bridge check out */
	if (_hxb_chkout(CHK_HEART, CHK_XBOW, DONT_CHK_BRIDGE)) {
		msg_printf(ERR, "Heart/Xbow/Bridge Check Out Error\n");
		d_errors++;
		return(1);
	}

	/* the H->X and X->H tests are sufficiently different */
	/*  for not sharing the rest of the code */
	if ((_hx_badllp_from_heart() || (d_errors != 0)) 
		&& d_exit_on_error) goto _error;
	if (_hx_badllp_from_xbow() || (d_errors != 0)) goto _error;
	msg_printf(DBG,"\n");

_error:
	/* restore all readable/writable H and X registers */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);
	_xr_rest_regs(tmp_xr_buf, tmp_xr_save);

	/* top level function must report the pass/fail status */
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Heart/Xbow ""bad LLP""", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HEART_XBOW_0000], d_errors );
}

/*
 * Name:	_hx_badllp_from_heart
 * Description:	runs bad llp tests from Heart to Xbow
 * Input:	none
 * Output:	Returns 1 if failed, 0 if passed; increments d_errors
 * Error Handling: no interrupt, using polling
 * Side Effects: 
 * Remarks:      the interrupt and error reporting mechanisms are also tested
 * Debug Status: compiles, simulated, not emulated, not debugged
 */
bool_t
_hx_badllp_from_heart(void)
{
	/*
         * with F_BAD_LLP in H 
	 */
	bridgereg_t	bridge_widget_stat;
	heartreg_t	heart_errtype, heart_wid_control;
	heartreg_t	heart_wid_err, heart_wid_err_mask;
	__uint32_t	time_out;
	xbowreg_t	xb_link_control;
	xbowreg_t	xb_link_status;
	

	msg_printf(DBG,"\n_hx_badllp_from_heart... Begin\n");

	/*
	 * set-up: 
	 */
	_hb_disable_proc_intr();	/* disable coproc0 intr*/
	if(_hx_badllp_setup()) { /* generic setup */
		d_errors++;
		return(1);
	}
	/* "from_heart" specific setup */
	/* HEART */
        /* reset the heart cause register */
        PIO_REG_WR_64(HEART_CAUSE, ~0x0, ~0x0);
        _disp_heart_cause();
	/* Enable LLP_TxUpktRetry in the H Widget Error Mask Register */
	PIO_REG_WR_64(HEART_WID_ERR_MASK, ~0x0, ERRTYPE_LLP_TX_RETRY);
	PIO_REG_WR_64(HEART_WID_ERR_MASK, ~0x0, ~0x0);
	PIO_REG_RD_64(HEART_WID_ERR_MASK, ~0x0, heart_wid_err_mask);
	msg_printf(DBG, "_hx_badllp_from_heart... heart_wid_err_mask = 0x%x\n",
				heart_wid_err_mask);

	/* Set H Widget Control Register with F_BAD_LLP */
	/* NOTE: this bit is automatically cleared by a retry */
	d_errors += pio_reg_mod_64(HEART_WID_CONTROL, WIDGET_F_BAD_PKT, 
								SET_MODE);
	PIO_REG_RD_64(HEART_WID_CONTROL, ~0x0, heart_wid_control);
	msg_printf(DBG, "_hx_badllp_from_heart... heart_wid_control = 0x%x\n",
				heart_wid_control);
	/* XBOW */
	/* XXX */
	/* Enable XB_CTRL_RCV_IE bit in LINK_CONTROL (heart) reg */
	d_errors += pio_reg_mod_32(XB_LINK_CTRL(HEART_ID), 
               			XB_CTRL_RCV_IE, SET_MODE);
        XB_REG_RD_32(XB_LINK_CTRL(HEART_ID), ~0x0, xb_link_control);
	msg_printf(DBG, "_hx_badllp_from_heart... xb_link_control(heart) = 0x%x\n", xb_link_control);

	/* 
	 * Cause an error: Read from the bridge (i.e packet to B and back)
	 * 			XXX apropriate here for Xbow system ??
 	 */
	msg_printf(DBG,"_hx_badllp_from_heart... cause an error \n");
	BR_REG_RD_32(BRIDGE_WID_STAT, ~0x0, bridge_widget_stat);
	msg_printf(DBG,"_hx_badllp_from_heart... FYI bridge_widget_stat = 0x%x\n", 
			bridge_widget_stat);

	/* wait for Xbow interrupt */
	xb_link_status = 0; 
	time_out = 0xfff;
	while ((xb_link_status == 0) && (time_out != 0)) {
		XB_REG_RD_32(XB_LINK_STATUS(HEART_ID), ~0x0, xb_link_status);
		time_out--;
	}
	if (time_out == 0) {
		msg_printf(DBG, "_hx_badllp_from_heart... xb_link_status = 0x%x\n", xb_link_status);
		msg_printf(ERR,"*** timed out waiting for xbow interrupt *** \n");
		d_errors++;
                return(1);
	}

	/* 
	 * wait for interrupt
	 * NOTE: the H interrupt is used for waiting
	 */
	if(_hb_wait_for_widgeterr()) {
		msg_printf(ERR, "*** error in _hb_wait_for_widgeterr ***\n");
		d_errors++;
		return(1);
	}

	/*
	 * check registers
	 */
	msg_printf(DBG,"-------------------------------------- \n");
	msg_printf(DBG,"Printing (and checking some) registers \n");
	msg_printf(DBG,"-------------------------------------- \n");
 	_hx_print_retry_counters(); /* XXX no check for now */
        /* check the H Widget error type Register for the LLP_TxUpktRetry bit */
	PIO_REG_RD_64(HEART_WID_ERR_TYPE, ERRTYPE_LLP_TX_RETRY, heart_errtype);
	if ( heart_errtype == 0) {
		msg_printf(ERR,"*** ERRTYPE_LLP_TX_RETRY not set *** \n");
		d_errors++;
		return(1);
	}
        /* print the XB_CTRL_RCV_IE bit in the Xbow link status (heart) reg. */
	/* XXX  no check for now */
	XB_REG_RD_32(XB_LINK_STATUS(HEART_ID), ~0x0, xb_link_status);
	msg_printf(DBG,"_hx_badllp_from_heart... (print only)\n");
	msg_printf(DBG,"\t\tXB_CTRL_RCV_IE bit in the B INT_STATUS (unshifted): %x\n", xb_link_status & XB_CTRL_RCV_IE);
	/* print the H Widget Error Upper/Lower Address Register */ 
	PIO_REG_RD_64(HEART_WID_ERR_UPPER, MASK_16, heart_wid_err);
	msg_printf(DBG,"\t\tH Widget Error Upper Address Register: %4x\n",
					heart_wid_err);
	PIO_REG_RD_64(HEART_WID_ERR_LOWER, MASK_32, heart_wid_err);
	msg_printf(DBG,"\t\tH Widget Error Lower Address Register: %8x\n",
					heart_wid_err);
        /* print the H Widget Error Command Word Register */   
	PIO_REG_RD_64(HEART_WID_ERR_CMDWORD, MASK_32, heart_wid_err);
	msg_printf(DBG,"\t\tH Widget Error Command Word Register: %8x\n",
					heart_wid_err);
	msg_printf(DBG,"-------------------------------------- \n");

	/*
	 * clear the errors 
	 */
	if (hxb_reset()) {
		d_errors++;
		return(1);
	}

	msg_printf(DBG,"_hx_badllp_from_heart. End\n");
	return(0);
}

/*
 * Name:	_hx_badllp_from_xbow
 * Description:	runs bad llp tests from Xbow to Heart
 * Input:	none
 * Output:	Returns 1 if failed, 0 if passed; increments d_errors
 * Error Handling: no interrupt, using polling
 * Side Effects: 
 * Remarks:      the interrupt and error reporting mechanisms are also tested
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
_hx_badllp_from_xbow(void)
{
	/* 
	 * with F_BAD_LLP in X 
	 */
	heartreg_t	heart_isr, heart_wid_err, heart_cause;
	bridgereg_t	bridge_widget_stat;
	__uint32_t	time_out;
	xbowreg_t	xbow_wid_int_upper, xbow_wid_int_lower;
	xbowreg_t	xbow_intr_vector;
	xbowreg_t	xbow_wid_err_lower, xbow_wid_err_upper;
	xbowreg_t	xbow_wid_err_cmdword, xb_link_status;

	msg_printf(DBG,"\n_hx_badllp_from_xbow... Begin\n");
	/*
	 * set-up: 
	 */
	if(_hx_badllp_setup()) { /* generic setup */
		d_errors++;
		return(1);
	}
	/* "from_xbow" specific setup */
	/* Enable the XB_CTRL_XMT_RTRY_IE error bit in the X link control */
	/*	(heart) reg */  
	d_errors += pio_reg_mod_32(XB_LINK_CTRL(HEART_ID),
                                	XB_CTRL_XMT_RTRY_IE, SET_MODE);
	/* Set LLP_RcvCBError and LLP_RcvSquashData in the H Widget */
	/*	Error Mask Register */
	PIO_REG_WR_64(HEART_WID_ERR_MASK, ~0x0, 
			ERRTYPE_LLP_RCV_CB_ERR | ERRTYPE_LLP_RCV_SQUASH_DATA);
        
	/* Set X Control Register with F_BAD_LLP */
	/* NOTE: this bit is automatically cleared by a retry */
	d_errors += pio_reg_mod_32(XB_LINK_CTRL(HEART_ID), XB_CTRL_BAD_LLP_PKT, 
				SET_MODE);

	/* 
	 * Cause an error: Read from the bridge (i.e packet to B and back)
	 * 			XXX apropriate here for Xbow system ??
 	 */
	msg_printf(DBG,"_hx_badllp_from_xbow... cause an error\n");
	BR_REG_RD_32(BRIDGE_WID_STAT, ~0x0, bridge_widget_stat);
	msg_printf(DBG, "_hx_badllp_from_xbow... FYI bridge_widget_stat = 0x%x\n", bridge_widget_stat);

	/* 
	 * wait for interrupt
	 * NOTE: the X interrupt is used for waiting
	 */
	/* poll the X interrupt bit in the H Interrupt Status Register */
	/*  first, determine which bit it is */ 
	XB_REG_RD_32(XBOW_WID_INT_UPPER, ~0x0, xbow_wid_int_upper);
	msg_printf(DBG, "_hx_badllp_from_xbow... xbow_wid_int_upper = %x\n",
			xbow_wid_int_upper);
	XB_REG_RD_32(XBOW_WID_INT_LOWER, ~0x0, xbow_wid_int_lower);
	msg_printf(DBG, "_hx_badllp_from_xbow... xbow_wid_int_lower = %x\n",
			xbow_wid_int_lower);
	xbow_intr_vector = xbow_wid_int_upper >> 24; /* bits 31-24 */
	/* check against assigned vector (default) */
	if (xbow_intr_vector != WIDGET_ERRVEC_BASE) { /* in heart.h */
	/*  check whether it is a good vector (should be) and poll */
		msg_printf(ERR, "*** xbow intr vector (%d) should be %d ***\n",
			xbow_intr_vector, WIDGET_ERRVEC_BASE);
		d_errors++;
		return(1);
	}
	/* NOTE: called "_br" from br_intr.c, but applicable to xbow */
	if (_br_is_valid_vector(xbow_intr_vector)) { /* good vector */
       	/* poll the error bit in the H Interrupt Status Register */
		time_out = 0xfff;
       		do  {
			PIO_REG_RD_64(HEART_ISR, 0x1L<<xbow_intr_vector, 
				heart_isr) ;
			time_out--;
		}	
        	while ((heart_isr == 0) && (time_out !=0));
		if (time_out != 0)
      			msg_printf(DBG,"_hx_badllp_from_xbow... gotten the good intr vector bit in H ISR\n");
		else {	msg_printf(ERR,"*** timed-out waiting for xbow interrupt ***\n");
			d_errors++;
			return(1);
		}
	}
	else 	{ /* bad interrupt vector case */
		msg_printf(ERR,"*** bad intr vector for xbow ***\n");
		d_errors++;
		return(1); 
	}
	/* check the H exception bit in the H Interrupt Status Register */
	PIO_REG_RD_64(HEART_ISR, ((heartreg_t)HEART_ISR_HEART_EXC), heart_isr) ;
	if ( heart_isr == 0 ) {
		msg_printf(ERR,"*** H exception bit not set ***\n");
		d_errors++;
		return(1); 
	}
	/* check the widgeterr bit in the H cause register */
	PIO_REG_RD_64(HEART_CAUSE,HC_WIDGET_ERR,heart_cause);
	if (heart_cause != HC_WIDGET_ERR) { /* i.e. 0 */
		msg_printf(ERR,"*** widget_err not set in H cause ***\n");
		d_errors++;
		return(1); 
	}

	/*
	 * check registers
	 */
	msg_printf(DBG,"-------------------------------------- \n");
	msg_printf(DBG,"Printing (and checking some) registers \n");
	msg_printf(DBG,"-------------------------------------- \n");
	_hx_print_retry_counters(); /* XXX no check for now */
        /* Check that the XB_CTRL_XMT_RTRY_IE bit is set in the Xbow link status (heart) reg. */
	/* XXX  no check for now */
	XB_REG_RD_32(XB_LINK_STATUS(HEART_ID), ~0x0, xb_link_status);
	msg_printf(DBG,"_hx_badllp_from_heart... (print only)\n");
	msg_printf(DBG,"\t\tXB_CTRL_XMT_RTRY_IE bit in the Xbow link status (heart) reg (unshifted): %x\n", xb_link_status & XB_CTRL_XMT_RTRY_IE);
	if (xb_link_status & XB_CTRL_XMT_RTRY_IE == 0) {
		msg_printf(ERR,"*** XB_CTRL_XMT_RTRY_IE bit NOT set in Xbow link status ***\n");
		d_errors++;
		return(1);
	}
	/* print the LLP_SquashData/LLP_RcvCBError bits in the H Wid ErrType*/ 
	/* XXX no check for now */
	PIO_REG_RD_64(HEART_WID_ERR_TYPE, 
			ERRTYPE_LLP_RCV_SQUASH_DATA | ERRTYPE_LLP_RCV_CB_ERR, 
			heart_wid_err);
	msg_printf(DBG,"\t\tLLP_REC_CBERR bit in the H Wid ErrType register %1x\n",
		heart_wid_err & ERRTYPE_LLP_RCV_CB_ERR >> 15);
	msg_printf(DBG,"\t\tLLP_RCV_SQUASH_DATA bit in the H Wid ErrType reg %1x\n",
		heart_wid_err & ERRTYPE_LLP_RCV_SQUASH_DATA >> 14);
	/* print the X Error Lower/Upper Address Register */
	XB_REG_RD_32(XBOW_WID_ERR_UPPER, MASK_16, xbow_wid_err_upper);
	msg_printf(DBG,"\t\tX Error Upper Address Register: %4x\n",
				xbow_wid_err_upper);
	XB_REG_RD_32(XBOW_WID_ERR_LOWER, MASK_32, xbow_wid_err_lower);
	msg_printf(DBG,"\t\tX Error Lower Address Register: %8x\n",
				xbow_wid_err_lower);
        /* print the X Error Command Word Register */  
	XB_REG_RD_32(XBOW_WID_ERR_CMDWORD, MASK_32, xbow_wid_err_cmdword);
	msg_printf(DBG,"\t\tX Widget Error Command Word Register: %8x\n",
				xbow_wid_err_cmdword);
	msg_printf(DBG,"-------------------------------------- \n");

	/*
	 * clear the errors 
	 */
	if (hxb_reset()) {
		d_errors++;
		return(1);
	}

	msg_printf(DBG,"_hx_badllp_from_xbow... End\n");
	return(0);
}


/*
 * Name:	_hx_badllp_setup
 * Description:	sets the heart and xbow correctly, before the test
 *		does some checking (i.e. reset registers *are* reset)
 * Input:	none
 * Output:	Returns 1 if failed, 0 if passed
 * Remarks:     some of this is done in hb_status (XXX remove redundancies) 
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
_hx_badllp_setup(void)
{
	heartreg_t	hr_wid_errcmd; /* for H */
	xbowreg_t	xb_wid_errcmd; /* for X */
	xbowreg_t	xbow_retry_counter;
	heartreg_t	hr_retry_counter;
	char		index;	/* from 8 to f */

	/*
	 * xbow setup 
	 */
	/* clear retry counters by clearing the link status registers */
	/*  which also clears the link auxiliary status registers */
	if (x_reset) {
		d_errors++;
		if (d_exit_on_error) return(1);
	}
        /* set Request TimeOut Reg. */
	XB_REG_WR_32(XBOW_WID_REQ_TO, ~0x0, D_XBOW_WID_REQ_TO);
	/* Disable all Xbow intr (they are inividually enabled in tests) */
        XB_REG_WR_32(XBOW_WID_CONTROL, ~0x0, 0x0);
	for (index = XBOW_PORT_8; index <= XBOW_PORT_F; index++) 
		XB_REG_WR_32(XB_LINK_CTRL(index), ~0x0, 0x0);

	/*
	 * heart setup 
	 */
        /* disable the heart interrupts by setting the intr masks to 0 */
	PIO_REG_WR_64(HEART_IMR(0), ~0x0, 0x0);
	PIO_REG_WR_64(HEART_WID_ERR_MASK, ~0x0, 0x0);
	/* Clear the retry counters in the H widget control register */
	d_errors += pio_reg_mod_64(HEART_WID_CONTROL,
			WIDGET_CLR_RLLP_CNT | WIDGET_CLR_TLLP_CNT, SET_MODE);
	/* reset the Heart interrupts */
	PIO_REG_WR_64(HEART_CLR_ISR,  ~0x0, ~0x0);
	PIO_REG_WR_64(HEART_WID_ERR_CMDWORD, ~0x0, ~0x0);

	/* 
	 * checks 
	 */
       	/* read the Xbow Widget Error Command Word Reg and check that */
	/*  no error has occured (ERROR bit) */
	XB_REG_RD_32(XBOW_WID_ERR_CMDWORD, ~0x0, xb_wid_errcmd);
	if ( xb_wid_errcmd & 0x1L<<9 ) {  /* ERROR bit */
		msg_printf(ERR,"*** _hx_badllp_setup: ERROR bit set in WID_ERR_CMDWORD ***\n");
		return(1);
	}
       	/* read the H Widget Error Command Word Reg and check that */
	/*  no error has occured (ERROR bit) */
	PIO_REG_RD_64(HEART_WID_ERR_CMDWORD, ~0x0, hr_wid_errcmd);
	if ( hr_wid_errcmd & 0x1L<<9 ) {  /* ERROR bit */
		msg_printf(ERR,"*** _hx_badllp_setup: ERROR bit set in WID_ERR_CMDWORD ***\n");
		return(1);
	}

	/* Read the retry counters: (on X in the aux status register, */
        /*  on H in the H Widget Status Register); should be 0. */
	XB_REG_RD_32(XB_LINK_AUX_STATUS(HEART_ID),
                                WIDGET_LLP_REC_CNT | WIDGET_LLP_TX_CNT,
                                xbow_retry_counter);
	if (xbow_retry_counter != 0) {
		msg_printf(ERR,"*** xbow retry counters not reset *** \n");
		return(1);
	}
	PIO_REG_RD_64(HEART_WID_STAT, WIDGET_LLP_REC_CNT | WIDGET_LLP_TX_CNT,
								hr_retry_counter);
	if (hr_retry_counter != 0) {
		msg_printf(ERR,"*** heart retry counters not reset *** \n");
		return(1);
	}
	return(0); /* function completed */
}

/*
 * Name:	_hx_print_retry_counters
 * Description:	Reads the H and X retry counters and prints them.
 * Input:	none
 * Output:	msg_printf msg (none if verbosity is low). Returns 0, always.
 * Remarks:      
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
_hx_print_retry_counters(void)
{
	__int64_t	retry_cntr;
	char		index;

	/* heart: */
        PIO_REG_RD_64(HEART_WID_STAT, WIDGET_LLP_REC_CNT | WIDGET_LLP_TX_CNT,
                                                                retry_cntr);
	msg_printf(DBG," Heart retry counters: \n \
			\t\tReceive %2x\n, \t\tTransmit %2x\n",
			MASK_8 & (retry_cntr & WIDGET_LLP_REC_CNT >> 24), 
			MASK_8 & (retry_cntr & WIDGET_LLP_TX_CNT >> 16) );

	/* crossbow: */
	for (index = XBOW_PORT_8; index <= XBOW_PORT_F; index++) {
        	XB_REG_RD_32(XB_LINK_AUX_STATUS(index), 
				WIDGET_LLP_REC_CNT | WIDGET_LLP_TX_CNT,
                                retry_cntr);
		msg_printf(DBG," Xbow retry counters for link %1x: \n \
			\t\tReceive %2x\n, \t\tTransmit %2x\n",
			MASK_8 & (retry_cntr & WIDGET_LLP_REC_CNT >> 24),
			MASK_8 & (retry_cntr & WIDGET_LLP_TX_CNT >> 16));
	}

	return(0);
}
