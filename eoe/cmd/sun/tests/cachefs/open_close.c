#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/time.h>

#define USECS_PER_SEC		1000000
#define SEC_TO_USEC(sec)	(long)((sec) * USECS_PER_SEC)
#define TV_TO_USEC(tvp)		(long)(SEC_TO_USEC((tvp)->tv_sec) + (tvp)->tv_usec)

int
main(int argc, char **argv)
{
	int fd;
	char *progname = *argv;
	DIR *dirp;
	struct dirent *dentp;
	int count = 0;
	struct timeval start;
	struct timeval end;

	if (argc > 1) {
		if (gettimeofday(&start) == -1) {
			perror("gettimeofday");
			exit(1);
		}
		for (argv++, argc--; argc; argv++, argc--) {
			count++;
			if ((fd = open(*argv, O_RDONLY)) == -1) {
				perror(*argv);
				exit(1);
			} else if (close(fd) == -1) {
				perror(*argv);
				exit(1);
			}
		}
		if (gettimeofday(&end) == -1) {
			perror("gettimeofday");
			exit(1);
		}
	} else {
		dirp = opendir(".");
		if (dirp) {
			if (gettimeofday(&start) == -1) {
				perror("gettimeofday");
				exit(1);
			}
			while (dentp = readdir(dirp)) {
				count++;
				if ((fd = open(dentp->d_name, O_RDONLY)) == -1) {
					perror(dentp->d_name);
					exit(1);
				} else if (close(fd) == -1) {
					perror(dentp->d_name);
					exit(1);
				}
			}
			if (gettimeofday(&end) == -1) {
				perror("gettimeofday");
				exit(1);
			}
			closedir(dirp);
		} else {
			fprintf(stderr, "unable to open .: %s\n", strerror(errno));
		}
	}
	printf("total time %ld, open/close count %d, avg time %ld\n",
		(long)TV_TO_USEC(&end) - (long)TV_TO_USEC(&start), count,
		((long)TV_TO_USEC(&end) - (long)TV_TO_USEC(&start)) / (long)count);
}
