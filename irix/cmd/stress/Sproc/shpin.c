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

#ident	"$Revision: 1.1 $ $Author: yohn $"

#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "sys/types.h"
#include "sys/mman.h"
#include "sys/prctl.h"
#include "sys/stat.h"
#include "stdio.h"
#include "fcntl.h"
#include "signal.h"
#include "ulocks.h"

/*
 * try a sproc and mpin mapped file -- tests mrlock in update mode
 */
void runit(int);

caddr_t vaddr;
int nthreads;
off_t size;

int
main(int argc, char **argv)
{
	extern void slave();
	int pid, i, fd;
	struct stat statbuf;
	ptrdiff_t p;

	if (argc < 2) {
		fprintf(stderr, "Usage:%s nloop\n", argv[0]);
		exit(-1);
	}

	nthreads = atoi(argv[1]);

	if ((p = usconfig(CONF_INITUSERS, nthreads+2)) < 0) {
		perror("shpin: usconfig");
		exit(-1);
	}

	/* open a file for mapping */
	if ((fd = open("/unix", O_RDONLY, 0666)) < 0) {
		perror("shpin: cannot open /unix");
		exit(-1);
	}

	if (fstat(fd, &statbuf) < 0) {
		perror("shpin: stat failed");
		exit(-1);
	}

	size = statbuf.st_size;

	/* now map in file */
	if ((long)(vaddr = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0)) < 0L) {
		perror("shpin: mmap failed");
		exit(-1);
	}

	for (i = 0; i < nthreads; i++) {
		/* now sproc */
		if ((pid = sproc(slave, PR_SADDR|PR_SFDS)) < 0) {
			perror("shpin: sproc failed");
			exit(-1);
		}
	}

	runit(nthreads);
}

void
runit(int niterations)
{
	int i;

	/* now detach and re-attach */
	for (i = 0; i < niterations; i++) {

		/* pin file */
		if (mpin(vaddr, size) < 0) {
			perror("shpin: mpin failed");
			exit(-1);
		}

		sginap(1);

		if (munpin(vaddr, size) < 0) {
			perror("shpin: munpin failed");
			exit(-1);
		}

		sginap(1);
	}
}

void
slave()
{
	runit(nthreads);
}
