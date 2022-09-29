#ifndef _KLIB_H
#define _KLIB_H
#ifdef __cplusplus
extern "C" {
#endif
/* Copyright (C) 1989 Silicon Graphics, Inc. All rights reserved.  */
/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/include/RCS/klib.h,v 1.1 1999/05/25 19:19:20 tjm Exp $" */

/**
 **	This header file contains basic definitions and declarations
 **	for libklib. 
 **
 **/

#include <sys/dump.h>  /* To pick up the definition for dump_hdr_t */

#define HUBMASK1	0xf000000000000000
#define HUBMASK2	0xff00000000000000

    				/* the following is copied directly from
				 * from <sys/SN/SN0/addrs.h>
				 */
#define HSPEC_BASE	0x9000000000000000
#define IO_BASE		0x9200000000000000
#define MSPEC_BASE	0x9400000000000000
#define UNCAC_BASE	0x9600000000000000


#define FALSE   0
#define TRUE    1

/* Flags that can be passed to various functions. Applications can
 * make use of these flags when calling kl_readmem(), kl_get_struct(),
 * etc.
 */
#define K_TEMP	1
#define K_PERM  2
#define K_ALL	4

#define IRIX6_2			2
#define IRIX6_3			3
#define IRIX6_4			4
#define IRIX6_5			5
#define IRIX_SPECIAL	0

/* Global klib flags that allow applications to control klib behavior
 */
#define K_IGNORE_FLAG		1	/* Global ignore flag           */
#define K_IGNORE_MEMCHECK	2	/* Ignore memory validity check	*/
#define K_ICRASHDEF_FLAG	4	/* Use alternate icrashdef from file */

/** 
 ** Basic types 
 **/
typedef int                 int32;
typedef unsigned int        uint32;
typedef int                 bool;
#if (_MIPS_SZLONG == 64)
typedef long                int64;
typedef unsigned long       uint64;
#else /* 32-bit */
typedef long long           int64;
typedef unsigned long long  uint64;
#endif

/** 
 ** Special klib types
 **/

/* The following typedef should be used for variables or return values
 * that contain kernel virtual or physical addresses. This is true for
 * both 32-bit and 64-bit kernels (one size fits all). With 32-bit
 * values, the upper order 32 bits will always contain zeros.
 */
typedef uint64              kaddr_t;

/* The following typedef should be used for variables or return values
 * that contain unsigned integer values in a kernel structs or unions.
 * The functions that obtain the values determine their size (one byte, 
 * two bytes, four bytes, or eight bytes) and shift the value accordingly 
 * to fit in the 64-bit space. Using this approach allows klib-based 
 * applications to be much more flexible. They can use a single set of 
 * functions to handle both 32-bit and 64-bit dumps. And, they are less 
 * likely to be effected by minor changes to kernel structures (e.g. 
 * changing a short to an int). typedef unsigned long long k_uint_t;
 */
typedef unsigned long long	k_uint_t;  

/* The following typedef is similar to the previous one except it applys
 * to signed integer values.
 */

typedef long long 		 	k_int_t;  

/* The following typedef should be used for variables that will point
 * to a block of memory containing data read in rom the vmcore image 
 * (or live system). Many klib functions have k_ptr_t parameters and 
 * return k_ptr_t values.
 */
typedef void 		       *k_ptr_t;  

/* Generalized macros for pointing at different data types at particular
 * offsets in kernel structs.
 */
#define ADDR(K, p, s, f)  	((unsigned)(p) + kl_member_offset(K, s, f))
#define K_PTR(K, p, s, f)   ((k_ptr_t)ADDR(K, p, s, f))
#define CHAR(K, p, s, f)    ((char *)ADDR(K, p, s, f))

#define PTRSZ64(K) ((K_PTRSZ(K) == 64) ? 1 : 0)
#define PTRSZ32(K) ((K_PTRSZ(K) == 32) ? 1 : 0)

/* Macros that eliminate the offset paramaters to the kl_uint() and kl_int()
 * functions (just makes things cleaner looking)
 */
#define KL_UINT(K, p, s, m) kl_uint(K, p, s, m, 0)
#define KL_INT(K, p, s, m) kl_int(K, p, s, m, 0)

/* Basic address macros for use with multiple platforms
 */
#define k0_to_phys(K, X) ((kaddr_t)(X) & K_TO_PHYS_MASK(K))
#define k1_to_phys(K, X) ((kaddr_t)(X) & K_TO_PHYS_MASK(K))
#define is_kseg0(K, X) \
	((kaddr_t)(X) >= K_K0BASE(K) && (kaddr_t)(X) < K_K0BASE(K) + K_K0SIZE(K))
#define is_kseg1(K, X) \
	((kaddr_t)(X) >= K_K1BASE(K) && (kaddr_t)(X) < K_K1BASE(K) + K_K1SIZE(K))
/* TFP */
#define is_kseg2(K, X) \
	((kaddr_t)(X) >= K_K2BASE(K) && (kaddr_t)(X) < K_K2BASE(K) + K_K2SIZE(K))
/* all else */
#define _is_kseg2(K, X) \
	((kaddr_t)(X) >= K_K2BASE(K) && (kaddr_t)(X) < K_KPTE_SHDUBASE(K))

#define is_kpteseg(K, X) ((kaddr_t)(X) >= K_KPTE_SHDUBASE(K) && \
	 (kaddr_t)(X) < K_KPTEBASE(K) + K_KUSIZE(K))

#define IS_KERNELSTACK(K, addr) (K_EXTUSIZE(K) ? \
	 ((addr >= K_KEXTSTACK(K)) && (addr < K_KERNELSTACK(K))) : \
	((addr >= K_KERNELSTACK(K)) && (addr < K_KERNSTACK(K))))

#define Btoc(K, X) ((kaddr_t)(X) >> K_PNUMSHIFT(K))
#define Ctob(K, X) ((kaddr_t)(X) << K_PNUMSHIFT(K))
#define Pnum(K, X) ((kaddr_t)(X) >> K_PNUMSHIFT(K))

/* 
 * MACRO's mostly taken from immu.h. Why ?. 
 * Because we like it that's why. 
 * We have it in the #if for those icrash files which include the 
 * kernel header files with these defines..
 */
#if !defined(KERNEL) && !defined(_KERNEL)
#define BTOST(x)          (uint)((kaddr_t)(x) / K_NBPS(klp))
#define BTOS(x)           (uint)(((kaddr_t)(x) + (K_NBPS(klp)-1))/ K_NBPS(klp))
#define STOB(x)           (kaddr_t)((kaddr_t)(x) * K_NBPS(klp))
#endif /* !defined(KERNEL) && !defined(_KERNEL) */
#define KL_HIUSRATTACH_32    0x7fff8000
#define KL_HIUSRATTACH_64    0x10000000000
/* this is the same as Number of page tbls to map user space. */
/* the same as NPGTBLS */
#define KL_SEGTABSIZE        BTOS(KL_HIUSRATTACH_32) 
#define KL_SEGTABMASK        (KL_SEGTABSIZE-1)
/*
 * This is defined as btobasetab in immu.h .. but this is the correct meaning..
 * i.e., it returns an index into the base table.
 */
#define KL_BTOBASETABINDEX(x)      \
  ((__uint64_t)(x) / (K_NBPS(klp)*KL_SEGTABSIZE))
#define KL_BTOSEGTABINDEX(x)       \
  (BTOST(x) & KL_SEGTABMASK)
#define KL_PHYS_TO_K0(X)            (X | K_K0BASE(K))

/*
 * Platform independent defines.
 */
#define VALID_PFN(PFN)           valid_pfn(PFN)
#define NEXT_PFDAT(FIRST,LAST)           \
 ((PLATFORM_SNXX) ? sn0_next_valid_pfdat(FIRST,LAST) : (FIRST + PFDAT_SIZE(K)))
#define KL_PFDATTOPFN(PFDAT) kl_pfdattopfn(PFDAT)
#define KL_PFNTOPFDAT(PFN)   kl_pfntopfdat(PFN)
#define KL_SLOT_SIZE         (1LL << K_SLOT_SHIFT(K))
		
#define KL_SLOT_PFNSHIFT      (K_SLOT_SHIFT(K) - K_PNUMSHIFT(K))
#define KL_PFN_TO_SLOT_NUM(PFN)      \
 ( ((PFN) >> SLOT_PFNSHIFT) & K_SLOT_BITMASK(K) )
#define KL_PFDAT_TO_SLOT_NUM(PFD)    \
 (PFN_TO_SLOT_NUM(KL_PFDATTOPFN(PFD)))

#define KL_PFN_TO_SLOT_OFFSET(PFN)   \
 ((PFN) & ((KL_SLOT_SIZE/K_NBPC(K))-1))
#define KL_PFDAT_TO_SLOT_OFFSET(PFD) \
 (PFN_TO_SLOT_OFFSET(KL_PFDATTOPFN(PFD)))


#define KL_PFN_NASIDSHIFT   (K_NASID_SHIFT(K) - K_PNUMSHIFT(K))
#define KL_PFN_NASIDMASK    (K_NASID_BITMASK(K) << KL_PFN_NASIDSHIFT)
#define KL_PFD_NASIDMASK    (K_NASID_BITMASK(K) << K_NASID_SHIFT(K))
  
#define KL_PFN_NASID(pfn)            \
  (PLATFORM_SNXX ? ((pfn) >> KL_PFN_NASIDSHIFT) : 0)


#define Kvtokptbl(K, X) (&_kptbl[Pnum((kaddr_t)(X) - (kaddr_t)K_K2BASE(K)])

#define IS_HUBSPACE(K, X) (((kaddr_t)X & HUBMASK1) == HSPEC_BASE)
#define IS_PHYSICAL(K, X) ((kaddr_t)X <= (kaddr_t)K_MAXPHYS(K))
#define IS_SMALLMEM(K, X) (IS_PHYSICAL(K, X) ? \
        (Btoc(K, X) < 0x20000) : \
		(Btoc(K, kl_virtop(K, X, (k_ptr_t)NULL)) < 0x20000))

#define _PTE_TO_PFN(K, PTE) (uint)(((PTE) & K_PFN_MASK(K)) >> K_PFN_SHIFT(K))
#define PTE_TO_PFN pte_to_pfn
#define PTE_TO_K0(K, PTE) (Ctob(K, PTE_TO_PFN(PTE)) | K_K0BASE(K))
#define PFN_TO_PHYS(K, PFN) Ctob(K, PFN)
#define PFN_TO_K0(K, PFN) \
	((PFN <= Pnum(K, K_K1SIZE(K))) ? (Ctob(K, PFN) | K_K0BASE(K)) : 0)
#define PFN_TO_K1(K, PFN) \
	((PFN <= Pnum(K, K_K1SIZE(K))) ? (Ctob(K, PFN) | K_K1BASE(K)) : 0)

#define PROC_TO_USER(p) KL_UINT((p), "proc", "p_user")

/* Environment types that kthread can support
 */
#ifdef ANON_ITHREADS
#define KT_ITHREAD  1
#define KT_XTHREAD  4
#else
#define KT_XTHREAD  1
#endif
#define KT_STHREAD  2
#define KT_UTHREAD  3

#ifdef ANON_ITHREADS
#define IS_ITHREAD(K, ktp) \
    (KL_UINT(K, ktp, "kthread", "k_type") == KT_ITHREAD)
#endif

#define IS_STHREAD(K, ktp) \
    (KL_UINT(K, ktp, "kthread", "k_type") == KT_STHREAD)

#define IS_UTHREAD(K, ktp) \
    (KL_UINT(K, ktp, "kthread", "k_type") == KT_UTHREAD)

#define IS_XTHREAD(K, ktp) \
    (KL_UINT(K, ktp, "kthread", "k_type") == KT_XTHREAD)

/* Flag values used by the addr_convert() function
 */
#define PFN_FLAG                1
#define PADDR_FLAG              2
#define VADDR_FLAG              3

/* Flag that tells kl_is_valid_kaddr() to perform a word aligned check
 */
#define WORD_ALIGN_FLAG			1

/* XXX - Flag value necessary for the klib_cmp.c module
 */
#define REPORT_FLAG	1

/* Definitions for compressed cached reads.  I've recently lowered
 * these ... If they need to be increased later, I'll do so.
 */
#define CMP_HIGH_WATER_MARK 25
#define CMP_LOW_WATER_MARK  10

#define CMP_VM_CACHED   0x01
#define CMP_VM_UNCACHED 0x02

/* 64-bit macros.
 */
#define LSEEK(K, X, Y, Z) \
	(PTRSZ64(K) ? lseek64(X,(off64_t)Y, Z) : lseek(X, (off_t)Y, Z))

/* CORE_TYPE indicates type of corefile
 */
typedef enum {
	dev_kmem,   /* image of /dev/kmem, a running kernel */
	reg_core,   /* Regular (uncompressed) core file */
	cmp_core    /* compressed core file */
} CORE_TYPE;

/* PANIC_TYPE indicates type of panic 
 */
typedef enum {
	reg_panic,   /* internal panic */
	nmi_panic,   /* NMI induced core dump */
} PANIC_TYPE;

/**
 ** Callback function typedefs
 **
 ** Note: Callback functions returning negative numbers are
 **       treated as errors
 **/

/* Call back function 1: Given symbol name, return address
 * Function should return -1 in case of error.
 */
typedef int (*klib_sym_addr_func) (
    void*       /* app_ptr: pointer given to klib_add_callbacks */,
    char*       /* name of symbol */,
    uint64*     /* address of symbol */
    );

/* Call back function 2: Given struct name, return length
 * Function should return -1 in case of error.
 */
typedef int32 (*klib_struct_len_func) (
    void*       /* app_ptr: pointer given to klib_add_callbacks */,
    char*       /* name of struct */,
    int32*      /* length of struct */
    );

/* Call back function 3: Given struct name and member name,
 * return offset of member in struct
 * Function should return -1 in case of error.
 */
typedef int32 (*klib_member_offset_func) (
    void*       /* app_ptr: pointer given to klib_add_callbacks */,
    char*       /* name of struct */,
    char*       /* name of member */,
    int32*      /* offset of member */
    );

/* Call back function 4: Given struct name and member name,
 * return length of member in bytes
 * Function should return -1 in case of error.
 */
typedef int32 (*klib_member_size_func) (
    void*       /* app_ptr: pointer given to klib_add_callbacks */,
    char*       /* name of struct */,
    char*       /* name of member */,
    int32*      /* length of member in bytes */
    );

/* Call back function 5: Given struct name and member name,
 * return length of member in bits
 * Function should return -1 in case of error.
 */
typedef int32 (*klib_member_bitlen_func) (
    void*       /* app_ptr: pointer given to klib_add_callbacks */,
    char*       /* name of struct */,
    char*       /* name of member */,
    int32*      /* length of member in bits */
    );

/* Call back function 6: Given struct name and member name,
 * return member base value
 * Function should return -1 in case of error.
 */
typedef int32 (*klib_member_base_value_func) (
    void*       /* app_ptr: pointer given to klib_add_callbacks */,
    char*       /* name of struct */,
    char*       /* name of member */,
    k_uint_t*   /* length of member in bits */
    );

/* Call back function 7: Memory block allocator
 * Function should return 0 in case of error.
 */
typedef k_ptr_t (*klib_block_alloc_func) (
    void*       /* app_ptr: pointer given to klib_add_callbacks */,
    int32       /* size of block required */,
    int32       /* flag value (K_TEMP/K_PERM) */
    );

/* Call back function 8: Memory block free
 * Function should return 0 in case of error.
 */
typedef k_ptr_t (*klib_block_free_func) (
    void*       /* app_ptr: pointer given to klib_add_callbacks */,
    k_ptr_t     /* pointer to block */
    );

/* Call back function 9: Application error message printing function
 */
typedef k_ptr_t (*klib_print_error_func) ();

typedef struct banks_s {
    uint    b_size;         /* Size of memory in MB */
    kaddr_t b_paddr;        /* First physical address */
} banks_t;

typedef struct node_memory_s {
    int     n_module;
    char    *n_slot;
    int     n_nodeid;
    int     n_nasid;
    int     n_memsize;
    int     n_numbanks;
    int     n_flag;
    banks_t n_bank[1];
} node_memory_t;

/* Core dump information
 */
typedef struct coreinfo_s {
	CORE_TYPE 		type; 	     /* type of core file                        */
	PANIC_TYPE 		panic_type;  /* type of panic (internal or NMI)          */
	char 		   *corefile;    /* pathname for corefile                    */
	char 		   *namelist;    /* pathname for namelist (unix)             */
	char 		   *icrashdef;   /* pathname for icrashdef file              */
	int32			core_fd;     /* file descriptor for dump file            */
	dump_hdr_t 	   *dump_hdr;    /* dump header information                  */
	int32 			rw_flag;  /* O_RDONLY/O_RDWR (valid only for dev_kmem)   */
} coreinfo_t;

/* System information (hardware configuration, etc.)
 */
typedef struct sysinfo_s {
	void		   *utsname;	    /* Pointer to the utsname struct         */
	int32			IP;			    /* System type (e.g., IP19, IP22, etc.)	 */
	int32  			physmem;        /* physical memory installed (pages)     */
	int32 			numcpus;        /* number of CPUs installed              */
	int32 			maxcpus;        /* Maximum number of CPUs as configured  */

	/* TLB dump information
	 */
	int32 			ntlbentries;    /* number of tlb entries                 */
	int32 			tlbentrysz;     /* size of tlb dump entry                */
	int32 			tlbdumpsize;    /* size of tlb dump (per CPU)            */

	/* Node information
	 */
	int32			numnodes;       /* Number of nodes                       */
	int32			maxnodes;       /* Maximum number of nodes               */
	int32			master_nasid;	/* NASID of master node					 */
	int32 			nasid_shift;    /* NASID_SHIFT                           */
	int32 			slot_shift;     /* SLOT_SHIFT                            */
	int32 			parcel_shift;   /* PARCEL_SHIFT                          */
	int32 			parcel_bitmask;   /* PARCEL_BITMASK                      */
	int32 			parcels_per_slot; /* Number of parcels per slot          */
	int32 			slots_per_node;   /* MD_MEM_BANKS                        */
	uint64 			mem_per_node;     /* Maximum bytes phys memory per node  */
	uint64 			mem_per_slot;   /* Maximum bytes of phys memory per slot */
	uint64 			nasid_bitmask;  /* NASID_BITMASK                         */
	uint64 			slot_bitmask;   /* SLOT_BITMASK                          */

	uint64 			systemsize;     /* Pages of memory installed in system   */
	uint64 			sysmemsize;     /* MB of memory installed in system      */
	kaddr_t 		kend;           /* Pointer to end of kernel              */
	uint64                  mem_per_bank;   /* Maximum bytes of phys memory per bank */
                                             	/* This is a constant equal to 512 MB.   */

	/* XXX - Memory configuration info ??? */

} sysinfo_t;

/* Kernel information
 */
typedef struct kerninfo_s {

	int32			irix_rev;    /* OS revision level (e.g., IRIX6_4)        */
	int32 			syssegsz;    /* size of system segment                   */
	int32 			nprocs;      /* number of configured procs (from var)    */

	/* Values used in virtual to physical address translation, macros, etc.
	 */
	int32 			ptrsz;       /* Number of bytes in a pointer type        */
	int32 			regsz;       /* Number of bytes in a register (always 8) */
	int32 			pagesz;      /* Number of bytes in a page                */
	int32 			nbpw;        /* Number of bytes per word                 */
	int32 			nbpc;        /* Number of bytes per click                */
	int32 			ncps;        /* Number of clics per segment              */
	int32 			nbps;        /* Number of bytes per segment              */
	int32 			pnumshift;   /* PFN shift                                */
	kaddr_t 		to_phys_mask; /* virtual to physical address mask        */
	kaddr_t  		ram_offset;  /* Offset to first memory address           */
	uint64			maxpfn;      /* Largest possible physical page number    */
	kaddr_t  		maxphys;     /* Largest possible physical address        */

	/* Information for accessing mapped pages in the proc/uthread (ublock,
	 * kernelstack, and kextstack).
	 */
	int32 			usize;       /* Number of mapped pages for proc/uthread  */
	int32 			extusize;    /* non-zero - supports kstack extension     */
	int32 			upgidx;      /* Index in p_upgs[] for ublock 			 */
	int32 			kstkidx;     /* Index in p_upgs[] for kernel stack       */
	int32 			extstkidx;   /* Index in p_upgs[] for kstack extension   */

	/* Addresses and sizes of the various address segments
	 */
	kaddr_t 		kubase;      /* Base address of user address space       */
	uint64 			kusize;      /* Size of user segment                     */
	kaddr_t 		k0base;      /* Base address of K0 segment               */
	uint64 			k0size;      /* Size of the K0 segment                   */
	kaddr_t 		k1base;      /* Base address of K1 segment               */
	uint64 			k1size;      /* Size of the K1 segment                   */
	kaddr_t 		k2base;      /* Base address of K2 segment               */
	uint64			k2size;      /* Size of the K2 segment                   */
	kaddr_t 		kernstack;   /* High address of kernel stack             */
	kaddr_t 		kernelstack; /* Start address of kernel stack            */
	kaddr_t 		kextstack;   /* Start address of extension stack         */
	kaddr_t 		kptebase;    /* Start address of K2 memory               */
	kaddr_t 		kpte_usize;
	kaddr_t 		kpte_shdubase;

	/* Node memory information
	 */

	/* Mapped kernel address information
	 */
	kaddr_t 		mapped_ro_base;   /* read-only address base              */
	kaddr_t 		mapped_rw_base;   /* read-write address base             */
	kaddr_t 		mapped_page_size; /* Size of the mapped segment		     */

} kerninfo_t;

/* PTE bit masks and shift values (handles 32-bit and 64-bit PTEs)
 */
typedef struct pdeinfo_s {
	uint64     		pfn_mask;       /* to mask out all but pfn in pte        */
	int32        	pfn_shift;      /* to right align pfn                    */
	uint64     		Pde_pg_vr;      /* pte "valid" mask                      */
	int32      	  	pg_vr_shift;    /* to right align "valid" bits           */
	uint64     		Pde_pg_g;       /* pte "global" mask                     */
	int32        	pg_g_shift;     /* to right align "global" bits          */
	uint64     		Pde_pg_m;       /* pte "modified" mask                   */
	int32        	pg_m_shift;     /* to right align "modified" bits        */
	uint64     		Pde_pg_n;       /* pte "noncached" mask                  */
	int32        	pg_n_shift;     /* to right align "noncached" bits       */
	uint64     		Pde_pg_sv;      /* pte "page valid" mask                 */
	int32        	pg_sv_shift;    /* to right align "page valid" bits      */
	uint64     		Pde_pg_d;       /* pte "page dirty" mask                 */
	int32        	pg_d_shift;     /* to right align "page dirty" bits      */
	uint64     		Pde_pg_eop;     /* pte "eop" mask                        */
	int32        	pg_eop_shift;   /* to right align "eop" bits             */
	uint64     		Pde_pg_nr;      /* pte "reference count" mask            */
	int32        	pg_nr_shift;    /* to right align "reference count" bits */
	uint64     		Pde_pg_cc;      /* pte "cache coherency" mask            */
	int32        	pg_cc_shift;    /* to right align "cache coherency" bits */
} pdeinfo_t;

/* Kernel struct sizes
 */
typedef struct struct_sizes_s {
	int avlnode_size;               /* avlnode struct                        */
	int bhv_desc_size;              /* bhv_desc struct                       */
	int cfg_desc_size;              /* cfg_desc struct                       */
	int cred_size;                  /* cred struct                           */
	int domain_size;                /* domain struct                         */
	int eframe_s_size;              /* eframe_s struct                       */
	int exception_size;             /* exception struct                      */
	int fdt_size;                   /* fdt struct                            */
	int graph_size;               	/* actual size of hwgraph (not struct sz)*/
	int graph_s_size;               /* graph_s struct                        */
	int graph_vertex_group_s_size;  /* graph_vertex_group_s struct           */
	int graph_vertex_s_size;        /* graph_vertex_s struct                 */
	int graph_edge_s_size;          /* graph_edge_s struct                   */
	int graph_info_s_size;          /* graph_info_s struct                   */
	int in_addr_size;               /* in_addr struct                        */
	int invent_meminfo_size;        /* invent_meminfo struct                 */
	int inode_size;                 /* inode struct                          */
	int inpcb_size;                 /* inpcb struct                          */
#ifdef ANON_ITHREADS
	int ithread_s_size;             /* ithread_s struct                      */
#endif
	int kna_size;                   /* kan struct                            */
	int knetvec_size;               /* knetvec struct                        */
	int kthread_size;               /* kthread struct                        */
	int lf_listvec_size;            /* lf_listvec struct                     */
	int mbuf_size;                  /* mbuf struct                           */
	int ml_info_size;               /* ml_info struct                        */
	int ml_sym_size;		/* ml_sym struct                         */
	int mntinfo_size;               /* mntinfo struct 			 */
	int mrlock_s_size;              /* mrlock_s struct                       */
	int module_info_size;           /* module_info struct                    */
	int nodepda_s_size;             /* nodepda_s struct                      */
	int pda_s_size;                 /* pda_s struct                          */
	int pde_size;                   /* pde struct                            */
	int pfdat_size;                 /* pfdat struct                          */
	int pmap_size;                  /* pmap struct                           */
	int pid_entry_size;             /* pid_entry struct                      */
	int pid_slot_size;              /* pid_slot struct                       */
	int pregion_size;               /* pregion struct                        */
	int proc_size;                  /* proc struct                           */
	int proc_proxy_size;            /* proc_proxy struct                     */
	int protosw_size;               /* protosw struct                        */
	int qinit_size;                 /* qinit struct                          */
	int queue_size;                 /* queue struct                          */
	int region_size;                /* pregion struct                        */
	int rnode_size;                 /* rnode struct                          */
	int sema_size;                  /* sema struct                           */
	int lsnode_size;                 /* lsnode struct                          */
	int csnode_size;                 /* csnode struct                          */
	int socket_size;                /* socket struct                         */
	int stdata_size;                /* stdata struct                         */
	int sthread_s_size;             /* sthread_s struct                      */
	int strstat_size;               /* strstat struct                        */
	int swapinfo_size;              /* swapinfo struct                       */
	int tcpcb_size;                 /* tcpcb struct                          */
	int ufchunk_size;               /* ufchunk struct                        */
	int unpcb_size;                 /* unpcb struct                          */
	int uthread_s_size;             /* uthread_s struct                      */
	int vertex_size;				/* Actual size of vertex (not struct sz) */ 
	int vfile_size;                 /* vfile struct                          */
	int vfs_size;                   /* vfs struct                            */
	int vfssw_size;                 /* vfssw struct                          */
	int vnode_size;                 /* vnode struct                          */
	int vprgp_size;                 /* vprgp struct                          */
	int vproc_size;                 /* vproc struct                          */
	int vsocket_size;               /* vsocket struct                        */
	int xfs_inode_size;             /* xfs_inode struct                      */
	int xthread_s_size;             /* xthread_s struct                      */
	int zone_size;                  /* zone struct                           */
} struct_sizes_t;

/* Callback functions
 */
typedef struct callback_s {
	klib_sym_addr_func 			sym_addr;      /* Gets address of symbol     */
	klib_struct_len_func 		struct_len;    /* Gets length of struct      */
	klib_member_offset_func		member_offset; /* Gets offset of member      */
	klib_member_size_func		member_size;   /* Gets byte_size of member   */
	klib_member_bitlen_func		member_bitlen; /* Gets bit length of member  */
	klib_member_base_value_func	member_baseval; /* Gets base value of member */
	klib_block_alloc_func		block_alloc;   /* Returns pointer to block   */
	klib_block_free_func		block_free;    /* Frees memory block 		 */
	klib_print_error_func		print_error;   /* print app error msg.       */
} callback_t;

/* Global variables used by libklib
 */
typedef struct global_s {
	char 		   *program;   	 /* name of the program using libklib        */
	int				flags;		 /* Flags to control klib behavior			 */
	int				dumpcpu;     /* ID of CPU that panicked the system       */
	kaddr_t			dumpkthread; /* Kernel address of kthread that paniced   */
	kaddr_t 		defkthread;  /* kernel address of default kthread        */
	kaddr_t 		hwgraphp;    /* kernel address of hwgraph                */

	kaddr_t 		activefiles; /* linked list of open files (DEBUG only)   */
	kaddr_t 		dumpproc;    /* kernel address of panicing process       */
	kaddr_t			error_dumpbuf; /* HW error message buffer                */
#ifdef ANON_ITHREADS
	kaddr_t 		ithreadlist; /* start of ithread list                    */
#endif
	kaddr_t			kptbl;		 /* kernel address of page table 			 */
	kaddr_t 		lbolt;       /* current clock tick                       */
	kaddr_t 		mlinfolist;  /* loadable module info                     */
	kaddr_t			Nodepdaindr; /* start address of nodepdaindr array       */
	kaddr_t			pdaindr;	 /* start address of pdaindr array           */
	kaddr_t 		pidactive; 	 /* kernel address of active pids            */
	kaddr_t 		pidtab; 	 /* pid_entry table                          */
	kaddr_t 		pfdat; 	     /* pfdat table                              */
	kaddr_t 		putbuf;      /* putbuf                                   */
	kaddr_t 		sthreadlist; /* start of sthread list                    */
	kaddr_t 		strst;       /* streams statistics stuff                 */
	kaddr_t 		time;        /* current time (coredump only)             */
	kaddr_t 		xthreadlist; /* start of active xthread list             */

	int				pidtabsz;	 /* Number of elements in pidtab array       */
	short 			pid_base; 	 /* base pid 								 */

	k_ptr_t		    dumpregs;	 /* copy of dumpped register values          */
	k_ptr_t			kptblp;		 /* kernel page table (if not active)        */
	k_ptr_t			hwgraph;	 /* hwgraph									 */
} global_t;

/* Struct klib_t, contains all the information necessary for accessing
 * information int the kernel. A pointer to a klib_t struct will be 
 * returned from klib_init() if core dump analysis (or live system 
 * analysis) is possible. This pointer is passed back into other 
 * libklib routines by the application using the library.
 *
 */
typedef struct klib_s {
	coreinfo_t		k_coreinfo;
	sysinfo_t		k_sysinfo;
	kerninfo_t		k_kerninfo;
	pdeinfo_t		k_pdeinfo;
	struct_sizes_t  k_struct_sizes;
	callback_t		k_callback;
	global_t		k_global;
} klib_t;

/* Returns -1 in case of error.
 */
int klib_add_callbacks(
	klib_t*,	/* kernel */

	k_ptr_t,	/* app_ptr: passed in to libklib so it can pass back
				 * thru function pointers. calling app determines its
				 * use, not used by libklib (except to pass back when
				 * libklib calls callback functions ).
				 */
	klib_sym_addr_func,
		/* sym_addr, given symbol, return address */
	klib_struct_len_func,
		/* struct_len, gives length of structure */
	klib_member_offset_func,
		/* member_offset, gives offset of struct member */
	klib_member_size_func,
		/* member_size, length of struct member in bytes */
	klib_member_bitlen_func,
		/* member_bitlen, length of struct member in bits */
	klib_member_base_value_func,
		/* member_baseval, base value of member */
	klib_block_alloc_func,
		/* allocate memory block */
	klib_block_free_func,
		/* free memory block */
	klib_print_error_func
		/* print application error message */
	);

/**
 ** Macros that use values stored in the klib_t struct.
 **/
#define ACTIVE(K)				((K)->k_coreinfo.type == dev_kmem)
#define COMPRESSED(K)			((K)->k_coreinfo.type == cmp_core)
#define UNCOMPRESSED(K)			((K)->k_coreinfo.type == reg_core)
#define IS_PANIC(K)				((K)->k_coreinfo.panic_type == reg_panic)
#define IS_NMI(K)				((K)->k_coreinfo.panic_type == nmi_panic)

/** 
 ** Macros for accessing all fields in the klib_t struct. Most
 ** of the macros begin with "K_" to avoid conflicts with other
 ** macro definitions that may exist (e.g., in sys/immu.h).
 **/

/* From the coreinfo_s struct
 */
#define K_TYPE(K)				((K)->k_coreinfo.type)
#define K_PANIC_TYPE(K)			((K)->k_coreinfo.panic_type)
#define K_COREFILE(K)			((K)->k_coreinfo.corefile)
#define K_NAMELIST(K)			((K)->k_coreinfo.namelist)
#define K_ICRASHDEF(K)			((K)->k_coreinfo.icrashdef)
#define K_CORE_FD(K)			((K)->k_coreinfo.core_fd)
#define K_DUMP_HDR(K)			((K)->k_coreinfo.dump_hdr)
#define K_RW_FLAG(K)			((K)->k_coreinfo.rw_flag)

/* From the sysinfo_s struct
 */
#define K_UTSNAME(K)			((K)->k_sysinfo.utsname)
#define K_IP(K)				((K)->k_sysinfo.IP)
#define K_PHYSMEM(K)			((K)->k_sysinfo.physmem)
#define K_NUMCPUS(K)			((K)->k_sysinfo.numcpus)
#define K_MAXCPUS(K)			((K)->k_sysinfo.maxcpus)
#define K_NTLBENTRIES(K)		((K)->k_sysinfo.ntlbentries)
#define K_TLBENTRYSZ(K)			((K)->k_sysinfo.tlbentrysz)
#define K_TLBDUMPSIZE(K)		((K)->k_sysinfo.tlbdumpsize)
#define K_NUMNODES(K)			((K)->k_sysinfo.numnodes)
#define K_MAXNODES(K)			((K)->k_sysinfo.maxnodes)
#define K_MASTER_NASID(K)		((K)->k_sysinfo.master_nasid)
#define K_NASID_SHIFT(K)		((K)->k_sysinfo.nasid_shift)
#define K_SLOT_SHIFT(K)			((K)->k_sysinfo.slot_shift)
#define K_PARCEL_SHIFT(K)		((K)->k_sysinfo.parcel_shift)
#define K_PARCEL_BITMASK(K)		((K)->k_sysinfo.parcel_bitmask)
#define K_PARCELS_PER_SLOT(K)	        ((K)->k_sysinfo.parcels_per_slot)
#define K_SLOTS_PER_NODE(K)		((K)->k_sysinfo.slots_per_node)
#define K_MEM_PER_NODE(K)		((K)->k_sysinfo.mem_per_node)
#define K_MEM_PER_SLOT(K)		((K)->k_sysinfo.mem_per_slot)
#define K_MEM_PER_BANK(K)		((K)->k_sysinfo.mem_per_bank)
#define K_NASID_BITMASK(K)		((K)->k_sysinfo.nasid_bitmask)
#define K_SLOT_BITMASK(K)		((K)->k_sysinfo.slot_bitmask)
#define K_SYSTEMSIZE(K)			((K)->k_sysinfo.systemsize)
#define K_SYSMEMSIZE(K)			((K)->k_sysinfo.sysmemsize)
#define K_END(K)				((K)->k_sysinfo.kend)

/* From the kerninfo_s struct
 */
#define K_IRIX_REV(K)			((K)->k_kerninfo.irix_rev)
#define K_SYSSEGSZ(K)			((K)->k_kerninfo.syssegsz)
#define K_NPROCS(K)				((K)->k_kerninfo.nprocs)

#define K_PTRSZ(K)        		((K)->k_kerninfo.ptrsz)
#define K_REGSZ(K)        		((K)->k_kerninfo.regsz)
#define K_PAGESZ(K)       		((K)->k_kerninfo.pagesz)
#define K_NBPW(K)         		((K)->k_kerninfo.nbpw)
#define K_NBPC(K)         		((K)->k_kerninfo.nbpc)
#define K_NCPS(K)         		((K)->k_kerninfo.ncps)
#define K_NBPS(K)         		((K)->k_kerninfo.nbps)
#define K_PNUMSHIFT(K)    		((K)->k_kerninfo.pnumshift)
#define K_TO_PHYS_MASK(K) 		((K)->k_kerninfo.to_phys_mask)
#define K_RAM_OFFSET(K)   		((K)->k_kerninfo.ram_offset)
#define K_MAXPFN(K)       		((K)->k_kerninfo.maxpfn)
#define K_MAXPHYS(K)      		((K)->k_kerninfo.maxphys)

#define K_USIZE(K)        		((K)->k_kerninfo.usize)
#define K_EXTUSIZE(K)    	 	((K)->k_kerninfo.extusize)
#define K_UPGIDX(K)      	 	((K)->k_kerninfo.upgidx)
#define K_KSTKIDX(K)     	 	((K)->k_kerninfo.kstkidx)
#define K_EXTSTKIDX(K)   	 	((K)->k_kerninfo.extstkidx)

#define K_KUBASE(K)    			((K)->k_kerninfo.kubase)
#define K_KUSIZE(K)    			((K)->k_kerninfo.kusize)
#define K_K0BASE(K)    			((K)->k_kerninfo.k0base)
#define K_K0SIZE(K)    			((K)->k_kerninfo.k0size)
#define K_K1BASE(K)    			((K)->k_kerninfo.k1base)
#define K_K1SIZE(K)    			((K)->k_kerninfo.k1size)
#define K_K2BASE(K)    			((K)->k_kerninfo.k2base)
#define K_K2SIZE(K)    			((K)->k_kerninfo.k2size)
#define K_KERNSTACK(K) 	 		((K)->k_kerninfo.kernstack)
#define K_KERNELSTACK(K) 	 	((K)->k_kerninfo.kernelstack)
#define K_KEXTSTACK(K)  		((K)->k_kerninfo.kextstack)
#define K_KPTEBASE(K)  			((K)->k_kerninfo.kptebase)
#define K_KPTE_USIZE(K)  		((K)->k_kerninfo.kpte_usize)
#define K_KPTE_SHDUBASE(K)  	((K)->k_kerninfo.kpte_shdubase)

#define K_MAPPED_RO_BASE(K)  	((K)->k_kerninfo.mapped_ro_base)
#define K_MAPPED_RW_BASE(K)  	((K)->k_kerninfo.mapped_rw_base)
#define K_MAPPED_PAGE_SIZE(K) 	((K)->k_kerninfo.mapped_page_size)

/* From the pdeinfo_s struct
 */
#define K_PFN_MASK(K)			((K)->k_pdeinfo.pfn_mask)
#define K_PFN_SHIFT(K)			((K)->k_pdeinfo.pfn_shift)
#define K_PDE_PG_VR(K)			((K)->k_pdeinfo.Pde_pg_vr)
#define K_PG_VR_SHIFT(K)		((K)->k_pdeinfo.pg_vr_shift)
#define K_PDE_PG_G(K)			((K)->k_pdeinfo.Pde_pg_g)
#define K_PG_G_SHIFT(K)			((K)->k_pdeinfo.pg_g_shift)
#define K_PDE_PG_M(K)			((K)->k_pdeinfo.Pde_pg_m)
#define K_PG_M_SHIFT(K)			((K)->k_pdeinfo.pg_m_shift)
#define K_PDE_PG_N(K)			((K)->k_pdeinfo.Pde_pg_n)
#define K_PG_N_SHIFT(K)			((K)->k_pdeinfo.pg_n_shift)
#define K_PDE_PG_SV(K)			((K)->k_pdeinfo.Pde_pg_sv)
#define K_PG_SV_SHIFT(K)		((K)->k_pdeinfo.pg_sv_shift)
#define K_PDE_PG_D(K)			((K)->k_pdeinfo.Pde_pg_d)
#define K_PG_D_SHIFT(K)			((K)->k_pdeinfo.pg_d_shift)
#define K_PDE_PG_EOP(K)			((K)->k_pdeinfo.Pde_pg_eop)
#define K_PG_EOP_SHIFT(K)		((K)->k_pdeinfo.pg_eop_shift)
#define K_PDE_PG_NR(K)			((K)->k_pdeinfo.Pde_pg_nr)
#define K_PG_NR_SHIFT(K)		((K)->k_pdeinfo.pg_nr_shift)
#define K_PDE_PG_CC(K)			((K)->k_pdeinfo.Pde_pg_cc)
#define K_PG_CC_SHIFT(K)		((K)->k_pdeinfo.pg_cc_shift)

/* From the struct_sizes_s struct
 */
#define AVLNODE_SIZE(K)			((K)->k_struct_sizes.avlnode_size)
#define BHV_DESC_SIZE(K)		((K)->k_struct_sizes.bhv_desc_size)
#define CFG_DESC_SIZE(K)		((K)->k_struct_sizes.cfg_desc_size)
#define CRED_SIZE(K)			((K)->k_struct_sizes.cred_size)
#define DOMAIN_SIZE(K)			((K)->k_struct_sizes.domain_size)
#define EFRAME_S_SIZE(K)		((K)->k_struct_sizes.eframe_s_size)
#define EXCEPTION_SIZE(K)		((K)->k_struct_sizes.exception_size)
#define FDT_SIZE(K)				((K)->k_struct_sizes.fdt_size)
#define GRAPH_SIZE(K)			((K)->k_struct_sizes.graph_size)
#define GRAPH_S_SIZE(K)			((K)->k_struct_sizes.graph_s_size)
#define GRAPH_VERTEX_GROUP_S_SIZE(K) \
								((K)->k_struct_sizes.graph_vertex_group_s_size)
#define GRAPH_VERTEX_S_SIZE(K)	((K)->k_struct_sizes.graph_vertex_s_size)
#define GRAPH_EDGE_S_SIZE(K)	((K)->k_struct_sizes.graph_edge_s_size)
#define GRAPH_INFO_S_SIZE(K)	((K)->k_struct_sizes.graph_info_s_size)
#define IN_ADDR_SIZE(K)			((K)->k_struct_sizes.in_addr_size)
#define INVENT_MEMINFO_SIZE(K)	((K)->k_struct_sizes.invent_meminfo_size)
#define INODE_SIZE(K)			((K)->k_struct_sizes.inode_size)
#define INPCB_SIZE(K)			((K)->k_struct_sizes.inpcb_size)

#ifdef ANON_ITHREADS
#define ITHREAD_S_SIZE(K)		((K)->k_struct_sizes.ithread_s_size)
#endif

#define KNA_SIZE(K)				((K)->k_struct_sizes.kna_size)
#define KNETVEC_SIZE(K)			((K)->k_struct_sizes.knetvec_size)
#define KTHREAD_SIZE(K)			((K)->k_struct_sizes.kthread_size)
#define LF_LISTVEC_SIZE(K)		((K)->k_struct_sizes.lf_listvec_size)
#define MBUF_SIZE(K)			((K)->k_struct_sizes.mbuf_size)
#define ML_INFO_SIZE(K)			((K)->k_struct_sizes.ml_info_size)
#define ML_SYM_SIZE(K)			((K)->k_struct_sizes.ml_sym_size)
#define MNTINFO_SIZE(K)			((K)->k_struct_sizes.mntinfo_size)
#define MRLOCK_S_SIZE(K)        ((K)->k_struct_sizes.mrlock_s_size)
#define MODULE_INFO_SIZE(K)		((K)->k_struct_sizes.module_info_size)
#define NODEPDA_S_SIZE(K)		((K)->k_struct_sizes.nodepda_s_size)
#define PDA_S_SIZE(K)			((K)->k_struct_sizes.pda_s_size)
#define PDE_SIZE(K)				((K)->k_struct_sizes.pde_size)
#define PFDAT_SIZE(K)			((K)->k_struct_sizes.pfdat_size)
#define PMAP_SIZE(K)			((K)->k_struct_sizes.pmap_size)
#define PID_ENTRY_SIZE(K)		((K)->k_struct_sizes.pid_entry_size)
#define PID_SLOT_SIZE(K)		((K)->k_struct_sizes.pid_slot_size)
#define PREGION_SIZE(K)			((K)->k_struct_sizes.pregion_size)
#define PROC_SIZE(K)			((K)->k_struct_sizes.proc_size)
#define PROC_PROXY_SIZE(K)		((K)->k_struct_sizes.proc_proxy_size)
#define PROTOSW_SIZE(K)			((K)->k_struct_sizes.protosw_size)
#define QINIT_SIZE(K)			((K)->k_struct_sizes.qinit_size)
#define QUEUE_SIZE(K)			((K)->k_struct_sizes.queue_size)
#define REGION_SIZE(K)			((K)->k_struct_sizes.region_size)
#define RNODE_SIZE(K)			((K)->k_struct_sizes.rnode_size)
#define SEMA_SIZE(K)			((K)->k_struct_sizes.sema_size)
#define CSNODE_SIZE(K)			((K)->k_struct_sizes.csnode_size)
#define LSNODE_SIZE(K)			((K)->k_struct_sizes.lsnode_size)
#define SOCKET_SIZE(K)			((K)->k_struct_sizes.socket_size)
#define STDATA_SIZE(K)			((K)->k_struct_sizes.stdata_size)
#define STHREAD_S_SIZE(K)		((K)->k_struct_sizes.sthread_s_size)
#define STRSTAT_SIZE(K)			((K)->k_struct_sizes.strstat_size)
#define SWAPINFO_SIZE(K)		((K)->k_struct_sizes.swapinfo_size)
#define TCPCB_SIZE(K)			((K)->k_struct_sizes.tcpcb_size)
#define UFCHUNK_SIZE(K)			((K)->k_struct_sizes.ufchunk_size)
#define UNPCB_SIZE(K)			((K)->k_struct_sizes.unpcb_size)
#define UTHREAD_S_SIZE(K)		((K)->k_struct_sizes.uthread_s_size)
#define VERTEX_SIZE(K)			((K)->k_struct_sizes.vertex_size)
#define VFILE_SIZE(K)			((K)->k_struct_sizes.vfile_size)
#define VFS_SIZE(K)				((K)->k_struct_sizes.vfs_size)
#define VFSSW_SIZE(K)			((K)->k_struct_sizes.vfssw_size)
#define VNODE_SIZE(K)			((K)->k_struct_sizes.vnode_size)
#define VPRGP_SIZE(K)			((K)->k_struct_sizes.vprgp_size)
#define VPROC_SIZE(K)			((K)->k_struct_sizes.vproc_size)
#define VSOCKET_SIZE(K)			((K)->k_struct_sizes.vsocket_size)
#define XFS_INODE_SIZE(K)		((K)->k_struct_sizes.xfs_inode_size)
#define XTHREAD_S_SIZE(K)		((K)->k_struct_sizes.xthread_s_size)
#define ZONE_SIZE(K)			((K)->k_struct_sizes.zone_size)

/* From the callback_s struct
 */
#define K_SYM_ADDR(K)			((K)->k_callback.sym_addr)
#define K_STRUCT_LEN(K)			((K)->k_callback.struct_len)
#define K_MEMBER_OFFSET(K)		((K)->k_callback.member_offset)
#define K_MEMBER_SIZE(K)		((K)->k_callback.member_size)
#define K_MEMBER_BITLEN(K)		((K)->k_callback.member_bitlen)
#define K_MEMBER_BASEVAL(K)		((K)->k_callback.member_baseval)
#define K_BLOCK_ALLOC(K)		((K)->k_callback.block_alloc)
#define K_BLOCK_FREE(K)			((K)->k_callback.block_free)
#define K_PRINT_ERROR(K)		((K)->k_callback.print_error)

/* From the global_s struct
 */
#define K_PROGRAM(K)			((K)->k_global.program)
#define K_FLAGS(K)				((K)->k_global.flags)
#define K_DUMPCPU(K)		    ((K)->k_global.dumpcpu)
#define K_DUMPKTHREAD(K)		((K)->k_global.dumpkthread)
#define K_DEFKTHREAD(K)			((K)->k_global.defkthread)
#define K_HWGRAPHP(K)			((K)->k_global.hwgraphp)
#define K_ACTIVEFILES(K)		((K)->k_global.activefiles)
#define K_DUMPPROC(K)			((K)->k_global.dumpproc)
#define K_ERROR_DUMPBUF(K)		((K)->k_global.error_dumpbuf)

#ifdef ANON_ITHREADS
#define K_ITHREADLIST(K)		((K)->k_global.ithreadlist)
#endif

#define K_KPTBL(K)				((K)->k_global.kptbl)
#define K_LBOLT(K)				((K)->k_global.lbolt)
#define K_MLINFOLIST(K)			((K)->k_global.mlinfolist)
#define K_NODEPDAINDR(K)		((K)->k_global.Nodepdaindr)
#define K_PDAINDR(K)			((K)->k_global.pdaindr)
#define K_PIDACTIVE(K)			((K)->k_global.pidactive)
#define K_PIDTAB(K)				((K)->k_global.pidtab)
#define K_PFDAT(K)				((K)->k_global.pfdat)
#define K_PUTBUF(K)				((K)->k_global.putbuf)
#define K_STHREADLIST(K)		((K)->k_global.sthreadlist)
#define K_STRST(K)				((K)->k_global.strst)
#define K_TIME(K)				((K)->k_global.time)
#define K_XTHREADLIST(K)		((K)->k_global.xthreadlist)

#define K_PIDTABSZ(K)			((K)->k_global.pidtabsz)
#define K_PID_BASE(K)			((K)->k_global.pid_base)

#define K_DUMPREGS(K)			((K)->k_global.dumpregs)
#define K_KPTBLP(K)				((K)->k_global.kptblp)
#define K_HWGRAPH(K)			((K)->k_global.hwgraph)

/* List element header
 */
typedef struct element_s {
    struct element_s    *next;
    struct element_s    *prev;
} element_t;

/*
 * Node header struct for use in binary search tree routines
 */
typedef struct btnode_s {
    struct btnode_s     *bt_left;
    struct btnode_s     *bt_right;
    char                *bt_key;
    int                  bt_height;
} btnode_t;

#define DUPLICATES_OK   1

/* Some useful macros
 */
#define ENQUEUE(list, elem) kl_enqueue((element_t **)list, (element_t *)elem)
#define DEQUEUE(list) kl_dequeue((element_t **)list)
#define FINDQUEUE(list, elem) \
	kl_findqueue((element_t **)list, (element_t *)elem)
#define REMQUEUE(list, elem) kl_remqueue((element_t **)list, (element_t *)elem)

typedef struct list_of_ptrs {
	element_t elem;
	kaddr_t val64;
} list_of_ptrs_t;

#define FINDLIST_QUEUE(list,elem,compare) \
        kl_findlist_queue((element_t **)list, (element_t *)elem,compare)


/* The string table structure
 * 
 * String space is allocated from 4K blocks which are allocated 
 * as needed. The first four bytes of each block are reserved so 
 * that the blocks can be chained together (to make it easy to free 
 * them when the string table is no longer necessary).
 */
typedef struct string_table_s {
	int             num_strings;
	k_ptr_t         block_list;
} string_table_t;

/* Error structure 
 */
typedef struct error_s {
	FILE       *e_errorfp;  /* Default file deescriptor for error output     */
	int         e_flags;    /* Everybody needs some flags (see below)        */
	k_uint_t    e_code;     /* Error code (see below)                        */
	k_uint_t    e_nval;     /* Numeric value that caused error               */
	char       *e_cval;     /* String representation of error value          */
	int         e_mode;     /* Mode indicating how represent numeric value   */
} error_t;

/** 
 ** Some error related macros
 **/
#define KLIB_ERROR		klib_error
#define KL_ERROR 		(KLIB_ERROR.e_code)
#define KL_ERRORFP 		(KLIB_ERROR.e_errorfp)

#define KL_RETURN_ERROR return(KL_ERROR)

#define KL_CHECK_ERROR(K, code, nval, cval, mode) \
    if (KL_ERROR) { \
		if (code) { \
			kl_set_error(KL_ERROR|(code>>32), nval, cval, mode); \
		} \
		kl_print_error(K); \
		return(KL_ERROR); \
    }

#define KL_CHECK_ERROR_RETURN\
	if (KL_ERROR) {\
		KL_RETURN_ERROR;\
	}

#define KL_SET_ERROR(code) \
	kl_set_error(code, 0, (char*)0, -1)

#define KL_SET_ERROR_NVAL(code, nval, mode) \
	kl_set_error(code, nval, (char*)0, mode)

#define KL_SET_ERROR_CVAL(code, cval) \
	kl_set_error(code, 0, cval, -1)

#ifdef ANON_ITHREADS
#define KLE_KTHREAD_TYPE(K, ktp) \
	(IS_UTHREAD(K, ktp) ? KLE_IS_UPROC : \
	(IS_ITHREAD(K, ktp) ? KLE_IS_ITHREAD : \
	(IS_XTHREAD(K, ktp) ? KLE_IS_XTHREAD : \
	(IS_STHREAD(K, ktp) ? KLE_IS_STHREAD : KLE_UNKNOWN_ERROR))))
#else
#define KLE_KTHREAD_TYPE(K, ktp) \
	(IS_UTHREAD(K, ktp) ? KLE_IS_UPROC : \
	(IS_XTHREAD(K, ktp) ? KLE_IS_XTHREAD : \
	(IS_STHREAD(K, ktp) ? KLE_IS_STHREAD : KLE_UNKNOWN_ERROR)))
#endif

#define KLIB_ERROR_CODE_MAX		  999

/* Flags
 */
#define EF_NVAL_VALID           0x001
#define EF_CVAL_VALID           0x002

/* Error codes
 *
 * There are basically two types of error codes -- with each type
 * residing in a single word in a two word error code value. The lower
 * 32-bits contains an error code that represents exactly WHAT error
 * occurred (e.g., non-numeric text in a numeric value entered by a
 * user, bad virtual address, etc.). The error code space has been
 * divided into segments. The first 999 code segment is reserved for
 * libklib error codes. The rest of the space can be used by the
 * application. There is room in the callbacks_s struct for an
 * application print_error function. This function will be called
 * by kl_print_error() (if a callback exists) if the code value 
 * falls into the application range.
 * 
 * The upper 32-bits represents what struct was being referenced when 
 * the error occurred (e.g., bad proc struct). Having two tiers of error 
 * codes makes it easier to generate useful and specific error messages. 
 * Note that is possible to have situations where one or the other type 
 * of error codes is not set. This is OK as long as at least one type 
 * is set.
 */
#define KLE_INVALID_VALUE     	   1  /* Bad value detected in get_value()    */
#define KLE_INVALID_VADDR     	   2  /* Bad kernel virtual address           */
#define KLE_INVALID_VADDR_ALIGN    3  /* Bad alignment of virtual address     */
#define KLE_INVALID_K2_ADDR        4  /* Legal but invalid K2 address         */
#define KLE_INVALID_KERNELSTACK    5  /* Could not map kernelstack (defunct)  */
#define KLE_INVALID_LSEEK		   6  /* lseek() call failed                  */
#define KLE_INVALID_READ		   7  /* read() call failed                   */
#define KLE_INVALID_CMPREAD		   8  /* cmpread() call failed                */
#define KLE_INVALID_STRUCT_SIZE    9  /* invalid struct size                  */
#define KLE_BEFORE_RAM_OFFSET	  10  /* Illeagle physical memory address     */
#define KLE_AFTER_MAXPFN		  11  /* Higher than largest possible PFN     */
#define KLE_AFTER_PHYSMEM    	  12  /* Higher than largest phys mem addr    */
#define KLE_AFTER_MAXMEM   	      13  /* Higher than memory installed in sys  */
#define KLE_PHYSMEM_NOT_INSTALLED 14  /* Higher than memory installed in sys  */
#define KLE_IS_UPROC		      15  /* Got a proc, should be a kthread      */

#ifdef ANON_ITHREADS
#define KLE_IS_ITHREAD		      16  /* Got an ithread, should be a proc     */
#endif

#define KLE_IS_STHREAD		      17  /* Got an sthread, should be a proc     */
#define KLE_IS_XTHREAD		      18  /* Got an xthread, should be a proc     */
#define KLE_WRONG_KTHREAD_TYPE    19  /* wrong type of kthread                */
#define KLE_NO_DEFKTHREAD		  20  /* No default kthread set 	          */
#define KLE_PID_NOT_FOUND	      21  /* Could not locate PID in active procs */
#define KLE_INVALID_DUMPPROC 	  22  /* dumpproc pointer is invalid          */
#define KLE_NULL_BUFF		      23  /* NULL buffer pointer                  */
#define KLE_ACTIVE		          24  /* Action not legal on a live system    */
#define KLE_DEFKTHREAD_NOT_ON_CPU 25  /* Defkthread not running on CPU        */
#define KLE_NO_CURCPU    	      26  /* Couldn't determine current CPU       */
#define KLE_NO_CPU           	  27  /* valid CPU ID, but not installed      */
#define KLE_SHORT_DUMP		      28  /* Compressed dump without memory pages */

#define KLE_NOT_IMPLEMENTED      998  /* Feature not implemented yet          */
#define KLE_UNKNOWN_ERROR	     999  /* Unknown error                        */

#define KLIB_ERROR_MAX			 999 /* Maximum klib error value              */

/* Error codes that indicate what type of structure was being allocated 
 * when an error occurred.
 */
#define KLE_BAD_AVLNODE	    	(((k_uint_t)1)<<32)
#define KLE_BAD_BHV_DESC        (((k_uint_t)2)<<32)
#define KLE_BAD_BLOCK_S  	    (((k_uint_t)3)<<32)
#define KLE_BAD_BUCKET_S      	(((k_uint_t)4)<<32)
#define KLE_BAD_EFRAME_S	    (((k_uint_t)5)<<32) 
#define KLE_BAD_GRAPH_VERTEX_S  (((k_uint_t)6)<<32)
#define KLE_BAD_GRAPH_EDGE_S  	(((k_uint_t)7)<<32)
#define KLE_BAD_GRAPH_INFO_S  	(((k_uint_t)8)<<32)
#define KLE_BAD_INODE  	    	(((k_uint_t)9)<<32)
#define KLE_BAD_INPCB  	    	(((k_uint_t)10)<<32)
#define KLE_BAD_KTHREAD       	(((k_uint_t)11)<<32)

#ifdef ANON_ITHREADS
#define KLE_BAD_ITHREAD_S     	(((k_uint_t)12)<<32)
#endif

#define KLE_BAD_STHREAD_S     	(((k_uint_t)13)<<32)
#define KLE_BAD_MBSTAT        	(((k_uint_t)14)<<32)
#define KLE_BAD_MRLOCK_S		(((k_uint_t)15)<<32)
#define KLE_BAD_MBUF          	(((k_uint_t)16)<<32)
#define KLE_BAD_NODEPDA  		(((k_uint_t)17)<<32)
#define KLE_BAD_PDA  		    (((k_uint_t)18)<<32)
#define KLE_BAD_PDE  		    (((k_uint_t)19)<<32)
#define KLE_BAD_PFDAT  	    	(((k_uint_t)20)<<32)
#define KLE_BAD_PFDATHASH	    (((k_uint_t)21)<<32)
#define KLE_BAD_PID_ENTRY	    (((k_uint_t)22)<<32)
#define KLE_BAD_PID_SLOT	    (((k_uint_t)23)<<32)
#define KLE_BAD_PREGION  	    (((k_uint_t)24)<<32)
#define KLE_BAD_PROC	        (((k_uint_t)25)<<32)
#define KLE_BAD_QUEUE    	    (((k_uint_t)26)<<32)
#define KLE_BAD_REGION   	    (((k_uint_t)27)<<32)
#define KLE_BAD_RNODE   	    (((k_uint_t)28)<<32)
#define KLE_BAD_SEMA_S        	(((k_uint_t)29)<<32)
#define KLE_BAD_LSNODE    	    (((k_uint_t)30)<<32)
#define KLE_BAD_SOCKET        	(((k_uint_t)31)<<32)
#define KLE_BAD_STDATA        	(((k_uint_t)32)<<32)
#define KLE_BAD_STRSTAT       	(((k_uint_t)33)<<32)
#define KLE_BAD_SWAPINFO      	(((k_uint_t)34)<<32)
#define KLE_BAD_TCPCB    	    (((k_uint_t)35)<<32)
#define KLE_BAD_UNPCB    	    (((k_uint_t)36)<<32)
#define KLE_BAD_UTHREAD_S     	(((k_uint_t)37)<<32)
#define KLE_BAD_VFILE  	    	(((k_uint_t)38)<<32)
#define KLE_BAD_VFS    	    	(((k_uint_t)39)<<32)
#define KLE_BAD_VFSSW  			(((k_uint_t)40)<<32)
#define KLE_BAD_VNODE    	    (((k_uint_t)41)<<32)
#define KLE_BAD_VPROC    	    (((k_uint_t)42)<<32)
#define KLE_BAD_VSOCKET    		(((k_uint_t)43)<<32)
#define KLE_BAD_XFS_INODE    	(((k_uint_t)44)<<32)
#define KLE_BAD_XFS_DINODE_CORE (((k_uint_t)45)<<32)
#define KLE_BAD_XTHREAD_S     	(((k_uint_t)46)<<32)
#define KLE_BAD_ZONE    	    (((k_uint_t)47)<<32)
#define KLE_BAD_DIE           	(((k_uint_t)48)<<32) /* Bad Dwarf DIE info */
#define KLE_BAD_BASE_VALUE    	(((k_uint_t)49)<<32) /* Bad bit offset get */
#define KLE_BAD_CPUID         	(((k_uint_t)50)<<32) /* CPUID or pda_s ptr */
#define KLE_BAD_STREAM        	(((k_uint_t)51)<<32) /* Not in streamtab   */
#define KLE_BAD_LINENO        	(((k_uint_t)52)<<32)
#define KLE_BAD_SYMNAME       	(((k_uint_t)53)<<32)
#define KLE_BAD_SYMADDR       	(((k_uint_t)54)<<32)
#define KLE_BAD_FUNCADDR      	(((k_uint_t)55)<<32)
#define KLE_BAD_STRUCT        	(((k_uint_t)56)<<32)
#define KLE_BAD_FIELD         	(((k_uint_t)57)<<32)
#define KLE_BAD_PC 		    	(((k_uint_t)58)<<32)
#define KLE_BAD_SP 				(((k_uint_t)59)<<32)
#define KLE_BAD_EP 		  		(((k_uint_t)60)<<32)
#define KLE_BAD_SADDR         	(((k_uint_t)61)<<32)
#define KLE_BAD_KERNELSTACK   	(((k_uint_t)62)<<32)
#define KLE_BAD_DEBUG         	(((k_uint_t)63)<<32)
#define KLE_DEFUNCT_PROCESS   	(((k_uint_t)64)<<32)
#define KLE_BAD_VERTEX_HNDL   	(((k_uint_t)65)<<32)
#define KLE_BAD_VERTEX_EDGE   	(((k_uint_t)66)<<32)
#define KLE_BAD_VERTEX_INFO   	(((k_uint_t)67)<<32)
#define KLE_BAD_DEFKTHREAD     	(((k_uint_t)68)<<32)
#define KLE_BAD_ANON            (((k_uint_t)69)<<32)  
#define KLE_BAD_MNTINFO		(((k_uint_t)70)<<32)
#define KLE_BAD_CSNODE          (((k_uint_t)71)<<32)

/* DEBUG macro
 *
 * The contents of klib_s->k_global.debug determines if a particular
 * debug message will or will not be displayed. Bits 32-63 are
 * reserved for klib debug class flags (defined below); Bits 16-31 
 * are reserved for application level classes (defined by the 
 * application). Bits 0-15 are reserved for the debug level. In 
 * order for a debug message to be displayed, the flag for the 
 * appropriate class must be set AND the the specified debug level 
 * must be <= the current level.
 */

/* libklib level debug classes
 */
#define KLDC_GLOBAL         0x0000000000000000
#define KLDC_FUNCTRACE      0x0000001000000000
#define KLDC_CMP            0x0000002000000000
#define KLDC_ERROR          0x0000004000000000
#define KLDC_HWGRAPH        0x0000008000000000
#define KLDC_INIT           0x0000010000000000
#define KLDC_KTHREAD        0x0000020000000000
#define KLDC_MEM            0x0000040000000000
#define KLDC_PAGE           0x0000080000000000
#define KLDC_PROC           0x0000100000000000
#define KLDC_STRUCT         0x0000200000000000
#define KLDC_UTIL           0x0000400000000000

#define DEBUG(class, level) \
	((!class || (class & klib_debug)) && (level <= (klib_debug & 0xf)))

extern node_memory_t **node_memory;
extern error_t klib_error;
extern k_uint_t klib_debug;

/* Some defines stolen (for obvious reasons) from graph_private.h
 */
#define GRAPH_VERTEX_FREE   (void *)0L
#define GRAPH_VERTEX_NOINFO (void *)1L
#define GRAPH_VERTEX_NEW    (void *)2L
#define GRAPH_VERTEX_SPECIAL    2L

extern int graph_vertex_per_group;
extern int graph_vertex_per_group_log2;

#define GRAPH_VERTEX_PER_GROUP graph_vertex_per_group
#define GRAPH_VERTEX_PER_GROUP_LOG2 graph_vertex_per_group_log2
#define GRAPH_VERTEX_PER_GROUP_MASK (GRAPH_VERTEX_PER_GROUP - 1)

#define GRAPH_NUM_GROUP 128
#define GRAPH_NUM_VERTEX_MAX (GRAPH_NUM_GROUP * GRAPH_VERTEX_PER_GROUP)

#define handle_to_groupid(handle) (handle >> GRAPH_VERTEX_PER_GROUP_LOG2)
#define handle_to_grpidx(handle) (handle & GRAPH_VERTEX_PER_GROUP_MASK)

#define PATH(p) (p->pchunk->prev->path[p->pchunk->prev->current])

#define PATHS_PER_CHUNK 100

typedef struct path_chunk_s {
	struct path_chunk_s	   *next;
	struct path_chunk_s	   *prev;
	int						current;
	struct path_rec_s      *path[PATHS_PER_CHUNK];
} path_chunk_t;

typedef struct path_rec_s {
    struct path_rec_s      *next;
    struct path_rec_s      *prev;
    int                     vhndl;
    char                   *name;
} path_rec_t;

typedef struct path_s {
    int             	    count;
    path_chunk_t           *pchunk;
	string_table_t	       *st;
} path_t;

/* Macros useful when working with pidtab[]
 */
#define K_PID_INDEX(K, PID) (((PID) - K_PID_BASE(K)) % K_PIDTABSZ(K))
#define K_PID_SLOT(K, PID) (K_PIDTAB(K) + \
		(K_PID_INDEX(K, PID) * PID_SLOT_SIZE(K)))

/*
 * Create a hash list of all the vfiles in the system.
 * Do not keep duplicates of the viles.
 * Each element in the hash table is a (list_of_ptrs_t *).
 */
#define VFILE_HASHSIZE 127

#define MD_MEM_BANKS     8       /* Number of banks per node in M_MODE for SN0. */


/**
 ** libklib function prototypes
 **/

/* klib_error.c
 */
void kl_reset_error();
void kl_set_error(k_uint_t, k_uint_t, char *, int);
void kl_print_debug(klib_t *, char *);
void kl_print_error(klib_t *);

/*
 * klib_hwgraph.c
 */
kaddr_t handle_to_vertex(klib_t *, k_uint_t);
kaddr_t handle_to_vertex_group(klib_t *, k_uint_t);
kaddr_t groupid_to_group(klib_t *, int);
int grpidx_to_handle(klib_t *, int, int);
int vertex_to_handle(klib_t *, kaddr_t);
kaddr_t vertex_edge_ptr(klib_t *, k_ptr_t);
kaddr_t vertex_lbl_ptr(klib_t *, k_ptr_t);
int get_edge_name(klib_t *, k_ptr_t, char *);
int get_label_name(klib_t *, k_ptr_t, char *);
k_ptr_t vertex_edge_vhndl(klib_t *, k_ptr_t, int, k_ptr_t);
k_ptr_t vertex_edge_name(klib_t *, k_ptr_t, char *, k_ptr_t);
k_ptr_t vertex_info_name(klib_t *, k_ptr_t, char *, k_ptr_t);
int connect_point(klib_t *, int);
int is_symlink(klib_t *, k_ptr_t, int, int);
path_t *init_path_table(klib_t *);
void add_to_path(klib_t *, path_rec_t *, path_rec_t *);
void free_path_records(klib_t *, path_rec_t *);
void free_path(klib_t *, path_t *);
void push_path(klib_t *, path_t *, path_rec_t *, char *, int);
void pop_path(klib_t *, path_rec_t *);
void dup_current_path(klib_t *, path_t *);
int hw_find_pathname(klib_t *, int, char *, path_t *, int);
int kl_pathname_to_vertex(klib_t *, char *, int);
path_rec_t *kl_vertex_to_pathname(klib_t *, kaddr_t, int);
void kl_free_pathname(klib_t *, path_rec_t *);
int vertex_get_next(klib_t *, int *, int *);
int print_mfg_info(klib_t *, char *, char *, char *, char *, int, FILE *);

/* klib.c
 */
klib_t *klib_open(char *, char *, char *, char *, int32, int);
int klib_init(klib_t *);
int klib_add_callbacks(klib_t *, k_ptr_t, klib_sym_addr_func, 
	klib_struct_len_func, klib_member_offset_func, klib_member_size_func, 
	klib_member_bitlen_func, klib_member_base_value_func, 
	klib_block_alloc_func, klib_print_error_func, klib_block_free_func);

/* klib_kthread.c
 */
int kl_set_defkthread(klib_t *, kaddr_t);
kaddr_t kl_get_curkthread(klib_t *, int);
kaddr_t kl_kthread_addr(klib_t *, k_ptr_t);
k_ptr_t kl_kthread_name(klib_t *, k_ptr_t);
int kl_kthread_type(klib_t *, k_ptr_t);
kaddr_t kl_get_kthread_stack(klib_t *, kaddr_t, int *);

/* klib_mem.c
 */
int kl_is_valid_kaddr(klib_t *, kaddr_t, k_ptr_t, int);
kaddr_t kl_virtop(klib_t *, kaddr_t, k_ptr_t);
kaddr_t kl_pfn_to_K2(klib_t *, uint);
int kl_get_addr_mode(klib_t *, kaddr_t);
int kl_seekmem(klib_t *, kaddr_t, int, k_ptr_t);
int kl_readmem(klib_t *, kaddr_t, int, char *, k_ptr_t, unsigned, char *);
k_ptr_t kl_get_block(klib_t *, kaddr_t, unsigned, k_ptr_t, char *);
k_ptr_t kl_get_kaddr(klib_t *, kaddr_t, k_ptr_t, char *);
k_uint_t kl_uint(klib_t *, k_ptr_t, char *, char *, unsigned);
k_int_t kl_int(klib_t *, k_ptr_t, char *, char *, unsigned);
kaddr_t kl_kaddr(klib_t *, k_ptr_t, char *, char *);
kaddr_t kl_reg(klib_t *, k_ptr_t, char *, char *);
kaddr_t kl_kaddr_val(klib_t *, k_ptr_t);
kaddr_t kl_kaddr_to_ptr(klib_t *klp, kaddr_t);
int kl_addr_convert(klib_t *, kaddr_t, int, kaddr_t *, uint *, kaddr_t *, 
	kaddr_t *, kaddr_t *);
int kl_addr_to_nasid(klib_t *, kaddr_t);
int kl_addr_to_slot(klib_t *, kaddr_t);
int kl_valid_physmem(klib_t *, kaddr_t);

/* klib_page.c
 */
k_uint_t kl_get_pgi(klib_t *, k_ptr_t);
int kl_pte_to_pfn(klib_t *, k_ptr_t);
k_ptr_t kl_locate_pfdat(klib_t *, k_uint_t, kaddr_t, k_ptr_t, kaddr_t *);
k_ptr_t *kl_pmap_to_pde(klib_t *, k_ptr_t, kaddr_t, k_ptr_t, kaddr_t *);

/* klib_proc.c
 */
kaddr_t kl_proc_to_kthread(klib_t *, k_ptr_t, int *);
kaddr_t kl_pid_to_proc(klib_t *, int);
int kl_uthread_to_pid(klib_t *, k_ptr_t);

/* klib_struct.c
 */
k_ptr_t kl_get_struct(klib_t *, kaddr_t, int, k_ptr_t, char *);
k_ptr_t kl_get_graph_edge_s(klib_t *, k_ptr_t, kaddr_t ,int, k_ptr_t, int);
k_ptr_t kl_get_graph_info_s(klib_t *, k_ptr_t, kaddr_t, int, k_ptr_t, int);
k_ptr_t kl_get_graph_vertex_s(klib_t *, kaddr_t, int, k_ptr_t, int);
#ifdef ANON_ITHREADS
k_ptr_t kl_get_ithread_s(klib_t *, kaddr_t, int, int);
#endif
k_ptr_t kl_get_kthread(klib_t *, kaddr_t, int);
kaddr_t kl_get_pda_s(klib_t *, kaddr_t, k_ptr_t);
k_ptr_t kl_get_pde(klib_t *, kaddr_t, k_ptr_t);
k_ptr_t kl_get_proc(klib_t *, kaddr_t, int, int);
k_ptr_t kl_get_sthread_s(klib_t *, kaddr_t, int, int);
k_ptr_t kl_get_uthread_s(klib_t *, kaddr_t, int, int);
k_ptr_t kl_get_xthread_s(klib_t *, kaddr_t, int, int);

/* klib_util.c
 */
k_uint_t kl_get_bit_value(k_ptr_t, int, int, int);
int kl_shift_value(k_uint_t);
void kl_hold_signals();
void kl_release_signals();
int kl_get_curcpu(klib_t *, k_ptr_t);
kaddr_t kl_pda_s_addr(klib_t *, int);
int kl_get_cpuid(klib_t *, kaddr_t);
kaddr_t kl_nodepda_s_addr(klib_t *, int);
int kl_get_nodeid(klib_t *, kaddr_t);
int kl_cpu_nasid(klib_t *, int);
kaddr_t kl_NMI_regbase(klib_t *, int);
k_ptr_t kl_NMI_eframe(klib_t *, int, kaddr_t *);
int kl_NMI_saveregs(klib_t *, int, kaddr_t *, kaddr_t *, kaddr_t *, kaddr_t *);
kaddr_t kl_bhv_pdata(klib_t *, kaddr_t);
kaddr_t kl_bhv_vobj(klib_t *, kaddr_t);
kaddr_t kl_bhvp_pdata(klib_t *, k_ptr_t);
kaddr_t kl_bhvp_vobj(klib_t *, k_ptr_t);
void kl_hold_signals();
void kl_release_signals();
void kl_enqueue(element_t **, element_t *);
element_t *kl_dequeue(element_t **);
int kl_findqueue(element_t **, element_t *);
void kl_remqueue(element_t **, element_t *);
int max(int, int);
btnode_t *alloc_btnode(char *);
void free_btnode(btnode_t *);
int btnode_height(btnode_t *);
void set_btnode_height(btnode_t *);
int insert_btnode(btnode_t **, btnode_t *, int);
void swap_btnode(btnode_t **, btnode_t **);
int delete_btnode(btnode_t **, btnode_t *, void(*)(), int);
void single_left(btnode_t **);
void single_right(btnode_t **);
void double_left(btnode_t **);
void double_right(btnode_t **);
void balance_tree(btnode_t **);
btnode_t *find_btnode(btnode_t *, char *, int *);
string_table_t *kl_init_string_table(klib_t *, int);
void kl_free_string_table(klib_t *, string_table_t *);
char *kl_get_string(klib_t *, string_table_t *, char *, int);
k_ptr_t klib_alloc_block(klib_t *, int, int);
void klib_free_block(klib_t *, k_ptr_t);
k_ptr_t kl_sp_to_pdap(klib_t *, kaddr_t, k_ptr_t);
int kl_is_cpu_stack(klib_t *, kaddr_t, k_ptr_t);

#ifdef __cplusplus
}
#endif
#endif

