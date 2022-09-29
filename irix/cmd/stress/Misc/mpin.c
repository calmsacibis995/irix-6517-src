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

#ident "$Revision: 1.12 $"

#include <stdio.h>
#include <signal.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <getopt.h>

/* hold info about process's virtual address spaces */
struct pseg {
	caddr_t addr;		/* start addr */
	size_t len;		/* length in bytes */
};

/* hold info about each mpin - to allow unpins */
struct pininfo {
	caddr_t addr;
	size_t len;
};

void slave();
void sigc();
int rpin(struct pseg *sp, int nseg, int npins);
void cexit(void);

extern int _ftext[], etext[], _fdata[];
extern char *sys_errlist[];
int		verbose = 0;
int		nprocs = 4;
int		nloops = 100;
int		nsegs = 10;
int		npins = 1000;
int		doexit = 0;
struct pseg *psegtbl;
char *s1, *s2;
int sid1, sid2;

char dummy;

static int pagesize;

/* Use reasonable segment sizes - these match the historical segment
 * sizes for 64-bit and 32-bit machines, based on page size for those
 * machines.
 */
#define	SEGSIZE_16K	0x2000000
#define	SEGSIZE_4K	0x400000
#define stob(x)		((x) * (pagesize == 16384 ? SEGSIZE_16K : SEGSIZE_4K))

int
main(int argc, char **argv)
{
	register int	i;
	int	c;
	char fake[0x10000];

	pagesize = getpagesize();
	while ((c = getopt(argc, argv, "p:l:n:v")) != EOF)
		switch(c) {
		case 'v':
			verbose++;
			break;
		case 'n':	/* npins */
			npins = atoi(optarg);
			break;
		case 'p':	/* nprocs */
			nprocs = atoi(optarg);
			break;
		case 'l':	/* loops */
			nloops = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage:mpin [-p procs][-n pins][-l loops]\n");
			exit(-1);
		}

	sigset(SIGTERM, sigc);
	sigset(SIGINT, sigc);
	sigset(SIGHUP, sigc);
	/* set up segments */
	if ((psegtbl = (struct pseg *)malloc(nsegs * sizeof(*psegtbl))) == NULL) {
		perror("mpin:no mem");
		exit(-1);
	}
	i = 0;
	psegtbl[i].addr = (char *)_ftext;
	psegtbl[i++].len = (char *)etext - (char *)_ftext;
	psegtbl[i].addr = (char *)_fdata;
	psegtbl[i++].len = (char *)sbrk(0) - (char *)_fdata;
	psegtbl[i].addr = &fake[0];
	dummy = *(psegtbl[i].addr);
	psegtbl[i++].len = 0x10000;

	/* set up 2 shd mem segments that abut each other and call it one seg */
	if ((sid1 = shmget(IPC_PRIVATE, stob(1), IPC_CREAT|0777)) < 0) {
		perror("mpin shmget failed");
		goto bad;
	}
	if ((sid2 = shmget(IPC_PRIVATE, stob(1), IPC_CREAT|0777)) < 0) {
		perror("mpin shmget failed");
		goto bad;
	}
	if ((s1 = shmat(sid1, (caddr_t)0x20000000L, 0)) == (char *)-1L) {
		perror("mpin:shmat failed");
		goto bad;
	}
	if ((s2 = shmat(sid1, (caddr_t)0x20000000L+stob(1), 0)) == (char *)-1L) {
		perror("mpin:shmat failed");
		goto bad;
	}
	psegtbl[i].addr = s1;
	psegtbl[i++].len = stob(2);
	nsegs = i;

	/* try out locks once */
	printf("Pin test - single process\n");
	rpin(psegtbl, nsegs, npins);

	/* spawn sproc guys */
	prctl(PR_SETEXITSIG, SIGTERM);

	printf("Pin test - multiple sproc processes\n");
	for(i = 0; i < nprocs; i++) {
		if (sproc(slave, PR_SALL, 0) < 0) {
			perror("mpin:sproc failed");
			goto bad;
		}
	}

	/* now run for a while */
	for (i = 0; i < nloops; i++) {
		if (doexit)
			break;
		rpin(psegtbl, nsegs, npins);
	}

	doexit++;
	errno = 0;
	while (wait(0) != -1 || errno != ECHILD)
		errno = 0;
	/* Only the parent should call cexit - multiple
	 * calls to shmdt and shmctl(IPC_RMID) from within a
	 * single share group are bogus - only the first caller
	 * will succeed. Having the children call cexit while other
	 * children are still pinning/unpinning leads to error messages
	 * from the rpin routine.
	 */
	cexit();

	exit(0);
	/* NOTREACHED */
bad:
	cexit();
	exit(-1);
	/* NOTREACHED */
}

void
sigc(signo)
{
	/* note that printing in a signal handler is basically
	 * a bad idea since we might have interrupted a signal
	 */
	if (verbose)
		printf("mpin:received signal %d\n", signo);
	doexit++;
}

void
cexit(void)
{
	shmdt(s1);
	shmdt(s2);
	shmctl(sid1, IPC_RMID);
	shmctl(sid2, IPC_RMID);
}

/*
 * rpin - randomly pin/unpin pages from a process' segments
 */
int
rpin(struct pseg *sp, int nseg, int npins)
{
	register struct pininfo *pi;
	register int i, wseg;
	size_t off;

	if ((pi = (struct pininfo *)calloc(npins, sizeof(*pi))) == NULL) {
		perror("mpin:rpin no mem");
		exit(-1);
	}

	for (i = 0; i < npins; i++) {
		wseg = rand() % nseg;
		off = rand() % (sp[wseg].len-1);
		pi[i].addr = sp[wseg].addr + off;
		pi[i].len = rand() % (sp[wseg].len - off);
		if (mpin(pi[i].addr, pi[i].len) < 0) {
			fprintf(stderr, "mpin: pin failed at 0x%lx for %d bytes (%s)\n",
				pi[i].addr, pi[i].len, sys_errlist[errno]);
			fprintf(stderr, "off 0x%x wseg %d spaddr 0x%lx splen 0x%x\n",
				off, wseg, sp[wseg].addr, sp[wseg].len);
			break;
		}

	}

	/* now free */
	for (i = 0; i < npins; i++) {
		if (pi[i].addr)
			if (munpin(pi[i].addr, pi[i].len) < 0) {
				if (errno != EINVAL)
					fprintf(stderr, "mpin: unpin failed at 0x%x for %d bytes (%s)\n",
					pi[i].addr, pi[i].len, sys_errlist[errno]);
			}
	}

	free(pi);
	return(0);
}

void
slave()
{
	for(;;) {
		if (doexit) {
			exit(-1);
		}
		rpin(psegtbl, nsegs, npins);
		sginap(1);
	}
}
