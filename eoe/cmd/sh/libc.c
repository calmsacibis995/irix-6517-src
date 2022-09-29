/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Sh includes a private IOB table since we don't need or want to
 * pull in the huge multihundred file IOB table from LIBC.
 */

#define MFILES	3
#define	SMBUFSZ	8

#define	_NFILE	MFILES
#include <stdio.h>
#include <sys/types.h>

#pragma weak __iob = _iob

unsigned char _smbuf[_NFILE][SMBUFSZ] = { 0 };
unsigned char _bufendadj[_NFILE] = { 0 };
unsigned char _bufdirtytab[_NFILE] = { 0 };
unsigned char *_bufendtab[_NFILE] = { &_smbuf[0][SMBUFSZ], &_smbuf[1][SMBUFSZ],
		&_smbuf[2][SMBUFSZ] };

FILE _iob[_NFILE] = {
#if _MIPS_SIM == _ABIN32
	{ 0, _smbuf[0], _smbuf[0], _IOREAD+_IONBF, 0, 0},
	{ 0, _smbuf[1], _smbuf[1],  _IOWRT+_IONBF, 1, 1},
	{ 0, _smbuf[2], _smbuf[2],  _IOWRT+_IONBF, 2, 2},
#else
	{ 0, _smbuf[0], _smbuf[0], _IOREAD+_IONBF, 0},
	{ 0, _smbuf[1], _smbuf[1],  _IOWRT+_IONBF, 1},
	{ 0, _smbuf[2], _smbuf[2],  _IOWRT+_IONBF, 2},
#endif
};

FILE *_lastbuf = &_iob[_NFILE];

/*
 * sh doesn't use private exit ops
 */
atexit(void *func)
{
	abort();
}

void
_exithandle(void)
{
	return;
}

void
__eachexithandle(void)
{
	return;
}
