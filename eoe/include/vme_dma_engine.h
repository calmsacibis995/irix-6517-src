/*
 * Copyright 1996, Silicon Graphics, Inc.
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

/* User interface for VME DMA engine */

#ifndef __VME_DMA_ENGINE_H__
#define __VME_DMA_ENGINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/vme/vmeio.h>

/* Opaque handle representing the DMA engine */
typedef struct vme_dma_engine_handle_s * vme_dma_engine_handle_t;

/* Opaque handle representing the DMA buffer */
typedef struct vme_dma_engine_buffer_s * vme_dma_engine_buffer_t;

/* Opaque handle representing the transfer object */
typedef struct vme_dma_engine_transfer_s * vme_dma_engine_transfer_t;

/* ----------------------------------------------------------------------
 * 
 * Acquire the DMA engine 
 *
 * ---------------------------------------------------------------------- */

/* Flag passed in vme_dma_engine_handle_alloc() */
#define VME_DMA_ENGINE_DEBUG       (0x0001)  

/* Return: handle of the DMA engine if success; 0 if failure */
extern vme_dma_engine_handle_t 
vme_dma_engine_handle_alloc(char *,         /* Hardware graph pathname */
			    unsigned int);  /* Flags                   */

/* ----------------------------------------------------------------------
 * 
 * Prepare DMA buffer
 *
 * ---------------------------------------------------------------------- */

/*
 * If the user gives a virtual address 0, we will create a 
 * virtual address range for the user.
 * Return: handle of the buffer if success; 0 if failure.
 */
extern vme_dma_engine_buffer_t
vme_dma_engine_buffer_alloc(vme_dma_engine_handle_t,
			    void *,  /* User process virtual address */
			    size_t); /* Length of the buffer */


/* 
 * Get the user virtual address out of the buffer handle
 */
extern void *
vme_dma_engine_buffer_vaddr_get(vme_dma_engine_buffer_t);

/* ----------------------------------------------------------------------
 * 
 * Allocate a transfer handle
 *
 * ---------------------------------------------------------------------- */


/* Users have to specify which way the DMA is going */
typedef enum {
	VME_DMA_ENGINE_DIR_READ,
	VME_DMA_ENGINE_DIR_WRITE
} vme_dma_engine_dir_t;

/* 
 * User can choose their preferred throttle value. However, the system
 * can decide what's used for the transfer given the software and
 * hardware constraint.  
 */
typedef enum {
	VME_DMA_ENGINE_THROTTLE_256,
	VME_DMA_ENGINE_THROTTLE_512,
	VME_DMA_ENGINE_THROTTLE_1024,
	VME_DMA_ENGINE_THROTTLE_2048,
	VME_DMA_ENGINE_THROTTLE_4096,
	VME_DMA_ENGINE_THROTTLE_8192,
	VME_DMA_ENGINE_THROTTLE_16384,
	VME_DMA_ENGINE_THROTTLE_DEFAULT
} vme_dma_engine_throttle_t;

/* 
 * User can choose to release the bus when done, or release the bus on
 * request. However, the system can decide what's used for the transfer given 
 * the software and hardware constraint.  
 */
typedef enum {
	VME_DMA_ENGINE_RELEASE_DONE,
	VME_DMA_ENGINE_RELEASE_DEMAND,
	VME_DMA_ENGINE_RELEASE_DEFAULT
} vme_dma_engine_release_t;
   
/* 
 * Note: it is user's responsibility to prepare the VME device.
 * Return: 0 if success, -1 if failure
 */
extern vme_dma_engine_transfer_t
vme_dma_engine_transfer_alloc(vme_dma_engine_handle_t,    /* Engine handle   */
			      vme_dma_engine_buffer_t,    /* DMA buffer      */
			      iopaddr_t,                  /* device address  */
			      size_t,                     /* Transfer size   */
			      vme_dma_engine_dir_t,       /* Xfer direction  */
			      vmeio_am_t,                 /* Addr modifier   */
			      vme_dma_engine_throttle_t,  /* Throttle size   */
			      vme_dma_engine_release_t);  /* Release mode    */


/* ----------------------------------------------------------------------
 * 
 * Schedule a transfer for future DMA 
 * 
 * ---------------------------------------------------------------------- */


/* 
 * User can schedule multiple transfers for one continuous DMA operation.
 * Return: 0 if success, -1 if failure.
 */
extern int 
vme_dma_engine_schedule(vme_dma_engine_handle_t,
			vme_dma_engine_transfer_t);

/* ----------------------------------------------------------------------
 * 
 * Present our schedule of transfers to the DMA enigne.
 * 
 * ---------------------------------------------------------------------- */

/* 
 * Users can choose to block here until the DMA is finished, or 
 * do something else in the meanwhile and come back later for the data.
 * if the user chooses to block here, he/she could also choose to wait by 
 * spinning or sleeping.
 */
typedef enum {
	VME_DMA_ENGINE_COMMIT_BLOCK,
	VME_DMA_ENGINE_COMMIT_NONBLOCK
} vme_dma_engine_commit_t;

/* 
 * In the Sync mode, users can choose to spin wait on the engine to service 
 * its request, or sleep wait on the engine to finish the service.
 * This is an advisory value.
 */
typedef enum {
	VME_DMA_ENGINE_WAIT_SPIN,
	VME_DMA_ENGINE_WAIT_SLEEP,
	VME_DMA_ENGINE_WAIT_DEFAULT
} vme_dma_engine_wait_t;

/* 
 * Return: 0 if success, -1 if failure
 */
extern int 
vme_dma_engine_commit(vme_dma_engine_handle_t,
		      vme_dma_engine_commit_t,
		      vme_dma_engine_wait_t);

/* 
 * Check to see if the DMA is finished.
 * Return: 1 if DMA transfer finishes, 0 if not
 */
extern int
vme_dma_engine_isdone(vme_dma_engine_handle_t);

/* 
 * If the asychronous mode is selected, wait on the DMA engine to finish 
 * the DMA for scheduled transfers.  
 * Return: 0 if success, -1 if failure
 */
extern int 
vme_dma_engine_rendezvous(vme_dma_engine_handle_t, 
			  vme_dma_engine_wait_t);

/* ----------------------------------------------------------------------
 * 
 * Resource deallocation
 * 
 * ---------------------------------------------------------------------- */

/*
 * Free the transfer handle 
 */
extern void
vme_dma_engine_transfer_free(vme_dma_engine_handle_t,
			     vme_dma_engine_transfer_t);


/* 
 * Tear down the buffer for DMA
 * Note: Since one buffer ties up the DMA resources in the system, it's 
 *       sometimes good to free up the DMA resources if we decide to free up 
 *       the resources and let other DMA get the resources
 */
extern void 
vme_dma_engine_buffer_free(vme_dma_engine_handle_t,
			   vme_dma_engine_buffer_t);

/*
 * Release the DMA engine
 */
extern void 
vme_dma_engine_handle_free(vme_dma_engine_handle_t);

#ifdef __cplusplus
}
#endif

#endif /* __VME_DMA_ENGINE_H__ */





