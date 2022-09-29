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
#ident "$Revision: 1.10 $"

#include "stdlib.h"
#include "unistd.h"
#include "stdio.h"
#include "bstring.h"
#include "stdarg.h"
#if AMALLOC
#include "ulocks.h"
#endif
#include "malloc.h"
#include "stress.h"

#define NSMALL	4096
char *addrssmall[NSMALL];
#define NLARGE	100
char *addrslarge[NLARGE];
char mbuf[50047];

#if AMALLOC
#define MALIGN	16
#else
#define MALIGN	8
#endif

char *Cmd;
void dumpmi(register struct mallinfo *mi);
void domalloc(void *(*mptr)(size_t, void *), void (*fptr)(void *, void *), void *arena);

int
main(int argc, char **argv)
{
#ifdef AMALLOC
	void *ar;
	int initted = 0;
	int size, nl;
	char *filename;
	usptr_t *u;
#endif
	int pass = 0;
	int nloops;
#if AMALLOC | LIBMALLOC
	struct mallinfo mi;
#endif

	Cmd = argv[0];
	if (argc <= 1) {
		fprintf(stderr, "Usage:%s nloops\n", Cmd);
		exit(-1);
	}
	nloops = atoi(argv[1]);

	setlinebuf(stdout);
	printf("%s:nloops %d\n", Cmd, nloops);
	while (nloops--) {
		printf("%s:Pass:%d\n", Cmd, pass);

#if LIBMALLOC
		printf("%s:starting malloc\n", Cmd);
		domalloc((void *(*)(size_t, void *))malloc,
				(void (*)(void *, void *))free, NULL);
		printf("%s:Done malloc\n", Cmd);

		mi = mallinfo();
		dumpmi(&mi);
#endif

#ifdef AMALLOC
		while ((size = rand() % (int)sizeof(mbuf)) < 2048)
			;
		if (size < (12 * 1024))
			nl = rand() % 20;
		else if (size > (100 * 1024))
			nl = rand() % 500;
		else
			nl = 100;
		printf("%s:starting amalloc size:%d nlblks:%d\n", Cmd, size, nl);
		ar = acreate(mbuf, size, 0, NULL, NULL);
		amallopt(M_NLBLKS, nl, ar);
		domalloc(amalloc, afree, ar);
		mi = amallinfo(ar);
		dumpmi(&mi);
		printf("%s:Done amalloc\n", Cmd);

		if (!initted) {
			filename = tempnam(NULL, "mal");
			if ((u = usinit(filename)) == NULL)
				errprintf(1, "usinit failed");
			initted++;
			usmallopt(M_DEBUG, 1, u);
		}
		printf("%s:starting usmalloc\n", Cmd);
		domalloc(usmalloc, usfree, u);
		printf("%s:Done usmalloc\n", Cmd);

		printf("%s:starting SHARED amalloc size:%d nlblks:%d\n",
			Cmd, size, nl);
		ar = acreate(mbuf, size, MEM_SHARED, u, NULL);
		amallopt(M_NLBLKS, nl, ar);
		domalloc(amalloc, afree, ar);
		mi = amallinfo(ar);
		dumpmi(&mi);
		adelete(ar);
		printf("%s:Done SHARED amalloc\n", Cmd);
#endif
		pass++;
	}
#ifdef AMALLOC
	unlink(filename);
#endif
	return 0;
}

void
domalloc(void *(*mptr)(size_t, void *), void (*fptr)(void *, void *), void *arena)
{
	register int i, j;

	bzero(addrssmall, sizeof(addrssmall));
	for (i = 1; i < NSMALL; i++)  {
		if ((addrssmall[i] = (*mptr)(i, arena)) == 0) {
			printf("%s:break @ i = %d\n", Cmd, i);
			break;
		}
		if (((long)addrssmall[i] % MALIGN))
			printf("%s:Unalligned addr:%x i:%d\n",
				Cmd, addrssmall[i], i);
	}
	bzero(addrslarge, sizeof(addrslarge));
	for (j = 0, i = 1000; j < NLARGE; i+=117, j++)  {
		if ((addrslarge[j] = (*mptr)(i, arena)) == 0) {
			printf("%s:break @ i = %d\n", Cmd, i);
			break;
		}
		if (((long)addrslarge[j] % MALIGN))
			printf("%s:Unalligned addr:0x%p i:%d\n",
				Cmd, addrslarge[j], i);
	}

	/* now free */
	for (j = NSMALL-1; j > 0; j--)
		if (addrssmall[j])
			(*fptr)(addrssmall[j], arena);
	for (j = NLARGE-1; j >= 0; j--)
		if (addrslarge[j])
			(*fptr)(addrslarge[j], arena);
}

#if AMALLOC | LIBMALLOC
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
