/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.10 $"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "wait.h"
#include "memory.h"
#include "bstring.h"
#include "malloc.h"
#include "getopt.h"
#include "fcntl.h"
#include "stress.h"
#include "sys/types.h"
#include "sys/mman.h"

/*
 * Basic amalloc checking
 */
int alignsizes[] = {8, 16, 64, 1024, 4096, 8192 };
int naligns = sizeof(alignsizes) / sizeof(int);
int arenasizes[] = {1024, 16*1024, 20000, 70000, 245346, 1060708 };
int narenas = sizeof(arenasizes) / sizeof(int);
int allocsizes[] = {32, 7, 127, 9876, -1 };
int nallocs = sizeof(allocsizes) / sizeof(int);

/*
 * small block checking
 */
int sarenasizes[] = {100708 };
int snarenas = sizeof(sarenasizes) / sizeof(int);
/* grains are aligned up to alignsz */
int sgrainsizes[] = {8, 24};
int sngrains = sizeof(sgrainsizes) / sizeof(int);

int verbose = 0;
int debug = 0;
int scribble = 1;
char *Cmd;
int blksz = 7152;	/* just to try something non-obvious */
int rblksz;

typedef struct al_s {
	void *addr;
	size_t len;		/* total length in bytes */
	size_t nalloced;	/* # bytes  no alloced */
	struct al_s *next;
} al_t;
al_t *al;
#define CHUNK	(1024*64)
int chunksz = CHUNK;
size_t alcur, almax;
int pgsz;

extern int domalloc(int alignz, int allocsz, int arenasz);
extern int dosmalloc(int arenasz, int grainsz, int mxfast);
extern void *mygrow(size_t s, void *ap);
extern void deletechunks(void);
extern void *newchunk(size_t);
extern void initchunks(size_t);
void usage(void);
void dumpmi(register struct mallinfo *mi);

int
main(int argc, char **argv)
{
	register int i, j, k, l;
	int c;

	Cmd = errinit(argv[0]);
	setlinebuf(stderr);
	setlinebuf(stdout);

	pgsz = getpagesize();
	while ((c = getopt(argc, argv, "b:da:s:t:v")) != EOF)
		switch (c) {
		case 'd':
			debug++;
			break;
		case 'a':
			alignsizes[0] = atoi(optarg);
			naligns = 1;
			break;
		case 't':
			arenasizes[0] = atoi(optarg);
			narenas = 1;
			break;
		case 'b':
			blksz = atoi(optarg);
			break;
		case 's':
			allocsizes[0] = atoi(optarg);
			nallocs = 1;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			exit(-1);
		}

	for (i = 0; i < narenas; i++)
		for (j = 0; j < nallocs; j++)
			for (k = 0; k < naligns; k++)
				domalloc(alignsizes[k],
					 allocsizes[j],
					 arenasizes[i]);

	/*
	 * Test small block handling
	 * We try every third maxfast 0 - 128
	 */
	for (i = 0; i < snarenas; i++)
		for (k = 0; k < sngrains; k++)
			for (l = 1; l < 128; l+=3)
				dosmalloc(sarenasizes[i], sgrainsizes[k], l);
	return 0;
}

int
domalloc(int alignsz, int allocsz, int arenasz)
{
	register void *ap, *p;
	char buf[1024];
	struct mallinfo mi;
	int n, asz;

	initchunks(arenasz);

	if (verbose)
		printf("Arena size %d alignment %d alloc size %d\n",
				arenasz, alignsz, allocsz);
	if ((ap = acreate(buf, sizeof(buf), 0, 0, mygrow)) == NULL) {
		errprintf(0, "acreate failed for %d bytes\n", arenasz);
	} else {
		if (debug)
			if (amallopt(M_DEBUG, 1, ap) != 0)
				errprintf(0, "M_DEBUG failed");
		rblksz = blksz;
		if (arenasz < (2 * blksz)) {
			rblksz = 512;
			if (amallopt(M_MXFAST, 0, ap) != 0)
				errprintf(0, "M_MXFAST failed");
		}
		if (amallopt(M_BLKSZ, rblksz, ap) != 0)
			errprintf(0, "M_BLKSZ failed");

		n = 0;
		if (allocsz < 0)
			asz = rand() % arenasz;
		else
			asz = allocsz;
		while (p = amemalign(alignsz, asz, ap)) {
			if ((long)p % alignsz)
				errprintf(0, "alignment error p:0x%x alignsz 0x%x\n",
					p, alignsz);
			if (scribble)
				memset(p, 0x3f, asz);
			if (allocsz < 0)
				asz = rand() % arenasz;
			else
				asz = allocsz;
			n++;
		}
		if (verbose)
			printf("alloced %d %d byte blocks. %d bytes total\n",
			n, allocsz, n * allocsz);
	}
	if (verbose) {
		mi = amallinfo(ap);
		dumpmi(&mi);
	}

	adelete(ap);
	deletechunks();
	return(0);
}

int
dosmalloc(int arenasz, int grainsz, int mxfast)
{
	register void *ap, *p;
	char buf[1024];
	struct mallinfo mi;
	int n, asz;

	initchunks(arenasz);

	if (verbose)
		printf("Arena size %d grain size %d mxfast %d\n",
				arenasz, grainsz, mxfast);
	if ((ap = acreate(buf, sizeof(buf), 0, 0, mygrow)) == NULL) {
		errprintf(0, "acreate failed for %d bytes\n", arenasz);
		return(-1);
	} else {
		if (debug)
			if (amallopt(M_DEBUG, 1, ap) != 0)
				errprintf(0, "M_DEBUG failed");
		if (amallopt(M_GRAIN, grainsz, ap) != 0)
			errprintf(0, "M_GRAIN failed");
		if (amallopt(M_MXFAST, mxfast, ap) != 0)
			errprintf(0, "M_MXFAST failed");
		if (arenasz < (2 * blksz))
			rblksz = 512;
		else
			rblksz = blksz;
		if (amallopt(M_BLKSZ, rblksz, ap) != 0)
			errprintf(0, "M_BLKSZ failed");
		n = 0;
		while ((asz = rand() % (mxfast+1)) == 0)
			;
		while (p = amalloc(asz, ap)) {
			if (scribble)
				bcopy((void *)&domalloc, p, asz);
			n++;
		}
		if (verbose)
			printf("alloced %d blocks.\n", n);
	}
	if (verbose) {
		mi = amallinfo(ap);
		dumpmi(&mi);
	}

	adelete(ap);
	deletechunks();
	return(0);
}

void
dumpmi(register struct mallinfo *mi)
{
	size_t size;
	printf("Mallinfo:arena:%d ordblks:%d smblks:%d hblkhd:%d\n",
		mi->arena, mi->ordblks, mi->smblks, mi->hblkhd);
	printf("\thblks:%d usmblks:%d fsmblks:%d uordblks:%d\n",
		mi->hblks, mi->usmblks, mi->fsmblks, mi->uordblks);
	printf("\tfordblks:%d keepcost:%d\n",
		mi->fordblks, mi->keepcost);
	size = mi->hblkhd + mi->usmblks + mi->fsmblks + mi->uordblks +
		mi->fordblks;
	if (mi->arena != size)
		errprintf(0, "\tsize %d != arena %d\n", size, mi->arena);
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-dv][-a sz][-s sz][-g sz][-t sz][-b sz]\n", Cmd);
	fprintf(stderr, "\t-a align size (power of 2)\n");
	fprintf(stderr, "\t-t total arena size\n");
	fprintf(stderr, "\t-g grain size\n");
	fprintf(stderr, "\t-s alloc size\n");
	fprintf(stderr, "\t-b block size\n");
	fprintf(stderr, "\t-v verbose\n");
	fprintf(stderr, "\t-d turn on malloc debug\n");
}

/* ARGSUSED */
void *
mygrow(size_t s, void *ap)
{
	void *r;

	if (s == 0 || ((s % rblksz) != 0)) {
		errprintf(ERR_EXIT, "grow function got 0 or non-block size request:%d",
			s);
		/* NOTREACHED */
	}
	r = newchunk(s);
	if (r == NULL)
		return (void *)-1L;

	memset(r, 0x2e, s);
	return r;
}

void
initchunks(size_t max)
{
	alcur = 0;
	almax = max;
}

void *
newchunk(size_t sz)
{
	al_t *nal, *alp;
	int fd;
	size_t tsz = sz;
	void *rv;

	/*
	 * if we've got enough left from a previous allocation - return
	 * that
	 */
	for (alp = al; alp; alp = alp->next) {
		if (sz <= alp->nalloced) {
			rv = (char *)alp->addr + (alp->len - alp->nalloced);
			alp->nalloced -= sz;
			return rv;
		}
	}

	/*
	 * round size up to chunksz as long as that doesn't exceed almax.
	 * In any case round up to pgsz
	 */
	tsz = (tsz + chunksz - 1) / chunksz;
	tsz *= chunksz;
	if ((tsz + alcur) > almax)
		tsz = almax - alcur;
	if (tsz <= 0)
		return NULL;
	tsz = (tsz + pgsz - 1) / pgsz;
	tsz *= pgsz;
	if (tsz < sz)
		return NULL;

	nal = malloc(sizeof(*nal));
	nal->next = al;
	al = nal;
	nal->len = tsz;

	if ((fd = open("/dev/zero", O_RDWR)) < 0)
		errprintf(ERR_ERRNO_EXIT, "open of /dev/zero failed");

	nal->addr = mmap(0, tsz, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (nal->addr == MAP_FAILED)
		errprintf(ERR_ERRNO_EXIT, "mmap failed of %d bytes", tsz);
	nal->nalloced = tsz - sz;
	if (verbose)
		printf("%s: new chunk at 0x%x\n", Cmd, nal->addr);
	
	close(fd);
	alcur += tsz;
	return nal->addr;
}

void
deletechunks(void)
{
	al_t *talp, *alp;

	for (alp = al; alp; alp = talp) {
		talp = alp->next;

		if (munmap(alp->addr, alp->len) != 0) {
			errprintf(ERR_ERRNO_EXIT, "unmap at 0x%x len %d failed",
				alp->addr, alp->len);
			/* NOTREACHED */
		}
		free(alp);
	}
	al = NULL;
}
