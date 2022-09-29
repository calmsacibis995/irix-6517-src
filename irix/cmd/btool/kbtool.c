#include "sys/types.h"
#include "sys/syssgi.h"
#include "stdio.h"
#include "getopt.h"
#include "unistd.h"
#include "stdlib.h"
#define NUM_BRANCHES 1
#include "btool_defs.h"

extern void Usage(void);

int
main(int argc, char **argv)
{
	int i, c;
	int zero = 0;
	size_t size;
	btool_branch_t *bp;

	while ((c = getopt(argc, argv, "z")) != EOF) {
		switch (c) {
		case 'z':
			zero = 1;
			break;
		default:
			Usage();
			exit(1);
		}
	}

	if (zero) {
		if (syssgi(SGI_BTOOLREINIT) != 0) {
			perror("syssgi(BTOOLREINIT)");
			exit(1);
		}
		return 0;
	}

	if ((size = syssgi(SGI_BTOOLSIZE)) < 0) {
		perror("syssgi(BTOOLSIZE)");
		exit(1);
	}

	bp = malloc(size);
	if (syssgi(SGI_BTOOLGET, bp) < 0) {
		perror("syssgi(BTOOLGET)");
		exit(1);
	}

	for (i = 0; i < size/sizeof(*bp); i++) {
		fprintf(stdout, "%08d %d %d\n", i,
			bp[i].true_count, bp[i].false_count);
	}

	return 0;
}

void
Usage(void)
{
	fprintf(stderr, "Usage:kbtool [-z]\n");
	fprintf(stderr, "\t-z	reset all counters\n");
}
