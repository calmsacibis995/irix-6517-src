#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <ctype.h>
#include <assert.h>

#include <rtmon.h>
#include "util.h"

#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))

static	int decodeevents = 1;
static	uint64_t counts[MAXCPU];

static ssize_t
dumpEvents(rtmonPrintState* rs, const tstamp_event_entry_t* ev, ssize_t cc)
{
    while (cc >= sizeof (*ev)) {
	if (ev->cpu >= numb_processors && ev->cpu < MAXCPU-1)
	    numb_processors = ev->cpu+1;
	assert(ev->cpu < MAXCPU);
	counts[ev->cpu]++;
	if (!decodeevents || !rtmon_printEvent(rs, stdout, ev))
	    rtmon_printRawEvent(rs, stdout, ev);
	cc -= (1+ev->jumbocnt)*sizeof (*ev);
	ev += 1+ev->jumbocnt;
    }
    return (cc);
}

static void
setsyscalltrace(rtmonPrintState* rs, const char* arg, int onoff)
{
    if (isdigit(arg[0])) {
	if (rtmon_settracebynum(rs, atoi(arg), onoff) ||
	    rtmon_settracebyname(rs, arg, onoff))
	    return;
    } else if (rtmon_settracebyname(rs, arg, onoff))
	return;
    fatal("Unknown system call name/number %s.", arg);
    /*NOTREACHED*/
}

static void
usage(void)
{
    fprintf(stderr, "usage: %s: [-fiOruv] [-d usecs] [-m events] [-p cpus] [file]\n", appName);
    exit(EXIT_FAILURE);
}

void
main(int argc, char** argv)
{
    rtmonPrintState* rs;
    int follow = 0;
    int fd;
    int piped;
    int c;
    int verbose = 0;
    char* filename = NULL;
    extern char *optarg;
    extern int optind;

    appName = strrchr(argv[0], '/');
    if (appName != NULL)
	appName++;
    else
	appName = argv[0];

    rs = rtmon_printBegin();
    while ((c = getopt(argc, argv, "d:e:fiKm:n:Oxp:ruv")) != -1) {
	switch (c) {
	case 'd':
	    rs->syscalldelta = strtoll(optarg, 0, NULL);
	    break;
	case 'i':
	    rs->flags |= RTPRINT_INTERNAL;
	    break;
	case 'f':
	    follow = 1;
	    break;
	case 'm':
	    rs->eventmask = parse_event_str(optarg);
	    break;
	case 'n':
	case 'e':
	    setsyscalltrace(rs, optarg, c == 'n');
	    break;
	case 'K':
	    rs->flags |= (RTPRINT_SHOWKID | RTPRINT_SHOWPID);
	    break;
	case 'O':
	    rtmon_setOutputBase(rs, 8);
	    break;
	case 'x':
	    rtmon_setOutputBase(rs, 16);
	    break;
	case 'p':
	    rtmon_traceProcess(rs, (pid_t) atoi(optarg));
	    break;
	case 'r':
	    decodeevents = 0;
	    rs->flags &= ~RTPRINT_SHOWPID;
	    break;
	case 'u':
	    rs->flags |= RTPRINT_USEUS;
	    break;
	case 'v':
	    verbose++;
	    break;
	case '?':
	default:
	    usage();
	    break;
	}
    }
    if (argc-optind >= 1) {
	filename = argv[optind];
	fd = open(filename,O_RDONLY);
	if (fd < 0)
	    fatal("Cannot open %s: %s", filename, strerror(errno));
    } else {
	filename = "<stdin>";
	fd = fileno(stdin);
    }
    piped = (lseek(fd, 0, SEEK_CUR) != 0 && errno == ESPIPE);

    if (!follow) {			/* try to mmap event data file */
	const tstamp_event_entry_t* ev;
	struct stat sb;

	(void) fstat(fd, &sb);
	ev = (const tstamp_event_entry_t*) mmap(0, sb.st_size,
	    PROT_READ, MAP_PRIVATE|MAP_AUTORESRV, fd, 0);
	if (ev != (const tstamp_event_entry_t*) -1) {
	    (void) dumpEvents(rs, ev, sb.st_size);
	    if (verbose) {
		for (c = 0; c < MAXCPU; c++)
		    if (counts[c])
			fprintf(stderr, "CPU %d: %lld events\n", c, counts[c]);
	    }
	    exit(EXIT_SUCCESS);
	}
    }
    for (;;) {
	union {
	    tstamp_event_entry_t ev[1];
	    char buf[roundup(64*1024, sizeof (tstamp_event_entry_t))];
	} u;
	ssize_t n, cc;
	
	cc = 0;
	do {
	    n = read(fd, &u.buf[cc], sizeof (u) - cc);
	    if (n < 0)
		fatal("read error: %s", strerror(errno));
	    if (n == 0) {
		if (follow && !piped) {
		    sleep(1);
		    continue;
		}
		if (cc > 0)
		    fatal("partial event read");
		if (verbose) {
		    for (c = 0; c < MAXCPU; c++)
			if (counts[c])
			    fprintf(stderr, "CPU %d: %lld events\n",
				c, counts[c]);
		}
		exit(EXIT_SUCCESS);
	    }
	    cc += n;
	} while (cc < sizeof (tstamp_event_entry_t));
	/*
	 * NB: We assume we have no partial events; if we
	 *     do get one it'll be discarded.
	 */
	cc = dumpEvents(rs, u.ev, cc);
	if (cc > 0)
	    fatal("partial event discarded; out of sync");
    }
}
