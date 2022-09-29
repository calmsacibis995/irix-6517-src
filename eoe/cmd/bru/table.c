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
 *	table.c    functions for doing table of contents
 *
 *  SCCS
 *
 *	@(#)table.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains those functions which are solely for doing the
 *	table of contents of a bru archive.
 *
 */

#include "autoconfig.h"

#include <stdio.h>

#if (unix || xenix)
#  include <sys/types.h>
#  include <sys/stat.h>
#  if BSD4_2
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#else
#  include "sys.h"
#endif

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "config.h"		/* Configuration information */
#include "blocks.h"		/* Archive structures */
#include "macros.h"		/* Useful macros */
#include "trees.h"		/* Tree types */
#include "finfo.h"		/* File information structure */
#include "flags.h"		/* Command line flags */
#include "bruinfo.h"		/* Invocation information */


/*
 *	External bru functions.
 */

extern int s_fprintf ();	/* Formatted print */
extern VOID s_fflush ();	/* Invoke fflush function */
extern BOOLEAN confirmed ();	/* Confirm action */
extern VOID filemode ();	/* Decode file mode word */
extern VOID scan ();		/* Invoke function on each archive file */
extern VOID ar_close ();	/* Flush buffers and close the archive */
extern VOID ar_open ();		/* Open the archive */
extern VOID reload ();		/* Reload first volume for rescan */
extern char *ur_gname ();	/* Translate uid to name */
extern char *gp_gname ();	/* Translate gid to name */
extern char *namelink ();	/* Decode conditional and normal symbolic links */
extern struct tm *s_localtime ();
extern char *s_asctime ();	/* Convert tm structure to ascii string */
extern char *getlinkname();
extern char *add_link();

/*
 *	External bru variables.
 */

extern char mode;		/* Current mode (citdxh) */
extern struct bru_info info;	/* Current bru invocation info */
extern struct cmd_flags flags;	/* Flags from command line */
extern FILE *logfp;		/* Verbosity stream */

/*
 *	Local functions.
 */

static VOID tfile ();		/* Print stat buffer */
static VOID pentry ();		/* Print an archive file entry line */


/*
 *  NAME
 *
 *	table    main entry point for table of contents
 *
 *  SYNOPSIS
 *
 *	VOID table ()
 *
 *  DESCRIPTION
 *
 *	Performs table of contents for the selected archive.
 *	This routine is called after all initialization has been
 *	performed.  It sets the current mode to 't', for table,
 *	opens the archive, scans the archive printing table
 *	of contents, closes the archive, and returns to
 *	the startup code.
 *
 */

VOID table ()
{
    DBUG_ENTER ("table");
    mode = 't';
    reload ("table of contents");
    ar_open ();
    scan (tfile);
    ar_close ();
    DBUG_VOID_RETURN;
}


/*
 *  NAME
 *
 *	tfile    print contents of stat buffer
 *
 *  SYNOPSIS
 *
 *	static VOID tfile (blkp, fip)
 *	register union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Prints a header entry using contents of stat buffer
 *	and header block.
 *
 *	Note that the confirmation flag usage doesn't really provide
 *	any useful protection here.  It is used simply for completeness,
 *	since every other mode which accesses an archive also uses it.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin tfile
 *	    If explicitly or implicitly named then
 *		If action confirmed by user then
 *		    Print this entry
 *		End if
 *	    End if
 *	End tfile
 *
 */

static VOID tfile (blkp, fip)
register union blk *blkp;
register struct finfo *fip;
{
    DBUG_ENTER ("tfile");
    if (IS_LEAF (fip) || IS_EXTENSION (fip)) {
	/* symbolic links that are hard linked to other symbolic links we
	 * have already seen are treated as just links.  SGI seems to
	 * be one of the few vendors that even allows this; the only way
	 * make this work (and remain compatible with older versions of
	 * bru both directions) on list and extract is to build the link
	 * table, same as on create and estimate... */
	char *savelinkname;
	savelinkname = fip->lname;
	if(!IS_FLNK(fip->statp->st_mode) || (fip->statp->st_nlink>1 &&
	    getlinkname(fip))) {
	    fip->lname = NULL;
	}
	if(!IS_DIR(fip->statp->st_mode) && fip -> statp -> st_nlink > 1) {
	    fip->lname = add_link (fip);
	    if(!fip->lname && IS_FLNK(fip->statp->st_mode))
		fip->lname = savelinkname;
	}

	if (confirmed ("t %s", fip)) {
	    pentry (blkp, fip);
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	pentry    print archive entry information
 *
 *  SYNOPSIS
 *
 *	static VOID pentry (blkp, fip)
 *	register union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Once the decision has been made to print entry information
 *	for the current archive file, this routine is called to
 *	do the actual printing.
 *
 *	If the file was archived in compressed form, and the
 *	command line flag for compression is given, the file
 *	is listed in it's compressed form (name and size) rather
 *	than the original uncompressed form.
 *
 *  NOTES
 *
 *	This routine once used a line of the form:
 *
 *		s_fprintf (logfp, fip -> fname);
 *
 *	to print the file name, resulting in an obscure bug when the
 *	file name contained printf control characters (consider
 *	"/tmp/funny%sname").  The moral is, if the string being printed
 *	comes from external input, never pass it directly to print for
 *	printing.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin pentry
 *	    If verbose form of table of contents then
 *		Get the mode of the file
 *		Tranlate mode from internal format to string
 *		Get invocation time structure
 *		Remember the year of current bru invocation
 *		Get file modification time structure
 *		Tranlate modification time to string
 *		Print the mode string
 *		Print number of links to the file
 *		Translate the uid and print result
 *		Translate the gid and print result
 *		If file is block or character special file then
 *		    Get major/minor device numbers
 *		    Print major/minor device numbers
 *		Else
 *		    Print the file size
 *		End if
 *		Print month and day of modification
 *		If modified this year then
 *		    Print time of modification
 *		Else
 *		    Print year of modification
 *		End if
 *	    End if
 *	    Print the file name
 *	    If verbose listing and linked to another file then
 *		Print name of file linked to
 *	    End if
 *	    If verbose listing and archived on an Amiga then
 *		If file had a filenote attached then
 *		    Print the filenote in parenthesis
 *		Endif
 *	    Endif
 *	    Terminate the line
 *	End pentry
 *
 */


static VOID pentry (blkp, fip)
register union blk *blkp;
register struct finfo *fip;
{
    char modestr[MODESZ+1];			/* Pointer to mode string */
    char *mtime;				/* Pointer to time string */
    int year_now;				/* Current year */
    register int fst_mode;			/* Mode of file */
    auto dev_t rdev;				/* Major/minor device no */
    auto struct tm *tmp;			/* Pointer to tm struct */
    register char *name;			/* Temporary pointer */
    char *tmpname;

    DBUG_ENTER ("pentry");
    if (flags.vflag > 0) {
	fst_mode = fip -> statp -> st_mode;
	filemode (modestr, fst_mode);
	tmp = s_localtime (&info.bru_time);
	year_now = tmp -> tm_year;
	tmp = s_localtime ((long *) &fip -> statp -> st_mtime);
	mtime = s_asctime (tmp);
	(VOID) s_fprintf (logfp, modestr);
	(VOID) s_fprintf (logfp, "%4d", fip -> statp -> st_nlink);
	name = ur_gname ((uid_t)(fip -> statp -> st_uid));
	(VOID) s_fprintf (logfp, " %-9.8s", name);
	name = gp_gname ((gid_t)(fip -> statp -> st_gid));
	(VOID) s_fprintf (logfp, "%-9.8s", name);
	if (IS_BSPEC (fst_mode) || IS_CSPEC (fst_mode)) {
	    rdev = fip -> statp -> st_rdev;
	    (VOID) s_fprintf (logfp, "%3d,%3d", (rdev & 0xFF00) >> 8, rdev & 0xFF);
	} else {
	    if (flags.Zflag && IS_COMPRESSED (fip)) {
		s_fprintf (logfp, "%13ld", fip -> zsize);
	    } else {
		/*
		 * Added for xfs
		 */
		if (fip -> statp -> st_size == -1)
			s_fprintf (logfp, "%13s","Very Big");
		else
			s_fprintf (logfp, "%13lld", fip -> statp -> st_size);
	    }
	}
	(VOID) s_fprintf (logfp, " %-6.6s ", &mtime[4]);
	DBUG_PRINT ("year", ("current year = %d", year_now));
	DBUG_PRINT ("year", ("file year = %d", tmp -> tm_year));
	if (tmp -> tm_year == year_now) {
	    (VOID) s_fprintf (logfp, "%5.5s ", &mtime[11]);
	} else {
	    (VOID) s_fprintf (logfp, " %4.4s ", &mtime[20]);
	}
    }
    if (flags.Zflag && IS_COMPRESSED (fip)) {
	s_fprintf (logfp, "%s.Z", fip -> fname);  /* CAUTION (see NOTES above) */
    } else {
	s_fprintf (logfp, "%s", fip -> fname);  /* CAUTION (see NOTES above) */
    }
    if (flags.vflag > 1 && IS_FLNK (fip -> statp -> st_mode) &&
	    (fip->statp->st_nlink==1 || !(tmpname=getlinkname(fip)) ||
	    !strcmp(fip->fname, tmpname))) {
	/* print as hardlink if this is a hardlink to a symlink and not the
	 * first instance! */
	(VOID) s_fprintf (logfp, " symbolic link to %s", namelink (blkp -> FH.f_lname));
    } else if (flags.vflag > 1 && LINKED (blkp)) {
	(VOID) s_fprintf (logfp, " linked to %s", blkp -> FH.f_lname);
    }
    if ((flags.vflag > 1) && (fip -> fi_flags & FI_AMIGA)) {
	if (fip -> fib_Comment[0] != EOS) {
	    (VOID) s_fprintf (logfp, " (%s)", fip -> fib_Comment);
	}
    }
    (VOID) s_fprintf (logfp, "\n");
    s_fflush (logfp);
    DBUG_VOID_RETURN;
}
