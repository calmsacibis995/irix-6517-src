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
 * padc - read the process activity data from rtmond and write it
 *	to the standard output.
 *
 *	usage: padc [-srkxX] [-I max][-z][-h pri][-b bufsz][-pP pid]
 *	       padc [-srkxX] [-I max][-z][-h pri][-b bufsz] -t 'time'
 *
 *	-p - monitor the named process
 *	-P - monitor the named process which has been forked by par
 *      -s - trace system calls with parameters
 *      -i - turn on child inheritance of syscall trace
 *      -r - trace scheduler
 *      -t - trace for a specified time
 *	-I - specify # bytes to collect for indirect args
 *      -x - trace network queueing
 *      -X - trace network throughput
 *	-k - trace disk activity
 *	-h - set padc's priority
 *	-b - set buf size (0 -> line buffereing)
 *	-z - enable collection of padc w/ global syscall tracing
 */

#include <sys/types.h>
#include <rtmon.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/syssgi.h>
#include <sys/prctl.h>
#include <sched.h>
#include <sys/lock.h>
#include <string.h>
#include <bstring.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#include "par.h"		/* XXX for MAXPIDS */

#define	PAR	"/dev/par"
#define	BSIZE	((size_t)128*1024)

const char* pname;
int	dev = -1;
pid_t	pids[MAXPIDS];
int	wakekid[MAXPIDS];
int	npids = 0;
size_t	obufsz = -1;		/* requested buffer size */
size_t	occ;
int	linebuffer;
char*	obuf;
int	leave;
int	numb_processors;
int	debug = 0;
rtmond_t* rt = NULL;
int	inherit = 0;
struct sched_param param = { -1 };	/* rt scheduling pri */

static int merge(uint64_t, const tstamp_event_entry_t*[], int npending[]);
static void dowrite(int fd, const void *buf, size_t len);
static int doread(int fd, char* data, size_t cc0);
static void dosetup(int dev, __int64_t, int op);
static void docleanup(int dev, __int64_t);
static int trackproc(const tstamp_event_entry_t*, size_t n);
static void usage(void);
static void verror(const char* fmt, va_list ap);
static void _perror(const char* fmt, ...);
static void fatal(const char* fmt, ...);
static void pfatal(const char* fmt, ...);
static void parse_proc_str(const char* arg, uint64_t[]);
static void checksum(int cpu, const tstamp_event_entry_t*, size_t, uint64_t);

static void sigcatcher(void)	{ leave++; }
static void sigalarm(void)	{ leave++; }

int
main(int argc, char **argv)
{
    int	err = 0;
    int	tlen = 0;
    int	c, i;
    int maxbytes = -1;
    uint64_t event_mask = 0;
    const char* cpu_mask_str = "0-";
    uint64_t cpu_mask[RTMOND_MAXCPU>>6];
    int datafd;
    int nactive;
    int zflag = 0;
    const tstamp_event_entry_t* pending[RTMOND_MAXCPU];
    int npending[RTMOND_MAXCPU];
    char* data[RTMOND_MAXCPU];
    size_t datalen[RTMOND_MAXCPU];
    uint64_t tlast[RTMOND_MAXCPU];
    int cpu;

    pname = argv[0];
    while ((c = getopt(argc, argv, "c:kI:iDb:h:P:p:srt:xXz")) != EOF) {
	switch (c) {
	case 'c':
	    cpu_mask_str = optarg;
	    break;
	case 'k':
	    event_mask |= RTMON_DISK;
	    break;
	case 'I':
	    maxbytes = atoi(optarg);
	    if (maxbytes < 0)
		    maxbytes = -1;
	    break;
	case 'i':			/* inherit children */
	    inherit = 1;
	    break;
	case 'D':
	    debug++;
	    break;
	case 'b':			/* for compatibility */
	    obufsz = atoi(optarg);
	    break;
	case 'h':
	    param.sched_priority = atoi(optarg);
	    break;
	case 'P':
	case 'p':
	    pids[npids] = (pid_t) strtol(optarg, (char**) 0, 0);
	    wakekid[npids] = (c == 'P');
	    npids++;
	    break;
	case 's':
	    event_mask |= RTMON_SYSCALL|RTMON_SIGNAL;
	    break;
	case 'r':
	    event_mask |= RTMON_TASKPROC;
	    break;
	case 't':
	    tlen = (int) strtol(optarg, (char **)NULL, 0);
	    if (tlen == 0)
		fatal("-t needs a > 0 numeric value");
	    break;
	case 'x':
	    event_mask |= RTMON_NETSCHED;
	    break;
	case 'X':
	    event_mask |= RTMON_NETFLOW;
	    break;
	case 'z':
	    zflag = 1;
	    break;
	default:
	    err++;
	    break;
	}
    }
    if (event_mask == 0) {
	fprintf(stderr, "%s: type of monitoring not specified\n", pname);
	err++;
    }
    if (npids && (event_mask & RTMON_SYSCALL|RTMON_TASK|RTMON_DISK) == 0) {
	fprintf(stderr, "%s: no specified options require a pid\n", pname);
	err++;
    }
    if (err) {
	usage();
	/* NOTREACHED */
    }
    if (npids == 0 && (event_mask & RTMON_TASKPROC)) {
	/*
	 * If -r was specified with no pids then act as if task scheduling
	 * information was asked for the entire system (-q) in order to
	 * emulate the old padc behavior.
	 */
	event_mask &= ~RTMON_TASKPROC;
	event_mask |= RTMON_TASK;
    }

    if (linebuffer = (obufsz == 0))
	obufsz = BSIZE;
    else if (obufsz == (size_t) -1)
	obufsz = BSIZE;
    obuf = (char*) malloc(obufsz);
    if (obuf == NULL)
	fatal("Cannot allocate %ld byte buffer", (long) obufsz);
    occ = 0;

    dev = open(PAR, 0);
    if (dev == -1)
	pfatal("%s: open", PAR);

    rt = rtmond_open("", 0);	/* local connection to server */
    if (!rt) {
	if (errno == EPERM)
	    fatal("Cannot reach rtmon service on local host, access denied");
	else
	    fatal("Unable to contact rtmon service on local host.");
    }
    numb_processors = rt->ncpus;

    if (param.sched_priority == -1)
	param.sched_priority = rt->schedpri;
    if (param.sched_priority) {
	/* NB: can't be fatal since only root can do this */
	if (sched_setscheduler(0, SCHED_RR, &param) < 0 && debug)
	    fprintf(stderr, "Cannot set scheduling priority to %u: %s",
		param.sched_priority, strerror(errno));
    }
    (void) plock(PROCLOCK);

    parse_proc_str(cpu_mask_str, cpu_mask);

    if (npids > 0) {
	/*
	 * Data is delivered by the server in-order for
	 * each CPU and we must merge the streams into a
	 * single time-ordered stream of events.  To do
	 * this we buffer events until we can safely merge
	 * events w/o worrying about receiving any more
	 * that will need to be merged.
	 *
	 * For buffering we allocate an initial (nominal)
	 * amount of space.  If more space is needed, this
	 * buffer is grown below.  For a 128P system this
	 * nominal allocation size causes ~1.2MB of memory
	 * be allocated (probably ok).  The total amount of
	 * memory needed for buffering depends on the type
	 * of data that is being collected and the network
	 * connectivity (as it can result in delays that
	 * may cause backups in the server).
	 */
	for (cpu = 0; cpu < numb_processors; cpu++) {
	    datalen[cpu] = 200*sizeof (tstamp_event_entry_t);
	    data[cpu] = (char*) malloc(datalen[cpu]);
	    if (data[cpu] == NULL)
		fatal("Out of memory (event buffer).");
	    pending[cpu] = (const tstamp_event_entry_t*) data[cpu];
	    npending[cpu] = 0;		/* nothing pending */
	    tlast[cpu] = 0;			/* time of oldest event */
	}
    }

    if (maxbytes != -1)				/* pass to server */
	rt->maxindbytes = maxbytes;
    if (rtmond_start_ncpu(rt, numb_processors, cpu_mask, event_mask) < 0) {
	const char* why = "unknown reason";
	switch (errno) {
	case EAGAIN:
	    why = "temporarily out of resources";
	    break;
	case EFAULT:
	    why = "protocol botch (bad command or parameter)";
	    break;
	case ENXIO:
	    why = "invalid command parameter (CPU or event mask, protocol)";
	    break;
	case EPERM:
	    why = "server disallowed event collection";
	    break;
	}
	fatal("Unable to start data collection; %s.", why);
	/*NOTREACHED*/
    }
    datafd = rt->socket;

    sigset(SIGHUP, sigcatcher);
    sigset(SIGINT, sigcatcher);
    sigset(SIGQUIT, sigcatcher);
    sigset(SIGTERM, sigcatcher);
    sigset(SIGPIPE, SIG_IGN);			/* NB: check write instead */
    /*
     * Now that event collection is setup, enable per-process system call
     * and context switch tracing if requested.
     */
    if (event_mask & (RTMON_SYSCALL|RTMON_TASKPROC)) {
	/*
	 * If tracing the entire system turn off tracing
	 * of ourselves to avoid loading the system.  The zflag can
	 * be used to override this behaviour but should not be done
	 * lightly since we run with a realtime priority.
	 */
	if (npids == 0 && !zflag && ioctl(dev, PAR_DISALLOW, (pid_t) 0) < 0)
	    _perror("ioctl(PAR_DISALLOW)");
	dosetup(dev, rt->cookie,
		((event_mask & RTMON_SYSCALL)  ? PAR_SYSCALL : 0) |
		((event_mask & RTMON_TASKPROC) ? PAR_SWTCH   : 0) |
		(inherit                       ? PAR_INHERIT : 0));
    }
    /*
     * Tracing should all be setup now; signal the
     * processes so they'll start running (used by
     * par to spawn commands that are traced).
     */
    if (npids > 0) {			/* count procs setup properly */
	nactive = 0;
	for (i = 0; i < npids; i++)
	    if (pids[i] != 0) {
		if (wakekid[i] && kill(pids[i], SIGUSR1) != 0) {
		    _perror(
			"Could not wakeup traced process; kill(%d, SIGUSR1)",
			pids[i]);
		    docleanup(dev, rt->cookie);
		    exit(-1);
		}
		nactive++;
	    }
    } else				/* tracing entire system */
	nactive = -1;
    /*
     * If collecting for a fixed length of
     * time, start a timer to terminate us.
     */
    if (tlen) {
	sigset(SIGALRM, sigalarm);
	alarm(tlen);
    }
    if (nactive == -1) {
	/*
	 * When collecting for the entire system do not merge
	 * events because it slows padc down so much that rtmond
	 * backs up and loses and/or drops events.  par will
	 * recognize that it needs to merge data and handle it.
	 */
	while (!leave) {
	    ssize_t cc = read(datafd, obuf+occ, obufsz - occ);
	    if (cc == -1)
		break;
	    occ += cc;
	    if (obufsz - occ < 1024)
		dowrite(STDOUT_FILENO, obuf, occ), occ = 0;
	}
    } else {
	while (!leave && nactive != 0) {
	    union {
		char buf[sizeof (tstamp_event_entry_t)];
		tstamp_event_entry_t ev;
	    } u;
	    size_t chunksize;
	    size_t off;
	    size_t nevts;
	    uint64_t threshold;

	    if (!doread(datafd, u.buf, sizeof (u.ev))) {
		if (debug)
		    fprintf(stderr, "padc: short read of event\n");
		break;
	    }
	    if (u.ev.evt == TSTAMP_EV_SORECORD) {
		/*
		 * A new "record" of data to process.  Read the
		 * new chunk of event data into the per-CPU buffer.
		 * We move data or expand the buffer as needed to
		 * accomodate the new events.
		 */
		cpu = u.ev.cpu;
		chunksize = (size_t) u.ev.qual[0];
		nevts = chunksize / sizeof (u.ev);
		off = ((const char*) pending[cpu]) - data[cpu];
		if (off + (npending[cpu]+nevts)*sizeof (u.ev) > datalen[cpu]) {
		    if (chunksize <= off) {
			/*
			 * Space is available at the front of the buffer;
			 * just copy the pending events down.
			 */
			if (npending[cpu])
			    memmove(data[cpu], pending[cpu],
				npending[cpu]*sizeof (u.ev));
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
			datalen[cpu] = off+(npending[cpu]+nevts)*sizeof (u.ev);
			dp = (char*) malloc(datalen[cpu]);
			if (dp == NULL) {
			    fprintf(stderr,
				"padc: Out of memory (event data).\n");
			    break;
			}
			memcpy(dp, pending[cpu], npending[cpu]*sizeof (u.ev));
			free(data[cpu]), data[cpu] = dp;
			pending[cpu] = (const tstamp_event_entry_t*) dp;
		    }
		}
		/*
		 * Read the chunk of event data.
		 */
		if (!doread(datafd, (char*)(pending[cpu]+npending[cpu]), chunksize)) {
		    if (debug)
			fprintf(stderr,
			    "padc: short read of %d bytes of event data\n",
			    chunksize);
		    break;
		}
		if (u.ev.qual[1])
		    checksum(cpu, pending[cpu]+npending[cpu],
			chunksize, u.ev.qual[1]);
		npending[cpu] += nevts;
		if (numb_processors == 1) {
		    /*
		     * When collecting events for a subset of processes
		     * track process exit+fork events and so we can stop
		     * when all the processes we care about are gone.
		     */
		    nactive -= trackproc(pending[cpu], nevts);
		    /*
		     * No need to do merge work on a uniprocessor;
		     * just pass data long.  Note that doing this
		     * also avoids the issue of handling 32-bit time
		     * values wrapping (since we know the only systems
		     * with this are uniprocessor machines, right?).
		     *
		     * For now we *copy* data to the output buffer;
		     * this could also be eliminated.
		     */
		    while (occ + npending[0]*sizeof (u.ev) > obufsz) {
			size_t n = (obufsz - occ) / sizeof (u.ev);
			memcpy(obuf+occ, pending[0], n*sizeof (u.ev));
			pending[0] += n, npending[0] -= n;

			dowrite(STDOUT_FILENO, obuf, obufsz), occ = 0;
		    }
		    memcpy(obuf+occ, pending[0], npending[0]*sizeof (u.ev));
		    occ += npending[0]*sizeof (u.ev);
		    pending[0] = (const tstamp_event_entry_t*) data[0];
		    npending[0] = 0;
		} else {
		    /*
		     * Calculate the latest time that we can merge
		     * events up to.  We know each stream of events
		     * is ordered so by comparing the time of the
		     * last event for each CPU we can select a time
		     * that is safe to use in selecting events to merge.
		     */
		    tlast[cpu] = u.ev.qual[2];
		    threshold = tlast[0];
		    for (cpu = 1; cpu < numb_processors; cpu++)
			if (tlast[cpu] < threshold)
			    threshold = tlast[cpu];
		    nactive -= merge(threshold, pending, npending);
		}
	    } else {
		if (occ + sizeof (u.ev) > obufsz)
		    dowrite(STDOUT_FILENO, obuf, occ), occ = 0;
		memcpy(obuf+occ, &u.ev, sizeof (u.ev)), occ += sizeof (u.ev);
	    }
	    if (linebuffer && occ > 0)
		dowrite(STDOUT_FILENO, obuf, occ), occ = 0;
	}
	(void) merge(-1, pending, npending);	/* flush any remainder */
    }
    if (occ > 0)
	dowrite(STDOUT_FILENO, obuf, occ);
    docleanup(dev, rt->cookie);
    if (debug && nactive != -1) {
	long total = 0;
	for (cpu = 0; cpu < numb_processors; cpu++) {
	    fprintf(stderr, "padc: CPU[%d] datalen %d\n", cpu, datalen[cpu]);
	    total += datalen[cpu];
	}
	fprintf(stderr, "Total space %lu = %.1f KB\n",
	    total, (double) total / 1024.);
    }
    exit(0);
    /* NOTREACHED */
}

/*
 * Handle a process exit event.
 */
static int
pid_exit(pid_t pid)
{
    int i;
    if (debug)
	fprintf(stderr, "padc: pid %d exit\n", (int) pid);
    for (i = 0; i < npids && pids[i] != pid; i++)
	;
    if (i < npids) {
	pids[i] = 0;			/* mark done */
	return (1);
    } else
	return (0);
}

/*
 * Handle a process fork event.
 */
static int
pid_fork(pid_t ppid, pid_t cpid)
{
    int i;
    if (debug)
	fprintf(stderr, "padc: fork %d/%d\n", (int) ppid, (int) cpid);
    for (i = 0; i < npids && pids[i] != ppid; i++)
	;
    if (i < npids) {
	/*
	 * We're tracing this process, find a free
	 * slot to add the new child process.
	 */
	for (i = 0; i < MAXPIDS && pids[i] != 0; i++)
	    ;
	if (i < MAXPIDS) {		/* found free slot */
	    pids[i] = cpid;
	    if (i >= npids)		/* bump pid count if necessary */
		npids = i+1;
	} else if (debug)
	    fprintf(stderr, "padc: No space for child %d in pid array\n",
		(int) cpid);
	return (1);
    } else
	return (0);
}

/*
 * Look for process exit+fork events and update
 * pid tracing state.  This routine is used only
 * on uniprocessor systems; on MP systems the
 * work is done after merging events so we process
 * these events in proper order.
 */
static int
trackproc(const tstamp_event_entry_t* ev, size_t nevents)
{
    int n = 0;

    while ((ssize_t) nevents > 0) {
	if (ev->evt == TSTAMP_EV_EXIT) {
	    pid_t pid = (pid_t) ev->qual[1];
	    if (pid_exit(pid)) {
		if (debug)
		    fprintf(stderr, "padc: pid %d done\n", (int) pid);
		n++;
	    }
	} else if (ev->evt == TSTAMP_EV_FORK) {
	    pid_t cpid, ppid;
	    int len = 0;
	    const tstamp_event_fork_data_t* fdp = 
		(const tstamp_event_fork_data_t*) &ev->qual[0];
	    cpid = syssgi(SGI_GET_CONTEXT_NAME, fdp->ckid, NULL, &len);
	    ppid = syssgi(SGI_GET_CONTEXT_NAME, fdp->pkid, NULL, &len);
	    if (pid_fork(ppid, cpid)) {
		if (debug)
		    fprintf(stderr, "padc: kid %lld forked, new kid %lld\n",
			fdp->pkid, fdp->ckid);
		n--;
	    }
	}
	nevents -= 1+ev->jumbocnt;
	ev += 1+ev->jumbocnt;
    }
    return (n);
}

/*
 * Merge event data up to tmax time.  Merged events
 * are written to stdout (w/ buffering).  This code
 * assumes that each CPU's set of events are already
 * ordered in time; we just merge the event streams
 * from different CPUs.
 */
static int
merge(uint64_t tmax, const tstamp_event_entry_t* pending[], int npending[])
{
    const tstamp_event_entry_t* candidate[RTMOND_MAXCPU];
    const tstamp_event_entry_t* ev;
    int nc, i, j, k;
    int n = 0;

    /*
     * Find CPUs with events to merge and do a
     * first-level sort of the top events for
     * each CPU.  We assume below that each CPU's
     * events are already sorted.
     */
    nc = 0;
    if (debug > 1)
	fprintf(stderr, "padc: merge to %llu: ", tmax);
    for (i = 0; i < numb_processors; i++) {
	if (npending[i] && (ev = pending[i])->tstamp < tmax) {
	    for (j = 0; j < nc && ev->tstamp > candidate[j]->tstamp; j++)
		;
	    if (j < nc) {			/* insert in middle */
		for (k = nc-1; k >= j; k--)
		    candidate[k+1] = candidate[k];
	    }
	    candidate[j] = ev, nc++;
	    if (debug > 1)
		fprintf(stderr, " CPU[%d] %d", i, npending[i]);
	} else if (debug > 1)
	    fprintf(stderr, " !CPU[%d](%d,%lld)", i, npending[i],
		npending[i] ? pending[i]->tstamp : 0);
    }
    if (debug > 1)
	fprintf(stderr, "\n");
    while (nc > 0) {
	ev = candidate[0];			/* sorted event */
	if (ev->tstamp > tmax) {
	    for (i = 0; i < nc; i++)
		pending[candidate[i]->cpu] = candidate[i];
	    break;
	}
	j = 1+ev->jumbocnt;			/* slots used by event */

	if (occ + j*sizeof (*ev) > obufsz)
	    dowrite(STDOUT_FILENO, obuf, occ), occ = 0;
	memcpy(obuf+occ, ev, j*sizeof (*ev)), occ += j*sizeof (*ev);

	/*
	 * Track process exit+fork events.  Must check for
	 * them here so that we process them in sorted order.
	 */
	if (ev->evt == TSTAMP_EV_EXIT) {
	    pid_t pid = (pid_t) ev->qual[1];
	    if (pid_exit(pid)) {
		if (debug)
		    fprintf(stderr, "padc: pid %d done\n", (int) pid);
		n++;
	    }
	} else if (ev->evt == TSTAMP_EV_FORK) {
	    pid_t cpid, ppid;
	    int len = 0;
	    const tstamp_event_fork_data_t* fdp =
    		(const tstamp_event_fork_data_t*) &ev->qual[0];
	    cpid = syssgi(SGI_GET_CONTEXT_NAME, fdp->ckid, NULL, &len);
	    ppid = syssgi(SGI_GET_CONTEXT_NAME, fdp->pkid, NULL, &len);
	    if (pid_fork(ppid, cpid)) {
		if (debug)
		    fprintf(stderr, "padc: kid %d forked, new kid %d\n",
			    ppid, cpid);
		n--;
	    }
	}
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
    return (n);
}

/*
 * Check a server-calculated checksum.  By
 * default data does *not* have checksums;
 * these are added only for debugging.
 */
static void
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

/*
 * Read data from a network connection;
 * handling partial reads and signals
 * (that mark "leave").
 */
static int
doread(int fd, char* data, size_t cc0)
{
    size_t cc = read(fd, data, cc0);
    if (cc != cc0) {
	if (cc == (size_t) -1 || leave)
	    return (0);
	do {
	    size_t n = read(fd, data+cc, cc0 - cc);
	    if (n == (size_t) -1 || leave)
		return (0);
	    cc += n;
	} while (cc != cc0);
    }
    if (debug > 1) {
	#pragma mips_frequency_hint NEVER
	tstamp_event_entry_t *ev = (tstamp_event_entry_t *)data;
	int n = cc0 / sizeof (*ev);

	while (n--) {
	    fprintf(stderr, "event %5d %3d %3d %llu %#10llx %#10llx %#10llx %#10llx\n",
		    ev->evt, ev->cpu, ev->jumbocnt, ev->tstamp,
		    ev->qual[0], ev->qual[1], ev->qual[2], ev->qual[3]);
	    ev++;
	}
    }
    return (1);
}

/*
 * Write data, restarting in case we
 * get interrupted by a timer.
 */
static void
dowrite(int fd, const void *buf, size_t len)
{
    const char *bp = buf;
    size_t resid = len;

    if (debug) {
	if (debug > 1)
	    return;
        fprintf(stderr, "padc: write %ld\n", len);
    }
    while (resid > 0) {
	ssize_t nwrite = write(fd, bp, resid);
	if (nwrite < 0) {
	    const char* msg = strerror(errno);
	    /*
	     * If interrupted by the timer and we've written
	     * part of our output, keep trying.  Otherwise
	     * cleanup and abort collection.
	     */
	    if (errno == EINTR && resid < len)
		continue;
	    docleanup(dev, rt->cookie);
	    if (errno != EPIPE)			/* usually par interrupted */
		fatal("write error: %s", msg);
	    else
		exit(EXIT_FAILURE);
	}
	resid -= nwrite;
	bp += nwrite;
    }
}

static void
proc_syntax(const char* what)
{
    fatal("syntax error in processor list; %s", what);
}

static void
parse_proc_str(const char* arg, uint64_t mask[])
{
    const char* cp = arg;

    memset(mask, 0, RTMOND_MAXCPU>>3);
    while (*cp) {
	int m, n;

	if (!isdigit(*cp))
	    proc_syntax("expecting CPU number");
	m = 0;
	do {
	    m = 10*m + (*cp-'0');
	} while (isdigit(*++cp));
	if (*cp == '-') {
	    cp++;
	    if (isdigit(*cp)) {
		n = 0;
		do {
		    n = 10*n + (*cp-'0');
		} while (isdigit(*++cp));
	    } else if (*cp == '\0') {		/* e.g. 2- */
		n = numb_processors-1;
	    } else
		proc_syntax("expecting CPU number");
	} else if (*cp == ',') {
	    cp++;
	    n = m;
	} else if (*cp == '\0') {
	    n = m;
	} else
	    proc_syntax("expecting ',' or '-' after CPU number");
	if (m > n)
	    fatal("inverted CPU range, %d > %d", m, n);
	if (m >= numb_processors)
	    fatal("%d: CPU number out of range; system has only %d processors",
		m, numb_processors);
	for (; m <= n; m++)
	    mask[m>>6] |= ((uint64_t) 1)<<(m&0x3f);
    }
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
		wakekid[i] = 0;
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
    fprintf(stderr,
	"usage: %s [-rsi] [-I max][-h pri] [-t time] [-p pid]*\n"
	"       -s          trace system calls\n"
	"       -i          trace system calls across forks\n"
	"       -r          trace scheduler\n"
	"       -k          trace disk activity\n"
	"       -x          trace network queueing\n"
	"       -X          trace network throughput\n"
	"       -t time     trace for 'time' seconds\n"
	"       -p pid      trace specific pid (more than 1 allowed)\n"
	"       -h pri      set priority for padc\n"
	"       -I max      set maximum bytes of args to collect\n"
	"       -b bufsz    set internal buffer size (0 -> line buffered)\n",
	pname
    );
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
