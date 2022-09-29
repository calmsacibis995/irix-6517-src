/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/file.h>
#include <time.h>
#include <platform_defs.h>

#define NOTICES	PCPLOGDIR ## "/NOTICES"

static int
mkdir_r(char *path)
{
    struct stat	sbuf;

    if (stat(path, &sbuf) < 0) {
	if (mkdir_r(dirname(strdup(path))) < 0)
	    return -1;
	return mkdir(path, 0755);
    }
    else if ((sbuf.st_mode & S_IFDIR) == 0) {
	fprintf(stderr, "pmpost: \"%s\" is not a directory\n", path);
	exit(1);
    }
    return 0;
}

/*ARGSUSED*/
static void
onalarm(int dummy)
{
}

#define LAST_UNDEFINED	-1
#define LAST_NEWFILE	-2

main(int argc, char **argv)
{
    int		i;
    int		fd;
    FILE	*np;
    char	*dir;
    struct stat	sbuf;
    time_t	now;
    int		lastday = LAST_UNDEFINED;
    struct tm	*tmp;
    int		sts = 0;

    if ((argc == 1) || (argc == 2 && strcmp(argv[1], "-?") == 0)) {
	fprintf(stderr, "Usage: pmpost message\n");
	exit(1);
    }

    dir = dirname(strdup(NOTICES));
    if (mkdir_r(dir) < 0) {
	fprintf(stderr, "pmpost: cannot create directory \"%s\": %s\n",
	    dir, strerror(errno));
	exit(1);
    }


    if ((fd = open(NOTICES, O_WRONLY, 0)) < 0) {
	umask(022);		/* is this just paranoid? */
	if ((fd = open(NOTICES, O_WRONLY|O_CREAT, 0644)) < 0) {
	    fprintf(stderr, "pmpost: cannot create file \"%s\": %s\n",
		NOTICES, strerror(errno));
	    exit(1);
	}
	lastday = LAST_NEWFILE;
    }
    signal(SIGALRM, onalarm);
    alarm(3);

    if (flock(fd, LOCK_EX) < 0) {
	fprintf(stderr, "pmpost: warning, cannot lock file \"%s\"", NOTICES);
	if (errno != EINTR)
	    fprintf(stderr, ": %s", strerror(errno));
	fputc('\n', stderr);
    }
    alarm(0);

    /*
     * have lock, get last modified day unless file just created
     */
    if (lastday != LAST_NEWFILE) {
	if (fstat(fd, &sbuf) < 0)
	    /* should never happen */
	    ;
	else {
	    tmp = localtime(&sbuf.st_mtime);
	    lastday = tmp->tm_yday;
	}
    }

    if ((np = fdopen(fd, "a")) == NULL) {
	fprintf(stderr, "pmpost: fdopen: %s\n", strerror(errno));
	exit(1);
    }

    time(&now);
    tmp = localtime(&now);

    if (lastday != tmp->tm_yday) {
	if (fprintf(np, "\nDATE: %s", ctime(&now)) < 0)
	    sts = errno;
    }

    if (fprintf(np, "%02d:%02d", tmp->tm_hour, tmp->tm_min) < 0)
	sts = errno;

    for (i = 1; i < argc; i++) {
	if (fprintf(np, " %s", argv[i]) < 0)
	    sts = errno;
    }

    if (fputc('\n', np) < 0)
	sts = errno;

    if (fclose(np) < 0)
	sts = errno;

    if (sts < 0) {
	fprintf(stderr, "pmpost: write failed: %s\n", strerror(errno));
	fprintf(stderr, "Lost message ...");
	for (i = 1; i < argc; i++) {
	    fprintf(stderr, " %s", argv[i]);
	}
	fputc('\n', stderr);
    }

    exit(0);
    /* NOTREACHED */
}
