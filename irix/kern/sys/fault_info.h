/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1992, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_FAULT_INFO_H__
#define __SYS_FAULT_INFO_H__
#ident	"$Revision: 1.10 $"

#include "sys/types.h"
#include "sys/pfdat.h"
#include "sys/immu.h"

#define	FLT_PGALLOC_DONE	0x1	/* A pagealloc was tried */
#define	FLT_PFIND		0x2	/* Pfind done for this fault entry */
#define	FLT_DONE		0x4	/* The fault is completed succesfully */
#define	FLT_SOFT_VALID		0x8	/* The pte's software bit is valid */
#define	FLT_DO_FILE_IO		0x10	/* Force a file I/O */

#define	MAX_LOCAL_FAULT_ENTRIES	4	/* Assume the popular large page size 
					 * will be 4 times larger than min.
					 * page size.
					 */
typedef	struct	fault_info {
	pfd_t		*pfd;		/* Pfdat for the address if found */
	pde_t		*pd;		/* Page table entry for the address */
	void		*anon_handle;	
	sm_swaphandle_t swap_handle;
	int		flags;
} fault_info_t;

/*
 * Fault info contains enough information to fault a base page. Fault data
 * contains a list of fault info structures, one for each base page of 
 * a large page. Thus there will be 4 entries for a 64k page assume NBPP=16k.
 * This structure is allocated on the stack of the vfault() routine. For
 * small sized pages (4*NBPP) all fault info structures are allocated on the
 * stack for performance reasons. This is the fd_small_list array.
 * For very large pages the fault() routine allocates the fault_info array 
 * dynamically. This array is kept in fd_large_list.
 */
typedef	struct	{
	fault_info_t	fd_small_list[MAX_LOCAL_FAULT_ENTRIES];
	fault_info_t	*fd_large_list;
	int		fd_flags;
} faultdata_t;

/*
 * fault_info_flags definition.
 */
#define	FD_PAGE_WAIT	1	/* Wait for page to be allocated */
#define	FD_GET_PAGESIZE	2	/* Get page size from PM  or pte */

/*
 * Recover from large page fault. The do while clause catches unsuspecting
 * else's that may follow the calls to the macro.
 */
#define	LPAGE_FAULT_RECOVER(pas, flt_infop, vaddr, npgs) \
		do { \
			if (large_page_fault) \
				lpage_fault_recover(pas, flt_infop, vaddr, npgs);\
		} while (0)
/*
 * Return values for lpage_vfault
 */

#define	USE_BASE_PAGE			0	/* Use base page size */
#define	USE_LARGE_PAGE			1	/* Use current page size */
#define	RETRY_FAULT			2	/* Retry the fault with same */
						/* page size */
#define	USE_BASE_PAGE_SKIP_DOWNGRADE	3	/* Use base page size but skip*/
						/* the downgrade operation */
#define	RETRY_LOWER_PAGE_SIZE		4	/* Try next lower page size */

#define	TRUE		1
#define	FALSE		0

/*
 * Retry codes for retry variable in vfault.
 */

#define	NO_RETRY		0
#define	LARGE_PAGE_FAULT_RETRY	1
#define	BASE_PAGE_FAULT_RETRY	2

/* IP22 has a space crunch - debug kernel needs to fit in 7.25M. */
#if defined(DEBUG) && !defined(IP22) && !defined(SN0XXL)
#define	VFAULT_TRACE_ENABLE	1
#endif

#ifdef	VFAULT_TRACE_ENABLE

#define	VFAULT_WAIT_SENTRY	1
#define	VFAULT_FAULT		2
#define	VFAULT_REFBIT		3
#define	VFAULT_ANON		4
#define	VFAULT_SWAPIN		5
#define	VFAULT_DFILL		6
#define	VFAULT_VNODE		7
#define	VFAULT_LPAGE_TLBDROPIN	8
#define	VFAULT_NBPP_TLBDROPIN	9
#define	FLT_FAIL_HDR_VALID	10
#define	FLT_FAIL_SENTRY		11
#define	FLT_FAIL_PFD		12
#define	FLT_SUCC_PFD		13
#define	FLT_FAIL_VNODE		14
#define	FLT_FAIL_BREAK		15
#define	FLT_SUCC_NEWPFD		16
#define	PFAULT_FAULT		17
#define	FLT_AUTORESERVE		18
#define	FLT_COW_FAULT_FAIL	19
#define	FLT_NON_COW		20
#define	FLT_PAGE_HOLE		21
#define	FLT_ANON_PFIND		22
#define	VHAND_TOSS_PAGE		23
#define	PMP_DGRD_LPG_BUDDY	24
#define	PMP_DGRD_LPG_PTE	25
#define	PMP_ADDR_BOUNDARY	26
#define	FLT_BUDDY_SMALL		27
#define	VFAULT_LPG_FAILURE	28
#define	PMP_DGRD_LPG_PTE_NOFLUSH 29
#define	FLT_UTLBMISS_SWTCH	30
#define	FLT_SET_UTLBMISSWTCH	31
#define	FLT_FAIL_PDONE		32
#define	FLT_LPAGE_SWAPIN	33
#define	FLT_FAIL_PFD_ALIGN	34
#define	FLT_FAIL_NBPP		35
#define	FAULT_LPG_RECOVER	36

#define	VFAULT_TRACE(event, p1, p2, p3, p4) if ( large_page_fault)  \
					vfault_trace(event, (ulong)p1, \
					(ulong) p2, (ulong)p3, (ulong)p4)

#define	LPG_FLT_TRACE(event, p1, p2, p3, p4) vfault_trace(event, (ulong)p1, \
					(ulong) p2, (ulong)p3, (ulong)p4)

extern	void	vfault_trace(int, ulong, ulong, ulong, ulong);

#else
#define	VFAULT_TRACE(event, p1, p2, p3, p4) 
#define	LPG_FLT_TRACE(event, p1, p2, p3, p4) 
#endif /* VFAULT_TRACE_ENABLE */

#endif /* __SYS_FAULT_INFO_H__ */
