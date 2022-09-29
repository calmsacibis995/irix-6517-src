/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/rewind.c	1.7"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
#include "synonyms.h"
#include "mplib.h"
#include "stdiom.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>		/* prototype for lseek() */

void
rewind(register FILE *iop)
{
	LOCKDECLINIT(l, LOCKFILE(iop));
	(void)fflush(iop);
	(void)lseek(fileno(iop), 0L, 0);
	iop->_cnt = 0;
	iop->_ptr = iop->_base;
	if (bufendadj(iop))
		resetbufend(iop);
	iop->_flag &= (unsigned short)~(_IOERR | _IOEOF);
	if(iop->_flag & _IORW)
		iop->_flag &= (unsigned short)~(_IOREAD | _IOWRT);
	UNLOCKFILE(iop, l);
}
