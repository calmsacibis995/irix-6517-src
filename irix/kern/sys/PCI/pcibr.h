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

#ident	"sys/RACER/pcibr.h: $Revision: 1.38 $"

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
#include <sys/PCI/bridge.h>

/* =====================================================================
 *    symbolic constants used by pcibr's xtalk bus provider
 */

#define PCIBR_PIOMAP_BUSY		0x80000000

#define PCIBR_DMAMAP_BUSY		0x80000000
#define	PCIBR_DMAMAP_SSRAM		0x40000000

#define PCIBR_INTR_BLOCKED		0x40000000
#define PCIBR_INTR_BUSY			0x80000000

#if LANGUAGE_C

/* =====================================================================
 *    opaque types used by pcibr's xtalk bus provider
 */

typedef struct pcibr_piomap_s *pcibr_piomap_t;
typedef struct pcibr_dmamap_s *pcibr_dmamap_t;
typedef struct pcibr_intr_s *pcibr_intr_t;

/* =====================================================================
 *    primary entry points: Bridge (pcibr) device driver
 *
 *	These functions are normal device driver entry points
 *	and are called along with the similar entry points from
 *	other device drivers. They are included here as documentation
 *	of their existance and purpose.
 *
 *	pcibr_init() is called to inform us that there is a pcibr driver
 *	configured into the kernel; it is responsible for registering
 *	as a crosstalk widget and providing a routine to be called
 *	when a widget with the proper part number is observed.
 *
 *	pcibr_attach() is called for each vertex in the hardware graph
 *	corresponding to a crosstalk widget with the manufacturer
 *	code and part number registered by pcibr_init().
 */

extern void		pcibr_init(void);

extern int		pcibr_attach(vertex_hdl_t);

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

extern pciio_provider_t pcibr_provider;

/* =====================================================================
 *    secondary entry points: pcibr PCI bus provider
 *
 *	These functions are normally exported explicitly by
 *	a direct call from the pcibr initialization routine
 *	into the generic crosstalk provider; they are included
 *	here to enable a more aggressive performance hack in
 *	the generic crosstalk layer, where if we know that the
 *	only possible crosstalk provider is pcibr, and we can
 *	guarantee that all entry points are properly named, and
 *	we can deal with the implicit casting properly, then
 *	we can turn many of the generic provider routines into
 *	plain brances, or even eliminate them (given sufficient
 *	smarts on the part of the compilation system).
 */

extern pcibr_piomap_t	pcibr_piomap_alloc(vertex_hdl_t dev,
					   device_desc_t dev_desc,
					   pciio_space_t space,
					   iopaddr_t pci_addr,
					   size_t byte_count,
					   size_t byte_count_max,
					   unsigned flags);

extern void		pcibr_piomap_free(pcibr_piomap_t piomap);

extern caddr_t		pcibr_piomap_addr(pcibr_piomap_t piomap,
					  iopaddr_t xtalk_addr,
					  size_t byte_count);

extern void		pcibr_piomap_done(pcibr_piomap_t piomap);

extern caddr_t		pcibr_piotrans_addr(vertex_hdl_t dev,
					    device_desc_t dev_desc,
					    pciio_space_t space,
					    iopaddr_t pci_addr,
					    size_t byte_count,
					    unsigned flags);

extern iopaddr_t	pcibr_piospace_alloc(vertex_hdl_t dev,
					     device_desc_t dev_desc,
					     pciio_space_t space,
					     size_t byte_count,
					     size_t alignment);
extern void		pcibr_piospace_free(vertex_hdl_t dev,
					    pciio_space_t space,
					    iopaddr_t pciaddr,
					    size_t byte_count);

extern pcibr_dmamap_t	pcibr_dmamap_alloc(vertex_hdl_t dev,
					   device_desc_t dev_desc,
					   size_t byte_count_max,
					   unsigned flags);

extern void		pcibr_dmamap_free(pcibr_dmamap_t dmamap);

extern iopaddr_t	pcibr_dmamap_addr(pcibr_dmamap_t dmamap,
					  paddr_t paddr,
					  size_t byte_count);

extern alenlist_t	pcibr_dmamap_list(pcibr_dmamap_t dmamap,
					  alenlist_t palenlist,
					  unsigned flags);

extern void		pcibr_dmamap_done(pcibr_dmamap_t dmamap);

extern iopaddr_t	pcibr_dmatrans_addr(vertex_hdl_t dev,
					    device_desc_t dev_desc,
					    paddr_t paddr,
					    size_t byte_count,
					    unsigned flags);

extern alenlist_t	pcibr_dmatrans_list(vertex_hdl_t dev,
					    device_desc_t dev_desc,
					    alenlist_t palenlist,
					    unsigned flags);

extern void		pcibr_dmamap_drain(pcibr_dmamap_t map);

extern void		pcibr_dmaaddr_drain(vertex_hdl_t vhdl,
					    paddr_t addr,
					    size_t bytes);

extern void		pcibr_dmalist_drain(vertex_hdl_t vhdl,
					    alenlist_t list);

typedef unsigned	pcibr_intr_ibit_f(pciio_info_t info,
					  pciio_intr_line_t lines);

extern void		pcibr_intr_ibit_set(vertex_hdl_t, pcibr_intr_ibit_f *);

extern pcibr_intr_t	pcibr_intr_alloc(vertex_hdl_t dev,
					 device_desc_t dev_desc,
					 pciio_intr_line_t lines,
					 vertex_hdl_t owner_dev);

extern void		pcibr_intr_free(pcibr_intr_t intr);

extern int		pcibr_intr_connect(pcibr_intr_t intr,
					   intr_func_t intr_func,
					   intr_arg_t intr_arg,
					   void *thread);

extern void		pcibr_intr_disconnect(pcibr_intr_t intr);

extern vertex_hdl_t	pcibr_intr_cpu_get(pcibr_intr_t intr);

extern void		pcibr_provider_startup(vertex_hdl_t pcibr);

extern void		pcibr_provider_shutdown(vertex_hdl_t pcibr);

extern int		pcibr_reset(vertex_hdl_t dev);

extern int              pcibr_write_gather_flush(vertex_hdl_t dev);

extern pciio_endian_t	pcibr_endian_set(vertex_hdl_t dev,
					 pciio_endian_t device_end,
					 pciio_endian_t desired_end);

extern pciio_priority_t pcibr_priority_set(vertex_hdl_t dev,
					   pciio_priority_t device_prio);

extern uint64_t		pcibr_config_get(vertex_hdl_t conn,
					 unsigned reg,
					 unsigned size);

extern void		pcibr_config_set(vertex_hdl_t conn,
					 unsigned reg,
					 unsigned size,
					 uint64_t value);

extern int		pcibr_error_devenable(vertex_hdl_t pconn_vhdl,
					      int error_code);

extern pciio_slot_t	pcibr_error_extract(vertex_hdl_t pcibr_vhdl,
					    pciio_space_t *spacep,
					    iopaddr_t *addrp);

extern int		pcibr_rrb_alloc(vertex_hdl_t pconn_vhdl,
					int *count_vchan0,
					int *count_vchan1);

extern int		pcibr_rrb_check(vertex_hdl_t pconn_vhdl,
					int *count_vchan0,
					int *count_vchan1,
					int *count_reserved,
					int *count_pool);

extern int		pcibr_alloc_all_rrbs(vertex_hdl_t vhdl, int even_odd,
					     int dev_1_rrbs, int virt1,
					     int dev_2_rrbs, int virt2,
					     int dev_3_rrbs, int virt3,
					     int dev_4_rrbs, int virt4);

typedef void
rrb_alloc_funct_f	(vertex_hdl_t xconn_vhdl,
			 int *vendor_list);

typedef rrb_alloc_funct_f      *rrb_alloc_funct_t;

void			pcibr_set_rrb_callback(vertex_hdl_t xconn_vhdl,
					       rrb_alloc_funct_f *func);

/*
 * Bridge-specific flags that can be set via pcibr_device_flags_set
 * and cleared via pcibr_device_flags_clear.  Other flags are
 * more generic and are maniuplated through PCI-generic interfaces.
 *
 * Note that all PCI implementation-specific flags (Bridge flags, in
 * this case) are in the upper half-word.  The lower half-word of
 * flags are reserved for PCI-generic flags.
 *
 * Some of these flags have been "promoted" to the
 * generic layer, so they can be used without having
 * to "know" that the PCI bus is hosted by a Bridge.
 */
#define PCIBR_WRITE_GATHER	0x00010000	/* please use PCIIO version */
#define PCIBR_NOWRITE_GATHER	0x00020000	/* please use PCIIO version */
#define PCIBR_PREFETCH		0x00040000	/* please use PCIIO version */
#define PCIBR_NOPREFETCH	0x00080000	/* please use PCIIO version */
#define PCIBR_PRECISE		0x00100000
#define PCIBR_NOPRECISE		0x00200000
#define PCIBR_BARRIER		0x00400000
#define PCIBR_NOBARRIER		0x00800000
#define PCIBR_VCHAN0		0x01000000
#define PCIBR_VCHAN1		0x02000000
#define PCIBR_64BIT		0x04000000
#define PCIBR_NO64BIT		0x08000000

#define	PCIBR_EXTERNAL_ATES	0x40000000	/* uses external ATEs */
#define	PCIBR_ACTIVE		0x80000000	/* need a "done" */

/* Flags that have meaning to pcibr_device_flags_{set,clear} */
#define PCIBR_DEVICE_FLAGS (	\
	PCIBR_WRITE_GATHER	|\
	PCIBR_NOWRITE_GATHER	|\
	PCIBR_PREFETCH		|\
	PCIBR_NOPREFETCH	|\
	PCIBR_PRECISE		|\
	PCIBR_NOPRECISE		|\
	PCIBR_BARRIER		|\
	PCIBR_NOBARRIER		\
)

/* Flags that have meaning to *_dmamap_alloc, *_dmatrans_{addr,list} */
#define PCIBR_DMA_FLAGS (	\
	PCIBR_PREFETCH		|\
	PCIBR_NOPREFETCH	|\
	PCIBR_PRECISE		|\
	PCIBR_NOPRECISE		|\
	PCIBR_BARRIER		|\
	PCIBR_NOBARRIER		|\
	PCIBR_VCHAN0		|\
	PCIBR_VCHAN1		\
)

typedef int		pcibr_device_flags_t;

/*
 * Set bits in the Bridge Device(x) register for this device.
 * "flags" are defined above. NOTE: this includes turning
 * things *OFF* as well as turning them *ON* ...
 */
extern int		pcibr_device_flags_set(vertex_hdl_t dev,
					     pcibr_device_flags_t flags);

/*
 * Allocate Read Response Buffers for use by the specified device.
 * count_vchan0 is the total number of buffers desired for the
 * "normal" channel.  count_vchan1 is the total number of buffers
 * desired for the "virtual" channel.  Returns 0 on success, or
 * <0 on failure, which occurs when we're unable to allocate any
 * buffers to a channel that desires at least one buffer.
 */
extern int		pcibr_rrb_alloc(vertex_hdl_t pconn_vhdl,
					int *count_vchan0,
					int *count_vchan1);

/*
 * Get the starting PCIbus address out of the given DMA map.
 * This function is supposed to be used by a close friend of PCI bridge
 * since it relies on the fact that the starting address of the map is fixed at
 * the allocation time in the current implementation of PCI bridge.
 */
extern iopaddr_t	pcibr_dmamap_pciaddr_get(pcibr_dmamap_t);

extern xwidget_intr_preset_f pcibr_xintr_preset;

extern void		pcibr_hints_fix_rrbs(vertex_hdl_t);
extern void		pcibr_hints_dualslot(vertex_hdl_t, pciio_slot_t, pciio_slot_t);
extern void		pcibr_hints_subdevs(vertex_hdl_t, pciio_slot_t, ulong_t);
extern void		pcibr_hints_handsoff(vertex_hdl_t);

typedef unsigned	pcibr_intr_bits_f(pciio_info_t, pciio_intr_line_t);
extern void		pcibr_hints_intr_bits(vertex_hdl_t, pcibr_intr_bits_f *);

extern int		pcibr_asic_rev(vertex_hdl_t);

#endif				/* _LANGUAGE_C */
#endif				/* __PCI_PCIBR_H__ */
