/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_FLOCK_H	/* wrapper symbol for kernel use */
#define _FS_FLOCK_H	/* subject to change without notice */

/*#ident	"@(#)uts-3b2:fs/flock.h	1.3"*/
#ident	"$Revision: 3.21 $"

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/sema.h>

#define	INOFLCK		0x0001	/* Vnode is locked when reclock() is called. */
#define	SETFLCK		0x0002	/* Set a file lock. */
#define	SLPFLCK		0x0004	/* Wait if blocked. */
#define	RCMDLCK		0x0008	/* RGETLK/RSETLK/RSETLKW specified */
#define SETBSDFLCK	0x0010	/* Set a Berkeley record lock. */
#define INOFLCK_READ	0x0020  /* vnode locked for read (default is write) */

#define IGN_PID		(-1)	/* ignore epid when cleaning locks */

/* file locking structure (connected to vnode) */

#define l_end 		l_len

#ifndef _KERNEL
#define MAXEND  	017777777777
#else
#define MAXEND  	0x7fffffffffffffffLL
#endif /* _KERNEL */

typedef struct filock {
	struct	flock set;	/* contains type, start, and end */
	union	{
		int wakeflg;	/* for locks sleeping on this one */
		struct {
			struct filock *blocked;
			sysid_t sysid;
			pid_t pid;
			struct vnode *vp;
			u_int cancel;
		} blk;			/* for sleeping locks only */
	}	stat;
	sv_t	wakesem;
	struct	filock *prev;
	struct	filock *next;
} filock_t;

/*
 * struct for passing owner/requestor id
 */
typedef struct flid {
	pid_t	fl_pid;
	sysid_t fl_sysid;
} flid_t;

#ifdef _KERNEL

struct vnode;
struct cred;
extern int	reclock(struct vnode *, struct flock *, int, int, off_t,
				struct cred *);
extern int	convoff(struct vnode *, struct flock *, int, off_t, off_t,
				struct cred *);
extern int	haslocks(struct vnode *, pid_t, sysid_t);
extern int	haslock(struct vnode *, struct flock *);
extern int	remotelocks(struct vnode *);
extern void	release_remote_locks(struct vnode *);
extern int	lock_pending(struct vnode *, struct flock *);
extern int	cancel_lock(struct vnode *, struct flock *);
extern void	cleanlocks(struct vnode *, pid_t, sysid_t);

extern flid_t	*sys_flid;

extern void cancel_blocked_locks(__uint32_t);
#ifdef DEBUG
extern int	locks_pending(pid_t, __uint32_t);
#endif /* DEBUG */

#endif /* _KERNEL */
#endif /* _FS_FLOCK_H */
