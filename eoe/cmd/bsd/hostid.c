/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)hostid.c	5.4 (Berkeley) 5/19/86";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/capability.h>

extern long gethostid(void);

int
main(argc, argv)
	int argc;
	char **argv;
{
	register char *id;
	u_int addr;
	int hostid = -1;
	struct hostent *hp;
	cap_t ocap;
	cap_value_t capv = CAP_SYSINFO_MGT;

	if (argc < 2) {
		printf("%#lx\n", gethostid());
		exit(0);
	} 
	if (argc == 3) {
		/*
		 * -h option means treat arg as hostname only.
		 * exit status set 1 if can't map the name to an inet address.
		 */
		if (strcmp(argv[1], "-h"))
			goto usage;
		if (hp = gethostbyname(argv[2])) {
		    bcopy(hp->h_addr, &addr, sizeof(addr));
		    hostid = addr;
		}
	} else {
		id = argv[1];
		if (hp = gethostbyname(id)) {
			bcopy(hp->h_addr, &addr, sizeof(addr));
			hostid = addr;
		} else if (index(id, '.')) {
			if ((hostid = inet_addr(id)) == -1) {
			    fprintf(stderr, "Bad Internet address: %s\n", id);
			    goto usage;
			}
		} else {
			char *bad;
			hostid = strtoul(id, &bad, 16);
			if (*bad != '\0') {
			    fprintf(stderr, "Illegal hex number: %s\n", id);
			    goto usage;
			}
		}
	}
	if (hostid == -1)	/* hostname not found */
		exit(1);

	ocap = cap_acquire(1, &capv);
	if (sethostid(hostid) < 0) {
		cap_surrender(ocap);
		perror("sethostid");
		exit(1);
	}
	cap_surrender(ocap);
	exit(0);
usage:
	fprintf(stderr,
	    "usage: %s [hostname or hexnum or internet address]\n",
	    argv[0]);
	fprintf(stderr,
	    "       %s [-h hostname]\n",
	    argv[0]);
	exit(1);
}
