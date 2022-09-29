#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <wait.h>
#include <sys/termio.h>
#include <sys/types.h>
#include <sys/prctl.h>

#define TTYDN	"/dev/ttyd2"
#define TTY	"/dev/tty"
#define BUFLEN  (sizeof(outbuf)-1)

struct termio buf;
char *cmd;

/*
 * If a process, which is not a member of the controlling process's session, 
 * makes the lastclose of the controlling terminal, the controlling
 * terminal should be detached.
 */

int
main(int argc, char **argv)
{
	int fd;
	int fd2;
	int fdc1;
	pid_t ppid, cpid;
	auto int child, grandchild;
	char *outbuf = "No no";

	cmd = argv[0];
	ppid = fork();
	if  (ppid == 0) {

		if ((fd = open(TTYDN, O_RDWR|O_NDELAY)) < 0) {
			
			perror("open ttyd fails");
			exit(1);
		}

		if ((cpid = fork()) == 0) {
			close(fd);
			setsid();
			if ((fdc1 = open(TTYDN, O_RDWR|O_NDELAY)) < 0) {
				perror("open controlling terminal fails");
				exit(1);
			}
			if ((fd2 = open(TTY, O_RDWR)) < 0) {
				perror("child open of /dev/tty fails");
				exit(1);
			}
			if (write(fd2, outbuf, BUFLEN) < 0) {
				perror("child write to /dev/tty fails");
				exit(1);
			}

			close(fdc1);

			if (write(fd2, outbuf, BUFLEN) < 0) {
				perror("child second write to /dev/tty fails");
				exit(1);
			}

			unblockproc(getppid());
			blockproc(getpid());
			if (write(fd2, outbuf, BUFLEN) >= 0) {
				fprintf(stderr, "%s: write should get ENXIO\n", cmd);
				exit(1);
				
			}
			if (errno != ENXIO)
				fprintf(stderr, "%s: write should get ENXIO\n",cmd);
			close(fd2);

		} else if (cpid >0) {
			blockproc(getpid()); 
			close(fd);
			unblockproc(cpid); 
			wait(&grandchild);
		}
	} else if (ppid >0) {
		wait(&child);
	}
	return 0;
}
