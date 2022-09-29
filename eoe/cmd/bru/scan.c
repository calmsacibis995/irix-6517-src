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
 *	scan.c    routines to execute a function for each archived file
 *
 *  SCCS
 *
 *	@(#)scan.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	These routines provide a central facility for scanning an
 *	archive, passing control to a given function for each file
 *	in the archive.  The called function can process the file
 *	or ignore it as it chooses.
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

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "config.h"		/* Configuration file */
#include "errors.h"		/* Error codes */
#include "blocks.h"		/* Archive structures */
#include "macros.h"		/* Useful macros */
#include "finfo.h"		/* File info structure */
#include "trees.h"		/* File tree stuff */


/*
 *	External bru functions.
 */

extern VOID stripz ();		/* Strip a .Z extension from filename */
extern BOOLEAN hcheck ();	/* Perform sanity check on header block */
extern LBA ar_tell ();		/* Give current block number */
extern VOID bru_error ();		/* Report an error to user */
extern VOID ar_read ();		/* Read archive block */
extern VOID copyname ();	/* Copy names around */
extern VOID hstat ();		/* Decode a header block */
extern VOID nodes_ignored ();	/* Test tree for nodes not processed */
extern VOID clear_tree ();	/* Reset all match info for next scan */
extern int path_type ();	/* Determine type of path */
extern union blk *ar_next ();	/* Get pointer to next archive block */
extern union blk *ar_seek ();	/* Locate given logical block in archive */
extern UINT bufsize;		/* Archive read/write buffer size */
extern struct finfo afile;	/* The archive file info struct */

VOID finfo_init ();		/* Initialize a finfo structure */

/*
 *	Local functions.
 */

static VOID scan_errors ();	/* Give scan errors for each file */
static VOID missing ();		/* Process nodes missing from archive */


/*
 *  FUNCTION
 *
 *	scan    scan the archive executing function for each file
 *
 *  SYNOPSIS
 *
 *	VOID scan (funcp)
 *	VOID (*funcp)();
 *
 *  DESCRIPTION
 *
 *	Scans the archive, locating the header block of each
 *	archived file, decoding the header into a stat buffer,
 *	and calling the specified function with a block pointer
 *	pointing to the file header block, and a file information
 *	structure pointer.
 *
 *	Note that the pathname and state of the file linkage
 *	flag must be saved prior to executing the function.
 *	This is because if the file is a regular file,
 *	the function may request archive blocks itself,
 *	which may potentially overlay the block pointed to
 *	by blkp.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin scan
 *	    Initialize a file info structure for files found
 *	    Remember buffer for linked name
 *	    Reset flag for done with archive
 *	    While not done with archive
 *		Seek to next block of archive
 *		Reset file logical block address to zero
 *		Read the block from archive
 *		Done if at end of archive
 *		If not done then
 *		    If header check fails then
 *		        Tell user about error and resync attempt
 *		        Do
 *			    Seek to next block
 *			    Read block from archive
 *			    Done if at end of archive
 *		        While not done and bad header
 *		    Endif
 *		    If not done then
 *			Remember archive logical block of file header
 *			Decode header block
 *			Save copy of name from header block
 *			Save copy of linked name from header block
 *			Save state of file linkage from header block
 *			If this file is selected then
 *			    Reset scan flags
 *			    Initialize file logical block address
 *			    Reset file checksum error count
 *			    Get type of pathname
 *			    If no more matches possible then
 *				Set done with archive flag
 *			    Else
 *			        Invoke caller specified function
 *			        Check for errors during scan
 *			    End if
 *			End if
 *			If not done and not linked to another file then
 *			    Compute number of blocks to skip
 *			    Skip over contents of file, if any
 *			End if
 *		    End if
 *		End if
 *	    End while
 *	    Test tree for nodes not processed in archive scan
 *	    Reset all match information in tree for next scan
 *	End scan
 *
 */

VOID scan (funcp)
VOID (*funcp)();
{
    register LBA skip;			/* Number of blocks to skip */
    register LBA hdrlba;		/* Lba of file header block */
    register BOOLEAN done;		/* Finished scanning archive */
    register union blk *blkp;		/* Pointer to block */
    register BOOLEAN linked;		/* Linked to previous entry */
    auto struct finfo file;		/* A file info buffer */
    auto char name[NAMESIZE];		/* File name buffer */
    auto char lname[NAMESIZE];		/* Linked name buffer */
    auto struct stat64 sbuf;		/* Decoded header stat */
    BOOLEAN selected ();		/* Apply selection criteria */
    extern union blk *blocks;		/* Pointer to block buffer */
    extern int lbi;			/* Index into blocks[] */
    BOOLEAN bflag=FALSE;
    
    DBUG_ENTER ("scan");
    finfo_init (&file, name, &sbuf);
    done = FALSE;
    while (!done) {
	DBUG_PRINT ("header", ("next header block"));
	file.lname = lname;	/* inside loop because of new add_link calls
		during extract and list to handled hardlinks to symlinks */
	blkp = ar_next ();
	file.flba = 0L;
	ar_read (&file);
	done = eoablk (blkp);
	if (!done) {
	    if (!hcheck (blkp)) {
	        bru_error (ERR_DSYNC);
	        do {
		    int tapeblocksize = 512; /* use MTIOGETBLKINFO later*/
		    if(tapeblocksize < BLKSIZE) {
		    	int i, rval;
		    	for(i=0; i< BLKSIZE/tapeblocksize; i++) {
			    blkp = (union blk *)((char *)blkp + tapeblocksize);
			    if (FROMHEX (blkp -> HD.h_magic) == H_MAGIC) {
			    	if(&blkp[1] >= &blocks[bufsize/BLKSIZE]) {
				    i = (char *)blkp - (char *)blocks;
				    memcpy(blocks, blkp, bufsize - i);
				    rval = s_read(afile.fildes,
					    (char *)blocks + bufsize-i,i);
				    if(rval != i) {
				   	s_fprintf(stderr,
					    "s_read req=%d read =%d\n",i,rval);
				    	s_fflush(stderr);
				    }
				    blkp = blocks;
				    lbi = 0;
			    	}
			    	if(hcheck(blkp)) {
				    LBA bfound;
				    bflag = TRUE;
				    if(blkp != blocks) {
					/* need to copy down and read rest of
					   data, to fill the blocks buffer. */
					i = (char *)blkp - (char *)blocks;
				    	memcpy(blocks, blkp, bufsize - i);
				    	rval = s_read(afile.fildes,
						(char *)blocks + bufsize-i,i);
				    	if(rval != i) {
					    s_fprintf(stderr,
						"s_read req=%d read=%d\n",i,rval);
					    s_fflush(stderr);
				    	}
				   	blkp = blocks;
				   	lbi = 0;
				    }
				    /* else did before the hcheck call */
				    bfound = FROMHEX (blkp->HD.h_flba);
				    file.flba = bfound;
				    break;
			    	}
			    }
		    	}
		    	if(bflag) {
			    bflag = FALSE;
			    break;
		    	}
		    }
		    blkp = ar_next ();
		    file.flba = 0L;
		    ar_read (&file);
		    done = eoablk (blkp);
	        } while (!done && !hcheck (blkp));
	    }
	    if (!done) {
		hdrlba = ar_tell ();
		hstat (blkp, &file);
		copyname (file.fname, blkp -> HD.h_name);
		copyname (file.lname, blkp -> FH.f_lname);
		if (IS_COMPRESSED (&file)) {
		    stripz (file.fname);
		    stripz (file.lname);
		}
		linked = LINKED (blkp);
		if (selected (&file)) {
		    file.fi_flags &= ~(FI_CHKSUM | FI_BSEQ);
		    file.flba = 1L;
		    file.chkerrs = 0L;
		    file.type = path_type (file.fname);
		    if (file.type == FINISHED) {
			done = TRUE;
		    } else {
			(*funcp) (blkp, &file);
			scan_errors (&file);
		    }
		}
		if (!done && !linked) {
		    skip = ZARBLKS (&file);
		    DBUG_PRINT ("skip", ("skip %ld blocks", skip));
		    (VOID) ar_seek ((LBA)(hdrlba + skip), 0);
		}
	    }
	}
    }
    nodes_ignored (missing);
    clear_tree ();
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	scan_errors    check for errors during file scan
 *
 *  SYNOPSIS
 *
 *	static VOID scan_errors (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	There are certain errors, such as a block checksum error,
 *	for which it is more appropriate to issue a message about
 *	on a file by file basis, rather than at each occurance.
 *	Thus information about these errors is collected for
 *	testing after the file has been processed.  This routine
 *	is responsible for testing for these special errors and
 *	issuing appropriate messages.
 *
 */

static VOID scan_errors (fip)
struct finfo *fip;
{
    DBUG_ENTER ("scan_errors");
    DBUG_PRINT ("fip", ("flags %#4x", fip -> fi_flags));
    if (fip -> fi_flags & FI_CHKSUM) {
	bru_error (ERR_SUM, fip -> fname, fip -> chkerrs);
    }
    if (fip -> fi_flags & FI_BSEQ) {
	bru_error (ERR_BSEQ, fip -> fname);
    }
    DBUG_VOID_RETURN;
}

static VOID missing (name)
char *name;
{
    DBUG_ENTER ("missing");
    bru_error (ERR_IGNORED, name);
    DBUG_VOID_RETURN;
}
