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
#ident "$Revision: 1.6 $"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "bstring.h"
#include "wait.h"
#include "malloc.h"
#include "getopt.h"
#include "stress.h"

/*
 * Basic libmalloc checking
 */
int alignsizes[] = {8, 16, 64, 1024, 4096, 8192 };
int naligns = sizeof(alignsizes) / sizeof(int);
int arenasizes[] = {1024, 20000, 1060708 };
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
#define MAXPTRS 10000

extern int domalloc(int alignz, int allocsz, int arenasz);
extern int dosmalloc(int arenasz, int grainsz, int mxfast);
void usage(void);
void dumpmi(register struct mallinfo *mi);
extern int _edata;

int
main(int argc, char **argv)
{
	register int i, j, k, l;
	int c, ano;
	char *args[20];
	pid_t pid;
	int mxfast;

	/*
	setlinebuf(stderr);
	setlinebuf(stdout);
	*/
	Cmd = argv[0];

	while ((c = getopt(argc, argv, "SPda:s:t:g:vm:")) != EOF)
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
			sarenasizes[0] = atoi(optarg);
			snarenas = narenas = 1;
			break;
		case 'g':
			sgrainsizes[0] = atoi(optarg);
			sngrains = 1;
			break;
		case 's':
			allocsizes[0] = atoi(optarg);
			nallocs = 1;
			break;
		case 'm':
			mxfast = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'P':
			domalloc(alignsizes[0], allocsizes[0], arenasizes[0]);
			exit(0);
		case 'S':
			dosmalloc(sarenasizes[0], sgrainsizes[0], mxfast);
			exit(0);
		default:
			usage();
			exit(-1);
		}

	for (i = 0; i < narenas; i++)
		for (j = 0; j < nallocs; j++)
			for (k = 0; k < naligns; k++) {
				char align[10], alloc[10], arena[10];
				sprintf(align, "%d", alignsizes[k]);
				sprintf(alloc, "%d", allocsizes[j]);
				sprintf(arena, "%d", arenasizes[i]);
				ano = 0;
				args[ano++] = "chk";
				if (verbose)
					args[ano++] = "-v";
				if (debug)
					args[ano++] = "-d";
				args[ano++] = "-a";
				args[ano++] = align;
				args[ano++] = "-t";
				args[ano++] = arena;
				args[ano++] = "-s";
				args[ano++] = alloc;
				args[ano++] = "-P";
				args[ano++] = NULL;
				if ((pid = pcreatev(argv[0], args) ) < 0) {
					errprintf(1, "pcreate failed\n");
					/* NOTREACHED */
				} else {
					waitpid(pid, NULL, 0);
				}
			}

	/*
	 * Test small block handling
	 * We try every third maxfast 1 - 128
	 */
	for (i = 0; i < snarenas; i++)
		for (k = 0; k < sngrains; k++)
			for (l = 1; l < 128; l+=3) {
				char grain[10], arena[10];
				char mxf[10];
				sprintf(grain, "%d", sgrainsizes[k]);
				sprintf(arena, "%d", sarenasizes[i]);
				sprintf(mxf, "%d", l);
				ano = 0;
				args[ano++] = "chk";
				if (verbose)
					args[ano++] = "-v";
				if (debug)
					args[ano++] = "-d";
				args[ano++] = "-t";
				args[ano++] = arena;
				args[ano++] = "-g";
				args[ano++] = grain;
				args[ano++] = "-m";
				args[ano++] = mxf;
				args[ano++] = "-S";
				args[ano++] = NULL;
				if ((pid = pcreatev(argv[0], args) ) < 0) {
					errprintf(1, "pcreate failed\n");
					/* NOTREACHED */
				} else {
					waitpid(pid, NULL, 0);
				}
			}
	return 0;
}

int
domalloc(int alignsz, int allocsz, int arenasz)
{
	register void *p;
	struct mallinfo mi;
	int totsize, n, asz;

	if (verbose)
		printf("size %d alignment %d alloc size %d\n",
				arenasz, alignsz, allocsz);

	if (debug)
		if (mallopt(M_DEBUG, 1) != 0)
			errprintf(0, "M_DEBUG failed");
	n = 0;
	if (allocsz < 0)
		asz = rand() % arenasz;
	else
		asz = allocsz;
	totsize = 0;
	while (totsize < arenasz) {
		if ((p = memalign(alignsz, asz)) == NULL)
			errprintf(0, "malloc returnd null");
		if ((long)p % alignsz)
			errprintf(0, "alignment error p:0x%x alignsz 0x%x",
				p, alignsz);
		if (scribble)
			bcopy(&_edata, p, asz);
		totsize += ((asz + alignsz)/alignsz) * alignsz;
		if (allocsz < 0)
			asz = rand() % arenasz;
		else
			asz = allocsz;
		n++;
	}
	if (verbose)
		printf("alloced %d %d byte blocks. %d bytes total\n",
					n, allocsz, n * allocsz);

	if (verbose) {
		mi = mallinfo();
		dumpmi(&mi);
	}

	return(0);
}

int
dosmalloc(int arenasz, int grainsz, int mxfast)
{
	register void *p;
	struct mallinfo mi;
	int totsize, n, asz;
	void *ptrs[MAXPTRS+1];
	void **pp;

	if (verbose)
		printf("size %d grain size %d mxfast %d\n",
				arenasz, grainsz, mxfast);
	bzero(ptrs, (MAXPTRS+1) * sizeof (void *));
	if (debug)
		mallopt(M_DEBUG, 1);
	if (mallopt(M_GRAIN, grainsz) != 0)
		errprintf(0, "M_GRAIN failed");
	if (mallopt(M_MXFAST, mxfast) != 0)
		errprintf(0, "M_MXFAST failed");
	n = 0;
	while ((asz = rand() % (mxfast+1)) == 0)
		;
	totsize = 0;
	while (totsize < arenasz && n < MAXPTRS) {
		if ((p = malloc(asz)) == NULL)
			errprintf(0, "malloc returned null");
		if (scribble)
			bcopy(&_edata, p, asz);
		totsize += asz;
		ptrs[n] = p;
		n++;
	}
	if (verbose)
		printf("alloced %d blocks.\n", n);
	if (verbose) {
		mi = mallinfo();
		dumpmi(&mi);
	}
	for (pp = &ptrs[0]; *pp; pp++)
		free(*pp);

	return(0);
}

void
dumpmi(register struct mallinfo *mi)
{
	size_t size;

	printf("Mallinfo:arena:%d ordblks:%d smblks:%d hblks:%d\n",
		mi->arena, mi->ordblks, mi->smblks, mi->hblks);
	printf("\thblkhd:%d usmblks:%d fsmblks:%d uordblks:%d\n",
		mi->hblkhd, mi->usmblks, mi->fsmblks, mi->uordblks);
	printf("\tfordblks:%d keepcost:%d\n",
		mi->fordblks, mi->keepcost);

	size = mi->hblkhd + mi->usmblks + mi->fsmblks + mi->uordblks +
		mi->fordblks;
	if (mi->arena != size)
		errprintf(0, "\t size %d != arena %d\n", size, mi->arena);
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-dv][-a sz][-s sz][-g sz][-t sz]\n", Cmd);
	fprintf(stderr, "\t-a align size (power of 2)\n");
	fprintf(stderr, "\t-t total arena size\n");
	fprintf(stderr, "\t-g grain size\n");
	fprintf(stderr, "\t-s alloc size\n");
	fprintf(stderr, "\t-v verbose\n");
	fprintf(stderr, "\t-d turn on malloc debug\n");
}
