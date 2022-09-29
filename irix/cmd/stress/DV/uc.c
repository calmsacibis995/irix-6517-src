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
#include <sys/param.h>
#include <sys/un.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

char host[MAXHOSTNAMELEN];

char *files[] = {
	"file.16k",
	"file.1b",
	"file.1k",
	"file.1m",
	"file.2m",
	"file.32k",
	"file.32k+1b",
	"file.37k",
	"file.4k",
	"file.64k",
	"file.64k+1b",
	"file.87k",
	"file.8k",
	"file.8m",
	"file.96k"
};

int num_files = sizeof(files) / sizeof(files[0]);

int nfile = 0;

#define	NUM_CONN	20

char *port = "/tmp/9999";
int num_clients = 1;
char cmdbuf[BUFSIZ];
char file[PATH_MAX];

int exitflag = 0;

/* ARGSUSED */
void
ahand(int sig)
{
	(void) unlink(file);
	exit(0);
}

/* ARGSUSED */
void
cahand(int sig)
{
	exitflag++;
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
		"usage: c [-v] [-i secs] [-n num] [-p port]\n");
	exit(1);
}

extern char buf[];
extern int buflen;

main(int argc, char **argv)
{
	struct sockaddr_un sun;
	int r;
	int c;
	int secs = 300;
	int s;
	pid_t pid;
	int whine = 0;
	int total = 0, written = 0;
	int i;
	FILE *fp;

	(void) gethostname(host, sizeof(host));

	while ((c = getopt(argc, argv, "vi:p:n:")) != EOF) {
		switch (c) {
		case 'i':
			secs = atoi(optarg);
			break;
		case 'n':
			num_clients = atoi(optarg);
			break;
		case 'p':
			port = optarg;
			break;
		case 'v':
			whine++;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc) {
		usage();
	}

	memset((char *)&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, port);

	(void) printf("connecting to %s\n",port);

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
	if (secs)
		(void) alarm(secs);
	(void) pause();
	exit(0);
	
child:
	signal(SIGALRM, cahand);
	signal(SIGTERM, cahand);
	if (secs)
		alarm(secs);
	sprintf(file, "utfile.%s.%d",host,getpid());
	while (1) {
		total = written = 0;
		if (exitflag) {
			unlink(file);
			exit(0);
		}
		s = socket(AF_UNIX, SOCK_STREAM, 0);

		if (s < 0) {
			error("socket");
		}

		sighold(SIGALRM);
		r = connect(s, (struct sockaddr *)&sun, 
			(sizeof(sun.sun_family) + strlen(port) + 1));
		if (r < 0) {
			if (whine)
				(void) perror("connect");
			(void) close(s);
			sigrelse(SIGALRM);
			continue;
		}
		r = write(s, files[nfile], strlen(files[nfile]) + 1);
		if (r < 0) {
			error("write");
		}
		r = write(s, "\n", 1);
		if (r < 0) {
			error("write");
		}
#if 0
		r = shutdown(s, 1);
		if (r < 0) {
			error("shutdown");
		}
#endif
		(void)unlink(file);
		fp = fopen(file, "w");
		if (fp == NULL) {
			error("fopen");
		}
		while ((r = read(s, buf, buflen)) > 0) {
			total += r;
			r = fwrite(buf, 1, r, fp);
			if (r < 0) {
				error("fwrite");
			} else {
				written += r;
			}
		}
		close(s);
		fclose(fp);
		sprintf(cmdbuf, "cmp -l %s %s", files[nfile], file);
		r = system(cmdbuf);
		if (r) {
			printf("ERROR: compare of %s %s failed\n",
				files[nfile], file);
			printf("read %d bytes, wrote %d bytes\n", total, 
				written);
			abort();
		}
		(void)unlink(file);
		sigrelse(SIGALRM);
		nfile++;
		if (nfile == num_files) {
			nfile = 0;
		}
	}
	/* NOTREACHED */
	exit(0);
}
