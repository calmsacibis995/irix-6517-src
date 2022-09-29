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
#ifndef	_VSEM_PRIVATE_H
#define	_VSEM_PRIVATE_H 1
#ident "$Id: vsem_private.h,v 1.2 1997/08/01 21:00:44 sp Exp $"

#ifdef DEBUG
#define SEMDEBUG 1
#endif

#include <ksys/vsem.h>
#include <sys/ipc.h>

/*
 * Exported operations
 */
struct psem;
struct semid_ds;
extern int psem_alloc(int, int, int, struct cred *, vsem_t *);
extern void psem_free(struct psem *);
extern int psem_keycheck(bhv_desc_t *, struct cred *, int, int);
extern void psem_setperm(bhv_desc_t *, struct ipc_perm *);
extern int psem_rmid(bhv_desc_t *, struct cred *);
extern int psem_ipcset(bhv_desc_t *, struct ipc_perm *, struct cred *);
extern int psem_getstat(bhv_desc_t *, struct cred *,
				struct semid_ds *, cell_t *);
extern int psem_mac_access(bhv_desc_t *, struct cred *);
extern void psem_destroy(bhv_desc_t *);

/*
 * Other private parts of sem implementation
 */
extern int	vsem_getstatus(struct semstat *, struct cred *);

/*
 * Registration for the positions at which the different vsem behaviors 
 * are chained.  When on the same chain, a behavior with a higher position 
 * number is invoked before one with a lower position number.
 */
#define VSEM_POSITION_PSEM	BHV_POSITION_BASE	/* chain bottom */
#define VSEM_POSITION_DC	BHV_POSITION_BASE+1	/* distr. client */
#define VSEM_POSITION_DS	BHV_POSITION_BASE+2	/* distr. server */

#define VSEM_BHV_HEAD_PTR(v)	(&(v)->vsm_bh)
#define VSEM_TO_FIRST_BHV(v)	(BHV_HEAD_FIRST(&(v)->vsm_bh))
#define BHV_TO_VSEM(bdp)	((struct vsem *)BHV_VOBJ(bdp))

#define BHV_TO_PSEM(bdp) \
	(ASSERT(BHV_OPS(bdp) == &psem_ops), \
	(psem_t *)(BHV_PDATA(bdp)))
#define BHV_TO_DCSEM(bdp) \
	(ASSERT(BHV_OPS(bdp) == &dcsem_ops), \
	(dcsem_t *)(BHV_PDATA(bdp)))
#define BHV_TO_DSSEM(bdp) \
	(ASSERT(BHV_OPS(bdp) == &dssem_ops), \
	(dssem_t *)(BHV_PDATA(bdp)))

extern semops_t dssem_ops;
extern semops_t dcsem_ops;
extern semops_t psem_ops;

#if DEBUG
#include <ksys/vproc.h>
#include <sys/ktrace.h>
#include <sys/pda.h>
#include <sys/atomic_ops.h>

extern struct ktrace *vsem_trace;
extern int	vsem_trace_id;
extern void	idbg_vsem_bhv_print(vsem_t *);
extern void	idbg_psem_print(struct psem *);

#define	VSEM_TRACE2(a,b) { \
	if (vsem_trace_id == (b) || vsem_trace_id == -1) { \
		KTRACE4(vsem_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid()); \
	} \
}
#define	VSEM_TRACE4(a,b,c,d) { \
	if (vsem_trace_id == (b) || vsem_trace_id == -1) { \
		KTRACE6(vsem_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d)); \
	} \
}
#define	VSEM_TRACE6(a,b,c,d,e,f) { \
	if (vsem_trace_id == (b) || vsem_trace_id == -1) { \
		 KTRACE8(vsem_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f)); \
	} \
}
#define	VSEM_TRACE8(a,b,c,d,e,f,g,h) { \
	if (vsem_trace_id == (b) || vsem_trace_id == -1) { \
		KTRACE10(vsem_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f), \
			(g), (void *)(__psint_t)(h)); \
	} \
}
#define	VSEM_TRACE10(a,b,c,d,e,f,g,h,i,j) { \
	if (vsem_trace_id == (b) || vsem_trace_id == -1) { \
		KTRACE12(vsem_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f), \
			(g), (void *)(__psint_t)(h), \
			(i), (void *)(__psint_t)(j)); \
	} \
}
#else
#define	VSEM_TRACE2(a,b)
#define	VSEM_TRACE4(a,b,c,d)
#define	VSEM_TRACE6(a,b,c,d,e,f)
#define	VSEM_TRACE8(a,b,c,d,e,f,g,h)
#define	VSEM_TRACE10(a,b,c,d,e,f,g,h,i,j)
#endif

extern int vsemtabsz;
extern struct vsemtable *vsemtab;
extern mrlock_t vsem_lookup_lock;
extern int semid_base;
extern int semid_max;
extern struct zone *vsem_zone;

#endif	/* _VSEM_PRIVATE_H */
