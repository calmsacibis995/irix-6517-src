# include	<sys/types.h>
# include	<sys/time.h>
# include	<sys/schedctl.h>
# include	<sys/sysmp.h>
# include	<sys/pda.h>
# include	<stdio.h>
# include	<sys/prctl.h>
# include	<sys/syssgi.h>
# include	<sys/fcntl.h>
# include	<sys/mman.h>
# include	<sys/signal.h>


/*
 * This test checks out the concept of lowestpri cpu
 * ProcA is a cpu intensive at NDHIMAX and must run on cpu 0
 * It has a 10ms itimer running which it uses to periodically wake up procB
 * which can run on any cpu.
 * ProcC is also a cpu intensive process that must run on cpu 1.
 * ProcB,C are both NDPRI processes but procB is at higher priority than procC.
 *
 * Since procA and procC will be running all the time, the kernel should
 * pick cpu 1 as the lowestpri cpu in the system and since procB is at
 * higher pri than procC, the kernel should preempt procC with procB
 * as soon as procB is ready to run. 
 * Statistic on how soon procB is dispatched will be shown at the end of the 
 * test.
 */

typedef struct {
	int	unblocktime;	/* other process issues the unblock call to 
				this process*/
	int	disptime;	/* this process starts running */ 
	int	latency;	/* difference */
} disp_t;

#define PROCB 0

#define GETIOTIME() *iotimer_addr

disp_t *procdisp;
int Aid, Bid, Cid;		/* processes's id */
int loop;
volatile unsigned 	*iotimer_addr, *phys_addr;
int cycleval;
int mustrunproc = 1;
int masterproc;
int maxloop = 1000;
int interval = 10000;
int notimer;

int procA(), procB(), procC();

usage()
{
	fprintf(stderr,"lowestpri [-l loop] [-s cpu] [-m cpu]\n");	
	exit(-1);
}

main(argc, argv)
int argc;
char **argv;
{
	int fd;
   	extern int		optind;
   	extern char		*optarg;
	int			c;
	int 			err = 0;
	int			addr;

	/*
	 * Parse arguments.
	 */
	setbuf(stdout, NULL);
	while ((c = getopt(argc, argv, "s:m:l:")) != EOF) {
	  	switch (c) {
	  	case 'l':
	       		if ((maxloop = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 'i':
	       		if ((interval = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 's':
	       		if ((mustrunproc = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 'm':
	       		if ((masterproc = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		default:
			break;
	 	}
     	}

	if (err)
		usage();

	if (getuid() != 0) {
		perror("lowestpri: must be super user");
		exit(0);
	}

	if ((addr = malloc(maxloop*sizeof(disp_t))) == NULL) {
		perror("lowestpri: can't malloc");
		abort();
	}

	procdisp = (disp_t *)addr;

	phys_addr = (volatile unsigned *)syssgi(SGI_QUERY_CYCLECNTR, 
							&cycleval);
	if (phys_addr == (unsigned *)-1) {
		fprintf(stderr,"can't query cycleval counter addr\n");
		exit(-1);
	}
	/* map in */
	fd = open("/dev/mmem", O_RDONLY);
	if (fd == -1) {
		fprintf(stderr,"can't open /dev/mmem\n");
		exit(-1);
	}
	iotimer_addr = (volatile unsigned *)mmap(0, 4, PROT_READ, MAP_PRIVATE, 
					fd, (int)phys_addr);

	if (sysmp(MP_NPROCS) < 2) {
		perror("wantdips: needs two processors");
		exit(0);
	}

	if (sproc(procC, PR_SALL) == -1) {
		perror("lowestpri: failed sproc");
		abort();
	}

	if (sproc(procB, PR_SALL) == -1) {
		perror("lowestpri: failed sproc");
		abort();
	}

	procA();
}

typedef struct {
	unsigned 	max;
	unsigned 	min;
	unsigned 	average;
	int		maxi;
	int		mini;
	float		vars;
} result_t;

display_result()
{
	result_t 	reslt;
	int i, proc, latency;
	int procBmax, procCmax;
	
	/* convert into time */
	for (i=0; i<loop; i++) {
		if (procdisp[i].disptime > procdisp[i].unblocktime) 
			latency = procdisp[i].disptime - 
					procdisp[i].unblocktime;
		else 
			latency = procdisp[i].unblocktime - 
					procdisp[i].disptime;
		procdisp[i].latency = ((latency*(unsigned)cycleval)/(unsigned)1000000);
	}
	
		
	printf("\nDispatch Latency for proc B\n");
	findlimit(&reslt, procdisp);
	printf("Average=%d var=%f samples size=%d\n",reslt.average,reslt.vars,
		loop);
	printf("Max latency = %d, maxi = %d\n",reslt.max, reslt.maxi);
	printf("Min latency = %d, mini = %d\n",reslt.min, reslt.mini);

	if (procBmax > 300) {
		fprintf(stderr,"lowestpri ERROR: unusually large dispatch latency\n");
		abort();
	}
}

findlimit(reslt, disp)
result_t *reslt;
disp_t *disp;
{
	unsigned sum;
	unsigned	c;
	float		diffxs, tmp;
	int	latency;


	reslt->min = reslt->max = disp[1].latency;
	sum = 0;
	for (c=0; c<loop; c++) {
		if (reslt->max < disp[c].latency) {
			reslt->max = disp[c].latency;
			reslt->maxi = c;
		}
		if (reslt->min > disp[c].latency) {
			reslt->min = disp[c].latency;
			reslt->mini = c;
		}
		sum += disp[c].latency;
	}

	/* Compute mean value for intervals */
	reslt->average = sum / loop;

	/* Compute variance on intervals */
	diffxs = 0.0;
	for (c=0; c<loop; c++) {
		tmp = (float)(disp[c].latency - reslt->average);
		diffxs = diffxs + (tmp*tmp);
	}
	reslt->vars = fsqrt(diffxs)/(loop - 1);
}


procA()
{
	struct itimerval itv;
	extern int catcher();
	extern int childexit();

	/* stay at master cpu */
	if (sysmp(MP_MUSTRUN, masterproc) == -1) {
		perror("lowestpri: procA failed mustrun");
		abort();
	}

	if (schedctl(NDPRI, 0, NDPHIMAX) == -1) {
		perror("lowestpri: failed schedctl");
		abort();
	}

	sigset(SIGCLD, childexit);
     	/*
      	* Set up signal handling.  Initialize the alarm 
      	* signal to be "held", which means that 
      	* timer pops will be ignored until we
      	* "release" the signal.
      	*/
     	sigset(SIGALRM, catcher);
     	sighold(SIGALRM);


     	/*
      	* Set up timer.  The interval is the time 
      	* between each successive timer pop.  
      	* The value is the initial value of the 
      	* timer, which can be anything.  We will 
      	* set the timer to start 10ms from now, and 
      	* keep interrupting every 10ms thereafter.
      	*/
     	itv.it_interval.tv_sec = 0;
     	itv.it_interval.tv_usec = interval;
     	itv.it_value = itv.it_interval;

	Aid = getpid();
	while (Bid == 0) {}
	sleep(1);
     	setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);
	sigrelse(SIGALRM);
	
	while (loop < maxloop) {
		/* unblock B, B will go back to sleep again */
		unblockproc(Bid);
		procdisp[loop].unblocktime = GETIOTIME();
		/* wait for itimer */
		notimer = 1;
		while (notimer) {}	
		loop++;
	}
	display_result();	
	unblockproc(Bid);
}

childexit()
{
printf("child exit\n");
}

catcher()
{
	notimer = 0;
}

procB()
{
	if (schedctl(NDPRI, 0, NDPHIMAX+1) == -1) {
		perror("lowestpri: failed schedctl");
		abort();
	}

	Bid = getpid();
	while (loop < maxloop) {
		blockproc(Bid);
		if (loop >= maxloop) {
			break;
		}
		procdisp[loop].disptime = GETIOTIME();
	}
	exit(0);
}

	
procC()
{
	if (sysmp(MP_MUSTRUN, mustrunproc) == -1) {
		perror("lowestpri: procC failed mustrun");
		abort();
	}

	if (schedctl(NDPRI, 0, NDPHIMAX+2) == -1) {
		perror("lowestpri: failed schedctl");
		abort();
	}

	while (loop < maxloop) {
	}
	exit(0);
}

