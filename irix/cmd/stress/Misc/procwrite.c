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
#ident	"$Revision: 1.1 $ $Author: kanoj $"

#include "sys/types.h"
#include "signal.h"
#include "errno.h"
#include "unistd.h"
#include "getopt.h"
#include "sys/ucontext.h"
#include "sys/time.h"
#include "sys/siginfo.h"
#include "sys/procfs.h"
#include "sys/ipc.h"
#include "sys/shm.h"
#include "stdio.h"
#include "math.h"
#include "fcntl.h"
#include "malloc.h"
#include "sys/times.h"
#include "limits.h"
#include <sys/sysmp.h>
#include <sys/mman.h>


int verbose;
int spin;

typedef volatile struct sstate {
	int go;
	char *addr;
	unsigned char value;
} sstate_t;
sstate_t *shaddr;

void doscan(void);
int pin = 0;
char *scanar;
int arsize = 5*1024*1024;

main(argc, argv)
int argc;
char **argv;
{
	int c, i, j;
	pid_t pid;
	char pname[40];
	int pfd, numcpus, cpu1, cpu2;
	int nloops = 10;
	int shmid;

	while((c = getopt(argc, argv, "psvm:l:")) != EOF)
		switch (c) {
		case 'p':
			pin = 1;
			break;
		case 's':
			spin = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'm':
			arsize = atoi(optarg);
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
		}

	scanar = malloc(arsize);
	bzero(scanar, arsize);

	/* set up some shd mem */
	if ((shmid = shmget(IPC_PRIVATE, 0x1000, IPC_CREAT|0777)) < 0) {
		perror("procwrite:ERROR:shmget");
		exit(-1);
	}
	if ((shaddr = shmat(shmid, 0, 0)) == (sstate_t *)-1) {
		perror("procwrite:ERROR:shmat");
		exit(-1);
	}
	if (shmctl(shmid, IPC_RMID) != 0) {
		perror("procwrite:ERROR:shmctl");
		exit(-1);
	}

	shaddr->go = 0;
	numcpus = sysmp(MP_NAPROCS);
	switch(numcpus) {
		case 1 :
			cpu1 = cpu2 = 0;
			break;
		case 2 :
			cpu1 = 0; cpu2 = 1;
			break;
		default :
			cpu1 = 0; cpu2 = 2;
			break;
	}
	if ((pid = fork()) < 0) {
		perror("procwrite:ERROR:fork");
		exit(-1);
	} else if (pid == 0) {
		sysmp(MP_MUSTRUN, cpu1);
		doscan();
		exit(0);
	}

	sysmp(MP_MUSTRUN, cpu2);
	if (verbose)
		printf("procwrite: subject pid %d addr 0x%x len 0x%x loops %d pin %d\n", pid, scanar, arsize, nloops, pin);

	/* open up /debug file */
	sprintf(pname, "/proc/%d", pid);
	if ((pfd = open(pname, O_RDWR)) < 0) {
		fprintf(stderr, "procwrite:ERROR:Cannot open %s\n", pname);
		perror("");
		exit(-1);
	}

	/* wait for child to start */
	while (shaddr->go == 0);
	for (i = 0; i < nloops; i++) {
		shaddr->addr = (random() % arsize) + scanar;
		shaddr->value = random() & 0xff;
		shaddr->go = 2;		/* indicate addr/value set */
		if (lseek(pfd, (off_t)shaddr->addr, SEEK_SET) != (off_t)shaddr->addr) {
			fprintf(stderr, "procwrite: seek to 0x%x failed:%s\n",
						shaddr->addr, strerror(errno));
			exit(1);
		}
		if (write(pfd, (char *)&shaddr->value, 1) != 1) {
			fprintf(stderr, "procwrite: write at 0x%x failed:%s\n",
						shaddr->addr, strerror(errno));
			exit(1);
		}

		/* say 'go' */
		shaddr->go = 3;
		while (shaddr->go == 3);
	}
	kill(pid, SIGKILL);
	exit(0);
}

void
doscan(void)
{
	int i;
	char *p;
	unsigned char tvalue;
	if (pin) mlock(scanar, arsize);
	shaddr->go = 1; 	/* indicate child started */
	for (;;) {
		/* wait for parent to tell us where it modified our page */
		while (shaddr->go == 1);
		/* start accessing to force page into tlb */
		while (shaddr->go == 2)
			tvalue = *shaddr->addr;
		/* shaddr->go == 3 ie parent has completed write */
		tvalue = *shaddr->addr;
		while (tvalue != (shaddr->value)) {
			fprintf(stderr, "procwrite:value at 0x%x is 0x%x not 0x%x\n",
					shaddr->addr, tvalue, shaddr->value);
			fprintf(stderr, "procwrite: reread 0x%x\n",
					tvalue = *(shaddr->addr));
			abort(); 	/* just so that stress tester sees */
			if (spin)
				;
			else
				break;
		}
		shaddr->go = 1;
	}
}
