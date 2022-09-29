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
 * Frame Scheduler -- implementation 
 * Example II
 */

#include "multi.h"

/*
 * This example creates 3 synchronous frame schedulers
 *
 * Specification for FRS1
 * cpu: 1
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 20
 * period (intr_qualifier): 50 [ms]
 * processes: A@20Hz, K1@BACK
 *
 * Specification for FRS2
 * cpu: 2
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 20
 * period (intr_qualifier): 50 [ms]
 * processes: B@10Hz, C@5Hz, K2@BACK
 *
 * Specification for FRS3
 * cpu: 3
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 20
 * period (intr_qualifier): 50 [ms]
 * processes: D@20Hz, E@4Hz, F@2Hz, K3@BACK
 *
 */

/*
 * Globally visible reference to master FRS
 * We need it to create the slave frs's.
 */

frs_t* master_frs = 0;

/*
 * Globally visible semaphore to let
 * everybody know when the master FRS has
 * been created.
 */

usema_t* master_frs_created = 0;

/*
 * Barrier to synchronize enqueues and joins.
 * We need to block before each join and before each start.
 * Since there will be 9 joins (one join per process) and
 * 3 starts, the number of waiters for the barrier is 12 (9 + 3)
 */
barrier_t* all_procs_enqueued = 0;
 

/*
 * Management of pdesc_t objects
 */

pdesc_t*
pdesc_create(frs_t* frs, uint messages, int (*action_function)())
{
	pdesc_t* pdesc;
	
	assert(frs != NULL);
	assert(action_function != NULL);

	pdesc = (pdesc_t*)malloc(sizeof(pdesc_t));
	if (pdesc == NULL) {
		perror("[pdesc_create]: malloc");
		exit(1);
	}

	pdesc->frs = frs;
	pdesc->messages = messages;
	pdesc->action_function = action_function;

        return (pdesc);
}

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
process_skeleton(void* arg)
{
	int previous_minor;
	int rv;
	int pid;
	int cpu;
	pdesc_t* pdesc;
	
	pdesc = (pdesc_t*)arg;
	assert(pdesc != NULL);

	pid = getpid();
	cpu = pdesc->frs->frs_info.cpu;
	
	/*
	 * We join the frs after making sure
         * everybody has been enqueued.
	 */

        barrier(all_procs_enqueued, 12);
        
	if (frs_join(pdesc->frs) < 0) {
		fprintf(stderr, "[process_skeleton]: Cpu [%d] Process [%d] frs_join failed\n",
			cpu, pid);
		perror("[process_skeleton]");
		exit(1);
	}

	if (pdesc->messages & PDESC_MSG_JOIN) {
		fprintf(stderr, "[process_skeleton] Cpu [%d] Process [%d]: Joined Frame Scheduler\n",
			cpu, pid);
	}


	/*
	 * This is the real-time loop. The first iteration
	 * is done right after returning from the join
	 */

	rv = 0;
    
	do {
		/*
		 * We call the routine that executes the rt code
		 */

		rv = (*pdesc->action_function)();
		
		/*
		 * After we are done with our computations, we
		 * yield the cpu. THe yield call will not return until
		 * it's our turn to execute again.
		 */

		if ((previous_minor = frs_yield()) < 0) {
			fprintf(stderr, "[process_skeleton]: Cpu [%d] Process [%d] frs_yield failed\n",
				cpu, pid);
			perror("[process_skeleton]");
			exit(1);
		}

		if (pdesc->messages & PDESC_MSG_YIELD) {
			fprintf(stderr, "[process_skeleton] Cpu [%d] Process [%d]: Return from Yield; pm: %d\n",
				cpu, pid, previous_minor);
		}
		
	} while (!rv);

    
	fprintf(stderr, "[process_skeleton]: Cpu [%d] Process [%d]: Exiting\n",
		cpu, pid);
	exit(0);
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
	if ((int)signal(SIGHUP, termination_signal) == -1) {
		perror("[setup_signals]: Error while setting termination signal");
		exit(1);
	}
}

void
process_for_master_frs(void* arg)
{
	frs_t* frs;
	pdesc_t* pdesc;
	int pid;
	int minor;
	int wstatus;
	
	/*
	 * Create the master frame scheduler
	 * cpu = 1
	 * interrupt source = CCTIMER
	 * number of minors = 20
	 * slaves = 2, 3
	 * number_of_slaves = 2
	 * period (intr_qualifier) = 50 [ms] == 50000 [microseconds]
	 */

	if ((frs = frs_create_master(1, FRS_INTRSOURCE_CCTIMER, 50000, 20, 2)) == NULL) {
		perror("[main]: frs_create_master failed");
		exit(1);
	}

	/*
	 * Signal the main process that the
	 * master frs has been created
	 */
	assert(master_frs_created != 0);
	master_frs = frs;
	sema_v(master_frs_created);

	/*
	 * Process A
	 * frs: 0 (on cpu 1)
	 * freq: 20Hz
	 */

	pdesc = pdesc_create(frs, 0 , processA_loop);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processA");
		exit(1);
	}
	for (minor = 0; minor < 20; minor++) {
		if (frs_enqueue(frs, pid, minor, FRS_DISC_RT) < 0) {
			perror("[frs_enqueue]: processA");
			exit(1);
		}
	}

	/*
	 * Process K1
	 * frs: 0 (on cpu 1)
	 * freq: BACKG
	 */

	pdesc = pdesc_create(frs,  PDESC_MSG_YIELD, processK1_loop);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processK1");
		exit(1);
	}
	for (minor = 0; minor < 20; minor++) {
		int disc = FRS_DISC_RT |
		           FRS_DISC_UNDERRUNNABLE |
			   FRS_DISC_OVERRUNNABLE |
			   FRS_DISC_CONT;
		if (frs_enqueue(frs, pid, minor, disc) < 0) {
			perror("[frs_enqueue]: processK1");
			exit(1);
		}
	}

        barrier(all_procs_enqueued, 12);
        
	/*
	 * We now start this Frame Scheduler
	 */
	
	if (frs_start(frs) < 0) {
		fprintf(stderr, "[frs_start]: Cpu: [%d] MasterProcess: [%d]\n",
			frs->frs_info.cpu, getpid());
		perror("[frs_start]");
		exit(1);
	}


	
       /*
	* Wait for the two processes to finish
	* The actual exit will happen from the
	* termination signal handler.
	*/
	wait(&wstatus);
	wait(&wstatus);

	exit(0);
}


void
process_for_first_slave_frs(void* mfrs_arg)
{
	frs_t* frs;
	pdesc_t* pdesc;
	int pid;
	int minor;
	int subminor;
	int wstatus;

	/*
	 * Create first slave
	 * cpu = 2
	 * interrupt source = CCTIMER
	 * number of minors = 20
	 * slaves = --
	 * number_of_slaves = --
	 * period (intr_qualifier) = 50 [ms] == 50000 [microseconds]
	 */

	if ((frs = frs_create_slave(2, (frs_t*)mfrs_arg)) == NULL) {
		perror("[main]: frs_create_slave on cpu 2 failed");
		exit(1);
	}

	/*
	 * Process B
	 * frs: 1 (on cpu 2)
	 * freq: 10Hz
	 */

	pdesc = pdesc_create(frs, 0, processB_loop);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processB");
		exit(1);
	}

	for (minor = 0; minor < 20; minor += 2) {
		int disc = FRS_DISC_RT |
		           FRS_DISC_UNDERRUNNABLE |
			   FRS_DISC_OVERRUNNABLE |
			   FRS_DISC_CONT;
		if (frs_enqueue(frs, pid, minor, disc) < 0) {
			perror("[frs_enqueue]: processB");
			exit(1);
		}
		if (frs_enqueue(frs, pid, minor + 1, FRS_DISC_RT) < 0) {
			perror("[frs_enqueue]: processB");
			exit(1);
		}
	}
        
	/*
	 * Process C
	 * frs: 1 (on cpu 2)
	 * freq: 5Hz
	 */

	pdesc = pdesc_create(frs, 0, processC_loop);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processC");
		exit(1);
	}

	for (minor = 0; minor < 20; minor += 4) {
		int disc = FRS_DISC_RT |
		           FRS_DISC_UNDERRUNNABLE |
			   FRS_DISC_OVERRUNNABLE |
			   FRS_DISC_CONT;
		for (subminor = minor; subminor < (minor + 3); subminor++) {
			if (frs_enqueue(frs, pid, subminor, disc) < 0) {
				perror("[frs_enqueue]: processC");
				exit(1);
			}
		}
		if (frs_enqueue(frs, pid, minor + 3, FRS_DISC_RT) < 0) {
			perror("[frs_enqueue]: processC");
			exit(1);
		}
	}

	/*
	 * Process K2
	 * frs: 1 (on cpu 2)
	 * freq: BACKG
	 */

	pdesc = pdesc_create(frs,  PDESC_MSG_YIELD, processK2_loop);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processK2");
		exit(1);
	}
	for (minor = 0; minor < 20; minor++) {
		int disc = FRS_DISC_RT |
		           FRS_DISC_UNDERRUNNABLE |
			   FRS_DISC_OVERRUNNABLE |
			   FRS_DISC_CONT;
		if (frs_enqueue(frs, pid, minor, disc) < 0) {
			perror("[frs_enqueue]: processK2");
			exit(1);
		}
	}

        barrier(all_procs_enqueued, 12);

	/*
	 * We now start this Frame Scheduler
	 */
	
	if (frs_start(frs) < 0) {
		fprintf(stderr, "[frs_start]: Cpu: [%d] MasterProcess: [%d]\n",
			frs->frs_info.cpu, getpid());
		perror("[frs_start]");
		exit(1);
	}


	/*
	 * Wait for the three processes to finish
	 * The actual exit will happen from the
	 * termination signal handler.
	 */
	wait(&wstatus);
	wait(&wstatus);
	wait(&wstatus);

	exit(0);
}

void
process_for_second_slave_frs(void* mfrs_arg)
{
	frs_t* frs;
	pdesc_t* pdesc;
	int pid;
	int minor;
	int subminor;
	int wstatus;


	/*
	 * Create second slave
	 * cpu = 3
	 * interrupt source = CCTIMER
	 * number of minors = 20
	 * slaves = --
	 * number_of_slaves = --
	 * period (intr_qualifier) = 50 [ms] == 50000 [microseconds]
	 */

	frs = (frs_t*)mfrs_arg;
	printf("master_controler: %d\n", frs->master_controller);
	printf("cpu: %d\n", frs->frs_info.cpu);
	printf("syncm_controller: %d\n", frs->frs_info.syncm_controller);

	
	if ((frs = frs_create_slave(3, (frs_t*)mfrs_arg)) == NULL) {
		perror("[main]: frs_create_slave on cpu 3 failed");
		exit(1);
	}

	/*
	 * Process D
	 * frs: 2 (on cpu 3)
	 * freq: 20Hz
	 */

	pdesc = pdesc_create(frs, 0, processD_loop);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processD");
		exit(1);
	}

	for (minor = 0; minor < 20; minor++) {
		if (frs_enqueue(frs, pid, minor, FRS_DISC_RT) < 0) {
			perror("[frs_enqueue]: processD");
			exit(1);
		}
	}

	/*
	 * Process E
	 * frs: 2 (on cpu 3)
	 * freq: 4Hz
	 */

	pdesc = pdesc_create(frs, 0, processE_loop);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processE");
		exit(1);
	}

	for (minor = 0; minor < 20; minor += 5) {
		int disc = FRS_DISC_RT |
		           FRS_DISC_UNDERRUNNABLE |
			   FRS_DISC_OVERRUNNABLE |
			   FRS_DISC_CONT;
		for (subminor = minor; subminor < (minor + 4); subminor++) {
			if (frs_enqueue(frs, pid, subminor, disc) < 0) {
				perror("[frs_enqueue]: processE");
				exit(1);
			}
		}
		if (frs_enqueue(frs, pid, minor + 4, FRS_DISC_RT) < 0) {
			perror("[frs_enqueue]: processE");
			exit(1);
		}
	}

	/*
	 * Process F
	 * frs: 2 (on cpu 3)
	 * freq: 2Hz
	 */

	pdesc = pdesc_create(frs, 0, processF_loop);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processF");
		exit(1);
	}

	for (minor = 0; minor < 20; minor += 10) {
		int disc = FRS_DISC_RT |
		           FRS_DISC_UNDERRUNNABLE |
			   FRS_DISC_OVERRUNNABLE |
			   FRS_DISC_CONT;
		for (subminor = minor; subminor < (minor + 9); subminor++) {
			if (frs_enqueue(frs, pid, subminor, disc) < 0) {
				perror("[frs_enqueue]: processF");
				exit(1);
			}
		}
		if (frs_enqueue(frs, pid, minor + 9, FRS_DISC_RT) < 0) {
			perror("[frs_enqueue]: processF");
			exit(1);
		}
	}

	
	/*
	 * Process K3
	 * frs: 2 (on cpu 3)
	 * freq: BACKG
	 */

	pdesc = pdesc_create(frs,  PDESC_MSG_YIELD, processK3_loop);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processK3");
		exit(1);
	}
	for (minor = 0; minor < 20; minor++) {
		int disc = FRS_DISC_RT |
		           FRS_DISC_UNDERRUNNABLE |
			   FRS_DISC_OVERRUNNABLE |
			   FRS_DISC_CONT;
		if (frs_enqueue(frs, pid, minor, disc) < 0) {
			perror("[frs_enqueue]: processK3");
			exit(1);
		}
	}

        barrier(all_procs_enqueued, 12);
        
	/*
	 * We now start this Frame Scheduler
	 */

        
	if (frs_start(frs) < 0) {
		fprintf(stderr, "[frs_start]: Cpu: [%d] MasterProcess: [%d]\n",
			frs->frs_info.cpu, getpid());
		perror("[frs_start]");
		exit(1);
	}

       /*
	* Wait for the four processes to finish
	* The actual exit will happen from the
	* termination signal handler.
	*/
	wait(&wstatus);
	wait(&wstatus);
	wait(&wstatus);
	wait(&wstatus);

	exit(0);
}	

main(int argc, char** argv)
{
	int rv;
	int wstatus;
	frs_t master_frs1;
	frs_t master_frs2;
	
	printf("Running Frame Scheduler on Processors 1, 2, 3\n");

	/*
	 * Initialization of semaphores
	 */

	sema_init();
	master_frs_created = sema_create();
        all_procs_enqueued = barrier_create();
	
	/*
	 * Initialize signals to catch
	 * termination signals and underrun, overrun errors.
	 */
	
	setup_signals();
	

	/*
	 * Create the master processes that will
	 * create the frame schedulers
	 */

	rv = sproc(process_for_master_frs, PR_SALL, NULL);
	if (rv == -1) {
		perror("[sproc]-master_frs");
		exit(1);
	}

	/*
	 * Wait for the master FRs to be created
	 */

	sema_p(master_frs_created);
	assert(master_frs != 0);

	printf("The master has been created\n");

	master_frs1 = *master_frs;
	master_frs2 = *master_frs;
	
	/*
	 * The master has been created,
	 * we can proceed to create the slaves
	 */
	
	rv = sproc(process_for_first_slave_frs, PR_SALL, (void*)&master_frs1);
	if (rv == -1) {
		perror("[sproc]-first_slave_frs");
		exit(1);
	}

	
	rv = sproc(process_for_second_slave_frs, PR_SALL, (void*)&master_frs2);
	if (rv == -1) {
		perror("[sproc]-second_slave_frs");
		exit(1);
	}

	/*
	 * Wait for each of the master processes
	 */

	wait(&wstatus);
	wait(&wstatus);
	wait(&wstatus);
	
	exit(0);
}

	













