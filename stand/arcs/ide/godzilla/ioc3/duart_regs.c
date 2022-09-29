/*   * duart_regs.c : 
 *	duart Register read/write Tests
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S GOVERNMENT RESTRICTED RIGHTS LEGEND:
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

#ident "ide/godzilla/duart/duart_regs.c:  $Revision: 1.7 $"

/*
 * duart_regs.c - duart Register read/write Tests
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_duart.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"

/*
 * Forward References: all in d_prototypes.h 
 */

/*::
 * Global Variables
 */
bool_t		exit_on_duart_error = 0;
__uint32_t	duart_errors = 0;

duart_Registers	gd_duart_regs[] = {

/*{"DU_SIO_PP_DATA",		DU_SIO_PP_DATA,		GZ_READ_WRITE,
DU_SIO_PP_DATA_MASK,		DU_SIO_PP_DATA_DEFAULT},*/

{"DU_SIO_PP_DCR",		DU_SIO_PP_DCR,		GZ_READ_WRITE,
  DU_SIO_PP_DCR_MASK,        	DU_SIO_PP_DCR_DEFAULT},

/*points to buffer whose pointer in incremented after write
{"DU_SIO_PP_FIFA",		DU_SIO_PP_FIFA,		GZ_READ_WRITE,
DU_SIO_PP_FIFA_MASK,		DU_SIO_PP_FIFA_DEFAULT},
*/

{"DU_SIO_UB_SCRPAD",		DU_SIO_UB_SCRPAD,		GZ_READ_WRITE,
DU_SIO_UB_SCRPAD_MASK,		DU_SIO_UB_SCRPAD_DEFAULT},

{"DU_SIO_UA_SCRPAD",		DU_SIO_UA_SCRPAD,		GZ_READ_WRITE,
DU_SIO_UA_SCRPAD_MASK,		DU_SIO_UA_SCRPAD_DEFAULT},

{"", -1, -1, -1, -1}

};

/*
 * Name:	duart_regs.c
 * Description:	tests registers in the duart
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
duart_regs(__uint32_t argc, char **argv)
{
	duart_Registers	*regs_ptr = gd_duart_regs;
	bool_t		check_defaults;

	msg_printf(INFO,"DUART_regs test\n");

	duart_errors = 0; /* init : it is incremented here, 
				not in the called subroutines */

	while (regs_ptr->name[0] != NULL) {

	   switch (regs_ptr->mode) {
	      case  GZ_READ_ONLY:
	        if (_duart_regs_check_default(regs_ptr)) {
	        	msg_printf( DBG,"\n");
			duart_errors++;
	  		if (exit_on_duart_error) goto _error;
	  	        msg_printf( DBG,"\n");
		}
		break;
	      case  GZ_READ_WRITE:
		if (_duart_regs_read_write(regs_ptr)) {
		    duart_errors++;
	   	    if (exit_on_duart_error) goto _error;
		}
		break;
	      case  GZ_WRITE_ONLY:
		break;
	      case  GZ_NO_ACCESS:
		break;
	   }
	   regs_ptr++;
	}
	/*
	if (_special_duart_cases()) { 
		duart_errors++;
	}
	*/

_error:
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(duart_errors, "DUART Read-Write Register", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_IOC3_0000], duart_errors );
}

/*
 * Name:	_duart_regs_check_default.c
 * Description:	Tests the default values of duart Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: NOT DEBUGGED OR USED, 
 */
bool_t
_duart_regs_check_default(duart_Registers *regs_ptr)
{
	duartregisters_t	reg_val;

	PIO_REG_RD_8(regs_ptr->address, regs_ptr->mask, reg_val);

	if (reg_val != (regs_ptr->def & regs_ptr->mask)) {
	   msg_printf(INFO, "ERROR Default:    %s; Addr = 0x%x; Mask = 0x%x; \
		\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
		regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
		(regs_ptr->def & regs_ptr->mask), (reg_val & regs_ptr->mask));
	   if (exit_on_duart_error) {
		return(1);
	   }
	}
        else {
	   msg_printf(DBG, "Configuration Register Test Passed\n");
	   msg_printf(DBG, "\t%s (0x%x) mask= 0x%x; default OK= 0x%x; ",
			regs_ptr->name, regs_ptr->address, regs_ptr->mask,
			(regs_ptr->def & regs_ptr->mask));
	}
	
	return(0);
}

/*
 * Name:	_duart_regs_read_write.c
 * Description:	Performs Write/Reads on the duart Registers
 * Input:	Pointer to the register structure 
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_duart_regs_read_write(duart_Registers *regs_ptr)
{
	duartregisters_t	saved_reg_val, write_val, read_val;
	duartregisters_t	pattern[DUART_REGS_PATTERN_MAX] = {
					0x5a, 0xa5,
					0xcc, 0x33,
					0xf0, 0x0f};
	char 		pattern_index;
					
    /* Save the current value */
    PIO_REG_RD_8(regs_ptr->address, (duartregisters_t)~0x0, saved_reg_val);

    for (pattern_index=0; pattern_index<HR_REGS_PATTERN_MAX; pattern_index++) {
	write_val = pattern[pattern_index];

	/* Write the test value */
	PIO_REG_WR_8(regs_ptr->address, regs_ptr->mask, write_val);

	/* Read back the register */
	PIO_REG_RD_8(regs_ptr->address, regs_ptr->mask, read_val);

	/* Compare the expected and received values */
	if ((write_val & regs_ptr->mask) != read_val) {
	   msg_printf(INFO, "\nERROR Write/Read: %s; Addr = 0x%x; Mask = 0x%x; \
		\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
		regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
		(write_val&regs_ptr->mask), read_val);
	   /* Restore the register with old value */
	   PIO_REG_WR_8(regs_ptr->address, (duartregisters_t)~0x0, saved_reg_val);
	   return(1);	/* exit on first error */
	}
	else if (pattern_index == HR_REGS_PATTERN_MAX -1) {
		msg_printf(DBG, "Write/Read Test OK.\n");
	}
    } /* pattern_index loop */

    /* Restore the register with old value */
    PIO_REG_WR_8(regs_ptr->address, (duartregisters_t)~0x0, saved_reg_val);

    return(0);
}

/*
 * Name:	_special_duart_cases
 * Description:	handles special information about some registers
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: none
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_special_duart_cases()
{
	return (0);
 
}
