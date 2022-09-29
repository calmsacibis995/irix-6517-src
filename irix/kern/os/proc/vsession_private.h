/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_OS_PROC_VSESS_PRIVATE_H_
#define	_OS_PROC_VSESS_PRIVATE_H_	1

#ident "$Id: vsession_private.h,v 1.11 1997/06/05 21:55:31 sp Exp $"

#include <ksys/behavior.h>
#include <ksys/kqueue.h>
#include <ksys/vsession.h>
#include <sys/sema.h>
#include <sys/debug.h>

struct vnode;

/*
 * lookup table - sessions are hashed by sid
 */
typedef struct vsessiontable {
	kqueue_t	vsesst_queue;
	mrlock_t	vsesst_lock;
} vsessiontab_t;

extern vsessiontab_t	*vsessiontab;
extern int		vsessiontabsz;
extern struct zone 	*vsession_zone;
extern struct zone 	*psession_zone;

#define VSESSION_ENTRY(sid)	&vsessiontab[(sid)%vsessiontabsz];

#define VSESSIONTAB_LOCK(vq, mode)	{ \
				ASSERT((vq) != 0); \
				mrlock(&(vq)->vsesst_lock, (mode), PZERO); \
				}
#define VSESSIONTAB_UNLOCK(vq)	mrunlock(&(vq)->vsesst_lock)

#define	VSESSION_IS_VALID(v)	(!kqueue_isnull(&(v)->vs_queue))

extern void	vsession_cell_init(void);
extern vsession_t *vsession_create_local(pid_t);
extern vsession_t *vsession_lookup_local(pid_t);
extern vsession_t *vsession_qlookup_locked(kqueue_t *, pid_t);
extern vsession_t *vsession_alloc(pid_t);
extern void	vsession_destroy(vsession_t *);
extern void	vsession_enter(vsession_t *);
extern void	vsession_remove(vsession_t *);

#define VSESSION_REFCNT_LOCK_INIT(v)	{ \
					spinlock_init(&(v)->vs_refcnt_lock, \
							"vsess"); \
					}
#define VSESSION_REFCNT_LOCK_DESTROY(v)	{ \
					spinlock_destroy(&(v)->vs_refcnt_lock);\
					}
#define VSESSION_REFCNT_LOCK(v)		mutex_spinlock(&(v)->vs_refcnt_lock)
#define VSESSION_REFCNT_UNLOCK(v, s)	mutex_spinunlock(&(v)->vs_refcnt_lock,s)

typedef struct psession {
	mutex_t		se_lock;
	sv_t		se_wait;	/* wait for se_ttycnt -> 0 */
	int		se_flag;	/* session flags */
	int		se_memcnt;	/* # pgrps in session (locally) */
	int		se_ttycnt;	/* # outstanding tty ops */
	struct vnode	*se_ttyvp;	/* tty vnode */
	void		(*se_ttycall)(void*,int); /* tty driver callback */
	void		*se_ttycallarg;	/* argument to tty driver callback */
	bhv_desc_t	se_bhv;		/* base behavior */
} psession_t;

#define PSESSION_LOCKINIT(s, i) \
			init_mutex(&(s)->se_lock, MUTEX_DEFAULT, "session", i)
#define PSESSION_LOCKDESTROY(s)		mutex_destroy(&(s)->se_lock)
#define PSESSION_LOCK(s)		mutex_lock(&(s)->se_lock, PZERO)
#define PSESSION_UNLOCK(s)		mutex_unlock(&(s)->se_lock)

/* psession flags (se_flag) */
#define	SETTYCLOSE	0x0001

extern void	psession_init(void);
extern void	psession_create(struct vsession *);
extern void	psession_struct_init(struct psession *, pid_t);
extern void	psession_getstate(bhv_desc_t *, int *);

extern void	psession_join(bhv_desc_t *, vpgrp_t *);
extern void	psession_leave(bhv_desc_t *, vpgrp_t *);
extern int	psession_ctty_alloc(bhv_desc_t *, struct vnode *,
			void (*)(void *,int), void *);
extern void	psession_ctty_dealloc(bhv_desc_t *, int);
extern int	psession_ctty_hold(bhv_desc_t *, struct vnode **);
extern int	psession_ctty_getvnode(bhv_desc_t *, struct vnode **);
extern void	psession_ctty_rele(bhv_desc_t *, struct vnode *);
extern void	psession_ctty_wait(bhv_desc_t *, int);
extern void	psession_ctty_devnum(bhv_desc_t *, dev_t *);
extern void	psession_membership(bhv_desc_t *, int);
extern void	psession_destroy(bhv_desc_t *);

/*
 * Registration for the positions at which the different vsession behaviors 
 * are chained.  When on the same chain, a behavior with a higher position 
 * number is invoked before one with a lower position number.
 */
#define VSESSION_POSITION_PS	BHV_POSITION_BASE	/* chain bottom */
#define VSESSION_POSITION_DC	(VSESSION_POSITION_PS+1)/* distr. client */
#define VSESSION_POSITION_DS	(VSESSION_POSITION_DC+1)/* distr. server */

#define VSESSION_BHV_HEAD_PTR(v)	(&(v)->vs_bhvh)
#define VSESSION_TO_FIRST_BHV(v)	(BHV_HEAD_FIRST(&(v)->vs_bhvh))
#define BHV_TO_VSESSION(bdp)		((struct vsession *)BHV_VOBJ(bdp))

#define	BHV_IS_PSESSION(b)	(BHV_OPS(b) == &psession_ops)

#define BHV_TO_PSESSION(bdp) \
	(ASSERT(BHV_IS_PSESSION(bdp)), \
	(psession_t *)(BHV_PDATA(bdp)))
extern session_ops_t psession_ops;

#ifdef	DEBUG

#include <sys/ktrace.h>

extern struct ktrace *vsession_trace;
extern pid_t vsession_trace_id;

#define SESSION_TRACE2(a,b) { \
	if (vsession_trace_id == (b) || vsession_trace_id == -1) { \
		KTRACE6(vsession_trace, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid()); \
	} \
}

#define	SESSION_TRACE4(a,b,c,d) { \
	if (vsession_trace_id == (b) || vsession_trace_id == -1) { \
		KTRACE8(vsession_trace, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d)); \
	} \
}
#define	SESSION_TRACE6(a,b,c,d,e,f) { \
	if (vsession_trace_id == (b) || vsession_trace_id == -1) { \
		KTRACE10(vsession_trace, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f)); \
	} \
}
#define	SESSION_TRACE8(a,b,c,d,e,f,g,h) { \
	if (vsession_trace_id == (b) || vsession_trace_id == -1) { \
		KTRACE12(vsession_trace, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f), \
			(g), (h)); \
	} \
}
#define	SESSION_TRACE10(a,b,c,d,e,f,g,h,i,j) { \
	if (vsession_trace_id == (b) || vsession_trace_id == -1) { \
		KTRACE14(vsession_trace, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f), \
			(g), (h), \
			(i), (j)); \
	} \
}

#else

#define SESSION_TRACE2(a,b)
#define	SESSION_TRACE4(a,b,c,d)
#define	SESSION_TRACE6(a,b,c,d,e,f)
#define	SESSION_TRACE8(a,b,c,d,e,f,g,h)
#define	SESSION_TRACE10(a,b,c,d,e,f,g,h,i,j)

#endif

#endif	/* _OS_PROC_VSESS_PRIVATE_H_ */
