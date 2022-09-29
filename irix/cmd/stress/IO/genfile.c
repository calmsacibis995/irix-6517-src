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

#define	BUFSIZE	(64 * 1024)/sizeof(char)
#define BUFSIZEW (BUFSIZE/sizeof(char))
unsigned bufw[BUFSIZEW];
char *buf = (char *)&bufw;

main(int argc, char **argv)
{
	int fd;
	int count;
	int r;
	unsigned  x = 0;

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
		uint w, i;
#if 0
		int c = rand();
		memset(buf, c, BUFSIZE);
#endif
		w = count < BUFSIZE ? count : BUFSIZE;
		for (i=0; i < (w+3)/sizeof(int); i++)
		    bufw[i] = x++;
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
