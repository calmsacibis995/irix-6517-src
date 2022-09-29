/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.4 $"
#include <stdio.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/sysmp.h>

/*
 * swapinc - test out swpinc overflow
 * There are 2 checks - one at fork() and one for getpages
 */
struct rminfo rm;
int pgsz, quiet;
void swapit(), d_printf();

main(argc, argv)
int argc;
char **argv;
{
	register int i, rv;
	int after = 0;
	char *p;

#if 0
	if (argc > 1)
		after = 1;
#else
	for (i = 1 ; i < argc ; i++){
		switch (argv[i][1]) {
		case 'q':
			quiet++;
			break;
		case 's':
			after++;
			break;
		case 'h':
			printf("usage: %s -{qs} \n", argv[0]);
			printf("\t%s -q quiet mode\n ", argv[0]);
			printf("\t%s -s swap mode\n", argv[0]);
			exit(0);
			break;
		}
	}
#endif
	pgsz = getpagesize();
	sysmp(MP_SAGET, MPSA_RMINFO, &rm, sizeof(rm));

	d_printf("swapinc:freemem %d availrmem %d physmem %d pgsz %d \n",
		rm.freemem, rm.availrmem, rm.physmem, pgsz);


	/* first alloc some memory */

	if ((p = (char *)malloc((size_t)(10 * pgsz))) == NULL) {
		perror("swapinc:ERROR:malloc(main)");
		exit(-1);
	}

	for (i = 0; i < (10 * pgsz); i += pgsz)
		p[i] = 0xfe;

	if (!after)
		swapit();
	/* now crank up lots of processes */
	d_printf("swapinc:Starting up processes\n");
	for (i = 0; i < 260; i++) {
		if ((rv = fork()) < 0) {
			perror("swapinc:ERROR:fork");
			exit(-1);
		} else if (rv == 0) {
			/* child */
			sleep(30);
			exit(0);
		}
	}
	if (after)
		swapit();
	sleep(10);
	exit(0);
}

void swapit()
{
	#define MAX_MEM 0x7fffffff
	char *p;
	ulong_t i, j, mem;

	/* now cause it to be swapped out */

	if (pgsz == 4096 && rm.freemem >= 524288)
		mem = MAX_MEM;
	else
		mem = rm.freemem*pgsz;

	if ((p = (char *)malloc((size_t)mem)) == NULL) {
		perror("swapinc:ERROR:malloc(swapit)");
		exit(-1);
	}

	for (i = 0; i < mem; i += pgsz)
		p[i] = 0xfe;

	/* now release excess mem */

	free((void *)p);
}

void d_printf(s1, s2, s3, s4, s5,s6)
char *s1;
int s2, s3,s4,s5,s6;
{
	if (!quiet) 
        	printf(s1,s2,s3,s4,s5,s6);
}
