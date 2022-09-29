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
 *	done.c    routines for cleaning up and exiting
 *
 *  SCCS
 *
 *	@(#)done.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines for graceful exit by cleaning up and
 *	then calling exit with appropriate status.
 *
 *	Also is responsible for printing an execution summary
 *	if requested.
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
#include "bruinfo.h"
#include "exeinfo.h"
#include "flags.h"
#include "finfo.h"


/*
 *	External bru functions
 */

extern VOID s_exit ();		/* Terminate process */
extern int s_fprintf ();	/* Invoke the fprintf function */
extern VOID s_fflush ();	/* Invoke the fflush function */
extern int s_close ();		/* Invoke library file close function */

/*
 *	External bru variables.
 */

extern FILE *logfp;		/* Verbosity stream */
extern struct bru_info info;	/* Invocation information */
extern struct exe_info einfo;	/* Execution information */
extern struct cmd_flags flags;	/* Command line flags */
extern struct finfo afile;	/* The archive file info struct */

/*
 *	Local functions.
 */

static VOID summary ();		/* Print execution summary */


/*
 *  FUNCTION
 *
 *	done    clean up and exit with status
 *
 *  SYNOPSIS
 *
 *	VOID done ()
 *
 *  DESCRIPTION
 *
 *	Unlinks the temporary file and exits with meaningful status.
 *
 */

VOID done ()
{
    register int exitstatus;
#if HAVE_SHM
    extern void dbl_done ();
#endif

    DBUG_ENTER ("done");
    if (flags.vflag > 3) {
	summary ();
    }
    if (einfo.errors > 0) {
	exitstatus = 2;
    } else if (einfo.warnings > 0) {
	exitstatus = 1;
    } else {
	exitstatus = 0;
    }
#ifdef amiga
    s_close (afile.fildes);
#endif
#if HAVE_SHM
    dbl_done ();
#endif
    s_exit (exitstatus);
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	summary    print execution summary info
 *
 *  SYNOPSIS
 *
 *	static VOID summary ()
 *
 *  DESCRIPTION
 *
 *	Prints a rather verbose execution summary containing such
 *	information as number of archive blocks read or written,
 *	total number of errors or warnings, soft or hard read or
 *	write errors, total blocks with checksum errors, etc.
 *
 */

static VOID summary ()
{
    DBUG_ENTER ("summary");
    (VOID) s_fprintf (logfp, "\n**** %s: execution summary ****\n\n", info.bru_name);
    (VOID) s_fprintf (logfp, "Messages:\t\t%d warnings,  %d errors\n",
		einfo.warnings, einfo.errors);
    (VOID) s_fprintf (logfp, "Archive I/O:\t\t%ld blocks written,  %ld blocks read\n",
		einfo.bwrites, einfo.breads);
    (VOID) s_fprintf (logfp, "Read errors:\t\t%d soft,  %d hard\n",
		einfo.rsoft, einfo.rhard);
    (VOID) s_fprintf (logfp, "Write errors:\t\t%d soft,  %d hard\n",
		einfo.wsoft, einfo.whard);
    (VOID) s_fprintf (logfp, "Checksum errors:\t%d\n", einfo.chkerr);
    s_fflush (logfp);
    DBUG_VOID_RETURN;
}
