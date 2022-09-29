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

#ident  "io/io6.c: $Revision: 1.24 $"

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

int	io6_devflag = D_MP;

void			io6_init(void);
static nic_vmc_func	io6_vmc;

void
io6_init(void)
{

#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT, "io6_init()\n");
#endif
	nic_vmc_add("030-0734-", io6_vmc);
	nic_vmc_add("030-1023-", io6_vmc);
	nic_vmc_add("030-1124-", io6_vmc);
}

static void
io6_vmc(vertex_hdl_t vhdl)
{
	bridge_t       *bridge;
	dev_t		contr;
	int		rv;
	ulong_t		more_subdevs = 0;

	/* "G" variant has KB/MS connectors added. */
	if (nic_vertex_info_match(vhdl, "030-0734-"))
		more_subdevs |= IOC3_SDB_KBMS;

#ifdef SN0
	if (private.p_sn00)
		panic("SN00 without programed nic on mother board");
#endif
	pcibr_hints_dualslot(vhdl, 2, 4);
	pcibr_hints_subdevs(vhdl, 2,
			    IOC3_SDB_ETHER	|
			    IOC3_SDB_GENERIC	|
			    IOC3_SDB_NIC	|
			    IOC3_SDB_RT		|
			    IOC3_SDB_SERIAL	|
			    more_subdevs);

	/*
	** XXX- do we have any real evidence
	** that the automagic  RRB handling in
	** pcibr does not do significantly worse
	** than this *incorrect* preallocation?
	**
	** NOTE: if we remove this, then we also
	** need to do the same thing to the
	** daughterboard code (mio.c).
	**
	** we use seven even RRBs
	** and five odd RRBs, reserving
	** the top even RRB and the three
	** top odd RRBs for daughterboards.
	**
	** daughter boards may be called
	** by NIC before *or* after us,
	** so be careful!
	**
	**    Population:		errb	orrb
	**	0- ql			4+1
	**	1- ql				4+1
	**	2- ioc3 ethernet	1
	**	3- 
	**	4- ioc3 secondary	1
	*/
	
	bridge = (bridge_t *)
		xtalk_piotrans_addr(vhdl, NULL, 
				    0, sizeof (bridge_t), 0);

	pcibr_hints_fix_rrbs(vhdl);
	bridge->b_even_resp = (0xF0000000 & bridge->b_even_resp) | 0x0CA98888;
	bridge->b_odd_resp = (0xFFF00000 & bridge->b_odd_resp) | 0x000C8888;

	/*
	 * Here we lable the controller node on the IO6 as being a prom.
	 * This is used by the flash command
	 */
	rv = hwgraph_traverse(vhdl, EDGE_LBL_PCI "/" EDGE_LBL_CONTROLLER, &contr);
	ASSERT(rv == GRAPH_SUCCESS);
	rv = rv;
	device_inventory_add(contr, INV_PROM, INV_IO6PROM,
			     NULL, NULL, NULL);
}
