/* msg0		This is a simple test of the System V msg routines.  A message
 *		is sent, then received and checked.  An optional count argument
 *		may be given to repeat the test.
 */

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define nextarg {argc--; argv++;}

int
main(int argc, char **argv)
{
	register int i;
	int msqid;
	struct msgbuf *sendmsgp;
	struct msgbuf *recvmsgp;
	size_t messagelen ;
	int count;
	time_t tbefore, tafter;

	nextarg;
	while (argc && (**argv == '-')) {
		switch(*++*argv) {
			case 'l':
				nextarg;
				if (argc) {
				        messagelen = atoi(*argv);
				} else {
					messagelen = 100;
				}
				break;

			default:
				fprintf(stderr, "unknown key\n");
				exit(1);
		}
		nextarg;
	}

	if (argc) {
		count = atoi(*argv);
	} else {
		count = 1;
	}

	if ((msqid = msgget(IPC_PRIVATE, 0777)) < 0) {
		perror("msgget");
		exit(1);
	}

	if ((sendmsgp=(struct msgbuf *)malloc(sizeof(struct msgbuf)+messagelen))== NULL) {
		fprintf(stderr, "malloc failed\n");
	}
	if ((recvmsgp=(struct msgbuf *)malloc(sizeof(struct msgbuf)+messagelen))== NULL) {
		fprintf(stderr, "malloc failed\n");
	}
	sendmsgp->mtype = 1;

	time(&tbefore);
	for (i = count; i--; ) {
		if (msgsnd(msqid, sendmsgp, messagelen, 0) < 0) {
			perror("msgsnd");
		}
	
		if (msgrcv(msqid, recvmsgp, messagelen, 0, 0) < 0) {
			perror("msgrcv");
		}
	}
	time (&tafter);

	fprintf(stderr, "%d msg/%d sec = %f msg/sec\n",
		count, tafter-tbefore, (float)count/(float)(tafter-tbefore));
	if (msgctl(msqid, IPC_RMID, 0) < 0) {
		perror("msgctl");
		exit(1);
	}
	return(0);
}
