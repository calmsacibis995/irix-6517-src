/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			    All Rights Reserved				*
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
 *	bru.c    mainline routines for bru utility
 *
 *  SCCS
 *
 *	@(#)bru.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Bru (Backup and Restore Utility) is a file system
 *	backup utility with roots in the Unix system "tar"
 *	utility.  It started out to be a complete rewrite
 *	of "tar" with significant enhancements, but once the
 *	subtle (and not so subtle) limitations of "tar" were
 *	discovered, the author decided to make a clean break with
 *	the past and write a significantly more capable utility.
 *	For example, some of bru's capabilities are:
 *
 *		(1)	Archives can span multiple volumes,
 *			more than a single floppy or tape
 *
 *		(2)	When possible, seeks are used to
 *			skip over archived files.  This
 *			greatly improves the performance
 *			when doing a table of contents or
 *			extraction of specific files
 *
 *		(3)	Improved tolerance to archive
 *			corruption or read errors.
 *
 *		(4)	Can recreate directories, character
 *			special files, block special files,
 *			and other such special files with the
 *			correct attributes.
 *
 *		(5)	Compare archive with existing disk
 *			files for differences.
 *
 *	Users familiar with tar will have no problem using bru since
 *	bru's capabilities are basically a superset of tar's.  However,
 *	the archive formats are incompatible thus bru cannot create or
 *	read tar archives and vice versa.
 *
 *  AUTHOR
 *
 *	Fred Fish
 *	Tempe, Arizona
 *
 */


#include "autoconfig.h"

#include <stdio.h>

#if unix || xenix
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  include "sys.h"		/* Header file to fake unix stuff */
#endif

#include "typedefs.h"		/* Local types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "finfo.h"		/* File information structure */
#include "flags.h"		/* Command line flags */
#include "config.h"
 
/*
 *	External bru functions.
 */

extern VOID create ();		/* Mainline for create new archive */
extern VOID diff ();		/* Mainline for diff function */
extern VOID done ();		/* Finish up and exit */
extern VOID estimate ();	/* Estimate media requirements */
extern VOID extract ();		/* Mainline for extract function */
extern VOID finfo_init ();	/* Initialize a finfo structure */
extern VOID info_only ();	/* Mainline to give only header info */
extern VOID init ();		/* Initialize process */
extern VOID inspect ();		/* Mainline for the inspect function */
extern VOID table ();		/* Mainline for table function */
extern VOID usage ();		/* Mainline for usage */

/*
 *	External bru variables/structures.
 */

extern struct cmd_flags flags;	/* Flags given on command line */
extern struct finfo afile;	/* The archive file info struct */


/*
 *  FUNCTION
 *
 *	main    entry point for bru application code
 *
 *  SYNOPSIS
 *
 *	main (argc, argv)
 *	int argc;
 *	char *argv[];
 *
 *  DESCRIPTION
 *
 *	Main entry point for the bru utility.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin main
 *	    Initialize the archive file info structure
 *	    Do initialization
 *	    If estimating media requirements then
 *		Make estimate of media usage
 *	    End if
 *	    If creating an archive then
 *		Create the archive
 *	    End if
 *	    If inspecting an archive then
 *		Inspect the archive
 *	    End if
 *	    If doing table of contents then
 *		Do table of contents
 *	    End if
 *	    If doing extract then
 *		Extract the files
 *	    End if
 *	    If doing differences then
 *		Do differences
 *	    End if
 *	    If doing help then
 *		Do help
 *	    End if
 *	    Clean up and exit
 *	End main
 *
 */


int main (argc, argv)
int argc;
char **argv;
{
    auto char arnamebuf[NAMESIZE];

    DBUG_ENTER ("main");
    DBUG_PROCESS (argv[0]);
    finfo_init (&afile, arnamebuf, (struct stat64 *) NULL);
    init (argc, argv);
    if (flags.eflag) {
	estimate ();
    }
    if (flags.cflag) {
	create ();
    }
    if (flags.iflag) {
	inspect ();
    }
    if (flags.tflag) {
	table ();
    }
    if (flags.xflag) {
	extract ();
    }
    if (flags.dflag > 0) {
	diff ();
    }
    if (flags.gflag) {
	info_only ();
    }
#ifndef sgi
    if (flags.hflag) {
	usage (VERBOSE);
    }
#endif
    done ();
    DBUG_RETURN (0);
}
