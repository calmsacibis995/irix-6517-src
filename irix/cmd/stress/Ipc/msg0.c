/* msg0		This is a simple test of the System V msg routines.  A message
 *		is sent, then received and checked.  An optional count argument
 *		may be given to repeat the test.
 */

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define nextarg {argc--; argv++;}

int forkflag = 0;

main(int argc, char **argv)
{
	register int i;
	int msqid;
	struct msgbuf *sendmsgp;
	struct msgbuf *recvmsgp;
	char message[30];
	size_t messagelen;
	int count;
	int mainpid, mainwval;

	nextarg;

	while (argc && (**argv == '-')) {
		switch(*++*argv) {
			case 'f':
				forkflag++;
				break;

			default:
				fprintf(stderr, "unknown key\n");
				exit(1);
		}
		nextarg;
	}

	if (forkflag) {
		if ((mainpid = fork()) == -1) {
			perror("fork");
			exit(1);
		}
	}


	if (argc) {
		count = atoi(*argv);
	} else {
		count = 1;
	}

	for (i = count; i--; ) {
		if ((msqid = msgget(IPC_PRIVATE, 0777)) < 0) {
			perror("msgget");
			exit(1);
		}
		sprintf(message, "Message %d", msqid);
		messagelen = strlen(message)+1;
		if ((sendmsgp=(struct msgbuf *)malloc(sizeof(struct msgbuf)+30))== NULL) {
			fprintf(stderr, "malloc failed\n");
		}
		if ((recvmsgp=(struct msgbuf *)malloc(sizeof(struct msgbuf)+30))== NULL) {
			fprintf(stderr, "malloc failed\n");
		}
		
	
		sendmsgp->mtype = 1;
		strcpy(sendmsgp->mtext, message);
		if (msgsnd(msqid, sendmsgp, messagelen, 0) < 0) {
			perror("msgsnd");
		}
	
		if (msgrcv(msqid, recvmsgp, messagelen, 0, 0) < 0) {
			perror("msgrcv");
		}
		if (strcmp(recvmsgp->mtext, message) != 0) {
			fprintf(stderr, "FAIL\n");
		}
		
		if (msgctl(msqid, IPC_RMID, 0) < 0) {
			perror("msgctl");
			exit(1);
		}
	}
	if (forkflag && mainpid && ((mainwval = wait(0)) != mainpid)) {
		if (mainwval == -1) {
			perror("wait");
		} else {
			fprintf(stderr, "wait returned bogus pid\n");
		}
		exit(1);
	}
	return(0);
}
