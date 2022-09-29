/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ucblibc:port/stdio/fopen.c	1.2"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

#ifdef __STDC__
	#pragma weak BSDfopen   = _BSDfopen
	#pragma weak BSDfreopen = _BSDfreopen
#endif
#include "synonyms.h"

/*LINTLIBRARY*/
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "../stdio/stdiom.h"
#include "bsd_extern.h"

static FILE *_BSD_endopen(const char *file, const char *mode, register FILE *iop);

FILE *
BSDfopen(const char *file, const char *mode)
{
	return (_BSD_endopen(file, mode, _findiop()));
}

FILE *
BSDfreopen(const char *file, const char *mode, FILE *iop)
{
	(void) fclose(iop); /* doesn't matter if this fails */
	return (_BSD_endopen(file, mode, iop));
}

static FILE *
_BSD_endopen(const char *file, const char *mode, register FILE *iop)
{
	register int	plus, oflag, fd;

	if (iop == NULL || file == NULL || file[0] == '\0')
		return (NULL);
	plus = (mode[1] == '+');
	switch (mode[0]) {
	case 'w':
		oflag = (plus ? O_RDWR : O_WRONLY) | O_TRUNC | O_CREAT;
		break;
	case 'a':
		oflag = (plus ? O_RDWR : O_WRONLY) | O_CREAT;
		break;
	case 'r':
		oflag = plus ? O_RDWR : O_RDONLY;
		break;
	default:
		return (NULL);
	}
	if ((fd = open(file, oflag, 0666)) < 0)
		return (NULL);
	iop->_cnt = 0;
	iop->_file = fd;
	iop->_flag = plus ? _IORW : (mode[0] == 'r') ? _IOREAD : _IOWRT;
	if (mode[0] == 'a')   {
		if ((lseek(fd,0L,2)) < 0)  {
			(void) close(fd);
			return NULL;
		}
	}
	iop->_base = iop->_ptr = NULL;
	/*
	 * Sys5 does not support _bufsiz
	 *
	 * iop->_bufsiz = 0;
	 */
	return (iop);
}
