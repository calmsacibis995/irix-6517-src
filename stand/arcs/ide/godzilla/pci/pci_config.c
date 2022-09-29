/*
 * pci_config.c.c
 *	
 *	PCI Configuration Space Tests
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

#ident "$Revision: 1.8 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/vdma.h"
#include "sys/time.h"
#include "sys/immu.h"
#include "sys/PCI/ioc3.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_frus.h"
#include "d_bridge.h"
#include "d_pci.h"

/*
 * Forward References 
 */
bool_t 	pci_cfg_read();
bool_t 	pci_cfg_write();

/*
 * Name:        pci_cfg_read
 * Description: Tests the PCI Configuration Read
 * Input:       None
 * Output:      Returns 0 if no error, 1 if error
 * Error Handling:
 * Side Effects: none
 * Remarks: Uses IOC3
 * Debug Status: compiles, simulated, not emulated, debugged
 */
bool_t
pci_cfg_read()
{
	ioc3reg_t	mask = ~0x0;
	ioc3reg_t	pci_id, pci_scr, pci_rev;
	__uint32_t	ioc3_dev_num = IOC3_PCI_SLOT;
	bridgereg_t	ioc3_cfg;

	msg_printf(DBG,"PCI Configuration Read Test... Begin\n");

	/* get the pointer to config space */
	ioc3_cfg = BRIDGE_TYPE0_CFG_DEV(ioc3_dev_num);
	msg_printf(DBG, "ioc3_cfg 0x%x\n", ioc3_cfg);

	/* read the ID register and verify */
	PIO_REG_RD_32((ioc3_cfg+IOC3_PCI_ID), mask, pci_id);
	msg_printf(DBG, "IOC3_PCI_ID 0x%x; pci_id 0x%x\n", 
		(ioc3_cfg+IOC3_PCI_ID), pci_id);

	/* verify if the register values are correct */
	if (pci_id != IOC3_PCI_ID_DEF) {
	    msg_printf(ERR, "PCI Configuration Read Error in PCI_ID\n\
		exp 0x%x; rcv 0x%x\n", IOC3_PCI_ID_DEF, pci_id);
	    d_errors++;
	}

	/* read Status and Revision register and display for more info */
	PIO_REG_RD_32((ioc3_cfg+IOC3_PCI_SCR), mask, pci_scr);
	PIO_REG_RD_32((ioc3_cfg+IOC3_PCI_REV), mask, pci_rev);
	msg_printf(SUM, "IOC3 Status/Command Regsiter 0x%x\n\
	IOC3 Revision Register 0x%x\n", pci_scr, pci_rev);

	/* XXX need to differentiate which PCI bus in FRU */
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "PCI Configuration Read", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_PCI_0000], d_errors );
}

/*
 * Name:        pci_cfg_write
 * Description: Tests the PCI Configuration Write
 * Input:       None
 * Output:      Returns 0 if no error, 1 if error
 * Error Handling:
 * Side Effects: none
 * Remarks: Uses IOC3
 * Debug Status: compiles, simulated, not emulated, debugged
 */
bool_t
pci_cfg_write()
{
	bridgereg_t	i, exp, rcv, mask = ~0x0;
	ioc3reg_t	ioc3_dev_num = IOC3_PCI_SLOT;
	ioc3reg_t	pci_lat_saved, pci_addr_saved;
	bridgereg_t	ioc3_cfg, ioc3_pci_lat, ioc3_pci_addr;

	msg_printf(DBG,"PCI Configuration Write Test... Begin\n");

	/* get the pointer to config space */
	ioc3_cfg = BRIDGE_TYPE0_CFG_DEV(ioc3_dev_num);
	ioc3_pci_lat = ioc3_cfg+IOC3_PCI_LAT;
	ioc3_pci_addr= ioc3_cfg+IOC3_PCI_ADDR;
	msg_printf(DBG,"ioc3_cfg 0x%x; ioc3_pci_lat 0x%x; ioc3_pci_addr 0x%x\n",
		ioc3_cfg, ioc3_pci_lat, ioc3_pci_addr);

	/*
	 * There are only two config registers worth writing and reading 
	 * back: PCI Latency Register and PCI Base Address Register
	 * So, these are used in this test
	 */
	/* Read them and save first */
	PIO_REG_RD_32(ioc3_pci_lat, mask, pci_lat_saved);
	PIO_REG_RD_32(ioc3_pci_addr, mask, pci_addr_saved);

	msg_printf(DBG, "pci_lat_saved 0x%x; pci_addr_saved 0x%x\n",
		pci_lat_saved, pci_addr_saved);

	for (i = 0; i < 32; i++) {
	    exp = (1 << i) & IOC3_PCI_LAT_TMR_MASK;
	    PIO_REG_WR_32(ioc3_pci_lat, mask, exp);
	    PIO_REG_RD_32(ioc3_pci_lat, mask, rcv);
	    if (exp != rcv) {
		msg_printf(ERR, "PCI Config Write Error in PCI Latency\n\
		exp 0x%x rcv 0x%x \n",exp, rcv, IOC3_PCI_LAT_TMR_MASK);
		d_errors++;
	    }
	    msg_printf(DBG, "PCI Latency exp 0x%x; rcv 0x%x\n", exp, rcv);
	    exp = ~exp & IOC3_PCI_LAT_TMR_MASK;
	    PIO_REG_WR_32(ioc3_pci_lat, mask, exp);
	    PIO_REG_RD_32(ioc3_pci_lat, mask, rcv);
	    if (exp != rcv) {
		msg_printf(ERR, "PCI Config Write Error in PCI Latency\n\
		exp 0x%x rcv 0x%x mask 0x%x\n",exp, rcv, IOC3_PCI_LAT_TMR_MASK);
		d_errors++;
	    }
	    msg_printf(DBG, "PCI Latency exp 0x%x; rcv 0x%x\n", exp, rcv);

	    exp = (1 << i) & IOC3_PCI_ADDR_BASE_MASK;
	    PIO_REG_WR_32(ioc3_pci_addr, mask, exp);
	    PIO_REG_RD_32(ioc3_pci_addr, mask, rcv);
	    if (exp != rcv) {
		msg_printf(ERR, "PCI Config Write Error in PCI Base Addr\n\
		exp 0x%x rcv 0x%x mask 0x%x\n",exp,rcv,IOC3_PCI_ADDR_BASE_MASK);
		d_errors++;
	    }
	    msg_printf(DBG, "PCI Base Addr exp 0x%x; rcv 0x%x\n", exp, rcv);
	    exp = ~exp & IOC3_PCI_ADDR_BASE_MASK;
	    PIO_REG_WR_32(ioc3_pci_addr, mask, exp);
	    PIO_REG_RD_32(ioc3_pci_addr, mask, rcv);
	    if (exp != rcv) {
		msg_printf(ERR, "PCI Config Write Error in PCI Base Addr\n\
		exp 0x%x rcv 0x%x mask 0x%x\n",exp,rcv,IOC3_PCI_ADDR_BASE_MASK);
		d_errors++;
	    }
	    msg_printf(DBG, "PCI Base Addr exp 0x%x; rcv 0x%x\n", exp, rcv);
	}

	/* restore the registers */
	PIO_REG_WR_32(ioc3_pci_lat, mask, pci_lat_saved);
	PIO_REG_WR_32(ioc3_pci_addr, mask, pci_addr_saved);

	/* XXX need to differentiate which PCI bus in FRU */
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "PCI Configuration Write", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_PCI_0001], d_errors );
}

