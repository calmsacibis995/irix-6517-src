/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.5 $"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h> 
#include <getopt.h> 
#include <string.h> 
#include <ulocks.h>
#include <wait.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "stress.h"

/*
 * shclose - have one or more threads try to close fds as they
 * are in the process of being opened
 */

char *Cmd;
char *afile;
usptr_t *handle;

int debug = 0;
int verbose = 0;
pid_t mpid;
usema_t *syncsema;
volatile int stopit;

extern void segcatch(int sig, int code, struct sigcontext *sc);
extern void slave(void *);
extern void osocketpair(void), osocket(void), opipe(void), ostrpipe(void);

void (*fcns[])(void) = {
	osocket,
	opipe,
	ostrpipe,
	osocketpair,
};
int nfcns = sizeof(fcns) / sizeof (void (*)());

int
main(int argc, char **argv)
{
	int fd, i, c;
	int err = 0;
	int nloops = 100;
	int nusers = 20;
	pid_t *pids;
	int *fds, maxfd;
	char *devfd = "/dev/fd/XXXX";
	char *devfdp;
	int maxfds = 0;
	struct rlimit rt;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "m:l:u:vd")) != EOF) {
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'd':
			debug++;
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
		case 'u':
			nusers = atoi(optarg);
			break;
		case 'm':
			maxfds = atoi(optarg);
			break;
		case '?':
			err++;
			break;
		}
	}
	if (err) {
		fprintf(stderr, "Usage: %s [-v][-d][-l loops][-u users][-m maxfds]\n", Cmd);
		exit(1);
	}

	if (maxfds) {
		getrlimit(RLIMIT_NOFILE, &rt);
		rt.rlim_cur = maxfds;
		if (setrlimit(RLIMIT_NOFILE, &rt) != 0) {
			errprintf(ERR_ERRNO_EXIT, "RLIMIT_NOFILE to %d failed",
				maxfds);
			/* NOTREACHED */
		}
	}
	mpid = get_pid();
	sigset(SIGSEGV, segcatch);
	sigset(SIGBUS, segcatch);
	parentexinit(0);

	printf("%s:%d:# loops %d\n", Cmd, getpid(), nloops);

	syssgi(SGI_SPIPE, 1); /* turn on streams pipes */

	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	usconfig(CONF_INITUSERS, nusers+1);
	afile = tempnam(NULL, "shclose");
	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */
	if ((syncsema = usnewsema(handle, 0)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "usnewsema 'syncsema' failed");
		/* NOTREACHED */
	}

	prctl(PR_SETSTACKSIZE, 64*1024);
	/* start up all users */
	pids = malloc(nusers * sizeof(pid_t));
	for (i = 0; i < nusers; i++) {
		if ((pids[i] = sproc(slave, PR_SALL, 0)) < 0) {
			if (errno != EAGAIN) {
				errprintf(ERR_ERRNO_RET, "sproc failed i:%d", i);
				goto err;
			} else {
				fprintf(stderr, "%s:can't sproc:%s\n",
					Cmd, strerror(errno));
				exit(1);
			}
		}
	}

	/*
	 * map out current fds - we don't want to touch them
	 */
	maxfd = getdtablesize();
	fds = malloc(maxfd * sizeof(int));
	devfdp = devfd + strlen("/dev/fd/");
	for (i = 0; i < maxfd; i++) {
		sprintf(devfdp, "%d", i);
		if ((fd = open(devfd, O_RDONLY)) > 0) {
			if (verbose)
				printf("%s:current valid fd %d\n", Cmd, i);

			fds[i] = 1;
			close(fd);
		} else
			fds[i] = 0;
	}

	/* let everyone go */
	for (i = 0; i < nusers; i++)
		usvsema(syncsema);

	/*
	 * we sit around and try to close things!
	 */
	for (i = 0; i < nloops; i++) {
		do {
			maxfd = getdtablehi();
			fd = rand() % maxfd;
		} while (fds[fd] == 1);
		err = close(fd);
		if (verbose && (err == 0))
			printf("%s:closed %d\n", Cmd, fd);
	}
	stopit = 1;
#ifdef NEVER
	for (i = 0; i < nusers; i++)
		if (pids[i]) {
			if (verbose)
				printf("%s:killing %d\n", Cmd, pids[i]);
			kill(pids[i], SIGKILL);
		}
#endif

	while (wait(0) >= 0 || errno == EINTR)
		;
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
err:
	abort();
	/* NOTREACHED */
}

/* ARGSUSED */
void
slave(void *arg)
{
	register int i;

	slaveexinit();
	if ((handle = usinit(afile)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "%d slave usinit", get_pid());
		/* NOTREACHED */
	}

	/* wait */
	uspsema(syncsema);

	while (stopit == 0) {
		/* do a function */
		i = rand() % nfcns;
		if (verbose)
			printf("%s:%d:calling func #%d out of %d\n",
				Cmd, get_pid(), i, nfcns);
		fcns[i]();
	}
}

void
osocket(void)
{
	int fd;
	struct stat sb;

	if ((fd = socket(PF_UNIX, SOCK_STREAM, IPPROTO_IP)) < 0) {
		if (errno == EMFILE) {
			if (verbose)
				printf("%s:socket failed:%s\n", Cmd, strerror(errno));
			return;
		}
		errprintf(ERR_ERRNO_EXIT, "socket failed");
		/* NOTREACHED */
	}
	fstat(fd, &sb);
	dup2(fd, 47);
	close(fd);
}

void
opipe(void)
{
	int p[2];
	struct stat sb;

	/* relies on fact that SGI_PIPE is per thread */
	syssgi(SGI_SPIPE, 0);

	if (pipe(p) != 0) {
		if (errno == EMFILE) {
			if (verbose)
				printf("%s:pipe failed:%s\n", Cmd, strerror(errno));
			return;
		}
		errprintf(ERR_ERRNO_EXIT, "pipe failed");
		/* NOTREACHED */
	}
	fstat(p[1], &sb);
	fstat(p[0], &sb);
	dup2(p[1], 48);
	dup2(p[0], 49);
	close(p[0]);
	close(p[1]);
}

void
osocketpair(void)
{
	int p[2];
	struct stat sb;

	if (socketpair(PF_UNIX, SOCK_STREAM, IPPROTO_IP, p) < 0) {
		if (errno == EMFILE) {
			if (verbose)
				printf("%s:socketpair failed:%s\n", Cmd, strerror(errno));
			return;
		}
		errprintf(ERR_ERRNO_EXIT, "socketpair failed");
		/* NOTREACHED */
	}
	fstat(p[1], &sb);
	fstat(p[0], &sb);
	dup2(p[1], 49);
	dup2(p[0], 48);
	close(p[0]);
	close(p[1]);
}

void
ostrpipe(void)
{
	int p[2];
	struct stat sb;

	/* relies on fact that SGI_PIPE is per thread */
	syssgi(SGI_SPIPE, 1);

	if (pipe(p) != 0) {
		if (errno == EMFILE) {
			if (verbose)
				printf("%s:strpipe failed:%s\n", Cmd, strerror(errno));
			return;
		}
		errprintf(ERR_ERRNO_EXIT, "strpipe failed");
		/* NOTREACHED */
	}
	fstat(p[1], &sb);
	fstat(p[0], &sb);
	dup2(p[1], 50);
	dup2(p[0], 51);
	close(p[0]);
	close(p[1]);
}

void
segcatch(int sig, int code, struct sigcontext *sc)
{
	fprintf(stderr, "%s:%d:ERROR: caught signal %d at pc 0x%llx code %d badvaddr %llx\n",
			Cmd, getpid(), sig, sc->sc_pc, code,
			sc->sc_badvaddr);
	abort();
	/* NOTREACHED */
}
