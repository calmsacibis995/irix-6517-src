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
 *	errors.c    error handling module for new bru utility
 *
 *  SCCS
 *
 *	@(#)errors.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Each possible error is assigned a number (actually a preprocessor
 *	symbol) which is recognized by this module.  The appropriate
 *	error message is printed.
 *
 *	It is also handy to have all the error strings in one place
 *	so that format modifications can be made easily.
 *
 *  BUGS
 *
 *	Perhaps it would be better to differentiate more between
 *	errors and warnings via two separate mechanisms, such as
 *	"bru_error" and "bru_warning".  This would simplify the
 *	code somewhat since the prefix "error - " or "warning -"
 *	could be issued at the entry point, along with incrementing
 *	the appropriate error or warning count.
 *
 */

#include "autoconfig.h"

#include <stdio.h>

#include <stdarg.h>			/* Use system supplied varargs */

#if unix || xenix
#  include <sys/types.h>
#else
#  include "sys.h"
#endif

#include "typedefs.h"
#include "dbug.h"
#include "manifest.h"
#include "errors.h"
#include "bruinfo.h"
#include "exeinfo.h"
#include "flags.h"

/*
 *	Externals used.
 */

extern int errno;			/* System error return code */
extern char *sys_errlist[];		/* Array of error messages */
extern int sys_nerr;			/* Size of sys_errlist[] */
extern struct bru_info info;		/* Information about bru invocation */
extern struct exe_info einfo;		/* Execution information */
extern char mode;			/* Current execution mode */
extern FILE *logfp;		/* Verbosity stream */
extern struct cmd_flags flags;	/* Flags given on command line */

/*
 *	External bru functions
 */

extern int s_vfprintf ();	/* Invoke library vfprintf function */
extern VOID s_fflush ();	/* Invoke the fflush function */
extern int s_fprintf ();	/* Invoke the fprintf function */

/*
 *	Define a structure with information about each known error.
 */

struct err {
    int err_no;			/* The internal error number */
    BOOLEAN err_sysmsg;		/* True if system msg available */
    int *err_cnt;		/* Pointer to count of these error types */
    char *err_msg;		/* Format string for error message */
};

#define MSG	TRUE		/* A system error message available */
#define NOMSG	FALSE		/* No system message available */

static struct err errs[] = {
    ERR_MODE, NOMSG, &einfo.errors, "specify mode (-cdeghitx)",
    ERR_AROPEN, MSG, &einfo.errors,"\"%s\": can't open archive",
    ERR_ARCLOSE, MSG, &einfo.warnings, "warning - close error on archive",
    ERR_ARREAD, MSG, &einfo.errors, "warning - archive read error at block %ld",
    ERR_ARWRITE, MSG, &einfo.errors, "warning - archive write error at block %ld",
    ERR_ARSEEK, MSG, &einfo.errors, "seek error on archive",
    ERR_BUFSZ, NOMSG, &einfo.errors, "media size smaller than I/O buffer size!",
    ERR_FORMAT, NOMSG, &einfo.warnings, "warning - media appears to be unformatted",
    ERR_BALLOC, MSG, &einfo.errors, "can't allocate %dk archive buffer",
    ERR_BSEQ, NOMSG, &einfo.warnings, "\"%s\": warning - block sequence error",
    ERR_DSYNC, NOMSG, &einfo.warnings, "warning - file synchronization error; attempting recovery ...",
    ERR_EACCESS, MSG, &einfo.errors, "\"%s\": no file",
    ERR_STAT, MSG, &einfo.errors, "\"%s\": can't stat",
    ERR_BIGPATH, NOMSG, &einfo.errors, "pathname \"%s\" too big",
    ERR_BIGFAC, NOMSG, &einfo.errors, "blocking factor %d too big",
    ERR_OPEN, MSG, &einfo.errors, "\"%s\": can't open",
    ERR_CLOSE, MSG, &einfo.warnings, "\"%s\": warning - file close error",
    ERR_READ, MSG, &einfo.errors, "\"%s\": read error",
    ERR_FTRUNC, NOMSG, &einfo.warnings, "\"%s\": warning - file was truncated",
    ERR_FGREW, NOMSG, &einfo.warnings, "\"%s\": warning - file grew while archiving",
    ERR_SUID, NOMSG, &einfo.warnings, "\"%s\": warning - can't set user id: Not owner",
    ERR_SGID, NOMSG, &einfo.warnings, "\"%s\": warning - can't set group id: Permission denied",
    ERR_EXEC, MSG, &einfo.errors, "\"%s\": can't exec",
    ERR_FORK, MSG, &einfo.errors, "can't fork, try again",
    ERR_BADWAIT, NOMSG, &einfo.errors, "unrecognized wait return %d",
    ERR_EINTR, MSG, &einfo.errors, "child interrupted",
    ERR_CSTOP, NOMSG, &einfo.errors, "\"%s\": fatal error; stopped by signal %d",
    ERR_CTERM, NOMSG, &einfo.errors, "\"%s\": fatal error; terminated by signal %d",
    ERR_CORE, NOMSG, &einfo.warnings, "\"%s\" core dumped",
    ERR_WSTATUS, NOMSG, &einfo.errors, "inconsistent wait status %o",
    ERR_AVAIL0, MSG, &einfo.errors, "Oops; supposed to be an unused error!",
    ERR_AVAIL1, MSG, &einfo.errors, "Oops; supposed to be an unused error!",
    ERR_SUM, NOMSG, &einfo.warnings, "\"%s\": warning - %ld block checksum errors",
    ERR_BUG, NOMSG, &einfo.errors, "internal bug in routine \"%s\"",
    ERR_MALLOC, MSG, &einfo.errors, "can't allocate %u more bytes",
    ERR_WALK, NOMSG, &einfo.errors, "internal error in tree; pathname overflow",
    ERR_DEPTH, NOMSG, &einfo.errors, "pathname too big, lost \"%s/%s\"",
    ERR_SEEK, MSG, &einfo.errors, "\"%s\": seek error",
    ERR_ISUM, NOMSG, &einfo.warnings, "warning - info block checksum error",
    ERR_WRITE, MSG, &einfo.errors, "\"%s\": write error",
    ERR_SMODE, MSG, &einfo.warnings, "\"%s\": warning - error setting mode",
    ERR_CHOWN, MSG, &einfo.warnings, "\"%s\": warning - error setting owner/group",
    ERR_STIMES, MSG, &einfo.warnings, "\"%s\": warning - error setting times",
    ERR_MKNOD, MSG, &einfo.errors, "\"%s\": error making node",
    ERR_MKLINK, MSG, &einfo.errors, "\"%s\": can't link to \"%s\"",
    ERR_ARPASS, NOMSG, &einfo.errors, "internal error; inconsistent phys blk addrs",
    ERR_IMAGIC, NOMSG, &einfo.warnings, "warning - missing archive header block; starting at volume %d",
    ERR_LALLOC, MSG, &einfo.warnings, "\"%s\": warning - lost linkage",
    ERR_URLINKS, NOMSG, &einfo.warnings, "\"%s\": warning - %d unresolved link(s)",
    ERR_TTYOPEN, MSG, &einfo.warnings, "\"%s\": warning - can't open for interaction",
    ERR_NTIME, NOMSG, &einfo.errors, "date conversion error: \"%s\"",
    ERR_UNAME, MSG, &einfo.warnings, "warning - uname failed",
    ERR_LABEL, NOMSG, &einfo.warnings, "warning - label \"%s\" too big",
    ERR_GUID, NOMSG, &einfo.errors, "uid conversion error: \"%s\"",
    ERR_CCLASS, NOMSG, &einfo.errors, "unterminated character class",
    ERR_OVRWRT, MSG, &einfo.errors, "\"%s\": can't overwrite",
    ERR_WACCESS, MSG, &einfo.errors, "\"%s\": can't access for write",
    ERR_RACCESS, MSG, &einfo.errors, "\"%s\": can't access for read",
    ERR_ARTIME, NOMSG, &einfo.warnings, "warning - volume not part of archive created %s",
    ERR_ARVOL, NOMSG, &einfo.warnings, "warning - found volume %d, expecting %d",
    ERR_STDIN, NOMSG, &einfo.errors, "can't read both file list and archive from stdin!",
    ERR_EOV, NOMSG, &einfo.warnings, "warning - premature end of volume %d",
    ERR_WPROT, NOMSG, &einfo.warnings, "warning - media appears to be write protected",
    ERR_FIRST, NOMSG, &einfo.warnings, "warning - media appears to be unformatted or write protected",
    ERR_BRUTAB, NOMSG, &einfo.warnings, "warning - using internal default device table",
    ERR_SUPERSEDE, NOMSG, &einfo.warnings, "\"%s\": warning - not superseded",
    ERR_IGNORED, NOMSG, &einfo.warnings, "\"%s\": warning - not found or not selected",
    ERR_FASTMODE, NOMSG, &einfo.warnings, "warning - may have to use -F option to read archive",
    ERR_BACKGND, NOMSG, &einfo.errors, "interaction needed, aborted by -B option",
    ERR_MKDIR, MSG, &einfo.errors, "\"%s\": error making directory",
    ERR_RDLINK, MSG, &einfo.errors, "\"%s\": error reading symbolic link",
    ERR_NOSYMLINKS, NOMSG, &einfo.errors, "\"%s\": symbolic links not supported",
    ERR_MKSYMLINK, MSG, &einfo.errors, "\"%s\": could not make symbolic link",
    ERR_MKFIFO, NOMSG, &einfo.errors, "\"%s\": could not make fifo",
    ERR_SYMTODIR, NOMSG, &einfo.warnings, "warning - link of \"%s\" to \"%s\", \"%s\" is a directory, no link made",
    ERR_HARDLINK, NOMSG, &einfo.warnings, "warning - link of \"%s\" to \"%s\", \"%s\" does not exist",
    ERR_FIFOTOREG, NOMSG, &einfo.warnings, "warning - extracted fifo \"%s\" as a regular file",
    ERR_IEOV, NOMSG, &einfo.warnings, "warning - unrecoverable error, assuming end of volume %d",
    ERR_ALINKS, NOMSG, &einfo.warnings, "\"%s\": warning - %d additional link(s) added while archiving",
    ERR_OBTF, NOMSG, &einfo.warnings, "\"%s\": line %d, obsolete brutab format",
    ERR_DEFDEV, NOMSG, &einfo.errors, "no default device in brutab file, use -f option",
    ERR_NOBTF, NOMSG, &einfo.errors, "support for obsolete brutab format not compiled in",
    ERR_BSIZE, NOMSG, &einfo.warnings, "warning - attempt to change buffer size from %u to %u ignored (incompatible brutab entries)",
    ERR_QFWRITE, NOMSG, &einfo.warnings, "warning - all data currently on \"%s\" will be destroyed",
    ERR_DBLSUP, MSG, &einfo.errors, "problem during double buffering",
    ERR_EJECT, MSG, &einfo.errors, "\"%s\": media ejection failed",
    ERR_NOSHRINK, NOMSG, &einfo.warnings, "\"%s\": warning - compressed version was larger, stored uncompressed",
    ERR_ZXFAIL, NOMSG, &einfo.warnings, "\"%s\": warning - decompression failed, not extracted",
    ERR_NOEZ, NOMSG, &einfo.warnings, "warning - estimate mode ignores compression",
    ERR_UNLINK, MSG, &einfo.warnings, "\"%s\": not deleted",
    ERR_ZFAILED, NOMSG, &einfo.warnings, "\"%s\": warning - compression failed, stored uncompressed",
    ERR_NOQIC24, NOMSG, &einfo.warnings, "warning - can't write QIC24 tapes on this device",
    ERR_CWD, NOMSG, &einfo.errors, "cannot stat the current directory",
    ERR_IOERR, NOMSG, &einfo.warnings, "warning - Input/Output Error",
    ERR_ARGS, NOMSG, &einfo.errors, "%s isn't a valid argument to %s",
    ERR_NOTNUM, NOMSG, &einfo.errors, "%s isn't a valid numeric value%s",
    ERR_NOMEDIA, NOMSG, &einfo.errors, "No media in drive",
    ERR_CREAT, MSG, &einfo.errors, "\"%s\": can't create",
    ERR_MKLINK_J, NOMSG, &einfo.errors, "\"%s\": can't link to unextracted file with -j",
    ERR_PAGESZ, MSG, &einfo.errors, "error - illegal pagesize (%d)",
    ERR_TOO_BIG,NOMSG,&einfo.warnings,"\"%s\": warning - skipping file, too big when compressed",
    ERR_DIFF_BIG,NOMSG,&einfo.warnings,"\"%s\": warning - cannot determine size, too big",
    ERR_NO_KFLAG,NOMSG,&einfo.warnings,"\"%s\": warning - skipping file, too big and -K not set",
    ERR_NO_ZFLAG,NOMSG,&einfo.warnings,"\"%s\": warning - skipping file, too big and -Z not set",
    ERR_OK_FLAG,NOMSG,&einfo.warnings,"\"%s\": warning - inclusion of large file will result in a non-portable archive",
    ERR_AUDIO, NOMSG, &einfo.warnings, "warning: drive was in audio mode, turning audio mode off",
    ERR_FIXAUDIO, NOMSG, &einfo.warnings, "warning: unable to disable audio mode.  Tape may not be readable",
    0, NOMSG, NULL, NULL
};


/*
 *  FUNCTION
 *
 *	bru_error    print appropriate message for given error
 *
 *  SYNOPSIS
 *
 *	bru_error (errorno, va_alist)
 *	int errorno;
 *	va_dcl
 *
 *  DESCRIPTION
 *
 *	Prints an error message, for the error specified by "errorno".
 *	Note that values for "errorno" are given in "errors.h".
 *
 *	Note that sys_errlist[] and sys_nerr are supplied by the
 *	runtime environment in most standard Unix systems.  Other
 *	implementations may require this module to be customized.
 *
 */

/*VARARGS1*/
VOID bru_error (int errorno, ...)
{
    register struct err *ep;
    auto va_list args;
    
    DBUG_ENTER ("bru_error");
    if(flags.Tflag) {
	/* handle some errors specially with T flag */
	switch(errorno) {
	case ERR_NOQIC24:
	    s_fprintf(logfp, "E5\n");
	    s_fflush(logfp);
	    DBUG_VOID_RETURN;
	case ERR_WPROT:
	    s_fprintf(logfp, "E4\n");
	    s_fflush(logfp);
	    DBUG_VOID_RETURN;
	case ERR_IEOV:
	    s_fprintf(logfp, "E6\n");
	    s_fflush(logfp);
	    DBUG_VOID_RETURN;
	case ERR_TOO_BIG:	/* too big to backup */
	case ERR_NO_ZFLAG:	/* same effective error; gui tool never uses -Z */
	case ERR_NO_KFLAG:	/* same effective error; gui tool never uses -K */
		/* unlike the above cases, we would really like a filename with
		 * this one. */
		va_start (args, errorno);
		(VOID) s_vfprintf (logfp, "E7 %s\n", args);
		va_end (args);
	    s_fflush(logfp);
	    DBUG_VOID_RETURN;
	case ERR_OK_FLAG:	/* if gui ever does use -ZK, ignore this 'error' */
	    DBUG_VOID_RETURN;
    }
	}
    va_start (args, errorno);
    (VOID) s_fprintf (stderr, "%s: ", info.bru_name);
    for (ep = errs; ep -> err_no != 0 && ep -> err_no != errorno; ep++) {;}
    if (ep -> err_no != errorno) {
	(VOID) s_fprintf (stderr, "warning - unknown internal error %d!", errorno);
	einfo.errors++;
    } else {
	(*(ep -> err_cnt))++;
	(VOID) s_vfprintf (stderr, ep -> err_msg, args);
	if (ep -> err_sysmsg) {
	    if (errno < 1 || errno > sys_nerr) {
		 (VOID) s_fprintf (stderr, ": !! bad errno %d !!", errno);
	    } else {
		 (VOID) s_fprintf (stderr, ": %s", sys_errlist[errno]);
	    }
	}
    }
    (VOID) s_fprintf (stderr, "\n");
    s_fflush (stderr);
    va_end (args);
    DBUG_VOID_RETURN;
}
