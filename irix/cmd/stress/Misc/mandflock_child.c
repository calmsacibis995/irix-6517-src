#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "stress.h"

/*
 * An exec'd child from 'mandflock'.
 */
int
main(int argc, char **argv)
{
	int fd = atoi(argv[0]);
	char buf[512];

	/*
	 * Check that it obeys the mandatory lock that covers the
	 * first byte.
	 */
	lseek(fd, 0, 0);
	if (read(fd, buf, 1) != 1) {
		printf("mandflock ERROR: exec child read of first bytes\n");
	}
	lseek(fd, 0, 0);
	if (read(fd, buf, 512) != -1 || errno != EAGAIN) {
		printf("mandflock ERROR: exec child read of all bytes\n");
	}

	/*
	 * Now chmod the file to turn off mandatory locking.
	 * Parent should then be able to read and write all bytes.
	 */
	if (fchmod(fd, 0666)) {
		printf("mandflock ERROR: exec child turn off mand. locking\n");
		abort();
	}
	exit(0);
}
