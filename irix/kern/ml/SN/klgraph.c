/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * klgraph.c-
 *      This file specifies the interface between the kernel and the PROM's
 *      configuration data structures.
 */


#include <sys/types.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/pda.h>
#include <sys/kopt.h>
#include <sys/SN/agent.h>
#include <sys/SN/gda.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/module.h>
#include <sys/SN/router.h>
#include <sys/SN/nvram.h>
#include <sys/graph.h>
#include <ksys/hwg.h>
#include <sys/iograph.h>
#include <sys/invent.h>
#include <sys/driver.h>
#include <sys/nic.h>

#if defined(SN0)
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/diagval_strs.i>
#include <sys/SN/SN0/ip27log.h>
#endif

#ifdef KLGRAPH_DEBUG
#define GRPRINTF(x)	printf x
#define CE_GRPANIC	CE_PANIC
#else
#define GRPRINTF(x)
#define CE_GRPANIC	CE_PANIC
#endif

#include <sys/systm.h>
#include "sn_private.h"

extern char arg_maxnodes[];

/*
 * Gets reason for diagval using table lookup.
 */
static char*
get_diag_string(uint diagcode)
{
  int num_entries;
  int i;
  num_entries = sizeof(diagval_map) / sizeof(diagval_t);
  for (i = 0; i < num_entries; i++){
    if ((unchar)diagval_map[i].dv_code == (unchar)diagcode)
      return diagval_map[i].dv_msg;
  }
  return "Unknown";
}


/*
 * Support for verbose inventory via hardware graph. 
 * klhwg_invent_alloc allocates the necessary size of inventory information
 * and fills in the generic information.
 */
invent_generic_t *
klhwg_invent_alloc(cnodeid_t cnode, int class, int size)
{
	invent_generic_t *invent;

	invent = kern_malloc(size);
	if (!invent) return NULL;
	
	invent->ig_module = NODE_MODULEID(cnode);
	invent->ig_slot = SLOTNUM_GETSLOT(NODE_SLOTID(cnode));
	invent->ig_invclass = class;

	return invent;
}


/*
 * Add detailed cpu inventory info to the hardware graph.
 */
void
klhwg_cpu_invent_info(vertex_hdl_t cpuv,
		      cnodeid_t cnode, 
		      klcpu_t *cpu)
{
	invent_cpuinfo_t *cpu_invent;
	static int	 cpu_frequency = 0;	/* Speed at which the cpus
						 * are running in the system.
						 * Right now the valid values
						 * are all at 195Mhz or all at
						 * 180Mhz.
						 */

	nasid_t		nasid;
	char		backplane[8];

	cpu_invent = (invent_cpuinfo_t *) 
	    klhwg_invent_alloc(cnode, INV_PROCESSOR, sizeof(invent_cpuinfo_t));
	if (!cpu_invent)
	    return;

	if (KLCONFIG_INFO_ENABLED((klinfo_t *)cpu))
	    cpu_invent->ic_gen.ig_flag = INVENT_ENABLED;
	else
	    cpu_invent->ic_gen.ig_flag = 0x0;

	cpu_invent->ic_cpu_info.cpuflavor = cpu->cpu_prid;
	cpu_invent->ic_cpu_info.cpufq = cpu->cpu_speed;
	cpu_invent->ic_cpu_info.sdfreq = cpu->cpu_scachespeed;


	/* If this is the first cpu we are looking at 
	 * store its speed for later use.
	 */
	if (!cpu_frequency)
		cpu_frequency = cpu->cpu_speed;
	else {
		/* Check if the current cpu's speed matches the common speed
		 * of the ones seen so far. Panic in the case of mismatched
		 * speeds.
		 */
		 /* However, for the new IP31 boards, we will have to
		  * allow for mixed CPU speeds. The new boards are also
		  * supposed to have a IP27 prom that adjusts for max-busrts
		  * of hub/routers. One way of checking that is to find
		  * whether the prom has identified the back-plane frequency
		  * and stored it in prom-log. The other way is to look out
		  * out for occurrance of IP31 in the nic. But the nic is 
		  * not available at this point.
		  */
		
		nasid = COMPACT_TO_NASID_NODEID(cnode);
		if(ip27log_getenv(nasid, BACK_PLANE, backplane,"000",0)>= 0)
			;
		else if (cpu_frequency != cpu->cpu_speed) 
			cmn_err(CE_PANIC,
				"Speed of the CPU %d (%v) is %dMHz, different "
				"from another CPU at %dMHz\n",
				cpu->cpu_info.virtid,cpuv,cpu->cpu_speed,
				cpu_frequency);
	}
		
	cpu_invent->ic_cpu_info.sdsize = cpu->cpu_scachesz;
	cpu_invent->ic_cpuid = cpu->cpu_info.virtid;
	cpu_invent->ic_slice = cputoslice(cpu->cpu_info.virtid);

	if (cpu->cpu_prid < 0x926)
		cmn_err(CE_WARN, "CPU %d (%v) is downrev (%x)",
			cpu->cpu_info.virtid, cpuv, cpu->cpu_prid);

	hwgraph_info_add_LBL(cpuv, INFO_LBL_DETAIL_INVENT, 
			     (arbitrary_info_t) cpu_invent);
        hwgraph_info_export_LBL(cpuv, INFO_LBL_DETAIL_INVENT,
				sizeof(invent_cpuinfo_t));
}




/*
 * Add detailed disabled cpu inventory info to the hardware graph.
 */
void
klhwg_disabled_cpu_invent_info(vertex_hdl_t cpuv,
			       cnodeid_t cnode, 
			       klcpu_t *cpu, slotid_t slot)
{
	invent_cpuinfo_t *cpu_invent;
	diag_inv_t       *diag_invent;

	cpu_invent = (invent_cpuinfo_t *) 
	  klhwg_invent_alloc(cnode, INV_PROCESSOR, sizeof(invent_cpuinfo_t));
	if (!cpu_invent)
	    return;

	/* Diag information on this processor */
	diag_invent = (diag_inv_t *) 
	  klhwg_invent_alloc(cnode, INV_CPUDIAGVAL, sizeof(diag_inv_t));

	if (!diag_invent)
	    return;


	/* Disabled CPU */
	cpu_invent->ic_gen.ig_flag = 0x0;
	cpu_invent->ic_gen.ig_slot = slot;
	cpu_invent->ic_cpu_info.cpuflavor = cpu->cpu_prid;
	cpu_invent->ic_cpu_info.cpufq = cpu->cpu_speed;
	cpu_invent->ic_cpu_info.sdfreq = cpu->cpu_scachespeed;

	/* Fill in diagval label */
	diag_invent->diagval = ((klinfo_t *)cpu)->diagval;
	sprintf(diag_invent->name, "%s", get_diag_string(diag_invent->diagval));
	diag_invent->virtid = cpu->cpu_info.virtid;
	diag_invent->physid = cpu->cpu_info.physid; 

	cpu_invent->ic_cpu_info.sdsize = cpu->cpu_scachesz;
	cpu_invent->ic_cpuid = cpu->cpu_info.virtid;
	cpu_invent->ic_slice = cpu->cpu_info.physid; 

	if (cpu->cpu_prid < 0x926)
		cmn_err(CE_WARN, "CPU %d (%v) is downrev (%x)",
			cpu->cpu_info.virtid, cpuv, cpu->cpu_prid);

	/* Disabled CPU label */
	hwgraph_info_add_LBL(cpuv, INFO_LBL_DETAIL_INVENT, 
			     (arbitrary_info_t) cpu_invent);
        hwgraph_info_export_LBL(cpuv, INFO_LBL_DETAIL_INVENT,
				sizeof(invent_cpuinfo_t));

	/* Diagval label - stores reason for disable +{virt,phys}id +diagval*/
	hwgraph_info_add_LBL(cpuv, INFO_LBL_DIAGVAL, 
			     (arbitrary_info_t) diag_invent);

        hwgraph_info_export_LBL(cpuv, INFO_LBL_DIAGVAL, 
				sizeof(diag_inv_t));

	
}




/* 
 * Add information about the baseio prom version number
 * as a part of detailed inventory info in the hwgraph.
 */
void
klhwg_baseio_inventory_add(vertex_hdl_t baseio_vhdl,cnodeid_t cnode)
{
	invent_miscinfo_t	*baseio_inventory;
	unsigned char		version = 0,revision = 0;

	/* Allocate memory for the "detailed inventory" info
	 * for the baseio
	 */
	baseio_inventory = (invent_miscinfo_t *) 
		klhwg_invent_alloc(cnode, INV_PROM, sizeof(invent_miscinfo_t));
	baseio_inventory->im_type = INV_IO6PROM;
	/* Read the io6prom revision from the nvram */
	nvram_prom_version_get(&version,&revision);
	/* Store the revision info  in the inventory */
	baseio_inventory->im_version = version;
	baseio_inventory->im_rev = revision;
	/* Put the inventory info in the hardware graph */
	hwgraph_info_add_LBL(baseio_vhdl, INFO_LBL_DETAIL_INVENT, 
			     (arbitrary_info_t) baseio_inventory);
	/* Make the information available to the user programs
	 * thru hwgfs.
	 */
        hwgraph_info_export_LBL(baseio_vhdl, INFO_LBL_DETAIL_INVENT,
				sizeof(invent_miscinfo_t));
}

char	*hub_rev[] = {
	"0.0",
	"1.0",
	"2.0",
	"2.1",
	"2.2",
	"2.3"
};

/*
 * Add detailed cpu inventory info to the hardware graph.
 */
void
klhwg_hub_invent_info(vertex_hdl_t hubv,
		      cnodeid_t cnode, 
		      klhub_t *hub)
{
	invent_miscinfo_t *hub_invent;

	hub_invent = (invent_miscinfo_t *) 
	    klhwg_invent_alloc(cnode, INV_MISC, sizeof(invent_miscinfo_t));
	if (!hub_invent)
	    return;

	if (KLCONFIG_INFO_ENABLED((klinfo_t *)hub))
	    hub_invent->im_gen.ig_flag = INVENT_ENABLED;

	hub_invent->im_type = INV_HUB;
	hub_invent->im_rev = hub->hub_info.revision;
	hub_invent->im_speed = hub->hub_speed;
#if defined (SN0)
	if (hub_invent->im_rev < HUB_REV_2_1)
	    cmn_err(CE_WARN,
		"INVALID CUSTOMER CONFIGURATION - "
		"DOWNREV Hub ASIC: rev %s (code=%d) at %v.",
		hub_rev[hub_invent->im_rev],
                hub_invent->im_rev, hubv);
	
	if ((hub_invent->im_rev < HUB_REV_2_3) && (maxcpus > 32))
	    cmn_err(CE_WARN,
		"INVALID CUSTOMER CONFIGURATION - All Hub ASICs need to "
		"be atleast rev 2.3 for configurations exceeding 32 CPUs."
		"\n\tDOWNREV Hub ASIC: rev %s (code=%d) at %v.",
		hub_rev[hub_invent->im_rev],
                hub_invent->im_rev, hubv);
#endif /* SN0 */

	hwgraph_info_add_LBL(hubv, INFO_LBL_DETAIL_INVENT, 
			     (arbitrary_info_t) hub_invent);
        hwgraph_info_export_LBL(hubv, INFO_LBL_DETAIL_INVENT,
				sizeof(invent_miscinfo_t));
}

/*
 * Add detailed memory inventory info to the hardware graph.
 */

void
klhwg_mem_invent_info(vertex_hdl_t memv,
		      cnodeid_t cnode, 
		      klmembnk_t *mem, slotid_t slot, nasid_t mem_nasid)
{
	invent_meminfo_t *mem_invent;
	int inv_size;
	int mem_size;
	int bank;
	int disable;
	char disable_sz[MD_MEM_BANKS + 1];
	char mem_disable[MD_MEM_BANKS + 1];
	int cpu_speed;

	inv_size = sizeof(invent_meminfo_t) + 
	    (MD_MEM_BANKS - 1) * sizeof(invent_membnkinfo_t);

	mem_invent = (invent_meminfo_t *) 
	    klhwg_invent_alloc(cnode, INV_MEMORY, inv_size);
	if (!mem_invent)
	    return;

	mem_invent->im_gen.ig_slot = slot;
	if (KLCONFIG_INFO_ENABLED((klinfo_t *)mem))
	    mem_invent->im_gen.ig_flag = INVENT_ENABLED;
	else
	    mem_invent->im_gen.ig_flag = 0x0;

	disable = (ip27log_getenv(mem_nasid, "DisableMemMask", mem_disable,
0, 0) >= 0);
	if (disable) {
                ip27log_getenv(mem_nasid, "DisableMemSz", disable_sz, 0, 0);
                }

	/* 
	 * Need to know if this is an IP27 or IP31 board when
	 * testing for 256 MB dimm banks.
	 */
	cpu_speed = (int)(IP27CONFIG_NODE(mem_nasid).freq_cpu / 1000000);

	for (mem_size = 0, bank = 0; bank < MD_MEM_BANKS; bank++) {
		mem_invent->im_bank_info[bank].imb_size = 
		    KLCONFIG_MEMBNK_SIZE(mem, bank);
		mem_invent->im_bank_info[bank].imb_attr = 0;
		mem_invent->im_bank_info[bank].imb_flag = 0;

		/* Check for invalid 256MB memory bank size on IP27 nodes */
		if (KLCONFIG_MEMBNK_SIZE(mem, bank) == MD_SIZE_MBYTES(MD_SIZE_256MB) &&
		    (cpu_speed == 180 || cpu_speed == 195)) {
		/*
		 * In the case of 256MB dimm banks, make sure we're on
		 * an IP31 node board.  If not, issue a warning.
		 */
			cmn_err(CE_WARN,
				"Found 256 MB in bank %d on IP27 node in module %d slot %d\n",
				bank, mem_invent->im_gen.ig_module, slot);
		}
		mem_size += KLCONFIG_MEMBNK_SIZE(mem, bank);

		if(mem->membnk_info.struct_version >= 2)
		{
		  if (KLCONFIG_MEMBNK_PREMIUM(mem, bank))
		    mem_invent->im_bank_info[bank].imb_attr |= INV_MEM_PREMIUM;
		}
		else
		{
		  if (KLCONFIG_MEMBNK_PREMIUM(mem, 0))
		    mem_invent->im_bank_info[bank].imb_attr |= INV_MEM_PREMIUM;
		}
		if(mem->membnk_info.struct_version < 2)
		{
		    if (KLCONFIG_INFO_ENABLED((klinfo_t *)mem))
		    	mem_invent->im_bank_info[bank].imb_flag |= INVENT_ENABLED;
		}
		else 
		{
		    if (disable && strchr(mem_disable, '0' + bank))
                    {
			mem_invent->im_bank_info[bank].imb_size = 
                         MD_SIZE_MBYTES(disable_sz[bank] - '0');
		    }
		    else
		    {
		    	mem_invent->im_bank_info[bank].imb_flag |= INVENT_ENABLED;
		    }

		}
	}
	mem_invent->im_banks = MD_MEM_BANKS;
	mem_invent->im_size = mem_size;

	hwgraph_info_add_LBL(memv, INFO_LBL_DETAIL_INVENT, 
			     (arbitrary_info_t) mem_invent);
	hwgraph_info_export_LBL(memv, INFO_LBL_DETAIL_INVENT, inv_size);
}

/* ARGSUSED */
void
klhwg_add_hub(vertex_hdl_t node_vertex, klhub_t *hub, cnodeid_t cnode)
{
	vertex_hdl_t myhubv;
	int rc;

	GRPRINTF(("klhwg_add_hub: adding %s\n", EDGE_LBL_HUB));

	(void) hwgraph_path_add(node_vertex, EDGE_LBL_HUB, &myhubv);
	device_master_set(myhubv, node_vertex);

#if defined (SN0)
	rc = hwgraph_info_add_LBL(myhubv, INFO_LBL_HUB_INFO,
                        (arbitrary_info_t)(&NODEPDA(cnode)->hubstats));

	if (rc != GRAPH_SUCCESS) {
		cmn_err(CE_WARN,
			"klhwg_add_hub: Can't add hub info label %v, code %d",
			myhubv, rc);
	}
#endif

	klhwg_hub_invent_info(myhubv, cnode, hub);

#if defined (SN0)
	init_hub_stats(cnode, NODEPDA(cnode));

	sn0drv_attach(myhubv);
#endif
}

/* ARGSUSED */
void 
klhwg_add_disabled_cpu(vertex_hdl_t n_vertex, cnodeid_t cnode, klcpu_t *cpu, slotid_t slot)
{
        vertex_hdl_t mycpuv;
	char name[120];
	cpuid_t cpu_id;
	cpu_id = cpu->cpu_info.virtid;
	if(cpu_id != -1){
	  sprintf(name, "%s/%c", EDGE_LBL_CPU, 'a' + cpu->cpu_info.physid);
	  (void) hwgraph_path_add(n_vertex, name, &mycpuv);
	  
	  mark_cpuvertex_as_cpu(mycpuv, cpu_id);	
	  device_master_set(mycpuv, n_vertex);

	  klhwg_disabled_cpu_invent_info(mycpuv, cnode, cpu, slot);
	  return;
	}
}

/* ARGSUSED */
void
klhwg_add_cpu(vertex_hdl_t node_vertex, cnodeid_t cnode, klcpu_t *cpu)
{
	vertex_hdl_t mycpuv;
	char name[120];
	cpuid_t cpu_id;

	cpu_id = cpu->cpu_info.virtid;

	sprintf(name, "%s/%c", EDGE_LBL_CPU, 'a' + cputoslice(cpu_id));

	GRPRINTF(("klhwg_add_cpu: adding %s to vertex 0x%x\n", EDGE_LBL_CPU,
			node_vertex));

	(void) hwgraph_path_add(node_vertex, name, &mycpuv);

	mark_cpuvertex_as_cpu(mycpuv, cpu->cpu_info.virtid);
	cpuinfo_set(mycpuv, pdaindr[cpu->cpu_info.virtid].pda->pdinfo);
	device_master_set(mycpuv, node_vertex);

	klhwg_cpu_invent_info(mycpuv, cnode, cpu);

	sprintf(name, "%v", mycpuv);

	/* Allocate space to keep the processor name. */
	pdaindr[cpu->cpu_info.virtid].pda->p_cpunamestr =
			kmem_alloc_node(strlen(name) + 1, VM_DIRECT|KM_NOSLEEP,
					cnode);
	ASSERT_ALWAYS(pdaindr[cpu->cpu_info.virtid].pda->p_cpunamestr);

	/* Copy the name in */
	strcpy(pdaindr[cpu->cpu_info.virtid].pda->p_cpunamestr, name);
}

void
klhwg_add_rps(vertex_hdl_t node_vertex, cnodeid_t cnode, int flag)
{
	vertex_hdl_t myrpsv;
	invent_rpsinfo_t *rps_invent;
	int rc;

        if(cnode == CNODEID_NONE)
                return;                                                        
	
	GRPRINTF(("klhwg_add_rps: adding %s to vertex 0x%x\n", EDGE_LBL_RPS,
		node_vertex));

	rc = hwgraph_path_add(node_vertex, EDGE_LBL_RPS, &myrpsv);
	if (rc != GRAPH_SUCCESS)
		return;

	device_master_set(myrpsv, node_vertex);

        rps_invent = (invent_rpsinfo_t *)
            klhwg_invent_alloc(cnode, INV_RPS, sizeof(invent_rpsinfo_t));

        if (!rps_invent)
            return;

	rps_invent->ir_xbox = 0;	/* not an xbox RPS */

        if (flag)
            rps_invent->ir_gen.ig_flag = INVENT_ENABLED;
        else
            rps_invent->ir_gen.ig_flag = 0x0;                                  

        hwgraph_info_add_LBL(myrpsv, INFO_LBL_DETAIL_INVENT,
                             (arbitrary_info_t) rps_invent);
        hwgraph_info_export_LBL(myrpsv, INFO_LBL_DETAIL_INVENT,
                                sizeof(invent_rpsinfo_t));                     
	
}

/*
 * klhwg_update_rps gets invoked when the system controller sends an 
 * interrupt indicating the power supply has lost/regained the redundancy.
 * It's responsible for updating the Hardware graph information.
 *	rps_state = 0 -> if the rps lost the redundancy
 *		  = 1 -> If it is redundant. 
 */
void 
klhwg_update_rps(cnodeid_t cnode, int rps_state)
{
        vertex_hdl_t node_vertex;
        vertex_hdl_t rpsv;
        invent_rpsinfo_t *rps_invent;                                          
        int rc;
        if(cnode == CNODEID_NONE)
                return;                                                        

        node_vertex = cnodeid_to_vertex(cnode);                                
	rc = hwgraph_edge_get(node_vertex, EDGE_LBL_RPS, &rpsv);
        if (rc != GRAPH_SUCCESS)  {
		return;
	}

	rc = hwgraph_info_get_LBL(rpsv, INFO_LBL_DETAIL_INVENT, 
				  (arbitrary_info_t *)&rps_invent);
        if (rc != GRAPH_SUCCESS)  {
                return;
        }                                                                      

	if (rps_state == 0 ) 
		rps_invent->ir_gen.ig_flag = 0;
	else 
		rps_invent->ir_gen.ig_flag = INVENT_ENABLED;
}

void
klhwg_add_xbox_rps(vertex_hdl_t node_vertex, cnodeid_t cnode, int flag)
{
	vertex_hdl_t myrpsv;
	invent_rpsinfo_t *rps_invent;
	int rc;

        if(cnode == CNODEID_NONE)
                return;                                                        
	
	GRPRINTF(("klhwg_add_rps: adding %s to vertex 0x%x\n", 
		  EDGE_LBL_XBOX_RPS, node_vertex));

	rc = hwgraph_path_add(node_vertex, EDGE_LBL_XBOX_RPS, &myrpsv);
	if (rc != GRAPH_SUCCESS)
		return;

	device_master_set(myrpsv, node_vertex);

        rps_invent = (invent_rpsinfo_t *)
            klhwg_invent_alloc(cnode, INV_RPS, sizeof(invent_rpsinfo_t));

        if (!rps_invent)
            return;

	rps_invent->ir_xbox = 1;	/* xbox RPS */

        if (flag)
            rps_invent->ir_gen.ig_flag = INVENT_ENABLED;
        else
            rps_invent->ir_gen.ig_flag = 0x0;                                  

        hwgraph_info_add_LBL(myrpsv, INFO_LBL_DETAIL_INVENT,
                             (arbitrary_info_t) rps_invent);
        hwgraph_info_export_LBL(myrpsv, INFO_LBL_DETAIL_INVENT,
                                sizeof(invent_rpsinfo_t));                     
	
}

/*
 * klhwg_update_xbox_rps gets invoked when the xbox system controller
 * polls the status register and discovers that the power supply has 
 * lost/regained the redundancy.
 * It's responsible for updating the Hardware graph information.
 *	rps_state = 0 -> if the rps lost the redundancy
 *		  = 1 -> If it is redundant. 
 */
void 
klhwg_update_xbox_rps(cnodeid_t cnode, int rps_state)
{
        vertex_hdl_t node_vertex;
        vertex_hdl_t rpsv;
        invent_rpsinfo_t *rps_invent;                                          
        int rc;
        if(cnode == CNODEID_NONE)
                return;                                                        

        node_vertex = cnodeid_to_vertex(cnode);                                
	rc = hwgraph_edge_get(node_vertex, EDGE_LBL_XBOX_RPS, &rpsv);
        if (rc != GRAPH_SUCCESS)  {
		return;
	}

	rc = hwgraph_info_get_LBL(rpsv, INFO_LBL_DETAIL_INVENT, 
				  (arbitrary_info_t *)&rps_invent);
        if (rc != GRAPH_SUCCESS)  {
                return;
        }                                                                      

	if (rps_state == 0 ) 
		rps_invent->ir_gen.ig_flag = 0;
	else 
		rps_invent->ir_gen.ig_flag = INVENT_ENABLED;
}


/* ARGSUSED */
void
klhwg_add_mem(vertex_hdl_t node_vertex, cnodeid_t cnode, klmembnk_t *mem, slotid_t slot, nasid_t mem_nasid)
{
	vertex_hdl_t	mymemv;
	int rc;

	GRPRINTF(("klhwg_add_mem: adding %s to vertex 0x%x\n", EDGE_LBL_HUB,
			node_vertex));
	rc = hwgraph_path_add(node_vertex, EDGE_LBL_MEMORY, &mymemv);

	if (rc != GRAPH_SUCCESS)
	    return;

	device_master_set(mymemv, node_vertex);

	klhwg_mem_invent_info(mymemv, cnode, mem, slot, mem_nasid);
}


void
klhwg_add_xbow(cnodeid_t cnode, nasid_t nasid)
{
	lboard_t *brd;
	klxbow_t *xbow_p;
	nasid_t hub_nasid;
	cnodeid_t hub_cnode;
	int widgetnum;
	vertex_hdl_t xbow_v, hubv;
	/*REFERENCED*/
	graph_error_t err;

	if ((brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), 
			       KLTYPE_MIDPLANE8)) == NULL)
	    return;

	if (KL_CONFIG_DUPLICATE_BOARD(brd))
	    return;

	GRPRINTF(("klhwg_add_xbow: adding cnode %d nasid %d xbow edges\n",
			cnode, nasid));

	if ((xbow_p = (klxbow_t *)find_component(brd, NULL, KLSTRUCT_XBOW))
	    == NULL)
	    return;

	err = hwgraph_vertex_create(&xbow_v);
	ASSERT(err == GRAPH_SUCCESS);
	xswitch_vertex_init(xbow_v);

	for (widgetnum = HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX; widgetnum++) {
		if (!XBOW_PORT_TYPE_HUB(xbow_p, widgetnum)) 
		    continue;
		hub_nasid = XBOW_PORT_NASID(xbow_p, widgetnum);
		if (hub_nasid == INVALID_NASID) {
			cmn_err(CE_WARN, "hub widget %d, skipping xbow graph\n");
			continue;
		}
		
		(void)hwgraph_info_add_LBL(xbow_v, INFO_LBL_XSWITCH_ID,
			(arbitrary_info_t)hub_slot_to_crossbow(NODEPDA(cnode)->slotdesc));
		hub_cnode = NASID_TO_COMPACT_NODEID(hub_nasid);

		if (is_specified(arg_maxnodes) && hub_cnode == INVALID_CNODEID) {
			continue;
		}
			
		hubv = cnodeid_to_vertex(hub_cnode);

		NODEPDA(hub_cnode)->xbow_vhdl = xbow_v;

		/*
		 * XXX - This won't work is we ever hook up two hubs
		 * by crosstown through a crossbow.
		 */
		if (hub_nasid != nasid) {
			NODEPDA(hub_cnode)->xbow_peer = nasid;
			NODEPDA(NASID_TO_COMPACT_NODEID(nasid))->xbow_peer =
				hub_nasid;
		}

		GRPRINTF(("klhwg_add_xbow: adding port nasid %d %s to vertex 0x%x\n",
			hub_nasid, EDGE_LBL_XTALK, hubv));
		err = hwgraph_edge_add(hubv, xbow_v, EDGE_LBL_XTALK);
		if (err != GRAPH_SUCCESS) {
			if (err == GRAPH_DUP)
				cmn_err(CE_WARN, "klhwg_add_xbow: Check for working routers and router links!");

			cmn_err(CE_GRPANIC, "klhwg_add_xbow: Failed to add edge: vertex 0x%x (%v) to vertex 0x%x (%v), error %d\n",
				hubv, hubv, xbow_v, xbow_v, err);
		}
	}
}


/* ARGSUSED */
void
klhwg_add_node(vertex_hdl_t hwgraph_root, cnodeid_t cnode, gda_t *gdap)
{
	nasid_t	nasid, mem_nasid;
	lboard_t *brd;
	klmembnk_t *mem;
	klcpu_t	*cpu;
	klhub_t *hub;
	vertex_hdl_t node_vertex;
	char path_buffer[100];
	int rv;
	char *s;
	int boardnum = 0; 
#if defined (SABLE)

	if (fake_prom) {
	    cmn_err(CE_NOTE, "Fixme: klhwg_add_node %d node\n", cnode);
	    return;
	}
#endif	
	nasid = COMPACT_TO_NASID_NODEID(cnode);
	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_IP27);
	ASSERT(brd);

	do {
	/* Generate a hardware graph path for this board. */
	board_to_path(brd, path_buffer);

	GRPRINTF(("klhwg_add_node: adding %s to vertex 0x%x\n",
		path_buffer, hwgraph_root));
	rv = hwgraph_path_add(hwgraph_root, path_buffer, &node_vertex);

	if (rv != GRAPH_SUCCESS)
		cmn_err(CE_PANIC, "Node vertex creation failed.  Path == %s",
			path_buffer);

	hub = (klhub_t *)find_first_component(brd, KLSTRUCT_HUB);
	ASSERT(hub);
	if(hub->hub_info.flags & KLINFO_ENABLE)
		boardnum = 0;
	else
		boardnum = 1;
	
	if(!boardnum) {
	mark_nodevertex_as_node(node_vertex, cnode + boardnum*numnodes);

	s = dev_to_name(node_vertex, path_buffer, MAXDEVNAME);
	NODEPDA(cnode)->hwg_node_name = kmem_alloc(strlen(s) + 1, KM_NOSLEEP);
	ASSERT_ALWAYS(NODEPDA(cnode)->hwg_node_name != NULL);
	strcpy(NODEPDA(cnode)->hwg_node_name, s);

	hubinfo_set(node_vertex, NODEPDA(cnode)->pdinfo);

	/* Set up node board's slot */
	NODEPDA(cnode)->slotdesc = brd->brd_slot;

	/* Set up the module we're in */
	NODEPDA(cnode)->module_id = brd->brd_module;
	NODEPDA(cnode)->module = module_lookup(brd->brd_module);
	}
/*
 * XXX Also create a link between the root "node" vertex and this node.  Name
 * it with the pnodeid
 */
	/* Get the memory bank structure */
	mem = (klmembnk_t *)find_first_component(brd, KLSTRUCT_MEMBNK);
	ASSERT(mem);
	if(boardnum == 0)
		mem_nasid =  nasid;
	else
		mem_nasid = ((klinfo_t *)hub)->physid;
	/* Add node memory to the graph */
	klhwg_add_mem(node_vertex, cnode, mem, brd->brd_slot,mem_nasid);

	/* Get the first CPU structure */
	cpu = (klcpu_t *)find_first_component(brd, KLSTRUCT_CPU);

	while (cpu) {
		/* Add each CPU */
		if (KLCONFIG_INFO_ENABLED(&cpu->cpu_info))
		  klhwg_add_cpu(node_vertex, cnode, cpu);
		else 
		  klhwg_add_disabled_cpu(node_vertex, cnode, cpu, brd->brd_slot);

		cpu = (klcpu_t *)
		    find_component(brd, (klinfo_t *)cpu, KLSTRUCT_CPU);
	} /* while */

	if(!boardnum)
	klhwg_add_hub(node_vertex, hub, cnode);
	
	brd = KLCF_NEXT(brd);
	if(brd)
		brd = find_lboard(brd,KLTYPE_IP27);
	else
		break;
	}while(brd);
}

/* ARGSUSED */
void
klhwg_add_all_routers(vertex_hdl_t hwgraph_root)
{
	nasid_t nasid;
	cnodeid_t cnode;
	lboard_t *brd;
	vertex_hdl_t node_vertex;
	char path_buffer[100];
        klrou_t *rou;
        lboard_t *node_brd;
        klhub_t *hub;
        int i, pn;
        char *nic_info, *c;
        char sname[32];
	int rv;

	for (cnode = 0; cnode < maxnodes; cnode++) {
#if FAKE_ROUTER_SLOT_WAR
		slotid_t slot;
		slotid_t fake_router_slot;
		lboard_t *hub_brd;
#endif

		nasid = COMPACT_TO_NASID_NODEID(cnode);

		GRPRINTF(("klhwg_add_all_routers: adding router on cnode %d\n",
			cnode));

		brd = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid),
				KLTYPE_ROUTER);

		if (!brd)
			/* No routers stored in this node's memory */
			continue;

		do {
			ASSERT(brd);
			GRPRINTF(("Router board struct is %x\n", brd));

			/* Don't add duplicate boards. */
			if (brd->brd_flags & DUPLICATE_BOARD)
				continue;

#ifdef FAKE_ROUTER_SLOT_WAR
			hub_brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),
					KLTYPE_IP27);
			ASSERT(hub_brd);

			slot = hub_brd->brd_slot;

			fake_router_slot = (((SLOTNUM_GETSLOT(slot) - 1) / 2)
					    + 1) |
				   SLOTNUM_ROUTER_CLASS;

			/*
			 * The routers aren't all jumpered correctly for this
			 * yet.  Later, change this to an ASSERT(brd->brd_slot
			 * == fake_router_slot
			 */
#ifndef BAD_ROUTER_JUMPER_WAR
			if (SLOTNUM_GETCLASS(brd->brd_slot) ==
						SLOTNUM_INVALID_CLASS) {
				brd->brd_slot = fake_router_slot;
				cmn_err(CE_WARN, "Improperly jumpered router: module %d", brd->brd_module);

			} else {
				GRPRINTF(("Router found at module %d, slot r%d\n", brd->brd_module, SLOTNUM_GETSLOT(brd->brd_slot)));
			}
#else
			brd->brd_slot = fake_router_slot;
			GRPRINTF(("Router found at module %d, slot r%d\n",
				brd->brd_module, SLOTNUM_GETSLOT(brd->brd_slot)));
#endif
#endif
			nasid = COMPACT_TO_NASID_NODEID(cnode);
                        /* with IP31 boards, routers with part numbers
                         * 030-0841-002 and down do not work well. We
                         * need to warn users about that */
                        rou = (klrou_t *)find_component(brd, NULL, KLSTRUCT_ROU);
                        pn = 3;
                        if(rou) {
                                if(rou->rou_mfg_nic != NULL)
                                {
                                        nic_info = (char *)NODE_OFFSET_TO_K1(nasid, rou->rou_mfg_nic);
                                        if(nic_info && (c = strstr(nic_info,"Part")))
                                        {
                                                if(strstr(c,"030-0841")) /* current router's part number */
                                                {
                                                        c+= 14;
                                                        pn = atoi(c);
                                                }
                                        }
                                        else {
                                                cmn_err(CE_WARN, "Cannot read router nic in module %d, slot r%d\n", brd->brd_module, SLOTNUM_GETSLOT(brd->brd_slot));
                                        }
                                }
                                for(i=1; i<=6; i++)
                                {
                                        if(rou->rou_port[i].port_nasid != INVALID_NASID)
                                        {
                                                node_brd = (lboard_t *)NODE_OFFSET_TO_K1(rou->rou_port[i].port_nasid, rou->rou_port[i].port_offset);
                                                if(!node_brd || node_brd->brd_type != KLTYPE_IP27)
                                                        continue;
                                                hub = (klhub_t *)find_first_component(node_brd, KLSTRUCT_HUB);
                                                if (hub && (KLCONFIG_INFO_ENABLED(&hub->hub_info)))
                                                {
                                                        if(hub->hub_speed == 100 * 1000000 && pn <= 2)
                                                        {
                                                                get_slotname(brd->brd_slot, sname);
                                                                cmn_err(CE_WARN, "downrev router board in module %d slot %s\n", brd->brd_module, sname);
                                                        }

                                                }
                                        }
                                }

			}
			GRPRINTF(("Router 0x%x module number is %d\n", brd, brd->brd_module));
			/* Generate a hardware graph path for this board. */
			board_to_path(brd, path_buffer);

			GRPRINTF(("Router path is %s\n", path_buffer));

			/* Add the router */
			GRPRINTF(("klhwg_add_all_routers: adding %s to vertex 0x%x\n",
				path_buffer, hwgraph_root));
			rv = hwgraph_path_add(hwgraph_root, path_buffer, &node_vertex);

			if (rv != GRAPH_SUCCESS)
				cmn_err(CE_PANIC, "Router vertex creation "
						  "failed.  Path == %s",
					path_buffer);

			GRPRINTF(("klhwg_add_all_routers: get next board from 0x%x\n",
					brd));
		/* Find the rest of the routers stored on this node. */
		} while (brd = find_lboard_class(KLCF_NEXT(brd),
			 KLTYPE_ROUTER));

		GRPRINTF(("klhwg_add_all_routers: Done.\n"));
	}

}

/* ARGSUSED */
void
klhwg_connect_one_router(vertex_hdl_t hwgraph_root, lboard_t *brd,
			 cnodeid_t cnode, nasid_t nasid)
{
	klrou_t *router;
	char path_buffer[50];
	char dest_path[50];
	vertex_hdl_t router_hndl;
	vertex_hdl_t dest_hndl;
	int rc;
	int port;
	lboard_t *dest_brd;

	GRPRINTF(("klhwg_connect_one_router: Connecting router on cnode %d\n",
			cnode));

	/* Don't add duplicate boards. */
	if (brd->brd_flags & DUPLICATE_BOARD) {
		GRPRINTF(("klhwg_connect_one_router: Duplicate router 0x%x on cnode %d\n",
			brd, cnode));
		return;
	}

	/* Generate a hardware graph path for this board. */
	board_to_path(brd, path_buffer);

	rc = hwgraph_traverse(hwgraph_root, path_buffer, &router_hndl);

	if (rc != GRAPH_SUCCESS && is_specified(arg_maxnodes))
			return;

	if (rc != GRAPH_SUCCESS)
		cmn_err(CE_WARN, "Can't find router: %s", path_buffer);

	/* We don't know what to do with multiple router components */
	if (brd->brd_numcompts != 1) {
		cmn_err(CE_PANIC,
			"klhwg_connect_one_router: %d cmpts on router\n",
			brd->brd_numcompts);
		return;
	}


	/* Convert component 0 to klrou_t ptr */
	router = (klrou_t *)NODE_OFFSET_TO_K1(NASID_GET(brd),
					      brd->brd_compts[0]);

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		/* See if the port's active */
		if (router->rou_port[port].port_nasid == INVALID_NASID) {
			GRPRINTF(("klhwg_connect_one_router: port %d inactive.\n",
				 port));
			continue;
		}
		if (is_specified(arg_maxnodes) && NASID_TO_COMPACT_NODEID(router->rou_port[port].port_nasid) 
		    == INVALID_CNODEID) {
			continue;
		}

		dest_brd = (lboard_t *)NODE_OFFSET_TO_K1(
				router->rou_port[port].port_nasid,
				router->rou_port[port].port_offset);

		/* Generate a hardware graph path for this board. */
		board_to_path(dest_brd, dest_path);

		rc = hwgraph_traverse(hwgraph_root, dest_path, &dest_hndl);

		if (rc != GRAPH_SUCCESS) {
			if (is_specified(arg_maxnodes) && KL_CONFIG_DUPLICATE_BOARD(dest_brd))
				continue;
			cmn_err(CE_PANIC, "Can't find router: %s", dest_path);
		}
		GRPRINTF(("klhwg_connect_one_router: Link from %s/%d to %s\n",
			  path_buffer, port, dest_path));

		sprintf(dest_path, "%d", port);

		rc = hwgraph_edge_add(router_hndl, dest_hndl, dest_path);

		if (rc == GRAPH_DUP) {
			GRPRINTF(("Skipping port %d. nasid %d %s/%s\n",
				  port, router->rou_port[port].port_nasid,
				  path_buffer, dest_path));
			continue;
		}

		if (rc != GRAPH_SUCCESS && !is_specified(arg_maxnodes))
			cmn_err(CE_GRPANIC, "Can't create edge: %s/%s to vertex 0x%x error 0x%x\n",
				path_buffer, dest_path, dest_hndl, rc);
		
	}
}


void
klhwg_connect_routers(vertex_hdl_t hwgraph_root)
{
	nasid_t nasid;
	cnodeid_t cnode;
	lboard_t *brd;

	for (cnode = 0; cnode < maxnodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);

		GRPRINTF(("klhwg_connect_routers: Connecting routers on cnode %d\n",
			cnode));

		brd = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid),
				KLTYPE_ROUTER);

		if (!brd)
			continue;

		do {

			nasid = COMPACT_TO_NASID_NODEID(cnode);

			klhwg_connect_one_router(hwgraph_root, brd,
						 cnode, nasid);

		/* Find the rest of the routers stored on this node. */
		} while (brd = find_lboard_class(KLCF_NEXT(brd), KLTYPE_ROUTER));
	}
}


void
klhwg_connect_hubs(vertex_hdl_t hwgraph_root)
{
	nasid_t nasid;
	cnodeid_t cnode;
	lboard_t *brd;
	klhub_t *hub;
	lboard_t *dest_brd;
	vertex_hdl_t hub_hndl;
	vertex_hdl_t dest_hndl;
	char path_buffer[50];
	char dest_path[50];
	int rc;

#if defined (SABLE)
	if (fake_prom) {
	    cmn_err(CE_NOTE, "fixme: klhwg_connect_hubs");
	    return;
	}
#endif /* SABLE */
	for (cnode = 0; cnode < maxnodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);

		GRPRINTF(("klhwg_connect_routers: Connecting routers on cnode %d\n",
			cnode));

		brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),
				KLTYPE_IP27);
		ASSERT(brd);

		hub = (klhub_t *)find_first_component(brd, KLSTRUCT_HUB);
		ASSERT(hub);

		/* See if the port's active */
		if (hub->hub_port.port_nasid == INVALID_NASID) {
			GRPRINTF(("klhwg_connect_hubs: port inactive.\n"));
			continue;
		}

		if (is_specified(arg_maxnodes) && NASID_TO_COMPACT_NODEID(hub->hub_port.port_nasid) == INVALID_CNODEID)
			continue;

		/* Generate a hardware graph path for this board. */
		board_to_path(brd, path_buffer);

		GRPRINTF(("klhwg_connect_hubs: Hub path is %s.\n", path_buffer));
		rc = hwgraph_traverse(hwgraph_root, path_buffer, &hub_hndl);

		if (rc != GRAPH_SUCCESS)
			cmn_err(CE_WARN, "Can't find hub: %s", path_buffer);

		dest_brd = (lboard_t *)NODE_OFFSET_TO_K1(
				hub->hub_port.port_nasid,
				hub->hub_port.port_offset);

		/* Generate a hardware graph path for this board. */
		board_to_path(dest_brd, dest_path);

		rc = hwgraph_traverse(hwgraph_root, dest_path, &dest_hndl);

		if (rc != GRAPH_SUCCESS) {
			if (is_specified(arg_maxnodes) && KL_CONFIG_DUPLICATE_BOARD(dest_brd))
				continue;
			cmn_err(CE_PANIC, "Can't find board: %s", dest_path);
		} else {
		

			GRPRINTF(("klhwg_connect_hubs: Link from %s to %s.\n",
			  path_buffer, dest_path));

			rc = hwgraph_edge_add(hub_hndl, dest_hndl, EDGE_LBL_INTERCONNECT);

			if (rc != GRAPH_SUCCESS)
				cmn_err(CE_GRPANIC, "Can't create edge: %s/%s to vertex 0x%x, error 0x%x\n",
				path_buffer, dest_path, dest_hndl, rc);

		}
	}
}

/* Store the pci/vme disabled board information as extended administrative
 * hints which can later be used by the drivers using the device/driver
 * admin interface. 
 */
void
klhwg_device_disable_hints_add(void)
{
	cnodeid_t	cnode; 		/* node we are looking at */
	nasid_t		nasid;		/* nasid of the node */
	lboard_t	*board;		/* board we are looking at */
	int		comp_index;	/* component index */
	klinfo_t	*component;	/* component in the board we are
					 * looking at 
					 */
	char		device_name[MAXDEVNAME];
	
	device_admin_table_init();
	for(cnode = 0; cnode < numnodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);
		board = (lboard_t *)KL_CONFIG_INFO(nasid);
		/* Check out all the board info stored  on a node */
		while(board) {
			/* No need to look at duplicate boards or non-io 
			 * boards
			 */
			if (KL_CONFIG_DUPLICATE_BOARD(board) ||
			    KLCLASS(board->brd_type) != KLCLASS_IO) {
				board = KLCF_NEXT(board);
				continue;
			}
			/* Check out all the components of a board */
			for (comp_index = 0; 
			     comp_index < KLCF_NUM_COMPS(board);
			     comp_index++) {
				component = KLCF_COMP(board,comp_index);
				/* If the component is enabled move on to
				 * the next component
				 */
				if (KLCONFIG_INFO_ENABLED(component))
					continue;
				/* NOTE : Since the prom only supports
				 * the disabling of pci devices the following
				 * piece of code makes sense. 
				 * Make sure that this assumption is valid
				 */
				/* This component is disabled. Store this
				 * hint in the extended device admin table
				 */
				/* Get the canonical name of the pci device */
				device_component_canonical_name_get(board,
							    component,
							    device_name);
				device_admin_table_update(device_name,
							  ADMIN_LBL_DISABLED,
							  "yes");
#ifdef DEBUG
				printf("%s DISABLED\n",device_name);
#endif				
			}
			/* go to the next board info stored on this 
			 * node 
			 */
			board = KLCF_NEXT(board);
		}
	}
}
/*
 * Print out the list of nasids in a user readable hwgraph format.
 * This is helpful in identifying which the nodes with premium
 * directories and those with standard directories.
 */
void
klhwg_dimm_flavor_info_print(int num_nodes,nasid_t nodes[],int premium)
{
	int 		i;
	nasid_t		last_printed_node = INVALID_NASID;

	
	/* This loop prints out the sequence of nasids that 
	 * from the given list of nasids.
	 * If there is a consecutive sequence of nasids
	 * "n , n+1 , ...., m" then "n-m" gets printed
	 * to decrease the amount of output.
	 */
	for (i = 0 ; i < num_nodes ; i++) {
		/* Print the output for the first node */
		if (i == 0) {
			cmn_err(CE_CONT,"%s dimms on nasids : %d",
				premium ? "Premium" : "Standard",
				nodes[i]);
			last_printed_node = nodes[0];
		} else {
			if (nodes[i] == nodes[i-1] + 1 ) {
				if (i == num_nodes - 1)
					cmn_err(CE_CONT,"-%d",nodes[i]);
			} else {
				if (last_printed_node != nodes[i-1])
					cmn_err(CE_CONT,"-%d,%d",nodes[i-1],
						nodes[i]);
				else
					cmn_err(CE_CONT,",%d",nodes[i]);
				last_printed_node = nodes[i];
			}
			
		}
	}
	cmn_err(CE_CONT,"\n");
	/* Now print a series of hwgraph names for the above list of
	 * nasids each separated by a newline
	 */
	for (i = 0 ; i < num_nodes ; i++) {
		char		path_buffer[100];
		lboard_t 	*brd;

		brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nodes[i]), 
				  KLTYPE_IP27);
		board_to_path(brd,path_buffer);
		cmn_err(CE_CONT,"\t/hw/%s\n",path_buffer);
	}


}
#define STANDARD_FLAVOR_DIMM_NODE_MAX 16	/* Max. # nodes allowed 
						 * in the case of
						 * standard dimms
						 */

/* Go through all the klconfig information in the system and disable 
 * premium directory dimms if they are not present in all the modules.
 */
char
klhwg_premium_std_dimm_mix_check(void)
{
	cnodeid_t		cnode;
#ifdef DEBUG
	gda_t			*gdap = GDA;
#endif
	nasid_t			nasid;
	lboard_t		*brd = NULL;	
	klmembnk_t		*mem;
	static char 		dimm_flavor_premium = 0xff; /* invalid value */
	cnodeid_t		premium_nodes[MAX_NASIDS];
	cnodeid_t		std_nodes[MAX_NASIDS];
	int			num_premium_nodes = 0,num_std_nodes = 0;
	
	/* If the flavor of all the dimms has already been decided
	 * use that instead of going through the whole process
	 * again.
	 */
	if (dimm_flavor_premium != 0xff)
		return(dimm_flavor_premium);

	for(cnode = 0; cnode < numnodes ; cnode++) {

		/* Get the IP27 board info for this node */
		ASSERT(gdap->g_nasidtable[cnode] != INVALID_NASID);
		nasid = COMPACT_TO_NASID_NODEID(cnode);
		brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), 
				  KLTYPE_IP27);
		ASSERT(brd);

		/* Get the memory bank structure */
		mem = (klmembnk_t *)find_first_component(brd, KLSTRUCT_MEMBNK);
		ASSERT(mem);

		/* Remember that we have seen at least one node
		 * either with a particular flavor of dimms
		 */
		if(mem->membnk_info.struct_version >= 2)
		{
		if (KLCONFIG_MEMBNK_PREMIUM(mem, MD_MEM_BANKS)) {
			premium_nodes[num_premium_nodes++] = nasid;
		} else {
			std_nodes[num_std_nodes++] = nasid;
		}
		}
		else
		{
		if (KLCONFIG_MEMBNK_PREMIUM(mem, 0)) {
			premium_nodes[num_premium_nodes++] = nasid;
		} else {
			std_nodes[num_std_nodes++] = nasid;
		}
		}
			
	}

	
	if (num_std_nodes && num_premium_nodes) {
		/* If a mixture of premium & standard dimms are found
		 * then all the premium dimms MUST be disabled on
		 * systems with less than 16 nodes. On systems with 
		 * more than 16 nodes we should panic.
		 */
		if (numnodes > STANDARD_FLAVOR_DIMM_NODE_MAX ) {
			klhwg_dimm_flavor_info_print(num_std_nodes,
						     std_nodes,0);
			cmn_err(CE_PANIC,"Mixture of premium & std dimms\n");
		} else {
			/* If a mixture of dimm flavors is found 
			 * then set all the dimms to be standard
			 * for all software purposes.
			 */
			cmn_err(CE_WARN|CE_RUNNINGPOOR,
				"Mixture of premium & std dimms\n");
			klhwg_dimm_flavor_info_print(num_std_nodes,
						     std_nodes,0);
			klhwg_dimm_flavor_info_print(num_premium_nodes,
						     premium_nodes,1);
			dimm_flavor_premium = 0;
		}

	} else if (num_premium_nodes)
		/* If only nodes with premium dimms were found 
		 * remember the fact that we are in premium directory
		 * mode.
		 */
		dimm_flavor_premium = 1;
	else 	/* We have only standard dimms */
		dimm_flavor_premium = 0;
	return(dimm_flavor_premium);
}

void
klhwg_add_all_modules(vertex_hdl_t hwgraph_root)
{
	cmoduleid_t	cm;
	char		name[128];
	vertex_hdl_t	vhdl;
	int		rc;

	/* Add devices under each module */

	for (cm = 0; cm < nummodules; cm++) {
		/* Use module as module vertex fastinfo */

		sprintf(name, EDGE_LBL_MODULE "/%d", modules[cm]->id);

		rc = hwgraph_path_add(hwgraph_root, name, &vhdl);
		ASSERT(rc == GRAPH_SUCCESS);
		rc = rc;

		hwgraph_fastinfo_set(vhdl, (arbitrary_info_t) modules[cm]);

		/* Add ELSC */

		sprintf(name,
			EDGE_LBL_MODULE "/%d/" EDGE_LBL_ELSC,
			modules[cm]->id);

		rc = hwgraph_path_add(hwgraph_root, name, &vhdl);
		ASSERT_ALWAYS(rc == GRAPH_SUCCESS); 
		rc = rc;

		hwgraph_info_add_LBL(vhdl,
				     INFO_LBL_ELSC,
				     (arbitrary_info_t) (__psint_t) 1);

#if defined (SN0)
		sn0drv_attach(vhdl);
#endif
	}
}

void
klhwg_add_all_nodes(vertex_hdl_t hwgraph_root)
{
	gda_t		*gdap = GDA;
	cnodeid_t	cnode;

	for (cnode = 0; cnode < numnodes; cnode++) {
		ASSERT(gdap->g_nasidtable[cnode] != INVALID_NASID);
		klhwg_add_node(hwgraph_root, cnode, gdap);
	}

	for (cnode = 0; cnode < numnodes; cnode++) {
		ASSERT(gdap->g_nasidtable[cnode] != INVALID_NASID);

		klhwg_add_xbow(cnode, gdap->g_nasidtable[cnode]);
	}

	/*
	 * As for router hardware inventory information, we set this
	 * up in router.c. 
	 */
	
	klhwg_add_all_routers(hwgraph_root);
	klhwg_connect_routers(hwgraph_root);
	klhwg_connect_hubs(hwgraph_root);

	/* Assign guardian nodes to each of the
	 * routers in the system.
	 */
	router_guardians_set(hwgraph_root);
	/* Go through the entire system's klconfig
	 * to figure out which pci components have been disabled
	 */
	klhwg_device_disable_hints_add();
}
