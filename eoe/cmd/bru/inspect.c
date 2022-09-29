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
 *	inspect.c    routines for doing self consistency checks
 *
 *  SCCS
 *
 *	@(#)inspect.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines for doing archive self consistency check.
 *	A self consistency check scans every block of the archive,
 *	verifying the checksum, the file logical block number,
 *	the header information, etc.
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
#include "config.h"		/* Configuration information */
#include "errors.h"		/* Error codes */
#include "blocks.h"		/* Archive format */
#include "macros.h"		/* Useful macros */
#include "trees.h"		/* Tree types */
#include "finfo.h"		/* File information structure */


/*
 *	External bru functions.
 */

extern BOOLEAN confirmed ();	/* Confirm action */
extern BOOLEAN chksum_ok ();	/* Test for good checksum */
extern VOID ar_close ();	/* Flush buffers and close the archive */
extern VOID ar_open ();		/* Open the archive */
extern union blk *ar_next ();	/* Get pointer to next archive block */
extern VOID ar_read ();		/* Read archive block */
extern VOID reload ();		/* Reload first volume for rescan */
extern S32BIT fromhex ();	/* Convert hex to 32 bit integer */
extern VOID verbosity ();	/* Give a verbosity message */
extern VOID scan ();		/* Scan an archive */

/*
 *	External bru variables.
 */

extern char mode;		/* Current mode (citdxh) */

/*
 *	Local functions.
 */

static VOID fcheck ();		/* Check file */
static VOID ccheck ();		/* Check contents */


/*
 *  FUNCTION
 *
 *	inspect    mainline for the inspect mode
 *
 *  SYNOPSIS
 *
 *	VOID inspect ()
 *
 *  DESCRIPTION
 *
 *	Mainline for the inspect mode.  Called after all initialization
 *	is complete.  Controls inspection of archive.
 *
 */

VOID inspect ()
{
    DBUG_ENTER ("inspect");
    mode = 'i';
    reload ("archive inspection");
    ar_open ();
    scan (fcheck);
    ar_close ();
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	fcheck    check a single file in archive
 *
 *  SYNOPSIS
 *
 *	static VOID fcheck (blkp, fip)
 *	register union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given pointer to the header block of an archived file (blkp)
 *	and pointer to a file info structure for archived file (fip),
 *	examines the file for readability and self consistency.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin fcheck
 *	    If file is explicitly or implicitly named then
 *		If action confirmed then
 *		    Issue verbosity message
 *		    If the file is a regular file and not linked then
 *			Check the file contents
 *		    End if
 *		End if
 *	    End if
 *	End fcheck
 *
 */

static VOID fcheck (blkp, fip)
register union blk *blkp;
register struct finfo *fip;
{
    DBUG_ENTER ("fcheck");
    if (IS_LEAF (fip) || IS_EXTENSION (fip)) {
	if (confirmed ("i %s", fip)) {
	    verbosity (fip);
	    if (IS_REG (fip -> statp -> st_mode) && !LINKED (blkp)) {
		ccheck (fip);
	    }
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	ccheck    check contents of an archive file
 *
 *  SYNOPSIS
 *
 *	static VOID ccheck (fip)
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a file info structure for and archived file,
 *	examines the file's archived contents for consistency and
 *	data integrity.
 *
 *  BUGS
 *
 *	This function needs to be expanded to perform more
 *	comprehensive checks.  Currently only checks readibility
 *	and checksums for each block.  Need to also check headers.
 *	Notice that simply reading the block also verifies it's
 *	checksum via "scan" and "ar_read".
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin ccheck
 *	    Get size of the file to check
 *	    While there is more archive blocks to check
 *		Seek to the next archive block
 *		Read the archive block
 *		Reduce number of bytes left to read
 *	    End while
 *	End ccheck
 *
 */

static VOID ccheck (fip)
register struct finfo *fip;
{
    register S32BIT bytes;

    DBUG_ENTER ("ccheck");
    DBUG_PRINT ("verify", ("verify contents of %s", fip -> fname));
    bytes = ZSIZE (fip);
    while (bytes > 0L) {
	(VOID) ar_next ();
	ar_read (fip);
	bytes -= DATASIZE;
    }
    DBUG_VOID_RETURN;
}
