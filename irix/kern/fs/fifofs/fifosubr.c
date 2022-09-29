/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/fifofs/fifosubr.c	1.22"*/
#ident	"$Revision: 1.61 $"

/*
 * The routines defined in this file are supporting routines for FIFOFS
 * file sytem type.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/stream.h>
#include <sys/strmp.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/ddi.h>
#include <fs/fifofs/fifonode.h>

/*
 * The next set of lines define the bit map used to assign
 * unique pipe inode numbers.  The value chosen for FIFOMAXID
 * is arbitrary, but must fit in a short.
 *
 * fifomap   --> bitmap with one bit per pipe ino
 * testid(x) --> is ino x already in use?
 * setid(x)  --> mark ino x as used
 * clearid(x)--> mark ino x as unused
 */

#define FIFOMAXID	8191
#define testid(i)	((fifomap[(i)/NBBY] & (1 << ((i)%NBBY))))
#define setid(i)	((fifomap[(i)/NBBY] |= (1 << ((i)%NBBY))), (fifoids++))
#define clearid(i)	((fifomap[(i)/NBBY] &= ~(1 << ((i)%NBBY))), (fifoids--))

static lock_t fifolock;
static char fifomap[FIFOMAXID/NBBY + 1];
static ushort fifoids;

/*
 * Define global data structures used in the fifo specific code.
 */
mutex_t		fifoallocmon;
struct fifonode	*fifoalloc;
struct zone	*fifozone;
struct zone	*fifolckzone;
sv_t		fifowaitclose;

/*
 * Define routines/data structures within this file.
 */
static struct vfs	*fifovfsp;
dev_t			fifodev;
static bhv_desc_t	fifo_bhv;	/* vfs fifo behavior */

static void		fifoinsert(struct fifonode *);
static struct fifonode	*fifofind(struct vnode *);
static void		fifo_free_release(struct fifonode *);

/*
 * Declare external routines/variables.
 */
extern vnodeops_t fifo_vnodeops;

vfsops_t fifo_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	fs_nosys,	/* mount */
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	fs_nosys,	/* dounmount */
	fs_nosys,	/* umount */
	fs_nosys,	/* root */
	fs_nosys,	/* statvfs */
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_noerr,	/* realvfsops */
	fs_import,	/* import */
	fs_quotactl     /* quotactl */
};

/*
 * Save file system type/index, initialize vfs operations vector, get
 * unique device number for FIFOFS and initialize the FIFOFS hash.
 * Create and initialize a "generic" vfs pointer that will be placed
 * in the v_vfsp field of each pipes vnode.
 * Create global array of semaphores to serialize the closing of the
 * pipe vnode pairs. This array will be used to handle the race condition
 * that previously existed in fifo_close when both ends of a pipe are
 * closed simultaneously.
 */
/* ARGSUSED */
int
fifo_init(vswp, fstype)
	register struct vfssw *vswp;
	int fstype;
{
	register int dev;

	spinlock_init(&fifolock, "fifolck");
	if ((dev = getudev()) == -1) {
		cmn_err(CE_WARN, "fifoinit: can't get unique device number");
		dev = 0;
	}
	fifodev = makedevice(dev, 0);
	mutex_init(&fifoallocmon, MUTEX_DEFAULT, "fifomon");
	sv_init(&fifowaitclose, SV_DEFAULT, "fifowtc");
	fifoalloc = NULL;

	if ((fifovfsp = vfs_allocate_dummy(fstype, fifodev)) == NULL)
		return -1;
	fifozone = kmem_zone_init(sizeof(struct fifonode),"FIFO nodes");
	fifolckzone = kmem_zone_init(sizeof(fifo_lock_t),"FIFO locks");

	vfs_insertbhv(fifovfsp, &fifo_bhv, &fifo_vfsops, &fifo_bhv);
	return 0;
}

/*
 * NOTE: This procedure does NOT allocate the common fifo lock node
 * in the case that you are creating a pipe. This is due to the fact
 * that the fifo lock node is pointed to by both vnodes, one of each end
 * of the pipe, and must be linked together in the pipe creation code
 */
static void
fifo_initlocks(struct fifonode *fnp, long number)
{
	init_sv(&fnp->fn_empty, SV_DEFAULT, "fifoem", number);
	init_sv(&fnp->fn_full, SV_DEFAULT, "fifofu", number);
	init_sv(&fnp->fn_ioctl, SV_DEFAULT, "fifoio", number);
	init_sv(&fnp->fn_rwait, SV_DEFAULT, "frc", number);
	init_sv(&fnp->fn_wwait, SV_DEFAULT, "fwc", number);
	init_sv(&fnp->fn_openwait, SV_DEFAULT, "fow", number);

	/*
	 * In the pipe case the fifo locking node must be handled
	 * at the higher level since it is linked to both fifonodes.
	 * In the regular fifo case we can allocate the locking node
	 * and set its reference count to one.
	 */
	if (!(fnp->fn_flag & ISPIPE)) {
		fnp->fn_lockp = fifo_cr_lock(number);
	}
}

void
fifo_freelocks(struct fifonode *fnp)
{
	sv_destroy(&fnp->fn_empty);
	sv_destroy(&fnp->fn_full);
	sv_destroy(&fnp->fn_ioctl);
	sv_destroy(&fnp->fn_rwait);
	sv_destroy(&fnp->fn_wwait);
	sv_destroy(&fnp->fn_openwait);

	if (fnp->fn_lockp) { /* we have a lock node to potentially free */
		fifo_free_release(fnp);
	}
}

/*
 * Allocate a node which will hold the mutex, trip counters, etc.
 * We also initialize the mutex name with the unique number passed
 * in from the vnode.
 */
fifo_lock_t *
fifo_cr_lock(long number)
{
	fifo_lock_t *flp;

	flp = (fifo_lock_t *)kmem_zone_alloc(fifolckzone, KM_SLEEP);
	if (flp) {
		init_mutex(&flp->fn_lock, MUTEX_DEFAULT, "flk", number);
		flp->fn_refcnt = 1;
	}
	return flp;
}

/*
 * Free the semaphore and release the storage for the lock node.
 */
void
fifo_del_lock(fifo_lock_t *flp)
{
	mutex_destroy(&flp->fn_lock);
	kmem_zone_free(fifolckzone, flp);
	return;
}

/*
 * handle the releasing of the fifo node lock and the storage deallocation
 * of both the fifo lock node and the fifo node. Specifically we decrment
 * the fifo lock node's reference count and if it goes to zero we:
 *
 * 1. perform a vsema operation on the pipe-pair semaphore (always!)
 * 2. free the fifo lock semaphore
 * 3. release the storage for the lock node
 * 4. release the storage for the fifo node
 *
 * NOTE: The caller must hold the common lock semaphore.
 */
static void
fifo_free_release(struct fifonode *fnp)
{
	fifo_lock_t *flp = fnp->fn_lockp;

	ASSERT(flp->fn_refcnt > 0);
	flp->fn_refcnt--;
	fifo_unlock(fnp);

	if (flp->fn_refcnt == 0) { /* unlock and free shared lock */
		mutex_destroy(&flp->fn_lock);
		kmem_zone_free(fifolckzone, flp);
	}
	return;
}

/*
 * Lock a read/write fifonode given the vnode.
 * This must be a procedure as it is called via the VOP_XXX operation
 * in the case of read/write operations prior to calling through the vop
 * switch macro such as VOP_READ/VOP_WRITE.
 */
/* ARGSUSED */
void
fifo_rwlock(
	bhv_desc_t *bdp,
	vrwlock_t write_lock)
{
	register struct fifonode *fnp = BHVTOF(bdp);

	fifo_lock(fnp);
	fnp->fn_flag |= FIFOLOCK;
	return;
}

/*
 * Unlock a read/write fifonode given the vnode.
 * This must be a procedure as it is called via the VOP_XXX operation
 * in the case of read/write operations prior to calling through the vop
 * switch macro such as VOP_READ/VOP_WRITE.
 */
/* ARGSUSED */
void
fifo_rwunlock(
	bhv_desc_t *bdp,
	vrwlock_t write_lock)
{
	register struct fifonode *fnp = BHVTOF(bdp);

	fnp->fn_flag &= ~FIFOLOCK;
	fifo_unlock(fnp);
	return;
}

/*
 * Provide a shadow for a vnode. If vp already has a shadow in the hash list,
 * return its shadow. Otherwise, create a vnode to shadow vp, hash the
 * new vnode and return its pointer to the caller.
 */
struct vnode *
fifovp(struct vnode *vp, struct cred *crp)
{
	register struct fifonode *fnp;
	register struct vnode *newvp;
	struct vattr va;
	vmap_t vmap;
	int rv;
	bhv_desc_t *bdp;

loop:
	mutex_lock(&fifoallocmon, PZERO);

	if ((fnp = fifofind(vp)) == NULL) {
		fnp = kmem_zone_zalloc(fifozone, KM_SLEEP);

		/*
		 * initialize the times from vp.
		 */
		va.va_mask = AT_TIMES;
		VOP_GETATTR(vp, &va, 0, crp, rv);
		if (vp && !rv) {
			fnp->fn_atime = va.va_atime.tv_sec;
			fnp->fn_mtime = va.va_mtime.tv_sec;
			fnp->fn_ctime = va.va_ctime.tv_sec;
		}

		fnp->fn_realvp = vp;
		newvp = vn_alloc(NULL, VFIFO, NODEV);
		bhv_desc_init(FTOBHV(fnp), fnp, newvp, &fifo_vnodeops);
		vn_bhv_insert_initial(VN_BHV_HEAD(newvp), FTOBHV(fnp));
		VN_FLAGSET(newvp, VINACTIVE_TEARDOWN);

		if (vp != NULL) {        /* hold as long as shadow is active */
			VN_HOLD(vp);
			newvp->v_vfsp = vp->v_vfsp;
			newvp->v_rdev = vp->v_rdev;
		}

		fifo_initlocks(fnp, (long)newvp->v_number);
		fifoinsert(fnp);
		mutex_unlock(&fifoallocmon);
	}
	else { /* found it */

		newvp =  FTOV(fnp);
		VMAP(newvp, vmap);

		mutex_unlock(&fifoallocmon);

		if (!(newvp = vn_get(newvp, &vmap, 0)))
			goto loop;

		/*
		 * Check to see if we already disassociated the fifo
		 * behavior from the vnode.  If so, then just release
		 * this guy and start over.
		 */
		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(newvp), 
					     &fifo_vnodeops);
		if (bdp == NULL) {
			VN_RELE(newvp);
			goto loop;
		}
	}
	return newvp;
}

/*
 * Create a pipe end by...
 * allocating a vnode-fifonode pair, intializing the fifonode,
 * setting the ISPIPE flag in the fifonode and assigning a unique
 * ino to the fifonode.
 * NOTE: THe fifo lock node is NOT allocated here as it is a shared
 * node between the two vnodes. It is allocated by the upper level
 * software and linked to both ends of the pipe vnodes.
 */
/*ARGSUSED*/
struct fifonode *
makepipe(int flag)
{
	register struct fifonode *fnp;
	register struct vnode *vp;
	timespec_t tv;

	fnp = kmem_zone_zalloc(fifozone, KM_SLEEP);
	nanotime_syscall(&tv);
	fnp->fn_numr = fnp->fn_numw = 1;
	fnp->fn_atime = fnp->fn_mtime = fnp->fn_ctime = tv.tv_sec;
	fnp->fn_flag |= ISPIPE;

	vp = vn_alloc(fifovfsp, VFIFO, fifodev);
	bhv_desc_init(FTOBHV(fnp), fnp, vp, &fifo_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), FTOBHV(fnp));
	VN_FLAGSET(vp, VINACTIVE_TEARDOWN);

	fifo_initlocks(fnp, (long)vp->v_number);
	return fnp;
}

/*
 * Release a pipe-ino.
 */
void
fifoclearid(ushort ino)
{
	int s;

	/*
	 * Safeguard against decrementing fifoids when bit has
	 * already been cleared.
	 */
	s = mp_mutex_spinlock(&fifolock);
	if (testid(ino))
		clearid(ino);
	mp_mutex_spinunlock(&fifolock, s);
}

/*
 * Attempt to establish a unique pipe id. Start searching the bit map where
 * the previous search stopped. If a free bit is located, set the bit and
 * return the new position in the bit map.
 */
ushort
fifogetid(void)
{
	int s;
	register ushort i;
	register ushort j;
	static ushort prev = 0;

	s = mp_mutex_spinlock(&fifolock);
	i = prev;

	for (j = FIFOMAXID; j--; ) {
		if (i++ >= (ushort)FIFOMAXID)
			i = 1;

		if (!testid(i)) {
			setid(i);
			prev = i;
			mp_mutex_spinunlock(&fifolock, s);
			return i;
		}
	}

	mp_mutex_spinunlock(&fifolock, s);
#if 0
	/*
	 * This currently costs us too much.  We'll be trying to rework the
	 * performance problem caused by the fifo inode number allocation
	 * scheme in bug #551505, ``pipe inode number generation is slow and
	 * complex.''
	 */
	cmn_err(CE_WARN, "fifogetid: could not establish a unique node id\n");
#endif
	return 0;
}

/*
 * Stream a pipe/FIFO.
 * The FIFOPASS flag is used when CONNLD is pushed on the stream.
 * If the flag is set, a new vnode is being passed to the upper
 * layer file system as the vnode representing an open request.
 * In that case, this process will sleep until the FIFOPASS flag
 * has been turned off.
 *
 * After returning from stropen, if the FIFOPASS flag has been set,
 * CONNLD is on the pipe and has placed a new vnode in the
 * fn_unique field of the fifonode. In that case, return the new
 * vnode to the upper layer and release the current vnode.
 */
int
fifo_stropen(
	struct vnode **vpp,
	int flag,
	struct cred *crp)
{
	register int error = 0;
	register struct vnode *vp = *vpp;
	struct stdata *stp;
	struct queue *wqp;
	dev_t pdev = 0;
#ifdef DEBUG
	struct fifonode *fnp;
	bhv_desc_t *bdp;

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &fifo_vnodeops);
	ASSERT(bdp != NULL);
	fnp = BHVTOF(bdp);
#endif
	ASSERT(!(fnp->fn_flag & FIFOPASS));

	if ((error = stropen(vp, &pdev, NULL, flag, crp)) != 0) {
		/* if vnode changes then have locking problem huy */	
		ASSERT(FTOV(fnp) == vp);	
		return error;
	}

	/*
	 * Set up the stream head in order to maintain compatibility.
	 * Check the hi-water, low-water and packet sizes to ensure
	 * the user can at least write PIPE_BUF bytes to the stream
	 * head and that a message at least PIPE_BUF bytes can be
	 * packaged and placed on the stream head's read queue
	 * (atomic writes).
	 */
	STRHEAD_LOCK(&(vp)->v_stream, stp);
	ASSERT(stp);
	stp->sd_flag |= OLDNDELAY;
	wqp = stp->sd_wrq;
	if (wqp->q_hiwat < PIPE_BUF) {
		wqp->q_hiwat = PIPE_BUF;
		RD(wqp)->q_hiwat = PIPE_BUF;
	}
	wqp->q_lowat = PIPE_BUF -1;
	RD(wqp)->q_lowat = PIPE_BUF -1;

	if (wqp->q_minpsz > 0) {
		wqp->q_minpsz = 0;
		RD(wqp)->q_minpsz = 0;
	}
	if (wqp->q_maxpsz < PIPE_BUF) {
		wqp->q_maxpsz = PIPE_BUF;
		RD(wqp)->q_maxpsz = PIPE_BUF;
	}
	STRHEAD_UNLOCK(stp);

	return 0;
}

/*
 * Insert a fifonode-vnode pair onto the fifoalloc hash list.
 * Called inside fifoallocmon.
 */
static void
fifoinsert(struct fifonode *fnp)
{
	fnp->fn_backp = NULL;
	fnp->fn_nextp = fifoalloc;
	fifoalloc = fnp;
	if (fnp->fn_nextp)
		fnp->fn_nextp->fn_backp = fnp;
}

/*
 * Find a fifonode-vnode pair on the fifoalloc hash list.
 * vp is a vnode to be shadowed. If it's on the hash list, it already has
 * a shadow, therefore return its corresponding fifonode.
 *
 * Called inside fifoallocmon to serialize find/insert and delete.
 */
static struct fifonode *
fifofind(struct vnode *vp)
{
	register struct fifonode *fnode;

	for (fnode = fifoalloc;  fnode;  fnode = fnode->fn_nextp)
		if (fnode->fn_realvp == vp) {
			return fnode;
		}
	return NULL;
}

/*
 * Remove a fifonode-vnode pair from the fifoalloc hash list.
 * This routine is called from the fifo_inactive() routine when a
 * FIFO is being released.
 * If the link to be removed is the only link, set fifoalloc to NULL.
 * Called inside fifoallocmon.
 */
void
fiforemove(struct fifonode *fnp)
{
	register struct fifonode *fnode;

	for (fnode = fifoalloc;  fnode;  fnode = fnode->fn_nextp)
		if (fnode == fnp) {
			if (fnode == fifoalloc)
				fifoalloc = fnode->fn_nextp;
			if (fnode->fn_nextp)
				fnode->fn_nextp->fn_backp = fnode->fn_backp;
			if (fnode->fn_backp)
				fnode->fn_backp->fn_nextp = fnode->fn_nextp;
			break;
		}
}

/*
 * Flush "all" messages on qp.
 * If pending PASSFD messages on the queue, close the file.
 * If flow control has been lifted, enable the queues.
 */
void
fifo_flush(register queue_t *qp)
{
	mblk_t *mp, *tmp;
	int wantw;
	queue_t *nq;
	register int s;
	/*REFERENCED*/
	int rtn;

	STRQUEUE_LOCK(qp->q_monpp, rtn);
	ASSERT(rtn);
	STR_LOCK_SPL(s);
	wantw = qp->q_flag & QWANTW;
	mp = qp->q_first;
	qp->q_first = qp->q_last = NULL;
	qp->q_count = 0;
	qp->q_flag &= ~(QFULL | QWANTW);
	STR_UNLOCK_SPL(s);
	while (mp) {
		tmp = mp->b_next;
		if (mp->b_datap->db_type == M_PASSFP) {
			STRQUEUE_UNLOCK(qp->q_monpp);
			vfile_close(((struct strrecvfd *)mp->b_rptr)->f.fp);
			STRQUEUE_LOCK(qp->q_monpp, rtn);
			ASSERT(rtn);
		}
		freemsg(mp);
		mp = tmp;
	}

	/*
	 * Only data messages can be queued on the
	 * stream head read queue.  We just flushed
	 * the queue, so there is no need to check
	 * if q_count < q_lowat.
	 */
	if (wantw) {
		/* find nearest back queue with service proc */
		for (nq = backq(qp);
		     nq && !nq->q_qinfo->qi_srvp;
		     nq = backq(nq))
			;
		if (nq)
			qenable(nq);
	}
	STRQUEUE_UNLOCK(qp->q_monpp);
}
