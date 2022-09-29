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

#ident	"$Revision: 1.7 $ $Author: jwag $"

#include "sys/types.h"
#include "limits.h"
#include "stdio.h"
#include "fcntl.h"
#include "sys/times.h"
#include "errno.h"
#include "wait.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/prctl.h"
#include "ulocks.h"
#include "malloc.h"
#include "string.h"

/*
 * shdma -test sproc simulataneous physio
 */
char *dev = "/dev/swap";
int fd;			/* file to read from */
int szread;
int nloops;		/* # times to read */
char *Cmd;

int
main(int argc, char **argv)
{
	extern void slave(void *);
	struct tms tm;
	register int i, j;
	char *f;
	char *Gbuf;
	char *Gbuf2;
	clock_t start, ti;
	int nprocs;		/* total procs created */
	int nprocspl;		/* # procs per loop */
	int doloop;		/* # procs this loop */
	pid_t pid;
	auto int status;

	if (argc < 3) {
		fprintf(stderr, "Usage:%s nprocs nloops [szread]\n", argv[0]);
		exit(-1);
	}
	if (argc == 4) {
		szread = atoi(argv[3]);
		if (argv[3][strlen(argv[3]) - 1] == 'k')
			szread *= 1024;
	} else
		szread = 4096;
	nloops = atoi(argv[2]);
	Cmd = argv[0];

	if ((Gbuf = malloc(szread)) == NULL) {
		perror("shdma:no mem for Gbuf");
		exit(-2);
	}
	if ((Gbuf2 = malloc(szread)) == NULL) {
		perror("shdma:no mem for Gbuf2");
		exit(-2);
	}
	nprocs = atoi(argv[1]);
	if ((fd = open(dev, O_RDONLY)) != -1) {
		f = dev;
	}
	else {
		perror("shdma:no raw file");
		exit(-1);
	}
	printf("%s:Creating %d processes each reading %d bytes from %s\n",
		Cmd, nprocs, szread, f);
	nprocspl = 20;

	start = times(&tm);
	usconfig(CONF_INITUSERS, nprocspl+1);

	for (j = nprocs; j > 0; j -= nprocspl) {
		if (lseek(fd, 0, 0) < 0) {
			perror("shdma:lseek failed");
			exit(-3);
		}
		if (j > nprocspl)
			doloop = nprocspl;
		else
			doloop = j;

		for (i = 0; i < doloop; i++) {
			if ((pid = sproc(slave, PR_SALL, i & 0x1 ? Gbuf : Gbuf2)) < 0) {
				perror("shdma:sproc failed");
			}
		}
		/* wait for all processes to finish */
		for (;;) {
			pid = wait(&status);
			if (pid < 0 && oserror() == ECHILD)
				break;
			else if ((pid >= 0) && (status & 0xff))
				fprintf(stderr, "%s:proc %d died of signal %d\n",
					Cmd, pid, status & ~0200);
		}
	}

	ti = times(&tm) - start;
	printf("%s:start:%x time:%x\n", Cmd, start, times(&tm));
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("%s:time for %d sprocs %d mS or %d uS per process\n",
		Cmd, nprocs, ti, (ti*1000)/nprocs);
	printf("%s:master exitting\n", Cmd);
	return 0;
}

void
slave(void *gbuf)
{
	char buf[100];
	int i;
	ssize_t rv;

	for (i = 0; i < nloops; i++) {
		if ((rv = read(fd, gbuf, szread)) != szread) {
			if (rv == -1)
				sprintf(buf, "%s:read error:%s\n", Cmd, sys_errlist[oserror()]);
			else
				sprintf(buf, "%s:read got:%d wanted:%d\n", Cmd, rv, szread);
			write(1, buf, strlen(buf));
		}
	}
	_exit(0);
}
