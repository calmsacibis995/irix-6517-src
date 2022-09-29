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
 *	devices.c    routines for manipulating information about known devices
 *
 *  SCCS
 *
 *	@(#)devices.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	When bru is configured for a given environment, one of the
 *	things that must be set up is a table of known devices, to
 *	help in error recovery.  Initial versions of bru had this table
 *	compiled in and required source code changes to change the
 *	known devices.  This scheme was quickly superceded by an
 *	external table in human readable form, generally in /etc/brutab.
 *	The first form of this table had records with a fixed number of
 *	fields.  This presented various problems, leading to the current
 *	implementation which is very much like termcap.
 *
 *	Routines in this module are responsible for maintaining and
 *	examining the internal format of one entry in the device
 *	table.  The external representation and manipulation is
 *	performed by routines in the brutab.c module.
 *
 */
 

#include "autoconfig.h"

#include <stdio.h>

#if unix || xenix
#include <errno.h>
#endif
  
#if unix || xenix
#  include <sys/types.h>
#else
#  include "sys.h"			/* Fake stuff for non-unix hosts */
#endif

#include "typedefs.h"			/* Local type definitions */
#include "dbug.h"			/* Macro based debugger */
#include "manifest.h"			/* Manifest constants */
#include "config.h"			/* Configuration file */
#include "errors.h"			/* Known internal error codes */
#include "devices.h"			/* Structure of device table entry */
#include "macros.h"			/* Some useful macros */

extern struct device *ardp;		/* Currently active device */

extern BOOLEAN regular_file ();		/* Test for non-special file */


/*
 *	Given number of bytes transfered in last I/O operation,
 *	system error number returned, and a flag indicating if
 *	the error occured on a read, determines if it was likely
 *	that the end of the device was found.  This test is
 *	not always conclusive.  Sometimes unix hides too much
 *	information!.
 *
 *  Always returns true if count is 0.  This is what cpio
 *  has always done, and tar and bru now do starting with
 *  IRIX 4.0, since filemarks are now always written at the
 *  end of a tape for 9 track, DAT, and any new tape devices.
 *  Writes from tpsc.c will return 0 on first write after EW/EOM
 *  is encountered, and therefore reads will return 0 when
 *  the FM is encountered.  We don't bother to actually check
 *  for EW/EOM set via MTIOCGET because that seems like overkill.
 *  This also allows multivolume to work with files, rather than
 *  devices.
 */
BOOLEAN end_of_device (iobytes, ioerr, read)
int iobytes;
int ioerr;
BOOLEAN read;
{
    register BOOLEAN end;

    DBUG_ENTER ("end_of_device");
    DBUG_PRINT ("eod", ("ioerr %d, iobytes %d", ioerr, iobytes));
    end = FALSE;
    if (ardp != NULL) {
	if (iobytes == 0)
		end = TRUE;
        else if (iobytes == -1) {
	    if (read && ardp -> dv_zrerr == ioerr) {
		end = TRUE;
	    } else if (!read && ardp -> dv_zwerr == ioerr) {
		end = TRUE;
	    }
	} else {
	    if (read && ardp -> dv_prerr == ioerr) {
		end = TRUE;
	    } else if (!read && ardp -> dv_pwerr == ioerr) {
		end = TRUE;
	    }
	}
    }	
    else {
	/* If no description for the device, try for the ones that we
	 * know are likely to * be useful for SGI, and in general...
	 * In particular, ardp is usually null for remote tape  */
	if(ioerr == ENOSPC || iobytes == 0)
	    end = TRUE;
    }
    DBUG_PRINT ("eod", ("end of device flag %d", end));
    DBUG_RETURN (end);
}


/*
 *  FUNCTION
 *
 *	unformatted    test for error due to media unformatted
 *
 *  SYNOPSIS
 *
 *	BOOLEAN unformatted (iobytes, ioerr, read)
 *	int iobytes;
 *	int ioerr;
 *	BOOLEAN read;
 *
 *  DESCRIPTION
 *
 *	Uses information about number of bytes transferred on
 *	last I/O operation, error returned by operation, and
 *	whether operation was read or write, to determine if
 *	the media was unformatted.  As with the end of
 *	device test, this test may not always be accurate.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin unformatted
 *	    Default is FALSE
 *	    If operation returned error then
 *		If reading then
 *		    Get unformatted read error expected
 *		Else
 *		    Get unformatted write error expected
 *		End if
 *		If error was the expected error then
 *		    Result is TRUE
 *		End if
 *	    End if
 *	    Return result
 *	End unformatted
 *
 */

BOOLEAN unformatted (iobytes, ioerr, read)
int iobytes;
int ioerr;
BOOLEAN read;
{
    register BOOLEAN nofmt;
    register int err;

    DBUG_ENTER ("unformatted");
    DBUG_PRINT ("fmt", ("ioerr %d", ioerr));
    nofmt = FALSE;
    if (iobytes <= 0 && ardp != NULL) {
	if (read) {
	    err = ardp -> dv_frerr;
	} else {
	    err = ardp -> dv_fwerr;
	}
	if (err == ioerr) {
	    nofmt = TRUE;
	}
    }	
    DBUG_PRINT ("fmt", ("unformatted flag %d", nofmt));
    DBUG_RETURN (nofmt);
}


/*
 *  FUNCTION
 *
 *	write_protect    test for error due to media write_protect
 *
 *  SYNOPSIS
 *
 *	BOOLEAN write_protect (iobytes, ioerr, read)
 *	int iobytes;
 *	int ioerr;
 *	BOOLEAN read;
 *
 *  DESCRIPTION
 *
 *	Uses information about number of bytes transferred on
 *	last I/O operation, error returned by operation, and
 *	whether operation was read or write, to determine if
 *	the media was write_protected.  As with the end of
 *	device test, this test may not always be accurate.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin write_protect
 *	    Default is FALSE
 *	    If operation returned error then
 *		If not reading then
 *		    Get write_protected error expected
 *		    If error was the expected error then
 *		        Result is TRUE
 *		    End if
 *		End if
 *	    End if
 *	    Return result
 *	End write_protect
 *
 */

BOOLEAN write_protect (iobytes, ioerr, read)
int iobytes;
int ioerr;
BOOLEAN read;
{
    register BOOLEAN wprot;
    register int err;

    DBUG_ENTER ("write_protect");
    DBUG_PRINT ("wprot", ("ioerr %d", ioerr));
    wprot = FALSE;
    if (iobytes <= 0 && ardp != NULL) {
	if (!read) {
	    err = ardp -> dv_wperr;
	    if (err == ioerr) {
	        wprot = TRUE;
	    }
	}
    }	
    DBUG_PRINT ("wprot", ("write_protected flag %d", wprot));
    DBUG_RETURN (wprot);
}


/*
 *  FUNCTION
 *
 *	seekable    test to see if file is seekable
 *
 *  SYNOPSIS
 *
 *	BOOLEAN seekable (fname, increment)
 *	char *fname;
 *	int increment;
 *
 *  DESCRIPTION
 *
 *	Tests to see if the specified file is seekable to at least
 *	the given increment.  If so, then seeks may be used instead
 *	of reads, to skip over blocks of the archive.  This speeds
 *	things up considerably.
 *
 *	If the device is unknown, then it is also checked to see if
 *	it is an existing normal (regular) disk file.  If so, then it is
 *	seekable by default.
 *
 *	Returns TRUE if seekable to the given increment.
 *
 */

BOOLEAN seekable (fname, increment)
char *fname;
int increment;
{
    register BOOLEAN seekok;

    DBUG_ENTER ("seekable");
    DBUG_PRINT ("seek", ("check file '%s' for seekability", fname));
    seekok = FALSE;
    if (ardp != NULL) {
	if (ardp -> dv_seek > 0 && ardp -> dv_seek <= increment) {
	    seekok = TRUE;
	}
    } else {
	seekok = regular_file (fname);
    }
    DBUG_PRINT ("seek", ("seekable returns logical value %d", seekok));
    DBUG_RETURN (seekok);
}


/*
 *  FUNCTION
 *
 *	raw_tape    test for raw tape as device
 *
 *  SYNOPSIS
 *
 *	BOOLEAN raw_tape ()
 *
 *  DESCRIPTION
 *
 *	Uses information from the archive device table to
 *	determine if the current archive device is a raw
 *	magnetic tape drive.
 *
 */

BOOLEAN raw_tape ()
{
    register BOOLEAN raw;

    DBUG_ENTER ("raw_tape");
    if (ardp != NULL && (ardp -> dv_flags & D_RAWTAPE)) {
	raw = TRUE;
    } else {
	raw = FALSE;
    }	
    DBUG_PRINT ("raw", ("raw_tape flag %d", raw));
    DBUG_RETURN (raw);
}

