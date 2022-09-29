/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.13 $"

#include "malloc.h"
#include "errno.h"
#include "stdio.h"
#include "wait.h"
#include "getopt.h"
#include "signal.h"
#include "sys/types.h"
#include "sys/times.h"
#include "sys/prctl.h"
#include "sys/schedctl.h"
#include "limits.h"
#include "stress.h"
#include "string.h"
#include "stdlib.h"
#include "unistd.h"

struct f {
	char *addr;
	int size;
};

volatile int stopit = 0;
int blocksize;			/* M_BLOCKSZ value */
int fragsize = -1;			/* M_FRAG value */
int alignsize;			/* M_GRAIN value */
int nprocs = 1;
int maxptrs = 40000;
float maxavg, minavg;
static void slave(void *);
static void doit(__psint_t);
void Usage(void);
void dumpmi(register struct mallinfo *mi);
int mallocit(int, int, int, int, struct f *, int *, int *, int *);
int verbose;
char *Cmd;

int
main(int argc, char **argv)
{
	int i, c;
	extern void quitit(), catchit();
	__psint_t nloops = -1;
#ifdef LIBMALLOC
	int smallsize = -1;
	int debug = 0;
#endif

	Cmd = strdup(argv[0]);
	maxavg = 0;
	minavg = 100000;
	while ((c = getopt(argc, argv, "vp:n:s:a:l:f:b:d:")) != -1)
		switch (c) {
		case 'p':
			maxptrs = atoi(optarg);
			break;
		case 'n':
			nprocs = atoi(optarg);
			break;
		case 'b':
			blocksize = atoi(optarg);
			break;
		case 'f':
			fragsize = atoi(optarg);
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
#ifdef LIBMALLOC
		case 's':
			smallsize = atoi(optarg);
			break;
		case 'd':
			debug = atoi(optarg);
			break;
#endif
		case 'a':
			alignsize = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		default:
			Usage();
			/* NOTREACHED */
		}

	printf("%s:nloops %d nprocs %d\n", Cmd, nloops, nprocs);
#ifdef LIBMALLOC
#ifdef M_BLKSZ
	if (blocksize) {
		printf("Sbrk BLOCKSZ:%d\n", blocksize);
		mallopt(M_BLKSZ, blocksize);
	}
#endif
#ifdef M_MXCHK
	if (fragsize != -1) {
		printf("Max number of blocks to check:%d\n", fragsize);
		mallopt(M_MXCHK, fragsize);
	}
#endif
#ifdef M_CLRONFREE
	mallopt(M_CLRONFREE, 0xde);
#endif
	if (smallsize != -1) {
		printf("Size of small block:%d\n", smallsize);
		mallopt(M_MXFAST, smallsize);
	}
	if (alignsize) {
		printf("Alignment size:%d\n", alignsize);
		mallopt(M_GRAIN, alignsize);
	}
	if (debug) {
		printf("m_debug set to :%d\n", debug);
		mallopt(M_DEBUG, debug);
	}
#endif /* LIBMALLOC */

	sigset(SIGINT, catchit);
	sigset(SIGQUIT, quitit);
	if (usconfig(CONF_INITUSERS, nprocs) < 0) {
		errprintf(1, "INITUSERS");
		/* NOTREACHED */
	}

	if (schedctl(NDPRI, 0, 60) != 0)
	       printf("%s:Could not give good priority - continuing", Cmd);

	/*
	 * in case parent decides to exit - this makes sure that all
	 * kids will be killed
	 */
	parentexinit(0);

	for (i = 1; i < nprocs; i++) {
		if (sproc(slave, PR_SALL, nloops) < 0) {
			if (errno != EAGAIN) {
				errprintf(ERR_ERRNO_EXIT, "sproc failed");
				/* NOTREACHED */
			}
			printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
			/*
			 * GANG below will fail if NO sprocs get created -
			 * just bail 
			 */
			exit(0);
		}
	}
	if ((nprocs > 1) && schedctl(SCHEDMODE, SGS_GANG) < 0) {
		errprintf(ERR_ERRNO_EXIT, "schedctl - GANG");
	}

	doit(nloops);
	errno = 0;
	while (wait(NULL) >= 0 || errno != ECHILD)
		errno = 0;
	printf("\n%s:Max avg time:%fuS Min avt time:%fuS\n",
		Cmd, maxavg, minavg);
	return 0;
}

static void
slave(void *nl)
{
	slaveexinit();
	doit((__psint_t)nl);
}

static void
doit(__psint_t nloops)
{
#ifdef LIBMALLOC
	struct mallinfo mi;
#endif
	int n, span, start, size;
	struct tms bt, et;
	clock_t smsec, umsec;
	float avgumsec;
	int iter = 0;
	struct f *ptrs;
	auto int totmallocs = 0;
	auto int totfrees = 0;
	auto int usesize = 0;

	if(!(ptrs = calloc(maxptrs, sizeof(*ptrs)))) {
		errprintf(3, "calloc of initial pointers (%d bytes) failed\n",
			maxptrs*sizeof(*ptrs));
		return;
	}
		
	while (--nloops) {
		span = (rand() % 5) + 1;
		start = rand() % 5;
		do {
			size = rand() % 140;
		} while (size == 0);
		if (verbose)
			printf("\n#%d start:%d span:%d size:%d\n", iter, start, span, size);
		times(&bt);
		n = mallocit(start, maxptrs, span, size, ptrs, &totmallocs,
			&totfrees, &usesize);
		times(&et);
		umsec = ((et.tms_utime - bt.tms_utime) * 1000)/(clock_t)CLK_TCK;
		smsec = ((et.tms_stime - bt.tms_stime) * 1000)/(clock_t)CLK_TCK;
#ifdef LIBMALLOC
		if (verbose) {
			mi = mallinfo();
			dumpmi(&mi);
		}
#endif
		if (verbose)
			printf("%s:totmallocs:%d totfrees:%d usesize:%d\n",
				Cmd, totmallocs, totfrees, usesize);

		avgumsec = ((float)umsec/(float)n) * 1000;
		if (avgumsec > maxavg)
			maxavg = avgumsec;
		if (avgumsec < minavg)
			minavg = avgumsec;
		if (verbose)
			printf("%s:utime:%dmS avg:%fuS stime:%dmS avg:%fuS\n",
				Cmd, umsec, avgumsec,
				smsec, ((float)smsec/(float)n) * 1000);
		if (stopit)
			break;
		iter++;
	}
}

void
Usage(void)
{
	fprintf(stderr, "Usage:mtest [-vd][-s smallsize] [-a alignsize] [-l nloops] [-b blocksize] [-f nmbr-of-free-frags-to-check][-n nprocs][-p max mallocs\n");
	exit(-1);
}

int
mallocit(
	int start,	/* which index to start at */
	int num,	/* # to do */
	int span,	/* how many to skip */
	int size,	/* how many bytes */
	struct f *ptrs,
	int *totmallocs,
	int *totfrees,
	int *usesize)
{
	register int i, n;
#ifdef LIBMALLOC
	size_t psize;
#endif
	extern int md;

	n = 0;
	for (i = start; i < num; i += span) {
		++n;
		if (i >= maxptrs)
			break;
		if (ptrs[i].addr) {
			(*totfrees)++;
			*usesize = *usesize - ptrs[i].size;
#ifdef LIBMALLOC
			psize = mallocblksize(ptrs[i].addr);
			if (psize < ptrs[i].size)
				errprintf(0, "bad return size for ptr @ 0x%x was %d stored as %d",
					ptrs[i].addr, psize, ptrs[i].size);
				/* NOTREACHED */
#endif
			free(ptrs[i].addr);
#ifdef LIBMALLOC
			if (nprocs == 1)
				if (*(ptrs[i].addr + psize - 1) != 0xde)
					errprintf(0, "not cleared on free for ptr @ 0x%x",
						ptrs[i].addr);
					/* NOTREACHED */
#endif
		}
		if ((ptrs[i].addr = malloc(size)) == NULL) {
			errprintf(3, "malloc of %d failed\n", size);
			errprintf(2, "totmallocs:%d totfrees:%d usesize:%d\n",
				*totmallocs, *totfrees, *usesize);
			/* NOTREACHED */
		}
		ptrs[i].size = size;
		*usesize = *usesize + ptrs[i].size;
		(*totmallocs)++;
		if (stopit)
			return(n);
	}
	return(n);
}

#ifdef LIBMALLOC
void
dumpmi(register struct mallinfo *mi)
{
	printf("Mallinfo:arena:%d ordblks:%d smblks:%d hblkhd:%d\n",
		mi->arena, mi->ordblks, mi->smblks, mi->hblkhd);
	printf("\thblks:%d usmblks:%d fsmblks:%d uordblks:%d\n",
		mi->hblks, mi->usmblks, mi->fsmblks, mi->uordblks);
	printf("\tfordblks:%d keepcost:%d\n",
		mi->fordblks, mi->keepcost);
}
#endif

void
quitit()
{
	signal(SIGQUIT, SIG_IGN);
	printf("%s:caught signal\n", Cmd);
	stopit++;
}

void
catchit()
{
	printf("\n%s:Max avg time:%fuS Min avt time:%fuS\n",
		Cmd, maxavg, minavg);
}
