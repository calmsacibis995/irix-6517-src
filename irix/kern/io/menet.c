/**************************************************************************
 *                                                                        *
 *                 Copyright (C) 1996, Silicon Graphics, Inc              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "io/menet.c: $Revision: 1.6 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/nic.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/pcibr.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/ioc3.h>

int	menet_devflag = D_MP;

void			menet_init(void);
static nic_vmc_func	menet_vmc;

void
menet_init(void)
{
#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT, "menet_init()\n");
#endif
	nic_vmc_add("030-0873-", menet_vmc);
}

static void
menet_vmc(vertex_hdl_t vhdl)
{
	int rtn_val;

#if DEBUG
	cmn_err(CE_CONT, "%v looks like an MENET\n", vhdl);
#endif

	/* MENET cards have four IOC3 chips, which are
	 * attached to two sets of PCI slot resources
	 * each: the primary connections are on slots
	 * 0..3 and the secondaries are on 4..7
	 */

	pcibr_hints_dualslot(vhdl, 0, 4);
	pcibr_hints_dualslot(vhdl, 1, 5);
	pcibr_hints_dualslot(vhdl, 2, 6);
	pcibr_hints_dualslot(vhdl, 3, 7);

	/* All four ethernets are brought out to
	 * connectors; six serial ports (a pair from
	 * each of the first three IOC3s) are brought
	 * out to MiniDINs; all other subdevices are
	 * left swinging in the wind, leave them
	 * disabled.
	 */

	pcibr_hints_subdevs(vhdl, 0, IOC3_SDB_ETHER|IOC3_SDB_SERIAL);
	pcibr_hints_subdevs(vhdl, 1, IOC3_SDB_ETHER|IOC3_SDB_SERIAL);
	pcibr_hints_subdevs(vhdl, 2, IOC3_SDB_ETHER|IOC3_SDB_SERIAL);
	pcibr_hints_subdevs(vhdl, 3, IOC3_SDB_ETHER);
	
	/* tell pcibr not to mess with the RRB allocation
	 */
	pcibr_hints_fix_rrbs(vhdl);

	/* set up the Read Response Buffer allocation
	 */
	
	rtn_val = pcibr_alloc_all_rrbs(vhdl, 0, 3,0, 3,0, 1,0, 1,0);
	if (rtn_val == -1) {
		cmn_err(CE_PANIC, "menet failed to alloc even rrbs\n");
	}
	rtn_val = pcibr_alloc_all_rrbs(vhdl, 1, 3,0, 3,0, 1,0, 1,0);
	if (rtn_val == -1) {
		cmn_err(CE_PANIC, "menet failed to alloc odd rrbs\n");
	}
}
