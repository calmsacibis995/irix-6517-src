/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * rtrun [-t secs] [-i] [-p pid] ...
 */

#include <sys/types.h>
#include <sys/par.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/rtmon.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>

#define	PAR	"/dev/par"
#define	MAXPIDS 1000

char*	pname;
int	dev;
pid_t	pids[MAXPIDS];
int	npids = 0;
int	leave;

static void dosetup(int dev, __int64_t, int op);
static void docleanup(int dev, __int64_t);
static void usage(void);
static void verror(const char* fmt, va_list ap);
static void _perror(const char* fmt, ...);
static void fatal(const char* fmt, ...);
static void pfatal(const char* fmt, ...);

static void sigcatcher(void)	{}

int
main(int argc, char **argv)
{
    int	tlen = 0;
    int	c;
    int inherit = 0;
    uint64_t cookie = 0;
    uint64_t event_mask = RTMON_SYSCALL; /* compatibility */

    pname = argv[0];
    while ((c = getopt(argc, argv, "c:ip:rst:")) != EOF) {
	switch (c) {
	case 'c':
	    cookie = strtoull(optarg, NULL, 0);
	    break;
	case 'i':			/* inherit children */
	    inherit = 1;
	    break;
	case 'r':
	    event_mask |= RTMON_TASKPROC;
	    break;
	case 's':
	    event_mask |= RTMON_SYSCALL;
	    break;
	case 'p':
	    pids[npids++] = (pid_t) strtol(optarg, (char**) 0, 0);
	    break;
	case 't':
	    tlen = (int) strtol(optarg, (char **)NULL, 0);
	    if (tlen == 0)
		fatal("-t needs a > 0 numeric value");
	    break;
	default:
	    usage();
	    break;
	}
    }
    if (cookie == 0)
	fatal("No par cookie specified (do you know what you're doing?)");

    dev = open(PAR, O_RDONLY);
    if (dev == -1)
	pfatal("%s: open", PAR);

    sigset(SIGHUP, sigcatcher);
    sigset(SIGINT, sigcatcher);
    sigset(SIGQUIT, sigcatcher);
    sigset(SIGTERM, sigcatcher);

    dosetup(dev, cookie,
	    ((event_mask & RTMON_SYSCALL)  ? PAR_SYSCALL : 0) |
	    ((event_mask & RTMON_TASKPROC) ? PAR_SWTCH   : 0) |
	    (inherit                       ? PAR_INHERIT : 0));

    if (tlen) {
	sigset(SIGALRM, sigcatcher);
	alarm(tlen);
    }
    pause();
    docleanup(dev, cookie);

    exit(0);
    /* NOTREACHED */
}

/*
 * Setup system call tracing for the specified processes.
 * In actuality these calls just enable return of indirect
 * parameter information that is only returned when the
 * process(es) are marked specially.
 */
static void
dosetup(int dev, __int64_t cookie, int op)
{
    struct {
	__int64_t	cookie;
	pid_t		pid;
    } args;

    args.cookie = cookie;
    if (npids > 0) {
	int i;
	for (i = 0; i < npids; i++) {
	    args.pid = pids[i];
	    if (ioctl(dev, op, &args) == -1)  {
		_perror("Cannot enable system call tracing for pid %d",
		    pids[i]);
		if (errno != ESRCH || npids == 1)
		    exit(EXIT_FAILURE);
		pids[i] = 0;
	    }
	}
    } else {
	args.pid = (pid_t) -2;
	if (ioctl(dev, op, &args) == -1)
	    pfatal("Cannot enable global system call tracing");
    }
}

/*
 * Cleanup any system call tracing work done above.
 */
static void
docleanup(int dev, __int64_t cookie)
{
    struct {
	__int64_t	cookie;
	pid_t		pid;
    } args;

    args.cookie = cookie;
    if (npids > 0) {
	int i;
	for (i = 0; i < npids; i++) {
	    args.pid = pids[i];
	    if (pids[i] && ioctl(dev, PAR_DISABLE, &args) == -1 && errno != ESRCH)
		_perror("Cannot disable tracing for pid %d", pids[i]);
	}
    } else {
	args.pid = (pid_t) -2;
	if (ioctl(dev, PAR_DISABLE, &args) == -1)
	    pfatal("Cannot disable global tracing");
    }
}

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-i] [-t time] [-c cookie] [-p pid]*\n", pname);
    fprintf(stderr, "%s\n%s\n%s\n%s\n", 
    "       -i              trace system calls across forks",
    "       -c cookie       specify collector cookie",
    "       -t time         trace for 'time' seconds",
    "       -p pid          trace specific pid (more than 1 allowed)");
    exit(EXIT_FAILURE);
}

static void
verror(const char* fmt, va_list ap)
{
    fprintf(stderr, "%s: ", pname);
    vfprintf(stderr, fmt, ap);
}

static void
_perror(const char* fmt, ...)
{
    const char* emsg = strerror(errno);
    va_list ap;
    va_start(ap, fmt);
    verror(fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", emsg);
}

static void
fatal(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror(fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static void
pfatal(const char* fmt, ...)
{
    const char* emsg = strerror(errno);
    va_list ap;
    va_start(ap, fmt);
    verror(fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", emsg);
    exit(EXIT_FAILURE);
}
