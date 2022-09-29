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
 *	init.c    routines for doing initialization at startup
 *
 *  SCCS
 *
 *	@(#)init.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines for doing initialization at startup.
 *	These routines are called to process command line options,
 *	initialize uid/gid translation table, build the file tree, etc.
 *
 */


#include "autoconfig.h"

#include <stdio.h>
#include <ctype.h>
#include <limits.h>

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
#include "errors.h"
#include "blocks.h"
#include "macros.h"
#include "finfo.h"
#include "devices.h"
#include "flags.h"
#include "bruinfo.h"


/*
 *	External bru functions.
 */

extern char *s_getenv ();	/* Get an environment variable */
extern int s_atoi ();		/* Convert ascii string to integer */
extern int s_isdigit ();	/* Test for a digit (0-9) */
extern char *s_strrchr ();	/* Find last given character in string */
extern char *s_strchr ();	/* Find given character in string */
extern char *s_gets ();		/* Get string from a stream */
extern char *s_ctime ();	/* Get string form of time */
extern int s_getuid ();		/* Get real user ID of process */
extern int s_getgid ();		/* Get real group ID of process */
extern long s_time ();		/* Get current system time */
extern int s_getopt ();		/* Parse arguments */
extern VOID bru_error ();	/* Report an error to user */
extern VOID usage ();		/* Give usage info */
extern VOID done ();		/* Clean up and exit with status */
extern BOOLEAN file_stat ();	/* Get file stat buffer */
extern VOID ur_init ();		/* Initialize uid translation list */
extern VOID gp_init ();		/* Initialize gid translation list */
extern int ur_guid ();		/* Translate name to uid */
extern char *ur_gname ();	/* Translate uid to name */
extern VOID finfo_init ();	/* Initialize a finfo structure */
extern ULONG getsize ();		/* Get a size adjusted by scale factor */
extern time_t date ();		/* Convert date string to time */
extern VOID ar_open ();		/* Open archive file */
extern VOID copyname ();	/* Copy pathnames around */
extern int s_fprintf ();	/* Formatted print to stream */

extern struct device *get_ardp ();
extern char *getcwd();

#ifdef amiga
extern SIGTYPE s_signal();		/* Set up signal handling */
#endif

/*
 *	Each explicit filename argument on the command line is
 *	added to the tree of files to archive.  The tree_add
 *	function is responsible for adding each name to the tree.
 *	On systems where the shell expands wildcard characters
 *	in filenames (Unix for example), the tree_add routine can be
 *	called directly.  Others, such as the Amiga, must have
 *	the arguments expanded before calling tree_add, so on these
 *	machines first call the appropriate expansion routine, which
 *	call tree_add with each expanded filename.
 */

#ifdef amiga
#define TREE_ADD amiga_tree_add
extern VOID amiga_tree_add ();	/* Add pathname to tree, with expansions */
#else
#define TREE_ADD tree_add
extern VOID tree_add ();	/* Add pathname to tree */
#endif

/*
 *	Library variables.
 */

extern char *optarg;		/* Next optional argument */
extern int optind;		/* Index of first non-option arg */

/*
 *	External bru variables.
 */

extern struct bru_info info;	/* Invocation information */
extern struct cmd_flags flags;	/* Flags from command line */
extern ULONG bufsize;		/* Archive read/write buffer size */
extern ULONG msize;		/* Size of archive media */
extern struct device *ardp;	/* Archive device info pointer */
extern time_t ntime;		/* Time given by -n option */
extern uid_t uid;		/* User ID derived via -o option */
extern char *label;		/* Archive label string */
extern struct finfo afile;	/* Archive file info */
extern FILE *logfp;		/* Verbosity stream */
extern char* working_dir;	/* pathname of current working dir */
extern int getsize_err;	/* to distinguish err 0 return from normal 0 */

/*
 *	Locals.
 */

static VOID init_info ();
static VOID init_time ();
static VOID init_uid ();
static VOID options ();
static VOID buildtree ();
static VOID process_opts ();

/*
 *	To limit the size of the option parsing routine "options"
 *	option arguments are saved temporarily to be processed
 *	at a later time.  Also, there are some option arguments
 *	which cannot be effectively used until some other processing
 *	is done first, the -o argument for example.  The -o argument
 *	requires that ur_init be called first.
 *
 */

static char *A_arg;		/* Argument given for -A option */
static char *b_arg;		/* Argument given for -b option */
static char *n_arg;		/* Argument given for -n option */
static char *o_arg;		/* Argument given for -o option */
static char *s_arg;		/* Argument given for -s option */
static char *u_arg;		/* Argument given for -u option */

#ifdef DBUG
#define OPTSTRING "#:aA:b:BcCdDef:FghijKlL:mn:o:pRSs:tTu:vwxXZ"
#else
#define OPTSTRING "aA:b:BcCdDef:FghijKlL:mn:o:pRSs:tTu:vwxXZ"
#endif


/*
 *  FUNCTION
 *
 *	init    perform initialization procedures
 *
 *  SYNOPSIS
 *
 *	VOID init (argc, argv)
 *	int argc;
 *	char *argv[];
 *
 *  DESCRIPTION
 *
 *	Performs initialization functions which are common to
 *	all operations: create, table, update, extract, etc.
 *
 *	Any errors at this time cause immediate exit.
 *
 *	On the Commodore Amiga, system resources are not automatically
 *	returned to the system when a task exits.  Thus as part of
 *	the cleanup process, we must be sure to return any resouces.
 *	For normal runs, this is taken care of, but for aborted (via
 *	control-C) runs, we must do it in the signal handling function.
 *	Thus we must set up to catch SIG_INT all the time, not just
 *	when it is necessary under UNIX.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin init
 *	    Initialize the execution information structure
 *	    Get command line options
 *	    If need uid/gid translation tables then
 *		Initialize the tables
 *	    End if
 *	    Process command line options
 *	    Build directory tree
 *	End init
 *
 */

VOID init (argc, argv)
int argc;
char *argv[];
{
    DBUG_ENTER ("init");
    options (argc, argv);
#ifdef sgi
    if (flags.hflag) {
	usage(VERBOSE);
	done();
    }
#endif
    init_info (argv);
    if (flags.oflag || flags.tflag || flags.iflag || flags.gflag) {
	ur_init ();
	gp_init ();
    }
    process_opts (argv);
    /* this is done because bru used to be setuid root, and so it
     * does all kinds of file access checking using access().  We
     * want to allow folks to invoke bru from setuid programs and
     * have it work as that uid, so rather than fixing up all of
     * the access code, I simply set group and user to the effective
     * ids...  Olson, 7/91.
    */
    setuid(geteuid());
    setgid(getegid());
    buildtree (argc, argv);
#ifdef amiga
    (VOID) s_signal (SIGINT, done);
#endif
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	options    process command line options
 *
 *  SYNOPSIS
 *
 *	static VOID options (argc, argv)
 *	int argc;
 *	char *argv[];
 *
 *  DESCRIPTION
 *
 *	Processes command line options up to the first file name
 *	specified on the command line.  Returns an updated argv
 *	pointer with will contain only the file names, starting
 *	with argv[0].
 *
 *	Note that the only processing of the arguments done at this
 *	time is redirection of the log file stream from stdout to
 *	stderr for the special case of writing an archive to stdout.
 *	This insures that the verbosity/logfile stream can be used
 *	as soon as the command line arguments are squirreled away.
 *	All processing is deferred until later.
 *
 */

static VOID options (argc, argv)
int argc;
char *argv[];
{
    register int c;

    DBUG_ENTER ("options");
    if (argc < 2) {
	usage (BRIEF);
	done ();
    }
    while ((c = s_getopt (argc, argv, OPTSTRING)) != EOF) {
	switch (c) {
#ifdef DBUG
	case '#':
	    DBUG_PUSH (optarg);
	    break;
#endif
	case 'a':
	    flags.aflag = TRUE;
	    break;
#ifndef sgi
	case 'A': 
	    flags.Aflag = TRUE;
	    A_arg = optarg;
	    break;
#endif
	case 'b': 
	    flags.bflag = TRUE;
	    b_arg = optarg;
	    break;
	case 'B':
	    flags.Bflag = TRUE;
	    break;
	case 'c': 
	    flags.cflag = TRUE;
	    break;
	case 'C':
	    flags.Cflag = TRUE;
	    break;
	case 'd':
	    flags.dflag++;
	    break;
	case 'D':
#ifdef NEVEREVER /* as of 4.0.x, -D is completely broken.  I'm tired
	* of fixing new bugs, and don't have time now, so silently ignore it
	* OLSON, 7/92 */
	    if(flags.cflag)	    /* Double buffering only works correctly */
	    	flags.Dflag = TRUE; /* in create mode right now. problems */
				    /* with media error handling */
#endif /* NEVEREVER */
	    break;
	case 'e':
	    flags.eflag = TRUE;
	    break;
	case 'f': 
	    flags.fflag = TRUE;
	    copyname (afile.fname, optarg);
	    break;
	case 'F':
	    flags.Fflag = TRUE;
	    break;
	case 'g':
	    flags.gflag = TRUE;
	    break;
	case 'h':
	    flags.hflag = TRUE;
	    break;
	case 'i':
	    flags.iflag = TRUE;
	    break;
	case 'j':
	    flags.jflag = TRUE;
	    break;
	case 'K':
	    flags.Kflag = TRUE;
	    break;
	case 'l':
	    flags.lflag = TRUE;
	    break;
	case 'L':
	    flags.Lflag = TRUE;
	    label = optarg;
	    break;
	case 'm':
	    flags.mflag = TRUE;
	    break;
	case 'n':
	    flags.nflag = TRUE;
	    n_arg = optarg;
	    break;
	case 'o':
	    flags.oflag = TRUE;
	    o_arg = optarg;
	    break;
	case 'p':
	    flags.pflag = TRUE;
	    break;
	case 'R':
	    flags.Rflag = TRUE;
	    break;
	case 'S':
	    flags.Sflag = TRUE;
	    break;
	case 's':
	    flags.sflag = TRUE;
	    s_arg = optarg;
	    break;
	case 't': 
	    flags.tflag = TRUE;
	    break;
	case 'T': 			/* used by vadmin backup and restore */
	    flags.Tflag = TRUE;		/* to use stdout and stdin instead of */
	    break;			/* tty_read and tty_write */
	case 'u': 
	    flags.uflag = TRUE;
	    u_arg = optarg;
	    break;
	case 'v': 
	    flags.vflag++;		/* Multilevel verbosity */
	    break;
	case 'w': 
	    flags.wflag = TRUE;
	    break;
	case 'x': 
	    flags.xflag = TRUE;
	    break;
	case 'X': 
	    if(flags.xflag && flags.jflag) /* relative pathnames echoed as  */
	    	flags.Xflag = TRUE;	   /* fullpathnames, used by vadmin */
	    break;			   /* vadmin restore only. */
	case 'Z':
	    flags.Zflag = TRUE;
	    break;
	default:
	    usage (BRIEF);
	    done ();
	}
    }
    if ((flags.eflag || flags.cflag) && flags.fflag) {
	if (afile.fname != NULL && STRSAME ("-", afile.fname)) {
	    logfp = stderr;
	}
    }

	/* if listing, linebuffer output, regardless of whether it is
	 * a terminal.  Compared to the other processing, the extra
	 * overhead is negligible, and it makes it possible to do some
	 * things in shell scripts (particuarly system recovery with
	 * restore-mr) that aren't otherwise possible.  Olson, 4/92 */
	if(flags.tflag)
		setlinebuf(logfp);
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	buildtree    build directory tree from files on command line
 *
 *  SYNOPSIS
 *
 *	static VOID buildtree (argc, argv)
 *	int argc;
 *	char *argv[];
 *
 *  DESCRIPTION
 *
 *	Adds each file name specified on the command line to the
 *	directory tree.  If there are no files specified, the
 *	directory tree is "." by default.  If "-" is given
 *	instead of files, a list of files is read from the
 *	standard input and used to build the tree.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin buildtree
 *	    If read files from standard input then
 *		While there is another name on standard input
 *		    Add that name to the tree
 *		End while
 *	    Else
 *		If no files given and creating archive then
 *		    Tree is simply "."
 *		Else
 *		    For each pathname on command line
 *			Add pathname to the tree
 *		    End for
 *		End if
 *	    End if
 *	End buildtree
 *
 */

static VOID buildtree (argc, argv)
int argc;
char *argv[];
{
    register int index;
    auto char namebuf[PATH_MAX+1];

    DBUG_ENTER ("buildtree");
    if (argv[optind] != NULL && STRSAME (argv[optind], "-")) {
	while (s_fgets (namebuf, sizeof(namebuf), stdin)) {
		size_t s = strlen(namebuf);
		if(s) { /* paranoia */
			char lastc = namebuf[s-1];
			namebuf[s-1] = '\0';
			/* tree_add() will complain about the name being too long, for us */
			TREE_ADD (namebuf);
			/* if really long input line, read up to newline */
			if(!lastc) while(lastc=getchar() != EOF && lastc != '\n') ;
		}
	}
    } else {
	if (argv[optind] == NULL && (flags.cflag || flags.eflag)) {
	    TREE_ADD (".");
	} else {
	    for (index = optind; index < argc; index++) {
		TREE_ADD (argv[index]);
	    }
	}
    }
    DBUG_VOID_RETURN;
}



/*
 *  FUNCTION
 *
 *	init_time    get selection time for use with -n option
 *
 *  SYNOPSIS
 *
 *	static VOID init_time (cp)
 *	char *cp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a pathname or date string, initialize the
 *	selection time for the -n option.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin init_time
 *	    Make a dummy file info structure for testing access
 *	    If the string represents an existing file then
 *		If the file can be stat'd then
 *		    The time is the modification time of the file
 *		Else
 *		    Notify user of error
 *		End if
 *	    Else
 *		Convert the string as if it was a date string
 *	    End if
 *	End init_time
 *
 */

static VOID init_time (cp)
char *cp;
{
    auto struct stat64 sbuf;
    auto struct finfo file;
    register char *time;

    DBUG_ENTER ("init_time");
    DBUG_PRINT ("time", ("convert %s", cp));
    finfo_init (&file, cp, &sbuf);
    if (file_access (cp, A_EXISTS, FALSE)) {
	if (file_stat (&file)) {
	    ntime = file.statp -> st_mtime;
	} else {
	    bru_error (ERR_NTIME, cp);
	}
    } else {
	ntime = date (cp);
    }
    if (flags.vflag > 1) {
	time = s_ctime ((long *) &ntime);
	(VOID) s_fprintf (logfp, "select files modified since %s", time);
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	init_uid    initialize the user id for -o option
 *
 *  SYNOPSIS
 *
 *	static VOID init_uid (cp)
 *	char *cp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a string which is either a valid password
 *	file entry name or a numeric uid, initializes the user id
 *	for use with the -o option.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin init_uid
 *	    Initialize a dummy file info structure for testing access
 *	    If the string is the name of an existing file then
 *		If the file can be stat'd then
 *		    Use the file's user as the uid
 *		Else
 *		    Notify user of error 
 *		End if
 *	    Else
 *		Translate the string to a numeric id
 *		If translation successful then
 *		    Use the numeric id as the uid
 *		Else
 *		    If the first character is numeric then
 *			Convert string to numeric value
 *		    Else
 *			Notify user of conversion error
 *		    End if
 *		End if
 *	    End if
 *	    If verbosity greater than level one then
 *		Tranlate uid for user and print name
 *	    End if
 *	End init_uid
 *
 */


static VOID init_uid (cp)
char *cp;
{
    auto struct stat64 sbuf;
    auto struct finfo file;
    register int intuid;

    DBUG_ENTER ("init_uid");
    DBUG_PRINT ("uid", ("convert %s", cp));
    finfo_init (&file, cp, &sbuf);
    if (file_access (cp, A_EXISTS, FALSE)) {
	if (file_stat (&file)) {
	    uid = file.statp -> st_uid;
	} else {
	    bru_error (ERR_GUID, cp);
	}
    } else {
	intuid = ur_guid (cp);
	if (intuid != -1) {
	    uid = intuid;
	} else {
	    if (s_isdigit (*cp)) {
		uid = (unsigned) s_atoi (cp);
	    } else {
		bru_error (ERR_GUID, cp);
	    }
	}
    }
    DBUG_PRINT ("uid", ("got uid %u", uid));
    if (flags.vflag > 1) {
	(VOID) s_fprintf (logfp, "select files owned by %s (uid %d)\n",
		ur_gname (uid), uid);
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	process_opts    miscellaneous processing of command line options
 *
 *  SYNOPSIS
 *
 *	static VOID process_ops (argv)
 *	char *argv[];
 *
 *  DESCRIPTION
 *
 *	Certain processing required by some command line options is best
 *	delayed slightly from the time the option itself is recognized.
 *	This also helps to reduce the size of the usually large switch
 *	statement for detecting various options.  This post processing
 *	of arguments is done here.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin process_opts
 *	    If not creating inspecting or listing archive then
 *		If not finding differences, extracting, or getting help then
 *		    Tell user to specify a mode
 *		    Clean up and exit
 *		End if
 *	    End if
 *	    If reading file list from standard input then
 *		If reading archive from standard input then
 *		    Notify user of conflict in stdin usage
 *		    Clean up and exit
 *		End if
 *	    End if
 *	    If selecting only files for given user then
 *		Initialize the user id
 *	    End if
 *	    If selecting only files past given date then
 *		Initialize the date
 *	    End if
 *	    If unconditional selection specified then
 *		Remember if block special files are to be selected
 *		Remember if character special files are to be selected
 *		Remember if directories are to be selected
 *		Remember if symbolic links are to be selected
 *		Remember if fifos are to be selected
 *		Remember if regular files are to be selected
 *	    End if
 *	    If archive device was specified then
 *		Look it up in device table
 *	    Else
 *		Use first entry in device table
 *		Initialize the file name in file information structure
 *	    End if
 *	    Initialize the device pointer
 *	    If buffer size specified then
 *		Convert argument to internal format
 *		Round up to nearest archive block boundry
 *	    Else if device found and default buffer size for device then
 *		Round up default size to nearest block boundry and use it
 *	    End if
 *	    If size of media given then
 *		Convert size and save it
 *	    Else
 *		Get size from device table
 *	    End if
 *	    If media size unknown then
 *		If a media usage estimate was requested then
 *		    Warn user can't do it
 *		    Reset the media estimate flag to ignore request
 *		End if
 *	    Else
 *		Compute number of block groups per media
 *		If no groups on media then 
 *		    Notify user block media size too small
 *		    Clean up and exit
 *		Else
 *		    Make media size even multiple of buffer size
 *		End if
 *	    End if
 *	End process_opts
 *
 */


static VOID process_opts (argv)
char *argv[];
{
    register LBA grps;		/* Number of block groups */

    DBUG_ENTER ("process_opts");
    if (!flags.cflag && !flags.iflag && !flags.tflag && !flags.eflag
    	&& !flags.gflag && flags.dflag == 0 && !flags.xflag && !flags.hflag) {
	    bru_error (ERR_MODE);
	    done ();
    }
    if (argv[optind] != NULL && STRSAME (argv[optind], "-")) {
	if (STRSAME (afile.fname, "-") && !flags.cflag) {
	    bru_error (ERR_STDIN);
	    done ();
	}
    }
    if (flags.oflag) {
	init_uid (o_arg);
    }
    if (flags.nflag) {
	init_time (n_arg);
    }
    if (flags.uflag) {	/* sanity check the args */
	    char *tmp = u_arg;
	    while(*tmp) {
		switch(*tmp) {
		    case 'b':
			flags.ubflag = TRUE;
			break;
		    case 'c':
			flags.ucflag = TRUE;
			break;
		    case 'd':
			flags.udflag = TRUE;
			break;
		    case 'l':
			flags.ulflag = TRUE;
			break;
		    case 'p':
			flags.upflag = TRUE;
			break;
		    case 'r':
			flags.urflag = TRUE;
			break;
		    default:
			bru_error(ERR_ARGS, u_arg, "-u");
			done();
		}
		tmp++;
	    }
    }
    if (flags.Aflag) {	/* sanity check the args */
	char *tmp = A_arg;
	while(*tmp) {
	    switch(*tmp) {
		case 'c':
		    flags.Acflag = TRUE;
		    break;
		case 'i':
		    flags.Aiflag = TRUE;
		    break;
		case 'r':
		    flags.Arflag = TRUE;
		    break;
		case 's':
		    flags.Asflag = TRUE;
		    break;
		default:
		    bru_error(ERR_ARGS, A_arg, "-a");
		    done();
	    }
	    tmp++;
	}
    }
    if (flags.fflag) {
	ardp = get_ardp (afile.fname);
    } else {
	ardp = get_ardp ((char *)NULL);
	if (ardp == NULL) {
	    bru_error (ERR_DEFDEV);
	    done ();
	} else {
	    copyname (afile.fname, ardp -> dv_dev);
	}
    }
    if (flags.bflag) {
	bufsize =  getsize (b_arg);
	if(bufsize == 0) {	/* bad -b on command line */
	    bru_error (ERR_NOTNUM, b_arg, " for -b");
	    done();
	}
	bufsize = BLOCKS (bufsize) * BLKSIZE;
    } else if (ardp != NULL && ardp -> dv_bsize != 0) {
	bufsize = BLOCKS (ardp -> dv_bsize) * BLKSIZE;
    }
    if((int)bufsize <= 0) {	/* bad in brutab */
	char ebuf[32];
	sprintf(ebuf, "%d", bufsize);
	bru_error (ERR_NOTNUM, ebuf, " for blocksize (from brutab)");
	done();
    }
    DBUG_PRINT ("bufsize", ("buffer size %uk bytes", (UINT) bufsize/1024));
    if (flags.sflag) {
	msize = getsize (s_arg);
	if(msize == 0 && getsize_err) { /* bad -s */
	    bru_error (ERR_NOTNUM, s_arg, " for -s");
	    done();
	}
    } else if (ardp != NULL) {
	msize = ardp -> dv_msize;
    }
    DBUG_PRINT ("opts", ("msize %ld  bufsize %u", msize, bufsize));
    if (msize > 0) {
	DBUG_PRINT ("opts", ("msize %ld  bufsize %u", msize, bufsize));
	grps = msize / bufsize;
	if (grps == 0L) {
	    bru_error (ERR_BUFSZ);
	    done ();
	} else {
	    msize = grps * bufsize;
	}
	DBUG_PRINT ("opts", ("grps %ld  msize %ld", grps, msize));
    }
    if(flags.Xflag) {
	if((working_dir = getcwd((char *)NULL, 128)) == NULL) {
		bru_error(ERR_CWD);
		done();
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	init_info    initialize the invocation information structure
 *
 *  SYNOPSIS
 *
 *	static VOID init_info (argv)
 *	char *argv[];
 *
 *  DESCRIPTION
 *
 *	Initialize the invocation information structure.
 *
 */

static VOID init_info (argv)
char *argv[];
{
    DBUG_ENTER ("init_info");
    info.bru_uid = s_getuid ();
    info.bru_gid = s_getgid ();
    info.bru_tty = TERMINAL;
    info.bru_time = s_time ((long *) 0);
    info.bru_name = s_strrchr (argv[0], '/');
    if (info.bru_name == NULL) {
	info.bru_name = argv[0];
    } else {
	info.bru_name++;
    }
    if ((info.bru_tmpdir = s_getenv ("BRUTMPDIR")) == NULL) {
	info.bru_tmpdir = BRUTMPDIR;
    }
    DBUG_PRINT ("brutmpdir", ("prefered tmp dir is '%s'", info.bru_tmpdir));
    DBUG_VOID_RETURN;
}
