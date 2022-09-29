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
 * Example using separate programs
 */

/*
 * This example creates one simple frame scheduler
 * in one process, running two other processes forked
 * independently:
 * Master Process: master.c
 *    Creates the frame scheduler and then waits for messages from
 *    the  other processes that will be frame scheduled. A message
 *    contains the pid of the process to be enqueued and the minor
 *    where it should run. After Every process has sent a EOT message,
 *    the master processes starts the frame scheduler.
 * Process A: processA.c
 *    Realtime process A
 * Process B: processB.c
 *    Realtime processB
 *
 * PARAMETERS:
 * cpu: argument
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
termination_signal()
{
	if ((int)signal(SIGHUP, termination_signal) == -1) {
		perror("[termination_signal]: Error while resetting signal");
		exit(1);
	}
	fprintf(stderr, "[termination_signal], MASTER-PID=%d\n", getpid());	
	exit(8);
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
	int cpu_number;
	msgbuf_t message;
	int number_of_slaves;
	int errflag = 0;
	int c;
	char* command;
	int counter;
	int sequence;
	extern char* optarg;

	/*
	 * Usage: simple [-p cpu_number] [-s num_slaves]
	 */

	command = argv[0];
	cpu_number = 1;
	number_of_slaves = 2;

	while ((c = getopt(argc, argv, "p:s:")) != EOF) {
		switch (c) {
		      case 'p':
			cpu_number = atoi(optarg);
			break;
		      case 's':
			number_of_slaves = atoi(optarg);
			break;
		      case '?':
			errflag++;
			break;
		}
	}

	if (errflag) {
		(void)fprintf(stderr, "usage: %s [-p cpu_number] [-s number_of_slaves]\n", command);
		exit(1);
	}

	printf("Running Frame Scheduler on Processor [%d], with [%d] procs\n",
	       cpu_number, number_of_slaves);
	
	/*
	 * Initialize signals to catch
	 * termination signals and underrun, overrun errors.
	 */
	
	setup_signals();
	
	/*
	 * Create the frame scheduler object
	 * cpu = cpu_number,
	 * interrupt source = CCTIMER
	 * period = 600 [ms] == 600000 [microseconds] 
	 * number of minors = 4
	 * slave list = NULL (no slaves)
	 * number of slaves = 0, no slaves
	 */

	if ((frs = frs_create_master(cpu_number, FRS_INTRSOURCE_CCTIMER, 600000, 4, 0)) == NULL) {
		perror("[main]: frs_create_master failed");
		exit(1);
	}

	/*
	 * Slave processes need to be enqueued by the master. The kernel does not permit
	 * slaves enqueing themselves because it may result in a deadlock.
	 * Since we have independent processes here, I'm going to have the master act as
	 * an "enqueing server". Slaves will send messages to the master asking to
	 * be enqueued. In addition, the slaves processes also need access to the frs
	 * handle in order to be able to join and yield. The master will also take care
	 * of sending this handle over to the slaves via messages. It is safe to serialize this
	 * handle and ship it over to a different process domain if the received handle is only used
	 * for joining and yielding. Otherwise, we'd have to use a shared memory arena.
	 *
	 * Processes have to be enqueued in the order they are supposed to run. Therefore,
	 * we have to sequence the enqueuing requests according to their priority. I will
	 * do this sequencing by initially sending tokens that specify which process can
	 * start issuing requests.
	 * 
	 * I will use SVR4 IPC messages for synchronization and sending data across.
	 *
	 */

	/*
	 * Now we wait for slave messages, responding to the frs requests, and decrementing the
	 * while loop counter everytime we receive a "ready to run" message.
	 */

	
	counter = number_of_slaves;
	sequence = 0;

	/*
	 * First, we issue the sequencing token, asking the higuest priority
	 * process to start issuing enqueuing requests.
	 */

	message.mtype = MESSAGE_SEQBASE + sequence;
	message.errorcode = 0;
	if (msend(SEQKEY, &message, sizeof(msgbuf_t)) <0) {
		syserr("msend-seq");
	}
	printf("Issued Enqueuing Token for process with priority <%d>\n", sequence);
	
	/*
	 * We loop until all slaves have told us they are ready to run
	 */

	
	while (counter) {
		if (mreceive(REQKEY, &message, sizeof(msgbuf_t)) < 0) {
			syserr("receive");
		}

		switch (message.mtype) {
		      case MESSAGE_ENQUEUE:
			/*
			 * Enqueue Process
			 */
			printf("Process [%d] has requested to be enqueued on minor %d\n",
			       message.pid, message.minor);
			if (frs_enqueue(frs, message.pid, message.minor, message.disc) < 0) {
				message.mtype = MESSAGE_RERROR;
				message.errorcode = errno;
				msend(message.replykey, &message, sizeof(msgbuf_t));
				syserr("frs_enqueue");
			}
			message.mtype = MESSAGE_ROK;
			message.errorcode = 0;
			if (msend(message.replykey, &message, sizeof(msgbuf_t)) < 0) {
				syserr("msend-replyok");
			}
			break;
		      case MESSAGE_FRS:
			/*
			 * frs handle request
			 */
			printf("Process [%d] has requested the frs handle\n", message.pid);
			message.frs = *frs;
			message.mtype = MESSAGE_FRSREPLY;
			if (msend(message.replykey, &message, sizeof(msgbuf_t)) < 0) {
				syserr("send frs");
			}
			break;
		      case MESSAGE_READY:
			/*
			 * Slave process ready to go
			 */
			printf("Process [%d] is ready to start\n", message.pid);
			counter--;
			message.mtype = MESSAGE_GO;
			if (msend(message.replykey, &message, sizeof(msgbuf_t)) < 0) {
				syserr("send go");
			}
			/*
			 * Since the process with the current priority is done, we can
			 * issue a token with the next sequence number, letting the process
			 * with the next priority start issuing enqueuing requests.
			 */
			sequence++;
			message.mtype = MESSAGE_SEQBASE + sequence;
			message.errorcode = 0;
			if (msend(SEQKEY, &message, sizeof(msgbuf_t)) <0) {
				syserr("msend-seq");
			}
			printf("Issued Enqueuing Token for process with priority <%d>\n", sequence);
			break;
		      default:
			syserr("Invalid Message");
			break;
		}
	}

	rmqueue(SEQKEY); 
	rmqueue(REQKEY);

	/*
	 * At this point all processes have been enqueued
	 * We can start the frame scheduler
	 */
				
	if (frs_start(frs) < 0) {
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

	













