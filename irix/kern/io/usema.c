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

#ident	"$Revision: 1.100 $"

#include "sys/types.h"
#include "sys/avl.h"
#include "sys/systm.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/kmem.h"
#include "sys/poll.h"
#include <ksys/vproc.h>
#include "sys/proc.h"
#include "sys/prctl.h"
#include "ksys/vfile.h"
#include "ksys/fdt.h"
#include "sys/sema.h"
#include "sys/vnode.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/usioctl.h"
#include <sys/conf.h>
#include <sys/atomic_ops.h>
#include <sys/ddi.h>
#include <sys/idbgentry.h>
#include <sys/xlate.h>
#include <sys/capability.h>

#ifdef DEBUG
#include <sys/idbg.h>
#endif
#include "os/proc/pproc_private.h"	/* XXX bogus */

/*
 * usema - polled semaphore driver for asynchronous blocking
 *
 * There are 2 devices:
 *	a cdev minor 0 for getting new semaphores
 *	a cdev minor 1 for attaching to existing semaphores
 *	All others are semaphore devices
 *
 * XXX - race when exiting/clean up of upsinfo structs
 */

/*
 * Thread Tracking: The following data structures are maintained to allow
 *                  a single thread of execution to be queued on multiple
 *                  polled semaphores.
 *
 *           +-----------+
 *           |  Proc C   |
 *       +-----------+   |
 *       |  Proc B   |---+
 *   +-----------+   |
 *   |  Proc A   |---+
 *   |  uplist_t |
 *   +-----------+
 *         |
 *         V
 *   +-----------+    +------------+    +------------+    +------------+
 *   | Proc A    |    | Proc A     |    | Proc A     |    | Proc A     |
 *   | Sem 1     |->  | Sem 1      |->  | Sem 1      |->  | Sem 1      |
 *   |           |    | Thread 1   |    | Thread 2   |    | Thread 3   |
 *   | upsinfo_t |    | upstinfo_t |    | upstinfo_t |    | upstinfo_t |
 *   +-----------+    +------------+    +------------+    +------------+
 *         |
 *         V
 *   +-----------+    +------------+    +------------+    +------------+
 *   | Proc A    |    | Proc A     |    | Proc A     |    | Proc A     |
 *   | Sem 2     |->  | Sem 2      |->  | Sem 2      |->  | Sem 2      |
 *   |           |    | Thread 1   |    | Thread 2   |    | Thread 3   |
 *   | upsinfo_t |    | upstinfo_t |    | upstinfo_t |    | upstinfo_t |
 *   +-----------+    +------------+    +------------+    +------------+
 *
 * Note: semaphores (minor devices) queue upstinfo_t structures.
 *
 */

#define ATTACHMINOR	1
#define CLONEMINOR	0

int usdevflag = D_MP;

static mrlock_t uslock;
#ifdef DEBUG
static void dumpup(int x);
static void dumpub(int x);
#endif
/*VARARGS1*/
void qprintf(char *, ...);
void usexit(void);

/*
 * upstinfo queuing macros
 */

#define UB_QUEUEINIT(ub)			\
	ub->u_qhead = NULL;			\
	ub->u_qtail = NULL;

#define UB_ENQUEUE(ub, ut)			\
	if (ub->u_qtail) {			\
		ASSERT(ub->u_qhead);		\
		ub->u_qtail->pst_next = ut;	\
	} else {				\
		ASSERT(ub->u_qhead == NULL);	\
		ub->u_qhead = ut;		\
	}					\
	ub->u_qtail = ut;			\
	ut->pst_next = NULL;			\
	ub->u_waiters++;

#define UB_DEQUEUE(ub, ut)			\
	if ((ut = ub->u_qhead) != NULL)	{	\
		if (ub->u_qhead == ub->u_qtail)	\
			ub->u_qtail = NULL;	\
		ub->u_qhead = ut->pst_next;	\
		ub->u_waiters--;		\
	}

#define UB_RMQUEUE(ub, ut) {					\
	upstinfo_t *scan, *prev = NULL;				\
	for (scan = ub->u_qhead; scan; scan = scan->pst_next) {	\
		if (scan == ut)	{				\
			if (prev)				\
			 	prev->pst_next = scan->pst_next;\
			else					\
			 	ub->u_qhead = scan->pst_next; 	\
			if (scan == ub->u_qtail)		\
				ub->u_qtail = prev;		\
			ub->u_waiters--;			\
			break;					\
		}						\
		prev = scan;					\
	}							\
}

/*
 * per semaphore info
 * The u_lock protects updates to ALL processes (upsinfo struct) that
 * reference this semaphore.
 */
typedef struct {
	sv_t			u_wait;
	lock_t			u_lock;
	int 			u_prepost;
	int 			u_waiters;
	struct upstinfo_s	*u_qhead;
	struct upstinfo_s	*u_qtail;
	vfile_t			*u_fp;
	void 			*u_handle; /* user level semaphore owner  */
					   /* field address for recursion */
	struct pollhead		u_poll;
	struct upstinfo_s	*u_lastups;
} ublock_t;

/*
 * per process info
 */
typedef struct uplist_s {
	avlnode_t		p_node; /* tree linkage -- must be first */
	pid_t			p_pid;
	avltree_desc_t		p_pslist; /* root of upsinfo tree */
} uplist_t;

/*
 * per process per semaphore info
 */
typedef struct upsinfo_s {
	avlnode_t		ps_node; /* tree linkage -- must be first */
	dev_t			ps_dev;
	pid_t			ps_pid;
	avltree_desc_t		ps_pstlist; /* root of upstinfo tree */
} upsinfo_t;

/*
 * per process per semaphore per thread info
 *
 * These are the queued entities
 */
typedef struct upstinfo_s {
	avlnode_t		pst_node; /* tree linkage -- must be first */
	pid_t			pst_pid;
	dev_t			pst_dev;
	int			pst_tid;  
	int			pst_count;
	struct upstinfo_s	*pst_next; /* Q link */
} upstinfo_t;

/*
 * Avl tree search key hooks
 */ 
static __psunsigned_t
upl_avl_start(avlnode_t *up)
{
	return ((__psunsigned_t) (((uplist_t *)up)->p_pid));
}

static __psunsigned_t
upl_avl_end(avlnode_t *up)
{
	return ((__psunsigned_t) (((uplist_t *)up)->p_pid) + 1);
}

static __psunsigned_t
ups_avl_start(avlnode_t *up)
{
	return ((__psunsigned_t) (((upsinfo_t *)up)->ps_dev));
}

static __psunsigned_t
ups_avl_end(avlnode_t *up)
{
	return ((__psunsigned_t) (((upsinfo_t *)up)->ps_dev) + 1);
}

static __psunsigned_t
upt_avl_start(avlnode_t *up)
{
	return ((__psunsigned_t) (((upstinfo_t *)up)->pst_tid));
}

static __psunsigned_t
upt_avl_end(avlnode_t *up)
{
	return ((__psunsigned_t) (((upstinfo_t *)up)->pst_tid) + 1);
}

static avlops_t upl_avlops = {
	upl_avl_start,
	upl_avl_end,
};

static avlops_t ups_avlops = {
	ups_avl_start,
	ups_avl_end,
};

static avlops_t upt_avlops = {
	upt_avl_start,
	upt_avl_end,
};

/* table of active procs */
static avltree_desc_t uplist;

/* table of active semaphores */
static ublock_t **usmintab;
static int usnmin;

/* maximum number of minor devices */
#define USMAXDEVS 262140 

/* usmintab table expansion increment */
#define USEXNUM	 25

static int
current_tid(void)
{
	int tid;

	if (IS_THREADED(curuthread->ut_pproxy))
		tid = curuthread->ut_prda->t_sys.t_nid;
	else {
		/* Note: this value should be -1 to be consistent with
		 * the tids used in libc, but avl trees do not accept
		 * negative keys.
		 * 
		 * Instead, we will use zero to mean non-pthreaded
		 * and correct the inconsistancy when marking the
		 * owner of a recursive semaphore in UIOCAUNBLOCKQ.
		 */
		tid = 0;
	}

	return tid;
}
	
/*
 * getpstinfo - given the semaphore's minor number, as well as,
 *	        the pid and tid of a particular thread:
 *
 *	        Create requisite structures
 */
static upstinfo_t *
getpstinfo(dev_t dev, pid_t pid, int tid)
{
	register uplist_t *up = NULL, *tup = NULL;
	register upsinfo_t *ups, *tups;
	register upstinfo_t *upst, *tupst;
	int free_up = 0;
	int free_ups = 0;
	int free_upst = 0;

	/* first alloc upsinfo (per proc/sem) entry */
	ups = kmem_alloc(sizeof(*ups), KM_SLEEP);
	ups->ps_pid = pid;
	ups->ps_dev = dev;
	avl_init_tree(&ups->ps_pstlist, &upt_avlops);

	/* then allocate a upstinfo (per proc/sem/thread) entry */
	upst = kmem_alloc(sizeof(*upst), KM_SLEEP);
	upst->pst_pid = pid;
	upst->pst_dev = dev;
	upst->pst_tid = tid;
	upst->pst_count = 0;
	upst->pst_next = NULL;

again:
	mrupdate(&uslock);

	/*
	 * Find and set-up (if necessary) the per proc structure
	 */
	if ((tup = (uplist_t *)avl_find(&uplist, (__psunsigned_t) pid)) == NULL)
	{
		if (up == NULL) {
			mrunlock(&uslock);

			up = kmem_zalloc(sizeof(*up), KM_SLEEP);
			avl_init_tree(&up->p_pslist, &ups_avlops);
			up->p_pid = pid;
			goto again;
		}
		tup = (uplist_t *) avl_insert(&uplist, (avlnode_t *)up);
		ASSERT(tup == up);
	} else {
		if (up) {
			/* Someone beat us to it, free the struct later */
			free_up++;
		}
	}

	/*
	 * Find the per proc/sem structure
	 */
	tups = (upsinfo_t *) avl_find(&tup->p_pslist, (__psunsigned_t) dev);

	if (tups) {
		/* Someone beat us to it, free the struct later */
		free_ups++;
	} else {
		tups =  (upsinfo_t *)
			avl_insert(&tup->p_pslist, (avlnode_t *) ups);
		ASSERT(tups == ups);
	}

	/*
	 * Find the per proc/sem/thread structure
	 */
	tupst = (upstinfo_t *)
		avl_find(&tups->ps_pstlist, (__psunsigned_t) tid);	

	if (tupst) {
		/* Someone beat us to it, free the struct later */
		free_upst++;
	} else {
		tupst = (upstinfo_t *)
			avl_insert(&tups->ps_pstlist, (avlnode_t *) upst);
		ASSERT(tupst == upst);
	}

	mrunlock(&uslock);

	/*
	 * Free-up the data structures we didn't need
	 */
	if (free_up)
		kmem_free(up, sizeof(*up));
	else if (up)
		(void) add_exit_callback(pid, 0, (void (*)(void *))usexit, 0);

	if (free_ups)
		kmem_free(ups, sizeof(*ups));

	if (free_upst)
		kmem_free(upst, sizeof(*upst));

	return(tupst);
}

/*
 * devtopstinfo - given minor number, a pid, and a tid
 * 		  convert to particular thread's upstinfo struct
 */
static upstinfo_t *
devtopstinfo(dev_t dev, pid_t pid, int tid)
{
	register uplist_t *up;
	register upsinfo_t *us;
	register upstinfo_t *upst = NULL;

	mraccess(&uslock);
	/* find process */
	up = (uplist_t *) avl_find(&uplist, (__psunsigned_t) pid);

	if (up) {
		/* find semaphore */  
		us = (upsinfo_t *) avl_find(&up->p_pslist, (__psunsigned_t) dev);
		if (us) {
			/* find thread */
			upst = (upstinfo_t *) avl_find(&us->ps_pstlist,
						       (__psunsigned_t) tid);
		}
	}

	mraccunlock(&uslock);
	return(upst);
}

/*
 * getsema - allocate a minor dev and replace current open fd with new one
 * pointing to this minor dev (a 'clone')
 *
 * We can pass in a minor number to force a particular device.  It returns
 * EBUSY if its already being used.
 */
int
getsema(dev_t *devp)
{
	register int i, j;
	register ublock_t *ub;
	struct vfile *nfp;
	int minor = minor(*devp);

	/*
	 * heres a hack - we really need to know the file * for this
	 * file being opened. open nicely side effects
	 * ut_openfp for us.
	 */
	if ((nfp = curuthread->ut_openfp) == NULL)
		return EINVAL;

	if (!VF_IS_VNODE(nfp))
		return EINVAL;

	mrupdate(&uslock);

	if (minor != CLONEMINOR) {
		int err = 0;
		
		if (minor <= ATTACHMINOR || minor >= USMAXDEVS)
			err = EINVAL;
		else if (minor < usnmin && usmintab[minor] != NULL)
			err = EBUSY;

		if (err) {
			mrunlock(&uslock);
			return err;
		}
		
		i = minor;
	} else {
		/*
		 * Cycle around rather than always starting at 2 so to
		 * reduce the chance of minor number being busy. Start
		 * searching from last+1 to give each slot equal chance. 
		 */
		static int last = 1;
		for (i = last+1; i != last; i++) {
			/* try to re-cycle first before expanding */
			if (i >= usnmin)
				i = 0;

			/* find a free one */
			if (usmintab[i] == NULL)
				break;
		}
		if (i == last)
			last = i = usnmin;
		else
			last = i;
	}

	if (i >= usnmin) {
		register int newusnmin, expansion;
		if (usnmin >= USMAXDEVS) {
			mrunlock(&uslock);
			return ENOSPC;
		}
		/* expand usmintab */
		expansion = max(USEXNUM, i - usnmin+1);
		newusnmin = min(USMAXDEVS, usnmin + expansion);
		usmintab = kern_realloc(usmintab,
					newusnmin * sizeof(ublock_t *));
		for (j = usnmin; j < newusnmin; j++)
			usmintab[j] = NULL;
		usnmin = newusnmin;
	}

	/* init new entry */
	ub = usmintab[i] = kern_calloc(1, sizeof(ublock_t));
	init_sv(&ub->u_wait, SV_DEFAULT, "uses", i);
	init_spinlock(&ub->u_lock, "usel", i);
	initpollhead(&ub->u_poll);

	/* return new device number to signal a clone */
	ASSERT(i > ATTACHMINOR && i < USMAXDEVS);
	*devp = makedev(emajor(*devp), i);

	UB_QUEUEINIT(ub);

	ub->u_fp = nfp;
	ub->u_prepost = 0;
	ub->u_waiters = 0;

	mrunlock(&uslock);
	return 0;
}

/*
 * attachsema - create an open file descriptor to the device passed in
 * This is basically just a dup
 */
int
attachsema(vfile_t *fp, int *fdp)
{
	int	error;
	vproc_t	*vp;

	vp = VPROC_LOOKUP(current_pid());
	VPROC_FDT_DUP(vp, fp, fdp, error);
	VPROC_RELE(vp);
	return(error);
}

/*
 * usema - semaphore driver
 */
void
usinit(void)
{
	mrinit(&uslock, "uslock");
	avl_init_tree(&uplist, &upl_avlops);

	usmintab = (ublock_t **)kern_calloc(USEXNUM, sizeof(ublock_t *));
	usmintab[CLONEMINOR] = (ublock_t *)1L;	/* so don't give it out */
	usmintab[ATTACHMINOR] = (ublock_t *)1L;	/* so don't give it out */
	usnmin = USEXNUM;
	
#ifdef DEBUG
	idbg_addfunc("usemproc", dumpup);
	idbg_addfunc("usemdev", dumpub);
#endif
}

int
usopen(dev_t *devp)
{
	dev_t dev = *devp;

	if (minor(dev) != ATTACHMINOR)
		return getsema(devp); 

	return 0;
}

#if _MIPS_SIM == _ABI64
static int irix5_to_usattach(enum xlate_mode, void *,
				int, struct xlate_info_s *);
#endif

/*
 * usioctl
 * Can only do UIOCATTACHSEMA on minor ATTACHMINOR.
 * All others only on minor > ATTACHMINOR
 */
/* ARGSUSED */
int
usioctl(dev_t dev,
	unsigned int cmd,
	__psint_t arg,
	int flag,
	struct cred *cr,
	int *rvalp)
{
	register ublock_t	*ub;
	register uplist_t	*up;
	register upsinfo_t	*ups;
	register upstinfo_t	*upst;
	register pid_t		pid;
	register int		tid;
	register int		s;
	register int		mindev;
	vproc_t			*vpr;
	usattach_t		at;
	int	 		error;
	proc_t	 		*p;

	mindev = minor(dev);
	ASSERT(mindev <= usnmin);

	mraccess(&uslock);
	ub = usmintab[mindev];
	mraccunlock(&uslock);

	switch (cmd) {
	case UIOCABLOCKQ:
		/*
		 * async block with kernel queuing
		 */
		ASSERT(ub);
		if (mindev <= ATTACHMINOR)
			return EINVAL;

		pid = current_pid();
		tid = current_tid();

		if ((upst = devtopstinfo(dev, pid, tid)) == NULL) {
			/* first time */
			upst = getpstinfo(dev, pid, tid);
		}

		ASSERT(upst);

		s = mp_mutex_spinlock(&ub->u_lock);
		if (upst->pst_count < 0) {
			/* Already on the queue */
			mp_mutex_spinunlock(&ub->u_lock, s);
			return ERANGE;
		}

		ASSERT(upst->pst_count == 0);

		if (ub->u_prepost) {
			/* Consume prepost */
			ub->u_prepost--;
			mp_mutex_spinunlock(&ub->u_lock, s);
			return EAGAIN;
		}

		UB_ENQUEUE(ub, upst);
		upst->pst_count--;
		mp_mutex_spinunlock(&ub->u_lock, s);

		return 0;

	case UIOCAUNBLOCKQ:
		/*
		 * async unblock with kernel queuing
		 */
		ASSERT(ub);
		if (mindev <= ATTACHMINOR)
			return EINVAL;

	getanother:

		s = mp_mutex_spinlock(&ub->u_lock);
		UB_DEQUEUE(ub, upst);
		if (upst == NULL) {
			/*
			 * Nobody to wakeup - prepost!
			 */
			ub->u_prepost++;
			mp_mutex_spinunlock(&ub->u_lock, s);
			return EAGAIN;
		}

		upst->pst_count++;
		ASSERT(upst->pst_count == 0);
		mp_mutex_spinunlock(&ub->u_lock, s);

		/*
		 * If the proc is dead, dequeue another one
		 */
		if ((vpr = VPROC_LOOKUP(upst->pst_pid)) == NULL) {
			goto getanother;
		}

		if (arg == 1) {
			/*
			 * recursive, mark new owner
			 */

		  	int idbuf[2];
			idbuf[0] = upst->pst_pid;
			idbuf[1] = upst->pst_tid ? upst->pst_tid : -1;

			if (copyout(&idbuf, (caddr_t) ub->u_handle,
				    sizeof(int)*2)) {
				VPROC_RELE(vpr);
				return EFAULT;
			}
		}

		pollwakeup(&ub->u_poll, POLLIN | POLLRDNORM);
		VPROC_RELE(vpr);

		return 0;

	case UIOCIDADDR:
		/*
		 * Semaphore registration, required for marking ownership
		 * of recursive polled semaphores.
		 */
		ASSERT(ub);
		if (mindev <= ATTACHMINOR)
			return EINVAL;

		if (COPYIN_XLATE((void *)arg, &at, sizeof(at),
				irix5_to_usattach, get_current_abi(), 1))
			return EFAULT;

		ub->u_handle = at.us_handle;
		return 0;

	case UIOCATTACHSEMA:

		if (mindev != ATTACHMINOR)
			return EINVAL;

		if (COPYIN_XLATE((void *)arg, &at, sizeof(at),
				irix5_to_usattach, get_current_abi(), 1))
			return EFAULT;

		if (minor(at.us_dev) >= usnmin ||
		    minor(at.us_dev) <= ATTACHMINOR)
			return EINVAL;

		/* single thread to avoid someone closing it down */
		mraccess(&uslock);
		if ((ub = usmintab[minor(at.us_dev)]) == NULL) {
			error = EINVAL;
		} else {
			struct vfile *fp = ub->u_fp;
			/*
			 * if a process randomly does an ATTACH and
			 * another process is just in the process of
			 * initializing the device, u_fp and the vnode
			 * could be NULL.
			 */
			if (fp == NULL || VF_TO_VNODE(fp) == NULL) {
				error = EINVAL;
				goto err;
			}
			/*
			 * At this point we know that any potential
			 * closers can only be as far as in usclose waiting
			 * for the uslock. However, they have already
			 * decremented f_count and will call VN_RELE
			 * and drop the vnode count as soon as they
			 * return from usclose. We need to understand
			 * whether they are in close or on their way.
			 * We grab the fp lock - if the count is 0
			 * then the close is about to happen and we need
			 * to leave, if not - we grab an extra reference
			 * now, then release fp lock and do the dup.
			 * The dup also adds a reference, at the end we drop
			 * ours by calling vfile_close()
			 */
			s = VFLOCK(fp);
			if (fp->vf_count <= 0) {
				/* already closing */
				VFUNLOCK(fp, s);
				error = EINVAL;
			} else {
				fp->vf_count++;
				VFUNLOCK(fp, s);
				VOP_ACCESS(VF_TO_VNODE(ub->u_fp), VREAD, 
					   cr, error);
				if (!error) {
					error = attachsema(ub->u_fp, rvalp);
					ub->u_handle = at.us_handle;
				}
				/*
				 * if the attachsema fails, then we
				 * might have the last referece to the fp -
				 * thus vfile_close() might end up calling
				 * usclose - we need to have released the uslock
				 */
				mrunlock(&uslock);
				vfile_close(fp);
				goto out;
			}
		}
err:
		mrunlock(&uslock);
out:
		return error;

	case UIOCBLOCK:
	case UIOCABLOCK:
		/*
		 * Used by CPR to enable IRIX 6.4 apps to be stopped and
		 * restarted on IRIX 6.5.
		 *
		 * XXX: This support may be removed for all major releases
		 *      of IRIX following (but not including) 6.5.
		 */
		ASSERT(ub);
		if (mindev <= ATTACHMINOR)
			return EINVAL;
		if (curuthread->ut_pproxy->prxy_shmask & PR_THREADS)
                 	return EPERM;

		pid = current_pid();
		tid = 0; /* threads are not supported by this ioctl */
		if ((upst = devtopstinfo(dev, pid, tid)) == NULL) {
			/* first time */
			upst = getpstinfo(dev, pid, tid);
		}
		break;

	case UIOCUNBLOCK:
	case UIOCAUNBLOCK:
		/*
		 * Used by CPR to enable IRIX 6.4 apps to be stopped and
		 * restarted on IRIX 6.5.
		 *
		 * XXX: This support may be removed for all major releases
		 *      of IRIX following (but not including) 6.5.
		 */
		ASSERT(ub);
		if (mindev <= ATTACHMINOR)
			return EINVAL;

		/*
		 * since we're acting on another proc, prevent them
		 * from exiting until we've completed the operation
		 */
		pid = (pid_t)arg;
		tid = 0; /* threads are not supported by this ioctl */
 
		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;
		VPROC_GET_PROC(vpr, &p);
 
		/*
		 * Which uthread?  EAMBIGUOUS.
		 */
		if (p->p_proxy.prxy_shmask & PR_THREADS) {
			VPROC_RELE(vpr);
			return EPERM;
		}
		
		if ((upst = devtopstinfo(dev, pid, tid)) == NULL)
			upst = getpstinfo(dev, pid, tid);
		break;

	case UIOCGETCOUNT:
		ASSERT(ub);
		if (ub->u_prepost)
			*rvalp = ub->u_prepost;
		else
			*rvalp = 0 - ub->u_waiters;
		break;

	case UIOCGETSEMASTATE:
	{
		/*
		 * Used by CPR to obtain the state of a particular semaphore
		 */
		ussemastate_t *st = (ussemastate_t *) arg;
		ussematidstate_t *pst;
		int i;

	        if (mindev <= ATTACHMINOR)
	                return EINVAL;

	        /* We don't handle 32 bit processes with this ioctl */
	        if (!ABI_IS_64BIT(get_current_abi()))
	                return EINVAL;

		if (useracc((void *)st, sizeof(*st), B_WRITE, 0) != 0)
			return EFAULT;

		if (useracc((void *)st->tidinfo,
			    sizeof(*pst) * st->ntid, B_WRITE, 0) != 0) {
			unuseracc((void *)st, sizeof(*st), B_WRITE);
			return EFAULT;
		}

		pst = st->tidinfo;
		i = 0;

		mraccess(&uslock);

		/* per proc loop */
		for (up = (uplist_t *) uplist.avl_firstino;
		     up;
		     up = (uplist_t *) up->p_node.avl_nextino) {

			/* per proc/sem loop */
			for (ups = (upsinfo_t *) up->p_pslist.avl_firstino;
			     ups;
			     ups = (upsinfo_t *) ups->ps_node.avl_nextino) {

				if (ups->ps_dev != dev)
					continue;

				/* per proc/sem/thread loop */
				for (upst = (upstinfo_t *)
				       ups->ps_pstlist.avl_firstino;
				     upst;
				     upst = (upstinfo_t *)
				       upst->pst_node.avl_nextino) {
	
					if (i < st->ntid) {
						pst[i].count = upst->pst_count;
						pst[i].pid   = upst->pst_pid;
						pst[i].tid   = upst->pst_tid;
					}
					i++;
				}
			}
		}

		mraccunlock(&uslock);

		st->nfilled = i > st->ntid ? st->ntid : i;
		st->nthread = i;
		st->nprepost = ub->u_prepost;

		unuseracc((void *)st->tidinfo, sizeof(*pst) * st->ntid, B_WRITE);
		unuseracc((void *)st, sizeof(*st), B_WRITE);

		break;
	}

	case UIOCSETSEMASTATE:
	{
		/*
		 * Used by CPR to restore the state of a particular semaphore
		 */
		ussemastate_t *st = (ussemastate_t *) arg;
		ussematidstate_t *pst;
		int i;

		if (!_CAP_ABLE(CAP_DEVICE_MGT))
	                return EPERM;
		  
	        if (mindev <= ATTACHMINOR)
	                return EINVAL;

	        /* We don't handle 32 bit processes with this ioctl */
	        if (!ABI_IS_64BIT(get_current_abi()))
	                return EINVAL;

		if (useracc((void *)st, sizeof(*st), B_WRITE, 0) != 0)
			return EFAULT;

		if (st->ntid) {
			if (useracc((void *)st->tidinfo,
				    sizeof(*pst) * st->ntid, B_WRITE, 0) != 0) {
				unuseracc((void *)st, sizeof(*st), B_WRITE);
				return EFAULT;
			}
			pst = st->tidinfo;
		}
		/*
		 * XXX Do we really need the semaphore's spin lock for this
		 * XXX CPR operation?
		 */
		for (i=0; i < st->ntid; i++) {

			pid = pst[i].pid;
			tid = pst[i].tid;

			/*
			 * since we're acting on another proc, prevent them
			 * from exiting until we've completed the operation
			 */
			if ((vpr = VPROC_LOOKUP(pid)) == NULL) {
				unuseracc((void *)st, sizeof(*st), B_WRITE);
				unuseracc((void *)st->tidinfo,
					sizeof(*pst) * st->ntid, B_WRITE);
				return ESRCH;
			}
			if ((upst = devtopstinfo(dev, pid, tid)) == NULL)
				upst = getpstinfo(dev, pid, tid);

			if (pst[i].count < 0) {
				s = mp_mutex_spinlock(&ub->u_lock);
				if (upst->pst_count < 0) {
					/* Already on the queue */
					mp_mutex_spinunlock(&ub->u_lock, s);
					VPROC_RELE(vpr);
					return ERANGE;
				}

				UB_ENQUEUE(ub, upst);
				upst->pst_count--;
				mp_mutex_spinunlock(&ub->u_lock, s);
			}
			VPROC_RELE(vpr);
		}

		if (st->nprepost) {
			s = mp_mutex_spinlock(&ub->u_lock);
			ASSERT(ub->u_qhead == NULL);
			ASSERT(ub->u_qtail == NULL);
			ub->u_prepost = st->nprepost;
			mp_mutex_spinunlock(&ub->u_lock, s);
		}
		unuseracc((void *)st, sizeof(*st), B_WRITE);
		if (st->ntid) {
			unuseracc((void *)st->tidinfo,
				sizeof(*pst) * st->ntid, B_WRITE);
		}
		break;
	}

	default:
		return EINVAL;
	}

	/*
	 * The switch statement below is used by CPR to enable IRIX 6.4
	 * apps to be stopped and restarted on IRIX 6.5.
	 *
	 * XXX: This entire switch statement may be removed for all major
	 *      releases of IRIX following (but not including) 6.5.
	 */
	switch (cmd) {
	case UIOCBLOCK:
		s = mp_mutex_spinlock(&ub->u_lock);
		upst->pst_count--;
		while (upst->pst_count < 0) {
			/* I should be asleep */
			if (mp_sv_wait_sig(&ub->u_wait,
					   (PZERO+1),
					   &ub->u_lock, s) < 0) {
				/* got interrupted */
				s = mp_mutex_spinlock(&ub->u_lock);
				upst->pst_count++;
				mp_mutex_spinunlock(&ub->u_lock, s);
				return EINTR;
			}
			s = mp_mutex_spinlock(&ub->u_lock);
		}
		mp_mutex_spinunlock(&ub->u_lock, s);
		break;

	case UIOCUNBLOCK:
        	s = mp_mutex_spinlock(&ub->u_lock);
		upst->pst_count++;
		if (upst->pst_count == 0) {
			if (wsyncv_proc(&ub->u_wait, p) == 0) {
				/*
				 * not on queue - unlocker may have beat
				 * process waiting to queue - since the
				 * p_count is 0 it won't go to sleep
				 */
				;
			}
		}
		mp_mutex_spinunlock(&ub->u_lock, s);
		VPROC_RELE(vpr); 
		break;

	case UIOCABLOCK:        /* async block */
		s = mp_mutex_spinlock(&ub->u_lock);
		upst->pst_count--;
		mp_mutex_spinunlock(&ub->u_lock, s);
		break;

	case UIOCAUNBLOCK:      /* async unblock */
		s = mp_mutex_spinlock(&ub->u_lock);
		upst->pst_count++;
		mp_mutex_spinunlock(&ub->u_lock, s);
		pollwakeup(&ub->u_poll, POLLIN | POLLRDNORM);
		VPROC_RELE(vpr);
		break;
	}

	return 0;
}

/* ARGSUSED */
int
uspoll(dev_t dev, short events, int anyyet, short *reventsp,
       struct pollhead ** phpp, unsigned int *genp)
{
	register unsigned revents;
	register upstinfo_t *upst;
	register ublock_t *ub;
	register pid_t pid;
	register int tid;
	unsigned int gen;

	ASSERT(minor(dev) <= usnmin);

	mraccess(&uslock);
	ub = usmintab[minor(dev)];
	mraccunlock(&uslock);

	ASSERT(ub);

	/* snapshot pollhead generation number before checking state */
	gen = POLLGEN(&ub->u_poll);

	pid = current_pid();
	tid = current_tid();

	/*
	 * we only understand POLLIN and POLLRDNORM.
	 */
	if ((revents = (events & (POLLIN|POLLRDNORM))) == 0) {
		goto out;
	}

	/*
	 * Common case, same pid/tid paired with this dev as last time?
	 */
	if (((upst = ub->u_lastups) != NULL) && (pid == upst->pst_pid) &&
	    (tid == upst->pst_tid)) {
		goto cache;
	}

	/*
	 * Take long path, lookup <dev,pid,tid> combination in upstinfo list.
	 */
	if ((upst = devtopstinfo(dev, pid, tid)) == NULL) {
		/*
		 * not really set up and certainly not blocking
		 * But, no reason for error really, the fd is valid.
		 */
		revents = 0;
		goto out;
	}
	ub->u_lastups = upst;

cache:
	if (upst->pst_count < 0) {
		revents = 0;
	}
  
out:
	if (!anyyet) {
		*phpp = &ub->u_poll;
		*genp = gen;
	}
	*reventsp = revents;
	return 0;
}

/*
 * last close on a semaphore - dealloc ublock_t - let the exit routine
 * dealloc per process stuff
 */
int
usclose(dev_t dev)
{
	register uplist_t *up;
	register ublock_t *ub;
	register upsinfo_t *ups;
	register upstinfo_t *upst;
	int	s;

	if (minor(dev) == ATTACHMINOR || minor(dev) == CLONEMINOR)
		return 0;
	ASSERT(minor(dev) <= usnmin);

	/* protect against racing with ATTACHers */
	mrupdate(&uslock);

	ub = usmintab[minor(dev)];
	ASSERT(ub);

	/*
	 * we are called only when we're last close - but
	 * while we were waiting for uslock another process
	 * could have attached. The only way to know whether to
	 * tear down the ub struct is to look if now really, the
	 * file count is one
	 */
	s = VFLOCK(ub->u_fp);
	if (ub->u_fp->vf_count != 0) {
		VFUNLOCK(ub->u_fp, s);
		mrunlock(&uslock);
		return 0;
	}
	VFUNLOCK(ub->u_fp, s);

	usmintab[minor(dev)] = NULL;

	/*
	 * Now find all procs that have this semaphore open...
	 */
	for (up = (uplist_t *)uplist.avl_firstino;
	     up;
	     up = (uplist_t *)up->p_node.avl_nextino) {

		/* Remove the process reference structure */

		if (ups = (upsinfo_t *)
				avl_find(&up->p_pslist, (__psunsigned_t) dev)) {
			avl_delete(&up->p_pslist, (avlnode_t *)ups);

			/* As well as, all thread reference structures */

			while (upst = (upstinfo_t*)
			       avl_firstino(&ups->ps_pstlist)) {
				avl_delete(&ups->ps_pstlist, (avlnode_t *)upst);
				kmem_free(upst, sizeof(*upst));
			}
			kmem_free(ups, sizeof(*ups));
		}
	}

	mrunlock(&uslock);

	sv_destroy(&ub->u_wait);
	spinlock_destroy(&ub->u_lock);
	destroypollhead(&ub->u_poll);
	kern_free(ub);

	return 0;
}

/*
 * called on exit of a process
 */
void
usexit(void)
{
	register uplist_t *up;
	register upsinfo_t *ups, *nups;
	register upstinfo_t *upst;
	register pid_t pid = current_pid();
	register ublock_t *ub;
	int s;

	/*
	 * Peek in access mode first so we don't hang up everyone.
	 */
	mraccess(&uslock);
	up = (uplist_t *) avl_find(&uplist, (__psunsigned_t) pid);
	mraccunlock(&uslock);

	if (up == NULL)
		return;

	mrupdate(&uslock);
	if (up = (uplist_t *) avl_find(&uplist, (__psunsigned_t) pid))
		avl_delete(&uplist, (avlnode_t *)up);

	if (up == NULL) {
		mrunlock(&uslock);
		return;
	}

	/*
	 * Drop from UPDATE to ACCESS mode to prevent the usmintab
	 * table from being grown by getsema while we're accessing it.
	 */
	mrdemote(&uslock);

	for (ups = (upsinfo_t *) up->p_pslist.avl_firstino; ups; ups = nups) {
	  	nups = (upsinfo_t *)ups->ps_node.avl_nextino;
		if ((ub = usmintab[minor(ups->ps_dev)]) != NULL) {
			ub->u_lastups = NULL;

			/*
			 * Remove all thread reference structures
			 */
			while (upst= (upstinfo_t*)
			       avl_firstino(&ups->ps_pstlist)){
				avl_delete(&ups->ps_pstlist, (avlnode_t *)upst);
				if (upst->pst_count < 0) {
					/*
					 * Remove from semaphore wait queue
					 */
					s = mp_mutex_spinlock(&ub->u_lock);
					UB_RMQUEUE(ub, upst);
					mp_mutex_spinunlock(&ub->u_lock, s);
				}
				kmem_free(upst, sizeof(*upst));
			}
		}
		kmem_free(ups, sizeof(*ups));
	}

	mraccunlock(&uslock);

	kmem_free(up, sizeof(*up));
}

#ifdef DEBUG
/*
 * dump list of minors
 */
/* ARGSUSED */
static void
dumpub(int x)
{
	register int i;
	register ublock_t *ub;
	struct upstinfo_s *qscan;

	for (i = 2; i < usnmin; i++) {
		if ((ub = usmintab[i]) == NULL)
			continue;
		qprintf(
		 "Minor %d &wait 0x%x &lock 0x%x fp 0x%x poll 0x%x pp %d\n",
			i, &ub->u_wait, &ub->u_lock, ub->u_fp,
			&ub->u_poll, ub->u_prepost);
		qprintf("\thandle 0x%x qhead 0x%x qtail 0x%x\n",
			ub->u_handle, ub->u_qhead, ub->u_qtail);

		if (qscan = ub->u_qhead) {
			qprintf("\tQ:");
			while (qscan) {
				qprintf(" (%d:%d upst 0x%x)",
					qscan->pst_pid, qscan->pst_tid, qscan);
				qscan = qscan->pst_next;
			}
			qprintf("\n");	
		}
	}
}

/*
 * dump list of procs & threads
 */
/* ARGSUSED */
static void
dumpup(int x)
{
	register uplist_t *up;
	register upsinfo_t *ups;
	register upstinfo_t *upst;
	register int i;

	/* per proc loop */
	for (up = (uplist_t *) uplist.avl_firstino;
	     up;
	     up = (uplist_t *) up->p_node.avl_nextino) {
		i = 0;

		/* per proc/sem loop (queued) */
		qprintf("Pid %d\n\tQueued upstinfo structures:\n", up->p_pid);
		for (ups = (upsinfo_t *) up->p_pslist.avl_firstino;
		     ups;
		     ups = (upsinfo_t *) ups->ps_node.avl_nextino) {

			/* per proc/sem/thread loop */
			for (upst = (upstinfo_t *) ups->ps_pstlist.avl_firstino;
			     upst;
			     upst = (upstinfo_t *) upst->pst_node.avl_nextino) {

				if (upst->pst_count == 0)
					continue;

				if (upst->pst_count != -1) {
				  	qprintf("\t(tid %d minor %d COUNT %d)",
						upst->pst_tid,
						minor(upst->pst_dev),
						upst->pst_count);
				} else {
				  	qprintf("\t(tid %d minor %d)",
						upst->pst_tid,
						minor(upst->pst_dev));
				}

				if ((++i % 4) == 0)
					qprintf("\n");
			}
		}

		if ((i % 4) != 0)
			qprintf("\n");
		i = 0;

		/* per proc/sem loop (dequeued) */
		qprintf("\tDequeued upstinfo structures:\n", up->p_pid);
		for (ups = (upsinfo_t *) up->p_pslist.avl_firstino;
		     ups;
		     ups = (upsinfo_t *) ups->ps_node.avl_nextino) {

			/* per proc/sem/thread loop */
			for (upst = (upstinfo_t *) ups->ps_pstlist.avl_firstino;
			     upst;
			     upst = (upstinfo_t *) upst->pst_node.avl_nextino) {

				if (upst->pst_count != 0)
					continue;

				qprintf("\t(tid %d minor %d)",
					upst->pst_tid, minor(upst->pst_dev));
				if ((++i % 4) == 0)
					qprintf("\n");
			}
		}

		if ((i % 3) != 0)
			qprintf("\n");
	}
}
#endif

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
int
irix5_to_usattach(
	enum xlate_mode mode,
	void *to,
	int count,
	register xlate_info_t *info)
{
	usattach_t	 *up;
	irix5_usattach_t *i5_up;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(irix5_usattach_t) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(sizeof(irix5_usattach_t));
		info->copysize = sizeof(irix5_usattach_t);
		return 0;
	}

	ASSERT(info->copysize == sizeof(irix5_usattach_t));
	ASSERT(info->copybuf != NULL);

	up = (usattach_t *)to;
	i5_up = (irix5_usattach_t *)info->copybuf;

	up->us_dev = (dev_t)i5_up->us_dev;
	up->us_handle = (void *)(__psint_t)i5_up->us_handle;

	return 0;
}
#endif /* _ABI64 */

