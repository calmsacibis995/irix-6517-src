/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1990, 1991, 1992, 1993 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)make:main.c	1.24.1.2"
#include "defs"
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <pfmt.h>
#include <locale.h>
#include <paths.h>

/* NAME
**	make - maintain, update and regenerate groups of programs.
**
** OPTIONS for make
** 	'-f'  The description file is the next argument;
** 	     	(makefile is the default)
** 	'-p'  Print out a complete set of macro definitions and target
**		descriptions
** 	'-i'  Ignore error codes returned by invoked commands
** 	'-S'  stop after any command fails (normally do parallel work)
** 	'-s'  Silent mode.  Do not print command lines before invoking
**	'-n'  No execute mode.  Print commands, but do not execute them.
** 	'-t'  Touch the target files but don't issue command
**	'-g'  Turn on auto $(GET)
** 	'-q'  Question.  Check if object is up-to-date;
** 	      	returns exit code 0 if up-to-date, -1 if not
** 	'-u'  unconditional flag.  Ignore timestamps.
**	'-e'  environment variable override locals
**	'-r'  ignore built in rules
**	'-d'  Debug.  No-op unless compiled with -DMKDEBUG
**		prints nice debug tracking messages
**	'-D'  Debug alot.
**	'-w'  Set off warning msgs.
**	'-P'	Set on parallel.
**	'-B'	Block the target's output.
** Additional SGI options:
**	'-N'  turn on NULL suffix allowing dependencies option
**	'-M'  turn off NULL suffix ... option
**	'-O'  turn off -b'
**	'-b'  turn on MH DEFAULT handling (default)
*/

#ifndef MAKE_SHELL
#define MAKE_SHELL      _PATH_BSHELL
#endif

static char	makefile[] = "makefile",
	Makefile[] = "Makefile",
	Makeflags[] = "MAKEFLAGS",
	RELEASE[] = "RELEASE";

char	Nullstr[] = "",
	funny[CHAR_LIST];

extern CHARSTAR builtin[];
CHARSTAR *linesptr = builtin;

int parallel = PARALLEL ; /* The max. no. of parallel proc. that make will fork */
char tmp_block[30];
char * cur_makefile;
char * cur_wd;			/* Current working directory */
int nproc = 0 ;		  /* No. of process currently running */
int desc_start = NO;

FILE *fin;

struct nameblock pspace;
NAMEBLOCK curpname = &pspace,
	mainname,
	firstname;

LINEBLOCK sufflist;
VARBLOCK firstvar;
PATTERN firstpat ;
OPENDIR firstod;


void	(*intstat) (),
	(*quitstat) (),
	(*hupstat) (),
	(*termstat) ();

/*
**	Declare local functions and make LINT happy.
*/

static int	rddescf();
static int	rdd1();
static void	printdesc();
static void	prname();
static void	getmflgs();
static void	setflags();
static int	optswitch();
static void	usage();
static void	setmflgs();
static int	chkmflgs();
static void	setmmacs();
static int	chkmmacs();
static void	readenv();
static int	eqsign();
static void	callyacc();
       void	posix_env();
       void	convtmflgs();

static char temp_path[MAXPATHLEN];	/* max temp pathname */

int	Mflags = MH_DEP;

int	okdel = YES;

int	k_error = 0;			/* For -k option errors */

int	mf_dashed = 0;			/* If MAKEFLAGS is dashed format or not */

/*
 * Save command line args for a possible alternate make -
 * the arg parsing stuff trashes argv before we can determine
 * that we need to switch makes
 */
CHARSTAR *sargv;
int sargc;
static CHARSTAR Cmd;
/*
 * OUTMAX really needs to scale with ARG_MAX - we use outmax instead.
 * Since quite a few routines (subst()) have no length checking
 * we give ourselves some extra - its still easy to coredump make by
 * having too long a command line, but at least we'll never core dump when
 * we could have executed the command successfully.
 */
long outmax;
extern char yyfilename[];
extern int prereq();

main(argc, argv)
int	argc;
CHARSTAR argv[];
{
	void	intrupt();
	register NAMEBLOCK p;
	register CHARSTAR s;
	register VARBLOCK v;
	register int i;
	time_t	tjunk;
	char	*getenv(), *sname(),
		*m_getcwd(), *vptr;
	void	setvar(), enbint(), pwait();
	int	nfargs, chdir(), doname(), isdir(),
		descset = 0;
	char *args;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxepu");	
	(void)setlabel("UX:make");

	for (s = "#|=^();&<>*?[]:$`'\"\\\n" ; *s ; ++s)
		funny[(unsigned char)*s] |= META;
	/* make '$' a terminal so yylex can interpret */
	for (s = "\n\t :=;{}&>|$" ; *s ; ++s)
		funny[(unsigned char)*s] |= TERMINAL;
	funny[(unsigned char)'\0'] |= TERMINAL;

	TURNON(INTRULE);	/* Default internal rules, turned on */

	init_lex();

	builtin[1] = "MAKE=make";

	/*
	 * WARNING - this is only useful for error messages, etc - we
	 * permit make to continue (and set this to ".") if the getcwd
	 * fails.
	 */
	cur_wd = m_getcwd();
	outmax = sysconf(_SC_ARG_MAX) + 8000;

        /* remember args since setflags trashes */
        Cmd = argv[0];
        sargv = malloc((argc+1-1) * sizeof(CHARSTAR));
        for(i=1; i<argc; ++i)
                sargv[i-1] = argv[i];
        sargv[i-1] = NULL;
        sargc = argc - 1;

	v = varptr("SHELL");
	v->varval.charstar = MAKE_SHELL;
	v->envflg = YES;
	v->noreset = NO;

	TURNON(EXPORT);			/* Export $(MAKEFLAGS) macros */
	getmflgs();			/* Init $(MAKEFLAGS) variable */
	TURNOFF(EXPORT);

	setflags(argc, argv);		/* Set command line flags */

	setvar("$", "$");

	/*	Read command line "=" type args and make them readonly.	*/

	TURNON(INARGS | EXPORT);
#ifdef MKDEBUG
	if (IS_ON(DBUG)) 
		printf("Reading \"=\" args on command line.\n");
#endif
        args = ck_malloc(outmax);
	strcpy(args,"MAKEARGS="); /* set to all command line macros */
	/* set a yyfilename - eqsign calls yacc that may output an error */
	strcpy(yyfilename, "Command line argruments");
	for (i = 1; i < argc; ++i)
		if ( argv[i] && argv[i][0] != MINUS &&
		     eqsign(argv[i]) ) {
			strcat(args, " '");
			strcat(args, argv[i]);
			strcat(args, "'");
			setmmacs(argv[i]);	/* POSIX Add to MAKEFLAGS */
			argv[i] = 0;
		}
	callyacc(args);

	TURNOFF(INARGS | EXPORT);

	if (IS_ON(INTRULE)) {	/* Read internal definitions and rules. */
#ifdef MKDEBUG
		if (IS_ON(DBUG))
			printf("Reading internal rules.\n");
#endif
		strcpy(yyfilename, "Internal Rules");
		(void)rdd1((FILE *) NULL);
	}
	TURNOFF(INTRULE);	/* Done with internal rules, now. */

	/* Read environment args.
	 * Let file args which follow override, unless 'e'
	 * in MAKEFLAGS variable is set.
	 */
	if (chkmflgs('e'))		/* New POSIX format but backward compat */
		TURNON(ENVOVER);
#ifdef MKDEBUG
	if (IS_ON(DBUG))
		printf("Reading environment.\n");
#endif
	TURNON(EXPORT);
	strcpy(yyfilename, "Environment variables");
	readenv();
	TURNOFF(EXPORT | ENVOVER);

	/*	Get description file in the following order:
	**	 - command line '-f' parameters
	**	 - default names (makefile, Makefile, s.makefile, s.Makefile)
	*/
	desc_start = YES;
	for (i = 1; i < argc; i++)
		if ( argv[i] && (argv[i][0] == MINUS) &&
		    (argv[i][1] == 'f') && (argv[i][2] == CNULL)) {
			argv[i] = 0;
			if (i >= argc - 1 || argv[i+1] == 0)
				fatal(":117:no description file after -f flag (bu1)");
			if ( rddescf(argv[++i], YES) )
				fatal1(":118:cannot open %s", argv[i]);
			argv[i] = 0;
			++descset;
		}
	if ( !descset ) {
		if ( rddescf(makefile, NO))
			if ( rddescf(Makefile, NO) )
				if ( rddescf(makefile, YES))
					(void)rddescf(Makefile, YES);
	}
	/* Set the POSIX default for ARFLAGS */

	if(IS_ON(POSIX) && !getenv("ARFLAGS") ) {
		setvar("ARFLAGS","-rv");
	}

	if (IS_ON(PRTR)) 
		printdesc(NO);

	if ( p = SRCHNAME(".IGNORE") ) {
		/* POSIX: Turn on globally only if no prerequisites */
		if(IS_ON(POSIX)) {
			if(!prereq(p,(CHARSTAR) 0))
				TURNON(IGNERR);
		}
		else	TURNON(IGNERR);
	}
	if ( p = SRCHNAME(".SILENT") ) {
		/* POSIX: Turn on globally only if no prerequisites */
		if(IS_ON(POSIX)) {
			if(!prereq(p,(CHARSTAR) 0))
				TURNON(SIL);
		}
		else	TURNON(SIL);
	}
	/*
	 * Permit flags to be turned on from within a Makefile
	 * We don't use .MAKEFLAGS since smake already uses that
	 */
	if (p = SRCHNAME(".MAKEOPTS")) {
		DEPBLOCK d;
		LINEBLOCK lp;
		for (lp = p->linep; lp; lp = lp->nextline) {
			for (d = lp->depp; d; d = d->nextdep)
				if (d->depname->namep[0] == '-')
					optswitch(d->depname->namep[1]);
		}
	}

	/* perform sanity checking for parallel (after MAKEOPTS) */
	if(IS_ON(PAR) && IS_ON(NOEX))
		TURNOFF(PAR);

	if (IS_ON(PAR)) {
		if(IS_ON(BLOCK))
			sprintf(tmp_block,"/var/tmp/make%d", getpid());

		if ((vptr = getenv("PARALLEL")) && !(STREQ(vptr, "")))
			if((parallel = atoi(vptr)) <= 0 )
				parallel = 1;
	} else
		TURNOFF(BLOCK);


	if (p = SRCHNAME(".SUFFIXES")) 
		sufflist = p->linep;

	intstat = (void(*)()) signal(SIGINT, SIG_IGN);
	quitstat = (void(*)()) signal(SIGQUIT, SIG_IGN);
	hupstat = (void(*)()) signal(SIGHUP, SIG_IGN);
	termstat = (void(*)()) signal(SIGTERM, SIG_IGN);
	enbint(intrupt);

	nfargs = 0;

	for (i = 1; i < argc; ++i)
		if ( s = argv[i] ) {
			if ( !(p = SRCHNAME(s)) )
				p = makename(s);

			++nfargs;

			(void)doname(p, 0, &tjunk );
			if(IS_ON(PAR)){
#ifdef MKDEBUG
			if (IS_ON(DBUG)) 
				printdesc(YES);
#endif
				pwait(0);
			}
#ifdef MKDEBUG
			if (IS_ON(DBUG)) 
				printdesc(YES);
#endif
		}

	/*
	 * If no file arguments have been encountered, make the first
	 * name encountered that doesn't start with a dot
	 */
	if ( !nfargs )
		if ( !mainname )
			fatal(":119:no arguments or description file (bu7)");
		else {
			(void)doname(mainname, 0, &tjunk);
			if(IS_ON(PAR)){
#ifdef MKDEBUG
			if (IS_ON(DBUG)) 
				printdesc(YES);
#endif
				pwait(0);
			}
#ifdef MKDEBUG
			if (IS_ON(DBUG)) 
				printdesc(YES);
#endif
		}

	mkexit(0);	/* make succeeded; no fatal errors */
	/*NOTREACHED*/
}




void
intrupt()
{
	time_t	exists();
	int	isprecious(), member_ar(), unlink();
	void	mkexit();
	int	lev, ret_isdir;
	CHARSTAR p;

	NAMEBLOCK lookup_name();

	/*
	 * wait for all children to finish so they can get any
	 * errors out
	 */
	while (wait(NULL) != -1 && errno != ECHILD)
		;
	if (okdel && IS_OFF(NOEX) && IS_OFF(TOUCH) && 
	    (p = (varptr("@")->varval.charstar)) &&
	    (exists( lookup_name(p), &lev) != -1) && 
	    (!isprecious(p)) &&
	    (!member_ar(p)))	/* don't remove archives generated during make */
	{
		if ((ret_isdir = isdir(p)) == 1)
			pfmt(stderr, MM_NOSTD,
				":102:\n*** %s NOT removed.\n", p);
		else if ( !( ( ret_isdir == 2 ) || unlink(p) ) )
			pfmt(stderr, MM_NOSTD,
				":103:\n*** %s removed.\n", p);
	}
	fprintf(stderr, "\n");
	mkexit(2);
	/*NOTREACHED*/
}


void
enbint(onintr)
void (*onintr)();
{
	if (intstat == (void(*)())SIG_DFL)
		(void)sigset(SIGINT, onintr);
	if (quitstat == (void(*)())SIG_DFL)
		(void)sigset(SIGQUIT, onintr);
	if (hupstat == (void(*)())SIG_DFL)
		(void)sigset(SIGHUP, onintr);
	if (termstat == (void(*)())SIG_DFL)
		(void)sigset(SIGTERM, onintr);
}




static int
rddescf(descfile, sflg)		/* read and parse description file */
CHARSTAR descfile;
int	sflg;		/* if YES try s.descfile */
{
	void	setvar();
	FILE *k;

	setvar("MAKEFILE", descfile);
	strcpy(yyfilename, descfile);
	if (STREQ(descfile, "-")) {
		cur_makefile = descfile;

		return( rdd1(stdin) );
	}

retry:
	strcpy(yyfilename, descfile);
	if (k = fopen(descfile, "r")) {
#ifdef MKDEBUG
		if (IS_ON(DBUG))
			printf("Reading %s\n", descfile);
#endif
		cur_makefile = descfile;
		return( rdd1(k) );
	}

	if ( !sflg || !get(descfile, varptr(RELEASE)->varval.charstar,0) )
		return(1);
	sflg = NO;
	goto retry;
}


/**	used by yyparse		**/
extern int	yylineno;
extern CHARSTAR zznextc;


static int
rdd1(k)
FILE *k;
{
	int yyparse();
	void	fatal();

	fin = k;
	yylineno = 0;
	zznextc = 0;

	if ( yyparse() )
		fatal(":120:description file error (bu9)");

	if ( ! (fin == NULL || fin == stdin) )
		(void)fclose(fin);

	return(0);
}


static void
printdesc(prntflag)
int	prntflag;
{
	register NAMEBLOCK p;
	register VARBLOCK vp;
	register CHAIN pch;
	FILE *op;

        if (prntflag)
                op = stderr;
        else
                op = stdout;
        fflush(stdout);
        fflush(stderr);
	if (prntflag) {
		register OPENDIR od;
		pfmt(op, MM_NOSTD, ":104:Open directories:\n");
		for (od = firstod; od; od = od->nextopendir)
			fprintf(op, "\t%s\n", od->dirn);
	}
	if (firstvar) 
		pfmt(op, MM_NOSTD, ":105:Macros:\n");
	for (vp = firstvar; vp; vp = vp->nextvar)
		if ( !(vp->v_aflg) )
			fprintf(op, "%s = %s\n", vp->varname,
				((vp->varval.charstar) == NULL? " ":vp->varval.charstar));
		else {
			pfmt(op, MM_NOSTD,
				":106:Lookup chain: %s\n\t", vp->varname);
			for (pch = (vp->varval.chain); pch; pch = pch->nextchain)
				fprintf(op, " %s", (pch->datap.nameblock)->namep);
			fprintf(op, "\n");
		}

	for (p = firstname; p; p = p->nextname)
		prname(p, prntflag);
	fprintf(op, "\n");
	(void)fflush(op);
}

#include "termio.h"

static void
prname(p, prntflag)
register NAMEBLOCK p;
{
	register DEPBLOCK dp;
	register SHBLOCK sp;
	register LINEBLOCK lp;
        FILE *op;
	static int cols = -1;
	register int deplen;

        if (prntflag)
                op = stderr;
        else
                op = stdout;

	if (cols < 0) {
		/* determine # columns on screen */
		struct winsize w;
		if (ioctl(fileno(op), TIOCGWINSZ, &w) < 0)
			cols = 80;
		else
			cols = w.ws_col;
	}
	fprintf(op, "\nTARGET:%s:", p->namep);

	if (prntflag)
		fprintf(op, "  done=%d", p->done);

	if (p == mainname) 
		pfmt(op, MM_NOSTD, ":107:(MAIN NAME)");

	for (lp = p->linep; lp; lp = lp->nextline) {
		if ( dp = lp->depp ) {
			pfmt(op, MM_NOSTD, ":108:\n depends on:");
			for (; dp; dp = dp->nextdep)
				if ( dp->depname) {
                                        register ilen;
                                        ilen = strlen(dp->depname->namep) + 1;
                                        if (ilen + deplen >= cols) {
                                                deplen = 12;
                                                fprintf(op, "\n\t    ");
                                        }
                                        deplen += ilen;
                                        fprintf(op, "%s ", dp->depname->namep);
				}
		}
		if (sp = lp->shp) {
			fprintf(op, "\n");
			pfmt(op, MM_NOSTD, ":109:commands:\n");
			for ( ; sp; sp = sp->nextsh)
				fprintf(op, "\t%s\n", sp->shbp);
		}
	}
}

/*
 * POSIX: $(MAKEFLAGS) now contains options and command line and user 
 *	  defined macros.
 *	  Options can be "abcd" or "-a -b -c -d"
 *	  Macros are white space seperated with no spaces around '=' char.
 */

static void
getmflgs()		/* export command line flags for future invocation */
{
	void	setvar();
	int	sindex();
	register CHARSTAR m, args, mf, eq;
	register VARBLOCK vpr = varptr(Makeflags);

        args = ck_malloc(outmax);
	setvar(Makeflags, args);
	vpr->varval.charstar[0] = CNULL;
	vpr->envflg = YES;
	vpr->noreset = YES;
	/*optswitch('b');*/

	if(!(mf = getenv("MAKEFLAGS")))
		return;
	m = mf;
	while(m=strchr(m,'=')){
		if(isspace(*(m-1))) {
			printf("Environment MAKEFLAGS: No white space before '=' char");
			mkexit(1);
		}
		++m;
	}
	while(*mf) {
		char *mac;
		int len;

		switch(*mf) {
			case '-':	/* Handle single letter dashed option */
				++mf_dashed;
				++mf;
				optswitch(*mf);
				if(*mf == 'f') { /* Skip past possible f option arg */
					while(isspace(*mf))++mf;
					while(*mf && !isspace(*mf)) ++mf;
				}
				else 	++mf;
				break;
			case ' ':
			case '\t':
				++mf;
				break;
			case '\'':
			case '\"':
				++mf;	/* Skip user imbedded chars */
				break;
			default:	/* Handle undashed options or macros */
				m=mf;
				if(!strchr(mf,'=')) {	/* No macros - just options */
					optswitch(*mf++);
					break;
				}
				eq = strchr(mf,'=');
				while(*m && !isspace(*m))++m;	/* Past starting chars */
				if(m < eq) {	
					optswitch(*mf++);	/* Options */
					break;
				}
					/* Macros */
				m=mf;
				while(*m && !isspace(*m)) ++m;
				mac = ck_malloc(m-mf+1);
				strncpy(mac,mf,m-mf);
				mac[m-mf] = CNULL;
				callyacc(mac);
				setmmacs(mac);
				free(mac);
				mf = m;
				break;
		}
	}
}


static void
setflags(ac, av)
register int	ac;
CHARSTAR *av;
{
	register int	i, j;
	register char	c;
	int	flflg = 0; 		/* flag to note '-f' option. */

	for (i = 1; i < ac; ++i) {
		if (flflg ) {
			flflg = 0;
			continue;
		}
		if (av[i] && av[i][0] == MINUS) {
			if (ANY(av[i], 'f'))
				flflg++;

			for (j = 1 ; (c = av[i][j]) != CNULL ; ++j)
				if(optswitch(c))
					break;

			if (flflg)
				av[i] = "-f";
			else
				av[i] = 0;
		}
	}
}



static  int
optswitch(c)	/* Handle a single character option */
char	c;
{
	switch (c) {

	case 's':	/* silent flag */
		TURNON(SIL);
		setmflgs(c);
		return(0);

	case 'n':	/* do not exec any commands, just print */
		TURNON(NOEX);
		setmflgs(c);
		return(0);

	case 'e':	/* environment override flag */
		setmflgs(c);
		return(0);

	case 'p':	/* print description */
		TURNON(PRTR);
		return(0);

	case 'i':	/* ignore errors */
		TURNON(IGNERR);
		setmflgs(c);
		return(0);

	case 'S':
		TURNOFF(KEEPGO);
		setmflgs(c);
		return(0);

	case 'k':
		TURNON(KEEPGO);
		setmflgs(c);
		return(0);

	case 'r':	/* turn off internal rules */
		TURNOFF(INTRULE);
		return(0);

	case 't':	/* touch flag */
		TURNON(TOUCH);
		setmflgs(c);
		return(0);

	case 'q':	/* question flag */
		TURNON(QUEST);
		setmflgs(c);
		return(0);

	case 'g':	/* turn default $(GET) of files not found */
		TURNON(GET);
		setmflgs(c);
		return(0);

	case 'b':	/* use MH version of test for whether cmd exists */
		TURNON(MH_DEP);
		setmflgs(c);
		return(0);

/* turn off -b flag */
	case 'O':	
		TURNOFF(MH_DEP);
		setmflgs(c);
		return(0);

	case 'd':	/* debug flag */
	case 'D':
#ifdef MKDEBUG
		if (c == 'd')
			TURNON(DBUG2);
		else
			TURNON(DBUG);
		setmflgs(c);
		setlinebuf(stdout);
		setlinebuf(stderr);
#endif
		return(0);

	case 'm':	/* print memory map is not supported any more */
		return(0);

	case 'f':	/* named makefile: handled by setflags() */
		return(0);

	case 'u':	/* unconditional build indicator */
		TURNON(UCBLD);
		setmflgs(c);
		return(0);

	case 'w':	/* Set off warning msgs */
		TURNON(WARN);
		setmflgs(c);
		return(0);

	case 'P':	/* Set on parallel */
		TURNON(PAR);
		setmflgs(c);
		return(0);

	case 'B':	/* Set on target's output blocking */
		TURNON(BLOCK);
		setmflgs(c);
		return(0);
	case 'M':	/* deny explicit null suffix dependencies */
		TURNOFF(NULLSFX);
		setmflgs(c);
		return(0);
	case 'N':	/* permit explicit null suffix dependencies */
		TURNON(NULLSFX);
		setmflgs(c);
		return(0);
	case '-':
		return(1);
	}

	usage(c);
	/* NOTREACHED */
}


static void
usage(c)
char c;
{
	pfmt(stderr, MM_ACTION, _SGI_MMX_make_usage
		":Usage: make [-f makefile] [-p] [-i] [-k] [-s] [-r] [-n] [-u]\n\t");
#ifdef MKDEBUG
	fprintf(stderr, "[-d] [-D] ");
#endif
	fprintf(stderr, "[-S] [-g] [-w] [-P] [-B] [-b] [-O] [-e] [-t] [-q] [-M] [-N] [names]\n");
	mkexit(1);
}

/*	Called from parser to reconvert MAKEFLAGS variable back to original
 *	format, that is, without macros and single letter dashed options.
 */
void
convtmflgs()
{
	register CHARSTAR m = (varptr(Makeflags))->varval.charstar;
	register CHARSTAR s, args;
	void	 setvar();
	int off = 0;

	if(s = strchr(m,'=')){	/* Macros */
		/* Search backwards for start of macro */
		--s;
		while(s > m && !isspace(*s)) --s;
		*s = CNULL;
	}

	args = ck_malloc(29);	/* Old style was 29 'Z' letters */
	args[off] = CNULL;
	s = m;
	while(*s) {
		if(*s != '-' && !isspace(*s))
			args[off++] = *s;
		++s;
	}
	args[off] = CNULL;
	free(m);
	(varptr(Makeflags))->noreset = NO;
	setvar(Makeflags, args);
	(varptr(Makeflags))->noreset = YES;
}

/*	Called from parser to establish POSIX environment */
void
posix_env()
{
	VARBLOCK srchvar();
	void	 setvar();
	register VARBLOCK mp,sp;

	if(mp = srchvar("MAKEFLAGS_POSIX")){
		strcpy((srchvar(Makeflags))->varval.charstar,mp->varval.charstar);
		free(mp->varval.charstar);
		mp->varval.charstar = "";
	}
	if(sp = srchvar("SHELL_POSIX")){
		strcpy((srchvar("SHELL"))->varval.charstar,sp->varval.charstar);
		free(sp->varval.charstar);
		sp->varval.charstar = "";
	}
	(srchvar(Makeflags))->noreset = NO;
}

/*
 *	POSIX: Called from readenv() to prevent overwriting the MAKEFLAGS and SHELL 
 *      variables and to prevent overwriting any macros included in the MAKEFLAGS variable.
 *	We only know if we are in a POSIX environment after the makefile is parsed.
 *	Save the "MAKEFLAGS" and "SHELL" variables and restore from the parser when
 *	we know we are POSIX. See posix_env().
 */

static int
chkmmacs(mac)
register char	*mac;
{
	register CHARSTAR m = (varptr(Makeflags))->varval.charstar;
	register CHARSTAR s = (varptr("SHELL"))->varval.charstar;
	register CHARSTAR a,l,mp,sp;
	register int mlen;
	void setvar();

	/* Insure new invocations do not inherit these flags */
	if(strncmp(mac,"MAKEFLAGS_POSIX=",16)==0 || strncmp(mac,"SHELL_POSIX=",12)==0)
		return(0);

	if(strncmp(mac,"MAKEFLAGS=",10)==0) {
		mp = ck_malloc(outmax);
		strcpy(mp,m);
		setvar("MAKEFLAGS_POSIX",mp);	/* Save and allow to be overwritten */
		return(1);
	}
	if(strncmp(mac,"SHELL=",6)==0 ) {
		sp = ck_malloc(strlen(s)+1);
		strcpy(sp,s);
		setvar("SHELL_POSIX",sp);	/* Save and allow to be overwritten */
		return(0);
	}
	a = m;
	mlen = (strchr(mac,'=') - mac);
	while(a = strchr(a,'=')) {
		l = a;
		while(!isspace(*a) && a > m)--a;	/* Beginning of macro */
		if(isspace(*a)) ++a;
		if(strncmp(a,mac,mlen)==0)
			return(0);
		a = l+1;
	}
	return(1);
}


static void
setmmacs(mac)		/* POSIX: Add macro to $(MAKEFLAGS) */
register char	*mac;
{
	register CHARSTAR p = (varptr(Makeflags))->varval.charstar;
	register CHARSTAR s;
	register int mlen = 0;

	s = mac;
	while(*s) {
		if(!isspace(*s))
			++mlen;
		++s;
	}
	if((mlen + strlen(p) + 2) > outmax) {
		fatal(":169:MAKEFLAGS buffer overrun");
	}
	s = p + strlen(p);
	if(s != p)
		*s++ = ' ';
	while(*mac){
		if(!isspace(*mac))	/* Trim out spaces */
			*s++ = *mac;
		++mac;
	}
	*s = CNULL;
}


static int
chkmflgs(c)		/* Check $(MAKEFLAGS) for option */
register char	c;
{
	register CHARSTAR p = (varptr(Makeflags))->varval.charstar;
	register CHARSTAR mac,o;

	if(mac = strchr(p,'='))
		while(!isspace(*mac) && mac > p)--mac;

	if((!mac && *p) || (mac && mac > p)) {	/* We have options */
		if(*p == '-') {
			for(o=p;o = strchr(o,'-');)
				if(c == *++o)
					return(1);		/* Duplicate */
			return(0);
		} else {
			for (; *p && !isspace(*p); p++)
				if (*p == c)
					return(1);		/* Duplicate */
			return(0);
		}
	}
	return(0);
}


/* Add the option to $(MAKEFLAGS) */

static void
setmflgs(c)		/* set up the cmd line input flags for EXPORT. */
register char	c;
{
	register CHARSTAR p = (varptr(Makeflags))->varval.charstar;
	register int mlen = strlen(p);
	register CHARSTAR mac, sv, o;

	if((mlen+(mf_dashed?3:1)) > outmax) {		/* Overrun */
		fatal(":169:MAKEFLAGS buffer overrun");
	}
	/* If macros exist, save and append them after option insertion */
	if(mac = strchr(p,'=')) {
		while(!isspace(*mac) && mac > p)--mac;
		if(isspace(*mac)) {
			*mac = CNULL;
			++mac;
			sv = ck_malloc(strlen(mac)+1);
			strcpy(sv,mac);
		}
		else {
			sv = ck_malloc(strlen(mac)+1);
			strcpy(sv,mac);
			*p = CNULL;
		}
	}
	if(*p) {		/* Existing options */
		char opt[4];
		if(*p == '-') {
			for(o=p;o = strchr(o,'-');)
				if(c == *++o)
					return;		/* Duplicate */
			sprintf(opt," -%c",c);		/* Append */
			strcat(p,opt);
		} else {
			for (; *p && !isspace(*p); p++)
				if (*p == c)
					return;		/* Duplicate */
			sprintf(opt,"%c",c);		/* Append */
			strcat(p,opt);
		}
	} else {		/* No options */
		if(mf_dashed) {
			*p++ = '-';
			*p++ = c;
			*p   = CNULL;
		} else {
			*p++ = c;
			*p   = CNULL;
		}
	}
	if(mac) {
		strcat(p," ");
		strcat(p,sv);
		free(sv);
	}
}


/*
 *	If a string like "CC=" occurs then CC is not put in environment.
 *	This is because there is no good way to remove a variable
 *	from the environment within the shell.
 *	Note: POSIX wants these empty variables, so in setenv(), don't
 *	export them if in non-POSIX environment.
 */
static void
readenv()
{
	register CHARSTAR *ea, p;

	ea = environ;
	for (; *ea; ea++) {
		for (p = *ea; *p && *p != EQUALS; p++)
			;
		/* if ((*p == EQUALS) && *(p + 1)) */
		if (*p == EQUALS) {		/* POSIX */
			if(chkmmacs(*ea)) 	/* POSIX Don't overwrite certain var or macros */
				(void)eqsign(*ea);
		}
	}
}


static int
eqsign(a)
register CHARSTAR a;
{
	register CHARSTAR p;

	/* allow most anything in a macro name - this begins to help
	 * porting from VMS etc where file names have strange chars
	 */
	for (p = "="; *p; p++)
		if (ANY(a, *p)) {
			callyacc(a);
			return(YES);
		}
	return(NO);
}


static void
callyacc(str)
register CHARSTAR str;
{
	CHARSTAR lines[2];
	FILE 	*finsave = fin;
	CHARSTAR *lpsave = linesptr;
	char fnsave[PATH_MAX];

	strcpy(fnsave, yyfilename);
	fin = 0;
	lines[0] = str;
	lines[1] = 0;
	linesptr = lines;
	(void)yyparse();
	fin = finsave;
	linesptr = lpsave;
	strcpy(yyfilename, fnsave);
}

NAMEBLOCK
lookup_name(namep)
CHARSTAR namep;
{
	NAMEBLOCK p;
	for (p = firstname; p; p = p->nextname) {
		if (STREQ(namep, p->namep)) 
			return (p);
	}
	return ( NULL );
}
char *
m_getcwd()
{
char *getcwd();
char *p;
	/*
	 * SGI - this is ONLY used for printing error messages in
	 * parallel mode - but some customers like to run make in
	 * an area they can't do a pwd in :-) so we make this optional
	 */
	if(getcwd(temp_path, MAXPATHLEN) == NULL)
 		strcpy(temp_path, ".");

	p = ck_malloc(strlen(temp_path) + 1);
	strcpy(p, temp_path);
	return(p);
}

#include "sys/stat.h"
CHARSTAR findfl(CHARSTAR, CHARSTAR);
CHARSTAR execat(CHARSTAR, CHARSTAR, CHARSTAR);
static char *sstr(char *);
/*
 * donmake - handle #! on 1st line of makefile
 */
void
donmake(line)
char *line;
{
	int len;
	char *from = line;
	char buf[128];
	char *ncmd, *nCmd;
	char **nargs;
	char **nnargs, **pnnargs;
	int i, ncargs;
	int havefflag = 0;
	int fflgloc;
	struct stat sc, sn;

	/* grab additional options on #! line */
	nargs = malloc(sizeof(char *));
	nargs[0] = NULL;
	ncargs = 0;
	for(; len=getword(from,buf); from += len) {
		if (buf[0] == TAB || buf[0] == BLANK)
			continue;
		/*
		 * If a makefile is suitable to be run as a shell
		 * script it may have a line like:
		 * #!/bin/make -kf
		 * where exec will add the name of the makefile AFTER
		 * the f... this is incompatible from the way we do
		 * things.
		 * Also, it would be nice if it were possible that
		 * in makes #! line one could specify an alternate
		 * makefile:
		 * #!smake -f smakefile
		 * 
		 * So: if we find a lone f option - we squash it
		 * if we find an f option with an argument - we squash
		 * any command line f options
		 */
		if (buf[0] == '-' && ANY(&buf[1], 'f')) {
			havefflag = 1;
			fflgloc = ncargs;
		} else if (havefflag == 1) {
			/* have a -f flag and now an argument - set
			 * flag so that we squash any original -f arg 
			 */
			havefflag = 2;
		}

		nargs = realloc(nargs, (ncargs+1)*sizeof(char *));
		nargs[ncargs++] = sstr(buf);
	}


	/* if we had a lone -f option then havefflag will be 1 -
	 * we need to go back and erase it
	 */
	if (havefflag == 1) {
		char *floc;
		if (nargs[fflgloc][1] == 'f') {
			/* lone flag */
			nargs[fflgloc] = NULL;
		} else {
			floc = strchr(nargs[fflgloc], 'f');
			*floc = '\0';
		}
	}

	/* now have all args */
	if (nargs[0] == NULL ||
	    ((ncmd = findfl(nargs[0], "PATH")) == (CHARSTAR) -1) ||
	    (stat(ncmd, &sn) != 0)) {
		/* if fail - just proceed */
		free(nargs);
		if(IS_ON(DBUG) || IS_ON(DBUG2))
			pfmt(stdout, MM_NOSTD, _SGI_MMX_make_noaltmake
			":Cannot find alternate make:%s\n", nargs[0]);
		return;
	}

	ncmd = sstr(ncmd);	/* findfl overwrites */
	nCmd = Cmd;
	if (*Cmd == '/' || (nCmd = findfl(Cmd, "PATH")) != (CHARSTAR) -1) {
		/* do a stab at making sure we don't exec ourselves */
		if (stat(nCmd, &sc) == 0 && stat(ncmd, &sn) == 0) {
			if (sc.st_dev == sn.st_dev &&
			    sc.st_ino == sn.st_ino) {
				if(IS_ON(DBUG))
					pfmt(stdout, MM_NOSTD, _SGI_MMX_make_ignaltmake
					":Ignoring alternate make; its me!:%s\n",
					ncmd);
				return;
			}
		}
	}
	
	/* now insert all #! args before command line args */
	nnargs = malloc((ncargs+sargc+1) * sizeof (char*));
	pnnargs = nnargs;
	for (i = 0; i < ncargs; i++)
		if (nargs[i])
			*pnnargs++ = nargs[i];
	for (i = 0; i < sargc; i++) {
		if (havefflag == 2 && sargv[i][0] == '-' &&
					ANY(&sargv[i][1], 'f')) {

			/* remove -f and next arg
			 * note that 'f' MUST be last
			 */
			if (sargv[i][1] == 'f')
				/* skip sargv[i] & sargv[i+1] */
				i++;
			else {
				char *floc;
				/* remove f and sargv[i+1] */
				*pnnargs = sstr(sargv[i]);
				floc = strchr(*pnnargs, 'f');
				*floc = '\0';
				pnnargs++;
				i++;
			}
			continue;
		}
		*pnnargs++ = sargv[i];
	}
	*pnnargs = NULL;
	if (IS_ON(DBUG) || IS_ON(DBUG2)) {
		pfmt(stdout, MM_NOSTD, _SGI_MMX_make_swaltmake
			":Switching to alternate make:%s\nargs:", ncmd);
		for (pnnargs = nnargs; *pnnargs; pnnargs++)
			printf("%s ", *pnnargs);
		printf("\n");
	}
	execv(ncmd, nnargs);

	/* if fail - just proceed */
	free(nnargs);
	free(nargs);
}

static char *
sstr(s)
char *s;
{
	char *r;

	r = ck_malloc(strlen(s) + 1);
	strcpy(r, s);
	return(r);
}

/*
 *	findfl(name)	(like execvp, but does path search and finds files)
 */
static char fname[PATH_MAX];

CHARSTAR
findfl(name, varnm)
register CHARSTAR name;
CHARSTAR varnm;			/* variable containing path information */
{
	register CHARSTAR p;
	register VARBLOCK cp;
	char	tempbuf[PATH_MAX];
	char	*tempval = tempbuf;
	size_t	len;

	if(name[0] == SLASH)
		return(name);
	cp = varptr(varnm);
	if(*cp->varval.charstar == 0)
		p = ":";
	else
		p = cp->varval.charstar;

	/* Since subst() doesn't take into account the length of the
	 * variable, if input path is greater than PATH_MAX then
	 * fail-over to dynamic allocation.
	 */
	if ((len = strlen(p)) >= PATH_MAX) {
		tempval = ck_malloc(len + 1);
	}

	subst(p, tempval, 0);
	p = tempval;

	do
	{
		p = execat(p, name, fname);
		if(access(fname, 4) == 0) {
			if (tempval != tempbuf)
				free(tempval);
			return(fname);
		}
	} while (p);

	if (tempval != tempbuf)
		free(tempval);
	return((CHARSTAR )-1);
}

CHARSTAR
execat(s1, s2, si)
register CHARSTAR s1, s2;
CHARSTAR si;
{
	register CHARSTAR s;

	s = si;
	while (*s1 && *s1 != KOLON)
		*s++ = *s1++;
	if (si != s)
		*s++ = SLASH;
	while (*s2)
		*s++ = *s2++;
	*s = CNULL;
	return(*s1? ++s1: 0);
}
