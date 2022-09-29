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
#ident  "$Revision: 1.15 $"

#include <sys/types.h>
#include <ksys/vpag.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/page.h>
#include <sys/sema.h>
#include <ksys/vm_pool.h>
#include <sys/cmn_err.h>
#include "as_private.h"
#include <ksys/vproc.h>
#include <values.h>
#include "sys/kmem.h"
#include "sys/cred.h"
#include "ksys/vproc.h"
#include "sys.s"
#include <sys/tile.h>
#include <sys/pfdat.h>
#ifdef NUMA_BASE
#include <sys/nodepda.h>
#endif
#if VMPOOL_STATS
#include <sys/idbgentry.h>
#endif



#ifdef	DEBUG
static pgno_t maxavailrmem;
#endif
static int	_reservemem(vm_pool_t *, pgno_t , pgno_t , pgno_t );

vm_pool_t	global_pool;			/* Global pool type 	*/

#if NUMA_BASE
/*
 * The following definitions are used for the cached global pool and are needed
 * only for NUMA_BASE systems.
 */
static int	cached_global_reservemem(pgno_t , pgno_t , pgno_t, int * );
static void	reclaim_cached_global_memory(void);
static void	try_enable_cached_global(void);

#ifdef VMPOOL_STATS
static void	idbg_cgpool(int);
#endif

time_t		next_enable_cached_global_time=0; /* lbolt time when caching can be enabled. */

/*
 * Statistics for testing 
 */
struct {
	uint	cg_enabled;
	uint	cg_update_availmem;
	uint	cg_update_availmem_required;
} cg_stats;


/*
 * The following macros return a pointer to a per-node cached global pool structure.
 * Note that MYCGPOOL returns a pointer to the pool for the current node. If the
 * thread is not running at splhi, it may migrate to another node and the pointer
 * returned by MYCGPOOL will now point to a different node's pool. As long
 * as the SAME pointer is used, it's ok, but dont use the MYCGPOOL macro again
 * & expect to get the same pool.
 */
#define MYCGPOOL		((vm_cached_global_pool_t*)nodepda->cached_global_pool)
#define CGPOOL(node)		((vm_cached_global_pool_t*)NODEPDA(node)->cached_global_pool)

#endif /* NUMA_BASE */

#ifdef VMPOOL_STATS
#define POOLSTAT(id)		cg_stats.id++;
#define CGPOOLSTAT(p,id)	if(p) (p)->id++
#else
#define POOLSTAT(id)
#define CGPOOLSTAT(p,id)
#endif /* VMPOOL_STATS */


#ifdef	DEBUG
int vm_pool_debug = 1;
static __inline void
vm_pool_printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (vm_pool_debug)
                 icmn_err(CE_CONT, fmt, ap);
        va_end(ap);
}
#else
/*ARGSUSED*/
static __inline void
vm_pool_printf(char *fmt, ...)
{
}
#endif /* DEBUG */


#define	MAX_SWAPMEM	MAXLONG


/*
 * init_availmem:
 * Initialize global pool of availsmem/availrmem.
 */
void
init_global_pool(pgno_t smem, pgno_t rmem, pgno_t umem)
{
	bzero(&global_pool, sizeof(global_pool));

	global_pool.pl_flags = VM_POOL_BCACHE;

	spinlock_init(&(GLOBAL_POOL->pl_lck), "vm_pool_lck");
	sv_init(&((GLOBAL_POOL)->pl_wait), SV_DEFAULT, "Pool wait");

#ifdef	DEBUG
	maxavailrmem = GLOBAL_FREEMEM();
#endif

	/*
	 * Note pl_bufmem gets initialized through binit and cinit
	 * by calling breservemem to include min_file_pages and min_bufmem
	 */
	unreservemem(GLOBAL_POOL, smem, rmem, umem);

#ifdef VMPOOL_STATS
	idbg_addfunc("cgpool", idbg_cgpool);
#endif

}

#if NUMA_BASE
/*
 * init_cached_global_pool:
 * Called during boot to initialize the per-node caching structures 
 * for the GLOBAL POOL.
 * Note that this routine cant be called until the kmem allocators are
 * initialized. Also note that no references can be made to the cache pools
 * until this procedure completes. If we move the cache pool structures
 * to the NODEPDA, this code would be cleaner. 
 */
void
init_cached_global_pool(void)
{
	cnodeid_t		node;

	for (node=0; node < numnodes; node++) {
		NODEPDA(node)->cached_global_pool = kmem_zalloc_node_hint(
				sizeof(vm_cached_global_pool_t), 0, node);
	}

	GLOBAL_POOL->pl_flags |= VM_POOL_CACHING_ALLOWED;
}
#endif /* NUMA_BASE */


/*
 * reservemem:
 * Reserve resident memory. This is called to reserve memory or swap space
 * when a process or kernel needs memory. smem is memory that can be
 * swapped. rmem  represents number of pages of memory that cannot be swapped. 
 * This is required by kernel and when pages are pinned.
 */
int
reservemem(vm_pool_t *pool, pgno_t smem, pgno_t rmem, pgno_t umem)
{
	int	locktoken;
	int	error;
#if NUMA_BASE
	/*
	 * Check if the required space is available from the cached global
	 * pool. If so, we are done.
	 */
	int	cg_disabled=1;


	if ((pool->pl_flags&VM_POOL_CACHING_ENABLED) &&
			cached_global_reservemem(smem, rmem, umem, &cg_disabled) == 0)
		return 0;
#endif /* NUMA_BASE */

	locktoken = VM_POOL_LOCK(pool);

	error = _reservemem(pool, smem, rmem, umem);
#if NUMA_BASE
	/*
	 * If this is the GLOBAL_POOL:
	 * 	- if the global reservemem failed, reclaim cached memory
	 * 	  from other nodes and try again. 
	 *	- if the global reserve suceeded & local caching is currently
	 *	  disabled, try to enable it.
	 */	  
	if (pool == GLOBAL_POOL) {
		if (error) {
			if (pool->pl_flags&VM_POOL_CACHING_ENABLED) {
				reclaim_cached_global_memory();
				error = _reservemem(pool, smem, rmem, umem);
			}
		} else if ((pool->pl_flags&VM_POOL_CACHING_ALLOWED) && cg_disabled) 
			try_enable_cached_global();
			
	} 

	if (pool == GLOBAL_POOL) {	
		CGPOOLSTAT(MYCGPOOL, cqpl_reservemem);
		if (error)
			CGPOOLSTAT(MYCGPOOL, cqpl_reservefail);
	}
#endif /* NUMA_BASE */

	VM_POOL_UNLOCK(pool, locktoken);

	return error;
}

#if NUMA_BASE
/*
 * try_enable_cached_global:
 * Called to try to enable per-node caching of availmem. 
 * This routine is called when a reservemem fails in the local pool but
 * succeeds from the global pool. Several possibilities exist:
 *	- caching is globally disabled. If it is ok to enable global caching,
 *	  enable it globally & for this node. Other nodes will enable it
 *	  for themselves as they try to do their own reservemem.
 *	- caching is already enabled globally but has not been enabled
 *	  for this node. Just enable it for this node.
 *
 * Also note that if any thread is waiting for availmem, globally caching 
 * must remain disabled.
 */
static void
try_enable_cached_global(void)
{
	vm_cached_global_pool_t *cgpool;
	int			enabled;

	ASSERT(VM_POOL_IS_LOCKED(GLOBAL_POOL));

	enabled = (GLOBAL_POOL->pl_flags&VM_POOL_CACHING_ENABLED);

	if (!enabled && next_enable_cached_global_time < lbolt &&
			(GLOBAL_POOL->pl_flags&VM_POOL_CACHING_ALLOWED) &&
			!(GLOBAL_POOL->pl_flags&VM_POOL_WAIT)) {
		GLOBAL_POOL->pl_flags |= VM_POOL_CACHING_ENABLED;
		enabled = 1;
		POOLSTAT(cg_enabled);
	} 

	if (enabled) {
		cgpool = MYCGPOOL;
		VM_CACHED_GLOBAL_POOL_NESTED_LOCK(cgpool);
		cgpool->cgpl_flags |= VM_POOL_CACHING_ENABLED;
		CGPOOLSTAT(cgpool, cqpl_enable_caching);
		VM_CACHED_GLOBAL_POOL_NESTED_UNLOCK(cgpool);
	} else
		CGPOOLSTAT(MYCGPOOL, cqpl_enable_caching_failed);
}


/*
 * cached_global_reservemem
 * Try to reserve availmem from the cached-global pool.
 * 	Returns 0 if success, ENOMEM otherwise.
 */
static int 
cached_global_reservemem(pgno_t smem, pgno_t rmem, pgno_t umem, int *disabled)
{
	vm_cached_global_pool_t	*cgpool = MYCGPOOL;
	int			locktoken;
	int			error;

	locktoken = VM_CACHED_GLOBAL_POOL_LOCK(cgpool);
	if ((smem && smem > cgpool->cgpl_availsmem) ||
	    (rmem && rmem > cgpool->cgpl_availrmem)) {
		*disabled = ((cgpool->cgpl_flags&VM_POOL_CACHING_ENABLED) == 0);
		CGPOOLSTAT(cgpool, cqpl_cgreserve_fail);
		error = ENOMEM;
	} else {
		CGPOOLSTAT(cgpool, cqpl_cgreserve);
		cgpool->cgpl_availsmem -= smem;
		cgpool->cgpl_availrmem -= rmem;
		cgpool->cgpl_usermem -= umem;
		error = 0;
	}
	VM_CACHED_GLOBAL_POOL_UNLOCK(cgpool, locktoken);
	return error;
}


/*
 * reclaim_cached_global_memory
 * Disable cached-global & reclaim all the space from the per-node
 * pools. Cached global will remain disabled for a few seconds
 * before it can be enabled again.
 */
static void
reclaim_cached_global_memory(void)
{
	vm_cached_global_pool_t	*cgpool;
	vm_pool_t		*pool = GLOBAL_POOL;
	cnodeid_t		node;

	ASSERT(VM_POOL_IS_LOCKED(GLOBAL_POOL));
	pool->pl_flags &= ~VM_POOL_CACHING_ENABLED;
	for (node=0; node < numnodes; node++) {
		cgpool = CGPOOL(node);
		VM_CACHED_GLOBAL_POOL_NESTED_LOCK(cgpool);
		pool->pl_availsmem += cgpool->cgpl_availsmem;
		pool->pl_availrmem += cgpool->cgpl_availrmem;
		pool->pl_usermem += cgpool->cgpl_usermem;
		cgpool->cgpl_availsmem = 0;
		cgpool->cgpl_availrmem = 0;
		cgpool->cgpl_usermem = 0;
		cgpool->cgpl_flags &= ~VM_POOL_CACHING_ENABLED;
		VM_CACHED_GLOBAL_POOL_NESTED_UNLOCK(cgpool);
	}
	next_enable_cached_global_time = lbolt + CGPL_DISABLE_TIME;
}
#endif /* NUMA_BASE */

/*
 * _reservemem assumes VM_POOL lock is held. Does most of the real work of
 * reservemem.
 */
static int
_reservemem(vm_pool_t *pool, pgno_t smem, pgno_t rmem, pgno_t umem)
{
	/*
	 * Leave room for file cache to grow to min_file_pages.
	 * The file cache will trim itself if it finds that
	 * avail{rs}mem has gone below 'chunkpages' and the
	 * file cache has grown past min_file_pages.
	 */
	if ((smem && smem > pool->pl_availsmem - pool->pl_bufmem) ||
	    (rmem && rmem > pool->pl_availrmem - pool->pl_bufmem))
			return ENOMEM;

	pool->pl_availsmem -= smem;
	pool->pl_availrmem -= rmem;
	pool->pl_usermem -= umem;
	return(0);
}

/*
 * kreservemem:
 * Called by the kernel while reserving memory. This allocates memory without
 * checking. Availsmem is less than actual available memory by tune.t_minasmem
 * This routine returns availsmem even when pool->pl_availsmem is 0. The caller
 * will ensure that there is no deadlock.
 */
void
kreservemem(vm_pool_t *pool, pgno_t smem, pgno_t rmem, pgno_t umem)
{
	int locktoken = VM_POOL_LOCK(pool);

	pool->pl_availsmem -= smem;
	pool->pl_availrmem -= rmem;
	pool->pl_usermem -= umem;
	VM_POOL_UNLOCK(pool, locktoken);
	CGPOOLSTAT(MYCGPOOL, cqpl_kreservemem);
}

/*
 * unreservemem:
 * Give back reserved memory back to pool.
 */
void
unreservemem(vm_pool_t *pool, register pgno_t smem, register pgno_t rmem, 
							register pgno_t umem)
{
	register int locktoken;

	ASSERT((GLOBAL_POOL->pl_flags&(VM_POOL_CACHING_ENABLED|VM_POOL_WAIT)) !=
		(VM_POOL_CACHING_ENABLED|VM_POOL_WAIT));
		
#ifdef NUMA_BASE
	if (pool == GLOBAL_POOL && (pool->pl_flags&VM_POOL_CACHING_ALLOWED)) {
		vm_cached_global_pool_t	*cgpool = MYCGPOOL;
		if ((cgpool->cgpl_flags&VM_POOL_CACHING_ENABLED)) {
			locktoken = VM_CACHED_GLOBAL_POOL_LOCK(cgpool);
			if ((cgpool->cgpl_flags&VM_POOL_CACHING_ENABLED)) {
				cgpool->cgpl_availsmem += smem;
				cgpool->cgpl_availrmem += rmem;
				cgpool->cgpl_usermem += umem;
				VM_CACHED_GLOBAL_POOL_UNLOCK(cgpool, locktoken);
				return;
			} else 
				VM_CACHED_GLOBAL_POOL_UNLOCK(cgpool, locktoken);
		}
	}
#endif /* NUMA_BASE */

	locktoken = VM_POOL_LOCK(pool);
	pool->pl_availsmem += smem;
	pool->pl_availrmem += rmem;
	ASSERT(pool->pl_availrmem <= maxavailrmem);
	pool->pl_usermem += umem;
	vm_pool_wakeup(pool, 0);
	VM_POOL_UNLOCK(pool, locktoken);
}

/*
 * pas_reservemem:
 * Called by processes to reserve memory. This interface tries to reserve
 * memory from the appropriate pool to which the process belongs. If the
 * process is a batch process and it exceeds its memory limit
 * the call fails.
 */ 
int
pas_reservemem(pas_t *pas, pgno_t smem, pgno_t rmem)
{
	vpagg_t		*vpag;
	int		error;
	pgno_t		pre_reserved_mem;

	vpag = PAS_TO_VPAGG(pas);

	error = 0;

	if (vpag) {
		pre_reserved_mem = pas->pas_smem_reserved;
		error = VPAG_RESERVEMEM(vpag, smem, rmem, &pre_reserved_mem);
		pas->pas_smem_reserved = pre_reserved_mem;
	} else {
		if (reservemem(GLOBAL_POOL, smem, rmem, 0))
			error = EAGAIN;
	}

#ifdef	DEBUG
	if (error)
		cmn_err(CE_WARN,"Pas_reservemem returns error %d pas 0x%x smem %d rmem %d\n",error, pas, smem,rmem);
#endif


	return error;
}

/*
 * pas_unreservemem:
 * Give back previously reserved memory  back to the pool.
 */
void
pas_unreservemem(pas_t *pas, pgno_t smem, pgno_t rmem)
{
	vpagg_t         *vpag;

        vpag = PAS_TO_VPAGG(pas);
	
	if (vpag) 
		 VPAG_UNRESERVEMEM(vpag, smem, rmem); 
	else unreservemem(GLOBAL_POOL, smem, rmem, 0);

}

void
breservemem(pgno_t bmem)
{
	global_pool.pl_bufmem += bmem;
}

/*
 * Pre-reserve memory at exec-time for miser jobs. Always use GLOBAL POOL
 * as pre-reserving is necessary only for batch jobs and not batch-critical 
 * jobs.
 */
pgno_t
vm_pool_exec_reservemem(pgno_t smem_needed)
{
	vm_pool_t *vm_pool;
	pgno_t	smem_reserved;

	vm_pool = GLOBAL_POOL;

	if (reservemem(vm_pool, smem_needed, 0, 0) == 0) {
		smem_reserved = smem_needed;
	} else {
		vm_pool_set_wanted(vm_pool, smem_needed, 0);
		return -1;
	}

	return smem_reserved;
}

/*
 * Set the wanted fields in the vm_pool so that unreservemem will know
 * when to wake us up.
 */
void
vm_pool_set_wanted(vm_pool_t *vm_pool, pgno_t smem, pgno_t rmem)
{
	int s = VM_POOL_LOCK(vm_pool);

	ASSERT(vm_pool == GLOBAL_POOL);
	if (smem < vm_pool->pl_smem_wanted 
			|| !vm_pool->pl_smem_wanted)
		vm_pool->pl_smem_wanted =  smem;
	if (rmem < vm_pool->pl_rmem_wanted 
			|| !vm_pool->pl_rmem_wanted)
		vm_pool->pl_rmem_wanted =  rmem;
	VM_POOL_UNLOCK(vm_pool, s);
}

/*
 * nomemmsg:
 * Called to print out a warning message to the console when the system
 * runs out of availsmem and pl_availrmem.
 */
void
nomemmsg(char *msg)
{
	cmn_err(CE_ALERT,
		"%s [%d] - out of logical swap space during %s - see swap(1M)",
		get_current_name(), current_pid(), msg);
#ifdef DEBUG
	cmn_err(CE_CONT, "Availsmem %d availrmem %d rlx freemem %d, real freemem %d\n",
		GLOBAL_AVAILSMEM(), GLOBAL_AVAILRMEM(), 
			GLOBAL_FREEMEM(), GLOBAL_FREEMEM_GET());
#endif
}

/*
 * vm_pool_create:
 * Create a new pool of availsmem/availrmem. A process can be attached to this
 * pool and its resources will be allocated from this pool. This is used
 * to create a separate pool for critical batch jobs that need memory guarantees
 * The creation of the pool can fail if there is not enough free memory in the 
 * global pool to be transferred to the new pool.
 */

vm_pool_t *
vm_pool_create(pgno_t rmem)
{
	vm_pool_t	*vm_pool;

	/*
	 * First see if we can get enough memory from the global pool to 
	 * create a new pool.
	 */
	if (reservemem(GLOBAL_POOL, rmem, rmem, 0))
		return NULL;

	vm_pool_printf("Creating Miser pool rmem %d\n",rmem);

	vm_pool = kmem_zalloc(sizeof(vm_pool_t), KM_SLEEP);
	vm_pool->pl_availsmem = MAX_SWAPMEM;
	vm_pool->pl_availrmem = rmem;
	/* vm_pool->pl_usermem = 0; zalloc! */
	sv_init(&vm_pool->pl_wait, SV_DEFAULT, "Pool wait");

	return vm_pool;
}

/*
 * vm_pool_free:
 * Free up a VM pool. It returns the memory back to the global pool.
 * At this point is assumed that no process is attached to this pool.
 */

void
vm_pool_free(vm_pool_t *vm_pool)
{
	ASSERT(vm_pool != GLOBAL_POOL);
	unreservemem(GLOBAL_POOL, vm_pool->pl_availrmem, vm_pool->pl_availrmem, 0);
	kmem_free(vm_pool, sizeof(vm_pool_t));
}


/*
 * vm_pool_wait:
 * Batch jobs call this routine to wait until enough availsmem is available.
 * in a given pool. It is also called when a process is transferred from
 * global pool to miser pool. We need to make sure to go to sleep only
 * if process priority is batch. If it is batch critical we just return 1 so 
 * that the process will retry the operation. If it is neither then return 0
 * so that the caller will not retry the system call.
 * Returns 1 if the caller has to retry the system call else 0.
 */ 
int
vm_pool_wait(vm_pool_t *vm_pool) 
{
	int	locktoken;

	/*
	 * We wait only in the global pool.
	 */
	ASSERT(vm_pool == GLOBAL_POOL);
	locktoken = VM_POOL_LOCK(vm_pool);
	if (kt_basepri(curthreadp) == PBATCH) {
		vm_pool->pl_flags |= VM_POOL_WAIT;
		if (sv_wait_sig(&(vm_pool->pl_wait), 
			PZERO|PLTWAIT, &(vm_pool->pl_lck), locktoken) < 0)
			return 0; /* No retry */
		else return 1;
	} else if (is_bcritical(curthreadp)) {
		VM_POOL_UNLOCK(vm_pool, locktoken);
		return 1;	/* Do a retry */
	} else {
		VM_POOL_UNLOCK(vm_pool, locktoken);
		return 0 ; /* No retry */
	}
}

/*
 * vm_pool_wakeup:
 * Wakeup jobs that are sleeping for availsmem. This is called from unreservemem
 */
void
vm_pool_wakeup(vm_pool_t *vm_pool, int force_wakeup)
{
	/*
	 * We wait only in the global pool.
	 */
	if (vm_pool->pl_flags & VM_POOL_WAIT) {
		ASSERT(vm_pool == GLOBAL_POOL);
		if ((vm_pool->pl_smem_wanted && 
			(vm_pool->pl_availsmem > vm_pool->pl_smem_wanted)) ||
		   (vm_pool->pl_rmem_wanted && 
			(vm_pool->pl_availrmem > vm_pool->pl_rmem_wanted)) ||
			force_wakeup)  {
				vm_pool->pl_flags &= ~VM_POOL_WAIT;
				sv_broadcast(&vm_pool->pl_wait); 
				vm_pool->pl_smem_wanted = 0;
				vm_pool->pl_rmem_wanted = 0;
		}
	}
}
	

/*
 * vm_pool_retry_syscall:
 * Return 1 if the system call can be retried 0 otherwise.
 * This is called from postsyserrors() if a system call returns EMEMRETRY.
 * We wait for memory to become available in the global pool and retry
 * if necessary.
 */

int
vm_pool_retry_syscall(int syscallno)
{
	extern int fork(), exece(),mmap(), mmap64();

	syscallno +=  SYSVoffset;
	if ((syscallno == SYS_fork) ||
		(syscallno == SYS_mmap) ||
		(syscallno == SYS_mmap64) ||
		(syscallno == SYS_execv) ||
		(syscallno == SYS_execve)) {
		
		vm_pool_printf("retrying system call no %x\n", syscallno);

		return (vm_pool_wait(GLOBAL_POOL));
	} else
		return 0;
	
}


/*
 * vm_pool_update_guaranteed_mem:
 * Update the amount of memory guaranteed from this pool. This is used
 * to see if we resize the vm pool later on if the guaranteed memory
 * is lower than the available memory in the pool.
 */
void
vm_pool_update_guaranteed_mem(vm_pool_t *vm_pool, pgno_t guaranteed_mem)
{
	int s = VM_POOL_LOCK(vm_pool);
	vm_pool->pl_guaranteed_mem += guaranteed_mem;
	VM_POOL_UNLOCK(vm_pool, s);
}

/*
 * vm_pool_resize:
 * Resize the vm pool to the size passed in. First check if the new size
 * will go below the memory guaranteed to batch critical jobs. If so fail
 * the resize. If not shrink the pool size and give back memory to the GLOBAL
 * POOL. If resize needs more memory then resize the pool only if memory is 
 * available from the global pool.
 */
int
vm_pool_resize(vm_pool_t *vm_pool, pgno_t new_rmem)
{

	int s;

	ASSERT(vm_pool != GLOBAL_POOL);

	s = VM_POOL_LOCK(vm_pool);
	if (new_rmem < vm_pool->pl_guaranteed_mem) {
		VM_POOL_UNLOCK(vm_pool, s);
		return ENOMEM;
	}

	if (vm_pool->pl_availrmem == new_rmem) {
		VM_POOL_UNLOCK(vm_pool, s);
		return 0;
	} else if (vm_pool->pl_availrmem > new_rmem) {
		pgno_t excess_rmem = vm_pool->pl_availrmem - new_rmem;
		if (_reservemem(vm_pool, excess_rmem, excess_rmem, 0)) {
			VM_POOL_UNLOCK(vm_pool, s);
			return ENOMEM;
		}
		VM_POOL_UNLOCK(vm_pool, s);
		unreservemem(GLOBAL_POOL, excess_rmem, excess_rmem, 0);
		return 0;
	} else {
		pgno_t needed_rmem = new_rmem - vm_pool->pl_availrmem;
		VM_POOL_UNLOCK(vm_pool, s);
		if (reservemem(GLOBAL_POOL, needed_rmem, needed_rmem, 0))
			return ENOMEM;
		 unreservemem(vm_pool, 0, needed_rmem, 0);
		return 0;
	}
}

/*
 * Do r/smem accounting on the page if needed and mark it locked,
 * ie, non swappable/migratable. The caller has made sure the
 * page can not migrate by either grabbing an use count on the 
 * page (lockpages) or by keeping the pte locked (useracc).
 */
int
pas_pagelock(pas_t *pas, pfd_t *pfd, int smem, int interruptible)
{
	int x, error = 0;

	/*
	 * lock page - if we're the first then must
	 * decrement availrmem
	 */
	x = pfdat_lock(pfd);
	ASSERT(pfd->pf_use > 0);
	if (pfd->pf_rawcnt != 0) {
		/*
		 * Easy case - no need to get r/smem.
		 * Locking succeeds.
		 */
		pfdat_inc_rawcnt(pfd);
		pfdat_unlock(pfd, x);
	} else {
		/*
		 * Need to get r/smem, although we will
		 * free it up if someone else races us
		 * and accounts this page.
		 */
		pfdat_unlock(pfd, x);
		error = pas_reservemem(pas, smem, 1);
		/*
		 * Help detect deadlock, and make
		 * long lock operations interruptible.
		 * In the future this needs to check
		 * the interruptibility of the thread
		 * for now curthreadp should work.
 		 */
		if (interruptible && (isfatalsig(KT_TO_UT(curthreadp), NULL))) {
			if (error == 0)
				pas_unreservemem(pas, smem, 1);
			return(EINTR);
		}
		x = pfdat_lock(pfd);
		if (pfd->pf_rawcnt != 0) {
			/*
			 * If someone has already done r/smem,
			 * free up any we got. Locking succeeds
			 * even if we failed to get r/smem.
			 */
			pfdat_inc_rawcnt(pfd);
			pfdat_unlock(pfd, x);
			if (error == 0)
				pas_unreservemem(pas, smem, 1);
			else
				error = 0;
		} else {
			if (error == 0) {
				/*
				 * Locking succeeds only if we got r/smem.
				 */
				pfdat_inc_rawcnt(pfd);
			}
			pfdat_unlock(pfd,x);
		}
	}
	return(error);
}

/*
 * Returns whether the given page is becoming unlocked, in which case
 * caller should update r/smem.
 */
/* ARGSUSED */
int
pas_pageunlock(pas_t *pas, pfd_t *pfd)
{
	int dounresv = 0;
	int x = pfdat_lock(pfd);

	if (pfdat_dec_rawcnt(pfd) == 0)
		dounresv = 1;
	TILE_PAGEUNLOCK(pfd, 1);
	pfdat_unlock(pfd, x);
	PFD_WAKEUP_RAWCNT(pfd);
	return(dounresv);
}


#if NUMA_BASE

typedef struct {
	pgno_t	ga_availsmem;
	pgno_t	ga_availrmem;
	pgno_t	ga_usermem;
	time_t	ga_next_update;	
} global_availmem_t;

global_availmem_t	global_availmem;

static void
update_availmem(int force_update)
{
	cnodeid_t		node;
	global_availmem_t	next_ga;
	vm_cached_global_pool_t	*cgpool;

	POOLSTAT(cg_update_availmem);
	if (force_update || lbolt >= global_availmem.ga_next_update) {
		POOLSTAT(cg_update_availmem_required);
		global_availmem.ga_next_update = next_ga.ga_next_update = lbolt + HZ;
		next_ga.ga_availsmem = GLOBAL_POOL->pl_availsmem;
		next_ga.ga_availrmem = GLOBAL_POOL->pl_availrmem;
		next_ga.ga_usermem = GLOBAL_POOL->pl_usermem;
		if (GLOBAL_POOL->pl_flags&VM_POOL_CACHING_ENABLED) {
			for (node=0; node < numnodes; node++) {
				cgpool = CGPOOL(node);
				next_ga.ga_availsmem += cgpool->cgpl_availsmem;
				next_ga.ga_availrmem += cgpool->cgpl_availrmem;
				next_ga.ga_usermem += cgpool->cgpl_usermem;
			}
		}
		global_availmem = next_ga;
	}
}

pgno_t
get_availsmem(vm_pool_t *pool)
{
	if (pool == GLOBAL_POOL) {
		update_availmem(0);
		return global_availmem.ga_availsmem;
	} else
		return pool->pl_availsmem;
}


pgno_t
get_availrmem(vm_pool_t *pool)
{
	if (pool == GLOBAL_POOL) {
		update_availmem(0);
		return global_availmem.ga_availrmem;
	} else
		return pool->pl_availrmem;
}


pgno_t
get_usermem(vm_pool_t *pool)
{
	if (pool == GLOBAL_POOL) {
		update_availmem(0);
		return global_availmem.ga_usermem;
	} else
		return pool->pl_usermem;
}
#endif /* NUMA_BASE */


#ifdef VMPOOL_STATS
void
idbg_cgpool(int	full)
{
	vm_cached_global_pool_t	*cgpool, tmp;
	cnodeid_t			node;

	qprintf("state: %s. enabled_count: %d, update-availmem %d (%d)\n", 
		(GLOBAL_POOL->pl_flags&VM_POOL_CACHING_ENABLED) ? "ENABLED" : "DISABLED",
		cg_stats.cg_enabled, cg_stats.cg_update_availmem, cg_stats.cg_update_availmem_required);
	qprintf("AVAILMEM     smem %d, rmem %d, user %d\n", global_availmem.ga_availsmem,
		global_availmem.ga_availrmem, global_availmem.ga_usermem);
	qprintf("GLOBAL POOL  smem %d, rmem %d, user %d\n", GLOBAL_POOL->pl_availsmem,
		GLOBAL_POOL->pl_availrmem, GLOBAL_POOL->pl_usermem);

	bzero(&tmp, sizeof(tmp));
	for (node=0; node < numnodes; node++) {
		cgpool = CGPOOL(node);
		tmp.cgpl_availsmem += cgpool->cgpl_availsmem;
		tmp.cgpl_availrmem += cgpool->cgpl_availrmem;
		tmp.cgpl_usermem += cgpool->cgpl_usermem;
		tmp.cqpl_enable_caching += cgpool->cqpl_enable_caching;
		tmp.cqpl_enable_caching_failed += cgpool->cqpl_enable_caching_failed;
		tmp.cqpl_cgreserve += cgpool->cqpl_cgreserve;
		tmp.cqpl_cgreserve_fail += cgpool->cqpl_cgreserve_fail;
		tmp.cqpl_reservemem += cgpool->cqpl_reservemem;
		tmp.cqpl_reservefail += cgpool->cqpl_reservefail;
		tmp.cqpl_kreservemem += cgpool->cqpl_kreservemem;
	}
	qprintf("node availsmem availrmem usermem enable failed cgreserve cgfail reserve resfail kreserve\n");
	qprintf("%4s %9d %9d %7d %6d %6d", "TOT",
		tmp.cgpl_availsmem, tmp.cgpl_availrmem, tmp.cgpl_usermem,	
		tmp.cqpl_enable_caching, tmp.cqpl_enable_caching_failed);
	qprintf(" %9d %6d %7d %7d %8d\n", 
		tmp.cqpl_cgreserve, tmp.cqpl_cgreserve_fail, 
		tmp.cqpl_reservemem, tmp.cqpl_reservefail, 
		tmp.cqpl_kreservemem);

	if (full) {
		for (node=0; node < numnodes; node++) {
			cgpool = CGPOOL(node);
			qprintf("%4d %9d %9d %7d %6d %6d", node,
				cgpool->cgpl_availsmem, cgpool->cgpl_availrmem, cgpool->cgpl_usermem,	
				cgpool->cqpl_enable_caching, cgpool->cqpl_enable_caching_failed);
			qprintf(" %9d %6d %7d %7d %8d\n", 
				cgpool->cqpl_cgreserve, cgpool->cqpl_cgreserve_fail, 
				cgpool->cqpl_reservemem, cgpool->cqpl_reservefail, 
				cgpool->cqpl_kreservemem);
		}
	}
}

#endif /* VMPOOL_STATS */
