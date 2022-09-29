//
// nfsunhang.c++
//
//	A little program that hangs on nfs mounts until they come
//	back.  Fam and the desktop use this.
//
//
// Copyright 1997, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//

#ident "$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

__sigret_t SigHup(__sigargs)
{
    if (getppid() == 1) {
	exit(0);
    }
}

void usage()
{
    // Don't need to internationalize this because this program
    // is intended to be run by fam and the desktop, which won't botch
    // the command line.
    fprintf(stderr, "usage: nfsunhang [ -printpid ] <path>\n");
    exit(1);
}

void main(int argc, char* argv[])
{
    bool printPid = false;
    char* path = NULL;

    if (argc < 2) {
	usage();
    }

    if (argc == 2) {
	path = argv[1];
    } else {
	if (strcmp(argv[1], "-printpid") != 0) {
	    usage();
	}
	printPid = true;
	path = argv[2];
    }

    struct sigaction act;
    act.sa_handler = SigHup;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP, &act, NULL);

    prctl(PR_TERMCHILD);

    struct stat64 st;
    stat64(path, &st);

    // The stat info might be cached.  Try to read the first and last
    // blocks of the file to really hang if the nfs server is down.
    int fd = open(path, O_RDONLY);
    if (fd != -1) {
	char buf[4096];
	read(fd, buf, sizeof buf);
	if (st.st_size > sizeof buf) {
	    lseek(fd, st.st_size - sizeof buf, SEEK_SET);
	    read(fd, buf, sizeof buf);
	}
	close(fd);
    }

    if (printPid) {
	pid_t pid = getpid();
	write(1, &pid, sizeof pid);
	blockproc(pid);
    }
    exit(0);
}
