/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef _SYS_MAPPED_KERNEL_H
#define _SYS_MAPPED_KERNEL_H

#include "sys/sbd.h"
#include "sys/mips_addrspace.h"

#ident  "$Revision: 1.8 $"

/*
 * Definitions for mapped kernels.
 */

#ifdef MAPPED_KERNEL
/* Amount of K2 space used. */
#define MAPPED_KERN_PAGE_SIZE   0x1000000               /* 1 16M page */
#define MAPPED_KERN_SIZE        (2 * MAPPED_KERN_PAGE_SIZE)
/* Start of mapped kernel K2 space */
#define MAPPED_KERN_RO_BASE     K2BASE
#define MAPPED_KERN_RW_BASE     K2BASE + MAPPED_KERN_PAGE_SIZE
/* Mapped kernel page size information */
#define MAPPED_KERN_PAGEMASK    0xffffff
#define MAPPED_KERN_TLBMASK     TLBPGMASK_16M

#else /* !MAPPED_KERNEL */

#define MAPPED_KERN_SIZE	0

#endif /* MAPPED_KERNEL */

/* macros for mapped kernel text and data */
#ifdef MAPPED_KERNEL
/* Decode direct-mapped kernel text and data by picking the right node. */
/* When we replicate text, the KTEXT and KRWDATA macros will be different. */
/* XXX - Right now, we're assuming the rw data is on NASID 0 */
#define IS_MAPPED_KERN_RO(x)		(((__psunsigned_t)(x) >=	\
						MAPPED_KERN_RO_BASE) && \
					 ((__psunsigned_t)(x) <		\
						MAPPED_KERN_RO_BASE +	\
					        MAPPED_KERN_PAGE_SIZE))
#define IS_MAPPED_KERN_RW(x)		(((__psunsigned_t)(x) >=	\
						MAPPED_KERN_RW_BASE) && \
					 ((__psunsigned_t)(x) <		\
						MAPPED_KERN_RW_BASE +	\
					        MAPPED_KERN_PAGE_SIZE)) 

#ifdef SN0

#ifdef _STANDALONE

/* Use 0 as the base for memory in stand-alone mode.  Stand-alone code
 * Figures out the NASIDs for itself.
 */
#define MAPPED_KERN_RO_PHYSBASE(_cnode)	0
#define MAPPED_KERN_RW_PHYSBASE(_cnode) 0

#else /* !_STANDALONE */
#define MAPPED_KERN_RO_PHYSBASE(_cnode)	(NODEPDA(_cnode)->kern_vars.kv_ro_baseaddr)
#define MAPPED_KERN_RW_PHYSBASE(_cnode)	(NODEPDA(_cnode)->kern_vars.kv_rw_baseaddr)
#endif /* _STANDALONE */

#define MAPPED_KERN_RO_TO_PHYS(x)					\
	(((__psunsigned_t)KDM_TO_PHYS(x) & MAPPED_KERN_PAGEMASK)	\
	 | MAPPED_KERN_RO_PHYSBASE(cnodeid()))
#define MAPPED_KERN_RW_TO_PHYS(x)					\
	(((__psunsigned_t)KDM_TO_PHYS(x) & MAPPED_KERN_PAGEMASK)	\
	| MAPPED_KERN_RW_PHYSBASE(cnodeid()))

#define EARLY_MAPPED_KERN_RW_TO_PHYS(x)					\
	(((__psunsigned_t)KDM_TO_PHYS(x) & MAPPED_KERN_PAGEMASK)	\
	| NODE_CAC_BASE(get_nasid()))
#else /* !SN0 */

#if defined(IP19) || defined(IP25)
/* These defines are necessary for EVMK builds where we want to operate
 * multiple-kernels within the same EVEREST system for test purposes.
 */

#ifdef MULTIKERNEL

#define MAPPED_KERN_RO_PHYSBASE(cellid)	ctob(BLOCs_to_pages(EVCFGINFO->ecfg_cell[cellid].cell_membase))
#define MAPPED_KERN_RW_PHYSBASE(cellid)	ctob(BLOCs_to_pages(EVCFGINFO->ecfg_cell[cellid].cell_membase))
#define MAPPED_KERN_RO_TO_PHYS(x)					\
	(((__psunsigned_t)KDM_TO_PHYS(x) & MAPPED_KERN_PAGEMASK)	\
	 | MAPPED_KERN_RO_PHYSBASE(evmk_cellid))
#define MAPPED_KERN_RW_TO_PHYS(x)					\
	(((__psunsigned_t)KDM_TO_PHYS(x) & MAPPED_KERN_PAGEMASK)	\
	 | MAPPED_KERN_RW_PHYSBASE(evmk_cellid))

#else /* !MULTIKERNEL */

#define MAPPED_KERN_RO_PHYSBASE(cellid)	0
#define MAPPED_KERN_RW_PHYSBASE(cellid)	0
#define MAPPED_KERN_RO_TO_PHYS(x)	KDM_TO_PHYS(x)
#define MAPPED_KERN_RW_TO_PHYS(x)	KDM_TO_PHYS(x)

#endif /* !MULTIKERNEL */
#define	EARLY_MAPPED_KERN_RW_TO_PHYS(x)	MAPPED_KERN_RW_TO_PHYS(x)

#endif /* IP19 || IP25 */
#endif /* SN0 */

#else /* !MAPPED_KERNEL */

/* In unmapped kernels, text and data are in K0 space */
#define IS_MAPPED_KERN_RO(x)		0
#define IS_MAPPED_KERN_RW(x)		0
#define MAPPED_KERN_RO_TO_PHYS(x)	KDM_TO_PHYS(x)
#define MAPPED_KERN_RW_TO_PHYS(x)	KDM_TO_PHYS(x)
#define	EARLY_MAPPED_KERN_RW_TO_PHYS(x)	MAPPED_KERN_RW_TO_PHYS(x)
#endif

#define IS_MAPPED_KERN_SPACE(x)		(IS_MAPPED_KERN_RO(x) || \
					 IS_MAPPED_KERN_RW(x))
#define	MAPPED_KERN_RO_TO_K0(x)		PHYS_TO_K0(MAPPED_KERN_RO_TO_PHYS(x))
#define MAPPED_KERN_RW_TO_K0(x)		PHYS_TO_K0(MAPPED_KERN_RW_TO_PHYS(x))
#define	MAPPED_KERN_RO_TO_K1(x)		PHYS_TO_K1(MAPPED_KERN_RO_TO_PHYS(x))
#define MAPPED_KERN_RW_TO_K1(x)		PHYS_TO_K1(MAPPED_KERN_RW_TO_PHYS(x))

#endif /* _SYS_MAPPED_KERNEL_H */

