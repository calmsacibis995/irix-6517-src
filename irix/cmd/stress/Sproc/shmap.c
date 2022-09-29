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

#ident	"$Revision: 1.9 $ $Author: cp $"

#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "sys/types.h"
#include "sys/mman.h"
#include "sys/prctl.h"
#include "stdio.h"
#include "fcntl.h"
#include "signal.h"
#include "ulocks.h"

/*
 * try a sproc and open a mapped file
 */
char sbuf[1024];	/* starting contents of file */
pid_t ppid;
caddr_t vaddr;
void fault(int sig, int code, struct sigcontext *scp);

int
main(int argc, char **argv)
{
	extern void slave();
	int pid, i, j, fd;
	int nloop;

	if (argc < 2) {
		fprintf(stderr, "Usage:%s nloop\n", argv[0]);
		exit(-1);
	}
	nloop = atoi(argv[1]);
	ppid = get_pid();

	/* open a file and write a pattern into it */
	if ((fd = open("/tmp/shmap", O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0) {
		perror("shmap:cannot open /tmp/shmap");
		exit(-1);
	}

	/* fill sbuf */
	for (i = 0; i < 1024; i++)
		sbuf[i] = "abcdefghijklmnopqrstuvwxyz0123456789"[i % 36];

	/* write some stuff into it */
	if (write(fd, sbuf, 1024) != 1024) {
		perror("shmap:write failed");
		exit(-1);
	}

	/* now sproc */
	if ((pid = sproc(slave, PR_SADDR|PR_SFDS)) < 0) {
		perror("shmap:sproc failed");
		exit(-1);
	}

	/* now map in file */
	if ((long)(vaddr = mmap(0, 1024, PROT_READ|PROT_WRITE, MAP_SHARED,
							fd, 0)) < 0L) {
		perror("shmap:mmap failed");
		kill(pid, SIGKILL);
		exit(-1);
	}
	/* unlock slave */
	unblockproc(pid);

	/* wait for slave */
	blockproc(get_pid());

	/* now detach and re-attach */
	for (i = 0; i < nloop; i++) {
		/* unmap file */
		if (munmap(vaddr, 1024) < 0) {
			perror("shmap:munmap failed");
			kill(pid, SIGKILL);
			exit(-1);
		}

		/*sginap(10);*/
		for (j = 0; j < 10000; j++) {
			volatile int k;
			k = j * 10;
		}
		while (!prctl(PR_ISBLOCKED, pid)) {
			char bb[128];
			sprintf(bb, "shmap:Waiting for slave to fault\n");
			write(1, bb, strlen(bb));
			sginap(10);
		}

		/* now map in file */
		if ((long)(vaddr = mmap(0, 1024, PROT_READ|PROT_WRITE, MAP_SHARED,
								fd, 0)) < 0L) {
			perror("shmap:mmap failed");
			kill(pid, SIGKILL);
			exit(-1);
		}
		unblockproc(pid);
		sginap(40);
	}
	kill(pid, SIGKILL);
	return 0;
}

/* ARGSUSED */
void
fault(int sig, int code, struct sigcontext *scp)
{
	char	str[128];
	/*
	 * Can't use printf here because this would risk libc taking
	 * a usema for thread locking and never being released if our
	 * parent kills us. The parent would then hang.
	 */
	sprintf(str, "shmap:pid %d fault @ %x\n", get_pid(), scp->sc_badvaddr);
	write(1, str, strlen(str));
	signal(SIGSEGV, fault);
	signal(SIGBUS, fault);
	blockproc(get_pid());
}

void
slave()
{
	register caddr_t c;

	/* wait for mmap */
	blockproc(get_pid());

	signal(SIGSEGV, fault);
	signal(SIGBUS, fault);

	printf("shmap:slave pid %d accessing mapped file @ %x\n",
					get_pid(), vaddr);
	for (;;) {
		/* now try to read */
		for (c = vaddr; c < vaddr + 1024; c++) {
			if (*c != sbuf[c - vaddr]) {
				fprintf(stderr, "shmap:mismatch:c:%x *c %c sbuf %c\n",
					c, *c, sbuf[c - vaddr]);
			}
		}

		/* unblock parent (really just 1st time) */
		unblockproc(ppid);
		/*sginap(1);*/
	}
}
