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

#ident "io/gioio.c $Revision: 1.22 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/driver.h>
#include <ksys/hwg.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/ioerror.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/cdl.h>

#include <ksys/sthread.h>
#include <sys/schedctl.h>

#include <sys/GIO/gioio.h>
#include <sys/GIO/gioio_private.h>

#define	NEW(ptr)	(ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

char                    gioio_info_fingerprint[] = "gioio_info";

cdl_p                   gioio_registry = NULL;

/* =====================================================================
 *            GIO Generic Bus Provider
 * Implement GIO provider operations.  The gioio* layer provides a
 * platform-independent interface for GIO devices.  This layer
 * switches among the possible implementations of a GIO adapter.
 */

/* =====================================================================
 *            Provider Function Location SHORTCUT
 *
 * On platforms with only one possible GIO provider, macros can be
 * set up at the top that cause the table lookups and indirections to
 * completely disappear.
 */

#if IP30
/*
 *    We will assume that IP30 (SpeedRacer)
 *      only use Bridge ASICs to provide GIO support.
 */
#include <sys/GIO/giobr.h>
#define	DEV_FUNC(dev,func)	giobr_##func
#define	CAST_PIOMAP(x)		((giobr_piomap_t)(x))
#define	CAST_DMAMAP(x)		((giobr_dmamap_t)(x))
#define	CAST_INTR(x)		((giobr_intr_t)(x))
#endif

/* =====================================================================
 *              Provider Function Location
 *
 *      If there is more than one possible provider for
 *      this platform, we need to examine the master
 *      vertex of the current vertex for a provider
 *      function structure, and indirect through the
 *      appropriately named member.
 */

#if !defined(DEV_FUNC)

static gioio_provider_t *
gioio_to_provider_fns(vertex_hdl_t gconn)
{
    gioio_info_t            card_info;
    gioio_provider_t       *provider_fns;

    card_info = gioio_info_get(gconn);
    ASSERT(card_info != NULL);

    provider_fns = (gioio_provider_t *) gioio_info_pops_get(card_info);
    ASSERT(provider_fns != NULL);

    return (provider_fns);
}

#define	DEV_FUNC(dev,func)	gioio_to_provider_fns(dev)->func
#define	CAST_PIOMAP(x)		((gioio_piomap_t)(x))
#define	CAST_DMAMAP(x)		((gioio_dmamap_t)(x))
#define	CAST_INTR(x)		((gioio_intr_t)(x))

#endif

/*
 * Many functions are not passed their vertex
 * information directly; rather, they must
 * dive through a resource map. These macros
 * are available to coordinate this detail.
 */
#define	PIOMAP_FUNC(map,func)	DEV_FUNC(map->pp_dev,func)
#define	DMAMAP_FUNC(map,func)	DEV_FUNC(map->pd_dev,func)
#define	INTR_FUNC(intr,func)	DEV_FUNC(intr_hdl->pi_dev,func)

/* =====================================================================
 *                    PIO MANAGEMENT
 *
 *      For mapping system virtual address space to
 *      gioio space on a specified card
 */

gioio_piomap_t
gioio_piomap_alloc(vertex_hdl_t dev,	/* set up mapping for this device */
		   device_desc_t dev_desc,	/* device descriptor */
		   gioio_space_t space,		/* GIO only has memory space */
		   iopaddr_t addr,	/* lowest address (or offset in window) */
		   size_t byte_count,	/* size of region containing our mappings */
		   size_t byte_count_max,	/* maximum size of a mapping */
		   unsigned flags)
{				/* defined in sys/pio.h */
    return (gioio_piomap_t) DEV_FUNC(dev, piomap_alloc)
	(dev, dev_desc, space, addr, byte_count, byte_count_max, flags);
}


void
gioio_piomap_free(gioio_piomap_t gioio_piomap)
{
    PIOMAP_FUNC(gioio_piomap, piomap_free)
	(CAST_PIOMAP(gioio_piomap));
}


caddr_t
gioio_piomap_addr(gioio_piomap_t gioio_piomap,	/* mapping resources */
		  iopaddr_t gioio_addr,		/* map for this gioio address */
		  size_t byte_count)
{				/* map this many bytes */
    gioio_piomap->pp_kvaddr = PIOMAP_FUNC(gioio_piomap, piomap_addr)
	(CAST_PIOMAP(gioio_piomap), gioio_addr, byte_count);
    return gioio_piomap->pp_kvaddr;
}


void
gioio_piomap_done(gioio_piomap_t gioio_piomap)
{
    PIOMAP_FUNC(gioio_piomap, piomap_done)
	(CAST_PIOMAP(gioio_piomap));
}


caddr_t
gioio_piotrans_addr(vertex_hdl_t dev,	/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    gioio_space_t space,	/* GIO only has mem space */
		    iopaddr_t addr,	/* starting address (or offset in window) */
		    size_t byte_count,	/* map this many bytes */
		    unsigned flags)
{				/* (currently unused) */
    return DEV_FUNC(dev, piotrans_addr)
	(dev, dev_desc, space, addr, byte_count, flags);
}

/* =====================================================================
 *                    DMA MANAGEMENT
 *
 *      For mapping from gio space to system
 *      physical space.
 */

gioio_dmamap_t
gioio_dmamap_alloc(vertex_hdl_t dev,	/* set up mappings for this device */
		   device_desc_t dev_desc,	/* device descriptor */
		   size_t byte_count_max,	/* max size of a mapping */
		   unsigned flags)
{				/* defined in dma.h */
    return (gioio_dmamap_t) DEV_FUNC(dev, dmamap_alloc)
	(dev, dev_desc, byte_count_max, flags);
}


void
gioio_dmamap_free(gioio_dmamap_t gioio_dmamap)
{
    DMAMAP_FUNC(gioio_dmamap, dmamap_free)
	(CAST_DMAMAP(gioio_dmamap));
}


iopaddr_t
gioio_dmamap_addr(gioio_dmamap_t gioio_dmamap,	/* use these mapping resources */
		  paddr_t paddr,	/* map for this address */
		  size_t byte_count)
{				/* map this many bytes */
    return DMAMAP_FUNC(gioio_dmamap, dmamap_addr)
	(CAST_DMAMAP(gioio_dmamap), paddr, byte_count);
}


alenlist_t
gioio_dmamap_list(gioio_dmamap_t gioio_dmamap,	/* use these mapping resources */
		  alenlist_t alenlist)
{				/* map this Address/Length List */
    return DMAMAP_FUNC(gioio_dmamap, dmamap_list)
	(CAST_DMAMAP(gioio_dmamap), alenlist);
}


void
gioio_dmamap_done(gioio_dmamap_t gioio_dmamap)
{
    DMAMAP_FUNC(gioio_dmamap, dmamap_done)
	(CAST_DMAMAP(gioio_dmamap));
}


iopaddr_t
gioio_dmatrans_addr(vertex_hdl_t dev,	/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    paddr_t paddr,	/* system physical address */
		    size_t byte_count,	/* length */
		    unsigned flags)
{				/* defined in dma.h */
    return DEV_FUNC(dev, dmatrans_addr)
	(dev, dev_desc, paddr, byte_count, flags);
}


alenlist_t
gioio_dmatrans_list(vertex_hdl_t dev,	/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    alenlist_t palenlist,	/* system address/length list */
		    unsigned flags)
{				/* defined in dma.h */
    return DEV_FUNC(dev, dmatrans_list)
	(dev, dev_desc, palenlist, flags);
}

/* =====================================================================
 *                    INTERRUPT MANAGEMENT
 *
 *      Allow crosstalk devices to establish interrupts
 */

/*
 * Allocate resources required for an interrupt as specified in intr_desc.
 * Return resource handle in intr_hdl.
 */
gioio_intr_t
gioio_intr_alloc(vertex_hdl_t dev,	/* which Crosstalk device */
		 device_desc_t dev_desc,	/* device descriptor */
		 gioio_intr_line_t lines,	/* INTR line(s) to attach */
		 vertex_hdl_t owner_dev)
{				/* owner of this interrupt */
    return (gioio_intr_t) DEV_FUNC(dev, intr_alloc)
	(dev, dev_desc, lines, owner_dev);
}


/*
 * Free resources consumed by intr_alloc.
 */
void
gioio_intr_free(gioio_intr_t intr_hdl)
{
    INTR_FUNC(intr_hdl, intr_free)
	(CAST_INTR(intr_hdl));
}


/*
 * Associate resources allocated with a previous gioio_intr_alloc call with the
 * described handler, arg, name, etc.
 *
 * Returns 0 on success, returns <0 on failure.
 */
int
gioio_intr_connect(gioio_intr_t intr_hdl,	/* gioio intr resource handle */
		   intr_func_t intr_func,	/* gioio intr handler */
		   intr_arg_t intr_arg,		/* arg to intr handler */
		   void *thread)
{				/* interrupt thread */
    return INTR_FUNC(intr_hdl, intr_connect)
	(CAST_INTR(intr_hdl), intr_func, intr_arg, thread);
}


/*
 * Disassociate handler with the specified interrupt.
 */
void
gioio_intr_disconnect(gioio_intr_t intr_hdl)
{
    INTR_FUNC(intr_hdl, intr_disconnect)
	(CAST_INTR(intr_hdl));
}


/*
 * Return a hwgraph vertex that represents the CPU currently
 * targeted by an interrupt.
 */
vertex_hdl_t
gioio_intr_cpu_get(gioio_intr_t intr_hdl)
{
    return INTR_FUNC(intr_hdl, intr_cpu_get)
	(CAST_INTR(intr_hdl));
}


/* =====================================================================
 *                    ERROR MANAGEMENT
 */

/*
 * gioio_cardinfo_get
 *
 * get the gioio_info structure for the specified slot
 * the GIO device which come built-in to the GIO-XTALK
 * card at logically device(0), but will use multiple
 * dma channels (REQ/GNT pair).
 */
gioio_info_t
gioio_cardinfo_get(
		      vertex_hdl_t gioio_vhdl,
		      gioio_slot_t gio_slot,
		      vertex_hdl_t *gconn)
{
    char                    namebuf[16];
    gioio_info_t            gioio_info;
    graph_error_t           rc;

    /*
     * Scan through the vertices attached to this GIO bus, which
     * gives the list of devices attached to this bus, since
     * for GIO we only have 1 device/slot (i.e. 0) it better
     * be the device(0)
     */
    sprintf(namebuf, "%d", gio_slot);
    rc = hwgraph_traverse(gioio_vhdl, namebuf, gconn);
    if (rc != GRAPH_SUCCESS)
	return (gioio_info_t) 0;

    /*
     * Get the gioio_info structure corresponding to the
     * device that caused the error
     */
    gioio_info = gioio_info_get(*gconn);
    ASSERT(gioio_info);
    ASSERT(gioio_info_slot_get(gioio_info) == gio_slot);
    return gioio_info;
}

/*ARGSUSED */
int
gioio_error_handler(
		       vertex_hdl_t gioio_vhdl,
		       int error_code,
		       ioerror_mode_t mode,
		       ioerror_t *ioerror)
{
    gioio_info_t            gioio_info;
    vertex_hdl_t            pconn;
    gioio_slot_t            slot;

#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: gioio_error_handler\n", gioio_vhdl);
#endif

    slot = IOERROR_GETVALUE(ioerror, widgetdev);
    gioio_info = gioio_cardinfo_get(gioio_vhdl, slot, &pconn);

    if (!gioio_info) {
	/*
	 * If we get a bus error, and there is no attached info,
	 * and the error is user process generated, attribute
	 * it to operator error, and return.
	 * We should just kill the process that's runing right
	 * now if the error was due to read. If due to write,
	 * can't do anything.
	 */
	return IOERROR_WIDGETLEVEL;
    }
    IOERR_PRINTF(cmn_err(CE_NOTE,
		    "%v: GIO Bus Error: Error code: %d Error mode: %d\n",
			 gioio_vhdl, error_code, mode));

    IOERR_PRINTF(cmn_err(CE_NOTE,
	    "!Error GIO Device ID: 0x%x Product ID: 0x%x Bus Slot: %d\n",
			 gioio_info_device_id_get(gioio_info),
			 gioio_info_product_id_get(gioio_info),
			 gioio_info_slot_get(gioio_info)));

    /*
     * if this slot has registered a handler,
     * turn control over to it.
     */
    if (gioio_info->c_efunc)
	return gioio_info->c_efunc
	    (gioio_info->c_einfo, error_code, mode, ioerror);

    ioerror_dump("giobr", error_code, mode, ioerror);

    cmn_err(CE_PANIC,
	    "%v: GIO Bus Access error\n"
	    "Product ID 0x%x Slot 0x%x Addr 0x%x",
	    pconn,
	    gioio_info_product_id_get(gioio_info),
	    gioio_info_slot_get(gioio_info),
	    IOERROR_GETVALUE(ioerror, busaddr));
    /*NOTREACHED */
}

/*ARGSUSED */
int
gioio_error_devenable(vertex_hdl_t gconn_vhdl, int error_code)
{
    int                     rc;

    rc = DEV_FUNC(gconn_vhdl, error_devenable)
	(gconn_vhdl, error_code);

    if (rc == IOERROR_HANDLED) {
	/* Do any cleanup specific to this layer..
	 * Nothing for now..
	 */
    }
    return rc;
}

/* =====================================================================
 *                    CONFIGURATION MANAGEMENT
 */

/*
 * Startup a crosstalk provider
 */
void
gioio_provider_startup(vertex_hdl_t gioio_provider)
{
    DEV_FUNC(gioio_provider, provider_startup)
	(gioio_provider);
}


/*
 * Shutdown a crosstalk provider
 */
void
gioio_provider_shutdown(vertex_hdl_t gioio_provider)
{
    DEV_FUNC(gioio_provider, provider_shutdown)
	(gioio_provider);
}

/*
 * Specify PCI arbitration priority.
 */
gioio_priority_t
gioio_priority_set(vertex_hdl_t dev,
		   gioio_priority_t device_prio)
{
    return DEV_FUNC(dev, priority_set)
	(dev, device_prio);
}

/* =====================================================================
 *                    GENERIC GIO SUPPORT FUNCTIONS
 */

vertex_hdl_t
gioio_intr_dev_get(gioio_intr_t gioio_intr)
{
    return (gioio_intr->pi_dev);
}

/****** Generic crosstalk pio interfaces ******/
vertex_hdl_t
gioio_pio_dev_get(gioio_piomap_t gioio_piomap)
{
    return (gioio_piomap->pp_dev);
}

gioio_slot_t
gioio_pio_slot_get(gioio_piomap_t gioio_piomap)
{
    return (gioio_piomap->pp_slot);
}

gioio_space_t
gioio_pio_space_get(gioio_piomap_t gioio_piomap)
{
    return (gioio_piomap->pp_space);
}

iopaddr_t
gioio_pio_gioaddr_get(gioio_piomap_t gioio_piomap)
{
    return (gioio_piomap->pp_gioaddr);
}

ulong
gioio_pio_mapsz_get(gioio_piomap_t gioio_piomap)
{
    return (gioio_piomap->pp_mapsz);
}

caddr_t
gioio_pio_kvaddr_get(gioio_piomap_t gioio_piomap)
{
    return (gioio_piomap->pp_kvaddr);
}


/****** Generic crosstalk dma interfaces ******/
vertex_hdl_t
gioio_dma_dev_get(gioio_dmamap_t gioio_dmamap)
{
    return (gioio_dmamap->pd_dev);
}

gioio_slot_t
gioio_dma_slot_get(gioio_dmamap_t gioio_dmamap)
{
    return (gioio_dmamap->pd_slot);
}


/****** Generic gio slot information interfaces ******/
gioio_info_t
gioio_info_get(vertex_hdl_t gioio)
{
    gioio_info_t            gioio_info;

    gioio_info = (gioio_info_t) hwgraph_fastinfo_get(gioio);

    if ((gioio_info != NULL) &&
	(gioio_info->c_fingerprint != gioio_info_fingerprint))
	cmn_err(CE_PANIC, "%v: gioio_info bad fingerprint", gioio);

    return gioio_info;
}

void
gioio_info_set(vertex_hdl_t gioio, gioio_info_t gioio_info)
{
    if (gioio_info != NULL)
	gioio_info->c_fingerprint = gioio_info_fingerprint;
    hwgraph_fastinfo_set(gioio, (arbitrary_info_t) gioio_info);
}

vertex_hdl_t
gioio_info_dev_get(gioio_info_t gioio_info)
{
    return (gioio_info->c_vertex);
}

gioio_slot_t
gioio_info_slot_get(gioio_info_t gioio_info)
{
    return (gioio_info->c_slot);
}

gioio_product_id_t
gioio_info_product_id_get(gioio_info_t gioio_info)
{
    return (gioio_info->c_pr_id);
}

gioio_product_id_t
gioio_info_device_id_get(gioio_info_t gioio_info)
{
    return (gioio_info->c_device);
}

vertex_hdl_t
gioio_info_master_get(gioio_info_t gioio_info)
{
    return (gioio_info->c_master);
}

arbitrary_info_t
gioio_info_mfast_get(gioio_info_t gioio_info)
{
    return (gioio_info->c_mfast);
}

gioio_provider_t       *
gioio_info_pops_get(gioio_info_t gioio_info)
{
    return (gioio_info->c_pops);
}

/* =====================================================================
 *                    GENERIC GIO INITIALIZATION FUNCTIONS
 */

/*
 *    gioioinit: called once during device driver
 *      initializtion if this driver is configured into
 *      the system.
 */
void
gioio_init(void)
{
    cdl_p                   cp;

#if DEBUG && ATTACH_DEBUG
    printf("gioio_init\n");
#endif

    /* Allocate the registry.
     * We might already have one.
     * If we don't, go get one.
     * MPness: someone might have
     * set one up for us while we
     * were not looking; use an atomic
     * compare-and-swap to commit to
     * using the new registry if and
     * only if nobody else did first.
     * If someone did get there first,
     * toss the one we allocated back
     * into the pool.
     */
    if (gioio_registry == NULL) {
	cp = cdl_new(EDGE_LBL_GIO, "product", "device");
	if (!compare_and_swap_ptr((void **) &gioio_registry, NULL, (void *) cp)) {
	    cdl_del(cp);
	}
    }
    ASSERT(gioio_registry != NULL);
}

/*
 *    gioioattach: called for each vertex in the graph
 *      that is a GIO provider.
 */
/*ARGSUSED */
int
gioio_attach(vertex_hdl_t gioio)
{
#if DEBUG && ATTACH_DEBUG
    cmn_err(CE_CONT, "%v: gioio_attach\n", gioio);
#endif
    return 0;
}

/*
 * Associate a set of gioio_provider functions with a vertex.
 */
void
gioio_provider_register(vertex_hdl_t provider, gioio_provider_t *gioio_fns)
{
    hwgraph_info_add_LBL(provider, INFO_LBL_GFUNCS,
			 (arbitrary_info_t) gioio_fns);
}

/*
 * Disassociate a set of gioio_provider functions with a vertex.
 */
void
gioio_provider_unregister(vertex_hdl_t provider)
{
    hwgraph_info_remove_LBL(provider, INFO_LBL_GFUNCS,
			    (arbitrary_info_t *) 0);
}

/*
 * Obtain a pointer to the gioio_provider functions for a specified Crosstalk
 * provider.
 */
gioio_provider_t       *
gioio_provider_fns_get(vertex_hdl_t provider)
{
    graph_error_t           rc;
    arbitrary_info_t        ainfo;

    rc = hwgraph_info_get_LBL(provider, INFO_LBL_GFUNCS, &ainfo);
    if (rc != GRAPH_SUCCESS)
	ainfo = 0;
    return (gioio_provider_t *) ainfo;
}

/*ARGSUSED4 */
int
gioio_driver_register(
			 gioio_product_id_t product_id,
			 gioio_device_id_t device_id,
			 char *driver_prefix,
			 unsigned flags)
{
    /* it's possible that someone might call
     * gioio_driver_register before the system
     * manages to call gioio_init ... so we
     * trigger the gioio_init ourselves.
     */
    if (gioio_registry == NULL)
	gioio_init();

    return cdl_add_driver(gioio_registry,
			  product_id, device_id,
			  driver_prefix, flags);
}

/*
 * Remove an initialization function.
 */
/*ARGSUSED */
void
gioio_driver_unregister(
			   char *driver_prefix)
{
    /* if a driver is calling unregister,
     * it must have called register before,
     * so we must already have a registry.
     */
    ASSERT(gioio_registry != NULL);

    cdl_del_driver(gioio_registry, driver_prefix);
}

/*
 * Call some function with each vertex that
 * might be one of this driver's attach points.
 */
void
gioio_iterate(char *driver_prefix,
	      gioio_iter_f *func)
{
    ASSERT(gioio_registry != NULL);

    cdl_iterate(gioio_registry, driver_prefix, (cdl_iter_f *)func);
}

vertex_hdl_t
gioio_device_register(
			 vertex_hdl_t giobus,	/* gio bus vertex (will add "/%d" to this) */
			 vertex_hdl_t master,	/* vertex for specific provider */
			 gioio_slot_t slot,	/* card's slot */
			 gioio_product_id_t product,
			 gioio_device_id_t device)
{
    /* REFERENCED */
    graph_error_t           rc;
    char                    name[160];
    vertex_hdl_t            gconn;

    /* REFERENCED */
    cnodeid_t               cnodeid = master_node_get(master);
    gioio_info_t            gioio_info;

    sprintf(name, "%d", slot);
    rc = hwgraph_path_add(giobus, name, &gconn);
    ASSERT(rc == GRAPH_SUCCESS);

    /*
     * set up gioio_info
     */
    NEW(gioio_info);
    ASSERT(gioio_info != NULL);

    gioio_info->c_vertex = gconn;
    gioio_info->c_slot = slot;
    gioio_info->c_pr_id = product;
    gioio_info->c_device = device;
    gioio_info->c_master = master;
    gioio_info->c_mfast = hwgraph_fastinfo_get(master);
    gioio_info->c_pops = gioio_provider_fns_get(master);
    gioio_info->c_efunc = 0;
    gioio_info->c_einfo = 0;

    gioio_info_set(gconn, gioio_info);

    /*
     * create link to our gio provider
     */

    device_master_set(gconn, master);

    return gconn;
}

/*ARGSUSED */
int
gioio_device_attach(vertex_hdl_t gconn)
{
    gioio_info_t            gioio_info;
    gioio_product_id_t      product_id;
    gioio_device_id_t       device_id;

    gioio_info = gioio_info_get(gconn);
    product_id = gioio_info->c_pr_id;
    device_id = gioio_info->c_device;

    /* we don't start attaching things until
     * all the driver init routines (including
     * pciio_init) have been called; so we
     * can assume here that we have a registry.
     */
    ASSERT(gioio_registry != NULL);

    return cdl_add_connpt(gioio_registry, product_id, device_id, gconn);
}

/*
 * pciio_error_register:
 * arrange for a function to be called with
 * a specified first parameter plus other
 * information when an error is encountered
 * and traced to the pci slot corresponding
 * to the connection point pconn.
 *
 * may also be called with a null function
 * pointer to "unregister" the error handler.
 *
 * NOTE: subsequent calls silently overwrite
 * previous data for this vertex. We assume that
 * cooperating drivers, well, cooperate ...
 */
void
gioio_error_register(vertex_hdl_t gconn,
		     error_handler_f *efunc,
		     error_handler_arg_t einfo)
{
    gioio_info_t            gioio_info;

    gioio_info = gioio_info_get(gconn);
    ASSERT(gioio_info != NULL);
    gioio_info->c_efunc = efunc;
    gioio_info->c_einfo = einfo;
}

/*
 * Issue a hardware reset to a card.
 */
/*ARGSUSED */
void
gioio_reset(vertex_hdl_t gioio)
{
    /* TBD -- need more info on how to do this in general.
       may require a new gioio_provider entry point. */
}


