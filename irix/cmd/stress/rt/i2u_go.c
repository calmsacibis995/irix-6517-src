/*
 * i2u_go:
 *		This program is used to collect interrupt to
 *		user-level execution latency samples.
 *
 * Parameters:
 *		-s # : number of samples to collect
 *		-p   : print the samples to stdout as they are collected
 *		-t # : set spike threshold
 *		-l   : record a sample log to /usr/tmp/i2u_data
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/syssgi.h>
#include <sys/mman.h>
#include <sys/sysmp.h>
#include <sched.h>

#define I2U_INTERRUPT	1
#define I2U_BLOCK	2
#define I2U_DEBUG	3

#define GET_TIME()  *clockp

#define DATAFILE "/usr/tmp/i2u_data"

int p = 0;
int samples = 10;
int update_average = 10;
int spike_threshold = 0;
int log = 0;

int dev_id;
int data_fd;
int *data_array;

int time_high = 0;
int time_low = 99999;
int time_mean = 0;
int time_mean_tmp = 0;

u_int spikes = 0;
int loops;

unsigned long long *clockp = NULL;
unsigned int cycleval;

void child (void);
void map_cycle_counter(void);
void cleanup(void);

main(int argc, char **argv)
{
	int parse;
	struct timespec sleep_time;
	struct sched_param param;
	
	while ((parse = getopt(argc, argv, "s:pt:l")) != EOF)
		switch (parse) {
		case 's':
		  	samples = atoi(optarg);
			break;
		case 'p':
			p = 1;
			break;
		case 't':
			spike_threshold = atoi(optarg);
			break;
		case 'l':
			log = 1;
			break;
		default:
			exit(1);
		}

	/*
	 * Map in cycle counter
	 */
	map_cycle_counter();
	

	if (log) {
		/*
		 * Open and map in data file
		 */
	  	if ((data_fd = open(DATAFILE, O_RDWR|O_CREAT, 666)) < 0) {
			perror("open data file");
			exit(1);
		}
    
		data_array = (int *) mmap(0,
					  samples * sizeof(int),
					  PROT_READ | PROT_WRITE,
					  MAP_SHARED,
					  data_fd,
					  0);

		if ((int) data_array < 0) {
			perror("mmap data file");
			exit(1);
		}
    
		if (ftruncate(data_fd, samples * sizeof(int)) < 0) {
			perror("ftruncate");
			exit(1);
		}

		if (mlockall(MCL_CURRENT) < 0) {
			perror("mlockall");
			exit(1);
		}
	}

	/*
	 * Open i2u latency measurement device
	 */
	dev_id = open("/hw/i2u/1", O_RDONLY);
	if (dev_id == -1) {
		perror("open");
		exit(1);
	}

	if (sysmp(MP_ISOLATE, 1) < 0) {
		perror("sysmp MP_ISOLATE");
		exit(1);
	}


	if (sysmp(MP_NONPREEMPTIVE, 1) < 0) {
		perror("sysmp MP_NONPREEMPTIVE");
		exit(1);
	}

	/*
	 * Spawn child
	 */
	if (sproc(child, PR_SALL) == -1) {
		perror("sproc failed");
		exit(1);
	}

	if ((int) signal(SIGINT, cleanup) == -1) {
		perror("signal");
		exit(1);
	}

	param.sched_priority = sched_get_priority_min(SCHED_RR);
	if (sched_setscheduler(0, SCHED_RR, &param) < 0) {
		perror("sched_setscheduler");
		exit(1);
	}

	printf("i2u_go: sampling %d interrupts\n", samples);

	if (spike_threshold)
		printf("i2u_go: seeking latencies over %d us\n",
		       spike_threshold);

	if (log)
		printf("i2u_go: recording log to %s\n", DATAFILE);
	else
		printf("i2u_go: logging is disabled\n");
	  
	sleep_time.tv_sec = 0;

	if (p)
		sleep_time.tv_nsec = 300000; /* 300 mics */
	else
		sleep_time.tv_nsec = 600000; /* 600 mics */

	nanosleep(&sleep_time, NULL);

	/*
	 * Interrupt loop
	 */
	for (loops=0; loops<samples; loops++) {

		nanosleep(&sleep_time, NULL);

		/*
		 * periodically print some status
		 */
		if (p && (loops%10000) == 0) {
		  if (loops)
		    printf("i2u_go: SAMPLES: %d ... Hi: %d Lo: %d Av: %d\n",
			   loops, time_high, time_low, time_mean);
		}

		/*
		 * Trigger interrupt
		 */
		while (ioctl(dev_id, I2U_INTERRUPT) == -1)
			if (errno != EAGAIN) {
				perror("ioctl: parent");
				exit(1);
			}
	}

	cleanup();
}

void
child(void)
{
 	u_int time_start = 0;
	u_int time_end = 0;
	u_int latency = 0;
	int x;

	struct sched_param param;

	if (sysmp(MP_MUSTRUN, 1) < 0) {
		perror("sysmp MP_MUSTRUN");
		exit(1);
	}

	if (mlockall(MCL_CURRENT) < 0) {
		perror("mlockall");
		exit(1);
	}

	param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if (sched_setscheduler(0, SCHED_FIFO, &param) < 0) {
		perror("sched_setscheduler");
		exit(1);
	}

	for (x=0; x < samples; x++) {
		/*
		 * Wait for interrupt
		 */
		time_start = ioctl(dev_id, I2U_BLOCK);
		time_end = GET_TIME();

		if (time_start == -1 && errno == EINTR) {
			/*
			 * Bad data point.
			 * System call was interrupted.
			 * Retry.
			 */
			x--;
			continue;
		}

		latency = ((time_end - time_start) * cycleval) / 1000;

		if (latency < time_low)
			time_low = latency;
		if (latency > time_high)
			time_high = latency;
		time_mean += latency;

		if (time_mean != latency)
			time_mean = time_mean/2;

		if (spike_threshold) {
			/*
			 * Drop into the kernel debugger if the
			 * interrupt latency exceeds the spike threshold
			 */
		 	if (latency > spike_threshold) {
				if (ioctl(dev_id, I2U_DEBUG) < 0) {
					perror("ioctl: child I2U_DEBUG");
					exit(1);
				}
			}
		}

		if (log)
			data_array[x] = latency;
	}
}

void
map_cycle_counter(void)
{
	ptrdiff_t phys_addr, raddr;
	int poffmask;
	int fd;

	poffmask = getpagesize() - 1;
	phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
	/* Change from picoseconds to nanoseconds */
	cycleval = cycleval/1000;

	raddr = phys_addr & ~poffmask;

	fd = open("/dev/mmem", O_RDONLY);
	if (fd == -1) {
		perror("open dev/mem");
		exit(1);
	}

	clockp = (unsigned long long *) mmap(0,
					     poffmask,
					     PROT_READ,
					     MAP_PRIVATE,
					     fd,
					     (__psint_t)raddr);
    
	if ((int) clockp == -1) {
		perror("mmap clock");
		exit(1);
	}

	clockp = (unsigned long long *)
	  ((__psunsigned_t) clockp + (phys_addr & poffmask));

	close(fd);
}

void
cleanup(void)
{
	printf("\ni2u_go: SAMPLES: %d ... Hi: %d Lo: %d Av: %d\n",
	       loops, time_high, time_low, time_mean);

	close(data_fd);
	close(dev_id);

	(void) sysmp(MP_PREEMPTIVE, 1);
	(void) sysmp(MP_UNISOLATE, 1);

	munlockall();

	exit(0);
}
