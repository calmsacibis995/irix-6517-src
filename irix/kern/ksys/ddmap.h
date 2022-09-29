/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_DDMAP_H__
#define __SYS_DDMAP_H__

#ident	"$Revision: 1.11 $"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Public interface for device drivers that wish to map parts of their I/O
 * space.
 * Supports mapping of device or kernel space in either a static or
 * demand paged mode.
 *
 * There are 3 sets of routines here:
 * 1) routines callable only from the devices mmap/munmap entry point.
 *	All locking is done by the layers above the driver. These all take
 *	a vhandl_t * argument that identifies the mapping. This value must not
 *	be stored.
 * 2) routines callable from fault handlers - these are similar to the above
 *	routines except that they do all their own locking
 * 3) routines callable from other places. These do all their own locking
 *	and take a ddv_handle_t argument.
 *	Few drivers will need this complexity, and since they will have
 *	references to address space, need to find some way of being notified
 *	when that address space changes..
 *
 * Note that the handle returned from v_gethandle is NOT a ddv_handle_t. It
 * is a handle that defines the underlying memory object, and can be used
 * in the munmap routine to discern which mapping is going away which is
 * independent of address space (could be due to a fork).
 * The driver's munmap routine is called on last reference of the memory object
 *	which doesn't necessarily correspond to the address space that set up
 *	the object.
 */
typedef struct __vhandl_s vhandl_t;

/* v_getaddr - get virtual address in process space where device is mapped */
extern uvaddr_t v_getaddr(vhandl_t *);

/* get a handle that can be stored that uniquely identifies this mapping */
extern __psunsigned_t v_gethandle(vhandl_t *);

/* get length of mapping in bytes */
extern size_t v_getlen(vhandl_t *);

/*
 * v_getprot - get protections.
 * The returned values are the same as mmap's (PROT_*)
 * We use 'int' here following the type from mmap(2).
 */
extern int v_getprot(vhandl_t *,
	uvaddr_t,	/* address */
	int *,		/* max protections */
	int *);		/* protections for address */

/*
 * v_initphys - set up to be able to demand-page mappings
 * v_setupphys - similar to v_initphys except that it permits setting up
 * 	of per-page attributes (read/write/cache)
 *
 * These routines are called from the device's mmap entry point.
 *
 * The handler is called:
 * int (*)(vhandl_t *,
 *	void *,		opaque arg passed to initphys
 *	uvaddr_t,	address that faulted
 *	int),		read/write/exec
 *
 *  For most drivers, the default cache algorithm should be used. Not all
 *  of these are available on all systems.
 *
 * Unless one really knows what's going on, use VDD_DEFAULT which provides
 * the 'best' way to map I/O space. If you're mapping kernel memory
 * use VDD_COHRNT_EXLWR.
 *
 * NOTE: if using the extended form of v_setupphys (with cache attributes)
 * then v_enterpage, v_enterpfns, v_mapphys, v_mappfns should NOT be used
 * as these routines will reset the attributes. Insteand, use
 * ddv_mappages or ddv_mappfns.
 */
typedef enum {
	VDD_DEFAULT,		/* default cache algorithm for I/O */
	VDD_COHRNT_EXLWR,	/* coherent exclusive write */
	VDD_TRUEUNCACHED	/* true uncached memory */
} v_cachealgo_t;

#define VDD_UNCACHED_DEFAULT	0
#define VDD_UNCACHED_IO 	1

/* values for faulthandler fourth arg and v_mapattr.v_prot */
#define VDD_READ	1
#define VDD_WRITE	2
#define VDD_EXEC	4

typedef struct {
        uvaddr_t v_start;
        uvaddr_t v_end;
        v_cachealgo_t v_cc;	/* cache control */
        uint v_uc;	        /* uncached qualifiers */
	int v_prot;		/* protections */
} v_mapattr_t;

extern int v_initphys(vhandl_t *,
	int (*)(vhandl_t *, void *, uvaddr_t, int), /* fault handler */
	size_t,				/* length in bytes to set up */
	void *);			/* opaque arg to fault handler */
extern int v_setupphys(vhandl_t *,
	int (*)(vhandl_t *, void *, uvaddr_t, int), /* fault handler */
	size_t,				/* length in bytes to set up */
	void *,				/* opaque arg to fault handler */
	v_mapattr_t *,			/* attributes */
	int);				/* number of attributes */

/*
 * add page(s) to device. This differs from v_mapphys in that
 * it performs all locking, so is used outside of the devmap() routine.
 * It also permits specifying an arbitrary virtual address within
 * the devices mapped space.
 * This is called usually from the devices fault handler.
 */
extern int v_enterpage(vhandl_t *,
	uvaddr_t,	/* virtual address in space to map */
	void *,		/* kernel virtual address to map */
	pgcnt_t);	/* # of pages to map */

extern int v_enterpfns(vhandl_t *, 
	uvaddr_t, 	/* virtual addres in space to map */
	pfn_t *, 	/* page to map */
	pgcnt_t,	/* # of pages to map */
	int,		/* cacheability flag */
	__uint64_t ptebits);	/* additional hardware specific bits for pte */

/*
 * v_mapphys - map pages to given address
 * This is usually called from within the driver's mmap entry point to
 * statically setup the mapping. It assumes that everything is already locked
 */
extern int v_mapphys(vhandl_t *,
	void *,		/* kernel virtual address - K0,1,2, vme, etc. */
	size_t);	/* length in bytes */

/*
 * v_mappfns - map pages to given address
 *  This is usually called from within the driver's mmap entry point to
 * statically setup the mapping.
 */
extern int v_mappfns(vhandl_t *, 
	pfn_t *, 	/* list of pfns to be mapped */
	pgcnt_t,	/* # of pages to map */
	int,		/* cacheability flag */
	__uint64_t ptebits);	/* additional hardware specific bits for pte */

/* 
 * v_getmappedpfns - return in the pfn array, the pfns of pages
 * that are currently mapped
 * This may only be called from the drvunmap routine
 */
extern int v_getmappedpfns(vhandl_t *,
	off_t,		/* (byte) offset in space to start at */
	pfn_t *,	/* where to store pfns */
	pgcnt_t);	/* length of pfn list */

/*
 * v_allocpage - allocate a page of physical memory.
 * The uvaddr_t arg is used to tell the Policy Module information that
 * the application might have set up.
 * The size_t parameter is the page size requested - currently that
 * better be NBPP.
 */
extern int v_allocpage(vhandl_t *, uvaddr_t, pfn_t *, size_t *, int);

extern void v_freepage(vhandl_t *, pfn_t);

/*
 * The following routines are only for special drivers that require the
 * complexity of tracking address spaces, tlb's, etc..
 */
typedef struct __ddv_handle_s *ddv_handle_t;

/*
 * v_getddv_handle - given a vhandl_t, get a ddv_handle_t
 * This routine can sleep.
 * NOTE: must be called after any call to v_setupphys.
 */
extern ddv_handle_t v_getddv_handle(vhandl_t *);

/*
 * dispose of a ddv_handle_t. This must be called on last use,
 * and may only be called once.
 */
extern void ddv_destroy_handle(ddv_handle_t);

/*
 * ddv_getaddr - get virtual address in process space where device is mapped
 */
extern uvaddr_t ddv_getaddr(ddv_handle_t);

/*
 * ddv_getlen - return length in bytes
 */
extern size_t ddv_getlen(ddv_handle_t);

/*
 * ddv_getprot - return protections
 */
extern int ddv_getprot(ddv_handle_t, off_t, int *, int *);

/*
 * ddv_lock - lock down mapping. This must be done prior to a ddv_mappages
 *	call. This call may sleep
 */
extern void ddv_lock(ddv_handle_t);

/*
 * ddv_unlock - unlock down mapping. 
 */
extern void ddv_unlock(ddv_handle_t);

/*
 * ddv_mappages - set up a page table to a set of pages.
 *  It sets the protections and cache algorithm based on the pregion attributes
 *  set up with a previous call (v_setupphys).
 * The  __uint64_t argument should only be used to specify hardware specific
 * bits. Do not specify a pfn, or a standard processor cache mode - those
 * should have been set already using v_setupphys. These bits will be
 * or'd into the pte w/o any changes
 *
 * It assumes that things are locked - either via ddv_lock OR due to being
 * in the drvmap routine.
 */
extern int ddv_mappages(ddv_handle_t,
	off_t,		/* (byte) offset in space to map */
	void *,         /* kernel virtual address (can be physical) to map */
	pgcnt_t);	/* # of pages to map */
extern int ddv_mappfns(ddv_handle_t,
	off_t,		/* (byte) offset in space to map */
	pfn_t *,        /* pfns to map */
	pgcnt_t, 	/* # of pages to map */
	__uint64_t ptebits);	/* additional hardware specific bits for pte */

/*
 * ddv_invaltlb - used to flush the hardware TLB.
 * This is primarily for use at context switch time.
 * it does no locking and acts on the current CPU only and assumes 'current'
 * thread.
 * Passing in a len == -1 means the entire mapped area (off is ignored)
 */
extern void ddv_invaltlb(ddv_handle_t, off_t, ssize_t);

/*
 * ddv_invalphys - invalidate pages of a mapped space.
 * remove pages from page map (undo the v_initphys/v_setupphys/v_enterpage
 *	ddv_mappages) which will cause
 *	a fault to occur if a process attempts to access them. In this case
 *	if the space isn't being demand pages, these pages will be unmapped
 *	forever and access to them will cause a SEGV.
 * Passing in len == -1 will cause the entire space to be unmapped.
 * This routine will return:
 *	>0 (errno value) on error
 *	0 if successfully unmapped at least 1 page.
 * It returns ENOENT if there are no pages in the specified range.
 *
 * This routine has 2 flavors - the first is completely MP safe
 *	but can't be called at context switch time.
 * The second CAN be called at context switch time BUT it is the caller's
 *	responsibility to make sure that NO other process/thread is accessing
 *	the device with the same mapping on ANY processor.
 *	(fal ==> fast-and-loose)
 *
 */
extern int ddv_invalphys(ddv_handle_t,
	off_t,		/* (byte offset to start unmap at */
	ssize_t);	/* length in bytes (<0 means do the whole thing) */
extern int ddv_invalphys_fal(ddv_handle_t,
	off_t,		/* (byte offset to start unmap at */
	ssize_t);	/* length in bytes (<0 means do the whole thing) */

/*
 * special gfx interfaces - used by kabi for now to keep the API ..
 */
#define VDD_GFX_NOSHARE		0x0001	/* don't share map on fork */
extern int gfxdd_mmap(int, size_t, uvaddr_t,
	int (*)(vhandl_t *, void *, uvaddr_t, int),
	void *, v_mapattr_t *, int, ddv_handle_t *);
extern int gfxdd_mmap_mem(int, size_t, uvaddr_t, ddv_handle_t *);
extern int gfxdd_mmap_mem_ex(int, size_t, uvaddr_t, v_mapattr_t *, int, ddv_handle_t *);
  
extern void gfxdd_munmap(ddv_handle_t);
extern int gfxdd_lookup(ddv_handle_t);
extern int gfxdd_recycle(void (*)(void *, void *, void *), void *, uvaddr_t);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_DDMAP_H__ */
