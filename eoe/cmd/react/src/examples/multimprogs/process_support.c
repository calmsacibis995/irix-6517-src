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

#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include "defs.h"
#include "ipc.h"


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
skeleton(void* arg)
{
	int previous_minor;
	int rv;
	int pid;
	int cpu;
	pdesc_t* pdesc;
	
	pdesc = (pdesc_t*)arg;
	assert(pdesc != NULL);

	pid = getpid();
	cpu = frs_getcpu(pdesc->frs);
	
	/*
	 * First, we join the frame scheduler
	 */

	if (frs_join(pdesc->frs) < 0) {
		fprintf(stderr,
                        "[skeleton]: Cpu [%d] Process [%d] frs_join failed\n",
			cpu,
                        pid);
		perror("[process_skeleton]");
		exit(1);
	}

	if (pdesc->messages & PDESC_MSG_JOIN) {
		fprintf(stderr,
                        "[skeleton] Cpu [%d] Process [%d]: Joined Frame Scheduler\n",
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

		rv = (*pdesc->action_function)();
		
		/*
		 * After we are done with our computations, we
		 * yield the cpu. THe yield call will not return until
		 * it's our turn to execute again.
		 */

		if ((previous_minor = frs_yield()) < 0) {
			fprintf(stderr,
                                "[skeleton]: Cpu [%d] Process [%d] frs_yield failed\n",
				cpu,
                                pid);
			perror("[process_skeleton]");
			exit(1);
		}

		if (pdesc->messages & PDESC_MSG_YIELD) {
			fprintf(stderr,
                                "[sk] Cpu [%d] Pid [%d]\n",
				cpu,
                                pid);
		}
		
	} while (!rv);

        fprintf(stderr,
                "[skeleton]: Destroying frs on cpu [%d] from process [%d]\n",
                cpu,
                pid);

	if (frs_destroy(pdesc->frs) < 0) {
		perror("[skeleton]: frs_destroy failed\n");
		exit(1);
	}

    
	fprintf(stderr,
                "[skeleton]: Cpu [%d] Process [%d]: Exiting\n",
		cpu,
                pid);
        
	exit(0);
}

frs_t*
enqueue_client(int cpu, int seq, int enqdesc_list_len, enqdesc_t* enqdesc_list)
{
	frs_t* frs;
	int minor;
	int pid;
	int wstatus;
	int cpu_number;
	msgbuf_t message;
        int i;

        assert(cpu > 0);
        assert(enqdesc_list_len > 0);
        assert(enqdesc_list != NULL);
        
	pid = getpid();

        printf("ENQUEUE CLIENT running on behalf of process [%d], seq: %d\n", pid, seq);
	
	/*
	 * Initialize signals to catch
	 * termination signals and underrun, overrun errors.
	 */
	
	setup_signals();
	
	/*
	 * Wait for my sequence number
	 */

	if (mrecv_select(cpukey(SEQKEY, cpu),
                         &message,
                         sizeof(msgbuf_t),
                         MESSAGE_SEQBASE + seq) < 0) {
		syserr("mrecv_select");
	}
	
	/*
	 * Request frs handle from master
	 */

	message.mtype = MESSAGE_FRS;
	message.replykey = processkey(REPLKEY, pid);
	message.pid = pid;
	
	if (msend(cpukey(REQKEY, cpu), &message, sizeof(msgbuf_t)) < 0) {
		syserr("send request for frs");
	}

	if (mreceive(processkey(REPLKEY, pid), &message, sizeof(msgbuf_t)) < 0) {
		syserr("receive frs");
	}

	assert(message.mtype == MESSAGE_FRSREPLY);
	assert(message.pid == pid);
	
	/*
	 * Now we have the frs handle
	 */

        if ((frs = (frs_t*)malloc(sizeof(frs_t))) == NULL) {
                syserr("malloc");
        }
	*frs = message.frs;

        printf("FRS object for process [%d] on cpu [%d]:\n", pid, cpu);
	printf("cpu: %d, intrsource: %d, intrq: %d\n",
	       frs->frs_info.cpu,
	       frs->frs_info.intr_source,
	       frs->frs_info.intr_qualifier);
	printf("nminors: %d, sync_master_controller: %d, numslaves: %d\n",
	       frs->frs_info.n_minors,
	       frs->frs_info.syncm_controller,
	       frs->frs_info.num_slaves);
	
        assert(cpu == frs_getcpu(frs));

        /*
	 * ProcessA will be enqueued on all minor frame queues
	 * with a strict RT discipline
	 * Issue enqueueing requests to the master
	 */
	
	for (i = 0; i < enqdesc_list_len; i++) {
		message.mtype = MESSAGE_ENQUEUE;
		message.replykey = processkey(REPLKEY, pid);
		message.pid = pid;
		message.minor = enqdesc_list[i].minor;
		message.disc = enqdesc_list[i].disc;
		if (msend(cpukey(REQKEY, cpu), &message, sizeof(msgbuf_t)) < 0) {
			syserr("send request for enqueueing");
		}
		if (mreceive(processkey(REPLKEY, pid), &message, sizeof(msgbuf_t)) < 0) {
			syserr("recv enqueueong reply");
		}
		if (message.mtype == MESSAGE_RERROR) {
			errno = message.errorcode;
			syserr("Enqueueing failed");
		}
	}

	/*
	 * Now we should inform the master
	 * we are ready to run
	 */


	message.mtype = MESSAGE_READY;
	message.replykey = processkey(REPLKEY, pid);
	message.pid = pid;
	
	if (msend(cpukey(REQKEY, cpu), &message, sizeof(msgbuf_t)) < 0) {
		syserr("send ready");
	}

	if (mreceive(processkey(REPLKEY, pid), &message, sizeof(msgbuf_t)) < 0) {
		syserr("receive go");
	}

	/*
	 * We don't need this comm channel anymore
	 */

	rmqueue(processkey(REPLKEY, pid));

        return (frs);
}
	          
