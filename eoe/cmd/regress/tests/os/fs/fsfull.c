#include <errno.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#define BSIZE 16384
#define TAIL 10
char buffer[BSIZE];
extern int errno;

main(argc, argv)
int argc;
char **argv;
{
	long sz, wsz;
	int fd;
	char *filename = tempnam((char *)NULL, "fsfull");

	fd = open(filename, O_RDWR|O_CREAT, 0666);
	if (fd < 0) {
		printf("INFO: %s: couldn't create file %s to run test, aborting\n", argv[0], filename);
		exit(1);
	}

	unlink(filename);

	for (sz = 0 ; ; sz += wsz ) {
		wsz = write(fd, buffer, BSIZE);
		if (wsz < BSIZE) {
			if (wsz > 0) {
				sz += wsz;
			}
			break;
		}
	}

	wsz = ftruncate(fd, sz - TAIL);
	if (wsz < 0) {
		printf("ERROR: truncate returned %d, errno %d\n", wsz, errno);
		exit(1);
	}
	
	wsz = lseek(fd, 0, SEEK_END);
	if (wsz < 0) {
		printf("ERROR: lseek returned %d, errno %d\n", wsz, errno);
		exit(1);
	}

	wsz = write(fd, buffer, BSIZE);

	if (wsz != TAIL)
		printf("INFO: write after truncate returned %d, not %d\n",
			wsz, TAIL);

	close(fd);

	exit(0);
}
