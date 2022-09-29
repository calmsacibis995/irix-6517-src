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

#ident	"$Revision: 1.7 $ $Author: sfc $"
/**************************************************************************

  msync0.c, used to test functionality of msync(), had following testing cases.

  1. check msync() with 0 flag synchronize memory with mapped file.
  2. check msync() with MS_INVALIDATE get memory to be invalidated.
  3. check msync() with MS_ASYNC synchronize memory with mapped file after
     return.

  note : Since this test depends the timing of processes and I/O, it might
         get different results for future kernels.

***************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include "stress.h"

#define DIV 32
#define BS (DIV<<5)
#define NB 20

caddr_t	paddr;
char	cstring[] = ":<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char	buf[BS];
char	ops[8] = "ABCDEFG";
int	nops;
int	verbose;
pid_t	cpid, ppid;
int	fd;
char	*filename;
char	*Cmd;
void	child(void *), bail(void);

int
main(int argc, char **argv)
{
	int psize, i;
	caddr_t p;
	int c;
	int errflg = 0;

	Cmd = errinit(argv[0]);
	filename = tempnam(NULL, "Tmags");
	while ((c = getopt(argc, argv, "vf:")) != -1)
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'f':
			filename = optarg;
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, "Usage: agsync [-v][-f filename]\n");
		exit(1);
	}
	if ((fd = open(filename, O_CREAT | O_RDWR |O_TRUNC, 0666)) < 0)  {
		errprintf(ERR_ERRNO_EXIT, "open of %s failed", filename);
		/* NOTREACHED */
	}

	p = mmap(0, BS*NB, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_AUTOGROW, fd, 0);
	if (p == (caddr_t) -1L) {
		errprintf(ERR_ERRNO_RET, "mmap autogrow failed");
		bail();
	}

	for (i = 0; i < BS*NB; i++)
		p[i] = cstring[i % DIV];

	if (munmap(p, BS*NB) < 0)  {
		errprintf(ERR_ERRNO_RET, "munmap failed");
		bail();
	}

	paddr = mmap(0, BS*NB, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (paddr == (caddr_t) -1L) {
		errprintf(ERR_ERRNO_RET, "mmap failed");
		bail();
	}

	if (lseek(fd, BS*(NB-1), 0) < 0) {
		errprintf(ERR_ERRNO_RET, "lseek");
		bail();
	}
	if (read(fd, buf, BS) != BS) {
		errprintf(ERR_ERRNO_RET, "read1 tempfile failed");
		bail();
	}
	for (i = 0; i < BS; i++)
		if (buf[i] != cstring[i%DIV]) {
			errprintf(ERR_RET, "tempfile is not synchronized\n");
			bail();
		}

	ppid = getpid();
	parentexinit(0);
	if ((cpid = sproc(child, PR_SALL, 0)) < 0) {
		if (errno != EAGAIN) {
			errprintf(ERR_ERRNO_RET, "sproc failed i:%d", i);
			bail();
		} else {
			fprintf(stderr, "%s:can't sproc:%s\n",
				Cmd, strerror(errno));
			unlink(filename);
			exit(1);
		}
	}

	/*
	 * NOPS == 1
	 */
	nops++;
	for (i = 0; i < BS*NB; i++)
		paddr[i] = ops[nops];

	if (msync(paddr, BS*NB, MS_INVALIDATE) < 0) {
		errprintf(ERR_ERRNO_RET, "msync MS_INVALIDATE failed");
		bail();
	}

	/*
	 * NOPS == 2
	 */
	nops++;
	if (lseek(fd, 0, 0) < 0) {
		errprintf(ERR_ERRNO_RET, "parent lseek 0");
		bail();
	}
	for (i = 0; i < BS; i++)
		buf[i] = ops[nops];

	for (i = 0; i < NB; i++)
		if (write(fd, buf, BS) != BS) {
			errprintf(ERR_ERRNO_RET, "write tempfile failed");
			bail();
		}

	if (unblockproc(cpid)) {
		errprintf(ERR_ERRNO_RET, "unblock child");
		bail();
	}
	if (blockproc(ppid)) {
		errprintf(ERR_ERRNO_RET, "block self (parent)");
		bail();
	}

	/*
	 * NOPS == 3
	 */
	/* check that after msync(MS_INVALIDATE) read gets mmapped data */
	if (lseek(fd, 0, 0) < 0) {
		errprintf(ERR_ERRNO_RET, "parent lseek 1");
		bail();
	}

	if (read(fd, buf, BS) != BS) {
		errprintf(ERR_ERRNO_RET, "read2 tempfile failed");
		bail();
	}
	for (i = 0; i < BS; i++)
		if (buf[i] != ops[nops]) {
			errprintf(ERR_RET, "did not read mmapped data after msync(INVAL)");
			break;
		}

	if (i != BS) {
		errprintf(ERR_RET, "buf[%d] is %x, ops[%d] is %x",
			i, buf[i], nops, ops[nops]);
		bail();
	}

	/*
	 * NOPS == 4
	 */
	nops++;
	for (i = 0; i < BS; i++)
		paddr[i] = ops[nops];
	if (msync(paddr, BS*NB, 0) < 0) {
		errprintf(ERR_ERRNO_RET, "msync failed");
		bail();
	}

	if (unblockproc(cpid)) {
		errprintf(ERR_ERRNO_RET, "parent unblock child");
		bail();
	}
	if (blockproc(ppid)) {
		errprintf(ERR_ERRNO_RET, "parent block self");
		bail();
	}

	for (i = 0; i < BS; i++)
		buf[i] = ops[0];

	if (verbose)
		printf("agsync:testing msync(MS_ASYNC) ");
	for (;;) {
		lseek(fd, BS*(NB-1), 0);
		if (read(fd, buf, BS) != BS) {
			errprintf(ERR_ERRNO_RET, "read tempfile failed");
			bail();
		}
		for (i = 0; i < BS; i++)
			if (buf[i] != ops[nops]) {
				if (verbose)
					printf("...");
			}
		if (i == BS) {
			if (verbose)
				printf("%d : succeed\n", nops);
			break;
		}
	}

	if (unblockproc(cpid)) {
		errprintf(ERR_ERRNO_RET, "parent unblock child");
		bail();
	}

	if (munmap(paddr, BS*NB) < 0)  {
		errprintf(ERR_ERRNO_RET, "munmap failed");
		bail();
	}

	printf("%s:%d:PASSED\n", Cmd, getpid());

	unlink(filename);
	exit(0);
}

void
child(void *arg)
{
	int i;

	slaveexinit();
	/*
	 * Since parent probably hasn't executed
	 * cpid = sproc(....) yet, can't access
	 * cpid and expect to find the actual id.
	 */
	if (blockproc(getpid())) {
		errprintf(ERR_ERRNO_RET, "block self (child)");
		bail();
	}

	/*
	 * NOPS == 2
	 */

	/* check results of msync with MS_INVALIDATE */
	for (i = 0; i < BS*NB; i++)
		if (paddr[i] != ops[nops]) {
			errprintf(ERR_RET, "memory is not invalidated correctly");
			break;
		}

	if (i == BS*NB) {
		if (verbose)
			printf("agsync:%d : succeed\n", nops);
	} else {
		errprintf(ERR_RET, "ops[%d] is %x, paddr[%d] is %x\n",
			nops, ops[nops], i, paddr[i]);
		bail();
	}

	/*
	 * NOPS == 3
	 */
	nops++;
	for (i = 0; i < BS*NB; i++)
		paddr[i] = ops[nops];
	if (msync(paddr, BS*NB, MS_INVALIDATE) < 0) {
		errprintf(ERR_ERRNO_RET, "msync MS_INVALIDATE failed");
		bail();
	}


	if (unblockproc(ppid)) {
		errprintf(ERR_ERRNO_RET, "unblock parent (child)");
		bail();
	}
	if (blockproc(getpid())) {
		errprintf(ERR_ERRNO_RET, "block self (child)");
		bail();
	}

	/*
	 * NOPS == 4
	 */
	/* check results of msync without MS_INVALIDATE */
	lseek(fd, 0, 0);
	if (read(fd, buf, BS) != BS) {
		errprintf(ERR_ERRNO_RET, "slave read2 tempfile failed");
		bail();
	}
	for (i = 0; i < BS; i++)
		if (buf[i] != ops[nops]) {
			errprintf(ERR_RET, "read incorrect data after msync(..0)");
			break;
		}

	if (i == BS) {
		if (verbose)
			printf("agsync:%d : succeed\n", nops);
	} else {
		errprintf(ERR_RET, "buf[%d] is %x, ops[%d] is %x",
			i, buf[i], nops, ops[nops]);
		bail();
	}

	/*
	 * NOPS == 5
	 */
	nops++;
	for (i = 0; i < BS*NB; i++)
		paddr[i] = ops[nops];
	if (msync(paddr, BS*NB, MS_ASYNC) < 0) {
		errprintf(ERR_ERRNO_RET, "msync MS_INVALIDATE failed");
		bail();
	}

	if (unblockproc(ppid)) {
		errprintf(ERR_ERRNO_RET, "unblock parent (child)");
		bail();
	}
	if (blockproc(getpid())) {
		errprintf(ERR_ERRNO_RET, "block self (child)");
		bail();
	}
	exit(0);
}

void
bail(void)
{
	unlink(filename);
	abort();
}
