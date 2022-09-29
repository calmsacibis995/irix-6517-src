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
 * Frame Scheduler Test
 */

#include <sys/types.h>
#include <sys/frs.h>
#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include <stdlib.h>
#include <task.h> 
#define WD    10
#define WC    50
#define WB    50
#define WA1   30

#define JOINM
#define YIELDM

int nloops = 20;
int WA = 30;
int justone = 1;

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
processD(int cpu)
{
    int counter = nloops;
    double res = 2;
    int i;
    int minor;

    if ((minor = schedctl(MPTS_FRS_JOIN, cpu)) <0) {
	perror("[processA]: Error when JOINING");
	exit(1);
    }

#ifdef JOINM
    printf("ProcessD Joined - Doing first Activation, minor=%d\n", minor);
#endif    
    do {
	for (i = 0; i < WD; i++) {
	    res = res * log(res) - res * sqrt(res);
	}

	if ((minor = schedctl(MPTS_FRS_YIELD)) < 0) {
	    perror("[ProcessA]: Error when yielding");
	    exit(1);
	}

#ifdef YIELDM	
	printf("ProcessD returning from Yield, minor=%d\n", minor);
#endif
	
    } while (counter--);

    printf("ProcessD is Exiting\n");
    
    exit(0);
}

    
void
processC(int cpu)
{
    int counter = nloops;
    double res = 2;
    int i;
    int minor;

    if ((minor = schedctl(MPTS_FRS_JOIN, cpu)) <0) {
	perror("[processC]: Error when JOINING");
	exit(1);
    }
#ifdef JOINM
    printf("ProcessC Joined - Doing first Activation, minor=%d\n", minor);
#endif    
    do {
	for (i = 0; i < WC; i++) {
	    res = res * log(res) - res * sqrt(res);
	}

	if ((minor = schedctl(MPTS_FRS_YIELD)) < 0) {
	    perror("[ProcessC]: Error when yielding");
	    exit(1);
	}

#ifdef YIELDM	
	printf("ProcessC returning from Yield, minor=%d\n", minor);
#endif	

    } while (counter--);

    printf("ProcessC is Exiting\n");
    
    exit(0);
}
 
void
processB(int cpu)
{
    int counter = nloops;
    double res = 2;
    int i;
    int minor;

    if ((minor = schedctl(MPTS_FRS_JOIN, cpu)) <0) {
	perror("[processB]: Error when JOINING");
	exit(1);
    }
#ifdef JOINM
    printf("ProcessB Joined - Doing first Activation, minor=%d\n", minor);
#endif    
    do {
	for (i = 0; i < WB; i++) {
	    res = res * log(res) - res * sqrt(res);
	}

	if ((minor = schedctl(MPTS_FRS_YIELD)) < 0) {
	    perror("[ProcessB]: Error when yielding");
	    exit(1);
	}

#ifdef YIELDM	
	printf("ProcessB returning from Yield, minor=%d\n", minor); 
#endif	
    } while (counter--);

    printf("ProcessB is Exiting\n");
    
    exit(0);
}

void
processA(int cpu)
{
    int counter = nloops;
    double res = 2;
    int i;
    int minor;

    if ((minor = schedctl(MPTS_FRS_JOIN, cpu)) <0) {
	perror("[processA]: Error when JOINING");
	exit(1);
    }
#ifdef JOINM
    printf("ProcessA Joined - Doing first Activation, minor=%d\n", minor);
#endif    
    do {
	for (i = 0; i < WA; i++) {
	    res = res * log(res) - res * sqrt(res);
	}

	if ((minor = schedctl(MPTS_FRS_YIELD)) < 0) {
	    perror("[ProcessA]: Error when yielding");
	    exit(1);
	}

#ifdef YIELDM	
	printf("ProcessA returning from Yield, minor=%d\n", minor);
#endif	
    } while (counter--);

    printf("Destroying frs from ProcessA\n");

    if (schedctl(MPTS_FRS_DESTROY, cpu) < 0) {
	printf("Error when trying to destroy frs\n");
	perror("schedctl MPTS_FRS_DESTROY");
	exit(1);
    }
    
    printf("ProcessA is Exiting\n");
    
    exit(0);
}

void
processA1(int cpu)
{
    int counter = nloops;
    double res = 2;
    int i;
    int minor;

    if ((minor = schedctl(MPTS_FRS_JOIN, cpu)) <0) {
	perror("[processA1]: Error when JOINING");
	exit(1);
    }
#ifdef JOINM
    printf("ProcessA1 Joined - Doing first Activation, minor=%d\n", minor);
#endif    
    do {
	for (i = 0; i < WA1; i++) {
	    res = res * log(res) - res * sqrt(res);
	}

	if ((minor = schedctl(MPTS_FRS_YIELD)) < 0) {
	    perror("[ProcessA1]: Error when yielding");
	    exit(1);
	}
#ifdef YIELDM
	printf("ProcessA1 returning from Yield, minor=%d\n", minor);
#endif	
	
    } while (counter--);

    printf("Destroying frs\n");

    if (schedctl(MPTS_FRS_DESTROY, cpu) < 0) {
	printf("Error when trying to destroy frs\n");
	perror("schedctl MPTS_FRS_DESTROY");
	exit(1);
    }
    
    printf("ProcessA1 is Exiting\n");
    
    exit(0);
}

void
create_frs(int cpu, int master_pid)
{
    int pid;
    frs_fsched_info_t frs_fsched_info;
    
    if ((int)signal(SIGUSR1, underrun_error) == -1) {
	perror("[create_frs]: Error while setting underrun_error signal");
	exit(1);
    }

    if ((int)signal(SIGUSR2, overrun_error) == -1) {
	perror("[create_frs]: Error while setting overrun_error signal");
	exit(1);
    }
    if ((int)signal(SIGHUP, termination_signal) == -1) {
	perror("[create_frs]: Error while setting termination signal");
	exit(1);
    }


    if (cpu == 1) {
	assert(master_pid == 0);
	frs_fsched_info.cpu = cpu;
	frs_fsched_info.intr_source = FRS_INTRSOURCE_EXTINTR;
	frs_fsched_info.intr_qualifier = 0;
	frs_fsched_info.n_minors = 3;
	frs_fsched_info.syncm_controller = FRS_SYNC_MASTER;
	if (justone) {
		frs_fsched_info.num_slaves = 0;
	} else {
		frs_fsched_info.num_slaves = 2;
	}

    } else {
	assert(master_pid != 0);
	frs_fsched_info.cpu = cpu;
	frs_fsched_info.intr_source = FRS_INTRSOURCE_EXTINTR;
	frs_fsched_info.intr_qualifier = 0;	
	frs_fsched_info.n_minors = 3;
	frs_fsched_info.syncm_controller = master_pid;
	frs_fsched_info.num_slaves = 0;
    }

    if (schedctl(MPTS_FRS_CREATE, &frs_fsched_info) < 0) {
	perror("[create_frs]: Error while Creating FRS");
	exit(1);
    }

    printf("User: FRS created on cpu %d\n", cpu);

    
    switch ((pid = fork())) {
      case 0:
	/* child process */
	processA(cpu);
	exit(0);
      case -1:
	perror("[create_frs]: Error while forking processA");
	exit(1);
      default:
	{
	    frs_queue_info_t frs_queue_info;
	    
	    frs_queue_info.controller = get_pid();
	    frs_queue_info.thread = pid;
	    frs_queue_info.minor_index = 0;
	    frs_queue_info.discipline = FRS_DISC_RT;
	    
	    if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) <0) {
		perror("[processA]: Error when enqueing");
		exit(1);
	    }

	    printf("ProcessA enqueued in QUEUE 0\n");

	    frs_queue_info.controller = get_pid();	    
	    frs_queue_info.thread = pid;
	    frs_queue_info.minor_index = 1;
	    frs_queue_info.discipline = FRS_DISC_RT;
	    
	    if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) <0) {
		perror("[processA]: Error when enqueing");
		exit(1);
	    }

	    printf("ProcessA enqueued in  QUEUE 1 \n");
	    frs_queue_info.controller = get_pid();	    
	    frs_queue_info.thread = pid;
	    frs_queue_info.minor_index = 2;
	    frs_queue_info.discipline = FRS_DISC_RT;
	    
	    if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) <0) {
		perror("[processA]: Error when enqueing");
		exit(1);
	    }

	    printf("ProcessA enqueued in  QUEUE 2 \n");

	}
    }

    switch ((pid = fork())) {
      case 0:
	/* child process */
	processA1(cpu);
	exit(0);
      case -1:
	perror("[create_frs]: Error while forking processA1");
	exit(1);
      default:
	{
	    frs_queue_info_t frs_queue_info;

	    frs_queue_info.controller = get_pid();
	    frs_queue_info.thread = pid;
	    frs_queue_info.minor_index = 0;
	    frs_queue_info.discipline = FRS_DISC_RT;
	    
	    if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) <0) {
		perror("[processA1]: Error when enqueing");
		exit(1);
	    }

	    printf("ProcessA1 enqueued \n");
	}
    }

   switch ((pid = fork())) {
      case 0:
	/* child process */
	processB(cpu);
	exit(0);
      case -1:
	perror("[create_frs]: Error while forking processB");
	exit(1);
      default:
	{
	    frs_queue_info_t frs_queue_info;

	    frs_queue_info.controller = get_pid();
	    frs_queue_info.thread = pid;
	    frs_queue_info.minor_index = 1;
	    frs_queue_info.discipline = FRS_DISC_RT;
	    
	    if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) <0) {
		perror("[processB]: Error when enqueing");
		exit(1);
	    }

	    printf("ProcessB enqueued \n");
	}
    }

    switch ((pid = fork())) {
      case 0:
	/* child process */
	processC(cpu);
	exit(0);
      case -1:
	perror("[create_frs]: Error while forking processC");
	exit(1);
      default:
	{
	    frs_queue_info_t frs_queue_info;
	    
	    frs_queue_info.controller = get_pid();
	    frs_queue_info.thread = pid;
	    frs_queue_info.minor_index = 2;
	    frs_queue_info.discipline = FRS_DISC_RT ;
	    
	    if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) <0) {
		perror("[processC]: Error when enqueing");
		exit(1);
	    }

	    printf("ProcessC enqueued \n");
	}
    }


    switch ((pid = fork())) {
      case 0:
	/* child process */
	processD(cpu);
	exit(0);
      case -1:
	perror("[create_frs]: Error while forking processD");
	exit(1);
      default:
	{
	    frs_queue_info_t frs_queue_info;
	    
	    frs_queue_info.controller = get_pid();
	    frs_queue_info.thread = pid;
	    frs_queue_info.minor_index = 0;
	    frs_queue_info.discipline =
	      FRS_DISC_RT | FRS_DISC_CONT |
		FRS_DISC_OVERRUNNABLE | FRS_DISC_UNDERRUNNABLE;
	    
	    if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) <0) {
		perror("[processD]: Error when enqueing");
		exit(1);
	    }

	    printf("ProcessD enqueued on QUEUE 0\n");


	    frs_queue_info.controller = get_pid();
	    frs_queue_info.thread = pid;
	    frs_queue_info.minor_index = 1;
	    frs_queue_info.discipline =
	      FRS_DISC_RT | FRS_DISC_CONT |
		FRS_DISC_OVERRUNNABLE | FRS_DISC_UNDERRUNNABLE;
	    
	    if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) <0) {
		perror("[processD]: Error when enqueing");
		exit(1);
	    }

	    printf("ProcessD enqueued on QUEUE 1\n");

	    frs_queue_info.controller = get_pid();
	    frs_queue_info.thread = pid;
	    frs_queue_info.minor_index = 2;
	    frs_queue_info.discipline = FRS_DISC_RT;
	    
	    if (schedctl(MPTS_FRS_ENQUEUE, &frs_queue_info) <0) {
		perror("[processD]: Error when enqueing");
		exit(1);
	    }

	    printf("ProcessD enqueued on QUEUE 2\n");

	}
    }
    
    /* master process */

#ifdef WAITFORJOINS
    printf("Sleeping for 6 seconds\n");
    sleep(6);
#endif
    
    printf("**************************Starting FRS*******************\n");
    if (schedctl(MPTS_FRS_START, cpu) < 0) {
	perror("[create_frs]: Error while starting frs");
	exit(1);
    }

    while (1) {
	printf("Main process is pausing\n");
	pause();
    }
    
    exit(0);
}

int
create_frs_master_process(int cpu, int master_pid)
{
    int pid;
    
    switch (pid = fork()) {
      case 0:
	printf("Created Master Process [%d] for cpu [%d]\n", getpid(), cpu);
	create_frs(cpu, master_pid);
	exit(0);
      case -1:
	printf("Failed Creating Master PRocess for cpu %d\n", cpu);
	exit(1);
      default:
	return (pid);
    }
}

main(int argc, char** argv)
{
    int cpu;
    int sync_master_pid;
    int cestatus;
    int i;
    int errflag = 0;
    char* command;
    int c;

    justone = 1;
    command = argv[0];

    while ((c = getopt(argc, argv, "smv")) != EOF) {
	    switch (c) {
		  case 's':
		    justone = 1;
		    break;
		  case 'm':
		    justone = 0;
		    break;
		  case 'v':
		    WA = 99999999;
		    break;
		  case '?':
		    errflag++;
	    }
    }

    if (errflag) {
	    (void)fprintf(stderr, "usage: %s [-s|-m]\n", command);
	    exit(1);
    }

    if (justone) {
	    create_frs(1, 0);
	    exit(0);
    } else {
	    sync_master_pid = create_frs_master_process(1, 0);
	    create_frs_master_process(2, sync_master_pid);
	    create_frs_master_process(3, sync_master_pid);

	    for (i = 0; i < 3; i++) {
		    wait(&cestatus);
	    }
	    
	    exit(cestatus);
    }
}


        
















