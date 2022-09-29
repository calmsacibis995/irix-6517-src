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
 *	estimate.c    functions for estimating media usage
 *
 *  SCCS
 *
 *	@(#)estimate.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains functions for estimating media usage for same
 *	command with "create mode".  This is only an estimate and
 *	may not be accurate if some files are "statable" but
 *	unreadable.  However, when wrong the estimate will almost always
 *	be an over-estimate.
 *
 *	Reports the estimated number of volumes required, the number
 *	of files to be archive, the total number of archive blocks,
 *	and the total size of the archive in kilobytes.
 *
 */

#include "autoconfig.h"

#include <stdio.h>
#include <limits.h>

#if (unix || xenix)
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  include "sys.h"
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
#include "bruinfo.h"		/* Information about invocation */


/*
 *	External bru functions.
 */

extern BOOLEAN confirmed ();	/* Confirm action to be taken */
extern BOOLEAN selected ();	/* File passes selection criteria */
extern LBA ar_tell ();		/* Give current block number */
extern VOID bru_error ();	/* Report an error to user */
extern VOID ar_estimate ();	/* Add logical blocks to estimate */
extern VOID free_list ();	/* Free the list of links */
extern VOID tree_walk ();	/* Walk tree */
extern VOID verbosity ();	/* Give a verbosity message */
extern char *add_link ();	/* Add linked file to list */
extern int ar_vol ();		/* Find current volume number */
extern char *getlinkname();

/*
 *	System library interface functions.
 */
 
extern int s_fprintf ();	/* Formatted print on stream */

/*
 *	Extern bru variables.
 */

extern struct cmd_flags flags;	/* Flags given on command line */
extern FILE *logfp;		/* Verbosity stream */
extern char mode;		/* Current mode (cdehitx) */
extern struct bru_info info;	/* Information about bru invocation */

/*
 *	Local variables.
 */

static long files = 0L;		/* Number of files found */


/*
 *	Local function declarations.
 */

static VOID do_estimate ();	/* Estimate file after tests applied */
static VOID efile ();		/* Estimate file requirements */
static VOID totals ();		/* Output estimated totals */


/*
 *  NAME
 *
 *	estimate    estimate the media usage requirements
 *
 *  SYNOPSIS
 *
 *	VOID estimate ()
 *
 *  DESCRIPTION
 *
 *	This is the main entry point for estimate media usage when
 *	creating archives.
 *	It is called once all the command line options have been
 *	processed and common initialization has been performed.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin estimate
 *	    Set current mode to estimate
 *	    Reset count of files
 *	    Accumulate space for archive header block
 *	    Walk directory tree, estimate requirements for files
 *	    Accumulate space for archive trailer block
 *	    Print estimate totals
 *	    Discard any linkage information collected
 *	End create
 *
 */

VOID estimate ()
{
    
    DBUG_ENTER ("estimate");
    mode = 'e';
    files = 0L;
    ar_estimate (1L);
    tree_walk (efile);
    ar_estimate (1L);
    totals ();
    free_list ();
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	efile    estimate usage for a file
 *
 *  SYNOPSIS
 *
 *	static VOID efile (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a file information structure, estimates
 *	the media requirements for archiving this particular file.
 *	Each regular file requires one block for the file header
 *	and zero or more data blocks for storing the file's contents.
 *
 *	Note that directories, block special files, character special
 *	files, and other special files only have a header block written.
 *	This is so those nodes can be restored with the proper attributes.
 *	This is a particular failing of the Unix "tar" utility.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin efile
 *	    If bad arguments then
 *		Tell user about bug
 *	    End if
 *	    If there is a file to estimate and is not "." or ".." then
 *		If this file is selected then
 *		    If estimating confirmed then
 *			Committed, so estimate it's requirements
 *		    End if
 *		End if
 *	    End if
 *	End efile
 *
 */

static VOID efile (fip)
struct finfo *fip;
{
    DBUG_ENTER ("efile");
    if (fip == NULL) {
	bru_error (ERR_BUG, "efile");
    }
    DBUG_PRINT ("path", ("estimate usage for \"%s\"", fip -> fname));
    if (*fip -> fname != EOS && !DOTFILE (fip -> fname)) {
	if (selected (fip)) {
	    if (confirmed ("e %s", fip)) {
		do_estimate (fip);
	    }
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	do_estimate    committed to doing estimate so do it
 *
 *  SYNOPSIS
 *
 *	static VOID do_estimate (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Once it has been decided to estimate a file's requirements
 *	after applying all tests, this function is called to do
 *	the actual estimation.  Note how this parallels the archive
 *	creation logic.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin do_estimate
 *	    If bad arguments then
 *		Tell user about bug
 *	    End if
 *	    Increment count of files to archive
 *	    If not a symbolic link then
 *		Set linked file name to NULL.
 *	    End if
 *	    If not directory and more than one link then
 *		Add link to list of linked files
 *	    End if
 *	    Do verbosity processing
 *	    If the file is linked to another file then
 *		Remember that file is linked
 *	    Else
 *		Remember file is not linked
 *	    End if
 *	    Accumulate space for file header block
 *	    If not linked and file is a regular file then
 *		Accumulate space for file contents
 *	    End if
 *	End do_estimate
 *
 */

static VOID do_estimate (fip)
struct finfo *fip;
{
	register BOOLEAN linked;
	char *savelinkname;

	DBUG_ENTER ("do_estimate");
	if (fip == NULL) {
		bru_error (ERR_BUG, "do_estimate");
	}
	files++;
	/* symbolic links that are hard linked to other symbolic links we
     * have already seen are treated as just links.  SGI seems to
     * be one of the few vendors that even allows this */
	savelinkname = fip->lname;
	if (!IS_FLNK (fip -> statp -> st_mode) || (fip->statp->st_nlink>1 &&
	    getlinkname(fip))) {
		fip->lname = NULL;
	}
	if (!IS_DIR (fip -> statp -> st_mode) && fip -> statp -> st_nlink > 1) {
		fip -> lname = add_link (fip);
		if(!fip->lname && IS_FLNK(fip->statp->st_mode))
			fip->lname = savelinkname;
	}
	if(fip -> lname != NULL && *fip -> lname != EOS)
		linked = TRUE;
	else
		linked = FALSE;

	/* same as in create.c; do this check before we ever get to open */
	if(IS_REG(fip->statp->st_mode) && fip->statp->st_size > LONG_MAX) {
		if(!flags.Kflag) {
			bru_error (ERR_NO_KFLAG,fip->fname);
			DBUG_VOID_RETURN;
		}
		else if(!flags.Zflag) {
			bru_error (ERR_NO_ZFLAG,fip->fname);
			DBUG_VOID_RETURN;
		}
	}

	if(openandchkcompress(fip) == TRUE) {
	    /* verbosity and ar_estimate *must* be called before discard_zfile */
		verbosity (fip);
		if (!linked && IS_REG (fip->statp->st_mode))
		ar_estimate ((LBA) ZARBLKS (fip));
	    if(IS_REG (fip->statp->st_mode) && fip->lname == NULL){
		if (s_close (fip -> fildes) == SYS_ERROR)
		    bru_error (ERR_CLOSE, fip -> fname);
		discard_zfile (fip);
		reset_times (fip);
	    }
	}
	ar_estimate (1L);
	DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	totals   print results of estimate
 *
 *  SYNOPSIS
 *
 *	static VOID totals ()
 *
 *  DESCRIPTION
 *
 *	Prints results of the estimate.  Reports the number of
 *	volumes, the number of archive blocks, and the total
 *	number of kilobytes of archive.
 *
 */
 
static VOID totals ()
{
    register int volume;
    ULONG arblocks;
    
    DBUG_ENTER ("totals");
    volume = ar_vol () + 1;
    arblocks = (ULONG)ar_tell ();
    (VOID) s_fprintf (logfp, "%s:", info.bru_name);
    (VOID) s_fprintf (logfp, " %d volume(s),", volume);
    (VOID) s_fprintf (logfp, " %ld files,", files);
    (VOID) s_fprintf (logfp, " %lu archive blocks,", arblocks);
    (VOID) s_fprintf (logfp, " %lu Kbytes\n",  KBYTES (arblocks));
    DBUG_VOID_RETURN;
}

