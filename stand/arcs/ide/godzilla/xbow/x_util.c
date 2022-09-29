/*
 * x_util.c : 
 *	utilities for debugging Xbow diags
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

/* NOTE: the xbow widget registers start at 0x0, and
 *   	 the link registers (l=8-f) start at 0x00_0000_01L0 where L=(l-8)*0x40 :
 * 	 these addresses are input as the -w argument.
 */

#ident "ide/godzilla/xbow/x_util.c:  $Revision: 1.2 $"

/*
 * x_util.c - utilities for debugging Xbow diags
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_prototypes.h"

/*
 * Forward References 
 */
/*
bool_t	x_read_reg(__uint32_t argc, char **argv);
bool_t	x_write_reg(__uint32_t argc, char **argv);
*/

/*
 * Name:	x_read_reg.c
 * Description:	reads a 32 bit register from Xbow
 * Input:	following switches are currently used: (with or without space)
 *		-w <widget register offset> 
 *		-m <mask> (no mask by default)
 * Output:	displays the value of the register
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, not emulated, not debugged
 */
bool_t
x_read_reg(__uint32_t argc, char **argv)
{
	char		bad_arg = 0, wflag = 0;
	xbowreg_t	wid_reg_off = XBOW_WID_STAT;
	xbowreg_t	reg_val;
	long long	wid_reg_off_long;
	long long	mask = ~0x0;

	d_errors = 0;

   	/* get the args */
   	argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 'w':
			wflag = 1;
			if (argv[0][2]=='\0') {
				atob_L(&argv[1][0], (long long *)&wid_reg_off_long);
				argc--; argv++;
			} else {
				atob_L(&argv[0][2], (long long *)&wid_reg_off_long);
			}
			/* if the offset is manually provided, one needs to */
			/*  offset the address by 0x4 to access the least */
			/*  significant word in the double word */
			wid_reg_off = (xbowreg_t)wid_reg_off_long + 0x4;
			break;
		case 'm':
			if (argv[0][2]=='\0') {
				atob_L(&argv[1][0], (long long *)&mask);
				argc--; argv++;
			} else {
				atob_L(&argv[0][2], (long long *)&mask);
			}
			break;
		default: 
			bad_arg++; break;
	    }
	    argc--; argv++;
   	}

   	if ( bad_arg || argc ) {
	    msg_printf(ERR, "Usage: -w <widget register offset> -m <mask> \n");
	    msg_printf(INFO, "defaults: register=XBOW STATUS, mask=~0 \n");
	    return(1);
   	} 
	else {
	    if (wflag) {
		XB_REG_RD_32(wid_reg_off, mask, reg_val);
	    	msg_printf(SUM, "Xbow Register 0x%016x has 0x%08x masked value\n",
				wid_reg_off+XBOW_BASE-0x4, reg_val);
	    }
	    else {
		XB_REG_RD_32(XBOW_WID_STAT, mask, reg_val);
	        msg_printf(SUM, "Xbow Widget STATUS Register has 0x%08x masked value\n",
				reg_val);
	    }
	    return(0);
	}
}

/*
 * Name:	x_write_reg.c
 * Description:	writes a 32 bit value into a register from Xbow
 * Input:	following switches are currently used: (with or without space)
 *		-w <widget register offset> (MUST be provided)
 *		-m <mask>
 *		-d <data>
 * Output:	none.
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, not emulated, not debugged
 */
bool_t
x_write_reg(__uint32_t argc, char **argv)
{
	char		bad_arg = 0;
	char		reg_value_provided = 0, reg_offset_provided = 0;
	char		wflag = 0;
	long long	reg_val = 0;
	xbowreg_t	xbow_reg_val = 0;
	xbowreg_t	wid_reg_off;
	long long	mask = ~0x0;
	long long	wid_reg_off_long;

	d_errors = 0;

   	/* get the args */
   	argc--; argv++;
   	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 'w':
			wflag = 1;
			if (argv[0][2]=='\0') {
				atob_L(&argv[1][0], (long long *)&wid_reg_off_long);
				argc--; argv++;
			} else {
				atob_L(&argv[0][2], (long long *)&wid_reg_off_long);
			}
			/* if the offset is manually provided, one needs to */
			/*  offset the address by 0x4 to access the least */
			/*  significant word in the double word */
			wid_reg_off = (xbowreg_t) wid_reg_off_long + 0x4;
			reg_offset_provided = 1;
			break;
		case 'm':
			if (argv[0][2]=='\0') {
				atob_L(&argv[1][0], (long long *)&mask);
				argc--; argv++;
			} else {
				atob_L(&argv[0][2], (long long *)&mask);
			}
			break;
		case 'd':
			if (argv[0][2]=='\0') {
				atob_L(&argv[1][0], (long long *)&reg_val);
				argc--; argv++;
			} else {
				atob_L(&argv[0][2], (long long *)&reg_val);
			}
			xbow_reg_val = (xbowreg_t) reg_val;
			reg_value_provided = 1;
			break;
		default: bad_arg++; break;
	    }
	    argc--; argv++;
   	}

   	if ( bad_arg || argc || !reg_offset_provided || !reg_value_provided ) {
	    msg_printf(ERR,"Usage: -r <register offset> -m <mask> -d <data>\n");
	    msg_printf(INFO,"Usage: the register offset and value must be provided\n");
	    return(1);
   	}
	else {
	    if (wflag) {
		XB_REG_WR_32(wid_reg_off, mask, xbow_reg_val);
	    	msg_printf(SUM, "Xbow Register 0x%016x: 0x%08x value written\n",
				wid_reg_off+XBOW_BASE-0x4, reg_val & mask);
	    }
	    else 
		msg_printf(INFO,"you MUST provide a register offset\n");
	    return(0);
	}
}

