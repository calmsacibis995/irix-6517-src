/*
 * br_regs.c.c
 *	
 *	bridge internal register tests
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

#ident "ide/bridge/br_regs.c: $Revision: 1.24 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/xtalk/xwidget.h"  /* for widget identification fields */
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"

/*
 * Forward References: all in d_prototypes.h 
 */

int bridge_xbow_port = XBOW_PORT_F;

Bridge_Regs	gz_bridge_regs[] = {
{"BRIDGE_WID_ID",		BRIDGE_WID_ID,
  BRIDGE_WID_ID_MASK,		GZ_READ_ONLY},

{"BRIDGE_WID_STAT",		BRIDGE_WID_STAT,
  BRIDGE_WID_STAT_MASK,		GZ_READ_ONLY},

/* the error address registers are X at power-up, hence they are not tested */
{"BRIDGE_WID_ERR_UPPER",	BRIDGE_WID_ERR_UPPER,
  BRIDGE_WID_ERR_UPPER_MASK,	GZ_NO_ACCESS},

{"BRIDGE_WID_ERR_LOWER",	BRIDGE_WID_ERR_LOWER,
  BRIDGE_WID_ERR_LOWER_MASK,	GZ_NO_ACCESS},

/* Was GZ_READ_WRITE with single W/R pattern (0x55aa55aa), but is too
 * sensitive to the pattern written to it: made it GZ_READ_ONLY.
 * This also needs to stay read only to avoid a bridge race condition
 * when writing this register.
 */
{"BRIDGE_WID_CONTROL",		BRIDGE_WID_CONTROL,
  BRIDGE_WID_CONTROL_MASK,	GZ_READ_ONLY},

{"BRIDGE_WID_REQ_TIMEOUT",	BRIDGE_WID_REQ_TIMEOUT,
  BRIDGE_WID_REQ_TIMEOUT_MASK,	GZ_READ_WRITE},

/* the intr dest error address reg. are X at power-up, hence are not tested */
{"BRIDGE_WID_INT_UPPER",	BRIDGE_WID_INT_UPPER,
  BRIDGE_WID_INT_UPPER_MASK,	GZ_NO_ACCESS},

{"BRIDGE_WID_INT_LOWER",	BRIDGE_WID_INT_LOWER,
  BRIDGE_WID_INT_LOWER_MASK,	GZ_NO_ACCESS},

/* X at power-up, hence not tested */
{"BRIDGE_WID_ERR_CMDWORD",	BRIDGE_WID_ERR_CMDWORD,
  BRIDGE_WID_ERR_CMDWORD_MASK,	GZ_NO_ACCESS},

#if 0	/* Do not read write the register as there is potential bug in
	 * bridge with back to back writes after writing this register.
	 */
{"BRIDGE_WID_LLP",		BRIDGE_WID_LLP,
  BRIDGE_WID_LLP_MASK,		GZ_READ_WRITE},
#endif

/* When read, this register will return a 0x00 after all previous transfers to 
the Bridge have completed. Not tested */
{"BRIDGE_WID_TFLUSH",		BRIDGE_WID_TFLUSH,
  BRIDGE_WID_TFLUSH_MASK,	GZ_NO_ACCESS},

/* X at power-up, hence not tested */
{"BRIDGE_WID_AUX_ERR",		BRIDGE_WID_AUX_ERR,
  BRIDGE_WID_AUX_ERR_MASK,	GZ_NO_ACCESS},

{"BRIDGE_WID_RESP_UPPER",	BRIDGE_WID_RESP_UPPER,
  BRIDGE_WID_RESP_UPPER_MASK,	GZ_READ_ONLY},

{"BRIDGE_WID_RESP_LOWER",	BRIDGE_WID_RESP_LOWER,
  BRIDGE_WID_RESP_LOWER_MASK,	GZ_READ_ONLY},

{"BRIDGE_WID_TST_PIN_CTRL",	BRIDGE_WID_TST_PIN_CTRL,
  BRIDGE_WID_TST_PIN_CTRL_MASK,	GZ_READ_WRITE},

{"BRIDGE_DIR_MAP",		BRIDGE_DIR_MAP,
  BRIDGE_DIR_MAP_MASK,		GZ_READ_WRITE},

{"BRIDGE_ARB",			BRIDGE_ARB,
  BRIDGE_ARB_MASK,		GZ_READ_WRITE},

/* do a read-only test on the BRIDGE_NIC because most fields are counter */
/*  which start to count down at the time of the write. */
{"BRIDGE_NIC",			BRIDGE_NIC,
  BRIDGE_NIC_MASK,		GZ_READ_ONLY},

{"BRIDGE_PCI_BUS_TIMEOUT",	BRIDGE_PCI_BUS_TIMEOUT,
  BRIDGE_PCI_BUS_TIMEOUT_MASK,	GZ_READ_WRITE},

{"BRIDGE_PCI_CFG",		BRIDGE_PCI_CFG,
  BRIDGE_PCI_CFG_MASK,		GZ_READ_WRITE},

{"BRIDGE_PCI_ERR_UPPER",	BRIDGE_PCI_ERR_UPPER,
  BRIDGE_PCI_ERR_UPPER_MASK,	GZ_READ_ONLY},

{"BRIDGE_PCI_ERR_LOWER",	BRIDGE_PCI_ERR_LOWER,
  BRIDGE_PCI_ERR_LOWER_MASK,	GZ_READ_ONLY},

{"BRIDGE_INT_STATUS",		BRIDGE_INT_STATUS,
  BRIDGE_INT_STATUS_MASK,	GZ_READ_ONLY},

{"BRIDGE_INT_ENABLE",		BRIDGE_INT_ENABLE,
  BRIDGE_INT_ENABLE_MASK,	GZ_READ_WRITE},

/* write-only !*/
{"BRIDGE_INT_RST_STAT",		BRIDGE_INT_RST_STAT,
  BRIDGE_INT_RST_STAT_MASK,	GZ_NO_ACCESS},

{"BRIDGE_INT_MODE",		BRIDGE_INT_MODE,
  BRIDGE_INT_MODE_MASK,		GZ_READ_WRITE},

{"BRIDGE_INT_DEVICE",		BRIDGE_INT_DEVICE,
  BRIDGE_INT_DEVICE_MASK,	GZ_READ_WRITE},

/* X at power-up, not tested */
{"BRIDGE_INT_HOST_ERR",		BRIDGE_INT_HOST_ERR,
  BRIDGE_INT_HOST_ERR_MASK,	GZ_NO_ACCESS},

{"BRIDGE_INT_ADDR(0)",		BRIDGE_INT_ADDR(0),
  BRIDGE_INT_ADDR_MASK(0),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(1)",		BRIDGE_INT_ADDR(1),
  BRIDGE_INT_ADDR_MASK(1),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(2)",		BRIDGE_INT_ADDR(2),
  BRIDGE_INT_ADDR_MASK(2),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(3)",		BRIDGE_INT_ADDR(3),
  BRIDGE_INT_ADDR_MASK(3),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(4)",		BRIDGE_INT_ADDR(4),
  BRIDGE_INT_ADDR_MASK(4),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(5)",		BRIDGE_INT_ADDR(5),
  BRIDGE_INT_ADDR_MASK(5),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(6)",		BRIDGE_INT_ADDR(6),
  BRIDGE_INT_ADDR_MASK(6),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(7)",		BRIDGE_INT_ADDR(7),
  BRIDGE_INT_ADDR_MASK(7),	GZ_READ_WRITE},

/*removed as writes cause PANIC in rad,scsi register accesses
{"BRIDGE_DEVICE(0)",		BRIDGE_DEVICE(0),
  BRIDGE_DEVICE_MASK(0),	GZ_READ_WRITE},

{"BRIDGE_DEVICE(1)",		BRIDGE_DEVICE(1),
  BRIDGE_DEVICE_MASK(1),	GZ_READ_WRITE},

out: IP30 xtalk port 15 will always be a baseio bridge with
IOC3 using slots 2 and 4. So ide should not mess with slot 2 and 4 
{"BRIDGE_DEVICE(2)",		BRIDGE_DEVICE(2),
  BRIDGE_DEVICE_MASK(2),	GZ_READ_WRITE},


{"BRIDGE_DEVICE(3)",		BRIDGE_DEVICE(3),
  BRIDGE_DEVICE_MASK(3),	GZ_READ_WRITE},

out: IP30 xtalk port 15 will always be a baseio bridge with
IOC3 using slots 2 and 4. So ide should not mess with slot 2 and 4 
{"BRIDGE_DEVICE(4)",		BRIDGE_DEVICE(4),
  BRIDGE_DEVICE_MASK(4),	GZ_READ_WRITE},

{"BRIDGE_DEVICE(5)",		BRIDGE_DEVICE(5),
  BRIDGE_DEVICE_MASK(5),	GZ_READ_WRITE},

{"BRIDGE_DEVICE(6)",		BRIDGE_DEVICE(6),
  BRIDGE_DEVICE_MASK(6),	GZ_READ_WRITE},

{"BRIDGE_DEVICE(7)",		BRIDGE_DEVICE(7),
  BRIDGE_DEVICE_MASK(7),	GZ_READ_WRITE},
*/

{"BRIDGE_WR_REQ_BUF(0)",	BRIDGE_WR_REQ_BUF(0),
  BRIDGE_WR_REQ_BUF_MASK(0),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(1)",	BRIDGE_WR_REQ_BUF(1),
  BRIDGE_WR_REQ_BUF_MASK(1),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(2)",	BRIDGE_WR_REQ_BUF(2),
  BRIDGE_WR_REQ_BUF_MASK(2),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(3)",	BRIDGE_WR_REQ_BUF(3),
  BRIDGE_WR_REQ_BUF_MASK(3),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(4)",	BRIDGE_WR_REQ_BUF(4),
  BRIDGE_WR_REQ_BUF_MASK(4),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(5)",	BRIDGE_WR_REQ_BUF(5),
  BRIDGE_WR_REQ_BUF_MASK(5),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(6)",	BRIDGE_WR_REQ_BUF(6),
  BRIDGE_WR_REQ_BUF_MASK(6),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(7)",	BRIDGE_WR_REQ_BUF(7),
  BRIDGE_WR_REQ_BUF_MASK(7),	GZ_READ_ONLY},

{"", -1, -1, -1 }
};

/*
 * Name:        br_regs
 * Description: Tests Bridge Registers
 * Input:       -d, widget port no
 * Output:      Returns 0 if no error, 1 if error
 * Error Handling:
 * Side Effects: none
 * Remarks:
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
br_regs(__uint32_t argc, char **argv)
{
	Bridge_Regs	*regs_ptr = gz_bridge_regs;
	bridgereg_t	mask = ~0x0;

	SET_BRIDGE_XBOW_PORT(argc, argv);

	msg_printf(VRB, "Testing Bridge at xbow port 0x%x, addr 0x%x\n",
		bridge_xbow_port, PHYS_TO_K1(MAIN_WIDGET(bridge_xbow_port)));

	d_errors = 0; /* init it is incremented here,
			 not in the called subroutines */

	/* register loop */
	while (regs_ptr->name[0] != NULL) {
	    if (regs_ptr->mode == GZ_READ_WRITE) {
		if (_br_regs_read_write(regs_ptr)) {
		    d_errors++;
		    if (d_exit_on_error) goto _error;
		}
	    }
	    /* GZ_READ_ONLY, GZ_WRITE_ONLY, and GZ_NO_ACCESS are nops here */
	   regs_ptr++;
	}	/* end register loop */
	if (_special_br_cases()) {
		d_errors++;
	}

_error:
	/* XXX only on baseio bridge */
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Bridge Read-Write Register", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_BRIDGE_0003], d_errors );
}

/*
 * Name:        _br_regs_read_write
 * Description: Performs Write/Reads on Bridge Registers
 * Input:       Pointer to the register structure
 * Output:      Returns 0 if no error, 1 if error
 * Error Handling:
 * Side Effects: none
 * Remarks:
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_br_regs_read_write(Bridge_Regs *regs_ptr)
{
	bridgereg_t	saved_reg_val, write_val, read_val;
	bridgereg_t 	pattern[BR_REGS_PATTERN_MAX] = {
					0x5a5a5a5a, 0xa5a5a5a5,
					0xcccccccc, 0x33333333,
					0xf0f0f0f0, 0x0f0f0f0f};
	char		pattern_index;

	/* give register info if it was not given in default checking */
	msg_printf(DBG, "\t%s (0x%x) ", regs_ptr->name, regs_ptr->address);

	/* Save current value */
	BR_REG_RD_32(regs_ptr->address, ~0x0, saved_reg_val);


	for (pattern_index=0; pattern_index<HR_REGS_PATTERN_MAX; pattern_index++) {
            write_val = pattern[pattern_index];

	    /* Write test value */
	    BR_REG_WR_32(regs_ptr->address, regs_ptr->mask, write_val);

	    /* Read back register */
	    BR_REG_RD_32(regs_ptr->address, regs_ptr->mask, read_val);

	    /* Compare expected and received values */
	    if ((write_val & regs_ptr->mask) != read_val) {
		msg_printf(INFO, "\nERROR Write/Read: %s; Addr = 0x%x; Mask = 0x%x; \n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
			regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
			(write_val&regs_ptr->mask), read_val);
		/* Restore the register with old value */
		BR_REG_WR_32(regs_ptr->address, (bridgereg_t)~0x0, saved_reg_val);
		return(1); /* exit on first error */
	   }
	   else if (pattern_index == BR_REGS_PATTERN_MAX -1) {
			msg_printf(DBG, "mask= 0x%x; ", regs_ptr->mask);
			msg_printf(DBG, "Write/Read OK.\n");
	   }
	} /* pattern_index loop */

	/* Restore the register with old value */
	BR_REG_WR_32(regs_ptr->address, ~0x0, saved_reg_val);

	return(0);
}

/*
 * Name:	_special_br_cases
 * Description:	handles special information about some registers
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: none
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_special_br_cases()
{
	bridgereg_t	bridge_wid_id, host_err_field;

	msg_printf(INFO,"Misc bridge register info:\n");

	msg_printf(INFO,"\t\tslot 2 and 4 are not tested as IOC3 uses them\n");

	BR_REG_RD_32(BRIDGE_WID_ID, ~0x0, bridge_wid_id);
	/* per released specs, rev 1.3 */
	if ((bridge_wid_id & WIDGET_REV_NUM) && (bridge_wid_id & 1 == 1))
	    msg_printf(INFO,"\t\twidget revision number %d\n\t\twidget part number 0x%x\n\t\twidget manufacturer number %d\n", 
		(bridge_wid_id & WIDGET_REV_NUM) >> WIDGET_REV_NUM_SHFT,
		(bridge_wid_id & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT,
		(bridge_wid_id & WIDGET_MFG_NUM) >> WIDGET_MFG_NUM_SHFT);
	else return (1);

	/* read bridge interrupt number in heart */
	BR_REG_RD_32(BRIDGE_INT_HOST_ERR, ~0x0, host_err_field);
	msg_printf(INFO,"\t\tbridge widget interrupt (in heart) %d\n", 
			host_err_field);

	return (0);
}

void
get_bridge_port(int argc, char **argv)
{
	int n;

	if (argc == 1) {
		/* use build in bridge */
		bridge_xbow_port = 0x0f;
		return;
	}

	(void)atob(argv[1], &n);
	/* xbow port # must be > 8 and < 16 */
	if (n <= HEART_ID || n > BRIDGE_ID) {
		msg_printf(VRB,"xbow port # should be %d < id <= %d\n",
			HEART_ID,BRIDGE_ID);
		msg_printf(DBG,"Using built in bridge (0xf)\n");
		bridge_xbow_port = 0x0f;
		return;
	}
	bridge_xbow_port = n;
	return;
}
