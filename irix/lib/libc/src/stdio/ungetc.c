/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/ungetc.c	2.11"
/*	3.0 SID #	1.3	*/
/*LINTLIBRARY*/
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>
#include <stdlib.h>
#include "stdiom.h"
#include <errno.h>

int
ungetc(int c, FILE *iop)
{
	Uchar *from, *to;
	ssize_t i;
	LOCKDECL(l);

	if (c == EOF)   
		return EOF;
	LOCKINIT(l, LOCKFILE(iop));
	if (iop->_ptr == iop->_base)
	{
		if (iop->_base == 0)
		{
			if (_findbuf(iop) == 0)
			{
				UNLOCKFILE(iop, l);
				return EOF;
			}
		}
		else if (iop->_ptr + iop->_cnt >= _bufend(iop))
		{
			if (bufendadj(iop))
			{
				UNLOCKFILE(iop, l);
				return EOF;
			}
			incbufend(iop, PUSHBACK);
		}
		for (i = iop->_cnt - 1, from = iop->_ptr + i,
		      to = from + PUSHBACK;
		     i >= 0;
		     from--, to--, i--)
			*to = *from;
		iop->_ptr += PUSHBACK;
		setbufdirty(iop);
	}
	if ((iop->_flag & _IOREAD) == 0) /* basically a no-op on write stream */
		if (iop->_flag & _IOWRT)
		{
			setoserror(EBADF);
			UNLOCKFILE(iop, l);
			return EOF;
		}
		else
			++iop->_ptr;

	if (iop->_ptr[-1] != c)
		setbufdirty(iop);
	*--iop->_ptr = c;
	++iop->_cnt;
	iop->_flag &= (unsigned short)~_IOEOF;
	UNLOCKFILE(iop, l);
	return c;
}
