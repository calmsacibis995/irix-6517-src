/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1997, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * rtmond server-related stuff.
 */
#include "rtmond.h"
#include "timer.h"

#include <stdlib.h>
#include <unistd.h>
#include <paths.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/systeminfo.h>
#include <sched.h>
#include <sys/lock.h>
#include <sys/param.h>

#include <sys/par.h>				/* for misc. ioctls */

#define	_PATH_DEVPAR		"/dev/par"

static	int numb_processors;
static	int debug = 0;				/* 1 => don't detach from tty */
static	int allow_tracing = FALSE;		/* allow tracing of threads */
static	struct sched_param param = { _CONFIG_DEFPRI };	/* rt scheduling pri */
int	trace = _CONFIG_DEFTRACE;		/* diag tracing control */
int	dochecksum = 0;				/* checksum over event data */

static	const char* shm_file = RTMON_SHM_FILENAME;/* shm file for app events */
static	const char* pid_file = _PATH_PID_FILENAME;/* server lock file */
static	const char* devpar_file = _PATH_DEVPAR;	/* special for doing ioctls */
static	int devpar_fd = -1;			/* open /dev/par file */

static	int holdingLock = FALSE;		/* holding server lock file */
static	daemon_info_t* daemon_info;		/* per-cpu thread state */
static	void* user_queue_start = MAP_FAILED;	/* mmap'd shared user event q */
static	size_t user_queue_size;

static	const char *myname;

static	uint parse_trace_str(const char* arg);
static	void lockDaemon(void);
static	void detachFromTTY(void);
static	int init_nproc(void);
static	void init_daemon_info(void);

extern	const char* access_file;
extern	const char* global_access;
extern	int maxiobufs;
extern	size_t iobufsiz;
extern	int waittime;
extern	int quiettime;

static void
cleanup()
{
    if (user_queue_start != MAP_FAILED)
	(void) munmap(user_queue_start, user_queue_size);
    (void) unlink(shm_file);
    network_cleanup();
    if (holdingLock)
	(void) unlink(pid_file);
}

static void
sigINT()
{
    cleanup();
    exit(EXIT_SUCCESS);
}

static void
usage()
{
    fprintf(stderr, "usage: %s [-a access] [-b iobufsiz] [-c] [-d] [-f file] [-i maxiobufs] [-l] [-p priority] [-q quiet-time] [-t trace] [-w wait-time] [-z] [-P port] [-U sockname]\n", myname);
    exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
    struct sigaction act;
    int c;
    int lockServer = FALSE;
    const char* sockname = NULL;
    int port = 0;

    myname = strrchr(argv[0], '/');
    if (myname != NULL)
	myname++;
    else
	myname = argv[0];

    while ((c = getopt(argc, argv, "a:b:cdf:i:lp:q:t:uw:zP:U:?")) != -1)
	switch(c) {
	case 'a':
	    global_access = optarg;
	    break;
	case 'b':
	    iobufsiz = (size_t) strtoul(optarg, NULL, 0);
	    break;
	case 'c':
	    dochecksum = 1;
	    break;
	case 'd':
	    debug = 1;
	    break;
	case 'f':
	    access_file = optarg;
	    break;
	case 'i':
	    maxiobufs = atoi(optarg);
	    if (maxiobufs < 2) {
		fprintf(stderr, "%s: Bad max number of i/o buffers \"%s\";"
		    " must be at least 2.\n", myname, optarg);
		exit(EXIT_FAILURE);
	    }
	    break;
	case 'l':
	    lockServer = TRUE;
	    break;
	case 'p':
	    param.sched_priority = atoi(optarg);
	    break;
	case 'q':
	    quiettime = atoi(optarg);
	    break;
	case 't':
	    trace = parse_trace_str(optarg);
	    break ;
	case 'u':
	    /* silently accept old flag for compatibility */
	    break;
	case 'w':
	    waittime = atoi(optarg);
	    break;
	case 'z':
	    allow_tracing = TRUE;
	    break;
	case 'P':
	    port = atoi(optarg);
	    break;
	case 'U':
	    sockname = optarg;
	    break;
	case '?':
	default :
	    usage();
	}

    if (!debug)
	detachFromTTY();
    /*
     * Interlock against running multiple daemon processes;
     * this must be done after we detach from the tty because
     * we write our PID to the lock file for others to see
     * (and in detaching we fork).
     */
    lockDaemon();

    init_network(port, sockname);

    sigemptyset(&act.sa_mask);
#ifdef USE_SPROC
    act.sa_flags = SA_NOCLDWAIT;
    act.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &act, NULL);		/* reap children w/o handler */
#endif
    act.sa_flags = 0;
    act.sa_handler = sigINT;
    sigaction(SIGINT, &act, NULL);		/* cleanup on interrupt */

    /*
     * Ignore SIGPIPE; instead we catch write errors when a
     * remote client goes away.  NB: the async i/o support
     * assumes this is inherited via sproc/pthread_create.
     */
    act.sa_flags = 0;
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    sigaction(SIGPIPE, &act, NULL);

    /*
     * Initialize shared state that is inherited
     * across sproc's to various threads; then
     * run the main loop that listens for requests
     * that initiate work on behalf of clients.  Note
     * that the mmap'd user queue area must be
     * setup prior to filling in the daemon info
     * data structures.
     */
    numb_processors = init_nproc();
    map_timer();				/* map RTC state */
    init_daemon_info();				/* per-thread state */
    init_client();				/* client global state */
    init_thread();				/* tstamp global state */
    init_io();					/* async push global state */

    if (lockServer && plock(PROCLOCK) < 0)
	Log(LOG_WARNING, NULL, "Cannot lock process in memory: %s",
	    strerror(errno));
    /*
     * Register client-server protocol support.
     */
    protocol2_init();				/* new native event protocol */

#ifdef USE_SPROC
    /*
     * Set realtime priority and FIFO scheduling poilicy
     * here so we have the same priority as each event
     * collection thread.  It is necessary for all the
     * threads to have the same policy to insure they
     * get proper access to the per-thread client list when
     * adding and removing clients (the event collection
     * threads leave a window for us to get in each time
     * through the main collection loop, but this can only
     * work if we can get to run when they unlock the list).
     */
    if (param.sched_priority) {
	IFTRACE(DEBUG)(NULL,
	    "Set realtime scheduling: SCHED_FIFO policy, priority %d",
	    param.sched_priority);
	if (sched_setscheduler(0, SCHED_FIFO, &param)  < 0)
	    Log(LOG_WARNING, NULL, "Cannot set priority to %d: %s",
		param.sched_priority, strerror(errno));
    }
#endif

    network_run();
    /*NOTREACHED*/
}

static void
trace_syntax(const char* what)
{
    Fatal(NULL, "syntax error in event list; %s", what);
}

static uint
trace_mask_name(const char* tp, int n)
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
    static struct maskname {
	const char* name;
	uint	mask;
    } masks[] = {
	{ "client",	TRACE_CLIENT },
	{ "rpc",	TRACE_RPC },
	{ "perf",	TRACE_PERF },
	{ "thread",	TRACE_THREAD },
	{ "events",	TRACE_EVENTS },
	{ "eventio",	TRACE_EVENTIO },
	{ "tstamp",	TRACE_TSTAMP },
	{ "sync",	TRACE_SYNC },
	{ "kid",	TRACE_KID },
	{ "access",	TRACE_ACCESS },
	{ "lostevents",	TRACE_LOSTEVENTS },
	{ "debug",	TRACE_DEBUG },

	{ "none",	0 },
	{ "all",	(uint) -1 },
    };
    struct maskname* mp;

    for (mp = masks; mp < &masks[N(masks)]; mp++)
	if (strncasecmp(mp->name, tp, n) == 0)
	    return (mp->mask);
    Fatal(NULL, "%.*s: Uknown trace mask name", n, tp);
    /*NOTREACHED*/
#undef N
}

static uint
parse_trace_str(const char* arg)
{
    const char* cp = arg;
    uint mask = 0;
    int incl = 1;
    while (*cp) {
	int m, n;

	if (isalpha(*cp)) {
	    const char* tp = cp;
	    do {
		*cp++;
	    } while (isalpha(*cp));
	    if (incl)
		mask |= trace_mask_name(tp, cp-tp);
	    else
		mask &= ~trace_mask_name(tp, cp-tp);
	} else
	    trace_syntax("expecting trace mask name");
	if (*cp == ',' || *cp == '|' || *cp == '+' || *cp == '-')
	    incl = (*cp++ != '-');
	else if (*cp != '\0')
	    trace_syntax("expecting separator (e.g. ',') after trace mask name");
    }
    return (mask);
}

static void
event_syntax(const char* what)
{
    Fatal(NULL, "syntax error in event list; %s", what);
}

static uint64_t
event_mask_name(const char* tp, int n)
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
    static struct maskname {
	const char*	name;
	uint64_t	mask;
    } masks[] = {
	{ "debug",	RTMON_DEBUG },
	{ "signal",	RTMON_SIGNAL },
	{ "syscall",	RTMON_SYSCALL },
	{ "task",	RTMON_TASK },
	{ "taskproc",	RTMON_TASKPROC },
	{ "intr",	RTMON_INTR },
	{ "framesched",	RTMON_FRAMESCHED },
	{ "profile",	RTMON_PROFILE },
	{ "vm",		RTMON_VM },
	{ "scheduler",	RTMON_SCHEDULER },
	{ "disk",	RTMON_DISK },
	{ "netsched",	RTMON_NETSCHED },
	{ "netflow",	RTMON_NETFLOW },
	{ "alloc",	RTMON_ALLOC },

	{ "all",	RTMON_ALL },
	{ "network",	RTMON_NETSCHED|RTMON_NETFLOW },
	{ "io",		RTMON_DISK|RTMON_INTR },
	{ "none",	0 },
    };
    struct maskname* mp;
    uint64_t maskval;
    char *sval;

    sval = (char *) malloc(n + 1);
    if (sval == NULL) {
	    Fatal(NULL,"cannot allocate working storage");
	    /*NOTREACHED*/
    }
    strncpy(sval,tp,n);
    sval[n] = 0;
    for (mp = masks; mp < &masks[N(masks)]; mp++)
	    if (strcasecmp(mp->name, sval) == 0) {
		    free((void *) sval);
		    return (mp->mask);
	    }
    if (sscanf(sval,"%lli",&maskval) == 1) {
	    free((void *) sval);
	    return(maskval);
    }
    free((void *) sval);
    Fatal(NULL, "%.*s: Uknown event mask name", n, tp);
    /*NOTREACHED*/
#undef N
}

uint64_t
parse_event_str(const char* arg)
{
    const char* cp = arg;
    uint64_t mask = 0;
    int incl = 1;
    while (*cp) {
	int m, n;

	if (isalnum(*cp)) {
	    const char* tp = cp;
	    do {
		*cp++;
	    } while (isalnum(*cp));
	    if (incl)
		mask |= event_mask_name(tp, cp-tp);
	    else
		mask &= ~event_mask_name(tp, cp-tp);
	} else
	    event_syntax("expecting event mask name");
	if (*cp == ',' || *cp == '|' || *cp == '+' || *cp == '-')
	    incl = (*cp++ != '-');
	else if (*cp != '\0')
	    event_syntax("expecting ',' after event mask name");
    }
    return (mask);
}

/*
 * Check the daemon lock file to see if another process
 * is already running, if so issue a message and exit;
 * otherwise write our PID for others to see.
 */
static void
lockDaemon(void)
{
    int fd = open(pid_file, O_RDWR|O_CREAT, 0644);
    if (fd < 0)
	Fatal(NULL, "%s: %s", pid_file, strerror(errno));
    if (flock(fd, LOCK_EX) >= 0) {
	char pidbuf[20];
	pid_t pid;
	int n = read(fd, pidbuf, sizeof (pidbuf)-1);
	if (n > 0) {
	    pidbuf[n] = '\0';
	    pid = (pid_t) strtoul(pidbuf, 0, 10);
	    if (kill(pid, 0) == 0 || errno != ESRCH)
		Fatal(NULL, "Server appears to be running; pid %s", pidbuf);
	    else
		Log(LOG_WARNING, NULL, "%s: Purging old lock file; pid was %lu",
		    pid_file, (unsigned long) pid);
	}
	holdingLock = TRUE;			/* from here on, do cleanup */
	sprintf(pidbuf, "%.*lu", sizeof (pidbuf)-1, (u_long) getpid());
	n = strlen(pidbuf);
	if (lseek(fd, 0L, SEEK_SET) != 0 || write(fd, pidbuf, n) != n)
	    Fatal(NULL, "%s: Error writing pid to lock file; %s",
		pid_file, strerror(errno));
	(void) close(fd);			/* NB: implicit unlock */
    } else
	Fatal(NULL, "%s: Unable to lock file; %s",
	    pid_file, strerror(errno));
}

static int
open_devpar(void)
{
    if (devpar_fd == -1) {
	devpar_fd = open(devpar_file, O_RDWR);
	if (devpar_fd < 0) {
	    Log(LOG_WARNING, NULL, "Cannot open %s for PAR_DISALLOW call: %s",
		devpar_file, strerror(errno));
	    return (FALSE);
	}
    }
    return (TRUE);
}

/*
 * Disallow par system call tracing for the server
 * thread; this is important as we can generate a
 * lot of event traffic when global tracing is enabled.
 */
void
disallow_tracing(daemon_info_t* dp)
{
    if (allow_tracing)
	return;
    if (open_devpar() && ioctl(devpar_fd, PAR_DISALLOW, 0) < 0)
	Log(LOG_WARNING, dp, "Cannot disable par tracing: %s", strerror(errno));
}

/*
 * Set the max number of indirect bytes to return
 * with system call events.  This is set to reflect
 * the max over all clients since the kernel only
 * keeps one system-wide setting.
 */
int
setmaxindbytes(int max)
{
    if (open_devpar() && ioctl(devpar_fd, PAR_SETMAXIND, max) < 0) {
	Log(LOG_WARNING, NULL, "Cannot set max indirect bytes to %d: %s",
	    max, strerror(errno));
	return (TRUE);
    } else
	return (FALSE);
}

/*
 * Detach the process from the controlling tty.
 */
static void
detachFromTTY(void)
{
    int fd = open(_PATH_DEVNULL, O_RDWR);
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    switch (fork()) {
    case 0:	break;
    case -1:	Fatal(NULL, "Could not fork: %s", strerror(errno));
		_exit(EXIT_FAILURE);
    default:	_exit(EXIT_SUCCESS);
    }
    (void) setsid();
    openlog(myname, LOG_PID|LOG_CONS, LOG_DAEMON);
}

/*
 * Retrieve, and verify, the number of processors.
 */ 
static int
init_nproc(void)
{
    char buf[32];
    int nproc;

    if (sysinfo(_MIPS_SI_NUM_PROCESSORS, buf, sizeof (buf)) < 0)
	Fatal(NULL, "sysinfo(_MIPS_SI_NUM_PROCESSORS): %s", strerror(errno));
    nproc = atoi(buf);
    if (nproc > RTMOND_MAXCPU) {
	Log(LOG_WARNING, NULL,
	    "Only able to trace cpus 0-%d of %d cpus present with protocols 1 & 2",
	    RTMOND_MAXCPU, nproc);
	nproc = RTMOND_MAXCPU;
    }
    if (nproc < 1)
	Fatal(NULL, "Bogus number of processors: %s", buf);
#ifdef USE_SPROC
    /*
     * Tell libc usarena code to size waiter queues large enough to handle
     * the maximum number of sproc() threads we think we're ever going to
     * have.  Obviously it's impossible to pick such a value so we just try
     * to make sure we pick something halfway reasonable.  Allow for at least
     * _SPROC_MAX_CLIENTS or nproc client connections.  We need to allow for
     * at least nproc because kernprof starts up a separate client connection
     * for each CPU and rtmon-client has this attribute also if its -O option
     * is specified.
     */
    if (usconfig(CONF_INITUSERS, nproc + 1 + MAX(_SPROC_MAX_CLIENTS, nproc)) == -1)
	Fatal(NULL, "Cannot set max share group size: usconfig: %s",
	    strerror(errno));
#endif
    return (nproc);
}

/*
 * Setup the memory mapped file through which
 * the queue of user events is shared.
 */
static void
init_userq(void)
{
    int fd = open(shm_file, O_RDWR|O_CREAT, 0600);
    if (fd >= 0) {
	user_queue_size = numb_processors*sizeof(user_queue_t);
	user_queue_start = mmap(0, user_queue_size, PROT_WRITE|PROT_READ,
				MAP_SHARED|MAP_AUTOGROW, fd, 0);
	if (user_queue_start == MAP_FAILED)
	    Fatal(NULL, "mmap %s: %s", shm_file, strerror(errno));
	close(fd);
	memset(user_queue_start, '\0', user_queue_size);
    } else
	Fatal(NULL, "open %s: %s", shm_file, strerror(errno));
}


/*
 * Parse the next CPU identification in the
 * sysinfo string; return a token and update
 * the string reference to the next ID.
 */
static int
cpu_type(const char** cpp)
{
    const char* cp = *cpp;
    const char* tp;
    int cpu;

    while (isspace(*cp))
	cp++;
    tp = cp;
    for (; *cp && *cp != ',' && *cp != ' '; cp++)
	;
    if (cp - tp == 0) {
	/*
	 * The buffer used to hold sysinfo is probably too small.
	 * Check the value of PROC_LIST_BUFSZ against the number
	 * of processors and the name of the processor (including
	 * revision information.
	 */
	Fatal(NULL, "Missing processor name in sysinfo string");
    }

    if (strncmp(tp, "R3000", cp-tp) == 0)
	cpu = TSTAMP_CPUTYPE_R3000;
    else if (strncmp(tp, "R4000", cp-tp) == 0)
	cpu = TSTAMP_CPUTYPE_R4000;
    else if (strncmp(tp, "R4300", cp-tp) == 0)
	cpu = TSTAMP_CPUTYPE_R4300;
    else if (strncmp(tp, "R4400", cp-tp) == 0)
	cpu = TSTAMP_CPUTYPE_R4400;
    else if (strncmp(tp, "R4600", cp-tp) == 0)
	cpu = TSTAMP_CPUTYPE_R4600;
    else if (strncmp(tp, "R5000", cp-tp) == 0)
	cpu = TSTAMP_CPUTYPE_R5000;
    else if (strncmp(tp, "R8000", cp-tp) == 0)
	cpu = TSTAMP_CPUTYPE_R8000;
    else if (strncmp(tp, "R10000", cp-tp) == 0)
	cpu = TSTAMP_CPUTYPE_R10000;
    else if (strncmp(tp, "R12000", cp-tp) == 0)
	cpu = TSTAMP_CPUTYPE_R12000;
    else {
	Log(LOG_WARNING, NULL, "%.*s is an unknown processor name,"
	    " assigning MIPS", cp-tp, tp);
	cpu = TSTAMP_CPUTYPE_MIPS;
    }

    while (*cp && *cp != ',')
	cp++;
    *cpp = (*cp == ',' ? cp+1 : cp);
    return (cpu);
}


/*
 * Allow enough room for 15 characters of data for each CPU.
 * Data includes processor name, revision string, and white space.
 */
#define PROC_LIST_BUFSZ (RTMOND_MAXCPU*15)

/*
 * Initialize the "daemon" data structures.
 */
static void
init_daemon_info(void)
{
    char proc_list[PROC_LIST_BUFSZ+1];
    const char* cp;
    int cpu;

    init_userq();				/* must precede daemon_info */
    daemon_info = (daemon_info_t*)
	malloc(numb_processors*sizeof (daemon_info_t));
    if (daemon_info == NULL)
	Fatal(NULL, "Unable to allocate memory for CPU info (ncpu %u)",
	    numb_processors);

    sysinfo(_MIPS_SI_PROCESSORS, proc_list, sizeof (proc_list)-1);
    proc_list[sizeof (proc_list)-1] = '\0';
    cp = proc_list;
    for (cpu = 0; cpu < numb_processors; cpu++) {
	daemon_info_t* dp = &daemon_info[cpu];
	dp->isrunning = 0;
	dp->cpu = cpu;
	dp->cpu_type = cpu_type(&cp);
	dp->eobmode = 0;
	dp->mask = dp->omask = 0LL;
	dp->kern_count = dp->kern_index = 0;
	dp->user_queue = (user_queue_t*)
	    ((char*) user_queue_start + cpu*sizeof(user_queue_t));
	dp->user_count = dp->user_index = 0;
	dp->clients = NULL;
	sem_init(&dp->clients_sem, 0, 1);
	dp->paused = NULL;

	kid_init(dp);

    }
}

daemon_info_t*
getdaemoninfo(int cpu)
{
    assert(0 <= cpu && cpu < numb_processors);
    return (&daemon_info[cpu]);
}

uint
getncpu(void)
{
    return (numb_processors);
}

uint
getschedpri(void)
{
    return (param.sched_priority);
}

/*
 * Log a message to the terminal or syslog.
 */
static void
vlog(int pri, daemon_info_t* dp, const char* fmt, va_list ap)
{
    char buf[8*1024];
    if (debug) {
	/*
	 * Use a single write to standard error in order to avoid
	 * intermingling messages from multiple threads ...
	 */	 
	char* bp = buf;
	struct timeval tv;
	(void) gettimeofday(&tv, 0);
	bp += strftime(buf, sizeof (buf), "%h %d %T",
		    localtime((time_t*) &tv.tv_sec));
	bp += sprintf(buf+strlen(buf), ".%02u: %s[%5d]: ",
		    tv.tv_usec / 10000, myname,
#ifdef USE_SPROC
		    get_pid()
#else
		    pthread_self()
#endif
		    );
	if (dp)
	    bp += sprintf(bp, "(CPU %d) ", dp->cpu);
#ifdef notdef
	bp += vsnprintf(bp, sizeof (buf)-1 - (bp-buf), fmt, ap);
#else
	bp += vsprintf(bp, fmt, ap);
#endif
	*bp++ = '\n';
	write(STDERR_FILENO, buf, bp-buf);
    } else {
	if (dp) {
	    sprintf(buf, "(CPU %d) %s", dp->cpu, fmt);
	    vsyslog(pri, buf, ap);
	} else
	    vsyslog(pri, fmt, ap);
    }
}

/*
 * Normal logging interface
 */
void
Log(int pri, daemon_info_t* dp, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog(pri, dp, fmt, ap);
    va_end(ap);
}

/*
 * Tracing message interface
 */
void
Trace(daemon_info_t* dp, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog(LOG_DEBUG, dp, fmt, ap);
    va_end(ap);
}

/*
 * Log message and terminate; should not be
 * used in a sub-process/thread because it
 * cleans up master thread state.
 */
void
Fatal(daemon_info_t* dp, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog(LOG_ERR, dp, fmt, ap);
    va_end(ap);
#ifdef USE_SPROC
    if (!dp)				/* NB: don't cleanup in sub-threads */
	cleanup();
    exit(EXIT_FAILURE);
#else
    if (!dp) {
	cleanup();
	exit(EXIT_FAILURE);
    } else
	pthread_exit((void*) EXIT_FAILURE);
#endif
}
