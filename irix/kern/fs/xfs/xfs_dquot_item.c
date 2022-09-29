#ident "$Revision: 1.9 $"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/uuid.h>
#include <sys/kmem.h>
#ifdef DEBUG
#include <sys/debug.h>
#include <sys/kabi.h>
#endif
#include <sys/errno.h>
#include <sys/vfs.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>

#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_ialloc.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_btree.h"
#include "xfs_bmap.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_da_btree.h"
#include "xfs_inode.h"
#include "xfs_trans_priv.h"
#include "xfs_error.h"
#include "xfs_quota.h"
#include "xfs_dqblk.h"
#include "xfs_dquot_item.h"
#include "xfs_dquot.h"
#include "xfs_qm.h"
#include "xfs_quota_priv.h"
#include "xfs_rw.h"

STATIC uint		xfs_qm_dquot_logitem_size(xfs_dq_logitem_t *logitem);
STATIC void		xfs_qm_dquot_logitem_format(xfs_dq_logitem_t *logitem,
					      xfs_log_iovec_t *logvec);
STATIC void		xfs_qm_dquot_logitem_unpin(xfs_dq_logitem_t *logitem);
STATIC void		xfs_qm_dquot_logitem_unpin_remove(
						xfs_dq_logitem_t *logitem,
						xfs_trans_t *tp);
STATIC xfs_lsn_t 	xfs_qm_dquot_logitem_committed(xfs_dq_logitem_t *logitem, 
						 xfs_lsn_t lsn);
STATIC void		xfs_qm_dquot_logitem_push(xfs_dq_logitem_t *logitem);


STATIC uint		xfs_qm_qoff_logitem_size(xfs_qoff_logitem_t *logitem);
STATIC void		xfs_qm_qoff_logitem_format(xfs_qoff_logitem_t *logitem,
					      xfs_log_iovec_t *logvec);
STATIC void		xfs_qm_qoff_logitem_unpin(xfs_qoff_logitem_t *logitem);
STATIC void		xfs_qm_qoff_logitem_unpin_remove(
						xfs_qoff_logitem_t *logitem,
						xfs_trans_t *tp);
STATIC xfs_lsn_t 	xfs_qm_qoff_logitem_committed(xfs_qoff_logitem_t *logitem, 
						 xfs_lsn_t lsn);
STATIC void		xfs_qm_qoff_logitem_push(xfs_qoff_logitem_t *logitem);

STATIC xfs_lsn_t	xfs_qm_qoffend_logitem_committed(xfs_qoff_logitem_t *qfe,
							 xfs_lsn_t lsn);

/*
 * returns the number of iovecs needed to log the given dquot item.
 */
/* ARGSUSED */
STATIC uint
xfs_qm_dquot_logitem_size(
	xfs_dq_logitem_t	*logitem)
{
	/* 
	 * we need only two iovecs, one for the format, one for the real thing 
	 */
	return (2);
}

/*
 * fills in the vector of log iovecs for the given dquot log item.
 */
STATIC void
xfs_qm_dquot_logitem_format(
	xfs_dq_logitem_t	*logitem,
	xfs_log_iovec_t		*logvec)
{
	ASSERT(logitem);
	ASSERT(logitem->qli_dquot);

	logvec->i_addr = (caddr_t)&logitem->qli_format;
	logvec->i_len  = sizeof(xfs_dq_logformat_t);
	logvec++;
	logvec->i_addr = (caddr_t)&logitem->qli_dquot->q_core;
	logvec->i_len  = sizeof(xfs_disk_dquot_t);

	ASSERT(2 == logitem->qli_item.li_desc->lid_size);
	logitem->qli_format.qlf_size = 2;
	
}

/*
 * Increment the pin count of the given dquot.
 * This value is protected by pinlock spinlock in the xQM structure.
 */
STATIC void
xfs_qm_dquot_logitem_pin(
	xfs_dq_logitem_t *logitem)
{
	int 	s;
	xfs_dquot_t *dqp;
	
	dqp = logitem->qli_dquot;
	ASSERT(XFS_DQ_IS_LOCKED(dqp));
	s = XFS_DQ_PINLOCK(dqp);
	dqp->q_pincount++;
	XFS_DQ_PINUNLOCK(dqp, s);
}

/*
 * Decrement the pin count of the given dquot, and wake up
 * anyone in xfs_dqwait_unpin() if the count goes to 0.  The
 * dquot must have been previously pinned with a call to xfs_dqpin().
 */
STATIC void
xfs_qm_dquot_logitem_unpin(
	xfs_dq_logitem_t *logitem)
{
	int 	s;
	xfs_dquot_t *dqp;

	dqp = logitem->qli_dquot;
	ASSERT(dqp->q_pincount > 0);
	s = XFS_DQ_PINLOCK(dqp);
	dqp->q_pincount--;
	if (dqp->q_pincount == 0) {
		sv_broadcast(&dqp->q_pinwait);
	}
	XFS_DQ_PINUNLOCK(dqp, s);
}

/* ARGSUSED */
STATIC void
xfs_qm_dquot_logitem_unpin_remove(
	xfs_dq_logitem_t *logitem,
	xfs_trans_t	 *tp)
{
	xfs_qm_dquot_logitem_unpin(logitem);

	return;
}

/*
 * Given the logitem, this writes the corresponding dquot entry to disk
 * asynchronously. This is called with the dquot entry securely locked;
 * we simply get xfs_qm_dqflush() to do the work, and unlock the dquot
 * at the end.
 */
STATIC void
xfs_qm_dquot_logitem_push(
	xfs_dq_logitem_t	*logitem)
{
	xfs_dquot_t	*dqp;

	dqp = logitem->qli_dquot;
	
	ASSERT(XFS_DQ_IS_LOCKED(dqp));
	ASSERT(XFS_DQ_IS_FLUSH_LOCKED(dqp));  
     
        /*
         * Since we were able to lock the dquot's flush lock and
         * we found it on the AIL, the dquot must be dirty.  This
         * is because the dquot is removed from the AIL while still
         * holding the flush lock in xfs_dqflush_done().  Thus, if
         * we found it in the AIL and were able to obtain the flush
         * lock without sleeping, then there must not have been
         * anyone in the process of flushing the dquot.
         */
	xfs_qm_dqflush(dqp, B_DELWRI);
	xfs_dqunlock(dqp);

}

/*ARGSUSED*/
STATIC xfs_lsn_t
xfs_qm_dquot_logitem_committed(
	xfs_dq_logitem_t	*l,
	xfs_lsn_t		lsn)
{
	/*
	 * We always re-log the entire dquot when it becomes dirty,
	 * so, the latest copy _is_ the only one that matters.
	 */
	return (lsn);
}


/*
 * This is called to wait for the given dquot to be unpinned.
 * Most of these pin/unpin routines are plagiarized from inode code.
 */
void
xfs_qm_dqunpin_wait(
	xfs_dquot_t	*dqp)
{
	int             s;
	
	ASSERT(XFS_DQ_IS_LOCKED(dqp));
	if (dqp->q_pincount == 0) {
		return;
	}

	/*
	 * Give the log a push so we don't wait here too long.
	 */
	xfs_log_force(dqp->q_mount, (xfs_lsn_t)0, XFS_LOG_FORCE);
	s = XFS_DQ_PINLOCK(dqp);
	if (dqp->q_pincount == 0) {
		XFS_DQ_PINUNLOCK(dqp, s);
		return;
	}
	sv_wait(&(dqp->q_pinwait), PINOD,
		&(XFS_DQ_TO_QINF(dqp)->qi_pinlock), s);
	return;
}

/*
 * This is called when IOP_TRYLOCK returns XFS_ITEM_PUSHBUF to indicate that
 * the dquot is locked by us, but the flush lock isn't. So, here we are
 * going to see if the relevant dquot buffer is incore, waiting on DELWRI.
 * If so, we want to push it out to help us take this item off the AIL as soon
 * as possible.
 *
 * We must not be holding the AIL_LOCK at this point. Calling incore() to
 * search the buffercache can be a time consuming thing, and AIL_LOCK is a
 * spinlock.
 */
STATIC void
xfs_qm_dquot_logitem_pushbuf(
        xfs_dq_logitem_t    *qip)
{
	xfs_dquot_t	*dqp;
	xfs_mount_t	*mp;
        buf_t	 	*bp;
	uint		dopush;

	dqp = qip->qli_dquot;
	ASSERT(XFS_DQ_IS_LOCKED(dqp));

	/*
	 * The qli_pushbuf_flag keeps others from
	 * trying to duplicate our effort.
	 */
	ASSERT(qip->qli_pushbuf_flag != 0);
	ASSERT(qip->qli_push_owner == get_thread_id());
	
	/*
	 * If flushlock isn't locked anymore, chances are that the
	 * inode flush completed and the inode was taken off the AIL.
	 * So, just get out.
	 */
	if ((valusema(&(dqp->q_flock)) > 0)  ||
	    ((qip->qli_item.li_flags & XFS_LI_IN_AIL) == 0)) {
		qip->qli_pushbuf_flag = 0;
		xfs_dqunlock(dqp);
	    	return;
	}
	mp = dqp->q_mount;
	bp = incore(mp->m_dev, qip->qli_format.qlf_blkno, 
		    XFS_QI_DQCHUNKLEN(mp),
		    INCORE_TRYLOCK);
	if (bp != NULL) {
		if (bp->b_flags & B_DELWRI) {
			dopush = ((qip->qli_item.li_flags & XFS_LI_IN_AIL) && 
				  (valusema(&(dqp->q_flock)) <= 0));
			qip->qli_pushbuf_flag = 0;
			xfs_dqunlock(dqp);

			if (bp->b_pincount > 0) {
				xfs_log_force(mp, (xfs_lsn_t)0,
					      XFS_LOG_FORCE);
			}
			if (dopush) {
#ifdef XFSRACEDEBUG
				delay_for_intr();
				delay(300);
#endif				
				xfs_bawrite(mp, bp);
			} else {
				brelse(bp);
			}
		} else {
			qip->qli_pushbuf_flag = 0;
			xfs_dqunlock(dqp);		
			brelse(bp);
		}
		return;
	}

	qip->qli_pushbuf_flag = 0;
	xfs_dqunlock(dqp);	
	return;
}

/*
 * This is called to attempt to lock the dquot associated with this
 * dquot log item.  Don't sleep on the dquot lock or the flush lock.
 * If the flush lock is already held, indicating that the dquot has
 * been or is in the process of being flushed, then see if we can
 * find the dquot's buffer in the buffer cache without sleeping.  If
 * we can and it is marked delayed write, then we want to send it out.
 * We delay doing so until the push routine, though, to avoid sleeping
 * in any device strategy routines.
 */
STATIC uint
xfs_qm_dquot_logitem_trylock(
        xfs_dq_logitem_t    *qip)
{
	xfs_dquot_t	        *dqp;
	uint			retval;

	dqp = qip->qli_dquot;
	if (dqp->q_pincount > 0)
		return (XFS_ITEM_PINNED);
	
	if (! xfs_qm_dqlock_nowait(dqp))
		return (XFS_ITEM_LOCKED);
	
	retval = XFS_ITEM_SUCCESS;
	if (! xfs_qm_dqflock_nowait(dqp)) {
		/* 
                 * The dquot is already being flushed.  It may have been
                 * flushed delayed write, however, and we don't want to
                 * get stuck waiting for that to complete.  So, we want to check
                 * to see if we can lock the dquot's buffer without sleeping.
                 * If we can and it is marked for delayed write, then we
                 * hold it and send it out from the push routine.  We don't
                 * want to do that now since we might sleep in the device
                 * strategy routine.  We also don't want to grab the buffer lock
		 * here because we'd like not to call into the buffer cache while
		 * holding the AIL_LOCK.
		 * Make sure to only return PUSHBUF if we set pushbuf_flag
		 * ourselves.  If someone else is doing it then we don't 
		 * want to go to the push routine and duplicate their efforts.
                 */
                if (qip->qli_pushbuf_flag == 0) {
			qip->qli_pushbuf_flag = 1;
			ASSERT(qip->qli_format.qlf_blkno == dqp->q_blkno);
#ifdef DEBUG
			qip->qli_push_owner = get_thread_id();
#endif
			/*
			 * The dquot is left locked.
			 */
			retval = XFS_ITEM_PUSHBUF;
		} else {
			retval = XFS_ITEM_FLUSHING;
			xfs_dqunlock_nonotify(dqp);
		}
	}
	
	ASSERT(qip->qli_item.li_flags & XFS_LI_IN_AIL);
	return (retval);
}


/*
 * Unlock the dquot associated with the log item.
 * Clear the fields of the dquot and dquot log item that
 * are specific to the current transaction.  If the
 * hold flags is set, do not unlock the dquot.  
 */
STATIC void
xfs_qm_dquot_logitem_unlock(
        xfs_dq_logitem_t    *ql)
{
	xfs_dquot_t	*dqp;

	ASSERT(ql != NULL);
	dqp = ql->qli_dquot;
	ASSERT(XFS_DQ_IS_LOCKED(dqp));

	/*
         * Clear the transaction pointer in the dquot
         */
	dqp->q_transp = NULL;

	/*
	 * dquots are never 'held' from getting unlocked at the end of
	 * a transaction.  Their locking and unlocking is hidden inside the
	 * transaction layer, within trans_commit. Hence, no LI_HOLD flag
	 * for the logitem.
	 */
	xfs_dqunlock(dqp);
}


/*
 * The transaction with the dquot locked has aborted.  The dquot
 * must not be dirty within the transaction.  We simply unlock just
 * as if the transaction had been cancelled.
 */
STATIC void
xfs_qm_dquot_logitem_abort(
        xfs_dq_logitem_t    *ql)
{
        xfs_qm_dquot_logitem_unlock(ql);
        return;
}


/*
 * This is the ops vector for dquots
 */
struct xfs_item_ops xfs_dquot_item_ops = {
	(uint(*)(xfs_log_item_t*))xfs_qm_dquot_logitem_size,
	(void(*)(xfs_log_item_t*, xfs_log_iovec_t*))xfs_qm_dquot_logitem_format,
	(void(*)(xfs_log_item_t*))xfs_qm_dquot_logitem_pin,
	(void(*)(xfs_log_item_t*))xfs_qm_dquot_logitem_unpin,
	(void(*)(xfs_log_item_t*, xfs_trans_t*))
					xfs_qm_dquot_logitem_unpin_remove,
	(uint(*)(xfs_log_item_t*))xfs_qm_dquot_logitem_trylock,
	(void(*)(xfs_log_item_t*))xfs_qm_dquot_logitem_unlock,
	(xfs_lsn_t(*)(xfs_log_item_t*, xfs_lsn_t))xfs_qm_dquot_logitem_committed,
	(void(*)(xfs_log_item_t*))xfs_qm_dquot_logitem_push,
	(void(*)(xfs_log_item_t*))xfs_qm_dquot_logitem_abort,
	(void(*)(xfs_log_item_t*))xfs_qm_dquot_logitem_pushbuf
};

/*
 * Initialize the dquot log item for a newly allocated dquot.
 * The dquot isn't locked at this point, but it isn't on any of the lists
 * either, so we don't care.
 */
void
xfs_qm_dquot_logitem_init(
	struct xfs_dquot *dqp)
{
	xfs_dq_logitem_t  *lp;
	lp = &dqp->q_logitem;

	lp->qli_item.li_type = XFS_LI_DQUOT;
        lp->qli_item.li_ops = &xfs_dquot_item_ops;
        lp->qli_item.li_mountp = dqp->q_mount;
	lp->qli_dquot = dqp;
        lp->qli_format.qlf_type = XFS_LI_DQUOT;
        lp->qli_format.qlf_id = dqp->q_core.d_id;
        lp->qli_format.qlf_blkno = dqp->q_blkno;
        lp->qli_format.qlf_len = 1;
	/*
	 * This is just the offset of this dquot within its buffer
	 * (which is currently 1 FSB and probably won't change).
	 * Hence 32 bits for this offset should be just fine. 
	 * Alternatively, we can store (bufoffset / sizeof(xfs_dqblk_t))
	 * here, and recompute it at recovery time.
	 */
        lp->qli_format.qlf_boffset = (__uint32_t)dqp->q_bufoffset;
}
	
/*------------------  QUOTAOFF LOG ITEMS  -------------------*/

/*
 * This returns the number of iovecs needed to log the given quotaoff item.
 * We only need 1 iovec for an quotaoff item.  It just logs the quotaoff_log_format
 * structure.
 */
/*ARGSUSED*/
STATIC uint
xfs_qm_qoff_logitem_size(xfs_qoff_logitem_t *qf)
{
	return (1);
}

/*
 * This is called to fill in the vector of log iovecs for the
 * given quotaoff log item. We use only 1 iovec, and we point that
 * at the quotaoff_log_format structure embedded in the quotaoff item.
 * It is at this point that we assert that all of the extent
 * slots in the quotaoff item have been filled.
 */
STATIC void
xfs_qm_qoff_logitem_format(xfs_qoff_logitem_t	*qf,
			   xfs_log_iovec_t	*log_vector)
{
	ASSERT(qf->qql_format.qf_type == XFS_LI_QUOTAOFF);

	log_vector->i_addr = (caddr_t)&(qf->qql_format);
	log_vector->i_len = sizeof(xfs_qoff_logitem_t);
	qf->qql_format.qf_size = 1;
}
	

/*
 * Pinning has no meaning for an quotaoff item, so just return.
 */
/*ARGSUSED*/
STATIC void
xfs_qm_qoff_logitem_pin(xfs_qoff_logitem_t *qf)
{
	return;
}


/*
 * Since pinning has no meaning for an quotaoff item, unpinning does
 * not either.
 */
/*ARGSUSED*/
STATIC void
xfs_qm_qoff_logitem_unpin(xfs_qoff_logitem_t *qf)
{
	return;
}

/*ARGSUSED*/
STATIC void
xfs_qm_qoff_logitem_unpin_remove(xfs_qoff_logitem_t *qf, xfs_trans_t *tp)
{
	return;
}

/*
 * Quotaoff items have no locking, so just return success.
 */
/*ARGSUSED*/
STATIC uint
xfs_qm_qoff_logitem_trylock(xfs_qoff_logitem_t *qf)
{
	return XFS_ITEM_LOCKED;
}

/*
 * Quotaoff items have no locking or pushing, so return failure
 * so that the caller doesn't bother with us.
 */
/*ARGSUSED*/
STATIC void
xfs_qm_qoff_logitem_unlock(xfs_qoff_logitem_t *qf)
{
	return;
}

/*
 * The quotaoff-start-item is logged only once and cannot be moved in the log, so
 * simply return the lsn at which it's been logged.  
 */
/*ARGSUSED*/
STATIC xfs_lsn_t
xfs_qm_qoff_logitem_committed(xfs_qoff_logitem_t *qf, xfs_lsn_t lsn)
{
	return (lsn);
}

/*
 * The transaction of which this QUOTAOFF is a part has been aborted.
 * Just clean up after ourselves.
 * Shouldn't this never happen in the case of qoffend logitems? XXX
 */
STATIC void
xfs_qm_qoff_logitem_abort(xfs_qoff_logitem_t *qf)
{
	kmem_free(qf, sizeof(xfs_qoff_logitem_t));
}

/*
 * There isn't much you can do to push on an quotaoff item.  It is simply
 * stuck waiting for the log to be flushed to disk.
 */
/*ARGSUSED*/
STATIC void
xfs_qm_qoff_logitem_push(xfs_qoff_logitem_t *qf)
{
	return;
}


/*ARGSUSED*/
STATIC xfs_lsn_t
xfs_qm_qoffend_logitem_committed(
	xfs_qoff_logitem_t *qfe, 
	xfs_lsn_t lsn)
{
	xfs_qoff_logitem_t 	*qfs;
	SPLDECL(s);
	
	qfs = qfe->qql_start_lip;
	AIL_LOCK(qfs->qql_item.li_mountp,s);
	/*
	 * Delete the qoff-start logitem from the AIL.
	 * xfs_trans_delete_ail() drops the AIL lock.
	 */
	xfs_trans_delete_ail(qfs->qql_item.li_mountp, (xfs_log_item_t *)qfs, s);
	kmem_free(qfs, sizeof(xfs_qoff_logitem_t));
	kmem_free(qfe, sizeof(xfs_qoff_logitem_t));
	return (xfs_lsn_t)-1;
}

struct xfs_item_ops xfs_qm_qoffend_logitem_ops = {
	(uint(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_size,
	(void(*)(xfs_log_item_t*, xfs_log_iovec_t*))xfs_qm_qoff_logitem_format,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_pin,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_unpin,
	(void(*)(xfs_log_item_t*,xfs_trans_t*))xfs_qm_qoff_logitem_unpin_remove,
	(uint(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_trylock,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_unlock,
	(xfs_lsn_t(*)(xfs_log_item_t*, xfs_lsn_t))xfs_qm_qoffend_logitem_committed,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_push,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_abort
};

/*
 * This is the ops vector shared by all quotaoff-start log items.
 */
struct xfs_item_ops xfs_qm_qoff_logitem_ops = {
	(uint(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_size,
	(void(*)(xfs_log_item_t*, xfs_log_iovec_t*))xfs_qm_qoff_logitem_format,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_pin,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_unpin,
	(void(*)(xfs_log_item_t*,xfs_trans_t*))xfs_qm_qoff_logitem_unpin_remove,
	(uint(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_trylock,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_unlock,
	(xfs_lsn_t(*)(xfs_log_item_t*, xfs_lsn_t))xfs_qm_qoff_logitem_committed,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_push,
	(void(*)(xfs_log_item_t*))xfs_qm_qoff_logitem_abort
};

/*
 * Allocate and initialize an quotaoff item of the correct quota type(s).
 */
xfs_qoff_logitem_t *
xfs_qm_qoff_logitem_init(
	struct xfs_mount *mp, 
	xfs_qoff_logitem_t *start,
	uint flags)
{
	xfs_qoff_logitem_t	*qf;

	qf = (xfs_qoff_logitem_t*) kmem_zalloc(sizeof(xfs_qoff_logitem_t), KM_SLEEP);

	qf->qql_item.li_type = XFS_LI_QUOTAOFF;
	if (start) 
		qf->qql_item.li_ops = &xfs_qm_qoffend_logitem_ops;
	else
		qf->qql_item.li_ops = &xfs_qm_qoff_logitem_ops;
	qf->qql_item.li_mountp = mp;
	qf->qql_format.qf_type = XFS_LI_QUOTAOFF;
	qf->qql_format.qf_flags = flags;
	qf->qql_start_lip = start;
	return (qf);
}

