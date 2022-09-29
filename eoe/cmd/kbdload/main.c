/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)kbdload:main.c	1.2"			*/

#ident  "$Revision: 1.1 $"

/*
 * kbdload/main.c	This program loads kbd maps.  This is main().
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>	/* for major/minor dev numbers */
#include <fcntl.h>
#include <pfmt.h>
#include <locale.h>
#include <string.h>
#include <errno.h>
#include "tach.h"

extern int optind, opterr;
extern char *optarg;
char *prog;
char *trusted = "/usr/lib/kbd";	/* THE trusted place */
int unload;
int uid, euid;

const char noperm[] = ":120:Permission denied (%s)\n";

main(argc, argv)

	int argc;
	char **argv;
{
	char *scrutinize(), *path;
	register int c, fd, pub, didone;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxmesg");
	(void)setlabel("UX:kbdload");

	opterr = didone = unload = pub = 0;
	prog = *argv;
	while ((c = getopt(argc, argv, "pL:l:e:u")) != EOF) {
		switch (c) {
		case 'L':	fakelink(optarg, 1, IS_LINK); ++didone; break;
		case 'l':	fakelink(optarg, 0, IS_LINK); ++didone; break;
		case 'e':	fakelink(optarg, 1, IS_EXT); ++didone; break;
		case 'u':	unload = 1; break;
		case 'p':	pub = 1; break;
		case '?':
		default:
usage:
		  pfmt(stderr, MM_ERROR, ":26:Incorrect usage\n");
		  pfmt(stderr, MM_ACTION, 
		  ":121:Usage:\n\t%s file\n\t%s -L str\n\t%s -l str\n\t%s -u table\n",
			prog, prog, prog, prog);
			exit(1);
		}
	}
	if (optind >= argc) {	/* no trailing args */
		if (didone)
			exit(0);	/* exit, did -l */
		goto usage;
	}
	/*
	 * Check for minor permission details...
	 *	Don't allow loading if we're setuid-root: it's a VERY
	 *	bad security risk.
	 */
	uid = getuid(); euid = geteuid();
	if ((uid != 0) && (euid == 0)) {
		pfmt(stderr, MM_ERROR, ":122:Permission denied to uid %d.\n", uid);
		exit(2);
	}
	if (uid != euid) {
		pfmt(stderr, MM_ERROR, noperm, "setuid");
		exit(2);
	}
	if (unload) {	/* unloader exits (1) on error */
		unloader(argv[optind]);
		exit(0);
	}
	/*
	 * Do stuff to argv[optind], the thing we want to load.  First,
	 * take rather restrictive security measures.
	 * If the user is trying to pull a fast one with the pathname,
	 * then disallow it.
	 * If it's not root, and stdin & stdout AREN'T owned by the
	 * user, then disallow it.
	 * If this is root, and the thing is from the trusted directory,
	 * then load it publicly.
	 */
	path = scrutinize(argv[optind]);
	if ((argv[optind][0] == '/') && (strcmp(argv[optind], path) != 0)) {
		pfmt(stderr, MM_ERROR,
		":123:Permission denied: path \"%s\" is not \"%s\".\n",
		argv[optind], path);
		exit(1);
	}
	if ((euid != 0) &&
	    ((stat_ok(euid, 0) == 0) || (stat_ok(euid, 1) == 0))) {
		pfmt(stderr, MM_ERROR, noperm,
			gettxt(":124", "not owner of stdio"));
		exit(1);
	}
	if ((strncmp(trusted, path, strlen(trusted)) == 0) && (euid == 0)) {
		; /* do nothing */
	}
	else
		pub = 0;
	if ((fd = open(path, O_RDONLY)) >= 0) {
		/* last arg is 1 for public load attempt */
		loader(fd, 0, pub);
		exit(0);
	}
	pfmt(stderr, MM_ERROR, ":125:Cannot open %s for loading: %s\n", path,	
		strerror(errno));
	exit(1);
}

/*
 * Check a file descriptor to see if we are the owner of the file.
 * if not, refuse to load or unload anything.
 */

stat_ok(uid, fd)
	int uid;	/* who we are */
	int fd;		/* fdesc to check */
{
	struct stat st;

	if (fstat(fd, &st)) {
		pfmt(stderr, MM_ERROR,
			":126:Cannot access stdio file %d: %s\n", fd,
			strerror(errno));
		exit(1);	/* can't even find it! */
	}
	if (uid != st.st_uid)
		return(0);	/* not ok */
	return(1);		/* ok */
}

#if 0

/* If these conditions hold, allow the user to not "own" a file desc:
 *	1. The file is a char special device
 *	2. The major is the minor of /dev/spx (i.e., it's a clone)
 * This is obsolete code used on pre-SVR4 systems to allow KBD to
 * be used in a /dev/spx (STREAMS) pipe: the "owner" of the file
 * won't necessarily be the user, so this CHECKS the inode info.
 */

spx_clone(fd)
	int fd;
{
	struct stat spx_st, fd_st;

	if (stat("/dev/spx", &spx_st))
		return(0);
	if (fstat(fd, &fd_st))
		return(0);
	if (! (fd_st.st_mode & S_IFCHR))
		return(0);	/* is NOT char special */
	if (minor(spx_st.st_rdev) != major(fd_st.st_rdev))
		return(0);	/* is NOT a clone of /dev/spx */
	return(1);	/* all OK */
}
#endif
