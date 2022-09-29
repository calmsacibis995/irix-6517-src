/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.6 $"

#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "sys/types.h"
#include "sys/mman.h"
#include "sys/prctl.h"
#include "sys/wait.h"
#include "stdio.h"
#include "fcntl.h"
#include "signal.h"
#include "ulocks.h"
#include "errno.h"
#include "stress.h"

/*
 * shfile - simple sharing open files test
 */
#define BFSZ	1024
int bufsize = BFSZ;
char sbuf[BFSZ];	/* starting contents of file */
char buf[BFSZ];		/* slaves read buffer */
pid_t ppid;
pid_t spid = 0;
int Gfd;
int Verbose = 0;
char *Cmd;
char *afile;

void fault(int sig, int code, struct sigcontext *scp);
extern void sigcld();
extern void slave();

int
main(int argc, char **argv)
{
	int i, fd;
	int nloop;

	Cmd = argv[0];
	if (argc < 2) {
		fprintf(stderr, "Usage:%s [-v] nloop\n", argv[0]);
		exit(-1);
	}
	if (argc == 3) {
		Verbose++;
		argv++;
	}
	nloop = atoi(argv[1]);
	ppid = get_pid();
	sigset(SIGSEGV, fault);
	sigset(SIGBUS, fault);

	/* open a file and write a pattern into it */
	afile = tempnam(NULL, "shfile");
	if ((fd = open(afile, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0) {
		errprintf(1, "cannot open %s", afile);
		/* NOTREACHED */
	}

	/* fill sbuf */
	for (i = 0; i < bufsize; i++)
		sbuf[i] = "abcdefghijklmnopqrstuvwxyz0123456789"[i % 36];

	/* write some stuff into it */
	if (write(fd, sbuf, bufsize) != bufsize) {
		unlink(afile);
		errprintf(1, "write failed");
		/* NOTREACHED */
	}
	close(fd);
	sigset(SIGCLD, sigcld);
	sigset(SIGHUP, SIG_DFL);	/* for TERMCHILD to work */

	/* now sproc */
	if ((spid = sproc(slave, PR_SALL, 0)) < 0) {
		unlink(afile);
		errprintf(3, "sproc failed");
		sigignore(SIGCLD);
		exit(-1);
	}

	/* re-open file */
	if ((Gfd = open(afile, O_RDWR)) < 0) {
		errprintf(3, "cannot re-open %s", afile);
		goto bad;
	}
	/* unlock slave */
	unblockproc(spid);

	/* wait for slave */
	blockproc(get_pid());

	/* now open and close */
	for (i = 0; i < nloop; i++) {
		/* close file */
		if (close(Gfd) < 0) {
			errprintf(3, "close failed");
			goto bad;
		}

		sginap(1);
		while (!prctl(PR_ISBLOCKED, spid)) {
			if (Verbose)
				wrout("Waiting for slave to find closed file");
			sginap(10);
		}

		/* now re-open file */
		if ((Gfd = open(afile, O_RDWR)) < 0) {
			errprintf(3, "cannot re-open %s", afile);
			goto bad;
		}
		if (Gfd < 3) {
			wrout("master:Gfd < 3:%d looping", Gfd);
			for (;;)
				;
		}
		unblockproc(spid);
		sginap(30);
	}
	sigignore(SIGCLD);
	unlink(afile);
	return 0;

bad:
	sigignore(SIGCLD);
	unlink(afile);
	abort();
	/* NOTREACHED */
}

void
slave()
{
	ssize_t cnt;
	int first = 1;

	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(0);

	/* wait for mmap */
	blockproc(get_pid());

	for (;;) {
		if (Verbose)
			wrout("slave accessing file");
		/* now try to read */
		if (Gfd < 3) {
			wrout("slave Gfd < 3:%d looping", Gfd);
			for (;;)
				;
		}
		lseek(Gfd, 0, 0);
		if ((cnt = read(Gfd, buf, bufsize)) < 0) {
			if (errno != EBADF)
				errprintf(1, "unknown return from read");
				/* NOTREACHED */
			if (Verbose)
				wrout("slave got EBADF");
			blockproc(get_pid());
		} else if (cnt != bufsize) {
			wrout("short count from read:%d", cnt);
			blockproc(get_pid());
		} else {
			if (strncmp(sbuf, buf, bufsize) != 0) {
				wrout("buffer mismatch");
				write(2, sbuf, bufsize);
				write(2, "\nbuf\n", 5);
				write(2, buf, bufsize);
				write(2, "\n\n", 2);
			}
		}

		if (first) {
			first = 0;
			/* unblock parent (really just 1st time) */
			unblockproc(ppid);
		}
	}
}

void
sigcld()
{
	auto int status;
	int pid = wait(&status);

	if (pid == spid) {
		fprintf(stderr, "%s:sproc process died: status:%x\n", Cmd, status);
		unlink(afile);
		exit(-1);
	}
}

/* ARGSUSED */
void
fault(int sig, int code, struct sigcontext *scp)
{
	wrout("FAULT @ %x", scp->sc_badvaddr);
	unlink(afile);
	sigignore(SIGCLD);
	abort();
}
