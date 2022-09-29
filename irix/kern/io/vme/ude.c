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
 * A kernel device driver to manage Universe DMA engine
 * A close friend of Universe chip device driver
 */

#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <ksys/xthread.h>
#include <sys/debug.h>         /* assert()                          */
#include <sys/iobus.h>         /* device_desc_t, intr_func_t        */
#include <sys/ioerror.h>       /* error_handler_f                   */
#include <sys/cred.h>          /* cred_t                            */
#include <sys/errno.h>         /* errors                            */ 
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/edt.h>           /* edt_t iospace_t                   */
#include <sys/systm.h>         /* copyin[out]                       */
#include <sys/kmem.h>          /* Kernel memory: dynamic allocation */     
#include <ksys/ddmap.h>        /* v_mapphys()                       */
#include <sys/alenlist.h>
#include <sys/PCI/pciio.h>
#include <sys/vme/vmeio.h>
#include <sys/vme/universe.h>
#include "universe_private.h"

int ude_devflag = D_MP; /* We can handle multiprocessor */

#define UDE_PREFIX "ude_"

#define	NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

int ude_unmap(dev_t dev, vhandl_t *vt);

typedef struct ude_info_s {
	vertex_hdl_t vertex;
	vertex_hdl_t provider;
	arbitrary_info_t provider_fastinfo;
} * ude_info_t;

static void
ude_info_dev_set(ude_info_t info, vertex_hdl_t ude)
{
	info->vertex = ude;
}

/* REFERENCED */
static vertex_hdl_t
ude_info_dev_get(ude_info_t info)
{
	return(info->vertex);
}

static void
ude_info_provider_set(ude_info_t info, vertex_hdl_t provider)
{
	info->provider = provider;
}

/* REFERENCED */
static vertex_hdl_t
ude_info_provider_get(ude_info_t info)
{
	return(info->provider);
}

static void
ude_info_provider_fastinfo_set(ude_info_t info, 
			       arbitrary_info_t provider_fastinfo)
{
	info->provider_fastinfo = provider_fastinfo;
}

static arbitrary_info_t
ude_info_provider_fastinfo_get(ude_info_t info)
{
	return(info->provider_fastinfo);
}

static void
ude_info_set(vertex_hdl_t ude, ude_info_t info)
{
	hwgraph_fastinfo_set(ude, (arbitrary_info_t)info);
}

static ude_info_t
ude_info_get(vertex_hdl_t ude)
{
	return((ude_info_t) hwgraph_fastinfo_get(ude));
}

/* -------------------------------------------------------------------------

   Device configuration

   ------------------------------------------------------------------------- */

/* 
 * -- Create the device vertex
 */
vertex_hdl_t
ude_device_register(vertex_hdl_t provider)
{
	vertex_hdl_t ude;
	ude_info_t info;
	graph_error_t rv;
	
#if VME_DEBUG
	printf("%v: ude_device_register()\n", provider);
#endif /* VME_DEBUG */

	rv = hwgraph_char_device_add(provider, 
				     EDGE_LBL_DMA_ENGINE,
				     UDE_PREFIX,
				     &ude);
	if (rv != GRAPH_SUCCESS) {
		ASSERT(0);
		return(0);
	}

	info = 0;
	NEW(info);
	if (info == 0) {
		ASSERT(0);
		return(0);
	}
	ude_info_dev_set(info, ude);
	ude_info_provider_set(info, provider);
	ude_info_provider_fastinfo_set(info, hwgraph_fastinfo_get(provider));
	ude_info_set(ude, info);

	return(ude);
}

void
ude_init()
{
#if VME_DEBUG
	printf("ude_init\n");
#endif /* VME_DEBUG */
}

/* ARGSUSED */
int
ude_open(dev_t *devp, int flag, int type, cred_t *crp)
{
	vertex_hdl_t dma_engine;
	universe_state_t univ_state;
	ude_info_t info;
	int s;

#if VME_DEBUG
	printf("ude_open\n");
#endif 

	dma_engine = dev_to_vhdl(*devp);
	info = ude_info_get(dma_engine);
	ASSERT(info != 0);
	univ_state = (universe_state_t) ude_info_provider_fastinfo_get(info);
	ASSERT(univ_state != 0);

	ASSERT(univ_state->dma_engine.pci_dmamaps == 0);

	/* Acquire exclusive right on the DMA engine */
	s = mutex_spinlock(&(univ_state->dma_engine.lock));
	if (univ_state->dma_engine.state == UDE_BUSY) {
		mutex_spinunlock(&(univ_state->dma_engine.lock), s);
		return(EBUSY);
	}
	univ_state->dma_engine.state = UDE_BUSY;
	mutex_spinunlock(&(univ_state->dma_engine.lock), s);

	return(0);
}

/* 
 * Map the physical device registers and physical memory into the user space: 
 * -- The DMA engine control registers
 * -- The DMA linked list memory 
 * Security issues:
 * -- Even SGI provides the users the library which makes mmap system call 
 *    to map the DMA engine control registers, it is still desirable to 
 *    map as few registers as necessary into the user address space. 
 * ??? Offset was used as a flag to tell the map function what to do, is 
 *     it a breach of the common understanding?
 */


/* ARGSUSED */
int
ude_map(dev_t dev, vhandl_t *vt, off_t addr, size_t len)
{
	vertex_hdl_t dma_engine;
	ude_info_t info;
	universe_state_t univ_state;
	int rv;

#if VME_DEBUG
	printf("ude_map\n");
#endif /* VME_DEBUG */

	dma_engine = dev_to_vhdl(dev);
	info = ude_info_get(dma_engine);
	ASSERT(info != 0);
	univ_state = (universe_state_t) ude_info_provider_fastinfo_get(info);
	ASSERT(univ_state != 0);

	if (addr == UNIVERSE_DMA_ENGINE_REGS_OFFSET) {
		/* 
		 * Map only the necessary set of DMA engine control 
		 * registers into user address space.
		 */
		rv = v_mapphys(vt, univ_state->chip, len);
		if (rv) {
			ude_unmap(dev, vt);
			return(rv);
		}
	}
	else {
		ASSERT(addr == UNIVERSE_DMA_ENGINE_LINKED_LIST_OFFSET);
		rv = v_mapphys(vt, univ_state->dma_engine.ll_kvaddr, len);
		if (rv) {
			ude_unmap(dev, vt);
			return(rv);
		}
	}
	
	return(0);
}

/* ARGSUSED */
int
ude_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp)
{
	vertex_hdl_t                          dma_engine;
	ude_info_t                            info;
	universe_state_t                      univ_state;
	caddr_t                               addr;
	size_t                                byte_count;
	alenlist_t                            alenlist;
	universe_dma_engine_buffer_internal_t buffer;
	int                                   rv;
	pciio_dmamap_t                        pci_dmamap;
	int                                   i;
	iopaddr_t                             pciaddr;
	universe_dma_engine_buffer_item_t     pci_dmamap_item;
	universe_dma_engine_buffer_item_t     pci_dmamap_prev;

#if VME_DEBUG
	printf("ude_ioctl\n");
#endif /* VME_DEBUG */
	
	dma_engine = dev_to_vhdl(dev);
	info = ude_info_get(dma_engine);
	ASSERT(info != 0);
	univ_state = (universe_state_t)ude_info_provider_fastinfo_get(info);
	ASSERT(univ_state != 0);
	
	switch(cmd) {
	case UDE_IOCTL_BUFPREP:
		buffer.alens[0].addr = 0;
		buffer.alens[0].byte_count = 0;

		/* Set up the DMA resources for this range of the addr range */
		rv = copyin((void *)arg, (void *)&buffer, 
			    sizeof(universe_dma_engine_buffer_internal_t));
		if (rv != 0) {
			return(EFAULT);
		}

		ASSERT(buffer.num_of_items == 1);

		/* Deal with applicatons compiled in o32 and n32 */
		addr = (caddr_t) (__uint64_t) buffer.alens[0].addr;
		byte_count = buffer.alens[0].byte_count;

#if VME_DEBUG
		printf("addr 0x%x, byte_count 0x%x\n", 
		       buffer.alens[0].addr, buffer.alens[0].byte_count);
#endif /* VME_DEBUG */

		/* Allocate PCIbus DMA resources */
		pci_dmamap = pciio_dmamap_alloc(univ_state->pci_conn, 
						0,
						byte_count, 
						PCIIO_BYTE_STREAM | 
						PCIIO_DMA_DATA);
		if (pci_dmamap == 0) {
			/* PCIbus DMA resources are temporarily unavailable */
			ASSERT(0);
			return(EAGAIN); 
		}

		buffer.pci_dmamap = pci_dmamap;

		NEW(pci_dmamap_item);
		pci_dmamap_item->pci_dmamap = pci_dmamap;
		pci_dmamap_item->next = 0;

		if (univ_state->dma_engine.pci_dmamaps == 0) {
			univ_state->dma_engine.pci_dmamaps = pci_dmamap_item;
			univ_state->dma_engine.pci_dmamaps->last = 
							     pci_dmamap_item;
		}
		else {
			univ_state->dma_engine.pci_dmamaps->last->next = 
							     pci_dmamap_item;
			univ_state->dma_engine.pci_dmamaps->last = 
							     pci_dmamap_item;
		}

		alenlist = uvaddr_to_alenlist(0, addr, byte_count, 0);
		ASSERT(alenlist);

		alenlist = pciio_dmamap_list(pci_dmamap, 
					     alenlist, 
					     0);
		ASSERT(alenlist);

		if (alenlist == 0) {
			/* PCIbus DMA resources are temporarily unavailable */
			return(EAGAIN);
		}

		alenlist_cursor_init(alenlist, 0, 0);  		

		/* Assemble the returning (pciaddr, len) pairs */
		buffer.num_of_items = 0;
		i = 0;
		while (alenlist_get(alenlist, 
				    NULL, 
				    0, 
				    (alenaddr_t *)&pciaddr, 
				    &byte_count, 
				    0) == ALENLIST_SUCCESS) {

			if (i >= UNIVERSE_DMA_ENGINE_BUF_MAX_ITEMS) {
				ASSERT(0);
				return(EFAULT);
			}

			buffer.alens[i].addr = (__uint32_t) pciaddr;
			buffer.alens[i].byte_count = byte_count;
#if VME_DEBUG
			printf("pciaddr 0x%x byte_count 0x%x\n",
			       pciaddr, byte_count);
#endif 
			buffer.num_of_items++;
			i++;
		}

		alenlist_done(alenlist);

		/* Fill the data for the user */
		if (copyout(&buffer, arg, sizeof(buffer))) {
			return(EFAULT);
		}
			
		break;

	case UDE_IOCTL_BUFTEAR:
		/* Set up the DMA resources for this range of the addr range */
		rv = copyin((void *)arg, (void *)&buffer, 
			    sizeof(universe_dma_engine_buffer_internal_t));
		if (rv != 0) {
			return(EFAULT);
		}

		pciio_dmamap_done(buffer.pci_dmamap);
		pciio_dmamap_free(buffer.pci_dmamap);

		pci_dmamap_item = univ_state->dma_engine.pci_dmamaps;
		pci_dmamap_prev = 0;
		while (pci_dmamap_item != 0) {
			if (pci_dmamap_item->pci_dmamap == buffer.pci_dmamap) {
				if (pci_dmamap_prev != 0) {
					pci_dmamap_prev->next = 
							pci_dmamap_item->next;
				}
				else {
					univ_state->dma_engine.pci_dmamaps =
							pci_dmamap_item->next;
				}
				DEL(pci_dmamap_item);
				break;
			}
			pci_dmamap_prev = pci_dmamap_item;
			pci_dmamap_item = pci_dmamap_item->next;
		}

		break;
	default:
		*rvalp = -1;
		return(EIO);
	}
	
	return(0);
}

/* ARGSUSED */
int 
ude_unmap(dev_t dev, vhandl_t *vt)
{
#if VME_DEBUG
	printf("ude_unmap\n");
#endif /* VME_DEBUG */

	return(0);
}

/* ARGSUSED */
int
ude_close(dev_t dev, int flag, int type, cred_t *crp)
{
	vertex_hdl_t dma_engine;
	ude_info_t info;
	universe_state_t univ_state;
	int s;
	universe_dma_engine_buffer_item_t     pci_dmamap_item;
	
	dma_engine = dev_to_vhdl(dev);
	info = ude_info_get(dma_engine);
	ASSERT(info != 0);
	univ_state = (universe_state_t) ude_info_provider_fastinfo_get(info);
	ASSERT(univ_state != 0);

	pci_dmamap_item = univ_state->dma_engine.pci_dmamaps;
	while (pci_dmamap_item != 0) {
		pciio_dmamap_done(pci_dmamap_item->pci_dmamap);
		pciio_dmamap_free(pci_dmamap_item->pci_dmamap);
		DEL(pci_dmamap_item);

		pci_dmamap_item = pci_dmamap_item->next;
	}
	univ_state->dma_engine.pci_dmamaps = 0;
		
	s = mutex_spinlock(&(univ_state->dma_engine.lock));
	ASSERT(univ_state->dma_engine.state == UDE_BUSY);
	univ_state->dma_engine.state = UDE_FREE;
	mutex_spinunlock(&(univ_state->dma_engine.lock), s);

	return(0);
}

/* 
 * Handling error occurred due to use of DMA engine
 */

int
ude_error_handler(void)
{
	return(0);
}

