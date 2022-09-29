/*
 * msg2		This program tests the ability of msgsnd() to wait for more
 *		space.  The program forks into a parent and a child.  The
 *		parent sends several large messages, several of which should
 *		hang for more space.  The child sleeps for a while, and then
 *		reads the messages.
 */
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
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
	int messagelen;
	int pid;
	int wval;

	messagelen = 1024;

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
		sendmsgp->mtype = 1;
		for (i = 0; i < 6; i++) {
			sprintf(message, "Message %02d\n", i);
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
		for (i = 0; i < 6; i++) {
			sprintf(message, "Message %02d\n", i);
			if (msgrcv(msqid, recvmsgp, messagelen, 0, 0) < 0) {
				perror("msgrcv");
			}
			if (strcmp(message, recvmsgp->mtext) != 0) {
				fprintf(stderr, "Compare Error\n");
			}
		}
		if (msgctl(msqid, IPC_RMID, 0) < 0) {
			perror("msgctl");
			exit(1);
		}
	}
	return(0);
}
