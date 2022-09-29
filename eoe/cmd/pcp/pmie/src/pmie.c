/***********************************************************************
 * pmie.c - performance inference engine
 ***********************************************************************
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: pmie.c,v 1.1 1999/04/28 10:06:17 kenmcd Exp $"

/*
 * pmie debug flags:
 *	APPL0	- lexical scanning
 *	APPL1	- parse/expression tree construction
 *	APPL2	- expression execution
 */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include "dstruct.h"
#include "syntax.h"
#include "pragmatics.h"
#include "eval.h"
#include "show.h"
#include "pmapi_dev.h"

#if HAVE_TRACE_BACK_STACK
#define MAX_PCS 30	/* max callback procedure depth */
#define MAX_SIZE 48	/* max function name length     */
#include <libexc.h>
#endif


/***********************************************************************
 * constants
 ***********************************************************************/

#define LINE_LENGTH	255		/* max length of command token */
#define PROC_FNAMESIZE  20              /* from proc pmda - proc.h */
#define PMIE_PATHSIZE   (sizeof(PMIE_DIR)+PROC_FNAMESIZE)

static char *prompt = "pmie> ";
static char *intro  = "Performance Co-Pilot Inference Engine (pmie), "
		      "Version %s\n\n%s%s";
static char *cpath  = "/var/pcp/config/pmie/";

static char logfile[MAXPATHLEN+1];
static char perffile[PMIE_PATHSIZE];	/* /var/tmp/<pid> file name */

static char usage[] =
    "Usage: pmie [options] [filename ...]\n\n"
    "Options:\n"
    "  -A align     align sample times on natural boundaries\n"
    "  -a archive   metrics source is a PCP log archive\n"
    "  -b           one line buffered output stream, stdout on stderr\n"
    "  -C           parse configuration and exit\n"
    "  -c filename  configuration file\n"
    "  -d           interactive debugging mode\n"
    "  -f           run in foreground\n"
    "  -h host      metrics source is PMCD on host\n"
    "  -l logfile   send status and error messages to logfile\n"
    "  -n pmnsfile  use an alternative PMNS\n"
    "  -O offset    initial offset into the time window\n"
    "  -S starttime start of the time window\n"
    "  -T endtime   end of the time window\n"
    "  -t interval  sample interval [default 10 seconds]\n"
    "  -V           verbose mode, annotated expression values printed\n"
    "  -v           verbose mode, expression values printed\n"
    "  -W           verbose mode, satisfying expression values printed\n"
    "  -x           run in domain agent mode (summary PMDA)\n"
    "  -Z timezone  set reporting timezone\n"
    "  -z           set reporting timezone to local time of metrics source\n";

static char menu[] =
"pmie debugger commands\n\n"
"  f [file-name]      - load expressions from given file or stdin\n"
"  l [expr-name]      - list named expression or all expressions\n"
"  r [interval]       - run for given or default interval\n"
"  S time-spec        - set start time for run\n"
"  T time-spec        - set default interval for run command\n"
"  v [expr-name]      - print subexpression used for %h, %i and\n"
"                       %v bindings\n"
"  h or ?             - print this menu of commands\n"
"  q                  - quit\n\n";


/***********************************************************************
 * interactive commands
 ***********************************************************************/

/* read command input line */
static int
readLine(char *bfr, int max)
{
    int		c, i;

    /* skip blanks */
    do
	c = getchar();
    while (isspace(c));

    /* scan till end of line */
    i = 0;
    while ((c != '\n') && (c != EOF) && (i < max)) {
	bfr[i++] = c;
	c = getchar();
    }
    bfr[i] = '\0';
    return (c != EOF);
}


/* scan interactive command token */
static char *
scanCmd(char **pp)
{
    char	*p = *pp;

    /* skip blanks */
    while (isspace(*p))
	p++;

    /* single char token */
    if (isgraph(*p)) {
	*pp = p + 1;
	return p;
    }

    return NULL;
}


/* scan interactive command argument */
static char *
scanArg(char *p)
{
    char	*q;

    /* strip leading blanks */
    while (isspace(*p))
	p++;
    if (*p == '\0')
	return NULL;
    q = p;

    /* strip trailing blanks */
    while (*q != '\0')
	q++;
    q--;
    while (isspace(*q))
	q--;
    *(q + 1) = '\0';

    /* return result */
    return p;
}


/* load rules from given file or stdin */
static void
load(char *fname)
{
    Symbol	s;
    Expr	*d;
    int		sts;
    char	config[MAXPATHLEN+1];

    /* search for configfile on configuration file path */
    if (fname && access(fname, F_OK) != 0) {
	sts = oserror();	/* always report the first error */
	if (fname[0] == '/') {
	    fprintf(stderr, "%s: cannot access config file %s: %s\n", pmProgname, 
		    fname, strerror(sts));
	    exit(1);
	}
#if PCP_DEBUG
	else if (pmDebug & DBG_TRACE_APPL0) {
	    fprintf(stderr, "load: cannot access config file %s: %s\n", fname, strerror(sts));
	}
#endif
	sprintf(config, "%s%s", cpath, fname);
	if (access(config, F_OK) != 0) {
	    fprintf(stderr, "%s: cannot access config file as either %s or %s: %s\n",
		    pmProgname, fname, config, strerror(sts));
	    exit(1);
	}
#if PCP_DEBUG
	else if (pmDebug & DBG_TRACE_APPL0) {
	    fprintf(stderr, "load: using standard config file %s%s\n", cpath, fname);
	}
#endif
	fname = config;
    }
#if PCP_DEBUG
    else if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "load: using config file %s\n",
		fname == NULL? "<stdin>":fname);
    }
#endif

    if (perf->config[0] == '\0') {	/* keep record of first config */
	if (fname == NULL)
	    strcpy(perf->config, "<stdin>");
	else if (realpath(fname, perf->config) == NULL) {
	    fprintf(stderr, "%s: failed to resolve realpath for %s: %s\n",
		    pmProgname, fname, strerror(sts));
	    exit(1);
	}
    }

    if (synInit(fname)) {
	while ((s = syntax()) != NULL) {
	    d = (Expr *) symValue(symDelta);
	    pragmatics(s, *(RealTime *)d->smpls[0].ptr);
	}
    }
}


/* list given expression or all expressions */
static void
list(char *name)
{
    Task	*t;
    Symbol	*r;
    Symbol	s;
    int		i;

    if (name) {	/* single named rule */
	if (s = symLookup(&rules, name))
	    showSyntax(stdout, s);
	else
	    printf("%s: error - rule \"%s\" not defined\n", pmProgname, name);
    }
    else {	/* all rules */
	t = taskq;
	while (t) {
	    r = t->rules;
	    for (i = 0; i < t->nrules; i++) {
		showSyntax(stdout, *r);
		r++;
	    }
	    t = t->next;
	}
    }
}


/* list binding subexpression of given expression or all expressions */
static void
sublist(char *name)
{
    Task	*t;
    Symbol	*r;
    Symbol	s;
    int		i;

    if (name) {	/* single named rule */
	if (s = symLookup(&rules, name))
	    showSubsyntax(stdout, s);
	else
	    printf("%s: error - rule '%s' not defined\n", pmProgname, name);
    }
    else {	/* all rules */
	t = taskq;
	while (t) {
	    r = t->rules;
	    for (i = 0; i < t->nrules; i++) {
		showSubsyntax(stdout, *r);
		r++;
	    }
	    t = t->next;
	}
    }
}


/***********************************************************************
 * manipulate the performance instrumentation data structure
 ***********************************************************************/

static void
stopmonitor(void)
{
    if (*perffile)
	unlink(perffile);
}

static void
startmonitor(void)
{
    struct hostent	*hep = gethostbyname(dfltHost);
    void		*ptr;
    int			fd;
    char		zero = '\0';

    /* try to create the port file directory. OK if it already exists */
    if ( (mkdir(PMIE_DIR, S_IRWXU | S_IRWXG | S_IRWXO) < 0) &&
	 (oserror() != EEXIST) ) {
	fprintf(stderr, "%s: error creating stats file dir %s: %s\n",
		pmProgname, PMIE_DIR, strerror(oserror()));
	exit(1);
    }

    chmod(PMIE_DIR, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);
    atexit(stopmonitor);

    /* create and initialize mmapped performance data file */
    sprintf(perffile, "%s/%u", PMIE_DIR, getpid());
    if ((fd = open(perffile, O_RDWR | O_CREAT | O_EXCL | O_TRUNC,
			    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
	fprintf(stderr, "%s: cannot create stats file %s: %s\n", pmProgname, perffile,
		strerror(oserror()));
	exit(1);
    }
    /* seek to struct size and write one zero */
    lseek(fd, sizeof(pmiestats_t)-1, SEEK_SET);
    write(fd, &zero, 1);

    /* mmap perffile & associate the instrumentation struct with it */
    if ((ptr = mmap(0, sizeof(pmiestats_t), PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0)) == MAP_FAILED) {
	fprintf(stderr, "%s: mmap failed for stats file %s: %s\n", pmProgname, perffile, strerror(oserror()));
	exit(1);
    }
    close(fd);

    perf = (pmiestats_t *)ptr;
    strcpy(perf->logfile, logfile[0] == '\0'? "<none>" : logfile);
    strcpy(perf->defaultfqdn, hep == NULL? dfltHost : hep->h_name);
    perf->version = 1;
}


/***********************************************************************
 * signal handling
 ***********************************************************************/

/*ARGSUSED*/
static void
sigintproc(int sig)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    __pmNotifyErr(LOG_INFO, "%s caught SIGINT or SIGTERM\n", pmProgname);
    exit(1);
}

/*ARGSUSED*/
static void
sigbye(int sig)
{
    exit(0);
}


static void
dotraceback(void)
{
#if HAVE_TRACE_BACK_STACK
    __uint64_t	call_addr[MAX_PCS];
    char	*call_fn[MAX_PCS];
    char	names[MAX_PCS][MAX_SIZE];
    int		res;
    int		i;

    fprintf(stderr, "\nProcedure call traceback ...\n");
    for (i = 0; i < MAX_PCS; i++)
        call_fn[i] = names[i];
    res = trace_back_stack(MAX_PCS, call_addr, call_fn, MAX_PCS, MAX_SIZE);
    for (i = 1; i < res; i++)
    fprintf(stderr, "  0x%p [%s]\n", (void *)call_addr[i], call_fn[i]);
#endif
    return;
}


static void
sigbadproc(int sig)
{
    __pmNotifyErr(LOG_ERR, "Unexpected signal %d ...\n", sig);
    dotraceback();
    fprintf(stderr, "\nDumping to core ...\n");
    fflush(stderr);
    stopmonitor();
    abort();
}


/***********************************************************************
 * command line processing - extract command line arguments & initialize
 ***********************************************************************/

static void
getargs(int argc, char *argv[])
{
    static char		*nosubs[] = {NULL};
    char		*configfile = NULL;
    char		*commandlog = NULL;
    char		*subopts;
    char		*subopt;
    char		*msg;
    int			checkFlag = 0;
    int			foreground = 0;
    int			err = 0;
    int			sts;
    int			c;
    int			bflag = 0;
    Archive		*a;
    struct timeval	tv, tv1, tv2;
    extern char		*optarg;
    extern int		optind;

    memset(&tv, 0, sizeof(tv));
    memset(&tv1, 0, sizeof(tv1));
    memset(&tv2, 0, sizeof(tv2));
    dstructInit();

    while ((c=getopt(argc, argv, "a:A:bc:CdD:fh:l:n:O:S:t:T:vVWXxzZ:?")) != EOF) {
        switch (c) {

	case 'a':			/* archives */
	    if (dfltConn && dfltConn != PM_CONTEXT_ARCHIVE) {
		fprintf(stderr, "%s: at most one of -a or -h allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    dfltConn = PM_CONTEXT_ARCHIVE;
	    subopts = optarg;
	    while (*subopts != '\0' &&
		   getsubopt(&subopts, nosubs, &subopt) == -1) {
		a = (Archive *) zalloc(sizeof(Archive));
		a->fname = subopt;
		if (!initArchive(a)) {
		    exit(1);
		    /*NOTREACHED*/
		}
	    }
	    foreground = 1;
	    break;

	case 'A': 			/* sample alignment */
	    alignFlag = optarg;
	    break;

	case 'b':			/* line buffered, stdout on stderr */
	    bflag++;
	    break;

	case 'c': 			/* configuration file */
	    if (interactive) {
		fprintf(stderr, "%s: at most one of -c and -d allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    configfile = optarg;
	    break;

	case 'C': 			/* check config and exit */
	    checkFlag = 1;
	    break;

	case 'd': 			/* interactive mode */
	    if (configfile) {
		fprintf(stderr, "%s: at most one of -c and -d allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    interactive = 1;
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification "
			"(%s)\n", pmProgname, optarg);
		err++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'f':			/* in foreground, not as daemon */
	    foreground = 1;
	    break;

	case 'h': 			/* default host name */
	    if (dfltConn) {
		fprintf(stderr, "%s: at most one of -a or -h allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    dfltConn = PM_CONTEXT_HOST;
	    dfltHost = optarg;
	    break;

	case 'l':			/* alternate log file */
	    if (commandlog != NULL) {
		fprintf(stderr, "%s: at most one -l option is allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    commandlog = optarg;
	    isdaemon = 1;
	    break;

        case 'n':			/* alternate namespace file */
	    pmnsfile = optarg;
            break;

	case 'O':			/* position within time window */
	    offsetFlag = optarg;
	    break;

	case 'S':			/* start run time */
	    startFlag = optarg;
	    break;

	case 't':			/* sample interval */
	    if (pmParseInterval(optarg, &tv1, &msg) == 0)
		dfltDelta = realize(tv1);
	    else {
		fprintf(stderr, "%s: could not parse -t argument (%s)\n", pmProgname, optarg);
		fputs(msg, stderr);
		free(msg);
		err++;
	    }
	    break;

	case 'T':			/* evaluation period */
	    stopFlag = optarg;
	    break;

	case 'v': 			/* print values */
	    verbose = 1;
	    break;

	case 'V': 			/* print annotated values */
	    verbose = 2;
	    break;

	case 'W': 			/* print satisfying values */
	    verbose = 3;
	    break;

	case 'X': 			/* secret applet flag */
	    applet = 1;
	    verbose = 1;
	    setlinebuf(stdout);
	    break;

	case 'x': 			/* summary PMDA flag */
	    agent = 1;
	    verbose = 1;
	    isdaemon = 1;
	    break;

	case 'z':			/* timezone from host */
	    hostZone = 1;
	    if (timeZone) {
		fprintf(stderr, "%s: only one of -Z and -z allowed\n",
			pmProgname);
		err++;
	    }
	    break;

	case 'Z':			/* explicit TZ string */
	    timeZone = optarg;
	    if (hostZone) {
		fprintf(stderr, "%s: only one of -Z and -z allowed\n",
			pmProgname);
		err++;
	    }
	    break;

	case '?':
            err++;
	}
    }

    if (configfile && optind != argc) {
	fprintf(stderr, "%s: extra filenames cannot be given after using -c\n",
		pmProgname);
	err++;
    }
    if (err) {
	fprintf(stderr, usage);
	exit(1);
    }

    if (foreground)
	isdaemon = 0;

    if (archives || interactive)
	perf = &instrument;

    if (isdaemon) {			/* daemon mode */
	signal(SIGHUP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGINT, sigintproc);
	signal(SIGTERM, sigintproc);
	signal(SIGBUS, sigbadproc);
	signal(SIGSEGV, sigbadproc);
    }
    else {
	/* need to catch these so the atexit() processing is done */
	signal(SIGINT, sigbye);
	signal(SIGTERM, sigbye);
    }

    if (commandlog != NULL) {
	__pmOpenLog(pmProgname, commandlog, stderr, &sts);
	if (realpath(commandlog, logfile) == NULL) {
	    fprintf(stderr, "%s: cannot find realpath for log %s: %s\n",
		    pmProgname, commandlog, strerror(oserror()));
	    exit(1);
	}
    }

    if (bflag) {
	/*
	 * -b ... force line buffering and stdout onto stderr
	 */
	int	i, j;

	fflush(stderr);
	fflush(stdout);
	setlinebuf(stderr);
	setlinebuf(stdout);
	i = fileno(stdout);
	close(i);
	if ((j = dup(fileno(stderr))) != i)
	    fprintf(stderr, "%s: Warning: failed to link stdout (-b option) "
			    "... dup() returns %d, expected %d\n", pmProgname, j, i);
    }

    if (__pmGetLicense(PM_LIC_MON, pmProgname,
			GET_LICENSE_SHOW_EXP) == PM_LIC_MON ||
	__pmGetLicense(PM_LIC_COL, pmProgname,
			GET_LICENSE_SHOW_EXP) == PM_LIC_COL)
	licensed = 1;

    if (dfltConn == 0) {
	/* default case, no -a or -h */
	dfltConn = PM_CONTEXT_HOST;
	dfltHost = localHost;
    }

    if (!archives && !interactive) {
	if (commandlog != NULL)
	    fprintf(stderr, "pmie: PID = %d, default host = %s\n\n", getpid(), dfltHost);
	startmonitor();
    }

    /* default host from leftmost archive on command line */
    if (archives && dfltHost == localHost) {
	a = archives;
	while (a->next)
	    a = a->next;
	dfltHost = a->hname;
    }

    /* initialize time */
    now = archives ? first : getReal() + 1.0;
    zoneInit();
    reflectTime(dfltDelta);

    /* parse time window - just to check argument syntax */
    unrealize(now, &tv1);
    if (archives)
	unrealize(last, &tv2);
    else
	tv2.tv_sec = INT_MAX;		/* sizeof(time_t) == sizeof(int) */
    if (pmParseTimeWindow(startFlag, stopFlag, alignFlag, offsetFlag,
                          &tv1, &tv2,
                          &tv, &tv2, &tv1,
		          &msg) < 0) {
	fputs(msg, stderr);
        exit(1);
    }
    start = realize(tv1);
    stop = realize(tv2);
    runTime = stop - start;

    /* initialize PMAPI */
    if (pmnsfile != PM_NS_DEFAULT && (sts = pmLoadNameSpace(pmnsfile)) < 0) {
	fprintf(stderr, "%s: pmLoadNameSpace failed: %s\n", pmProgname,
		pmErrStr(sts));
	exit(1);
    }

    if (!interactive && optind == argc) {	/* stdin or config file */
	load(configfile);
    }
    else {					/* list of 1/more filenames */
	while (optind < argc) {
	    load(argv[optind]);
	    optind++;
	}
    }

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	dumpRules();
#endif

    if (checkFlag)
	exit(0);

    if (isdaemon) {			/* daemon mode */
	close(fileno(stdin));		/* ensure stdin closed for daemon */
	setsid();	/* not process group leader, lose controlling tty */
    }

    if (agent)
	agentInit();			/* initialize secret agent stuff */

    /* really parse time window */
    if (!archives) {
	now = getReal() + 1.0;
	reflectTime(dfltDelta);
    }
    unrealize(now, &tv1);
    if (archives)
	unrealize(last, &tv2);
    else
	tv2.tv_sec = INT_MAX;
    if (pmParseTimeWindow(startFlag, stopFlag, alignFlag, offsetFlag,
		          &tv1, &tv2,
                          &tv, &tv2, &tv1,
		          &msg) < 0) {
	fputs(msg, stderr);
	exit(1);
    }

    /* set run timing window */
    start = realize(tv1);
    stop = realize(tv2);
    runTime = stop - start;
}

/***********************************************************************
 * interactive (debugging) mode
 ***********************************************************************/

static void
interact(void)
{
    int			quit = 0;
    char		*line = (char *) zalloc(LINE_LENGTH + 2);
    char		*finger;
    char		*token;
    char		*msg;
    RealTime		rt;
    struct timeval	tv1, tv2;

    printf(intro, PCP_VERSION, menu, prompt);
    fflush(stdout);
    while (!quit && readLine(line, LINE_LENGTH)) {
	finger = line;

	if (token = scanCmd(&finger)) {
	    switch (*token) {

	    case 'f':
		token = scanArg(finger);
		load(token);
		break;

	    case 'l':
		token = scanArg(finger);
		list(token);
		break;

	    case 'r':
		token = scanArg(finger);
		if (token) {
		    if (pmParseInterval(token, &tv1, &msg) == 0)
			runTime = realize(tv1);
		    else {
			fputs(msg, stderr);
			free(msg);
			break;
		    }
		}
		if (!archives) {
		    invalidate();
		    rt = getReal();
		    if (now < rt)
			now = rt;
		    start = now;
		}
		stop = start + runTime;
		run();
		break;

	    case 'S':
		token = scanArg(finger);
		if (token == NULL) {
		    fprintf(stderr, "%s: error - argument required\n", pmProgname);
		    break;
		}
		unrealize(start, &tv1);
		if (archives)
		    unrealize(last, &tv2);
		else
		    tv2.tv_sec = INT_MAX;
		if (__pmParseTime(token, &tv1, &tv2, &tv1, &msg) < 0) {
		    fputs(msg, stderr);
		    free(msg);
		    break;
		}
		start = realize(tv1);
		if (archives)
		    invalidate();
		break;

	    case 'T':
		token = scanArg(finger);
		if (token == NULL) {
		    fprintf(stderr, "%s: error - argument required\n", pmProgname);
		    break;
		}
		if (pmParseInterval(token, &tv1, &msg) < 0) {
		    fputs(msg, stderr);
		    free(msg);
		    break;
		}
		runTime = realize(tv1);
		break;
	    case 'q':
		quit = 1;
		break;

	    case 'v':
		token = scanArg(finger);
		sublist(token);
		break;

	    case '?':
	    default:
		printf("%s", menu);
	    }
	}
	if (!quit) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
    }
}


/***********************************************************************
 * main
 ***********************************************************************/

void
main(int argc, char **argv)
{
    char	*p;

    __pmSetAuthClient();

    /* trim cmd name of leading directory components */
    for (p = pmProgname = argv[0]; *p; p++)
	if (*p == '/')
	    pmProgname = p+1;

    /* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
    if (getenv("PCP_COUNTER_WRAP") != NULL)
	dowrap = 1;

    getargs(argc, argv);

    if (interactive)
	interact();
    else
	run();
    exit(0);
}
