/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)profil-3b5:prfdc.c	1.3" */
#ident "$Revision: 1.8 $"
/*
 *	prfdc - profiler data collector
 */

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/prf.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "prf.h"

char	*buf;			/* Symbols, Kernel ctrs, and User ctr */
int	prfmax;			/* number of text addresses */

static void sigalrm(void);

static void
sigalrm(void)
{
    signal(SIGALRM, sigalrm);
}

main(int argc, char *const*argv)
{
    int prf, log;
    int rate = 10, first = 1, toff = 17;
    time_t tvec;
    int	nprocs;
    int	nread;

    switch(argc) {
    default:
	error("usage: prfdc logfile [ rate [ off_hour ] ]");
    case 4:
	toff = atoi(argv[3]);
    case 3:
	rate = atoi(argv[2]);
    case 2:
	break;
    }
    if (rate <= 0)
	error("invalid sampling rate");
    if (toff < 0 || toff > 24)
	error("invalid off hour");
    if ((prf = open("/dev/prf", 0)) < 0)
	error("cannot open /dev/prf");
    if (open(argv[1], 0) >= 0)
	error("existing file would be truncated");
    if ((log = creat(argv[1], 0666)) < 0)
	error("cannot creat log file");

    if (ioctl(prf, PRF_SETPROFTYPE, PRF_RANGE_PC|PRF_DOMAIN_TIME))
	error("cannot activate profiling");
    if (fork())
	exit(0);
    setpgrp();
    sigalrm();

    /* find out how many procesors */
    if ((nprocs = (int)sysmp(MP_NPROCS)) < 0)
	nprocs = 1;
    prfmax = ioctl(prf, PRF_GETNSYMS);
    /* need room for prfmax symbols + nprocs * prfmax counters */
    nread = prfmax * (int)sizeof(symaddr_t) +
	nprocs * ((int)sizeof(int) + (prfmax * (int)sizeof(uint)));

    if ((buf = (char *) malloc(nread)) == 0)
	error("cannot allocate space for addresses");

    write(log, &prfmax, sizeof prfmax);
    write(log, &nprocs, sizeof nprocs);

    for (;;) {
	alarm(60 * rate);
	time(&tvec);
	if (read(prf, buf, nread) != nread)
	    error("read of address/counters failed");
	if (first) {
	    write(log, buf, prfmax * sizeof (symaddr_t));
	    first = 0;
	}
	write(log, &tvec, sizeof tvec);
	write(log, &buf[prfmax * sizeof (symaddr_t)],
	      (nprocs + nprocs * prfmax) * sizeof(int));
	if (localtime(&tvec)->tm_hour == toff)
	    exit(0);
	pause();
    }
}

void
error(char *s)
{
    write(2, "prfdc: ", 6);
    write(2, s, strlen(s));
    write(2, "\n", 1);
    exit(1);
}
