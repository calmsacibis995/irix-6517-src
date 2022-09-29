#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/rtmon.h>
#include <sys/param.h>

/*
 * Argument parsing ...
 */
static char *usage = "usage: %s: [-x] file\n";
static char *myname;
static char *filename;
static int outputbase = 10;
static int output32 = 0;

static void parse_args(int argc, char **argv);

void
main(int argc, char** argv)
{
    int fd;
    unsigned long record;
    uint64_t lasttime;

    parse_args(argc, argv);
    if ((fd = open(filename,O_RDONLY)) < 0) {
	fprintf(stderr, "%s: open failed: %s\n", myname, strerror(errno));
	exit(EXIT_FAILURE);
	/*NOTREACHED*/
    }

    for (record = 0, lasttime = 0; ; record++) {
	int cc, i;
	tstamp_event_entry_t event;
	
	cc = read(fd, &event, sizeof event);
	if (cc < sizeof event) {
	    if (cc == 0)
		exit(EXIT_SUCCESS);
	    if (cc < 0) {
		fprintf(stderr, "%s: read failed: %s\n", myname, strerror(errno));
		exit(EXIT_FAILURE);
	    }
	    fprintf(stderr, "%s: partial event read\n", myname, strerror(errno));
	    exit(EXIT_FAILURE);
	}

	if (record > 0)
	    putchar('\n');
	printf("record %lu, event id %d ", record, event.evt);
	if (IS_INT_ENT_EVENT(event.evt))
	    printf("(Interrupt entry %d)\n",INT_LEVEL(event.evt));
	else if (IS_KERNEL_USER_EVENT(event.evt))
	    printf("(Kernel user event %d)\n",event.evt - MIN_KERNEL_ID);
	else if (IS_USER_EVENT(event.evt))
	    printf("(User event %d)\n",event.evt - MIN_USER_ID);
	else
	    switch (event.evt) {
	    case EVENT_TASKNAME:
		printf("(event taskname)\n");
		printf("\tpid %d named %s\n", event.reserved,
		       (char *)&event.tstamp);
		continue;
	    case TSTAMP_EV_CONFIG: {
		tstamp_config_event_t *config = (tstamp_config_event_t *)&event;
		printf("(config)\n");
		printf("\t%-16s %d\n", "revision", config->revision);
		printf("\t%-16s %d\n", "cpunum", config->cpunum);
		printf("\t%-16s %lld\n", "timestampperiod", config->timestampperiod);
		printf("\t%-16s %d\n", "timestampfreq", config->timestampfreq);
		printf("\t%-16s %lld\n", "collectionmode", config->collectionmode);
		printf("\t%-16s %d\n", "manufacture", config->manufacture);
		printf("\t%-16s %d\n", "cputype", config->cputype);
		printf("\t%-16s %d\n", "cpufreq", config->cpufreq);
		continue;
	    }
	    case TSTAMP_EV_EVINTREXIT:
		printf("(Interrupt exit)\n");
		break;
	    case TSTAMP_EV_EOSWITCH:
		printf("(process dispatch)\n");
		break;
	    case TSTAMP_EV_EOSWITCH_RTPRI:
		printf("(process dispatch with RT pri)\n");
		break;
	    case TSTAMP_EV_SIGSEND:
		printf("(sigsend)\n");
		break;
	    case TSTAMP_EV_SIGRECV:
		printf("(sigrecv)\n");
		break;
	    case TSTAMP_EV_EXIT:
		printf("(process exit)\n");
		break;
	    case EVENT_TASK_STATECHANGE:
		printf("(task state change)\n");
		break;
	    case EVENT_WIND_EXIT_IDLE:
		printf("(switch to idle)\n");
		break;
	    default:
		printf("\n");
		break;
	    }
	if (lasttime)
	    printf("\ttimestamp	%lld(%lld)\n",
		   event.tstamp, event.tstamp - lasttime);
	else
	    printf("\ttimestamp	%lld\n",
		   event.tstamp);
	lasttime = event.tstamp;
	printf("\tqualifiers:\n");
	for (i = 0; i < TSTAMP_NUM_QUALS; i++)
	    printf(outputbase == 10
		   ? "\t\tqual[%d] %lld (%ld %ld)\n"
		   : "\t\tqual[%d] %#018llx (%#010lx %#010lx)\n",
		   i, event.qual[i],
		   (event.qual[i]>>32) & 0xffffffff,
		   event.qual[i] & 0xffffffff);
    }
}

static void
parse_args(int argc, char **argv)
{
    int c;
    extern char *optarg;
    extern int optind;

    myname = strrchr(argv[0], '/');
    if (myname != NULL)
	myname++;
    else
	myname = argv[0];

    while (( c = getopt(argc, argv, "f:sx")) != -1) {
	switch(c) {
	default:
	case '?':
	    fprintf(stderr, usage, myname);
	    exit(EXIT_FAILURE);
	case 'f':
	    fprintf(stderr, "%s: -f option is deprecated\n",
		    myname);
	    filename = optarg;
	    break;
	case 's':
	    output32 = 1;
	    break;
	case 'x':
	    outputbase = 16;
	    break;
	}
    }
    if (argc-optind != 1 && filename == NULL) {
	fprintf(stderr, usage, myname);
	exit(EXIT_FAILURE);
    }
    filename = argv[optind];
}
