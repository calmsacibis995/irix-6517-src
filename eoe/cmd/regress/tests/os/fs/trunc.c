#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

extern int errno;

#define BSZ 0x42000
#define SEEKMASK 0x3ffff
#define WRITEMASK 0x1fff
#define CHARMASK 0x3f
#define SLEEPMASK 0xf

char inbuf[BSZ+1];
char outbuf[BSZ+1];
int verbose;
char *filename;

#include <stdio.h>
#include <sys/signal.h>

__sigret_t do_unlink(__sigargs);
extern __sigret_t	(*signal(int,__sigret_t (*)(__sigargs)))(__sigargs);

main(argc, argv)
char **argv;
{
	int fd;
	int result, writesz, readsz, truncsz, seekpoint;
	register int iterations = 100;
	register int i;
	char *in, *out;
	char c;
	pid_t pid;

#ifdef notyet
	if (argc > 1)
		iterations = atoi(argv[1]);
	
	if (argc > 2)
		verbose = 1;
#endif

	pid = fork();

	switch(pid) {
		case 0:
			pid = getpid();
			break;
		case -1:
			printf("INFO: test %s: fork failed (errno %d), running single-process test\n", argv[0], errno);
			
	}

	srandom(time(0));

	filename = tempnam(NULL, "seekr");
	fd = open(filename, O_CREAT|O_TRUNC|O_RDWR, 0666);
	if (fd < 0) {
		printf("INFO: test %s: open of file %s failed, must abort before performing truncation test\n", argv[0], filename, errno);
		exit(1);
	}

	signal(SIGINT, do_unlink);
	signal(SIGHUP, do_unlink);
	signal(SIGQUIT, do_unlink);

	while (--iterations >= 0) {
		seekpoint = random() & SEEKMASK;
		result = lseek(fd, seekpoint, SEEK_SET);
		if (result != seekpoint) {
			perror("lseek");
			unlink(filename);
			exit(1);
		}

		writesz = random() & WRITEMASK;
		if (writesz + seekpoint > BSZ)
			writesz = BSZ - seekpoint;

		c = (random() & CHARMASK) + 0x20;

		out = outbuf + seekpoint;
		i = writesz;
		while (--i >= 0)
			*out++ = c;

		if (verbose)
			fprintf(stderr,
				"process %d at %d writing %d bytes of %c\n",
				pid, seekpoint, writesz, c);

		result = write(fd, outbuf + seekpoint, writesz);
		if (result != writesz) {
			printf("ERROR: test %s: write failed, errno %d\n",
				argv[0], errno);
			unlink(filename);
			exit(1);
		}

		sginap(random() & SLEEPMASK);

		result = lseek(fd, 0, SEEK_SET);
		if (result != 0) {
			printf("ERROR: test %s: lseek failed, errno %d\n",
				argv[0], errno);
			unlink(filename);
			exit(1);
		}
		readsz = read(fd, inbuf, BSZ);
		if (verbose)
			fprintf(stderr, "process %d read %d bytes\n",
				pid, readsz);
		out = outbuf;
		in = inbuf;

		for (i = 0; i < readsz; i++) {
			if (*out != *in) {
				int rfd;
				printf("ERROR: test %s: file %s data mismatch at %d, got %c, not %c\n",
					argv[0], filename, i, *in, *out);
#ifdef notdef
				filename = tempnam(NULL, "seekw");
				rfd = open(filename,
					   O_CREAT|O_TRUNC|O_RDWR, 0666);
				if (rfd < 0) {
					perror("open compare file");
					exit(1);
				}
				fprintf(stderr, "dumping to compare file %s\n",
					filename);
				result = write(rfd, outbuf, readsz);
				if (result != readsz) {
					perror("write compare file");
					unlink(filename);
				}
#else
				unlink(filename);
#endif
				exit(1);
			}
			out++, in++;
		}

		truncsz = random() & SEEKMASK;
		if (truncsz < readsz) {
			if (ftruncate(fd, truncsz)) {
				perror("truncate");
				unlink(filename);
				exit(1);
			}
			if (verbose)
				fprintf(stderr,
					"process %d truncated to %d bytes\n",
					pid, truncsz);

			out = outbuf + truncsz;
			i = BSZ - truncsz;
			while (--i >= 0)
				*out++ = 0;
		}

		if (random() & 1) {
			close(fd);
			fd = open(filename, O_RDWR);
			if (fd < 0) {
				printf("ERROR: test %s: open failed, errno %d\n", argv[0], errno);
				unlink(filename);
				exit(1);
			}
		}

	}

	unlink(filename);
}

__sigret_t
do_unlink(__sigargs sig)
{
	unlink(filename);
	exit(0);
}
