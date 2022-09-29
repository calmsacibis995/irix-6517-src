/*
 * br_ram.c - bridge internal/external RAM tests 
 * 		Three RAMs are tested:
 *		- Internal Address Translation Entry RAM (internal)
 *		- Read Response Buffer RAM (internal)
 *		- External Sync SSRAM (external: not in IP30)
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

#ident "ide/godzilla/bridge/br_ram.c: $Revision 1.1$"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"
#include "d_mem.h"

/*
 * Forward References : in d_prototypes.h
 */

/* Bridge_Ram structure : 
 * {name of the ram to be tested, start address, RAM size in bytes, test type} 
 */
Bridge_Ram	gz_bridge_ram[] = {
{"INTERNAL_ATE RAM", BRIDGE_ATE_RAM, BRIDGE_ATE_RAM_SIZE, RAM_ADDR_UNIQ},
{"INTERNAL_ATE RAM", BRIDGE_ATE_RAM, BRIDGE_ATE_RAM_SIZE, RAM_WALKING},
/* NOTE: there is no External SSRAM on IP30  */
{"EXTERNAL_SYNC SSRAM", BRIDGE_EXT_SSRAM, BRIDGE_EXT_SSRAM_SIZE, RAM_ADDR_UNIQ},
{"", 0, 0, 0}		
};


/*
 * Name:	_br_get_SSRAM_size
 * Description:	Determines the SSRAM size (in bytes) from the BRIDGE cntrl reg
 * Input:	the saved value of the BRIDGE cntrl reg (the PROM has 
 *		 set this before ide runs)
 * Output:	Returns -1 if an error occured, else the size in bytes
 * Error Handling:
 * Side Effects: none
 * Remarks:	 assumption: the size has already been determined and stored.
 * Debug Status: compiles, simulated, emulated, 
 */
__uint32_t
_br_get_SSRAM_size(__uint64_t bridge_control_reg)
{
	__uint32_t ssram_size;
	switch(BRIDGE_CTRL_SSRAM_SIZE_MASK & bridge_control_reg) {
   	    case BRIDGE_CTRL_SSRAM_512K: ssram_size = BRIDGE_SSRAM_512K; break;
   	    case BRIDGE_CTRL_SSRAM_128K: ssram_size = BRIDGE_SSRAM_128K; break;
   	    case BRIDGE_CTRL_SSRAM_64K: ssram_size = BRIDGE_SSRAM_64K; break;
   	    case BRIDGE_CTRL_SSRAM_1K: ssram_size = BRIDGE_SSRAM_0K; break;
	    default: ssram_size = -1;
		     msg_printf(ERR,"*** in _br_get_SSRAM_size ***\n"); break;
	    }
	return (ssram_size);
}
/*
 * Name:	br_ram
 * Description:	Tests Bridge RAM space
 * Input:	None
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling:
 * Side Effects: modifies the contents of the ATE ram, the RRB ram, 
 *		 and the external SSRAM
 * Remarks:	The test of the SSRAM is included (size 0 for IP30)
 *		The RAM addressing is peculiar: Write Double Words
 *						and Read Single Words.
 *			AND if a word is written at 0x10000, half
 *			is read at 0x10004 (63:32), half at 0x11004 (31:0)
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
br_ram(__uint32_t argc, char **argv)
{
	Bridge_Ram	*ram_ptr = gz_bridge_ram; /* struct in d_bridge.h */
	bridgereg_t	saved_bridge_control_reg; /* used to get ram size */
	bridgereg_t 	value, b_mask = 0;
	heartreg_t	pattern;	/* 64 bits */
	heartreg_t	address;

	SET_BRIDGE_XBOW_PORT(argc, argv);

	msg_printf(DBG,"Bridge Internal/External RAM test... Begin\n");

	/* reset the heart and bridge prior to starting */
	if (_hb_reset(RES_HEART, RES_BRIDGE)) {
		d_errors++;
		goto _error;
	}

	/* Heart and Bridge  check out */
	if (_hb_chkout(CHK_HEART, CHK_BRIDGE)) {
		msg_printf(ERR, "Heart/Bridge Check Out Error\n");
		goto _error;
	}

	/* read the bridge control register separately (needed further) */
	BR_REG_RD_32(BRIDGE_WID_CONTROL, ~b_mask, saved_bridge_control_reg);
	
	/* go through each test in the test list */
	while (ram_ptr->ram_name[0] != NULL) {
	 /* determine the SSRAM size */
	 if(strcmp(ram_ptr->ram_name,"EXTERNAL_SYNC SSRAM") == 0) {
	 	ram_ptr->ram_size = _br_get_SSRAM_size(saved_bridge_control_reg);
		
	 	if (ram_ptr->ram_size == -1) {
			d_errors++;
			goto _error;
		}
	 }
	 msg_printf(VRB,"Bridge RAM test... Testing %s as a Bridge RAM\n",ram_ptr->ram_name);
	 /* test only the non-zero RAM spaces */
	 if (ram_ptr->ram_size != 0) {
             msg_printf(DBG,"\t\ttest_type: %s\n",(ram_ptr->test_type)
				== RAM_ADDR_UNIQ ? "address uniqueness":"walking bit");
             msg_printf(DBG,"\t\tstart_address: %x\n",ram_ptr->start_address);
             msg_printf(DBG,"\t\tstop_address: %x\n",ram_ptr->start_address
			+ ram_ptr->ram_size -1);
	     switch(ram_ptr->test_type) {
		case RAM_ADDR_UNIQ:
		   /* write the pattern (64 bits) */
		   for (address = ram_ptr->start_address ;
			address < ram_ptr->start_address + ram_ptr->ram_size;
			address += 8) {
	   		BR_ATE_DW_WR_64(address, ~0x0, address);
		   }
		   /* read the pattern */
		   for (address = ram_ptr->start_address ;
			address < ram_ptr->start_address + ram_ptr->ram_size;
			address += 8) {
			/* read the Least significant word at modified addr */
	   		BR_REG_RD_32(LSWORD_ADDR(address), ~b_mask, value);
	   		if (value != (bridgereg_t)address) {
				msg_printf(INFO,"%s LS ERROR: addr=0x%x/0x%x, value=0x%x\n", 
					ram_ptr->ram_name,
					address, LSWORD_ADDR(address),
					value);
				d_errors ++;
				goto _error;
			}
			/* read the Most significant word at modified addr */
	   		BR_REG_RD_32(MSWORD_ADDR(address), ~b_mask, value);
	   		if (value != (bridgereg_t)(address>>32)) {
				msg_printf(INFO,"%s MS ERROR: addr=0x%x/0x%x,\n\t value=0x%x\n", 
					ram_ptr->ram_name,
					address, MSWORD_ADDR(address),
					value);
				d_errors ++;
				goto _error;
			}
		   }
		   break;

		case RAM_WALKING:
		   /* write the pattern (64 bits) */
		   pattern = 1;
		   for (address = ram_ptr->start_address;
			address < ram_ptr->start_address + ram_ptr->ram_size; 
			address+=8) {
	   		BR_ATE_DW_WR_64(address, ~0x0, pattern);
			pattern <<= 1;
			if (pattern == 0) pattern = 1;
		   }
		   /* read the pattern */
		   pattern = 1;
		   for (address = ram_ptr->start_address;
			address < ram_ptr->start_address + ram_ptr->ram_size; 
			address+=8) {
			/* read the Least significant word at modified addr */
	   		BR_REG_RD_32(LSWORD_ADDR(address), ~b_mask, value);
	   		if (value != (bridgereg_t)pattern) {
				msg_printf(INFO,"%s LS ERROR: addr=0x%x/0x%x, pat=0x%x/0x%x, value=0x%x\n", 
					ram_ptr->ram_name,
					address, LSWORD_ADDR(address),
					(bridgereg_t)pattern, pattern,
					value);
				d_errors ++;
				goto _error;
			}
			/* read the Most significant word at modified addr */
	   		BR_REG_RD_32(MSWORD_ADDR(address), ~b_mask, value);
	   		if ((bridgereg_t)(MASK_32&(heartreg_t)value) 
			 != (bridgereg_t)(pattern>>32)) {
				msg_printf(INFO,"%s MS ERROR: addr=0x%x/0x%x, pat=0x%x/0x%x, value=0x%x\n", 
					ram_ptr->ram_name,
					address, MSWORD_ADDR(address),
					(bridgereg_t)(pattern>>32), pattern,
					value);
				d_errors ++;
				goto _error;
			}
			pattern <<= 1;
			if (pattern == 0) pattern = 1;
		   }
		   break;

		default:
		   msg_printf(ERR,"*** test type not implemented! \n");
		   break;
	     } /* switch */
	 } /* end of ram_ptr->ram_size != 0 */
	 else msg_printf(INFO,"\t\t\t*** RAM is size 0: not tested *** \n");
	 ram_ptr++; /* point to the next ram test */
	}
       	msg_printf(DBG,"Bridge Internal/External RAM test... End\n");

_error:
	/* XXX only on base bridge */
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Bridge Internal/External RAM", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_BRIDGE_0002], d_errors );
}
