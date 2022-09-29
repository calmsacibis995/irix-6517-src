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
 * This test check out the speed at which hi-band NDPRI processes got
 * woken up.
 * All of these are sprocs to avoid having to setup shared memory. 
 * Parent process A forks two child processes B and C, then set mustrun on 
 * master CPU.
 * Process B will set itself to be must run on slave CPU then block itself.
 * Process C will set itself to be must run on master CPU then block itself.
 * Process A also forks process D on slave CPU to compete with process B.
 * Process D is set to a lower priority and is cpu intensive.
 * Process A waits for B and C to block themselves then just unblockproc() them.
 * Process A then blocks itself and use a 10ms itimer to wake it up.
 * Once process B, C got woken up, they will immediately block themselves
 * This is repeated for a number of loop that is specified by user.
 * Each process will keep track of the time between a wakeup issued by the 
 * other process until it is actually running.
 * At the end the speed at which each of these processes are woken up is
 * displayed(max, min, average).
 * Process A, B, C  will have hi-band NDPRI while process D is not
 *
 * The dispatch latency between B,C should be very close. If it is then
 * it shows that hi-band NDPRI dispatch latency is not affected by
 * clock frequency and is uniform within a machine regardless of where
 * the process is supposed to run.
 */

typedef struct {
	int	unblocktime;	/* other process issues the unblock call to 
				this process*/
	int	disptime;	/* this process starts running */ 
	int	latency;	/* difference */
} disp_t;

#define PROCB 0
#define PROCC 1

#define GETIOTIME() *iotimer_addr

disp_t *procdisp[2];
int Aid, Bid, Cid;		/* processes's id */
int loop;
volatile unsigned 	*iotimer_addr, *phys_addr;
int cycleval;
int mustrunproc = 1;
int masterproc;
int maxloop = 1000;
int interval = 10000;

int procA(), procB(), procC(), procD();

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
		perror("wantdisp: must be super user");
		exit(0);
	}

	if ((addr = malloc(2*maxloop*sizeof(disp_t))) == NULL) {
		perror("wantdisp: can't malloc");
		abort();
	}

	procdisp[0] = (disp_t *)addr;
	addr += maxloop*sizeof(disp_t);
	procdisp[1] = (disp_t *)addr;

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


	if (sproc(procD, PR_SALL) == -1) {
		perror("wantdisp: failed sproc");
		abort();
	}


	if (schedctl(NDPRI, 0, NDPHIMAX) == -1) {
		perror("wantdisp: failed schedctl");
		abort();
	}


	if (sproc(procB, PR_SALL) == -1) {
		perror("wantdisp: failed sproc");
		abort();
	}

	if (sproc(procC, PR_SALL) == -1) {
		perror("wantdisp: failed sproc");
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
		for (proc=0; proc<2; proc++) {
		if (procdisp[proc][i].disptime > procdisp[proc][i].unblocktime) 
			latency = procdisp[proc][i].disptime - 
					procdisp[proc][i].unblocktime;
		else 
			latency = procdisp[proc][i].unblocktime - 
					procdisp[proc][i].disptime;
		procdisp[proc][i].latency = ((latency*(unsigned)cycleval)/(unsigned)1000000);
		}
	}
	
		
	printf("\nDispatch Latency for proc B\n");
	findlimit(&reslt, &procdisp[0][0]);
	printf("Average=%d var=%f samples size=%d\n",reslt.average,reslt.vars,
		loop);
	printf("Max latency = %d, maxi = %d\n",reslt.max, reslt.maxi);
	printf("Min latency = %d, mini = %d\n",reslt.min, reslt.mini);
	procBmax = reslt.max;
	printf("\nDispatch Latency for proc C\n");
	findlimit(&reslt, &procdisp[1][0]);
	printf("Average=%d var=%f samples size=%d\n",reslt.average,reslt.vars,
		loop);
	printf("Max latency = %d, maxi = %d\n",reslt.max, reslt.maxi);
	printf("Min latency = %d, mini = %d\n",reslt.min, reslt.mini);
	procCmax = reslt.max;

	if (procBmax - procCmax > 200) {
		fprintf(stderr,"wantdisp ERROR: unusually large dispatch latency\n");
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
		perror("wantdisp: procA failed mustrun");
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
	while (Bid == 0 || Cid == 0) {}
     	setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);
	sigrelse(SIGALRM);
	
	while (loop < maxloop) {
		/* unblock C, C will go back to sleep again */
		unblockproc(Cid);
		procdisp[PROCC][loop].unblocktime = GETIOTIME();
		/* unblock B, B will go back to sleep again */
		unblockproc(Bid);
		procdisp[PROCB][loop].unblocktime = GETIOTIME();
		/* self block, itimer will wake A up */
		pause();
		loop++;
	}
	display_result();	
	unblockproc(Cid);
	unblockproc(Bid);
}

childexit()
{
printf("child exit\n");
}

catcher()
{
}

procB()
{
	if (sysmp(MP_MUSTRUN, mustrunproc) == -1) {
		perror("wantdisp: procB failed mustrun");
		abort();
	}

	Bid = getpid();
	while (loop < maxloop) {
		blockproc(Bid);
		if (loop >= maxloop) {
			break;
		}
		procdisp[PROCB][loop].disptime = GETIOTIME();
	}
	exit(0);
}

	
procC()
{
	if (sysmp(MP_MUSTRUN, masterproc) == -1) {
		perror("wantdisp: procC failed mustrun");
		abort();
	}

	Cid = getpid();
	while (loop < maxloop) {
		blockproc(Cid);
		if (loop >= maxloop) {
			break;
		}
		procdisp[PROCC][loop].disptime = GETIOTIME();
	}
	exit(0);
}
		
procD()
{
	if (sysmp(MP_MUSTRUN, mustrunproc) == -1) {
		perror("wantdisp: procD failed mustrun");
		abort();
	}

	if (schedctl(NDPRI, 0, NDPHIMIN) == -1) {
		perror("wantdisp: failed schedctl");
		abort();
	}

	while (loop < maxloop) {}
	exit(0);
}

