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
 *	create.c    functions for creating archive
 *
 *  SCCS
 *
 *	@(#)create.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains functions concerned primarily with creating new
 *	archives.
 *
 */

#include "autoconfig.h"

#include <stdio.h>
#include <limits.h>

#if (unix || xenix)
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#else
#  include "sys.h"		/* Header to fake stuff for non-unix */
#endif

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "config.h"		/* Configuration file */
#include "errors.h"		/* Error codes */
#include "blocks.h"		/* Archive format */
#include "macros.h"		/* Useful macros */
#include "finfo.h"		/* File information structure */
#include "flags.h"		/* Command line flags */
#include "bruinfo.h"		/* Current invocation info */
#include "exeinfo.h"		/* Runtime info */


/*
 *	External bru functions.
 */

extern VOID discard_zfile ();	/* Discard the compressed version of file */
extern BOOLEAN compressfip ();	/* Compress given file if compressable */
extern VOID addz ();		/* Add a ".Z" extension to filename */
extern BOOLEAN confirmed ();	/* Confirm action to be taken */
extern BOOLEAN copy ();		/* Copy N characters */
extern BOOLEAN selected ();	/* File passes selection criteria */
extern VOID bru_error ();	/* Report an error to user */
extern VOID ar_close ();	/* Flush buffers and close the archive */
extern VOID ar_open ();		/* Open the archive */
extern VOID ar_write ();	/* Logical write of current archive block */
extern VOID copyname ();	/* Copy pathnames around */
extern VOID done ();		/* Finish up and exit */
extern VOID free_list ();	/* Free the list of links */
extern VOID reload ();		/* Reload first volume for rescan */
extern VOID reset_times ();	/* Reset atime and mtime of file */
extern VOID tohex ();		/* Convert 32 bit integer to hex */
extern VOID tree_walk ();	/* Walk tree */
extern VOID unresolved ();	/* Complain about unresolved links */
extern VOID verbosity ();	/* Give a verbosity message */
extern char *add_link ();	/* Add linked file to list */
extern union blk *ar_next ();	/* Get pointer to next archive block */
extern union blk *ar_seek ();	/* Locate given logical block in archive */
extern VOID mark_archived ();	/* Set the "has been archived" bit */
extern VOID clear_archived ();	/* Clear the "has been archived" bit */
extern char *getlinkname();

#if HAVE_MEMSET
#define zeroblk(blkp) (void)memset((blkp)->bytes,0,ELEMENTS((blkp)->bytes))
#else
extern VOID zeroblk ();		/* Zero an archive block */
#endif

/*
 *	System library interface functions.
 */
 
extern char *s_strcpy ();	/* Copy strings */
extern int s_close ();		/* Invoke library file close function */
extern int s_open ();		/* Invoke library file open function */
extern int s_read ();		/* Invoke library file read function */
extern int s_uname ();		/* Invoke library function to get sys info */
extern int s_unlink ();		/* Invoke library function to delete file */

/*
 *	Extern bru variables.
 */

extern struct cmd_flags flags;	/* Flags given on command line */
extern struct exe_info einfo;	/* Execution information */
extern struct bru_info info;	/* Invocation information */
extern char *id;		/* ID of bru */
extern char mode;		/* Current mode (citdxh) */
extern char *label;		/* Archive label string given by user */
extern struct finfo afile;	/* The archive file */


/*
 *	Local function declarations.
 */

static VOID do_write ();	/* Write file after tests applied */
static VOID winfo ();		/* Write archive info block */
static VOID wheader ();		/* Write a file header block */
static VOID markend ();		/* Mark end of archive */
static VOID wfile ();		/* Write a file to archive */
static VOID wcontents ();	/* Write file contents to archive */
static VOID bldhdr ();		/* Build a file header block */
static VOID check_intr ();	/* Check for interrupt */

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
 *	create    main entry point for creating new archives
 *
 *  SYNOPSIS
 *
 *	VOID create ()
 *
 *  DESCRIPTION
 *
 *	This is the main entry point for creating new archives.
 *	It is called once all the command line options have been
 *	processed and common initialization has been performed.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin create
 *	    Set current mode to create
 *	    Open the archive file
 *	    Save state of signal handling
 *	    Catch signals
 *	    Write the archive header block
 *	    Walk directory tree, archiving each file
 *	    Mark end of archive
 *	    Restore previous signal handling state
 *	    If not suppressing link warnings
 *		Complain about unresolved links
 *	    End if
 *	    Free the list of unresolved links
 *	    Close the archive file
 *	End create
 *
 */

VOID create ()
{
    SIGTYPE prevINT;
    SIGTYPE prevQUIT;
    
    DBUG_ENTER ("create");
    mode = 'c';
    reload ("archive creation");
    ar_open ();
    sig_push (&prevINT, &prevQUIT);
    sig_catch ();
    winfo (&afile);
    tree_walk (wfile);
    markend (&afile);
    sig_pop (&prevINT, &prevQUIT);
    if (!flags.lflag) {
	unresolved ();
    }
    free_list ();
    ar_close ();
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	wfile    write a file to archive
 *
 *  SYNOPSIS
 *
 *	static VOID wfile (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given pathname to a file, writes that file to the archive.
 *	Each regular file written in the archive has a header block 
 *	containing information about the file, and zero or more data
 *	blocks containing the actual file contents.
 *
 *	Note that directories, block special files, character special
 *	files, and other special files only have a header block written.
 *	This is so those nodes can be restored with the proper attributes.
 *	This is a particular failing of the Unix "tar" utility.
 *
 *	Note that the file must be opened with the O_NDELAY flag
 *	set or else character special files associated with communication
 *	lines will cause the open to block until the line has carrier
 *	present.  See open(2) in the Unix Sys V User's Manual.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin wfile
 *	    If bad arguments then
 *		Tell user about bug
 *	    End if
 *	    Check for interrupts
 *	    If there is a file to write and is not "." or ".." then
 *		If this file is selected then
 *		    If archiving confirmed then
 *			Committed, so write it to archive
 *		    End if
 *		End if
 *	    End if
 *	End wfile
 *
 */

static VOID wfile (fip)
struct finfo *fip;
{
	DBUG_ENTER ("wfile");
	if (fip == NULL) {
		bru_error (ERR_BUG, "wfile");
	}
	check_intr ();
	DBUG_PRINT ("path", ("write \"%s\" to archive", fip -> fname));
	if (*fip -> fname != EOS && !DOTFILE (fip -> fname)) {
		/*
		 * Added for xfs - Don't archive big files unless -K and -Z set
		 */
		if (fip->statp->st_size > LONG_MAX ) {
			if (!flags.Kflag) {
				bru_error (ERR_NO_KFLAG,fip->fname);
				DBUG_VOID_RETURN;
			}
			if (!flags.Zflag) {
				bru_error (ERR_NO_ZFLAG,fip->fname);
				DBUG_VOID_RETURN;
			}
		}
	}
	if (selected (fip)) {
		if (confirmed ("c %s", fip)) {
			do_write (fip);
		}
	}
	DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	do_write    committed to doing write so do it
 *
 *  SYNOPSIS
 *
 *	static VOID do_write (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Once it has been decided to write a file to the archive,
 *	after applying all tests, this function is called to do
 *	the actual write.
 *
 *	If the command line option -Z to store files in a compressed
 *	form has been given, the input file is compressed, and the
 *	resulting temporary compressed file is saved in the archive.
 *	For backwards compatibility with versions of bru that don't
 *	understand compressed files, the saved file name has a ".Z"
 *	extension appended to it, the standard name for compressed
 *	versions of files.  The original file size is saved in an
 *	area that won't be seen by older versions of bru, and the
 *	compressed file size is stored in the normal size location.
 *	Thus, if an archive built by a new version of bru is read
 *	by an old version of bru, it will appear that all the files
 *	were compressed before archiving, and the files will be
 *	extracted as compressed files.
 *
 *	This routine has been reorganized considerably in order to
 *	get file compression implemented, because the input file 
 *	needs to be compressed BEFORE verbosity() is called.  Plus
 *	verbosity() should not be called if we can't access or open
 *	the file for some reason, and the file is a file we will
 *	need to access and read.
 */


/*
 *  PSEUDO CODE
 *
 *	Begin do_write
 *	    If bad arguments then
 *		Tell user about bug
 *	    End if
 *	    If not a symbolic link then
 *		Set linked file name to NULL.
 *	    End if
 *	    If not directory and more than one link then
 *		Add link to list of linked files
 *	    End if
 *	    Do verbosity processing
 *	    Set current file block to zero
 *	    If is special file or linked to another file then
 *		Write header block only
 *	    Else
 *		If file is accessible for read then
 *		    Open the file for read
 *		    If open failed then
 *			Report open error to user
 *		    Else
 *			Write the header block to archive
 *			Write the file contents to archive
 *			If file close failed then
 *			    Report close error to user
 *			End if
 *			Reset the file times
 *			If should mark file as archived then
 *			    Set file archived bit
 *			Else if should mark file unarchived then
 *			    Reset file archived bit
 *			Endif
 *		    End if
 *		End if
 *	    End if
 *	End do_write
 *
 */

static VOID do_write (fip)
struct finfo *fip;
{
	BOOLEAN fileok = FALSE;
	char *savelinkname;

	DBUG_ENTER ("do_write");
	if (fip == NULL) {
		bru_error (ERR_BUG, "do_write");
	}
	/* symbolic links that are hard linked to other symbolic links we
	 * have already seen are treated as just links.  SGI seems to
	 * be one of the few vendors that even allows this */
	savelinkname = fip->lname;
	if(!IS_FLNK(fip->statp->st_mode) || (fip->statp->st_nlink>1 &&
	    getlinkname(fip))) {
		fip->lname = NULL;
	}
	if (!IS_DIR (fip -> statp -> st_mode) && fip -> statp -> st_nlink > 1) {
		fip -> lname = add_link (fip);
		if(!fip->lname && IS_FLNK(fip->statp->st_mode))
			fip->lname = savelinkname;
	}
	fileok = openandchkcompress(fip);
	if (!IS_REG (fip -> statp -> st_mode) || fip -> lname != NULL) {
		verbosity (fip);
		fip -> flba = 0L;
		wheader (fip);
	} else if (fileok) {
			if (fip->statp->st_size > LONG_MAX )
				bru_error (ERR_OK_FLAG,fip->fname);
		    verbosity (fip);
		    fip -> flba = 0L;
		    wheader (fip);
		    wcontents (fip);
		if (s_close (fip -> fildes) == SYS_ERROR) {
			bru_error (ERR_CLOSE, fip -> fname);
		}
		discard_zfile (fip);
		reset_times (fip);
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
 *	wheader    build and write header block to archive
 *
 *  SYNOPSIS
 *
 *	static VOID wheader (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Builds a header block and writes it to the archive.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin wheader
 *	    If bad arguments then
 *		Warn user about bug
 *	    End if
 *	    Get next archive block
 *	    Zero the block
 *	    Copy the file name to block header
 *	    If linked to another file then
 *		Copy the linked name to file header
 *	    End if
 *	    Build the rest of the file header
 *	    Write the header block to archive
 *	End wheader
 *
 */

static VOID wheader (fip)
struct finfo *fip;
{
    register union blk *blkp;

    DBUG_ENTER ("wheader");
    if (fip == NULL) {
	bru_error (ERR_BUG, "wheader");
    }
    blkp = ar_next ();
    zeroblk (blkp);
    copyname (blkp -> HD.h_name, fip -> fname);
    if (IS_COMPRESSED (fip)) {
	addz (blkp -> HD.h_name);
    }
    if (fip -> lname != NULL) {
	copyname (blkp -> FH.f_lname, fip -> lname);
	if (IS_COMPRESSED (fip)) {
	    addz (blkp -> FH.f_lname);
	}
    }
    bldhdr (blkp, fip);
    ar_write (fip, H_MAGIC);
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	wcontents    write contents of normal file to archive
 *
 *  SYNOPSIS
 *
 *	static VOID wcontents (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	This routine is responsible for writing the contents of a normal
 *	file to the archive.
 *
 *	Note that each block is zeroed to assure that no junk is left
 *	laying around in it from it's last use.  This is necessary because
 *	the data section is only part of the block (otherwise only the
 *	last block would require zeroing).  The header is initialized
 *	by ar_write, which may not clear unused fields.
 *
 *  BUGS
 *
 *	In previous versions of bru, if the read failed or the file was
 *	truncated, the number of blocks in the archive did not match the
 *	number expected from the file size stored in the file header
 *	block.  This caused problems when the archive was read because
 *	the header block of the following file was never found.
 *
 *	The solution was to always write the correct number of blocks
 *	to the archive, regardless of whether or not their contents
 *	are correct.  This fixes the previous problem and insures
 *	that as much as possible of the file is preserved.  Note
 *	however, that the only error observed is when the archive
 *	is created.  Once the archive is created, as far as it is
 *	concerned, all the internal data is correct.
 *
 *	Another problem is what to do about files that grow while
 *	archiving.  The solution implemented currently is simply to warn
 *	the user and discard any file additions.  The file scanning
 *	routines could be made smarter about finding a data block when
 *	expecting a file header.  This would allow the additional data
 *	to be archived.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin wcontents
 *	    Initialize the file truncated flag to FALSE
 *	    If the file information pointer is bad then
 *		Warn user about bug
 *	    End if
 *	    Get the number of bytes in the file to be archived
 *	    If the file has anything in it at all then
 *		For each block of data to be archived
 *		    Increment the file block number
 *		    Get the next archive block
 *		    Zero the block
 *		    If only a partial block left to read then
 *			Read only the correct number of bytes
 *		    End if
 *		    Read data from file into block
 *		    Write the block to the archive
 *		    If read was not correct then
 *			Set the file has been truncated flag
 *		    End if
 *		End for
 *		If last read returned with error condition then
 *		    Warn user about read error
 *		End if
 *		If the file was truncated then
 *		    Warn user about truncation
 *		Else
 *		    Attempt to read some more data from file
 *		    If successful then
 *			Warn user that the file grew while archiving
 *		    End if
 *		End if
 *	    End if
 *	End wcontents
 *
 */

static VOID wcontents (fip)
struct finfo *fip;
{
    register off_t bytes;
    register int iobytes;
    register UINT rbytes;
    register union blk *blkp;
    auto char testbuf[DATASIZE];
    register BOOLEAN truncated;
    register int fildes;
    char *fname;

    DBUG_ENTER ("wcontents");
    truncated = FALSE;
    if (fip == NULL) {
	bru_error (ERR_BUG, "wcontents");
    }
    if (IS_COMPRESSED (fip)) {
	bytes = fip -> zsize;
	fildes = fip -> zfildes;
	fname = fip -> zfname;
    } else {
	bytes = fip -> statp -> st_size;
	fildes = fip -> fildes;
	fname = fip -> fname;
    }
    DBUG_PRINT ("rbytes", ("read and save %ld bytes from '%s'", bytes, fname));
    if (bytes > 0) {
	for (rbytes = DATASIZE; bytes > 0; bytes -= rbytes) {
	    fip -> flba++;
	    blkp = ar_next ();
	    zeroblk (blkp);
	    if (bytes < DATASIZE) {
		rbytes = bytes;
	    }
	    iobytes = s_read (fildes, blkp -> FD, rbytes);
	    ar_write (fip, D_MAGIC);
	    if (iobytes != rbytes) {
		truncated = TRUE;
	    }
	}
	if (iobytes == SYS_ERROR) {
	    bru_error (ERR_READ, fname);
	}
	if (truncated) {
	    bru_error (ERR_FTRUNC, fname);
	} else {
	    iobytes = s_read (fildes, testbuf, DATASIZE);
	    if (iobytes > 0) {
		bru_error (ERR_FGREW, fname);
	    }
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	bldhdr    do grunge work for building a file header
 *
 *  SYNOPSIS
 *
 *	static VOID bldhdr (blkp, fip)
 *	register union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Does the actual work of building each field in the file
 *	header block.
 *
 *	Note that if the file is a directory, the size in the file
 *	header block is set to zero, since we don't actually archive
 *	any of the contents of directory files.
 *
 */

static VOID bldhdr (blkp, fip)
register union blk *blkp;
register struct finfo *fip;
{
    DBUG_ENTER ("bldhdr");
    if (fip == NULL || blkp == NULL) {
	bru_error (ERR_BUG, "bldhdr");
    }
    TOHEX (blkp -> HD.h_magic, H_MAGIC);
    TOHEX (blkp -> FH.f_mode, fip -> statp -> st_mode);
    TOHEX (blkp -> FH.f_ino, fip -> statp -> st_ino);
    TOHEX (blkp -> FH.f_dev, fip -> statp -> st_dev);
    TOHEX (blkp -> FH.f_rdev, fip -> statp -> st_rdev);
    TOHEX (blkp -> FH.f_nlink, fip -> statp -> st_nlink);
    TOHEX (blkp -> FH.f_uid, fip -> statp -> st_uid);
    TOHEX (blkp -> FH.f_gid, fip -> statp -> st_gid);
    if (IS_REG (fip -> statp -> st_mode)) {
	if (IS_COMPRESSED (fip)) {
	    TOHEX (blkp -> FH.f_size, fip -> zsize);
	    /*
	     * Added for xfs
	     */
	    if (fip -> statp -> st_size > LONG_MAX)
 	 	    TOHEX (blkp -> FH.f_xsize, -1);
	    else
		    TOHEX (blkp -> FH.f_xsize, fip -> statp -> st_size);
	} else {
	    if (fip -> statp -> st_size > LONG_MAX)
		    TOHEX (blkp -> FH.f_size, -1);
	    else
		    TOHEX (blkp -> FH.f_size, fip -> statp -> st_size);
	}
    } else {
	TOHEX (blkp -> FH.f_size, 0L);
    }
    TOHEX (blkp -> FH.f_atime, fip -> statp -> st_atime);
    TOHEX (blkp -> FH.f_mtime, fip -> statp -> st_mtime);
    TOHEX (blkp -> FH.f_ctime, fip -> statp -> st_ctime);
    TOHEX (blkp -> FH.f_flags, fip -> fi_flags);
    TOHEX (blkp -> FH.f_fibprot, fip -> fib_Protection);
    (VOID) s_strcpy (blkp -> FH.f_fibcomm, fip -> fib_Comment);
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	winfo    write the archive header block
 *
 *  SYNOPSIS
 *
 *	static VOID winfo (afip)
 *	register struct finfo *afip;
 *
 *  DESCRIPTION
 *
 *	Builds and writes the archive header block, which is
 *	the first block in the archive.  This block contains
 *	useful information about the archive.
 *
 */

static VOID winfo (afip)
register struct finfo *afip;
{
    register union blk *blkp;

    DBUG_ENTER ("winfo");
    if (afip == NULL) {
	bru_error (ERR_BUG, "winfo");
    }
    blkp = ar_seek (0L, 0);
    zeroblk (blkp);
    TOHEX (blkp -> AH.a_uid, info.bru_uid);
    TOHEX (blkp -> AH.a_gid, info.bru_gid);
    copyname (blkp -> AH.a_name, afile.fname);
    if (flags.Lflag) {
	if (!copy (blkp -> AH.a_label, label, BLABELSIZE)) {
	    bru_error (ERR_LABEL, label);
	}
    }
    (VOID) s_strcpy (blkp -> AH.a_id, id);
    if (s_uname (&blkp -> AH.a_host) == SYS_ERROR) {
	bru_error (ERR_UNAME);
    }
    ar_write (afip, A_MAGIC);
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	markend    mark the end of the archive with a terminator block
 *
 *  SYNOPSIS
 *
 *	static VOID markend (afip)
 *	struct finfo *afip;
 *
 *  DESCRIPTION
 *
 *	Marks the end of the archive by writing a terminator block.
 *
 */

static VOID markend (afip)
struct finfo *afip;
{
    register union blk *blkp;

    DBUG_ENTER ("markend");
    if (afip == NULL) {
	bru_error (ERR_BUG, "afip");
    }
    blkp = ar_next ();
    zeroblk (blkp);
    ar_write (afip, T_MAGIC);
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	check_intr    check for an interrupt received
 *
 *  SYNOPSIS
 *
 *	static VOID check_intr ()
 *
 *  DESCRIPTION
 *
 *	Checks to see if an interrupt was received while signals
 *	were being caught.  If so, marks the end of the archive
 *	and cleans up and exits.
 *
 */

static VOID check_intr ()
{
    if (interrupt) {
	DBUG_PRINT ("sig", ("interrupt recognized"));
	einfo.warnings++;
	markend (&afile);
	ar_close ();
	done ();
    }
}
