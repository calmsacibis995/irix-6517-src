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
#ident "$Revision: 1.4 $"
#include "unistd.h"
#include "bstring.h"
#include "stdlib.h"
#include "wait.h"
#include "stdio.h"
#include "malloc.h"
#include "getopt.h"
#include "stress.h"

/*
 * Basic memalign test
 */
#define MAXPTRS 5000

int verbose = 0;
int debug = 0;
int scribble = 1;
char *Cmd;
int maxsz = 2666;
int maxalign = (8*1024);
int maxptrs = MAXPTRS;
int alignsz = -1;
int size = -1;
void *ap;

extern int domalloc(void);
void dumpmi(register struct mallinfo *mi);
void usage(void);

int
main(int argc, char **argv)
{
	int nloops = 4, i, c;
#ifdef AMALLOC
	void *buf = NULL;
#endif

	Cmd = argv[0];

	while ((c = getopt(argc, argv, "p:A:a:s:S:vdn:")) != EOF)
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'd':
			debug++;
			break;
		case 'A':
			alignsz = atoi(optarg);
			break;
		case 'a':
			maxalign = atoi(optarg);
			break;
		case 's':
			maxsz = atoi(optarg);
			break;
		case 'S':
			size = atoi(optarg);
			break;
		case 'p':
			maxptrs = atoi(optarg);
			break;
		case 'n':
			nloops = atoi(optarg);
			break;
		default:
			usage();
			exit(-1);
		}

	for (i = 0; i < nloops; i++) {
#ifdef AMALLOC
		if (buf == NULL)
			buf = malloc(maxptrs * maxsz);
		if ((ap = acreate(buf, maxptrs * maxsz, 0, 0, 0)) == NULL)
			errprintf(0, "acreate failed for %d bytes\n", maxptrs*maxsz);
		adelete(ap);
#endif
		domalloc();
	}
	return 0;
}

int
domalloc(void)
{
	register void *p;
#if LIBMALLOC || AMALLOC
	struct mallinfo mi;
#endif
	size_t n, asz, align;
	void *ptrs[MAXPTRS];

#ifdef LIBMALLOC
	if (debug)
		if (mallopt(M_DEBUG, 1) != 0)
			errprintf(0, "M_DEBUG failed");
#endif
#ifdef AMALLOC
	if (debug)
		if (amallopt(M_DEBUG, 1, ap) != 0)
			errprintf(0, "M_DEBUG failed");
#endif
	bzero(ptrs, sizeof(ptrs));
	for (n = 0; n < maxptrs; n++) {
		if (size == -1) {
			do {
				asz = rand() % maxsz;
			} while (asz == 0);
		} else
			asz = size;
		if (alignsz == -1) {
			do {
				align = rand() % maxalign;
#if _MIPS_SZPTR == 32
			} while (align == 0 || (align & 3));
#else
			} while (align == 0 || (align & 7));
#endif
		} else
			align = alignsz;

#ifdef AMALLOC
		if ((p = amemalign(align, asz, ap)) == NULL) {
			if (verbose)
				printf("%s:out of memory after %d iterations\n",
					Cmd, n);
			break;
		}	
#else
		if ((p = memalign(align, asz)) == NULL)
			errprintf(0, "malloc returnd null");
#endif
		if ((long)p % align)
			errprintf(0, "alignment error p:0x%x alignsz 0x%x asz %d",
				p, align, asz);
		if (scribble)
			bcopy((void *)&domalloc, p, (int)asz);
		if (verbose > 1)
			printf("align 0x%x nbytes %d ptr %x\n",
				align, asz, p);
		ptrs[n] = p;
	}

#ifdef LIBMALLOC
	mi = mallinfo();
	dumpmi(&mi);
#endif
#ifdef AMALLOC
	mi = amallinfo(ap);
	dumpmi(&mi);
#endif
	for (n = 0; n < maxptrs; n++)
		if (ptrs[n])
#ifdef AMALLOC
			afree(ptrs[n], ap);
#else
			free(ptrs[n]);
#endif

#ifdef LIBMALLOC
	mi = mallinfo();
	dumpmi(&mi);
#endif
#ifdef AMALLOC
	mi = amallinfo(ap);
	dumpmi(&mi);
#endif

	return(0);
}

#if LIBMALLOC || AMALLOC
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
#endif

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-dv][-n loops][-a maxalign][-s maxsz][-A aligsz]\n", Cmd);
	fprintf(stderr, "\t-n number of loops\n");
	fprintf(stderr, "\t-v verbose\n");
	fprintf(stderr, "\t-d turn on malloc debug\n");
}
