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

#ident "$Revision: 1.4 $"

/*
 * User-level DMA for the Universe chip
 * A trusted user for manipulating Universe hardware directly
 */

/* XXX Todo: 
 * -- Verbose(debugging) mode
 * -- error handling
 * -- Multiple users access the same linked list
 */

/* 
 * The linked list mode of the DMA engine is selected all the time.
 * 
 * Performance 
 *   The implementation is optimized based on the following 
 * assumptions: a. Successful DMA transfer is the common case; b. People 
 * would be using the same buffer for multiple DMA tranfers most of time.
 * Thus, DMA with the above characteristics would be processed most 
 * efficiently.
 */

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <assert.h>                    /* assert()                         */
#include <stdlib.h>                    /* malloc()                         */
#include <sys/types.h>
#include <sys/mman.h>                  /* mmap                             */
#include <sys/vme/vmeio.h>             /* VMEbus generic stuff             */
#include <sys/vme/universe.h>          /* universe related                 */
#include <vme_dma_engine.h>            /* User inferface of VME DMA engine */
#include "vme_dma_engine_private.h"    


/* Dynamic memory allocation */
#define NEW(ptr)          (ptr = malloc(sizeof(*(ptr))))
#define DEL(ptr)          (free(ptr))

/*
 * Software representation of linked list used by the DMA engine 
 */
typedef struct universe_dma_engine_linked_list_s {
	/* 
	 * Static part 
	 */
	/* Pointer to the very first packet */
	universe_dma_engine_linked_list_packet_t start_uvaddr;
	size_t                                   max_items;

	/* Floating part */
	/* Pointer to the first available packet */
	universe_dma_engine_linked_list_packet_t avail_uvaddr;  
	size_t                                   num_of_items; 
} * universe_dma_engine_linked_list_t;

/* 
 * Buffer: user/system created buffer in the host memory 
 * -- A buffer can have multiple system buffers to realize
 */
typedef struct universe_dma_engine_buffer_s {
	struct vme_dma_engine_buffer_s        generic_buffer;
#define        vd_addr     generic_buffer.addr      
#define        vd_byte_count    generic_buffer.byte_count 
	int				      system;
	/* One buffer is many buffers -- system's view */
	universe_dma_engine_buffer_internal_t internal; 
} * universe_dma_engine_buffer_t;

/* 
 * List of buffers: 
 * -- Bookkeeping for deallocation 
 */
typedef struct universe_dma_engine_buffer_item_s {
	universe_dma_engine_buffer_t               buffer;
	struct universe_dma_engine_buffer_item_s * next;
	struct universe_dma_engine_buffer_item_s * prev;
} * universe_dma_engine_buffer_item_t;	

typedef struct universe_dma_engine_buffer_list_s {
	struct universe_dma_engine_buffer_item_s * head;
	struct universe_dma_engine_buffer_item_s * tail;
} * universe_dma_engine_buffer_list_t;

/* 
 * Transfer: 
 * -- Multiple transfers can map to the same buffer
 */
typedef struct universe_dma_engine_transfer_s {
	vme_dma_engine_dir_t                 dir;
	universe_dma_engine_buffer_t         buffer;
	iopaddr_t                            vmeaddr;
	size_t                               byte_count;
	vmeio_am_t                           am;
	vme_dma_engine_throttle_t            throttle;
	vme_dma_engine_release_t             release;
} * universe_dma_engine_transfer_t;

/* 
 * List of transfers: 
 * -- Bookkeeping for deallocation 
 */
typedef struct universe_dma_engine_transfer_item_s {
	struct universe_dma_engine_transfer_s *      transfer;
	struct universe_dma_engine_transfer_item_s * next;
	struct universe_dma_engine_transfer_item_s * prev;
} * universe_dma_engine_transfer_item_t;	

typedef struct universe_dma_engine_transfer_list_s {
	struct universe_dma_engine_transfer_item_s * head;
	struct universe_dma_engine_transfer_item_s * tail;
	size_t                                       num_of_items;
} * universe_dma_engine_transfer_list_t;

/* Handle of the DMA engine */
typedef struct universe_dma_engine_handle_s {
	int                                         fd;
	char *					    regs_page;
	universe_dma_engine_regs_t *                regs;
	universe_dma_engine_linked_list_t           linked_list;
	universe_dma_engine_buffer_list_t           buffers;
	universe_dma_engine_transfer_list_t         transfers;
	vme_dma_engine_provider_t *                 provider;
} * universe_dma_engine_handle_t;

#define UNIVERSE_DMA_ENGINE_REGS_PAGE_SIZE 16384

universe_reg_t word_swap(universe_reg_t);
void dump_universe_regs(vme_dma_engine_handle_t);

/* 
 * Request the dma engine:
 * -- Mutual exclusion is done here to ensure that only one user can 
 *    use the DMA engine at one time.
 * -- The Universe registers related to the DMA engine are mapped into 
 *    user space. 
 * -- The Universe linked list memory is also mapped into the user space.
 * Return: handle of the DMA engine if success; 0 if failure
 */

/* ARGSUSED */
vme_dma_engine_handle_t
universe_dma_engine_handle_alloc(vme_dma_engine_handle_t generic_handle, 
				 char *                  pathname)
{
	int                                 fd;
	char *				    regs_page;
	universe_dma_engine_regs_t *        regs;
	universe_dma_engine_buffer_list_t   buffers;
	universe_dma_engine_transfer_list_t transfers;
	universe_dma_engine_linked_list_t   linked_list;
	universe_dma_engine_handle_t        handle;

	/* Acquire mutual exclusion on the DMA engine */
	fd = open(pathname, O_RDWR);
	if (fd == -1) {
		perror("open");
		return(0);
	}

	/* Map in the DMA engine registers. */
	regs_page = mmap(0, 
			 UNIVERSE_DMA_ENGINE_REGS_PAGE_SIZE,
			 PROT_READ | PROT_WRITE, 
			 MAP_PRIVATE, 
			 fd, 
			 UNIVERSE_DMA_ENGINE_REGS_OFFSET);
	if (regs_page == (char *) MAP_FAILED) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_handle_alloc: unable to map in DMA engine registers\n");
		}
		close(fd);
		return(0);
	}

	/*
	 * The registers we mapped in are actually a page full of Universe
	 * registers.  We have to adjust to the beginning of the 
	 * DMA engine control registers.
	 * It's really unfortunate that we have to map in additional 
	 * Universe registers not used in DMA engine operations just 
	 * because we have to map in device registers on page aligned 
	 * boundary.  It certainly creates a hole in security since 
	 * the developers of this SGI internal library could access other
	 * Universe registers.  Fortunately, customers don't get to play 
	 * the registers since the library takes care of glory details in
	 * manipulating the DMA engine.
	 */
	regs = (universe_dma_engine_regs_t *) (regs_page + UNIVERSE_DMA_CTL); 

	/* XXX The coding here isn't very straightforward */
	linked_list = 0;
	NEW(linked_list);
	if (linked_list == 0) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_handle_alloc: unable to malloc for the linked list\n");
		}
		munmap((void *) regs_page, 
		       UNIVERSE_DMA_ENGINE_REGS_PAGE_SIZE);

		close(fd);
	}
	linked_list->num_of_items = 0;

	/* 
	 * The kernel will already have a linked list area ready
	 * We just need map the linked list in.
	 */
	linked_list->start_uvaddr = 
		mmap(0, 
		     UNIVERSE_DMA_ENGINE_LINKED_LIST_SIZE,
		     PROT_READ | PROT_WRITE,
		     MAP_PRIVATE,
		     fd, 
		     UNIVERSE_DMA_ENGINE_LINKED_LIST_OFFSET);
	if (linked_list->start_uvaddr == 
	    (universe_dma_engine_linked_list_packet_t) MAP_FAILED) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_handle_alloc: unable to map in linked list area\n");
		}
		munmap((void *) regs_page, UNIVERSE_DMA_ENGINE_REGS_PAGE_SIZE);
		DEL(linked_list);
		linked_list = 0;
		close(fd);
		return(0);
	}
	linked_list->avail_uvaddr = linked_list->start_uvaddr;
	
	linked_list->max_items = UNIVERSE_DMA_ENGINE_LINKED_LIST_MAX_PACKETS;

	/* Initialize the buffer list */
	buffers = 0;
	NEW(buffers);
	if (buffers == 0) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_handle_alloc: unable to malloc buffer list\n");
		}
		munmap((void *) regs_page, UNIVERSE_DMA_ENGINE_REGS_PAGE_SIZE);
		munmap((void *) linked_list->start_uvaddr, 
		       UNIVERSE_DMA_ENGINE_LINKED_LIST_SIZE);
		DEL(linked_list);
		linked_list = 0;
		close(fd);
		return(0);
	}
	buffers->head = 0;
	buffers->tail = 0;

	/* Initialize the transfer list */
	transfers = 0;
	NEW(transfers);
	if (transfers == 0) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_handle_alloc: unable to malloc transfer list\n");
		}
		DEL(buffers);
		buffers = 0;
		munmap((void *) regs_page, UNIVERSE_DMA_ENGINE_REGS_PAGE_SIZE);
		munmap((void *) linked_list->start_uvaddr, 
		       UNIVERSE_DMA_ENGINE_LINKED_LIST_SIZE);
		DEL(linked_list);
		linked_list = 0;
		close(fd);
		return(0);
	}
	transfers->head = 0;
	transfers->tail = 0;
	transfers->num_of_items = 0;

	/* Assemble the handle */
	handle = 0;
	NEW(handle);
	if (handle == 0) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_handle_alloc: unable to malloc engine handle\n");
		}
		DEL(buffers);
		buffers = 0;
		DEL(transfers);
		transfers = 0;
		munmap((void *) regs_page, UNIVERSE_DMA_ENGINE_REGS_PAGE_SIZE);
		munmap((void *) linked_list->start_uvaddr, 
		       UNIVERSE_DMA_ENGINE_LINKED_LIST_SIZE);
		DEL(linked_list);
		linked_list = 0;
		close(fd);
		return(0);
	}
	handle->fd = fd;
	handle->regs_page = regs_page;
	handle->regs = regs;
	handle->linked_list = linked_list;
	handle->buffers = buffers;
	handle->transfers = transfers;

	return((vme_dma_engine_handle_t)handle);
}

/*
 * Prepare a buffer for future DMA transfer:
 * -- Pin down the pages for the DMA
 * -- Allocate DMA resources
 * Perf: It's not on the performance path since one buffer created here
 *       will be used for many DMA's.
 * Return: handle of the buffer if success; 0 if failure
 */
vme_dma_engine_buffer_t
universe_dma_engine_buffer_alloc(vme_dma_engine_handle_t generic_handle,
				 void *                  addr,
				 size_t                  byte_count)

{	
	universe_dma_engine_handle_t handle;
	universe_dma_engine_buffer_t buffer;
	universe_dma_engine_buffer_item_t buffer_item;
	universe_dma_engine_buffer_internal_t *internal;
	int rv;

	handle = (universe_dma_engine_handle_t) generic_handle;

	buffer = 0;
	NEW(buffer);
	if (buffer == 0) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_buffer_alloc: unable to malloc buffer handle\n");
		}
		return(0);
	}

	/* Get a user virtual address range if the user doesn't specify one */
	if (addr == 0) {
		addr = memalign(sysconf(_SC_PAGESIZE), byte_count + 4);
		if (addr == 0) {
			if (vme_dma_engine_debug) {
				printf("vme_dma_engine_buffer_alloc: unable to allocate buffer\n");
			}
			DEL(buffer);
			buffer = 0;
			return(0);
		}

		buffer->system = 1;
	}
	else
		buffer->system = 0;

	buffer->vd_addr = addr;
	buffer->vd_byte_count = byte_count;
	
	/* Tell the kernel to make the underlying pages non-swappable */
	rv = mpin(addr, byte_count);
	if (rv == -1) {
		if (vme_dma_engine_debug) {
			perror("mpin");
		}

		if (buffer->system) {
			free(addr);
		}
		DEL(buffer);
		buffer = 0;

		return(0);
	}

	/* 
	 * The call will normally return us a list of (pci address, byte_count)
	 * pairs.
	 */
	internal = &(buffer->internal); 

	internal->num_of_items = 1;

	/* XXX Deal with 64bit address */
	internal->alens[0].addr = (unsigned int) addr; 
	internal->alens[0].byte_count = byte_count;
	
	rv = ioctl(handle->fd, UDE_IOCTL_BUFPREP, internal);
	if (rv == -1) {
		if (vme_dma_engine_debug) {
			perror("ioctl - UDE_IOCTL_BUFPREP");
		}

		if (buffer->system) {
			free(addr);
		}
		munpin(addr, byte_count);
		DEL(buffer);
		buffer = 0;

		return(0);
	}

	/* Bookkeeping */
	buffer_item = 0;
	NEW(buffer_item);
	if (buffer_item == 0) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_buffer_alloc: unable to malloc buffer_item\n");
		}

		if (buffer->system) {
			free(addr);
		}
		munpin(addr, byte_count);
		DEL(buffer);
		buffer = 0;

		return(0);
	}
	buffer_item->buffer = buffer;
	buffer_item->prev = handle->buffers->tail;
	buffer_item->next = 0;
	if (handle->buffers->head == 0) {
		handle->buffers->head = buffer_item;
	}
	else {
		handle->buffers->tail->next = buffer_item;
	}
	handle->buffers->tail = buffer_item;

	return((vme_dma_engine_buffer_t)buffer);
}

/* 
 * Allocate a transfer object for DMA engine to process later.
 * Perf: This function is not on the performance path since it is assumed that 
 * the user will sepecify a buffer in the system and do a lot of DMA's on
 * that single buffer.
 * Note: a. This function is usually called multiple times on the same buffer 
 *       since one buffer can be used for different transfers; b. It is user's 
 *       responsibility to prepare the VME device.  
 * Return: handle of the transfer if success, 0 if failure
 */

vme_dma_engine_transfer_t
universe_dma_engine_transfer_alloc(vme_dma_engine_handle_t    generic_handle,
				   vme_dma_engine_buffer_t    buffer,
				   iopaddr_t                  vmeaddr,
				   size_t                     byte_count,
				   vme_dma_engine_dir_t       dir,
				   vmeio_am_t                 am, 
				   vme_dma_engine_throttle_t  throttle,
				   vme_dma_engine_release_t   release)
{
	universe_dma_engine_handle_t             handle;
	universe_dma_engine_transfer_t           transfer;
	universe_dma_engine_transfer_item_t      transfer_item;

	handle = (universe_dma_engine_handle_t) generic_handle;

	transfer = 0;
	NEW(transfer);
	if (transfer == 0) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_transfer_alloc: unable to malloc transfer handle\n");
		}
		return(0);
	}

	transfer->buffer = (universe_dma_engine_buffer_t) buffer;
	transfer->dir = dir;
	transfer->vmeaddr = vmeaddr;
	transfer->byte_count = byte_count;
	transfer->am = am;
	transfer->throttle = throttle;
	transfer->release = release;

	/* Bookkeeping */
	transfer_item = 0;
	NEW(transfer_item);
	if (transfer_item == 0) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_transfer_alloc: unable to malloc transfer_item\n");
		}
		DEL(transfer);
		transfer = 0;

		return(0);
	}
	transfer_item->transfer = transfer;
	transfer_item->prev = handle->transfers->tail;
	transfer_item->next = 0;
	if (handle->transfers->head == 0) {
		handle->transfers->head = transfer_item;
	}
	else {
		handle->transfers->tail->next = transfer_item;
	}
	handle->transfers->tail = transfer_item;
	handle->transfers->num_of_items++;

	return((vme_dma_engine_transfer_t) transfer);
}

/* 
 * Schedule a transfer for future DMA 
 * -- Build an internal list of all the transfers ready to be committed
 * Note: this call is necessary for multiple users to share the same 
 *       linked list in that a user can present a list of transfers 
 *       to the DMA engine to save forseeable overhead.
 * XXX Now, we return failure if the limit of the linked list is reached.
 * Return: 0 if success, -1 if failure.
 */
int 
universe_dma_engine_schedule(vme_dma_engine_handle_t   generic_handle,
			     vme_dma_engine_transfer_t generic_transfer)
{
	universe_dma_engine_handle_t         handle;
	universe_dma_engine_transfer_t       transfer;
	universe_dma_engine_linked_list_packet_t packet;
	size_t                                   total_length;
	int					 i;
	iopaddr_t                                vme_addr;
	register unsigned int am, *Pam;

	handle = (universe_dma_engine_handle_t) generic_handle;
	transfer = (universe_dma_engine_transfer_t) generic_transfer;

	/* Check if there's enough packets reserved for this transfer */
	if ((handle->linked_list->max_items - 
	     handle->linked_list->num_of_items) < 
	    transfer->buffer->internal.num_of_items) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_schedule: too many scheduled transfers or fragmented buffers\n");
		}

		return(-1);
	}

	/* Dump the transfer on the linked list memory */
	packet = handle->linked_list->avail_uvaddr;

	total_length = 0;
	i = 0;
	for (i = 0; i < transfer->buffer->internal.num_of_items; i++) {

		/* 
		 * Assemble one packet 
		 */

		/* It's fixed that only data is allowed */
		packet->ctl.pgm = UNIVERSE_DMA_CTL_PGM_DATA;

		/* PCI is 64-bit wide */
		packet->ctl.ld64en = UNIVERSE_DMA_CTL_LD64EN_ON; 

		switch(transfer->dir) {
		case VME_DMA_ENGINE_DIR_READ:
			packet->ctl.l2v = UNIVERSE_DMA_CTL_L2V_READ;
			break;
		case VME_DMA_ENGINE_DIR_WRITE:
			packet->ctl.l2v = UNIVERSE_DMA_CTL_L2V_WRITE;
			break;
		default:
			printf("vme_dma_engine_schedule: invalid direction\n");
			return(-1);
		}

		Pam = &transfer->am;
		am = *Pam;
		if (am & VMEIO_AM_A32) {
			packet->ctl.vas = UNIVERSE_DMA_CTL_VAS_A32;
		}
		else if (am & VMEIO_AM_A24) {
			packet->ctl.vas = UNIVERSE_DMA_CTL_VAS_A24;
		}
		else if (am & VMEIO_AM_A16) {
			packet->ctl.vas = UNIVERSE_DMA_CTL_VAS_A16;
		}
		else {
			printf("vme_dma_engine_schedule: invalid address modifier\n");
			return(-1);
		}

		if (am & VMEIO_AM_S) {
			packet->ctl.super = UNIVERSE_DMA_CTL_SUPER_S;
		}
		else if (am & VMEIO_AM_N) {
			packet->ctl.super = UNIVERSE_DMA_CTL_SUPER_N;
		}
		else {
			printf("vme_dma_engine_schedule: invalid address modifier\n");
			return(-1);
		}

		if (am & VMEIO_AM_D8) {
			if (am & VMEIO_AM_BLOCK) {
				printf("vme_dma_engine_schedule: invalid address modifier\n");
				return(-1);
			}

			packet->ctl.vdw = UNIVERSE_DMA_CTL_VDW_8;
		}
		else if (am & VMEIO_AM_D16) {
			packet->ctl.vdw = UNIVERSE_DMA_CTL_VDW_16;
		}
		else if (am & VMEIO_AM_D32) {
			packet->ctl.vdw = UNIVERSE_DMA_CTL_VDW_32;
		}
		else if (am & VMEIO_AM_D64) {
			packet->ctl.vdw = UNIVERSE_DMA_CTL_VDW_64;
		}
		else {
			/* The default is 8 bits */
			assert(packet->ctl.vdw == UNIVERSE_DMA_CTL_VDW_8);
		}

		if (am & VMEIO_AM_BLOCK) {
			packet->ctl.vct = UNIVERSE_DMA_CTL_VCT_BLOCK;
		}
		else if (am & VMEIO_AM_SINGLE) {
			packet->ctl.vct = UNIVERSE_DMA_CTL_VCT_SINGLE;
		}
		else {
			/* The default is signle cycles */
			assert(packet->ctl.vct == UNIVERSE_DMA_CTL_VCT_SINGLE);
		}

		vme_addr = transfer->vmeaddr + total_length;

		packet->tbc = word_swap(transfer->buffer->internal.alens[i].byte_count);

		/*
		 * It's safe to do the cast here since
		 * our Universe chip is a PCI 32bit address
		 * space device.  
		 */
		packet->pciaddr = (universe_dma_la_t)word_swap
			((universe_reg_t) (transfer->buffer->internal.alens[i].addr));
		packet->vmeaddr = (universe_dma_va_t)word_swap(vme_addr);

		/* Always clear the processed bit */
		packet->cpp.processed = UNIVERSE_DMA_CPP_PROCESSED_CLEAR; 

		packet->cpp.null = UNIVERSE_DMA_CPP_NULL_CONTINUE;

		total_length += transfer->buffer->internal.alens[i].byte_count;

		packet++;
	}

	if (transfer->byte_count > total_length) {
		printf("vme_dma_engine_schedule: transfer count exceeds buffer size\n");
		return(-1);
	}

	handle->linked_list->avail_uvaddr = packet;

	handle->linked_list->num_of_items += 
			transfer->buffer->internal.num_of_items;

	return(0);
}

/* 
 * Present our schedule of transfers to the DMA enigne:
 * -- Update the DMA engine linked list with the scheduled transfers
 * -- Start the DMA engine if the engine isn't started
 * In the Sync mode, we will spin until all of our scheduled DMA transfers
 * are finished; in the Async mode, we will leave the DMA engine to do its
 * work and continue on our own computation.  We will rendezvous with the
 * DMA engine when we need the data from the DMA transfer.

 * XXX We'll always spin wait, not sleep wait for the completion.
 * XXXXX byte count is not equal to buffer lenght BUG 
 * Performance: This routine sits on the performance path, 
 *              system calls should be avoided.
 * Return: 0 if successful, -1 if failing
 * Future Improvement: in the current implementation of the Sync mode,
 *       we spin until DMA 
 *       engine is done with all the transfers scheduled.  In the future 
 *       implementation, we would return from the spin if the particular
 *       transfer is finished, or all the transfers scheduled by the 
 *       current user is finished.
 */

/* ARGSUSED */
int
universe_dma_engine_commit(vme_dma_engine_handle_t generic_handle,
			   vme_dma_engine_commit_t commit_mode,
			   vme_dma_engine_wait_t   wait_mode)
{
	universe_dma_engine_regs_t *             regs;
	universe_dma_engine_handle_t             handle;
	universe_dma_engine_linked_list_packet_t last_packet;
	universe_dma_engine_transfer_t		 transfer;
	universe_dma_engine_transfer_item_t	 transfer_item;
	universe_dma_gcs_t			 gcs = {0};
	int                                      rv = 0, throt = 0;
	
	handle = (universe_dma_engine_handle_t) generic_handle;

	transfer_item = handle->transfers->head;
	transfer = transfer_item->transfer;

	regs = handle->regs;

	/* 
	 * Should be a lock right about here in case a transfer is in
	 * progress.
	 */
	while(regs->gcs.act)
		;

	switch(transfer->throttle) {
	case VME_DMA_ENGINE_THROTTLE_256:
		throt = 1;
		break;
	case VME_DMA_ENGINE_THROTTLE_512:
		throt = 2;
		break;
	case VME_DMA_ENGINE_THROTTLE_1024:
		throt = 3;
		break;
	case VME_DMA_ENGINE_THROTTLE_2048:
		throt = 4;
		break;
	case VME_DMA_ENGINE_THROTTLE_4096:
		throt = 5;
		break;
	case VME_DMA_ENGINE_THROTTLE_8192:
		throt = 6;
		break;
	case VME_DMA_ENGINE_THROTTLE_16384:
		throt = 7;
		break;
	case VME_DMA_ENGINE_THROTTLE_DEFAULT:
	default:
		throt = 0;
		break;
	}

	/* Terminate the linked list */
	last_packet = handle->linked_list->avail_uvaddr - 1; 

	/* XXX any race here ??? */

	/* 
	 * Start the DMA engine anyway no matter whether the DMA engine
	 * is done or not
	 * XXX coding
	 */
	if (handle->linked_list->num_of_items == 1) {
		register universe_reg_t *lctl = (universe_reg_t *)(&last_packet->ctl);
		register universe_reg_t *rctl = (universe_reg_t *)(&regs->ctl);

		*rctl = word_swap(*lctl);
		regs->tbc = word_swap(last_packet->tbc);
		regs->la = word_swap(last_packet->pciaddr);
		regs->va = word_swap(last_packet->vmeaddr);

		gcs.von = throt;
		gcs.go = UNIVERSE_DMA_GCS_GO_ENABLE; /* Kick the DMA engine */
		regs->gcs = gcs;
	}
	else {
		last_packet->cpp.null = UNIVERSE_DMA_CPP_NULL_FINAL;

		/* 
		 * Acquire the right to update 
		 */
		regs->llue.update = UNIVERSE_DMA_LLUE_UPDATE_ON;  
		for (;;) {
			/* Spin here until the update is proceedable */
			if (regs->llue.update == UNIVERSE_DMA_LLUE_UPDATE_ON) {
				break;
			}
		}
		regs->llue.update = UNIVERSE_DMA_LLUE_UPDATE_OFF; /* Let DMA go on */
	
		regs->cpp = word_swap(*((universe_reg_t *) handle->linked_list->start_uvaddr + 6)) & 0xfffffff8 - 0x20;
	
		gcs = regs->gcs;
		gcs.von = throt;
		gcs.done = UNIVERSE_DMA_GCS_DONE_CLEAR;  
		gcs.chain = UNIVERSE_DMA_GCS_CHAIN_LINKEDLIST;
		gcs.go = UNIVERSE_DMA_GCS_GO_ENABLE; /* Kick the DMA engine */
		regs->gcs = gcs;
	}

	if (commit_mode == VME_DMA_ENGINE_COMMIT_BLOCK) {
		/* Spin until the DMA engine is done */
		rv = universe_dma_engine_rendezvous(generic_handle, VME_DMA_ENGINE_WAIT_SPIN);
	}

       return(rv);
}

/* 
 * Check to see the DMA transfer is done.
 * Return: 0 if the DMA transfer is done; 1 if the DMA transfer is in progress.
 */
int
universe_dma_engine_isdone(vme_dma_engine_handle_t generic_handle)
{
	universe_dma_engine_handle_t handle;
	universe_dma_engine_regs_t * regs;
	universe_dma_engine_linked_list_t linked_list;
	int rv = 1;
	volatile universe_dma_gcs_t		 gcs;

	handle = (universe_dma_engine_handle_t) generic_handle;
	linked_list = handle->linked_list;

	regs = handle->regs;
	gcs = regs->gcs;
	
	if (gcs.lerr == UNIVERSE_DMA_GCS_LERR_ERROR) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_isdone:");
			printf(" PCIbus error\n");
		}

		gcs.lerr = UNIVERSE_DMA_GCS_LERR_CLEAR;
		regs->gcs = gcs;

		rv = -1;
	}

	if (gcs.verr == UNIVERSE_DMA_GCS_VERR_ERROR) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_isdone:");
			printf(" VMEbus error\n");
		}

		gcs.verr = UNIVERSE_DMA_GCS_VERR_CLEAR;
		regs->gcs = gcs;

		rv = -1;
	}

	if (gcs.perr == UNIVERSE_DMA_GCS_PERR_ERROR) {
		if (vme_dma_engine_debug) {
			printf("vme_dma_engine_isdone:");
			printf(" Protocol error\n");
		}

		gcs.perr = UNIVERSE_DMA_GCS_PERR_CLEAR;
		regs->gcs = gcs;

		rv = -1;
	}

	if (gcs.done == UNIVERSE_DMA_GCS_DONE_COMPLETED) {
		if (rv == 1)
			rv = 0;

		gcs.done = UNIVERSE_DMA_GCS_DONE_CLEAR;
		regs->gcs = gcs;

		/* All space in the linked list memory are free */
		linked_list->num_of_items = 0;
	}

	return(rv);
}

void
dump_universe_regs(vme_dma_engine_handle_t generic_handle)
{
	universe_dma_engine_handle_t handle;
	universe_dma_engine_regs_t * regs;
	universe_dma_engine_linked_list_packet_t packet;
	int i; 

	handle = (universe_dma_engine_handle_t) generic_handle;

	regs = handle->regs;

	printf("Universe GCS Register:\n");
	printf("\tgo = 0x%x\n", regs->gcs.go);
	printf("\tstop_req = 0x%x\n", regs->gcs.stop_req);
	printf("\thalt_req = 0x%x\n", regs->gcs.halt_req);
	printf("\tchain = 0x%x\n", regs->gcs.chain);
	printf("\tvon = 0x%x\n", regs->gcs.von);
	printf("\tvoff = 0x%x\n", regs->gcs.voff);
	printf("\tact = 0x%x\n", regs->gcs.act);
	printf("\tstop = 0x%x\n", regs->gcs.stop);
	printf("\thalt = 0x%x\n", regs->gcs.halt);
	printf("\tdone = 0x%x\n", regs->gcs.done);
	printf("\tlerr = 0x%x\n", regs->gcs.lerr);
	printf("\tverr = 0x%x\n", regs->gcs.verr);
	printf("\tperr = 0x%x\n", regs->gcs.perr);
	printf("\tint_stop = 0x%x\n", regs->gcs.int_stop);
	printf("\tint_halt = 0x%x\n", regs->gcs.int_halt);
	printf("\tint_done = 0x%x\n", regs->gcs.int_done);
	printf("\tint_lerr = 0x%x\n", regs->gcs.int_lerr);
	printf("\tint_verr = 0x%x\n", regs->gcs.int_verr);
	printf("\tint_m_err = 0x%x\n", regs->gcs.int_m_err);

	printf("Universe DMA Transfer Control Register:\n");
	printf("\tl2v = 0x%x\n", regs->ctl.l2v);
	printf("\tvdw = 0x%x\n", regs->ctl.vdw);
	printf("\tvas = 0x%x\n", regs->ctl.vas);
	printf("\tpgm = 0x%x\n", regs->ctl.pgm);
	printf("\tsuper = 0x%x\n", regs->ctl.super);
	printf("\tvct = 0x%x\n", regs->ctl.vct);
	printf("\tld64en = 0x%x\n", regs->ctl.ld64en);

	printf("DMA VMEbus Address Register:\n");
	printf("\t0x%x\n", regs->va);

	printf("DMA PCIbus Address Register:\n");
	printf("\t0x%x\n", regs->la);

	printf("DMA Transfer Byte Count Register:\n");
	printf("\t0x%x\n", regs->tbc);

	printf("DMA Command Packet Pointer:\n");
	printf("\t0x%x\n", regs->cpp);

	printf("Linked List:\n");
	printf("\tmax_items = %d\n", handle->linked_list->max_items);
	printf("\tnum_of_items = %d\n", handle->linked_list->num_of_items);

	packet = (universe_dma_engine_linked_list_packet_t) 
		handle->linked_list->start_uvaddr;

	for (i=0; i<handle->linked_list->num_of_items; i++) {
		printf("\tpacket = 0x%x\n", packet);
		printf("\t\tpciaddr = 0x%x\n", packet->pciaddr);
		printf("\t\tpciaddr = 0x%x\n", word_swap(packet->pciaddr));
		printf("\t\tvmeaddr = 0x%x\n", packet->vmeaddr);
		printf("\t\tvmeaddr = 0x%x\n", word_swap(packet->vmeaddr));
		printf("\t\ttbc = 0x%x\n", packet->tbc);
		printf("\t\ttbc = 0x%x\n", word_swap(packet->tbc));

		printf("\t\tctl.l2v = 0x%x\n", packet->ctl.l2v);
		printf("\t\tctl.vdw = 0x%x\n", packet->ctl.vdw);
		printf("\t\tctl.vas = 0x%x\n", packet->ctl.vas);
		printf("\t\tctl.pgm = 0x%x\n", packet->ctl.pgm);
		printf("\t\tctl.super = 0x%x\n", packet->ctl.super);
		printf("\t\tctl.vct = 0x%x\n", packet->ctl.vct);
		printf("\t\tctl.ld64en = 0x%x\n", packet->ctl.ld64en);

		printf("\t\tcpp.processed = 0x%x\n", packet->cpp.processed);
		printf("\t\tcpp.null = 0x%x\n\n", packet->cpp.null);

		packet++;
	}
}


/* 
 * Wait on the DMA engine service finish the DMA transfer.
 * Provide the user a way to rendezvous with the DMA service
 * if the async mode was selected.
 * Note: In the current implementation of the async mode, we'll spin 
 *       Until the DMA engine is finished with all the packets in the
 *       linked list.  Later, we would typically return from the spin
 *       if all our transfers are done.  More over, if the user specifies
 *       a specific transfer, we'll return if that transfer is done.
 *       XXX We'll always spin wait, not sleep wait for the completion.
 * Returns 0 when transfer has completed normally, -1 on failure.
 */

/* ARGSUSED */
int
universe_dma_engine_rendezvous(vme_dma_engine_handle_t generic_handle,
			       vme_dma_engine_wait_t   wait)
{
	int rv, count = 0;
	universe_dma_engine_handle_t        handle;

	handle = (universe_dma_engine_handle_t) generic_handle;

	/* Spin until the DMA engine is done */
	while ((rv = universe_dma_engine_isdone(generic_handle)) == 1)
		count++;

	/*
	 * Reset the linked list
	 */
	handle->linked_list->avail_uvaddr = handle->linked_list->start_uvaddr;

	return(rv);
}

/*
 * Free the transfer handle 
 * -- Free the transfer handle
 * -- Free the transfer item from the linked list
 * Note: This function isn't necessary to be called since all the transfer
 *       handles will be freed at the engine handle free time.
 * Return: 0 if success, -1 if failure
 * XXX Not MT-safe, need a lock on update the list if MT-safeness is needed.
 */

void
universe_dma_engine_transfer_free(vme_dma_engine_handle_t   generic_handle,
				  vme_dma_engine_transfer_t generic_transfer)
{
	universe_dma_engine_handle_t        handle;
	universe_dma_engine_transfer_t      transfer;
	universe_dma_engine_transfer_item_t transfer_item;

	handle = (universe_dma_engine_handle_t) generic_handle;
	transfer = (universe_dma_engine_transfer_t) generic_transfer;

	for (transfer_item = handle->transfers->head; 
	     transfer_item != 0; 
	     transfer_item = transfer_item->next) {
		if (transfer_item->transfer == transfer) {
			if (handle->transfers->num_of_items == 1) {
				handle->transfers->head = 0;
				handle->transfers->tail = 0;
			}
			else {
				if (transfer_item == handle->transfers->head) {
					handle->transfers->head = 
						transfer_item->next;
				}
				else {
					transfer_item->prev->next = 
						transfer_item->next;
				}
				if (transfer_item == handle->transfers->tail) {
					handle->transfers->tail = 
						transfer_item->prev;
				}
				else {
					transfer_item->next->prev = 
						transfer_item->prev;
				}
			}

			handle->transfers->num_of_items--;
			break;
		}
	}

	DEL(transfer_item->transfer);
	transfer_item->transfer = 0;
	DEL(transfer_item);
	transfer_item = 0;

	/*
	 * Reset the linked list
	 */
	handle->linked_list->avail_uvaddr = handle->linked_list->start_uvaddr;
}

/* 
 * Tear down the buffer for DMA
 * Perf: This function isn't on the performance path, two system calls are
 *       used.
 * Note: Since one buffer ties up the DMA resources in the system, it's 
 *       sometimes good to free up the DMA resources if we decide to free up 
 *       the resources and let other DMA get the resources.
 *       This function isn't necessary to be called since all the buffers 
 *       will be torn down at the engine free time.
 * Return: 0 if success, -1 if failure
 * XXX Not MT-safe, need a lock on updating the list if MT-safeness is needed.
 */

void
universe_dma_engine_buffer_free(vme_dma_engine_handle_t generic_handle,
				vme_dma_engine_buffer_t generic_buffer)
{
	universe_dma_engine_handle_t      handle;
	universe_dma_engine_buffer_t      buffer;
	universe_dma_engine_buffer_item_t buffer_item;
	int                               rv;

	handle = (universe_dma_engine_handle_t) generic_handle;
	buffer = (universe_dma_engine_buffer_t) generic_buffer;

	rv = ioctl(handle->fd, UDE_IOCTL_BUFTEAR, &buffer->internal);
	if (rv == -1) {
		if (vme_dma_engine_debug) {
			perror("ioctl - UDE_IOCTL_BUFTEAR\n");
		}
	}

	rv = munpin(buffer->vd_addr, buffer->vd_byte_count);
	if (rv == -1) {
		if (vme_dma_engine_debug) {
			perror("munpin\n");
		}
	}
	
	if (buffer->system) {
		free(buffer->vd_addr);
	}

	for (buffer_item = handle->buffers->head; 
	     buffer_item != 0; 
	     buffer_item = buffer_item->next) {
		if (buffer_item->buffer == buffer) {
			if (handle->buffers->head == 
			    handle->buffers->tail) {
				/* One-item case */
				handle->buffers->head = 0;
				handle->buffers->tail = 0;
			}
			else {
				if (buffer_item == handle->buffers->head) {
					handle->buffers->head = 
						buffer_item->next;
				}
				else {
					buffer_item->prev->next = 
						buffer_item->next;
				}
				if (buffer_item == handle->buffers->tail) {
					handle->buffers->tail = 
						buffer_item->prev;
				}
				else {
					buffer_item->next->prev = 
						buffer_item->prev;
				}
			}
			break;

		}
	}

	DEL(buffer_item->buffer);
	buffer_item->buffer = 0;
	DEL(buffer_item);	
	buffer_item = 0;
}

/* 
 * Release the DMA engine:
 * -- Tear down all the dangling transfers
 * -- Tear down all the dangling buffers
 * -- Unmap the DMA engine registers
 * -- Unmap the DMA linked list area
 * -- Relinquish the mutual exclusion on the DMA engine
 * Perf: This function isn't on the performance path, at least two system 
 *       calls are made.
 * XXX Has to deal with the case that DMA engine is still active,
 *     and scheduled transfers have been finished
 * Return: 0 if success, -1 if failure
 */
void
universe_dma_engine_handle_free(vme_dma_engine_handle_t generic_handle)
{
	universe_dma_engine_handle_t        handle;
	universe_dma_engine_transfer_item_t transfer_item;
	universe_dma_engine_transfer_item_t transfer_item_current;
	universe_dma_engine_buffer_item_t   buffer_item;
	universe_dma_engine_buffer_item_t   buffer_item_current;
	int                                 rv;

	handle = (universe_dma_engine_handle_t) generic_handle;

	/* Tear down all the transfers one by one */
	transfer_item = handle->transfers->head;
	while (transfer_item != 0) {
		transfer_item_current = transfer_item;
		transfer_item = transfer_item->next;
		DEL(transfer_item_current);
		transfer_item_current = 0;
	}
	DEL(handle->transfers);
	handle->transfers = 0;


	/* Tear down all the remaing buffers one by one */
	buffer_item = handle->buffers->head;
	while (buffer_item != 0) {
		buffer_item_current = buffer_item;
		buffer_item = buffer_item->next;
		DEL(buffer_item_current);
		transfer_item_current = 0;
	}
	DEL(handle->buffers);
	handle->buffers = 0;
		
	rv = munmap((void *)handle->regs_page, 
		    UNIVERSE_DMA_ENGINE_REGS_PAGE_SIZE);  

	if (vme_dma_engine_debug) {
		if (rv == -1) {
			printf("vme_dma_engine_handle_free: regs munmap failed\n");
		}
	}
	rv = munmap((void *)handle->linked_list->start_uvaddr, 
		    UNIVERSE_DMA_ENGINE_LINKED_LIST_SIZE); /* Unmap linked 
							      list */
	if (vme_dma_engine_debug) {
		if (rv == -1) {
			printf("vme_dma_engine_handle_free: start_uvaddr munmap failed\n");
		}
	}
	DEL(handle->linked_list);
	handle->linked_list = 0;

	close(handle->fd);  /* Release the mutual exclusion on the engine */

	DEL(handle); 
	handle = 0;
}


/* -----------------------------------------------------------------------
   
                  utility function -- byte-swap

   ----------------------------------------------------------------------- */

universe_reg_t 
word_swap(universe_reg_t value)
{
	universe_reg_t  ret;
	register unsigned char   byte0, byte1, byte2, byte3;
	
	ret = 0;
	
	byte0 = value & 0xff;
	byte1 = (value & 0xff00) >> 8;
	byte2 = (value & 0xff0000) >> 16;
	byte3 = (value & 0xff000000) >> 24;
	
	ret = (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;

	return(ret);
}
