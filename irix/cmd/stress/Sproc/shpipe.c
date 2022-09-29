/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.12 $"

#include "sys/types.h"
#include "sys/mman.h"
#include "stdlib.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "fcntl.h"
#include "signal.h"
#include "ulocks.h"
#include "errno.h"
#include "sys/stat.h"
#include "sys/wait.h"
#include "stress.h"

/*
 * shpipe - simple sharing open files test w/ pipes
 * Also tests that file descriptor syncronization takes place correctly
 * i.e. even though one thread is blocked, the last close on a pipe happens
 */
pid_t ppid;	/* parents pid */
pid_t spid = 0;	/* shared procs pid */
pid_t pipid = 0;	/* piped process' pid */
int Gfd;
char sbuf[1024];
char *Cmd;
int Verbose = 0;
char *afile, *tfile;
ulock_t mutex;
usptr_t *handle;

extern void dopipe(char *pfile);
extern void sigcld();
extern void slave(), sprocchild(void *);

int
main(int argc, char **argv)
{
	int pp[2];
	int i, rv;
	ssize_t n;
	pid_t pid;
	int nloop;
	int fd;
	struct stat statb;
	char buf[128];

	Cmd = errinit(argv[0]);
	if (argc < 2) {
		fprintf(stderr, "Usage:%s [-v] nloop\n", argv[0]);
		exit(-1);
	}
	if (strcmp(argv[1], "PIPE") == 0) {
		/* sub-process - read from pipe and wait */
		dopipe(argv[2]);
		/* NOTREACHED */
	}
	if (argc == 3) {
		Verbose++;
		nloop = atoi(argv[2]);
	} else
		nloop = atoi(argv[1]);
	ppid = get_pid();

	/* open a file and write a pattern into it */
	tfile = tempnam(NULL, "shpipe");
	if ((fd = open(tfile, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0) {
		errprintf(1, "cannot open %s", tfile);
		/* NOTREACHED */
	}
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	afile = tempnam(NULL, "shpipe");
	if ((handle = usinit(afile)) == NULL) {
		errprintf(ERR_ERRNO_RET, "usinit of %s failed", afile);
		goto err;
	}
	mutex = usnewlock(handle);

	/* fill sbuf */
	for (i = 0; i < 1024; i++)
		sbuf[i] = "abcdefghijklmnopqrstuvwxyz0123456789"[i % 36];

	/* write some stuff into it */
	if (write(fd, sbuf, 1024) != 1024) {
		errprintf(ERR_ERRNO_RET, "write failed %s", tfile);
		goto err;
	}
	close(fd);
	sigset(SIGCLD, sigcld);
	sigset(SIGHUP, SIG_DFL);	/* for TERMCHILD to work */

	/* now sproc */
	if ((spid = sproc(slave, PR_SALL)) < 0) {
		if (errno != EAGAIN) {
			errprintf(ERR_ERRNO_RET, "sproc failed i:%d", i);
			goto err;
		} else {
			fprintf(stderr, "%s:can't sproc:%s\n",
				Cmd, strerror(errno));
			unlink(tfile);
			exit(1);
		}
	}
	/* wait for it to go blocked */
	while ((rv = (int)prctl(PR_ISBLOCKED, spid)) != 1) {
		if (Verbose)
			wrout("Waiting for slave to block");
		if (rv < 0) {
			errprintf(ERR_ERRNO_RET,
				"wait for block failed on pid %d", spid);
			goto err;
		}
		sginap(10);
	}

	/* now open and close */
	for (i = 0; i < nloop; i++) {
		/* set up a pipe */
		if (pipe(pp) < 0) {
			errprintf(3, "cannot create pipe");
			goto err;
		}
		if ((pipid = fork()) < 0) {
			errprintf(3, "cannot fork");
			goto out;
		} else if (pipid == 0) {
			/* child */
			close(1);
			dup(pp[1]);
			close(pp[0]);
			close(0);
			close(pp[1]);
			execlp(argv[0], argv[0], "PIPE", tfile, NULL);
			errprintf(3, "exec of %s failed", argv[0]);
			goto err;
		} else {
			/* parent */
			close(pp[1]);
			Gfd = pp[0];
		}

		/* let slave go and try to read */
		if (unblockproc(spid) < 0) {
			errprintf(3, "unblockproc failed");
			goto err;
		}

		while ((rv = (int)prctl(PR_ISBLOCKED, spid)) != 1) {
			if (Verbose)
				wrout("Waiting for slave to finish and close");
			if (rv < 0) {
				errprintf(3, "ISBLOCKED on pid %d failed",
						spid);
				goto err;
			}
			sginap(10);
		}

		/* now make sure that slave has closed pipes */
		if (fstat(Gfd, &statb) >= 0) {
			errprintf(2, "pipe still open");
			goto err;
		} else if (errno != EBADF) {
			errprintf(3, "fstat failed");
			goto err;
		}
	}
	pipid = 0;
	sigignore(SIGCLD);

	/*
	 * now test fd syncronization
	 */
	if ((spid = sproc(sprocchild, PR_SALL, 0)) < 0) {
		if (errno != EAGAIN) {
			errprintf(ERR_ERRNO_RET, "sproc failed i:%d", i);
			goto err;
		} else {
			fprintf(stderr, "%s:can't sproc:%s\n",
				Cmd, strerror(errno));
			exit(1);
		}
	}
	for (i = 0; i < nloop; i++) {
		uswsetlock(mutex, 1);

		if (pipe(pp) == -1) {
		    errprintf(ERR_ERRNO_RET, "pipe failed");
		    goto err;
		}

		if ((pid = fork()) == -1) {
			if (errno != EAGAIN) {
				errprintf(ERR_ERRNO_RET, "fork failed i:%d", i);
				goto err;
			} else {
				fprintf(stderr, "%s:can't fork:%s\n",
					Cmd, strerror(errno));
				unlink(tfile);
				exit(1);
			}
		}

		if (pid == 0) {
		    (void)close(pp[0]);
		    (void)write(pp[1], "hello world\n", 13);
		    (void)exit(0);
		}

		(void)close(pp[1]);
		while ((n = read(pp[0], buf, sizeof buf)) > 0) {
			if (Verbose) {
				(void)write(1, "received:", 9);
				(void)write(1, buf, n);
			}
		}
		if (n == -1) {
			errprintf(ERR_ERRNO_RET, "read");
			goto err;
		}
		    
		(void)close(pp[0]);
		while (waitpid(pid, NULL, 0) >= 0 || errno == EINTR)
			;

		usunsetlock(mutex);
	}

out:
	sigignore(SIGCLD);
	unlink(tfile);
	if (pipid > 0)
		kill(pipid, SIGKILL);
	exit(0);

err:
	sigignore(SIGCLD);
	unlink(tfile);
	if (pipid > 0)
		kill(pipid, SIGKILL);
	abort();
	/* NOTREACHED */
}

void
slave()
{
	char buf[1024];
	ssize_t cnt;

	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(0);
	for (;;) {
		/* wait for parent */
		blockproc(get_pid());

		if (Verbose)
			wrout("slave accessing pipe");
		/* now try to read */
		if ((cnt = read(Gfd, buf, 1024)) != 1024) {
			if (cnt < 0) {
				errprintf(1, "error on read");
				/* NOTREACHED */
			}
			errprintf(0, "short count from read:%d", cnt);
			/* NOTREACHED */
		} else {
			if (strncmp(sbuf, buf, 1024) != 0) {
				errprintf(0, "buffer mismatch");
				/* NOTREACHED */
			}
		}
		if (Verbose)
			wrout("close Gfd:%x", Gfd);
		if (close(Gfd) < 0) {
			errprintf(1, "close of pipe failed");
			/* NOTREACHED */
		}
		if (pipid <= 0) {
			errprintf(0, "pipid is %d", pipid);
			/* NOTREACHED */
		}
		if (kill(pipid, SIGKILL) < 0) {
			errprintf(1, "kill of pipe process failed");
			/* NOTREACHED */
		}
	}
}

void
sigcld()
{
	auto int status;

	pid_t pid = wait(&status);
	if (pid == spid) {
		fprintf(stderr, "shpipe:sproc process died: status:%x\n", status);
		/* just exit so we don't overwrite core file */
		unlink(tfile);
		exit(-1);
	}
}

void
dopipe(char *pfile)
{
	int fd;
	ssize_t rv;
	char buf[1024];

	/* open a file and read */
	if ((fd = open(pfile, O_RDONLY)) < 0) {
		errprintf(1, "cannot open %s", pfile);
		/* NOTREACHED */
	}
	if ((rv = read(fd, buf, 1024)) != 1024) {
		errprintf(1, "read failed %d", rv);
		/* NOTREACHED */
	}

	/* write down pipe */
	if ((rv = write(1, buf, 1024)) != 1024) {
		errprintf(1, "write failed %d", rv);
		/* NOTREACHED */
	}
	close(fd);
	pause();
}

/* ARGSUSED */
void
sprocchild(void *blah)
{
    prctl(PR_TERMCHILD);
    if (getppid() == 1) {
        exit(0);
    }

    for (;;) {
        uswsetlock(mutex, 1);
        usunsetlock(mutex);
    }
}
