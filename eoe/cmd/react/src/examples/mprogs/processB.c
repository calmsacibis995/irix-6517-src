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
 * Frame Scheduler
 * ProcessB
 */

/*
 *
 * PARAMETERS:
 * cpu: arg
 * intr_source: FRS_INTRSOURCE_CCTIMER
 * n_minors: 4
 * period: 600 [ms] (a long period to allow for the printf statements)
 *
 * PROCESSES:
 * Process A: Period of 600 [ms] (determined base minor frame period)
 * Process B: Period of 2400 [ms] (determined # of minor frames per major frame)
 */

#include <math.h>
#include <errno.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>
#include "ipc.h"

/*
 * Some fixed real-time loop parameters
 */

#define NLOOPS 40
#define LOGLOOPS_B 35000
#define JOINM_B
#define YIELDM_B

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
processB(frs_t* frs)
{
	int counter;
	double res;
	int i;
	int previous_minor;
	int pid = getpid();

	/*
	 * First, we join to the frame scheduler
	 */

	if (frs_join(frs) < 0) {
		perror("[processB]: frs_join failed");
		exit(1);
	}
    
#ifdef JOINM_B
	fprintf(stderr, "[processB (%d)]: Joined Frame Scheduler on cpu %d\n",
		pid, frs->frs_info.cpu);
#endif

	counter = NLOOPS;
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
			perror("[processB]: frs_yield failed");
			exit(1);
		}

#ifdef YIELDM_B	
		fprintf(stderr, "[processB (%d)]: Return from Yield; previous_minor: %d\n",
			pid, previous_minor);
#endif	
	} while (counter--);


	fprintf(stderr, "[processB (%d)]: Exiting\n", pid);
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


main(int argc, char** argv)
{
	frs_t* frs;
	int minor;
	int pid;
	int wstatus;
	int cpu_number;
	msgbuf_t message;


	printf("ProcessB Initialization...\n");

	pid = getpid();
	
	/*
	 * Initialize signals to catch
	 * termination signals and underrun, overrun errors.
	 */
	
	setup_signals();
	

	/*
	 * Wait for my sequence number
	 * I want to be enqueued second, so I wait for
	 * message of type MESSAGE_SEQBASE + 1
	 */

	if (mrecv_select(SEQKEY, &message, sizeof(msgbuf_t), MESSAGE_SEQBASE + 1) < 0) {
		syserr("mrecv_select");
	}

	/*
	 * Request frs handle from master
	 */

	message.mtype = MESSAGE_FRS;
	message.replykey = BASEKEY + pid;
	message.pid = pid;
	
	if (msend(REQKEY, &message, sizeof(msgbuf_t)) < 0) {
		syserr("send request for frs, processB");
	}

	if (mreceive(BASEKEY + pid, &message, sizeof(msgbuf_t)) < 0) {
		syserr("receive frs, processA");
	}

	assert(message.mtype == MESSAGE_FRSREPLY);
	assert(message.pid == pid);
	
	/*
	 * Now we have the frs handle
	 */

	frs = &message.frs;


	/*
	 * ProcessB will be enqueued on all minor frames, but the
	 * disciplines will differ. We need continuability for the first
	 * 3 frames, and absolute real-time for the last frame.
	 */

	for (minor = 0; minor < 3; minor++) {
		message.mtype = MESSAGE_ENQUEUE;
		message.replykey = BASEKEY + pid;
		message.pid = pid;
		message.minor = minor;
		message.disc = FRS_DISC_RT |
		               FRS_DISC_UNDERRUNNABLE |
		               FRS_DISC_OVERRUNNABLE |
		               FRS_DISC_CONT;
		if (msend(REQKEY, &message, sizeof(msgbuf_t)) < 0) {
			syserr("send request for enqueueing, processB");
		}
		if (mreceive(BASEKEY + pid, &message, sizeof(msgbuf_t)) < 0) {
			syserr("recv enqueueong reply, processB");
		}
		if (message.mtype == MESSAGE_RERROR) {
			errno = message.errorcode;
			syserr("Enqueueing failed");
		}
	}

	message.mtype = MESSAGE_ENQUEUE;
	message.replykey = BASEKEY + pid;
	message.pid = pid;
	message.minor = 3;
	message.disc = FRS_DISC_RT;
	if (msend(REQKEY, &message, sizeof(msgbuf_t)) < 0) {
		syserr("send request for enqueueing, processB");
	}
	if (mreceive(BASEKEY + pid, &message, sizeof(msgbuf_t)) < 0) {
		syserr("recv enqueueong reply, processB");
	}
	if (message.mtype == MESSAGE_RERROR) {
		errno = message.errorcode;
		syserr("Enqueueing failed");
	}

	/*
	 * Now we should inform the master
	 * we are ready to run
	 */


	message.mtype = MESSAGE_READY;
	message.replykey = BASEKEY + pid;
	message.pid = pid;
	
	if (msend(REQKEY, &message, sizeof(msgbuf_t)) < 0) {
		syserr("send ready, processA");
	}

	if (mreceive(BASEKEY + pid, &message, sizeof(msgbuf_t)) < 0) {
		syserr("receive go, processB");
	}

	/*
	 * We don't need this comm channel anymore
	 */

	rmqueue(BASEKEY + pid);
	
	/*
	 * Now we can go ahead and join the real-time game
	 * I'll do this at the beginning of the routine that
	 * implements the real-time loop
	 */

	processB(frs);
	 
	exit(0);
}

	

















