/*
 * File: sem_sbench.c
 *
 * Function: sem_sbench is a benchmark for measuring the average
 *           worst case performance (100% contention) of arena
 *           semaphores, Posix.1b unnamed semaphores, and Posix.1b
 *           named semaphores shared between SPROCS.
 *
 * Arguments:
 *           -u    Posix.1b Unnamed semaphore test
 *           -n    Posix.1b Named semaphore test
 *           -a    Irix arena semaphores (usemas)
 *           -c #  Specify the number of producer/consumer cycles 
 *
 * Default:
 *           Measure Posix.1b unnamed semaphores using 1000000 cycles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <time.h>
#include <sched.h>
#include <ulocks.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define MAX_PARALLELISM 200

/*
 * define priorities to be above system daemons, but
 * below interrupt threads.
 */
#define PRIORITY_HI 198
#define PRIORITY_LO 197

typedef struct sem_pair_s {
	sem_t sema1;
	char  x1[600];
	sem_t sema2;
	char  x2[1000];
} sem_pair_t;

sem_pair_t sems[MAX_PARALLELISM];

int realtime = 1;
int global_count = 25000;
int parallelism = 64;
int flood = 0;

void producer(int);
void consumer(int);

main(int argc, char *argv[])
{
	int parse, x;
	struct  sched_param param;
	struct timespec start_stamp, end_stamp;

	while ((parse = getopt(argc, argv, "c:p:rf")) != EOF)
		switch (parse) {
		case 'c':
			global_count = atoi(optarg);
			break;
		case 'r':
			realtime = 0;
			break;
		case 'p':
			parallelism = atoi(optarg);
			if (parallelism > MAX_PARALLELISM) {
				printf("parallelism set to large\n");
				exit(1);
			}
			break;
		case 'f':
			flood = 1;
			break;
		}

	if (realtime) {
		param.sched_priority = PRIORITY_LO;
		if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
			if (errno == EPERM)
			   printf("You must be root to run this benchmark\n");
			else
				perror("sched_setscheduler");              
			exit(0);                                           
		}
	}
	
	if (mlockall(MCL_CURRENT|MCL_FUTURE)) {
		perror("mlockall");
		exit(1);
	}

	if (usconfig(CONF_INITUSERS, parallelism*2+1) < 0) {
		perror("usconfig");
		exit(1);
	}

	/*
	 * Init semas
	 */

	for (x=0; x<MAX_PARALLELISM; x++) {
		if (sem_init(&sems[x].sema1, 0, 0)) {
			perror("sem_init 1");
			exit(1);
		}

		if (sem_init(&sems[x].sema2, 0, 0)) {
			perror("sem_init 2");
			exit(1);
		}
	}

	for (x=0; x < parallelism; x++) {
		/* launch producer */
		if (sprocsp(producer, PR_SALL,(void *)x, NULL, 8192) < 0) {
			perror("sproc 1");
			exit(1);
		}

		/* launch consumer */
		if (sprocsp(consumer, PR_SALL,(void*)x, NULL, 8192) < 0) {
			perror("sproc 2");
			exit(1);
		}
	}

	if (realtime) {
		param.sched_priority = PRIORITY_HI;
		if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
			if (errno == EPERM)
			   printf("You must be root to run this benchmark\n");
			else
				perror("sched_setscheduler 2");              
			exit(0);                                           
		}
	}

	if ((clock_gettime(CLOCK_REALTIME, &start_stamp)) == -1) {
		perror("clock_gettime");
		exit(1);
	}

	printf("sem_par: Starting %d producer/consumer pairs...\n",
	       parallelism);

	if ((clock_gettime(CLOCK_REALTIME, &start_stamp)) == -1) {
		perror("clock_gettime");
		exit(1);
	}

	/*
	 * Start test
	 */
	for (x=0; x<parallelism; x++) {
		if (sem_post(&sems[x].sema1)) {
			perror("sem_post startup");
			exit(1);
		}
	}

	/*
	 * Wait for children to complete
	 */
	while (wait(0) >= 0 || errno == EINTR)
		;
	
	if ((clock_gettime(CLOCK_REALTIME, &end_stamp)) == -1) {
		perror("clock_gettime");
		exit(1);
	}  

	end_stamp.tv_sec -= start_stamp.tv_sec;
	if (end_stamp.tv_nsec >= start_stamp.tv_nsec) {
		end_stamp.tv_nsec -= start_stamp.tv_nsec;
	} else {
		if (end_stamp.tv_sec > 0)
			end_stamp.tv_sec--;
		end_stamp.tv_nsec = (end_stamp.tv_nsec - start_stamp.tv_nsec)
			+ 1000000000;
	}

	if (flood) {
		printf("%d parallelized semaphore ops took %d.%d seconds\n",
		       global_count*parallelism,
		       end_stamp.tv_sec,
		       end_stamp.tv_nsec/1000);
	} else {
		printf("%d parallelized semaphore blocks took %d.%d seconds\n",
		       global_count*parallelism,
		       end_stamp.tv_sec,
		       end_stamp.tv_nsec/1000);
	}

	for (x=0; x<parallelism; x++) {
		sem_destroy(&sems[x].sema1);
		sem_destroy(&sems[x].sema2);
	}
}

void
producer(int index)
{
	int loop = global_count/2;
	int x, val;

	if (flood) {
		while (loop--)
			(void) sem_getvalue(&sems[index].sema1, &val);
		exit(0);
	}

	while (loop--) {
		if (sem_wait(&sems[index].sema1) < 0) {
			perror("p: sem_wait");
			exit(1);
		}
		if (sem_post(&sems[index].sema2) < 0) {
			perror("p: sem_post");
			exit(1);
		}
	}
}

void
consumer(int index)
{
	int loop = global_count/2;
	int x, val;

	if (flood) {
		while (loop--)
			(void) sem_getvalue(&sems[index].sema2, &val);
		exit(0);
	}

	while (loop--) {
		if (sem_wait(&sems[index].sema2) < 0) {
			perror("c: sem_wait");
			exit(1);
		}
		if (sem_post(&sems[index].sema1) < 0) {
			perror("c: sem_post");
			exit(1);
		}
	}
}
