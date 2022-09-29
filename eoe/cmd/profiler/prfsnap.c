/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)profil-3b5:prfsnap.c	1.3" */
#ident "$Revision: 1.10 $"

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/time.h>
#include <sys/prf.h>

#include "prf.h"

/*
 *	prfsnap - dump profile data to a log file
 *
 * Format of file:
 *	prfmax - # of addresses (4 bytes)
 *	address table - (prfmax * sizeof(symaddr_t) bytes)
 *
 *	'n' peg counter reports:
 *	time - (4 bytes)
 *	cpuid - (4 bytes)
 *	peg counters - prfmax * sizeof(int)
 *	cpuid - (4bytes)
 *	peg counters - prfmax * sizeof(int)
 *	...
 */

static char	*buf;
static int	prfmax;

static void fatal(const char *, ...);

int
main(int argc, char *const*argv)
{
    int  prffd, logfd;
    time_t tvec;
    int	nprocs;
    size_t bufsize;
    ssize_t nread;
    unsigned int prfstatus;

    if (argc != 2)
	fatal("usage: prfsnap logfile");
    if ((prffd = open("/dev/prf", O_RDWR)) < 0)
	fatal("cannot open /dev/prf: %s", strerror(errno));
    if ((logfd = open(argv[1], O_RDWR|O_CREAT, 0666)) < 0)
	fatal("cannot creat log file: %s", strerror(errno));

    prfstatus = ioctl(prffd, PRF_GETSTATUS);
    if ((int)prfstatus < 0)
	fatal("cannot get /dev/prf status: %s", strerror(errno));
    if ((prfstatus & PRF_PROFTYPE_MASK) == PRF_PROFTYPE_NONE) {
	puts("enabling profiling: range = pc, domain = time");
	if (ioctl(prffd, PRF_SETPROFTYPE, PRF_RANGE_PC|PRF_DOMAIN_TIME))
	    fatal("cannot activate profiling: %s", strerror(errno));
    } else if ((prfstatus & PRF_RANGE_MASK) != PRF_RANGE_PC)
	fatal("currently enabled profiling is not pc profiling:"
	      " range code = %#x", prfstatus & PRF_RANGE_MASK);

    /* find out how many procesors */
    if ((nprocs = (int)sysmp(MP_NPROCS)) < 0)
	nprocs = 1;
    /* find out how many symbols were loaded */
    prfmax = ioctl(prffd, PRF_GETNSYMS);

    /* need room for prfmax symbols + nprocs ids +
     * nprocs * prfmax counters
     */
    bufsize = prfmax * sizeof(symaddr_t)
	+ (nprocs + (nprocs * prfmax)) * sizeof(int);

    if ((buf = (char *)malloc(bufsize)) == NULL)
	fatal("cannot allocate space for addresses: %s", strerror(errno));
    time(&tvec);
    
    nread = read(prffd, buf, bufsize);
    if (nread != bufsize)
	if (nread < 0)
	    fatal("read of address/counters failed: %s", strerror(errno));
	else
	    fatal("read of address/counters returned short"
		  " -- requested %u, got %d", bufsize, nread);

    if (lseek(logfd, 0, SEEK_END) == 0) {
	/* new file - write out prfmax, nprocs and addresses */
	write(logfd, &prfmax, sizeof prfmax);
	write(logfd, &nprocs, sizeof nprocs);
	write(logfd, buf, prfmax * sizeof (symaddr_t));
    } else {
	/* make sure that log makes sense - prfmax must match */
	int tmax;
	lseek(logfd, 0, SEEK_SET);
	nread = read(logfd, &tmax, sizeof tmax);
	if (nread != sizeof tmax)
	    if (nread < 0)
		fatal("error reading log file %s: %s", argv[1],
		      strerror(errno));
	    else
		fatal("read of log file %s returned short: asked for %u"
		      " bytes but got %d", sizeof tmax, nread);
	if (tmax != prfmax)
	    fatal("log file exists and has %d symbols while current kernel"
		  " has %d", tmax, prfmax);
	lseek(logfd, 0, SEEK_END);
    }
    write(logfd, &tvec, sizeof tvec);
    write(logfd, &buf[prfmax * sizeof(symaddr_t)],
	  (nprocs + nprocs * prfmax) * sizeof(int));
    exit(EXIT_SUCCESS);
    /*NOTREACHED*/
}

static void
fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("prfsnap: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
    /*NOTREACHED*/
}
