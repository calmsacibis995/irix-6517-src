/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/fopen.c	1.20"
/*LINTLIBRARY*/

#include <sgidefs.h>
#ifdef __STDC__
	#pragma weak fopen = _fopen
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	/* o32 - pass O_LARGEFILE */
	#pragma weak fopen64 = _fopen64
	#pragma weak freopen64 = _freopen64
#else
	/* 
	 * for 64 and n32 all files are large file aware, so there
	 * is no reason to pass O_LARGEFILE. The kernel shouldn't
	 * even be looking at it..
	 */
	#pragma weak fopen64 = _fopen
	#pragma weak _fopen64 = _fopen
	#pragma weak freopen64 = freopen
	#pragma weak _freopen64 = freopen
#endif
#endif
#include "synonyms.h"
#include <stdio.h>
#include "stdiom.h"
#include "mplib.h"
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

static FILE *
_endopen(	/* open UNIX file name, associate with iop */
	const char *name,
	const char *type,
	register FILE *iop,
	int oflag)
{
	register int plus, fd;

	if (iop == 0)
		return 0;
	switch (type[0])
	{
	default:
		return 0;
	case 'r':
		oflag |= O_RDONLY;
		break;
	case 'w':
		oflag |= O_WRONLY | O_TRUNC | O_CREAT;
		break;
	case 'a':
		oflag |= O_WRONLY | O_APPEND | O_CREAT;
		break;
	}
	/* UNIX ignores 'b' and treats text and binary the same */
	if ((plus = type[1]) == 'b')
		plus = type[2];
	if (plus == '+')
		oflag = (oflag & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
	if ((fd = open(name, oflag, 0666)) < 0)
		return 0;

	if (fd > (int)FD_MAX)
	{
		(void)close(fd);
		return 0;
	}

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	iop->_file = (Uchar) fd; /* assume that fd fits in unsigned char */
#else
	iop->_file = fd;
#endif

	if (plus == '+')
		iop->_flag = _IORW;
	else if (type[0] == 'r')
		iop->_flag = _IOREAD;
	else
		iop->_flag = _IOWRT;
	if (oflag == (O_WRONLY | O_APPEND | O_CREAT))	/* type == "a" */
		if (lseek64(fd, 0LL, 2) < 0LL)
			return NULL;
	return iop;	
}

FILE *
fopen(const char *name, const char *type)/* open name, return new stream */
{
	FILE *res;
	LOCKDECLINIT(l, LOCKOPEN);

	res = _endopen(name, type, _findiop(), 0);
	UNLOCKOPEN(l);
	return(res);
}

FILE *
freopen(	/* open name, associate with existing stream */
	const char *name,
	const char *type,
	FILE *iop)
{
	FILE *res;
	LOCKDECLINIT(l, LOCKFILE(iop));
	LOCKDECL(l2);
	
	/*
	 * Must LOCKOPEN across fclose since otherwise another thread could
	 * get into fopen, grab the fd that has been closed and assign
	 * it to a new iop before we grab the LOCKOPEN
	 */
	LOCKINIT(l2, LOCKOPEN);
	(void)fclose(iop);
	UNLOCKFILE(iop, l);
	res = _endopen(name, type, iop, 0);
	UNLOCKOPEN(l2);
	return res;
}

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
FILE *
fopen64(const char *name, const char *type)/* open name, return new stream */
{
	FILE *res;
	LOCKDECLINIT(l, LOCKOPEN);

	res = _endopen(name, type, _findiop(), O_LARGEFILE);
	UNLOCKOPEN(l);
	return(res);
}

FILE *
freopen64(	/* open name, associate with existing stream */
	const char *name,
	const char *type,
	FILE *iop)
{
	FILE *res;
	LOCKDECLINIT(l, LOCKFILE(iop));
	LOCKDECL(l2);
	
	/*
	 * Must LOCKOPEN across fclose since otherwise another thread could
	 * get into fopen, grab the fd that has been closed and assign
	 * it to a new iop before we grab the LOCKOPEN
	 */
	LOCKINIT(l2, LOCKOPEN);
	(void)fclose(iop);
	UNLOCKFILE(iop, l);
	res = _endopen(name, type, iop, O_LARGEFILE);
	UNLOCKOPEN(l2);
	return res;
}
#endif
