#ident "$Header: "

#include <klib/kl_page.h>
#include <klib/kl_struct_sizes.h>
#include <klib/kl_hwgraph.h>

#define HUBMASK1    0xf000000000000000
#define HUBMASK2    0xff00000000000000

/* the following is copied directly from
 * from <sys/SN/SN0/addrs.h>
 */
#define HSPEC_BASE  0x9000000000000000
#define IO_BASE     0x9200000000000000
#define MSPEC_BASE  0x9400000000000000
#define UNCAC_BASE  0x9600000000000000

#define IRIX6_5			5
#define IRIX_SPECIAL	0

/* PANIC_TYPE indicates type of panic
 */
typedef enum {
	no_panic,    /* Looking at a live system */
	reg_panic,   /* internal panic */
	nmi_panic,   /* NMI induced core dump */
} PANIC_TYPE;

/* System information (hardware configuration, etc.)
 */
typedef struct sysinfo_s {
	int32           IP;             /* System type (e.g., IP19, IP22, etc.)  */
	int32           physmem;        /* physical memory installed (pages)     */
	int32           numcpus;        /* number of CPUs installed              */
	int32           maxcpus;        /* Maximum number of CPUs as configured  */

	/* TLB dump information
	 */
	int32           ntlbentries;    /* number of tlb entries                 */
	int32           tlbentrysz;     /* size of tlb dump entry                */
	int32           tlbdumpsize;    /* size of tlb dump (per CPU)            */

	/* Node information
	 */
	int32           numnodes;       /* Number of nodes                       */
	int32           maxnodes;       /* Maximum number of nodes               */
	int32           master_nasid;   /* NASID of master node                  */
	int32           nasid_shift;    /* NASID_SHIFT                           */
	int32           slot_shift;     /* SLOT_SHIFT                            */
	int32           parcel_shift;   /* PARCEL_SHIFT                          */
	int32           parcel_bitmask;   /* PARCEL_BITMASK                      */
	int32           parcels_per_slot; /* Number of parcels per slot          */
	int32           slots_per_node;   /* MD_MEM_BANKS                        */
	uint64          mem_per_node;     /* Maximum bytes phys memory per node  */
	uint64          mem_per_slot;   /* Maximum bytes of phys memory per slot */
	uint64          nasid_bitmask;  /* NASID_BITMASK                         */
	uint64          slot_bitmask;   /* SLOT_BITMASK                          */

	uint64          systemsize;     /* Pages of memory installed in system   */
	uint64          sysmemsize;     /* MB of memory installed in system      */
	kaddr_t         kend;           /* Pointer to end of kernel              */
	uint64          mem_per_bank;   /* Maximum bytes of phys memory per bank */
									/* This is a constant equal to 512 MB.   */

	/* XXX - Memory configuration info ??? */

} sysinfo_t;

/* Kernel information
 */
typedef struct kerninfo_s {

	int32           irix_rev;    /* OS revision level (e.g., IRIX6_4)        */
	int32           syssegsz;    /* size of system segment                   */
	int32           nprocs;      /* number of configured procs (from var)    */

	/* Values used in virtual to physical address translation, macros, etc.
	 */
	int32           regsz;       /* Number of bytes in a register (always 8) */
	int32           pagesz;      /* Number of bytes in a page                */
	int32           nbpw;        /* Number of bytes per word                 */
	int32           nbpc;        /* Number of bytes per click                */
	int32           ncps;        /* Number of clics per segment              */
	int32           nbps;        /* Number of bytes per segment              */
	int32           pnumshift;   /* PFN shift                                */
	kaddr_t         to_phys_mask; /* virtual to physical address mask        */
	kaddr_t         ram_offset;  /* Offset to first memory address           */
	uint64          maxpfn;      /* Largest possible physical page number    */
	kaddr_t         maxphys;     /* Largest possible physical address        */

	/* Information for accessing mapped pages in the proc/uthread (ublock,
	 * kernelstack, and kextstack).
	 */
	int32           usize;       /* Number of mapped pages for proc/uthread  */
	int32           extusize;    /* non-zero - supports kstack extension     */
	int32           upgidx;      /* Index in p_upgs[] for ublock             */
	int32           kstkidx;     /* Index in p_upgs[] for kernel stack       */
	int32           extstkidx;   /* Index in p_upgs[] for kstack extension   */

	/* Addresses and sizes of the various address segments
	 */
	kaddr_t         kubase;      /* Base address of user address space       */
	uint64          kusize;      /* Size of user segment                     */
	kaddr_t         k0base;      /* Base address of K0 segment               */
	uint64          k0size;      /* Size of the K0 segment                   */
	kaddr_t         k1base;      /* Base address of K1 segment               */
	uint64          k1size;      /* Size of the K1 segment                   */
	kaddr_t         k2base;      /* Base address of K2 segment               */
	uint64          k2size;      /* Size of the K2 segment                   */
	kaddr_t         kernstack;   /* High address of kernel stack             */
	kaddr_t         kernelstack; /* Start address of kernel stack            */
	kaddr_t         kextstack;   /* Start address of extension stack         */
	kaddr_t         kptebase;    /* Start address of K2 memory               */
	kaddr_t         kpte_usize;
	kaddr_t         kpte_shdubase;

	/* Node memory information
	 */

	/* Mapped kernel address information
	 */
	kaddr_t         mapped_ro_base;   /* read-only address base              */
	kaddr_t         mapped_rw_base;   /* read-write address base             */
	kaddr_t         mapped_page_size; /* Size of the mapped segment          */

} kerninfo_t;

/* Kernel data (global variables, counts, etc.) used by KLIB.
 */
typedef struct kerndata_s {
	PANIC_TYPE 		panic_type;  /* type of panic (internal, NMI, or none)   */
	int				dumpcpu;     /* ID of CPU that panicked the system       */
	int 			graph_size;  /* actual size of hwgraph (not struct size) */
	int 			vertex_size; /* Actual size of vertex (not struct size)  */ 

	kaddr_t			dumpkthread; /* Kernel address of kthread that paniced   */
	kaddr_t 		defkthread;  /* kernel address of default kthread        */
	kaddr_t 		hwgraphp;    /* kernel address of hwgraph                */
	kaddr_t 		activefiles; /* linked list of open files (DEBUG only)   */
	kaddr_t 		dumpproc;    /* kernel address of panicing process       */
	kaddr_t			error_dumpbuf; /* HW error message buffer                */
	kaddr_t			kptbl;		 /* kernel address of page table 			 */
	kaddr_t 		lbolt;       /* current clock tick                       */
	kaddr_t 		mlinfolist;  /* loadable module info                     */
	kaddr_t			Nodepdaindr; /* start address of nodepdaindr array       */
	kaddr_t			pdaindr;	 /* start address of pdaindr array           */
	kaddr_t 		pid_active_list; /* kernel address of active pids        */
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
} kerndata_t;

typedef struct libkern_info_s {
	sysinfo_t		*k_sysinfo;
	kerninfo_t		*k_kerninfo;
	kerndata_t		*k_kerndata;
	pdeinfo_t		*k_pdeinfo;
	struct_sizes_t  *k_struct_sizes;
} libkern_info_t;

#define	LIBKERN_DATA	((libkern_info_t *)(KLP)->k_libkern.k_data)

/* From the sysinfo_s struct
 */
#define K_IP                (LIBKERN_DATA->k_sysinfo->IP)
#define K_PHYSMEM           (LIBKERN_DATA->k_sysinfo->physmem)
#define K_NUMCPUS           (LIBKERN_DATA->k_sysinfo->numcpus)
#define K_MAXCPUS           (LIBKERN_DATA->k_sysinfo->maxcpus)
#define K_NTLBENTRIES       (LIBKERN_DATA->k_sysinfo->ntlbentries)
#define K_TLBENTRYSZ        (LIBKERN_DATA->k_sysinfo->tlbentrysz)
#define K_TLBDUMPSIZE       (LIBKERN_DATA->k_sysinfo->tlbdumpsize)
#define K_NUMNODES          (LIBKERN_DATA->k_sysinfo->numnodes)
#define K_MAXNODES          (LIBKERN_DATA->k_sysinfo->maxnodes)
#define K_MASTER_NASID      (LIBKERN_DATA->k_sysinfo->master_nasid)
#define K_NASID_SHIFT       (LIBKERN_DATA->k_sysinfo->nasid_shift)
#define K_SLOT_SHIFT        (LIBKERN_DATA->k_sysinfo->slot_shift)
#define K_PARCEL_SHIFT      (LIBKERN_DATA->k_sysinfo->parcel_shift)
#define K_PARCEL_BITMASK    (LIBKERN_DATA->k_sysinfo->parcel_bitmask)
#define K_PARCELS_PER_SLOT  (LIBKERN_DATA->k_sysinfo->parcels_per_slot)
#define K_SLOTS_PER_NODE    (LIBKERN_DATA->k_sysinfo->slots_per_node)
#define K_MEM_PER_NODE      (LIBKERN_DATA->k_sysinfo->mem_per_node)
#define K_MEM_PER_SLOT      (LIBKERN_DATA->k_sysinfo->mem_per_slot)
#define K_MEM_PER_BANK      (LIBKERN_DATA->k_sysinfo->mem_per_bank)
#define K_NASID_BITMASK     (LIBKERN_DATA->k_sysinfo->nasid_bitmask)
#define K_SLOT_BITMASK      (LIBKERN_DATA->k_sysinfo->slot_bitmask)
#define K_SYSTEMSIZE        (LIBKERN_DATA->k_sysinfo->systemsize)
#define K_SYSMEMSIZE        (LIBKERN_DATA->k_sysinfo->sysmemsize)
#define K_END               (LIBKERN_DATA->k_sysinfo->kend)

/* From the kerninfo_s struct
 */
#define K_IRIX_REV          (LIBKERN_DATA->k_kerninfo->irix_rev)
#define K_SYSSEGSZ          (LIBKERN_DATA->k_kerninfo->syssegsz)
#define K_NPROCS            (LIBKERN_DATA->k_kerninfo->nprocs)

#define K_REGSZ             (LIBKERN_DATA->k_kerninfo->regsz)
#define K_PAGESZ            (LIBKERN_DATA->k_kerninfo->pagesz)
#define K_NBPW              (LIBKERN_DATA->k_kerninfo->nbpw)
#define K_NBPC              (LIBKERN_DATA->k_kerninfo->nbpc)
#define K_NCPS              (LIBKERN_DATA->k_kerninfo->ncps)
#define K_NBPS              (LIBKERN_DATA->k_kerninfo->nbps)
#define K_PNUMSHIFT         (LIBKERN_DATA->k_kerninfo->pnumshift)
#define K_TO_PHYS_MASK      (LIBKERN_DATA->k_kerninfo->to_phys_mask)
#define K_RAM_OFFSET        (LIBKERN_DATA->k_kerninfo->ram_offset)
#define K_MAXPFN            (LIBKERN_DATA->k_kerninfo->maxpfn)
#define K_MAXPHYS           (LIBKERN_DATA->k_kerninfo->maxphys)
#define K_USIZE             (LIBKERN_DATA->k_kerninfo->usize)
#define K_EXTUSIZE          (LIBKERN_DATA->k_kerninfo->extusize)
#define K_UPGIDX            (LIBKERN_DATA->k_kerninfo->upgidx)
#define K_KSTKIDX           (LIBKERN_DATA->k_kerninfo->kstkidx)
#define K_EXTSTKIDX         (LIBKERN_DATA->k_kerninfo->extstkidx)

#define K_KUBASE            (LIBKERN_DATA->k_kerninfo->kubase)
#define K_KUSIZE            (LIBKERN_DATA->k_kerninfo->kusize)
#define K_K0BASE            (LIBKERN_DATA->k_kerninfo->k0base)
#define K_K0SIZE            (LIBKERN_DATA->k_kerninfo->k0size)
#define K_K1BASE            (LIBKERN_DATA->k_kerninfo->k1base)
#define K_K1SIZE            (LIBKERN_DATA->k_kerninfo->k1size)
#define K_K2BASE            (LIBKERN_DATA->k_kerninfo->k2base)
#define K_K2SIZE            (LIBKERN_DATA->k_kerninfo->k2size)
#define K_KERNSTACK         (LIBKERN_DATA->k_kerninfo->kernstack)
#define K_KERNELSTACK       (LIBKERN_DATA->k_kerninfo->kernelstack)
#define K_KEXTSTACK         (LIBKERN_DATA->k_kerninfo->kextstack)
#define K_KPTEBASE          (LIBKERN_DATA->k_kerninfo->kptebase)
#define K_KPTE_USIZE        (LIBKERN_DATA->k_kerninfo->kpte_usize)
#define K_KPTE_SHDUBASE     (LIBKERN_DATA->k_kerninfo->kpte_shdubase)

#define K_MAPPED_RO_BASE    (LIBKERN_DATA->k_kerninfo->mapped_ro_base)
#define K_MAPPED_RW_BASE    (LIBKERN_DATA->k_kerninfo->mapped_rw_base)
#define K_MAPPED_PAGE_SIZE  (LIBKERN_DATA->k_kerninfo->mapped_page_size)

/* From the kerndata_s struct
 */
#define K_PANIC_TYPE   		(LIBKERN_DATA->k_kerndata->panic_type)
#define K_DUMPCPU   		(LIBKERN_DATA->k_kerndata->dumpcpu)
#define K_GRAPH_SIZE   		(LIBKERN_DATA->k_kerndata->graph_size)
#define K_VERTEX_SIZE   	(LIBKERN_DATA->k_kerndata->vertex_size)

#define K_DUMPKTHREAD   	(LIBKERN_DATA->k_kerndata->dumpkthread)
#define K_DEFKTHREAD   		(LIBKERN_DATA->k_kerndata->defkthread)
#define K_HWGRAPHP   		(LIBKERN_DATA->k_kerndata->hwgraphp)
#define K_ACTIVEFILES   	(LIBKERN_DATA->k_kerndata->activefiles)

#define K_DUMPPROC   		(LIBKERN_DATA->k_kerndata->dumpproc)
#define K_ERROR_DUMPBUF   	(LIBKERN_DATA->k_kerndata->error_dumpbuf)
#define K_KPTBL   			(LIBKERN_DATA->k_kerndata->kptbl)
#define K_LBOLT   			(LIBKERN_DATA->k_kerndata->lbolt)
#define K_MLINFOLIST   		(LIBKERN_DATA->k_kerndata->mlinfolist)
#define K_NODEPDAINDR   	(LIBKERN_DATA->k_kerndata->Nodepdaindr)
#define K_PDAINDR   		(LIBKERN_DATA->k_kerndata->pdaindr)
#define K_PID_ACTIVE_LIST	(LIBKERN_DATA->k_kerndata->pid_active_list)
#define K_PIDTAB   			(LIBKERN_DATA->k_kerndata->pidtab)
#define K_PFDAT   			(LIBKERN_DATA->k_kerndata->pfdat)
#define K_PUTBUF   			(LIBKERN_DATA->k_kerndata->putbuf)
#define K_STHREADLIST   	(LIBKERN_DATA->k_kerndata->sthreadlist)
#define K_STRST   			(LIBKERN_DATA->k_kerndata->strst)
#define K_TIME   			(LIBKERN_DATA->k_kerndata->time)
#define K_XTHREADLIST   	(LIBKERN_DATA->k_kerndata->xthreadlist)

#define K_PIDTABSZ   		(LIBKERN_DATA->k_kerndata->pidtabsz)
#define K_PID_BASE   		(LIBKERN_DATA->k_kerndata->pid_base)

#define K_DUMPREGS   		(LIBKERN_DATA->k_kerndata->dumpregs)
#define K_KPTBLP   			(LIBKERN_DATA->k_kerndata->kptblp)
#define K_HWGRAPH   		(LIBKERN_DATA->k_kerndata->hwgraph)

#define IS_PANIC   			(LIBKERN_DATA->k_kerndata->panic_type == reg_panic)
#define IS_NMI     			(LIBKERN_DATA->k_kerndata->panic_type == nmi_panic)

/* Generalized macros for pointing at different data types at particular
 * offsets in kernel structs.
 */
#define ADDR(p, s, f)  	((unsigned)(p) + kl_member_offset(s, f))
#define K_PTR(p, s, f) 	((k_ptr_t)ADDR(p, s, f))
#define CHAR(p, s, f)  	((char *)ADDR(p, s, f))

#define PTRSZ64 ((K_PTRSZ == 64) ? 1 : 0)
#define PTRSZ32 ((K_PTRSZ == 32) ? 1 : 0)

/* Basic address macros for use with multiple platforms
 */
#define k0_to_phys(X) ((kaddr_t)(X) & K_TO_PHYS_MASK)
#define k1_to_phys(X) ((kaddr_t)(X) & K_TO_PHYS_MASK)
#define is_kseg0(X) \
	((kaddr_t)(X) >= K_K0BASE && (kaddr_t)(X) < K_K0BASE + K_K0SIZE)
#define is_kseg1(X) \
	((kaddr_t)(X) >= K_K1BASE && (kaddr_t)(X) < K_K1BASE + K_K1SIZE)
/* TFP */
#define is_kseg2(X) \
	((kaddr_t)(X) >= K_K2BASE && (kaddr_t)(X) < K_K2BASE + K_K2SIZE)
/* all else */
#define _is_kseg2(X) \
	((kaddr_t)(X) >= K_K2BASE && (kaddr_t)(X) < K_KPTE_SHDUBASE)

#define is_kpteseg(X) ((kaddr_t)(X) >= K_KPTE_SHDUBASE && \
	 (kaddr_t)(X) < K_KPTEBASE + K_KUSIZE)

#define IS_KERNELSTACK(addr) (K_EXTUSIZE ? \
	 ((addr >= K_KEXTSTACK) && (addr < K_KERNELSTACK)) : \
	((addr >= K_KERNELSTACK) && (addr < K_KERNSTACK)))

#define Btoc(X) ((kaddr_t)(X) >> K_PNUMSHIFT)
#define Ctob(X) ((kaddr_t)(X) << K_PNUMSHIFT)
#define Pnum(X) ((kaddr_t)(X) >> K_PNUMSHIFT)

/* 
 * MACRO's mostly taken from immu.h. Why ?. 
 * Because we like it that's why. 
 * We have it in the #if for those icrash files which include the 
 * kernel header files with these defines..
 */
#if !defined(KERNEL) && !defined(_KERNEL)
#define BTOST(x)          (uint)((kaddr_t)(x) / K_NBPS)
#define BTOS(x)           (uint)(((kaddr_t)(x) + (K_NBPS - 1))/ K_NBPS)
#define STOB(x)           (kaddr_t)((kaddr_t)(x) * K_NBPS)
#endif /* !defined(KERNEL) && !defined(_KERNEL) */
#define KL_HIUSRATTACH_32    0x7fff8000
#define KL_HIUSRATTACH_64    0x10000000000

/* this is the same as Number of page tbls to map user space. 
 * the same as NPGTBLS 
 */
#define KL_SEGTABSIZE        BTOS(KL_HIUSRATTACH_32) 
#define KL_SEGTABMASK        (KL_SEGTABSIZE-1)

/* This is defined as btobasetab in immu.h .. but this is the correct meaning..
 * i.e., it returns an index into the base table.
 */
#define KL_BTOBASETABINDEX(x)    ((__uint64_t)(x) / (K_NBPS * KL_SEGTABSIZE))
#define KL_BTOSEGTABINDEX(x)     (BTOST(x) & KL_SEGTABMASK)
#define KL_PHYS_TO_K0(X)         (X | K_K0BASE)

/*
 * Platform independent defines.
 */
#define VALID_PFN(PFN) valid_pfn(PFN)
#define NEXT_PFDAT(FIRST, LAST)           \
	((PLATFORM_SNXX) ? sn0_next_valid_pfdat(FIRST, LAST) : (FIRST + PFDAT_SIZE))
#define KL_PFDATTOPFN(PFDAT) kl_pfdattopfn(PFDAT)
#define KL_PFNTOPFDAT(PFN) kl_pfntopfdat(PFN)
#define KL_SLOT_SIZE (1LL << K_SLOT_SHIFT)
#define KL_SLOT_PFNSHIFT (K_SLOT_SHIFT - K_PNUMSHIFT)
#define KL_PFN_TO_SLOT_NUM(PFN) (((PFN) >> SLOT_PFNSHIFT) & K_SLOT_BITMASK)
#define KL_PFDAT_TO_SLOT_NUM(PFD) (PFN_TO_SLOT_NUM(KL_PFDATTOPFN(PFD)))
#define KL_PFN_TO_SLOT_OFFSET(PFN) ((PFN) & ((KL_SLOT_SIZE/K_NBPC)-1))
#define KL_PFDAT_TO_SLOT_OFFSET(PFD) (PFN_TO_SLOT_OFFSET(KL_PFDATTOPFN(PFD)))
#define KL_PFN_NASIDSHIFT (K_NASID_SHIFT - K_PNUMSHIFT)
#define KL_PFN_NASIDMASK (K_NASID_BITMASK << KL_PFN_NASIDSHIFT)
#define KL_PFD_NASIDMASK (K_NASID_BITMASK << K_NASID_SHIFT)
#define KL_PFN_NASID(pfn) (PLATFORM_SNXX ? ((pfn) >> KL_PFN_NASIDSHIFT) : 0)

#define Kvtokptbl(X) (&_kptbl[Pnum((kaddr_t)(X) - (kaddr_t)K_K2BASE])

#define IS_HUBSPACE(X) (((kaddr_t)X & HUBMASK1) == HSPEC_BASE)
#define IS_PHYSICAL(X) ((kaddr_t)X <= (kaddr_t)K_MAXPHYS)
#define IS_SMALLMEM(X) (IS_PHYSICAL(X) ? (Btoc(X) < 0x20000) : \
	(Btoc(kl_virtop(X, (k_ptr_t)NULL)) < 0x20000))

#define _PTE_TO_PFN(PTE) (uint)(((PTE) & K_PFN_MASK) >> K_PFN_SHIFT)
#define PTE_TO_PFN pte_to_pfn
#define PTE_TO_K0(PTE) (Ctob(PTE_TO_PFN(PTE)) | K_K0BASE)
#define PFN_TO_PHYS(PFN) Ctob(PFN)
#define PFN_TO_K0(PFN) ((PFN <= Pnum(K_K1SIZE)) ? (Ctob(PFN) | K_K0BASE) : 0)
#define PFN_TO_K1(PFN) ((PFN <= Pnum(K_K1SIZE)) ? (Ctob(PFN) | K_K1BASE) : 0)

#define PROC_TO_USER(p) KL_UINT((p), "proc", "p_user")

/* XXX -- A number of things were lifted from the sys/kthread.h file. We
 * should really be including the header file and using the values there,
 * but there is a lot of other junk that we don't want to pick up. So we
 * have to make sure that these values stay in sync with the ones in the
 * header file.
 */
#define KT_STACK_MALLOC         0x00000001
#define KT_WMUTEX               0x00000002 /* waiting on mutex */
#define KT_LOCK                 0x00000004 /* lock for k_flags */
#define KT_WACC                 0x00000008 /*  waiting for mr(acc) */
/*UNUSED                        0x00000010 */
#define KT_WSV                  0x00000020 /* waiting on sv */
#define KT_WSVQUEUEING          0x00000040 /* about to go on an SV */
#define KT_INDIRECTQ            0x00000080 /* about to go on an SV */
#define KT_WSEMA                0x00000100 /* waiting on a sema */
#define KT_WMRLOCK              0x00000200 /* waiting on a mrlock */
#define KT_ISEMA                0x00000400 /* waiting on an interrupt sema */
#define KT_WUPD                 0x00000800 /* waiting for mr(upd)lock */
#define KT_PS                   0x00001000 /* priority scheduled */
#define KT_BIND                 0x00002000 /* eligible for binding */
#define KT_NPRMPT               0x00004000 /* do not kick scheduler */
#define KT_BASEPS               0x00008000 /* orig priority scheduled */
#define KT_NBASEPRMPT           0x00010000 /* orig non-preemptive */
#define KT_PSMR                 0x00020000 /* ps because kernel mustrun */
#define KT_HOLD                 0x00040000 /* keep on this cpu */
#define KT_PSPIN                0x00080000 /* ps because kernel pin_thread */
#define KT_INTERRUPTED          0x00100000 /* thread was interrupted */
#define KT_SLEEP                0x00200000 /* thread asleep */
#define KT_NWAKE                0x00400000 /* thread cannot be interrupted */
#define KT_LTWAIT               0x00800000 /* long term wait */
#define KT_BHVINTR              0x01000000 /* thread interrupted for BHV lock*/
#define KT_SERVER               0x02000000 /* server thread */
#define KT_NOAFF                0x04000000 /* thread has no affinity */
#define KT_MUTEX_INHERIT        0x08000000 /* thread inherited through mutex */

#define KT_SYNCFLAGS \
  (KT_WSEMA|KT_WSVQUEUEING|KT_WSV|KT_WMUTEX|KT_INDIRECTQ|KT_WMRLOCK)

/* Environment types that kthread can support
 */
#define KT_XTHREAD  1
#define KT_STHREAD  2
#define KT_UTHREAD  3

#define IS_STHREAD(ktp) (KL_UINT(ktp, "kthread", "k_type") == KT_STHREAD)
#define IS_UTHREAD(ktp) (KL_UINT(ktp, "kthread", "k_type") == KT_UTHREAD)
#define IS_XTHREAD(ktp) (KL_UINT(ktp, "kthread", "k_type") == KT_XTHREAD)

/* Flag values used by the addr_convert() function
 */
#define PFN_FLAG                1
#define PADDR_FLAG              2
#define VADDR_FLAG              3

/* Flag that tells kl_is_valid_kaddr() to perform a word aligned check
 */
#define WORD_ALIGN_FLAG			1

/* Macros useful when working with pidtab[]
 */
#define K_PID_INDEX(PID) (((PID) - K_PID_BASE) % K_PIDTABSZ)
#define K_PID_SLOT(PID) (K_PIDTAB + (K_PID_INDEX(PID) * PID_SLOT_SIZE))

/* Create a hash list of all the vfiles in the system.
 * Do not keep duplicates of the viles.
 * Each element in the hash table is a (list_of_ptrs_t *).
 */
#define VFILE_HASHSIZE 127

#define MD_MEM_BANKS     8  /* Number of banks per node in M_MODE for SN0. */
#define MD_BANK_SHFT            29                      /* log2(512 MB)     */

extern node_memory_t **node_memory;

/**
 ** system memory operation function prototypes
 **/
int check_node_memory();

int map_system_memory();

/**
 ** kthread operation function prototypes
 **/
int kl_set_defkthread(
	kaddr_t		/* kernel address of kthread */);

kaddr_t kl_get_curkthread(
	int			/* cpuid */);

kaddr_t kl_kthread_addr(
	k_ptr_t		/* pointer to block of memory containing kthread struct */);

k_ptr_t kl_kthread_name(
	k_ptr_t		/* pointer to block of memory containing kthread struct */);

int kl_kthread_type(
	k_ptr_t		/* pointer to block of memory containing kthread struct */);

kaddr_t kl_kthread_stack(
	kaddr_t		/* kernel address of kthread */, 
	int*		/* pointer to where the stack size will be placed */);

/**
 ** proc/uthread operation function prototypes
 **/
kaddr_t kl_proc_to_kthread(
	k_ptr_t		/* pointer to block of memory containing proc struct */, 
	int*		/* pointer to where the kthread count is placed */);

int kl_uthread_to_pid(
	k_ptr_t		/* pointer to block of memory containing uthread struct */);

kaddr_t kl_pid_to_proc(
	int			/* pid */);

/**
 ** kernel structure operation function prototypes
 **/
k_uint_t kl_get_struct(
	kaddr_t		/* kernel address of the struct */, 
	int			/* struct size */, 
	k_ptr_t		/* pointer to buffer where struct will be placed */, 
	char*		/* name of the struct */);

k_ptr_t kl_get_ml_info(
	kaddr_t     /* kernel address of ml_inof struct */,
	int         /* mode */,
	k_ptr_t     /* pointer to buffer where ml_info struct will be placed */);

k_ptr_t kl_get_kthread(
	kaddr_t		/* kernel address of kthread struct */, 
	int			/* flags */);

kaddr_t kl_get_nodepda_s(
	kaddr_t		/* address or id of node */, 
	k_ptr_t 	/* pointer to buffer where nodepda_s struct will be placed */);

kaddr_t kl_get_pda_s(
	kaddr_t		/* address or id of cpu */, 
	k_ptr_t 	/* pointer to buffer where pda_s struct will be placed */);

k_ptr_t kl_get_pde(
	kaddr_t 	/* address of pde struct */, 
	k_ptr_t 	/* pointer to buffer where pde struct will be placed */);

k_ptr_t kl_get_proc(
	kaddr_t 	/* pid or address of proc */, 
	int 		/* mode - (0 == address), (1 == pid) */, 
	int 		/* flags (K_TEMP/K_PERM) */);

k_ptr_t kl_get_sthread_s(
	kaddr_t 	/* address of sthread_s struct */, 
	int 		/* mode (at this time must be 2) */, 
	int 		/* flags (K_TEMP/K_PERM) */);

k_ptr_t kl_get_uthread_s(
	kaddr_t 	/* address of uthread_s struct */, 
	int 		/* mode (at this time must be 2) */, 
	int 		/* flags (K_TEMP/K_PERM) */);

k_ptr_t kl_get_xthread_s(
	kaddr_t 	/* address of xthread_s struct */, 
	int 		/* mode (at this time must be 2) */, 
	int 		/* flags (K_TEMP/K_PERM) */);

/**
 ** Misc. operation function prototypes
 **/

k_ptr_t kl_sp_to_pdap(
	kaddr_t		/* kernel address of stack (stack pointer) */, 
	k_ptr_t		/* pointer to buffer where pda_s struct will be placed */);

int kl_is_cpu_stack(
	kaddr_t		/* kernel address of stack */, 
	k_ptr_t		/* pointer to a buffer containing pda_s struct */);

int kl_get_curcpu(
	k_ptr_t		/* pointer to buffer where pda_s struct will be placed */);

kaddr_t kl_pda_s_addr(
	int			/* cpuid */);

int kl_get_cpuid(
	kaddr_t		/* kernel address of pda_s struct */);

kaddr_t kl_nodepda_s_addr(
	int			/* nodeid */);

int kl_get_nodeid(
	kaddr_t		/* kernel address of nodepda_s struct */);

int kl_cpu_nasid(
	int			/* cpuid */);

kaddr_t kl_NMI_regbase(
	int			/* cpuid */);

k_ptr_t kl_NMI_eframe(
	int			/* cpuid */,
	kaddr_t*	/* pointer to where eframe_s address will be placed */);

int kl_NMI_saveregs(
	int			/* cpuid */, 
	kaddr_t*	/* pointer to where errorepc will be placed */, 
	kaddr_t*	/* pointer to where epc will be placed */, 
	kaddr_t*	/* pointer to where sp will be placed */, 
	kaddr_t*	/* pointer to where ra will be placed */);

kaddr_t kl_bhv_pdata(
	kaddr_t		/* kernel address of bhv_desc struct */);

kaddr_t kl_bhv_vobj(
	kaddr_t		/* kernel address of bhv_desc struct */);

kaddr_t kl_bhvp_pdata(
	k_ptr_t		/* pointer to block containing bhv_desc struct */);

kaddr_t kl_bhvp_vobj(
	k_ptr_t		/* pointer to block containing bhv_desc struct */);

/**
 ** klib_s struct operation function prototypes 
 **/

/* Set the filename containing a dump of an icrashdef_s struct
 */
int kl_set_icrashdef(
	char* 		/* pathname to file containing icrashdef struct */); 

k_ptr_t kl_get_utsname(int);

int libkern_init();

/**
 ** memory access operation function prototypes
 **/
k_uint_t kl_read_block(
	kaddr_t		/* kernel address to start reading at */,
	unsigned	/* size of memory block to read in */,
	k_ptr_t		/* pointer to buff where contents of memory will be placed */,
	k_ptr_t		/* pointer to block of memory containing kthread struct */,
	char*		/* name of what's being read in */);

k_uint_t kl_get_block(
	kaddr_t		/* kernel address to start reading at */, 
	unsigned	/* size of memory block to read in */, 
	k_ptr_t		/* pointer to buff where contents of memory will be placed */, 
	char*		/* name of what's being read in */);

k_uint_t kl_get_kaddr(
	kaddr_t		/* kernel address to start reading at */, 
	k_ptr_t		/* pointer to buff where contents of memory will be placed */, 
	char*		/* name of what's being read in */);

kaddr_t kl_kaddr_to_ptr(
	kaddr_t 	/* kernel address to start reading at */);

k_uint_t kl_uint(
	k_ptr_t 	/* pointer to block containing struct */, 
	char*		/* strut name */, 
	char*		/* member name */, 
	unsigned offset);

k_int_t kl_int(
	k_ptr_t 	/* pointer to block containing struct */, 
	char*		/* strut name */, 
	char*		/* member name */, 
	unsigned offset);

kaddr_t kl_kaddr(
	k_ptr_t 	/* pointer to block containing struct */, 
	char*		/* strut name */, 
	char*		/* member name */);

kaddr_t kl_reg(
	k_ptr_t 	/* pointer to block containing struct */, 
	char*		/* strut name */, 
	char*		/* member name */);

kaddr_t kl_kaddr_val(
	k_ptr_t		/* pointer to block containing a kernel address */);

/**
 ** memory page operation function prototypes
 **/
k_uint_t kl_get_pgi(
	k_ptr_t 	/* pointer to block containing a pde struct */);

int kl_pte_to_pfn(
	k_ptr_t 	/* pointer to block containing a pde struct */);

k_ptr_t kl_locate_pfdat(
	k_uint_t	/* page number */, 
	kaddr_t		/* tag */, 
	k_ptr_t		/* pointer to buffer containing a pfdat struct */, 
	kaddr_t*	/* pointer to buffer where pfdat address will be placed */);

k_ptr_t *kl_pmap_to_pde(
	k_ptr_t 	/* pointer to buffer containing a pmap struct */, 
	kaddr_t 	/* kernel address to search for */, 
	k_ptr_t 	/* pointer to buffer where pde struct will be placed */, 
	kaddr_t*	/* pointer to where kernel address of pde will be placed */);

/**
 ** virtual memory translation operation function prototypes
 **/
kaddr_t kl_virtop(
	kaddr_t 	/* virtual address to translate */, 
	k_ptr_t 	/* pointer to buffer containing a kthread struct */);

int kl_is_valid_kaddr(
	kaddr_t 	/* kernel address */, 
	k_ptr_t 	/* pointer to buffer containing a kthread struct */, 
	int 		/* flags */);

int kl_addr_convert(
	kaddr_t		/* kernel_address/physican_address/pfn to convert */, 
	int 		/* flag (PFN_FLAG/PADDR_FLAG/VADDR_FLAG) */, 
	kaddr_t*	/* pointer to buffer where physical address will be placed */, 
	uint*		/* pointer to buffer where pfn will be placed */, 
	kaddr_t*	/* pointer to buffer where k0 address will be placed */, 
	kaddr_t*	/* pointer to buffer where k1 address will be placed */, 
	kaddr_t*	/* pointer to buffer where k2 address will be placed */);

int is_mapped_kern_ro(	
	kaddr_t 	/* kernel address */);

int is_mapped_kern_rw(
	kaddr_t 	/* kernel address */);

kaddr_t kl_pfn_to_K2(
	uint 		/* pfn */);

int kl_get_addr_mode(
	kaddr_t 	/* kernel address */);

int kl_addr_to_nasid(
	kaddr_t 	/* kernel address */);

int kl_addr_to_slot(
	kaddr_t 	/* kernel address */);
