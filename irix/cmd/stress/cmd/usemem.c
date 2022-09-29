#include "stdlib.h"
#include "math.h"
#include "errno.h"
#include "stdio.h"
#include "unistd.h"
#include "getopt.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "sys/sysmp.h"
#include "sys/resource.h"

extern void Usage(void);
extern void play(char *b, size_t, int random);

main(int argc, char **argv)
{
	int i, c;
	size_t size;
	int nloops = 1000;
	char *base;
	int rndom = 0;
	size_t psize = 0;
	int oncpu = -1;
	double dsize =0.0, dpsize = 0.0;
	int forever = 0;
	long rsslimit = 0;
	struct rlimit64 rlim;

	while ((c = getopt(argc, argv, "Li:R:l:rb:k:m:B:K:M:")) != -1)
		switch (c) {
		case 'L':
			forever = 1;
			break;
		case 'r':
			rndom = 1;
			break;
		case 'm':
			dsize = atof(optarg) * 1024. * 1024.;
			break;
		case 'k':
			dsize = atof(optarg) * 1024.;
			break;
		case 'b':
			dsize = atof(optarg);
			break;
		case 'M':
			dpsize = atof(optarg) * 1024. * 1024.;
			break;
		case 'K':
			dpsize = atof(optarg) * 1024.;
			break;
		case 'B':
			dpsize = atof(optarg);
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
		case 'R':
			rsslimit = atoi(optarg);
			break;
		case 'i':
			oncpu = atoi(optarg);
			sysmp(MP_UNISOLATE, oncpu);
			if (sysmp(MP_MUSTRUN, oncpu) != 0) {
				fprintf(stderr, "cannot runon cpu %d\n", oncpu);
				exit(1);
			}
			break;
		default:
			Usage();
			exit(1);
		}

	if (rsslimit) {
		getrlimit64(RLIMIT_RSS, &rlim);

		rlim.rlim_cur = rsslimit * 1024 * 1024;

		if (setrlimit64(RLIMIT_RSS, &rlim) != 0) {
			fprintf(stderr, "usemem:setrlimit RSS failed\n");
			exit(1);
		}
		printf("RSS limit set to %d Mbytes\n", rsslimit);
	}

	size = (size_t)dsize;
	psize = (size_t)dpsize;
	if ((base = malloc(size)) == NULL) {
		fprintf(stderr, "cannot malloc %d bytes\n", size);
		exit(1);
	}

	if (psize) {
		if (psize > size)
			psize = size;
		if (mpin(base, psize) != 0) {
			fprintf(stderr, "cannot pin %d bytes:%s\n",
				psize, strerror(errno));
		}
		/* don't bother to play with pinned memory - its ther
		 * already
		 */
		base += psize;
		size -= psize;
	}
	/* now isolate .. */
	if (oncpu >= 0) {
		if (sysmp(MP_ISOLATE, oncpu) != 0) {
			fprintf(stderr, "cannot isolate cpu %d:%s\n",
				oncpu, strerror(errno));
			exit(1);
		}
	}

	if (forever) {
		for (;;)
			play(base, size, rndom);
	} else {
		for (i = 0; i < nloops; i++)
			play(base, size, rndom);
	}
	if (oncpu >= 0)
		sysmp(MP_UNISOLATE, oncpu);
	exit(0);
}

void
play(char *b, size_t s, int rndom)
{
	char *p;
	int i;
	volatile long sum = 0;

	if (rndom) {
		for (i = 0; i < s; i++) {
			p = b + (random() % s);
			sum += *p;
		}
	} else {
		for (i = 0, p = b; i < s; i++, p++)
			sum += *p;
	}
}

void
Usage(void)
{
	fprintf(stderr, "Usage:usemem [-[bB] bytes][-[kK] kbytes][-[mM] mbytes][-l loops][-r][i #][-R limit]\n");
	fprintf(stderr, "\t-[bkm]\t size of total alloc\n");
	fprintf(stderr, "\t-[BKM]\t size of total alloc to pin\n");
	fprintf(stderr, "\t-r\tplay in memory at random\n");
	fprintf(stderr, "\t-l #\tnumber of loops to run test\n");
	fprintf(stderr, "\t-L\tloop forever\n");
	fprintf(stderr, "\t-R #\tchange rss limit to # mbytes\n");
	fprintf(stderr, "\t-i #\trun process on processor and isolate\n");
}
