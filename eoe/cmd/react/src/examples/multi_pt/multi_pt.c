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
 * Frame Scheduler -- implementation 
 * Pthread Example II
 */

#include <stdio.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <sys/frs.h>
#include "bar.h"

/*
 * This example creates 3 synchronous frame schedulers
 *
 * Specification for FRS1
 * cpu: 1
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 20
 * period (intr_qualifier): 50 [ms]
 * threades: A@20Hz, K1@BACK
 *
 * Specification for FRS2
 * cpu: 2
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 20
 * period (intr_qualifier): 50 [ms]
 * threades: B@10Hz, C@5Hz, K2@BACK
 *
 * Specification for FRS3
 * cpu: 3
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 20
 * period (intr_qualifier): 50 [ms]
 * threades: D@20Hz, E@4Hz, F@2Hz, K3@BACK
 *
 */

#define CPU_MASTER	1
#define CPU_SLAVE1	2
#define CPU_SLAVE2	3

/*
 * frs_abort: If a pthread calls exit, then all pthreads within the process
 *            will be terminated and the FRS will be destroyed.
 *
 *            For some failure conditions, this is the desired behavior.
 */
#define frs_abort(x)	exit(x)

/*
 * Globally visible reference to master FRS
 * We need it to create the slave frs's.
 */

frs_t* master_frs = 0;

sem_t sem_master_created;
bar_t bar_threads_enqueued;

pthread_attr_t pthread_attributes;

pthread_t K1;
pthread_t K2;
pthread_t K3;

void Thread_Master(void);
void Thread_Slave1(void);
void Thread_Slave2(void);
void Thread_Skeleton(frs_t *frs);
void setup_signals(void);

/*
 * NOTE: The initial thread of a pthread application (i.e., the thread
 *       executing main) cannot be an FRS controller or an FRS scheduled
 *       activity thread.  This is because all FRS controller and activity
 *       threads must be system scope threads.  The initial thread, however,
 *       is process scope (see pthread_attr_setscope(3P)).
 * 
 *       In this example, the initial thread simply performs some set-up
 *       tasks, launches the system scope Master & Slave Controller threads,
 *       and waits for the controllers to exit.
 */

main(int argc, char** argv)
{
	pthread_t	pthread_id_master;
	pthread_t	pthread_id_slave1;
	pthread_t	pthread_id_slave2;
	int		ret;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	/*
	 * Initialization of synchronizers
	 */

	if (sem_init(&sem_master_created, 1, 0)) {
		perror("Main: sem_init failed");
		frs_abort(1);
	}

	if (ret = bar_init(&bar_threads_enqueued, 12)) {
		fprintf(stderr, "Main: bar_init failed (%d)\n", ret);
		frs_abort(1);
	}
	
	/*
	 * Initialize signals to catch FRS underrun, and
	 * overrun condition signals
	 */

	setup_signals();

	/*
	 * Initialize system scope thread attributes, since
	 * ALL FRS threads (controllers and activity) must be system scope.
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
			"Main: pthread_create Master failed (%d)\n", ret);
		frs_abort(1);
	}	

	/*
	 * Wait for the master FRS to be created
	 */

	if (sem_wait(&sem_master_created)) {
		perror("Main: sem_wait failed");
		frs_abort(1);
	}
	
	/*
	 * The master has been created.
	 * We can proceed to create the Slave Controller Threads.
	 */

	ret = pthread_create(&pthread_id_slave1,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Slave1,
			     NULL);
	if (ret) {
		fprintf(stderr,
			"Main: pthread_create Slave1 failed (%d)\n", ret);
		frs_abort(1);
	}

	ret = pthread_create(&pthread_id_slave2,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Slave2,
			     NULL);
	if (ret) {
		fprintf(stderr,
			"Main: pthread_create Slave2 failed (%d)\n", ret);
		frs_abort(1);
	}

	/*
	 * Wait for each Controller Thread to exit
	 *
	 * NOTE: In this example, the Thread_Skeleton activity threads
	 *       execute in an infinite loop. This behavior will prevent
	 *       the controller threads exiting gracefully via pthread_exit
	 *       and rendezvousing with the pthread_join below.
	 */

	if (ret = pthread_join(pthread_id_master, 0)) {
		fprintf(stderr,
			"Main: pthread_join with Master failed (%d)\n", ret);
		frs_abort(1);
	}

	if (ret = pthread_join(pthread_id_slave1, 0)) {
		fprintf(stderr,
			"Main: pthread_join with Slave1 failed (%d)\n", ret);
		frs_abort(1);
	}

	if (ret = pthread_join(pthread_id_slave2, 0)) {
		fprintf(stderr,
			"Main: pthread_join with Slave2 failed (%d)\n", ret);
		frs_abort(1);
	}		

	/*
	 * Clean-up before exiting
	 */

	(void) pthread_attr_destroy(&pthread_attributes);
	(void) bar_destroy(&bar_threads_enqueued);
	
	pthread_exit(0);
}

void
Thread_Master(void)
{
	frs_t		*frs;
	int		 minor;
	int		 ret;

	pthread_t	 pthread_id_a;
	pthread_t	 pthread_id_k1;

	/*
	 * Create the master frame scheduler
	 * cpu = 1
	 * interrupt source = CCTIMER
	 * number of minors = 20
	 * slaves = 2, 3
	 * number_of_slaves = 2
	 * period (intr_qualifier) = 50 [ms] == 50000 [microseconds]
	 */

	frs = frs_create_master(CPU_MASTER,
				FRS_INTRSOURCE_CCTIMER,
				50000,
				20,
				2);
	if (frs == NULL) {
		perror("Thread Master: frs_create_master failed");
		pthread_exit(0);
	}

	master_frs = frs;

	printf("Master Controller initialized for cpu %d\n", CPU_MASTER);

	/*
	 * Signal the main thread that the master frs has been created
	 */

	if (sem_post(&sem_master_created)) {
		perror("Thread Master: sem_post failed");
		frs_abort();
	}

	/*
	 * Thread: A
	 * frs:    Master (on cpu 1)
	 * freq:   20Hz
	 */

	ret = pthread_create(&pthread_id_a,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Skeleton,
			     (void *) frs);
	if (ret) {
		fprintf(stderr,
		"Thread Master: pthread_create Thread A failed (%d)\n", ret);
		pthread_exit(0);
	}

	for (minor = 0; minor < 20; minor++) {
		ret = frs_pthread_enqueue(frs,
					  pthread_id_a,
					  minor,
					  FRS_DISC_RT);
		if (ret) {
			perror("Thread Master: frs_pthread_enqueue Thread A");
			pthread_exit(0);
		}
	}

	/*
	 * Thread: K1
	 * frs:    Master (on cpu 1)
	 * freq:   Background
	 */

	ret = pthread_create(&pthread_id_k1,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Skeleton,
			     (void *) frs);
	if (ret) {
		fprintf(stderr,
		"Thread Master: pthread_create Thread K1 failed (%d)\n", ret);
		pthread_exit(0);
	}

	K1 = pthread_id_k1;

	for (minor = 0; minor < 20; minor++) {
		ret = frs_pthread_enqueue(frs,
					  pthread_id_k1,
					  minor,
					  FRS_DISC_BACKGROUND);
		if (ret) {
			perror("Thread Master: frs_pthread_enqueue Thread K1");
			pthread_exit(0);
		}
	}

        bar_wait(&bar_threads_enqueued);

	printf("Running Frame Scheduler on Threadors 1, 2, 3\n");

	/*
	 * We now start this Frame Scheduler
	 */
	
	if (frs_start(frs) < 0) {
		perror("Thread Master: frs_start cpu 1");
		pthread_exit(0);
	}
	
       /*
	* Wait for the two FRS scheduled threads to finish.
	*
	* NOTE: In this example, the Thread_Skeleton activity threads
	*       execute in an infinite loop. This behavior prevents
	*       the activity threads from ever rendezvousing with the
	*       pthread_join routines below.
	*/

	if (ret = pthread_join(pthread_id_a, 0)) {
		fprintf(stderr,
			"Master: pthread_join with thread A failed (%d)\n", ret);
		frs_abort(1);
	}

	if (ret = pthread_join(pthread_id_k1, 0)) {
		fprintf(stderr,
			"Master: pthread_join with thread K1 failed (%d)\n", ret);
		frs_abort(1);
	}

	pthread_exit(0);
}

void
Thread_Slave1(void)
{
	frs_t* frs;
	int minor;
	int subminor;
	int ret;

	pthread_t pthread_id_b;
	pthread_t pthread_id_c;
	pthread_t pthread_id_k2;

	/*
	 * Create first slave
	 * cpu = 2
	 * interrupt source = CCTIMER
	 * number of minors = 20
	 * slaves = --
	 * number_of_slaves = --
	 * period (intr_qualifier) = 50 [ms] == 50000 [microseconds]
	 */

	if ((frs = frs_create_slave(CPU_SLAVE1, master_frs)) == NULL) {
		perror("Thread_Slave1: frs_create_slave on cpu 2 failed");
		pthread_exit(0);
	}

	/*
	 * Thread: B
	 * frs:    Slave1 (on cpu 2)
	 * freq:   10Hz
	 */

	ret = pthread_create(&pthread_id_b,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Skeleton,
			     (void *) frs);
	if (ret) {
		fprintf(stderr,
		"Thread Slave1: pthread_create Thread B failed (%d)\n", ret);
		pthread_exit(0);
	}

	for (minor = 0; minor < 20; minor += 2) {
		ret = frs_pthread_enqueue(frs,
					  pthread_id_b,
					  minor,
					  FRS_DISC_BACKGROUND);
		if (ret) {
			perror("Thread Slave1: frs_pthread_enqueue Thread B");
			pthread_exit(0);
		}

		ret = frs_pthread_enqueue(frs,
					  pthread_id_b,
					  minor+1,
					  FRS_DISC_RT);
		if (ret) {
			perror("Thread Slave1: frs_pthread_enqueue Thread B");
			pthread_exit(0);
		}
	}

	/*
	 * Thread: C
	 * frs:    Slave1 (on cpu 2)
	 * freq:   5Hz
	 */

	ret = pthread_create(&pthread_id_c,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Skeleton,
			     (void *) frs);
	if (ret) {
		fprintf(stderr,
		"Thread Slave1: pthread_create Thread C failed (%d)\n", ret);
		pthread_exit(0);
	}

	for (minor = 0; minor < 20; minor += 4) {
		for (subminor = minor; subminor < (minor + 3); subminor++) {
			ret = frs_pthread_enqueue(frs,
						  pthread_id_c,
						  subminor,
						  FRS_DISC_BACKGROUND);
			if (ret) {
				perror(
				"Thread Slave1: frs_pthread_enqueue Thread C");
				pthread_exit(0);
			}
		}

		ret = frs_pthread_enqueue(frs,
					  pthread_id_c,
					  minor + 3,
					  FRS_DISC_RT);
		if (ret) {
			perror("Thread Slave1: frs_pthread_enqueue Thread C");
			pthread_exit(0);
		}
	}

	/*
	 * Thread: K2
	 * frs:    Slave1 (on cpu 2)
	 * freq:   Background
	 */

	ret = pthread_create(&pthread_id_k2,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Skeleton,
			     (void *) frs);
	if (ret) {
		fprintf(stderr,
		"Thread Slave1: pthread_create Thread K2 failed (%d)\n", ret);
		pthread_exit(0);
	}

	K2 = pthread_id_k2;

	for (minor = 0; minor < 20; minor++) {
		ret = frs_pthread_enqueue(frs,
					  pthread_id_k2,
					  minor,
					  FRS_DISC_BACKGROUND);
		if (ret) {
			perror("Thread Slave1: frs_pthread_enqueue Thread K2");
			pthread_exit(0);
		}
	}

	printf("Slave1 Controller initialized for cpu %d\n", CPU_SLAVE1);

        bar_wait(&bar_threads_enqueued);

	/*
	 * We now start this Frame Scheduler
	 */
	
	if (frs_start(frs) < 0) {
		perror("Thread Slave1: frs_start cpu 2");
		pthread_exit(0);
	}

	/*
	 * Wait for the three FRS scheduled threads to finish.
	 *
	 * NOTE: In this example, the Thread_Skeleton activity threads
	 *       execute in an infinite loop. This behavior prevents
	 *       the activity threads from ever rendezvousing with the
	 *       pthread_join below.
	 */

	if (ret = pthread_join(pthread_id_b, 0)) {
		fprintf(stderr,
			"Slave1: pthread_join with thread B failed (%d)\n", ret);
		frs_abort(1);
	}

	if (ret = pthread_join(pthread_id_c, 0)) {
		fprintf(stderr,
			"Slave1: pthread_join with thread C failed (%d)\n", ret);
		frs_abort(1);
	}

	if (ret = pthread_join(pthread_id_k2, 0)) {
		fprintf(stderr,
			"Slave1: pthread_join with thread K2 failed (%d)\n", ret);
		frs_abort(1);
	}

	pthread_exit(0);
}

void
Thread_Slave2(void)
{
	frs_t* frs;
	int minor;
	int subminor;
	int ret;

	pthread_t pthread_id_d;
	pthread_t pthread_id_e;
	pthread_t pthread_id_f;
	pthread_t pthread_id_k3;

	/*
	 * Create second slave
	 * cpu = 3
	 * interrupt source = CCTIMER
	 * number of minors = 20
	 * slaves = --
	 * number_of_slaves = --
	 * period (intr_qualifier) = 50 [ms] == 50000 [microseconds]
	 */

	if ((frs = frs_create_slave(CPU_SLAVE2, master_frs)) == NULL) {
		perror("Thread_Slave2: frs_create_slave on cpu 3 failed");
		pthread_exit(0);
	}

	/*
	 * Thread: D
	 * frs:    Slave2 (on cpu 3)
	 * freq:   20Hz
	 */

	ret = pthread_create(&pthread_id_d,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Skeleton,
			     (void *) frs);
	if (ret) {
		fprintf(stderr,
		"Thread Slave2: pthread_create Thread D failed (%d)\n", ret);
		pthread_exit(0);
	}

	for (minor = 0; minor < 20; minor++) {
		if (frs_pthread_enqueue(frs, pthread_id_d, minor, FRS_DISC_RT)) {
			perror("Thread Slave2: frs_pthread_enqueue Thread D");
			pthread_exit(0);
		}
	}

	/*
	 * Thread: E
	 * frs:    Slave2 (on cpu 3)
	 * freq:   4Hz
	 */

	ret = pthread_create(&pthread_id_e,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Skeleton,
			     (void *) frs);
	if (ret) {
		fprintf(stderr,
		"Thread Slave2: pthread_create Thread E failed (%d)\n", ret);
		pthread_exit(0);
	}

	for (minor = 0; minor < 20; minor += 5) {
		for (subminor = minor; subminor < (minor + 4); subminor++) {
			ret = frs_pthread_enqueue(frs,
						  pthread_id_e,
						  subminor,
						  FRS_DISC_BACKGROUND);
			if (ret) {
				perror(
				"Thread Slave2: frs_pthread_enqueue Thread E");
				pthread_exit(0);
			}
		}
		ret = frs_pthread_enqueue(frs,
					  pthread_id_e,
					  minor + 4,
					  FRS_DISC_RT);
		if (ret) {
			perror("Thread Slave2: frs_pthread_enqueue Thread E");
			pthread_exit(0);
		}
	}

	/*
	 * Thread: F
	 * frs:    Slave2 (on cpu 3)
	 * freq:   2Hz
	 */

	ret = pthread_create(&pthread_id_f,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Skeleton,
			     (void *) frs);
	if (ret) {
		fprintf(stderr,
		"Thread Slave2: pthread_create Thread E failed (%d)\n", ret);
		pthread_exit(0);
	}

	for (minor = 0; minor < 20; minor += 10) {
		for (subminor = minor; subminor < (minor + 9); subminor++) {
			ret = frs_pthread_enqueue(frs,
						  pthread_id_f,
						  subminor,
						  FRS_DISC_BACKGROUND);
			if (ret) {
				perror(
				"Thread Slave1: frs_pthread_enqueue Thread F");
				pthread_exit(0);
			}
		}
		ret = frs_pthread_enqueue(frs,
					  pthread_id_f,
					  minor + 9,
					  FRS_DISC_RT);
		if (ret) {
			perror("Thread Slave1: frs_pthread_enqueue Thread F");
			pthread_exit(0);
		}
	}

	/*
	 * Thread: K3
	 * frs:    Slave2 (on cpu 3)
	 * freq:   Background
	 */

	ret = pthread_create(&pthread_id_k3,
			     &pthread_attributes,
			     (void *(*)(void *)) Thread_Skeleton,
			     (void *) frs);
	if (ret) {
		fprintf(stderr,
		"Thread Slave2: pthread_create Thread K3 failed (%d)\n", ret);
		pthread_exit(0);
	}

	K3 = pthread_id_k3;

	for (minor = 0; minor < 20; minor++) {
		ret = frs_pthread_enqueue(frs,
					  pthread_id_k3,
					  minor,
					  FRS_DISC_BACKGROUND);
		if (ret) {
			perror("Thread Slave2: frs_pthread_enqueue Thread K3");
			pthread_exit(0);
		}
	}

	printf("Slave2 Controller initialized for cpu %d\n", CPU_SLAVE2);

        bar_wait(&bar_threads_enqueued);
        
	/*
	 * We now start this Frame Scheduler
	 */
        
	if (frs_start(frs) < 0) {
		perror("Thread Slave2: frs_start cpu 2");
		pthread_exit(0);
	}

       /*
	* Wait for the four FRS scheduled threads to finish.
	*
	* NOTE: In this example, the Thread_Skeleton activity threads
	*       execute in an infinite loop. This behavior prevents
	*       the rendezvous with the pthread_join below.
	*/

	if (ret = pthread_join(pthread_id_d, 0)) {
		fprintf(stderr,
			"Slave2: pthread_join with thread D failed (%d)\n", ret);
		frs_abort(1);
	}

	if (ret = pthread_join(pthread_id_e, 0)) {
		fprintf(stderr,
			"Slave2: pthread_join with thread E failed (%d)\n", ret);
		frs_abort(1);
	}

	if (ret = pthread_join(pthread_id_f, 0)) {
		fprintf(stderr,
			"Slave2: pthread_join with thread F failed (%d)\n", ret);
		frs_abort(1);
	}

	if (ret = pthread_join(pthread_id_k3, 0)) {
		fprintf(stderr,
			"Slave2: pthread_join with thread K3 failed (%d)\n", ret);
		frs_abort(1);
	}

	pthread_exit(0);
}	

void
Thread_Skeleton(frs_t *frs)
{
	int previous_minor;
	int cpu;
	int loop = 1;
	int background_thread;
	pthread_t pthread_id = pthread_self();

	cpu = frs->frs_info.cpu;
	
	/*
	 * We join the frs after making sure
         * everybody has been enqueued.
	 */

        bar_wait(&bar_threads_enqueued);

	if (pthread_id == K1 || pthread_id == K2 || pthread_id == K3)
		background_thread = 1;
	else
		background_thread = 0;

	fprintf(stderr,
	"Thread_Skeleton: Cpu [%d] Thread [%x] Joining Frame Scheduler\n",
		cpu, pthread_id);

	if (frs_join(frs) < 0) {
		fprintf(stderr,
		"Thread_Skeleton: Cpu [%d] Thread [%x] frs_join failed\n",
			cpu, pthread_id);
		perror("Thread_Skeleton");
		pthread_exit(0);
	}

	/*
	 * This is the real-time loop.
	 * The first iteration executes after returning from frs_join
	 */

	do {
		/*
		 * RT computations would be performed here.
		 */

		/*
		 * Once computations are completed we yield the cpu.
		 * The yield call will not return until it's our turn
		 * to execute again.
		 */

		if ((previous_minor = frs_yield()) < 0) {
			fprintf(stderr,
				"%s Cpu [%d] Thread [%x] frs_yield failed\n",
				"Thread_Skeleton:", cpu, pthread_id);
			perror("Thread_Skeleton");
			pthread_exit(0);
		}
			
		if (background_thread) {
			fprintf(stderr,
			"%s Cpu [%d] Thread [%x]: Return from Yield; pm: %d\n",
				"Thread_Skeleton:", cpu, pthread_id,
				previous_minor);
		}
		
	} while (loop);
		
    
	fprintf(stderr, "Thread_Skeleton: Cpu [%d] Thread [%x]: Exiting\n",
		cpu, pthread_id);

	pthread_exit(0);
}

/*
 * Error Signal handlers
 */

void
underrun_error()
{
	if ((int)signal(SIGUSR1, underrun_error) == -1) {
		perror("underrun_error: Error while resetting signal");
		frs_abort(1);
	}
	fprintf(stderr, "underrun_error: Thread %x\n", pthread_self());	
	frs_abort(2);
}

void
overrun_error()
{
	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("overrun_error: Error while resetting signal");
		frs_abort(1);
	}
	fprintf(stderr, "overrun_error: Thread %x\n", pthread_self());	
	frs_abort(2);
}

void
setup_signals()
{
	if ((int)signal(SIGUSR1, underrun_error) == -1) {
		perror("setup_signals: Error setting underrun_error signal");
		frs_abort(1);
	}

	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("setup_signals: Error setting overrun_error signal");
		frs_abort(1);
	}
}
