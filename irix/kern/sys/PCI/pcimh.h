/**************************************************************************
 *									  *
 *		 Copyright (C) 1990-1993, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __PCI_PCIBR_H__
#define __PCI_PCIBR_H__

#ident	"sys/PCI/pcimh.h: $Revision: 1.9 $"

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

#include <sys/PCI/pciio.h>

#if LANGUAGE_C

typedef struct pcimh_intr_s    *pcimh_intr_t;



extern void		pcimh_attach(vertex_hdl_t);

/* =====================================================================
 *    bus provider function table
 *
 *	Normally, this table is only handed off explicitly
 *	during provider initialization, and the PCI generic
 *	layer will stash a pointer to it in the vertex; however,
 *	exporting it explicitly enables a performance hack in
 *	the generic PCI provider where if we know at compile
 *	time that the only possible PCI provider is a
 *	pcibr, we can go directly to this ops table.
 */

extern pciio_provider_t pcimh_provider;

/* =====================================================================
 *    secondary entry points: pcibr PCI bus provider
 *
 *	These functions are normally exported explicitly by
 *	a direct call from the pcibr initialization routine
 *	into the generic crosstalk provider; they are included
 *	here to enable a more aggressive performance hack in
 *	the generic crosstalk layer, where if we know that the
 *	only possible crosstalk provider is pcimh, and we can
 *	guarantee that all entry points are properly named, and
 *	we can deal with the implicit casting properly, then
 *	we can turn many of the generic provider routines into
 *	plain brances, or even eliminate them (given sufficient
 *	smarts on the part of the compilation system).
 */

extern pciio_piomap_alloc_f	pcimh_piomap_alloc;
extern pciio_piomap_free_f	pcimh_piomap_free;
extern pciio_piomap_addr_f	pcimh_piomap_addr;
extern pciio_piomap_done_f	pcimh_piomap_done;
extern pciio_piotrans_addr_f	pcimh_piotrans_addr;
extern pciio_piospace_alloc_f	pcimh_piospace_alloc;
extern pciio_piospace_free_f	pcimh_piospace_free;

extern pciio_dmamap_alloc_f	pcimh_dmamap_alloc;
extern pciio_dmamap_free_f	pcimh_dmamap_free;
extern pciio_dmamap_addr_f	pcimh_dmamap_addr;
extern pciio_dmamap_list_f	pcimh_dmamap_list;
extern pciio_dmamap_done_f	pcimh_dmamap_done;

extern pciio_dmatrans_addr_f	pcimh_dmatrans_addr;
extern pciio_dmatrans_list_f	pcimh_dmatrans_list;

extern pciio_dmamap_drain_f	pcimh_dmamap_drain;
extern pciio_dmaaddr_drain_f	pcimh_dmaaddr_drain;
extern pciio_dmalist_drain_f	pcimh_dmalist_drain;

extern pcimh_intr_t	pcimh_intr_alloc(vertex_hdl_t dev,
					 device_desc_t dev_desc,
					 pciio_intr_line_t lines,
					 vertex_hdl_t owner_dev);

extern void		pcimh_intr_free(pcimh_intr_t intr);

extern int		pcimh_intr_connect(pcimh_intr_t intr,
					   intr_func_t intr_func,
					   intr_arg_t intr_arg,
					   void *thread);

extern void		pcimh_intr_disconnect(pcimh_intr_t intr);

extern vertex_hdl_t	pcimh_intr_cpu_get(pcimh_intr_t intr);

extern pciio_config_get_f	pcimh_config_get;
extern pciio_config_set_f	pcimh_config_set;
extern pciio_error_devenable_f	pcimh_error_devenable;
extern pciio_error_extract_f	pcimh_error_extract;

extern pciio_provider_startup_f	pcimh_provider_startup;
extern pciio_reset_f		pcimh_reset;
extern pciio_write_gather_flush_f pcimh_write_gather_flush;
extern pciio_endian_set_f	pcimh_endian_set;
extern pciio_priority_set_f	pcimh_priority_set;
extern pciio_provider_shutdown_f	pcimh_provider_shutdown;

extern int		IP32_hinv_location(vertex_hdl_t);

#endif				/* _LANGUAGE_C */
#endif				/* __PCI_PCIBR_H__ */
