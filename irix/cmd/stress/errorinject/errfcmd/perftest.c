#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/fcntl.h>
#include <fs/procfs/procfs.h>
#include <syslog.h>
#include <sys/sysmp.h>
#include <sys/resource.h>

int gen, fd, thresh;

#define ALLOC_SIZE	0x10000
#define ALLOC_TABLE_SIZE 256
char *alloc_table[ALLOC_TABLE_SIZE];


void
cache_err_die(char *x)
{
	syslog(LOG_ALERT, x);
	exit(-1);
}



do_accesses(int cpu)
{
	char *tmp;
	int i, j;
	int old_cnt, bad_cnt;
	hwperf_cntr_t cnts;
	
	if (sysmp(MP_MUSTRUN, cpu) == -1)  {
		printf("could not run on cpu %d\n", cpu);
		return -1;
	}

	if (ioctl(fd, PIOCGETEVCTRS, (void *)&cnts) != gen)
	    return -1;

	old_cnt = cnts.hwp_evctr[0];

	for (i = 0; i < ALLOC_TABLE_SIZE; i++) {
		if (alloc_table[i] == NULL) break;
		for (j = 0; j < ALLOC_SIZE; j += 128) 
		    *(volatile char *)(alloc_table[i] + j);
	}

	if (ioctl(fd, PIOCGETEVCTRS, (void *)&cnts) != gen)
	    return -1;

	bad_cnt = cnts.hwp_evctr[0] - old_cnt;
	if (bad_cnt > thresh) {
		syslog(LOG_ALERT, 
		       "CPU %d: %d scache single bit errors\n",
		       cpu, cnts.hwp_evctr[0] - old_cnt);
		if (bad_cnt > (thresh * 4))
		    syslog(LOG_ALERT,
			   "Excessive SBE's. Call your local cache broker");
	}
	return 0;
}

main(int argc, char *argv[])
{
	char file[32];
	hwperf_profevctrarg_t hwprof;
	hwperf_eventctrl_t *hwevent;	
	int i;
	int error;
	int ev0, ev1, pid, mode;
	int x = 2;

	int size = 0x400000;
	int numprocs;

	if (fork() != 0) goto last;


	if (argc > 1) 
	    ev0 = strtol(argv[1], NULL, 0);
	else 
	    ev0 = 0;

	if (argc > 2)
	    ev1 = strtol(argv[2], NULL, 0);
	else
	    ev1 = 0;

	if (argc > 3)
	    mode = strtol(argv[3], NULL, 0);
	else
	    mode = HWPERF_CNTEN_U;

	if (argc > 4)
	    pid = strtol(argv[4], NULL, 0);
	else
	    pid = getpid();

	for (i = 0; i <= size / ALLOC_SIZE; i++) {
		if (i >= ALLOC_TABLE_SIZE) break;
		if ((alloc_table[i] = (char *)malloc(ALLOC_SIZE)) == NULL)
		    cache_err_die("Unable to allocate memory for test");
	}

	openlog(argv[0], LOG_PID|LOG_CONS|LOG_PERROR|LOG_NDELAY, LOG_USER);
	syslog(LOG_INFO, "started. Cache size %x. Threshold %d", 
	       size, thresh);
	
	numprocs = sysmp(MP_NPROCS);
	if (numprocs == -1) 
		cache_err_die("sysmp(MP_NPROCS) = -1!");

	if (setpriority(PRIO_PROCESS, getpid(), 20) < 0)
	    perror("setprio");

	printf("%s Running scache tests on size %d (%x)\n", argv[0], size, size);
	hwevent = &hwprof.hwp_evctrargs;

	bzero(&hwprof, sizeof(hwperf_profevctrarg_t));

	sprintf(file, "/proc/%d", pid);
	fd = open(file, O_RDWR);

	hwevent->hwp_evctrl[0].hwperf_creg.hwp_ev = ev0;
	hwevent->hwp_evctrl[0].hwperf_creg.hwp_mode = mode;
	hwevent->hwp_evctrl[HWPERF_CNT1BASE].hwperf_creg.hwp_ev = ev1;
	hwevent->hwp_evctrl[HWPERF_CNT1BASE].hwperf_creg.hwp_mode = mode;

	error = 1;
	while (x--) {
		int cpu;
		int num_err;

		num_err = 0;
		while (error) {
			if ((++num_err % 12) == 0) 
				syslog(LOG_ALERT,
				       "Unable to get event counters %d iterations", num_err);
			if ((gen = ioctl(fd, 
					 PIOCENEVCTRS, (void *)&hwprof)) < 0) {
				perror("ioctl");
				printf("Unable to get event counters\n");
				sleep(300);
				continue;
			}
			error = 0;
			break;
		}
		for (cpu = 0; cpu < numprocs; cpu++) {
			if (do_accesses(cpu)) {
				perror("ioctl");
				ioctl(fd, PIOCRELEVCTRS);
				error = 1;
				break;
			}
		}
		sleep (1);
		exit(1);
	}
	

last:
	exit(0);

}

