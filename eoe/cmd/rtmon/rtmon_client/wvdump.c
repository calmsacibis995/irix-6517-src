#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/rtmon.h>
#include <errno.h>
#include <stdarg.h>

char*	appName;

static	void decodeWindView(FILE* fp, int fd);

static void
fatal(const char* fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s: ", appName);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static void
usage(void)
{
    fprintf(stderr, "usage: %s [files]\n", appName);
    exit(EXIT_FAILURE); 
}

void
main(int argc, char** argv)
{
    extern char *optarg; 
    extern int optind;
    int c;

    appName = strrchr(argv[0], '/');
    if (appName != NULL)
	appName++;
    else
	appName = argv[0];

    while ((c = getopt(argc, argv, "")) != -1) 
	switch(c) {
	case '?':
	default :
	    usage();
	}

    if (optind < argc) {
	for (; optind < argc; optind++) {
	    int fd = open(argv[optind], O_RDONLY);
	    if (fd < 0)
		fatal("%s: Cannot open: %s", strerror(errno));
	    decodeWindView(stdout, fd);
	    close(fd);
	}
    } else
	decodeWindView(stdout, fileno(stdin));
    exit(0);
}

#define MAX_IO_EVS 10

#define EVL_INIT()	(ev_entries = 0, ev_len = 0)
#define EVL(object) \
    (ev_vec[ev_entries].iov_base = (void *)&(object), \
     ev_vec[ev_entries].iov_len = sizeof (object), \
     ev_entries++, \
     ev_len += sizeof (object))
#define EVLA(array, nitems) \
    (ev_vec[ev_entries].iov_base = (void *)(array), \
     ev_vec[ev_entries].iov_len = sizeof (array)[0] * (nitems), \
     ev_entries++, \
     ev_len += sizeof (array)[0] * (nitems))
#define READ(_fd, object) \
    bytes = read(_fd, (void *)&(object), sizeof (object)); \
    if (bytes != sizeof (object)) { \
	if (bytes == 0) \
	    return; \
	fatal("read error, expected %u, got %u", sizeof (object), bytes); \
    }
#define READA(_fd, array, nitems) \
    bytes = read(_fd, (void *)(array), sizeof (array)[0] * (nitems)); \
    if (bytes != sizeof (array)[0] * (nitems)) \
	fatal("read error, expected %u, got %u", sizeof (array)[0] * (nitems), bytes);
#define READV(_fd) \
    bytes = readv(_fd, ev_vec, ev_entries); \
    if (bytes != ev_len) \
	fatal("read error, expected %u, got %u", ev_len, bytes);

static void
decodeWindView(FILE* fp, int fd)
{
    double tick = 0;
    int count = -1;

    for (;;) {
	uint16_t evt;		/* Windview version of evt is short */
	uint32_t my_time;
	uint32_t qual, quals[12];
	ssize_t bytes;
	struct iovec ev_vec[MAX_IO_EVS];
	int ev_entries;
	size_t ev_len;

	count++;
	bytes = read(fd, &evt, sizeof (evt));
	if (bytes == 0)
	    break;
	if (bytes != sizeof (evt))
	    fatal("read error, expected %u, got %u", sizeof (evt), bytes);

	fprintf(fp, "record %d, event id %u\n", count, evt);
	
	if (IS_USER_EVENT(evt)) {
	    uint32_t spare, size;
	    int i, numb_quals;

	    EVL_INIT();
	    EVL(my_time);
	    EVL(spare);
	    EVL(size);
	    READV(fd);
	    READA(fd, (char*) quals, size);

	    fprintf(fp, "    USER\n"
		    "\ttime %#x = %fs\n"
		    "\tspare %#x\n"
		    "\tsize %u\n",
		    my_time, my_time * tick, spare, size);
	    numb_quals = size/sizeof quals[0];
	    for (i = 0; i < numb_quals; i++)
		fprintf(fp, "\tquals[%d] %u\n", i, quals[i]);
	} else if (IS_INT_ENT_EVENT(evt)) {
	    READ(fd, qual);
	    fprintf(fp, "    INT_ENT\n"
		"\tqual %#x = %fs\n",
		qual, (float)qual * tick);
	} else {
	    switch(evt) {
	    case EVENT_CONFIG: {
		uint32_t revision;	/* WV_REV_ID | WV_EVT_PROTO_REV */
		uint32_t timestampFreq;	/* frequency of timer clock */
		uint32_t timestampPeriod;/* maximum timer count before rollover */
		uint32_t autoRollover;	/* TRUE = target supplies EVENT_TIMER_ROLLOVER */
		uint32_t clkRate;	/* not used */
		uint32_t collectionMode;/* RUE == context switch mode, otherwise, FALSE */
		uint32_t loc_proc;

		EVL_INIT();
		EVL(revision);
		EVL(timestampFreq);
		EVL(timestampPeriod);
		EVL(autoRollover);
		EVL(clkRate);
		EVL(collectionMode);
		EVL(loc_proc);
		READV(fd);
		tick = 1.0 / timestampFreq;
		fprintf(fp, "    CONFIG\n"
			   "\trevision %#x\n"
			   "\ttimestampFreq %u/s  = 1/%gns\n"
			   "\ttimestampPeriod %u\n"
			   "\tautoRollover %u\n"
			   "\tclkRate %u\n"
			   "\tcollectionMode %u\n"
			   "\tprocessor %u\n", 
			   revision, timestampFreq, tick * 1.0e9,
			   timestampPeriod, autoRollover, clkRate,
			   collectionMode, loc_proc);
		break;
	    }
	    case EVENT_BUFFER: {
		uint32_t taskIdCurrent;	/* the currently running task */

		READ(fd, taskIdCurrent);
		fprintf(fp, "    BUFFER\n"
			   "\ttaskIdCurrent %u\n",
			   taskIdCurrent);
		break;
	    }
	    case EVENT_TASKNAME: {
		uint32_t status;
		uint32_t priority;
		uint32_t taskLockCount;
		int32_t tid;
		uint32_t nameSize;
		char name[128];

		EVL_INIT();
		EVL(status);
		EVL(priority);
		EVL(taskLockCount);
		EVL(tid);
		EVL(nameSize);
		READV(fd);
		READA(fd, name, nameSize);
		fprintf(fp, "    TASKNAME\n"
		       "\tstatus %u\n"
		       "\tpriority %u\n"
		       "\ttaskLockCount %u\n"
		       "\ttid %d\n"
		       "\tnameSize %u\n"
		       "\tname %.*s\n",
		       status, priority, taskLockCount, tid,
		       nameSize, nameSize, name);
		break;
	    }
	    case EVENT_BEGIN: {
		uint32_t CPU;		/* MIPS = 40, R3000 = 41, R4000 = 44 */
		uint32_t bspSize;	/* ength of the following string */
		char bspName[128];	/* manufacturer of target copmuter */
		int32_t taskIdCurrent;	/* the currently running task */
		uint32_t collectionMode;/* RUE == context switch mode, otherwise, FALSE */
		uint32_t revision;	/* WV_REV_ID | WV_EVT_PROTO_REV */

		EVL_INIT();
		EVL(CPU);
		EVL(bspSize);
		READV(fd);

		EVL_INIT();
		EVLA(bspName, bspSize);
		EVL(taskIdCurrent);
		EVL(collectionMode);
		EVL(revision);
		READV(fd);
		fprintf(fp, "    BEGIN\n"
		       "\tcputype %u\n"
		       "\tmanufacturerNameSize %u\n"
		       "\tmanufacturerName %.*s\n"
		       "\ttaskIdCurrent %d\n"
		       "\tcollectionMode %u\n"
		       "\trevision %#x\n", 
		       CPU, bspSize, bspSize, bspName,
		       taskIdCurrent, collectionMode, revision);
		break;
	    }
	    case EVENT_TIMER_SYNC: {
		uint32_t spare;

		EVL_INIT();
		EVL(my_time);
		EVL(spare);
		READV(fd);
		fprintf(fp, "    TIMER_SYNC\n"
			"\ttime %#x = %fs\n"
			"\tspare %#x\n",
			my_time, (float)my_time * tick,
			spare);
		break;
	    }
	    case EVENT_TIMER_ROLLOVER:
		READ(fd, my_time);
		fprintf(fp, "    TIMER_ROLLOVER\n"
		    "\ttime %#x = %fs\n",
		    my_time, my_time * tick);
		break;
	    case TSTAMP_EV_INTREXIT:
		/* we don't need the following case statement because it is
		 *  the same as the previous two and we get a compile error, if
		 *  it is changed so it is unique we should uncomment it
		 */
		/*case TSTAMP_EV_EVINTREXIT:*/
	    case EVENT_WIND_EXIT_IDLE:
		READ(fd, qual);
		fprintf(fp, "    %s\n"
		    "\tqual %#x = %fs\n",
		    evt==TSTAMP_EV_INTREXIT ? "INTREXIT" : "EXIT_IDLE",
		    qual, qual * tick);
		break;
	    case TSTAMP_EV_SIGRECV: 
	    case TSTAMP_EV_SIGSEND: 
	    case TSTAMP_EV_EOSWITCH:
	    case TSTAMP_EV_EOSWITCH_RTPRI:
	    case EVENT_TASK_STATECHANGE:
		READA(fd, quals, 3);
		fprintf(fp, "    %s\n"
			"\tquals[0] %#x = %fs\n"
			"\tquals[1] %d\n"
			"\tquals[2] %d\n",
			evt==TSTAMP_EV_SIGRECV ? "SIGRECV" :
			evt==TSTAMP_EV_SIGSEND ? "SIGSEND" :
			evt==TSTAMP_EV_EOSWITCH ? "EOSWITCH" :
			evt==TSTAMP_EV_EOSWITCH_RTPRI ? "EOSWITCH_RTPRI"
			: "TASK_STATECHANGE",
			quals[0], (float)quals[0] * tick,
			quals[1], quals[2]);
		break;
	    case TSTAMP_EV_EXIT:
		READ(fd, qual);
		fprintf(fp, "    EXIT\n" "\tqual %d\n", qual);
		break;
	    default:
		fprintf(fp, "error: record %d, undefined event %#x\n",
		    count, evt);
		break;
	    }
	}
    }
}
