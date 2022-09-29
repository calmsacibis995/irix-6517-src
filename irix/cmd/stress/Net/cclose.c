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
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define	NUM_CONN	20

int port = 9999;
int num_clients = 1;

/* ARGSUSED */
void
ahand(int sig)
{
	exit(0);
}

void
error(char *s)
{
	(void) perror(s);
	exit(1);
}

void
usage(void)
{
	(void) fprintf(stderr, 
		"usage: cclose [-v] [-i secs] [-n num] [-p port] host\n");
	exit(1);
}

main(int argc, char **argv)
{
	struct sockaddr_in sin;
	int r;
	int i;
	int c;
	int cid[NUM_CONN];
	int secs = 300;
	pid_t pid;
	int whine = 0;

	struct hostent *hp;

	while ((c = getopt(argc, argv, "vi:p:n:")) != EOF) {
		switch (c) {
		case 'v':
			whine++;
			break;
		case 'i':
			secs = atoi(optarg);
			break;
		case 'n':
			num_clients = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
	}

	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;

	if (inet_aton(*argv, &sin.sin_addr) == 0) {
		hp = gethostbyname(*argv);
		if (hp == (struct hostent *)0) {
			(void) herror(*argv);
			exit(1);
		}

		(void) memcpy((char *)&sin.sin_addr.s_addr, hp->h_addr, 
			sizeof(sin.sin_addr.s_addr));
	}

	(void) printf("connecting to %s\n",inet_ntoa(sin.sin_addr));

	for (i = 0; i < num_clients; i++) {
		if ((pid = fork()) == 0) {
			goto child;
		}
		if (pid == -1) {
			error("fork");
			(void) kill(0, SIGTERM);
			exit(0);
		}
	}
	(void) sigset(SIGALRM, ahand);
	(void) alarm(secs);
	(void) pause();
	(void) kill(0, SIGTERM);
	exit(0);
	
child:
	i = 0;
	signal(SIGALRM, ahand);
	alarm(secs);
	while (1) {
		struct linger l;
		/*
		 * keep connections around a while so that server builds
		 * up more in TIME-WAIT by closing first.
		 */
		cid[i] = socket(AF_INET, SOCK_STREAM, 0);

		if (cid[i] < 0) {
			error("socket");
		}
		r = connect(cid[i], (struct sockaddr *)&sin, sizeof(sin));
		if (r < 0) {
			if (whine)
				(void) perror("connect");
			(void) close(cid[i]);
			continue;
		}
		l.l_onoff = 1;
		l.l_linger = 0;
		(void)setsockopt(cid[i], SOL_SOCKET, SO_LINGER, (char *)&l, 
			sizeof(l));
		(void) close(cid[i]);
	}
	/* NOTREACHED */
	exit(0);
}
