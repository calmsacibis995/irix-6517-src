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
 * Frame Scheduler Tests
 */

#define _BSD_SIGNALS
#include "multi.h"

/*
 * This example creates one or more synchronous
 * frame schedulers, all running the same processes
 * as follows:
 * Background:
 *    K1, K2, K3
 * Realtime:
 *    A,D @ 20 Hz
 *    B @ 10 Hz
 *    C @ 5 Hz
 *    E @ 4 Hz
 *    F @ 2 Hz
 *
 */


/*
 * The amount of time each process
 * requires to execute its loop may
 * be controlled by specifying iterations
 * via the following array:
 */

uint ppnloops[TPROCS] = {
        10,
        20,
        10,
        20,
        30,
        50,
        100,
        1000,
        10000
};

/*
 * Total number of loops per process
 * before exiting.
 */

uint ppxloops[TPROCS] = {
        1000000,
        1000000,
        1000000,
        1000000,
        1000000,
        1000000,
        1000000,
        1000000,
        1000000
};
        
/*
 * Messages are controlled via this array:
 */

uint ppmess[TPROCS] = {
        0x0,
        0x0,
        PDESC_MSG_JOIN,
        0x0,
        PDESC_MSG_JOIN | PDESC_MSG_YIELD,
        PDESC_MSG_JOIN | PDESC_MSG_YIELD,
        PDESC_MSG_JOIN | PDESC_MSG_YIELD,
        PDESC_MSG_JOIN | PDESC_MSG_YIELD, 
        PDESC_MSG_JOIN | PDESC_MSG_YIELD
};

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
 * Barrier to synchronize joins, enqueues, starts.
 * The number of waiters is equal to the total number
 * of processes that are going to be doing a join plus
 * the number of frame schedulers we have to start.
 * (30 = 9*3+3).
 */
barrier_t* all_procs_enqueued = 0;


/*
 * Management of pdesc_t objects
 */

pdesc_t*
pdesc_create(frs_t* frs,
             uint messages,
             int (*action_function)(pdesc_t*),
             uint nloops,
             uint xloops)
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
        pdesc->nloops = nloops;
        pdesc->xloops = xloops;
        pdesc->ccount = 0;
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
	 * First, we join the frame scheduler after
         * all procs have been enqueued.
	 */

        barrier(all_procs_enqueued, 30);
        
	if (frs_join(pdesc->frs) < 0) {
		fprintf(stderr,
                        "[process_skeleton]: Cpu [%d] Process [%d] frs_join failed\n",
			cpu,
                        pid);
		perror("[process_skeleton]");
		exit(1);
	}
    
	if (pdesc->messages & PDESC_MSG_JOIN) {
		fprintf(stderr,
                        "[process_skeleton] Cpu [%d] Process [%d]: Joined FRS\n",
			cpu,
                        pid);
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

		rv = (*pdesc->action_function)(pdesc);
		
		/*
		 * After we are done with our computations, we
		 * yield the cpu. THe yield call will not return until
		 * it's our turn to execute again.
		 */

		if ((previous_minor = frs_yield()) < 0) {
			fprintf(stderr,
                                "[process_skeleton]:Cpu[%d] Process[%d] yield failed\n",
				cpu,
                                pid);
			perror("[process_skeleton]");
			exit(1);
		}

		if (pdesc->messages & PDESC_MSG_YIELD) {
			fprintf(stderr,
                                "[process_skeleton] Cpu[%d] Process[%d]: Yield, PM: %d\n",
				cpu,
                                pid,
                                previous_minor);
		}
		
	} while (!rv);

    
	fprintf(stderr,
                "[process_skeleton]: Cpu [%d] Process [%d]: Exiting\n",
		cpu,
                pid);
        
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
}


void
enqueue_realtime_processes(frs_t* frs)
{
        pdesc_t* pdesc;
        int pid;
        int minor;


	/*
	 * Process A
	 * freq: 20Hz
	 */

	pdesc = pdesc_create(frs, ppmess[PROCESS_A],
                             processA_loop, ppnloops[PROCESS_A], ppxloops[PROCESS_A]);
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
	 * Process B
	 * freq: 10Hz
	 */

	pdesc = pdesc_create(frs, ppmess[PROCESS_B],
                             processB_loop, ppnloops[PROCESS_B], ppxloops[PROCESS_B]);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processB");
		exit(1);
	}

	for (minor = 0; minor < 20; minor += 2) {
		int disc = FRS_DISC_RT | FRS_DISC_CONT;
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
	 * freq: 5Hz
	 */

	pdesc = pdesc_create(frs, ppmess[PROCESS_C],
                             processC_loop, ppnloops[PROCESS_C], ppxloops[PROCESS_C]);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processC");
		exit(1);
	}

	for (minor = 0; minor < 20; minor += 4) {
                int subminor;
		int disc = FRS_DISC_RT | FRS_DISC_CONT;
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
	 * Process D
	 * freq: 20Hz
	 */

	pdesc = pdesc_create(frs, ppmess[PROCESS_D],
                             processD_loop, ppnloops[PROCESS_D], ppxloops[PROCESS_D]);
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
	 * freq: 4Hz
	 */

	pdesc = pdesc_create(frs, ppmess[PROCESS_E],
                             processE_loop, ppnloops[PROCESS_E], ppxloops[PROCESS_E]);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processE");
		exit(1);
	}

	for (minor = 0; minor < 20; minor += 5) {
                int subminor;
		int disc = FRS_DISC_RT | FRS_DISC_CONT;
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
	 * freq: 2Hz
	 */

	pdesc = pdesc_create(frs, ppmess[PROCESS_F],
                             processF_loop, ppnloops[PROCESS_F], ppxloops[PROCESS_F]);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processF");
		exit(1);
	}

	for (minor = 0; minor < 20; minor += 10) {
                int subminor;
		int disc = FRS_DISC_RT | FRS_DISC_CONT;
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
	 * Process K1
	 * freq: BACKG
         * MUST be enqueued after ALL realtime processes.
	 */

	pdesc = pdesc_create(frs, ppmess[PROCESS_K1],
                             processK1_loop, ppnloops[PROCESS_K1], ppxloops[PROCESS_K1]);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processK1");
		exit(1);
	}
	for (minor = 0; minor < 20; minor++) {
		if (frs_enqueue(frs, pid, minor, FRS_DISC_BACKGROUND) < 0) {
			perror("[frs_enqueue]: processK1");
			exit(1);
		}
	}

 	/*
	 * Process K2
	 * freq: BACKG
         * MUST be enqueued after ALL realtime processes.         
	 */

	pdesc = pdesc_create(frs, ppmess[PROCESS_K2],
                             processK2_loop, ppnloops[PROCESS_K2], ppxloops[PROCESS_K2]);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processK2");
		exit(1);
	}
	for (minor = 0; minor < 20; minor++) {
		if (frs_enqueue(frs, pid, minor, FRS_DISC_BACKGROUND) < 0) {
			perror("[frs_enqueue]: processK2");
			exit(1);
		}
	}

 	/*
	 * Process K3
	 * freq: BACKG
         * MUST be enqueued after ALL realtime processes.               
	 */

	pdesc = pdesc_create(frs, ppmess[PROCESS_K3],
                             processK3_loop, ppnloops[PROCESS_K3], ppxloops[PROCESS_K3]);
	pid = sproc(process_skeleton, PR_SALL, (void*)pdesc);
	if (pid == -1) {
		perror("[sproc] processK3");
		exit(1);
	}
	for (minor = 0; minor < 20; minor++) {
		if (frs_enqueue(frs, pid, minor, FRS_DISC_BACKGROUND) < 0) {
			perror("[frs_enqueue]: processK3");
			exit(1);
		}
	}
}       

void
wait_for_all_processes(void)
{
        int i;
        int wstatus;
        
        /*
         * Wait for all processes to finish
         * The actual exit will happen from the
         * termination signal handler.
         */

        for (i = 0; i < TPROCS; i++) {
                wait(&wstatus);
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
	frs_fsched_info_t* info = (frs_fsched_info_t*)arg;
        
	if ((frs = frs_create_master(info->cpu,
                                     info->intr_source,
                                     info->intr_qualifier,
                                     info->n_minors,
                                     info->num_slaves)) == NULL) {
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

        enqueue_realtime_processes(frs);

        barrier(all_procs_enqueued, 30);
        
	/*
	 * We now start this Frame Scheduler
	 */
	
	if (frs_start(frs) < 0) {
		fprintf(stderr, "[frs_start]: Cpu: [%d] MasterProcess: [%d]\n",
			info->cpu, getpid());
		perror("[frs_start]");
		exit(1);
	}

	wait_for_all_processes();
        
	exit(0);
}


void
process_for_slave_frs(void* arg)
{
	frs_t* frs;
        sinfo_t* sinfo = (sinfo_t*)arg;

	/*
	 * Create  slave
         */

	if ((frs = frs_create_slave(sinfo->scpu, sinfo->mfrs)) == NULL) {
		perror("[main]: frs_create_slave failed");
		exit(1);
	}

        enqueue_realtime_processes(frs);
        
        barrier(all_procs_enqueued, 30);
        
	/*
	 * We now start this Frame Scheduler
	 */
	
	if (frs_start(frs) < 0) {
		fprintf(stderr, "[frs_start]: Cpu: [%d] MasterProcess: [%d]\n",
			sinfo->scpu, getpid());
		perror("[frs_start]");
		exit(1);
	}

        wait_for_all_processes();

	exit(0);
}


main(int argc, char** argv)
{
	int rv;
	int wstatus;
	frs_t master_frs1;
	frs_t master_frs2;

        printf("Running 3 frame scheduler\n");

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

        {
                frs_fsched_info_t info;

                info.cpu = 1;
                info.intr_source = FRS_INTRSOURCE_CCTIMER;
                info.intr_qualifier = 50000;
                info.n_minors = 20;
                info.num_slaves = 2;
                
                rv = sproc(process_for_master_frs, PR_SALL, &info);
                if (rv == -1) {
                        perror("[sproc]-master_frs");
                        exit(1);
                }
        }

	/*
	 * Wait for the master FRs to be created
	 */

	sema_p(master_frs_created);
	assert(master_frs != 0);

	printf("The master has been created\n");

	/*
	 * The master has been created,
	 * we can proceed to create the slaves
	 */

        {
                sinfo_t sinfo;
                sinfo.mfrs = master_frs;
                sinfo.scpu = 2;
                
                rv = sproc(process_for_slave_frs, PR_SALL, (void*)&sinfo);
                if (rv == -1) {
                        perror("[sproc]-slave_frs");
                        exit(1);
                }
        }

        {
                sinfo_t sinfo;
                sinfo.mfrs = master_frs;
                sinfo.scpu = 3;
                
                rv = sproc(process_for_slave_frs, PR_SALL, (void*)&sinfo);
                if (rv == -1) {
                        perror("[sproc]-slave_frs");
                        exit(1);
                }
        }

	
	
	/*
	 * Wait for each of the master processes
	 */

        sema_cleanup();
        
	wait(&wstatus);
	wait(&wstatus);
	wait(&wstatus);
	
	exit(0);
}

	













