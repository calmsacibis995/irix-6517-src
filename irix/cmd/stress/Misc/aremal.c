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
#ident "$Revision: 1.5 $"

#include "stdlib.h"
#include "wait.h"
#include "malloc.h"
#include "memory.h"
#include "stdio.h"
#include "getopt.h"
#include "unistd.h"
#include "stress.h"

char mar[40*1024*6];
void *mygrow(size_t s, void *ap);
void Usage(void);
char *Cmd;
int verbose;

int
main(int argc, char **argv)
{
	char *ptr2, *ptr;
	int nloops = 100, cursize, size;
	void *ap;
	int c, usesbrk = 0, usegrow = 0;
	int mxfast = -1;

	Cmd = errinit(argv[0]);
	size = 1024;

	while ((c = getopt(argc, argv, "vm:gl:s")) != EOF)
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'g':
			usegrow = 1;
			break;
		case 's':
			usesbrk = 1;
			break;
		case 'm':
			mxfast = atoi(optarg);
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
		default:
			Usage();
			exit(1);
		}

	printf("%s:nloops %d ", Cmd, nloops);
	if (usesbrk) {
		ap = acreate(sbrk(2048), 2048, 0, 0, (void *(*)(size_t, void *))sbrk);
		printf("using sbrk");
	} else if (usegrow) {
		ap = acreate(sbrk(2048), 2048, 0, 0, mygrow);
		printf("using mygrow");
	} else {
		ap = acreate(mar, sizeof(mar), 0, 0, 0);
		printf("using internal buffer");
	}
	printf(":re-alloc size:%d\n", size);
	if (ap == NULL) {
		perror("acreate failed");
		exit(-1);
	}

	if (mxfast >= 0) {
		if (amallopt(M_MXFAST, mxfast, ap) != 0)
			errprintf(ERR_EXIT, "M_MXFAST failed");
	}

	ptr = amalloc(1024, ap);
	cursize = size;

	while (nloops--) {
		if ((ptr2 = arealloc(ptr, cursize, ap)) == NULL) {
			if (usegrow || usesbrk)
				errprintf(ERR_RET, "realloc size %d failed",
					cursize);
			/* this is of course expected if running in
			 * local buffer mode
			 */
			exit(0);
		}

		if (verbose)
			printf("realloc from 0x%x size %d to 0x%x\n",
					ptr, cursize, ptr2);
		cursize += size;
		ptr = ptr2;
	}
	return 0;
}

/* ARGSUSED */
void *
mygrow(size_t s, void *ap)
{
	void *r;

	if (s == 0)
		return sbrk(0);
	if ((r = sbrk((ssize_t)s)) != (void *)-1L) {
		memset(r, 0x2e, s);
	}
	return r;
}

void
Usage(void)
{
	fprintf(stderr, "Usage:%s [-l loops][-gsv][-m mxfast]\n", Cmd);
	exit(1);
}
