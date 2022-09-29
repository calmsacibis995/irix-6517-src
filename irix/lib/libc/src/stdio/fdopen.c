/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/fdopen.c	1.16"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * Unix routine to do an "fopen" on file descriptor
 * The mode has to be repeated because you can't query its
 * status
 */
#ifdef __STDC__
	#pragma weak fdopen = _fdopen
#endif
#include "synonyms.h"
#include <sgidefs.h>
#include <stdio.h>
#include <limits.h>
#include "stdiom.h"
#include "mplib.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>		/* prototype for lseek64() */

FILE *
fdopen(int fd, const char *type)	/* associate file desc. with stream */
{
	register FILE *iop;
	register int plus;
	register unsigned char flag;
	struct stat64 buf;
	LOCKDECLINIT(l, LOCKOPEN);

	if (fstat64(fd, &buf) == -1 && errno==EBADF) {
		UNLOCKOPEN(l);
		return 0;
	}

	if (fd > (int)FD_MAX || (iop = _findiop()) == 0) {
		UNLOCKOPEN(l);
		return 0;
	}

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	iop->_file = (Uchar) fd;
#else
	iop->_file = fd;
#endif

	switch (type[0])
	{
	default:
		UNLOCKOPEN(l);
		return 0;
	case 'r':
		flag = _IOREAD;
		break;
	case 'a':
		(void)lseek64(fd, 0LL, 2);
		/*FALLTHROUGH*/
	case 'w':
		flag = _IOWRT;
		break;
	}
	if ((plus = type[1]) == 'b')	/* Unix ignores 'b' ANSI std */
		plus = type[2];
	if (plus == '+')
		flag = _IORW;
	iop->_flag = flag;
	UNLOCKOPEN(l);
	return iop;
}
