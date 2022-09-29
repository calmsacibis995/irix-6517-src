#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <bstring.h>
#include <sys/stat.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/fcntl.h>
#include <sys/procfs.h>
#include <syslog.h>
#include <sys/sysmp.h>
#include <sys/resource.h>
#include <sys/capability.h>
#include <sys/wait.h>
#include <sys/pda.h>

int gen, fd, thresh;
int verbose;
int cons_fd;
struct pda_stat *cpu_stats;


#define ALLOC_SIZE	0x10000
#define ALLOC_TABLE_SIZE 260
char *alloc_table[ALLOC_TABLE_SIZE];

char printbuf[256];

void
cache_err_die(char *x)
{
	syslog(LOG_ALERT, x);
	exit(-1);
}


void child_cached(void);

int
do_accesses(int cpu)
{
	int i, j;
	int old_cnt, bad_cnt;
	hwperf_cntr_t cnts;
	
	if (ioctl(fd, PIOCGETEVCTRS, (void *)&cnts) != gen) {
		perror("cached PIOCGETEVCTRS: ");
		return -1;
	}
	
	old_cnt = cnts.hwp_evctr[0];

	for (i = 0; i < ALLOC_TABLE_SIZE; i++) {
		if (alloc_table[i] == NULL) break;
		for (j = 0; j < ALLOC_SIZE; j += 128) 
		    *(volatile char *)(alloc_table[i] + j);
	}

	if (ioctl(fd, PIOCGETEVCTRS, (void *)&cnts) != gen) {
		perror("cached PIOCGETEVCTRS: ");		
		return -1;
	}

	bad_cnt = cnts.hwp_evctr[0] - old_cnt;
	if (bad_cnt > thresh) {
		sprintf(printbuf, 
			"CPU %d: %d scache single bit errors\n",
			cpu, (int) (cnts.hwp_evctr[0] - old_cnt));
		syslog(LOG_ALERT, printbuf);
		if (cons_fd > 0)
		    write(cons_fd, printbuf, strlen(printbuf));

		sprintf(printbuf, 
			"Excessive SBE's. Call your support engineer");

		syslog(LOG_ALERT, printbuf);
		if (cons_fd > 0)
		    write(cons_fd, printbuf, strlen(printbuf));
	}
	return 0;
}


int sleep_time = 60*60; 	/* One hour */
int size = 0x400000;
int numprocs = 0;
int always_run = 0;
int continuous = 0;

int
main(int argc, char *argv[])
{
	int c;
	pid_t pid;
	int status;
	int first;
	int priority = 10;

	thresh = 10;

	cons_fd = open("/dev/console", O_RDWR);

	while ((c = getopt(argc, argv, "fs:t:vS:c:p:C")) != -1) {
		switch (c) {
		case 's':
			size = strtol(optarg, NULL, 0);
			break;
		case 't':
			thresh = strtol(optarg, NULL, 0);
			break;
		case 'v':
			verbose++;
			break;
		case 'S':
			sleep_time = strtol(optarg, NULL, 0);
			break;
		case 'c':
			numprocs = strtol(optarg, NULL, 0);
			break;
		case 'f':
			always_run = 1;
			break;
		case 'C':
		        continuous = 1;
			break;
		case 'p':
		        priority = strtol(optarg, NULL, 0);
			break;
		default:
			exit(1);
		}
	}

	openlog(argv[0], LOG_PID|LOG_CONS|LOG_PERROR|LOG_NDELAY, LOG_USER);

	if (continuous)
		syslog(LOG_INFO,
		       "Started using %d MB buffer and threshold %d", 
		       size/(1024*1024), thresh);

	if (!numprocs) 
	    numprocs = sysmp(MP_NPROCS);

	if ((cpu_stats = 
	     (struct pda_stat *)calloc(numprocs, sizeof(struct pda_stat)))
	                                                              == NULL) 
		cache_err_die("calloc failed");

	if (numprocs == -1) 
		cache_err_die("sysmp(MP_NPROCS) = -1!");

	if (setpriority(PRIO_PROCESS, getpid(), priority) < 0)
	    perror("setprio");

	if (continuous)
		printf("Cache monitor daemon started (%d MB buffer)\n",
		       size/(1024*1024));

	first = 1;
	while (continuous || first) {

		if (sysmp(MP_STAT, cpu_stats) == -1) 
		    cache_err_die("sysmp(MP_STAT) = -1");

		switch((pid = fork())) {
		case -1:
			sleep(sleep_time);
			break;

		case 0:
			child_cached();
			break;

		default:
			waitpid(pid, &status, 0);
			if (verbose)
				printf("Done waiting for child %d\n", pid);
			if(continuous) sleep(sleep_time);
			break;
		}
		first = 0;
	}
	return (0);

}

void
child_cached(void)
{
	int i;
	hwperf_profevctrarg_t hwprof;
	hwperf_eventctrl_t *hwevent;	
	char file[32];
	int cpu;
	cap_t ocap;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	cap_value_t cap_sched_mgt = CAP_SCHED_MGT;

	for (i = 0; i <= size / ALLOC_SIZE; i++) {
		if (i >= ALLOC_TABLE_SIZE) break;
		if ((alloc_table[i] = (char *)malloc(ALLOC_SIZE)) == NULL)
		    cache_err_die("Unable to allocate memory for test");
	}

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

	ocap = cap_acquire(1, &cap_device_mgt);
	if ((gen = ioctl(fd, PIOCENEVCTRS, (void *)&hwprof)) < 0) {
		cap_surrender(ocap);
		if (verbose)				
			syslog(LOG_ALERT,"Unable to get event counters");
		exit(1);
	}
	cap_surrender(ocap);

	for (cpu = 0; cpu < numprocs; cpu++) {

		if (cpu_stats[cpu].p_flags & PDAF_ISOLATED) {
			if (!always_run) {
				if (verbose)
				    syslog(LOG_ALERT,
					   "Not running on cpu %d: Isolated",
					   cpu_stats[cpu].p_cpuid);
				continue;
			}
		}
		else if ((cpu_stats[cpu].p_flags & PDAF_ENABLED) == 0) {
			if (verbose)
			    syslog(LOG_ALERT,
				   "Not running on cpu %d: Disabled",
				   cpu_stats[cpu].p_cpuid);
			continue;
		}
		
		if (verbose)
		    printf("Now running on cpu %d\n", cpu_stats[cpu].p_cpuid);

		ocap = cap_acquire(1, &cap_sched_mgt);
		if (sysmp(MP_MUSTRUN, cpu_stats[cpu].p_cpuid) == -1) {
		    cap_surrender(ocap);
		    continue;
		}
		cap_surrender(ocap);
		if (do_accesses(cpu_stats[cpu].p_cpuid)) {
			ocap = cap_acquire(1, &cap_device_mgt);
			ioctl(fd, PIOCRELEVCTRS);
			cap_surrender(ocap);
			break;
		}
	}
	if (verbose)
	    printf("Child pid %d exiting\n", getpid());
	exit(0);
}
