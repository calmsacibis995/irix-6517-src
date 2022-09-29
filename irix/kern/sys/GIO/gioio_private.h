
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
#ifndef __GIO_GIOIO_PRIVATE_H__
#define __GIO_GIOIO_PRIVATE_H__

/*
 * gioio_private.h -- private definitions for gioio layer
 * GIO drivers should NOT include this file.
 */

#ident "sys/GIO/gioio_private: $Revision: 1.5 $"

/* XXX double check the slot number usage, change name 
   to avoid confusion with PCI if necessary XXX */
/*
 * All GIO providers set up PIO using this information.
 */
struct gioio_piomap_s {
	unsigned		pp_flags;	/* GIOIO_PIOMAP flags */
	vertex_hdl_t		pp_dev;		/* associated gio card */
	gioio_slot_t		pp_slot;	/* which slot the card is in */
	gioio_space_t		pp_space;	/* which address space */
	iopaddr_t		pp_gioaddr;	/* starting offset of mapping */
	size_t			pp_mapsz;	/* size of this mapping */
	caddr_t			pp_kvaddr;	/* kernel virtual address to use */
};

/*
 * All GIO providers set up DMA using this information.
 */
struct gioio_dmamap_s {
	unsigned		pd_flags;	/* GIOIO_DMAMAP flags */
	vertex_hdl_t		pd_dev;		/* associated gio card */
	gioio_slot_t		pd_slot;	/* which slot the card is in */
};

/*
 * All GIO providers set up interrupts using this information.
 */

struct gioio_intr_s {
	unsigned		pi_flags;	/* GIOIO_INTR flags */
	vertex_hdl_t		pi_dev;		/* associated gio card */
	gioio_intr_line_t	pi_line;	/* which interrupt line */
};

/*
 * Each GIO Card has one of these.
 */
struct gioio_info_s {
	char		       *c_fingerprint;	/* paranoia */
	vertex_hdl_t		c_vertex;	/* back pointer to vertex */
	gioio_slot_t		c_slot;		/* which slot the card is in */
	gioio_product_id_t	c_pr_id;	/* GIO card Product ID */
	gioio_device_id_t	c_device;	/* GIO card device num */
	int	       		c_state;	/* card state */
	vertex_hdl_t		c_master;	/* GIO bus provider */
	arbitrary_info_t	c_mfast;	/* CACHED master fastinfo */
	gioio_provider_t       *c_pops;		/* CACHED provider ops */
	error_handler_f	       *c_efunc;	/* error handling function */
	error_handler_arg_t	c_einfo;	/* first parameter for efunc */
};

#endif /* __GIO_GIOIO_PRIVATE_H__ */
