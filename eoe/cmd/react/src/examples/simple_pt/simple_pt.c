/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997, Silicon Graphics, Inc.               *
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
 * Pthread Example I
 */

/*
 * This example creates one simple frame scheduler using pthreads.
 *
 * Parameters:
 *
 * 	cpu: 1
 * 	intr_source: FRS_INTRSOURCE_CCTIMER
 * 	n_minors: 4
 * 	period: 600 [ms] (a long period to allow for the printf statements)
 *
 * Threads:
 * 	Thread A: Period of 600 [ms]
 *		  (determined base minor frame period)
 *
 * 	Thread B: Period of 2400 [ms]
 *		  (determined # of minor frames/major frame)
 */

#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <sys/frs.h>

/*
 * frs_abort: If a pthread calls exit, then all pthreads within the process
 *            will be terminated and the FRS will be destroyed.
 *
 *            For some failure conditions, this is the desired behavior.
 */
#define frs_abort(x)	exit(x)

sem_t sem_threads_enqueued;

pthread_attr_t pthread_attributes;

int cpu_number = 1;

/*
 * Some fixed real-time loop parameters
 */

#define NLOOPS_A 20
#define NLOOPS_B 15
#define LOGLOOPS_A 150
#define LOGLOOPS_B 30000

void Thread_Master(void);
void Thread_A(frs_t* frs);
void Thread_B(frs_t* frs);
void setup_signals(void);

/*
 * NOTE: The initial thread of a pthread application (i.e., the thread
 *       executing main) cannot be an FRS controller or an FRS scheduled
 *       activity thread.  This is because all FRS controller and activity
 *       threads must be system scope threads.  The initial thread, however,
 *       is process scope (see pthread_attr_setscope(3P)).
 * 
 *       In this example, the initial thread simply performs some set-up
 *       tasks, launches the system scope Master Controller thread, and
 *       exits.
 */

main(int argc, char** argv)
{
	pthread_t pthread_id_master;
	int ret;

	/*
	 * Usage: simple [cpu_number]
	 */

	if (argc == 2)
		cpu_number = atoi(argv[1]);
			
	/*
	 * Initialize semaphore
	 */

	if (sem_init(&sem_threads_enqueued, 1, 0)) {
		perror("Main: sem_init failed");
		frs_abort(1);
	}

	/*
	 * Initialize signals to catch FRS termination
	 * underrun, and overrun error signals
	 */

	setup_signals();

	/*
	 * Initialize system scope thread attributes
	 */

	if (ret = pthread_attr_init(&pthread_attributes)) {
		fprintf(stderr,
			"Main: pthread_attr_init failed (%d)\n", ret);
		frs_abort(1);
	}

	ret = pthread_attr_setscope(&pthread_attributes, PTHREAD_SCOPE_SYSTEM);
	if (ret) {
		fprintf(stderr,
			"Main: pthread_attr_setscope failed (%d)\n", ret);
		frs_abort(1);
	}
        
	/*
	 * Launch Master Controller Thread
	 */

	ret = pthread_create(&pthread_id_master,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Master,
			     NULL);
	if (ret) {
		fprintf(stderr,
			"Main: pthread_create Thread Master failed (%d)\n", ret);
		frs_abort(1);
	}

	/*
	 * Once the Master Controller is launched, there is no need for
	 * us to hang around.  So we might as well free-up our stack by
	 * exiting via pthread_exit().
	 *
	 * NOTE: Exiting via exit() would be fatal, terminating the
	 *       entire process.
	 */

	pthread_exit(0);
}

void
Thread_Master(void)
{	
	frs_t* frs;
	pthread_t pthread_id_a;
	pthread_t pthread_id_b;
	int minor;
	int disc;
	int ret;

	/*
	 * Create the Frame Scheduler object:
	 *
	 *	cpu = cpu_number,
	 *	interrupt source = CCTIMER
	 *	number of minors = 4
	 *	slave mask = 0, no slaves
	 *	period = 600 [ms] == 600000 [microseconds]
	 */

	frs = frs_create_master(cpu_number,
				FRS_INTRSOURCE_CCTIMER,
				600000,
				4,
				0);
	if (frs == NULL) {
		perror("Master: frs_create_master failed");
		pthread_exit(0);
	}

	/*
	 * Thread A will be enqueued on all minor frame queues
	 * with a strict RT discipline
	 */
	ret = pthread_create(&pthread_id_a,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_A,
			     (void*) frs);
	if (ret) {
		fprintf(stderr,
			"Master: pthread_create Thread A failed (%d)\n", ret);
		pthread_exit(0);
	}

	for (minor = 0; minor < 4; minor++) {
		ret = frs_pthread_enqueue(frs,
					  pthread_id_a,
					  minor,
					  FRS_DISC_RT);
		if (ret) {
			perror("Master: frs_pthread_enqueue Thread A failed");
			pthread_exit(0);
		}
	}

	/*
	 * Thread B will be enqueued on all minor frames, but the
	 * disciplines will differ. We need continuability for the first
	 * 3 frames, and absolute real-time for the last frame.
	 */
	ret = pthread_create(&pthread_id_b,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_B,
			     (void*) frs);
	if (ret) {
		fprintf(stderr,
			"Master: pthread_create Thread B failed (%d)\n", ret);
		pthread_exit(0);
	}

	disc =  FRS_DISC_RT | FRS_DISC_UNDERRUNNABLE |
		FRS_DISC_OVERRUNNABLE | FRS_DISC_CONT;

	for (minor = 0; minor < 3; minor++) {
		ret = frs_pthread_enqueue(frs,
					  pthread_id_b,
					  minor,
					  disc);
		if (ret) {
			perror("Master: frs_pthread_enqueue ThreadB failed");
			pthread_exit(0);
		}
	}

	ret = frs_pthread_enqueue(frs,
				  pthread_id_b,
				  3,
				  FRS_DISC_RT | FRS_DISC_UNDERRUNNABLE);
	if (ret) {
		perror("Master: frs_pthread_enqueue ThreadB failed");
		pthread_exit(0);
	}

	/*
	 * Give all FRS threads the go-ahead to join
	 */

        if (sem_post(&sem_threads_enqueued)) {
		perror("Master: sem_post failed");
		pthread_exit(0);
	}

	if (sem_post(&sem_threads_enqueued)) {
		perror("Master: sem_post failed");
		pthread_exit(0);
	}
        
	/*
	 * Now we are ready to start the frame scheduler
	 */

	printf("Running Frame Scheduler on Processor [%d]\n", cpu_number);

	if (frs_start(frs) < 0) {
		perror("Master: frs_start failed");
		pthread_exit(0);
	}

	/*
	 * Wait for FRS scheduled threads to complete
	 */
	
	if (ret = pthread_join(pthread_id_a, 0)) {
		fprintf(stderr,
			"Master: pthread_join thread A (%d)\n", ret);
		pthread_exit(0);
	}

	if (ret = pthread_join(pthread_id_b, 0)) {
		fprintf(stderr,
			"Master: pthread_join thread B (%d)\n", ret);
		pthread_exit(0);
	}


	/*
	 * Clean-up before exiting
	 */

	(void) pthread_attr_destroy(&pthread_attributes);
	(void) sem_destroy(&sem_threads_enqueued);

	pthread_exit(0);
}

void
Thread_A(frs_t* frs)
{
	int counter;
	double res;
	int i;
	int previous_minor;
	pthread_t pthread_id = pthread_self();

	/*
	 * Join to the frame scheduler once given the go-ahead
	 */

	if (sem_wait(&sem_threads_enqueued)) {
		perror("ThreadA: sem_wait failed");
		frs_abort(1);
	}
        
	if (frs_join(frs) < 0) {
		perror("ThreadA: frs_join failed");
		frs_abort(1);
	}
    
	fprintf(stderr, "ThreadA (%x): Joined Frame Scheduler on cpu %d\n",
		pthread_id, frs->frs_info.cpu);

	counter = NLOOPS_A;
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
		 * yield the cpu. The yield call will not return until
		 * it's our turn to execute again.
		 */

		if ((previous_minor = frs_yield()) < 0) {
			perror("ThreadA: frs_yield failed");
			frs_abort(1);
		}

		fprintf(stderr,
		"ThreadA (%x): Return from Yield; previous_minor: %d\n",
			pthread_id, previous_minor);

	} while (counter--);

	fprintf(stderr, "ThreadA (%x): Exiting\n", pthread_id);

	pthread_exit(0);
}
 
void
Thread_B(frs_t* frs)
{
	int counter;
	double res;
	int i;
	int previous_minor;
	pthread_t pthread_id = pthread_self();

	/*
	 * Join to the frame scheduler once given the go-ahead
	 */

        if (sem_wait(&sem_threads_enqueued)) {
		perror("ThreadB: sem_wait failed");
		frs_abort(1);
	}
        
	if (frs_join(frs) < 0) {
		perror("ThreadB: frs_join failed");
		frs_abort(1);
	}
    
	fprintf(stderr, "ThreadB (%x): Joined Frame Scheduler on cpu %d\n",
		pthread_id, frs->frs_info.cpu);

	counter = NLOOPS_B;
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
			perror("ThreadB: frs_yield failed");
			frs_abort(1);
		}

		fprintf(stderr,
		"ThreadB (%x): Return from Yield; previous_minor: %d\n",
			pthread_id, previous_minor);

	} while (counter--);


	fprintf(stderr, "ThreadB (%x): Exiting\n", pthread_id);

	pthread_exit(0);
}

/*
 * Error Signal handlers
 */

void
underrun_error()
{
	if ((int)signal(SIGUSR1, underrun_error) == -1) {
		perror("[underrun_error]: Error while resetting signal");
		frs_abort(1);
	}
	fprintf(stderr, "[underrun_error], thread %x\n", pthread_self());
	frs_abort(2);
}

void
overrun_error()
{
	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("[overrun_error]: Error while resetting signal");
		frs_abort(1);
	}
	fprintf(stderr, "[overrun_error], thread %d\n", pthread_self());
	frs_abort(2);
}

void
setup_signals()
{
	if ((int)signal(SIGUSR1, underrun_error) == -1) {
		perror("[setup_signals]: Error setting underrun_error signal");
		frs_abort(1);
	}

	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("[setup_signals]: Error setting overrun_error signal");
		frs_abort(1);
	}
}
