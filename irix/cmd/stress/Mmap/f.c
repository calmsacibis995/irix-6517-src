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

#ident	"$Revision: 1.5 $ $Author: jwag $"
/* program to test mmap */
/* test different flags of mmap */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/prctl.h>

caddr_t paddr;
char cstring[] = ";<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ", buf[1024];
int gint, pg, fd, ppid;

extern void slave(void);

int
main(void)
{
    int i, spid;
    char *filename;

    pg = getpagesize();

    filename = tempnam(NULL, "mf");
    if ((fd = open(filename, O_CREAT | O_RDWR, 0666)) < 0) {
        perror("open of tmpfile failed");
	exit(-1);
    }
    
    /* write to mapped file */
    for (i = 0; i < 1024; i++)
	buf[i] = cstring[i % 32];
    for (i = 0; i < 4 * pg; i += 1024)
        if (write(fd, buf, 1024) != 1024) {
	    fprintf(stderr, "write to tmpfile failed with %d\n", errno);
	    unlink(filename);
	    exit(-1);
        }    

    ppid = getpid();

    if ((spid = fork()) < 0) {
	perror("fork failed");
	unlink(filename);
	exit(-1);
    }
    else if (spid == 0) {
	    slave();
	    exit(0);
	 }

    if ((long)(paddr = mmap(0,4*pg,PROT_WRITE,MAP_SHARED,fd,0)) < 0L) {
	perror("mmap failed");
	unlink(filename);
	kill(spid, SIGKILL);
        exit(-1);
    }

    unblockproc(spid);
    blockproc(ppid);

    if (munmap(paddr, 4*pg) < 0) {
	perror("parent munmap failed");
	unlink(filename);
	exit(-1);
    }
    printf("test done.\n");
    unlink(filename);
    return 0;
}

void
slave(void)
{
    pid_t spid;
    caddr_t tmpaddr;

    /* wait for parent to mmap */
    spid = getpid();
    blockproc(spid);

    if ((long)(paddr = mmap(0,2*pg,PROT_WRITE,MAP_SHARED,fd,0)) < 0L) {
	perror("mmap failed");
	kill(ppid, SIGKILL);
        exit(-1);
    }
    if (munmap(paddr, 2*pg) < 0) {
	perror("first child munmap failed");
	kill(ppid, SIGKILL);
	exit(-1);
    }

    tmpaddr = paddr + 2*pg;

    if ((long)(paddr=mmap(tmpaddr,pg,PROT_WRITE,MAP_SHARED,fd,pg)) < 0L) {
	perror("mmap failed in child");
	kill(ppid, SIGKILL);
	exit(-1);
    }
    printf("paddr : %x, tmpaddr : %x\n", paddr, tmpaddr);


    if (munmap(paddr, pg) < 0) {
	perror("second child munmap failed");
	kill(ppid, SIGKILL);
	exit(-1);
    }

    if ((long)(paddr=mmap(tmpaddr,
			pg, PROT_READ,
			MAP_SHARED|MAP_FIXED, fd, pg)) < 0L) {
	perror("mmap MAP_FIXED failed");
	kill(ppid, SIGKILL);
	exit(-1);
    }
    if (paddr != tmpaddr)
	printf("mmap of MAP_FIXED get %x should be %x\n", paddr, tmpaddr);
    if (munmap(paddr, pg) < 0) {
	perror("third child munmap failed");
	kill(ppid, SIGKILL);
	exit(-1);
    }
    unblockproc(ppid);

    printf("test in child done.\n");
}
