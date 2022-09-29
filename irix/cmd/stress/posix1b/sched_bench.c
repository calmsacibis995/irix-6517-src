/*
 * File: sched_bench.c
 *
 * Function: sched_bench is a benchmark for measuring user-level
 *           context switch performance for processes using posix.1b
 *           scheduling facilities.
 *
 * Arguments:
 *           -l #  Specify the number of context switch loops
 *           -b    Run cpu balance test
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <task.h>
#include <sys/sysmp.h>
#include <sys/syssgi.h>

#define PRIORITY 198

#define CPU_MAX 128
#define PRDA_INIT 176

int loops = 1000000;
int balance_test = 0;

void child(void);
int print_cpu_count(int *);

main(int argc, char *argv[])
{
	int cpus, procs;
	int parse;
	int ctx_switches;
	int proc_override = 0;
	int yield_loops;
	struct sched_param sparams;
	float total_time;
	struct timespec start_stamp, end_stamp;
	int cpu_counts[CPU_MAX];

	while ((parse = getopt(argc, argv, "l:p:b")) != EOF)
		switch (parse) {
		case 'l':
			loops = atoi(optarg);
			break;
		case 'b':
			balance_test = 1;
			break;
		case 'p':
			proc_override = atoi(optarg);
			break;
		default:
			exit(1);
		}

	cpus = sysconf(_SC_NPROC_ONLN);

	if (proc_override)
		procs = proc_override;
	else
		procs = cpus+1;

	ctx_switches = procs * loops;
	yield_loops = loops;

	printf("Performing %d context switches between %d procs on %d CPUs\n",
	       ctx_switches, procs, cpus);

	/*
	 * set priority above system daemons, but below interrupt threads.
	 */	
	sparams.sched_priority = PRIORITY;
	if (sched_setscheduler(0, SCHED_FIFO, &sparams) < 0) {
		if (errno == EPERM)
			printf("You must be root to run this benchmark\n");
		else
	  		perror("sched_bench: ERROR - sched_setscheduler");
		exit(1);
	}

	if (balance_test) {
		if (syssgi(PRDA_INIT, USER_LEVEL) < 0) {
			perror("syssgi");
			exit(1);
		}
	}

	while(--procs)
		if (sproc(child, PR_SALL) == -1) {
			perror("sproc");
			exit(1);
		}

	if ((clock_gettime(CLOCK_REALTIME, &start_stamp)) == -1) {
		perror("sched_bench: ERROR - clock_gettime");
		exit(1);
	}

	if (balance_test) {
		while (yield_loops--) {
			sched_yield();
			cpu_counts[get_cpu()]++;
		}
	} else {
		while (yield_loops--)
			sched_yield();
	}

	if ((clock_gettime(CLOCK_REALTIME, &end_stamp)) == -1) {
		perror("sched_bench: ERROR - clock_gettime 2");
		exit(1);
	}  

	end_stamp.tv_sec -= start_stamp.tv_sec;
	if (end_stamp.tv_nsec >= start_stamp.tv_nsec)
		end_stamp.tv_nsec -= start_stamp.tv_nsec;
	else {
		if (end_stamp.tv_sec > 0)
			end_stamp.tv_sec--;
		end_stamp.tv_nsec = (end_stamp.tv_nsec - start_stamp.tv_nsec)
		  + 1000000000;
	}

	total_time = end_stamp.tv_sec * 1000000 + end_stamp.tv_nsec/1000;

	if (balance_test)
		print_cpu_count(cpu_counts);

	printf("%d context switches took %d.%d seconds\n",
	       ctx_switches, end_stamp.tv_sec, end_stamp.tv_nsec/1000);

	printf("%5.2f uS per context switch\n", total_time/ctx_switches);

	exit(0);
}

void
child(void)
{
	int yield_loops = loops;
	int cpu_counts[CPU_MAX];

	if (balance_test) {

		if (syssgi(PRDA_INIT, USER_LEVEL) < 0) {
			perror("syssgi");
			exit(1);
		}

		while (yield_loops--) {
			sched_yield();
			cpu_counts[get_cpu()]++;
		}

		print_cpu_count(cpu_counts);
	} else {
		while (yield_loops--)
			sched_yield();
	}

	sched_yield();

	exit(0);
}

int
print_cpu_count(int *cpu_counts)
{
	int i;
	int len;
	char buffer[4*1024];
	int pid = getpid();

	len = sprintf(buffer, "pid=%-6d:", pid);

	for (i=0; i< CPU_MAX; i++) {
		if (cpu_counts[i])
			len += sprintf(buffer + len,
				       " cpu%-3d=%-9d", i, cpu_counts[i]);
	}
	len += sprintf(buffer + len, "\n");

	write(1, buffer, len);
}
