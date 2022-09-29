/*
 * File: partition.h
 * Purpose: Maps partition structures.
 *
 * Copyright 1995, Silicon Graphics, Inc.
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

#ifndef	_KSYS_PARTITION_H
#define	_KSYS_PARTITION_H 1

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/partition.h>
#include <sys/param.h>

#include <ksys/xthread.h>

#include <sys/SN/klpartvars.h>


typedef enum part_error_type {
    PART_ERR_NONE,			/* 0 - no error */
    PART_ERR_RD_TIMEOUT,		/* read timeout */
    PART_ERR_NACK,			/* Nack error on page */
    PART_ERR_WB,			/* Write back error */
    PART_ERR_RD_DIR_ERR,		/* read directory entry */
    PART_ERR_POISON,			/* poison access */
    PART_ERR_ACCESS,			/* eccess vioation */
    PART_ERR_UNKNOWN,			/* unknown error */
    PART_ERR_PARTIAL,			/* partial read/write error */
    PART_ERR_DEACTIVATE			/* partition deactivation */
} part_error_t;

typedef enum part_error_result {
    PART_ERROR_RESULT_NONE,		/* No error */
    PART_ERROR_RESULT_HANDLED,		/* Error recovered from */
    PART_ERROR_RESULT_USER,		/* Deal with as normal */
    PART_ERROR_RESULT_FATAL		/* PANIC */
} part_error_result_t;

typedef	enum {
    pAccessInvalid = 0, 
    pAccessNone = 1, 
    pAccessRW = 2
} pAccess_t;

typedef	part_error_result_t (*part_error_handler_t)(eframe_t *, paddr_t, 
						    part_error_t);

/*
 * Function: 	part_id
 * Purpose:	Return partition ID of calling partition.
 * Parameters:	nothing
 * Returns:	partid.
 */
extern partid_t part_id(void);

/*
 * Function: 	part_from_addr
 * Purpose:	Determine the owning partition of a memory address.
 * Parameters:	pa - physical address to check.
 * Returns:	partid_t of partition owning address. INVALID_PARTID 
 *		returned if no one owns address.
 */
extern partid_t part_from_addr(paddr_t pa);

/*
 * Function: 	part_from_region
 * Purpose:	To locate a partition based on a region #.
 * Parameters:	region - locate the partition that contains 
 *			 the specified region.
 * Returns:	Partition ID.
 */
extern partid_t	part_from_region(__uint32_t);

/*
 * Function: 	part_page_free
 * Purpose:	To free up a page of memory previously allocated by 
 *		partPageAllocateNode
 * Parameters:	pfd - pfd for page being freed (returned by partPageAllocNode)
 *		size- page size as passed to partPageAllocNode.
 * Returns:	Nothing
 */
extern	void	part_page_free(pfd_t *, size_t);

/*
 * Function: 	part_page_allocate_node
 * Purpose:	To allocate a page of memory for cross partition mapping.
 * Parameters:	cnodeid - compact nodeid indicating where to allocate memory
 *			  from.
 *		partid  - partition id that is allowed access to this page.
 *		ck	- cache key (same as pagealloc_size)
 *		flags	- (passed to pagealloc_size).
 *		size	- size of page requested (same as pagealloc_size).
 *		size- page size as passed to partPageAllocNode.
 * Returns:	Nothing
 */
extern 	pfd_t	*part_page_allocate_node(cnodeid_t, partid_t, pAccess_t, 
					 uint,int,size_t,part_error_handler_t);
/*
 * Function: 	part_page_allocate
 * Purpose:	To allocate a page of memory for cross partition mapping.
 * Parameters:	partid  - partition id that is allowed access to this page.
 *		ck	- cache key (same as pagealloc_size)
 *		flags	- (passed to pagealloc_size).
 *		size	- size of page requested (same as pagealloc_size).
 *		size- page size as passed to partPageAllocNode.
 * Returns:	Nothing
 */
extern 	pfd_t	*part_page_allocate(partid_t, pAccess_t, uint, int, size_t,
				    part_error_handler_t);

/*
 * Function: 	part_permit_page
 * Purpose:	To set the partition accesses to a page. 
 * Parameters:	partid	- partition id to set access for.
 *		pfd 	- pointer to PFD for page
 *		a	- access requested (pAccess_t).
 * Returns:	Nothing.
 */
extern	void	part_permit_page(partid_t partid, pfd_t *pfd, pAccess_t a);

/*
 * Function:	part_get_nasid
 * Purpose:	Find a node in a partition.
 * Parameters:	partid	- partition ID of partition to locate a node in.
 *		nasid   - starting nasid, updated to nasid selected on
 *			  return (May be NULL). If non-null, usually
 *			  '0' is passed on the first call. If return
 *			  value is NULL - this value is undefined.
 * Returns: 	Pointer to pn_t or NULL if failed.
 */
extern	pn_t	*part_get_nasid(const partid_t partid, nasid_t *nasid);

/*
 * Function: 	part_get_promid
 * Purpose:	Read determine the "proms" concept of the local 
 *		partition number.
 * Parameters:	None.
 * Returns:	partid.
 */
extern partid_t	part_get_promid(void);

/*
 * Function:	part_poison_page
 * Purpose: 	Posion the specified page. 
 * Parameters:	pfd - pointer to pfd for page.
 * Returns:	Nothing.
 */
extern	void	part_poison_page(pfd_t *pfd);

/*
 * Function:	part_register_page
 * Purpose:	Register a page that is being mapped from another partition.
 * Parameters:	a - physicall address of base of page. 	
 *		s - size of page
 *		eh - pointer to error handler for page.
 *
 * Returns:	0 - failed, !0 - success.
 */
extern void 	*part_register_page(paddr_t, size_t, part_error_handler_t);

/*
 * Function:	part_unregister_page
 * Purpose:	Un-register a pgae previously registered using 
 *		partRegisterPage.
 * Parameters:	a - physical address of base of page (MUST BE IDENTICAL 
 *		    to address passed to partRegisterPage).
 * Returns:	Nothing.
 */
extern void 	part_unregister_page(paddr_t);

/*
 * Function: 	part_interrupt_set
 * Purpose:	Set an interrupt handler for a partition handler.
 * Parameters:	partid_t - partition to set handler for.
 *		int - If > 0, interrupt is threaded, and # threads allocated 
 *		      and "ready" at any time does not exceed this value.
 *		function - to call on interrupt
 *			partid_t - partition # of interrupting partition.
 *			int      - # idle threads
 * Returns:	Nothing.
 */
extern void	part_interrupt_set(partid_t, int, void (*)(void *, int));

/*
 * Function: 	part_interrupt_id
 * Purpose:	Determine the current interrupt id (count) for the specified
 *		partition.
 * Parameters:	p - partition id to locate count for.
 * Returns:	Current count (interrupt id).
 */
extern __uint64_t part_interrupt_id(partid_t p);

/*
 * Function:	part_start_thread
 * Purpose:	Start another interrupt thread for the specified partition.
 * Parameters:	partid - partition id to start interrupt tread for.
 *		c      - # of threads to start
 *		w      - (wait) if B_TRUE, sleep waiting to start, B_FALSE, 
 *			 fail if memory not available right now.
 * Returns:	B_TRUE - started 
 *		B_FALSE - failed to start another thread.
 */
extern boolean_t part_start_thread(partid_t partid, int c, boolean_t w);


/*
 * Function: 	part_register_handler
 * Purpose:	Register a partition "activate" and "deactivate"
 *		handler.
 * Parameters:	ar - pointer to routine to call when a partition
 *		     is "activated.
 *		dr - pointer to routine to call when a partition is
 *		     deactivated.
 * Returns:	Nothing
 * Notes: 	Routines are called in the order in which they are 
 *		registered. 
 *
 * 		Any "active" partitions when an activate routine is
 *		registered will cause a call out  to the newly 
 *		registered routine.
 */
extern	void	part_register_handlers(void (*ar)(partid_t), 
				       void (*dr)(partid_t));

/*
 * Function: 	part_deactivate
 * Purpose:	Remove a partitions region present mappings and terminate
 *		the interrupt thread.
 * Parameters:	partid - partition number being deactivated.
 * Returns:	nothing
 */
extern	void	part_deactivate(partid_t partid);

/*
 * Function:	part_operation
 * Purpose:	Performs tasks available at user level via SYSSGI.
 * Parameters:	op - SYSSGI_PARTOP to perform.
 *		a1,a2,a3 - arguments
 *		rvp - pointer to return value.
 * Returns:	0 for success, errno for failure.
 */
extern	int	part_operation(int op, int a1, void *a2, 
			       void *a3, rval_t *rvp);

/*
 * Function: 	part_export_page
 * Purpose:	Export page to another cell by dropping the firewall
 * Parameters:	paddr - physical addr
 *		size - number of bytes to export (multiple of NBPP)
 *		part  - partition to export to
 * Returns:	0 = page exported, 1 = page already exported
 */
extern	int 	part_export_page(paddr_t, size_t, partid_t);

/*
 * Function: 	part_unexport_page
 * Purpose:	Unexport pages to another cell by raising the firewalls
 * Parameters:	paddr - physical addr
 *		size - number of bytes to export (multiple of NBPP)
 *		part  - partition to export to
 *		clean_cache - clean cache before raising firewall
 * Returns:	0 = page no longer exported to ANY cell, 1 = page still
 *		exported to other cells
 */
extern	int 	part_unexport_page(paddr_t, size_t, partid_t, int);

/*
 * Function: 	part_set_var
 * Purpose:	Set a cross partition variable.
 * Parameters:	vid - variable ID to set
 *		v   - value to set variable to.
 * Returns:     Nothing.
 * Notes:	Cross partition variables are stored uncached in a poisoned
 *		region, and can only be accesses uncached.
 */
extern	void	part_set_var(int vid, __psunsigned_t v);

/*
 * Function: 	part_get_var
 * Purpose:	Retrieve a remote partitions cross partition parameter.
 * Parameters:	pid - remote partition id to retrieve variable from.
 *		vid - variable id to retrieve
 *		v   - address of value to fill in with variables value.
 * Returns:    	0 - success
 *		!0 - failed.
 */
extern	int	part_get_var(partid_t, int, __psunsigned_t *);

/*
 * Function:   	part_heart_beater
 * Purpose:	Perform local heart beat. 
 * Parameters:	None
 * Returns:	Nothing
 */
extern  void	part_heart_beater(void);

/*
 * Function: 	nasid_to_partid
 * Purpose:	Convert a nasid to the partition that contains the node
 * Parameters:	nasid
 * Returns:	partid
 */
extern partid_t nasid_to_partid(nasid_t nasid);

#ifdef CELL_IRIX
/*
 * Function:   	part_get_clusterid
 * Purpose:	Gets clusterid to which the partition belongs
 * Parameters:	partid
 * Returns:	clusterid
 */
extern  clusterid_t	part_get_clusterid(partid_t);

/*
 * Function:   	part_get_domainid
 * Purpose:	Gets domainid to which the partition belongs
 * Parameters:	partid
 * Returns:	domainid
 */
extern  domainid_t	part_get_domainid(partid_t);
#endif /* CELL_IRIX */
#endif /* _KSYS_PARTITION_H */
