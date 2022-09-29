#ident "$Revision: 1.1 $"

/*
 * xfs_dir2.c
 *	XFS stand alone library directory routines.
 * 
 */
#include <arcs/io.h>
#include <arcs/dirent.h>
#include <arcs/pvector.h>

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/uuid.h>
#include <sys/dirent.h>
#include <saio.h>
#include <arcs/fs.h>
#include <values.h>
#include <libsc.h>
#include <libsc_internal.h>
#include <stddef.h>

#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_alloc.h>
#include <sys/fs/xfs_bmap.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_bmap.h>
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_error.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_dir_leaf.h>
#include <sys/fs/xfs_dir2_data.h>
#include <sys/fs/xfs_dir2_leaf.h>
#include <sys/fs/xfs_dir2_block.h>

#include "xfs_local.h"

static int _xfs_dirv2_data_getdent(FSBLOCK *, xfs_dirent_t *);
static int _xfs_dirv2_data_lookup(FSBLOCK *, xfs_da_args_t *);
static int _xfs_dirv2_data_lookup_int(FSBLOCK *, char *, xfs_da_args_t *);
static int _xfs_dirv2_read_block(FSBLOCK *, xfs_inode_t *, xfs_dir2_db_t,
	char *, xfs_fsblock_t *);
static int _xfs_dirv2_sf_getdent(FSBLOCK *, xfs_dirent_t *);
static int _xfs_dirv2_sf_lookup(xfs_da_args_t *);

extern uint xfs_dir_hashname(char *, int);
extern xfs_fsblock_t _xfs_get_blockno(xfs_mount_t *, xfs_inode_t *,
	xfs_fileoff_t, long *);
extern int xfs_lbread(FSBLOCK *, daddr_t, void *, long, int);


/*
 * _xfs_dirv2_lookup()
 * 	Get the inode number of the file with the given name in the
 * 	given directory.
 * 
 *  RETURNS:
 *	1 on success 
 * 	0 on failure
 */
int
_xfs_dirv2_lookup(
	FSBLOCK		*fsb,
	xfs_inode_t	*dp, 
	char 		*name, 
	int		namelen,
   	xfs_ino_t	*inum)
{
	xfs_da_args_t	args;
	int 		retval;

	if (namelen >= MAXNAMELEN)
		return 0;

	/*
	 * Fill in the arg structure for this request.
	 */
	args.name       = name;
	args.namelen    = namelen;
	args.hashval    = xfs_dir_hashname(name, namelen);
	args.inumber    = 0;
	args.dp         = dp;

	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL) {
		retval = _xfs_dirv2_sf_lookup(&args);
	} else {
		retval = _xfs_dirv2_data_lookup(fsb, &args);
	}

	*inum = args.inumber;

	return retval;
}

/*
 * _xfs_dirv2_sf_lookup()
 *	Find the inode number of the file with the given name.
 *	This routine handles "shortform" type directories.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
static int
_xfs_dirv2_sf_lookup(
	xfs_da_args_t		*args)
{
	int			i;
	xfs_inode_t		*dp;
	xfs_dir2_sf_entry_t	*sfe;
	xfs_dir2_sf_t		*sf;

	dp = args->dp;

	if ((dp->i_df.if_flags & XFS_IFINLINE) == 0) {
		printf("xfs: invalid shortform inode.\n");
		return 0;
	}

	sf = (struct xfs_dir2_sf *)dp->i_df.if_u1.if_data;

	/*
	 * Check for the ".." entry.
 	 */
	if (args->namelen == 2 &&
	    args->name[0] == '.' && args->name[1] == '.') {
		args->inumber = XFS_DIR2_SF_GET_INUMBER(sf, &sf->hdr.parent);
		return 1;
	}

	/*
	 * Check for the "." entry.
 	 */
	if (args->namelen == 1 && args->name[0] == '.') {
		args->inumber = dp->i_ino;
		return 1;
	}

	sfe = XFS_DIR2_SF_FIRSTENTRY(sf);
	for (i = 0; i < sf->hdr.count; i++) {
		if (sfe->namelen == args->namelen &&
		    bcmp(args->name, sfe->name, args->namelen) == 0) {
			args->inumber =
				XFS_DIR2_SF_GET_INUMBER(sf,
					XFS_DIR2_SF_INUMBERP(sfe));
			return 1;
		}
		sfe = XFS_DIR2_SF_NEXTENTRY(sf, sfe);
	}
	return 0;
}

/*
 * _xfs_dirv2_data_lookup()
 *	Find the inode number of the file with the given name.
 *	This routine handles "block", "leaf", and "node" type directories.
 *	It calls the _xfs_dirv2_data_lookup_int() routine to search each block
 *	of the directory.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
static int
_xfs_dirv2_data_lookup(
	FSBLOCK			*fsb,
	xfs_da_args_t		*args)
{
	int			retval;
	char			*buf;
	xfs_dir2_db_t		dbno;
	ulong			len;
	xfs_inode_t		*dp = args->dp;
	xfs_mount_t		*mp = fsb_to_mount(fsb);
	xfs_fsblock_t		bno, numblocks;
	xfs_da_intnode_t	*node;

	/*
	 * Allocate buffer to read inode blocks.
	 */
        len = mp->m_dirblksize;
        if ((buf = dmabuf_malloc(len)) == 0) {
                printf("xfs: could not allocate memory for dir data buffer.\n");
		return 0;
        }

	/*
	 * For each logical block in the directory, do a
	 * data lookup until the desired file is found.
 	 */
	numblocks = dp->i_d.di_size / mp->m_dirblksize;
	
        for (dbno = 0; dbno < numblocks; dbno++) {

		/*
		 * Read this directory block.
		 */
		if (!_xfs_dirv2_read_block(fsb, dp, dbno, buf, &bno)) {
			printf("xfs: read of directory data block failed.\n");
			retval = 0;
			break;
		}

		/*
		 * There could be a hole in the directory file.
		 */
                if (bno == NULLFSBLOCK) {
                        continue;
		}

		/*
		 * Check for the desired file in this data block.
		 */
		retval = _xfs_dirv2_data_lookup_int(fsb, buf, args);
		if (retval) 
			break;
        }

	dmabuf_free(buf);

	return retval;
}

/*
 * _xfs_dirv2_data_lookup_int()
 *	Look up a name in a directory data/block structure.
 *	Return the file information in the args structure.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
/*ARGSUSED*/
static int
_xfs_dirv2_data_lookup_int(
	FSBLOCK			*fsb, 
	char			*buf, 
	xfs_da_args_t		*args)
{
	xfs_mount_t		*mp;
	xfs_dir2_data_t		*data;
	xfs_dir2_block_tail_t	*btp;
	char			*ptr, *endptr;
	xfs_dir2_data_unused_t	*dup;
	xfs_dir2_data_entry_t	*dep;

	mp = fsb_to_mount(fsb);
	data = (xfs_dir2_data_t *)buf;
	ptr = (char *)data->u;
	if (data->hdr.magic == XFS_DIR2_DATA_MAGIC)
		endptr = buf + mp->m_dirblksize;
	else if (data->hdr.magic == XFS_DIR2_BLOCK_MAGIC) {
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, (xfs_dir2_block_t *)buf);
		endptr = (char *)XFS_DIR2_BLOCK_LEAF_P(btp);
	} else
		return 0;
	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (dup->freetag == XFS_DIR2_DATA_FREE_TAG) {
			ptr += dup->length;
			continue;
		}
		dep = (xfs_dir2_data_entry_t *)dup;
		if (dep->namelen == args->namelen &&
		    bcmp(dep->name, args->name, args->namelen) == 0) {
			args->inumber = dep->inumber;
			return 1;
		}
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
	}
	return 0;
}

/*
 * _xfs_dirv2_read_block()
 *	Map and read the given directory block from the inode.
 *
 *  RETURNS:
 *	1 on success
 *	0 on error
 *  bnop is filled in with the first fsblock or NULLFSBLOCK if it's a hole
 */
static int
_xfs_dirv2_read_block(
	FSBLOCK		*fsb,
	xfs_inode_t	*dp,
	xfs_dir2_db_t	dbno,
	char		*buf,
	xfs_fsblock_t	*bnop)
{
	xfs_fsblock_t	bno;
	int		len;
	xfs_mount_t	*mp = fsb_to_mount(fsb);
	xfs_fileoff_t	off;
	int		ret;
	long		rlen;

	off = XFS_DIR2_DB_TO_DA(mp, dbno);
	for (len = 0; len < mp->m_dirblkfsbs; len += rlen) {
		bno = _xfs_get_blockno(mp, dp, off, &rlen);
		if (bno == NULLFSBLOCK) {
			*bnop = NULLFSBLOCK;
			return 1;
		}
		if (len == 0)
			*bnop = bno;
		ret = xfs_lbread(fsb, XFS_FSB_TO_DADDR(mp, bno), buf,
			XFS_FSB_TO_B(mp, rlen), 1);
		if (ret == 0)
			return 0;
		buf += XFS_FSB_TO_B(mp, rlen);
		off += rlen;
	}
	return 1;
}

/*
 * _xfs_dirv2_getdent()
 *	Return the next directory entry from the directory pointed to 
 *	by fsb in the dep structure. This routine determines what type
 *	of directory is being accessed and calls the appropriate function.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
int
_xfs_dirv2_getdent(
	FSBLOCK		*fsb,
	xfs_dirent_t	*dep)
{
	xfs_inode_t	*dp = fsb_to_inode(fsb);
	int		retval;

	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL) {
		retval = _xfs_dirv2_sf_getdent(fsb, dep);
	} else {
		retval = _xfs_dirv2_data_getdent(fsb, dep);
	}
	return retval;
}

/*
 * _xfs_dirv2_data_getdent()
 *	Return the next directory entry from the directory pointed to 
 *	by fsb in the dep structure. This routine applies to non-shortform
 *	directories.
 *
 * RETURNS:
 *	1 on success
 *	0 on error
 */
static int
_xfs_dirv2_data_getdent(
	FSBLOCK			*fsb,
	xfs_dirent_t		*direntp)
{
	xfs_dir2_data_t		*data;
	int			boff;
	off_t			offset;
        IOBLOCK 		*iob = fsb->IO;
	xfs_inode_t		*dp  = fsb_to_inode(fsb);
	xfs_mount_t		*mp  = fsb_to_mount(fsb);
	xfs_fsblock_t		bno, numblocks;
	xfs_dir2_db_t		db;
	xfs_dir2_data_unused_t	*dup;
	xfs_dir2_data_entry_t	*dep;
	char			*ptr, *endptr;
	xfs_dir2_block_tail_t	*btp;

	offset = iob->Offset.lo;
	numblocks = dp->i_d.di_size / mp->m_dirblksize;
	db = XFS_DIR2_BYTE_TO_DB(mp, offset);
	boff = XFS_DIR2_BYTE_TO_OFF(mp, offset);
        if ((data = dmabuf_malloc(mp->m_dirblksize)) == NULL) {
                printf("xfs: could not allocate memory for dir data buffer.\n");
		return 0;
        }
	while (db < numblocks) {
		if (!_xfs_dirv2_read_block(fsb, dp, db, (char *)data, &bno)) {
			printf("xfs: error reading dir data block\n");
			return 0;
		}
		if (bno == NULLFSBLOCK) {
			db++;
			boff = 0;
			continue;
		}
		ptr = (char *)data->u;
		if (data->hdr.magic == XFS_DIR2_DATA_MAGIC)
			endptr = (char *)data + mp->m_dirblksize;
		else {
			btp = XFS_DIR2_BLOCK_TAIL_P(mp,
				(xfs_dir2_block_t *)data);
			endptr = (char *)XFS_DIR2_BLOCK_LEAF_P(btp);
		}
		while (ptr < endptr) {
			dup = (xfs_dir2_data_unused_t *)ptr;
			if (dup->freetag == XFS_DIR2_DATA_FREE_TAG) {
				ptr += dup->length;
				continue;
			}
			dep = (xfs_dir2_data_entry_t *)dup;
			if ((char *)dep - (char *)data < boff) {
				ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
				continue;
			}
			bcopy(dep->name, direntp->name, dep->namelen);
			direntp->ino = dep->inumber;
			direntp->namelen = dep->namelen;
			ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
			iob->Offset.lo =
				XFS_DIR2_DB_OFF_TO_BYTE(mp, db,
					ptr - (char *)data);
			dmabuf_free(data);
			return 1;
		}
		db++;
		boff = 0;
	}
	dmabuf_free(data);
        return 0;
}

/*
 * _xfs_dirv2_sf_getdent()
 *	Return the next directory entry from the directory pointed to 
 *	by fsb in the dep structure. This routine is specific to "shortform"
 *	directories.
 *
 * RETURNS:
 *	1 on success
 *	0 on error
 */
static int
_xfs_dirv2_sf_getdent(
	FSBLOCK			*fsb,
	xfs_dirent_t		*dep)
{
	int			i;
	off_t			offset;
        IOBLOCK 		*iob = fsb->IO;
	xfs_inode_t		*dp  = fsb_to_inode(fsb);
	xfs_dir2_sf_entry_t	*sfe;
	xfs_dir2_sf_t		*sf;

	offset = (xfs_fsize_t)iob->Offset.lo;
	sf = (xfs_dir2_sf_t *)dp->i_df.if_u1.if_data;
	if (offset <= XFS_DIR2_DATA_DOT_OFFSET) {
		bcopy(".", dep->name, 1);
		dep->ino = dp->i_ino;
		dep->namelen = 1;
		iob->Offset.lo = XFS_DIR2_DATA_DOTDOT_OFFSET;
		return 1;
	}
	if (offset <= XFS_DIR2_DATA_DOTDOT_OFFSET) {
		bcopy("..", dep->name, 2);
		dep->ino = XFS_DIR2_SF_GET_INUMBER(sf, &sf->hdr.parent);
		dep->namelen = 2;
		iob->Offset.lo = XFS_DIR2_DATA_FIRST_OFFSET;
		return 1;
	}
	for (i = 0, sfe = XFS_DIR2_SF_FIRSTENTRY(sf);
	     i < sf->hdr.count;
	     i++, sfe = XFS_DIR2_SF_NEXTENTRY(sf, sfe)) {
		if (offset > XFS_DIR2_SF_GET_OFFSET(sfe))
			continue;
		bcopy(sfe->name, dep->name, sfe->namelen);
		dep->ino =
			XFS_DIR2_SF_GET_INUMBER(sf, XFS_DIR2_SF_INUMBERP(sfe));
		dep->namelen = sfe->namelen;
		iob->Offset.lo =
			XFS_DIR2_SF_GET_OFFSET(sfe) +
			XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		return 1;
	} 
        return 0;

}
