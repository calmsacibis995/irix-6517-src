/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1998, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include <semaphore.h>
#include <sys/prctl.h>
#include <sched.h>
#include <pthread.h>

/*
 * Example: variable_pt_m1
 *
 * Frame schedulers: 3 (1 Master 2 Slaves)
 *
 * Sources: cctimer
 *
 * Minor Frames: Variable length
 *
 * Thread Model: pthreads
 */

/*
 *  MASTER
 *
 *  |<---300-->|<---300-->|<------600-------->|<-200->|<-200->|<-200->|
 *  |          |          |                   |       |       |       |
 *  |    0     |     1    |         2         |   3   |   4   |   5   |
 *  |          |          |                   |       |       |       |
 *  |    At0   |    At1   |        At2        |  At3  |  At3  |  At3  |
 *  |          |          |                   |       |       |       |
 *  timer      timer      timer               timer   timer   timer
 *
 *
 *  SLAVE 0
 *
 *  |<---300-->|<---300-->|<------600-------->|<-200->|<-200->|<-200->|
 *  |          |          |                   |       |       |       |
 *  |    0     |     1    |         2         |   3   |   4   |   5   |
 *  |          |          |                   |       |       |       |
 *  |    At4   |    At5   |                   |  At6  |       |       |
 *  |          |          |                   |       |       |       |
 *  timer      timer      timer               timer   timer   timer
 *
 *
 *  SLAVE 1
 *
 *  |<---300-->|<---300-->|<------600-------->|<-200->|<-200->|<-200->|
 *  |          |          |                   |       |       |       |
 *  |    0     |     1    |         2         |   3   |   4   |   5   |
 *  |          |          |                   |       |       |       |
 *  |  At7-----+--------> |  At7------------> |  At7 -+-------+-----> |
 *  |          |          |                   |       |       |       |
 *  timer      timer      timer               timer   timer   timer
 *
 */

#define N_MINORS 6
#define N_ACTIVITY_THREADS 8

frs_intr_info_t intr_info[N_MINORS] = {
	{FRS_INTRSOURCE_CCTIMER, 200000},
	{FRS_INTRSOURCE_CCTIMER, 300000},
	{FRS_INTRSOURCE_CCTIMER, 300000},
	{FRS_INTRSOURCE_CCTIMER, 600000},
	{FRS_INTRSOURCE_CCTIMER, 200000},
	{FRS_INTRSOURCE_CCTIMER, 200000}
};

frs_t* master_frs;
frs_t* slave0_frs;
frs_t* slave1_frs;

sem_t enqueue_sem;
sem_t sleep_sem;
sem_t slave_ready_sem;

#define SPIN_COUNT 9000000
int j = 1;

pthread_attr_t pthread_attributes;

pthread_t act_tid[N_ACTIVITY_THREADS];
pthread_t controller_tid[3];

#define MASTER 0
#define SLAVE0 1
#define SLAVE1 2

/*
 * frs_abort: If a pthread calls exit, then all pthreads within the process
 *            will be terminated and the FRS will be destroyed.
 *
 *            For some failure conditions, this is the desired behavior.
 */
#define frs_abort(x)	exit(x)

void master_controller(void);
void slave0_controller(void);
void slave1_controller(void);
void activity(int);
void setup_signals(void);
void overrun_error(void);
void underrun_error(void);

main(int argc, char** argv)
{
	int ret;

        sem_init(&enqueue_sem, 1, 0);
        sem_init(&slave_ready_sem, 1, 0);
        sem_init(&sleep_sem, 1, 0);

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

	ret = pthread_create(&controller_tid[MASTER],
			     &pthread_attributes,
			     (void *(*)(void *)) master_controller,
			     NULL);
	if (ret) {
		fprintf(stderr,	"pthread_create Master failed (%d)\n", ret);
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
master_controller(void)
{
	int i;
	int processor = 1;
	int ret;

	/*
	 * Create Master FRS
	 */

	master_frs = frs_create_vmaster(processor,
					N_MINORS,
					2,
					intr_info);

	if (master_frs == NULL) {
		perror("frs_create_vmaster");
		frs_abort(1);
	}

	/*
	 * Create Slave Controllers
	 */

	ret = pthread_create(&controller_tid[SLAVE0],
			     &pthread_attributes,
			     (void *(*)(void *)) slave0_controller,
			     NULL);
	if (ret) {
		fprintf(stderr,	"pthread_create Slave failed (%d)\n", ret);
		frs_abort(1);
	}

	ret = pthread_create(&controller_tid[SLAVE1],
			     &pthread_attributes,
			     (void *(*)(void *)) slave1_controller,
			     NULL);
	if (ret) {
		fprintf(stderr,	"pthread_create Slave1 failed (%d)\n", ret);
		frs_abort(1);
	}
	
	/*
	 * Create and enqueue Activity Thread 0 on Minor 0
	 */

	ret = pthread_create(&act_tid[0],
			     &pthread_attributes,
			     (void *(*)(void *)) activity,
			     (void*) 0);
	if (ret) {
		fprintf(stderr,	"pthread_create Activity0 failed (%d)\n", ret);
		frs_abort(1);
	}

	if (frs_pthread_enqueue(master_frs, act_tid[0], 0, FRS_DISC_RT) < 0) {
		perror("frs_pthread_enqueue 0:0");
		frs_abort(1);
	}

	/*
	 * Create and enqueue Activity Thread 1 on minor 1
	 */
	ret = pthread_create(&act_tid[1],
			     &pthread_attributes,
			     (void *(*)(void *)) activity,
			     (void*) 1);
	if (ret) {
		fprintf(stderr,	"pthread_create Activity1 failed (%d)\n", ret);
		frs_abort(1);
	}

	if (frs_pthread_enqueue(master_frs, act_tid[1], 1, FRS_DISC_RT) < 0) {
		perror("frs_pthread_enqueue 1:1");
		frs_abort(1);
	}

	/*
	 * Create and enqueue Activity Thread 2 on minor 2
	 */
	ret = pthread_create(&act_tid[2],
			     &pthread_attributes,
			     (void *(*)(void *)) activity,
			     (void*) 2);
	if (ret) {
		fprintf(stderr,	"pthread_create Activity2 failed (%d)\n", ret);
		frs_abort(1);
	}

	if (frs_pthread_enqueue(master_frs, act_tid[2], 2, FRS_DISC_RT) < 0) {
		perror("frs_pthread_enqueue 2:2");
		frs_abort(1);
	}

	/*
	 * Create and enqueue Activity Thread 3 on minors 3,4 & 5
	 */
	ret = pthread_create(&act_tid[3],
			     &pthread_attributes,
			     (void *(*)(void *)) activity,
			     (void*) 3);
	if (ret) {
		fprintf(stderr,	"pthread_create Activity3 failed (%d)\n", ret);
		frs_abort(1);
	}

	if (frs_pthread_enqueue(master_frs, act_tid[3], 3, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 3:3");
		frs_abort(1);
	}

	if (frs_pthread_enqueue(master_frs, act_tid[3], 4, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 3:4");
		frs_abort(1);
	}

	if (frs_pthread_enqueue(master_frs, act_tid[3], 5, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 3:5");
		frs_abort(1);
	}

	/*
	 * Setup error signals
	 */

	setup_signals();

	/*
	 * Wait for slaves to complete initialization
	 */
	sem_wait(&slave_ready_sem);
	sem_wait(&slave_ready_sem);

	/*
	 * Allow all Activity Threads to join their frame schedulers
	 */
	for (i=0; i<N_ACTIVITY_THREADS; i++)
		sem_post(&enqueue_sem);

	/*
	 * Start the Master frame scheduler
	 */

	if (frs_start(master_frs) < 0) {
		perror("frs_start: master");
		frs_abort(1);
	}

	/*
	 * Wait indefinately
	 */

	sem_wait(&sleep_sem);

	frs_abort(0);
}

void
slave0_controller(void)
{
	int wstatus;
	int processor = 2;
	int ret;

	slave0_frs = frs_create_slave(processor, master_frs);
	if (slave0_frs == NULL) {
		perror("frs_create_slave 0");
		frs_abort(1);
	}

	/*
	 * Create and enqueue Activity Thread 4 on minor 0
	 */
	ret = pthread_create(&act_tid[4],
			     &pthread_attributes,
			     (void *(*)(void *)) activity,
			     (void*) 4);
	if (ret) {
		fprintf(stderr,	"pthread_create Activity4 failed (%d)\n", ret);
		frs_abort(1);
	}

	if (frs_pthread_enqueue(slave0_frs, act_tid[4], 0, FRS_DISC_RT) < 0) {
		perror("frs_pthread_enqueue slave1 4:0");
		frs_abort(1);
	}

	/*
	 * Create and enqueue Activity Thread 5 on minor 1
	 */
	ret = pthread_create(&act_tid[5],
			     &pthread_attributes,
			     (void *(*)(void *)) activity,
			     (void*) 5);
	if (ret) {
		fprintf(stderr,	"pthread_create Activity5 failed (%d)\n", ret);
		frs_abort(1);
	}

	if (frs_pthread_enqueue(slave0_frs, act_tid[5], 1, FRS_DISC_RT) < 0) {
		perror("frs_pthread_enqueue 5:1");
		frs_abort(1);
	}

	/*
	 * Create and enqueue Activity Thread 6 on minor 3
	 */
	ret = pthread_create(&act_tid[6],
			     &pthread_attributes,
			     (void *(*)(void *)) activity,
			     (void*) 6);
	if (ret) {
		fprintf(stderr,	"pthread_create Activity6 failed (%d)\n", ret);
		frs_abort(1);
	}

	if (frs_pthread_enqueue(slave0_frs, act_tid[6], 3, FRS_DISC_RT) < 0) {
		perror("frs_pthread_enqueue 6:3");
		frs_abort(1);
	}

	/*
	 * Setup error signals
	 */

	setup_signals();

	/*
	 * Slave finished initialization, signal Master
	 */

	sem_post(&slave_ready_sem);

	/*
	 * Start Slave0 frame scheduler
	 */

	if (frs_start(slave0_frs) < 0) {
		perror("frs_start: slave0");
		frs_abort(1);
	}
	
	/*
	 * Wait indefinately
	 */

	sem_wait(&sleep_sem);

	frs_abort(0);
}

void
slave1_controller(void)
{
	int wstatus;
	int processor = 3;
	int ret;
	int disc;

	slave1_frs = frs_create_slave(processor, master_frs);
	if (slave1_frs == NULL) {
		perror("frs_create_slave 1");
		frs_abort(1);
	}

	/*
	 * Create and enqueue Activity Thread 7 on minors 0 through 1
	 */
	ret = pthread_create(&act_tid[7],
			     &pthread_attributes,
			     (void *(*)(void *)) activity,
			     (void*) 7);
	if (ret) {
		fprintf(stderr,	"pthread_create Activity7 failed (%d)\n", ret);
		frs_abort(1);
	}

	disc = FRS_DISC_RT | FRS_DISC_OVERRUNNABLE;
	if (frs_pthread_enqueue(slave1_frs, act_tid[7], 0, disc) < 0) {
		perror("frs_pthread_enqueue slave1 7:0");
		frs_abort(1);
	}

	disc = FRS_DISC_RT;
	if (frs_pthread_enqueue(slave1_frs, act_tid[7], 1, disc) < 0) {
		perror("frs_pthread_enqueue slave1 7:1");
		frs_abort(1);
	}

	/*
	 * Enqueue Activity Thread 7 on minor 2
	 */

	disc = FRS_DISC_RT;
	if (frs_pthread_enqueue(slave1_frs, act_tid[7], 2, disc) < 0) {
		perror("frs_pthread_enqueue slave1 7:2");
		frs_abort(1);
	}

	/*
	 * Enqueue Activity Thread 7 on minors 3 through 5
	 */

	disc = FRS_DISC_RT | FRS_DISC_OVERRUNNABLE;
	if (frs_pthread_enqueue(slave1_frs, act_tid[7], 3, disc) < 0) {
		perror("frs_pthread_enqueue slave1 7:3");
		frs_abort(1);
	}

	disc = FRS_DISC_RT | FRS_DISC_OVERRUNNABLE;
	if (frs_pthread_enqueue(slave1_frs, act_tid[7], 4, disc) < 0) {
		perror("frs_pthread_enqueue slave1 7:4");
		frs_abort(1);
	}

	disc = FRS_DISC_RT;
	if (frs_pthread_enqueue(slave1_frs, act_tid[7], 5, disc) < 0) {
		perror("frs_pthread_enqueue slave1 7:5");
		frs_abort(1);
	}

	/*
	 * Setup error signals
	 */

	setup_signals();

	/*
	 * Slave finished initialization, signal Master
	 */

	sem_post(&slave_ready_sem);

	/*
	 * Start Slave1 frame scheduler
	 */

	if (frs_start(slave1_frs) < 0) {
		perror("frs_start: slave1");
		frs_abort(1);
	}
	
	/*
	 * Wait indefinately
	 */

	sem_wait(&sleep_sem);

	frs_abort(0);
}
	
void
activity(int act_id)
{
	int previous_minor;
	int pid = getpid();
	int ret;
	frs_t* frs;
	int local_spin = SPIN_COUNT;

	if (act_id < 4)
		frs = master_frs;
	else if (act_id < 7)
		frs = slave0_frs;
	else
		frs = slave1_frs;

	if (act_id == 2 || act_id == 7)
		local_spin *= 2;

	sem_wait(&enqueue_sem);

	/*
	 * Join the frame scheduler
	 */

	if (frs_join(frs) < 0) {
		perror("frs_join");
		frs_abort(1);
	}

	fprintf(stderr, "[Activity:%d (%d)]: Joined Frame Scheduler\n", act_id, pid);
    
	while (1) {
		int i;

		for (i=0; i<local_spin; i++)
			j += i+10;
		
		if ((previous_minor = frs_yield()) < 0) {
			perror("frs_yield");
			frs_abort(1);
		}

		fprintf(stderr,
			"[Activity:%d (%d)] Return from Yield; previous_minor: %d\n",
			act_id, pid, previous_minor);
	}
}

void
setup_signals()
{
	if ((int)signal(SIGUSR1, underrun_error) == -1) {
		perror("[setup_signals]: Error while setting underrun_error signal");
		frs_abort(1);
	}

	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("[setup_signals]: Error while setting overrun_error signal");
		frs_abort(1);
	}
}

void
underrun_error()
{
	if ((int)signal(SIGUSR1, underrun_error) == -1) {
		perror("[underrun_error]: Error while resetting signal");
		frs_abort(1);
	}
	fprintf(stderr, "[underrun_error], PID=%d\n", getpid());	
	frs_abort(2);
}

void
overrun_error()
{
	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("[overrun_error]: Error while resetting signal");
		frs_abort(1);
	}
	fprintf(stderr, "[overrun_error], PID=%d\n", getpid());	
	frs_abort(2);
}	













