#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/syssgi.h>
#include <sys/mman.h>
#include <sys/signal.h>

int interval = 10000;	/* 10msec */
unsigned long long get_time();

void sigalrm();
main(int argc, char **argv)
{
   	extern int		optind;
   	extern char		*optarg;
	int			c;
	int 			err = 0;
	unsigned 		fd;
	int			poffmask;

	/*
	 * Parse arguments.
	 */
	setbuf(stdout, NULL);
	while ((c = getopt(argc, argv, "i:")) != EOF) {
	  	switch (c) {
	  	case 'i':
	       		if ((interval = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		default:
			break;
		}
	}
	if (err) {
		exit(-1);
	}

	if (interval < 10000) 
		system("/etc/ftimer -f on -i on");
	sighold(SIGALRM);
	sigset(SIGALRM, sigalrm);
	alarm(1);
	sigrelse(SIGALRM);
	pause();
}

void
sigalrm()
{
	unsigned long long before, after;
	struct timeval 		tv;
	float			usec, cycle;

	tv.tv_sec = 0;
	tv.tv_usec = interval;

	before = get_time();
	if (select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tv) < 0) {
		fprintf(stderr,"select error\n");
		exit(-1);
	}
	after = get_time();
	cycle = (float)(after - before);
	if (cycle < 0) cycle += 0xffffffff;
	usec = (float)cycle/1000;
	if (interval < 10000) 
		system("/etc/ftimer -f off -i off");
	printf("Elapsed time is %f usec\n",usec);
	if (interval < 10000 && usec >= 10000) {
		fprintf(stderr,"Elapsed time is %f usec instead of %d\n",usec, interval);
		exit(-1);
	}
}


		
/*
 * Use the POSIX clock calls to get the time, convert to nsec. Handle wrap
 * for CPU's that have 32 bit counters.
 */
unsigned long long
get_time()
{
	static unsigned long long lasttime = 0;
	static long long adj = 0;
	struct timespec tp;
	long long nps = (long long)NSEC_PER_SEC;
	unsigned long long time;
	int x;
	clock_gettime(CLOCK_SGI_CYCLE, &tp);
	time = tp.tv_sec * (long long)NSEC_PER_SEC;
	time += tp.tv_nsec;
	time += adj;
	if ((long long)time < 0) printf("time < 0\n");
	if (time < lasttime) {
		printf("wrap \n");
		adj += (long long)1 << 32;
		time += (long long)1 << 32;
	}
	lasttime = time;
	return(time);
}
	
