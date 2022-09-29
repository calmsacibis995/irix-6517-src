/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File : promgraph.c
 * Description: Build a Graph of the hardware from the KLCONFIG
 *              data.
 * Assumption:
 *              Hardware discovery is done and KLCONFIG has been setup.
 *              Hardware includes the ASICS like HUB, BRIDGE etc
 * 		and Device controllers like Qlogic.
 *              The graph is built in 2 phases:
 *              discover controllers
 *              discover devices and update KLCONFIG and the promgraph.
 * Algorithm:
 *              graph_create() ;
 *              while (there are more lboards)
 *                 for all components on the board
 * 			graph_vertex_create()
 *			graph_info_add_LBL() Ptr to component struct
 *              Link internal components using
 *		graph_edge_add()
 * 		Link all internal components to their respective
 *		Locator nodes, for eg: all cpus to /cpu etc.
 *                  
 * 		Inter link boards using IP27's cfg_t graph and
 * 		other data from iodiscovery.
 */

#include <sys/SN/addrs.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/klconfig.h>   /* config stuff */
#include <arcs/eiob.h>
#include <sys/graph.h>            
#include <sys/hwgraph.h>          
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/nvram.h>
#include <sys/SN/gda.h>
#include <sys/PCI/bridge.h>
#include <sys/iograph.h>
#include <libsc.h>
#include <libsk.h>
#include <libkl.h>
#include <promgraph.h>
#include <pgdrv.h>
#include "io6inst.c"

#ifndef SN_PDI

/* #define PG_DEBUG */
int 	pg_debug;

extern graph_hdl_t hwgraph ;           /* To be initted so that we can */
extern int hwgraph_initted ;           /* use the hwgraph routines     */

extern moduleid_t      master_baseio_modid ;
extern slotid_t        master_baseio_slotid ;

#ifdef SABLE
extern int num_qlogic  ;
extern int num_sbldsk  ;
extern int num_ioc3    ;
extern int ioc3_installed ;
#endif

extern struct mod_tab   module_table[] ;
extern int    		dksc_map[] ;
extern int    		dksc_inv_map[] ;

#define WEIRD_MODID -1
#define MAXPNT 5

#define PROM_EDGE_LBL_PCI_SLOT 		"pci"
#define PROM_EDGE_LBL_CPU 		"cpu"

/* This name starts with a '/' because it is going to
   be appeneded to another string before getting passed to 
   create_graph_path.
   Normally, the string should not begin with a '/'. 
   If it does, it will confuse the create_graph_path routine.
*/
#define PROM_EDGE_LBL_LUN_0_DISK 	"/lun/0/disk"
#define PROM_EDGE_LBL_LUN_0_CDROM 	"/lun/0/cdrom"
#define PROM_EDGE_LBL_LUN_0_TAPE  	"/lun/0/tape"

/*
 * This flag gets set to 1 if this lib is linked into symmon.
 * This is not the same as _prom. Symmon also sets _prom
 * to 1, so that flag can't be used for some decisions.
 * Declared here so it gets linked into all stand programs.
 */
extern int _symmon;


/* XXX - This should be correctly initted by IP27prom.
 *       This is one variable per SN0 partition assuming 
 *       there is one IO PROM running per partition.
 */
extern int   num_modules ;

static int	num_scsi_drive ;
static int add_node_edge(lboard_t *) ;
int init_router_graph(lboard_t *, int , pcfg_router_t *) ;

graph_hdl_t prom_graph_hdl ;           /* THE PROM GRAPH */
graph_attr_t prom_graph_attr ;
char graph_name[] = "PROM_GRAPH" ;

vertex_hdl_t 	root_vhdl ;            /* The / vertex */
vertex_hdl_t 	hw_vertex_hdl ;        /* The /hw vertex */
vertex_hdl_t 	mod_vertex_hdl ;       /* The /hw/module vertex */

extern int sk_sable;
 
struct locator_info {
	char *name ;
	vertex_hdl_t vhdl ;
} ;

int
promgraph_connect_brd_compts(lboard_t *, 
			     unsigned char,
			     vertex_hdl_t *, int);

/* 
 * This represents the top level locator string tree structure under the
 * '/hw' vertex. 
 */
struct locator_info hw_locator_edges[] = {
#define DEV_LOCATOR_INDEX      0
	"dev", GRAPH_VERTEX_NONE,         /* All devices */
#define CPU_LOCATOR_INDEX      1
	EDGE_LBL_CPU, GRAPH_VERTEX_NONE,         /* All CPUs */
#define HUB_LOCATOR_INDEX      2
	"hub", GRAPH_VERTEX_NONE,         /* All HUBs */
#define XBOW_LOCATOR_INDEX     3
	"xbow", GRAPH_VERTEX_NONE,        /* All Xbows */
#define ROUTER_LOCATOR_INDEX     4
	"router", GRAPH_VERTEX_NONE,      /* All Routers */
#define BRIDGE_LOCATOR_INDEX     5
	"xwidget", GRAPH_VERTEX_NONE,     /* All Bridges */
#define TPU_LOCATOR_INDEX     6
	"tpu", GRAPH_VERTEX_NONE,     	  /* All TPUs */
#define GSN_LOCATOR_INDEX     7
	"gsn", GRAPH_VERTEX_NONE,     	  /* All GSN boards */
NULL, GRAPH_VERTEX_NONE
} ;

/* 
 * The locator edges hanging off /dev vertex.
 */
char *dev_locator_edges[] = {
"disk", 
"tape",
"cdrom",
"tty",
EDGE_LBL_ENET,
"audio",
"fddi",
"fc",
"input",
"graphics",
"tpu",
"gsn",
NULL
} ;

/*
 * These correspond to the #define KLSTRUCT_* stuff in klconfig.h.
 * The array index should match the corresponding string value.
 */

char *compt_names[] = {
"",
EDGE_LBL_CPU,
"hub",
"mem",
"xbow",
"bridge",
"ioc3",
"pci",
"vme",
"router",
"graphics",
"ql",
"fddi",
"mio",
"disk",
"tape",
"cdrom",
"hubuart",
EDGE_LBL_EF,
"ioc3uart",
"UNKNOWN",
"ioc3pckm",
"sgirad",
"hubtty",
"ioc3",
"fc",
"snum",
"ioc3ms",
"tpu",
"gsn", /* Primary GSN board */
"gsn", /* Auxiliary GSN board */
NULL
} ;


static struct {
        struct inst_later *head;
        struct inst_later *tail;
} instnext, instlist;

/*
 * Initializes the complete graph from KLCFGINFO, cfg_t and other data.
 */

int
init_prom_graph(void)
{
	graph_error_t graph_err ;
	int 	i ;
	gda_t *gdap;
	nasid_t		master_nasid = get_nasid() ;

	prom_graph_hdl = 0 ;
	
	/* create a graph */
	/*
  	 * Do all the stuff that is common for all NODES and
	 * PARTITIONS like, component locator struct, Module/partition
	 * locator structure etc.
	 * Then call init_prom_graph_node for each node that
	 * was found in IP27PROM.
	pg_debug = 1 ;
 	 */


#if 0
	if (!hwgraph_initted)
	{
#endif
		prom_graph_attr.ga_name = graph_name ;
		prom_graph_attr.ga_separator = '/' ;
		prom_graph_attr.ga_num_index = 64 ;
		prom_graph_attr.ga_reserved_places = 32 ;

		graph_err = graph_create(&prom_graph_attr, 
					 &prom_graph_hdl,
					 0) ; 
                if (graph_err != GRAPH_SUCCESS) {
			printf("PROM_GRAPH create error %d\n", graph_err) ;
			return 0 ;
		}

		/* Create the top level locator string graph */

		if (!init_component_locator())
			return 0 ;

		/* Init external variables so that hwgraph routines work */

		hwgraph = prom_graph_hdl ;
		hwgraph_initted = 1 ;
#if 0
	}
#endif

	if (!(init_module_locator()))
		return 0 ;

	gdap = (gda_t *)GDA_ADDR(master_nasid);
	for (i = 0; i < MAX_COMPACT_NODES; i++) {
		if (gdap->g_nasidtable[i] == INVALID_NASID)
		    break;
		PG_DPRINT("initing prom node graph for nasid %d\n",
					gdap->g_nasidtable[i]) ;
                if (_symmon) {
                        lboard_t        *lb ;
                        lb = (lboard_t *)KL_CONFIG_INFO(gdap->g_nasidtable[i]) ;                       
			lb = find_lboard(lb, KLTYPE_BASEIO);
                        if ((lb) && (lb->brd_flags & GLOBAL_MASTER_IO6)) {
                                init_prom_node_graph(gdap->g_nasidtable[i]);
                                break ;
                        } else
                                continue ;
                }
		init_prom_node_graph(gdap->g_nasidtable[i]);
	}

	make_serial(1) ;
	if (!_symmon) {
		printf("Installing PROM Device drivers ............\t\t\n") ;
	}

#ifdef SN0PROM
	/*
	 *  now perform any graphics console installation
	 */
	kl_graphics_install();
#endif

	instnext.head = instnext.tail = NULL ;

	prom_install_devices() ;

	/* If any of the guys have registered for delayed install, 
	   run them now.
	*/

        do {
		struct inst_later *execute ;

                instlist = instnext;
                instnext.head = instnext.tail = (struct inst_later *)NULL;
                while (execute=instlist.head) {
                        execute->install(execute->hw, execute->loc) ;
                        instlist.head = execute->next;
                        free(execute);
                }
        } while(instnext.head) ;

	/*
  	 * Setup a special node for the master bridge so that path
	 * aliases work.
	 */
	if (!pg_setup_bridge_alias())
		printf("Could not setup node master_bridge.\
		  	Path aliases like bootp() may not work\n") ;

	return 1 ;
}

/*
 * Create the Graph for a node. This includes the node board
 * and all its associated IO boards.
 */

init_prom_node_graph(int node)
{
	lboard_t	*brd_ptr;

	/* Create the graph for each of the boards found in this node. */

        brd_ptr = (lboard_t *)KL_CONFIG_INFO(node);
        while (brd_ptr) {
                if (brd_ptr->struct_type == REMOTE_BOARD) {
			printf("Remote board in init_prom_node_graph: fixme\n");
                }
                else {
			/* XXX Avoid DISABLED boards too */
			if ((!(brd_ptr->brd_flags & DUPLICATE_BOARD))
			&& (brd_ptr->brd_flags & ENABLE_BOARD)) {
			    PG_DPRINT("init graph for brd %x P %x %x\n", 
			brd_ptr, brd_ptr->brd_parent, brd_ptr->brd_type) ;
			    init_board_graph(brd_ptr, node) ;
			}
                }
		brd_ptr = KLCF_NEXT(brd_ptr);
        }

	/* 
	   All boards with their individual graphs are linked to make the
           complete graph. Info from ip27prom is used to link hubs routers
	   and info from iodiscover is used to link up others.
         */

	/* Link all boards found in a node. */

	link_node_boards(node) ;

	return 1 ;
}

/*
 * Build a internal 'locator' network which can be used to
 * get to component's klinfo_t struct quickly. For example
 * /dev/cpu/cpu/<id>, /xwidget/<number>/pci/slot/... 
 */

int 
init_component_locator(void)
{
	/* 
	   We need a base locator frame work to hang new compt vertices
  	   off. This is the one that does the job.
	 */
	int 		i ;
	graph_error_t 	graph_err ;
	vertex_hdl_t 	dev_vhdl, devloc_vhdl ;

	graph_err = graph_vertex_create(prom_graph_hdl, &root_vhdl) ; 
	if (graph_err != GRAPH_SUCCESS) {
		printf("Root create err %d\n", graph_err) ;
		return 0 ;
	}
	graph_err = graph_info_add_LBL(prom_graph_hdl, root_vhdl, 
                              	       INFO_LBL_VERTEX_NAME, NULL,
		     	               (arbitrary_info_t)VERTEX_NAME_ROOT) ; 
	if (graph_err != GRAPH_SUCCESS) {
		printf("Root addLBL error %d\n", graph_err) ;
		return 0 ;
	}

	hw_vertex_hdl = create_graph_path(root_vhdl, EDGE_LBL_HARDWARE, 1) ;
	graph_err = graph_info_add_LBL(prom_graph_hdl, 
				       hw_vertex_hdl, 
                              	       INFO_LBL_VERTEX_NAME, NULL,
			  	       (arbitrary_info_t)EDGE_LBL_SYSNAME) ;
	if (graph_err != GRAPH_SUCCESS) {
		printf("hw addLBL error %d\n", graph_err) ;
		return 0 ;
	}

	/* Build the locator tree for / */

	for (i = 0; hw_locator_edges[i].name != NULL; i++) {
		hw_locator_edges[i].vhdl = create_graph_path(
						hw_vertex_hdl, 
						hw_locator_edges[i].name, 1) ;
		graph_err = graph_info_add_LBL(prom_graph_hdl, 
                     			       hw_locator_edges[i].vhdl, 
					       INFO_LBL_VERTEX_NAME, 
					       NULL,
                     		               (arbitrary_info_t)
						hw_locator_edges[i].name) ;
		if (graph_err != GRAPH_SUCCESS) {
			printf("locedge addLBL error %d\n", graph_err) ;
			return 0 ;
		}
	}

	/* Build the locator tree for the /dev node */

	devloc_vhdl = hw_locator_edges[DEV_LOCATOR_INDEX].vhdl ;

	for (i=0; dev_locator_edges[i] != NULL; i++) {
		dev_vhdl = create_graph_path(devloc_vhdl, 
					     dev_locator_edges[i], 1) ;
		graph_err = graph_info_add_LBL(prom_graph_hdl, 
						dev_vhdl,
                   				INFO_LBL_VERTEX_NAME, 
					        NULL,
				  		(arbitrary_info_t)
						dev_locator_edges[i]) ;
		if (graph_err != GRAPH_SUCCESS) {
			printf("devloc addLBL error %d\n", graph_err) ;
			return 0 ;
		}
	}

	/* 
	   add an edge "hw" from the real "/hw" node to itself. This makes
	   pathnames like /hw/module work as well as /module or /dev/...
	*/
        graph_err = graph_edge_add(prom_graph_hdl, hw_vertex_hdl,
                                   hw_vertex_hdl, EDGE_LBL_HARDWARE) ;
        if (graph_err != GRAPH_SUCCESS) {
               printf("err adding edge %s to itself %d\n", 
			EDGE_LBL_HARDWARE, graph_err) ;
               return 0 ;
        }

	return 1 ;
}

int
init_module_locator(void)
{
	graph_error_t 	graph_err ;
	char	  	tmp_buf[32] ;
	int 		i ;
	vertex_hdl_t 	vhdl ;

	/* Create the vertex /hw/module */

	mod_vertex_hdl = create_graph_path(hw_vertex_hdl, EDGE_LBL_MODULE, 1) ;

	/*
	 * For all the modules found in this partition, 
	 * add edge /hw/module/<module_id>.
 	 * XXX - init_module_table should have initted the number of
  	 *       modules per partition.
 	 */
	for (i = 1; i <= num_modules; i++) {
		sprintf(tmp_buf, "%d\0", module_table[i].module_id) ;
		module_table[i].vhdl = create_graph_path(mod_vertex_hdl,
							 tmp_buf, 1) ;

		graph_err = graph_info_add_LBL(prom_graph_hdl, 
						module_table[i].vhdl,
						INFO_LBL_VERTEX_NAME, 
						NULL,
				  		(arbitrary_info_t)tmp_buf) ;
		if (graph_err != GRAPH_SUCCESS) {
			printf("module name add err %d\n", graph_err);
			return 0 ;
		}

		/* create vertex for SLOT */

		module_table[i].vhdl = create_graph_path(
					module_table[i].vhdl, 
					EDGE_LBL_SLOT, 1) ;
	} /* for */
	return (1) ;
}

static int
add_node_edge(lboard_t *lbinfo)
{
	vertex_hdl_t 	node_vhdl, vhdl = 0 ;
	char 		tmp_buf[32], buf[32] ;
	graph_error_t 	graph_err ;
	int		i ;

	/* get the name of my slot */
	get_slotname(lbinfo->brd_slot, tmp_buf);
	
	/* Get the vertex handle of my module. It usually is /hw/module/<mid>slot */
	for (i=1; i <= num_modules; i++) {
		if (module_table[i].module_id == lbinfo->brd_module) {
			vhdl = module_table[i].vhdl ;
			break ;
		}
	}

	if (!vhdl) {
		printf("add_node_edge: No vertex for module %d\n",
			lbinfo->brd_module) ;
		return 0;
	}

	/* Add edge /<slotname> to /hw/module/<mid>slot */
	node_vhdl = create_graph_path(vhdl, tmp_buf, 1) ;
        graph_err = graph_info_add_LBL(prom_graph_hdl, node_vhdl,
                                       INFO_LBL_BRD_INFO, NULL,
                                       (arbitrary_info_t)lbinfo) ;

	/* complete current path as /hw/module/<mid>/slot/<slotname>/node */
        graph_err = graph_edge_add(prom_graph_hdl, 
				   node_vhdl,
				   lbinfo->brd_graph_link,
 				   EDGE_LBL_NODE) ;
	if (graph_err != GRAPH_SUCCESS) {
               printf("mod edge %s add err %d\n", tmp_buf, graph_err) ;
               return 0 ;
        }
	return 1 ;
}

int
add_io_edge(lboard_t *lbinfo)
{
	char 		tmp_buf[32], slotname[32] , board_name[32];
	vertex_hdl_t	vhdl, new_vhdl;
	char 		*remain ;
	graph_error_t	graph_err ;

	sprintf(tmp_buf, "/module/%d/slot", lbinfo->brd_module) ;
        graph_err = hwgraph_path_lookup(hw_vertex_hdl, tmp_buf,
                                        &vhdl, &remain) ;
        if (graph_err != GRAPH_SUCCESS) {
                printf("add_io_edge: %s lookup failed %d\n", remain, graph_err) ;
                return 0 ;
        }

	get_slotname(lbinfo->brd_slot, slotname);

	/*
	 * We pass 0 for the errflg as it is possible to add dups.
	 * On Origin 200 The motherboard is both a node and io board.
	 */
        if ((new_vhdl = create_graph_path(vhdl, slotname, 0)) == vhdl) {
		printf("create_graph_path: %s can't be added to %s\n",
			board_name, tmp_buf);
		return 0;
	}
        graph_err = graph_info_add_LBL(prom_graph_hdl, new_vhdl,
                                       INFO_LBL_BRD_INFO, NULL,
                                       (arbitrary_info_t)lbinfo) ;

        get_board_name(lbinfo->brd_nasid, lbinfo->brd_module, 
			lbinfo->brd_slot, board_name);

        graph_err = graph_edge_add(prom_graph_hdl,
                         new_vhdl, lbinfo->brd_graph_link, board_name) ;

        if (graph_err != GRAPH_SUCCESS) {
                printf("add_io_edge: edgeadd error %s %d\n",
                        slotname, graph_err) ;
                return 0 ;
        }

	return 1 ;
}

/*
 * Try adding router info to the promgraph.
 * Do not report any errors. Meta routers need correct
 * module numbers and new names. Till then, this routine
 * will warn about duplicate edges as all meta routers
 * get their parent module number, which would already
 * have the normal routers
 */

int
add_router_edge(lboard_t *lbinfo)
{
        char            tmp_buf[32], slotname[32] ;
        vertex_hdl_t    vhdl ;
        char            *remain ;
        graph_error_t   graph_err ;

        sprintf(tmp_buf, "/module/%d/slot", lbinfo->brd_module) ;
        graph_err = hwgraph_path_lookup(hw_vertex_hdl, tmp_buf,
                                        &vhdl, &remain) ;
        if (graph_err != GRAPH_SUCCESS) {
#if 0
                printf("add_router_edge: %s lookup failed %d\n", remain, graph_err) ;
#endif
                return 0 ;
        }
        get_slotname(lbinfo->brd_slot, slotname);

	vhdl = create_graph_path(vhdl, slotname, 0) ;
  	if (!vhdl)
		return 0 ;
        graph_err = graph_info_add_LBL(prom_graph_hdl, vhdl,
                                       INFO_LBL_BRD_INFO, NULL,
                                       (arbitrary_info_t)lbinfo) ;

        if (graph_err != GRAPH_SUCCESS) {
#if 0
                printf("add_router_edge: brd info add err %d\n", graph_err) ;
#endif
                return 0 ;
        }
        return 1 ;
}

int
init_board_graph(lboard_t *lbinfo, int node)
{
	int rc = 0;

	switch (lbinfo->brd_type) {
	case KLTYPE_IP27:
		if ((rc = init_ip27_graph(lbinfo, node)) == 0)
		    printf("init_board_graph:init_ip27_graph error,node %d\n",
			   node);
		break ;

	case KLTYPE_IO6:
		if ((rc = init_baseio_graph(lbinfo, node)) == 0) 
		    printf("init_board_graph:init_baseio_graph err,node %d\n",
			   node);
		break ;

	case KLTYPE_MSCSI:
	case KLTYPE_MENET:
	case KLTYPE_HAROLD: /* PCI SHOEBOX BOARD */
	case KLTYPE_WEIRDIO:
		if ((rc = init_xtalkbrd_graph(lbinfo, node)) == 0)
		    printf("init_board_graph:init_xtalkbrd_graph err,node %d\n",
			   node);
		break ;

	case KLTYPE_ROUTER:
	case KLTYPE_META_ROUTER:
		init_router_graph(lbinfo, node, NULL) ;
		break ;

	case KLTYPE_MIDPLANE:
	    /* In the case of non-speedo or a speedo with an xbow
	     * build the graph for midplane (which basically populates
	     * the graph with xbow & its components).
	     */
	    if ((!(SN00) && !(CONFIG_12P4I)) || is_xbox_config(node))
		if ((rc = init_midplane8_graph(lbinfo, node)) == 0)
		    printf("init_board_graph: midplane_graph err, node %d\n",
			   node);
		break ;

	case KLTYPE_TPU:
		if ((rc = init_tpu_graph(lbinfo, node)) == 0)
		    printf("init_board_graph: tpu_graph err, node %d\n",
			   node);
		break ;

	case KLTYPE_GSN_A:
	case KLTYPE_GSN_B:
		if ((rc = init_gsn_graph(lbinfo, node)) == 0)
		    printf("init_board_graph: gsn_graph err, node %d\n",
			   node);
		break ;

	default:
            if ((KLCLASS(lbinfo->brd_type) == KLCLASS_GFX) ||
		(KLCLASS(lbinfo->brd_type) == KLCLASS_PSEUDO_GFX)) {
                if ((rc = init_graphics_graph(lbinfo, node)) == 0)
                    printf("init_board_graph: graphics_graph err, node %d\n",
                           node);
            } else
	    if (KLCLASS(lbinfo->brd_type) == KLCLASS_IO) {
                if ((rc = init_xtalkbrd_graph(lbinfo, node)) == 0)
                    printf("init_board_graph:init_xtalkbrd_graph err,node %d\n",
                           node);
	    }

		break ;
	}

	return rc ;
}

int
promgraph_edge_add(klinfo_t *klcompt,  vertex_hdl_t vert1, 
			vertex_hdl_t vert2, int flag)
{
	char		 buf[32];
	graph_error_t	 err;

	get_edge_name(klcompt, buf, flag);

        err = graph_edge_add(prom_graph_hdl, vert1, vert2, buf) ;
	if (err != GRAPH_SUCCESS) {
		printf("node %d: %d<->%d edge %s add error %d\n",
			klcompt->nasid, vert1, vert2, buf, err);
               return 0;
        }
	return 1;
}

int
promgraph_connect(klinfo_t *klcompt1, klinfo_t *klcompt2, 
		  vertex_hdl_t vert1, vertex_hdl_t vert2, int flag)
{
	if (promgraph_edge_add(klcompt1, vert1, vert2, flag) == 0)
		return 0;

#if 0
	if (promgraph_edge_add(klcompt2, vert2, vert1, flag) == 0)
		return 0;
#endif

	return 1;
}

int
promgraph_create_board(lboard_t *lb, int node, vertex_hdl_t *vert, int type)
{
	register int	i;
	klinfo_t	*klcompt;
	graph_error_t	graph_err;
	vertex_hdl_t  	tvert[MAX_COMPTS_PER_BRD], tmp_vhdl ;
	char 		tmp_buf[32] ;
	void 		*nic_info ;

        for (i = 0; i < lb->brd_numcompts; i++) {
		klcompt = (klinfo_t *)(NODE_OFFSET_TO_K1(
					node, lb->brd_compts[i]));

		/* XXX take care of DISABLED Components here */

#if 0
		if (!(klcompt->flags & KLINFO_ENABLE))
			continue ;
#endif

		graph_err = graph_vertex_create(prom_graph_hdl, &vert[i]) ;
                if (graph_err != GRAPH_SUCCESS) {
			printf("node %d: %d create error %d\n", 
					node, i, graph_err);
			return 0;
		}

                if (graph_err = pg_add_lbl(vert[i], INFO_LBL_KLCFG_INFO, 
				     	   (void *)klcompt)) {
		    printf("%d: add LBL error %d\n", node, graph_err);
		    return 0 ;
		}

                switch(klcompt->struct_type) {
                        case KLSTRUCT_BRI:
                        case KLSTRUCT_HUB:
                        case KLSTRUCT_GFX:
                        case KLSTRUCT_ROU:
                        case KLSTRUCT_TPU:
			case KLSTRUCT_GSN_A:
			case KLSTRUCT_GSN_B:
			case KLSTRUCT_XTHD:
                                if (graph_err = pg_add_lbl(vert[i], 
				    INFO_LBL_BRD_INFO, (void *)lb)) {
                    		 	printf("%d: add LBL BRD error %d\n", 
                                	    node, graph_err);
					return 0 ;
				}
                        break;
                        default:
                        break ;
                }

		switch(klcompt->struct_type) {
			case KLSTRUCT_BRI:
			case KLSTRUCT_HUB:
			case KLSTRUCT_ROU:
			case KLSTRUCT_GFX:
			case KLSTRUCT_TPU:
			case KLSTRUCT_GSN_A:
			case KLSTRUCT_GSN_B:
			case KLSTRUCT_XTHD:
				pg_add_nic_info(klcompt, vert[i]) ;
			break;
			default:
			break ;
		}
	}

	return 1;
}

int
init_ip27_graph(lboard_t *lbinfo, int node)
{
	vertex_hdl_t 	compt_vert[MAX_COMPTS_PER_BRD];
	klhub_t		*klhubp;

	/* Create a vertex for all components. */
	if (promgraph_create_board(lbinfo, node, compt_vert, KLSTRUCT_HUB) == 0)
		return 0;

#ifdef SABLE
	if (promgraph_setup_ip27_devices(lbinfo, node, compt_vert) == 0)
	        return 0;
#endif

	if (promgraph_connect_brd_compts(lbinfo, KLSTRUCT_HUB,compt_vert, 1) == 0)
	    return 0;

	add_node_edge(lbinfo) ;

	return 1 ;
}

int 
init_baseio_graph(lboard_t *lbinfo, int node)
{
	vertex_hdl_t 	compt_vert[MAX_COMPTS_PER_BRD] ;

	if (promgraph_create_board(lbinfo, node, 
			compt_vert, KLSTRUCT_BRI) == 0)
		return 0;

	if (promgraph_setup_xtalk_devices(lbinfo, node, compt_vert) == 0)
	        return 0;
	
	if (promgraph_connect_brd_compts(lbinfo, KLSTRUCT_BRI,compt_vert, 0) == 0)
	    return 0;

	return 1 ;

}

int
init_xtalkbrd_graph(lboard_t *lbinfo, int node)
{
	vertex_hdl_t 	compt_vert[MAX_COMPTS_PER_BRD] ;

	if (promgraph_create_board(lbinfo, node, compt_vert, KLSTRUCT_SCSI) == 0)
		return 0;

	if (promgraph_setup_xtalk_devices(lbinfo, node, compt_vert) == 0)
	        return 0;

	if (promgraph_connect_brd_compts(lbinfo, KLSTRUCT_BRI,compt_vert, 0) == 0)
	    return 0;

	return 1 ;
}

int
init_router_graph(lboard_t *lbinfo, int node, pcfg_router_t *cfg_rp)
{
	vertex_hdl_t 	compt_vert[MAX_COMPTS_PER_BRD] ;

	if (promgraph_create_board(lbinfo, node, compt_vert, KLSTRUCT_ROU) == 0)
		return 0;

	add_router_edge(lbinfo) ;

	return 1 ;
}

int
init_midplane8_graph(lboard_t *lbinfo, int node)
{
	vertex_hdl_t 	compt_vert[MAX_COMPTS_PER_BRD] ;

	if (promgraph_create_board(lbinfo, node, compt_vert, KLSTRUCT_XBOW) == 0)
		return 0;

	if (promgraph_connect_brd_compts(lbinfo, KLSTRUCT_XBOW, compt_vert, 1) == 0)
	    return 0;

	return 1 ;
}

int
init_graphics_graph(lboard_t *lbinfo, int node)
{
        vertex_hdl_t    compt_vert[MAX_COMPTS_PER_BRD] ;
	int		type = 0 ;

	if (KLCF_CLASS(lbinfo) == KLCLASS_GFX)
		type = KLSTRUCT_GFX ;
	else
	if (KLCF_CLASS(lbinfo) == KLCLASS_PSEUDO_GFX)
		type = KLSTRUCT_XTHD ;

        if (promgraph_create_board(lbinfo, node, compt_vert, type) == 0)
                return 0;

        if (promgraph_setup_xtalk_devices(lbinfo, node, compt_vert) == 0)
                return 0;

        if (promgraph_connect_brd_compts(lbinfo, type, compt_vert, 1) == 0)
            return 0;

	return 1 ;
}

int 
init_tpu_graph(lboard_t *lbinfo, int node)
{
	vertex_hdl_t 	compt_vert[MAX_COMPTS_PER_BRD] ;

	if (promgraph_create_board(lbinfo, node, compt_vert, KLSTRUCT_TPU) == 0)
		return 0;

        if (promgraph_connect_brd_compts(lbinfo, KLSTRUCT_TPU, compt_vert, 1) == 0)
            return 0;

  	return 1;
}

int 
init_gsn_graph(lboard_t *lbinfo, int node)
{
	vertex_hdl_t 	compt_vert[MAX_COMPTS_PER_BRD] ;
	int struct_type;

	struct_type = (lbinfo->brd_type == KLTYPE_GSN_A) ? 
		KLSTRUCT_GSN_A : KLSTRUCT_GSN_B;

	if (promgraph_create_board(lbinfo, node, compt_vert, struct_type) == 0)
		return 0;

        if (promgraph_connect_brd_compts(lbinfo, struct_type, compt_vert, 1) == 0)
            return 0;

  	return 1;
}

link_node_boards(int node)
{
	lboard_t	*brd_ptr;

        brd_ptr = (lboard_t *)KL_CONFIG_INFO(node) ;
        while (brd_ptr)  {
                if (brd_ptr->struct_type == REMOTE_BOARD) {
			printf("Remote board in link_node_boards: fixme\n");
                }
                else  {
			if ((brd_ptr->brd_parent) && /* not a root */
                         	(!(brd_ptr->brd_flags & DUPLICATE_BOARD)) && 
			    	(brd_ptr->brd_type != KLTYPE_ROUTER) &&
			    	(brd_ptr->brd_type != KLTYPE_TPU) &&
			    	(brd_ptr->brd_type != KLTYPE_GSN_A) &&
			    	(brd_ptr->brd_type != KLTYPE_GSN_B) &&
				(brd_ptr->brd_flags & KLINFO_ENABLE)) {
			    PG_DPRINT("link brd %x P %x %x\n", 
				brd_ptr, brd_ptr->brd_parent, brd_ptr->brd_type) ;
			    link_2_boards(brd_ptr);
			}
                }
		brd_ptr = KLCF_NEXT(brd_ptr);
        }
	return 1 ;
}


link_2_boards(lboard_t *lbinfo)
{
	vertex_hdl_t	v1, v2, v3 ;
	klinfo_t 	*aip1, *aip2 ;
	graph_error_t	graph_err ;
	char		edge_name[32] ;

	v1 = lbinfo->brd_graph_link ; /* usually Non Node Brds */
	v2 = lbinfo->brd_parent->brd_graph_link ; /* Usually, a node brd */

        graph_err = graph_info_get_LBL(prom_graph_hdl, v1,
             				INFO_LBL_KLCFG_INFO, NULL,
					(arbitrary_info_t *)&aip1) ;
	if (graph_err != GRAPH_SUCCESS) {
		printf("Get Label v1 failed in link_2_boards, grapherr=%d\n", graph_err) ;
		return 0 ;
	}
        graph_err = graph_info_get_LBL(prom_graph_hdl, v2,
             				INFO_LBL_KLCFG_INFO, NULL,
					(arbitrary_info_t *)&aip2) ;
	if (graph_err != GRAPH_SUCCESS) {
		printf("Get Label v2 failed in link_2_boards, graph_err=%d\n", graph_err) ;
		return 0 ;
	}

	PG_DPRINT("v1 = %d, a1 = %lx, v2 = %d, a2 = %lx\n", 
			v1, aip1, v2, aip2) ;
	PG_DPRINT("lbptrs - 1 = %lx, P = %lx\n", lbinfo, lbinfo->brd_parent) ;

	/* If the target to which we are going is a XTALK brd,
	   use widget instead of actual component name. */

	if (aip1->struct_type == KLSTRUCT_BRI) { /* XXX and any others */
		v3 = create_graph_path(v2, EDGE_LBL_XTALK, 0) ;
		sprintf(edge_name, "%d\0", aip1->widid) ;

		PG_DPRINT("Add edge %s, from %s%x --> %s%x\n", edge_name, 
				compt_names[aip2->struct_type], aip2->virtid,
				compt_names[aip1->struct_type], aip1->virtid) ;
		PG_DPRINT("Phys ids: v2 = %x, v1 = %x\n", 
				aip2->physid, aip1->physid) ;

        	graph_err = graph_edge_add(prom_graph_hdl, 
             			v3, v1, edge_name) ;
		if (graph_err != GRAPH_SUCCESS) {
			printf("%s link2 v3 to v1 edgeadd error %d\n", 
					edge_name, graph_err) ;
			return 0 ;
		}

		add_io_edge(lbinfo) ;
	} else
	if ((aip1->struct_type == KLSTRUCT_GFX) || 
	    (KLCF_CLASS(lbinfo) == KLCLASS_PSEUDO_GFX)) {
		add_io_edge(lbinfo) ;
	}
	else {
		get_edge_name(aip1, edge_name, 1);

		PG_DPRINT("Add edge %s, from %s%x --> %s%x\n", edge_name, 
				compt_names[aip2->struct_type], aip2->virtid,
				compt_names[aip1->struct_type], aip1->virtid) ;

        	graph_err = graph_edge_add(prom_graph_hdl, 
             		 	           v2, v1, edge_name) ;
		if (graph_err != GRAPH_SUCCESS) {
			printf("%s link2 v2 to v1 edgeadd error %d\n", 
						edge_name, graph_err) ;
			return 0 ;
		}
	}
		
	return 1 ;
}

void
prom_install_devices(void)
{
        vertex_hdl_t    next_vertex_hdl;
        graph_error_t   graph_err ;

        graph_vertex_visit(prom_graph_hdl, vvisit_install, &hw_vertex_hdl,
                                &graph_err, &next_vertex_hdl) ;
		
}

int
vvisit_install(void *arg, vertex_hdl_t vhdl)
{
	prom_dev_info_t	*arbinf_ptr ;
	klinfo_t	*kl_comp;
        graph_error_t   graph_err ;

	/* Init all controller numbers and any driver specific 
	   globals here. */

#ifdef SABLE
	num_qlogic = 0 ;
	num_sbldsk = 0 ;
	num_ioc3   = 0 ;
	ioc3_installed = 0 ;
#endif
	/* Do we have a dev_info label associated with this vertex? */

        graph_err = graph_info_get_LBL(prom_graph_hdl, vhdl,
                INFO_LBL_DEV_INFO, NULL, (arbitrary_info_t *)&arbinf_ptr) ;
	if (graph_err == GRAPH_SUCCESS) {
		/* Is there a valid install routine */
		/* Is it OK to call the install routine */

                if ((inst_valid(arbinf_ptr->install)) &&
		   (((klinfo_t *)arbinf_ptr->kl_comp)->flags & KLINFO_INSTALL)
		 &&(((klinfo_t *)arbinf_ptr->kl_comp)->flags & KLINFO_ENABLE))
		{
			/* Can this be moved to init? XXX */
			if (((klinfo_t *)arbinf_ptr->kl_comp)->struct_type
				== KLSTRUCT_SCSI)
			    ((klscsi_t *)arbinf_ptr->kl_comp)->scsi_numdevs = 0;
			arbinf_ptr->install(*(vertex_hdl_t *)arg, vhdl) ;
		}
	}
	return 0 ;
}

int pass ;

void
kl_init_devices(void)
{
        vertex_hdl_t    next_vertex_hdl;
        graph_error_t   graph_err ;

	if (!_symmon)
		printf("Initializing PROM Device drivers ..........\t\t") ;

	pass ++ ;
        graph_vertex_visit(prom_graph_hdl, vvisit_init, &hw_vertex_hdl,
                                &graph_err, &next_vertex_hdl) ;

	if (!_symmon)
		printf("DONE\n") ;
}

int
vvisit_init(void *arg, vertex_hdl_t vhdl)
{
	prom_dev_info_t		*arbinf_ptr ;
        register struct eiob 	*io;
	klinfo_t 		*kl_comp;
	graph_error_t		graph_err ;

        graph_err = graph_info_get_LBL(prom_graph_hdl, vhdl,
             				INFO_LBL_DEV_INFO, NULL,
					(arbitrary_info_t *)&arbinf_ptr) ;
	if (graph_err == GRAPH_SUCCESS) {
		kl_comp = (klinfo_t *)(arbinf_ptr->kl_comp);
		if ((kl_comp->flags & KLINFO_CONTROLLER) && 
						(arbinf_ptr->driver)) {
                	if ((io = new_eiob()) == NULL) {
				printf("vvinit: Out of eiobs\n") ;
                        	return 1;
			}

                	io->iob.Controller   = kl_comp->virtid ;
                	io->iob.FunctionCode = FC_INITIALIZE;
                	io->iob.Count        = pass ;

                	if ((*arbinf_ptr->driver)
			    (kl_comp->arcs_compt, &io->iob) != 0) {
                        	arbinf_ptr->flags |= Failed;

				/* XXX verify that driver fills up
				   arcs_compt->Flags |= Failed */
                	}
			if (arbinf_ptr->driver == dksc_strat)
				tpsc_strat(kl_comp->arcs_compt, &io->iob) ;
                	free_eiob(io);
		}
	}

	return 0 ; /* continue for all nodes */

}

int
get_locator_id(klinfo_t *klcompt)
{
	int rv ;

	switch(klcompt->struct_type)
	{
		case KLSTRUCT_DISK:
		case KLSTRUCT_TAPE:
		case KLSTRUCT_CDROM:
		case KLSTRUCT_IOC3ENET:
		case KLSTRUCT_IOC3UART:
			rv = (klcompt->virtid * 16 + klcompt->physid) ;
		break;

		case KLSTRUCT_IOC3_TTY:
			rv = klcompt->virtid ;
		break ;

		default:
			rv = (klcompt->nasid * 16 + klcompt->physid) ;
		break ;
	}
	return rv ;
}

/*
 * Get the name of the edge to be used in edge add. This is derived
 * based on the pre-defined component name based on component type
 * and the virtual id of the component in the klinfo_t structure.
 */
void
get_edge_name(klinfo_t *klcompt, char *buf, int flag)
{
	char  	tmp_buf[32] ;
	int	virtid ;  	/* Virtual id of the component */

	if (klcompt->struct_type > (sizeof(compt_names)/sizeof(char *))) {
		*buf = 0 ;
		return ;
	}

	switch(flag) {
		case 0:
			sprintf(buf, "%d\0", klcompt->physid) ;
		break ;

		case 1:
			strcpy(buf, compt_names[klcompt->struct_type]) ;
		break ;

		case 2:
			strcpy(tmp_buf, compt_names[klcompt->struct_type]) ;
			strcat(tmp_buf, "%d\0") ;
			virtid = get_locator_id(klcompt) ;
			sprintf(buf, tmp_buf, virtid) ;
		break ;

		default:
			*buf = 0 ;
		break ;
	}

	return ;
}

/*
 * Creates the necessary data structures for a device discovered 
 * by the device driver. Covers all devices we are currently aware
 * of. Returns a pointer to the actual device type, for example,
 * klioc3_t * for IOC3, klscdev_t * for scsi devices etc.
 */

#define MAX_SCSI_DEV_PROBE	40

klinfo_t *
init_device_graph(vertex_hdl_t devctlr_vhdl, int dev_type)
{
        graph_error_t   graph_err ;
	klinfo_t	*ctrkli_ptr;
        klioc3_t        *ioc3kli_ptr ;
	klinfo_t	*devkli_ptr;

        /* First get the controller's klinfo_t */

        graph_err = graph_info_get_LBL(prom_graph_hdl, devctlr_vhdl, 
					INFO_LBL_KLCFG_INFO,NULL,
                        		(arbitrary_info_t *)&ctrkli_ptr) ;
        if (graph_err != GRAPH_SUCCESS) {
                printf("init_dev_graph: getLBL e %d t %d\n", graph_err, dev_type) ;
                return NULL ;
        }

	/* Return the controller klinfo ptr if dev_type does
	   not match. XXX this causes probs in case of unknown devs.
	*/

        devkli_ptr = ctrkli_ptr ;

	/* call the respective init_klcfg based on dev_type */

	switch (dev_type) {
		/* XXX assuming that IOC3 has been inited */
		case KLSTRUCT_IOC3ENET:
			devkli_ptr=(klinfo_t *)init_klcfg_enet(
					(klioc3_t *)ctrkli_ptr) ;
		break ;
		case KLSTRUCT_IOC3UART:
		case KLSTRUCT_HUB_UART:
			devkli_ptr=(klinfo_t *)init_klcfg_tty((klioc3_t *)ctrkli_ptr) ;
		break ;
                case KLSTRUCT_IOC3PCKM:
                        devkli_ptr = (klinfo_t *)init_klcfg_kbd((klioc3_t *)ctrkli_ptr, dev_type) ;
		break ;

                case KLSTRUCT_IOC3MS:
                        devkli_ptr = (klinfo_t *)init_klcfg_ms((klioc3_t *)ctrkli_ptr) ;
                break ;

		case KLSTRUCT_DISK:
		case KLSTRUCT_CDROM:
		case KLSTRUCT_TAPE:

			/* XXX This can fillup the arcs stuff too based
			   on device type. */

		/* Probing all buses here, leaves klcfg memory
		 * exhausted. And there is none left for symmon
		 * which panics. So limit number of buses even
	 	 * if probeall scsi is set. */

			if (num_scsi_drive < MAX_SCSI_DEV_PROBE) {
			devkli_ptr=(klinfo_t *)init_klcfg_scsi_drive(
					(klscsi_t *)ctrkli_ptr, dev_type) ;
				num_scsi_drive++ ;
			} else
				devkli_ptr = NULL ;
		break ;

		default:
			printf("Invalid device type 0x%x!!\n", dev_type);
                return NULL ;
	}

	if (devkli_ptr)
		devkli_ptr->arcs_compt = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
	return(devkli_ptr) ;
}

link_device_to_graph(vertex_hdl_t hw_vhdl, vertex_hdl_t devctlr_vhdl, 
		klinfo_t *devkli_ptr, int(*dev_strat)())
{
        char            locedge_name[32], ctlredge_name[32], *remain ;
	char		tmp_buf[32] ;
        graph_error_t   graph_err ;
        vertex_hdl_t    newdev_vhdl, locator_vhdl, link_vhdl ;
	klinfo_t 	*ctrkli_ptr ;
        int             i = 0 ;
	prom_dev_info_t	*dev_info;
	klinfo_t	*tmpkli_ptr ;
        graph_edge_place_t      eplace = GRAPH_EDGE_PLACE_NONE ;
	char	*ai ;

	strcpy(locedge_name, "/dev/") ;

        switch(devkli_ptr->struct_type) {
                case KLSTRUCT_HUB_UART:
                case KLSTRUCT_IOC3UART:
                case KLSTRUCT_HUB_TTY:
                case KLSTRUCT_IOC3_TTY:
                        strcat(locedge_name, "tty") ;
                break ;
                case KLSTRUCT_IOC3ENET:
                        strcat(locedge_name, EDGE_LBL_ENET) ;
                break ;
                case KLSTRUCT_DISK:
                        strcat(locedge_name, "disk") ;
                break ;
                case KLSTRUCT_IOC3PCKM:
                case KLSTRUCT_IOC3MS:
                        strcat(locedge_name, "input");
                break ;
                default:
                        strcat(locedge_name, 
				compt_names[devkli_ptr->struct_type]);
                break ;
        }

        graph_err = hwgraph_path_lookup(hw_vhdl, locedge_name,
          				&locator_vhdl, &remain) ;
	if (graph_err != GRAPH_SUCCESS) {
                printf("dev_vhdl lookup failed %d %s\n", graph_err, remain) ;
                return 0 ;
        }

        graph_err = graph_info_get_LBL(prom_graph_hdl, devctlr_vhdl, 
				       INFO_LBL_LINK, NULL,
				       (arbitrary_info_t *)&ai) ;
        if (graph_err != GRAPH_SUCCESS) {
                /* printf("get link LBL failed %d\n", graph_err) ; */
		link_vhdl = devctlr_vhdl ;
        }
	else {
        	graph_err = hwgraph_path_lookup(devctlr_vhdl, ai,
                        			&link_vhdl, &remain) ; 
		if (graph_err != GRAPH_SUCCESS) {
                	printf("link lookup err %d %s\n", graph_err, remain) ;
                	return 0 ;
        	}
	}

        /* Create new vertex and Fill up label INFO_LBL_KLCFG_INFO */

	newdev_vhdl = create_device_graph(link_vhdl, devkli_ptr, dev_strat) ;

        /* Create 3 path names */

        get_edge_name(devkli_ptr, locedge_name, 2) ;
	i = devkli_ptr->virtid ; /* save virtid */
	devkli_ptr->virtid = devkli_ptr->physid ; /* XXX right value ?? */
        get_edge_name(devkli_ptr, ctlredge_name, 1) ;
	devkli_ptr->virtid = i ; 

        /* Locator substring to device */

        graph_err = graph_edge_add(prom_graph_hdl, locator_vhdl, 
				   newdev_vhdl, locedge_name) ;
        if (graph_err != GRAPH_SUCCESS) {
                printf("locator device edge %s add err %d\n", 
				locedge_name, graph_err) ;
        }

        return 0 ;
}

vertex_hdl_t
create_device_graph(vertex_hdl_t link_vhdl, klinfo_t *kp, int(*dev_strat)())
{
	vertex_hdl_t 	vhdl ;
	graph_error_t	graph_err ;
	char		tmp_buf[32] ;
	prom_dev_info_t *dev_info ;

	if (kp->struct_type == KLSTRUCT_DISK) {
		sprintf(tmp_buf, "%d\0", kp->physid) ;
		/* switch (kp->arcs_compt->Type) disk, tape, ... XXX */
		strcat(tmp_buf, PROM_EDGE_LBL_LUN_0_DISK) ;
		vhdl = create_graph_path(link_vhdl, tmp_buf, 1) ;
	}
	else if (kp->struct_type == KLSTRUCT_CDROM) {
		vertex_hdl_t	vhdl1 ;

                sprintf(tmp_buf, "%d\0", kp->physid) ;
                strcat(tmp_buf, "/lun/0") ;
                vhdl1 = create_graph_path(link_vhdl, tmp_buf, 1) ;
		vhdl = create_graph_path(vhdl1, "cdrom", 1) ;
		graph_err=graph_edge_add(prom_graph_hdl, vhdl1, vhdl, "disk") ;
		if (graph_err != GRAPH_SUCCESS)
			printf("Add disk to cdrom fail %d\n", graph_err) ;
	}
        else if (kp->struct_type == KLSTRUCT_TAPE) {
                sprintf(tmp_buf, "%d\0", kp->physid) ;
                strcat(tmp_buf, PROM_EDGE_LBL_LUN_0_TAPE) ;
                vhdl = create_graph_path(link_vhdl, tmp_buf, 1) ;
        }
        else if (kp->struct_type == KLSTRUCT_IOC3_TTY) {
		get_edge_name(kp, tmp_buf, 2) ;
                vhdl = create_graph_path(link_vhdl, tmp_buf, 1) ;
        }
	else {
		get_edge_name(kp, tmp_buf, 1) ;
		vhdl = create_graph_path(link_vhdl, tmp_buf, 1) ;
	}

        graph_err = graph_info_add_LBL(prom_graph_hdl, vhdl,
                  			INFO_LBL_KLCFG_INFO, NULL,
					(arbitrary_info_t)kp) ;
	if (graph_err != GRAPH_SUCCESS) {
                printf("dev info addLBL error %d\n", graph_err) ;
                return 0 ;
        }

	dev_info = malloc(sizeof(prom_dev_info_t));
	if (dev_info == NULL) {
		printf("link device: Cannot allocate prom_dev_info\n");
		return 0;
	}
	bzero(dev_info, sizeof(prom_dev_info_t)) ;

	graph_err = graph_info_add_LBL(prom_graph_hdl, vhdl,
				       INFO_LBL_DEV_INFO, NULL,
		     		  	(arbitrary_info_t)dev_info);
	if (graph_err != GRAPH_SUCCESS) {
		printf("add devinfo failed %d\n", graph_err) ;
		return 0 ;
	}

	/* Just register the driver for the device */
	/* This is the same as the driver for the controller */

	dev_info->kl_comp = kp ;
        kl_reg_drv_strat(dev_info, dev_strat) ;

	return (vhdl) ;
}


/* Find the address of the driver routine given the sa_pci_ident_t */

__psunsigned_t
find_pci_driver(sa_pci_ident_t *pi)
{
	sa_pci_ident_t 	*tpi ;
	tpi = (sa_pci_ident_t *)prom_pci_info ;
	while (tpi->vendor_id) /* entry valid */
		if ((tpi->vendor_id == pi->vendor_id) &&
		    (tpi->device_id == pi->device_id))
		/* Ignore Revision for now. */
			return((__psunsigned_t)tpi->inst_fptr) ;
		else
			tpi ++ ;
	return(NULL) ;
}


void 
setup_pci_identity_info(klinfo_t *klcompt, sa_pci_ident_t *tpi)
{
	pcicfg32_t	*pci_cfg_dev_ptr ;			
	__uint64_t	pci_key;

	if (klcompt->struct_type == KLSTRUCT_HUB_UART) {
		tpi->vendor_id = 0x0300;
		tpi->device_id = 0xfafd;
		tpi->revision_id = 10;
		return;
	}

	if (klcompt->nic == 0xABfEDDDD) { /* SABLE disk magic*/
		tpi->vendor_id = 0x0300 ;
		tpi->device_id = 0xfafe ;
		tpi->revision_id = 10 ;
	} else {
		pci_key = MK_SN0_KEY(klcompt->nasid, 
				   klcompt->widid, klcompt->physid);
		
		pci_cfg_dev_ptr = 
		    (pcicfg32_t *)GET_PCICFGBASE_FROM_KEY(pci_key);

		tpi->vendor_id = (pci_cfg_dev_ptr->pci_id >> 16);
		tpi->device_id = (pci_cfg_dev_ptr->pci_id & 0xffff);
		tpi->revision_id = 10 ;
	}
	return;
}

/* Check if the function pointer is a valid install routine */

inst_valid(int (*fptr)())
{
	sa_pci_ident_t 	*tpi  = (sa_pci_ident_t *)prom_pci_info ;
	
	while (tpi->vendor_id) /* entry valid */
		if(tpi->inst_fptr == fptr)
			return 1 ;
		else
			tpi ++ ;

	return(0) ;
}

#ifdef SABLE
int
promgraph_setup_ip27_devices(lboard_t *brd_ptr,
			     nasid_t nasid, 
			     vertex_hdl_t *vert)
{
	register int	i;
	klinfo_t	*klcompt;
	prom_dev_info_t	*dev_info;
	graph_error_t	graph_err;
	sa_pci_ident_t pi ;
	char		tmp_buf[32] ;

        for (i = 0; i < brd_ptr->brd_numcompts; i++) {
		klcompt = 
		    (klinfo_t *)(NODE_OFFSET_TO_K1(nasid, 
					   brd_ptr->brd_compts[i]));

		switch(klcompt->struct_type) {
			case KLSTRUCT_HUB_UART:
			case KLSTRUCT_HUB_TTY:
				dev_info = malloc(sizeof(prom_dev_info_t));
				if (dev_info == NULL) {
					printf("promgraph_setup_ip27_devices: \
				Cannot allocate prom_dev_info for hub uart\n");
					return 0;
				}
				setup_pci_identity_info(klcompt, &pi);

				dev_info->install = (int (*)())
							find_pci_driver(&pi);
				if (nasid == 0)
					dev_info->install = (int (*)())
							kl_hubuart_install ;
				dev_info->kl_comp = klcompt;
				dev_info->flags = 0;
				dev_info->driver = NULL;
				klcompt->virtid = nasid ;

				graph_err = 
				graph_info_add_LBL(prom_graph_hdl, 
						   vert[i],
						   INFO_LBL_DEV_INFO, NULL,
						   (arbitrary_info_t)dev_info);
				if (graph_err != GRAPH_SUCCESS) {
					printf("promgraph_setup_ip27_devices: \
						cannot add dev_info label\n");
					return 0;
				}
			break;

			default:
			break;
		}
	}
	return 1;
}
#endif

/*
 * promgraph_setup_xtalk_devices
 * Setup the device drivers for all controllers found in the system.
 * This routine has become complicated due to various requirements.
 * - do not setup drivers for disk if we are symmon.
 * - If ProbeAllScsi is set to y then probe all boards/scsi buses.
 * - do not setup drivers for any but the first/master IO6 board.
 * - do not setup drivers for controllers disabled by the disable cmd.
 */

int
promgraph_setup_xtalk_devices(lboard_t *lb,
                              nasid_t  n,
                              vertex_hdl_t *vert)
{
    klinfo_t		*k ;
    prom_dev_info_t 	*d ;
    sa_pci_ident_t 	pi ;
    graph_error_t       gerr;
    int			i ;

    /* check nvram vars */

    int    		mod, slot, ms ;
    char   		*p = NULL ;

    mod  = slot  = 0 ;
    num_scsi_drive = 0 ;

    if ((p=getenv("ProbeAllScsi")) && p && ((*p == 'y') || (*p == 'Y'))) {
	mod = slot = -1 ;
    } else if ((p=getenv("ProbeWhichScsi")) && p) {
	if ((ms = check_console_path(p)) > 0) {
	    mod  = ms >> 16 ;
	    slot = ms & 0xffff ;
	}
    }

    /* Handle known compts with drivers. Skip the rest */
    for (i = 0; i < lb->brd_numcompts; i++) {
	k = (klinfo_t *)(NODE_OFFSET_TO_K1(n,
                        	lb->brd_compts[i]));
        /* Skip disabled components */
        if (!(k->flags & KLINFO_ENABLE))
            continue ;

	switch (k->struct_type) {
	    case KLSTRUCT_SCSI:
		/* 
		   The order of these condition checks is very
		   important. Disturbing this has disastrous
		   consequences.
		*/

		if ((_symmon) || (p && (*p == 'X')))
		    continue ;

		if (mod == -1)   /* ProbeAllScsi */
		    break ;

		if ((!_symmon) && /* master_baseio_modid invalid in symmon */
		    (lb->brd_type   == KLTYPE_BASEIO) &&
		    (lb->brd_module == master_baseio_modid) &&
		    (lb->brd_slot   == master_baseio_slotid))
			break ;
		else if (mod > 0) {   /* ProbeWhichScsi */
		    /* Check if specified path is already master */
		    if ((mod  == master_baseio_modid) &&
                        (slot == master_baseio_slotid))
			continue ;

		    if ((lb->brd_module == mod) && 
			((SLOTNUM_GETSLOT(lb->brd_slot)) == slot))
		    	break ;
		    else
			continue ;
		} else /* not master and module == 0 */
		    continue ;

/* NOTREACHED */
	    break ;

	    case KLSTRUCT_IOC3:
		if (lb->brd_type   == KLTYPE_BASEIO)
		    break ;
		else
		    continue ;
/* NOTREACHED */
	    break ;

	    case KLSTRUCT_GFX:
	    case KLSTRUCT_PCI:
		if (_symmon) 
		    continue ;

	    break ;

	    default :
		continue ;
	}

	/* Install the driver */
        d = malloc(sizeof(prom_dev_info_t));
        if (d == NULL) {
            printf("promgraph_setup_xtalk_devices: \
                    Cannot allocate prom_dev_info for %d.\n",
                    k->struct_type);
            return 0;
        }

	if ((k->struct_type == KLSTRUCT_GFX) ||
	    (KLCF_CLASS(lb) == KLCLASS_PSEUDO_GFX)) {
            d->install = (int (*)())NULL;
	} else {
            setup_pci_identity_info(k, &pi);
            d->install = (int (*)())find_pci_driver(&pi);
	}
        d->kl_comp 	= k;
	d->lb		= lb ;
        d->flags 	= 0;

        d->driver = NULL;
        d->modid = lb->brd_module ;
        d->slotid = lb->brd_slot ;
        k->arcs_compt = (COMPONENT *) malloc(sizeof(COMPONENT)) ;

        gerr = graph_info_add_LBL(prom_graph_hdl, vert[i],
                                INFO_LBL_DEV_INFO, NULL,
                                (arbitrary_info_t)d);
        if (gerr != GRAPH_SUCCESS) {
            printf("promgraph_setup_xtalk_devices: \
                    cannot add dev_info label for %d, %d",
                    k->struct_type, gerr);
            return 0;
        }
    }
    return 1;
}


int
promgraph_connect_brd_compts(lboard_t *brd, 
		    unsigned char type, vertex_hdl_t *compt_vert, int flag)
{
	int i;
	klinfo_t *klcompt;
	int main_ndx = -1;
	vertex_hdl_t 	bridge_vhdl, hubcpu_vhdl, v2 ;
	graph_error_t 	graph_err ;
	int		saveflag = flag ;

	bridge_vhdl = 0 ;
	v2 = 0 ;
	for (i = 0; i < KLCF_NUM_COMPS(brd); i++) {
		if (KLCF_COMP_TYPE(KLCF_COMP(brd, i)) == type) {
			brd->brd_graph_link = compt_vert[i];
			main_ndx = i;
			if (type == KLSTRUCT_BRI) {
				v2 = create_graph_path(compt_vert[i], 
						       PROM_EDGE_LBL_PCI_SLOT, 1) ;
				bridge_vhdl = compt_vert[i] ;
				compt_vert[i] = v2 ;
			}
			if (type == KLSTRUCT_HUB) {
				v2 = create_graph_path(compt_vert[i], 
						       PROM_EDGE_LBL_CPU, 1) ;
				hubcpu_vhdl = v2 ;
			}
			break;
		}
	}

	if (main_ndx == -1) {
		printf("promgraph_connect_brd_components: \
			cannot find type %x for brd %x\n", type, brd->brd_type);
		return 0;
	}
	
	for (i = 0; i < KLCF_NUM_COMPS(brd); i++) {
		klcompt = KLCF_COMP(brd, i);
		if (i != main_ndx) {
			vertex_hdl_t	tmp_vhdl ;

			if (klcompt->struct_type == KLSTRUCT_CPU) {
				tmp_vhdl = hubcpu_vhdl ;
				flag = 0 ;
			}
			else {
				tmp_vhdl = compt_vert[main_ndx] ;
				flag = saveflag ;
			}

			if (promgraph_connect(KLCF_COMP(brd, i),
					      KLCF_COMP(brd, main_ndx),
					      tmp_vhdl,
					      compt_vert[i], flag) == 0)
			    return 0;
		}
		
		switch (KLCF_COMP_TYPE(klcompt)) {
		case KLSTRUCT_HUB:
			/* locator edge /hw/hub to hub */
			if (promgraph_edge_add(klcompt, 
				hw_locator_edges[HUB_LOCATOR_INDEX].vhdl,
					       compt_vert[i], 2) == 0)
			    return 0;
			break;
		case KLSTRUCT_CPU:
			/* locator edge /hw/cpu to cpu */
			if (promgraph_edge_add(klcompt, 
			       hw_locator_edges[CPU_LOCATOR_INDEX].vhdl,
					       compt_vert[i], 2) == 0)
			    return 0;
			break;

		case KLSTRUCT_BRI:
			if (promgraph_edge_add(klcompt,
			     	hw_locator_edges[BRIDGE_LOCATOR_INDEX].vhdl,
			        bridge_vhdl, 2) == 0)
			    return 0;
			break;
			
		case KLSTRUCT_XBOW:
			if (promgraph_edge_add(klcompt,
				hw_locator_edges[XBOW_LOCATOR_INDEX].vhdl,
			       	compt_vert[i], 2) == 0)
			    return 0;
			break;
			
		case KLSTRUCT_ROU:
			if (promgraph_edge_add(klcompt,
				hw_locator_edges[ROUTER_LOCATOR_INDEX].vhdl,
			       	compt_vert[i], 2) == 0)
			    return 0;
			break;

		case KLSTRUCT_TPU:
			if (promgraph_edge_add(klcompt,
				hw_locator_edges[TPU_LOCATOR_INDEX].vhdl,
			       	compt_vert[i], 2) == 0)
			    return 0;
			break;

		case KLSTRUCT_GSN_A:
		case KLSTRUCT_GSN_B:
			if (promgraph_edge_add(klcompt,
				hw_locator_edges[GSN_LOCATOR_INDEX].vhdl,
			       	compt_vert[i], 2) == 0)
			    return 0;
			break;

			/* add other interesting types here */
		default:
			break;
		}
	}
	
	if (klcompt->struct_type == KLSTRUCT_BRI)
		compt_vert[main_ndx] = bridge_vhdl ;

	return 1;
}

/*
 * Create a set of nodes and link them up with the given
 * path name.
 * If errflg is set report any error, else if an edge already
 * exists, dont report error but return the existing edge as 
 * if it was just now created.
 */
vertex_hdl_t
create_graph_path(vertex_hdl_t start_vhdl, char *path, int errflg)
{
	vertex_hdl_t 	v1, v2 ;
	graph_error_t	graph_err ;
	char		tmp_buf[32], *next ;

	if (!(*path)) return 0 ; /* Null path */

	v2 = start_vhdl ;
	PG_DPRINT("crgfpth: path is %s\n",path);
	do {
		if ((next = index(path, '/'))!=0) {
			strncpy(tmp_buf, path, next-path) ;
			tmp_buf[next-path] = 0 ;
			path = next+1 ;
		}
		else 
			strcpy(tmp_buf, path) ;
		PG_DPRINT("crgfpth: tmp_buf is %s\n",tmp_buf);		
        	graph_err = graph_vertex_create(prom_graph_hdl, &v1) ;
        	if (graph_err != GRAPH_SUCCESS) {
                	printf("crgfpth: %s create err %d\n", path, graph_err) ;
                	return 0 ;
        	}
        	graph_err = graph_edge_add(prom_graph_hdl, v2, v1, tmp_buf) ;
        	if (graph_err != GRAPH_SUCCESS) {
		    if (errflg) {
reporterr:
                	printf("crgfpth: %s add err %d \n", path, graph_err) ;
                	return 0 ;
		    }
		    else { /* dont report error if GRAPH_DUP only */
			if (graph_err != GRAPH_DUP)
				goto reporterr ;
			else {
				graph_vertex_destroy(prom_graph_hdl, v1) ;
				if (graph_edge_get(prom_graph_hdl,
					       v2, tmp_buf, &v1) != GRAPH_SUCCESS) {
					printf("Dup Edge %s not found\n", tmp_buf) ;
					return 0;
				}
		 	}
		    }
        	}
		v2 = v1 ;
	}
	while (next) ;

	return (v1) ;

}

/*
 * This routine manages a linked list of install routines.
 * It is used to change the order of device driver install.
 * Normally the drivers are installed as and when they are
 * 'discovered' or in random order.
 * For example, if we want to install all scsi drivers after
 * all devices, then scsi_install registers itself first
 * and gets called later.
 */

void 
kl_register_install(vertex_hdl_t hw_vhdl, vertex_hdl_t loc_vhdl, 
				void (*install)())
{
        struct inst_later *ilptr;

	/* Allocate Q element */

        if (!(ilptr = (struct inst_later *)malloc(sizeof(*ilptr))))
                panic("cannont malloc space for RegisterInstall node\n");

	/* Fill up Q element */

        ilptr->next = (struct inst_later *)NULL;
        ilptr->hw = hw_vhdl ;
        ilptr->loc = loc_vhdl ;
        ilptr->install = install;

	/* Add to Q */

        if (!instnext.head) {
                instnext.head = instnext.tail = ilptr;
        }
        else {
                instnext.tail->next = ilptr;
                instnext.tail = ilptr;
#if 0 
/* This will register devices in the order received. 
   This had to be changed to let qlogics install in the 
   reverse order received. BUG 376780
*/
		ilptr->next = instnext.head ;
		instnext.head = ilptr ;
#endif
        }
}

int 	
pg_setup_bridge_alias()
{
        graph_error_t   graph_err ;
        vertex_hdl_t    xwidget_vhdl, pci_vhdl = GRAPH_VERTEX_NONE, 
			next_vhdl ;
        graph_edge_place_t      eplace = GRAPH_EDGE_PLACE_NONE ;
	char                    *remain ;
        char            widget_name[64] ;
	char		tmp_buf[32] ;
	int		ab_num = 1, dm_num = 2, dm_tmp = 0 ;
	int 		bridge_found = 0, i ; 
	lboard_t	*brd_ptr ;

	/* Lookup "/xwidget" */

        graph_err = hwgraph_path_lookup(hw_vertex_hdl, EDGE_LBL_ROOT_XWIDGET,
                                        &xwidget_vhdl, &remain) ;
        if (graph_err != GRAPH_SUCCESS) {
	    printf("pg_setup_bridge_alias: Cannot find edge /xwidget %d\n", 
			graph_err) ;
	    return 0 ;
	}

        do {
                graph_err = graph_edge_get_next(prom_graph_hdl,
                                                xwidget_vhdl,
                                                widget_name,
                                                &next_vhdl, &eplace) ;
                if (graph_err != GRAPH_SUCCESS) {
                        break ;
                }

		/* For all edges that begin with "bridge" */

		if (strncmp(widget_name, "bridge", 6)) 
				continue ;
                graph_err = graph_info_get_LBL(prom_graph_hdl, next_vhdl,
                                        INFO_LBL_BRD_INFO, NULL,
                                        (arbitrary_info_t *)&brd_ptr) ;
                if (graph_err != GRAPH_SUCCESS) {
	    		printf("pg_setup_bridge_alias: Cannot find brdinfo %d\n", 
			graph_err) ;
	    		return 0 ;
		}

		/* for all baseio boards */
		if (brd_ptr->brd_flags & GLOBAL_MASTER_IO6) {

			/* Setup an edge "alias_bridge0" */

        		graph_err = graph_edge_add(prom_graph_hdl,
                                   		xwidget_vhdl,
                                   		next_vhdl ,
                                   		"alias_bridge0") ;
        		if (graph_err != GRAPH_SUCCESS) {
               			printf("alias_bridge0 edge add err %d\n", 
					graph_err) ;
               			return 0 ;
        		}
			dksc_map[0] = dksc_map[1] = 0 ;
			dm_tmp = 0 ;
		}
		else  { /* BASEIO or MSCSI or MENET */
			sprintf(tmp_buf, "alias_bridge%d", ab_num) ;
        		graph_err = graph_edge_add(prom_graph_hdl,
                                   		xwidget_vhdl,
                                   		next_vhdl ,
                                   		tmp_buf) ;
        		if (graph_err != GRAPH_SUCCESS) {
               		    printf("%s edge add err %d\n", tmp_buf, graph_err) ;
               			return 0 ;
        		}

                	if (brd_ptr->brd_type == KLTYPE_MSCSI) {
				for (i=0; i<4; i++)
					dksc_map[dm_num+i] = ab_num ;
				dm_tmp = dm_num ;
				dm_num += 4 ;
			}
			else
                	if (brd_ptr->brd_type == KLTYPE_BASEIO) {
				for (i=0; i<2; i++)
					dksc_map[dm_num+i] = ab_num ;
				dm_tmp = dm_num ;
				dm_num += 2 ;
			}
			ab_num++ ;
		}
                if ((brd_ptr->brd_type == KLTYPE_4CHSCSI) ||
		    (brd_ptr->brd_type == KLTYPE_BASEIO)) {

        		graph_err = hwgraph_path_lookup(next_vhdl, "pci",
                                        	&pci_vhdl, &remain) ;
        		if (graph_err != GRAPH_SUCCESS) {
	    			printf("pg_setup_bridge_alias: pci-slot not found %d\n", 
				graph_err) ;
	    			return 0 ;
			}

			/* XXX Call this routine only for the master io6 */
			pg_setup_compt_alias(pci_vhdl, 
				brd_ptr, dm_tmp) ;
		}
		
        } while (graph_err == GRAPH_SUCCESS) ;

	return 1 ;
}



int
pg_setup_compt_alias(vertex_hdl_t 	bvh, lboard_t *lb, int map_ind)
{
        graph_error_t   	graph_err ;
        vertex_hdl_t    	next_vhdl ;
        graph_edge_place_t      eplace = GRAPH_EDGE_PLACE_NONE ;
	char                    *remain, *name ;
        char            	widget_name[64], buf[32] ;
	int 			master_ioc3_found = 0 ; 
	prom_dev_info_t 	*arbinf_ptr ;
	extern int symmon;

        do {
                graph_err = graph_edge_get_next(prom_graph_hdl,
                                                bvh,
                                                widget_name,
                                                &next_vhdl, &eplace) ;
                if (graph_err != GRAPH_SUCCESS) {
                        break ;
                }

		/* Skip duplicate edges */

		if (!strncmp(widget_name, "alias", 5) ||
		    !strncmp(widget_name, "master", 6))
			continue ;

        	graph_err = graph_info_get_LBL(prom_graph_hdl, next_vhdl,
             				INFO_LBL_DEV_INFO, NULL,
					(arbitrary_info_t *)&arbinf_ptr) ;

			
		/* we may not find prom_dev_info for disabled controllers */
		/* continue if we don't */

		if (graph_err != GRAPH_SUCCESS) {
			graph_err = GRAPH_SUCCESS;
			continue ;
		}

		switch (((klinfo_t *)(arbinf_ptr->kl_comp))->struct_type) {
			case KLSTRUCT_IOC3 :
				if (!master_ioc3_found) {
					master_ioc3_found = 1 ;
					name = EDGE_LBL_MASTER_IOC3 ;
				}
				else {
					graph_err = GRAPH_SUCCESS;
					continue ;
				}
			break ;

			case KLSTRUCT_SCSI:
			{
				unsigned short vid ;
				vid = ((klinfo_t *)
					(arbinf_ptr->kl_comp))->virtid ;

				sprintf(buf, "alias_ql%d", vid) ;
				dksc_map[map_ind] |= (vid << 8) ;
				dksc_inv_map[vid] = map_ind ;
				map_ind++ ;
				name = buf ;
			}
			break ;

			default:
				continue ;
/* NOTREACHED */
			break ;

		}
		
        	graph_err = graph_edge_add(prom_graph_hdl,
                                   	bvh,
                                   	next_vhdl,
                                   	name) ;
        	if (graph_err != GRAPH_SUCCESS) {
               		printf("%s edge add err %d\n", name, graph_err) ;
               		return 0 ;
		}


        } while (graph_err == GRAPH_SUCCESS) ;

	if ((!master_ioc3_found) && (lb->brd_type == KLTYPE_BASEIO)) {
		/* No ioc3 on an io6 */
		return 0 ;
	}

	return 1 ;
}

#endif /* !SN_PDI */

#ifdef PG_DEBUG

static void
dump_graph_info(nasid_t nasid)
{
	lboard_t	*brd_ptr;
	int 		i ;
	klrou_t		*klroup ;

        brd_ptr = (lboard_t *)KL_CONFIG_INFO(nasid);

        while (brd_ptr)  {
                if (brd_ptr->struct_type == REMOTE_BOARD) {
			printf("Remote board in dump_graph_info: fixme\n");
                }
                else {
			if (brd_ptr->brd_flags & DUPLICATE_BOARD)
				;
			else if (brd_ptr->brd_type == KLTYPE_ROUTER) {
				klroup = (klrou_t *)NODE_OFFSET_TO_K1(
						nasid, brd_ptr->brd_compts[0]);
				for (i=1; i<=MAX_ROUTER_PORTS; i++)
					printf(" %d -> %d %x \n", i, 
					klroup->rou_port[i].port_nasid,
					klroup->rou_port[i].port_offset) ;
			}
                }
		brd_ptr = KLCF_NEXT(brd_ptr);
	}
}

#endif /* DEBUG */

