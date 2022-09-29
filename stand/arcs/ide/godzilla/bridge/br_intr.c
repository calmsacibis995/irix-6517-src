/*
 * br_intr.c - Tests  the interrupt mechanism in the Bridge
 *		Since the heart is involved, this can be considered a
 *		heart-bridge system interrupt test. (see heart_bridge/Plan)
 *
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

#ident "ide/godzilla/bridge/br_intr.c: $Revision 1.1$"

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
 * Name:	br_intr
 * Description:	Tests  the interrupt mechanism in the Bridge
 *		- programs all intr vectors (good and bad)
 *		- calls the module exercising the RESET_INT_STATUS reg
 * Input:	None
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: no interrupt, using polling
 * Side Effects: all intr H/B registers are affected, 
 *		 the intr vector is restored
 * Remarks:	 Since the heart is involved, this can be considered a
 *               heart-bridge system interrupt test.
 * Assumption:	only one processor
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
br_intr(int argc, char *argv[])
{
	bridgereg_t     b_mask = 0;
	heartreg_t     	h_mask = 0;
	bridgereg_t     intr_vect, saved_vect;
	heartreg_t	heart_isr = 0, heart_mode;
	heartreg_t	heart_wid_err_type;	
	bridgereg_t	bridge_isr;
	bridgereg_t	lower_addr,upper_addr;
        __uint64_t	add_off = 0x100000; /* reserved area in the 
						bridge registers (Tab 42) */
	heartreg_t tmp_hr_buf[4] = {HEART_IMR(0), HEART_WID_ERR_MASK, 
					HEART_MODE, D_EOLIST};
	heartreg_t tmp_hr_save[4];
	bridgereg_t tmp_br_buf[3] = {BRIDGE_INT_HOST_ERR, BRIDGE_INT_ENABLE, \
					D_EOLIST};
	bridgereg_t tmp_br_save[3];
	bool_t		intr_to_proc;	/* if TRUE, let intr go to the uP */
					/*	cause register	*/
	uchar_t		vect_intr_level ; /* between 0 and 4 */
	__uint64_t	time_out;
	heartreg_t	heart_wid_err_mask, heart_imr0;
	char		*test_name="Bridge Interrupt Test";

	SET_BRIDGE_XBOW_PORT(argc, argv);

	msg_printf(DBG,"%s Begin\n", test_name);
	
	_hb_disable_proc_intr(); /* disable the coprocessor0 intr */

	if (argc == 2) intr_to_proc = (atoi(argv[1]) == 0) ? FALSE : TRUE;
		 else  intr_to_proc = TRUE;
	msg_printf(DBG,"%s intr go to the uP ? %s\n", test_name,
						intr_to_proc? "Yes":"No");
	
	/* Reset heart and bridge */
	if (_hb_reset(RES_HEART, RES_BRIDGE)) {
                d_errors++;
                if (d_exit_on_error) goto _error;
        }

	/* save the programmed intr vector */
	/* 	XXX remove when function below is debugged */
	BR_REG_RD_32(BRIDGE_INT_HOST_ERR, ~b_mask, saved_vect);
	/* save all H and B registers that are written to in this module */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);
	_br_save_regs(tmp_br_buf, tmp_br_save);

	/* reset the diag error cntr */
	d_errors = 0;

	/* heart and Bridge Setup: */
	/* disable the heart interrupts by setting the intr masks to 0 */
	PIO_REG_WR_64(HEART_IMR(0), ~h_mask, 0x0);
	PIO_REG_WR_64(HEART_WID_ERR_MASK, ~h_mask, ERRTYPE_IOR_INT_VEC_ERR | ERRTYPE_IOR_INT_VEC_WARN);
	PIO_REG_RD_64(HEART_WID_ERR_MASK, ~h_mask, heart_wid_err_mask);
	msg_printf(DBG,"%s H wid_err_mask = 0x%x\n",
			test_name, heart_wid_err_mask);
	/* reset the heart cause register */
	_disp_heart_cause();
	PIO_REG_WR_64(HEART_CAUSE, ~0x0, ~0x0);
	_disp_heart_cause();
	/* heart mode: print for now */
	PIO_REG_RD_64(HEART_MODE, ~0x0, heart_mode);
	msg_printf(DBG,"%s H mode = 0x%x\n", test_name,
			heart_mode);

	/* reset the bridge ISR in any case */
	BR_REG_WR_32(BRIDGE_INT_RST_STAT, ~0x0, 0x7f);
	/* enable the INVALID_ADDRESS bit in the B INT_ENABLE reg */
	BR_REG_WR_32(BRIDGE_INT_ENABLE, ~b_mask, BRIDGE_ISR_INVLD_ADDR);
	/* set a good interrupt vector (value between 3 and 49 , */
	/*  or between 51 and 58, inclusive) in the B Host Error Field Reg. */
	/* and for the bad intr vectors: verify IOR_IntVectErr */
	/*	and IOR_IntVectWarn in the H Widget error type register. */

	/* vector loop */
	/* XXX should be for ( intr_vect = 0; intr_vect < 64; intr_vect++ )  */
	/* XXX but sable does not allow that: It's used for flow control and */
	/*	is generated by heart.  It should be illegal, but we depend */
	/*	on it not generating an error for a few things.	*/
	for ( intr_vect = 1; intr_vect < 64; intr_vect++ ) { 
	    msg_printf(DBG,"\n%s intr vector = %d (%s interrupt vector)\n",
		test_name,
		intr_vect,_br_is_valid_vector(intr_vect)? "good":"bad");
	    /* load the interrupt vector */
	    BR_REG_WR_32(BRIDGE_INT_HOST_ERR, ~b_mask, intr_vect);
	    msg_printf(DBG,"%s intr vector loaded\n", test_name);
	    if (!_br_is_valid_vector(intr_vect)) {  
	 	/* check the heart_wid_err_type */
	  	PIO_REG_RD_64(HEART_WID_ERR_TYPE, ~0x0, heart_wid_err_type);
	  	msg_printf(DBG,"%s H wid_err_type = %x\n",
			test_name, heart_wid_err_type);
	    } 
	    /* adjust the interrupt mask if the intr go to the proc */
	    if (intr_to_proc && _br_is_valid_vector(intr_vect)) {
		PIO_REG_WR_64(HEART_IMR(0), ~h_mask, 0x1L<<intr_vect);
		PIO_REG_RD_64(HEART_IMR(0), ~h_mask, heart_imr0);
		msg_printf(DBG,"%s H intr mask 0x%x\n",
			test_name, heart_imr0);
		if (d_exit_on_error) goto _error;
		}
	    /* cause an intr. by accessing the unused Xtalk widget addr space*/
	    /* used same addr as kkn's in his B access test (in br_err.c) */
	    msg_printf(DBG,"%s cause intr\n", test_name);
	    BR_REG_WR_32(add_off,~0x0,~0x0); 
	    /* for level 2 tests, do a write to a reserved Device address XXX*/
	    if (_br_is_valid_vector(intr_vect)) {	/* good vector */
	        /* poll the error bit in the H Interrupt Status Register */
		time_out = 0xfff;
	        do {	PIO_REG_RD_64(HEART_ISR, ~h_mask, heart_isr) ;
			time_out --;
		}
	        while ((heart_isr == 0) && (time_out != 0));
		PIO_REG_RD_64(HEART_ISR, ~h_mask, heart_isr);
		if (time_out == 0) {
			msg_printf(ERR,"*** time-out waiting for the intr\n");
			d_errors++;
			if (d_exit_on_error) goto _error;
                }
	        else if (heart_isr & (0x1L<<intr_vect)) 
			msg_printf(DBG,"%s good intr vector bit in H ISR 0x%x\n", 
				test_name, heart_isr);
		    /* mask the TIMER intr */
		    else if (heart_isr & ~HEART_INT_TIMER) { 
			msg_printf(ERR,"*** wrong intr vector bit in H ISR 0x%x\n", heart_isr);
			d_errors++;
			if (d_exit_on_error) goto _error;
		}
	        /* poll the interrupt bit in the R4k/R10k cause register */
	        if (intr_to_proc)	{
			vect_intr_level = _br_intr_level (intr_vect); /* 0-4 */
			if(_hb_poll_proc_cause(vect_intr_level)) {
				d_errors++;
				if (d_exit_on_error) goto _error;
			}
		}
		/* reset the mask */
		if (intr_to_proc) PIO_REG_WR_64(HEART_IMR(0), ~h_mask, 0x0);
	      }
	    else { /* bad interrupt vector case */
	      /* check the heart_wid_err_type */
	      PIO_REG_RD_64(HEART_WID_ERR_TYPE, ~0x0, heart_wid_err_type);
	      msg_printf(DBG,"%s H wid_err_type = %x\n",
			test_name, heart_wid_err_type);
	      if(_hb_wait_for_widgeterr()) {
		 msg_printf(ERR, "*** error in _hb_wait_for_widgeterr ***\n");
		 d_errors++;
		 if (d_exit_on_error) goto _error;
		 }
	      /* check the IOR_IntVectErr/IOR_IntVectWarn bits in the H */
	      /*   Widget error type Register */
	      PIO_REG_RD_64(HEART_WID_ERR_TYPE, ~h_mask, heart_wid_err_type);
	      msg_printf(DBG,"%s H wid_err_type = %x\n",
			test_name, heart_wid_err_type);
	      msg_printf(DBG,"\t\t\tH wid_err_type: IntVectErr = %1x \n",
			(heart_wid_err_type & ERRTYPE_IOR_INT_VEC_ERR) >> 21);
	      msg_printf(DBG,"\t\t\tH wid_err_type: IntVectWarn = %1x \n",
			(heart_wid_err_type & ERRTYPE_IOR_INT_VEC_WARN) >> 20);
	      if (((heart_wid_err_type & ERRTYPE_IOR_INT_VEC_ERR)
		  |(heart_wid_err_type & ERRTYPE_IOR_INT_VEC_WARN)) == 0) {
		 msg_printf(ERR,"\t\tIntVectWarn AND IntVectErr not set! \n");
		 d_errors++;
		 if (d_exit_on_error) goto _error;
                 }
	    } /* end of bad intr vector case */
	    /* check the INVALID_ADDRESS bit in the B INT_STATUS reg */
	    BR_REG_RD_32(BRIDGE_INT_STATUS, ~b_mask, bridge_isr);
	    msg_printf(DBG,"%s INVALID_ADDRESS bit \n\t\t\tin the bridge_isr = 0x%x ?\n", test_name, bridge_isr);
	    if ((bridge_isr & BRIDGE_ISR_INVLD_ADDR) == 0) {
		msg_printf(ERR,"\t\tB int_status (invalid address) = %x\n",
				bridge_isr);
		d_errors++;
		if (d_exit_on_error) goto _error;
	    }
	    /* check the erred address in B Error Address Reg */
	    BR_REG_RD_32(BRIDGE_WID_ERR_LOWER, ~b_mask, lower_addr);
	    BR_REG_RD_32(BRIDGE_WID_ERR_UPPER, ~b_mask, upper_addr);
	    msg_printf(DBG,"%s B Error Address Reg: lower = %x\n",
			test_name, lower_addr);	
	    msg_printf(DBG,"%s B Error Address Reg: upper = %x\n",
			test_name, upper_addr);
	    /* compare with address used */
	    if((BRIDGE_BASE+add_off) & 0xffffffff != lower_addr) {
		msg_printf(ERR,"mismatched lower address in Error Address Reg\n");
		d_errors++;
		if (d_exit_on_error) goto _error;
	    }
	    if (_hb_reset(RES_HEART, RES_BRIDGE)) {
		d_errors++;
		if (d_exit_on_error) goto _error;
	    }  
	    /* second reset for the bad vectors only: */
	    /*  when the error condition is cleared in bridge, it sends */
	    /*  another packet to heart ("please turn off my interrupt") */
	    /*  which will still have a bad vector number. This will trigger */
	    /*  another error in Heart, which needs to be cleared out before */
	    /*  the test goes on to test the next bit. */
	    if (!_br_is_valid_vector(intr_vect)) {
		msg_printf( DBG, "only for a \"bad\" intr vector: ");
	    	if (_hb_reset(RES_HEART, RES_BRIDGE)) {
			d_errors++;
			if (d_exit_on_error) goto _error;
	    	}  
	    }
	} /* intr vector loop */

	/* tests passed without error */
	msg_printf(DBG,"%s End without error\n", test_name);
	
_error:
	/* restore  the programmed intr vector */
	/* XXX remove when function below is debugged */
	BR_REG_WR_32(BRIDGE_INT_HOST_ERR, ~b_mask, saved_vect);

	if (_hb_reset(RES_HEART, RES_BRIDGE)) d_errors++; /* reset H/B */

	/* restore all H and B registers that are written to in this module */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);
	_br_rest_regs(tmp_br_buf, tmp_br_save);

	/* XXX only one baseio bridge */
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Bridge Interrupt", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_BRIDGE_0001], d_errors );
}

/*
 * Name:	_br_is_valid_vector
 * Description:	tests whether the interrupt vector is valid or not
 *		It should have value between 3 and 49 , 
 *			or between 51 and 58, inclusive.
 * Input:	interrupt vector (i.e. bit position in the H interrupt 
 *			status register)
 * Output:	Returns 1 if it is a valid vector, 0 if it is an invalid vector
 */
bool_t
_br_is_valid_vector(__uint64_t intr_vector)
{
	if (((intr_vector >= LEVEL0_LOW) && (intr_vector <= LEVEL2_HIGH))
	   ||((intr_vector >= LEVEL4_LOW) && (intr_vector <= LEVEL4_HIGH)))
	     return(1);
	else return(0);
}

/*
 * Name:	_br_reset_br_isr
 * Description:	tests the RESET_INT_STATUS Reg, by resetting each intr group
 *			one at a time; special attention to the intr group
 *			in question. 
 * Input:	group of the interrupt vector  (just to point it out)
 * Output:	Returns 1 if there is an error, 0 if not
 */
bool_t
_br_reset_br_isr(__uint32_t intr_group)
{
	bridgereg_t 	br_isr, br_isr_old, br_isr_clr;
	__uint32_t	reset_bit;
	__uint32_t	reset_error = 0;
#if EMULATION
	char		*test_name = " ";
#else
	char		*test_name = "_br_reset_br_isr...";
#endif

	/* read and display the B INT_STATUS reg */
	BR_REG_RD_32(BRIDGE_INT_STATUS, ~0x0, br_isr);
	msg_printf(DBG,"%s B INT_STATUS reg = %x\n", test_name, br_isr);
	if (br_isr & BRIDGE_ISR_MULTI_ERR != 0) 
		msg_printf(DBG,"%s Bridge multi error (look into) \n", 
				test_name);

	/* for all 7 reset bits in RESET_INT_STATUS register */
	for (reset_bit = 0; reset_bit < 7; reset_bit ++) {
	   if ( reset_bit == intr_group )
	     msg_printf(DBG,"%s intr grp: bit (%1x)\n", test_name, reset_bit);
	   br_isr_old = br_isr;
	   BR_REG_WR_32(BRIDGE_INT_RST_STAT, ~0x0, 0x1<<reset_bit);
	   /* check the B intr Status */
	   BR_REG_RD_32(BRIDGE_INT_STATUS, ~0x0, br_isr);
	   /* raise an error if we don't clear what we should. */
	   /* XXX- raise an error if we do clear what we should not? */
	   switch (0x1<<reset_bit) {
	     case BRIDGE_IRR_PCI_GRP_CLR:
	       br_isr_clr = BRIDGE_IRR_PCI_GRP;
	       break;
	     case BRIDGE_IRR_SSRAM_GRP_CLR:
	       br_isr_clr = BRIDGE_IRR_SSRAM_GRP;
	       break;
	     case BRIDGE_IRR_LLP_GRP_CLR:
	       br_isr_clr = BRIDGE_IRR_LLP_GRP;
	       break;
	     case BRIDGE_IRR_REQ_DSP_GRP_CLR:
	       br_isr_clr = BRIDGE_IRR_REQ_DSP_GRP;
	       break;
	     case BRIDGE_IRR_RESP_BUF_GRP_CLR:
	       br_isr_clr = BRIDGE_IRR_RESP_BUF_GRP;
	       break;
	     case BRIDGE_IRR_CRP_GRP_CLR:
	       br_isr_clr = BRIDGE_IRR_CRP_GRP;
	       break;
	     case BRIDGE_IRR_MULTI_CLR: /* pointless, here */
	       br_isr_clr = ~0;
	       break;
	     default: 
	       msg_printf(ERR,"in _br_reset_br_isr: default\n");
	       br_isr_clr = 0;
	       d_errors++;
	       return (1);
	     }
	   /* If we didn't clear what we should have, error. */
	   /* ... and tell us what went wrong! */
	   if (br_isr & br_isr_clr) {
	     msg_printf(ERR,"writing %x to INT_RST_STAT did not clear bit(s) %x (but should have)\n",
			0x1<<reset_bit, br_isr & br_isr_clr);
	     reset_error ++;
	   }
	   /* If we cleared any other bits, error. */
	   if ((br_isr_old & ~br_isr) & ~br_isr_clr) {
	     msg_printf(ERR,"writing %x to INT_RST_STAT cleared bit(s) %x (but shound not have)\n",
			0x1<<reset_bit, (br_isr_old & ~br_isr) & ~br_isr_clr);
	     reset_error ++;
	   }
	   /* If we set any bits at all, error. */
	   if (br_isr & ~br_isr_old) {
	     msg_printf(ERR,"writing %x to INT_RST_STAT set bit(s) %x (but shound not have)\n",
			0x1<<reset_bit, br_isr & ~br_isr_old);
	     reset_error ++;
	   }
	}
	msg_printf(DBG,"%s completed br_reset_br_intr\n", test_name);

	if (reset_error != 0) {
	   msg_printf(ERR,"Resetting the B interrupts\n");
	   d_errors++;
	   return (1);
	   }
	return(0);
}
/*
 * Name:        _br_intr_level
 * Description: returns the interrupt level, from an interrupt vector
 *			(this comes actually from the Heart specs)
 * Input:       interrupt number
 * Output:      interrupt level (must be between 0 and 4)
 */
uchar_t
_br_intr_level(__uint64_t	intr_vector)
{
	uchar_t	intr_level;
	if((intr_vector >= LEVEL0_LOW) && (intr_vector <= LEVEL0_HIGH))
		intr_level = LEVEL0;
	else if((intr_vector >= LEVEL1_LOW) && (intr_vector <= LEVEL1_HIGH))
		intr_level = LEVEL1;
	else if((intr_vector >= LEVEL2_LOW) && (intr_vector <= LEVEL2_HIGH))
		intr_level = LEVEL2;
	else if((intr_vector >= LEVEL3_LOW) && (intr_vector <= LEVEL3_HIGH))
		intr_level = LEVEL3;
	else if((intr_vector >= LEVEL4_LOW) && (intr_vector <= LEVEL4_HIGH))
		intr_level = LEVEL4;
	else { msg_printf(ERR," _br_intr_level: intr_level not found \n");
	       return (0);
	     }
	return (intr_level);
}

