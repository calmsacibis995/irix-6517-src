/*   * rtc_regs.c : 
 *	rtc Register read/write Tests
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

#ident "ide/godzilla/rtc/rtc_regs.c:  $Revision: 1.4 $"

/*
 * rtc_regs.c - rtc Register read/write Tests
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/ds1687clk.h"
#include "sys/RACER/IP30nvram.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_rtc.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"

/*
 * Forward References: all in d_prototypes.h 
 */

/*::
 * Global Variables
 */
bool_t		exit_on_rtc_error = 0;
__uint32_t	rtc_errors = 0;

rtc_Registers	gd_rtc_regs[] = {
/* scratch *must* be first for safe test -- this byte not in nvram checksum */
{"RTC_SCRATCH", NVRAM_REG_OFFSET, GZ_READ_WRITE,	0xFF,   0x0},
{"RTC_SEC", RTC_SEC, GZ_READ_WRITE,	0x7F,	0x0},
{"RTC_MIN", RTC_MIN, GZ_READ_WRITE,	0xFF,	0x0},
{"RTC_DATE", RTC_DATE, GZ_READ_WRITE,	0xFF,	0x0},
{"RTC_MONTH", RTC_MONTH, GZ_READ_WRITE,	0xFF,	0x0},
{"RTC_YEAR", RTC_YEAR, GZ_READ_WRITE,	0xFF,	0x0},
{0, -1, -1, -1, -1}

};

/*
 * Name:	rtc_regs.c
 * Description:	tests registers in the rtc
 * Input:	-w option will try destructive tests
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
rtc_regs(int argc, char **argv)
{
	rtc_Registers	*regs_ptr = gd_rtc_regs;
	bool_t		check_defaults;
	int writetest = 0;

	msg_printf(INFO,"RTC_regs test\n");

	rtc_errors = 0; /* init : it is incremented here, 
				not in the called subroutines */

	if (argc > 1 && (strcmp(argv[1],"-w") == 0))
		writetest = 1;

	/* Safe test for field diagnostics.  It is now the default.
	 */
	if (writetest == 0) {
		_rtc_regs_read_write(&gd_rtc_regs[0]);
		goto _error;
	}

	while (regs_ptr->name != NULL) {

	   switch (regs_ptr->mode) {
	      case  GZ_READ_ONLY:
	        if (_rtc_regs_check_default(regs_ptr)) {
	        	msg_printf( DBG,"\n");
			rtc_errors++;
	  		if (exit_on_rtc_error) goto _error;
	  	        msg_printf( DBG,"\n");
		}
		break;
	      case  GZ_READ_WRITE:
		if (_rtc_regs_read_write(regs_ptr)) {
		    rtc_errors++;
	   	    if (exit_on_rtc_error) goto _error;
		}
		break;
	      case  GZ_WRITE_ONLY:
		break;
	      case  GZ_NO_ACCESS:
		break;
	   }
	   regs_ptr++;
	}

_error:
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(rtc_errors, "RTC Read-Write Register", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_RTC_0000], rtc_errors );
}

/*
 * Name:	_rtc_regs_check_default.c
 * Description:	Tests the default values of rtc Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: NOT DEBUGGED OR USED, 
 */
bool_t
_rtc_regs_check_default(rtc_Registers *regs_ptr)
{
	rtcregisters_t	reg_val;

	PIO_REG_RD_8(regs_ptr->address, regs_ptr->mask, reg_val);

	if (reg_val != (regs_ptr->def & regs_ptr->mask)) {
	   msg_printf(INFO, "ERROR Default:    %s; Addr = 0x%x; Mask = 0x%x; \
		\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
		regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
		(regs_ptr->def & regs_ptr->mask), (reg_val & regs_ptr->mask));
	   if (exit_on_rtc_error) {
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
 * Name:	_rtc_regs_read_write.c
 * Description:	Performs Write/Reads on the rtc Registers
 * Input:	Pointer to the register structure 
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_rtc_regs_read_write(rtc_Registers *regs_ptr)
{
	rtcregisters_t	saved_reg_val, write_val, read_val;
	rtcregisters_t	pattern[RTC_REGS_PATTERN_MAX] = {
					0x5a, 0xa5,
					0xcc, 0x33,
					0xf0, 0x0f};
	char 		pattern_index;
					
    /* Save the current value */
    saved_reg_val = rtcRead(regs_ptr->address);

    for (pattern_index=0; pattern_index<HR_REGS_PATTERN_MAX; pattern_index++) {
	write_val = pattern[pattern_index];

	/* Write the test value */
	rtcWrite(regs_ptr->address, write_val);

	/* Read back the register */
	read_val = rtcRead(regs_ptr->address);

	/* Compare the expected and received values */
	if ((write_val & regs_ptr->mask) != read_val) {
	   msg_printf(INFO, "\nERROR Write/Read: %s; Addr = 0x%x; Mask = 0x%x; \
		\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
		regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
		(write_val&regs_ptr->mask), read_val);
	   /* Restore the register with old value */
	   rtcWrite(regs_ptr->address, saved_reg_val);
	   return(1);	/* exit on first error */
	}
	else if (pattern_index == HR_REGS_PATTERN_MAX -1) {
		msg_printf(DBG, "Write/Read Test OK.\n");
	}
    } /* pattern_index loop */

    /* Restore the register with old value */
    rtcWrite(regs_ptr->address, saved_reg_val);

    return(0);
}
