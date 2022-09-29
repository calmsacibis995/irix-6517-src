/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1996 Silicon Graphics, Inc.                             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * evgraph.c-
 *      This file specifies the interface between the kernel and the PROM's
 *      configuration data structures.
 */
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/pda.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/iograph.h>
#include <sys/hwgraph.h>
#include <sys/systm.h>
#include <ksys/hwg.h>
#include <sys/vmereg.h>
#include "ev_private.h"

/*
 * Add a fixed everest root vertex in the hwgraph similar
 * to the one for lego
 * This is /hw/node/slot
 */
vertex_hdl_t
evhwg_base_add(vertex_hdl_t hwgraph_root)
{
	char		basename[MAXDEVNAME];
	vertex_hdl_t	basevhdl;
	
	sprintf(basename,
		EDGE_LBL_NODE"/"
		EDGE_LBL_SLOT);

	hwgraph_path_add(hwgraph_root,basename,&basevhdl);
	ASSERT(basevhdl != GRAPH_VERTEX_NONE);

	return basevhdl;
}
/*
 * Add a cpu vertex at the cpu board (IP19 | IP21 | IP25) vertex.
 *  /hw/node/slot/#/cpu/#
 *		    -----	
 */
void
evhwg_cpu_add(evcpucfg_t *evcpu,vertex_hdl_t master_node,int cpuslot)
{
	char		cpuname[MAXDEVNAME];
	vertex_hdl_t	cpuvhdl;
	cpuid_t		cpuid = evcpu->cpu_vpid;		
	
	/* Check if we have a valid cpu */
	if (!evcpu->cpu_enable)
		return;
#if MULTIKERNEL
	/* Only consider cpu enabled if it's in our cell */

	if ((cpuid & EVCFGINFO->ecfg_cellmask) != evmk_cellid)
		return;
#endif 

	sprintf(cpuname,EDGE_LBL_CPU"/%d",
		cpuslot);
	hwgraph_path_add(master_node,cpuname,&cpuvhdl);
	ASSERT(cpuvhdl != GRAPH_VERTEX_NONE);

	mark_cpuvertex_as_cpu(cpuvhdl,cpuid);
	/* Hang the cpu info off the cpu vertex as 
	 * labelled info 
	 */
	cpuinfo_set(cpuvhdl,pdaindr[cpuid].pda->pdinfo);
	/* Set the cpu board as the master */
	device_master_set(cpuvhdl, master_node);

	sprintf(cpuname, "%v", cpuvhdl);

	/* Allocate space to keep the processor name. */
	pdaindr[cpuid].pda->p_cpunamestr = kern_calloc(1,strlen(cpuname) + 1);

	/* Copy the name in */
	strcpy(pdaindr[cpuid].pda->p_cpunamestr, cpuname);
}	
/* 
 * Add the cpu vertices corr. to an IP19 | IP21 | IP25 board at
 * /hw/node/slot/#
 *		 
 */
void
evhwg_cpubrd_add(evbrdinfo_t *brd,vertex_hdl_t master_node)
{
	int		cpu;

	/* For each possible cpu slot on the board check if
	 * we have got a valid cpu there and add a vertex
	 * for it in the hwgraph 
	 */
	for(cpu = 0; cpu < EV_MAX_CPUS_BOARD;cpu++) {
		evhwg_cpu_add((evcpucfg_t *)&(brd->eb_cpu.eb_cpus[cpu]),
			      master_node,cpu);
	}
}

/* 
 * Add the IO4 vertex corr. to an IO4 board at /hw/node/slot/#
 */
/* ARGSUSED */
void
evhwg_io4brd_add(evbrdinfo_t *brd,vertex_hdl_t master_node)
{
	vertex_hdl_t	io4vhdl;
	
	hwgraph_path_add(master_node,EDGE_LBL_IO4,&io4vhdl);
	ASSERT(io4vhdl != GRAPH_VERTEX_NONE);
}

/*
 * Add a cpu board vertex at the everest hwgraph base.
 * /hw/node/slot/#
 *		 - 
 */
void
evhwg_brd_add(evbrdinfo_t *brd,vertex_hdl_t base)
{
	char		slotname[MAXDEVNAME];
	vertex_hdl_t	slotvhdl;

	sprintf(slotname,"%d/",brd->eb_slot);
	hwgraph_path_add(base,slotname,&slotvhdl);

	switch(brd->eb_type) {
	case EVTYPE_IP19:
	case EVTYPE_IP21:
	case EVTYPE_IP25:	
		evhwg_cpubrd_add(brd,slotvhdl);
		break;
	case EVTYPE_IO4:	
		evhwg_io4brd_add(brd,slotvhdl);
		break;
	default:
		break;

	}
}	
/*
 * Setup the pointers in the pda pointing to the cpu
 * info which also hangs off the cpu vertex.
 */
void init_platform_pda(pda_t *ppda, cpuid_t cpu)
{
	cpuinfo_t cpuinfo;

	/* Allocate per-cpu platform-dependent data */
	cpuinfo = (cpuinfo_t)kern_calloc(1,sizeof(struct cpuinfo_s));
	ASSERT(cpuinfo);

	/* Put the pointer to the cpu info in the corr.
	 * pda
	 */
	ppda->pdinfo = (void *)cpuinfo;

	/* Store the pointer back to the pda in the cpu info.
	 * This is convenient to get the pda through the cpu 
	 * vertex handle since this cpu info hangs off the 
	 * cpu vertex
	 */
	cpuinfo->ci_cpupda = ppda;
	cpuinfo->ci_cpuid = cpu;

}
/*
 *	Set up the cpu portion of the hwgraph 
 */
void
evhwg_setup(vertex_hdl_t root,cpumask_t boot_cpumask) 
{
	evbrdinfo_t	*brd;
	int		slot;
	vertex_hdl_t	base;
	int		cpu;

	/*
	 * Now set up platform specific stuff in the processor PDA.
	 */
	for (cpu = 0; cpu < maxcpus; cpu++) {
		/* Skip holes in CPU space */
		if (CPUMASK_TSTB(boot_cpumask, cpu)) {
			init_platform_pda(pdaindr[cpu].pda, cpu);
		}
	}

	base = evhwg_base_add(root);
	for(slot = 0; slot < EV_MAX_SLOTS;slot++) {
		brd = BOARD(slot);
		evhwg_brd_add(brd,base);
	}
}	

/*
 * Need to ensure that VME bus adaptors are in the iograph before we call
 * edt_init() for VME devices.  Since EVEREST didn't use this routine, we
 * steal it.
 */
void
init_all_devices(void)
{
	int vmeadapters = readadapters(ADAP_VME);
	int i;
	char loc_str[16];
	vertex_hdl_t vmevhdl, nodevhdl;
	graph_error_t rc;

	if (vmeadapters == 0) {
		return;
	}

	/*
	 * Do this here rather than in io/vme.c so that we ensure that the
	 * hardware graph is fully initialized.
	 */
	rc = hwgraph_traverse(GRAPH_VERTEX_NONE, EDGE_LBL_NODE, &nodevhdl);
	if (rc != GRAPH_SUCCESS) {
		cmn_err(CE_WARN,
			"failed to locate node entry in hardware graph");
		return;
	}

	/* add the busses to the inventory */
	for (i = 0; i < vmeadapters; i++) {
		sprintf(loc_str, "%s/%d", EDGE_LBL_VME, VMEADAP_ITOX(i));
		(void) hwgraph_path_add(nodevhdl, loc_str, &vmevhdl);
	}
}

/*
 * Hide the vagaries of VME bus naming from device drivers.
 * Returns a vertex handle for the VME bus with internal number N.
 */
graph_error_t
vme_hwgraph_lookup(uint adaptor, vertex_hdl_t *vmevhdl)
{
	char vmestr[32];
	graph_error_t rc;

	sprintf(vmestr, "%s/%s/%d", EDGE_LBL_NODE, EDGE_LBL_VME,
		VMEADAP_ITOX(adaptor));

	rc = hwgraph_traverse(GRAPH_VERTEX_NONE, vmestr, vmevhdl);
	return rc;
}

/*
 * Hide the vagaries of IO4 naming from device drivers.
 * Returns a vertex handle for the IO4 in slot N.
 */
graph_error_t
io4_hwgraph_lookup(uint slot, vertex_hdl_t *io4vhdl)
{
	char io4str[32];
	graph_error_t rc;

	sprintf(io4str, "%s/%s/%d/%s", EDGE_LBL_NODE, EDGE_LBL_SLOT, slot,
		EDGE_LBL_IO4);

	rc = hwgraph_traverse(GRAPH_VERTEX_NONE, io4str, io4vhdl);
	return rc;
}
