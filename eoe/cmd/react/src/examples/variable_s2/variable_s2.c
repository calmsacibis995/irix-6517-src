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

/*
 * Example: variable_s2
 *
 * Frame schedulers: 1 Master
 *
 * Sources: user cputimer
 *
 * Minor Frames: Variable length
 *
 * Thread Model: sproc
 */

/*
 *  |<---300-->|<---300-->|<-------600------->|<-200->|<-200->|<-200->|
 *  |          |          |                   |       |       |       |
 *  |    0     |     1    |         2         |   3   |   4   |   5   |
 *  |          |          |                   |       |       |       |
 *  |   At0    |   At1    |        At2        |   At0 |  At1  |  At2  |
 *  |          |          |                   |       |       |       |
 *  user       timer      user                user    timer   timer
 *
 */

#define N_MINORS 6

frs_intr_info_t intr_info[N_MINORS] = {
	{FRS_INTRSOURCE_USER,          0},
	{FRS_INTRSOURCE_CPUTIMER, 300000},
	{FRS_INTRSOURCE_USER,          0},
	{FRS_INTRSOURCE_USER,          0},
	{FRS_INTRSOURCE_CPUTIMER, 200000},
	{FRS_INTRSOURCE_CPUTIMER, 200000}
};

frs_t* frs;
sem_t enqueue_sem;
int activity0_done = 0;

void activity(int);
void setup_signals(void);
void overrun_error(void);
void underrun_error(void);
void sequence_error(void);
void strobe(void);

main(int argc, char** argv)
{
	pid_t act_pid;
	int wstatus;
        int parse;
	
	int processor = 1;

	while ((parse = getopt(argc, argv, "p:")) != EOF)
		switch (parse) {
		case 'p':
			processor = atoi(optarg);
			break;
		}

        sem_init(&enqueue_sem, 1, 0);

	/*
	 * Create FRS
	 */

	frs = frs_create_vmaster(processor, N_MINORS, 0, intr_info);
	if (frs == NULL) {
		perror("frs_create_vmaster");
		exit(1);
	}

	/*
	 * Create and enqueue Activity Thread 0 on minor 0 & minor 3
	 */

	if ((act_pid = sproc(activity, PR_SALL, 0)) < 0) {
		perror("sproc: Activity Thread 0");
		exit(1);
	}

	if (frs_enqueue(frs, act_pid, 0, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 0:0");
		exit(1);
	}

	if (frs_enqueue(frs, act_pid, 3, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 0:3");
		exit(1);
	}

	/*
	 * Create and enqueue Activity Thread 1 on minor 1 & minor 4
	 */

	if ((act_pid = sproc(activity, PR_SALL, 1)) < 0) {
		perror("sproc: Activity Thread 1");
		exit(1);
	}

	if (frs_enqueue(frs, act_pid, 1, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 1:1");
		exit(1);
	}

	if (frs_enqueue(frs, act_pid, 4, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 1:1");
		exit(1);
	}

	/*
	 * Create and enqueue activity 2 on minor 2 & minor 5
	 */

	if ((act_pid = sproc(activity, PR_SALL, 2)) < 0) {
		perror("sproc: Activity Thread 2");
		exit(1);
	}

	if (frs_enqueue(frs, act_pid, 2, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 2:2");
		exit(1);
	}

	if (frs_enqueue(frs, act_pid, 5, FRS_DISC_RT) < 0) {
		perror("frs_enqueue 2:5");
		exit(1);
	}

	sem_post(&enqueue_sem);
	sem_post(&enqueue_sem);
	sem_post(&enqueue_sem);

	setup_signals();
        
	/*
	 * Start the frame scheduler
	 */

	if (frs_start(frs) < 0) {
		perror("frs_start");
		exit(1);
	}

	/*
	 * Generate interrupts
	 */
	strobe();

	wait(&wstatus);
	wait(&wstatus);
	wait(&wstatus);

	exit(0);
}

void
activity(int act_id)
{
	int counter = 20;
	int previous_minor;
	int my_pid = getpid();

        sem_wait(&enqueue_sem);

	/*
	 * Join the frame scheduler
	 */

	if (frs_join(frs) < 0) {
		perror("frs_join");
		exit(1);
	}

	fprintf(stderr,
		"[Activity:%d (%d)]: Joined Frame Scheduler\n",
		act_id, my_pid);

	/*
	 * Execute real-time loop
	 */

	do {
		if ((previous_minor = frs_yield()) < 0) {
			perror("frs_yield");
			exit(1);
		}

		fprintf(stderr,
		"[Activity:%d (%d)] Return from Yield; previous_minor: %d\n",
			act_id, my_pid, previous_minor);

	} while (counter--);

	if (act_id == 0)
		activity0_done = 1;

	fprintf(stderr, "[Activity:%d (%d)] Exiting\n", act_id, my_pid);
	exit(0);
}

void
strobe()
{
	struct timespec sleep_time = {0, 600000000};
	struct sched_param param;

	param.sched_priority = 199;
	if (sched_setscheduler(0, SCHED_FIFO, &param) < 0) {
		perror("sched_setscheduler");
		exit(0);                                           
	}

	/*
	 * Generate user level interrupts
	 */
	while (!activity0_done) {
		nanosleep(&sleep_time, NULL);
		frs_userintr(frs);
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

	if ((int)signal(SIGRTMIN+1, sequence_error) == -1) {
		perror("[setup_signals]: sequence");
		exit(1);
	}
}

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
sequence_error()
{
	fprintf(stderr, "[sequence_error], PID=%d\n", getpid());	
	exit(2);
}	













