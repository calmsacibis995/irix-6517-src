/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* ident  "$Revision: 1.4 $" */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/pfdat.h>
#include <sys/kmem.h>
#include <sys/immu.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>

#include <ksys/cell/tkm.h>
#include <fs/cell/cxfs_intfc.h>
#include <fs/cell/dvn_tokens.h>

#include "cxfs.h"
#include "dsxvn.h"

typedef struct {
	uint	lock_mode;
	struct xfs_inode *ip;
} dsxbuf_t;

static	zone_t	*dsxvn_zone;
static	zone_t	*dsxbuf_zone;

static	int	max_ext_count;

#define min(x, y) ((x) < (y) ? (x) : (y))

void	dsxvn_init()
{
	dsxvn_zone = kmem_zone_init(sizeof(dsxvn_t), "dsxvn");
	dsxbuf_zone = kmem_zone_init(sizeof(dsxbuf_t), "dsxbuf");

	max_ext_count = NBPP / sizeof(xfs_bmbt_rec_t);
}


extern int                                             /* error */
xfs_bmapi_single(
	struct xfs_trans        *tp,            /* transaction pointer */
	struct xfs_inode        *ip,            /* incore inode */
	int                     whichfork,      /* data or attr fork */
	xfs_fsblock_t           *fsb,           /* output: mapped block */
	xfs_fileoff_t           bno);           /* starting file offs. mapped */


/*	Create the dsxvn structure on the dsvn structure
 */

/* ARGSUSED */ 
dsxvn_t *cxfs_dsxvn_make(
	dsvn_t		*dsp,
	bhv_desc_t	*bdp,
	tks_state_t	*tks,
	tkc_state_t	*tkc)
 {
	dsxvn_t		*dsxp;
	vnode_t		*vp;

	dsxp = kmem_zone_alloc(dsxvn_zone, KM_SLEEP);
	dsxp->dsx_dsvn = dsp;
	dsxp->dsx_tclient = tkc;
	dsxp->dsx_tserver = tks;

	vp = BHV_TO_VNODE(bdp);
	dsxp->dsx_bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), &xfs_vnodeops);

	return(dsxp);
}

void	cxfs_dsxvn_destroy(
	dsxvn_t  *dsxp)
{
	if (dsxp) {
		kmem_zone_free(dsxvn_zone, dsxp);
	}
}

/* ARGSUSED */
int	cxfs_dsxvn_obtain(
	dsxvn_t		*dsxp,
	char		**cxfs_buff,
	size_t		*cxfs_count,
	int		*cxfs_flags,
	tk_set_t	granted_set,
	tk_set_t	refused_set,
	tk_set_t	held_set,
	void		**bufdesc)
{
	ASSERT(dsxp->dsx_bdp);

	*cxfs_count = 0;
	*cxfs_flags = 0;
	*bufdesc = 0;
	if (TK_IS_IN_SET(DVN_EXTENT_READ, granted_set) ||
	    TK_IS_IN_SET(DVN_EXTENT_READ, held_set)) {

		return(cxfs_fetch_extents(dsxp->dsx_bdp, cxfs_buff, cxfs_count,
					  cxfs_flags, 0, 0, 0, bufdesc));
	}

	return(0);
}

/* ARGSUSED */
void	cxfs_dsxvn_obtain_done(
	char		*cxfs_buff,
	size_t		cxfs_count,
	void		*bufdesc)
{
	dsxbuf_t *bp = (dsxbuf_t *) bufdesc;

	if (bp) {
		ASSERT(bp->ip->i_d.di_magic == XFS_DINODE_MAGIC);
		xfs_iunlock_map_shared(bp->ip, bp->lock_mode);
		kmem_zone_free(dsxbuf_zone, bp);
	}
}

/*	Return a pointer to a range of extents from the in core extent list
 *
 *	TODO - possibly leave inode locked, and allow a callback to unlock
 *		it again. Need to ensure extent data passed back consistently.
 */

int	cxfs_fetch_extents(
	bhv_desc_t	*bdp,	/* behaviour pointer */
	char	**cxfs_buff,	/* pointer for returned extents */
	size_t	*cxfs_count,	/* byte count of returned extents */
	int	*cxfs_flags,	/* what got returned */
	off_t	file_offset,	/* starting file offset to return extent for */
	size_t	ext_count,	/* count of extents to return */
	int	read_extents,	/* read extents from disk if not present */
	void	**bufdesc)	/* pointer so RPC callout can free/unlock */
{
	xfs_inode_t	*ip;
	xfs_ifork_t	*ifp;
	int		error;
	xfs_fsblock_t	fsb;
	xfs_fileoff_t	bno;
	xfs_extnum_t	lastx = 0;
	size_t		ext_avail;
	uint		lock_mode;
	dsxbuf_t	*bp;

	ip = XFS_BHVTOI(bdp);

	*cxfs_count = 0;
	*bufdesc = 0; 

	if (!ext_count || (ext_count > max_ext_count)) {
		ext_count = max_ext_count;
	} 

	lock_mode = xfs_ilock_map_shared(ip);
	ifp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);

	/* Read in extents if not already present in core */
	if (((ifp->if_flags & XFS_IFEXTENTS) == 0) && read_extents) {
		/* What to do in an error case here */
		error = xfs_iread_extents(NULL, ip, XFS_DATA_FORK);
		if (error) {
			xfs_iunlock_map_shared(ip, lock_mode);
			return(error);
		}
	}

	/* If we have extents present then search for the requested
	 * starting position.
	 */
	if (ifp->if_flags & XFS_IFEXTENTS) {
		ext_avail = ifp->if_bytes / sizeof(xfs_bmbt_rec_t);
		if (file_offset) {
			bno = XFS_B_TO_FSBT(ip->i_mount, file_offset);
			error = xfs_bmapi_single(NULL, ip, XFS_DATA_FORK,
						 &fsb, bno);
			if (error) {
				xfs_iunlock_map_shared(ip, lock_mode);
				return(error);
			}
			/* Check for reading past EOF */
			if (fsb == NULLFSBLOCK) {
				xfs_iunlock_map_shared(ip, lock_mode);
				return (0);
			}
			/* This gets set by xfs_bmapi_single */
			lastx = ifp->if_lastex;
			ext_avail -= lastx;
		}
		*cxfs_count = min(ext_avail, ext_count);
		if (*cxfs_count < ext_avail) {
			*cxfs_flags |= CXFS_MOREEXTENTS;
		}
		if (*cxfs_count) {
			*cxfs_count *= sizeof(xfs_bmbt_rec_t);
			*cxfs_buff = (char *) &ifp->if_u1.if_extents[lastx];
			ASSERT(*cxfs_buff);
		}
		*cxfs_flags |= CXFS_IFEXTENTS;

		bp = kmem_zone_alloc(dsxbuf_zone, KM_SLEEP);
		bp->ip = ip;
		bp->lock_mode = lock_mode;
		*bufdesc = (void *)bp;
	} else {
		xfs_iunlock_map_shared(ip, lock_mode);
	}

	return(0);
}

/* ARGSUSED */ 
void
cxfs_dsxvn_return(
	dsxvn_t		*dsxp,
	cell_t		client,
	tk_set_t	retset,
	tk_set_t	unknownset,
	tk_disp_t	why)
{
	/*
	 * This routine is called from DS when a token has been returned
	 * by a client. If any data is ever returned by a client, this routine
	 * should handle it.
	 *
	 * To return data, the I_dsvn_return RPC needs to carry the data
	 * and pass it to this routine with some new parameters.
	 *
	 * For now, just a no-op since all that needs to
	 * be done is the tks_return which I_dsvn_return already does.
	 *
	 */
	return;
}
