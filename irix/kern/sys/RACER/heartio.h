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

#ifndef	__RACER_HEARTIO_H__
#define	__RACER_HEARTIO_H__

#ident  "sys/RACER/heartio.h: $Revision: 1.14 $"

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

#if LANGUAGE_C

/* =====================================================================
 *	opaque types used by heart's xtalk bus provider
 */

typedef struct heart_piomap_s	       *heart_piomap_t;
typedef struct heart_dmamap_s	       *heart_dmamap_t;
typedef struct heart_intr_s	       *heart_intr_t;

/* =====================================================================
 *	primary entry points: heart device driver
 *
 *	These functions are normal device driver entry points
 *	and are called along with the similar entry points from
 *	other device drivers. They are included here as documentation
 *	of their existance and purpose. 
 *
 *	heart_init() is called to inform us that there is a heart driver
 *	configured into the kernel; it is responsible for registering
 *	as a crosstalk widget and providing a routine to be called
 *	when a widget with the proper part number is observed.
 *
 *	heart_startup() is called after most drivers have had
 *	their xxx_init routines called; it is given a
 *	vertex representing a heart. This routine is
 *	responsible for creating and traversing the
 *	hardware graph "downstream" of the heart,
 *	triggering attach routines for appropriate
 *	devices. This is distinct from heart_attach in
 *	that it represents a heart found from the SysAD
 *	side, not from the XTalk side.
 *
 *	heart_attach() is called for each vertex in the
 *	hardware graph corresponding to a crosstalk
 *	widget with the manufacturer code and part
 *	number registered by heartinit(). If a heart is
 *	used to host an XTalk bus, this routine will be
 *	called for that heart as well. If this becomes a
 *	concern, heart_startup and heart_attach should
 *	agree on a way to distinguish this "master" node
 *	from potential other hearts, which might be used
 *	(for instance) in diagnostic cards for xtalk
 *	exercise, or in expanded MP systems. Of course,
 *	in the MP case, things get trickier as we
 *	potentially have multiple masters, and we would
 *	need to take a careful look at the HUB provider
 *	for more ideas on how to handle this.
 */

extern void
heart_init(		void);

extern void
heart_startup(		vertex_hdl_t);

extern int
heart_attach(		vertex_hdl_t);

/* =====================================================================
 *	bus provider function table
 *
 *	Normally, this table is only handed off explicitly
 *	during provider initialization, and the crosstalk generic
 *	layer will stash a pointer to it in the vertex; however,
 *	exporting it explicitly enables a performance hack in
 *	the generic crosstalk provider where if we know at compile
 *	time that the only possible crosstalk provider is the
 *	heart, we can go directly to this ops table.
 */

extern xtalk_provider_t	heart_provider;

/* =====================================================================
 *	secondary entry points: heart xtalk bus provider
 *
 *	These functions are normally exported explicitly by
 *	a direct call from the heart initialization routine
 *	into the generic crosstalk provider; they are included
 *	here to enable a more aggressive performance hack in
 *	the generic crosstalk layer, where if we know that the
 *	only possible crosstalk provider is heart, and we can
 *	guarantee that all entry points are properly named, and
 *	we can deal with the implicit casting properly, then
 *	we can turn many of the generic provider routines into
 *	plain brances, or even eliminate them (given sufficient
 *	smarts on the part of the compilation system).
 */

extern heart_piomap_t
heart_piomap_alloc(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			iopaddr_t	xtalk_addr,
			size_t		byte_count,
			size_t		byte_count_max,
			unsigned	flags);

extern void
heart_piomap_free(	heart_piomap_t	piomap);

extern caddr_t
heart_piomap_addr(	heart_piomap_t	piomap,
			iopaddr_t	xtalk_addr,
			size_t		byte_count);

extern void
heart_piomap_done(	heart_piomap_t	piomap);

extern caddr_t
heart_piotrans_addr(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			iopaddr_t	xtalk_addr,
			size_t		byte_count,
			unsigned	flags);

extern heart_dmamap_t
heart_dmamap_alloc(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			size_t		byte_count_max,
			unsigned	flags);

extern void
heart_dmamap_free(	heart_dmamap_t	dmamap);

extern iopaddr_t
heart_dmamap_addr(	heart_dmamap_t	dmamap,
			paddr_t		paddr,
			size_t		byte_count);

extern alenlist_t
heart_dmamap_list(	heart_dmamap_t	dmamap,
			alenlist_t	palenlist,
			unsigned	flags);

extern void
heart_dmamap_done(	heart_dmamap_t	dmamap);

extern iopaddr_t
heart_dmatrans_addr(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			paddr_t		paddr,
			size_t		byte_count,
			unsigned	flags);

extern alenlist_t
heart_dmatrans_list(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			alenlist_t	palenlist,
			unsigned	flags);

extern void
heart_dmamap_drain(	heart_dmamap_t map);

extern void
heart_dmaaddr_drain(	vertex_hdl_t vhdl,
			paddr_t addr,
			size_t bytes);

extern void
heart_dmalist_drain(	vertex_hdl_t vhdl,
			alenlist_t list);

extern heart_intr_t
heart_intr_alloc(	vertex_hdl_t	dev,
			device_desc_t	dev_desc,
			vertex_hdl_t	owner_dev);

extern void
heart_intr_free(	heart_intr_t	intr);

extern int
heart_intr_connect(	heart_intr_t	intr,
			intr_func_t	intr_func,
			intr_arg_t	intr_arg,
		 xtalk_intr_setfunc_t	setfunc,
			void	       *setfunc_arg,
			void	       *thread);

extern void
heart_intr_disconnect(	heart_intr_t	intr);

extern vertex_hdl_t
heart_intr_cpu_get(	heart_intr_t	intr);

extern void
heart_provider_startup(	vertex_hdl_t	heart);

extern void
heart_provider_shutdown(vertex_hdl_t	heart);

extern int
heart_error_devenable(vertex_hdl_t, int, int);

extern xtalk_intr_prealloc_f	heart_intr_prealloc;
extern xtalk_intr_preconn_f	heart_intr_preconn;

#endif /* _LANGUAGE_C */
#endif	/* __RACER_HEARTIO_H__ */
