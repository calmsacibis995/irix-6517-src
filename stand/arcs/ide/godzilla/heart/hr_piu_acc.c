/*
 * hr_piu_acc.c : 
 *	Heart PIU Access Tests
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

#ident "ide/godzilla/heart/hr_piu_acc.c:  $Revision: 1.18 $"

/*
 * hr_piu_acc.c - Heart PIU Access Tests
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
#include <fault.h>
#include <setjmp.h>

static jmp_buf fault_buf;

/*
 * Global Variables
 */
/* struture is {register address, access mode} */
Heart_Regs_PIU_Access	heart_regs_PIU_access[HEART_REGS_PIU_MAX] = {
{ "HEART_CLR_ISR",	HEART_CLR_ISR,		DO_A_READ},
{ "HEART_ISR",		HEART_ISR,		DO_A_WRITE},
{ "HEART_PIU_UNDEF",	HEART_PIU_UNDEF,	DO_A_READ}, 
{ "HEART_PIU_UNDEF",	HEART_PIU_UNDEF,	DO_A_WRITE},
};

/*
 * Name:	hr_piu_acc.c
 * Description:	tests PIU accesss in the Heart
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
hr_piu_acc()
{
	Heart_Regs_PIU_Access	*regs_PIU_ptr = heart_regs_PIU_access;
	heartreg_t		mask = ~0x0;
	__uint64_t		index, time_out = 0xfff; 
	heartreg_t		piur_acc_err, saved_mode;
	heartreg_t		saved_reg_val, heart_cause = 0;

        /*
        * Test the error registers (really the PIU access error register):
        *
        *  the Processor Bus Error (addr and miscs) and 
	*  Memory error (addr/data and bad memory data) 
	*  registers are used and tested with the ecc tests.
	* the status register value is tested with the R/W tests,
        *  since it does not change
        * the only register that's left for functional testing is 
	*  the PIU Access Error Register, which can be tested in several
 	*  ways, either by writing or reading from read-only or write-only
  	*  registers resp. OR from an undefined register (4 tests).
	*  1. clear the heart exception cause register PIU_AccessErr bit
	*  2. cause a PIU Access error (one of 4 ways)
	*  3. check the heart exception cause register for PIU_AccessErr 
	*      (bit 42 of the Cause register): wait for the bit to be set
	*  4. read the PIU Access Error Register and check its contents
	*  5. repeat from 2. for the next testing way
	*/
	msg_printf(DBG,"Heart PIU Access Tests... Begin\n");

	d_errors = 0; /* clear error count */

	/* heart check out */
	if (_hb_chkout(CHK_HEART, DONT_CHK_BRIDGE)) {
		msg_printf(ERR, "Heart/Bridge Check Out Error\n");
		d_errors++;
		goto _error;
	}

	/* print Heart mode */
	PIO_REG_RD_64(HEART_MODE, mask, saved_mode);
	msg_printf(SUM,"Heart PIU Access Tests... FYI Heart mode 0x%x\n",saved_mode);


	/* for each one of the possible error causes */
        for (index = 0; index < HEART_REGS_PIU_MAX; index++, regs_PIU_ptr++) 
	{
		msg_printf(DBG,"Heart PIU Access Tests... Testing %s, %s \n",regs_PIU_ptr->name,((regs_PIU_ptr->access_mode)==DO_A_READ) ? " Rd":" Wr");
		/* 1. clear the exception cause register PIU_AccessErr bit */
		/* a one clears the particular bit */
		PIO_REG_WR_64(HEART_CAUSE, mask, HC_PIUR_ACC_ERR);
		PIO_REG_RD_64(HEART_CAUSE, mask, heart_cause); /*verify it's 0*/
		if (heart_cause & HC_PIUR_ACC_ERR) {
			d_errors++;
			msg_printf(ERR, "*** heart cause not reset \n");
	   		if (d_exit_on_error) goto _error;
		}
		nofault = fault_buf;
		/* 2. cause a PIU Access error (one of 4 ways) */
		if (setjmp(fault_buf))
		{
		time_out = 0xfff;
 		/* 3. wait for the PIU_AccessErr bit in the heart exception */
		heart_cause = 0;
		while(((heart_cause & HC_PIUR_ACC_ERR)==0) && (time_out > 0)) {
			PIO_REG_RD_64(HEART_CAUSE, ~0x0, heart_cause);
			time_out--;
		}
#if EMULATION
		if (time_out == 0) {
			msg_printf(ERR, "PIU_AccessErr bit Poll Test FAILED\n");
			d_errors++;
			if (d_exit_on_error) goto _error; 
		}
		else msg_printf(SUM, "\t\t\t\tPIU_AccessErr bit was set. \n");
#endif
		/* 4. read the PIU Access Error Register & check its contents*/
		PIO_REG_RD_64(HEART_PIUR_ACC_ERR, mask, piur_acc_err);
		msg_printf(DBG, "\t\t\t\tPIU Access Error Register: 0x%016x\n",
				piur_acc_err);
		if (piur_acc_err & HPE_ACC_TYPE_WR != 
						regs_PIU_ptr->access_mode) {
			d_errors++;
			msg_printf(ERR, "*** access type error \n");
	   		if (d_exit_on_error) goto _error;
		}
		if (piur_acc_err & HPE_ACC_PROC_ID != 0) { /* 0= proc 0 */
			/* XXX-MP  revise for MP machine */
			d_errors++;
			msg_printf(ERR, "*** processor number error \n");
	   		if (d_exit_on_error) goto _error;
		}
		if (piur_acc_err & HPE_ACC_ERR_ADDR != 
			regs_PIU_ptr->register_addr & HPE_ACC_ERR_ADDR) {
			/* only 20 bits: */
			/* the error address is only 20 bit long because this */
			/*  register will not be loaded unless the erred */
			/*  access has the address 0x00_0FFx_xxxx. Hence, the */
			/*  upper address is redundant and omitted. */
			d_errors++;
			msg_printf(ERR, "*** address error \n");
	   		if (d_exit_on_error) goto _error;
		}
		nofault = 0;

		}
		else
		{
		switch (regs_PIU_ptr->access_mode) {  
			/* saved_reg_val is trashed: its value is irrelevant */
			case DO_A_READ: /* read from a WO or undef. register */
				PIO_REG_RD_64(regs_PIU_ptr->register_addr, mask,
							saved_reg_val); 
				break;
			case DO_A_WRITE: /* write to a RO or undef. register */
				PIO_REG_WR_64(regs_PIU_ptr->register_addr, mask,
							saved_reg_val); 
				break;
			default: 
				msg_printf(ERR,"*** wrong access mode ***\n"); 
				break;
		}

		}
	} /* loop: for each one of the possible error causes */

	nofault = 0;
	/* reset the heart and bridge */
        if (_hb_reset(RES_HEART, DONT_RES_BRIDGE)) {
		d_errors++;
		if (d_exit_on_error) goto _error;
	}

_error:
	/* reset the HC_PIUR_ACC_ERR bit in the cause register */
	PIO_REG_WR_64(HEART_CAUSE, mask, HC_PIUR_ACC_ERR);
	PIO_REG_RD_64(HEART_CAUSE, mask, heart_cause); /*verify it's 0*/
	if (heart_cause & HC_PIUR_ACC_ERR) {
		d_errors++;
		msg_printf(ERR, "*** heart cause not reset \n");
	}

	msg_printf(DBG,"Heart PIU Access Tests... End\n");

#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Heart PIU Access", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HEART_0003], d_errors );
}

