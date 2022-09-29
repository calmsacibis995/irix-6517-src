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
/* $Id: netver.c,v 1.1 1997/06/23 06:51:31 sca Exp $ */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/filio.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

void error(char *);

void
usage(void)
{
	fprintf(stderr,
	    "usage: netver -h host -p port [-C count] [-n] [-c]\n");
	exit(1);
}

int	count = 0x7fffffff;
/* ARGSUSED */
void
ahand(int sig)
{
	if (count == 0) {
		exit(1);
	}
	alarm(3);
}

char	buf[90];

main(argc, argv)
	int	argc;
	char	**argv;
{
	int	s, r, sinlen;
	struct	sockaddr_in sin, sin2;
	int	port = 0;
	struct	hostent *hp;
	int	on = 1;
	int 	i = 0;
	extern	int optind;
	extern	char *optarg;
	int	c;
	int	conn = 0;
	int	nonblock = 0;
	char	*host = (char *)0;

	while ((c = getopt(argc, argv, "h:p:cnC:")) != EOF) {
		switch (c) {
		case 'h':
			host = optarg;
			break;
		case 'c':
			conn = 1;
			break;
		case 'n':
			nonblock = 1;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'C':
			count = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}

	if (!host || !port)  {
		usage();
	}
	sigset(SIGALRM, ahand);
	alarm(10);

	hp = gethostbyname(host);
	if (!hp) {
#ifndef sun
		herror(host);
#else
		fprintf(stderr,"netver: no such host: %s\n",host);
#endif
		exit(1);
	}
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	memcpy(&sin.sin_addr.s_addr, hp->h_addr_list[0], sizeof(long));

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		error("socket");
	r = setsockopt(s, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
	if (r < 0)
		error("setsockopt SO_BROADCAST");
	if (conn) {
		r = connect(s, &sin, sizeof(sin));
		if (r < 0)
			error("connect");
	}

	sinlen = sizeof(sin2);
	if (nonblock)
		ioctl(s, FIONBIO, &on);
	while (count--) {
		sprintf(buf, "line %d\n",i++);
		if (conn) {
			r = send(s, buf, (int)strlen(buf), 0);
		} else {
			r = sendto(s, buf, (int)strlen(buf), 0, &sin, 
				sizeof(sin));
		}
		if (r < 0 && errno != EINTR)
			error("send");
		if (conn) {
			r = recv(s, buf, sizeof(buf), 0);
		} else {
			r = recvfrom(s, buf, sizeof(buf), 0, &sin2, &sinlen);
		}
		if (r < 0 && errno != EWOULDBLOCK) {
			if (errno == EINTR) {
				continue;
			}
			error("recv");
		}
		if (errno == EWOULDBLOCK)
			continue;
		exit(0);
	}
	exit(0);
	/* NOTREACHED */
}

void
error(char *s)
{
	perror(s);
	exit(1);
}
