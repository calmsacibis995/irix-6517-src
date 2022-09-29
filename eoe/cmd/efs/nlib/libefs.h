/*
 * libefs.a declarations
 * 
 * libefs is a useful collection of subprograms to manipulate EFS data
 * structures (superblock, bitmap, inodes, extents, directories).
 * The similarity to kernel EFS ends at the data structures -- there's no
 * attempt to mimic the kernel's allocation policies, etc.
 *
 * Even though these subprograms are used in mkfs and fsr this collection
 * does not presume to be a well designed library.  User beware: some
 * routines do _not_ mix well with others, and some routines expect
 * prerequisite callers and most routines don't expend much effort
 * checking arguments.
 *
 * $Revision: 1.11 $
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <values.h>
#include <sys/fs/efs.h>

#ifdef	BIG
extern int __efs_block_size;

#define __EFS_SWITCH_VAL	__efs_block_size
#else
#define	EFS_BBSIZE		BBSIZE
#define	EFS_BIG_BBSIZE		BBSIZE
#define	EFS_SMALL_BBSIZE	BBSIZE
#define	EFS_BBTOB(x)		BBTOB(x)
#define	EFS_BTOBB(x)		((int)BTOBB(x))
#define	EFS_BTOBBT(x)		((int)BTOBBT(x))
#endif

#define	ISDOT(namep, len)	((len) == 1 && *(namep) == '.')
#define	ISDOTORDOTDOT(np, len)	(*(np) == '.' && \
			 ((len) == 1 || ((len) == 2 && *((np)+1) == '.')))

/*
 * All libefs.a reads and writes are by block mostly because >2gb file
 * systems must be read using the SGI-special block read/write system call.
 */
#define	efs_readb(fd, buf, blk, nblks) \
				efs_rdwrb(SGI_READB, fd, buf, blk, nblks)
#define	efs_writeb(fd, buf, blk, nblks)	\
				efs_rdwrb(SGI_WRITEB, fd, buf, blk, nblks)
extern efs_rdwrb(int rdwr, int fd, char *buf, efs_daddr_t blk, int nblks);


#define	bset(bp, b)	(*((bp) + ((b) >> 3)) |= 1 << ((b) & 7))
#define	bclr(bp, b)	(*((bp) + ((b) >> 3)) &= ~(1 << ((b) & 7)))
#define	btst(bp, b)	(*((bp) + ((b) >> 3)) & (1 << ((b) & 7)))
#define tstfree		btst

typedef struct efs_mount {
	struct efs	   *m_fs;
	dev_t		    m_dev;
	int		    m_fd;
	struct efs_dinode **m_ilist;
	char		   *m_devname;	/* used only in debug/error printing */
	char		   *m_bitmap;
	char		   *m_progname;
	int		    active;	/* GUI uses to bypass failed units */
	char		    tofile[40];
} EFS_MOUNT;

/*
 * initialize libefs.  arg is function for printing error messages from
 * within the library -- default is fprintf(stderr, )
 */
extern void efs_init(int (*func)(const char *,...));
extern int (*efs_errpr)(const char*, ...);
extern int _efsprintf(const char*, ...);

extern char *devnm(dev_t);
extern char filetype(ushort);

/*
 * efs_mount() open(2)s the specified file system device (must be a char dev)
 * with the given mode (bits defined in <fcntl.h>), mallocs space for and reads
 * in the superblock (m_fs) and saves the devname in a malloc'ed buffer
 * (m_devname). efs_umount() * closes the device file and frees the malloc'ed
 * space for m_fs, m_devname, and the specified struct efs_mount itself.
 */
extern EFS_MOUNT *efs_mount(char *devname, int rw);
extern void efs_umount(EFS_MOUNT* mp);
extern long efs_checksum(struct efs *fs);

/* Some handy macros to compute file system capacity from superblock data */
#define CAPACITYBLKS(fs) ((fs)->fs_ncg * ((fs)->fs_cgfsize - (fs)->fs_cgisize))
#define CAPACITYINODES(fs) ((fs)->fs_ncg * (fs)->fs_cgisize * EFS_INOPBB - 2)

/*
 * efs_iget() returns a pointer to a _static_buffer_ containing the disk inode
 * for the specified inode.  efs_igetcg() mallocs and reads in all inode blocks
 * for the specified cylinder group.  efs_getextents() mallocs and copies/reads
 * the extents for the file specified by the struct efs_dinode. efs_figet()
 * returns a pointer to the inum within an internally maintained pool of
 * i-lists it reads using efs_igetcg() XXX there is no "free" analog
 */
extern struct efs_dinode *efs_iget(EFS_MOUNT *mp, efs_ino_t inum);
extern struct efs_dinode *efs_figet(EFS_MOUNT *mp, efs_ino_t inum);
extern struct efs_dinode *efs_igetcg(EFS_MOUNT *mp, int cgno);
extern extent *efs_getextents(EFS_MOUNT *mp, struct efs_dinode *di,
	efs_ino_t inum);

/*
 * efs_iput writes a single inode back to disk.
 */
extern int efs_iput(EFS_MOUNT *mp, struct efs_dinode *dip, efs_ino_t ino);

/*
 * Create a new inode.
 */
extern void efs_mknod(EFS_MOUNT *mp, efs_ino_t ino, ushort mode, ushort uid,
	ushort gid);


/*
 * malloc and read in the bitmap for the specified mounted file system
 */
extern int efs_bget(EFS_MOUNT *mp);
extern int efs_bput(EFS_MOUNT *mp);
extern void efs_bfree(EFS_MOUNT *mp);

/*
 * read in all inodes, dirblks and indirect extents using the efs_dinode
 * as an "in-core" inode
 */
extern efs_ino_t efs_readinodes(EFS_MOUNT *mp, FILE *progress, int *blkcount,
								int flags);

#define	DIDATA(d,t)	((t)(&(d)->di_u.di_extents[0]))
typedef struct {
	char	*dirblkp;	/* if directory */
	int	dirblocks;	/* " */
	extent	*ex;		/* if indirect extents */
	int	indirblks;
} didata;

/*
 * Number of blocks of indirect extents.
 * Assumes 1) ne > EFS_DIRECTECTEXTENTS,
 *     and 2) each indirect extent is fully packed.
 */
#define	INDIRBLKS(ne)	(ne * sizeof(extent) / BBSIZE + 1)

#define	DATABLKS(ex, ne) \
	(((ex) + (ne) - 1)->ex_offset + ((ex) + (ne) - 1)->ex_length)

#define INUMTODI(mp, inum) \
	mp->m_ilist[EFS_ITOCG(mp->m_fs, inum)] + \
	inum % (mp->m_fs->fs_cgisize*EFS_INOPBB)


/*
 * print inode, superblock and bitmap
 */
extern void efs_prino(FILE *fp, EFS_MOUNT *mp,
	efs_ino_t inum, struct efs_dinode *dip, extent *ext, char *name,
	int nl);
extern prsuper(FILE *fp, EFS_MOUNT *mp);
extern prbit(FILE *fp, EFS_MOUNT *mp);

/*
 * map name to inum
 */
extern efs_ino_t efs_namei(EFS_MOUNT *mp, char *filename);

/*
 * directory stuff
 * XXX this should present either a getdents(2) or opendir(3) interface
 */
typedef struct {
        char    	*dir_blkp;
        int     	dir_nblocks;
        int     	dir_curblk;
        int     	dir_curslot;
	EFS_MOUNT	*dir_mp;
} EFS_DIR;

extern EFS_DIR *efs_opendir(EFS_MOUNT *mp, char *filename);
extern EFS_DIR *efs_opendiri(EFS_MOUNT *mp, efs_ino_t inum);
extern EFS_DIR *efs_opendirb(char *dp, int nblocks);
extern struct efs_dent *efs_readdir(EFS_DIR *dirp);
extern void efs_closedir(EFS_DIR *dirp);
extern char *efs_getdirblks(int fd, struct efs_dinode *dp, extent *ex,
	efs_ino_t inum);
extern void efs_rewinddir(EFS_DIR *dirp);


/*
 * "ftw"
 */
extern void efs_walk(EFS_MOUNT *mp, efs_ino_t inum, char *dirname, int flags,
        void (*dodent)(EFS_MOUNT *mp, struct efs_dent *, char *, int));
extern void efs_ddent(EFS_MOUNT *, struct efs_dent *dentp, char *dirname,
	int flags);

#define	DO_EXT		0x00000001
#define	DO_RECURSE	0x00000002

extern void efs_extend(EFS_MOUNT *mp, struct efs_dinode *di, int incr);
extern efs_daddr_t efs_bmap(EFS_MOUNT *mp, struct efs_dinode *di, off_t offset,
								off_t count);

/*
 * Enter the given name/inode pair into the given directory.
 */
extern void efs_enter(EFS_MOUNT *mp, efs_ino_t dirinum, efs_ino_t inum,
	char *name);

/*
 * Extend the given file by allocating data blocks.
 */
extern void efs_extend(EFS_MOUNT *mp, struct efs_dinode *di, int incr);

/*
 * Make the lost+found directory.  Just grow it, sinces its already
 * got "." and "..".
 */
extern void efs_mklostandfound(EFS_MOUNT *mp, efs_ino_t ino);



extern efs_ino_t efs_allocino(EFS_MOUNT *mp);

extern dev_t efs_getdev(union di_addr *di);
extern void efs_putdev(dev_t dev, union di_addr *di);

/*
 * Creates a new regular file at the given inode and name.
 * (This doesn't make the directory entry).
 */
extern void efs_newregfile(EFS_MOUNT *mp, efs_ino_t ino, char *name);

/*
 * Write into a file.  Data is always appended.
 */
extern void efs_write(EFS_MOUNT *mp, efs_ino_t ino, char *data, int len);

extern void error(void);
