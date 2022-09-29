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
 * Very simple 60Hz frame scheduling of a process
 * It also does makes some simple changes to the signal
 * attributes
 */

/*
 * This example creates one simple frame scheduler.
 *
 * PARAMETERS:
 * cpu: 1
 * intr_source: FRS_INTRSOURCE_CCTIMER or FRS_INTRSOURCE_CPUTIMER
 * n_minors: 1
 * period: 16.666 [ms]
 *
 * PROCESSES:
 * Process A: Frequency of 60Hz
 */
#define _BSD_SIGNALS
#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include "sema.h"

#define FRS_SIGNAL_UNDERRUN	SIGRTMIN + 1
#define FRS_SIGNAL_OVERRUN	SIGRTMIN + 2
#define FRS_SIGNAL_DEQUEUE	0
#define FRS_SIGNAL_UNFRAMESCHED	0

/*
 * Some fixed real-time loop parameters
 */

#define NLOOPS 400
#define LOGLOOPS_A 15

/*
 * A message right after join will take up time from the
 * first minor frame - it may NOT be safe to leave here;
 * sometimes, depending on where the printf characters are
 * being displayed, overrun errors may happen.
 */
#define JOINM_A

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

	fprintf(stderr, "[underrun_error], PID=%d\n", getpid());	
	exit(2);
}

void
overrun_error()
{
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
         * after all procs have been enqueued
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

    struct sigaction act;

    sigemptyset(&act.sa_mask);
    act.sa_handler = underrun_error;
    if((int)sigaction(FRS_SIGNAL_UNDERRUN, &act, (sigaction_t *)0) == -1) {
	perror("[setup_signals]: Error while setting underrun_error signal");
	exit(1);
    }
    act.sa_handler = overrun_error;
    if((int)sigaction(FRS_SIGNAL_OVERRUN, &act, (sigaction_t *)0) == -1) {
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
	frs_signal_info_t signal_info;
	int error;
	int parse;

	int cpu_number = 1;
	int intr_source = FRS_INTRSOURCE_CCTIMER;
	/*
	 * Usage: simple [cpu_number]
	 */
	while ((parse = getopt(argc, argv, "c:i")) != EOF)
		switch (parse) {
		case 'c':
			cpu_number = atoi(optarg);
			break;
		case 'i':
			intr_source = FRS_INTRSOURCE_CPUTIMER;
			break;
		}

	printf("Running Frame Scheduler on Processor [%d]\n", cpu_number);
	
	/*
	 * Initialize signals to catch underrun and overrun conditions.
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
	 * interrupt source = CCTIMER or CPUTIMER
	 * number of minors = 1
	 * slave mask = 0, no slaves
	 * period = 16.666 [ms] == 16666 [microseconds]
	 */

	frs = frs_create_master(cpu_number, intr_source, 16666, 1, 0);
	if (frs == NULL) {
		perror("[main]: frs_create_master failed");
		exit(1);
	}

	/*
	 * Create the processes and enqueue
	 */
	bzero(&signal_info, sizeof(signal_info));
	error = frs_getattr(frs, 0, 0, FRS_ATTR_SIGNALS, &signal_info);
	signal_info.sig_underrun = FRS_SIGNAL_UNDERRUN;
	signal_info.sig_overrun = FRS_SIGNAL_OVERRUN;
	signal_info.sig_dequeue = FRS_SIGNAL_DEQUEUE;
	signal_info.sig_unframesched = FRS_SIGNAL_UNFRAMESCHED;

	error = frs_setattr(frs, 0, 0, FRS_ATTR_SIGNALS, &signal_info);
	error = frs_getattr(frs, 0, 0, FRS_ATTR_SIGNALS, &signal_info);

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
	* Wait for the  process to finish.
	*/
	wait(&wstatus);

        sema_cleanup();
        
	exit(0);
}

	













