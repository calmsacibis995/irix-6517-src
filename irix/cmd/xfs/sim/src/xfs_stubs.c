#define _KERNEL 1
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/buf.h>
#undef _KERNEL
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode_item.h>
#include <sys/fs/xfs_inode.h>

#include "sim.h"

int			xlog_debug = 0;

/* ARGSUSED */
void
grio_iodone(buf_t *bp)
{
	return;
}

/* ARGSUSED */
xfs_lsn_t
xfs_log_done(xfs_mount_t *mp, xfs_log_ticket_t xtic, uint flags)
{
	return 0;
}

/* ARGSUSED */
int
xfs_log_force(xfs_mount_t *mp, xfs_lsn_t lsn, uint flags)
{
	return 0;
}

/* ARGSUSED */
int
xfs_log_mount(xfs_mount_t *mp, dev_t log_dev, daddr_t start_block,
		int num_bblocks)
{
	return 0;
}

/* ARGSUSED */
void
xfs_log_move_tail(xfs_mount_t *mp, xfs_lsn_t tail_lsn)
{
	return;
}

/* ARGSUSED */
void
xfs_log_notify(xfs_mount_t *mp, xfs_lsn_t lsn, xfs_log_callback_t *cb)
{
	return;
}

/* ARGSUSED */
int
xfs_log_reserve(xfs_mount_t *mp, int unit_bytes, int cnt,
		xfs_log_ticket_t *ticket, char client, uint flags)
{
	return 0;
}

/* ARGSUSED */
int
xfs_log_unmount(xfs_mount_t *mp)
{
	return 0;
}

/* ARGSUSED */
int
xfs_log_write(xfs_mount_t *mp, xfs_log_iovec_t reg[], int nentries,
		xfs_log_ticket_t tic, xfs_lsn_t *start_lsn)
{
	*start_lsn = 0;
	return 0;
}

/* ARGSUSED */
void
xfs_zero_eof(xfs_inode_t *ip, off_t offset, xfs_fsize_t isize,
	     cred_t *credp, struct pm *pmp)
{
	return;
}

/* ARGSUSED */
void		
xfs_trans_dup_dqinfo(struct xfs_trans *otp, struct xfs_trans *ntp) 
{ 
	return;
}

/* ARGSUSED */
void		
xfs_trans_unreserve_and_mod_dquots(struct xfs_trans *tp) 
{ 
	return;
}

/* ARGSUSED */
int		
xfs_trans_mod_dquot_byino(struct xfs_trans *tp, 
			  struct xfs_inode *ip,
			  uint field, long delta) 
{ 
	return 0;
}

/* ARGSUSED */
void 		
xfs_trans_free_dqinfo(struct xfs_trans *tp) 
{
	return;
}

/* ARGSUSED */
void		
xfs_qm_dqdettach_inode(struct xfs_inode *ip) 
{ 
	return; 
}

/* ARGSUSED */
void		
xfs_qm_unmount_quotadestroy(struct xfs_mount *mp) 
{
	return; 
}

/* ARGSUSED */
int		
xfs_trans_reserve_quota_nblks(struct xfs_trans *tp,
			      struct xfs_inode *ip,
			      long nblks,
			      uint type) 
{
	return 0;
}

/* ARGSUSED */
int
xfs_log_force_umount(struct xfs_mount *mp, int logerror)
{
	return (0);
}



/* ARGSUSED */
int
xfs_bioerror(
	buf_t *bp)
{
	return (5); /* EIO */
}

/* ARGSUSED */
void
xfs_ioerror_alert(
	char 			*func,
	struct xfs_mount	*mp,
	dev_t			dev,
	daddr_t			blkno)
{
}
/* ARGSUSED */
int
xfsbdstrat(
	struct xfs_mount 	*mp,
	struct buf		*bp)
{
	struct bdevsw	*my_bdevsw;

	ASSERT(mp);
	my_bdevsw = get_bdevsw(bp->b_edev);
	ASSERT(my_bdevsw != NULL);
	bdstrat(my_bdevsw, bp);
	return 0;
}


/* ARGSUSED */
int
xfs_bwrite(
	struct xfs_mount *mp,
	struct buf	 *bp)
{
	bp->b_bdstrat = 0;
	return (bwrite(bp));
}

/* ARGSUSED */
int
xfs_bdstrat_cb(struct buf *bp)
{
	struct bdevsw	*my_bdevsw;
	my_bdevsw = get_bdevsw(bp->b_edev);
	ASSERT(my_bdevsw != NULL);
	bdstrat(my_bdevsw, bp);
	return 0;
}
	

/* ARGSUSED */
int
xfs_read_buf(
	struct xfs_mount *mp,
	buftarg_t	 *targ,
        daddr_t 	 blkno,
        int              len,
        uint             flags,
	buf_t		 **bpp)
{
	buf_t		 *bp;
	int 		 error;
	bp = read_buf(targ->dev, blkno, len, flags);
	error = geterror(bp);
	if (bp && !error) {
		*bpp = bp;
	} else {
		*bpp = NULL;
		if (bp)
			brelse(bp);
	}
	return (error);
}


/* ARGSUSED */
void
xfs_force_shutdown(
	struct xfs_mount	*mp,
	int			logerror)
{
}
