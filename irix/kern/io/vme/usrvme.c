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
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c(1)(ii) of the Rights
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
 * User level PIO access and user level interrupt of VME devices
 */

#ident  "$Revision: 1.10 $"

#if SN0

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/signal.h>                /* SYNC_SIGNAL                   */
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <ksys/hwg.h>                  /* device_master_set()           */
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/ddi.h>
#include <ksys/ddmap.h>                /* vhandl_t                      */
#include <sys/uthread.h>
#include <sys/proc.h>  
#include <sys/uli.h>                   /* uliargs_t                     */
#include <ksys/uli.h>                  /* new_uli(), free_uli()         */

#include <sys/vmereg.h> 

#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/usrpci.h>

#include <sys/vme/vmeio.h>
#include <sys/vme/usrvme.h>            /* UVIOCREGISTERULI              */
#include "vmeio_private.h"
#include "usrvme_private.h"             

#define	NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

int usrvme_devflag = D_MP;

#define	USRVME_PREFIX           "usrvme_"

#define EDGE_LBL_USRVME         "usrvme"

/* 
 * Private information stored at the master user level PIO device vertex 
 * XXX Need provide access function for this object 
 */
typedef struct usrvme_private_info_s {
	vertex_hdl_t         a32s_d64;
	vertex_hdl_t         a32s_d32;
	vertex_hdl_t         a32n_d64;
	vertex_hdl_t         a32n_d32;
	vertex_hdl_t         a24s_d32;
	vertex_hdl_t         a24s_d16;
	vertex_hdl_t         a24n_d32;
	vertex_hdl_t         a24n_d16;
	vertex_hdl_t         a16s_d32;
	vertex_hdl_t         a16s_d16;
	vertex_hdl_t         a16n_d32;
	vertex_hdl_t         a16n_d16;
	usrvme_piomap_list_t piomap_list;  
} * usrvme_private_info_t;

lock_t usrvme_lock; 

static vmeio_am_t 
usrvme_am(vertex_hdl_t, usrvme_private_info_t);  


/* Internal function for error handling */
static int
usrvme_error_handler(void *,          /* Error handler argument */
		     int,             /* Error code             */
		     ioerror_mode_t,  /* Error mode             */
		     ioerror_t *);      /* Error                  */

/* Internal functions for user level interrupt */
static void vmeuli_intr(intr_arg_t);
static void vmeuli_teardown(struct uli *);

/* ------------------------------------------------------------------------
  
   usrvme pseudo device registration

   ------------------------------------------------------------------------ */

/*
 * Create the usrvme pseudo device.
 * Each VMEbus gets one such device.  
 * Return: a vertex handle if success, 0 if failure
 */
vertex_hdl_t
usrvme_device_register(vertex_hdl_t provider)
{
	graph_error_t           rv;
	vertex_hdl_t            usrvme_master;
	vertex_hdl_t            a32s;
	vertex_hdl_t            a32n;
	vertex_hdl_t            a24s;
	vertex_hdl_t            a24n;
	vertex_hdl_t            a16s;
	vertex_hdl_t            a16n;
	vertex_hdl_t            a32s_d64;
	vertex_hdl_t            a32s_d32;
	vertex_hdl_t            a32n_d64;
	vertex_hdl_t            a32n_d32;
	vertex_hdl_t            a24s_d32;
	vertex_hdl_t            a24s_d16;
	vertex_hdl_t            a24n_d32;
	vertex_hdl_t            a24n_d16;
	vertex_hdl_t            a16s_d32;
	vertex_hdl_t            a16s_d16;
	vertex_hdl_t            a16n_d32;
	vertex_hdl_t            a16n_d16;
	usrvme_piomap_list_t    piomap_list;
	vmeio_info_t            vme_info;
	usrvme_private_info_t   private_info;

	/* Make the device visible for the provider */
	rv = hwgraph_path_add(provider,
			      EDGE_LBL_USRVME,
			      &usrvme_master);
	ASSERT(rv == GRAPH_SUCCESS);
	if (rv != GRAPH_SUCCESS) {
		return(0);
	}

	rv = hwgraph_path_add(usrvme_master,
			      EDGE_LBL_A32S,
			      &a32s);
	ASSERT(rv == GRAPH_SUCCESS);
	if (rv != GRAPH_SUCCESS) {
		return(0);
	}

	rv = hwgraph_char_device_add(a32s,
				     "d64",
				     USRVME_PREFIX,
				     &a32s_d64);
	ASSERT(rv == GRAPH_SUCCESS);

	rv = hwgraph_char_device_add(a32s,
				     "d32",
				     USRVME_PREFIX,
				     &a32s_d32);
	ASSERT(rv == GRAPH_SUCCESS);

	rv = hwgraph_path_add(usrvme_master,
			      EDGE_LBL_A32N,
			      &a32n);
	ASSERT(rv == GRAPH_SUCCESS);
	if (rv != GRAPH_SUCCESS) {
		return(0);
	}

	rv = hwgraph_char_device_add(a32n,
				     "d64",
				     USRVME_PREFIX,
				     &a32n_d64);
	ASSERT(rv == GRAPH_SUCCESS);

	rv = hwgraph_char_device_add(a32n,
				     "d32",
				     USRVME_PREFIX,
				     &a32n_d32);
	ASSERT(rv == GRAPH_SUCCESS);

	rv = hwgraph_path_add(usrvme_master,
			      EDGE_LBL_A24S,
			      &a24s);
	ASSERT(rv == GRAPH_SUCCESS);
	if (rv != GRAPH_SUCCESS) {
		return(0);
	}

	rv = hwgraph_char_device_add(a24s,
				     "d32",
				     USRVME_PREFIX,
				     &a24s_d32);
	ASSERT(rv == GRAPH_SUCCESS);


	rv = hwgraph_char_device_add(a24s,
				     "d16",
				     USRVME_PREFIX,
				     &a24s_d16);
	ASSERT(rv == GRAPH_SUCCESS);


	rv = hwgraph_path_add(usrvme_master,
			      EDGE_LBL_A24N,
			      &a24n);
	ASSERT(rv == GRAPH_SUCCESS);
	if (rv != GRAPH_SUCCESS) {
		return(0);
	}

	rv = hwgraph_char_device_add(a24n,
				     "d32",
				     USRVME_PREFIX,
				     &a24n_d32);
	ASSERT(rv == GRAPH_SUCCESS);

	rv = hwgraph_char_device_add(a24n,
				     "d16",
				     USRVME_PREFIX,
				     &a24n_d16);
	ASSERT(rv == GRAPH_SUCCESS);


	rv = hwgraph_path_add(usrvme_master,
			      EDGE_LBL_A16S,
			      &a16s);
	ASSERT(rv == GRAPH_SUCCESS);
	if (rv != GRAPH_SUCCESS) {
		return(0);
	}

	rv = hwgraph_char_device_add(a16s,
				     "d32",
				     USRVME_PREFIX,
				     &a16s_d32);
	ASSERT(rv == GRAPH_SUCCESS);

	rv = hwgraph_char_device_add(a16s,
				     "d16",
				     USRVME_PREFIX,
				     &a16s_d16);
	ASSERT(rv == GRAPH_SUCCESS);

	rv = hwgraph_path_add(usrvme_master,
			      EDGE_LBL_A16N,
			      &a16n);
	ASSERT(rv == GRAPH_SUCCESS);
	if (rv != GRAPH_SUCCESS) {
		return(0);
	}

	rv = hwgraph_char_device_add(a16n,
				     "d32",
				     USRVME_PREFIX,
				     &a16n_d32);
	ASSERT(rv == GRAPH_SUCCESS);


	rv = hwgraph_char_device_add(a16n,
				     "d16",
				     USRVME_PREFIX,
				     &a16n_d16);
	ASSERT(rv == GRAPH_SUCCESS);


	/* Initialize the list of PIO maps */
	piomap_list = 0;
	NEW(piomap_list);
	ASSERT(piomap_list != 0);
	ASSERT(piomap_list->head == 0);
	spinlock_init(&piomap_list->lock, "usrvme");

	/* Fill the private information of this VME device */
	private_info = 0;
	NEW(private_info);
	ASSERT(private_info != 0);
	private_info->a32s_d64 = a32s_d64;
	private_info->a32s_d32 = a32s_d32;
	private_info->a32n_d64 = a32n_d64;
	private_info->a32n_d32 = a32n_d32;
	private_info->a24s_d32 = a24s_d32;
	private_info->a24s_d16 = a24s_d16;
	private_info->a24n_d32 = a24n_d32;
	private_info->a24n_d16 = a24n_d16;
	private_info->a16s_d32 = a16s_d32;
	private_info->a16s_d16 = a16s_d16;
	private_info->a16n_d32 = a16n_d32;
	private_info->a16n_d16 = a16n_d16;	
	private_info->piomap_list = piomap_list;

	/* Fill the regular VME device information */
	vme_info = 0;
	NEW(vme_info);
	ASSERT(vme_info != 0);
	if (vme_info == 0) {
		cmn_err(CE_ALERT, "usrvme driver: can't create device info");
		return(0);
	}
	vmeio_info_dev_set(vme_info, usrvme_master);
	vmeio_info_provider_set(vme_info, provider);
	vmeio_info_provider_fastinfo_set(vme_info, 
					 hwgraph_fastinfo_get(provider));
	vmeio_info_private_set(vme_info, private_info);
	vmeio_info_set(usrvme_master, vme_info);

	/* Register error handler */
	vmeio_error_register(usrvme_master, 
			     (error_handler_f *) usrvme_error_handler, 
			     private_info);

	return(usrvme_master);
}

/* ------------------------------------------------------------------------
  
   User level PIO

   ------------------------------------------------------------------------ */

void
usrvme_init(void)
{
#if VME_DEBUG
	printf("usrvme_init\n");
#endif 
}

/* ARGSUSED */
int
usrvme_open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
	vertex_hdl_t          usrvme_dev;
	vertex_hdl_t          usrvme_master;
	vmeio_info_t          vme_info;
	usrvme_private_info_t private_info;
	vmeio_am_t            am;

	usrvme_dev = dev_to_vhdl(*devp);
	usrvme_master = hwgraph_connectpt_get(hwgraph_connectpt_get(usrvme_dev));
	ASSERT(usrvme_master != GRAPH_VERTEX_NONE);
	vme_info = vmeio_info_get(usrvme_master);
	ASSERT(vme_info != 0);
	private_info = vmeio_info_private_get(vme_info);
	ASSERT(private_info != 0);
	if (!private_info){
#if VME_DEBUG
		printf("usrvme_open: usrvme_info not found for 0x%x\n",
		       usrvme_master);
#endif
		return ENODEV;
	}

	am = usrvme_am(usrvme_dev, private_info);

	if (!((am & VMEIO_AM_A32) ||
	      (am & VMEIO_AM_A24) ||
	      (am & VMEIO_AM_A16))) {
#if VME_DEBUG
		printf("usrvme_open: Invald addr modifier%d to open\n", am);
#endif
		return EIO;
	}

#if VME_DEBUG
	printf("%v: usrvme_open succeeded\n", usrvme_dev);
#endif /* VME_DEBUG */

	return(0);
}


/* ARGSUSED */
int
usrvme_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
	vertex_hdl_t              usrvme_dev;
	vertex_hdl_t              usrvme_master;
	vmeio_info_t              vme_info;
	usrvme_private_info_t     private_info;
	caddr_t	                  kvaddr;
	vmeio_am_t                am;
	int                       error;
	iopaddr_t                 vme_addr;
	vmeio_piomap_t            piomap;
	usrvme_piomap_list_t      list;
	usrvme_piomap_list_item_t item;
	int                       s;

	usrvme_dev = dev_to_vhdl(dev);
	usrvme_master = hwgraph_connectpt_get(hwgraph_connectpt_get(usrvme_dev));
	vme_info = vmeio_info_get(usrvme_master);
	private_info = vmeio_info_private_get(vme_info);
	if (private_info == 0) {
#if VME_DEBUG
		printf("usrvme_map: usrvme_info not found for 0x%x\n",
		       usrvme_dev);
#endif
		return(ENODEV);
	}

	am = usrvme_am(usrvme_dev, private_info);
	ASSERT((am & VMEIO_AM_A32) ||
	       (am & VMEIO_AM_A24) ||
	       (am & VMEIO_AM_A16));

	vme_addr = off;
	len = (v_getlen(vt) > len) ? v_getlen(vt) : len;
	len = ctob(btoc(len));

	piomap = vmeio_piomap_alloc(usrvme_master,
				    0, 
				    am, 
				    vme_addr,
				    len, 
				    len, 
				    VMEIO_PIOMAP_FIXED);

	if (piomap == 0) {
		/* 
		 * Tell the user the PIO mapping resources
		 * couldn't be obtained.
		 */
		return(ENOMEM);
	}

	kvaddr = vmeio_piomap_addr(piomap, vme_addr, len);

	error = v_mapphys(vt, kvaddr, len);
 
	if (error) { 
		vmeio_piomap_done(piomap);
		vmeio_piomap_free(piomap);
	}
	else {
		/* Bookkeeping */
		item = 0;
		NEW(item);
		if (item == 0) {
			return(ENOSPC);
		}
		item->piomap = piomap;
		item->uthread = curuthread;
		item->uvaddr = v_getaddr(vt);
		item->kvaddr = kvaddr;
		
		/* Prepend the created PIOmap to the PIOmap list */
		list = private_info->piomap_list;
		s = mutex_spinlock(&list->lock);
		if (list->head == 0) {
			list->head = item;
		}
		else {
			item->next = list->head;
			list->head->prev = item;
			list->head = item;
		}
		mutex_spinunlock(&list->lock, s);

#if VME_DEBUG
		printf("usrvme_map: uthread 0x%x", curuthread);
		printf(" virtual: 0x%x kvaddr 0x%x\n", 
		       v_getaddr(vt), kvaddr);
#endif /* VME_DEBUG */
		
	}

	return error;
}

/* 
 * Unmap the device from the user address space
 * -- Free all the piomaps associated with the device 
 */
/* ARGSUSED2 */
int
usrvme_unmap(dev_t dev, vhandl_t *vt)
{
	vertex_hdl_t              usrvme_dev;    
	vertex_hdl_t              usrvme_master;
	vmeio_info_t              vme_info;
	usrvme_private_info_t     private_info;
	usrvme_piomap_list_t      list;
	usrvme_piomap_list_item_t item;
	int                       s;
	int                       found;         
	
	usrvme_dev = dev_to_vhdl(dev);
	usrvme_master = hwgraph_connectpt_get(hwgraph_connectpt_get(usrvme_dev));
	vme_info = vmeio_info_get(usrvme_master);
	private_info = vmeio_info_private_get(vme_info);

	list = private_info->piomap_list;
	s = mutex_spinlock(&list->lock);
	found = 0; 
	for (item = list->head; item != 0; item = item->next) {
		if ((item->uvaddr == v_getaddr(vt)) && 
		    (item->uthread == curuthread)) {
			if (item == list->head) {
				if (item->next == 0) {
					list->head = 0;
				}
				else {
					item->next->prev = 0;
					list->head = item->next;
				}
			}
			else if (item->next == 0) {
				item->prev->next = 0;
			}
			else {
				item->prev->next = item->next;
				item->next->prev = item->prev;
			}
			found = 1;
			break;
		}
	}
	mutex_spinunlock(&list->lock, s);

	if (found == 0) {
#if VME_DEBUG
		printf("No PIOmap was allocated: uthread 0x%x vaddr 0x%x\n",
			    curuthread, v_getaddr(vt));
#endif /* VME_DEBUG */
	}
	else {
		/* Free PIO and bookkeeping resources */
		vmeio_piomap_done(item->piomap);
		vmeio_piomap_free(item->piomap);
		DEL(item); 
	}
		
	return(0);	

}

/* ARGSUSED */
int
usrvme_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
	return 0;
}



/* -----------------------------------------------------------------------

   Error handling

   ----------------------------------------------------------------------- */

/*
 * Handle errors caused by this device: 
 * -- Send bus error signal to the process who mapped in the address
 * XXX Do we release the PIO resources for the process mapping the address
 * Called by: VMEbus provider error handler
 */
/*ARGSUSED*/
int
usrvme_error_handler(void * err_info,          /* Error handler argument */
		     int err_code,             /* Error code             */
		     ioerror_mode_t err_mode,  /* Error mode             */
		     ioerror_t *err)           /* Error                  */
{
	usrvme_private_info_t     private_info;
	iopaddr_t                 vme_addr;  
	usrvme_piomap_list_item_t item;
	int                       s;

#if VME_DEBUG
	printf("usrvme_error_hanlder\n");
#endif

	private_info = (usrvme_private_info_t) err_info;
	ASSERT(private_info != 0);
	vme_addr = IOERROR_GETVALUE(err, busaddr);
	ASSERT(vme_addr != 0);

	if (err_code & IOECODE_PIO) {
		/* 
		 * Find all the process mapping the VMEbus address
		 * and send bus error signal to them
		 */
		s = mutex_spinlock(&private_info->piomap_list->lock);
		for (item = private_info->piomap_list->head;
		     item != 0;
		     item = item->next) {
			if ((vme_addr >= 
			     vmeio_piomap_vmeaddr_get(item->piomap)) &&
			    (vme_addr < 
			     vmeio_piomap_vmeaddr_get(item->piomap) + 
			     vmeio_piomap_mapsz_get(item->piomap))) {
				sigtouthread(item->uthread, 
					     SIGBUS, 
					     0);
			}
		}
		mutex_spinunlock(&private_info->piomap_list->lock, s);

	}
	else {
		ASSERT(0);
	}

	return(IOERROR_HANDLED);
}

/*
 * Find out which space the vertex is responsible for.
 */

static vmeio_am_t
usrvme_am(vertex_hdl_t usrvme_dev, usrvme_private_info_t info)
{
	vmeio_am_t am;

	if (usrvme_dev == info->a32s_d64) {
		am = VMEIO_AM_A32 | VMEIO_AM_S | VMEIO_AM_D64;
	}
	else if (usrvme_dev == info->a32s_d32) {
		am = VMEIO_AM_A32 | VMEIO_AM_S | VMEIO_AM_D32;
	}	
	else if (usrvme_dev == info->a32n_d64) {
		am = VMEIO_AM_A32 | VMEIO_AM_N | VMEIO_AM_D64;
	}
	else if (usrvme_dev == info->a32n_d32) {
		am = VMEIO_AM_A32 | VMEIO_AM_N | VMEIO_AM_D32;
	}
	else if (usrvme_dev == info->a24s_d32) {
		am = VMEIO_AM_A24 | VMEIO_AM_S | VMEIO_AM_D32;
	}
	else if (usrvme_dev == info->a24s_d16) {
		am = VMEIO_AM_A24 | VMEIO_AM_S | VMEIO_AM_D16;
	}
	else if (usrvme_dev == info->a24n_d32) {
		am = VMEIO_AM_A24 | VMEIO_AM_N | VMEIO_AM_D32;
	}
	else if (usrvme_dev == info->a24n_d16) {
		am = VMEIO_AM_A24 | VMEIO_AM_N | VMEIO_AM_D16;
	}
	else if (usrvme_dev == info->a16s_d32) {
		am = VMEIO_AM_A16 | VMEIO_AM_S | VMEIO_AM_D32;
	}
	else if (usrvme_dev == info->a16s_d16) {
		am = VMEIO_AM_A16 | VMEIO_AM_S | VMEIO_AM_D16;
	}
	else if (usrvme_dev == info->a16n_d32) {
		am = VMEIO_AM_A16 | VMEIO_AM_N | VMEIO_AM_D32;
	}
	else if (usrvme_dev == info->a16n_d16) {
		am = VMEIO_AM_A16 | VMEIO_AM_N | VMEIO_AM_D16;
	}
	else {
		ASSERT(0);
	}
	return(am);
}	

/* ------------------------------------------------------------------------
  
   User level interrupt

   ----------------------------------------------------------------------- */


/*
 * Register a VME user level interrupt.
 * Called by: ioctl system call on 
 */

/* ARGSUSED */
int
usrvme_ioctl(dev_t dev, 
	     int cmd, 
	     void *arg,
	     int mode, 
	     cred_t *crp, 
	     int *rvalp)
{
	vertex_hdl_t     usrvme_dev;
	vertex_hdl_t     usrvme_master;
	struct uliargs   args;
	struct uli *     uli;
	vmeio_intr_t     intr;
	int              rv;
	device_desc_t    desc;
	cpuid_t          cpu;

	usrvme_dev = dev_to_vhdl(dev);
	usrvme_master = hwgraph_connectpt_get(hwgraph_connectpt_get(usrvme_dev));

	switch(cmd) {
	case UVIOCREGISTERULI:
		if (COPYIN_XLATE((void*)arg, &args, sizeof(args),
			   irix5_to_uliargs, get_current_abi(), 1)) {
			return(EFAULT);
		}

		desc = device_desc_dup(usrvme_dev);
		device_desc_flags_set(desc, D_INTR_NOTHREAD);
		device_desc_intr_swlevel_set(desc, 
					     INTR_SWLEVEL_NOTHREAD_DEFAULT);
		
		if (args.dd.vme.vec == VMEVEC_ANY) {
			intr = vmeio_intr_alloc(usrvme_master,
						desc,
						VMEIO_INTR_VECTOR_ANY,
						args.dd.vme.ipl,
						usrvme_master,
						0);
			args.dd.vme.vec = intr->vec;
		}
		else {
			intr = vmeio_intr_alloc(usrvme_master,
						desc,
						args.dd.vme.vec,
						args.dd.vme.ipl,
						usrvme_master,
						0);
		}
		device_desc_free(desc);

		if (intr == 0) {
#if VME_DEBUG
			printf("vmeio_intr_alloc failed\n");
#endif
			return(EINVAL);
		}

		cpu = cpuvertex_to_cpuid(vmeio_intr_cpu_get(intr));

		rv = new_uli(&args, &uli, cpu);
		if (rv) {
			return(rv);
		}

		rv = vmeio_intr_connect(intr, vmeuli_intr, 
					(intr_arg_t)(ulong_t) uli->index, 0);
		if (rv == -1) {
			free_uli(uli);
			return(EINVAL);
		}

		/* Register the teardown function */
		uli->teardown = vmeuli_teardown;
		uli->teardownarg1 = (__psint_t) intr;

		args.id = uli->index;

		if (XLATE_COPYOUT(&args, (void*)arg, sizeof(args),
			    uliargs_to_irix5, get_current_abi(), 1)) {
			free_uli(uli);
			return(EFAULT);
		}

		return(0);

	default:
		*rvalp = -1;
		return(EIO);
	}
		
}

/* 
 * Interrupt handler for every user of the user level interrupt.
 * Called by:
 */
static void
vmeuli_intr(intr_arg_t arg)
{
	int ulinum = (int)(__psint_t)arg;
	if (ulinum >= 0 && ulinum < MAX_ULIS) {
		uli_callup(ulinum);
	}
}

/*
 * Free the interrupt resources associated with the user level interrupt.
 * Called by: free_uli
 */
static void
vmeuli_teardown(struct uli *uli)
{
	vmeio_intr_disconnect((vmeio_intr_t)uli->teardownarg1);
	vmeio_intr_free((vmeio_intr_t)uli->teardownarg1);
}

#endif /* SN0 */
