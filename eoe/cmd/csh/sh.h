/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.9 $"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code Were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.

		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley Software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#if 0
#define	TRACE			/* TRACE on/off */
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <sys/siginfo.h>
#include <sys/param.h>
#include <sys/stat.h>
#define	_SYS_TTOLD_H	1
#include <sys/resource.h>
#include <sys/termios.h>
#include <limits.h>
#include <dirent.h>
#include <stdlib.h>		/* mb..., wc... */
#include <errno.h>
#include <signal.h>		/* std sysV signal.h */
#include <setjmp.h>
#include <string.h>
#include <bstring.h>
#include <msgs/uxsgicsh.h>
#include "sh.local.h"
#include "sh.char.h"


#if !defined(MB_LEN_MAX) || !defined(MB_CUR_MAX)
	Error: I need both ANSI macros!
#endif

/*
 * C shell
 *
 * Bill Joy, UC Berkeley
 * October, 1978; May 1980
 *
 * Jim Kulp, IIASA, Laxenburg Austria
 * April, 1980
 *
 * internationalization by
 * Frank Berndt, frank@ceres.esd.sgi.com
 */

#define	isdir(d)	(((d).st_mode & S_IFMT) == S_IFDIR)

typedef	char	bool;

/*
 * always wide characters used
 * The QUOTE bit tells whether the character is subject to further
 * interpretation such as history substitution, file mathing, command
 * subsitution.  TRIM is a mask to strip off the QUOTE bit.
 *
 * This is a bug ! wchar_t must not be changed.
 */
#define QUOTE	0x80000000
#define	TRIM	((wchar_t)(QUOTE - 1))

/*
 * Since QUOTE is being used for wide characters, we need REASON_QUOTE that
 * will be used to indicate the exit status value for interrupted/killed children.
 */
#define REASON_QUOTE	0x80

#define	eq(a, b)	(!wscmp(a, b))

#define	MB_MAXPATH	(MAXPATHLEN * MB_LEN_MAX)

/*
 * Global flags
 */
extern bool	chkstop;	/* Warned of stopped jobs... allow exit */
extern bool	didfds;		/* Have setup i/o fd's for child */
extern bool	doneinp;	/* EOF indicator after reset from readc */
extern bool	exiterr;	/* Exit if error or non-zero exit status */
extern bool	child;		/* Child shell ... errors cause exit */
extern bool	haderr;		/* Reset was because of an error */
extern bool	intty;		/* Input is a tty */
extern bool	intact;		/* We are interactive... therefore prompt */
extern bool	justpr;		/* Just print because of :p hist mod */
extern bool	loginsh;	/* We are a loginsh -> .login/.logout */
extern bool	neednote;	/* Need to pnotify() */
extern bool	noexec;		/* Don't execute, just syntax check */
extern bool	pjobs;		/* want to print jobs if interrupted */
extern bool	setintr;	/* Set interrupts on/off -> Wait intr... */
extern bool	timflg;		/* Time the next waited for command */
extern bool	havhash;	/* path hashing is available */
extern bool	filec;		/* doing filename expansion */

/*
 * Global i/o info
 */
extern wchar_t	*arginp;	/* Argument input for sh -c and internal `xx` */
extern int	onelflg;	/* 2 -> need line for -t, 1 -> exit on read */
extern wchar_t	*file;		/* Name of shell file for $0 */

extern char	*errstr;	/* Error message from scanner/parser */
extern int	errno;		/* Error from C library routines */
extern wchar_t	*shtemp;	/* Temp name for << shell files in /tmp */
extern struct	timeval time0;	/* Time at which the shell started */
extern struct	rusage ru0;

extern char	cmd_label[];	/* label of csh */

/*
 * Miscellany
 */
extern wchar_t	*doldol;	/* Character pid for $$ */
extern int	uid;		/* Invokers uid */
extern time_t	chktim;		/* Time mail last checked */
extern int	shpgrp;		/* Pgrp of shell */
extern int	tpgrp;		/* Terminal process group, (-1 leave alone) */
extern int	opgrp;		/* Initial pgrp and tty pgrp */
extern int	oldisc;		/* Initial line discipline or -1 */
extern int	oswtch;		/* Initial swtch character or -1 */

/*
 * These are declared here because they want to be
 * initialized in sh.init.c (to allow them to be made readonly)
 */

struct command;
typedef void	(*bf_t)(wchar_t **, struct command *);

extern	struct	biltins {
	wchar_t	*bname;
	bf_t	bfunct;
	short	minargs, maxargs;
} bfunc[];

extern int nbfunc;

extern struct srch {
	wchar_t	*s_name;
	short	s_value;
} srchn[];

extern int nsrchn;

/*
 * To be able to redirect i/o for builtins easily, the shell moves the i/o
 * descriptors it uses away from 0,1,2.
 * Ideally these should be in units which are closed across exec's
 * (this saves work) but for version 6, this is not usually possible.
 * The desired initial values for these descriptors are defined in
 * sh.local.h.
 */
extern short	SHIN;		/* Current shell input (script) */
extern short	SHOUT;		/* Shell output */
extern short	SHDIAG;		/* Diagnostic output... shell errs go here */
extern short	OLDSTD;		/* Old standard input (def for cmds) */
extern short	FDIN;		/* /dev/fd descriptor */

#define BAD_FD	(-1)

/*
 * Error control
 *
 * Errors in scanning and parsing set up an error message to be printed
 * at the end and complete.  Other errors always cause a reset.
 * Because of source commands and .cshrc we need nested error catches.
 */
extern jmp_buf	reslab;

#define	setexit()	((void) setjmp(reslab))
#define	reset()		longjmp(reslab, 0)

#define	getexit(a)	copy((void *)(a), (void *)reslab, sizeof reslab)
#define	resexit(a)	copy((void *)reslab, ((void *)(a)), sizeof reslab)

extern wchar_t	*gointr;		/* Label for an onintr transfer */
extern void	(*parintr)(void);	/* Parents interrupt catch */
extern void	(*parterm)(void);	/* Parents terminate catch */

/*
 * Each level of input has a buffered input structure.
 * There are one or more blocks of buffered input for each level,
 * exactly one if the input is seekable and tell is available.
 * In other cases, the shell buffers enough blocks to keep all loops
 * in the buffer.
 */
struct	Bin {
	off_t	Bfseekp;		/* Seek pointer */
	off_t	Bfbobp;			/* Seekp of beginning of buffers */
	off_t	Bfeobp;			/* Seekp of end of buffers */
	short	Bfblocks;		/* Number of buffer blocks */
	wchar_t	**Bfbuf;		/* The array of buffer blocks */
};
extern struct Bin B;

#define	fseekp	B.Bfseekp
#define	fbobp	B.Bfbobp
#define	feobp	B.Bfeobp
#define	fblocks	B.Bfblocks
#define	fbuf	B.Bfbuf

#define btell()	fseekp

#ifndef btell
extern off_t	btell(void);
#endif

/*
 * The shell finds commands in loops by reseeking the input
 * For whiles, in particular, it reseeks to the beginning of the
 * line the while was on; hence the while placement restrictions.
 */
extern off_t	lineloc;

#ifdef	TELL
extern bool	cantell;		/* Is current source tellable ? */
void		settell(void);
#endif

/*
 * Input lines are parsed into doubly linked circular
 * lists of words of the following form.
 */
struct	wordent {
	wchar_t	*word;
	struct	wordent *prev;
	struct	wordent *next;
};

/*
 * During word building, both in the initial lexical phase and
 * when expanding $ variable substitutions, expansion by `!' and `$'
 * must be inhibited when reading ahead in routines which are themselves
 * processing `!' and `$' expansion or after characters such as `\' or in
 * quotations.  The following flags are passed to the getC routines
 * telling them which of these substitutions are appropriate for the
 * next character to be returned.
 */
#define	DODOL	1
#define	DOEXCL	2
#define	DOALL	(DODOL|DOEXCL)

/*
 * Labuf implements a general buffer for lookahead during lexical operations.
 * Text which is to be placed in the input stream can be stuck here.
 * We stick parsed ahead $ constructs during initial input,
 * process id's from `$$', and modified variable values (from qualifiers
 * during expansion in sh.dol.c) here.
 */
extern wchar_t	labuf[];
extern wchar_t	*lap;

/*
 * Parser structure
 *
 * Each command is parsed to a tree of command structures and
 * flags are set bottom up during this process, to be propagated down
 * as needed during the semantics/exeuction pass (sh.sem.c).
 */
struct	command {
	short	t_dtyp;				/* Type of node */
	short	t_dflg;				/* Flags, e.g. FAND|... */
	union {
		wchar_t	*T_dlef;		/* Input redirect word */
		struct	command *T_dcar;	/* Left part of list/pipe */
	} L;
	union {
		wchar_t	*T_drit;		/* Output redirect word */
		struct	command *T_dcdr;	/* Right part of list/pipe */
	} R;
	wchar_t	**t_dcom;			/* Command/argument vector */
	struct	command *t_dspr;		/* Pointer to ()'d subtree */
	short	t_nice;
};

#define	t_dlef	L.T_dlef
#define	t_dcar	L.T_dcar
#define	t_drit	R.T_drit
#define	t_dcdr	R.T_dcdr

#define	TCOM	1		/* t_dcom <t_dlef >t_drit	*/
#define	TPAR	2		/* ( t_dspr ) <t_dlef >t_drit	*/
#define	TFIL	3		/* t_dlef | t_drit		*/
#define	TLST	4		/* t_dlef ; t_drit		*/
#define	TOR	5		/* t_dlef || t_drit		*/
#define	TAND	6		/* t_dlef && t_drit		*/

#define	FSAVE	(FNICE|FTIME|FNOHUP)	/* save these when re-doing */

#define	FAND	(1<<0)		/* executes in background	*/
#define	FCAT	(1<<1)		/* output is redirected >>	*/
#define	FPIN	(1<<2)		/* input is a pipe		*/
#define	FPOU	(1<<3)		/* output is a pipe		*/
#define	FPAR	(1<<4)		/* don't fork, last ()ized cmd	*/
#define	FINT	(1<<5)		/* should be immune from intr's */

#define	FDIAG	(1<<7)		/* redirect unit 2 with unit 1	*/
#define	FANY	(1<<8)		/* output was !			*/
#define	FHERE	(1<<9)		/* input redirection is <<	*/
#define	FREDO	(1<<10)		/* reexec aft if, repeat,...	*/
#define	FNICE	(1<<11)		/* t_nice is meaningful */
#define	FNOHUP	(1<<12)		/* nohup this command */
#define	FTIME	(1<<13)		/* time this command */

/*
 * The keywords for the parser
 */
#define	ZBREAK		0
#define	ZBRKSW		1
#define	ZCASE		2
#define	ZDEFAULT 	3
#define	ZELSE		4
#define	ZEND		5
#define	ZENDIF		6
#define	ZENDSW		7
#define	ZEXIT		8
#define	ZFOREACH	9
#define	ZGOTO		10
#define	ZIF		11
#define	ZLABEL		12
#define	ZLET		13
#define	ZSET		14
#define	ZSWITCH		15
#define	ZTEST		16
#define	ZTHEN		17
#define	ZWHILE		18

/*
 * Structure defining the existing while/foreach loops at this
 * source level.  Loops are implemented by seeking back in the
 * input.  For foreach (fe), the word list is attached here.
 */
struct	whyle {
	off_t	w_start;		/* Point to restart loop */
	off_t	w_end;			/* End of loop (0 if unknown) */
	wchar_t	**w_fe, **w_fe0;	/* Current/initial wordlist for fe */
	wchar_t	*w_fename;		/* Name for fe */
	struct	whyle *w_next;		/* Next (more outer) loop */
};

extern struct whyle *whyles;

/*
 * Variable structure
 *
 * Aliases and variables are stored in AVL balanced binary trees.
 */
struct	varent {
	wchar_t	**vec;			/* Array of words which is the value */
	wchar_t	*v_name;		/* Name of variable/alias */
	struct	varent *v_link[3];	/* The links, see below */
	int	v_bal;			/* Balance factor */
};

extern struct varent  shvhed, aliases;

#define v_left		v_link[0]
#define v_right		v_link[1]
#define v_parent	v_link[2]

#define adrof(v)	adrof1((v), &shvhed)
#define value(v)	value1((v), &shvhed)

/*
 * The following are for interfacing redo substitution in
 * aliases to the lexical routines.
 */
extern struct	wordent *alhistp;	/* Argument list (first) */
extern struct	wordent *alhistt;	/* Node after last in arg list */
extern wchar_t	**alvec;		/* The (remnants of) alias vector */

/*
 * Filename/command name expansion variables
 */
extern short	gflag;			/* After tglob -> is globbing needed? */

extern int ncargs;	/* used to be NCARGS */
extern int gavsiz;	/* used to be GAVSIZ */

/*
 * Variables for filename expansion
 */
extern wchar_t	**gargv;		/* Pointer to the (stack) arglist */
extern long	gargc;			/* Number args in gargv */
extern long	gnleft;

/*
 * Variables for command expansion.
 */
extern wchar_t	**pargv;		/* Pointer to the argv list space */
extern wchar_t	*pargs;			/* Pointer to start current word */
extern long	pargc;			/* Count of arguments in pargv */
extern long	pnleft;			/* Number of chars left in pargs */
extern wchar_t	*pargcp;		/* Current index into pargs */

/*
 * History list
 *
 * Each history list entry contains an embedded wordlist
 * from the scanner, a number for the event, and a reference count
 * to aid in discarding old entries.
 *
 * Essentially "invisible" entries are put on the history list
 * when history substitution includes modifiers, and thrown away
 * at the next discarding since their event numbers are very negative.
 */
struct	Hist {
	struct	wordent Hlex;
	int	Hnum;
	int	Href;
	struct	Hist *Hnext;
};

extern struct Hist Histlist;

extern struct	wordent	paraml;		/* Current lexical word list */
extern int	eventno;		/* Next events number */
extern int	lastev;			/* Last event reference (default) */

extern wchar_t	HIST;			/* history invocation character */
extern wchar_t	HISTSUB;		/* auto-substitute character */

/*
 * In lines for frequently called functions
 */
#define XFREE(cp) { \
	extern char end[]; \
	char stack; \
	if (((char *)(cp)) >= end && ((char *)(cp)) < &stack) \
		free((void *)(cp)); \
}

extern int		access_(wchar_t *, int);
extern void		addla(wchar_t *);
extern struct varent	*adrof1(wchar_t *, struct varent *);
extern void		alias(struct wordent *);
extern int		alnum(wchar_t);
extern void		ambiguous(void);
extern int		any(int, wchar_t *);
extern double		atof_(wchar_t *);
extern int		atoi_(wchar_t *);
extern void		bferr(char *);
extern wchar_t		**blkcpy(wchar_t **, wchar_t **);
extern void		blkfree(wchar_t **);
extern int		blklen(wchar_t **);
extern void		blkpr(wchar_t **);
extern wchar_t		**blkspl(wchar_t **, wchar_t **);
extern char		**blkspl_(char **, char **);
extern void		bseek(off_t);
extern void		btoeof(void);
extern int		chdir_(wchar_t *);
extern void		closem(void);
extern int		cmlook(wchar_t, int);
extern int		cmpmbwc(wchar_t *, char *, char **);
extern wchar_t		*cname();
extern wchar_t		**copyblk(wchar_t **);
extern void		copylex(struct wordent *, struct wordent *);
extern int		creat_(wchar_t *, int);
extern int		dcopy(int, int);
extern void		Dfix(struct command *);
extern wchar_t		*Dfix1(wchar_t *);
extern int		dmove(int, int);
extern void		doalias(wchar_t **);
extern wchar_t		**dobackp(wchar_t *, bool);
extern void		dobreak(void);
extern void		docontin(void);
extern void		doecho(wchar_t **);
extern void		doelse(void);
extern void		doend(void);
extern void		doeval(wchar_t **);
extern void		doexec(struct command *);
extern void		doexit(wchar_t **);
extern void		doforeach(wchar_t **);
extern void		doglob(wchar_t **);
extern void		dogoto(wchar_t **);
extern void		dohash(void);
extern void		dohist(wchar_t **);
extern void		doif(wchar_t **, struct command *);
extern void		dolet(wchar_t **);
extern void		dolimit(wchar_t **);
extern void		dologin(wchar_t **);
extern void		dologout(void);
extern wchar_t		*domod(wchar_t *, wchar_t);
extern void		done(int);
extern void		donefds(void);
#ifdef NEWGRP
extern void		donewgrp(wchar_t **);
#endif
extern void		donice(wchar_t **);
extern void		donohup(void);
extern void		doonintr(wchar_t **);
extern void		dorepeat(wchar_t **, struct command *);
extern void		doset(wchar_t **);
extern void		dosetenv(wchar_t **);
extern void		dosource(wchar_t **);
extern void		dosuspend(void);
extern void		doswbrk(void);
extern void		doswitch(wchar_t **);
extern void		dotime(void);
extern void		doumask(wchar_t **);
extern void		dounhash(void);
extern void		dounlimit(wchar_t **);
extern void		dounsetenv(wchar_t **);
extern void		dowhile(wchar_t **);
extern void		dozip(void);
extern void		draino(void);
extern struct Hist	*enthist(int, struct wordent *, bool);
extern void		err_arg2long(void);
extern void		err_divby0(void);
extern void		err_experr(void);
extern void		err_fntruncated(char *);
extern void		err_line2long(void);
extern void		err_missing(wchar_t);
extern void		err_modby0(void);
extern void		err_nomatch(void);
extern void		err_nomem(void);
extern void		err_notfromtty(void);
extern void		err_notinwf(void);
extern void		err_notlogin(void);
extern void		err_toomany(wchar_t);
extern void		err_unknflag(wchar_t);
extern void		err_unmatched(wchar_t);
extern void		err_usage(char *);
extern void		err_word2long(void);
extern void		error(char *, ...);
extern void		error_errstr(void);
extern void		execash(wchar_t **, struct command *);
extern void		execute(struct command *, int, ...);
extern void		exitstat(void);
extern int		exp(wchar_t ***);
extern int		exp0(wchar_t ***, bool);
extern void		flush(void);
extern void		freelex(struct wordent *);
extern void		freesyn(struct command *);
extern void		func(struct command *, struct biltins *);
extern void		Gcat(wchar_t *, wchar_t *);
extern wchar_t		*getenv_(wchar_t *);
extern wchar_t		*getenvs_(char *);
extern int		gethdir(wchar_t *);
extern int		gethostname_(wchar_t *, int);
extern int		getldisc(int, int *, int *);
extern int		getn(wchar_t *);
extern wchar_t		*getwd_(wchar_t *);
extern void		ginit(wchar_t **);
extern wchar_t		**glob(wchar_t **);
extern wchar_t		*globone(wchar_t *);
extern int		Gmatch(wchar_t *, wchar_t *);
extern void		goodbye(void);
extern void		heredoc(wchar_t *);
extern void		illenvvar(char *);
extern void		illmbchar(char *);
extern void		importpath(wchar_t *);
extern void		initdesc(void);
extern struct biltins	*isbfunc(struct command *);
extern wchar_t		lastchr(wchar_t *);
extern int		letter(wchar_t);
extern int		lex(struct wordent *);
extern void		lshift(wchar_t **, int);
extern int		lstat_(wchar_t *, struct stat *);
extern size_t		mbstowcs(wchar_t *, const char *, size_t);
extern void		mypipe(int *);
extern int		number(wchar_t *);
extern int		onlyread(wchar_t *);
extern int		open_(wchar_t *, int);
extern DIR		*opendir_(wchar_t *);
extern void		Perror(wchar_t *);
extern void		pintr(void);
extern void		pintr1(bool);
extern void		plist(struct varent *);
extern int		prefix(wchar_t *, wchar_t *);
extern void		printprompt(void);
extern void		prlex(struct wordent *);
extern void		process(bool);
extern void		prusage(struct rusage *, struct rusage *,
				struct timeval *, struct timeval *);
extern void		prvars(void);
extern void		psecs(int64_t);
extern void		putbyte(int);
extern wchar_t		*putn(int);
extern int		read_(int, wchar_t *, int);
extern wchar_t		readc(bool);
extern int		readlink_(wchar_t *, wchar_t *, int);
extern void		rechist(void);
extern void		rscan(wchar_t **, void (*)(wchar_t));
extern void		ruadd(struct rusage *, struct rusage *);
extern void		*salloc(unsigned int, unsigned int);
extern wchar_t		**saveblk(wchar_t **);
extern void		savehist(struct wordent *);
extern wchar_t		*savestr(wchar_t *);
extern void		search(int, int, wchar_t *);
extern void		set(wchar_t *, wchar_t *);
extern void		set1(wchar_t *, wchar_t **, struct varent *);
extern void		setenv(wchar_t *, wchar_t *);
extern void		seterr(char *);
extern void		seterr2(wchar_t *, char *);
extern void		seterrc(char *, wchar_t);
extern int		setldisc(int, int, int);
extern void		setNS(wchar_t *);
extern void		setq(wchar_t *, wchar_t **, struct varent *);
extern void		settimes(void);
extern void		shift(wchar_t **);
extern void		showstr(int, char *, void *);
extern void		shprintf(char *, ...);
extern int		srchx(wchar_t *);
extern int		stat_(wchar_t *, struct stat *);
extern wchar_t		*strend(wchar_t *);
extern wchar_t		*strip(wchar_t *);
extern wchar_t		*strspl(wchar_t *, wchar_t *);
extern wchar_t		*strtots(wchar_t *, char *, int *);
extern struct command	*syntax(struct wordent *, struct wordent *, int);
extern void		syntaxerr(void);
extern char		*tctomb(char *, wchar_t);
extern int		tenex(wchar_t *, int);
extern void		tglob(wchar_t **);
extern void		trim(wchar_t **);
extern char		*tstomb();
extern char		*tstostr(char *, wchar_t *, int *);
extern int		tswidth(wchar_t *);
extern void		tvsub(struct timeval *, struct timeval *,
			      struct timeval *);
extern void		udvar(wchar_t *);
extern void		unalias(wchar_t **);
extern int		unlink_(wchar_t *);
extern void		unreadc(wchar_t);
extern void		unset(wchar_t **);
extern void		unset1(wchar_t *[], struct varent *);
extern void		unsetv(wchar_t *);
extern void		untty(void);
extern wchar_t		*value1(wchar_t *, struct varent *);
extern wchar_t		*wcalloc(unsigned int);
extern int		wcsetno(wchar_t, wchar_t);
extern size_t		wcstombs(char *, const wchar_t *, size_t);
extern void		wfree(void);
extern void		wputchar(wchar_t);
extern int		write_(int, wchar_t *, int);
extern void		write_string(char *);
extern wchar_t		*wscpyend(wchar_t *, wchar_t *);
extern void		*xalloc(unsigned int);
extern void		xechoit(wchar_t **);
extern void		xfree(void *);
extern wchar_t		*xhome();
extern wchar_t		*xname();

#define	NOSTR	0
#define	NOFLAG	((int *)0)

/*
 * setname is a macro to save space (see sh.err.c)
 */
extern wchar_t	*bname;

#define	setname(a)	(bname = (a))

extern wchar_t	**evalvec;
extern wchar_t	*evalp;

struct	mesg {
	wchar_t	*iname;		/* signal name from /usr/include */
	char	*cname;		/* catalog:nbr */
	char	*pname;		/* print name */
};

extern struct mesg mesg[NSIG+1];
