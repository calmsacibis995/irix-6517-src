/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.56 $"

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/poll.h"
#include "ksys/vproc.h"
#include "sys/proc.h"
#include <os/as/region.h> /* XXX */
#include "ksys/vfile.h"
#include "sys/sema.h"
#include "sys/vnode.h"
#include "sys/debug.h"
#include "sys/kmem.h"
#include "sys/cmn_err.h"
#include "sys/usync.h"
#include <sys/conf.h>
#include <sys/runq.h>
#include <sys/ddi.h>
#include <sys/idbgentry.h>
#include <sys/xlate.h>
#include <sys/sema_private.h>
#include <sys/kabi.h>
#include <sys/atomic_ops.h>
#include <sys/idbg.h>
#ifdef CKPT
#include <sys/ckpt.h>
#endif
#include "os/proc/pproc_private.h"  /* XXX bogus */
#include <os/as/as_private.h>       /* XXX bogus */
#include <sys/kthread.h>

/*
 * Usync
 *
 * Control module for the user-level synchronization of processes or threads 
 *
 */

/*
 * CPR IRIX 6.4/6.5 restart support:
 * XXX The following data structure may be removed for all major
 *     releases of IRIX following (but not including) 6.5.
 *
 * Version 1 usync data structure format
 */
typedef struct usync_v1_arg {
	int ua_version;
	__uint64_t ua_addr;
	ushort_t ua_policy;
	ushort_t ua_flags;
	usync_sigevent_t ua_sigev;
	__uint64_t ua_reserved[2];
} usync_v1_arg_t;

/*
 * Usync object
 */
typedef struct usync_obj {
	sv_t	uo_wait;		/* wait object	   	     */
	int	uo_count;		/* number of blocked procs   */
	int	uo_prepost;		/* prepost value	     */
	caddr_t	uo_handle;		/* vnode or region pointer   */
	off_t	uo_off;			/* vnode or region offset    */
	int	uo_flag;		/* debug identification flags*/
	pid_t	uo_notify_pid;		/* notification owner        */
	long	uo_notify_stime;	/* detects proc pid reuse    */
	uint	uo_notify_exec;		/* detects proc execs        */
	int	uo_handoff;		/* handoff state             */
	struct usync_obj *uo_prev;	/* hash list pointer         */
	struct usync_obj *uo_next;	/* hash list pointer	     */
	mutex_t	uo_lock;		/* protects usync_obj	     */
	int	uo_refs;		/* usync_obj reference count */
} usync_obj_t;

/*
 * Usync object flags
 */
#define UO_FLAG_PRIO		0x01
#define UO_FLAG_DEAD		0x02
#define UO_FLAG_MQ_NOTIFY	0x10	/* Debug hint */
#define UO_FLAG_MUTEX		0x20	/* Debug hint */
#define UO_FLAG_CV		0x40	/* Debug hint */

/*
 * Usync object macros
 */

#define UO_LOCK(uo)		mutex_lock(&uo->uo_lock, PZERO)
#define UO_UNLOCK(uo)		mutex_unlock(&uo->uo_lock)

#define UO_INC_REF(uo)		atomicAddInt(&uo->uo_refs, 1)
#define UO_DEC_REF(uo)		atomicAddInt(&uo->uo_refs, -1)
#define UO_TRYDEC_REF(uo)	compare_and_dec_int_gt(&uo->uo_refs, 1)
 
static struct zone *usync_zone;

#define US_DBG_GET_UO		0x1
#define US_DBG_UO_ALLOC		0x2
#define US_DBG_UO_FREE		0x4
#define US_DBG_BLOCK		0x8
#define US_DBG_UNBLOCK		0x10
#define US_DBG_CLEANUP		0x20
#define US_DBG_CTRL		0x40
#define US_DBG_FREE_HNDL	0x80
#define US_DBG_REM_UO		0x100

#ifdef DEBUG

static int usyncdebug;			/* runtime debug control */
static int usync_cleanup_cnt;
static int usync_block_cnt;
static int usync_intr_block_cnt;
static int usync_handoff_cnt;
static int usync_timed_block_cnt;
static int usync_unblock_cnt;
static int usync_unblock_all_cnt;
static int usync_get_state_cnt;
static int usync_notify_register_cnt;
static int usync_notify_cnt;
static int usync_notify_delete_cnt;
static int usync_notify_clear_cnt;
static int usync_interrupt_cnt;

#define UDPRINT(debug_lvl,arg) if (usyncdebug & debug_lvl) qprintf arg
#define UDINCR(debug_var) atomicAddInt(&debug_var, 1)

#else  /* DEBUG */
#define UDPRINT(debug_lvl,arg)
#define UDINCR(debug_var)
#endif /* DEBUG */

/*
 * Hash Table for usync objects
 */

static struct uo_hash {
	mutex_t		uoh_lock;
	usync_obj_t	*uoh_next;
	usync_obj_t	*uoh_cache_list;
	int		uoh_cache_count;
} *uo_hash;

u_int usync_cache_max = 4;
u_int uo_hash_mask;

#define UO_HASH_SHIFT	4
#define UO_HASH(uo_addr, uo_off) \
	(((((__psint_t)uo_addr) >> UO_HASH_SHIFT) + uo_off) & uo_hash_mask)

/*
 * mutex/cv to synchronize waiting for mem/freeing up memory
 */

extern int usync_max_objs;

lock_t	usync_mem_lock;	/* protects usync_objs */
sv_t	usync_mem_wait;
int     max_usync_objs;
int     usync_objs;

extern void qprintf(char *, ...);

/* Local functions */

static int  usync_object_get(caddr_t, usync_obj_t **, short);
static void usync_object_remove(usync_obj_t *);
static void usync_print_uo(usync_obj_t *);
static void usync_print_table(void);
static int  usync_get_handle(uthread_t *, caddr_t, caddr_t *, off_t *);

/*
 * usync_cntl:
 *
 * System call entry point
 *
 * Errno values:	EINVAL - invalid user address
 *			EINTR - process interrupted
 */

struct usync_cntla {
	sysarg_t cmd;
	usync_arg_t *ua;
};

int
usync_cntl(struct usync_cntla *uap, rval_t *rvp)
{
	usync_arg_t	sa;
	caddr_t		addr;
	usync_obj_t	*uo;
	usync_obj_t	*handoff_uo;
	proc_t		*p = NULL;
	sigval_t	sval;
	int		ret = 0;
	vproc_t		*vpr;
	int		nprocs;
	int		resid;
	struct timespec	now, timeout, *tsp;
	int		ver;

#if _K64U64
	UDPRINT(US_DBG_CTRL,
	("usync_cntl: cmd = 0x%x arg = 0x%llx \n", uap->cmd, uap->ua));
#else
	UDPRINT(US_DBG_CTRL,
	("usync_cntl: cmd = 0x%x arg = 0x%x \n", uap->cmd, uap->ua));
#endif	/* _K64U64 */

	if (copyin((caddr_t)uap->ua, (caddr_t)&sa, sizeof(usync_arg_t))) {
		/*
		 * CPR IRIX 6.4/6.5 restart support:
		 * XXX This version 1 copyin may be removed for all major
		 *     releases of IRIX following (but not including) 6.5
		 */
		if (copyin((caddr_t)uap->ua,
			   (caddr_t)&sa,
			   sizeof(usync_v1_arg_t)))
			return EFAULT;
	}

	ver = sa.ua_version;
	if (ver != USYNC_VERSION_2 && ver != USYNC_VERSION_1)
		return EINVAL;

	addr = (caddr_t ) sa.ua_addr;
	if (ret = usync_object_get(addr, &uo, sa.ua_policy))
		return (ret);

	/*
	 * uo == locked usync object
	 */
	ASSERT(MUTEX_OWNER(uo->uo_lock.m_bits) == curthreadp);

	switch (uap->cmd) {

	case USYNC_HANDOFF:
		UDINCR(usync_handoff_cnt);

		/*
		 * Protect against deadlock
		 */
		if (sa.ua_addr == sa.ua_handoff) {
			ret = EDEADLK;
			goto error;
		}

		/*
		 * Get secondary object to handoff
		 */
		ret = usync_object_get((caddr_t) sa.ua_handoff,
				       &handoff_uo,
				       sa.ua_policy);
		if (ret)
			goto error;

		/*
		 * Set up for handoff of secondary object
		 */
		handoff_uo->uo_flag |= UO_FLAG_MUTEX;
		handoff_uo->uo_handoff++;
		if (sv_signal(&handoff_uo->uo_wait) == 0)
			handoff_uo->uo_prepost++;

		usync_object_remove(handoff_uo);

		/*
		 * Set up primary object timeout, if specified.
		 * The specified timeout is an absolute timeout.
		 */
		if (sa.ua_flags & USYNC_FLAGS_TIMEOUT) {
			tsp = &timeout;
			tsp->tv_sec = (time_t)sa.ua_sec;
			tsp->tv_nsec = (long)sa.ua_nsec;
			nanotime(&now);
			timespec_sub(tsp, &now);
			if (tsp->tv_sec < 0
			    || tsp->tv_sec == 0 && tsp->tv_nsec <= 0) {
				/* timeout already expired */
				ret = ETIMEDOUT;
				goto error;
			}
		} else {
			tsp = NULL;
		}

		/*
		 * Now wait on primary object
		 */

		if (uo->uo_flag & UO_FLAG_PRIO) { 
			kthread_t *kt = private.p_curkthread;
			if (sa.ua_flags & USYNC_FLAGS_USERPRI)
				kt->k_qkey = sa.ua_userpri;
			else
				kt->k_qkey = kt->k_pri;
		}

		uo->uo_flag |= UO_FLAG_CV;
		uo->uo_count++;
		ret = sv_timedwait_sig_notify(&uo->uo_wait, PZERO, &uo->uo_lock,
					      0, 0, tsp, NULL);
		UO_LOCK(uo);
		uo->uo_count--;

		if (ret) {
			UDINCR(usync_interrupt_cnt);
			if (ret == -2)
				ret = ETIMEDOUT;
			else
				ret = EINTR;
		}
		break;

	case USYNC_INTR_BLOCK:
		UDINCR(usync_intr_block_cnt);

		if (uo->uo_prepost == 0) {
			/*
			 * Block
			 */

			if (uo->uo_flag & UO_FLAG_PRIO) {
				kthread_t *kt = private.p_curkthread;
				if (sa.ua_flags & USYNC_FLAGS_USERPRI)
					kt->k_qkey = sa.ua_userpri;
				else
					kt->k_qkey = kt->k_pri;
			}

			uo->uo_count++;
			ret = sv_wait_sig(&uo->uo_wait, PZERO, &uo->uo_lock, 0);

			UO_LOCK(uo);
			uo->uo_count--;

			if (ret) {
				UDINCR(usync_interrupt_cnt);
				ret = EINTR;
			}
		} else {
			/*
			 * No need to block, since a prepost is pending.
			 */
			ASSERT(uo->uo_prepost > 0);
			uo->uo_prepost--;
		}

		if (ret == 0 && uo->uo_handoff) {
			uo->uo_handoff--;
			rvp->r_val1 = 1;
		}
		break;

	case USYNC_BLOCK:
		UDINCR(usync_block_cnt);
		if (uo->uo_prepost == 0) {
			/*
			 * Block
			 */

			if (uo->uo_flag & UO_FLAG_PRIO) {
				kthread_t *kt = private.p_curkthread;
				if (sa.ua_flags & USYNC_FLAGS_USERPRI)
					kt->k_qkey = sa.ua_userpri;
				else
					kt->k_qkey = kt->k_pri;
			}

			uo->uo_count++;
			sv_wait(&uo->uo_wait, PZERO, &uo->uo_lock, 0);

			UO_LOCK(uo);
			uo->uo_count--;
		} else {
			/*
			 * No need to block, since a prepost is pending.
			 */
			ASSERT(uo->uo_prepost > 0);
			uo->uo_prepost--;
		}
		break; 

	case USYNC_UNBLOCK:
		UDINCR(usync_unblock_cnt);
		/*
		 * CPR IRIX 6.4/6.5 restart support: old semantic
		 * XXX The following version check may be removed for all
		 *     major releases of IRIX following (but not including) 6.5
		 */
		if (ver == USYNC_VERSION_1)
			sa.ua_flags = 0;

		/*
		 * Unblock one process
		 */
		if (sv_signal(&uo->uo_wait) == 0) {
			if (!(sa.ua_flags & USYNC_FLAGS_PREPOST_NONE)) {
				/*
				 * Prepost!
				 *
				 * No one to wake-up! Either processes blocked
				 * earlier have been signalled, or the unblock
				 * operation beat the block operation into the
				 * kernel.
				 */
				uo->uo_prepost++;
			}
		}
		break;

	case USYNC_UNBLOCK_ALL:
		UDINCR(usync_unblock_all_cnt);
		/*
		 * Unblock all blocked processes
		 */
		nprocs = rvp->r_val1 = sv_broadcast(&uo->uo_wait);
		if (!(sa.ua_flags & USYNC_FLAGS_PREPOST_NONE)
		    && (resid = sa.ua_count - nprocs) > 0)
			uo->uo_prepost += resid;

		break;

	case USYNC_GET_STATE:
		UDINCR(usync_get_state_cnt);
		/*
		 * Get the count/prepost state of a usync object
		 */
		if (uo->uo_prepost) {
			rvp->r_val1 = uo->uo_prepost;
			if (sa.ua_flags & USYNC_FLAGS_PREPOST_CONSUME)
				uo->uo_prepost--;
		} else {
			int val = 0;
			if (sa.ua_flags & USYNC_FLAGS_PREPOST_CONSUME) {
				/* inaccuracy ok */
				val = uo->uo_count;
			} else {
				/* accuracy required */
				if (uo->uo_count)
					val = sv_waitq(&uo->uo_wait);
			}					
			rvp->r_val1 = 0 - val;
		}

		break;

	case USYNC_NOTIFY_REGISTER:
		UDINCR(usync_notify_register_cnt);
		uo->uo_flag |= UO_FLAG_MQ_NOTIFY;

		/*
		 * check if some other proc is already registered
		 */
		if ((uo->uo_notify_pid != 0) &&
		    (vpr = VPROC_LOOKUP(uo->uo_notify_pid)) != NULL) {
			VPROC_GET_PROC(vpr, &p);
		}

		if (p != NULL) {
			/*
			 * another proc registered
			 */
			if (p->p_start == uo->uo_notify_stime &&
			    p->p_exec_cnt == uo->uo_notify_exec) {
				VPROC_RELE(vpr);
				/*
				 * return EBUSY to indicate that the
				 * previously registered process still exists
				 */
				ret = EBUSY;
				goto error;
			}
			VPROC_RELE(vpr);
		}

		uo->uo_notify_pid = curprocp->p_pid;
		uo->uo_notify_stime = curprocp->p_start;
		uo->uo_notify_exec = curprocp->p_exec_cnt;

		/*
		 * We increment the prepost here to prevent the uo from
		 * being removed until the notification is either sent
		 * or deleted
		 */
		uo->uo_prepost = 1;
		break; 

	case USYNC_NOTIFY:
		UDINCR(usync_notify_cnt);
		/*
		 * send signal to the process that registered for async
		 * notification
		 */
		if ((uo->uo_notify_pid != 0) &&
			(vpr = VPROC_LOOKUP(uo->uo_notify_pid)) != NULL) {
			VPROC_GET_PROC(vpr, &p);
		}

		if (p != NULL) {
			if (p->p_start == uo->uo_notify_stime &&
			    p->p_exec_cnt == uo->uo_notify_exec) {
				sval.sival_ptr =
					(void *) sa.ua_sigev.ss_value;
				ksigqueue(vpr, sa.ua_sigev.ss_signo,
					sa.ua_sigev.ss_si_code, sval, 0);
			} else {
				/*
				 * the proc that registered for async
				 * notification doesn't exist; it's
				 * pid has been recycled
				 */
				ret = ENOENT;
			}
			VPROC_RELE(vpr);
		} else
			ret = EINVAL;

		/*
		 * remove the notification
		 */
		uo->uo_notify_pid = 0;
		uo->uo_prepost = 0;
		break; 

	case USYNC_NOTIFY_DELETE:
		UDINCR(usync_notify_delete_cnt);

		/*
		 * delete the notification registration, if it belongs
		 * to this process
		 */
		p = curprocp;

		if (p->p_pid == uo->uo_notify_pid &&
		    p->p_start == uo->uo_notify_stime &&
		    p->p_exec_cnt == uo->uo_notify_exec) {
			uo->uo_notify_pid = 0;
			uo->uo_prepost = 0;
		} else
			ret = EBUSY;

		break;

	case USYNC_NOTIFY_CLEAR:
		UDINCR(usync_notify_clear_cnt);
		/*
		 * delete the notification registration
		 */
		if (uo->uo_notify_pid)
			uo->uo_prepost = 0;
		uo->uo_notify_pid = 0;
		break;

	default:
		ret = EINVAL;
	}

error:
	usync_object_remove(uo);

	return (ret);
}

/*
 * usync_object_get:
 *
 * Lookup the usync object associated with addr.
 *
 * Returns: locked usync object
 */
static int
usync_object_get(caddr_t user_addr, usync_obj_t **uo, short policy)
{
	usync_obj_t *tmp_uo;
	struct uo_hash *uoh;
	caddr_t addr;
	off_t off;
	int hindex;
	int s;

	if (usync_get_handle(NULL, user_addr, &addr, &off))
		return EINVAL;

#if _K64U64
	UDPRINT(US_DBG_GET_UO,
	("usync_object_get: user_addr = 0x%llx, addr = 0x%llx off = 0x%llx\n",
			user_addr, addr, off));
#else
	UDPRINT(US_DBG_GET_UO,
	("usync_object_get: user_addr = 0x%x, addr = 0x%x off = 0x%llx\n",
			user_addr, addr, off));
#endif
	/*
	 * Hash chain lookup
	 */
	hindex = UO_HASH(addr, off);
	ASSERT((hindex >= 0) && (hindex <= uo_hash_mask));
	uoh = &uo_hash[hindex];

start_over:

	mutex_lock(&uoh->uoh_lock, PZERO);	

	for (tmp_uo = uoh->uoh_next; tmp_uo; tmp_uo = tmp_uo->uo_next) {
                if ((tmp_uo->uo_handle == addr) && (tmp_uo->uo_off == off))
			break;
	}

	if (tmp_uo) {
		/*
		 * A usync object exists
		 */
		UO_INC_REF(tmp_uo);
		mutex_unlock(&uoh->uoh_lock);
		UO_LOCK(tmp_uo);

		/*
		 * Make sure the uo is not marked for dead
		 */
		if (tmp_uo->uo_flag & UO_FLAG_DEAD) {
			usync_object_remove(tmp_uo);
			return EINVAL;
		}
	} else {
		/*
		 * A usync object was not found in the hash table.
		 * Allocate and initialize a new one.
		 */

		/* Check hash chain cache first */
		if (uoh->uoh_cache_count) {
			tmp_uo = uoh->uoh_cache_list;
			ASSERT(tmp_uo);
			uoh->uoh_cache_list = tmp_uo->uo_next;
			uoh->uoh_cache_count--;
		} else {
			s = mutex_spinlock(&usync_mem_lock);
			if (usync_objs >= max_usync_objs) {
				mutex_unlock(&uoh->uoh_lock);
				if (sv_wait_sig(&usync_mem_wait, PZERO,
						&usync_mem_lock, s) == -1) {
					return EINTR;
				}
				goto start_over;
			}
			usync_objs++;
			mutex_spinunlock(&usync_mem_lock, s);

			tmp_uo = kmem_zone_alloc(usync_zone, KM_SLEEP);
		}

		/* insert in hash chain */
		tmp_uo->uo_handle = addr;
		tmp_uo->uo_off = off;

		tmp_uo->uo_next = uoh->uoh_next;
		tmp_uo->uo_prev = NULL;
		if (uoh->uoh_next)
			uoh->uoh_next->uo_prev = tmp_uo;
		uoh->uoh_next = tmp_uo;

		/* set first reference */
		tmp_uo->uo_refs = 1;

		/* grab uo lock */
		mutex_init(&tmp_uo->uo_lock, MUTEX_DEFAULT, "uo_lock");
		UO_LOCK(tmp_uo);

		/* release hash lock */
		mutex_unlock(&uoh->uoh_lock);

		/*
		 * The new object is now on the hash chain, and we own it.
		 * Continue with initialization.
		 */
		tmp_uo->uo_count = 0;
		tmp_uo->uo_prepost = 0;
		tmp_uo->uo_notify_pid = 0;
		tmp_uo->uo_handoff = 0;

		if (policy == USYNC_POLICY_PRIORITY) {
			sv_init(&tmp_uo->uo_wait, SV_KEYED, "uo_wait:p");
			tmp_uo->uo_flag = UO_FLAG_PRIO;
		} else {
			sv_init(&tmp_uo->uo_wait, SV_FIFO, "uo_wait:f");
			tmp_uo->uo_flag = 0;
		}			
	}

	*uo = tmp_uo;

	return 0;
}

/*
 * usync_object_remove:
 *
 * Remove the usync object if it is no longer needed.
 */
static void
usync_object_remove(usync_obj_t *uo)
{
	struct uo_hash *uoh;
	int preposts;
	int s;
	int refs;

	/*
	 * Usync object locking & reference counting
	 *
	 * Warning: this isn't as simple as one might think.
	 *
	 * A usync object reference count can only be atomically incremented
	 * when the hash chain lock is held.
	 *
	 * A usync object reference count can be atomically decremented
	 * whenever the decrementor knows it already has a reference
	 * (no locks are necessary).
	 *
	 * A usync object can only be removed when: the hash chain lock is
	 * held, references are zero, and preposts are zero.  The usync object
	 * lock doesn't need to be acquired when removing the uo, since we
	 * implicitly have exclusive access to the uo when the hash chain lock
	 * is acquired AND references are zero.
	 *
	 * Preposts can only be modified when the usync object lock is held,
	 * and the usync object lock can only be acquired if at least one
	 * reference is outstanding.  Therefore, if there are no references,
	 * the uo lock cannot be acquired and preposts cannot be modified.
	 */

	ASSERT(uo->uo_refs);
	if (preposts = uo->uo_prepost)
		UO_DEC_REF(uo);

	UO_UNLOCK(uo);

	if (preposts || UO_TRYDEC_REF(uo))
		return;

	/*
	 * At this point we still hold our reference, and we believe
	 * we hold the last one, but we won't know for sure until we
	 * acquire the hash chain lock.
	 */

#if _K64U64
	UDPRINT(US_DBG_REM_UO, ("usync_object_remove: uo = 0x%llx\n", uo));
#else
	UDPRINT(US_DBG_REM_UO, ("usync_object_remove: uo = 0x%x\n", uo));
#endif

	/*
	 * Grab hash chain lock
	 */
	uoh = &uo_hash[UO_HASH((uo->uo_handle),(uo->uo_off))];
	mutex_lock(&uoh->uoh_lock, PZERO);

	refs = UO_DEC_REF(uo);

	/*
	 * Check if the usync object can be removed
	 */
	if (refs == 0 && uo->uo_prepost == 0) {

		ASSERT(!(MUTEX_ISLOCKED(&uo->uo_lock)));
		ASSERT(!(SV_WAITER(&uo->uo_wait)));

		/*
		 * Remove from the hash chain
		 */
		if (uo->uo_next)
			uo->uo_next->uo_prev = uo->uo_prev;
		if (uo->uo_prev)
			uo->uo_prev->uo_next = uo->uo_next;
		else
			uoh->uoh_next = uo->uo_next;

		mutex_destroy(&uo->uo_lock);
		sv_destroy(&uo->uo_wait);

		if (uoh->uoh_cache_count < usync_cache_max) {
			uo->uo_next = uoh->uoh_cache_list;
			uoh->uoh_cache_list = uo;
			uoh->uoh_cache_count++;
			mutex_unlock(&uoh->uoh_lock);
		} else {
			mutex_unlock(&uoh->uoh_lock);
			kmem_zone_free(usync_zone, (void *) uo);

			s = mutex_spinlock(&usync_mem_lock);
			if (usync_objs-- >= max_usync_objs)
				sv_broadcast(&usync_mem_wait);
			mutex_spinunlock(&usync_mem_lock, s);
		}
	} else
		mutex_unlock(&uoh->uoh_lock);

	return;
}

/*
 * usync_get_handle:
 *
 *	Find unique handle for a given user address.
 *
 *	The computed handle is either a vnode/offset pair for vnode-backed
 *	memory, or a region/offset pair for anonymous memory.
 *
 *	The handle is used by the usync module to allocate unique usync
 *	objects for process synchronization.
 *
 *	A "callback" flag is used for calling the usync module when the
 *	memory object is unmapped by all processes: RG_USYNC or VUSYNC.
 *
 *	Returns: 0 on success, with addr and off set to the unique handle.
 *	 	-1, on failure (in case the user address is invalid)
 */
static int
usync_get_handle(uthread_t *ut, caddr_t user_addr, caddr_t *addr, off_t *off)
{
	register preg_t *prp;
	register reg_t *rp;
	pgno_t rpn;
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;

	if (ut == NULL)
		as_lookup_current(&vasid);
	else
		as_lookup(ut->ut_asid, &vasid);

	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;

	mraccess(&pas->pas_aspacelock);
	if ((prp = findpreg(pas, ppas, user_addr)) == NULL) {
		mrunlock(&pas->pas_aspacelock);
		if (ut)
			as_rele(vasid);
		return -1;
	}

	rp = prp->p_reg;

	/*
	 * vnode pointer is the handle for vnode-backed objects
	 * and region pointer is the handle for the anon-backed objects
	 */
	if (rp->r_anon == NULL) {
		ASSERT(rp->r_vnode);

		VN_FLAGSET(rp->r_vnode, VUSYNC);

		*addr = (caddr_t) rp->r_vnode;
		rpn = vtorpn(prp, user_addr);
		*off = (off_t) (ctob(rpn) + rp->r_fileoff);

	} else {
		rp->r_flags |= RG_USYNC;

		*addr = (caddr_t) rp;
		rpn = vtoapn(prp, user_addr);
		*off = (off_t) ctob(rpn);
	}

	mrunlock(&pas->pas_aspacelock);

	*off += poff(user_addr);	/* add the offset part */

	if (ut)
		as_rele(vasid);

	return 0;
}

/*
 * usyncinit
 */
void
usyncinit(void)
{
	int i;

	max_usync_objs = usync_max_objs;
	/*
	 * Allocate locks, synch variables, hash table
	 */
	usync_zone = kmem_zone_init(sizeof(usync_obj_t),"usync_objects");
	spinlock_init(&usync_mem_lock, "usync_mem_lock");
	sv_init(&usync_mem_wait, SV_PRIO, "usync_mem_wait");
	
	for (i = 1; i < max_usync_objs; i <<= 1)
		;

	if (i >= 10)
		i /= 10;
	uo_hash_mask = i - 1;	
	uo_hash = kmem_zalloc(i * sizeof (*uo_hash), KM_SLEEP);
	while (i-- > 0) {
		mutex_init(&(uo_hash[i].uoh_lock), MUTEX_DEFAULT, "uoh_lock");
	}

	idbg_addfunc("usync_table", usync_print_table);
	idbg_addfunc("usync", usync_print_uo);
}

/*
 * usync_cleanup:
 *
 * Free all usync objects allocated for this handle, i.e. for all offsets
 * within this mem object, because the object is unmapped by all processes.
 */
void
usync_cleanup(caddr_t handle)
{
	usync_obj_t *uo, *uo_tmp;
	int i, s, freed = 0;

	UDINCR(usync_cleanup_cnt);

	for (i = 0; i <= uo_hash_mask; i++) {
		mutex_lock(&(uo_hash[i].uoh_lock), PZERO);	
		uo = uo_hash[i].uoh_next;

		while (uo) {

			if (uo->uo_handle == handle) {

				/*
				 * We don't bother taking a reference
				 * here, since we are not going to drop
				 * the hash chain lock.
				 */
				UO_LOCK(uo);

				/*
				 * If stragglers exist, wake them up and
				 * let them handle the tear-down.
				 *
				 * Note: some may be blocked waiting for,
				 * the uo lock. To prevent further operations,
				 * we mark the uo for dead.
				 */
				if (uo->uo_refs) {
					(void) sv_broadcast(&uo->uo_wait);
					uo->uo_flag |= UO_FLAG_DEAD;
					uo->uo_prepost = 0;
					UO_UNLOCK(uo);
					uo = uo->uo_next;
					continue;
				}

				/*
				 * Otherwise the usync object is no longer
				 * in use.
				 */
				ASSERT(!(SV_WAITER(&uo->uo_wait)));

				/*
				 * Remove from the hash list, no caching
				 */
				if (uo->uo_next)
					uo->uo_next->uo_prev = uo->uo_prev;
				if (uo->uo_prev)
					uo->uo_prev->uo_next = uo->uo_next;
				else
					uo_hash[i].uoh_next = uo->uo_next;

				uo_tmp = uo->uo_next;

				/*
				 * Tear-down synchronizers
				 */
				UO_UNLOCK(uo);
				mutex_destroy(&uo->uo_lock);
				sv_destroy(&uo->uo_wait);

				kmem_zone_free(usync_zone, (void *) uo);
				freed++;

				uo = uo_tmp;
			} else {
				uo = uo->uo_next;
			}
		}
		mutex_unlock(&(uo_hash[i].uoh_lock));
	}

	s = mutex_spinlock(&usync_mem_lock);
	if (usync_objs >= max_usync_objs)
		sv_broadcast(&usync_mem_wait);
	usync_objs -= freed;
	mutex_spinunlock(&usync_mem_lock, s);

	return;
}

#ifdef CKPT
/*
 * usync_object_ckpt:
 *
 * Find all usync objects in the region for process p identified by user_addr
 */
int
usync_object_ckpt(uthread_t *ut, caddr_t user_addr, int *count, caddr_t bufaddr)
{
	struct uo_hash *uoh;
	usync_obj_t *tmp_uo;
	caddr_t addr;
	off_t off;
	int i, j = 0;
	usync_ckpt_t ckpt;

	if (usync_get_handle(ut, user_addr, &addr, &off))
		return EINVAL;
#if _K64U64
	UDPRINT(US_DBG_GET_UO,
	("usync_object_get: user_addr = 0x%llx, addr = 0x%llx off = 0x%llx\n",
			user_addr, addr, off));
#else
	UDPRINT(US_DBG_GET_UO,
	("usync_object_get: user_addr = 0x%x, addr = 0x%x off = 0x%llx\n",
			user_addr, addr, off));
#endif
	for (i = 0; i <= uo_hash_mask; i++) {

		mutex_lock(&(uo_hash[i].uoh_lock), PZERO);	
		uoh = &uo_hash[i];

		for (tmp_uo = uoh->uoh_next; tmp_uo; tmp_uo = tmp_uo->uo_next) {

	                if ((tmp_uo->uo_handle == addr)&&(tmp_uo->uo_off >= off)) {
				if (j < *count) {
					ckpt.off = tmp_uo->uo_off;
					ckpt.voff = tmp_uo->uo_off - off;
					ckpt.notify = tmp_uo->uo_notify_pid;
					ckpt.value = tmp_uo->uo_prepost;
					ckpt.handoff = tmp_uo->uo_handoff;
					if (tmp_uo->uo_flag & UO_FLAG_PRIO)
						ckpt.policy = USYNC_POLICY_PRIORITY;
					else 
						ckpt.policy = USYNC_POLICY_FIFO;
					if (copyout((caddr_t)&ckpt, bufaddr, sizeof(ckpt)) < 0)
						return EFAULT;
	
					bufaddr += sizeof(ckpt);
				}
				j++;
			}
		}
		mutex_unlock(&uoh->uoh_lock);
	}
	*count = j;

	return (0);
}
#endif

/*
 * print usync_obj struct
 */
static void
usync_print_uo(usync_obj_t *uo)
{
#if _K64U64
	qprintf("uo 0x%llx handle 0x%llx offset 0x%llx\n",
		uo, uo->uo_handle, uo->uo_off);
	qprintf("   lock 0x%llx wait 0x%llx\n", &uo->uo_lock, &uo->uo_wait);
#else
	qprintf("uo 0x%x handle 0x%x offset 0x%x\n",
		uo, uo->uo_handle, uo->uo_off);
	qprintf("   lock 0x%x wait 0x%x\n", &uo->uo_lock, &uo->uo_wait);
#endif
	qprintf("   count %d prepost %d refs %d handoff %d\n",
		uo->uo_count, uo->uo_prepost, uo->uo_refs, uo->uo_handoff);

	qprintf("   flags:");

	if (uo->uo_flag & UO_FLAG_DEAD)
		qprintf(" DEAD");

	if (uo->uo_flag & UO_FLAG_PRIO)
		qprintf(" PRIO");
	else
		qprintf(" FIFO");

	if ((uo->uo_flag & ~(UO_FLAG_PRIO|UO_FLAG_DEAD)) == 0)
		qprintf(" SEM MQ");
	else if (uo->uo_flag & UO_FLAG_MQ_NOTIFY)
		qprintf(" MQ_NOTIFY");
	else if (uo->uo_flag & UO_FLAG_MUTEX)
		qprintf(" MUTEX");
	else if (uo->uo_flag & UO_FLAG_CV)
		qprintf(" CV");

	qprintf("\n");
}

/*
 * dump list of block structures
 */
static void
usync_print_table(void)
{
	int i;
	usync_obj_t *uo;

	qprintf("usync_objs = %d\n", usync_objs);

#ifdef DEBUG

	qprintf("usync_cleanup_cnt = %d\n", usync_cleanup_cnt);
	qprintf("usync_block_cnt = %d\n", usync_block_cnt);
	qprintf("usync_intr_block_cnt = %d\n", usync_intr_block_cnt);
	qprintf("usync_handoff_cnt = %d\n", usync_handoff_cnt);
	qprintf("usync_timed_block_cnt = %d\n", usync_timed_block_cnt);
	qprintf("usync_unblock_cnt = %d\n", usync_unblock_cnt);
	qprintf("usync_unblock_all_cnt = %d\n", usync_unblock_all_cnt);
	qprintf("usync_interrupt_cnt = %d\n", usync_interrupt_cnt);
	qprintf("usync_get_state_cnt = %d\n", usync_get_state_cnt);
	qprintf("usync_notify_register_cnt = %d\n", usync_notify_register_cnt);
	qprintf("usync_notify_cnt = %d\n", usync_notify_cnt);
	qprintf("usync_notify_delete_cnt = %d\n", usync_notify_delete_cnt);
	qprintf("usync_notify_clear_cnt = %d\n", usync_notify_clear_cnt);

	usync_cleanup_cnt = 0;
	usync_block_cnt = 0;
	usync_intr_block_cnt = 0;
	usync_handoff_cnt = 0;
	usync_timed_block_cnt = 0;
	usync_unblock_cnt = 0;
	usync_unblock_all_cnt = 0;
	usync_interrupt_cnt = 0;
	usync_get_state_cnt = 0;
	usync_notify_register_cnt = 0;
	usync_notify_cnt = 0;
	usync_notify_delete_cnt = 0;
	usync_notify_clear_cnt = 0;

#endif	/* DEBUG */

	qprintf("hash_table_size = %d\n", uo_hash_mask + 1);
	for (i=0; i <= uo_hash_mask; i++) {
		uo = uo_hash[i].uoh_next;
		if (uo || uo_hash[i].uoh_cache_count) {
#if _K64U64
			qprintf("hash index [%d] cache %d hlock 0x%llx\n",
				i, uo_hash[i].uoh_cache_count,
				&uo_hash[i].uoh_lock);
#else
			qprintf("hash index [%d] cache %d hlock 0x%x\n",
				i, uo_hash[i].uoh_cache_count,
				&uo_hash[i].uoh_lock);
#endif
			while (uo) {
				usync_print_uo(uo);
				uo = uo->uo_next;
			}
		}
	}
}
