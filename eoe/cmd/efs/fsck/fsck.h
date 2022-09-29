#ident "$Revision: 1.31 $"

#define _SGI_MP_SOURCE
#include <sys/param.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/stat.h>
#include <bstring.h>
#include <ctype.h>
#include <limits.h>
#include <mutex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ulocks.h>
#include <unistd.h>
#include <values.h>
#include <sys/fs/efs.h>

extern usptr_t *fsck_arena;

typedef struct efs_dent	DIRECT;

#define	FIRSTINO	((efs_ino_t)1)

# define BITMAPB	EFS_BITMAPBB
# define BITMAPOFF	EFS_BITMAPBOFF
# define ITOD(fs, i)	EFS_ITOBB(fs, i)
# define ITOO(fs, i)	EFS_ITOO(fs, i)
# define CGIMIN(fs, cg)	EFS_CGIMIN(fs, cg)

# define BITMAPWORDSHIFT	3
# define BITMAPWORDMASK		((1<<BITMAPWORDSHIFT)-1)

# define BITMAPWORD(b,i)	((char *)(b))[(i)>>BITMAPWORDSHIFT]
# define BITMAPBIT(b,i)		(1<<((i)&BITMAPWORDMASK))

# define GETBIT(b,i) (BITMAPWORD(b,i) & BITMAPBIT(b,i))
# define CLRBIT(b,i) (BITMAPWORD(b,i) &= ~BITMAPBIT(b,i))
# define SETBIT(b,i) (BITMAPWORD(b,i) |= BITMAPBIT(b,i))

# define di_nx			di_numextents  
# define di_x			di_u.di_extents

# define EXTSPERDINODE		EFS_DIRECTEXTENTS
# define EFS_EXTENT_MAGIC	0
# define EXTSPERBB		(BBSIZE/sizeof (struct extent))

typedef struct efs_dinode	DINODE;

struct bufarea {
	struct bufarea	*b_next;		/* must be first */
	efs_daddr_t	b_bno;
	union {
		char	b_buf[BBSIZE];		/* buffer space */
		short	b_lnks[1];		/* link counts */
		struct efs b_fs;		/* super block */
		struct efs_dinode b_dinode[1];	/* inode block */
		struct extent b_ext[1];		/* extent */
		DIRECT b_dir[1];		/* directory */
	} b_un;
	char	b_dirty;
};
typedef struct bufarea BUFAREA;

# define BMSET		0
# define BMGET		1
# define BMCLR		2

# define NDIRECT	(BBSIZE/sizeof(struct efs_direct))

# define NO		0
# define YES		1

/* values for result of sb_sizecheck */

#define SB_OK		0
#define SB_BAD		1
#define SB_TOOBIG	2

# define MAXDUP		500		/* limit on dup blks (per inode) */
# define MAXBAD		500		/* limit on bad blks (per inode) */

# define STEPSIZE	7		/* default step for freelist spacing */
# define CYLSIZE	400		/* default cyl size for spacing */
# define MAXCYL		1000		/* maximum cylinder size */

# define BITSPERWORD	(sizeof(u_long) * (size_t)NBBY)	/* log2(BITSPERWORD) */
# define WORDSHIFT	5		/* log2(BITSPERWORD) */
# define WORDMASK	(BITSPERWORD - 1)

extern u_long bitmask[];

# define LSTATE		3		/* bits per inode state */
# define STATEPB (BITSPERBYTE/LSTATE)	/* inode states per byte */
# define SMASK		07		/* mask for inode state */
# define USTATE		0		/* inode not allocated */
# define FSTATE		01		/* inode is file */
# define DSTATE		02		/* inode is directory */
# define CLEAR		03		/* inode is to be cleared later */
# define TRASH		04		/* inode is to be cleared now */
#ifdef AFS
# define VSTATE		05		/* inode is an AFS inode */
#endif
# define EMPT		32		/* empty directory? */

# define sfiletype(sp)	((sp)->st_mode & S_IFMT)
# define dfiletype(dp)	((dp)->di_mode & IFMT)
# define ALLOC		(dfiletype(dp) != 0)
# define REG		(dfiletype(dp) == IFREG)
# define DIR		(dfiletype(dp) == IFDIR)
# define LNK		(dfiletype(dp) == IFLNK)
# define BLK		(dfiletype(dp) == IFBLK)
# define CHR		(dfiletype(dp) == IFCHR)
# define BLKLNK		(dfiletype(dp) == IFBLKLNK)
# define CHRLNK		(dfiletype(dp) == IFCHRLNK)
# define FIFO		(dfiletype(dp) == IFIFO)
# define SOCK		(dfiletype(dp) == IFSOCK)
# define SPECIAL 	(BLK || CHR || BLKLNK || CHRLNK)
# define ftypeok(dp)	(REG||DIR||LNK||FIFO||SOCK||SPECIAL)

# define MAXPATH	8000		/*
					 * max size for pathname string.
					 * Increase and recompile if pathname
					 * overflows.
					 */

extern unsigned niblk;			/* num of blks in raw area */

extern BUFAREA	sblk;			/* file system superblock */

#define initbarea(x)	(x)->b_dirty = 0;(x)->b_bno = (efs_daddr_t)-1
#define dirty(x)	(x)->b_dirty = 1

#define dirblk		fileblk.b_un.b_dir
#define superblk	sblk.b_un.b_fs

struct filecntl {
	int	rfdes;
	int	wfdes;
	int	mod;
};

extern struct filecntl	dfile;		/* file descriptors for filesys */

#define	DUPTBLSIZE	20000		/* num of dup blocks to remember */
extern efs_daddr_t	duplist[];	/* dup block table */
extern efs_daddr_t	*enddup;	/* next entry in dup table */

extern usema_t *dup_lock;
#define DUP_LOCK() if (dup_lock) { uspsema(dup_lock); }
#define DUP_UNLOCK() if (dup_lock) { usvsema(dup_lock); }

#define MAXLNCNT	200		/* num zero link cnts to remember */
extern efs_ino_t badlncnt[];		/* table of inos with zero link cnts */
extern u_long	badln;			/* next entry in table */

extern char	tflag;			/* scratch file specified */
extern char	qflag;			/* less verbose flag */
extern char	sflag;			/* salvage freeblocks */
extern char	csflag;			/* salvage freeblocks (conditional) */
extern char	nflag;			/* assume a no response */
extern char	yflag;			/* assume a yes response */
extern char	gflag;			/* "gentle" mode (like 'preen'). */
extern char 	condflag;		/* check only if dirty */
extern char	multflag;		/* run parallel passes in background */
extern char	rplyflag;		/* any questions asked? */
extern char	Dirc;			/* extensive directory check */
extern char	fast;			/* fast check- dups and freelist */
extern char	hotroot;		/* checking root device */
extern char	rawflg;			/* read raw device */
extern char	fixfree;		/* corrupted free list */
extern char	*statemap;		/* ptr to inode state table */
extern char	*pathp;			/* pointer to pathname position */
extern char	*thisname;		/* ptr to current pathname component */
extern char	*srchname;		/* name being searched for in dir */
extern char	pathname[];
extern char	devname[];
extern char	initdone;

extern int 	pass;

extern short	*lncntp;		/* ptr to link count table */
extern char 	*disk_bitmap;		/* ptr to buffer for disk bitmap */

extern char 	*blkmap;		/* ptr to block use bitmap */
extern usema_t *bmap_lock;
	
extern int	cylsize;		/* num blocks per cylinder */
extern int	stepsize;		/* num blocks for spacing purposes */

extern efs_ino_t max_inodes;		/* number of inodes */
extern efs_ino_t foundino;		/* i number found by searching a dir */
extern efs_ino_t lfdir;			/* lost & found directory */
extern efs_ino_t orphan;		/* orphaned inode */

extern efs_daddr_t bitmap_blocks;	/* num bitmap blocks in fs */
extern efs_daddr_t data_blocks;		/* num data blocks in fs */
extern efs_daddr_t inode_blocks;	/* num inode blocks in fs */

extern efs_daddr_t	fmin;		/* number of lowest valid data block */
extern efs_daddr_t	fmax;		/* number of blocks in the volume */

/* This flag is set in parallel mode when some significant change is made
 * (eg inode cleared or directory entry wiped) to alert the user that
 * there may be something to check in the logfile.
 */

extern char fixdone;

/* This flag is set if the root directory is lost. */

extern char lostroot;

/* directory for saving files */

extern char *savedir;

/* possible exit codes. Normal OK exit is of course 0. */

#define GSTATEABORT	0x04
#define FIXDONECODE  	0x08	/* passed child -> parent */
#define FATALEXIT	0xff

#define minsz(x,y)	(x>y ? y : x)
#define howmany(x,y)	(((x)+((y)-1))/(y))
#define roundup(x,y)	((((x)+((y)-1))/(y))*(y))
#define outrange(x)	(x < fmin || x >= fmax)
#define ioutrange(x)	(x < EFS_ROOTINO || x > max_inodes)
#define zapino(x)	clear((x),sizeof(DINODE))

#define setlncnt(x)	dolncnt(x,0)
#define getlncnt()	dolncnt(0,1)
#define declncnt()	dolncnt(0,2)

#define setbmap(x)	maphack(x,BMSET)
#define getbmap(x)	maphack(x,BMGET)
#define clrbmap(x)	maphack(x,BMCLR)

#define setstate(x)	statemap[inum] = x
#define getstate()	statemap[inum]

#define ADDR	0	/* check arg: check addresses */
#define DATA	1	/* check arg: do dirscan */
#define DIRSCAN	DATA
#define DEMPT	3	/* check arg: check dir empty */

#define ALTERD	010	/* return flag: changed */
#define KEEPON	004	/* return flag: keep checking */
#define SKIP	002	/* return flag: skip */
#define STOP	001	/* return flag: stop */
#define REM	007	/* return flag: remove */

/* values for flag passed to 'block-handling' function when scanning through
 * an inode */

#define	FIRSTBLOCK	1	/* first actual block of inode's data blocks */
#define INDIRBLOCK	2	/* Block is an indirect extent */

extern char	id;

# define copy(s, t, n)		memcpy(t, s, n)
# define clear(t, n)		memset(t, 0, n)

/* values for dstateflag */

#define DNODOT		0x1	/* no entry for dot */
#define DNODOTDOT	0x2	/* no entry for dotdot */
#define DFIXED		0x4	/* one or more blocks was corrupted and has 
				 * been fixed */
#define DCORRUPT	0x8	/* one or more blocks was corrupted and has 
				 * NOT been fixed */
#define DBADBLOCK	0x10	/* One or more blocks pointed to by the extent 
				 * info was not in a valid data block area */

/* define for the hashed-linked-list buffering that stores directory blocks
 * and blocks containing directory inodes to avoid random I/O after phase 1.
 */

#define NHASHHEADS	128	/* should be power of 2 for efficiency */

#ifdef AFS
/* this is stolen from afs/auxinode.h */
#define IS_DVICEMAGIC(dp)       (((dp)->di_version == EFS_IVER_AFSSPEC || \
                                  (dp)->di_version == EFS_IVER_AFSINO) \
                                 ?  1 : 0)
#define CLEAR_DVICEMAGIC(dp)    dp->di_version = EFS_IVER_EFS

#define VICEINODE	(REG && IS_DVICEMAGIC(dp))
extern int nViceFiles;

#endif

extern int lfproblem;
extern char *lfname;
extern char *disk_bitmap;
extern int pass1check;

struct externs_per_checker {
	char	Extentbuf[EFS_MAXEXTENTLEN * BBSIZE];
	int	Saveextentblk;
	int	Saveextentlen;
	efs_daddr_t	Startib;	/* blk num of first in raw area */
	BUFAREA	Inoblk;			/* inode blocks */
	BUFAREA	Fileblk;		/* other blks in filesys */
	char 	*Inodearea;		/* ptr to buffer for reading inodes */
	char 	*Mdcache;		/* ptr to buffer for metadata */
	int 	*Mdbits;		/* ptr to loaded bitmap for metadata */
	int 	Mdcount;		/* ptr to count of loaded buffers */
	int	Badblk;			/* num of bad blks seen (per inode) */
	int	Dupblk;			/* num of dup blks seen (per inode) */
	off_t	Filsize;		/* num blks seen in file */
	efs_ino_t Inum;			/* inode we are currently working on. */
	int	(*Pfunc)();		/* function to call to chk blk */
	u_long	Lastino;		/* hiwater mark of inodes */
	u_long	N_blks;			/* number of blocks used */
	u_long	N_files;		/* number of files seen */
	efs_daddr_t Startmdcache;	/* start of metadata cache */
	efs_daddr_t Endmdcache;		/* end of metadata cache */
	long Dir_size;			/* size of directory being checked */
	int Dstateflag;
	int Dentrycount;	/* # valid entries (excluding . and ..) */
	int Io_report;
	int Procn;
};

extern struct externs_per_checker **sproc_pp;

#define sproc_p (*sproc_pp)

#define extentbuf	sproc_p->Extentbuf
#define saveextentblk	sproc_p->Saveextentblk
#define saveextentlen	sproc_p->Saveextentlen
#define startib	sproc_p->Startib
#define inoblk	sproc_p->Inoblk
#define	fileblk sproc_p->Fileblk
#define inodearea	sproc_p->Inodearea
#define mdcache	sproc_p->Mdcache
#define mdbits	sproc_p->Mdbits
#define mdcount	sproc_p->Mdcount
#define badblk	sproc_p->Badblk
#define dupblk	sproc_p->Dupblk
#define filsize	sproc_p->Filsize
#define inum	sproc_p->Inum
#define pfunc	sproc_p->Pfunc
#define lastino	sproc_p->Lastino
#define n_blks	sproc_p->N_blks
#define n_files	sproc_p->N_files
#define startmdcache	sproc_p->Startmdcache
#define endmdcache	sproc_p->Endmdcache
#define dir_size	sproc_p->Dir_size
#define dstateflag	sproc_p->Dstateflag
#define dentrycount	sproc_p->Dentrycount
#define io_report	sproc_p->Io_report
#define procn	sproc_p->Procn

#define PERM 0
#define TEMP 1
#define BCACHE_LOCK() if (bcache_lock) { uspsema(bcache_lock); }
#define BCACHE_UNLOCK() if (bcache_lock) { usvsema(bcache_lock); }

extern usema_t *bcache_lock;
extern barrier_t *mp_barrier;
extern int nsprocs;

extern void Phase1(void);
extern void Phase2(void);
extern void Phase3(void);
extern void Phase4(void);
extern void Phase5(void);
extern void Phase6(void);
extern int _mem_reserve(int size, int newstate, int wait);
extern int addentry(struct efs_dirblk *db, char *name, efs_ino_t num);
extern void adjust(short lcnt);
extern void all_exit(void);
extern void banner(char *name);
extern void bcacheinit(void);
extern int bcachempinit(void);
extern int bcload(unsigned blk, char *buf);
extern char *bclookup(unsigned blk);
extern void bcmddump(void);
extern char *blkis(efs_daddr_t blk, char *buf);
extern void blkerr(char *s, efs_daddr_t blk, int severe);
extern int bread(char *buf, efs_daddr_t blk, int size);
extern void busyunavail(char *bmap);
extern int bwrite(char *buf, efs_daddr_t blk, int size);
extern void cache_ddot(efs_ino_t in, efs_ino_t to);
extern void catch(void);
extern void check(char *dev);
extern int chgdd(DIRECT *dirp);
extern void child_launch(char *name, int logfd);
extern int chk_ext(struct extent *xp, int (*func)(), int stopper,
	efs_daddr_t *_firstoff, efs_ino_t pinum, efs_ino_t curinum);
extern int chk_extlist(struct extent *xp, int nx, int (*func)(), int stopper,
	efs_daddr_t *_firstoff, efs_ino_t pinum, efs_ino_t curinum);
extern int chk_iext(struct extent *xp, int nx, int (*func)(), int stopper,
	efs_daddr_t *(_firstoff), efs_ino_t pinum, efs_ino_t curinum,
	DINODE *dp);
extern int chk_zeroext(struct extent *ixp, int *nxp, struct extent *dxp,
	efs_daddr_t firstoff, DINODE *dp, struct extent *bixp);
extern int chkblk(efs_daddr_t blk, int checkdot, efs_ino_t pinum,
	efs_ino_t curinum);
extern int chkeblk(efs_daddr_t bn);
extern int chkempt(DINODE *dp);
extern int choose_nsprocs(void);
extern void ckfini(void);
extern int ckinode(DINODE *dp, int flg, efs_ino_t pinum);
extern void clear_cyl_map(void);
extern void clear_ddot(void);
extern void closefiles(void);
extern void clri(char *s, int flg);
extern void clrinode(DINODE *dp);
extern int copyoff(DINODE *dp, int ino);
extern int countargs(FILE *fp);
extern void cylpass1(int cyl);
extern void descend(efs_ino_t pinum);
extern void dircompact(struct efs_dirblk *db, struct efs_dent *dep, int slot);
extern int direrr(char *s);
extern int direvaluate(void);
extern int dirscan(efs_daddr_t blk, int firstblk, efs_ino_t pinum,
	efs_ino_t curinum);
extern char *dname(char *name);
extern int dolncnt(short val, int flg);
extern int dup_record(efs_daddr_t blk);
extern int dupmpinit(void);
extern long efs_cksum(ushort *sp, int len);
extern long efs_supersum(struct efs *sp);
extern int eread(int fd, char *buf, int size);
extern void errexit(char *fmt, ...);
extern int ewrite(int fd, char *buf, int size);
extern struct extent *getindirs(DINODE *dp, int ino);
extern void exterr(char *s, struct extent *xp);
extern void fastfreechk(void);
extern void fbdirty(void);
extern void findgoodstart(int *bcount, efs_daddr_t *firstbn,
	efs_daddr_t *blkcount);
extern int findino(DIRECT *dirp);
extern void flush(BUFAREA *bp);
extern void flushall(void);
extern void free_phase1_resources(void);
extern void freealltmpmap(void);
extern void freetmpmap(void *addr, int len);
extern void fsck_from_fstab(void);
extern void gerrexit(void);
extern efs_ino_t get_ddot(efs_ino_t in);
extern int get_sysmem(void);
extern BUFAREA *getblk(BUFAREA *bp, efs_daddr_t blk);
extern int getcyl(int cyl);
extern int getline(FILE *fp, char *loc, int maxlen);
extern DINODE *ginode(void);
extern int grabextent(int blk, unsigned len);
extern void iderrexit(char *fmt, ...);
extern void iderror(char *fmt, ...);
extern void idprintf(char *fmt, ...);
extern void infanticide(void);
extern void init_bitmask(void);
extern void init_bmap(void);
extern void inodirty(void);
extern int insert_mdbcache(efs_daddr_t smd, efs_daddr_t emd, char *buf);
extern void invalidate_mdcache(void);
extern int is_data_block(efs_daddr_t blk);
extern int isdir(char *buf);
extern int isinode(efs_daddr_t blk);
extern int linkup(void);
extern void logclean(char *name);
extern int makelogfd(char *name);
extern int maphack(efs_daddr_t blk, int flg);
extern int mdb_greater_than(efs_daddr_t smd);
extern int mdb_less_than_or_equal(efs_daddr_t smd);
extern void mem_preserve(int size);
extern void mem_ready(void);
extern void mem_release(int size);
extern int mem_reserve(int size, int newstate, int wait);
extern void mem_wait(int size);
extern void mem_wakeup(void);
extern int memsetup(void);
extern int mkentry(DIRECT *dirp, int size);
extern void mpinit(void);
extern void newline(void);
extern int pass1(efs_daddr_t blk, int blockflag);
extern int pass1b(efs_daddr_t blk);
extern int pass1i(efs_daddr_t blk, int len, int stopper);
extern int pass1i_badblock(efs_daddr_t blk);
extern int pass1i_badextent(efs_daddr_t blk, int len);
extern int pass2(DIRECT *dirp, int size, efs_ino_t curinum);
extern void phase1mdread(void);
extern void phase4fstate(void);
extern void pinode(void);
extern void reap(void);
extern int reply(char *s);
extern void rm_ddot(efs_ino_t in);
extern void rwerr(char *s, efs_daddr_t blk);
extern void sb_clean(void);
extern int sb_sizecheck(struct efs *sp);
extern void sbdirty(void);
extern int sbsetup(char *dev, int rofs);
extern void sizechk(DINODE *dp);
extern void sproc_Phase1(void *foo);
extern void sproc_exit(void);
extern long sumsize(DINODE *dp);
extern int supersizerr(char *fmt, ...);
extern void *tmpmap(int size);
extern int trunc_exlist(extent *xp, int nex, off_t to, int *tossed);
extern void truncinode(DINODE *dp, off_t to);
extern int unwind_map(efs_daddr_t blk);
extern int writeextent(void);

#ifdef AFS
extern char *unrawname(char *cp);
#endif
