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

/*
 * SN0 PCI "Shoebox"
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/kern/io/RCS/pciiox.c,v 1.1 1996/09/23 23:23:36 nn Exp $"

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

int	pciiox_devflag = D_MP;

void			pciiox_init(void);
static nic_vmc_func	pciiox_vmc;

void
pciiox_init(void)
{
#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT, "pciiox_init()\n");
#endif
	nic_vmc_add("030-1062-", pciiox_vmc);
}

static void
pciiox_vmc(vertex_hdl_t vhdl)
{
	bridge_t       *bridge;

#if DEBUG
	cmn_err(CE_CONT, "%v looks like PCI Shoebox\n", vhdl);
#endif

	/* tell pcibr not to mess with the RRB allocation
	 */
	pcibr_hints_fix_rrbs(vhdl);

	/* set up default Read Response Buffer allocation
	 */
	bridge = (bridge_t *)
		xtalk_piotrans_addr(vhdl, NULL, 
				    0, sizeof (bridge_t), 0);

	/*
	 * Shoebox has three slots: 0, 1, 2 .
	 * Give each slot 3 buffers.
	 */
	bridge->b_even_resp = 0x00999888;
	bridge->b_odd_resp = 0x00000888;
}
