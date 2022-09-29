/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.2 $"

/*
 * Driver for kernel heap exercise.
 * Requires that kernel be compiled with syssgi(1002-1004) on (HEAPDEBUG).
 *
 * usage: heapdriver nbuckets maxmallocsize
 *
 * You'd better not ask for more memory than is available!
 */

#include "stdio.h"
#include "malloc.h"
#include "sys/types.h"
#include "sys/syssgi.h"
#include "sys/signal.h"

#define MALLOC	1002
#define REALLOC	1003
#define FREE	1004
#define SZMASK	(8192-1)

unsigned long *bp;
int nbuckets = 512;
int szmask = 8191;
int bucketmask;
int debug = 1;

extern int errno;
extern void cleanup();

main(argc, argv)
char **argv;
{
	int rv;
	int sz;
	int nb;

	if (argc > 1) {
		nbuckets = atoi(argv[1]);
		if (nbuckets < 2) {
			fprintf(stderr, "usage: %s nbuckets\n", argv[0]);
			exit(1);
		}
		while (nbuckets & (nbuckets-1))
			nbuckets--;
	}
	if (argc > 2) {
		szmask = atoi(argv[2]);
		if (szmask < 256) {
			fprintf(stderr, "usage: %s nbuckets maxsz\n", argv[0]);
			exit(1);
		}
		while (szmask & (szmask-1))
			szmask--;
		szmask--;
	} else
		szmask = SZMASK;

	bucketmask = nbuckets - 1;
	bp = (unsigned long *)calloc(nbuckets, sizeof(long));

	srand(time(0));

	signal(SIGHUP, cleanup);
	signal(SIGINT, cleanup);

	for ( ; ; ) {
		nb = rand();
		nb &= bucketmask;

		if (bp[nb]) {
			if ((rand() & 11) == 1) {
				unsigned long bt;

				sz = rand() & szmask;
				if (sz == 0)
					continue;
				bt = bp[nb];
#ifdef notdef
				if (bt >= 0xc0000000)
					continue;
#endif
				rv = syssgi(REALLOC, bp[nb], sz);
				if (debug)
					printf("realloc %x of %x bytes\n",
						bp[nb], sz);
				if (rv == -1)
					cleanup();
				bp[nb] = rv;
			} else {
				sz = bp[nb];
				bp[nb] = 0;
				syssgi(FREE, sz);
				if (debug)
					printf("free %x\n", sz);
			}
		} else {
			sz = rand() & szmask;
			if (sz == 0)
				continue;
			rv = syssgi(MALLOC, sz);
			if (debug)
				printf("malloc of %x bytes\n", sz);
			if (rv == -1)
				cleanup();
			bp[nb] = rv;
		}
	}
}

void
cleanup()
{
	int i;
	int addr;

	printf("cleanup...\n");
	for (i = 0; i < bucketmask; i++) {
		if (addr = bp[i]) {
			bp[i] = 0;
			syssgi(FREE, addr);
		}
	}
	exit(1);
}
