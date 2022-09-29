#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <bstring.h>
#include <string.h>
#include <stdlib.h>
#ifndef NOSTRESS
#include "stress.h"
#endif
#include "grp.h"
#include "getopt.h"

char *Cmd;
int verbose;
#ifndef NOGRP
extern char *__grpfilename;
#endif

main(int argc, char *argv[])
{
	int j, c, i, ngrps;
	int nloops = 1;
	FILE *grpf;
	struct group *grps = NULL;
	struct group *gp;

#ifndef NOSTRESS
	Cmd = errinit(argv[0]);
#else
	Cmd = argv[0];
#endif
	while ((c = getopt(argc, argv, "vl:f:")) != EOF)
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
#ifndef NOGRP
		case 'f':
			__grpfilename = optarg;
			break;
#endif
		default:
			usage();
			/* NOTREACHED */
		}
	
	printf("%s: starting %d loops brk 0x%x\n", Cmd, nloops, sbrk(0));
	ngrps = 0;
	for (j = 0; j < nloops; j++) {
		setgrent();
		for (i = 0; i < ngrps; i++)
			free((&grps[i])->gr_name);
		free(grps);
		grps = NULL;
		/* read in group file */
		ngrps = 0;
		while ((gp = getgrent()) != NULL) {
			ngrps++;
			grps = realloc(grps, ngrps * sizeof(struct group));
			*(grps + ngrps - 1) = *gp;
			(grps + ngrps - 1)->gr_name = strdup(gp->gr_name);
			if (verbose)
				prgrps(gp, 1);
		}
		if (verbose) {
			printf("All groups:\n");
			for (i = 0; i < ngrps; i++)
				prgrps(&grps[i], 0);
		}
		printf("%s: read %d groups pass %d brk 0x%x\n",
			Cmd, ngrps, j, sbrk(0));
		endgrent();
	}
	exit(0);
}

usage()
{
	fprintf(stderr, "Usage:%s [-v][-f grpfile][-l loops]\n", Cmd);
}

prgrps(struct group *gp, int all)
{
	char **q;

	printf("name %s pass <%s> gid %d ", gp->gr_name, gp->gr_passwd,
		gp->gr_gid);
	if (all)
		for (q = gp->gr_mem; *q; q++)
			printf("%s,", *q);
	printf("\n");
}
