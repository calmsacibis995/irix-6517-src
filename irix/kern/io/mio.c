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

#ident  "io/mio.c: $Revision: 1.3 $"

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
#include <sys/PCI/ioc3.h>

int	mio_devflag = D_MP;

void			mio_init(void);
static nic_vmc_func	mio_vmc;

void
mio_init(void)
{
#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT, "mio_init()\n");
#endif
	nic_vmc_add("030-0880-", mio_vmc);
}

static void
mio_vmc(vertex_hdl_t vhdl)
{
	bridge_t       *bridge;

#if DEBUG
	cmn_err(CE_CONT, "%v looks like an MIO\n", vhdl);
#endif

	/*
	** MIO cards piggyback onto
	** IO6 cards, add an IOC3 in
	** slot 6 (no ethernet) and
	** a RAD in slot 7. We need
	** to specify the slot 6 subdevs
	** and set up our RRBs.
	*/
	pcibr_hints_subdevs(vhdl, 6,
			    IOC3_SDB_ECPP	|
			    IOC3_SDB_GENERIC	|
			    IOC3_SDB_KBMS	|
			    IOC3_SDB_SERIAL);

	pcibr_hints_fix_rrbs(vhdl);

	bridge = (bridge_t *)
		xtalk_piotrans_addr(vhdl, NULL, 
				    0, sizeof (bridge_t), 0);

	/*
	** We may be called before *or* after
	** the main io6 board.
	**
	** XXX- what if the main IO6 doesn't
	** show up in the NIC string?  Should
	** we trigger io6_badnic_vmc here?
	*/
	bridge->b_even_resp = (0x0FFFFFFF & bridge->b_even_resp) | 0xB0000000;
	bridge->b_odd_resp = (0x000FFFFF & bridge->b_odd_resp) | 0xBBB00000;
}
