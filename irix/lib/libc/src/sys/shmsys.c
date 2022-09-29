/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Revision: 1.18 $"

#ifdef __STDC__
	#pragma weak shmat = _shmat
	#pragma weak shmctl = _shmctl
	#pragma weak shmdt = _shmdt
	#pragma weak shmget = _shmget
#endif
#include	"synonyms.h"
#include	<stdarg.h>
#include	<sys/types.h>
#include	<sys/ipc.h>
#include	<sys.s>
#include	"sys_extern.h"

#define SHMSYS SYS_shmsys

#define	SHMAT	0
#define	SHMCTL	1
#define	SHMDT	2
#define	SHMGET	3
#define SHMSTAT	4

void *
shmat(int shmid, void *shmaddr, int shmflg)
{
	return((void *)syscall(SHMSYS, SHMAT, shmid, shmaddr, shmflg));
}

int
shmctl(int shmid, int cmd, ...)
{
	void *buf;
	va_list ap;

	va_start(ap, cmd);
	buf = va_arg(ap, void *);
	va_end(ap);
	return((int)syscall(SHMSYS, SHMCTL, shmid, cmd, buf));
}

int
shmdt(void *shmaddr)
{
	return((int)syscall(SHMSYS, SHMDT, shmaddr));
}

int
shmget(key_t key, size_t size, int shmflg)
{
	return((int)syscall(SHMSYS, SHMGET, key, size, shmflg));
}

struct shmstat;

int
__shmstatus(struct shmstat *stat)
{
	return((int)syscall(SHMSYS, SHMSTAT, stat));
}
