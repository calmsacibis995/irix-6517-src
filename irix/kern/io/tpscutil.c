#include <sys/types.h>
#include <sys/debug.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/iograph.h>
#include <ksys/hwg.h>
#include <sys/invent.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include <sys/scsi.h>
#include <string.h>
#include <bstring.h>

/*	determine which one of the supported drives we are looking at
	from the inquiry data.  This is needed for all the little
	idiosyncrasies...  Called from wd93.c during inventory setup, and
	from ctsetup() in the tpsc driver.
	The return value is the hinv type, for scsi.
*/
int
tpsctapetype(ct_g0inq_data_t *inv, struct tpsc_types *tpp)
{
	int i;
	struct tpsc_types *tpt;
	u_char *id = inv->id_vid;
	u_char *pid = inv->id_pid;
	extern struct tpsc_types tpsc_types[];
	extern struct tpsc_types tpsc_generic;
	extern int tpsc_numtypes;

	tpt = tpsc_types;
	if(inv->id_ailen == 0) {	/* hope only 540S... */
		id = (u_char *)"CIPHER";	/* fake it; must match master.d/tpsc */
		pid = (u_char *)"540S";	/* fake it; must match master.d/tpsc */
	}
	for(i=0; i<tpsc_numtypes; i++, tpt++) {
		if(strncmp((char *)tpt->tp_vendor, (char *)id, tpt->tp_vlen) == 0 &&
		   strncmp((char *)tpt->tp_product, (char *)pid, tpt->tp_plen) == 0)
			break;
	}
	if(i == tpsc_numtypes)
		tpt = &tpsc_generic; 
	if(tpp)
		bcopy(tpt, tpp, sizeof(*tpt));
	return tpt->tp_hinv;
}

/* make an alias name of the form 
 *	tps<ctlr_num>d<targ_num>l<lun_num> given a 
 * path name corresponding to a tape device
 */
int
tp_alias_make(vertex_hdl_t tape_vhdl,char *alias,char *suffix)
{

	scsi_unit_info_t	*unit_info = scsi_unit_info_get(tape_vhdl);
	
	/* for lun 0 there should not be an "l0" in the alias name */
	if (SUI_LUN(unit_info))
		sprintf(alias , "tps%dd%dl%d%s",
			device_controller_num_get(SUI_CTLR_VHDL(unit_info)),
			SUI_TARG(unit_info),
			SUI_LUN(unit_info),
			suffix);
	else
		sprintf(alias , "tps%dd%d%s",
			device_controller_num_get(SUI_CTLR_VHDL(unit_info)),
			SUI_TARG(unit_info),
			suffix);
	
	return 1;

}

/*
 * sub_paths    - new style relative paths from the default vertex for
 *                 the each of the corresponding old style suffix 
 * *_crypt_name - old style suffixes used in aliases.
 */
typedef struct tpsc_alias_name {
	char *alias_name;
	char  hwg_subpath_index;
} tpsc_alias_name_s;

char *sub_paths[] = {
	"",				/* 0 */
	"/swap",			/* 1 */
	"/norewind/swap",		/* 2 */
	"/norewind",			/* 3 */
	"/swap/varblk",			/* 4 */
	"/norewind/swap/varblk",	/* 5 */
	"/varblk",			/* 6 */
	"/norewind/varblk"		/* 7 */
};
#define NOSWAP_REWIND_NOVAR   0
#define SWAP_REWIND_NOVAR     1
#define SWAP_NOREWIND_NOVAR   2
#define NOSWAP_NOREWIND_NOVAR 3
#define SWAP_REWIND_VAR       4
#define SWAP_NOREWIND_VAR     5
#define NOSWAP_REWIND_VAR     6
#define NOSWAP_NOREWIND_VAR   7

tpsc_alias_name_s var_crypt_name[] = {
	{ "s",		SWAP_REWIND_NOVAR },
	{ "nrs",	SWAP_NOREWIND_NOVAR },
	{ "ns",		NOSWAP_REWIND_NOVAR },
	{ "",		NOSWAP_REWIND_NOVAR },
	{ "nrns",	NOSWAP_NOREWIND_NOVAR },
	{ "nr",		NOSWAP_NOREWIND_NOVAR },
	{ "sv",		SWAP_REWIND_VAR },
	{ "nrsv",	SWAP_NOREWIND_VAR },
	{ "nsv",	NOSWAP_REWIND_VAR },
	{ "v",		NOSWAP_REWIND_VAR },
	{ "nrnsv",      NOSWAP_NOREWIND_VAR },
	{ "nrv",	NOSWAP_NOREWIND_VAR }
};
tpsc_alias_name_s nonvar_crypt_name[] = {
	{ "s",		SWAP_REWIND_NOVAR },
	{ "nrs",	SWAP_NOREWIND_NOVAR },
	{ "ns",		NOSWAP_REWIND_NOVAR },
	{ "",		NOSWAP_REWIND_NOVAR },
	{ "nrns",	NOSWAP_NOREWIND_NOVAR },
	{ "nr",		NOSWAP_NOREWIND_NOVAR },
};
tpsc_alias_name_s swap_crypt_name[] = { 
	{ "s",		SWAP_REWIND_NOVAR },
	{ "nrs",	SWAP_NOREWIND_NOVAR },
	{ "ns",		NOSWAP_REWIND_NOVAR },
	{ "",		SWAP_REWIND_NOVAR },
	{ "nrns",	NOSWAP_NOREWIND_NOVAR },
	{ "nr",		SWAP_NOREWIND_NOVAR },
};

#define VAR_CRYPT_NAME_ARR_LEN		(sizeof(var_crypt_name) / sizeof(tpsc_alias_name_s))
#define NONVAR_CRYPT_NAME_ARR_LEN	(sizeof(nonvar_crypt_name) / sizeof(tpsc_alias_name_s))
#define SWAP_CRYPT_NAME_ARR_LEN         (sizeof(swap_crypt_name) / sizeof(tpsc_alias_name_s))

#define IS_SWAP_DEV(ttp)		(((ttp)->tp_hinv == TPQIC150) || \
					 ((ttp)->tp_hinv == TPQIC24)  || \
					 ((ttp)->tp_hinv == TPQIC1000))
#define CRYPT_NAME(ttp)			(((IS_SWAP_DEV(ttp)) ? swap_crypt_name : \
					  (ttp->tp_capablity & MTCAN_VAR) ? var_crypt_name : \
					  nonvar_crypt_name))
#define MAX_OLD_TPS_SUFFIXES(ttp) 	(((IS_SWAP_DEV(ttp)) ? SWAP_CRYPT_NAME_ARR_LEN : \
					  (ttp->tp_capablity & MTCAN_VAR) ? VAR_CRYPT_NAME_ARR_LEN : \
					  NONVAR_CRYPT_NAME_ARR_LEN))

/* make all aliases of /hw/tape/....
 */
/* ARGSUSED */
void
ct_alias_make(vertex_hdl_t	tape_vhdl,		
              vertex_hdl_t 	default_mode_vhdl,
	      struct tpsc_types *ttp)
{
	scsi_unit_info_t	*unit_info;

	/*
	 * Non-fabric devices will have a target node value of 0.
	 * SAN Fabric devices will be non-zero, and we need to make
	 * different shortcut devices.
	 */
	unit_info = scsi_unit_info_get(tape_vhdl);
	if (SUI_NODE(unit_info) == 0) 
		ct_alias_make_non_fabric(tape_vhdl, default_mode_vhdl, ttp);
	else
		ct_alias_make_fabric(tape_vhdl, default_mode_vhdl, ttp);
}

/* make all aliases of /hw/tape/tps#d#l#.... to the /hw/ctlr/#/target/#/lun/#/tape/default/...
 */
/* ARGSUSED */
void
ct_alias_make_non_fabric(vertex_hdl_t	tape_vhdl,		
			 vertex_hdl_t 	default_mode_vhdl,
			 struct tpsc_types *ttp)
{
	char			path_name[200];
	char			alias[20];
	dev_t			hwtape;
	/* REFERENCED */
	graph_error_t		rv;
	vertex_hdl_t		to;
	int			i,j;
	struct tpsc_alias_name	*crypt_name;
	char			**dens;
	char			**crypt_dens;
	vertex_hdl_t		mode_vhdl;
	dev_t			hwscsi;
	vertex_hdl_t		scsi_vhdl;
	scsi_unit_info_t	*unit_info;	
	int			controller_number;


	if ((hwscsi = hwgraph_path_to_dev("/hw/"EDGE_LBL_SCSI)) == NODEV) {
		rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_SCSI , &hwscsi);
		ASSERT(rv == GRAPH_SUCCESS);
	} 
	unit_info = scsi_unit_info_get(tape_vhdl);
	/*
	 * Get the ioconfig assigned controller number for this tape drive
	 */
	controller_number = device_controller_num_get(SUI_CTLR_VHDL(unit_info) );
	/* Form the alias to the devscsi device for this tape drive
	 * using the persistent ioconfig assigned controller number.
	 */
	sprintf(alias,"sc%dd%dl%d",
		controller_number,
		SUI_TARG(unit_info),
		SUI_LUN(unit_info));
	/* Store the persistent scsi controller information on the
	 * tape device vertex.
	 */
	device_controller_num_set(tape_vhdl,controller_number);

	rv = hwgraph_traverse(tape_vhdl,"../"EDGE_LBL_SCSI,&scsi_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	hwgraph_vertex_unref(scsi_vhdl);
	if (hwgraph_traverse(hwscsi,alias,&scsi_vhdl) != GRAPH_SUCCESS) {
		rv = hwgraph_edge_add(hwscsi,scsi_vhdl,alias);
		ASSERT(rv == GRAPH_SUCCESS);
	} else
		hwgraph_vertex_unref(scsi_vhdl);

	if ((hwtape = hwgraph_path_to_dev("/hw/"EDGE_LBL_TAPE)) == NODEV) {
		rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_TAPE , &hwtape);
		ASSERT(rv == GRAPH_SUCCESS);
	}

	vertex_to_name(tape_vhdl,path_name,200);
	strcat(path_name,"/"TPSC_STAT"/"EDGE_LBL_CHAR);
	tp_alias_make(tape_vhdl,alias,TPSC_STAT);
	if (hwgraph_traverse(hwtape,alias,&to) != GRAPH_SUCCESS) {
		rv = hwgraph_traverse(tape_vhdl,TPSC_STAT,&mode_vhdl);
		ASSERT(rv == GRAPH_SUCCESS);
		rv = hwgraph_edge_add(hwtape,mode_vhdl,alias);
		ASSERT(rv == GRAPH_SUCCESS); 
		hwgraph_vertex_unref(mode_vhdl);
	} else
		hwgraph_vertex_unref(to);

	
	crypt_name = CRYPT_NAME(ttp);
	dens = ttp->tp_hwg_dens_names;
	crypt_dens = ttp->tp_alias_dens_names;
	for ( i = 0 ; i < MAX_OLD_TPS_SUFFIXES(ttp) ; i++) {
	
		char		sub_path[100];
		char		suffix[20];
		
		if (ttp->tp_capablity & (MTCAN_COMPRESS | MTCAN_SETDEN)) {

			for (j = 0; j < ttp->tp_dens_count; j++) {

				sprintf(sub_path,"/../%s/%s/"EDGE_LBL_CHAR,
					sub_paths[crypt_name[i].hwg_subpath_index], (j ? dens[j] : ""));
				sprintf(suffix, "%s%s",
					crypt_name[i].alias_name, crypt_dens[j]);
				tp_alias_make(tape_vhdl,alias,suffix);
				rv = hwgraph_traverse(default_mode_vhdl,sub_path,
						      &mode_vhdl);
				if (rv != GRAPH_SUCCESS)
					continue;

				if (hwgraph_traverse(hwtape,alias,&to) != GRAPH_SUCCESS) {
					rv = hwgraph_edge_add(hwtape,mode_vhdl,alias);
					ASSERT(rv == GRAPH_SUCCESS);
				} else
					hwgraph_vertex_unref(to);
				hwgraph_vertex_unref(mode_vhdl);
			}
		} 
		sprintf(sub_path,"/../%s/"EDGE_LBL_CHAR, sub_paths[crypt_name[i].hwg_subpath_index]);
		sprintf(suffix,"%s",crypt_name[i].alias_name);
		tp_alias_make(tape_vhdl,alias,suffix);
		rv = hwgraph_traverse(default_mode_vhdl,sub_path,
				      &mode_vhdl);
		if (rv != GRAPH_SUCCESS)
			continue;
		if (hwgraph_traverse(hwtape,alias,&to) != GRAPH_SUCCESS) {
			rv = hwgraph_edge_add(hwtape,mode_vhdl,alias);
			ASSERT(rv == GRAPH_SUCCESS);
		} else
			hwgraph_vertex_unref(to);
		hwgraph_vertex_unref(mode_vhdl);

	}

	hwgraph_vertex_unref(hwscsi);
	hwgraph_vertex_unref(hwtape);
}


/* make all aliases of /hw/tape/nodename/lun#xxx/c#p#
 */
/* ARGSUSED */
void
ct_alias_make_fabric(vertex_hdl_t	tape_vhdl,		
		     vertex_hdl_t 	default_mode_vhdl,
		     struct tpsc_types	*ttp)
{
	dev_t			hwtape;
	/* REFERENCED */
	graph_error_t		rv;
	int			i,j;
	struct tpsc_alias_name	*crypt_name;
	char			**dens;
	char			**crypt_dens;
	vertex_hdl_t		mode_vhdl;
	dev_t			hwscsi;
	vertex_hdl_t		scsi_vhdl;
	scsi_unit_info_t	*unit_info;	
	int			ctrl;
	int			lun;
	uint64_t		node, port;

	if ((hwscsi = hwgraph_path_to_dev("/hw/"EDGE_LBL_SCSI)) == NODEV) {
		rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_SCSI , &hwscsi);
		ASSERT(rv == GRAPH_SUCCESS);
	} 
	unit_info = scsi_unit_info_get(tape_vhdl);
	/*
	 * Get the ioconfig assigned controller number for this tape drive
	 */
	ctrl = device_controller_num_get(SUI_CTLR_VHDL(unit_info));
	node = SUI_NODE(unit_info);
	port = SUI_PORT(unit_info);
	lun = SUI_LUN(unit_info);

	/* Store the persistent scsi controller information on the
	 * tape device vertex.
	 */
	device_controller_num_set(tape_vhdl,ctrl);

	/* Construct the alias
	 * /hw/scsi/#/lun#/c#p# -> /hw/module/.../lun/#/scsi
	 */
	rv = hwgraph_traverse(tape_vhdl,"../"EDGE_LBL_SCSI,&scsi_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	hwgraph_vertex_unref(scsi_vhdl);
	ct_make_fabric_alias(hwscsi, node, lun, "", ctrl, port, scsi_vhdl);

	/* Make path /hw/tape
	 */
	if ((hwtape = hwgraph_path_to_dev("/hw/"EDGE_LBL_TAPE)) == NODEV) {
		rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_TAPE , &hwtape);
		ASSERT(rv == GRAPH_SUCCESS);
	}

	/* Add the stat alias
	 * /hw/scsi/#/lun#stat/c#p# -> /hw/module/.../lun/#/tape/stat
	 */
	rv = hwgraph_traverse(tape_vhdl, TPSC_STAT, &mode_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	hwgraph_vertex_unref(mode_vhdl);
	ct_make_fabric_alias(hwtape, node, lun, TPSC_STAT, ctrl, port, 
			     mode_vhdl);


	crypt_name = CRYPT_NAME(ttp);
	dens = ttp->tp_hwg_dens_names;
	crypt_dens = ttp->tp_alias_dens_names;
	for ( i = 0 ; i < MAX_OLD_TPS_SUFFIXES(ttp) ; i++) {
	
		char		sub_path[100];
		char		suffix[20];
		
		if (ttp->tp_capablity & (MTCAN_COMPRESS | MTCAN_SETDEN)) {

			for (j = 0; j < ttp->tp_dens_count; j++) {

				sprintf(sub_path,"/../%s/%s/"EDGE_LBL_CHAR,
					sub_paths[crypt_name[i].
						 hwg_subpath_index],
					(j ? dens[j] : ""));
				rv = hwgraph_traverse(default_mode_vhdl,
						      sub_path,
						      &mode_vhdl);
				if (rv != GRAPH_SUCCESS) {
					continue;
				}

				sprintf(suffix, "%s%s",
					crypt_name[i].alias_name, 
					crypt_dens[j]);
				ct_make_fabric_alias(hwtape,
						     node, lun, 
						     suffix,
						     ctrl, port,
						     mode_vhdl);
				hwgraph_vertex_unref(mode_vhdl);
			}
		} 

		sprintf(sub_path,"/../%s/"EDGE_LBL_CHAR,
			sub_paths[crypt_name[i].hwg_subpath_index]);
		rv = hwgraph_traverse(default_mode_vhdl,sub_path,
				      &mode_vhdl);
		if (rv != GRAPH_SUCCESS) {
			continue;
		}

		sprintf(suffix,"%s",crypt_name[i].alias_name);
		ct_make_fabric_alias(hwtape, node, lun, suffix, ctrl, port,
				     mode_vhdl);
		hwgraph_vertex_unref(mode_vhdl);
	}
	hwgraph_vertex_unref(hwscsi);
	hwgraph_vertex_unref(hwtape);
}

/*
 * Add an alias starting from the "from" vertex, with path
 * nodename/lun#suffix/c#p# pointing to "target"
 */

void
ct_make_fabric_alias(vertex_hdl_t from, uint64_t node, int lun, char *suffix,
		     int ctrl, uint64_t port, vertex_hdl_t target)
{
	char			alias[150];
	char			node_slash_lun[80];
	char			ctrl_and_port[80];
	vertex_hdl_t		to;
	/* REFERENCED */
	int			rv;

	sprintf(node_slash_lun,"%llx/lun%d", node, lun);
	sprintf(ctrl_and_port, "c%dp%llx", ctrl, port);

	sprintf(alias, "%s%s/%s", node_slash_lun, suffix, ctrl_and_port);
	if (hwgraph_traverse(from, alias, &to) != GRAPH_SUCCESS) {
		sprintf(alias, "%s%s", node_slash_lun, suffix);
		if (hwgraph_traverse(from, alias, &to) != GRAPH_SUCCESS) {
			rv = hwgraph_path_add(from, alias, &to);
			ASSERT(rv == GRAPH_SUCCESS);
		}
		rv = hwgraph_edge_add(to, target, ctrl_and_port);
		ASSERT(rv == GRAPH_SUCCESS);
	}
	hwgraph_vertex_unref(to);
}

/*
 * Remove all alias from /hw/tape directory
 */
void
ct_alias_remove(vertex_hdl_t	tape_vhdl) 
{
	scsi_unit_info_t	*unit_info;

	/*
	 * Non-fabric devices will have a target node value of 0.
	 * SAN Fabric devices will be non-zero, and we need to 
	 * delete different aliases.
	 */
	unit_info = scsi_unit_info_get(tape_vhdl);
	if (SUI_NODE(unit_info) == 0) 
		ct_alias_remove_non_fabric(tape_vhdl);
	else 
		ct_alias_remove_fabric(tape_vhdl);
}

/* 
 * remove all the the nodename/lun#..... aliases from the /hw/tape directory
 */
void
ct_alias_remove_fabric(vertex_hdl_t	tape_vhdl) 
{
	dev_t			hwtape;
	/* REFERENCED */
	graph_error_t		rv;
	int			i,j;
	struct tpsc_alias_name	*crypt_name;
	char			**crypt_dens;
	vertex_hdl_t		default_mode_vhdl;
	struct tpsc_types	*ttp;
	tpsc_local_info_t	*tli;
	scsi_unit_info_t	*unit_info;
	uint64_t		node, port;
	int			lun;
	int			ctrl;

	unit_info = scsi_unit_info_get(tape_vhdl);
	ctrl = device_controller_num_get(SUI_CTLR_VHDL(unit_info));
	node = SUI_NODE(unit_info);
	port = SUI_PORT(unit_info);
	lun = SUI_LUN(unit_info);

	if ((hwtape = hwgraph_path_to_dev("/hw/"EDGE_LBL_TAPE)) == NODEV) {
		return;
	}

	/* remove the stat alias */
	ct_alias_remove_lun_ctrl_port(hwtape, node, lun, TPSC_STAT, ctrl, port);
	rv = hwgraph_traverse(tape_vhdl,TPSC_DEFAULT"/"EDGE_LBL_CHAR,
			      &default_mode_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	tli  = (tpsc_local_info_t *)hwgraph_fastinfo_get(default_mode_vhdl);
	ASSERT(tli);
	hwgraph_vertex_unref(default_mode_vhdl);
	ttp = tli->tli_ttp;
	crypt_name = CRYPT_NAME(ttp);
	crypt_dens = ttp->tp_alias_dens_names;
	for ( i = 0 ; i < MAX_OLD_TPS_SUFFIXES(ttp) ; i++) {
		char		suffix[20];
		
		if (ttp->tp_capablity & (MTCAN_COMPRESS | MTCAN_SETDEN)) {
			for (j = 0; j < ttp->tp_dens_count; j++) {
				sprintf(suffix, "%s%s",
					crypt_name[i].alias_name,
					crypt_dens[j]);
				ct_alias_remove_lun_ctrl_port(hwtape, node,
							      lun,
							      suffix,
							      ctrl,
							      port);
			}
		} 
		sprintf(suffix,"%s",crypt_name[i].alias_name);
		ct_alias_remove_lun_ctrl_port(hwtape, node, lun, suffix,
					      ctrl, port);
	}
	ct_alias_remove_node_edge(hwtape, node);
	hwgraph_vertex_unref(hwtape);
}

void 
ct_alias_remove_node_edge(dev_t hwtape, uint64_t node)
{	
	vertex_hdl_t		to;
	vertex_hdl_t		child;
	graph_edge_place_t	place;
	char			node_edge[50];
	char			some_edge[50];
	/* REFERENCED */
	graph_error_t		rv;

	/* Cannot remove the node edge unless
	   all the lun#xx edges have been removed */
	sprintf(node_edge, "%llx", node);
	rv = hwgraph_traverse(hwtape, node_edge, &to);
	ASSERT(rv == GRAPH_SUCCESS);
	hwgraph_vertex_unref(to);
	place = EDGE_PLACE_WANT_REAL_EDGES;
	if (hwgraph_edge_get_next(to, some_edge, &child, &place) == 
	    GRAPH_SUCCESS) {
		/* There is at least one edge remaining.
		   Can't remove the node edge */
		hwgraph_vertex_unref(child);
	} else {
		/* No more lun#xx edges, remove the node edge and the vertex 
		   it connects to. */
		rv = hwgraph_edge_remove(hwtape, node_edge, NULL);
		ASSERT(rv == GRAPH_SUCCESS);
		hwgraph_vertex_destroy(to);
	}
}

void
ct_alias_remove_lun_ctrl_port(dev_t hwtape, uint64_t node, int lun,
			      char *suffix, int ctrl, uint64_t port)
{
	char			node_slash_lun[50];
	char			ctrl_and_port[50];
	char			node_edge[50];	
	char			alias[150];
	vertex_hdl_t		to;
	vertex_hdl_t		child;
	/* REFERENCED */
	graph_error_t		rv;
	graph_edge_place_t	place;

	sprintf(node_edge, "%llx", node);
	sprintf(node_slash_lun,"%llx/lun%d", node, lun);
	sprintf(ctrl_and_port, "c%dp%llx", ctrl, port);

	/*
	 * Remove the ctrl_and_port edge
	 */
	sprintf(alias, "%s%s/%s", node_slash_lun, suffix, ctrl_and_port);
	if (hwgraph_traverse(hwtape, alias, &to) == GRAPH_SUCCESS) {
		hwgraph_vertex_unref(to);

		/* remove ctrl_and_port */
		sprintf(alias, "%s%s", node_slash_lun, suffix);
		rv = hwgraph_traverse(hwtape, alias, &to);
		ASSERT(rv == GRAPH_SUCCESS);
		rv = hwgraph_edge_remove(to, ctrl_and_port, NULL);
		ASSERT(rv == GRAPH_SUCCESS);
		hwgraph_vertex_unref(to);
	}

	/* Cannot remove the lun portion of the alias unless
	   all the ctrl_and_port edges have been removed */
	place = EDGE_PLACE_WANT_REAL_EDGES;
	if (hwgraph_edge_get_next(to, alias, &child, &place) ==
	    GRAPH_SUCCESS) {
		/* there is at least one edge remaining */
		hwgraph_vertex_unref(child);
		return;
	}

	/* No more ctrl_and_port edges from this vertex,
	   remove lun#stat edge and the vertex it is connected to */
	rv = hwgraph_traverse(hwtape, node_edge, &to);
	ASSERT(rv == GRAPH_SUCCESS);
	sprintf(alias, "lun%d%s", lun, suffix);
	rv = hwgraph_edge_remove(to, alias, NULL);
	ASSERT(rv == GRAPH_SUCCESS);
	hwgraph_vertex_unref(to);
	hwgraph_vertex_destroy(to);
}

/* 
 * remove all the the tps#d#.. aliases from the /hw/tape directory
 */
void
ct_alias_remove_non_fabric(vertex_hdl_t	tape_vhdl) 
{
	char			alias[20];
	dev_t			hwtape;
	/* REFERENCED */
	graph_error_t		rv;
	vertex_hdl_t		to;
	int			i,j;
	struct tpsc_alias_name	*crypt_name;
	char			**crypt_dens;
	vertex_hdl_t		default_mode_vhdl;
	struct tpsc_types	*ttp;
	tpsc_local_info_t	*tli;

	if ((hwtape = hwgraph_path_to_dev("/hw/"EDGE_LBL_TAPE)) == NODEV) {
		return;
	}

	/* remove the stat alias */
	tp_alias_make(tape_vhdl,alias,TPSC_STAT);
	if (hwgraph_traverse(hwtape,alias,&to) == GRAPH_SUCCESS) {
		rv = hwgraph_edge_remove(hwtape,alias,NULL);
		ASSERT(rv == GRAPH_SUCCESS);
		hwgraph_vertex_unref(to);
	}
	rv = hwgraph_traverse(tape_vhdl,TPSC_DEFAULT"/"EDGE_LBL_CHAR,
			      &default_mode_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	tli  = (tpsc_local_info_t *)hwgraph_fastinfo_get(default_mode_vhdl);
	ASSERT(tli);
	hwgraph_vertex_unref(default_mode_vhdl);
	ttp = tli->tli_ttp;
	crypt_name = CRYPT_NAME(ttp);
	crypt_dens = ttp->tp_alias_dens_names;
	for ( i = 0 ; i < MAX_OLD_TPS_SUFFIXES(ttp) ; i++) {
	
		char		suffix[20];
		
		if (ttp->tp_capablity & (MTCAN_COMPRESS | MTCAN_SETDEN)) {

			for (j = 0; j < ttp->tp_dens_count; j++) {

				sprintf(suffix, "%s%s",
					crypt_name[i].alias_name, crypt_dens[j]);
				tp_alias_make(tape_vhdl,alias,suffix);
				if (hwgraph_traverse(hwtape,alias,&to) == GRAPH_SUCCESS) {
					rv = hwgraph_edge_remove(hwtape,
								 alias,NULL);
					ASSERT(rv == GRAPH_SUCCESS);
					hwgraph_vertex_unref(to);
				}
			}
		} 
		sprintf(suffix,"%s",crypt_name[i].alias_name);
		tp_alias_make(tape_vhdl,alias,suffix);
		if (hwgraph_traverse(hwtape,alias,&to) == GRAPH_SUCCESS) {
			rv = hwgraph_edge_remove(hwtape,alias,NULL);
			ASSERT(rv == GRAPH_SUCCESS);
			hwgraph_vertex_unref(to);
		}
	}
	hwgraph_vertex_unref(hwtape);
}
