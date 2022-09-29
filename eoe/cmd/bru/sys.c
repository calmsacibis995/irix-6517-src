/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *		(Major additions by Arnold Robbins at Georgia Tech)	*
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
 *	sys.c    runtime support library calls
 *
 *  SCCS
 *
 *	@(#)sys.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Calls to the runtime support library are localized here
 *	to aid portability and modification and/or inclusion
 *	of custom versions.
 *
 *	This form of interface adds an extra level of function
 *	call overhead, but the benefits for modification or
 *	debugging purposes outweigh the negligible decrease
 *	in efficiency.  (Use the profiler to discover just
 *	how little time is actually spent in these routines!)
 *
 *	There is some question about the best way to deal with
 *	situations in which functions like "printf" return
 *	error results.  This generally indicates a severe
 *	problem with the environment or corrupted runtime
 *	data space.  In any case, it is probably undesirable
 *	to continue execution under those circumstances, so
 *	we abort with an IOT signal and core dump.
 *
 *	An additional advantage of having these functions
 *	centralized is that lint will only have one version
 *	of the library call to complain about.  This
 *	considerably reduces the typical size and complexity
 *	of the lint output log.
 *
 */

#include "autoconfig.h"

#include <stdarg.h>
#include <errno.h>

#if RMT				/* Want remote mag tape support via /etc/rmt */
#  if SYSRMTLIB			/* Use the installed system version */
#    include <local/rmt.h>	/* Remote mag tape functions, if available */
#  else				/* Else use our local internal copy */
#    include "rmt.h"		/* Local copy's include file */
#  endif
#endif

#if (amiga && LATTICE)
#define NARGS			/* stdio.h defines some args incompatibly */
#endif

#include <stdio.h>

#if (unix || xenix)
#  include <sys/types.h>
#  include "utsname.h"
#  include <sys/stat.h>
#  include <pwd.h>
#  include <grp.h>
#else
#  include "sys.h"
#  include "utsname.h"
#endif

#include <ctype.h>

#if unix || xenix
#  if BSD4_2
#    include <sys/wait.h>		/* Used by s_wait() */
#    include <sys/time.h>		/* Used by s_timezone() */
#  else
#    include <time.h>
#  endif
#  if HAVE_SYMLINKS
#      include <errno.h>
#  endif
#endif

#if amiga
#    include <errno.h>
#endif

#if sgi
#include <string.h>
#include <stdlib.h>
#endif

#include "typedefs.h"		/* Type definitions */
#include "config.h"
#include "dbug.h"		/* Macro based debugger file */
#include "manifest.h"		/* Manifest constants */
#include "macros.h"		/* Useful macros */
#include "errors.h"		/* Error codes */


/*
 *  FUNCTION
 *
 *	s_fprintf    call the fprintf function
 *
 *  SYNOPSIS
 *
 *
 *  DESCRIPTION
 *
 *	Invokes the library "fprintf" function.  We call the
 *	internal version of vprintf and let it deal with the
 *	variable arguments problem.
 */

/*VARARGS2*/
int s_fprintf (FILE *stream, char *format, ...)
{
    int rtnval;
    va_list args;

    va_start (args, format);
    rtnval = vfprintf (stream, format, args);
    va_end (args);
    return (rtnval);
}


int s_vfprintf (FILE *stream, char *format, va_list args)
{
    return(vfprintf (stream, format, args));
}


/*
 *  FUNCTION
 *
 *	s_sprintf    call the sprintf function
 *
 *  DESCRIPTION
 *
 *	Invokes the library "sprintf" function.  We call the internal
 *	form of vsprintf and let it deal with the variable arguments
 *	problem.
 *
 */

/*VARARGS2*/
int s_sprintf (char *s, char *format, ...)
{
    int rtnval;
    va_list args;

    va_start (args, format);
    rtnval = vsprintf (s, format, args);
    va_end (args);
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_fflush    invoke the library fflush function
 *
 *  SYNOPSIS
 *
 *	VOID s_fflush (stream)
 *	FILE *stream;
 *
 *  DESCRIPTION
 *
 *	Invokes the library fflush function.  Any error is
 *	immediately fatal.
 *
 */

VOID s_fflush (stream)
FILE *stream;
{
    DBUG_ENTER ("s_flush");
    if (fflush (stream) == EOF) {
		if(stream != stderr)
			fprintf(stderr, "bru: fflush failed: %s\n", strerror(errno));
		exit(2);
    }
    DBUG_VOID_RETURN;
}
 

/*
 *  FUNCTION
 *
 *	s_open    invoke file open library function
 *
 *  SYNOPSIS
 *
 *	int s_open (path, oflag [ , mode ])
 *	char *path;
 *	int oflag, mode;
 *
 *  DESCRIPTION
 *
 *	Invoke the library function to open a file.  Path points
 *	to a path name, oflag is for the file status flags, and
 *	mode is the file mode if the file is created.
 *
 */

int s_open (path, oflag, mode)
char *path;
int oflag, mode;
{
    register int rtnval;
    extern int open ();			/* See open(2) */

    DBUG_ENTER ("s_open");
    DBUG_PRINT ("open", ("file \"%s\", flag %d, mode %o", path, oflag, mode));
#ifdef amiga
    if (AccessRaw (path, 0) == 0) {
	DBUG_PRINT ("xopen", ("open '%s' as a raw device file", path));
	rtnval = OpenRaw (path, oflag, mode);
    } else {
	DBUG_PRINT ("xopen", ("open '%s' as a normal file", path));
	rtnval = open (path, oflag, mode);
    }
#else
    rtnval = open (path, oflag, mode);
#endif
    DBUG_PRINT ("open", ("open on stream %d", rtnval));
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_close    invoke the low level file close facility
 *
 *  SYNOPSIS
 *
 *	int s_close (fildes)
 *	int fildes;
 *
 *  DESCRIPTION
 *
 *	Close the file for the given file descriptor.
 *
 */

int s_close (fildes)
int fildes;
{
    register int rtnval;
    extern int close ();			/* See close(2) */

    DBUG_ENTER ("s_close");
    DBUG_PRINT ("close", ("close stream %d", fildes));
#ifdef amiga
    if (IsRawFd (fildes)) {
	rtnval = CloseRaw (fildes);
    } else {
	rtnval =  close (fildes);
    }
#else
    rtnval =  close (fildes);
#endif
    DBUG_RETURN (rtnval);
}
 


/*
 *  FUNCTION
 *
 *	s_access    invoke low level access library routine
 *
 *  SYNOPSIS
 *
 *	int s_access (path, amode)
 *	char *path;
 *	int amode;
 *
 *  DESCRIPTION
 *
 *	Invoke low level library routine "access" to test files
 *	for accessibility.
 *
 */

int s_access (path, amode)
char *path;
int amode;
{
    register int rtnval;
    extern int access ();			/* See access(2) */
    
    DBUG_ENTER ("s_access");
    DBUG_PRINT ("access", ("test '%s' for access %d", path, amode));
#ifdef amiga
    if ((rtnval = AccessRaw (path, amode)) != 0) {
	rtnval = access (path, amode);
    }
#else
    rtnval = access (path, amode);
#endif
    DBUG_PRINT ("access", ("result = %d", rtnval));
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_read    invoke library function to read from file
 *
 *  SYNOPSIS
 *
 *	int s_read (fildes, buf, nbyte)
 *	int fildes;
 *	char *buf;
 *	UINT nbyte;
 *
 *  DESCRIPTION
 *
 *	Invoke library function to read from file.
 *
 */

int s_read (fildes, buf, nbyte)
int fildes;
char *buf;
UINT nbyte;
{
    register int rtnval;
    extern int read ();			/* See read(2) */
    
#ifdef amiga
    if (IsRawFd (fildes)) {
	rtnval = ReadRaw (fildes, buf, nbyte);
    } else {
	rtnval = read (fildes, buf, nbyte);
    }
#else
    rtnval = read (fildes, buf, nbyte);
#endif
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_chown    invoke library function to change file owner
 *
 *  SYNOPSIS
 *
 *	int s_chown (path, owner, group)
 *	char *path;
 *	int owner, group;
 *
 *  DESCRIPTION
 *
 *	Invoke low level library routine to change ownership of file.
 *
 */

int s_chown (path, owner, group)
char *path;
int owner, group;
{
    register int rtnval;
    extern int chown ();			/* See chown(2) */

    DBUG_ENTER ("s_chown");
#if unix || xenix
    rtnval = chown (path, owner, group);
#else
    rtnval = 0;
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_sleep    invoke library function to sleep for N seconds
 *
 *  SYNOPSIS
 *
 *	VOID s_sleep (seconds)
 *	UINT seconds;
 *
 *  DESCRIPTION
 *
 *	Invoke library function to sleep for specified number of
 *	seconds.
 *
 */

VOID s_sleep (seconds)
UINT seconds;
{
#if unix || xenix
#if BSD4_2
    extern int sleep ();			/* See sleep(3C) */
#else
    extern UINT sleep ();			/* See sleep(3C) */
#endif
#endif
#ifdef amiga
    extern int Delay ();
#endif

    DBUG_ENTER ("s_sleep");
#if unix || xenix
    (VOID) sleep (seconds);
#endif
#ifdef amiga
    (VOID) Delay ((long) (50 * seconds));
#endif
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	s_uname    invoke library function to get system info
 *
 *  SYNOPSIS
 *
 *	int s_uname (name)
 *	struct my_utsname *name;
 *
 *  DESCRIPTION
 *
 *	Invoke library function to get information about host system.
 *
 *	Previous versions just either picked up a copy of utsname from
 *	the <sys/utsname.h> file, or the faked version.  In both cases
 *	the structure was identical, and of the same size.  Then along
 *	came xenix release 3.0, with a slightly different layout for
 *	the utsname structure, and I realised that it was a bad idea
 *	to blindly use the utsname structure directly in the declarations
 *	to describe archive blocks.  This is really an internal data
 *	structure containing system information, that simply happens
 *	to have a one-to-one mapping with a system structure on some
 *	systems, but not all.  Thus was born the my_utsname structure
 *	and the policy of filling the my_utsname structure from the
 *	actual contents of a real utsname structure, if available.
 *
 */

int s_uname (name)
struct my_utsname *name;
{
    register int rtnval;
#ifndef sgi
    extern int uname ();			/* See uname(2) */
#endif
    extern struct utsname utsname;
    extern char *s_strncpy ();
    
    DBUG_ENTER ("s_uname");
    rtnval = uname (&utsname);
    (VOID) s_strncpy (name -> sysname, utsname.sysname, UTSELSZ - 1);
    (VOID) s_strncpy (name -> nodename, utsname.nodename, UTSELSZ - 1);
    (VOID) s_strncpy (name -> release, utsname.release, UTSELSZ - 1);
    (VOID) s_strncpy (name -> version, utsname.version, UTSELSZ - 1);
#ifdef xenix
    (VOID) s_strncpy (name -> machine, "i80286", UTSELSZ - 1);
#else
    (VOID) s_strncpy (name -> machine, utsname.machine, UTSELSZ - 1);
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_umask    invoke library function to set/get file mask
 *
 *  SYNOPSIS
 *
 *	int s_umask (cmask)
 *	int cmask;
 *
 *  DESCRIPTION
 *
 *	Invoke library function to set and get the file creation mask.
 *
 */

int s_umask (cmask)
int cmask;
{
    register int rtnval;

    DBUG_ENTER ("s_umask");
#if unix || xenix
    rtnval = umask (cmask);
#else
    rtnval = 0;
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_utime    set file access and modification times
 *
 *  SYNOPSIS
 *
 *	int s_utime (path, times)
 *	char *path;
 *	struct {time_t x,y;} *times;
 *
 *  DESCRIPTION
 *
 *	Invoke the library function utime to set file access and
 *	modification times.
 *
 *  NOTES
 *
 *	The structure declaration is used to match that in the
 *	lint library /usr/lib/llib-lc, to shut up lint.
 *	The Unix System V User's Manual has the type of the
 *	second argument as a pointer to a structure "utimbuf",
 *	which is not declared anywhere in the header files.
 *
 *	For non-unix systems, this is currently a NOP, though
 *	in theory it is possible to change the date for systems
 *	like the Amiga by accessing the raw file system (yetch!).
 *
 */

/*VARARGS1*/
int s_utime (path, times)
char *path;
struct {time_t x, y;} *times;
{
    register int rtnval;
    extern int utime ();			/* See utime(2) */

    DBUG_ENTER ("s_utime");
    rtnval = utime (path, times);
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_chmod    change mode of file
 *
 *  SYNOPSIS
 *
 *	int s_chmod (path, mode)
 *	char *path;
 *	int mode;
 *
 *  DESCRIPTION
 *
 *	Invoke library function to change the access mode of a file.
 *
 *  NOTES
 *
 *	On some systems there is a real problem with lint concerning the
 *	type of the second argument (sometimes declared as unsigned short
 *	in the lint library).  The declaration used here matches the actual
 *	code in the system V library and the System V Interface Definition
 *	published by AT&T.  If your lint complains then either fix your
 *	library or complain to your vender.  This has been fixed in the
 *	UniPlus 5.2 lint library.
 *
 *	For the Amiga, the owner modes are used.  Delete permission
 *	is always set.
 *
 */

int s_chmod (path, mode)
char *path;
int mode;
{
    register int rtnval;
#if unix || (amiga && LATTICE)
#ifndef sgi
    extern int chmod ();			/* See chmod(2) */
#endif
#endif

    DBUG_ENTER ("s_chmod");
#if (amiga && LATTICE)
    mode >>= 5;
    mode &= 0xF;
    mode |= 1;
#endif
#if unix || (amiga && LATTICE)
    rtnval = chmod (path, mode);
#else
    rtnval = 0;
#endif
#if (amiga && LATTICE && VANILLA3_10)
    /*
     *  Bug in vanilla Lattice 3.10 always gives result of 0 for
     *  mode == 0, or gives 0 for failure and -1 for success for
     *  mode != 0.  So map these to correct returns...
     */
    if (mode != 0) {
	if (rtnval == 0) {
	    rtnval = -1;
	} else {
	    rtnval = 0;
	}
    }
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_write    write data to a file
 *
 *  SYNOPSIS
 *
 *	int s_write (fildes, buf, nbyte)
 *	int fildes;
 *	char *buf;
 *	UINT nbyte;
 *
 *  DESCRIPTION
 *
 *	Invoke the low level I/O routine to write to a file.
 *
 */

int s_write (fildes, buf, nbyte)
int fildes;
char *buf;
UINT nbyte;
{
    register int rtnval;
    extern int write ();			/* See write(2) */

#ifdef amiga
    if (IsRawFd (fildes)) {
	rtnval = WriteRaw (fildes, buf, nbyte);
    } else {
	rtnval = write (fildes, buf, nbyte);
    }
#else
    rtnval = write (fildes, buf, nbyte);
#endif
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_unlink    remove a directory entry
 *
 *  SYNOPSIS
 *
 *	int s_unlink (path)
 *	char *path;
 *
 *  DESCRIPTION
 *
 *	Invoke low level routine to remove a directory entry.
 *
 */

int s_unlink (path)
char *path;
{
    register int rtnval;
    extern int unlink ();			/* See unlink(2) */

    DBUG_ENTER ("s_unlink");
    rtnval = unlink (path);
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_time    get current system time
 *
 *  SYNOPSIS
 *
 *	long s_time (tloc)
 *	long *tloc;
 *
 *  DESCRIPTION
 *
 *	Invoke low level routine to get system time.
 *
 *	Note that BSD4_2 lint library defines both the argument and result
 *	as typedef time_t, which is an int, and the BSD4_2 online manual
 *	defines the types correctly (grrrrrrrr!!!!!).
 *
 */

long s_time (tloc)
long *tloc;
{
    register long rtnval;
#if BSD4_2 && lint
    extern time_t time ();			/* See time(2) */
#else
#ifndef sgi
    extern long time ();			/* See time(2) */
#endif
#endif
#ifdef MANX
    extern long manx2unix ();
#endif

    DBUG_ENTER ("s_time");
#if BSD4_2 && lint
    rtnval = (long) time ((time_t *) tloc);
#else
    rtnval = time (tloc);
#endif
#ifdef MANX
    rtnval = manx2unix (rtnval);
#endif
    DBUG_PRINT ("time", ("time is %ld", rtnval));
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_getuid    get real user ID of process
 *
 *  SYNOPSIS
 *
 *	int s_getuid ()
 *
 *  DESCRIPTION
 *
 *	Invoke low level routine to get the real user ID of process.
 *
 */

int s_getuid ()
{
    register int rtnval;
#ifndef sgi
    extern int getuid ();			/* See getuid(2) */
#endif

    DBUG_ENTER ("s_getuid");
#if unix || xenix
    rtnval = getuid ();
#else
    rtnval = 0;
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_getgid    get real group ID of process
 *
 *  SYNOPSIS
 *
 *	int s_getgid ()
 *
 *  DESCRIPTION
 *
 *	Invoke low level routine to get the real group ID of process.
 *
 */

int s_getgid ()
{
    register int rtnval;
#ifndef sgi
    extern int getgid ();			/* See getgid(2) */
#endif

    DBUG_ENTER ("s_getgid");
#if unix || xenix
    rtnval = getgid ();
#else
    rtnval = 0;
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_ctime    convert internal time to string
 *
 *  SYNOPSIS
 *
 *	char *s_ctime (clock)
 *	long *clock;
 *
 *  DESCRIPTION
 *
 *	Call standard library routine to convert from internal time
 *	format to string form.
 *
 *	Note that BSD4_2 lint library defines the argument as typedef
 *	time_t, which is an int, and the BSD4_2 online manual defines
 *	the type correctly (grrrrrrrr!!!!!).
 */

char *s_ctime (clock)
long *clock;
{
    register char *rtnval;
#ifdef MANX
    auto long myclock;
#endif
#ifndef sgi
    extern char *ctime ();			/* See ctime(3C) */
#endif

    DBUG_ENTER ("s_ctime");
#ifdef MANX
    myclock = unix2manx (*clock);
    clock = &myclock;
    DBUG_PRINT ("ctime", ("convert 'Manx' time %ld", *clock));
#endif
#if BSD4_2 && lint
    rtnval = ctime ((time_t *) clock);
#else
    rtnval = ctime (clock);
#endif
    DBUG_PRINT ("ctime", ("converted to '%s'", rtnval));
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_localtime    convert internal time to time structure
 *
 *  SYNOPSIS
 *
 *	struct tm *s_localtime (clock)
 *	long *clock;
 *
 *  DESCRIPTION
 *
 *	Call standard library routine to convert from internal time
 *	format to time structure form.
 *
 *	Note that BSD4_2 lint library defines the argument as "time_t *"
 *	where time_t is typedef as an int, and the BSD4_2 online manual defines
 *	the type correctly (grrrrrrrr!!!!!).
 */

struct tm *s_localtime (clock)
long *clock;
{
    register struct tm *rtnval;
#ifndef sgi
    extern struct tm *localtime ();		/* See ctime(3C) */
#endif
#ifdef MANX
    struct manx_tm *tmp;
    auto long myclock;
    static struct tm mytm;
#endif

    DBUG_ENTER ("s_localtime");
    DBUG_PRINT ("localtime", ("convert time %ld", *clock));
#ifdef MANX
    myclock = unix2manx (*clock);
    clock = &myclock;
    DBUG_PRINT ("localtime", ("convert 'Manx' time %ld", *clock));
#endif
#if BSD4_2 && lint
    rtnval = localtime ((time_t *) clock);
#else
    rtnval = localtime (clock);
#endif
#ifdef MANX
    tmp = (struct manx_tm *) rtnval;
    mytm.tm_sec = tmp -> tm_sec;
    mytm.tm_min = tmp -> tm_min;
    mytm.tm_hour = tmp -> tm_hour;
    mytm.tm_mday = tmp -> tm_mday;
    mytm.tm_mon = tmp -> tm_mon;
    mytm.tm_year = tmp -> tm_year;
    mytm.tm_wday = tmp -> tm_wday;
    mytm.tm_yday = tmp -> tm_wday;
    mytm.tm_isdst = tmp -> tm_isdst;
    rtnval = &mytm;
#endif    
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_asctime    convert time structure to ascii string
 *
 *  SYNOPSIS
 *
 *	char *s_asctime (tm)
 *	struct tm *tm;
 *
 *  DESCRIPTION
 *
 *	Call standard library routine to convert from time structure form
 *	to an ascii string.
 *
 */

char *s_asctime (tm)
struct tm *tm;
{
    register char *rtnval;
#ifndef sgi
    extern char *asctime ();		/* See ctime(3C) */
#endif
#ifdef MANX
    static struct manx_tm mytm;
#endif

    DBUG_ENTER ("s_asctime");
#ifdef MANX
    mytm.tm_sec = tm -> tm_sec;
    mytm.tm_min = tm -> tm_min;
    mytm.tm_hour = tm -> tm_hour;
    mytm.tm_mday = tm -> tm_mday;
    mytm.tm_mon = tm -> tm_mon;
    mytm.tm_year = tm -> tm_year;
    mytm.tm_wday = tm -> tm_wday;
    mytm.tm_yday = tm -> tm_wday;
    mytm.tm_isdst = tm -> tm_isdst;
    tm = (struct tm *) &mytm;
#endif
    rtnval = asctime (tm);
    DBUG_PRINT ("asctime", ("asctime returns '%s'", rtnval));
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_fopen    open a stream
 *
 *  SYNOPSIS
 *
 *	FILE *s_fopen (file_name, type)
 *	char *file_name;
 *	char *type;
 *
 *  DESCRIPTION
 *
 *	Invokes the standard library routine to open a stream.
 *
 */

FILE *s_fopen (file_name, type)
char *file_name;
char *type;
{
    register FILE *rtnval;
#ifndef sgi
    extern FILE *fopen ();			/* See fopen(3S) */
#endif
    
    DBUG_ENTER ("s_fopen");
    rtnval = fopen (file_name, type);
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_mknod    make a special or ordinary file
 *
 *  SYNOPSIS
 * (nuked the code that dealt with dirs; only used for special devices)
 *
 *	int s_mknod (path, mode, dev)
 *	char *path;
 *	int mode, dev;
 *
 *  DESCRIPTION
 *
 *	If called on a symbolic link, report an error, since that code
 *	should go through s_symlink and/or s_csymlink.
 *
 */

int s_mknod (path, mode, dev)
char *path;
int mode, dev;
{
    register int rtnval;
    extern VOID bru_error ();			/* Report an error to user */

    DBUG_ENTER ("s_mknod");
    DBUG_PRINT ("mknod", ("make node \"%s\" mode %o device %x", path, mode, dev));
    if (IS_FLNK (mode)) {
	bru_error (ERR_BUG, "mknod (symlink)");
	rtnval = FALSE;
    }
    else
	rtnval = mknod (path, mode, dev);
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_stat    get file status
 *
 *  SYNOPSIS
 *
 *	int s_stat (path, buf)
 *	char *path;
 *	struct stat64 *buf;
 *
 *  DESCRIPTION
 *
 *	Invoke low level routine to get file status.
 *
 *	Under 4.2BSD, we use the lstat routine instead, which
 *	will tell us whether or not we've found a symbolic link.
 *	An lstat on a normal file acts just like regular stat.
 *
 */

/*VARARGS1*/	/* Funny stuff with 2nd arg due to rmt.h define of stat */
int s_stat (path, buf)
char *path;
struct stat64 *buf;
{
    register int rtnval;
#if HAVE_SYMLINKS
#ifndef sgi
    extern int lstat64 ();			/* See stat(2) */
#endif
#else
    extern int stat64 ();				/* See stat(2) */
#endif

    DBUG_ENTER ("s_stat");
#if HAVE_SYMLINKS
    rtnval = lstat64 (path, buf);
#else
    rtnval = stat64 (path, buf);
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_fstat    get file status
 *
 *  SYNOPSIS
 *
 *	int s_fstat (fildes, buf)
 *	int fildes;
 *	struct stat64 *buf;
 *
 *  DESCRIPTION
 *
 *	Invoke low level routine to get file status, using an open file
 *	descriptor.  Currently only used by the directory access routines
 *	under USG, so we don't need a BSD equivalent (lfstat ?).
 *
 */

#if (!HAVE_SEEKDIR) || amiga
int s_fstat (fildes, buf)
int fildes;
struct stat64 *buf;
{
    register int rtnval;
#if unix || xenix
#ifndef sgi
    extern int fstat ();			/* See stat(2) */
#endif
#endif

    DBUG_ENTER ("s_fstat");
#if unix || xenix
    rtnval = fstat64 (fildes, buf);
#else
    (VOID) s_fprintf (stderr, "warning -- fstat() not implemented!\n");
    rtnval = SYS_ERROR;
#endif
    DBUG_RETURN (rtnval);
}
#endif  /* (!HAVE_SEEKDIR) || amiga */


/*
 *  FUNCTION
 *
 *	s_malloc    allocate memory from system
 *
 *  SYNOPSIS
 *
 *	char *s_malloc (size)
 *	UINT size;
 *
 *  DESCRIPTION
 *
 *	Call standard library routine to allocate memory.
 *
 */

char *s_malloc (size)
UINT size;
{
    register char *rtnval;
#ifndef sgi
    extern char *malloc ();			/* See malloc(3C) */
#endif

    rtnval = malloc (size);
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_exit    terminate process
 *
 *  SYNOPSIS
 *
 *	VOID s_exit (status)
 *	int status;
 *
 *  DESCRIPTION
 *
 *	Call the low level routine to terminate process.
 *
 */

VOID s_exit (status)
int status;
{
#if BSD4_2
    extern int exit ();			/* See exit(2) */
#else
#ifndef sgi
    extern VOID exit ();			/* See exit(2) */
#endif
#endif

    DBUG_ENTER ("s_exit");
#if BSD4_2
    (VOID) exit (status);
#else
    exit (status);
#endif
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	s_getopt    parse command line options
 *
 *  SYNOPSIS
 *
 *	int s_getopt (argc, argv, optstring)
 *	int argc;
 *	char **argv;
 *	char *optstring;
 *
 */

int s_getopt (argc, argv, optstring)
int argc;
char **argv;
char *optstring;
{
    register int rtnval;
#ifndef sgi
    extern int getopt ();			/* See getopt(3C) */
#endif

    DBUG_ENTER ("s_getopt");
    rtnval = getopt (argc, argv, optstring);
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_free    free memory received via malloc
 *
 *  SYNOPSIS
 *
 *	VOID s_free (ptr)
 *	char *ptr;
 *
 *  DESCRIPTION
 *
 *	Used to free memory allocated via the standard library
 *	memory allocator (malloc).
 *
 */

VOID s_free (ptr)
char *ptr;
{
#if BSD4_2
    extern int free ();			/* See malloc(3C) */
#else
#ifndef sgi
    extern VOID free ();			/* See malloc(3C) */
#endif
#endif

    DBUG_ENTER ("s_free");
#if BSD4_2
    (VOID) free (ptr);
#else
    free (ptr);
#endif
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	s_getpwent    get password file entry
 *
 *  SYNOPSIS
 *
 *	struct passwd *s_getpwent ()
 *
 *  DESCRIPTION
 *
 *	Invoke standard library routine to get a password file entry.
 *
 */

#ifndef unix
static BOOLEAN pwentopen = FALSE;
static struct passwd pwentfake = {
    "root",
    "NONE",
    0,
    0,
    "NONE",
    "NONE",
    "NONE",
    "/",
    "/bin/sh"
};
#endif

struct passwd *s_getpwent ()
{
    register struct passwd *rtnval;
#if unix || xenix
#ifndef sgi
    extern struct passwd *getpwent ();		/* See getpwent(3C) */
#endif
    rtnval = getpwent ();
#else
    if (pwentopen == TRUE) {
	rtnval = (struct passwd *)NULL;
    } else {
	pwentopen = TRUE;
	rtnval = &pwentfake;
    }
#endif
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_endpwent    close password file when done with it
 *
 *  SYNOPSIS
 *
 *	VOID s_endpwent ()
 *
 *  DESCRIPTION
 *
 *	Calls standard library function endpwent to close password
 *	file when done with it.
 *
 */

VOID s_endpwent ()
{
#if unix || xenix
#if BSD4_2
    extern int endpwent ();		/* See getpwent(3C) */
    (VOID) endpwent ();
#else
#ifndef sgi
    extern VOID endpwent ();		/* See getpwent(3C) */
#endif
    endpwent ();
#endif
#else
    pwentopen = FALSE;
#endif
}


/*
 *  FUNCTION
 *
 *	s_getgrent    get group file entry
 *
 *  SYNOPSIS
 *
 *	struct group *s_getgrent ()
 *
 *  DESCRIPTION
 *
 *	Invoke standard library routine to get a group file entry.
 *
 */

#ifndef unix
static BOOLEAN grentopen = FALSE;
static struct group grentfake = {
    "root",
    "NONE",
    0,
    NULL
};
#endif

struct group *s_getgrent ()
{
    register struct group *rtnval;
#if unix || xenix
#ifndef sgi
    extern struct group *getgrent ();		/* See getgrent(3C) */
#endif
    rtnval = getgrent ();
#else
    if (grentopen == TRUE) {
	rtnval = (struct group *)NULL;
    } else {
	grentopen = TRUE;
	rtnval = &grentfake;
    }
#endif
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_endgrent    close group file when done with it
 *
 *  SYNOPSIS
 *
 *	VOID s_endgrent ()
 *
 *  DESCRIPTION
 *
 *	Calls standard library function endpwent to close group
 *	file when done with it.
 *
 */

VOID s_endgrent ()
{
#if unix || xenix
#if BSD4_2
    extern int endgrent ();		/* See getgrent(3C) */
    (VOID) endgrent ();
#else
#ifndef sgi
    extern VOID endgrent ();		/* See getgrent(3C) */
#endif
    endgrent ();
#endif
#else
    grentopen = FALSE;
#endif
}


/*
 *  FUNCTION
 *
 *	s_signal    set up signal handling
 *
 *  SYNOPSIS
 *
 *	int (*s_signal (sig, func))()
 *	int sig;
 *	int (*func)();
 *
 *  DESCRIPTION
 *
 *	Set signal handling by calling low level routine "signal".
 *
 *  NOTES
 *
 *	There is considerable disagreement, between different implementations
 *	and proposed standards, for the declared type of signal() and it's
 *	second argument.  For example:
 *
 *		(1) System V Interface Definition from AT&T
 *
 *			int (*signal (sig, func)()) { int sig; int (*func)();
 *
 *		(2) Proposed ANSI C standard
 *
 *			void (*signal (sig, func)()) { int sig; void (*func)();
 *
 *		(3) 68000 System V generic release (Motorola Microport)
 *
 *			int (*signal (sig, func)()) { int sig; int (*func)();
 *			(in library)
 *
 *			int (*signal (sig, func)()) { int sig; void (*func)();
 *			(in lint and documentation for signal(2))
 *
 *	The inconsistency in (3) has been corrected in UniSoft's UniPlus
 *	based on the 68000 generic release, by fixing the lint library to
 *	be in agreement with the code and header files (change void to int).
 *
 *	If your lint complains about this then complain to your vendor!
 *
 */

int (*s_signal (sig, func))()
int sig;
#ifdef sgi
void (*func)();
#else
int (*func)();
#endif
{
    register int (*rtnval)();
#if unix || amiga
#ifndef sgi
    extern int (*signal())();		/* See signal(2) */
#endif
#endif

    DBUG_ENTER ("s_signal");
    DBUG_PRINT ("signals", ("set signal (%d,%lx)", sig, func));
#if unix || amiga
#ifdef sgi
    rtnval = (int (*)())signal (sig, func);
#else
    rtnval = signal (sig, func);
#endif
#else
    DBUG_PRINT ("signals", ("signals not implemented in this system"));
    rtnval = (int (*)()) SYS_ERROR;
#endif
    DBUG_PRINT ("signals", ("signal returns %d", rtnval));
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_ioctl    issue low level function to control device
 *
 *  SYNOPSIS
 *
 *	int s_ioctl (fildes, request, arg)
 *	int fildes;
 *	int request;
 *	int arg;
 *
 *  DESCRIPTION
 *
 *	Issue the low level request to control device.
 *
 */

int s_ioctl (fildes, request, arg)
int fildes, request, arg;
{
    register int rtnval;
#if unix || xenix
#ifndef sgi
    extern int ioctl ();			/* See ioctl(2) */
#endif
#endif

    DBUG_ENTER ("s_ioctl");
#if unix || xenix
#if BSD4_2
    rtnval = ioctl (fildes, request, (char *) arg);
#else
    rtnval = ioctl (fildes, request, arg);
#endif
#else
    (VOID) s_fprintf (stderr, "warning -- ioctl() not implemented!\n");
    rtnval = SYS_ERROR;
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_strcat    concatenate two strings
 *
 *  SYNOPSIS
 *
 *	char *s_strcat (s1, s2)
 *	char *s1, *s2;
 *
 *  DESCRIPTION
 *
 *	Append copy of string s2 to end of string s1.
 *
 */

char *s_strcat (s1, s2)
char *s1, *s2;
{
    register char *rtnval;
#ifndef sgi
    extern char *strcat ();
#endif
    
    rtnval = strcat (s1, s2);
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_strcmp    compare two strings lexicographically
 *
 *  SYNOPSIS
 *
 *	int s_strcmp (s1, s2)
 *	char *s1, *s2;
 *
 *  DESCRIPTION
 *
 *	Compare string s1 with string s2 and return an integer less
 *	than, equal to, or greater than zero according as s1 is
 *	lexicographically less than, equal to, or greater than s2.
 *
 */

int s_strcmp (s1, s2)
char *s1, *s2;
{
    register int rtnval;
#ifndef sgi
    extern int strcmp ();
#endif

    rtnval = strcmp (s1, s2);
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_strcpy    copy strings from one location to another
 *
 *  SYNOPSIS
 *
 *	char *s_strcpy (s1, s2)
 *	char *s1, *s2;
 *
 *  DESCRIPTION
 *
 *	Copy string s2 to s1, stopping after the null character has been
 *	copied.
 *
 */

char *s_strcpy (s1, s2)
char *s1, *s2;
{
    register char *rtnval;
#ifndef sgi
    extern char *strcpy ();
#endif
    
    rtnval = strcpy (s1, s2);
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_strncpy    copy strings from one location to another
 *
 *  SYNOPSIS
 *
 *	char *s_strncpy (s1, s2, n)
 *	char *s1, *s2;
 *	int n;
 *
 *  DESCRIPTION
 *
 *	Copy n characters from string s2 to s1, truncating s2 or
 *	padding s1 with nulls as required.
 *
 */

char *s_strncpy (s1, s2, n)
char *s1, *s2;
int n;
{
    register char *rtnval;
#ifndef sgi
    extern char *strncpy ();
#endif
    
    rtnval = strncpy (s1, s2, n);
    return (rtnval);
}



/*
 *  FUNCTION
 *
 *	s_strlen    find length of string
 *
 *  SYNOPSIS
 *
 *	int s_strlen (s)
 *	char *s;
 *
 *  DESCRIPTION
 *
 *	Count characters in string s, not including terminating null.
 *
 */

int s_strlen (s)
char *s;
{
    register int rtnval;
#ifndef sgi
    extern int strlen ();
#endif

    rtnval = strlen (s);
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_strchr    find first occurance of character in string
 *
 *  SYNOPSIS
 *
 *	char *s_strchr (s, c)
 *	char *s;
 *	char c;
 *
 *  DESCRIPTION
 *
 *	Search forward in string s until first occurance of
 *	string character c, returning pointer.
 *
 */

#if BSD4_2
#define strchr index
#endif

char *s_strchr (s, c)
char *s;
char c;
{
    register char *rtnval;
#if unix || xenix
#ifndef sgi
    extern char *strchr ();
#endif
    rtnval = strchr (s, c);
#else
    rtnval = NULL;
    if (s != NULL) {
	while (*s != EOS && *s != c) {s++;}
	if (*s == c) {
	    rtnval = s;
	}
    }
#endif
    return (rtnval);
}



/*
 *  FUNCTION
 *
 *	s_strrchr    find last given character in string
 *
 *  SYNOPSIS
 *
 *	char *s_strrchr (s, c)
 *	char *s;
 *	char c;
 *
 *  DESCRIPTION
 *
 *	Find last occurance of the given character c in the string s.
 *
 */

#if BSD4_2
#define strrchr rindex
#endif

char *s_strrchr (s, c)
char *s;
char c;
{
    register char *rtnval;
#if unix || xenix
#ifndef sgi
    extern char *strrchr ();
#endif
    rtnval = strrchr (s, c);
#else
    if (s == NULL) {
	rtnval = NULL;
    } else {
	rtnval = s;
	while (*s != EOS) {s++;}
	while (s > rtnval && *s != c) {s--;}
	if (*s == c) {
	    rtnval = s;
	} else {
	    rtnval = NULL;
	}
    }
#endif
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_mkdir    make a directory node
 *
 *  SYNOPSIS
 *
 *	int s_mkdir (path, mode)
 *	char *path;
 *	int mode;
 *
 *  DESCRIPTION
 *
 *	Contains code to make a directory node and its entries
 *	"." and ".."
 *
 *	This is a system call under 4.2, so we just use it.
 *	Also just a system call under UniPlus with NFS.
 *
 *	Note that the effective user id must be root for this
 *	to work.  Thus this is only usable by root or if the
 *	process is owned by root and has the SUID bit set.
 *
 *	As an alternative, the system "mkdir" command could
 *	be forked but this is much faster.  This is probably
 *	fertile ground for anyone looking for system security
 *	loopholes.
 *
 *	Note that the parent directory must be writable by the
 *	real user.
 *
 *	Under 4.2, 'mkdir' is a system call, so this routine will just
 *	use the system call.  Using the system call should help
 *	security issues too.  Similarly UniPlus with NFS...
 *
 *	Note, this routine may not be completely bulletproof.  How
 *	will it behave if the name ends with a '/', as in
 *	"/usr/bin/"?  How will BSD4.2 mkdir() behave in this case?
 *
 */

/* the old hack way of doing it is in the RCS history, if you
 * ever need it... */
int s_mkdir (path, mode)
char *path;
int mode;
{
    register int rtnval;

    DBUG_ENTER ("s_mkdir");
    rtnval = mkdir (path, mode);
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_lseek    seek to location in file
 *
 *  SYNOPSIS
 *
 *	S32BIT s_lseek (fildes, offset, whence)
 *	int fildes;
 *	S32BIT offset;
 *	int whence;
 *
 *  DESCRIPTION
 *
 *	Invoke the standard library function to seek to file
 *	location.
 *
 */

S32BIT s_lseek (fildes, offset, whence)
int fildes;
S32BIT offset;
int whence;
{
    register S32BIT rtnval;
#ifndef sgi
    extern S32BIT lseek ();			/* See lseek(2) */
#endif
    
    DBUG_ENTER ("s_lseek");
#ifdef amiga
    if (IsRawFd (fildes)) {
	rtnval = LseekRaw (fildes, offset, whence);
    } else {
	rtnval = lseek (fildes, offset, whence);
    }
#else
    rtnval = lseek (fildes, offset, whence);
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_rewind    allow mixed read/writes on stream
 *
 *  SYNOPSIS
 *
 *	VOID s_rewind (stream)
 *	FILE *stream;
 *
 *  DESCRIPTION
 *
 *	Interface to the system "rewind" call, which must
 *	be issued each time the file access mode changes from
 *	read to write and vice-versa.
 *
 */

VOID s_rewind (stream)
FILE *stream;
{
#ifndef rewind					/* Sometimes macro */
#if BSD4_2
    extern int rewind ();			/* See fseek(3S) */
#else
#ifndef sgi
    extern VOID rewind ();			/* See fseek(3S) */
#endif
#endif
#endif

#if BSD4_2
    (VOID) rewind (stream);
#else
#ifndef MANX	/* Hack for possible manx bug on the Amiga */
    rewind (stream);
#endif
#endif
}


int s_tolower (c)
int c;
{
#ifndef tolower					/* A macro on some systems */
#ifndef sgi
    extern int tolower ();			/* See conv(3C) */
#endif
#endif

#if BSD4_2
    /* BSD tolower blindly converts, while S5 checks, so we check for it */
    if (!isupper (c)) {
	return (c);
    } else {
	return (tolower (c));
    }
#else
    return (tolower (c));
#endif
}


int s_creat (path, mode)
char *path;
int mode;
{
    register int rtnval;
#ifndef sgi
    extern int creat ();			/* See creat(2) */
#endif
    
    DBUG_ENTER ("s_creat");
    rtnval = creat (path, mode);
    DBUG_RETURN (rtnval);
}


int s_isdigit (c)
int c;
{
    return (isdigit (c));
}


int s_atoi (str)
char *str;
{
    register int rtnval;
#ifndef sgi
    extern int atoi ();			/* See strtol(3C) */
#endif
    
    DBUG_ENTER ("s_atoi");
    rtnval = atoi (str);
    DBUG_RETURN (rtnval);
}


int s_fileno (stream)
FILE *stream;
{
    return (fileno (stream));
}


int s_getc (stream)
FILE *stream;
{
    DBUG_ENTER ("s_getc");
    DBUG_RETURN ((int) getc (stream));
}


char *s_memcpy (s1, s2, n)
char *s1, *s2;
int n;
{
#if BSD4_2
    (VOID) bcopy (s2, s1, n);
    return (s1);
#else
#if xenix || (amiga && MANX)
    char *saved_s1 = s1;
    while (n-- > 0) {
	*s1++ = *s2++;
    }
    return (saved_s1);
#else
#ifndef sgi
    extern char *memcpy ();			/* See memory(3C) */
#endif
    return (memcpy (s1, s2, n));
#endif
#endif
}


int s_link (path1, path2)
char *path1, *path2;
{
    register int rtnval;
#if unix || xenix
#ifndef sgi
    extern int link ();			/* See link(2) */
#endif
#endif
    
    DBUG_ENTER ("s_link");
    DBUG_PRINT ("link", ("link \"%s\" to existing \"%s\"", path2, path1));
#if unix || xenix
    rtnval = link (path1, path2);
#else
    rtnval = SYS_ERROR;		/* No obvious way to fake it */
#endif
    DBUG_RETURN (rtnval);
}


int s_putc (c, stream)
char c;
FILE *stream;
{
    return (putc (c, stream));
}


/*
 *	For the amiga version, we only need to fake a dup for
 *	stdin, stdout, or stderr by simply returning the given fildes.
 *	This should work ok for what we want to do with it...
 */

int s_dup (fildes)
int fildes;
{
    register int rtnval;
    
    DBUG_ENTER ("s_dup");
#if unix || xenix
    rtnval = dup (fildes);
#else
#if amiga
    if (fildes == 0 || fildes == 1 || fildes == 2) {
	rtnval = fildes;
    } else {
	errno = EBADF;
	rtnval = SYS_ERROR;
    }
#else
    (VOID) s_fprintf (stderr, "warning -- dup() not implemented!\n");
    rtnval = SYS_ERROR;
#endif
#endif
    DBUG_RETURN (rtnval);
}


char *s_getenv (name)
char *name;
{
    register char *rtnval;
#ifndef sgi
    extern char *getenv ();			/* See getenv(3C) */
#endif
    
    DBUG_ENTER ("s_getenv");
    rtnval = getenv (name);
    DBUG_RETURN (rtnval);
}


int s_fork ()
{
    register int rtnval;
#ifndef sgi
    extern int fork ();			/* See fork(2) */
#endif
    
    DBUG_ENTER ("s_fork");
#if unix || xenix
    rtnval = fork ();
#else
    rtnval = SYS_ERROR;
#endif
    DBUG_RETURN (rtnval);
}


int s_wait (stat_loc)
int *stat_loc;
{
    register int rtnval;
#ifndef sgi
    extern int wait ();			/* See wait(2) */
#endif
    
    DBUG_ENTER ("s_wait");
#if BSD4_2
    rtnval = wait ((union wait *) stat_loc);
#else
    rtnval = wait (stat_loc);
#endif
    DBUG_RETURN (rtnval);
}


/*
 *	The BSD4_2 lint library has a totally screwed up entry for execv.
 *	It has no explicitly declared return type, so defaults to int, but
 *	then does not explicitly return anything so lint complains about
 *	any code that tries to use the return value.
 */

int s_execv (path, argv)
char *path;
char *argv[];
{
    register int rtnval;
#ifndef sgi
    extern int execv ();			/* See exec(2) */
#endif
    
    DBUG_ENTER ("s_execv");
#if BSD4_2 && lint
    (VOID) execv (path, argv);			/* Lint library lies! */
    rtnval = -1;
#else
#ifdef amiga
    (VOID) s_fprintf(stderr, "Sorry, execv not yet implemented!\n");
    rtnval = -1;
#else
    rtnval = execv (path, argv);
#endif
#endif
    DBUG_RETURN (rtnval);
}



int s_fclose (stream)
FILE *stream;
{
    register int rtnval;
#ifndef sgi
    extern int fclose ();			/* See fclose(3S) */
#endif
    
    DBUG_ENTER ("s_fclose");
    rtnval = fclose (stream);
    DBUG_RETURN (rtnval);
}


char *s_fgets (s, n, stream)
char *s;
int n;
FILE *stream;
{
    register char *rtnval;
#ifndef sgi
    extern char *fgets ();			/* See gets(3S) */
#endif
    
    rtnval = fgets (s, n, stream);
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_readlink    read a symbolic link
 *
 *  SYNOPSIS
 *
 *	int s_readlink (name, buf, size);
 *	char *name;
 *	char *buf;
 *	int size;
 *
 *  DESCRIPTION
 *
 *	Read a symbolic link on systems that have symbolic links.
 *	Is a NOP on other systems and just returns SYS_ERROR.
 *
 *	Under SUN's 3.2 OS, readlink() may return garbage if the first
 *	argument is one of the special files in /dev.  So, we must
 *	avoid calling readlink for anything except real symbolic links.
 *	Readlink is documented to leave ENXIO in errno if the name is
 *	not a symbolic link, so that's what we do also...
 *
 */

/*ARGSUSED*/	/* Turn off lint checking for unused args in s_readlink */

int s_readlink (name, buf, size)
char *name, *buf;
int size;
{
    register int rtnval;
#if HAVE_SYMLINKS
    struct stat64 sbuf;			/* Used to avoid SUN 3.2 bug */
#ifndef sgi
    extern int readlink ();		/* See readlink(2) */
#endif
    extern int errno;			/* System error return code */
#endif

    DBUG_ENTER ("s_readlink");
#if HAVE_SYMLINKS
    if (s_stat (name, &sbuf) == SYS_ERROR) {
	rtnval = SYS_ERROR;
    } else {
	if (IS_FLNK (sbuf.st_mode)) {
	    rtnval = readlink (name, buf, size);
	} else {
	    errno = ENXIO;
	    rtnval = SYS_ERROR;
	}
    }
#else
    rtnval = SYS_ERROR;
#endif
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	s_timezone    get local time zone information
 *
 *	S5 defines an extern long timezone == difference in seconds between
 *	GMT and local standard time. BSD does not have this extern, but it
 *	will tell us what our timezone is in minutes west of GMT. So we
 *	just compute it on the fly.
 *
 */

#define SEC_PER_MIN (60)

#if BSD4_2
long s_timezone ()
{
    auto long timezone;
    struct timeval tv;
    struct timezone tz;

    (VOID) gettimeofday (&tv, &tz);
    timezone = tz.tz_minuteswest * SEC_PER_MIN;
    return (timezone);
}
#endif

#if USG5 || (amiga && LATTICE)
long s_timezone ()
{
#ifndef sgi
    extern long timezone;
    extern VOID tzset ();
#endif

    tzset ();
    return (timezone);
}
#endif

#if amiga && MANX
long s_timezone ()
{
    return (0L);
}
#endif


#if HAVE_SYMLINKS

/*
 * Supply these routines on our own for 4.2.  Make them static (if
 * appropriate) to keep this stuff all in one file.
 */

/* s_symlink --- make a symbolic link */

int s_symlink (name1, name2)
char *name1, *name2;
{
    register int rtnval;
#ifndef sgi
    extern int symlink ();			/* See symlink(2) */
#endif

    DBUG_ENTER ("s_symlink");
    rtnval = symlink (name1, name2);
    DBUG_RETURN (rtnval);
}

#ifdef pyr

/* s_csymlink --- make a conditional symbolic link */

int s_csymlink (name1, name2)
char *name1, *name2;
{
    register int rtnval;
    extern int csymlink ();			/* See csymlink(2) for OSx */

    DBUG_ENTER ("s_symlink");
    rtnval = csymlink (name1, name2);
    DBUG_RETURN (rtnval);
}

#endif	/* pyr */
#endif	/* HAVE_SYMLINKS */

#if AUX
#include <sys/ssioctl.h>	/* Contains special ioctl's for Apple A/UX */
#endif

int s_eject (fildes)
int fildes;
{
    int rtnval = 0;

    DBUG_ENTER ("s_eject");
#if AUX
    rtnval = s_ioctl (fildes, AL_EJECT, 0);
#endif
    DBUG_RETURN (rtnval);
}

#if AUX
#include <fcntl.h>
#include <sys/diskformat.h>
#include <sys/ioctl.h>
#include <errno.h>
#endif

s_format (fildes)
int fildes;
{
    int status = 0;
#if AUX
    auto struct diskformat fmt;

    fmt.d_secsize = 512;
    fmt.d_dens = 2;
    fmt.d_fsec = 0;
    fmt.d_lsec = 1599;
    fmt.d_fhead = 0;
    fmt.d_lhead = 1;
    fmt.d_fcyl = 0;
    fmt.d_lcyl = 79;
    fmt.d_ileave = 1;
    status = !(ioctl (fildes, UIOCFORMAT, &fmt) == -1);
#endif
    return (status);
}

