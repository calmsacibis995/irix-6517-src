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
#ifndef	_VSHM_PRIVATE_H
#define	_VSHM_PRIVATE_H 1
#ident "$Id: vshm_private.h,v 1.27 1998/09/14 16:14:27 jh Exp $"

#include <ksys/vshm.h>
#include <ksys/as.h>
#include <sys/ipc.h>

/*
 * physical shm descriptor
 */
typedef struct pshm {
	struct ipc_perm psm_perm;	/* operation permission struct */
	size_t		psm_segsz;	/* size of segment in bytes */
	as_mohandle_t	psm_mo;		/* ptr to memory object */
	pid_t		psm_lpid;	/* pid of last shmop */
	pid_t		psm_cpid;	/* pid of creator */
	time_t		psm_atime;	/* last shmat time */
	time_t		psm_dtime;	/* last shmdt time */
	time_t		psm_ctime;	/* last change time */
	bhv_desc_t	psm_bd;		/* behavior descriptor */
	struct mac_label *psm_maclabel;	/* MAC label */
	mutex_t		psm_lock;
#ifdef _VCE_AVOIDANCE
	bhv_desc_t	psm_vbd;	/* vce vnode behavior descriptor*/
#endif
} pshm_t;

/*
 * Exported operations on a shared memory descriptor
 */
struct shmid_ds;
extern int pshm_alloc(int, int, size_t, pid_t, struct cred *, vshm_t *);
extern void pshm_free(struct pshm *);
extern void pshm_getmo(bhv_desc_t *, as_mohandle_t *);
extern int pshm_keycheck(bhv_desc_t *, struct cred *, int, size_t);
extern void pshm_setperm(bhv_desc_t *, struct ipc_perm *);
extern int pshm_attach(bhv_desc_t *, int, asid_t, pid_t,
				struct cred *, caddr_t, caddr_t *);
extern void pshm_setdtime(bhv_desc_t *, int, pid_t);
extern int pshm_rmid(bhv_desc_t *, struct cred *, int);
extern int pshm_ipcset(bhv_desc_t *, struct ipc_perm *, struct cred *);
extern int pshm_getstat(bhv_desc_t *, struct cred *,
				struct shmid_ds *, cell_t *);
extern int pshm_mac_access(bhv_desc_t *, struct cred *);
extern void pshm_destroy(bhv_desc_t *);
extern int pshm_push(bhv_desc_t *, cell_t);
extern int getnshm(void);
extern void shm_rmid_callback(void*);

/*
 * Other private parts of shm implementation
 */
extern int	vshm_getstatus(struct shmstat *, struct cred *);

/*
 * Registration for the positions at which the different vshm behaviors 
 * are chained.  When on the same chain, a behavior with a higher position 
 * number is invoked before one with a lower position number.
 */
#define VSHM_POSITION_PSHM	BHV_POSITION_BASE	/* chain bottom */
#define VSHM_POSITION_DC	BHV_POSITION_BASE+1	/* distr. client */
#define VSHM_POSITION_DS	BHV_POSITION_BASE+2	/* distr. server */

#define VSHM_BHV_HEAD_PTR(v)	(&(v)->vsm_bh)
#define VSHM_TO_FIRST_BHV(v)	(BHV_HEAD_FIRST(&(v)->vsm_bh))
#define BHV_TO_VSHM(bdp)	((struct vshm *)BHV_VOBJ(bdp))

#define BHV_TO_PSHM(bdp) \
	(ASSERT(BHV_OPS(bdp) == &pshm_ops), \
	(pshm_t *)(BHV_PDATA(bdp)))
#define BHV_TO_DCSHM(bdp) \
	(ASSERT(BHV_OPS(bdp) == &dcshm_ops), \
	(dcshm_t *)(BHV_PDATA(bdp)))
#define BHV_TO_DSSHM(bdp) \
	(ASSERT(BHV_OPS(bdp) == &dsshm_ops), \
	(dsshm_t *)(BHV_PDATA(bdp)))

extern shmops_t dsshm_ops;
extern shmops_t dcshm_ops;
extern shmops_t pshm_ops;

#if DEBUG
#include <ksys/vproc.h>
#include <sys/ktrace.h>
#include <sys/pda.h>
#include <sys/atomic_ops.h>
extern int vshm_count;
#define ADDVSHM()	atomicAddInt(&vshm_count, 1)
#define DELETEVSHM()	atomicAddInt(&vshm_count, -1)

extern struct ktrace *vshm_trace;
extern int	vshm_trace_id;
extern void	idbg_vshm_bhv_print(vshm_t *);
extern void	idbg_pshm_print(pshm_t *);

#define	VSHM_TRACE2(a,b) { \
	if (vshm_trace_id == (b) || vshm_trace_id == -1) { \
		KTRACE4(vshm_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid()); \
	} \
}
#define	VSHM_TRACE4(a,b,c,d) { \
	if (vshm_trace_id == (b) || vshm_trace_id == -1) { \
		KTRACE6(vshm_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d)); \
	} \
}
#define	VSHM_TRACE6(a,b,c,d,e,f) { \
	if (vshm_trace_id == (b) || vshm_trace_id == -1) { \
		 KTRACE8(vshm_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f)); \
	} \
}
#define	VSHM_TRACE8(a,b,c,d,e,f,g,h) { \
	if (vshm_trace_id == (b) || vshm_trace_id == -1) { \
		KTRACE10(vshm_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f), \
			(g), (void *)(__psint_t)(h)); \
	} \
}
#define	VSHM_TRACE10(a,b,c,d,e,f,g,h,i,j) { \
	if (vshm_trace_id == (b) || vshm_trace_id == -1) { \
		KTRACE12(vshm_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f), \
			(g), (void *)(__psint_t)(h), \
			(i), (void *)(__psint_t)(j)); \
	} \
}
#else
#define ADDVSHM()
#define DELETEVSHM()
#define	VSHM_TRACE2(a,b)
#define	VSHM_TRACE4(a,b,c,d)
#define	VSHM_TRACE6(a,b,c,d,e,f)
#define	VSHM_TRACE8(a,b,c,d,e,f,g,h)
#define	VSHM_TRACE10(a,b,c,d,e,f,g,h,i,j)
#endif

extern int vshmtabsz;
extern struct vshmtable *vshmtab;
extern mrlock_t vshm_lookup_lock;
extern int shmid_base;
extern int shmid_max;
extern struct zone *vshm_zone;
extern sv_t vshm_syncinit;

#endif	/* _VSHM_PRIVATE_H */
