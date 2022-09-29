/*
 * hb_util.c   
 *
 *	utilities needed for hb_ functions
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

#ident "ide/godzilla/heart_bridge/hb_util.c:  $Revision: 1.21 $"

/*
 * hb_util.c -  utilities needed for hb_ functions
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
 * Name:	_hb_wait_for_widgeterr
 * Description:	
 * Input:	none
 * Output:	Returns 1 if error, else, 0
 * Error Handling: no interrupt, using polling
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_hb_wait_for_widgeterr(void)
{
	heartreg_t	heart_cause;
	heartreg_t	heart_isr;
	bool_t		intr_to_proc;	/* if TRUE, let intr go to the uP */
					/*      cause register  */
	__uint64_t	time_out = 0xff;

	intr_to_proc = FALSE;   /* XXX debug with TRUE later */
	/* poll the coprocessor 0 cause register for level 4 */
	/*	(heart exception is at that level)		*/
	if (intr_to_proc) {
		if(_hb_poll_proc_cause(LEVEL4)) return(1);
	}

	/* poll the H exception bit in the H Interrupt Status Register */
	do  {	PIO_REG_RD_64(HEART_ISR, HEART_ISR_HEART_EXC, heart_isr) ;
		time_out--;
	}
	while (( heart_isr == 0 ) && (time_out != 0));
	if (time_out == 0) {
		msg_printf(DBG,"*** time out in _hb_wait_for_widgeterr\n");
		return(1);
	}
	PIO_REG_RD_64(HEART_ISR, ~0x0, heart_isr);
	/* check the widgeterr bit in the H cause register */
	PIO_REG_RD_64(HEART_CAUSE,HC_WIDGET_ERR,heart_cause);
	if (heart_cause != HC_WIDGET_ERR) { /* i.e. 0 */
		msg_printf(ERR,"*** widget_err not set in H cause ***\n");
		PIO_REG_RD_64(HEART_CAUSE, ~0x0, heart_cause);
		msg_printf(DBG,"_hb_wait_for_widgeterr...  heart_cause = %x\n",heart_cause);
		return(1);
	}
	return(0);
}
/* function name:       _disp_heart_cause
 * input:               
 * output:              display of bits set
 * Operation:           prints a list of the bits that are set in the heart
 *				cause register
 */
void
_disp_heart_cause(void)
{
	extern struct reg_desc heart_cause_desc[];
	heartreg_t      heart_cause;

	PIO_REG_RD_64(HEART_CAUSE, ~0x0, heart_cause);
	if (heart_cause) 
		msg_printf(DBG,"%R\n",heart_cause,heart_cause_desc);
}


/* function name:	_hb_disable_proc_intr
 * input:		none
 * output:		disables all interrupts in the coprocessor 0
 *				status register. Does not restore/save
 *				the previously-enabled interrupts.
 */
void
_hb_disable_proc_intr(void)
{
	k_machreg_t old_SR;
	old_SR = get_SR(); 
        set_SR (old_SR & ~SR_IMASK);	/* defined in irix/kern/sys/sbd.h */
}

/* function name:	_hb_poll_proc_cause
 * input:		interrupt level (0-4)
 * output:		none
 */
bool_t
_hb_poll_proc_cause(uchar_t     interrupt_level)
{
	k_machreg_t     current_cause;  /* processor cause register */
	__uint64_t	time_out;
	heartreg_t	heart_imsr;
#if EMULATION
	char	*test_name = " ";
#else
	char	*test_name = "_hb_poll_proc_cause...";
#endif /* EMULATION */

	msg_printf(DBG,"%s Begin \n", test_name);

	msg_printf(DBG,"%s interrupt level %d\n", test_name, interrupt_level);
	current_cause = 0;
	time_out = 0xfff ;
	while (((current_cause & ( CAUSE_LEVEL0_BIT<<interrupt_level ))== 0) 
			 			&& (time_out != 0) ) {
		current_cause = get_cause(); /* get_cause in genasm.s */
		time_out--;
	}
	msg_printf(DBG,"%s proc cause = 0x%x\n", test_name, current_cause);
	if (time_out == 0) {
		msg_printf(DBG,"%s timed-out !!!! \n", test_name);
		return(1);
	}
	else	msg_printf(DBG,"%s intr bit set in proc cause register\n",
			test_name);

	/* print the interrupt masked status register (intr controller) */
	PIO_REG_RD_64(HEART_IMSR, ~0x0, heart_imsr);
	msg_printf(DBG,"%s interrupt masked status register = 0x%x\n", 
			test_name, heart_imsr);

	msg_printf(DBG,"%s End \n", test_name);

        return (0); 
	
}
/*
 * Name:	hb_reset
 * Description: call to _hb_reset as ide command
 */
bool_t
hb_reset(__uint32_t argc, char **argv)
{
	bool_t	reset_heart = FALSE;
	bool_t	reset_bridge = FALSE;

	/* "-h" arg for heart, "-b" for bridge */
	/*  default is "reset both " */
	if (argc == 1) {
		reset_heart = TRUE;
		reset_bridge = TRUE;
	}
	else  {
		argc--; argv++;
		if (argc && argv[0][0]=='-' && argv[0][1]=='h')
			reset_heart = TRUE;
		else if (argc && argv[0][0]=='-' && argv[0][1]=='b')
		reset_bridge = TRUE;
		if (argc != 0) {
			argc--; argv++;
			if (argc && argv[0][0]=='-' && argv[0][1]=='b')
				reset_bridge = TRUE;
			else if (argc && argv[0][0]=='-' && argv[0][1]=='h')
				reset_heart = TRUE;
		}
	}

	if (_hb_reset(reset_heart, reset_bridge))	/* computes d_errors */
		msg_printf(DBG,"_hb_reset Failed\n");

	if (reset_heart && !reset_bridge)
	    REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_BRIDGE_0005], d_errors );
#ifdef NOTNOW
	    REPORT_PASS_OR_FAIL(d_errors, "Heart Reset", D_FRU_IP30);
#endif
	else if (!reset_heart && reset_bridge)
	    REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HEART_0006], d_errors );
#ifdef NOTNOW
	    REPORT_PASS_OR_FAIL(d_errors, "Bridge Reset", D_FRU_IP30);
#endif
	else
	    REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HAB_0003], d_errors );
#ifdef NOTNOW
	    REPORT_PASS_OR_FAIL(d_errors, "Heart & Bridge Reset", D_FRU_IP30);
#endif
}

/* function name:	_hb_reset
 * input:		whether to reset the heart or the bridge or both
 * output:		resets the major registers in heart and bridge
 */
bool_t
_hb_reset(bool_t res_heart, bool_t res_bridge)
{
	heartreg_t	heart_isr, heart_cause;
	bridgereg_t	bridge_int_status, bridge_wid_err_cmdword;
	bool_t		reset_heart_failed = FALSE;

    if (res_heart && !res_bridge)
	msg_printf(DBG,"Calling _hb_reset (heart only)...\n");
    else if (!res_heart && res_bridge)
	msg_printf(DBG,"Calling _hb_reset (bridge only)...\n");
    else msg_printf(DBG,"Calling _hb_reset...\n");

    d_errors = 0;

    /* Heart */
    if (res_heart) {
	/* reset the Heart error type register (RW1C) */
	PIO_REG_WR_64(HEART_WID_ERR_TYPE, ~0x0, ~0x0);
	/* THEN reset the Heart command error word */
	/*   (any value clears the register) */
	PIO_REG_WR_64(HEART_WID_ERR_CMDWORD, ~0x0, 0x0);
	/* reset the cause register */
	PIO_REG_WR_64(HEART_CAUSE, ~0x0, ~0x0);
	PIO_REG_RD_64(HEART_CAUSE, ~0x0, heart_cause);
	if (heart_cause) {
		msg_printf(ERR, "Heart cause was not reset (0x%x)\n",
			heart_cause);
		reset_heart_failed = TRUE;
		d_errors ++;
		goto _res_bridge;
	}
	/* reset the ISR */
	PIO_REG_WR_64(HEART_CLR_ISR, ~0x0, ~0x0);
	PIO_REG_RD_64(HEART_ISR, ~0x0, heart_isr);
	if (heart_isr) {
		msg_printf(ERR, "Heart ISR was not reset (0x%x)\n",
			heart_isr);
		reset_heart_failed = TRUE;
		d_errors ++;
		goto _res_bridge;
	}
    } /* res_heart */

_res_bridge:
	/* bridge */
    if (res_bridge) {
	/* scm: THebridge error command word registers should retain 
		the last value, when the ISR is reset, the error register
		is enabled so should another error occur then the new 
		value is loaded. */
	/* reset the ISR */
	BR_REG_WR_32(BRIDGE_INT_RST_STAT, ~0x0, 0x7f);
	BR_REG_RD_32(BRIDGE_INT_STATUS, ~0x0, bridge_int_status);
	if (bridge_int_status & ~SUPER_IO_INTR) {
		msg_printf(ERR, "Bridge ISR was not reset (0x%x)\n",
			bridge_int_status);
		d_errors ++;
		return(1);
	}
    } /* res_bridge */

    if (reset_heart_failed) return(1);
    return(0);
}

	
