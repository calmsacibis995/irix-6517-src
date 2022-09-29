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

#if SN0

/* Generic VME bus provider */

#include <sys/types.h>
#include <sys/proc.h>         /* proc_t                               */
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/driver.h>
#include <sys/errno.h>        /* system call error numbers            */
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/ioerror.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <ksys/sthread.h>
#include <ksys/xthread.h>
#include <ksys/hwg.h>
#include <sys/vmereg.h>
#include <sys/schedctl.h>
#include <sys/edt.h>          /* edt_t                                */
#include <sys/alenlist.h>

#include <sys/PCI/pciio.h>

#include <sys/vme/vmeio.h>
#include <sys/vme/universe.h>
#include "usrvme_private.h"
#include "vmeio_private.h"

#define	NEW(ptr)	(ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

#ifndef	INFO_LBL_EDT
/* XXX move to <sys/iograph.h> */
#define	INFO_LBL_EDT	"_edt"
#endif

/* ------------------------------------------------------------------------

   Generic VMEbus service provider initialization

   ------------------------------------------------------------------------ */

/* 
 * Provider Function Location SHORTCUT
 *
 * On platforms with only one possible VMEbus provider, macros can be
 * set up at the top that cause the table lookups and indirections to
 * completely disappear.
 */

#if SN0

#include <sys/vme/universe.h>
#include "universe_private.h"
#define DEV_FUNC(dev, func)       (universe_##func)
#define CAST_PIOMAP(x)            ((universe_piomap_t)(x))
#define CAST_DMAMAP(x)            ((universe_dmamap_t)(x))
#define CAST_INTR(x)              ((universe_intr_t)(x))

#endif /* SN0 */

/*
 * Many functions are not passed their vertex
 * information directly; rather, they must
 * dive through a resource map. These macros
 * are available to coordinate this detail.
 */
#define	PIOMAP_FUNC(map,func)	DEV_FUNC((map)->dev,func)
#define	DMAMAP_FUNC(map,func)	DEV_FUNC((map)->dev,func)
#define	INTR_FUNC(intr,func)	DEV_FUNC((intr)->dev,func)

/* ------------------------------------------------------------------------

   Local function declarations

   ------------------------------------------------------------------------ */

/*
 *	vmeio_attach: called for each vertex in the graph
 *	that is a VMEbus provider.
 */
/*ARGSUSED*/
int
vmeio_attach(vertex_hdl_t vme)
{
	return 0;
}

/*
 * Associate a set of vmeio_provider functions with a vertex.
 */
void
vmeio_provider_register(vertex_hdl_t provider, vmeio_provider_t *vme_fns)
{
	hwgraph_info_add_LBL(provider, 
			     INFO_LBL_VME_FUNCS, 
			     (arbitrary_info_t)vme_fns);
}

/*
 * Disassociate a set of vmeio_provider functions with a vertex.
 */
void
vmeio_provider_unregister(vertex_hdl_t provider)
{
	arbitrary_info_t vme_fns;
	hwgraph_info_remove_LBL(provider, INFO_LBL_VME_FUNCS, &vme_fns);
}

/*
 * Obtain a pointer to the vmeio_provider functions for a specific VMEbus
 * provider.
 */
vmeio_provider_t *
vmeio_provider_fns_get(vertex_hdl_t provider)
{
	return((vmeio_provider_t *)hwgraph_fastinfo_get(provider));
}

/* 
 * Register an error handler
 */
void
vmeio_error_register(vertex_hdl_t vme_conn,
		     error_handler_f * errhandler,
		     error_handler_arg_t errinfo)
{
       vmeio_info_t info;

       info = vmeio_info_get(vme_conn);
       ASSERT(info != 0);
       info->errhandler = errhandler;
       info->errinfo = errinfo;
}

/* ------------------------------------------------------------------------

   Access functions of VME device information

   ------------------------------------------------------------------------ */

void
vmeio_info_set(vertex_hdl_t vme_conn, vmeio_info_t vme_info)
{
        hwgraph_fastinfo_set(vme_conn, (arbitrary_info_t)vme_info);
}

vmeio_info_t
vmeio_info_get(vertex_hdl_t vme_conn)
{
        return((vmeio_info_t)hwgraph_fastinfo_get(vme_conn));
}

void
vmeio_info_dev_set(vmeio_info_t vme_info, vertex_hdl_t vme_conn)
{
        vme_info->conn = vme_conn;
}

vertex_hdl_t
vmeio_info_dev_get(vmeio_info_t vme_info)
{
        return(vme_info->conn);
}

void
vmeio_info_provider_set(vmeio_info_t vme_info, vertex_hdl_t provider)
{
        vme_info->provider = provider;
}

vertex_hdl_t
vmeio_info_provider_get(vmeio_info_t vme_info)
{
        return(vme_info->provider);
}

void
vmeio_info_provider_fastinfo_set(vmeio_info_t vmeio_info, 
                               arbitrary_info_t provider_fastinfo)
{
        vmeio_info->provider_fastinfo = provider_fastinfo;
}

arbitrary_info_t
vmeio_info_provider_fastinfo_get(vmeio_info_t vmeio_info)
{
        return(vmeio_info->provider_fastinfo);
}

void
vmeio_info_am_set(vmeio_info_t vmeio_info, 
		  vmeio_am_t   am)
{
        vmeio_info->am = am;
}

vmeio_am_t
vmeio_info_am_get(vmeio_info_t vmeio_info)
{
        return(vmeio_info->am);
}


void
vmeio_info_addr_set(vmeio_info_t vmeio_info, 
		    iopaddr_t    vmeaddr)
{
        vmeio_info->addr = vmeaddr;
}

iopaddr_t
vmeio_info_addr_get(vmeio_info_t vmeio_info)
{
        return(vmeio_info->addr);
}

void
vmeio_info_errhandler_set(vmeio_info_t info, error_handler_f *errhandler)
{
	info->errhandler = errhandler;
}

error_handler_f *
vmeio_info_errhandler_get(vmeio_info_t info)
{
	return(info->errhandler);
}

void
vmeio_info_errinfo_set(vmeio_info_t info, error_handler_arg_t errinfo)
{
	info->errinfo = errinfo;
}

error_handler_arg_t
vmeio_info_errinfo_get(vmeio_info_t info)
{
	return(info->errinfo);
}

void
vmeio_info_private_set(vmeio_info_t info, error_handler_arg_t private_info)
{
	info->private_info = private_info;
}

error_handler_arg_t
vmeio_info_private_get(vmeio_info_t info)
{
	return(info->private_info);
}

/* ------------------------------------------------------------------------

   Generic VMEbus service provider configuration management

   ------------------------------------------------------------------------ */

#if !defined(DEV_FUNC)

#define DEV_FUNC(dev,func)      vmeio_to_provider_fns(dev)->func
#define CAST_PIOMAP(x)          ((vmeio_piomap_t)(x))
#define CAST_DMAMAP(x)          ((vmeio_dmamap_t)(x))
#define CAST_INTR(x)            ((vmeio_intr_t)(x))

static vmeio_provider_t *
vmeio_to_provider_fns(vertex_hdl_t dev)
{
        /* REFERENCED */
        vmeio_info_t vme_info;
        vertex_hdl_t provider;
        vmeio_provider_t *provider_fns;

        vme_info = vmeio_info_get(dev);
        ASSERT(vme_info != NULL);

        provider = vmeio_info_provider_get(vme_info);
        ASSERT(provider != GRAPH_VERTEX_NONE);

        provider_fns = vmeio_provider_fns_get(provider);
        ASSERT(provider_fns != NULL);

        return(provider_fns);
}

#endif /* !defined(DEV_FUNC) */

/* 
 * Startup a VMEbus provider
 */
void
vmeio_provider_startup(vertex_hdl_t vme_provider)
{
        DEV_FUNC(vme_provider,provider_startup)
                (vme_provider);
}

/* 
 * Shutdown a VMEbus provider
 */
void
vmeio_provider_shutdown(vertex_hdl_t vme_provider)
{
        DEV_FUNC(vme_provider,provider_shutdown)
                (vme_provider);
}

/* ------------------------------------------------------------------------

   Generic VME PIO

   ------------------------------------------------------------------------ */

vmeio_piomap_t
vmeio_piomap_alloc(vertex_hdl_t  vme_conn, /* This device               */
		   device_desc_t dev_desc, /* device descriptor         */
		   vmeio_am_t    am,       /* Address space             */
		   iopaddr_t     addr,     /* lowest address (or offset in window) */
		   size_t byte_count,      /* size of region */
		   size_t byte_count_max,  /* maximum size of a mapping */
		   unsigned flags)	   /* defined in sys/pio.h */
{
	return((vmeio_piomap_t)DEV_FUNC(vme_conn, piomap_alloc)
	       (vme_conn, dev_desc, am, addr, 
		byte_count, byte_count_max, flags));
}

void
vmeio_piomap_free(vmeio_piomap_t vme_piomap)
{
	PIOMAP_FUNC(vme_piomap,piomap_free)
		(CAST_PIOMAP(vme_piomap));
}

caddr_t
vmeio_piomap_addr(vmeio_piomap_t  vme_piomap,	/* mapping resources        */
		  iopaddr_t       vme_addr,	/* map for this vme address */
		  size_t          byte_count)	/* map this many bytes      */
{
	/* Sanity check */
	if  (vme_piomap == 0) {
		ASSERT(0);
		cmn_err(CE_ALERT,
			"%v: vmeio_piomap_addr: null VMEbus PIO map",
			vme_piomap->dev);
		return(0);
	}

	/* Ragne check */
	if ((vme_addr < vme_piomap->vmeaddr) ||
	    (vme_addr + byte_count >
	     vme_piomap->vmeaddr + vme_piomap->mapsz)) {
		ASSERT(0);
		cmn_err(CE_ALERT,
			"%v: vmeio_piomap_addr: request out of range",
			vme_piomap->dev);
		return(0);
	}

	vme_piomap->kvaddr = PIOMAP_FUNC(vme_piomap,piomap_addr)
		(CAST_PIOMAP(vme_piomap), vme_addr, byte_count);

	return(vme_piomap->kvaddr);
}


void
vmeio_piomap_done(vmeio_piomap_t vme_piomap)
{
	PIOMAP_FUNC(vme_piomap,piomap_done)
		(CAST_PIOMAP(vme_piomap));
}

/*
 * Retrieval functions for PIO map
 */
iopaddr_t
vmeio_piomap_vmeaddr_get(vmeio_piomap_t piomap)
{
	return(piomap->vmeaddr);
}

iopaddr_t
vmeio_piomap_mapsz_get(vmeio_piomap_t piomap)
{
	return(piomap->mapsz);
}

size_t
vmeio_pio_bcopyin(vmeio_piomap_t vme_piomap,
		  iopaddr_t	 vmeaddr,
		  caddr_t	 dest_sys_addr,
		  size_t	 size,
		  int		 itmsz,             
		  unsigned int	 flags)
{
	return((size_t)DEV_FUNC(vme_piomap,pio_bcopyin)
		(CAST_PIOMAP(vme_piomap), vmeaddr, dest_sys_addr, size, itmsz, flags));
}

size_t
vmeio_pio_bcopyout(vmeio_piomap_t vme_piomap,
		   iopaddr_t	  vmeaddr,
		   caddr_t	  src_sys_addr,
		   size_t	  size,
		   int		  itmsz,
		   unsigned int	  flags)
{
	return((size_t)DEV_FUNC(vme_piomap,pio_bcopyout)
		(CAST_PIOMAP(vme_piomap), vmeaddr, src_sys_addr, size, itmsz, flags));
}

/* -----------------------------------------------------------------------

   Generic VME DMA

   ----------------------------------------------------------------------- */

vmeio_dmamap_t
vmeio_dmamap_alloc(vertex_hdl_t  dev,	         /* device vertex handle  */
		   device_desc_t dev_desc,	 /* device descriptor     */
		   vmeio_am_t    am,          /* address space         */
		   size_t        byte_count_max, /* max size of a mapping */
		   unsigned      flags)          /* defined in "vmeio.h"  */
{	
	return((vmeio_dmamap_t) DEV_FUNC(dev, dmamap_alloc)
	       (dev, dev_desc, am, byte_count_max, flags));
}

void
vmeio_dmamap_free(vmeio_dmamap_t dmamap)   /* DMA mapping */
{
	DMAMAP_FUNC(pciio_dmamap, dmamap_free)
		(CAST_DMAMAP(dmamap));
}

iopaddr_t
vmeio_dmamap_addr(vmeio_dmamap_t vmeio_dmamap, /* Mapping resources          */
		  paddr_t        paddr,        /* Address to be mapped       */
		  size_t         byte_count)   /* Num. of bytes to be mapped */
{
	return DMAMAP_FUNC(vmeio_dmamap,dmamap_addr)
		(CAST_DMAMAP(vmeio_dmamap), paddr, byte_count);
}

void
vmeio_dmamap_done(vmeio_dmamap_t dmamap)
{
	DMAMAP_FUNC(dmamap,dmamap_done)(CAST_DMAMAP(dmamap));
}

alenlist_t
vmeio_dmamap_list(vmeio_dmamap_t  dmamap,
		  alenlist_t      alenlist,
		  unsigned        flags)
{
	return(DMAMAP_FUNC(dmamap, dmamap_list)
	       (CAST_DMAMAP(dmamap), alenlist, flags));
}

/* -----------------------------------------------------------------------

   Generic VME Interrupt handling

   ----------------------------------------------------------------------- */

/* 
 * Allocate interrupt resources
 */
vmeio_intr_t
vmeio_intr_alloc(vertex_hdl_t        vme_conn,
		 device_desc_t       dev_desc,
		 vmeio_intr_vector_t vec,
		 vmeio_intr_level_t  level,
		 vertex_hdl_t        owner_dev,
		 unsigned            flags)
{
	return((vmeio_intr_t) DEV_FUNC(vme_conn, intr_alloc) 
	       (vme_conn, dev_desc, vec, level, owner_dev, flags));
}

/*
 * Retrieve interrupt vector number from the interrupt object 
 * Return: a valid vector number if success, VME_INTR_VECTOR_NONE if failure
 */
vmeio_intr_vector_t
vmeio_intr_vector_get(vmeio_intr_t intr)
{
	ASSERT(intr != 0);  
	return(intr->vec);
}


/* 
 * Associate a software interrupt handler to the hardware interrupt 
 * resources.
 * Return: 0 if success, -1 if failure.
 */
/* ARGSUSED */
int
vmeio_intr_connect(vmeio_intr_t intr,
		   intr_func_t intr_func,
		   intr_arg_t intr_arg,
		   void *thread)
{
	return(INTR_FUNC(intr, intr_connect)
	       (CAST_INTR(intr), intr_func, intr_arg, thread));
}

/*
 * Disassociate handler with the specified interrupt.
 */
void
vmeio_intr_disconnect(vmeio_intr_t intr)
{
	INTR_FUNC(intr,intr_disconnect)(CAST_INTR(intr));
}


/* Release interrupt resources */
void
vmeio_intr_free(vmeio_intr_t intr)
{
	INTR_FUNC(intr, intr_free)(CAST_INTR(intr));
}

vertex_hdl_t
vmeio_intr_cpu_get(vmeio_intr_t intr)
{
	return(INTR_FUNC(intr, intr_cpu_get)
	       (CAST_INTR(intr)));
}


/* -----------------------------------------------------------------------

   Error management

   ----------------------------------------------------------------------- */


/* 
 * Reenable a device after the error is handled.
 */
int
vmeio_error_devenable(vertex_hdl_t vconn_vhdl, int err_code)
{
	int rv;

	rv = DEV_FUNC(vconn_vhdl, error_devenable)
		(vconn_vhdl, err_code);
	if (rv == IOERROR_HANDLED) {
		/*
		 * XXX Do any cleanup specific to this layer.. 
		 * Nothing for now..
		 */
	}

	return(rv);
}

/* ARGSUSED */
int
vmeio_error_handler(vertex_hdl_t   vme_conn,
		    int            err_code,
		    ioerror_mode_t err_mode,
		    ioerror_t *    ioerr)
{
	vmeio_info_t        info;
	error_handler_f *   error_handler;
	error_handler_arg_t error_info;

#if VME_DEBUG
	sync_printf("vmeio_error_handler\n");
#endif 
	
	info = vmeio_info_get(vme_conn);
	error_handler = vmeio_info_errhandler_get(info);

	if (error_handler) {
		error_info = vmeio_info_errinfo_get(info);
		return(error_handler(error_info,
				     err_code,
				     err_mode, 
				     ioerr));
	}
	else if (err_mode == MODE_DEVPROBE) {
		return(IOERROR_HANDLED);
	}
	else {
		return(IOERROR_UNHANDLED);
	}
}

/* -----------------------------------------------------------------------

   Atomic operations -- exported functions 

   ----------------------------------------------------------------------- */

/* 
 * Compare and swap atomic operation
 * Sync: only one guy can use this facility at one time.
 * Return: 0 if operation is successful, -1 if operation aborts abnoramlly
 */
int
vmeio_compare_and_swap(vmeio_piomap_t piomap,
		       iopaddr_t iopaddr,
		       __uint32_t old,
		       __uint32_t new)
{
	return(PIOMAP_FUNC(piomap, compare_and_swap)
	       (CAST_PIOMAP(piomap), iopaddr, old, new));
}


/* -----------------------------------------------------------------------

   VME device probing

   ----------------------------------------------------------------------- */

/* 
 * Probe the VME bus for a specified PIO space 
 * Return: 0 if probe is successful, error code otherwise.
 */
int
vmeio_probe(char *provider_name, iospace_t *iospace, int *rvalue)
{
	vertex_hdl_t provider;

	provider = hwgraph_path_to_vertex(provider_name);
	if (provider == GRAPH_VERTEX_NONE) {
		ASSERT(0);
		return(ENODEV);
	}

	return(DEV_FUNC(provider, probe)
	       (provider_name, iospace, rvalue));
}

/* -----------------------------------------------------------------------

   Supporting old interface

   ----------------------------------------------------------------------- */

/*
 * Convert the old general-purpose IO space type to the new VME specific 
 * IO space type.
 */

vmeio_am_t 
iospace_to_vmeioam(unsigned char old_type)
{
	/* VMEIO_SPACE_* kept in step with PIOMAP_* */
	/*
	return old_type;
	*/
	switch(old_type) {
	case PIOMAP_A16N:
		return(VMEIO_AM_A16 | VMEIO_AM_N);
	case PIOMAP_A16S:
		return(VMEIO_AM_A16 | VMEIO_AM_S);
	case PIOMAP_A24N:
		return(VMEIO_AM_A24 | VMEIO_AM_N);
	case PIOMAP_A24S:
		return(VMEIO_AM_A24 | VMEIO_AM_S);
	case PIOMAP_A32N:
		return(VMEIO_AM_A32 | VMEIO_AM_N);
	case PIOMAP_A32S:
		return(VMEIO_AM_A32 | VMEIO_AM_S);
	default:
		ASSERT(0);
	}

	return(0);
}

typedef int             edtinit_f(edt_t *);

/* =====================================================================
 *            Initialization
 */

#include <sys/cdl.h>

cdl_p                   vmeio_registry = NULL;

void
vmeio_init(void)
{
	cdl_p                   cp;

#if DEBUG_VMEIO
	printf("vmeio_init()\n");
#endif
	/* Allocate the registry.
	 * We might already have one.
	 * If we don't, go get one.
	 * Keep it MT/MP safe!
	 */
	if (vmeio_registry == NULL) {
		cp = cdl_new(EDGE_LBL_VME, "space", "base");
		if (!compare_and_swap_ptr((void **) &vmeio_registry, NULL, (void *) cp)) {
			cdl_del(cp);
		}
	}
	ASSERT(vmeio_registry != NULL);
}

#define	VMEIO_CDL_KEYS(c,t,a)	(((c)<<16)|(0xFFFF&(t))), (a)

void
vmeio_driver_register(edtinit_f * edtinit_func,
		      char *driver_prefix,
		      unsigned flags)
{
	int                     i = nedt;
	edt_t                  *ep;

#if DEBUG_VMEIO
	printf("vmeio_driver_register(0x%x, '%s', %d)\n", edtinit_func, driver_prefix, flags);
#endif

	/* in case some VME driver's init routine
	 * calls us before vmeio_init() gets called.
	 */
	if (vmeio_registry == NULL)
		vmeio_init();

	if (edtinit_func == NULL) {
		/* Null function? connect this driver
		 * to the "direct" connect points.
		 */
		cdl_add_driver(vmeio_registry,
			       VMEIO_CDL_KEYS(-1, 0, 0),
			       driver_prefix, flags);
		return;
	}
	i = nedt;
	for (ep = edt; i-- > 0; ep++) {

		if (ep->e_bus_type != ADAP_VME)
			continue;

		if (ep->e_init != edtinit_func) {
#if DEBUG_VMEIO
			printf("vmeio: skipped EDT ctlr=%d type=%d addr=0x%x\n",
			       ep->e_ctlr, ep->e_space->ios_type, ep->e_space->ios_iopaddr);
#endif
			continue;
		}
#if DEBUG_VMEIO
		printf("vmeio: matched EDT ctlr=%d type=%d addr=0x%x\n",
		       ep->e_ctlr, ep->e_space->ios_type, ep->e_space->ios_iopaddr);
#endif

		cdl_add_driver(vmeio_registry,
			       VMEIO_CDL_KEYS(ep->e_ctlr,
					      ep->e_space->ios_type,
					      ep->e_space->ios_iopaddr),
			       driver_prefix, flags);
	}
}

void
vmeio_driver_unregister(char *driver_prefix)
{
	cdl_del_driver(vmeio_registry, driver_prefix);
}

void
vmeio_edtscan(vertex_hdl_t master,
	      int adap,
	      device_register_f * device_register)
{
	vertex_hdl_t            vconn;
	edt_t                  *ep;
	edt_t                  *cep;
	edtinit_f              *e_init;
	int                     i;
	vmeio_am_t              am;
	iopaddr_t               base;

#if DEBUG_VMEIO
	cmn_err(CE_CONT, "vmeio_edtscan(%d, %d, 0x%x)\n\t%v\n", master, adap, device_register, master);
#endif

	/* start off with a neutral vertex */
	vconn = (*device_register) (master, adap, 0, 0);
	vmeio_device_register(vconn, adap, 0, 0);

	i = nedt;
	for (ep = edt; i-- > 0; ep++) {

		if (ep->e_bus_type != ADAP_VME)
			continue;

		am = iospace_to_vmeioam(ep->e_space->ios_type);
		base = ep->e_space->ios_iopaddr;

		if (adap != ep->e_adap) {
#if DEBUG_VMEIO
			cmn_err(CE_CONT, "vmeio: skipping EDT for adap %d ctlr %d am 0x%x base 0x%x\n",
				ep->e_adap, ep->e_ctlr, am, base);
#endif
			continue;
		}

#if DEBUG_VMEIO
		cmn_err(CE_CONT, "vmeio: matching EDT for adap %d ctlr %d am 0x%x base 0x%x\n",
		       ep->e_adap, ep->e_ctlr, am, base);
#endif

		vconn = (*device_register) (master, adap, am, base);

		/* update the EDT entry and tag it onto the
		 * connection point.
		 */
		if (adap == -1) {
			NEW(cep);
			*cep = *ep;
		} else
			cep = ep;

		cep->e_connectpt = vconn;
		cep->e_master = master;

		hwgraph_info_add_LBL
		    (vconn, INFO_LBL_EDT, (arbitrary_info_t) cep);

		/* If the edtinit function pointer isn't NULL,
		 * go set up the device.
		 */
		e_init = cep->e_init;
		if (e_init != NULL) {
#if DEBUG_VMEIO
			cmn_err(CE_CONT, "vmeio: applying EDTINIT at 0x%x\n", e_init);
#endif
			(*e_init) (cep);
		}
		vmeio_device_register(vconn, adap, am, base);
	}
}

void
vmeio_device_register(vertex_hdl_t vconn,
		      int          ctlr,
		      vmeio_am_t   am,
		      iopaddr_t    base)
{
#if DEBUG_VMEIO
	printf("vmeio_device_register(%v, %d, 0x%x, 0x%x)\n",
	       vconn, ctlr, am, base);
#endif
	cdl_add_connpt(vmeio_registry,
		       VMEIO_CDL_KEYS(ctlr, am, base),
		       vconn);
	}

/*
 * Call some function with each vertex that
 * might be one of this driver's attach points.
 */
void
vmeio_iterate(char *driver_prefix,
	      vmeio_iter_f * func)
{
	ASSERT(vmeio_registry != NULL);

	cdl_iterate(vmeio_registry, driver_prefix, (cdl_iter_f *) func);
}

#endif /* SN0 */
