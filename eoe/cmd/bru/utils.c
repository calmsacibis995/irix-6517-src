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
 *	utils.c    general utility routines
 *
 *  SCCS
 *
 *	@(#)utils.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines which are general in nature and didn't
 *	seem to fit in any other file.
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
#  include "sys.h"
#endif

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "config.h"		/* Configuration file */
#include "errors.h"		/* Error codes */
#include "blocks.h"		/* Archive structures */
#include "macros.h"		/* Useful macros */
#include "finfo.h"		/* File information structure */
#include "flags.h"		/* Command line flags */
#include "bruinfo.h"		/* Invocation information */

#if (unix || xenix)
#define TEMPLATE "%s/%s"
#else
#define TEMPLATE "%s%s"
#endif


/*
 *	External bru functions.
 */

extern S32BIT s_lseek ();	/* Seek to location in file */
extern BOOLEAN execute ();	/* Execute a child process */
extern int ar_vol ();		/* Find current volume number */
extern LBA ar_tell ();		/* Give current block number */
extern BOOLEAN file_access ();	/* Test file for access */
extern BOOLEAN new_arfile ();	/* Load a new volume */
extern VOID bru_error ();	/* Report an error to user */
extern VOID done ();		/* Wrapup and exit */
extern char *s_getenv ();	/* Get an environment variable */
extern int s_link ();		/* Link files */
extern VOID s_fflush ();	/* Invoke library fflush function */
extern int s_chown ();		/* Invoke library change owner func */
extern int s_utime ();		/* Invoke library function utime */
extern char *s_malloc ();	/* Allocate memory */
extern BOOLEAN iscondlink ();	/* Decide if have a conditional sym link */
extern char *univlink ();	/* Return the proper file for a non pyramid */
extern char *namelink ();	/* Print what a cond link points to */
extern int s_mkdir ();		/* System call to make a new directory */
extern char *getlinkname();

#if HAVE_SYMLINKS
extern int s_readlink ();	/* Invoke library read symbolic link func */
extern int s_symlink ();	/* Invoke library make symbolic link func */
#endif

#if pyr
extern int s_csymlink ();	/* Invoke library make cond sym link func */
#endif

/*
 *	External bru variables.
 */

extern time_t ntime;		/* Time for -n option */
extern uid_t uid;		/* User ID derived via -o option */
extern struct cmd_flags flags;	/* Command line flags */
extern char mode;		/* Current execution mode */
extern struct bru_info info;	/* Current invocation information */
extern FILE *logfp;		/* Verbosity messages */
extern char* working_dir;	/* pathname of current working dir */

int getsize_err;	/* to distinguish err 0 return from normal 0 */

/*
 *	Functions defined here.
 */

VOID finfo_init ();		/* Initialize a finfo structure */


/*
 *  NAME
 *
 *	eoablk    test block for end of archive marker block
 *
 *  SYNOPSIS
 *
 *	BOOLEAN eoablk (blkp)
 *	register union blk *blkp;
 *
 *  DESCRIPTION
 *
 *	Test block to see if it is the end of archive terminator block.
 *
 *	Note that there is some question about a logical return
 *	value if the pointer is NULL.  It was arbitrarily
 *	decided to return TRUE in this case, as if the end
 *	of the archive was found.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin eoablk
 *	    Set default return to TRUE
 *	    If block pointer is valid then
 *		Test for end of block
 *	    End if
 *	    Return result 
 *	End eoablk
 *
 */

BOOLEAN eoablk (blkp)
register union blk *blkp;
{
    register BOOLEAN result;		/* Result of test */

    DBUG_ENTER ("eoablk");
    result = TRUE;
    if (blkp == NULL) {
	bru_error (ERR_BUG, "eoablk");
    } else {
	result = (FROMHEX (blkp -> HD.h_magic) == T_MAGIC);
    }
    DBUG_RETURN (result);
}


/*
 *  FUNCTION
 *
 *	zeroblk    zero an archive block
 *
 *  SYNOPSIS
 *
 *	VOID zeroblk (blkp)
 *	register union blk *blkp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to an archive block, zeros the block.
 *
 *	At one time, when the block was treated as an array
 *	of BLKSIZE bytes, bru was spending about 10% of it's
 *	time simply zeroing the block one byte at a time.
 *	By going to longs, the time was considerably reduced.
 *	If the preprocessor define HAVE_MEMSET is nonzero then
 *	the library memset routine will be called instead of
 *	zeroblk.
 *
 */

#if !HAVE_MEMSET

VOID zeroblk (blkp)
register union blk *blkp;
{
    register long *lp;

    DBUG_ENTER ("zeroblk");
    if (blkp == NULL) {
	bru_error (ERR_BUG, "zeroblk");
    } else {
	lp = blkp -> longs;
	while (lp < OVERRUN (blkp -> longs)) {
	    *lp++ = 0L;
	}
    }
    DBUG_VOID_RETURN;
}

#endif	/* !HAVE_MEMSET */


/*
 *  FUNCTION
 *
 *	copyname    copy a pathname from one place to another
 *
 *  SYNOPSIS
 *
 *	VOID copyname (out, in)
 *	char *out;
 *	char *in;
 *
 *  DESCRIPTION
 *
 *	Copyname copies a pathname from one location to another,
 *	enforcing the current size limit on pathnames set by
 *	the manifest constant "NAMESIZE" in the configuration file.
 *
 *	If however, the input name is too long, then the output
 *	will be truncated and null terminated such that the
 *	output still fits in an array of size "NAMESIZE". 
 *
 *	If the -Z command line option is given, we reduce the
 *	maximum name length by two characters, so that a ".Z" can
 *	always be tacked on later if necessary, without having
 *	to check again for overflow.
 *
 */

VOID copyname (out, in)
char *out;
char *in;
{
    BOOLEAN copy ();
    int namesize = NAMESIZE;
    
    DBUG_ENTER ("copyname");
    if (out == NULL || in == NULL) {
	bru_error (ERR_BUG, "copyname");
    } else {
	if (flags.Zflag) {
	    namesize -= 2;
	}
	if (!copy (out, in, namesize)) {
	    bru_error (ERR_BIGPATH, in);
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	mklink    make a link to a file
 *
 *  SYNOPSIS
 *
 *	BOOLEAN mklink (exists, new)
 *	char *exists;
 *	char *new;
 *
 *  DESCRIPTION
 *
 *	Given pointer to the name of an existing file, and
 *	pointer to the name of a file to link to it, attempts
 *	to make the link, returning TRUE if successful, FALSE
 *	otherwise.
 *
 */

BOOLEAN mklink (exists, new)
char *exists;
char *new;
{
    register int rtnval;

    if (s_link (exists, new) == SYS_ERROR) {
	bru_error (ERR_MKLINK, new, exists);
	rtnval = FALSE;
    } else {
	rtnval = TRUE;
    }
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	mksymlink    make a symbolic link to a file
 *
 *  SYNOPSIS
 *
 *	BOOLEAN mksymlink (exists, new)
 *	char *exists;
 *	char *new;
 *
 *  DESCRIPTION
 *
 *	Given pointer to the name of an existing file, and
 *	pointer to the name of a file to link to it, attempts
 *	to make the symbolic link, returning TRUE if successful, FALSE
 *	otherwise.
 *
 *	This is where we handle Pyramid style conditional symbolic
 *	links, and do hard links on a non-4.2BSD system.  On a non-4.2
 *	system, be sure to do a hard link only if the file to be linked
 *	to exists, and is not a directory.
 *
 */


/*
 *   PSEUDO CODE
 *
 *	Begin mksymlink
 *	    Initialize return value to TRUE
 *	    If on a pyramid then
 *		set linking function to be csymlink
 *	    Else if on a 4.2 system then
 *		get currect pathname
 *		set linking function to be symlink
 *	    Else
 *		get currect pathname
 *		set linking function to be link
 *		If file to be linked to exists then
 *		    If it is directory then
 *			return value = FALSE
 *		    End if
 *		Else
 *		    return value = FALSE
 *		End if
 *	    End if
 *	    If return value still TRUE and linking succeeds then
 *		return value = TRUE
 *	    Else
 *		return value = FALSE
 *	    End if
 *	End mksymlink
 */


BOOLEAN mksymlink (exists, new)
char *exists;
char *new;
{
    register int rtnval = TRUE;
    int (*linkfile)();
#if !HAVE_SYMLINKS && !pyr
    struct finfo file;
    struct stat64 sbuf;
#endif

    DBUG_ENTER ("mksymlink");
    DBUG_PRINT ("mksymlink", ("exists %s  new %s", exists, new));

#if pyr
    DBUG_PRINT ("symlink", ("on a pyramid"));
    linkfile = s_csymlink;
#else
    exists = univlink (exists);
#if HAVE_SYMLINKS
    DBUG_PRINT ("symlink", ("under 4.2"));
    linkfile = s_symlink;
#else
    DBUG_PRINT ("symlink", ("under System V"));
    linkfile = s_link;
    if (file_access (exists, A_EXISTS, FALSE)) {
	finfo_init (&file, exists, &sbuf);
	if (file_stat (&file) && IS_DIR (sbuf.st_mode)) {
	    bru_error (ERR_SYMTODIR, new, exists, exists);
	    rtnval = FALSE;
	}
    } else {
	bru_error (ERR_HARDLINK, new, exists, exists);
	rtnval = FALSE;
    }
    DBUG_PRINT ("symlink", ("will try hard link of %s to %s", exists, new));
#endif
#endif
    if (rtnval && (*linkfile)(exists, new) != SYS_ERROR) {
	rtnval = TRUE;
    } else {
	rtnval = FALSE;
    }

    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	regular_file    see if a file is not a special file
 *
 *  SYNOPSIS
 *
 *	BOOLEAN regular_file (fname)
 *	char *fname;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a file name, check to see if it is a regular file.
 *	Returns TRUE if the file exists and the stat bits indicate that 
 *	the file is a regular file.  If the file is a special file, or does
 *	not exist, then returns FALSE.
 *
 */

BOOLEAN regular_file (fname)
char *fname;
{
    register int rtnval = FALSE;
    auto struct stat64 statbuf;

    DBUG_ENTER ("regular_file");
    DBUG_PRINT ("regfile", ("test '%s' for existing, regular file", fname));
    if (s_stat (fname, &statbuf) != SYS_ERROR) {
	if (IS_REG (statbuf.st_mode)) {
	    rtnval = TRUE;
	}
    }
    DBUG_PRINT ("regfile", ("returns logical value %d", rtnval));
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	file_stat    do a stat on a file
 *
 *  SYNOPSIS
 *
 *	BOOLEAN file_stat (fip)
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a file information structure, attempts
 *	to stat the file to initialize the file information
 *	stat buffer and any machine dependent fields in the
 *	file information structure.
 *
 *	Returns TRUE if successful, FALSE otherwise.
 *
 */

BOOLEAN file_stat (fip)
register struct finfo *fip;
{
    register int rtnval;
#if HAVE_SYMLINKS
    register int nmlen;
    static char linkbuf[NAMESIZE];
#endif

    DBUG_ENTER ("file_stat");
    DBUG_PRINT ("stat", ("stat %s", fip -> fname));
    if (s_stat (fip -> fname, fip -> statp) == SYS_ERROR) {
	bru_error (ERR_STAT, fip -> fname);
	rtnval = FALSE;
    } else {
	rtnval = TRUE;
#if HAVE_SYMLINKS
	if (IS_FLNK (fip -> statp -> st_mode)) {
	    nmlen = s_readlink (fip -> fname, linkbuf, sizeof linkbuf);
	    if (nmlen == SYS_ERROR) {
		bru_error (ERR_RDLINK, fip -> fname);
		rtnval = FALSE;
	    } else {
		linkbuf[nmlen] = EOS;
		fip -> lname = linkbuf;
		DBUG_PRINT ("symlink", ("symlink is %s", fip -> lname));
	    }
	}
#endif
#ifdef amiga
	(VOID) getinfo (fip);	
#endif
    }
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	verbosity    issue a verbosity message
 *
 *  SYNOPSIS
 *
 *	VOID verbosity (fip)
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a file information structure, issues
 *	a verbosity message concerning current mode and file
 *	name, if verbosity enabled.
 *
 */

VOID verbosity (fip)
register struct finfo *fip;
{
    register LBA blocks;
    register LBA total;
    register ULONG kbytes;
    register BOOLEAN linked;
    register S64BIT zratio = 0LL;
    char *tmpname;

    DBUG_ENTER ("verbosity");
    if (fip == NULL || fip -> fname == NULL || fip -> statp == NULL) {
	bru_error (ERR_BUG, "verbosity");
    } else if (flags.vflag > 0) {
	if (fip -> lname != NULL && *fip -> lname != EOS) {
	    linked = TRUE;
	} else {
	    linked = FALSE;
	}
	(VOID) s_fprintf (logfp, "%c", mode);
	blocks = 1;
	zratio = 0;
	if (!linked && IS_REG (fip -> statp -> st_mode)) {
	    blocks += ZARBLKS (fip);
	    if (IS_COMPRESSED (fip)) {
		zratio = fip -> zsize * 100;		/* overflow possible (not anymore)*/
		zratio /= fip -> statp -> st_size;
		zratio = 100 - zratio;
	    }
	}
	if (flags.vflag > 0 && (fip -> fi_flags & FI_ZFLAG)) {
	    /*
	     * Added for xfs
	     */
	    if (fip -> statp -> st_size == -1)
		    s_fprintf (logfp, " ( \?%%)");
	    else
		    s_fprintf (logfp, " (%2lld%%)", zratio);
	}
	kbytes = KBYTES (blocks);
	(VOID) s_fprintf (logfp, " %6luk", kbytes);
	total = ar_tell () + blocks;
	if (mode == 'c') {
	    total++;
	}
	kbytes = KBYTES (total);
	(VOID) s_fprintf (logfp, " of %7luk", kbytes);
	(VOID) s_fprintf (logfp, " [%d]", ar_vol () + 1);
	if( (flags.Xflag) && (working_dir != NULL) )
	    (VOID) s_fprintf (logfp, " %s/%s", working_dir, fip -> fname);
	else
	    (VOID) s_fprintf (logfp, " %s", fip -> fname);
	/* must test for symlinks first; we print a hardlink to a symlink
	 * as a hardlink if it wasn't the first instance in the archive.
	 * SGI seems to be one of the few vendors that allows hardlinks to
	 * symlinks... */
	if (flags.vflag > 1 && IS_FLNK (fip -> statp -> st_mode) &&
	    (fip->statp->st_nlink==1 || !(tmpname=getlinkname(fip)) ||
	    !strcmp(fip->fname, tmpname))) {
	    (VOID) s_fprintf (logfp, " symbolic link to %s", namelink (fip -> lname));
	} else if (flags.vflag > 1 && linked) {
	    (VOID) s_fprintf (logfp, " linked to %s", fip -> lname);
	}
	(VOID) s_fprintf (logfp, "\n");
	s_fflush (logfp);
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	out_of_date    check for existing file is out of date
 *
 *  SYNOPSIS
 *
 *	BOOLEAN out_of_date (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Checks to see if there is an existing file of the same name
 *	with an earlier modification date.
 *
 *	If there is no existing file of the same name, then the
 *	result is TRUE.
 *
 *	If there is an existing file but the date cannot be determined
 *	for any reason, then the result is FALSE.
 *
 *	If there is an existing file and it has the same or a more recent
 *	modification date then the result is FALSE.
 *
 */

BOOLEAN out_of_date (fip)
struct finfo *fip;
{
    register BOOLEAN result;
    auto struct stat64 sbuf;
    auto struct finfo file;

    DBUG_ENTER ("out_of_date");
    result = FALSE;
    if (fip == NULL || fip -> fname == NULL) {
	bru_error (ERR_BUG, "out_of_date");
    } else {
	if (!file_access (fip -> fname, A_EXISTS, FALSE)) {
	    result = TRUE;
	} else {
	    finfo_init (&file, fip -> fname, &sbuf);
	    if (file_stat (&file)) {
		if (file.statp -> st_mtime < fip -> statp -> st_mtime) {
		    result = TRUE;
		}
	    }
	}
    }
    DBUG_RETURN (result);
}


/*
 *  FUNCTION
 *
 *	reset_times    reset the access and modification times
 *
 *  SYNOPSIS
 *
 *	VOID reset_times (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION 
 *
 *	Resets the access and modification times for the file
 *	according to the values in the stat buffer.
 *
 *	This routine is primarily used to prevent files which
 *	have not been accessed in a long time by any other
 *	program from appearing to have been used recently.
 *
 *	Note there is a possible race condition here in that
 *	the stat buffer is obtained prior to reading the file
 *	and the times are reset after the file is closed.
 *	If during this window, another program accesses or
 *	modifies the same file, the times will still be
 *	reset when bru is through.  It is not likely that
 *	this circumstance will ever cause any problems though.
 *
 *  BUGS
 *
 *	The warning generation is suppressed because it appears
 *	that even if the executable is owned by root and has
 *	SUID bit set, utime will still return with EPERM.
 *	Is this a bug in the Unisoft kernel?
 *
 *  NOTES
 *
 *	Many of the unix sources (tar and cpio as examples) do
 *	NOT use the utime call correctly.  They pass utime a pointer
 *	to the atime member of a stat structure, assuming that the
 *	next member of the stat structure is the mtime member.
 *	There is NO guarantee that atime and mtime are contiguous
 *	in the stat structure, and in fact, this is not true
 *	under 4.2BSD.  Beware!
 *
 */


struct utimbuf {		/* Do it right, as given in User's Manual */
    time_t actime;		/* Do NOT pass pointer to atime in stat */
    time_t modtime;		/* structure instead */
};

VOID reset_times (fip)
struct finfo *fip;
{
    auto struct utimbuf ftime;

    DBUG_ENTER ("reset_times");
    ftime.actime = fip -> statp -> st_atime;
    ftime.modtime = fip -> statp -> st_mtime;
    if (fip==NULL || fip->fname==NULL || *fip->fname==EOS || fip->statp==NULL) {
	bru_error (ERR_BUG, "reset_times");
    } else {
	if (!flags.aflag) {
	    if (s_utime (fip -> fname, &ftime) == SYS_ERROR) {
		/****** bru_error (ERR_STIMES, fip -> fname); ******/
	    }
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	selected    apply selection criteria
 *
 *  SYNOPSIS
 *
 *	BOOLEAN selected (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a file information structure,
 *	applies the selection criteria specified on the command line,
 *	such as date of modification, wildcard filename match, etc.
 *
 */

BOOLEAN selected (fip)
struct finfo *fip;
{
    register BOOLEAN select;

    DBUG_ENTER ("selected");
    select = TRUE;
    if (flags.nflag) {
	select &= fip -> statp -> st_mtime > ntime;
    }
    if (flags.oflag) {
	select &= fip -> statp -> st_uid == uid;
    }
    if ((fip -> fi_flags & FI_AMIGA) && flags.Arflag) {
	select &= !abit_set (fip);
    }
    DBUG_RETURN (select);
}


/*
 *  FUNCTION
 *
 *	getsize    convert an ascii size string to internal long
 *
 *  SYNOPSIS
 *
 *	S32BIT getsize (cp)
 *	char *cp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a string which is presumably an ascii
 *	numeric string with an optional scale factor, converts
 *	the string to an internal long and returns the value.
 *
 */

ULONG getsize (cp)
char *cp;
{
    register ULONG size;
    auto char *scale;

    DBUG_ENTER ("getsize");
    scale = NULL;
    size = strtoul (cp, &scale, 0);
    DBUG_PRINT ("getsize", ("size from strtol is %d", size));
    if (scale == NULL) {
	bru_error (ERR_BUG, "getsize");
	size = 0;	/* paranoia */
	getsize_err = 1;	/* distinguish err 0 return from normal 0 */
    } else if(scale == cp) {
	bru_error (ERR_NOTNUM, cp, "");
	done();
    } else {
	getsize_err = 0;
	switch (*scale) {
	    case 'k':
	    case 'K':
	        size *= 1024;
		break;
	    case 'b':
	    case 'B':
	        size *= 512;
		break;
	    case 'm':
	    case 'M':
		size *= (1024 * 1024);
		break;
	}
    }
    DBUG_RETURN (size);
}

    

/*
 *	The library function "strncpy" is almost what we need but not
 *	quite.
 *
 *	Returns TRUE if copy was successful with no truncation, FALSE
 *	otherwise.  Output string is always null terminated.
 *
 */

BOOLEAN copy (out, in, outsize)
register char *out;
register char *in;
register int outsize;
{
    DBUG_ENTER ("copy");
    DBUG_PRINT ("copy", ("copy %s", in));
    while (--outsize > 0) {
	if (*in == EOS) {
	    break;
	} else {
	    *out++ = *in++;
	}
    }
    *out = EOS;
    DBUG_RETURN (*in == EOS);
}


/*
 *  FUNCTION
 *
 *	unconditional    test for unconditional selection
 *
 *  SYNOPSIS
 *
 *	BOOLEAN unconditional (fip)
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a file information structure, uses
 *	the information in the stat buffer for the file, along
 *	with flags set on the command line, to indicate
 *	if the file is to be unconditionally selected regardless
 *	of its modification date.
 *
 *	Returns TRUE if the file is to be selected, FALSE otherwise.
 *
 */

BOOLEAN unconditional (fip)
register struct finfo *fip;
{
    register BOOLEAN rtnval;
    register ushort stmode;

    DBUG_ENTER ("unconditional");
    rtnval = FALSE;
    if (flags.uflag) {
	stmode = fip -> statp -> st_mode;
	if (IS_BSPEC (stmode)) {
	    rtnval = flags.ubflag;
	} else if (IS_CSPEC (stmode)) {
	    rtnval = flags.ucflag;
	} else if (IS_DIR (stmode)) {
	    rtnval = flags.udflag;
	} else if (IS_FLNK (stmode)) {
	    rtnval = flags.ulflag;
	} else if (IS_FIFO (stmode)) {
	    rtnval = flags.upflag;
	} else if (IS_REG (stmode)) {
	    rtnval = flags.urflag;
	}
    }
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	finfo_init    initialize a file information structure
 *
 *  SYNOPSIS
 *
 *	VOID finfo_init (fip, name, statp)
 *	register struct finfo *fip;
 *	char *name;
 *	struct stat64 *statp;
 *
 *  DESCRIPTION
 *
 *	Most file information structures are allocated on the stack.
 *	Such structures are not guaranteed to be in any particular state,
 *	thus this routine is available to initialize specific fields
 *	and zero the rest.
 *
 */

VOID finfo_init (fip, name, statp)
register struct finfo *fip;
char *name;
struct stat64 *statp;
{
    DBUG_ENTER ("finfo_init");
    fip -> statp = statp;
    fip -> fname = name;
    fip -> lname = NULL;
    fip -> fildes = 0;
    fip -> flba = 0L;
    fip -> chkerrs = 0L;
    fip -> fi_flags = FI_FLAGS_INIT;
    if (flags.Zflag) {
	fip -> fi_flags |= FI_ZFLAG;
    }
    fip -> type = 0;
    fip -> fib_Protection = 0L;
    fip -> fib_Comment[0] = EOS;
    fip -> zsize = 0;
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	swapbytes    swap bytes in an archive block
 *
 *  SYNOPSIS
 *
 *	VOID swapbytes (blkp)
 *	register union blk *blkp;
 *
 *  DESCRIPTION
 *
 *	Swaps each pair of bytes in an archive block.
 */

VOID swapbytes (blkp)
register union blk *blkp;
{
    register char *cs1;
    register char *cs2;
    register char tmp;

    DBUG_ENTER ("swapbytes");
    cs1 = &blkp -> bytes[0];
    cs2 = &blkp -> bytes[1];
    while (cs1 < OVERRUN (blkp -> bytes)) {
	tmp = *cs1;
	*cs1++ = *cs2++;
	*cs1++ = tmp;
	cs2++;
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	swapshorts    swap shorts in an archive block
 *
 *  SYNOPSIS
 *
 *	VOID swapshorts (blkp)
 *	register union blk *blkp;
 *
 *  DESCRIPTION
 *
 *	Swaps each pair of shorts in an archive block.
 */

VOID swapshorts (blkp)
register union blk *blkp;
{
    register short *ss1;
    register short *ss2;
    register short stmp;

    DBUG_ENTER ("swapshorts");
    ss1 = &blkp -> shorts[0];
    ss2 = &blkp -> shorts[1];
    while (ss1 < OVERRUN (blkp -> shorts)) {
	stmp = *ss1;
	*ss1++ = *ss2++;
	*ss1++ = stmp;
	ss2++;
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	get_memory    ask system for more memory
 *
 *  SYNOPSIS
 *
 *	VOID *get_memory (size, quit)
 *	UINT size;
 *	BOOLEAN quit;
 *
 *  DESCRIPTION
 *
 *	Given number of bytes to allocate from system (size),
 *	issues the appropriate system call to get the memory.
 *
 *	Returns pointer to the allocated memory if successful,
 *	NULL otherwise.  Note that any memory allocated is guaranteed
 *	to be properly aligned for any use (see malloc(3C) in UPM).
 *	Get_memory is declared to return type "pointer to void" so
 *	that casting to any other type of object will not give
 *	lint messages about "possible pointer alignment problem".
 *	So instead of numerous bogus warnings, we will get exactly
 *	one lint warning here, for the conversion from (char *) to
 *	(void *).
 *
 *	If the quit flag is set, and no memory can be allocated,
 *	the program will call the routine to clean up and exit.
 *
 *	The memory allocated may be freed with the "free" system
 *	call.
 *
 *	Note that the memory is NOT guaranteed to be zeroed.
 *
 */
 
# ifdef sgi
#	undef VOID
#	define VOID	char
# endif

VOID *get_memory (size, quit)
UINT size;
BOOLEAN quit;
{
    register VOID *new;
    
    new = (VOID *) s_malloc (size);
    if (new == NULL && quit) {
	bru_error (ERR_MALLOC, size);
	done ();
    }
    return (new);
}

# ifdef sgi
#	undef VOID
#	define VOID	void
# endif


/*
 *	In cases of extreme bugs, we can define DUMPBLK, which enables
 *	dumping of the contents of each archive block using the DBUG
 *	macro mechanism.  We don't normally want this, as it results
 *	in overwhelming output...
 */

#ifdef DUMPBLK

do_dump (blkp)
register union blk *blkp;
{
    register char *cp;
    register int count;

    DBUG_ENTER ("do_dump");
    cp = blkp -> bytes;
    count = 0;
    while (cp < &blkp -> bytes[BLKSIZE]) {
	if (*cp < '\040') {
	    (VOID) s_fprintf (DBUG_FILE, "   ^%c", *cp | 0100);
	} else if (*cp < '\177') {
	    (VOID) s_fprintf (DBUG_FILE, "    %c", *cp);
	} else {
	    (VOID) s_fprintf (DBUG_FILE, " \\%3.3o", *cp);
	}
	cp++;
	count++;
	if ((count % 16) == 0) {
	    (VOID) s_fprintf (DBUG_FILE, "\n");
	}
    }
    DBUG_VOID_RETURN;
}

VOID dump_blk (blkp, lba)
register union blk *blkp;
LBA lba;
{
    DBUG_ENTER ("dump_blk");
    DBUG_EXECUTE ("dump", {(VOID) s_fprintf (DEBUGFILE, "\nDump of block %ld\n\n", lba);});
    DBUG_EXECUTE ("dump", {(VOID) do_dump (blkp);});
    DBUG_VOID_RETURN;
}

#endif	/* DUMPBLK */


/*
 *  FUNCTION
 *
 *	file_chown    change ownership of a file
 *
 *  SYNOPSIS
 *
 *	VOID file_chown (path, owner, group)
 *	char *path;
 *	int owner, group;
 *
 *  DESCRIPTION
 *
 *	Changes owner and group of a file pointed to by path.
 *	Returns no value but causes error message to be printed
 *	if any problems occur.
 *
 *	Under 4.2 BSD the chown system call only works if called
 *	with effective uid of root.  This is supposedly for accounting.
 *
 *	The bru default is to always restore as much as possible,
 *	including the file ownership to the archived uid.  This has
 *	obvious problems for normal users reading tapes from other sites
 *	where the numeric uid's most likely are completely different.
 *	For this case, we provide a special flag (-C) that chown's all
 *	the files to the uid of the user running bru.  The reasoning
 *	behind which case to make the default and which case to make
 *	a special flag is given in the "ownership" file in the notes
 *	directory.  Basically, the default is easier to correct, and
 *	potentially less damaging to the system, than the results of
 *	applying the -C flag.
 *
 */

VOID file_chown (path, owner, group)
char *path;
int owner, group;
{
    DBUG_ENTER ("file_chown");

    if (flags.Cflag) {
	owner = info.bru_uid;
	group = info.bru_gid;
    }

    DBUG_PRINT ("chown", ("file %s, owner %d, group %d", path, owner, group));

    if (s_chown (path, owner, group) == SYS_ERROR) {
	bru_error (ERR_CHOWN, path);
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	reload    reload first volume of multivolume archive
 *
 *  SYNOPSIS
 *
 *	VOID reload (reason)
 *	char *reason;
 *
 *  DESCRIPTION
 *
 *	Issues prompt for user to reload first volume of a multivolume
 *	archive for the reason (pass) specified.  For example, a differences
 *	pass of a newly created archive.
 *
 */
 
VOID reload (reason)
char *reason;
{
	int vol;
	extern int lastvolume;

    DBUG_ENTER ("reload");
	/* if lastvolume != 0, then we are doing -i or -d after a -c, and we
	 * had multiple tapes, so we need to prompt for the original volume.
	*/
    if(vol=(ar_vol()) || lastvolume) {
	tty_printf ("%s: ready for %s pass\n", info.bru_name, reason);
	(VOID) new_arfile (0);
    }
    DBUG_VOID_RETURN;
}


VOID fork_shell ()
{
    register char *dir;
    register char *file;
    register char *shell;
    auto char *vector[2];
    
    DBUG_ENTER ("fork_shell");
    shell = s_getenv ("SHELL");
    if (shell == NULL) {
#ifdef amiga
	dir = NULL;		/* Turn these into defines */
	file = "c:newcli";
#else
	dir = "/bin";
	file = "sh";
#endif
    } else {
	dir = "";
	file = shell;
    }
    vector[0] = file;
    vector[1] = NULL;
    (VOID) execute (dir, file, vector);
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	newdir    make a directory node
 *
 *  SYNOPSIS
 *
 *	BOOLEAN newdir (fip)
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Makes a directory with given stat and name.  If an error occurs,
 *	print error message.
 *
 */


BOOLEAN newdir (fip)
register struct finfo *fip;
{
    register BOOLEAN filemade;

    DBUG_ENTER ("newdir");
    if (s_mkdir (fip -> fname, (int) fip->statp->st_mode) == SYS_ERROR) {
	bru_error (ERR_MKDIR, fip -> fname);
    } else {
	filemade = TRUE;
    }
    DBUG_RETURN (filemade);
}


/*
 *  FUNCTION
 *
 *	magic_ok    test a block to see if the magic value is recognized
 *
 *  SYNOPSIS
 *
 *	BOOLEAN magic_ok (blkp)
 *	register union blk *blkp;
 *
 *  DESCRIPTION
 *
 *	Test the block pointed to and return TRUE if the block has a
 *	recognized magic number, FALSE otherwise.
 *
 */

BOOLEAN magic_ok (blkp)
register union blk *blkp;
{
    register BOOLEAN rtnval;
    register int mag;
	
    DBUG_ENTER ("magic_ok");
    rtnval = FALSE;
    if (blkp == NULL) {
	bru_error (ERR_BUG, "magic_ok");
    } else {
	mag = (int) FROMHEX (blkp -> HD.h_magic);
	if (mag==A_MAGIC || mag==H_MAGIC || mag==D_MAGIC || mag==T_MAGIC) {
	    rtnval = TRUE;
	}
    }
    DBUG_RETURN (rtnval);
 }
 

/*
 *	Note that because the name buffer is static and reused on each call,
 *	we can only have one finfo structure, with an attached compressed
 *	file, active at any given time.
 */

BOOLEAN openzfile (fip)
struct finfo *fip;
{
    int status = FALSE;
    static char tname[NAMESIZE];
    static char *tfilep;
#ifndef sgi
    extern char *mktemp ();
#endif

    DBUG_ENTER ("openzfile");
    if (tfilep == NULL) {
	tfilep = mktemp ("brutmpXXXXXX");
    }
    sprintf (tname, TEMPLATE, info.bru_tmpdir, tfilep);
    DBUG_PRINT ("brutmpdir", ("use temp file '%s'", tname));
    fip -> zfname = tname;
    if ((fip -> zfildes = s_creat (fip -> zfname, 0600)) == SYS_ERROR) {
	bru_error (ERR_OPEN, fip -> zfname);
    } else {
	status = TRUE;
    }
    DBUG_RETURN (status);
}


/*
 *  FUNCTION
 *
 *	compressfip    compress a file
 *
 *  DESCRIPTION
 *
 *	Given file info pointer for an open uncompressed file, attempt
 *	to create a compressed version of the file in the directory
 *	set by the BRUTMPDIR environment variable.  Returns TRUE if
 *	successful, FALSE if the compression fails for some reason.
 *
 *  NOTES
 *
 *	We play it safe before proceeding and unconditionally mark 
 *	the fip as not having a compressed version of the file.
 *
 *	Since we aren't bothering to check the return values of
 *	s_unlink and s_close, just do them at the end if we fail
 *	for some reason, and don't worry about whether or not
 *	the file descriptor is open or the file exists.  This might
 *	be a potential problem on nonunix systems (closing nonopen
 *	descriptors or unlinking nonexistant files).
 *
 */

/* global so openandchk can print write message if > LONG_MAX */
static struct stat64 comp_statbuf;

BOOLEAN compressfip (fip)
struct finfo *fip;
{
    int status = FALSE;

    DBUG_ENTER ("compressfip");
	comp_statbuf.st_size = 0;
    fip -> fi_flags &= ~FI_LZW;
    if (openzfile (fip)) {
	if (compress (fip)) {
	    (VOID) s_lseek (fip -> fildes, 0L, 0);
	    (VOID) s_close (fip -> zfildes);
	    fip -> zfildes = s_open (fip -> zfname, O_RDONLY, 0);
	    if (fip -> zfildes == SYS_ERROR) {
		bru_error (ERR_OPEN, fip -> zfname);
	    } else {
		if (s_stat (fip -> zfname, &comp_statbuf) == SYS_ERROR) {
		    bru_error (ERR_STAT, fip -> zfname);
		} else {
		    /*
		     * Added for xfs
		     */
		    if (comp_statbuf.st_size <= LONG_MAX){
			    status = TRUE;
			    fip -> fi_flags |= FI_LZW;
			    fip -> zsize = comp_statbuf.st_size;
		    }
		}
	    }
	}
	if (!status) {
	    (VOID) s_close (fip -> zfildes);
	    (VOID) s_unlink (fip -> zfname);
	}
    }
    DBUG_RETURN (status);
}


BOOLEAN decompfip (fip)
struct finfo *fip;
{
    int status = FALSE;

    DBUG_ENTER ("decompfip");
    fip -> zfildes = s_open (fip -> zfname, O_RDONLY, 0);
    if (fip -> zfildes == SYS_ERROR) {
	bru_error (ERR_OPEN, fip -> zfname);
	bru_error (ERR_ZXFAIL, fip -> fname);
    } else {
	fip -> fildes = s_creat (fip -> fname, 0600);
	if (fip -> fildes == SYS_ERROR) {
	    bru_error (ERR_OPEN, fip -> fname);
	    bru_error (ERR_ZXFAIL, fip -> fname);
	} else {
	    if (!decompress (fip)) {
		bru_error (ERR_ZXFAIL, fip -> fname);
	    } else {
		status = TRUE;
	    }
	    s_close (fip -> fildes);
	}
	s_close (fip -> zfildes);
    }
    s_unlink (fip -> zfname);
    DBUG_RETURN (status);
}

/*
 *	Add the ".Z" extension to a name.  Note that we only call addz
 *	on buffers which have previously been filled via copyname, which
 *	ensures that there is sufficient space for the ".Z" extension
 *	if the -Z command line option is given.  Thus we don't need to
 *	check for a buffer overflow.
 */

VOID addz (fname)
char *fname;
{
    register char *endp;
    
    DBUG_ENTER ("addz");
    DBUG_PRINT ("addz", ("add a .Z extension to '%s'", fname));
    endp = fname + s_strlen (fname);
    *endp++ = '.';
    *endp++ = 'Z';
    *endp = EOS;
    DBUG_PRINT ("addz", ("final name '%s'", fname));
    DBUG_VOID_RETURN;
}

/*
 *	Strip the ".Z" extension from a name.
 */

VOID stripz (fname)
char *fname;
{
    register char *endp;
    
    DBUG_ENTER ("stripz");
    DBUG_PRINT ("stripz", ("strip a .Z extension from '%s'", fname));
    endp = fname + s_strlen (fname);
    if ((*--endp == 'Z') && (*--endp == '.')) {
	*endp = EOS;
    }
    DBUG_PRINT ("stripz", ("final name '%s'", fname));
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	discard_zfile    discard any compressed version of file
 *
 *  SYNOPSIS
 *
 *	static VOID discard_zfile (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given a pointer to a file information structure, if there
 *	is a compressed version of the file associated with it,
 *	discard the compressed version and fix up the file info
 *	structure appropriately.
 *
 */

VOID discard_zfile (fip)
struct finfo *fip;
{
    DBUG_ENTER ("discard_zfile");
    if (IS_COMPRESSED (fip)) {
	fip -> fi_flags &= ~FI_LZW;
	if (s_close (fip -> zfildes) == SYS_ERROR) {
	    bru_error (ERR_CLOSE, fip -> zfname);
	}
	if (s_unlink (fip -> zfname) == SYS_ERROR) {
	    bru_error (ERR_UNLINK, fip -> zfname);
	}
    }
    DBUG_VOID_RETURN;
}


/* open a file, doing all the error checking, and possibly doing the
 * compression.  This code is used by both estimate and by
 * create()
*/
BOOLEAN
openandchkcompress(struct finfo *fip)
{
    BOOLEAN rval = FALSE;

    if (IS_REG (fip->statp->st_mode) && fip->lname == NULL) {
	if (file_access (fip->fname, A_READ, TRUE)) {
	    fip->fildes = s_open (fip->fname, O_RDONLY|O_NDELAY, 0);
	    if (fip->fildes == SYS_ERROR) {
		bru_error (ERR_OPEN, fip->fname);
	    } else {
		rval = TRUE;
		if (flags.Zflag) {
		    if (!compressfip (fip)) {
			if(comp_statbuf.st_size > LONG_MAX) {
			    bru_error (ERR_TOO_BIG, fip->fname);
				rval = FALSE;
			}
			else {
			    bru_error (ERR_ZFAILED, fip->fname);
				fprintf(stderr, "%llx < %llx\n",
			(unsigned long long)comp_statbuf.st_size, (unsigned long long)LONG_MAX);
			}
			discard_zfile (fip);
			s_lseek (fip->fildes, 0L, SEEK_SET);
		    } else {
			if (fip->zsize > fip->statp->st_size) {
			    if (flags.vflag > 3)
				bru_error (ERR_NOSHRINK, fip->fname);
			    discard_zfile (fip);
				s_lseek (fip->fildes, 0L, SEEK_SET);
			}
		    }
		}
	    }
	}
    }
    return rval;
}
