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
 * Very simple 60Hz frame scheduling of a process (with mem locking)
 */

/*
 * This example creates one simple frame scheduler.
 *
 * PARAMETERS:
 * cpu: 1
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 1
 * period: 16.666 [ms]
 *
 * PROCESSES:
 * Process A: Frequency of 60Hz
 */

#include <math.h>
#include <errno.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include "sema.h"

/*
 * Some fixed real-time loop parameters
 */

#define NLOOPS 400
#define LOGLOOPS_A 20

/*
 * A message right after join will take up time from the
 * first minor frame - it may NOT be safe to leave here;
 * sometimes, depending on where the printf characters are
 * being displayed, overrun errors may happen.
 * For this example, this message will be disabled.
 */
/*#define JOINM_A*/

/*
 *  Messages after every yield are NOT recommended
 *  when running at 60 Hz
 */

/*#define YIELDM_A*/

/*
 * Barrier to synchronize joins, enqueues, starts.
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
processA(frs_t* frs)
{
	int counter;
	double res;
	int i;
	int previous_minor;
	int pid = getpid();

	/*
	 * In order to avoid page-faults while
	 * running the real-time loop, we
	 * can lock-down (in memory) the whole
	 * process.
	 *
	 * See mlockall(2), plock(2) and mpin(2).
	 */
	if (mlockall(MCL_CURRENT) < 0) {
		perror("[processA]: mlockall");
		exit(1);
	}
	
	/*
	 * First, we join to the frame scheduler after
         * the process has been enqueued.
	 */

        barrier(all_procs_enqueued, 2);
        
	if (frs_join(frs) < 0) {
		perror("[processA]: frs_join failed");
		exit(1);
	}
    
#ifdef JOINM_A
	fprintf(stderr, "[processA (%d)]: Joined Frame Scheduler on cpu %d\n",
		pid, frs->frs_info.cpu);
#endif

	/*
	 * To avoid overruns caused by initial cold caches,
	 * the first major frame may be skipped.
	 * This can be accomplished by doing as many
	 * frs_yields as minors per major.
	 * In this case, the number is 1
	 */
	
	if ((previous_minor = frs_yield()) < 0) {
		perror("[processA]: frs_yield failed");
		exit(1);
	}
	
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

	fprintf(stderr, "[ProcessA (%d)]: Exiting\n", pid);

	if (munlockall() < 0) {
		perror("[processA]: munlockall");
		exit(1);
	}

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
	 * interrupt source = CCTIMER
	 * number of minors = 1
	 * slave mask = 0, no slaves
	 * period = 16.666 [ms] == 16666 [microseconds]
	 */

	if ((frs = frs_create_master(cpu_number, FRS_INTRSOURCE_CCTIMER, 16666, 1, 0)) == NULL) {
		perror("[main]: frs_create_master failed");
		exit(1);
	}

	/*
	 * Create the processes and enqueue
	 */

	/*
	 * ProcessA will be enqueued on the single minor frame queue
	 * with a strict RT discipline
	 */
	
	pid = create_process(processA, frs);
	if (frs_enqueue(frs, pid, 0, FRS_DISC_RT) < 0) {
		perror("[main]: frs_enqueue of ProcessA failed");
		exit(1);
	}

        barrier(all_procs_enqueued, 2);

	/*
	 * Now we are ready to start the frame scheduler
	 */

	if (frs_start(frs) < 0) {
		perror("[main]: frs_start failed");
		exit(1);
	}

       /*
	* Wait for the  process to finish
	* The actual exit will happen from the
	* termination signal handler.
	*/
	wait(&wstatus);

        sema_cleanup();

	exit(0);
}

	













