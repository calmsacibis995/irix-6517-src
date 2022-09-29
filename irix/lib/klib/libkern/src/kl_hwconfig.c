#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/libkern/src/RCS/kl_hwconfig.c,v 1.8 1999/10/19 15:40:09 lucc Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <dslib.h>
#include <sys/invent.h>
#include <fcntl.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/sbd.h>
#include <sys/gfx.h>
#include <sys/kona.h>
#include <sys/kona_diag.h>
#include <klib/klib.h>
 
/* Global variables
 */
static evcfginfo_t *evcfgip = 0;

/* Local function prototypes
 */
static void add_board_name(hwconfig_t *, hw_component_t *);

/*
 * alloc_hwcomponent() -- Allocate and initialize a hw_component_s struct
 */
static hw_component_t *
alloc_hwcomponent(int category, int type, int flags)
{
	int aflag = ALLOCFLG(flags);	/* Block allocation flag */
	hw_component_t *cmp;

	cmp = (hw_component_t *)kl_alloc_block(sizeof(hw_component_t), aflag);
	if (!cmp) {
		return((hw_component_t *)NULL);
	}
	cmp->c_category = category; 
	cmp->c_type = type;

	/* the flags parameter may contain the STRINGTAB_FLG flag, which 
	 * would indicate that any strings have been allocated from a
	 * string_table and should not be freed (via kl_free_block()) when
	 * the hwcomponent is torn down.
	 */
	cmp->c_flags = flags;

	switch (category) {

		case HW_SYSBOARD :
			break;

		case HW_BOARD :
			switch (type) {

				case BD_IP19 :
				case BD_IP21 :
				case BD_IP25 :
					cmp->c_data = 
						kl_alloc_block(sizeof(processor_data_t), aflag);
					break;

				case BD_IP30 :
					/* XXX -- does the node_data_t make sense for the IP30
					 * system board?
					 */
					cmp->c_data = kl_alloc_block(sizeof(node_data_t), aflag);
					break;

				case BD_NODE :
				case BD_IP31 :
					cmp->c_data = kl_alloc_block(sizeof(node_data_t), aflag);
					break;

				case BD_MC3 :
					break;

				default :
					break;
			}
			break;

		case HW_CPU :
			cmp->c_data = kl_alloc_block(sizeof(cpu_data_t), aflag);
			break;

		case HW_MEMBANK :
			cmp->c_data = kl_alloc_block(sizeof(membank_data_t), aflag);
			break;

		case HW_IOA :
			cmp->c_data = kl_alloc_block(sizeof(ioa_data_t), aflag);
			break;

		case HW_INVENTORY:
		case HW_PARTITION :
		case HW_MODULE :
		case HW_POWER_SUPPLY:
		case HW_PCI:
			break;

		case HW_SYSTEM :
			cmp->c_next = cmp->c_prev = cmp;
			return(cmp);

		default :
			kl_free_block(cmp);
			return((hw_component_t *)NULL);
	}
	cmp->c_enabled = -1;
	cmp->c_next = cmp->c_prev = cmp;
	return(cmp);
}

/*
 * kl_free_evcfginfo()
 */
void
kl_free_evcfginfo()
{
	if (evcfgip) {
		kl_free_block(evcfgip);
		evcfgip = 0;
	}
}

/*
 * kl_get_hwconfig()
 */
int
kl_get_hwconfig(hwconfig_t *hcp, int flag)
{
	invent_rec_t *invent_table;

	hcp->h_hwcmp_root = alloc_hwcomponent(HW_SYSTEM, K_IP, hcp->h_flags);
	hcp->h_sys_type = K_IP; 

	switch (K_IP) {

		case 27:
			get_IP27_config(hcp);
			break;

		case 19:
		case 21:
		case 25:
			get_EVEREST_config(hcp);
			kl_free_evcfginfo();
			break;

		case 30:
			get_IP30_config(hcp);
			break;

		case 32:
			get_IP32_config(hcp);
			break;

		default:
			return(1);
	}
	kl_update_hwconfig(hcp);

	if (flag) {
		if (invent_table = get_invent_table(0, flag, K_TEMP)) {
			if (K_IP == 32) {
				walk_IP32_invent_table(hcp, invent_table, flag);
			}
			else {
				walk_invent_table(hcp, invent_table, flag);
			}
			free_invent_table(invent_table);
		}
		else {
			return(1);
		}
	}

	get_graphics_info(hcp);
	return(0);
}

/*
 * get_EVEREST_config()
 */
static int
get_EVEREST_config(hwconfig_t *hcp)
{
	int i, node_vhndl;
	k_ptr_t gvp, gep;
	hw_component_t *hwcp;

	if (!IS_EVEREST) {
		/* This is not an EVEREST class system!
		 */
		return(1);
	}

	evcfgip = (evcfginfo_t*)kl_alloc_block(sizeof(evcfginfo_t), HWAFLG(hcp));
	kl_get_block(EV_CFGINFO_ADDR, sizeof(evcfginfo_t), evcfgip, "evcfginfo_t");

	/* Get the first vertex in the hwgraph
	 */
	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	if (KL_ERROR) {
		return(1);
	}
	kl_get_graph_vertex_s(1, 0, gvp, 0);
	if (KL_ERROR) {
		kl_free_block(gvp);
		return(1);
	}

	/* Get the handle for the vertex containing the "node" edge
	 */
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	if (KL_ERROR) {
		kl_free_block(gvp);
		return(1);
	}
	if (!kl_vertex_edge_name(gvp, "node", gep)) {
		kl_free_block(gvp);
		kl_free_block(gep);
		return(1);
	}
	kl_free_block(gvp);

	/* Now get the actual node vertex handle
	 */
	node_vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");
	kl_free_block(gep);

	/* Allocate a hw_component struct for the midplane node.
	 */
	hwcp = alloc_hwcomponent(HW_SYSBOARD, SBD_MIDPLANE, hcp->h_flags);

	/* Determine what hardware components are installed in the system
	 */
	hwcp->c_vhndl = node_vhndl;

	/* Add the component to the tree and fill in the rest of the info
	 */
	hwcmp_add_child(hcp->h_hwcmp_root, hwcp);
	get_EVEREST_info(hcp, hwcp);
	return(0);
}

/*
 * get_IP27_config()
 */
static int
get_IP27_config(hwconfig_t *hcp)
{
	int i, vhndl, num_modules, module_vhndl;
	k_ptr_t gvp, gep;
	char module_id[256];
	hw_component_t *cmp, *mp;

	/* Get the first vertex in the hwgraph
	 */
	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	if (KL_ERROR) {
		return(1);
	}
	kl_get_graph_vertex_s(1, 0, gvp, 0);
	if (KL_ERROR) {
		kl_free_block(gvp);
		return(1);
	}

	/* Get the handle for the vertex containing the "module" edge
	 */
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	if (!kl_vertex_edge_name(gvp, "module", gep)) {
		kl_free_block(gep);
		return(1);
	}
	module_vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");

	/* Get the vertex containing the module edges 
	 */
	kl_get_graph_vertex_s(module_vhndl, 0, gvp, 0);
	if (KL_ERROR) {
		kl_free_block(gvp);
		kl_free_block(gep);
		return(1);
	}

	/* Determine how many modules there are in the system
	 */
	num_modules = KL_INT(gvp, "graph_vertex_s", "v_num_edge");

	/* If there is only one module, we have to see if this is really
	 * an IP29 system (Origin 200). If it is, it has to be handled in 
	 * a completely different manner (even though architecturally they 
	 * are considered the same machine type).
	 */
	if (num_modules == 1) {

		/* Get the edge for module 1
		 */
		kl_get_graph_edge_s(gvp, (kaddr_t)0, 0, gep, 0);
		if (KL_ERROR) {
			kl_free_block(gvp);
			kl_free_block(gep);
			return(1);
		}
		kl_get_edge_name(gep, module_id);
		mp = alloc_hwcomponent(HW_MODULE, 0, hcp->h_flags);
		mp->c_name = get_string(hcp->h_st, module_id, HWAFLG(hcp));
		mp->c_vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");

		/* Link the module hw_component to the root
		 */
		hwcmp_add_child(hcp->h_hwcmp_root, mp);

		/* Now determine if this is an IP29 system
		 */
		kl_get_graph_vertex_s(mp->c_vhndl, 0, gvp, 0);
		if (!kl_vertex_edge_name(gvp, "slot", gep)) {
			kl_free_block(gvp);
			kl_free_block(gep);
			return(1);
		}

		/* Get the slot vertex 
		 */
		vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");
		kl_get_graph_vertex_s(vhndl, 0, gvp, 0);
		if (kl_vertex_edge_name(gvp, "MotherBoard", gep)) {

			/* This is an IP29 system
			 */
			get_IP29_info(hcp, mp);
		}
		else {
			get_module_info(hcp, mp);
		}
	}
	else {
		for (i = 0; i < num_modules; i++) {
			kl_get_graph_edge_s(gvp, (kaddr_t)i, 0, gep, 0);
			if (KL_ERROR) {
				kl_free_block(gvp);
				kl_free_block(gep);
				return(1);
			}
			kl_get_edge_name(gep, module_id);
			mp = alloc_hwcomponent(HW_MODULE, 0, hcp->h_flags);
			mp->c_name = get_string(hcp->h_st, module_id, HWAFLG(hcp));
			mp->c_vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");

			/* Gather information about all hardware components installed
			 * in the module.
			 */
			hwcmp_add_child(hcp->h_hwcmp_root, mp);
			get_module_info(hcp, mp);
		}
	}
	kl_free_block(gvp);
	kl_free_block(gep);
	return(0);
}

/* 
 * find_hwcp()
 */
hw_component_t *
find_hwcp(hw_component_t *root, int category, int type)
{
	hw_component_t *hwcp;

	if (!(hwcp = root)) {
		return((hw_component_t*)NULL);
	}
	do {
		if ((hwcp->c_category == category) && (hwcp->c_type == type)) {
			break;
		}
	} while (hwcp = next_hwcmp(hwcp));
	return(hwcp);
}

/* get_IP32_config()
 */
static int
get_IP32_config(hwconfig_t *hcp)
{
	int i, node_vhndl, nic_size;
	k_ptr_t gvp, gep, gip, nic;
	char *t, *Part, *Name, *Serial, *Revision, *Group;
	char *Capability, *Variety, *Laser;
	hw_component_t *hwcp, *next_hwcp, *parent_hwcp = (hw_component_t*)NULL;
	path_t *path;
	path_rec_t *prec, *p;
	path_chunk_t *pcp;

	if (K_IP != 32) {
		/* This is not an IP32 system!
		 */
		return(1);
	}

	/* There isn't very much hardware config data in the hwgraph for
	 * an IP32 (O2) system. We pretty much need to just allocate a
	 * hardware component struct for a motherboard and attach everything
	 * (obtained via the _invent records) to it.
	 */

	/* Allocate a hw_component struct for the motherboard.
	 */
	hwcp = alloc_hwcomponent(HW_SYSBOARD, SBD_MOTHERBOARD, hcp->h_flags);
	add_board_name(hcp, hwcp);

	/* Add the component to the root of the tree.
	 */
	hwcmp_add_child(hcp->h_hwcmp_root, hwcp);
	return(0);
}

/*
 * get_IP30_config()
 */
static int
get_IP30_config(hwconfig_t *hcp)
{
	int i, node_vhndl, nic_size;
	k_ptr_t gvp, gep, gip, nic;
	char *t, *Part, *Name, *Serial, *Revision, *Group;
	char *Capability, *Variety, *Laser;
	hw_component_t *hwcp, *next_hwcp, *parent_hwcp = (hw_component_t*)NULL;
	hw_component_t *mot_hwcp=NULL; /* to hold mot?? nic info when gfx is missing */
	path_t *path;
	path_rec_t *prec, *p;
	path_chunk_t *pcp;

	if (K_IP != 30) {
		/* This is not an IP30 system!
		 */
		return(1);
	}

	/* Get the first vertex in the hwgraph
	 */
	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	if (KL_ERROR) {
		return(1);
	}
	kl_get_graph_vertex_s(1, 0, gvp, 0);
	if (KL_ERROR) {
		kl_free_block(gvp);
		return(1);
	}

	/* Get the handle for the vertex containing the "node" edge
	 */
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	if (KL_ERROR) {
		kl_free_block(gvp);
		return(1);
	}
	if (!kl_vertex_edge_name(gvp, "node", gep)) {
		kl_free_block(gvp);
		kl_free_block(gep);
		return(1);
	}

	/* Now get the actual node vertex handle
	 */
	node_vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");

	/* Allocate a hw_component struct for the frontplane.
	 */
	hwcp = alloc_hwcomponent(HW_SYSBOARD, SBD_FRONTPLANE, hcp->h_flags);
	hwcp->c_vhndl = node_vhndl;

	/* Add the component to the root of the tree.
	 */
	hwcmp_add_child(hcp->h_hwcmp_root, hwcp);

	/* Determine what hardware components are installed in the system.
	 * The IP30 configuration is fairly static (unlike the IP27 which
	 * can have a wide variety of system configurations). We will
	 * construct the basic hw_component framework and then fill in 
	 * the details from obtained from the various NIC entries.
	 */

	/* Allocate a hw_component_s struct for the PCI Module. We will
	 * set the type=PCI_MODULE (generic).
	 */
	next_hwcp = alloc_hwcomponent(HW_PCI, PCI_MODULE, hcp->h_flags);
	next_hwcp->c_name = get_string(hcp->h_st, "PCI_MODULE", HWAFLG(hcp));
	hwcmp_add_peer(hwcp, next_hwcp);

	/* Allocate a hw_component_s struct for the IP30 system board
	 * Also, allocate a hw_component_s struct for the Processor Board. 
	 * We will set type=BD_PROCESSOR (generic) until we are able to read
	 * in the data from the NIC.
	 */
	next_hwcp = alloc_hwcomponent(HW_BOARD, BD_IP30, hcp->h_flags);
	next_hwcp->c_cmplist = 
		alloc_hwcomponent(HW_BOARD, BD_PROCESSOR, hcp->h_flags);

	/* Make sure we don't break the backward chain...
	 */
	next_hwcp->c_cmplist->c_parent = next_hwcp;
	hwcmp_add_child(hwcp, next_hwcp);

	/* Make sure the tree is updated and that all node level values
	 * are set properly. This is necessary so that the call to 
	 * next_hwcmp() works properly.
	 */
	if (kl_update_hwconfig(hcp)) {
		/* XXX - ???
		 */
		kl_free_block(gvp);
		kl_free_block(gep);
		return(1);
	}

	/* Even though some of the components contain info stored in
	 * NIC chips, an IP30 system is significantly different from an IP27
	 * that we cannot use the add_nic_info() function to gather the data.
	 * That's OK, because the possible configurations are significantly
	 * more simplified than with the larger systems...
	 */
	path = kl_init_path_table();
	if (kl_find_pathname(0, "_nic", path, hcp->h_flags, 0)) {
		kl_free_path(path);
		kl_free_block(gvp);
		kl_free_block(gep);
		return(1);
	}
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);
	if (pcp = path->pchunk) {
		for (i = 0; i <= pcp->current; i++) {
			p = pcp->path[i]->prev;
			kl_get_graph_vertex_s(p->vhndl, 0, gvp, 0);
			if (KL_ERROR) {
				continue;
			}
			/* Get the pointer to the NIC information
			 */
			if (kl_vertex_info_name(gvp, "_nic", gip)) {

				/* We have to allocate an extra byte just in case there 
				 * is more than one entry in a NIC string. The extra byte 
				 * just ensures that we don't overrun the allocated 
				 * memory in the kl_get_mfg_info() call.
				 */
				nic_size = KL_UINT(gip, "graph_info_s", "i_info_desc") + 2;
				nic = (char*)kl_alloc_block(nic_size, K_TEMP);
				if (!nic) {
					KL_SET_ERROR(KLE_BAD_NIC_DATA);
				}
				else {
					kl_get_block(kl_kaddr(gip, "graph_info_s", "i_info"),
						nic_size, nic, "NIC");
					if (*(char*)nic == 0) {
						KL_SET_ERROR(KLE_BAD_NIC_DATA);
					}
				}
				if (KL_ERROR) {
					if (nic) {
						kl_free_block(nic);
					}
					continue;
				}
				t = nic;
				do {

					t = kl_get_mfg_info(&Part, &Name, &Serial, &Revision,
							&Group, &Capability, &Variety, &Laser, t);

					/* Reset next_hwcp to the "root"
					 */
					next_hwcp = hwcp;

					if (!strcmp(Name, "IP30")) {
						/* System Module Board
						 */
						next_hwcp = find_hwcp(next_hwcp, HW_BOARD, BD_IP30);
					}
					else if (!strncmp(Name, "PM", 2)) {
						/* Processor Board
						 */
						next_hwcp = find_hwcp(next_hwcp, 
										HW_BOARD, BD_PROCESSOR);

						if (!next_hwcp) {
							next_hwcp = alloc_hwcomponent(HW_BOARD,
									BD_PROCESSOR, hcp->h_flags);
						}

						/* If we found the hw_component_s record, 
						 * we need to adjust the the c_type field 
						 * to reflect WHICH Processor Board we 
						 * actually have...otherwise, we leave the type
						 * as the generic BD_PROCESSOR.
						 */
						if (!strcmp(Name, "PM10")) {
							next_hwcp->c_type = BD_PM10;
						}
						else if (!strcmp(Name, "PM20")) {
							next_hwcp->c_type = BD_PM20;
						} 
					}
					else if (!strcmp(Name, "XTALKPCI")) {
						/* PCI_MODULE 
						 */
						if (next_hwcp = find_hwcp(next_hwcp, 
											HW_PCI, PCI_MODULE)) {
							next_hwcp->c_type = PCI_XTALKPCI;
						}
						else {
							next_hwcp = alloc_hwcomponent(HW_PCI,
									PCI_XTALKPCI, hcp->h_flags);
						}
						/* Get the vertex handle of the xtalk vertex
						 */
						next_hwcp->c_vhndl = p->prev->vhndl;
					}
					else if (!strncmp(Name, "PWR", 3)) {
						/* Power Supply
						 */
						parent_hwcp = find_hwcp(next_hwcp, 
										HW_SYSBOARD, SBD_FRONTPLANE);
						next_hwcp = alloc_hwcomponent(HW_POWER_SUPPLY, 
									PWR_S2, hcp->h_flags);
					}
					else if (!strncmp(Name, "FP", 2)) {
						/* Frontplane
						 */
						next_hwcp = find_hwcp(next_hwcp, 
									HW_SYSBOARD, SBD_FRONTPLANE);
					}
					else if (!strncmp(Name, "GM", 2)) {

						/* Copy over the starting point for our search
						 */
						parent_hwcp = next_hwcp;

						/* Allocate a hw_component_s struct for the
						 * GM?? board.
						 */
						if (!strncmp(Name, "GM10", 4)) {
							next_hwcp = alloc_hwcomponent(HW_BOARD, 
								BD_GM10, hcp->h_flags);
						}
						else if (!strncmp(Name, "GM20", 4)) {
							next_hwcp = alloc_hwcomponent(HW_BOARD, 
								BD_GM20, hcp->h_flags);
						}
						/* we we had already found a daughterboard then link it in now */
						if(mot_hwcp) {
							hwcmp_add_child(next_hwcp, mot_hwcp);
							mot_hwcp=NULL;
						}

						parent_hwcp = find_hwcp(parent_hwcp,
											HW_SYSBOARD, SBD_FRONTPLANE);
					}
					else if (!strncmp(Name, "MOT", 3)) {
						/* Graphics daughter board
						 */
						if (!strcmp(Name, "MOT10")) {
							mot_hwcp = alloc_hwcomponent(HW_BOARD, BD_MOT10, hcp->h_flags);
						}
						else if (!strcmp(Name, "MOT20")) {
							mot_hwcp = alloc_hwcomponent(HW_BOARD, BD_MOT20, hcp->h_flags);
						} 
						do {
							if ((next_hwcp->c_category == HW_BOARD) &&
										((next_hwcp->c_type == BD_GRAPHICS) ||
										 (next_hwcp->c_type == BD_GM10) ||
										 (next_hwcp->c_type == BD_GM20))) {

								/* we found the gfx component. Update and link mot to it.
								   set mot_hwcp to NULL. We will not need it */
								parent_hwcp = next_hwcp;
								next_hwcp = mot_hwcp;
								mot_hwcp = 0;
								break;
							} 
						} while (next_hwcp = next_hwcmp(next_hwcp));
						/* if GFX is not yet there just update the nic info for the MOT */
						if(!next_hwcp) next_hwcp = mot_hwcp;
	
					}
					else if (!strncmp(Name, "GALILEO", 7)) {
						/* Galileo 1.5 
						 */
						do {
							if ((next_hwcp->c_category == HW_PCI) &&
										((next_hwcp->c_type == PCI_MODULE) ||
										 (next_hwcp->c_type == PCI_XTALKPCI))) {

								/* If we found the hw_component_s record
								 * for the PCI_MODULE, we need to move 
								 * the pointer we found over to parent_hwcp 
								 * and allocate a new hw_component_s struct 
								 * for next_hwcmp.
								 */
								parent_hwcp = next_hwcp;
								next_hwcp = alloc_hwcomponent(HW_BOARD,
											BD_GALILEO15, hcp->h_flags);
								break;
							} 
						} while (next_hwcp = next_hwcmp(next_hwcp));
					}
					else if (!strcmp(Name, "PCI_ENET")) {
						/* Galileo 1.5 
						 */
						do {
							if ((next_hwcp->c_category == HW_PCI) &&
										((next_hwcp->c_type == PCI_MODULE) ||
										 (next_hwcp->c_type == PCI_XTALKPCI))) {

								/* If we found the hw_component_s record
								 * for the PCI_MODULE, we need to move 
								 * the pointer we found over to parent_hwcp 
								 * and allocate a new hw_component_s struct 
								 * for next_hwcmp.
								 */
								parent_hwcp = next_hwcp;
								next_hwcp = alloc_hwcomponent(HW_BOARD,
											BD_PCI_ENET, hcp->h_flags);
								break;
							} 
						} while (next_hwcp = next_hwcmp(next_hwcp));
					}
					else if (!strcmp(Name, "PRESENTER")) {
						parent_hwcp = find_hwcp(next_hwcp, HW_BOARD, BD_IP30);
						next_hwcp = alloc_hwcomponent(HW_BOARD, 
									BD_PRESENTER, hcp->h_flags);
					}
					else if (!strcmp(Name, "MCO")) {
						parent_hwcp = find_hwcp(next_hwcp, HW_BOARD, BD_IP30);
						next_hwcp = alloc_hwcomponent(HW_BOARD, 
									BD_MCO, hcp->h_flags);
					}
					else {
						/* Handle anything else that shows up as a NIC
						 * entry...
						 */
						parent_hwcp = find_hwcp(next_hwcp, HW_BOARD, BD_IP30);
						if (parent_hwcp) {
							next_hwcp = alloc_hwcomponent(HW_BOARD, 
									BD_UNKNOWN, hcp->h_flags);
						}
						else {
							next_hwcp = (hw_component_t *)NULL;
						}
					}			

					/* Now fill in the rest of the data obtained from 
					 * the NIC
					 */
					if (next_hwcp) {
						if (*Name) {
							next_hwcp->c_name = 
								get_string(hcp->h_st, Name, HWAFLG(hcp));
						}
						if (*Serial) {
							next_hwcp->c_serial_number = 
								get_string(hcp->h_st, Serial, HWAFLG(hcp));
						}
						if (*Part) {
							next_hwcp->c_part_number = 
								get_string(hcp->h_st, Part, HWAFLG(hcp));
						}
						if (*Revision) {
							next_hwcp->c_revision =
								get_string(hcp->h_st, Revision, HWAFLG(hcp));
						}
						/* If parent_hwcp is not-NULL, then add the next_hwcp
						 * as a child to that hw_component.
						 */
						if (parent_hwcp) {
							hwcmp_add_child(parent_hwcp, next_hwcp);
							parent_hwcp = (hw_component_t*)NULL;
						}
					}
				} while (t && *t);
			}
		}
	}
	kl_free_path(path);
	kl_free_block(gvp);
	kl_free_block(gep);

	/* Make sure the tree node levels are set properly
	 */
	kl_update_hwconfig(hcp);
	return(0);
}

/*
 * get_board_info()
 */
static hw_component_t *
get_board_info(hwconfig_t *hcp, int vhndl, char *board_name)
{
	int type, inv_class = 0, inv_type = 0;
	hw_component_t *bp;
	k_ptr_t gvp; 

	/* Make sure, if there is a vertex handle, that it is valid
	 */
	if (vhndl) {

		/* Get the vertex for this board
		 */
		if (gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP)) {
			kl_get_graph_vertex_s(vhndl, 0, gvp, 0);
		}
		if (KL_ERROR) {
			kl_free_block(gvp);
			return((hw_component_t*)NULL);
		}
		kl_free_block(gvp);
	}

	if (!strcmp(board_name, "node")) {
		type = BD_NODE;
		inv_class = INV_PROCESSOR;
		inv_type = INV_CPUBOARD;
	}
	else if (!strcmp(board_name, "router")) {
		type = BD_ROUTER;
	}
	else if (!strcmp(board_name, "metarouter")) {
		type = BD_METAROUTER;
	}
	else if (!strncmp(board_name, "basei", 5)) {
		type = BD_BASEIO;
	}
	else if (!strcmp(board_name, "hippi_serial")) {
		type = BD_HIPPI_SERIAL;
	}
	else if (!strcmp(board_name, "pci_xio")) {
		type = BD_PCI_XIO;
	}
	else if (!strcmp(board_name, "mscsi")) {
		type = BD_MSCSI;
	}
	else if (!strcmp(board_name, "ge14_4")) {
		type = BD_GE14_4;
	}
	else if (!strcmp(board_name, "ge16_4")) {
		type = BD_GE16_4;
	}
	else if (!strcmp(board_name, "fibre_channel")) {
		type = BD_FIBRE_CHANNEL;
	}
	else if (!strcmp(board_name, "cpu")) {
		switch (K_IP) {
			case 19:
				type = BD_IP19;
				break;

			case 21:
				type = BD_IP21;
				break;

			case 25:
				type = BD_IP25;
				break;

			case 30:
				type = BD_IP30;
				break;

			default:
				break;
		}
		inv_class = INV_PROCESSOR;
		inv_type = INV_CPUBOARD;
	}
	else if (!strcmp(board_name, "io4")) {
		type = BD_IO4;
	}
	else if (!strcmp(board_name, "mc3")) {
		type = BD_MC3;
	}
	else if (!strcmp(board_name, "mio")) {
		type = BD_MIO;
	}
	else if (!strcmp(board_name, "ktown")) {
		type = BD_KTOWN;
	}
	else if (!strcmp(board_name, "xtalk_pci")) {
		type = BD_XTALK_PCI;
	}
	else if (!strcmp(board_name, "quad_atm")) {
		type = BD_QUAD_ATM;
	}
	else if (!strcmp(board_name, "vme_xtown_9u")) {
		type = BD_VME_XTOWN_9U;
	}
	else if (!strcmp(board_name, "menet")) {
		type = BD_MENET;
	}
	else if (board_name[0]) {
		type = BD_UNKNOWN;
	}
	else {
		type = BD_NOBOARD; 
	}
	bp = alloc_hwcomponent(HW_BOARD, type, hcp->h_flags);
	bp->c_inv_class = inv_class;
	bp->c_inv_type = inv_type;
	return(bp);
}

/*
 * add_board_name()
 */
static void
add_board_name(hwconfig_t *hcp, hw_component_t *bp)
{
	if (bp->c_category == HW_SYSBOARD) {
		/* System level boards
		 */
		if (bp->c_type == SBD_MOTHERBOARD) {
			bp->c_name = get_string(hcp->h_st, "MOTHERBOARD", HWAFLG(hcp));
		}
		if (bp->c_type == SBD_BACKPLANE) {
			bp->c_name = get_string(hcp->h_st, "BACKPLANE", HWAFLG(hcp));
		}
		if (bp->c_type == SBD_MIDPLANE) {
			bp->c_name = get_string(hcp->h_st, "MIDPLANE", HWAFLG(hcp));
		}
		if (bp->c_type == SBD_FRONTPLANE) {
			bp->c_name = get_string(hcp->h_st, "FRONTPLANE", HWAFLG(hcp));
		}
	}
	else if (bp->c_category == HW_BOARD) {

		/* Regular boards
		 */
		if (bp->c_type == BD_NODE) {
			bp->c_name = get_string(hcp->h_st, "NODE", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_ROUTER) {
			bp->c_name = get_string(hcp->h_st, "ROUTER", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_METAROUTER) {
			bp->c_name = get_string(hcp->h_st, "METAROUTER", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_BASEIO) {
			bp->c_name = get_string(hcp->h_st, "BASEIO", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_HIPPI_SERIAL) {
			bp->c_name = get_string(hcp->h_st, "HIPPI_SERIAL", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_PCI_XIO) {
			bp->c_name = get_string(hcp->h_st, "PCI_XIO", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_MSCSI) {
			bp->c_name = get_string(hcp->h_st, "MSCSI", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_GE14_4) {
			bp->c_name = get_string(hcp->h_st, "GE14_4", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_GE16_4) {
			bp->c_name = get_string(hcp->h_st, "GE16_4", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_FIBRE_CHANNEL) {
			bp->c_name = get_string(hcp->h_st, "FIBRE_CHANNEL", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_IP19) {
			bp->c_name = get_string(hcp->h_st, "IP19", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_IP21) {
			bp->c_name = get_string(hcp->h_st, "IP21", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_IP25) {
			bp->c_name = get_string(hcp->h_st, "IP25", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_IP30) {
			bp->c_name = get_string(hcp->h_st, "IP30", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_IO4) {
			bp->c_name = get_string(hcp->h_st, "IO4", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_MC3) {
			bp->c_name = get_string(hcp->h_st, "MC3", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_MIO) {
			bp->c_name = get_string(hcp->h_st, "MIO", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_KTOWN) {
			bp->c_name = get_string(hcp->h_st, "KTOWN", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_XTALK_PCI) {
			bp->c_name = get_string(hcp->h_st, "XTALK_PCI", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_QUAD_ATM) {
			bp->c_name = get_string(hcp->h_st, "QUAD_ATM", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_VME_XTOWN_9U) {
			bp->c_name = get_string(hcp->h_st, "VME_XTOWN_9U", HWAFLG(hcp));
		}
		else if (bp->c_type == BD_MENET) {
			bp->c_name = get_string(hcp->h_st, "MENET", HWAFLG(hcp));
		}
	}
}

/*
 * cpu_name_to_type()
 */
int
cpu_name_to_type(char *name)
{
	if (!strcmp(name, "r2000")) {
		return(CPU_R2000);
	}
	if (!strcmp(name, "R2000A")) {
		return(CPU_R2000A);
	}
	if (!strcmp(name, "R3000")) {
		return(CPU_R3000);
	}
	if (!strcmp(name, "R3000A")) {
		return(CPU_R3000A);
	}
	if (!strcmp(name, "R4000")) {
		return(CPU_R4000);
	}
	if (!strcmp(name, "R4400")) {
		return(CPU_R4400);
	}
	if (!strcmp(name, "R4600")) {
		return(CPU_R4600);
	}
	if (!strcmp(name, "R4650")) {
		return(CPU_R4650);
	}
	if (!strcmp(name, "R4700")) {
		return(CPU_R4700);
	}
	if (!strcmp(name, "R5000")) {
		return(CPU_R5000);
	}
	if (!strcmp(name, "R6000")) {
		return(CPU_R6000);
	}
	if (!strcmp(name, "R6000A")) {
		return(CPU_R6000A);
	}
	if (!strcmp(name, "r2000")) {
		return(CPU_R8000);
	}
	if (!strcmp(name, "R10000")) {
		return(CPU_R10000);
	}
	if (!strcmp(name, "R12000")) {
		return(CPU_R12000);
	}
	return(CPU_UNKNOWN);
}

/*
 * add_sub_components()
 */
int
add_sub_components(hwconfig_t *hcp, hw_component_t *cp)
{
	int i, len, vhndl, banks, bank_size, cpus, cpuinfo_size;
	k_ptr_t gvp, ngvp, gep, gip, meminfop, mbp, membankp;
	k_ptr_t cpuinfop, cpu_infop;
	kaddr_t meminfo, membank, cpuinfo;
	hw_component_t *mbcp, *cip;
	char membank_name[15];
	char Name[256];

	vhndl = cp->c_vhndl;
	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	ngvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);
	meminfop = kl_alloc_block(INVENT_MEMINFO_SIZE, K_TEMP);

	switch (cp->c_category) {

		case HW_BOARD:

			switch (cp->c_type) {

				case BD_NODE:
				case BD_IP31:
					kl_get_graph_vertex_s(vhndl, 0, gvp, 0);
					if (KL_ERROR) {
					}

					/* Get the detailed information on node memory
					 */
					kl_vertex_edge_name(gvp, "memory", gep);
					if (KL_ERROR) {
					}
					vhndl = KL_UINT(gep, "graph_edge_s", "e_vertex");
					kl_get_graph_vertex_s(vhndl, 0, ngvp, 0);
					if (KL_ERROR) {
					}
					/* Get the _detail_invent info entry
					 */
					kl_vertex_info_name(ngvp, "_detail_invent", gip);
					if (KL_ERROR) {
					}

					meminfo = kl_kaddr(gip, "graph_info_s", "i_info");
					kl_get_struct(meminfo, INVENT_MEMINFO_SIZE, meminfop,
							"invent_meminfo");
					NODE_DATA(cp)->n_memory = 
						KL_UINT(meminfop, "invent_meminfo", "im_size");

					banks = KL_UINT(meminfop, "invent_meminfo", "im_banks");
					bank_size = (banks * kl_struct_len("invent_membnkinfo"));
					membankp = kl_alloc_block(bank_size, K_TEMP);
					membank = meminfo +
						kl_member_offset("invent_meminfo", "im_bank_info");
					kl_get_block(membank, bank_size, membankp, 
						"invent_membnkinfo");
					mbp = membankp;

					for (i = 0; i < banks; i++) {

						if (KL_UINT(mbp, "invent_membnkinfo", "imb_size")) {
							mbcp = alloc_hwcomponent(HW_MEMBANK, 
								0, hcp->h_flags);
							sprintf(membank_name, "MEMBANK_%d", i);
							mbcp->c_name = get_string(hcp->h_st, 
									membank_name, HWAFLG(hcp));
							mbcp->c_location = cp->c_location;
							MEMBANK_DATA(mbcp)->mb_attr = 
								KL_UINT(mbp, "invent_membnkinfo", "imb_attr");
							MEMBANK_DATA(mbcp)->mb_flag = 
								KL_UINT(mbp, "invent_membnkinfo", "imb_flag");
							MEMBANK_DATA(mbcp)->mb_size = 
								KL_UINT(mbp, "invent_membnkinfo", "imb_size");
							MEMBANK_DATA(mbcp)->mb_bnk_number = i;
							hwcmp_add_child(cp, mbcp);
						}
						len = kl_struct_len("invent_membnkinfo");
						mbp = (k_ptr_t)((uint)mbp + len);
					}
					kl_free_block(membankp);

					/* Get the detailed information on node cpus
					 */
					kl_vertex_edge_name(gvp, "cpu", gep);
					if (KL_ERROR) {
					}
					vhndl = KL_UINT(gep, "graph_edge_s", "e_vertex");
					kl_get_graph_vertex_s(vhndl, 0, ngvp, 0);
					if (KL_ERROR) {
					}

					/* Get the number of cpus installed
					 */
					cpus = KL_UINT(ngvp, "graph_vertex_s", "v_num_edge");

					/* Get the cpu data
					 */
					for (i = 0; i < cpus; i++) {

						/* Get the next cpu edge....get the vhndl for
						 * the vertex the edge points to...then get the
						 * vertex.
						 */
						kl_get_graph_edge_s(ngvp, (kaddr_t)i, 0, gep, 0);
						vhndl = KL_INT(gep, "graph_edge_s", "e_vertex"); 
						kl_get_graph_vertex_s(vhndl, 0, gvp, 0);

						/* Get the _detail_invent info entry
						 */
						kl_vertex_info_name(gvp, "_detail_invent", gip);
						if (KL_ERROR) {
						}
						cpuinfo = kl_kaddr(gip, "graph_info_s", "i_info");
						cpuinfo_size = 
							KL_INT(gip, "graph_info_s", "i_info_desc");
						cpuinfop = kl_alloc_block(cpuinfo_size, K_TEMP);
						kl_get_block(cpuinfo, cpuinfo_size, 
									 cpuinfop, "invent_cpuinfo");
						if (KL_ERROR) {
						}

						cip = alloc_hwcomponent(HW_CPU, 0, hcp->h_flags);
						
						cip->c_inv_class = INV_PROCESSOR;
						cip->c_inv_type = INV_CPUCHIP;
						cip->c_vhndl = vhndl;

						CPU_DATA(cip)->cpu_enabled = 
							KL_INT(cpuinfop, "invent_generic_s", "ig_flag")
							& INVENT_ENABLED;
						CPU_DATA(cip)->cpu_module = 
							KL_INT(cpuinfop, "invent_generic_s", "ig_module");
						CPU_DATA(cip)->cpu_slot = 
							KL_INT(cpuinfop, "invent_generic_s", "ig_slot");
						CPU_DATA(cip)->cpu_slice = 
							KL_INT(cpuinfop, "invent_cpuinfo", "ic_slice");
						CPU_DATA(cip)->cpu_id = 
							KL_INT(cpuinfop, "invent_cpuinfo", "ic_cpuid");
						cpu_infop = (k_ptr_t)((unsigned)cpuinfop + 
							kl_member_offset("invent_cpuinfo", "ic_cpu_info"));
						CPU_DATA(cip)->cpu_speed = 
							KL_INT(cpu_infop, "cpu_inv_s", "cpufq");
						CPU_DATA(cip)->cpu_flavor = 
							KL_INT(cpu_infop, "cpu_inv_s", "cpuflavor");
						CPU_DATA(cip)->cpu_cachesz = 
							KL_INT(cpu_infop, "cpu_inv_s", "sdsize");
						CPU_DATA(cip)->cpu_cachefreq = 
							KL_INT(cpu_infop, "cpu_inv_s", "sdfreq");

						cip->c_location = cp->c_location;
						cip->c_name = get_string(hcp->h_st, 
							cpu_name(CPU_DATA(cip)->cpu_flavor), HWAFLG(hcp));

						cip->c_type = cpu_name_to_type(cip->c_name);
						
						hwcmp_add_child(cp, cip);
						kl_free_block(cpuinfop);
					}
					break;
			}
			break;
	}
	kl_free_block(gvp);
	kl_free_block(ngvp);
	kl_free_block(gep);
	kl_free_block(gip);
	kl_free_block(meminfop);
	return(0);
}

/*
 * get_module_info()
 */
static int
get_module_info(hwconfig_t *hcp, hw_component_t *mp)
{
	int i, type, num_edges, v, vhndl, module;
	k_ptr_t gvp, gvp2, gep, gip;
	hw_component_t *mplnp, *bp;
	char slot_name[256];
	char board_name[256];

	if (K_IP != 27) {
		return(1);
	}

	/* Allocate a hw_component_s struct for the module's midplane
	 */
	mplnp = alloc_hwcomponent(HW_SYSBOARD, SBD_MIDPLANE, hcp->h_flags);
	hwcmp_add_child(mp, mplnp);

	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gvp2 = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);

	/* Determine which slots on the midplane have boards in them
	 */
	kl_get_graph_vertex_s(mp->c_vhndl, 0, gvp, 0);
	kl_vertex_edge_name(gvp, "slot", gep);
	v = KL_INT(gep, "graph_edge_s", "e_vertex");
	kl_get_graph_vertex_s(v, 0, gvp, 0);
	num_edges = KL_INT(gvp, "graph_vertex_s", "v_num_edge");
	for (i = 0; i < num_edges; i++) {

		/* Get the edge for the next slot and capture the slot name
		 * and vertex handle from the edge.
		 */
		kl_get_graph_edge_s(gvp, (kaddr_t)i, 0, gep, 0);
		kl_get_edge_name(gep, slot_name);
		vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");

		/* Now, using the vertex handle contained in the edge, get the
		 * vertex containing the name of the board contained in the slot.
		 * At the same time, get the vertex handle for the board entry
		 * itself. 
		 */
		kl_get_graph_vertex_s(vhndl, 0, gvp2, 0);
		kl_get_graph_edge_s(gvp2, 0, 0, gep, 0);
		vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");
		kl_get_edge_name(gep, board_name);
		if (bp = get_board_info(hcp, vhndl, board_name)) {
			bp->c_location = get_string(hcp->h_st, slot_name, HWAFLG(hcp));
			bp->c_vhndl = vhndl;
			if (add_nic_info(hcp, mp, &bp)) {
				add_board_name(hcp, bp);
			}
			hwcmp_add_child(mplnp, bp);
			add_sub_components(hcp, bp);
		}
	}
	/* Just in case, we have to make sure we have a name for the midplane.
	 * If the NIC info is missing, it will be NULL...
	 */
	if (!mp->c_cmplist->c_name) {
		add_board_name(hcp, mp->c_cmplist);
	}
	kl_free_block(gvp);
	kl_free_block(gvp2);
	kl_free_block(gep);
	kl_free_block(gip);

	/* Make sure the tree node levels are set properly
	 */
	kl_update_hwconfig(hcp);
	return(0);
}

/*
 * get_IP29_info()
 */
static int
get_IP29_info(hwconfig_t *hcp, hw_component_t *mp)
{
	int i, type, num_edges, v, vhndl, module, nic_size;
	k_ptr_t gvp, gep, gip, nic;
	char *t, *Part, *Name, *Serial, *Revision, *Group;
	char *Capability, *Variety, *Laser;
	hw_component_t *mbdp, *nodep, *bp;
	hw_component_t *next_hwcp, *parent_hwcp = (hw_component_t*)NULL;
	path_t *path;
	path_rec_t *p;
	path_chunk_t *pcp;
	char slot_name[256];
	char board_name[256];

	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);

	/* Get the module vertex and then the slot edge
	 */
	kl_get_graph_vertex_s(mp->c_vhndl, 0, gvp, 0);
	if (!kl_vertex_edge_name(gvp, "slot", gep)) {
		kl_free_block(gvp);
		kl_free_block(gep);
		kl_free_block(gip);
		return(1);
	}

	/* Now, get the slot vertex 
	 */
	vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");
	kl_get_graph_vertex_s(vhndl, 0, gvp, 0);

	/* Get the MotherBoard vertex
	 */
	if (!kl_vertex_edge_name(gvp, "MotherBoard", gep)) {
		kl_free_block(gvp);
		kl_free_block(gep);
		kl_free_block(gip);
		return(1);
	}
	kl_get_edge_name(gep, slot_name);
	vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");
	kl_get_graph_vertex_s(vhndl, 0, gvp, 0);

	/* Allocate a hw_component_s struct for the motherboard
	 */
	mbdp = alloc_hwcomponent(HW_SYSBOARD, SBD_MOTHERBOARD, hcp->h_flags);
	mbdp->c_vhndl = vhndl;
	add_board_name(hcp, mbdp);
	hwcmp_add_child(mp, mbdp);

	/* Get the node vertex
	 */
	if (!kl_vertex_edge_name(gvp, "node", gep)) {
		kl_free_block(gvp);
		kl_free_block(gep);
		kl_free_block(gip);
		return(1);
	}
	vhndl = KL_INT(gep, "graph_edge_s", "e_vertex");
	kl_get_graph_vertex_s(vhndl, 0, gvp, 0);

	/* Allocate a hw_component_s struct for the node board
	 */
	nodep = alloc_hwcomponent(HW_BOARD, BD_NODE, hcp->h_flags);
	nodep->c_vhndl = vhndl;
	nodep->c_location = get_string(hcp->h_st, slot_name, HWAFLG(hcp));
	hwcmp_add_child(mbdp, nodep);
	add_board_name(hcp, nodep);
	add_sub_components(hcp, nodep);

	/* Even though some of the components contain info stored in
	 * NIC chips, an IP29 system is significantly different from an IP27
	 * that we cannot use the add_nic_info() function to gather the data.
	 * That's OK, because the possible configurations are significantly
	 * more simplified than with the larger systems...
	 */
	path = kl_init_path_table();
	if (kl_find_pathname(0, "_nic", path, hcp->h_flags, 0)) {
		kl_free_block(gvp);
		kl_free_block(gep);
		kl_free_block(gip);
		kl_free_path(path);
		return(1);
	}
	if (pcp = path->pchunk) {
		for (i = 0; i <= pcp->current; i++) {
			p = pcp->path[i]->prev;
			kl_get_graph_vertex_s(p->vhndl, 0, gvp, 0);
			if (KL_ERROR) {
				continue;
			}
			/* Get the pointer to the NIC information
			 */
			if (kl_vertex_info_name(gvp, "_nic", gip)) {

				/* We have to allocate an extra byte just in case there 
				 * is more than one entry in a NIC string. The extra byte 
				 * just ensures that we don't overrun the allocated 
				 * memory in the kl_get_mfg_info() call.
				 */
				nic_size = KL_UINT(gip, "graph_info_s", "i_info_desc") + 2;
				nic = (char*)kl_alloc_block(nic_size, K_TEMP);
				if (!nic) {
					KL_SET_ERROR(KLE_BAD_NIC_DATA);
				}
				else {
					kl_get_block(kl_kaddr(gip, "graph_info_s", "i_info"),
						nic_size, nic, "NIC");
					if (*(char*)nic == 0) {
						KL_SET_ERROR(KLE_BAD_NIC_DATA);
					}
				}
				if (KL_ERROR) {
					if (nic) {
						kl_free_block(nic);
					}
					continue;
				}
				t = nic;
				do {

					t = kl_get_mfg_info(&Part, &Name, &Serial, &Revision,
							&Group, &Capability, &Variety, &Laser, t);

					/* Reset next_hwcp to the "root"
					 */
					next_hwcp = hcp->h_hwcmp_root;

					if (!strcmp(Name, "PIMM_2XT5_1MB")) {
						/* System MotherBoard
						 */
						next_hwcp = 
							find_hwcp(next_hwcp, HW_SYSBOARD, SBD_MOTHERBOARD);
					}
					else if (!strncmp(Name, "IP29", 2)) {
						/* Node Board
						 */
						next_hwcp = find_hwcp(next_hwcp, HW_BOARD, BD_NODE);

					}
					else {
						/* Handle anything else that shows up as a NIC
						 * entry...
						 */
						parent_hwcp = find_hwcp(next_hwcp, 
							 HW_SYSBOARD, SBD_MOTHERBOARD);
						if (parent_hwcp) {
							next_hwcp = alloc_hwcomponent(HW_BOARD, 
									BD_UNKNOWN, hcp->h_flags);
						}
						else {
							next_hwcp = (hw_component_t *)NULL;
						}
					}			

					/* Now fill in the rest of the data obtained from 
					 * the NIC
					 */
					if (next_hwcp) {
						if (*Name) {
							next_hwcp->c_name = 
								get_string(hcp->h_st, Name, HWAFLG(hcp));
						}
						if (*Serial) {
							next_hwcp->c_serial_number = 
								get_string(hcp->h_st, Serial, HWAFLG(hcp));
						}
						if (*Part) {
							next_hwcp->c_part_number = 
								get_string(hcp->h_st, Part, HWAFLG(hcp));
						}
						if (*Revision) {
							next_hwcp->c_revision =
								get_string(hcp->h_st, Revision, HWAFLG(hcp));
						}
						/* If parent_hwcp is not-NULL, then add the next_hwcp
						 * as a child to that hw_component.
						 */
						if (parent_hwcp) {
							hwcmp_add_child(parent_hwcp, next_hwcp);
							parent_hwcp = (hw_component_t*)NULL;
						}
					}
				} while (t && *t);
				if (nic) {
					kl_free_block(nic);
					nic = 0;
				}
			}
		}
	}
	kl_free_path(path);

	/* Make sure the tree node levels are set properly
	 */
	kl_update_hwconfig(hcp);
	kl_free_block(gvp);
	kl_free_block(gep);
	kl_free_block(gip);
	return(0);
}

/*
 * add_nic_info()
 */
static int
add_nic_info(hwconfig_t *hcp, hw_component_t *m, hw_component_t **bpp)
{
	int nic_size, nic_count = 0;
	k_ptr_t gvp, gip, nic = (k_ptr_t)NULL;
	char *t, *Part, *Name, *Serial, *Revision, *Group; 
	char *Capability, *Variety, *Laser;
	hw_component_t *b = *bpp;

	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	kl_get_graph_vertex_s(b->c_vhndl, 0, gvp, 0);
	if (KL_ERROR) {
		kl_free_block(gvp);
		return(1);
	}

	/* Get the pointer to the NIC information
	 */
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);
	if (kl_vertex_info_name(gvp, "_nic", gip)) {

		/* We have to allocate an extra byte just in case there is more
		 * than one entry in a NIC string (which is the case for the first
		 * node in a module which has the info for System Serial Number, 
		 * and the midplane -- in addition to the IP27 info. The extra
		 * byte just ensures that we don't overrun the allocated memory
		 * in the kl_get_mfg_info() call.
		 */
		nic_size = KL_UINT(gip, "graph_info_s", "i_info_desc") + 2;
		nic = (char*)kl_alloc_block(nic_size, K_TEMP);
		if (!nic) {
			KL_SET_ERROR(KLE_BAD_NIC_DATA);
		}
		else {
			kl_get_block(kl_kaddr(gip, "graph_info_s", "i_info"),
				nic_size, nic, "NIC");
			if (*(char*)nic == 0) {
				KL_SET_ERROR(KLE_BAD_NIC_DATA);
			}
		}
		if (KL_ERROR) {
			kl_free_block(gvp);
			kl_free_block(gip);
			if (nic) {
				kl_free_block(nic);
			}
			return(1);
		}
	}
	kl_free_block(gvp);
	kl_free_block(gip);

	/* If nic is NULL, it means that there wasn't any _nic entry in
	 * the vertex. That being the case, we need to return an error.
	 */
	if (!(t = nic)) {
		return(1);
	}

	do {
		t = kl_get_mfg_info(&Part, &Name, &Serial, &Revision, 
							&Group, &Capability, &Variety, &Laser, t);
		if (KL_ERROR) {
			kl_free_block(nic);
			return(1);
		}

		if (!strcmp(Name, "MODULEID")) {
			if (*Serial) {
				m->c_serial_number = 
					get_string(hcp->h_st, Serial, HWAFLG(hcp));
			}
			else {
				m->c_serial_number = 
					get_string(hcp->h_st, "NA", HWAFLG(hcp));
			}

			/* This is a hack to allow the IP27 board to be treated as
			 * a regular board and not a sub-component (plug-in board).
			 */
			nic_count--;
		}
		else if (strstr(Name, "MPLN") || strstr(Name, "Midplane")) {

			hw_component_t *mplnp;

			if ((m->c_cmplist->c_category != HW_SYSBOARD) && 
					(m->c_cmplist->c_type != SBD_MIDPLANE)) {
				kl_free_block(nic);
				return(1);	
			}

			mplnp = m->c_cmplist;

			if (*Name) {
				mplnp->c_name = get_string(hcp->h_st, Name, HWAFLG(hcp));
			}
			if (*Serial) {
				mplnp->c_serial_number = 
					get_string(hcp->h_st, Serial, HWAFLG(hcp));
			}
			if (*Part) {
				mplnp->c_part_number = 
					get_string(hcp->h_st, Part, HWAFLG(hcp));
			}
			if (*Revision) {
				mplnp->c_revision = 
					get_string(hcp->h_st, Revision, HWAFLG(hcp));
			}

			/* See above...
			 */
			nic_count--;
		}
		else {
			
			/* There is a possiblity of multiple entries in a NIC
			 * where more than one entry is not MODULEID or *MPLN. In
			 * this case, we have to allocate another hwcomponent_s
			 * struct and add the data.
			 *
			 * For example, the KTOWN board plugs into the GE14-4 
			 * board and details for both boards are in the NIC
			 * located on the GE14-4 board.  We have to be careful
			 * here. The entries in the NIC are in no particular order.
			 * We can't assume that just because an entry is first, 
			 * that the next entry is a sub-component (it often is
			 * the other way around).
			 */

			hw_component_t *nextb;

			/* If this is not our first time through the loop, allocate
			 * a new hwcomponent_s struct. We then need to try and determine 
			 * if the new component is a sub-component of the first one. If
			 * it's the other way around (the first one is a sub-component
			 * of the second one), we need to adjust the records accordingly.
			 * If we can't determine which is which (because the component
			 * is something we don't know about), then just treat the second
			 * one as the sub-component.
			 *
			 * Note that there are a few pieces of data from the first 
			 * component that are valid for the second component (namely
			 * the location and vertex handle). We can't assume what 
			 * the category and type are...since we don't know what it
			 * is yet. These have to be determined and resolved later...
			 */
			if (nic_count > 0) {
				nextb = alloc_hwcomponent(0, 0, hcp->h_flags);
				nextb->c_location = 
					get_string(hcp->h_st, b->c_location, HWAFLG(hcp));
				nextb->c_vhndl = b->c_vhndl;
			}
			else {
				nextb = b;
			}

			if (Name && *Name) {
				nextb->c_name = get_string(hcp->h_st, Name, HWAFLG(hcp));

				/* There are several boards that might have been assigned
				 * the wrong type value or where the board was not 
				 * identified properly. This results from info from more 
				 * than one board being contained in a single NIC entry,
				 * or by there being some other problem with the hwpath.
				 * We have to make sure that the type found by the 
				 * get_board_info() function is correct.
				 */
				if (strstr(Name, "MIO")) {
					nextb->c_category = HW_BOARD;
					nextb->c_type = BD_MIO;
				}
				else if (strstr(Name, "KTOWN")) {
					nextb->c_category = HW_BOARD;
					nextb->c_type = BD_KTOWN;
				}
				if (strstr(Name, "GE14_2")) {
					nextb->c_category = HW_BOARD;
					nextb->c_type = BD_GE14_2;
				}
				if (strstr(Name, "GE14_4")) {
					nextb->c_category = HW_BOARD;
					nextb->c_type = BD_GE14_4;
				}
				else if (strstr(Name, "GE16_4")) {
					nextb->c_category = HW_BOARD;
					nextb->c_type = BD_GE16_4;
				}
				else if (strstr(Name, "IP31PIMM")) {
					nextb->c_category = HW_BOARD;
					nextb->c_type = BD_IP31PIMM;
				}
			}
			if (Serial && *Serial) {
				nextb->c_serial_number = 
					get_string(hcp->h_st, Serial, HWAFLG(hcp));
			}
			if (Part && *Part) {
				nextb->c_part_number = 
					get_string(hcp->h_st, Part, HWAFLG(hcp));
			}
			if (Revision && *Revision) {
				nextb->c_revision = 
					get_string(hcp->h_st, Revision, HWAFLG(hcp));
			}
			if (nextb != b) {

				if (Name && *Name) {

					/* This is where we have to figure out if the current
					 * component is the sub-component of b or the other
					 * way around. We also need to fill in some details 
					 * on the second component.
					 */
					if (strstr(Name, "GE14")) {
						nextb->c_category = HW_BOARD;
						nextb->c_type = BD_GE14_4;
						hwcmp_add_child(nextb, b);
						*bpp = nextb;
					}
					else if (strstr(Name, "GE16")) {
						nextb->c_category = HW_BOARD;
						nextb->c_type = BD_GE16_4;
						hwcmp_add_child(nextb, b);
						*bpp = nextb;
					}
					else if (!strncmp(Name, "BASEI", 5)) {
						nextb->c_category = HW_BOARD;
						nextb->c_type = BD_BASEIO;
						hwcmp_add_child(nextb, b);
						*bpp = nextb;
					}
					else if (!strcmp(Name, "IP31")) {
						nextb->c_category = HW_BOARD;
						nextb->c_type = BD_IP31;

						/* If we get here, we have to copy the private
						 * node_data struct over to the actual IP31
						 * hw_component_s struct. This is real messy,
						 * but necessary because the NIC entries are in
						 * reverse order.
						 */
						if ((b->c_category == HW_BOARD) 
								&& (b->c_type == BD_IP31PIMM)) {
							if (b->c_data) {
								nextb->c_data = b->c_data;
								b->c_data = 0;
							}
						}
						if (!nextb->c_data) {
							nextb->c_data = kl_alloc_block(sizeof(node_data_t), 								HWAFLG(hcp));
						}
						hwcmp_add_child(nextb, b);
						*bpp = nextb;
					}
					else if (strstr(Name, "MIO")) {
						nextb->c_category = HW_BOARD;
						nextb->c_type = BD_MIO;
						hwcmp_add_child(b, nextb);
					}
					else if (strstr(Name, "KTOWN")) {
						nextb->c_category = HW_BOARD;
						nextb->c_type = BD_KTOWN;
						hwcmp_add_child(b, nextb);
					}
					else if (strstr(Name, "Midplane")) {
						nextb->c_category = HW_SYSBOARD;
						nextb->c_type = SBD_MIDPLANE;
						hwcmp_add_child(b, nextb);
					}
					else {
						hwcmp_add_child(b, nextb);
					}
				}
				else {
					add_board_name(hcp, nextb);
					hwcmp_add_child(b, nextb);
				}
			}
		}
		if (t && *t) {
			/* There is more info in the NIC...
			 */
			nic_count++;
		}
	} while (t && *t);
	kl_free_block(nic);
	return(0);
}

/*
 * get_board_list()
 */
static void
get_board_list(hwconfig_t *hcp, hw_component_t *parent, int vhndl)
{
	int i, type, num_edges, v;
	k_ptr_t gvp, gvp2, gep, gip;
	hw_component_t *bp;
	char slot_name[256];
	char board_name[256];

	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gvp2 = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);

	/* Load in the vertex, get the slot edge, and then get the vertex
	 * it points to.
	 */
	kl_get_graph_vertex_s(vhndl, 0, gvp, 0);
	if (KL_ERROR) {
		return;
	}
	kl_vertex_edge_name(gvp, "slot", gep);
	v = KL_INT(gep, "graph_edge_s", "e_vertex");
	kl_get_graph_vertex_s(v, 0, gvp, 0);
	num_edges = KL_INT(gvp, "graph_vertex_s", "v_num_edge");
	for (i = 0; i < num_edges; i++) {

		/* Get the edge for the next slot and capture the slot name
		 * and vertex handle from the edge.
		 */
		kl_get_graph_edge_s(gvp, (kaddr_t)i, 0, gep, 0);
		kl_get_edge_name(gep, slot_name);
		v = KL_INT(gep, "graph_edge_s", "e_vertex");

		/* Now, using the vertex handle contained in the edge, get the
		 * vertex containing the name of the board contained in the slot.
		 * At the same time, get the vertex handle for the board entry
		 * itself. 
		 */
		kl_get_graph_vertex_s(v, 0, gvp2, 0);
		kl_get_graph_edge_s(gvp2, 0, 0, gep, 0);
		if (!KL_ERROR) {
			v = KL_INT(gep, "graph_edge_s", "e_vertex");
			kl_get_edge_name(gep, board_name);
		}
		else {
			v = 0;
			board_name[0] = 0;
		}
		if (bp = get_board_info(hcp, v, board_name)) {
			bp->c_location = get_string(hcp->h_st, slot_name, HWAFLG(hcp));
			bp->c_vhndl = v;
		}
		hwcmp_add_child(parent, bp);
	}
	kl_free_block(gvp);
	kl_free_block(gvp2);
	kl_free_block(gep);
	kl_free_block(gip);
	return;
}

/*
 * get_EVEREST_info()
 */
static int
get_EVEREST_info(hwconfig_t *hcp, hw_component_t *mplnp)
{
	int slot;
	evcfginfo_t *ep = evcfgip;
	evbrdinfo_t *brd;
	hw_component_t *bp, *first_hwcp, *thwcp;
	char membank_name[15];
	char ioa_name[15];

	if (!IS_EVEREST) {
		return(1);
	}

	get_board_list(hcp, mplnp, mplnp->c_vhndl);

	/* Walk the EVEREST config table to see what boards are installed
	 * on the system.
	 */
	bp = mplnp->c_cmplist;
	do {
		slot = atoi(bp->c_location);
		if (slot < EV_MAX_SLOTS) {
			brd = &(ep->ecfg_board[slot]);
			switch (brd->eb_type) {

				case EVTYPE_IP19:
				case EVTYPE_IP21:
				case EVTYPE_IP25: {
					int k, numcpus;
					evcpucfg_t *evcpu;
					hw_component_t *cip;

					bp->c_enabled = brd->eb_enabled;

					if (K_IP == 21) {
						numcpus = 2;
					}
					else {
						numcpus = 4;
					}
					for (k = 0; k < numcpus; k++) {
						evcpu = (evcpucfg_t*)&brd->eb_cpu.eb_cpus[k];

						/* Skip if the cpu does not appear to be present
						 */
						if (evcpu->cpu_diagval == EVDIAG_NOTFOUND) {
							continue;
						}

						cip = alloc_hwcomponent(HW_CPU, 0, hcp->h_flags);
						
						cip->c_enabled = evcpu->cpu_enable;
						cip->c_inv_class = INV_PROCESSOR;
						cip->c_inv_type = INV_CPUCHIP;

						CPU_DATA(cip)->cpu_slot = brd->eb_slot;
						CPU_DATA(cip)->cpu_slice = k;
						CPU_DATA(cip)->cpu_id = evcpu->cpu_vpid;
						CPU_DATA(cip)->cpu_speed = evcpu->cpu_speed;
						CPU_DATA(cip)->cpu_cachesz = evcpu->cpu_cachesz;
						CPU_DATA(cip)->cpu_promrev = evcpu->cpu_promrev;

						if (K_IP == 19) {
							cip->c_name = 
								get_string(hcp->h_st, "R4400", HWAFLG(hcp));
							cip->c_type = CPU_R4400;
						}
						else if (K_IP == 21) {
							cip->c_name = 
								get_string(hcp->h_st, "R8000", HWAFLG(hcp));
							cip->c_type = CPU_R8000;
						}
						else if (K_IP == 21) {
							cip->c_name = 
								get_string(hcp->h_st, "R10000", HWAFLG(hcp));
							cip->c_type = CPU_R10000;
						}
						else {
							cip->c_name =
								get_string(hcp->h_st, "UNKNOWN", HWAFLG(hcp));
							cip->c_type = CPU_UNKNOWN;
						}
						hwcmp_add_child(bp, cip);
					}
					break;
				}

				case EVTYPE_MC3: {
					int k;
					evbnkcfg_t *bnk;
					hw_component_t *mbp;
					static int flip[] = {0, 4, 1, 5, 2, 6, 3, 7};

					/* Make sure some the base level information about
					 * the board is set.
					 */
					bp->c_type = BD_MC3;
					bp->c_inv_class = INV_MEMORY;
					bp->c_enabled = brd->eb_enabled;

					/* Skip if the mc3 board does not appear to 
					 * have passed diags.
					 */
					if (brd->eb_diagval != EVDIAG_PASSED) {
						continue;
					}

					/* Now walk through the memory banks and capture
					 * the relevant information
					 */
					for (k = 0; k < MC3_NUM_BANKS; k++) {

						bnk = (evbnkcfg_t*)&brd->eb_mem.ebun_banks[flip[k]];

						if (bnk->bnk_size == MC3_NOBANK) {
							continue;
						}

						mbp = alloc_hwcomponent(HW_MEMBANK, 0, hcp->h_flags);
						sprintf(membank_name, "MEMBANK_%d", k);
						mbp->c_name = get_string(hcp->h_st, 
								membank_name, HWAFLG(hcp));
						MEMBANK_DATA(mbp)->mb_enable = bnk->bnk_enable;
						MEMBANK_DATA(mbp)->mb_size = bnk->bnk_size;
						MEMBANK_DATA(mbp)->mb_bnk_number = k;
						hwcmp_add_child(bp, mbp);
					}
					break;
				}

				case EVTYPE_IO4: {
					int k;
					evioacfg_t *ioap;
					hw_component_t *ioacmp;

					bp->c_type = BD_IO4;
					bp->c_inv_class = INV_IOBD;
					bp->c_inv_type = INV_EVIO;
					bp->c_enabled = brd->eb_enabled;

					/* Skip if the IO4 board does not appear to 
					 * have passed diags.
					 */
					if (brd->eb_diagval != EVDIAG_PASSED) {
						continue;
					}

					/* Now walk through the io adapter records and capture
					 * the relevant information. Note that physical adapter
					 * 0 is not valid, it is reserved for map ram.
					 */
					for (k = 1; k < IO4_MAX_PADAPS; k++) {

						ioap = (evioacfg_t*)&brd->eb_io.eb_ioas[k];

						if (!ioap->ioa_enable || (
									ioap->ioa_type == IO4_ADAP_NULL)) {
							continue;
						}
						
						ioacmp = alloc_hwcomponent(HW_IOA, 0, hcp->h_flags);
						sprintf(ioa_name, "IOA_%d", k);
						ioacmp->c_name = get_string(hcp->h_st, 
								ioa_name, HWAFLG(hcp));
						IOA_DATA(ioacmp)->ioa_enable = ioap->ioa_enable;
						IOA_DATA(ioacmp)->ioa_type = ioap->ioa_type;
						switch (ioap->ioa_type) {
							case IO4_ADAP_SCSI:
								ioacmp->c_type = IOA_SCSI;
								break;

							case IO4_ADAP_EPC:
								ioacmp->c_type = IOA_EPC;
								break;

							case IO4_ADAP_FCHIP:
								ioacmp->c_type = IOA_FCHIP;
								break;

							case IO4_ADAP_SCIP:
								ioacmp->c_type = IOA_SCIP;
								break;

							case IO4_ADAP_VMECC:
								ioacmp->c_type = IOA_VMECC;
								break;

							case IO4_ADAP_HIPPI:
								ioacmp->c_type = IOA_HIPPI;
								break;

							case IO4_ADAP_FCG:
								ioacmp->c_type = IOA_FCG;
								break;

							case IO4_ADAP_DANG:
								ioacmp->c_type = IOA_DANG;
								break;

							case IO4_ADAP_GIOCC:
								ioacmp->c_type = IOA_GIOCC;
								break;

							case IO4_ADAP_HIP1A:
								ioacmp->c_type = IOA_HIP1A;
								break;

							case IO4_ADAP_HIP1B:
								ioacmp->c_type = IOA_HIP1B;
								break;

							default:
								ioacmp->c_type = IOA_UNKNOWN;
								break;
						}
						IOA_DATA(ioacmp)->ioa_virtid = ioap->ioa_virtid;
						IOA_DATA(ioacmp)->ioa_subtype = ioap->ioa_subtype;
						hwcmp_add_child(bp, ioacmp);
					}
					break;
				}

				case EVTYPE_EMPTY:
					break;

				default:
					break;
			}
		}
		add_board_name(hcp, bp);
		bp = bp->c_next;
	} while (bp != mplnp->c_cmplist);

	/* Now remove any hw_component records for slots that do not have
	 * any boards in them. We only want the hw_components that are
	 * installed.
	 */
	bp = first_hwcp = mplnp->c_cmplist;
	do {
next:		
		if (bp->c_type == 0) {
			bp = bp->c_next;
			thwcp = bp->c_prev;
			unlink_hwcomponent(thwcp);
			free_hwcomponents(thwcp);
			if (bp == mplnp->c_cmplist) {
				/* We removed the first hw_component
				 */
				first_hwcp = mplnp->c_cmplist;
				goto next;
			}
		}
		else {
			bp = bp->c_next;
		}
	} while (bp != first_hwcp);

	/* Make sure the tree node levels are set properly
	 */
	kl_update_hwconfig(hcp);
	return(0);
}

/*
 * kl_get_mfg_info()
 */
char *
kl_get_mfg_info(
	char **Part, 
	char **Name, 
	char **Serial, 
	char **Revision, 
	char **Group, 
	char **Capability, 
	char **Variety, 
	char **Laser, 
	char *s)
{
	char *NIC = "";
	char *t;

	*Part = "";
	*Name = "";
	*Serial = "";
	*Revision = "";
	*Group = "";
	*Capability = "";
	*Variety = "";
	*Laser = "";
	NIC = "";

	/* Get the first token
	 */
	if (!(t = strtok(s, ":;"))) {
		KL_SET_ERROR(KLE_BAD_NIC_DATA);
		return((char*)NULL);
	}

	if (s) {
		s = (char *)0;
	}

	while (t) {

		if (!strcmp(t,"Part")) {
			if (!(*Part = strtok(s,":;"))) {
				break;
			}
		}
		else if (!strcmp(t,"Name")) {
			if (!(*Name = strtok(s,":;"))) {
				break;
			}
		}
		else if (!strcmp(t,"Serial")) {
			if (!(*Serial = strtok(s,":;"))) {
				break;
			}
		}
		else if (!strcmp(t,"Revision")) {
			if (!(*Revision = strtok(s,":;"))) {
				break;
			}
		}
		else if (!strcmp(t,"Group")) {
			if (!(*Group = strtok(s,":;"))) {
				break;
			}
		}
		else if (!strcmp(t,"Capability")) {
			if (!(*Capability = strtok(s,":;"))) {
				break;
			}
		}
		else if (!strcmp(t,"Variety")) {
			if (!(*Variety = strtok(s,":;"))) {
				break;
			}
		}
		else if (!strcmp(t,"Laser")) {

			if (!(*Laser = strtok(s,":;"))) {
				break;
			}

			/* Advance the current string pointer to just beyond
			 * the last token
			 */
			t = *Laser + strlen(*Laser) + 1;

			if (strcmp(NIC,"")) {
				if (DEBUG(KLDC_GLOBAL, 1)) {
					printf("[missing info: %s] Laser %12s\n", NIC, *Laser);
				}
				KL_SET_ERROR(KLE_BAD_NIC_DATA);
			}
			return(t);
		}
		if (!strcmp(t,"NIC")) {
			if (!(NIC = strtok(s,":;"))) {
				break;
			}
		}

		/* Get the next token
		 */
		t = strtok(s, ":;");
	}
	return(t);
}

/*
 * inv_data()
 */
void
inv_data(hw_component_t *hwcp, invent_data_t *invp, int aflg)
{
	hwcp->c_inv_class = invp->inv_class;
	hwcp->c_inv_type = invp->inv_type;
	hwcp->c_inv_controller = invp->inv_controller;
	hwcp->c_inv_unit = invp->inv_unit;
	hwcp->c_inv_state = invp->inv_state;
	hwcp->c_inv_vhndl = hwcp->c_vhndl = invp->inv_vhndl;
}

/* The following facility for probing connected SCSI devices was modified
 * from a program authored by Matthias Fouquet-Lapar mfl@sgi.com.
 */
#define SCSI_DIR        "/dev/scsi"

scsi_inq_t *
scsi_device_info(int ctlr, int dev, int lun, int aflg)
{
	dsreq_t *dsp;
	scsi_inq_t *scsi_inq;
	char path [64];

	if (!ACTIVE) {
		return((scsi_inq_t *)NULL);
	}

	sprintf(path,"/dev/scsi/sc%dd%dl%d",ctlr,dev,lun);
	scsi_inq = kl_alloc_block(sizeof(scsi_inq_t), aflg);

	if ((dsp = dsopen(path, O_RDONLY)) == NULL) {
		kl_free_block(scsi_inq);
		return((scsi_inq_t *)NULL);
	}

	if (inquiry12(dsp, (char *)scsi_inq, sizeof(scsi_inq_t), 0) == 0) {
		dsclose(dsp);
		return(scsi_inq);
	}
	kl_free_block(scsi_inq);
	dsclose(dsp);
	return((scsi_inq_t *)NULL);
}

/*
 * get_invent_hwcp()
 */
static hw_component_t *
get_invent_hwcp(hwconfig_t *hcp, invstruct_t *invrec, int flag) 
{
	hw_component_t *hwcp;
	invent_data_t *invp; 
	char Name[256];

	if (!hcp || !invrec) {
		return((hw_component_t *)NULL);
	}

	if (!(invp = invrec->ptr)) {
		return((hw_component_t *)NULL);
	}

	hwcp = alloc_hwcomponent(HW_INVENTORY, 0, hcp->h_flags);

	/* Get the handle for the vertex containing the _invent info
	 * to allow the path to easily be reconstructed.
	 */
	hwcp->c_vhndl = invp->inv_vhndl;

	/* Setup an invent_data_s struct and copy it to the one in hwcp
	 */
	inv_data(hwcp, invp, hcp->h_flags);

	/* If there isn't already a name, get one now. Note that the name
	 * is somewhat cryptic in nature. A full description string can be 
	 * obtained at any time using the hwcp_description() function in
	 * libhwconfig.
	 */
	if (!hwcp->c_name) {
		if (hwcp->c_name = inv_name(hwcp, hcp->h_st, HWAFLG(hcp))) {

			/* If this is being run on a live system, get whatever
			 * vendor information we can from the attached SCSI devices.
			 */
			if (ACTIVE && (hwcp->c_inv_class == INV_DISK)) {
				int i;
				scsi_inq_t *sinfo;
				char scsi_info[256];

				if (sinfo = scsi_device_info(hwcp->c_inv_controller,
								hwcp->c_inv_unit, 0, HWAFLG(hcp))) {
					
					sprintf(scsi_info, "%.16s", sinfo->product_id);
					for(i = 15; (i >= 0) && (scsi_info[i] == ' '); i--) {
						scsi_info[i] = 0;
					}
					hwcp->c_part_number = 
						get_string(hcp->h_st, scsi_info, HWAFLG(hcp));

					sprintf(scsi_info, "%.4s", sinfo->product_rev);
					for(i = 3; (i >= 0) && (scsi_info[i] == ' '); i--) {
						scsi_info[i] = 0;
					}
					hwcp->c_revision = 
						get_string(hcp->h_st, scsi_info, HWAFLG(hcp));
					
					sprintf(scsi_info, "%.20s", sinfo->vendor_spec);
					for(i = 19; (i >= 0) && (scsi_info[i] == ' '); i--) {
						scsi_info[i] = 0;
					}
					hwcp->c_serial_number = 
						get_string(hcp->h_st, scsi_info, HWAFLG(hcp));

#ifdef NOT 
					sprintf(scsi_info, "controller=%d, device=%d, "
							"product_id=%.16s, product_rev=%.4s, "
							"vendor_spec=%.20s",
							hwcp->c_inv_controller,
							hwcp->c_inv_unit,
							sinfo->product_id,
							sinfo->product_rev,
							sinfo->vendor_spec);
					fprintf(stderr, "%s\n", scsi_info);
#endif
					kl_free_block(sinfo);
				}
			}
			return(hwcp);
		}
	}
	free_hwcomponents(hwcp);
	return((hw_component_t *)NULL);
}

/*
 * alloc_invstruct()
 */
invstruct_t *
alloc_invstruct(
	int class, 
	int type, 
	int controller, 
	int unit, 
	int state, 
	path_rec_t *path)
{
	invstruct_t *invp;

	invp = (invstruct_t *)kl_alloc_block(sizeof(invstruct_t), K_TEMP);
	invp->ptr = (invent_data_t*)kl_alloc_block(sizeof(invent_data_t), K_TEMP);
	invp->ptr->inv_class = class;
	invp->ptr->inv_type = type;
	invp->ptr->inv_controller = controller;
	invp->ptr->inv_unit = unit;
	invp->ptr->inv_state = state;
	invp->ptr->inv_vhndl = path->prev->vhndl;
	invp->path = path;
	return(invp);
}

/* 
 * free_invstruct()
 */
void
free_invstruct(invstruct_t *invp)
{
	kl_free_pathname(invp->path);
	kl_free_block(invp->ptr);
	kl_free_block(invp);
}

/*
 * hwcp_find_by_vhndl()
 */
hw_component_t *
hwcp_find_by_vhndl(hwconfig_t *hcp, int vhndl)
{
	hw_component_t *hwcp;

    /* Just in case...
	 */
	if (!hcp || !hcp->h_hwcmp_root) {
		return((hw_component_t *)NULL);
	}

	/* Get the first real hw_component (not the root, which is
	 * a dummy entry for 'system').
	 */
	if (!(hwcp = hcp->h_hwcmp_root->c_cmplist)) {
		return((hw_component_t *)NULL);
	}

	do {
		if (hwcp->c_vhndl == vhndl) {
			break;
		}
	} while (hwcp = next_hwcmp(hwcp));
	return(hwcp);
}

/*
 * push_level()
 */
invent_level_t *
push_level(
	invent_level_t *cur_level, 
	invstruct_t *invp, 
	int invp_free,
	hw_component_t *hwcp)
{
	invent_level_t *new_level;

	new_level = (invent_level_t *)kl_alloc_block(
		sizeof(invent_level_t), K_TEMP);

	if (cur_level) {
		new_level->prev = cur_level;
		cur_level->next = new_level;
		new_level->level = cur_level->level + 1;
	}
	if (new_level->invp = invp) {
		new_level->invp_free = invp_free;
		new_level->vhndl = invp->path->prev->vhndl;
	}
	new_level->hwcp = hwcp;

#ifdef INVENT_LEVEL_DEBUG
	fprintf(stderr, "LEVEL: ");
	if (cur_level) {
		fprintf(stderr, "%d-->", cur_level->level);
	}
	fprintf(stderr, "%d\n", new_level->level);
#endif

	return(new_level);
}

/*
 * pop_level()
 */
invent_level_t *
pop_level(invent_level_t *cur_level)
{
	invent_level_t *new_level;
	path_rec_t *p, *p1;

	if (!cur_level) {
#ifdef INVENT_LEVEL_DEBUG
		fprintf(stderr, "LEVEL: no level\n");
#endif
		return((invent_level_t *)NULL);
	}

	if (new_level = cur_level->prev) {
		new_level->next = (invent_level_t *)NULL;
	}
	if (cur_level->invp && cur_level->invp_free) {
		free_invstruct(cur_level->invp);
	}
	kl_free_block(cur_level);

#ifdef INVENT_LEVEL_DEBUG
	fprintf(stderr, "LEVEL: ");
	if (cur_level) {
		fprintf(stderr, "%d-->", cur_level->level);
	}
	if (new_level) {
		fprintf(stderr, "%d\n", new_level->level);
	}
	else {
		fprintf(stderr, "NO_LEVEL\n");
	}
#endif

	return(new_level);
}

/*
 * walk_IP32_invent_table()
 */
int
walk_IP32_invent_table(hwconfig_t *hcp, invent_rec_t *invent_table, int flag)
{
	int i, class, type;
	hw_component_t *hwcp;
	invstruct_t *invp, *tinvp;
	invent_level_t *cur_level = (invent_level_t *)NULL; 
	path_rec_t *p, *p2, *p3, *path;

	/* Set up the motherboard as the base level
	 */
	if (!(hwcp = hcp->h_hwcmp_root->c_cmplist)) {
		return(1);
	}
	cur_level = push_level(cur_level, 0, 0, hwcp);

	for (i = 0; i < invent_table->count; i++) {
		invp = invent_table->table[i];
		path = invp->path;

		/* Get the hw_component_s struct for the new component
		 */
		if (!(hwcp = get_invent_hwcp(hcp, invp, flag))) {
#ifdef INVENT_DEBUG
			fprintf(stderr, "%d: CONTINUING...\n", invp->path->vhndl); 
#endif
			continue;
		}

		if (cur_level) {
#ifdef INVENT_DEBUG
			fprintf(stderr, "#2 vertex %d in path: ",  cur_level->vhndl);

			p = path;
			do {
				fprintf(stderr, "/%s", p->name);
				p = p->next;
			} while (p != path);
			fprintf(stderr, "\n");
#endif
			/* We need to make sure that our current level is
			 * correct. There are some instances where a PCI
			 * adapter does not have an _invent entry. In such
			 * a case, we will (incorrectly) attach the sub-component
			 * directly to the board. The pathname contains the
			 * pci slot...all we have to do is search for it.
			 */
			if (p = find_parent(path, flag)) {
found_parent:
				if (p->vhndl != cur_level->vhndl) {
					hw_component_t *new_hwcp;

#ifdef INVENT_DEBUG
					fprintf(stderr, "p->vhndl=%d, cur_level->vhndl=%d\n",
						p->vhndl, cur_level->vhndl);
#endif
					if (!strcmp(p->name, "scsi_ctlr") || 
								!strcmp(p->prev->name, "scsi_ctlr")) {

						if (!strcmp(p->name, "scsi_ctlr")) {
							p = p->next;
						}
						p2 = kl_clone_path(path, K_TEMP);
						while (p2->prev->vhndl != p->vhndl) {
							p3 = p2->prev;
							p2->prev = p3->prev;
							p2->prev->next = p2;
							kl_free_block(p3->name);
							kl_free_block(p3);
						}
						tinvp = alloc_invstruct(INV_DISK, INV_SCSICONTROL, 
										0, 0, atoi(p->name), p2);
					}
					else if (!strcmp(p->name, "pci") ||
								!strcmp(p->prev->name, "pci")) {

						if (!strcmp(p->name, "pci")) {
							p = p->next;
						}
						p2 = kl_clone_path(path, K_TEMP);
						while (p2->prev->vhndl != p->vhndl) {
							p3 = p2->prev;
							p2->prev = p3->prev;
							p2->prev->next = p2;
							kl_free_block(p3->name);
							kl_free_block(p3);
						}
						tinvp = alloc_invstruct(INV_IOBD, INV_PCIADAP, 
										0, 0, atoi(p->name), p2);
					}
					if (tinvp) {
						if (new_hwcp = get_invent_hwcp(hcp, tinvp, flag)) {
							hwcmp_add_child(cur_level->hwcp, new_hwcp);
							cur_level = 
								push_level(cur_level, tinvp, 1, new_hwcp);
						}
						else {
							/* XXX -- this shouldn't happen!!
							 */
							free_invstruct(tinvp);
						}
						tinvp = (invstruct_t*)NULL;
					}
				}
			}
			else {
				if (p = find_parent(path, flag)) {
					if (hwcp = hwcp_find_by_vhndl(hcp, p->prev->vhndl)) {
						cur_level = push_level(cur_level, 0, 0, hwcp);
					}
				}
				if (!cur_level) {
					fprintf(stderr, "NO CUR_LEVEL\n");
				}
			}

			/* Add the new hw_component_s struct as a sub_component
			 * of the current level hw_component.
			 */
			hwcmp_add_child(cur_level->hwcp, hwcp);

			/* Check to see if the new inventory component should 
			 * be a new level. These types of components include
			 * boards, PCI adapters, and SCSI controllers. 
			 */
			class = hwcp->c_inv_class;
			type = hwcp->c_inv_type;
			if (((class == INV_IOBD) && (type == INV_PCIADAP)) ||
				((class == INV_DISK) && (type == INV_SCSICONTROL))) {

				cur_level = push_level(cur_level, invp, 0, hwcp);
			}
		}
	}
	/* Make sure that cur_level is set equal to zero...
	 */
	while (cur_level = pop_level(cur_level)) {
		/* Do nothing! */
	}
	return(0);
}

/* 
 * walk_invent_table()
 */
int
walk_invent_table(hwconfig_t *hcp, invent_rec_t *invent_table, int flag)
{
	int				 i, vhndl, class, type, twocpu=0;
	invstruct_t		*invp, *tinvp, *curinvp = (invstruct_t*)NULL;
	invent_level_t	*cur_level = (invent_level_t *)NULL; 
	path_rec_t		*p, *p2, *p3, *path, *curpath;
	hw_component_t  *hwcp, *hwcp2;


	for (i = 0; i < invent_table->count; i++) {
		invp = invent_table->table[i];
		path = invp->path;
		vhndl = path->prev->vhndl;

		/* Make sure that vhndl does not reference the .invent entry.
		 * if it does, than that handle will be the handle for a bunch
		 * of hw components. We want to search only for unique 
		 * vhndls
		 */
		if (!strcmp(path->prev->name, ".invent")) {
			vhndl = 0;
		}

		if (vhndl && (hwcp = hwcp_find_by_vhndl(hcp, vhndl))) {

			/* Update the hw_component_s struct with the data from the
			 * inventory_s struct (class, type, etc.).
			 */
			inv_data(hwcp, invp->ptr, HWAFLG(hcp));

			/* Set the current level to zero...
			 */
			while (cur_level = pop_level(cur_level)) {
				/* Do nothing! */
			}
			cur_level = push_level(cur_level, invp, 0, hwcp);
		}
		else {
		
			 /* Get the hw_component_s struct for the new component
			  */
			if (!(hwcp = get_invent_hwcp(hcp, invp, flag))) {
#ifdef INVENT_DEBUG
				fprintf(stderr, "%d: CONTINUING...\n", vhndl); 
#endif
				continue;
			}

			if (cur_level) {
				if (!kl_vertex_in_path(path, cur_level->vhndl)) {
					/* The vhndl of our current level is not in the new 
					 * path. We have to pop paths off until we find the
					 * level the current path is part of...
					 */

					do {
						if (cur_level = pop_level(cur_level)) {
							if (kl_vertex_in_path(path, cur_level->vhndl)) {
								break;
							}
						}
					} while (cur_level);
				}
			}

			if (cur_level) {
#ifdef INVENT_DEBUG
				fprintf(stderr, "#1 vertex %d in path: ",  cur_level->vhndl);

				p = path;
				do {
					fprintf(stderr, "/%s", p->name);
					p = p->next;
				} while (p != path);
				fprintf(stderr, "\n");
#endif
				/* We need to make sure that our current level is
				 * correct. There are some instances where a PCI
				 * adapter does not have an _invent entry. In such
				 * a case, we will (incorrectly) attach the sub-component
				 * directly to the board. The pathname contains the
				 * pci slot...all we have to do is search for it.
				 */
				if (p = find_parent(path, flag)) {
found_parent:
					if (p->vhndl != cur_level->vhndl) {
						hw_component_t *new_hwcp;

#ifdef INVENT_DEBUG
						fprintf(stderr, "p->vhndl=%d, cur_level->vhndl=%d\n",
							p->vhndl, cur_level->vhndl);
#endif
						if (!strcmp(p->name, "scsi_ctlr") || 
									!strcmp(p->prev->name, "scsi_ctlr")) {

							if (!strcmp(p->name, "scsi_ctlr")) {
								p = p->next;
							}
							p2 = kl_clone_path(path, K_TEMP);
							while (p2->prev->vhndl != p->vhndl) {
								p3 = p2->prev;
								p2->prev = p3->prev;
								p2->prev->next = p2;
								kl_free_block(p3->name);
								kl_free_block(p3);
							}
							tinvp = alloc_invstruct(INV_DISK, INV_SCSICONTROL, 
											0, 0, atoi(p->name), p2);
						}
						else if (!strcmp(p->name, "pci") ||
									!strcmp(p->prev->name, "pci")) {

							if (!strcmp(p->name, "pci")) {
								p = p->next;
							}
							p2 = kl_clone_path(path, K_TEMP);
							while (p2->prev->vhndl != p->vhndl) {
								p3 = p2->prev;
								p2->prev = p3->prev;
								p2->prev->next = p2;
								kl_free_block(p3->name);
								kl_free_block(p3);
							}
							tinvp = alloc_invstruct(INV_IOBD, INV_PCIADAP, 
											0, 0, atoi(p->name), p2);
						}
						if (tinvp) {
							if (new_hwcp = get_invent_hwcp(hcp, tinvp, flag)) {
								hwcmp_add_child(cur_level->hwcp, new_hwcp);
								cur_level = 
									push_level(cur_level, tinvp, 1, new_hwcp);
							}
							else {
								/* XXX -- this shouldn't happen!!
								 */
								free_invstruct(tinvp);
							}
							tinvp = (invstruct_t*)NULL;
						}
					}
				}

				/* Add the new hw_component_s struct as a sub_component
				 * of the current level hw_component.
				 */
				hwcmp_add_child(cur_level->hwcp, hwcp);

				/* Check to see if the new inventory component should 
				 * be a new level. These types of components include
				 * boards, PCI adapters, and SCSI controllers. 
				 */
				class = hwcp->c_inv_class;
				type = hwcp->c_inv_type;
				if (((class == INV_IOBD) && (type == INV_PCIADAP)) ||
					((class == INV_DISK) && (type == INV_SCSICONTROL))) {

					cur_level = push_level(cur_level, invp, 0, hwcp);
				}
			}
			else {
				/* There isn't a level struct to tell us which
				 * component to link this into...
				 * We need to do a search of the pathname to see
				 * what board this item is associated with. If
				 * there isn't a board in the pathname, then we
				 * should add it to the system level component
				 * for the system/module.
				 */
#ifdef INVENT_DEBUG
				 fprintf(stderr, "%4d: %s\n", vhndl, hwcp->c_name);
#endif
				 if (p = find_parent(path, flag)) {
					if (hwcp2 = hwcp_find_by_vhndl(hcp, p->vhndl)) {
						class = hwcp2->c_inv_class;
						type = hwcp2->c_inv_class;
						p2 = kl_clone_path(path, K_TEMP);
						while (p2->prev->vhndl != p->vhndl) {
							p3 = p2->prev;
							p2->prev = p3->prev;
							p2->prev->next = p2;
							kl_free_block(p3->name);
							kl_free_block(p3);
						}
						tinvp = alloc_invstruct(class, type, 0, 0, 
							atoi(p->name), p2);
						cur_level = push_level(cur_level, tinvp, 1, hwcp2);
						tinvp = (invstruct_t *)NULL;
						goto found_parent;
					}
				}
				/* We STILL haven't found the parent of our current
				 * hardware component. We're down to the point where we
				 * have to hack something special to handle each particular
				 * exception/system.
				 */
				class = hwcp->c_inv_class;
				type = hwcp->c_inv_type;
				hwcp2 = (hw_component_t*)NULL;
				switch(class) {

					case INV_PROCESSOR:
						switch (K_IP) {
							case 30:
								if(!(hwcp2 = find_hwcp(hcp->h_hwcmp_root, 
											HW_BOARD, BD_PM10)))
								{
									if(!(hwcp2 = find_hwcp(hcp->h_hwcmp_root, 
												HW_BOARD, BD_PM20)))
									{

										hwcp2 = find_hwcp(hcp->h_hwcmp_root, 
												HW_BOARD, BD_PROCESSOR);
									} else twocpu=1;
								}
								break;
						}
						break;

					case INV_MEMORY:
						switch (K_IP) {
							case 30:
								hwcp2 = find_hwcp(hcp->h_hwcmp_root, 
											HW_BOARD, BD_IP30);
								break;
						}
						break;
				}
				if (hwcp2) {
					hwcmp_add_child(hwcp2, hwcp);
					/* IP30 as only one _invent for a 2 CPU system */
					if(twocpu)
					{
						hwcmp_add_peer(hwcp, dup_hwcomponent(hcp, hwcp));
						twocpu=0;
					}
				}
				else {
					free_hwcomponents(hwcp);
				}
			}
		}

#ifdef INVENT_DEBUG
		if (hwcp) {
			fprintf(stderr, "%4d: %s\n", vhndl, hwcp->c_name);
		}
#endif
	}

	/* Make sure that cur_level is set equal to zero...
	 */
	while (cur_level = pop_level(cur_level)) {
		/* Do nothing! */
	}
	return(0);
}

/*
 * invent_match()
 */
int
invent_match(k_ptr_t invp, int match_flags)
{
	int class, type;

	/* If match_flags is equal to INVENT_ALL, we just return one
	 * as all events match...
	 */
	if (match_flags == INVENT_ALL) {
		return(1);
	}

	class = KL_INT(invp, "inventory_s", "inv_class");
	type = KL_INT(invp, "inventory_s", "inv_type");

	/* Check to see if we want to include any and all SCSI interfaces, 
	 * devices, ete. 
	 */
	if ((match_flags & INVENT_SCSI_ALL) == INVENT_SCSI_ALL) {
		switch(class) {
			case INV_DISK:
			case INV_TAPE:
			case INV_SCSI:
				return(1);
		}
	}
	if (match_flags & INVENT_SCSICONTROL) {
		if ((class == INV_DISK) && (type == INV_SCSICONTROL)) {
			return(1);
		}
	}
	if (match_flags & INVENT_SCSIDRIVE) {
		if ((class == INV_DISK) && (type == INV_SCSIDRIVE)) {
			return(1);
		}
	}
	if (match_flags & INVENT_SCSITAPE) {
		if (class == INV_TAPE) {
			return(1);
		}
	}
	if (match_flags & INVENT_PCIADAP) {
		if ((class == INV_IOBD) && (type == INV_PCIADAP)) {
			return(1);
		}
	}
	if (match_flags & INVENT_CPUCHIP) {
		if ((class == INV_PROCESSOR) && (type == INV_CPUCHIP)) {
			return(1);
		}
	}
	if (match_flags & INVENT_FPUCHIP) {
		if ((class == INV_PROCESSOR) && (type == INV_FPUCHIP)) {
			return(1);
		}
	}
	if (match_flags & INVENT_MEM_MAIN) {
		if ((class == INV_MEMORY) && (type == INV_MAIN_MB)) {
			return(1);
		}
	}
	if (match_flags & INVENT_NETWORK) {
		if ((class == INV_NETWORK)) {
			return(1);
		}
	}

	/* Always include any entries for O2K I/O boards...
	 */
	if (class == INV_IOBD) {
		switch(type) {
			case INV_O200IO: 
			case INV_O2000BASEIO: 
			case INV_O2000MSCSI: 
			case INV_O2000MENET: 
			case INV_O2000HIPPI: 
			case INV_O2000HAROLD: 
			case INV_O2000MIO: 
			case INV_O2000FC:
				return(1);
		}
	}

	if (match_flags & INVENT_MISC) {
		/* XXX -- Isn't this the same as INVENT_ALL ?
		 */
		return(1);
	}
	return(0);
}

static int
kl_inmod(path_rec_t *p, int module)
{
	/* If module is set:
	 *
	 * module > 0  -- get ONLY those _invent records that are for 
	 *                items installed in module.
	 *
	 * module < 0  -- get ONLY those _invent records that are NOT
	 *                associated with ANY module.
	 *
	 * module == 0 -- get all _invent records
	 */
	if (module) {

		path_rec_t *pi=p;

		do {

			int id;

			if (strstr(p->name, "module")) {

				if (module < 0) {
					return 0;
				}

				id = atoi(p->next->name);
				if (id == module) {
					return 1;
				}
				else {
					return 0;
				}
			}
			p = p->next;
		} while(p != pi);

		if (module > 0 && p == pi) {
			return 0;
		}
	}
	return 1;
}

/*
 * get_invent_table()
 */
invent_rec_t *
get_invent_table(int module, int match_flags, int aflg)
{
	int i, j, cur = 0, invent_size;
	k_ptr_t gvp, gip;
	path_t *path;
	path_rec_t *prec, *p;
	path_chunk_t *pcp;
	kaddr_t invent;
	invent_rec_t *invent_table;
	invstruct_t **table;
	invent_data_t *ptr;
	inventory_t *invp;

	invent_table = (invent_rec_t *)kl_alloc_block(sizeof(invent_rec_t), aflg);
	table = (invstruct_t**) kl_alloc_block(sizeof(invstruct_t) * 25, aflg);

	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);
	invent_size = kl_struct_len("inventory_s");

	path = kl_init_path_table();
	if (kl_find_pathname(0, "_invent", path, K_TEMP, 0)) {
		kl_free_block(gvp);
		kl_free_block(gip);
		kl_free_path(path);
		return((invent_rec_t *)NULL);
	}

	for(pcp = path->pchunk; pcp; ) {
	
		for(i=0;i<pcp->current;i++) {

			if(!kl_inmod(pcp->path[i], module)) {
				continue;
			}

			p = pcp->path[i]->prev;

			kl_get_graph_vertex_s(p->vhndl, 0, gvp, 0);
			if (KL_ERROR) {
				continue;
			}

			/* Get the pointer to the inventory information
			 */
			if (kl_vertex_info_name(gvp, "_invent", gip)) {

				if (invent = kl_kaddr(gip, "graph_info_s", "i_info")) {

					/* It's possible that the inventory_s struct is the
					 * head of a list. If that is the case, process ALL 
					 * items on the list.
					 */
					while (invent) {
						invp = kl_alloc_block(invent_size, aflg);
						kl_get_struct(invent, invent_size, invp, "inventory_s");

						/* Check to see if this type of inventory item is
						 * one we are looking for...
						 */
						if (!invent_match(invp, match_flags)) {
							invent = kl_kaddr(invp, "inventory_s", "inv_next");
							kl_free_block(invp);
							continue;
						}
						
						if (cur && (!(cur % 25))) {
							table = kl_realloc_block(table, 
								sizeof(invstruct_t *) * (cur + 25), aflg);
						}


						ptr = (invent_data_t *)
							kl_alloc_block(sizeof(invent_data_t), aflg);

						ptr->inv_class =
							KL_UINT(invp, "inventory_s", "inv_class");
						ptr->inv_type =
							KL_UINT(invp, "inventory_s", "inv_type");
						ptr->inv_controller =
							KL_UINT(invp, "inventory_s", "inv_controller");
						ptr->inv_unit =
							KL_UINT(invp, "inventory_s", "inv_unit");
						ptr->inv_state =
							KL_UINT(invp, "inventory_s", "inv_state");

						ptr->inv_vhndl = pcp->path[i]->prev->vhndl;
						table[cur] = (invstruct_t *) 
							kl_alloc_block(sizeof(invstruct_t), aflg);
						table[cur]->ptr = ptr;
						table[cur]->path = kl_clone_path(pcp->path[i], aflg);

						invent = kl_kaddr(invp, "inventory_s", "inv_next");
						kl_free_block(invp);
						cur++;
					}
				}
			} 
		}
		if ((pcp = pcp->next) == path->pchunk) {
			break;
		}
	}
	invent_table->count = cur;
	invent_table->table = table;
	kl_free_path(path);
	kl_free_block(gvp);
	kl_free_block(gip);
	return(invent_table);
}

/*
 * compare_invent_recs()
 */
int
compare_invent_recs(invstruct_t *k1, invstruct_t *k2)
{
	int class1, type1, controller1, unit1, state1;
	int class2, type2, controller2, unit2, state2;

	class1 = KL_INT(k1->ptr, "inventory_s", "inv_class");
	type1 = KL_INT(k1->ptr, "inventory_s", "inv_type");
	controller1 = KL_UINT(k1->ptr, "inventory_s", "inv_controller");
	unit1 = KL_UINT(k1->ptr, "inventory_s", "inv_unit");
	state1 = KL_UINT(k1->ptr, "inventory_s", "inv_state");

	class2 = KL_INT(k2->ptr, "inventory_s", "inv_class");
	type2 = KL_INT(k2->ptr, "inventory_s", "inv_type");
	controller2 = KL_UINT(k2->ptr, "inventory_s", "inv_controller");
	unit2 = KL_UINT(k2->ptr, "inventory_s", "inv_unit");
	state2 = KL_UINT(k2->ptr, "inventory_s", "inv_state");

	if (class1 < class2) {
		return(-1);
	}
	else if (class1 > class2) {
		return(1);
	}

	if (type1 < type2) {
		return(-1);
	}
	else if (type1 > type2) {
		return(1);
	}

	if (controller1 < controller2) {
		return(-1);
	}
	else if (controller1 > controller2) {
		return(1);
	}

	if (unit1 < unit2) {
		return(-1);
	}
	else if (unit1 > unit2) {
		return(1);
	}

	if (state1 < state2) {
		return(-1);
	}
	else if (state1 > state2) {
		return(1);
	}
	return(0);
}

/* 
 * sort_invent_table()
 */
int
sort_invent_table(invent_rec_t *invent_table)
{
	int i, j, count, result;
	invstruct_t *temp;
	invstruct_t **list;

	count = invent_table->count;
	list = invent_table->table;

	for (i = 0; i < count; i++) {
		for (j = 0; j < (count - i - 1); j++) {
			result = compare_invent_recs(list[j], list[j + 1]);
			if (result > 0) {
				temp = list[j];
				list[j] = list[j + 1];
				list[j + 1] = temp;
			}
		}
	}
	return(0);
}

/* 
 * free_invent_table()
 */
void
free_invent_table(invent_rec_t *invent_table)
{
	int i;

	for (i = 0; i < invent_table->count; i++) {
		kl_free_pathname(invent_table->table[i]->path);
		kl_free_block(invent_table->table[i]->ptr);
		kl_free_block(invent_table->table[i]);
	}
	kl_free_block(invent_table->table);
	kl_free_block(invent_table);
}

/*
 * setup_invent_data()
 */
invent_data_t *
setup_invent_data(k_ptr_t inventp, int vhndl, int aflg)
{
	k_ptr_t *ptr;
	invent_data_t *invp;

	invp = (invent_data_t *)kl_alloc_block(sizeof(invent_data_t), aflg);
	invp->inv_class = KL_INT(inventp, "inventory_s", "inv_class");
	invp->inv_type = KL_INT(inventp, "inventory_s", "inv_type");
	invp->inv_controller = KL_UINT(inventp, "inventory_s", "inv_controller");
	invp->inv_unit = KL_UINT(inventp, "inventory_s", "inv_unit");
	invp->inv_state = KL_UINT(inventp, "inventory_s", "inv_state");
	invp->inv_vhndl = vhndl;
	return(invp);
}

/*
 * find_parent()
 */
path_rec_t *
find_parent(path_rec_t *path, int flag)
{
	path_rec_t *p;

    p = path->prev;

	/* If we are looking to see what a scsi_ctlr or pci entry is 
	 * attached to, we need to skip over it to find the correct
	 * path record.
	 */
	if (!strcmp(p->name, "scsi_ctlr") || !strcmp(p->name, "pci")) {
		p = p->prev;
	}
	else if (!strcmp(p->prev->name, "scsi_ctlr") || 
									!strcmp(p->prev->name, "pci")) {
		p = p->prev->prev;
	}

	do {
		if (!strcmp(p->name, "scsi_ctlr")) {
			if (flag & INVENT_SCSICONTROL) {
				p = p->next;
				break;
			}
		}
		else if (!strcmp(p->name, "pci")) {
			if (flag & INVENT_PCIADAP) {
				p = p->next;
				break;
			}
		}
		else if (K_IP == 27) {
			if (!strcmp(p->name, "slot")) {
				p = p->next->next;
				break;
			}
			if (!strcmp(p->name, "module")) {
				p = p->next;
				break;
			}
		}
		else if (K_IP == 30) {
			if (!strcmp(p->name, "xtalk")) {
				break;
			}
		}
		p = p->prev;
	} while (p != path->prev);
	if (p == path->prev) {
		return((path_rec_t *)NULL);
	}
	return(p);
}

#ifdef NOTYET
/* 
 * get_prom_info() - lifted from hinv.c, verbose_prom_display() 
 */
static void
get_prom_info(invent_data_t *invp, char *info)
{
	int vhndl;
	int size, im_type, im_rev, im_version;
	k_uint_t im_speed;
	static int master_io6_info_displayed = 0;
	kaddr_t i_info;
	k_ptr_t gvp, gip, misc_invent;
	path_rec_t *path, *p;

	path = kl_vertex_to_pathname(invp->inv_vhndl, 0, K_TEMP);

	vhndl = path->prev->vhndl;
	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);

	kl_get_graph_vertex_s(vhndl, 0, gvp, 0);
	if (KL_ERROR) {
		kl_free_pathname(path);
		return;
	}

	/* Get the _detail_invent info entry. If the current vertex does not
	 * have a _detail_invent entry, walk backwards until we find one...
	 */
	kl_vertex_info_name(gvp, "_detail_invent", gip);
	if (KL_ERROR) {
		if (p = find_parent(path, INVENT_ALL)) {
			while (p->vhndl != path->prev->vhndl) {
				kl_get_graph_vertex_s(p->vhndl, 0, gvp, 0);
				if (KL_ERROR) {
					p = p->next;
					continue;
				}
				kl_vertex_info_name(gvp, "_detail_invent", gip);
				if (!KL_ERROR) {
					break;
				}
				p = p->next;
			}
		}
	}

	i_info = kl_kaddr(gip, "graph_info_s", "i_info");
	size = KL_UINT(gip, "graph_info_s", "i_info_desc");
	misc_invent = kl_alloc_block(size, K_TEMP);
	kl_get_struct(i_info, size, misc_invent, "misc_invent");

	im_rev = *(int*)((unsigned)misc_invent + 
					kl_struct_len("invent_generic_s"));
	im_version = *(int*)((unsigned)misc_invent + 
					kl_struct_len("invent_generic_s") + 4);
	im_type = *(int*)((unsigned)misc_invent + 
					kl_struct_len("invent_generic_s") + 8);
	im_speed = *(k_uint_t*)((unsigned)misc_invent + 
					kl_struct_len("invent_generic_s") + 12);

	switch (im_type) {
		case INV_IP27PROM:
			sprintf(info, "IP27prom in Module %lld/Slot n%lld: Revision %d.%d",
				KL_INT(misc_invent, "invent_generic_s", "ig_module"),
				KL_INT(misc_invent, "invent_generic_s", "ig_slot"),
				im_version, im_rev);
			break;

		case INV_IO6PROM:
			if (!master_io6_info_displayed) {
				sprintf(info, "IO6prom on Global Master Baseio in Module %lld/"
					"Slot io%lld: Revision %d.%d",
					KL_INT(misc_invent, "invent_generic_s", "ig_module"),
					KL_INT(misc_invent, "invent_generic_s", "ig_slot"),
					im_version, im_rev);
				master_io6_info_displayed = 1;
			}
			break;

		default:
			break;
	}
	kl_free_block(misc_invent);
	kl_free_block(gvp);
	kl_free_block(gip);
	kl_free_pathname(path);
}
#endif

/*
 * get_gfx_board()
 */
int
get_gfx_board(kaddr_t value, int mode, k_ptr_t gfxbdp)
{
	int size, numofboards;
	struct syment *symp;    /* temp syment pointer */
	k_ptr_t GfxBoardp;
	kaddr_t addr, GfxBoard;

	switch (mode) {

		case 1:
			/* Index into GfxBoards[] array
			 */
			if (symp = kl_get_sym("GfxNumberOfBoards", K_TEMP)) {
				kl_get_block(symp->n_value, 4, 
					&numofboards, "GfxNumberOfBoards");
				kl_free_sym(symp);
			}
			else {
				return(1);
			}

			if (value >= numofboards) {
				return(1);
			}

			if (symp = kl_get_sym("GfxBoards", K_TEMP)) {
				GfxBoard = symp->n_value + (value * K_NBPW);
				addr = kl_kaddr_to_ptr(GfxBoard);
				kl_free_sym(symp);
			}
			break;

		case 2:
			/* Address of gfx_board struct
			 */
			addr = value;
			break;

		default:
			return(1);
	}

	size = kl_struct_len("gfx_board");
	kl_get_struct(addr, size, gfxbdp, "gfx_board");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_STRUCT;
		return(1);
	}
	return(0);
}

/*
 * strip_spaces()
 */
int
strip_spaces(char *s, char *buf, int len) {
	int count = len;
	char *c = s;

	if (len == 0) {
		return(1);
	}

	while (len && *c == ' ') {
		c++;
		len--;
	}
	if (len) {
		strncpy(buf, c, len);
		buf[len] = 0;
		return(0);
	}
	return(1);
}

/*
 * get_nic_info(()
 */
int
get_nic_info(
	nic_info_t *nip,
	char *nic_num, 
	char *name, 
	char *serial_num, 
	char *part_num, 
	char *rev)
{
	int ret_val = 0;
	char buf[30];

	if (!nip) {
		return(-1);
	}

	/* "Zero" out the nic strings. If we don't find anything, then we
	 * don't want to have something old laying around.
	 */
	nic_num[0] = 0;
	name[0] = 0;
	serial_num[0] = 0;
	part_num[0] = 0;
	rev[0] = 0;

	if((nip->flags & NIC_BAD_SERNUM) || (nip->sernum.fmt.family == 0)) {
		/* No NIC serial number available...
		 */
		return(-1);
	}
	if (nic_num) {
		sprintf(nic_num, "%02x%02x.%02x%02x.%02x%02x (family: %02x)",
			nip->sernum.fmt.serial[0],
			nip->sernum.fmt.serial[1],
			nip->sernum.fmt.serial[2],
			nip->sernum.fmt.serial[3],
			nip->sernum.fmt.serial[4],
			nip->sernum.fmt.serial[5],
			nip->sernum.fmt.family);

		if (!nip->sernum.fmt.crc_passed) {
			strcat(nic_num, " [CRC BAD]\n");
			return(1);
		}
	}

	if(nip->flags & NIC_BAD_PAGE_A) {
		/* No NIC page A found...
		 */
		return(2);
	}

	switch (nip->page_a.sgi_fmt_1.format) {

		case SGI_FORMAT_1:
			if (!strip_spaces(nip->page_b.sgi_fmt_1.name, buf, 14)) {
				strcat(name, buf);
			}
			if (!strip_spaces(nip->page_a.sgi_fmt_1.sgi_ser_num, buf, 10)) {
				strcat(serial_num, buf);
			}
			if (!strip_spaces(nip->page_a.sgi_fmt_1.sgi_part_num, buf, 19)) {
				strcat(part_num, buf);
			}
			if (nip->flags & NIC_BAD_PAGE_B) {
				strcat(part_num, "XXX");
			}
			else {
				if (!strip_spaces(nip->page_b.sgi_fmt_1.dash_lvl, buf, 6)) {
					strcat(part_num, buf);
				}
			}
			if (!strip_spaces(nip->page_b.sgi_fmt_1.rev_code, buf, 4)) {
				strcat(rev, buf);
			}
			break;

		case 0:
			/* Programmed NIC data not reportable...
			 */
			ret_val = 3;
			break;

		case SGI_FORMAT_INVALID:  /* 255 */
			/* Uninitialized NIC...
			 */
			ret_val = 4;
			break;

		default:
			/* Unknown NIC format...
			 */
			ret_val = 5;
			break;
	}
	return(ret_val);
}

/* 
 * alloc_gfx_component()
 */
hw_component_t *
alloc_gfx_component(
	hwconfig_t *hcp, 
	char *name, 
	char *serial_num, 
	char *part_num, 
	char *rev,
	char *loc)
{
	hw_component_t *hwcp;

	hwcp = alloc_hwcomponent(HW_BOARD, BD_GFX, hcp->h_flags);
	hwcp->c_name = get_string(hcp->h_st, name, HWAFLG(hcp));
	hwcp->c_serial_number = get_string(hcp->h_st, serial_num, HWAFLG(hcp));
	hwcp->c_part_number = get_string(hcp->h_st, part_num, HWAFLG(hcp));
	hwcp->c_revision = get_string(hcp->h_st, rev, HWAFLG(hcp));
	hwcp->c_location = get_string(hcp->h_st, loc, HWAFLG(hcp));
	return(hwcp);
}

/* 
 * add_gfx_cmponents()
 */
void
add_gfx_components(
	hwconfig_t *hcp, 
	hw_component_t *gfx_hwcp, 
	kona_config *psysconf)
{
	int found, ret_val;
	hw_component_t *hwcp;
	int hilo, hilo_lite;
	char name[30], nic_num[30], serial_num[30], part_num[30], rev[30];
	
	hilo = ((psysconf->bdset == BDSET_HILO) ||
			(psysconf->bdset == BDSET_HILO_8C) ||
			(psysconf->bdset == BDSET_HILO_GE16));

	hilo_lite = (psysconf->bdset == BDSET_HILO_LITE);

	if (hilo || hilo_lite) {
		ret_val = get_nic_info(&psysconf->nic.ktown, 
							nic_num, name, serial_num, part_num, rev);
		hwcp = hcp->h_hwcmp_root;
		found = 0;
		do {
			if ((hwcp->c_category == HW_BOARD) && (hwcp->c_type == BD_KTOWN)) {
				if (!strcmp(hwcp->c_serial_number, serial_num)) {
					found++;
					break;
				}
			}
		} while (hwcp = next_hwcmp(hwcp));
		if (!found) {
			/* KTOWN board is not already linked in...we need to 
			 * do it now.
			 */
			hwcp = alloc_gfx_component(hcp, name, serial_num, 
							part_num, rev, gfx_hwcp->c_location);
			hwcmp_add_child(gfx_hwcp, hwcp);
		}
	}
	if (!(ret_val = get_nic_info(&psysconf->nic.rm[0], 
				nic_num, name, serial_num, part_num, rev))) {
		hwcp = alloc_gfx_component(hcp, name, serial_num, 
							part_num, rev, gfx_hwcp->c_location);
		hwcmp_add_child(gfx_hwcp, hwcp);
	}
	if (hilo) {
		if (!(ret_val = get_nic_info(&psysconf->nic.tm[0], 
					nic_num, name, serial_num, part_num, rev))) {
			hwcp = alloc_gfx_component(hcp, name, serial_num, 
								part_num, rev, gfx_hwcp->c_location);
			hwcmp_add_child(gfx_hwcp, hwcp);
		}
	}
	if (!(ret_val = get_nic_info(&psysconf->nic.rm[1], 
				nic_num, name, serial_num, part_num, rev))) {
		hwcp = alloc_gfx_component(hcp, name, serial_num, 
							part_num, rev, gfx_hwcp->c_location);
		hwcmp_add_child(gfx_hwcp, hwcp);
	}
	if (hilo) {
		if (!(ret_val = get_nic_info(&psysconf->nic.tm[1], 
					nic_num, name, serial_num, part_num, rev))) {
			hwcp = alloc_gfx_component(hcp, name, serial_num, 
								part_num, rev, gfx_hwcp->c_location);
			hwcmp_add_child(gfx_hwcp, hwcp);
		}
	}
	if (!(ret_val = get_nic_info(&psysconf->nic.rm[2], 
				nic_num, name, serial_num, part_num, rev))) {
		hwcp = alloc_gfx_component(hcp, name, serial_num, 
							part_num, rev, gfx_hwcp->c_location);
		hwcmp_add_child(gfx_hwcp, hwcp);
	}
	if (hilo) {
		if (!(ret_val = get_nic_info(&psysconf->nic.tm[2], 
					nic_num, name, serial_num, part_num, rev))) {
			hwcp = alloc_gfx_component(hcp, name, serial_num, 
								part_num, rev, gfx_hwcp->c_location);
			hwcmp_add_child(gfx_hwcp, hwcp);
		}
	}
	if (!(ret_val = get_nic_info(&psysconf->nic.backplane, 
				nic_num, name, serial_num, part_num, rev))) {
		hwcp = alloc_gfx_component(hcp, name, serial_num, 
							part_num, rev, gfx_hwcp->c_location);
		hwcmp_add_child(gfx_hwcp, hwcp);
	}
	if (!(ret_val = get_nic_info(&psysconf->nic.dg, 
				nic_num, name, serial_num, part_num, rev))) {
		hwcp = alloc_gfx_component(hcp, name, serial_num, 
							part_num, rev, gfx_hwcp->c_location);
		hwcmp_add_child(gfx_hwcp, hwcp);
	}
	if (!(ret_val = get_nic_info(&psysconf->nic.dgopt, 
				nic_num, name, serial_num, part_num, rev))) {
		hwcp = alloc_gfx_component(hcp, name, serial_num, 
							part_num, rev, gfx_hwcp->c_location);
		hwcmp_add_child(gfx_hwcp, hwcp);
	}
}

/*
 * get_graphics_info()
 */
int
get_graphics_info(hwconfig_t *hcp)
{
	int i, size, numofboards, info_size, ret_val;
	int kona_len;
	k_ptr_t gfxbp;
	hw_component_t *hwcp;
	struct syment *symp;    /* temp syment pointer */
	kaddr_t info;
	k_ptr_t *infop;
	kona_info_t *konainfo;
	kona_config *psysconf;
	char name[30], nic_num[30], serial_num[30], part_num[30], rev[30];

	size = kl_struct_len("gfx_board");
	gfxbp = kl_alloc_block(size, K_TEMP);
	if (symp = kl_get_sym("GfxNumberOfBoards", K_TEMP)) {
		kl_get_block(symp->n_value, 4, &numofboards, "GfxNumberOfBoards");
		kl_free_sym(symp);
	}
	else {
		kl_free_block(gfxbp);
		return(1);
	}
	for (i = 0; i < numofboards; i++) {
		get_gfx_board(i, 1, gfxbp);
		if (KL_ERROR) {
			continue;
		}

		/* Get graphics board info struct...
		 */
		info = kl_kaddr(gfxbp, "gfx_board", "gb_info");
		info_size = kl_struct_len("gfx_info");
		infop = kl_alloc_block(info_size, K_TEMP);
		kl_get_block(info, info_size, infop, "gfx_info");
		if (!strncmp(CHAR(infop, "gfx_info", "name"), "KONA", 4)) {

			/* this is a Kona graphics board
			 */
			kona_len = KL_INT(infop, "gfx_info", "length");

			konainfo = (kona_info_t*)kl_alloc_block(kona_len, K_TEMP);
			kl_get_block(info, kona_len, konainfo, "kona_info_t");
			psysconf = &konainfo->config_info;

			ret_val = get_nic_info(&psysconf->nic.ge, 
				nic_num, name, serial_num, part_num, rev);
			if (ret_val) {
				kl_free_block(infop);
				kl_free_block(konainfo);
				continue;
			}

			/* Search for the graphics board in the hardware component
			 * tree...
			 */
			if (hwcp = hcp->h_hwcmp_root) {
				do {
					if (hwcp->c_name && !strcmp(hwcp->c_name, name)) {
						if (hwcp->c_serial_number && 
								!strcmp(hwcp->c_serial_number, serial_num)) {
							break;
						}
					}
				} while (hwcp = next_hwcmp(hwcp));
				if (hwcp) {
					add_gfx_components(hcp, hwcp, psysconf);
				}
			}
			else {
				kl_free_block(infop);
				kl_free_block(konainfo);
				return(1);
			}
			kl_free_block(konainfo);
		}
		kl_free_block(infop);
	}
	kl_free_block(gfxbp);
	return(0);
}
