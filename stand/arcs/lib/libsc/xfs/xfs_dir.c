#ident "$Revision: 1.17 $"

/*
 * xfs_dir.c
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
#include <sys/fs/xfs_dir_leaf.h>
#include <sys/fs/xfs_error.h>
#include <sys/fs/xfs_bit.h>

#include "xfs_local.h"

static int _xfs_dirv1_leaf_lookup(FSBLOCK *, xfs_da_args_t *);
static int _xfs_dirv1_leaf_lookup_int(FSBLOCK *, char *, xfs_da_args_t *,
				      int *);
static int _xfs_dirv1_node_lookup(FSBLOCK *, xfs_da_args_t *);
static int _xfs_dirv1_read_buf(FSBLOCK *, xfs_inode_t *, xfs_fsblock_t, char *);
static int _xfs_dirv1_shortform_lookup(xfs_da_args_t *);
static int _xfs_dirv1_leaf_getdent(FSBLOCK *, xfs_dirent_t *);
static int _xfs_dirv1_node_getdent(FSBLOCK *, xfs_dirent_t *);
static int _xfs_dirv1_shortform_getdent(FSBLOCK *, xfs_dirent_t *);

extern int xfs_lbread(FSBLOCK *, daddr_t, void *, long, int);
extern xfs_fsblock_t _xfs_get_blockno(xfs_mount_t *, xfs_inode_t *,
	xfs_fileoff_t, long *);

/*
 * xfs_dir_hashname()
 *	Create the hash value for the given XFS file name.
 *
 * RETURNS:
 *	hash value for file name
 */
uint
xfs_dir_hashname(
	char	*name,
	int	namelen)
{
        uint	hash;

        hash = 0;
        for (  ; namelen > 0; namelen--) {
                hash = *name++ ^ ((hash << 7) | (hash >> (32-7)));
        }
        return(hash);
}

/*
 * _xfs_dirv1_lookup()
 * 	Get the inode number of the file with the given name in the
 * 	given directory.
 * 
 *  RETURNS:
 *	1 on success 
 * 	0 on failure
 */
int
_xfs_dirv1_lookup(
	FSBLOCK			*fsb,
	xfs_inode_t		*dp, 
	char 			*name, 
	int			namelen,
   	xfs_ino_t		*inum)
{
	int 			retval;
	xfs_mount_t		*mp;
	xfs_dinode_core_t	*dic;
	xfs_da_args_t		args;

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
	args.firstblock = NULL;
	args.flist      = NULL;
	args.total      = 0;

	mp = dp->i_mount;
	dic = &(dp->i_d);

	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dic->di_size <= XFS_IFORK_DSIZE(dp)) {
		retval = _xfs_dirv1_shortform_lookup(&args );
	} else if (dic->di_size == XFS_LBSIZE(mp)) {
		retval = _xfs_dirv1_leaf_lookup( fsb, &args );
	} else {
		retval = _xfs_dirv1_node_lookup( fsb, &args );
	}

	*inum = args.inumber;

	return retval ;
}


/*
 * _xfs_dirv1_shortform_lookup()
 *	Find the inode number of the file with the given name.
 *	This routine handles "shortform" type directories.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
static int
_xfs_dirv1_shortform_lookup(
	xfs_da_args_t		*args)
{
	int			i;
	xfs_inode_t		*dp;
	xfs_dir_sf_entry_t	*sfe;
	xfs_dir_shortform_t	*sf;

	dp = args->dp;

	if ((dp->i_df.if_flags & XFS_IFINLINE) == 0) {
		printf("xfs: invalid shortform inode.\n");
		return 0;
	}

	sf = (struct xfs_dir_shortform *)dp->i_df.if_u1.if_data;

	/*
	 * Check for the ".." entry.
 	 */
	if (args->namelen == 2 &&
	    args->name[0] == '.' && args->name[1] == '.') {
		bcopy((char *)&sf->hdr.parent, 
			(char *)&args->inumber, sizeof(xfs_ino_t));
		return 1 ;
	}

	/*
	 * Check for the "." entry.
 	 */
	if (args->namelen == 1 && args->name[0] == '.') {
		args->inumber = dp->i_ino;
		return 1;
	}

	sfe = &sf->list[0];
	for (i = sf->hdr.count-1; i >= 0; i--) {
		if (sfe->namelen == args->namelen) {
			if (bcmp(args->name, sfe->name, args->namelen) == 0) {
				bcopy((char *)&sfe->inumber,
					(char *)&args->inumber,
					sizeof(xfs_ino_t));
				return 1;
			}
		}
		sfe = XFS_DIR_SF_NEXTENTRY(sfe);
	}
	return 0 ;
}


/*
 * _xfs_dirv1_leaf_lookup()
 *	Find the inode number of the file with the given name.
 *	This routine handles "leaf" type directories.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
static int
_xfs_dirv1_leaf_lookup(
	FSBLOCK		*fsb,
	xfs_da_args_t 	*args)
{
	int 		index, retval;
	char		*buf;
	ulong		len;
	xfs_mount_t 	*mp = fsb_to_mount(fsb);
	xfs_inode_t	*ip = fsb_to_inode(fsb);
	xfs_fsblock_t	bno;
	xfs_fileoff_t	off;

	/*
	 * Allocate a buffer to read the contents of the directory.
	 */
	len = BBTOB(BTOBB( (XFS_FSB_TO_B(mp,1))) );
	if ((buf = dmabuf_malloc(len)) == 0) {
		printf("xfs: could not allocate memory for leaf buffer.\n");
		return 0 ;
	}

	off = 0;
	bno = _xfs_get_blockno( mp, ip, off, NULL);

	if (bno == NULLFSBLOCK) {
		printf("xfs: could not get block 0 of leaf directory.\n");
		return 0;
	}

	if (!_xfs_dirv1_read_buf( fsb, args->dp, bno, buf)) {
		printf("xfs: could not read block 0 of leaf directory.\n");
		return 0;
	}
	
	retval = _xfs_dirv1_leaf_lookup_int(fsb, buf, args, &index);

	dmabuf_free(buf);

	return(retval);
}


/*
 * _xfs_dirv1_node_lookup()
 *	Find the inode number of the file with the given name.
 *	This routine handles "node" type directories it calls the 
 *  	_xfs_dirv1_leaf_lookup_int() routine to search each block of the 
 *	directory.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
static int
_xfs_dirv1_node_lookup(
	FSBLOCK			*fsb,
	xfs_da_args_t		*args)
{
	int			index, retval;
	char			*buf;
	xfs_fileoff_t		dbno;
	ulong			len;
	xfs_mount_t		*mp = fsb_to_mount(fsb);
	xfs_inode_t		*ip = fsb_to_inode(fsb);
	xfs_fsblock_t		bno, numblocks;
	xfs_da_intnode_t	*node;

	/*
	 * Allocate buffer to read inode blocks.
	 */
        len = BBTOB(BTOBB( (XFS_FSB_TO_B(mp,1))) );
        if ((buf = dmabuf_malloc(len)) == 0) {
                printf("xfs: could not allocate memory for node buffer.\n");
		return 0;
        }

	/*
	 * For each block in the directory, do a
	 * leaf lookup until the desired file is found.
 	 */
	numblocks = (xfs_fsblock_t)((ip->i_d.di_size + mp->m_sb.sb_blocksize -1) /
			mp->m_sb.sb_blocksize);
	
        for (dbno = 0; dbno < numblocks ; dbno++) {

	 	bno = _xfs_get_blockno( mp, ip, dbno, NULL);

                if (bno == NULLFSBLOCK && dbno == 0) {
			printf("xfs: could not get node directory block 0.\n");
			return 0;
                }

		/*
		 * There could be a hole in the directory file.
		 */
                if (bno == NULLFSBLOCK) {
                        continue;
		}

		/*
		 * Read this directory block.
		 */
		if (!_xfs_dirv1_read_buf( fsb, args->dp, bno, buf)) {
			printf("xfs: read of node directory block failed.\n");
			return 0;
		}

		/*
	 	 * If this is a directory block, skip it.
		 */
                node = (struct xfs_da_intnode *)buf;
                if (node->hdr.info.magic == XFS_DA_NODE_MAGIC) {
                        continue;
                }

		/*
		 * Check of the desired file in this leaf block.
		 */
		retval = _xfs_dirv1_leaf_lookup_int(fsb, buf, args, &index);
		if (retval) 
			break;
        }


	dmabuf_free(buf);

	return(retval);
}


/*
 * _xfs_dirv1_read_buf()
 *	Read the given directory block from the inode.
 *
 * RETURNS:
 *	1 on success
 * 	0 on error
 */
/*ARGSUSED*/
static int
_xfs_dirv1_read_buf(
	FSBLOCK		*fsb,
	xfs_inode_t	*dp, 
	xfs_fsblock_t	bno,
	char		*buf)
{
	int		ret;
	xfs_mount_t	*mp = fsb_to_mount(fsb);

	ret = xfs_lbread(fsb, 
		XFS_FSB_TO_DADDR(mp, bno), buf, XFS_FSB_TO_B(mp,1), 1);

	return ret;
}

/*
 * _xfs_dirv1_getdent()
 *	Return the next directory entry from the directory pointed to 
 *	by fsb in the dep structure. This routine determines what type
 *	of directory is being accessed and calls the appropriate function.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
int
_xfs_dirv1_getdent(
	FSBLOCK		*fsb,
	xfs_dirent_t	*dep)
{
	int		ret;
	xfs_inode_t	*ip = fsb_to_inode(fsb);
	xfs_mount_t	*mp = fsb_to_mount(fsb);

	if (ip->i_d.di_size <= XFS_IFORK_DSIZE(ip)) {
		ret = _xfs_dirv1_shortform_getdent(fsb, dep);
	} else if (ip->i_d.di_size == XFS_LBSIZE(mp)) {
		ret = _xfs_dirv1_leaf_getdent(fsb, dep);
	} else {
		ret = _xfs_dirv1_node_getdent(fsb, dep);
	}
	return ret ;
}

/*
 * _xfs_dirv1_leaf_getdent()
 *	Return the next directory entry from the directory pointed to 
 *	by fsb in the dep structure. This routine is specific to "leaf"
 *	directories.
 *
 * RETURNS:
 *	1 on success
 *	0 on error
 */
static int
_xfs_dirv1_leaf_getdent(
	FSBLOCK			*fsb,
	xfs_dirent_t		*dep)
{
	xfs_fileoff_t		off;
	ulong			index;
        IOBLOCK 		*iob = fsb->IO;
	xfs_inode_t		*ip  = fsb_to_inode(fsb);
	xfs_mount_t		*mp  = fsb_to_mount(fsb);
	xfs_fsblock_t		bno;
	xfs_dir_leafblock_t	*leaf;
	xfs_dir_leaf_entry_t	*entry;
	xfs_dir_leaf_name_t	*namest;

	index = iob->Offset.lo;

	/*
	 * If this is the first time this routine is being called
	 * for this directory, read in the contents of the directory.
 	 */
	if (index == 0) {
		off = 0;
		bno = _xfs_get_blockno( mp, ip, off, NULL);
		xfs_lbread( fsb, XFS_FSB_TO_DADDR(mp, bno), fsb->Buffer, 
			XFS_FSB_TO_B(mp,1),1);
	}
	
	leaf = (struct xfs_dir_leafblock *)fsb->Buffer;

	/*
	 * Check if this is a leaf directory.
 	 */
	if (leaf->hdr.info.magic != XFS_DIR_LEAF_MAGIC) {
		printf("xfs: directory has invalid leaf magic number.\n");
		return 0;
	} 

	/*
 	 * Convert iob->Offset.lo to an index in the leaf.
	 */
	index = iob->Offset.lo;

	if (index >= leaf->hdr.count) {
		;
	} else {
		entry = &leaf->entries[index];
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, entry->nameidx);
		bcopy(namest->name, dep->name, entry->namelen);
		dep->namelen =  entry->namelen;
		bcopy((char *)&namest->inumber, &dep->ino, sizeof(xfs_ino_t));
		iob->Offset.lo++;
		return 1;
	}

        return 0;
}

/*
 * _xfs_dirv1_node_getdent()
 *	Return the next directory entry from the directory pointed to 
 *	by fsb in the dep structure. This routine is specific to "node"
 *	directories.
 * NOTE:
 *	This is a very inefficient algorithm. There is lots of redundant
 *	reading of file blocks. It was done in this manner so that as
 *	little state information as possible is being passed between
 *	subsequent calls to xfs_dirv1_node_getdent().
 *
 * RETURNS:
 *	1 on success
 *	0 on error
 */
static int
_xfs_dirv1_node_getdent(
	FSBLOCK			*fsb,
	xfs_dirent_t		*dep)
{
        int 			totalcount;
	xfs_fileoff_t		whichblock;
	ulong			index;
        IOBLOCK 		*iob = fsb->IO;
	xfs_inode_t		*ip  = fsb_to_inode(fsb);
	xfs_mount_t		*mp  = fsb_to_mount(fsb);
	xfs_fsblock_t		bno, numblocks;
	xfs_dir_leafblock_t	*leaf;
	xfs_dir_leaf_entry_t	*entry;
	xfs_dir_leaf_name_t	*namest;
	xfs_da_intnode_t	*node;

	index      = iob->Offset.lo;
	totalcount = 0;
	whichblock = 0;
	numblocks = (xfs_fsblock_t)((ip->i_d.di_size + mp->m_sb.sb_blocksize -1) /
			mp->m_sb.sb_blocksize);

	/*
	 * Find the correct block in the directory
	 * which has the entry for the file with the desired index.
	 */
	while ( totalcount <= index ) {

		/*
		 * Request index is out of range.
		 */
		if (whichblock > numblocks) {
			return 0;
		}

		/*
		 * Get the file system block corresponding 
		 * to the logical file offset.
		 */
		bno = _xfs_get_blockno( mp, ip, whichblock, NULL);
		whichblock++;
	
		if (bno == NULLFSBLOCK) {
			continue;
		}
	
		xfs_lbread( fsb, XFS_FSB_TO_DADDR(mp, bno), fsb->Buffer, 
			XFS_FSB_TO_B(mp,1),1);
	
		node = (struct xfs_da_intnode *)fsb->Buffer;

		/*
		 * If this a directory node entry - skip it.
		 */
		if (node->hdr.info.magic == XFS_DA_NODE_MAGIC) {
			continue;
		}
	
		leaf = (struct xfs_dir_leafblock *)fsb->Buffer;

		if (leaf->hdr.info.magic != XFS_DIR_LEAF_MAGIC) {
			printf("xfs: invalid leaf magic number. \n");
			return 0;
		} 
		totalcount += leaf->hdr.count;
	}

	totalcount -= leaf->hdr.count;
	
	/*
 	 * Convert iob->Offset.lo to the correct offset
	 * relative to this directory block.
	 */
	index = iob->Offset.lo;
	index -= totalcount;

	if (index >= leaf->hdr.count) {
		;
	} else {
		entry = &leaf->entries[index];
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, entry->nameidx);
		bcopy(namest->name, dep->name, entry->namelen);
		dep->namelen =  entry->namelen;
		bcopy((char *)&namest->inumber, &dep->ino, sizeof(xfs_ino_t));
		iob->Offset.lo++;
		return 1;
	}

        return 0;
}


/*
 * _xfs_dirv1_shortform_getdent()
 *	Return the next directory entry from the directory pointed to 
 *	by fsb in the dep structure. This routine is specific to "shortform"
 *	directories.
 *
 * RETURNS:
 *	1 on success
 *	0 on error
 */
static int
_xfs_dirv1_shortform_getdent(
	FSBLOCK			*fsb,
	xfs_dirent_t		*dep)
{
        int 			i = 0;
	ulong			index;
        IOBLOCK 		*iob = fsb->IO;
	xfs_inode_t		*ip  = fsb_to_inode(fsb);
	xfs_dir_sf_entry_t	*sfe;
	xfs_dir_shortform_t	*sf;

	index = iob->Offset.lo;

	sf = (struct xfs_dir_shortform *)(ip->i_df.if_u1.if_data);

	if (index < sf->hdr.count) {
        	sfe = &sf->list[0];
		while (i++ < index) sfe = XFS_DIR_SF_NEXTENTRY(sfe);
	
		bcopy(sfe->name, dep->name, sfe->namelen);
		bcopy((char *)&sfe->inumber, &dep->ino, sizeof(xfs_ino_t));
		dep->namelen = sfe->namelen;
		iob->Offset.lo++;
		return 1;
	} 

        return 0;

}

/*
 * _xfs_dirv1_leaf_lookup_int()
 *	Look up a name in a leaf directory structure.
 *	Return the file information in the args structure.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
/*ARGSUSED*/
static int
_xfs_dirv1_leaf_lookup_int(
	FSBLOCK			*fsb, 
	char			*buf, 
	xfs_da_args_t		*args, 
	int 			*index)
{
	int 			probe, span, i;
	xfs_dir_leafblock_t	*leaf;
	xfs_dir_leaf_name_t	*namest;
	xfs_dir_leaf_entry_t	*entry;

	leaf = (struct xfs_dir_leafblock *)buf;
	if (!(leaf->hdr.info.magic == XFS_DIR_LEAF_MAGIC)) {
		printf("xfs: invalid leaf magic number \n");
		return 0;
	} 

	/*
	 * Only bother with a binary search if more than a few entries.
	 */
	if (leaf->hdr.count < 64) {
		/*
		 * Linear search...
		 */
		entry = &leaf->entries[0];
		for (i = 0; i < leaf->hdr.count; entry++, i++) {

			if (entry->hashval > args->hashval) {
				break;
			}
			if (entry->hashval == args->hashval) {
				namest = XFS_DIR_LEAF_NAMESTRUCT(leaf,
							     entry->nameidx);
				if ((entry->namelen == args->namelen) &&
				    (bcmp(args->name, namest->name,
						      args->namelen) == 0)) {
					*index = i;
					bcopy((char *)&namest->inumber,
					      (char *)&args->inumber,
					      sizeof(xfs_ino_t));
					return 1 ;
				}
			}
		}
		*index = i;
		return 0;
	}

	/*
	 * Binary search...
	 */
	probe = span = leaf->hdr.count / 2;
	do {
		span /= 2;
		entry = &leaf->entries[probe];
		if (entry->hashval < args->hashval)
			probe += span;
		else if (entry->hashval > args->hashval)
			probe -= span;
		else
			break;
	} while (span > 4);
	entry = &leaf->entries[probe];

	/*
	 * Binary search on a random number of elements will only get
	 * you close, so we must search around a bit more by hand.
	 */
	while ((probe > 0) && (entry->hashval > args->hashval)) {
		entry--;
		probe--;
	}
	while ((probe < leaf->hdr.count) && (entry->hashval < args->hashval)) {
		entry++;
		probe++;
	}
	if ((probe == leaf->hdr.count) || (entry->hashval != args->hashval)) {
		*index = probe;
		return 0 ;
	}

	/*
	 * Duplicate keys may be present, so search all of them for a match.
	 */
	while ((probe >= 0) && (entry->hashval == args->hashval)) {
		entry--;
		probe--;
	}
	entry++;			/* loop left us 1 below last match */
	probe++;
	while ((probe < leaf->hdr.count) && (entry->hashval == args->hashval)) {
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, entry->nameidx);
		if ((entry->namelen == args->namelen) &&
		    (bcmp(args->name, namest->name, args->namelen) == 0)) {
			bcopy((char *)&namest->inumber, (char *)&args->inumber,
					       sizeof(xfs_ino_t));
			*index = probe;
			return 1;
		}
		entry++;
		probe++;
	}
	*index = probe;
	return 0;
}
