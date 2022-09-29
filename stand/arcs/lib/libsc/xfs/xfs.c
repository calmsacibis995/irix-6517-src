#ident	"$Revision: 1.23 $"

/*
 * xfs.c
 *	XFS stand alone libarary routines.
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

#include <sys/uuid.h>
#include <sys/buf.h>
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
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_buf_item.h>
#include <sys/fs/xfs_rw.h>
#include <sys/fs/xfs_rtalloc.h>
#include <sys/fs/xfs_error.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_dir2_data.h>
#include <sys/fs/xfs_dir2_leaf.h>
#include "xfs_local.h"


extern int _xfs_iread_extents(FSBLOCK *, xfs_inode_t *);
extern int _xfs_ifree(xfs_inode_t *);
extern xfs_fsblock_t _xfs_get_blockno(xfs_mount_t *, xfs_inode_t *,
	xfs_fileoff_t, long *);
extern int  xfs_highbit32(__uint32_t );
extern int  _xfs_dirv1_lookup(FSBLOCK *, xfs_inode_t *, char *, int,
	xfs_ino_t *);
extern int  _xfs_dirv2_lookup(FSBLOCK *, xfs_inode_t *, char *, int,
	xfs_ino_t *);
extern int  _xfs_dirv1_getdent(FSBLOCK *, xfs_dirent_t *);
extern int  _xfs_dirv2_getdent(FSBLOCK *, xfs_dirent_t *);
extern void xfs_iformat(xfs_mount_t *, xfs_inode_t *, xfs_dinode_t *);
extern void process_bmbt_reclist(xfs_bmbt_rec_t  *, int);

static int _xfs_strat(FSBLOCK *);
static int _xfs_checkfs(FSBLOCK *);
static int _xfs_open(FSBLOCK *);
static int _xfs_close(FSBLOCK *);
static int _xfs_read(FSBLOCK *);
static int _xfs_getdirent(FSBLOCK *);
static int _xfs_grs(FSBLOCK *);
static int _xfs_dir_lookup(FSBLOCK *, xfs_inode_t *, char *, int, xfs_ino_t *);
static int _xfs_lookup_name(FSBLOCK *, char *);
static int _xfs_getdent(FSBLOCK *, xfs_dirent_t *);

#define _min(a,b)	( (a < b) ? a : b )

/*
 * xfs_install - register the XFS filesystem to the 
 * standalone kernel and initialize any variables
 */
int
xfs_install(void)
{
	/*
	 * HACK:
	 *	The XFS file system is registered as type DTFS_EFS so
	 * that the prom routine fs_search() will check for XFS style
	 * file systems. The correct fix is to change the prom code.
 	 */
	return fs_register (_xfs_strat, "xfs", DTFS_EFS);
}

/*
 * _free_xfsinfo()
 *	Free the memory allocated to hold the mount structure, inode
 *	and block map for an XFS file.
 * RETURN:
 *	none
 */
static void
_free_xfsinfo(xfs_info_t	*xfs_infop)
{
	dmabuf_free(xfs_infop->xfs_mp);
	xfs_infop->xfs_mp = NULL;
	dmabuf_free(xfs_infop->xfs_ip);
	xfs_infop->xfs_ip = NULL;
	free(xfs_infop);
}

/*
 * _get_xfsinfo()
 *	Allocate memory to contain the XFS mount structure, inode structure,
 *	and block map for an XFS file, as well as memory to contain the
 *	xfs_info_t structure itself.
 *
 * RETURNS:
 *	a pointer to the xfs_info_t structure.
 */
static xfs_info_t *
_get_xfsinfo(void)
{
	xfs_info_t 	*xfs_infop;
	int		len;

	if ( (xfs_infop = (xfs_info_t *)malloc(sizeof(xfs_info_t))) == 0 ) {
		return (xfs_info_t *)0;
	}

	len = BBTOB(BTOBB(sizeof(xfs_mount_t)));
	if ( (xfs_infop->xfs_mp = (xfs_mount_t *)dmabuf_malloc(len)) == 0) {
		free(xfs_infop);
		return (xfs_info_t *)0;
	}

	len = BBTOB(BTOBB(sizeof(xfs_inode_t)));
	if ( (xfs_infop->xfs_ip = (xfs_inode_t *)dmabuf_malloc(len)) == 0) {
		dmabuf_free(xfs_infop->xfs_mp);
		free(xfs_infop);
		return (xfs_info_t *)0;
	}
	return xfs_infop;
}


/*
 * _xfs_strat ()
 *	This is the entry point into the XFS driver.
 *
 * RETURNS:
 *	varies according to the type of request but usually
 *	1 on success 0 on failure.
 */
static int
_xfs_strat(FSBLOCK *fsb)
{

	switch (fsb->FunctionCode) {
		case FS_CHECK:
			return _xfs_checkfs(fsb);
		case FS_OPEN:
			return _xfs_open(fsb);
		case FS_CLOSE:
			return _xfs_close(fsb);
		case FS_READ:
			return _xfs_read(fsb);
		case FS_WRITE:
			return fsb->IO->ErrorNumber = EROFS;
		case FS_GETDIRENT:
			return _xfs_getdirent(fsb);
		case FS_GETREADSTATUS:
			return _xfs_grs(fsb) ;
		default:
			return fsb->IO->ErrorNumber = ENXIO;
	}
}


/*
 * xfs_lbread()
 *	Read block bn of size len from the XFS file pointed to by 
 * 	fsb and store the contents in buf.
 *
 * RETURNS:
 *	1 on success
 *	0 on error
 */
int
xfs_lbread(FSBLOCK *fsb, daddr_t bn, void *buf, long len, int printerr)
{
        IOBLOCK *io = fsb->IO;

	/*
	 * Round the length up to a disk sector boundary.
 	 */
        len = (long)BBTOB(BTOBB(len));

        io->Count	= len;
        io->Address	= buf;
        io->StartBlock	= bn;

        if (DEVREAD(fsb) != len) {
                if(printerr)
                        printf("xfs: file read error.\n");
                return (0);
        }
        return (1);
}


/*
 * _xfs_get_superblock()
 *	Read the XFS superblock from the device pointed to by fsb
 * 	and store the data in the sbp structure.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
static int 
_xfs_get_superblock( FSBLOCK *fsb, xfs_sb_t *sbp)
{
	char	*buf;
	int	len, ret;

	/*
	 * Round the length up to a disk sector boundary.
 	 */
	len = BBTOB(BTOBB(sizeof(xfs_sb_t)));

	if ((buf = (char *)dmabuf_malloc(len)) == 0) {
		printf("xfs: could not allocate memory to check superblock.\n");
		return 0;
	}

	if ( ret = xfs_lbread( fsb, (long)XFS_SB_DADDR, 
				buf, (ulong)(sizeof(xfs_sb_t)), 0))
		bcopy(buf, sbp, sizeof(xfs_sb_t));

	return ret ; 
}

/*
 * _xfs_checkfs()
 *	Determine is the device pointed to by fsb contains an XFS
 *	file system.
 *
 * RETURNS:
 *	ESUCCESS if the device contains an XFS file system.
 *	-1 otherwise
 */
static int
_xfs_checkfs(FSBLOCK *fsb)
{
	xfs_sb_t 	*sbp;
	int		len, ret;

	/*
	 * Round the length up to a disk sector boundary.
 	 */
	len = BBTOB(BTOBB(sizeof(xfs_sb_t)));

	/*
	 * Allocate memory buffer to contain file system superblock.
 	 */
	if ((sbp = (xfs_sb_t *)dmabuf_malloc(len)) == 0) {
		return ENOMEM;
	}
	
	bzero( sbp, sizeof(xfs_sb_t) );

	
	ret = ESUCCESS;
	if (!_xfs_get_superblock(fsb, sbp)) {
		ret = EINVAL;
	} else {
		/*
		 * Determine if this is an XFS superblock.
		 */
		if ( (sbp->sb_magicnum   != XFS_SB_MAGIC)    ||
		     !XFS_SB_GOOD_SASH_VERSION(sbp) ) {
			ret = EINVAL;
		}
	}



	dmabuf_free(sbp);
	return( ret );

}


/*
 * _xfs_dilocate()
 *	Determine the file system block number and the offset within
 *	that block where the given inode is located.
 *
 * RETURNS:
 *	none
 */
/*ARGSUSED*/
void
_xfs_dilocate(
	xfs_mount_t	*mp, 
	xfs_trans_t	*tp,
	xfs_ino_t	ino, 
	xfs_fsblock_t	*bno, 	
	int		*off)
{
	xfs_agblock_t	agbno;
	xfs_agnumber_t	agno;
	int		offset;

	agno	= XFS_INO_TO_AGNO(mp, ino);
	agbno	= XFS_INO_TO_AGBNO(mp, ino);
	offset	= XFS_INO_TO_OFFSET(mp, ino);

	*bno 	= XFS_AGB_TO_FSB(mp, agno, agbno);
	*off	= offset;

	return;
}

/*
 * _xfs_imap()
 *	Setup and xfs_imap_t structure which describes the size
 *	and locate of an on disk XFS inode.
 *
 *
 * RETURNS:
 *	none
 */
/*ARGSUSED*/
void
_xfs_imap(xfs_mount_t *mp, 
	xfs_trans_t	*tp,
	xfs_ino_t 	ino, 
	xfs_imap_t 	*imap)
{
	xfs_fsblock_t	fsbno;
	int		off;

	_xfs_dilocate( mp, NULL, ino, &fsbno, &off);
	imap->im_blkno 		= (daddr_t)XFS_FSB_TO_DADDR(mp, fsbno);
	imap->im_len		= XFS_FSB_TO_BB(mp, 1);
	imap->im_agblkno	= XFS_FSB_TO_AGBNO( mp, fsbno);
	imap->im_ioffset	= (ushort)off;
	imap->im_boffset	= (ushort)(off << mp->m_sb.sb_inodelog);
	return;
}

/*
 * _xfs_iread()
 *	Read the given XFS inode (ino) from disk, create an incore inode
 *	structure (ip) .
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
int
_xfs_iread(
	FSBLOCK		*fsb, 
	xfs_mount_t	*mp, 
	xfs_ino_t 	ino, 
	xfs_inode_t	*ip)
{
	char 		*buf;
	xfs_imap_t	imap;
	xfs_dinode_t	*dip;


	/*
	 * Determine the location of the inode structure on disk.
	 */
	_xfs_imap( mp, NULL, ino, &imap);

	/*
	 * Allocate buffer to read the disk structure.
	 */
	if ((buf = dmabuf_malloc(BBTOB(imap.im_len))) == 0) {
		printf("xfs: could not allocate memory to read inode.\n");
		return 0;
	}
	
	/*
	 * Read a block of inode structures from the disk.
	 */
	if (xfs_lbread(fsb, imap.im_blkno, buf, (long)(BBTOB(imap.im_len)), 1) == 0) {
		printf("xfs: read of inode failed.\n");
		dmabuf_free(buf);
		return 0;
	}

	/*
	 * Determine the requested inode structure.
	 */
	dip = (xfs_dinode_t *)(buf + imap.im_boffset);

	/*
 	 * Verify inode magic number.
	 */
	if (dip->di_core.di_magic != XFS_DINODE_MAGIC) {
		printf("xfs: inode %lld has invalid magic number 0x%x.\n",
				ino, dip->di_core.di_magic);
		dmabuf_free(buf);
		return 0;
	} 

	/*
	 * Copy the disk inode information to the return structure.
 	 */
	bcopy(&(dip->di_core), &(ip->i_d),sizeof(xfs_dinode_core_t));

	xfs_iformat( mp, ip, dip);
	ip->i_mount = mp;
	ip->i_ino = ino;

	if (ip->i_d.di_format == XFS_DINODE_FMT_BTREE) {
		_xfs_iread_extents( fsb, ip);
	}

	dmabuf_free(buf);

	return 1;
}

/*
 * _xfs_lookup_name()
 *	Find the inode number of the file corresponding to the given file
 *	(filename).
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
static int 
_xfs_lookup_name(FSBLOCK *fsb, char *filename)
{
	int		namelen, ret = 1;
	char		*sp, name[MAXNAMELEN + 1];
	IOBLOCK		*iob = fsb->IO;
	xfs_sb_t 	*sbp;
	xfs_ino_t	nextinum;
	xfs_inode_t	*ip = fsb_to_inode(fsb);
	xfs_mount_t	*mp = fsb_to_mount(fsb);
	
	sbp = &(mp->m_sb);
	iob->Offset.hi = 0;
	iob->Offset.lo = 0;

	/*
	 * Skip over leading blanks, tabs, and slashes.
	 * All paths are grounded at root.
 	 */
	sp = filename;
	while (*sp && ( *sp == ' '|| *sp == '\t' || *sp == '/')) sp++;

	if (sbp->sb_rootino == NULLFSINO) {
		printf("xfs: root inode is null.\n");
		return  0;
	}

	/*
	 * Read the root inode.
	 */
	if (!_xfs_iread(fsb, mp, sbp->sb_rootino, ip)) {
		printf("xfs: could not read root inode %lld.\n",
			sbp->sb_rootino);
		return 0 ;
	}

	/*
	 * Create name of root.
	 */
	name[0] = '/';
	name[1] = '\0';

	/*
	 * Search through directories until the end is found.
	 */
	while (*sp) {
		/*
		 * Verify that we have a directory inode.
		 */
		if ((ip->i_d.di_mode & IFMT) != IFDIR) {
			printf("xfs: %s not a directory inode.\n",name);
			return 0 ;
		}
		if (((ip->i_d.di_version == XFS_DINODE_VERSION_1) &&
		     (ip->i_d.di_onlink == 0)) ||
		    ((ip->i_d.di_version == XFS_DINODE_VERSION_2) &&
		     (ip->i_d.di_nlink == 0))) {
			printf("xfs: %s directory has a link count of zero.\n",
						name);
			return 0 ;
		}
		if ((ip->i_d.di_mode & 0111) == 0) {
			printf("xfs: Warning - directory %s is not searchable "
				"(mode %o).\n",
				name, ip->i_d.di_mode & 0777);
		}

		for(namelen = 0; namelen < MAXNAMELEN && *sp && *sp != '/';
			name[namelen++] = *sp++);

		/*
		 * Discard characters of lengthy file name.
		 */
		if (namelen >= MAXNAMELEN) {
			while (*sp && *sp != '/') sp++;
		}

		name[namelen] = '\0';

		/*
		 * Skip over slashes between directory names.
	 	 */
		while (*sp == '/') sp++;
			

		/*
		 * Find the inode number for this file.
		 */
		ret = _xfs_dir_lookup(fsb, ip, name, namelen, &nextinum);

		/*
		 * Free this inode and read the next.
		 */
		_xfs_ifree(ip);

		if (ret) {
			/*
			 * Lookup was successful.
			 * Read the contents of the file.
			 */
			if (!_xfs_iread(fsb, mp, nextinum, ip)) {
				printf("Could not read XFS file %s.\n",name);
				return 0 ;
			}
		}
	}

	return( ret );
}

/*
 * _xfs_open()
 *	Determine if the given file to be opened exists and if so,
 *	fill in the appropriate fields in the fsb structure to access the
 *	file.
 *
 * RETURNS:
 *	ESUCCESS on success
 *	non zero on failure.
 */
static int
_xfs_open(FSBLOCK *fsb)
{
	int		ret, i;
	long		len, bno;
	char		*buf;
	char 		*filename = (char *)fsb->Filename;
	xfs_mount_t	*xfs_mp;

	if (fsb->IO->Flags & F_WRITE) {
		printf("xfs: cannot open file for writing.\n");
		return( fsb->IO->ErrorNumber = EIO );
	}

	/*
	 * Allocate the xfs info structure.
	 */
	if ((fsb->FsPtr = _get_xfsinfo()) == 0) {
		printf("xfs: could not allocate memory for xfs info.\n");
		return( fsb->IO->ErrorNumber = ENOMEM );
	}	

	xfs_mp =  fsb_to_mount(fsb);

	/*
	 * Read in and verify the XFS superblock.
 	 */
	if (!_xfs_get_superblock(fsb, &(xfs_mp->m_sb))) {
		return( fsb->IO->ErrorNumber = ENXIO );
	} else {
		if ( (xfs_mp->m_sb.sb_magicnum != XFS_SB_MAGIC)    ||
		     !XFS_SB_GOOD_SASH_VERSION(&xfs_mp->m_sb) ) {
			printf("xfs: invalid magic numbers in superblock.\n");
			return( fsb->IO->ErrorNumber = ENXIO );
		}
	}

	/*
	 * Fill in other mount structure fields.
	 */
	xfs_mp->m_agfrotor 	= xfs_mp->m_agirotor = 0;
	xfs_mp->m_blkbit_log	= xfs_mp->m_sb.sb_blocklog + XFS_NBBYLOG;
	xfs_mp->m_blkbb_log	= xfs_mp->m_sb.sb_blocklog - BBSHIFT;
	xfs_mp->m_agno_log	= xfs_highbit32(xfs_mp->m_sb.sb_agcount -1) + 1;
	xfs_mp->m_agino_log	= xfs_mp->m_sb.sb_inopblog +
				  xfs_mp->m_sb.sb_agblklog;
	xfs_mp->m_litino	= (int)(xfs_mp->m_sb.sb_inodesize - 
				  (sizeof(xfs_dinode_core_t) +
				   sizeof(xfs_agino_t)));
	xfs_mp->m_blockmask	= xfs_mp->m_sb.sb_blocksize -1;
	xfs_mp->m_blockwsize	= xfs_mp->m_sb.sb_blocksize >> XFS_WORDLOG;
	xfs_mp->m_blockwmask	= xfs_mp->m_blockwsize - 1;

	for (i = 0; i < 2; i++) {
		xfs_mp->m_bmap_dmxr[i] = (uint)XFS_BTREE_BLOCK_MAXRECS(
			xfs_mp->m_sb.sb_blocksize, xfs_bmbt, i == 0);
		xfs_mp->m_bmap_dmnr[i] = (uint)XFS_BTREE_BLOCK_MINRECS(
			xfs_mp->m_sb.sb_blocksize, xfs_bmbt, i == 0);
	}

	if (XFS_SB_VERSION_HASDIRV2(&xfs_mp->m_sb)) {
		xfs_mp->m_dirblksize =
			1 << (xfs_mp->m_sb.sb_blocklog +
			      xfs_mp->m_sb.sb_dirblklog);
		xfs_mp->m_dirblkfsbs = 1 << xfs_mp->m_sb.sb_dirblklog;
		xfs_mp->m_dirdatablk =
			XFS_DIR2_DB_TO_DA(xfs_mp,
				XFS_DIR2_DATA_FIRSTDB(xfs_mp));
		xfs_mp->m_dirleafblk =
			XFS_DIR2_DB_TO_DA(xfs_mp,
				XFS_DIR2_LEAF_FIRSTDB(xfs_mp));
	} else {
		xfs_mp->m_dirblksize = xfs_mp->m_sb.sb_blocksize;
		xfs_mp->m_dirblkfsbs = 1;
	}

	/*
	 * Allocate buffer to be used in read/write code.
	 */
	len    =  XFS_FSB_TO_B(xfs_mp, 1);
	if ((fsb->Buffer = (CHAR *)dmabuf_malloc(len)) == 0) {
		printf("xfs: could not allocate memory for file buffer.\n");
		return( fsb->IO->ErrorNumber = ENOMEM );
	}

	if ((buf = (CHAR *)dmabuf_malloc(len)) == 0) {
		printf("xfs: could not allocate memory for temporary buffer.\n");
		return( fsb->IO->ErrorNumber = ENOMEM );
	}

	/*
	 * Determine if file system is bigger than partition.
	 */
	bno = (long)XFS_FSB_TO_BB(xfs_mp, xfs_mp->m_sb.sb_dblocks) - 1;
	len =  BBSIZE;
	if ( (ret = xfs_lbread( fsb, (long)bno, buf, len, 0)) == 0) {
		printf("xfs: disk partition size is smaller than file system size\n");
		dmabuf_free(buf);
		return( fsb->IO->ErrorNumber = EFAULT );
	}
	dmabuf_free(buf);

	/*
	 * Find file to open.
 	 */
	if (_xfs_lookup_name(fsb, filename)) {
		ret = ESUCCESS;
	} else {
		ret = ENOENT;
	}

	return( fsb->IO->ErrorNumber = ret );
}

/*
 * _xfs_close()
 *	Free the memory for the data structures which were allocated
 *	in _xfs_open();
 *
 * RETURNS:
 *	ESUCCESS
 */
static int
_xfs_close(FSBLOCK *fsb)
{

	_xfs_ifree(fsb_to_inode(fsb));
	dmabuf_free((char *)fsb->Buffer);
	fsb->Buffer = NULL;
	_free_xfsinfo(fsb->FsPtr);
	fsb->FsPtr = NULL;

	return( ESUCCESS );
}

/*
 *  _xfs_bmap()
 *	Get the disk block number that contains the requested 
 * 	file offset and return the value in the "result" structure.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
/*ARGSUSED*/
_xfs_bmap( 
	FSBLOCK *fsb,
	extent	*result,
	long 	maxvblks)
{
	xfs_mount_t	*mp	= fsb_to_mount(fsb);
	xfs_inode_t	*ip	= fsb_to_inode(fsb);
	xfs_dfsbno_t	dbno;
	xfs_fileoff_t	off;
	IOBLOCK		*iob = fsb->IO;
	long		len;

	/*
	 * Truncate offset to file system block boundary.
	 */
	off  = (xfs_fileoff_t)XFS_B_TO_FSBT( mp, iob->Offset.lo );

	/*
	 * Get the file system block number for this file offset.
	 */
	dbno = _xfs_get_blockno( mp, ip, off, &len );

	if (dbno == NULLFSBLOCK) {
		/*
		 * The block is not mapped due to a hole in the file.
		 * The caller must look for a block number of -1 and
		 * assume a block of zeros.
		 */
		if ( off < XFS_B_TO_FSB(mp, ip->i_d.di_size) ) {
			result->ex_bn 		= -1;
			result->ex_length 	= mp->m_sb.sb_blocksize;
	 		return 1;
		} else {
	 		return 0;
		}
	}

	result->ex_bn = XFS_FSB_TO_DADDR(mp, dbno);
	result->ex_length = mp->m_sb.sb_blocksize * _min(len, maxvblks);
	result->ex_length = _min(result->ex_length, MAX_XFS_DMA);
	result->ex_offset	= (uint)off;
	
	return 1;
}

/*
 * _xfs_read()
 a	Read the contents of given file at the given offset.
 *
 *
 * RETURNS:
 *	ESUCCESS on success
 *	other on failure
 */
static int
_xfs_read(FSBLOCK *fsb)
{
	long		i;
	off_t		off;
	extent		result;
	caddr_t		dmaaddr;
	xfs_inode_t	*ip = fsb_to_inode(fsb);
	xfs_mount_t	*mp = fsb_to_mount(fsb);
	IOBLOCK		*iob	= fsb->IO;
	char 		*buf	= iob->Address;
	long		ocount, count	= iob->Count;

	
	/*
	 * Truncate the count value if it extents past end of file.
 	 */
	if (iob->Offset.lo + count > ip->i_d.di_size) {
		count = (long)(ip->i_d.di_size - iob->Offset.lo);
	}

	/*
	 * Check that count is a positive value.
 	 */
	if (count <= 0) {
		iob->Count = 0;
		return ESUCCESS;
	}


	ocount = count;
	while ( count > 0 ) {
                /* 
		 * if reading more than one block, and alignments OK, 
		 * dma directly into user buffer.
                 * The i_offset check is so partial blocks are 
		 * handled by the copy code, so multiple reads with small 
		 * sizes (one read gets first part of sector, next xfs_read 
		 * gets rest) will work.
                 */
		i = (long)XFS_B_TO_FSBT(mp, count);
                if ( (!XFS_B_FSB_OFFSET(mp,iob->Offset.lo)) &&
		     (((ulong)buf & 3) == 0) 	&&
		     (i) ) {
                        dmaaddr = buf;
                } else {  /* reading single sectors */
                        i = 1;
                        dmaaddr = (char *) fsb->Buffer;
                }

		if (!_xfs_bmap(fsb, &result, i)) {
			return(iob->ErrorNumber = EIO);
		}

		/*
		 * There is no buffering between calls to _xfs_read.
		 * Always read the data from disk.
		 */
		iob->StartBlock  = result.ex_bn;
		iob->Address     = dmaaddr;
		i = iob->Count   = result.ex_length;


		if (result.ex_bn == -1) {
			bzero( dmaaddr, result.ex_length);
		} else {
			if(DEVREAD(fsb) != i)
				return(iob->ErrorNumber = EIO);
		}

                if(dmaaddr != buf) {    
			/* 
			 * copy from 'block buffer' 
			 */
                        off = ((long)iob->Offset.lo) - 
				XFS_FSB_TO_B(mp, result.ex_offset);

                        if (off < 0)
                                return(iob->ErrorNumber = EIO);

                        i = _min(result.ex_length - off, count);
                        if (i <= 0)
                                return(iob->ErrorNumber = EIO);

                        bcopy(&fsb->Buffer[off], buf, (int)i);
                } else {    
			/*
			 * force next read from disk; in case 
                         * next read is small, and same ex_bn 
			 */
                        iob->StartBlock = -1; 
		}

                count -= i;
                buf += i;
                iob->Offset.lo += i;
        }

  	iob->Count = ocount;
        return(ESUCCESS);
}


/*
 * _xfs_getdent()
 *	Return the next directory entry from the directory pointed to 
 *	by fsb in the dep structure. This routine determines what type
 *	of directory is being accessed and calls the appropriate function.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
static int
_xfs_getdent(
	FSBLOCK		*fsb,
	xfs_dirent_t	*dep)
{
	xfs_mount_t	*mp = fsb_to_mount(fsb);

	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return _xfs_dirv2_getdent(fsb, dep);
	else
		return _xfs_dirv1_getdent(fsb, dep);
}

/*
 * _xfs_getdirent()
 *	Get the directory entries for the directory described by the
 * 	fsb.
 *
 *
 * RETURNS:
 *	ESUCCESS on success
 *	ENOTDIR when there are no more entries.
 *
 */
static int
_xfs_getdirent(FSBLOCK *fsb)
{
        int		namelen;
	long		left, count = fsb->IO->Count;
        xfs_inode_t	ip;
	xfs_mount_t	*mp = fsb_to_mount(fsb);
	xfs_dirent_t	dep;
        DIRECTORYENTRY 	*dp = (DIRECTORYENTRY *)fsb->IO->Address;

	/*
 	 * No directory entries requested.
	 */
        if (count == 0)
                return(fsb->IO->ErrorNumber = EINVAL);

        left = count;
        while (left && ( _xfs_getdent(fsb, &dep)) ) {

                namelen = _min(dep.namelen, FileNameLengthMax - 1);

                dp->FileAttribute = ReadOnlyFile;

                /* 
		 * Figure out if this file is a directory or not,
                 *  must read the inode to do so
                 */
		if (!_xfs_iread(fsb, mp, dep.ino, &ip)) {
                        fsb->IO->ErrorNumber = EIO;
			printf("xfs: could not read file %s \n",dep.name);
                        break;
                }

                if ((ip.i_d.di_mode & S_IFMT) == S_IFDIR)
                        dp->FileAttribute |= DirectoryFile;

                dp->FileNameLength = namelen;
                bcopy (dep.name, dp->FileName, (int)namelen);
                dp->FileName[namelen] = '\0';
                dp++;
                left--;
		_xfs_ifree(&ip);
        }

        fsb->IO->Count = count - left;

        /* 
	 * ARCS specifies that ENOTDIR is returned when no more entries.
         */
        if (fsb->IO->Count == 0) {
                return(fsb->IO->ErrorNumber = ENOTDIR);
	}

        return(ESUCCESS);
}

/*
 * _xfs_grs()
 *	Returns read status.
 *
 * RETURNS:
 *	always returns ESUCCESS
 */
static int
_xfs_grs(FSBLOCK *fsb)
{

	if (fsb->IO->Offset.lo >= fsb_to_inode(fsb)->i_d.di_size)
		return(fsb->IO->ErrorNumber = EAGAIN);
	fsb->IO->Count = 
		(long)(fsb_to_inode(fsb)->i_d.di_size - fsb->IO->Offset.lo);
	return( ESUCCESS );
}

/*
 * _xfs_dir_lookup()
 *	Get the inode number of the file with the given name in the
 *	given directory.
 *	Vector out to v1 or v2 routine.
 *
 * RETURNS:
 *	1 on success
 *	0 on failure
 */
static int
_xfs_dir_lookup(
	FSBLOCK		*fsb,
	xfs_inode_t	*dp,
	char		*name,
	int		namelen,
	xfs_ino_t	*inum)
{
	xfs_mount_t	*mp;

	mp = dp->i_mount;
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return _xfs_dirv2_lookup(fsb, dp, name, namelen, inum);
	else
		return _xfs_dirv1_lookup(fsb, dp, name, namelen, inum);
}
