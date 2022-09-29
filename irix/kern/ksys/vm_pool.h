/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.6 $"

#ifndef _KSYS_VM_POOL_H_
#define _KSYS_VM_POOL_H_

#if defined(NUMA_BASE) && defined(DEBUG)
#define VMPOOL_STATS 1
#endif

/*
 * Vm Pools:
 * Vm pools are pools of availsmem/availrmem from which to reserve swap space
 * for user and kernel memory allocations. Currently there are only two types
 * pools. One is a global pool. At boot time all memory/swap space in the 
 * system go to the global pool. Global pool is used while reserving memory
 * for kernel and regular timesharing/realtime processes. A new pool 
 * is created for the miser batch scheduler (miser pool). The miser pool is
 * used while reserving memory for batch critical jobs.
 * The pool to which an address space belongs is kept in the Vpagg object
 * The vpagg object is accessed from the address space. All miser jobs
 * have a vpagg object to which they belong. If an address space has no
 * associated vpagg object it reserves memory from the global pool.
 * A batch job can be using memory from the global pool as long it is not
 * critical. Once a batch job becomes critical it starts using the miser pool.
 * At this time we transfer the reservation from the global pool to the miser
 * pool using the VPAG_TRANSFER_VM_RESOURCE_POOL() operation on the vpagg 
 * object.
 */

struct sv;
struct vpagg_s;

typedef struct vm_pool_s {
	lock_t	pl_lck;		/* Spin lock to control allocation */
	int	pl_flags;	/* Pool flags */
	pgno_t	pl_availsmem;	/* Available swappable memory in pages */
	pgno_t  pl_availrmem;	/* Available resident (not swappable)  */
				/* memory in pages. */
	pgno_t  pl_bufmem;      /* Memory used by min_file_pages & min_bufmem */
	pgno_t	pl_usermem;	/* Memory that can be allocated to user pgms  */
	pgno_t	pl_smem_wanted; /* Min. Availsmem wanted by a miser job */
	pgno_t	pl_rmem_wanted; /* Min. Availrmem wanted by a miser job */
	pgno_t	pl_guaranteed_mem; /* Amount of guaranteed memory. */
				/* Used to resize miser pools */
	struct sv pl_wait;	/* Synch. variable to wait for avail[sr]mem */
} vm_pool_t;


#if NUMA_BASE
/*
 * The GLOBAL_POOL is a source of cache contention on large cpu count systems.
 * To reduce the contention, a per-node cache is maintained. This cache contains
 * a subset of the information from the GLOBAL_POOL. As long as sufficient
 * resources remain in the local cache, no references need to be made to any
 * shared writeable structures.
 */

typedef struct vm_cached_global_pool_s {
	lock_t	cgpl_lck;	/* Spin lock to control allocation */
	int	cgpl_flags;	/* Pool flags */
	pgno_t	cgpl_availsmem;	/* Available swappable memory in pages */
	pgno_t  cgpl_availrmem;	/* Available resident (not swappable)  */
				/* memory in pages. */
	pgno_t	cgpl_usermem;	/* Memory that can be allocated to user pgms  */
#ifdef VMPOOL_STATS
	uint	cqpl_cgreserve;
	uint	cqpl_cgreserve_fail;
	uint	cqpl_reservemem;
	uint	cqpl_reservefail;
	uint	cqpl_kreservemem;
	uint	cqpl_enable_caching;
	uint	cqpl_enable_caching_failed;
#endif /* VMPOOL_STATS */
} vm_cached_global_pool_t;

#define CGPL_DISABLE_TIME	HZ*4	/* When a reservemem request has to disable caching
					 * & move all cached memory to the global pool, 
					 * caching remains disabled for a short time.
					 */
#endif /* NUMA_BASE */


#define	GLOBAL_POOL	(&global_pool)

#if NUMA_BASE
#define	GET_AVAILSMEM(pool)	get_availsmem(pool)
#define	GET_AVAILRMEM(pool)	get_availrmem(pool)
#define	GET_USERMEM(pool)	get_usermem(pool)

#define	GLOBAL_USERMEM_VAR	(global_pool.pl_usermem) /* ZZZ fixme */

extern	pgno_t get_availsmem(vm_pool_t *);
extern	pgno_t get_availrmem(vm_pool_t *);
extern	pgno_t get_usermem(vm_pool_t *);

#else

#define GET_AVAILSMEM(pool)	((pool)->pl_availsmem)
#define GET_AVAILRMEM(pool)	((pool)->pl_availrmem)
#define GET_USERMEM(pool)	((pool)->pl_usermem)

#define	GLOBAL_USERMEM_VAR	(global_pool.pl_usermem)

#endif /* NUMA_BASE */

#define	GLOBAL_AVAILSMEM()	GET_AVAILSMEM(GLOBAL_POOL)
#define	GLOBAL_AVAILRMEM()	GET_AVAILRMEM(GLOBAL_POOL)
#define	GLOBAL_USERMEM()	GET_USERMEM(GLOBAL_POOL)


#define	VM_POOL_LOCK(P)		mutex_spinlock(&((P)->pl_lck))
#define	VM_POOL_UNLOCK(P, T)	mutex_spinunlock(&((P)->pl_lck), T)
#define VM_POOL_IS_LOCKED(P)	spinlock_islocked(&((P)->pl_lck))

#if NUMA_BASE
#define	VM_CACHED_GLOBAL_POOL_LOCK(P)		mutex_spinlock(&((P)->cgpl_lck))
#define	VM_CACHED_GLOBAL_POOL_UNLOCK(P,T)	mutex_spinunlock(&((P)->cgpl_lck), T)
#define	VM_CACHED_GLOBAL_POOL_NESTED_LOCK(P)	nested_spinlock(&((P)->cgpl_lck))
#define	VM_CACHED_GLOBAL_POOL_NESTED_UNLOCK(P)	nested_spinunlock(&((P)->cgpl_lck))
#endif /* NUMA_BASE */

/*
 * Definition for pl_flags.
 */
#define	VM_POOL_WAIT		0x1	/* Indicates processes waiting on VM pools */
#define	VM_POOL_BCACHE		0x2	/* This pool must be buffer-cache aware */
					/* when promising memory */
#define VM_POOL_CACHING_ENABLED	0x4	/* pool is being cached locally (GLOBAL_POOL only)*/
#define VM_POOL_CACHING_ALLOWED	0x8	/* pool is allowed to be cached locally (GLOBAL_POOL only)*/



extern	vm_pool_t	global_pool;

extern	int reservemem(vm_pool_t *, pgno_t, pgno_t, pgno_t);
extern	void unreservemem(vm_pool_t *, pgno_t, pgno_t, pgno_t);
extern	void kreservemem(vm_pool_t *, pgno_t, pgno_t, pgno_t);
extern	void init_global_pool(pgno_t, pgno_t, pgno_t);

extern	vm_pool_t *vm_pool_create(pgno_t);
extern	void vm_pool_free(vm_pool_t *);
extern  int vm_pool_wait(vm_pool_t *);
extern pgno_t vm_pool_exec_reservemem(pgno_t);
extern void vm_pool_set_wanted(vm_pool_t *, pgno_t, pgno_t);
extern void vm_pool_wakeup(vm_pool_t *, int);
extern int vm_pool_retry_syscall(int);
extern void vm_pool_update_guaranteed_mem(vm_pool_t *, pgno_t);
extern int vm_pool_resize(vm_pool_t *, pgno_t);

#endif /* _KSYS_VM_POOL_H_ */
