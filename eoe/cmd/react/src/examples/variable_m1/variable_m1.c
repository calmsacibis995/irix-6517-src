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
#include <ulocks.h>

/*
 * Example: variable_m1
 *
 * Frame schedulers: 3 (1 Master 2 Slaves)
 *
 * Sources: cctimer
 *
 * Minor Frames: Variable length
 *
 * Thread Model: sproc
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
sem_t slave_ready_sem;

#define SPIN_COUNT 9000000
int j = 1;

pid_t act_pid[N_ACTIVITY_THREADS];
pid_t slave_pid[2];

void slave0_controller(void);
void slave1_controller(void);
void activity(int);
void setup_signals(void);
void overrun_error(void);
void underrun_error(void);

main(int argc, char** argv)
{
	int i;
	int wstatus;
	int processor = 1;

        sem_init(&enqueue_sem, 1, 0);
        sem_init(&slave_ready_sem, 1, 0);

	usconfig(CONF_INITUSERS, 11);

	/*
	 * Create Master FRS
	 */

	master_frs = frs_create_vmaster(processor,
					N_MINORS,
					2,
					intr_info);

	if (master_frs == NULL) {
		perror("frs_create_vmaster");
		exit(1);
	}

	/*
	 * Create Slave Controllers
	 */
	slave_pid[0] = sprocsp(slave0_controller, PR_SALL, (void*)0, NULL, 8192);
	if (slave_pid[0] < 0) {
		perror("sproc slave0");
		exit(1);
	}

	slave_pid[1] = sprocsp(slave1_controller, PR_SALL, (void*)0, NULL, 8192);
	if (slave_pid[1] < 0) {
		perror("sproc slave1");
		exit(1);
	}
	
	/*
	 * Create and enqueue Activity Thread 0 on Minor 0
	 */

	act_pid[0] = sprocsp(activity, PR_SALL, (void*)0, NULL, 8192);
	if (act_pid[0] < 0) {
		perror("sproc: Activity Thread 0");
		exit(1);
	}

	if (frs_enqueue(master_frs, act_pid[0], 0, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 0:0");
		exit(1);
	}

	/*
	 * Create and enqueue Activity Thread 1 on minor 1
	 */
	act_pid[1] = sprocsp(activity, PR_SALL, (void*)1, NULL, 8192);
	if (act_pid[1] < 0) {
		perror("sproc: Activity Thread 1");
		exit(1);
	}

	if (frs_enqueue(master_frs, act_pid[1], 1, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 1:1");
		exit(1);
	}

	/*
	 * Create and enqueue Activity Thread 2 on minor 2
	 */
	act_pid[2] = sprocsp(activity, PR_SALL, (void*)2, NULL, 8192);
	if (act_pid[2] < 0) {
		perror("sproc: Activity Thread 2");
		exit(1);
	}

	if (frs_enqueue(master_frs, act_pid[2], 2, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 2:2");
		exit(1);
	}

	/*
	 * Create and enqueue Activity Thread 3 on minors 3,4 & 5
	 */
	act_pid[3] = sprocsp(activity, PR_SALL, (void*)3, NULL, 8192);
	if (act_pid[3] < 0) {
		perror("sproc: Activity Thread 3");
		exit(1);
	}

	if (frs_enqueue(master_frs, act_pid[3], 3, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 3:3");
		exit(1);
	}

	if (frs_enqueue(master_frs, act_pid[3], 4, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 3:4");
		exit(1);
	}

	if (frs_enqueue(master_frs, act_pid[3], 5, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 3:5");
		exit(1);
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
		exit(1);
	}

	/*
	 * Wait for Slave controllers and
	 * Master Activity threads to exit
	 */
	wait(&wstatus);
	wait(&wstatus);

	wait(&wstatus);
	wait(&wstatus);
	wait(&wstatus);
	wait(&wstatus);

	exit(0);
}

void
slave0_controller(void)
{
	int wstatus;
	int processor = 2;

	slave0_frs = frs_create_slave(processor, master_frs);
	if (slave0_frs == NULL) {
		perror("frs_create_slave 0");
		exit(1);
	}

	/*
	 * Create and enqueue Activity Thread 4 on minor 0
	 */

	act_pid[4] = sprocsp(activity, PR_SALL, (void*)4, NULL, 8192);
	if (act_pid[4] < 0) {
		perror("sproc: Activity Thread 4");
		exit(1);
	}

	if (frs_enqueue(slave0_frs, act_pid[4], 0, FRS_DISC_RT) < 0) {
		perror("frs_enqueue slave1 4:0");
		exit(1);
	}

	/*
	 * Create and enqueue Activity Thread 5 on minor 1
	 */

	act_pid[5] = sprocsp(activity, PR_SALL, (void*)5, NULL, 8192);
	if (act_pid[5] < 0) {
		perror("sproc: Activity Thread 5");
		exit(1);
	}

	if (frs_enqueue(slave0_frs, act_pid[5], 1, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 5:1");
		exit(1);
	}

	/*
	 * Create and enqueue Activity Thread 6 on minor 3
	 */

	act_pid[6] = sprocsp(activity, PR_SALL, (void*)6, NULL, 8192);
	if (act_pid[6] < 0) {
		perror("sproc: Activity Thread 6");
		exit(1);
	}

	if (frs_enqueue(slave0_frs, act_pid[6], 3, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 6:3");
		exit(1);
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
		exit(1);
	}
	
	/*
	 * Wait for child Activity Threads to complete
	 */

	wait(&wstatus);
	wait(&wstatus);
	wait(&wstatus);
}

void
slave1_controller(void)
{
	int wstatus;
	int processor = 3;
	int disc;

	slave1_frs = frs_create_slave(processor, master_frs);
	if (slave1_frs == NULL) {
		perror("frs_create_slave 1");
		exit(1);
	}

	/*
	 * Create and enqueue Activity Thread 7 on minors 0 through 1
	 */

	act_pid[7] = sprocsp(activity, PR_SALL, (void*)7, NULL, 8192);
	if (act_pid[7] < 0) {
		perror("sproc: Activity Thread 7");
		exit(1);
	}

	disc = FRS_DISC_RT | FRS_DISC_OVERRUNNABLE;
	if (frs_enqueue(slave1_frs, act_pid[7], 0, disc) < 0) {
		perror("frs_enqueue slave1 7:0");
		exit(1);
	}

	disc = FRS_DISC_RT;
	if (frs_enqueue(slave1_frs, act_pid[7], 1, disc) < 0) {
		perror("frs_enqueue slave1 7:1");
		exit(1);
	}

	/*
	 * Enqueue Activity Thread 7 on minor 2
	 */

	disc = FRS_DISC_RT;
	if (frs_enqueue(slave1_frs, act_pid[7], 2, disc) < 0) {
		perror("frs_enqueue slave1 7:2");
		exit(1);
	}

	/*
	 * Enqueue Activity Thread 7 on minors 3 through 5
	 */

	disc = FRS_DISC_RT | FRS_DISC_OVERRUNNABLE;
	if (frs_enqueue(slave1_frs, act_pid[7], 3, disc) < 0) {
		perror("frs_enqueue slave1 7:3");
		exit(1);
	}

	disc = FRS_DISC_RT | FRS_DISC_OVERRUNNABLE;
	if (frs_enqueue(slave1_frs, act_pid[7], 4, disc) < 0) {
		perror("frs_enqueue slave1 7:4");
		exit(1);
	}

	disc = FRS_DISC_RT;
	if (frs_enqueue(slave1_frs, act_pid[7], 5, disc) < 0) {
		perror("frs_enqueue slave1 7:5");
		exit(1);
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
		exit(1);
	}
	
	/*
	 * Wait for child Activity Thread to complete
	 */

	wait(&wstatus);
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
		exit(1);
	}

	fprintf(stderr,
		"[Activity:%d (%d)]: Joined Frame Scheduler\n", act_id, pid);
    
	while (1) {
		int i;

		for (i=0; i<local_spin; i++)
			j += i+10;
		
		if ((previous_minor = frs_yield()) < 0) {
			perror("frs_yield");
			exit(1);
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
		perror("[setup_signals]: underrun");
		exit(1);
	}

	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("[setup_signals]: overrun");
		exit(1);
	}
}

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
