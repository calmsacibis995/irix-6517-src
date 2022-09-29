/*
 *  iso.c
 *
 *  Description:
 *      Routines called from nfs_server that know all about the iso 9660
 *      file system format
 *
 *  Note:
 *      Thie file descriptor of a directory is The disc offset in
 *      bytes of its self-referential directory entry.  It used to be
 *      the directory entry in the parent directory, but there are
 *      some buggy CDs out there that have bogus ".." entries that
 *      messed up that scheme.  --rogerc 3/20/96
 *
 *      Caching:
 *          Directory and extended attribute blocks are cached in the
 *          cdrom module.  Data file blocks are not cached; client NFS
 *          takes care of this, so there's no need to use the memory.
 *          This module controls which blocks are cached via the last
 *          parameter to cd_read.
 *
 *  History:
 *      rogerc      12/21/90    Created
 *      rogerc      01/25/91    Cache only directory blocks
 *      rogerc      01/25/91    Changes for gfs
 *      rogerc      04/02/91    Adding support for High Sierra
 *      rogerc      02/12/92    Added support for block sizes other than
 *                              2048
 */

#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <rpcsvc/ypclnt.h>

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <filehdr.h>
#include <ctype.h>
#include <exportent.h>
#include <netdb.h>
#include <elf.h>

#include "iso.h"
#include "util.h"
#include "main.h"

#define ROOT_DIR() (cd_voldesc(cd) + (int)&((struct p_vol_desc *)(0))->root)

#define HS_ROOT_DIR() (cd_voldesc(cd) + (int)&((hsvol_t *)(0))->root)

#define ISO_MAXNAMLEN   (30 + 2 + 5) 

#define PERM_RALL   0444
#define PERM_XALL   0111
#define PERM_RXALL  0555

#define max(a,b) ((a) < (b) ? (b) : (a))

#define CHARSTOLONG(chars) ((chars)[0] << 24 | (chars)[1] << 16 \
			    | (chars)[2] << 8 | (chars)[3])
#define CHARSTOSHORT(chars) (chars[0] << 8 | chars[1])

#define DIRTOFILE(dirp,fp) {\
	(fp)->block =  CHARSTOLONG((dirp)->ext_loc_msb);\
	(fp)->xattr_len = (dirp)->xattr_len;\
	(fp)->int_gap = (dirp)->int_gap;\
	(fp)->fu_size = (dirp)->fu_size;\
}

#define ISLASTDIRENT(dirp,ent,contents,len) \
    (!((char *)ent-((char *)contents) < len - dirp->xattr_len && ent->len_dir))

/*
 * Construct a file descriptor given the directory entry of its
 * parent, the address of its contents (base), the offset into the
 * contents of the directory entry (dirp), and the block size of the cd.
 */
#define MKFD(parent,base,dirp,blksize) \
    (dirp && (((dir_t *)dirp)->flags & FFLAG_DIRECTORY) \
     ? ((CHARSTOLONG(((dir_t *)dirp)->ext_loc_msb) \
	 + ((dir_t *)dirp)->xattr_len) * blksize) \
     : ((CHARSTOLONG(parent->ext_loc_msb) + parent->xattr_len) * blksize \
	+ ((char *)dirp - (char *)base)))

#ifdef DEBUG
#define ISODBG(x) {if (isodebug) {x;}}
#else
#define ISODBG(x)
#endif

#define FLAGS(d) (fsType == ISO ? ((dir_t *)d)->flags : \
 ((hsdir_t *)d)->flags)

#define IS_LEAPYEAR(year) \
 ((year%4 == 0) && ((year%100 != 0) || (year%400 == 0)))

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/*
 * Special stuff so we can recognize SUN executables.  This is the
 * format of the executable file header on a 68000 or SPARC based
 * SUN system.  Their 386i line is handled by COFF.
 */
#define sun
struct exec {
#ifdef sun
    unsigned char   a_dynamic:1;    /* has a __DYNAMIC */
    unsigned char   a_toolversion:7;/* version of toolset
				       used to create this file */
    unsigned char   a_machtype;     /* machine type */
    unsigned short  a_magic;        /* magic number */
#else
    unsigned long   a_magic;        /* magic number */
#endif
    unsigned long   a_text;         /* size of text segment */
    unsigned long   a_data;         /* size of initialized data */
    unsigned long   a_bss;          /* size of uninitialized data */
    unsigned long   a_syms;         /* size of symbol table */
    unsigned long   a_entry;        /* entry point */
    unsigned long   a_trsize;       /* size of text relocation */
    unsigned long   a_drsize;       /* size of data relocation */
};

#define OMAGIC  0407            /* old impure format */
#define NMAGIC  0410            /* read-only text */
#define ZMAGIC  0413            /* demand load format */

#define N_BADMAG(x) \
	((x).a_magic!=OMAGIC && (x).a_magic!=NMAGIC && (x).a_magic!=ZMAGIC)
#undef sun

/*
 * Sometimes we need to differentiate between iso9660 and high sierra
 */
typedef enum cdtype { ISO, HSFS } cdtype_t;

/*
 * Structure that we parse rock ridge extensions into.  The flags at
 * the beginning get set when we encounter the corresponding system
 * use record; we need this because not all fields are necessarily
 * present and we need to know to fall back onto base ISO 9660.
 */
typedef struct _ext {
    unsigned int gotName : 1;
    unsigned int gotPath : 1;
    unsigned int gotMode : 1;
    unsigned int gotATime : 1, gotMTime : 1, gotCTime : 1;
    unsigned int isRelocated : 1;
    unsigned int doSlash : 1;
    char name[NFS_MAXNAMLEN];
    char path[NFS_MAXNAMLEN];
    u_int mode;
    u_int nlink;
    u_int uid, gid;
    ftype type;
    nfstime atime, mtime, ctime;
    u_int rdev;
    int parent, child;
} EXT;

static char dot[] = ".";
static char dotdot[] = "..";

/*
 * "notranslate" controls the "to_unixname" name translation.
 *
 *	'c' = the user will see raw, ugly file names
 *	'l' = the user will see lower-case, ugly file names
 *	'm' = the user will see upper-case names less the version #
 *	'x' = the user will see lower-case names less the version #
 */
static char notranslate = 'x';		/* The internal default,	*/
					/* acts like 'l'+'m'		*/

/*
 * if setx == 1, we'll blindly set execute permissions for every file.
 */
static int  setx = 0;
static int  def_num_blocks = 256;  /* backup default */

static int  useCache = 1;

/*
 * By default, use rock ridge extensions if they're there.
 */
static int  useExt = 1;

/*
 * Whether or not CD has extensions
 */
static int  hasExt = 0;

/*
 * Number of bytes in each system use area to skip
 */
static int skipSysBytes = 0;

/*
 * The location on the CD of the '.' entry of the root directory.
 * This is use in ext_first_field when reading Rock Ridge extensions
 * to determine whether or not to skip skipSysBytes.
 */
static int rootSelfEntry = 0;

int isodebug = 0;

/*
 * Stuff for the cdrom module
 */
static CDROM *cd;

/*
 * Number of blocks in the mounted file system
 */
static int fsBlocks;

/*
 * Type of file system - iso9660 or high sierra
 */
static cdtype_t fsType;

/*
 * The root file handle of the mounted file system
 */
static fhandle_t rootfh;

/*
 * The path name of the mount point of the file system
 */
static char *mountPoint;

/*
 * Forward declarations of static functions
 */
static int set_blocks();
static int get_parent_fd(int fd, int *fdp);
static void add_entry(entry *entries, int fd, char *name, int
		      name_len, int cookie);
static int read_dir(dir_t *dirp, int dirfd, dir_t **datap, int *len);
static int read_dir_entry(int fd, dir_t **dpp);
static void convert_time(struct nfstime *to, struct date_time *from,
			 enum cdtype type);
static int convert_to_seconds(int year, int month, int day, int hour,
			      int min, int sec, int csec, int gw);
static int valid_day(int day, int month, int year);
static int days_so_far(int day, int month, int year);
static char * to_unixname(char *name, int length);

/*
 * Rock Ridge support functions
 */
static void check_for_extensions(void);
static void ext_parse_extensions(dir_t *dirp, int fd, EXT *ext);
static int ext_get_relo_dir(EXT *ext, dir_t *inDir, int infd,
			    dir_t **outDir, int *outfd);

/*
 * Support for keeping one directory entry privately "cached".
 */
static int read_dir_entry_cached(int fd, dir_t **dpp, multi_xtnt_t **mpp);

static int		 dirc_hit     =  0;
static int		 dirc_miss    =  0;
static int		 dirc_last_fd = -1;
static dir_t		 *dirc_dirp   = NULL;
static multi_xtnt_t	 *dirc_mep    = NULL;


/*
 * Level 3 (Multi-Extent) support data & functions
 */

/*
 * Must be a power of 2!
 */
#define	MULTI_XTNT_HASH_SIZE	16

/*
 * Most (all that I've seen) fileid's are even numbers,
 * so drop the bottom bit in the hash function.
 */
#define	MULTI_XTNT_HASH(Fd)	((Fd >> 1) & (MULTI_XTNT_HASH_SIZE-1))

static multi_xtnt_t *multi_extent_head[MULTI_XTNT_HASH_SIZE];
static multi_xtnt_t *multi_extent_tail[MULTI_XTNT_HASH_SIZE];


static void
iso_me_add_xtnt(multi_xtnt_t	*mep,
		int		fileid,
		int		blksize,
		int		start_blk,
		int		length)
{
    one_xtnt_t *oep;


    oep = safe_malloc(sizeof(one_xtnt_t));

    oep->oe_fileid    = fileid;
    oep->oe_start_blk = start_blk;
    oep->oe_length    = length;
    oep->oe_next      = NULL;

    oep->oe_start_byte = mep->me_length;

    mep->me_length += length;

    mep->me_num_xtnts++;

    if (mep->me_xtnts_head == NULL) {

	mep->me_xtnts_head = oep;
	mep->me_xtnts_tail = oep;

    } else {

	mep->me_xtnts_tail->oe_next = oep;
	mep->me_xtnts_tail          = oep;
    }
}


static multi_xtnt_t *
iso_me_add_file(int fileid)
{
    int			hash;
    multi_xtnt_t	*mep;


    hash = MULTI_XTNT_HASH(fileid);

    /*
     * Scan the chain to see if we already "know" this guy.
     */
    for (mep = multi_extent_head[hash]; mep != NULL; mep = mep->me_next) {
	
	if (mep->me_fileid == fileid)
	    return mep;
    }

    /*
     * Not found, so make a new one.
     */
    mep = safe_malloc(sizeof(multi_xtnt_t));

    mep->me_fileid = fileid;
    mep->me_next   = NULL;;

    mep->me_xtnts_head  = NULL;
    mep->me_xtnts_tail  = NULL;

    mep->me_length    = 0;
    mep->me_num_xtnts = 0;

    if (multi_extent_head[hash] == NULL) {

	multi_extent_head[hash] = mep;
	multi_extent_tail[hash] = mep;

    } else {

	multi_extent_tail[hash]->me_next = mep;
	multi_extent_tail[hash]          = mep;
    }

    return mep;
}


static multi_xtnt_t *
iso_me_search(int fileid)
{
    int			hash;
    multi_xtnt_t	*mep;


    hash = MULTI_XTNT_HASH(fileid);

    /*
     * Scan the chain to see if we already "know" this guy.
     */
    for (mep = multi_extent_head[hash]; mep != NULL; mep = mep->me_next) {
	
	if (mep->me_fileid == fileid)
	    return mep;
    }

    return NULL;
}


void
iso_me_flush()
{
    int			hash;
    multi_xtnt_t	*mep;
    multi_xtnt_t	*mep2free;
    one_xtnt_t		*oep;
    one_xtnt_t		*oep2free;

    ISODBG(fprintf(stderr, ">>>>iso: iso_me_flush\n"));

    /*
     * Scan the chains
     */
    for (hash = 0; hash < MULTI_XTNT_HASH_SIZE; hash++) {

	mep = multi_extent_head[hash];

	while (mep) {
	
	    mep2free = mep;

	    oep = mep->me_xtnts_head;

	    while (oep) {
		oep2free = oep;

		oep = oep->oe_next;

		free(oep2free);
	    }

	    mep = mep->me_next;

	    free(mep2free);
	}

	multi_extent_head[hash] = NULL;
	multi_extent_tail[hash] = NULL;
    }
}


void
iso_me_dump()
{
    int		i;
    int		hash;
    multi_xtnt_t *mep;
    one_xtnt_t	 *oep;

    /*
     * Scan the chains
     */
    for (hash = 0; hash < MULTI_XTNT_HASH_SIZE; hash++) {

	for (mep = multi_extent_head[hash];
		    mep != NULL;
			    mep = mep->me_next) {
	
	    fprintf(stderr,
    ">>>>iso: iso_me_dump: MULTI[%d] fileid/%d numxtnts/%d length/%d\n",
				    hash, mep->me_fileid, mep->me_num_xtnts,
				    mep->me_length);
	    i = 0;

	    oep = mep->me_xtnts_head;

	    while (oep) {
		fprintf(stderr,
    ">>>>iso: iso_me_dump:      %d: fd/%d stblk/%d start/%d length/%d\n",
				    i++, oep->oe_fileid,
				    oep->oe_start_blk,
				    oep->oe_start_byte,
				    oep->oe_length);

		oep = oep->oe_next;
	    }
	}
    }
}


/*
 *  static dir_t *
 *  skip_zero_dir(dir_t *dp, contents, len, int blksize)
 *
 *  Description:
 *      Skip past zero filled bits at the end of a block in a
 *      directory.  directory entries are supposed to end in the same
 *      block in which they begin, so if there's some space left over
 *      but not enough to hold an entire directory entry we need to
 *      skip over that space to the beginning of the next block.
 *
 *  Parameters:
 *      dp       Points to a directory entry
 *      contents Pointer to the beginning of the directory into which
 *               dp points
 *      len      len of the directory pointed to by contents
 *      blksize  Size of a block on this CD
 *
 *  Returns:
 *	dp if it fits
 */

static dir_t *
skip_zero_dir(dir_t *dp, dir_t *contents, int len, int blksize)
{
    int block;

    if (!dp->len_dir && (char *)dp - (char *)contents < len) {
	/*
	 * Directories are supposed to end in the same block in
	 * which they begin, so this starts at the next block.  The
	 * zero in dp->len_dir is zero fill from the unused portion of
	 * a block in the middle of a directory.
	 */
	block = ((char *)dp - (char *)contents +
		 blksize - 1) / blksize;
	if (block * blksize < len) {
	    dp = (dir_t *)
		((char *)contents + block * blksize);
	}
    }
    return dp;
}

/*
 *  static dir_t *
 *  first_dir(dir_t *dirp, dir_t *contents, int len, int cookie, int blksize)
 *
 *  Description:
 *      Return the first directory entry found in contents, or the
 *      directory entry found cookie bytes into the contents.
 *
 *  Parameters:
 *      dirp      dir_t structure of directory whose contents we're
 *                looking at
 *      contents  Contents of the directory
 *      len       length of contents
 *      cookie    Byte offset into the directory to start.  This
 *                exists because of the way nfs does readdir.
 *      blksize   The blocksize used by the CD.  This is needed for
 *                calculating when we've reached the end of a block
 *
 *  Returns:
 *      Pointer to the first directory entry in contents
 */

static dir_t *
first_dir(dir_t *dirp, dir_t *contents, int len, int cookie, int blksize)
{
    dir_t *dp;
    int block;

    dp = (dir_t *)((char *)contents + cookie);
    dp = skip_zero_dir(dp, contents, len, blksize);
    return ISLASTDIRENT(dirp, dp, contents, len) ? NULL : dp;
}

/*
 *  static dir_t *
 *  next_dir(dir_t *dirp, dir_t *dp, dir_t *contents, int len, int blksize)
 *
 *  Description:
 *      Return a pointer to the next entry in a directory after dp, or
 *      NULL if dp is the last entry.
 *
 *  Parameters:
 *      dirp      directory whose contents we're searching
 *      dp        dp before the one we want
 *      contents  contents of the directory
 *      len       length of the directory contents
 *      blksize   The blocksize used by the CD.  This is needed for
 *                calculating when we've reached the end of a block
 *
 *  Returns:
 *      pointer to next directory, NULL if dp was the last one.
 */

static dir_t *
next_dir(dir_t *dirp, dir_t *dp, dir_t *contents, int len, int blksize)
{
    int block;

    dp = (dir_t *)((char *)dp + dp->len_dir);

    dp = skip_zero_dir(dp, contents, len, blksize);

    return ISLASTDIRENT(dirp, dp, contents, len) ? NULL : dp;
}

/*
 *  void
 *  iso_init(void)
 *
 *  Description:
 *      Initialization for the iso module
 */

void
iso_init(int num_blocks)
{
    int	i;


    def_num_blocks = num_blocks;
    useCache = useCache && num_blocks > 0;

    for (i = 0; i < MULTI_XTNT_HASH_SIZE; i++) {
	multi_extent_head[i] = NULL;
	multi_extent_tail[i] = NULL;
    }
}

/*
 *  int
 *  iso_numblocks(unsigned int *blocks)
 *
 *  Description:
 *      Fill in blocks with the number of blocks in the file system
 *
 *  Parameters:
 *      blocks  receives # of blocks
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
iso_numblocks(unsigned int *blocks)
{
    int   error, changed;
    
    error = cd_media_changed(cd, &changed);

    if (error)
	return error;

    if (changed) {
	cd_flush_cache(cd);

	iso_me_flush();
    }

    error = set_blocks();

    if (error)
	return error;
    
    *blocks = (unsigned int)fsBlocks;

    ISODBG(fprintf(stderr, ">>>>iso: iso_numblocks() == %d\n", *blocks));

    return 0;
}

/*
 *  int
 *  iso_isfd(int fd)
 *
 *  Description:
 *      Determine whether fd is a file descriptor that we care about, so
 *      that the caller knows not to close it
 *
 *  Parameters:
 *      fd      file descriptor
 *
 *  Returns:
 *      1 if we care, 0 if we don't
 */

int
iso_isfd(int fd)
{
    return cd_is_dsp_fd(cd, fd);
}

/*
 *  int
 *  iso_openfs(char *dev, char *mntpnt, int flags, int partition)
 *
 *  Description:
 *	Get ready to mount the dev on mntpnt
 *
 *  Parameters:
 *      dev         Device for CD-ROM drive
 *      mntpnt      The mount point
 *      root        gets root file handle
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
iso_openfs(char *dev, char *mntpnt, fhandle_t *root)
{
    int                 error;
    static dev_t        fakedev = 1;
    struct stat         sb;
    
    ISODBG(fprintf(stderr, ">>>>iso: iso_addfs(%s)\n", dev));
    
    error = cd_open(dev, def_num_blocks, &cd);
    if (error)
	return error;
    
    error = cd_stat(cd, &sb);
    if (error)
	return error;

    rootfh.fh_dev = S_ISREG(sb.st_mode) ? 0xff00 | (fakedev++ & 0xff)
	: sb.st_rdev;
    mountPoint = strdup(mntpnt);
    
    error = set_blocks();
    /*
     * We need to do this so that the user can eject.
     */
    if (error) {
	cd_close(cd);
    } else {
	bcopy(&rootfh, root, sizeof *root);
    }
    
    return error;
}

/*
 *  void
 *  iso_removefs()
 *
 *  Description:
 *      ISO 9660 specific removal of a generic file system
 *
 *  Parameters:
 */

void
iso_removefs()
{
    cd_close(cd);

    ISODBG( iso_me_dump() );

    ISODBG(fprintf(stderr, ">>>>iso: read_dir_cached: hits/%d misses/%d\n",
						dirc_hit, dirc_miss));
}

/*
 *  int
 *  iso_lookup(fhandle_t *fh, char *name, fhandle_t *fh_ret)
 *
 *  Description:
 *      Look for name in fh (a directory); if found, fill in fh_ret
 *
 *  Parameters:
 *      fh      file handle of directory in which to look
 *      name    name of file we're looking for
 *      fh_ret  Receive handle for name
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
iso_lookup(fhandle_t *fh, char *name, fhandle_t *fh_ret)
{
    int		blksize;
    int		multi_seen = 0;
    int		error, fd, tmpfd, len, dirfd, newfd;
    char	*uname;
    dir_t	*dirp;      /* directory entry for dir */
    dir_t	*contents;  /* contents of dir */
    dir_t	*dp;        /* pointer into contents */
    dir_t	*newdirp;
    EXT		ext;
    
    ISODBG(fprintf(stderr, ">>>>iso: iso_lookup(%d, %s)\n", fh->fh_fno, name));

    blksize = cd_get_blksize(cd);
    
    if (strcmp(name, dot) == 0) {
	*fh_ret = *fh;
	return 0;
    } else if (strcmp(name, dotdot) == 0) {
	error = get_parent_fd(fh->fh_fno, &fd);
	if (error)
	    return error;
	bzero(fh_ret, sizeof *fh_ret);
	fh_ret->fh_dev = fh->fh_dev;
	fh_ret->fh_fid.lfid_fno = fd;
	fh_ret->fh_fid.lfid_len =
	    sizeof fh_ret->fh_fid - sizeof fh_ret->fh_fid.lfid_len;
	return 0;
    }

    error = read_dir_entry(fh->fh_fno, &dirp);

    if (error)
	return error;

    dirfd = fh->fh_fno;

    if (hasExt && useExt) {

	ext_parse_extensions(dirp, dirfd, &ext);

	error = ext_get_relo_dir(&ext, dirp, dirfd, &newdirp, &newfd);
	
	if (error) {
	    free(dirp);
	    return error;
	}	    

	if (dirp != newdirp) {
	    free(dirp);
	    dirp = newdirp;
	    dirfd = newfd;
	}
    }
    
    error = read_dir(dirp, dirfd, &contents, &len);
    
    if (error) {
	free(dirp);
	return error;
    }
    
    fd = 0;
    
    for (dp = first_dir(dirp, contents, len, 0, blksize);
		 dp;
			 dp = next_dir(dirp, dp, contents, len, blksize)) {

	tmpfd = MKFD(dirp, contents, dp, blksize);

	/*
	 * Skip associated files
	 */
	if (dp->flags & FFLAG_ASSOC)
	    continue;

	if (useExt && hasExt) {

	    ext_parse_extensions(dp, tmpfd, &ext);

	    /*
	     * Skip relocated directories
	     */
	    if (ext.isRelocated) {
		continue;
	    }

	    uname = ext.gotName ? ext.name
				: to_unixname(dp->file_id, dp->len_fi);
	} else {
	    uname = to_unixname(dp->file_id, dp->len_fi);
	}

	if (strcmp(name, uname) == 0) {
	    /*
	     * Only "remember" the fd of the 1st of
	     * a series for Multi-Extent files.
	     */

	    /*
	     * Do the mapping and return the inode number of the relocated child
	     * as opposed to the original number.  Why?  Places like getcwd()
	     * do keep track of the inode numbers to make sure that child directories
	     * inode numbers show up in a read of the parent's directory, which isn't
	     * the case if the mapping is not done.
	    */
	    if (! fd) {
		fd = (useExt && hasExt && ext.child) ? ext.child * blksize : tmpfd;
	    }

	    if (dp->flags & FFLAG_MULTI) {
		multi_xtnt_t *mep;

		/*
		 * Check to see if we already know the extents for this file.
		 */
		if (! multi_seen) {		/* 1st extent?		*/

		    mep = iso_me_search(fd);	/* Try to find them	*/

		    if (mep) {			/* If we found them:	*/

			ISODBG(fprintf(stderr,
			    ">>>>iso: iso_lookup: MULTI already built fd/%d\n",
							    fh->fh_fno));

			break;			/* that's all we need	*/
		    }
		}

		multi_seen++;			/* Into multi-extents	*/
						/* mode			*/
		mep = iso_me_add_file(fd);

		iso_me_add_xtnt(mep, tmpfd, blksize,
				    CHARSTOLONG(dp->ext_loc_msb),
				    CHARSTOLONG(dp->data_len_msb));

		ISODBG(fprintf(stderr,
">>>>iso: iso_lookup: MULTI=1 fd/%d tmpfd/%d ext_loc_msb/%d xattr_len/%d size/%d\n",
				    fd, tmpfd,
				    CHARSTOLONG(dp->ext_loc_msb),
				    dp->xattr_len,
				    CHARSTOLONG(dp->data_len_msb)));
	    
		continue;	/* Read the next directory entry */

	    } else {
		multi_xtnt_t *mep;

		/*
		 * Are we scanning a set of multi-extent entries?
		 */
		if (! multi_seen)
			break;

		mep = iso_me_add_file(fd);

		iso_me_add_xtnt(mep, tmpfd, blksize,
				    CHARSTOLONG(dp->ext_loc_msb),
				    CHARSTOLONG(dp->data_len_msb));

		ISODBG(fprintf(stderr,
">>>>iso: iso_lookup: MULTI=0 fd/%d tmpfd/%d ext_loc_msb/%d xattr_len/%d size/%d\n",
				    fd, tmpfd,
				    CHARSTOLONG(dp->ext_loc_msb),
				    dp->xattr_len,
				    CHARSTOLONG(dp->data_len_msb)));
	    

		break;		/* We've read enough	*/
	    }
	}
    }
    
    free(dirp);
    free(contents);
    
    if (fd) {
	bzero(fh_ret, sizeof *fh_ret);

	fh_ret->fh_dev = fh->fh_dev;
	fh_ret->fh_fid.lfid_fno = fd;
	fh_ret->fh_fid.lfid_len =
	    sizeof fh_ret->fh_fid - sizeof fh_ret->fh_fid.lfid_len;

	return 0;
    }
    
    return ENOENT;
}
		
/*
 *  int
 *  iso_read(fhandle_t *fh, int offset, int count, char *data,
 *           unsigned int *amountRead)
 *
 *  Description:
 *      Read count bytes from file fh into buffer data
 *
 *  Parameters:
 *      fh          file handle of file from which to read
 *      offset      offset into file to start reading
 *      count       number of bytes to read
 *      data        buffer to read bytes into
 *      amountRead  Amount of data actually read
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
iso_read(fhandle_t *fh, int offset, int count, char *data,
	 unsigned int *amountRead)
{
    int			error;
    long		fileSize;
    dir_t		*dirp;
    multi_xtnt_t	*mep;
    CD_FILE	f;
    
    ISODBG(fprintf(stderr, ">>>>iso: iso_read(fd/%d, offset/%d, count/%d)\n",
					fh->fh_fno, offset, count));
    
    /*
     * We use the privately "cached" directory entry because NFS reads
     * files in 8K chunks; if we're reading a large file sequentially,
     * we don't want to waste time re-reading the directory entry each time.
     */
    error = read_dir_entry_cached(fh->fh_fno, &dirp, &mep);

    if (error)
	return error;
    
    /*
     * Figure out where the end of the file is, and adjust count
     * accordingly.
     *
     * If this is a Multi-Extent file, and we found the extent
     * list information collected by lookup, use it.
     */
    if (mep)
	fileSize = mep->me_length;
    else
	fileSize = CHARSTOLONG(dirp->data_len_msb);

    if (offset >= fileSize) {
	*amountRead = 0;
	return 0;
    }

    DIRTOFILE(dirp, &f);

    count = MIN(count, fileSize - offset);

    /*
     * If this is a Multi-Extent file, and we found the extent
     * list information collected by lookup, use it to cycle
     * through the extents to find the right ones.
     */
    if (mep) {
	int		rem_count;
	int		now_count;
	int		now_offset;
	int		xtnt_num = 0;
	char		*buf;
	one_xtnt_t	*oep;

	buf = data;

	rem_count = count;

	*amountRead = 0;

	oep = mep->me_xtnts_head;

	while (oep && rem_count) {
	    long	xtnt_strt;
	    long	xtnt_end;

	    xtnt_strt = oep->oe_start_byte;
	    xtnt_end  = xtnt_strt + oep->oe_length;

	    if (offset >= xtnt_strt && offset < xtnt_end) {

		now_count = MIN(rem_count, xtnt_end - offset);

		f.block = oep->oe_start_blk;

		now_offset = offset - oep->oe_start_byte;

		ISODBG(fprintf(stderr,
	    ">>>>iso: iso_read(%d) MULTI:%d offset/%d noff/%d cnt/%d blk/%d\n",
							fh->fh_fno, xtnt_num,
							offset, now_offset,
							now_count, f.block));

		error = cd_read_file(cd, &f, now_offset, now_count, buf);

		if (error)
		    return error;
		    
		rem_count   -= now_count;
		offset      += now_count;
		buf         += now_count;
		*amountRead += now_count;
	    }

	    oep = oep->oe_next;
	    xtnt_num++;
	}

    } else {

	/*
	 * Single extent logic.
	 */
	error = cd_read_file(cd, &f, offset, count, data);

	if (! error)
	    *amountRead = count;
    }

    return error;
}

/*
 *  int
 *  iso_readdir(fhandle_t *fh, entry *entries, int count, int cookie,
 *   bool_t *eof, int *nread)
 *
 *  Description:
 *      Read up to count byes worth of directory entries from the directory
 *      described by fd
 *
 *  Parameters:
 *      fh          file handle of directory whose entries we want
 *      entries     Buffer to receive entries
 *      count       Maximum # of bytes to read into entries
 *      cookie      offset into directory entry from which to start
 *      eof         set to 1 if we've read the last entrie
 *      nread       set to the number of entries read
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
iso_readdir(fhandle_t *fh, entry *entries, int count, int cookie,
		bool_t *eof, int *nread)
{
    int			blksize;
    int			error, fileid, len, dirfd, newfd;
    int			free_dirp = 0;
    char		*uname;
    dir_t		*dirp, *contents, *dp, *newdirp;
    entry		*lastentry = 0, *entrybase = entries;
    /* REFERENCED */
    multi_xtnt_t	*mep;
    EXT			ext;
    
    ISODBG(fprintf(stderr, ">>>>iso: iso_readdir(%d, %d) == \n", fh->fh_fno,
								 cookie));
    *nread = 0;

    blksize = cd_get_blksize(cd);
    
    dirfd = fh->fh_fno;

    /*
     * We use the privately "cached" directory entry because there's a
     * high probability that this same file handle was just the subject
     * of a "get_attr", which followed a lookup and we don't want to waste
     * time re-reading the directory entry if we don't need to.
     */
    error = read_dir_entry_cached(dirfd, &dirp, &mep);

    if (error)
	return error;

    if (hasExt && useExt) {

	ext_parse_extensions(dirp, dirfd, &ext);

	error = ext_get_relo_dir(&ext, dirp, dirfd, &newdirp, &newfd);

	if (error)
	    return error;

	if (dirp != newdirp) {

	    dirp  = newdirp;
	    dirfd = newfd;

	    free_dirp = 1;
	}
    }
    
    error = read_dir(dirp, dirfd, &contents, &len);

    if (error) {

	if (free_dirp)
	    free(dirp);

	return error;
    }

    /*
     *  We won't even try if there isn't at least a self and parent
     *  reference.
     */
    if (len <  2 * sizeof *dp) {

	if (free_dirp)
	    free(dirp);

	free(contents);

	return EIO;
    }
    
    /*
     *  We do this to clue the nfs_server module into the fact that there
     *  are no entries left to be read.  It turns out that the NFS client
     *  won't listen to us when we set *eof == 0; the nfs_server module
     *  has to actually return a NULL entry pointer in order to convince
     *  the client that there are no more entries.
     */
    if (cookie >= len - dirp->xattr_len) {

	if (free_dirp)
	    free(dirp);

	free(contents);

	return 0;
    }

    /*
     *  Take care of self and parent entries
     */
    dp = first_dir(dirp, contents, len, cookie, blksize);

    if (dp == NULL) {

	if (free_dirp)
	    free(dirp);

	return EIO;
    }
    
    if (cookie == 0) {

	(*nread)++;

	add_entry(entries, fh->fh_fno, dot, strlen (dot),
		  ((char *)dp) - ((char *)contents) + dp->len_dir);

	lastentry = entries;

	entries = entries->nextentry;

	dp = next_dir(dirp, dp, contents, len, blksize);

	if (dp == NULL) {

	    if (free_dirp)
		free(dirp);

	    return EIO;
	}
    }
    
    if (cookie < 2 * sizeof (dir_t)) {

	(*nread)++;

	error = get_parent_fd(fh->fh_fno, &fileid);

	if (error) {

	    if (free_dirp)
		free(dirp);

	    free(contents);

	    return error;
	}

	add_entry(entries, fileid, dotdot, strlen (dotdot),
		  ((char *)dp) - ((char *)contents) + dp->len_dir);

	lastentry = entries;

	entries = entries->nextentry;

	dp = next_dir(dirp, dp, contents, len, blksize);
    }
    
    /*
     *  Now do the rest
     */
    while (dp && (char *)entries - (char *)entrybase
			    + sizeof (entry) + NFS_MAXNAMLEN + 1 < count) {
	/*
	 * Skip associated files
	 * & all but the first multi-extent directory entry.
	 */
	if ( ! (dp->flags & (FFLAG_ASSOC) )) {

	    fileid = MKFD(dirp, contents, dp, blksize);

	    if (useExt && hasExt) {

		ext_parse_extensions(dp, fileid, &ext);

		if (ext.isRelocated) {
		    /*
		     * Banking on the fact here that we don't have multi-extent
		     * files that are relocated.  This seems a safe assumption
		     * since relocation seems to be a property of directories
		     * which we know can only be a single extent.
		     * (ISO 9660 6.8.1)
		     */
		    dp = next_dir(dirp, dp, contents, len, blksize);
		    continue;
		}
		/*
		 * If the directory is really mapped to another location,
		 * return the fileid (viewed as the inode number on the outside
		 * world) of the new location to the NFS layer.  Otherwise, it
		 * doesn't look like the new directory is a child of the parent.
		 * getcwd() does indeed ensure that the current directory is
		 * a child of the parent, which isn't the case unless you return
		 * the new inode number.
		*/
		if (ext.child) {
			fileid = ext.child * blksize;
		}

		uname = ext.gotName ? ext.name
				    : to_unixname(dp->file_id, dp->len_fi);
	    } else {
		uname = to_unixname(dp->file_id, dp->len_fi);
	    }

	    (*nread)++;

	    add_entry(entries, 
		      fileid,
		      uname,
		      strlen (uname),
		      ((char *)dp) - ((char *)contents) + dp->len_dir);

	    ISODBG(fprintf(stderr,
		">>>>iso: iso_readdir add_entry  MULTI=%d(%d): %s\n",
					(dp->flags & FFLAG_MULTI) ? 1 : 0,
					fileid, uname));
	    lastentry = entries;

	    entries = entries->nextentry;

	    /*
	     * Is this the 1st of a series of directory entries
	     * for a multi-extent file?
	     */
	    if (dp->flags & FFLAG_MULTI) {

		/*
		 * Skip all the entries with FFLAG_MULTI, and the next
		 * one where it's not set (last entry of a series).
		 */
		do {

		    dp = next_dir(dirp, dp, contents, len, blksize);

		    /*
		     * Update the "cookie" field in the last entry
		     * to reflect that we've skipped an entry.
		     */
		    if (dp)
			*(int *)lastentry->cookie = ((char *)dp)
						  - ((char *)contents)
						  + dp->len_dir;

		} while (dp && (dp->flags & FFLAG_MULTI));

		if (dp == NULL)
			break;
	    }
	}

	dp = next_dir(dirp, dp, contents, len, blksize);
    }
    
    *eof = ! dp
	    || ! dp->len_dir
	    || ((char *)dp - (char *)contents) + dp->len_dir >= len;

    if (lastentry)
	lastentry->nextentry = 0;
    
    ISODBG(fprintf(stderr, ">>>>iso: iso_readdir: eof = %d\n", *eof));

    if (free_dirp)
	free(dirp);

    free(contents);

    return 0;
}

/*
 *  int
 *  iso_readraw(unsigned long block, void *buf, unsigned long count)
 *
 *  Description:
 *      Read raw data from the CD.  This is here for debugging support
 *      for the testcd test program.
 *
 *  Parameters:
 *      block   block on the CD to read from
 *      buf     memory to read into
 *      count   number of bytes to read
 *
 *  Returns:
 *	0 if successful, error code if error
 */

int
iso_readraw(unsigned long offset, void *buf, unsigned long count)
{

    /*
     * If the test program is trying to read the volume descriptor,
     * the block number will be 16, and we should use CDROM_BLKSIZE.
     * Otherwise, it's asking for data, and we should use the
     * block size we got from the volume descriptor.
     */ 
    return cd_read(cd, offset == 16 ? 16 * CDROM_BLKSIZE :
		   offset * cd_get_blksize(cd), buf, count, 0,
		   "iso_readraw", __LINE__);
}


/*
 *  void
 *  iso_disable_name_translations(char)
 *
 *  Description:
 *      Modify the way we muck with the filenames on the CD.
 *	'c' ==	No translation, this results in very ugly file names in
 *		all caps, sometimes ending with a semi-colon and then a
 *		version number.
 *	'l' ==	Translate to lower case.
 *	'm' ==	Suppress the version #.
 *	'x' ==	(Default) Translate to lower case & suppress the version #.
 */
void
iso_disable_name_translations(char xchar)
{
    notranslate = xchar;
}

void
iso_disable_extensions(void)
{
    useExt = 0;
}

/*
 *  void
 *  iso_setx(void)
 *
 *  Description:
 *      Make it so we automatically set execute permission on every
 *      file.  This speeds up attribute checking and enables execution
 *      of files that our algorithm may not give execute permission to.
 */
void
iso_setx(void)
{
    setx = 1;
}

/*
 *  int
 *  iso_getfs(char *path, fhandle_t *fh, struct svc_req *rq )
 *
 *  Description:
 *	Get a file handle given its mount point.  Also, if rq is
 *	non-NULL, perform check to see if path should be exported to
 *	the requestor represented by rq.
 *
 *  Parameters:
 *      path	Mount point in which we're interested
 *      fh	receives file handle
 *      rq	NFS service request
 *
 *  Returns:
 *	0 if successful, error code otherwise
 */
int
iso_getfs(char *path, fhandle_t *fh, struct svc_req *rq)
{
    FILE *fp;
    struct exportent *xent;
    int	error;
    struct sockaddr_in *sin;
    struct hostent *hp;
    char *access, **aliases, *host;

    /*
     * Make sure they're asking about the file system that we've got
     * mounted.
     */
    if (strcmp(path, mountPoint) != 0) {
	return ENOENT;
    }

    /*
     * Check to see if letting the requestor see this file system is
     * allowed (someone must run exportfs(1M).
     *
     * Note: this code is derived from code found in cmd/sun/rpc.mountd.c
     */
    fp = setexportent();
    
    if (!fp) {
	return EACCES;
    }
    
    error = EACCES;
    while (error && (xent = getexportent(fp))) {
	if (strcmp(path, xent->xent_dirname) == 0) {
	    sin = svc_getcaller(rq->rq_xprt);
	    hp = gethostbyaddr(&sin->sin_addr, sizeof sin->sin_addr,
			       AF_INET);
	    /*
	     *	Derived from cmd/sun/rpc.mountd.c
	     */
	    access = getexportopt(xent, ACCESS_OPT);
	    if (!access) {
		error = 0;
	    } else {
		while ((host = strtok(access, ":")) != NULL) {
		    access = NULL;
		    if (strcasecmp(host, hp->h_name) == 0
			|| innetgr(host, hp->h_name, NULL, _yp_domain))
			error = 0;
		    else
			for (aliases = hp->h_aliases; *aliases && error;
			     aliases++)
			    if (strcasecmp(host, *aliases) == 0
				|| innetgr(access, *aliases, NULL,
					   _yp_domain))
				error = 0;
		}
	    }	    

	    if (error) {
		access = getexportopt(xent, ROOT_OPT);
	    }
	    if (access) {
		while ((host = strtok(access, ":")) != NULL) {
		    access = NULL;
		    if (strcasecmp(host, hp->h_name) == 0)
			error = 0;
		}
	    
	    }

	    if (error) {
		access = getexportopt(xent, RW_OPT);
	    }
	    if (access) {
		while ((host = strtok(access, ":")) != NULL) {
		    access = NULL;
		    if (strcasecmp(host, hp->h_name) == 0)
			error = 0;
		}
	    }
	}
    }
    endexportent(fp);
    
    if (!error) {
	bcopy(&rootfh, fh, sizeof *fh);
    }

    return error;
}

/*
 *  int
 *  iso_getblksize(void)
 *
 *  Description:
 *      Get the CD block size for the nfs_server module (which does
 *      not have access to the cd pointer)
 *
 *  Returns:
 *      block size of our file system
 */

int
iso_getblksize(void)
{
    return cd_get_blksize(cd);
}

/*
 *  int
 *  iso_getattr(fhandlet_t *fh, fattr *fa)
 *
 *  Description:
 *      Fill in fa with the attributes of fh
 *
 *  Parameters:
 *      fh      file handle whose attributes we want
 *      fa      Buffer to receive attributes
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
iso_getattr(fhandle_t *fh,  fattr *fa)
{
    int			error, blksize;
    int			gotTimes = 0, gotMode = 0, newfd, fileid;
    int			free_dirp = 0;
    dir_t		*dirp, *newDirp;
    hsxattr_t		*hsxattr;
    /* REFERENCED */
    multi_xtnt_t	*mep;
    xattr_t		xattr;
    EXT			ext;
    
    ISODBG(fprintf(stderr, ">>>>iso: iso_getattr(%d)\n", fh->fh_fno));

    fileid = fh->fh_fno;

    /*
     * We use the privately "cached" directory entry because there's a
     * high probability that this same file handle was just the subject
     * of a lookup and we don't want to waste time re-reading the directory
     * entry if we don't need to.
     */
    error = read_dir_entry_cached(fileid, &dirp, &mep);

    if (error)
	return error;

    if (hasExt && useExt) {
	ext_parse_extensions(dirp, fileid, &ext);

	error = ext_get_relo_dir(&ext, dirp, fileid, &newDirp, &newfd);

	if (error)
	    return error;

	if (newDirp != dirp) {

	    dirp   = newDirp;
	    fileid = newfd;

	    free_dirp = 1;

	    ext_parse_extensions(dirp, fileid, &ext);
	}

	gotMode = ext.gotMode;

	if (ext.gotMode) {
	    fa->mode = ext.mode;
	    fa->nlink = ext.nlink;
	    fa->uid = ext.uid;
	    fa->gid = ext.gid;
	    fa->type = ext.type;
	}

	fa->rdev = ext.rdev;

	gotTimes = ext.gotATime + ext.gotMTime + ext.gotCTime;

	/*
	 * Take care of the situation in which only one or two of create,
	 * modify, and access were available.  Set the missing one(s)
	 * according to the following order of preference: create, modify,
	 * access.
	 */
	if (0 < gotTimes && gotTimes < 3) {
	    if (ext.gotCTime) {
		if (!ext.gotMTime) {
		    ext.mtime = ext.ctime;
		}
		if (!ext.gotATime) {
		    ext.atime = ext.ctime;
		}
	    } else if (!ext.gotMTime) {
		if (!ext.gotCTime) {
		    ext.ctime = ext.mtime;
		}
		if (!ext.gotATime) {
		    ext.atime = ext.mtime;
		}
	    } else {
		if (!ext.gotCTime) {
		    ext.ctime = ext.atime;
		}
		if (!ext.gotMTime) {
		    ext.mtime = ext.atime;
		}
	    }
	}

	if (gotTimes) {
	    fa->atime = ext.atime;
	    fa->mtime = ext.mtime;
	    fa->ctime = ext.ctime;
	}
    }

    /*
     * These attributes are the same no matter what, but they can't be
     * set until after we've read the relocated directory.
     */
    fa->blocksize = blksize = cd_get_blksize(cd);
    fa->fsid = fh->fh_dev;
    fa->fileid = fh->fh_fno;


    /*
     * If this is a Multi-Extent file, use the extent
     * list information collected by lookup.
     */
    if (mep)
	fa->size = mep->me_length;
    else
	fa->size = CHARSTOLONG(dirp->data_len_msb);


    fa->blocks = (fa->size + blksize - 1) / blksize;

    /*
     * Now fall back on the extended attributes if we didn't find what
     * we wanted in the Rock Ridge extensions.  If there are no
     * extended attributes, we'll do the best we can with the info in
     * the directory entry and some good guesses.
     *
     * If there's an extended attribute record for this file,
     * we'll use it.  Otherwise, we'll use mode PERM_RXALL and
     * use the recording date for all dates.
     */
    if (! gotMode) {

	fa->nlink = 1;

	fa->type = (FLAGS(dirp) & FFLAG_DIRECTORY) ? NFDIR : NFREG;

	if (dirp->xattr_len) {

	    ISODBG(fprintf(stderr, ">>>>iso: iso_getattr: fh has xattr's\n"));

	    error = cd_read(cd, CHARSTOLONG(dirp->ext_loc_msb) *
			    blksize, &xattr, sizeof xattr, useCache,
			    "iso_getattr", __LINE__);

	    hsxattr = (hsxattr_t *)&xattr;

	    if (error) {

		if (free_dirp)
		    free(dirp);

		return error;
	    }

	    ISODBG(fprintf(stderr, ">>>>iso: iso_getattr: perm = 0x%x\n",
								xattr.perm));
	    
	    fa->mode = ((fa->type == NFDIR) ? S_IFDIR : 0) |
		((~xattr.perm & PERM_OREAD) ? S_IRUSR : 0) |
		    ((~xattr.perm & PERM_OEXE) ? S_IXUSR : 0) |
			((~xattr.perm & PERM_GREAD) ? S_IRGRP : 0) |
			    ((~xattr.perm & PERM_GEXE) ? S_IXGRP : 0) |
				((~xattr.perm & PERM_WREAD) ? S_IROTH : 0) |
				    ((~xattr.perm & PERM_WEXE) ? S_IXOTH : 0);
	    fa->uid = xattr.owner_id;
	    fa->gid = xattr.group_id;

	} else {

	    fa->uid = 0;
	    fa->gid = 0;

	    if (fa->type == NFDIR)
		fa->mode = S_IFDIR | PERM_RXALL;
	    else if (setx)
		fa->mode = PERM_RXALL;
	    else {
		char             buf[CDROM_BLKSIZE];
		struct filehdr  *file;
		struct exec     *e;
		Elf32_Ehdr      *elf;
		
		ISODBG(fprintf(stderr,
">>>>iso: iso_getattr: non-xattr case ext_loc_msb/%d xattr_len/%d size/%d\n",
				    CHARSTOLONG(dirp->ext_loc_msb),
				    dirp->xattr_len, fa->size));
	    
		/*
		 * Figure out what permissons to give the file.
		 *
		 * Always set read permission for all.
		 *
		 * Never set write permission.
		 *
		 * Set execute permissions on COFF files (we may be
		 * exporting this to other systems), SUN executables,
		 * ELF executables, or shell scripts.
		 */
		fa->mode = PERM_RALL;

		/*
		 * Don't try to "sniff" zero-length files,
		 * ext_loc_msb may take us out into the "weeds".
		 */
		if (fa->blocks) {

		    error = cd_read(cd,
				CHARSTOLONG(dirp->ext_loc_msb) * blksize +
				dirp->xattr_len, buf, sizeof buf, useCache,
				"iso_getattr", __LINE__);
		    if (error) {

			if (free_dirp)
			    free(dirp);

			return error;
		    }
		
		    file = (struct filehdr *)buf;
		    e = (struct exec *)buf;
		    elf = (Elf32_Ehdr *)buf;

		    if ((ISCOFF(file->f_magic) && file->f_flags & F_EXEC)
			|| !N_BADMAG(*e)
			|| IS_ELF(*elf)
			|| (buf[0] == '#' && buf[1] == '!'))
			fa->mode |= PERM_XALL;
		}
	    }
	}
    }

    if (gotTimes == 0) {

	if (dirp->xattr_len) {

	    convert_time(&fa->atime, fsType == ISO ? &xattr.modify :
			 (struct date_time *)&hsxattr->modify, fsType);
	    convert_time(&fa->mtime, fsType == ISO ? &xattr.modify :
			 (struct date_time *)&hsxattr->modify, fsType);
	    convert_time(&fa->ctime, fsType == ISO ? &xattr.create :
			 (struct date_time *)&hsxattr->create, fsType);
	    
	} else {

	    fa->atime.seconds =
		convert_to_seconds(dirp->recording.year +1900,
				   dirp->recording.month, dirp->recording.day,
				   dirp->recording.hour,
				   dirp->recording.minute,
				   dirp->recording.second, 0,
				   fsType == ISO ?
				   dirp->recording.greenwich : 0);
	    
	    fa->atime.useconds = 0;
	    fa->mtime = fa->ctime = fa->atime;
	}
    }


    if (free_dirp)
	free(dirp);

    return 0;
}

/*
 *  int
 *  iso_readlink(fhandle_t *fh, char **link)
 *
 *  Description:
 *      Get the value of a symbolic link from the Rock Ridge
 *      extensions.
 *
 *  Parameters:
 *      fh    File handle to get link for
 *      link  Gets link value
 *
 *  Returns:
 *      0 if successful, errno otherwise.
 */

int
iso_readlink(fhandle_t *fh, char **link)
{
    static EXT ext;

    int			error;
    dir_t		*dirp;
    /* REFERENCED */
    multi_xtnt_t	*mep;

    if (!useExt || !hasExt)
	return ENXIO;

    /*
     * We use the privately "cached" directory entry because there's a
     * high probability that this same file handle was just the subject
     * of a lookup and we don't want to waste time re-reading the directory
     * entry if we don't need to.
     */
    error = read_dir_entry_cached(fh->fh_fno, &dirp, &mep);

    if (error)
	return error;

    ext_parse_extensions(dirp, fh->fh_fno, &ext);

    if (!ext.gotPath)
	return ENXIO;

    *link = ext.path;

    return 0;
}

void
dump_block(char *cp, int sz)
{
	int i;


	for (i = 0; i < sz; i++) {

		if (! (i % 16))
			fprintf(stderr, "\n>>>>iso: ");

		if (isprint(cp[i]))
			fprintf(stderr, "   %c", cp[i]);
		else if (iscntrl(cp[i]))
			fprintf(stderr, "  \\%c", cp[i] | ' ');
		else
			fprintf(stderr, " %3o", cp[i]);
			
	}

	fprintf(stderr, "\n");
}


/*
 *  static int
 *  set_blocks()
 *
 *  Description:
 *      Set the number of blocks in the generic file system.  This gets
 *      called at startup, and also every time a change in media is detected.
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

static int
set_blocks()
{
    int                 error;
    struct p_vol_desc   vol;
    hsvol_t             *hsvol;
    int                 blksize;
    
    error = cd_read(cd, cd_voldesc(cd), &vol, sizeof vol, 0,
						"set_blocks", __LINE__);
    if (error)
	return error;

    hsvol = (hsvol_t *)&vol;

    if (strncmp(vol.id, "CD001", 5) == 0)
	    fsType = ISO;
    else if (strncmp((const char *)hsvol->ident, "CDROM", 5) == 0)
	    fsType = HSFS;
    else {
	    ISODBG(fprintf(stderr, ">>>>iso: Disc is Unknown format\n"));
	    ISODBG(dump_block((char *)&vol, sizeof vol));

	    return EIO;
    }
    
    ISODBG(fprintf(stderr, ">>>>iso: Disc is in %s format\n",
				(fsType == ISO) ? "ISO 9660" : "High Sierra"));
    
    fsBlocks = (fsType == ISO) ? vol.vol_space_size_msb
			       : CHARSTOLONG(hsvol->vol_space_size_msb);

    rootfh.fh_fid.lfid_len = sizeof rootfh.fh_fid
			   - sizeof rootfh.fh_fid.lfid_len;

    blksize = (fsType == ISO) ? vol.blksize_msb
			      : CHARSTOSHORT(hsvol->blksize_msb);

    rootfh.fh_fno
	    = (fsType == ISO ?
	       (CHARSTOLONG(vol.root.ext_loc_msb) + vol.root.xattr_len)
	       : (CHARSTOLONG(hsvol->root.ext_loc_msb) + hsvol->root.xattr_len))
		    * blksize;

    error = cd_set_blksize(cd, blksize);

    if (error) {
	return error;
    }

    if (fsType == ISO) {
	check_for_extensions();
    }

    return 0;
}
	
/*
 *  static int
 *  get_parent_fd(int dir, int *fdp)
 *
 *  Description:
 *      Get the file descriptor of this directory's parent
 *
 *  Parameters:
 *      dir     File descriptor of directory we want parent of
 *      fdp     receives file descriptor of parent
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

static int
get_parent_fd(int dir, int *fdp)
{
    dir_t *this_dir, *this_contents, *parent;
    int error, blksize, len;
    int parent_block;
    
    blksize = cd_get_blksize(cd);

    error = read_dir_entry(dir, &this_dir);

    if (error)
	return error;

    error = read_dir(this_dir, dir, &this_contents, &len);

    if (error) {
	free(this_dir);
	return error;
    }

    parent = (dir_t *)((char *)this_contents + this_contents->len_dir);
    parent_block = CHARSTOLONG(parent->ext_loc_msb) + parent->xattr_len;
    if (hasExt && useExt) {
      EXT ext_info;
      int parent_fd;

      parent_fd = MKFD(this_dir, this_contents, parent, blksize);
      ext_parse_extensions(parent, parent_fd, &ext_info);
      if (ext_info.parent) {
        parent_block = ext_info.parent;
      }
    }
    *fdp = parent_block * blksize;

    free(this_dir);
    free(this_contents);

    return 0;
}

/*
 *  static void
 *  add_entry(entry *entries, int fd, char *name, int name_len, int cookie)
 *
 *  Description:
 *      Helper function for iso_readdir().  Puts fd and name into an entry,
 *      and sets entries->nextentry to point to the place for the next
 *      entry.
 *
 *  Parameters:
 *      entries     buffer for entries
 *      fd          file descriptor to add to entries
 *      name        name of file to add to entries
 *      name_len    lengthe of name
 *      cookie      cookie for next entry
 */

static void
add_entry(entry *entries, int fd, char *name, int name_len, int cookie)
{
    char *ptr;
    
    entries->fileid = fd;
    /*
     *  We copy the name to the memory immediately following this entry.
     *  We'll set entries->nextentry to the first byte afterwards that we
     *  can use, taking alignment into consideration.
     */
    entries->name = (char *)(entries + 1);
    strncpy(entries->name, name, name_len);
    entries->name[name_len] = '\0';
    *(int *)entries->cookie = cookie;
    entries->nextentry = (entry *)((char *)(entries + 1) + name_len + 1);
    ptr = (char *)entries->nextentry;
    /*
     *  Fix alignment problems
     */
    if ((int)ptr % 4) {
	ptr = ptr + 4 - ((int)ptr % 4);
	entries->nextentry = (entry *)ptr;
    }
}

/*
 *  static int
 *  read_dir(dir_t *dirp, int dirfd, dir_t **datap, int *len)
 *
 *  Description:
 *      Read a directory into memory
 *
 *  Parameters:
 *      dirp    pointer to directory structure of the directory we want to read
 *      dirfd   fd of dirp
 *      datap   Make this point to the directory's contents
 *      len     Make this point to the length of the directory
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

static int
read_dir(dir_t *dirp, int dirfd, dir_t **datap, int *len)
{
    dir_t *dp_ret, *newdirp;
    int loc, error, blksize, toRead, newfd;
    EXT ext;
    void *toFree = 0;

    blksize = cd_get_blksize(cd);

    if (hasExt && useExt) {
	ext_parse_extensions(dirp, dirfd, &ext);
	/*
	 * If dirp points to a relocated directory or the parent of a
	 * relocated directory, then use the child/parent pointer in
	 * the extensions rather than the ISO pointer.  What we do is
	 * then replace dirp with the appropriate self-referential
	 * directory entry (the self-referential dir entry is always
	 * the first on in the directory, and we're assuming that it's
	 * never relocated).
	 */
	if ((error = ext_get_relo_dir(&ext, dirp, dirfd,
				      &newdirp, &newfd)) != 0) {
	    return error;
	}

	if (newdirp != dirp) {
	    /*
	     * Set this up to be freed later so we don't leak.
	     */
	    toFree = dirp = newdirp;
	}
    }
    
    if (!(FLAGS(dirp) & FFLAG_DIRECTORY))
	return ENOTDIR;
    
    loc = (CHARSTOLONG(dirp->ext_loc_msb) + dirp->xattr_len) * blksize;
    
    *len = CHARSTOLONG(dirp->data_len_msb);
    
    if (toFree) {
	free(toFree);
    }
    
    dp_ret = safe_malloc(*len);
    
    error = cd_read(cd, loc, dp_ret, *len, useCache, "read_dir", __LINE__);

    if (error) {
	free(dp_ret);
	return error;
    }
    
    *datap = dp_ret;
    return 0;
}


/*
 *  static int
 *  read_dir_entry_cached(int fd, dir_t **dpp, multi_xtnt_t **mpp)
 *
 *  Description:
 *      Read the directory entry for the file descriptor fd into memory
 *	if we don't already have it there.  Keep it there until a request
 *	for a different arrives.  If it's already there, skip the read.
 *
 *  Parameters:
 *      fd      file descriptor of directory to read entry of
 *      dpp     make this point to the directory entry
 *      mep     make this point to the multi extent entry, if it exists
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */
static int
read_dir_entry_cached(int fd, dir_t **dpp, multi_xtnt_t **mpp)
{
    int	error;


    if (!useCache || fd != dirc_last_fd || !dirc_dirp) {

	if (dirc_dirp) {
	    free(dirc_dirp);

	    dirc_mep  = NULL;
	    dirc_dirp = NULL;

	    dirc_last_fd = -1;
	}

	error = read_dir_entry(fd, &dirc_dirp);

	if (error)
	    return error;

	dirc_last_fd = fd;

	/*
	 * If this is a Multi-Extent file, find the extent
	 * list information collected by lookup.
	 */
	if (dirc_dirp->flags & FFLAG_MULTI) {

	    dirc_mep = iso_me_search(fd);

	    ISODBG(fprintf(stderr,
		">>>>iso: read_dir_entry_cached: me_search for %d = 0x%x\n",
							    fd, dirc_mep));
	}

	dirc_miss++;

    } else {
	dirc_hit++;
    }

	
    *dpp = dirc_dirp;
    *mpp = dirc_mep;

    return 0;
}


/*
 *  static int
 *  read_dir_entry(int fd, dir_t **dpp)
 *
 *  Description:
 *      Read the directory entry for the file descriptor fd into memory
 *
 *  Parameters:
 *      fd      file descriptor of directory to read entry of
 *      dpp     make this point to the directory entry
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */
static int
read_dir_entry(int fd, dir_t **dpp)
{
    dir_t *dirp;
    int error, changed, blksize;

    /*
     *  Here we check to see if the user has ejected the disc and inserted
     *  another.  This is not fatal for us for the reason that the root
     *  file handle of any ISO 9660 file system is the same the way
     *  we've defined it.  The user is thus free to switch CD's at
     *  will; the only complications will be to processes that have
     *  the CD as their current directoriess.
     */
    error = cd_media_changed(cd, &changed);

    if (error)
	return error;

    if (changed) {
	cd_flush_cache(cd);

	iso_me_flush();

	error = set_blocks();

	if (error)
	    return error;
    }
    
    /*
     * Read one block's worth of data.  We don't know how big a
     * directory entry will be without reading it first, and since we
     * might be using the system use area we can't even guess, except
     * that we do know that directory entries can't span blocks.  So
     * we just read a block.
     *
     * Should just read the block that dir entry is in, but then dirp
     * won't point to the beginning of malloced memory so caller can't
     * free it.
     */
    blksize = cd_get_blksize(cd);
    dirp = safe_malloc(blksize);

    error = cd_read(cd, fd, dirp, blksize, useCache, "read_dir_entry", __LINE__);

    if (error) {
	free (dirp);
	return error;
    }

    /*
     *  Make sure that the data read in makes sense when interpreted
     *  as a directory entry
     */
    if (dirp->len_dir < sizeof (dir_t) ||
	dirp->len_fi > dirp->len_dir - sizeof (dir_t) + 1) {
	free (dirp);
	return ENOTDIR;
    }
    
    /*
     * I've seen discs that don't have the directory bit set in
     * the root directory.  So we'll set it if it isn't.
     */
    if ((fd == ROOT_DIR() || fd == HS_ROOT_DIR())
	&& !(dirp->flags & FFLAG_DIRECTORY)) {
	dirp->flags |= FFLAG_DIRECTORY;
	
#ifdef WEIRD_DMA_DISC
	/*
	 * The Defense Mapping Agency put out a number of bogus
	 * discs, with 0 in the length field of the root directory.
	 * Apparently, they ship special drivers so these discs can
	 * be read on PC's.  It seems that enough of our customers
	 * have these discs that including this stuff is warranted;
	 * as of 02/13/92 the Makefile defined WEIRD_DMA_DISC so
	 * that this code would go in.
	 */
	if (CHARSTOLONG(dirp->data_len_msb) == 0) {
	    dirp->data_len_msb[3] = (char)CDROM_BLKSIZE & 0xff;
	    dirp->data_len_msb[2] = (char)(CDROM_BLKSIZE >> 8) & 0xff;
	    dirp->data_len_msb[1] = (char)(CDROM_BLKSIZE >> 16) & 0xff;
	    dirp->data_len_msb[0] = (char)(CDROM_BLKSIZE >> 24) & 0xff;
	}
#endif
    }
    
    *dpp = dirp;
    
    return 0;
}

/*
 *  static void
 *  convert_time(struct nfstime *to, struct date_time *from)
 *
 *  Description:
 *      Convert from a date_time struct to an nfs time struct.  This involves
 *      converting from (year, month, day, hour, minute, second) to
 *      (seconds since 1/1/1970)
 *
 *  Parameters:
 *      to      the nfs version
 *      from    the ISO 9660 version
 */

static void
convert_time(struct nfstime *to, struct date_time *from, enum cdtype type)
{
#define TWODECDIGITSTOINT(dd) ((dd[1] - '0') + (dd[0] - '0') * 10)
    int     year, month, day, min, hour, sec, csec;
    
    csec = TWODECDIGITSTOINT(from->centiseconds);
    to->useconds = csec * 10;
    
    year = (from->year[3] - '0') + 10 * (from->year[2] - '0')
	+ 100 * (from->year[1] - '0') + 1000 * (from->year[0] - '0');
    month = TWODECDIGITSTOINT(from->month);
    day = TWODECDIGITSTOINT(from->day);
    hour = TWODECDIGITSTOINT(from->hour);
    min = TWODECDIGITSTOINT(from->minute);
    sec = TWODECDIGITSTOINT(from->second);
    to->seconds = convert_to_seconds(year, month, day, hour, min, sec,
				     csec, type == ISO ? from->greenwich : 0);
#undef TWODECDIGITSTOINT
}

/*
 *  static int
 *  convert_to_seconds(int year, int month, int day, int hour,
 *   int min, int sec, int csec, int gw)
 *
 *  Description:
 *      Turn (year, month, day, hour, minute, second) into the number of
 *      seconds since 1/1/1970
 *
 *  Parameters:
 *      year    
 *      month
 *      day
 *      hour
 *      min
 *      sec
 *      csec    hundredths of a second (ignored)
 *      gw      offset from greenwich time in # of 15 minute intervals
 *
 *  Returns:
 *      The number of seconds since 1970 of (year, month...)
 */

static int
convert_to_seconds(int year, int month, int day, int hour,
 int min, int sec, int csec, int gw)
{
    if (year < 1970)
	year = 1970;
    if (1 > month || 12 < month)
	month = 1;
    if (!valid_day(day, month, year))
	day = 1;
    if (hour < 0 || hour > 23)
	hour = 0;
    if (min < 0 || min > 59)
	min = 0;
    if (sec < 0 || sec > 59)
	sec = 0;
    return (((((((year - 1970) * 365 + ((year - 1969) / 4) +
		 days_so_far(day, month, year)) * 24) + hour) * 60) +
	     min - gw * 15) * 60 + sec);
}

/*
 *  static int
 *  valid_day(int day, int month, int year)
 *
 *  Description:
 *      Determine if there is, was, or will be a date month/day/year
 *
 *  Parameters:
 *      day
 *      month
 *      year
 *
 *  Returns:
 *      1 if this is a valid date, 0 if not
 */

static int
valid_day(int day, int month, int year)
{
    static int  days[12] =
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    
    /*
     *  Special case February in a leap year
     */
    if (IS_LEAPYEAR(year) == 0 && month == 2)
	if (day >= 1 && day <= 29)
	    return 1;
    
    if (day < 1 || day > days[month - 1])
	return 0;
    
    return 1;
}

/*
 *  static int
 *  days_so_far(int day, int month, int year)
 *
 *  Description:
 *      Figure out how may days have gone by in the year as of month/day
 *
 *  Parameters:
 *      day
 *      month
 *      year
 *
 *  Returns:
 *      The number of days so far
 */

static int
days_so_far(int day, int month, int year)
{
    static int  days[12] =
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    
    return day - 1 + days[month - 1] +
	    (IS_LEAPYEAR(year) && month > 2 ? 1 : 0);
}

/*
 *  static char *
 *  to_unixname(char *name, int length)
 *
 *  Description:
 *      Convert an ISO 9660 name to a UNIX name.  We convert to all lower case
 *      letters, and blow away any fluff of the form ";#".
 *
 *  Parameters:
 *      name
 *      length
 *
 *  Returns:
 *      The unix name.  Note: this is static, will be overwritten next
 *      this function is called
 */

static char *
to_unixname(char *name, int length)
{
#define min(a,b) ((a) < (b) ? (a) : (b))
#define tolower(c) (isupper(c) ? _tolower(c) : (c))

	static char uname[NFS_MAXNAMLEN];

	int         i;
	char        *dotp;

	length = min(sizeof uname - 1, length);

	if (*name == '\0')
		return dot;

	if (*name == '\1')
		return dotdot;
 
	/*
	 * Check for "no translation at all"
	 */
	if (notranslate == 'c') {
		bcopy(name, uname, length);
		uname[length] = '\0';

		return uname;
	}
 

	/*
	 * 1st check for the "fast" default mode name translation.
	 * This behaves as if both 'l' & 'm' modes were in effect.
	 */
	if (notranslate == 'x') {

		for (i = 0; i < length; i++) {

			if (name[i] == ';')
				break;

			uname[i] = tolower(name[i]);
		}

	} else {

		for (i = 0; i < length; i++) {

			/*
			 * Check to see if we're to "lop off" the version #.
			 */
			if (notranslate == 'm') {

				if (name[i] == ';')
					break;
				else
					uname[i] = name[i];
			}

			/*
			 * Check to see if we're to change to lower case.
			 */
			if (notranslate == 'l')
				uname[i] = tolower(name[i]);
		}
	}

	uname[i] = '\0';
 
	/*
	 * Custom dictates that if the translated 8.3
	 * name ends in just a ".", we can trim it off.
	 */
	dotp = strrchr(uname, '.');
 
	if (dotp && *(dotp + 1) == '\0')
		*dotp = '\0';
 
	return uname;

#undef min
#undef tolower
}

/*
 * Rock Ridge extensions
 */

#define EXT_VERSION 1

#define MKSYSUSE(byte1,byte2) ((byte1) | ((byte2) << 8))

#define EXT_CONT MKSYSUSE('C', 'E')
#define EXT_PAD MKSYSUSE('P', 'D')
#define EXT_SYSUSE MKSYSUSE('S', 'P')
#define EXT_TERM MKSYSUSE('S', 'T')
#define EXT_EXT MKSYSUSE('E', 'R')

#define EXT_MODE MKSYSUSE('P', 'X')
#define EXT_DEVNODE MKSYSUSE('P', 'N')
#define EXT_SYMLINK MKSYSUSE('S', 'L')
#define EXT_NAME MKSYSUSE('N', 'M')
#define EXT_CHILD MKSYSUSE('C', 'L')
#define EXT_PARENT MKSYSUSE('P', 'L')
#define EXT_RELO MKSYSUSE('R', 'E')
#define EXT_TIME MKSYSUSE('T', 'F')
#define EXT_FLAGS MKSYSUSE('R', 'R')

/*
 * Flags for NM and SL fields
 */
#define NM_CONT 1
#define NM_CUR 2
#define NM_PAR 4
#define NM_ROOT 8
#define NM_VOLROOT 0x10
#define NM_HOST 0x20

/*
 * Flags for TM field
 */
#define TM_CREATE 1
#define TM_MODIFY 2
#define TM_ACCESS 4
#define TM_ATTR 8
#define TM_BACKUP 0x10
#define TM_EXPIRE 0x20
#define TM_EFFECTIVE 0x40
#define TM_LONG 0x80

/*
 * Flags for the RR field
 */
#define FL_PX 1
#define FL_PN 2
#define FL_SL 4
#define FL_NM 8
#define FL_CL 0x10
#define FL_PL 0x20
#define FL_RE 0x40
#define FL_TF 0x80

/*
 * This structure is used to traverse the system used fields.  cur
 * points to the current field, and base is the start of either the
 * system use area of dirp or to contBuf, depending on whether we're
 * going through the directory record or a continuation area.
 */
typedef struct tagSysUse {
    dir_t *dirp;   /* directory */
    char *cur;     /* current offset */
    char *base;    /* base */
    int length;    /* current length */

    char *contBuf; /* continutation buffer */
    int cont;      /* continuation block */
    int contLen;   /* continuation length */
    int contOff;   /* continuation offset */
} SYSUSE;

/*
 *  static void
 *  ext_process_fields(SYSUSE *sys)
 *
 *  Description:
 *      Do generic Rock Ridge extension field processing.  This takes
 *      care of continuation, padding, and termination fields all in
 *      one place.  sys->cur is set to NULL when there are no more
 *      fields to be processed.
 *
 *  Parameters:
 *      sys  struct to maintain state while traversing extensions.
 */

static void
ext_process_fields(SYSUSE *sys)
{
    short field;
    int error;

    while (1) {
	/*
	 * We check if we're at the end of this system use area.
	 * If there was a CE, we read it in and keep going from there;
	 * otherwise we set sys->cur = NULL indicating that we're done.
	 */
	if (sys->cur - sys->base >= sys->length || sys->cur[2] == 0) {
	    if (sys->cont) {
		if (sys->contBuf) {
		    free(sys->contBuf);
		}
		sys->contBuf = safe_malloc(sys->contLen);

		ISODBG(fprintf(stderr,
				">>>>iso: Reading in a continuation!!\n"));

		error = cd_read(cd, sys->cont * cd_get_blksize(cd) +
				sys->contOff, sys->contBuf,
				sys->contLen, useCache,
				"ext_process_fields", __LINE__);
		if (error) {
		    ISODBG(fprintf(stderr, ">>>>iso: Cont read error = %d\n",
								error));
		    sys->cur = NULL;
		    return;
		}

		sys->base = sys->cur = sys->contBuf;
		sys->length = sys->contLen;
		sys->cont = 0;
	    } else {
		sys->cur = NULL;
		return;
	    }
	}

	/*
	 * Ignore fields that have the wrong version
	 */
	if (sys->cur[3] != EXT_VERSION) {
	    sys->cur += sys->cur[2];
	    continue;
	}

	field = MKSYSUSE(sys->cur[0], sys->cur[1]);
	switch (field) {
	case EXT_CONT:
	    ISODBG(fprintf(stderr, ">>>>iso: Got a continuation!!\n"));
	    /*
	     * When we encounter a continuation field, we stash away
	     * its values in our SYSUSE structure.  The CE gives us
	     * info for finding a continuation of the system use area.
	     * There's only supposed to be one CE per system use area,
	     * so it's safe to stash it for later and keep going (it
	     * would be pretty braindamaged, but there could be more
	     * fields after the CE field).
	     */
	    sys->cont = CHARSTOLONG(sys->cur + 8);
	    sys->contLen = CHARSTOLONG(sys->cur + 24);
	    sys->contOff = CHARSTOLONG(sys->cur + 16);
	    /* intentional fall-through */
	case EXT_PAD:
	    /*
	     * After processing CE, or if we find PD, skip over it.
	     */
	    ISODBG(fprintf(stderr, ">>>>iso: Skipping %c%c\n",
					   sys->cur[0], sys->cur[1]));
	    sys->cur += sys->cur[2];
	    break;
	case EXT_TERM:
	    ISODBG(fprintf(stderr, ">>>>iso: Got an ST\n"));
	    /*
	     * ST tells us that this is the system use area is over.
	     */
	    sys->cur = NULL;
	    return;
	default:
	    /*
	     * Anything else, pass it back to the caller for deciphering.
	     */
	    return;
	}
    }
}

/*
 *  static void
 *  ext_free_sys(SYSUSE *sys)
 *
 *  Description:
 *      Function to be called when we're all done traversing Rock
 *      Ridge extension fields.  This frees any memory we malloced for
 *      reading in continuation blocks.
 *
 *  Parameters:
 *      sys  struct for maintaining state while traversing extensions.
 */

static void
ext_free_sys(SYSUSE *sys)
{
    if (sys->contBuf) {
	free(sys->contBuf);
	sys->contBuf = NULL;
    }
}

/*
 *  static char *
 *  ext_first_field(dir_t *dirp, int dirfd, SYSUSE *sys)
 *
 *  Description:
 *      Get the first field in the Rock Ridge extensions of a
 *      directory entry.  The reason that we need dirfd here is to
 *      differentiate the '.' entry of the root directory from all
 *      others.  If this is the '.' entry of the root directory, then
 *      we don't skip skipSysBytes, as per the spec.  For all other
 *      directory entries, we do skip skipSysBytes.
 *
 *  Parameters:
 *      dirp   directory entry to get extensions from
 *      dirfd  Descriptor for this directory
 *      sys    A structure that helps us traverse extensions
 *
 *  Returns:
 *      a pointer to the first Rock Ridge extension field, NULL if
 *      there are none.
 */

static char *
ext_first_field(dir_t *dirp, int dirfd, SYSUSE *sys)
{
    char *sysUse;
    
    bzero(sys, sizeof *sys);
    sys->dirp = dirp;
    sys->length = dirp->len_dir - sizeof(dir_t) - dirp->len_fi + 1;

    if ((dirp->len_fi & 1) == 0) {
	sys->length--;
    }

    if (sys->length < 2) {
	return NULL;
    }
    
    /*
     * skipSysBytes gets set in check_for_extensions, below.
     * check_for_extensinons calls us before it ever sets this, which
     * is OK because the spec says that the '.' entry of the
     * root directory (which is what check_for_extensions looks at)
     * does not use the skip bytes.
     */
    sys->cur = sys->base = (char *)dirp + dirp->len_dir - sys->length
	+ (dirfd != rootSelfEntry ? skipSysBytes : 0);

    ext_process_fields(sys);
    return sys->cur;
}

/*
 *  static char *
 *  ext_next_field(SYSUSE *sys)
 *
 *  Description:
 *      Get the next field from the Rock Ridge extensions
 *
 *  Parameters:
 *      sys  parameter that keeps state around about rock ridge
 *      extensions
 *
 *  Returns:
 *      Pointer to the next field in the extensions, NULL if there are
 *      none left.
 */

static char *
ext_next_field(SYSUSE *sys)
{
    sys->cur += sys->cur[2];
    ext_process_fields(sys);
    return sys->cur;
}

/*
 * Globals to hold our hostname.  These variables are shared between
 * the functions ext_name and iso_readlink, so that we don't have to
 * call gethostname more than once.
 */
static char hostName[100];
static int gotHost = 0;

/*
 *  char *
 *  ext_get_time(char *tm, struct nfstime *to, int longForm)
 *
 *  Description:
 *      Convert Rock Ridge time to NFS time.  Rock Ridge can be in
 *      either xattr form (longForm == 1) or directory record form
 *      (longForm == 0).
 *
 *  Parameters:
 *      tm        pointer into a TF field
 *      to        NFS time
 *      longForm  whether to use xattr time (longForm == 1) or dir_t
 *                time (longForm == 0)
 *
 *  Returns:
 *      tm advanced past this time record.
 */

char *
ext_get_time(char *tm, struct nfstime *to, int longForm)
{
    struct rec_date_time *shortTime;

    if (longForm) {
	convert_time(to, (struct date_time *)tm, ISO);
	tm += sizeof(struct date_time);
    } else {
	shortTime = (struct rec_date_time *)tm;
	to->useconds = 0;
	to->seconds = 
	    convert_to_seconds(shortTime->year + 1900,
			       shortTime->month,
			       shortTime->day,
			       shortTime->hour,
			       shortTime->minute,
			       shortTime->second, 0,
			       shortTime->greenwich);
	tm += sizeof(struct rec_date_time);
    }
    return tm;
}

/*
 *  static void
 *  check_for_extensions(void)
 *
 *  Description:
 *      Check to see if this is a Rock Ridge CD.  If it is, we need to
 *      set the global variable skipSysBytes to the number of bytes to
 *      skip in each system use area.
 */

static void
check_for_extensions(void)
{
    int hasExtensions = 0, isRockRidge = 0, error, len, dirlen;
    static char *rripid = "RRIP_1991A";
    char *sysUse;
    SYSUSE sys;
    dir_t *dirp, *contents;
    int entry_count;

    if (0 != read_dir_entry(ROOT_DIR(), &dirp)) {
	return;
    }

    rootSelfEntry = MKFD(dirp, 0, 0, cd_get_blksize(cd));

    error = read_dir(dirp, rootSelfEntry, &contents, &dirlen);

    free(dirp);

    if (error) {
	return;
    }

    /*
     * Look at the system use area of the first entry of the root
     * directory.  Make sure that the first entry is the EXT_SYSUSE
     * entry iff there are to be extensions.
     */
    for (sysUse = ext_first_field(contents, rootSelfEntry, &sys), entry_count = 1;
	 sysUse; sysUse = ext_next_field(&sys), entry_count++) {
	if (entry_count == 1 && MKSYSUSE(sysUse[0], sysUse[1]) == EXT_SYSUSE &&
	    sysUse[2] == 7 && sysUse[4] == 0xbe && sysUse[5] == 0xef) {
	    hasExtensions++;
	    skipSysBytes = sysUse[6];
	} else if (hasExtensions && MKSYSUSE(sysUse[0], sysUse[1]) == EXT_EXT) {
	    len = strlen(rripid);
	    if (sysUse[4] == len &&
		strncmp(sysUse + 8, rripid, len) == 0) {
		isRockRidge++;
	    } else {
		free(contents);
		return;
	    }
	}
    }

    hasExt = isRockRidge && hasExtensions;
    free(contents);
    ext_free_sys(&sys);
}
		
/*
 *  static void ext_parse_symlink(char *sysUse, EXT *ext)
 *
 *  Description:
 *      Parse a rock ridge field pertaining to a symbolic link.  Note
 *      that the link name can span several fields; we concatenate
 *      this round of names onto what was already there, so when
 *      starting fresh ext->path[0] has to be '\0'.
 *
 *  Parameters:
 *      sysUse  Pointer to the system use field with symlink info
 *      ext     extension struct to get info
 */

static void 
ext_parse_symlink(char *sysUse, EXT *ext)
{
    dir_t *dirp;
    int rv, error, len, compLen;
    unsigned char flags, fieldFlags;
    static char path[NFS_MAXPATHLEN];
    char *comp, *component;
    SYSUSE sys;

    for (comp = sysUse + 5; comp - sysUse < sysUse[2];
	 comp += comp[1] + 2) {
	flags = comp[0];
	if (flags & NM_CUR) {
	    component = dot;
	    compLen = strlen(dot);
	} else if (flags & NM_PAR) {
	    component = dotdot;
	    compLen = strlen(dotdot);
	} else if (flags & NM_ROOT) {
	    component = "/";
	    compLen = strlen(component);
	} else if (flags & NM_VOLROOT) {
	    component = mountPoint;
	    compLen = strlen(component);
	} else if (flags & NM_HOST) {
	    if (!gotHost) {
		if (gethostname(hostName, sizeof hostName - 1) < 0) {
		    continue;
		}
		gotHost = 1;
	    }
	    component = hostName;
	    compLen = strlen(component);
	} else {
	    component = comp + 2;
	    compLen = comp[1];
	}
	
	len = strlen(ext->path) + compLen + (ext->doSlash ? 1 : 0);
	
	if (len < sizeof ext->path) {
	    if (ext->doSlash) {
		strcat(ext->path, "/");
	    }
	    strncat(ext->path, component, compLen);
	    ext->path[len] = '\0';
	    rv = 0;
	}
	
	/*
	 * Put a slash after this component if there's another
	 * one that's not a continuation of this one and if
	 * this one is not already a slash.
	 */
	ext->doSlash = !(flags & NM_CONT) && !(flags & NM_ROOT);
    }
}

/*
 *  static void ext_parse_name(char *sysUse, EXT *ext)
 *
 *  Description:
 *      Parse a NM rock ridge extension field.  Note that like
 *      symlinks, the name can span multiple fields, so we concatenate
 *      what we find in the field with what was already there.  To get
 *      started, ext->name[0] must be set to '\0'.
 *
 *  Parameters:
 *      sysUse  pointer to name system use field
 *      ext     extension struct that gets name info
 */

static void 
ext_parse_name(char *sysUse, EXT *ext)
{
    char *component;
    unsigned char flags;
    int compLen;
    int len;

    flags = (unsigned char)sysUse[4];
    if (flags & NM_CUR) {
	component = dot;
	compLen = strlen(dot);
    } else if (flags & NM_PAR) {
	component = dotdot;
	compLen = strlen(dotdot);
    } else if (flags & NM_ROOT) {
	component = "/";
	compLen = strlen(component);
    } else if (flags & NM_VOLROOT) {
	component = mountPoint;
	compLen = strlen(component);
    } else if (flags & NM_HOST) {
	if (!gotHost) {
	    if (gethostname(hostName, sizeof hostName - 1) < 0) {
		return;
	    }
	    gotHost = 1;
	}
	component = hostName;
	compLen = strlen(component);
    } else {
	component = sysUse + 5;
	compLen = sysUse[2] - 5;
    }
    len = strlen(ext->name) + compLen;
    if (len < sizeof ext->name - 1) {
	strncat(ext->name, component, compLen);
	ext->name[len] = '\0';
    }
}

/*
 *  static void ext_parse_mode(char *sysUse, EXT *ext)
 *
 *  Description:
 *      Parse the rock ridge PX field
 *
 *  Parameters:
 *      sysUse  pointer to PX field
 *      ext     extensions struct to get mode info
 */

static void 
ext_parse_mode(char *sysUse, EXT *ext)
{
    ext->mode = CHARSTOLONG(sysUse + 8);
    ext->nlink = CHARSTOLONG(sysUse + 16);
    ext->uid = CHARSTOLONG(sysUse + 24);
    ext->gid = CHARSTOLONG(sysUse + 32);
    if (S_ISDIR(ext->mode)) {
	ext->type = NFDIR;
    } else if (S_ISCHR(ext->mode)) {
	ext->type = NFCHR;
    } else if (S_ISBLK(ext->mode)) {		    
	ext->type = NFBLK;
    } else if (S_ISREG(ext->mode)) {
	ext->type = NFREG;
    } else if (S_ISFIFO(ext->mode)) {
	ext->type = NFFIFO;
    } else if (S_ISLNK(ext->mode)) {
	ext->type = NFLNK;
    } else if (S_ISSOCK(ext->mode)) {
	ext->type = NFSOCK;
    } else {
	ext->type = NFBAD;
    }
}

/*
 *  static void ext_parse_devnode(char *sysUse, EXT *ext)
 *
 *  Description:
 *      Get device node from rock ridge extension field
 *
 *  Parameters:
 *      sysUse  pointer to PN field
 *      ext     gets results
 */

static void 
ext_parse_devnode(char *sysUse, EXT *ext)
{
    ext->rdev = makedev(CHARSTOLONG(sysUse + 8),
		       CHARSTOLONG(sysUse + 16));
}

/*
 *  static void ext_parse_time(char *sysUse, EXT *ext)
 *
 *  Description:
 *      Parse the rock ridge TF field.  Note that multiple TF fields
 *      can be present in one system use area, so we have to be really
 *      careful about keeping track of our state in here.  All the
 *      time related flags should be zeroed before processing a new
 *      system use area.  See comment below.
 *
 *  Parameters:
 *      sysUse  pointer to TF field
 *      ext     gets results
 */

static void 
ext_parse_time(char *sysUse, EXT *ext)
{
    char *tm;
    unsigned char flags;
    int longForm;
    
    /*
     * Parse the time field, setting flags as we go to
     * indicate what we've found.  The spec allows for any
     * combinations of times to be specified in any number
     * of fields, so we must be careful to cover some very
     * weird cases, like modify and access are specified
     * in two different fields and create is not.
     */
    flags = sysUse[4];
    longForm = !!(flags & TM_LONG);
    tm = sysUse + 5;
    
    if ((flags & TM_CREATE) && !ext->gotCTime) {
	tm = ext_get_time(tm, &ext->ctime, longForm);
	ext->gotCTime = 1;
    }
    
    if (flags & TM_MODIFY && !ext->gotMTime) {
	tm = ext_get_time(tm, &ext->mtime, longForm);
	ext->gotMTime = 1;
    }
    
    if (flags & TM_ACCESS && !ext->gotATime) {
	ext_get_time(tm, &ext->atime, longForm);
	ext->gotATime = 1;
    }			
}

/*
 *  static void ext_parse_extensions(dir_t *dirp, int fd, EXT *ext)
 *
 *  Description:
 *      Parse all of the rock ridge extensions in a system use area
 *      into an EXT structure.  We bzero the thing first, because we
 *      have to clear all of the flags to clear the state.  Not all
 *      extensions will always be present, so we've got to keep track
 *      of what we see.
 *
 *  Parameters:
 *      dirp  directory entry we're parsing sys use field of
 *      fd    file descriptor of fd (to check whether it's the
 *            rootSelfEntry in ext_first_field
 *      ext   Gets results of parsing.
 */

static void 
ext_parse_extensions(dir_t *dirp, int fd, EXT *ext)
{
    char *sysUse;
    SYSUSE sys;

    bzero(ext, sizeof *ext);

    for (sysUse = ext_first_field(dirp, fd, &sys);
	 sysUse; sysUse = ext_next_field(&sys)) {
	switch (MKSYSUSE(sysUse[0], sysUse[1])) {
	case EXT_SYMLINK:
	    ext_parse_symlink(sysUse, ext);
	    ext->gotPath = 1;
	    break;
	case EXT_NAME:
	    ext_parse_name(sysUse, ext);
	    ext->gotName = 1;
	    break;
	case EXT_MODE:
	    ext_parse_mode(sysUse, ext);
	    ext->gotMode = 1;
	    break;
	case EXT_DEVNODE:
	    ext_parse_devnode(sysUse, ext);
	    break;
	case EXT_TIME:
	    ext_parse_time(sysUse, ext);
	    break;
	case EXT_CHILD:
	    ext->child = CHARSTOLONG(sysUse + 8);
	    break;
	case EXT_PARENT:
	    ext->parent = CHARSTOLONG(sysUse + 8);
	    break;
	case EXT_RELO:
	    ext->isRelocated = 1;
	    break;
	}
    }
    ext_free_sys(&sys);
}

/*
 *  static int ext_get_relo_dir(EXT *ext, dir_t *inDir, int infd,
 *  			    dir_t **outDir, int *outfd)
 *
 *  Description:
 *      If inDir refers to a relocated directory, this fills in
 *      *outDir with the self-referential directory entry for the
 *      relocated directory, which can fill the same role in directory
 *      entry processing.  This means that we look at the rock ridge
 *      extensions, and if there's a CL or a PL field we use that.
 *
 *      In the normal case, where ext->parent == ext->child == 0, we
 *      just set *outDir to indir and *outfd to infd so the caller can
 *      detect that nothing weird happened.
 *
 *  Parameters:
 *      ext     rock ridge extensions pointer
 *      inDir   The directory under consideration
 *      infd    fileid for inDir
 *      outDir  Gets replacement directory entry
 *      outfd   fileid for outfd
 *
 *  Returns:
 *      0 if successful, error code if failure
 */

static int
ext_get_relo_dir(EXT *ext, dir_t *inDir, int infd,
			    dir_t **outDir, int *outfd)
{
    int blksize = cd_get_blksize(cd);

    if (!ext->child && !ext->parent) {
	*outDir = inDir;
	*outfd = infd;
	return 0;
    }

    *outfd = (ext->child ? ext->child : ext->parent) * blksize;

    ISODBG(fprintf(stderr, ">>>>iso: ext_get_relo_dir: %d -> %d\n",
					   infd, *outfd));

    return read_dir_entry(*outfd, outDir);
}
