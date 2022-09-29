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

#ident "io/xswitch.c: $Revision: 1.15 $"

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
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/PCI/bridge.h>
#include <sys/pci_intf.h>
#include <sys/sema.h>
#include <sys/ddi.h>

#include <sys/xtalk/xtalk_private.h>
#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xswitch.h>
#include <sys/xtalk/xwidget.h>

#define	NEW(ptr)	(ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

int                     xswitch_devflag = D_MP;

/*
 * This file provides generic support for Crosstalk
 * Switches, in a way that insulates crosstalk providers
 * from specifics about the switch chips being used.
 */

#if SN || IP30
#include <sys/xtalk/xbow.h>
#define	DEV_FUNC(dev,func)	xbow_##func
#endif

#if !defined(DEV_FUNC)
/*
 * There is more than one possible provider
 * for this platform. We need to examine the
 * master vertex of the current vertex for
 * a provider function structure, and indirect
 * through the appropriately named member.
 */
#define	DEV_FUNC(dev,func)	xwidget_to_provider_fns(dev)->func

static xswitch_provider_t *
xwidget_to_provider_fns(vertex_hdl_t xconn)
{
    vertex_hdl_t            busv;
    xswitch_info_t          xswitch_info;
    xswitch_provider_t      provider_fns;

    busv = hwgraph_connectpt_get(xconn_vhdl);
    ASSERT(busv != GRAPH_VERTEX_NONE);

    xswitch_info = xswitch_info_get(busv);
    ASSERT(xswitch_info != NULL);

    provider_fns = xswitch_info->xswitch_fns;
    ASSERT(provider_fns != NULL);

    return provider_fns;
}
#endif

#define	XSWITCH_CENSUS_BIT(port)		(1<<(port))
#define	XSWITCH_CENSUS_PORT_MIN			(0x0)
#define	XSWITCH_CENSUS_PORT_MAX			(0xF)
#define	XSWITCH_CENSUS_PORTS			(0x10)
#define	XSWITCH_WIDGET_PRESENT(infop,port)	((infop)->census & XSWITCH_CENSUS_BIT(port))

static char             xswitch_info_fingerprint[] = "xswitch_info";

struct xswitch_info_s {
    char                   *fingerprint;
    unsigned                census;
    vertex_hdl_t            vhdl[XSWITCH_CENSUS_PORTS];
    vertex_hdl_t            master_vhdl[XSWITCH_CENSUS_PORTS];
    xswitch_provider_t     *xswitch_fns;
};

xswitch_info_t
xswitch_info_get(vertex_hdl_t xwidget)
{
    xswitch_info_t          xswitch_info;

    xswitch_info = (xswitch_info_t)
	hwgraph_fastinfo_get(xwidget);
    if ((xswitch_info != NULL) &&
	(xswitch_info->fingerprint != xswitch_info_fingerprint))
	cmn_err(CE_PANIC, "%v xswitch_info_get bad fingerprint", xwidget);

    return (xswitch_info);
}

void
xswitch_info_vhdl_set(xswitch_info_t xswitch_info,
		      xwidgetnum_t port,
		      vertex_hdl_t xwidget)
{
#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return;
#endif
    if (port > XSWITCH_CENSUS_PORT_MAX)
	return;

    xswitch_info->vhdl[port - XSWITCH_CENSUS_PORT_MIN] = xwidget;
}

vertex_hdl_t
xswitch_info_vhdl_get(xswitch_info_t xswitch_info,
		      xwidgetnum_t port)
{
    if (xswitch_info == NULL)
	cmn_err(CE_PANIC, "xswitch_info_vhdl_get: null xswitch_info");

#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return GRAPH_VERTEX_NONE;
#endif
    if (port > XSWITCH_CENSUS_PORT_MAX)
	return GRAPH_VERTEX_NONE;

    return xswitch_info->vhdl[port - XSWITCH_CENSUS_PORT_MIN];
}

/*
 * Some systems may allow for multiple switch masters.  On such systems,
 * we assign a master for each port on the switch.  These interfaces
 * establish and retrieve that assignment.
 */
void
xswitch_info_master_assignment_set(xswitch_info_t xswitch_info,
				   xwidgetnum_t port,
				   vertex_hdl_t master_vhdl)
{
#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return;
#endif
    if (port > XSWITCH_CENSUS_PORT_MAX)
	return;

    xswitch_info->master_vhdl[port - XSWITCH_CENSUS_PORT_MIN] = master_vhdl;
}

vertex_hdl_t
xswitch_info_master_assignment_get(xswitch_info_t xswitch_info,
				   xwidgetnum_t port)
{
#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return GRAPH_VERTEX_NONE;
#endif
    if (port > XSWITCH_CENSUS_PORT_MAX)
	return GRAPH_VERTEX_NONE;

    return xswitch_info->master_vhdl[port - XSWITCH_CENSUS_PORT_MIN];
}

void
xswitch_info_set(vertex_hdl_t xwidget, xswitch_info_t xswitch_info)
{
    xswitch_info->fingerprint = xswitch_info_fingerprint;
    hwgraph_fastinfo_set(xwidget, (arbitrary_info_t) xswitch_info);
}

xswitch_info_t
xswitch_info_new(vertex_hdl_t xwidget)
{
    xswitch_info_t          xswitch_info;

    xswitch_info = xswitch_info_get(xwidget);
    if (xswitch_info == NULL) {
	int                     port;

	/*REFERENCED */
	graph_error_t           rc;

	NEW(xswitch_info);
	xswitch_info->census = 0;
	for (port = XSWITCH_CENSUS_PORT_MIN;
	     port <= XSWITCH_CENSUS_PORT_MAX;
	     port++) {
	    xswitch_info_vhdl_set(xswitch_info, port,
				  GRAPH_VERTEX_NONE);

	    xswitch_info_master_assignment_set(xswitch_info,
					       port,
					       GRAPH_VERTEX_NONE);
	}
	xswitch_info_set(xwidget, xswitch_info);
    }
    return xswitch_info;
}

void
xswitch_provider_register(vertex_hdl_t busv,
			  xswitch_provider_t * xswitch_fns)
{
    xswitch_info_t          xswitch_info = xswitch_info_get(busv);

    ASSERT(xswitch_info);
    xswitch_info->xswitch_fns = xswitch_fns;
}

void
xswitch_info_link_is_ok(xswitch_info_t xswitch_info, xwidgetnum_t port)
{
    xswitch_info->census |= XSWITCH_CENSUS_BIT(port);
}

int
xswitch_info_link_ok(xswitch_info_t xswitch_info, xwidgetnum_t port)
{
#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return 0;
#endif
    if (port > XSWITCH_CENSUS_PORT_MAX)
	return 0;

    return (xswitch_info->census & XSWITCH_CENSUS_BIT(port));
}

int
xswitch_reset_link(vertex_hdl_t xconn_vhdl)
{
    return DEV_FUNC(xconn_vhdl, reset_link)
	(xconn_vhdl);
}

/* Given a vertex handle to the xswitch get its logical
 * id.
 */
int
xswitch_id_get(vertex_hdl_t	xconn_vhdl)
{
    arbitrary_info_t 	xbow_num;
    graph_error_t	rv;

    rv = hwgraph_info_get_LBL(xconn_vhdl,INFO_LBL_XSWITCH_ID,&xbow_num);
    ASSERT(rv == GRAPH_SUCCESS);
    return(xbow_num);
}

/* Given a vertex handle to the xswitch set its logical
 * id.
 */
void
xswitch_id_set(vertex_hdl_t	xconn_vhdl,int xbow_num)
{
    graph_error_t	rv;

    rv = hwgraph_info_add_LBL(xconn_vhdl,INFO_LBL_XSWITCH_ID,
			      (arbitrary_info_t)xbow_num);
    ASSERT(rv == GRAPH_SUCCESS);
}
