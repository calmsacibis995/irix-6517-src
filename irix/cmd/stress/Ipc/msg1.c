/*
 * msg1		This program tests synchronization of sending and receiving
 *		messages.  It forks into a parent, which sends 10 messages,
 *		and a child, which reads 10 and sees if the text is correct.
 *		The child may sleep temporarily if it gets ahead of the parent,
 *		but the should always catch up eventually.
 */
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define nextarg {argc--; argv++;}

int
main(int argc, char **argv)
{
	register int i;
	int msqid;
	struct msgbuf *sendmsgp;
	struct msgbuf *recvmsgp;
	char message[30];
	size_t messagelen;
	pid_t pid;
	int wval;

	sprintf(message, "Message %02d", 0);
	messagelen = strlen(message)+1;

	if ((sendmsgp = (struct msgbuf *)malloc(sizeof(struct msgbuf)+messagelen)) == NULL) {
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}
	if ((recvmsgp = (struct msgbuf *)malloc(sizeof(struct msgbuf)+messagelen)) == NULL) {
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	if ((msqid = msgget(IPC_PRIVATE, 0777)) < 0) {
		perror("msgget");
		exit(1);
	}

	if ((pid = fork()) == -1) {
		perror("fork");
		if (msgctl(msqid, IPC_RMID, 0) < 0) {
			perror("msgctl");
		}
		exit(1);
	}
	if (pid) {	/* PARENT */
		sleep(2);
		sendmsgp->mtype = 1;
		for (i = 10; i--; ) {
			sprintf(message, "Message %02d", i);
			strcpy(sendmsgp->mtext, message);
			if (msgsnd(msqid, sendmsgp, messagelen, 0) < 0) {
				perror("msgsnd");
			}
		}
		if ((wval = wait(0)) != pid) {
			if (wval == -1) {
				perror("wait");
			} else {
				fprintf(stderr, "wait returned bogus pid\n");
			}
			exit(1);
		}
		exit(0);
	} else {	/* CHILD */
		for (i = 10; i--; ) {
			sprintf(message, "Message %02d", i);
			if (msgrcv(msqid, recvmsgp, messagelen, 0, 0) < 0) {
				perror("msgrcv");
			}
			if (strcmp(recvmsgp->mtext, message) != 0) {
				fprintf(stderr, "FAIL\n");
			}
		}

		if (msgctl(msqid, IPC_RMID, 0) < 0) {
			perror("msgctl");
			exit(1);
		}
	}
	return(0);
}
