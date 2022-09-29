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

#ident "$Revision: 1.2 $"

/* Used by modules in the current directory */

#ifndef __LIB_VME_DMA_ENGINE_PRIVATE_H__
#define __LIB_VME_DMA_ENGINE_PRIVATE_H__

/* Debug flag */
extern int vme_dma_engine_debug;

struct vme_dma_engine_handle_s {
	struct vme_dma_engine_provider_s * provider;
	struct vme_dma_engine_handle_s *   real;
};

struct vme_dma_engine_buffer_s {
	void * addr;
	size_t byte_count;
};

typedef struct vme_dma_engine_provider_s {
	vme_dma_engine_handle_t   (*handle_alloc)(vme_dma_engine_handle_t,
						  char *);
	vme_dma_engine_buffer_t   (*buffer_alloc)(vme_dma_engine_handle_t,
						  void *,
						  size_t);
	vme_dma_engine_transfer_t (*transfer_alloc)(vme_dma_engine_handle_t,
						    vme_dma_engine_buffer_t,
						    iopaddr_t,
						    size_t,
						    vme_dma_engine_dir_t,
						    vmeio_am_t,
						    vme_dma_engine_throttle_t,
						    vme_dma_engine_release_t);
	int                       (*schedule)(vme_dma_engine_handle_t,
					      vme_dma_engine_transfer_t);
	int                       (*commit)(vme_dma_engine_handle_t,
					    vme_dma_engine_commit_t,
					    vme_dma_engine_wait_t);
	int                       (*isdone)(vme_dma_engine_handle_t);
	int                       (*rendezvous)(vme_dma_engine_handle_t,
						vme_dma_engine_wait_t);
	void                      (*transfer_free)(vme_dma_engine_handle_t,
						   vme_dma_engine_transfer_t);
	void                      (*buffer_free)(vme_dma_engine_handle_t,
						 vme_dma_engine_buffer_t);
	void                      (*handle_free)(vme_dma_engine_handle_t);
} vme_dma_engine_provider_t;

/* 
 * VME DMA service providers register their services right here 
 */

#define UNIVERSE_DMA_ENGINE_PROVIDER_INDEX        (0)

extern vme_dma_engine_handle_t 
universe_dma_engine_handle_alloc(vme_dma_engine_handle_t, char *);

extern vme_dma_engine_buffer_t
universe_dma_engine_buffer_alloc(vme_dma_engine_handle_t handle,
				 void * addr,
				 size_t byte_count);

extern vme_dma_engine_transfer_t
universe_dma_engine_transfer_alloc(vme_dma_engine_handle_t,
				   vme_dma_engine_buffer_t,
				   iopaddr_t,
				   size_t,
				   vme_dma_engine_dir_t,
				   vmeio_am_t,  
				   vme_dma_engine_throttle_t,
				   vme_dma_engine_release_t);

extern int 
universe_dma_engine_schedule(vme_dma_engine_handle_t handle,
			     vme_dma_engine_transfer_t transfer);
extern int
universe_dma_engine_commit(vme_dma_engine_handle_t handle,
			   vme_dma_engine_commit_t commit_mode,
			   vme_dma_engine_wait_t wait_mode);

extern int
universe_dma_engine_isdone(vme_dma_engine_handle_t handle);

extern int
universe_dma_engine_rendezvous(vme_dma_engine_handle_t handle,
			       vme_dma_engine_wait_t wait);


extern void
universe_dma_engine_transfer_free(vme_dma_engine_handle_t handle,
				  vme_dma_engine_transfer_t transfer);

extern void
universe_dma_engine_buffer_free(vme_dma_engine_handle_t handle,
					vme_dma_engine_buffer_t buffer);

extern void
universe_dma_engine_handle_free(vme_dma_engine_handle_t handle);

#endif /* __LIB_VME_DMA_ENGINE_PRIVATE_H__ */


