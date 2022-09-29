/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Frame Scheduler
 * Example I
 */

/*
 * This example creates one simple frame scheduler.
 *
 * PARAMETERS:
 * cpu: 1
 * intr_source: FRS_INTRSOURCE_R4KTIMER
 * n_minors: 4
 * period: 600 [ms] (a long period to allow for the printf statements)
 *
 * PROCESSES:
 * Process A: Period of 600 [ms] (determined base minor frame period)
 * Process B: Period of 2400 [ms] (determined # of minor frames per major frame)
 */

#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include "sema.h"

/*
 * Some fixed real-time loop parameters
 */

#define NLOOPS 40
#define LOGLOOPS_A 150
#define LOGLOOPS_B 30000
#define JOINM_A
#define JOINM_B
#define YIELDM_A
#define YIELDM_B

/*
 * Barrier to synchronize joins, enqueuesm starts.
 */
barrier_t* all_procs_enqueued = 0;

/*
 * Error Signal handlers
 */

void
underrun_error()
{
	if ((int)signal(SIGUSR1, underrun_error) == -1) {
		perror("[underrun_error]: Error while resetting signal");
		exit(1);
	}
	fprintf(stderr, "[underrun_error], PID=%d\n", getpid());	
	exit(2);
}

void
overrun_error()
{
	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("[overrun_error]: Error while resetting signal");
		exit(1);
	}
	fprintf(stderr, "[overrun_error], PID=%d\n", getpid());	
	exit(2);
}

void
termination_signal()
{
	if ((int)signal(SIGHUP, termination_signal) == -1) {
		perror("[termination_signal]: Error while resetting signal");
		exit(1);
	}
	fprintf(stderr, "[termination_signal], PID=%d\n", getpid());	
	exit(8);
}

void
processA(frs_t* frs)
{
	int counter;
	double res;
	int i;
	int previous_minor;
	int pid = getpid();

	/*
	 * First, we join to the frame scheduler
         * after all procs have been enqueued.
	 */

        barrier(all_procs_enqueued, 3);
        
	if (frs_join(frs) < 0) {
		perror("[processA]: frs_join failed");
		exit(1);
	}
    
#ifdef JOINM_A
	fprintf(stderr, "[processA (%d)]: Joined Frame Scheduler on cpu %d\n",
		pid, frs->frs_info.cpu);
#endif

	counter = NLOOPS;
	res = 2;

	/*
	 * This is the real-time loop. The first iteration
	 * is done right after returning from the join
	 */
    
	do {
		for (i = 0; i < LOGLOOPS_A; i++) {
			res = res * log(res) - res * sqrt(res);
		}

		/*
		 * After we are done with our computations, we
		 * yield the cpu. THe yield call will not return until
		 * it's our turn to execute again.
		 */

		if ((previous_minor = frs_yield()) < 0) {
			perror("[processA]: frs_yield failed");
			exit(1);
		}

#ifdef YIELDM_A	
		fprintf(stderr, "[processA (%d)]: Return from Yield; previous_minor: %d\n",
			pid, previous_minor);
#endif	
	} while (counter--);


	fprintf(stderr, "[ProcessA (%d)]: Destroying Frame Scheduler Managing cpu %d\n",
		pid, frs->frs_info.cpu);

	if (frs_destroy(frs) < 0) {
		perror("[ProcessA]: frs_destroy failed\n");
		exit(1);
	}
    
	fprintf(stderr, "[ProcessA (%d)]: Exiting\n", pid);
	exit(0);
}
 
void
processB(frs_t* frs)
{
	int counter;
	double res;
	int i;
	int previous_minor;
	int pid = getpid();

	/*
	 * First, we join to the frame scheduler
         * after all procs have been enqueued.
	 */

        barrier(all_procs_enqueued, 3);
        
	if (frs_join(frs) < 0) {
		perror("[processB]: frs_join failed");
		exit(1);
	}
    
#ifdef JOINM_B
	fprintf(stderr, "[processB (%d)]: Joined Frame Scheduler on cpu %d\n",
		pid, frs->frs_info.cpu);
#endif

	counter = NLOOPS;
	res = 2;

	/*
	 * This is the real-time loop. The first iteration
	 * is done right after returning from the join
	 */
    
	do {
		for (i = 0; i < LOGLOOPS_B; i++) {
			res = res * log(res) - res * sqrt(res);
		}

		/*
		 * After we are done with our computations, we
		 * yield the cpu. THe yield call will not return until
		 * it's our turn to execute again.
		 */

		if ((previous_minor = frs_yield()) < 0) {
			perror("[processB]: frs_yield failed");
			exit(1);
		}

#ifdef YIELDM_B	
		fprintf(stderr, "[processB (%d)]: Return from Yield; previous_minor: %d\n",
			pid, previous_minor);
#endif	
	} while (counter--);


	fprintf(stderr, "[processB (%d)]: Exiting\n", pid);
	exit(0);
}

int
create_process(void (*f)(frs_t*), frs_t* frs)
{
	int pid;
	switch ((pid = fork())) {
	      case 0:
		/* child process */
		(*f)(frs);
	      case -1:
		perror("[create_process]: fork() failed");
		exit(1);
	      default:
		return (pid);
	}
}

void
setup_signals()
{
	if ((int)signal(SIGUSR1, underrun_error) == -1) {
		perror("[setup_signals]: Error while setting underrun_error signal");
		exit(1);
	}

	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("[setup_signals]: Error while setting overrun_error signal");
		exit(1);
	}
	if ((int)signal(SIGHUP, termination_signal) == -1) {
		perror("[setup_signals]: Error while setting termination signal");
		exit(1);
	}
}


main(int argc, char** argv)
{
	frs_t* frs;
	int minor;
	int pid;
	int wstatus;
	int cpu_number;


	/*
	 * Usage: simple [cpu_number]
	 */

	if (argc == 1) {
		cpu_number = 1;
	} else {
		cpu_number = atoi(argv[1]);
	}

	printf("Running Frame Scheduler on Processor [%d]\n", cpu_number);
	
	/*
	 * Initialize signals to catch
	 * termination signals and underrun, overrun errors.
	 */
	
	setup_signals();

        /*
         * Create sync barrier
         */
        sema_init();
        all_procs_enqueued = barrier_create();
	
	/*
	 * Create the frame scheduler object
	 * cpu = cpu_number,
	 * interrupt source = R4KTIMER
	 * number of minors = 4
	 * slave mask = 0, no slaves
	 * period = 600 [ms] == 600000 [microseconds]
	 */

	if ((frs = frs_create_master(cpu_number, FRS_INTRSOURCE_R4KTIMER, 600000, 4, 0)) == NULL) {
		perror("[main]: frs_create_master failed");
		exit(1);
	}

	/*
	 * Create the processes and enqueue
	 */

	/*
	 * ProcessA will be enqueued on all minor frame queues
	 * with a strict RT discipline
	 */
	
	pid = create_process(processA, frs);
	for (minor = 0; minor < 4; minor++) {
		if (frs_enqueue(frs, pid, minor, FRS_DISC_RT) < 0) {
			perror("[main]: frs_enqueue of ProcessA failed");
			exit(1);
		}
	}

	/*
	 * ProcessB will be enqueued on all minor frames, but the
	 * disciplines will differ. We need continuability for the first
	 * 3 frames, and absolute real-time for the last frame.
	 */

	pid = create_process(processB, frs);
	for (minor = 0; minor < 3; minor++) {
		int disc = FRS_DISC_RT |
		           FRS_DISC_UNDERRUNNABLE |
			   FRS_DISC_OVERRUNNABLE |
			   FRS_DISC_CONT;
		if (frs_enqueue(frs, pid, minor, disc) < 0) {
			perror("[main]: frs_enqueue of ProcessB failed");
			exit(1);
		}
	}
	if (frs_enqueue(frs, pid, 3, FRS_DISC_RT | FRS_DISC_UNDERRUNNABLE) < 0) {
		perror("[main]: frs_enqueue of ProcessB failed");
		exit(1);
	}

        barrier(all_procs_enqueued, 3);
        
	/*
	 * Now we are ready to start the frame scheduler
	 */

	if (frs_start(frs) < 0) {
		perror("[main]: frs_start failed");
		exit(1);
	}

       /*
	* Wait for the two processes to finish
	* The actual exit will happen from the
	* termination signal handler.
	*/
	wait(&wstatus);
	wait(&wstatus);

        sema_cleanup();
        
	exit(0);
}

	













