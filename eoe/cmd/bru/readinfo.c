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
 *	readinfo.c    routines to read and process archive info block
 *
 *  SCCS
 *
 *	@(#)readinfo.c	9.11	5/11/88
 *
 */

 
/*
 *	Many of these routines used to be in the file blocks.c but have
 *	been moved to this file in an attempt to simplify blocks.c and
 *	decouple some of the routines.  This was only partially successful
 *	since they have gradually evolved into mutually referential files.
 *
 *	The entire "virtual archive" implementation has evolved from a
 *	relatively simple straightforward set of routines to the complicated
 *	convoluted code which now exists.  It is time to chuck the whole
 *	thing and start over, using what has been learned about the
 *	implementation requirements.  This is a prime example of why
 *	you should plan to throw away the first implementation of anything,
 *	because you WILL throw it away whether you plan to or not!!
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
#include "config.h"
#include "blocks.h"
#include "macros.h"
#include "errors.h"
#include "devices.h"
#include "flags.h"
#include "bruinfo.h"
#include "finfo.h"


extern VOID bru_error ();		/* Report an error to user */
extern VOID fork_shell ();		/* Fork a shell */
extern int execute ();
extern char *s_ctime ();
extern BOOLEAN ar_ispipe ();
extern union blk *ar_seek ();
extern VOID switch_media ();
extern VOID ar_close ();
extern VOID ar_open ();
extern VOID ar_read ();
extern VOID deallocate ();		/* Deallocate the block buffer */
extern VOID done ();
extern BOOLEAN chksum_ok ();		/* Test for good block checksum */
extern CHKSUM chksum ();		/* Compute checksum for block */
extern BOOLEAN need_swap ();		/* Test for swap needed */
extern VOID do_swap ();			/* Swap bytes/shorts */
extern VOID phys_seek ();		/* Physical seek to given location */
extern char *ur_gname ();		/* Translate uid to name */
extern char *gp_gname ();		/* Translate gid to name */
extern BOOLEAN seekable ();		/* Test for archive device seekable */

extern FILE *logfp;			/* Verbosity stream */
extern struct bru_info info;		/* Invocation information */
extern struct finfo afile;		/* Archive file info */
extern struct cmd_flags flags;		/* Flags from command line */
extern struct device *ardp;		/* Pointer to archive device info */
extern ULONG bufsize;			/* Archive read/write buffer size */
extern ULONG msize;			/* Size of archive media */
extern char mode;			/* Current mode */
extern time_t artime;			/* Time read from existing archive */
extern char response ();		/* Get single character response */

extern VOID s_fflush ();		/* Invoke library fflush function */
extern int s_fprintf ();		/* Invoke library fprintf function */

static VOID dump_info ();
static BOOLEAN is_junk ();
static VOID bad_magic ();
static VOID size_hack ();


/*
 *	The info_done flag prevents the recursive buffer size
 *	adjustment from printing the info block more than once.
 */
 
VOID read_info ()
{
    static BOOLEAN info_done = FALSE;
    register union blk *blkp;
    register int magic;
    VOID readsizes ();

    DBUG_ENTER ("read_info");
    blkp = ar_seek (0L, 0);
    ar_read (&afile);
    if (!is_junk (blkp)) {
	artime = (time_t) FROMHEX (blkp -> HD.h_time);
	readsizes (blkp);
	magic = (int) FROMHEX (blkp -> HD.h_magic);
	DBUG_PRINT ("magic", ("archive header magic number %#x", magic));
	if (magic != A_MAGIC) {
	    bad_magic (blkp);
	} else {
	    if ((mode == 'i' && flags.vflag > 2 && !info_done)
		|| (mode == 'g')) {
		info_done = TRUE;
		dump_info (blkp);
	    }
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *	Return non-zero if buffer can be forced to BLKSIZE for
 *	efficiency during special cases (like for reading only
 *	the file header blocks during -t mode).
 */

BOOLEAN forcebuffer ()
{
    register int result = FALSE;

    DBUG_ENTER ("forcebuffer");
    result = (mode == 't' && msize != 0L && !flags.bflag && !flags.pflag);
    result &= seekable (afile.fname, BLKSIZE);
    DBUG_PRINT ("force", ("returns logical value %d", result));
    DBUG_RETURN (result);
}


/*
 *  FUNCTION
 *
 *	sanity    check various options for sanity
 *
 *  SYNOPSIS
 *
 *	static VOID sanity ()
 *
 *  DESCRIPTION
 *
 *	After all command line options have been processed, this
 *	function performs various "sanity" checks.  These could
 *	be done in-line in options() but there are some advantages
 *	to delaying the operation:
 *
 *		(1)	Syntax errors in the command line show
 *			up before semantic errors.
 *
 *		(2)	The switch statement in options() is
 *			usually quite long already.
 *
 *	Note that this is also a logical place to emit various
 *	verbosity messages and do some "post-processing" of
 *	options.
 *
 *  NOTE
 *
 *	The been_here flag is a hack to prevent the recursive
 *	auto buffer size adjustment from printing the sizes more
 *	than once.  Note that the first instance of ar_open prints
 *	the new sizes, which the second instance of ar_open implemented.
 *	This is all very dependent on that fact than sanity() is called
 *	after read_info() in ar_open().
 *
 */

VOID sanity ()
{
    static BOOLEAN been_here = FALSE;
    
    DBUG_ENTER ("sanity");
    if (forcebuffer ()) {
	DBUG_PRINT ("bufsize", ("buffer size forced to BLKSIZE"));
	bufsize = BLKSIZE;
    }
    if (flags.vflag > 1 && !been_here) {
	been_here = TRUE;
	(VOID) s_fprintf (logfp, "buffer size = %uk bytes\n", (UINT) bufsize/1024);
	(VOID) s_fprintf (logfp, "media size = ");
	if (msize != 0L) {
	    (VOID) s_fprintf (logfp, "%ldk bytes\n", (long) (msize/1024));
	} else {
	    (VOID) s_fprintf (logfp, "<unknown>\n");
	}
	s_fflush (logfp);
    }
    DBUG_VOID_RETURN;
}


static BOOLEAN is_junk (blkp)
union blk *blkp;
{
    register BOOLEAN result;

    DBUG_ENTER ("is_junk");
    result = FALSE;
    if (!chksum_ok (blkp)) {
	if (!need_swap (blkp)) {
	    if(flags.Tflag) {
		s_fprintf(logfp, "E2\n");
		s_fflush(logfp);
		result = TRUE;
	    }
	    else {
	    	bru_error (ERR_ISUM);
	    	result = TRUE;
	    	if (chksum (blkp) != 0L) {
	        	bru_error (ERR_FASTMODE);
	    	}
	    }
	} else {
	    do_swap ();
	}
    }
    DBUG_RETURN (result);
}


static VOID dump_info (blkp)
union blk *blkp;
{
    uid_t ar_uid;
    gid_t ar_gid;

    DBUG_ENTER ("dump_info");
    ar_uid = (uid_t) FROMHEX (blkp -> AH.a_uid);
    ar_gid = (gid_t) FROMHEX (blkp -> AH.a_gid);
    if (mode == 'g') {
	(VOID) s_fprintf (logfp, "label:\t	%s\n", blkp -> AH.a_label);
	(VOID) s_fprintf (logfp, "created:	%s", s_ctime((long *) &artime));
	(VOID) s_fprintf (logfp, "device:\t	%s\n", blkp -> AH.a_name);
	(VOID) s_fprintf (logfp, "user:\t	%s\n", ur_gname (ar_uid));
	(VOID) s_fprintf (logfp, "group:\t	%s\n", gp_gname (ar_gid));
	(VOID) s_fprintf (logfp, "system:\t	%s %s %s %s %s\n",
		   blkp -> AH.a_host.sysname, blkp -> AH.a_host.nodename,
		   blkp -> AH.a_host.release, blkp -> AH.a_host.version,
		   blkp -> AH.a_host.machine);
	(VOID) s_fprintf (logfp, "bru:		\"%s\"\n", blkp -> AH.a_id);
	(VOID) s_fprintf (logfp, "release:	%ld.%ld\n",
		   FROMHEX (blkp -> HD.h_release),
		   FROMHEX (blkp -> HD.h_level));
	(VOID) s_fprintf (logfp, "variant:	%ld\n",
		   FROMHEX (blkp -> HD.h_variant));
    } else {
	(VOID) s_fprintf (logfp, "\n**** archive header info ****\n\n");
	(VOID) s_fprintf (logfp, "archive created: %s", s_ctime((long *) &artime));
	(VOID) s_fprintf (logfp, "release: %ld.%ld  ",
		   FROMHEX (blkp -> HD.h_release),
		   FROMHEX (blkp -> HD.h_level));
	(VOID) s_fprintf (logfp, "variant: %ld\n", FROMHEX (blkp -> HD.h_variant));
	(VOID) s_fprintf (logfp, "bru id: \"%s\"\n", blkp -> AH.a_id);
	(VOID) s_fprintf (logfp, "archive label: \"%s\"\n",	blkp -> AH.a_label);
	(VOID) s_fprintf (logfp, "written on: \"%s\"\n", blkp -> AH.a_name);
	(VOID) s_fprintf (logfp, "user id: %s  ", ur_gname (ar_uid));
	(VOID) s_fprintf (logfp, "group id: %s\n", gp_gname (ar_gid));
	(VOID) s_fprintf (logfp, "system identification: %s %s %s %s %s\n",
		   blkp -> AH.a_host.sysname, blkp -> AH.a_host.nodename,
		   blkp -> AH.a_host.release, blkp -> AH.a_host.version,
		   blkp -> AH.a_host.machine);
	(VOID) s_fprintf (logfp, "\n**** end info ****\n\n");
    }
    s_fflush (logfp);
    DBUG_VOID_RETURN;
}


static VOID bad_magic (blkp)
union blk *blkp;
{
    register int vol;
    register char key;

    DBUG_ENTER ("bad_magic");
    vol = (int) FROMHEX (blkp -> HD.h_vol);
    bru_error (ERR_IMAGIC, vol + 1);
    key = response ("c => continue  q => quit  r => reload  s => fork shell",
    	'q');
    switch (key) {
	case 'q':
	case 'Q':
	    done ();
	    break;
	case 'r':
	case 'R':
	    switch_media (0);
	    phys_seek (0L);
	    read_info ();
	    break;
	case 's':
	case 'S':
	    fork_shell ();
	    bad_magic (blkp);
	    break;
    }
    DBUG_VOID_RETURN;
}


/*
 *	Attempt to adjust to new buffer size.
 */

static VOID size_hack (size)
UINT size;
{
    DBUG_ENTER ("size_hack");

    if (ardp != NULL && ardp -> dv_flags & D_NOREWIND) {
	size /= 1024;
	(VOID) s_fprintf (stderr, "%s: don't know how to rewind archive device\n",
		info.bru_name);
	(VOID) s_fprintf (stderr,"%s: rerun with \"-b %uk\" argument\n",
		info.bru_name, size);
	s_fflush (stderr);
	done ();
    } else {
	DBUG_PRINT ("bufsize", ("adjust buffer size to %u", size));
	flags.bflag = TRUE;
	bufsize = size;
	ar_close ();
	deallocate ();
	ar_open ();
    }
    DBUG_VOID_RETURN;
}


/*
 *	Note that in fast mode (-F flag), the information in the header
 *	block may be unreliable.  We ignore the sizes if they look
 *	unreasonable (like 0).  This can actually happen if there is
 *	a hard read error on the first block and -F is used.
 */

VOID readsizes (blkp)
register union blk *blkp;
{
    register UINT size;
    register long ar_msize;

    DBUG_ENTER ("readsizes");
    if (!flags.sflag) {
	ar_msize = FROMHEX (blkp -> HD.h_msize);
	if (ar_msize > 0) {
	    msize = ar_msize;
	}
    }
    if (!flags.bflag) {
	size = (UINT) FROMHEX (blkp -> HD.h_bufsize);
	DBUG_PRINT ("bufsize", ("recorded buffer size = %u", size));
	DBUG_PRINT ("bufsize", ("current buffer size = %u", bufsize));
	if (size != 0 && size != bufsize && !ar_ispipe()) {
	    size_hack (size);
	}
    }
    DBUG_VOID_RETURN;
}

