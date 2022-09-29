/*
 * This is used to test the segment manager.
 *
 * Massive numbers of randomly-sized and randomly-placed mappings
 * are created in and removed from the user's address space.
 * This will, indirectly, test kernel segment handling, especially
 * avail[rs]mem accounting.
 *
 * usage: segment [-v] [-s size] [-i iterations]
 *
 * where	-v: verbosity
 * 		-s: max size of an mmap/munmap
 * 		-i: number of mmap/munmap calls
 */
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/immu.h>
#include <sys/mman.h>
#include <unistd.h>

#define NPGS	0x20000
#define ITERATIONS	100000

long sz;
int verbose;
int niterations;
extern int errno;

void
usage(char *s)
{
	fprintf(stderr,
	"usage: %s [-v] [-s size] [-i iterations]\n", s);

	fprintf(stderr, "	size: max size, in pages, of mmap/munmap\n");
	fprintf(stderr, "	iterations: number of mmap/munmap calls\n");
}

main(argc, argv)
char **argv;
{
	register int i;
	long mapsize;
	extern char *optarg;
	extern int optind;
	char *filename;
	char *startvaddr;
	char *mapvaddr;
	int c;
	int off;
	int fd;
	int pagesize = getpagesize();

	sz = NPGS;
	niterations = ITERATIONS;

	while ((c = getopt(argc, argv, "vs:i:")) != EOF)
		switch (c) {
			case 'v':
				verbose = 1;
				break;
			case 's':
				sz = strtol(optarg, (char *)0, 0);
				if (sz & (sz-1)) {
					usage(argv[0]);
					fprintf(stderr,
						"size must be power of two\n");
					exit(1);
				}
				break;
			case 'i':
				niterations = strtol(optarg, (char *)0, 0);
				if (niterations <= 0) {
					usage(argv[0]);
					fprintf(stderr,
						"interations must be > 0\n");
					exit(1);
				}
				break;
			case '?':
				usage(argv[0]);
				exit(1);
				break;
		}

	if (optind < argc) {
		usage(argv[0]);
		fprintf(stderr, "unknown arguments: ");
		for ( ; optind < argc; optind++)
			(void)fprintf(stderr, " %s", argv[optind]);
		fprintf(stderr, "\n");
		exit(1);
	}

	sz = sz * 2;
	if (verbose)
		printf("%s: mmap/munmap %d iterations, max-alloc-size %d pages\n", argv[0], niterations, sz);

	(void)srand(time(0));

	filename = tempnam(NULL, "SEGVM");
	fd = open(filename, O_RDWR|O_CREAT, 0666);
	if (fd < 0) {
		perror("open");
		exit (1);
	}

	unlink(filename);

	mapsize = sz * pagesize;

	/*
	 * If (sz * pagesize) causes an overflow, then repeatedly reduce
	 * sz by half until we get a non-overflowing result.
	 */

	while (mapsize <= 0) {
		sz = sz / 2;
		mapsize = sz * pagesize;
	}

	for ( ; ; ) {
		startvaddr = (char *)mmap(0, mapsize, PROT_READ,
				MAP_SHARED|MAP_AUTOGROW, fd, 0);

		if ((int)startvaddr >= 0) {
			munmap((void *)startvaddr, mapsize);
			if (verbose)
				printf("%s: carved out 0x%x, 0x%x [%d]\n",
					argv[0], startvaddr, mapsize, sz);
			break;
		}

		if (verbose)
			printf("%s: mmap failed for %d pages\n", argv[0], sz);

		mapsize = mapsize / 2;
		sz = sz / 2;
		if (sz == 0) {
			fprintf(stderr, "%s: shriveled to nothing!\n", argv[0]);
			exit(1);
		}
	}

	sz = sz / 2;
	if (verbose)
		printf("%s: max-alloc-size %d pages\n", argv[0], sz);

	while (--niterations >= 0) {
		mapsize = rand() & (sz-1);
		mapsize /= 2;

		if (mapsize == 0)
			continue;

		off = rand() & (sz-1);
		off *= pagesize;
		mapvaddr = startvaddr + off;

		i = rand();
		if ((i & 0x3) == 0x3) {

			i = munmap(mapvaddr, mapsize * pagesize);

			if (verbose) {
				if (i == 0)
					printf("munmap: 0x%x, 0x%x pages, ok\n",
						mapvaddr, mapsize);
				else
					printf("munmap: 0x%x, 0x%x pages, errno %d\n",
						mapvaddr, mapsize, errno);
			}

		} else {

			mapvaddr = (char *)mmap(mapvaddr, mapsize * pagesize,
					PROT_READ,
					MAP_SHARED|MAP_AUTOGROW|MAP_FIXED,
					fd, 0);

			if (verbose)
				printf("mmap: 0x%x, 0x%x pages => %x\n",
					startvaddr + off, mapsize, mapvaddr);
		}
	}
}
