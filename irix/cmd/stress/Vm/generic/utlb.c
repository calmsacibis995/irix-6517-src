#ident "$Revision: 1.2 $"

#include "stdio.h"
#include "malloc.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/times.h"

/*
 * utlb - measure utlbmiss performace
 */
int cycle = 60;		/* nS per cycle */
int lcycles = 13;	/* # cycles for uncached read */

main(argc, argv)
int argc;
char **argv;
{
	extern int getopt();
	extern char *optarg;
	int ocycles, c;
	register int nrefs;
	register int size;
	struct tms stimes, etimes;
	long stime, etime;
	register char *b;
	volatile char *cacheaddr, *ucacheaddr1, *ucacheaddr2;
	register int i, off, stride;
	register int j, ncached, nucached;
	char junk;
	float els, elu;

	/* default parameters */
	nrefs = 1000000;		/* # memory references */
	size = 256*1024;
	stride = 4096;
	/*stride = (3*4096)+27;*/
	ncached = 10;
	nucached = 2;

	while ((c = getopt(argc, argv, "s:c:u:n:")) != EOF)
	switch (c) {
	case 'u':	/* # uncached references/ref */
		nucached = atoi(optarg);
		break;
	case 'c':	/* # cached references/ref */
		ncached = atoi(optarg);
		break;
	case 's':
		size = atoi(optarg) * 1024;
		break;
	case 'n':
		nrefs = atoi(optarg);
		break;
	default:
		break;
	}

	/* now get enough memory */
	cacheaddr = (volatile char *) malloc(256*1024 + 32);
	ucacheaddr1 = cacheaddr + 16;
	ucacheaddr2 = ucacheaddr1 + (256*1024);
	junk = *cacheaddr;
	junk = *ucacheaddr1;
	junk = *ucacheaddr2;

	b = (char *)malloc(size);

	/* fault it in */
	for (i = 0; i < size; i += 1024)
		*(b + i) = 0;

	/* walk through memory */
	stime = times(&stimes);

	off = 0;
	/*\
	 * Basic overhead:
	 * 13 cycles excluding loops, including sw of b+off
	 * exact if both ncached & nucached == 0
	 * add 1 more cycle each if non-zero
	 * 3*ncached - excluding lb itself
	 * 9*nucached - excluding lb itself
	 */
	for (i = 0; i < nrefs; i++) {
		*(uint *)(b + off) = 1;

		/* do cached/uncached refs */
		for (j = 0; j < ncached; j++)
			junk = *cacheaddr;
		for (j = 0; j < nucached; j++)
			if (j & 1)
				junk = *ucacheaddr1;
			else
				junk = *ucacheaddr2;
		off += stride;
		if (off >= size)
			off = size - off;
	}
	etime = times(&etimes);

	els = (float)(etimes.tms_stime - stimes.tms_stime)/(float)HZ;
	elu = (float)(etimes.tms_utime - stimes.tms_utime)/(float)HZ;

	printf("#ref:%d System:%.3f mS User:%.3f mS\n",
		nrefs, els*1000.0, elu*1000.0);
	
	printf("Per Ref: addn'l cached:%d uncached:%d System:%.3f uS User:%.3f uS\n",
		ncached, nucached,
		(els * 1000000.0)/(float)nrefs,
		(elu * 1000000.0)/(float)nrefs);
	ocycles = 13 + 3*ncached + 9*nucached + (ncached ? 1 : 0) +
			(nucached ? 1 : 0);
	printf("Cycles: excluding loads:per ref:%d = %d nS +loads:%d nS\n",
		ocycles,
		ocycles * cycle,
		(ocycles + ncached + nucached * lcycles) * cycle);
}
