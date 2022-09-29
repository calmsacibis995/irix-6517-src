#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <wait.h>
#include <errno.h>
#include <stdio.h>
#include <sys/termio.h>
#include <sys/types.h>
#include <sys/prctl.h>

#define TTYDN	"/dev/ttyd2"
#define TTY	"/dev/tty"
#define BUFLEN  (sizeof(outbuf)-1)

struct termio buf;
char *cmd;

/*
 * read/write to /dev/tty will fail if the controlling terminal 
 * is closed.
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
		setsid();

		if ((fd = open(TTYDN, O_RDWR|O_NDELAY)) < 0) {
			
			perror("open ttyd fails");
			exit(1);
		}

		if ((cpid = fork()) == 0) {
			if ((fd2 = open(TTY, O_RDWR)) < 0) {
				perror("child open of /dev/tty fails");
				exit(1);
			}

			unblockproc(getppid());
			blockproc(getpid());
			close(fd);
			if (write(fd2, outbuf, BUFLEN) >= 0) {
				fprintf(stderr,"%s: write should get ENXIO\n", cmd);
				exit(1);
				
			}
			if (errno != ENXIO)
				fprintf(stderr, "%s: write should get ENXIO\n", cmd);
			close(fd2);
	
			setsid();
			if ((fdc1 = open(TTYDN, O_RDWR|O_NDELAY)) < 0) {
				perror("second open /dev/ttyd fails");
				exit(1);
			}
			if ((fd2 = open(TTY, O_RDWR)) < 0) {
				perror("second open of /dev/tty fails");
				exit(1);
			}
			if (write(fd2, outbuf, BUFLEN) < 0) {
				perror("child second write fails");
				exit(1);
			}
			close(fdc1);

			if (open(TTY, O_RDWR) >= 0) {
				fprintf(stderr, "%s: third open of /dev/tty should fail\n", cmd);
			}

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
