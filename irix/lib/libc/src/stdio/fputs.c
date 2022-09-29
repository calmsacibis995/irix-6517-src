/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/fputs.c	3.20"
/*LINTLIBRARY*/
/*
 * Ptr args aren't checked for NULL because the program would be a
 * catastrophic mess anyway.  Better to abort than just to return NULL.
 */
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>
#include "stdiom.h"
#include <string.h>
#include <unistd.h>

int
fputs(const char *ptr, FILE *iop)
{
	ptrdiff_t n, ndone = 0;
	register unsigned char *cptr, *bufend;
	char *p;
	LOCKDECLINIT(l, LOCKFILE(iop));

	if (_WRTCHK(iop)) {
		UNLOCKFILE(iop, l);
		return EOF;
	}
	bufend = _bufend(iop);

	if ((iop->_flag & _IONBF) == 0)  
	{
		for ( ; ; ptr += n) 
		{
			while ((n = bufend - (cptr = iop->_ptr)) <= 0)  
			{
				/* full buf */
				if (_xflsbuf(iop) == EOF) {
					UNLOCKFILE(iop, l);
					return(EOF);
				}
			}
			if ((p = memccpy((char *) cptr, ptr, '\0', (size_t)n)) != 0)
				n = (p - (char *) cptr) - 1;
			iop->_cnt -= n;
			iop->_ptr += n;
			if (_needsync(iop, bufend))
				_bufsync(iop, bufend);
			ndone += n;
			if (p != 0)  
			{ 
				/* done; flush buffer if line-buffered */
	       			if (iop->_flag & _IOLBF)
	       				if (_xflsbuf(iop) == EOF) {
						UNLOCKFILE(iop, l);
	       					return EOF;
					}
				UNLOCKFILE(iop, l);
	       			return (int)ndone;
	       		}
		}
		/* NOTREACHED */
	}
	else  
	{
		/* write out to an unbuffered file */
                size_t cnt = strlen(ptr);
                register ssize_t num_wrote;
                size_t count = cnt;

                while((num_wrote = write(iop->_file, ptr, count)) != count) {
                                if (num_wrote <= 0) {
                                        iop->_flag |= _IOERR;
					UNLOCKFILE(iop, l);
                                        return EOF;
                                }
                                count -= num_wrote;
                                ptr += num_wrote;
                }
		UNLOCKFILE(iop, l);
                return ((int)cnt);
	}
}
