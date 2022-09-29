#include "stdlib.h"
#include "unistd.h"
#include "stdio.h"
#include "getopt.h"
#include "errno.h"
#include "fcntl.h"
#include "sys/types.h"
#include "sys/syssgi.h"
#include "wait.h"
#include "signal.h"

int verbose;
int list;
#define DOFILE	1
#define DOPNAME	2
#define DOPID	3
struct getpname gp[200];
struct getvnode gv[200];

void usage(void);

int
main(int argc, char **argv)
{
	struct getpname *lp;
	struct getvnode *vp;
	int i, fd, rv, c;
	auto int n;
	int type = 0;
	long minrss;
	char *name;
	int del = 0;

	while ((c = getopt(argc, argv, "lrfnv")) != EOF)
		switch (c) {
		case 'l':
			list = 1;
			break;
		case 'f':
			if (type)
				usage();
			type = DOFILE;
			break;
		case 'n':
			if (type)
				usage();
			type = DOPNAME;
			break;
		case 'v':
			verbose++;
			break;
		case 'r':
			del = 1;
			break;
		}
	if (list) {
		n = sizeof(gp)/sizeof(struct getpname);
		if ((rv = syssgi(SGI_MINRSS, MINRSS_LISTPNAME, &gp, &n)) < 0) {
			perror("Can't get list of pnames");
			exit(1);
		}
		printf("%d process(es) have a minimum rss set\n", n);
		for (i = 0, lp = gp; i < n && lp->g_name[0]; i++, lp++) {
			printf("%s minrss %d pages\n", lp->g_name, lp->g_minrss);
		}
		n = sizeof(gv)/sizeof(struct getvnode);
		if ((rv = syssgi(SGI_MINRSS, MINRSS_LISTVNODE, &gv, &n)) < 0) {
			perror("Can't get list of vnodes");
			exit(1);
		}
		printf("%d vnode(s) have a minimum rss set\n", n);
		for (i = 0, vp = gv; i < n && vp->g_fsid; i++, vp++) {
			printf("dev 0x%x inode %d minrss %d pages\n",
				 vp->g_fsid, vp->g_nodeid, vp->g_minrss);
		}
		exit(0);
	}
	if (type == 0)
		usage();
	if (del) {
		if (optind > (argc - 1))
			usage();
		name = argv[optind];
	} else {
		if (optind > (argc - 2))
			usage();
		name = argv[optind];
		minrss = strtol(argv[optind+1], 0, 0);
	}

	if (type == DOFILE) {
		if ((fd = open(name, O_RDONLY)) < 0) {
			perror(name);
			exit(1);
		}
		if (del)
			rv = syssgi(SGI_MINRSS, MINRSS_DELVNODE, fd);
		else
			rv = syssgi(SGI_MINRSS, MINRSS_ADDVNODE, fd, minrss);
	} else if (type == DOPNAME) {
		if (del)
			rv = syssgi(SGI_MINRSS, MINRSS_DELPNAME, name);
		else
			rv = syssgi(SGI_MINRSS, MINRSS_ADDPNAME, name, minrss);
	}
	if (rv)
		perror("syssgi failed");
	exit(0);
}

void
usage(void)
{
	fprintf(stderr, "Usage: minrss [-vlrfn] object minrss(pages)\n");
	fprintf(stderr, "\t-f\tinterpret 'object' as a file name\n");
	fprintf(stderr, "\t-n\tinterpret 'object' as a process name\n");
	fprintf(stderr, "\t-r\tremove minrss\n");
	fprintf(stderr, "\t-l\tlist objects with minimum rss limits\n");
	exit(1);
}
