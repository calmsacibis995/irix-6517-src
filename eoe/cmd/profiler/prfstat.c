/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 1.12 $"
/*
 *	prfstat - change and/or report profiler status
 */

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/prf.h>


/*
 * Profiling range and domain name decoding.
 */
typedef struct { /* range and domain parameter descriptions */
    char *name;
    unsigned int value;
    char *description;
} nameval_t;

static int lookupname(const nameval_t *, const char *, unsigned int *);
static int lookupval(const nameval_t *, unsigned int, const char **);

static nameval_t rangetab[] = {
    "off",	PRF_RANGE_NONE,		"turn off kernel profiling",
    "pc",	PRF_RANGE_PC,		"PCs (function profiling)",
    "stack",	PRF_RANGE_STACK,	"call stacks (gprof-like)",
    NULL,	0,			NULL
};
static nameval_t domaintab[] = {
    "time",	PRF_DOMAIN_TIME,	"time",
    "switch",	PRF_DOMAIN_SWTCH,	"context switches",
    "ipl",	PRF_DOMAIN_IPL,		"non-zero interrupt priority level",
    "cycles",	PRF_DOMAIN_CYCLES,	"cycle time (*)",
    "dcache1",	PRF_DOMAIN_DCACHE1,	"primary data cache misses (*)",
    "dcache2",	PRF_DOMAIN_DCACHE2,	"secondary data cache misses (*)",
    "icache1",	PRF_DOMAIN_ICACHE1,	"primary instruction cache misses (*)",
    "icache2",	PRF_DOMAIN_ICACHE2,	"secondary instruction cache misses (*)",
    "scfail",	PRF_DOMAIN_SCFAIL,	"store-conditional failures (*)",
    "brmiss",	PRF_DOMAIN_BRMISS,	"branch mispredicted (*)",
    "upgclean",	PRF_DOMAIN_UPGCLEAN,	"exclusive upgrade on clean secondary cache line (*)",
    "upgshared",PRF_DOMAIN_UPGSHARED,	"exclusive upgrade on shared secondary cache line (*)",
    NULL,	0,			NULL
};


/*
 * Argument processing.
 */
static char *myname;			/* name we were invoked under */

static void
usage(void)
{
    nameval_t *tp;
    fprintf(stderr,
	    "usage: %s [ range [ domain ] ]\n"
	    "    Range values are sampled from a subset of the domain.  If no\n"
	    "    range/domain is specified, the current sampling range and\n"
	    "    domain are displayed.\n",
	    myname);
    fprintf(stderr, "        Range options are:\n");
    for (tp = rangetab; tp->name; tp++)
	fprintf(stderr, "            %-10s  %s\n", tp->name, tp->description);
    fprintf(stderr, "        Domain options are:\n");
    for (tp = domaintab; tp->name; tp++)
	fprintf(stderr, "            %-10s  %s\n", tp->name, tp->description);
    fprintf(stderr, "            (*) Only available on systems based on R10K or later CPUs.\n");
    exit(EXIT_FAILURE);
    /*NOTREACHED*/
}


int
main(int argc, char *const*argv)
{
    unsigned int range, domain, prfstatus;
    int  prffd;

    /*
     * Parse arguments.
     */
    myname = strrchr(argv[0], '/');
    if (myname != NULL)
	myname++;
    else
	myname = argv[0];
    if(argc > 3)
	usage();
    if (argc >= 2) {
	if (!lookupname(rangetab, argv[1], &range)) {
	    fprintf(stderr, "%s: invalid range specification \"%s\"\n",
		    myname, argv[1]);
	    usage();
	}
	if (argc == 2) {
	    domain = ((range == PRF_RANGE_NONE)
		      ? PRF_DOMAIN_NONE
		      : PRF_DOMAIN_TIME);
	} else {
	    if (range == PRF_RANGE_NONE) {
		fprintf(stderr, "%s: can't specify domain for range \"off\"\n",
			myname);
		exit(EXIT_FAILURE);
		/*NOTREACHED*/
	    }
	    if (!lookupname(domaintab, argv[2], &domain)) {
		fprintf(stderr, "%s: invalid domain specification \"%s\"\n",
			myname, argv[2]);
		usage();
	    }
	}
    }

    /*
     * The prf driver unfortunately returns EINVAL in a number of cases
     * because there aren't more specific error numbers to return.  When
     * we get an EINVAL we attempt to offer a more precise error or at
     * least a hint as to what might be wrong ...
     */
    prffd = open("/dev/prf", O_RDWR);
    if (prffd < 0) {
	int open_err = errno;
	fprintf(stderr, "%s: can't open /dev/prf: %s\n",
		myname, strerror(open_err));
	if (open_err == EINVAL)
	    fprintf(stderr, " --> make sure that %s and /unix are both 32- or"
		    " both 64-bit\n", myname);
	exit(EXIT_FAILURE);
	/*NOTREACHED*/
    }

    /*
     * If any arguments were supplied, try to set the profiling status.
     */
    if (argc >= 2 && ioctl(prffd, PRF_SETPROFTYPE, range|domain) < 0) {
	fprintf(stderr, "%s: can't set status on /dev/prf: %s\n",
		myname, strerror(errno));
	if (errno == EINVALSTATE)
	    fprintf(stderr, " --> kernel symbol table not loaded, run prfld\n");
	else if (errno == ENOTSUP && domain != PRF_DOMAIN_TIME)
	    fprintf(stderr, " --> this machine may not support the requested"
		    " domain \"%s\"\n", argv[2]);
	exit(EXIT_FAILURE);
	/*NOTREACHED*/
    }

    /*
     * Always display the current status.
     */
    prfstatus = ioctl(prffd, PRF_GETSTATUS);
    if ((int)prfstatus < 0) {
	fprintf(stderr, "%s: can't get status for /dev/prf: %s\n",
		myname, strerror(errno));
	exit(EXIT_FAILURE);
	/*NOTREACHED*/
    }
    if ((prfstatus & PRF_PROFTYPE_MASK) == PRF_PROFTYPE_NONE)
	puts("profiling disabled");
    else {
	const char *rangename, *domainname;
	rangename = domainname = "UNKNOWN";
	if (!lookupval(rangetab, prfstatus & PRF_RANGE_MASK, &rangename)
	    || !lookupval(domaintab, prfstatus & PRF_DOMAIN_MASK, &domainname))
	    printf("profiling enabled: range = %s (%#x), domain = %s (%#x)\n",
		   rangename, prfstatus & PRF_RANGE_MASK,
		   domainname, prfstatus & PRF_DOMAIN_MASK);
	else
	    printf("profiling enabled: range = %s, domain = %s\n",
		   rangename, domainname);
    }
    printf("%d kernel functions in symbol table\n",
	   prfstatus & PRF_VALID_SYMTAB ? ioctl(prffd, PRF_GETNSYMS) : 0);

    exit(EXIT_SUCCESS);
    /*NOTREACHED*/
}

static int
lookupname(const nameval_t *table, const char *name, unsigned int *value)
{
    for (/*empty*/; table->name; table++)
	if (strcmp(table->name, name) == 0) {
	    *value = table->value;
	    return 1;
	}
    return 0;
}

static int
lookupval(const nameval_t *table, unsigned int value, const char **name)
{
    for (/*empty*/; table->name; table++)
	if (table->value == value) {
	    *name = table->name;
	    return 1;
	}
    return 0;
}
