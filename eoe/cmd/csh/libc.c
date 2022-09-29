/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Csh includes a private IOB table since we don't need or want to
 * pull in the huge multihundred file IOB table from LIBC.
 */

#define MFILES	3
#define	SMBUFSZ	8

#define	_NFILE	MFILES
#include <stdio.h>
#include <stdlib.h>
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
 * csh doesn't use private exit ops
 */
/*ARGSUSED*/

/* 
	As the atexit and _exithandle and __eachexithandle are in libc and
	atexit is a special & documented function -- use the one in libc 
	rather than define are own. This was done when csh was static to
	avoid pulling in the libc stuff. Since csh is now dynamic, deleted
	the 3 functions from this file -- so that the ones from libc are used.
*/
