/*
 * hr_intr.c : 
 *	Heart Interrupt Register Tests
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

#ident "ide/godzilla/heart/hr_intr.c:  $Revision: 1.13 $"

/*
 * hr_intr.c - Heart Interrupt Register Tests
 * 
 * NOTE: some events are only hardware setable, in which case
 *	  this test would fail. ?? XXX
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
 * Global Variables
 */
/* XXX implement more patterns */
heartreg_t	hr_isr_pattern[] = 
{ 0x0f0f0f0f0f0f0f0f, ~0x0f0f0f0f0f0f0f0f,
  0x00ff00ff00ff00ff, ~0x00ff00ff00ff00ff,
  0x1122334455667788, ~0x1122334455667788,
  0xcccccccccccccccc, ~0xcccccccccccccccc,
  0x3333333333333333, ~0x3333333333333333};
heartreg_t	hr_im0_pattern[] = 
{ 0x1122334455667788, ~0x1122334455667788,
  0x0f0f0f0f0f0f0f0f, ~0x0f0f0f0f0f0f0f0f,
  0x00ff00ff00ff00ff, ~0x00ff00ff00ff00ff,
  0x3333333333333333, ~0x3333333333333333,
  0xcccccccccccccccc, ~0xcccccccccccccccc};

/*
 * Name:	hr_intr.c
 * Description:	tests the interrupt registers in the Heart
 * Input:	none
 * Output:	Returns d_errors
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, not emulated, not debugged
 */
bool_t
hr_intr()
{
	heartreg_t	s_isr, s_im0, s_im1, s_im2, s_im3;
	heartreg_t	mask = ~0x0;
	heartreg_t	all_ones = ~0x0;
	heartreg_t	set_isr_wr, isr_rd, im0_wr, imsr_rd, i;
	heartreg_t	tmp_hr_buf[5] = {HEART_IMR(0), HEART_IMR(1), 
					HEART_IMR(2), HEART_IMR(3),
					D_EOLIST};
	heartreg_t	tmp_hr_save[5];

	d_errors = 0;

	msg_printf(DBG, "Heart Interrupt Registers Test... Begin\n");

	/* save registers first */
	msg_printf(DBG, "Heart Interrupt Registers Test... save registers\n");
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);

	/* reset the heart */
	if (_hb_reset(RES_HEART, DONT_RES_BRIDGE)) {
		d_errors++;
		if (d_exit_on_error) goto _error;
	}
	
	/* 
	 * Write 0 in Set ISR register 
	 * This should not affect the ISR value
	 */
	msg_printf(DBG, "Heart Interrupt Registers Test... HEART_SET_ISR test\n");
	PIO_REG_WR_64(HEART_SET_ISR, mask, 0x0);
	PIO_REG_RD_64(HEART_ISR, mask, isr_rd);
	if (isr_rd) {
	    msg_printf(ERR, "Heart ISR is not cleared\n");
	    msg_printf(DBG, "Heart Int Test: SET_ISR = 0x%x; ISR = 0x%x\n", 
			0x0, isr_rd);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}

	/* walking 1 test on ISR */
	msg_printf(DBG, "Heart Interrupt Registers Test... walking 1 test\n");
	for (i = 0; i < sizeof(heartreg_t)*8; i++) {
	    /* There are some bits in the Interrupt Status Register */
	    /* 	that are immune from this setting function. */
	    /* These interrupt status bits are: HEART_Exc, PPErr(3:0), */
	    /*  Timer_IP, FC_TimeOut(1:0) and IR. */
	    if ( ((i<=INT_STAT_VECT_EXC) && (i>=HEART_INT_L3SHIFT))
		 || (i<=INT_STAT_VECT_FC_TO_1) ) {
	        msg_printf(DBG, "vector %d is immune to setting (internal intr)\n", i);
	    }
	    else {	
		set_isr_wr = (HEARTCONST 0x1) << i;
		PIO_REG_WR_64(HEART_SET_ISR, mask, set_isr_wr);
		PIO_REG_RD_64(HEART_ISR, mask, isr_rd);
		if (isr_rd & ~HEART_INT_TIMER != set_isr_wr) {
		    msg_printf(ERR, "Heart ISR is not set right\n");
		    msg_printf(DBG, "Heart Int Test (bit %d): SET_ISR = 0x%x; ISR = 0x%x\n", i, set_isr_wr, isr_rd & ~HEART_INT_TIMER);
		    d_errors++;
		    if (d_exit_on_error) goto _error;
	        }
		/* clear the ISR */
		PIO_REG_WR_64(HEART_CLR_ISR, mask, set_isr_wr);
		/* verify it is cleared */
		PIO_REG_RD_64(HEART_ISR, mask, isr_rd);
		if (isr_rd & ~HEART_INT_TIMER) { /* mask the timer intr (bit 50) */
	    	    msg_printf(ERR, "Heart ISR (0x%x) is not cleared\n", isr_rd);
	    	    d_errors++;
	    	    if (d_exit_on_error) goto _error;
		}
	    }
	}

	/* Some bits are immune to setting: some of these are immune */
	/*  to clearing. */
	/* The bits that are immune to both set and reset function are sort */
	/*  of "read-only" bits. */
	/*  The change of the state is not software controllable. */
	/* The bits that are immune only to the set function, */
	/*  but not to the clear function, are the bits that can only */
	/*  be set by hardware events, but needs to be cleared by software. */

	/* in the pattern tests, this is dealt with with INT_STAT_IMMUNE_MSK */

	/* pattern test on ISR */
	msg_printf(DBG, "Heart Interrupt Registers Test... ISR pattern test\n");
	for (i = 0; i < sizeof(hr_isr_pattern)/sizeof(heartreg_t); i++) {
	    /* clear the ISR first */
	    PIO_REG_WR_64(HEART_CLR_ISR, mask, all_ones);
	    set_isr_wr = hr_isr_pattern[i];
	    PIO_REG_WR_64(HEART_SET_ISR, mask, set_isr_wr);
	    PIO_REG_RD_64(HEART_ISR, INT_STAT_IMMUNE_MSK, isr_rd);
	    if (isr_rd & ~HEART_INT_TIMER != (set_isr_wr & INT_STAT_IMMUNE_MSK)) {
		msg_printf(ERR, "Heart ISR is not set right\n");
		msg_printf(DBG, "Heart Int Test: SET_ISR (&MSK) = 0x%x; ISR = 0x%x\n", set_isr_wr & INT_STAT_IMMUNE_MSK, isr_rd & ~HEART_INT_TIMER);
		d_errors++;
		if (d_exit_on_error) goto _error;
	    }
	}

	/* pattern test on ISR and IMSR */
	/* XXX: Right now, we test this for processor 0 */
	msg_printf(DBG, "Heart Interrupt Registers Test... ISR/IMSR pattern test\n");
	for (i = 0; i < sizeof(hr_isr_pattern)/sizeof(heartreg_t); i++) {
	    /* clear the ISR first */
	    PIO_REG_WR_64(HEART_CLR_ISR, mask, all_ones);
	    set_isr_wr = hr_isr_pattern[i];
	    im0_wr = hr_im0_pattern[i];
	    PIO_REG_WR_64(HEART_SET_ISR, mask, set_isr_wr);
	    PIO_REG_WR_64(HEART_IMR(0), mask, im0_wr);
	    PIO_REG_RD_64(HEART_IMSR, INT_STAT_IMMUNE_MSK, imsr_rd);
	    PIO_REG_RD_64(HEART_ISR, INT_STAT_IMMUNE_MSK, isr_rd);
	    if (imsr_rd & ~HEART_INT_TIMER != (isr_rd & im0_wr)) {
		msg_printf(ERR, "Heart IMSR is not set right\n");
		msg_printf(DBG, "Heart Int Test: SET_ISR (&MSK) = 0x%x; IM0 = 0x%x; ISR = 0x%x; IMSR = 0x%x\n", 
		   set_isr_wr & INT_STAT_IMMUNE_MSK, im0_wr, isr_rd, imsr_rd & ~HEART_INT_TIMER);
		d_errors++;
		if (d_exit_on_error) goto _error;
	    }
	}

	/* reset the heart */
	if (_hb_reset(RES_HEART, DONT_RES_BRIDGE)) {
		d_errors++;
		if (d_exit_on_error) goto _error;
	}

_error:
	/* clear the ISR first */
	msg_printf(DBG, "Heart Interrupt Registers Test... restore regs and clear ISR\n");
	PIO_REG_WR_64(HEART_CLR_ISR, mask, all_ones);

	/* restore all H registers that are written to in this module */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);

	msg_printf(DBG, "Heart Interrupt Registers Test... End\n");

	/* top level function must report the pass/fail status */
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Heart Interrupt Registers", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HEART_0000], d_errors );
}
