/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Id: vsession.h,v 1.6 1997/04/16 20:22:58 emb Exp $"

#ifndef _KSYS_VSESSION_H_
#define _KSYS_VSESSION_H_

#include <sys/types.h>
#include <ksys/behavior.h>
#include <ksys/kqueue.h>
#include <ksys/vpgrp.h>

/*
 * Vsession structure.
 */
typedef struct vsession {
	kqueue_t	vs_queue;	/* hash queue link */
	pid_t		vs_sid;		/* vsession session ID */
	int		vs_refcnt;	/* vsession reference count */
	lock_t		vs_refcnt_lock; /* lock for ref count */
	bhv_head_t	vs_bhvh;	/* Behavior head */
} vsession_t;

#define VSESSION_NULL	((vsession_t *)0)

#define	VSESSION_LOOKUP(sid)		vsession_lookup(sid)
#define	VSESSION_CREATE(sid)		vsession_create(sid)
#define VSESSION_HOLD(v)		vsession_ref(v)
#define VSESSION_RELE(v)		vsession_unref(v)

extern void		vsession_init(void);
extern vsession_t	*vsession_create(pid_t);
extern vsession_t	*vsession_lookup(pid_t);
extern void		vsession_ref(vsession_t *);
extern void		vsession_unref(vsession_t *);

struct vnode;

typedef struct session_ops {
	bhv_position_t	vs_position;	/* position within behavior chain */

	void (*session_join)
		(bhv_desc_t *b,		/* behavior */
		 vpgrp_t *vpg);		/* joining vpgrp */

	void (*session_leave)
		(bhv_desc_t *b,		/* behavior */
		 vpgrp_t *vpg);		/* leaving vpgrp */

	int (*session_ctty_alloc)
		(bhv_desc_t *b,		/* behavior */
		 struct vnode *vp,	/* tty vnode */
		 void (*ttycall)(void *, int),	/* tty driver callback */
		 void *ttycallarg);	/* argument to callback function */

	void (*session_ctty_dealloc)
		(bhv_desc_t *b,		/* behavior */
		 int mode);		/* ttyclose or hangup */

	int (*session_ctty_hold)
		(bhv_desc_t *b,		/* behavior */
		 struct vnode **vnpp);	/* return tty vnode */

	void (*session_ctty_rele)
		(bhv_desc_t *b,		/* behavior */
		 struct vnode *vnp);	/* tty vnode */

	void (*session_ctty_wait)
		(bhv_desc_t *b,		/* behavior */
		 int mode);		/* ttyclose or hangup */

	void (*session_ctty_devnum)
		(bhv_desc_t *b,		/* behavior */
		 dev_t *devnump);	/* return tty device number */

	void (*session_membership)
		(bhv_desc_t *b,		/* behavior */
		 int memcnt);		/* num member to be (only 0/1 used) */

	void (*session_destroy)
		(bhv_desc_t *b);	/* behavior */

} session_ops_t;


/*
 * Flags for tty callback routine specified in VSESSION_CTTY_ALLOC()
 */
#define	TTYCB_GETMONITOR	0x0001	/* get tty monitor */
#define	TTYCB_RELMONITOR	0x0002	/* release tty monitor */
#define	TTYCB_SIGFGPGRP		0x0004	/* signal foreground process group */
#define	TTYCB_RELSESS		0x0008	/* release tty session */

/*
 * Arguments to VSESSION_CTTY_DEALLOC
 */
#define	SESSTTY_CLOSE	0	/* tty close */
#define	SESSTTY_HANGUP	1	/* session leader exit or vhangup */

/*
 * Virtual Operations
 */
#define VSESSION_BHV_HEAD(v)	(&(v)->vs_bhvh)
#define VSESSION_BHV_FIRST(v)	BHV_HEAD_FIRST(VSESSION_BHV_HEAD(v))
#define	vs_ops	vs_fbhv->bd_ops		/* ops for 1st behavior */
#define _VSESSOP_(op, v)	\
	(*((session_ops_t *)(BHV_OPS(VSESSION_BHV_FIRST(v))))->op)

#define VSESSION_JOIN(v, p) \
	{ \
	BHV_READ_LOCK(VSESSION_BHV_HEAD(v)); \
	_VSESSOP_(session_join, v) (VSESSION_BHV_FIRST(v), (p)); \
	BHV_READ_UNLOCK(VSESSION_BHV_HEAD(v)); \
	}

#define VSESSION_LEAVE(v, p) \
	{ \
	BHV_READ_LOCK(VSESSION_BHV_HEAD(v)); \
	_VSESSOP_(session_leave, v) (VSESSION_BHV_FIRST(v), (p)); \
	BHV_READ_UNLOCK(VSESSION_BHV_HEAD(v)); \
	}

#define VSESSION_CTTY_ALLOC(v, vp, cb, cbarg, ret) \
	{ \
	BHV_READ_LOCK(VSESSION_BHV_HEAD(v)); \
	ret = _VSESSOP_(session_ctty_alloc, v) \
			(VSESSION_BHV_FIRST(v), (vp), (cb), (cbarg));\
	BHV_READ_UNLOCK(VSESSION_BHV_HEAD(v)); \
	}

#define VSESSION_CTTY_DEALLOC(v, mode) \
	{ \
	BHV_READ_LOCK(VSESSION_BHV_HEAD(v)); \
	_VSESSOP_(session_ctty_dealloc, v) (VSESSION_BHV_FIRST(v), (mode)); \
	BHV_READ_UNLOCK(VSESSION_BHV_HEAD(v)); \
	}

#define VSESSION_CTTY_HOLD(v, vnpp, ret) \
	{ \
	BHV_READ_LOCK(VSESSION_BHV_HEAD(v)); \
	ret = _VSESSOP_(session_ctty_hold, v) (VSESSION_BHV_FIRST(v), (vnpp)); \
	BHV_READ_UNLOCK(VSESSION_BHV_HEAD(v)); \
	}

#define VSESSION_CTTY_RELE(v, vnp) \
	{ \
	BHV_READ_LOCK(VSESSION_BHV_HEAD(v)); \
	_VSESSOP_(session_ctty_rele, v) (VSESSION_BHV_FIRST(v), (vnp)); \
	BHV_READ_UNLOCK(VSESSION_BHV_HEAD(v)); \
	}

#define VSESSION_CTTY_WAIT(v, ishup) \
	{ \
	BHV_READ_LOCK(VSESSION_BHV_HEAD(v)); \
	_VSESSOP_(session_ctty_wait, v) (VSESSION_BHV_FIRST(v), (ishup)); \
	BHV_READ_UNLOCK(VSESSION_BHV_HEAD(v)); \
	}

#define VSESSION_CTTY_DEVNUM(v, dp) \
	{ \
	BHV_READ_LOCK(VSESSION_BHV_HEAD(v)); \
	_VSESSOP_(session_ctty_devnum, v) (VSESSION_BHV_FIRST(v), (dp)); \
	BHV_READ_UNLOCK(VSESSION_BHV_HEAD(v)); \
	}

#define VSESSION_MEMBERSHIP(v, memcnt) \
	{ \
	BHV_READ_LOCK(VSESSION_BHV_HEAD(v)); \
	_VSESSOP_(session_membership, v) (VSESSION_BHV_FIRST(v), (memcnt)); \
	BHV_READ_UNLOCK(VSESSION_BHV_HEAD(v)); \
	}

#define VSESSION_DESTROY(v) \
	(_VSESSOP_(session_destroy, v) (VSESSION_BHV_FIRST(v)))

#endif	/* _KSYS_VSESSION_H_ */
