#ident	"$Revision: 1.17 $"
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/uuid.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>	
#include <sys/atomic_ops.h>
#include <sys/systm.h>
#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#endif /* _SHAREII */

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
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_btree.h"
#include "xfs_bmap.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_trans_priv.h"
#include "xfs_buf_item.h"
#include "xfs_quota.h"
#include "xfs_dqblk.h"
#include "xfs_dquot_item.h"
#include "xfs_dquot.h"
#include "xfs_qm.h"
#include "xfs_quota_priv.h"

STATIC
int		xfs_trans_dqresv( xfs_trans_t	*tp,
				 xfs_dquot_t 	*dqp,
				 long  		nblks,
				 long		ninos,
				 uint		flags);

STATIC 
void		xfs_trans_dqlockedjoin(xfs_trans_t *tp, 
				       xfs_dqtrx_t *q);
      		      
/*
 * Add the locked dquot to the transaction.
 * The dquot must be locked, and it cannot be associated with any
 * transaction.  
 */
void
xfs_trans_dqjoin(
        xfs_trans_t     *tp,
	xfs_dquot_t     *dqp)
{
        xfs_dq_logitem_t    *lp;
	
        ASSERT(! XFS_DQ_IS_ADDEDTO_TRX(tp, dqp));
        ASSERT(XFS_DQ_IS_LOCKED(dqp));
        ASSERT(XFS_DQ_IS_LOGITEM_INITD(dqp));
	lp = &dqp->q_logitem;
	
        /*
         * Get a log_item_desc to point at the new item.
         */
        (void) xfs_trans_add_item(tp, (xfs_log_item_t*)(lp));

        /*
         * Initialize i_transp so we can later determine if this dquot is
	 * associated with this transaction.
         */
        dqp->q_transp = tp;
}


/*
 * This is called to mark the dquot as needing
 * to be logged when the transaction is committed.  The dquot must
 * already be associated with the given transaction.
 * Note that it marks the entire transaction as dirty. In the ordinary
 * case, this gets called via xfs_trans_commit, after the transaction
 * is already dirty. However, there's nothing stop this from getting
 * called directly, as done by xfs_qm_scall_setqlim. Hence, the TRANS_DIRTY
 * flag.
 */
void
xfs_trans_log_dquot(
        xfs_trans_t     *tp,
        xfs_dquot_t     *dqp)
{
        xfs_log_item_desc_t     *lidp;

	ASSERT(XFS_DQ_IS_ADDEDTO_TRX(tp, dqp));
        ASSERT(XFS_DQ_IS_LOCKED(dqp));

        lidp = xfs_trans_find_item(tp, (xfs_log_item_t*)(&dqp->q_logitem));
        ASSERT(lidp != NULL);

        tp->t_flags |= XFS_TRANS_DIRTY;
        lidp->lid_flags |= XFS_LID_DIRTY;
}

/*
 * Carry forward whatever is left of the quota blk reservation to
 * the spanky new transaction
 */
void
xfs_trans_dup_dqinfo(
	xfs_trans_t	*otp, 
	xfs_trans_t	*ntp)
{
	xfs_dqtrx_t 	*oq, *nq;
	int		i,j;
	xfs_dqtrx_t 	*oqa, *nqa;

	xfs_trans_alloc_dqinfo(ntp);
	oqa = otp->t_dqinfo->dqa_usrdquots;
	nqa = ntp->t_dqinfo->dqa_usrdquots;
	for (j = 0; j < 2; j++) {
		for (i = 0; i < XFS_QM_TRANS_MAXDQS; i++) {
			if (oqa[i].qt_dquot == NULL)
				break;
			oq = &oqa[i];
			nq = &nqa[i];
		
			nq->qt_dquot = oq->qt_dquot;
			nq->qt_bcount_delta = nq->qt_icount_delta = 0;
			nq->qt_rtbcount_delta = 0;

			/*
			 * Transfer whatever is left of the reservations.
			 */
			nq->qt_blk_res = oq->qt_blk_res - oq->qt_blk_res_used;
			oq->qt_blk_res = oq->qt_blk_res_used;

			nq->qt_rtblk_res = oq->qt_rtblk_res - 
				oq->qt_rtblk_res_used;
			oq->qt_rtblk_res = oq->qt_rtblk_res_used;

			nq->qt_ino_res = oq->qt_ino_res - oq->qt_ino_res_used;
			oq->qt_ino_res = oq->qt_ino_res_used;
			
		}
		oqa = otp->t_dqinfo->dqa_prjdquots;
		nqa = ntp->t_dqinfo->dqa_prjdquots;
	}
}

/*
 * Wrap around mod_dquot to account for both user and proj quotas.
 */
int
xfs_trans_mod_dquot_byino(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip,
	uint		field,
	long		delta)
{
	ASSERT(tp);

	if (tp->t_dqinfo == NULL)
		xfs_trans_alloc_dqinfo(tp);

	if (XFS_IS_UQUOTA_ON(tp->t_mountp) && ip->i_udquot) {
		(void) xfs_trans_mod_dquot(tp, ip->i_udquot, field, delta);
	}
	if (XFS_IS_PQUOTA_ON(tp->t_mountp) && ip->i_pdquot) {
		(void) xfs_trans_mod_dquot(tp, ip->i_pdquot, field, delta);
	}
	return (0);
}	

STATIC xfs_dqtrx_t *
xfs_trans_get_dqtrx(
	xfs_trans_t	*tp,
	xfs_dquot_t	*dqp)
{
	int 		i;
	xfs_dqtrx_t 	*qa;

	for (i = 0; i < XFS_QM_TRANS_MAXDQS; i++) {
		qa = XFS_QM_DQP_TO_DQACCT(tp, dqp);

		if (qa[i].qt_dquot == NULL || 
		    qa[i].qt_dquot == dqp) {
			return (&qa[i]);
		}
	}

	return (NULL);
}

/*
 * Make the changes in the transaction structure.
 * The moral equivalent to xfs_trans_mod_sb().
 * We don't touch any fields in the dquot, so we don't care
 * if it's locked or not (most of the time it won't be).
 */
void
xfs_trans_mod_dquot(
	xfs_trans_t 	*tp,
	xfs_dquot_t 	*dqp,
	uint		field,
	long	  	delta)
{
	xfs_dqtrx_t 	*qtrx;

	ASSERT(tp);
	qtrx = NULL;

	if (tp->t_dqinfo == NULL)
		xfs_trans_alloc_dqinfo(tp);
	/*
	 * Find either the first free slot or the slot that belongs
	 * to this dquot.
	 */
	qtrx = xfs_trans_get_dqtrx(tp, dqp);
	ASSERT(qtrx);
	if (qtrx->qt_dquot == NULL)
		qtrx->qt_dquot = dqp;

	switch (field) {

		/*
		 * regular disk blk reservation 
		 */
	      case XFS_TRANS_DQ_RES_BLKS:
		qtrx->qt_blk_res += (ulong)delta;
		break;
		
		/*
		 * inode reservation 
		 */
	      case XFS_TRANS_DQ_RES_INOS:
		qtrx->qt_ino_res += (ulong)delta;
		break;

		/* 
		 * disk blocks used.
		 */
	      case XFS_TRANS_DQ_BCOUNT:
		if (qtrx->qt_blk_res && delta > 0) {
			qtrx->qt_blk_res_used += (ulong)delta;
			ASSERT(qtrx->qt_blk_res >= qtrx->qt_blk_res_used);
		}
		qtrx->qt_bcount_delta += delta;
		break;
	      
	      case XFS_TRANS_DQ_DELBCOUNT:
		qtrx->qt_delbcnt_delta += delta;
		break;

		/* 
		 * Inode Count
		 */
	      case XFS_TRANS_DQ_ICOUNT:
		if (qtrx->qt_ino_res && delta > 0) {
			qtrx->qt_ino_res_used += (ulong)delta;
			ASSERT(qtrx->qt_ino_res >= qtrx->qt_ino_res_used);
		}
		qtrx->qt_icount_delta += delta;
		break;

		/*
		 * rtblk reservation 
		 */
	      case XFS_TRANS_DQ_RES_RTBLKS:
		qtrx->qt_rtblk_res += (ulong)delta;
		break;

		/* 
		 * rtblk count 
		 */	
	      case XFS_TRANS_DQ_RTBCOUNT:
		if (qtrx->qt_rtblk_res && delta > 0) {
			qtrx->qt_rtblk_res_used += (ulong)delta;
			ASSERT(qtrx->qt_rtblk_res >= qtrx->qt_rtblk_res_used);
		}
		qtrx->qt_rtbcount_delta += delta;
		break;
		  
	      case XFS_TRANS_DQ_DELRTBCOUNT:
		qtrx->qt_delrtb_delta += delta;
		break;

	      default:
		ASSERT(0);
	}
	tp->t_flags |= XFS_TRANS_DQ_DIRTY;
}


/*
 * Given an array of dqtrx structures, lock all the dquots associated
 * and join them to the transaction, provided they have been modified.
 * We know that the highest number of dquots (of one type - usr OR prj),
 * involved in a transaction is 2 and that both usr and prj combined - 3.
 * So, we don't attempt to make this very generic.
 */
STATIC void
xfs_trans_dqlockedjoin(
	xfs_trans_t  	*tp,
	xfs_dqtrx_t  	*q)
{
	ASSERT(q[0].qt_dquot != NULL);
	if (q[1].qt_dquot == NULL) {
		xfs_dqlock(q[0].qt_dquot);
		xfs_trans_dqjoin(tp, q[0].qt_dquot);
	} else {
		ASSERT(XFS_QM_TRANS_MAXDQS == 2);
		xfs_dqlock2(q[0].qt_dquot, q[1].qt_dquot);
		xfs_trans_dqjoin(tp, q[0].qt_dquot);
		xfs_trans_dqjoin(tp, q[1].qt_dquot);
	}
}



/*
 * Called by xfs_trans_commit() and similar in spirit to 
 * xfs_trans_apply_sb_deltas().
 * Go thru all the dquots belonging to this transaction and modify the
 * INCORE dquot to reflect the actual usages.
 * Unreserve just the reservations done by this transaction
 * dquot is still left locked at exit.
 */
void
xfs_trans_apply_dquot_deltas(
	xfs_trans_t 		*tp)
{
	int 			i, j;
	xfs_dquot_t 		*dqp;
	xfs_dqtrx_t 		*qtrx, *qa;
	xfs_disk_dquot_t	*d;
	long			totalbdelta;
	long			totalrtbdelta;
#ifdef _SHAREII
	xfs_qcnt_t		oldresbcnt;
	xfs_qcnt_t		oldresicnt;
#endif /* _SHAREII */
	ASSERT(tp->t_dqinfo);

	qa = tp->t_dqinfo->dqa_usrdquots;
	for (j = 0; j < 2; j++) {
		if (qa[0].qt_dquot == NULL) {
			qa = tp->t_dqinfo->dqa_prjdquots;
			continue;
		}

		/*
		 * Lock all of the dquots and join them to the transaction.
		 */
		xfs_trans_dqlockedjoin(tp, qa);

		for (i = 0; i < XFS_QM_TRANS_MAXDQS; i++) {
			qtrx = &qa[i];
			/*
			 * The array of dquots is filled
			 * sequentially, not sparsely.
			 */
			if ((dqp = qtrx->qt_dquot) == NULL)
				break;

			ASSERT(XFS_DQ_IS_LOCKED(dqp));
			ASSERT(XFS_DQ_IS_ADDEDTO_TRX(tp, dqp));
			
			/* 
			 * adjust the actual number of blocks used 
			 */
			d = &dqp->q_core;
			
			/*
			 * The issue here is - sometimes we don't make a blkquota
			 * reservation intentionally to be fair to users 
			 * (when the amount is small). On the other hand,
			 * delayed allocs do make reservations, but that's
			 * outside of a transaction, so we have no 
			 * idea how much was really reserved.
			 * So, here we've accumulated delayed allocation blks and
			 * non-delay blks. The assumption is that the 
			 * delayed ones are always reserved (outside of a 
			 * transaction), and the others may or may not have
			 * quota reservations.
			 */
			totalbdelta = qtrx->qt_bcount_delta + 	
				qtrx->qt_delbcnt_delta;
			totalrtbdelta = qtrx->qt_rtbcount_delta + 	
				qtrx->qt_delrtb_delta;
#ifdef QUOTADEBUG
			if (totalbdelta < 0)
				ASSERT(d->d_bcount >= 
				       (xfs_qcnt_t) -totalbdelta);

			if (totalrtbdelta < 0)
				ASSERT(d->d_rtbcount >= 
				       (xfs_qcnt_t) -totalrtbdelta);

			if (qtrx->qt_icount_delta < 0)
				ASSERT(d->d_icount >= 
				       (xfs_qcnt_t) -qtrx->qt_icount_delta);
#endif
			if (totalbdelta)
				d->d_bcount += (xfs_qcnt_t)totalbdelta;

			if (qtrx->qt_icount_delta)
				d->d_icount += (xfs_qcnt_t)qtrx->qt_icount_delta;

			if (totalrtbdelta)
				d->d_rtbcount += (xfs_qcnt_t)totalrtbdelta;
	
			/*
			 * Start/reset the timer(s) if needed.
			 */
			xfs_qm_adjust_dqtimers(tp->t_mountp, d);

			dqp->dq_flags |= XFS_DQ_DIRTY;
			/* 
			 * add this to the list of items to get logged 
			 */
			xfs_trans_log_dquot(tp, dqp);
#ifdef _SHAREII
			/*
			 * SHAREII doesn't distinguish between RT and reg blks.
			 */
			if (SHR_ACTIVE) {
				oldresbcnt = dqp->q_res_bcount + 
					dqp->q_res_rtbcount;
				oldresicnt = dqp->q_res_icount;
			}
#endif /* _SHAREII */
			/* 
			 * Take off what's left of the original reservation.
			 * In case of delayed allocations, there's no 
			 * reservation that a transaction structure knows of.
			 */
			if (qtrx->qt_blk_res != 0) {
				if (qtrx->qt_blk_res != qtrx->qt_blk_res_used) {
					if (qtrx->qt_blk_res > 
					    qtrx->qt_blk_res_used)
						dqp->q_res_bcount -= (xfs_qcnt_t) 
							(qtrx->qt_blk_res - 
							 qtrx->qt_blk_res_used);
					else
						dqp->q_res_bcount -= (xfs_qcnt_t) 
							(qtrx->qt_blk_res_used - 
							 qtrx->qt_blk_res);
				}
			} else {
				/*
				 * These blks were never reserved, either inside
				 * a transaction or outside one (in a delayed
				 * allocation). Also, this isn't always a 
				 * negative number since we sometimes
				 * deliberately skip quota reservations.
				 */
				if (qtrx->qt_bcount_delta) {
					dqp->q_res_bcount += 
					      (xfs_qcnt_t)qtrx->qt_bcount_delta;
				}
			}
			/*
			 * Adjust the RT reservation.
			 */
			if (qtrx->qt_rtblk_res != 0) { 
				if (qtrx->qt_blk_res != qtrx->qt_blk_res_used) {
					if (qtrx->qt_rtblk_res > 
					    qtrx->qt_rtblk_res_used)
					       dqp->q_res_rtbcount -= (xfs_qcnt_t)
						       (qtrx->qt_rtblk_res - 
							qtrx->qt_rtblk_res_used);
					else
					       dqp->q_res_rtbcount -= (xfs_qcnt_t)
						       (qtrx->qt_rtblk_res_used - 
							qtrx->qt_rtblk_res);
				}
			} else {
				if (qtrx->qt_rtbcount_delta)
					dqp->q_res_rtbcount += 
					    (xfs_qcnt_t)qtrx->qt_rtbcount_delta;
			}
			
			/*
			 * Adjust the inode reservation.
			 */
			if (qtrx->qt_ino_res != 0) { 
				ASSERT(qtrx->qt_ino_res >= 
				       qtrx->qt_ino_res_used);
				if (qtrx->qt_ino_res > qtrx->qt_ino_res_used)
					dqp->q_res_icount -= (xfs_qcnt_t)
						(qtrx->qt_ino_res - 
						 qtrx->qt_ino_res_used);
			} else {
				if (qtrx->qt_icount_delta)
					dqp->q_res_icount += 
					    (xfs_qcnt_t)qtrx->qt_icount_delta;
			}


#ifdef QUOTADEBUG
			if (qtrx->qt_rtblk_res != 0)
				printf("RT res %d for 0x%x\n",
				      (int) qtrx->qt_rtblk_res,
				      dqp);
#endif
			ASSERT(dqp->q_res_bcount >= dqp->q_core.d_bcount);
			ASSERT(dqp->q_res_icount >= dqp->q_core.d_icount);
			ASSERT(dqp->q_res_rtbcount >= dqp->q_core.d_rtbcount);
#ifdef _SHAREII
			if (SHR_ACTIVE) {
				long x = dqp->q_res_bcount + dqp->q_res_rtbcount -
				    oldresbcnt;
				if (x)
				{
				      (void) SHR_LIMITDISK(XFS_MTOVFS(dqp->q_mount),
						   dqp->q_core.d_id,
						   (u_long)(x > 0 ? x : -x),
						   XFS_FSB_TO_B(dqp->q_mount, 1),
						   (x > 0 ? LI_ALLOC : LI_FREE)|LI_UPDATE, NULL, NULL);
			        }
				x = dqp->q_res_icount - oldresicnt;
				if (x)
				{
				     (void) SHR_LIMITDISK(XFS_MTOVFS(dqp->q_mount),
							dqp->q_core.d_id,
							(u_long)(x > 0 ? x : -x),
							0, 
							(x > 0 ? LI_ALLOC : LI_FREE)|LI_UPDATE, NULL, NULL);
				}
			}	
#endif /* _SHAREII */
		}
		/*
		 * Do the project quotas next
		 */
		qa = tp->t_dqinfo->dqa_prjdquots;
	}
}

/*
 * Release the reservations, and adjust the dquots accordingly. 
 * This is called only when the transaction is being aborted. If by
 * any chance we have done dquot modifications incore (ie. deltas) already,
 * we simply throw those away, since that's the expected behavior
 * when a transaction is curtailed without a commit.
 */
void
xfs_trans_unreserve_and_mod_dquots(
	xfs_trans_t	*tp)
{
	int 			i, j;
	xfs_dquot_t 		*dqp;
	xfs_dqtrx_t 		*qtrx, *qa;
	boolean_t		locked;
#ifdef _SHAREII
	int			blkunres = 0;
	int			inounres = 0;
#endif /* _SHAREII */

	ASSERT(tp->t_dqinfo);
	qa = tp->t_dqinfo->dqa_usrdquots;

	for (j = 0; j < 2; j++) {
		for (i = 0; i < XFS_QM_TRANS_MAXDQS; i++) {
			qtrx = &qa[i];
			/*
			 * We assume that the array of dquots is filled
			 * sequentially, not sparsely.
			 */
			if ((dqp = qtrx->qt_dquot) == NULL)
				break;
			/*
			 * Unreserve the original reservation. We don't care
			 * about the number of blocks used field, or deltas.
			 * Also we don't bother to zero the fields.
			 */
			locked = B_FALSE;
			if (qtrx->qt_blk_res) {
				xfs_dqlock(dqp);
				locked = B_TRUE;
				dqp->q_res_bcount -= 
					(xfs_qcnt_t)qtrx->qt_blk_res; 
#ifdef _SHAREII
				if (SHR_ACTIVE)
					blkunres = (int)qtrx->qt_blk_res;
#endif /* _SHAREII */
			}
			if (qtrx->qt_ino_res) {
				if (!locked) {
					xfs_dqlock(dqp);
					locked = B_TRUE;
				}
				dqp->q_res_icount -= 
					(xfs_qcnt_t)qtrx->qt_ino_res; 
#ifdef _SHAREII
				if (SHR_ACTIVE) 
					inounres = (int)qtrx->qt_ino_res;
#endif /* _SHAREII */
			}
			
			if (qtrx->qt_rtblk_res) {
				if (!locked) {
					xfs_dqlock(dqp);
					locked = B_TRUE;
				}
				dqp->q_res_rtbcount -= 
					(xfs_qcnt_t)qtrx->qt_rtblk_res; 
#ifdef _SHAREII
				if (SHR_ACTIVE)
					blkunres += (int)qtrx->qt_rtblk_res;
#endif /* _SHAREII */
			}
#ifdef _SHAREII
			/* Undo the SHAREII reservation. */
			if (SHR_ACTIVE) {
				if (blkunres)
				     (void) SHR_LIMITDISK(XFS_MTOVFS(dqp->q_mount),
						   dqp->q_core.d_id,
						   (u_long) blkunres,
						   XFS_FSB_TO_B(dqp->q_mount, 1),
						   LI_UPDATE|LI_FREE,
						   NULL,
						   NULL);
				if (inounres)
				     (void) SHR_LIMITDISK(XFS_MTOVFS(dqp->q_mount),
							dqp->q_core.d_id,
							(u_long) inounres,
							0, LI_UPDATE|LI_FREE, NULL, NULL);
			}
#endif /* _SHAREII */
			if (locked)
				xfs_dqunlock(dqp);

		}
		qa = tp->t_dqinfo->dqa_prjdquots;
	}
}

/*
 * This reserves disk blocks and inodes against a dquot.
 * Flags indicate if the dquot is to be locked here and also
 * if the blk reservation is for RT or regular blocks.
 * Sending in XFS_QMOPT_FORCE_RES flag skips the quota check.
 * Returns EDQUOT if quota is exceeded. 
 */
STATIC int
xfs_trans_dqresv(
	xfs_trans_t	*tp,
	xfs_dquot_t 	*dqp,
	long		nblks,
	long		ninos,
	uint		flags)
{
	int 		error;
	xfs_qcnt_t	hardlimit;
	xfs_qcnt_t	softlimit;
	time_t		btimer;
	xfs_qcnt_t	*resbcountp;

#ifdef _SHAREII
	uint		enforceflag;
#endif /* _SHAREII */

	if (! (flags & XFS_QMOPT_DQLOCK)) {
		xfs_dqlock(dqp);
	} 
	ASSERT(XFS_DQ_IS_LOCKED(dqp));
	if (flags & XFS_TRANS_DQ_RES_BLKS) {
		hardlimit = dqp->q_core.d_blk_hardlimit;
		softlimit = dqp->q_core.d_blk_softlimit;
		btimer = dqp->q_core.d_btimer;
		resbcountp = &dqp->q_res_bcount;
	} else {
		ASSERT(flags & XFS_TRANS_DQ_RES_RTBLKS);
		hardlimit = dqp->q_core.d_rtb_hardlimit;
		softlimit = dqp->q_core.d_rtb_softlimit;
		btimer = dqp->q_core.d_rtbtimer;
		resbcountp = &dqp->q_res_rtbcount;
	}
	error = 0;

	if ((flags & XFS_QMOPT_FORCE_RES) == 0 &&
	    dqp->q_core.d_id != 0 && 
	    XFS_IS_QUOTA_ENFORCED(dqp->q_mount)) {
		if (nblks > 0) {
			/*
			 * dquot is locked already. See if we'd go over the 
			 * hardlimit or exceed the timelimit if we allocate
			 * nblks.
			 */
			if (hardlimit > 0ULL &&
			     (hardlimit <= nblks + *resbcountp)) {
				/* XXX XFSSTATS */
				error = EDQUOT;
				goto error_return;
			}
				
			if (softlimit > 0ULL &&
			     (softlimit <= nblks + *resbcountp)) {
				/*
				 * If timer or warnings has expired,
				 * return EDQUOT
				 */
				if ((btimer != 0 && time > btimer) ||
				    (dqp->q_core.d_bwarns != 0 && 
				     dqp->q_core.d_bwarns >= 
				     XFS_QI_BWARNLIMIT(dqp->q_mount))) {
					/* XXX XFSSTATS */
					error = EDQUOT;
					goto error_return;
				}
			}
		}
		if (ninos > 0) {
			if (dqp->q_core.d_ino_hardlimit > 0ULL &&
			    dqp->q_core.d_icount >=
			    dqp->q_core.d_ino_hardlimit) {
				/* XXX XFSSTATS */
				error = EDQUOT;
				goto error_return;
			} else if (dqp->q_core.d_ino_softlimit > 0ULL &&
				   dqp->q_core.d_icount >= 
				   dqp->q_core.d_ino_softlimit) {
				/*
				 * If timer or warnings has expired,
				 * return EDQUOT
				 */
				if ((dqp->q_core.d_itimer != 0 &&
				     time > dqp->q_core.d_itimer) || 
				    (dqp->q_core.d_iwarns != 0 &&
				     dqp->q_core.d_iwarns >= 
				     XFS_QI_IWARNLIMIT(dqp->q_mount))) {
					/* XXX XFSSTATS */
					error = EDQUOT;
					goto error_return;
				}
			}
		}
	}

#ifdef _SHAREII
	if (SHR_ACTIVE) {
		if ((flags & XFS_QMOPT_FORCE_RES) == 0  &&
		    dqp->q_core.d_id != 0) 
			enforceflag = LI_ENFORCE;
		else
			enforceflag = 0;
		/* disk blk reservation */
		if (SHR_LIMITDISK(XFS_MTOVFS(dqp->q_mount),
				dqp->q_core.d_id,
				(u_long) nblks,
				XFS_FSB_TO_B(dqp->q_mount, 1),
				LI_ALLOC|LI_UPDATE | enforceflag,
				NULL,
				NULL)) {
			error = EDQUOT;
			goto error_return;
		}
		/* inode reservation */
		if (ninos != 0) {
			if (SHR_LIMITDISK(XFS_MTOVFS(dqp->q_mount),
					dqp->q_core.d_id, 
					(u_long) ninos, 
					0,
					LI_ALLOC|LI_UPDATE | enforceflag,
					NULL,
					NULL)) {
				error = EDQUOT;
				goto error_return;
			}
		}
	}
#endif /* _SHAREII */
        /*
	 * Change the reservation, but not the actual usage. 
	 * Note that q_res_bcount = q_core.d_bcount + resv
	 */
	(*resbcountp) += (xfs_qcnt_t)nblks;
	if (ninos != 0)
		dqp->q_res_icount += (xfs_qcnt_t)ninos;
	
	/*
	 * note the reservation amt in the trans struct too,
	 * so that the transaction knows how much was reserved by
	 * it against this particular dquot.
	 * We don't do this when we are reserving for a delayed allocation,
	 * because we don't have the luxury of a transaction envelope then.
	 */
	if (tp) {
		ASSERT(tp->t_dqinfo);
		ASSERT(flags & XFS_QMOPT_RESBLK_MASK);
		if (nblks != 0)
			xfs_trans_mod_dquot(tp, dqp, 
					    flags & XFS_QMOPT_RESBLK_MASK, 
					    nblks);
		if (ninos != 0)
			xfs_trans_mod_dquot(tp, dqp, 
					    XFS_TRANS_DQ_RES_INOS, 
					    ninos);
	} 
	ASSERT(dqp->q_res_bcount >= dqp->q_core.d_bcount);
	ASSERT(dqp->q_res_rtbcount >= dqp->q_core.d_rtbcount);
	ASSERT(dqp->q_res_icount >= dqp->q_core.d_icount);

error_return:
	if (! (flags & XFS_QMOPT_DQLOCK)) {
		xfs_dqunlock(dqp);
	}
	return (error);
}


/*
 * Given a dquot(s), make disk block and/or inode reservations against them.
 * The fact that this does the reservation against both the usr and
 * prj quotas is important, because this follows a both-or-nothing
 * approach.
 * 
 * flags = XFS_QMOPT_DQLOCK indicate if dquot(s) need to be locked.
 *         XFS_QMOPT_FORCE_RES evades limit enforcement. Used by chown.
 * 	   XFS_TRANS_DQ_RES_BLKS reserves regular disk blocks
 * 	   XFS_TRANS_DQ_RES_RTBLKS reserves realtime disk blocks
 * dquots are unlocked on return, if they were not locked by caller.
 */

int
xfs_trans_reserve_quota_bydquots(
	xfs_trans_t 	*tp,
	xfs_dquot_t 	*udqp,
	xfs_dquot_t 	*pdqp,	
	long		nblks,
	long		ninos,
	uint		flags)
{
	int 		resvd;

	if (tp && tp->t_dqinfo == NULL)
		xfs_trans_alloc_dqinfo(tp);

	ASSERT(flags & XFS_QMOPT_RESBLK_MASK);
	resvd = 0;

	if (udqp) {
		if (xfs_trans_dqresv(tp, udqp, nblks, ninos, flags))
			return (EDQUOT);
		resvd = 1;
	}
	
	if (pdqp) {
		if (xfs_trans_dqresv(tp, pdqp, nblks, ninos, flags)) {
			/* 
			 * can't do it, so backout previous reservation
			 */
			if (resvd) {
				xfs_trans_dqresv(tp, udqp,  -nblks, -ninos,
						 flags);
			}
			return (EDQUOT);
		}
	}

	/* 
	 * Didnt change anything critical, so, no need to log
	 */
	return (0);
}


/*
 * Lock the dquot and change the reservation if we can.
 * This doesnt change the actual usage, just the reservation.
 * The inode sent in is locked.
 *
 * Returns 0 on success, EDQUOT or other errors otherwise
 */
int
xfs_trans_reserve_quota_nblks(
	xfs_trans_t 	*tp,
	xfs_inode_t 	*ip,
	long		nblks,
	long		ninos,
	uint		type)
{
	int error;

#ifdef QUOTADEBUG
	if (ip->i_udquot)
		ASSERT(! XFS_DQ_IS_LOCKED(ip->i_udquot));
	if (ip->i_pdquot)
		ASSERT(! XFS_DQ_IS_LOCKED(ip->i_pdquot));
#endif

	ASSERT(XFS_ISLOCKED_INODE_EXCL(ip));
	ASSERT(XFS_IS_QUOTA_RUNNING(ip->i_mount));
	ASSERT(type == XFS_TRANS_DQ_RES_RTBLKS ||
	       type == XFS_TRANS_DQ_RES_BLKS);

	/*
	 * Reserve nblks against these dquots, with trans as the mediator.
	 */
	error = xfs_trans_reserve_quota_bydquots(tp, 
						 ip->i_udquot, ip->i_pdquot,
						 nblks, ninos,
						 type);
	return (error);
	
}

/*
 * This routine is called to allocate a quotaoff log item. 
 */
xfs_qoff_logitem_t *
xfs_trans_get_qoff_item(
	xfs_trans_t		*tp,
	xfs_qoff_logitem_t 	*startqoff,
	uint			flags)
{
	xfs_qoff_logitem_t	*q;

	ASSERT(tp != NULL);

	q = xfs_qm_qoff_logitem_init(tp->t_mountp, startqoff, flags);
	ASSERT(q != NULL);

	/*
	 * Get a log_item_desc to point at the new item.
	 */
	(void) xfs_trans_add_item(tp, (xfs_log_item_t*)q);

	return (q);
}


/*
 * This is called to mark the quotaoff logitem as needing
 * to be logged when the transaction is committed.  The logitem must
 * already be associated with the given transaction.
 */
void
xfs_trans_log_quotaoff_item(
        xfs_trans_t     	*tp,
	xfs_qoff_logitem_t	*qlp)
{
        xfs_log_item_desc_t     *lidp;

        lidp = xfs_trans_find_item(tp, (xfs_log_item_t *)qlp);
        ASSERT(lidp != NULL);

        tp->t_flags |= XFS_TRANS_DIRTY;
        lidp->lid_flags |= XFS_LID_DIRTY;
}

void
xfs_trans_alloc_dqinfo(
	xfs_trans_t	*tp)
{
	(tp)->t_dqinfo = kmem_zone_zalloc(xfs_Gqm->qm_dqtrxzone, KM_SLEEP);
}

void
xfs_trans_free_dqinfo(
	xfs_trans_t	*tp)
{
	kmem_zone_free(xfs_Gqm->qm_dqtrxzone, (tp)->t_dqinfo);
	(tp)->t_dqinfo = NULL;
}
