/*
 * x_acc.c : 
 *	Xbow Access Tests
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

#ident "ide/godzilla/xbow/x_acc.c:  $Revision: 1.14 $"

/*
 * x_acc.c - Xbow Access Tests
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/xtalk/xbow.h"
#include "sys/xtalk/xwidget.h"
#include "sys/RACER/heart.h"
#include "sys/PCI/bridge.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h" /* instead of Forward References */
#include <fault.h>
#include <setjmp.h>

extern  bool_t x_regs_dump(void);

static jmp_buf fault_buf;
/*
 * Forward References 
 */

/*
 * Global Variables
 */
/* struture is {register address, access mode} */
Xbow_Regs_Access	xbow_regs_access[XBOW_ACC_REGS_MAX] = {
/* no write only crossbow register */
{ "XBOW_WID_ID",	XBOW_WID_ID,		DO_A_WRITE},
{ "XBOW_WID_UNDEF",	XBOW_WID_UNDEF,		DO_A_READ}, 
{ "XBOW_WID_UNDEF",	XBOW_WID_UNDEF,		DO_A_WRITE},
};

/*
 * Name:	x_acc.c
 * Description:	tests accesss in the Xbow
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
x_acc(void)
{
	Xbow_Regs_Access	*regs_ptr = xbow_regs_access;
	xbowreg_t		mask = ~0x0; /* not changed */
	heartreg_t		h_mask = ~0x0; /* not changed */
	heartreg_t		heart_isr;
	__uint64_t		index, time_out = 0xfff; 
	xbowreg_t		saved_wid_control;
	xbowreg_t		saved_reg_val, saved_wid_int_upper;
	__uint32_t		intr_vect;
	xbowreg_t		xbow_widget0_status;
	xbowreg_t		xbow_wid_err_upper, xbow_wid_err_lower;

        /*
        * Test the error registers (really the access error register):
	*  test in several ways, either by writing or reading from 
	*  read-only or write-only registers resp. 
  	*  OR from an undefined register (4 tests).
	*/
	msg_printf(DBG,"Xbow Access Tests... Begin\n");

	d_errors = 0; /* clear error count */

	/* reset ASIC state */
	if (hxb_reset()) {
		msg_printf(ERR, "Heart/Xbow/Bridge Reset Error\n");
		d_errors++;
		if (d_exit_on_error) goto _error;
	}

	/* heart and Bridge check out */
	if (_hb_chkout(CHK_HEART, CHK_BRIDGE)) {
		msg_printf(ERR, "Heart/Bridge Check Out Error\n");
		d_errors++;
		if (d_exit_on_error) goto _error;
	}
#if DEBUG
	x_regs_dump();
#endif
	_hb_disable_proc_intr(); /* disable the coprocessor0 intr */
	/* disable the heart intr */
	PIO_REG_WR_64(HEART_IMR(0), ~0, 0);
	BR_REG_WR_32(BRIDGE_INT_ENABLE, ~0, 0);

	/* print and save the Xbow control register */
	XB_REG_RD_32(XBOW_WID_CONTROL, mask, saved_wid_control);
	msg_printf(DBG,"Xbow Access Tests...FYI Xbow control register 0x%x\n",
			saved_wid_control);
	/* enable only the intr on access error bit; disable the others */
	XB_REG_WR_32(XBOW_WID_CONTROL, mask, XB_WID_CTRL_REG_ACC_IE);

	/* print and save the Xbow int vector */
	XB_REG_RD_32(XBOW_WID_INT_UPPER, mask, saved_wid_int_upper);
	intr_vect = (saved_wid_int_upper & WIDGET_INT_VECTOR)
			>>WIDGET_INT_VECTOR_SHFT;
	msg_printf(SUM,"Xbow Access Tests... FYI Xbow intr vector %d\n",
			intr_vect);

	/* for each one of the possible error causes */
        for (index = 0; index < XBOW_ACC_REGS_MAX; index++, regs_ptr++) {
		msg_printf(INFO,"Xbow Access Tests...Testing %s, %s \n",
			regs_ptr->name,
			(regs_ptr->access_mode)==DO_A_READ? " Rd":" Wr");
		/* 1. reset the heart (and bridge) and xbow */
		if (hxb_reset()) {
			d_errors++;
			if (d_exit_on_error) goto _error;
		}  
		/* nofault = fault_buf; */
		if (setjmp(fault_buf)) {
			/* reading/writing from/to an undefined register */
			msg_printf(DBG,"Exception:\n");
#if DEBUG
			x_regs_dump();
#endif
 			/* 3. don't wait for the Xbow intr bit in the heart ISR */

			/* 4. read the Xbow Widget0 Status register and check */
			/*	the Register Access Error bit */
			XB_REG_RD_32(XBOW_WID_STAT, mask, xbow_widget0_status);
			msg_printf(DBG,"\t\t\t\tXbow Widget Status Register: 0x%016x\n",
				xbow_widget0_status);
			if (xbow_widget0_status & XB_WID_STAT_REG_ACC_ERR == 0) {
				d_errors++;
				msg_printf(ERR, "*** access error bit not set ***\n");
	   			if (d_exit_on_error) goto _error;
			}
			/* 5. read the xbow error upper/lower addr register and */
			/* 	check their values */
			msg_printf(DBG,"Xbow Access Tests... addr registers:\n");
			XB_REG_RD_32(XBOW_WID_ERR_UPPER, mask, xbow_wid_err_upper);
			msg_printf(DBG,"\t\t\t\tupper addr register: 0x%x\n",
						xbow_wid_err_upper);
			msg_printf(DBG,"\t\t\t\tlower addr register: 0x%x\n",
                                                xbow_wid_err_lower);
			XB_REG_RD_32(XBOW_WID_ERR_LOWER, mask, xbow_wid_err_lower);
			if ((xbow_wid_err_upper & WIDGET_ERR_UPPER_ADDR_ONLY)
			!= ((regs_ptr->register_addr >> D_UPPER_ADDR_SHIFT) & 
				WIDGET_ERR_UPPER_ADDR_ONLY)) {
				msg_printf(ERR, "*** upper addresses don't match***\n");
				d_errors++;
				if (d_exit_on_error) goto _error;
			}
			nofault = 0;
		}	
		else /* NORMALLY */ {
		    nofault = fault_buf; 
		    /* 2. cause a Access error (one of 3 ways) */
		    switch (regs_ptr->access_mode) {  
			/* saved_reg_val is trashed: its value is irrelevant */
			case DO_A_READ: /* read from a WO or undef. register */
			    XB_REG_RD_32(regs_ptr->register_addr, mask,
							saved_reg_val); 
			    break;
		    	case DO_A_WRITE: /* write to a RO or undef. register */
			    XB_REG_WR_32(regs_ptr->register_addr, mask,
							saved_reg_val); 
			    break;
		        default: 
			    msg_printf(ERR,"*** wrong access mode ***\n"); 
			    break;
		    }
#if DEBUG
		    x_regs_dump();
#endif
 		    /* 3. wait for the Xbow intr bit in the heart ISR */
		    /*  	only for Writing an undef reg */
		    if ((regs_ptr->access_mode==DO_A_READ) && 
		  	(strcmp(regs_ptr->name,"XBOW_WID_UNDEF")==0)) {
			time_out = 0xfff;
			do {    PIO_REG_RD_64(HEART_ISR, h_mask, heart_isr) ;
				time_out --;
		    	}
			while ((heart_isr == 0) && (time_out != 0));
			PIO_REG_RD_64(HEART_ISR, h_mask, heart_isr);
		    	if (time_out == 0) {
			    msg_printf(ERR,"*** time-out waiting for the intr\n");
			    d_errors++;
			    if (d_exit_on_error) goto _error;
		        }
		        PIO_REG_RD_64(HEART_ISR, h_mask, heart_isr);
		        if (heart_isr & (0x1L<<intr_vect)) 
			    msg_printf(DBG, "\t\t\t\tXbow intr bit was set. \n");
		        else {  
			    msg_printf(ERR,"*** wrong intr vector bit in H ISR (heart_isr 0x%x, vs 0x%x) \n", heart_isr, (0x1L<<intr_vect) );
			    d_errors++;
			    if (d_exit_on_error) goto _error;
		 	}
		    }
		    /* 4. read the Xbow Widget0 Status register and check */
		    /*	the Register Access Error bit */
		    XB_REG_RD_32(XBOW_WID_STAT, mask, xbow_widget0_status);
		    msg_printf(DBG,"\t\t\t\tXbow Widget Status Register: 0x%016x\n",
				xbow_widget0_status);
		    if (xbow_widget0_status & XB_WID_STAT_REG_ACC_ERR == 0) {
			d_errors++;
			msg_printf(ERR, "*** access error bit not set ***\n");
	   		if (d_exit_on_error) goto _error;
		    }
		    /* 5. read the xbow error upper/lower addr register and */
		    /* 	check their values */
		    msg_printf(DBG,"Xbow Access Tests... addr registers:\n");
		    XB_REG_RD_32(XBOW_WID_ERR_UPPER, mask, xbow_wid_err_upper);
		    msg_printf(DBG,"\t\t\t\tupper addr register: 0x%x\n",
						xbow_wid_err_upper);
		    msg_printf(DBG,"\t\t\t\tlower addr register: 0x%x\n",
                                                xbow_wid_err_lower);
		    XB_REG_RD_32(XBOW_WID_ERR_LOWER, mask, xbow_wid_err_lower);
		    if ((xbow_wid_err_upper & WIDGET_ERR_UPPER_ADDR_ONLY)
			!= ((regs_ptr->register_addr >> D_UPPER_ADDR_SHIFT) & 
				WIDGET_ERR_UPPER_ADDR_ONLY)) {
			msg_printf(ERR, "*** upper addresses don't match***\n");
			d_errors++;
		    }
		}
	} /* loop: for each one of the possible error causes */

_error:
	if (_hb_reset(RES_HEART, RES_BRIDGE)) d_errors++; /* reset H/B */
	/* note: keep these calls separate, to make sure both are run */
	if (x_reset()) d_errors++; /* reset Xbow */

	/* restore the Xbow widget 0 control register */
	XB_REG_WR_32(XBOW_WID_CONTROL, mask, saved_wid_control);

#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Xbow Access", D_FRU_FRONT_PLANE);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_XBOW_0000], d_errors );
}

/* function name:       x_reset
 * input:               none
 * output:              resets the major registers in crossbow
 *			XXX integrate to hb_reset in 
 *				godzilla/heart_xbow/hb_util.c?
 * status:		compiles
 */
bool_t
x_reset(void)
{
	xbowreg_t	xbow_widget0_status;
	xbowreg_t	xbow_wid_err_cmdword;
	xbowreg_t	x_link_status_clr, x_link_status;
	xbowreg_t	mask = ~0x0;
	char		index; /* 8 through f */

	msg_printf(DBG,"Calling x_reset...\n");
	/* Crossbow */

	/* clear the link status registers */
	/*	so that the corresponding intr are reset in the Xbow stat reg */
	/*	i.e. read reg. from their read&clear addresses */
	for (index = XBOW_PORT_8; index <= XBOW_PORT_F; index++) {
		if ((index != XBOW_PORT_8) || (index != XBOW_PORT_F))
			continue;
		XB_REG_RD_32(XB_LINK_STATUS(index), mask, 
			x_link_status);
		if ((x_link_status & XB_LINK_STATUS_MASK) != XB_LINK_STATUS_DEFAULT) {
			msg_printf(DBG,"\t\t\t non-zero link %1x status before clearing: 0x%x\n", index, x_link_status);
		}
		XB_REG_RD_32(XB_LINK_STATUS_CLR(index), mask, 
			x_link_status_clr);
		if ((x_link_status_clr & XB_LINK_STATUS_MASK) != XB_LINK_STATUS_DEFAULT) {
			msg_printf(ERR,"\t\t\t non-zero link %1x status after clearing: 0x%x\n", index, x_link_status_clr);
			return(1);
		}
		if (x_link_status != x_link_status_clr) {
			msg_printf(ERR,"\t\t\t non-matching link status in the status register and in the Read&Clear register");
			return(1);
		}
	}

	/* reset the Xbow status register by reading its read&clear address */
	XB_REG_RD_32(XBOW_WID_STAT_CLR, ~0x0, xbow_widget0_status);
	XB_REG_RD_32(XBOW_WID_STAT, mask, xbow_widget0_status);
	if ((xbow_widget0_status & XBOW_WID0_BITS_CLR)!=0) {
		msg_printf(DBG,"xbow_widget0_status = 0x%08x\n",
			xbow_widget0_status);
		msg_printf(ERR, "Xbow status was not reset\n");
		return(1);
	}
	/* reset the error command word register (hence the upper/lower addr) */
	XB_REG_WR_32(XBOW_WID_ERR_CMDWORD, mask, ~0x0);
	XB_REG_RD_32(XBOW_WID_ERR_CMDWORD, mask, xbow_wid_err_cmdword);
	if (xbow_wid_err_cmdword) {
		msg_printf(ERR, "Xbow xbow_wid_err_cmdword was not reset\n");
		return(1);
	}

	return(0);
}
