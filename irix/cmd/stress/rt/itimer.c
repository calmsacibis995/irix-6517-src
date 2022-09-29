#define _BSD_SIGNALS	1
#include	<stdio.h>
#include	<malloc.h>
#include	<sys/types.h>
#include	<sys/signal.h>
#include	<sys/time.h>
#include	<sys/schedctl.h>
#include	<sys/lock.h>
#include	<sys/fcntl.h>
#include	<sys/file.h>
#include	<sys/syssgi.h>
# include	<sys/mman.h>
# include	<sys/times.h>
#include	<math.h>
unsigned long long get_time(void);
void itimer_setup(char *);

/*
** Check accuracy of itimer delivery
** set up the cycle counter on IO3 board
** record the cycle counter time stamp for each itimer signal
** at the end give a summary of max, min, average and standard deviation
** run this program retricted on non-clock processor
** run this program restircited on clock processor
** do this at various frequency from 833us to 10ms
*/
int	isolate;
int	restrict;
int	p_cpu = -1;
int	c_cpu = -1;
int	sec = 1;
long	interval =  10000;
int	ndpri = -1;
int	verbose;
struct tms tmsbuf;
unsigned long long startio_tick, endio_tick;
clock_t startlbolt, endlbolt;

/*
** this is the structure used to communicate
** all critical data should be here for mpin
*/
volatile struct com {	
	int		nintr;		/* # of SGIALARM */
	float	max;
	float	min;
	float	sum;
	int		maxi;
	int		mini;
	unsigned 	cycleval;	/* time value of cycle counter in pico-sec */
} comarea;
#define SECOND	1000000			/* in usec */
volatile unsigned long long *latency;

void catcher(void);

main(argc, argv)
int argc;
char **argv;
{
   	extern int		optind;
   	extern char		*optarg;
	int			c;
	int 			err = 0;

	/*
	 * Parse arguments.
	 */
	setbuf(stdout, NULL);
	while ((c = getopt(argc, argv, "rIvn:i:p:c:s:")) != EOF) {
	  	switch (c) {
	  	case 'I':
			isolate++;
			break;
	  	case 'r':
			restrict++;
			break;
	  	case 'i':
	       		if ((interval = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 'p':
	       		if ((p_cpu = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 'c':
	       		if ((c_cpu = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 'n':
	       		if ((ndpri = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 's':
	       		if ((sec = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 'v':
			verbose++;
			break;
		default:
			break;
	 	}
     	}

	if (isolate && (p_cpu == -1))
		err++;
	if (restrict && (p_cpu == -1))
		err++;
	

	if (err)
		usage();

	/* set up sighandler, lock memory, NPRI, MUSTRUN,... */
	(void)itimer_setup((char *)&err);
	itimer_loop();
}

/* 
** set up io timer for measurement and itimer 
** lock critical memory
** set mustrun
** set clock processor
*/
void
itimer_setup(char *sp)
{
	int		fd, tmp;
	struct itimerval	itv;
	char	*stack;
	struct sigvec svec;
	struct sigstack ss;
	int poffmask;

	if (getuid() != 0) {
		fprintf(stderr,"Must be super user!\n");
		exit(0);
	}
	stack = (char *)malloc(10*1024);
	if (stack == (char *)0) {
		fprintf(stderr,"can't malloc\n");
		exit(-1);
	}
		
	latency = (unsigned long long *)malloc(sizeof(unsigned long long)*(SECOND/interval)*sec);
	if (latency == (unsigned long long*)0) {
		fprintf(stderr,"can't malloc\n");
		exit(-1);
	}
    	/*
     	* pin memory, stack, text
     	*/
    	pinmem(sp);
	comarea.max = 0;
	comarea.min = 10000000;
	comarea.sum = 0;

	/* set up must run */
	if (p_cpu != -1)
		if (sysmp(MP_MUSTRUN, p_cpu) == -1) {
			fprintf(stderr,"Failed MP_MUSTRUN on %d\n",p_cpu);
			exit(-1);
		}

	/* set up clock processor */
	if (c_cpu != -1)
		if (sysmp(MP_CLOCK, c_cpu) == -1) {
			fprintf(stderr,"Failed MP_CLOCK on %d\n",c_cpu);
			exit(0);
		}

	if (isolate) {
		if (sysmp(MP_ISOLATE, p_cpu) == -1) {
			fprintf(stderr,"Failed MP_ISOLATE on %d\n",p_cpu);
			exit(-1);
		}
	}
	else if (restrict) {
		if (sysmp(MP_RESTRICT, p_cpu) == -1) {
			fprintf(stderr,"Failed MP_ISOLATE on %d\n",p_cpu);
			exit(-1);
		}
	}

	/* raise priority */
	if (ndpri != -1)
		if (schedctl(NDPRI, 0, ndpri) == -1) {
			fprintf(stderr,"Failed NDPRI\n");
		}

     	/*
      	* Set up signal handling.  Initialize the alarm 
      	* signal to be "held", which means that 
      	* timer pops will be ignored until we
      	* "release" the signal.
      	*/
	svec.sv_handler = catcher;
	svec.sv_mask = 0;
	svec.sv_flags = SV_ONSTACK;
	sigvec(SIGALRM, &svec, (struct sigvec *)0);

	ss.ss_sp = stack + 10*1024 - 1;
	ss.ss_onstack = 0;
	sigstack(&ss, (struct sigstack *)0);
     	sighold(SIGALRM);


     	/*
      	* Set up timer.  The interval is the time 
      	* between each successive timer pop.  
      	* The value is the initial value of the 
      	* timer, which can be anything.  We will 
      	* set the timer to start 10ms from now, and 
      	* keep interrupting every 10ms thereafter.
      	*/
	if (interval < 10000) {
		/* must use fast timer */
		system("ftimer -f on -i on");
	}
     	itv.it_interval.tv_sec = 0;
     	itv.it_interval.tv_usec = interval;
     	itv.it_value = itv.it_interval;
     	setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);

/*
XXX	release it after Xsetup
     		sigrelse(SIGALRM);
*/
}

itimer_loop()
{
     	register unsigned count = (unsigned)(sec*(SECOND/interval));

	startlbolt = times(&tmsbuf);
	startio_tick = get_time();
	/* release the signal */ 
     	sigrelse(SIGALRM);

     	while (comarea.nintr <= count) {}
	endio_tick = get_time();
	endlbolt = times(&tmsbuf);

	sighold(SIGALRM);
	/* display results so far */
	display_result();
	clean_up();
}

clean_up()
{
	if (p_cpu != -1) {
		if (isolate) {
			if (sysmp(MP_UNISOLATE, p_cpu) == -1) 
				printf("Failed UNISOLATE\n");
		}
		else if (restrict) {
			if (sysmp(MP_EMPOWER, p_cpu) == -1) 
				printf("Failed EMPOWER\n");
		}
	}
}

void
catcher()
{
	float 		diff;
	struct itimerval	itv;
	static	int		first = -10;

	if (first != 0) {
		first++;
		return;
	}
	latency[comarea.nintr] = get_time();
	if (comarea.nintr == 0) {
		comarea.nintr++;
		return;
	}
#ifdef NOTDEF
	if (latency[comarea.nintr] > latency[comarea.nintr-1])
		diff = latency[comarea.nintr]-latency[comarea.nintr-1];
	else
		diff = (unsigned)0xffffffff-latency[comarea.nintr-1]+latency[comarea.nintr-1];
	diff = (float)(diff*comarea.cycleval)/(unsigned)1000000;
	comarea.sum += diff;
	if (comarea.max < diff) {
		comarea.max = diff;
		comarea.maxi = comarea.nintr;
	}
	if (comarea.min > diff) {
		comarea.min = diff;
		comarea.mini = comarea.nintr;
	}
	if (verbose && (comarea.nintr % 100) == 0)
		printf("%d signals. Interval: Max=%f(%d) Min=%f(%d) Ave=%f\r",comarea.nintr, comarea.max, comarea.maxi, comarea.min, comarea.mini, comarea.sum/comarea.nintr);
#endif
	
	comarea.nintr++;
	if (comarea.nintr > sec*(SECOND/interval)) {
     		itv.it_interval.tv_sec = 0;
     		itv.it_interval.tv_usec = 0;
     		itv.it_value = itv.it_interval;
     		setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);
	}
}

/*
** code to lock critical memory in core lock 
** stack, signal handler and the communication area
** lock all text between catcher() and pinmem()
*/
pinmem(sbot)
   char		*sbot;
{
#ifdef NOTDEF
    	/*
     	* Pin down various pieces of critical 
     	* memory.  Start with the stack.  
     	* Allow about 2K for the stack (signal 
     	* handlers run on the same stack).
	* XXX check for mpin status
     	*/
    	/* stack grows downward */
   	mpin(sbot - (2*1024), 2*1024);

    	/* pin data areas */
	mpin((char *)latency, (sizeof(unsigned)*(SECOND/interval)*sec));

    	/* pin text areas */
   	mpin((char *) catcher, (int) pinmem - (int) catcher);
#endif
	volatile int *sp = (volatile int *)sbot;

	sp -= 4096*4;
	*sp = 0;	/* grow the stack */
	if (plock(PROCLOCK) == -1) {
		perror("pinmem");
	}
}


display_result()
{
	unsigned	i;
	float		diff, diff2;

	printf("\nItimer Accuracy\n");
	printf("Set to run for %d seconds.\n",sec);
	printf("NDPRI=%d Clock processor=%d Mustrun=%d.\n",ndpri, c_cpu, p_cpu);
	if (isolate)
		printf("Processor %d is isolated\n",p_cpu);
	
	printf("Total signals received %d\n",comarea.nintr);
	for (i=1; i<comarea.nintr; i++) {
		if (latency[i] > latency[i-1])
			diff = latency[i]-latency[i-1];
		else
			diff = (unsigned)0xffffffff-latency[i-1]+latency[i];
/*		diff = (float)(diff*comarea.cycleval)/(unsigned)1000000; */
		diff = (float)(diff/1000); /* Convert nsec to usec */
		if (verbose) 
			printf("i=%d, diff=%f\n",i, diff);
		comarea.sum += diff;
		if (comarea.max < diff) {
			comarea.max = diff;
			comarea.maxi = comarea.nintr;
		}
		if (comarea.min > diff) {
			comarea.min = diff;
			comarea.mini = comarea.nintr;
		}
	}
	printf("Delay between signals: Average=%f Max=%f Min=%f Expected=%d\n",
		comarea.sum/(comarea.nintr),comarea.max,comarea.min,interval);
	if (startio_tick > endio_tick)
		diff = (unsigned int)0xffffffff - startio_tick + endio_tick;
	else
		diff = endio_tick - startio_tick;
	diff = (float) diff/1000;
/*	diff = (float)(diff*comarea.cycleval)/(unsigned)(1000000); */
	printf("Wall Time elapsed: %f(us)\n",diff);
	printf("Lbolt elapsed: %d(10ms tick)\n",endlbolt - startlbolt);
	diff = diff/1000; /* in ms */
	diff2 = (endlbolt-startlbolt)*10;
	if (diff2 > diff)
		diff = diff2 - diff;
	else
		diff = diff - diff2;
	if (diff > (float)10000 && ((endlbolt-startlbolt) < 250))
		printf("Loosing clock tick!!!");

}



usage()
{
	fprintf(stderr,"itimer\t[-c cpu] 	set clock processor to 'cpu''\n");
	fprintf(stderr,"\t[-i] 		timer interval\n");
	fprintf(stderr,"\t[-p cpu] 	lock program to 'cpu'\n");
	fprintf(stderr,"\t[-s sec] 	test duration in seconds\n");
	fprintf(stderr,"\t[-v]		verbose\n");
	fprintf(stderr,"\t[-I]		isolate processor, used with '-p' \n");
	exit(-1);
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
		printf("wrap at %d \n",comarea.nintr);
		adj += (long long)1 << 32;
		time += (long long)1 << 32;
	}
	lasttime = time;
	return(time);
}
