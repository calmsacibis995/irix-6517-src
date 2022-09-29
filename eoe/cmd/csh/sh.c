/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.17 $"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
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
 * frank@ceres.esd.sgi.com
 *	Internationalization done Nov 6 1992
 */

#include <stdio.h>
#include <unistd.h>
#include <locale.h>
#include <fcntl.h>
#include <pfmt.h>
#include <pwd.h>
#include "sh.h"
#include "sh.dir.h"
#include "sh.proc.h"
#include "sh.wconst.h"

wchar_t *pathlist[] = { S_DOT, S_usrsbin, S_usrbsd, S_bin, S_usrbin, 0 };

wchar_t *dumphist[] = { S_history, S_h, 0, 0 };
wchar_t *loadhist[] = { S_source, S_h, S_NDOThistory, 0 };

int	nofile;
bool	reenter;
bool	nverbose;
bool	nexececho;
bool	quitit;
bool	fast;
bool	batch;
bool	prompt = 1;
bool	enterhist = 0;

/*
 * Global flags
 */
bool	chkstop;		/* Warned of stopped jobs... allow exit */
bool	didfds;			/* Have setup i/o fd's for child */
bool	doneinp;		/* EOF indicator after reset from readc */
bool	exiterr;		/* Exit if error or non-zero exit status */
bool	child;			/* Child shell ... errors cause exit */
bool	haderr;			/* Reset was because of an error */
bool	intty;			/* Input is a tty */
bool	intact;			/* We are interactive... therefore prompt */
bool	justpr;			/* Just print because of :p hist mod */
bool	loginsh;		/* We are a loginsh -> .login/.logout */
bool	neednote;		/* Need to pnotify() */
bool	noexec;			/* Don't execute, just syntax check */
bool	pjobs;			/* want to print jobs if interrupted */
bool	setintr;		/* Set interrupts on/off -> Wait intr... */
bool	timflg;			/* Time the next waited for command */
bool	havhash;		/* path hashing is available */

bool	filec = 0;		/* doing filename expansion */

/*
 * Global i/o info
 */
wchar_t	*arginp;		/* Argument input for sh -c and internal `xx` */
int	onelflg;		/* 2 -> need line for -t, 1 -> exit on read */
wchar_t	*file;			/* Name of shell file for $0 */

char	*errstr;		/* Error message from scanner/parser */
wchar_t	*shtemp;		/* Temp name for << shell files in /tmp */
struct	timeval time0;		/* Time at which the shell started */
struct	rusage ru0;

/*
 * Miscellany
 */
wchar_t	*doldol;		/* Character pid for $$ */
int	uid;			/* Invokers uid */
time_t	chktim;			/* Time mail last checked */
int	shpgrp;			/* Pgrp of shell */
int	tpgrp;			/* Terminal process group, -1 leave tty alone */
int	opgrp;			/* Initial pgrp and tty pgrp */
int	oldisc;			/* Initial line discipline or -1 */
int	oswtch;			/* Initial swtch character or -1 */

/*
 * To be able to redirect i/o for builtins easily, the shell moves the i/o
 * descriptors it uses away from 0,1,2.
 * Ideally these should be in units which are closed across exec's
 * (this saves work) but for version 6, this is not usually possible.
 * The desired initial values for these descriptors are defined in
 * sh.local.h.
 */
short	SHIN;			/* Current shell input (script) */
short	SHOUT;			/* Shell output */
short	SHDIAG;			/* Diagnostic output... shell errs go here */
short	OLDSTD;			/* Old standard input (def for cmds) */
short	FDIN = BAD_FD;		/* /dev/fd descriptor */

/*
 * Error control
 *
 * Errors in scanning and parsing set up an error message to be printed
 * at the end and complete.  Other errors always cause a reset.
 * Because of source commands and .cshrc we need nested error catches.
 */
jmp_buf	reslab;

wchar_t	*gointr;		/* Label for an onintr transfer */
void	(*parintr)(void);	/* Parents interrupt catch */
void	(*parterm)(void);	/* Parents terminate catch */

#ifndef btell
off_t	btell();
#endif

/*
 * The shell finds commands in loops by reseeking the input
 * For whiles, in particular, it reseeks to the beginning of the
 * line the while was on; hence the while placement restrictions.
 */
off_t	lineloc;

#ifdef	TELL
bool	cantell;			/* Is current source tellable ? */
#endif

/*
 * Labuf implements a general buffer for lookahead during lexical operations.
 * Text which is to be placed in the input stream can be stuck here.
 * We stick parsed ahead $ constructs during initial input,
 * process id's from `$$', and modified variable values (from qualifiers
 * during expansion in sh.dol.c) here.
 */
wchar_t	labuf[CSHBUFSIZ + 16];

wchar_t	*lap;

struct whyle *whyles;

struct varent  shvhed, aliases;

/*
 * The following are for interfacing redo substitution in
 * aliases to the lexical routines.
 */
struct	wordent *alhistp;		/* Argument list (first) */
struct	wordent *alhistt;		/* Node after last in arg list */
wchar_t	**alvec;			/* The (remnants of) alias vector */

/*
 * Filename/command name expansion variables
 */
short	gflag;				/* After tglob -> is globbing needed? */


int ncargs;	/* used to be NCARGS */
int gavsiz;	/* used to be GAVSIZ */

/*
 * Variables for filename expansion
 */
wchar_t	**gargv;			/* Pointer to the (stack) arglist */
long	gargc;				/* Number args in gargv */
long	gnleft;

/*
 * Variables for command expansion.
 */
wchar_t	**pargv;			/* Pointer to the argv list space */
wchar_t	*pargs;				/* Pointer to start current word */
wchar_t	*pargcp;			/* Current index into pargs */
long	pargc;				/* Count of arguments in pargv */
long	pnleft;				/* Number of chars left in pargs */

struct Hist Histlist;

struct	wordent	paraml;			/* Current lexical word list */
int	eventno;			/* Next events number */
int	lastev;				/* Last event reference (default) */

wchar_t HIST = '!';			/* history invocation character */
wchar_t HISTSUB = '^';			/* auto-substitute character */


wchar_t	*bname;

wchar_t	**evalvec;
wchar_t	*evalp;

char	cmd_label[] = "UX:csh";		/* label for fmtmsg() */

struct Bin B;				/* buffered input */

unsigned int	Z; 	/* A place to save macro arg to avoid side-effect!*/

static void	mailchk(void);
static void	phup(void);
static void	srccat(wchar_t *, wchar_t *);
static void	srcunit(int, bool, bool);
static wchar_t	**strblktotsblk(char **, int);

/*
 * Save and convert char * block.
 */
static wchar_t **
strblktotsblk(char **v, int num)
{
	register wchar_t **nv;
	register wchar_t **old;
	int cflag;

	old = nv = (wchar_t **)salloc(num + 1, sizeof(wchar_t **));
	while(*v && num--) {
	    *nv++ = strtots(NOSTR, *v, &cflag);
	    if(cflag) {
		showstr(MM_ERROR,
		    gettxt(_SGI_DMMX_csh_invarg, "'%s' - Invalid argument"),
		    *v);
		return(0);
	    }
	    v++;
	}
	*nv = 0;
	return(old);
}

/*
 * record history
 */
void
rechist(void)
{
	register int fp, ftmp, oldidfds;
	register wchar_t *svh;
	struct stat statb;
	wchar_t buf[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- rechist()\n");
#endif
	if( !fast) {
	    svh = value(S_savehist);
	    if( !*svh)
		return;				/* no history recording */
	    wscpy(buf, value(S_home));
	    /*
	     * Check that the current effective uid is the owner
	     * of the home directory so that .history is not written
	     * by su'ed shells.
	     */
	    if(stat_(buf, &statb) < 0)
		return;
	    if(statb.st_uid != geteuid())
		return;
	    wscat(buf, S_SLADOThistory);	/* /.history */
	    fp = creat_(buf, 0666);
	    if(fp == -1)
		return;
	    oldidfds = didfds;
	    didfds = 0;
	    ftmp = SHOUT;
	    SHOUT = fp;
	    wscpy(buf, svh);
	    dumphist[2] = buf;
	    dohist(dumphist);			/* print history to file */
	    (void)close(fp);
	    SHOUT = ftmp;
	    didfds = oldidfds;
	}
}

/*
 * main entry
 */
main(int c, char **av)
{
	register wchar_t **v, *cp;
	register int f;
	struct sigvec osv;
	wchar_t s_prompt[MAXHOSTNAMELEN + 128];

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicsh");
	(void)setlabel(cmd_label);

	/* argsize determined dynamically now;  was GAVSIZ */
	ncargs = sysconf(_SC_ARG_MAX);
	if(ncargs <= 1)
		ncargs = 10240;
	gavsiz = ncargs/6;	/* number of args 1/6 of max arglist is reasonable */

	v = strblktotsblk(av, c);		/* copy and convert args */
	if( !v)
	    return(-1);				/* args conversion error */

	/*
	 * Initialize paraml list
	 */
	paraml.next = paraml.prev = &paraml;
	havhash = 0;

	settimes();				/* Immed. estab. timing base */
	 
	if(eq(v[0], S_aout))			/* A.out's are quittable */
	    quitit = 1;
	uid = getuid();
	loginsh = ((**v == '-') && (c == 1));
	if(loginsh)
	    (void)time(&chktim);
	else {
	    /*
	     * Make a scan of the argument list to see if /dev/fd
	     * is in the path.  If so, we need to make sure that
	     * we don't close the fd during our initialization of
	     * the descriptor table.
	     */
	    int ac;
	    wchar_t **aav, *acp;
	    for (ac = c - 1, aav = v + 1; (ac > 0) && aav[0][0] == '-';
	    					aav++, ac--) {
		/* iterate through argument list */
		acp = &aav[0][1];
		while (*acp) {
			switch (*acp) {
			case 'c':
			case 'i':
			case 's':
			case 't':
				ac = 0;	/* done */
				break;
			}
			acp++;
		}
	    }
	    if (ac > 0) {
	    	if (!wsncmp(aav[0], S_devfd, 8 /*XXX*/)) {
			FDIN = atoi_(&aav[0][8]);
		}
	    }
	}

	/*
	 * Move the descriptors to safe places.
	 * The variable didfds is 0 while we have only FSH* to work with.
	 * When didfds is true, we have 0,1,2 and prefer to use these.
	 */
	initdesc();

	/*
	 * Initialize the shell variables.
	 * ARGV and PROMPT are initialized later.
	 * STATUS is also munged in several places.
	 * CHILD is munged when forking/waiting
	 */
	set(S_status, S_0);
	cp = getenvs_("HOME");
	dinit(cp);
	if( !cp)
	    fast++;			/* No home -> can't read scripts */
	else
	    set(S_home, cp);

	/*
	 * Grab other useful things from the environment.
	 * Should we grab everything??
	 */
	if(cp = getenvs_("USER"))
	    set(S_user, cp);
	if(cp = getenvs_("TERM"))
	    set(S_term, cp);

	/*
	 * Re-initialize path if set in environment
	 */
	if( !(cp = getenvs_("PATH")))
	    set1(S_path, saveblk(pathlist), &shvhed);
	else
	    importpath(cp);
	set(S_shell, S_SHELLPATH);

	doldol = putn(getpid());		/* For $$ */
	shtemp = strspl(S_tmpshell, doldol);	/* For << */

	/*
	 * Record the interrupt states from the parent process.
	 * If the parent is non-interruptible our hand must be forced
	 * or we (and our children) won't be either.
	 * Our children inherit termination from our parent.
	 * We catch it only if we are the login shell.
	 */

	/*
	 * parents interruptibility
	 */
	(void)sigvec(SIGINT, (struct sigvec *)0, &osv);
	parintr = osv.sv_handler;

	/*
	 * parents terminability
	 */
	(void)sigvec(SIGTERM, (struct sigvec *)0, &osv);
	parterm = osv.sv_handler;

	if(loginsh) {
	    (void)signal(SIGHUP, phup);		/* exit processing on HUP */
	    (void)signal(SIGXCPU, phup);	/* ...and on XCPU */
	    (void)signal(SIGXFSZ, phup);	/* ...and on XFSZ */
	}

	/*
	 * Process the arguments.
	 *
	 * Note that processing of -v/-x is actually delayed till after
	 * script processing.
	 */
	c--, v++;
	while (c > 0 && (cp = v[0])[0] == '-' && *++cp != '\0' && !batch) {
		do switch (*cp++) {

		case 'b':		/* -b	Next arg is input file */
			batch++;
			break;

		case 'c':		/* -c	Command input from arg */
			if (c == 1)
				done(0);
			c--, v++;
			arginp = v[0];
			prompt = 0;
			nofile++;
			break;

		case 'e':		/* -e	Exit on any error */
			exiterr++;
			break;

		case 'f':		/* -f	Fast start */
			fast++;
			break;

		case 'i':		/* -i	Interactive, even if !intty */
			intact++;
			nofile++;
			break;

		case 'n':		/* -n	Don't execute */
			noexec++;
			break;

		case 'q':		/* -q	(Undoc'd) ... die on quit */
			quitit = 1;
			break;

		case 's':		/* -s	Read from std input */
			nofile++;
			break;

		case 't':		/* -t	Read one line from input */
			onelflg = 2;
			prompt = 0;
			nofile++;
			break;
#ifdef TRACE
		case 'T':		/* -T 	trace switch on */
			trace_init();
			break;
#endif

		case 'v':		/* -v	Echo hist expanded input */
			nverbose = 1;			/* ... later */
			break;

		case 'x':		/* -x	Echo just before execution */
			nexececho = 1;			/* ... later */
			break;

		case 'V':		/* -V	Echo hist expanded input */
			setNS(S_verbose/*"verbose"*/);		/* NOW! */
			break;

		case 'X':		/* -X	Echo just before execution */
			setNS(S_echo);			/* NOW! */
			break;

		} while (*cp);
		v++, c--;
	}

	if(quitit)
	    (void)signal(SIGQUIT, SIG_DFL);

	/*
	 * Unless prevented by -c, -i, -s, or -t, if there
	 * are remaining arguments the first of them is the name
	 * of a shell file from which to read commands.
	 */
	if( !nofile && (c > 0)) {
	    nofile = open_(v[0], O_RDONLY);
	    if(nofile < 0) {
		child++;
		Perror(v[0]);			/* no return */
	    }
	    file = v[0];
	    SHIN = dmove(nofile, FSHIN);	/* Replace FSHIN */
	    (void) fcntl(SHIN, F_SETFD, 1);
	    prompt = 0;
	    c--, v++;
	}
	if( !batch && (uid != geteuid() || (getgid() != getegid()))) {
	    errno = EACCES;
	    child++;
	    Perror(S_csh);			/* no return */
	}

	/*
	 * Consider input a tty if it really is or we are interactive.
	 */
	intty = intact || isatty(SHIN);

	/*
	 * Decide whether we should play with signals or not.
	 * If we are explicitly told (via -i, or -) or we are a login
	 * shell (arg0 starts with -) or the input and output are both
	 * the ttys("csh", or "csh</dev/ttyx>/dev/ttyx")
	 * Note that in only the login shell is it likely that parent
	 * may have set signals to be ignored
	 */
	if(loginsh || intact || intty && isatty(SHOUT))
	    setintr = 1;
#ifdef TELL
	settell();
#endif
	/*
	 * Save the remaining arguments in argv.
	 */
	setq(S_argv, saveblk(v), &shvhed);

	/*
	 * Set up the prompt.
	 */
	if(prompt) {
	    gethostname_(s_prompt, MAXHOSTNAMELEN);
	    wscat(s_prompt, uid? S_PERSENTSP : S_SHARPSP);
	    set(S_prompt, s_prompt);
	}

	/*
	 * If we are an interactive shell, then start fiddling
	 * with the signals; this is a tricky game.
	 */
	shpgrp = getpgid(0);
	opgrp = tpgrp = -1;
	oldisc = -1;
	if(setintr) {
		**av = '-';
		if( !quitit)		/* Wary! */
			(void) signal(SIGQUIT, SIG_IGN);
		(void) signal(SIGINT, pintr);
		(void) sigblock(sigmask(SIGINT));
		(void) signal(SIGTERM, SIG_IGN);
		if( !quitit && !arginp) {
			(void) sigset(SIGTSTP, SIG_IGN);
			(void) sigset(SIGTTIN, SIG_IGN);
			(void) sigset(SIGTTOU, SIG_IGN);
			/*
			 * Wait till in foreground, in case someone
			 * stupidly runs
			 *	csh &
			 * dont want to try to grab away the tty.
			 */
			if(isatty(FSHDIAG))
				f = FSHDIAG;
			else if(isatty(FSHOUT))
				f = FSHOUT;
			else if(isatty(OLDSTD))
				f = OLDSTD;
			else
				f = -1;
retry:
			if ((tpgrp = tcgetpgrp(f)) != -1) {
				int ldisc;

				if (tpgrp != shpgrp) {
				    void (*old)() = signal(SIGTTIN, SIG_DFL);
				    (void)kill(0, SIGTTIN);
				    (void)signal(SIGTTIN, old);
				    goto retry;
				}

				/*
				 * Get current line discipline and VSUSP
				 * character.  If the line discipline is
				 * not LDISC1, set it to LDISC1.  If the
				 * VSUSP character is not defined, set
				 * it to ^Z.  The old values will be restored
				 * when the shell exits.
				 */
				if(getldisc(f, &oldisc, &oswtch) < 0) {
				    shprintf("Warning: no access to tty, thus no job control in this shell");
				    goto notty;
				}
				if(oldisc != NTTYDISC || !oswtch) {
				    int swtch;

				    if(oldisc == NTTYDISC)
					ldisc = oldisc = -1;
				    else
					ldisc = NTTYDISC;
				    if( !oswtch)
					swtch = CSWTCH;
				    else
					swtch = oswtch = -1;
				    (void)setldisc(f, ldisc, swtch);
				}
				opgrp = shpgrp;
				shpgrp = getpid();
				tpgrp = shpgrp;

				/* Setpgid will fail if we are a session
				 * leasder, mypid == mypgrp (POSIX 4.3.3)
				 */
				if (opgrp != shpgrp)
					if (setpgid(0, shpgrp) == -1)
						goto notty;

				if (tcsetpgrp(f, shpgrp) == -1)
					goto notty;
				(void)fcntl(dcopy(f, FSHTTY), F_SETFD, 1);
			} else {
notty:
				tpgrp = -1;
			}
		}
	}
	if( !setintr && (parintr == SIG_DFL))
	    setintr++;
	(void)signal(SIGCHLD, pchild);		/* while signals not ready */

	/*
	 * Set an exit here in case of an interrupt or error reading
	 * the shell start-up scripts.
	 */
	setexit();
	haderr = 0;			/* In case second time through */
	if( !fast && !reenter) {
	    reenter++;

	    /*
	     * If this is a login csh, and /etc/cshrc exists,
	     * source /etc/cshrc first.
	     */
	    if(loginsh) {
		srccat(S_etc, S_cshrc);		/* sgi compatible */
		srccat(S_etc, S_SLADOTlogin);	/* SVR4 compatible */
		srccat(S_etc, S_DOTCSHRC);	/* BSD4.4 compatible */
            }
	    /*
	     * Will have value("home") here because set fast if don't
	     */
	    srccat(value(S_home), S_SLADOTcshrc);
	    if( !fast && !arginp && !onelflg && !havhash)
		dohash();

	    /*
	     * Reconstruct the history list now, so that it's
	     * available from within .login.
	     */
	    dosource(loadhist);
	    if(loginsh) {
		srccat(value(S_home), S_SLADOTlogin);
	    }
	}

	/*
	 * Now are ready for the -v and -x flags
	 */
	if(nverbose)
	    setNS(S_verbose);		/* verbose */
	if(nexececho)
	    setNS(S_echo);		/* echo */

	/*
	 * All the rest of the world is inside this call.
	 * The argument to process indicates whether it should
	 * catch "error unwinds".  Thus if we are a interactive shell
	 * our call here will never return by being blown past on an error.
	 */
	process(setintr);

	/*
	 * Mop-up.
	 */
	if(loginsh) {
	    shprintf("logout\n");
	    (void)close(SHIN);
	    child++;
	    goodbye();
	}
	rechist();
	exitstat();
	/*NOTREACHED */
}

void
untty(void)
{
#ifdef TRACE
	tprintf("TRACE- untty()\n");
#endif
	if(tpgrp > 0) {
	    (void) setpgid(0, opgrp);
	    (void) tcsetpgrp(FSHTTY, opgrp);
	    (void)setldisc(FSHTTY, oldisc, oswtch);
	}
}

void
importpath(wchar_t *cp)
{
	register wchar_t *dp;
	register wchar_t **pv;
	register wchar_t c;
	register int i = 0;

#ifdef TRACE
	tprintf("TRACE- importpath()\n");
#endif
	/*
	 * count # of components
	 */
	for(dp = cp; *dp; dp++)
	    if(*dp == ':')
		i++;
	/*
	 * i+2 where i is the number of colons in the path.
	 * There are i+1 directories in the path plus we need
	 * room for a zero terminator.
	 */
	pv = (wchar_t **)salloc(i + 2, sizeof(wchar_t **));
	i = 0;
	if(*cp) {
	    for(dp = cp;; dp++) {
		c = *dp;
		if((c == ':') || !c) {
		    *dp = 0;
		    pv[i++] = savestr(*cp? cp : S_DOT);
		    if( !c)
			break;
		    cp = dp + 1;
		    *dp = ':';
		}
	    }
	}
	pv[i] = 0;
	set1(S_path, pv, &shvhed);
}

/*
 * Source to the file which is the catenation of the argument names.
 */
static void
srccat(wchar_t *cp, wchar_t *dp)
{
	register wchar_t *ep;
	register int unit;

#ifdef TRACE
	tprintf("TRACE- srccat()\n");
#endif
	ep = strspl(cp, dp);
	unit = dmove(open_(ep, O_RDONLY), -1);
	(void)fcntl(unit, F_SETFD, 1);
	xfree(ep);
	srcunit(unit, 0, 0);
}

/*
 * Source to a unit.  If onlyown it must be our file or our group or
 * we don't chance it.	This occurs on ".cshrc"s and the like.
 */
static void
srcunit(int unit, bool onlyown, bool hflg)
{
	volatile int oSHIN = -1;
	int oldintty = intty;
	int oonelflg = onelflg;
	struct whyle *oldwhyl = whyles;
	wchar_t *ogointr = gointr, *oarginp = arginp;
	wchar_t *oevalp = evalp, **oevalvec = evalvec;
	wchar_t OHIST = HIST;
	bool oenterhist = enterhist;
#ifdef TELL
	bool otell = cantell;
#endif
	struct Bin saveB;
	volatile int reenter;		/* must be volatile (setjmp/longjmp) */
	int omask;
	jmp_buf oldexit;

#ifdef TRACE
	tprintf("TRACE- srcunit()\n");
#endif
	if(unit < 0)
	    return;
	if(didfds)
	    donefds();
	if(onlyown) {
	    struct stat stb;

	    if((fstat(unit, &stb) < 0)
		|| (stb.st_uid != uid && stb.st_gid != getgid())) {
			(void) close(unit);
			return;
	    }
	}

	/*
	 * There is a critical section here while we are pushing down the
	 * input stream since we have stuff in different structures.
	 * If we weren't careful an interrupt could corrupt SHIN's Bin
	 * structure and kill the shell.
	 *
	 * We could avoid the critical region by grouping all the stuff
	 * in a single structure and pointing at it to move it all at
	 * once.  This is less efficient globally on many variable references
	 * however.
	 */
	getexit(oldexit);
	reenter = 0;
	if(setintr)
	    omask = sigblock(sigmask(SIGINT));
	setexit();
	reenter++;
	if(reenter == 1) {
	    /*
	     * Setup the new values of the state stuff saved above
	     */
	    bcopy((void *)&B, (void *)&saveB, sizeof(struct Bin));
	    fbuf = (wchar_t **)0;
	    fseekp = feobp = fblocks = 0;
	    oSHIN = SHIN, SHIN = unit, arginp = 0, onelflg = 0;
	    intty = isatty(SHIN), whyles = 0, gointr = 0;
	    evalvec = 0; evalp = 0;
	    enterhist = hflg;
	    if(enterhist)
		HIST = '\0';

	    /*
	     * Now if we are allowing commands to be interrupted,
	     * we let ourselves be interrupted.
	     */
	    if(setintr)
		(void)sigsetmask(omask);
#ifdef TELL
	    settell();
#endif
	    process(0);			/* 0 -> blow away on errors */
	}
	if(setintr)
	    (void) sigsetmask(omask);

	if(oSHIN >= 0) {
	    register int i;

	    /*
	     * We made it to the new state... free up its storage
	     * This code could get run twice but xfree doesn't care
	     */
	    for(i = 0; i < fblocks; i++)
		xfree(fbuf[i]);
	    xfree(fbuf);

	    /*
	     * Reset input arena
	     */
	    bcopy((void *)&saveB, (void *)&B, sizeof(struct Bin));
	    (void)close(SHIN), SHIN = oSHIN;
	    arginp = oarginp, onelflg = oonelflg;
	    evalp = oevalp, evalvec = oevalvec;
	    intty = oldintty, whyles = oldwhyl, gointr = ogointr;
	    if(enterhist)
		HIST = OHIST;
	    enterhist = oenterhist;
#ifdef TELL
	    cantell = otell;
#endif
	}
	resexit(oldexit);

	/*
	 * If process reset() (effectively an unwind) then
	 * we must also unwind.
	 */
	if(reenter >= 2)
	    error(NOSTR);
}

void
goodbye(void)
{
#ifdef TRACE
	tprintf("TRACE- goodbye()\n");
#endif
	if(loginsh) {
	    (void) signal(SIGQUIT, SIG_IGN);
	    (void) signal(SIGINT, SIG_IGN);
	    (void) signal(SIGTERM, SIG_IGN);
	    setintr = 0;			/* no intr after "logout" */
	    if(adrof(S_home))
		srccat(value(S_home), S_SLADOTlogout);
	}
	rechist();
	exitstat();
}

void
exitstat(void)
{
#ifdef PROF
	monitor(0);
#endif
	/*
	 * Note that if STATUS is corrupted (i.e. getn bombs)
	 * then error will exit directly because we poke child here.
	 * Otherwise we might continue unwarrantedly (sic).
	 */
#ifdef TRACE
	tprintf("TRACE- exitstat()\n");
#endif
	child++;
	done(getn(value(S_status)));
}

/*
 * in the event of a HUP we want to save the history
 */
static void
phup(void)
{
#ifdef TRACE
	tprintf("TRACE- phup()\n");
#endif
	rechist();

	/*
	 * We kill the last foreground process group. It then becomes
	 * responsible to propagate the SIGHUP to its progeny.
	 */

	killfgpgroup();
	done(1);
}

/*
 * Catch an interrupt, e.g. during lexical input.
 * If we are an interactive shell, we reset the interrupt catch
 * immediately.  In any case we drain the shell output,
 * and finally go through the normal error mechanism, which
 * gets a chance to make the shell go away.
 */
wchar_t *jobargv[2] = { S_jobs, 0 };

void
pintr1(bool wantnl)
{
	register wchar_t **v;
	register int omask;

#ifdef TRACE
	tprintf("TRACE- pintr1()\n");
#endif
	omask = sigblock(0);
	if(setintr) {
	    (void)sigsetmask(omask & ~sigmask(SIGINT));
	    if(pjobs) {
		pjobs = 0;
		shprintf("\n");
		dojobs(jobargv);
		bferr(gettxt(_SGI_DMMX_csh_Interrupted, "Interrupted"));
	    }
	}
	(void)sigsetmask(omask & ~sigmask(SIGCHLD));
	draino();

	/*
	 * If we have an active "onintr" then we search for the label.
	 * Note that if one does "onintr -" then we shan't be interruptible
	 * so we needn't worry about that here.
	 */
	if(gointr) {
	    search(ZGOTO, 0, gointr);
	    timflg = 0;
	    if(v = pargv)
		pargv = 0, blkfree(v);
	    if(v = gargv)
		gargv = 0, blkfree(v);
	    reset();
	} else 
	    if(intty && wantnl)
		shprintf("\n");	/* Some like this, others don't */
	error(NOSTR);
}

/*
 * Catch interrupt 1
 */
void
pintr(void)
{
#ifdef TRACE
	tprintf("TRACE- pintr()\n");
#endif
	pintr1(1);
}

/*
 * Process is the main driving routine for the shell.
 * It runs all command processing, except for those within { ... }
 * in expressions (which is run by a routine evalav in sh.exp.c which
 * is a stripped down process), and `...` evaluation which is run
 * also by a subset of this code in sh.glob.c in the routine backeval.
 *
 * The code here is a little strange because part of it is interruptible
 * and hence freeing of structures appears to occur when none is necessary
 * if this is ignored.
 *
 * Note that if catch is not set then we will unwind on any error.
 * If an end-of-file occurs, we return.
 */
void
process(bool catch)
{
	register struct command *t;
	jmp_buf osetexit;

#ifdef TRACE
	tprintf("TRACE- process()\n");
#endif
	getexit(osetexit);
	for(;;) {
	    pendjob();
	    paraml.next = paraml.prev = &paraml;
	    paraml.word = S_;
	    t = 0;
	    setexit();
	    justpr = enterhist;		/* execute if not entering history */

	    /*
	     * Interruptible during interactive reads
	     */
	    if(setintr)
		(void)sigsetmask(sigblock(0) & ~sigmask(SIGINT));

	    /*
	     * For the sake of reset()
	     */
	    freelex(&paraml); freesyn(t); t = 0;

	    if(haderr) {
		if( !catch) {
		    doneinp = 0;
		    resexit(osetexit);
		    reset();
		}
		haderr = 0;
		/*
		 * Every error is eventually caught here or
		 * the shell dies.  It is at this
		 * point that we clean up any left-over open
		 * files, by closing all but a fixed number
		 * of pre-defined files.  Thus routines don't
		 * have to worry about leaving files open due
		 * to deeper errors... they will get closed here.
		 */
		closem();
		continue;
	    }
	    if(doneinp) {
		doneinp = 0;
		break;
	    }
	    if(chkstop)
		chkstop--;
	    if(neednote)
		pnote();
	    if(intty && prompt && !evalvec) {
		mailchk();
		/*
		 * If we are at the end of the input buffer
		 * then we are going to read fresh stuff.
		 * Otherwise, we are rereading input and don't
		 * need or want to prompt.
		 */
		if(fseekp == feobp)
		    printprompt();
	    }
	    errstr = 0;

	    /*
	     * Echo not only on VERBOSE, but also with history expansion.
	     * If there is a lexical error then we forego history echo.
	     */
	    if(lex(&paraml) && !errstr && intty || adrof(S_verbose)) {
		haderr = 1;
		prlex(&paraml);
		haderr = 0;
	    }
#ifdef DBG_CC
	    shprintf("DEBUG prlex ###\n");
	    prlex(&paraml);
#endif
	    /*
	     * The parser may lose space if interrupted.
	     */
	    if(setintr)
		(void)sigblock(sigmask(SIGINT));

	    /*
	     * Save input text on the history list if 
	     * reading in old history, or it
	     * is from the terminal at the top level and not
	     * in a loop.
	     */
	    if(enterhist || catch && intty && !whyles)
		savehist(&paraml);

	    /*
	     * Print lexical error messages, except when sourcing
	     * history lists.
	     */
	    if( !enterhist && errstr)
		error_errstr();

	    /*
	     * If had a history command :p modifier then
	     * this is as far as we should go
	     */
	    if(justpr)
		reset();
	    alias(&paraml);

	    /*
	     * Parse the words of the input into a parse tree.
	     */
	    t = syntax(paraml.next, &paraml, 0);
	    if(errstr)
		error_errstr();

	    /*
	     * Execute the parse tree
	     */
	    {
		/*
		 * POSIX requires SIGCHLD to be held
		 * until all processes have joined the
		 * process group in order to avoid race
		 * condition.
		 */
		int omask;

		omask = sigblock(sigmask(SIGCHLD));
		execute(t, tpgrp);
		(void)sigsetmask(omask &~ sigmask(SIGCHLD));
	    }

	    /*
	     * Made it!
	     */
	    freelex(&paraml), freesyn(t);
	}
	resexit(osetexit);
}

void
dosource(wchar_t **t)
{
	register wchar_t *f;
	register int u;
	bool hflg = 0;
	wchar_t buf[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- dosource()\n");
#endif
	t++;
	if(*t && eq(*t, S_h)) {
	    if(*++t == NOSTR)
		bferr(gettxt(_SGI_DMMX_csh_2fewargs, "Too few arguments"));
	    hflg++;
	}
	wscpy(buf, *t);
	f = globone(buf);
	wscpy(buf, f);
	u = dmove(open_(f, O_RDONLY), -1);
	xfree(f);
	freelex(&paraml);
	if(u < 0 && !hflg)
	    Perror(buf);			/* no return */
	(void)fcntl(u, F_SETFD, 1);
	srcunit(u, 0, hflg);
}

/*
 * Check for mail.
 * If we are a login shell, then we don't want to tell
 * about any mail file unless its been modified
 * after the time we started.
 * This prevents us from telling the user things he already
 * knows, since the login program insists on saying
 * "You have mail."
 */
static void
mailchk(void)
{
	register struct varent *v;
	register wchar_t **vp;
	time_t t;
	int intvl, cnt;
	bool new;
	char *s;
	struct stat stb;

#ifdef TRACE
	tprintf("TRACE- mailchk()\n");
#endif
	v = adrof(S_mail);
	if( !v)
	    return;				/* no mail path */
	(void)time(&t);
	vp = v->vec;
	cnt = blklen(vp);
	intvl = (cnt && number(*vp))? (--cnt, getn(*vp++)) : MAILINTVL;
	if(intvl < 1)
	    intvl = 1;
	if((chktim + intvl) > t)
	    return;

	for(; *vp; vp++) {
	    if (stat_(*vp, &stb) < 0)
		continue;
	    new = stb.st_mtime > time0.tv_sec;
	    if( !stb.st_size
		|| (stb.st_atime >= stb.st_mtime)
		|| ((stb.st_atime <= chktim) && (stb.st_mtime <= chktim))
		|| loginsh && !new)
			continue;
	    if(cnt == 1) {
		s = new?
		    gettxt(_SGI_DMMX_csh_nmail, "You have new mail") :
		    gettxt(_SGI_DMMX_csh_mail, "You have mail");
		showstr(MM_INFO, "%s", s);
	    } else {
		s = new?
		    gettxt(_SGI_DMMX_csh_nmailin, "New mail in %t") :
		    gettxt(_SGI_DMMX_csh_mailin,  "Mail in %t");
		showstr(MM_INFO, s, *vp);
	    }
	}
	chktim = t;
}

/*
 * Extract a home directory from the password file
 * The argument points to a buffer where the name of the
 * user whose home directory is sought is currently.
 * We write the home directory of the user back there.
 */
int
gethdir(wchar_t *home)
{
	register struct passwd *pp;
	register wchar_t *p;
	int cflag;
	char home_str[CSHBUFSIZ];
	wchar_t	home_ws[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- gethdir()\n");
#endif
	pp = getpwnam(tstostr(home_str, home, &cflag));
	if( !pp || cflag)
	    return(1);
	p = strtots(home_ws, pp->pw_dir, &cflag);
	if(cflag)
	    error(gettxt(_SGI_DMMX_csh_ihppasswd,
		"Illegal character in home path of passwd entry"));
	wscpy(home, p);
	return(0);
}

/*
 * Move the initial descriptors to their eventual
 * resting places, closing all other units.
 */
void
initdesc(void)
{
#ifdef TRACE
	tprintf("TRACE- initdesc()\n");
#endif
	didfds = 0;			/* 0, 1, 2 aren't set up */
	(void)fcntl(SHIN = dcopy(0, FSHIN), F_SETFD,  1);
	(void)fcntl(SHOUT = dcopy(1, FSHOUT), F_SETFD,  1);
	(void)fcntl(SHDIAG = dcopy(2, FSHDIAG), F_SETFD,  1);
	(void)fcntl(OLDSTD = dcopy(SHIN, FOLDSTD), F_SETFD,  1);
	closem();
}

void
done(int i)
{
	untty();
	_exit(i);
	/*NOTREACHED*/
}

/*XXXX*/
#include <time.h>

static struct tm *tim;

#define	TIMENULL	((struct tm *) 0)
#define	TIMEPR	'@'	/* character to include time/date in prompt */

char	*month[12] = {
	"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

char	*days[7] = {
	"Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

/*
 * Print current time in various gaudy formats
 */
static void
timepr(wchar_t c)
{
	int	h;
	int	hflag;
	long	tbuf;

	if (tim == TIMENULL) {
		(void) time(&tbuf);
		tim = localtime(&tbuf);
	}

	hflag = 0;
	switch (c) {
	  case 'n':
		shprintf("\n");
		break;
	  case 't':
		shprintf("\t");
		break;
	  case 'm' :
		shprintf("%02d",tim->tm_mon+1);
		break;
	  case 'd':
		shprintf("%02d",tim->tm_mday);
		break;
	  case 'y' :
		shprintf("%02d",tim->tm_year%100);
		break;
	  case 'D':
		shprintf("%02d/%02d/%02d",
		  tim->tm_mon+1,tim->tm_mday,tim->tm_year%100);
		break;
	  case 'H':
		shprintf("%02d",tim->tm_hour);
		break;
	  case 'M':
		shprintf("%02d",tim->tm_min);
		break;
	  case 'S':
		shprintf("%02d",tim->tm_sec);
		break;
	  case 'T':
		shprintf("%02d:%02d:%02d",
		  tim->tm_hour,tim->tm_min,tim->tm_sec);
		break;
	  case 'j':
		shprintf("%03d",tim->tm_yday+1);
		break;
	  case 'w':
		shprintf("%01d",tim->tm_wday);
		break;
	  case 'r':
		if ((h = tim->tm_hour) >= 12)
			hflag++;
		if ((h %= 12) == 0)
			h = 12;
		shprintf("%02d:%02d:%02d %cM",
		  h,tim->tm_min,tim->tm_sec,hflag ? 'P' : 'A');
		break;
	  case 'R':
		if ((h = tim->tm_hour) >= 12)
			hflag++;
		if ((h %= 12) == 0)
			h = 12;
		shprintf("%d:%02d%cM",
		  h,tim->tm_min,hflag ? 'P' : 'A');
		break;
	  case 'h':
		shprintf("%s",month[tim->tm_mon]);
		break;
	  case 'a':
		shprintf("%s",days[tim->tm_wday]);
		break;
	}
}

void
printprompt(void)
{
	register wchar_t *cp;

#ifdef TRACE
	tprintf("TRACE- printprompt()\n");
#endif
	if( !whyles) {
	    /*
	     * Zero out tim so that timepr() will read time the first time.
	     */
	    tim = TIMENULL;
	    for(cp = value(S_prompt); *cp; cp++) {
		if(*cp == HIST) {
		    shprintf("%d", eventno + 1);
		    continue;
		}
		if(*cp == '\\') {
		    if(cp[1] == HIST || cp[1] == '\\') {
			cp++;
		    } else if (cp[1] == TIMEPR && cp[2] != '\0') {
			cp += 2;
			timepr(*cp);
			continue;
		    }
		}
		shprintf("%T", (wchar_t)(*cp | QUOTE));
	    }
	} else
	    /* 
	     * Prompt for forward reading loop
	     * body content.
	     */
	    shprintf("? ");
	flush();
}
