/*
 * Copyright (c) 1991 by Silicon Graphics, Inc.
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* 
 * Filter to convert addresses in /etc/hosts file to standard form.
 */

main(argc, argv)
	char **argv;
{
	char line[256];
	char adr[256];
	char *trailer;
	register char *cp;
	FILE *fp;
	
	if (argc > 1) {
		fp = fopen(argv[1], "r");
		if (fp == NULL) {
			fprintf(stderr, "stdhosts: can't open %s\n", argv[1]);
			exit(1);
		}
	}
	else
		fp = stdin;
	while (fgets(line, sizeof(line), fp)) {
		struct in_addr in;

		if ((cp = strpbrk(line, "#\n")) != NULL)
			*cp = '\0';
		if ((trailer = strpbrk(line, " \t")) == NULL)
			continue;
		sscanf(line, "%s", adr);
		if (inet_isaddr(adr, &in.s_addr)) {
			fputs(inet_ntoa(in), stdout);
			fputs(trailer, stdout);
			fputc('\n', stdout);
		} else {
			fprintf(stderr,
		    "%s: Warning: ignoring invalid IP address '%s' for '%s'\n",
			    argv[0], adr, trailer);
		}
	}
	exit(0);
	/* NOTREACHED */
}
