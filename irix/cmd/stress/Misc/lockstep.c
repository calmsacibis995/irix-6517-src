/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.7 $ $Author: jwag $"

#include "sys/types.h"
#include "unistd.h"
#include "stdlib.h"
#include "signal.h"
#include "getopt.h"
#include "sys/ipc.h"
#include "sys/shm.h"
#include "sys/time.h"
#include "sys/siginfo.h"
#include "sys/procfs.h"
#include "sys/prctl.h"
#include "stdio.h"
#include "fcntl.h"
#include "malloc.h"
#include "sys/times.h"
#include "limits.h"

int verbose = 0;
pid_t parentpid;
int exitonerror = 1;
void errexit(void);
void kid(void);

/*
 * lockstep - single step/write into a locked text region
 */

void
wstopit(int pfd, prstatus_t *psi)
{
	if (ioctl(pfd, PIOCWSTOP, psi) != 0) {
		perror("lockstep:ERROR:PWSTOP");
		errexit();
	}
}

void
stopit(int pfd)
{
	if (ioctl(pfd, PIOCSTOP, NULL) != 0) {
		perror("lockstep:ERROR:PSTOP");
		errexit();
	}
}

void
runit(int pfd, long flags)
{
	prrun_t pr;

	pr.pr_flags = flags;
	if (ioctl(pfd, PIOCRUN, &pr) != 0) {
		perror("lockstep:ERROR:PRUN");
		fprintf(stderr, "lockstep:ERROR:PRUN flags 0x%x\n", flags);
		errexit();
	}
}


char *whytab[] = {
	"UNK",
	"REQ",
	"SIG",
	"SYSENTRY",
	"SYSEXIT",
	"FAULTED",
	"JOBCONTROL"
};
char *whatftab[] = {
	"",
	"FLTILL",
	"FLTPRIV",
	"FLTBPT",
	"FLTTRACE",
	"FLTACCESS",
	"FLTBOUNDS",
	"FLTIOVF",
	"FLTIZDIV",
	"FLTFPE",
	"FLTSTACK",
	"FLTPAGE",
	"FLTPCINVAL",
	"FLTWATCH",
	"FLTKWATCH",
};

void
prps(char *s, prstatus_t *psi)
{
	printf("lockstep:%s:Pid %d flags 0x%x why %s(%d)",
		s, psi->pr_pid, psi->pr_flags,
		whytab[psi->pr_why], psi->pr_why);
	if (psi->pr_why == PR_FAULTED) {
		printf(" what %s(%d)\n",
			whatftab[psi->pr_what], psi->pr_what);
	} else {
		printf(" what (%d)\n",
			psi->pr_what);
	}
	if (psi->pr_why == PR_FAULTED && (psi->pr_what == FLTWATCH ||
			psi->pr_what == FLTKWATCH)) {
		printf("lockstep:%s:Pid %d PC 0x%x code %s%s%s(0x%x) addr 0x%x\n",
			s, psi->pr_pid,
			psi->pr_reg[CTX_EPC],
			psi->pr_info.si_code & MA_READ ? "READ " : "",
			psi->pr_info.si_code & MA_WRITE ? "WRITE " : "",
			psi->pr_info.si_code & MA_EXEC ? "EXEC " : "",
			psi->pr_info.si_code,
			psi->pr_info.si_addr);
	} else {
		printf("lockstep:%s:Pid %d PC 0x%x\n",
			s, psi->pr_pid, psi->pr_reg[CTX_EPC]);
	}
}

/* ARGSUSED */
void
ctrap(int sig, int code)
{
	fprintf(stderr, "pid %d caught SIGTRAP code %d\n",
		getpid(), code);
	abort();
}

int
main(int argc, char **argv)
{
	int c, i;
	char pname[32];
	int pfd;
	int pid;
	prstatus_t psi;
	sigset_t sigs;
	fltset_t faults;
	int flags;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	while((c = getopt(argc, argv, "dv")) != EOF)
	switch (c) {
	case 'd':
		exitonerror = 0;
		break;
	case 'v':
		verbose++;
		break;
	default:
		exit(-1);
	}

	parentpid = getpid();
	sigset(SIGTRAP, ctrap);

	/* fork our own kid */
	if ((pid = fork()) < 0) {
		perror("lockstep:ERROR:fork");
		errexit();
	} else if (pid == 0) {
		kid();
		exit(0);
	}

	/* parent */
	/* open up /debug file */
	sprintf(pname, "/debug/%05d", pid);
	if ((pfd = open(pname, O_RDWR)) < 0) {
		fprintf(stderr, "lockstep:ERROR:Cannot open %s\n", pname);
		perror("");
		errexit();
	}

	/* wait till process has run and locked down its text */
	blockproc(parentpid);

	/* stop process */
	stopit(pfd);
	unblockproc(pid);

	if (ioctl(pfd, PIOCSTATUS, &psi) != 0) {
		perror("lockstep:ERROR:PSTATUS");
		errexit();
	}
	prps("1st stop", &psi);

	/* set up to get SIGTRAP & ALL faults */
	premptyset(&sigs);
	praddset(&sigs, SIGTRAP);
	if (ioctl(pfd, PIOCSTRACE, &sigs) != 0) {
		perror("lockstep:ERROR:PSTRACE");
		errexit();
	}
	prfillset(&faults);
	prdelset(&faults, FLTPAGE);
	if (ioctl(pfd, PIOCSFAULT, &faults) != 0) {
		perror("lockstep:ERROR:PSFAULT");
		errexit();
	}

	flags = PRSTEP;
	for (i = 0; i < 100; i++) {
		runit(pfd, flags);
		flags |= PRCFAULT;
		wstopit(pfd, &psi);
		if (psi.pr_why != PR_FAULTED || psi.pr_what != FLTTRACE) {
			fprintf(stderr, "lockstep:ERROR:expected FAULT\n",
				psi.pr_why, psi.pr_what);
			prps("step test", &psi);
			errexit();
		}
		if (verbose) {
			printf("(0x%x 0x%x) ",
				psi.pr_reg[CTX_EPC], psi.pr_flags);
		}
		if (verbose)
			printf("\n");
	}

	prps("finished", &psi);
	/* let child finish up */
	runit(pfd, PRCFAULT);
	close(pfd);	/* will run child */
	printf("lockstep: done\n");

	return 0;
}

void
kid(void)
{
	pid_t mypid;
	register int i;

	mypid = getpid();
	if (mpin((caddr_t)kid, 1024) < 0) {
		perror("lockstep:ERROR:mpin");
		errexit();
	}
	unblockproc(parentpid);
	blockproc(mypid);
	for (i = 0; i < 1000; i++)
		sginap(0);

}

void
errexit(void)
{
	if (exitonerror)
		abort();
}
