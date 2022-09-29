/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.152 $"

/*
 *	par -- process activity reporter
 *
 *	It generates a report either by taking padc's output 
 *	or by invoking padc to get process activity data.
 * 	
 *	usage: par <collect_functions> <display_options> <command + argument list>
 *	       par <collect_functions> <display_options> -p pid
 *	       par <collect_functions> <display_options> -t 'time'
 *	       par <collect_functions> <display_options>
 *	       par <display_options>	
 *
 *	In the first instance, par execs the supplied command with the
 *	specified arguments and monitors the activity of that process.
 *
 *	If -p is specified, then par reports on the activity of the
 *	specified process.
 *
 *	If -t is specified, then par records information for 'time' seconds
 *	If no -t is given cotinuous information is collected
 *
 *	In the last instance, par reads a padc output file from
 *	standard input and reports on its contents.
 *
 *	The types of activity which can be reported are controlled
 *	by the following functions and options:
 *	
 *	collection functions:
 *      	-s - trace system calls with parameters
 *      	-r - trace scheduler
 *      	-x - trace network queueing
 *      	-X - trace network throughput
 *		-k - trace disk activity
 *		-p - trace a specific pid
 *		-i - inherit tracing on fork
 *		-O file - copy trace data to specified file
 *
 *	display options:
 *		-l 		list system call in long format
 *		-n syscall-num	select system call by system call number/name
 *		-e syscall-num	exclude system call by system call number/name
 *		-N syscall-name	select system call by system call name/number
 *		-P pid		trace named process only
 *		-S		print summary of system call and signal counts
 *		-SS		print system calls and signals trace
 *		-Q		print scheduling summary
 *		-QQ		print scheduling trace
 *		-QQQ		print full runq context scheduling trace
 *		-c		do not print cpuid
 *		-d		show any syscall time delta
 *		-u		show times in uS
 *		-o file		output to file
 *		-a len		max ascii length to print
 *		-b len		max binary length to print
 *		-B		force binary output rather than ascii
 *		-A		force ascii output rather than binary
 *		-z		sort syscall summary by syscall name
 *
 * NB: event processing and display is handled by common code found
 *     in -lrtmon; go there to add new support for decoding system
 *     calls and other events.
 */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>

#include <rtmon.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/schedctl.h>

#define PADC	"/usr/sbin/padc"	/* name of event collector app */

typedef __uint64_t ptime_t;		/* time base in par */

struct kidlist {		/* linked list of processes */
    struct kidlist* next;
    struct kidlist* prev;
};

struct kidq {			/* queue of processes */
    struct kidlist h;		/* list of processes */
    int		qlen;		/* queue length */
    int		qmaxlen;	/* max queue length over time */
};
struct kidq runq;			/* global run queue */
struct kidq localqs[RTMOND_MAXCPU];	/* per-cpu affinity queue */

struct cpurec {			/* per-CPU statistics */
    ptime_t	idletimes;	/* total number of times went idle */
    ptime_t	idleticks;	/* total mS spent idle */
    ptime_t	wentidle;	/* tick number last went idle */
    int		disps;		/* total times dispatched */
    int		hotdisps;	/* times dispatched from local/affinity q */
    ptime_t	runticks;	/* total mS spent running */
    int		swtchs;		/* number of calls to swtch */
    int64_t	curkid;		/* running kid */
    int64_t	lastkid;	/* last running kid */
    int		runsame;	/* times called swtch but ran same guy */
    int		nothighest;	/* ran guy other than top of run q */
    int		curpri;		/* priority of proc running at dispatch */
};
struct cpurec cpus[RTMOND_MAXCPU];	/* cpu statistics table */

struct kidrec {		/* per-thread state+statistics */
    struct kidlist h;		/* queue links */
    struct kidq* curq;		/* current q process is on */
    struct kidrec* hnext;	/* hash chain */
    int		traced;		/* 1 if being traced */
    char*	name;		/* process name string */
    int64_t	kid;		/* thread id */ 
	/*    pid_t	pid;		 process id */
    int		pri;		/* scheduling priority */
    int		rtpri;		/* priority if realtime */
    int		lastrun;	/* last CPU proc ran on */
    int		slices;		/* # times switched in */
    ptime_t	starttick;	/* time last switched in */
    ptime_t	runtime;	/* total time spent running */
    int		queued;		/* # times placed on global run queue */
    int		lqueued;	/* # times placed on local run queue */
    ptime_t	queuedtick;	/* time last placed on global queue */
    ptime_t	queuedtime;	/* total time spent on global queue */
    ptime_t	lqueuedtime;	/* total time spent on local queue */
    ptime_t	sleeptick;	/* time last put to sleep */
    ptime_t	sleeptime;	/* total time spent asleep */
    int		cpuswitch;	/* # times moved between CPUs */
    int		preempt_short;	/* # times preempted for <10 ticks */
    int		preempt_long;	/* # times preempted for >=10 ticks */
    int		swtch;		/* # times switched out */
    int		sleep;		/* # times put to sleep */
    int		yield;		/* # times voluntarily yielding CPU */
    int		mustrun;	/* # times switched out 'cuz of mustrun proc */
};

/*
 * Per-process state+statistics are maintained separately from
 * the state maintained by the library support code.  We should
 * be able to hook our data on to that of the library but there's
 * currently no way to do that.
 *
 * We use an egregiously large hash table size in order to handle
 * cases where we may be keeping track of hundreds or even thousands
 * of processes.  Since pids tend to clump together we probably will
 * only use a few cache lines out of each table when we're working
 * with a small number of processes.
 */
#define KIDHASHSIZE  4096			/* must be power of 2 */
#define KIDHASH(kid) ((kid) & (KIDHASHSIZE-1))	/* hash pid to hash index */
struct kidrec *kidhash[KIDHASHSIZE];	/* process statistics table */

char	funs[512];
FILE	*nfd = stdin;
int	debug = 0;		/* print debug information */
int	collect;		/* we're collecting data */
int	spawn;			/* should we spawn process to monitor */
int	Qflag;			/* scheduling display flag */
int	Sflag;			/* syscall display flag */
int	display;		/* disk/net/misc. collect/display flag */
int	cflag = 0;		/* 1 if -c option specified */
ptime_t	msecs;			/* current time */
int	syscallp;		/* report restricted to pids?? */
int	longl = 0;
int	inherit = 0;		/* have tracing inherited on fork */
FILE*	outf = stdout;		/* output file */
FILE*	errf = stderr;		/* error file */
const	char* padcfile = NULL;	/* name of optional event data file */
FILE*	dataf = NULL;		/* event data file */
rtmonPrintState* rs;		/* decoder state */
int	syscalls = 0;		/* # syscalls specified */
int	zflag = 0;		/* sort syscall summary by name */
u_int	contextswtch = 0;	/* number of context switches */
u_int	lost = 0;		/* number of events lost */

void	usage(char *progname);
void	readpadc(FILE*);
void	copypadc(FILE*);
void	setsyscalltrace(const char* arg, int onoff);
void	cpuidle(const tstamp_event_entry_t*);
void	cpusched(const tstamp_event_entry_t*);
void	kidexit(const tstamp_event_entry_t*);
void	cpusummary(void);
void	runqsummary(void);
void	printkidsum(register struct kidrec *kp);
void	kidsummary(void);
void	schedsummary(void);
void	dumpcpushort(void);
void	dumprunq(void);
void	showstat(void);
void	dumperrors(char *msg);
void	dumpsums(void);
void	sys_summary(void);
void	Qinit(struct kidq *);
void	Qinsert(struct kidq *qp, struct kidrec *entp);
void	Qremove(struct kidrec *entp);
void	kidinsert(struct kidrec *kp);
struct	kidrec *newkid(int64_t pid);
struct	kidrec *getkid(int64_t pid);
struct	kidrec *kidfind(register int64_t pid);
int	getpri(struct kidrec *kp);
void	kidwalk(void (*)(struct kidrec *));
void	handleevent(const tstamp_event_entry_t* ev);
void	merge(uint64_t, const tstamp_event_entry_t* pending[], int npending[]);

void sigcatcher(int sig) { (void) sig; }

static void
error(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(errf, "par: ");
    vfprintf(errf, fmt, ap);
    va_end(ap);
    fputc('\n', errf);
}

int
main(int argc, char **argv)
{
    int monitoring = 0;		/* number of pids we're monitoring */
    pid_t childid;		/* pid of child executing command */
    int c;			/* option switch */
    const char* optstr = "Aa:Bb:cdDe:iKklN:n:O:o:P:p:rst:uSQXxz";

    rs = rtmon_printBegin();
    rs->eventmask = 0;		/* by default, display nothing */
    strcpy(funs, PADC);
    while ((c = getopt(argc, argv, optstr)) != -1) {
	switch(c) {
	case 'i':		/* inherit tracing */
	    inherit++;
	    strcat(funs, " -i");
	    break;
#ifdef NET_DEBUG
	case 'X':		/* network */
	    rs->eventmask |= RTMON_NETFLOW;
	    display=1;
	    goto collect_arg;
	case 'x':		/* network */
	    rs->eventmask |= RTMON_NETSCHED;
	    display=1;
	    goto collect_arg;
#endif
	case 'K':
	    rs->flags |= (RTPRINT_SHOWKID | RTPRINT_SHOWPID);
	    display=1;
	    break;
	case 'k':		/* disk */
	    rs->eventmask |= RTMON_DISK;
	    display=1;
	    goto collect_arg;
	case 'r':		/* scheduler */
	case 's':		/* system calls+signals */
	collect_arg:
	    collect++;
	    sprintf(strchr(funs,'\0'), " -%c", c);
	    break;
	case 't':		/* collect for a period of time */
	    collect++;
	    sprintf(strchr(funs,'\0'), " -t %s", optarg);
	    break;
	case 'p':		/* collect events from a specific pid */
	    collect++;
	    monitoring++;
	    sprintf(strchr(funs,'\0'), " -p %s", optarg);
	    /* simulate previous behaviour--limit reporting */
	    rtmon_traceProcess(rs, atoi(optarg));
	    break;
	case 'l':		/* force long listing */
	    ++longl;
	    break;
	case 'a':		/* constrain max ascii string length */
	    rs->max_asciilen = atoi(optarg);
	    break;
	case 'A':		/* print data as ascii */
	    rs->flags |= RTPRINT_ASCII;
	    rs->flags &= ~RTPRINT_BINARY;
	    break;
	case 'b':		/* constrain max binary data length */
	    rs->max_binlen = atoi(optarg);
	    break;
	case 'B':		/* print data as binary */
	    rs->flags &= ~RTPRINT_ASCII;
	    rs->flags |= RTPRINT_BINARY;
	    break;
	case 'P':		/* filter reporting by pid */
	    rtmon_traceProcess(rs, atoi(optarg));
	    syscallp++;
	    break;
	case 'Q':		/* report scheduling activity */
	    if (++Qflag > 1)
		rs->eventmask |= RTMON_TASK;
	    break;
	case 'S':		/* report system call activity */
	    if (++Sflag > 1)
		rs->eventmask |= RTMON_SYSCALL|RTMON_SIGNAL;
	    break;
	case 'n':		/* filter reporting by sys call name/number */
	case 'e':		/* exclude sys call name/number */
	case 'N':		/* for compatibility */
	    setsyscalltrace(optarg, c != 'e');
	    break;
	case 'c':		/* disable CPU printing */
	    rs->flags &= ~RTPRINT_SHOWCPU;
	    cflag = 1;
	    break;
	case 'd':		/* show system calls as begin+end */
	    rs->syscalldelta = 0;
	    break;
	case 'u':		/* show times as ms+us delta */
	    rs->flags |= RTPRINT_USEUS;
	    break;
	case 'D':
	    rs->flags |= RTPRINT_INTERNAL;
	    debug++;
	    break;
	case 'o':		/* send report output to specified file */
	    outf = fopen(optarg, "w+");
	    if (outf == NULL) {
		error("%s: Cannot open: %s", optarg, strerror(errno));
		exit(1);
	    }
	    errf = outf;
	    break;
	case 'O':		/* send event data to specified file */
	    padcfile = optarg;
	    break;
	case 'z':
	    zflag = 1;
	    break;
	default:
	    usage(argv[0]);
	    exit(1);
	}
    }
    /* if any args left then we should spawn a process */
    if (optind < argc) {
	if (!Qflag && !Sflag && !display && !collect) {
	    /*
	     * If no analysis or collection options were specified
	     * and a command line appears to be present, then do a
	     * system call trace since that's what most people seem
	     * to want (e.g. par ls -> par -sSS ls).
	     */
	    Sflag = 2;
	    rs->eventmask |= RTMON_SYSCALL|RTMON_SIGNAL;
	    strcat(funs, " -s");
	}
	collect++, spawn++;
    }

    if (!(Qflag || Sflag || display)) {	/* no analysis options */
	if (!collect) {
	    error("No statistics collection or analysis options specified.");
	    usage(argv[0]);
	    exit(1);
	} else if (monitoring) {
	    /*
	     * If no analysis or collection options were specified
	     * then do a system call trace since that's what most
	     * people seem to want (e.g. par -p 178 -> par -sSS -p 178).
	     */
	    Sflag = 2;
	    rs->eventmask |= RTMON_SYSCALL|RTMON_SIGNAL;
	    strcat(funs, " -s");
	} else if (padcfile == NULL) {
	    error("No event data file specified "
		"for data collection; use the -O option.");
	    usage(argv[0]);
	    exit(1);
	}
    }
    if (padcfile != NULL) {
	dataf = fopen(padcfile, "w");
	if (dataf == NULL) {
	    error("Cannot open event data file %s: %s.",
		padcfile, strerror(errno));
	    exit(1);
	}
    }

    if (isatty(fileno(outf)))		/* line buffer output to tty */
	setlinebuf(outf);

    /*
     * Force a long listing if more than one
     * process might appear in the traces.
     */
    if (!longl)
	longl = (Qflag > 1 || syscallp > 1 || inherit ||
		    (!spawn && monitoring != 1)) ||
		    (rs->flags & RTPRINT_SHOWKID) ;
    if (longl)
	rs->flags |= RTPRINT_SHOWPID;
    else	
	rs->flags &= ~RTPRINT_SHOWPID;

    if (collect) {		   	/* collect event data */
	if (spawn) {			/* fork a child to exec command */
	    switch (childid = fork()) {
	    case 0:			/* child */
		/*
		 * Synchronize w/ padc by pausing until we
		 * receive SIGUSR1.  This lets padc start up
		 * event collection before the command is exec'd.
		 */
		signal(SIGUSR1, sigcatcher);
		pause();
		execvp(argv[optind], &argv[optind]);
		dumperrors("exec failed");
		/*NOTREACHED*/
	    case -1:
		dumperrors("command fork failed");
		/*NOTREACHED*/
	    default:			/* parent */
		/*
		 * Pass child's pid to padc so it can send
		 * it SIGUSR1 when it's setup to collect
		 * event data for the command.
		 */
		sprintf(strchr(funs,'\0'), " -P %d", childid);
		/* restrict tracing to child proc */
		rtmon_traceProcess(rs, childid);
		syscallp++;
		break;
	    }
	}
	/*
	 * Spawn padc to collect data.  We add args
	 * to force padc to ``line buffer'' its output
	 * so long-running processes w/ small amounts
	 * of event data don't have their results buffered
	 * indefinitely; and pass the max # bytes to
	 * collect for indirect params.  Note that the
	 * latter is actually a system-wide parameter
	 * so one padc can affect another (sigh).
	 */
	sprintf(strchr(funs,'\0'), " -b 0 -I %d",
	    rs->max_asciilen > rs->max_binlen ?
		rs->max_asciilen : rs->max_binlen);
	if (debug > 1)
	    strcat(funs, debug > 2 ? " -DD" : " -D");
	if ((nfd = popen(funs, "r")) == 0)
	    dumperrors("padc exec failed");
    }

    if (Qflag || Sflag || display) {
	if (Qflag) {
	    u_int i;
	    Qinit(&runq);
	    for (i = 0; i < RTMOND_MAXCPU; i++)
		Qinit(&localqs[i]);
	}

	readpadc(nfd);
	if (Sflag)
	    sys_summary();

	if (Qflag) {
	    dumpsums();
	    schedsummary();
#ifdef NET_DEBUG
	    netsummary();
#endif
	}
    } else
	copypadc(nfd);

    if (collect) {
	if (spawn)	/* in case padc neglected to do this */
	    kill(childid, SIGUSR1);
	pclose(nfd);
    }
    if (outf != stdout)
	fclose(outf);
    if (dataf != NULL)
	fclose(dataf);
    return (0);
}

void
setsyscalltrace(const char* arg, int onoff)
{
    if (isdigit(arg[0])) {
	if (rtmon_settracebynum(rs, atoi(arg), onoff) ||
	    rtmon_settracebyname(rs, arg, onoff))
	    goto done;
    } else if (rtmon_settracebyname(rs, arg, onoff))
	goto done;
    error("Unknown system call name/number %s.", arg);
    exit(1);
    /*NOTREACHED*/
done:
    syscalls++;
    if (Sflag < 2)				/* enable syscall reporting */
	Sflag = 2;
    rs->eventmask |= RTMON_SYSCALL|RTMON_SIGNAL;
    strcat(funs, " -s");
}

/*
 * Read the remainder of a partial event.
 */
static int
readrem(FILE* fd, char* buf, size_t cc, size_t max)
{
    size_t rem = max - (cc % sizeof (tstamp_event_entry_t));
    do {
	size_t n = fread(&buf[cc], 1, rem, fd);
	if (n == 0)
	    break;
	rem -= n;
    } while (rem > 0);
    return (rem == 0);
}

void
readpadc(FILE* fd)
{
    const tstamp_event_entry_t* pending[RTMOND_MAXCPU];
    int npending[RTMOND_MAXCPU];
    char* data[RTMOND_MAXCPU];
    size_t datalen[RTMOND_MAXCPU];
    uint64_t tlast[RTMOND_MAXCPU];

    memset(pending, 0, sizeof (pending));
    memset(npending, 0, sizeof (npending));
    memset(data, 0, sizeof (data));
    memset(datalen, 0, sizeof (datalen));
    memset(tlast, 0, sizeof (tlast));
    for (;;) {
	union {
		tstamp_event_entry_t ev[NUMB_KERNEL_TSTAMPS];
		char buf[1];
	} u;
	size_t n;
	tstamp_event_entry_t* ev;

	n = fread(u.buf, 1, sizeof (u.ev[0]), fd);
	if (n == 0)
	    break;
	if (n != sizeof (u.ev[0]) && !readrem(fd, u.buf, n, sizeof (u.ev[0]))) {
	    error("Partial event read. n is %d and size is %d evt is %d", n,sizeof(u.ev[0]),u.ev[0].evt);
	    break;
	}
	ev = u.ev;
	if (ev->jumbocnt > 0) {		/* jumbo event, read the remainder */
	    n = ev->jumbocnt * sizeof (*ev);
	    if (!readrem(fd, (char*) u.buf, sizeof (u.ev[0]), n)) {
		error("Partial event read, terminating.");
		break;
	    }
	}
	if (dataf != NULL &&
	  fwrite(u.buf, (1+ev->jumbocnt)*sizeof (*ev), 1, dataf) != 1) {
	    error("%s: Output write error: %s.", padcfile, strerror(errno));
	    break;		/* XXX? */
	}
	if (ev->evt == TSTAMP_EV_SORECORD) {
	    int cpu;
	    size_t chunksize;
	    size_t nevts;
	    size_t off;
	    uint64_t threshold;
	    /*
	     * A new "record" of data to process.  Read the
	     * new chunk of event data into the per-CPU buffer.
	     * We move data or expand the buffer as needed to
	     * accomodate the new events.
	     */
	    cpu = ev->cpu;
	    assert(cpu < RTMOND_MAXCPU);
	    chunksize = (size_t) ev->qual[0];
	    nevts = chunksize / sizeof (*ev);
	    off = ((const char*) pending[cpu]) - data[cpu];
	    if (off + (npending[cpu]+nevts)*sizeof (*ev) > datalen[cpu]) {
		if (chunksize <= off) {
		    /*
		     * Space is available at the front of the buffer;
		     * just copy the pending events down.
		     */
		    if (npending[cpu])
			memmove(data[cpu], pending[cpu],
			    npending[cpu]*sizeof (*ev));
		    pending[cpu] = (const tstamp_event_entry_t*) data[cpu];
		} else {
		    char* dp;
		    /*
		     * Grow the data buffer to hold this chunk of
		     * events and any that are pending; and reset
		     * the pending reference to the buffer.  We
		     * allocate a new buffer and copy pending data
		     * to the front instead of realloc'ing the
		     * existing buffer because we're likely to
		     * get more data soon and that would just
		     * cause us to copy lots of data to the front
		     * anyway.  This way we copy (hopefully) less
		     * data and read the new chunk directly into
		     * the right spot--eliminating a copy.
		     */
		    datalen[cpu] = off+(npending[cpu]+nevts)*sizeof (*ev);
		    dp = (char*) malloc(datalen[cpu]);
		    if (dp == NULL) {
			error("Out of memory (event data).");
			break;
		    }
		    if (data[cpu]) {
			memcpy(dp, pending[cpu], npending[cpu]*sizeof (*ev));
			free(data[cpu]);
		    }
		    data[cpu] = dp;
		    pending[cpu] = (const tstamp_event_entry_t*) dp;
		}
	    }
	    /*
	     * Read the chunk of event data.
	     */
	    if (chunksize && fread((char*)(pending[cpu]+npending[cpu]), chunksize, 1, fd) != 1) {
		if (debug)
		    fprintf(outf, "short read of %d bytes of event data\n",
			chunksize);
		break;
	    }
	    npending[cpu] += nevts;
	    /*
	     * Calculate the latest time that we can merge
	     * events up to.  We know each stream of events
	     * is ordered so by comparing the time of the
	     * last event for each CPU we can select a time
	     * that is safe to use in selecting events to merge.
	     */
	    tlast[cpu] = ev->qual[2];
	    threshold = tlast[0];
	    if (debug)
		    fprintf(stdout,"threashold %lld\n",threshold);
	    for (cpu = rtmon_ncpus(rs)-1; cpu > 0; cpu--)
		if (tlast[cpu] < threshold)
		    threshold = tlast[cpu];
	    merge(threshold, pending, npending);
	} else
	    handleevent(ev);
	if (debug)
	    fflush(outf);
    }
    if (debug)
	    fprintf(stdout,"threashold %lld\n",-1LL);
    merge(-1LL, pending, npending);		/* flush any remainder */
}

void
handleevent(const tstamp_event_entry_t* ev)
{
    static int lost = 0;
    static ptime_t starttime = 0;

    /*
     * Use the first non-config event to set the
     * base time for calculating relative time
     * and to decide whether or not to display
     * CPU numbers in the listing.
     */
    if (!starttime && ev->evt != TSTAMP_EV_CONFIG) {
	starttime = ev->tstamp;
	if (!cflag) {
	    if (rtmon_ncpus(rs) > 1)
		rs->flags |= RTPRINT_SHOWCPU;
	    else
		rs->flags &= ~RTPRINT_SHOWCPU;
	}
    }
    msecs = ev->tstamp - starttime;

    switch (ev->evt) {
    case TSTAMP_EV_EXIT:		/* process exit'd */
	kidexit(ev);
	rtmon_printEvent(rs, outf, ev);
	break;
    case TSTAMP_EV_FORK: {		/* process fork'd */
	const tstamp_event_fork_data_t* fdp =
	    (const tstamp_event_fork_data_t*) &ev->qual[0];

	if (inherit && _rtmon_kid_istraced(&rs->ds, (int64_t) fdp->pkid))
	    rtmon_traceKid(rs, (int64_t)fdp->ckid);

	rtmon_printEvent(rs, outf, ev);
	break;
    }
    case EVENT_TASK_STATECHANGE:	/* proc state change */
	contextswtch++;
	cpusched(ev);
	break;
    case TSTAMP_EV_EOSWITCH:		/* new proc to run */
    case TSTAMP_EV_EODISP:		/* put proc on runq */
	cpusched(ev);
	break;
    case EVENT_WIND_EXIT_IDLE:		/* idle processor */
	contextswtch++;
	cpuidle(ev);
	break;
    case TSTAMP_EV_LOST_TSTAMP:		/* server lost events */
	lost += (int) ev->qual[0];
	fprintf(outf, "OVERFLOW: CPU %d lost %d so far\n", ev->cpu, lost);
	break;
    default: 
	rtmon_printEvent(rs, outf, ev);
	break;
    }
}

/*
 * Merge event data up to tmax time.  Merged events
 * are written to stdout (w/ buffering).  This code
 * assumes that each CPU's set of events are already
 * ordered in time; we just merge the event streams
 * from different CPUs.
 */
void
merge(uint64_t tmax, const tstamp_event_entry_t* pending[], int npending[])
{
    const tstamp_event_entry_t* candidate[RTMOND_MAXCPU];
    const tstamp_event_entry_t* ev;
    int nc, i, j, k;

    /*
     * Find CPUs with events to merge and do a
     * first-level sort of the top events for
     * each CPU.  We assume below that each CPU's
     * events are already sorted.
     */
    nc = 0;
    if (debug > 1)
	fprintf(outf, "par: merge to %llu: ", tmax);
    for (i = rtmon_ncpus(rs)-1; i >= 0; i--) {
	if (npending[i] && (ev = pending[i])->tstamp < tmax) {
	    for (j = 0; j < nc && ev->tstamp > candidate[j]->tstamp; j++)
		;
	    if (j < nc) {			/* insert in middle */
		for (k = nc-1; k >= j; k--)
		    candidate[k+1] = candidate[k];
	    }
	    candidate[j] = ev, nc++;
	    if (debug > 1)
		fprintf(outf, " CPU[%d] %d", i, npending[i]);
	} else if (debug > 1)
	    fprintf(outf, " !CPU[%d](%d,%lld)", i, npending[i],
		npending[i] ? pending[i]->tstamp : 0);
    }
    if (debug > 1)
	fprintf(outf, "\n");
    while (nc > 0) {
	ev = candidate[0];			/* sorted event */
	if (ev->tstamp > tmax) {
	    for (i = 0; i < nc; i++)
		pending[candidate[i]->cpu] = candidate[i];
	    break;
	}
	handleevent(ev);			/* process event */
	j = 1+ev->jumbocnt;			/* slots used by event */
	assert(j <= npending[ev->cpu]);
	npending[ev->cpu] -= j;
	if (npending[ev->cpu]) {		/* merge next event for CPU */
	    /*
	     * Advance to next event for this CPU and
	     * re-sort the top events based on time.
	     * Since we know that all other candidate
	     * events are already sorted by time this
	     * just entails inserting the new event in
	     * the correct place.
	     */
	    ev += j;
	    for (i = 0; i < nc-1 && ev->tstamp > candidate[i+1]->tstamp; i++)
		candidate[i] = candidate[i+1];
	    candidate[i] = ev;
	} else {				/* no more events for CPU */
	    pending[ev->cpu] = ev+j;
	    nc--;
	    for (i = 1; i <= nc; i++)		/* shift down, already sorted */
		candidate[i-1] = candidate[i];
	}
    }
}

/*
 * Check a server-calculated checksum.  By
 * default data does *not* have checksums;
 * these are added only for debugging.
 */
void
checksum(int cpu, const tstamp_event_entry_t* ev, size_t cc0, uint64_t esum)
{
    const uint64_t* lp = (const uint64_t*) ev;
    uint64_t sum = 0;
    size_t cc;
    for (cc = cc0; (ssize_t) cc > 0; cc -= sizeof (*lp))
	sum += *lp++;
    if (sum != esum) {
	fprintf(stderr,
"padc: checksum mismatch, cpu %d, count %d, got %#llx, expected %#llx\n",
	    cpu, cc0, sum, esum);
	lp = (const uint64_t*) ev;
	for (cc = 0; cc != cc0; cc += sizeof (*lp))
	    fprintf(stderr, "%4d: %#llx\n", cc, *lp++);
    }
}

void
copypadc(FILE* fd)
{
    for (;;) {
	char buf[128*1024];
	size_t n = fread(buf, 1, sizeof (buf), fd);
	if (n == 0)
	    break;
	if (fwrite(buf, n, 1, dataf) != 1) {
	    error("%s: Output write error: %s.", padcfile, strerror(errno));
	    break;
	}
    }
}

void
dumpsums(void)
{
    if (contextswtch && msecs) {
	u_int secs = (u_int)(rtmon_toms(rs,msecs) / 1000);
	fprintf(outf, " %d context switches / sec\n",
	    contextswtch / (secs ? secs : 1));
    }
}

typedef struct {
    const char*	name;
    int		count;
    ptime_t	time;
} scallstat_t;
static	scallstat_t* stats = NULL;
static	int nstats;

/* dump system calls' summary */
static void
recordstat(rtmonPrintState* rs, const char* name, int count, __int64_t time)
{
    (void) rs;
    if (nstats == 0)
	stats = (scallstat_t*) malloc(sizeof (*stats));
    else
	stats = (scallstat_t*) realloc(stats, (nstats+1) * sizeof (*stats));
    stats[nstats].name = name;
    stats[nstats].count = count;
    stats[nstats].time = time;
    nstats++;
}
static int
statcompar(const void* a, const void* b)
{
    const scallstat_t* sa = (const scallstat_t*) a;
    const scallstat_t* sb = (const scallstat_t*) b;

    if (sa->time != sb->time)
	return ((int)(sb->time - sa->time));
    if (sa->count != sb->count)
	return (sb->count - sa->count);
    return (strcmp(sa->name, sb->name));
}
static int
namecompar(const void* a, const void* b)
{
    const scallstat_t* sa = (const scallstat_t*) a;
    const scallstat_t* sb = (const scallstat_t*) b;

    return (strcmp(sa->name, sb->name));
}
static void
banner(const char* c1, const char* c2, const char* c3, const char* c4)
{
    fprintf(outf, "%-14s %6s %9s %9s\n", c1, c2, c3, c4);
}
void
sys_summary(void)
{
    int i;

    rtmon_walksyscalls(rs, recordstat);
    qsort(stats, nstats, sizeof (scallstat_t), zflag ? namecompar : statcompar);
    if (nstats > 0) {
	if (Sflag > 1)
	    fputc('\n', outf);
	fprintf(outf, "System call summary:\n");
	banner(    "",       "", "Average",     "Total");
	banner("Name", "#Calls", "Time(ms)", "Time(ms)");
	fprintf(outf, "-----------------------------------------\n");
	for (i = 0; i < nstats; i++) {
	    const scallstat_t* st = &stats[i];
	    fprintf(outf, "%-14s %6d %9.2f %9.2f\n"
		, st->name
		, st->count
		, rtmon_tomsf(rs, st->time/st->count)
		, rtmon_tomsf(rs, st->time)
	    );
	}
	free(stats);
    }
}

/*
 * Queue manipulation primitives
 */
void
Qinit(struct kidq* qp)
{
    qp->h.next = qp->h.prev = &qp->h;
    qp->qlen = 0;
    qp->qmaxlen = 0;
}

#define	Qempty(qp)	(qp->next == qp)

int
getpri(struct kidrec* kp)
{
    return (kp->pri <= PZERO || kp->rtpri == 0 ? kp->pri : kp->rtpri);
}

void
Qinsert(struct kidq* qp, struct kidrec* entp)
{
    int newpri = getpri(entp);
    struct kidlist* wp;

    for (wp = qp->h.next; wp != &qp->h; wp = wp->next) {
	int workpri = getpri((struct kidrec*) wp);
	if (newpri < workpri)
	    break;
    }

    entp->curq = qp;
    entp->h.next = wp;
    entp->h.prev = wp->prev;
    entp->h.prev->next = wp->prev = &entp->h;
    if (++(qp->qlen) > qp->qmaxlen)
	    qp->qmaxlen = qp->qlen;
}

void
Qremove(struct kidrec* entp)
{
    struct kidq* qp = entp->curq;

    assert(qp != NULL);
    entp->h.next->prev = entp->h.prev;
    entp->h.prev->next = entp->h.next;
    entp->h.next = entp->h.prev = NULL;
    entp->curq = NULL;
    qp->qlen--;
    assert(qp->qlen >= 0);
}

void
kidinsert(struct kidrec* kp)
{
    struct kidrec **phead = &kidhash[KIDHASH(kp->kid)];
    while (*phead != NULL) {
	assert((*phead)->kid != kp->kid);
	phead = &(*phead)->hnext;
    }
    assert(kp->hnext == NULL);
    *phead = kp;
}

struct kidrec*
kidfind(register int64_t kid)
{
    struct kidrec *kp = kidhash[KIDHASH(kid)];
    for (; kp != NULL && kp->kid != kid; kp = kp->hnext)
	;
    return (kp);
}

void
kidwalk(void (*func)(struct kidrec*))
{
    int i;
    for (i = 0; i < KIDHASHSIZE; i++) {
	struct kidrec* kp = kidhash[i];
	for (; kp != NULL; kp = kp->hnext)
	    (*func)(kp);
    }
}

struct kidrec*
newkid(int64_t kid)
{
    register struct kidrec* kp;

    if ((kp = (struct kidrec*) malloc(sizeof(*kp))) == NULL) {
	error("Out of memory: can not allocate %d bytes for KID %d.",
	    sizeof (*kp), kid);
	exit(1);
    }

    memset(kp, 0, sizeof(*kp));
    kp->traced =
       (rs->flags & RTPRINT_ALLPROCS) != 0 || _rtmon_kid_istraced(&rs->ds, kid);
    kp->kid = kid;
    kp->pri = PIDLE;
    kp->rtpri = PIDLE;
    kp->lastrun = -1;
    kp->starttick = -1;
    kp->queuedtick = -1;
    kp->sleeptick = -1;

    /*
     * Insert the new record
     */
    kidinsert(kp);

    return (kp);
}

struct kidrec*
getkid(int64_t kid)
{
    struct kidrec* kp = kidfind(kid);
    return (kp == NULL ? newkid(kid) : kp);
}

void
cpuidle(const tstamp_event_entry_t* ev)
{
    struct cpurec* crp = &cpus[ev->cpu];

    if (!Qflag)
	return;
    if (crp->wentidle == 0) {
	crp->idletimes++;
	crp->curkid = -1;
	crp->curpri = PIDLE;
	crp->wentidle = ev->tstamp;
	rtmon_printEvent(rs, outf, ev);
    }
}

#define RTMON_UNPACKPRI(v, f, p, bp) { \
    f = (int)(v>>32); p = (short)(v&0xffff); bp = (short)((v>>16)&0xffff); \
}

/*
 * handle scheduling event
 * Always maintain all statistics even if only tracing a single pid
 * The display options only handle what is printed.
 * This lets us print reasonable summary stats
 */
void
cpusched(const tstamp_event_entry_t* ev)
{
    register struct kidrec *kp;
    register struct cpurec *crp;
    ptime_t now;
    cpuid_t onrq;
    int64_t kid;
    int flags, pri, basepri;

    if (!Qflag)
	    return;

    kid = (int64_t) ev->qual[0];
    kp = getkid(kid);
    crp = &cpus[ev->cpu];
    now = ev->tstamp;
    RTMON_UNPACKPRI(ev->qual[1], flags, pri, basepri);

    switch (ev->evt) {
    case TSTAMP_EV_EODISP:		/* put on a run q */
	    kp->pri = pri;
	    kp->rtpri = basepri;
	    if (kp->sleeptick != -1) {
		    if (! (now >= kp->sleeptick))
			    now = kp->sleeptick;
		    kp->sleeptime += now - kp->sleeptick;
		    kp->sleeptick = -1;
	    }
	    kp->queuedtick = now;
	    /*
	     * Processor affinity is implemented by placing
	     * processes/threads on a per-cpu local q first,
	     * then moving them to the global run q if not
	     * re-scheduled quickly.  This can result in
	     * multiple event records, one for each move.
	     *
	     * NB: cpu identifies the processor where the event
	     *     record was generated.  The target processor
	     *     (or ncpus if none) is identified separately
	     *     since they need not be the same.
	     */
	    if (kp->curq != NULL)		/* remove from previous q */
		    Qremove(kp);
	    onrq = (cpuid_t) flags;		/* NB: packed in w/ pri's */
	    assert(onrq != (cpuid_t) -1);
	    if (onrq < rtmon_ncpus(rs)) {
		    kp->lqueued++;
		    Qinsert(&localqs[onrq], kp);
	    } else if (onrq == rtmon_ncpus(rs)) {
		    kp->queued++;
		    Qinsert(&runq, kp);
	    } else {
		    error(
			  "Scheduling event for invalid CPU %u (ncpus %u); max CPUs %u.",
			  onrq, rtmon_ncpus(rs), RTMOND_MAXCPU);
		    exit(1);
	    }
	    break;
    case EVENT_TASK_STATECHANGE:	/* switched out */
	    crp->swtchs++;
	    crp->lastkid = kp->kid;
	    /*
	     * set up for idle here - only really important
	     * when not tracing everyone - so runq stuff still looks
	     * right
	     */
	    crp->curkid = -1;
	    crp->curpri = PIDLE;
	    kp->pri = pri;
	    kp->rtpri = basepri;
	    if (kp->starttick != -1) {
		    ptime_t runtime;
		    if (! (now >= kp->starttick))
			    now = kp->starttick;
		    runtime = now - kp->starttick;
		    kp->runtime += runtime;
		    crp->runticks += runtime;
	    }
	    kp->swtch++;
	    switch (flags) {
	    case SEMAWAIT:
	    case MUTEXWAIT:
	    case SVWAIT:
	    case MRWAIT:
	    case GANGWAIT:
		    kp->sleep++;
		    kp->sleeptick = now;
		    break;
	    case MUSTRUNCPU:
		    kp->mustrun++;
		    break;
	    case RESCHED_P:
	    case RESCHED_KP:
		    if (kp->starttick != -1 && rtmon_toms(rs,now - kp->starttick) < 10)
			    kp->preempt_short++;
		    else
			    kp->preempt_long++;
		    break;
	    case RESCHED_S:
	    case RESCHED_Y:
	    case RESCHED_D:
		    kp->yield++;
		    break;
	    default:
		    fprintf(outf, "Unknown sched reason %d\n", flags);
		    break;
	    }
	    kp->starttick = -1;

	    break;
	case TSTAMP_EV_EOSWITCH:	/* switched in */
		kp->pri = pri;
		kp->rtpri = basepri;
		if (kp->queuedtick != -1 && now > kp->queuedtick) {
				/*
				 * NB: We cannot properly attribute queue time
				 *     if we lose the set on run q event that
				 *     tells us which queue the process is on.
				 */
			if (kp->curq == &runq || kp->curq == NULL)
				kp->queuedtime += now - kp->queuedtick;
			else
				kp->lqueuedtime += now - kp->queuedtick;
		}
		kp->slices++;
		kp->starttick = now;
		kp->queuedtick = -1;
		if (kp->lastrun != -1 && kp->lastrun != ev->cpu)
			kp->cpuswitch++;
		kp->lastrun = ev->cpu;
		if (crp->wentidle > 0) {
			if (! (now >= crp->wentidle))
				now = crp->wentidle;
			crp->idleticks += now - crp->wentidle;
			crp->wentidle = 0;
		}
		crp->curpri = getpri(kp);
		if (crp->lastkid == kp->kid)
			crp->runsame++;
		crp->curkid = kp->kid;
		crp->disps++;
		if (kp->curq != NULL) {
			if (kp != (struct kidrec*) kp->curq->h.next)
				crp->nothighest++;
			if (kp->curq != &runq)	/* processor affinity win */
				crp->hotdisps++;
			Qremove(kp);
		}
		break;
    }
    rtmon_printEvent(rs, outf, ev);
    if (Qflag > 2)
	    showstat();
}

/*
 * Handle process termination event; if process
 * was running complete run time calculation.
 */
void
kidexit(const tstamp_event_entry_t* ev)
{
    struct kidrec* kp = getkid(ev->qual[0]);
    assert(kp != NULL);
    if (kp->starttick != -1) {
	ptime_t runtime;
	ptime_t now = ev->tstamp;
	if (! (now >= kp->starttick))
	    now = kp->starttick;
	runtime = now - kp->starttick;
	kp->runtime += runtime;
	kp->starttick = -1;
    }
    if (Qflag && kp->traced) {
	/*
	 * Sigh, if we're going to generate a summary
	 * we must copy the process name string from
	 * the library because it'll free it when the
	 * process terminates.
	 */
	kp->name = strdup(rtmon_kidLookup(&rs->ds, kp->kid));
    }
}

#define	NZ(x)	((x) ? (x) : 1)
#define	PCT(a,b)	((100.0 * (double) (a)) / (double)NZ((a) + (b)))

void
cpusummary(void)
{
    register int i;
    register struct cpurec *cp;
    register struct kidrec *kp;

    for (i = 0, cp = cpus; i < RTMOND_MAXCPU; i++, cp++) {
	if (cp->idletimes == 0 && cp->disps == 0 && cp->swtchs == 0)
	    continue;
	/*
	 * Tally the final execution segment.
	 */
	if (cp->wentidle > 0 && cp->wentidle <= msecs) {
	    assert(cp->curpri == PIDLE);
	    cp->idleticks += msecs - cp->wentidle;
	} else if (cp->curkid > 0) {
	    kp = getkid(cp->curkid);
	    assert(kp != NULL);
	    if (kp->starttick != -1 && kp->starttick <= msecs)
		cp->runticks += msecs - kp->starttick;
	}
	fprintf(outf, "\nCPU %d summary:\n", i);
	fprintf(outf, "  Total mS idle        = %lld\n",
	    rtmon_toms(rs,cp->idleticks));
	fprintf(outf, "  Total mS running     = %lld (saturation = %0.1f%%)\n",
	    rtmon_toms(rs,cp->runticks),
	    PCT(cp->runticks, cp->idleticks));
	fprintf(outf, "  Processes dispatched = %d (local %d global %d)\n",
	    cp->disps, cp->hotdisps, cp->disps - cp->hotdisps);
	fprintf(outf, "  Ran same             = %d (%0.1f%%)\n",
	    cp->runsame, (100.0 * cp->runsame)/NZ(cp->disps));
	fprintf(outf, "  Ran not highest      = %d (%0.1f%%)\n",
	    cp->nothighest, (100.0 * cp->nothighest)/NZ(cp->disps));
    }

}

void
runqsummary(void)
{
    fprintf(outf, "\nRun queue summary:\n");
    fprintf(outf, "  Max queue len\t= %d\n", runq.qmaxlen);
}

void
printkidsum(register struct kidrec *kp)
{
	pid_t pid = 0;
#define	AVG(a,b)	((a) > 0 ? rtmon_tomsf(rs,(b)/(a)) : 0.0)
    if (!kp->traced)
	return;
    if (kp->starttick != -1 && kp->starttick <= msecs)
	kp->runtime += msecs - kp->starttick;
    else if (kp->queuedtick != -1 && kp->queuedtick <= msecs) {
	if (kp->curq == &runq || kp->curq == NULL)
	    kp->queuedtime += msecs - kp->queuedtick;
	else
	    kp->lqueuedtime += msecs - kp->queuedtick;
    } else if (kp->sleeptick != -1 && kp->sleeptick <= msecs)
	kp->sleeptime += msecs - kp->sleeptick;
    pid = rtmon_pidLookup(&rs->ds, kp->kid);
    if (rs->flags & RTPRINT_SHOWKID)
	    fprintf(outf, "\nProcess summary for %s (pid:kid %d:%lld):\n",
		    kp->name != NULL ? kp->name : rtmon_kidLookup(&rs->ds, kp->kid),
		    pid, kp->kid);
    else
	    fprintf(outf, "\nProcess summary for %s (%s %lld):\n",
		    kp->name != NULL ? kp->name : rtmon_kidLookup(&rs->ds, kp->kid),
		    (pid != 0) ? "pid" : "kid",
		    (pid != 0) ? pid : kp->kid);

    fprintf(outf, "  Time slices %d\n", kp->slices);
    fprintf(outf, "  Total run mS %lld (avg mS/slice %0.2f)\n",
	rtmon_toms(rs,kp->runtime), AVG(kp->slices, kp->runtime));
    if (rtmon_ncpus(rs) > 1) {
	fprintf(outf, "  Times queued %d (local %d global %d)\n",
	    kp->queued+kp->lqueued, kp->lqueued, kp->queued);
	fprintf(outf, "  Total mS queued locally %llu (avg residence %0.2f)\n",
	    rtmon_toms(rs,kp->lqueuedtime), AVG(kp->lqueued, kp->lqueuedtime));
	fprintf(outf, "  Total mS queued globally %llu (avg residence %0.2f)\n",
	    rtmon_toms(rs,kp->queuedtime), AVG(kp->queued, kp->queuedtime));
    } else {
	fprintf(outf, "  Times queued %d\n", kp->queued+kp->lqueued);
	fprintf(outf, "  Total mS queued %llu (avg queue residence %0.2f)\n",
	    rtmon_toms(rs,kp->queuedtime+kp->lqueuedtime),
	    AVG(kp->lqueued+kp->queued, kp->lqueuedtime+kp->queuedtime));
    }
    fprintf(outf, "  Total mS sleeping %llu (avg wait time %0.2f)\n",
	rtmon_toms(rs,kp->sleeptime), AVG(kp->sleep, kp->sleeptime));
    fprintf(outf, "  Total context switches %d (counts by reason for swtch):\n",
	kp->swtch);
    fprintf(outf, "    preempt short\t%d (%0.1f%%)\n", kp->preempt_short,
	(100.0*kp->preempt_short)/NZ(kp->swtch));
    fprintf(outf, "    preempt long\t%d (%0.1f%%)\n", kp->preempt_long,
	(100.0*kp->preempt_long)/NZ(kp->swtch));
    fprintf(outf, "    sleep\t\t%d (%0.1f%%)\n", kp->sleep,
	(100.0*kp->sleep)/NZ(kp->swtch));
    fprintf(outf, "    yield\t\t%d (%0.1f%%)\n", kp->yield,
	(100.0*kp->yield)/NZ(kp->swtch));
    fprintf(outf, "    mustrun\t\t%d (%0.1f%%)\n", kp->mustrun,
	(100.0*kp->mustrun)/NZ(kp->swtch));
#undef AVG
}

void
kidsummary(void)
{
    kidwalk(printkidsum);
}

void
schedsummary(void)
{
    cpusummary();
	runqsummary();
	kidsummary();
}

void
dumpcpushort(void)
{
    register int i;
    register struct cpurec *cp;

    fprintf(outf, "  CPU Status:");
    for (i = 0, cp = cpus; i < RTMOND_MAXCPU; i++, cp++) {
	pid_t pid;
	if (cp->idletimes == 0 && cp->disps == 0 && cp->swtchs == 0)
	    continue;
	if (cp->curpri == PIDLE) {
	    fprintf(outf, "  [%d] IDLE", i);
	    continue;
	}
	pid =  rtmon_pidLookup(&rs->ds, cp->curkid);
	if (rs->flags & RTPRINT_SHOWKID) 
		fprintf(outf, "  [%d] %d:%lld(pri %d)", i, pid, cp->curkid, cp->curpri);
	else
		fprintf(outf, "  [%d] %lld(pri %d)", i, pid ? (int64_t)pid :  cp->curkid, cp->curpri);
    }
    fprintf(outf, "\n");
}

void
dumprunq(void)
{
    register int i;
    
    fprintf(outf, "  Run Queue:");

    for (i = 0; i <= RTMOND_MAXCPU; i++) {
	register struct kidlist *lp;
	register struct kidlist *kp;

	lp = (i == RTMOND_MAXCPU) ? &runq.h : &localqs[i].h;
	if (!lp || lp->next == lp)
	    continue;

	if (i == RTMOND_MAXCPU)
	    fprintf(outf, "  [G]");
	else
	    fprintf(outf, "  [%d]", i);

	for (kp = lp->next; kp != lp; kp = kp->next) {
	    pid_t pid;
	    struct kidrec* pkp = (struct kidrec*) kp;
	    
	    pid = rtmon_pidLookup(&rs->ds, pkp->kid);
	    
	    if (rs->flags & RTPRINT_SHOWKID) 
		    fprintf(outf, " %d:%lld(pri %d)", pid, pkp->kid, getpri(pkp));
	    else
		    fprintf(outf, " %lld(pri %d)", pid ? pid : pkp->kid, getpri(pkp));
	}
    }

    fprintf(outf, "\n");
}

void
showstat(void)
{
    dumpcpushort();
    dumprunq();
}

void
usage(char *progname)
{
    fprintf(errf,
	"Usage: %s <functions> <options> <command + args>\n"
	"       %s <functions> <options> -p pid\n"
	"       %s <functions> <options> -t time\n"
	"       %s <Display Options> <logfile\n",
	progname, progname, progname, progname);
    fprintf(errf,
	"Collection Functions:\n"
	"       -s              collect system call events\n"
	"       -r              collect process scheduling events\n"
	"       -k              collect disk activity events\n"
	"       -i              inherit event collection on fork\n"
	"       -O file         write event data to file\n"
#ifdef NET_DEBUG
	"       -x              collect network queueing events\n"
	"       -X              collect network throughput events\n"
#endif
	);
    fprintf(errf,
	"Display Options:\n"
	"       -l              list system calls in long format\n"
	"       -n syscall      select system call by name or number\n"
	"       -e syscall      exclude system call by name or number\n"
	"       -P pid          list system calls for <pid> only\n"
	"       -d              show time delta when printing calls\n"
	"       -c              do not show cpu id when printing calls\n"
	"       -u              show delta time in uSec\n"
	"	-k		print disk I/O trace (same as collection flag)\n"
	"       -S              print system call summary\n"
	"       -SS             print system call trace\n"
	"       -Q              print scheduling summary\n"
	"       -QQ             print scheduling trace\n"
	"       -QQQ            print runq trace also\n"
	"       -o file         write analysis listing to file\n"
	"       -a len          max bytes to print for ascii data\n"
	"       -b len          max bytes to print for binary data\n"
	"       -A              force character data printing\n"
	"       -B              force hex data printing\n"
	"       -z              sort system call summary by syscall name\n"
	);
}

void
dumperrors(char *msg)
{
    const char* emsg = strerror(errno);
    fflush(outf);
    fflush(errf);
    error("%s: %s", msg, emsg);
    if (collect)
	pclose(nfd);
    exit(1);
}
