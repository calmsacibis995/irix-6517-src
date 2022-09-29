/*
 * Kernel memory alloc/free/realloc tester.
 *
 * Needs to be run on a system with -DHEAPDEBUG defined on
 * compile of os/syssgi.c.
 */
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <unistd.h>
#include <sys/kmem.h>

#define NARENAS	512
#define SZ	8192

struct arena {
	daddr_t	addr;
	int	size;
	int	flags;
	int	value;
} arenas[NARENAS];

extern int errno;
int verbose = 0;

void
hangup(void)
{
	register int i, addr;

	for (i = 0; i < NARENAS; i++)
		if (addr = arenas[i].addr) {
			arenas[i].addr = 0;
			syssgi(1004, addr);
		}
	exit(1);
}

void
main(argc, argv)
char **argv;
{
	register int aindx, size, addr;
	register int i;
	register char pattern;
	int fd;

	if (argc > 1)
		if (strcmp(argv[1], "-v") == 0) {
			verbose = 1;
			printf("VERBOSE MODE\n");
		}
	/*
	 * XXX fix to take narena and size args!
	 */
#ifdef randomize
	(void)srand(time(0));
#endif

	signal(SIGHUP, hangup);
	signal(SIGINT, hangup);

	while (1) {
		aindx = random() % NARENAS;

		if (addr = arenas[aindx].addr) {
			/* check old data */
			if (syssgi(1008, arenas[aindx].addr, arenas[aindx].size,
				   arenas[aindx].value))
			{
				perror("syssgi 1008");
				printf("arena %d, addr=0x%x, size=%d (%d), value=0x%x\n",
				       aindx, arenas[aindx].addr, arenas[aindx].size,
				       arenas[aindx].size / 4, arenas[aindx].value);
				hangup();
			}

			/* either free or realloc */
			i = random();
			if ((i % 4) != 3) {
				syssgi(1004, addr);
	if (verbose)
	{
	printf("FREE    arena %d, addr=0x%x, size=0x%x, value=0x%x, flags=0x%x\n",
	       aindx, arenas[aindx].addr, arenas[aindx].size,
	       arenas[aindx].value, arenas[aindx].flags);
	fflush(stdout);
	}
				arenas[aindx].size = 0;
				arenas[aindx].addr = 0;
				continue;
			}

			/* check old size */
			size = arenas[aindx].size;
			if (size <= 0) {
				fprintf(stderr, "size == %d!\n", size);
				hangup();
			}

			/* pick new size and realloc */
			size = (1 + random() % SZ) & ~3;
			if (size == 0)
				continue;
			arenas[aindx].size = size;
			addr = arenas[aindx].addr =
					syssgi(1003, addr, size,
					       arenas[aindx].flags);
			if (errno) {
				perror("syssgi 1003");
				hangup();
			}

			/* fill with new value */
			i = random();
			if (syssgi(1009, addr, size, i)) {
				perror("syssgi 1009");
				hangup();
			}
			arenas[aindx].value = i;
	if (verbose)
	{
	printf("REALLOC arena %d, addr=0x%x, size=0x%x, value=0x%x, flags=0x%x\n",
	       aindx, arenas[aindx].addr, arenas[aindx].size,
	       arenas[aindx].value, arenas[aindx].flags);
	fflush(stdout);
	}
		} else {
			size = (1 + random() % SZ) & ~3;
			if (size == 0)
				continue;
			arenas[aindx].size = size;

			arenas[aindx].flags = 0;
			if (random() % 4  == 0)
				arenas[aindx].flags |= KM_CACHEALIGN;
			if (random() % 4 == 0)
				arenas[aindx].flags |= KM_PHYSCONTIG;

			addr = arenas[aindx].addr =
					syssgi(1002, size,
					       arenas[aindx].flags);
			if (errno) {
				perror("syssgi 1002");
				hangup();
			}
			errno = 0;

			i = random();
			if (syssgi(1009, addr, size, i)) {
				perror("syssgi 1009");
				hangup();
			}
			arenas[aindx].value = i;
	if (verbose)
	{
	printf("ALLOC   arena %d, addr=0x%x, size=0x%x, value=0x%x, flags=0x%x\n",
	       aindx, arenas[aindx].addr, arenas[aindx].size,
	       arenas[aindx].value, arenas[aindx].flags);
	fflush(stdout);
	}
		}
	}
}
