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

#ident	"$Revision: 1.5 $ $Author: sfc $"
/*
 * Should be able to map a file if not sharing file descriptors.
 */
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>

char	*vaddr;

void catch();
void	mapit();
int	fd;
int	ppid;

main()
{
	int	cpid;

	ppid = getpid();
	if ((cpid = sproc(mapit, PR_SADDR, 0)) < 0) {
		printf("sproc failed with errno %d\n", errno);
		exit(-1);
	}
	blockproc(ppid);
}

void
mapit(arg)
	char *arg;
{
	caddr_t		vaddr;
	char		c;
	struct stat	statbuf;
	int		fd, i;

	signal(SIGSEGV, catch);
	fd = open("/etc/passwd", O_RDONLY);
	if (fd < 0) {
		perror("nfds:open");
		printf("nfds:test failed!\n");
		unblockproc(ppid);
		exit(-1);
	}
	if (stat("/etc/passwd", &statbuf)) {
		perror("nfds:stat");
		printf("nfds:test failed!\n");
		unblockproc(ppid);
		exit(-1);
	}
	vaddr = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if ((long)vaddr == -1L) {
		printf("nfds:test failed!\n");
		unblockproc(ppid);
		exit(0);
	}
	for (i = 0; i < statbuf.st_size; i++)
		c = *(vaddr+i);
	printf("nfds:test passed!\n");
	unblockproc(ppid);
	c = *vaddr;
	*vaddr = c;
	printf("nfds:should not get here!\n");
}

void
catch(sig, code)
{
	/*
	errno = code;
	fprintf(stderr, "nfds:got sig# %d ", sig);
	perror(" ");
	*/
	exit(0);
}
