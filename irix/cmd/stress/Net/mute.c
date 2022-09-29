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
error(char *s)
{
	perror(s);
	exit(1);
}

void
usage(void)
{
	fprintf(stderr, "usage: mute [-i secs]\n");
	exit(1);
}

char buf[BUFSIZ];

main(int argc, char **argv)
{
	int s;
	int s2;
	int secs = 300;
	int sinlen;
	struct sockaddr_in sin;
	int r;
	int cnt;
	int c;

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

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		error("socket");

	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(9997);

	cnt = 1;
	r = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &cnt, sizeof(cnt));

	r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
	if (r < 0)
		error("bind");

	(void) listen(s, 5);

	sinlen = sizeof(sin);

	signal(SIGALRM, ahand);
	alarm(secs);

	while (1) {
		s2 = accept(s, (struct sockaddr *)&sin, &sinlen);
		if (s2 < 0) {
			if (errno == EWOULDBLOCK) {
				fprintf(stderr, "ewouldblock\n");
				continue;
			} else {
				error("accept");
			}
		}
		(void) write(s2,"\n\n\n", 3);
		close(s2);
	}
	/* NOTREACHED */
	exit(0);
}
