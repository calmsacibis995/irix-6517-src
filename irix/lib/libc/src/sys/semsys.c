/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Revision: 1.13 $"
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifdef __STDC__
	#pragma weak semctl = _semctl
	#pragma weak semget = _semget
	#pragma weak semop = _semop
#endif
#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/ipc.h>
#include	<sys/sem.h>
#include	<sys.s>
#include	<stdarg.h>
#include	"sys_extern.h"
#include	"mplib.h"

#define SEMSYS SYS_semsys

#define	SEMCTL	0
#define	SEMGET	1
#define	SEMOP	2
#define SEMSTAT 3

int
semctl(int semid, int semnum, int cmd, ...)
{
	union semun arg;
	va_list ap;

	va_start(ap, cmd);
	arg = va_arg(ap, union semun);
	va_end(ap);
	return((int)syscall(SEMSYS, SEMCTL, semid, semnum, cmd, arg));
}

int
semget(key_t key, int nsems, int semflg)
{
	return((int)syscall(SEMSYS, SEMGET, key, nsems, semflg));
}

int
semop(int semid, struct sembuf *sops, size_t nsops)
{
	extern int __syscall(int, ...);

	MTLIB_BLOCK_CNCL_RET( int,
		__syscall(SEMSYS, SEMOP, semid, sops, nsops) );
}

struct semstat;

int
__semstatus(struct semstat *stat)
{
	return((int)syscall(SEMSYS, SEMSTAT, stat));
}
