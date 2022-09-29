#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <bstring.h>
#include <string.h>
#include <signal.h>
#include "stress.h"
#include <getopt.h>
#include <time.h>
#include "sys/times.h"

char *Cmd;
int verbose;
static unsigned long dspec(timespec_t *start, timespec_t *end);

int
main(int argc, char *argv[])
{
	struct tms tm;
	int c, fd;
	unsigned long nbytes;
	unsigned long long totaltime;
	int mappings;
	size_t size, pgsize;
	void *vaddr;
	struct stat sb;
	clock_t tstart, ti;
	timespec_t res, start, end;
	char *file = "/unix";
	int maxmaps = 100000000;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "vf:n:")) != EOF)
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'f':
			file = optarg;
			break;
		case 'n':
			maxmaps = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage:%s\n", Cmd);
			exit(0);
			/* NOTREACHED */
		}
	if ((fd = open(file, O_RDONLY)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "cannot open %s", file);
		/* NOTREACHED */
	}
	fstat(fd, &sb);
	pgsize = getpagesize();
	size = sb.st_size & ~(pgsize-1);
	clock_getres(CLOCK_SGI_CYCLE, &res);
	printf("%s:resolution %dS %dnS. mapping 0x%x bytes per\n",
			Cmd, res.tv_sec, res.tv_nsec, size);

	nbytes = 0;
	mappings = 0;
	totaltime = 0;
	tstart = times(&tm);
	while (maxmaps--) {
		clock_gettime(CLOCK_SGI_CYCLE, &start);
		vaddr = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
		clock_gettime(CLOCK_SGI_CYCLE, &end);
		if (vaddr == MAP_FAILED) {
			break;
		}
		if (verbose)
			printf("%s:mapped at 0x%11lx %14dnS\n",
					Cmd, vaddr, dspec(&start, &end));
		mappings++;
		nbytes += size;
		totaltime += dspec(&start, &end);
	}
	printf("%s:total mappings %d space %d bytes %d Mb\n", 
		Cmd, mappings, nbytes, nbytes / (1024*1024));
	printf("%s:total time %lldnS(%lldmS)\n",
			Cmd, totaltime, totaltime / (1024*1024));
	ti = times(&tm) - tstart;
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("%s:elapsed time for %d mappings %d mS or %d uS per\n",
		Cmd, mappings, ti, (ti*1000)/(clock_t)mappings);
	return 0;
}

static unsigned long
dspec(timespec_t *start, timespec_t *end)
{
	unsigned long long sn, en;
	unsigned long tn;

	sn = (start->tv_sec * (1024 * 1024 * 1024LL)) + start->tv_nsec;
	en = (end->tv_sec * (1024 * 1024 * 1024LL)) + end->tv_nsec;

	tn = (unsigned long)(en - sn);
	return tn;
}
