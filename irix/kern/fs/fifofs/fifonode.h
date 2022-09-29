/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_FIFOFS_FIFONODE_H	/* wrapper symbol for kernel use */
#define _FS_FIFOFS_FIFONODE_H	/* subject to change without notice */

#ident	"@(#)uts-3b2:fs/fifofs/fifonode.h	1.7"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/vnode.h>	/* for lastclos_t */

/*
 * fifo lock structure. In the case of normal fifo's there is one
 * lock node associated with each fifo node. In the pipe case there
 * is only one lock node which is shared between both fifo nodes.
 */
typedef struct {
	mutex_t		fn_lock;
	unsigned int	fn_refcnt;
} fifo_lock_t;

/*
 * fifo lock/unlock operations
 */
#define fifo_lock(fnp) \
	if (!(fnp->fn_flag & FIFODONTLOCK)) { \
		mutex_lock(&(fnp->fn_lockp->fn_lock), PZERO); \
	}

#define fifo_unlock(fnp) \
	if (!(fnp->fn_flag & FIFODONTLOCK)) { \
		mutex_unlock(&(fnp->fn_lockp->fn_lock)); \
	}
/*
 * Each FIFOFS object is identified by a struct fifonode/vnode pair.
 */
typedef struct fifonode {
	struct fifonode	*fn_mate;	/* the other end of a pipe */
	struct vnode	*fn_realvp;	/* node being shadowed by fifo */
	bhv_desc_t	fn_bhv_desc;	/* FIFO behavior descriptor */
	ushort		fn_ino;		/* node id for pipes */
	short		fn_open;	/* open count of node */
	fifo_lock_t	*fn_lockp;	/* lock node ptr; common for pipes */
	short		fn_numw;	/* # writers */
	short		fn_numr;	/* # readers */
	sv_t		fn_openwait;	/* serialize fifo_open */
	sv_t		fn_rwait;	/* wait for first reader */
	sv_t		fn_wwait;	/* wait for first writer */
	sv_t		fn_empty;	/* procs waiting on empty */
	sv_t		fn_full;	/* procs waiting on full */
	sv_t		fn_ioctl;	/* procs waiting on ioctl */
	struct vnode	*fn_unique;	/* new vnode created by CONNLD */
	ushort		fn_flag;	/* flags as defined below */
	time_t		fn_atime;	/* creation times for pipe */
	time_t		fn_mtime;
	time_t		fn_ctime;
	struct fifonode	*fn_nextp;	/* next link in the linked list */
	struct fifonode	*fn_backp;	/* back link in linked list */
} fifonode_t;

/*
 * Valid flags for fifonodes (16-bits wide)
 */
#define ISPIPE		0x1	/* fifonode is that of a pipe */
#define FIFOLOCK	0x2	/* fifonode is locked */
#define FIFOSEND        0x4	/* file descriptor at stream head of pipe */
#define FIFOWRITE       0x8	/* process is blocked waiting to write */
#define FIFOWANT       0x10	/* a process wants to access the fifonode */
#define FIFOREAD       0x20	/* process is blocked waiting to read */
#define FIFOPASS       0x40	/* CONNLD passed a new vnode in fn_unique */
#define FIFOCLOSING    0x80	/* fifonode in process of being closed */
#define FIFODONTLOCK  0x100	/* do NOT lock the fifo node as it's already */
#define FIFOWCLOSED   0x200	/* last writer has closed */
#define FIFOWOPEN     0x400	/* open in progress */
#define FIFOWCLOSE    0x800	/* close in progress */


/*
 * Macros to convert a vnode to a fifnode, and vice versa.
 */
#define BHVTOF(bdp)	((struct fifonode *)(BHV_PDATA(bdp)))
#define	FTOBHV(fp)	(&((fp)->fn_bhv_desc))
#define FTOV(fp)	BHV_TO_VNODE(FTOBHV(fp))

/*
 * Functions used in multiple places.
 */
#ifdef _KERNEL

struct cred;
struct vnode;
struct queue;

extern ushort fifogetid(void);
extern void fifoclearid(ushort);
extern int fifo_rdchk(struct vnode *);
extern int fifo_stropen(struct vnode **, int, struct cred *);
extern struct fifonode *makepipe(int flag);
extern fifo_lock_t *fifo_cr_lock(long);
extern void fifo_del_lock(fifo_lock_t *);
extern int fifo_close(bhv_desc_t *, int, lastclose_t, struct cred *);
extern void fifo_flush(struct queue *);
extern void fiforemove(struct fifonode *);
extern void fifo_freelocks(struct fifonode *);
extern void fifo_rwlock(bhv_desc_t *, vrwlock_t);
extern void fifo_rwunlock(bhv_desc_t *, vrwlock_t);

extern struct vnodeops fifo_vnodeops;

#endif	/* _KERNEL */

#endif	/* _FS_FIFOFS_FIFONODE_H */
