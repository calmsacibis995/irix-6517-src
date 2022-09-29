/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.13 $ $Author: jwag $"

#include "sys/types.h"
#include "signal.h"
#include "errno.h"
#include "unistd.h"
#include "stdlib.h"
#include "string.h"
#include "sys/ucontext.h"
#include "sys/time.h"
#include "sys/siginfo.h"
#include "sys/procfs.h"
#include "stdio.h"
#include "getopt.h"
#include "fcntl.h"
#include "malloc.h"
#include "sys/times.h"
#include "limits.h"

/*
 * snoopw - when pointed at a pid, set up watchpoints and snoop
 * on it
 */
int dataonly = 0;
int verbose = 0;
int debug = 0;
int leave = 0;
int removeold = 0;
int all = 0;
int exitonerror = 1;
void catch();
void errexit(void);
void delallwatch(int pfd, prwatch_t *wp, int nwp);
void prwatch(prwatch_t *wp, int nwp);
prmap_t *readmap(int pfd);
prwatch_t *getwatch(int pfd, int *nwp);
prwatch_t *findwatch(prwatch_t *wp, int nwp, caddr_t vaddr);
int getnwatch(int pfd);

#define EXEC 11+1
#define EXECE 59+1
#define SPROC 129+1
#define SPROCSP 132+1
#define EXIT 1+1

int
wstopit(int pfd, prstatus_t *psi)
{
	if (ioctl(pfd, PIOCWSTOP, psi) != 0) {
		if (errno != EINTR) {
			perror("snoopw:ERROR:PWSTOP");
			errexit();
			if (errno == ENOENT)
				exit(-1);
			return(-1);
		} else
			perror("snoopw:PWSTOP");
	}
	return(0);
}

int
stopit(int pfd)
{
	if (ioctl(pfd, PIOCSTOP, NULL) != 0) {
		perror("snoopw:ERROR:PSTOP");
		errexit();
		if (errno == ENOENT)
			exit(-1);
		return(-1);
	}
	return(0);
}

int
runit(int pfd, long flags)
{
	prrun_t pr;

	pr.pr_flags = flags;
	if (ioctl(pfd, PIOCRUN, &pr) != 0) {
		perror("snoopw:ERROR:PRUN");
		errexit();
		if (errno == ENOENT)
			exit(-1);
		return(-1);
	}
	return(0);
}

int
setwatch(int pfd, caddr_t addr, u_long size, u_long mode)
{
	prwatch_t pw;

	pw.pr_vaddr = addr;
	pw.pr_size = size;
	pw.pr_wflags = mode;
	return(ioctl(pfd, PIOCSWATCH, &pw));
}

int
clrwatch(int pfd, caddr_t addr)
{
	prwatch_t pw;

	pw.pr_vaddr = addr;
	pw.pr_size = 0;
	return(ioctl(pfd, PIOCSWATCH, &pw));
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
	printf("snoop:%s:Pid %d flags 0x%lx why %s(%d)",
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
		printf("snoop:%s:Pid %d PC 0x%p code %s%s%s(0x%x) addr 0x%p\n",
			s, psi->pr_pid,
			psi->pr_reg[CTX_EPC],
			psi->pr_info.si_code & MA_READ ? "READ " : "",
			psi->pr_info.si_code & MA_WRITE ? "WRITE " : "",
			psi->pr_info.si_code & MA_EXEC ? "EXEC " : "",
			psi->pr_info.si_code,
			psi->pr_info.si_addr);
	} else {
		printf("snoop:%s:Pid %d PC 0x%p\n",
			s, psi->pr_pid, psi->pr_reg[CTX_EPC]);
	}
}

/*
 * stop over watch
 * Leaves process stopped at a FLTTRACE
 * Returns 0 - if stepped ok
 *	   1 - if hit another watchpoint at same PC
 * 	   2 - if got a KWATCH while stepping at same PC
 *	   3 - if hit another watchpoint after successfully stepping
 *		over one requested
 *	   4 - if process exitted
 */
int
stepoverwatch(char *s, int pfd, caddr_t addr, prstatus_t *opsi)
{
	prstatus_t psi;

	if (opsi->pr_why != PR_FAULTED) {
		fprintf(stderr, "snoop:ERROR:%s not FAULTED; why == %d\n",
			s, opsi->pr_why);
		errexit();
	}
	if (debug)
		prps(s, opsi);

	/* clear watchpoint */
	if (clrwatch(pfd, addr) < 0) {
		perror("snoop:ERROR:stepoverwatch:clrwatch");
		errexit();
	}

	if (opsi->pr_what == FLTKWATCH) {
		/* there is nothing to skip over since it occurred in
		 * a system call
		 */
		return(0);
	}
	/* step process */
	runit(pfd, PRSTEP|PRCFAULT);
	wstopit(pfd, &psi);
	if (debug)
		prps(s, &psi);
	/* if we really didn't advance, return status */
	if (opsi->pr_reg[CTX_EPC] == psi.pr_reg[CTX_EPC]) {
		if (psi.pr_why == PR_FAULTED && psi.pr_what == FLTWATCH)
			return(1);
		fprintf(stderr, "snoopw:ERROR: didn't advance from 0x%p why %s(%d) what %d\n",
			opsi->pr_reg[CTX_EPC],
			whytab[psi.pr_why], psi.pr_why,
			psi.pr_what);
		errexit();
		return(0);
	}
	/* advanced but hit another watch point - thats ok - we
	 * did step past it
	 */
	if (psi.pr_why == PR_FAULTED && psi.pr_what == FLTWATCH)
		return(3);
	if (psi.pr_why == PR_FAULTED && psi.pr_what == FLTKWATCH)
		return(2);
	if (psi.pr_why == PR_FAULTED && psi.pr_what == FLTTRACE)
		return(0);
	if (psi.pr_why == PR_SYSENTRY && psi.pr_what == EXIT)
		return(4);
	fprintf(stderr, "snoopw:ERROR:stepoverwatch:stop not FAULTED why %s(%d) what %d\n",
			whytab[psi.pr_why], psi.pr_why,
			psi.pr_what);
	errexit();
	return(0);
}

/*
 * retrieve current set of watchpoints
 */
prwatch_t *
getwatch(int pfd, int *nwp)
{
	int nw, maxw;
	prwatch_t *wp;

	if (ioctl(pfd, PIOCNWATCH, &maxw) < 0) {
		perror("snoop:ERROR:NWATCH");
		errexit();
	}

	wp = malloc(sizeof(*wp) * maxw);

	if ((nw = ioctl(pfd, PIOCGWATCH, (caddr_t)wp)) < 0) {
		perror("snoop:ERROR:GWATCH");
		errexit();
	}
	*nwp = nw;
	return(wp);
}

/*
 * retrieve current number of watchpoints
 */
int
getnwatch(int pfd)
{
	prwatch_t *w;
	int nwp;

	w = getwatch(pfd, &nwp);
	free(w);
	return(nwp);
}

prwatch_t *
findwatch(prwatch_t *wp, int nwp, caddr_t vaddr)
{
	int i;
	prwatch_t *lwp = NULL;

	for (i = 0; i < nwp; i++, wp++) {
		if (vaddr >= wp->pr_vaddr && vaddr < (wp->pr_vaddr + wp->pr_size)) {
			if (lwp) {
				printf("Overlapping watchpoints at 0x%p\n",
					vaddr);
			}
			lwp = wp;
		}
	}
	return(lwp);
}

/*
 * readmap - read in address space info
 */
prmap_t *
readmap(int pfd)
{
	int i, nw;
	prmap_t *wp;

	if (ioctl(pfd, PIOCNMAP, &nw) < 0) {
		perror("snoop:ERROR:NMAP");
		errexit();
		return(NULL);
	}

	wp = calloc(sizeof(*wp), (nw+1));

	if (ioctl(pfd, PIOCMAP, (caddr_t)wp) < 0) {
		perror("snoop:ERROR:MAP");
		errexit();
	}
	printf("snoopw:%d maps total\n", nw);
	if (verbose) {
		printf("snoopw:start addrs ");
		for (i = 0; i < nw; i++)
			printf(" 0x%p", wp[i].pr_vaddr);
	}
	return(wp);
}

int
main(int argc, char **argv)
{
	int c, i;
	int writeonly = 0;
	char pname[32];
	int pfd;
	int pid;
	prstatus_t psi;
	auto int inst;
	sigset_t sigs;
	fltset_t faults;
	sysset_t syscalls;
	prmap_t *map, *reg;
	int dura;
	ssize_t rv;
	prwatch_t *wp;	/* list of all watchpoints */
	prwatch_t *lwp, *lwp2;
	auto int nwp;
	int blocked = 0;
	char *sysname;
	int startonsproc = 0;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	pid = 0;
	dura = 30;
	while((c = getopt(argc, argv, "Dswadrvt:p:")) != EOF)
	switch (c) {
	case 'D':
		dataonly = 1;
		break;
	case 't':
		dura = atoi(optarg);
		break;
	case 'p':
		pid = atoi(optarg);
		break;
	case 'd':
		debug++;
		exitonerror = 0;
		break;
	case 'v':
		verbose++;
		break;
	case 'r':
		removeold++;
		break;
	case 's':
		startonsproc++;
		break;
	case 'w':
		writeonly++;
		break;
	case 'a':
		all++;
		break;
	default:
		fprintf(stderr, "Usage:snoopw [-p pid][-Dvwds][-t time][-r][-a][cmd args]\n");
		fprintf(stderr, "\t-p# - specify pid\n");
		fprintf(stderr, "\t-v - verbose\n");
		fprintf(stderr, "\t-d - debug\n");
		fprintf(stderr, "\t-a - watch it all\n");
		fprintf(stderr, "\t-w - write only watchpoints\n");
		fprintf(stderr, "\t-t - stop in n seconds (default 30)\n");
		fprintf(stderr, "\t-r - remove any old watchpoints\n");
		fprintf(stderr, "\t-s - set watchpoints after proc call sproc\n");
		fprintf(stderr, "\t-D - set watchpoints in data areas only\n");
		exit(-1);
	}

	if (pid == 0) {
		blocked++;
		/* exec command */
		if ((pid = fork()) < 0) {
			perror("snoopw:ERROR:fork");
			exit(-1);
		} else if (pid == 0) {
			blockproc(getpid());
			execvp(argv[optind], &argv[optind]);
			perror("snoopw:ERROR execvp");
			fprintf(stderr, "snoopw:ERROR: exec of %s failed\n",
				argv[optind]);
			exit(-1);
		}
	}

	/* open up /debug file */
	sprintf(pname, "/debug/%05d", pid);
	if ((pfd = open(pname, O_RDWR)) < 0) {
		fprintf(stderr, "snoop:ERROR:Cannot open %s\n", pname);
		perror("");
		exit(-1);
	}

	/* stop process */
	stopit(pfd);

	if (ioctl(pfd, PIOCSTATUS, &psi) != 0) {
		perror("snoop:ERROR:PSTATUS");
		errexit();
	}
	prps("1st stop", &psi);

	/* set up to get SIGTRAP & ALL faults (incl watchpoints) */
	premptyset(&sigs);
	praddset(&sigs, SIGTRAP);
	if (ioctl(pfd, PIOCSTRACE, &sigs) != 0) {
		perror("snoop:ERROR:PSTRACE");
		errexit();
	}
	prfillset(&faults);
	prdelset(&faults, FLTPAGE);
	if (ioctl(pfd, PIOCSFAULT, &faults) != 0) {
		perror("snoop:ERROR:PSFAULT");
		errexit();
	}
	i = PR_FORK;
	if (ioctl(pfd, PIOCRESET, &i) != 0) {
		perror("snoop:ERROR:PIOCRESET:FORK");
		errexit();
	}
	if (blocked) {
		/* set up exec/sproc exit */
		premptyset(&syscalls);
		if (startonsproc) {
			praddset(&syscalls, SPROC);
			praddset(&syscalls, SPROCSP);
			sysname = "sproc";
		} else {
			praddset(&syscalls, EXEC);
			praddset(&syscalls, EXECE);
			sysname = "exec";
		}
		if (ioctl(pfd, PIOCSEXIT, &syscalls) != 0) {
			perror("snoop:ERROR:PSEXIT");
			errexit();
		}
		premptyset(&syscalls);
		praddset(&syscalls, EXIT);
		if (ioctl(pfd, PIOCSENTRY, &syscalls) != 0) {
			perror("snoop:ERROR:PSENTRY");
			errexit();
		}
		/* run process */
		runit(pfd, 0);
		unblockproc(pid);

		/* wait for process to leave exec() */
		for (;;) {
			wstopit(pfd, &psi);
			if (psi.pr_why == PR_FAULTED) {
				runit(pfd, PRCFAULT);
				continue;
			} else if (psi.pr_why == PR_SYSEXIT) {
				if (verbose)
					printf("snoopw:child at end of %s\n",
						sysname);
				if (psi.pr_reg[CTX_A3]) {
					/* got an error */
					errno = (int)psi.pr_reg[CTX_V0];
					fprintf(stderr, "snoopw:child %s failed:%s\n",
						sysname, strerror(errno));
					runit(pfd, 0);
				} else
					break;
			} else {
				prps("wait for syscall", &psi);
			}
		}
	}

	/* read in mapping info */
	if ((map = readmap(pfd)) == NULL)
		exit(-1);

	if (!dataonly) {
	    /* force all text writable */
	    for (reg = map; reg->pr_mflags != 0; reg++) {
		if (reg->pr_mflags & MA_PHYS)
			continue;
		if (reg->pr_mflags & MA_EXEC) {
			if (lseek(pfd, (off_t)(reg->pr_vaddr), SEEK_SET) <= 0) {
				fprintf(stderr, "snoopw:ERROR:lseek 0x%p:%s\n",
					reg->pr_vaddr, strerror(errno));
				errexit();
			}
			if ((rv = read(pfd, &inst, 4)) != 4) {
				fprintf(stderr, "snoopw:ERROR:read @0x%p returned %ld:%s\n",
					reg->pr_vaddr, rv, strerror(errno));
				errexit();
			}
			if (lseek(pfd, (off_t)(reg->pr_vaddr), SEEK_SET) <= 0) {
				fprintf(stderr, "snoopw:ERROR:lseek 0x%p:%s\n",
					reg->pr_vaddr, strerror(errno));
				errexit();
			}
			if ((rv = write(pfd, &inst, 4)) != 4) {
				fprintf(stderr, "snoopw:ERROR:write rv %ld 0x%p:%s\n",
					rv, reg->pr_vaddr, strerror(errno));
				errexit();
			}
		}
	    }
	}
	sigset(SIGALRM, catch);

	/* 
	 * if removeold set then first make sure NO current watchpoints
	 */
	if (removeold) {
		wp = getwatch(pfd, &nwp);
		delallwatch(pfd, wp, nwp);
		wp = getwatch(pfd, &nwp);
		printf("watchpoints after delete all\n");
		prwatch(wp, nwp);
	}

	/*
	 * walk through maps and set some watchpoints in each
	 * Hard to claim these are really errors
	 */
	for (reg = map; reg->pr_mflags != 0; reg++) {
		if (reg->pr_mflags & MA_PHYS)
			continue;
		if (dataonly) {
			if (reg->pr_mflags & MA_STACK)
				continue;
			if (reg->pr_mflags & MA_EXEC) /* XXX not quite correct */
				continue;
		}
		if (writeonly && ((reg->pr_mflags & MA_WRITE) == 0))
			continue;
		if (all) {
			if (setwatch(pfd, reg->pr_vaddr, reg->pr_size, writeonly ? MA_WRITE : MA_READ|MA_WRITE|MA_EXEC) < 0) {
				fprintf(stderr, "snoopw:setwatch 0x%p\n",
					reg->pr_vaddr);
				perror("snoop:setwatch ALL");
			}
			continue;
		}
		if (setwatch(pfd, reg->pr_vaddr, 4, writeonly ? MA_WRITE : MA_READ) < 0) {
			fprintf(stderr, "snoopw:setwatch 0x%p\n",
				reg->pr_vaddr);
			perror("snoop:setwatch R");
		}
		if (setwatch(pfd, reg->pr_vaddr+reg->pr_size - 3, 1, MA_WRITE) < 0) {
			fprintf(stderr, "snoopw:setwatch 0x%p\n",
				reg->pr_vaddr);
			perror("snoop:setwatch W");
		}
		if (setwatch(pfd, reg->pr_vaddr+8, 24, writeonly ? MA_WRITE : MA_EXEC) < 0) {
			fprintf(stderr, "snoopw:setwatch 0x%p\n",
				reg->pr_vaddr);
			perror("snoop:setwatch X");
		}
		if (setwatch(pfd, reg->pr_vaddr + (reg->pr_size/2),
					reg->pr_size/16, MA_WRITE) < 0) {
			fprintf(stderr, "snoopw:setwatch 0x%p\n",
				reg->pr_vaddr);
			perror("snoop:setwatch W2");
		}
	}
	wp = getwatch(pfd, &nwp);
	prwatch(wp, nwp);

	/* run process */
	runit(pfd, 0);

	alarm(dura);
	for (;;) {
		int rv;
		/* wait for it to stop */
		sigrelse(SIGALRM);
		wstopit(pfd, &psi);
		if (leave) {
			printf("snoopw:times up!\n");
			break;
		}
		sighold(SIGALRM);
		if (psi.pr_why == PR_FAULTED && (psi.pr_what != FLTWATCH &&
				psi.pr_what != FLTKWATCH)) {
			runit(pfd, PRCFAULT);
			continue;
		} else if (psi.pr_why == PR_SYSENTRY) {
			printf("snoopw:child entered sysexit # %d\n", psi.pr_what);
			break;
		} else if (psi.pr_why == PR_SYSEXIT) {
			printf("snoopw:child leaving syscall # %d\n", psi.pr_what);
			runit(pfd, 0);
			continue;
		}
		lwp2 = NULL;
		lwp = findwatch(wp, nwp, psi.pr_info.si_addr); 
		if (verbose) {
			printf("Hit at 0x%p %s%s%s(0x%x) from PC 0x%p",
				psi.pr_info.si_addr,
				psi.pr_info.si_code & MA_READ ? "READ " : "",
				psi.pr_info.si_code & MA_WRITE ? "WRITE " : "",
				psi.pr_info.si_code & MA_EXEC ? "EXEC " : "",
				psi.pr_info.si_code,
				psi.pr_reg[CTX_EPC]);
			if (psi.pr_info.si_code & MA_WRITE) {
				unsigned int a;
				unsigned short b;
				unsigned char c;
				ssize_t rcnt;
				if (lseek(pfd, (off_t)(psi.pr_info.si_addr), SEEK_SET) <= 0) {
					fprintf(stderr, "snoopw:ERROR:lseek at hit 0x%p:%s\n",
						psi.pr_info.si_addr, strerror(errno));
					errexit();
				}
				printf(" new value ");
				if ((off_t)psi.pr_info.si_addr & 0x1) {
					rcnt = read(pfd, &c, 1);
					printf("0x%x", c);
				} else if ((off_t)psi.pr_info.si_addr & 0x2) {
					rcnt = read(pfd, &b, 2);
					printf("0x%x", b);
				} else {
					rcnt = read(pfd, &a, 4);
					printf("0x%x", a);
				}
				if (rcnt <= 0) {
					fprintf(stderr, "snoopw:ERROR:read at hit\n");
					errexit();
				}
			}
			printf("\n");
		}
		if (lwp == NULL) {
			fprintf(stderr, "snoopw:ERROR:hit but no watchpoint!\n");
			prps("bad hit?", &psi);
			prwatch(wp, nwp);
			break;
		}
		rv = stepoverwatch("", pfd, lwp->pr_vaddr, &psi);
		if (rv == 4) {
			printf("snoopw:child exitted\n");
			break;
		}
		if (rv == 1 || rv == 2) {
			/* hit another watchpoint */
			if (ioctl(pfd, PIOCSTATUS, &psi) != 0) {
				perror("snoop:ERROR:PSTATUS");
				errexit();
			}
			lwp2 = findwatch(wp, nwp, psi.pr_info.si_addr); 
			if (verbose) {
				if (rv == 1)
					printf("Hit-again at 0x%x\n", psi.pr_info.si_addr);
				else if (rv == 2)
					printf("Hit-insystem at 0x%x\n", psi.pr_info.si_addr);
			}
			if (lwp2 == NULL) {
				fprintf(stderr, "snoopw:ERROR:hit2 but no watchpoint!\n");
				prps("bad hit2?", &psi);
				prwatch(wp, nwp);
				break;
			}
			rv = stepoverwatch("", pfd, lwp2->pr_vaddr, &psi);
			if (rv == 4) {
				printf("snoopw:child exitted\n");
				break;
			}
			if (rv ==1 || rv == 2) {
				/* hit another watchpoint! */
				if (ioctl(pfd, PIOCSTATUS, &psi) != 0) {
					perror("snoop:ERROR:PSTATUS");
					errexit();
				}
				prps("stepover twice?", &psi);
				fprintf(stderr, "1st @ 0x%p 2nd @ 0x%p\n",
					lwp->pr_vaddr, lwp2->pr_vaddr);
				wp = getwatch(pfd, &nwp);
				prwatch(wp, nwp);
				break;
			}
		}
		if (setwatch(pfd, lwp->pr_vaddr, lwp->pr_size,
						lwp->pr_wflags) != 0) {
			perror("snoopw:ERROR:resetwatch");
			errexit();
		}
		if (lwp2) {
			if (setwatch(pfd, lwp2->pr_vaddr, lwp2->pr_size,
							lwp2->pr_wflags) != 0) {
				perror("snoopw:ERROR:resetwatch2");
				errexit();
			}
		}
		if (getnwatch(pfd) != nwp) {
			fprintf(stderr, "Lost a watchpoint?\n");
		}
		if (rv != 3) {
			/* rv == 3 implies stepped but already got another one
			 */
			runit(pfd, PRCFAULT);
		}
	}
	/* stop process */
	stopit(pfd);
	wp = getwatch(pfd, &nwp);
	delallwatch(pfd, wp, nwp);
	wp = getwatch(pfd, &nwp);
	printf("watchpoints after delete all\n");
	prwatch(wp, nwp);
	if (psi.pr_why == PR_FAULTED) {
		runit(pfd, PRCFAULT);
	} else {
		runit(pfd, 0);
	}
	close(pfd);	/* will run child */

	return 0;
}

void
errexit(void)
{
	if (exitonerror)
		abort();
}

void
delallwatch(int pfd, prwatch_t *wp, int nwp)
{
	int i;

	for (i = 0; i < nwp; i++, wp++) {
		if (clrwatch(pfd, wp->pr_vaddr) != 0) {
			perror("snoopw:ERROR:deleteallwatch");
			fprintf(stderr, "snoopw:ERROR:delete at 0x%p\n", wp->pr_vaddr);
		}
	}
}

void
prwatch(prwatch_t *wp, int nwp)
{
	int i;

	for (i = 0; i < nwp; i++, wp++) {
		printf("snoop:Watchpoint at 0x%p len 0x%lx mode 0x%lx\n",
			wp->pr_vaddr,
			wp->pr_size,
			wp->pr_wflags);
	}
	printf("snoopw:%d watchpoints total\n", nwp);
}

void
catch()
{
	leave = 1;
}
