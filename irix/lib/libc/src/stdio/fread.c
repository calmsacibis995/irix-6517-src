/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/fread.c	3.29"
/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>
#include "stdiom.h"
#include <stddef.h>
#include <values.h>
#include <memory.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

size_t
fread(void *ptr, size_t size, size_t count, FILE *iop)
{
	register size_t nleft;		/* total bytes to read */
	register size_t n;		/* number of bytes transfered this time */
					/* will never be greater than MAXINT */
	register size_t ndone = 0;	/* number of size bytes done */
	register size_t leftover = 0;	/* bytes left from last whole size read */
	Uchar *bufend;
	size_t orig_count = count;
	LOCKDECL(l);

	if(count == 0) return 0;

	LOCKINIT(l, LOCKFILE(iop));
	if (!(iop->_flag & (_IOREAD | _IORW))) { /* is it a readable stream */
		iop->_flag |= _IOERR;
		UNLOCKFILE(iop, l);
		setoserror(EBADF);
		return 0;
	}
	if (iop->_base == 0)
	{
	 	/* avoid repetitive testing for !bufend after _filbuf() */
		if ((bufend = _findbuf(iop)) == 0) {
			UNLOCKFILE(iop, l);
			return 0;
		}
	}
	else
		bufend = _bufend(iop);

	/* Only loops if (count * size) overflows */
	for (;;)
	{
		nleft = count * size;	/* may overflow */
		if (nleft < count || nleft < size)
		{
			if ((nleft = size) == 0) {  /* overflow: use a safe amount */
				UNLOCKFILE(iop, l);
				return 0;
			}
			count--;		/* one-at-a-time */
		}
		else
			count = 0;		/* going for the whole thing */
		do 
		{
			if (iop->_cnt <= 0)	/* empty buffer */
			{
				if (nleft >= BUFSIZ)	/* use direct read */
				{
					register ssize_t res;

					/*
					* only want to do read in BUFSIZ 
					* chunks - performance 
					*/
					if (nleft < MAXVAL)
						n = MULTIBFSZ(nleft);
					else
						n = MAXVAL;
					if ((res = read(fileno(iop), ptr, n)) != n)
					{
						if (res > 0) {
							n = res;
							goto reduce;
						}
						else if (res == 0)
							iop->_flag |= _IOEOF;
						else
							iop->_flag |= _IOERR;
						/*
						* any partial part of a
						* size successfully read is ignored
						*/
						UNLOCKFILE(iop, l);
						return ndone;
					}
					goto reduce;
				}
				else if (_filbuf(iop) != EOF)
				{
					iop->_ptr--;	/* unget the char */
					iop->_cnt++;
				}
				else	/* at EOF */
				{
					/*
					* any partial part of a
					* size successfully read is ignored
					*/
					UNLOCKFILE(iop, l);
					return ndone;
				}
			}
			if ((n = iop->_cnt) > nleft)
				n = nleft;
			(void)memcpy(ptr, (char *)iop->_ptr, (unsigned)n);
			iop->_cnt -= n;
			iop->_ptr += n;
			if (_needsync(iop, bufend))
				_bufsync(iop, bufend);
		reduce:;
			if (((nleft -= n) <= 0) && count == 0) {
				UNLOCKFILE(iop, l);
				return orig_count;
			}
			ptr = (char *)ptr +  n;
			n += leftover;
			ndone += n / size;
			leftover = n % size;	
		} while (nleft != 0);
	}
}
