/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990-1993, Silicon Graphics, Inc           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef	__GIO_GIOBR_H__
#define	__GIO_GIOBR_H__

#ident  "sys/GIO/giobr.h: $Revision: 1.10 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/pda.h>

#include <sys/GIO/gioio.h>

/* =====================================================================
 *	symbolic constants used by giobr's xtalk bus provider
 */

#define	GIOBR_PIOMAP_BUSY		0x80000000	

#define	GIOBR_DMAMAP_BUSY		0x80000000	

#define GIOBR_INTR_BLOCKED		0x40000000	
#define	GIOBR_INTR_BUSY			0x80000000	

#if LANGUAGE_C


/* =====================================================================
 *	structure used by devices to configure bridge chip
 */

/*  Validity bits for mask */
#define GIOBR_ARB_FLAGS (1 << 0)
#define GIOBR_DEV_ATTRS (1 << 1)
#define GIOBR_RRB_MAP   (1 << 2)
#define GIOBR_INTR_DEV  (1 << 3)

typedef struct giobr_bridge_config_s {
    __uint64_t          mask;             /* Validity of following fields */
    bridgereg_t        	arb_flags;        /* Arbitratrion flags */
    bridgereg_t        	device_attrs[8];  /* CMD/DATA attributes*/
    char                rrb[8];           /* RRB assignments */
    char                intr_devices[8];  /* Interrupt channels */
} *giobr_bridge_config_t;

/* =====================================================================*/


/* =====================================================================
 *	opaque types used by giobr's xtalk bus provider
 */


typedef struct giobr_piomap_s	       *giobr_piomap_t;
typedef struct giobr_dmamap_s	       *giobr_dmamap_t;
typedef struct giobr_intr_s	       *giobr_intr_t;

/* =====================================================================
 *	primary entry points: Bridge (giobr) device driver
 *
 *	These functions are normal device driver entry points
 *	and are called along with the similar entry points from
 *	other device drivers. They are included here as documentation
 *	of their existance and purpose. 
 *
 *	giobrinit() is called to inform us that there is a giobr driver
 *	configured into the kernel; it is responsible for registering
 *	as a crosstalk widget and providing a routine to be called
 *	when a widget with the proper part number is observed.
 *
 *	giobredtinit() is called to initiate a scan of a
 *	specified bridge in a system when that bridge is
 *	not going to be otherwise discovered; the EDT structure
 *	contains the physical address of the Bridge registers.
 *
 *	giobrattach() is called for each vertex in the hardware graph
 *	corresponding to a crosstalk widget with the manufacturer
 *	code and part number registered by giobrinit().
 */

extern void
giobrinit(		void);

extern void
giobredtinit(		struct edt     *e);

extern void
giobrattach(		vertex_hdl_t);

/* =====================================================================
 *	bus provider function table
 *
 *	Normally, this table is only handed off explicitly
 *	during provider initialization, and the GIO generic
 *	layer will stash a pointer to it in the vertex; however,
 *	exporting it explicitly enables a performance hack in
 *	the generic GIO provider where if we know at compile
 *	time that the only possible GIO provider is a
 *	giobr, we can go directly to this ops table.
 */

extern gioio_provider_t	giobr_provider;

/* =====================================================================
 *	secondary entry points: giobr GIO bus provider
 *
 *	These functions are normally exported explicitly by
 *	a direct call from the giobr initialization routine
 *	into the generic crosstalk provider; they are included
 *	here to enable a more aggressive performance hack in
 *	the generic crosstalk layer, where if we know that the
 *	only possible crosstalk provider is giobr, and we can
 *	guarantee that all entry points are properly named, and
 *	we can deal with the implicit casting properly, then
 *	we can turn many of the generic provider routines into
 *	plain brances, or even eliminate them (given sufficient
 *	smarts on the part of the compilation system).
 */

extern giobr_piomap_t
giobr_piomap_alloc(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			gioio_space_t	space,
			iopaddr_t	gio_addr,
			size_t		byte_count,
			size_t		byte_count_max,
			unsigned	flags);

extern void
giobr_piomap_free(	giobr_piomap_t	piomap);

extern caddr_t
giobr_piomap_addr(	giobr_piomap_t	piomap,
			iopaddr_t	xtalk_addr,
			size_t		byte_count);

extern void
giobr_piomap_done(	giobr_piomap_t	piomap);

extern caddr_t
giobr_piotrans_addr(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			gioio_space_t	space,
			iopaddr_t	gio_addr,
			size_t		byte_count,
			unsigned	flags);

extern giobr_dmamap_t
giobr_dmamap_alloc(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			size_t		byte_count_max,
			unsigned	flags);

extern void
giobr_dmamap_free(	giobr_dmamap_t	dmamap);

extern iopaddr_t
giobr_dmamap_addr(	giobr_dmamap_t	dmamap,
			paddr_t		paddr,
			size_t		byte_count);

extern alenlist_t
giobr_dmamap_list(	giobr_dmamap_t	dmamap,
			alenlist_t	palenlist);

extern void
giobr_dmamap_done(	giobr_dmamap_t	dmamap);

extern iopaddr_t
giobr_dmatrans_addr(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			paddr_t		paddr,
			size_t		byte_count,
			unsigned	flags);

extern alenlist_t
giobr_dmatrans_list(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			alenlist_t	palenlist,
			unsigned	flags);

extern giobr_intr_t
giobr_intr_alloc(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			gioio_intr_line_t lines,
			vertex_hdl_t	owner_dev);

extern void
giobr_intr_free(	giobr_intr_t	intr);

extern int
giobr_intr_connect(	giobr_intr_t	intr,
			intr_func_t	intr_func,
			intr_arg_t	intr_arg,
			void	       *thread);

extern void
giobr_intr_disconnect(	giobr_intr_t	intr);

extern vertex_hdl_t
giobr_intr_cpu_get(	giobr_intr_t	intr);

extern void
giobr_provider_startup(	vertex_hdl_t	giobr);

extern void
giobr_provider_shutdown(vertex_hdl_t	giobr);

extern gioio_priority_t
giobr_priority_set(	vertex_hdl_t giocard,
                        gioio_priority_t dmachan_pri);

extern int
giobr_error_devenable(	vertex_hdl_t gconn_vhdl, int error_code);

extern void
giobr_bridge_config(	vertex_hdl_t	conn,
			giobr_bridge_config_t  bridge_config);

extern void
giobr_bus_reset(	vertex_hdl_t 	conn);

extern xwidget_intr_preset_f	giobr_xintr_preset;

#endif /* _LANGUAGE_C */
#endif	/* __GIO_GIOBR_H__ */
