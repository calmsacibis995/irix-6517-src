#ident "$Revision: 1.12 $"

/*
 * xfs_inode.c 
 * 	XFS stand alone library inode routines.
 * 
 */

#include <arcs/io.h>
#include <arcs/dirent.h>
#include <arcs/pvector.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/sbd.h>
#include <sys/dkio.h>
#include <saio.h>
#include <arcs/fs.h>
#include <values.h>
#include <libsc.h>
#include <libsc_internal.h>

#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_imap.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_alloc.h>
#include <sys/fs/xfs_bmap.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_buf_item.h>
#include <sys/fs/xfs_rw.h>
#include <sys/fs/xfs_error.h>
#include <sys/fs/xfs_bit.h>

#include "xfs_local.h"

extern int xfs_lbread(FSBLOCK *, daddr_t, void *, long, int);

/*
 * _xfs_bmap_read_extents()
 *	Read in the blocks of a BTREE format file and save
 *	extent information.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
int
_xfs_bmap_read_extents( 
	FSBLOCK		*fsb, 
	xfs_inode_t	*ip)
{
        int                     level;  /* btree level, for checking   */
        int                     ret;    /* return value                */
	char 			*buf;	/* local I/O buffer            */
        xfs_mount_t             *mp;    /* file system mount structure */
        xfs_extnum_t            room;   /* entries there's room for    */
        xfs_extnum_t            i;      /* index into the extents list */
        xfs_fsblock_t           bno;    /* block # of "block"          */
        xfs_bmbt_ptr_t          *pp;    /* pointer to block address    */
	xfs_bmbt_rec_t          *trp;   /* target record pointer       */
        xfs_bmbt_block_t        *block; /* current btree block         */


        bno   = NULLFSBLOCK;
        mp    = fsb_to_mount(fsb);
	block = ip->i_df.if_broot;
	
	/*
	 * Allocate I/O buffer.
	 */
	if ((buf = dmabuf_malloc(XFS_FSB_TO_B(mp, 1))) == 0) {
		printf("xfs: could not allocate memory for node buffer \n");
		return 0;
	}

        /*
         * Get initial btree level.
         */
	if ((level = block->bb_level) <= 0) {
		printf("xfs: initial node level less than 0.\n");
		dmabuf_free(buf);
		return 0;
	}

        /*
         * Root level must use BMAP_BROOT_PTR_ADDR macro to get ptr out.
         */
        pp = XFS_BMAP_BROOT_PTR_ADDR(block, 1, ip->i_df.if_broot_bytes);

	if ((bno = ((xfs_fsblock_t)*pp) ) == NULLDFSBNO) {
		printf("xfs: initial node block number is NULL.\n");
		dmabuf_free(buf);
		return 0;
	}

        /*
         * Go down the tree until leaf level is reached, following the first
         * pointer (leftmost) at each level.
         */
        while ( level-- > 0) {

		/*
		 * Read the file system block.
 		 */
		if (!xfs_lbread(fsb, XFS_FSB_TO_DADDR(mp, bno), buf,
				XFS_FSB_TO_B(mp, 1), 1)) {
			printf("xfs: error reading node block \n");
			dmabuf_free(buf);
			return 0;
		}

                block = (xfs_bmbt_block_t *)buf;

		if (block->bb_level != level) {
			printf("xfs: unexpected node block level.\n");
			dmabuf_free(buf);
			return 0;
		}

                if (level == 0)
                        break;

                pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, 
			xfs_bmbt, block, 1, mp->m_bmap_dmxr[1]);

                bno = (xfs_fsblock_t)*pp;
        }


	/*
	 * Here with buf and block set to the leftmost leaf node in the tree.
	 */
	room = (xfs_extnum_t)(ip->i_df.if_bytes / sizeof(*trp));
	trp  = ip->i_df.if_u1.if_extents;
	i    = 0;

	/*
	 * Loop over all leaf nodes.  Copy information to the extent list.
	 */
	for (;;) {
		xfs_bmbt_rec_t  *frp;

		if ( (i + block->bb_numrecs) > room) {
			printf("xfs: not enough space for extents in node.\n");
			dmabuf_free(buf);
			return 0;
		}

		/*
		 * Copy records into the extent list.
		 */
		frp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, 
			xfs_bmbt, block, 1, mp->m_bmap_dmxr[0]);

		bcopy(frp, trp, block->bb_numrecs * ((int)sizeof(*frp)) );

		trp += block->bb_numrecs;
		i   += block->bb_numrecs;

		/*
		 * Get the next leaf block's address.
		 */
		bno = (xfs_fsblock_t)block->bb_rightsib;

		/*
		 * If we've reached the end, stop.
		 */
		if (bno == NULLFSBLOCK)
			break;

		if (!xfs_lbread(fsb, XFS_FSB_TO_DADDR(mp, bno), buf,
				XFS_FSB_TO_B(mp, 1), 1)) {
			printf("xfs: error reading node block 2\n");
			dmabuf_free(buf);
			return 0;
		}

                block = (xfs_bmbt_block_t *)buf;
	}
	ret = 1;

	if (i != (ip->i_df.if_bytes/ sizeof(*trp))) {
		printf("xfs: incorrect number of extents in node.\n");
		ret = 0;
	}

	if (i != ip->i_d.di_nextents) {
		printf("xfs: incorrect number of extents in node.\n");
		ret = 0;
	}

	dmabuf_free(buf);
	return ret;
}

/*
 * _xfs_iread_extents()
 *	Allocate memory and read in the file extent information.
 *
 * RETURNS:
 *	none
 */
void
_xfs_iread_extents( FSBLOCK *fsb, xfs_inode_t *ip)
{
	size_t 	size = ip->i_d.di_nextents * sizeof(xfs_bmbt_rec_t);

        if ((ip->i_df.if_u1.if_extents = malloc(size)) == 0) {
		printf("xfs: could not allocate memory for extents \n");
		return;
	}
	
        ip->i_df.if_lastex = NULLEXTNUM;
        ip->i_df.if_bytes  = ip->i_df.if_real_bytes = (int)size;
        ip->i_df.if_flags |= XFS_IFEXTENTS;
        _xfs_bmap_read_extents( fsb, ip);

	return;
}


/*
 * _xfs_bmdr_to_bmbt()
 * 	Convert on-disk form of btree root to in-memory form.
 * 
 * RETURNS: 
 * 	none
 */
void
_xfs_bmdr_to_bmbt(
        xfs_bmdr_block_t        *dblock,
        int                     dblocklen,
        xfs_bmbt_block_t        *rblock,
        int                     rblocklen)
{
        int                     dmxr, lf = 0;
        xfs_bmbt_key_t          *fkp, *tkp;
        xfs_bmbt_ptr_t          *fpp, *tpp;

        rblock->bb_magic   = XFS_BMAP_MAGIC;
        rblock->bb_level   = dblock->bb_level;
        rblock->bb_numrecs = dblock->bb_numrecs;
        rblock->bb_leftsib = rblock->bb_rightsib = NULLDFSBNO;

        dmxr = (int)XFS_BTREE_BLOCK_MAXRECS(dblocklen, xfs_bmdr, lf);
        fkp  = XFS_BTREE_KEY_ADDR(dblocklen, xfs_bmdr, dblock, 1, dmxr);
        tkp  = XFS_BMAP_BROOT_KEY_ADDR(rblock, 1, rblocklen);
        fpp  = XFS_BTREE_PTR_ADDR(dblocklen, xfs_bmdr, dblock, 1, dmxr);
        tpp  = XFS_BMAP_BROOT_PTR_ADDR(rblock, 1, rblocklen);

        bcopy(fkp, tkp, (int)sizeof(*fkp) * dblock->bb_numrecs);
        bcopy(fpp, tpp, (int)sizeof(*fpp) * dblock->bb_numrecs);
	return;
}



/*
 * xfs_iformat()
 * 	Convert on-disk inode to the in-core inode.
 *
 * RETURNS:
 *	none
 */
void
xfs_iformat(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip,
	xfs_dinode_t	*dip)
{
	register int		size, nex, nrecs;
	int			real_size;

	switch (ip->i_d.di_mode & IFMT) {
	case IFIFO:
	case IFCHR:
	case IFBLK:
	case IFSOCK:
		if (dip->di_core.di_format != XFS_DINODE_FMT_DEV) {
			printf("xfs: bad inode format \n");
			return;
		}
		ip->i_d.di_size = 0;
		ip->i_df.if_u2.if_rdev = dip->di_u.di_dev;
		return;

	case IFREG:
	case IFLNK:
	case IFDIR:

		switch (dip->di_core.di_format) {
		case XFS_DINODE_FMT_LOCAL:
			/*
			 * The file is in-lined in the on-disk inode.
			 * If it fits into if_inline_data, then copy
			 * it there, otherwise allocate a buffer for it
			 * and copy the data there.  Either way, set
			 * if_data to point at the data.
			 * If we allocate a buffer for the data, make
			 * sure that its size is a multiple of 4 and
			 * record the real size in if_real_bytes.
			 */
			size = (int) ip->i_d.di_size;
			real_size = 0;
			if (size == 0) {
				ip->i_df.if_u1.if_data = NULL;
			} else if (size <= sizeof(ip->i_df.if_u2.if_inline_data)) {
				ip->i_df.if_u1.if_data = ip->i_df.if_u2.if_inline_data;
			} else {
				real_size = (((size + 3) >> 2) << 2);
				ip->i_df.if_u1.if_data = (char*)malloc(real_size);
			}
			ip->i_df.if_bytes = size;
			ip->i_df.if_real_bytes = real_size;
			if (size)
				bcopy(dip->di_u.di_c, ip->i_df.if_u1.if_data, size);
			ip->i_df.if_flags &= ~XFS_IFEXTENTS;
			ip->i_df.if_flags |= XFS_IFINLINE;
			return;

		case XFS_DINODE_FMT_EXTENTS:
			/*
			 * The file consists of a set of extents all
			 * of which fit into the on-disk inode.
			 * If there are few enough extents to fit into
			 * the if_inline_ext, then copy them there.
			 * Otherwise allocate a buffer for them and copy
			 * them into it.  Either way, set if_extents
			 * to point at the extents.
			 */
			nex = (int)dip->di_core.di_nextents;
			size = nex * (int)sizeof(xfs_bmbt_rec_t);
			real_size = 0;
			if (nex == 0) {
				ip->i_df.if_u1.if_extents = NULL;
			} else if (nex <= XFS_INLINE_EXTS) {
				ip->i_df.if_u1.if_extents = ip->i_df.if_u2.if_inline_ext;
			} else {
				ip->i_df.if_u1.if_extents = (xfs_bmbt_rec_t*)
						malloc(size);
				real_size = size;
			}
			ip->i_df.if_bytes = size;
			ip->i_df.if_real_bytes = real_size;
			if (size) {
				bcopy(dip->di_u.di_bmx, ip->i_df.if_u1.if_extents,
				      size);
			}
			ip->i_df.if_flags |= XFS_IFEXTENTS;
			return;

		case XFS_DINODE_FMT_BTREE:
			/*
			 * The file has too many extents to fit into
			 * the inode, so they are in B-tree format.
			 * Allocate a buffer for the root of the B-tree
			 * and copy the root into it.  The i_extents
			 * field will remain NULL until all of the
			 * extents are read in (when they are needed).
			 */
			size = XFS_BMAP_BROOT_SPACE(&(dip->di_u.di_bmbt));
			nrecs = XFS_BMAP_BROOT_NUMRECS(&(dip->di_u.di_bmbt));
			if (nrecs <= 0) {
				printf("xfs: invalid number of records in inode \n");
				return;
			}
			ip->i_df.if_broot_bytes = size;
			ip->i_df.if_broot = malloc(size);

			/*
			 * Copy and convert from the on-disk structure
			 * to the in-memory structure.
			 */
			_xfs_bmdr_to_bmbt(&dip->di_u.di_bmbt,
				(int)XFS_DFORK_DSIZE(dip, mp),
				ip->i_df.if_broot, size);
			ip->i_df.if_flags &= ~XFS_IFEXTENTS;
			ip->i_df.if_flags |= XFS_IFBROOT;
			return;

		default:
			printf("xfs: bad inode core format \n");
			return;
		}

	default:
		printf("xfs: bad inode core mode \n");
		return;
	}
}

/*
 * _xfs_ifree()
 * 	Free the structure allocated by the xfs_iformat() routine.
 *
 * RETURNS:
 *	none
 */
void
_xfs_ifree( xfs_inode_t	*ip)
{
	int nex;

	switch (ip->i_d.di_mode & IFMT) {
	case IFIFO:
	case IFCHR:
	case IFBLK:
	case IFSOCK:
		return;

	case IFREG:
	case IFLNK:
	case IFDIR:
		switch (ip->i_d.di_format) {
		case XFS_DINODE_FMT_LOCAL:
			return;

		case XFS_DINODE_FMT_EXTENTS:
			nex = (int)ip->i_d.di_nextents;
			if (nex > XFS_INLINE_EXTS) {
				free(ip->i_df.if_u1.if_extents);
			}
			return;

		case XFS_DINODE_FMT_BTREE:
			free(ip->i_df.if_broot);
			free(ip->i_df.if_u1.if_extents);
			return;

		default:
			printf("xfs: ifree bad format.\n");
			return;
		}

	default:
		printf("xfs: ifree bad format.\n");
		return;
	}
}


/*
 * xfs_convert_extent()
 *	Breakup the packed extent record into its
 *	fileoffset, startblock, and size in blocks.
 *
 * RETURNS:
 *	none
 */
static void
_xfs_convert_extent(
	xfs_bmbt_rec_32_t	*rp, 
	xfs_dfiloff_t		*op, 
	xfs_dfsbno_t		*sp, 
	xfs_dfilblks_t		*cp)
{
        xfs_dfsbno_t		s;
        xfs_dfilblks_t		c;
        xfs_dfiloff_t		o;

        o = (((xfs_dfiloff_t)rp->l0) << 23) |
            (((xfs_dfiloff_t)rp->l1) >> 9);
        s = (((xfs_dfsbno_t)(rp->l1 & 0x000001ff)) << 43) |
            (((xfs_dfsbno_t)rp->l2) << 11) |
            (((xfs_dfsbno_t)rp->l3) >> 21);
        c = (xfs_dfilblks_t)(rp->l3 & 0x001fffff);
        *op = o;
        *sp = s;
        *cp = c;
}



/*
 *  _xfs_get_blockno
 *	Translate a file offset into a file system block number.
 *
 *
 * RETURNS:
 *	a valid file system block number on success
 *	NULLFSBLOCK on failure
 */
/*ARGSUSED*/
xfs_fsblock_t
_xfs_get_blockno(xfs_mount_t *mp, xfs_inode_t *ip, xfs_fileoff_t off,
		 long *rlen)
{
        int		nextents, i;
	xfs_fileoff_t	len;
        xfs_dfilblks_t	count;
        xfs_dfsbno_t	startblk;
	xfs_fsblock_t	thisblk;
        xfs_dfiloff_t	extentb, extente;

	thisblk = NULLFSBLOCK;

        if (!(ip->i_df.if_flags & XFS_IFEXTENTS)) {
		printf("xfs: trying to read extents of a non extent file.\n");
                return ( thisblk );
	}
        nextents = (int) ( (ip->i_df.if_bytes) / sizeof(xfs_bmbt_rec_t) );

        for (i = 0; i < nextents; i++) {
                _xfs_convert_extent(
			(xfs_bmbt_rec_32_t *)&ip->i_df.if_u1.if_extents[i],
			&extentb, &startblk, &count);

		/*
		 * Compute the block number of the end of the extent.
		 */
		extente = extentb + count;

		/*
		 * If have gone past the end of this extent, give up.
		 */
		if ( off < extentb )
			break;

		/*
		 * Determine if the requested offset is within this extent.
		 */
		if ( ( off >= extentb ) && ( off < extente )) {
			len = off - (xfs_fileoff_t)extentb;

			/*
		 	 * Compute the block offset within this extent.
			 *
			 * blknos on disk are 64 bits, in memory they are 
			 * either 32 or 64 bits.
			 */
			thisblk = (xfs_fsblock_t)
				(((xfs_fsblock_t)(startblk)) + len);
			if (rlen)
				*rlen = (long)count - (long)len;
			break;
		}
        }
	return( thisblk );

}
