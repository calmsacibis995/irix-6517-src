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
#ifndef __AS_PRIVATE_H__
#define __AS_PRIVATE_H__

#ident  "$Revision: 1.46 $"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Address Space - private definitions
 *
 * These distributed objects represent the virtual address space of
 * process or shared process. The term 'physical' in this context refers
 * to the physical object representation - NOT physical address space.
 *
 * XXX changes:
 * 1) no more hasprda
 * 4) keep track of shared vs private size and shared size rather than
 *	using recalcrss
 * 6) move allocate of prp from mmap to fs_map_subr.. --> get preg out of VOP
 *	change *addr in VOP_MAP. spec_map needs to call addspace ..
 * 7) io/zero.c - change to new mmap return magic cookie that means to
 *	just do the work in asvo_addspace.. Or, have zero register w/ spec..
 * 8) add execunmap rather than detachreg's in error case in elfmap
 * 9) add prot argument to cdmap/bdmap. change int to unsigned.
 * 10) change vt_handle to be a gen number kept at server per pregion.??
 *
 * XXX todo:
 * 1) segtbl's
 * 2) avail[sr]mem! need block grant - which subsys owns??
 * 3) need to remove most VASID_TO_PAS!
 * 4) setup_wired_tlb really shouldn't be called from below AS layer.
 *
 * Policy:
 *	nlockpg and rss values are used in 2 ways - as write mostly read
 *	rarely counters and as policy data for computing maximal
 *	resource usage.
 *	In the first case, a shared-update token works well.
 *	The second case - policy data - is a read-alot global value
 *	The policy we wish to enforce is a 'not-to-exceed' number -
 *	but we clearly can't require exclusive access to these values.
 *	We use an ABG - Adaptive Block Grant - algorithm
 *		This algorithm has the server hold the true maximal
 *		value (the total number of resource rights)
 *		(as defined by some administrative setup) and the server
 *		doles out blocks of resource usage rights to clients.
 *		As long as a client has enough rights, they can use a
 *		shared-update token to change the value, and the client
 *		knows a-priori that the overall maximum hasn't been exceeded.
 *		Once it runs out it asks the server for more. To read
 *		the current value, a client gets the read token which
 *		effectively forces all clients to send back their grants
 *		with a count of how many they have allocated.
 *
 * Architecture -
 *	The main idea is that all major operations can occur completely
 *	on the client side - so the client data structures are pretty
 *	much complete representations of the state of the address space.
 *	The exceptions are sizes, and the nlockpg & rss values.
 *	This means that the aso_t structure is both the client side
 *	'cached' data as well as the data structure used when there is
 *	no distribution of address space (e.g. a non-shared process)
 *	
 *	There are 2 different interposition layers - a local and a distributed
 *	one. These provide the special data unique to the implementation.
 *	This includes things like tokens and block-grant administration.
 */
#include <sys/types.h>
#include <ksys/as.h>
#include <ksys/behavior.h>
#include "region.h"
#include <sys/resource.h>

/*
 * physical address space object. There are one of these per address space
 * (either process or shared process)
 * Private address space regions are hung off of here.
 *
 * This data structure (and ppas_s) is used as the server side, local, and
 *	client side. Much of the synchronization is in the interposition
 *	layers.
 *
 * pas_aspacelock -
 *            Grab the lock for AS_EXCL, if the operation will change:
 *              1) pregion list
 *              2) regva, pgoff, pgsz, r_list
 *		3) changing the private list
 *            Otherwise grab it AS_SHARED if accessing most anything
 *
 * pas_preglock - pregion lock
 *		used for insertion/deletion from pas_pregion list - both
 *		shared and private.
 * pas_plistlock - insert/delete/lookup from plist
 * pas_refcnt - ref count on this node. This is the number of ppas's -
 *		nothing to do with # of folks accessing - is used
 *		primarily to determine when last one leaves AS
 * SIZES:
 *	rss size is defined to be the total valid pages in all shared and
 *		private mappings of all share group members. This is kept
 *		as a count of shared valid pages in pas_t and a count per
 *		share group member in ppas_t.
 * sizes - pas_size + ppas_size gives size of current process (for all
 *	processes including standard processes).
 *
 * Locking:
 * pas_refcnt - atomic inc/dec
 * pas_brk* - pas_brklock
 * pas_lockdown - pas_brklock/atomic_ops
 *	We really need to atomically lock and set for data space w.r.t changing
 *	of the brk value. The rest of pas_lockdown is carefully done
 *	with atomic ops...
 * pas_plist - pas_plistlock for ins/del/lookup
 * pas_pregions - pas_aspacelock && pas_preglock for ins/del - scan with either
 * pas_hiusrattach - constant
 * pas_nlockpg - none - use atomic increment.
 * pas_flags - atomic or/and
 * pas_rss/ppas_rss - pas_preglock
 * pas_size/ppas_size - pas_aspacelock && pas_preglock for modify, read w/ either
 * pas_faultlog - pas_brklock (not used much)
 * pas_vpagg - Used for miser VM resource control. vpagg points to the
 * process aggregate to which the process belongs. It contains VM resources
 * limits for that aggregate and the pool from which to allocate resources.
 */
typedef struct pas_s {			/* Address Space Object */
	bhv_desc_t pas_bhv;		/* behavior */
	mrlock_t pas_aspacelock;	/* global protector for ins/del */
	struct ppas_s *pas_plist;	/* ptr to private list */
	mrlock_t pas_plistlock;		/* protect plist */
	int pas_refcnt;			/* ref count */
	unsigned int pas_flags;	
	preg_set_t pas_pregions;	/* list of pregions */
	mutex_t pas_preglock;		/* pregion lock */
	mutex_t pas_brklock;		/* brk* lock */
	uvaddr_t pas_brkbase;		/* base addr of heap */
	size_t pas_brksize;		/* heap size */
	uvaddr_t pas_lastspbase;	/* stack allocator hint */
	uvaddr_t pas_nextaalloc;	/* allocaddr allocator hint */
	uvaddr_t pas_hiusrattach;	/* max address for attach */
	uint_t pas_lockdown;		/* plock(2) support */
	pgcnt_t pas_size;		/* sz in pages of shared regions */
	pgcnt_t pas_physsize;		/* sz in pages of shared regions that
					 * map PHYS devices
					 */
	ulong_t pas_size_gen;		/* address change generation # */
	pgcnt_t pas_rss;		/* rss in pages of shared regions */
	pgcnt_t pas_bgremainrss;	/* remaining pgs in grant */
	pgcnt_t pas_bgremainlockpg;	/* remaining pgs in grant */
	rlim_t pas_datamax;		/* max data size in bytes */
	rlim_t pas_vmemmax;		/* max total size in bytes */
	uvaddr_t pas_stkbase;		/* base addr of stack (non-SPROC) */
	size_t pas_stksize;		/* stack size in bytes (ditto) */
	rlim_t pas_stkmax;		/* max stack size in bytes (ditto) */
	rlim_t pas_rssmax;		/* max RSS size in bytes */
	struct pmap *pas_pmap;		/* page map pointer */
	pgcnt_t pas_nlockpg;		/* TEMP - # locked pages in AS */
	/*
	 * the following 2 fields are used for local and server and ignored
	 * for client
	 */
	pgcnt_t pas_maxrss;		/* max permitted rss (in pages) */
	pgcnt_t pas_maxlockpg;		/* max permitted locked pages */
	pgno_t	pas_smem_reserved; 	/* Pre-reserved smem for miser jobs */
	struct pregion *pas_tsave;	/* 'tsaved' pregions */
	cpumask_t pas_isomask;		/* isolation cpus AS is running on */
        struct aspm *pas_aspm;		/* for memory placement policy mgmt */
	struct faultlog *pas_faultlog;	/* for /proc */
	struct	vpagg_s *pas_vpagg;	/* Process aggregate for miser */
} pas_t;

/* pas_flags */
#define PAS_SHARED	0x0001		/* pas could have more than one */
					/* thread sharing pmaps */
#define PAS_PSHARED	0x0002		/* pthreaded process */
#define PAS_LOCKPRDA	0x0004		/* AS has locked prda */
#define PAS_NOSWAP	0x0008		/* ran out of swap on this AS */
#define PAS_64		0x0010		/* AS is for a 64 bit program */
#define PAS_EXECING	0x0020		/* AS is in midst of execing */
#define PAS_ULI		0xffff0000	/* AS has ULI(s) */
#define PAS_ULISHFT	16
#define PAS_INC		0x10000
#define PAS_DEC		0xffff0000

/*
 * The following flag is used to disable VM resource accounting for the
 * current address space. This is needed if a process creates a new  process
 * aggregate and joins it. We should skip the VM resource accouting for that
 * process even though it is part of the process aggregate.
 */
#define PAS_NOMEM_ACCT	0x0040		/* Disable miser smem/rmem acct. */


#define pas_flagset(p, x)	atomicSetUint(&(p)->pas_flags, x)
#define pas_flagclr(p, x)	atomicClearUint(&(p)->pas_flags, x)
#define pas_inculi(p)		atomicAddUint(&(p)->pas_flags, PAS_INC)
#define pas_deculi(p)		atomicAddUint(&(p)->pas_flags, PAS_DEC)

/*
 * private physical address space object. If a process/thread has some
 * private regions, they are represented here. Also here is information
 * that is kept per process/thread.
 *
 * Locking:
 *	ppas_next/prev - aspacelock for UPDATE
 *	ppas_stk* - aspacelock for UPDATE XXX-should do better
 *	ppas_tsave - none - totally private
 *	ppas_size - aspacelock for ACCESS/UPDATE
 *	ppas_rss
 *	ppas_refcnt - atomic inc/dec
 *	ppas_flags - atomic or/and
 *	ppas_utas - a pointer to part of the uthread. This may get set
 *		to NULL when a thread/share member exits even though
 *		the ppas might continue to have a reference. Users must
 *		access ppas_utas ONLY with aspacelock OR while holding
 *		ppas_utaslock, and must always
 *		check for NULL unless it is guarenteed to be the current
 *		thread.
 *
 * XXX ppas_rss is currently the equivalent of the old
 * p_rss - it represents the entire 'process' including shared
 * areas. This needs to change.
 */
typedef struct ppas_s {	/* Private Physcal Address Space Object */
	int ppas_refcnt;
	unsigned int ppas_flags;	
	struct ppas_s *ppas_next;
	struct ppas_s **ppas_prevp;	/* ptr to previous next ptr */
	preg_set_t ppas_pregions;	/* list of private pregions */
	uvaddr_t ppas_stkbase;		/* base addr of stack (SPROC) */
	size_t ppas_stksize;		/* stack size in bytes (SPROC) */
	rlim_t ppas_stkmax;		/* max stack size in bytes (SPROC) */
	struct pregion *ppas_tsave;	/* saved text regions during exec */
	pgcnt_t ppas_size;		/* sz in pages of private regions */
	pgcnt_t ppas_physsize;		/* sz in pages of private regions that
					 * map PHYS devices
					 */
	pgcnt_t ppas_hides;		/* # pages local mappings hide of shared
					 * mapping
					 */
	ulong_t ppas_size_gen;		/* is hides up to date? */
	pgcnt_t ppas_rss;		/* rss in pages of private regions */
	struct pmap *ppas_pmap;		/* page map pointer */
	lock_t ppas_utaslock;		/* protect ppas_utas */
	struct utas_s *ppas_utas;	/* fields in uthread we manage */
} ppas_t;

/* ppas flags */
#define PPAS_SADDR	0x0001		/* sharing address space with sprocs */
#define PPAS_LOCKPRDA	0x0002		/* PAS has locked prda */
#define PPAS_ISO	0x0004		/* this thread running on isolated p */
#define PPAS_STACK	0x0008		/* stack mgmt from ppas, not pas */
#define ppas_flagset(p, x)	atomicSetUint(&(p)->ppas_flags, x)
#define ppas_flagclr(p, x)	atomicClearUint(&(p)->ppas_flags, x)

#if CELL
#include <ksys/cell/service.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/tkm.h>

/*
 * Distributed address space management
 */

/*
 * address space handle - we need one additional piece of information
 * the 'private address space id' that doesn't fit in the standard
 * obj_handle_t, so we wrap our own.
 */
typedef struct {
	obj_handle_t ash_handle;
	int ash_pasid;
} as_handle_t;

/*
 * Client side
 * The dcas - distributed client address space object
 *	vas_bhvh points to dcas_t when the address space is distributed.
 */
/* tokens */
#define DAS_EXIST_CLASS	0
#define DAS_EXIST	TK_MAKE(DAS_EXIST_CLASS, TK_READ)

/*
 * DAS_COUNTERS_CLASS - permit simultaneous updating of:
 *	pas_size, pas_rss, pas_nlockpg
 * The algorithm is that each node keeps track of its changes,
 * and when the UPDATE token is revoked, sends back the data
 * AND zeroes its counters.
 * rss uses a ABG algorithm to change the read-global semantic into
 *	a read-local semantic.
 * nlockpg also uses the ABG algorithm
 */
#define DAS_COUNTERS_CLASS	1
#define DAS_COUNTERS_READ	TK_MAKE(DAS_COUNTERS_CLASS, TK_READ)
#define DAS_COUNTERS_UPDATE	TK_MAKE(DAS_COUNTERS_CLASS, TK_SWRITE)

/*
 * DAS_ASPACELOCK_CLASS - distributed aspace lock
 *	Protects brkbase, brksize 
 */
#define DAS_ASPACE_CLASS	2
#define DAS_ASPACE_ACCESS	TK_MAKE(DAS_ASPACE_CLASS, TK_READ)
#define DAS_ASPACE_UPDATE	TK_MAKE(DAS_ASPACE_CLASS, TK_WRITE)

/*
 * DAS_PREGION_CLASS - lock pregions
 *	This is used when one can't really afford to use the aspacelock
 *	To insert/delete pregions, must hold this AND aspacelock for UPDATE
 */
#define DAS_PREGION_CLASS	3
#define DAS_PREGION_LOCK	TK_MAKE(DAS_PREGION_CLASS, TK_WRITE)

/*
 * DAS_MISC_CLASS
 *	Protects - pas_refcnt, pas_maxrss
 */
#define DAS_MISC_CLASS		4
#define DAS_MISC_LOCK		TK_MAKE(DAS_MISC_CLASS, TK_WRITE)

#define DAS_NTOKENS	5

typedef struct dcas_s {		/* Distributed Client Address Space */
	bhv_desc_t dcas_bhv;		/* behavior */
	struct dcpas_s *dcas_plist;	/* pointer to private clients */
	as_handle_t dcas_handle;	/* handle to object on server */
	TKC_DECL(dcas_ctokens, DAS_NTOKENS);
} dcas_t;

/* dcpas - distributed client private address space */
/* tokens */
#define DPAS_EXIST_CLASS	0
#define DPAS_EXIST	TK_MAKE(DPAS_EXIST_CLASS, TK_READ)

#define DPAS_NTOKENS 1

typedef struct dcpas_s {	/* Distributed Client Private Address Space */
	struct dcpas_s *dcpas_next;
	struct dcpas_s **dcpas_prevp;	/* ptr to previous next ptr */
	TKC_DECL(dcpas_ctokens, DPAS_NTOKENS);
	ppas_t dcpas_ppas;		/* private data */
} dcpas_t;

/*
 * Server side
 *
 * The handle (dcas_handle e.g.) h_object points to a dsas_t. The dsas_t
 *	contains the token server and a pointer to the 'real' data located in
 *	aspo_t.
 */

/*
 * The distributed server side - the dcas_handle.h_object points to this.
 * dcas_handle.h_objspec points to the appropriate dspas_t.
 * Sizes & limits:
 * nlockpg - server tracks maximum and current and implements BG algorithm
 */
typedef struct dsas_s {		/* Distributed Server Address Space */
	bhv_desc_t dsas_bhv;		/* behavior */
	struct dspas_s *dsas_plist;	/* private object list */
	TKS_DECL(dsas_stokens, DAS_NTOKENS);
	TKC_DECL(dsas_ctokens, DAS_NTOKENS);
	pgcnt_t dsas_bgnlockpg;		/* remaining to give out */
	pgcnt_t dsas_bgrss;		/* remainint rss to give out */
	pas_t dsas_pas;			/* physical object - shared */
} dsas_t;

typedef struct dspas_s {	/* Distributed Server Private Address Space */
	struct dspas_s *dspas_next;
	struct dspas_s **dspas_prevp;	/* ptr to previous next ptr */
	TKS_DECL(dspas_stokens, DPAS_NTOKENS);
	TKC_DECL(dspas_ctokens, DPAS_NTOKENS);
	ppas_t dspas_ppas;
} dspas_t;
#endif /* CELL */

/*
 * flag values for asvo_detachreg...
 */
#define RF_FORCE        0x0002          /* force a private region */
#define RF_NOFLUSH      0x0004          /* do not flush tlbs (detachreg) */
#define RF_TSAVE        0x0008          /* preg is tsave'd (detachreg) */

/*
 * Registration for the positions at which the different vas behaviors 
 * are chained.  When on the same chain, a behavior with a higher position 
 * number is invoked before one with a lower position number.
 */
#define VAS_POSITION_PAS	BHV_POSITION_BASE   	/* chain bottom */
#define VAS_POSITION_DC		BHV_POSITION_TOP	/* distr. client */
#define VAS_POSITION_DS		BHV_POSITION_TOP	/* distr. server */

#define VAS_TO_FIRST_BHV(v)	(BHV_HEAD_FIRST(&(v)->vas_bhvh))
#define BHV_TO_VAS(bdp)		((vas_t *)BHV_VOBJ(bdp))

#define BHV_TO_PAS(bdp) \
	(ASSERT(BHV_OPS(bdp) == &pas_ops), \
	(pas_t *)(BHV_PDATA(bdp)))

#define VASID_TO_VAS(vasid)	((vas_t *)((vasid).vas_obj))
#define ASID_TO_VAS(asid)	((vas_t *)((asid).as_obj))
#define ASID_TO_GEN(asid)	(asid.as_gen)

/*
 * Macro to go from pas to process aggregate. This returns NULL if no pagg 
 * accounting needs to be done for this address space.
 */
#define	PAS_TO_VPAGG(pas)	\
	((pas->pas_flags & PAS_NOMEM_ACCT) ? NULL : (pas)->pas_vpagg)

/*
 * XXX convert asid to pas - until we get all virtualized
 */
#define VASID_TO_PAS(vas)	BHV_TO_PAS(VAS_TO_FIRST_BHV((vas).vas_obj))

/*
 * implementation functions
 */
struct eframe_s;
struct prda;
union pde;
struct prilist_s;
struct utas_s;
struct pfdat;

extern void pas_free(pas_t *);
extern void ppas_free(pas_t *, ppas_t *);
extern pas_t *pas_alloc(vas_t *, struct utas_s *, int);
extern void ppas_alloc(pas_t *, ppas_t *, vasid_t *, asid_t *, struct utas_s *);
extern void pas_init(void);
extern int pas_getattrscan(pas_t *, ppas_t *, as_getattrop_t, as_getattrin_t *, as_getattr_t *);
extern void pas_ulimit(pas_t *, ppas_t *, as_getasattr_t *);
extern int pas_shakerss(pas_t *);
extern int pas_memlock(pas_t *, ppas_t *, uvaddr_t, size_t, as_setrangeattr_t *);
extern int as_vhandscan(struct prilist_s *, int);
extern void as_rele_common(vasid_t);

extern int pas_watch(bhv_desc_t *, aspasid_t, as_watchop_t, as_watch_t *);
extern void ascancelskipwatch(pas_t *pas, ppas_t *ppas);
extern void deleteallwatch(pas_t *, ppas_t *, int);
extern int skipwatch(pas_t *, ppas_t *, struct uthread_s *, uvaddr_t,
				int, uvaddr_t,
				ulong, struct pageattr *, struct region *);
extern int chkwatch(uvaddr_t, struct uthread_s *, int, int, ulong, int *);
extern void clnattachaddrptes(pas_t *, preg_t *, uvaddr_t, union pde *);
extern void newptes(struct pas_s *, struct ppas_s *, uint);
extern void newpmap(struct pas_s *, struct ppas_s *);
extern void tlbclean(struct pas_s *, pgno_t, cpumask_t);
extern void tlbcleansa(struct pas_s *, struct ppas_s *, cpumask_t);
extern int fault_in(pas_t *, ppas_t *, uvaddr_t, size_t, int);
extern void pas_unlockprda(pas_t *, ppas_t *, struct prda *);
extern int pas_addprda(pas_t *, ppas_t *, as_addspace_t *, uvaddr_t *);
extern int pas_addexec(pas_t *, ppas_t *, as_addspace_t *, uvaddr_t *);
extern int pas_addmmap(pas_t *, ppas_t *, as_addspace_t *, uvaddr_t *);
extern int pas_addmmapdevice(pas_t *, ppas_t *, as_addspace_t *, uvaddr_t *);
extern int pas_addload(pas_t *, ppas_t *, as_addspace_t *, uvaddr_t *);
extern int pas_addstackexec(pas_t *, ppas_t *, as_addspace_t *, uvaddr_t *);
extern int pas_addattach(pas_t *, ppas_t *, as_addspace_t *, as_addspaceres_t *);
extern int pas_addshm(pas_t *, ppas_t *, as_addspace_t *, uvaddr_t *);
extern int pas_addinit(pas_t *, ppas_t *, as_addspace_t *, uvaddr_t *);
extern int pas_unmap(pas_t *, ppas_t *, uvaddr_t, size_t, int);
extern uvaddr_t pas_allocaddr(pas_t *, ppas_t *, pgcnt_t, uchar_t);
extern int pas_break(pas_t *, ppas_t *, uvaddr_t);
extern int pas_stackgrow(pas_t *, ppas_t *, uvaddr_t);
extern int chkpgrowth(pas_t *, ppas_t *, pgcnt_t);
extern int setup_prda(pas_t *, ppas_t *);
extern int replace_pmap(pas_t *, ppas_t *, uchar_t);
extern int pas_ageregion(pas_t *);
extern int pas_spinas(pas_t *, as_shake_t *);
extern int pas_delexit(pas_t *, ppas_t *, as_deletespace_t *);
extern int pas_delexec(pas_t *, ppas_t *, as_deletespace_t *);
extern int pas_delshm(pas_t *, ppas_t *, uvaddr_t, as_deletespaceres_t *);
extern int pas_msync(pas_t *, ppas_t *, uvaddr_t, size_t, uint_t);
extern int pas_mprotect(pas_t *, ppas_t *, uvaddr_t, size_t, mprot_t, int);
extern int pas_madvise(pas_t *, ppas_t *, uvaddr_t, size_t, uint_t);
/* Fault logging routine prototype */
extern void prfaultlog(pas_t *, ppas_t *, preg_t *, uvaddr_t, int);
extern int pas_getpginfo(pas_t *, ppas_t *, uvaddr_t, size_t,
				as_getrangeattrin_t *, as_getrangeattr_t *);
extern int pas_getprattr(pas_t *, ppas_t *, as_getattrin_t *, as_getattr_t *);
extern int pas_getnshm(pas_t *, ppas_t *);
extern int pas_getkvmap(pas_t *, ppas_t *, as_getattr_t *);
extern int pas_fork(pas_t *, ppas_t *, as_newres_t *, uvaddr_t, 
				size_t, struct utas_s *, as_newop_t);
extern int pas_sproc(pas_t *, ppas_t *, as_newres_t *, uvaddr_t, size_t,
					struct utas_s *);
extern int pas_uthread(pas_t *, ppas_t *, as_newres_t *, struct utas_s *);
extern void initsegtbl(struct utas_s *, pas_t *, ppas_t *);
extern int pas_vfault(pas_t *, ppas_t *, as_fault_t *, as_faultres_t *);
extern int pas_pfault(pas_t *, ppas_t *, as_fault_t *, as_faultres_t *);
extern int pas_pattach(pas_t *pas, ppas_t *ppas, uvaddr_t vaddr, int,
		       caddr_t *);
extern int pas_pflip(pas_t *, ppas_t *, caddr_t, uvaddr_t, int, caddr_t *);
extern int pas_psync(pas_t *, ppas_t *);
extern int pas_vtopv(pas_t *, ppas_t *, as_getrangeattrop_t,
		uvaddr_t, size_t, as_getrangeattrin_t *, as_getrangeattr_t *);
extern int pas_reservemem(pas_t *, pgno_t, pgno_t);
extern void pas_unreservemem(pas_t *, pgno_t, pgno_t);
extern int pas_pagelock(pas_t *, struct pfdat *, int, int);
extern int pas_pageunlock(pas_t *, struct pfdat *);

#if NEVER
/* 
 * build lists of ppas's for a pas - this grabs refs on each ppas.
 */
/* values for 'crit' - pas_buildlist */
#define PAS_C_ISO	0x0001		/* check for isolated ppas's */
#define PAS_C_RSS	0x0002		/* check for RSS excess */
#define PAS_C_ULI	0x0004		/* check for ULI */

#define PAS_BCHUNK 40
typedef struct pas_blist_s {
	ppas_t *blist_ppas[PAS_BCHUNK];
	struct pas_blist_s *blist_next;
	int  blist_n;			/* number of valid entries in chunk */
} pas_blist_t;

extern int pas_buildlist(pas_t *, pas_blist_t *, int);
extern void pas_freelist(pas_t *, pas_blist_t *);
#endif
#ifdef R10000_SPECULATION_WAR
extern void duplockedpreg(pas_t *pas, ppas_t *ppas, preg_t *prp, preg_t *src);
extern void fixlockedpreg(pas_t *pas, ppas_t *ppas, preg_t *prp, int val);
extern void rmlockedpreg(pas_t *pas, ppas_t *ppas, preg_t *prp, reg_t *regp,
			 int lock);
extern int mpin_sleep_if_locked(pas_t *, ppas_t *, preg_t *, caddr_t);
extern int pinpagescanner(vasid_t, void *);
#endif

/*
 * implementation globals, etc ..
 */
extern asvo_ops_t pas_ops;		/* physical ops */

#ifdef __cplusplus
}
#endif
#endif
