/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:ulimit.c	1.5.14.1"

/*
 * ulimit builtin
 */

#include <sys/resource.h>
#include <stdlib.h>
#include <pfmt.h>
#include "defs.h"
#ifdef sgi
#include <msgs/uxsgicore.h>
#endif

extern	int	getrlimit64();
extern	int	setrlimit64();

/*
 * order is important in this table! it is indexed by resource ID.
 */

static struct rlimtab {
	char	*name;
	char	*nameid;
	char	*scale;
	char	*scaleid;
	int	divisor;
	ulong	mask;
} rlimtab[] = {
/* RLIMIT_CPU	*/	"time",		":546",	"seconds",	":547",	1,	0x80000000,
/* RLIMIT_FSIZE */	"file",		":548",	"blocks",	":549",	512,	0x80000000,
/* RLIMIT_DATA	*/	"data",		":550",	"kbytes",	":551",	1024,	0xffc00000,
/* RLIMIT_STACK */	"stack",	":552",	"kbytes",	":551",	1024,	0xffc00000,
/* RLIMIT_CORE	*/	"coredump",	":553",	"blocks",	":549",	512,	0x80000000,
/* RLIMIT_NOFILE */	"nofiles",	":554",	"descriptors",	":555",	1,	0x80000000,
/* RLIMIT_VMEM */	"memory",	":556",	"kbytes",	":551",	1024,	0xffc00000,
/* RLIMIT_RSS */	"rss", _SGI_MMX_sh_rss, "kbytes",	":551",	1024,	0xffc00000
};


void
sysulimit(argc, argv)
int argc;
char **argv;
{
	extern int opterr, optind;
	int savopterr, savoptind, savsp;
	char *savoptarg;
	char *args;
	int hard, soft, cnt, c, res;
	rlim64_t limit;
	struct rlimit64 rlimit;
	char resources[RLIM_NLIMITS];

	for (res = 0;  res < RLIM_NLIMITS; res++)
		resources[res] = 0;

	savoptind = optind;
	savopterr = opterr;
	savsp = _sp;
	savoptarg = optarg;
	optind = 1;
	_sp = 1;
	opterr = 0;
	hard = 0;
	soft = 0;
	cnt = 0;

	while ((c = getopt(argc, argv, "HSacdfnstv")) != -1) {
		switch(c) {
			case 'S':
				soft++;
				continue;
			case 'H':
				hard++;
				continue;
			case 'a':
				for (res = 0;  res < RLIM_NLIMITS; res++)
					resources[res]++;
				cnt = RLIM_NLIMITS;
				continue;
			case 'c':
				res = RLIMIT_CORE;
				break;
			case 'd':	
				res = RLIMIT_DATA;
				break;
			case 'f':
				res = RLIMIT_FSIZE;
				break;
			case 'n':	
				res = RLIMIT_NOFILE;
				break;
			case 's':	
				res = RLIMIT_STACK;
				break;
			case 't':	
				res = RLIMIT_CPU;
				break;
			case 'v':	
				res = RLIMIT_VMEM;
				break;
			case 'r':	
				res = RLIMIT_RSS;
				break;
			case '?':
				prusage(SYSULIMIT, ulimuse, ulimuseid);
				goto err;
		}
		resources[res]++;
		cnt++;
	}

	if (cnt == 0) {
		resources[res = RLIMIT_FSIZE]++;
		cnt++;
	}

	/*
	 * if out of arguments, then print the specified resources
	 */

	if (optind == argc) {
		if (!hard && !soft)
			soft++;
		for (res = 0; res < RLIM_NLIMITS; res++) {
			if (resources[res] == 0)
				continue;
			if (res >= sizeof(rlimtab)/sizeof(struct rlimtab))
				break;
			if (getrlimit64(res, &rlimit) < 0)
				continue;
			if (cnt > 1) {
				prs_buff(gettxt(rlimtab[res].nameid, rlimtab[res].name));
				prc_buff('(');
				prs_buff(gettxt(rlimtab[res].scaleid, rlimtab[res].scale));
				prc_buff(')');
				prc_buff(' ');
			}
			if (soft) {
				if (rlimit.rlim_cur == RLIM64_INFINITY)
					prs_buff(gettxt(":557", "unlimited"));
				else 
					prll_buff(rlimit.rlim_cur/rlimtab[res].divisor);
			}
			if (hard && soft)
				prc_buff(':');
			if (hard) {
				if (rlimit.rlim_max == RLIM64_INFINITY)
					prs_buff(gettxt(":557", "unlimited"));
				else 
					prll_buff(rlimit.rlim_max/rlimtab[res].divisor);
			}
			prc_buff('\n');
		}
		goto err;
	}

	if (cnt > 1 || optind + 1 != argc) {
		prusage(SYSULIMIT, ulimuse, ulimuseid);
		goto err;
	}

	if (eq(argv[optind],"unlimited"))
		limit = RLIM64_INFINITY;
	else {
		args = argv[optind];

		limit = 0;
		do {
			if (*args < '0' || *args > '9') {
				error_fail(SYSULIMIT, badulimit, badulimitid);
				goto err;
			}
			limit = (limit * 10) + (*args - '0');
		} while (*++args);

 		limit *= rlimtab[res].divisor;
	}

	if (getrlimit64(res, &rlimit) < 0) {
		error_fail(SYSULIMIT, badulimit, badulimitid);
		goto err;
	}

	if (!hard && !soft)
		hard++, soft++;
	if (hard)
		rlimit.rlim_max = limit;
	if (soft)
		rlimit.rlim_cur = limit;

	if (setrlimit64(res, &rlimit) < 0)
		error_fail(SYSULIMIT, badulimit, badulimitid);

err:
	optind = savoptind;
	opterr = savopterr;
	_sp = savsp;
	optarg = savoptarg;
}
