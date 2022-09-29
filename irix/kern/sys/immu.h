/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1997, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#ifndef __SYS_IMMU_H__
#define __SYS_IMMU_H__

#ident	"$Revision: 3.290 $"

#include <sys/sbd.h>
#include <sys/mips_addrspace.h>
#include <sys/mapped_kernel.h>
#if IP32
#include <sys/cpu.h>
#endif

#if !defined(_LANGUAGE_ASSEMBLY)
#include <sys/types.h>
#include <sys/atomic_ops.h>
#endif

#define	MIN_NBPP	4096		/* smallest acceptable page size */
#define MIN_PNUMSHFT	12		/* Shift for page number from addr. */
#define MIN_POFFMASK	(MIN_NBPP - 1)	/* Mask for offset into page. */

#define	IO_NBPP		4096		/* Number of bytes per page (for DMA) */
#define IO_PNUMSHFT	12		/* Shift for page number from addr. */
#define IO_POFFMASK	(IO_NBPP - 1)	/* Mask for offset into page. */

#if _PAGESZ == 4096
#define PNUMSHFT	12		/* Shift for page number from addr. */
#endif
#if _PAGESZ == 16384
#define PNUMSHFT	14		/* Shift for page number from addr. */
#endif

#define	NBPP		_PAGESZ		/* Number of bytes per page (virtual) */
#define	PGFCTR		(NBPP / MIN_NBPP)
#define	PGSHFTFCTR	(PNUMSHFT - MIN_PNUMSHFT)

#if !LANGUAGE_ASSEMBLY

/*
 * This structure is used to define the sizes of various constants that
 * are shared between kernel and administrative utilities.
 */
struct pageconst {
	int		p_pagesz;	/* _PAGESZ */
	int		p_pnumshft;	/* PNUMSHFT */
	int		p_nbpp;		/* NBPP */
};

#if _KERNEL
#include <sys/sema.h>
#endif

/*
 * Page Descriptor (Table) Entry Definitions for MIPS CPU.
 */

/*
 * For the R4000 IP19/IP20/IP22
 *       <-             hard bits               ->
 *  +---+-------------------------------+--+-+-+-+
 *  |   |              pfn		|cc|m|v|g|
 *  +---+-------------------------------+--+-+-+-+
 *    2                  24		  3 1 1 1
 *
 * For the IP19(R4000A)/MIPS-II only two address bits are used.
 * As above, we left shift the HW bits so that SW bits can be
 * cleared with a single srl, this time of 4 bits.
 *
 *  +-------------------------+--+-+-+-+--+---+--+
 *  |		 pfn          |cc|m|v|g|nr|d/w|sv|
 *  +-------------------------+--+-+-+-+--+---+--+
 *     	    23			3 1 1 1  1  1   1
 *
 * For the IP21/MIPS-III
 *
 * #ifdef TFP_PTE64
 *
 *  <- soft bits -> <-    hard bits               <-soft
 *  +--------------+----------------------+--+-+-+--+-------+
 *  |              |		pfn	  |cc|m|v|  |nr|d|sv|
 *  +--------------+----------------------+--+-+-+--+--+-+--+
 *        24			28	    3 1 1  2  3 1  1 
 * #else
 *
 *     <-    hard bits            <-soft
 *  +-+-------------------+--+-+-+--+-+--+
 *  |0|		pfn	  |cc|m|v|nr|d|sv|
 *  +-+-------------------+--+-+-+--+-+--+
 *   1 		23	    3 1 1  1 1  1 
 * #endif
 *
 * For the IP26/TFP (hard bits the same as R4000 for VDMA engine compat).
 *      soft   <-       hard bits            -> soft
 *  +-+--+--+-+-+-----------------------+--+-+-+-+
 *  |0|sv|nr|0|0|      pfn		|cc|m|v|d|
 *  +-+--+--+-+-+-----------------------+--+-+-+-+
 *   1 1  3  1 1         19		  3 1 1 1
 *
 * For R10000:
 *
 *      soft <-       hard bits            -> soft
 *  +--+-+--+--------------------------+--+-+-+-+
 *  |nr|d|sv|          pfn             |cc|m|v|g|
 *  +--+-+--+--------------------------+--+-+-+-+
 *   1  1 1            23               3  1 1 1
 *
 *
 * For Beast:
 *
 *    soft <-         hard bits            -> soft
 *  +--+-+--+----+---------------------+--+-+-+-+---+
 *  |nr|d|sv|hint|       pfn           |cc|m|v|g|psz|
 *  +--+-+--+----+---------------------+--+-+-+-+---+
 *   1  1 1   4           32            3  1 1 1  4
 *
 */

/*
 * Hardware bits:
 *
 *  pfn	- Page frame number
 *  n	- if set is non-cacheable
 *  m	- modified bit if clear a write will cause fault
 *  vr	- Valid and referenced bit.  If clear a fault will occur.
 *	  Used as reference bit also because there is no HW reference bit.
 *  g	- Global so ignore pid in translations
 *  cc  - (R4000) Cache coherency algorithm.
 *
 * Software bits:
 *
 *  d/w - page is dirty (with respect to pte's mapping),
 *	  or process wants wakeup after read of (virtual) page.
 *  sv	- software valid bit set if page is valid
 *  nr  - Reference count. Set to all ones on reference.
 *	  Decremented (not below zero) when page is aged by vhand.
 *        Pages with lower nr will be taken by getpages.
 */

#if NBPP == MIN_NBPP
#define	PTE_PFN(num)	pte_pfn:num
#define PFNWIDTH	PFNDWIDTH
#define PFNSHIFT	0
#elif !defined(_KERNEL)
#define	PTE_PFN(num)	pte_pfn:num
#define PFNWIDTH	PFNDWIDTH
#define PFNSHIFT	0
#else
#define	PTE_PFN(num)	 pte_pfn:num-PGSHFTFCTR, \
				:PGSHFTFCTR
#define PFNWIDTH	(PFNDWIDTH-PGSHFTFCTR)
#define PFNSHIFT	PGSHFTFCTR
#endif

#if R4000 || IP28 || IP32
#if defined(IP20) || defined(IP22) || defined(IP28) || defined(IPMHSIM)
typedef struct pte {
        uint    pte_dirty : 1,  /* SW: page dirty bit */
                pte_sv : 1,     /* SW: page valid bit */
		pte_pfnlock:1,  /* SW: pte lock bit */
                pte_nr : 2,     /* SW: page reference count */
                pte_eop : 1,    /* SW: end-of-page bug */
                PTE_PFN(20),    /* HW: core page frame number or 0 */
                pte_cc:3,       /* HW: coherency algorithm for this page */
                pte_m:1,        /* HW: modified (dirty) bit */
                pte_vr:1,       /* HW: valid bit (& SW referenced)*/
                pte_g:1;        /* HW: ignore pid bit */
} pte_t;
#define PFNDWIDTH	20
#define PFNOFFSET	6
#define	PG_PFNLOCK	0x20000000
#define NPFNMASK        0x03FFFFC0
#define NPFNSHIFT       (6 + PGSHFTFCTR)
#define NRMASK          0x18000000
#define NRSHIFT         27
#define CCMASK          0x00000038
#define CCSHIFT         3
#define	PG_SHOTDN	0
#endif	/* IP20 || IP22 || IP28 */
#if defined(IP32)
typedef struct pte {
        uint    pte_dirty : 1,  /* SW: page dirty bit */
                pte_sv : 1,     /* SW: page valid bit */
                pte_nr : 1,     /* SW: page reference count */
                pte_pfnlock : 1,/* SW: pte lock bit */
                PTE_PFN(22),    /* HW: core page frame number or 0 */
                pte_cc:3,       /* HW: coherency algorithm for this page */
                pte_m:1,        /* HW: modified (dirty) bit */
                pte_vr:1,       /* HW: valid bit (& SW referenced)*/
                pte_g:1;        /* HW: ignore pid bit */
} pte_t;
#define PFNDWIDTH       22
#define PFNOFFSET       4
#define	PG_PFNLOCK	0x10000000
#define NPFNMASK        0x0FFFFFC0
#define NPFNSHIFT       (6 + PGSHFTFCTR)
#define NRMASK          0x20000000
#define NRSHIFT         29
#define CCMASK          0x00000038
#define CCSHIFT         3
#define	PG_SHOTDN	0
#endif	/* IP32 */
#if IP19
#if	!defined(PTE_64BIT)
typedef struct pte {
	uint	PTE_PFN(23),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_g : 1,	/* HW: ignore pid bit */
		pte_nr : 1,	/* SW: page reference count */
#define PFNDWIDTH	23
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1;	/* SW: page valid bit */
} pte_t;
#define	PG_SHOTDN	0
#define	PG_PFNLOCK	0
#define PFNOFFSET	0

#else /* !defined(PTE_64BIT) */

/* PTE structure for machines requiring 64 bits */

#if defined(NUMA_BASE) /* IP19 && PTE_64BIT */

/*
 * To support migration we need to add a bitlock protecting accesses
 * and update to the pfn field. In addition, all pte accesses and updates
 * must be done atomically. Doc
 */

typedef struct pte {
	__uint64_t
		pte_uncattr: 2, /* HW: PTE Uncached attributes */
                pte_pfnlock: 1, /* SW: pfn access/update lock. CHECK PG_PFNLOCK! */	
		/* pte_shotdn field: Sync its position with PG_SHOTDN macro */
		pte_shotdn: 1,	/* SW: Page with this pte got shot down */ 
                pte_pgmaskshift: 4, 	/* SW: pfn page size field */	
		pte_rmap_index:16,	/* SW: rmap index field */
		pte_numa_home: 8,	/* SW: Numa policy mgmt */
		PTE_PFN(23),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_g : 1,	/* HW: ignore pid bit */
		pte_nr : 1,	/* SW: page reference count */
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1;	/* SW: page valid bit */
}pte_t;


#else /* NUM_BASE */

typedef struct pte {
	__uint64_t	
		pte_uncattr:2,	/* HW: PTE Uncached attributes */
                pte_pfnlock: 1, /* SW: pfn access/update lock. CHECK PG_PFNLOCK! */
                pte_pgmaskshift: 4, 	/* SW: pfn page size field */	
		pte_rmap_index:16,	/* SW: rmap index field */
		pte_reserved: 9,	/* SW: Reserved bits in PTE	*/
		PTE_PFN(23),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_g : 1,	/* HW: ignore pid bit */
		pte_nr : 1,	/* SW: page reference count */
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1;	/* SW: page valid bit */
}pte_t;

#endif /* NUMA_BASE */

#define PFNDWIDTH	23
#define	PFNOFFSET	0
#define	UNCATTRMASK	0xC000000000000000
#define	UNCATTRSHIFT	62
#define NPFNMASK        0x00000000FFFFFE00
#define NPFNSHIFT       (9+PGSHFTFCTR)
#define NRMASK          0x0000000000000004
#define NRSHIFT         2
#define CCMASK          0x00000000000001C0
#define CCSHIFT         6
#define RESVDMASK       0x0FFFFFFF00000000

/*
 * Shift and mask values to get the page size info. from the pte.
 * Used in the utlbmiss handler.
 */
#ifdef	NUMA_BASE
#define	PAGE_MASK_INDEX_MASK	0x0F00000000000000
#define	PAGE_MASK_INDEX_SHIFT	56
#define	PTE_RMAP_INDEX_SHIFT	40
#define	PTE_RMAP_INDEX_MASK	0x00FFFF0000000000
#define PG_PFNLOCK      	0x2000000000000000 /* pfn bitlock position */
#define	PG_SHOTDN		0x1000000000000000 /* page for this pte got shot down */
#else
#define	PAGE_MASK_INDEX_MASK	0x3C00000000000000
#define	PAGE_MASK_INDEX_SHIFT	57
#define	PTE_RMAP_INDEX_SHIFT	41
#define	PTE_RMAP_INDEX_MASK	0x01FFFE0000000000
#define PG_PFNLOCK      	0x2000000000000000 /* pfn bitlock position */
#define	PG_SHOTDN		0
#endif


#endif	/* !defined(PTE_64BIT) */
#endif	/* IP19 */

#endif	/* R4000 */

#if R10000  && !IP28 && !IP32
#if	!defined(PTE_64BIT)
#ifdef	IP30
typedef struct pte {
	__uint32_t
		pte_nr : 1,	/* SW: page reference count */
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1,	/* SW: page valid bit */
                pte_pfnlock: 1, /* SW: pfn access/update lock */
		PTE_PFN(22),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_g : 1;	/* HW: ignore pid bit */
} pte_t;
#define	PG_PFNLOCK	0x10000000	
#define NPFNMASK        0x0FFFFFC0
#define NPFNSHIFT       (6 + PGSHFTFCTR)
#define NRMASK          0x80000000
#define NRSHIFT         31
#define CCMASK          0x00000038
#define CCSHIFT         3
#define	PG_SHOTDN	0
#define PFNDWIDTH	23
#define PFNOFFSET	3
#else 	/* IP30 */
#if IPMHSIM && ! R4000
typedef struct pte {
	uint	pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1,	/* SW: page valid bit */
		pte_nr : 3,	/* SW: page reference count */
		pte_eop : 1,	/* SW: end-of-page bug */
		PTE_PFN(20),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced) */
		pte_g : 1;	/* HW: ignore pid bit */
} pte_t;
#define PFNDWIDTH	20
#define PFNOFFSET	6
#else	/* IPMHSIM && ! R4000 */
typedef struct pte {
	__uint32_t
		pte_nr : 1,	/* SW: page reference count */
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1,	/* SW: page valid bit */
		PTE_PFN(23),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_g : 1;	/* HW: ignore pid bit */
} pte_t;
#define PFNDWIDTH	23
#define PFNOFFSET	3
#endif /* IPMHSIM && ! R4000 */
#define	PG_PFNLOCK	0
#define	PG_SHOTDN	0
#endif /* IP30 */

#else /* !defined (PTE_64BIT) */

/* pte_t Structure for systems with 64 bit ptes */

#if defined(NUMA_BASE)   /*R10000 && PTE_64BIT*/

/*
 * To support migration we need to add a bitlock protecting accesses
 * and update to the pfn field. In addition, all pte accesses and updates
 * must be done atomically. Doc
 */

#if defined(SN0XXL)

typedef struct pte {
	__uint64_t
		pte_uncattr: 2,	/* HW: PTE Uncached attributes    */
                pte_pfnlock: 1, /* SW: pfn access/update lock. CHECK PG_PFNLOCK! */
		/* pte_shotdn field: Sync its position with PG_SHOTDN macro */
		pte_shotdn: 1,	/* SW: Page with this pte got shot down. */
                pte_pgmaskshift: 4, 	/* SW: pfn page size field */	
		pte_rmap_index:16,	/* SW: rmap index field */
		pte_numa_home: 3,	/* SW: Numa policy mgmt */
		pte_nr : 1,	/* SW: page reference count */
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1,	/* SW: page valid bit */
		PTE_PFN(28),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_g : 1;	/* HW: ignore pid bit */
} pte_t;

#else /* SN0XXL */


typedef struct pte {
	__uint64_t
		pte_uncattr: 2,	/* HW: PTE Uncached attributes    */
                pte_pfnlock: 1, /* SW: pfn access/update lock. CHECK PG_PFNLOCK! */
		/* pte_shotdn field: Sync its position with PG_SHOTDN macro */
		pte_shotdn: 1,	/* SW: Page with this pte got shot down. */
                pte_pgmaskshift: 4, 	/* SW: pfn page size field */	
		pte_rmap_index:16,	/* SW: rmap index field */
		pte_numa_home: 5,	/* SW: Numa policy mgmt */
		pte_nr : 1,	/* SW: page reference count */
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1,	/* SW: page valid bit */
		PTE_PFN(26),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_g : 1;	/* HW: ignore pid bit */
} pte_t;

#endif /* SN0XXL */

#else /* NUMA_BASE */

typedef struct pte {
	__uint64_t
		pte_uncattr: 2,	/* HW: PTE uncached attributes */
                pte_pfnlock: 1, /* SW: pfn access/update lock. CHECK PG_PFNLOCK! */
                pte_pgmaskshift: 4, 	/* SW: pfn page size field */	
		pte_rmap_index:16,	/* SW: rmap index field */
		pte_reserved: 6,	/* Unused bits at this time */
		pte_nr : 1,	/* SW: page reference count */
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1,	/* SW: page valid bit */
		PTE_PFN(26),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_g : 1;	/* HW: ignore pid bit */
} pte_t;

#endif /* NUMA_BASE */

#if defined (SN0XXL) 

#define PFNDWIDTH	28
#define NPFNMASK        0x00000003FFFFFFC0
#define NRMASK          0x0000001000000000
#define NRSHIFT         36
#define RESVDMASK       0x0FFFFFE000000000

#else /* SN0XXL */

#define PFNDWIDTH	26
#define NPFNMASK        0x00000000FFFFFFC0
#define NRMASK          0x0000000400000000
#define NRSHIFT         34
#define RESVDMASK       0x0FFFFFF800000000

#endif /* SN0XXL */

#define PFNOFFSET	3
#define UNCATTRMASK	0xC000000000000000
#define UNCATTRSHIFT	62
#define NPFNSHIFT       (6+PGSHFTFCTR)
#define CCMASK          0x0000000000000038
#define CCSHIFT         3

/*
 * Shift and mask values to get the page size info. from the pte.
 * Used in the utlbmiss handler.
 */
#ifdef	NUMA_BASE
#define	PAGE_MASK_INDEX_MASK	0x0F00000000000000
#define	PAGE_MASK_INDEX_SHIFT	56
#define	PTE_RMAP_INDEX_SHIFT	40
#define PG_PFNLOCK      	0x2000000000000000 /* pfn bitlock position */
#define	PTE_RMAP_INDEX_MASK	0x00FFFF0000000000
#define	PG_SHOTDN		0x1000000000000000 /* page for this pte got shot down */
#else
#define	PAGE_MASK_INDEX_MASK	0x1E00000000000000
#define	PAGE_MASK_INDEX_SHIFT	57
#define	PTE_RMAP_INDEX_SHIFT	41
#define	PTE_RMAP_INDEX_MASK	0x01FFFE0000000000
#define PG_PFNLOCK      	0x2000000000000000 /* pfn bitlock position */
#define	PG_SHOTDN		0
#endif

#endif	/* !defined(PTE_64BIT) */
#endif	/* R10000 && !IP28 */

#if TFP
#if IP21
typedef struct pte {
#ifdef TFP_PTE64
	__uint64_t
		pte_swun1: 24,	/* SW: unused */
		PTE_PFN(28),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_swun2: 2,	/* SW: unused */
		pte_nr : 3,	/* SW: page reference count */
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1;	/* SW: page valid bit */
#define PFNDWDITH	28
#define PFNOFFSET	24
#else
	/* This definition lets ptes fit in 32 bits.  Fields are same sizes
	 * as for the other EVEREST cpu board -- IP19.
	 */
	__uint32_t
		PTE_PFN(23),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW referenced)*/
		pte_pfnlock: 1,	/* SW: pte lock bit */
		pte_nr : 1,	/* SW: page reference count */
		pte_dirty : 1,	/* SW: page dirty bit */
		pte_sv : 1;	/* SW: page valid bit */
#define PFNDWIDTH	23
#define PFNOFFSET	1
#define	PG_PFNLOCK	0x00000008
#define NPFNMASK        0xFFFFFE00
#define NPFNSHIFT       (9 + PGSHFTFCTR)
#define NRMASK          0x00000004
#define NRSHIFT         2
#define CCMASK          0x00001C0
#define CCSHIFT         6
#define	PG_SHOTDN	0
#endif
} pte_t;
#endif	/* IP21 */
#ifdef IP26		/* HW bits need to be the same as IP20/IP22 */
typedef struct pte {
	uint	pte_unused : 1,	/* SW: bit zero to avoid sign extension */
		pte_sv : 1,	/* SW: page valid bit */
		pte_nr : 3,	/* SW: page reference count */
		pte_pfnlock : 1,/* SW: pte pfn lock */
		pte_zero : 1,	/* HW: bit 31 of pfn -- must be zero */
		PTE_PFN(19),	/* HW: core page frame number or 0 */
		pte_cc : 3,	/* HW: coherency algorithm for this page */
		pte_m : 1,	/* HW: modified (dirty) bit */
		pte_vr : 1,	/* HW: valid bit (& SW reference) */
		pte_dirty : 1;	/* SW: page dirty bit (R4000 global bit) */
} pte_t;
#define PFNDWIDTH	19
#define PFNOFFSET	7
#define	PG_PFNLOCK	0x04000000
#define NPFNMASK        0x01FFFFC0
#define NPFNSHIFT       (6 + PGSHFTFCTR)
#define NRMASK          0x38000000
#define NRSHIFT         27
#define CCMASK          0x00000038
#define CCSHIFT         3
#define	PG_SHOTDN	0
#endif	/* IP26 */
#endif	/* TFP */


#if BEAST

#if !defined (PTE_64BIT)

#error << need to fix this as appropriate>>
typedef struct pte {
	__uint32_t
		pte_nr     : 1,	/* SW: page reference count */
		pte_dirty  : 1,	/* SW: page dirty bit */
		pte_sv     : 1,	/* SW: page valid bit */
                pte_pfnlock: 1, /* SW: pfn access/update lock */
		PTE_PFN(18),	/* HW: core page frame number or 0 */
		pte_cc     : 3,	/* HW: coherency algorithm for this page */
		pte_m      : 1,	/* HW: modified (dirty) bit */
		pte_vr     : 1,	/* HW: valid bit (& SW referenced)*/
		pte_g      : 1,	/* HW: ignore pid bit */
	        pte_pgsz   : 4; /* HW: page size field */
} pte_t;
#define	PG_PFNLOCK	
#define NPFNMASK        
#define NPFNSHIFT       
#define NRMASK          
#define NRSHIFT         
#define CCMASK          
#define CCSHIFT         
#define	PG_SHOTDN	
#define PFNDWIDTH	
#define PFNOFFSET	

#define	PAGE_MASK_INDEX_MASK	0x000000000000000F
#define	PAGE_MASK_INDEX_SHIFT	0

#else	/* PTE_64BIT */

typedef struct pte {
	__uint64_t
                pte_rsvd        :  8,   /* SW: reserved bits            */
                pte_pfnlock     :  1,   /* SW: pfn lock. CHECK PG_PFNLOCK!  */
		/* pte_shotdn field: Sync its position with PG_SHOTDN macro */
		pte_shotdn      :  1,   /* SW: pte Page got shot down. 	*/
		pte_numa_home   :  5,	/* SW: Numa policy mgmt 	*/
		pte_nr 		:  1,	/* SW: page reference count 	*/
		pte_dirty 	:  1,	/* SW: page dirty bit 		*/
		pte_sv 		:  1,	/* SW: page valid bit 		*/
		pte_shint	:  4,	/* HW: system hint bits 	*/
		PTE_PFN(32),		/* HW: core page frame number or 0  */
		pte_cc 		:  3,	/* HW: coherency algorithm for page */
		pte_m 		:  1,	/* HW: modified (dirty) bit 	*/
		pte_vr 		:  1,	/* HW: valid bit (& SW referenced)  */
		pte_g 		:  1,	/* HW: ignore pid bit 		*/
		pte_pgsz	:  4;	/* HW: page size to be used	*/
} pte_t;

#define PFNDWIDTH	32
#define PFNOFFSET	22
#define SYSHINTMASK	0x00003C0000000000
#define SYSHINTSHIFT	42
#define NPFNMASK        0x000003FFFFFFFC00
#define NPFNSHIFT       (10+PGSHFTFCTR)

#define NRMASK          0x0001000000000000
#define NRSHIFT         48
#define CCMASK          0x0000000000000380
#define CCSHIFT         7

#define PG_PFNLOCK      	0x0080000000000000 /* pfn bitlock position */
#define	PG_SHOTDN		0x0040000000000000 /* pte page shot down   */

/*
 * Shift and mask values to get the page size info. from the pte.
 * Used in the utlbmiss handler.
 */

#define	PAGE_MASK_INDEX_MASK	0x000000000000000F
#define	PAGE_MASK_INDEX_SHIFT	0

#endif /* PTE_64BIT */

#endif /* BEAST */


#if defined (PTE_64BIT) && !defined (BEAST)
#define RMAP_PTE_INDEX
#endif

#if !R4000 && !TFP && !R10000 && !BEAST
typedef struct pte {
	uint_t	opaque_pte;
} pte_t;
#endif

#ifdef	PTE_64BIT
typedef	__uint64_t	pgi_t;
#else
typedef	uint_t		pgi_t;
#endif	/* PTE_64BIT */

typedef union pde {
	pte_t		pte;		/* pte structure */
	pgi_t		pgi;		/* Full page table entry */
} pde_t;


#define pg_pfn	pte_pfn
#define pg_m	pte_m
#define pg_vr	pte_vr
#define pg_g	pte_g
#define pg_sv	pte_sv
#define pg_nr	pte_nr
#define pg_dirty pte_dirty

#define pg_cc	pte_cc
#define pg_uc	pte_uncattr

#if defined(IP19) || defined(R10000)
#if defined(PTE_64BIT) 
/* Define pg_uncattr in both contexts */
#define	pg_uncattr 	pte_uncattr
#define	pg_pgmaskshift	pte_pgmaskshift
#define	pg_rmap_index	pte_rmap_index


#if defined(NUMA_BASE)
#define	pg_shotdn pte_shotdn
#endif	/* NUMA_BASE */

#endif /* PTE_64BIT */
#endif /* IP19 || R10000 */


#if defined (BEAST)

#define pg_shint	pte_shint
#define pg_pgsz		pte_pgsz

#endif
  
#define pg_pfnlock pte_pfnlock

/*
 * For the R4000, a single tlb maps 2 consecutive virtual pages.
 * This structure is used in the upage, for wired entries.
 */
typedef struct {
	pde_t	pde_low;
	pde_t	pde_hi;
} tlbpde_t;

#endif /* !LANGUAGE_ASSEMBLY */
					/* A disk block is 512 bytes (sector) */
#define NDPP		(8*PGFCTR)	/* Number of disk blocks per page */
#define DPPSHFT		(3+PGSHFTFCTR)	/* Shift for disk blocks per page. */

/*
 * IO map registers (usually) map 4KB chunks while system
 * virtual memory may be using a larger page size.
 */
#if NBPP == IO_NBPP
#define	SYSTOIOPFN(x,i)	(x)
#else
#define	SYSTOIOPFN(x,i)	(((x) << PGSHFTFCTR) + ((i) % PGFCTR))
#endif

/* Page descriptor (table) entry dependent constants */

					/* Nbr of page tbls to map user space */
#define NPGTBLS		btos(HIUSRATTACH_32)
#define	NBPPT		NBPP		/* Number of bytes per page table */
					/* Nbr of pages per page table (seg). */
#define NPGPT		(NBPPT/sizeof(pte_t))
#define	BPTSHFT		PNUMSHFT	/* LOG2(NBPPT) if exact */

#define POFFMASK	(NBPP - 1)	/* Mask for offset into page. */
#define PTOFFMASK	(NPGPT - 1)	/* Mask for offset into page table. */
#define SOFFMASK	((NPGPT * NBPP) - 1)/* Mask for offset into segment. */
#define soff(x)		((__psunsigned_t)(x) & SOFFMASK)
#define ptoff(x)	((__psunsigned_t)(x) & PTOFFMASK)

#define PG_SENTRY	0x7f		/* physical address <= this */
					/* sentinel special handling */
#define PDEFER_SENTRY	0x01		/* defer to next pmap handler */
#define WAIT_SENTRY_LO	0x08		/* physical address as wait key ... */
#define WAIT_SENTRY_HI	0x7f		/* ... for page fault READMAP */

/* Constants for three-level segment table
 *
 * A three level mapping scheme is composed of a base table, which
 * has pointers to the 2nd level segment tables, which in turn point
 * to the 1st level page tables.
 */

/* Number of entries in a 2nd level table. Currently, this is the
 * same as a segment table, which is enough to map 0x80000000 per
 * segment table
 */
#define SEGTABSIZE		NPGTBLS

/* The number of segment tables required to map extended user space. */
#define BASETABSIZE		howmany(HIUSRATTACH_64, stob(SEGTABSIZE))

/* Select a segment pointer from the base table - this is log2(SEGTABSIZE) */
#if _PAGESZ == 4096
#define BASETABSHIFT		9
#endif
#if _PAGESZ == 16384
#ifdef	PTE_64BIT
#define BASETABSHIFT		6
#else
#define BASETABSHIFT		5
#endif	/* PTE_64BIT */
#endif

/* mask off high order bits to select a page table pointer within a segment */
#define SEGTABMASK		(SEGTABSIZE-1)

/* Number of bytes mapped by a segment table */
#define NBPSEGTAB		(SEGTABSIZE * NBPS)

/* Number of pages mapped by a segment table */
#define NPGSEGTAB		(NPGPT * SEGTABSIZE)

/* Given a page number, find its page number within a space
 * mapped by a segment table
 */
#define SEGPGMASK		(NPGSEGTAB - 1)

/* Bytes per base tab shift - turn a vaddr into a base table index */
#define BPBASETABSHIFT		(BPSSHIFT+BASETABSHIFT)

/* A shift amount to scale a pointer into a base table -
 * log2(sizeof pcom_t) - for assembly coding.
 */
#define PCOM_TSHIFT		4

/* given vaddr x, find index into base table (or segment table number) */
#define	btobasetab(x)		((__psunsigned_t)(x) >> BPBASETABSHIFT)

/* given vaddr x, find index into a segment table */
#define btosegtabindex(x)	(btost(x) & SEGTABMASK)

/* given page number x, find index into a segment table */
#define ctosegtabindex(x)	(ctost(x) & SEGTABMASK)

/* given segment x, find index into a segment table */
#define segtosegtabindex(x)	((__psunsigned_t)(x) & SEGTABMASK)

/* given page number x, find its page number within a space mapped
 * by a segment table
 */
#define ctosegpg(x)		((__psunsigned_t)(x) & SEGPGMASK)


#define	PAGE_MASK_INDEX_BITMASK	0xf	/* Size of page mask index */

/* Page descriptor (table) entry field masks */

#if defined(IP20) || defined(IP22) || defined(IP28) || defined(IP32) || defined(IPMHSIM)

#define	PG_CACHMASK	0x00000038	/* cache coherency algorithm */
#define	PG_CACHSHFT	3		/* position in pte_t */
#define PG_UNCACHED	0x00000010	/* non-cached */
#define PG_TRUEUNCACHED	PG_UNCACHED	/* non-cached */
#if _RUN_UNCACHED
#define	PG_NONCOHRNT	PG_UNCACHED
#define PG_COHRNT_EXLWR	PG_UNCACHED
#else
#define	PG_NONCOHRNT	0x00000018	/* Cacheable non-coherent */
#define PG_COHRNT_EXLWR	0x00000028	/* exclusive write */
#endif
#define PG_M		0x00000004	/* modified bit	*/
#define PG_VR		0x00000002	/* valid and referenced  bit */
#define PG_G		0x00000001	/* global (ignore pid)  bit */
#if IP32
#define PG_NDREF	0x20000000	/* reference count (software) */
#define NDREF		1		/* positionless count */
#define PG_NDREFNORM	29		/* normalize NDREF for arithmetic */
#define PG_ADDR		(0x0fffffc0<<PGSHFTFCTR) /* physical page address */
#else
#define PG_EOP		0x04000000	/* end-of-page bit */
#define PG_NDREF	0x18000000	/* reference count (software) */
#define NDREF		2		/* positionless count */
#define PG_NDREFNORM	27		/* normalize NDREF for arithmetic */
#define PG_ADDR		(0x03ffffc0<<PGSHFTFCTR) /* physical page address */
#endif  /* IP32 */
#define PTE_PFNSHFT	(6+PGSHFTFCTR)	/* shift pfn to place in pte */

#define pg_isnoncache(P)	(pg_cachemode(P) == PG_UNCACHED)
#define	pte_iscached(pte)	(((pte) & PG_CACHMASK) != PG_UNCACHED)

#define PG_D		0x80000000	/* page is dirty w.r.t. pte */
#define PG_SV		0x40000000	/* software valid bit */
#define PG_N		PG_UNCACHED
#define PG_PFNUM	PG_ADDR		/* physical page address */

#ifdef R10000
#define	FRAMEMASK_MASK	0x0000fe00	/* Frame Mask register value */
#endif

#endif	/* IP20 || IP22 || IP28 || IP32 */

#if IP19
#define PG_NDREF	0x00000004	/* reference count (software) */
#define NDREF		1		/* positionless count */
#define PG_NDREFNORM	2		/* normalize NDREF for arithmetic */
#define	PG_CACHMASK	0x000001c0	/* cache coherency algorithm */
#define	PG_CACHSHFT	6		/* position in pte_t */
#define PG_TRUEUNCACHED	0x00000080	/* non-cached */
#define PG_M		0x00000020	/* modified bit	*/
#define PG_VR		0x00000010	/* valid and referenced  bit */
#define PG_G		0x00000008	/* global (ignore pid)  bit */
#define PG_ADDR		(0xfffffd00<<PGSHFTFCTR) /* physical page address */
#define PTE_PFNSHFT	(9+PGSHFTFCTR)	/* shift pfn to place in pte */

#if _RUN_UNCACHED
#define	PG_NONCOHRNT	PG_UNCACHED
#define PG_COHRNT_EXLWR	PG_UNCACHED
#else
#define	PG_NONCOHRNT	0x000000c0	/* Cacheable non-coherent */
#define PG_COHRNT_EXLWR	0x00000140	/* exclusive write */
#define	PG_COHRNT_EXCL	0x00000100	/* exclusive access */
#endif	/* _RUN_UNCACHED */

#define PG_D		0x00000002	/* page is dirty w.r.t. pte */
#define PG_SV		0x00000001	/* software valid bit */
#define PG_PFNUM	PG_ADDR		/* physical page address */

#endif /* IP19 */

#if IP21
#ifdef TFP_PTE64	/* in case we ever use 64-bit ptes */
#define PG_ADDR		(0x000000fffffff000<<PGSHFTFCTR)	/* physical page address */
#define PTE_PFNSHFT	(12+PGSHFTFCTR)	/* shift pfn to place in pte */
#define PG_SV		0x00000001	/* software valid bit */
#define PG_D		0x00000002	/* page is dirty w.r.t. pte */
#define PG_NDREF	0x0000001c	/* reference count (software) */
#define NDREF		7		/* positionless count */
#define PG_NDREFNORM	2		/* normalize NDREF for arithmetic */
#define	PG_CACHMASK	0x00000e00	/* cache coherency algorithm */
#define	PG_CACHSHFT	9		/* position in pte_t */
#define PG_TRUEUNCACHED	0x00000400	/* non-cached */
#define PG_UNC_PROCORD  0x00000000      /* un-cached processor ordered */
#define	PG_NONCOHRNT	0x00000600	/* Cacheable non-coherent */
#define PG_COHRNT_EXLWR	0x00000a00	/* Cacheanle coherent, excl on write */
#define PG_M		0x00000100	/* modified bit	*/
#define PG_VR		0x00000080	/* valid and referenced  bit */

#else	/* !TFP_PTE64 -- still using 32-bit ptes */

#define PG_ADDR		(0x7fffff00<<PGSHFTFCTR) /* physical page address */
#define PTE_PFNSHFT	(9+PGSHFTFCTR)	/* shift pfn to place in pte */
#define PG_SV		0x00000001	/* software valid bit */
#define PG_D		0x00000002	/* page is dirty w.r.t. pte */
#define PG_NDREF	0x00000004	/* reference count (software) */
#define NDREF		1		/* positionless count */
#define PG_NDREFNORM	2		/* normalize NDREF for arithmetic */
#define	PG_CACHMASK	0x000001c0	/* cache coherency algorithm */
#define	PG_CACHSHFT	6		/* position in pte_t */
#define PG_TRUEUNCACHED	0x00000080	/* non-cached */
#define PG_UNC_PROCORD  0x00000000      /* un-cached processor ordered */
#define	PG_NONCOHRNT	0x00000180	/* Cacheable non-coherent */
#define PG_COHRNT_EXLWR	0x00000140	/* Cacheable coherent, excl on write */
#define PG_M		0x00000020	/* modified bit	*/
#define PG_VR		0x00000010	/* valid and referenced  bit */
#endif	/* !TFP_PTE64 */

#define PG_G		0x00000000	/* global (ignore pid)  bit */
					/* PG_G doesn't really exist -- */
					/* determined by virtual address */
#endif	/* IP21 */

#if defined(EVEREST) || defined(SN) || defined(IP30)
#define PG_N		PG_UNCACHED
#define PG_UNCACHED	PG_COHRNT_EXLWR	/* (Actually cached) */
#define pg_isnoncache(P)	(pg_cachemode(P) == PG_TRUEUNCACHED)
#define	pte_iscached(pte)	(((pte) & PG_CACHMASK) != PG_TRUEUNCACHED)
#endif	/* EVEREST || ... */

#if IP26
#define PG_ADDR		(0x01ffffc0<<PGSHFTFCTR) /* physical page address */
#define PTE_PFNSHFT	(6+PGSHFTFCTR)	/* shift pfn into place in pte */

#define PG_D		0x00000001	/* page is dirty wrt pte */
#define PG_VR		0x00000002	/* valid and referenced bit */
#define PG_M		0x00000004	/* modified bit */
#define PG_CACHMASK	0x00000038	/* cache coherency algorithm */
#define PG_NDREF	0x38000000	/* reference count (software) */
#define PG_SV		0x40000000	/* software valid bit */

#define PG_CACHSHFT	3		/* position in pte_t */
#define NDREF		7		/* positionless count */
#define PG_NDREFNORM	27		/* normalize NDREF for arithmetic */

#define PG_TRUEUNCACHED	0x00000010	/* non-cached */
#define PG_UNC_PROCORD	0x00000000	/* un-cached processor ordered */
#define PG_NONCOHRNT	0x00000018	/* Cacheable non-coherent */
#define PG_COHRNT_EXLWR	0x00000028	/* Cacheable coherent, excl on write */

#define PG_N			PG_UNCACHED
#define PG_UNCACHED		PG_TRUEUNCACHED
#define pg_isnoncache(P)	(pg_cachemode(P) == PG_UNCACHED)
#define	pte_iscached(pte)	(((pte) & PG_CACHMASK) != PG_UNCACHED)

#define PG_G			0		/* for compat with R4k */
#endif	/* IP26 */

#if defined(IP25) || defined(IP27) || defined(IP30)
#ifdef PTE_64BIT
#if defined (SN0XXL)
#define PG_NDREF	0x1000000000UL	/* reference count (software) */
#define PG_D		0x0800000000UL	/* page is dirty w.r.t. pte */
#define PG_SV		0x0400000000UL	/* software valid bit */
#define PG_ADDR		(0x3ffffffc0<<PGSHFTFCTR) /* physical page address */
#else /* SN0XXL */
#define PG_NDREF	0x0400000000UL	/* reference count (software) */
#define PG_D		0x0200000000UL	/* page is dirty w.r.t. pte */
#define PG_SV		0x0100000000UL	/* software valid bit */
#define PG_ADDR		(0xffffffc0<<PGSHFTFCTR) /* physical page address */
#endif /* SN0XXL */
#else
#define PG_NDREF	0x80000000UL	/* reference count (software) */
#define PG_D		0x40000000UL	/* page is dirty w.r.t. pte */
#define PG_SV		0x20000000UL	/* software valid bit */
#define PG_ADDR		(0x1fffffc0<<PGSHFTFCTR) /* physical page address */
#endif
#define	PG_CACHMASK	0x00000038UL	/* cache coherency algorithm */
#define PG_UNCATTRMASK   0xc000000000000000 /* uncached attr */
#define NDREF		1		/* positionless count */
#define PG_NDREFNORM	30		/* normalize NDREF for arithmetic */
#define	PG_CACHSHFT	3		/* position in pte_t */
#define PG_TRUEUNCACHED	0x00000010UL	/* non-cached */
#define	PG_NONCOHRNT	0x00000018UL	/* Cacheable non-coherent */
#define	PG_COHRNT_EXCL	0x00000020UL	/* exclusive access */
#define PG_COHRNT_EXLWR	0x00000028UL	/* exclusive write */
#define PG_UNCACHED_ACC	0x00000038UL	/* Uncached Accelerated */
#define PG_M		0x00000004UL	/* modified bit	*/
#define PG_VR		0x00000002UL	/* valid and referenced  bit */
#define PG_G		0x00000001UL	/* global (ignore pid)  bit */
#define PTE_PFNSHFT	(6+PGSHFTFCTR)	/* shift pfn to place in pte */

#ifdef SN0 
/* 
 * Uncached Attribute available on SN0 -- SN0 has to use 
 * 64-bit pte in order to support this feature.
 */
#define PG_UC_HSPEC     0x0000000000000000 /* HSpec uncached space  */
#define PG_UC_IO        0x4000000000000000 /* IO uncached space     */
#define PG_UC_MSPEC     0x8000000000000000 /* MSpec uncached space  */
#define PG_UC_MEMORY    0xc000000000000000 /* Memory uncached space */
#if defined (SN0XXL)
#define FRAMEMASK_MASK	0x00000000	/* Use the whole physical addr space */
#else /* SN0XXL */
#define FRAMEMASK_MASK	0x0000c000	/* Use the whole physical addr space */
#endif /* SN0XXL */
#elif IP30
#ifdef PTE_64BIT
#define FRAMEMASK_MASK  0x0000c000      /* Frame Mask register value */
#else
#define	FRAMEMASK_MASK	0x0000fc00	/* Frame Mask register value */
#endif /* PTE_64BIT */
#else
#define	FRAMEMASK_MASK	0x0000f800	/* Frame Mask register value */
#endif /* SN0 */

#endif /* IP25 || IP27 || IP30 */

#if defined (BEAST)
#ifdef PTE_64BIT
#define PG_NDREF	0x0001000000000000UL  /* reference count (software) */
#define PG_D		0x0000800000000000UL  /* page is dirty w.r.t. pte   */
#define PG_SV		0x0000400000000000UL  /* software valid bit         */
#define PG_ADDR		(0xfffffffc00<<PGSHFTFCTR) /* physical page address */
#else
#define PG_NDREF	0x80000000UL	/* reference count (software) */
#define PG_D		0x40000000UL	/* page is dirty w.r.t. pte */
#define PG_SV		0x20000000UL	/* software valid bit */
#define PG_ADDR		(0x1ffffc00<<PGSHFTFCTR) /* physical page address */
#endif
#define	PG_CACHMASK	0x000000380UL	/* cache coherency algorithm */
#define PG_SYSHINTMASK  0x00003c0000000000 /* cached attributes */
#define NDREF		1		/* positionless count */
#define PG_NDREFNORM	30		/* normalize NDREF for arithmetic */
#define	PG_CACHSHFT	7		/* position in pte_t */
#define PG_TRUEUNCACHED	0x000000100UL	/* non-cached */
#define	PG_NONCOHRNT	0x000000180UL	/* Cacheable non-coherent */
#define	PG_COHRNT_EXCL	0x000000200UL	/* exclusive access */
#define PG_COHRNT_EXLWR	0x000000280UL	/* exclusive write */
#define PG_UNCACHED_ACC	0x000000380UL	/* Uncached Accelerated */
#define PG_M		0x000000040UL	/* modified bit	*/
#define PG_VR		0x000000020UL	/* valid and referenced  bit */
#define PG_G		0x000000010UL	/* global (ignore pid)  bit */
#define PTE_PFNSHFT	(10+PGSHFTFCTR)	/* shift pfn to place in pte */

#ifdef SN1 
/* 
 * Cached Attribute available on SN1 -- SN1 has to use 
 * 64-bit pte in order to support this feature.
 */
#define PG_UC_HSPEC     0x0000700000000000 /* HSpec uncached space  */
#define PG_UC_IO        0x0000100000000000 /* IO uncached space     */
#define PG_UC_MSPEC     0x0000200000000000 /* MSpec uncached space  */
#define PG_UC_MEMORY    0x0000300000000000 /* Memory uncached space */
#endif /* SN1 */
#endif /* BEAST */


/*
 * Number of translations that can be held in one tlb entry. In 
 * R4K and R10K it is 2.
 */

#if	R4000 || R10000
#define	NUM_TRANSLATIONS_PER_TLB_ENTRY	2
#else
#define	NUM_TRANSLATIONS_PER_TLB_ENTRY	1
#endif

/* byte addr to virtual page */
#define pnum(X)		((__psunsigned_t)(X) >> PNUMSHFT) 
#define io_pnum(X)	((__psunsigned_t)(X) >> IO_PNUMSHFT) 

/* page offset */
#define poff(X)		((__psunsigned_t)(X) & POFFMASK)
#define io_poff(X)	((__psunsigned_t)(X) & IO_POFFMASK)

/* page base */
#define pbase(X)	((__psunsigned_t)(X) & ~POFFMASK)

/* Disk blocks (sectors) and pages.  */
#define	ptod(PP)	((PP) << DPPSHFT)		/* pages to sectors */
#define	dtop(DD)	(((DD) + NDPP - 1) >> DPPSHFT)  /* sectors to pages */
#define	dtopt(DD)	((DD) >> DPPSHFT)  		/* sectors to pages,*/
							/* truncated	    */
#define dpoff(DD)	((DD) & (NDPP-1))		/* sector page offset */

/* Disk blocks (sectors) and bytes. */
#define	btod(BB)	(((BB) + NBPSCTR - 1) >> SCTRSHFT)

/* Form page descriptor (table) entry from modes and page frame number.  */
#define	mkpde(mode,pfn)	(mode | ((pfn) << PTE_PFNSHFT))

/*	The following macros are used to check the value
 *	of the bits in a page descriptor (table) entry 
 */

/*
 * Migration Support in NUMA machines
 * requires atomic pte updates.
 */



#ifdef	PTE_64BIT
#define PG_SET(l, b)        (atomicSetLong((long*)&(l), (b)))
#define PG_CLR(l, b)        (atomicClearLong((long *)&(l), (b)))
#define PG_FASSIGN(l, f, b) atomicFieldAssignLong((long*)&(l), (f), (b))
#define PG_LOCK(l, b)       bitlock_acquire((long*)&(l), (b))
#define PG_UNLOCK(l, b)     (bitlock_release((long*)&(l), (b)))
#define PG_TRYLOCK(l, b)    (bitlock_condacq((long*)&(l), (b)))
#else
#define PG_SET(l, b)        (atomicSetInt((int*)&(l), (b)))
#define PG_CLR(l, b)        (atomicClearInt((int *)&(l), (b)))
#define PG_FASSIGN(l, f, b) atomicFieldAssignUint((uint*)&(l), (f), (b))
#define PG_LOCK(l, b)       bitlock_acquire_32bit((uint*)&(l), (b))
#define PG_UNLOCK(l, b)     (bitlock_release_32bit((uint*)&(l), (b)))
#define PG_TRYLOCK(l, b)    (bitlock_condacq_32bit((uint*)&(l), (b)))
#endif

#define PG_ISLOCKED(l, b)   ((l) & (b)) 
#define PG_NOTLOCKED(l, b)  (!((l) & (b)))   
#define PG_SETPFN(P, b)     PG_FASSIGN((P)->pgi, NPFNMASK, ((__uint64_t)(b) << NPFNSHIFT))
/*
 * This is just an assignment instead of an atomic operation assuming
 * it needs a single store instruction is needed to do this op. This will
 * happen if the pte size is 64bits or lower on a 64bit kernel and 32bits on a 
 * 32 bit kernel. This is true for all the current kernel configurations.
 */
#define PG_SETPGI(P, b)     ((P)->pgi = (b))
#ifdef SN0
#define PG_SETCCUC(P, b, u)  PG_FASSIGN((P)->pgi, (CCMASK | UNCATTRMASK), \
					(((b) << CCSHIFT) | ((__uint64_t)(u) << UNCATTRSHIFT)));
#else
#define PG_SETCCUC(P, b, u)  PG_FASSIGN((P)->pgi, CCMASK, ((b) << CCSHIFT)); 
#endif
#define PG_SETNR(P, b)      PG_FASSIGN((P)->pgi, NRMASK, ((b) << NRSHIFT))
#define PG_DECNR(P)         PG_SETNR((P), ((P)->pte.pg_nr - 1))
#define	PG_SETPGMASKINDEX(P, indx)   PG_FASSIGN((P)->pgi, PAGE_MASK_INDEX_MASK, \
					((long)(indx) << PAGE_MASK_INDEX_SHIFT))
#define	PG_SETRMAPINDEX(P, indx)   PG_FASSIGN((P)->pgi, PTE_RMAP_INDEX_MASK, \
					((long)(indx) << PTE_RMAP_INDEX_SHIFT))



  
/*	Software bits:
*/
#define pg_isvalid(P) 	((P)->pte.pg_sv)

/*	Set, clear dirty bit.
*/
#define pg_isdirty(P)	((P)->pte.pg_dirty)
#define pg_setdirty(P)	PG_SET((P)->pgi, PG_D)
#define pg_clrdirty(P)	PG_CLR((P)->pgi, PG_D)

/*
 *      Set, clear waiter bit (Overloading dirty bit)
 */
#define pg_iswaiter(P)  ((P)->pte.pg_dirty)
#define pg_setwaiter(P)	PG_SET((P)->pgi, PG_D)
#define pg_clrwaiter(P)	PG_CLR((P)->pgi, PG_D)

/*
 * pfn locking
 * The implementation for the locking routines for the
 * non-debug case is done via macros above in this file.
 * For the debug case, the routines are defined in rmap.c
 */

#ifdef DEBUG_PFNLOCKS
extern void pg_pfnacquire(pde_t*);
extern void pg_pfnrelease(pde_t*);
extern int pg_pfncondacq(pde_t*);
#else 
#define pg_pfnacquire(P)     PG_LOCK((P)->pgi, PG_PFNLOCK)
#define pg_pfnrelease(P)     PG_UNLOCK((P)->pgi, PG_PFNLOCK)
#define pg_pfncondacq(P)     PG_TRYLOCK((P)->pgi, PG_PFNLOCK)
#endif /* DEBUG_PFNLOCKS */

/*
 * pgshotdn interface
 * pgshotdn interface is used as part of shooting down replicated pages.
 * Main purpose of this bit is to indicate that a pte has undergone shootdown
 * process. This information is used to keep the p_nvalid field in the 
 * relavent pregion structure upto date. Since the shootdown mechanism
 * uses reverse maps to shootdown the page, it doesnot have access to
 * pregion structure. Instead it sets the PG_SHOTDN bit in pte. Later,
 * either when the process decides to fault in this page, or process decides
 * to exit, this bit is looked. As part of faulting in a page for this process,
 * p_nvalid is not bumped, if this bit is set. 
 * This is used only when NUMA_REPLICATION is defined.
 */
#define	pg_setshotdn(P)	PG_SET((P)->pgi, PG_SHOTDN)
#define	pg_clrshotdn(P)	PG_CLR((P)->pgi, PG_SHOTDN)
#define	pg_isshotdn(P)	((P)->pgi & PG_SHOTDN)


#define pg_pfnislocked(P)    PG_ISLOCKED((P)->pgi, PG_PFNLOCK)
#define pg_pfnnotlocked(P)   PG_NOTLOCKED((P)->pgi, PG_PFNLOCK)

/*             
 * pgi interface
 */
/***** Does this guy over-write the pfn lock???? */             
#define pg_setpgi(P, s)      PG_SETPGI((P), (s))
#define pg_clrpgi(P)         PG_SETPGI((P), 0)

#ifdef DEBUG_PFNLOCKS

#ifndef PTE_64BIT
#error <<BOMB: DEBUG_PFNLOCKS only when using 64bit ptes>>
#endif /* PTE_64BIT */
#define pg_getpgi(P)         (((P)->pgi) & ~RESVDMASK)

#else /* !DEBUG_PFNLOCKS */
#define pg_getpgi(P)         ((P)->pgi)
#endif /* !DEBUG_PFNLOCKS */

/*
 * pfn interface
 */

#define pg_setpfn(P, s)      PG_SETPFN((P), (s))
#define pg_getpfn(P)         ((P)->pte.pg_pfn)

/*
 * nr interface
 */

#define pg_setnr(P, s)       PG_SETNR((P), (s))
#define pg_getnr(P)          ((P)->pte.pg_nr)
#define pg_decnr(P)          PG_DECNR((P))


/*	The following macros are used to set the value
 *	of the bits in a page descrptor (table) entry 
 */

/*	Hardware bits:
*/

/*	Set, clear hardware valid bit.
*/
#define pg_ishrdvalid(P)	((P)->pgi & PG_VR)
#define pg_sethrdvalid(P)	PG_SET((P)->pgi, PG_VR)
#define pg_clrhrdvalid(P)	PG_CLR((P)->pgi, PG_VR)

/*	Set, clear writeable bit.
*/
#define pg_ismod(P)	((P)->pte.pg_m)
#define pg_setmod(P)	PG_SET((P)->pgi, PG_M|PG_D)
#define pg_clrmod(P)	PG_CLR((P)->pgi, PG_M)

/*	Set, clear global (ignore tlb pid) bit.
*/
#define	pg_isglob(P)	((P)->pgi & PG_G)
#define pg_setglob(P)	PG_SET((P)->pgi, PG_G)
#define pg_clrglob(P)	PG_CLR((P)->pgi, PG_G)

/*
 * Getting and setting rmap index in the pte.
 */

#if defined(RMAP_PTE_INDEX)

#define	pg_set_rmap_index(P, rmap_index) PG_SETRMAPINDEX(P, (rmap_index))
#define	pg_get_rmap_index(P)		((P)->pte.pg_rmap_index)

#else /* RMAP_PTE_INDEX */

#define	pg_set_rmap_index(P, rmap_index) 
#define	pg_get_rmap_index(P)		0

#endif /* RMAP_PTE_INDEX */


/*
 * Getting and setting rmap index in the pte.
 */

#if (defined (IP19) || defined (R10000)) && defined (PTE_64BIT)

#define	pg_set_page_mask_index(P, page_mask_index)	\
					PG_SETPGMASKINDEX(P, page_mask_index)

#define	pg_get_page_mask_index(P)	((P)->pte.pg_pgmaskshift)

#elif defined (BEAST)

#define	pg_set_page_mask_index(P, page_mask_index)	\
					PG_SETPGMASKINDEX(P, page_mask_index)

#define	pg_get_page_mask_index(P)	((P)->pte.pg_pgsz)

#else

#define	pg_set_page_mask_index(P, page_mask_index)
#define pg_get_page_mask_index(P)	0

#endif

/*	Test, set, clear, copy cache attributes.
*/

#define	pg_cachemode(P)		((P)->pgi & PG_CACHMASK)
#define pg_setnoncache(P)	PG_FASSIGN((P)->pgi, PG_CACHMASK, PG_UNCACHED)
#define pg_setnoncohrnt(P)	PG_FASSIGN((P)->pgi, PG_CACHMASK, PG_NONCOHRNT)
#define pg_setcohrnt_exlwr(P)	PG_FASSIGN((P)->pgi, PG_CACHMASK, PG_COHRNT_EXLWR)
#define pg_copycachemode(T,S)	PG_FASSIGN((T)->pgi, PG_CACHMASK, pg_cachemode(S))
#define	pg_setccuc(P, cc, uc)	PG_SETCCUC((P), (cc), (uc))
#define pg_getcc(P)             ((P)->pte.pg_cc)
#define pg_getuc(P)             ((P)->pte.pg_uc)
#if !LANGUAGE_ASSEMBLY && _KERNEL
void pg_setcachable(pde_t *P);
#endif /* !LANGUAGE_ASSEMBLY && KERNEL */
#if JUMP_WAR
#define pg_seteop(P)	PG_SET((P)->pgi, PG_EOP)
#define pg_iseop(P)	((P)->pgi & PG_EOP)
#endif



/*	Set cache alg. for pages in RG_PHYS regions.
*/
#if EVEREST || SN0 || IP30
#define pg_setcache_phys(P)	PG_FASSIGN((P)->pgi, PG_CACHMASK, PG_TRUEUNCACHED)
#else
#define pg_setcache_phys(P)	pg_setnoncache(P)
#endif /* EVEREST || ... */

#if SN0
#define	pg_uncattrmode(P)		((P)->pgi & PG_UNCATTRMASK)
#define pg_setfetchop(P)        PG_FASSIGN((P)->pgi, PG_UNCATTRMASK, PG_UC_MSPEC);\
                                PG_FASSIGN((P)->pgi, PG_CACHMASK, PG_TRUEUNCACHED)
#define pg_isfetchop(P)         ((pg_cachemode(P) == PG_TRUEUNCACHED) && \
				 (pg_uncattrmode(P) == PG_UC_MSPEC))
#define attr_is_fetchop(a)      (((((__uint64_t)(a->attr_uc)) << UNCATTRSHIFT) == PG_UC_MSPEC) && ((((__uint64_t)(a->attr_cc)) << CCSHIFT) == PG_TRUEUNCACHED))


#define pg_check_isfetchop_fault(pd, pfd, m) if (pg_isfetchop(pd)) \
					handle_fetchop_fault(pfd, m);
#else
#define pg_check_isfetchop_fault(pd, pfd, m)
#define attr_is_fetchop(a) 0
#define pg_isfetchop(P) 0
#endif

/*	Some 'bit' macros (rather than pde * macros) to
 *	manipulate cache attributes of pte's.
 */

#if !LANGUAGE_ASSEMBLY && _KERNEL
unsigned pte_cachebits(void);
#endif /* !LANGUAGE_ASSEMBLY && _KERNEL */
#define	pte_cached(pte)		(pgi_t)(((pte) & ~PG_CACHMASK) | pte_cachebits())
#define	pte_noncached(pte)	(pgi_t)(((pte) & ~PG_CACHMASK) | PG_UNCACHED)
#if EVEREST || SN0 || IP30
#define	pte_uncachephys(pte)	(pgi_t)(((pte) & ~PG_CACHMASK) | PG_TRUEUNCACHED)
#else
#define	pte_uncachephys(pte)	(pgi_t)pte_noncached(pte)
#endif


/*	Software bits:
*/

/*	Note that the valid and refernce bits are the same.
*/
#define pg_setref(P)	pg_setvalid(P)	/* Set ref bit.	*/
#define pg_clrref(P)	pg_clrvalid(P)	/* Clear ref bit.	*/

/*	Set valid bit.
**	Note: due to lack of reference bits, "valid bit" is vr, nr, and sv.
*/
#define pg_setvalid(P)	PG_SET((P)->pgi, PG_VR|PG_NDREF|PG_SV)

/*	Clear valid bit.
*/
#define pg_clrvalid(P)	PG_CLR((P)->pgi, (PG_VR|PG_NDREF|PG_SV))

/*	Set, clear soft valid bit.
*/
#define pg_setsftval(P)	PG_SET((P)->pgi, PG_SV)
#define pg_clrsftval(P)	PG_CLR((P)->pgi, PG_SV)

/*	Set, clear reference bits.
*/
#define pg_setndref(P)	PG_SET((P)->pgi, PG_NDREF)
#define pg_clrndref(P)	PG_CLR((P)->pgi, PG_NDREF)

/*
 * Check if the page in the pte is in the process of being migrating.
 */
#if !_LANGUAGE_ASSEMBLY && _KERNEL
extern int pg_ismigrating(pde_t*);
#endif


/*	Virtual addresses.
*/
/*
 * KPTE window is 4 megabytes long and is placed
 * 8 Meg from the top of kseg2, to leave space for upage).
 * KPTE window must be on a 4 Meg boundary.
 * (Note: this definition relies upon 2's complement arithmetic!)
 *
 * KPTEBASE holds the segment table for regular processes and sproc
 * shared address processes that have no local mappings.  It is
 * 2 Mb long (512 pages).  If a shared address process has local
 * mappings (as well as shared mappings) within one logical
 * segment (page of page tables), the shared page table will be
 * mapped at KPTESHRD bytes beyond the private mapping.  A sentinal
 * value is placed in the private mapping's page table entry to
 * indicate that the KPTESHRD entry should be used.
 */
#if R4000 || R10000
#if _MIPS_SIM == _ABI64
#ifdef R10000
#define	KPTEBASE	(0xc0000fc000000000)
#else
#define	KPTEBASE	(0xc00000fc00000000)
#endif /* R10000 */
#else
#define	KPTEBASE	(-0x800000)
#endif
#endif
/*
 * Note that for TFP we must relocate the KPTE to kernel private space
 * in order to allow the mappings to be ASID dependent.  This is required
 * because TFP has no concept of a "global" bit in the tlb.
 */
#if TFP
#define	KPTEBASE	(0x4000fff000000000)
#endif	/* TFP */

/*
 * Beast allows larger virtual pages. This KPTEBASE may change in the
 * future.
 */
#if BEAST
#define	KPTEBASE	(0xc0000fc000000000)
#endif /* BEAST */

#if R4000 || R10000
#ifdef	PTE_64BIT
#define KPTE_TLBPRESHIFT 0		/* R4000 ctxt assumes 16 byte pte's */
#else
#define KPTE_TLBPRESHIFT 1		/* R4000 ctxt assumes 8 byte pte's */
#endif	/* PTE_64BIT */
#else
#define KPTE_TLBPRESHIFT 0
#endif

#ifdef	PTE_64BIT
#define	PDESHFT		3		/* 8 byte pte's */
#else
#define	PDESHFT		2		/* 4 byte pte's */
#endif	/* PTE_64BIT */

#define KPTE_USIZE	(KUSIZE / NBPS * NBPC)
#define KPTE_SHDUBASE	(KPTEBASE - KPTE_USIZE)
#ifndef _LANGUAGE_ASSEMBLY
#if (R4000 || R10000) && _MIPS_SIM == _ABI64
#define KPTE_KBASE	\
	(KPTEBASE + 	\
	(K2BASE >> (TLBEXTCTXT_REGIONSHIFT + KPTE_TLBPRESHIFT + PGSHFTFCTR)))
#else
#define KPTE_KBASE	(KPTEBASE + ((unsigned int)K2BASE / NBPS * NBPC))
#endif
#endif

#if _MIPS_SIM == _ABI64
#define KPTE_REGIONSHIFT (TLBEXTCTXT_REGIONSHIFT+KPTE_TLBPRESHIFT+PGSHFTFCTR)
#define KPTE_REGIONMASK	(TLBEXTCTXT_REGIONMASK >> (KPTE_TLBPRESHIFT+PGSHFTFCTR))
#define KPTE_VPNMASK	((TLBEXTCTXT_VPNMASK >> (KPTE_TLBPRESHIFT+PGSHFTFCTR)) & ~KPTE_REGIONMASK)
#endif

/*	First 64K reserved for system - graphics, etc.
*/
#define PRDAADDR	0x00200000L	/* process private data area */
#define LOWUSRATTACH	0x00000000L	/* lowest addr allowed for USR attach */
#define USRCODE		0x10000000L	/* default location of user code */
#define USRDATA		0x10000200L	/* default location of user data */

/*
 * A small hack - gfx attaches boards at fixed addresses and can't
 * easily deal with failures.
 * Xsgi attaches multiple boards and passes 0x2000000 as an address and
 * needs to attach multiple boards.
 * We need to be sure that by default we don't allocate those addresses
 * (and Xsgi does some mmaping early on).
 * For new compilers, text loads at 0x1000000 (7 zeros) so that no problem
 * We need to be careful about using segment size since it changes
 * with pte size (and a 16K page with 64 bit ptes places a segment at
 * exactly 0x2000000). So, we place LOWDEFATTACH at 0x4000000 = 64Mb.
 */
#define LOWDEFATTACH	((uvaddr_t)0x4000000L)	/* leave the first segment empty by */
						/* default */
#define LOWDEFATTACH64	((uvaddr_t)0x4000000000UL)	/* 1/4 Tb */
					/* avoid address wrap problem */
/*
 * This avoids an address wrap problem. The compiler uses addu
 * instructions to reference arrays on the stack which may end up being
 * above 0x80000000L and sign extended. For example, the code sequence:
 *	addu    v0,sp,v0
 *	lw      v0,-25600(v0)
 * can generate a bus error. If sp+v0 sign extends (e.g., 0xffffffff800012f0),
 * then -25600(v0) will be 0xffffffff7fffaef0 which causes a bus error.
 */
#define DEFUSRSTACK_32	0x7fff8000L	/* default location of user's stack */
#define HIUSRATTACH_32	0x7fff8000L	/* highest allowable user address */

#ifdef _MIPS3_ADDRSPACE
#if defined(SN0XXLnotyet) || defined(SN1)
#define DEFUSRSTACK_64	0x100000000000	/* defaultlocation of user's stack */
#define HIUSRATTACH_64	0x100000000000	/* highest allowable user address */
#else
#define DEFUSRSTACK_64	0x10000000000	/* defaultlocation of user's stack */
#define HIUSRATTACH_64	0x10000000000	/* highest allowable user address */
#endif /* SN0XXL || SN1 */
#else
#define DEFUSRSTACK_64	DEFUSRSTACK_32	/* defaultlocation of user's stack */
#define HIUSRATTACH_64	HIUSRATTACH_32	/* highest allowable user address */
#endif	/* _MIPS3_ADDRSPACE */

/*
 * Largest possible legal user address.  Used in copyin/out and friends to
 * protect against copying over kernel space.  They could use K0BASE, but
 * MAXHIUSRATTACH is more conservative.
 */
#define MAXHIUSRATTACH	HIUSRATTACH_64

#if NBPP == 4096
#define KERNELSTACK	_S_EXT_(0xffffd000)	/* location of kernel stack */
#define PDAPAGE		_S_EXT_(0xffffa000)	/* private data area page */
#endif

#if NBPP == 16384
#if TFP
/* On TFP we need to avoid negative addressing, which only works from r0
 * and can get an Address Error exception when attempted from other registers
 * when the region bits of the effective address differ from the base reg.
 */
#define KERNELSTACK	_S_EXT_(0xffff4000)	/* location of kernel stack */
#else
#define KERNELSTACK	_S_EXT_(0xffffc000)	/* location of kernel stack */
#endif
#define PDAPAGE		_S_EXT_(0xffffc000)	/* private data area */
#endif

#define PDAADDR		PDAPAGE
#define PDASIZE		NBPP

#define KSTACKPAGE	(KERNELSTACK-NBPP)

#define PDAOFFSET	(PDAADDR-PDAPAGE)
#define PRDASIZE	NBPP		/* private proc data area size */

#define K0SEG		K0BASE		/* k0seg kernel text/data cached */
#define K1SEG		K1BASE		/* k1seg kernel text/data uncached */
#define K2SEG		(K2BASE+MAPPED_KERN_SIZE) 	/* Just past mapped
							   kernel text
							   and data if any */
#define K0SEG_SIZE	K0SIZE

#if !_LANGUAGE_ASSEMBLY
/* Include param.h since EXTUSIZE is set in there */
#include <sys/param.h>
#endif	/* !_LANGUAGE_ASSEMBLY */

/*
 * Special tlb indices - these of course are very dependent of PDA, Upage,
 * etc. placement
 */
#define PDATLBINDEX	0		/* processor PDA - set once */

#if NBPP == 16384
#define TLBKSLOTS	0

/* upage, kstk, pda all share one entry */

#else /* NBPP == 4096 */

#ifdef MH_R10000_SPECULATION_WAR
#define EXTK0TLBINDEX	1		/* extended k0 wired indices */
#define EXTK0TLBINDEX2	2
#define UKSTKTLBINDEX	3		/* ukstk wired index */
#else
#define UKSTKTLBINDEX	1		/* ukstk wired index */
#endif /* MH_R10000_SPECULATION_WAR */
#define TLBKSLOTS	1		/* kernel tlb slots for upg/kstk */

#endif /* NBPP */

/*
 * Flags for the nofault handler. 0 means no fault is expected.
 */
#define	NF_BADADDR	1
#define	NF_COPYIO	2
#define	NF_ADDUPC	3
#define	NF_FSUMEM	4
#define	NF_BZERO	5
#define NF_SOFTFP	6
#define NF_REVID	7
#define NF_SOFTFPI	8
#define NF_EARLYBADADDR	9
#define NF_IDBG		10
#define NF_FIXADE	11
#define	NF_SUERROR	12
#define NF_BADVADDR	13
#define NF_DUMPCOPY	14
#define	NF_NENTRIES	15

#if !LANGUAGE_ASSEMBLY && _KERNEL

/*	The following variables describe the memory managed by
**	the kernel.  This includes all memory above the kernel
**	itself.
*/
extern pfn_t	kpbase;		/* The address of the start of	*/
				/* the first physical page of	*/
				/* memory above the kernel.	*/
				/* Physical memory from here to	*/
				/* the end of physical memory	*/
				/* is represented in the pfdat.	*/
extern pde_t	*kptbl;		/* Kernel page table.		*/
extern int	mmem;		/* Number of resident mapped	*/
				/* file pages.			*/
				/* currently in core.		*/
extern int	mmemlim;	/* Maxsystmimum number of resident	*/
				/* mapped file pages.		*/
extern pfn_t	freemem;	/* Current free memory.		*/
extern ulong	freeswap;	/* free swap space		*/


#define availmemlock()		mutex_spinlock(&avail_lock)
#define availmemlockspl(SPL)	mutex_spinlock_spl(&avail_lock, SPL)
#define	availmemunlock(T)	mutex_spinunlock(&avail_lock, T)

#ifdef MH_R10000_SPECULATION_WAR
extern pfn_t smallk0_freemem;	/* count of free "SMALLMEM_K0" */
				/* pages (also included in freemem) */
#endif /* MH_R10000_SPECULATION_WAR */
#endif /* !LANGUAGE_ASSEMBLY && _KERNEL*/

/*	Conversion macros
*/

/*	convert system address to physical address
 *	(works only for k0seg and k1seg)
 *	64 bit kernels support up to 16 GB of memory but we need to
 *	keep 40 bits because the Everest CC uses 0x400000000 range
 *	of addresses.
 */

#if _MIPS_SIM == _ABI64
#define svirtophys(x)	((__psunsigned_t)(x) & 0x0FFFFFFFFFFll)
#else
#define svirtophys(x)	((__psunsigned_t)(x) & 0x1FFFFFFF)
#endif

/*	convert pde to physical address */

#define pdetophys(x)	(((__psunsigned_t)(x)->pte.pg_pfn)<<PNUMSHFT)

/*	Get page frame number from pde	*/
#define pdetopfn(x)          (pg_getpfn((x)))
#define pdetopfdat(x)	     pfntopfdat(pdetopfn(x))


#if !LANGUAGE_ASSEMBLY
/*
 * This structure is used to define the sizes of various constants that
 * are shared between kernel and administrative utilities.
 */
struct pteconst {
	int		pt_ptesize;	/* sizeof(pte_t) */
	int		pt_pdesize;	/* sizeof(pde_t) */
	int		pt_pfnwidth;	/* Width of pfn */
	int		pt_pfnshift;	/* Shift for pfn */
	int		pt_pfnoffset;	/* Offset of pfn */
};
#endif

#ifdef	_KERNEL


/* Macros to set the uncached attributes in a pte on R10k systems 
 * This feature is supported starting from R10000 processor, and
 * its use is first on SN0 machines. Note that low end R10000-based
 * machines still use 32 bit ptes which do not include the uncached
 * attribute bits.
 */
#if _MIPS_SIM == _ABI64 && (defined(R10000) && defined(SN0))
/*
 * K1ATTR_SHFT defined in sys/mips_addrspace.h.
 */

/* Calculate attribute value from the flag field 'f' */
#define	pte_attrval(f)	(((f & VM_UNCACH_ATTR) >> VM_UNCACH_SHFT) - 1)

#define	XKPHYS_UNCACHATTR(x)	((__uint64_t)pte_attrval(x) << K1ATTR_SHFT)

/* Convert V to  uncached XKphys address with suitable attributes */
#define XKPHYS_UNCACHADDR(v, f)  \
			v = (f & VM_UNCACH_ATTR) ? \
			   (caddr_t)(K1MASK(v) | XKPHYS_UNCACHATTR(f)) : (v)

/* Set the nucached attributes for pte 's' */
#define	SET_UNCACH_ATTRVAL(f)	((__uint64_t)pte_attrval(f) << UNCATTRSHIFT)
#define PTE_SETUNCACH_ATTR(s, f) if(f & VM_UNCACH_ATTR)  \
					s |= SET_UNCACH_ATTRVAL(f)
/*
 * Copy uncached attributes from "src" pte to "tgt" pte.
 */
#define	pg_copy_uncachattr(tgt, src)	\
			((tgt)->pte.pg_uncattr = (src)->pte.pg_uncattr)

/* 
 * transfer uncached attribute in k1addr to pte. 
 * Make sure that it picks the right bits from k1addr, and places them
 * at right position in pte.
 */ 
#define	pg_xfer_uncachattr(tgt, k1addr)	\
			((tgt)->pte.pg_uncattr = \
				(((__psunsigned_t)(k1addr) & K1ATTR_MASK) >> K1ATTR_SHFT))

#define pg_getuncattr(P)	((P)->pte.pg_uncattr)

#else

#define PTE_SETUNCACH_ATTR(s, f)
#define XKPHYS_UNCACHADDR(v,f)
#define	pg_copy_uncachattr(tgt, src)
#define	pg_xfer_uncachattr(tgt, k1addr)

#define pg_setuncattr(P, f)
#define pg_getuncattr(P)	0
#endif


/*	Test whether a virtual address is in the kernel dynamically
**	allocated area.
*/

#define	iskvir(va)	((caddr_t)(va) >= (caddr_t)K2SEG && \
			 (caddr_t)(va) < (caddr_t)(K2SEG + ctob(syssegsz)))

#ifdef MH_R10000_SPECULATION_WAR
#define iskvirpte(pte)	(((pte) >= kptbl) && ((pte) < (kptbl + syssegsz)))
#endif /* MH_R10000_SPECULATION_WAR */

/* The following defines let us manage large memory configurations.
 * Kernel Virtual addresses will be K0 when possible 
 * (i.e. when < 256 MB) otherwise will be K2.  We generally handle
 * addresses as Virtual Frame Numbers.
 */

#define vatovfn(x)		((__psunsigned_t)(x) >> PNUMSHFT)
#define vfntova(x)		((caddr_t)((__psunsigned_t)(x) << PNUMSHFT))

#define VFNMIN			((__psunsigned_t)K0SEG >> PNUMSHFT)

#if _MIPS_SIM == _ABI64
#define VFNMAX			((__psunsigned_t)0xffffffffffffffff >> PNUMSHFT)
#else
#define VFNMAX			((__psunsigned_t)0xffffffff >> PNUMSHFT)
#endif

#ifdef MH_R10000_SPECULATION_WAR
#define kvirptetovfn(X)	(((X) - kptbl) + vatovfn(K2SEG))
#endif /* MH_R10000_SPECULATION_WAR */

/*
 * SMALLMEM_PFNMAX	is largest PFN directly mappable in both K0 & K1
 * SMALLMEM_K0_PFNMAX	is largest PFN directly mappable in K0
 */
#if EVEREST
#define	SMALLMEM_K0_PFNMAX	((K0SEG_SIZE >> PNUMSHFT) - 1)
#define	SMALLMEM_K1_PFNMAX	((0x10000000 >> PNUMSHFT) - 1)
#define	SMALLMEM_PFNMAX		SMALLMEM_K1_PFNMAX
#else
#if IP20 || IP22 || IP26 || IP28 || IP30 || SN0
#define SMALLMEM_PFNMAX		((K0SEG_SIZE >> PNUMSHFT) - 1)
#define SMALLMEM_K0_PFNMAX	SMALLMEM_PFNMAX
#define SMALLMEM_K1_PFNMAX	SMALLMEM_PFNMAX
#elif IP32 || IPMHSIM
#define SMALLMEM_PFNMAX         ((K1SIZE >> PNUMSHFT) - 1)
#ifdef MH_R10000_SPECULATION_WAR
/* Moosehead R10000 can only use the first 8 MB of K0SEG */
#define SMALLMEM_K0_R10000_PFNMAX	((0x800000 >> PNUMSHFT) - 1)
#define SMALLMEM_K0_PFNMAX	(IS_R10000() \
					? SMALLMEM_K0_R10000_PFNMAX	\
					: (((K0SIZE / 2) >> PNUMSHFT) - 1))
#else /* MH_R10000_SPECULATION_WAR */
#define SMALLMEM_K0_PFNMAX      (((K0SIZE/2) >> PNUMSHFT) - 1)
#endif /* MH_R10000_SPECULATION_WAR */
#define SMALLMEM_K1_PFNMAX      SMALLMEM_PFNMAX
#else
#define SMALLMEM_PFNMAX		(((K0SEG_SIZE/2) >> PNUMSHFT) - 1)
#define	SMALLMEM_K0_PFNMAX	SMALLMEM_PFNMAX
#define	SMALLMEM_K1_PFNMAX	SMALLMEM_PFNMAX
#endif
#endif
#define SMALLMEM_VFNMIN_K0	VFNMIN
#define SMALLMEM_VFNMAX_K0	(SMALLMEM_VFNMIN_K0 + SMALLMEM_K0_PFNMAX)
#define SMALLMEM_VFNMIN_K1	((__psunsigned_t)K1SEG >> PNUMSHFT)
#define SMALLMEM_VFNMAX_K1	(SMALLMEM_VFNMIN_K1 + SMALLMEM_K1_PFNMAX)
#define K2MEM_VFNMIN		((__psunsigned_t)K2BASE >> PNUMSHFT)

#define IS_SMALLVFN_K0(x)	(((x) >= SMALLMEM_VFNMIN_K0) && \
				 ((x) <= SMALLMEM_VFNMAX_K0))
#define IS_SMALLVFN_K1(x)	(((x) >= SMALLMEM_VFNMIN_K1) && \
				 ((x) <= SMALLMEM_VFNMAX_K1))

#define IS_SMALLVFN(x)		(IS_SMALLVFN_K0(x) || IS_SMALLVFN_K1(x))
#define IS_K2VFN(x)		(((x) >= K2MEM_VFNMIN) &&((x) <= VFNMAX))
#define IS_VFN(x)		(IS_SMALLVFN(x) || IS_K2VFN(x))
#if _MIPS_SIM == _ABI64
/*
 * On 64-bit kernels, all pfns are accessible with "small" addresses.
 * Note that this is NOT true for K1 space on EVEREST some of the
 * uncached K1 space maps to CPU board local registers.
 */
#define small_K0_pfn(x)		1
#if SMALLMEM_K0_PFNMAX == SMALLMEM_K1_PFNMAX	/* catch EVEREST case */
#define small_pfn(x)		1
#define small_K1_pfn(x)		1
#else
#define small_pfn(x)		((x) <= SMALLMEM_PFNMAX)
#define small_K1_pfn(x)		((x) <= SMALLMEM_K1_PFNMAX)
#endif

#else
/*
 * On 32-bit kernels, only "small" memory addresses are accessible
 */
#define small_pfn(x)		((x) <= SMALLMEM_PFNMAX)
#define small_K0_pfn(x)		((x) <= SMALLMEM_K0_PFNMAX)
#define small_K1_pfn(x)		((x) <= SMALLMEM_K1_PFNMAX)
#endif
#define small_pfntovfn_K0(x)	((x) + SMALLMEM_VFNMIN_K0)
#define small_pfntovfn_K1(x)	((x) + SMALLMEM_VFNMIN_K1)
#define small_pfntova_K0(x)	(vfntova(small_pfntovfn_K0(x)))
#define small_pfntova_K1(x)	(vfntova(small_pfntovfn_K1(x)))
#define vfntopfn(x)	((IS_SMALLVFN_K0(x) ? ((x)-SMALLMEM_VFNMIN_K0) : \
			  (IS_SMALLVFN_K1(x) ? \
				((x)-SMALLMEM_VFNMIN_K1) : \
			   (IS_K2VFN(x) ? kptbl[(x)-K2MEM_VFNMIN].pte.pg_pfn:\
			    bad_vfn(x)))))
#define vfntopfdat(x)	(pfntopfdat(vfntopfn(x)))
/*
 * kvatopfn -- replaces svtop and svtopfn with a macro which handles K2 address
 *             as well as K0/K1 address
 * kvatopfdat -- kvtopfdat with a macro which handles K0/K1/K2 address
 */
#define kvatopfn(x)	(vfntopfn(vatovfn(x)))
#define kvatopfdat(x)	(vfntopfdat(vatovfn(x)))
#define	kvatonasid(x)	(pfn_nasid(vfntopfn(vatovfn(x))))

#if !LANGUAGE_ASSEMBLY
extern int bad_vfn(pgno_t);
#endif
#endif /* _KERNEL */

/*	Convert dynamically allocated kernel virtual address 
 *	(in k2seg) to address of kernel page table entry.
 *	(dbx needs this so its outside _KERNEL)
 */
#define kvtokptbl(X) (&kptbl[pnum((__psunsigned_t)(X) - (__psunsigned_t)K2BASE)])

/*	Test whether an address is (secondary) cache-aligned.
 *	On a machine with no secondary cache, this always returns
 *	"aligned".
 */
#define SCACHE_ALIGNED(x)	(((__psunsigned_t)(x) & scache_linemask) == 0)
#if !LANGUAGE_ASSEMBLY && _KERNEL
extern int scache_linemask;
#endif /* !LANGUAGE_ASSEMBLY && _KERNEL */

/*	Flags used by various vm routines.
 *	NOTE: VM_* must match KM_*.  See kmem.h
*/

#define VM_NOSLEEP	0x0001	/* do not wait (sleep) for pages. */
#define VM_TLBNOSET	0x0002	/* do not set wired entries to tlbpid */
#define VM_ONSWAP	0x0004	/* backing store already allocated */
#define VM_FLUSHWIRED	0x0008	/* flush wired pdes. */
				/* On EVEREST systems we avoid uncached access
				 * as it should not be needed and not all
				 * physical addresses have an uncached cpu
				 * address.  Many older systems use VM_UNCACHED
				 * to obtain an "IO coherent" address.
				 */
#if !defined(EVEREST) && !defined(SN0) && !defined(IP30)
#define VM_UNCACHED	0x0010	/* return uncached virtual address */
#else
/* XXX - How should we handle this on SN0? */
#define VM_UNCACHED	0x0000	/* Coherent IO machines allow cached access only */
#endif
#define VM_TLBINVAL	0x0020	/* invalidate all tlbpids */
#define VM_TLBVALID	0x0040	/* TLB ids on other processors still valid */
#define VM_UNSEQ	0x0080	/* unsequential access */
#define VM_DIRECT	0x0100	/* direct map -- that is, k0 or k1 seg */
#define VM_ATTACH	0x0200	/* attach page */
#define VM_BULKDATA	0x0400	/* used for buffer cache, mbufs, etc */
#define VM_CACHEALIGN	0x0800	/* guarantee that memory is cache aligned */
#define	VM_NOAVAIL	0x1000	/* avail[rs]mem already accounted */
#define	VM_BREAKABLE	0x2000	/* breakable VM call (e.g.: user dma) */
#define	VM_STALE	0x4000	/* physical contents can be in other's icache */
#define VM_PHYSCONTIG	0x8000	/* physically contiguous */
#define VM_NODEMASK	0x10000 /* Allocate ONLY on nodes specified in k_nodemask */
				/*    (only used in pagealloc_size) */

#if (! defined(R10000)) || defined(R4000)
#define VM_VACOLOR	0x20000	/* color virtual addresses */
#else
#define VM_VACOLOR	0x00000	/* color virtual addresses - not required */
#endif /* R10000 */
#define VM_PACOLOR	0x40000	/* color physical addresses */
#define VM_VM		0x80000	/* Allocate in K2 space, please */
#define VM_UNCACHED_PIO	0x100000/* Return uncached space for all platforms */

/* 
 * R10000 supports upto 4 uncached address spaces. This space is used
 * on SN0 systems to access special areas available via HUB ASIC.
 * VM needs to provide a way to map these special spaces to user address
 * space. 
 * Flag Bits 23-21 is used for this purpose.
 * These bits are suitably set to indicate to the VM, uncached attribute
 * to be associated with the mapping.
 * These field values are valid , only if either VM_UNCACHE
 * or VM_UNCACHED_PIO flag is set. (i.e. requesting for uncached mapping).
 *
 * Bits 23-21 is used as a single 3-bit wide field rather than 
 * 3 individual flag bits.
 *
 * These fields are specific to SN0.
 * If we ever run out of flag bits for VM, these bits
 * could be recirculated. (Will require some changes in code)
 */
#define	VM_UNCACH_HSPEC	0x200000/* HSpec uncached attribute value 0x1 (001) */
#define	VM_UNCACH_ISPEC	0x400000/* ISpec uncached attribute value 0x2 (010) */
#define	VM_UNCACH_MSPEC	0x600000/* MSpec uncached attribute value 0x3 (011) */
#define	VM_UNCACH_USPEC 0x800000/* USpec uncached attribute value 0x4 (100) */
#define	VM_UNCACH_ATTR	0xe00000/* Attribute space mask               (111) */
#define	VM_UNCACH_SHFT 	(21)	/* uncached attribute shift from right */

/* Following variables are use for NUMA heap allocation */
#define	VM_NODESPECIFIC	0x1000000	/* Allocate memory on specified node */
#define VM_MVOK		0x2000000	/* Ok to move, if necessary */
#ifdef _VCE_AVOIDANCE
#define VM_OTHER_VACOLOR 0x4000000	/* map to alternate virtual color */
#endif /* _VCE_AVOIDANCE */
/*
 *	R10000 Speculative Miss Management
 *
 *	Note: these flags are defined at all times, to avoid requiring
 *	a #ifdef at every use.
 */
#ifdef MH_R10000_SPECULATION_WAR
#define VM_DMA_RD	0x08000000	/* DMA read into memory intended */
#define VM_DMA_WR	0x10000000	/* DMA write from memory intended */
#define VM_NO_DMA	0x20000000	/* no DMA read or write intended */
#else /* MH_R10000_SPECULATION_WAR */
#define VM_DMA_RD	0x00000000	/* DMA read into memory intended */
#define VM_DMA_WR	0x00000000	/* DMA write from memory intended */
#define VM_NO_DMA	0x00000000	/* no DMA read or write intended */
#endif /* MH_R10000_SPECULATION_WAR */
#define VM_DMA_RW	(VM_DMA_RD | VM_DMA_WR)	/* DMA read/write intended */

#if !LANGUAGE_ASSEMBLY && _KERNEL
extern uint cachecolormask;

#define vcache_color(x) (((unsigned int) (x)) & cachecolormask)
#endif /* !LANGUAGE_ASSEMBLY && _KERNEL */

#define colorof(address)	\
		(((__psunsigned_t)(address) >> PNUMSHFT) & cachecolormask)
#define pfncolorof(pfn)         \
                (((__psunsigned_t)(pfn)) &  cachecolormask)
 
#define NOCOLOR	128

/*	Flags used by cache flushing routines.
 */
/* Which cache to flush */
#define	CACH_ICACHE		0x10000
#define	CACH_DCACHE		0x20000
#define CACH_PRIMARY (CACH_ICACHE | CACH_DCACHE)
#define CACH_SCACHE		0x40000
#define CACH_ALL_CACHES (CACH_PRIMARY | CACH_SCACHE)

/* cache ops and address types */
#define	CACH_OPMASK		0xff
#define	CACH_INVAL		0x1
#define	CACH_WBACK		0x2

#define	CACH_NOADDR		0x100
#define	CACH_VADDR		0x200
#define	CACH_LOCAL_ONLY		0x400	/* operate only on current CPU */
#ifdef _VCE_AVOIDANCE
#define CACH_OTHER_COLORS	0x800	/* flush colors other than current */
#endif /* _VCE_AVOIDANCE */

#define	CACH_IO_COHERENCY	0x1000	/* flush for I/O coherency */
#define CACH_ICACHE_COHERENCY	0x2000	/* flush for icache coherency - text
					 * written in dcache */
#define CACH_AVOID_VCES		0x4000	/* flush to avoid VCEs */
#define	CACH_FORCE		0x8000	/* flush because I said so */
#define CACH_REASONS (CACH_IO_COHERENCY|CACH_ICACHE_COHERENCY|CACH_AVOID_VCES|CACH_FORCE)

#define	CACH_SN0_POQ_WAR	0x80000

#if !LANGUAGE_ASSEMBLY && _KERNEL

/* Function Prototypes */
extern void	*kvpalloc(uint, int, int);
#if NUMA_BASE
extern void	*kvpalloc_node(cnodeid_t, uint, int, int);
extern void	*kvpalloc_node_hint(cnodeid_t, uint, int, int);
#else
#define kvpalloc_node(node, size, flags, color)   kvpalloc(size, flags, color)
#define kvpalloc_node_hint(node, size, flags, color)   kvpalloc(size, flags, color)
#endif
extern void	*kvalloc(uint, int, int);
extern void	kvpfree(void *, uint);
extern void	kvpffree(void *, uint, int);
extern void	kvpfree_defer_cleanup(void);
extern void	kvfree(void *, uint);
extern pgno_t	contmemall(int, int, int);
extern pgno_t	contig_memalloc(int, int, int);

extern pfn_t	kvtophyspnum(void *);
extern paddr_t	kvtophys(void *);
extern paddr_t	kvtophyspg(void *);

extern void	kvswappable(void *, pgno_t);
extern int 	kvswapin(void *, register uint, int);

#ifdef LOWMEMDATA_DEBUG
extern void	lowmemdata_record(char *name, cnodeid_t node,
			int size, caddr_t addr);
#else
#define		lowmemdata_record(name, node, size, addr)
#endif

struct eframe_s;
struct uthread_s;

/* Low level TLB management routines */
#if defined(DEBUG) && !defined(JUMP_WAR)
/* Since this routine only unmaps the current processor's tlb, it makes no
 * sense to invoke the routine if the process could migrate - so we check
 * for this condition.
 */
extern void _unmaptlb(unsigned char *, __psunsigned_t);
#define unmaptlb(x, y)	{ ASSERT_NOMIGRATE; _unmaptlb(x,y); }
#else
extern void unmaptlb(unsigned char *, __psunsigned_t);
#endif
#ifdef JUMP_WAR
extern void _invaltlb(int);
#endif

extern void invaltlb(int);
extern uint tlbdropin(unsigned char *, caddr_t, pte_t);
extern void flush_tlb(int);
extern void set_tlbpid(unsigned char);
extern unsigned char tlbgetpid(void);
extern int tlbgetrand(void);
extern int getrand(void);
#if R4000 || R10000
#ifndef _STANDALONE
extern void tlbwired(int, unsigned char *, caddr_t, pte_t, pte_t, int);
#endif
extern void tlbdrop2in(unsigned char, caddr_t, pte_t, pte_t);
#endif
#if TFP
extern void set_icachepid(unsigned char);
extern unsigned char icachegetpid(void);
extern void tlbwired(int, unsigned char *, caddr_t, pte_t);
#endif

#if BEAST
extern void tlbwired(int, unsigned char *, caddr_t, pte_t);
#endif

struct pregion;
struct pfdat;
extern int handlepd(caddr_t, struct pfdat *, pde_t *, struct pregion *, int);

extern void tlbdropin_lpgs(unsigned char, caddr_t, pte_t, pte_t, uint);
extern __psint_t tlb_probe(unsigned char, caddr_t, uint *);
extern void tlb_vaddr_to_pde(unsigned char, caddr_t, pde_t *);


#ifdef MH_R10000_SPECULATION_WAR
#define INV_REF_READONLY 1
#define INV_REF_FAST	2
#define INV_REF_ALL	4

extern void invalidate_kptbl_entry(void *, struct pfdat *, __psint_t);
extern int invalidate_page_references(struct pfdat *, int, int);
extern int invalidate_range_references(void *, int, int, int);
#endif /* MH_R10000_SPECULATION_WAR */

#endif /* !LANGUAGE_ASSEMBLY && _KERNEL */

#endif /* __SYS_IMMU_H__ */
