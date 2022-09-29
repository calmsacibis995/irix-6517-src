/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, 1998 , Silicon Graphics, Inc.        *
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
 * Example Using frs_pinsert and frs_premove - modified from simple.c
 */

/*
 * This example creates one simple frame scheduler.
 *
 * PARAMETERS:
 * cpu: 1
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 4
 * period: 600 [ms] (a long period to allow for the printf statements)
 *
 * PROCESSES:
 * Process A: Period of 600 [ms] (determined base minor frame period)
 * Process B: Period of 2400 [ms] (determined # of minor frames per major frame)
 *            (inserted into the frs 10 frames after startup - then 
 *             removed 10 frames later)
 */


#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include "sema.h"

usema_t* all_procs_enqueued;
usema_t* process_ready_to_insert;
int pidA, pidB;

/*
 * Some fixed real-time loop parameters
 */

#define NLOOPS 60
#define LOGLOOPS_A 150
#define LOGLOOPS_B 30000
#define JOINM_A
#define JOINM_B
#define YIELDM_A
#define YIELDM_B

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
processA(frs_t* frs)
{
	int counter;
	double res;
	int i;
	int previous_minor;
	int pid = getpid();

	/*
	 * First, we join to the frame scheduler
	 */

        sema_p(all_procs_enqueued);
        
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

		if (NLOOPS == (counter+10))
        		sema_v(process_ready_to_insert);

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
	int disc, minor;

	/*
	 * First, we wait to be inserted 
	 */

	fprintf(stderr, "[processB (%d)]: Entered - waiting  \n",pid);
        sema_p(process_ready_to_insert);

	/*
	 * Next, we insert ourselves into the running frs
	 */

	disc = 	FRS_DISC_RT | FRS_DISC_UNDERRUNNABLE |
		FRS_DISC_OVERRUNNABLE | FRS_DISC_CONT;

	for (minor = 0; minor < 3; minor++) {
		if (frs_pinsert(frs, minor, pid, disc, pidA) < 0) {
			perror("[processB]: frs_pinsert of ProcessB failed");
			exit(1);
		}
	}
	disc = FRS_DISC_RT|FRS_DISC_UNDERRUNNABLE;
	if (frs_pinsert(frs, 3, pid, disc, pidA) < 0) {
		perror("[processB]: frs_pinsert of ProcessB failed");
		exit(1);
	}


	/*
	 * Next, we join to the frame scheduler
	 */

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
    
	if ((previous_minor = frs_yield()) < 0) {
		perror("[processB]: frs_yield failed");
		exit(1);
	}

	do {
		for (i = 0; i < LOGLOOPS_B; i++) {
			res = res * log(res) - res * sqrt(res);
		}

		if (NLOOPS == (counter+10)) {
			fprintf(stderr, "[processB (%d)]: Removing from frs\n",pid);
			for (minor = 0; minor < 4; minor++) {
				if (frs_premove(frs, minor, pid) < 0) {
					perror("[processB]: frs_premove of ProcessB failed");
					exit(1);
				}
			}
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
}


main(int argc, char** argv)
{
	frs_t* frs;
	int minor;
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

        sema_init(2); /* Called with the # of procs in the share group */

        all_procs_enqueued = sema_create();
        process_ready_to_insert = sema_create();
        
	/*
	 * Initialize signals to catch underrun and overrun errors.
	 */
	
	setup_signals();
	
	/*
	 * Create the frame scheduler object
	 * cpu = cpu_number,
	 * interrupt source = CCTIMER
	 * number of minors = 4
	 * slave mask = 0, no slaves
	 * period = 600 [ms] == 600000 [microseconds]
	 */

	if ((frs = frs_create_master(cpu_number, FRS_INTRSOURCE_CCTIMER, 600000, 4, 0)) == NULL) {
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
	
	pidA = create_process(processA, frs);
	for (minor = 0; minor < 4; minor++) {
		if (frs_enqueue(frs, pidA, minor, FRS_DISC_RT) < 0) {
			perror("[main]: frs_enqueue of ProcessA failed");
			exit(1);
		}
	}

	/*
	 * ProcessB will be enqueued on all minor frames, but the
	 * disciplines will differ. We need continuability for the first
	 * 3 frames, and absolute real-time for the last frame.
	 *
	 * do not enqueue - insert at a later time
	 */

	pidB = create_process(processB, frs);
        sema_v(all_procs_enqueued);
        
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

	


