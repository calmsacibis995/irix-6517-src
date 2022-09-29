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

#ident	"$Revision: 1.9 $ $Author: jwag $"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "getopt.h"
#include "fcntl.h"
#include "signal.h"
#include "malloc.h"
#include "errno.h"
#include "sys/wait.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "sys/mman.h"
#include "stress.h"

void slave2(void *);
void slave(void *);
char *Cmd;
char *pmaddr;

#define NBYTES	(64*1024)

caddr_t
addrattach(pid_t pid, void *addr)
{
	caddr_t vaddr;

	if ((vaddr = (caddr_t)prctl(PR_ATTACHADDR, pid, addr)) == (caddr_t) -1L)
		return(NULL);
	return(vaddr);

}

/* ARGSUSED */
void
rtcatch(int sig, siginfo_t *si, void *unused)
{
	if (sig != SIGRTMIN || si->si_code != SI_QUEUE) {
		errprintf(ERR_EXIT, "invalid signal");
		/* NOTREACHED */
	}

	pmaddr = si->si_value.sival_ptr;
}

int buf[100];
pid_t ppid;
char *maddr;

int
main(int argc, char **argv)
{
	caddr_t newaddr, vaddr, caddr;
	pid_t cpid, spid;
	int c, fd, i;
	int nbytes = NBYTES;
	int private = 0;
	struct sigaction sa;

	Cmd = errinit(argv[0]);
	while((c = getopt(argc, argv, "n:p")) != EOF)
	switch (c) {
	case 'n':
		nbytes = atoi(optarg);
		break;
	case 'p':
		private = 1;
		break;
	default:
		fprintf(stderr, "Usage:addrattach [-n nbytes][-p]\n");
		exit(-1);
	}
	ppid = getpid();

	sa.sa_handler = rtcatch;
	bzero(&sa.sa_mask, sizeof(sa.sa_mask));
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
		errprintf(ERR_ERRNO_EXIT, "sigaction");
		/* NOTREACHED */
	}

	prctl(PR_SETEXITSIG, SIGTERM);
	if ((spid = sproc(slave, private ? 0 : PR_SALL, nbytes)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "sproc");
		/* NOTREACHED */
	}
	blockproc(ppid);

	/* attach in a portion on the slaves address space (it and our
	 * data space
	 */
	if ((vaddr = addrattach(spid, buf)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "addrattach");
		/* NOTREACHED */
	}

	/* slave has initialized maddr, sent us a signal and we have
	 * initialized pmaddr
	 * check memory via back door
	 */
	caddr = (pmaddr - (caddr_t)buf) + vaddr;
	printf("parent checking starting at 0x%x (vaddr = 0x%x)\n",
		caddr, vaddr);

	for (i = 0; i < nbytes; i++)
		if (*(pmaddr + i) != *(caddr + i)) {
			errprintf(ERR_EXIT, "mismatch at 0x%x", caddr+i);
			/* NOTREACHED */
		}

	if ((cpid = fork()) < 0) {
		errprintf(ERR_ERRNO_EXIT, "fork");
		/* NOTREACHED */
	} else if (cpid != 0) {
		/* parent */
		blockproc(ppid);
		/* shouldn't have changed! */
		printf("parent checking starting at 0x%x (vaddr = 0x%x)\n",
			caddr, vaddr);

		for (i = 0; i < nbytes; i++)
			if (*(pmaddr + i) != *(caddr + i)) {
				errprintf(ERR_EXIT, "mismatch at 0x%x", caddr+i);
				/* NOTREACHED */
			}
	} else {
		/* child */
		for (i = 0; i < nbytes; i++)
			*(pmaddr + i) = (~i) & 0xff;
		
		printf("child write %d bytes starting at 0x%x\n", nbytes, pmaddr);
		unblockproc(ppid);
		exit(0);
	}

	/*kill(cpid, SIGKILL);*/
	prctl(PR_SETEXITSIG, 0);
	kill(spid, SIGKILL);
	while (wait(0) >= 0 || errno == EINTR)
		;

	/* now set up mmap region to see if sproc child gets own copy */
	if ((fd = open("/dev/zero", O_RDWR)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "open of dev/zero");
		/* NOTREACHED */
	}

	if ((newaddr = mmap(0, nbytes, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_LOCAL, fd, 0)) == (caddr_t) -1L) {
		errprintf(ERR_ERRNO_EXIT, "mmap");
		/* NOTREACHED */
	}
	maddr = newaddr;	/* set for sproc child */

	/* fill in parent copy */
	printf("parent filling in at 0x%x\n", newaddr);
	for (i = 0; i < nbytes; i++)
		*(newaddr + i) = (~i) & 0xff;

	/* spawn sproc child, should get own copy */
	prctl(PR_SETEXITSIG, SIGTERM);
	if ((spid = sproc(slave2, PR_SALL, nbytes)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "sproc");
		/* NOTREACHED */
	}

	/* wait for it to init its area */
	blockproc(ppid);

	/* our area shouldn't have changed */
	printf("Checking parents area not disturbed by child\n");
	for (i = 0; i < nbytes; i++)
		if (*(newaddr + i) != ((~i) & 0xff)) {
			errprintf(ERR_EXIT, "mismatch at 0x%x", newaddr+i);
			/* NOTREACHED */
		}

	/* remap childs area */
	if ((vaddr = addrattach(spid, newaddr)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "addrattach");
		/* NOTREACHED */
	}

	/* check it to match what child put in */
	printf("Checking childs area as remapped into parent @ 0x%x\n",
		vaddr);
	for (i = 0; i < nbytes; i++)
		if (*(vaddr + i) != ((i) & 0xff)) {
			errprintf(ERR_EXIT, "mismatch at 0x%x", vaddr+i);
			/* NOTREACHED */
		}
	prctl(PR_SETEXITSIG, 0);
	kill(spid, SIGKILL);
	while (wait(0) >= 0 || errno == EINTR)
		;
	return 0;
}

void
slave(void *a)
{
	int i;
	int n = (int)(ptrdiff_t)a;
	sigval_t sg;

	/* malloc n bytes, and write pattern */
	if ((maddr = malloc(n)) == NULL) {
		perror("malloc");
		exit(-1);
	}

	for (i = 0; i < n; i++)
		*(maddr + i) = i & 0xff;
	
	printf("slave write %d bytes starting at 0x%x\n", n, maddr);
	/* send maddr to parent */
	sg.sival_ptr = maddr;
	sigqueue(ppid, SIGRTMIN, sg);
	unblockproc(ppid);
	for (;;)
		pause();
}

void
slave2(void *a)
{
	int i;
	int n = (int)(ptrdiff_t)a;

	for (i = 0; i < n; i++)
		*(maddr + i) = i & 0xff;
	
	printf("slave1 write %d bytes starting at 0x%x\n", n, maddr);
	unblockproc(ppid);
	for (;;)
		pause();
}
