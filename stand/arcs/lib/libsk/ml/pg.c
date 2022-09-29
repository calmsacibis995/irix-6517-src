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


#ident "$Revision: 1.8 $"

#include <sys/types.h>
#include <libsc.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <promgraph.h>
#include "pgpriv.h"

#include <sys/SN/klconfig.h>
#include <sys/SN/gda.h>
#include <sys/SN/SN0/klhwinit_gfx.h>

#include <pgdrv.h>

#ifdef SN_PDI

graph_hdl_t      	pg_hdl          ;	/* prom graph hdl */
vertex_hdl_t     	snpdi_rvhdl     ;	/* root vertex hdl */

extern graph_hdl_t 	hwgraph  ;          /* To be initted so that we can */
extern int 		hwgraph_initted ;   /* use the hwgraph routines     */
extern struct mod_tab   module_table[] ; 	/* pglib.c */
extern int   		num_modules ; 		/* pglib.c */

void 			pgInit(void) ;
static void 		pgCreateBaseGraph(vertex_hdl_t) ;
static void 		pgCreateMod(void) ;
static int  		get_cgcb_indx(uchar) ;
static klinfo_t 	*get_nic_compt(lboard_t *) ;
static void 		pgSetupAlias(void) ;

extern	void            pg_create_ghdl(graph_hdl_t *, char *) ;
extern void 		pgDriverRegister(void) ;

/*
 * A hwgraph locator network, for easy access to certain devices
 * like /dev/tty and others. These vertices contain links to 
 * actual device vertices.
 */

static char *pg_base_graph[] = {
#define HW_MODULE 	0
	"/hw/module"		,
	"/cpu"			,
	"/hub"			,
	"/xbow"			,
	"/router"		,
	"/xwidget"		,
	"/dev/tty"		,
	"/dev/disk"		,
	"/dev/tape"		,
	"/dev/cdrom"		,
	"/dev/graphics"		,
	"/dev/network"		,
NULL
} ;

/* board graph create call back functions. */
/* indexed on brd_class */
/* General template is (lboard_t *, vertex_hdl_t) */
 
#if KLCLASS_MAX > 5
	"ERROR! bgcb inconsistent in pg.c"
#endif

void (*bgcb[])() = {
	pgCreateBrdNone		, 	/* KLCLASS_NONE 	*/
	pgCreateBrdNode		, 	/* KLCLASS_NODE 	*/
	pgCreateBrdIo		,	/* KLCLASS_IO		*/
	pgCreateBrdDefVoid	,	/* KLCLASS_ROUTER	*/
	pgCreateBrdNone		,	/* KLCLASS_MIDPLANE	*/
	pgCreateBrdGfx		,	/* KLCLASS_GFX		*/
	NULL
} ;

/* component graph create callback */
/* General template is (lboard_t *, klinfo_t *, vertex_hdl_t) */

void (*cgcb[])() = {
	pgCreateCompNone	,
	pgCreateCompCpu		,
	pgCreateCompMem		,
	pgCreateCompPci		,
	pgCreateCompGfx		,
	NULL
} ;

/*
 * pgCreateBaseGraph
 *
 * Create a bunch of standard locator paths like
 * /dev/disk, /dev/graphics
 */

static void 
pgCreateBaseGraph(vertex_hdl_t rvhdl)
{
	int 	i = 0 ;
	char	*cp ;

	while (cp = pg_base_graph[i++]) {
		create_graph_path(rvhdl, cp, 1) ;
	}
}

/*
 * pgCreateBrd
 *
 * For each brd found, invoke its call back
 */

static void
pgCreateBrd(lboard_t *lb)
{
	(*bgcb[(KLCLASS(lb->brd_type)>>4)])(lb, snpdi_rvhdl) ;
}

/*
 * pgCreateKlcfg
 * 
 * create a path for all brds found in klcfg
 */

/* XXX can this be replaced by visit_lboard */
static void
pgCreateKlcfg(void)
{
	cnodeid_t       cnode ;
	nasid_t         nasid ;
	lboard_t        *lb ;
	gda_t           *gdap;

	gdap = (gda_t *)GDA_ADDR(get_nasid());
	if (gdap->g_magic != GDA_MAGIC) {
		printf("create_brd_graph: Invalid GDA MAGIC\n") ;
		return ;
	}

	for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
		nasid = gdap->g_nasidtable[cnode];
		if (nasid == INVALID_NASID)
			continue;

		lb = (lboard_t *)KL_CONFIG_INFO(nasid) ;

		while (lb) {
			if (!(lb->brd_flags & DUPLICATE_BOARD))
				pgCreateBrd(lb) ;
			lb = KLCF_NEXT(lb);
		}
	}
}

/*
 * These 2 functions were present in init_prom_graph of promgraph.c
 * So they are here. May be they need another place.
 */

static void
pgDoOtherPreInit(void)
{
	make_serial(1) ;
	kl_check_consenv() ;
}

static void
pgDoOtherPostInit(void)
{
        kl_check_cons_in(snpdi_rvhdl) ;
}

/*
 * pgSetupAlias
 *
 * Set up any hardware specific aliasing here, so that
 * the prom can parse paths like bootp, dksc etc.
 * Right now, since we have just 1 network and 1 tty device,
 * their aliases are
 * 
 * /dev/network/0	- the lone network device - bootp() aliases to this.
 * /dev/tty/1, 2 	- the 2 tty ports - from the master ioc3,
 *
 * The driver AddChild routine creates these nodes and so we dont need any
 * thing here. Only a map between dksc(X and pci/Y
 * 
 * dksc aliases to /hw/module .... pci/X/ql.../target/Y/.. partition/Z/block
 * The table below maps dksc(0 and 1 to pci/X
 * On any baseio dksc(0 is PCI 0 and dksc(1 is PCI 1
 */

int sn_dksc_map[NUM_DKSC_CHAN] ;

void
pgSetupAlias(void)
{
	int i ;

	for (i=0; i< NUM_DKSC_CHAN; i++)
		sn_dksc_map[i] = i ;
}

/*
 * pgInit
 *
 * Called from libsk/lib/saio.c during init_saio
 * Creates the hwgraph for all boards found, 
 * installs any prom device drivers and they 
 * extend the hwgraph if necessary, to make the
 * complete prom hwgraph, ready to go.
 */

void 
pgInit(void)
{
	graph_error_t	ge ;

	/* Init prom devsw table from pgdrvinst.c */

	pgDrvTabInit() ; 	/* pgdrv.c */

	pg_create_ghdl(&pg_hdl, "SN_PDI_PG") ;
	/* Make hwgraph routines work */
	hwgraph = pg_hdl ;
        hwgraph_initted = 1 ;

	ge = graph_vertex_create(pg_hdl, &snpdi_rvhdl) ;
	if (ge != GRAPH_SUCCESS) {
		printf("Root vertex create err\n") ;
		return ;
	}

        pg_add_lbl(snpdi_rvhdl, INFO_LBL_VERTEX_NAME, (void *)EDGE_LBL_SYSNAME);

	pgCreateBaseGraph(snpdi_rvhdl) ;

	/* Create a graph path for each board in klcfg */

	pgCreateKlcfg() ;

	pgDoOtherPreInit() ;

	/* Register, install all the drivers in prom devsw */

	pgDriverRegister() ;

	pgDoOtherPostInit() ;

	pgSetupAlias() ;

#ifdef DEBUG
        print_pg_paths(pg_hdl, snpdi_rvhdl, "/") ;
#endif
}

/*
 * pgCreateComp
 *
 * Invoke component call back for each compt
 * found on the brd.
 */

void
pgCreateComp(lboard_t *lb, vertex_hdl_t v)
{
	int 		i, indx ;
	klinfo_t    	*k ;

	for (i = 0; i < KLCF_NUM_COMPS(lb); i++) {
		k = KLCF_COMP(lb, i) ;
		indx = get_cgcb_indx(k->struct_type) ;
		(*cgcb[indx])(lb, k, v) ;
    	}
}

/*
 * pgCreateBrd
 * Create the standard path for a brd
 */

vertex_hdl_t 
pgCreateBrdDefault(lboard_t *lb, vertex_hdl_t v)
{
	vertex_hdl_t        v1 ;
	char                buf[MAX_HW_MOD_SLOT_LEN] ;
	char                sname[SLOTNUM_MAXLENGTH] ;

	get_slotname(lb->brd_slot, sname);

	sprintf(buf, "%s/%d/%s/%s\0", pg_base_graph[HW_MODULE],
                                  lb->brd_module,
                                  EDGE_LBL_SLOT,
                                  sname) ;
	v1 = create_graph(buf) ;

	if (v1 != GRAPH_VERTEX_NONE) {
		pg_add_lbl(v1, INFO_LBL_BRD_INFO, (void *)lb) ;
		pgAddNicInfo(lb, v1) ;
	}

	return v1 ;
}

/*
 * pgCreateBrdNode
 * 
 * Call back for Node board.
 */

void 
pgCreateBrdNode(lboard_t *lb, vertex_hdl_t v)
{
	vertex_hdl_t 	v1 ;

	v1 = pgCreateBrdDefault(lb, v) ;

	v1 = create_graph_path(v1, EDGE_LBL_NODE, 1) ;

	pgCreateComp(lb, v1) ;
}

/*
 * pgCreateBrdIo
 * 
 * Call back for Io boards
 */

void 
pgCreateBrdIo(lboard_t *lb, vertex_hdl_t v)
{
	vertex_hdl_t        v1 ;
	char                buf[MAX_HW_MOD_SLOT_LEN];

	v1 = pgCreateBrdDefault(lb, v) ;

	*buf = 0 ;
	nic_name_convert(lb->brd_name, buf) ;

	v1 = create_graph_path(v1, buf, 1) ;

	pgCreateComp(lb, v1) ;
}

/*
 * pgCreateBrdGfx
 *
 * Call back for Gfx boards
 */

void
pgCreateBrdGfx(lboard_t *lb, vertex_hdl_t v)
{
	pgCreateBrdIo(lb, v) ;
}

void
pgCreateBrdDefVoid(lboard_t *lb, vertex_hdl_t v)
{
	pgCreateBrdDefault(lb, v) ;
}

void 
pgCreateBrdNone(lboard_t *lb, vertex_hdl_t v)
{
	return ;
}

void
pgAddNicInfo(lboard_t *lb, vertex_hdl_t v)
{
	klinfo_t *k ;

	k = get_nic_compt(lb) ;
	if (k)
		pg_add_nic_info(k, v) ;
}

/*
 * Call back for Components
 */

void
pgCreateCompNone(lboard_t *lb, klinfo_t *k, vertex_hdl_t v)
{
	return ;
}

void
pgCreateCompCpu(lboard_t *lb, klinfo_t *k, vertex_hdl_t v)
{
	char buf[MAX_HW_MOD_SLOT_LEN];
	vertex_hdl_t v1 ;

	strcpy(buf, "cpu") ;
	strcat(buf, "/%d\0") ;
	sprintf(buf, buf, k->physid) ;
	v1 = create_graph_path(v, buf, 1) ;
	pg_add_lbl(v1, INFO_LBL_KLCFG_INFO, (void *)k) ;

	return ;
}

void
pgCreateCompMem(lboard_t *lb, klinfo_t *k, vertex_hdl_t v)
{
	char buf[MAX_HW_MOD_SLOT_LEN];
	vertex_hdl_t v1 ;

	strcpy(buf, "mem") ;
	v1 = create_graph_path(v, buf, 1) ;
	pg_add_lbl(v1, INFO_LBL_KLCFG_INFO, (void *)k) ;

	return ;
}

/* XXX Make this type based - to be supplied by component guy */

void
pgGetCompKey(lboard_t *lb, klinfo_t *k, int *k1, int *k2, char *sk)
{
	klbri_t 	*lk ;

	*k1 = *k2 = -1 ;
	*sk = 0 ;

	if (KLCLASS(lb->brd_type) == KLCLASS_GFX) {
		if (lb->brd_type == KLTYPE_GFX_KONA)
			*k1 = XG_WIDGET_PART_NUM ;
		else if (lb->brd_type == KLTYPE_GFX_MGRA)
                        *k1 = HQ4_WIDGET_PART_NUM ;
		return ;
	}

	if ((lk = (klbri_t *)find_component(lb, NULL, KLSTRUCT_BRI)) == NULL)
		return ;

	*k1 = (lk->bri_devices[k->physid].pci_device_id) >> 16 ;
	*k2 = (lk->bri_devices[k->physid].pci_device_id) & 0xFFFF ;
}

#define MAX_STR_KEY_LEN	32

/*
 * pgCreatePromDrv
 *
 * Create Prom Driver Infrastructure for a Controller.
 */

void
pgCreatePromDrv(lboard_t *lb, klinfo_t *k, vertex_hdl_t v)
{
        int             key1, key2 ;
        char            strkey[MAX_STR_KEY_LEN] ;
        char            *sk = strkey ;
        prom_drv_t      *pd ;
        pd_info_t       *pdi ;
        int             dindx ;

        /*
         * Driver related support.
         * Get the key for this component.
         */
        pgGetCompKey(lb, k, &key1, &key2, strkey) ;
        if (!(*strkey))
                sk = NULL ;

        /* Search for a prom devsw entry */
        if ((pd = pgGetDrvTabPtrIndx(key1, key2, sk, &dindx)) == NULL)
                return ;

        /* Create a driver info. Hangs off of v as INFO_LBL_DRIVER */
        pdi = (pd_info_t *)pgCreatePd(sizeof(pd_info_t)) ;
        if (pdi == NULL)
                return ;

        pdi->pdi_devsw_p        = pd    ;
        pdi->pdi_devsw_i        = dindx ;
        pdi->pdi_flags          = 0     ;
        pdi->pdi_k              = k     ;
        pdi->pdi_lb             = lb    ;
        /* XXX fill in a COMPONENT for pdi->pdi_ctrl_ac */

        pg_add_lbl(v, INFO_LBL_DRIVER, (void *)pdi) ;
}

void
pgCreateCompPci(lboard_t *lb, klinfo_t *k, vertex_hdl_t v)
{
	char 		buf[MAX_HW_MOD_SLOT_LEN];
	vertex_hdl_t 	v1 ;

	strcpy(buf, EDGE_LBL_PCI"/") ;
	sprintf(&buf[strlen(buf)], "%d\0", k->physid) ;
	v1 = create_graph_path(v, buf, 1) ;
	pg_add_lbl(v1, INFO_LBL_KLCFG_INFO, (void *)k) ;

	pgCreatePromDrv(lb, k, v1) ;
}

void
pgCreateCompGfx(lboard_t *lb, klinfo_t *k, vertex_hdl_t v)
{
        pg_add_lbl(v, INFO_LBL_KLCFG_INFO, (void *)k) ;
	pgCreatePromDrv(lb, k, v) ;
}

/* XXX must be replaced by a srch routine */
static int
get_cgcb_indx(uchar type)
{
	int indx ;

	switch(type) {
		case KLSTRUCT_CPU:
	    		indx = 1 ;
		break ;

		case KLSTRUCT_MEMBNK:
	    		indx = 2 ;
		break ;

		case KLSTRUCT_IOC3:
		case KLSTRUCT_SCSI:
	    		indx = 3 ;
		break ;

		case KLSTRUCT_GFX:
	    		indx = 4 ;
		break ;

		default:
	    		indx = 0 ;
		break ;
	}

	return indx ;
}

static klinfo_t *
get_nic_compt(lboard_t *lb)
{
	int         i;
	klinfo_t    *k ;

	for (i = 0; i < KLCF_NUM_COMPS(lb); i++) {
		k = KLCF_COMP(lb, i) ;
		switch(k->struct_type) {
            		case KLSTRUCT_BRI:
            		case KLSTRUCT_HUB:
            		case KLSTRUCT_ROU:
            		case KLSTRUCT_GFX:
	     			return k ;
/* NOTREACHED */
	    			break ;
 		}
	}
	return NULL ;
}

#endif SN_PDI

