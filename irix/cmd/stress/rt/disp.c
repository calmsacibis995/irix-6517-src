#define _KMEMUSER
# include	<sys/types.h>
# include	<sys/param.h>
# include	<sys/time.h>
# include	<sys/schedctl.h>
# include	<sys/sysmp.h>
# include	<sys/pda.h>
# include	<signal.h>
# include	<setjmp.h>
# include	<stdio.h>
# include	<sys/prctl.h>
# include	<sys/syssgi.h>
# include	<sys/fcntl.h>
# include	<sys/mman.h>
# include	<sys/ipc.h>
# include	<sys/shm.h>
# include	<sys/lock.h>
# include	<sys/sema.h>
# include	<sys/proc.h>
# include	<sys/rte.h>

#define EVEREST	1
#define DEBUG	1
/*
** Check dispatch latency
** Huy Nguyen July 90
** TO run this program make sure that there is a /dev/rte
** (char device, major 87, minor xx)
** ALso make sure that the kernel USE rte.
** Needs two cpus.
** Invoked as disp -p0 -c1 -s100 -I -C
** for parent on cpu0, child on cpu 1, runing for 100 secs, 
** isolate cpu 1 with clock off.
*/
#define 	SECOND	1000000		/* in usec */

#define 	SHMSIZE	sizeof(struct com)
#define 	SHMKEY	62
int		shmid;

/*
** this is the structure used to communicate
** all critical data should be here for mpin
*/
struct com {	
	int	ppid;		/* parent process ID */
	int	cpid;		/* child process ID */
	int	spid;		/* sproc pid */
	int	nintr;		/* # of SGIALARM */
	int	nwakeup;	/* # of slave blockproc */
	rtedat_t rtdat;		/* data concerning intr latency */
	unsigned dispatch;	/* time when child process is dispatched */
	unsigned cycleval;	/* time value of cycle counter in pico-sec */
	unsigned intr_send2;
} ;
#define intr_snd	rtdat.intr_send
#define intr_rcv	rtdat.intr_recv
#define intr_rtrn	rtdat.intr_ret
#define slept		rtdat.slept
#define pid		rtdat.pid

extern void rtproc();
extern void child_exit();
extern void child_pexit();
extern void intr_exit();
extern void catcher();

volatile struct com		*comarea;
volatile unsigned 	*iotimer_addr, *phys_addr;
unsigned		*displat, *intrlat, *intrsrv;
unsigned		*disp2lat;
int 			rtefd;
#define	SKIP	0

/* option flags */
int	interactive = 0;		/* if set interrupt is sent upon user input */	
int	verbose;	
int	sec = 1;		/* for non-interactive only */
int	c_cpu = -1;		/* child CPU, must be specified by user */
int	p_cpu = -1;		/* parent CPU, optional */
int	s_cpu = -1;		/* sproc CPU, optional */
int	isolate;
int	clock_disable;
int	interval = 10000;
caddr_t	sproc_stk;
rtestat_t rtestat;

#define	STKSIZE	(16*0x1000)

main(argc, argv)
int	argc;
char 	**argv;
{
   	extern int		optind;
   	extern char		*optarg;
	int			c;
	int 			err = 0;
	/*
	 * Parse arguments.
	 */
	setbuf(stdout, NULL);
	while ((c = getopt(argc, argv, "xCIi:vp:c:s:S:")) != EOF) {
	  	switch (c) {
	  	case 'I':
			isolate++;
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
		case 'C':
			clock_disable++;
			break;
		case 's':
	       		if ((sec = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 'S':
			if ((s_cpu = strtol(optarg, (char **)0, 0)) < 0)
				err++;
			break;

		case 'v':
			verbose++;
			break;
		default:
			break;
	 	}
     	}

	if (c_cpu == -1)
		err++;

	if (err)
		usage();

	/* set up sighandler, lock memory, NPRI, MUSTRUN,... */
	rtsetup((char *)&err);
	rtloop();
}

usage()
{
	fprintf(stderr,"disp\t-c cpu 	lock child process to 'cpu'\n");
	fprintf(stderr,"\t[-i] 		timer interval\n");
	fprintf(stderr,"\t[-p cpu] 	lock parent process to 'cpu'\n");
	fprintf(stderr,"\t[-s sec] 	test duration in seconds\n");
	fprintf(stderr,"\t[-v]		verbose\n");
	fprintf(stderr,"\t[-I]		isolate child process\n");
	fprintf(stderr,"\t[-C]		turn off clock\n");
	fprintf(stderr,"\t[-S cpu]	run an sproc on cpu\n");
	exit(-1);
}

/*
*/
rtsetup(sp)
	char *sp;
{
	int		fd, tmp;
	struct itimerval	itv;

	if (getuid() != 0) {
		fprintf(stderr,"Must be super user!\n");
		exit(0);
	}
		
	/* need at least 2 cpu */
	if (sysmp(MP_NPROCS) < 2) {
		fprintf(stderr,"Need at least 2 processors\n");
		exit(0);
	}
	if ((displat = (unsigned *)malloc(sizeof(unsigned)*(SECOND/interval)*sec)) == (unsigned *)0) {
		fprintf(stderr,"can't malloc\n");
		exit(-1);
	}
	if ((disp2lat = (unsigned *)malloc(sizeof(unsigned)*(SECOND/interval)*sec)) == (unsigned *)0) {
		fprintf(stderr,"can't malloc\n");
		exit(-1);
	}
	if ((intrlat = (unsigned *)malloc(sizeof(unsigned)*(SECOND/interval)*sec)) == (unsigned *)0) {
		fprintf(stderr,"can't malloc\n");
		exit(-1);
	}
	if ((intrsrv = (unsigned *)malloc(sizeof(unsigned)*(SECOND/interval)*sec)) == (unsigned *)0) {
		fprintf(stderr,"can't malloc\n");
		exit(-1);
	}

	shmem_setup();
	phys_addr = (volatile unsigned *)syssgi(SGI_QUERY_CYCLECNTR, 
							&comarea->cycleval);
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
					fd, (__psint_t)phys_addr);
#ifdef EVEREST
	iotimer_addr += 1;
#endif
    	comarea->ppid = getpid();
    	comarea->dispatch = comarea->intr_snd = 0;

	/* open /dev/rte */
	rtefd = open("/dev/rte", O_RDONLY);
	if (rtefd == -1) {
		perror("can't open /dev/rte");
		exit(1);
	}

    	/*
     	* pin memory, stack, text
     	*/
    	pinmem();

    	tmp = fork();
	/* child run rtproc() */
	if (tmp == 0) {
		rtproc();
	}
	else 
		comarea->cpid = tmp;

	if (p_cpu != -1) {
		if (sysmp(MP_MUSTRUN, p_cpu) == -1) {
			fprintf(stderr,"Failed MP_MUSTRUN on %d\n",p_cpu);
			exit(-1);
		}
	}
	else {
		fprintf(stderr,"main process is not mustrun\n");
		exit(-1);
	}


	if (interactive)
		return;
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
     	setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);
/*
XXX	release it after Xsetup
     		sigrelse(SIGALRM);
*/

}

/*
** set up shared memory segment for communication
*/
shmem_setup()
{

	if ((shmid = shmget((key_t) SHMKEY, SHMSIZE, IPC_EXCL|IPC_CREAT|0777)) 
					== -1 ) { /* create the partition */
		if ((shmid = shmget((key_t) SHMKEY, SHMSIZE, 0777)) == -1 ) {
				perror("shmget (master)");
				exit(-1);	/* or attach if already there*/
		}
		if (shmctl(shmid,IPC_RMID,(struct shmid_ds *) 0) == -1) {
			perror("shmctl (master)");	/* and get rid of it */
		}

		if ((shmid = shmget((key_t) SHMKEY, SHMSIZE,
			IPC_EXCL|IPC_CREAT|0777)) == -1 ) {	/* and retry */
				perror("shmget (master) retry");
				exit(-1);
		}
	}
	if ((__psint_t) (comarea=(struct com *) shmat(shmid,(char *) 0,0)) == -1)
	{
		perror("shmat (master)");
		exit(-1);
	}	
	
} 


shmem_sattch()
{
	if ((shmid = shmget((key_t) SHMKEY, SHMSIZE, 0777)) == -1 ) { 
		perror("shmget (slave)");
		exit(-1);
	}
	if ((__psint_t) (comarea=(struct com *) shmat(shmid,(char *) 0,0)) == -1)
	{
		perror("shmat (slave)");
		exit(-1);			/* and point to it	*/
	}
} 


shmem_dettch()			
{

	if (shmdt((char *) comarea) == -1) {	
		perror("shmdt (master)");
		exit(-1);
	}

	if (shmctl(shmid,IPC_RMID,(struct shmid_ds *) 0) == -1) { 
		perror("shmctl (master)");
		exit(-1);
	}

} 



shmem_sdettch()	
{
	if (shmdt((char *) comarea) == -1) {
		perror("shmdt (slave)");
		exit(-1);
	}
}

rtloop()
{
     	sigset(SIGINT, intr_exit);

	/* release the signal */ 
     	sigrelse(SIGALRM);

     	while (comarea->nintr < sec*(SECOND/interval)) {}
	/* display results so far */
	display_result();

	/* do some cleaning */
	self_exit();
}

/*
** parent reacts to sigcld
*/
void
child_pexit()
{
	printf("Child exited!\n");
	intr_exit();
}

/*
** child react to sigkill 
*/
void
child_exit()
{	
	shmem_sdettch();
	exit(0);
}

void
intr_exit()
{
	clean_up();
}

self_exit()
{
	kill(comarea->cpid, SIGKILL);
	clean_up();
}

clean_up()
{
	kill(comarea->cpid, SIGKILL);
	if (comarea->spid != -1)
		kill(comarea->spid, SIGKILL);

	if (c_cpu != -1) {
		if (isolate) {
			if (sysmp(MP_UNISOLATE, c_cpu) == -1) 
				printf("Failed UNISOLATE\n");
		}
		else if (sysmp(MP_EMPOWER, c_cpu) == -1) 
			printf("Failed EMPOWER\n");
	}
	sighold(SIGALRM);
	/* shmem unattache XXX */
	shmem_dettch();
	close(rtefd);
	if (isolate)
		sysmp(MP_UNISOLATE, c_cpu);
	else
		sysmp(MP_EMPOWER, c_cpu);
	if (clock_disable)
		sysmp(MP_PREEMPTIVE, c_cpu);
	exit(0);
}

display_result()
{
	unsigned average, max, maxi, min, mini;
	float var;

	compute_result(displat, &average, &var, &max, &maxi, &min, &mini);
	printf("\nInterrupt to Dispatch Latency\n");
	printf("Slaves were awaken %d times\n",comarea->nwakeup);
	printf("Average=%d var=%f samples size=%d\n",average, var, 
		comarea->nintr);
	printf("Max latency = %d, maxi = %d\n",max, maxi);
	printf("Min latency = %d, mini = %d\n",min, mini);

	compute_result(disp2lat, &average, &var, &max, &maxi, &min, &mini);
	printf("\nInterrupt to Dispatch Latency II\n");
	printf("Slaves were awaken %d times\n",comarea->nwakeup);
	printf("Average=%d var=%f samples size=%d\n",average, var, 
		comarea->nintr);
	printf("Max latency = %d, maxi = %d\n",max, maxi);
	printf("Min latency = %d, mini = %d\n",min, mini);

	compute_result(intrlat, &average, &var, &max, &maxi, &min, &mini);
	printf("\nInterrupt Latency\n");
	printf("Average=%d var=%f samples size=%d\n",average, var, 
		comarea->nintr);
	printf("Max latency = %d, maxi = %d\n",max, maxi);
	printf("Min latency = %d, mini = %d\n",min, mini);
	
	compute_result(intrsrv, &average, &var, &max, &maxi, &min, &mini);
	printf("\nInterrupt Service\n");
	printf("Average=%d var=%f samples size=%d\n",average, var, 
		comarea->nintr);
	printf("Max latency = %d, maxi = %d\n",max, maxi);
	printf("Min latency = %d, mini = %d\n",min, mini);

	printf("Isolated processor vfaults = %d(%d,%d), pfaults = %d(%d,%d)\n", 
		comarea->rtdat.stat.iso_vfault - rtestat.iso_vfault,
		comarea->rtdat.stat.iso_vfault,
		rtestat.iso_vfault,
		comarea->rtdat.stat.iso_pfault - rtestat.iso_pfault,
		comarea->rtdat.stat.iso_pfault,
		rtestat.iso_pfault);
       printf("Isolated processor tlbsync = %d(%d,%d), tlbclean = %d(%dm,%d)\n",
		comarea->rtdat.stat.iso_tlbsync - rtestat.iso_tlbsync,
		comarea->rtdat.stat.iso_tlbsync,
		rtestat.iso_tlbsync,
		comarea->rtdat.stat.iso_tlbclean - rtestat.iso_tlbclean,
		comarea->rtdat.stat.iso_tlbclean,
		rtestat.iso_tlbclean);
	printf("Isolated processor delayed");
	printf(" tlbflush = %d(%d,%d), iflush = %d(%d,%d)\n",
	      comarea->rtdat.stat.iso_delaytlbflush - rtestat.iso_delaytlbflush,
	      comarea->rtdat.stat.iso_delaytlbflush,
	      rtestat.iso_delaytlbflush,
	      comarea->rtdat.stat.iso_delayiflush - rtestat.iso_delayiflush,
	      comarea->rtdat.stat.iso_delayiflush,
	      rtestat.iso_delayiflush);
}

compute_result(latency, average, diffxs, max, maxi, min, mini)
unsigned *latency, *average, *max, *min, *maxi, *mini;
float *diffxs;
{
	unsigned	c;
	unsigned 	sum;
	float		tmp;

	*max = 0;
	*min = latency[SKIP];
	sum = 0;
	for (c=SKIP; c<comarea->nintr; c++) {
		if (*max < latency[c]) {
			*max = latency[c];
			*maxi = c;
		}
		if (*min > latency[c]) {
			*min = latency[c];
			*mini = c;
		}
		sum += latency[c];
	}

	/* Compute mean value for intervals */
	*average = sum / comarea->nintr;
	/* Compute variance on intervals */
	*diffxs = 0.0;
	for (c=SKIP; c<comarea->nintr; c++) {
		tmp = (float)(latency[c] - *average);
		*diffxs = *diffxs + (tmp*tmp);
	}
	*diffxs = fsqrt(*diffxs)/(comarea->nintr - 1);
}

/*
** sproc code
*/
void
sproc_entry(void *s_cpu)
{
	int sp;
	char *startaddr;
	volatile char *memaddr;
	int fd;
	int i;
	char	membuf[1024];
#define	LEN	(1024*1024)
	/*
	 * Grow the stack now!
	 */
	sp = *(int *)((__psint_t)&sp - 4096);
	/*
	 * Run around touching memory and things while the rt activity is
	 * going on
	 */
	/* must run on c_cpu */
	if (sysmp(MP_MUSTRUN, (int)s_cpu) == -1) {
		fprintf(stderr,"Failed MUSTRUN on %d\n",(int)s_cpu);
		exit(0);
	}
	/*
	 * Create a file for mapping purposes
	 */
	if ((fd = open("/usr/tmp/sproc", O_RDWR|O_CREAT, 0777)) < 0) {
		fprintf(stderr,"Failed creation of tmp file\n");
		exit(0);
	}
#ifdef NOTYET
	memaddr = (char *)malloc(1024);
	if (memaddr == NULL) {
		fprintf(stderr,"Failed malloc\n");
		exit(0);
	}
#else
	memaddr = &membuf[0];
#endif
	for (i = 0; i < LEN/1024; i++)
		write(fd, memaddr, 1024);
#ifdef NOTYET
	free(memaddr);
#endif
	close(fd);
	while (1) {
		/*
		 * do some private mapping operations
		 */
		if ((fd = open("/usr/tmp/sproc", O_RDWR)) < 0) {
			fprintf(stderr, "open failed\n");
			exit(0);
		}
		startaddr = mmap(0, LEN, PROT_READ|PROT_WRITE,
				MAP_LOCAL|MAP_PRIVATE, fd, 0);
		for (memaddr = startaddr; memaddr < startaddr + LEN; memaddr++)
			*memaddr = 1;
		munmap(startaddr, LEN);
		close(fd);
	}
			
}

/*
** text to be locked 
*/
void	
rtproc()
{
	/* set up shared memory */
	shmem_sattch();
	/* lock memory */
	pinmem();

	if (s_cpu != -1) {
		comarea->spid = sproc(sproc_entry, PR_SADDR, (void *)s_cpu, 0);
		/* sleep(1); */
	} else
		comarea->spid = -1;

	/* must run on c_cpu */
	if (sysmp(MP_MUSTRUN, c_cpu) == -1) {
		fprintf(stderr,"Failed MUSTRUN on %d\n",c_cpu);
		exit(0);
	}
	if (isolate) {
		if (sysmp(MP_ISOLATE, c_cpu) == -1) {
			fprintf(stderr,"Failed ISOLATE on %d\n",c_cpu);
			exit(0);
		}
	}
	else if (sysmp(MP_RESTRICT, c_cpu) == -1) {
		fprintf(stderr,"Failed RESTRICT on %d\n",c_cpu);
		exit(0);
	}
	if (schedctl(NDPRI, 0, NDPHIMAX) == -1) {
		fprintf(stderr,"Failed NDPRI\n");
	}
	if (clock_disable) {
		if (sysmp(MP_NONPREEMPTIVE, c_cpu) == -1) {
			fprintf(stderr,"Failed CLOCK_OFF on %d\n",c_cpu);
			exit(0);
		}
	}

	/* does RTSET */
	if (ioctl(rtefd, RTSET, comarea->cpid) < 0) {
		perror("can't RTSET");;
		exit (1);
	}

	/* flow control */
loop:
	if (ioctl(rtefd, RTSLEEP, 30+1) < 0) {
		perror("can't RTSLEEP");
		exit(1);
	}
	/* got woken up */
	comarea->dispatch = *iotimer_addr;
	comarea->nwakeup++;

	goto loop;
}

/*
** parent signal handler
*/
void
catcher()
{
	struct itimerval	itv;
	static unsigned	curmax = 0;
	static int curmaxi= -20;
	static unsigned	curmin = -1;
	static int curmini;
	static unsigned maxsend, maxdispatch, maxrcv, maxrtrn;
	int rv;

	/* first itimer signal */
	if (curmaxi < 0) {	
		/* skip the first few */
		curmaxi++;
		/* wait for child to sleep */
		while ((rv = ioctl(rtefd, RTGET, &comarea->rtdat)) >= 0) {
			if (comarea->slept && 
					comarea->pid == comarea->cpid) 
				break;
		}
		if (rv < 0) {
			perror("can't RTGET");
			exit(1);
		}
		comarea->dispatch = 0;
		comarea->intr_snd = 0;

		rtestat = comarea->rtdat.stat;
	}
	else {
		/* wait for child to sleep */
		while ((rv = ioctl(rtefd, RTGET, &comarea->rtdat)) >= 0) {
			if (comarea->slept && 
					comarea->pid == comarea->cpid) 
				break;
		}
		if (rv < 0) {
			perror("can't RTGET");
			exit(1);
		}
		compute_latency(comarea->intr_rtrn, comarea->intr_rcv, intrsrv);
		compute_latency(comarea->intr_rcv, comarea->intr_snd, intrlat);
		compute_latency(comarea->dispatch, comarea->intr_snd, displat);
		compute_latency(comarea->dispatch, comarea->intr_send2, disp2lat);
		if (displat[comarea->nintr] > curmax) {
			curmax = displat[comarea->nintr];
			curmaxi = comarea->nintr;
			maxsend = comarea->intr_snd;
			maxrcv = comarea->intr_rcv;
			maxrtrn = comarea->intr_rtrn;
			maxdispatch = comarea->dispatch;
		}
		if (displat[comarea->nintr] < curmin) {
			curmin = displat[comarea->nintr];
			curmini = comarea->nintr;
		}
    		comarea->nintr++;
		comarea->dispatch = 0;
		comarea->intr_snd = 0;
		comarea->intr_rcv = 0;
		comarea->intr_rtrn = 0;

		if (verbose && (comarea->nintr % 200) == 0) 
			printf("i=%d max=%d(us) maxi=%d sent=%x, dispatch=%x\r",comarea->nintr, curmax, curmaxi, maxsend, maxdispatch);
	}
	/* wake-up child */
	comarea->intr_send2 = *iotimer_addr;
	if (ioctl(rtefd, RTWAKEUP, comarea->cpid) < 0) {
		perror("can't RTWAKEUP");
		exit(1);
	}

	if (comarea->nintr >= sec*(SECOND/interval)) {
     		itv.it_interval.tv_sec = 0;
     		itv.it_interval.tv_usec = 0;
     		itv.it_value = itv.it_interval;
     		setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);
	}
	comarea->slept = 0;
}

compute_latency(end, begin, buf)
unsigned end, begin, *buf;
{
	float diff;

	if (end > begin) {
		diff = (float)(end - begin);
	}
	else {
		diff = (float)(((unsigned)0xffffffff-begin) + end);
	}
	diff = ((diff*(float)comarea->cycleval)/(float)1000000);
	buf[comarea->nintr] = (unsigned)diff;
#if DEBUG
	if (buf[comarea->nintr] == 0 || buf[comarea->nintr] > 1000) {
		printf("latency = %d index = %d, intr_snd = %x, intr_rcv = %x, intr_rtrn = %x, dispatch = %x\n",buf[comarea->nintr], comarea->nintr, comarea->intr_snd, comarea->intr_rcv, comarea->intr_rtrn, comarea->dispatch);
	}
#endif
}

/*
** code to lock critical memory in core lock 
** stack, signal handler and the communication area
** lock all text between rtproc() and pinmem()
*/
pinmem()
{
	int sp;

	
	if (plock(PROCLOCK) == -1) {
		perror("disp: failed plock(PROCLOCK)");
		exit(-1);
	}
    	/* stack grows downward */
	sp = *(int *)((__psint_t)&sp - 4096);

	if (s_cpu != -1) {
		if ((sproc_stk = (caddr_t)malloc(STKSIZE)) == (caddr_t)0) {
			fprintf(stderr,"can't malloc\n");
			exit(-1);
		}
		sproc_stk += STKSIZE;
	}
	/* also lock the shared memory region */
	mpin((char *) comarea, SHMSIZE);

}

	
