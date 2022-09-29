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
#ifndef _KSYS_BEHAVIOR_H
#define _KSYS_BEHAVIOR_H

#ident	"$Revision: 1.47 $"

/*
 * Header file used to associate behaviors with virtualized objects.
 * 
 * A virtualized object is an internal, virtualized representation of 
 * OS entities such as persistent files, processes, or sockets.  Examples
 * of virtualized objects include vnodes, vprocs, and vsockets.  Often
 * a virtualized object is referred to simply as an "object."
 *
 * A behavior is essentially an implementation layer associated with 
 * an object.  Multiple behaviors for an object are chained together,
 * the order of chaining determining the order of invocation.  Each 
 * behavior of a given object implements the same set of interfaces 
 * (e.g., the VOP interfaces).
 *
 * Behaviors may be dynamically inserted into an object's behavior chain,
 * such that the addition is transparent to consumers that already have 
 * references to the object.  Typically, a given behavior will be inserted
 * at a particular location in the behavior chain.  Insertion of new 
 * behaviors is synchronized with operations-in-progress (oip's) so that 
 * the oip's always see a consistent view of the chain.
 *
 * The term "interpostion" is used to refer to the act of inserting
 * a behavior such that it interposes on (i.e., is inserted in front 
 * of) a particular other behavior.  A key example of this is when a
 * system implementing distributed single system image wishes to 
 * interpose a distribution layer (providing distributed coherency)
 * in front of an object that is otherwise only accessed locally.
 *
 * Note that the traditional vnode/inode combination is simply a virtualized 
 * object that has exactly one associated behavior.
 *
 * BHV_SYNCH is used to control whether insertion of behaviors must be
 * synchronized with ops-in-progress.  It adds some extra overhead to
 * virtual op invocations (VOP, VPOP, etc.) by requiring a mrlock be taken.
 * 
 * BHV_SYNCH is needed whenever there's at least one class of object in 
 * the system for which:
 * 1) multiple behaviors for a given object are supported, 
 * -- AND --
 * 2a) insertion of a new behavior can happen dynamically at any time during 
 *     the life of an active object, 
 * 	-- AND -- 
 * 	3a) insertion of a new behavior needs to synchronize with existing 
 *    	    ops-in-progress.
 * 	-- OR --
 * 	3b) multiple different behaviors can be dynamically inserted at 
 *	    any time during the life of an active object
 * 	-- OR --
 * 	3c) removal of a behavior can occur at any time during the life of
 *	    an active object.
 * -- OR --
 * 2b) removal of a behavior can occur at any time during the life of an
 *     active object
 * 	-- AND -- 
 *     3a) removal of a behavior needs to synchronize with existing
 *	   ops-in-progress
 *
 * Note that modifying the behavior chain due to insertion of a new behavior
 * is done atomically w.r.t. ops-in-progress.  This implies that even if 
 * BHV_SYNCH is off, a racing op-in-progress will always see a consistent 
 * view of the chain.  However, if the correctness of an op-in-progress is 
 * dependent on whether or not a new behavior is inserted while it is 
 * executing, then BHV_SYNCH should be on.  The same applies to removal
 * of an existing behavior.
 *
 * For most configurations, BHV_SYNCH will be off.  It's on, for example,
 * with Cellular Irix.
 *
 * It's also possible to have behavior synchronization be on a 
 * per-object type basis.  This is done by having those objects use
 * the bhv2_head_t type and associated bhv2_* interfaces.  In this case,
 * the synchronization is unconditional (in contrast to the bhv_head_t
 * type and associated bhv_* interfaces for which synchronization is 
 * conditional on BHV_SYNCH).
 */

#include <sys/sema.h>
#include <sys/debug.h>

/*
 * Behavior head.  Head of the chain of behaviors.
 * Contained within each virtualized object data structure.
 */
struct bhv_ucq;
typedef struct bhv_head {
	struct bhv_desc	*bh_first;	/* first behavior in chain */
#if BHV_SYNCH
	mrlock_t	bh_mrlock;   	/* lock for ops-in-progress synch. */
	struct bhv_ucq	*bh_ucallout;	/* deferred update callout queue */
	lock_t		bh_ucqlock;	/* update callout queue lock */
#elif CELL_PREPARE
        __int64_t       bh_mrlspace;    /* Space for bh access locking */
        void            *bh_cqhspace;   /* Space for callout queue head */ 
        __int64_t       bh_cqlspace;    /* Space for callout queue lock */
#endif 
} bhv_head_t;

/*
 * Behavior head type 2.  Same as a bhv_head_t except the mrlock
 * synchronization is unconditional.
 */
typedef struct bhv2_head {
	struct bhv_desc	*bh2_first;	/* first behavior in chain */
	mrlock_t	bh2_mrlock;   	/* lock for ops-in-progress synch. */
} bhv2_head_t;

/*
 * Behavior descriptor.  Descriptor associated with each behavior.
 * Contained within the behavior's private data structure.
 */
typedef struct bhv_desc {
	void		*bd_pdata;	/* private data for this behavior */
	void		*bd_vobj;	/* virtual object associated with */
	void		*bd_ops;	/* ops for this behavior */
	struct bhv_desc	*bd_next;	/* next behavior in chain */
} bhv_desc_t;

/*
 * Behavior identity field.  A behavior's identity determines the position 
 * where it lives within a behavior chain, and it's always the first field
 * of the behavior's ops vector. The optional id field further identifies the
 * subsystem responsible for the behavior.
 */
typedef struct bhv_identity {
	ushort	bi_id;		/* owning subsystem id */
	ushort	bi_position;	/* position in chain */
} bhv_identity_t;
typedef bhv_identity_t bhv_position_t;
#ifdef CELL_IRIX
#define BHV_IDENTITY_INIT(id,pos)	{id, pos}
#else
#define BHV_IDENTITY_INIT(id,pos)	{0, pos}
#endif
#define BHV_IDENTITY_INIT_POSITION(pos)	BHV_IDENTITY_INIT(0, pos)


/*
 * Define boundaries of position values.  
 */
#define BHV_POSITION_INVALID	0	/* invalid position number */
#define BHV_POSITION_BASE	1	/* base (last) implementation layer */
#define BHV_POSITION_TOP	64	/* top (first) implementation layer */

/*
 * Plumbing macros.
 */
#define BHV_HEAD_FIRST(bhp)	(ASSERT((bhp)->bh_first), (bhp)->bh_first)
#define BHV2_HEAD_FIRST(bhp)	(ASSERT((bhp)->bh2_first), (bhp)->bh2_first)
#define BHV_NEXT(bdp)		(ASSERT((bdp)->bd_next), (bdp)->bd_next)
#define BHV_VOBJ(bdp)		(ASSERT((bdp)->bd_vobj), (bdp)->bd_vobj)
#define BHV_VOBJNULL(bdp)	((bdp)->bd_vobj)
#define BHV_PDATA(bdp)		(bdp)->bd_pdata
#define BHV_OPS(bdp)		(bdp)->bd_ops
#define BHV_IDENTITY(bdp)	((bhv_identity_t *)(bdp)->bd_ops)
#define BHV_POSITION(bdp)	(BHV_IDENTITY(bdp)->bi_position)

/*
 * Different read-locking routines are used on CELL & non-CELL systems.
 *   (write-locking routines are the same)
 */
#ifdef CELL
#define BHV_MRACCESS(l)		mrbhaccess(l)
#define BHV_MRACCUNLOCK(l)	mrbhaccunlock(l)
#define BHV_MRUPDATE(l)		mrupdate(l)
#define BHV_MRUNLOCK(l)		mrunlock(l)
#define BHV_MRDEMOTE(l)		mrbhdemote(l)
#define BHV_MRTRYACCESS(l)	mrtryaccess(l)
#define BHV_MRTRYPROMOTE(l)	mrtrypromote(l)
#else /* CELL */
#define BHV_MRACCESS(l)		mraccess(l)
#define BHV_MRACCUNLOCK(l)	mraccunlock(l)
#define BHV_MRUPDATE(l)		mrupdate(l)
#define BHV_MRUNLOCK(l)		mrunlock(l)
#define BHV_MRDEMOTE(l)		mrdemote(l)
#define BHV_MRTRYACCESS(l)	mrtryaccess(l)
#define BHV_MRTRYPROMOTE(l)	mrtrypromote(l)
#endif /* CELL */

#if CELL && BLALOG
#define BHV_BLA_PUSH(mrp,rw)		bla_push(mrp,rw)
#define BHV_BLA_POP(mrp,rw)		bla_pop(mrp,rw)

extern void	bla_push(mrlock_t *mrp, int rw);
extern void	bla_pop(mrlock_t *mrp, int rw);

#else /* BLALOG */

#define BHV_BLA_PUSH(bhp,rw)
#define BHV_BLA_POP(bhp,rw)

#endif /* BLALOG */

#if BHV_SYNCH
/* 
 * Behavior chain read lock - typically used by ops-in-progress to 
 * synchronize with behavior insertion and object migration.
 */
#define BHV_READ_LOCK(bhp)				\
{							\
        BHV_MRACCESS(&(bhp)->bh_mrlock);		\
	BHV_BLA_PUSH(&(bhp)->bh_mrlock,0);		\
}
#define BHV_READ_UNLOCK(bhp)				\
{							\
	BHV_BLA_POP(&(bhp)->bh_mrlock,0);		\
	if (((bhp)->bh_ucallout != NULL) &&		\
	    BHV_MRTRYPROMOTE(&(bhp)->bh_mrlock)) {	\
		BHV_BLA_PUSH(&(bhp)->bh_mrlock,1);	\
		bhv_do_ucallout(bhp);			\
		BHV_BLA_POP(&(bhp)->bh_mrlock,1);	\
        	BHV_MRUNLOCK(&(bhp)->bh_mrlock);	\
	} else {					\
        	BHV_MRACCUNLOCK(&(bhp)->bh_mrlock);	\
	}						\
}

/* 
 * Behavior chain write lock - typically used by behavior insertion and 
 * object migration to synchronize with ops-in-progress.
 */
#define BHV_WRITE_LOCK(bhp)				\
{							\
        BHV_MRUPDATE(&(bhp)->bh_mrlock);		\
	BHV_BLA_PUSH(&(bhp)->bh_mrlock,1);		\
}
#define BHV_WRITE_UNLOCK(bhp)				\
{							\
	BHV_BLA_POP(&(bhp)->bh_mrlock,1);		\
        BHV_MRUNLOCK(&(bhp)->bh_mrlock);		\
}
#define BHV_WRITE_TO_READ(bhp)				\
{							\
	BHV_BLA_POP(&(bhp)->bh_mrlock,1);		\
        BHV_MRDEMOTE(&(bhp)->bh_mrlock);		\
	BHV_BLA_PUSH(&(bhp)->bh_mrlock,0);		\
}

/*
 * Request a callout to be made ((*func)(bhp, arg1, arg2, arg3, arg4))
 * with the behavior chain update locked.
 * The return value from this registration is:
 * 	-1	if the behavior is currently update-locked
 *	0	if the callout has been made
 *	>0	is the number of callout requests now pending.
 * When the update lock is obtained, all queued callouts are made.
 * Note that the callouts will occur in the context of the last
 * accessor unlocking the behavior.
 */
typedef void bhv_ucallout_t(bhv_head_t *bhp, void *, void *, void *, void *);
#define BHV_WRITE_LOCK_CALLOUT(bhp, func, arg1, arg2, arg3, arg4)	\
	bhv_queue_ucallout(bhp, func, arg1, arg2, arg3, arg4)
	
#define BHV_IS_READ_LOCKED(bhp) 	mrislocked_access(&(bhp)->bh_mrlock)
#define BHV_NOT_READ_LOCKED(bhp) 	!mrislocked_access(&(bhp)->bh_mrlock)
#define BHV_IS_WRITE_LOCKED(bhp) 	mrislocked_update(&(bhp)->bh_mrlock)
#define BHV_NOT_WRITE_LOCKED(bhp) 	!mrislocked_update(&(bhp)->bh_mrlock)
#define bhv_lock_init(bhp,name)		mrbhinit(&(bhp)->bh_mrlock, (name))
#define bhv_lock_free(bhp)		mrfree(&(bhp)->bh_mrlock)
#define bhv_ucq_init(bhp,name)		(bhp)->bh_ucallout = NULL; 	\
					spinlock_init(&(bhp)->bh_ucqlock,(name))
#define bhv_ucq_free(bhp)		ASSERT((bhp)->bh_ucallout == NULL); \
					spinlock_destroy(&(bhp)->bh_ucqlock)

#else	/* BHV_SYNCH */

#if BHV_PREPARE
#define BHV_READ_LOCK(bhp)              bhv_read_lock(&(bhp)->bh_mrlspace);
#define BHV_READ_UNLOCK(bhp)		bhv_read_unlock(&(bhp)->bh_mrlspace);
#define BHV_WRITE_LOCK(bhp)		bhv_write_lock(&(bhp)->bh_mrlspace);
#define BHV_WRITE_UNLOCK(bhp)		bhv_write_unlock(&(bhp)->bh_mrlspace);

extern void bhv_read_lock(__int64_t *);
extern void bhv_read_unlock(__int64_t *);
extern void bhv_write_lock(__int64_t *);
extern void bhv_write_unlock(__int64_t *);
#else	/* BHV_PREPARE */
#define BHV_READ_LOCK(bhp)
#define BHV_READ_UNLOCK(bhp)
#define BHV_WRITE_LOCK(bhp)
#define BHV_WRITE_UNLOCK(bhp)
#endif	/* BHV_PREPARE */
#define BHV_WRITE_TO_READ(bhp)
#define BHV_WRITE_LOCK_CALLOUT(bhp, func, arg)
#define BHV_IS_READ_LOCKED(bhp)		1
#define BHV_NOT_READ_LOCKED(bhp)	1
#define BHV_IS_WRITE_LOCKED(bhp)	1
#define BHV_NOT_WRITE_LOCKED(bhp)	1
#define bhv_lock_init(bhp,name)
#define bhv_lock_free(bhp)
#define bhv_ucq_init(bhp,name)
#define bhv_ucq_free(bhp)

#endif /* BHV_SYNCH */

#if CELL_PREPARE
extern void bhv_head_init(bhv_head_t *, char *);
extern void bhv_head_destroy(bhv_head_t *);
extern void bhv_head_reinit(bhv_head_t *);
extern void bhv_insert_initial(bhv_head_t *, bhv_desc_t *);
#else	/* CELL_PREPARE */
/* Initialize an object's behavior chain head. */
#define bhv_head_init(bhp,name)			  	\
{						  	\
	(bhp)->bh_first = NULL;			 	\
        bhv_lock_init(bhp,name);	 	  	\
        bhv_ucq_init(bhp,name);		 	  	\
}

/*
 * Teardown the state associated with a behavior head.
 * Behavior chain must be empty.
 */
#define bhv_head_destroy(bhp)				\
{							\
        ASSERT((bhp)->bh_first == NULL);		\
	if(BHV_IS_READ_LOCKED(bhp))			\
		BHV_READ_UNLOCK(bhp);			\
	bhv_lock_free(bhp);				\
	bhv_ucq_free(bhp);				\
}

/* Same as bhv_head_init, but for behavior heads being reinitialized. */
#define bhv_head_reinit(bhp)				\
{							\
        ASSERT((bhp)->bh_first == NULL);		\
	ASSERT(BHV_NOT_READ_LOCKED(bhp));	 	\
	ASSERT(BHV_NOT_WRITE_LOCKED(bhp));		\
}

/*
 * Insert an initial behavior.  Optimized form of bhv_insert()
 * to be used when inserting the initial behavior in the chain.
 * No locking is done.
 */
#define bhv_insert_initial(bhp, bdp)			\
{							\
        ASSERT((bhp)->bh_first == NULL);		\
        ASSERT(BHV_POSITION(bdp) >= BHV_POSITION_BASE);	\
        ASSERT(BHV_POSITION(bdp) <= BHV_POSITION_TOP);	\
        (bhp)->bh_first = bdp;				\
}
#endif	/* CELL_PREPARE */

/*
 * Initialize a new behavior descriptor.
 * Arguments:
 * 	bdp - pointer to behavior descriptor
 *	pdata - pointer to behavior's private data
 *	vobj - pointer to associated virtual object
 *	ops - pointer to ops for this behavior
 */
#define bhv_desc_init(bdp, pdata, vobj, ops)		\
{							\
	(bdp)->bd_pdata = pdata;			\
	(bdp)->bd_vobj = vobj;				\
	(bdp)->bd_ops = ops;				\
	(bdp)->bd_next = NULL;				\
}

/*
 * Remove a behavior descriptor from a behavior chain.
 */
#define bhv_remove(bhp, bdp)				\
{							\
	if ((bhp)->bh_first == (bdp)) {			\
               /*					\
	        * Remove from front of chain.		\
                * Atomic wrt oip's.			\
		*/					\
	       (bhp)->bh_first = (bdp)->bd_next;	\
        } else {					\
	       /* remove from non-front of chain */	\
	       bhv_remove_not_first(bhp, bdp);		\
	}						\
}

/*
 * Support for the bhv2_head_t.  
 */
#define BHV2_READ_LOCK(bhp)				\
{							\
        BHV_MRACCESS(&(bhp)->bh2_mrlock);		\
	BHV_BLA_PUSH(&(bhp)->bh2_mrlock,0);		\
}
#define BHV2_READ_UNLOCK(bhp)				\
{							\
	BHV_BLA_POP(&(bhp)->bh2_mrlock,0);		\
        BHV_MRACCUNLOCK(&(bhp)->bh2_mrlock);		\
}
#define BHV2_WRITE_LOCK(bhp)				\
{							\
        BHV_MRUPDATE(&(bhp)->bh2_mrlock);		\
	BHV_BLA_PUSH(&(bhp)->bh2_mrlock,1);		\
}
#define BHV2_WRITE_UNLOCK(bhp)				\
{							\
	BHV_BLA_POP(&(bhp)->bh2_mrlock,1);		\
        BHV_MRUNLOCK(&(bhp)->bh2_mrlock);		\
}
#define BHV2_WRITE_TO_READ(bhp)				\
{							\
	BHV_BLA_POP(&(bhp)->bh2_mrlock,1);		\
        BHV_MRDEMOTE(&(bhp)->bh2_mrlock);		\
	BHV_BLA_PUSH(&(bhp)->bh2_mrlock,0);		\
}

#define BHV2_IS_READ_LOCKED(bhp) 	mrislocked_access(&(bhp)->bh2_mrlock)
#define BHV2_NOT_READ_LOCKED(bhp) 	!mrislocked_access(&(bhp)->bh2_mrlock)
#define BHV2_IS_WRITE_LOCKED(bhp) 	mrislocked_update(&(bhp)->bh2_mrlock)
#define BHV2_NOT_WRITE_LOCKED(bhp) 	!mrislocked_update(&(bhp)->bh2_mrlock)
#define bhv2_lock_init(bhp,name)	mrbhinit(&(bhp)->bh2_mrlock, (name))
#define bhv2_lock_free(bhp)		mrfree(&(bhp)->bh2_mrlock)

#define bhv2_head_init(bhp,name)		  	\
{						  	\
	(bhp)->bh2_first = NULL;		 	\
        bhv2_lock_init(bhp,name);	 	  	\
}
#define bhv2_head_destroy(bhp)				\
{							\
        ASSERT((bhp)->bh2_first == NULL);		\
	if(BHV2_IS_READ_LOCKED(bhp))			\
		BHV2_READ_UNLOCK(bhp);			\
	bhv2_lock_free(bhp);				\
}
#define bhv2_head_reinit(bhp)				\
{							\
        ASSERT((bhp)->bh2_first == NULL);		\
	ASSERT(BHV2_NOT_READ_LOCKED(bhp));	 	\
	ASSERT(BHV2_NOT_WRITE_LOCKED(bhp));		\
}
#define bhv2_insert_initial(bhp, bdp)			\
{							\
        ASSERT((bhp)->bh2_first == NULL);		\
        ASSERT(BHV_POSITION(bdp) >= BHV_POSITION_BASE);	\
        ASSERT(BHV_POSITION(bdp) <= BHV_POSITION_TOP);	\
        (bhp)->bh2_first = bdp;				\
}
#define bhv2_remove(bhp, bdp)				\
{							\
	if ((bhp)->bh2_first == (bdp)) {		\
               /*					\
	        * Remove from front of chain.		\
                * Atomic wrt oip's.			\
		*/					\
	       (bhp)->bh2_first = (bdp)->bd_next;	\
        } else {					\
	       /* remove from non-front of chain */	\
	       bhv2_remove_not_first(bhp, bdp);		\
	}						\
}

/*
 * Behavior module prototypes.
 */
extern int      	bhv_insert(bhv_head_t *bhp, bhv_desc_t *bdp);
extern void		bhv_remove_not_first(bhv_head_t *bhp, bhv_desc_t *bdp);
extern bhv_desc_t 	*bhv_lookup(bhv_head_t *bhp, void *ops);
extern bhv_desc_t 	*bhv_lookup_unlocked(bhv_head_t *bhp, void *ops);
extern bhv_desc_t	*bhv_base_unlocked(bhv_head_t *bhp);

extern int      	bhv2_insert(bhv2_head_t *bhp, bhv_desc_t *bdp);
extern void		bhv2_remove_not_first(bhv2_head_t *bhp, 
					      bhv_desc_t *bdp);
extern bhv_desc_t 	*bhv2_lookup(bhv2_head_t *bhp, void *ops);
extern bhv_desc_t 	*bhv2_lookup_unlocked(bhv2_head_t *bhp, void *ops);
extern bhv_desc_t	*bhv2_base_unlocked(bhv2_head_t *bhp);

extern void		bhv_global_init(void);
extern struct zone	*bhv_global_zone;

#ifdef BHV_SYNCH
extern void		bhv_do_ucallout(bhv_head_t *bhp);
extern int		bhv_queue_ucallout(bhv_head_t *bhp,
				bhv_ucallout_t *func,
				void *, void *, void *, void *);
#ifdef DEBUG
extern void		bhv_print_ucallout(bhv_head_t *bhp);
#endif
#endif /* BHV_SYNCH */

/*
 * Prototypes for interruptible sleep requests
 * Noop on non-cell kernels.
 */
#ifdef CELL
extern void			bla_isleep(int *);
extern void 			bla_iunsleep(void);
extern uint_t			bla_wait_for_mrlock(mrlock_t *mrp);
extern void			bla_got_mrlock(uint_t rv);
extern void			bla_prevent_starvation(int);
#define bla_curlocksheld()	(private.p_blaptr - (curthreadp)->k_bla.kb_lockp)
#define bla_klocksheld(kt)	((kt)->k_bla.kb_lockpp - (kt)->k_bla.kb_lockp)
#else
#define bla_isleep(s)
#define bla_iunsleep()
#define bla_wait_for_mrlock(mrp)
#define bla_got_mrlock(mrp)
#define bla_curlocksheld()	0
#define bla_klocksheld(kt)	0
#endif /* CELL */

#endif /* _KSYS_BEHAVIOR_H */
