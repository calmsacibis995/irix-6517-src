/*
 * Copyright 1996-1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/*
 * Generic VME bus service provider interface: 
 * Different VME adapters can plug into this interface
 */

#ident "$Revision: 1.8 $"

#ifndef __VME_VMEIO_H__
#define __VME_VMEIO_H__

#ifndef	__SYS_VMEREG_H
#include <sys/vmereg.h>
#endif

/* 
 * The value for each VME addr space has to be matched to that 
 * defined in vmereg.h for compatibility reasons. 
 */

typedef unsigned int vmeio_am_t;

#define VMEIO_AM_A16         (0x1)
#define VMEIO_AM_A24         (0x2)
#define VMEIO_AM_A32         (0x4)

#define VMEIO_AM_S           (0x8)
#define VMEIO_AM_N           (0x10)

#define VMEIO_AM_D64         (0x20)
#define VMEIO_AM_D32         (0x40)
#define VMEIO_AM_D16         (0x80)
#define VMEIO_AM_D8          (0x100)

#define VMEIO_AM_SINGLE      (0x200)
#define VMEIO_AM_BLOCK       (0x400)

extern vmeio_am_t
iospace_to_vmeioam(unsigned char);


/* Interrupt level type */

typedef unsigned char vmeio_intr_level_t;

#define VMEIO_INTR_LEVEL_NONE  (0)
#define VMEIO_INTR_LEVEL_1     (1)
#define VMEIO_INTR_LEVEL_2     (2)
#define VMEIO_INTR_LEVEL_3     (3)
#define VMEIO_INTR_LEVEL_4     (4)
#define VMEIO_INTR_LEVEL_5     (5)
#define VMEIO_INTR_LEVEL_6     (6)
#define VMEIO_INTR_LEVEL_7     (7)

/* Interrupt vector type */

typedef unchar vmeio_intr_vector_t; /* VME interrupt vector [1, 254] */

/* Only 256 interrupt vectors are allowed on 1 bus */
#define VMEIO_INTR_VECTOR_NONE      (0)
#define VMEIO_INTR_VECTOR_ANY       (0)
#define VMEIO_INTR_VECTOR_FIRST     (1) 
#define VMEIO_INTR_VECTOR_LAST      (254)
#define VMEIO_INTR_TABLE_SIZE       (256)

#if (_KERNEL && SN) 

/* 
 * The following declarations are used by kernel device drivers only.
 */

typedef void * vmeio_probe_arg_t;
typedef int vmeio_probe_func_t(vmeio_probe_arg_t arg1, 
			       vmeio_probe_arg_t arg2);

/* if this weren't a patch, edtinit_f would go in <sys/edt.h> */
typedef int		edtinit_f(edt_t *);

typedef vertex_hdl_t	device_register_f(vertex_hdl_t, int, vmeio_am_t, iopaddr_t);

/* ------------------------------------------------------------------------
  
   Device Driver Configuration
  
   ------------------------------------------------------------------------ */

/* 
 * Register the device driver with the bus provider 
 * used when the driver is *not* the one named
 * in the VECTOR line.
 */
extern void
vmeio_driver_register(edtinit_f *edtinit_func,
		      char *driver_prefix,
		      unsigned flags);

extern void
vmeio_driver_unregister(char * driver_prefix);

/* 
 * Register the error handler
 */
extern void 
vmeio_error_register(vertex_hdl_t,         /* Connection point on VMEbus */
		     error_handler_f *,    /* Error handling function    */
		     error_handler_arg_t); /* Error handler argument     */

/* 
 * Probe a VME device
 */
typedef int
(vmeio_probe_f)(char *,        /* Hwgraph pathname for the provider */
		iospace_t *,   /* IO space                          */
		int *rvalue);  /* Return value                      */

/* ------------------------------------------------------------------------
  
   VME PIO service provider interface
  
   ------------------------------------------------------------------------ */

/* 
 * Flag used to indicate that a possibly performance degrading and more
 * integrity checking mode is used to identify driver problems.
 * Used by: vmeio_piomap_alloc(), vmeio_dmamap_alloc(), and possibly more.
 */
#define VMEIO_DEBUG_FLAG        (0x8000)

/*
 * All VME bus providers set up PIO by using this information.
 */

/* Flags used in vmeio_piomap_alloc() */
#define VMEIO_PIOMAP_FIXED       (0x0001)
#define VMEIO_PIOMAP_UNFIXED     (0x0002)

typedef struct vmeio_piomap_s * vmeio_piomap_t;

/*
 * Allocate the PIO resources
 * Return: handle of the PIO resources if success, 0 otherwise.
 */
typedef vmeio_piomap_t
(vmeio_piomap_alloc_f)(vertex_hdl_t vme_conn,  /* Connection point on VMEbus */
		       device_desc_t dev_desc, /* Description of the device  */
		       vmeio_am_t  am,    /* Type of address space      */
		       iopaddr_t vme_addr,     /* Starting VMEbus address    */
		       size_t byte_count,      /* Size of a mapping          */
		       size_t byte_count_max,  /* Max size of a mapping      */
		       unsigned int flags);    /* Defined above              */

/*
 * Free the PIO mapping resource
 */
typedef void
(vmeio_piomap_free_f)(vmeio_piomap_t piomap); /* PIO map to be freed */

/*
 * Establish mapping from a VME bus address range to a kernel virtual address 
 * range using the allocated PIO resources. 
 * Return: kernel virtual address if success, 0 if otherwise.
 */
typedef caddr_t
(vmeio_piomap_addr_f)(vmeio_piomap_t piomap, /* PIO mapping resources        */
		      iopaddr_t vme_addr,    /* Starting VME bus address     */
		      size_t byte_count);    /* Number of bytes to be mapped */

/* 
 * Notify the system that a driver is done with using the piomap resource.
 * Parameters: the piomap to be notified
 */
typedef void
(vmeio_piomap_done_f)(vmeio_piomap_t piomap); /* PIO mapping resources       */


/* Copy data from the device buffer to the system memory */
typedef size_t
(vmeio_pio_bcopyin_f)(vmeio_piomap_t piomap,  /* PIO mapping resources */
		      iopaddr_t vme_addr,     /* VMEbus addr of the source */
		      caddr_t dest_sys_addr,  /* Sys addr of the destination */
		      size_t byte_count,      /* Size of the transfer */
		      int itmsz,              /* Unit of the trasfer */
		      unsigned int flags);

/* Copy data from the system memory to the device buffer */
typedef size_t
(vmeio_pio_bcopyout_f)(vmeio_piomap_t piomap, /* PIO mapping resources */
		     iopaddr_t	vme_addr,     /* VMEbus addr of destination */
		     caddr_t src_sys_addr,    /* Sys addr of the source */
		     size_t size,             /* Size of the transfer */
		     int itmsz,               /* Unit of the trasfer */
		     unsigned int flag);  

extern iopaddr_t
vmeio_piomap_vmeaddr_get(vmeio_piomap_t);

extern iopaddr_t
vmeio_piomap_mapsz_get(vmeio_piomap_t);

/* ------------------------------------------------------------------------
  
   VME DMA service provider interface
  
   ------------------------------------------------------------------------ */

/*
 * Flags used by the function below
 *
 * VMEIO_DMA_CMD: configure this stream as a generic "command" stream. 
 *	Generally this means turn off prefetchers and write gatherers, and 
 *	whatever else might be necessary to make command ring DMAs work as 
 *	expected. (see PCIIO_DMA_CMD)
 *
 * VMEIO_DMA_DATA: configure this stream as a generic "data" stream. 
 *	Generally, this means turning on prefetchers and write gatherers, and 
 *	anything else that might increase the DMA throughput (short of using 
 *	"high priority" or "real time" resources that may lower overall 
 *	system performance). (see PCIIO_DMA_DATA)
 */

#define VMEIO_INPLACE	(0x0001) /* Overload the passed in addr-len list */
#define VMEIO_NOSLEEP	(0x0002) /* Don't sleep on waiting for memory    */
#define VMEIO_CONTIG	(0x0004) /* Please give me one VMEbus addr range */
#define VMEIO_DMA_CMD	(0x0008)
#define VMEIO_DMA_DATA	(0x0010)

typedef struct vmeio_dmamap_s *vmeio_dmamap_t;

/*
 * Allocate mapping resources needed for DMA
 * Return: handle of the new DMA map if success, 0 if failed.
 */
typedef vmeio_dmamap_t
(vmeio_dmamap_alloc_f)(vertex_hdl_t  dev,             /* Hwgraph handle      */
		       device_desc_t dev_desc,        /* Device descriptor   */
		       vmeio_am_t    am,              /* Address space       */
		       size_t        byte_count_max,  /* Max size of mapping */
		       unsigned      flags);	      /* defined above       */

/*
 * Free the DMA mapping resources
 */
typedef void 
(vmeio_dmamap_free_f)(vmeio_dmamap_t dmamap);

#define VMEIO_DMA_A32_START       (0x80000000) 
#define VMEIO_DMA_A32_END         (0xffffffff) 
#define VMEIO_DMA_A32_SIZE        (VMEIO_DMA_A32_END - VMEIO_DMA_A32_START + 1)
#define VMEIO_DMA_A24_START       (0x0)
#define VMEIO_DMA_A24_END         (0x7fffff) 
#define VMEIO_DMA_A24_SIZE        (VMEIO_DMA_A24_END - VMEIO_DMA_A24_START + 1)
#define VMEIO_DMA_INVALID_ADDR    (0x800001)    /* Invalid VME DMA addr */

/*
 * Establish the mapping from phys addr range to VME bus addr range
 * Return: VME bus address mapped to the physical addr range if success,
 *         VMEIO_DMA_INVALID_VMEADDR otherwise.
 */
typedef iopaddr_t
(vmeio_dmamap_addr_f)(vmeio_dmamap_t dmamap,      /* The DMA mapping used    */
		      paddr_t        paddr,       /* System phys addr        */
		      size_t         byte_count); /* Num of bytes to be used */


/*
 * Establish the mapping from phys addr range to VME bus addr range
 * Return: An addr/len pair of VME bus addr range if success, 0 otherwise.
 * Note: The returned alenlist should be freed in the caller
 */
typedef alenlist_t
(vmeio_dmamap_list_f)(vmeio_dmamap_t dmamap,   /* DMA mapping resources    */
		      alenlist_t     alenlist, /* List of (phys addr, len) */
		      unsigned       flags);   /* Flags                    */

/*
 * Notify the system that a driver is done with using the DMA resources.
 */
typedef void
(vmeio_dmamap_done_f)(vmeio_dmamap_t dmamap); 

/* ------------------------------------------------------------------------
  
   VME Interrupt service provider interface
  
   ------------------------------------------------------------------------ */

typedef struct vmeio_intr_s *vmeio_intr_t;

/*
 * Allocate resources for interrupt handling of the device
 * Return: an opaque handle of the interrupt resources
 */
typedef vmeio_intr_t
(vmeio_intr_alloc_f)(vertex_hdl_t,        /* Associated VME device     */
		     device_desc_t,       /* Descriptor for VME device */
		     vmeio_intr_vector_t, /* Interrupt vector          */
		     vmeio_intr_level_t,  /* Interrupt level           */
		     vertex_hdl_t,        /* Owner device              */
		     unsigned flags);     /* Flags                     */

/*
 * Retrieve interrupt vector number from the interrupt object 
 * Return: a valid vector number if success, VMEIO_INTR_VECTOR_NONE if failure
 */
extern vmeio_intr_vector_t
vmeio_intr_vector_get(vmeio_intr_t);       /* Interrupt resources        */

/*
 * Free the interrupt resources
 */
typedef void
(vmeio_intr_free_f)(vmeio_intr_t intr_hdl); /* VME intr resource handle */

/*
 * Connect an interrupt handler to the interrupt resources 
 * Return: 0 if the intrrupt handler is installed successfully
 *         -1 if failed
 */
typedef int
(vmeio_intr_connect_f)(vmeio_intr_t, /* VME intr resource handle */
		       intr_func_t, /* VME intr handler */
		       intr_arg_t,
		       void *thread); /* Arg to intr handler */

/*
 * Disconnect an intr handler from the specified interrupt resources 
 */
typedef void
(vmeio_intr_disconnect_f)(vmeio_intr_t intr_hdl); /* VME intr resource handle */

extern vertex_hdl_t 
vmeio_intr_cpu_get(vmeio_intr_t); 

/* ------------------------------------------------------------------------
  
   Error handling
  
   ------------------------------------------------------------------------ */

/* 
 * Reenable a device after handling the error 
 */
typedef int
(vmeio_error_devenable_f)(vertex_hdl_t,  /* Connection point on VMEbus */
			  int);          /* IO error code              */

/* ------------------------------------------------------------------------
  
   Atomic Operations -- Compare and Swap 
  
   ------------------------------------------------------------------------ */

/* 
 * Compare and swap atomic operation
 * Sync: only one guy can use this facility at one time.
 * Return: 0 if operation is successful, -1 if operation aborts abnoramlly
 */

typedef int
(vmeio_compare_and_swap_f)(vmeio_piomap_t,  /* PIO resources    */
			   iopaddr_t,       /* VMEbus address   */
			   __uint32_t,      /* Comparison value */
			   __uint32_t);     /* New value        */

/* ------------------------------------------------------------------------
  
   Generic VMEbus service provider configuration management
  
   ------------------------------------------------------------------------ */

/*
 * Initialize the provider to serve PIO, DMA, interrupt, error handling, 
 * user level access (PIO, DMA, DMA engine, Interrupt) needs for VME devices.
 * Called by: <provider>_attach()
 */
typedef void
(vmeio_provider_startup_f)(vertex_hdl_t);  /* VMEbus provider vertex */

/*
 * Shut down the services provided for all the VME devices on the bus
 */
typedef void
(vmeio_provider_shutdown_f)(vertex_hdl_t); /* VMEbus provider vertex */


/*
 * Adapters that provide a VME interface adhere to this software interface.
 */
typedef struct vmeio_provider_s {
	/* PIO MANAGEMENT */
	vmeio_piomap_alloc_f 			*piomap_alloc;
	vmeio_piomap_free_f			*piomap_free;
	vmeio_piomap_addr_f			*piomap_addr;
	vmeio_piomap_done_f			*piomap_done;
	vmeio_pio_bcopyin_f                     *pio_bcopyin;
	vmeio_pio_bcopyout_f                    *pio_bcopyout;
 
	/* DMA MANAGEMENT */
	vmeio_dmamap_alloc_f			*dmamap_alloc;
	vmeio_dmamap_free_f			*dmamap_free;
	vmeio_dmamap_addr_f			*dmamap_addr;
	vmeio_dmamap_list_f			*dmamap_list;
	vmeio_dmamap_done_f			*dmamap_done;

	/* INTERRUPT MANAGEMENT */
	vmeio_intr_alloc_f			*intr_alloc;
	vmeio_intr_free_f			*intr_free;
	vmeio_intr_connect_f			*intr_connect;
	vmeio_intr_disconnect_f			*intr_disconnect;

	/* CONFIGURATION MANAGEMENT */
	vmeio_provider_startup_f		*provider_startup;
	vmeio_provider_shutdown_f		*provider_shutdown;
	
	/* Error handling interface */
	vmeio_error_devenable_f			*error_devenable;

	/* Device configuration */
	vmeio_probe_f                           *probe;

	/* Atomic operations */
	vmeio_compare_and_swap_f                *compare_and_swap;

} vmeio_provider_t;

/* VME devices use these standard VMEbus provider interfaces */
extern vmeio_piomap_alloc_f		vmeio_piomap_alloc;
extern vmeio_piomap_free_f		vmeio_piomap_free;
extern vmeio_piomap_addr_f		vmeio_piomap_addr;
extern vmeio_piomap_done_f		vmeio_piomap_done;
extern vmeio_pio_bcopyin_f              vmeio_pio_bcopyin;
extern vmeio_pio_bcopyout_f             vmeio_pio_bcopyout;

extern vmeio_dmamap_alloc_f		vmeio_dmamap_alloc;
extern vmeio_dmamap_free_f		vmeio_dmamap_free;
extern vmeio_dmamap_addr_f		vmeio_dmamap_addr;
extern vmeio_dmamap_list_f		vmeio_dmamap_list;
extern vmeio_dmamap_done_f		vmeio_dmamap_done;

extern vmeio_intr_alloc_f		vmeio_intr_alloc;
extern vmeio_intr_free_f		vmeio_intr_free;
extern vmeio_intr_connect_f		vmeio_intr_connect;
extern vmeio_intr_disconnect_f		vmeio_intr_disconnect;

extern vmeio_provider_startup_f		vmeio_provider_startup;
extern vmeio_provider_shutdown_f	vmeio_provider_shutdown;
extern vmeio_error_devenable_f		vmeio_error_devenable;
extern vmeio_probe_f                    vmeio_probe;

extern vmeio_compare_and_swap_f         vmeio_compare_and_swap;

/* ------------------------------------------------------------------------
  
   Generic VMEbus service provider configuration management
  
   ------------------------------------------------------------------------ */

void vmeio_provider_register(vertex_hdl_t provider, vmeio_provider_t *vme_fns);
void vmeio_provider_unregister(vertex_hdl_t provider);
vmeio_provider_t *vmeio_provider_fns_get(vertex_hdl_t provider);

/* ------------------------------------------------------------------------
  
   Generic VME device information access interface
  
   ------------------------------------------------------------------------ */

typedef struct vmeio_info_s * vmeio_info_t;

extern vmeio_info_t vmeio_info_get(vertex_hdl_t);     
extern vertex_hdl_t vmeio_info_dev_get(vmeio_info_t vme_info);
extern vmeio_am_t vmeio_info_am_get(vmeio_info_t);                
extern iopaddr_t vmeio_info_addr_get(vmeio_info_t);                 
extern vertex_hdl_t vmeio_info_provider_get(vmeio_info_t);
extern arbitrary_info_t vmeio_info_provider_fastinfo_get(vmeio_info_t); 
extern void * vmeio_info_private_get(vmeio_info_t);  

extern void	vmeio_edtscan(vertex_hdl_t, int, device_register_f *);
extern void	vmeio_device_register(vertex_hdl_t, int, vmeio_am_t, iopaddr_t);

typedef void		vmeio_iter_f(vertex_hdl_t conn);

extern void             vmeio_iterate(char *driver_prefix,
				      vmeio_iter_f * func);

#endif /* _KERNEL */

#define	VMEIOCSETCTLR	_IO('s','c')	/* Set Controller */

#endif /* __VME_VMEIO_H__ */

