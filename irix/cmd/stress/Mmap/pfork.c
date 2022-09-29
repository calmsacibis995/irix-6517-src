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

/*
 *	Parent maps a zero-length PRIVATE file with AUTOGROW.
 *	Stores to the file and msync-s all of user address
 *	space and checks that file length remains 0.
 *
 *	Process forks and each process stores private data
 *	into the address space and checks that its data has
 *	not been trashed by the other.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <errno.h>

#define	MPSZ	8192

int	pid, ppid;
caddr_t	addr;

main(agrc, argv)
char	**argv;
{
	int fd;
	char c;
	register int i, j;
	struct stat statbuf;
	int dosleep = 0;
	char *filename;

	pid = getpid();

	filename = tempnam(NULL, "fkpriv");
	fd = open(filename, O_CREAT|O_RDWR, 0666);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	addr = mmap(0, MPSZ, PROT_READ|PROT_WRITE,
				MAP_PRIVATE|MAP_AUTOGROW, fd, 0);

	if (addr == (caddr_t)-1L) {
		perror("map");
		unlink(filename);
		exit(1);
	}

	for (i = 0; i < MPSZ; i++)
		*(addr+i) = 0;

	if (msync(addr, MPSZ, 0)) {
		perror("msync");
		exit(1);
	}

	if (fstat(fd, &statbuf)) {
		perror("fstat");
		exit(1);
	}
	if (statbuf.st_size) {
		fprintf(stderr, "Error: temp file %s non-zero!\n", filename);
		exit(1);
	}

	ppid = getpid();

	switch(pid = fork()) {
	case -1:
		perror("fork");
		unlink(filename);
		exit(1);
		/* NOTREACHED */
	case 0:
		pid = getpid();
		dosleep++;
		break;
	}
	unlink(filename);

	c = pid & 0xff;

	for (i = 0; i < MPSZ; i++)
		*(addr+i) = c;

	for (j = 3; j > 0; j--) {

		sleep(1);

		for (i = 0; i < MPSZ; i++) {
			if (*(addr+i) != c) {
				printf("%s: found %c instead of %c!\n",
					argv[0], *(argv+i), c);
				printf("	test FAILED!\n");
				kill(getpid() == ppid ? pid : ppid, SIGKILL);
				exit(1);
			}
		}
	}
	if (dosleep) {
		sleep(3);
		exit(0);
	}
	sleep(2);
	printf("%s: test PASSED!\n", argv[0]);
}
