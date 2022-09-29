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

#ident	"$Revision: 1.6 $ $Author: sfc $"
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "stress.h"

char *Cmd;

static void ckfail(char *s, int ret, int err, int exerr);

int
main(int argc, char **argv)
{
	int i, fd;
	caddr_t paddr;
	char *buf;
	int sz = 0;
	char *filename;

	Cmd = errinit(argv[0]);

	filename = tempnam(NULL, "msclo");
	sz = getpagesize();
	buf = malloc(sz);

	/* prepare the mapped file */
	if ((fd = open(filename, O_CREAT | O_RDWR)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "open of %s failed", filename);
		/* NOTREACHED */
	}
	if (unlink(filename) < 0) {
		errprintf(ERR_ERRNO_EXIT, "unlink of %s failed", filename);
		abort();
	}

	for (i = 0; i < sz; i++)
		buf[i] = '.';
	if (write(fd, buf, sz) != sz) {
		errprintf(ERR_ERRNO_EXIT, "write tmpfile failed");
		/* NOTREACHED */
	}
	if ((paddr = mmap(0,sz,PROT_WRITE,MAP_SHARED,fd,0)) == (caddr_t)-1L) {
		errprintf(ERR_ERRNO_EXIT, "mmap failed");
		/* NOTREACHED */
	}

	ckfail("BIG", msync(paddr, sz*10, 0), errno, ENOMEM);

	if (mpin(paddr, sz) < 0) {
		errprintf(ERR_ERRNO_EXIT, "mpin failed");
		/* NOTREACHED */
	}

	ckfail("INVAL", msync(paddr, sz, MS_INVALIDATE), errno, EBUSY);

	if (close(fd) < 0) {
		errprintf(ERR_ERRNO_EXIT, "close failed");
		/* NOTREACHED */
	}

	if (munpin(paddr, sz) < 0) {
		errprintf(ERR_ERRNO_EXIT, "munpin failed");
		/* NOTREACHED */
	}

	if (msync(paddr, sz, MS_INVALIDATE)) {
		errprintf(ERR_ERRNO_EXIT, "msync invalidate");
		/* NOTREACHED */
	}

	for (i = 0; i < sz; i++)
		paddr[i] = '+';

	printf("%s:PASSED\n", Cmd);
	exit(0);
}

static void
ckfail(char *s, int ret, int err, int exerr)
{
	if (ret < 0) {
		if (err != exerr)
			errprintf(ERR_EXIT, "%s: failed with errno %d should be %d\n",
				s, err, exerr);
			/* NOTREACHED */
	} else {
		errprintf(ERR_EXIT, "%s: should fail but succeded.\n", s);
		/* NOTREACHED */
	}
}
