/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef __SYS_AS_H__
#define __SYS_AS_H__

#ident  "$Revision: 1.47 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <ksys/behavior.h>
#include <ksys/kqueue.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/watch.h>

/*
 * Address Space Management
 */

/*
 * The basic handle to an address space given out to local clients -
 * 'asid_t' is defined in sys/types.h
 * The asid_t must be converted to a vasid_t - for a process acting on
 * its own address space this is simple - for 3rd party access this
 * lookup is synchronized with a process execing/exitting
 * A vasid is valid only on the cell where the lookup was done.
 */
struct vas_s;
typedef struct {
	struct vas_s *vas_obj;		/* virtual address space object */
	aspasid_t vas_pasid;		/* private address space id */
} vasid_t;

/*
 * Basic handle given to kernel clients that initiate an attach to a processes
 * space and need a way to refer to it. This is primarily for mmap devices
 * and gfx
 */
typedef struct {
	asid_t asoh_handle;
	uvaddr_t *asoh_addr;		/* attach address */
} as_ohandle_t;

/*
 * a memory object handle. This is a system wide id - valid on any
 * cell.
 * XXX not yet it isn't!
 */
typedef struct {
	void *as_mohandle;
} as_mohandle_t;

/*
 * Virtual address space descriptor
 * vasid_t.vas_obj points to here.
 */
typedef struct vas_s {
	kqueue_t vas_queue;		/* lookup list */
	kqueue_t vas_squeue;		/* scanning list */
	bhv_head_t vas_bhvh;		/* behavior head */
	uint64_t vas_gen;
	int vas_refcnt;
	int vas_defunct;
} vas_t;
#define SKQ_TO_VAS(skq)		((vas_t *)((char *)(skq) - offsetof(vas_t, vas_squeue)))

/*
 * Decls for asvo_fault
 * Note that because this gets passed a uthread pointer, most of
 * it must run on the 'local' cell
 */
typedef enum {
	AS_VFAULT,
	AS_PFAULT
} as_faultop_t;

typedef struct {
	uvaddr_t as_epc;
	uvaddr_t as_uvaddr;		/* fault address */
	struct uthread_s *as_ut;	/* faulting uthread */
	char as_rw;
	signed char as_szmem;	/* # bytes of memory access */
	uint_t as_flags;
} as_fault_t;
/* values for as_flags */
#define ASF_FROMUSER		0x0001	/* from user mode */
#define ASF_ISSC		0x0002	/* instruction was sc/sd */
#define ASF_PTRACED		0x0004	/* thread being debugged */
#define ASF_NOEXC		0x0008	/* pfault called w/o exception ptr -
					 * used to break COW in kernel */

typedef struct {
	int as_code;
} as_faultres_t;

/*
 * Decls for asvo_addspace.
 */
typedef enum {
	AS_ADD_PRDA,
	AS_ADD_SHM,
	AS_ADD_STACKEXEC,
	AS_ADD_EXEC,
	AS_ADD_MMAP,
	AS_ADD_MMAPDEV,
	AS_ADD_LOAD,
	AS_ADD_INIT,
	AS_ADD_ATTACH
} as_addspaceop_t;

typedef struct {
	as_addspaceop_t as_op;
	uvaddr_t as_addr;	/* addr to attach at or AS_ADD_UNKVADDR */
	size_t as_length;	/* length in bytes */
	mprot_t as_prot;	/* protections (PROT_*) from mman.h */
	mprot_t as_maxprot;
	union {
		struct { /* AS_ADD_SHM */
			as_mohandle_t __add_mo;	/* memory object */
			int __add_maxsegs;	/* max # of shm segments */
			int __add_flags;	/* flags */
		} __add_shm;
		struct { /* AS_ADD_MMAP */
			uint_t __add_flags;	/* user flags */
			off_t __add_off;	/* offset in file */
			struct vnode *__add_vp;	/* vnode handle */
			int __add_ckpt;		/* file ckpt id */
		} __add_mmap;
		struct { /* AS_ADD_EXEC */
			uint_t __add_flags;	/* user flags */
			off_t __add_off;	/* offset in file */
			struct vnode *__add_vp;	/* vnode handle */
			int __add_ckpt;		/* file ckpt id */
			size_t __add_zfodlen;	/* zero-fill-on-demand */
		} __add_exec;
		struct { /* AS_ADD_LOAD */
			uint_t __add_flags;	/* user flags */
			off_t __add_off;	/* offset in file */
			struct vnode *__add_vp;	/* vnode handle */
			int __add_ckpt;		/* file ckpt id */
			size_t __add_zfodlen;	/* zero-fill-on-demand */
			caddr_t __add_laddr;	/* addr to start loading at */
			size_t __add_llength;	/* load length */
		} __add_load;
		struct { /* AS_ADD_STACKEXEC */
			caddr_t __add_contents;	/* ptr to stack contents */
		} __add_stackexec;
		struct { /* AS_ADD_ATTACH */
			uvaddr_t __add_src_vaddr; /* address in source AS */
			asid_t __add_asid;	/* asid to attach from */
			uint_t __add_flags;
		} __add_attach;
	} __add_un;
} as_addspace_t;
#define as_shm_mo	__add_un.__add_shm.__add_mo
#define as_shm_maxsegs	__add_un.__add_shm.__add_maxsegs
#define as_shm_flags	__add_un.__add_shm.__add_flags
#define as_mmap_flags	__add_un.__add_mmap.__add_flags
#define as_mmap_off	__add_un.__add_mmap.__add_off
#define as_mmap_vp	__add_un.__add_mmap.__add_vp
#define as_mmap_ckpt	__add_un.__add_mmap.__add_ckpt
#define as_exec_flags	__add_un.__add_exec.__add_flags
#define as_exec_off	__add_un.__add_exec.__add_off
#define as_exec_vp	__add_un.__add_exec.__add_vp
#define as_exec_ckpt	__add_un.__add_exec.__add_ckpt
#define as_exec_zfodlen	__add_un.__add_exec.__add_zfodlen
#define as_load_flags	__add_un.__add_load.__add_flags
#define as_load_off	__add_un.__add_load.__add_off
#define as_load_vp	__add_un.__add_load.__add_vp
#define as_load_ckpt	__add_un.__add_load.__add_ckpt
#define as_load_zfodlen	__add_un.__add_load.__add_zfodlen
#define as_load_laddr	__add_un.__add_load.__add_laddr
#define as_load_llength	__add_un.__add_load.__add_llength
#define as_stackexec_contents	__add_un.__add_stackexec.__add_contents
#define as_attach_srcaddr __add_un.__add_attach.__add_src_vaddr
#define as_attach_asid	__add_un.__add_attach.__add_asid
#define as_attach_flags	__add_un.__add_attach.__add_flags

#define AS_ADD_UNKVADDR ((uvaddr_t)-1L)
/* values for as_attach_flags */
#define AS_ATTACH_MYSELF	0x0001

/* valures for as_shm flags */
#define AS_SHM_INIT	0x0001
#define AS_SHM_SHATTR	0x0002

typedef struct {
	uvaddr_t as_addr;	/* attach address */
	mprot_t as_prot;	/* actual prots attached with */
	union {
		struct { /* AS_ADD_ATTACH */
			uvaddr_t __add_local_vaddr;
			size_t __add_size;
		} __add_attach;
	} __add_un;
} as_addspaceres_t;

#define as_attachres_vaddr	__add_un.__add_attach.__add_local_vaddr
#define as_attachres_size	__add_un.__add_attach.__add_size

/*
 * decls for as_deletespace
 * Note that for some types, lots of work is done (e.g. exec, the prda
 * and watchpoints are deleted ..)
 */
typedef enum {
	AS_DEL_MUNMAP,
	AS_DEL_EXEC,
	AS_DEL_SHM,
	AS_DEL_EXIT,
	AS_DEL_VRELVM,
	AS_DEL_EXEC_SMEM
} as_deletespaceop_t;

typedef struct {
	as_deletespaceop_t as_op;
	union {
		struct { /* DEL_MUNMAP */
			uvaddr_t __del_start;
			size_t __del_len;	/* in bytes */
			int __del_flags;
		} __del_munmap;
		struct { /* DEL_SHM */
			uvaddr_t __del_start;
		} __del_shm;
		struct { /* DEL_VRELVM/DEL_EXIT */
			struct prda *__del_prda;
			uchar_t __del_detachstk;
		} __del_vrelvm;
		struct { /* DEL_EXEC */
			struct prda *__del_prda;
			uchar_t __del_detachstk;
			uchar_t __del_rmp;
		} __del_exec;
	} __del_un;
} as_deletespace_t;
#define as_munmap_start	__del_un.__del_munmap.__del_start
#define as_munmap_len	__del_un.__del_munmap.__del_len
#define as_munmap_flags	__del_un.__del_munmap.__del_flags
#define as_shm_start	__del_un.__del_shm.__del_start
#define as_vrelvm_prda	__del_un.__del_vrelvm.__del_prda
#define as_vrelvm_detachstk	__del_un.__del_vrelvm.__del_detachstk
#define as_exec_prda	__del_un.__del_exec.__del_prda
#define as_exec_detachstk	__del_un.__del_exec.__del_detachstk
#define as_exec_rmp	__del_un.__del_exec.__del_rmp

/* values for munmap_flags */
#define DEL_MUNMAP_LOCAL	0x1
#define DEL_MUNMAP_NOLOCK	0x2

typedef union {
	int as_shmid;	/* AS_DEL_SHM */
} as_deletespaceres_t;

/*
 * decls for asvo_new
 */
typedef enum {
	AS_FORK,
	AS_SPROC,
	AS_NOSHARE_SPROC,
	AS_UTHREAD
} as_newop_t;

/* decl for asvo_new - input parameters */
typedef struct {
	as_newop_t as_op;
	struct utas_s *as_utas;	/* new threads ut_as */
	uvaddr_t as_stkptr;
	size_t as_stklen;
} as_new_t;
#define AS_USE_RLIMIT ((size_t)-1L)

/* return struct from asvo_new - fork/sproc */
typedef struct {
	asid_t as_casid;	/* thread's asid */
	vasid_t as_cvasid;	/* thread's vasid */
	struct pm *as_pm;
	uvaddr_t as_stkptr;
} as_newres_t;

struct vpagg_s ;
/*
 * There are 4 routines to get attributes about an AS:
 *
 * asvo_getasattr - simple attributes about entire AS -
 *			multiple attributes may be retrieved
 * asvo_getvaddrattr - simple attributes about a particular vaddr -
 *			multiple attributes may be retrieved
 * asvo_getattr - complex attributes that may require input and output args
 *			exclusive attributes only
 * asvo_getrangeattr - get attributes about a range of addresses
 */

/*
 * decls for asvo_getasattr - get simple attributes about the address space
 * as a whole.
 */
typedef struct {
	rlim_t as_stkmax;
	rlim_t as_datamax;
	rlim_t as_vmemmax;
	rlim_t as_rssmax;
	uvaddr_t as_brkmax;
	uvaddr_t as_stkbase;	/* lowest valid stack address */
	size_t as_stksize;	/* # bytes in stack */
	uvaddr_t as_brkbase;
	size_t as_brksize;
	pgcnt_t as_psize;	/* process size for share group member */
	pgcnt_t as_rsssize;	/* rss size for entire share group */
	int as_lockdown;
	cpumask_t as_isomask;	/* isolation cpus AS is running on */
	uvaddr_t as_prda;	/* address of PRDA */
	struct vpagg_s  *as_vpagg;	/* address of process aggregate */
} as_getasattr_t;

/* values for as_mask */
#define AS_VMEMMAX	0x00000001	/* current PSIZE(rlimit VMEM) max */
					/* out:as_vmemmax */
#define AS_BRKMAX	0x00000002	/* ulimit */
					/* out:as_brkmax */
#define AS_STKMAX	0x00000004	/* current STACK (rlimit) max */
					/* out:as_stkmax */
#define AS_STKBASE	0x00000008	/* current low stack addr */
					/* out:as_stkbase */
#define AS_STKSIZE	0x00000010	/* current stack size in pages */
					/* out:as_stksize */
#define AS_BRKBASE	0x00000020	/* current brk addr */
					/* out:as_brkbase */
#define AS_BRKSIZE	0x00000040	/* current brk size in bytes */
					/* out:as_brksize */
#define AS_DATAMAX	0x00000080	/* current DATA (rlimit) max */
					/* out:as_datamax */
#define AS_RSSMAX	0x00000100	/* current RSS (rlimit) max */
					/* out:as_rssmax */
#define AS_PSIZE	0x00000200	/* get process size in pages */
					/* out:as_psize */
#define AS_RSSSIZE	0x00000400	/* get resident size in pages */
					/* out:as_rsssize */
#define AS_LOCKDOWN	0x00000800	/* plock status */
					/* out:as_lockdown */
#define AS_ISOMASK	0x00001000	/* isolation mask */
					/* out:as_isomask */
#define AS_PRDA		0x00002000	/* PRDA address (if have one) */
					/* out:as_prda */
#define AS_VPAGG	0x00004000	/* Process aggregate (if have one) */
					/* out:as_vpagg */

/*
 * decls for asvo_getvaddrattr - get attributes about for a particular address.
 * Does not give details of page mappings
 * Note that if an error indication is given back it is unknown which
 * mask value generated the error - thus, for some of these options
 * that can legitimately return errors, it is best to pass them singly.
 * These mask values are tagged as EXCL (exclusive).
 */
typedef struct {
	uint_t as_flags;	/* flags assoc. with region containing addr */
	struct vnode *as_vp;
	struct vnode *as_loadvp;
	struct {
		uint	as_ckptflags;
		int	as_ckptinfo;
	} as_ckpt;
	struct {
		uvaddr_t as_base; /* attach addr */
		size_t as_len;	  /* length of attach */
	} as_pinfo;
	mprot_t as_mprot;	/* protections for addr */
} as_getvaddrattr_t;

/* values for as_mask */
#define AS_VNODE	0x00000001	/* file handle */
					/* out:as_vp */
#define AS_LOADVNODE	0x00000002	/* vnode if pre-paged */
					/* out:as_loadvp */
#define AS_FLAGS	0x00000004	/* out:as_flags */
#define AS_CKPTMAP	0x00000010	/* checkpoint map address */
					/* out:as_ckptmap */
#define AS_PINFO	0x00000020	/* info about pregion */
					/* out:as_pinfo */
#define AS_MPROT	0x00000040	/* return protections */
					/* out:as_prot */

/* values for as_flags */
#define AS_LPAGES	0x00000001
#define AS_PHYS		0x00000002	/* address maps physical device */

/*
 * decls for asvo_getattr - complex attribute requests - these are all exclusive
 */
typedef enum {
	AS_NATTRS,		/* # segments/attributes /proc */
	AS_GFXLOOKUP,		/* look up gfx region  */
	AS_GFXRECYCLE,		/* gfx recycle  */
				/* out:as_fltarg as_recyclearg */
	AS_INUSE,		/* is file handle in use (fuser) */
				/* out:EBUSY */
	AS_CKPTGETMAP,		/* get info about all pregions */
	AS_GET_PAGERINFO,	/* pager info */
	AS_GET_PRATTR,		/* get summation of attributes (for /proc) */
	AS_GET_KVMAP		/* get memory map (core) */
} as_getattrop_t;

/* returned values */
typedef union {
	pgcnt_t as_nattrs;	/* AS_NATTRS */
	struct {		/* AS_GFXRECYCLE */
		void *as_fltarg;	/* fault function for PHYS regions */
		void *as_recyclearg;	/* XXX */
	} as_recycle;
	int as_nckptmaps;	/* AS_CKPTGETMAP */
	struct {		/* AS_PAGERINFO */
		short as_pri;		/* info for building paging lists */
		short as_state;		/* info for building paging lists */
		long as_slptime;	/* info for building paging lists */
	} as_pagerinfo;
	int as_nentries;	/* AS_GET_PRATTR */
	struct {		/* AS_GET_KVMAP */
		int as_nentries;
		struct kvmap *as_kvmap;
	} as_kvmap;
} as_getattr_t;

/* a kvmap */
typedef struct kvmap {
	uvaddr_t	v_vaddr;
	size_t		v_len;
	ushort		v_flags;	/* defined in core.out.h */
	ushort		v_type;		/* defined in core.out.h */
	uint_t		v_iflags;	/* internal flags */
} kvmap_t;

/*
 * various input parameters for asvo_getattr
 */
typedef union {
	uvaddr_t	as_uvaddr;
	struct {			/* AS_INUSE */
		struct vnode *as_vnode;
		int as_contained;
	} as_inuse;
	struct {			/* AS_CKPTGETMAP */
		int as_n;
		uvaddr_t as_udest;
	} as_ckptgetmap;
	struct {			/* AS_GET_PRATTR */
		int as_maxentries;
#if (_MIPS_SIM == _ABI64)
		struct prmap_sgi *as_data;
#elif (_MIPS_SIM == _ABIO32 || _MIPS_SIM == _ABIN32)
		struct irix5_n32_prmap_sgi *as_data;
#else
		<<<BOMB>>>
#endif
	} as_prattr;
} as_getattrin_t;

/*
 * asvo_setattr - set attributes common to the entire address space
 * We use an enum here for more data checking since we don't expect to
 * set more than one thing.
 */
typedef enum {
	AS_SET_ISOLATE,
	AS_SET_UNISOLATE,
	AS_SET_STKMAX,
	AS_SET_DATAMAX,		/* heap/data max */
	AS_SET_VMEMMAX,		/* process size maximum */
	AS_SET_RSSMAX,		/* RSS maximum */
	AS_SET_STKBASE,		/* set stack base */
	AS_SET_PMAP,		/* change pmap (CKPT) */
	AS_SET_STKSIZE,		/* set stack size (pages) */
	AS_SET_ULI,		/* mark as part of a ULI */
	AS_SET_CKPTBRK,		/* set brkbase/size for CKPT */
	AS_SET_VPAGG		/* set process aggregate */
} as_setattrop_t;

typedef struct {
	as_setattrop_t as_op;		/* what to set */
	union {
		struct { /* SET_ULI */
			int __set;	/* 1 == set, 0 == unset */
		} __set_uli;
		struct { /* SET_ISOLATE/SET_UNISOLATE */
			cpuid_t __set_cpu; /* what cpu isolating to */
		} __set_iso;
		struct { /* SET_STKMAX, SET_DATAMAX, SET_VMEMMAX, SET_RSSMAX */
			rlim_t	__set_max;
		} __set_max;
		struct { /* SET_STKBASE */
			uvaddr_t __set_stkbase;
		} __set_stkbase;
		struct { /* SET_STKSIZE */
			pgcnt_t __set_stksize;
		} __set_stksize;
		struct { /* SET_PMAP */
			u_char __set_abi;
		} __set_pmap;
		struct { /* SET_CKPTBRK */
			uvaddr_t __set_brkbase;
			size_t __set_brksize;
		} __set_ckptbrk;
		struct { /* SET_VPAGG */
			struct vpagg_s *__set_vpagg;
		} __set_vpagg;
	} __set_un;
} as_setattr_t;

#define as_set_uli	__set_un.__set_uli.__set
#define as_iso_cpu	__set_un.__set_iso.__set_cpu
#define as_set_max	__set_un.__set_max.__set_max
#define as_set_stkbase	__set_un.__set_stkbase.__set_stkbase
#define as_set_stksize	__set_un.__set_stksize.__set_stksize
#define as_pmap_abi	__set_un.__set_pmap.__set_abi
#define as_set_brkbase	__set_un.__set_ckptbrk.__set_brkbase
#define as_set_brksize	__set_un.__set_ckptbrk.__set_brksize
#define as_set_vpagg	__set_un.__set_vpagg.__set_vpagg

/*
 * asvo_getrangeattr - get attributes of a range of addresses
 */
typedef enum {
	AS_GET_ADDR,	/* allocate an address range */
	AS_GET_PGD,	/* get page table info */
	AS_GET_VTOP,	/* virtual to phys mappings */
	AS_GET_PDE,	/* virtual to pde */
	AS_GET_KPTE,	/* virtual to equiv kpte */
	AS_GET_ALEN	/* virtual to alenlist */
} as_getrangeattrop_t;

typedef union {
	struct {		/* AS_GET_VTOP */
		pfn_t *as_ppfn;
                size_t *as_ppgsz;
		int as_step;
		int as_shift;
		ulong_t as_bits;
	} as_vtop;
	struct {		/* AS_GET_PDE / AS_GET_KPTE */
		union pde *as_ppde;
		ulong_t as_bits;
	} as_pde;
	struct {		/* AS_GET_ALEN */
		struct alenlist_s *as_alenlist;
		unsigned as_flags;
	} as_alen;
	uchar_t as_color;	/* AS_GET_ADDR */
	struct pgd *as_pgd_data;/* AS_GET_PGD */
} as_getrangeattrin_t;

/* returned info */
typedef union {
	uvaddr_t as_addr;	/* AS_GET_ADDR */
	int as_pgd_flush;	/* AS_GET_PGD */
	pgcnt_t as_vtop_nmapped;/* AS_GET_VTOP */
} as_getrangeattr_t;

/*
 * asvo_setrangeattr - set attributes on a range of addresses
 */
typedef enum {
	AS_LOCK,	/* lock range */
	AS_UNLOCK,	/* unlock range */
	AS_LOCKALL,	/* lock everything */
	AS_UNLOCKALL,	/* unlock everything */
	AS_PROT,	/* set protections on range */
	AS_CACHECTL,	/* set cache algorithm on range */
	AS_LOCK_BY_ATTR,
	AS_UNLOCK_BY_ATTR,
	AS_MAKE_PRIVATE,/* combines old ispcwritable/getprivatespace */
	AS_SYNC,	/* push back to memory object */
	AS_CACHEFLUSH,	/* flush cache for range */
	AS_ADVISE,	/* offer advice */
	AS_USYNC	/* set a range as being a handle for usync */
} as_setrangeattrop_t;

typedef struct {
	as_setrangeattrop_t as_op;		/* what to set */
	union {
		struct { 	/* AS_PROT */
			mprot_t __prot;		/* protections for AS_PROT */
			int __flags;
		} __set_prot;
		struct { 	/* AS_LOCK/UNLOCK_BY_ATTR */
			int __attr;
			as_mohandle_t __handle; /* handle for shm memobj */
		} __set_lock;
		struct { 	/* AS_SYNC/AS_CACHEFLUSH/AS_ADVISE */
			uint_t __flags;
		} __set_sync;
		struct {	/* AS_CACHECTL */
			ulong_t __cache;
		} __set_cachectl;
	} __set_un;
} as_setrangeattr_t;
#define as_prot_prot __set_un.__set_prot.__prot
#define as_prot_flags __set_un.__set_prot.__flags
#define as_lock_attr __set_un.__set_lock.__attr
#define as_lock_handle __set_un.__set_lock.__handle
#define as_sync_flags __set_un.__set_sync.__flags
#define as_cache_flags __set_un.__set_sync.__flags
#define as_advise_bhv __set_un.__set_sync.__flags
#define as_cachectl_flags __set_un.__set_cachectl.__cache

/* values for as_lock_attr */
#define AS_DATALOCK	0x1
#define AS_TEXTLOCK	0x2
#define AS_PROCLOCK	0x4
#define AS_SHMLOCK	0x8
#define AS_FUTURELOCK	0x10

/* values for as_sync_flags */
#define AS_TOSS		0x1	/* don't write back pages, just toss */

/* result from setrangeattr */
typedef struct {
	union {
		struct {	/* AS_USYNC */
			off_t	__off;
			void	*__handle;
		} __set_usync;
	} __set_un;
} as_setrangeattrres_t;

#define as_usync_off	__set_un.__set_usync.off
#define as_usync_handle	__set_un.__set_usync.handle

/*
 * asvo_shake ops
 */
typedef enum {
	AS_SHAKEANON,
	AS_SHAKESWAP,
	AS_SHAKERSS,
	AS_SHAKETSAVE,
	AS_SHAKEAGE,
	AS_SHAKEPAGE
} as_shakeop_t;

typedef union {
	struct {	/* AS_SHAKESWAP */
		int __hard;
		int __lswap;
	} __shakeswap;
	struct {	/* AS_SHAKETSAVE */
		int __flags;
	} __shaketsave;
	struct {	/* AS_SHAKEPAGE */
		short __pri;
		short __state;
		time_t __slptime;
	} __shakepage;
} as_shake_t;

#define as_shakeswap_hard __shakeswap.__hard
#define as_shakeswap_lswap __shakeswap.__lswap
#define as_shaketsave_flags __shaketsave.__flags
#define as_shakepage_pri __shakepage.__pri
#define as_shakepage_state __shakepage.__state
#define as_shakepage_slptime __shakepage.__slptime

/* flags for AS_SHAKETSAVE */
#define AS_NOFLUSH	0x1
#define AS_TSAVE_LOCKED 0x2	/* aspacelock already held */

/*
 * asvo_watch decls
 */
typedef enum {
	AS_WATCHCLRSYS,
	AS_WATCHCANCELSKIP,
	AS_WATCHSET,
	AS_WATCHCLR
} as_watchop_t;

typedef union {
	struct {		/* AS_WATCHSET, AS_WATCHCLR */
		watch_t __where;
	} __setclrwatch;
} as_watch_t;

#define as_setclrwatch_where __setclrwatch.__where

/*
 * asvo_share decls
 */
typedef enum {
	AS_PFLIP,		/* flip kernel to user */
	AS_PATTACH,		/* attach user to kernel */
	AS_PSYNC		/* sync TLBs after PFLIPs and PATTACHs */
} as_shareop_t;

typedef union {
	struct {		/* AS_PATTACH */
		uvaddr_t as_vaddr;
		int as_dosync;
	} as_share_pattach;
	struct {		/* AS_PFLIP */
		caddr_t as_kvaddr;
		uvaddr_t as_tgtaddr;
		int as_dosync;
	} as_share_pflip;
} as_share_t;

typedef union {
	caddr_t as_kvaddr;	/* kernel address - AS_PATTACH */
} as_shareres_t;

/*
 * declarations for asvo_verifyvnmap -- note that vaddr, len, and vnoffset
 * come back from verifyvnmap as page-aligned values.
 */
typedef struct vnmap {
	uvaddr_t	vnmap_vaddr;	/* starting user vaddr of mapping */
	size_t		vnmap_len;	/* mapping length in bytes */
	int		vnmap_flags;	/* AS_VNMAP_* */
	uvaddr_t	vnmap_ovvaddr;	/* first address that overlaps */
	size_t		vnmap_ovlen;	/* length of overlapping in bytes */
	off_t		vnmap_ovoffset;	/* starting file offset map/overlap */
} vnmap_t;

/*
 * note -- all virtual addresses should be page aligned
 */
struct vnode;

typedef struct as_verifyvnmap {
	struct vnode	*as_vp;		/* vnode to be checked */
	uvaddr_t	as_vaddr;	/* start of address range */
	size_t		as_len;		/* length of address range */
	off_t		as_offstart;	/* starting i/o offset */
	off_t		as_offend;	/* ending i/o offset */
	vnmap_t		*as_vnmap;	/* pointer to allocated vnmaps */
	int		as_nmaps;	/* # of maps in *as_vnmap */
} as_verifyvnmap_t;

typedef struct as_verifyvnmapres {
	uint		as_rescodes;	/* cumulative value of vnmap_flags */
	int		as_nmaps;	/* number of maps returned */
	vnmap_t		*as_multimaps;	/* to return more maps than expected */
	int		as_mapsize;	/* size of returned multimap in bytes */
	off_t		as_maxoffset;	/* the largest file offset touched */
					/*  by the biomove */
} as_verifyvnmapres_t;

/*
 * flags for as_rescodes and vnmap_flags
 */
#define AS_VNMAP_FOUND		0x0001	/* found a mapped vnode */
#define AS_VNMAP_AUTOGROW	0x0002	/* at least one found map is autogrow */
#define AS_VNMAP_OVERLAP	0x0004	/* found at least one overlap */

/*
 * values and types for asvo_ops
 */
/* values for asvo_grow */
typedef enum {
	AS_GROWSTACK,
	AS_GROWBRK
} as_growop_t;

typedef enum {
	AS_SHARED,
	AS_EXCL
} as_lockop_t;

/*
 * Address Space Virtual Ops
 */
struct utas_s;
typedef struct asvo_ops_s {
	bhv_position_t	asv_position;	/* position within behavior chain */
	void (*asvo_lock)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_lockop_t);		/* AS_SHARED/AS_EXCL */
	int (*asvo_trylock)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_lockop_t);		/* AS_SHARED/AS_EXCL */
	int (*asvo_islocked)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_lockop_t);		/* AS_SHARED/AS_EXCL */
	void (*asvo_unlock)(
		bhv_desc_t *,		/* behavior */
		aspasid_t);		/* private AS id */

	int (*asvo_fault)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_faultop_t,
		as_fault_t *,		/* input info */
		as_faultres_t *);	/* result info */

	/*
	 * asvo_klock - kernel version of locking - faster.
	 * used by useracc and friends
	 * The flags indicate whether AS_LOCK/AS_UNLOCK and direction
	 * (AS_READ/AS_WRITE)
	 */
	int (*asvo_klock)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		uvaddr_t,		/* start address */
		size_t,			/* # of bytes */
		int,			/* flags/direction */
		int *);			/* cookie */
#define AS_KLOCK	0x1
#define AS_KUNLOCK	0x2
#define AS_READ		0x4		/* read from outside & write memory */
#define AS_WRITE	0x8		/* read from memory and write outside */
	
	/*
	 * asvo_addspace - generic routine to add mappings
	 * different things happen based on the mapping type. Some are
	 * pretty specific..
	 * Subsumes (parts of) specmap(),execmap(), fs_mapsubr()
	 */
	int (*asvo_addspace)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_addspace_t *,	/* parameters */
		as_addspaceres_t *);	/* OUT:returned info */
	/*
	 * asvo_deletespace - remove parts of address space
	 * (munmap, exec)
	 */
	int (*asvo_deletespace)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_deletespace_t *,	/* parameters */
		as_deletespaceres_t *);	/* OUT:returned info */

	int (*asvo_grow)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_growop_t,		/* type */
		uvaddr_t);		/* new address */

	/*
	 * asvo_procio - read/write bytes from one address space into
	 * a buffer
	 * For non-valid pages this is done via a surrogate rather than
	 * attaching into the callers space. This surrogate can run on
	 * the local node.
	 */
	int (*asvo_procio)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		uio_t *,		/* target space to read/write */
		void *,			/* result/data buffer */
		size_t);		/* length of buffer */

	/*
	 * asvo_new - do fork/sproc/uthread processing
	 */
	int (*asvo_new)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_new_t *,		/* input parms */
		as_newres_t *);		/* result info */
	/*
	 * asvo_validate - validate an address change. optionally perform
	 * any auto-grow that needs to happen
	 */
	int (*asvo_validate)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		uvaddr_t,		/* address */
		size_t,			/* length */
		uint_t);		/* flag */
#define AS_DOGROW	0x1	/* perform autogrow if addr not valid */
	int (*asvo_getasattr)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		uint_t,			/* mask of what to get */
		as_getasattr_t *);	/* return value */
	int (*asvo_getvaddrattr)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		uint_t,			/* mask of what to get */
		uvaddr_t,		/* info about this address */
		as_getvaddrattr_t *);	/* return value */
		
	/*
	 * asvo_getattr - complex queries
	 */
	int (*asvo_getattr)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_getattrop_t,		/* what to get */
		as_getattrin_t *,	/* input parameters */
		as_getattr_t *);	/* return value */
	int (*asvo_setattr)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		uvaddr_t,		/* address */
		as_setattr_t *); /* what to set & info about setting */

	/*
	 * asvo_getrangeattr - get attributes covering a range of address space
	 * Covers:
	 *	virtual to phys mappings (vtopv)
	 */
	int (*asvo_getrangeattr)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		uvaddr_t,		/* address */
		size_t,			/* length */
		as_getrangeattrop_t,	/* what to get */
		as_getrangeattrin_t *,	/* info about what to get */
		as_getrangeattr_t *);	/* returned info */
	/*
	 * asvo_setrangeattr - set attributes covering a range of address space
	 * Covers:
	 *	locking (mpin)
	 *	protections (mprotect)
	 *	caching
	 *	sync/flush (madvise/msync)
	 */
	int (*asvo_setrangeattr)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		uvaddr_t,		/* address */
		size_t,			/* length */
		as_setrangeattr_t *,	/* info about what to set */
		as_setrangeattrres_t *);/* returned info */

	/*
	 * asvo_lookuppasid - lookup to see if the pasid is valid
	 */
	int (*asvo_lookuppasid)(
		bhv_desc_t *,		/* behavior */
		aspasid_t);		/* private AS id */

	/*
	 * asvo_relepasid - release ref on pasid
	 */
	void (*asvo_relepasid)(
		bhv_desc_t *,		/* behavior */
		aspasid_t);		/* private AS id */
	
	/*
	 * asvo_destroy - destroy the aso
	 */
	void (*asvo_destroy)(
		bhv_desc_t *,		/* behavior */
		aspasid_t);		/* private AS id */
	
	/*
	 * asvo_shake - 'shake' various parts of address space
	 */
	int (*asvo_shake)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_shakeop_t,		/* type of shake */
		as_shake_t *);		/* parms */

	/*
	 * asvo_watch - watchpoint operations
	 */
	int (*asvo_watch)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_watchop_t,		/* watchpoint op */
		as_watch_t *);		/* parms */
	/*
	 * asvo_share - 'share' pages across kernel/user
	 */
	int (*asvo_share)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_shareop_t,		/* what? */
		as_share_t *,		/* input parms */
		as_shareres_t *);	/* output parms */

	/*
	 * probe for memory ranges mapped to a vnode
	 */
	int (*asvo_verifyvnmap)(
		bhv_desc_t *,		/* behavior */
		aspasid_t,		/* private AS id */
		as_verifyvnmap_t *,	/* parameters */
		as_verifyvnmapres_t *);	/* return values */
} asvo_ops_t;

static __inline void
VAS_LOCK(vasid_t v, as_lockop_t a)
{
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	(*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_lock)(
		bhvh->bh_first, v.vas_pasid, a);
	BHV_READ_UNLOCK(bhvh);
}

static __inline int
VAS_TRYLOCK(vasid_t v, as_lockop_t a)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_trylock)(
		bhvh->bh_first, v.vas_pasid, a);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_ISLOCKED(vasid_t v, as_lockop_t a)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_islocked)(
		bhvh->bh_first, v.vas_pasid, a);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline void
VAS_UNLOCK(vasid_t v)
{
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	(*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_unlock)(
		bhvh->bh_first, v.vas_pasid);
	BHV_READ_UNLOCK(bhvh);
}

static __inline int
VAS_FAULT(vasid_t v, as_faultop_t op, as_fault_t *as, as_faultres_t *asres)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_fault)(
		bhvh->bh_first, v.vas_pasid, op, as, asres);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_KLOCK(vasid_t v, uvaddr_t vaddr, size_t len, int flags, int *cookie)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_klock)(
		bhvh->bh_first, v.vas_pasid, vaddr, len, flags, cookie);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_ADDSPACE(vasid_t v, as_addspace_t *as, as_addspaceres_t *asres)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_addspace)(
		bhvh->bh_first, v.vas_pasid, as, asres);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_DELETESPACE(vasid_t v, as_deletespace_t *as, as_deletespaceres_t *asres)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_deletespace)(
		bhvh->bh_first, v.vas_pasid, as, asres);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_GROW(vasid_t v, as_growop_t a, uvaddr_t uva)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_grow)(
		bhvh->bh_first, v.vas_pasid, a, uva);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_PROCIO(vasid_t v, uio_t *uio, void *r, size_t len)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_procio)(
		bhvh->bh_first, v.vas_pasid, uio, r, len);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_NEW(vasid_t v, as_new_t *asn, as_newres_t *asnres)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_new)(
		bhvh->bh_first, v.vas_pasid, asn, asnres);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_VALIDATE(vasid_t v, uvaddr_t vaddr, size_t len, uint_t flags)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_validate)(
		bhvh->bh_first, v.vas_pasid, vaddr, len, flags);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_GETASATTR(vasid_t v, uint_t mask, as_getasattr_t *as)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_getasattr)(
		bhvh->bh_first, v.vas_pasid, mask, as);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_GETVADDRATTR(vasid_t v, uint_t mask, uvaddr_t uvaddr, as_getvaddrattr_t *as)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_getvaddrattr)(
		bhvh->bh_first, v.vas_pasid, mask, uvaddr, as);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_VERIFYVNMAP(vasid_t v, as_verifyvnmap_t *in, as_verifyvnmapres_t *res)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_verifyvnmap)(
		bhvh->bh_first, v.vas_pasid, in, res);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_GETATTR(vasid_t v, as_getattrop_t op, as_getattrin_t *in, as_getattr_t *as)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_getattr)(
		bhvh->bh_first, v.vas_pasid, op, in, as);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_SETATTR(vasid_t v, uvaddr_t vaddr, as_setattr_t *as)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_setattr)(
		bhvh->bh_first, v.vas_pasid, vaddr, as);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_GETRANGEATTR(
	vasid_t v,
	uvaddr_t vaddr,
	size_t len,
	as_getrangeattrop_t op,
	as_getrangeattrin_t *in,
	as_getrangeattr_t *attr)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_getrangeattr)(
		bhvh->bh_first, v.vas_pasid, vaddr, len, op, in, attr);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_SETRANGEATTR(
	vasid_t v,
	uvaddr_t vaddr,
	size_t len,
	as_setrangeattr_t *as,
	as_setrangeattrres_t *asres)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_setrangeattr)(
		bhvh->bh_first, v.vas_pasid, vaddr, len, as, asres);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_LOOKUPPASID(vasid_t v)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_lookuppasid)(
		bhvh->bh_first, v.vas_pasid);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline void
VAS_RELEPASID(vasid_t v)
{
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	(*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_relepasid)(
		bhvh->bh_first, v.vas_pasid);
	BHV_READ_UNLOCK(bhvh);
}

static __inline void
VAS_DESTROY(vasid_t v)
{
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	/* no locking since ref count must be zero */
	(*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_destroy)(
		bhvh->bh_first, v.vas_pasid);
}

static __inline int
VAS_SHAKE(vasid_t v, as_shakeop_t op, as_shake_t *shake)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_shake)(
		bhvh->bh_first, v.vas_pasid, op, shake);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_WATCH(vasid_t v, as_watchop_t op, as_watch_t *watch)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_watch)(
		bhvh->bh_first, v.vas_pasid, op, watch);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

static __inline int
VAS_SHARE(vasid_t v, as_shareop_t op, as_share_t *as, as_shareres_t *asres)
{
	int rv;
	bhv_head_t *bhvh = &v.vas_obj->vas_bhvh;

	BHV_READ_LOCK(bhvh);
	rv = (*((asvo_ops_t *)bhvh->bh_first->bd_ops)->asvo_share)(
		bhvh->bh_first, v.vas_pasid, op, as, asres);
	BHV_READ_UNLOCK(bhvh);
	return rv;
}

/*
 * AS manager functions
 */
struct utas_s;
struct uthread_s;
struct aspm;
struct pm;
struct eframe_s;

extern asid_t as_create(struct utas_s *, vasid_t *, int);

/*
 * as_exec_export - called to package up all state from the 'old' AS that
 * needs to be passed to the 'new' AS
 * This also calls as_rele on the AS since there is no more
 * good state left
 */
typedef struct as_exec_state_s {
	rlim_t as_vmemmax;
	rlim_t as_datamax;
	rlim_t as_stkmax;
	rlim_t as_rssmax;
	rlim_t as_maxrss;
	void *as_tsave;
	void *as_pmap;
	struct vpagg_s *as_vpagg;
	pgno_t as_smem_reserved;
} as_exec_state_t;
void as_exec_export(as_exec_state_t *, asid_t *);
int as_exec_import(vasid_t *, asid_t *, struct utas_s *, as_exec_state_t *,
				int, struct aspm *, struct pm **);

/*
 * as_lookup - lookup an asid - this is for 3rd aprty accesses to an address
 * space. It returns 0 if the asid_t was valid, an error
 * otherwise. On a successful return, a vasid_t with a reference count is
 * filled in
 * Current process access to its address space should use as_lookup_current()
 */
extern int as_lookup(asid_t, vasid_t *);
extern void as_rele(vasid_t);
extern int as_lookup_current(vasid_t *);

/* helper functions */
union pde;
extern int as_getassize(asid_t, pgcnt_t *, pgcnt_t *);
extern void isolatereg(struct uthread_s *, cpuid_t);
extern void unisolatereg(struct uthread_s *, cpuid_t);
extern void removetsave(int);
extern int chk_kuseg_abi(int, uvaddr_t, size_t);
extern int as_allocshmmo(struct vnode *, int, as_mohandle_t *);
extern void as_freeshmmo(as_mohandle_t);
extern int as_shmmo_refcnt(as_mohandle_t);
extern int pfault(uvaddr_t, struct eframe_s *, int *, int);
extern int vfault(uvaddr_t, int, struct eframe_s *, int *);
extern int tfault(struct eframe_s *, uint, uvaddr_t, int *);
extern caddr_t pattach(uvaddr_t, int);
extern void prelease(caddr_t);
extern caddr_t pflip(caddr_t, uvaddr_t, int);
extern void psync(void);
extern int vtop(uvaddr_t, size_t, pfn_t *, int);
extern int vtopv(uvaddr_t, size_t, pfn_t *, int, int, int);
extern int maputokptbl(vasid_t, uvaddr_t, size_t, caddr_t, ulong_t);
extern void vaddr_to_pde(uvaddr_t, union pde *);

/*
 * Routines that work on all addresses spaces.
 */
typedef enum {
	AS_RSSSCAN,
	AS_SWAPSCAN,
	AS_ANONSCAN,
	AS_AGESCAN,
	AS_STEALSCAN
} as_scanop_t;

typedef union {
	as_shake_t as_scan_shake;
	int foo;
} as_scan_t;

/* values for flags to as_scan */
#define AS_SCAN_LOCAL	0x0001	/* scan local AS's only */

extern int as_scan(as_scanop_t, int, as_scan_t *);

/*
 * Must typecast as_obj and as_pasid to volatile
 * because compiler optimization can reverse the order
 * of the two store operations, and thus open a window
 * that allows a race condition to occur when AS_ISNULL()
 * is called
 */

#define AS_NULL(a)	\
	(((volatile asid_t*)(a))->as_obj = (void *)(__psunsigned_t)0xf, \
	((volatile asid_t*)(a))->as_pasid = 0)

#define AS_ISNULL(a)	((a)->as_obj == (struct __as_opaque *)(__psunsigned_t)0xf)

/* sigh - need vasid_t type ... */
struct uthread_s;
void clr3rdparty(struct uthread_s *, vasid_t);

struct prda;
/* other transition functions */
extern int lockprda(struct prda **);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_AS_H__ */
