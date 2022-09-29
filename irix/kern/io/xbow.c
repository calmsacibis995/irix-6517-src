/**************************************************************************
 *									  *
 *		    Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "io/xbow.c: $Revision: 1.79 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/invent.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/edt.h>
#include <sys/dmamap.h>
#include <sys/pio.h>
#include <sys/cdl.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/PCI/bridge.h>
#include <sys/pci_intf.h>
#include <sys/sema.h>
#include <sys/ddi.h>
#include <sys/driver.h>
#include <sys/idbgentry.h>

#include <sys/xtalk/xtalk_private.h>

/*
 * Files needed to get the device driver entry points
 */
#include <sys/cred.h>
#include <ksys/ddmap.h>
#include <sys/errno.h>

#include <sys/xtalk/xbow.h>
#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xswitch.h>
#include <sys/xtalk/xwidget.h>

#include <sys/vnode.h>
#include <ksys/vfile.h>

#if IP30
#include <sys/RACER/IP30.h>
#endif

#if IP27
#include <sys/SN/SN0/sn0.h>
#endif

#include <sys/prio.h>
#include <sys/grio.h>
#include <sys/capability.h>

#define NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

int                     xbow_devflag = D_MP;

/*
 * This file supports the Xbow chip.  Main functions: initializtion,
 * error handling, and GBR.
 */

/*
 * each vertex corresponding to an xbow chip
 * has a "fastinfo" pointer pointing at one
 * of these things.
 */
typedef struct xbow_soft_s *xbow_soft_t;

struct xbow_soft_s {
    vertex_hdl_t            conn;	/* our connection point */
    vertex_hdl_t            vhdl;	/* xbow's private vertex */
    vertex_hdl_t            busv;	/* the xswitch vertex */
    xbow_t                 *base;	/* PIO pointer to crossbow chip */
    char                   *name;	/* hwgraph name */

    xbow_perf_t             xbow_perfcnt[XBOW_PERF_COUNTERS];
    xbow_perf_link_t        xbow_perflink[MAX_XBOW_PORTS];
    xbow_link_status_t      xbow_link_status[MAX_XBOW_PORTS];
    lock_t                  xbow_perf_lock;
    int                     link_monitor;
    widget_cfg_t	   *wpio[MAX_XBOW_PORTS];	/* cached PIO pointer */

    /* Bandwidth allocation state. Bandwidth values are for the
     * destination port since contention happens there.
     * Implicit mapping from xbow ports (8..f) -> (0..7) array indices.
     */
    lock_t		    xbow_bw_alloc_lock;		/* bw allocation lock */
    unsigned long long	    bw_hiwm[MAX_XBOW_PORTS];	/* hiwater mark values */
    unsigned long long      bw_cur_used[MAX_XBOW_PORTS]; /* bw used currently */
};

#define xbow_soft_set(v,i)	hwgraph_fastinfo_set((v), (arbitrary_info_t)(i))
#define xbow_soft_get(v)	((xbow_soft_t)hwgraph_fastinfo_get((v)))

/*
 * Function Table of Contents
 */

void                    xbow_mlreset(xbow_t *);
void                    xbow_init(void);
int                     xbow_attach(vertex_hdl_t);

int                     xbow_open(dev_t *, int, int, cred_t *);
int                     xbow_close(dev_t, int, int, cred_t *);

int                     xbow_map(dev_t, vhandl_t *, off_t, size_t, uint_t);
int                     xbow_unmap(dev_t, vhandl_t *);
int                     xbow_ioctl(dev_t, int, void *, int, struct cred *, int *);

int                     xbow_widget_present(xbow_t *, int);
static int              xbow_link_alive(xbow_t *, int);
vertex_hdl_t            xbow_widget_lookup(vertex_hdl_t, int);

static void             xbow_setwidint(xtalk_intr_t);
void                    xbow_intr_preset(void *, int, xwidgetnum_t, iopaddr_t, xtalk_intr_vector_t);

static void             xbow_errintr_handler(intr_arg_t);

static error_handler_f  xbow_error_handler;

void                    xbow_update_perf_counters(vertex_hdl_t);
xbow_perf_link_t       *xbow_get_perf_counters(vertex_hdl_t);
int                     xbow_enable_perf_counter(vertex_hdl_t, int, int, int);
xbow_link_status_t     *xbow_get_llp_status(vertex_hdl_t);
void                    xbow_update_llp_status(vertex_hdl_t);

int                     xbow_disable_llp_monitor(vertex_hdl_t);
int                     xbow_enable_llp_monitor(vertex_hdl_t);

int			xbow_prio_bw_alloc(dev_t, xwidgetnum_t, xwidgetnum_t,
				unsigned long long, unsigned long long);

#if defined (SN0)
#if defined (HUB_II_IFDR_WAR)
int                     xbow_get_bridge_pointers(cnodeid_t, __psunsigned_t *, int *);
int                     xbow_disable(dev_t);
int                     xbow_enable(dev_t);
#endif				/* HUB_II_IFDR_WAR */
#endif				/* SN0 */

xswitch_reset_link_f    xbow_reset_link;

void                    idbg_xbowregs(__int64_t);

xswitch_provider_t      xbow_provider =
{
    xbow_reset_link,
};

/*
 *    xbow_mlreset: called at mlreset time if the
 *      platform specific code determines that there is
 *      a crossbow in a critical path that must be
 *      functional before the driver would normally get
 *      the device properly set up.
 *
 *      what do we need to do, that the boot prom can
 *      not be counted on to have already done, that is
 *      generic across all platforms using crossbows?
 */
/*ARGSUSED */
void
xbow_mlreset(xbow_t * xbow)
{
}

/*
 *    xbow_init: called with the rest of the device
 *      driver XXX_init routines. This platform *might*
 *      have a Crossbow chip, or even several, but it
 *      might have none. Register with the crosstalk
 *      generic provider so when we encounter the chip
 *      the right magic happens.
 */
void
xbow_init(void)
{
#if DEBUG && ATTACH_DEBUG
    printf("xbow_init\n");
#endif
    xwidget_driver_register(XBOW_WIDGET_PART_NUM,
			    XBOW_WIDGET_MFGR_NUM,
			    "xbow_",
			    CDL_PRI_HI);	/* attach before friends */
}

/*
 *    xbow_attach: the crosstalk provider has
 *      determined that there is a crossbow widget
 *      present, and has handed us the connection
 *      point for that vertex.
 *
 *      We not only add our own vertex, but add
 *      some "xtalk switch" data to the switch
 *      vertex (at the connect point's parent) if
 *      it does not have any.
 */

/*ARGSUSED */
int
xbow_attach(vertex_hdl_t conn)
{
    /*REFERENCED */
    graph_error_t           rc;
    vertex_hdl_t            vhdl;
    vertex_hdl_t            busv;
    xbow_t                 *xbow;
    xbow_soft_t             soft;
    int                     port;
    xswitch_info_t          info;
    xtalk_intr_t            intr_hdl;
    device_desc_t           dev_desc;
    char                    devnm[MAXDEVNAME], *s;
    xbowreg_t               id;
    int                     rev;
    int			    i;
    int			    xbow_num;
	
    /*REFERENCED */
    int                     error;

#if DEBUG && ATTACH_DEBUG
    cmn_err(CE_CONT, "%v: xbow_attach\n", conn);
#endif

    /*
     * Get a PIO pointer to the base of the crossbow
     * chip.
     */
    xbow = (xbow_t *) xtalk_piotrans_addr(conn, 0, 0, sizeof(xbow_t), 0);

    /*
     * Locate the "switch" vertex: it is the parent
     * of our connection point.
     */
    busv = hwgraph_connectpt_get(conn);
    ASSERT(busv != GRAPH_VERTEX_NONE);

    /*
     * Create our private vertex, and connect our
     * driver information to it. This makes it possible
     * for diagnostic drivers to open the crossbow
     * vertex for access to registers.
     */
    hwgraph_char_device_add(conn, EDGE_LBL_XBOW, "xbow_", &vhdl);

    /*
     * Allocate the soft state structure and attach
     * it to the xbow's vertex
     */
    NEW(soft);
    soft->conn = conn;
    soft->vhdl = vhdl;
    soft->busv = busv;
    soft->base = xbow;
    xbow_soft_set(vhdl, (arbitrary_info_t) soft);

#define XBOW_NUM_SUFFIX_FORMAT	"[xbow# %d]"

    /* Add xbow number as a suffix to the hwgraph name of the xbow.
     * This is helpful while looking at the error/warning messages.
     */
    xbow_num = xswitch_id_get(busv);
    /*
     * get the name of this xbow vertex and keep the info.
     * This is needed during errors and interupts, but as
     * long as we have it, we can use it elsewhere.
     */
    s = dev_to_name(vhdl, devnm, MAXDEVNAME);
    soft->name = kmem_alloc(strlen(s) + strlen(XBOW_NUM_SUFFIX_FORMAT) + 1, 
			    KM_SLEEP);
    sprintf(soft->name,"%s"XBOW_NUM_SUFFIX_FORMAT, s,xbow_num);

    id = xbow->xb_wid_id;
    rev = XWIDGET_REV_NUM(id);

    /*
     * Print the revision if DEBUG, or SHOW_REVS and kdebug,
     * or the xbow is downrev.
     *
     * If xbow is downrev, make it a WARNING that the
     * Crossbow is DOWNREV: these chips are not good
     * to have around, and the operator should be told.
     */
#if !DEBUG
    if (
#if SHOW_REVS
	   (kdebug) ||
#endif
	   (rev < XBOW_REV_1_1))
#endif
	cmn_err((rev < XBOW_REV_1_1) ? CE_WARN : CE_CONT,
		"%sCrossbow ASIC: rev %s (code=%d) at %s%s",
		(rev < XBOW_REV_1_1) ? "DOWNREV " : "",
		(rev == XBOW_REV_1_0) ? "1.0" :
		(rev == XBOW_REV_1_1) ? "1.1" :
		(rev == XBOW_REV_1_2) ? "1.2" :
		(rev == XBOW_REV_1_3) ? "1.3" :
		(rev == XBOW_REV_2_0) ? "2.0" :
		"unknown",
		rev, soft->name,
		(rev < XBOW_REV_1_1) ? "" : "\n");

    spinlock_init(&soft->xbow_perf_lock, "xbow_perf_lock");
    soft->xbow_perfcnt[0].xp_perf_reg = &xbow->xb_perf_ctr_a;
    soft->xbow_perfcnt[1].xp_perf_reg = &xbow->xb_perf_ctr_b;

    /* Initialization for GBR bw allocation */
    spinlock_init(&soft->xbow_bw_alloc_lock, "xbow_bw_alloc_lock");

#define	XBOW_8_BIT_PORT_BW_MAX		(400 * 1000 * 1000)	/* 400 MB/s */
#define XBOW_16_BIT_PORT_BW_MAX		(800 * 1000 * 1000)	/* 800 MB/s */

    /* Set bandwidth hiwatermark and current values */
    for (i = 0; i < MAX_XBOW_PORTS; i++) {
	soft->bw_hiwm[i] = XBOW_16_BIT_PORT_BW_MAX;	/* for now */
	soft->bw_cur_used[i] = 0;
    }

    /*
     * attach the crossbow error interrupt.
     */
    dev_desc = device_desc_dup(vhdl);
    device_desc_flags_set(dev_desc,
			  device_desc_flags_get(dev_desc) | D_INTR_ISERR);
    device_desc_intr_name_set(dev_desc, "Crossbow error");

    intr_hdl = xtalk_intr_alloc(conn, dev_desc, vhdl);
    ASSERT(intr_hdl != NULL);

    xtalk_intr_connect(intr_hdl,
		       (intr_func_t) xbow_errintr_handler,
		       (intr_arg_t) soft,
		       (xtalk_intr_setfunc_t) xbow_setwidint,
		       (void *) xbow,
		       (void *) 0);
    device_desc_free(dev_desc);

    xwidget_error_register(conn, xbow_error_handler, soft);

    /*
     * Enable xbow error interrupts
     */
    xbow->xb_wid_control = (XB_WID_CTRL_REG_ACC_IE |
			    XB_WID_CTRL_XTALK_IE);

    /*
     * take a census of the widgets present,
     * leaving notes at the switch vertex.
     */
    info = xswitch_info_new(busv);

    for (port = MAX_PORT_NUM - MAX_XBOW_PORTS;
	 port < MAX_PORT_NUM; ++port) {
	if (!xbow_link_alive(xbow, port)) {
#if DEBUG && XBOW_DEBUG
	    cmn_err(CE_CONT, "%v link %d is not alive\n",
		    busv, port);
#endif
	    continue;
	}
	if (!xbow_widget_present(xbow, port)) {
#if DEBUG && XBOW_DEBUG
	    cmn_err(CE_CONT, "%v link %d is alive but no widget is present\n",
		    busv, port);
#endif
	    continue;
	}
#if DEBUG && XBOW_DEBUG
	cmn_err(CE_CONT, "%v link %d has a widget\n",
		busv, port);
#endif

	xswitch_info_link_is_ok(info, port);
	/*
	 * Turn some error interrupts on
	 * and turn others off. The PROM has
	 * some things turned on we don't
	 * want to see (bandwidth allocation
	 * errors for instance); so if it
	 * is not listed here, it is not on.
	 */
	xbow->xb_link(port).link_control =
	    xbow->xb_link(port).link_control
	/*
	 * Turn off these bits; they are non-fatal,
	 * but we might want to save some statistics
	 * on the frequency of these errors.
	 * XXX FIXME XXX
	 */
	    & ~XB_CTRL_RCV_CNT_OFLOW_IE
	    & ~XB_CTRL_XMT_CNT_OFLOW_IE
	    & ~XB_CTRL_BNDWDTH_ALLOC_IE
	    & ~XB_CTRL_RCV_IE
	/*
	 * These are the ones we want to turn on.
	 */
	    | XB_CTRL_ILLEGAL_DST_IE
	    | XB_CTRL_OALLOC_IBUF_IE
	    | XB_CTRL_XMT_MAX_RTRY_IE
	    | XB_CTRL_MAXREQ_TOUT_IE
	    | XB_CTRL_XMT_RTRY_IE
	    | XB_CTRL_SRC_TOUT_IE
	    ;
    }

    xswitch_provider_register(busv, &xbow_provider);

    return 0;				/* attach successful */
}

/*ARGSUSED */
int
xbow_open(dev_t *devp, int oflag, int otyp, cred_t *credp)
{
    if (!_CAP_CRABLE(credp, CAP_DEVICE_MGT))
	return EPERM;
    return 0;

}

/*ARGSUSED */
int
xbow_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    return 0;
}

/*ARGSUSED */
int
xbow_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    xbow_soft_t             soft = xbow_soft_get(vhdl);
    int                     error;

    ASSERT(soft);
    len = ctob(btoc(len));
    /* XXX- this ignores the offset!!! */
    error = v_mapphys(vt, (void *) soft->base, len);
    return error;
}

/*ARGSUSED */
int
xbow_unmap(dev_t dev, vhandl_t *vt)
{
    return 0;
}

/* This contains special-case code for grio. There are plans to make
 * this general sometime in the future, but till then this should
 * be good enough.
 */
xwidgetnum_t
xbow_widget_num_get(dev_t dev)
{
	vertex_hdl_t	tdev;
	char		devname[MAXDEVNAME];
	xwidget_info_t	xwidget_info;
	int		i;
	cnodeid_t	cnodeid = CNODEID_NONE;

	vertex_to_name(dev, devname, MAXDEVNAME);

#if IP30
	/* If there is a ".connection" edge from this vertex,
	 * then it must be "/hw/node" vertex. Return the widget
	 * number for heart: 8.
	 */
	if (hwgraph_edge_get(dev, EDGE_LBL_CONN, &tdev) ==
			GRAPH_SUCCESS) {
		return ((xwidgetnum_t) 8);
	}
#elif IP27
	if ((cnodeid = nodevertex_to_cnodeid(dev)) != CNODEID_NONE) {
		ASSERT(cnodeid < maxnodes);
		return(hub_widget_id(COMPACT_TO_NASID_NODEID(cnodeid)));
	}
#endif

	/* If this is a pci controller vertex, traverse up using
	 * the ".." links to get to the widget.
	 */
	if (strstr(devname, EDGE_LBL_PCI) &&
			strstr(devname, EDGE_LBL_CONTROLLER)) {
		tdev = dev;
		for (i=0; i< 2; i++) {
			if (hwgraph_edge_get(tdev,
				HWGRAPH_EDGELBL_DOTDOT, &tdev) !=
					GRAPH_SUCCESS)
				return XWIDGET_NONE;
		}

		if ((xwidget_info = xwidget_info_chk(tdev)) != NULL) {
			return (xwidget_info_id_get(xwidget_info));
		} else {
			return XWIDGET_NONE;
		}
	}

	return XWIDGET_NONE;
}

int
xbow_ioctl(dev_t dev,
	   int cmd,
	   void *arg,
	   int flag,
	   struct cred *cr,
	   int *rvalp)
{
    vertex_hdl_t            vhdl;
    int                     error = 0;

#if defined (DEBUG)
    int                     rc;
    vertex_hdl_t            conn;
    struct xwidget_info_s  *xwidget_info;
    xbow_soft_t             xbow_soft;
#endif
    *rvalp = 0;

    vhdl = dev_to_vhdl(dev);
#if defined (DEBUG)
    xbow_soft = xbow_soft_get(vhdl);
    conn = xbow_soft->conn;

    xwidget_info = xwidget_info_get(conn);
    ASSERT_ALWAYS(xwidget_info != NULL);

    rc = xwidget_hwid_is_xswitch(&xwidget_info->w_hwid);
    ASSERT_ALWAYS(rc != 0);
#endif
    switch (cmd) {
    case XBOWIOC_PERF_ENABLE:
    case XBOWIOC_PERF_DISABLE:
	{
	    struct xbow_perfarg_t   xbow_perf_en;

	    if (!_CAP_CRABLE(cr, CAP_DEVICE_MGT)) {
		error = EPERM;
		break;
	    }
	    if ((flag & FWRITE) == 0) {
		error = EBADF;
		break;
	    }
	    if (copyin(arg, &xbow_perf_en, sizeof(xbow_perf_en))) {
		error = EFAULT;
		break;
	    }
	    if (error = xbow_enable_perf_counter(vhdl,
						 xbow_perf_en.link,
						 (cmd == XBOWIOC_PERF_DISABLE) ? 0 : xbow_perf_en.mode,
						 xbow_perf_en.counter)) {
		error = EINVAL;
		break;
	    }
	    break;
	}

    case XBOWIOC_PERF_GET:
	{
	    xbow_perf_link_t       *xbow_perf_cnt;

	    if ((flag & FREAD) == 0) {
		error = EBADF;
		break;
	    }
	    xbow_perf_cnt = xbow_get_perf_counters(vhdl);
	    ASSERT_ALWAYS(xbow_perf_cnt != NULL);

	    if (copyout((void *) xbow_perf_cnt, (void *) arg,
			MAX_XBOW_PORTS * sizeof(xbow_perf_link_t))) {
		error = EFAULT;
		break;
	    }
	    break;
	}

    case XBOWIOC_LLP_ERROR_ENABLE:
	if (!_CAP_CRABLE(cr, CAP_DEVICE_MGT)) {
	    error = EPERM;
	    break;
	}
	if (error = xbow_enable_llp_monitor(vhdl))
	    error = EINVAL;

	break;

    case XBOWIOC_LLP_ERROR_DISABLE:

	if (!_CAP_CRABLE(cr, CAP_DEVICE_MGT)) {
	    error = EPERM;
	    break;
	}
	if (error = xbow_disable_llp_monitor(vhdl))
	    error = EINVAL;

	break;

    case XBOWIOC_LLP_ERROR_GET:
	{
	    xbow_link_status_t     *xbow_llp_status;

	    if ((flag & FREAD) == 0) {
		error = EBADF;
		break;
	    }
	    xbow_llp_status = xbow_get_llp_status(vhdl);
	    ASSERT_ALWAYS(xbow_llp_status != NULL);

	    if (copyout((void *) xbow_llp_status, (void *) arg,
			MAX_XBOW_PORTS * sizeof(xbow_link_status_t))) {
		error = EFAULT;
		break;
	    }
	    break;
	}

    case GIOCSETBW:
	{
	    grio_ioctl_info_t info;
	    xwidgetnum_t src_widgetnum, dest_widgetnum;

	    if (!cap_able(CAP_DEVICE_MGT)) {
		error = EPERM;
		break;
	    }

	    if (copyin(arg, &info, sizeof(grio_ioctl_info_t))) {
		error = EFAULT;
		break;
	    }
#ifdef GRIO_DEBUG
	    printf("xbow:: prev_vhdl: %d next_vhdl: %d reqbw: %lld\n",
			info.prev_vhdl, info.next_vhdl, info.reqbw);
#endif /* GRIO_DEBUG */

	    src_widgetnum = xbow_widget_num_get(info.prev_vhdl);
	    dest_widgetnum = xbow_widget_num_get(info.next_vhdl);

	    /* Bandwidth allocation is bi-directional. Since bandwidth
	     * reservations have already been done at an earlier stage,
	     * we cannot fail here for lack of bandwidth.
	     */
	    xbow_prio_bw_alloc(dev, src_widgetnum, dest_widgetnum,
			0, info.reqbw);
	    xbow_prio_bw_alloc(dev, dest_widgetnum, src_widgetnum,
			0, info.reqbw);

	    break;
	}

    case GIOCRELEASEBW:
	{
	    grio_ioctl_info_t info;
	    xwidgetnum_t src_widgetnum, dest_widgetnum;

	    if (!cap_able(CAP_DEVICE_MGT)) {
		error = EPERM;
		break;
	    }

	    if (copyin(arg, &info, sizeof(grio_ioctl_info_t))) {
		error = EFAULT;
		break;
	    }
#ifdef GRIO_DEBUG
	    printf("xbow:: prev_vhdl: %d next_vhdl: %d reqbw: %lld\n",
			info.prev_vhdl, info.next_vhdl, info.reqbw);
#endif /* GRIO_DEBUG */

	    src_widgetnum = xbow_widget_num_get(info.prev_vhdl);
	    dest_widgetnum = xbow_widget_num_get(info.next_vhdl);

	    /* Bandwidth reservation is bi-directional. Hence, remove
	     * bandwidth reservations for both directions.
	     */
	    xbow_prio_bw_alloc(dev, src_widgetnum, dest_widgetnum,
			info.reqbw, (-1 * info.reqbw));
	    xbow_prio_bw_alloc(dev, dest_widgetnum, src_widgetnum,
			info.reqbw, (-1 * info.reqbw));

	    break;
	}

    default:
	break;

    }
    return error;
}

/*
 * xbow_widget_present: See if a device is present
 * on the specified port of this crossbow.
 */
int
xbow_widget_present(xbow_t * xbow, int port)
{
    return xbow->xb_link(port).link_aux_status & XB_AUX_STAT_PRESENT;
}

static int
xbow_link_alive(xbow_t * xbow, int port)
{
    xbwX_stat_t             xbow_linkstat;

    xbow_linkstat.linkstatus = xbow->xb_link(port).link_status;
    return (xbow_linkstat.link_alive);
}

/*
 * xbow_widget_lookup
 *      Lookup the edges connected to the xbow specified, and
 *      retrieve the handle corresponding to the widgetnum
 *      specified.
 *      If not found, return 0.
 */
vertex_hdl_t
xbow_widget_lookup(vertex_hdl_t vhdl,
		   int widgetnum)
{
    xswitch_info_t          xswitch_info;
    vertex_hdl_t            conn;

    xswitch_info = xswitch_info_get(vhdl);
#if SN0
    ASSERT_ALWAYS((widgetnum >= HUB_WIDGET_ID_MIN) &&
		  (widgetnum <= HUB_WIDGET_ID_MAX));
#endif				/* SN0 */

    conn = xswitch_info_vhdl_get(xswitch_info, widgetnum);
    return conn;
}

/*
 * xbow_setwidint: called when xtalk
 * is establishing or migrating our
 * interrupt service.
 */
static void
xbow_setwidint(xtalk_intr_t intr)
{
    xwidgetnum_t            targ = xtalk_intr_target_get(intr);
    iopaddr_t               addr = xtalk_intr_addr_get(intr);
    xtalk_intr_vector_t     vect = xtalk_intr_vector_get(intr);
    xbow_t                 *xbow = (xbow_t *) xtalk_intr_sfarg_get(intr);

    xbow_intr_preset((void *) xbow, 0, targ, addr, vect);
}

/*
 * xbow_intr_preset: called during mlreset time
 * if the platform specific code needs to route
 * an xbow interrupt before the xtalk infrastructure
 * is available for use.
 *
 * Also called from xbow_setwidint, so we don't
 * replicate the guts of the routine.
 *
 * XXX- probably should be renamed xbow_wid_intr_set or
 * something to reduce confusion.
 */
/*ARGSUSED3 */
void
xbow_intr_preset(void *which_widget,
		 int which_widget_intr,
		 xwidgetnum_t targ,
		 iopaddr_t addr,
		 xtalk_intr_vector_t vect)
{
    xbow_t                 *xbow = (xbow_t *) which_widget;

    xbow->xb_wid_int_upper = ((0xFF000000 & (vect << 24)) |
			      (0x000F0000 & (targ << 16)) |
			      XTALK_ADDR_TO_UPPER(addr));
    xbow->xb_wid_int_lower = XTALK_ADDR_TO_LOWER(addr);
}

#define	XEM_ADD_STR(s)		cmn_err(CE_CONT, "%s", (s))
#define	XEM_ADD_NVAR(n,v)	cmn_err(CE_CONT, "\t%20s: 0x%x\n", (n), (v))
#define	XEM_ADD_VAR(v)		XEM_ADD_NVAR(#v,(v))
#define XEM_ADD_IOEF(n) 	if (IOERROR_FIELDVALID(ioe,n))		    \
				    XEM_ADD_NVAR("ioe." #n,		    \
						 IOERROR_GETVALUE(ioe,n))

static void
xem_add_ioe(ioerror_t *ioe)
{
    XEM_ADD_IOEF(errortype);
    XEM_ADD_IOEF(widgetnum);
    XEM_ADD_IOEF(widgetdev);
    XEM_ADD_IOEF(srccpu);
    XEM_ADD_IOEF(srcnode);
    XEM_ADD_IOEF(errnode);
    XEM_ADD_IOEF(sysioaddr);
    XEM_ADD_IOEF(xtalkaddr);
    XEM_ADD_IOEF(busspace);
    XEM_ADD_IOEF(busaddr);
    XEM_ADD_IOEF(vaddr);
    XEM_ADD_IOEF(memaddr);
    XEM_ADD_IOEF(epc);
    XEM_ADD_IOEF(ef);
}

#define XEM_ADD_IOE()	(xem_add_ioe(ioe))

int                     xbow_xmit_retry_errors = 0;

int
xbow_xmit_retry_error(xbow_soft_t soft,
		      int port)
{
    xswitch_info_t          info;
    vertex_hdl_t            vhdl;
    widget_cfg_t           *wid;
    widgetreg_t             id;
    int                     part;
    int                     mfgr;

    wid = soft->wpio[port - BASE_XBOW_PORT];
    if (wid == NULL) {
	/* If we can't track down a PIO
	 * pointer to our widget yet,
	 * leave our caller knowing that
	 * we are interested in this
	 * interrupt if it occurs in
	 * the future.
	 */
	info = xswitch_info_get(soft->busv);
	if (!info)
	    return 1;
	vhdl = xswitch_info_vhdl_get(info, port);
	if (vhdl == GRAPH_VERTEX_NONE)
	    return 1;
	wid = (widget_cfg_t *) xtalk_piotrans_addr
	    (vhdl, 0, 0, sizeof *wid, 0);
	if (!wid)
	    return 1;
	soft->wpio[port - BASE_XBOW_PORT] = wid;
    }
    id = wid->w_id;
    part = XWIDGET_PART_NUM(id);
    mfgr = XWIDGET_MFG_NUM(id);

    /* If this thing is not a Bridge,
     * do not activate the WAR, and
     * tell our caller we do not need
     * to be called again.
     */
    if ((part != BRIDGE_WIDGET_PART_NUM) ||
	(mfgr != BRIDGE_WIDGET_MFGR_NUM))
	return 0;

    /* count how many times we
     * have picked up after
     * LLP Transmit problems.
     */
    xbow_xmit_retry_errors++;

    /* rewrite the control register
     * to fix things up.
     */
    wid->w_control = wid->w_control;
    wid->w_control;

    return 1;
}

/*
 * xbow_errintr_handler will be called if the xbow
 * sends an interrupt request to report an error.
 */
static void
xbow_errintr_handler(intr_arg_t arg)
{
    ioerror_t               ioe[1];
    xbow_soft_t             soft = (xbow_soft_t) arg;
    xbow_t                 *xbow = soft->base;
    xbowreg_t               wid_control;
    xbowreg_t               wid_stat;
    xbowreg_t               wid_err_cmdword;
    xbowreg_t               wid_err_upper;
    xbowreg_t               wid_err_lower;
    w_err_cmd_word_u        wid_err;
    ulong_t                 wid_err_addr;

    int                     fatal = 0;
    int                     dump_ioe = 0;

    wid_control = xbow->xb_wid_control;
    wid_stat = xbow->xb_wid_stat_clr;
    wid_err_cmdword = xbow->xb_wid_err_cmdword;
    wid_err_upper = xbow->xb_wid_err_upper;
    wid_err_lower = xbow->xb_wid_err_lower;
    xbow->xb_wid_err_cmdword = 0;

    wid_err_addr =
	wid_err_lower
	| (((iopaddr_t) wid_err_upper
	    & WIDGET_ERR_UPPER_ADDR_ONLY)
	   << 32);

    if (wid_stat & XB_WID_STAT_LINK_INTR_MASK) {
	int                     port;

	wid_err.r = wid_err_cmdword;

	for (port = MAX_PORT_NUM - MAX_XBOW_PORTS;
	     port < MAX_PORT_NUM; port++) {
	    if (wid_stat & XB_WID_STAT_LINK_INTR(port)) {
		xb_linkregs_t          *link = &(xbow->xb_link(port));
		xbowreg_t               link_control = link->link_control;
		xbowreg_t               link_status = link->link_status_clr;
		xbowreg_t               link_aux_status = link->link_aux_status;
		xbowreg_t               link_pend;

		link_pend = link_status & link_control &
		    (XB_STAT_ILLEGAL_DST_ERR
		     | XB_STAT_OALLOC_IBUF_ERR
		     | XB_STAT_RCV_CNT_OFLOW_ERR
		     | XB_STAT_XMT_CNT_OFLOW_ERR
		     | XB_STAT_XMT_MAX_RTRY_ERR
		     | XB_STAT_RCV_ERR
		     | XB_STAT_XMT_RTRY_ERR
		     | XB_STAT_MAXREQ_TOUT_ERR
		     | XB_STAT_SRC_TOUT_ERR
		    );

		if (link_pend & XB_STAT_ILLEGAL_DST_ERR) {
		    if (wid_err.f.sidn == port) {
			IOERROR_INIT(ioe);
			IOERROR_SETVALUE(ioe, widgetnum, port);
			IOERROR_SETVALUE(ioe, xtalkaddr, wid_err_addr);
			if (IOERROR_HANDLED ==
			    xbow_error_handler(soft,
					       IOECODE_DMA,
					       MODE_DEVERROR,
					       ioe)) {
			    link_pend &= ~XB_STAT_ILLEGAL_DST_ERR;
			} else {
			    dump_ioe++;
			}
		    }
		}
		/* Xbow/Bridge WAR:
		 * if the bridge signals an LLP Transmitter Retry,
		 * rewrite its control register.
		 * If someone else triggers this interrupt,
		 * ignore (and disable) the interrupt.
		 */
		if (link_pend & XB_STAT_XMT_RTRY_ERR) {
		    if (!xbow_xmit_retry_error(soft, port)) {
			link_control &= ~XB_CTRL_XMT_RTRY_IE;
			link->link_control = link_control;
			link->link_control;	/* stall until written */
		    }
		    link_pend &= ~XB_STAT_XMT_RTRY_ERR;
		}
		if (link_pend) {
		    vertex_hdl_t	xwidget_vhdl;
		    char		*xwidget_name;
		    
		    /* Get the widget name corresponding to the current
		     * xbow link.
		     */
		    xwidget_vhdl = xbow_widget_lookup(soft->busv,port);
		    xwidget_name = xwidget_name_get(xwidget_vhdl);

		    cmn_err(CE_CONT,
			    "%s port %X[%s] XIO Bus Error",
			    soft->name, port, xwidget_name);
		    if (link_status & XB_STAT_MULTI_ERR)
			XEM_ADD_STR("\tMultiple Errors\n");
		    if (link_status & XB_STAT_ILLEGAL_DST_ERR)
			XEM_ADD_STR("\tInvalid Packet Destination\n");
		    if (link_status & XB_STAT_OALLOC_IBUF_ERR)
			XEM_ADD_STR("\tInput Overallocation Error\n");
		    if (link_status & XB_STAT_RCV_CNT_OFLOW_ERR)
			XEM_ADD_STR("\tLLP receive error counter overflow\n");
		    if (link_status & XB_STAT_XMT_CNT_OFLOW_ERR)
			XEM_ADD_STR("\tLLP transmit retry counter overflow\n");
		    if (link_status & XB_STAT_XMT_MAX_RTRY_ERR)
			XEM_ADD_STR("\tLLP Max Transmitter Retry\n");
		    if (link_status & XB_STAT_RCV_ERR)
			XEM_ADD_STR("\tLLP Receiver error\n");
		    if (link_status & XB_STAT_XMT_RTRY_ERR)
			XEM_ADD_STR("\tLLP Transmitter Retry\n");
		    if (link_status & XB_STAT_MAXREQ_TOUT_ERR)
			XEM_ADD_STR("\tMaximum Request Timeout\n");
		    if (link_status & XB_STAT_SRC_TOUT_ERR)
			XEM_ADD_STR("\tSource Timeout Error\n");

		    {
			int                     other_port;

			for (other_port = 8; other_port < 16; ++other_port) {
			    if (link_aux_status & (1 << other_port)) {
				/* XXX- need to go to "other_port"
				 * and clean up after the timeout?
				 */
				XEM_ADD_VAR(other_port);
			    }
			}
		    }

#if !DEBUG
		    if (kdebug) {
#endif
			XEM_ADD_VAR(link_control);
			XEM_ADD_VAR(link_status);
			XEM_ADD_VAR(link_aux_status);

			if (dump_ioe) {
			    XEM_ADD_IOE();
			    dump_ioe = 0;
			}
#if !DEBUG
		    }
#endif
		    fatal++;
		}
	    }
	}
    }
    if (wid_stat & wid_control & XB_WID_STAT_WIDGET0_INTR) {
	/* we have a "widget zero" problem */

	if (wid_stat & (XB_WID_STAT_MULTI_ERR
			| XB_WID_STAT_XTALK_ERR
			| XB_WID_STAT_REG_ACC_ERR)) {

	    cmn_err(CE_CONT,
		    "%s Port 0 XIO Bus Error",
		    soft->name);
	    if (wid_stat & XB_WID_STAT_MULTI_ERR)
		XEM_ADD_STR("\tMultiple Error\n");
	    if (wid_stat & XB_WID_STAT_XTALK_ERR)
		XEM_ADD_STR("\tXIO Error\n");
	    if (wid_stat & XB_WID_STAT_REG_ACC_ERR)
		XEM_ADD_STR("\tRegister Access Error\n");

	    fatal++;
	}
    }
    if (fatal) {
	XEM_ADD_VAR(wid_stat);
	XEM_ADD_VAR(wid_control);
	XEM_ADD_VAR(wid_err_cmdword);
	XEM_ADD_VAR(wid_err_upper);
	XEM_ADD_VAR(wid_err_lower);
	XEM_ADD_VAR(wid_err_addr);
	cmn_err_tag(8, CE_PANIC, "XIO Bus Error");
    }
}

/*
 * XBOW ERROR Handling routines.
 * These get invoked as part of walking down the error handling path
 * from hub/heart towards the I/O device that caused the error.
 */

/*
 * xbow_error_handler
 *      XBow error handling dispatch routine.
 *      This is the primary interface used by external world to invoke
 *      in case of an error related to a xbow.
 *      Only functionality in this layer is to identify the widget handle
 *      given the widgetnum. Otherwise, xbow does not gathers any error
 *      data.
 */
static int
xbow_error_handler(
		      void *einfo,
		      int error_code,
		      ioerror_mode_t mode,
		      ioerror_t *ioerror)
{
    int                     retval = IOERROR_WIDGETLEVEL;

    xbow_soft_t             soft = (xbow_soft_t) einfo;
    int                     port;
    vertex_hdl_t            conn;
    vertex_hdl_t            busv;

    xbow_t                 *xbow = soft->base;
    xbowreg_t               wid_stat;
    xbowreg_t               wid_err_cmdword;
    xbowreg_t               wid_err_upper;
    xbowreg_t               wid_err_lower;
    ulong_t                 wid_err_addr;

    xb_linkregs_t          *link;
    xbowreg_t               link_control;
    xbowreg_t               link_status;
    xbowreg_t               link_aux_status;

    ASSERT(soft != 0);
    busv = soft->busv;

#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%s: xbow_error_handler\n", soft->name, busv);
#endif

    port = IOERROR_GETVALUE(ioerror, widgetnum);

    if (port == 0) {
	/* error during access to xbow:
	 * do NOT attempt to access xbow regs.
	 */
	if (mode == MODE_DEVPROBE)
	    return IOERROR_HANDLED;

	if (error_code & IOECODE_DMA) {
	    cmn_err(CE_ALERT,
		    "DMA error blamed on Crossbow at %s\n"
		    "\tbut Crosbow never initiates DMA!",
		    soft->name);
	}
	if (error_code & IOECODE_PIO) {
	    cmn_err(CE_ALERT,
		    "PIO Error on XIO Bus %s\n"
		    "\tattempting to access XIO controller\n"
		    "\twith offset 0x%X",
		    soft->name,
		    IOERROR_GETVALUE(ioerror, xtalkaddr));
	}
	/* caller will dump contents of ioerror
	 * in DEBUG and kdebug kernels.
	 */

	return retval;
    }
    /*
     * error not on port zero:
     * safe to read xbow registers.
     */
    wid_stat = xbow->xb_wid_stat;
    wid_err_cmdword = xbow->xb_wid_err_cmdword;
    wid_err_upper = xbow->xb_wid_err_upper;
    wid_err_lower = xbow->xb_wid_err_lower;

    wid_err_addr =
	wid_err_lower
	| (((iopaddr_t) wid_err_upper
	    & WIDGET_ERR_UPPER_ADDR_ONLY)
	   << 32);

    if ((port < BASE_XBOW_PORT) ||
	(port >= MAX_PORT_NUM)) {

	if (mode == MODE_DEVPROBE)
	    return IOERROR_HANDLED;

	if (error_code & IOECODE_DMA) {
	    cmn_err(CE_ALERT,
		    "DMA error blamed on XIO port at %s/%d\n"
		    "\tbut Crossbow does not support that port",
		    soft->name, port);
	}
	if (error_code & IOECODE_PIO) {
	    cmn_err(CE_ALERT,
		    "PIO Error on XIO Bus %s\n"
		    "\tattempting to access XIO port %d\n"
		    "\t(which Crossbow does not support)"
		    "\twith offset 0x%X",
		    soft->name, port,
		    IOERROR_GETVALUE(ioerror, xtalkaddr));
	}
#if !DEBUG
	if (kdebug) {
#endif
	    XEM_ADD_STR("Raw status values for Crossbow:\n");
	    XEM_ADD_VAR(wid_stat);
	    XEM_ADD_VAR(wid_err_cmdword);
	    XEM_ADD_VAR(wid_err_upper);
	    XEM_ADD_VAR(wid_err_lower);
	    XEM_ADD_VAR(wid_err_addr);
#if !DEBUG
	}
#endif

	/* caller will dump contents of ioerror
	 * in DEBUG and kdebug kernels.
	 */

	return retval;
    }
    /* access to valid port:
     * ok to check port status.
     */

    link = &(xbow->xb_link(port));
    link_control = link->link_control;
    link_status = link->link_status;
    link_aux_status = link->link_aux_status;

    /* Check that there is something present
     * in that XIO port.
     */
    if (!(link_aux_status & XB_AUX_STAT_PRESENT)) {
	/* nobody connected. */
	if (mode == MODE_DEVPROBE)
	    return IOERROR_HANDLED;

	if (error_code & IOECODE_DMA) {
	    cmn_err(CE_ALERT,
		    "DMA error blamed on XIO port at %s/%d\n"
		    "\tbut there is no device connected there.",
		    soft->name, port);
	}
	if (error_code & IOECODE_PIO) {
	    cmn_err(CE_ALERT,
		    "PIO Error on XIO Bus %s\n"
		    "\tattempting to access XIO port %d\n"
		    "\t(which has no device connected)"
		    "\twith offset 0x%X",
		    soft->name, port,
		    IOERROR_GETVALUE(ioerror, xtalkaddr));
	}
#if !DEBUG
	if (kdebug) {
#endif
	    XEM_ADD_STR("Raw status values for Crossbow:\n");
	    XEM_ADD_VAR(wid_stat);
	    XEM_ADD_VAR(wid_err_cmdword);
	    XEM_ADD_VAR(wid_err_upper);
	    XEM_ADD_VAR(wid_err_lower);
	    XEM_ADD_VAR(wid_err_addr);
	    XEM_ADD_VAR(port);
	    XEM_ADD_VAR(link_control);
	    XEM_ADD_VAR(link_status);
	    XEM_ADD_VAR(link_aux_status);
#if !DEBUG
	}
#endif
	return retval;

    }
    /* Check that the link is alive.
     */
    if (!(link_status & XB_STAT_LINKALIVE)) {
	/* nobody connected. */
	if (mode == MODE_DEVPROBE)
	    return IOERROR_HANDLED;

	cmn_err(CE_ALERT,
		"%s%sError on XIO Bus %s port %d",
		(error_code & IOECODE_DMA) ? "DMA " : "",
		(error_code & IOECODE_PIO) ? "PIO " : "",
		soft->name, port);

	if ((error_code & IOECODE_PIO) &&
	    (IOERROR_FIELDVALID(ioerror, xtalkaddr))) {
	    cmn_err(CE_CONT,
		    "\tAccess attempted to offset 0x%X\n",
		    IOERROR_GETVALUE(ioerror, xtalkaddr));
	}
	if (link_aux_status & XB_AUX_LINKFAIL_RST_BAD)
	    XEM_ADD_STR("\tLink never came out of reset\n");
	else
	    XEM_ADD_STR("\tLink failed while transferring data\n");

    }
    /* get the connection point for the widget
     * involved in this error; if it exists and
     * is not our connectpoint, cycle back through
     * xtalk_error_handler to deliver control to
     * the proper handler (or to report a generic
     * crosstalk error).
     *
     * If the downstream handler won't handle
     * the problem, we let our upstream caller
     * deal with it, after (in DEBUG and kdebug
     * kernels) dumping the xbow state for this
     * port.
     */
    conn = xbow_widget_lookup(busv, port);
    if ((conn != GRAPH_VERTEX_NONE) &&
	(conn != soft->conn)) {
	retval = xtalk_error_handler(conn, error_code, mode, ioerror);
	if (retval == IOERROR_HANDLED)
	    return IOERROR_HANDLED;
    }
    if (mode == MODE_DEVPROBE)
	return IOERROR_HANDLED;

    if (retval == IOERROR_UNHANDLED) {
	retval = IOERROR_PANIC;

	cmn_err(CE_ALERT,
		"%s%sError on XIO Bus %s port %d",
		(error_code & IOECODE_DMA) ? "DMA " : "",
		(error_code & IOECODE_PIO) ? "PIO " : "",
		soft->name, port);

	if ((error_code & IOECODE_PIO) &&
	    (IOERROR_FIELDVALID(ioerror, xtalkaddr))) {
	    cmn_err(CE_CONT,
		    "\tAccess attempted to offset 0x%X\n",
		    IOERROR_GETVALUE(ioerror, xtalkaddr));
	}
    }

#if !DEBUG
    if (kdebug) {
#endif
	XEM_ADD_STR("Raw status values for Crossbow:\n");
	XEM_ADD_VAR(wid_stat);
	XEM_ADD_VAR(wid_err_cmdword);
	XEM_ADD_VAR(wid_err_upper);
	XEM_ADD_VAR(wid_err_lower);
	XEM_ADD_VAR(wid_err_addr);
	XEM_ADD_VAR(port);
	XEM_ADD_VAR(link_control);
	XEM_ADD_VAR(link_status);
	XEM_ADD_VAR(link_aux_status);
#if !DEBUG
    }
#endif
    /* caller will dump raw ioerror data
     * in DEBUG and kdebug kernels.
     */

    return retval;
}

void
xbow_update_perf_counters(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_perf_t            *xbow_perf = xbow_soft->xbow_perfcnt;
    xbow_perf_link_t       *xbow_plink = xbow_soft->xbow_perflink;
    xbow_perfcount_t        perf_reg;
    int                     link, s, i;

    for (i = 0; i < XBOW_PERF_COUNTERS; i++, xbow_perf++) {
	if (xbow_perf->xp_mode == XBOW_MONITOR_NONE)
	    continue;

	s = mutex_spinlock(&xbow_soft->xbow_perf_lock);

	perf_reg.xb_counter_val = *(xbowreg_t *) xbow_perf->xp_perf_reg;

	link = perf_reg.xb_perf.link_select;

	(xbow_plink + link)->xlp_cumulative[xbow_perf->xp_curmode] +=
	    ((perf_reg.xb_perf.count - xbow_perf->xp_current) & XBOW_COUNTER_MASK);
	xbow_perf->xp_current = perf_reg.xb_perf.count;

	mutex_spinunlock(&xbow_soft->xbow_perf_lock, s);
    }
    /* Do port /mode multiplexing here */

    (void) timeout(xbow_update_perf_counters,
		   (void *) (__psunsigned_t) vhdl, XBOW_PERF_TIMEOUT);

}

xbow_perf_link_t       *
xbow_get_perf_counters(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_perf_link_t       *xbow_perf_link = xbow_soft->xbow_perflink;

    return xbow_perf_link;
}

int
xbow_enable_perf_counter(vertex_hdl_t vhdl, int link, int mode, int counter)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_perf_t            *xbow_perf = xbow_soft->xbow_perfcnt;
    xbow_linkctrl_t         xbow_link_ctrl;
    xbow_t                 *xbow = xbow_soft->base;
    xbow_perfcount_t        perf_reg;
    int                     s, i;

    link -= BASE_XBOW_PORT;
    if ((link < 0) || (link >= MAX_XBOW_PORTS))
	return -1;

    if ((mode < XBOW_MONITOR_NONE) || (mode > XBOW_MONITOR_DEST_LINK))
	return -1;

    if ((counter < 0) || (counter >= XBOW_PERF_COUNTERS))
	return -1;

    s = mutex_spinlock(&xbow_soft->xbow_perf_lock);

    if ((xbow_perf + counter)->xp_mode && mode) {
	mutex_spinunlock(&xbow_soft->xbow_perf_lock, s);
	return -1;
    }
    for (i = 0; i < XBOW_PERF_COUNTERS; i++) {
	if (i == counter)
	    continue;
	if (((xbow_perf + i)->xp_link == link) &&
	    ((xbow_perf + i)->xp_mode)) {
	    mutex_spinunlock(&xbow_soft->xbow_perf_lock, s);
	    return -1;
	}
    }
    xbow_perf += counter;

    xbow_perf->xp_curlink = xbow_perf->xp_link = link;
    xbow_perf->xp_curmode = xbow_perf->xp_mode = mode;

    xbow_link_ctrl.xbl_ctrlword = xbow->xb_link_raw[link].link_control;
    xbow_link_ctrl.xb_linkcontrol.perf_mode = mode;
    xbow->xb_link_raw[link].link_control = xbow_link_ctrl.xbl_ctrlword;

    perf_reg.xb_counter_val = *(xbowreg_t *) xbow_perf->xp_perf_reg;
    perf_reg.xb_perf.link_select = link;
    *(xbowreg_t *) xbow_perf->xp_perf_reg = perf_reg.xb_counter_val;
    xbow_perf->xp_current = perf_reg.xb_perf.count;

    (void) timeout(xbow_update_perf_counters,
		   (void *) (__psunsigned_t) vhdl, XBOW_PERF_TIMEOUT);

    mutex_spinunlock(&xbow_soft->xbow_perf_lock, s);

    return 0;
}

xbow_link_status_t     *
xbow_get_llp_status(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_link_status_t     *xbow_llp_status = xbow_soft->xbow_link_status;

    return xbow_llp_status;
}

void
xbow_update_llp_status(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_link_status_t     *xbow_llp_status = xbow_soft->xbow_link_status;
    xbow_t                 *xbow;
    xbwX_stat_t             lnk_sts;
    xbow_aux_link_status_t  aux_sts;
    int                     link;
    vertex_hdl_t	    xwidget_vhdl;
    char		   *xwidget_name;	

    xbow = (xbow_t *) xbow_soft->base;
    for (link = 0; link < MAX_XBOW_PORTS; link++, xbow_llp_status++) {
	/* Get the widget name corresponding the current link.
	 * Note : 0 <= link < MAX_XBOW_PORTS(8).
	 * 	  BASE_XBOW_PORT(0x8) <= xwidget number < MAX_PORT_NUM (0x10)
	 */
	xwidget_vhdl = xbow_widget_lookup(xbow_soft->busv,link+BASE_XBOW_PORT);
	xwidget_name = xwidget_name_get(xwidget_vhdl);
	aux_sts.aux_linkstatus
	    = xbow->xb_link_raw[link].link_aux_status;
	lnk_sts.linkstatus = xbow->xb_link_raw[link].link_status_clr;

	if (lnk_sts.link_alive == 0)
	    continue;

	xbow_llp_status->rx_err_count +=
	    aux_sts.xb_aux_linkstatus.rx_err_cnt;

	xbow_llp_status->tx_retry_count +=
	    aux_sts.xb_aux_linkstatus.tx_retry_cnt;

	if (lnk_sts.linkstatus & ~(XB_STAT_RCV_ERR | XB_STAT_XMT_RTRY_ERR | XB_STAT_LINKALIVE)) {
	    cmn_err(CE_WARN, "link %d[%s]: bad status 0x%x\n",
		    link, xwidget_name, lnk_sts.linkstatus);
	}
    }
    if (xbow_soft->link_monitor)
	(void) timeout(xbow_update_llp_status,
		       (void *) (__psunsigned_t) vhdl, XBOW_STATS_TIMEOUT);
}

int
xbow_disable_llp_monitor(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    int                     port;

    for (port = 0; port < MAX_XBOW_PORTS; port++) {
	xbow_soft->xbow_link_status[port].rx_err_count = 0;
	xbow_soft->xbow_link_status[port].tx_retry_count = 0;
    }

    xbow_soft->link_monitor = 0;
    return 0;
}

int
xbow_enable_llp_monitor(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);

    (void) timeout(xbow_update_llp_status,
		   (void *) (__psunsigned_t) vhdl, XBOW_STATS_TIMEOUT);
    xbow_soft->link_monitor = 1;
    return 0;
}

#if defined (SN0)
#if defined (HUB_II_IFDR_WAR)

#include <sys/nodepda.h>

#define xwidget_hwid_is_bridge(_hwid)	\
		(((_hwid)->part_num == BRIDGE_WIDGET_PART_NUM ) &&	  \
		 ((_hwid)->mfg_num == BRIDGE_WIDGET_MFGR_NUM ))

extern device_driver_t  driver_get(vertex_hdl_t vhdl);

int
xbow_get_bridge_pointers(cnodeid_t cnode, __psunsigned_t * bridges, int *num)
{
    vertex_hdl_t            hubv, xswitch, xconn, conn;
    struct xwidget_info_s  *xwidget_info;
    nasid_t                 nasid = COMPACT_TO_NASID_NODEID(cnode);
    int                     rc;
    int                     i;
    char                    label[LABEL_LENGTH_MAX];
    long                    place;

    for (i = 0; i <= HUB_WIDGET_ID_MAX; i++)
	bridges[i] = 0;

    hubv = cnodeid_to_vertex(cnode);
    ASSERT(hubv != GRAPH_VERTEX_NONE);

    /* from the hub, find the switch vertex and its connection point. */

    rc = hwgraph_traverse(hubv, EDGE_LBL_XTALK, &xswitch);
    if (rc != GRAPH_SUCCESS)
	return 0;

    rc = hwgraph_traverse(xswitch, "0", &xconn);
    if (rc != GRAPH_SUCCESS)
	return 0;

    xwidget_info = xwidget_info_get(xconn);
    ASSERT(xwidget_info);

    if (xwidget_hwid_is_xswitch(&xwidget_info->w_hwid)) {
	place = EDGE_PLACE_WANT_REAL_EDGES;
	while (hwgraph_edge_get_next(xswitch, label, &conn, &place)
	       == GRAPH_SUCCESS) {
	    if (!(xwidget_info = xwidget_info_get(conn))) {
		continue;
	    }
	    if (xwidget_hwid_is_bridge(&xwidget_info->w_hwid)) {
		/* get bridge base */
		bridges[xwidget_info->w_id] =
		    NODE_SWIN_BASE(nasid, xwidget_info->w_id);
		(*num)++;
	    }
	}
    } else {
	if (xwidget_hwid_is_bridge(&xwidget_info->w_hwid)) {
	    /* get bridge base */
	    bridges[0] = NODE_SWIN_BASE(nasid, xwidget_info->w_id);
	    (*num)++;
	}
    }
    return 0;
}

int
xbow_disable(dev_t xbow_dev)
{
    xbow_soft_t             soft;
    vertex_hdl_t            vhdl, busv, conn;
    char                    label[LABEL_LENGTH_MAX];	/* Label for widget */
    long                    place;
    int                     rc = -1;

    cmn_err(CE_ALERT, "%v: xbow_disable does not work\n", xbow_dev);

    vhdl = dev_to_vhdl(xbow_dev);
    soft = xbow_soft_get(vhdl);
    busv = soft->busv;
    place = EDGE_PLACE_WANT_REAL_EDGES;

    while (hwgraph_edge_get_next(busv, label, &conn, &place) == GRAPH_SUCCESS) {

#if 0					/* XXX- this does not work -- wrong vertex! */

	device_driver_t         driver;
	struct cdevsw          *my_cdevsw;
	struct bdevsw          *my_bdevsw;
	dev_t                   dev;

	dev = vhdl_to_dev(conn);
	driver = driver_get(conn);

	device_driver_devsw_get(driver, &my_bdevsw, &my_cdevsw);

	if (my_cdevsw)
	    rc = cddisable(my_cdevsw, dev);
#endif
    }
    return rc;
}

int
xbow_enable(dev_t xbow_dev)
{
    vertex_hdl_t            vhdl;
    vertex_hdl_t            conn;
    char                    label[LABEL_LENGTH_MAX];	/* Label for widget */
    long                    place;
    int                     rc = -1;

    cmn_err(CE_ALERT, "%v: xbow_enable does not work\n", xbow_dev);

    place = EDGE_PLACE_WANT_REAL_EDGES;
    vhdl = dev_to_vhdl(xbow_dev);

    while (hwgraph_edge_get_next(vhdl, label, &conn, &place) == GRAPH_SUCCESS) {

#if 0					/* XXX- this does not work -- wrong vertex! */

	device_driver_t         driver;
	struct cdevsw          *my_cdevsw;
	struct bdevsw          *my_bdevsw;
	dev_t                   dev;

	dev = vhdl_to_dev(conn);
	driver = driver_get(conn);

	device_driver_devsw_get(driver, &my_bdevsw, &my_cdevsw);

	if (my_cdevsw)
	    rc = cdenable(my_cdevsw, dev);
#endif
    }
    return rc;
}
#endif				/* HUB_II_IFDR_WAR */
#endif				/* SN0 */

int
xbow_reset_link(vertex_hdl_t xconn_vhdl)
{
    xwidget_info_t          widget_info;
    xwidgetnum_t            port;
    xbow_t                 *xbow;
    xbowreg_t               ctrl;
    xbwX_stat_t             stat;
    unsigned                itick;
    unsigned                dtick;
    static int              ticks_per_ms = 0;

    if (!ticks_per_ms) {
	itick = get_timestamp();
	us_delay(1000);
	ticks_per_ms = get_timestamp() - itick;
    }
    widget_info = xwidget_info_get(xconn_vhdl);
    port = xwidget_info_id_get(widget_info);

#ifdef XBOW_K1PTR			/* defined if we only have one xbow ... */
    xbow = XBOW_K1PTR;
#else
    {
	vertex_hdl_t            xbow_vhdl;
	xbow_soft_t             xbow_soft;

	hwgraph_traverse(xconn_vhdl, ".master/xtalk/0/xbow", &xbow_vhdl);
	xbow_soft = xbow_soft_get(xbow_vhdl);
	xbow = xbow_soft->base;
    }
#endif

    /*
     * This requires three PIOs (reset the link, check for the
     * reset, restore the control register for the link) plus
     * 10us to wait for the reset. We allow up to 1ms for the
     * widget to come out of reset before giving up and
     * returning a failure.
     */
    ctrl = xbow->xb_link(port).link_control;
    xbow->xb_link(port).link_reset = 0;
    itick = get_timestamp();
    while (1) {
	stat.linkstatus = xbow->xb_link(port).link_status;
	if (stat.link_alive)
	    break;
	dtick = get_timestamp() - itick;
	if (dtick > ticks_per_ms) {
	    return -1;			/* never came out of reset */
	}
	DELAY(2);			/* don't beat on link_status */
    }
    xbow->xb_link(port).link_control = ctrl;
    return 0;
}

/*
 * Dump xbow registers.
 * input parameter is either a pointer to
 * the xbow chip or the vertex handle for
 * an xbow vertex.
 */
void
idbg_xbowregs(__int64_t regs)
{
    xbow_t                 *xbow;
    int                     i;
    xb_linkregs_t          *link;

    if (dev_is_vertex((dev_t) regs)) {
	vertex_hdl_t            vhdl = (vertex_hdl_t) regs;
	xbow_soft_t             soft = xbow_soft_get(vhdl);

	xbow = soft->base;
    } else {
	xbow = (xbow_t *) regs;
    }

    qprintf("Printing xbow registers starting at 0x%x\n", xbow);
    qprintf("wid %x status %x erruppr %x errlower %x control %x timeout %x\n",
	    xbow->xb_wid_id, xbow->xb_wid_stat, xbow->xb_wid_err_upper,
	    xbow->xb_wid_err_lower, xbow->xb_wid_control,
	    xbow->xb_wid_req_timeout);
    qprintf("intr uppr %x lower %x errcmd %x llp ctrl %x arb_reload %x\n",
	    xbow->xb_wid_int_upper, xbow->xb_wid_int_lower,
	    xbow->xb_wid_err_cmdword, xbow->xb_wid_llp,
	    xbow->xb_wid_arb_reload);

    for (i = 8; i <= 0xf; i++) {
	link = &xbow->xb_link(i);
	qprintf("Link %d registers\n", i);
	qprintf("\tctrl %x stat %x arbuppr %x arblowr %x auxstat %x\n",
		link->link_control, link->link_status,
		link->link_arb_upper, link->link_arb_lower,
		link->link_aux_status);
    }
}


#define XBOW_ARB_RELOAD_TICKS		25
					/* granularity: 4 MB/s, max: 124 MB/s */
#define GRANULARITY			((100 * 1000000) / XBOW_ARB_RELOAD_TICKS)

#define XBOW_BYTES_TO_GBR(BYTES_per_s)	(int) (BYTES_per_s / GRANULARITY)

#define XBOW_GBR_TO_BYTES(cnt)		(bandwidth_t) ((cnt) * GRANULARITY)

#define CEILING_BYTES_TO_GBR(gbr, bytes_per_sec)	\
			((XBOW_GBR_TO_BYTES(gbr) < bytes_per_sec) ? gbr+1 : gbr)

#define XBOW_ARB_GBR_MAX		31

#define ABS(x)				((x > 0) ? (x) : (-1 * x))
					/* absolute value */

int
xbow_bytes_to_gbr(bandwidth_t old_bytes_per_sec, bandwidth_t bytes_per_sec)
{
    int                     gbr_granted;
    int                     new_total_gbr;
    int                     change_gbr;
    bandwidth_t             new_total_bw;

#ifdef GRIO_DEBUG
    printf("xbow_bytes_to_gbr: old_bytes_per_sec %lld bytes_per_sec %lld\n",
		old_bytes_per_sec, bytes_per_sec);
#endif	/* GRIO_DEBUG */

    gbr_granted = CEILING_BYTES_TO_GBR((XBOW_BYTES_TO_GBR(old_bytes_per_sec)),
			old_bytes_per_sec);
    new_total_bw = old_bytes_per_sec + bytes_per_sec;
    new_total_gbr = CEILING_BYTES_TO_GBR((XBOW_BYTES_TO_GBR(new_total_bw)),
			new_total_bw);

    change_gbr = new_total_gbr - gbr_granted;

#ifdef GRIO_DEBUG
    printf("xbow_bytes_to_gbr: gbr_granted %d new_total_gbr %d change_gbr %d\n",
		gbr_granted, new_total_gbr, change_gbr);
#endif	/* GRIO_DEBUG */

    return (change_gbr);
}

/* Conversion from GBR to bytes */
bandwidth_t
xbow_gbr_to_bytes(int gbr)
{
    return (XBOW_GBR_TO_BYTES(gbr));
}

/* Given the vhdl for the desired xbow, the src and dest. widget ids
 * and the req_bw value, this xbow driver entry point accesses the
 * xbow registers and allocates the desired bandwidth if available.
 *
 * If bandwidth allocation is successful, return success else return failure.
 */
int
xbow_prio_bw_alloc(vertex_hdl_t vhdl,
		xwidgetnum_t src_wid,
		xwidgetnum_t dest_wid,
		unsigned long long old_alloc_bw,
		unsigned long long req_bw)
{
    xbow_soft_t             soft = xbow_soft_get(vhdl);
    volatile xbowreg_t     *xreg;
    xbowreg_t               mask;
    int                     s;
    int                     error = 0;
    bandwidth_t             old_bw_BYTES, req_bw_BYTES;
    xbowreg_t               old_xreg;
    int                     old_bw_GBR, req_bw_GBR, new_bw_GBR;

#ifdef GRIO_DEBUG
    printf("xbow_prio_bw_alloc: vhdl %d src_wid %d dest_wid %d req_bw %lld\n",
		(int) vhdl, (int) src_wid, (int) dest_wid, req_bw);
#endif

    ASSERT(XBOW_WIDGET_IS_VALID(src_wid));
    ASSERT(XBOW_WIDGET_IS_VALID(dest_wid));

    s = mutex_spinlock(&soft->xbow_bw_alloc_lock);

    /* Get pointer to the correct register */
    xreg = XBOW_PRIO_ARBREG_PTR(soft->base, dest_wid, src_wid);

    /* Get mask for GBR count value */
    mask = XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(src_wid);

    req_bw_GBR = xbow_bytes_to_gbr(old_alloc_bw, req_bw);
    req_bw_BYTES = (req_bw_GBR < 0) ? (-1 * xbow_gbr_to_bytes(ABS(req_bw_GBR)))
		: xbow_gbr_to_bytes(req_bw_GBR);

#ifdef GRIO_DEBUG
    printf("req_bw %lld req_bw_BYTES %lld req_bw_GBR %d\n",
		req_bw, req_bw_BYTES, req_bw_GBR);
#endif	/* GRIO_DEBUG */

    old_bw_BYTES = soft->bw_cur_used[(int) dest_wid - MAX_XBOW_PORTS];
    old_xreg = *xreg;
    old_bw_GBR = (((*xreg) & mask) >> XB_ARB_GBR_SHFT(src_wid));

#ifdef GRIO_DEBUG
    ASSERT(XBOW_BYTES_TO_GBR(old_bw_BYTES) == old_bw_GBR);

    printf("old_bw_BYTES %lld old_bw_GBR %d\n", old_bw_BYTES, old_bw_GBR);

    printf("req_bw_BYTES %lld old_bw_BYTES %lld soft->bw_hiwm %lld\n",
		req_bw_BYTES, old_bw_BYTES,
		soft->bw_hiwm[(int) dest_wid - MAX_XBOW_PORTS]);
	   
#endif				/* GRIO_DEBUG */

    /* Accept the request only if we don't exceed the destination
     * port HIWATER_MARK *AND* the max. link GBR arbitration count
     */
    if (((old_bw_BYTES + req_bw_BYTES) <=
		soft->bw_hiwm[(int) dest_wid - MAX_XBOW_PORTS]) &&
		(req_bw_GBR + old_bw_GBR <= XBOW_ARB_GBR_MAX)) {

	new_bw_GBR = (old_bw_GBR + req_bw_GBR);

	/* Set this in the xbow link register */
	*xreg = (old_xreg & ~mask) | \
	    (new_bw_GBR << XB_ARB_GBR_SHFT(src_wid) & mask);

	soft->bw_cur_used[(int) dest_wid - MAX_XBOW_PORTS] =
			xbow_gbr_to_bytes(new_bw_GBR);
    } else {
	error = 1;
    }

    mutex_spinunlock(&soft->xbow_bw_alloc_lock, s);

    return (error);
}
