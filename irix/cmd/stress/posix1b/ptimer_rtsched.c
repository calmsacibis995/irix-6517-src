#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/lock.h>
#include <signal.h>
#include <malloc.h>
#include <ulocks.h>


/* frequency of the interrupt. */
#define HZ 200
#define LOOPS 1000

#define SQR(x) ((x)*(x))

extern void init_frc(void);
extern long long get_frc(void);
#define READ_CLOCK CLOCK_REALTIME
#if 0
#define TIMEOUT_CLOCK CLOCK_REALTIME
#else
#define TIMEOUT_CLOCK CLOCK_SGI_FAST
#endif
int numint;                   /* number of interrupts done */
long long *timings;
struct itimerspec itv;

/*ARGSUSED*/
void catcher(int dummy)
{
  numint++;
/*
  timings[numint] = get_frc();
*/
}

main()
{
	int i;
	long long avg,var,max,min;
	int minloop =0,maxloop=0;
	timer_t mytimer;
	/* create storage for measurements */
	timings=calloc(LOOPS * 2, sizeof(long long));


	/* create a timer */
	sigset(SIGALRM, catcher);
	sighold(SIGALRM);
  
        /*
         * if privileged user
         * run timer with CLOCK_SGI_FAST
         */
        if (geteuid() == 0) {
		/* enable real-time scheduling */
		if (schedctl(NDPRI,0,NDPHIMAX)<0) {
			perror("schedctl");
			exit(1);
		}
		/* lock the complete process in memory */
		if (plock(PROCLOCK)<0) {
			perror("ptimer_rtsched: ERROR - plock");
			exit(5);
		} else
			printf("ptimer_rtsched: plock(PROCLOCK) succeeded\n");
		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_nsec = 1000000000/HZ;
		itv.it_value = itv.it_interval;
		if (timer_create(CLOCK_SGI_FAST, NULL, &mytimer)< 0){
			perror("timer_create");
			exit(5);
		}
		printf("ptimer_rtsched: CLOCK_SGI_FAST timer created\n");
        } else {
		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_nsec =  3 * (1000000000/HZ);
		itv.it_value = itv.it_interval;
		if (timer_create(CLOCK_REALTIME, NULL, &mytimer)< 0){
			perror("timer_create");
			exit(5);
		}
		printf("ptimer_rtsched: CLOCK_REALTIME timer created\n");
	}


	printf("ptimer_rtsched: starting timer\n");
	/* initialize free running clock */
	(void) get_frc();
	if(timer_settime(mytimer, 0, &itv, NULL) < 0) {
		perror("timer_settime");
		exit(3);
	}

	sigrelse(SIGALRM);

	pause();
	numint = -2;
	get_frc();
	while (numint<LOOPS) {
		long long time;
		pause();
		time = get_frc();
		if (numint >= 0)
			timings[numint] = time;
	}

 	/* show results */
	avg=0; var=0;
	max=0; min=itv.it_interval.tv_nsec *100;

	for (i=0; i<LOOPS; i++) {
		/* printf("%f\n",timings[i]); */
		avg+=timings[i];
		if (timings[i]>max){
			max=timings[i];
			maxloop = i;
		}
		if (timings[i]<min){
			min=timings[i];
			minloop = i;
		}
	}

	avg/=LOOPS;

	for (i=0; i<LOOPS; i++) {
		var+=SQR((avg-timings[i]));
	}

/*  var=sqrt(var/LOOPS); */
	printf("Requested %ld usecs per timer\n",itv.it_interval.tv_nsec/1000);
	printf("avg %lld +- var %lf\n",avg,sqrt(var/LOOPS));
	printf("[min %lld(try %d) , max %lld(try %d)]\n",min,minloop,max,maxloop);
	return(0);
}

long long
get_frc(void)
{
	struct timespec ts;
	long long usec_time, time;
	static long long last = 0;		
	clock_gettime(READ_CLOCK, &ts);
	time = (ts.tv_nsec/1000LL) + (ts.tv_sec*1000000LL);
	usec_time = time - last;
	last = time;
	if (usec_time > 100000 && numint > 5)
		printf("ptimer_rtsched: usec is %lld and numint is %ld\n",
						usec_time, numint);
	return(usec_time);
	
}
