#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

void
error(char *s)
{
	perror(s);
	exit(1);
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: genfile file size\n");
	exit(1);
}

#define	BUFSIZE	(64 * 1024)
char buf[BUFSIZE];

main(int argc, char **argv)
{
	int fd;
	int count;
	int r;

	if (argc != 3) {
		usage();
	}

	srand(getpid());

	fd = open(argv[1], O_CREAT|O_RDWR|O_TRUNC, 0644);
	if (fd < 0) {
		error(argv[1]);
	}

	count = atoi(argv[2]);

	while (count > 0) {
		uint w;
		int c = rand();
		memset(buf, c, BUFSIZE);
		w = count < BUFSIZE ? count : BUFSIZE;
		r = write(fd, buf, w);
		if (r < 0) {
			error("write");
		}
		count -= r;
	}

	(void)close(fd);
	exit(0);
	/* NOTREACHED */
}
