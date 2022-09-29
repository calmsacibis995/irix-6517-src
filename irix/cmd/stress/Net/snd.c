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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

/* ARGSUSED */
void
ahand(int sig)
{
	exit(0);
}

void
error(char *s)
{
	perror(s);
	exit(1);
}

void
usage(void)
{
	fprintf(stderr,"usage: snd [-i secs] host\n");
	exit(1);
}

char buf[BUFSIZ];

main(int argc, char **argv)
{
	int s;
	struct sockaddr_in sin;
	int r;
	int c;
	int secs = 300;
	struct hostent *hp;

	
	sigset(SIGPIPE, SIG_IGN);

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
	argv += optind;

	if (argc != 1) {
		usage();
		exit(1);
	}
	hp = gethostbyname(argv[0]);

	if (hp == 0) {
		fprintf(stderr,"no such host: %s\n",argv[0]);
		exit(1);
	}

	signal(SIGALRM, ahand);
	alarm(secs);

	while (1) {
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0)
			error("socket");

		memcpy(&sin.sin_addr.s_addr, hp->h_addr,
			sizeof(sin.sin_addr.s_addr));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(9997);

		r = connect(s, (struct sockaddr *)&sin, sizeof(sin));
		if (r < 0)
			error("connect");

		write(s, buf, 8);
		write(s, buf, 20);
		close(s);
	}
	/* NOTREACHED */
	exit(0);
}
