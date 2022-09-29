#include "fx.h"

#ifdef _STANDALONE
# include <saio.h>
# include <saioctl.h>
#endif

#ifndef _STANDALONE
#include <fcntl.h>
#include <signal.h>
#include <time.h>
FILE *logfd = NULL;	/* fd for logging */
#endif /* _STANDALONE */

#ifndef ARCS_SA
#include <stdlib.h>
#endif

/*
 * fx.c --
 * disk formatter/exerciser/labeller, etc for disks.
 *
 * there are 4 main functions:  format, exercise, set up label,
 * and restore.
 *
 * the first task is to determine which drive to use (by prompting).
 * a simple go / nogo controller check can occur at this point.
 *
 * format and exercise functions are straightforward applications
 * based on the drive type info.
 *
 * the label info consists of 5 main parts:  drive parameters,
 * partition info, boot info, bad sector info, and id info (sgilabel).
 *
 * basic operation is interactive, menu-driven.  however, all menu
 * commands accept optional command-line args so the interaction can
 * be short-circuited if desired.  erroneous arguments cause the current
 * function to be aborted.
 *
 * all label (including badblock) info is stored internally until a
 * sync command is given.  if the info has changed during operation,
 * the user prompted to write it out before quitting.
 */


/* ----- fx globals ----- */

struct volume_header vh;					/* current vh block */

/* controller & drive info: set by fx_find_ctlr() which looks at */
/* system hardware. */

char *driver;		/* driver name */
uint drivernum;		/* Internal driver num */
uint ctlrno;		/* controller number */
uint ctlrtype;	/* needed to distinguish 3201 & 4201 */
uint driveno;		/* drive number */
uint partnum;	/* partition to use */
int  scsilun;	/* LUN for SCSI drives */
int isrootdrive;
char fulldrivename[80];
int nomenus;
char *script;
int  device_name_specified;
char device_name[128];


CBLOCK sg;					/* sgi info */
int changed;					/* flag - label changed */
int expert;		/* expert use: allows more dangerous options */
int doing_auto;
int driveopen;	/* if 0, we haven't opened a drive, don't do cleanup */

int formatrequired;	/* has geometry been changed, but no format done? */
extern int _gotsgilab;

int do_overlap_checks = 1;

static int split_devstring(char *, char *, int *, int *, int *);
static void GetDriverSpec(void);

#ifndef _STANDALONE
/*ARGSUSED*/
static void
gothup(int sig)
{
	logmsg("fx terminating on hangup\n");
	exit(1);
}

char *signames[] = {
	"",	/* place holder */
	"HUP",	/* hangup */
	"INT",	/* interrupt (rubout) */
	"QUIT",	/* quit (ASCII FS) */
	"ILL",	/* illegal instruction (not reset when caught)*/
	"TRAP",	/* trace trap (not reset when caught) */
	"IOT",	/* IOT instruction */
	"EMT",	/* EMT instruction */
	"FPE",	/* floating point exception */
	"KILL",	/* kill (cannot be caught or ignored) */
	"BUS",	/* bus error */
	"SEGV",	/* segmentation violation */
	"SYS",	/* bad argument to system call */
	"PIPE",	/* write on a pipe with no one to read it */
	"ALRM",	/* alarm clock */
	"TERM",	/* software termination signal from kill */
	"USR1",	/* user defined signal 1 */
	"USR2",	/* user defined signal 2 */
	"CLD",	/* death of a child */
	"PWR",	/* power-fail restart */
	"STOP",	/* sendable stop signal not from tty */
	"TSTP",	/* stop signal from tty */
	"POLL",	/* pollable event occured */
	"IO",	/* input/output possible signal */
	"URG",	/* urgent condition on IO channel */
	"WINCH",	/* window size changes */
	"VTALRM",	/* virtual time alarm */
	"PROF",	/* profiling alarm */
	"CONT",	/* continue a stopped process */
	"TTIN",	/* to readers pgrp upon background tty read */
	"TTOU",	/* like TTIN for output if (tp->t_local&LTOSTOP) */
};

#define MAX_BAD_SIGS 15

/*ARGSUSED1*/
void
badsig(int signo, int flag, struct sigcontext *cont)
{
	static sigcnt;

	printf("At pc=0x%llx, got unexpected signal ", cont->sc_pc);
	if(signo>0 && signo < NSIG)
		printf("SIG%s\n", signames[signo]);
	else
		printf("# %d\n", signo);
	sigrelse(signo);	/* so we can take signal again */
	if(++sigcnt > MAX_BAD_SIGS) {
		/* in case we are in a loop getting sigsegv, etc */
		printf("too many unexpected signals (%d), bye\n", MAX_BAD_SIGS);
		exit(-1);
	}
	mpop();	/* pop to previous menu so they can continue doing other
		things if they want */
}
#endif /* _STANDALONE */

main(argc, argv)
    int argc; char **argv;
{
	int tmpint;
	char *tmp;

#ifdef ARCS_SA
	updatetv();		/* check for backlevel spb tv */
#endif

    /* Forget remaining arguments of type arg=value because they
     * are extraneous ARCS spec args.  This assumes there are no
     * valid arguments of that type, and that all valid arguments
     * come before the ARCS arguments.
     */
    for (tmpint = 0; tmpint < argc; tmpint++)
	if (argv[tmpint] && index(argv[tmpint], '=')) {
	    argv[tmpint] = 0;
	    argc = tmpint;
	    break;
	}

    /* check for 'expert' switch on cmd line */

    for( ; argc > 1 && argv[1][0] == '-'; argc--,argv++)
		switch(argv[1][1]) {
		case 'C':
			do_overlap_checks = 0;
			break;
		case 'x':
			expert = 1;
			break;
		case 'l':
#ifdef _STANDALONE
			printf("fx: logging not supported in standalone\n");
#else
			if(argv[1][2])
				tmp = &argv[1][2];
			else if(argc>1) {
				argc--;
				argv++;
				tmp = argv[1];
			}
			else
				goto usage;
			if(((tmpint=open(tmp, O_WRONLY|O_CREAT|O_SYNC|O_APPEND, 0664)) == -1)
				|| (logfd=fdopen(tmpint, "w")) == NULL)
				err_fmt("Couldn't open log file %s", tmp);
			else
				tzset();
#endif /* _STANDALONE */
			break;
		case 'r':	/* set max retries for io.c */
			if(argv[1][2])
				tmp = &argv[1][2];
			else if(argc>1) {
				argc--;
				argv++;
				tmp = argv[1];
			}
			else
				goto usage;
			tmpint = atoi(tmp);
			if(tmpint < 0)
				printf("retry count must be >= 0, %s ignored\n", tmp);
			else
				ioretries = tmpint;
			break;

		case 'c':
#ifdef _STANDALONE
			printf("fx: command mode not supported in standalone\n");
#else
			nomenus = 1;
#endif /* _STANDALONE */
			break;

		case 'd':
		{
			int length = sizeof(device_name);
#ifdef _STANDALONE
			printf("fx: ""-d"" option not supported in standalone\n");
			goto usage;
#else
			if (argv[1][2])
				tmp = &argv[1][2];
			else if (argc > 1)
			{
			    argc--;
			    argv++;
			    tmp = argv[1];
			    if (filename_to_devname(tmp, device_name, &length) &&
				strstr(device_name, "volume/char"))
			    {
				device_name_specified = 1;
			    }
			}
			if (!device_name_specified)
				goto usage;
			strcpy(device_name, tmp);
#endif
			break;
		}

		case 's':
#ifdef _STANDALONE
			printf("fx: script not supported in standalone\n");
#else
			if(argv[1][2])
				script = &argv[1][2];
			else if(argc>1) {
				argc--;
				argv++;
				script = argv[1];
			}
			else
				goto usage;
			break;
#endif /* _STANDALONE */

		default:
			printf("fx: Unrecognized option %c\n", argv[1][1]);
			/* Fall through */
		case '?':	/* usage message */
usage:
#ifndef _STANDALONE
			printf("Usage: fx [-x] [-r maxretries] [-l logfile] [-s script]\n\t[-d <device>] [-c] [drivetype[(ctrlr,unit)] ]\n\t[VERIFY] [INITIALIZE] [FORMAT] (with -c)\n");
#else
			printf("Usage: fx [-x] [-r maxretries] [drivetype[(ctrlr,unit)] ]\n");
#endif /* _STANDALONE */
			exit(1);
		}
    *argv = "fx";	/* gross hack to get messages correct */

    if(nomenus && script) {
	errno = 0;
	scerrwarn("The -c or -s options may not be used together\n");
	exit(1);
    }

    if(nomenus || script) {
	if(!expert) {
	    errno = 0;
	    scerrwarn("Must use -x option with -c or -s options\n");
		exit(1);
	}
    }

#ifndef _STANDALONE
    if(script) {
	nomenus = 1;	/* no reads from stdin for -s, same issues as with -c */
	close(0); /* ensure that any attempts to read fail, for anything I miss */
	fxscript(script);
	/* NOTREACHED */
    }
#endif

#ifdef _STANDALONE
    if (!expert)
    {
	printf("Currently in safe read-only mode.\n");
	if (!no("Do you require extended mode with all options available"))
		expert = 1;
    }
#endif

    if(!nomenus) /* see what type of controller is on the */
	fx_find_ctlr();   /* system & set default accordingly */
#ifndef _STANDALONE
    else
	close(0); /* ensure that any attempts to read fail, for anything I miss */
#endif

    for( ;; )
    {
	callsub(main_fx, argc, argv);
	argc = 1;
    }
}


static void
mustgivedisk(void)
{
	scerrwarn("you must specify the disk name and number on the command"
		"line with -c");
	/* NOTREACHED */
}

/*
 * using args if present, open a drive and get its label info.
 *	usage:  fx [devname [drivetype]]
 * first, get args if any.  if a drive type is specified, save it
 * for later.
 *
 * open the device.  do a quick controller check.  
 * Clear out any label info from a previous drive, get new label info.
 * then, we are finally ready to run the main menu.
 */
void
main_fx(void)
{
    STRBUF s;
    extern char *fx_version;

    printf("%s\n", fx_version);

    (void)setintr(-1);
    gclose();
    bzero(&vh, sizeof(vh));
    bzero(&sg, sizeof(sg));
    init_ex();
    init_db();
	driveopen = 0;	/* avoid errors from exit_func() */

    GetDriverSpec();

#ifndef _STANDALONE
	/* after we get the drive name, but before any i/o */
	logmsg("fx starts\n");
	sigset(SIGHUP, gothup);	/* so we can log termination */

	/*	Catch and report other signals; generally divide by 0, or
		dereferencing a pointer that hasn't yet been set.  Better
		than just dumping core, but not as good as fixing the (MANY)
		places in the code where not enough error checking has
		been done.  */
	sigset(SIGTRAP, badsig);
	sigset(SIGIOT, badsig);
	sigset(SIGILL, badsig);
	sigset(SIGFPE, badsig);
	sigset(SIGEMT, badsig);
	sigset(SIGSEGV, badsig);
	sigset(SIGBUS, badsig);
#endif /* _STANDALONE */

    *s.c = '\0';
    if(nomenus) {
	if(!driver || !*driver)
	    mustgivedisk();
    }
    else {
	if(!noargs()) {
	    errwarn("extra arguments after drive name ignored");
	    while(getarg())	/* pop the other args */
		;
	}
    }

   if( gopen(driveno, partnum) < 0 ) {
	mpop();	/* gopen prints message, don't be redundant with argerr()  */
	/*NOT REACHED*/
   }

	driveopen = 1;
    ctlrcheck();

    (void)setintr(0);
    init_label(s.c);

    menuloop_init();

#ifndef _STANDALONE
    if(nomenus) {
	if(noargs())
	    scerrwarn("no arguments specified with -c");
	/* don't use errwarn here, as we don't want the GUI stuff to parse
	 * this out and show it; that tool already has a confirmer. (that's
	 * why WARNING is all caps, also) */
	printf("fx: WARNING: About to perform destructive function to %s(%d,%d), with no\n"
	    "\tconfirmations.  You have 5 seconds to abort with an interrupt\n",
	    driver, ctlrno, driveno);
	(void)setintr(-1);
	flushoutp();
	sleep(5);
	(void)setintr(0);
	while(!noargs()) {
	    char *arg;
	    arg = getarg();
	    if(strcmp(arg, "VERIFY") == 0) {
		majorchange = 1; /* suppress warnings */
		complete_func();
		create_all_func();
		pt_optdrive_func();
		sync_func();
	    }
	    else if(strcmp(arg, "INITIALIZE") == 0) {
		majorchange = 1; /* suppress warnings */
		create_all_func();
		pt_optdrive_func();
		sync_func();
	    }
	    else if(strcmp(arg, "FORMAT") == 0) {
		majorchange = 1; /* suppress warnings */
		format_func();
		create_all_func();
		pt_optdrive_func();
		sync_func();
	    }
	    else 
		scerrwarn("unknown -c argument \"%s\"\n", arg);
	}
	exit(0); /* accumulate errors, and exit with them? */
    }
    else
#endif /* _STANDALONE */
		menuloop();
}



/*
 * Possible drivers that fx can work with.
 */
struct drivers {
	char *driver_name;
	int  driver_num;
	int rootdrive;	/* for isrootdrive setting */
} drivers[] = {
#ifdef SCSI_NAME
	{ SCSI_NAME, SCSI_NUMBER, 1},
#endif
#ifdef SMFD_NAME
	{ SMFD_NAME, SMFD_NUMBER, 0},
#endif
	{ 0, 0},
};



/*
 * Get device type, controller #, and drive #, the input may be
 * either a drive number or a standalone device name.  we prompt for
 * only one of the above however we accept either.
 *
 * this sets the fx globals
 *	driver
 *	ctlrno
 *	driveno
 *	scsilun
 *
 * on entry these globals are assumed to have reasonable values
 * that can be used for defaults when prompting.
 */
static void
GetDriverSpec(void)
{
    static STRBUF savedriver;
    STRBUF s;
    char *arg;
    int d, c, ret;
    STRBUF drbuf;
    struct drivers *driver_ptr;
    uint64_t tmp;

    partnum = PTNUM_VOLUME;	/* each time through; could
	be changed for ARCS, or floppy */

    /* clear some state flags; assumption is that we are
     * switching to a different drive (equivalent to exit) */
    _gotsgilab = formatrequired = changed = majorchange = 0;

    if (device_name_specified) {
	driver = SCSI_NAME;
	driveno = 0;
	ctlrno = 0;
	scsilun = 0;
	return;
    }

    /*
     * if there is an arg, use it the first time
     * through the loop
     */
    arg = 0;
    if( !noargs() )
	arg = getarg();

    for(;; arg = 0 )
    {
	if(!nomenus) { /* set up defaults */
	    strcpy(drbuf.c, driver);
	    c = ctlrno;
	    d = driveno;
	}

	/* if there is no arg, get something, if not doing -c  */
	if( arg == 0 ) {
	    if(nomenus)
		mustgivedisk();
	    getstring(s.c, driver, "device-name");
	}
	else
	    strcpy(s.c, arg);
	/* split out the device name into driver(ctlr, unit) */
	if((ret=split_devstring(s.c, drbuf.c, &c, &d, &scsilun)) < 0 ) {
	    /* argerr() jumps back to the fx main loop */
	    printf("\"%s\" is not a legal device name\n", s.c);
	    printf("Device name == \"driver(ctlr, unit)\"");
	    printf("\n");
	    continue;
	}
	if(nomenus && (c<0 || d<0))
	    mustgivedisk(); /* have to have full thing already with -c */
	if( c < 0 ) {
		tmp = (uint64_t)ctlrno;
	    getnum(&tmp, (uint64_t)MAX_CONTROLLER, "ctlr#");
		ctlrno = (uint)tmp;
	}
	else
	    ctlrno = (uint)c;
	if( d < 0 )
	{
		tmp = (uint64_t)driveno;
	    getnum(&tmp, (uint64_t)MAX_DRIVE, "drive#");
		driveno = (uint)tmp;
#ifndef _STANDALONE
	    if (!strcmp(drbuf.c, SCSI_NAME) && scsilun < 0)
	    {
		tmp = 0;
		getnum(&tmp, (uint64_t)MAX_LUN, "lun#");
		scsilun = (uint)tmp;
	    }
#endif
	}
	else
	    driveno = (uint)d;
	if (scsilun < 0)
	    scsilun = 0;

	for (driver_ptr = drivers; driver_ptr->driver_name != 0; 
		driver_ptr++) {
		if (!strcmp(driver_ptr->driver_name, drbuf.c)) {
			/* Got a driver match */
			drivernum = driver_ptr->driver_num;
			break;
		}
	}
	if (driver_ptr->driver_name == 0) {
		/* No match */
		printf("\"%s\" is not a legal driver name\n", drbuf.c);
		printf("Legal choices are:");
		for (driver_ptr = drivers; driver_ptr->driver_name != 0; 
			driver_ptr++)
			printf(" %s,", driver_ptr->driver_name);
		printf("\n");
		continue;
	}
	break;
    }

    /*
     * get everything.  store to globals
     */
    strcpy(savedriver.c, drbuf.c);
    driver = savedriver.c;
	sprintf(fulldrivename, "%s(%d,%d)", driver, ctlrno, driveno);

#ifdef SMFD_NAME
	if(strncmp(driver, "fd", 2) == 0)
		partnum = getfloptype(ret>0 ? &s.c[ret] : NULL);
#endif

	/*	This is somewhat of a hack, but it is far and away
		the most common case for our customers.  There is
		no way to get it 'correct' in the standalone case
		anyway.  Olson, 10/88 */
	isrootdrive = ctlrno == 0 && d == driver_ptr->rootdrive;
}

/*
 * split_devstring() --
 * split src string into driver, ctlrno, and driveno
 * unfortunately, this parsing is NOT same as accepted by the
 * standalone/prom, but it is too late to change
 *	DEV(X)		driver=DEV ctlrno=X driveno=-1
 *	DEV(X,)		driver=DEV ctlrno=X driveno= 0
 *	DEV(X,Y)	driver=DEV ctlrno=X driveno=Y
 *	DEV(X,Y,L)	driver=DEV ctlrno=X driveno=Y scsilun=L
 *	<other>		error
 * the src string is not harmed.
 * returns -1 on fatal errs; 0 if all ok, lent to ')' if any chars
 * after the ')'. This last allows a # after the paren (for floppies)
 */
static int
split_devstring(char *src, char *thedriver, int *_ctlr, int *_driveno, int *_scsilun)
{
    static STRBUF s;
    uint64_t l;
    register char *ap, *dp;

    strcpy(s.c, src);

    *_ctlr = *_driveno = *_scsilun = -1;
    src = s.c;
    dp = thedriver;
    while( *src != '\0' && !ispunct(*src) )
	*dp++ = *src++;
    *dp = '\0';
    if( *thedriver == '\0' )
	return -1;

    if( *src == '(' ) {
	src++;
	ap = src;
	src = skipcnum(src, 0, &l);
	if( src > ap )
	    *_ctlr = (int)l;
	if( *src == ',' )
	{
	    src++;
	    ap = src;
	    src = skipcnum(src, 0, &l);
	    if( src > ap )
		*_driveno = (int)l;
	}
	if( *src == ',' )
	{
	    src++;
	    ap = src;
	    src = skipcnum(src, 0, &l);
	    if( src > ap )
		*_scsilun = (int)l;
	}
	if( *src != ')' )
	    return -1;
	src++;
    }
    if( *src != '\0' )
	return src-s.c;
    return 0;
}

ITEM	format_items[] = {
	{"default", 0},
	{"current", 1},
	{0},
};

MENU	format_menu = {format_items, "format parameters"};

void
format_func(void)
{
    switch (drivernum) {
    case SCSI_NUMBER:
	{
    		int dp_param = 0;
		/* You can only format the entire unit on scsi so don't
		   worry about limits. */
		if( !nomenus ) {
			/* default to CURRENT parameters, NOT default.
			If someone is re-formatting a drive, chances are they
			want it the way it is, or the way they just set it up
			with the label/set/param command! Olson, 7/89 */
			argchoice(&dp_param, 1, &format_menu, 
				"Drive parameters to use in formatting");
		}
		if(!nomenus && dp_param == 0) {
			/* warn them one more time.  Note that when invoked with -a (from
			 * the auto menu choice, or directly, that we use the CURRENT
			 * params, since that is almost always the right thing to do.
			 * Even for a non-SGI drive, this is OK, because it will almost
			 * certainly be the default parameters anyway!  This also reduces
			 * problems for the hotline folks. */
			printf("Formatting with drive manufacturer's default geometry and parameters\n");
			do_scsidefault();
		}
		do_scsiformat(nomenus);
		bzero((char *)DT(&vh), sizeof DT(&vh)); /* Zap directory */
		break;
	}
#ifdef SMFD_NUMBER
    case SMFD_NUMBER:
	{
		do_smfdformat(nomenus);
		break;
	}
#endif

    default:
		printf("Don't know how to format disks for driver type %d\n",
			drivernum);
    }
    formatrequired = 0;
}

/* from pt.c */
extern void create_parts(void);
extern char whichparts[];

/* from lb.c */
extern char *bootfile;

void
auto_func(void)
{
    STRBUF s;

#ifdef SMFD_NUMBER
	if(drivernum != SMFD_NUMBER)
#endif
	{
		/* create, not set, since this is supposed to be automatic */
		setoff("create sgiinfo");
		callfunc(create_sgiinfo_func, "sgiinfo");
	}


    lastchance();

    doing_auto = 1;	/* set AFTER lastchance so we don't leave it set */

    /* write label before exercising so that badblocks will be forwarded */
    sync_func();

    setoff("exercise");
    sprintf(s.c, "complete -a");
    callfunc(complete_func, s.c);

	/* set bootinfo, same as create_bi_func, but no printf */
	VP(&vh)->vh_rootpt = PTNUM_FSROOT;
	VP(&vh)->vh_swappt = PTNUM_FSSWAP;
	strcpy(VP(&vh)->vh_bootfile, bootfile);

	/* create default partitions; create system disk, same as
	 * create_pt_func, but no printf. if they want an option
	 * disk, they should use /repart/option; otherwise some peole
	 * leave the overlapping parts, and can later hose themselves */
	bzero(whichparts, NPARTAB);
	whichparts[PTNUM_VOLUME] = whichparts[PTNUM_VOLHDR] =
		whichparts[PTNUM_FSROOT] = whichparts[PTNUM_FSSWAP] = 1;
	create_parts();

	/* same as create_dt_func, but no printf; clear the directory */
	bzero((char *)DT(&vh), sizeof DT(&vh));

	/* and now write the new vh back to disk */
    sync_func();
    doing_auto = 0;

    setoff("done");
}
void
exit_func(void)
{
    (void)setintr(1);
	if(driveopen) {
		if(formatrequired &&
			no("Geometry has changed, and a format is required\n"
			"Exit anyway"))
			mpop();
		/* after check, since the read may fail if a format is required */
		optupdate();
	}
	logmsg("fx exitting\n");
    if(nomenus)
	    exit(3);	/* only get here on attempt to read from stdin... */
    exit(0);
}
