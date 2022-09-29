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
 * I will discriminate among different frame schedulers in a synchronous group
 * by imbedding the processor number associated with an frs in the message key.
 *
 */


void
enqueue_server(frs_t* frs, int number_of_processes)
{
        int counter;
        msgbuf_t message;
       	int sequence;
        
	/*
	 * Now we wait for slave messages, responding to the frs requests, and decrementing the
	 * while loop counter everytime we receive a "ready to run" message.
	 */
	
	counter = number_of_processes;
	sequence = 0;

	/*
	 * First, we issue the sequencing token, asking the higuest priority
	 * process to start issuing enqueuing requests.
	 */

	message.mtype = MESSAGE_SEQBASE + sequence;
	message.errorcode = 0;
	if (msend(frskey(SEQKEY, frs), &message, sizeof(msgbuf_t)) <0) {
		syserr("msend-seq");
	}
	printf("FRS[%d]: Issued Enqueuing Token for process with priority <%d>\n",
               frs_getcpu(frs), sequence);
	
	/*
	 * We loop until all slaves have told us they are ready to run
	 */
	
	while (counter) {
		if (mreceive(frskey(REQKEY, frs), &message, sizeof(msgbuf_t)) < 0) {
			syserr("receive");
		}

		switch (message.mtype) {
		      case MESSAGE_ENQUEUE:
			/*
			 * Enqueue Process
			 */
			printf("FRS[%d]: Process [%d] has requested to be enqueued on minor %d\n",
			       frs_getcpu(frs), message.pid, message.minor);
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
			printf("FRS[%d]: Process [%d] has requested the frs handle\n",
                               frs_getcpu(frs), message.pid);
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
			printf("FRS[%d]: Process [%d] is ready to start\n",
                               frs_getcpu(frs), message.pid);
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
			if (msend(frskey(SEQKEY, frs), &message, sizeof(msgbuf_t)) <0) {
				syserr("msend-seq");
			}
			printf("FRS[%d]: Issued Enqueuing Token for process with priority <%d>\n",
                               frs_getcpu(frs), sequence);
			break;
		      default:
			syserr("Invalid Message");
			break;
		}
	}

	rmqueue(frskey(SEQKEY, frs)); 
	rmqueue(frskey(REQKEY, frs));
        
}
