/*
 * Copyright (c) 1983, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983, 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)hostname.c	5.3 (Berkeley) 6/18/88";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/capability.h>
#include <getopt.h>

int
main(argc,argv)
	int argc;
	char **argv;
{
	int ch, sflag;
	char hostname[MAXHOSTNAMELEN], *p;
	cap_t ocap;
	cap_value_t capv = CAP_SYSINFO_MGT;	

	sflag = 0;
	while ((ch = getopt(argc, argv, "s")) != EOF)
		switch((char)ch) {
		case 's':
			sflag = 1;
			break;
		case '?':
		default:
			fputs("hostname [-s] [hostname]\n", stderr);
			exit(1);
		}
	argv += optind;

	if (*argv) {
		ocap = cap_acquire(1, &capv);
		if (sethostname(*argv, strlen(*argv))) {
			cap_surrender(ocap);
			perror("sethostname");
			exit(1);
		}
		cap_surrender(ocap);
	} else {
		if (gethostname(hostname, sizeof(hostname))) {
			perror("gethostname");
			exit(1);
		}
		if (sflag && (p = strchr(hostname, '.')))
			*p = '\0';
		puts(hostname);
	}
	exit(0);
}
