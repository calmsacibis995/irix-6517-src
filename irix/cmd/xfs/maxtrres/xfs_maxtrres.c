#ident "$Revision: 1.2 $"

/*
 * maxtrres
 * Compute the maximum transaction reservation for every legal
 * combination of block size, inode size, directory version, 
 * and directory block size.
 * Generates a table compiled into mkfs_xfs, to control the
 * default and minimum log sizes.
 */

#define	_KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL
#include <sys/uuid.h>
#include <sys/vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/fs/xfs_macros.h>
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
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_trans_space.h>
#include <sys/fs/xfs_attr_leaf.h>
#include "xfs_mkfs.h"

extern int xfs_mountfs_int(vfs_t *, xfs_mount_t *, dev_t, int, int);
static int max_trans_res(xfs_mount_t *, int *);

main(int argc, char **argv)
{
	int		bl;
	buf_t		buf;
	int		dl;
	int		dv;
	int		i;
	int		il;
	xfs_mount_t	m;
	xfs_sb_t	*sbp;
	vfs_t		vfs;
	int		mul;

	if (argc > 1) {
		fprintf(stderr, "usage: xfs_maxtrres\n");
		return 1;
	}
	memset(&m, 0, sizeof(m));
	memset(&vfs, 0, sizeof(vfs));
	m.m_sb_bp = &buf;
	sbp = &m.m_sb;
	sbp->sb_magicnum = XFS_SB_MAGIC;
	sbp->sb_sectlog = 9;
	sbp->sb_sectsize = 1 << sbp->sb_sectlog;
	for (bl = XFS_MIN_BLOCKSIZE_LOG; bl <= XFS_MAX_BLOCKSIZE_LOG; bl++) {
		sbp->sb_blocklog = bl;
		sbp->sb_blocksize = 1 << bl;
		sbp->sb_agblocks = XFS_AG_MIN_BYTES / (1 << bl);
		for (il = XFS_DINODE_MIN_LOG; il <= XFS_DINODE_MAX_LOG; il++) {
			if ((1 << il) > (1 << bl) / XFS_MIN_INODE_PERBLOCK)
				continue;
			sbp->sb_inodelog = il;
			sbp->sb_inopblog = bl - il;
			sbp->sb_inodesize = 1 << il;
			sbp->sb_inopblock = 1 << (bl - il);
			for (dl = bl; dl <= XFS_MAX_BLOCKSIZE_LOG; dl++) {
				sbp->sb_dirblklog = dl - bl;
				for (dv = 1; dv <= 2; dv++) {
					if (dv == 1 && dl != bl)
						continue;
					sbp->sb_versionnum =
						XFS_SB_VERSION_4 |
						(dv == 2 ?
						    XFS_SB_VERSION_DIRV2BIT :
						    0);
					xfs_mountfs_int(&vfs, &m, 0, 0, 0);
					i = max_trans_res(&m, &mul);
					printf(
				"#define\tMAXTRRES_B%d_I%d_D%d_V%d\t%lld\t"
				"/* LOG_FACTOR %d */\n",
						bl, il, dl, dv,
						XFS_B_TO_FSB(&m, i), mul);
				}
			}
		}
	}
	return 0;
}


xfs_trans_reservations_t tr_count = {
	XFS_WRITE_LOG_COUNT,		/* extent alloc trans */
	XFS_ITRUNCATE_LOG_COUNT,	/* truncate trans */
	XFS_RENAME_LOG_COUNT,		/* rename trans */
	XFS_LINK_LOG_COUNT,		/* link trans */
	XFS_REMOVE_LOG_COUNT,		/* unlink trans */
	XFS_SYMLINK_LOG_COUNT,		/* symlink trans */
	XFS_CREATE_LOG_COUNT,		/* create trans */
	XFS_MKDIR_LOG_COUNT,		/* mkdir trans */
	XFS_DEFAULT_LOG_COUNT,		/* inode free trans */
	XFS_DEFAULT_LOG_COUNT,		/* inode update trans */
	XFS_DEFAULT_LOG_COUNT,		/* fs data section grow trans */
	XFS_DEFAULT_LOG_COUNT,		/* sync write inode trans */
	XFS_ADDAFORK_LOG_COUNT,		/* cvt inode to attributed trans */
	XFS_DEFAULT_LOG_COUNT,		/* write setuid/setgid file */
	XFS_ATTRINVAL_LOG_COUNT,	/* attr fork buffer invalidation */
	XFS_ATTRSET_LOG_COUNT,		/* set/create an attribute */
	XFS_ATTRRM_LOG_COUNT,		/* remove an attribute */
	XFS_DEFAULT_LOG_COUNT,		/* clear bad agi unlinked ino bucket */
	XFS_DEFAULT_PERM_LOG_COUNT,	/* grow realtime allocations */
	XFS_DEFAULT_LOG_COUNT,		/* grow realtime zeroing */
	XFS_DEFAULT_LOG_COUNT,		/* grow realtime freeing */
};

static int
max_trans_res(
	xfs_mount_t			*mp,
	int				*mul)
{
	uint				*p;
	uint				*q;
	int				rval;
	xfs_trans_reservations_t	*tr;
	xfs_da_args_t 			args;
	int				local;
	int				size;
	int				nblks = XFS_DAENTER_SPACE_RES(mp, XFS_ATTR_FORK);
	int				res;
	int xfs_attr_leaf_newentsize(xfs_da_args_t *args, int blocksize, int *local);

	/*
	 * Fill in the arg structure for this request.
	 */
	bzero((char *)&args, sizeof(args));
	args.name = NULL;
	args.namelen = MAXNAMELEN;
	args.value = NULL;
	args.valuelen = 65536;
	args.flags = 0;
	args.hashval = 0;
	args.dp = NULL;
	args.firstblock = NULL;
	args.flist = NULL;
	args.whichfork = XFS_ATTR_FORK;
	args.oknoent = 1;

	/* Determine space new attribute will use, and if it will be inline
	 * or out of line.
	 */
	size = xfs_attr_leaf_newentsize(&args, mp->m_sb.sb_blocksize, &local);

	if (local) {
		printf("Uh-oh.. attribute is local\n");
	} else {
		/* Out of line attribute, cannot double split, but make
		 * room for the attribute value itself.
		 */
		nblks += XFS_B_TO_FSB(mp, size);
		nblks += XFS_NEXTENTADD_SPACE_RES(mp, size, XFS_ATTR_FORK);
	}
	res = XFS_ATTRSET_LOG_RES(mp, nblks);
#if 0
	printf("size = %d nblks = %d res = %d\n",
		size, nblks, res);
#endif
	mp->m_reservations.tr_attrset = res;

	for (rval = 0, tr = &mp->m_reservations, p = (uint *)tr,
	     q = (uint *)&tr_count;
	     p < (uint *)(tr + 1);
	     p++, q++) {
		if ((int)*p > rval) {
			rval = (int)*p;
			*mul = (int)*q;
		}
	}
	return rval;
}

/*
 * THIS IS COPIED FROM xfs_attr_leaf.c
 * copying was easer than getting that file to compile as part of the
 * simulation library
 */

/*
 * Calculate the number of bytes that would be required to store the new
 * attribute (whether local or remote only calculate bytes in this block).
 * This routine decides as a side effect whether the attribute will be
 * a "local" or a "remote" attribute.
 */
int
xfs_attr_leaf_newentsize(xfs_da_args_t *args, int blocksize, int *local)
{
	int size;

	size = XFS_ATTR_LEAF_ENTSIZE_LOCAL(args->namelen, args->valuelen);
	if (size < XFS_ATTR_LEAF_ENTSIZE_LOCAL_MAX(blocksize)) { 
		if (local) {
			*local = 1;
		}
	} else {
		size = XFS_ATTR_LEAF_ENTSIZE_REMOTE(args->namelen);
		if (local) {
			*local = 0;
		}
	}
	return(size);
}
