/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* NOTES
 *
 * This module implements basic pthread management functionality.
 *
 * Key points:
	* pthread identities are stored in one big array mmap()'d at startup
	* pthread IDs are 32 bit types made up of an index and a generation
	* each pthread has its own context which is saved/restored when a
	  full context switch takes place
 */

#include "common.h"
#include "asm.h"
#include "cv.h"
#include "delay.h"
#include "event.h"
#include "intr.h"
#include "key.h"
#include "ptattr.h"
#include "ptdbg.h"
#include "pthreadrt.h"
#include "q.h"
#include "sig.h"
#include "stk.h"
#include "sys.h"
#include "vp.h"
#include "pt.h"
#include "mtx.h"

#include <errno.h>
#include <fcntl.h>
#include <mutex.h>
#include <pthread.h>
#undef pthread_equal
#include <sched.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>



/* Pthread ID table management variables and macros.
 */

static struct rlimit	pt_limit;
static unsigned int	pt_active_count = 1;	/* # of active pthreads. */
static slock_t 		pt_alloc_lock;		/* pt_t alloc protection. */
static pt_t		*pt_table;		/* Table of all pt_t's. */
static ushort		pt_table_index = 1;	/* Count of used entries. */
static QS_DECLARE(pt_freeq);			/* Queue of dead pt_t's. */

/* Pthread_t ids: 32 bits
 *	 31..		     15..		0
 *	| Generation number | Table index        |
 */
#define PT_ID_INITIAL			0x10000	/* Initial pt's pthread_t */
#define PT_LABEL_START			0x10000	/* gen num starts at 1 */

#define PT_GEN_NUM(pthread_t)	(pthread_t >> 16)
#define PT_PTR_TO_ID(pt)	PT_MAKE_ID(PT_GEN_BITS(pt->pt_label), \
					   (unsigned short)(pt - pt_table))


static pt_t	*pt_alloc(pthread_t *);
static int	pt_table_limit(const struct rlimit *);
static void	pt_setup(pt_t *, ptattr_t *, void *(*)(void *), void *);
static pt_t	*pt_fixup_initial(void);
static void	pt_exit(register pt_t *);

/*
 * Cache pagesize for use by library
 */
int	pt_page_size = -1;	/* bad value for fault recognition */


/* Info structure for debugging.
 *
 * Initialised here because the data is local.
 */
#define PT_LIBRARY_VERSION		1	/* version number of library */
pt_lib_info_t	__pt_lib_info = {

	PT_LIBRARY_INFO_VERSION,

	PT_LIBRARY_VERSION,

	&pt_table,
	sizeof(pt_t),	/* XXX tmp hack for compat */
	&pt_table_index
};



/*
 * pt_bootstrap()
 *
 * Library bootstrap.  Called from .init section.
 */
void
pt_bootstrap(void)
{
	int		error;
	struct rlimit	pt_klimits;
	extern int	_getpagesize(void);

	pt_page_size = _getpagesize();

	if (getrlimit(RLIMIT_PTHREAD, &pt_klimits)) {
		panic("pt_bootstrap", "Couldn't get initial rlimits");
	}

	pt_limit.rlim_max = pt_klimits.rlim_max;
	if (pt_table_limit(&pt_klimits)) {
		panic("pt_bootstrap", "Couldn't create pt_table");
	}

	if (prctl(PR_INIT_THREADS)) {
		panic("pt_bootstrap", "Couldn't INIT_THREADS");
	}
	sig_bootstrap();
	sched_bootstrap();	/* sched depends on sig bootstrap */
	lock_bootstrap();	/* lock depends on sched bootstrap */
	intr_bootstrap();
	timeout_bootstrap();
	ptattr_bootstrap();
	mtx_bootstrap();

	vp_fixup_initial(pt_fixup_initial());

	if (error = libc_locks_init()) {
		exit(error);
	}

#ifdef DEBUG
	{
		/* Verify that the bit field code works.
		 */
		pt_t	pt;
		pt.pt_flags = PT_DETACHED | PT_CANCELLED;
		ASSERT(pt.pt_detached && pt.pt_cancelled);
		ASSERT(!pt.pt_cncl_deferred && !pt.pt_signalled);
	}
	/* Verify that the pt sync union of mutex and slocks is correct.
	 */
	{ int zero = 0;
	ASSERT( offsetof(mtx_t, mtx_slock) == zero );
	ASSERT( offsetof(mtx_t, mtx_slock) == offsetof(cv_t, cv_lock) + zero );
	ASSERT( sizeof(pthread_t) == 4 + zero );
	}
#endif
}


/*
 * pt_alloc(pt_id)
 *
 * A pt_t has a pt_label field (__uint32_t) which is made up of two 16-bit
 * parts: a generation number and a reference count.  Both are initially 0
 * for all pt_t's.
 *
 * A pt_id (__uint32_t) is also made up of two 16-bit parts: a generation
 * number and an index.
 *
 * When pt_alloc() is called, an unused slot from pt_table is chosen
 * (either from the free list or from the table itself) and the slot number
 * is combined with the generation number from the entry to form a pt_id.
 *
 * After a pt has been selected, its reference count is incremented.
 *
 * Returns a pt_t pointer and corresponding pt_id if successful.  NULL
 * otherwise.
 */
static pt_t *
pt_alloc(pthread_t *pt_id)
{
	pt_t		*pt;
	ushort		pt_index;

	sched_enter();
	QS_REM_FIRST(&pt_freeq, pt, pt_t *);
	if (pt) {
		pt_index = (unsigned short)(pt - pt_table);
		sched_leave();
	} else {
		lock_enter(&pt_alloc_lock);
		if (pt_table_index >= pt_limit.rlim_cur) {
			lock_leave(&pt_alloc_lock);
			sched_leave();

			return (NULL);
		}

		pt_index = pt_table_index++;
		pt = pt_table + pt_index;

		lock_leave(&pt_alloc_lock);
		sched_leave();

		pt->pt_label = PT_LABEL_START;
	}

	ASSERT(PT_REF_BITS(pt->pt_label) == 0);

	pt->pt_nid = *pt_id = (pt->pt_label | pt_index);	/* pthread_t */

	pt->pt_label++;		/* initial reference count is 1 */

	return (pt);
}


/*
 * pt_ref(pt_id)
 *
 * Increment reference count (if appropriate).
 *
 * Returns the corresponding pt_t's address if pt_id refers to a valid
 * thread.  NULL otherwise.
 */
pt_t *
pt_ref(pthread_t pt_id)
{
	unsigned short	pt_index = PT_INDEX(pt_id);

	if (ref_if_same(&(pt_table[pt_index].pt_label), PT_GEN_NUM(pt_id))) {

		ASSERT(PT_REF_BITS(pt_table[pt_index].pt_label) != 0);

		return (pt_table + pt_index);
	}

	return (NULL);
}


/*
 * pt_unref(pt)
 *
 * Decrement reference count.  If this is the last reference to pt, place
 * it on the free queue.
 */
void
pt_unref(pt_t *pt)
{
	if (unref_and_test(&pt->pt_label)) {
		ASSERT(pt->pt_state == PT_DEAD);

		sched_enter();

		/* Free any memory allocated for this pthread.
		 */
		if (pt->pt_data) {
			_free(pt->pt_data);
		}

		QS_ADD_FIRST(&pt_freeq, pt);
		sched_leave();
	}
}


/*
 * pt_foreach(func, arg)
 *
 * Call func with each active pthread in the library and arg as arguments.
 *
 * Acquires the pt_alloc_lock to prevent "missing" a thread.
 *
 * To avoid deadlock, func (or it's children) should not call pt_alloc() or
 * pt_unref() -- both of these (potentially) acquire pt_alloc_lock.
 */
void
pt_foreach(void (*func)(pt_t *, void *), void *arg)
{
	int index;

	sched_enter();
	lock_enter(&pt_alloc_lock);

	for (index = 0; index < pt_table_index; index++) {
		if (pt_table[index].pt_state != PT_DEAD) {
			func(pt_table + index, arg);
		}
	}

	lock_leave(&pt_alloc_lock);
	sched_leave();
}


/*
 * pt_table_limit(req)
 *
 * The pthread table is a contiguous mapped region, in two parts.
 * The mapped region is real memory from which pthread_create() allocates
 * pthreads - this represents the high water mark for the "soft" limit.
 * The reserved region represents memory that can be mapped if the soft
 * limit is raised - it is bounded by the "hard" limit.
 *
 * The hard limit may not be raised, it may be lowered to the soft limit
 * which will free reserved memory.
 * The soft limit may be raised up to the hard limit, resources permitting.
 * It may be lowered but has no effect on the current process.
 */
int
pt_table_limit(const struct rlimit *req)
{
	int 		dev_zero;
	size_t		size;
	caddr_t		unrsrv;
	static caddr_t	rsrv_reg;	/* where to extend mapping from */
#define PTPAGES(p) ((sizeof(pt_t) * (p) + pt_page_size-1) & ~(pt_page_size-1))

	extern int 	_open(const char *, int, ...);
	extern int 	_close(int);

	/* Sanity check request.
	 * new-soft-limit <= new-hard-limit <= current-hard-limit
	 */
	if (req->rlim_max > pt_limit.rlim_max
	    || req->rlim_cur > req->rlim_max) {
		return (EINVAL);
	}

	if ((dev_zero = _open("/dev/zero", O_RDWR)) == -1) {
		return (oserror());
	}

	if (!pt_table) {

		/* Bootstrap call
		 *
		 * Allocate reserved memory region.
		 */
		ASSERT(req->rlim_max == pt_limit.rlim_max);

		if ((rsrv_reg = mmap(0,
				     PTPAGES(pt_limit.rlim_max),
				     PROT_READ | PROT_WRITE,
				     MAP_PRIVATE | MAP_AUTORESRV,
				     dev_zero,
				     0))
		    == (caddr_t)MAP_FAILED) {
			(void)_close(dev_zero);
			return (oserror());
		}
		TRACE(T_PT, ("pt_table_limit(%d, %d) init %#x %#x",
				req->rlim_cur, req->rlim_max,
				rsrv_reg, PTPAGES(req->rlim_max)));
		pt_table = (void *)rsrv_reg;

	} else if (req->rlim_max < pt_limit.rlim_max) {

		/* Lowering hard limit (raising is illegal).
		 *
		 * Unmap reserved space beyond the new hard limit.
		 */
		size = PTPAGES(req->rlim_max);
		unrsrv = (caddr_t)pt_table + size;

		/* Make certain only reserved space is unmapped.
		 */
		size = PTPAGES(pt_limit.rlim_max) - size;
		if (unrsrv < rsrv_reg) {
			ASSERT(req->rlim_max < pt_limit.rlim_cur);
			ASSERT(size >= rsrv_reg - unrsrv);
			size -= rsrv_reg - unrsrv;
			unrsrv = rsrv_reg;
		}

		/* There may be no need to unmap any pages.
		 */
		if (size) {
			TRACE(T_PT, ("pt_table_limit(%d, %d) shrink %#x %#x",
					req->rlim_cur, req->rlim_max,
					unrsrv, size));

			munmap(unrsrv, size);
			pt_limit.rlim_max =
				(pt_limit.rlim_cur > req->rlim_max)
				? pt_limit.rlim_cur
				: req->rlim_max;
		}
	}

	if (req->rlim_cur > pt_limit.rlim_cur) {

		/* Raising soft limit (lowering is ignored).
		 *
		 * Map reserved space up to the new soft limit.
		 * Only do this if the new limit requires reserved pages.
		 */
		size = PTPAGES(req->rlim_cur);
		if ((caddr_t)pt_table + size > rsrv_reg) {

			size -= rsrv_reg - (caddr_t)pt_table;

			TRACE(T_PT, ("pt_table_limit(%d, %d) %#x %#x",
					req->rlim_cur, req->rlim_max,
					rsrv_reg, size));

			if (mmap(rsrv_reg,
				 size,
				 PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_FIXED,
				 dev_zero,
				 0)
			    != rsrv_reg) {
				(void)_close(dev_zero);
				return (oserror());
			}
			rsrv_reg += size;	/* update reserved */
		}

		pt_limit.rlim_cur = req->rlim_cur;
	}

	(void)_close(dev_zero);

	ASSERT(pt_limit.rlim_max >= pt_limit.rlim_cur);
	ASSERT(rsrv_reg - (caddr_t)pt_table == PTPAGES(pt_limit.rlim_cur));
	ASSERT(rsrv_reg - (caddr_t)pt_table <= PTPAGES(pt_limit.rlim_max));

	return (0);
}


/*
 * pthread_self()
 *
 * Returns the thread id for the currently running thread.
 * Safeguard against use before the library has initialised.
 */
pthread_t
pthread_self(void)
{
	register pt_t	*pt = PT;

	return (pt ? PT_PTR_TO_ID(pt) : PT_ID_INITIAL);
}


/*
 * pthread_create(pt_id, attr, routine, arg)
 *
 * Create a new pthread and start it running routine(arg).
 */
int
pthread_create(pthread_t *pt_id, const pthread_attr_t *attr,
	void *(*routine)(void *), void *arg)
{
	ptattr_t	*ptattr = attr ? (ptattr_t*)attr : &ptattr_default;
	pt_t		*pt;
	int		err;

	/* Allocate pt_t */
	if ((pt = pt_alloc(pt_id)) == NULL) {
		return (EAGAIN);
	}

	/* Allocate stack */
	if (!(pt->pt_stk = stk_alloc(ptattr->pa_stackaddr,
				     ptattr->pa_stacksize,
				     ptattr->pa_guardsize))) {
		pt_unref(pt);
		return (EAGAIN);
	}

	pt_setup(pt, ptattr, routine, arg);

	test_then_add32(&pt_active_count, 1);

	sched_enter();

	/* Start the pthread, if it is system scope we insist
	 * on a vp otherwise it joins the ready pool.
	 */
	if (pt->pt_system) {
		if (err = vp_create(pt, 0)) {
			stk_free(pt->pt_stk);
			sched_leave();
			pt->pt_state = PT_DEAD;
			pt_unref(pt);
			test_then_add32(&pt_active_count, -1);

			return (err);
		}
	} else {
		sched_dispatch(pt);
	}
	sched_leave();

	TRACE(T_PT, ("pthread_create(%#x)", pt));

	return (0);
}


/*
 * pthread_equal(pt1, pt2)
 *
 * return true if the pthreads referenced by pt1 and pt2 are the same,
 * false otherwise
 */
int
pthread_equal(pthread_t pt1, pthread_t pt2)
{
	return(pt1 == pt2);
}

/*
 * pthread_join(pt_id, status)
 *
 * Have the current thread wait for the exit status from thread.
 */
int
pthread_join(pthread_t pt_id, void **status)
{
	register pt_t	*pt_self = PT;
	register pt_t	*pt;
	int		wait_result;

	if ((pt = pt_ref(pt_id)) == NULL) {
		return (ESRCH);
	}
	TRACE(T_PT, ("pthread_join(%#x)", pt));

	if (pt == pt_self) {
		pt_unref(pt);
		return (EDEADLK);
	}

retry_join:

	/* We might get cancelled here.  If we do, make sure
	 * pt_unref() gets called.
	 */
	pthread_cleanup_push((void (*)(void *))pt_unref, (void *)pt);
	PT_INTR_PENDING(pt_self, PT_INTERRUPTS);
#	pragma set woff 1209
	pthread_cleanup_pop(0);
#	pragma reset woff 1209

	lock_enter(&pt->pt_lock);

	if (pt->pt_detached || !Q_EMPTY(&pt->pt_join_q)) {
		lock_leave(&pt->pt_lock);
		lock_leave(&pt_self->pt_lock);
		sched_leave();

		pt_unref(pt);
		return (EINVAL);
	}

	/* If the pthread has terminated, grab it's exit status and drop
	 * its reference count.  Otherwise wait for it to terminate.
	 */
	if (pt->pt_state == PT_DEAD) {
		if (status) {
			*status = pt->pt_value.exit_status;
		}

		lock_leave(&pt->pt_lock);
		lock_leave(&pt_self->pt_lock);
		sched_leave();

		wait_result = 0;
		pt_unref(pt);	/* Remove pthread_create's reference. */
	} else {
		pt_self->pt_state = PT_JWAIT;
		pt_self->pt_sync = (pt_sync_t *)&pt->pt_lock;
		lock_leave(&pt_self->pt_lock);

		pt_q_insert_tail(&pt->pt_join_q, pt_self);
		lock_leave(&pt->pt_lock);

		if ((wait_result = sched_block(SCHED_READY)) == EINTR) {
			goto retry_join;
		}

		if (wait_result == 0 && status) {
			*status = pt_self->pt_value.join_status;
		}
	}

	pt_unref(pt);
	return (wait_result);
}


/*
 * pthread_detach(pt_id)
 *
 * Arrange for the storage associated with pt_id to be freed when this
 * thread exits.  Implies that no threads can join to it.
 */
int
pthread_detach(pthread_t pt_id)
{
	pt_t	*pt;
	pt_t	*pt_join;

	if ((pt = pt_ref(pt_id)) == NULL) {
		return (ESRCH);
	}
	TRACE(T_PT, ("pthread_detach(%#x)", pt));

	sched_enter();
	lock_enter(&pt->pt_lock);

	if (pt->pt_detached) {
		lock_leave(&pt->pt_lock);
		sched_leave();
		pt_unref(pt);
		return (EINVAL);
	}

	pt->pt_detached = TRUE;

	/* If the pthread has terminated, drop its reference count.
	 * Otherwise wake up any joiner with an error.
	 */
	if (pt->pt_state == PT_DEAD) {
		lock_leave(&pt->pt_lock);

		pt_unref(pt);	/* Remove pthread_create's reference. */
	} else {
		if (pt_join = pt_dequeue(&pt->pt_join_q)) {
			pt_join->pt_wait = EINVAL;
			pt_join->pt_state = PT_DISPATCH;

			sched_dispatch(pt_join);
		}
		lock_leave(&pt->pt_lock);
	}

	sched_leave();

	pt_unref(pt);
	return (0);
}


/*
 * pthread_exit(exit_status)
 *
 * Pthread termination through explicit call, return from start function
 * or cancellation.
 */
void
pthread_exit(void *exit_status)
{
	register pt_t			*pt_self = PT;
	struct __pthread_cncl_hdlr	*ptch_list;

	TRACE(T_PT, ("pthread_exit(%#x)", exit_status));

	/* Execute cancellation cleanup handlers.  Update list so that if
	 * we recurse the handler is not repeated.
	 */
	for (ptch_list = pt_self->pt_cncl_first; ptch_list;
	     ptch_list = pt_self->pt_cncl_first) {
		pt_self->pt_cncl_first = ptch_list->__ptch_next;
		(*ptch_list->__ptch_func)(ptch_list->__ptch_arg);
	}

	/* Cleanup thread specific data.
	 */
	key_data_cleanup();

	SS_DEL_ID(pthread_self());

	/* Exit now if this is the last active thread.
	 */
	if (!add_then_test32(&pt_active_count, -1u)) {
		TRACE(T_PT, ("Last thread exited"));
		exit(0);
	}

	sched_enter();

	lock_enter(&pt_self->pt_lock);

	pt_self->pt_value.exit_status = exit_status;
	pt_self->pt_state = PT_DEAD;

	/* We might be running on a user supplied stack so we need to get
	 * off of it before we're done exiting (the user may try to reuse
	 * it).  Jump to the vp's stack by pt_longjmp()ing to pt_exit.
	 */
	pt_longjmp(pt_self, pt_exit, VP->vp_stk);

	panic("pthread_exit", "pt_longjmp() returned");
}


/*
 * pt_exit(pt_self)
 *
 * Latter half of pthread_exit no longer running on pthread stack.
 */
static void
pt_exit(register pt_t *pt_self)
{
	pt_t		*pt_join;
	int		system = pt_self->pt_system;

	TRACE(T_PT, ("pt_exit()"));

	ASSERT(sched_entered());
	ASSERT(lock_held(&pt_self->pt_lock));

	/* Any joiner wakes up with our exit status and we become detached.
	 */
	if (!Q_EMPTY(&pt_self->pt_join_q)) {
		ASSERT(!pt_self->pt_detached);

		pt_self->pt_detached = TRUE;

		if (pt_join = pt_dequeue(&pt_self->pt_join_q)) {
			pt_join->pt_value.join_status =
				pt_self->pt_value.exit_status;
			pt_join->pt_wait = 0;
			pt_join->pt_state = PT_DISPATCH;
			sched_dispatch(pt_join);
		}
	}

	/* Free the stack.
	 */
	stk_free(pt_self->pt_stk);

	/* If the pthread is detached, unref it.
	 */
	if (pt_self->pt_detached) {
		lock_leave(&pt_self->pt_lock);

		pt_unref(pt_self);	/* Remove pthread_create's reference. */
	} else {
		lock_leave(&pt_self->pt_lock);
	}

	/* Unbind the pthread from the vp which ensures the block cannot
	 * affect the exited pthread.
	 */
	CLR_PT(0);
	VP->vp_pt = 0;

	/* System scope threads have their own vp with (possibly)
	 * unique kernel attributes - so we delete it when done.
	 */
	if (system) {
		vp_exit();
	}

	/* Otherwise the vp goes in search of more work.
	 */
	(void)sched_block(SCHED_NEW);
	/* NOTREACHED */
}


/*
 * pt_start(old_pt, start_routine, start_arg)
 *
 * This function is called (instead of calling start_routine directly) so
 * we can nest the start routine within a pthread_exit() call and we'll be
 * able to clean up if the thread doesn't call pthread_exit() explicitly.
 */
void
pt_start(pt_t *old_pt, void *(*start_routine)(void *), void *start_arg)
{
	TRACE(T_PT, ("pt_start()"));

	SS_NEW_ID(pthread_self(), start_routine);
	sched_resume(old_pt);

	pthread_exit(start_routine(start_arg));

	panic("pt_start", "pthread_exit() returned");
}


/*
 * pt_setup(pt, pa, routine, arg)
 *
 * Initialize pt and get it ready to run.
 */
static void
pt_setup(pt_t *pt, ptattr_t *pa, void *(*routine)(void *), void *arg)
{
	extern int	_get_fpc_csr(void);

	TRACE(T_PT, ("pt_setup(%#x)", pt));

	lock_init(&pt->pt_lock);

	pt->pt_state	= PT_DISPATCH;
	pt->pt_vp	= 0;

	if (pa->pa_inherit) {
		pt->pt_schedpriority =
		pt->pt_priority = PT->pt_schedpriority;
		pt->pt_policy	= PT->pt_policy;
	} else {
		pt->pt_schedpriority =
		pt->pt_priority	= pa->pa_priority;
		pt->pt_policy	= pa->pa_policy;
	}

	pt->pt_flags	= PT_CNCLENABLED | PT_CNCLDEFERRED;
	pt->pt_detached = pa->pa_detached;

	pt->pt_system	= pa->pa_system;		/* not inherited */
	pt->pt_resched	= pt->pt_system ?0 :SCHED_POLICY_QUANTUM(pt->pt_policy);

	pt->pt_occupied	= FALSE;
	pt->pt_blocked	= 0;

	pt->pt_data		= 0;
	pt->pt_cncl_first	= 0;
	Q_INIT(&pt->pt_join_q);
	pt->pt_minhnext = pt->pt_minhprev = (mtx_t *)pt;

	pt->pt_errno		= 0;
	pt->pt_ksigmask		= PT->pt_ksigmask; /* Inherit parent sigmask */
	sigemptyset(&pt->pt_sigpending);
	pt->pt_sigwait		= NULL;

	/* Create a context to run.
	 * SP points to our newly created stack (caller set it up) and
	 * PC points to pt_crt0().
	 *
	 * ABI n32 and 64 restore the gp on longjmp()
	 */
#	if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
		pt->pt_context[JB_GP] = gp_reg();
#	endif
	pt->pt_context[JB_SP] = (machreg_t) pt->pt_stk;
	pt->pt_context[JB_PC] = (machreg_t) pt_crt0;
	pt->pt_context[JB_S0] = (machreg_t) routine;
	pt->pt_context[JB_S1] = (machreg_t) arg;
	pt->pt_context[JB_FPC_CSR] = _get_fpc_csr();

	/* That's it for default values.  All other fields are set before
	 * they are examined.
	 */
}


/*
 * pt_fixup_initial()
 *
 * Fix up the pthread state for the (implicit) initial thread.
 */
static pt_t *
pt_fixup_initial(void)
{
	pt_t	*pt_self;

	pt_self			= pt_table;		/* first entry */
	pt_self->pt_label	= PT_LABEL_START + 1;	/* initial gen, 1 ref */
	pt_self->pt_nid		= PT_PTR_TO_ID(pt_self);
	pt_self->pt_ksigmask	= VP_SIGMASK;		/* cur mask from PRDA */

	SET_PT(pt_self);
	pt_self->pt_stk 	= NULL;	/* stk_free() understands NULL */

	pt_setup(pt_self, &ptattr_default, 0, 0);

	pt_self->pt_state	= PT_EXEC;
	pt_self->pt_occupied	= TRUE;

	return (pt_self);
}


/*
 * pt_create_bound(ptp, routine, arg)
 *
 * Create a bound pthread and start it running routine(arg).  This thread
 * does not get counted as an "active thread".
 */
int
pt_create_bound(pt_t **ptp, void *(*routine)(void *), void *arg)
{
	pt_t		*pt;
	int		err;

	/* Allocate pt_t */
	if (!(pt = _malloc(sizeof(pt_t)))) {
		return (EAGAIN);
	}

	/* Allocate stack */
	if (!(pt->pt_stk = stk_alloc(ptattr_default.pa_stackaddr,
			      ptattr_default.pa_stacksize,
			      ptattr_default.pa_guardsize))) {
		_free(pt);
		return (EAGAIN);
	}

	pt_setup(pt, &ptattr_default, routine, arg);
	pt->pt_glued = TRUE;
	pt->pt_resched = 0;

	pt->pt_ksigmask = sched_mask;		/* special start state */
	pt->pt_context[JB_FPC_CSR] = 0;		/* must be sane */

	sched_enter();
	if (err = vp_create(pt, PR_EVENT)) { /* XXX */
		stk_free(pt->pt_stk);
		sched_leave();
		_free(pt);
		return (err);
	}
	sched_leave();

	*ptp = pt;

	TRACE(T_PT, ("pt_create_bound(%#x)", pt));

	return (0);
}


/*
 * pt_dequeue(queue)
 *
 * Pull the first pt off of queue.
 */
pt_t *
pt_dequeue(q_t *queue)
{
	pt_t	*pt;

	Q_REM_FIRST(queue, pt, pt_t *);

	TRACE(T_VP, ("pt_dequeue(%#x) pt %#x", queue, pt));

	return (pt);
}


/*
 * pt_fork_child()
 *
 * Called in child after fork() and takes responsibility for
 * calling other post-fork() code.
 * The policy is to fix up the library state as fast as possible
 * even at the expense of lost memory.
 */
void
pt_fork_child(void)
{
	pt_t	*pt_self = PT;
	pt_t	*corpse;
	int	i;

	ASSERT(sched_entered());

	lock_init(&pt_self->pt_lock);
	pt_self->pt_label	= PT_GEN_BITS(pt_self->pt_label) + 1;
	pt_self->pt_state	= PT_EXEC;
	pt_self->pt_vp		= VP;
	pt_self->pt_flags	&= ~(PT_INTERRUPTS | PT_CNCLPENDING | PT_BOUND);
	pt_self->pt_occupied	= TRUE;
	pt_self->pt_blocked	= 0;
	Q_INIT(&pt_self->pt_join_q);
	sigemptyset(&pt_self->pt_sigpending);

	/* XXX fork() leaks
	 *	pt_data
	 *	stk_t for non-dynamic stacks
	 */
	pt_active_count = 1;
	lock_init(&pt_alloc_lock);
	QS_INIT(&pt_freeq);

	stk_fork_child();
	for (i = 0, corpse = pt_table; i < pt_table_index; i++, corpse++) {
		if (corpse == pt_self) {
			continue;
		}
		corpse->pt_label = PT_LABEL_START;
		if (corpse->pt_state != PT_DEAD) {
			stk_free(corpse->pt_stk);
			corpse->pt_state = PT_DEAD;
		}
		QS_ADD_FIRST(&pt_freeq, corpse);
	}

	vp_fork_child();

	/* Misc. */
	intr_destroy_sync = 0;
	timeout_bootstrap();
	mtx_bootstrap();
	evt_fork_child();
}


/* libc_setrlimit(typ, rlim)
 *
 * Wrapper to implement the RLIMIT_PTHREAD limit.
 */
/* ARGSUSED */
int
libc_setrlimit(int typ, const struct rlimit *rlim)
{
	int	ret;

	ASSERT(typ == RLIMIT_PTHREAD);

	sched_enter();
	lock_enter(&pt_alloc_lock);

	if (ret = pt_table_limit(rlim)) {
		setoserror(ret);
		ret = -1;
	}
	lock_leave(&pt_alloc_lock);
	sched_leave();
	return (ret);
}


/*
 * POSIX configurable system values (sysconf()).
 *
 * The libc sysconf() interface calls this function when queried for
 * POSIX.1C variables specific to the library:
 *
 * Nb: we must ensure that this symbol is imported by the linker
 * so that it preempts the dummy in libc.
 */
int
libc_sysconf(int name)
{
	switch(name) {

	/* Runtime values.
	 */
	case _SC_THREAD_DESTRUCTOR_ITERATIONS:
		return (_POSIX_THREAD_DESTRUCTOR_ITERATIONS);

	case _SC_THREAD_KEYS_MAX:
#		if _POSIX_THREAD_KEYS_MAX > PT_KEYS_MAX
#			error PT_KEYS_MAX below POSIX limit
#		endif
		return (PT_KEYS_MAX);

	case _SC_THREAD_STACK_MIN:
#		if _POSIX_THREAD_STACK_MIN > PT_STACK_MIN
#			error PT_STACK_MIN below POSIX limit
#		endif
		return (PT_STACK_MIN);

	case _SC_THREAD_THREADS_MAX:
		return ((int)pt_limit.rlim_cur);

	/* Libc takes care of these values in sysconf().
	 *	_SC_GETGR_R_SIZE_MAX
	 *	_SC_GETPW_R_SIZE_MAX
	 *	_SC_THREAD_SAFE_FUNCTIONS
	 *	_SC_LOGIN_NAME_MAX
	 *	_SC_TTY_NAME_MAX
	 */

	/* Options we support.
	 */
	case _SC_THREADS:
	case _SC_THREAD_ATTR_STACKADDR:
	case _SC_THREAD_ATTR_STACKSIZE:
	case _SC_THREAD_PRIORITY_SCHEDULING:
	case _SC_THREAD_PRIO_INHERIT:
	case _SC_THREAD_PRIO_PROTECT:
	case _SC_THREAD_PROCESS_SHARED:
		return (1);
	}
	setoserror(EINVAL);
	return (-1);
}


/* libc_pttokt(pt_id, kt)
 *
 * Undercover conversion of a pthread_t to a kernel handle.
 */
int
libc_pttokt(pthread_t pt_id, tid_t *kt)
{
	pt_t	*pt;

	if ((pt = pt_ref(pt_id)) == NULL) {
		return (ESRCH);
	}
	sched_enter();
	lock_enter(&pt->pt_lock);
	if (!pt->pt_system || pt->pt_state == PT_DEAD) {
		lock_leave(&pt->pt_lock);
		sched_leave();
		pt_unref(pt);
		return (EINVAL);
	}
	*kt = pt->pt_vp->vp_pid;
	lock_leave(&pt->pt_lock);
	sched_leave();
	pt_unref(pt);
	return (0);
}


/* libc_ptpri()
 *
 * Undercover priority peek.
 */
int
libc_ptpri(void)
{
	return (PT->pt_priority);
}


/* libc_ptuncncl()
 *
 * Undercover switch to cancel a current cancellation.
 */
void
libc_ptuncncl(void)
{
	pt_t	*pt_self = PT;

	sched_enter();
	lock_enter(&pt_self->pt_lock);
	pt_self->pt_cncl_enabled = PTHREAD_CANCEL_ENABLE;
	lock_leave(&pt_self->pt_lock);
	sched_leave();
}
