/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_VPROC_PRIVATE_H_
#define	_VPROC_PRIVATE_H_	1

#define VPROC_REFCNT_LOCK_INIT(v,n)	{ \
					spinlock_init(&(v)->vp_refcnt_lock, n);\
					sv_init(&(v)->vp_cond, SV_DEFAULT, \
							"vproc"); \
					}
#define VPROC_REFCNT_LOCK_DESTROY(v)	{ \
					spinlock_destroy(&(v)->vp_refcnt_lock);\
					sv_destroy(&(v)->vp_cond); \
					}
#define	VPROC_NESTED_REFCNT_LOCK(v)	nested_spinlock(&(v)->vp_refcnt_lock)
#define VPROC_REFCNT_LOCK(v)		mutex_spinlock(&(v)->vp_refcnt_lock)
#define VPROC_REFCNT_UNLOCK(v, s)	mutex_spinunlock(&(v)->vp_refcnt_lock,s)
#define	VPROC_REFCNT_WAIT(v, s)		sv_wait(&(v)->vp_cond, PZERO-1, \
						&(v)->vp_refcnt_lock, s);
#define	VPROC_REFCNT_SIGNAL(v)		sv_signal(&(v)->vp_cond);

#define	VPROC_BHV_DS	BHV_POSITION_BASE+1
#define	VPROC_BHV_DC	VPROC_BHV_DS+1

#define BHV_TO_VPROC(bdp)	((struct vproc *)BHV_VOBJ(bdp))

extern struct zone *vproc_zone;

extern vproc_t	*vproc_lookup_remote(pid_t, int);
extern vproc_t	*vproc_lookup_remote_locked(pid_t);
extern void	vproc_bhv_write_lock(vproc_t *);
extern void	vproc_quiesce(vproc_t *);
extern int	vproc_reference(vproc_t *, int);
extern int	vproc_reference_exclusive(vproc_t *, int *);
extern vproc_t	*vproc_create_embryo(pid_t pid);

extern int	remote_procscan(int ((*)(struct proc *, void *, int)), void *);

extern void	idbg_vproc_bhv_print(vproc_t *);

#endif	/* _VPROC_PRIVATE_H_ */
