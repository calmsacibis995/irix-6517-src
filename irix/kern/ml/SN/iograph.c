/**************************************************************************
 *                                                                        *
 *                  Copyright (C) 1995 Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.128 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/edt.h>
#include <sys/iobus.h>
#include <sys/pio.h>
#include <sys/runq.h>
#include <ksys/hwg.h>
#include <sys/xtalk/xbow.h>
#include <sys/PCI/bridge.h>
#include <sys/pci_intf.h>
#include <sys/iograph.h>
#include <sys/schedctl.h>
#include <sys/SN/xbow.h>
#include <sys/SN/xtalkaddrs.h>
#include <sys/SN/klconfig.h>
#if defined (SN0)
#include <sys/SN/SN0/hubdev.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/bte.h>
#endif
#include <sys/PCI/univ_vmestr.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/pcibr.h>
#include <sys/xtalk/xswitch.h>
#include <sys/nic.h>
#include <sys/invent.h>
#include "sn_private.h"

#include <ksys/sthread.h>

/* #define PROBE_TEST */

/* At most 2 hubs can be connected to an xswitch */
#define NUM_XSWITCH_VOLUNTEER 2

/*
 * Track which hubs have volunteered to manage devices hanging off of
 * a Crosstalk Switch (e.g. xbow).  This structure is allocated,
 * initialized, and hung off the xswitch vertex early on when the
 * xswitch vertex is created.
 */
typedef struct xswitch_vol_s {
	mutex_t		xswitch_volunteer_mutex;
	int		xswitch_volunteer_count;
	vertex_hdl_t	xswitch_volunteer[NUM_XSWITCH_VOLUNTEER];
} *xswitch_vol_t;

void
xswitch_vertex_init(vertex_hdl_t xswitch)
{
	xswitch_vol_t xvolinfo;
	int rc;

	xvolinfo = kmem_zalloc(sizeof(struct xswitch_vol_s), KM_SLEEP);
	mutex_init(&xvolinfo->xswitch_volunteer_mutex, MUTEX_DEFAULT, "xswvol");
	rc = hwgraph_info_add_LBL(xswitch, 
			INFO_LBL_XSWITCH_VOL,
			(arbitrary_info_t)xvolinfo);
	ASSERT(rc == GRAPH_SUCCESS); rc = rc;
}

/*
 * When assignment of hubs to widgets is complete, we no longer need the
 * xswitch volunteer structure hanging around.  Destroy it.
 */
static void
xswitch_volunteer_delete(vertex_hdl_t xswitch)
{
	xswitch_vol_t xvolinfo;
	int rc;

	rc = hwgraph_info_remove_LBL(xswitch, 
				INFO_LBL_XSWITCH_VOL,
				(arbitrary_info_t *)&xvolinfo);
	ASSERT(rc == GRAPH_SUCCESS); rc = rc;

	kmem_free(xvolinfo, sizeof(struct xswitch_vol_s));
}
/*
 * A Crosstalk master volunteers to manage xwidgets on the specified xswitch.
 */
/* ARGSUSED */
static void
volunteer_for_widgets(vertex_hdl_t xswitch, vertex_hdl_t master)
{
	xswitch_vol_t xvolinfo = NULL;

	(void)hwgraph_info_get_LBL(xswitch, 
				INFO_LBL_XSWITCH_VOL, 
				(arbitrary_info_t *)&xvolinfo);
	if (xvolinfo == NULL) {
	    if (!is_headless_node_vertex(master))
		cmn_err(CE_WARN, 
			"volunteer for widgets: vertex %v has no info label",
			xswitch);
	    return;
	}

	mutex_lock(&xvolinfo->xswitch_volunteer_mutex, PZERO);
	ASSERT(xvolinfo->xswitch_volunteer_count < NUM_XSWITCH_VOLUNTEER);
	xvolinfo->xswitch_volunteer[xvolinfo->xswitch_volunteer_count] = master;
	xvolinfo->xswitch_volunteer_count++;
	mutex_unlock(&xvolinfo->xswitch_volunteer_mutex);
}

/* 
 * The "ideal fixed assignment" of 12 IO slots to 4 node slots.
 * At index N is the node slot number of the node board that should
 * ideally control the widget in IO slot N.  Note that if there is
 * only one node board on a given xbow, it will control all of the
 * devices on that xbow regardless of these defaults.
 *
 * 	N1 controls IO slots IO1, IO3, IO5	(upper left)
 * 	N3 controls IO slots IO2, IO4, IO6	(upper right)
 * 	N2 controls IO slots IO7, IO9, IO11	(lower left)
 * 	N4 controls IO slots IO8, IO10, IO12	(lower right)
 *
 * This makes assignments predictable and easily controllable.
 * TBD: Allow administrator to override these defaults.
 */
static slotid_t ideal_assignment[] = {
	-1,	/* IO0 -->non-existent */
	1,	/* IO1 -->N1 */
	3,	/* IO2 -->N3 */
	1,	/* IO3 -->N1 */
	3,	/* IO4 -->N3 */
	1,	/* IO5 -->N1 */
	3,	/* IO6 -->N3 */
	2,	/* IO7 -->N2 */
	4,	/* IO8 -->N4 */
	2,	/* IO9 -->N2 */
	4,	/* IO10-->N4 */
	2,	/* IO11-->N2 */
	4	/* IO12-->N4 */
};

static int
is_ideal_assignment(slotid_t hubslot, slotid_t ioslot)
{
	return(ideal_assignment[ioslot] == hubslot);
}

extern int xbow_port_io_enabled(nasid_t nasid, int widgetnum);

/*
 * Assign all the xwidgets hanging off the specified xswitch to the
 * Crosstalk masters that have volunteered for xswitch duty.
 */
/* ARGSUSED */
static void
assign_widgets_to_volunteers(vertex_hdl_t xswitch, vertex_hdl_t hubv)
{
	xswitch_info_t xswitch_info;
	xswitch_vol_t xvolinfo = NULL;
	xwidgetnum_t widgetnum;
	int curr_volunteer, num_volunteer;
	nasid_t nasid;
	hubinfo_t hubinfo;
	int xbownum;

	hubinfo_get(hubv, &hubinfo);
	nasid = hubinfo->h_nasid;
	
	xswitch_info = xswitch_info_get(xswitch);
	ASSERT(xswitch_info != NULL);

	(void)hwgraph_info_get_LBL(xswitch, 
				INFO_LBL_XSWITCH_VOL, 
				(arbitrary_info_t *)&xvolinfo);
	if (xvolinfo == NULL) {
	    if (!is_headless_node_vertex(hubv))
		cmn_err(CE_WARN, 
			"assign_widgets_to_volunteers:vertex %v has "
			" no info label",
			xswitch);
	    return;
	}

	num_volunteer = xvolinfo->xswitch_volunteer_count;
	ASSERT(num_volunteer > 0);
	curr_volunteer = 0;

	/* Assign master hub for xswitch itself.  */
	if (HUB_WIDGET_ID_MIN > 0) {
		hubv = xvolinfo->xswitch_volunteer[0];
		xswitch_info_master_assignment_set(xswitch_info, (xwidgetnum_t)0, hubv);
	}

	xbownum = get_node_crossbow(nasid);

	/*
	 * TBD: Use administrative information to alter assignment of
	 * widgets to hubs.
	 */
	for (widgetnum=HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX; widgetnum++) {
		int i;

		/*
		 * Ignore disabled/empty ports.
		 */
		if (!xbow_port_io_enabled(nasid, widgetnum)) 
		    continue;

		/*
		 * If this is the master IO board, assign it to the same 
		 * hub that owned it in the prom.
		 */
		if (is_master_nasid_widget(nasid, widgetnum)) {
			int i;

			for (i=0; i<num_volunteer; i++) {
				hubv = xvolinfo->xswitch_volunteer[i];
				hubinfo_get(hubv, &hubinfo);
				nasid = hubinfo->h_nasid;
				if (nasid == get_console_nasid())
					goto do_assignment;
			}
			cmn_err(CE_PANIC,
				"Nasid == %d, console nasid == %d",
				nasid, get_console_nasid());
		}

		/*
		 * Try to do the "ideal" assignment if IO slots to nodes.
		 */
		for (i=0; i<num_volunteer; i++) {
			hubv = xvolinfo->xswitch_volunteer[i];
			hubinfo_get(hubv, &hubinfo);
			nasid = hubinfo->h_nasid;
			if (is_ideal_assignment(SLOTNUM_GETSLOT(get_node_slotid(nasid)),
						SLOTNUM_GETSLOT(get_widget_slotnum(xbownum, widgetnum)))) {

				goto do_assignment;
				
			}
		}

		/*
		 * Do a round-robin assignment among the volunteer nodes.
		 */
		hubv = xvolinfo->xswitch_volunteer[curr_volunteer];
		curr_volunteer = (curr_volunteer + 1) % num_volunteer;
		/* fall through */

do_assignment:
		/*
		 * At this point, we want to make hubv the master of widgetnum.
		 */
		xswitch_info_master_assignment_set(xswitch_info, widgetnum, hubv);
	}

	xswitch_volunteer_delete(xswitch);
}

/*
 * Early iograph initialization.  Called by master CPU in mlreset().
 * Useful for including iograph.o in kernel.o.
 */
void
iograph_early_init()
{
#if SN0
	cnodeid_t cnode;
	nasid_t nasid;
	lboard_t *board;

	/*
	 * Init. the board-to-hwgraph link early, so FRU analyzer
	 * doesn't trip on leftover values if we panic early on.
	 */
	for(cnode = 0; cnode < numnodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);
		board = (lboard_t *)KL_CONFIG_INFO(nasid);

		/* Check out all the board info stored on a node */
		while(board) {
			board->brd_graph_link = GRAPH_VERTEX_NONE;
			board = KLCF_NEXT(board);
		}
	}
#endif /* SN0 */

	hubio_init();
}

/* There is an identical definition of this in os/scheduler/runq.c */
#define INIT_COOKIE(cookie) cookie.must_run = 0; cookie.cpu = PDA_RUNANYWHERE
/*
 * These functions absolutely doesn't belong here.  It's  here, though, 
 * until the scheduler provides a platform-independent version
 * that works the way it should.  The interface will definitely change, 
 * too.  Currently used only in this file and by io/cdl.c in order to
 * bind various I/O threads to a CPU on the proper node.
 */
cpu_cookie_t
setnoderun(cnodeid_t cnodeid)
{
	int i;
	cpuid_t cpunum;
	cpu_cookie_t cookie;

	INIT_COOKIE(cookie);
	if (cnodeid == CNODEID_NONE)
		return(cookie);

	/*
	 * Do a setmustrun to one of the CPUs on the specified
	 * node.
	 */
	if ((cpunum = CNODE_TO_CPU_BASE(cnodeid)) == CPU_NONE) {
		return(cookie);
	}

	cpunum += CNODE_NUM_CPUS(cnodeid) - 1;

	for (i = 0; i < CNODE_NUM_CPUS(cnodeid); i++, cpunum--) {

		if (cpu_enabled(cpunum)) {
			cookie = setmustrun(cpunum);
			break;
		}
	}

	return(cookie);
}

void
restorenoderun(cpu_cookie_t cookie)
{
	restoremustrun(cookie);
}
static sema_t io_init_sema;


/*
 * Let boot processor know that we're done initializing our node's IO
 * and then exit.
 */
/* ARGSUSED */
static void
io_init_done(cnodeid_t cnodeid,cpu_cookie_t c)
{
	/* Let boot processor know that we're done. */
	vsema(&io_init_sema);
	/* This is for the setnoderun done when the io_init thread
	 * started 
	 */
	restorenoderun(c);
	sthread_exit();
}

/* 
 * Probe to see if this hub's xtalk link is active.  If so,
 * return the Crosstalk Identification of the widget that we talk to.  
 * This is called before any of the Crosstalk infrastructure for 
 * this hub is set up.  It's usually called on the node that we're
 * probing, but not always.
 *
 * TBD: Prom code should actually do this work, and pass through 
 * hwid for our use.
 */
static void
early_probe_for_widget(vertex_hdl_t hubv, xwidget_hwid_t hwid)
{
	hubreg_t llp_csr_reg;
	nasid_t nasid;
	hubinfo_t hubinfo;

	hubinfo_get(hubv, &hubinfo);
	nasid = hubinfo->h_nasid;

	llp_csr_reg = REMOTE_HUB_L(nasid, IIO_LLP_CSR);

	/* ,
	 * If link is up, read the widget's part number.
	 * A direct connect widget must respond to widgetnum=0.
	 */
	if (llp_csr_reg & IIO_LLP_CSR_IS_UP) {
		/* TBD: Put hub into "indirect" mode */
		/*
		 * We're able to read from a widget because our hub's 
		 * WIDGET_ID was set up earlier.
		 */
		widgetreg_t widget_id = XWIDGET_ID_READ(nasid, 0);

		hwid->part_num = XWIDGET_PART_NUM(widget_id);
		hwid->rev_num = XWIDGET_REV_NUM(widget_id);
		hwid->mfg_num = XWIDGET_MFG_NUM(widget_id);
		/* TBD: link reset */
	} else {
		hwid->part_num = XWIDGET_PART_NUM_NONE;
		hwid->rev_num = XWIDGET_REV_NUM_NONE;
		hwid->mfg_num = XWIDGET_MFG_NUM_NONE;
	}

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
	if (xwidget_hwid_is_xswitch(hwid)) {
	    lboard_t *brd;
	    klxbow_t *xbow_p;
	    int widgetnum;
	    nasid_t hub_nasid;

	    if ((brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), 
				   KLTYPE_MIDPLANE8)) == NULL)
		return;

	    if (!KL_CONFIG_DUPLICATE_BOARD(brd))
		return;

	    if ((xbow_p = (klxbow_t *)find_component(brd, NULL, KLSTRUCT_XBOW))
		== NULL)
		return;
	    
	    for (widgetnum = HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX; widgetnum++) {
		if (!XBOW_PORT_TYPE_HUB(xbow_p, widgetnum)) 
		    continue;
		hub_nasid = XBOW_PORT_NASID(xbow_p, widgetnum);
		if ((hub_nasid == INVALID_NASID) || (hub_nasid == nasid))
		    continue;

		if (NASID_TO_COMPACT_NODEID(hub_nasid) == INVALID_CNODEID) {
		    hubii_ilcsr_t csr;

		    cmn_err(CE_NOTE, "Disabling IO on %v. Xbow master in different cell", hubv);

		    csr.icsr_reg_value = REMOTE_HUB_L(nasid, IIO_LLP_CSR);
		    csr.icsr_fields_s.icsr_llp_en = 0;
		    REMOTE_HUB_S(nasid, IIO_LLP_CSR, csr.icsr_reg_value);
		    hwid->part_num = XWIDGET_PART_NUM_NONE;
		    hwid->rev_num = XWIDGET_REV_NUM_NONE;
		    hwid->mfg_num = XWIDGET_MFG_NUM_NONE;
		    return;
		}
	    }
	    
	}
#endif /* (CELL_IRIX) && (LOGICAL_CELLS) */
}
/* Add inventory information to the widget vertex 
 * Right now (module,slot,revision) is being
 * added as inventory information.
 */
static void
xwidget_inventory_add(vertex_hdl_t 		widgetv,
		      lboard_t 			*board,
		      struct xwidget_hwid_s 	hwid)
{
	if (!board)
		return;
	/* Donot add inventory information for the baseio
	 * on a speedo with an xbox. It has already been
	 * taken care of in SN00_vmc.
	 * Speedo with xbox's baseio comes in at slot io1 (widget 9)
	 */
	if (SN00 							&&  
	    (SLOTNUM_GETSLOT(board->brd_slot) == 1)			&&
	    (SLOTNUM_GETCLASS(board->brd_slot) == SLOTNUM_XTALK_CLASS))
		return;
	device_inventory_add(widgetv,INV_IOBD,board->brd_type,
			     board->brd_module,
			     SLOTNUM_GETSLOT(board->brd_slot),
			     hwid.rev_num);
}
static void
io_init_xswitch_widgets(vertex_hdl_t xswitchv, cnodeid_t cnode)
{
	xswitch_info_t		xswitch_info;
	xwidgetnum_t		widgetnum;
	xwidgetnum_t		hub_widgetid;
	vertex_hdl_t		widgetv;
	vertex_hdl_t		hubv;
	widgetreg_t		widget_id;
	nasid_t			nasid, peer_nasid;
	struct xwidget_hwid_s 	hwid;
	hubinfo_t		hubinfo;
	/*REFERENCED*/
	int			rc;
	arbitrary_info_t	xbow_num;
	char			slotname[SLOTNUM_MAXLENGTH];
	char 			pathname[128];
	char			new_name[64];
	moduleid_t		module;
	slotid_t		slot;
	lboard_t		*board;
	async_attach_t          aa;

	aa = async_attach_new();
	
	/*
	 * Verify that xswitchv is indeed an attached xswitch.
	 */
	xswitch_info = xswitch_info_get(xswitchv);
	ASSERT(xswitch_info != NULL);

	hubv = cnodeid_to_vertex(cnode);
	hubinfo_get(hubv, &hubinfo);
	nasid = hubinfo->h_nasid;
	hub_widgetid = hubinfo->h_widgetid;

	/* Get our crossbow number */
	hwgraph_info_get_LBL(xswitchv, INFO_LBL_XSWITCH_ID, &xbow_num);

	/* Who's the other guy on out crossbow (if anyone) */
	peer_nasid = NODEPDA(cnode)->xbow_peer;
	if (peer_nasid == INVALID_NASID)
		/* If I don't have a peer, use myself. */
		peer_nasid = nasid;

	for (widgetnum = HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX; widgetnum++) {

		/* Check my xbow structure and my peer's */
		if (!xbow_port_io_enabled(nasid, widgetnum) &&
		    !xbow_port_io_enabled(peer_nasid, widgetnum)) {
		    continue;
		}

		if (xswitch_info_link_ok(xswitch_info, widgetnum)) {
			char			name[4];
			/*
			 * If the current hub is not supposed to be the master 
			 * for this widgetnum, then skip this widget.
			 */
			if (xswitch_info_master_assignment_get(xswitch_info,
							widgetnum) != hubv) {
				continue;
			}

			/*
			 * Get the slot name for this widget
			 */
	
			get_widget_slotname(xbow_num, widgetnum, slotname);

			module 	= NODEPDA(cnode)->module_id;
			slot	= get_widget_slotnum(xbow_num, widgetnum);
			board = get_board_name(nasid, module,slot,new_name);

			/*
			 * Create the vertex for the widget, using the decimal 
			 * widgetnum as the name of the primary edge.
			 */
			sprintf(pathname, EDGE_LBL_MODULE "/%d/"
					  EDGE_LBL_SLOT "/%s/%s",
				NODEPDA(cnode)->module_id,
				slotname, new_name);

			rc = hwgraph_path_add(hwgraph_root, pathname, &widgetv);
		        /*
		         * This is a weird ass code needed for error injection
		         * purposes.
		         */
		        device_master_set(hwgraph_connectpt_get(widgetv), hubv);
			
			ASSERT(rc == GRAPH_SUCCESS);
					     
			/* If we are looking at the global master io6
			 * then add information about the version of
			 * the io6prom as a part of "detailed inventory"
			 * information.
			 */
			if (is_master_baseio(nasid,
					     NODEPDA(cnode)->module_id,
					     get_widget_slotnum(xbow_num, 
								widgetnum))) {
				extern void klhwg_baseio_inventory_add(dev_t,
								    cnodeid_t);

				klhwg_baseio_inventory_add(widgetv,cnode);
			}
			sprintf(name, "%d", widgetnum);
			rc = hwgraph_edge_add(xswitchv, widgetv, name);

			/*
			 * crosstalk switch code tracks which
			 * widget is attached to each link.
			 */
			xswitch_info_vhdl_set(xswitch_info, widgetnum, widgetv);

			/*
			 * Peek at the widget to get its crosstalk part and
			 * mfgr numbers, then present it to the generic xtalk
			 * bus provider to have its driver attach routine
			 * called (or not).
			 */
			widget_id = XWIDGET_ID_READ(nasid, widgetnum);
			hwid.part_num = XWIDGET_PART_NUM(widget_id);
			hwid.rev_num = XWIDGET_REV_NUM(widget_id);
			hwid.mfg_num = XWIDGET_MFG_NUM(widget_id);
			/* Store some inventory information about
			 * the xwidget in the hardware graph.
			 */
			xwidget_inventory_add(widgetv,board,hwid);

			(void)xwidget_init(&hwid, widgetv, widgetnum,
					   hubv, hub_widgetid,
					   aa);

#ifdef	SN0_USE_BTE
			bte_bpush_war(cnode, (void *)board);
#endif
		}
	}
	/* 
	 * Wait for parallel attach threads, if any, to complete.
	 */
	async_attach_waitall(aa);
	async_attach_free(aa);
}
#if SN0
/*
 * For each PCI bridge connected to the xswitch, add a link from the
 * board's klconfig info to the bridge's hwgraph vertex.  This lets
 * the FRU analyzer find the bridge without traversing the hardware
 * graph and risking hangs.
 */
static void
io_link_xswitch_widgets(vertex_hdl_t xswitchv, cnodeid_t cnodeid)
{
	arbitrary_info_t	xbow_num;
	xwidgetnum_t		widgetnum;
	char 			pathname[128];
	vertex_hdl_t		vhdl;
	nasid_t			nasid, peer_nasid;
	slotid_t		slot;
	lboard_t		*board;


	/* Get the crossbow number */
	hwgraph_info_get_LBL(xswitchv, INFO_LBL_XSWITCH_ID, &xbow_num);

	/* And its connected hub's nasids */
	nasid = COMPACT_TO_NASID_NODEID(cnodeid);
	peer_nasid = NODEPDA(cnodeid)->xbow_peer;

	/* 
	 * Look for paths matching "<widgetnum>/pci" under xswitchv.
	 * For every widget, init. its lboard's hwgraph link.  If the
	 * board has a PCI bridge, point the link to it.
	 */
	for (widgetnum = HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX;
		 widgetnum++) {
		sprintf(pathname, "%d", widgetnum);
		if (hwgraph_traverse(xswitchv, pathname, &vhdl) !=
		    GRAPH_SUCCESS)
			continue;

		slot = get_widget_slotnum(xbow_num, widgetnum);
		board = find_lboard_modslot((lboard_t *)KL_CONFIG_INFO(nasid),
				    NODEPDA(cnodeid)->module_id, slot);
		if (board == NULL && peer_nasid != INVALID_NASID) {
			/*
			 * Try to find the board on our peer
			 */
			board = find_lboard_modslot((lboard_t *)KL_CONFIG_INFO(peer_nasid),
						    NODEPDA(cnodeid)->module_id, slot);
		}
		if (board == NULL) {
			cmn_err(CE_WARN,
				"Could not find PROM info for vertex %v, "
				"FRU analyzer may fail",
				vhdl);
			return;
		}

		sprintf(pathname, "%d/"EDGE_LBL_PCI, widgetnum);
		if (hwgraph_traverse(xswitchv, pathname, &vhdl) == 
		    GRAPH_SUCCESS)
			board->brd_graph_link = vhdl;
		else
			board->brd_graph_link = GRAPH_VERTEX_NONE;
	}
}
/*
 * Do the same, for a direct-connected bridge on an O200
 */
static void
io_link_direct_widget(vertex_hdl_t xtalkv, cnodeid_t cnodeid)
{
	lboard_t	*board;
	vertex_hdl_t	vhdl;
	nasid_t		nasid;
	char		pathname[128];

	/* 
	 * Find this node's IO lboard
	 */
	nasid = COMPACT_TO_NASID_NODEID(cnodeid);
	board = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid),
				  KLCLASS_IO);
	if (board == NULL) {
		cmn_err(CE_WARN,
			"Could not find PROM I/O board info for NASID %d, "
			"FRU analyzer may fail",
			nasid);
		return;
	}
	
	/* 
	 * Look for the bridge under the path, "<base widgetnum>/pci",
	 * and point the lboard's hwgraph link to it.
	 */
	sprintf(pathname, "%d/"EDGE_LBL_PCI, NODEPDA(cnodeid)->basew_id);
	if (hwgraph_traverse(xtalkv, pathname, &vhdl) == GRAPH_SUCCESS) {
		board->brd_graph_link = vhdl;
	}
	else {
		board->brd_graph_link = GRAPH_VERTEX_NONE;
		cmn_err(CE_WARN, "Couldn't find hwgraph vertex for on-board "
			"bridge from vertex %d (%v)\n", xtalkv, xtalkv);
	}	
}
#endif /* SN0 */
/*
 * Initialize all I/O on the specified node.
 */
static void
io_init_node(cnodeid_t cnodeid)
{
	/*REFERENCED*/
	graph_error_t rc;
	vertex_hdl_t hubv, switchv, widgetv;
	struct xwidget_hwid_s hwid;
	hubinfo_t hubinfo;
	int is_xswitch;
	nodepda_t	*npdap;
	sema_t 		*peer_sema = 0;
	__uint32_t	widget_partnum;
	nodepda_router_info_t *npda_rip;
	cpu_cookie_t	c;

	/* Try to execute on the node that we're initializing. */
	c = setnoderun(cnodeid);

	npdap = NODEPDA(cnodeid);

	/*
	 * Get the "top" vertex for this node's hardware
	 * graph; it will carry the per-hub hub-specific
	 * data, and act as the crosstalk provider master.
	 * It's canonical path is probably something of the
	 * form /hw/module/%d/slot/%d/node
	 */
	hubv = cnodeid_to_vertex(cnodeid);
	ASSERT(hubv != GRAPH_VERTEX_NONE);

	/*
	 * Set up the dependent routers if we have any.
	 */
	npda_rip = npdap->npda_rip_first;

	while(npda_rip) {
		/* If the router info has not been initialized
		 * then we need to do the router initialization
		 */
		if (!npda_rip->router_infop) {
			router_init(cnodeid,0,npda_rip);
		}
		npda_rip = npda_rip->router_next;
	}
#if SN0
	hubdev_docallouts(hubv);
#endif
	/*
	 * Read NIC on this hub
	 */
	nic_hub_vertex_info(hubv);
	/* 
	 * If nothing connected to this hub's xtalk port, we're done.
	 */
	early_probe_for_widget(hubv, &hwid);
	if (hwid.part_num == XWIDGET_PART_NUM_NONE) {
#ifdef PROBE_TEST
		if ((cnodeid == 1) || (cnodeid == 2)) {
			int index;

			for (index = 0; index < 600; index++)
				printf("Interfering with device probing!!!\n");
		}
#endif
		/* io_init_done takes cpu cookie as 2nd argument 
		 * to do a restorenoderun for the setnoderun done 
		 * at the start of this thread 
		 */
		io_init_done(cnodeid,c);
		/* NOTREACHED */
	}

	/* 
	 * attach our hub_provider information to hubv,
	 * so we can use it as a crosstalk provider "master"
	 * vertex.
	 */
	xtalk_provider_register(hubv, &hub_provider);
	xtalk_provider_startup(hubv);

	/*
	 * Create a vertex to represent the crosstalk bus
	 * attached to this hub, and a vertex to be used
	 * as the connect point for whatever is out there
	 * on the other side of our crosstalk connection.
	 *
	 * Crosstalk Switch drivers "climb up" from their
	 * connection point to try and take over the switch
	 * point.
	 *
	 * Of course, the edges and verticies may already
	 * exist, in which case our net effect is just to
	 * associate the "xtalk_" driver with the connection
	 * point for the device.
	 */

	(void)hwgraph_path_add(hubv, EDGE_LBL_XTALK, &switchv);
	ASSERT(switchv != GRAPH_VERTEX_NONE);

	(void)hwgraph_edge_add(hubv, switchv, EDGE_LBL_IO);

	/*
	 * We need to find the widget id and update the basew_id field
	 * accordingly. In particular, SN00 has direct connected bridge,
	 * and hence widget id is Not 0.
	 */
	
	widget_partnum = (((*(volatile __int32_t *)(NODE_SWIN_BASE(COMPACT_TO_NASID_NODEID(cnodeid), 0) + WIDGET_ID))) & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT;
	
	if (widget_partnum == BRIDGE_WIDGET_PART_NUM){
		npdap->basew_id = (((*(volatile __int32_t *)(NODE_SWIN_BASE(COMPACT_TO_NASID_NODEID(cnodeid), 0) + BRIDGE_WID_CONTROL))) & WIDGET_WIDGET_ID);

	} else if (widget_partnum == XBOW_WIDGET_PART_NUM) {
		/* 
		 * Xbow control register does not have the widget ID field.
		 * So, hard code the widget ID to be zero.
		 */
		npdap->basew_id = 0;
	} else { 
		npdap->basew_id = (((*(volatile __int32_t *)(NODE_SWIN_BASE(COMPACT_TO_NASID_NODEID(cnodeid), 0) + BRIDGE_WID_CONTROL))) & WIDGET_WIDGET_ID);

		cmn_err(CE_PANIC, "Unknown Widget Part Number 0x%x Widgt ID 0x%x attached to Hub %v",
			widget_partnum, npdap->basew_id, hubv);
		/*NOTREACHED*/
	}
	{
		char widname[10];
		sprintf(widname, "%x", npdap->basew_id);
		(void)hwgraph_path_add(switchv, widname, &widgetv);
		ASSERT(widgetv != GRAPH_VERTEX_NONE);
	}
	
	nodepda->basew_xc = widgetv;

	(void)hwgraph_char_device_add(widgetv, NULL, "xtalk_", NULL);

	is_xswitch = xwidget_hwid_is_xswitch(&hwid);

	/* 
	 * Try to become the master of the widget.  If this is an xswitch
	 * with multiple hubs connected, only one will succeed.  Mastership
	 * of an xswitch is used only when touching registers on that xswitch.
	 * The slave xwidgets connected to the xswitch can be owned by various
	 * masters.
	 */
	if (device_master_set(widgetv, hubv) == 0) {

		/* Only one hub (thread) per Crosstalk device or switch makes
		 * it to here.
		 */

		/* 
		 * Initialize whatever xwidget is hanging off our hub.
		 * Whatever it is, it's accessible through widgetnum 0.
		 */
		hubinfo_get(hubv, &hubinfo);

		(void)xwidget_init(&hwid, widgetv, npdap->basew_id, hubv, hubinfo->h_widgetid, NULL);

		if (!is_xswitch) {
#if SN0
			io_link_direct_widget(switchv, cnodeid);
#endif
			/* io_init_done takes cpu cookie as 2nd argument 
			 * to do a restorenoderun for the setnoderun done 
			 * at the start of this thread 
			 */
			io_init_done(cnodeid,c);
			/* NOTREACHED */
		}

		/* 
		 * Special handling for Crosstalk Switches (e.g. xbow).
		 * We need to do things in roughly the following order:
		 *	1) Initialize xswitch hardware (done above)
		 *	2) Determine which hubs are available to be widget masters
		 *	3) Discover which links are active from the xswitch
		 *	4) Assign xwidgets hanging off the xswitch to hubs
		 *	5) Initialize all xwidgets on the xswitch
		 */

		volunteer_for_widgets(switchv, hubv);

		/* If there's someone else on this crossbow, recognize him */
		if (npdap->xbow_peer != INVALID_NASID) {
			nodepda_t *peer_npdap = NODEPDA(NASID_TO_COMPACT_NODEID(npdap->xbow_peer));
			peer_sema = &peer_npdap->xbow_sema;
			volunteer_for_widgets(switchv, peer_npdap->node_vertex);
		}

		assign_widgets_to_volunteers(switchv, hubv);

		/* Signal that we're done */
		if (peer_sema) {
			vsema(peer_sema);
		}
		
	}
	else {
	    /* Wait 'til master is done assigning widgets. */
	    psema(&npdap->xbow_sema, PZERO);
	}

#ifdef PROBE_TEST
	if ((cnodeid == 1) || (cnodeid == 2)) {
		int index;

		for (index = 0; index < 500; index++)
			printf("Interfering with device probing!!!\n");
	}
#endif
	/* Now both nodes can safely inititialize widgets */
	io_init_xswitch_widgets(switchv, cnodeid);
#if SN0
	io_link_xswitch_widgets(switchv, cnodeid);
#endif

#if defined (HUB_II_IFDR_WAR)
	{
	int num_bridges = 0;
	extern xbow_get_bridge_pointers(cnodeid_t, __psunsigned_t *, int *);

	xbow_get_bridge_pointers(cnodeid, NODEPDA(cnodeid)->bridge_base,
				 &num_bridges);
#ifndef SN0_USE_BTE
	/* Only kick hubs with IO attached */
	NODEPDA(cnodeid)->hub_dmatimeout_need_kick = (num_bridges > 0);
#else
	/* When we're using the BTE, all hubs need kicking. */
	NODEPDA(cnodeid)->hub_dmatimeout_need_kick = 1;
#endif
	NODEPDA(cnodeid)->kick_graph_done = 1;
	}
#endif	/* HUB_II_IFDR_WAR */

	/* io_init_done takes cpu cookie as 2nd argument 
	 * to do a restorenoderun for the setnoderun done 
	 * at the start of this thread 
	 */
	io_init_done(cnodeid,c);
}

#if defined (HUB_II_IFDR_WAR)
int   graph_done = 0;
#endif

#define IOINIT_STKSZ	(16 * 1024)

#include <sys/iograph.h>
#define __DEVSTR1 	"/../.master/"
#define __DEVSTR2 	"/target/"
#define __DEVSTR3 	"/lun/0/disk/partition/"
#define	__DEVSTR4	"/../ef"

vertex_hdl_t		base_io_scsi_ctlr_vhdl[2] = {	GRAPH_VERTEX_NONE,
							GRAPH_VERTEX_NONE
						    };

/*
 * put the logical controller number information in the 
 * scsi controller vertices
 */
static void
scsi_ctlr_nums_add(vertex_hdl_t pci_vhdl)
{
	int		pci_slot;
	char		sub_path[20];
	vertex_hdl_t	scsi_ctlr_vhdl;
	int		control_num = 0;

	/* go through each pci slot & check if there
	 * is a scsi controller in there. if so then 
	 * assign the next available controller
	 * number. We only check slots 0-4  as all BASE IO scsi
	 * devices will be found in that range. On sn00 we would
	 * often find additional controllers in slots 5, 6 and 7
	 * but we can not lable them here as their numbers are
	 * not deterministic. Today a pci card is in slot 6 and labbeled
	 * 3 but when we add a card to slot 5 the numbering will change.
	 * Best to leave the numbering up to ioconfig.
	 */
	for (pci_slot = 0 ; pci_slot < 5 ; pci_slot++) {
		sprintf(sub_path,"/%d/%s/0/",
			pci_slot,EDGE_LBL_SCSI_CTLR);
		if (hwgraph_traverse(pci_vhdl,
				     sub_path,
				     &scsi_ctlr_vhdl) == GRAPH_SUCCESS) {
			/*REFERENCED*/
			ASSERT(control_num < 2);
			base_io_scsi_ctlr_vhdl[control_num] = scsi_ctlr_vhdl;
			/* No loop needed as we know that scsi controllers
			 * only have one piece of inventory
			 */
			device_controller_num_set(scsi_ctlr_vhdl, control_num);
			control_num++;
			hwgraph_vertex_unref(scsi_ctlr_vhdl);
		}
	}
}

#ifndef SABLE
static void
baseio_ctlr_num_set(void)
{
	char 			name[MAXDEVNAME];
	vertex_hdl_t		console_vhdl, pci_vhdl, enet_vhdl;
	extern vertex_hdl_t 	ioc3_console_vhdl_get(void);

	console_vhdl = ioc3_console_vhdl_get();
	if (console_vhdl == GRAPH_VERTEX_NONE)
		return;
	vertex_to_name(console_vhdl,name,MAXDEVNAME);

	strcat(name,__DEVSTR1);
	pci_vhdl =  hwgraph_path_to_vertex(name);
	scsi_ctlr_nums_add(pci_vhdl);
	/* Unref the pci_vhdl due to the reference by hwgraph_path_to_vertex
	 */
	hwgraph_vertex_unref(pci_vhdl);

	vertex_to_name(console_vhdl, name, MAXDEVNAME);
	strcat(name, __DEVSTR4);
	enet_vhdl = hwgraph_path_to_vertex(name);
	device_controller_num_set(enet_vhdl, 0);
	/* Unref the enet_vhdl due to the reference by hwgraph_path_to_vertex
	 */
	hwgraph_vertex_unref(enet_vhdl);
}
#else /* SABLE */
static void
baseio_ctlr_num_set(void)
{
	printf("unexpected call to baseio_ctlr_num_set");
}
#endif

void
sn00_rrb_alloc(vertex_hdl_t vhdl, int *vendor_list)
{
	/* REFERENCED */
	int rtn_val;

	/* 
	** sn00 population:		errb	orrb
	**	0- ql			3+?
	**	1- ql			        2
	**	2- ioc3 ethernet	2+?
	**	3- ioc3 secondary	        1
	**	4-                      0
	** 	5- PCI slot
	** 	6- PCI slot
	** 	7- PCI slot
	*/	
	
	/* The following code implements this heuristic for getting 
	 * maximum usage out of the rrbs
	 *
	 * constraints:
	 *  8 bit ql1 needs 1+1
	 *  ql0 or ql5,6,7 wants 1+2
	 *  ethernet wants 2 or more
	 *
	 * rules for even rrbs:
	 *  if nothing in slot 6 
	 *   4 rrbs to 0 and 2  (0xc8889999)
	 *  else 
         *   3 2 3 to slots 0 2 6  (0xc8899bbb)
	 *
         * rules for odd rrbs
	 *  if nothing in slot 5 or 7  (0xc8889999)
	 *   4 rrbs to 1 and 3
	 *  else if 1 thing in 5 or 7  (0xc8899aaa) or (0xc8899bbb)
         *   3 2 3 to slots 1 3 5|7
         *  else
         *   2 1 3 2 to slots 1 3 5 7 (note: if there's a ql card in 7 this
	 *           (0xc89aaabb)      may short what it wants therefore the
	 *			       rule should be to plug pci slots in order)
	 */


	if (vendor_list[6] != PCIIO_VENDOR_ID_NONE) {
		/* something in slot 6 */
		rtn_val = pcibr_alloc_all_rrbs(vhdl, 0, 3,1, 2,0, 0,0, 3,0);
	}
	else {
		rtn_val = pcibr_alloc_all_rrbs(vhdl, 0, 4,1, 4,0, 0,0, 0,0);
	}
	if (rtn_val)
		cmn_err(CE_WARN, "sn00_rrb_alloc: pcibr_alloc_all_rrbs failed");

	if ((vendor_list[5] != PCIIO_VENDOR_ID_NONE) && 
	    (vendor_list[7] != PCIIO_VENDOR_ID_NONE)) {
		/* soemthing in slot 5 and 7 */
		rtn_val = pcibr_alloc_all_rrbs(vhdl, 1, 2,1, 1,0, 3,0, 2,0);
	}
	else if (vendor_list[5] != PCIIO_VENDOR_ID_NONE) {
		/* soemthing in slot 5 but not 7 */
		rtn_val = pcibr_alloc_all_rrbs(vhdl, 1, 3,1, 2,0, 3,0, 0,0);
	}
	else if (vendor_list[7] != PCIIO_VENDOR_ID_NONE) {
		/* soemthing in slot 7 but not 5 */
		rtn_val = pcibr_alloc_all_rrbs(vhdl, 1, 3,1, 2,0, 0,0, 3,0);
	}
	else {
		/* nothing in slot 5 or 7 */
		rtn_val = pcibr_alloc_all_rrbs(vhdl, 1, 4,1, 4,0, 0,0, 0,0);
	}
	if (rtn_val)
		cmn_err(CE_WARN, "sn00_rrb_alloc: pcibr_alloc_all_rrbs failed");
}
/*
 * xbox_vmc
 *	On recognizing the xbox_dual_xtown card this routine is called
 * 	to add the io prom specific inventory information. This is used
 * 	by the unix flash command to flash the io prom for XBOX.
 */
static void
xbox_vmc(vertex_hdl_t vhdl)
{
	int		rv;
	vertex_hdl_t	contr;
	/*
	 * Here we lable the controller node on the IO6 as being a prom.
	 * This is used by the flash command
	 */
	rv = hwgraph_traverse(vhdl, EDGE_LBL_PCI "/" EDGE_LBL_CONTROLLER, 
			      &contr);
	ASSERT(rv == GRAPH_SUCCESS);
	rv = rv;
#if defined(DEBUG)
	printf("xbox_vmc: called with %v\n",vhdl);
#endif
	device_inventory_add(contr, INV_PROM, INV_IO6PROM,
			     NULL, NULL, NULL);

}
static
void
sn00_vmc(vertex_hdl_t vhdl)
{
	int 		s;
	cnodeid_t	cnode = cnodeid();
#if DEBUG && ATTACH_DEBUG 
	printf("looks like a SN00 base IO\n");
#endif
	device_inventory_add(vhdl, INV_IOBD,
			     INV_O200IO, NODE_MODULEID(cnode),
			     NODE_SLOTID(cnode), 0);
	pcibr_hints_dualslot(vhdl, 2, 3);
	pcibr_hints_subdevs(vhdl, 2,
			    IOC3_SDB_ECPP	|
			    IOC3_SDB_ETHER	|
			    IOC3_SDB_GENERIC	|
			    IOC3_SDB_NIC	|
			    IOC3_SDB_SERIAL	|
			    IOC3_SDB_RT		|
			    IOC3_SDB_KBMS);
	pcibr_hints_fix_rrbs(vhdl);

	pcibr_set_rrb_callback(vhdl, sn00_rrb_alloc);

	/*
	 * Take it out of reset mode now. Also on the slave turn on
	 * UST clock from remote hub. We do it two times to work
	 * around a bug in the P0 version of the IP29 board.
	 */

	s = splprof();
	if (SLOTNUM_GETSLOT(get_my_slotid()) == 1) {
	    LOCAL_HUB_S(MD_LED0, 0x1);
	    LOCAL_HUB_S(MD_LED0, 0x1);
	} else {
	    LOCAL_HUB_S(MD_LED0, 0x9);
	    LOCAL_HUB_S(MD_LED0, 0x9);
	}

	splx(s);
}


/*
 * Initialize all I/O devices.  Starting closest to nodes, probe and
 * initialize outward.
 */
void
init_all_devices(void)
{
	extern int io_init_node_threads;
	cnodeid_t cnodeid, active;

	initnsema(&io_init_sema, 0, "io_init");
	nic_vmc_add(SN00_PART_NUM , sn00_vmc);
	nic_vmc_add(SN00A_PART_NUM, sn00_vmc);
	nic_vmc_add(SN00B_PART_NUM, sn00_vmc);
	nic_vmc_add(SN00C_PART_NUM, sn00_vmc);
	nic_vmc_add(SN00D_PART_NUM, sn00_vmc);
	nic_vmc_add(XBOX_XTOWN_PART_NUM, xbox_vmc);

#ifdef AA_DEBUG
	cmn_err(CE_DEBUG, "init_all_devices: Starting ..... %d\n", lbolt);
#endif

	active = 0;
	for (cnodeid = 0; cnodeid < maxnodes; cnodeid++) {
		char thread_name[16];
		extern int io_init_pri;
		/*
		 * Spawn a service thread for each node to initialize all
		 * I/O on that node.  Each thread attempts to bind itself 
		 * to the node whose I/O it's initializing.
		 */
		sprintf(thread_name, "IO_init[%d]", cnodeid);
		(void)sthread_create(thread_name, 0, IOINIT_STKSZ, 0,
			io_init_pri, KT_PS, (st_func_t *)io_init_node,
			(void *)(long)cnodeid, 0, 0, 0);

		/* Limit how many nodes go at once, to not overload hwgraph */
		/* TBD: Should timeout */
		active++;
#ifdef AA_DEBUG
		printf("started thread for cnode %d\n", cnodeid);
#endif
		if (io_init_node_threads && 
			active >= io_init_node_threads) {
			psema(&io_init_sema, PZERO);
			active--;
		}
	}

	/* Wait until all IO_init threads are done */
	while (active > 0) {
#ifdef AA_DEBUG
	    printf("waiting, %d still active\n", active);
#endif
	    psema(&io_init_sema, PZERO);
	    active--;
	}

#ifdef AA_DEBUG
	cmn_err(CE_DEBUG, "init_all_devices: DONE ..... %d\n", lbolt);
#endif

	for (cnodeid = 0; cnodeid < maxnodes; cnodeid++)
		/*
	 	 * Update information generated by IO init.
		 */
		update_node_information(cnodeid);

#if defined (HUB_II_IFDR_WAR)
	graph_done = 1;
#endif	/* HUB_II_IFDR_WAR */
	baseio_ctlr_num_set();

#if HWG_PRINT
	hwgraph_print();
#endif

}


void
devnamefromarcs(char *devnm)
{
	int 			val;
	char 			tmpnm[MAXDEVNAME];
	char 			*tmp1, *tmp2;
	
	val = strncmp(devnm, "dks", 3);
	if (val != 0) 
		return;
	tmp1 = devnm + 3;
	val = 0;
	/* On sn0 and sn00 the controller will never be 
	 * anything other then 0-9 (currently 0 or 1), if this 
	 * changes then we need to change this to read in the
	 * entire value and convert the string to a number
	 */
	if(tmp1[1] != 'd')
		return;
	val = tmp1[0] - '0';
	if (val != 0 && val != 1) {
		printf("Only controller numbers 0 and 1 are supported for\n");
		printf("prom \"root\" variables of the form dkscXdXsX.\n");
		printf("To use another disk you must use the full path\n");
		printf("to the disk from the hardware graph\n");
		if (kdebug)
			debug("ring");
		DELAY(15000000);
		prom_reboot();
		/* NOTREACHED */
	}
		
	tmp1 += 2;
	ASSERT(base_io_scsi_ctlr_vhdl[val]);
	vertex_to_name(base_io_scsi_ctlr_vhdl[val],
		       tmpnm,
		       MAXDEVNAME);
	tmp2 = 	tmpnm + strlen(tmpnm);
	strcpy(tmp2, __DEVSTR2);
	tmp2 += strlen(__DEVSTR2);
	while (*tmp1 != 's') {
		if((*tmp2++ = *tmp1++) == '\0')
			return;
	}	
	tmp1++;
	strcpy(tmp2, __DEVSTR3);
	tmp2 += strlen(__DEVSTR3);
	while (*tmp2++ = *tmp1++)
		;
	tmp2--;
	*tmp2++ = '/';
	strcpy(tmp2, EDGE_LBL_BLOCK);
	strcpy(devnm,tmpnm);
}
