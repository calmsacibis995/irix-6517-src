/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)srchtxt:srchtxt.c	1.4.2.2"

#include	<stdio.h>
#include	<ctype.h>
#include	<dirent.h>
#include	<regexpr.h>
#include	<string.h>
#include	<limits.h>
#include	<fcntl.h>
#include	<locale.h>
#include	<sys/euc.h>
#include	<getwidth.h>
#include	<sys/types.h>
#include	<sys/file.h>
#include	<sys/mman.h>
#include	<sys/stat.h>

#define	P_locale	"/usr/lib/locale/"
#define	L_locale	(sizeof(P_locale))
#define	MESSAGES	"/LC_MESSAGES/"
#define	ESIZE		BUFSIZ

/* External functions */

extern	int	getopt();
extern	void	exit();
extern	char	*strecpy();
extern	char	*strrchr();
extern	char	*strchr();
extern  void	close();


/* External variables */

extern	char	*optarg;
extern	int	optind;
extern	int	errno;
extern	int	sys_nerr;
extern	char	*sys_errlist[];

/* Internal functions */

static	void	usage();
static	void	prnt_str();
static	int	attach();
static	void	find_msgs();
static	char	*syserr();

/* Internal variables */

static	char	*cmdname; 	/* points to the name of the command */
static	int	lflg;		/* set if locale is specified on command line */
static	int	mflg;		/* set if message file is specified on command line */
static	char	locale[15];	/* save name of current locale */
static  char	*msgfile;       /* points to the argument immediately following the m option */
static	char	*text;		/* pointer to search pattern */
static	int	textflg;	/* set if text pattern is specified on command line */
static	int	sflg;		/* set if the s option is specified */
static	char	*fname;		/* points to message file name */
static	int	msgnum;		/* message number */
eucwidth_t	eucwidth;	/* used in strecpy() */
static	int	iflg;		/* print a msg number */
static	int	msgid;		/* id to print */


main(argc, argv)
int	argc;
char	*argv[];
{
	int	ch;
	char	*end;
	int	addr;
	int	len;
	int	len1;
	int	fd;
	size_t	size;
	char	pathname[PATH_MAX];
	char	*cp;
	char	ebuf[ESIZE];
	DIR	*dirp;
	struct	dirent	*dp;

	/* find last level of path in command name */
	if (cmdname = strrchr(*argv, '/'))
		++cmdname;
	else
		cmdname = *argv;

	/* parse command line */
	while ((ch = getopt(argc, argv, "sl:m:i:")) != -1)
		switch(ch) {
			case	'l':
				lflg++;
				(void)strcpy(locale, optarg);
				continue;
			case	'm':
				mflg++;
				msgfile = optarg;
				continue;
			case	's':
				sflg++;
				continue;
			case	'i':
				iflg++;
				msgid = atoi(optarg);
				continue;
			default:
				usage();
			}
	if (optind < argc) {
		text = argv[optind++];
		textflg++;
	}
	if (optind != argc )
		usage();

	/* create full path name to message files */
	if (!lflg) {
		if (setlocale(LC_MESSAGES, "") == NULL) {
			(void)fprintf(stderr, "%s: ERROR: setlocale(LC_MESSAGES) failed for default locale (check environment variables)\n", cmdname);
			exit(1);
		}
		(void)strcpy(locale, setlocale(LC_MESSAGES, ""));
	}
	(void)setlocale(LC_CTYPE, "");
	getwidth(&eucwidth);
	(void)strcpy(pathname, P_locale);
	(void)strcpy(&pathname[L_locale - 1], locale);
	(void)strcat(pathname, MESSAGES);
	len = strlen(pathname);

	if (textflg) {
			/* compile regular expression */
		if (compile(text, &ebuf[0], &ebuf[ESIZE]) == (char *)NULL) {
			(void)fprintf(stderr, "%s: ERROR: regular expression compile failed\n", cmdname);
			exit(1);
		}
	}

	/* access message files */ 
	if (mflg) {
		end = msgfile + strlen(msgfile) + 1;
		if (*msgfile == ',' || *(end - 2) == ',')
			usage();
		while ((fname = strtok(msgfile, ",\0")) != NULL) {
			if (strchr(fname, '/') != (char *)NULL) {
				cp = fname;
				len1 = 0;
			}
			else {
				cp = pathname;
				len1 = len;
			}	
			msgfile = msgfile + strlen(fname) + 1;
			if ((addr = attach(cp, len1, &fd, &size)) == -1) {
				(void)fprintf(stderr, "%s: ERROR: failed to access message file '%s'\n",cmdname, cp);
				if (end != msgfile)
					continue;
				else
					break;
			}
			find_msgs(addr, ebuf, size, cp);
			munmap((caddr_t)addr, size);
			close(fd);
			if  (end == msgfile)
				break;
		}
	} /* end if (mflg) */
	else { 
		if((dirp = opendir(pathname)) == (DIR *) NULL) {
			(void)fprintf(stderr, "%s: ERROR: %s %s\n", cmdname, pathname, syserr());
			exit(1);
		}
		while( (dp = readdir(dirp)) != (struct dirent *)NULL ) {
			if (dp->d_name[0] == '.')
				continue;
			fname = dp->d_name;
			if ((addr = attach(pathname, len, &fd, &size)) == -1) {
				(void)fprintf(stderr, "%s: ERROR: failed to access message file '%s'\n",cmdname, pathname);
				continue;
			}
			find_msgs(addr, ebuf, size, pathname);
			munmap((caddr_t)addr, size);
			close(fd);
		}
		(void)closedir(dirp);
	}
	exit(0);
}


/* print usage message */
static void
usage()
{
	(void)fprintf(stderr, "usage: srchtxt [-s][-i msgnum] [-l locale] [-m msgfile,...] [text]\n");
	exit(1);
}

/* print string - non-graphic characters are printed as alphabetic
		  escape sequences
*/
static	void
prnt_str(instring)
char	*instring;
{
	char	outstring[1024];

	if (iflg && msgid != msgnum)
		return;
	(void)strecpy(outstring, instring, (char *)NULL);
	if (sflg)
		(void)fprintf(stdout, "%s\n", outstring);
	else
		(void)fprintf(stdout, "<%s:%d>%s\n", fname, msgnum, outstring);
}

/* mmap a message file to the address space */
static int
attach(path, len, fdescr, size)
char	*path;
int	len;
int	*fdescr;
size_t	*size;
{
	int	fd = -1;
	caddr_t	addr;
	struct	stat	sb;

	(void)strcpy(&path[len], fname);
	if ( (fd = open(path, O_RDONLY)) != -1 &&
		fstat(fd, &sb) != -1 && 
			sb.st_size > 2*sizeof(int) &&
			(addr = mmap(0, sb.st_size,
				PROT_READ, MAP_SHARED,
					fd, 0)) != (caddr_t)-1 ) {
		*fdescr = fd; 
		*size = sb.st_size;
		return( (int) addr);
	}
	else {
		if (fd != -1)
			close(fd);
		return(-1);
	}
}


/* find messages in message files */
static void
find_msgs(addr, regexpr, size, name)
int	addr;
char	*regexpr;
int	size;
char	*name;
{
	int	num_msgs;
	char	*msg;

	num_msgs = *(int *)addr; 
	if (num_msgs * sizeof(int *) + sizeof(int *) > size) {
		fprintf(stderr, "Format error in file %s\n", name);
		return;
	}
	for (msgnum = 1; msgnum<=num_msgs; msgnum++) {
		msg = (char *)(*(int *)(addr + sizeof(int) * msgnum) + addr);
		if ((unsigned)msg >= ((unsigned)addr + size)) {
			fprintf(stderr, "Format error (not enough strings) in file %s\n", name);
			return;
		}
		if (textflg) {
			if (step(msg, regexpr))
				prnt_str(msg);
		} else
			prnt_str(msg);
	}
}

/* print description of error */ 
static char *
syserr()
{
	return (errno <= 0 ? "No error (?)"
	   : errno < sys_nerr ? sys_errlist[errno]
	   : "Unknown error (!)");
}

/* modified version of strecpy() */
/*
	strecpy(output, input, except)
	strecpy copys the input string to the output string expanding
	any non-graphic character with the C escape sequence.
	Esacpe sequences produced are those defined in "The C Programming
	Language" pages 180-181.
	Characters in the except string will not be expanded.
	Returns the first argument.

	streadd( output, input, except )
	Identical to strecpy() except returns address of null-byte at end
	of output.  Useful for concatenating strings.
*/

char *streadd();


char *
strecpy( pout, pin, except )
char	*pout;
const char	*pin;
const char	*except;
{
	(void)streadd( pout, pin, except );
	return  pout;
}


char *
streadd( pout, pin, except )
register char	*pout;
register const char	*pin;
const char	*except;
{
	register unsigned	c;

	while( c = *pin++ ) {
		if( !ISPRINT( c, eucwidth )  &&  ( !except  ||  !strchr( except, c ) ) ) {
			*pout++ = '\\';
			switch( c ) {
			case '\n':
				*pout++ = 'n';
				continue;
			case '\t':
				*pout++ = 't';
				continue;
			case '\b':
				*pout++ = 'b';
				continue;
			case '\r':
				*pout++ = 'r';
				continue;
			case '\f':
				*pout++ = 'f';
				continue;
			case '\v':
				*pout++ = 'v';
				continue;
			case '\007':
				*pout++ = 'a';
				continue;
			case '\\':
				continue;
			default:
				(void)sprintf( pout, "%.3o", c );
				pout += 3;
				continue;
			}
		}
		if( c == '\\'  &&  ( !except  ||  !strchr( except, c ) ) )
			*pout++ = '\\';
		*pout++ = c;
	}
	*pout = '\0';
	return  (pout);
}
