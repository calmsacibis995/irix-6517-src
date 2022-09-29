/*
 *	@(#)test4.c	1.1 97/01/03 Connectathon Testsuite
 *	1.4 Lachman ONC Test Suite source
 *
 * Test setattr, getattr and lookup
 *
 * Creates the files in the test directory - does not create a directory
 * tree.
 *
 * Uses the following important system calls against the server:
 *
 *	chdir()
 *	mkdir()		(for initial directory creation if not -m)
 *	creat()
 *	chmod()
 *	stat()
 */

#include <sys/param.h>
#ifndef major
#include <sys/types.h>
#endif
#ifdef SVR3
#include <sys/fs/nfs/time.h>
#else
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include "tests.h"

int Tflag = 0;		/* print timing */
int Hflag = 0;		/* print help message */
int Fflag = 0;		/* test function only;  set count to 1, negate -t */
int Nflag = 0;		/* Suppress directory operations */

void
usage()
{
	fprintf(stdout, "usage: %s [-htfn] [files count]\n", Myname);
	fprintf(stdout, "  Flags:  h    Help - print this usage info\n");
	fprintf(stdout, "          t    Print execution time statistics\n");
	fprintf(stdout, "          f    Test function only (negate -t)\n");
	fprintf(stdout, "          n    Suppress test directory create operations\n");
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int files = 10;		/* number of files in each dir */
	int fi;
	int count = 50;	/* times to do each file */
	int ct;
	int totfiles = 0;
	int totdirs = 0;
	char *fname = FNAME;
	struct timeval time;
	char str[MAXPATHLEN];
	struct stat statb;
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
				
				case 'n':	/* suppress initial directory */
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
		files = getparm(*argv, 1, "files");
		argv++;
		argc--;
	}
	if (argc) {
		count = getparm(*argv, 1, "count");
		argv++;
		argc--;
	}
	if (argc) {
		fname = *argv;
		argc--;
		argv++;
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

	dirtree(1, files, 0, fname, DNAME, &totfiles, &totdirs);

	fprintf(stdout, "%s: setattr, getattr, and lookup\n", Myname);

	if (Tflag) {
		starttime();
	}

	for (ct = 0; ct < count; ct++) {
		for (fi = 0; fi < files; fi++) {
			sprintf(str, "%s%d", fname, fi);
			if (chmod(str, 0) < 0) {
				error("can't chmod 0 %s", str);
				exit(0);
			}
			if (stat(str, &statb) < 0) {
				error("can't stat %s", str);
				exit(1);
			}
			if ((statb.st_mode & 0777) != 0) {
				error("%s has mode %o after chmod 0",
				    str, (statb.st_mode & 0777));
				exit(1);
			}
			if (chmod(str, 0666) < 0) {
				error("can't chmod 0666 %s", str);
				exit(0);
			}
			if (stat(str, &statb) < 0) {
				error("can't stat %s", str);
				exit(1);
			}
			if ((statb.st_mode & 0777) != 0666) {
				error("%s has mode %o after chmod 0666",
				    str, (statb.st_mode & 0777));
				exit(1);
			}
		}
	}

	if (Tflag) {
		endtime(&time);
	}
	fprintf(stdout, "\t%d chmods and stats on %d files",
		files * count * 2, files);
	if (Tflag) {
		fprintf(stdout, " in %d.%-2d seconds",
		    time.tv_sec, time.tv_usec / 10000);
	}
	fprintf(stdout, "\n");
	/* XXX REMOVE DIRECTORY TREE? */
	complete();
}
