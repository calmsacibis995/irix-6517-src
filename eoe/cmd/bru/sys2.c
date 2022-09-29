/*
 *	This file contains intersystem compatibility routines derived from
 *	various public domain sources.  This file is not subject to any of
 *	the licensing or distribution restrictions applicable to the rest
 *	of the bru source code.  It is provided to improved transportability
 *	of bru source code between various flavors of unix.
 *
 *	Fred Fish, Oct 1985.
 */
  
/*
 *	NOTE:  The directory access routines are derived from a
 *	public domain version of the BSD compatible directory access
 *	library by Doug Gwyn at BRL.  They are not subject to any distribution
 *	restrictions and Doug's contribution to the Unix community
 *	is gratefully acknowledged.  They are provided for
 *	convenience in using the bru source code on non-BSD systems.
 *	They have been reformatted to fit in with the style in the
 *	rest of the bru source code but other than that are essentially
 *	unchanged.
 *
 *	To disable use of these internal routines, and use the equivalent
 *	in a library available on your system, define "HAVE_SEEKDIR" in
 *	either CFLAGS in the makefile or in autoconfig.h.
 */

#include "autoconfig.h"

#include <stdio.h>
#include "typedefs.h"
#include "config.h"
#include "dbug.h"

#if unix && !HAVE_SEEKDIR

#include "dir.h"
#if unix || xenix
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  include "sys.h"
#endif
  
extern VOID *get_memory ();
/* extern char *s_malloc (); */

extern int s_open ();
extern int s_fstat ();
extern VOID s_free ();
extern int s_close ();
extern int s_read ();
extern char *s_strncpy ();
extern int s_strlen ();
extern long s_lseek ();

#ifndef NULL
#define NULL 0
#endif

#define DIRSIZ	14

struct olddir {
    ino_t od_ino;		/* inode */
    char od_name[DIRSIZ];	/* filename */
};


/*
 *	closedir -- C library extension routine
 *
 *	last edit:	21-Jan-1984	D A Gwyn
 */

VOID closedir (dirp)
register DIR *dirp;		/* stream from opendir() */
{
    (VOID) s_close (dirp -> dd_fd);
    s_free ((char *) dirp);
}


/*
 *	opendir -- C library extension routine
 *
 *	last edit:	09-Jul-1983	D A Gwyn
 */

DIR *opendir (filename)
char *filename;			/* name of directory */
{
    register DIR *dirp;		/* -> malloc'ed storage */
    register int fd;		/* file descriptor for read */
    struct stat64 sbuf;		/* result of fstat() */

    if ((fd = s_open (filename, 0, 0)) < 0) {
	return (NULL);
    }
    if (s_fstat (fd, &sbuf) < 0
	    || (sbuf.st_mode & S_IFMT) != S_IFDIR
	    || (dirp = (DIR *) get_memory (sizeof (DIR), 0)) == NULL
	) {
	(VOID) s_close (fd);
	return (NULL);		/* bad luck today */
    }
    dirp -> dd_fd = fd;
    dirp -> dd_loc = dirp -> dd_size = 0;	/* refill needed */
    return (dirp);
}


/*
 *	readdir -- C library extension routine for non-BSD UNIX
 *
 *	last edit:	09-Jul-1983	D A Gwyn
 */

struct direct *readdir (dirp)
register DIR *dirp;			/* stream from opendir() */
{
    register struct olddir *dp;		/* -> directory data */

    for (;;) {
	if (dirp -> dd_loc >= dirp -> dd_size) {
	    dirp -> dd_loc = dirp -> dd_size = 0;
	}
	if (dirp -> dd_size == 0	/* refill buffer */
		&& (dirp -> dd_size = s_read (dirp -> dd_fd, dirp -> dd_buf,
			DIRBLKSIZ
		    )
		) <= 0
	    )
	    return (NULL);	/* error or EOF */
	dp = (struct olddir *) &(dirp -> dd_buf[dirp -> dd_loc]);
	dirp -> dd_loc += sizeof (struct olddir);
	if (dp -> od_ino != 0) {		/* not deleted entry */
	    static struct direct dir;		/* simulated */
	    dir.d_ino = dp -> od_ino;
	    (VOID) s_strncpy (dir.d_name, dp -> od_name, DIRSIZ);
	    dir.d_name[DIRSIZ] = '\0';
	    dir.d_namlen = s_strlen (dir.d_name);
	    dir.d_reclen = sizeof (struct direct) - MAXNAMLEN + 3;
	    dir.d_reclen += dir.d_namlen - dir.d_namlen % 4;
	    return (&dir);		/* -> simulated structure */
	}
    }
}


/*
 *	seekdir -- C library extension routine
 *
 *	last edit:	21-Jan-1984	D A Gwyn
 */

VOID seekdir (dirp, loc)
register DIR *dirp;		/* stream from opendir() */
long loc;			/* position from telldir() */
{
    long base;			/* file location of block */
    long offset;		/* offset within block */

    if (telldir (dirp) == loc) {
	return;			/* save time */
    }
    offset = loc % DIRBLKSIZ;
    base = loc - offset;
    (VOID) s_lseek (dirp -> dd_fd, base, 0);	/* change blocks */
    dirp -> dd_loc = dirp -> dd_size = 0;
    while (dirp -> dd_loc < offset) {		/* skip entries */
	if (readdir (dirp) == NULL) {
	    return;				/* "can't happen" */
	}
    }
}


/*
 *	telldir -- C library extension routine
 *
 *	last edit:	09-Jul-1983	D A Gwyn
 */

long telldir (dirp)
DIR *dirp;			/* stream from opendir() */
{
    return (s_lseek (dirp -> dd_fd, 0L, 1) - (long) dirp -> dd_size
	+ (long) dirp -> dd_loc);
}

#endif  /* !HAVE_SEEKDIR */


#if BSD4_2 || xenix || amiga
#include "manifest.h"
#include <ctype.h>
#endif

#if BSD4_2 || amiga

#ifndef amiga
#include <errno.h>
#endif

#include "utsname.h"

extern int errno;
extern char *s_strcpy ();
extern char *s_strncpy ();

/* strtok --- s1 is tokens separated by stuff in s2, see S5 man page */

char *strtok (s1, s2)
char *s1, *s2;
{
    static char *save = NULL;	/* save the string between calls */
    char *cp;
    extern char *s_strchr ();

    DBUG_ENTER ("strtok");
    DBUG_PRINT ("strtok", ("s1 = <%s>, s2 = <%s>", s1, s2));

    if (s1 != NULL) {		/* first call */
    	save = s1;
    } else if (save == NULL || *save == EOS) {	/* no saved string, or empty */
    	DBUG_RETURN ((char *) NULL);
    }
    
    for (; *save && s_strchr (s2, *save) != NULL; save++) { ; }
		/* skip leading token separators */
    
    if (*save == EOS) {
    	save = NULL;
    	DBUG_RETURN ((char *) NULL);
    }

    cp = save;		/* save start of string */
    while (*save != EOS && s_strchr (s2, *save) == NULL) {
   	 save++;	/* skip non separators (the body of the token) */
    }
    
    if (*save == EOS) {
    	if (cp == save) {	/* do the test before clearing save */
    	    cp = save = NULL;
	}
    	/* else cp is ok */
    	DBUG_RETURN (cp);
    } else {
    	*save++ = EOS;
    }
    /* clobber trailing separator, and increment save */
    if (cp == save) {
	cp = NULL;
    }
    DBUG_RETURN (cp);
}

#endif	/* BSD4_2 */


/* strtol --- convert an arbitrary base number to a long int, see S5 manual */

#if BSD4_2 || xenix || amiga

long strtol (str, ptr, base)
char *str, **ptr;
int base;
{
    long val = 0L;
    int sign = FALSE;
    int converted = FALSE;
    char *save = str;
    int lim;				/* limiting character not in base */

    DBUG_ENTER ("strtol");

    while (isspace (*str)) {
	str++;
    }
    if (*str == '-') {
        str++;
        sign = TRUE;
    } else if (*str == '+') {
        str++;
    }
    if (base < 0 || base > 36) {
        base = 0;    /* don't use it */
    }
    if (base == 0) {
        if (*str == '0' && s_tolower (str[1]) == 'x') {
            str += 2;
            base = 16;
        } else if (*str == '0') {
            base = 8;
        } else {
            base = 10;
	}
    } else if (base == 16 && *str == '0' && s_tolower (str[1]) == 'x') {
        str += 2;
    }
    if (base == 10) {
	lim = '9' + 1;
    } else if (base > 10) {
        lim = base - 10 + 'a';	/* >= lim is not range */
    } else {
        lim = base + '0';
    }
    DBUG_PRINT ("strtol", ("base %d lim '%c'", base, lim));
    while (*str == '0') {
        str++;
    }
    for (; *str; str++) {
	DBUG_PRINT ("strtol", ("examining '%c'", *str));
        if (! isalnum (*str) || s_tolower(*str) >= lim
                || s_tolower(*str) < '0') {
            break;
	}
        val *= base;
        if (isalpha (*str)) {
            val += s_tolower (*str) - 'a';
	} else {
            val += *str - '0';
	}
        converted = TRUE;
    }
    if (sign) {
        val = -val;
	DBUG_PRINT ("strtol", ("number is negative"));
    }
    if (ptr != NULL) {
        if (! converted) {
            *ptr = save;
            val = 0L;
        } else {
            *ptr = str;
	}
    }
    DBUG_RETURN (val);
}
#endif /* BSD4_2 or xenix */


#if BSD4_2 || amiga

/*
 * From std-unix@ut-sally.UUCP (Moderator, John Quarterman) Sun Nov  3 14:34:15 1985
 * Relay-Version: version B 2.10.3 4.3bsd-beta 6/6/85; site gatech.CSNET
 * Posting-Version: version B 2.10.2 9/18/84; site ut-sally.UUCP
 * Path: gatech!akgua!mhuxv!mhuxt!mhuxr!ulysses!allegra!mit-eddie!genrad!panda!talcott!harvard!seismo!ut-sally!std-unix
 * From: std-unix@ut-sally.UUCP (Moderator, John Quarterman)
 * Newsgroups: mod.std.unix
 * Subject: public domain AT&T getopt source
 * Message-ID: <3352@ut-sally.UUCP>
 * Date: 3 Nov 85 19:34:15 GMT
 * Date-Received: 4 Nov 85 12:25:09 GMT
 * Organization: IEEE/P1003 Portable Operating System Environment Committee
 * Lines: 91
 * Approved: jsq@ut-sally.UUCP
 * 
 * Here's something you've all been waiting for:  the AT&T public domain
 * source for getopt(3).  It is the code which was given out at the 1985
 * UNIFORUM conference in Dallas.  I obtained it by electronic mail
 * directly from AT&T.  The people there assure me that it is indeed
 * in the public domain.
 * 
 * There is no manual page.  That is because the one they gave out at
 * UNIFORUM was slightly different from the current System V Release 2
 * manual page.  The difference apparently involved a note about the
 * famous rules 5 and 6, recommending using white space between an option
 * and its first argument, and not grouping options that have arguments.
 * Getopt itself is currently lenient about both of these things White
 * space is allowed, but not mandatory, and the last option in a group can
 * have an argument.  That particular version of the man page evidently
 * has no official existence, and my source at AT&T did not send a copy.
 * The current SVR2 man page reflects the actual behavor of this getopt.
 * However, I am not about to post a copy of anything licensed by AT&T.
 * 
 * I will submit this source to Berkeley as a bug fix.
 * 
 * I, personally, make no claims or guarantees of any kind about the
 * following source.  I did compile it to get some confidence that
 * it arrived whole, but beyond that you're on your own.
 * 
 */

/*LINTLIBRARY*/

#ifndef NULL
#define NULL	0
#endif

#ifndef EOF
#define EOF	(-1)
#endif

#define ERR(s, c)	if(opterr){\
	extern int s_strlen(), s_write();\
	char errbuf[2];\
	errbuf[0] = c; errbuf[1] = '\n';\
	(VOID) s_write(2, argv[0], (unsigned)s_strlen(argv[0]));\
	(VOID) s_write(2, s, (unsigned)s_strlen(s));\
	(VOID) s_write(2, errbuf, (unsigned)2);}

extern int s_strcmp();
extern char *s_strchr();

int	opterr = 1;
int	optind = 1;
int	optopt;
char	*optarg;

int
getopt(argc, argv, opts)
int	argc;
char	**argv, *opts;
{
	static int sp = 1;
	register int c;
	register char *cp;

	if(sp == 1)
		if(optind >= argc ||
		   argv[optind][0] != '-' || argv[optind][1] == '\0')
			return(EOF);
		else if(s_strcmp(argv[optind], "--") == NULL) {
			optind++;
			return(EOF);
		}
	optopt = c = argv[optind][sp];
	if(c == ':' || (cp=s_strchr(opts, c)) == NULL) {
		ERR(": illegal option -- ", c);
		if(argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		return('?');
	}
	if(*++cp == ':') {
		if(argv[optind][sp+1] != '\0')
			optarg = &argv[optind++][sp+1];
		else if(++optind >= argc) {
			ERR(": option requires an argument -- ", c);
			sp = 1;
			return('?');
		} else
			optarg = argv[optind++];
		sp = 1;
	} else {
		if(argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return(c);
}


/*
	uname -- system call emulation for 4.2BSD

	based on System V Emulation from BRL
	original's last edit:	05-Mar-1984	D A Gwyn

	last edit:	18-Jun-1985	Arnold Robbins
*/

/* note that all stuff which originates at BRL is public domain */

static int s_gethostname ();

#ifndef EFAULT
#define EFAULT 0
#endif

int uname (name)
register struct utsname *name;	/* where to put results */
{
	register char *np;	/* -> array being cleared */

	if (name == NULL) {
		errno = EFAULT;
		return -1;
	}

	for (np = name->nodename; np < &name->nodename[sizeof(name->nodename)];
				np++)
		*np = '\0';		/* for cleanliness */

	if (s_gethostname (name->nodename, sizeof (name->nodename)) != 0)
		(VOID) s_strcpy (name->nodename, "unknown");

	(VOID) s_strncpy (name->sysname, name->nodename, sizeof (name->sysname));

#ifdef amiga
	(VOID) s_strncpy (name->release, "AmigaDos", sizeof (name->release));
#else
	(VOID) s_strncpy (name->release, "4.2BSD", sizeof (name->release));
#endif
	(VOID) s_strncpy (name->version, "vm", sizeof (name->version));

	(VOID) s_strncpy (name->machine,
#ifdef	interdata
		       "interdata",
#else
#ifdef	pdp11
		       "pdp11",
#else
#ifdef	sun
		       "sun",
#else
#ifdef	u370
		       "u370",
#else
#ifdef	u3b
		       "u3b",
#else
#ifdef	vax
		       "vax",
#else
#ifdef	pyr
		       "pyramid",
#else
#ifdef	amiga
		       "amiga",
#else
		       "unknown",
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
		       sizeof name->machine);

	/* add a trailing end of string, just to be sure -- ADR */
	name->sysname [sizeof (name->sysname) - 1] = '\0';
	name->nodename [sizeof (name->nodename) - 1] = '\0';
	name->release [sizeof (name->release) - 1] = '\0';
	name->version [sizeof (name->version) - 1] = '\0';
	name->machine [sizeof (name->machine) - 1] = '\0';

	return 0;
}

extern int gethostname();

static int s_gethostname (name, namelen)
char *name;
int namelen;
{
#if unix || xenix
#ifdef lint
    return (gethostname (name, (int *) namelen));	/* Stupid lint lib! */
#else
    return (gethostname (name, namelen));
#endif  /* lint */
#else
    return (-1);
#endif
}

#endif  /* BSD4_2 */
