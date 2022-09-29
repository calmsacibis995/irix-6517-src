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
 *	usage.c    internal help stuff
 *
 *  SCCS
 *
 *	@(#)usage.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines for printing internal help or usage
 *	information.
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
#include "dbug.h"		/* Macro based debugger */
#include "manifest.h"		/* Manifest constants */
#include "bruinfo.h"		/* Invocation information */
#include "devices.h"		/* Device table structure */
#include "macros.h"		/* Useful macros */
#include "finfo.h"		/* File information structure */
#include "config.h"		/* for TERMINAL */

/*
 *	External bru functions
 */

extern VOID s_fflush ();	/* Invoke the library fflush function */
extern int s_fprintf ();	/* Invoke the library fprintf function */
extern int s_strlen ();		/* Find length of a string */

extern struct finfo afile;	/* The archive file info struct */

/*
 *	External bru variables.
 */

extern char mode;		/* Current mode */
extern struct bru_info info;	/* Invocation information */
extern int release;		/* Major release number */
extern int level;		/* Minor release level number */
extern int variant;		/* Variant of bru */
extern char *id;		/* Bru id */
extern UINT bufsize;		/* Archive read/write buffer size */
extern ULONG msize;		/* Size of archive media */
extern FILE *logfp;		/* Log file */


/*
 *	The following is the internal documentation which gets printed
 *	when invoked with the -h flag.
 */

static char *documentation[] = {
    "",
    "NAME",
    "",
    "\tbru -- backup and restore utility",
    "",
    "SYNOPSIS",
    "",
    "\tbru  mode  [control options] [selection options]  file(s)",
    "",
    "MODE",
    "",
    "\t-c         create a new archive with specified files",
    "\t-d         find differences between archived files and current files",
    "\t-e         estimate media requirements for create mode",
    "\t-g         give only information from archive header",
    "\t-h         print this help information",
    "\t-i         inspect archive for consistency and data integrity",
    "\t-t         list archive table of contents for files",
    "\t-x         extract named files from archive",
    "",
    "CONTROL OPTIONS",
    "",
#ifdef DBUG
    "\t-# str     use debugging control string str",
#endif
    "\t-a         do not reset file access times after reads",
#ifndef sgi
    "\t-A flags   Commodore Amiga specific flags:",
    "\t           c   clear file archived bit after processing",
    "\t           i   ignore file archived bit for selecting files",
    "\t           r   reject files that have archived bit set",
    "\t           s   set file archived bit after processing",
#endif
    "\t-b N[k|b]  set archive buffer size to N bytes [kbytes|blks]",
    "\t-B         background mode, no interaction with operator",
    "\t-C         always chown extracted files to the user's uid/gid",
    "\t-D         on some systems, provides speedup via double buffering",
    "\t-f file    use specified file as archive ('-' for stdin/stdout)",
    "\t-F         fast mode, no checksum computations or checking",
    "\t-K         allow archiving of files > 2GB in size, if compressed < 2GB",
    "\t-L str     label archive with given string (63 char max)",
    "\t-l         suppress warnings about unresolved links",
    "\t-m         limit directory expansions to same mounted filesystem",
    "\t-p         pass over archive files by reading rather than seeking",
    "\t-R         exclude remotely mounted files for NFS/RFS systems",
    "\t-s N[k|b]  specify size of archive media in bytes [kbytes|blks]",
    "\t-v         enable verbose mode (-vv and -vvv for more verbosity)",
    "\t-w         display action to be taken and wait for confirmation",
    "\t-Z         use 12-bit LZW compression on archived files",
    "",
    "FILE SELECTION OPTIONS",
    "",
    "\t-n date    select files modified since date",
    "\t           EX: 14-Apr-84,15:24:00",
    "\t-o user    select files owned by user,",
    "\t           user may be name, numeric, or file owned by user",
    "\t-u bcdlpr  use selected files in given class; block special,",
    "\t           character special, directory, symbolic link,",
    "\t           fifo (named pipe), or regular file,",
    "\t           regardless of modification dates",
    "",
    NULL
};

/* Don't put the options we don't support in the brief message; at one
 * time they were in brief, but not long! */
static char *quickdoc[] = {
    "usage:  bru -cdeghitx [-"
#ifdef DBUG
	"#"
#endif
#ifndef sgi
	"A"
#endif
	"aBbCfFLlmnopRsuvw] file(s)...",
    "bru -h for help",
    NULL
};

static char *fmt1 = "\t%-16s";		/* Multi-use format string */


/*
 *	Copyright for evaluation copies.
 */

static char *cpyright [] = {
#if COPYRIGHT
    "***********************************",
    "*                                 *",
    "*  Copyright (c) 1987, Fred Fish  *",
    "*       All Rights Reserved       *",
    "*                                 *",
#ifdef amiga
    "*         Amiga Beta Copy.        *",
    "* For Registered Beta Users Only. *",
#else
    "*         Evaluation copy.        *",
    "*      For in-house use only.     *",
#endif
#if AUX
    "*                                 *",
    "*  Not to be distributed outside  *",
    "*  Apple Computer Inc, 10500 N.   *",
    "*  DeAnza Blvd, Cupertino, CA.    *",
#endif
    "*                                 *",
    "***********************************",
#endif
    NULL,
};


/*
 *  FUNCTION
 *
 *	usage    give usage information
 *
 *  SYNOPSIS
 *
 *	VOID usage (type)
 *	int type;
 *
 *  DESCRIPTION
 *
 *	Gives specified type of usage information.  Type is
 *	usually either "BRIEF" or "VERBOSE".
 *
 */

VOID usage (type)
int type;
{
    register char **dp;
    register FILE *fp;

    DBUG_ENTER ("usage");
    mode = 'h';
    if (type == VERBOSE) {
	fp = logfp;
	dp = documentation;
    } else {
	fp = stderr;
	dp = quickdoc;
    }
    while (*dp) {
	(VOID) s_fprintf (fp,"%s\n",*dp++);
    }
    if (type == VERBOSE) {
	(VOID) s_fprintf (fp, "DEFAULTS:\n\n");
	(VOID) s_fprintf (fp, fmt1, "release:");
	(VOID) s_fprintf (fp, "%d.%d\n", release, level);
	(VOID) s_fprintf (fp, fmt1, "variant:");
	(VOID) s_fprintf (fp, "%d\n", variant);
	(VOID) s_fprintf (fp, fmt1, "bru id:");
	(VOID) s_fprintf (fp, "%s\n", id);
	(VOID) s_fprintf (fp, fmt1, "config:");
	(VOID) s_fprintf (fp, "%s\n", CONFIG_DATE);
	(VOID) s_fprintf (fp, fmt1, "archive:");
	(VOID) s_fprintf (fp, "%s\n", afile.fname);
	(VOID) s_fprintf (fp, fmt1, "media size:");
	if (msize == 0L) {
	    (VOID) s_fprintf (fp, "<unknown>\n");
	} else {
	    (VOID) s_fprintf (fp, "%ldk bytes usable\n", B2KB (msize));
	}
	(VOID) s_fprintf (fp, fmt1, "buffer size:");
	(VOID) s_fprintf (fp, "%uk bytes\n", (UINT) bufsize/1024);
	(VOID) s_fprintf (fp, fmt1, "terminal:");
	/* bru_tty not usually set if just doing bru -h */
	(VOID) s_fprintf (fp, "%s\n", info.bru_tty?info.bru_tty:TERMINAL);
	dp = cpyright;
	(VOID) s_fprintf (fp, "\n\n");
	while (*dp) {
	    (VOID) s_fprintf (fp,"\t\t\t%s\n",*dp++);
	}
    }
    s_fflush (fp);
    DBUG_VOID_RETURN;
}
