/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.12 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <signal.h> 
#include <ulocks.h>
#include <errno.h>
#include <pwd.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "stress.h"

/*
 * test pollable semaphore allocation
 * 1) alloc, use, free, re-alloc
 * 2) sproc guys can alloc/free
 * 3) unrelated procs can alloc/free
 * 4) other user-ids can alloc
 */
#define MAXSEMAS	FD_SETSIZE
#define DEFARENASIZE	128		/* 128kb */
#define DEFUSERS	4
#define DEFACQUIRES	4
#define DEFLOOPS	2
#define DEFGUEST	"guest"

char *Cmd;
char *afile;
/*
 * per sema info
 */
struct ss {
	usema_t *sp;
	int *fd;
	dev_t dev;
};

/*
 * all shared info goes here - the address of this struct is passed
 * via usgetinfo
 */
struct sinfo {
	struct ss *s;
	pid_t *pids;
	int nusers;
	int nacquires;
	int nsemas;
	long myturn;
	char afile[512];
	int unrel;
	barrier_t *b;
};

void simple1(void *);
void killall(usptr_t *);
int allocsemas(usptr_t *, struct ss *, int, int);
void deallocsemas(usptr_t *, struct ss *);
int opensemas(usptr_t *, struct ss *, int);
void closesemas(usptr_t *, struct ss *);
void ignunlink(char *f);

int debug = 0;
int verbose = 0;
int maxsemas = MAXSEMAS;

int
main(int argc, char **argv)
{
	int		c;
	int		err = 0;
	int		i, j;
	int		nloops = DEFLOOPS;
	int		arenasize = DEFARENASIZE;
	int		nusers = DEFUSERS;
	int		nacquires = DEFACQUIRES;
	int		mpid;
	int		idfield;
	long		myturn;
	char		*nargs[32];
	register struct sinfo *sd;
	register usptr_t *handle;
	register struct ss *s;
	register pid_t *pids;
	struct passwd *pw;
	int badperm = 0;
	int nosproctest = 0;
	int noforktest = 0;
	char *Argv0;

	Cmd = errinit(argv[0]);
	Argv0 = argv[0];
	while ((c = getopt(argc, argv, "pA:E:l:vdn:u:m:SFs:")) != EOF) {
		switch (c) {
		case 'F':
			noforktest++;
			break;
		case 'S':
			nosproctest++;
			break;
		case 'v':
			verbose++;
			break;
		case 'd':
			debug++;
			break;
		case 's':
			maxsemas = atoi(optarg);
			break;
		case 'u':
			nusers = atoi(optarg);
			break;
		case 'm':
			arenasize = atoi(optarg);
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
		case 'n':
			nacquires = atoi(optarg);
			break;
		case 'A':	/* internal execed option to pass afile */
			afile = optarg;
			break;
		case 'E':	/* internal execed option to pass tid */
			myturn = atol(optarg);
			simple1((void *)myturn);
			exit(0);
		case 'p':
			/* run with incorrect permissions on arena */
			badperm++;
			break;
		case '?':
			err++;
			break;
		}
	}
	mpid = get_pid();
	if (err) {
		fprintf(stderr, "Usage: %s [-pvFS][-d][-u users][-n acquires][-m arena size (inKb)][-l loops][-s nsemas]\n", Cmd);
		fprintf(stderr, "\t-S do not run sproc test\n");
		fprintf(stderr, "\t-F do not run fork test\n");
		fprintf(stderr, "\t-p run with incorrect arena permissions\n");
		exit(1);
	}
	if (debug)
		usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);

	if (usconfig(CONF_INITUSERS, nusers) < 0)
		errprintf(1, "INITUSERS");
	usconfig(CONF_INITSIZE, arenasize * 1024);

	if (nosproctest)
		goto forktest;

	printf("%s:%d: SPROC TEST:# users %d # loops %d # acquires %d nsemas %d\n",
		Cmd, get_pid(), nusers, nloops, nacquires, maxsemas);
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);

	afile = tempnam(NULL, "usemaalloc");
	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "usinit of %s failed", afile);

	s = malloc(MAXSEMAS * sizeof(*s));
	pids = malloc(nusers * sizeof(pid_t));
	if ((sd = usmalloc(sizeof(*sd), handle)) == NULL) {
		errprintf(1, "usmalloc of shared data area failed");
		/* NOTREACHED */
	}
	sd->s = s;
	sd->pids = pids;
	sd->nacquires = nacquires;
	sd->nusers = nusers;
	sd->unrel = 0;
	if ((sd->b = new_barrier(handle)) == NULL) {
		errprintf(1, "new_barrier failed");
		/* NOTHREACHED */
	}
	usputinfo(handle, sd);

	if (debug)
		usconfig(CONF_HISTON, handle);
	prctl(PR_SETSTACKSIZE, 64*1024);


	pids[0] = mpid;
	myturn = 0;
	sd->myturn = myturn;
	for (j = 0; j < nloops; j++) {
		for (i = 1; i < nusers; i++)
			pids[i] = 0;
		/* start up all users */
		for (i = 1; i < nusers; i++) {
			if ((pids[i] = sproc(simple1, PR_SALL, i)) < 0) {
				if (errno != EAGAIN) {
					errprintf(3, "sproc failed i:%d", i);
					killall(handle);
					abort();
				} else {
					/* running out of procs really isn't
					 * an error
					 */
					printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
					killall(handle);
					exit(1);
				}
			}
		}
		simple1((void *)0);

		while (wait(0) >= 0 || errno == EINTR)
			;

		/* all dead - now dealloc all semas */
		deallocsemas(handle, s);
		myturn = (myturn + 1) % nusers;
		sd->myturn = myturn;
	}
	usdetach(handle);

forktest:
	if (noforktest)
		goto unreltest;
	/*
	 * now try related via fork
	 */
	printf("%s:%d: FORK TEST:# users %d # loops %d # acquires %d nsemas %d\n",
		Cmd, get_pid(), nusers, nloops, nacquires, maxsemas);
	usconfig(CONF_ARENATYPE, US_GENERAL);

	afile = tempnam(NULL, "usemaalloc");
	if ((handle = usinit(afile)) == NULL) {
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */
	}

	s = usmalloc(MAXSEMAS * sizeof(*s), handle);
	pids = usmalloc(nusers * sizeof(pid_t), handle);
	if (pids == NULL || s == NULL) {
		ignunlink(afile);
		errprintf(1, "usmalloc of semaphores failed");
		/* NOTREACHED */
	}
	if ((sd = usmalloc(sizeof(*sd), handle)) == NULL) {
		ignunlink(afile);
		errprintf(1, "usmalloc of shared data area failed");
		/* NOTREACHED */
	}
	sd->s = s;
	sd->pids = pids;
	sd->nacquires = nacquires;
	sd->nusers = nusers;
	sd->unrel = 1;		/* unrelated procs must all init semas */
	strcpy(sd->afile, afile);
	if ((sd->b = new_barrier(handle)) == NULL) {
		ignunlink(afile);
		errprintf(1, "new_barrier failed");
		/* NOTHREACHED */
	}
	usputinfo(handle, sd);

	pids[0] = mpid;
	myturn = 0;
	sd->myturn = myturn;
	for (j = 0; j < nloops; j++) {
		int npid;

		for (i = 1; i < nusers; i++)
			pids[i] = 0;
		/* start up all users */
		for (i = 1; i < nusers; i++) {
			if ((npid = fork()) < 0) {
				if (errno != EAGAIN) {
					errprintf(3, "fork failed i:%d", i);
					killall(handle);
					abort();
				} else {
					/* running out of procs really isn't
					 * an error
					 */
					printf("%s:can't fork:%s\n", Cmd, strerror(errno));
					killall(handle);
					exit(1);
				}
			} else if (npid == 0) {
				/* child */
				simple1((void *)(long)i);
				exit(0);
			}
			pids[i] = npid;
		}
		simple1((void *)0);

		while (wait(0) >= 0 || errno == EINTR)
			;

		/* all dead - now dealloc all semas */
		deallocsemas(handle, s);
		myturn = (myturn + 1) % nusers;
		sd->myturn = myturn;
	}
	ignunlink(afile);
	usdetach(handle);

unreltest:
	/*
	 * now try unrelated and different uid
	 */
	if (getuid() != 0) {
		printf("%s: Cannot run unrelated process test - must run as root!\n",
			Cmd);
		exit(0);
	}

	printf("%s:%d: UNRELATED TEST:# users %d # loops %d # acquires %d nsemas %d\n",
		Cmd, get_pid(), nusers, nloops, nacquires, maxsemas);
	usconfig(CONF_ARENATYPE, US_GENERAL);

	afile = tempnam(NULL, "usemaalloc");
	if ((handle = usinit(afile)) == NULL) {
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */
	}

	/* set up permissions */
	if (!badperm) {
		if (usconfig(CONF_CHMOD, handle, 0666) != 0) {
			ignunlink(afile);
			errprintf(1, "CONF_CHMOD failed");
			/* NOTREACHED */
		}
	}

	s = usmalloc(MAXSEMAS * sizeof(*s), handle);
	pids = usmalloc(nusers * sizeof(pid_t), handle);
	if (pids == NULL || s == NULL) {
		ignunlink(afile);
		errprintf(1, "usmalloc of semaphores failed");
		/* NOTREACHED */
	}
	if ((sd = usmalloc(sizeof(*sd), handle)) == NULL) {
		ignunlink(afile);
		errprintf(1, "usmalloc of shared data area failed");
		/* NOTREACHED */
	}
	sd->s = s;
	sd->pids = pids;
	sd->nacquires = nacquires;
	sd->nusers = nusers;
	strcpy(sd->afile, afile);
	sd->unrel = 1;		/* unrelated procs must all init semas */
	if ((sd->b = new_barrier(handle)) == NULL) {
		ignunlink(afile);
		errprintf(1, "new_barrier failed");
		/* NOTHREACHED */
	}
	usputinfo(handle, sd);

	/* set up args to execed children */
	i = 0;
	nargs[i++] = argv[0];
	if (verbose)
		nargs[i++] = "-v";
	if (debug)
		nargs[i++] = "-d";
	nargs[i++] = "-A";
	nargs[i++] = afile;
	nargs[i++] = "-E";
	idfield = i;
	nargs[i++] = NULL;
	nargs[i++] = NULL;

	/* find new uid for children */
	if ((pw = getpwnam(DEFGUEST)) == NULL) {
		ignunlink(afile);
		errprintf(1, "getpwnam of %s failed", DEFGUEST);
		/* NOTREACHED */
	}

	pids[0] = mpid;
	myturn = 0;
	sd->myturn = myturn;
	for (j = 0; j < nloops; j++) {
		int npid;

		for (i = 1; i < nusers; i++)
			pids[i] = 0;
		/* start up all users */
		for (i = 1; i < nusers; i++) {
			if ((npid = fork()) < 0) {
				if (errno != EAGAIN) {
					errprintf(3, "fork failed i:%d", i);
					killall(handle);
					abort();
				} else {
					/* running out of procs really isn't
					 * an error
					 */
					printf("%s:can't fork:%s\n", Cmd, strerror(errno));
					killall(handle);
					exit(1);
				}
			} else if (npid == 0) {
				char idf[10];

				/* child */
				if (setgid(pw->pw_gid) != 0 ||
				    setuid(pw->pw_uid) != 0) {
					killall(handle);
					errprintf(1, "set[ug]id failed");
					/* NOTREACHED */
				}
				sprintf(idf, "%d", i);
				nargs[idfield] = idf;
				execvp(Argv0, nargs);
				killall(handle);
				errprintf(1, "execvp of <%s> failed", Cmd);
				/* NOTREACHED */
			}
			pids[i] = npid;
		}
		simple1((void *)0);

		while (wait(0) >= 0 || errno == EINTR)
			;

		/* all dead - now dealloc all semas */
		deallocsemas(handle, s);
		myturn = (myturn + 1) % nusers;
		sd->myturn = myturn;
	}
	ignunlink(afile);
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
}

void
killall(usptr_t *handle)
{
	register int i;
	struct sinfo *sd;
	register int err = oserror();

	sd = usgetinfo(handle);
	for (i = 0; i < sd->nusers; i++)
		if (sd->pids[i] != get_pid() && sd->pids[i] > 0)
			kill(sd->pids[i], SIGKILL);
	if (sd->afile[0])
		ignunlink(sd->afile);
	setoserror(err);
}

void
simple1(void *arg)
{
	int whoami = (int)(long) arg;
	register int i, j;
	register int gotton, nwait;
	fd_set lfd, tfd;
	register int rv;
	char buf[5120], sfd[20];
	struct sinfo *sd;
	register int nsemas;
	register struct ss *s;
	usptr_t *handle;
	int fdidx;

	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "%d slave usinit", get_pid());
	sd = usgetinfo(handle);
	s = sd->s;

	if (sd->myturn == whoami) {
		/* only alloc one set of fds for related procs */
		if (!sd->unrel) {
			sd->nsemas = allocsemas(handle, s, 1, 0);
			fdidx = 0;
		} else {
			sd->nsemas = allocsemas(handle, s, sd->nusers, whoami);
			fdidx = whoami;
		}
		/*
		 * make sure master has created ALL threads before unblocking
		 * everyone
		 */
		barrier(sd->b, sd->nusers);
	} else {
		/* wait for allocater */
		barrier(sd->b, sd->nusers);

		/* for unrelated procs - all must open semas */
		if (sd->unrel) {
			opensemas(handle, s, whoami);
			fdidx = whoami;
		} else {
			fdidx = 0;
		}
	}
	/* wait for everyone */
	barrier(sd->b, sd->nusers);

	nsemas = sd->nsemas;
	nwait = 0;
	for (j = 0; j < sd->nacquires; j++) {
		FD_ZERO(&lfd);
		gotton = 0;
		for (i = 0; i < nsemas; i++) {
			/*
			 * for unrelated procs some may not have
			 * enough fds to open all semas - just ignore these
			 */
			if (s[i].fd[fdidx] < 0) {
				gotton++;
				continue;
			}
			if ((rv = uspsema(s[i].sp)) > 0) {
				/* got it */
				gotton++;
				sginap(1);
				if (usvsema(s[i].sp) < 0) {
					killall(handle);
					errprintf(1, "%d usvsema failed i %d sp 0x%x",
						get_pid(), i, s[i].sp);
					/* NOTREACHED */
				}
			} else if (rv < 0) {
				killall(handle);
				errprintf(1, "%d uspsema failed i %d sp 0x%x",
					get_pid(), i, s[i].sp);
				/* NOTREACHED */
			} else {
				/* didn't get it - we'll select on it */
				FD_SET(s[i].fd[fdidx], &lfd);
			}
		}
		nwait += (nsemas - gotton);

		/* now wait for any I didn't get */
		while (gotton != nsemas) {
			if (verbose) {
				sprintf(buf, "%s:%d pass %d selecting on %d semas (",
						Cmd,
						get_pid(), j+1,
						nsemas - gotton);
				for (i = 0; i < nsemas; i++)
					if (FD_ISSET(s[i].fd[fdidx], &lfd)) {
						sprintf(sfd, "(%d-%d)",
							s[i].fd[fdidx],
							minor(s[i].dev));
						strcat(buf, sfd);
					}
				strcat(buf, ")\n");
				write(1, buf, strlen(buf));
			}
			tfd = lfd;
			rv = select(FD_SETSIZE, &tfd, NULL, NULL, NULL);
			if (rv < 0) {
				killall(handle);
				errprintf(1, "%d select", get_pid());
				/* NOTREACHED */
			}
			if (verbose)
				sprintf(buf, "%s:%d pass %d got (",
							Cmd, get_pid(), j+1);
			for (i = 0; i < nsemas; i++) {
				if (s[i].fd[fdidx] < 0)
					continue;
				if (FD_ISSET(s[i].fd[fdidx], &tfd)) {
					gotton++;
					FD_CLR(s[i].fd[fdidx], &lfd);
					if (verbose) {
						sprintf(sfd, "%d ",
								s[i].fd[fdidx]);
						strcat(buf, sfd);
					}
					sginap(1);
					if (usvsema(s[i].sp) < 0) {
						killall(handle);
						errprintf(1, "%d usvsema failed i %d sp 0x%x",
							i, get_pid(), s[i].sp);
						/* NOTREACHED */
					}
				}
			}
			if (verbose) {
				strcat(buf, ")\n");
				write(1, buf, strlen(buf));
			}
		}

		if (verbose)
			printf("%s:%d pass:%d\n", Cmd, get_pid(), j+1);
	}
	/* wait for everyone */
	barrier(sd->b, sd->nusers);

	/* close semas if unrelated procs */
	if (sd->myturn == whoami || sd->unrel) {
		closesemas(handle, s);
	}
	if (verbose)
		printf("%s:%d complete waited for %d semas\n",
			Cmd, get_pid(), nwait);
}

int
allocsemas(usptr_t *handle, struct ss *s, int nusers, int whoami)
{
	register int i;
	struct stat	sb;

	for (i = 0; i < maxsemas-1; i++) {
		if ((s[i].sp = usnewpollsema(handle, 1)) == NULL) {
			/*
			 * we can fail due to no more file descriptors
			 * if on a Power series we happen to need more locks
			 * and don't have a file descriptor available
			 * to open the hlock device
			 */
			if (oserror() == ENOMEM || oserror() == EMFILE)
				break;
			killall(handle);
			errprintf(1, "%d usnewpollsema failed i:%d", get_pid(), i);
		}

		/*
		 * for unrelated procs each need their own fds
		 */
		if ((s[i].fd = usmalloc(nusers * sizeof(int), handle)) == NULL) {
			killall(handle);
			errprintf(0, "%d out of memory for fds", get_pid());
			/* NOTREACHED */
		}

		if ((s[i].fd[whoami] = usopenpollsema(s[i].sp, 0644)) < 0) {
			if (oserror() == EMFILE || oserror() == ENOSPC) {
				usfreepollsema(s[i].sp, handle);
				s[i].sp = NULL;
				break;
			}
			killall(handle);
			errprintf(1, "%d usopenpollsema sp 0x%x",
					get_pid(), s[i].sp);
		}
		if (fstat(s[i].fd[whoami], &sb) != 0) {
			killall(handle);
			errprintf(1, "%d stat on fd %d",
						get_pid(), s[i].fd[whoami]);
		}
		s[i].dev = sb.st_rdev;
		if (debug) {
			usctlsema(s[i].sp, CS_DEBUGON);
		}
	}
	s[i].sp = NULL;
	if (verbose)
		printf("%s:%d allocated %d semaphores (stopped cause:%s)\n",
			Cmd, get_pid(), i, strerror(oserror()));
	return(i);
}

void
deallocsemas(usptr_t *handle, struct ss *s)
{
	register int i;
	for (i = 0; ; i++) {
		if (s[i].sp == NULL)
			break;
		if (usfreepollsema(s[i].sp, handle) != 0)
			errprintf(1, "usfreepollsema failed pid %d i %d",
				get_pid(), i);
		usfree(s[i].fd, handle);
	}
}

int
opensemas(usptr_t *handle, struct ss *s, int whoami)
{
	register int i, j;
	struct stat sb;
	register int err;

	for (i = 0; ; i++) {
		if (s[i].sp == NULL)
			break;
		if ((s[i].fd[whoami] = usopenpollsema(s[i].sp, 0644)) < 0) {
			if ((err = oserror()) == EMFILE) {
				break;
			}
			killall(handle);
			errprintf(1, "%d usopenpollsema", get_pid());
		}
		if (fstat(s[i].fd[whoami], &sb) != 0) {
			killall(handle);
			errprintf(1, "%d stat on fd %d", get_pid(),
					s[i].fd[whoami]);
		}
		if (s[i].dev != sb.st_rdev) {
			killall(handle);
			errprintf(0, "dev of sema 0x%x doesn't match dev of fd 0x%x",
				s[i].dev, sb.st_dev);
			/* NOTREACHED */
		}
	}
	/* if we could only open some - mark others as unusable */
	for (j = i; s[j].sp; j++)
		s[j].fd[whoami] = -1;

	if (verbose) {
		printf("%s:%d opened %d semaphores", Cmd, get_pid(), i);
		if (s[i].sp == NULL)
			printf("\n");
		else
			printf(" (stopped cause:%s)\n", strerror(err));
	}
	return(i);
}

/* ARGSUSED */
void
closesemas(usptr_t *handle, struct ss *s)
{
	register int i;
	for (i = 0; ; i++) {
		if (s[i].sp == NULL)
			break;
		if (usclosepollsema(s[i].sp) < 0) {
			errprintf(1, "usclosepollsema pid %d i %d",
				get_pid(), i);
			/* NOTREACHED */
		}
	}
}

void
ignunlink(char *f)
{
	register int err = oserror();

	unlink(f);
	setoserror(err);
}
