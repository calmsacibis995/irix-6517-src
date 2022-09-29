/*	@(#)truncate.c	1.1 97/01/03 Connectathon Testsuite	*/
/*
 * Test to see whether the server can handle extending a file via
 * a setattr request.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

main()
{
	int fd;
	struct stat statb;

	if ((fd = creat("testfile", 0644)) < 0) {
		perror("creat");
		exit(1);
	}

	if (ftruncate(fd, 0) < 0) {
		perror("ftruncate1");
		exit(1);
	}
	if (stat("testfile", &statb) < 0) {
		perror("stat1");
		exit(1);
	}
	if (statb.st_size != 0L) {
		fprintf(stderr,
	"truncate: testfile not zero length, but no error from ftruncate\n");
		exit(1);
	}

	if (ftruncate(fd, 10) < 0) {
		perror("ftruncate2");
		exit(1);
	}
	if (stat("testfile", &statb) < 0) {
		perror("stat1");
		exit(1);
	}
	if (statb.st_size != 10L) {
		fprintf(stderr,
"truncate: testfile length not set correctly, but no error from ftruncate\n");
		exit(1);
	}

	close(fd);
	(void) unlink("testfile");

	printf("truncate succeeded\n");

	exit(0);
}
