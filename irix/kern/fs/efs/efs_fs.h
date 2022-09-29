#ifndef	__efs_fs_
#define	__efs_fs_
/*
 * An extent filesystem is composed of some number of basic blocks.  A basic
 * block is composed of one disk sector (512 bytes).  The term
 * "bb" is used as short hand for a basic block.
 *
 * The superblock is replicated for robustness in IRIX 3.3 and beyond.
 * The replicated superblock is on the very last block of the storage where
 * the filesystem resides. For convenience, a new field 'fs_replsb' (claimed
 * from the area of spare fields) points to its location.
 *
 * Filesystems may be GROWN in IRIX 3.3 and beyond. To do this, the bitmap
 * for the newly expanded filesystem is moved to the END of the storage space 
 * (just before the replicated superblock), to avoid overlapping the first 
 * cylinder group.
 * It is then located via a pointer fs_bmblock in the superblock. 
 * The space formerly occupied by the bitmap at block 2 becomes unused.
 * Note that grown filesytems have a new fs magic number to enable tools
 * and the kernel to handle the different layout.
 * Filesystems from any IRIX release may be grown, and filesystems
 * may be grown repreatedly (by simply repeating the process of taking the
 * bitmap to the end); note however that a grown filesystem can NOT be used
 * with pre-3.3 kernels or tools.
 *
 * Physical layout of a normal efs:
 *	1. Basic block 0 is unused.
 *	2. Basic block 1 is the efs superblock.
 *	3. Basic block 2 begins the bitmap.  The bitmap covers
 *	   Basic blocks 2 through 2 + BTOD(fs->fs_bmsize) - 1, inclusively.
 *	4. Between the end of the bitmap and the basic block fs->fs_firstcg
 *	   are some number of unused basic blocks (left unused for alignment
 *	   reasons).  fs->fs_firstcg specifies the start of the first cylinder 
 *	   group, in basic blocks.
 *	5. Beginning at fs->fs_firstcg are fs->fs_ncg's worth of cylinder
 *	   groups.  The cylinder group size is fs->fs_cgfsize basic blocks.
 *	   Each cylinder group contains fs->fs_cgisize worth of inode basic
 *	   blocks.  The remaining basic blocks in the cylinder group are
 *	   data blocks.
 *	6. At the end of the filesystem, just past the end of the last
 *	   cylinder group are (usually) some number of trailing disk sectors,
 *	   left unused for alignment reasons.
 *	7. In 3.3 and beyond, the very last block of the filesystem space
 *	   is a replica of the superblock. (3.3.fsck will attempt to
 *	   retrofit a replicated superblock to earlier filesystems if
 *	   space permits). If present, fs->fs_replsb points to this.
 *
 * Physical layout of a grown efs:
 *	1. Basic block 0 is unused.
 *	2. Basic block 1 is the efs superblock.
 *	3. Between the superblock and the basic block fs->fs_firstcg
 *	   are some number of unused basic blocks.  fs->fs_firstcg specifies
 *	   the start of the first cylinder group, in basic blocks.
 *	4. Beginning at fs->fs_firstcg are fs->fs_ncg's worth of cylinder
 *	   groups.  The cylinder group size is fs->fs_cgfsize basic blocks.
 *	   Each cylinder group contains fs->fs_cgisize worth of inode basic
 *	   blocks.  The remaining basic blocks in the cylinder group are
 *	   data blocks.
 *	5. After the last cylinder group, before the bitmap, there will usually
 *	   be some number of unused basic blocks (a consequence of alignment).
 *	6. At the end of the filesystem space, BTOD(fs->fs_bmsize) blocks
 *	   constitute the bitmap. fs->fs_bmblock points to the start of this,
 *	   it ends one block short of the end of storage.
 *	7. The very last block of the space is the replicated superblock.
 *	   fs->fs_replsb points to this.
 *
 * How the layout is parameterized in the superblock:
 *
 *	In a regular, non-grown efs:
 *	   The size of the filesystem in basic blocks, excluding the trailing
 *	   basic blocks after the last cylinder group, is contained in
 *	   fs->fs_size.
 *
 *	In a grown efs:
 *	   fs->fs_size contains ALL blocks used by the filesystem, including
 *	   the bitmap & replicated superblock at the end. 
 *	   It's unfortunate that we had to change this, but alas pre-3.3
 *	   fsck does NOT look at the magic number; this was the ONLY way
 *	   to lock a pre-3.3 fsck out of running on a grown filesystem!
 *
 * Caveats:
 *	1. Trying to change the parameterization of the basic block size
 *	   probably won't work.
 *
 */
#ident "$Revision: 3.13 $"

/*
 * Locations of the efs superblock, bitmap and root inode.
 */
#define	EFS_SUPERBB	((efs_daddr_t)1)	/* bb # of the superblock */
#define	EFS_BITMAPBB	((efs_daddr_t)2) 	/* bb of the bitmap, pre 3.3*/
#define	EFS_SUPERBOFF	BBTOB(EFS_SUPERBB)	/* superblock byte offset */
#define	EFS_BITMAPBOFF	BBTOB(EFS_BITMAPBB)	/* bitmap byte offset */
#define	EFS_ROOTINO	((efs_ino_t)2)		/* where else... */

/*
 * Inode parameters.
 */
/* number of inodes per bb */
#define	EFS_INOPBB	(1 << EFS_INOPBBSHIFT)
#define	EFS_INOPBBSHIFT	(BBSHIFT - EFS_EFSINOSHIFT)
#define	EFS_INOPBBMASK	(EFS_INOPBB - 1)

/*
 * This macro initializes the computable fields of the efs superblock so
 * as to insure that the macros that use these fields will work.  It's other
 * purpose is to allow the macros to use the computable fields so that they
 * are simpler.  This macro should be executed, whenever a program wishes
 * to manipulate the filesystem, before actually manipulating the filesystem.
 */
#define	EFS_SETUP_SUPERB(fs) \
	{ \
		(fs)->fs_ipcg = EFS_COMPUTE_IPCG(fs); \
	}

/*
 * Compute the number of inodes-per-cylinder-group (IPCG) and the number
 * of inodes-per-basic-block (INOPBB).
 */
#define	EFS_COMPUTE_IPCG(fs) \
	((short) ((fs)->fs_cgisize << EFS_INOPBBSHIFT))

/*
 * Layout macros.  These macro provide easy access to the layout by
 * translating between sectors, basic blocks, and inode numbers.
 * WARNING: The macro EFS_SETUP_SUPERB must be executed before most
 * of these macros!
 */

/* inode number to bb, relative to cylinder group */
#define	EFS_ITOCGBB(fs, i) \
	((efs_daddr_t) (((i) >> EFS_INOPBBSHIFT) % (fs)->fs_cgisize))

/* inode number to offset from bb base */
#define	EFS_ITOO(fs, i) \
	((short) ((i) & EFS_INOPBBMASK))

/* inode number to cylinder group */
#define	EFS_ITOCG(fs, i) \
	((short) ((i) / (fs)->fs_ipcg))

/* inode number to cylinder group inode number offset */
#define	EFS_ITOCGOFF(fs, i) \
	((short) ((i) % (fs)->fs_ipcg))

/* inode number to disk bb number */
#define	EFS_ITOBB(fs, i) \
	((efs_daddr_t) ((fs)->fs_firstcg + \
		    (EFS_ITOCG(fs, i) * (fs)->fs_cgfsize) + \
		    EFS_ITOCGBB(fs, i)))

/* bb to cylinder group number */
#define	EFS_BBTOCG(fs, bb) \
	((short) ((bb - (fs)->fs_firstcg) / (fs)->fs_cgfsize))

/* cylinder group number to disk bb of base of cg */
#define	EFS_CGIMIN(fs, cg) \
	((efs_daddr_t) ((fs)->fs_firstcg + (cg) * (fs)->fs_cgfsize))

/* inode number to base inode number in its chunk */
#define	EFS_ITOCHUNKI(fs, cg, inum) \
	(((((inum) - (cg)->cg_firsti) / (fs)->fs_inopchunk) * \
	  (fs)->fs_inopchunk) + (cg)->cg_firsti)

/*
 * Allocation parameters.  EFS_MINFREE is based on the unix typical
 * file size - less than 1k.  EFS_MINDIRFREE is based on a guess as
 * to the typical number of those less than 1k files per directory.
 * EFS_ICHUNKSIZE is just a good number for the given hardware.
 */
#define	EFS_MINFREEBB		2
#define	EFS_MINDIRFREEBB	(10 * 2)
#define	EFS_INOPCHUNK		(NBPC / sizeof(struct efs_dinode))

#ifdef	_KERNEL
/* XXX tunable parameters */
extern	long efs_inopchunk;		/* # of inodes per inode chunk */
extern	long efs_minfree;		/* minimum bb's free for file creat */
extern	long efs_mindirfree;		/* min bb's free for dir creat */

struct buf;
extern void efs_asynchk(struct buf *);	/* async error reporting brelse */

/*
 * Buffer cache reference values.
 */
#define EFS_BMREF	3
#define EFS_DIRREF	2
#define EFS_INOREF	1
#define EFS_SBREF	1

#endif	/* _KERNEL */

#endif	/* __efs_fs_ */
