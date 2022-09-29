/*	@(#)rewind.c	1.1 97/01/03 Connectathon Testsuite	*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

main()
{
	char buffer[8192];
	int size = 8192;
	int fd;
	int i;
	off_t off;

	if ((fd = open("test.file", O_RDWR | O_CREAT, 0666)) == -1) {
		perror("open");
		exit(1);
	}

	for (i = 0; i < 3; i++) {
		if (write(fd, buffer, size) != size) {
			perror("write");
			exit(1);
		}
	}

	if ((off = lseek(fd, (off_t)0, SEEK_SET)) != 0) {
		printf("file offset=%ld\n", off);
		exit(1);
	}

	if (ftruncate(fd, 0)) {
		perror("ftruncate");
		exit(1);
	}

	if (write(fd, buffer, 1) != 1) {
		perror("write");
		exit(1);
	}

	if ((off = lseek(fd, 0, SEEK_END)) != 1) {
		printf("file offset=%ld\n", off);
		exit(1);
	}

	close(fd);

	exit(0);
}
