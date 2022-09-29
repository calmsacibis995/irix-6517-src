#ident "$Revision: 1.10 $"

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)main.c	5.3 (SGI Release) 4/23/86";
 */

/* REFERENCED */
static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";

/*
 *	Modified to recursively extract all files within a subtree
 *	(supressed by the h option) and recreate the heirarchical
 *	structure of that subtree and move extracted files to their
 *	proper homes (supressed by the m option).
 *	Includes the s (skip files) option for use with multiple
 *	dumps on a single tape.
 *	8/29/80		by Mike Litzkow
 *
 *	Modified to work on the new file system and to recover from
 *	tape read errors.
 *	1/19/82		by Kirk McKusick
 *
 *	Full incremental restore running entirely in user code and
 *	interactive tape browser.
 *	1/19/83		by Kirk McKusick
 */

#include "restore.h"
struct context curfile;

int	bflag = 0, cvtflag = 0, dflag = 0, vflag = 0, yflag = 0;
int	hflag = 1, mflag = 1, Nflag = 0;
int	oflag = 0;
char	command = '\0';
long	dumpnum = 1;
long	volno = 0;
long	ntrec;
char	*dumpmap;
char	*clrimap;
efs_ino_t maxino;
time_t	dumptime;
time_t	dumpdate;
FILE 	*terminal;
time_t	newer;
int	newest;
int	justcreate;
int	efsdirs;
int	suser = 0;		/* suser == 1 implies super-user */
union	u_spcl u_spcl;

main(int argc, char *argv[])
{
	char *cp;
	efs_ino_t ino;
	char *inputdev;
	char *symtbl = "./restoresymtable";
	char name[MAXPATHLEN];
	int found;

	if (geteuid() == 0)
		suser++;
	if ((inputdev = getenv("TAPE")) == NULL)
		inputdev = "/dev/tape";
	if (signal(SIGINT, onintr) == SIG_IGN)
		(void) signal(SIGINT, SIG_IGN);
	if (signal(SIGTERM, onintr) == SIG_IGN)
		(void) signal(SIGTERM, SIG_IGN);
	if (argc < 2) {
usage:
		fprintf(stderr, "Usage:\n%s%s%s%s%s",
			"\trestore tfhsvy [file file ...]\n",
			"\trestore xfhmsvy [file file ...]\n",
			"\trestore ifhmsvy\n",
			"\trestore rfsvy\n",
			"\trestore Rfsvy\n");
		done(1);
	}
	argv++;
	argc -= 2;
	command = '\0';
	for (cp = *argv++; *cp; cp++) {
		switch (*cp) {
		case '-':
			break;
		case 'c':
			cvtflag++;
			break;
		case 'd':
			dflag++;
			break;
		case 'h':
			hflag = 0;
			break;
		case 'm':
			mflag = 0;
			break;
		case 'N':
			Nflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'y':
			yflag++;
			break;
		case 'f':
			if (argc < 1) {
				fprintf(stderr, "missing device specifier\n");
				done(1);
			}
			inputdev = *argv++;
			argc--;
			break;
		case 'b':
			/*
			 * change default tape blocksize
			 */
			bflag++;
			if (argc < 1) {
				fprintf(stderr, "missing block size\n");
				done(1);
			}
			ntrec = atoi(*argv++);
			if (ntrec <= 0) {
				fprintf(stderr, "Block size must be a positive integer\n");
				done(1);
			}
			argc--;
			break;
		case 's':
			/*
			 * dumpnum (skip to) for multifile dump tapes
			 */
			if (argc < 1) {
				fprintf(stderr, "missing dump number\n");
				done(1);
			}
			dumpnum = atoi(*argv++);
			if (dumpnum <= 0) {
				fprintf(stderr, "Dump number must be a positive integer\n");
				done(1);
			}
			argc--;
			break;
		case 't':
		case 'R':
		case 'r':
		case 'x':
		case 'i':
			if (command != '\0') {
				fprintf(stderr,
					"%c and %c are mutually exclusive\n",
					*cp, command);
				goto usage;
			}
			command = *cp;
			break;
		case 'n':
			/*
			 * Consider only those files newer than the argument
			 * file.  Use the ctime(3C) to distinguish.
			 */
			if (argc < 1) {
				fprintf(stderr, "missing file name for 'n'\n");
				done(1);
			}
			{
				struct stat64 st;

				if (stat64(*argv, &st) < 0) {
					perror(*argv);
					done(1);
				}
				newer = st.st_mtime;
			}
			argv++;
			argc--;
			break;
		case 'e':
			/*
			 * Don't overwrite any existing files.
			 */
			justcreate++;
			break;
		case 'E':
			/*
			 * Restore only non-existent files or
			 * newer versions of existing files.
			 */
			newest++;
			break;
		case 'o':
			/*
			 * Normally restore will not chown(2) files unless
			 * it is being run by root (suser == 1). This can
			 * be overridden by the 'o' flag which will result
			 * in chown(2) being called.
			 */
			oflag++;
			break;
		default:
			fprintf(stderr, "Bad key character %c\n", *cp);
			goto usage;
		}
	}
	if (command == '\0') {
		fprintf(stderr, "must specify i, t, r, R, or x\n");
		goto usage;
	}
	setinput(inputdev);
	if (argc == 0) {
		argc = 1;
		*--argv = ".";
	}
	if (newer)
		Vprintf(stdout, "Ignoring files older than %s", ctime(&newer));
	if (newest)
		Vprintf(stdout, "Ignoring older versions of existing files\n");
	if (oflag)
		Vprintf(stdout, "Chown'ing files to original owners\n");
	if (Nflag)
		Vprintf(stdout, "Not writing anything to disk\n");
	switch (command) {
	/*
	 * Interactive mode.
	 */
	case 'i':
		setup();
		extractdirs(1);
		initsymtable((char *)0);
		runcmdshell();
		done(0);
	/*
	 * Incremental restoration of a file system.
	 */
	case 'r':
		setup();
		if (dumptime > 0) {
			/*
			 * This is an incremental dump tape.
			 */
			Vprintf(stdout, "Begin incremental restore\n");
			initsymtable(symtbl);
			extractdirs(1);
			removeoldleaves();
			Vprintf(stdout, "Calculate node updates.\n");
			treescan(".", ROOTINO, nodeupdates);
			findunreflinks();
			removeoldnodes();
		} else {
			/*
			 * This is a level zero dump tape.
			 */
			Vprintf(stdout, "Begin level 0 restore\n");
			initsymtable((char *)0);
			extractdirs(1);
			Vprintf(stdout, "Calculate extraction list.\n");
			treescan(".", ROOTINO, nodeupdates);
		}
		createleaves(symtbl);
		createlinks();
		setdirmodes();
		checkrestore();
		if (dflag) {
			Vprintf(stdout, "Verify the directory structure\n");
			treescan(".", ROOTINO, verifyfile);
		}
		dumpsymtable(symtbl, (long)1);
		done(0);
	/*
	 * Resume an incremental file system restoration.
	 */
	case 'R':
		initsymtable(symtbl);
		skipmaps();
		skipdirs();
		createleaves(symtbl);
		createlinks();
		setdirmodes();
		checkrestore();
		dumpsymtable(symtbl, (long)1);
		done(0);
	/*
	 * List contents of tape.
	 */
	case 't':
		setup();
		extractdirs(0);
		initsymtable((char *)0);
		while (argc--) {
			canon(*argv++, name);
			ino = dirlookup(name);
			if (ino == 0)
				continue;
			treescan(name, ino, listfile);
		}
		done(0);
	/*
	 * Batch extraction of tape contents.
	 */
	case 'x':
		setup();
		extractdirs(1);
		initsymtable((char *)0);
		found = 0;
		while (argc-- > 0) {
			canon(*argv++, name);
			ino = dirlookup(name);
			if (ino == 0)
				continue;
			found = 1;
			Vprintf(stdout, "%s found in directory/inode map\n", name);
			if (mflag)
				pathcheck(name);
			treescan(name, ino, addfile);
		}
		if (!found)
			done(1);
		createfiles();
		createlinks();
		setdirmodes();
		if (dflag)
			checkrestore();
		done(0);
	}
	exit(0);
	/* NOTREACHED */
}
