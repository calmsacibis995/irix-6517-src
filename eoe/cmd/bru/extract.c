/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	extract.c    contains routines specific to extracting files
 *
 *  SCCS
 *
 *	@(#)extract.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines specific to extracting files from bru archives.
 *
 */


#include "autoconfig.h"

#include <stdio.h>

#if (unix || xenix)
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  include "sys.h"
#endif

#include "typedefs.h"
#include "dbug.h"
#include "manifest.h"
#include "config.h"
#include "errors.h"
#include "blocks.h"
#include "macros.h"
#include "trees.h"
#include "finfo.h"
#include "flags.h"
#include "bruinfo.h"

#define DIR_MAGIC 0777		/* Default permissions for directories */
				/* This is modified by umask */
dev_t saved_st_dev = 0;


/*
 *	External bru functions.
 */

extern BOOLEAN decompfip ();	/* Decompress a file, saving to destination */
extern BOOLEAN openzfile ();	/* Open a temp file for compressed file */
extern int setinfo ();
extern int s_mknod ();		/* Make a directory, special, or other file */
extern BOOLEAN confirmed ();	/* Confirm action */
extern union blk *ar_next ();	/* Get pointer to next archive block */
extern VOID bru_error ();	/* Report an error to user */
extern VOID ar_close ();	/* Flush buffers and close the archive */
extern VOID ar_open ();		/* Open the archive */
extern VOID ar_read ();		/* Read archive block */
extern VOID reload ();		/* Reload first volume for rescan */
extern VOID done ();		/* Finish up and exit */
extern BOOLEAN newdir ();	/* Make a directory */
extern BOOLEAN mklink ();	/* Make a link to a file */
extern BOOLEAN mksymlink ();	/* Make a symbolic link to a file */
extern VOID scan ();		/* Scan archive */
extern VOID verbosity ();	/* Issue verbosity message */
extern BOOLEAN selected ();	/* Apply selection criteria */
extern VOID finfo_init ();	/* Initialize a finfo structure */
extern BOOLEAN file_access ();	/* Check file for access */
extern BOOLEAN dir_access ();	/* Check parent for access */
extern BOOLEAN out_of_date ();	/* Existing file out of date */
extern BOOLEAN unconditional();	/* Unconditionally overwrite */
extern char *s_strrchr ();	/* Find last given character in string */
extern int s_close ();		/* Invoke library file close function */
extern int s_umask ();		/* Invoke umask library function */
extern int s_creat ();		/* Create a file */
extern VOID file_chown ();	/* Change file owner and group */
extern int s_utime ();		/* Set file access/modification times */
extern int s_chmod ();		/* Change mode of a file */
extern int s_write ();		/* Write data to file */
extern int s_unlink ();		/* Remove a directory entry */
extern long s_time ();		/* Get current time */
extern VOID mark_archived ();	/* Set the "has been archived" bit */
extern VOID clear_archived ();	/* Clear the "has been archived" bit */

extern char *getlinkname();
extern char *add_link();

/*
 *	External bru variables.
 */

extern struct bru_info info;	/* Current invocation information */
extern struct cmd_flags flags;	/* Command line flags */
extern char mode;		/* Current mode (citdxh) */

/*
 *	Local functions.
 */

static VOID do_extract ();	/* Actually do the extraction */
static VOID xfile ();		/* Extract the file */
static VOID makelink ();	/* Make link to existing file */
static BOOLEAN makedir ();	/* Make a directory */
static BOOLEAN makeparent ();	/* Make a parent directory */
static VOID makestat ();	/* Make default stat buffer */
static VOID makespecial ();	/* Make a special file */
static VOID xregfile ();	/* Do extraction of regular file */
static VOID makefile ();	/* Make first copy of file */
static VOID attributes ();	/* Set attributes to match */

/*
 *	Stuff for signal handling.
 */

extern VOID sig_catch ();	/* Set signal catch state */
extern VOID sig_push ();	/* Push signal state */
extern VOID sig_pop ();		/* Pop signal state */
extern BOOLEAN interrupt;	/* Interrupt received */


/*
 *  NAME
 *
 *	extract    main entry point for extraction of file from archive
 *
 *  SYNOPSIS
 *
 *	VOID extract ()
 *
 *  DESCRIPTION
 *
 *	This is the main entry point for extracting files from a bru
 *	archive.  It is called once all the command line options
 *	have been processed and common initialization has been
 *	performed.
 *
 */

VOID extract ()
{
    DBUG_ENTER ("extract");
    mode = 'x';
    reload ("extraction");
    ar_open ();
    scan (xfile);
    ar_close ();
    DBUG_VOID_RETURN;
}


/*
 *  NAME
 *
 *	xfile    control extraction of a single file
 *
 *  SYNOPSIS
 *
 *	static VOID xfile (blkp, fip)
 *	register union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Controls extraction of a single file from the archive.
 *	Blkp points to the file header block and fip points
 *	to a file information structure.
 *
 *	There was initially some question about how to report
 *	files which are not extracted because there is an existing
 *	file which is more recent.  Since files that ARE extracted
 *	are only reported if verbosity is enabled, by analogy, files
 *	that are NOT extracted are reported only if verbosity is
 *	enabled.  The basic philosophy is to silently bring the existing
 *	hierarchy up to date by replacing missing files and
 *	superceding out of date files, without overwriting more
 *	recent files.  If every file in a given class (regular,
 *	directory, block special, etc) is to be extracted regardless
 *	of modification date, bru must be explicitly told this via
 *	the -u (unconditional) flag.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin xfile
 *	    Determine whether or not file is a directory
 *	    If this file is to be extracted then
 *		If unconditional extraction or file out of date then
 *		    If extraction confirmed then
 *			Issue verbosity message
 *			Do the extraction
 *			End if
 *		    End if
 *		Else
 *		    If file is not a directory and verbosity enabled
 *			Warn user that it was not extracted
 *		    End if
 *		End if
 *	    End if
 *	End xfile
 *
 */

static VOID xfile (blkp, fip)
register union blk *blkp;
register struct finfo *fip;
{
    register BOOLEAN is_directory;
    struct stat64 alpha;
    char tmp[256];
    char *savelinkname;

    DBUG_ENTER ("xfile");
    is_directory = IS_DIR (fip -> statp -> st_mode);
    if ((IS_STEM (fip) && is_directory) || IS_LEAF (fip) || IS_EXTENSION (fip)) {
    /* change absolute path to relative path if -j flag specified */
    	if( (flags.jflag) && (*fip->fname == '/') ) {
	    if(strcmp(fip->fname, "/") == 0)
    		DBUG_VOID_RETURN;
	    strcpy(tmp, fip->fname);
	    strcpy(fip->fname, &tmp[1]);
		
    	}
	if(flags.mflag) {
	    if(saved_st_dev != fip->statp->st_dev) {
		if(access(fip->fname, 0)) {
		    DBUG_VOID_RETURN;
		}
		else {
		    stat64(fip->fname, &alpha);
		    if(alpha.st_dev == fip->statp->st_dev)
		    	saved_st_dev = fip->statp->st_dev;
		}
	    }
	}

	/* symbolic links that are hard linked to other symbolic links we
	 * have already seen are treated as just links.  SGI seems to
	 * be one of the few vendors that even allows this; the only way
	 * make this work (and remain compatible with older versions of
	 * bru both directions) on list and extract is to build the link
	 * table, same as on create and estimate... */
	savelinkname = fip->lname;
	if(!IS_FLNK(fip->statp->st_mode) || (fip->statp->st_nlink>1 &&
	    getlinkname(fip))) {
	    fip->lname = NULL;
	}
	if(!is_directory && fip->statp -> st_nlink > 1) {
	    fip->lname = add_link(fip);
	    if(!fip->lname && IS_FLNK(fip->statp->st_mode))
		fip->lname = savelinkname;
	}

	if (unconditional (fip) || out_of_date (fip)) {
	    if (confirmed ("x %s", fip)) {
		verbosity (fip);
		do_extract (blkp, fip);
	    }
	} else {
	    if (!is_directory && flags.vflag > 1) {
		bru_error (ERR_SUPERSEDE, fip -> fname);
	    }
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	makelink    link file being extracted to an existing file
 *
 *  SYNOPSIS
 *
 *	static VOID makelink (blkp, fip)
 *	union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Links the file being extracted (file1) to the existing file
 *	(file2) that it is supposed to be linked to.
 *
 *	Note that this will fail if file2 does not exist.
 *	In this case there is nothing else to be done since the file
 *	contents for file2 are not stored in the archived file1.
 *
 *	Also note that any existing file1 is unlinked.  If the
 *	new link cannot be made for some reason, the effective
 *	result is the deletion of any existing file1.  This
 *	could be fixed by first linking an existing file1 to some
 *	temporary and then either unlinking or relinking it as
 *	necessary once the link between the new file1 and file2
 *	is established or denied respectively.  Another way is
 *	to open the existing file1, unlink the existing file1,
 *	then either close file1 or copy it back to a new 
 *	instance of file1 as necessary.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin makelink
 *	    If file to be linked to does not exist then
 *		Tell user we can't make the link
 *	    Else
 *		Unlink any existing file to be linked
 *		If the link to the target file is successful then
 *		    Set the attributes of the link just made
 *		End if
 *	    End if
 *	End makelink
 *
 */

static VOID makelink (blkp, fip)
union blk *blkp;
register struct finfo *fip;
{
    char tmp[256];

    DBUG_ENTER ("makelink");
    if(flags.jflag) {
	if(!fip->lname) {
		bru_error (ERR_MKLINK_J, fip->fname);
		return;
	}
	if(strcmp(fip->lname, "") && *fip->lname == '/') {
		strcpy(tmp, fip->lname);
	    	strcpy(fip->lname, &tmp[1]);
	}
    	if (!file_access (fip->lname, A_EXISTS, FALSE)) {
		bru_error (ERR_MKLINK, fip->fname, fip->lname);
    	} else {
		(VOID) s_unlink (fip->fname);
		if (mklink (fip->lname, fip->fname)) {
	    	attributes (fip);
		}
    	}
    }
    else {
    	if (!file_access (blkp -> FH.f_lname, A_EXISTS, FALSE)) {
		bru_error (ERR_MKLINK, blkp -> HD.h_name, blkp -> FH.f_lname);
    	} else {
		(VOID) s_unlink (blkp -> HD.h_name);
		if (mklink (blkp -> FH.f_lname, blkp -> HD.h_name)) {
	    	attributes (fip);
		}
    	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	makefile    extract a regular file
 *
 *  SYNOPSIS
 *
 *	static VOID makefile (blkp, fip)
 *	register union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Controls extract of a regular file by testing for
 *	for it's existance and/or writability.  The file
 *	will only be extracted if it does not currently exist
 *	or if the user has write permission to overwrite an
 *	existing version.
 *
 */


/*
 *  PSEUOD CODE
 *
 *	Begin makefile
 *	    If file does not exist then
 *		Go ahead and extract it from archive
 *	    Else
 *		If file is not writable then
 *		    Tell user he can't overwrite the file
 *		Else
 *		    Go ahead and extract it from archive
 *		End if
 *	    End if
 *	End makefile
 *
 */

static VOID makefile (blkp, fip)
register union blk *blkp;
register struct finfo *fip;
{
    DBUG_ENTER ("makefile");
    if (!file_access (fip -> fname, A_EXISTS, FALSE)) {
	xregfile (blkp, fip);
    } else {
	if (!file_access (fip -> fname, A_WRITE, FALSE)) {
	    bru_error (ERR_OVRWRT, fip -> fname);
	} else {
	    xregfile (blkp, fip);
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	xregfile    extract a normal file from archive
 *
 *  SYNOPSIS
 *
 *	static VOID xregfile (blkp, fip)
 *	register union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	This routine is responsible for extracting a normal file from
 *	an archive.
 *
 *	Returns TRUE if file was created, FALSE otherwise.
 *	Note that return of TRUE does NOT indicate successful
 *	extraction, only that a file was created.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin xregfile
 *	    Initialize the file truncated flag to FALSE
 *	    Create the file being extracted
 *	    If couldn't be created then 
 *		Inform user
 *	    Else
 *		Get number of bytes to extract
 *		If any bytes to extract then
 *		    For each block to be extracted
 *			Seek to archive block
 *			Read the archive block
 *			If extracting a partial block then
 *			    Only use that many bytes
 *			End if
 *			Write the bytes to the new file
 *			If write did not write requested number then
 *			    Remember that the file was truncated
 *			End if
 *		    End for
 *		    If last write returned error then
 *			Notify user about write error
 *		    End if
 *		    If file was truncated then
 *			Warn user about truncation
 *		    End if
 *		End if
 *		If file close get an error then
 *		    Warn user about the close error
 *		End if
 *		Set the attributes of the extracted file
 *	    End if
 *	End xregfile
 *
 */

static VOID xregfile (blkp, fip)
register union blk *blkp;
register struct finfo *fip;
{
    register off_t bytes;
    register int iobytes;
    register UINT wbytes;
    register int fildes;
    register BOOLEAN truncated = FALSE;
    register BOOLEAN doextract = TRUE;
    register char *fname;

    DBUG_ENTER ("xregfile");
    if (IS_COMPRESSED (fip)) {
	if (openzfile (fip)) {
	    fname = fip -> zfname;
	    fildes = fip -> zfildes;
	} else {
	    bru_error (ERR_ZXFAIL, fip -> fname);
	    doextract = FALSE;
	}
    } else {
	fname = fip -> fname;
	fildes = s_creat (fip -> fname, 0600);
	if (fildes == SYS_ERROR) {
	    bru_error (ERR_CREAT, fip -> fname);
	    doextract = FALSE;
	}
    }
    if (doextract) {
	bytes = ZSIZE (fip);
	if (bytes > 0) {
	    for (wbytes = DATASIZE; bytes > 0; bytes -= wbytes) {
		blkp = ar_next ();
		ar_read (fip);
		if (bytes < DATASIZE) {
		    wbytes = bytes;
		}
		iobytes = s_write (fildes, blkp -> FD, wbytes);
		if (iobytes != wbytes) {
		    truncated = TRUE;
		}
	    }
	    if (iobytes == SYS_ERROR) {
		bru_error (ERR_WRITE, fname);
	    }
	    if (truncated) {
		bru_error (ERR_FTRUNC, fname);
	    }
	}
	if (s_close (fildes) == SYS_ERROR) {
	    bru_error (ERR_CLOSE, fname);
	}
	if (!IS_COMPRESSED (fip) || decompfip (fip)) {
	    attributes (fip);
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	makespecial    make a special file node
 *
 *  SYNOPSIS
 *
 *	static VOID makespecial (fip)
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Is reponsible for controlling creation of special files.
 *	If the special file being made already exists, it is
 *	unlinked first, then the new node is made.
 *
 *	Adding code for 4.2 has complicated this routine, and made it
 *	very ugly.  It is even worse than it ought to be, since it is
 *	currently impossible under 4.2 to chmod a symbolic link (although
 *	a symlink with mode 000 can still be read!), or to reset the access
 *	and modification times on a symbolic link. This is dealt with 
 *	in attributes(), below.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin makespecial
 *	    If the current file exists then
 *		Unlink the file
 *	    End if
 *	    If under 4.2, not a pyramid, and file is a fifo, then
 *		Just create a regular file
 *		Set the attributes of the new node
 *	    Else if the file is a symbolic link then
 *		If cannot make the symbolic link then
 *		    Inform the user
 *		Else
 *		    Set the attributes of the new node
 *		Endif
 *	    Else if the new node cannot be made then
 *		Inform the user
 *	    Else
 *		Set attributes of the new node
 *	    End if
 *	End makespecial
 *
 */

static VOID makespecial (fip)
register struct finfo *fip;
{
	char *tmpname;
    DBUG_ENTER ("makespecial");
    if (file_access (fip -> fname, A_EXISTS, FALSE)) {
	(VOID) s_unlink (fip -> fname);	
    }
#if !HAVE_FIFOS
    if (IS_FIFO (fip -> statp -> st_mode)) {
	int fd;

	fd = s_creat (fip -> fname, 0666);	/* fudge it */
	if (fd == SYS_ERROR) {
	    bru_error (ERR_MKFIFO, fip -> fname);
	} else {
	    attributes (fip);
	    bru_error (ERR_FIFOTOREG, fip -> fname);
	    if (s_close (fd) == SYS_ERROR) {
		bru_error (ERR_CLOSE, fip -> fname);
	    }
	}
    } else
#endif
    if (IS_FLNK (fip -> statp -> st_mode)) {
	if (! mksymlink (fip -> lname, fip -> fname) && ! flags.lflag) {
	    bru_error (ERR_MKSYMLINK, fip -> fname);
	} else {
	    attributes (fip);
	}
    } else if (s_mknod (fip -> fname, (int) fip -> statp -> st_mode, (int) fip -> statp -> st_rdev) == SYS_ERROR) {
	bru_error (ERR_MKNOD, fip -> fname);
    } else {
	attributes (fip);
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	attributes    set attributes of newly created file
 *
 *  SYNOPSIS
 *
 *	static VOID attributes (fip)
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Using stat buffer information recovered from archive
 *	header block, sets the attributes of the file.
 *
 *	Under 4.2 BSD, it is impossible to reset the access and
 *	modification times of a symbolic link, or to change its
 *	mode with chmod.  Both chmod and utimes go through the
 *	symbolic link to the real file, which is not what we want.
 *	Therefore, we add code for checking symbolic links.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin attributes
 *	    If the mode cannot be set then
 *		Inform user that mode cannot be changed
 *	    End if
 *	    Change owner and group of the file
 *	    If the access and modification times cannot be set then
 *		Inform the user about time error
 *	    End if
 *	End attributes
 *
 */

static VOID attributes (fip)
register struct finfo *fip;
{
    register char *path;
    register int s_mode;
    register int s_owner;
    register int s_group;
    struct filetimes {		/* 4.2 changed the stat buffer; st_atime */
	time_t atime;		/* is not contiguous to st_mtime, so we */
	time_t mtime;		/* build the structure explicitly before */
    } t;			/* handing it off to s_utime() */

    DBUG_ENTER ("attributes");
    path = fip -> fname;
    s_mode = fip -> statp -> st_mode;
    s_owner = fip -> statp -> st_uid;
    s_group = fip -> statp -> st_gid;
    if (s_owner != info.bru_uid && (info.bru_uid != 0 || flags.Cflag)) {
	if (s_mode & S_ISUID) {
	    s_mode &= ~S_ISUID;
	    bru_error (ERR_SUID, path);
	}
    }
    if (s_group != info.bru_gid && (info.bru_uid != 0 || flags.Cflag)) {
	if (s_mode & S_ISGID) {
	    s_mode &= ~S_ISGID;
	    bru_error (ERR_SGID, path);
	}
    }

    DBUG_PRINT ("attributes",
	("file %s, owner %d, group %d",	fip -> fname, s_owner, s_group));
    DBUG_PRINT ("attributes", ("file %s, mode %#o", fip -> fname, s_mode));

    if(!IS_FLNK (fip -> statp -> st_mode)) {
	t.atime = fip -> statp -> st_atime;
	t.mtime = fip -> statp -> st_mtime;
	/* do in this order so when on sysV systems, the time
	 * and mode can be set correctly even if we are chown'ing
	 * the file to some other user. */
	if(s_utime (fip -> fname, &t) == SYS_ERROR)
	    bru_error (ERR_STIMES, fip -> fname);
	if(s_chmod (path, s_mode) == SYS_ERROR)
	    bru_error (ERR_SMODE, path);
	file_chown (path, s_owner, s_group);
    }
    if (fip -> fi_flags & FI_AMIGA) {
	(VOID) setinfo (fip);
	if (flags.Asflag) {
	    mark_archived (fip);
	} else if (flags.Acflag) {
	    clear_archived (fip);
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	do_extract    do the extraction
 *
 *  SYNOPSIS
 *
 *
 *	static VOID do_extract (blkp, fip)
 *	register union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Once all tests have been met and it has been decided to extract
 *	the file, this routine is called to do the actual extraction.
 *	At this point, we are committed to doing the extraction and
 *	cannot be interrupted until done.
 *
 *	Any missing parent directories will be created and then the
 *	file itself will be extracted.
 *
 *	For directories, there is an explicit call to set the attributes
 *	since the makedir function cannot do this if the directory already
 *	exists.  Thus directories which do not exist will have there
 *	attributes set when they are made by makedir, and again here.
 *	It is the case of existing directories that we only catch here.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin do_extract
 *	    Save current signal processing stat
 *	    Set up to catch signals
 *	    Check for directory in which to extract file
 *	    If no parent directory then
 *		Make a default stat buffer
 *		Make the parent directory
 *	    End if
 *	    If parent directory and parent accessible then
 *		If file to extract is a directory then
 *		    Make the directory
 *		    Set the directory attributes
 *		Else if file linked to another then
 *		    Make the linkage
 *		Else if not a regular file then
 *		    Make a special file
 *		Else
 *		    Make the file
 *		End if
 *	    End if
 *	    Pop the saved signal processing state
 *	    If interrupt came in while doing extraction
 *		Cleanup and exit
 *	    End if
 *	End do_extract
 *
 */


static VOID do_extract (blkp, fip)
register union blk *blkp;
register struct finfo *fip;
{
    auto struct stat64 sbuf;
    register BOOLEAN got_parent;
    char *tmpname;
    SIGTYPE prevINT;
    SIGTYPE prevQUIT;
    
    DBUG_ENTER ("do_extract");
    sig_push (&prevINT, &prevQUIT);
    sig_catch ();
    got_parent = dir_access (fip -> fname, A_EXISTS, FALSE);
    if (!got_parent) {
	makestat (&sbuf);
	got_parent = makeparent (&sbuf, fip -> fname);
    }
    if (got_parent) {
	if (IS_DIR (fip -> statp -> st_mode)) {
	    (VOID) makedir (fip);
	    attributes (fip);
	} else if (LINKED (blkp) && (!IS_FLNK (fip -> statp -> st_mode)) ||
	    fip->statp->st_nlink>1 && (tmpname=getlinkname(fip)) &&
	    strcmp(fip->fname, tmpname)) {
	    /* make hard link if not a symlink, or a symlink with st_nlink>1
	     * and this is the 2nd thru nth instance seen of the hardlink
	     * to the symlink during the backup. */
	    makelink (blkp, fip);
	} else if (!IS_REG (fip -> statp -> st_mode)) {
	    /*
	     * blkp and fip are passed down from scan, which will already
	     * set up the link name in fip, so makespecial can just pass
	     * it on if a symbolic link has to be made. In other words,
	     * we don't need to do any special checking for that here.
	     */
	    makespecial (fip);
	} else {
	    makefile (blkp, fip);
	}
    }
    sig_pop (&prevINT, &prevQUIT);
    if (interrupt) {
	done ();
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	makeparent    make a parent directory that does not exist
 *
 *  SYNOPSIS
 *
 *	static BOOLEAN makeparent (statp, name)
 *	struct stat64 *statp;
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a directory pathname in "name" and
 *	pointer to stat buffer in "statp", attempts to make
 *	the parent directory if one does not already exist.
 *
 *	Note that the installation is recursive.  That is,
 *	if the parent of the parent does not exist, it is
 *	made also, with the same attributes.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin makeparent
 *	    If parent already exists then
 *		Result is TRUE
 *	    Else
 *		Find where parent and child names join.
 *		If no parent name
 *		    Tell user about bug
 *		    Result is FALSE
 *		Else
 *		    Split parent and child names
 *		    Attempt to make grandparent recursively
 *		    If attempt successful then
 *			Build a file info struct for parent
 *			Make the parent
 *		    End if
 *		    Rejoin parent and child names
 *		End if
 *	    End if
 *	    Return result
 *	End makeparent
 *
 */

static BOOLEAN makeparent (statp, name)
struct stat64 *statp;
char *name;
{
    register char *slash;
    register BOOLEAN got_dir;
    auto struct finfo pfile;

    DBUG_ENTER ("makeparent");
    if (dir_access (name, A_EXISTS, FALSE)) {
	got_dir = TRUE;
    } else {
	slash = s_strrchr (name, '/');
	if (slash == NULL) {
	    bru_error (ERR_BUG, "makeparent");
	    got_dir = FALSE;
	} else {
	    if(slash != name) {
		*slash = EOS;
		if(got_dir = makeparent (statp, name)) {
		    finfo_init (&pfile, name, statp);
		    got_dir = makedir (&pfile);
		}
		*slash = '/';
	    }
	    else /* makedir called directly later if it's a dir; if
		it isn't, we definitely do not want to mkdir! */
		got_dir = 1; /* but makeparent still has to return true */
	}
    }
    DBUG_RETURN (got_dir);
}


/*
 *  FUNCTION
 *
 *	makedir    make a directory
 *
 *  SYNOPSIS
 *
 *	static BOOLEAN makedir (fip)
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Attempts to make a directory if one does not already exist.
 *
 *	Note that it is assumed that the parent has already
 *	been made by a call to makeparent.  If not, then
 *	makedir will fail if no parent exists.  Also, we need to
 *	verify that the user running bru has appropriate permission
 *	to write into the parent of the directory to be made, rather
 *	than depending upon the newdir/mkdir calls to enforce
 *	permissions.
 *
 *	Also note that the attributes are set ONLY if the directory
 *	is made.  This is because makedir may be called to ensure
 *	that parent directories exist in order to extract an archived
 *	directory.  If the attributes were unconditionally set,
 *	parents would take on the attributes of the child directory
 *	being extracted from the archive.  The actual directory
 *	being extracted has it attributes explicitly set elsewhere.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin makedir
 *	    If the directory already exists then
 *		Result is TRUE
 *	    Else if we can write in parent to make directory then
 *		Attempt to make the directory
 *		If directory node successfully made then
 *		    Set attributes of directory
 *		End if
 *	    Else
 *		Doesn't exist and can't make it, result is FALSE
 *	    End if
 *	    Return result
 *	End makedir
 *
 */

static BOOLEAN makedir (fip)
register struct finfo *fip;
{
    register BOOLEAN result;

    DBUG_ENTER ("makedir");
    if (file_access (fip -> fname, A_EXISTS, FALSE)) {
	result = TRUE;
    } else {
	/* idiotic program; don't check permissions, just
	 * try to make the directory. */
	result = newdir (fip);
	if (result) {
	    attributes (fip);
	}
    }
    DBUG_RETURN (result);
}


/*
 *  FUNCTION
 *
 *	makestat    make a default stat buffer
 *
 *  SYNOPSIS
 *
 *	static VOID makestat (statp)
 *	struct stat64 *statp;
 *
 *  DESCRIPTION
 *
 *	Makes a default stat buffer as appropriate for current user.
 *	This is typically used to set attributes of directories which
 *	must be made to install a file being extracted.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin makestat
 *	    Get current time
 *	    Get the mode word mask
 *	    Restore the mode word mask
 *	    Get the file mode and make it a directory
 *	    Initialize the stat buffer mode
 *	    Initialize the user id
 *	    Initialize the group id
 *	    Initialize the access time
 *	    Initialize the modification time
 *	End makestat
 *
 */

static VOID makestat (statp)
struct stat64 *statp;
{
    register int mask;
    register time_t now;

    DBUG_ENTER ("makestat");
    now = (time_t) s_time ((long *) 0);
    mask = s_umask (0);
    (VOID) s_umask (mask);
    statp -> st_mode = (ushort) (S_IFDIR | (DIR_MAGIC & ~mask));
    statp -> st_uid = info.bru_uid;
    statp -> st_gid = info.bru_gid;
    statp -> st_atime = now;
    statp -> st_mtime = now;
    DBUG_VOID_RETURN;
}
