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
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

#define MAPSZ 0x800000
#define NMAPS 20

char	*files[] = {
	"f00", "f01", "f02", "f03", "f04",
	"f05", "f06", "f07", "f08", "f09",
	"f10", "f11", "f12", "f13", "f14",
	"f15", "f16", "f17", "f18", "f19",
};

main(argc, argv)
char **argv;
{
	register int i;
	void	sigbus();

	signal(SIGSEGV, sigbus);

	for (i = 0; i < NMAPS; i++) {
		register int j;
		register caddr_t p, s;
		char *name;
		int fd;

		name = tempnam(NULL, files[i]);
		printf("agfull:Creating %s\n", name);
		fd = open(name, O_RDWR|O_CREAT, 0777);
		if (fd < 0) {
			perror("agfull:open");
			exit(1);
		}

		if (unlink(name) < 0) {
			perror("agfull:unlink");
			exit(1);
		}

		s = p = mmap(0, MAPSZ, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_AUTOGROW, fd, 0);
		if (p == (caddr_t)-1L) {
			perror("agfull:map");
			exit(1);
		} else
			printf("agfull:map returned 0x%x\n", p);
		
		for (j = 0; j < MAPSZ; j++) {
			*s++ = 'x';
		}

		if (munmap(p, MAPSZ) < 0) {
			perror("agfull:unmap");
			exit(1);
		}
	}
}

void
sigbus(sig, code)
{
	printf("agfull:should get file system full error (code == %d)\n", code);
	errno = code;
	perror("agfull:signal handler");
	exit(0);
}

