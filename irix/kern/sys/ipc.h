#ifndef __SYS_IPC_H__
#define __SYS_IPC_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <standards.h>

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/ipc.h	10.2"*/
#ident	"$Revision: 3.30 $"
/* Common IPC Access Structure */

#include <sys/types.h>

struct ipc_perm {
	uid_t	uid;	/* owner's user id */
	gid_t	gid;	/* owner's group id */
	uid_t	cuid;	/* creator's user id */
	gid_t	cgid;	/* creator's group id */
	mode_t	mode;	/* access modes */
	ulong_t	seq;	/* slot usage sequence number */
	key_t	key;	/* key */
	long	ipc_pad[4];	/* reserve area */
};

/* Common IPC Definitions. */
/* Mode bits. */
#define	IPC_ALLOC	0100000		/* entry currently allocated */
#define	IPC_CREAT	0001000		/* create entry if key doesn't exist */
#define	IPC_EXCL	0002000		/* fail if key exists */
#define	IPC_NOWAIT	0004000		/* error if request must wait */
#define	IPC_AUTORMID	0200000		/* auto-RMID when creator exits */
					/* (auto-RMID is for shm only) */

#define	IPC_READ	0000400		/* read access */
#define	IPC_WRITE	0000200		/* write access */

/* Keys. */
#define	IPC_PRIVATE	(key_t)0	/* private key */

/* Control Commands. */
#define	IPC_RMID	10	/* remove identifier */
#define	IPC_SET		11	/* set options */
#define	IPC_STAT	12	/* get options */

#ifdef _KERNEL

#include <sys/cred.h>
#include <sys/sema.h>

extern int ipcaccess(struct ipc_perm *, struct cred *, ushort_t);

extern int ipcget(
		key_t, int,
		int (*)(int, struct ipc_perm **, mutex_t **),
		int *, struct cred *, int *);

void ipc_init(struct ipc_perm *, key_t, int, struct cred *);

#else	/* !_KERNEL */

#if _XOPEN4UX
extern key_t 	ftok(const char *, int);
#endif	/* _XOPEN4UX */

#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif
#endif /* !__SYS_IPC_H__ */
