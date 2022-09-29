/*-
 * main.c --
 *	The main file for this entire program. Exit routines etc
 *	reside here.
 *
 * Copyright (c) 1988, 1989 by the Regents of the University of California
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any non-commercial purpose
 * and without fee is hereby granted, provided that the above copyright
 * notice appears in all copies.  The University of California,
 * Berkeley Softworks and Adam de Boor make no representations about
 * the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * Utility functions defined in this file:
 *	Main_ParseArgLine   	Takes a line of arguments, breaks them and
 *	    	  	    	treats them as if they were given when first
 *	    	  	    	invoked. Used by the parse module to implement
 *	    	  	    	the .MFLAGS target.
 *
 *	Error	  	    	Print a tagged error message. The global
 *	    	  	    	MAKE variable must have been defined. This
 *	    	  	    	takes a format string and two optional
 *	    	  	    	arguments for it.
 *
 *	Fatal	  	    	Print an error message and exit. Also takes
 *	    	  	    	a format string and two arguments.
 *
 *	Punt	  	    	Aborts all jobs and exits with a message. Also
 *	    	  	    	takes a format string and two arguments.
 *
 *	Finish	  	    	Finish things up by printing the number of
 *	    	  	    	errors which occured, as passed to it, and
 *	    	  	    	exiting.
 */
#if !defined(lint) && defined(keep_rcsid)
static char     *rcsid = "Id: main.c,v 1.68 90/03/12 15:50:25 adam Exp $ SPRITE (Berkeley)";
#endif lint

#include    <stdio.h>
#ifdef Sprite
#include    <stdlib.h>
#endif /* Sprite */
#include    <sys/types.h>
#include    <sys/signal.h>
#include    <sys/stat.h>
#include    <fcntl.h>
#include    <sys/errno.h>
#include    <ctype.h>
#include    "make.h"
#ifdef sgi
#include    <malloc.h>
#include    <locale.h>
#include    <sys/sysmp.h>
#endif

extern int errno;
void exit(int);


#ifndef DEFMAXLOCAL
#define DEFMAXLOCAL DEFMAXJOBS
#endif  DEFMAXLOCAL

#define MAKEFLAGS  	".MAKEFLAGS"

static char 	  	*progName;  	/* Our invocation name */
static Boolean	  	lockSet;    	/* TRUE if we set the lock file */
Lst			create;	    	/* Targets to be made */
time_t			now;	    	/* Time at start of make */
GNode			*DEFAULT;   	/* .DEFAULT node */
Boolean	    	    	allPrecious;	/* .PRECIOUS given on line by itself */
#ifdef sgi
int			pmake_depth;	/* invocation depth */
#endif

static int              printGraph;	/* -p flag */
static Boolean          noBuiltins;	/* -r flag */
static Boolean	  	noLocking;      /* -l flag */
static Lst  	    	makefiles;  	/* List of makefiles to read (in
					 * order) */
int		    	maxJobs;	/* -J argument */
static int  	  	maxLocal;  	/* -L argument */
Boolean	    	  	debug;	    	/* -d flag */
Boolean	  	  	amMake; 	/* -M flag */
Boolean	    	  	noWarnings; 	/* -W flag */
Boolean	    	    	noExecute;  	/* -n flag */
Boolean	    	    	keepgoing;  	/* -k flag */
Boolean			queryFlag;  	/* -q flag */
Boolean			touchFlag;  	/* -t flag */
Boolean			usePipes;   	/* !-P flag */
Boolean			backwards;  	/* -B flag */
Boolean			ignoreErrors;	/* -i flag */
Boolean			beSilent;   	/* -s flag */
Boolean	    	    	sysVmake;   	/* -v flag */
Boolean			oldVars;    	/* -V flag */
Boolean	    	    	checkEnvFirst;	/* -e flag */
#ifdef sgi
Boolean	    	  	unconditional;	/* -u flag */
#endif
#ifdef GLOBALJOBS
int			maxUserJobs = -1;/* -G argument */
#endif GLOBALJOBS
static Boolean	  	XFlag=FALSE;   	/* -X flag given */
static Boolean	  	xFlag=FALSE;	/* -x flag given */
Boolean	    	  	noExport;   	/* Set TRUE if shouldn't export */
static Boolean	  	jobsRunning;	/* TRUE if the jobs might be running */

static Boolean	    	ReadMakefile();

/*
 * Initial value for optind when parsing args. Different getopts start it
 * differently...
 */
static int  	initOptInd;

#ifdef sgi
#if defined(CAN_EXPORT) && defined(GLOBALJOBS)
#define OPTSTR "BCD:G:I:J:L:MPSVWXd:ef:iklnp:qrstvxhu"
#elif defined(CAN_EXPORT) && !defined(GLOBALJOBS)
#define OPTSTR "BCD:I:J:L:MPSVWXd:ef:iklnp:qrstvxhu"
#elif !defined(CAN_EXPORT) && defined(GLOBALJOBS)
#define OPTSTR "BCD:G:I:J:L:MPSVWd:ef:iklnp:qrstvhu"
#else
#define OPTSTR "BCD:I:J:L:MPSVWd:ef:iklnp:qrstvhu"
#endif
#else
#ifdef CAN_EXPORT
#define OPTSTR "BCD:I:J:L:MPSVWXd:ef:iklnp:qrstvxh"
#else
#define OPTSTR "BCD:I:J:L:MPSVWd:ef:iklnp:qrstvh"
#endif
#endif

static char 	    *help[] = {
"-B	    	Be as backwards-compatible with make as possible without\n\
		being make.",
"-C	    	Cancel any current indications of compatibility.",
"-D<var>	Define the variable <var> with value 1.",
"-I<dir>	Specify another directory in which to search for included\n\
		makefiles.",
"-J<num>	Specify maximum overall concurrency.",
"-L<num>	Specify maximum local concurrency.",
"-M		Be Make as closely as possible.",
"-P		Don't use pipes to catch the output of jobs, use files.",
"-S	    	Turn off the -k flag (see below).",
#ifndef POSIX
"-V		Use old-style variable substitution.",
#endif
"-W		Don't print warning messages.",
#ifdef CAN_EXPORT
"-X		Turn off exporting of commands.",
#endif
"-d<flags>  	Turn on debugging output.",
"-e		Give environment variables precedence over those in the\n\
		makefile(s).",
"-f<file>	Specify a(nother) makefile to read",
"-i		Ignore errors from executed commands.",
"-k		On error, continue working on targets that do not depend on\n\
		the one for which an error was detected.",
#ifdef DONT_LOCK
"-l	    	Turn on locking of the current directory.",
#else
"-l	    	Turn off locking of the current directory.",
#endif
"-n	    	Don't execute commands, just print them.",
"-p<num>    	Tell when to print the input graph: 1 (before processing),\n\
		2 (after processing), or 3 (both).",
"-q	    	See if anything needs to be done. Exits 1 if so.",
"-r	    	Do not read the system makefile for pre-defined rules.",
"-s	    	Don't print commands as they are executed.",
"-t	    	Update targets by \"touching\" them (see touch(1)).",
#ifdef sgi
"-u		Unconditonally remake targets, even if not out-of-date",
#endif
#ifdef GLOBALJOBS
"-G<num>	Specify maximum concurrency for all pmakes for user.",
#endif
"-v	    	Be compatible with System V make. Implies -B, -V and no\n\
		directory locking.",
#ifdef CAN_EXPORT
"-x	    	Allow exportation of commands.",
#endif
"System configuration information:",
SYSPATHDOC,
SHELLDOC,
ALTSYSMKDOC,
SYSMKDOC,
};


/*-
 *----------------------------------------------------------------------
 * MainParseArgs --
 *	Parse a given argument vector. Called from main() and from
 *	Main_ParseArgLine() when the .MAKEFLAGS target is used.
 *
 *	XXX: Deal with command line overriding .MAKEFLAGS in makefile
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Various global and local flags will be set depending on the flags
 *	given
 *----------------------------------------------------------------------
 */
static void
MainParseArgs (argc, argv)
    int		  argc;	      /* Number of arguments in argv */
    char	  **argv;     /* The arguments themselves */
{
    register int  i;
    register char *cp;
    extern int	optind;
    extern char	*optarg;
    char    	c;

    optind = initOptInd;

    while((c = getopt(argc, argv, OPTSTR)) != -1) {
	switch(c) {
	    case 'B':
		backwards = oldVars = TRUE;
		Var_Append(MAKEFLAGS, "-B", VAR_GLOBAL);
		break;
	    case 'C':
		oldVars = backwards = sysVmake = amMake = FALSE;
		Var_Append(MAKEFLAGS, "-C", VAR_GLOBAL);
		break;
	    case 'D':
		Var_Set(optarg, "1", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, "-D", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
		break;
	    case 'I':
		Parse_AddIncludeDir(optarg);
		Var_Append(MAKEFLAGS, "-I", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
		break;
	    case 'J':
		maxJobs = atoi(optarg);
		if (maxJobs <= 0)
			maxJobs = 1;
		Var_Append(MAKEFLAGS, "-J", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
		break;
#ifdef GLOBALJOBS
	    case 'G':
		maxUserJobs = atoi(optarg);
		if (maxUserJobs <= 0)
			maxUserJobs = 1;
		Var_Append(MAKEFLAGS, "-G", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
		break;
#endif
	    case 'L':
		maxLocal = atoi(optarg);
		if (maxLocal <= 0)
			maxLocal = 1;
		Var_Append(MAKEFLAGS, "-L", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
		break;
	    case 'M':
		amMake = TRUE;
		Var_Append(MAKEFLAGS, "-M", VAR_GLOBAL);
		break;
	    case 'P':
		usePipes = FALSE;
		Var_Append(MAKEFLAGS, "-P", VAR_GLOBAL);
		break;
	    case 'S':
		keepgoing = FALSE;
		Var_Append(MAKEFLAGS, "-S", VAR_GLOBAL);
		break;
	    case 'V':
		oldVars = TRUE;
		Var_Append(MAKEFLAGS, "-V", VAR_GLOBAL);
		break;
	    case 'W':
		noWarnings = TRUE;
		Var_Append(MAKEFLAGS, "-W", VAR_GLOBAL);
		break;
	    case 'X':
		XFlag = TRUE;
		Var_Append(MAKEFLAGS, "-X", VAR_GLOBAL);
		break;
	    case 'd':
	    {
		char	*modules = optarg;

		while (*modules) {
		    switch (*modules) {
			case 's':
			    debug |= DEBUG_SUFF;
			    break;
			case 'm':
			    debug |= DEBUG_MAKE;
			    break;
			case 'j':
			    debug |= DEBUG_JOB;
			    break;
			case 't':
			    debug |= DEBUG_TARG;
			    break;
			case 'd':
			    debug |= DEBUG_DIR;
			    break;
			case 'v':
			    debug |= DEBUG_VAR;
			    break;
			case 'c':
			    debug |= DEBUG_COND;
			    break;
			case 'p':
			    debug |= DEBUG_PARSE;
			    break;
			case 'r':
			    debug |= DEBUG_RMT;
			    break;
			case 'a':
			    debug |= DEBUG_ARCH;
			    break;
#ifdef sgi	/* reno */
			case 'A':
#endif
			case '*':
			    debug = ~0;
			    break;
		    }
		    modules++;
		}
		Var_Append(MAKEFLAGS, "-d", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
		break;
	    }
	    case 'e':
		checkEnvFirst = TRUE;
		Var_Append(MAKEFLAGS, "-e", VAR_GLOBAL);
		break;
	    case 'f':
		(void)Lst_AtEnd(makefiles, (ClientData)optarg);
		break;
	    case 'i':
		ignoreErrors = TRUE;
		Var_Append(MAKEFLAGS, "-i", VAR_GLOBAL);
		break;
	    case 'k':
		keepgoing = TRUE;
		Var_Append(MAKEFLAGS, "-k", VAR_GLOBAL);
		break;
	    case 'l':
#ifdef DONT_LOCK
		noLocking = FALSE;
#else
		noLocking = TRUE;
#endif
		Var_Append(MAKEFLAGS, "-l", VAR_GLOBAL);
		break;
	    case 'n':
		noExecute = TRUE;
		Var_Append(MAKEFLAGS, "-n", VAR_GLOBAL);
#if sgi
		/* POSIX.2 */
		allPrecious = TRUE;
#endif
		break;
	    case 'p':
		printGraph = atoi(optarg);
#if sgi
		/* POSIX.2 */
		allPrecious = TRUE;
#endif
		break;
	    case 'q':
		queryFlag = TRUE;
		Var_Append(MAKEFLAGS, "-q", VAR_GLOBAL); /* Kind of
							  * nonsensical, wot?
							  */
#if sgi
		/* POSIX.2 */
		allPrecious = TRUE;
#endif
		break;
	    case 'r':
		noBuiltins = TRUE;
		Var_Append(MAKEFLAGS, "-r", VAR_GLOBAL);
		break;
	    case 's':
		beSilent = TRUE;
		Var_Append(MAKEFLAGS, "-s", VAR_GLOBAL);
		break;
	    case 't':
		touchFlag = TRUE;
		Var_Append(MAKEFLAGS, "-t", VAR_GLOBAL);
		break;
#ifdef sgi
	    case 'u':
		unconditional = TRUE;
		Var_Append (MAKEFLAGS, "-u", VAR_GLOBAL);
		break;
#endif
	    case 'v':
		sysVmake = oldVars = backwards = noLocking = TRUE;
		Var_Append(MAKEFLAGS, "-v", VAR_GLOBAL);
		break;
	    case 'x':
		xFlag = TRUE;
		Var_Append(MAKEFLAGS, "-x", VAR_GLOBAL);
		break;
	    case 'h':
	    case '?':
	    {
		int 	i;

		for (i = 0; i < sizeof(help)/sizeof(help[0]); i++) {
		    printf("%s\n", help[i]);
		}
		exit(c == '?' ? -1 : 0);
	    }
	}
    }

    /*
     * Take care of encompassing compatibility levels...
     */
    if (amMake) {
	backwards = TRUE;
    }
    if (backwards) {
	oldVars = TRUE;
    }

    /*
     * See if the rest of the arguments are variable assignments and perform
     * them if so. Else take them to be targets and stuff them on the end
     * of the "create" list.
     */
    for (i = optind; i < argc; i++) {
	if (Parse_IsVar (argv[i])) {
	    Parse_DoVar(argv[i], VAR_CMD);
	} else {
	    if (argv[i][0] == 0) {
		Punt("Bogus argument in MainParseArgs");
	    }
	    (void)Lst_AtEnd (create, (ClientData)argv[i]);
	}
    }
}

/*-
 *----------------------------------------------------------------------
 * Main_ParseArgLine --
 *  	Used by the parse module when a .MFLAGS or .MAKEFLAGS target
 *	is encountered and by main() when reading the .MAKEFLAGS envariable.
 *	Takes a line of arguments and breaks it into its
 * 	component words and passes those words and the number of them to the
 *	MainParseArgs function.
 *	The line should have all its leading whitespace removed.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Only those that come from the various arguments.
 *-----------------------------------------------------------------------
 */
void
Main_ParseArgLine (line)
    char    	  *line;      /* Line to fracture */
{
    char    	  **argv;     /* Manufactured argument vector */
    int     	  argc;	      /* Number of arguments in argv */

    if (line == NULL) return;
    while (*line == ' ') line++;

    argv = Str_BreakString (line, " \t", "\n", &argc);

    MainParseArgs(argc, argv);

    Str_FreeVec(argc, argv);
}
#ifdef sgi

/*-
 *----------------------------------------------------------------------
 * Main_ParseOldFlags --
 *  	Parse old make's MAKEFILE options that have the same semantics and
 *	create an argument vector suitable for MainParseArgs.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Only those that come from the various arguments.
 *-----------------------------------------------------------------------
 */
void
Main_ParseOldFlags (line)
    char    	  *line;	/* Line to fracture */
{
    char	**argv;		/* Manufactured argument vector */
    int		argc;		/* Number of arguments in argv */
    int		len;

    if (line == NULL)
	return;
    while (*line == ' ')
	line++;

    /*
     * The line contains a concatenation of the options chars,
     * with no whitespace and no leading '-', e.g.,  "bik"
     */
    len = strlen(line);

    argv = (char **) malloc((len+1) * sizeof(char *));
    argc = 0;

    while (--len >= 0) {
	switch (line[len]) {
	    case 'i':
	    case 'k':
	    case 'n':
	    case 'q':
	    case 'r':
	    case 's':
	    case 't':
	    case 'u':
		argv[++argc] = Str_New("-X");
		argv[argc][1] = line[len];
		break;
	}
    }

    if (argc > 0) {
	argc++;		/* count argv[0] */
	argv[0] = line;	/* dummy value */
	MainParseArgs(argc, argv);
	while (--argc > 0) {	/* don't free argv[0] */
	    free (argv[argc]);
	}
    }
    free(argv);
}
#endif

/*-
 *-----------------------------------------------------------------------
 * MainUnlock --
 *	Unlock the current directory. Called as an ExitHandler.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The locking file LOCKFILE is removed.
 *
 *-----------------------------------------------------------------------
 */
static void
MainUnlock ()
{
    (void)unlink (LOCKFILE);
}

/*-
 *----------------------------------------------------------------------
 * main --
 *	The main function, for obvious reasons. Initializes variables
 *	and a few modules, then parses the arguments give it in the
 *	environment and on the command line. Reads the system makefile
 *	followed by either Makefile, makefile or the file given by the
 *	-f argument. Sets the .MAKEFLAGS PMake variable based on all the
 *	flags it has received by then uses either the Make or the Compat
 *	module to create the initial list of targets.
 *
 * Results:
 *	If -q was given, exits -1 if anything was out-of-date. Else it exits
 *	0.
 *
 * Side Effects:
 *	The program exits when done. Targets are created. etc. etc. etc.
 *
 *----------------------------------------------------------------------
 */
main (argc, argv)
    int           argc;
    char          **argv;
{
    Lst             targs;     	/* list of target nodes to create. Passed to
				 * Make_Init */
    Boolean         outOfDate; 	/* FALSE if all targets up to date */
    char    	    *cp;
    extern int	    optind;


    (void)setlocale(LC_ALL, "");

#if defined(sgi) && defined(MALLOPT)
    mallopt(M_BLKSZ, 64*1024);
#endif
    create = Lst_Init (FALSE);
    makefiles = Lst_Init(FALSE);

    beSilent = FALSE;	      	/* Print commands as executed */
    ignoreErrors = FALSE;     	/* Pay attention to non-zero returns */
    noExecute = FALSE;	      	/* Execute all commands */
    keepgoing = FALSE;	      	/* Stop on error */
    allPrecious = FALSE;      	/* Remove targets when interrupted */
    queryFlag = FALSE;	      	/* This is not just a check-run */
    noBuiltins = FALSE;	      	/* Read the built-in rules */
    touchFlag = FALSE;	      	/* Actually update targets */
    usePipes = TRUE;	      	/* Catch child output in pipes */
#ifndef DONT_LOCK
    noLocking = FALSE;	      	/* Lock the current directory against other
				 * pmakes */
#else
    noLocking = TRUE;
#endif /* DONT_LOCK */
    debug = 0;	      	    	/* No debug verbosity, please. */
    noWarnings = FALSE;	    	/* Print warning messages */
    sysVmake = FALSE;	    	/* Don't be System V compatible */

    jobsRunning = FALSE;

    maxJobs = DEFMAXJOBS;     	/* Set the default maximum concurrency */
    maxLocal = DEFMAXLOCAL;   	/* Set the default local max concurrency */
#ifdef sgi
    /* Multiprocessors can handle more concurrency */
    if (sysmp(MP_NAPROCS) > 1) {
	maxJobs  *= 2;
	maxLocal *= 2;
    }
#endif
#ifdef GLOBALJOBS
    {
    char *depthstr;
    static char depths[17];

    if ((depthstr = getenv("PMAKE_DEPTH")) != NULL) {
	pmake_depth = atoi(&depthstr[0]);
	pmake_depth++;
    }
    sprintf(depths, "PMAKE_DEPTH=%04d", pmake_depth);
    putenv(depths);
    }
#endif
    
    /*
     * Deal with disagreement between different getopt's as to what
     * the initial value of optind should be by simply saving the
     * damn thing.
     */
    initOptInd = optind;
    
    /*
     * See what the user calls us. If s/he calls us (yuck) "make", then
     * act like it. Otherwise act like our normal, cheerful self.
     */
    cp = rindex (argv[0], '/');
    if (cp != (char *)NULL) {
	cp += 1;
    } else {
	cp = argv[0];
    }
    progName = cp;

    if (strcmp (cp, "make") == 0) {
	amMake = TRUE;	      	/* Be like make */
	backwards = TRUE;     	/* Do things the old-fashioned way */
	oldVars = TRUE;	      	/* Same with variables */
#ifdef sgi
    } else if (strncmp(cp, "smake", 5) == 0 || strncmp(cp, "vmake", 5) == 0) {
#else
    } else if (strcmp(cp, "smake") == 0 || strcmp(cp, "vmake") == 0) {
#endif
	sysVmake = oldVars = backwards = noLocking = TRUE;
    } else {
	amMake = FALSE;
	backwards = FALSE;    	/* Do things MY way, not MAKE's */
#ifdef DEF_OLD_VARS
	oldVars = TRUE;
#else
	oldVars = FALSE;      	/* don't substitute for undefined variables */
#endif
    }

    /*
     * Initialize the parsing, directory and variable modules to prepare
     * for the reading of inclusion paths and variable settings on the
     * command line 
     */
    Dir_Init ();		/* Initialize directory structures so -I flags
				 * can be processed correctly */
#ifdef sgi
    Var_Init ();		/* As well as the lists of variables for
				 * parsing arguments */
    Parse_Init ();		/* Need to initialize the paths of #include
				 * directories */
#else
    Parse_Init ();		/* Need to initialize the paths of #include
				 * directories */
    Var_Init ();		/* As well as the lists of variables for
				 * parsing arguments */
#endif

    /*
     * Initialize various variables.
     *	.PMAKE gets how we were executed.
     *	MAKE also gets this name, for compatibility
     *	.MAKEFLAGS gets set to the empty string just in case.
     *  MFLAGS also gets initialized empty, for compatibility.
     */
    Var_Set (".PMAKE", argv[0], VAR_GLOBAL);
    Var_Set ("MAKE", argv[0], VAR_GLOBAL);
    Var_Set (MAKEFLAGS, "", VAR_GLOBAL);
    Var_Set ("MFLAGS", "", VAR_GLOBAL);

    /*
     * For POSIX - define a SHELL macro - we define it in GLOBAL space
     * so that it supecedes any environment variable SHELL value but
     * doesn't get re-exported.
     * Special handling is done for command line setting of SHELL
     */
    Var_Set ("SHELL", "/bin/sh", VAR_GLOBAL);

    /*
     * First snag any flags out of the PMAKE environment variable.
     * (Note this is *not* MAKEFLAGS since /bin/make uses that and it's in
     * a different format).
     */
#ifdef sgi
    /* Interpret any of old make's options that make sense */
    Main_ParseOldFlags(getenv("MAKEFLAGS"));
#endif
#ifdef POSIX
    Main_ParseArgLine(getenv("MAKEFLAGS"));
#else
    Main_ParseArgLine (getenv("PMAKE"));
#endif
    
    MainParseArgs (argc, argv);

    /*
     * If the user didn't tell us not to lock the directory, attempt to create
     * the lock file. Complain if we can't, otherwise set up an exit handler
     * to remove the lock file...
     */
    if (!noLocking) {
	int	  	oldMask;    /* Previous signal mask */
	int	  	lockID;     /* Stream ID of opened lock file */
	
#ifndef SYSV
	oldMask = sigblock(sigmask(SIGINT));
#else
	oldMask = sighold(SIGINT);
#endif
	
	lockID = open (LOCKFILE, O_CREAT | O_EXCL, 0666);
	if (lockID < 0 && errno == EEXIST) {
	    /*
	     * Find out who owns the file. If the user who called us
	     * owns it, then we ignore the lock file. Note that we also
	     * do not install an exit handler to remove the file -- if the
	     * lockfile is there from a previous make, it'll still be there
	     * when we leave.
	     */
	    struct stat   fsa;    /* Attributes of the lock file */

	    (void) stat (LOCKFILE,  &fsa);
	    if (fsa.st_uid == getuid()) {
		Error ("Lockfile owned by you -- ignoring it");
		lockSet = FALSE;
	    } else {
		char  	lsCmd[40];
		(void)sprintf (lsCmd, "ls -l %s", LOCKFILE);
		(void)system(lsCmd);
		Fatal ("This directory is already locked (%s exists)",
		       LOCKFILE);
	    }
	} else if (lockID < 0) {
	    Fatal ("Could not create lock file %s", LOCKFILE);
	} else {
#ifndef sgi
	    extern exit();
#endif

	    lockSet = TRUE;
#ifdef sun
	    on_exit(MainUnlock);
#endif
	    /* (void) Proc_SetExitHandler (MainUnlock, (ClientData)0); */
	    signal(SIGINT, exit);
	    (void)close (lockID);
	}
	
#ifndef SYSV
	(void) sigsetmask(oldMask);
#else 
	(void) sigrelse(SIGINT);
#endif
    }

    /*
     * Initialize archive, target and suffix modules in preparation for
     * parsing the makefile(s) 
     */
    Arch_Init ();
    Targ_Init ();
    Suff_Init ();

    DEFAULT = NILGNODE;

    now = time(0);

    /*
     * Set up the .TARGETS variable to contain the list of targets to be
     * created. If none specified, make the variable empty -- the parser
     * will fill the thing in with the default or .MAIN target.
     */
    if (!Lst_IsEmpty(create)) {
	LstNode	ln;

	for (ln = Lst_First(create); ln != NILLNODE; ln = Lst_Succ(ln)) {
	    char    *name = (char *)Lst_Datum(ln);

	    Var_Append(".TARGETS", name, VAR_GLOBAL);
	}
    } else {
	Var_Set(".TARGETS", "", VAR_GLOBAL);
    }

    /*
     * Read in the built-in rules first, followed by the specified makefile,
     * if it was (makefile != (char *) NULL), or the default Makefile and
     * makefile, in that order, if it wasn't. 
     */
#ifdef sgi
    {
    static char defsysmk[] = ALTDEFSYSMK;
    char *sysmk;
    sysmk = Var_Subst (defsysmk, VAR_CMD, FALSE);
    if (!noBuiltins && !ReadMakefile (sysmk) && !ReadMakefile(DEFSYSMK)) {
	Fatal ("Could not open system rules (%s or %s)", sysmk, DEFSYSMK);
    }
    free((Address)sysmk);
    }
#else
    if (!noBuiltins && !ReadMakefile (DEFSYSMK)) {
	Fatal ("Could not open system rules (%s)", DEFSYSMK);
    }
#endif

    if (!Lst_IsEmpty(makefiles)) {
	LstNode	ln = Lst_Find(makefiles, (ClientData)NULL, ReadMakefile);

	if (ln != NILLNODE) {
	    Fatal ("Cannot open %s", (char *)Lst_Datum(ln));
	}
    } else {
#ifdef POSIX
	if (!ReadMakefile("makefile")) {
	    (void)ReadMakefile("Makefile");
	}
#else
	if (!ReadMakefile ((amMake || sysVmake) ? "makefile" : "Makefile")) {
	    (void) ReadMakefile ((amMake||sysVmake) ? "Makefile" : "makefile");
	}
#endif
    }

    /*
     * Figure "noExport" out based on the current mode. Since exporting each
     * command in make mode is rather inefficient, we only export if the -x
     * flag was given. In regular mode though, we only refuse to export if
     * -X was given. In case the operative flag was given in the environment,
     * however, the opposite one may be given on the command line and cancel
     * the action.
     */
    if (amMake) {
	noExport = !xFlag || XFlag;
    } else {
	noExport = XFlag && !xFlag;
    }
    
    Var_Append ("MFLAGS", Var_Value(MAKEFLAGS, VAR_GLOBAL), VAR_GLOBAL);

    /*
     * Install all the flags into the PMAKE envariable.
     */
#ifdef POSIX
    setenv("MAKEFLAGS", Var_Value(MAKEFLAGS, VAR_GLOBAL));
#else
    setenv("PMAKE", Var_Value(MAKEFLAGS, VAR_GLOBAL));
#endif

    /*
     * For compatibility, look at the directories in the VPATH variable
     * and add them to the search path, if the variable is defined. The
     * variable's value is in the same format as the PATH envariable, i.e.
     * <directory>:<directory>:<directory>...
     */
    if (Var_Exists ("VPATH", VAR_CMD)) {
	char	  *vpath;
	char	  *path;
	char  	  *cp;
	char  	  savec;
	static char VPATH[] = "${VPATH}";   /* GCC stores string constants in
					     * read-only memory, but Var_Subst
					     * will want to write this thing,
					     * so store it in an array */
	
	vpath = Var_Subst (VPATH, VAR_CMD, FALSE);

	path = vpath;
	do {
	    /*
	     * Skip to end of directory
	     */
	    for (cp = path; *cp != ':' && *cp != '\0'; cp++) {
		continue;
	    }
	    /*
	     * Save terminator character to figure out when to stop
	     */
	    savec = *cp;
	    *cp = '\0';
	    /*
	     * Add directory to search path
	     */
	    Dir_AddDir (dirSearchPath, path);
	    *cp = savec;
	    path = cp + 1;
	} while (savec == ':');
	free((Address)vpath);
    }
	    
    /*
     * Now that all search paths have been read for suffixes et al, it's
     * time to add the default search path to their lists...
     */
    Suff_DoPaths();

    /*
     * Print the initial graph, if the user requested it
     */
    if (printGraph & 1) {
	Targ_PrintGraph (1);
    }

    Rmt_Init();

    /*
     * Have now read the entire graph and need to make a list of targets to
     * create. If none was given on the command line, we consult the parsing
     * module to find the main target(s) to create.
     */
    if (Lst_IsEmpty (create)) {
	targs = Parse_MainName ();
    } else {
	targs = Targ_FindList (create, TARG_CREATE);
    }

    if (!amMake) {
	/*
	 * Initialize job module before traversing the graph, now that any
	 * .BEGIN and .END targets have been read. This is done only if the
	 * -q flag wasn't given (to prevent the .BEGIN from being executed
	 * should it exist).
	 */
	if (!queryFlag) {
	    if (maxLocal == -1) {
		maxLocal = maxJobs;
	    }
#ifdef sgi
	    /* Job_Init starts running things (.BEGINs) */
	    jobsRunning = TRUE;
#endif
	    Job_Init (maxJobs, maxLocal);
#ifndef sgi
	    jobsRunning = TRUE;
#endif
	}
	
	/*
	 * Traverse the graph, checking on all the targets 
	 */
	outOfDate = Make_Run (targs);
    } else {
	/*
	 * Compat_Init will take care of creating all the targets as well
	 * as initializing the module.
	 */
	Compat_Run(targs);
    }
#ifdef GLOBALJOBS
    Job_exitglobal();
#endif
    
    /*
     * Print the graph now it's been processed if the user requested it
     */
    if (printGraph & 2) {
	Targ_PrintGraph (2);
    }

    if (queryFlag && outOfDate) {
	exit (1);
    } else {
	exit (0);
    }
}

/*-
 *-----------------------------------------------------------------------
 * ReadMakefile  --
 *	Open and parse the given makefile.
 *
 * Results:
 *	TRUE if ok. FALSE if couldn't open file.
 *
 * Side Effects:
 *	lots
 *-----------------------------------------------------------------------
 */
static Boolean
ReadMakefile (fname)
    char          *fname;     /* makefile to read */
{
    if (strcmp (fname, "-") == 0) {
	Parse_File ("(stdin)", stdin);
	Var_Set("MAKEFILE", "", VAR_GLOBAL);
	return (TRUE);
    } else {
	FILE *	  stream;
	extern Lst parseIncPath, sysIncPath;
	
	stream = fopen (fname, "r");
    
	if (stream == (FILE *) NULL) {
	    /*
	     * Look in -I directories...
	     */
	    char    *name = Dir_FindFile(fname, parseIncPath);

	    if (name == NULL) {
		/*
		 * Last-ditch: look in system include directories.
		 */
		name = Dir_FindFile(fname, sysIncPath);
		if (name == NULL) {
		    return (FALSE);
		}
	    }
	    stream = fopen(name, "r");
	    if (stream == (FILE *)NULL) {
		/* Better safe than sorry... */
		return(FALSE);
	    }
	    fname = name;
	}
	/*
	 * Set the MAKEFILE variable desired by System V fans -- the placement
	 * of the setting here means it gets set to the last makefile
	 * specified, as it is set by SysV make...
	 */
	Var_Set("MAKEFILE", fname, VAR_GLOBAL);
	Parse_File (fname, stream);
	fclose (stream);
	return (TRUE);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Error --
 *	Print an error message given its format and 0, 1, 2 or 3 arguments.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The message is printed.
 *
 *-----------------------------------------------------------------------
 */
/*VARARGS1*/
void
Error (fmt, arg1, arg2, arg3)
    char    	  *fmt;	    	    /* Format string */
    int	    	  arg1,	    	    /* First optional argument */
		  arg2,	    	    /* Second optional argument */
		  arg3;	    	    /* Third optional argument */
{
    static char   estr[BSIZE];	    /* output string */

#ifdef sgi
    fflush(stdout);	/* Flush debug output */
#endif
    sprintf (estr, "%s: Error: ", Var_Value(".PMAKE", VAR_GLOBAL));
    sprintf (&estr[strlen (estr)], fmt, arg1, arg2, arg3);
    (void) strcat (estr, "\n");

    fputs (estr, stderr);
    fflush (stderr);
}

/*-
 *-----------------------------------------------------------------------
 * Fatal --
 *	Produce a Fatal error message. If jobs are running, waits for them
 *	to finish.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The program exits
 *-----------------------------------------------------------------------
 */
/* VARARGS1 */
void
Fatal (fmt, arg1, arg2)
    char          *fmt;	      	  /* format string */
    int           arg1;	      	  /* first optional argument */
    int           arg2;	      	  /* second optional argument */
{
    if (jobsRunning) {
	Job_Wait();
    }
    
    Error (fmt, arg1, arg2);

    if (printGraph & 2) {
	Targ_PrintGraph(2);
    }
#ifdef GLOBALJOBS
    Job_exitglobal();
#endif
    exit (2);			/* Not 1 so -q can distinguish error */
}

/*
 *-----------------------------------------------------------------------
 * Punt --
 *	Major exception once jobs are being created. Kills all jobs, prints
 *	a message and exits.
 *
 * Results:
 *	None 
 *
 * Side Effects:
 *	All children are killed indiscriminately and the program Lib_Exits
 *-----------------------------------------------------------------------
 */
/* VARARGS1 */
void
Punt (fmt, arg1, arg2)
    char          *fmt;	/* format string */
    int           arg1;	/* optional argument */
    int	    	  arg2;	/* optional second argument */
{
    Error (fmt, arg1, arg2);

    DieHorribly();
}

/*-
 *-----------------------------------------------------------------------
 * DieHorribly --
 *	Exit without giving a message.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A big one...
 *-----------------------------------------------------------------------
 */
void
DieHorribly()
{
    if (jobsRunning) {
	Job_AbortAll ();
    }
    if (printGraph & 2) {
	Targ_PrintGraph(2);
    }
    
#ifdef GLOBALJOBS
    Job_exitglobal();
#endif
    exit (2);			/* Not 1, so -q can distinguish error */
}

#ifdef sgi
void
DieReallyHorribly()
{
    if (jobsRunning) {
	Job_AbortAll ();
    }
    if (printGraph & 2) {
	Targ_PrintGraph(2);
    }
    
    abort();
}
#endif

/*
 *-----------------------------------------------------------------------
 * Finish --
 *	Called when aborting due to errors in child shell to signal
 *	abnormal exit. 
 *
 * Results:
 *	None 
 *
 * Side Effects:
 *	The program exits
 * -----------------------------------------------------------------------
 */
void
Finish (errors)
    int             errors;	/* number of errors encountered in Make_Make */
{
#ifdef sgi
    Fatal ("%d error%s", errors, (__psint_t)(errors == 1 ? "" : "s"));
#else
    Fatal ("%d error%s", errors, errors == 1 ? "" : "s");
#endif
}

#if defined(SYSV) || !defined(sun)

#ifdef sgi
void
#endif
exit(int status)
{
    if (lockSet) {
	MainUnlock();
    }
#ifdef sgi
    _exithandle();
#endif
    _cleanup();
    _exit(status);
}
#endif /* System V || !sun */
