
int gen, fd, thresh;

/* The simple "panic function" It prints a message to a system log and
then does the exit stage left */

void
cache_err_die(char *x)
{
  syslog(LOG_ALERT, x);
  exit(-1);
}


do_accesses(int cpu, int size)
{
	char *tmp;
	int i, j;
	int old_cnt;
	int gen1;
	hwperf_cntr_t cnts;

	/* dedicates the process to run on only one CPU */
	if (sysmp(MP_MUSTRUN, cpu) == -1)  {
	  printf("could not run on cpu %d\n", cpu);
	  return -1;
	}
	/* Retrieve the cache counters */
	gen1 = ioctl(fd, PIOCGETEVCTRS, (void *)&cnts);
	if (gen1 != gen)
	    cache_err_die("terminated due to counter generation error");

	
	old_cnt = cnts.hwp_evctr[0];

	/* Now access the memory */
	for (i = 0; i < (size * 4); i += 4096) {
	  tmp = (char *)malloc(4096);
	  for (j = 0; j < 4096; j+= 128) {
	    *(volatile char *)(tmp + j);
	  }
	  free(tmp);
	}

	/* get the counts again */
	gen1 = ioctl(fd, PIOCGETEVCTRS, (void *)&cnts);
	if (gen1 != gen)
	    cache_err_die("terminated due to counter generation error");

	/* Print the difference if it is over the global threshold */
	if ((cnts.hwp_evctr[0] - old_cnt) > thresh) {
		syslog(LOG_ALERT, 
		       "CPU %d: %d SCACHE SBE's during scache test",
		       cpu, cnts.hwp_evctr[0] - old_cnt);
		old_cnt = cnts.hwp_evctr[0];
	}
	return 0;
}

main(int argc, char *argv[])
{
	char file[32];
	hwperf_profevctrarg_t hwprof;
	hwperf_eventctrl_t *hwevent;	

	int size = 0x400000;
	int numprocs;

	if (argc > 1) 
	    size = strtol(argv[1], NULL, 0);

	if (argc > 2)
	    thresh = strtol(argv[2], NULL, 0);
	
	/* Sets up the log for later */
	openlog(argv[0], LOG_PID|LOG_CONS|LOG_PERROR|LOG_NDELAY, LOG_USER);
	
	/* How may CPU's do we have? */
	numprocs = sysmp(MP_NPROCS);
	if (numprocs == -1) 
	  cache_err_die("sysmp(MP_NPROCS) = -1!");

	/* Give this process really low priority */
	if (setpriority(PRIO_PROCESS, getpid(), 20) < 0)
	  perror("setprio");

	printf("Running scache tests on size %d (%x)\n", size, size);
	hwevent = &hwprof.hwp_evctrargs;

	bzero(&hwprof, sizeof(hwperf_profevctrarg_t));

	sprintf(file, "/proc/%d", getpid());
	fd = open(file, O_RDWR);

	hwevent->hwp_evctrl[0].hwperf_creg.hwp_ev = HWPERF_C0PRFCNT0_SCDAECC;
	hwevent->hwp_evctrl[0].hwperf_creg.hwp_mode = HWPERF_CNTEN_U;
	hwevent->hwp_evctrl[HWPERF_CNT1BASE].hwperf_creg.hwp_ev = 
	  HWPERF_C0PRFCNT1_SDCMISS;
	hwevent->hwp_evctrl[HWPERF_CNT1BASE].hwperf_creg.hwp_mode = 
	  HWPERF_CNTEN_U;

	gen = ioctl(fd, PIOCENEVCTRS, (void *)&hwprof);
	if (gen < 0) 
	    cache_err_die("Unable to enable event counting\n");

	while (1) {
		int cpu;
		for (cpu = 0; cpu < numprocs; cpu++)
		    do_accesses(cpu, size);
		sleep (300);
	}
}

