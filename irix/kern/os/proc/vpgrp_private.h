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

#ifndef	_OS_PROC_VPGRP_PRIVATE_H_
#define	_OS_PROC_VPGRP_PRIVATE_H_	1

#include <ksys/behavior.h>
#include <ksys/kqueue.h>
#include <ksys/vpgrp.h>
#include <sys/sema.h>
#include <sys/debug.h>
#include <sys/space.h>


/*
 * lookup table - pgrps are hashed by pgid
 */
typedef struct vpgrptable {
	kqueue_t	vpgt_queue;
	mrlock_t	vpgt_lock;
} vpgrptab_t;

extern vpgrptab_t	*vpgrptab;
extern int		vpgrptabsz;
extern struct zone 	*vpgrp_zone;
extern struct zone 	*pgrp_zone;

#define VPGRP_ENTRY(pgid)	&vpgrptab[pgid%vpgrptabsz];

#define VPGRPTAB_LOCK(vq, mode)	{ \
				ASSERT((vq) != 0); \
				mrlock(&(vq)->vpgt_lock, (mode), PZERO); \
				}
#define VPGRPTAB_UNLOCK(vq)	mrunlock(&(vq)->vpgt_lock)

#define VPGRP_IS_VALID(v)	(!kqueue_isnull(&(v)->vpg_queue))

extern void	vpgrp_cell_init(void);
extern vpgrp_t *vpgrp_lookup_local(pid_t);
extern vpgrp_t *vpgrp_create_local(pid_t, pid_t);
extern vpgrp_t *vpgrp_qlookup_locked(kqueue_t *, pid_t);
extern vpgrp_t *vpgrp_alloc(pid_t, pid_t);
extern void	vpgrp_destroy(vpgrp_t *);
extern void	vpgrp_enter(vpgrp_t *);
extern void	vpgrp_remove(vpgrp_t *);
extern void	vpgrp_iterate(void (*)(vpgrp_t *, void *), void *);


#define VPGRP_REFCNT_LOCK_INIT(v)	{ \
					spinlock_init(&(v)->vpg_refcnt_lock, \
							"vprgp"); \
					}
#define VPGRP_REFCNT_LOCK_DESTROY(v)	{ \
					spinlock_destroy(&(v)->vpg_refcnt_lock);\
					}
#define VPGRP_REFCNT_LOCK(v)		mutex_spinlock(&(v)->vpg_refcnt_lock)
#define VPGRP_REFCNT_UNLOCK(v, s)	mutex_spinunlock(&(v)->vpg_refcnt_lock,s)

typedef struct pgrp {
	short		pg_batch; 	/* batch flag */
	short		pg_jclcnt; 	/* num mems contributing job control */
	short		pg_memcnt;	/* number of members (incl joiners) */
	sequence_num_t	pg_sigseq;	/* signal sequence number */
	struct proc	*pg_chain;	/* process group chain */
	mslock_t	pg_lockmode;	/* access mode for pgrp (list) lock */
	mutex_t		pg_lock;	/* lock protecting pgrp (list) */
	bhv_desc_t	pg_bhv;		/* base behavior */
} pgrp_t;

/*
 * Sequence number definition and basic manipulations.
 * (Plagiarized from TCP sequence numbers.)
 */
#define SEQ_CMP(a,b)	(__int32_t)((a)-(b))
#define SEQ_LT(a,b)	(SEQ_CMP(a,b) < 0)
#define SEQ_LEQ(a,b)	(SEQ_CMP(a,b) <= 0)
#define SEQ_GT(a,b)	(SEQ_CMP(a,b) > 0)
#define SEQ_GEQ(a,b)	(SEQ_CMP(a,b) >= 0)

#define	PGRP_SIGSEQ_INC(pg)	(pg)->pg_sigseq++

#define PGRP_SIGSEQ_CMP(pg,seq)	SEQ_CMP((pg)->pg_sigseq,seq)
#define PGRP_SIGSEQ_LT(pg,seq)	SEQ_LT((pg)->pg_sigseq,seq)
#define PGRP_SIGSEQ_LEQ(pg,seq)	SEQ_LEQ((pg)->pg_sigseq,seq)
#define PGRP_SIGSEQ_GT(pg,seq)	SEQ_GT((pg)->pg_sigseq,seq)
#define PGRP_SIGSEQ_GEQ(pg,seq)	SEQ_GEQ((pg)->pg_sigseq,seq)

#define PGRP_LOCKINIT(p, i) \
			init_mutex(&(p)->pg_lock, MUTEX_DEFAULT, "pgrp", i)
#define PGRP_LOCKDESTROY(p)	mutex_destroy(&(p)->pg_lock)
#define PGRP_LOCK(p)		mutex_lock(&(p)->pg_lock, PZERO)
#define PGRP_UNLOCK(p)		mutex_unlock(&(p)->pg_lock)

#define PGRPLIST_READLOCK(p)		msaccess(&(p)->pg_lockmode,&(p)->pg_lock)
#define PGRPLIST_WRITELOCK(p)		msupdate(&(p)->pg_lockmode,&(p)->pg_lock)
#define PGRPLIST_UNLOCK(p)		ASSERT(mutex_mine(&(p)->pg_lock)); \
					msunlock(&(p)->pg_lockmode)
#define PGRPLIST_IS_READLOCKED(p)	is_msaccess(&(p)->pg_lockmode)
#define PGRPLIST_IS_WRITELOCKED(p)	is_msupdate(&(p)->pg_lockmode)

extern void	ppgrp_init(void);
extern void	ppgrp_create(struct vpgrp *);
extern void	ppgrp_destroy(bhv_desc_t *);
extern void	ppgrp_membership(bhv_desc_t *, int, pid_t);
extern void 	ppgrp_getstate(bhv_desc_t *, int *, int *);
extern sequence_num_t ppgrp_sigseq(bhv_desc_t *);
extern void 	ppgrp_set_sigseq(bhv_desc_t *, sequence_num_t);
extern void	pgrp_struct_init(struct pgrp *, pid_t);
extern void 	ppgrp_setattr(bhv_desc_t *, int *);

extern void	ppgrp_getattr(bhv_desc_t *, pid_t *, int *, int *); 
extern int	ppgrp_join_begin(bhv_desc_t *);
extern void	ppgrp_join_end(bhv_desc_t *, struct proc *, int);
extern void	ppgrp_leave(bhv_desc_t *, struct proc *, int);
extern void	ppgrp_detach(bhv_desc_t *, struct proc *);
extern void	ppgrp_linkage(bhv_desc_t *, struct proc *, pid_t, pid_t);
extern void	ppgrp_orphan(bhv_desc_t *, int, int); 
extern void 	ppgrp_anystop(bhv_desc_t *, pid_t, int *);
extern int	ppgrp_sendsig(bhv_desc_t *, int, int,
			      pid_t, struct cred *, struct k_siginfo *);
extern int	ppgrp_sig_wait(bhv_desc_t *, sequence_num_t);

struct cred;
extern int	ppgrp_nice(bhv_desc_t *, int, int *, int *, struct cred *);
extern int	ppgrp_clearbatch(bhv_desc_t *);
extern int	ppgrp_setbatch(bhv_desc_t *);

/*
 * Registration for the positions at which the different vpgrp behaviors 
 * are chained.  When on the same chain, a behavior with a higher position 
 * number is invoked before one with a lower position number.
 */
#define VPGRP_POSITION_PP	BHV_POSITION_BASE	/* chain bottom */
#define VPGRP_POSITION_DS	VPGRP_POSITION_PP+1	/* distr. server */
#define VPGRP_POSITION_DC	VPGRP_POSITION_DS+1	/* distr. client */

#define VPGRP_BHV_HEAD_PTR(v)	(&(v)->vpg_bhvh)
#define VPGRP_TO_FIRST_BHV(v)	(BHV_HEAD_FIRST(&(v)->vpg_bhvh))
#define BHV_TO_VPGRP(bdp)	((struct vpgrp *)BHV_VOBJ(bdp))

#define BHV_TO_PPGRP(bdp) \
	(ASSERT(BHV_OPS(bdp) == &ppgrp_ops), \
	(pgrp_t *)(BHV_PDATA(bdp)))
extern pgrp_ops_t ppgrp_ops;

/*
 * Argument block passed into signal functions.
 */
typedef struct {
	int			sig;		/* IN: signal number */
	int			opts;		/* IN: signal options */
	pid_t			sid;		/* IN: caller's session */
	struct cred		*credp;		/* IN: pointer to creds */
	struct k_siginfo	*infop;		/* IN: pointer to sig info */
	int			error;		/* OUT: error */
} sig_arg_t;

extern void idbg_pgrp(__psint_t);

#ifdef	DEBUG
extern struct ktrace *vpgrp_trace;
extern pid_t vpgrp_trace_id;

#define PGRP_TRACE2(a,b) { \
	if (vpgrp_trace_id == (b) || vpgrp_trace_id == -1) { \
		KTRACE6(vpgrp_trace, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid()); \
	} \
}

#define	PGRP_TRACE4(a,b,c,d) { \
	if (vpgrp_trace_id == (b) || vpgrp_trace_id == -1) { \
		KTRACE8(vpgrp_trace, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d)); \
	} \
}
#define	PGRP_TRACE6(a,b,c,d,e,f) { \
	if (vpgrp_trace_id == (b) || vpgrp_trace_id == -1) { \
		KTRACE10(vpgrp_trace, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f)); \
	} \
}
#define	PGRP_TRACE8(a,b,c,d,e,f,g,h) { \
	if (vpgrp_trace_id == (b) || vpgrp_trace_id == -1) { \
		KTRACE12(vpgrp_trace, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f), \
			(g), (h)); \
	} \
}
#define	PGRP_TRACE10(a,b,c,d,e,f,g,h,i,j) { \
	if (vpgrp_trace_id == (b) || vpgrp_trace_id == -1) { \
		KTRACE14(vpgrp_trace, \
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

#define PGRP_TRACE2(a,b)
#define	PGRP_TRACE4(a,b,c,d)
#define	PGRP_TRACE6(a,b,c,d,e,f)
#define	PGRP_TRACE8(a,b,c,d,e,f,g,h)
#define	PGRP_TRACE10(a,b,c,d,e,f,g,h,i,j)

#endif

#endif	/* _OS_PROC_VPGRP_PRIVATE_H_ */
