
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
#ifndef __PCI_PCIIO_PRIVATE_H__
#define __PCI_PCIIO_PRIVATE_H__

/*
 * pciio_private.h -- private definitions for pciio
 * PCI drivers should NOT include this file.
 */

#ident "sys/PCI/pciio_private: $Revision: 1.12 $"

/*
 * All PCI providers set up PIO using this information.
 */
struct pciio_piomap_s {
    unsigned                pp_flags;	/* PCIIO_PIOMAP flags */
    vertex_hdl_t            pp_dev;	/* associated pci card */
    pciio_slot_t            pp_slot;	/* which slot the card is in */
    pciio_space_t           pp_space;	/* which address space */
    iopaddr_t               pp_pciaddr;		/* starting offset of mapping */
    size_t                  pp_mapsz;	/* size of this mapping */
    caddr_t                 pp_kvaddr;	/* kernel virtual address to use */
};

/*
 * All PCI providers set up DMA using this information.
 */
struct pciio_dmamap_s {
    unsigned                pd_flags;	/* PCIIO_DMAMAP flags */
    vertex_hdl_t            pd_dev;	/* associated pci card */
    pciio_slot_t            pd_slot;	/* which slot the card is in */
};

/*
 * All PCI providers set up interrupts using this information.
 */

struct pciio_intr_s {
    unsigned                pi_flags;	/* PCIIO_INTR flags */
    vertex_hdl_t            pi_dev;	/* associated pci card */
    device_desc_t	    pi_dev_desc;	/* override device descriptor */
    pciio_intr_line_t       pi_lines;	/* which interrupt line(s) */
    intr_func_t             pi_func;	/* handler function (when connected) */
    intr_arg_t              pi_arg;	/* handler parameter (when connected) */
};

/*
 * Each PCI Card has one of these.
 */

struct pciio_info_s {
    char                   *c_fingerprint;
    vertex_hdl_t            c_vertex;	/* back pointer to vertex */
    pciio_bus_t             c_bus;	/* which bus the card is in */
    pciio_slot_t            c_slot;	/* which slot the card is in */
    pciio_function_t        c_func;	/* which func (on multi-func cards) */
    pciio_vendor_id_t       c_vendor;	/* PCI card "vendor" code */
    pciio_device_id_t       c_device;	/* PCI card "device" code */
    vertex_hdl_t            c_master;	/* PCI bus provider */
    arbitrary_info_t        c_mfast;	/* cached fastinfo from c_master */
    pciio_provider_t       *c_pops;	/* cached provider from c_master */
    error_handler_f        *c_efunc;	/* error handling function */
    error_handler_arg_t     c_einfo;	/* first parameter for efunc */

    struct {				/* state of BASE regs */
	pciio_space_t		w_space;
	iopaddr_t		w_base;
	size_t			w_size;
    }			    c_window[6];

    unsigned		    c_rbase;	/* EXPANSION ROM base addr */
    unsigned		    c_rsize;	/* EXPANSION ROM size (bytes) */

    pciio_piospace_t	    c_piospace;	/* additional I/O spaces allocated */
};

extern char             pciio_info_fingerprint[];
#endif				/* __PCI_PCIIO_PRIVATE_H__ */
