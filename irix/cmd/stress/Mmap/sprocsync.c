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

#ident	"$Revision: 1.4 $ $Author: sfc $"
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
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>

#define DIV 32
#define BS (DIV<<5)
#define NB 20

caddr_t	paddr;
extern int	edata;
char	cstring[] = ":<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char	buf[BS];
char	ops[8] = "ABCDEFG";
int	nops;
int	cpid, ppid, fd;
char	*filename;
void	child();

main()
{
    int psize, i;

    filename = tempnam(NULL, "ms0f");
    if ((fd = open(filename, O_CREAT | O_RDWR, 0666)) < 0) 
        perror("open ms0file tempfile failed");
    
    unlink(filename);
    for ( i = 0; i < BS; i++)
	buf[i] = ops[nops];
    for (i = 0; i < NB; i++)
        if (write(fd, buf, BS) != BS) {
	    perror("write ms0file tempfile failed");
	    exit(-1);
	}

    if ((long)(paddr = mmap(0, BS*NB, PROT_WRITE, MAP_SHARED, fd, 0)) < 0L)  {
	perror("mmap PROT_WRITE failed");
	exit(1);
    }

    for (i = 0; i < BS*NB; i++)
	paddr[i] = cstring[i % DIV];

    /* read from file */
    lseek(fd, -BS, 2);
    if (read(fd, buf, BS) != BS) {
	perror("read ms0file tempfile failed");
	errno = 0;
    }

    for (i = 0; i < BS; i++)
	if (buf[i] != ops[nops]) {
	    printf("ms0file tempfile already synchronized to memory\n");
	    break;
	}

    if (msync(paddr, BS*NB, 0) < 0)
	perror("msync failed");
    else {
        if (lseek(fd, -BS, 2) < 0) {
	    perror("lseek");
	    errno = 0;
	}
        if (read(fd, buf, BS) != BS)
	    perror("read1 ms0file tempfile failed");
	for (i = 0; i < BS; i++)
	    if (buf[i] != cstring[i%DIV]) {
		printf("ms0file tempfile is not synchronized correctly\n");
		break;
	    }
	printf("%d : succeed\n", nops);
    }

    /* test msync with MS_INVALIDATE */
    ppid = getpid();
    errno = 0;
    if ((cpid = sproc(child, PR_SALL, 0)) < 0) {
	perror("sproc failed");
	exit(-1);
    }

    /*
     * NOPS == 1
     */
    nops++;
    for (i = 0; i < BS*NB; i++)
	paddr[i] = ops[nops];
    if (msync(paddr, BS*NB, MS_INVALIDATE) < 0)
	printf("msync MS_INVALIDATE failed with %d\n", errno);

    /*
     * NOPS == 2
     */
    nops++;
    setoserror(0);
    lseek(fd, 0, 0);
    if (oserror()) {
	perror("parent lseek 0");
	setoserror(0);
    }
    for (i = 0; i < BS; i++)
	buf[i] = ops[nops];
    for (i = 0; i < NB; i++)
        if (write(fd, buf, BS) != BS) {
	    perror("write ms0file tempfile failed");
	    exit(-1);
	}
    if (oserror()) {
	perror("write");
	setoserror(0);
    }

    if (unblockproc(cpid)) {
	perror("unblock child");
	setoserror(0);
    }
    if (blockproc(ppid)) {
	perror("block self (parent)");
	setoserror(0);
    }

    /*
     * NOPS == 3
     */
    /* check that after msync(MS_INVALIDATE) read gets mmapped data */
    lseek(fd, 0, 0);
    if (oserror()) {
	perror("parent lseek 1");
	setoserror(0);
    }
    if (read(fd, buf, BS) != BS)
        perror("read2 ms0file tempfile failed");
    for (i = 0; i < BS; i++)
        if (buf[i] != ops[nops]) {
	    printf("did not read mmapped data after msync(INVAL)\n");
	    break;
        }

    if (i == BS)
	printf("%d : succeed\n", nops);
    else
	printf("buf[%d] is %x, paddr[%d] is %x\n", i, buf[i], i, paddr[i]);

    /*
     * NOPS == 4
     */
    nops++;
    for (i = 0; i < BS; i++)
	paddr[i] = ops[nops];
    if (msync(paddr, BS*NB, 0) < 0) {
	printf("msync failed with %d\n", oserror());
	setoserror(0);
    }

    if (unblockproc(cpid)) {
	perror("parent unblock child");
	setoserror(0);
    }
    if (blockproc(ppid)) {
	perror("parent block self");
	setoserror(0);
    }

    for (i = 0; i < BS; i++)
	buf[i] = ops[0];

    printf("testing msync(MS_ASYNC) ");
    for (;;) {
	lseek(fd, BS*(NB-1), 0);
	if (read(fd, buf, BS) != BS) {
	    perror("read ms0file tempfile failed");
	    break;
	}
	for (i = 0; i < BS; i++)
	    if (buf[i] != ops[nops]) {
		printf("...");
		break;
	    }
	if (i == BS) {
	    printf("%d : succeed\n", nops);
	    break;
	}
    }

    if (unblockproc(cpid)) {
	perror("parent unblock child");
	setoserror(0);
    }

    if (munmap(paddr, BS*NB) < 0) 
	perror("munmap failed");

    printf("test done.\n");
}

void
child()
{
    int i;

    /*
     * Since parent probably hasn't executed
     * cpid = sproc(....) yet, can't access
     * cpid and expect to find the actual id.
     */
    if (blockproc(getpid())) {
	perror("block self (child)");
	errno = 0;
    }

    /*
     * NOPS == 2
     */

    /* check results of msync with MS_INVALIDATE */
    for (i = 0; i < BS*NB; i++)
        if (paddr[i] != ops[nops]) {
	    printf("memory is not invalidated correctly\n");
	    break;
        }
    if (i == BS*NB)
	printf("%d : succeed\n", nops);
    else
	printf("ops[%d] is %x, paddr[%d] is %x\n",
		nops, ops[nops], i, paddr[i]);

    /*
     * NOPS == 3
     */
    nops++;
    for (i = 0; i < BS*NB; i++)
	paddr[i] = ops[nops];
    if (msync(paddr, BS*NB, MS_INVALIDATE) < 0)
	printf("msync MS_INVALIDATE failed with %d\n", errno);


    if (unblockproc(ppid)) {
	perror("unblock parent (child)");
	setoserror(0);
    }
    if (blockproc(getpid())) {
	perror("block self (child)");
	setoserror(0);
    }

    /*
     * NOPS == 4
     */
    /* check results of msync without MS_INVALIDATE */
    lseek(fd, 0, 0);
    if (read(fd, buf, BS) != BS)
        perror("read2 ms0file tempfile failed");
    for (i = 0; i < BS; i++)
        if (buf[i] != ops[nops]) {
	    printf("read incorrect data after msync(...0)\n");
	    break;
        }

    if (i == BS)
	printf("%d : succeed\n", nops);
    else
	printf("buf[%d] is %x, paddr[%d] is %x\n", i, buf[i], i, paddr[i]);

    /*
     * NOPS == 5
     */
    nops++;
    for (i = 0; i < BS*NB; i++)
	paddr[i] = ops[nops];
    if (msync(paddr, BS*NB, MS_ASYNC) < 0)
	printf("msync MS_INVALIDATE failed with %d\n", errno);

    if (unblockproc(ppid)) {
	perror("unblock parent (child)");
	setoserror(0);
    }
    if (blockproc(getpid())) {
	perror("block self (child)");
	setoserror(0);
    }
    exit(0);
}
