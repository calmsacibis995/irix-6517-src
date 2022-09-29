/*
 * Copyright 1996 - 1997, Silicon Graphics, Inc.
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

#ident "$Revision: 1.3 $"

/* Generic User-level DMA engine */

#include <stdio.h>                       /* printf */
#include <stdlib.h>                      /* malloc */
#include <sys/types.h>
#include <sys/vme/vmeio.h>
#include <vme_dma_engine.h>
#include "vme_dma_engine_private.h"

#define VME_DMA_ENGINE_DEBUG_ON    (1)
#define VME_DMA_ENGINE_DEBUG_OFF   (0)

int vme_dma_engine_debug;

/* Dynamic memory allocation */
#define NEW(ptr)          (ptr = malloc(sizeof(*(ptr))))
#define DEL(ptr)          (free(ptr))

/* 
 * List of 
 */
vme_dma_engine_provider_t vme_dma_engine_providers[] = {
	{
		universe_dma_engine_handle_alloc,
		universe_dma_engine_buffer_alloc,
		universe_dma_engine_transfer_alloc,
		universe_dma_engine_schedule,
		universe_dma_engine_commit,
		universe_dma_engine_isdone,
		universe_dma_engine_rendezvous,
		universe_dma_engine_transfer_free,
		universe_dma_engine_buffer_free,
		universe_dma_engine_handle_free
	}
};


/* 
 * Allocate a VME DMA engine handle
 */
vme_dma_engine_handle_t
vme_dma_engine_handle_alloc(char *pathname, unsigned int flags)
{
	int                     provider_index;
	vme_dma_engine_handle_t handle;

	if (flags & VME_DMA_ENGINE_DEBUG) {
		vme_dma_engine_debug = VME_DMA_ENGINE_DEBUG_ON;
	}

	/* XXX */
	provider_index = UNIVERSE_DMA_ENGINE_PROVIDER_INDEX;

	/* Get to know who's the service provider */
	handle = 0;
	NEW(handle);
	if (handle == 0) {
		return(0);
	}
	handle->provider = 
		&vme_dma_engine_providers[provider_index];
	
	/* Let the particular service provider do the dirty work */
	if ((handle->real = (handle->provider->handle_alloc) (handle->real,
							 pathname)) == 0) {
		DEL(handle);
		return(0);
	}

	return(handle);
	/*
	return((vme_dma_engine_handle_t) 
	       (handle->provider->handle_alloc)(handle, pathname));
	       */
}

vme_dma_engine_buffer_t
vme_dma_engine_buffer_alloc(vme_dma_engine_handle_t handle,
			    void * addr,
			    size_t byte_count)
{
	return((vme_dma_engine_buffer_t)
	       (handle->provider->buffer_alloc)(handle->real, 
						addr, 
						byte_count));
	       /* universe_dma_engine_buffer_alloc(handle, addr, byte_count)); */
}

vme_dma_engine_transfer_t
vme_dma_engine_transfer_alloc(vme_dma_engine_handle_t handle,
			      vme_dma_engine_buffer_t buffer,
			      iopaddr_t vmeaddr,   
			      size_t byte_count,   
			      vme_dma_engine_dir_t dir,
			      vmeio_am_t am, 
			      vme_dma_engine_throttle_t throttle,
			      vme_dma_engine_release_t release)
{
	return((vme_dma_engine_transfer_t)
	       (handle->provider->transfer_alloc)(handle->real,
						  buffer, vmeaddr,
						  byte_count, dir,
						  am,
						  throttle, release));
	/*
	       universe_dma_engine_transfer_alloc(handle, buffer, vmeaddr,
						  byte_count, dir, am,
						  throttle, release)
						  );
						  */
}
						 
int
vme_dma_engine_schedule(vme_dma_engine_handle_t handle,
			vme_dma_engine_transfer_t transfer)
{
	return((handle->provider->schedule)(handle->real, transfer)); 
	/* return(universe_dma_engine_schedule(handle, transfer)); */
}

int
vme_dma_engine_commit(vme_dma_engine_handle_t handle,
		      vme_dma_engine_commit_t commit_mode,
		      vme_dma_engine_wait_t wait_mode)
{

	return((handle->provider->commit)(handle->real, 
					  commit_mode,
					  wait_mode));
	/* return(universe_dma_engine_commit(handle, commit_mode, wait_mode)); */
}

int 
vme_dma_engine_isdone(vme_dma_engine_handle_t handle)
{

	return((handle->provider->isdone)(handle->real));

	/* return(universe_dma_engine_isdone(handle)); */
}
		    

int
vme_dma_engine_rendezvous(vme_dma_engine_handle_t handle,
			  vme_dma_engine_wait_t wait)
{
	return((handle->provider->rendezvous)(handle->real, wait));

	/* return(universe_dma_engine_rendezvous(handle, wait)); */
}

void
vme_dma_engine_transfer_free(vme_dma_engine_handle_t handle,
			     vme_dma_engine_transfer_t transfer)
{
	(handle->provider->transfer_free)(handle->real, transfer);
	/* universe_dma_engine_transfer_free(handle, transfer); */
}

void
vme_dma_engine_buffer_free(vme_dma_engine_handle_t handle,
			   vme_dma_engine_buffer_t buffer)
{
	/*
	universe_dma_engine_buffer_free(handle, buffer);
	*/
	(handle->provider->buffer_free)(handle->real, buffer);
}

void
vme_dma_engine_handle_free(vme_dma_engine_handle_t handle)
{
	/*
	universe_dma_engine_handle_free(handle->real);
	*/
	(handle->provider->handle_free)(handle->real);
	DEL(handle);
	handle = 0; 
}

void *
vme_dma_engine_buffer_vaddr_get(vme_dma_engine_buffer_t buffer)
{
	return(buffer->addr);
}










