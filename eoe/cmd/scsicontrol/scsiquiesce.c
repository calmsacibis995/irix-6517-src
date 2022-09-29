#define _BSD_SIGNALS

#include <sys/types.h>
#include <sys/signal.h>
#include <stropts.h>
#include <sys/param.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/scsi.h>
#include <sys/lock.h>


void print_state(int state);
void signal_handler(int);
void signal_poll_handler(int);


#define VPRINTF if(verbose) printf

char verbose = 0;

void
sbusage(char *prog)
{
    fprintf(stderr,
        "usage: %s \t"
        "[-q <bus quiesce time>] [-t <quiesce in progress timeout>]\n"
        "[-v] <scsi_host_adapter number>\n",
        prog);
    exit(1);
}


/* globals */
struct scsi_ha_op	scsiioctl;

int  	fd;
char 	*fn;
int	bus = -1;
ulong  	quiesce_time = 0; /* this is the time remaining */
ulong     quiesce_time_usr = 0; /* this is what the user wanted */
int 	state = NULL;
char buf[256];

main(int argc, char **argv)
{
    int	 dopoll = 0;
    int  doquiesce = 0;
    ulong  in_progress_timeout = 10; /* default */
    int	 c;
    int	 errs;

    opterr = 0;	/* handle errors ourselves. */

    plock(PROCLOCK); /* lock this process down */

    signal(SIGINT,signal_handler);
    signal(SIGPOLL,signal_poll_handler);

    while ((c = getopt(argc, argv, "pvt:q:")) != -1)
    {
	switch (c)
	{
	case 'p':
	    dopoll = 1;
	    break;

	case 't':
	    in_progress_timeout = strtoul((const char *)optarg, NULL, 0);
	    VPRINTF("in_progress_timout is %d\n",in_progress_timeout);
	    break;

	case 'q':
	    doquiesce = 1;
	    quiesce_time_usr = quiesce_time = strtoul((const char *)optarg, NULL, 0);
	    VPRINTF("quiesce_time is %d\n",quiesce_time);
	    break;

	case 'v':
	    verbose = 1;
	    break;

	default:
	    sbusage(argv[0]);
	}
    }

    if (optind >= argc || optind == 1) /* need at 1 arg and one option */
	sbusage(argv[0]);
    if(in_progress_timeout && !doquiesce) {
	printf("quiesce in progress timeout is set but quiesce time is not\n");
	sbusage(argv[0]);
    }

    while (optind < argc)
    {
	bus = atoi(argv[optind++]);
	    sprintf(buf, "/hw/scsi_ctlr/%d/bus",bus);
	    fn = &buf[0];
	    fd = open(fn, O_RDONLY);
	    if(fd == -1)
	    {
		printf("%s:  ", fn);
		fflush(stdout);
		perror("cannot open");
		errs++;
		continue;
	    }

	if (dopoll)
	{

	    scsiioctl.sb_addr = (uintptr_t)&state;
	    if (ioctl(fd, SOP_QUIESCE_STATE, &scsiioctl) != 0)
	    {
		printf("%s:  ", fn);
		printf("poll failed: %s\n", strerror(errno));
		exit(1);
	    }

	    if(verbose) print_state(state);

	}

	if (doquiesce)
	{
	    volatile int state = 0;

	    scsiioctl.sb_opt = in_progress_timeout * HZ;
	    scsiioctl.sb_arg = (quiesce_time_usr ) * HZ;

	    VPRINTF("in_progress_timeout = %d seconds\nquiesce_time = %d seconds\n",in_progress_timeout,quiesce_time);

	    if (ioctl(fd, SOP_QUIESCE, &scsiioctl) != 0)
	    {
		printf("%s:  ", fn);
		printf("quiesce failed: %s\n", strerror(errno));
		exit(1);
	    }
	    printf("bus quiesce in progress: %s\n",fn);

	    do {
		VPRINTF("polling on QUIESCE_IS_COMPLETE\n");
		scsiioctl.sb_addr = (uintptr_t)&state;

		if (ioctl(fd, SOP_QUIESCE_STATE, &scsiioctl) != 0)
		{

		    printf("%s:  ", fn);
		    printf("poll failed: %s\n", strerror(errno));
		    exit(1);

		}
		if(state == NO_QUIESCE_IN_PROGRESS)
		{ /* timer must have popped */
		    printf("Failed to quiesce bus in alloted time (%d seconds)\n",in_progress_timeout);
		    exit(-1);
		}
		if(verbose) print_state(state);
	    } while (state != QUIESCE_IS_COMPLETE);


	    printf("bus quiesce successful %s\nwaiting %d seconds\nHit cntl-C to kill quiesce or ENTER to extend quiesce time\n",
	        fn,quiesce_time);

	    /* allow signal through */
	    ioctl(STDIN_FILENO ,I_SETSIG, S_INPUT );


	    do {

		sleep(1);
		scsiioctl.sb_addr = (uintptr_t)&state;
		if (ioctl(fd, SOP_QUIESCE_STATE, &scsiioctl) != 0)
		{
		    printf("%s:  ", fn);
		    printf("poll failed: %s\n", strerror(errno));
		    exit(1);
		}
		if(!(quiesce_time%5)) printf("%d ",quiesce_time);
		quiesce_time--;
		fflush(stdout);

	    } while (state != NO_QUIESCE_IN_PROGRESS);

	    printf("\n");

	    VPRINTF("about to start SOP_UN_QUIESCE\n");
	    if (ioctl(fd, SOP_UN_QUIESCE, &scsiioctl) != 0)
	    {
		printf("%s:  ", fn);
		printf("UNquiesce failed: %s\n", strerror(errno));
		exit(1);
	    }
	    printf("bus now unquiesced: %s\n",fn);

	    /* now scan the bus.  things might have changed */
	    printf("scanning the bus: %s\n",fn);
	    if (ioctl(fd, SOP_SCAN, &scsiioctl) != 0)
	    {
		printf("%s:  ", fn);
		printf("probe failed: %s\n", strerror(errno));
		exit(1);
	    }

	}

	close(fd);
    }
    plock(UNLOCK);

    return(errs);
}

void print_state(int state)
{
    switch (state)
    {
    case NO_QUIESCE_IN_PROGRESS:
	{
	    printf("NO_QUIESCE_IN_PROGRESS\n");
	    break;
	}
    case QUIESCE_IN_PROGRESS:
	{
	    printf("QUIESCE_IN_PROGRESS\n");
	    break;
	}

    case QUIESCE_IS_COMPLETE:
	{
	    printf("QUIESCE_IS_COMPLETE\n");
	    break;
	}
    }
}

void signal_handler(int sig)
{
    volatile i = sig;

    printf("\nSIGINT detected - unquiescing the bus: %s\n",fn);
    if (ioctl(fd, SOP_UN_QUIESCE, &scsiioctl) != 0)
    {
	printf("%s:  ", fn);
	printf("UNquiesce failed: %s\n", strerror(errno));
	exit(1);
    }

    printf("scanning the bus: %s\n",fn);
    if (ioctl(fd, SOP_SCAN, &scsiioctl) != 0)
    {
        printf("%s:  ", fn);
        printf("probe failed: %s\n", strerror(errno));
        exit(1);
    }

    exit(1);

}


void signal_poll_handler(int sig)
{
    char minutes[2];

    volatile i = sig;

    ioctl(STDIN_FILENO ,I_SETSIG, NULL );  /* don't take any for keyboard input till */
    /* we're done here			  */
tryagain:
    printf("\nFor how many minutes do you want to extend the quiesce? ");
    scanf("%s",minutes);

    scsiioctl.sb_opt =   HZ; /* assumed that we're in a quiesced state now */
    quiesce_time = atoi(minutes) * 60;
    if( (quiesce_time == 0) || (quiesce_time > (60*60)) )
    {
	printf("pick time between 1 and 60 minutes\n");
	goto tryagain;
    }
    scsiioctl.sb_arg = quiesce_time * HZ;

    VPRINTF("in_progress_timeout = %d seconds\nquiesce_time = %d seconds\n",scsiioctl.sb_opt/HZ,scsiioctl.sb_arg/HZ);


    scsiioctl.sb_addr = (uintptr_t)&state;
    if (ioctl(fd, SOP_QUIESCE_STATE, &scsiioctl) != 0)
    {
	printf("%s:  ", fn);
	printf("poll failed: %s\n", strerror(errno));
	exit(1);
    }

    if(state!=QUIESCE_IS_COMPLETE)
    {
	printf("Too late, bus is no longer quiesced\n");
	exit(1);
    }

    if (ioctl(fd, SOP_QUIESCE, &scsiioctl) != 0)
    {
	printf("%s:  ", fn);
	printf("quiesce failed: %s\n", strerror(errno));
	exit(1);
    }

    ioctl(STDIN_FILENO ,I_SETSIG, S_INPUT ); /* allow keyboard input to set sig */

}
