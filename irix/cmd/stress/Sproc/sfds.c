/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.3 $"

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
#include <stropts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include "stress.h"

/*
 * sfds - test basic fdfs, namefs, connld, svr4 pipes using
 * sproc w/ & w/o sharing fds
 */

char *Cmd;
char *afile;
usptr_t *handle;

int debug = 0;
int verbose = 0;
pid_t mpid;
ulock_t fdlock;
int p[2];

extern void segcatch(int sig, int code, struct sigcontext *sc);
extern void nofdslave(void *), fdslave(void *);

int
main(int argc, char **argv)
{
	int fd, j, i, c;
	int err = 0;
	int nloops = 10;
	int connections = 10;
	pid_t nofdpid;
	char *atname;
	char rbuf[128];

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "l:c:vd")) != EOF) {
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
		case 'c':
			connections = atoi(optarg);
			break;
		case '?':
			err++;
			break;
		}
	}
	if (err) {
		fprintf(stderr, "Usage: %s [-v][-d][-l loops]\n", Cmd);
		exit(1);
	}

	mpid = get_pid();
	sigset(SIGSEGV, segcatch);
	sigset(SIGBUS, segcatch);
	parentexinit(0);

	printf("%s:%d:# loops %d\n", Cmd, getpid(), nloops);

	syssgi(SGI_SPIPE, 1); /* turn on streams pipes */

	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	afile = tempnam(NULL, "sfds");
	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */
	if ((fdlock = usnewlock(handle)) == NULL)
		errprintf(1, "usnewlock");
		/* NOTREACHED */

	atname = tempnam(NULL, "sfdsat");
	if ((fd = open(atname, O_RDONLY|O_CREAT, 0666)) < 0) {
		errprintf(1, "create of %s failed", atname);
		/* NOTREACHED */
	}
	close(fd);
	for (i = 0; i < nloops; i++) {
		if (pipe(p) < 0) {
			errprintf(ERR_ERRNO_RET, "pipe");
			/* NOTREACHED */
		}

		/* fire up non-sharing  sproc */
		if ((nofdpid = sproc(nofdslave, PR_SADDR, 0)) < 0) {
			if (errno != EAGAIN) {
				errprintf(ERR_ERRNO_RET, "sproc failed i:%d", i);
				goto err;
			} else {
				fprintf(stderr, "%s:can't sproc:%s\n",
					Cmd, strerror(errno));
				exit(1);
			}
		}

		/* push connld */
		if (ioctl(p[1], I_PUSH, "connld") != 0) {
			errprintf(ERR_ERRNO_RET, "push of connld failed");
			goto err;
		}

		/* attach into file name space */
		if (fattach(p[1], atname) != 0) {
		/*if (fattach(p[0], atname) != 0) {*/
			errprintf(ERR_ERRNO_RET, "fattach failed");
			goto err;
		}

		for (j = 0; j < connections; j++) {
			/* open - this will pass a new descriptor
			 * to the slave
			 */
			if ((fd = open(atname, O_RDONLY)) < 0) {
				errprintf(ERR_ERRNO_RET, "open of connld  failed");
				goto err;
			}
			if (read(fd, rbuf, 128) < 0) {
				errprintf(ERR_ERRNO_RET, "read of fd failed");
				goto err;
			}
			if (strncmp(rbuf, "hello world\n", 12) != 0) {
				errprintf(ERR_RET, "read compare failed:<%s>",
					rbuf);
				goto err;
			}
			if (close(fd) != 0) {
				errprintf(ERR_ERRNO_RET, "close of fd:%d", fd);
				goto err;
			}
		}

		/* closing pipe should cause ENXIO for nofdslave */
		close(p[0]);
		close(p[1]);
		if (fdetach(atname) != 0) {
			errprintf(ERR_ERRNO_RET, "fdetach failed");
			goto err;
		}
		while (waitpid(nofdpid, NULL, 0) >= 0 || errno == EINTR)
			;
	}

	while (wait(0) >= 0 || errno == EINTR)
		;
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
err:
	unlink(atname);
	abort();
	/* NOTREACHED */
}

/* ARGSUSED */
void
nofdslave(void *arg)
{
	auto int rfd;

	close(p[1]);
	for (;;) {
		if (ioctl(p[0], I_RECVFD, &rfd) != 0) {
			if (errno == ENXIO)
				return;
			errprintf(ERR_ERRNO_EXIT, "nofdslave ioctl I_RECVFD failed");
			/* NOTREACHED */
		}
		if (verbose)
			printf("%s:received fd %d\n", Cmd, rfd);
		if (write(rfd, "hello world\n", 12) != 12) {
			errprintf(ERR_ERRNO_EXIT, "nofdslave write failed");
			/* NOTREACHED */
		}
		if (close(rfd) != 0) {
			errprintf(ERR_ERRNO_EXIT, "close of rfd:%d", rfd);
			/* NOTREACHED */
		}
	}
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
