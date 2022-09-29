/*
 * msg.h --
 *
 * 	header for msg(2)
 *
 *
 * Copyright 1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_MSG_H
#define _SYS_MSG_H

#ifdef __cplusplus
extern "C" {
#endif
#include <standards.h>

#ident	"$Revision: 3.30 $"

/*
 * X/Open XPG4 types.
 */

typedef unsigned long	msgqnum_t;
typedef unsigned long	msglen_t;

/*
**	IPC Message Facility.
*/

#include <sys/ipc.h>

/*
**	Implementation Constants.
*/
#ifdef _KERNEL
#define	PMSG	(PZERO + 2)	/* message facility sleep priority */
#endif /* _KERNEL */

/*
**	Permission Definitions.
*/

#define	MSG_R	0400	/* read permission */
#define	MSG_W	0200	/* write permission */

/*
**	Message Operation Flags.
*/

#define	MSG_NOERROR	010000	/* no error if big message */

/*
**	Structure Definitions.
*/

/*
**	There is one msg queue id data structure for each q in the system.
*/

struct msqid_ds {
	struct ipc_perm	msg_perm;	/* operation permission struct */
	struct msg	*msg_first;	/* ptr to first message on q */
	struct msg	*msg_last;	/* ptr to last message on q */
	ulong_t		msg_cbytes;	/* current # bytes on q */
	msgqnum_t	msg_qnum;	/* # of messages on q */
	msglen_t	msg_qbytes;	/* max # of bytes on q */
	pid_t		msg_lspid;	/* pid of last msgsnd */
	pid_t		msg_lrpid;	/* pid of last msgrcv */
	time_t		msg_stime;	/* last msgsnd time */
	long		msg_pad1;	/* reserved for time_t expansion */
	time_t		msg_rtime;	/* last msgrcv time */
	long		msg_pad2;	/* time_t expansion */
	time_t		msg_ctime;	/* last change time */
	long		msg_pad3;	/* last change time */
	long		msg_pad4[4];		/* reserve area */
};

/*
**	User message buffer template for msgsnd and msgrecv system calls.
*/

#if _NO_XOPEN4
struct msgbuf {
	long	mtype;		/* message type */
	char	mtext[1];	/* message text */
};
#endif	/* _NO_XOPEN4 */

/*
**	Message information structure.
*/

struct msginfo {
	int	msgmax,	/* max message size */
		msgmnb,	/* max # bytes on queue */
		msgmni,	/* # of message queue identifiers */
		msgssz,	/* msg segment size (should be word size multiple) */
		msgtql;	/* # of system message headers */
	ushort_t msgseg;   /* # of msg segments, lower half of the value */
	ushort_t msgseg_h; /* # of msg segments, upper half of the value */
			/* Structure change to accommodate increase in 
			 * max msgseg size, maximum is 33554431. 
			 * The change is done this way to preserve binary
			 * compatibility, with "old" programs only seeing
			 * the lower 16 bits.
			 */
};

#ifdef _KERNEL
/*	Each message queue is locked with its own semaphore,
**	and has its own readers/writers waiting semaphore.
**	We cannot add anything to the msqid_ds structure since
**	this is used in user programs and any change would break
**	object file compatibility.  Therefore, we allocate a
**	parallel array, msglock, which contains the message queue
**	locks and semaphores. The array is defined in the msg master
**	file. The following macro takes a pointer to the message
**	queue and returns its corresponding semaphore structure entry.
**/

#define	MSGADDR(X)	msgsem[X]

struct msgsem {
	mutex_t	msg_lock;	/* lock for msqid_ds structure */
	sv_t	msg_rwait;	/* to wait to read a msg */
	sv_t	msg_wwait;	/* to wait to write a msg */
};

/*
**	There is one msg structure for each message that may be in the system.
*/

struct msg {
	struct msg	*msg_next;	/* ptr to next message on q */
	long		msg_type;	/* message type */
	int		msg_ts;		/* message text size */
	caddr_t		msg_spot;	/* message text map address */
};

extern int msgconv(int, struct msqid_ds **);

/*
 * Argument vectors for the various flavors of msgsys().
 */

#define	MSGGET	0
#define	MSGCTL	1
#define	MSGRCV	2
#define	MSGSND	3

struct msgsysa {
	sysarg_t opcode;
};

extern int msgsys(struct msgsysa *, union rval *);

/*
**	msgget - Msgget system call.
*/
struct msggeta {
	sysarg_t opcode;
	sysarg_t key;
	sysarg_t msgflg;
};

/*
**	msgctl - Msgctl system call.
*/
struct msgctla {
	sysarg_t opcode;
	sysarg_t msgid;
	sysarg_t cmd;
	struct msqid_ds	*buf;
};

extern int msgctl(struct msgctla *, union rval *, int);

/*
**	msgsnd - Msgsnd system call.
*/
struct msgsnda {
	sysarg_t	opcode;
	sysarg_t	msqid;
	struct msgbuf	*msgp;
	sysarg_t	msgsz;
	sysarg_t	msgflg;
};

/*
**	msgrcv - Msgrcv system call.
*/
struct msgrcva {
	sysarg_t	opcode;
	sysarg_t	msqid;
	struct msgbuf	*msgp;
	sysarg_t	msgsz;
	long		msgtyp;
	sysarg_t	msgflg;
};
#endif /* _KERNEL */

#ifndef _KERNEL

#if _NO_XOPEN4
extern int	msgctl(int, int, ...);
#else	/* _XOPEN4 */
extern int	msgctl(int, int, struct msqid_ds *);
#endif	/* _NO_XOPEN4 */

extern int	msgget(key_t, int);
extern int	msgrcv(int, void *, size_t, long, int);
extern int	msgsnd(int, const void *, size_t, int);

#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_MSG_H */
