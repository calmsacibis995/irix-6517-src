#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#define _BSD_SIGNALS
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <ulocks.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <sched.h>
#include <netinet/in.h>

#include <rtmon.h>
#include "util.h"

#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))

int	debug = 0;
char	fileprefix[1024] = "default";
size_t	obufsz = (size_t) (128*1024);

static	void rtmon_daemon(void*, size_t);
static	void rtmon_merge_daemon(void*, size_t);

typedef struct {
    int cpu;			/* CPU number */
    rtmond_t* server;		/* server event data stream */
    int	collecting;		/* 1 while thread collecting events */
    int	wv;			/* file for event data */
    const char* datafile;	/* name of file for event data */
    FILE* fp;			/* stream for debugging info */
    char* obuf;			/* output buffer */
    size_t occ;			/* count of data in obuf */
} cpustate_t;
static	cpustate_t* cpustate;

#define	isCPUActive(n)	(cpu_mask[(n)>>6] & ((uint64_t) 1)<<((n)&0x3f))

void
sigINT()
{
    int i;
    for (i = 0; i < numb_processors; i++)
	cpustate[i].collecting--;
    if (cpustate[0].collecting < 3) {		/* after 3 times, just abort */
	kill(-getpid(), SIGKILL);
	exit(EXIT_FAILURE);
    }
}

void
sigALRM()
{
    int i;
    for (i=0; i<numb_processors; i++)
	cpustate[i].collecting = 0;
}

static void
usage(void)
{
    fprintf(stderr, "usage: %s"
	" [-d]"
	" [-p cpu-list]"
	" [-f file]"
	" [-m event-mask]"
	" [-n]"
	" [-h hostname]"
	" [-t timeout]"
	" [-O]\n", appName);
    exit(EXIT_FAILURE); 
}

static void
check_file(const char* filename)
{
    struct stat sb;
    int c;

    if (stat(filename, &sb) >= 0) {
	printf("\"%s\" exists, overwrite? ", filename); fflush(stdout);
	c = getchar();
	if (c != 'y' && c != 'Y')
	    exit(EXIT_SUCCESS);
	while ((c = getchar()) != EOF && c != '\n')
	    ;
    }
}

static void
make_file(cpustate_t* cs, const char* filename)
{
    check_file(filename);
    cs->wv = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0666);
    if (cs->wv < 0)
	fatal("%s: Cannot create: %s", filename, strerror(errno));
    cs->datafile = strdup(filename);
}

static int
islocal(int fd)
{
    struct sockaddr name;
    int namelen = sizeof (name);
    if (getpeername(fd, &name, &namelen) < 0)
	return (0);
    if (name.sa_family == AF_UNIX)
	return (1);
    if (name.sa_family == AF_INET) {
	struct sockaddr_in* sin = (struct sockaddr_in*) &name;
	return (sin->sin_addr.s_addr == INADDR_LOOPBACK);	/* good 'nuf */
    }
    return (0);
    
}

void
main(int argc, char** argv)
{
    rtmond_t* rt;
    int cpu, nactive;
    int c;
    int timeout = 0;
    int merge = 1;
    int oldWay = 0;
    const char* proc_str = "0-";		/* all cpus */
    const char* event_str = "all";		/* all events */
    uint64_t event_mask = 0;
    uint64_t cpu_mask[RTMOND_MAXCPU>>6];
    int verbose = 0;
    char filename[1024];
    extern char *optarg; 
    extern int optind;

    appName = strrchr(argv[0], '/');
    if (appName != NULL)
	appName++;
    else
	appName = argv[0];

    while ((c = getopt(argc, argv, "b:df:h:m:nOp:t:v")) != -1) 
	switch(c) {
	case 'b':
	    obufsz = atoi(optarg);
	    break;
	case 'd':			/* enable debugging */
	    debug++; 
	    break;
	case 'f':			/* output filename */
	    strcpy(fileprefix, optarg);
	    break;
	case 'h':			/* target host */
	    hostname = optarg;
	    break;
	case 'm':			/* event collection mask */
	    event_str = optarg;
	    break;
	case 'n':
	    merge = 0;
	    break;
	case 'O':
	    oldWay = 1;
	    break;
	case 'p':			/* processor set */
	    proc_str = optarg;
	    break ; 
	case 't':			/* collection interval */
	    (void) signal(SIGALRM, sigALRM);
	    timeout = atoi(optarg);
	    break;
	case 'v':
	    verbose++;
	    break;
	default :
	    usage();
	}

    event_mask = parse_event_str(event_str);	/* construct event mask */
    /*
     * Establish control connection to server prior to parsing
     * the processor set specification so we know how many
     * CPUs exist on the target machine.
     */
    rt = rtmond_open(hostname, 0);
    if (!rt)
	fatal("Unable to contact rtmon service on host %s.", hostname);

    numb_processors = rt->ncpus;
    cpustate = (cpustate_t*) malloc(numb_processors * sizeof (cpustate_t));

    parse_proc_str(proc_str, cpu_mask);		/* mask of selected proc's */
    nactive = 0;
    if (oldWay) {
	for (cpu = 0; cpu < numb_processors; cpu++) {
	    cpustate_t* cs = &cpustate[cpu];
	    cs->collecting = 0;
	    cs->cpu = cpu;
	    cs->fp = NULL;
	    cs->occ = 0;
	    if (isCPUActive(cpu)) {
		nactive++;
		cs->obuf = (char*) malloc(obufsz);
		if (cs->obuf == NULL)
		    fatal("Cannot allocate output buffer for CPU %d", cpu);
	    }
	    if (usconfig(CONF_INITUSERS, nactive+1) == -1)
		fatal("Cannot set max share group size: usconfig: %s",
		      strerror(errno));
	}
    } else {
	cpustate_t* cs = &cpustate[0];
	cs->collecting = 0;
	cs->cpu = 0;
	cs->fp = NULL;
	cs->occ = 0;
	cs->obuf = (char*) malloc(obufsz);
	if (cs->obuf == NULL)
	    fatal("Cannot allocate output buffer for CPU %d", cpu);
	for (cpu = 0; cpu < numb_processors; cpu++)
	    if (isCPUActive(cpu))
		nactive++;
    }

    (void) signal(SIGINT, sigINT);

    if (rt->schedpri && islocal(rt->socket)) {
	struct sched_param param;
	memset(&param, 0, sizeof (param));
	param.sched_priority = rt->schedpri;
	if (sched_setscheduler(0, SCHED_FIFO, &param) < 0)
	    fprintf(stderr, "Warning, cannot set realtime scheduling priority"
		" to match server.\n");
    }

    if (oldWay) {
	/*
	 * Start data collection on each CPU.  We work from the last
	 * CPU to the first to avoid a WindView bug: WindView opens
	 * the numerically last file first and uses the TIMER_SYNC
	 * time of the first file it opens as the start time for its
	 * display.   This means that if we don't use the same order
	 * for creating event streams then WindView may encounter
	 * events that preceed that start time; causing it to bad things.
	 */
	for (cpu = numb_processors-1; cpu >= 0; cpu--) {
	    cpustate_t* cs = &cpustate[cpu];
	    if (isCPUActive(cpu)) {
		if (rt) {
		    cs->server = rt;
		    rt = NULL;
		} else
		    cs->server = rtmond_open(hostname, 0);
		if (cs->server == NULL || rtmond_start_cpu(cs->server, cpu, event_mask) < 0)
		    fatal("Cannot start data collection for cpu %u on host %s.",
			cpu, hostname);

		sprintf(filename, "%s.%d.wvr", fileprefix, cpu);
		make_file(cs, filename);

		if (debug > 2) {
		    sprintf(filename, "%s%s%d", fileprefix, ".dbg.", cs->cpu);
		    check_file(filename);
		    cs->fp = fopen(filename, "w");
		    if (cs->fp == NULL)
			fprintf(stderr,
			    "Warning, cannot create %s for debugging output.\n",
			    filename);
		}
		if (debug)
		    printf("client gathering events on processor %d\n", cpu);

		if (sprocsp(rtmon_daemon, PR_SALL, cs, NULL, 64*1024) == -1)
		    fatal("sprocsp: %s", strerror(errno));
	    }
	}
    } else {
	/*
	 * New protocol uses a single IPC stream to send event
	 * data from all processors.  This means we don't need to
	 * start multiple threads to collect data; we can just do it
	 * here.  For convenience however we use one thread to do the
	 * collection; this allows us to reuse the logic needed for
	 * the old thread-per-cpu protocol.
	 */
	if (cpu_mask == 0)
	    fatal("No processors specified for event collection");
	
	make_file(&cpustate[0], fileprefix);
	if (debug > 2)
	    cpustate[0].fp = stdout;			/* XXX */

	cpustate[0].server = rt;
	if (rtmond_start_ncpu(rt, numb_processors, cpu_mask, event_mask) < 0)
	    fatal("Cannot start data collection for host %s.", hostname);

	if (verbose)
	    printf("Connected to %s; cookie %#llx\n", hostname, rt->cookie);

	if (sprocsp(nactive > 1 && merge ? rtmon_merge_daemon : rtmon_daemon,
	    PR_SALL, &cpustate[0], NULL, 64*1024) == -1)
	    fatal("sprocsp: %s", strerror(errno));
    }

    /*
     * Done setting up collection; now just wait for things
     * to complete (either by timeout or by keyboard interrupt).
     */
    if (timeout)
	(void) alarm(timeout);
    while (waitpid(-1, NULL, 0) >= 0 || errno == EINTR)
	;
    exit(EXIT_SUCCESS);
}

static int
doread(int fd, char* data, ssize_t cc0)
{
    ssize_t cc = read(fd, data, cc0);
    if (cc != cc0) {
	if (cc <= 0) {
	    if (errno != EINTR)
		return (0);
	    cc = 0;
	}
	do {
	    ssize_t n = read(fd, data+cc, cc0 - cc);
	    if (n <= 0) {
		if (errno != EINTR)
		    return (0);
		continue;
	    }
	    cc += n;
	} while (cc != cc0);
    }
    return (1);
}

static void
dodata(cpustate_t* cs, const void* data, size_t cc)
{
  /* Check for buffer overrun. */
  if (cs->occ + cc > obufsz) {
    /* Write out the cs buffer's data */
    if (write(cs->wv, cs->obuf, cs->occ) != cs->occ)
      fatal("%s: write error: %s", cs->datafile, strerror(errno));

    /*
     * Determine if the incoming data is smaller than the cs buffer.
     * If yes, then just copy the data and defer the write. Otherwise,
     * write out the data.
     */
    if (cc < obufsz) {
      memcpy(cs->obuf, data, cc);
      cs->occ = cc;
    } else {
      if (write(cs->wv, data, cc) != cc)
	fatal("%s: write error: %s", cs->datafile, strerror(errno));
      cs->occ = 0; /* delayed assignment of occ from write of cs->obuf data */
    }
  } else {
    memcpy(cs->obuf + cs->occ, data, cc);
    cs->occ += cc;
  }
}

static void
flushdata(cpustate_t* cs)
{
    if (cs->occ && write(cs->wv, cs->obuf, cs->occ) != cs->occ)
	fatal("%s: write error: %s", cs->datafile, strerror(errno));
}

/*
 * Collect event data formatted according to
 * the "new" protocol (i.e. raw kernel events).
 */
static void
rtmon_daemon(void* arg, size_t stacksize)
{
    cpustate_t* cs = (cpustate_t*) arg;
    rtmond_t* rt = cs->server;
    rtmonPrintState* rs;
    size_t datalen;
    char* data;

    (void) stacksize;

    cs->collecting = 1;

    datalen = NUMB_KERNEL_TSTAMPS*sizeof (tstamp_event_entry_t);
    data = (char*) malloc(datalen);
    rs = (cs->fp ? rtmon_printBegin() : NULL);
    while (cs->collecting) {
        tstamp_event_entry_t ev;
	size_t chunksize;
	
	if (!doread(rt->socket, (void *)&ev, sizeof (ev)))
	    break;
	if (rs) {
	    if (!rtmon_printEvent(rs, cs->fp, &ev))
		rtmon_printRawEvent(rs, cs->fp, &ev);
	    fflush(cs->fp);
	}
	if (ev.evt == TSTAMP_EV_SORECORD) {
	    chunksize = (size_t) ev.qual[0];
	    if (chunksize > datalen) {
		data = realloc(data, chunksize);
		datalen = chunksize;
	    }
	    /*
	     * Read the chunk of event data.
	     */
	    if (!doread(rt->socket, data, (ssize_t) chunksize))
		break;
	    if (rs) {
		const tstamp_event_entry_t* ev2 =
		    (const tstamp_event_entry_t*) data;
		ssize_t nevts = (ssize_t)(chunksize / sizeof (*ev2));
		while (nevts > 0) {
		    if (!rtmon_printEvent(rs, cs->fp, ev2))
			rtmon_printRawEvent(rs, cs->fp, ev2);
		    nevts -= 1+ev2->jumbocnt;
		    ev2 += 1+ev2->jumbocnt;
		}
		fflush(cs->fp);
	    }
	    dodata(cs, data, chunksize);
	} else
	    dodata(cs, &ev, sizeof (ev));
    }
    if (rs)
	rtmon_printEnd(rs);
    free(data);
    flushdata(cs);
    
    cs->collecting = 0;

    close(cs->wv);
    rtmond_close(rt);
    if (debug)
	fprintf(stderr, "client for processor %d exiting normally\n", cs->cpu);
    exit(EXIT_SUCCESS);
}

/*
 * Merge event data up to tmax time.  Merged events
 * are written to the cpu's data file.  This code
 * assumes that each CPU's set of events are already
 * ordered in time; we just merge the event streams
 * from different CPUs.
 */
static void
merge(cpustate_t* cs, uint64_t tmax, const tstamp_event_entry_t* pending[], int npending[])
{
    const tstamp_event_entry_t* candidate[MAXCPU];
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
	fprintf(stderr, "merge to %llu: ", tmax);
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
	}
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

	dodata(cs, ev, j*sizeof (*ev));

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
"checksum mismatch, cpu %d, count %d, got %#llx, expected %#llx\n",
	    cpu, cc0, sum, esum);
	lp = (const uint64_t*) ev;
	for (cc = 0; cc != cc0; cc += sizeof (*lp))
	    fprintf(stderr, "%4d: %#llx\n", cc, *lp++);
    }
}

/*
 * Collect and merge event data formatted according
 * to the "new" protocol (i.e. raw kernel events).
 */
static void
rtmon_merge_daemon(void* arg, size_t stacksize)
{
    cpustate_t* cs = (cpustate_t*) arg;
    rtmond_t* rt = cs->server;
    const tstamp_event_entry_t* pending[MAXCPU];
    int npending[MAXCPU];
    char* data[MAXCPU];
    size_t datalen[MAXCPU];
    uint64_t tlast[MAXCPU];
    int cpu;

    (void) stacksize;

    assert(RTMOND_MAXCPU > numb_processors);

    cs->collecting = 1;

    /*
     * Data is delivered by the server in-order for
     * each CPU and we must merge the streams into a
     * single time-ordered stream of events.  To do
     * this we buffer events until we can safely merge
     * events w/o worrying about receiving any more
     * (that will need to be merged).
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
	pending[cpu] = (const tstamp_event_entry_t*) data[cpu];
	npending[cpu] = 0;		/* nothing pending */
	tlast[cpu] = 0;			/* time of oldest event */
    }
    while (cs->collecting) {
	tstamp_event_entry_t ev;
	size_t chunksize;
	size_t off;
	size_t nevts;
	uint64_t threshold;

	if (!doread(rt->socket, (void *)&ev, sizeof (ev)))
	    break;
	if (ev.evt == TSTAMP_EV_SORECORD) {
	    /*
	     * A new "record" of data to process.  Read the
	     * new chunk of event data into the per-CPU buffer.
	     * We move data or expand the buffer as needed to
	     * accomodate the new events.
	     */
	    cpu = ev.cpu;
	    chunksize = (size_t) ev.qual[0];
	    nevts = chunksize / sizeof (ev);
	    off = ((const char*) pending[cpu]) - data[cpu];
	    if (off + (npending[cpu]+nevts)*sizeof (ev) > datalen[cpu]) {
		if (chunksize <= off) {
		    /*
		     * Space is available at the front of the buffer;
		     * just copy the pending events down.
		     */
		    if (npending[cpu])
			memmove(data[cpu], pending[cpu],
			    npending[cpu]*sizeof (ev));
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
		    datalen[cpu] = off + (npending[cpu]+nevts)*sizeof (ev);
		    dp = (char*) malloc(datalen[cpu]);
		    assert(dp != NULL);
		    memcpy(dp, pending[cpu], npending[cpu]*sizeof (ev));
		    free(data[cpu]), data[cpu] = dp;
		    pending[cpu] = (const tstamp_event_entry_t*) dp;
		}
	    }
	    /*
	     * Read the chunk of event data.
	     */
	    if (!doread(rt->socket, (char*)(pending[cpu]+npending[cpu]), (ssize_t) chunksize))
		break;
	    if (ev.qual[1])
		checksum(cpu, pending[cpu]+npending[cpu],
		    chunksize, ev.qual[1]);
	    npending[cpu] += nevts;
	    /*
	     * Calculate the latest time that we can merge
	     * events up to.  We know each stream of events
	     * is ordered so by comparing the time of the
	     * last event for each CPU we can select a time
	     * that is safe to use in selecting events to merge.
	     */
	    tlast[cpu] = ev.qual[2];
	    threshold = tlast[0];
	    for (cpu = 1; cpu < numb_processors; cpu++)
		if (tlast[cpu] < threshold)
		    threshold = tlast[cpu];
	    merge(cs, threshold, pending, npending);
	} else
	    dodata(cs, &ev, sizeof (ev));
    }
    merge(cs, -1, pending, npending);		/* flush any remainder */
    flushdata(cs);
    
    cs->collecting = 0;

    close(cs->wv);
    rtmond_close(rt);
    if (debug) {
	long total = 0;
	for (cpu = 0; cpu < numb_processors; cpu++) {
	    fprintf(stderr, "CPU[%d] datalen %d\n", cpu, datalen[cpu]);
	    total += datalen[cpu];
	}
	fprintf(stderr, "Total space %lu = %.1f KB\n",
	    total, (double) total / 1024.);
	fprintf(stderr, "client for processor %d exiting normally\n", cs->cpu);
    }
    exit(EXIT_SUCCESS);
}
