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
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#define	MPSZ	8192

int	pid;
caddr_t	addr;

main(agrc, argv)
char	**argv;
{
	int fd;
	char c;
	register int i;
	int dosleep = 0;
	char *filename;

	pid = getpid();

	filename = tempnam(NULL, "fksha");
	fd = open(filename, O_CREAT|O_RDWR, 0666);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	addr = mmap(0, MPSZ, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_AUTOGROW, fd, 0);
	if (addr == (caddr_t)-1L) {
		perror("map");
		unlink(filename);
		exit(1);
	}

	for (i = 0; i < MPSZ; i++) {
		*(addr+i) = 0;
	}

	switch(fork()) {
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

	for (i = 0; i < MPSZ; i++) {
		*(addr+i) = c;
	}

	if (dosleep)
		sleep(3);
	printf("%s: test PASSED!\n", argv[0]);
}
