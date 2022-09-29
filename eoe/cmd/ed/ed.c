/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 	Portions Copyright(c) 1988, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

#ident	"@(#)ed:ed.c	1.49.2.11"

/*
** Editor
*/

#include	<sys/types.h>
#include	<locale.h>
#include	<unistd.h>
#include	<stdio.h>
#include	<pfmt.h>
/* sgi - removed mac.h include.  removed all svr4 security. 14 Jan 92. pj */
/* sgi - added paths.h to get _PATH_BSHELL. 17 Jan 92. pj */
#include <paths.h>

/* sgi - change "sev" to "int sev" - known accom bug. 14 Jan 92. pj */
const struct {const int sev; const char *msg, *id; } msgtab[] =
{
	MM_ERROR,	"Write or open on pipe failed",		":162",	/*  0 */
	MM_WARNING,	"Expecting `w'",			":163",	/*  1 */
	MM_ERROR,	"Mark not lower case ascii",		":164",	/*  2 */
	MM_ERROR,	"Cannot open input file",		":165",	/*  3 */
	MM_ERROR,	"PWB spec problem",			":166",	/*  4 */
	MM_ERROR,	"Nothing to undo",			":167",	/*  5 */
	MM_ERROR,	"Restricted shell",			":168",	/*  6 */
	MM_ERROR,	"Cannot create output file",		":169",	/*  7 */
	MM_ERROR,	"Filesystem out of space!",		":170",	/*  8 */
	MM_ERROR,	"Cannot open file",			":171",	/*  9 */
	MM_ERROR,	"Cannot link",				":172",	/* 10 */
	MM_ERROR,	"Range endpoint too large",		":173",	/* 11 */
	MM_ERROR,	"Unknown command",			":174",	/* 12 */
	MM_ERROR,	"Search string not found",		":175",	/* 13 */
	0,		"-",					NULL,	/* 14 */
	MM_ERROR,	"Line out of range",			":176",	/* 15 */
	MM_ERROR,	"Bad number",				":177",	/* 16 */
	MM_ERROR,	"Bad range",				":178",	/* 17 */
	MM_ERROR,	"Illegal address count",		":179",	/* 18 */
	MM_ERROR,	"Incomplete global expression",		":180",	/* 19 */
	MM_ERROR,	"Illegal suffix",			":181",	/* 20 */
	MM_ERROR,	"Illegal or missing filename",		":182",	/* 21 */
	MM_ERROR,	"No space after command",		":183",	/* 22 */
	MM_ERROR,	"fork() failed - try again",		":184",	/* 23 */
/* sgi - perpetuate local change of max filename len to 1024. 14 Jan 92. pj */
	MM_ERROR,	"Maximum of 1024 characters in file names",":185",/* 24 */
	MM_ERROR,	"`\\digit' out of range",		":186",	/* 25 */
	MM_ERROR,	"Interrupt",				"uxlibc:6",	/* 26 */
	MM_ERROR,	"Line too long",			":187",	/* 27 */
	MM_ERROR,	"Illegal character in input file",	":188",	/* 28 */
	MM_ERROR,	"Write error",				":189",	/* 29 */
	MM_ERROR,	"Out of memory for append",		":190",	/* 30 */
	MM_ERROR,	"Temp file too big",			":191",	/* 31 */
	MM_ERROR,	"I/O error on temp file",		":192",	/* 32 */
	MM_ERROR,	"Multiple globals not allowed",		":193",	/* 33 */
	MM_ERROR,	"Global too long",			":194",	/* 34 */
	MM_ERROR,	"No match",				":195",	/* 35 */
	MM_ERROR,	"Illegal or missing delimiter",		":196",	/* 36 */
	0,		"-",					NULL,	/* 37 */
	MM_ERROR,	"Replacement string too long",		":197",	/* 38 */
	MM_ERROR,	"Illegal move destination",		":198",	/* 39 */
	0,		"-",					NULL,	/* 40 */
	MM_ERROR,	"No remembered search string",		":199",	/* 41 */
	MM_ERROR,	"'\\( \\)' imbalance",			":200",	/* 42 */
	MM_ERROR,	"Too many `\\(' s",			":201",	/* 43 */
	MM_ERROR,	"More than two numbers given in '\\{ \\}'",":202",/* 44 */
	MM_ERROR,	"'\\}' expected",			":203",	/* 45 */
	MM_ERROR,	"First number exceeds second in '\\{ \\}'",":204",/* 46 */
	MM_ERROR,	"Incomplete substitute",		":205",	/* 47 */
	MM_ERROR,	"Newline unexpected",			":206",	/* 48 */
	MM_ERROR,	"'[ ]' imbalance",			":207",	/* 49 */
	MM_ERROR,	"Regular expression overflow",		":208",	/* 50 */
	MM_ERROR,	"Regular expression error",		":209",	/* 51 */
	MM_ERROR,	"Command expected",			":210",	/* 52 */
	MM_ERROR,	"a, i, or c not allowed in G",		":211",	/* 53 */
	MM_ERROR,	"End of line expected",			":212",	/* 54 */
	MM_ERROR,	"No remembered replacement string",	":213",	/* 55 */
	MM_ERROR,	"No remembered command",		":214",	/* 56 */
	MM_ERROR,	"Illegal redirection",			":215",	/* 57 */
	MM_ERROR,	"Possible concurrent update",		":216",	/* 58 */
	0,		"-",					NULL,	/* 59 */
	MM_WARNING,	"The x command has become X (upper case)",":217",	/* 60 */
	MM_WARNING,	"'w' may destroy input file (due to `illegal char' read earlier)", ":218",/* 61 */
	MM_WARNING,	"'q' may lose data in buffer; 'w' may destroy input file", ":219",/* 62 */
	MM_ERROR,	"Encryption of string failed",		":220",	/* 63 */
	MM_ERROR,	"Encryption facility not available\n",	":221",	/* 64 */
	MM_ERROR,	"Cannot encrypt temporary file\n",	":222",	/* 65 */
	0,		"Enter key:",				":223",	/* 66 */
	MM_ERROR,	"Illegal byte sequence",		":224",	/* 67 */
	0
};

#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <termio.h>
#include <ctype.h>
#include <setjmp.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>

#define MULTI_BYTE_MAX MB_LEN_MAX
#define FTYPE(A)	(A.st_mode)
#define FMODE(A)	(A.st_mode)
#define	IDENTICAL(A,B)	(A.st_dev==B.st_dev && A.st_ino==B.st_ino)
#define ISBLK(A)	((A.st_mode & S_IFMT) == S_IFBLK)
#define ISCHR(A)	((A.st_mode & S_IFMT) == S_IFCHR)
#define ISDIR(A)	((A.st_mode & S_IFMT) == S_IFDIR)
#define ISFIFO(A)	((A.st_mode & S_IFMT) == S_IFIFO)
#define ISREG(A)	((A.st_mode & S_IFMT) == S_IFREG)

#define	PUTM()	if(xcode >= 0) {pfmt(stdout,msgtab[xcode].sev|MM_NOGET,gettxt(msgtab[xcode].id,msgtab[xcode].msg));putchar('\n');}
#define UNGETC(c)       (peekc = c)
/* sgi - perpetuate local change of max filename len to 1024. 14 Jan 92. pj */
#define	FNSIZE	1024
#define	LBSIZE	512

/* size of substitution replacement pattern buffer */
#define	RHSIZE	1024

/* size of regular expression buffer */
#define ESIZE	512

/* size of buffer for global commands */
#define	GBSIZE	256

#define	KSIZE	8

#define	READ	0
#define	WRITE	1

extern	int	errno;
extern  char	*optarg;	/* Value of argument */
extern  int	optind;		/* Indicator of argument */

int	Xqt = 0;
/* sgi - removed "mac" definition.  removed all svr4 security. 14 Jan 92. pj */
int	lastc;
char	savedfile[FNSIZE];
char	file[FNSIZE];
char	funny[LINE_MAX];
int	funlink = 0;
char	linebuf[LINE_MAX+2];
char	expbuf[ESIZE];
char	rhsbuf[RHSIZE];
struct	lin	{
	long cur;
	long sav;
	long Gflag;
};
typedef struct lin *LINE;
LINE	zero;
LINE	dot;
LINE	dol;
LINE	endcore;
LINE	fendcore;
LINE	addr1;
LINE	addr2;
LINE	savdol, savdot;
int	globflg;
int	initflg;
char	genbuf[LINE_MAX];
long	count;
int	numpass;	/* Number of passes thru dosub(). */
int	gsubf;		/* Occurrence value. 999=all. */
int	ocerr1;		/* Allows lines NOT changed by dosub() to NOT be put
			out. Retains last line changed as current line. */
int	ocerr2;		/* Flags if ANY line changed by substitute(). 0=nc. */
char	*nextip;
char	*linebp;
int	ninbuf;
int	peekc;
int	io;
void	(*oldhup)(), (*oldintr)(int);
void	(*oldquit)(), (*oldpipe)(int);
int	vflag = 1;
int	xflag;
int	xtflag;
int	kflag;
int	crflag;		/* Flag for determining if file being read is encrypted */
char   *getkey();
int	hflag;
int	xcode = -1;
char	crbuf[LINE_MAX];
int	perm[2];
int	tperm[2];
static  int permflag;
static  int tpermflag;
int	col;
char	*globp;
int	tfile = -1;
int	tline;
char	*tfname;
extern	char	*locs;
char	ibuff[LINE_MAX];
int	iblock = -1;
char	obuff[LINE_MAX];
int	oblock = -1;
int	ichanged;
int	nleft;
long	savnames[26], names[26];
int	anymarks;
long	subnewa;
int	fchange;
int	nline;
int	fflg, shflg;
char	prompt[16] = "*";
int	rflg;
int	readflg;
int	eflg;
int	ncflg;
int	listn;
int	listf;
int	pflag;
int	Gcmd;
int	flag28 = 0; /* Prevents write after a partial read */
int	save28 = 0; /* Flag whether buffer empty at start of read */
long 	savtime;
char	*name = "SHELL";
char	*rshell = "/usr/bin/rsh";
char	*val;
char	*home;
int nodelim;		/* Determine if regular expression is followed by
			 * delimiter
			 */
struct  regexp {
    regex_t preg;
    int     nomatch;    /* Mapping Expbuf[0] */
    int     nore;       /* Mapping Expbuf[1] */
    short   Nbra;   /* No. of sub-expression */
};
struct	regexp re;
int xpg4command = 0; 
int	cur_cmd = 0;
LINE	xpgdot;
/* Old regular expression mapping */
#define NBRA 9
#define REG_MSGLEN 36
char *braslist[NBRA+1];
char *braelist[NBRA+1];

char    *locs;
char    *loc1;      /* Where re began to match (in linebuf) */
char    *loc2;      /* First char after re match (") */

struct	Fspec	{
	char	Ftabs[22];
	char	Fdel;
	unsigned char	Flim;
	char	Fmov;
	char	Ffill;
};

typedef	ssize_t (*blkio_func_t)(int, void *, size_t);

LINE	address(void);
void	append(int (*)(void), LINE);
void	blkio(int, char *, blkio_func_t);
void	chktime(void);
void	clear(struct Fspec *);
void	commands(void);
void	comple(wchar_t);
void	Delete(void);
void	dosub(void);
void	eclose(int);
int	error(int);
void	error1(int);
void	exfile(void);
void	filename(int);
int	fspec(char *, struct Fspec *, int);
char	*getblock(long, int);
int	getcopy(void);
void	getime(void);
char	*getkey(char *);
char	*getline(long);
int	getsub(void);
void	global(int);
void	globaln(int);
void	init(void);
void	join(void);
void	mkfunny(void);
void	move(int);
void	newline(void);
void	newtime(void);
void	nonzero(void);
void	onhup(int);
void	onintr(int);
void	onpipe(int);
char	*place(char *, char *, char *);
void	putchr(wchar_t);
void	putd(void);
void	putfile(void);
long	putline(void);
void	putoctal(unsigned char);
void	Puts(const char *);
void	quit(int);
void	rdelete(LINE, LINE);
void	red(char *);
void	reverse(LINE, LINE);
void	save(void);
void	setall(void);
void	setdot(void);
void	setnoaddr(void);
void	strcopy(char *, char *);
void	substitute(int);
void	targ(struct Fspec *);
void	tincr(int, struct Fspec *);
void	tlist(struct Fspec *);
void	tstd(struct Fspec *);
void	undo(void);
void	unixcom(void);


struct stat	Fl, Tf; 
#ifndef RESEARCH
struct statvfs	U;
int	Short = 0;
mode_t	oldmask; /* No umask while writing */
#endif
jmp_buf	savej;

#define MULTIFILE 1	/* 0:disable 1:enable 'e %' multi-file edit */
#if MULTIFILE
/* sgi - local 'ed %' feature.  perpetuated from irix 3.3, 4.x. 14 Jan 92. pj */
	/*
	 * ed file1 file2 ...
	 *
	 * Supports editing multiple files named on the command line.
	 * To switch from the (N)th file to the (N+1)th file,
	 * issue the command: e %.  The '%' is replaced with the name
	 * of the next file from the command line.
	 *
	 * This "feature" added by:
	 *	Paul Jackson
	 *	Silicon Graphics
	 *	23 May 89
	 */
jmp_buf jmpnxtfil;	/* jmp here if 'e %' cmd to edit next argv[] file */
int nadtlfls;		/* # additional files (past cur. one) in argv[] */
#endif

#ifdef	NULLS
int	nulls;	/* Null count */
#endif
long	ccount;

struct	Fspec	fss;

int	errcnt = 0;

void
onpipe(int sig)
{
/* sgi - local fix. recatch SIGPIPE so 2nd such sig not fatal. 14 Jan 92. pj */
	signal(SIGPIPE, onpipe);
	error(0);
}

int
main(int _argc, char **_argv)
{
	register	char *p1, *p2, *ep;
	register	int c;
/* sgi - removed level definition.  removed all svr4 security. 14 Jan 92. pj */

	/*
	 * Put argc,argv in volatiles, so that we can rely
	 * on them being preserved across longjmp's.
	 */

	volatile int argc = _argc;
	char ** volatile argv = _argv;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:ed");

	setbuf(stdout, NULL);	/* Synchronize pfmt() and write() */
	oldquit = signal(SIGQUIT, SIG_IGN);
	oldhup = signal(SIGHUP, SIG_IGN);
	oldintr = signal(SIGINT, SIG_IGN);
	oldpipe = signal(SIGPIPE, onpipe);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, quit);

	if ((ep = getenv("_XPG")) != NULL)
		xpg4command = (atoi(ep) > 0);

	p1 = *argv;
	while(*p1++);
	while(--p1 >= *argv)
		if(*p1 == '/')
			break;
	*argv = p1 + 1;
	/* if SHELL set in environment and is /usr/bin/rsh, set rflg */
	if((val = getenv(name)) != NULL)
		if (strcmp(val, rshell) == 0)
			rflg++;
	if (**argv == 'r')
		rflg++;
	home = getenv("HOME");
	while(1) {
		while ((c = getopt(argc, argv,"sp:qxC"))!= EOF) {
			switch(c) {

			case 's':
				vflag = 0;
				break;

			case 'p':
				strncpy(prompt, optarg, sizeof(prompt)-1);
				shflg = 1;
				break;
		
			case 'q':
				signal(SIGQUIT, SIG_DFL);
				vflag = 1;
				break;

			case 'x':
				crflag = -1;
				xflag = 1;
				break;	
		
			case 'C':
				crflag = 1;
				xflag = 1;
				break;
		
			default:
				pfmt(stderr, MM_ACTION,
					":225:Usage: ed [- | -s] [-p string] [-x] [-C] [file]\n");
				exit(2);
			}
		}
		if(argv[optind] && strcmp(argv[optind], "-") == 0 && strcmp(argv[optind-1], "--") != 0) {
			vflag = 0;
			optind++;
			continue;
		}
		break;
	}
	argc = argc - optind;
	argv = &argv[optind];

	if(xflag){
		if(permflag)
			(void)crypt_close(perm);
		permflag = 1;
		if ((kflag = run_setkey(perm, getkey(gettxt(msgtab[66].id, msgtab[66].msg)))) == -1) {
			pfmt(stdout, msgtab[64].sev|MM_NOGET,
				gettxt(msgtab[64].id, msgtab[64].msg));
			xflag = 0;
			kflag = 0;
		}
		if(kflag == 0)
			crflag = 0;
	}
#if MULTIFILE
/* sgi - local 'ed %' feature.  perpetuated from irix 3.3, 4.x. 14 Jan 92. pj */
	nadtlfls = argc - 1;	/* # files past current one in argv[] */
	if (setjmp(jmpnxtfil)) {	/* if 'e %' cmd, edit next file */
		argc--, argv++;		/* bump to next argv[] filename */
		if (vflag) {
			write(1, "e ", 2);
			Puts (*argv);
			unlink(tfname);
		}
	}
#endif

	if (argc>0) {
		p1 = *argv;
		if(strlen(p1) >= FNSIZE) {
			pfmt(stderr, MM_ERROR, ":226:file name too long\n");
			if (kflag)
				crypt_close(perm);
			exit(2);
		}
		p2 = savedfile;
		while (*p2++ = *p1++);
		globp = "e";
		fflg++;
	}
	else 	/* editing with no file so set savtime to 0 */
		savtime = 0;
	eflg++;
/* sgi - removed lvlin call.  removed all svr4 security. 14 Jan 92. pj */

	tfname = tempnam("","ea");
/* sgi - local fix. be sure tfname set. 14 Jan 92. pj */
	if (tfname == 0) {
		pfmt(stderr,MM_ERROR,":233:Cannot create temporary file\n");
		if (kflag) crypt_close(perm);
		exit(2);
	}
	fendcore = (LINE )sbrk(0);
	init();
	if (oldintr != SIG_IGN)
		signal(SIGINT, onintr);
	if (oldhup != SIG_IGN)
		signal(SIGHUP, onhup);
	setjmp(savej);
	commands();
	quit(0);
	/* NOTREACHED */
}

void
commands(void)
{
	int getfile(), gettty();
	register LINE a1;
	register int c;
	register char	*p1, *p2;
	int	fsave, m, n;

	for (;;) {
	nodelim = 0;
	if ( pflag && !Gcmd) {
		pflag = 0;
		addr1 = addr2 = dot;
		goto print;
	}
	if (shflg && globp==0)
		write(1, prompt, strlen(prompt));
	addr1 = 0;
	addr2 = 0;
	if((c=getchr()) == ',') {
		addr1 = zero + 1;
		addr2 = dol;
		c = getchr();
		goto swch;
	} else if(c == ';') {
		addr1 = dot;
		addr2 = dol;
		c = getchr();
		goto swch;
	} else
		peekc = c;
	do {
		addr1 = addr2;
		if ((a1 = address())==0) {
			c = getchr();
			break;
		}
		addr2 = a1;
		if ((c=getchr()) == ';') {
			c = ',';
			dot = a1;
		}
	} while (c==',');
	if (addr1==0)
		addr1 = addr2;
swch:
	cur_cmd = c;
	switch(c) {

	case 'a':
		setdot();
		newline();
		if (!globflg) save();
		append(gettty, addr2);
		continue;

	case 'c':
		Delete();
		if (xpg4command)
			xpgdot = dot;
		append(gettty, addr1-1);
		if (dot == (addr1-1))
			dot = xpgdot;
		continue;

	case 'd':
		Delete();
		continue;

	case 'E':
		fchange = 0;
		c = 'e';
	case 'e':
		fflg++;
		setnoaddr();
		if (vflag && fchange) {
			fchange = 0;
			error(1);
		}
		filename(c);
		eflg++;
		init();
		addr2 = zero;
		goto caseread;

	case 'f':
		setnoaddr();
		filename(c);
		if (!ncflg)  /* there is a filename */
			getime();
		else
			ncflg--;
		Puts(savedfile);
		continue;

	case 'g':
		global(1);
		continue;
	case 'G':
		if(xpg4command) Gcmd = 1;
		globaln(1);
		Gcmd = 0;
		continue;

	case 'h':
		newline();
		setnoaddr();
		PUTM();
		continue;

	case 'H':
		newline();
		setnoaddr();
		if(!hflag) {
			hflag = 1;
			PUTM();
		}
		else
			hflag = 0;
		continue;

	case 'i':
		setdot();
		nonzero();
		newline();
		if (!globflg) save();
		append(gettty, addr2-1);
		if (dot == addr2-1)
			dot += 1;
		continue;

	case 'j':
		if (addr2==0) {
			addr1 = dot;
			addr2 = dot+1;
		}
		setdot();
		newline();
		nonzero();
		if (!globflg) save();
		join();
		continue;

	case 'k':
		if ((c = getchr()) < 'a' || c > 'z')
			error(2);
		newline();
		setdot();
		nonzero();
		names[c-'a'] = addr2->cur & ~01;
		anymarks |= 01;
		continue;

	case 'm':
		move(0);
		continue;

	case '\n':
		if (addr2==0)
			addr2 = dot+1;
		addr1 = addr2;
		goto print;

	case 'n':
		listn++;
		newline();
		goto print;

	case 'l':
		listf++;
	case 'p':
		newline();
	print:
		setdot();
		nonzero();
		a1 = addr1;
		do {
			if (listn) {
				count = a1 - zero;
				putd();
				putchr('\t');
			}
			Puts(getline(a1++->cur));
		}
		while (a1 <= addr2);
		dot = addr2;
		pflag = 0;
		listn = 0;
		listf = 0;
		continue;

	case 'Q':
		fchange = 0;
	case 'q':
		setnoaddr();
		newline();
		quit(0);

	case 'r':
		filename(c);
	caseread:
		readflg = 1;
		save28 = (dol != fendcore);
		if(crflag == 2 || crflag == -2)
			crflag = -1; /* restore crflag for next file */
		if ((io = eopen(file, O_RDONLY)) < 0) {
			lastc = '\n';
			/* if first entering editor and file does not exist */
			/* set saved access time to 0 */
			if (eflg) {
				savtime = 0;
				eflg  = 0;
			}
			error(3);
		}
		/* get last mod time of file */
		/* eflg - entered editor with ed or e  */
		if (eflg) {
			eflg = 0;
			getime();
		}
		setall();
		ninbuf = 0;
		n = zero != dol;
#ifdef NULLS
		nulls = 0;
#endif
		if (!globflg && (c == 'r')) save();
		append(getfile, addr2);
		exfile();
		readflg = 0;
		fchange = n;
		continue;

	case 's':
		setdot();
		nonzero();
		if (!globflg) save();
		substitute(globp!=0);
		continue;

	case 't':
		move(1);
		continue;

	case 'u':
		setdot();
		newline();
		if (!initflg) undo();
		else error(5);
		fchange = 1;
		continue;

	case 'v':
		global(0);
		continue;
	case 'V':
		globaln(0);
		continue;

	case 'W':
	case 'w':
		if(flag28){flag28 = 0; fchange = 0; error(61);}
		setall();
		if((zero != dol) && (addr1 <= zero || addr2 > dol))
			error(15);
		filename(c);
		if(Xqt) {
			io = eopen(file, O_WRONLY);
			n = 1;	/* set n so newtime will not execute */
			m = 1;	/* set m so fchange won't get set */
		} else {
			struct stat lFl;
			fstat(tfile,&Tf);
			if(stat(file, &Fl) < 0) {
				if((io = creat(file, S_IRUSR|S_IWUSR|S_IRGRP
					|S_IWGRP|S_IROTH|S_IWOTH)) < 0)
					error(7);
				fstat(io, &Fl);
				Fl.st_mtime = 0;
				lFl = Fl;
				close(io);
			}
			else {
#ifndef	RESEARCH
				oldmask = umask(0);
				/*
				 * Must determine if file is
				 * a symbolic link
				 */
				lstat(file, &lFl);
#endif
			}
#ifndef RESEARCH
		/*
		 * Determine if there are enough free blocks on system 
		*/
/* sgi - removed "mac" test.  removed all svr4 security. 14 Jan 92. pj */
			if(statvfs(file, &U) == 0 && !Short
				&& U.f_bfree < ((Tf.st_size/U.f_bsize) + 100)
				&& (U.f_bfree)) {

					Short = 1;
					error(8);
			}
			Short = 0;
#endif
			p1 = savedfile;		/* The current filename */
			p2 = file;
			m = strcmp(p1, p2);
			if (c == 'w' && Fl.st_nlink == 1 && ISREG(lFl)) {
				if (close(open(file, O_WRONLY)) < 0)
					error(9);
				if (!(n=m))
					chktime();
				mkfunny();
				/*
				 * If funlink equals one it means that
				 * funny points to a valid file which must
				 * be unlinked when interrupted.
				 */

				funlink = 1; 
				if ((io = creat(funny, FMODE(Fl))) >= 0) {
					chown(funny, Fl.st_uid, Fl.st_gid);
					chmod(funny, FMODE(Fl));
					putfile();
					exfile();

					if (rename(funny, file))
						error(10);
					funlink = 0;
					/* if filenames are the same */
					if (!n)
						newtime();
					/* check if entire buffer was written */
					fsave = fchange;
					fchange = ((addr1==zero || addr1==zero+1) && addr2==dol)?0:1;
					if(fchange == 1 && m != 0) fchange = fsave;
					continue;
				}
			}
			else   n = 1;	/* set n so newtime will not execute*/
			if((io = open(file,
				(c == 'w') ? O_WRONLY|O_CREAT|O_TRUNC
				: O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR
				|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0)
				error(7);
		}
		putfile();
		exfile();
		if (!n) newtime();
		fsave = fchange;
		fchange = ((addr1==zero||addr1==zero+1)&&addr2==dol)?0:1;
	/* Leave fchange alone if partial write was to another file */
		if(fchange == 1 && m != 0) fchange = fsave;
		continue;

	case 'C':
		crflag = 1; /* 
			     * C is same as X, but always assume input files are 
			     * ciphertext
			     */
		goto encrypt;

	case 'X':
		crflag = -1;
encrypt:
		setnoaddr();
		newline();
		xflag = 1;
		if(permflag)
			(void)crypt_close(perm);
		permflag = 1;
		if ((kflag = run_setkey(perm, getkey(gettxt(msgtab[66].id, msgtab[66].msg)))) == -1) {
			xflag = 0;
			kflag = 0;
			crflag = 0;
			error(64);
		}
		if(kflag == 0) 
			crflag = 0;
		continue;
	
	case '=':
		setall();
/* sgi - just toss 017777* mask - not needed?? 14 Jan 92. pj */
		count = (addr2-zero);
		putd();
		putchr('\n');
		newline();
		continue;

	case '!':
		unixcom();
		continue;

	case EOF:
		return;

	case 'P':
		setnoaddr();
		newline();
		if (shflg)
			shflg = 0;
		else
			shflg++;
		continue;
	}
	if (c == 'x')
		error(60);
	else
		error(12);
	}
}

LINE 
address(void)
{
	register minus, c;
	register LINE a1;
	int n, relerr;

	minus = 0;
	a1 = 0;
	for (;;) {
		c = getchr();
		if (isdigit(c)) {
			n = 0;
			do {
				n *= 10;
				n += c - '0';
			} while (isdigit(c = getchr()));
			peekc = c;
			if (a1==0)
				a1 = zero;
			if (minus<0)
				n = -n;
			a1 += n;
			minus = 0;
			continue;
		}
		relerr = 0;
		if (a1 || minus)
			relerr++;
		switch(c) {
		case ' ':
		case '\t':
			continue;
	
		case '+':
			minus++;
			if (a1==0)
				a1 = dot;
			continue;

		case '-':
		case '^':
			minus--;
			if (a1==0)
				a1 = dot;
			continue;
	
		case '?':
		case '/':
			comple(c);
			a1 = dot;
			for (;;) {
				if (c=='/') {
					a1++;
					if (a1 > dol)
						a1 = zero;
				} else {
					a1--;
					if (a1 < zero)
						a1 = dol;
				}
				if (execute(0, a1))
					break;
				if (a1==dot)
					error(13);
			}
			break;
	
		case '$':
			a1 = dol;
			break;
	
		case '.':
			a1 = dot;
			break;

		case '\'':
			if (!islower(c = getchr()))
				error(2);
			for (a1=zero; a1<=dol; a1++)
				if (names[c-'a'] == (a1->cur & ~01))
					break;
			break;
	
		default:
			peekc = c;
			if (a1==0)
				return(0);
			a1 += minus;
			if (a1<zero || a1>dol)
				error(15);
			return(a1);
		}
		if (relerr)
			error(16);
	}
}

void
setdot(void)
{
	if (addr2 == 0)
		addr1 = addr2 = dot;
	if (addr1 > addr2)
		error(17);
}

void
setall(void)
{
	if (addr2==0) {
		addr1 = zero+1;
		addr2 = dol;
		if (dol==zero)
			addr1 = zero;
	}
	setdot();
}

void
setnoaddr(void)
{
	if (addr2)
		error(18);
}

void
nonzero(void)
{
	if (addr1<=zero || addr2>dol)
		error(15);
}

void
newline(void)
{
	register c;

	c = getchr();
	if ( c == 'p' || c == 'l' || c == 'n' ) {
		pflag++;
		if ( c == 'l') listf++;
		if ( c == 'n') listn++;
		c = getchr();
	}
	if ( c != '\n')
		error(20);
}

void
filename(int comm)
{
	register char *p1, *p2;
	register c;
	register i = 0;

	count = 0;
	c = getchr();
	if (c=='\n' || c==EOF) {
		p1 = savedfile;
		if (*p1==0 && comm!='f')
			error(21);
		/* ncflg set means do not get mod time of file */
		/* since no filename followed f */
		if (comm == 'f')
			ncflg++;
		p2 = file;
		while (*p2++ = *p1++);
		red(savedfile);
		return;
	}
	if (c!=' ')
		error(22);
	while ((c = getchr()) == ' ');
#if MULTIFILE
/* sgi - local 'ed %' feature.  perpetuated from irix 3.3, 4.x. 14 Jan 92. pj */
	/*
	 * If exactly 'e %' and if # remaining argv[] filenames (nadtlfls)
	 * is at least one, then jump back to the beginning to edit
	 * the next file named in argv[].
	 */
	if (c == '%') {
		if (nadtlfls>0 && ((peekc=getchr())=='\n' || peekc==EOF)) {
			nadtlfls--;
			peekc = 0;
			longjmp (jmpnxtfil, 1);
		}
	}
#endif
	if(c == '!')
		++Xqt, c = getchr();
	if (c=='\n')
		error(21);
	p1 = file;
	do {
		if(++i >= FNSIZE)
			error(24);
		*p1++ = c;
		if(c==EOF || (c==' ' && !Xqt))
			error(21);
	} while ((c = getchr()) != '\n');
	*p1++ = 0;
	if(Xqt)
		if (comm=='f') {
			--Xqt;
			error(57);
		}
		else
			return;
	if (savedfile[0]==0 || comm=='e' || comm=='f') {
		p1 = savedfile;
		p2 = file;
		while (*p1++ = *p2++);
	}
	red(file);
}

void
exfile(void)
{
#ifdef NULLS
	int c;
#endif

#ifndef RESEARCH
	if(oldmask) {
		umask(oldmask);
		oldmask = 0;
	}
#endif
	eclose(io);
	io = -1;
	if (vflag) {
		putd();
		putchr('\n');
#ifdef NULLS
		if(nulls) {
			pfmt(stdout, MM_WARNING, ":227:%d nulls replaced by '\\0'\n",
				nulls);
			nulls = 0;
		}
#endif
	}
}

void
onintr(int sig)
{
	void	onintr();

	signal(SIGINT, onintr);
	putchr('\n');
	lastc = '\n';
	globflg = 0;
	Gcmd = 0;
	if (funlink) unlink(funny); /* remove tmp file */
	/* if interrupted a read, only part of file may be in buffer */
	if ( readflg ) {
		pfmt(stdout, MM_WARNING, ":228:\007read may be incomplete - beware!\007\n");
		fchange = 0;
	}
	error(26);
}

void
onhup(int sig)
{
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	/*
	 * if there are lines in file and file was not written 
	 * since last update, save in ed.hup, or $HOME/ed.hup 
	 */
	if (dol > zero && fchange == 1) {
		addr1 = zero+1;
		addr2 = dol;
		io = creat("ed.hup",
			S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
		if(io < 0 && home) {
			char	*fn;

			fn = (char *)calloc(strlen(home) + 8, sizeof(char));
			if(fn) {
				strcpy(fn, home);
				strcat(fn, "/ed.hup");
				io = creat(fn, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP
						|S_IROTH|S_IWOTH);
				free(fn);
			}
		}
		if (io > 0)
			putfile();
	}
	fchange = 0;
	quit(0);
}

int
error(int code)
{
	register c;

	if(code == 28 && save28 == 0){fchange = 0; flag28++;}
	readflg = 0;
	++errcnt;
	listf = listn = 0;
	pflag = 0;
#ifndef RESEARCH
	if(oldmask) {
		umask(oldmask);
		oldmask = 0;
	}
#endif
#ifdef NULLS	/* Not really nulls, but close enough */
	/* This is a bug because of buffering */
	if(code == 28) /* illegal char. */
		putd();
#endif
	putchr('?');
	if(vflag) {		/* won't print if "-s" or "-" option is used */
		if(code == 3)	/* Cant open file */
			Puts(file);
		else
			putchr('\n');
	}
	count = 0;
	lseek(0, (long)0, 2);
	if (globp)
		lastc = '\n';
	globp = 0;
	peekc = lastc;
	if(lastc)
		while ((c = getchr()) != '\n' && c != EOF);
	if (io > 0) {
		eclose(io);
		io = -1;
	}
	xcode = code;
	if(hflag)
		PUTM();
	if(code==4)return(0);	/* Non-fatal error. */
	longjmp(savej, 1);
	/* NOTREACHED */
}

int
getchr(void)
{
	char c;
	if (lastc=peekc) {
		peekc = 0;
		return(lastc);
	}
	if (globp) {
		if ((lastc = (unsigned char)*globp++) != 0)
			return(lastc);
		globp = 0;
		return(EOF);
	}
	if (read(0, &c, 1) <= 0)
		return(lastc = EOF);
	lastc = (unsigned char)c;
	return(lastc);
}

int
gettty(void)
{
	register c;
	register char *gf;
	register char *p;

	p = linebuf;
	gf = globp;
	while ((c = getchr()) != '\n') {
		if (c==EOF) {
			if (gf)
				peekc = c;
			return(c);
		}
		if (c == 0)
			continue;
		*p++ = c;
		if (p >= &linebuf[LINE_MAX])
			error(27);
	}
	*p++ = 0;
	if (linebuf[0]=='.' && linebuf[1]==0)
		return(EOF);
	if (linebuf[0]=='\\' && linebuf[1]=='.' && linebuf[2]==0) {
		linebuf[0] = '.';
		linebuf[1] = 0;
	}
	return(0);
}

int
getfile(void)
{
	register c;
	register char *lp, *fp;

	lp = linebuf;
	fp = nextip;
	do {
		if (--ninbuf < 0) {
			if ((ninbuf = read(io, genbuf, LINE_MAX)-1) < 0)
				if (lp > linebuf) {
					pfmt(stdout, MM_WARNING, ":229:'\\n' appended\n");
					*genbuf = '\n';
				}
				else
				 	return(EOF);
			if(crflag == -1) {
				if(isencrypt(genbuf, ninbuf + 1))
					crflag = 2;
				else
					crflag = -2;
			}
			fp = genbuf;
			if(crflag > 0)
				if(run_crypt(count, genbuf, ninbuf+1, perm) == -1)
						error(63);
		}
		if (lp >= &linebuf[LINE_MAX]) {
			lastc = '\n';
			error(27);
		}
		if ((*lp++ = c = *fp++) == 0) {
#ifdef NULLS
			lp[-1] = '\\';
			*lp++ = '0';
			nulls++;
#else
			lp--;
			continue;
#endif
		}
		count++;
	} while (c != '\n');
	*--lp = 0;
	nextip = fp;
	if (fss.Ffill && fss.Flim && lenchk(linebuf,&fss) < 0) {
		pfmt(stdout, MM_ERROR, ":230:line too long: lno = %d\n",
/* sgi - just toss 017777* mask - not needed?? 14 Jan 92. pj */
			(++dot-zero));
		dot--;
	}
	return(0);
}

void
putfile(void)
{
	int n;
	LINE a1;
	register char *fp, *lp;
	register nib;

	nib = LINE_MAX;
	fp = genbuf;
	a1 = addr1;
	do {
		lp = getline(a1++->cur);
		if (fss.Ffill && fss.Flim && lenchk(linebuf,&fss) < 0) {
			pfmt(stdout, MM_ERROR, ":230:line too long: lno = %d\n",
/* sgi - just toss 017777* mask - not needed?? 14 Jan 92. pj */
				(a1-zero-1));
		}
		for (;;) {
			if (--nib < 0) {
				n = fp-genbuf;
				if(kflag)
					if(run_crypt(count-n, genbuf, n, perm) == -1) 
						error(63);
				if(write(io, genbuf, n) != n)
					error(29);
				nib = LINE_MAX - 1;
				fp = genbuf;
			}
			if(dol->cur == 0L)break; /* Allow write of null file */
			count++;
			if ((*fp++ = *lp++) == 0) {
				fp[-1] = '\n';
				break;
			}
		}
	} while (a1 <= addr2);
	n = fp-genbuf;
	if(kflag)
		if(run_crypt(count-n, genbuf, n, perm) == -1)
			error(63);
	if(write(io, genbuf, n) != n)
		error(29);
}

void
append(int (*f)(void),LINE a)
{
	register LINE a1, a2, rdot;
	long tl;

	nline = 0;
	dot = a;
	while ((*f)() == 0) {
		if (dol >= endcore) {
			if ((int)sbrk(512*sizeof(struct lin)) == -1) {
				lastc = '\n';
				error(30);
			}
			endcore += 512;
		}
		tl = putline();
		nline++;
		a1 = ++dol;
		a2 = a1+1;
		rdot = ++dot;
		while (a1 > rdot)
			(--a2)->cur = (--a1)->cur;
		rdot->cur = tl;
	}
}

void
unixcom(void)
{
	register void (*savint)();
	register pid_t	pid, rpid;
	int retcode;
	static char savcmd[LBSIZE];	/* last command */
	char curcmd[LBSIZE];		/* current command */
	char *psavcmd, *pcurcmd, *psavedfile;
	register c, endflg=1, shflg=0;

	setnoaddr();
	if(rflg)
		error(6);
	pcurcmd = curcmd;
	/* read command til end */
	/* a '!' found in beginning of command is replaced with the saved command.
	   a '%' found in command is replaced with the current filename */
	c=getchr();
	if (c == '!') {
		if (savcmd[0]==0) 
			error(56);
		else {
			psavcmd = savcmd;
			while (*pcurcmd++ = *psavcmd++);
			--pcurcmd;
			shflg = 1;
		}
	}
	else UNGETC(c);  /* put c back */
	while (endflg==1) {
		while ((c=getchr()) != '\n' && c != '%' && c != '\\')
			*pcurcmd++ = c;
		if (c=='%') { 
			if (savedfile[0]==0)
				error(21);
			else {
				psavedfile = savedfile;
				while(pcurcmd < curcmd + LBSIZE && (*pcurcmd++ = *psavedfile++));
				--pcurcmd;
				shflg = 1;
			}
		}
		else if (c == '\\') {
			c = getchr();
			if (c != '%')
				*pcurcmd++ = '\\';
			*pcurcmd++ = c;
		}
		else
			/* end of command hit */
			endflg = 0;
	}
	*pcurcmd++ = 0;
	if (shflg == 1)
		Puts(curcmd);
	/* save command */
	strcpy(savcmd, curcmd);

	if ((pid = fork()) == 0) {
		signal(SIGHUP, oldhup);
		signal(SIGQUIT, oldquit);
		close(tfile);
/* sgi - use _PATH_BSHELL ("/sbin/sh") not "/usr/bin/sh". 17 Jan 92. pj */
		execlp(_PATH_BSHELL, "sh", "-c", curcmd, (char *) 0);
		exit(0100);
	}
	savint = signal(SIGINT, SIG_IGN);
	while ((rpid = wait(&retcode)) != pid && rpid != (pid_t)-1);
	signal(SIGINT, savint);
	if (vflag) Puts("!");
}

char	line[70];
char	*linp = line;

void
quit(int sig)
{
	if (vflag && fchange) {
		fchange = 0;
		if(flag28){flag28 = 0; error(62);} /* For case where user reads
					in BOTH a good file & a bad file */
		error(1);
	}
	unlink(tfname);
	if (kflag)
		crypt_close(perm);
	if (xtflag)
		crypt_close(tperm);
	if (linp > line)
		putchr('\n');	/* Flush the outstanding content */
	exit(errcnt? 2: 0);
}

void
Delete(void)
{
	setdot();
	newline();
	nonzero();
	if (!globflg) save();
	rdelete(addr1, addr2);
}

void
rdelete(LINE ad1, LINE ad2)
{
	register LINE a1, a2, a3;

	a1 = ad1;
	a2 = ad2+1;
	a3 = dol;
	dol -= a2 - a1;
	do
		a1++->cur = a2++->cur;
	while (a2 <= a3);
	a1 = ad1;
	if (a1 > dol)
		a1 = dol;
	dot = a1;
	fchange = 1;
}

void
gdelete(void)
{
	register LINE a1, a2, a3;

	a3 = dol;
	for (a1=zero+1; (a1->cur&01)==0; a1++)
		if (a1>=a3)
			return;
	for (a2=a1+1; a2<=a3;) {
		if (a2->cur&01) {
			a2++;
			dot = a1;
		} else
			a1++->cur = a2++->cur;
	}
	dol = a1-1;
	if (dot>dol)
		dot = dol;
	fchange = 1;
}

char *
getline(long tl)
{
	register char *bp, *lp;
	register nl;

	lp = linebuf;
	bp = getblock(tl, READ);
	nl = nleft;
	tl &= ~0377;
	while (*lp++ = *bp++)
		if (--nl == 0) {
			bp = getblock(tl+=0400, READ);
			nl = nleft;
		}
	return(linebuf);
}

long
putline(void)
{
	register char *bp, *lp;
	register nl;
	long tl;

	fchange = 1;
	lp = linebuf;
	tl = tline;
	bp = getblock(tl, WRITE);
	nl = nleft;
	tl &= ~0377;
	while (*bp = *lp++) {
		if (*bp++ == '\n') {
			*--bp = 0;
			linebp = lp;
			break;
		}
		if (--nl == 0) {
			bp = getblock(tl+=0400, WRITE);
			nl = nleft;
		}
	}
	nl = tline;
	tline += (((lp-linebuf)+03)>>1)&077776;
	return(nl);
}

char *
getblock(long atl,int iof)
{
	register bno, off;
	register char *p1, *p2;
	register int n;
	
	bno = (atl>>8)&077777;
	off = (atl<<1)&0774;
	if (bno >= 65534) {
		lastc = '\n';
		error(31);
	}
	nleft = 512 - off;
	if (bno==iblock) {
		ichanged |= iof;
		return(ibuff+off);
	}
	if (bno==oblock)
		return(obuff+off);
	if (iof==READ) {
		if (ichanged) {
			if(xtflag)
				if(run_crypt(0L, ibuff, 512, tperm) == -1)
					error(63);
			blkio(iblock, ibuff, (blkio_func_t)write);
		}
		ichanged = 0;
		iblock = bno;
		blkio(bno, ibuff, (blkio_func_t)read);
		if(xtflag)
			if(run_crypt(0L, ibuff, 512, tperm) == -1) 
				error(63);
		return(ibuff+off);
	}
	if (oblock>=0) {
		if(xtflag) {
			p1 = obuff;
			p2 = crbuf;
			n = 512;
			while(n--)
				*p2++ = *p1++;
			if(run_crypt(0L, crbuf, 512, tperm) == -1)
				error(63);
			blkio(oblock, crbuf, (blkio_func_t)write);
		} else
			blkio(oblock, obuff, (blkio_func_t)write);
	}
	oblock = bno;
	return(obuff+off);
}

void
blkio(int b,char *buf,blkio_func_t iofcn)
{
	lseek(tfile, (long)b<<9, 0);
	if ((*iofcn)(tfile, buf, 512) != 512) {
		if(dol != zero)error(32); /* Bypass this if writing null file */
	}
}

void
init(void)
{
	long *markp;
	mode_t omask;

	close(tfile);
	tline = 2;
	for (markp = names; markp < &names[26]; )
		*markp++ = 0L;
	subnewa = 0L;
	anymarks = 0;
	iblock = -1;
	oblock = -1;
	ichanged = 0;
	initflg = 1;
	omask = umask(0);
	tfile = open(tfname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	umask(omask);
	if(xflag) {
		xtflag = 1;
		if(tpermflag)
			(void)crypt_close(tperm);
		tpermflag = 1;
		if(makekey(tperm)) {
			xtflag = 0;
			pfmt(stdout, msgtab[65].sev|MM_NOGET,
				gettxt(msgtab[65].id, msgtab[65].msg));
		}
	}
	brk((char *)fendcore);
	dot = zero = dol = savdot = savdol = fendcore;
	flag28 = save28 = 0;
	endcore = fendcore - sizeof(struct lin);
}

void
global(int k)
{
	register char *gp;
	wchar_t l;
	char multic[MULTI_BYTE_MAX];
	register c;
	register LINE a1;
	char globuf[ESIZE];
	register int n;

	if (globp)
		error(33);
	setall();
	nonzero();
	if((n = mbftowc(multic, &l, getchr, &peekc)) <= 0)
		error(67);
	if (l == '\n')
		error(19);
	save();
	comple(l);
	gp = globuf;
	while ((c = getchr()) != '\n') {
		if (c==EOF)
			error(19);
		if (c=='\\') {
			c = getchr();
			if (c!='\n')
				*gp++ = '\\';
		}
		*gp++ = c;
		if (gp >= &globuf[GBSIZE-2])
			error(34);
	}
	if (gp == globuf)
		*gp++ = 'p';
	*gp++ = '\n';
	*gp++ = 0;
	for (a1=zero; a1<=dol; a1++) {
		a1->cur &= ~01;
		if (a1>=addr1 && a1<=addr2 && execute(0, a1)==k)
			a1->cur |= 01;
	}
	/*
	 * Special case: g/.../d (avoid n^2 algorithm)
	 */
	if (globuf[0]=='d' && globuf[1]=='\n' && globuf[2]=='\0') {
		gdelete();
		return;
	}
	for (a1=zero; a1<=dol; a1++) {
		if (a1->cur & 01) {
			a1->cur &= ~01;
			dot = a1;
			globp = globuf;
			globflg = 1;
			commands();
			globflg = 0;
			a1 = zero;
		}
	}
}

void
join(void)
{
	register char *gp, *lp;
	register LINE a1;

	if (addr1 == addr2) return;
	gp = genbuf;
	for (a1=addr1; a1<=addr2; a1++) {
		lp = getline(a1->cur);
		while (*gp = *lp++)
			if (gp++ >= &genbuf[LINE_MAX-2])
				error(27);
	}
	lp = linebuf;
	gp = genbuf;
	while (*lp++ = *gp++);
	addr1->cur = putline();
	if (addr1<addr2)
		rdelete(addr1+1, addr2);
	dot = addr1;
}

void
substitute(int inglob)
{
	register nl;
	register LINE a1;
	long *markp;
	int getsub();
	int ingsav;		/* For saving arg. */

	ingsav = inglob;
	ocerr2 = 0;
	gsubf = compsub();
	for (a1 = addr1; a1 <= addr2; a1++) {
		if (execute(0, a1)==0)
			continue;
		numpass = 0;
		ocerr1 = 0;
		inglob |= 01;
		dosub();
		if (gsubf) {
			while (*loc2) {
				if (execute(1, (LINE )0)==0)
					break;
				dosub();
			}
		}
		if(ocerr1 == 0)continue;	/* Don't put out-not changed. */
		subnewa = putline();
		a1->cur &= ~01;
		if (anymarks) {
			for (markp = names; markp < &names[26]; markp++)
				if (*markp == a1->cur)
					*markp = subnewa;
		}
		a1->cur = subnewa;
		append(getsub, a1);
		nl = nline;
		a1 += nl;
		addr2 += nl;
	}
	if(ingsav)return;	/* Was in global-no error msg allowed. */
	if (inglob==0)
		error(35);	/* Not in global, but not found. */
	if(ocerr2 == 0)error(35); /* RE found, but occurrence match failed. */
}

int
compsub(void)
{
	register int c;
	wchar_t seof; 
	register char *p;
	char multic[MULTI_BYTE_MAX];
	register int n;
	static char remem[RHSIZE];
	static int remflg = -1;
	int i;

	if((n = mbftowc(multic, &seof, getchr, &peekc)) <= 0)
		error(67);
	if (seof == '\n' || seof == ' ')
		error(36);
	comple(seof);
	p = rhsbuf;
	for (;;) {
		wchar_t cl;
		if((n = mbftowc(multic, &cl, getchr, &peekc)) <= 0)
			error(67);
		if (cl=='\\') {
			*p++ = '\\';
			if (p >= &rhsbuf[RHSIZE])
				error(38);
			if((n = mbftowc(multic, &cl, getchr, &peekc)) <= 0)
				error(67);
		} else if (cl=='\n') {
			if(nodelim == 1) {
				nodelim = 0;
				error(36);
			}
			if (!(globp && globp[0])) {
				UNGETC('\n');
				pflag++;
				break;
			}
		}
		else if (cl==seof)
			break;
		if(p + n > &rhsbuf[RHSIZE])
			error(38);
		(void)strncpy(p, multic, n);
		p += n;
	}
	*p++ = 0;
	if(rhsbuf[0] == '%' && rhsbuf[1] == 0)
		/*
		 * If there isn't a remembered string, it is an error;
		 * otherwise the right hand side is the previous right
		 * hand side.
		 */
		
		if (remflg == -1) 
			error (55);
		else 
			strcpy(rhsbuf, remem);
	else {
		strcpy(remem, rhsbuf);
		remflg = 0;
	}
	c = 0;
	peekc = getchr();	/* Gets char after third delimiter. */
	if(peekc == 'g'){c = 999; peekc = 0;}
	if(peekc >= '1' && peekc <= '9')
		{c = peekc-'0';
		peekc = 0;	/* Allows getchr() to get next char. */
		while(1)
			{i = getchr();
			if(i < '0' || i > '9')break;
			c = c*10 + i-'0';
			if(c > 512)error(20);	/* "Illegal suffix" */
			}
		peekc = i;	/* Effectively an unget. */
		}
	newline();
	return(c);	/* Returns occurrence value. 0 & 1 both do first
			occurrence only: c=0 if ordinary substitute; c=1
			if use 1 in global sub (s/a/b/1). 0 in global
			form is illegal. */
}

int
getsub(void)
{
	register char *p1, *p2;

	p1 = linebuf;
	if ((p2 = linebp) == 0)
		return(EOF);
	while (*p1++ = *p2++);
	linebp = 0;
	return(0);
}

void
dosub(void)
{
	register char *lp, *sp, *rp;
	int c;

	if(gsubf > 0 && gsubf < 999)
		{numpass++;
		if(gsubf != numpass)return;
		}
	ocerr1++;
	ocerr2++;
	lp = linebuf;
	sp = genbuf;
	rp = rhsbuf;
	while (lp < loc1)
		*sp++ = *lp++;
	while (c = *rp++) {
		if (c=='&') {
			sp = place(sp, loc1, loc2);
			continue;
		} else if(c == '\\') {
			c = *rp++;
			if(c >= '1' && c < re.Nbra + '1') {
				sp = place(sp, braslist[c-'1'], braelist[c-'1']);
				continue;
			}
		}
		*sp++ = c;
		if (sp >= &genbuf[LINE_MAX])
			error(27);
	}
	lp = loc2;
	loc2 = sp - genbuf + linebuf;
	while (*sp++ = *lp++)
		if (sp >= &genbuf[LINE_MAX])
			error(27);
	lp = linebuf;
	sp = genbuf;
	while (*lp++ = *sp++);
}

char *
place(char *sp, char *l1, char *l2)
{

	while (l1 < l2) {
		*sp++ = *l1++;
		if (sp >= &genbuf[LINE_MAX])
			error(27);
	}
	return(sp);
}

void
comple(wchar_t seof)
{
	int cclass = 0;
	wchar_t c; 
	register int n;
	register char *cp = genbuf;
	char multic[MULTI_BYTE_MAX];
	int	ret;
	char    *bstr;
	int re_flag = 0;

	while(1) {
		if((n = mbftowc(multic, &c, getchr, &peekc)) < 0)
			error1(67);
		if(n == 0 || c == '\n') {
			if(cclass)
				error1(49);
			else
				break;
		}
		if(c == seof && !cclass)
			break;
		if(cclass && c == ']') {
			cclass = 0;
			if(cp >= &genbuf[LINE_MAX-2])
				error1(50);
			*cp++ = ']';	
			continue;
		}
		if(c == '[' && !cclass) {
			cclass = 1;
			if(cp > &genbuf[LINE_MAX-2])
				error1(50);
			*cp++ = '[';
			if((n = mbftowc(multic, &c, getchr, &peekc)) < 0)
				error1(67);
			if(n == 0 || c == '\n') 
				error1(49);
		}
		if(c == '\\' && !cclass) {
			if(cp > &genbuf[LINE_MAX-2])
				error1(50);
			*cp++ = '\\';
			if((n = mbftowc(multic, &c, getchr, &peekc)) < 0)
				error1(67);
			if(n == 0 || c == '\n') 
				error1(36);	
		}
		if(cp + n > &genbuf[LINE_MAX-2])
			error1(50);
		(void)strncpy(cp, multic, n);
		cp += n;
	}
	*cp = '\0';
	if(n != 0 && c == '\n') 
		UNGETC('\n');
	if(n == 0 || c == '\n')
		nodelim = 1;

	if (cp == genbuf)
		return;		/* NULL RE */

	/* word boundary search */
	while(bstr = strstr(genbuf, "\\<")) {
		char tmpbuf[GBSIZE];
		char    wbow[] = "[[:<:]]"; /* Map to regcomp() syntax */
		register int    i;

		strcpy(tmpbuf,genbuf);
		i = bstr - genbuf;
		strcpy(&tmpbuf[i], wbow);
		strcat(tmpbuf,&genbuf[i+2]);
		strcpy(genbuf,tmpbuf);
	}
	while(bstr = strstr(genbuf, "\\>")) {
		char tmpbuf[GBSIZE];
		char	weow[] = "[[:>:]]";	/* Map to regcomp() syntax */
		register int    i;

		strcpy(tmpbuf,genbuf);
		i = bstr - genbuf;
		strcpy(&tmpbuf[i], weow);
		strcat(tmpbuf,&genbuf[i+2]);
		strcpy(genbuf,tmpbuf);
	}
	re.nore = 0;
	re.nomatch = 0;
	ret=regcomp(&re.preg, genbuf, re_flag);
	if(ret) {
		char msg[REG_MSGLEN];

		re.nore = 1;
		(void)regerror(ret, &re.preg, msg, REG_MSGLEN);
		error1(51);
	}
	re.Nbra = re.preg.re_nsub;
	
}

void
move(int cflag)
{
	register LINE adt, ad1, ad2;
	int getcopy();

	setdot();
	nonzero();
	if ((adt = address())==0)
		error(39);
	newline();
	if (!globflg) save();
	if (cflag) {
		ad1 = dol;
		append(getcopy, ad1++);
		ad2 = dol;
	} else {
		ad2 = addr2;
		for (ad1 = addr1; ad1 <= ad2;)
			ad1++->cur &= ~01;
		ad1 = addr1;
	}
	ad2++;
	if (adt<ad1) {
		dot = adt + (ad2-ad1);
		if ((++adt)==ad1)
			return;
		reverse(adt, ad1);
		reverse(ad1, ad2);
		reverse(adt, ad2);
	} else if (adt >= ad2) {
		dot = adt++;
		reverse(ad1, ad2);
		reverse(ad2, adt);
		reverse(ad1, adt);
	} else
		error(39);
	fchange = 1;
}

void
reverse(LINE a1, LINE a2)
{
	long t;

	for (;;) {
		t = (--a2)->cur;
		if (a2 <= a1)
			return;
		a2->cur = a1->cur;
		a1++->cur = t;
	}
}

int
getcopy(void)
{

	if (addr1 > addr2)
		return(EOF);
	getline(addr1++->cur);
	return(0);
}


void
error1(int code)
{
	expbuf[0] = expbuf[1] = 0;
	re.Nbra = 0;
	error(code);
}

int
execute(int gf, LINE addr)
{
	register char *p1; 
	register int c, i;
	regmatch_t pmatch[NBRA+1];
	size_t  nmatch = (size_t)NBRA;
	int re_flag = 0;
	int ret;

	if (gf) {
		if (re.nomatch)
			return (0);

		locs = p1 = loc2;
	} else {
		if (addr==zero)
			return(0);
		p1 = getline(addr->cur);
		locs = 0;
	}
	for (c=0; c<NBRA; c++) {
		braslist[c] = NULL;
		braelist[c] = NULL;
	}
	re.nomatch = 0;
	ret=regexec(&re.preg, (char *)p1, (size_t)nmatch, pmatch, re_flag);
	if (ret && (ret == REG_NOMATCH))
		re.nomatch = 1;
	if (!ret) {
		loc1 = p1 + pmatch[0].rm_so;
		loc2 = p1 + pmatch[0].rm_eo;
		for (i = 0; i < re.Nbra && i < NBRA ; i++) {
			braslist[i] = p1 + pmatch[i+1].rm_so;
			braelist[i] = p1 + pmatch[i+1].rm_eo;
		}
	}

	return(!ret);
}


void
putd(void)
{
	register r;

	r = (int)(count%10);
	count /= 10;
	if (count)
		putd();
	putchr(r + '0');
}

void
Puts(const char *sp)
{
	register int n;
	wchar_t c;
	int sz,i;
	if (fss.Ffill && (listf == 0)) {
		if ((i = expnd(sp,funny,&sz,&fss)) == -1) {
			write(1,funny,fss.Flim & 0377); putchr('\n');
			pfmt(stdout, MM_ERROR, ":231:too long");
		}
		else
			write(1, funny, sz);
		putchr('\n');
		if (i == -2) pfmt(stdout, MM_ERROR,":232:Tab count\n",10);
		return;
	}
	col = 0;
	while (n = mbtowc(&c, sp, MULTI_BYTE_MAX)) {
		if(n == -1) 
			putoctal((unsigned char)*sp++);
		else {
			putchr(c);
			sp += n;
		}
	}
	putchr('\n');
}

static wchar_t ESCAPES[] = {'\a','\b','\f','\n','\r','\t','\v','\\',
'\0'};
static char *ESCCHARS = "abfnrtv\\";

void
putchr(wchar_t ac)
{
	register char *lp;
	register wchar_t c;
	short len;
	int width, i;

	lp = linp;
	c = ac;
	if ( listf ) {
		width = scrwidth(c);

		for (i = 0; ESCAPES[i] ; i++)
			if (ESCAPES[i] == c)
				break;

		if (!width) {
			if ( (xpg4command && !ESCAPES[i]) ||
				 (!xpg4command &&
				 (c != '\n' && c != '\t' && c != '\b'))) {
				putoctal(c);
				return;
			}
		}
		if (col + width >= 72) {
			col = 0;
			*lp++ = '\\';
			*lp++ = '\n';
		}
		if (xpg4command) {
			if (ESCAPES[i]) {
				if (c == '\n') {
					if(!Gcmd){
						*lp++ = '$';
						col++;
					}
				} else {
					*lp++ = '\\';
					*lp++ = *(ESCCHARS + i);
					col++;
					goto out;
				}
			}
		} else {
			if (c=='\t') {
				c = '>';
				goto esc;
			} else if (c=='\b') {
				c = '<';
			esc:
				*lp++ = '-';
				*lp++ = '\b';
				*lp++ = c;
				col++;
				goto out;
			}
		}
	}
	col += width;
	/* we know there is enough room since &lp[70] - &lp[64] > 
	   MULTI_BYTE_MAX */
	lp += wctomb(lp, c);
out:
	if(c == '\n' || lp >= &line[64]) {
		linp = line;
		len = lp - line;
		write(1, line, len);
		return;
	}
	linp = lp;
}

void
putoctal(unsigned char c)
{
	register char *lp;
	int len;
	lp = linp;
	if(listf) {
		if (col + 1 >= 72) {
			col = 0;
			*lp++ = '\\';
			*lp++ = '\n';
		}
		*lp++ = '\\';
		*lp++ = (((int)c >> 6) & 03) + '0';
		*lp++ = (((int)c >> 3) & 07) + '0';
		*lp++ = (c & 07) + '0';
		col += 4;
	} else {
		*lp++=c;
		col++;
	}
	if(lp >= &line[64]) {
		linp = line;
		len = lp - line;
		write(1, line, len);
		return;
	}
	linp = lp;
}

char *
getkey(char *prompt)
{
	struct termio b;
	int save;
	void (*sig)();
	static char key[KSIZE+1]; 
	register char *p;
	int c;

	sig = signal(SIGINT, SIG_IGN);
	ioctl(0, TCGETA, &b);
	save = b.c_lflag;
	b.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	ioctl(0, TCSETAW, &b);
	write(1, prompt, strlen(prompt));
	p = key;
	while(((c=getchr()) != EOF) && (c!='\n')) {
		if(p < &key[KSIZE])
			*p++ = c;
	}
	*p = 0;
	write(1, "\n", 1);
	b.c_lflag = save;
	ioctl(0, TCSETAW, &b);
	signal(SIGINT, sig);
	return(key);
}

void
globaln(int k)
{
	register char *gp;
	register c;
	register int n;
	wchar_t cl;
	register LINE a1;
	int  nfirst;
	char globuf[ESIZE];
	char multic[MULTI_BYTE_MAX];

	if (globp)
		error(33);
	setall();
	nonzero();
	if((n = mbftowc(multic, &cl, getchr, &peekc)) <= 0)
		error(67);
	if (cl == '\n')
		error(19);
	save();
	comple(cl);
	for (a1=zero; a1<=dol; a1++) {
		a1->Gflag &= ~01;
		if (a1>=addr1 && a1<=addr2 && execute(0, a1)==k)
			a1->Gflag |= 01;
	}
	nfirst = 0;
	newline();
	for (a1=zero; a1<=dol; a1++) {
		if (a1->Gflag & 01) {
			a1->Gflag &= ~01;
			dot = a1;
			Puts(getline(a1->cur));
			if ((c=getchr()) == EOF)
				error(52);
			if(c=='a' || c=='i' || c=='c')
				error(53);
			if (c == '\n') {
				a1 = zero;
				continue;
			}
			if (c != '&') {
				char	*startsl, *endsl;

				startsl = endsl = NULL;
				gp = globuf;
				*gp++ = c;
				while ((c = getchr()) != '\n') {
					if (!startsl && c == '/')
						startsl = gp;
					else if (startsl && !endsl && c == '/')
						endsl = gp;

					if (c=='\\') {
						c = getchr();
						if (c!='\n')
							*gp++ = '\\';
					}
					*gp++ = c;
					if (((startsl && endsl) &&
						 (endsl > (startsl+GBSIZE+1))) ||
						(gp >= &globuf[ESIZE]))
						error(34);
				}
				*gp++ = '\n';
				*gp++ = 0;
				nfirst = 1;
			}
			else
				if ((c=getchr()) != '\n')
					error(54);
			globp = globuf;
			if (nfirst) {
				globflg = 1;
				commands();
				globflg = 0;
			}
			else error(56);
			globp = 0;
			a1 = zero;
		}
	}
}

int
eopen(char *string, int rw)
{
#define w_or_r(a,b) (rw?a:b)
	int pf[2];
	pid_t i;
	int io;
	int chcount;	/* # of char read. */
	char *fp;

	if (rflg) {	/* restricted shell */
		if (Xqt) {
			Xqt = 0;
			error(6);
		}
	}
	if(!Xqt) {
		if((io=open(string, rw)) >= 0) {
			if (fflg) {
				chcount = read(io,funny,LINE_MAX);
				if(crflag == -1) {
					if(isencrypt(funny, chcount)) 
						crflag = 2;
					else
						crflag = -2;
				}
				if(crflag > 0)
					if(run_crypt(0L, funny, chcount, perm) == -1)
						error(63);
				if (fspec(funny,&fss,0) < 0) {
					fss.Ffill = 0;
					fflg = 0;
					error(4);
				}
				lseek(io,0L,0);
			}
		}
		fflg = 0;
		return(io);
	}
	if(pipe(pf) < 0)
xerr:		error(0);
	if((i = fork()) == 0) {
		signal(SIGHUP, oldhup);
		signal(SIGQUIT, oldquit);
		signal(SIGPIPE, oldpipe);
		signal(SIGINT, (void (*)()) 0);
		close(w_or_r(pf[1], pf[0]));
		close(w_or_r(0, 1));
		dup(w_or_r(pf[0], pf[1]));
		close(w_or_r(pf[0], pf[1]));
/* sgi - use _PATH_BSHELL ("/sbin/sh") not "/usr/bin/sh". 17 Jan 92. pj */
		execlp(_PATH_BSHELL, "sh", "-c", string, (char *) 0);
		exit(1);
	}
	if(i == (pid_t)-1)
		goto xerr;
	close(w_or_r(pf[0], pf[1]));
	return w_or_r(pf[1], pf[0]);
}

void
eclose(int f)
{
	close(f);
	if(Xqt)
		Xqt = 0, wait((int *) 0);
}

void
mkfunny(void)
{
	register char *p, *p1, *p2;

	p2 = p1 = funny;
	p = file;
	/*
	 * Go to end of file name
	 */
	while(*p)
		p++;
	while(*--p  == '/')	/* delete trailing slashes */
		*p = '\0';
	/*
	 * go back to beginning of file
	 */
	p = file;
	/*
	 * Copy file name to funny setting p2 at
	 * basename of file.
	 */
	while (*p1++ = *p)
		if (*p++ == '/') p2 = p1;
	/*
	 * Set p1 to point to basename of tfname.
	 */
 	p1 = strrchr(tfname,'/');
	if (strlen(tfname) > 6)
		p1 = &tfname[strlen(tfname)-6];
	p1++;
	*p2 = '\007';	/* add unprintable char to make funny a unique name */
	/*
	 * Copy tfname to file.
	 */
	while (*++p2 = *p1++);
}

void
getime(void) /* get modified time of file and save */
{
	if (stat(file,&Fl) < 0)
		savtime = 0;
	else
		savtime = Fl.st_mtime;
}

void
chktime(void) /* check saved mod time against current mod time */
{
	if (savtime != 0 && Fl.st_mtime != 0) {
		if (savtime != Fl.st_mtime)
			error(58);
	}
}

void
newtime(void) /* get new mod time and save */
{
	stat(file,&Fl);
	savtime = Fl.st_mtime;
}

void
red(char *op) /* restricted - check for '/' in name */
        /* and delete trailing '/' */
{
	register char *p;

	p = op;
	while(*p)
		if(*p++ == '/'&& rflg) {
			*op = 0;
			error(6);
		}
	/* delete trailing '/' */
	while(p > op) {
		if (*--p == '/')
			*p = '\0';
		else break;
	}
}


char *fsp;
int fsprtn;

int
fspec(char *line,struct Fspec *f,int up)
{
	struct termio arg;
	register int havespec, n;

	if(!up) clear(f);

	havespec = fsprtn = 0;
	for(fsp=line; *fsp && *fsp != '\n'; fsp++)
		switch(*fsp) {

			case '<':       if(havespec) return(-1);
					if(*(fsp+1) == ':') {
						havespec = 1;
						clear(f);
						if(!ioctl(1, TCGETA, &arg) &&
							((arg.c_oflag&TAB3) == TAB3))
						  f->Ffill = 1;
						fsp++;
						continue;
					}

			case ' ':       continue;

			case 's':       if(havespec && (n=numb()) >= 0)
						f->Flim = n;
					continue;

			case 't':       if(havespec) targ(f);
					continue;

			case 'd':       continue;

			case 'm':       if(havespec)  n = numb();
					continue;

			case 'e':       continue;
			case ':':       if(!havespec) continue;
					if(*(fsp+1) != '>') fsprtn = -1;
					return(fsprtn);

			default:	if(!havespec) continue;
					return(-1);
		}
	return(1);
}


int
numb(void)
{
	register int n;

	n = 0;
	while(*++fsp >= '0' && *fsp <= '9')
		n = 10*n + *fsp-'0';
	fsp--;
	return(n);
}

void
targ(struct Fspec *f)
{

	if(*++fsp == '-') {
		if(isdigit(*(fsp+1))) tincr(numb(),f);
		else tstd(f);
		return;
	}
	if(isdigit(*fsp)) {
		tlist(f);
		return;
	}
	fsprtn = -1;
	fsp--;
	return;
}

void
tincr(int n,struct Fspec *f)
{
	register int l, i;

	l = 1;
	for(i=0; i<20; i++)
		f->Ftabs[i] = l += n;
	f->Ftabs[i] = 0;
}

void
tstd(struct Fspec *f)
{
	char std[3];

	std[0] = *++fsp;
	if (isdigit(*(fsp+1)))  {
						std[1] = *++fsp;
						std[2] = '\0';
	}
	else std[1] = '\0';
	fsprtn = stdtab(std,f->Ftabs);
	return;
}

void
tlist(struct Fspec *f)
{
	register int n, last, i;

	fsp--;
	last = i = 0;

	do {
		if((n=numb()) <= last || i >= 20) {
			fsprtn = -1;
			return;
		}
		f->Ftabs[i++] = last = n;
	} while(*++fsp == ',');

	f->Ftabs[i] = 0;
	fsp--;
}

int
expnd(char *line,char *buf,int *sz,struct Fspec *f)
{
	register char *l, *t;
	register int b;

	l = line - 1;
	b = 1;
	t = f->Ftabs;
	fsprtn = 0;

	while(*++l && *l != '\n' && b < 511) {
		if(*l == '\t') {
			while(*t && b >= *t) t++;
			if (*t == 0) fsprtn = -2;
			do buf[b-1] = ' '; while(++b < *t);
		}
		else buf[b++ - 1] = *l;
	}

	buf[b] = '\0';
	*sz = b;
	if(*l != '\0' && *l != '\n') {
		buf[b-1] = '\n';
		return(-1);
	}
	buf[b-1] = *l;
	if(f->Flim && b-1 > (int)f->Flim) return(-1);
	return(fsprtn);
}

void
clear(struct Fspec *f)
{
	f->Ftabs[0] = f->Fdel = f->Fmov = f->Ffill = 0;
	f->Flim = 0;
}

int
lenchk(char *line, struct Fspec *f)
{
	register char *l, *t;
	register int b;

	l = line - 1;
	b = 1;
	t = f->Ftabs;

	while(*++l && *l != '\n' && b < 511) {
		if(*l == '\t') {
			while(*t && b >= *t) t++;
			while(++b < *t);
		}
		else b++;
	}

	if((*l!='\0'&&*l!='\n') || (f->Flim&&b-1>(int)f->Flim))
		return(-1);
	return(0);
}
#define NTABS 21

/*      stdtabs: standard tabs table
	format: option code letter(s), null, tabs, null */
char stdtabs[] = {
'a',    0,1,10,16,36,72,0,      		/* IBM 370 Assembler */
'a','2',0,1,10,16,40,72,0,      		/* IBM Assembler alternative*/
'c',    0,1,8,12,16,20,55,0,    		/* COBOL, normal */
'c','2',0,1,6,10,14,49,0,       		/* COBOL, crunched*/
'c','3',0,1,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,67,0,
'f',    0,1,7,11,15,19,23,0,    		/* FORTRAN */
'p',    0,1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,0, /* PL/I */
's',    0,1,10,55,0,    			/* SNOBOL */
'u',    0,1,12,20,44,0, 			/* UNIVAC ASM */
0};
/*      stdtab: return tab list for any "canned" tab option.
	entry: option points to null-terminated option string
		tabvect points to vector to be filled in
	exit: return(0) if legal, tabvect filled, ending with zero
		return(-1) if unknown option
*/

int
stdtab(char *option,char tabvect[NTABS])
{
	char *scan;
	tabvect[0] = 0;
	scan = stdtabs;
	while (*scan)
		{
		if (strequal(&scan,option))
			{strcopy(scan,tabvect);break;}
		else while(*scan++);    /* skip over tab specs */
		}
/*      later: look up code in /etc/something */
	return(tabvect[0]?0:-1);
}

/*      strequal: checks strings for equality
	entry: scan1 points to scan pointer, str points to string
	exit: return(1) if equal, return(0) if not
		*scan1 is advanced to next nonzero byte after null
*/
int
strequal(char **scan1,char *str)
{
	char c, *scan;
	scan = *scan1;
	while ((c = *scan++) == *str && c) str++;
	*scan1 = scan;
	if (c == 0 && *str == 0) return(1);
	if (c) while(*scan++);
	*scan1 = scan;
	return(0);
}

/*      strcopy: copy source to destination */

void
strcopy(char *source,char *dest)
{
	while (*dest++ = *source++);
	return;
}

/* This is called before a buffer modifying command so that the */
/* current array of line ptrs is saved in sav and dot and dol are saved */
void
save(void)
{
	LINE i;
	int	j;

	savdot = dot;
	savdol = dol;
	for (j=0; j <= 25; j++)
		savnames[j] = names[j];

	for (i=zero+1; i<=dol; i++)
		i->sav = i->cur;
	initflg = 0;
}

/* The undo command calls this to restore the previous ptr array sav */
/* and swap with cur - dot and dol are swapped also. This allows user to */
/* undo an undo */
void
undo(void)
{
	int j; 
	long tmp;
	LINE i, tmpdot, tmpdol;

	tmpdot = dot; dot = savdot; savdot = tmpdot;
	tmpdol = dol; dol = savdol; savdol = tmpdol;
	/* swap arrays using the greater of dol or savdol as upper limit */
	for (i=zero+1; i<=((dol>savdol) ? dol : savdol); i++) {

		tmp = i->cur;
		i->cur = i->sav;
		i->sav = tmp;
	}
	/*
	 * If the current text lines are swapped with the
	 * text lines in the save buffer, then swap the current
	 * marks with those in the save area.
	 */

	for (j=0; j <= 25; j++) {
		tmp = names[j];
		names[j] = savnames[j];
		savnames[j] = tmp;
	}
}
