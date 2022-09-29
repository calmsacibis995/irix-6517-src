/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/iob.c	2.14"
/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak _iob = __iob
#endif

#include "synonyms.h"
#include <sgidefs.h>
#include <stdio.h>
#include "stdiom.h"

extern Uchar _smbuf[_NFILE + 1][_SMBFSZ], _sibuf[BUFSIZ], _sobuf[BUFSIZ];

/*
 * Ptrs to end of read/write buffers for first _NFILE devices.
 * There is an extra bufend pointer which corresponds to the dummy
 * file number _NFILE, which is used by sscanf and sprintf.
 */
Uchar *_bufendtab[_NFILE+1] = { &_sibuf[BUFSIZ], &_sobuf[BUFSIZ],
				_smbuf[2] + _SBFSIZ, };

/*
 * These can be replaced in nonshared apps
 */
FILE _iob[_NFILE] = {
#if _MIPS_SIM  == _ABIN32
	{ 0, NULL, NULL, _IOREAD, 0, 0},
	{ 0, NULL, NULL, _IOWRT, 1, 1},
	{ 0, _smbuf[2], _smbuf[2], _IOWRT+_IONBF, 2, 2},
#elif _MIPS_SIM  == _ABI64
	{ 0, NULL, NULL, {0, 0}, _IOREAD, 0},
	{ 0, NULL, NULL, {0, 0}, _IOWRT, 1},
	{ 0, _smbuf[2], _smbuf[2], {0, 0}, _IOWRT+_IONBF, 2},
#else
	{ 0, NULL, NULL, _IOREAD, 0},
	{ 0, NULL, NULL, _IOWRT, 1},
	{ 0, _smbuf[2], _smbuf[2], _IOWRT+_IONBF, 2},
#endif	/* _MIPS_SIM */
};

/*
 * Ptr to end of io control blocks
 */
FILE *_lastbuf = &_iob[_NFILE];

/*
 * File must be last to save wasted space in Xsetalign linker code!!!
 *
 * Ptrs to start of preallocated buffers for stdin, stdout.
 * Some slop is allowed at the end of the buffers in case an upset in
 * the synchronization of _cnt and _ptr (caused by an interrupt or other
 * signal) is not immediately detected.
 *
 * Note: _smbuf should come first since it is much more likely to be used,
 *	with _sobuf next and _sibuf last (least likely to be used)
 */
Uchar _smbuf[_NFILE + 1][_SMBFSZ];
