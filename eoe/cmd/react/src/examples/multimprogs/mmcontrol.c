/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Multiple synchronous frame schedulers
 * Example using separate programs
 */

/*
 * This example creates N (number_of_slaves) synchronous frame schedulers,
 * and accepts processes forked off independently to be enqueued in
 * the frame queues.
 */

#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include "defs.h"
#include "ipc.h"

#define PERIOD 1000000

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
	fprintf(stderr, "[underrun_error], MASTER-PID=%d\n", getpid());	
	exit(2);
}

void
overrun_error()
{
	if ((int)signal(SIGUSR2, overrun_error) == -1) {
		perror("[overrun_error]: Error while resetting signal");
		exit(1);
	}
	fprintf(stderr, "[overrun_error], MASTER-PID=%d\n", getpid());	
	exit(2);
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

int
create_process(void (*f)(void*, void*, void*, void*),
               void* arg1,
               void* arg2,
               void* arg3,
               void* arg4)
{
        int pid;
        switch ((pid = fork())) {
              case 0:
                /* child process */
                (*f)(arg1, arg2, arg3, arg4);
              case -1:
                perror("[create_process]: fork() failed");
                exit(1);
              default:
                return (pid);
        }
}

void
frs_show(frs_t* frs)
{
        if (frs == 0) {
                printf("Trying to show NULL frs!!!\n");
                return;
        }
        printf("Frame Scheduler at [0x%x]:\n", frs);
        printf("cpu: %d, intr_source: %d, intr_qual: %d\n",
               frs->frs_info.cpu,
               frs->frs_info.intr_source,
               frs->frs_info.intr_qualifier);
        printf("minors: %d, sync_master_process: %d\n",
               frs->frs_info.n_minors,
               frs->frs_info.syncm_controller);
}

               
void
create_slave_frs(void* arg1, void* arg2, void* arg3, void* arg4)
{
        int slave_processor = (int)arg1;
        frs_t* master_frs = (frs_t*)arg2;
        int number_of_processes = (int)arg3;
        frs_t* slave_frs;

        assert(slave_processor > 0);
        assert(master_frs != 0);
        assert(number_of_processes > 0);
        assert(arg4 == 0);

        printf("Creating slave frame scheduler on processor [%d], number of processes: %d\n",
               slave_processor, number_of_processes);

        frs_show(master_frs);

        slave_frs = frs_create_slave(slave_processor, master_frs);
        if (slave_frs == NULL) {
                perror("frs_create_slave");
                fprintf(stderr, "Failed creating slave frs on processor %d\n", slave_processor);
                exit(1);
        }

        /*
         * Now we can start accepting enqueueing requests
         */
        enqueue_server(slave_frs, number_of_processes);

	/*
	 * At this point all processes have been enqueued
	 * We can start this slave frame scheduler
	 */
				
	if (frs_start(slave_frs) < 0) {
		syserr("frs_start");
		exit(1);
	}


	/*
	 * The process' job is done; we'll
	 * just pause and wait for a signal
	 * to wake us up and exit.
	 */

	printf("Process Hosting frs [%d] is  Pausing\n", frs_getcpu(slave_frs));
	pause();
	printf("Process Hosting frs [%d] has  Waken Up -- Exiting\n", frs_getcpu(slave_frs));
	        
}        


main(int argc, char** argv)
{
        int i;
        int errcode;
        frs_t* master_frs;
        int master_processor;
        int master_number_of_processes;
        int number_of_slaves;
        int slave_processors[MAX_NUMBER_OF_SLAVES];
        int slave_number_of_processes[MAX_NUMBER_OF_SLAVES];
	char* command;


	/*
	 * Usage: simple [cpu_number [cpu_number...]]
	 */
        
        /*
         * Extract command name for error messages
         */
	command = *argv++;
        argc--;
        
        /*
         * Set defaults
         */
	number_of_slaves = 0;
        master_processor = 1;
        master_number_of_processes = 2;

        /*
         * The first argument is the master_processor
         */
        if (argc-- > 0) {
                master_processor = atoi(*argv++);
        }

        /*
         * The rest of the arguments are slave processors
         */
        while (argc-- > 0) {
                slave_processors[number_of_slaves] = atoi(*argv++);
                slave_number_of_processes[number_of_slaves] = 2;
                number_of_slaves++;
                if (number_of_slaves >= MAX_NUMBER_OF_SLAVES) {
                        fprintf(stderr, "Too many slaves\n");
                        exit(1);
                }
        }

        printf("Running Synchronous Frame Schedulers [Master + %d Slaves]:\n", number_of_slaves);
        printf("Master Processor: %d, Number of Processes: %d\n",
               master_processor,
               master_number_of_processes);
        for (i = 0; i < number_of_slaves; i++) {
                printf("Slave Processor: %d, Number of Processes: %d\n",
                       slave_processors[i], slave_number_of_processes[i]);
        }
        printf("\n");

	/*
	 * Initialize signals to catch
	 * termination signals and underrun, overrun errors.
	 */
	
	setup_signals();

        /*
         * We use this process to host the master frame scheduler
         */

	/*
	 * cpu = master_processor
	 * interrupt source = CCTIMER
	 * period = 600 [ms] == 600000 [microseconds] 
	 * number of minors = 4
	 * slave list = slave_processors
	 * number of slaves = number_of_slaves
	 */

	master_frs = frs_create_master(master_processor,
                                       FRS_INTRSOURCE_CCTIMER,
                                       PERIOD,
                                       4,
                                       number_of_slaves);
        if (master_frs == NULL) {
                perror("frs_create_master");
                fprintf(stderr, "Failed creating master frame scheduler\n");
                exit(1);
        }

        /*
         * Forking off one process per slave
         */

        for (i = 0; i < number_of_slaves; i++) {
                errcode = create_process(create_slave_frs,
                                         (void*)slave_processors[i],
                                         (void*)master_frs,
                                         (void*)slave_number_of_processes[i],
                                         (void*)0);
                if (errcode < 0) {
                        perror("create_process");
                        fprintf(stderr, "Failed creating process for slave frs");
                        exit(1);
                }
        }

        /*
         * All the slaves are done,
         * let the master-sync frs enqueue its processes.
         */

        enqueue_server(master_frs, master_number_of_processes);

	/*
	 * At this point all processes have been enqueued
	 * We can start the frame scheduler
	 */
				
	if (frs_start(master_frs) < 0) {
		syserr("frs_start");
		exit(1);
	}

	/*
	 * The master's job is done; we'll
	 * just pause and wait for a signal
	 * to wake us up and exit.
	 */

	printf("Master Process Pausing\n");
	pause();
	printf("Master Process Waken Up -- Exiting\n");
	
	exit(0);
}

	













