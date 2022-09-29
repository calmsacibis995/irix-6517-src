#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "wait.h"
#include "malloc.h"
#include "getopt.h"
#include "assert.h"
#include "ulocks.h"
#include "math.h"

#define TEST_ARENA	"test.arena"

size_t size= 30000000;
char *Cmd;
void dumpmi(register struct mallinfo *mi);

int
random_recalloc_sizes(int max,int min)
{
	static double scale;
	register int retsize;
	if(!scale)
		scale=1/(pow(2,31)-1);/* get random dist between 0 and 1 */

	retsize = (double)random()*scale*(max-min)+min;
	return retsize;
}

/* ARGSUSED */
size_t
usedsize(usptr_t *ap)
{
	register size_t csize;
	register struct mallinfo mi;
#ifdef LIBMALLOC
	mi = mallinfo();
#else
	mi = usmallinfo(ap);
#endif
	csize = mi.usmblks + mi.uordblks;
	return csize;
}

/* ARGSUSED */
size_t
sizeme(usptr_t *ap)
{
	register size_t csize;
	register struct mallinfo mi;
#ifdef LIBMALLOC
	mi = mallinfo();
#else
	mi = usmallinfo(ap);
#endif
	csize = mi.hblkhd + mi.usmblks + mi.fsmblks + mi.uordblks +mi.fordblks;
	if (mi.arena != csize)
		printf("\t size %d != arena %d\n", size, mi.arena);
	return csize;
}

int
main(int argc, char **argv)
{
	char *oldp=NULL,*newp=NULL, **lostpointerarray;
	usptr_t *ap;
	int min = 1, max = 2000, j, i, nloops = 1000,c;
	int newsize, lastsize=0,crowdsize;
	char pat = 'a';
	struct mallinfo mi;
	int freealottime=nloops/2;
	int verbose = 0;
	int contigfail = 0;
	int freelost = 0;

	Cmd = argv[0];
	while ((c = getopt(argc, argv, "vs:n:m:")) != EOF)
		switch (c) {
		case 's':
			size = atoi(optarg);
			break;
		case 'n':
			nloops = atoi(optarg);
			break;
		case 'm':
			min = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		}
	if (verbose)
		setlinebuf(stdout);
	else
		setbuf(stdout, NULL);

#ifdef LIBMALLOC
	lostpointerarray=calloc(nloops,sizeof(char **));
#else
	usconfig(CONF_INITSIZE,size);
	ap=usinit(TEST_ARENA);

	if(!ap)
		exit(24);

	lostpointerarray=uscalloc(nloops,sizeof(char **),ap);
#endif

	if(!lostpointerarray)
		exit(35);

	for (i = 0 ;i < nloops; i++) {
		if (freelost) {
			freelost = 0;
			for(j=0;j<i;j++) {
				if (lostpointerarray[j]) {
#ifdef LIBMALLOC
					free(lostpointerarray[j]);
#else
					usfree(lostpointerarray[j],ap);
#endif
					lostpointerarray[j]=NULL;
					if (verbose)
						printf("v");
				}
			}
			fflush(stdout);
		}
		/* lets crowd things up */

		while ((crowdsize = rand() % (max*10)) < min);

#ifdef LIBMALLOC
		lostpointerarray[i]=calloc(crowdsize,sizeof(char *));
#else
		lostpointerarray[i]=uscalloc(crowdsize,sizeof(char *),ap);
#endif

		if(!lostpointerarray[i])
		{
#if 0
			printf("too crowded for lostpointer%#x, %d,%d\n",
			    lostpointerarray[i],crowdsize,i);
#endif
			if (verbose)
				printf("f");
			fflush(stdout);
		}

		newsize=random_recalloc_sizes(max,min);

#ifdef LIBMALLOC
		newp = recalloc(oldp, sizeof(char), newsize);
#else
		newp = usrecalloc(oldp, sizeof(char), newsize, ap);
#endif

		if (verbose)
			printf("!");


		if(verbose && newsize<lastsize)
		{
			printf("\n%06d: blk siz %04d (%s) %04d[%04d]{%d}",
			    i,lastsize,(newsize>lastsize?"+":"-"),
			    newsize,newsize-lastsize,sizeme(ap));
		}

		if(verbose && newp!=oldp)
		{
			printf("\n%06d: blk siz %04d (%s) %04d[%04d] MV <<%s>> from %#x to %#x{%d}",
			    i,lastsize,(newsize>lastsize?"+":"-"),
			    newsize,newsize-lastsize,(newp>oldp?"up":"dw"),oldp,newp,sizeme(ap));
		}

		if(!newp) {
			if (verbose)
				printf("F");
			fflush(stdout);
			if (++contigfail > 7)
				freelost++;
			continue;
		} else
			contigfail = 0;

		for (j = 0; j < (lastsize > newsize ? newsize : lastsize); j++)
			assert(newp[j] == pat);

		for (j = lastsize; j < newsize; j++)
			assert(newp[j] == 0);

		/* write new pattern in */
		pat++;
		for (j = 0; j < newsize; j++)
			newp[j] = pat;
		oldp=newp;/* even up the old and new pointers for the next pass */
		lastsize = newsize;
#ifdef LIBMALLOC
		if(i==freealottime || usedsize(ap) > size)
#else
		if(i==freealottime)
#endif
			freelost++;
	}

#ifdef LIBMALLOC
	mi = mallinfo();
#else
	mi = usmallinfo(ap);
#endif
	dumpmi(&mi);
#ifdef LIBMALLOC
	free(newp);
#else
	usfree(newp, ap);
#endif

	for(i=0;i<nloops;i++)
	{
		if(lostpointerarray[i])
		{
#ifdef LIBMALLOC
			free(lostpointerarray[i]);
#else
			usfree(lostpointerarray[i],ap);
#endif
			fflush(stdout);
		}
	}
#ifdef LIBMALLOC
	free(lostpointerarray);
#else
	usfree(lostpointerarray,ap);
#endif
#ifdef LIBMALLOC
	mi = mallinfo();
#else
	mi = usmallinfo(ap);
#endif
	dumpmi(&mi);

#ifndef LIBMALLOC
	unlink(TEST_ARENA);
#endif
	return 0;
}

void
dumpmi(register struct mallinfo *mi)
{
	size_t csize;

	printf("%s:Mallinfo:arena:%d ordblks:%d smblks:%d hblks:%d\n",
	    Cmd, mi->arena, mi->ordblks, mi->smblks, mi->hblks);
	printf("\thblkhd:%d usmblks:%d fsmblks:%d uordblks:%d\n",
	    mi->hblkhd, mi->usmblks, mi->fsmblks, mi->uordblks);
	printf("\tfordblks:%d keepcost:%d\n",
	    mi->fordblks, mi->keepcost);

	csize = mi->hblkhd + mi->usmblks + mi->fsmblks + mi->uordblks +
	    mi->fordblks;
	if (mi->arena != csize)
		printf("\t size %d != arena %d\n", csize, mi->arena);
}
