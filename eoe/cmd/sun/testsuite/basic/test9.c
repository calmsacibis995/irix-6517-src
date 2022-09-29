/*
 *	@(#)test9.c	1.1 97/01/03 Connectathon Testsuite
 *	1.4 Lachman ONC Test Suite source
 *
#ifdef SVR4
 * Test statvfs
#else
 * Test statfs
#endif
 *
 * Uses the following important system calls against the server:
 *
 *	chdir()
 *	mkdir()		(for initial directory creation if not -m)
#ifdef SVR4
 *	statvfs()
#else
 *	statfs()
#endif
 */

#include <sys/param.h>
#ifdef SVR3
#include <sys/types.h>
#include <sys/fs/nfs/time.h>
#include <sys/statfs.h>
#else
#ifdef SVR4
#include <sys/statvfs.h>
#else
#ifdef OSF1
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#endif
#include <sys/time.h>
#endif
#include <sys/errno.h>
#include <stdio.h>
#include "tests.h"

int Tflag = 0;		/* print timing */
int Hflag = 0;		/* print help message */
int Fflag = 0;		/* test function only;  set count to 1, negate -t */
int Nflag = 0;		/* Suppress directory operations */

void
usage()
{
	fprintf(stdout, "usage: %s [-htfn] [count]\n", Myname);
	fprintf(stdout, "  Flags:  h    Help - print this usage info\n");
	fprintf(stdout, "          t    Print execution time statistics\n");
	fprintf(stdout, "          f    Test function only (negate -t)\n");
	fprintf(stdout, "          n    Suppress test directory create operations\n");
}

main(argc, argv)
	int argc;
	char *argv[];
{
#ifdef SVR4
	int count = 1500;	/* times to do statvfs call */
#else
	int count = 1500;	/* times to do statfs call */
#endif
	int ct;
	struct timeval time;
#ifdef SVR4
	struct statvfs sfsb;
#else
	struct statfs sfsb;
#endif
	char *opts;

	umask(0);
	setbuf(stdout, NULL);
	Myname = *argv++;
	argc--;
	while (argc && **argv == '-') {
		for (opts = &argv[0][1]; *opts; opts++) {
			switch (*opts) {
				case 'h':	/* help */
					usage();
					exit(1);

				case 't':	/* time */
					Tflag++;
					break;
				
				case 'f':	/* funtionality */
					Fflag++;
					break;
				
				case 'n':	/* No Test Directory create */
					Nflag++;
					break;

				default:
					error("unknown option '%c'", *opts);
					usage();
					exit(1);
			}
		}
		argc--;
		argv++;
	}

	if (argc) {
		count = getparm(*argv, 1, "count");
		argv++;
		argc--;
	}
	if (argc) {
		usage();
		exit(1);
	}

	if (Fflag) {
		Tflag = 0;
		count = 1;
	}

	if (!Nflag)
		testdir(NULL);
	else
		mtestdir(NULL);

#ifdef SVR4
	fprintf(stdout, "%s: statvfs\n", Myname);
#else
	fprintf(stdout, "%s: statfs\n", Myname);
#endif

	if (Tflag) {
		starttime();
	}

	for (ct = 0; ct < count; ct++) {
#ifdef SVR4
		if (statvfs(".", &sfsb) < 0) {
			error("can't do statvfs on \".\"");
			exit(1);
		}
#else
#ifdef SVR3
		if (statfs(".", &sfsb, sizeof(sfsb), 0) < 0) {
#else
		if (statfs(".", &sfsb) < 0) {
#endif
			error("can't do statfs on \".\"");
			exit(1);
		}
#endif
	}

	if (Tflag) {
		endtime(&time);
	}
#ifdef SVR4
	fprintf(stdout, "\t%d statvfs calls", count);
#else
	fprintf(stdout, "\t%d statfs calls", count);
#endif
	if (Tflag) {
		fprintf(stdout, " in %d.%-2d seconds",
		    time.tv_sec, time.tv_usec / 10000);
	}
	fprintf(stdout, "\n");
	complete();
}
