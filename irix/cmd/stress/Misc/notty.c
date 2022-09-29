#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>
#include <stdio.h>
#include <sys/termio.h>

#define TTYDN	"/dev/ttyd2"
#define TTY	"/dev/tty"
#define BUFLEN  (sizeof(outbuf)-1)

struct termio buf;
char *cmd;

/*
 * test ioctl(fd, TIOCNOTTY)
 */

int
main(int argc, char **argv)
{
	int fd;
	int fd2;
	pid_t ppid, cpid;
	int child, grandchild;
	char *outbuf = "No no";

	cmd = argv[0];
	ppid = fork();
	if  (ppid == 0) {
		setsid();

		/*
		 * open controlling terminal
		 */
		if ((fd = open(TTYDN, O_RDWR|O_NDELAY|O_NOCTTY)) < 0) {
			perror("open ttyd fails");
			exit(1);
		}

		if ((fd2 = open(TTY, O_RDWR)) >= 0) {
			fprintf(stderr, "open of /dev/tty should fail\n");
			exit(1);
		}
		close(fd);
		if ((fd = open(TTYDN, O_RDWR|O_NDELAY)) < 0) {
			perror("open ttyd fails");
			exit(1);
		}
		if ((fd2 = open(TTY, O_RDWR)) < 0) {
			perror("open /dev/tty fails");
			exit(1);
		}

                if ((cpid = fork()) == 0) {
               
			if (ioctl(fd, TIOCNOTTY, 0) < 0) {
				perror("ioctl TIOCNOTTY fails");
				exit(1);
			}
			if (write(fd2, outbuf, BUFLEN) >= 0) {
				fprintf(stderr, "%s: write should get ENXIO\n", cmd);
				exit(1);
			}
			if (errno != ENXIO)
				fprintf(stderr, "write should get ENXIO\n");
		} else if (cpid > 0) {
			wait(&grandchild);
			close(fd);
			close(fd2);
		}
	} else if (ppid >0) {
		wait(&child);
	}
	return 0;
}
