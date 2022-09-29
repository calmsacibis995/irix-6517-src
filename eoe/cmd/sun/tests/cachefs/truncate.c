#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

char *Progname;

extern int errno;

int
main(int argc, char **argv)
{
	int bytes;
	struct stat sb;
	char *testfile = "truncate.test";
	int fd;
	char buf[4096];

	Progname = *argv++;
	if (argc >= 2) {
		testfile = *argv++;
	}
	while (1) {
		fd = open(testfile, O_RDWR | O_CREAT, 0666);
		if (fd == -1) {
			perror("open/create");
			exit(errno);
		}
		bytes = write(fd, buf, sizeof(buf));
		if (bytes < sizeof(buf)) {
			if (bytes == -1) {
				perror("write");
				exit(errno);
			} else {
				fprintf(stderr, "Short write (%d of %d) to %s\n", bytes,
					sizeof(buf), testfile);
				exit(-1);
			}
		}
		if (close(fd) == -1) {
			perror("close");
			exit(errno);
		}
		fd = open(testfile, O_RDWR | O_TRUNC, 0666);
		if (fd == -1) {
			perror("open/truncate");
			exit(errno);
		}
		if (close(fd) == -1) {
			perror("close");
			exit(errno);
		}
		if (stat(testfile, &sb) == -1) {
			perror("stat");
			exit(errno);
		} else if (sb.st_size != 0) {
			fprintf(stderr, "File size not zero\n");
			exit(-1);
		}
	}
}
