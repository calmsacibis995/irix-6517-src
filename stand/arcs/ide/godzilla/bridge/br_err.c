/*
 * br_err.c - bridge error tests
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
 *
 * NOTE: PCI_DEVICE_CONNECTED is not defined anywhere for now XXX
 *	  one should define it to one when a CI is actually connected
 */

#ident "ide/godzilla/bridge/br_err.c: $Revision 1.1$"


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
 * Name:	br_err
 * Description:	Bridge Error Tests
 *		This tests try to create several error conditions in
 *		Bridge data path and report it.
 *		1. do a read/write on an invalid address
 * Input:	None
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling:
 * Side Effects: none
 * Remarks:	 
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
br_err(__uint32_t argc, char **argv)
{
#ifdef PCI_DEVICE_CONNECTED
	bool_t	io_mem = 0x1;
	bridgereg_t	add_off = 0x100001;
	unsigned char	dev = 0x0;
#endif
	bridgereg_t	data = 0x11223344;
	bridgereg_t	int_status = 0x0;
	bridgereg_t	mask = ~0x0;
	int		count = 0;

	SET_BRIDGE_XBOW_PORT(argc, argv);

	msg_printf(DBG,"Bridge Error Test. Begin\n");

	/* reset the heart and bridge prior to starting */
	if (_hb_reset(RES_HEART, RES_BRIDGE)) {
	    d_errors++;
	    goto _error;
	}

	/* heart and Bridge check out */
	if (_hb_chkout(CHK_HEART, CHK_BRIDGE)) {
	    msg_printf(ERR, "Heart/Bridge Check Out Error\n");
	    goto _error;
	}

        /* reset the bridge ISR */
        BR_REG_WR_32(BRIDGE_INT_RST_STAT, ~0x0, 0x7f);
        /* enable the INVALID_ADDRESS bit in the B INT_ENABLE reg */
        BR_REG_WR_32(BRIDGE_INT_ENABLE, mask, BRIDGE_ISR_INVLD_ADDR);

	/* 1. try to perform a read & write on an invalid address */
	BR_REG_WR_32((BRIDGE_DEVIO0 - 0x4), mask, data);

	/* 2. read the interrupt status register */
	count = 0xfff;
	while ((int_status == 0) && (count != 0)) {
	    BR_REG_RD_32(BRIDGE_INT_STATUS, mask, int_status);
	    count --;
	}
	msg_printf(DBG, "Bridge interrupt status = 0x%x\n", int_status);
	if (int_status & BRIDGE_ISR_INVLD_ADDR) {
	    d_errors = 0x0; /* no error w.r.t this test */
	} else {
	    msg_printf(ERR, "INVALID ADDRESS bit NOT set in ISR\n");
	    d_errors ++;
	    if (d_exit_on_error) goto _error;
	}

#ifdef PCI_DEVICE_CONNECTED
	/* 
	 * XXX: The following test is used when a PCI device is
	 * connected to Bridge
	 */
	if (_br_pci_master_core(io_mem, add_off, data, dev, &int_status)) {
	    /* found the error, as expected */
	    if (int_status & BRIDGE_ISR_INVLD_ADDR) {
		d_errors = 0x0; /* no error w.r.t this test */
	    } else {
		msg_printf(ERR, "INVALID ADDRESS bit NOT set in ISR\n");
		if (d_exit_on_error) goto _error;
	    }
	} else {
	    /* no error found, which is an error for this test */
	    msg_printf(ERR, "Bridge did not flag an error\n");
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}
#endif

_error:
	    if (_hb_reset(RES_HEART, RES_BRIDGE)) {
		d_errors++;
	    }  

	/* XXX D_FRU_IP30 only on base bridge.  Bridge diag should run
	 * XXX on other bridges.
	 */
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Bridge Error", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_BRIDGE_0000], d_errors );
}

