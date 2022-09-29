/*
** iso.c
**
** This file is based off mountcd/iso.c (revision 1.39) minus iso_getattr(),
** read_dir_cached() and read_dir_entry_cached().
**
** It also makes call to a modifed version of the cdrom.c functions.
**
*/

#include "iso.h"
#include "cdrom.h"
#include "common.h"
#include "cye_mdebug.h"





char dot[] = ".";
char dotdot[] = "..";


/*
 * "notranslate" controls the "to_unixname" name translation.
 *
 *	'c' = the user will see raw, ugly file names
 *	'l' = the user will see lower-case, ugly file names
 *	'm' = the user will see upper-case names less the version #
 *	'x' = the user will see lower-case names less the version #
 */
char notranslate = 'x';		/* The internal default,	*/
					/* acts like 'l'+'m'		*/

/*
 * if setx == 1, we'll blindly set execute permissions for every file.
 */
int  setx = 0;
int  def_num_blocks = 256;

/*
 * By default, use rock ridge extensions if they're there.
 */
int  useExt = 1;

/*
 * Whether or not CD has extensions
 */
int  hasExt = 0;

/*
 * Number of bytes in each system use area to skip
 */
int skipSysBytes = 0;

/*
 * The location on the CD of the '.' entry of the root directory.
 * This is use in ext_first_field when reading Rock Ridge extensions
 * to determine whether or not to skip skipSysBytes.
 */
int rootSelfEntry = 0;

int isodebug = 0;

/*
 * Number of blocks in the mounted file system
 */
int fsBlocks;

/*
 * Type of file system - iso9660 or high sierra
 */
cdtype_t fsType;

/*
 * The root file handle of the mounted file system
 */
fhandle_t rootfh;

/*
 * The path name of the mount point of the file system
 */
char *mountPoint;


int		 dirc_hit     =  0;
int		 dirc_miss    =  0;
int		 dirc_last_fd = -1;
dir_t		 *dirc_dirp   = NULL;
multi_xtnt_t	 *dirc_mep    = NULL;


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

multi_xtnt_t *multi_extent_head[MULTI_XTNT_HASH_SIZE];
multi_xtnt_t *multi_extent_tail[MULTI_XTNT_HASH_SIZE];



int set_blocks(FSBLOCK *fsb);
int get_parent_fd(FSBLOCK *fsb, int fd, int *fdp);
void add_entry(entry *entries, int fd, char *name, int name_len, int cookie);
int read_dir(FSBLOCK *fsb, dir_t *dirp, int dirfd, dir_t **datap, int *len);
int read_dir_entry(FSBLOCK *fsb, int fd, dir_t **dpp);
void convert_time(struct nfstime *to, struct date_time *from, enum cdtype type);
int convert_to_seconds(int year, int month, int day, int hour, int min,
int sec, int gw);
int valid_day(int day, int month, int year);
int days_so_far(int day, int month, int year);
char * to_unixname(char *name, int length);

int iso_lookup(FSBLOCK *fsb, fhandle_t *fh, char *name, fhandle_t *fh_ret);
int iso_readdir(FSBLOCK *fsb, fhandle_t *fh, entry *entries, int count,
int cookie, bool_t *eof, int *nread);
void iso_init(int num_blocks);


/*
 * Rock Ridge support functions
 */
void check_for_extensions(FSBLOCK *fsb);
void ext_parse_extensions(FSBLOCK *fsb, dir_t *dirp, int fd, EXT *ext);
int ext_get_relo_dir(FSBLOCK *fsb, EXT *ext, dir_t *inDir, int infd,
                     dir_t **outDir, int *outfd);





void
iso_me_add_xtnt(multi_xtnt_t	*mep,
		int		fileid,
		int		start_blk,
		int		length)
{
    one_xtnt_t *oep;

    oep = cye_malloc(sizeof(one_xtnt_t));

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


multi_xtnt_t *
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
    mep = cye_malloc(sizeof(multi_xtnt_t));

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


multi_xtnt_t *
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

dir_t *
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

dir_t *
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
	int i;

	def_num_blocks = num_blocks;

	for (i = 0; i < MULTI_XTNT_HASH_SIZE; i++) {
		multi_extent_head[i] = NULL;
		multi_extent_tail[i] = NULL;
	}
}




/*
** look for name in fh (a directory); if found, fill in fh_ret
** and fsb_to_inode(fsb)
**
**  Parameters:
**      fh      file handle of directory in which to look
**      name    name of file we're looking for
**      fh_ret  receive handle for name (caller must provide space)
**
**  Returns:
**      0 if successful, error code otherwise
**
** THIS IS PROBABLY A BIT BROKEN (on large directories)
*/

int
iso_lookup(FSBLOCK *fsb, fhandle_t *fh, char *name, fhandle_t *fh_ret)
{
	int blksize = cd_get_blksize();
	int multi_seen = 0;
	int error, fd, tmpfd, len, dirfd, newfd;
	char *uname;
	dir_t *dirp;      /* directory entry for dir */
	dir_t *contents;  /* contents of dir */
	dir_t *dp;        /* pointer into contents */
	dir_t *newdirp;
	EXT ext;

#ifdef DEBUG
	printf("iso_lookup() on %s\n", name);
#endif

	error = read_dir_entry(fsb, fh->fh_fno, &dirp);

	if (error) {
#ifdef DEBUG
		printf("iso_lookup() ERROR : read_dir_entry()");
#endif
		cye_free(dirp);
		return error;
	}

	dirfd = fh->fh_fno;
	if (hasExt && useExt) {
#ifdef DEBUG
		printf("iso_lookup() WE ARE IN EXTENSION CODE!\n");
#endif
		ext_parse_extensions(fsb, dirp, dirfd, &ext);

		error =
		ext_get_relo_dir(fsb, &ext, dirp, dirfd, &newdirp, &newfd);

		if (error) {
			cye_free(dirp);
			return error;
		}	    
		if (dirp != newdirp) {
			cye_free(dirp);
			dirp = newdirp;
			dirfd = newfd;
		}
	}
    
	error = read_dir(fsb, dirp, dirfd, &contents, &len);
    
	if (error) {
#ifdef DEBUG
		printf("iso_lookup() ERROR : read_dir()\n");
#endif
		cye_free(dirp);
		return error;
	}
    
	fd = 0;
    
	for (dp = first_dir(dirp, contents, len, 0, blksize);
	dp;
	dp = next_dir(dirp, dp, contents, len, blksize)) {

		tmpfd = MKFD(dirp, contents, dp, blksize);

		/* skip associated files */
		if (dp->flags & FFLAG_ASSOC)
			continue;

		if (useExt && hasExt) {

			ext_parse_extensions(fsb, dp, tmpfd, &ext);

			/* skip relocated directories */
			if (ext.isRelocated) {
				continue;
			}

			uname = ext.gotName ? ext.name
			: to_unixname(dp->file_id, dp->len_fi);
		} else {
			uname = to_unixname(dp->file_id, dp->len_fi);
		}

#ifdef DEBUG
		printf("iso_lookup() : [%s] block=%d len=%d\n",
		uname, CHARSTOLONG(dp->ext_loc_msb),
		CHARSTOLONG(dp->data_len_msb));
#endif
		if (strcmp(name, uname) == 0) {

			bcopy(dp, fsb_to_inode(fsb), sizeof(dir_t));
			/*
			** Only "remember" the fd of the 1st of
			** a series for Multi-Extent files.
			*/
			if (! fd)
				fd = tmpfd;
	
			if (dp->flags & FFLAG_MULTI) {
				multi_xtnt_t *mep;
	
				/*
				** check to see if we already know the extents
				** for this file.
				*/

				/* 1st extent? -- try to find them */
				if (! multi_seen) {
					mep = iso_me_search(fd);
	
					/* if found, that's all we need */
					if (mep) {
						break;
					}
				}
	
				/* into multi-extents mode */
				multi_seen++;
				mep = iso_me_add_file(fd);
	
				iso_me_add_xtnt(mep, tmpfd,
				CHARSTOLONG(dp->ext_loc_msb),
				CHARSTOLONG(dp->data_len_msb));
	
				/* read the next directory entry */
				continue;
	
			} else {
				multi_xtnt_t *mep;
	
				/*
				** Are we scanning a set of multi-extent
				** entries?
				*/
				if (! multi_seen)
					break;
	
				mep = iso_me_add_file(fd);
	
				iso_me_add_xtnt(mep, tmpfd,
				CHARSTOLONG(dp->ext_loc_msb),
				CHARSTOLONG(dp->data_len_msb));
	
				/* we've read enough */
				break;
			}
		}
	}
    
	cye_free(dirp);
	cye_free(contents);
    
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
iso_readdir(FSBLOCK *fsb, fhandle_t *fh, entry *entries, int count, int cookie,
		bool_t *eof, int *nread)
{
    int			blksize;
    int			error, fileid, len, dirfd, newfd;
    int                 free_dirp = 0;
    char		*uname;
    dir_t		*dirp, *contents, *dp, *newdirp;
    entry		*lastentry = 0, *entrybase = entries;
    /* REFERENCED */
    multi_xtnt_t	*mep;
    EXT			ext;
    
#ifdef DEBUG
	printf("iso_readdir() fh_fno=%d count=%d offset=%d\n", fh->fh_fno,
	count, cookie);
#endif

#ifdef MDEBUG
	printf("START iso_readdir()  ");
	cye_outstanding_nbytes();
#endif
	*nread = 0;

	cd_set_blksize(fsb, CDROM_BLKSIZE);
	blksize = cd_get_blksize();
    
	dirfd = fh->fh_fno;

	error = read_dir_entry(fsb, dirfd, &dirp);
	if (error) {
		return error;
	}

	if (hasExt && useExt) {
		ext_parse_extensions(fsb, dirp, dirfd, &ext);
		error = ext_get_relo_dir(fsb, &ext, dirp, dirfd, &newdirp,
		&newfd);
		if (error) {
			cye_free(dirp);
			return error;
		}

		if (dirp != newdirp) {
			dirp  = newdirp;
			dirfd = newfd;
			free_dirp = 1;
		}
	}

	/* actually, since we don't call read_dir_entry_cached() */
	free_dirp = 1;
    
	error = read_dir(fsb, dirp, dirfd, &contents, &len);
	if (error) {
		if(free_dirp)
			cye_free(dirp);
		return error;
	}

	/*
	**  We won't even try if there isn't at least a self and parent
	**  reference.
	*/
	if (len <  2 * sizeof *dp) {
		if(free_dirp)
			cye_free(dirp);
		cye_free(contents);
		return EIO;
	}
    
	/*
	**  We do this to clue the nfs_server module into the fact that there
	**  are no entries left to be read.  It turns out that the NFS client
	**  won't listen to us when we set *eof == 0; the nfs_server module
	**  has to actually return a NULL entry pointer in order to convince
	**  the client that there are no more entries.
	*/
	if (cookie >= len - dirp->xattr_len) {
		if(free_dirp)
			cye_free(dirp);
		cye_free(contents);
		return 0;
	}

	/* take care of self and parent entries */
	dp = first_dir(dirp, contents, len, cookie, blksize);

	if (dp == NULL) {
		if(free_dirp)
			cye_free(dirp);
	    		cye_free(contents);	/* added to avoid leak */
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
			if(free_dirp)
				cye_free(dirp);
	    		cye_free(contents);	/* added to avoid leak */
			return EIO;
		}
	}
    
	if (cookie < 2 * sizeof (dir_t)) {
		(*nread)++;
		error = get_parent_fd(fsb, fh->fh_fno, &fileid);
		if (error) {
			if(free_dirp)
				cye_free(dirp);
	    		cye_free(contents);
	    		return error;
		}
		add_entry(entries, fileid, dotdot, strlen (dotdot),
		((char *)dp) - ((char *)contents) + dp->len_dir);
		lastentry = entries;
		entries = entries->nextentry;
		dp = next_dir(dirp, dp, contents, len, blksize);
    	}
    
	/* now do the rest */
	while (dp && (char *)entries - (char *)entrybase
	+ sizeof (entry) + NFS_MAXNAMLEN + 1 < count) {

	/*
	 * Skip associated files
	 * & all but the first multi-extent directory entry.
	 */
	if ( ! (dp->flags & (FFLAG_ASSOC) )) {

	    fileid = MKFD(dirp, contents, dp, blksize);

	    if (useExt && hasExt) {

		ext_parse_extensions(fsb, dp, fileid, &ext);

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

	    ISODBG(printf(
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
    

	if(free_dirp)
		cye_free(dirp);
	cye_free(contents);
#ifdef MDEBUG
	printf("END iso_readdir()  ");
	cye_outstanding_nbytes();
#endif
	return 0;
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

int
set_blocks(FSBLOCK *fsb)
{
	struct p_vol_desc vol;
	hsvol_t *hsvol;
	IOBLOCK *iob;
	int blksize;

#ifdef DEBUG
	printf("set_blocks()\n");
#endif
	bcopy(fsb_to_mount(fsb), &vol, sizeof(vol));

	hsvol = (hsvol_t *)&vol;

	/* need to add back Standard Identifier check */
	fsType = ISO;
    
	fsBlocks = (fsType == ISO) ? vol.vol_space_size_msb
	: CHARSTOLONG(hsvol->vol_space_size_msb);

	rootfh.fh_fid.lfid_len = sizeof rootfh.fh_fid
	- sizeof rootfh.fh_fid.lfid_len;

	blksize = (fsType == ISO) ? vol.blksize_msb
	: CHARSTOSHORT(hsvol->blksize_msb);

	rootfh.fh_fno = (fsType == ISO ?
	(CHARSTOLONG(vol.root.ext_loc_msb) + vol.root.xattr_len)
	: (CHARSTOLONG(hsvol->root.ext_loc_msb) + hsvol->root.xattr_len))
	* blksize;

	cd_set_blksize(fsb, blksize);

	if (fsType == ISO) {
		check_for_extensions(fsb);
	}

#ifdef DEBUG
	printf("set_blocks() OKAY\n");
#endif
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

int
get_parent_fd(FSBLOCK *fsb, int dir, int *fdp)
{
    dir_t *this_dir, *this_contents, *parent;
    int error, blksize, len;
    int parent_block;
    
	cd_set_blksize(fsb, CDROM_BLKSIZE);
    blksize = cd_get_blksize();

    error = read_dir_entry(fsb, dir, &this_dir);

    if (error)
	return error;

    error = read_dir(fsb, this_dir, dir, &this_contents, &len);

    if (error) {
	cye_free(this_dir);
	return error;
    }

    parent = (dir_t *)((char *)this_contents + this_contents->len_dir);
    parent_block = CHARSTOLONG(parent->ext_loc_msb) + parent->xattr_len;
    if (hasExt && useExt) {
      EXT ext_info;
      int parent_fd;

      parent_fd = MKFD(this_dir, this_contents, parent, blksize);
      ext_parse_extensions(fsb, parent, parent_fd, &ext_info);
      if (ext_info.parent) {
        parent_block = ext_info.parent;
      }
    }
    *fdp = parent_block * blksize;

    cye_free(this_dir);
    cye_free(this_contents);

    return 0;
}

/*
 *  Description:
 *      Helper function for iso_readdir().  Puts fd and name into an entry,
 *      and sets entries->nextentry to point to the place for the next
 *      entry.
 *
 *      an entry struct is a u_int, char*, char[4], entry*
 *
 *  Parameters:
 *      entries     buffer for entries
 *      fd          file descriptor to add to entries
 *      name        name of file to add to entries
 *      name_len    length of name
 *      cookie      cookie for next entry
 */

void
add_entry(entry *entries, int fd, char *name, int name_len, int cookie)
{

	char *ptr;

	entries->fileid = fd;
	/*
	** We copy the name to the memory immediately following this entry.
	** We'll set entries->nextentry to the first byte afterwards that we
	** can use, taking alignment into consideration.
	*/
	entries->name = (char *)(entries + 1);
	strncpy(entries->name, name, name_len);
	entries->name[name_len] = '\0';
	*(int *)entries->cookie = cookie;
	entries->nextentry = (entry *)((char *)(entries + 1) + name_len + 1);
	ptr = (char *)entries->nextentry;

	/* fix alignment problems */
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

int
read_dir(FSBLOCK *fsb, dir_t *dirp, int dirfd, dir_t **datap, int *len)
{
	dir_t *dp_ret, *newdirp;
	int loc, error, blksize, toRead, newfd;
	EXT ext;
	void *toFree = 0;

#ifdef DEBUG
	printf("read_dir()\n");
#endif
	cd_set_blksize(fsb, CDROM_BLKSIZE);
	blksize = cd_get_blksize();

	if (hasExt && useExt) {
		ext_parse_extensions(fsb, dirp, dirfd, &ext);
		/*
		** If dirp points to a relocated directory or the parent of a
		** relocated directory, then use the child/parent pointer in
		** the extensions rather than the ISO pointer.  What we do is
		** then replace dirp with the appropriate self-referential
		** directory entry (the self-referential dir entry is always
		** the first on in the directory, and we're assuming that it's
		** never relocated).
		*/
		if ((error = ext_get_relo_dir(fsb, &ext, dirp, dirfd,
		&newdirp, &newfd)) != 0) {
			return error;
		}

		if (newdirp != dirp) {
			/* set this up to be freed later so we don't leak */
			toFree = dirp = newdirp;
		}
	}
    
	if (!(FLAGS(dirp) & FFLAG_DIRECTORY))
		return ENOTDIR;
    
	loc = (CHARSTOLONG(dirp->ext_loc_msb) + dirp->xattr_len) * blksize;
	*len = CHARSTOLONG(dirp->data_len_msb);
    
	if (toFree) {
		cye_free(toFree);
	}
    
	if(NULL == (dp_ret = (dir_t *) cye_malloc(*len))) {
#ifdef DEBUG
		printf("read_dir() ERROR : malloc(%d)\n", *len);
#endif
		return(ENOMEM);
	}

	error = cd_read(fsb, loc, dp_ret, *len);
	if (error) {
#ifdef DEBUG
		printf("read_dir() ERROR : cd_read()\n");
#endif
		cd_set_blksize(fsb, CDROM_BLKSIZE);
		cye_free(dp_ret);
		return(error);
	}
    
	*datap = dp_ret;
	return(0);
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
int
read_dir_entry(FSBLOCK *fsb, int fd, dir_t **dpp)
{
	dir_t *dirp;
	int error;

#ifdef DEBUG
	printf("read_dir_entry()\n");
#endif

	/* we used to check if the CD media has changed -- no need */

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

	cd_set_blksize(fsb, CDROM_BLKSIZE);

	if(NULL == (dirp = cye_malloc(sizeof(dir_t)))) {
		printf("read_dir_entry() ERROR : malloc()\n");
		return(ENOMEM);
	}
#ifdef DEBUG
	printf("read_dir_entry() fsb->IO->Address=0x%x dirp=0x%x\n",
	fsb->IO->Address, dirp);
#endif

	error = cd_read(fsb, fd, dirp, sizeof(dir_t));
	if (error) {
		printf("read_dir_entry() ERROR : cd_read()\n");
		cye_free(dirp);
		cd_set_blksize(fsb, 512);
		return error;
	}

		cd_set_blksize(fsb, 512);
	/*
	**  Make sure that the data read in makes sense when interpreted
	**  as a directory entry
	*/
	if (dirp->len_dir < sizeof (dir_t) ||
		dirp->len_fi > dirp->len_dir - sizeof (dir_t) + 1) {
		cye_free(dirp);
#ifdef DEBUG
		printf("read_dir_entry() ERROR len_dir=%d len_fi=%d\n",
		dirp->len_dir, dirp->len_fi);
#endif
		cd_set_blksize(fsb, 512);
		return ENOTDIR;
	}
    
	/*
	** I've seen discs that don't have the directory bit set in
	** the root directory.  So we'll set it if it isn't.
	*/
	if ((fd == ROOT_DIR() || fd == HS_ROOT_DIR())
	&& !(dirp->flags & FFLAG_DIRECTORY)) {
		dirp->flags |= FFLAG_DIRECTORY;
	}
    
	*dpp = dirp;
    
	cd_set_blksize(fsb, 512);
#ifdef DEBUG
	printf("read_dir_entry() OKAY\n");
#endif
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

void
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
				     type == ISO ? from->greenwich : 0);
#undef TWODECDIGITSTOINT
}

/*
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

int
convert_to_seconds(int year, int month, int day, int hour,
 int min, int sec, int gw)
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

int
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

int
days_so_far(int day, int month, int year)
{
    static int  days[12] =
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    
    return day - 1 + days[month - 1] +
	    (IS_LEAPYEAR(year) && month > 2 ? 1 : 0);
}

/*
 * Return the ptr in sp at which the character c last
 * appears; NULL if not found
*/

char *
mystrrchr(const char *sp, int c)
{
        const char *r = NULL;
        char ch = (char)c;

        do {
                if(*sp == ch)
                        r = sp;
        } while(*sp++);
        return((char *)r);
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

char *
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
	dotp = mystrrchr(uname, '.');
 
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

void
ext_process_fields(FSBLOCK *fsb, SYSUSE *sys)
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
		    cye_free(sys->contBuf);
		}
		sys->contBuf = cye_malloc(sys->contLen);

		ISODBG(printf(
				">>>>iso: Reading in a continuation!!\n"));

		error = cd_read(fsb, sys->cont * cd_get_blksize() +
				sys->contOff, sys->contBuf, sys->contLen);
		if (error) {
		    ISODBG(printf(">>>>iso: Cont read error = %d\n",
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
	    ISODBG(printf(">>>>iso: Got a continuation!!\n"));
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
	    ISODBG(printf(">>>>iso: Skipping %c%c\n",
					   sys->cur[0], sys->cur[1]));
	    sys->cur += sys->cur[2];
	    break;
	case EXT_TERM:
	    ISODBG(printf(">>>>iso: Got an ST\n"));
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

void
ext_free_sys(SYSUSE *sys)
{
	if (sys->contBuf) {
		cye_free(sys->contBuf);
		sys->contBuf = NULL;
	}
}

/*
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

char *
ext_first_field(FSBLOCK *fsb, dir_t *dirp, int dirfd, SYSUSE *sys)
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

    ext_process_fields(fsb, sys);
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

char *
ext_next_field(FSBLOCK *fsb, SYSUSE *sys)
{
    sys->cur += sys->cur[2];
    ext_process_fields(fsb, sys);
    return sys->cur;
}

/*
 * Globals to hold our hostname.  These variables are shared between
 * the functions ext_name and iso_readlink, so that we don't have to
 * call gethostname more than once.
 */
static char hostName[] = "localhost";
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
			       shortTime->second,
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

void
check_for_extensions(FSBLOCK *fsb)
{
	int hasExtensions = 0, isRockRidge = 0, error, len, dirlen;
	static char *rripid = "RRIP_1991A";
	char *sysUse;
	SYSUSE sys;
	dir_t *dirp, *contents;
	int entry_count;

#ifdef DEBUG
	printf("check_for_extensions()\n");
#endif
#ifdef MDEBUG
	printf("start of check_for_extensions()  ");
	cye_outstanding_nbytes();
#endif
	if (0 != read_dir_entry(fsb, ROOT_DIR(), &dirp)) {
		printf("check_for_extensions() ERROR : read_dir_entry()\n");
		return;
	}

	rootSelfEntry = MKFD(dirp, 0, 0, cd_get_blksize());

	error = read_dir(fsb, dirp, rootSelfEntry, &contents, &dirlen);
	cye_free(dirp);

	if (error) {
		printf("check_for_extensions() ERROR : read_dir()\n");
		return;
	}

	/*
	** Look at the system use area of the first entry of the root
	** directory.  Make sure that the first entry is the EXT_SYSUSE
	** entry iff there are to be extensions.
	*/
	for(sysUse = ext_first_field(fsb, contents, rootSelfEntry, &sys),
	entry_count = 1;
	sysUse; sysUse = ext_next_field(fsb, &sys), entry_count++) {
		if (entry_count == 1 && MKSYSUSE(sysUse[0], sysUse[1]) ==
		EXT_SYSUSE &&
		sysUse[2] == 7 && sysUse[4] == 0xbe && sysUse[5] == 0xef) {
			hasExtensions++;
			skipSysBytes = sysUse[6];
		} else if (hasExtensions && MKSYSUSE(sysUse[0], sysUse[1]) ==
		EXT_EXT) {
			len = strlen(rripid);
			if (sysUse[4] == len &&
			strncmp(sysUse + 8, rripid, len) == 0) {
				isRockRidge++;
			} else {
				cye_free(contents);
				return;
			}
		}
	}

	hasExt = isRockRidge && hasExtensions;
	cye_free(contents);
	ext_free_sys(&sys);
#ifdef DEBUG
	printf("check_for_extensions() OKAY\n");
#endif
#ifdef MDEBUG
	printf("end of check_for_extensions()  ");
	cye_outstanding_nbytes();
#endif
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
    int error, len, compLen;
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

void 
ext_parse_extensions(FSBLOCK *fsb, dir_t *dirp, int fd, EXT *ext)
{
    char *sysUse;
    SYSUSE sys;

#ifdef DEBUG
printf("ext_parse_extensions()\n");
#endif

    bzero(ext, sizeof *ext);

    for (sysUse = ext_first_field(fsb, dirp, fd, &sys);
	 sysUse; sysUse = ext_next_field(fsb, &sys)) {
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

int
ext_get_relo_dir(FSBLOCK *fsb, EXT *ext, dir_t *inDir, int infd,
			    dir_t **outDir, int *outfd)
{
    int blksize = cd_get_blksize();

    if (!ext->child && !ext->parent) {
	*outDir = inDir;
	*outfd = infd;
	return 0;
    }

    *outfd = (ext->child ? ext->child : ext->parent) * blksize;

    ISODBG(printf(">>>>iso: ext_get_relo_dir: %d -> %d\n",
					   infd, *outfd));

    return read_dir_entry(fsb, *outfd, outDir);
}
