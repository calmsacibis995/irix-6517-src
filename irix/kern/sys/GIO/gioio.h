/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __GIO_GIOIO_H__
#define __GIO_GIOIO_H__

#ident "sys/GIO/gioio.h $Revision: 1.19 $"


/*
 * gioio.h -- Generic GIO interface for
 */

#if LANGUAGE_C

#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/ioerror.h>
#include <sys/alenlist.h>

typedef __uint32_t gioio_product_id_t;

typedef __uint32_t gioio_device_id_t;

typedef char gioio_slot_t;	/* GIO slot number (0..2) */

#define GIOIO_NONE		-1

typedef char gioio_intr_line_t;	/* GIO interrupt line(s) */

#define	GIOIO_INTR_LINE(n)	(n)

#define	GIOIO_INTR_LVL(n)	(n)

/*
 *    A NOTE ON GIO PIO ADDRESSES AND GIO SLOTS
 *
 *      For details of GIO PIO mapping refer to comments
 *      in the gio implementation header files (i.e. giobr.h)
 *
 *      For GIO64 device slots 10MB of GIO address space
 *      have been defined:
 *      slot#   slot type   size  address range
 *      -----   ---------   ----  -------------------------
 *      - 0     GFX         4MB   0x1f00_0000 - 0x1f3f_ffff
 *      - 1     EXP0        2MB   0x1f40_0000 - 0x1f5f_ffff
 *      - 2     EXP1        4MB   0x1f60_0000 - 0x1f9f_ffff
 *
 *      There are un-slotted devices, HPC, I/O and misc devices,
 *      which are grouped into the HPC address space
 *      - -     MISC        1MB   0x1fb0_0000 - 0x1fbf_ffff
 *      
 *      Following space is reserved and unused
 *      - -     RESERVED  112MB   0x1800_0000 - 0x1eff_ffff
 *
 *      Confusion can rise from the terms used for the slot# and
 *      the slot type. The GIO Spec and h/w docs tend to use slot#'s
 *      while the MC Spec and s/w docs tend to use slot types.
 *      MC-s/w convention is followed but as long and terms are used
 *      carefully confusion can be minimized.
 *      slot0 - the "graphics" (GFX) slot but there is no requirement
 *              that a graphics dev may only use this slot
 *      slot1 - this is the "expansion"-slot 0  (EXP0), do not confuse
 *              with slot 0 (GFX).
 *      slot1x- this is the hack slot from I2 GIO compression board
 *              which occupied the 1f50_0000 addr space. Now its used
 *		by the xtalk-gio compression board and the EVO ASIC
 *      slot2 - this is the "expansion"-slot 1  (EXP1), do not confuse
 *              with slot 1 (EXP0).
 */
typedef int gioio_space_t;	/* GIO address space designation */

/* 
 * slotted space
 * GFX  a.k.a GIO slot 0
 * GIO1 a.k.a. GIO slot 1
 * GIO2 a.k.a. GIO slot 2
 */
#define	GIOIO_SPACE_GFX		(0)
#define	GIOIO_SPACE_GIO1	(1)
#define	GIOIO_SPACE_GIO1X	(2)
#define	GIOIO_SPACE_GIO2	(3)
/*
 * un-slotted space
 */
#define	GIOIO_SPACE_HPC_1		(3)
#define	GIOIO_SPACE_HPC_2		(4)

#define	GIOIO_SPACE_BAD			(5)

#define GIOIO_PIO_MAP_BASE		0x1f000000L
#define GIOIO_PIO_MAP_SIZE		(16 * 1024*1024)

#define GIOIO_ADDR_GFX			0x1f000000L
#define GIOIO_ADDR_GIO1			0x1f400000L
#define GIOIO_ADDR_GIO1X		0x1f500000L
#define GIOIO_ADDR_GIO2			0x1f600000L

#define GIOIO_GFX_PIO_MAP_SIZE		(4 * 1024*1024)
#define GIOIO_GIO1_PIO_MAP_SIZE		(2 * 1024*1024)
#define GIOIO_GIO1X_PIO_MAP_SIZE 	(1 * 1024*1024)
#define GIOIO_GIO2_PIO_MAP_SIZE		(4 * 1024*1024)

#define GIOIO_SL0_PR_SHFT		24
#define GIOIO_SL1_PR_SHFT		16
#define GIOIO_SL1X_PR_SHFT		8
#define GIOIO_SL2_PR_SHFT		0

#define	GIOIO_PROD_ID_MASK		0x7f
#define GIOIO_BUS_PROBE(_sl0, _sl1, _sl1x, _sl2)		\
	(							\
	    (_sl0  & GIOIO_PROD_ID_MASK) << GIOIO_SL0_PR_SHFT |	\
	    (_sl1  & GIOIO_PROD_ID_MASK) << GIOIO_SL1_PR_SHFT |	\
	    (_sl1x & GIOIO_PROD_ID_MASK) << GIOIO_SL1X_PR_SHFT |\
	    (_sl2  & GIOIO_PROD_ID_MASK) << GIOIO_SL2_PR_SHFT 	\
	)

#define	GIOIO_VGI1_ID			0x11
#define	GIOIO_CGI1_ID			0x12
#define	GIOIO_XXX_ID			0x2A		/* reserved */

/*
 * GIO DMA flags arguments
 *
 * GIO-Generic layer reserves low 16 bits while upper bits are for
 * GIO Implementation specific
 */
#define GIOIO_DMA_FLAGS		0xffff



/*
 * handles of various sorts
 */
typedef struct gioio_piomap_s *gioio_piomap_t;
typedef struct gioio_dmamap_s *gioio_dmamap_t;
typedef struct gioio_intr_s *gioio_intr_t;
typedef struct gioio_info_s *gioio_info_t;

/*
 * Interface to set GIO arbitration priority for devices that require
 * realtime characteristics.  pciio_priority_set is used to switch a
 * device between the PCI high-priority arbitration (i.e. real-time)
 * and the low priority arbitration (i.e. non real-time)
 */

typedef enum gioio_priority_e {
    GIO_PRIO_LOW,
    PCI_PRIO_HIGH
} gioio_priority_t;

/* PIO MANAGEMENT */


typedef gioio_piomap_t
gioio_piomap_alloc_f    (vertex_hdl_t dev,	/* set up mapping for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 gioio_space_t space,	/* which address space */
			 iopaddr_t giopio_addr,		/* starting address */
			 size_t byte_count,
			 size_t byte_count_max,		/* maximum size of a mapping */
			 unsigned flags);	/* defined in sys/pio.h */

typedef void
gioio_piomap_free_f     (gioio_piomap_t gioio_piomap);

typedef caddr_t
gioio_piomap_addr_f     (gioio_piomap_t gioio_piomap,	/* mapping resources */
			 iopaddr_t gioio_addr,	/* map for this giopio address */
			 size_t byte_count);	/* map this many bytes */

typedef void
gioio_piomap_done_f     (gioio_piomap_t gioio_piomap);

typedef caddr_t
gioio_piotrans_addr_f   (vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 gioio_space_t space,	/* which address space */
			 iopaddr_t gioio_addr,	/* starting address */
			 size_t byte_count,	/* map this many bytes */
			 unsigned flags);

/* DMA MANAGEMENT */

typedef gioio_dmamap_t
gioio_dmamap_alloc_f    (vertex_hdl_t dev,	/* set up mappings for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 size_t byte_count_max,		/* max size of a mapping */
			 unsigned flags);	/* defined in dma.h */

typedef void
gioio_dmamap_free_f     (gioio_dmamap_t dmamap);

typedef iopaddr_t
gioio_dmamap_addr_f     (gioio_dmamap_t dmamap,		/* use these mapping resources */
			 paddr_t paddr,		/* map for this address */
			 size_t byte_count);	/* map this many bytes */

typedef alenlist_t
gioio_dmamap_list_f     (gioio_dmamap_t dmamap,		/* use these mapping resources */
			 alenlist_t alenlist);	/* map this address/length list */

typedef void
gioio_dmamap_done_f     (gioio_dmamap_t dmamap);

typedef iopaddr_t
gioio_dmatrans_addr_f   (vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 paddr_t paddr,		/* system physical address */
			 size_t byte_count,	/* length */
			 unsigned flags);	/* defined in dma.h */

typedef alenlist_t
gioio_dmatrans_list_f   (vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 alenlist_t palenlist,	/* system address/length list */
			 unsigned flags);	/* defined in dma.h */


/* INTERRUPT MANAGEMENT */

typedef gioio_intr_t
gioio_intr_alloc_f      (vertex_hdl_t dev,	/* which GIO device */
			 device_desc_t dev_desc,	/* device descriptor */
			 gioio_intr_line_t lines,	/* which lines will be used */
			 vertex_hdl_t owner_dev);	/* owner of this intr */

typedef void
gioio_intr_free_f       (gioio_intr_t intr_hdl);

typedef int
gioio_intr_connect_f    (gioio_intr_t intr_hdl,		/* gioio intr resource handle */
			 intr_func_t intr_func,		/* gioio intr handler */
			 intr_arg_t intr_arg,	/* arg to intr handler */
			 void *thread);		/* intr thread to use */

typedef void
gioio_intr_disconnect_f (gioio_intr_t intr_hdl);

typedef vertex_hdl_t
gioio_intr_cpu_get_f    (gioio_intr_t intr_hdl);	/* gioio intr resource handle */


/* CONFIGURATION MANAGEMENT */

typedef void
gioio_provider_startup_f (vertex_hdl_t gioio_provider);

typedef void
gioio_provider_shutdown_f (vertex_hdl_t gioio_provider);

typedef gioio_priority_t
gioio_priority_set_f    (vertex_hdl_t giocard,
			 gioio_priority_t dmachan_pri);

typedef int
gioio_error_devenable_f (vertex_hdl_t gconn_vhdl,
			 int error_code);

/*
 * Adapters that provide a GIO interface adhere to this software interface.
 */
typedef struct gioio_provider_s {
    /* PIO MANAGEMENT */
    gioio_piomap_alloc_f   *piomap_alloc;
    gioio_piomap_free_f    *piomap_free;
    gioio_piomap_addr_f    *piomap_addr;
    gioio_piomap_done_f    *piomap_done;
    gioio_piotrans_addr_f  *piotrans_addr;

    /* DMA MANAGEMENT */
    gioio_dmamap_alloc_f   *dmamap_alloc;
    gioio_dmamap_free_f    *dmamap_free;
    gioio_dmamap_addr_f    *dmamap_addr;
    gioio_dmamap_list_f    *dmamap_list;
    gioio_dmamap_done_f    *dmamap_done;
    gioio_dmatrans_addr_f  *dmatrans_addr;
    gioio_dmatrans_list_f  *dmatrans_list;

    /* INTERRUPT MANAGEMENT */
    gioio_intr_alloc_f     *intr_alloc;
    gioio_intr_free_f      *intr_free;
    gioio_intr_connect_f   *intr_connect;
    gioio_intr_disconnect_f *intr_disconnect;
    gioio_intr_cpu_get_f   *intr_cpu_get;

    /* CONFIGURATION MANAGEMENT */
    gioio_provider_startup_f *provider_startup;
    gioio_provider_shutdown_f *provider_shutdown;
    gioio_priority_set_f   *priority_set;

    /* Error handling interface */
    gioio_error_devenable_f *error_devenable;
} gioio_provider_t;

/* GIO devices use these standard GIO provider interfaces */
extern gioio_piomap_alloc_f gioio_piomap_alloc;
extern gioio_piomap_free_f gioio_piomap_free;
extern gioio_piomap_addr_f gioio_piomap_addr;
extern gioio_piomap_done_f gioio_piomap_done;
extern gioio_piotrans_addr_f gioio_piotrans_addr;
extern gioio_dmamap_alloc_f gioio_dmamap_alloc;
extern gioio_dmamap_free_f gioio_dmamap_free;
extern gioio_dmamap_addr_f gioio_dmamap_addr;
extern gioio_dmamap_list_f gioio_dmamap_list;
extern gioio_dmamap_done_f gioio_dmamap_done;
extern gioio_dmatrans_addr_f gioio_dmatrans_addr;
extern gioio_dmatrans_list_f gioio_dmatrans_list;
extern gioio_intr_alloc_f gioio_intr_alloc;
extern gioio_intr_free_f gioio_intr_free;
extern gioio_intr_connect_f gioio_intr_connect;
extern gioio_intr_disconnect_f gioio_intr_disconnect;
extern gioio_intr_cpu_get_f gioio_intr_cpu_get;
extern gioio_provider_startup_f gioio_provider_startup;
extern gioio_provider_shutdown_f gioio_provider_shutdown;
extern gioio_priority_set_f gioio_priority_set;
extern gioio_error_devenable_f gioio_error_devenable;

/* Generic GIO card initialization interface */

typedef int
gioio_driver_register_f (gioio_product_id_t product_id,		/* card's product number */
			 gioio_device_id_t device_id,	/* card's device number */
			 char *driver_prefix,	/* driver prefix */
			 unsigned flags);

typedef vertex_hdl_t
gioio_device_register_f (vertex_hdl_t connectpt,	/* vertex for /hw/.../gioio/%d */
			 vertex_hdl_t master,	/* card's master ASIC (gio provider) */
			 gioio_slot_t slot,	/* card's slot (0..?) */
			 gioio_product_id_t product,	/* card's product id */
			 gioio_device_id_t device);	/* card's device number */

extern gioio_driver_register_f gioio_driver_register;
extern gioio_device_register_f gioio_device_register;

extern void             gioio_driver_unregister(char *driver_prefix);

extern int              gioio_device_attach(vertex_hdl_t giocard);	/* vertex created by gioio_device_register */

typedef void
gioio_error_register_f  (vertex_hdl_t conn,	/* gio slot of interest */
			 error_handler_f *efunc,	/* function to call */
			 error_handler_arg_t einfo);	/* first parameter */

extern gioio_error_register_f gioio_error_register;

extern void             gioio_reset(vertex_hdl_t card);

/*
 * Generic GIO interface, for use with all GIO providers
 * and all GIO devices.
 *
 * XXX- generated by analogy from XTALK and XWIDGET code,
 * these routines may not exist yet. If needed, tho, this
 * is what they should probably look like.
 */

/* Generic GIO interrupt interfaces */
extern vertex_hdl_t     gioio_intr_dev_get(gioio_intr_t gioio_intr);
extern vertex_hdl_t     gioio_intr_cpu_get(gioio_intr_t gioio_intr);

/* Generic GIO pio interfaces */
extern vertex_hdl_t     gioio_pio_dev_get(gioio_piomap_t gioio_piomap);
extern gioio_slot_t     gioio_pio_slot_get(gioio_piomap_t gioio_piomap);
extern gioio_space_t    gioio_pio_space_get(gioio_piomap_t gioio_piomap);
extern iopaddr_t        gioio_pio_gioaddr_get(gioio_piomap_t gioio_piomap);
extern size_t           gioio_pio_mapsz_get(gioio_piomap_t gioio_piomap);
extern caddr_t          gioio_pio_kvaddr_get(gioio_piomap_t gioio_piomap);

/* Generic GIO dma interfaces */
extern vertex_hdl_t     gioio_dma_dev_get(gioio_dmamap_t gioio_dmamap);
extern gioio_slot_t     gioio_dma_target_get(gioio_dmamap_t gioio_dmamap);

/* Register/unregister GIO providers and get implementation handle */
extern void             gioio_provider_register(vertex_hdl_t provider, gioio_provider_t *gioio_fns);
extern void             gioio_provider_unregister(vertex_hdl_t provider);
extern gioio_provider_t *gioio_provider_fns_get(vertex_hdl_t provider);

/* Generic gio slot information access interface */
extern gioio_info_t     gioio_info_get(vertex_hdl_t widget);
extern void             gioio_info_set(vertex_hdl_t widget, gioio_info_t widget_info);
extern vertex_hdl_t     gioio_info_dev_get(gioio_info_t gioio_info);
extern gioio_slot_t     gioio_info_slot_get(gioio_info_t gioio_info);
extern int              gioio_info_type_get(gioio_info_t gioio_info);
extern int              gioio_info_state_get(gioio_info_t gioio_info);
extern vertex_hdl_t     gioio_info_master_get(gioio_info_t gioio_info);
extern arbitrary_info_t gioio_info_mfast_get(gioio_info_t card_info);
extern gioio_provider_t *gioio_info_pops_get(gioio_info_t card_info);
extern gioio_product_id_t gioio_info_product_id_get(gioio_info_t gioio_info);
extern gioio_device_id_t gioio_info_device_id_get(gioio_info_t gioio_info);

extern gioio_info_t     gioio_cardinfo_get(vertex_hdl_t, gioio_slot_t, vertex_hdl_t *);

extern int              gioio_error_handler(vertex_hdl_t, int, ioerror_mode_t, ioerror_t *);

typedef void		gioio_iter_f(vertex_hdl_t);

extern void		gioio_iterate(char *prefix, gioio_iter_f *func);

#endif				/* _LANGUAGE_C */
#endif				/* __GIO_GIOIO_H__ */
