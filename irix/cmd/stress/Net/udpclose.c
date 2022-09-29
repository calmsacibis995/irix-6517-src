/*
 * Copyright 1990-1996 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* ARGSUSED */
void
ahand(int sig)
{
	exit(0);
}

void
usage(void)
{
	fprintf(stderr,"usage: udpclose [-i secs] [port]\n");
	exit(1);
}

void
error(char *s)
{
	perror(s);
	exit(1);
}

char buf[BUFSIZ];

main(int argc, char **argv)
{
	int s;
	int r;
	struct sockaddr_in sin;
	int port;
	int c;
	int secs = 300;

	while ((c = getopt(argc, argv, "i:")) != EOF) {
		switch (c) {
		case 'i':
			secs = atoi(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argc += optind;

	if (argc > 0) {
		port = htons(atoi(argv[0]));
	} else {
		port = htons(9999);
	}

	signal(SIGALRM, ahand);
	alarm(secs);

	while (1) {
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0) {
			error("socket");
		}

		sin.sin_family = AF_INET;
		sin.sin_port = port;
		sin.sin_addr.s_addr = htonl(INADDR_ANY);

		r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
		if (r < 0) {
			error("bind");
		}

		close(s);
	}
	/* NOTREACHED */
	exit(0);
}
