#include "sys/types.h"
#include "malloc.h"
#include "stdio.h"
#include "limits.h"
#include "sys/times.h"

/*
 * tlbthrash - thrash 2nd level tlb
 */
#define DEFKBYTES	40000
#define DEFNLOOPS	1000
#define DEFTPERC	40

struct l {
	int	value;
	int	pad[10000];
	struct l *left;
	struct l *right;
};

int verbose = 0;
int nnodes = 0;
void Usage(), walktree();
extern struct l *buildtree();

main(argc, argv)
int argc;
char **argv;
{
	extern int optind;
	extern char *optarg;

	register int i, c, mbytes;
	register struct l *head;
	struct tms tm;
	long start;
	int nelem, nloops;
	int throwcount = 0;
	int tperc = DEFTPERC;

	mbytes = DEFKBYTES;
	nloops = DEFNLOOPS;

	while((c = getopt(argc, argv, "vm:n:t:")) != EOF)
	switch (c) {
	case 'n':	/* # loops */
		nloops = atoi(optarg);
		if (nloops <= 0)
			Usage();
		break;
	case 'v':	/* verbose */
		verbose++;
		break;
	case 't':	/* throw-away % */
		tperc = atoi(optarg);
		if (tperc < 0 || tperc > 100)
			Usage();
		break;
	case 'm':	/* memory usage in Kbytes */
		mbytes = atoi(optarg);
		if (mbytes <= 0)
			Usage();
		break;
	default:
		Usage();
		/* NOTREACHED */
	}

	nelem = mbytes * 1024 / sizeof(struct l);
	throwcount = (nelem * tperc) / 100;

	printf("Building tree with %d elements (ignoring %d elements) %d bytes\n",
		nelem, throwcount, nelem * sizeof(struct l));
	head = buildtree(nelem, throwcount ? nelem/throwcount : 0);

	printf("Walking tree %d times\n", nloops);
	start = times(&tm);

	for (i = 0; i < nloops; i++)
		walktree(head);

	i = times(&tm) - start;
	printf("start:%x time:%x nodes:%d\n", start, times(&tm), nnodes);
	i = (i*1000)/CLK_TCK;
	printf("time for %d loops %d mS or %d uS per loop\n",
		nloops, i, (i*1000)/nloops);
	printf("time per element %d uS\n", (i*1000)/nnodes);
	exit(0);
}

void Usage()
{
	fprintf(stderr, "Usage:tlbthrash [-m #Kbytes][-n #loops][-v][-t % throw away]\n");
	exit(-1);
}

struct l *
buildtree(nelem, tcnt)
int nelem;
int tcnt;		/* throw away every tcnt entries */
{
	extern double drand48();
	register int i;
	register struct l *n, *head;

	if ((head = (struct l *)malloc(sizeof(struct l))) == NULL) {
		perror("tlbthrash:ERROR:malloc");
		exit(-1);
	}
	head->value = 50*nelem;

	for (i = 0; i < nelem; i++) {
		/* alloc a new block */
		if ((n = (struct l *)malloc(sizeof(struct l))) == NULL) {
			perror("tlbthrash:ERROR:malloc");
			exit(-1);
		}
		n->left = NULL;
		n->right = NULL;
		if (tcnt && (i % tcnt) == 0)
			continue;
		do {
			n->value = (unsigned long) (drand48() * (100*nelem));
		} while (insert(head, n) < 0);
	}
	return(head);
}

insert(node, new)
register struct l *node;
register struct l *new;
{
	/* walk down left side till new value is < current */
	if (node->value == new->value)
		return(-1);
	if (new->value < node->value) {
		if (node->left == NULL) {
			node->left = new;
			return(0);
		}
		return(insert(node->left, new));
	} else {
		if (node->right == NULL) {
			node->right = new;
			return(0);
		}
		return(insert(node->right, new));
	}
}

void walktree(p)
register struct l *p;
{
	if (p->left)
		walktree(p->left);
	if (p->right)
		walktree(p->right);

	nnodes++;
	if (verbose)
		printf("0x%x ", p);
}
