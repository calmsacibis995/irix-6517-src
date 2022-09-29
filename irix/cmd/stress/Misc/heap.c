/*
 * This is a kernel heap driver.
 *
 * Calls kmem_alloc/kmem_realloc/kmem_free through syssgi back door
 * that is only available when the kernel is compiled -DHEAPDEBUG.
 * For this reason, the test isn't run by the normal runtests script.
 *
 * Test also opens and scribbles on memory via /dev/kmem (fix for
 * Everest?), so must have superuser privilege.
 *
 * usage: heap [-v] [-s size] [-n narenas] [-f flags] [-S summary]
 *
 * where	-v: verbosity
 * 		-s: max size of an alloc/realloc
 * 		-n: max number of active allocations
 * 		-f: various VM_ flags (VM_DIRECT, VM_NOSLEEP, etc.)
 * 		-s: gives a summary evern ``summary'' operations
 */
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/immu.h>
#include <unistd.h>

#define NARENAS	512
#define SZ	8192*2

static int pagesize;
#undef _PAGESZ
#define _PAGESZ		pagesize

#ifndef NBPP
#define NBPP	_PAGESZ
#endif

struct arena {
	daddr_t	addr;
	int	size;
	char	pattern;
	char	action;
};

struct arena *arenas;

#define PLUS 1
#define MOIN 2

char *buf;
long sz;
long narenas;
int flags = 0;
int verbose;
long heaptotal;
extern int errno;
int summary;
unsigned long allocs, reallocs, frees;
int allocated_slots;

void
summarize(char *s)
{
	fprintf(stderr, "%s: max-alloc-size %d, %d arenas, flags = 0x%x\n",
		s, sz, narenas, flags);
	fprintf(stderr, "	%d allocs %d reallocs %d frees; heap=%d in %d slots\n",
		allocs, reallocs, frees, heaptotal, allocated_slots);
}

void
usage(char *s)
{
	fprintf(stderr,
	"usage: %s [-v] [-s size] [-n narenas] [-f flags] [-S summary]\n", s);

	fprintf(stderr, "	size: max size of alloc/realloc\n");
	fprintf(stderr, "	narenas: max number of extant allocs\n");
	fprintf(stderr, "	flags: flags to kmem_alloc/kmem_realloc\n");
	fprintf(stderr, "	summary: summaries given every summary operation\n");
}

main(argc, argv)
char **argv;
{
	register int aindx, size;
	register daddr_t addr;
	register int i;
	register char pattern;
	register int ops = 0;
	extern char *optarg;
	extern int optind;
	void hangup();
	int fd;
	int c;

	pagesize = getpagesize();
	narenas = NARENAS;
	sz = SZ;

	while ((c = getopt(argc, argv, "vs:n:f:S:")) != EOF)
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
			case 'n':
				narenas = strtol(optarg, (char *)0, 0);
				if (narenas & (narenas-1)) {
					usage(argv[0]);
					fprintf(stderr,
						"narenas must be power of two\n");
					exit(1);
				}
				break;
			case 'f':
				flags = strtol(optarg, (char *)0, 0);
				break;
			case 'S':
				summary = strtol(optarg, (char *)0, 0);
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
	fprintf(stderr, "%s: max-alloc-size %d, %d arenas, flags = 0x%x\n",
		argv[0], sz, narenas, flags);

	buf = (char *)calloc(1, sz);
	arenas = (struct arena *)calloc(sizeof(struct arena), narenas);

	(void)srand(time(0));
	fd = open("/dev/kmem", O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}
	syssgi(1007, fd, 0);	/* seek */
	if (errno) {
		perror("syssgi 1007 (lseek)");
		exit(1);
	}

	signal(SIGHUP, hangup);
	signal(SIGINT, hangup);

	while (1) {
		aindx = rand() & (narenas-1);

		if (addr = arenas[aindx].addr) {

			size = arenas[aindx].size;

			if (size <= 0) {
				fprintf(stderr, "size == %d!\n", size);
				hangup();
			}

			i = rand();
			if ((i & 0x3) != 0x3) {
				heaptotal -= size;

				if (verbose)
				printf("free: 0x%x / 0x%x [%d]\n",
					addr, size, heaptotal);

				arenas[aindx].addr = 0;
				arenas[aindx].size = 0;
				syssgi(1004, addr);	/* kern_free */

				if (summary) {
					frees++;
					allocated_slots--;
					if (++ops >= summary) {
						ops = 0;
						summarize(argv[0]);
					}
				}

				arenas[aindx].action = 0;
				continue;
			}

			pattern = arenas[aindx].pattern;
			if (check(fd, addr, size, pattern,
				  "read pattern failed"))
			{
				hangup();
			}

			/* pick new size */

			i = rand() & (sz-1);
			if (i == 0)
				continue;

			if (verbose)
			printf("realloc: 0x%x / 0x%x -> 0x%x ...",
				addr, size, i);

			arenas[aindx].size = i;
			arenas[aindx].addr = 0;

			/* kmem_realloc */
			arenas[aindx].addr = syssgi(1003, addr, i, flags);

			if (errno) {
				perror("syssgi 1003 (kmem_realloc)");
				hangup();
			}

			if (!arenas[aindx].addr) {
				arenas[aindx].addr = addr;
				arenas[aindx].size = size;
				if (verbose)
				printf("realloc: 0x%x -> 0x%x ... NULL [%d]\n",
					size, i, heaptotal);

				if (!(flags & VM_NOSLEEP)) {
					fprintf(stderr, "error: null return from syssgi 1003 (kmem_realloc)\n");
					hangup();
				}

			} else {

				heaptotal -= size;
				heaptotal += i;
				if (verbose)
				printf(" 0x%x [%d]\n",
					arenas[aindx].addr, heaptotal);

				if (summary) {
					reallocs++;
					if (++ops >= summary) {
						ops = 0;
						summarize(argv[0]);
					}
				}
			}

			if (size < arenas[aindx].size)
				i = size;
			else
				i = arenas[aindx].size;

			if (check(fd, arenas[aindx].addr, i,
					pattern, "read after realloc")) {
				fprintf(stderr, "was %d, now %d, read %d ",
					size, arenas[aindx].size, i);
				switch (arenas[aindx].action) {
					case PLUS:
						fprintf(stderr, "had grown");
						break;
					case MOIN:
						fprintf(stderr, "had shrunk");
						break;
				}
				fprintf(stderr, "\n");
				hangup();
			}

			if (size < arenas[aindx].size) {
				size = arenas[aindx].size - size;
				for (i = 0; i < size; i++)
					buf[i] = pattern;
				/*
				 * check() does lseek and read, so offset
				 * is positioned correctly for write
				 */
				i = write(fd, buf, size);
				if (i != size) {
					perror("write on realloc");
					hangup();
				}
				if (check(fd, arenas[aindx].addr,
					arenas[aindx].size, pattern,
					"write after realloc")) {
					hangup();
				}
				arenas[aindx].action = PLUS;
			} else
				arenas[aindx].action = MOIN;
		} else {
			size = rand() & (sz-1);
			if (size == 0)
				continue;

			arenas[aindx].size = size;
			arenas[aindx].action = 0;

			arenas[aindx].pattern = pattern = (rand() & 63) + '0';
			for (i = 0; i < size; i++)
				buf[i] = pattern;

			/* kmem_alloc */
			addr = arenas[aindx].addr = syssgi(1002, size, flags);
			if (errno) {
				perror("syssgi 1002 (kmem_alloc)");
				hangup();
			}

			if (summary) {
				allocs++;
				if (addr)
					allocated_slots++;
				if (++ops >= summary) {
					ops = 0;
					summarize(argv[0]);
				}
			}

			/*
			 * If passed VM_NOSLEEP, could have returned NULL.
			 */
			if (addr) {
				heaptotal += size;
				if (verbose)
				printf("alloc: 0x%x ... 0x%x [%d]\n",
					size, addr, heaptotal);

				errno = 0;
				syssgi(1007, fd, addr);	/* seek */
				if (errno) {
					perror("syssgi 1007 (lseek)");
					fprintf(stderr, "addr was 0x%x\n", (int)addr);
					hangup();
				}
				i = write(fd, buf, size);
				if (i != size) {
					perror("write");
					hangup();
				}
			} else {
				if (verbose)
				printf("alloc: 0x%x ... NULL [%d]\n",
					size, heaptotal);

				if (!(flags & VM_NOSLEEP)) {
					fprintf(stderr, "error: null return from syssgi 1002 (kmem_alloc)\n");
					hangup();
				}
			}
		}
	}
}

void
hangup()
{
	register int i, addr;

	for (i = 0; i < narenas; i++)
		if (addr = arenas[i].addr) {
			arenas[i].addr = 0;
			syssgi(1004, addr);
		}

	exit(1);
}

check(fd, addr, size, pattern, msg)
	register int fd;
	register daddr_t addr;
	register int size;
	register char pattern;
	register char *msg;
{
	register int i;
	register char *p = buf;

	if (addr == NULL)
		return 0;

	errno = 0;
	syssgi(1007, fd, addr); /* seek */
	if (errno) {
		perror("syssgi 1007 (lseek)");
		fprintf(stderr, "addr was 0x%x\n", addr);
		exit(1);
	}
	i = read(fd, buf, size);
	if (i != size) {
		perror("read");
		exit(1);
	}

	while (--i > 0)
		if (*p++ != pattern) {
			fprintf(stderr, "%s!\n", msg);
			--p;
			fprintf(stderr, "expected %c, got %c, ", pattern, *p);
			fprintf(stderr, "addr 0x%x size 0x%x, offset 0x%x\n",
				addr, size, size - i - 1);
			return(1);
		}
	return 0;
}
