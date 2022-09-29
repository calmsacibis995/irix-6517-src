/*
 * Copyright 1990-1998 Silicon Graphics, Inc.
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
#include <errno.h>
#include <arpa/inet.h>
#include <sys/sysmp.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include "libdv.h"

int port = 9999;
int num_servers = 1;
int backlog = 32;
unsigned aint = 10;
int lock = 0;
int acc_cnt = 0;

int exitflag = 0;

/* ARGSUSED */
void
ahand(int sig)
{
	exit(0);
}

void
cahand(int sig)
{
	exitflag++;
}

char *pname;
void
error(char *s)
{
	fprintf(stderr,"%s: ",pname);
	perror(s);
	exit(1);
}

void
usage(void)
{
	(void) fprintf(stderr, 
		"usage: s [-i secs] [-L] [-s int] [-l backlog] [-n num] [-p port]\n");
	exit(1);
}

#if 0
/* not used for stress */
/* ARGSUSED */
void
ahand(int sig)
{
	static int acnt = 0;

	acnt++;
	(void) printf("# of connections accepted == %d\n",acc_cnt);
	(void) printf("rate = %f conn/s\n", 
		(float)acc_cnt/((float)acnt * (float)aint));
	(void) alarm(aint);
}
#endif

char file[PATH_MAX];

main(int argc, char **argv)
{
	int sinlen;
	struct sockaddr_in sin;
	int fd;
	void *p;
	struct stat sb;
	int s, s1, r;
	int i;
	int secs = 300;
	int c;
	int on = 1;
	int ncpus;
	int cpu = 0;
	pid_t pid;
	FILE *fp;

	pname = argv[0];

	ncpus = (int)sysmp(MP_NAPROCS);
	(void) printf("ncpus = %d\n",ncpus);

	s = socket(AF_INET, SOCK_STREAM, 0);

	if (s < 0) {
		error("socket");
	}

	r = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
	if (r < 0) {
		error("SO_REUSEADDR");
	}

	while ((c = getopt(argc, argv, "i:Ls:l:p:n:")) != EOF) {
		switch (c) {
		case 'i':
			secs = atoi(optarg);
			break;
		case 'L':
			lock = 1;
			break;
		case 'n':
			num_servers = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'l':
			backlog = atoi(optarg);
			break;
		case 's':
			aint = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons((u_short)port);
	sin.sin_family = AF_INET;

retry:
	r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
	if (r < 0) {
		if (errno == EADDRINUSE) {
			goto retry;
		}
		error("bind");
	}

	(void) listen(s, backlog);

	for (i = 0; i < num_servers; i++) {
		if ((pid = fork()) == 0) {
			if (lock) {
				sysmp(MP_MUSTRUN, cpu);
				cpu++;
				if (cpu == ncpus) {
					cpu = 0;
				}					
			}
			goto child;
		}
		if (pid == -1) {
			error("fork");
			(void) kill(0, SIGTERM);
			exit(0);
		}
	}
	(void) sigset(SIGALRM, ahand);
	if (secs)
		(void) alarm(secs);
	(void) pause();
	exit(0);
	
child:
	(void) sigset(SIGALRM, cahand);
	(void) srand(getpid());
#if 0
	(void) alarm(aint);
#else
	if (secs)
		(void) alarm(secs);
#endif
	while (1) {
		if (exitflag) {
			exit(0);
		}
		sinlen = sizeof(sin);
		sighold(SIGALRM);
		s1 = accept(s, (struct sockaddr *)&sin, &sinlen);
		if (s1 < 0) {
			if (errno == EINTR) {
				sigrelse(SIGALRM);
				continue;
			}
			error("accept");
		}
		r = dodv(s1, s1);
		if (r < 0) {
			abort();
		}
		(void) close(s1);
		sigrelse(SIGALRM);
		acc_cnt++;
	}
	/* NOTREACHED */
	exit(0);
}
