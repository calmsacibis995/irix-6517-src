#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <signal.h>
#include <errno.h>
#include "stress.h"
#define _PAGESZ 4096
#include "sys/usrdma.h"

/*
 * test mapped devices a bit
 */
#define NAME "/dev/vme/dma0"
char *Cmd;
int verbose;

int
main(int argc, char **argv)
{
	int pagesize, fd, c;
	char *v1;
	off_t mapoff, bogusoff;
	/* REFERENCED */
	char tmp;
	pid_t pid;

	Cmd = errinit(argv[0]);
	pagesize = getpagesize();

	while ((c = getopt(argc, argv, "v")) != EOF)
		switch (c) {
		case 'v':
			verbose++;
			break;
		}

	mapoff = (off_t)(pagesize * VME_MAP_REGS);
	bogusoff = (off_t)0x1;

	if ((fd = open(NAME, O_RDWR)) < 0) {
		if (errno == ENOENT) {
			fprintf(stderr, "Cannot open %s - can't run test!\n",
				NAME);
			return 0;
		}
		errprintf(ERR_ERRNO_EXIT, "open");
	}
	
	/*
	 * map a bogus address that the driver will reject to test a
	 * bit of error handling
	 */
	v1 = mmap(0, pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, bogusoff);
	if (v1 != (char *)MAP_FAILED)
		errprintf(ERR_EXIT, "mmap-bogus");

	v1 = mmap(0, pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mapoff);
	if (v1 == (char *)MAP_FAILED)
		errprintf(ERR_ERRNO_EXIT, "mmap");
	close(fd);

	/* make sure still mapped - try to open again */
	if ((fd = open(NAME, O_RDWR)) >= 0)
		errprintf(ERR_EXIT, "reopen");
	
	/* read registers */
	tmp = *v1;

	/* unmap */
	if (munmap(v1, pagesize) != 0)
		errprintf(ERR_EXIT, "munmap");
	
	/*
	 * now check that things get dupped on fork.
	 */
	if ((fd = open(NAME, O_RDWR)) < 0) {
		if (errno == ENOENT) {
			fprintf(stderr, "Cannot open %s - can't run test!\n",
				NAME);
			return 0;
		}
		errprintf(ERR_ERRNO_EXIT, "open2");
	}
	
	v1 = mmap(0, pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mapoff);
	if (v1 == (char *)MAP_FAILED)
		errprintf(ERR_ERRNO_EXIT, "mmap2");
	close(fd);

	if ((pid = fork()) < 0)
		errprintf(ERR_ERRNO_EXIT, "fork2");
	else if (pid == 0) {
		/* child */
		while (getppid() != 1)
			sginap(1);
		/* make sure still mapped - try to open again */
		if ((fd = open(NAME, O_RDWR)) >= 0)
			errprintf(ERR_EXIT, "reopen");
		
		/* read registers */
		tmp = *v1;
	} else {
		/* parent - we exit first */
		return 0;
	}
	printf("%s:SUCCESS\n", Cmd);

	return 0;
}
