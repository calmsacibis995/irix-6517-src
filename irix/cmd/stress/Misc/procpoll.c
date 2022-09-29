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

#ident	"$Revision: 1.20 $ $Author: kanoj $"

/*
 * This tests 
 *	PIOCSTOP selecting on /proc file descriptors.
 *	PIOCSTOP (PR_STEP) single stepping a process and selecting
 *	single stepping and selecting on two processes at the same time
 *	FLTPAGE
 *	FLTSTACK
 *	PIOCSENTRY
 *	PIOCSEXIT
 *	PIOCFAULT
 * A lot of stuff gets printed on the console by the kernel!
 */

#include "sys/types.h"
#include "signal.h"
#include "sys/ipc.h"
#include "sys/shm.h"
#include "sys/time.h"
#include "sys/siginfo.h"
#include "sys/procfs.h"
#include "stdio.h"
#include "getopt.h"
#include "unistd.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "bstring.h"
#include "fcntl.h"
#include "malloc.h"
#include "sys/times.h"
#include "limits.h"
#include "sys/resource.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "sys/mman.h"
#include "stress.h"

int verbose = 0;
int debug = 0;
pid_t parentpid;
pid_t currpid = -2;
char *Cmd;
int sighand = 0;
int catch = 0;

#define myerror(x,y)	perror("ERROR: " # x ":" # y )
#define vprintf		if (verbose) printf
#define dprintf		if (debug) printf

void pbanner(char *str);
pid_t forkproc(void (*cfunc)(pid_t));
int openproc(pid_t pid);
void csync(void (*func)(pid_t));
void psync(void);
void kidstack(char *addr);
int syscommon(int pfd, pid_t cpid, int ictlid);

int stopnpoll(int, pid_t);
int singlestep(int, pid_t);
int select2(int, pid_t);
int pagefault(int, pid_t);
int sysentry(int, pid_t);
int sysexit(int, pid_t);
int sysstack(int, pid_t);
int parselect(int, pid_t);

void kidloop(pid_t);
void kidfault(pid_t);
void kidsyscall(pid_t);
void kidcatch(pid_t);
void kidignore(pid_t);
void kidselect(pid_t);
void kidblocksyscall(pid_t);

struct test {
	char	*vdesc;
	char	*type;
	void	(*cfunc)(pid_t);
	int	(*pfunc)(int, pid_t);
} tests[] = {
    { "Force stop in sleep\n", "PIOCSTOP/SLEEP", kidselect, parselect },
    { "Force stop and select\n", "PIOCSTOP",  kidloop, stopnpoll },
    { "Single step and select\n", "PR_STEP", kidloop, singlestep },
    { "Single step and select(2)\n", "SELECT(2)", kidloop, select2 },
    { "Page faults\n", "FLTPAGE", kidfault, pagefault },
    { "Syscall entry\n", "PIOCSENTRY", kidsyscall, sysentry },
    { "Syscall exit\n", "PIOCSEXIT", kidsyscall, sysexit },
    { "Stack fault (catch signal)\n", "FLTSTACK", kidcatch, sysstack },
    { "Stack fault (ignore signal)\n", "FLTSTACK", kidignore, sysstack },
    { "Syscall entry (blocking syscall)\n", "PIOCSENTRY", kidblocksyscall, sysentry },
    { "END OF TESTS", "", 0, 0 }
};
struct test *tp;

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
prstatus(char *s, prstatus_t *psp)
{
	printf("%s:%s:Pid %d flags 0x%x why %s(%d)",
		Cmd, s, psp->pr_pid, psp->pr_flags,
		whytab[psp->pr_why], psp->pr_why);
	if (psp->pr_why == PR_FAULTED) {
		printf(" what %s(%d)\n",
			whatftab[psp->pr_what], psp->pr_what);
	} else {
		printf(" what (%d)\n",
			psp->pr_what);
	}
	if (psp->pr_why == PR_FAULTED && (psp->pr_what == FLTWATCH ||
			psp->pr_what == FLTKWATCH)) {
		printf("%s:%s:Pid %d PC 0x%llx code %s%s%s(0x%x) addr 0x%p\n",
			Cmd, s, psp->pr_pid,
			psp->pr_reg[CTX_EPC],
			psp->pr_info.si_code & MA_READ ? "READ " : "",
			psp->pr_info.si_code & MA_WRITE ? "WRITE " : "",
			psp->pr_info.si_code & MA_EXEC ? "EXEC " : "",
			psp->pr_info.si_code,
			psp->pr_info.si_addr);
	} else {
		printf("%s:%s:Pid %d PC 0x%llx\n",
			Cmd, s, psp->pr_pid, psp->pr_reg[CTX_EPC]);
	}
}


/*
 * poll - single step/write into a locked text region
 */
void
errexit(void)
{
	(void) kill(currpid, SIGKILL);
	exit(1);
}

int
wstopit(int pfd, prstatus_t *psp)
{
	if (ioctl(pfd, PIOCWSTOP, psp) != 0) {
		myerror(wstopit, PIOCWSTOP);
		return(-1);
	}
	return(0);
}

int
selwstopit(int pfd, prstatus_t *psp)
{
	fd_set e_mask;
	int nfound;
	int rval = -1;

	FD_ZERO(&e_mask);
	FD_SET(pfd, &e_mask);
	if ((nfound = select(pfd+1, 0, 0, &e_mask, 0)) != 1) {
		if (nfound == -1)
			myerror(selwstopit, select);
		else if (nfound == 0) {
			/*
			 * Not necessarily an error !?
			 */
			fprintf(stderr, "stopselect: select timed out\n");
			rval = 0;
		} else
			fprintf(stderr,
			"poll: ERROR: select failed, count %d\n", nfound);
	} else if (!FD_ISSET(pfd, &e_mask)) {
		myerror(selwstopit, select_rval);
	} else {
		vprintf("select before wstopit succeeded\n");
		rval = 0;
	}
	if (ioctl(pfd, PIOCWSTOP, psp) != 0) {
		myerror(selwstopit, PIOCWSTOP);
		rval = -1;
	}
	return(rval);
}

int
stopit(int pfd)
{
	if (ioctl(pfd, PIOCSTOP, NULL) != 0) {
		myerror(stopit, PIOCSTOP);
		return(-1);
	}
	return 0;
}

int
stopselect(int pfd)
{
	int nfound;
	fd_set rfd, wfd, efd;
	struct timeval tval;
	int rval = -1;

	FD_ZERO(&rfd);
	FD_ZERO(&wfd);
	FD_ZERO(&efd);
	FD_SET(pfd, &efd);
	tval.tv_sec = 15;
	tval.tv_usec = 0;

	if ((nfound = select(pfd+1, &rfd, &wfd, &efd, &tval)) != 1) {
		if (nfound == -1)
			myerror(stopselect, select);
		else if (nfound == 0)
			fprintf(stderr, "stopselect: select timed out\n");
		else
			fprintf(stderr,
			"poll: ERROR: select failed, count %d\n", nfound);
	} else if (!FD_ISSET(pfd, &efd))
		myerror(stopselect, notselected);
	else {
		vprintf("select succeeded\n");
		rval = 0;
	}
	return(rval);
}

int
runit(int pfd, long flags)
{
	prrun_t pr;

	pr.pr_flags = flags;
	if (ioctl(pfd, PIOCRUN, &pr) != 0) {
		myerror(runit, PIOCRUN);
		fprintf(stderr, "ERROR:PIOCRUN flags 0x%x\n", flags);
		return(-1);
	}
	return 0;
}

int
main(int argc, char **argv)
{
	int pfd, c, i;
	int ntest = 0;
	int fails = 0;
	int utlbcount = 0;

	Cmd = errinit(argv[0]);
	tp = &tests[0];
	setlinebuf(stdout);
	setlinebuf(stderr);
	prctl(PR_COREPID, 0, 1);

	while((c = getopt(argc, argv, "uvdn:")) != EOF)
	switch (c) {
	case 'u':
		utlbcount = TLB_COUNT;
		break;
	case 'v':
		verbose++;
		break;
	case 'd':
		debug++;
		verbose++;
		break;
	case 'n':
		if ((ntest = atoi(optarg)) != 0)
			printf("Running test number %d\n", ntest);
		break;
	default:
		exit(-1);
	}
	parentpid = getpid();

	for (i = 1; tp->cfunc != NULL; tp++, i++) {
		if ((ntest) && (i != ntest))
			continue;
		pbanner(tp->vdesc);
		if ((currpid = forkproc(tp->cfunc)) == -2)
			errexit();
		if ((pfd = openproc(currpid)) == -1)
			errexit();
		if (utlbcount) {
			if (ioctl(pfd, PIOCTLBMISS, &utlbcount)) {
				fprintf(stderr, "PIOCTLBMISS failed:%s\n", 
					strerror(errno));
				errexit();
			}
		}
		dprintf("Calling pfunc for parent\n");
		if ((*tp->pfunc)(pfd, currpid) < 0) {
			fprintf(stderr, "FAILED: %s\n", tp->type);
			fails++;
		} else
			fprintf(stderr, "PASSED: %s\n", tp->type);
		(void) close(pfd);
		(void) kill(currpid, SIGKILL);
	}
	if (fails)
		abort();
	return 0;
}

void
pbanner(char *str)
{
	printf("--------------------------------------------------\n");
	printf("--------------------------------------------------\n");
	printf("BEGIN : %s", str);
}

/*
 * Fork a child, synchronize with parent using blockproc/unblockproc.
 * Child never returns, parent returns, getting back the childpid.
 */
pid_t
forkproc(void (*cfunc)(pid_t))
{
	pid_t pid;
	pid_t ppid;

	ppid = getpid();
	if ((pid = fork()) < 0) {
		myerror(forkproc, fork);
		return(-2);
	} else if (pid == 0) {
		csync(cfunc);
		dprintf("Calling cfunc for child\n");
		(*cfunc)(ppid);
		exit(0);
	} else {
		psync();
		return(pid);
	}
	/* NOTREACHED */
}

int
openproc(pid_t pid)
{
	char pname[32];
	int pfd;

	sprintf(pname, "/debug/%05d", pid);
	if ((pfd = open(pname, O_RDWR)) < 0) {
		myerror(openproc, open);
		fprintf(stderr, "Cannot open %s\n", pname);
		return(-1);
	}
	dprintf("Opened /proc file %s\n", pname);
	return(pfd);
}

void
csync(void (*func)(pid_t))
{
	pid_t mypid = getpid();

	dprintf("Child(%d) synchronizing.\n", mypid);
	if (mpin((void *)func, 1024) < 0) {
		myerror(csync, func);
		exit(0);
	}
	unblockproc(parentpid);
	blockproc(mypid);
}

void
psync(void)
{
	dprintf("Parent(%d) synchronizing.\n", parentpid);
	blockproc(parentpid);
}

/*
 * Stop process and try selecting
 */
int
stopnpoll(int pfd, pid_t cpid)
{
	int rval;

	if ((rval = stopit(pfd)) == -1)
		abort();
	unblockproc(cpid);
	rval = stopselect(pfd);
	return(rval);
}

/* ARGSUSED */
void
kidloop(pid_t ppid)
{
	register int i;

	for (i = 0; i < 10000; i++)
		sginap(0);

}

/*
 * Single step child process
 */
int
initialize(int pfd, int fltentry)
{
	prstatus_t status;
	prpsinfo_t psi;
	fltset_t faults;

	if (stopit(pfd) == -1)
		return(-1);
	if (ioctl(pfd, PIOCSTATUS, &status) != 0) {
		myerror(initialize, PIOCSTATUS);
		return(-1);
	}
	prstatus("1st Stop", &status);
	if (ioctl(pfd, PIOCPSINFO, &psi) != 0) {
		myerror(initialize,PIOCPSINFO);
	} else
		fprintf(stderr, "\tstate %d (%c)\n",
			psi.pr_state,
			psi.pr_sname);
	premptyset(&faults);
	if (fltentry)
		praddset(&faults, fltentry);
	else {
		/* disable page faults */
		prfillset(&faults);
		prdelset(&faults, FLTPAGE);
	}
	if (ioctl(pfd, PIOCSFAULT, &faults) != 0) {
		myerror(initialize, PIOCSFAULT);
		return(-1);
	}
	return 0;
}

int
singlestep(int pfd, pid_t cpid)
{
	int i;
	prstatus_t status;
	int flags;

	if (initialize(pfd, 0) == -1)
		abort();
	unblockproc(cpid);
	flags = PRSTEP;
	for (i=0; i < 100; i++) {
		if (runit(pfd, flags) == -1)
			abort();
		flags |= PRCFAULT;
		if (stopselect(pfd) == -1)
			abort();
		if (wstopit(pfd, &status) == -1)
			abort();
		if (status.pr_why != PR_FAULTED ||
		    status.pr_what != FLTTRACE) {
			fprintf(stderr,"ERROR: singlestep: expected FAULT\n");
			prstatus("step test", &status);
			abort();
		}
		vprintf("(0x%llx 0x%x) \n", status.pr_reg[CTX_EPC], status.pr_flags);
	}
	if (runit(pfd, PRCFAULT) == -1)
		abort();
	return(0);
}

/*
 * Fork one more process and select both together
 */
int
select2(int pfd, pid_t cpid)
{
	int pfd2, cpid2;
	int rval = 0, i;
	prstatus_t status;
	int flags;

	if (initialize(pfd, 0) == -1)
		abort();
	unblockproc(cpid);
	if ((cpid2 = forkproc(tp->cfunc)) == -1)
		abort();
	if ((pfd2 = openproc(cpid2)) == -1) {
		(void) kill(cpid2, SIGKILL);
		(void) kill(cpid, SIGKILL);
		abort();
	}
	unblockproc(cpid2);
	if (initialize(pfd2, 0) == -1) {
		(void) kill(cpid2, SIGKILL);
		abort();
	}

	flags = PRSTEP;
	for (i=0; i < 100; i++) {
		if ((rval = runit(pfd, flags)) == -1)
			goto out;
		if ((rval = runit(pfd2, flags)) == -1)
			goto out;
		flags |= PRCFAULT;
		if ((rval = stopselect(pfd)) == -1)
			goto out;
		if ((rval = wstopit(pfd, &status)) == -1)
			goto out;
		if (status.pr_why != PR_FAULTED ||
		    status.pr_what != FLTTRACE) {
			fprintf(stderr,"ERROR: select2/1: expected FAULT\n");
			prstatus("step test", &status);
			rval = -1;
			goto out;
		}
		vprintf("(0x%llx 0x%x) \n", status.pr_reg[CTX_EPC], status.pr_flags);
		if ((rval = stopselect(pfd2)) == -1)
			goto out;
		if ((rval = wstopit(pfd2, &status)) == -1)
			goto out;
		if (status.pr_why != PR_FAULTED ||
		    status.pr_what != FLTTRACE) {
			fprintf(stderr,"ERROR: select2/2: expected FAULT\n");
			prstatus("select(2) test", &status);
			rval = -1;
			goto out;
		}
		vprintf("(0x%llx 0x%x) \n", status.pr_reg[CTX_EPC], status.pr_flags);
	}
	rval = runit(pfd, PRCFAULT);
out:
	(void) kill(cpid2, SIGKILL);
	(void) close(pfd2);
	if (rval == -1)
		abort();
	return(rval);
}

#define FSIZE	20000
/*
 * page faults
 */
/* ARGSUSED */
void
kidfault(pid_t ppid)
{
	int cfd, i, fd, pagesz;
	char *pageflt, *afile;
	char buf[FSIZE];
	prusage_t psu;

	afile = tempnam(NULL, "procpoll");
	if ((fd = open(afile, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0) {
		fprintf(stderr, "procpoll:kid open of %s failed:%s\n",
			afile, strerror(errno));
		exit(1);
	}
	unlink(afile);
	write(fd, buf, FSIZE);

	pageflt = mmap(NULL, FSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (pageflt == (char *)-1L) {
		fprintf(stderr, "procpoll:kid mmap of %s failed:%s\n",
			afile, strerror(errno));
		exit(1);
	}
	if (msync(pageflt, FSIZE, MS_INVALIDATE) != 0) {
		fprintf(stderr, "procpoll:kid msync of %s failed:%s\n",
			afile, strerror(errno));
		exit(1);
	}

	if ((cfd = openproc(getpid())) == -1)
		exit(1);
	if (ioctl(cfd, PIOCUSAGE, &psu) != 0) {
		myerror(kidfault, PIOCUSAGE);
		exit(1);
	}
	printf("procpoll:kid min %d maj %d\n",
		psu.pu_minf, psu.pu_majf);
	
	/* note that due to paging, we may get more faults
	 * than just 50
	 */
	pagesz = getpagesize();
	for (i = 0; i < 50; i++) {
		vprintf("procpoll:kid accessing 0x%x\n", pageflt);
		pageflt[pagesz+i]++;
		/*sginap(0);*/
		if (msync(pageflt, FSIZE, MS_INVALIDATE) != 0) {
			fprintf(stderr, "procpoll:kid msync of %s failed:%s\n",
				afile, strerror(errno));
			exit(1);
		}
	}
}

int
pagefault(int pfd, pid_t cpid)
{
	int i;
	prstatus_t status;
	int flags;
	prusage_t psu;

	if (initialize(pfd, FLTPAGE) == -1)
		abort();
	unblockproc(cpid);
	flags = 0;
	for (i=0; i < 50; i++) {
		if (runit(pfd, flags) == -1)
			abort();
		flags |= PRCFAULT;
		if (stopselect(pfd) == -1)
			abort();
		if (wstopit(pfd, &status) == -1)
			abort();
		if (status.pr_why != PR_FAULTED ||
		    status.pr_what != FLTPAGE) {
			fprintf(stderr,"ERROR: pagefault: expected FLTPAGE\n");
			prstatus("fault page test", &status);
			abort();
		}
		vprintf("(0x%llx 0x%x) \n", status.pr_reg[CTX_EPC], status.pr_flags);
		vprintf("pid %d stopped for FLTPAGE\n", cpid);
	}
	if (ioctl(pfd, PIOCUSAGE, &psu) != 0) {
		myerror(pagefault, PIOCUSAGE);
		abort();
	}
	printf("procpoll:kid min %d maj %d\n",
		psu.pu_minf, psu.pu_majf);

	/*rval = runit(pfd, PRCFAULT);*/
	return(0);
}

void
kidbskid(void)
{
	dprintf("kidbskid::Hello\n");
	unblockproc(getppid());
	dprintf("kidbskid::Done\n");
}

typedef  void (*SPROCFUNC) (void*);

/* ARGSUSED */
void
kidblocksyscall(pid_t ppid)
{
	int 	i;

	dprintf("kidblocksyscall:Enter\n");
	sleep(3); /* to synchronize with parent */
	if (sproc((SPROCFUNC) kidbskid, PR_BLOCK|PR_NOLIBC) < 0) {
		perror("sproc failed\n");
		exit(-1);
	}
	dprintf("kidblocksyscall::Hello\n");
	for (i = 0; i < 100; i++) {
		(void) getpid();
		sginap(0);
	}
	/* process may be killed before this print is executed */
	dprintf("kidblocksyscall::Exit\n");
}

int
handlesysblock(pid_t vpid, int vpfd, prstatus_t vstatus)
{
	unsigned long mask;
	gregset_t 	gr;
	pid_t	 	scpid; /* sproc child */
	prstatus_t 	status;
	sysset_t 	sysval;

	mask = (unsigned long)vstatus.pr_sysarg[1];
	if (mask & PR_BLOCK) {
		dprintf("**** Unless mask 0x%lx is changed to 0x%lx,"
			"this process will block\n", mask, mask & (~PR_BLOCK));
	} else return 0;

	if (ioctl(vpfd, PIOCGREG, gr) != 0) {
		myerror(handlesysblock, PIOCGREG);
		return(-1);
	}
	dprintf("*** A1 is 0x%lx\n", gr[CTX_A1]);
	gr[CTX_A1] = mask & (~PR_BLOCK);

	if (ioctl(vpfd, PIOCSREG, gr) != 0) {
		myerror(handlesysblock, PIOCSREG);
		return(-1);
	}
	dprintf("*** A1 is reset! (is 0x%lx)\n", gr[CTX_A1]);

	/*
	 * Let the child go free; we are not interested in it
	 * except that it should eventually execute an unblockproc().
	 */
	mask = PR_FORK;
	if (ioctl(vpfd, PIOCRESET, &mask) != 0) {
		myerror(handlesysblock, PIOCRESET);
		return(-1);
	}
	dprintf("*** Child is free to run\n");
	/*
	 * Catch the syscall exit, so we know the pid of
	 * the child sproc - jfk (just for kicks!)
	 */
	prfillset(&sysval);
	if (ioctl(vpfd, PIOCSEXIT, &sysval) != 0) {
		myerror(handlesysblock, PIOCSEXIT);
		return(-1);
	}
	dprintf("*** Monitoring PIOCSEXIT\n");
	if (runit(vpfd, 0) == -1)
		return(-1);
	dprintf("*** Running for PIOCSEXIT\n");
	if (stopselect(vpfd) == -1)
		return(-1);
	if (wstopit(vpfd, &status) == -1)
		return(-1);
	if (status.pr_why != PR_SYSEXIT ||
	    status.pr_what == 0) {
		fprintf(stderr,"ERROR: handlesysblock: sproc PR_SYSEXIT\n");
		prstatus("sproc syscall exit test", &status);
		return(-1);
	}
	if (status.pr_what != SYS_sproc) { /* sproc */
		fprintf(stderr,"ERROR: handlesysblock: PR_SYSEXIT, expected %d, got %d\n", SYS_sproc, status.pr_what);
		return(-1);
	}
	scpid = (pid_t)status.pr_rval1;
	dprintf("*** Child's pid = %d\n", scpid);

	/*
	 * Reset to trace ENTRY of parent --- set EXIT trace to null set.
	 */
	premptyset(&sysval);
	if (ioctl(vpfd, PIOCSEXIT, &sysval) != 0) {
		myerror(handlesysblock, PIOCEXIT);
		return(-1);
	}
	blockproc(vpid);
	dprintf("*** Blocking %d\n", vpid);
	return 0;
}


/*
 * Syscall entry and exit
 */
/* ARGSUSED */
void
kidsyscall(pid_t ppid)
{
	int i;
	struct timeval tv;

	for (i = 0; i < 100; i++) {
		(void) profil((unsigned short *)(ptrdiff_t)1000, 2000, 3000, 0);
		(void) getpid();
		(void) gettimeofday(&tv);
		sginap(0);
	}
}

int
sysentry(int pfd, pid_t cpid)
{
	return(syscommon(pfd, cpid, PIOCSENTRY));
}

int
sysexit(int pfd, pid_t cpid)
{
	return(syscommon(pfd, cpid, PIOCSEXIT));
}

int
syscommon(int pfd, pid_t cpid, int ictlid)
{
	sysset_t sysval;
	prstatus_t status;
	prpsinfo_t psi;
	int rval, flags, i, j;
	int why;

	if (ictlid == PIOCSENTRY)
		why = PR_SYSENTRY;
	else
		why = PR_SYSEXIT;
	if (stopit(pfd) == -1)
		abort();
	if (ioctl(pfd, PIOCSTATUS, &status) != 0) {
		myerror(syscommon, PIOCSTATUS);
		abort();
	}
	prstatus("1st Stop", &status);
	if (ioctl(pfd, PIOCPSINFO, &psi) != 0) {
		myerror(syscommon,PIOCPSINFO);
	} else
		fprintf(stderr, "\tstate %d (%c) flags 0x%lx\n",
			psi.pr_state,
			psi.pr_sname,
			psi.pr_flag);
	prfillset(&sysval);
	if (ioctl(pfd, ictlid, &sysval) != 0) {
		myerror(syscommon, PIOCSENTRY_EXIT);
		abort();
	}
	unblockproc(cpid);

	flags = 0;
	for (i=0; i < 100; i++) {
		if (runit(pfd, flags) == -1)
			abort();
		if (stopselect(pfd) == -1)
			abort();
		if (wstopit(pfd, &status) == -1)
			abort();
		if (status.pr_why != why ||
		    status.pr_what == 0) {
			fprintf(stderr,"ERROR: syscommon: PR_SYSENTRY_EXIT\n");
			prstatus("syscall entry/exit test", &status);
			if (ioctl(pfd, PIOCPSINFO, &psi) != 0) {
				myerror(syscommon,PIOCPSINFO);
			} else
				fprintf(stderr, "\tstate %d (%c)\n",
					psi.pr_state,
					psi.pr_sname);
			abort();
		}
		if (status.pr_what == SYS_sproc) { /* sproc */
			dprintf("*** Caught sproc (%d)\n", SYS_sproc);
			handlesysblock(cpid, pfd, status);
		}

		vprintf("syscall %d nargs %d\n", status.pr_what,
			status.pr_nsysarg);
		if ((verbose) && status.pr_nsysarg) {
			for (j = 0; (j < 10) && (j < status.pr_nsysarg); j++)
				printf("0x%lx ", status.pr_sysarg[j]);
			printf("\n");
		}
	}
	rval = runit(pfd, 0);
	return(rval);
}

/*
 * Handler FLTSTACK
 */
void
sighandler(int sig)
{
	vprintf("entering sighandler");
	if (sig == SIGSEGV) {
		if (catch)
			signal(SIGSEGV, sighandler);
		else
			signal(SIGSEGV, SIG_IGN);
		vprintf(" SIGSEGV ");
		sighand++;
	}
	vprintf("\n");
}

/* ARGSUSED */
void
kidcatch(pid_t ppid)
{
	catch = 1;
	kidstack(NULL);
}

/* ARGSUSED */
void
kidignore(pid_t ppid)
{
	catch = 0;
	printf("Ignore console message 'Process has been killed to prevent infinite loop'\n");
	printf("that is a result of this test. It is a normal part of the test.\n");
	sginap(0);
	kidstack(NULL);
}

/* ARGSUSED */
void
kidstack(char *addr)
{
	static int first = 1, counter = 0;
	struct rlimit rl;
	char fillstack[1024*40];

	if (first) {
		rl.rlim_cur = 1024*1024;
		rl.rlim_max = 1024*1024*2;
		setrlimit(RLIMIT_STACK, &rl);
		first = 0;
		if (catch)
			signal(SIGSEGV, sighandler);
		else
			signal(SIGSEGV, SIG_IGN);
	}

	vprintf("Iteration %d\n", counter++);
	kidstack(&fillstack[0]);
}

int
sysstack(int pfd, pid_t cpid)
{
	int rval, i;
	prstatus_t status;
	long flags;
	auto long p;

	if (initialize(pfd, FLTSTACK) == -1)
		abort();
	unblockproc(cpid);
	flags = 0;
	for (i=0; i < 50; i++) {
		if ((rval = runit(pfd, flags)) == -1)
			abort();
		flags |= PRCFAULT;
		if ((rval = stopselect(pfd)) == -1)
			abort();
		if (wstopit(pfd, &status) == -1)
			abort();
		if (status.pr_why != PR_FAULTED ||
		    status.pr_what != FLTSTACK) {
			fprintf(stderr,"ERROR: sysstack: expected FLTSTACK\n");
			prstatus("fault stack test", &status);
			abort();
		}
		vprintf("(0x%llx 0x%x) \n", status.pr_reg[CTX_EPC], status.pr_flags);
		vprintf("pid %d stopped for FLTSTACK\n", cpid);
	}
	/*
	 * process has blown its stack - just kill it
	 */
	p = SIGKILL;
	rval = ioctl(pfd, PIOCKILL, &p);
	return(rval);
}

#define MAGIC	7777
/* ARGSUSED */
void
kidselect(pid_t ppid)
{
	int s, i;
	struct sockaddr_in name;
	fd_set rfd, wfd, efd;
	char buffer[2048];
	ssize_t n;

	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = INADDR_ANY;
	name.sin_port = MAGIC;
	if (bind(s, &name, (int)sizeof(name))) {
		perror("procpoll:bind");
		exit(1);
	}

	FD_ZERO(&rfd);
	FD_ZERO(&wfd);
	FD_ZERO(&efd);
	FD_SET(s, &rfd);
	for (i = 0; i < 5 ; i++) {
	    if (select(s+1, &rfd, &wfd, &efd, 0) == -1) {
		    perror("select");
		    exit(1);
	    } else {
		    dprintf ("%d: Process returned from select!!\n", i);
		    if ((n = read(s, buffer, sizeof(buffer))) != 1024) {
			    fprintf(stderr,
			    "ERROR: Read %d bytes instead of 1024\n", n);
			    exit(1);
		    }
		    dprintf("Process read %d bytes\n", n);
		    fflush(stdout);
	    }
	}
}

/*
 * This ensures that we can stop a process in the middle of a sleep in
 * the kernel. There are two cases here:
 * 1. The process is asleep and via /proc we stop it and set it running
 *    before a wakeup is issued.
 * 2. The processs is asleep and via /proc we stop it, it gets a wakeup
 *    because the event it is sleeping on is now true. The  process 
 *    should stop on exit from syscall(). /proc now starts it running again
 */
int
parselect(int pfd, pid_t cpid)
{
	prstatus_t status;
	int s, length;
	struct sockaddr_in name, to;
	char buffer[1024];
	sysset_t syscallset;

	if (stopit(pfd) == -1)
		abort();
	
	unblockproc(cpid);
	sleep(2);

	/*
	 * Child is now running. Stop it.
	 */
	if (stopit(pfd) == -1)
		abort();
	if (wstopit(pfd, &status) == -1)
		abort();
	if (verbose) prstatus("sleep/select stop 1", &status);
	if ((status.pr_why != PR_REQUESTED) || (status.pr_what != 0)) {
		fprintf(stderr, "ERROR: stop in sleep didn't work\n");
		abort();
	}
	/*
	 * Run it and stop it again. It should now be stopped inside of
	 * the sleep in select().
	 */
	if (runit(pfd, 0) == -1)
		abort();
	sleep(2);
	if (stopit(pfd) == -1)
		abort();
	if (wstopit(pfd, &status) == -1)
		abort();
	if (verbose) prstatus("sleep/select stop 2", &status);
	if ((status.pr_why != PR_REQUESTED) || (status.pr_what != 0)) {
		fprintf(stderr, "ERROR: stop in sleep didn't work\n");
		abort();
	}
	/*
	 * Now child will run again but continue sleeping in select()
	 */
	if (runit(pfd, 0) == -1)
		abort();

	/*
	 * Open connection so that we can send data to child to wake it
	 * up from its sleep in select()
	 */
	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		abort();
	}
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = INADDR_ANY;
	name.sin_port = 0;
	if (bind(s, &name, (int)sizeof(name))) {
		perror("bind");
		abort();
	}
	length = sizeof(struct sockaddr_in);
	if (getsockname(s, &name, &length)) {
		perror("getsockname");
		abort();
	}
	bcopy(&name, &to, sizeof(struct sockaddr_in));
	to.sin_port = MAGIC;

	/*
	 * Child will wakeup from sleep in select and sleep on SYSEXIT from read
	 */
	premptyset(&syscallset);
	praddset(&syscallset, SYS_read);
	if (ioctl(pfd, PIOCSEXIT, &syscallset) < 0) {
		perror("ioctl");
		abort();
	}
	if (sendto(s, buffer, sizeof(buffer), 0, &to, sizeof(to)) == -1) {
		perror("sendto");
		abort();
	}
	if (wstopit(pfd, &status) == -1)
		abort();
	if (verbose) prstatus("sleep/select stop 3", &status);
	if ((status.pr_why != PR_SYSEXIT) || (status.pr_what != SYS_read)) {
		fprintf(stderr, "ERROR: stop in sleep didn't work\n");
		abort();
	}
	sleep(2);

	/*
	 * Child will now read in the 1024 bytes sent and then call select()
	 * again
	 */
	if (runit(pfd, 0) == -1)
		abort();
	sleep(5);			/* give it a chance to read it */
	if (stopit(pfd) == -1)
		abort();
	if (wstopit(pfd, &status) == -1)
		abort();
	if (verbose) prstatus("sleep/select stop 4", &status);
	if ((status.pr_why != PR_REQUESTED) || (status.pr_what != 0)) {
		fprintf(stderr, "ERROR: stop in sleep didn't work\n");
		abort();
	}


	/*
	 * Send it some data so that it is woken up when it is stopped and
	 * asleep in select() ie SPRSTOP is set
	 */
	if (sendto(s, buffer, sizeof(buffer), 0, &to, sizeof(to)) == -1) {
		perror("sendto");
		abort();
	}
	sleep(5);			/* give data chance to be queued */
	/*
	 * The child should have woken up from select, but it will be
	 * stopped on SYSEXIT from select since SPRSTOP is set.
	 */
	if (wstopit(pfd, &status) == -1)
		return(-1);
	if (verbose) prstatus("sleep/select stop 5", &status);
	if ((status.pr_why != PR_REQUESTED) || (status.pr_what != 0)) {
		fprintf(stderr, "ERROR: stop in sleep didn't work\n");
		abort();
	}
	/*
	 * Now run it. Child should return from select and sleep on exit from
	 * read.
	 */
	if (runit(pfd, 0) == -1)
		abort();
	if (selwstopit(pfd, &status) == -1)
		abort();
	if (wstopit(pfd, &status) == -1)
		abort();
	if (verbose) prstatus("sleep/select stop 6", &status);
	if ((status.pr_why != PR_SYSEXIT) || (status.pr_what != SYS_read)) {
		fprintf(stderr, "ERROR: stop in sleep didn't work\n");
		abort();
	}
	/*
	 * Clear everything and set it running again. Child will call select()
	 * again and sleep there.
	 */
	premptyset(&syscallset);
	if (ioctl(pfd, PIOCSEXIT, &syscallset) < 0) {
		perror("ioctl");
		abort();
	}
	sleep(2);
	return(0);
}
