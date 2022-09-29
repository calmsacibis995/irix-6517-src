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
 *	info.c   routines to print only info from archive header
 *
 *  SCCS
 *
 *	@(#)info.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 * 	Opens archive, reading and dumping the header block.  Reads
 *	no other archive blocks, because scan() ain't called.
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

extern char mode;		/* Current mode (citdxhg) */


/*
 *  FUNCTION
 *
 *	info_only  main routine to display header info
 *
 *  SYNOPSIS
 *
 *	VOID info_only ()
 *
 *  DESCRIPTION
 *
 *	Mainline for the header-info mode.  Called after all initialization
 *	is complete.  Simply read archive header, prints info, then quits.
 *
 */

VOID info_only ()
{
    DBUG_ENTER ("info_only");
    mode = 'g';
    reload ("giving header info");
    ar_open ();		/* header info given here */
			/* ... so no reason to scan anything in archive */
    ar_close ();
    DBUG_VOID_RETURN;
}
