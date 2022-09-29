#include "unistd.h"
#include "stdlib.h"
#include "wait.h"
#include "stdio.h"
#include "getopt.h"
#include "malloc.h"

int nloops = 1000;
int maxsize = 128;
int verbose = 0;
int maxptrs = 100;

static void checkallpats(char **p, int *s, char *pat, int);
/*
 * Try some reallocs
 */
main(int argc, char **argv)
{
	void *p, *q;
	int c, i, j;
	char **ptrs;
	char *pats;
	int *siz;
	char newpat;
	int newptr, newsize;
#ifdef LIBMALLOC
	int maxfst = 28;
#endif

	while ((c = getopt(argc, argv, "x:p:m:vn:")) != -1)
		switch (c) {
		case 'p':
			maxptrs = atoi(optarg);
			break;
		case 'm':
			maxsize = atoi(optarg);
			break;
		case 'n':
			nloops = atoi(optarg);
			break;
#ifdef LIBMALLOC
		case 'x':
			maxfst = atoi(optarg);
			break;
#endif
		case 'v':
			verbose++;
			break;
		}
	
#ifdef LIBMALLOC
	if (mallopt(M_MXFAST, maxfst) != 0)
		fprintf(stderr, "setting of MXFAST failed\n");
#endif
	
	/* first try very small to very large */
	q = malloc(150000);
	p = malloc(3);
	free(q);
	q = realloc(p, 100000);
	free(q);

	ptrs = (char **)calloc(maxptrs, sizeof(*ptrs));
	siz = (int *)calloc(maxptrs, sizeof(*siz));
	pats = (char *)calloc(maxptrs, sizeof(*pats));
	for (i = 0; i < nloops; i++) {
		newptr = rand() % maxptrs;
		do {
			newpat = rand() % 0x100;
		} while (newpat <= 0);
		do {
			newsize = rand() % maxsize;
		} while (newsize <= 0);

		if (verbose)
			printf("Reallooc ptr %d from %d to %d\n",
				newptr, siz[newptr], newsize);
		if ((ptrs[newptr] = realloc(ptrs[newptr], newsize)) == NULL) {
			abort();
		}
		siz[newptr] = newsize;
		pats[newptr] = newpat;
		for (j = 0; j < newsize; j++)
			ptrs[newptr][j] = newpat;
		checkallpats(ptrs, siz, pats, maxptrs);
	}
	return(0);
}

static void
checkallpats(char **p, int *s, char *pat, int nptrs)
{
	int i, j;

	for (i = 0; i < nptrs; i++) {
		if (p[i] == NULL)
			continue;
		for (j = 0; j < s[i]; j++) {
			if (p[i][j] != pat[i])
				abort();
		}
	}
}
