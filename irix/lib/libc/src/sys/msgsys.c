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
#ident "$Revision: 1.11 $ $Author: jph $"

#include	<sys/types.h>
#include	<sys/ipc.h>
#include	<sys/msg.h>
#include	<sys.s>
#include	<stdarg.h>
#ifdef __STDC__
	#pragma weak msgctl = _msgctl
	#pragma weak msgget = _msgget
	#pragma weak msgrcv = _msgrcv
	#pragma weak msgsnd = _msgsnd
#endif
#include	"synonyms.h"
#include	"sys_extern.h"
#include	"mplib.h"

#define MSGSYS	SYS_msgsys

#define	MSGGET	0
#define	MSGCTL	1
#define	MSGRCV	2
#define	MSGSND	3

int
msgget(key_t key, int msgflg)
{
	return((int)syscall(MSGSYS, MSGGET, key, msgflg));
}

int
msgctl(int msqid, int cmd, ...)
{
	struct msqid_ds *buf;
	va_list ap;

	va_start(ap, cmd);
	buf = va_arg(ap, struct msqid_ds *);
	va_end(ap);
	return((int)syscall(MSGSYS, MSGCTL, msqid, cmd, buf));
}

int
msgrcv(
	int msqid,
	void *msgp,
	size_t msgsz,
	long msgtyp,
	int msgflg)
{
	extern int __syscall(int, ...);

	MTLIB_BLOCK_CNCL_RET( int,
		__syscall(MSGSYS, MSGRCV, msqid, msgp, msgsz, msgtyp, msgflg) );
}

int
msgsnd(
	int msqid,
	const void *msgp,
	size_t msgsz,
	int msgflg)
{
	extern int __syscall(int, ...);

	MTLIB_BLOCK_CNCL_RET( int,
		__syscall(MSGSYS, MSGSND, msqid, msgp, msgsz, msgflg) );
}
