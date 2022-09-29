/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.13 $"

#ifndef _KSYS_VPAG_H_
#define _KSYS_VPAG_H_

#include <ksys/behavior.h>
#include <sys/arsess.h>
#include <sys/extacct.h>
#include <values.h>		/* for MAXLONG */
#include <sys/cmn_err.h>
#include <sys/pfdat.h>

/*
 * Process aggregate.
 * A process aggregate is an object that represents a collection of processes.
 * It can be used by any subsystem that needs to track something (eg. resources) * for a set of processes. It is now used to represent an array session and
 * a miser job. The object is written in such a way so that it can be
 * distributed easily. 
 * The object is identified by an id of type paggid_t. It is unique for the 
 * system. For the array session it represents the array session handle.
 * VPAG_LOOKUP() on the paggid returns a pointer to the virtual object (vpagg_t) * Any operation on the process aggregate needs to be done by calling the
 * appropriate VPAG_.. routine and passing a vpagg.
 * Other subsystems can keep a pointer to vpag in their internal structures
 * after doing a VPAG_LOOKUP once and getting a reference to the object.
 * 
 * The object goes away when all references to the object goes away.
 * A process an belong to only one process aggregate at any time.
 */

typedef	__int64_t	paggid_t;

struct  uthread_s;
struct 	vm_pool_s;

typedef struct vpagg_s {
	struct vpagg_s	*vpag_next; /* Pointers to maintain the active list */
	struct vpagg_s	*vpag_prev;
	paggid_t	vpag_paggid;	/* Process aggregate id */
	int		vpag_refcnt;	/* Number of references to the object */
	bhv_head_t	vpag_bhvh;	/* Behaviour */
} vpagg_t;

#define	VPAG_BHV_HEAD(v)	(&((v)->vpag_bhvh))

/*
 * Vpag types. There are only two types right now. One is an array session
 * and another is miser. Note that a process can belong only process
 * aggregate at any time. Also one cannot move a process from one 
 * process aggregate type to another.
 */
typedef enum {
	VPAG_ARRAY_SESSION,
	VPAG_MISER
} vpag_type_t;

/*
 * Restriction control codes. Used for such things as controlling
 * the ability of an array session to execute newarraysess(), etc.
 */
typedef enum {
	VPAG_ALLOW_NEW,
	VPAG_NEW_IS_RESTRICTED,
	VPAG_RESTRICT_NEW
} vpag_restrict_ctl_t;

typedef	struct vpagg_ops {
	bhv_position_t  vpagg_position;
	void (*vpag_destroy)
			(bhv_desc_t *b, paggid_t paggid);  /* behaviour */
	vpag_type_t (*vpag_get_type)
			(bhv_desc_t *b);	/* behaviour */

	/*
	 * Array session operations.
	 */
	prid_t (*vpag_getprid)
			(bhv_desc_t *b); 	/* behaviour */
	void (*vpag_setprid)
			(bhv_desc_t *b, 	/* behaviour */
			prid_t prid);		/* prid value to set */
	void (*vpag_getspinfo)
			(bhv_desc_t *b, 	/* behaviour */
			char *spip,		/* Returned spi */
			int len);		/* length of spip */
	void (*vpag_setspinfo)
			(bhv_desc_t *b, 	/* behaviour */
			char *spip,		/* spi info value to set */
			int len);		/* length of spip */
	int  (*vpag_getspilen)
		        (bhv_desc_t *b);	/* behaviour */
	void (*vpag_setspilen)
		        (bhv_desc_t *b,		/* behaviour */
			 int len);		/* new length of spi */
	int  (*vpag_spinfo_isdflt)
			(bhv_desc_t *b);	/* behaviour */
	void (*vpag_make_shadow_acct)
			(bhv_desc_t *b);        /* behaviour */
	void (*vpag_accum_stats)
			(bhv_desc_t *b,         /* behaviour */
			struct acct_timers *, struct acct_counts *);
	void (*vpag_extract_arsess_info)
			(bhv_desc_t *b,         /* behaviour */
			arsess_t *);		/* returned array sess info */
	void (*vpag_extract_shacct_info)
			(bhv_desc_t *b,		/* behaviour */
			 shacct_t *);		/* returned shadow acct info */
	void (*vpag_flush_acct)
			(bhv_desc_t *b,		/* behaviour */
			 paggid_t,		/* ASH for acct record */
			 struct uthread_s *);	/* "owner" of acct record */
	int (*vpag_restrict_ctl)
			(bhv_desc_t *b,		/* behaviour */
			 vpag_restrict_ctl_t);	/* control code */

	/*
	 * Vm Resource  management 
	 */
	int  (*vpag_update_vm_resource)
			(bhv_desc_t *b, 
			int op, pfd_t *pfd, __psunsigned_t arg1, 
			__psunsigned_t arg2);
	void (*vpag_set_vm_resource_limits)
			(bhv_desc_t *b,
			pgno_t rss_limit);
	void (*vpag_get_vm_resource_limits)
			(bhv_desc_t *b,
			pgno_t *rss_limit);
	int  (*vpag_transfer_vm_pool) 
			(bhv_desc_t *b, struct vm_pool_s *);
	int  (*vpag_reservemem) 
			(bhv_desc_t *b, pgno_t, pgno_t, pgno_t *);
	void  (*vpag_unreservemem) 
			(bhv_desc_t *b, pgno_t, pgno_t);

	int (*vpag_suspend)(bhv_desc_t *b);

	void (*vpag_resume)(bhv_desc_t *b);
} vpagg_ops_t;

#define AddToVpagRefCnt(A,N) atomicAddInt(&(A)->vpag_refcnt, (N))

#define	VPAG_JOIN(vpag, pid)	vpag_join((vpag), (pid))
#define	VPAG_LEAVE(vpag, pid)	vpag_leave((vpag), (pid))
#define	VPAG_HOLD(vpag)		vpag_join((vpag), 0)
#define	VPAG_RELE(vpag)		vpag_leave((vpag), 0)

static __inline void
VPAG_DESTROY(vpagg_t *vpag)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_destroy)(
		bhvh->bh_first, vpag->vpag_paggid);
	BHV_READ_UNLOCK(bhvh);
}

static __inline vpag_type_t
VPAG_GET_TYPE(vpagg_t *vpag)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	vpag_type_t 	type;
	BHV_READ_LOCK(bhvh);
	type = (*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->
				vpag_get_type)(bhvh->bh_first);
	BHV_READ_UNLOCK(bhvh);
	return type;
}

/*
 * Array session operations.
 */
static __inline prid_t
VPAG_GETPRID(vpagg_t *vpag)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	prid_t	tmp_prid;
	BHV_READ_LOCK(bhvh);
	tmp_prid = (*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_getprid)(
		bhvh->bh_first);
	BHV_READ_UNLOCK(bhvh);
	return tmp_prid;
}

static __inline void
VPAG_SETPRID(vpagg_t *vpag, prid_t prid)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_setprid)(
		bhvh->bh_first, prid);
	BHV_READ_UNLOCK(bhvh);
}

static __inline void
VPAG_SETSPINFO(vpagg_t *vpag, char *pspi, int len)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_setspinfo)(
		bhvh->bh_first, pspi, len);
	BHV_READ_UNLOCK(bhvh);
}

static __inline void
VPAG_GETSPINFO(vpagg_t *vpag, char *pspi, int len)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_getspinfo)(
		bhvh->bh_first, pspi, len);
	BHV_READ_UNLOCK(bhvh);
}

static __inline int
VPAG_GETSPILEN(vpagg_t *vpag)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	int tmp_len;
	BHV_READ_LOCK(bhvh);
	tmp_len = (*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_getspilen)(
		bhvh->bh_first);
	BHV_READ_UNLOCK(bhvh);
	return tmp_len;
}

static __inline void
VPAG_SETSPILEN(vpagg_t *vpag, int len)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_setspilen)(
		bhvh->bh_first, len);
	BHV_READ_UNLOCK(bhvh);
}

static __inline int
VPAG_SPINFO_ISDFLT(vpagg_t *vpag)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	int rslt;
	BHV_READ_LOCK(bhvh);
	rslt = (*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_spinfo_isdflt)(
		bhvh->bh_first);
	BHV_READ_UNLOCK(bhvh);
	return rslt;
}

static __inline void
VPAG_MAKE_SHADOW_ACCT(vpagg_t *vpag)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_make_shadow_acct)(
		bhvh->bh_first);
	BHV_READ_UNLOCK(bhvh);
}

static __inline void
VPAG_ACCUM_STATS(vpagg_t *vpag, struct acct_timers *timers, 
			struct acct_counts *counts)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_accum_stats)(
		bhvh->bh_first, timers, counts);
	BHV_READ_UNLOCK(bhvh);
}

static __inline void
VPAG_EXTRACT_ARSESS_INFO(vpagg_t *vpag, arsess_t *arsess_info)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_extract_arsess_info)(
		bhvh->bh_first, arsess_info);
	BHV_READ_UNLOCK(bhvh);
	arsess_info->as_refcnt = vpag->vpag_refcnt;
	arsess_info->as_handle = vpag->vpag_paggid;
}

static __inline void
VPAG_EXTRACT_SHACCT_INFO(vpagg_t *vpag, shacct_t *shacct_info)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_extract_shacct_info)(
		bhvh->bh_first, shacct_info);
	BHV_READ_UNLOCK(bhvh);
}

static __inline void
VPAG_FLUSH_ACCT(vpagg_t *vpag, struct uthread_s *ut)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_flush_acct)(
		bhvh->bh_first, vpag->vpag_paggid, ut);
	BHV_READ_UNLOCK(bhvh);
}

static __inline int
VPAG_RESTRICT_CTL(vpagg_t *vpag, vpag_restrict_ctl_t code)
{
	int r;

	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	r = (*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_restrict_ctl)(
		bhvh->bh_first, code);
	BHV_READ_UNLOCK(bhvh);
	return r;
}

/*
 * Miser operations.
 */
static __inline int
VPAG_UPDATE_VM_RESOURCE(vpagg_t *vpag, int op, pfd_t *pfd, 
		__psunsigned_t arg1, __psunsigned_t arg2)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	int	ret;
	BHV_READ_LOCK(bhvh);
	ret = (*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_update_vm_resource)(
		bhvh->bh_first, op, pfd, arg1, arg2);
	BHV_READ_UNLOCK(bhvh);
	return ret;
}

static __inline void
VPAG_SET_VM_RESOURCE_LIMITS(vpagg_t *vpag, pgno_t rss_limit)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_set_vm_resource_limits)(
		bhvh->bh_first, rss_limit);
	BHV_READ_UNLOCK(bhvh);
}

static __inline void
VPAG_GET_VM_RESOURCE_LIMITS(vpagg_t *vpag, pgno_t *rss_limit)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_get_vm_resource_limits)(
		bhvh->bh_first, rss_limit);
	BHV_READ_UNLOCK(bhvh);
}

static __inline int
VPAG_TRANSFER_VM_POOL(vpagg_t *vpag, struct vm_pool_s *new_pool)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	int	error;

	BHV_READ_LOCK(bhvh);
	error = (*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->
			vpag_transfer_vm_pool)(bhvh->bh_first, new_pool);
	BHV_READ_UNLOCK(bhvh);
	return error;
}

static __inline int
VPAG_RESERVEMEM(vpagg_t *vpag, pgno_t smem, pgno_t rmem, pgno_t *pre_reserved_mem) 
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	int	ret;

	BHV_READ_LOCK(bhvh);
	ret = (*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->
			vpag_reservemem)(bhvh->bh_first, 
				smem, rmem, pre_reserved_mem);
	BHV_READ_UNLOCK(bhvh);
	return ret;
}

static __inline void
VPAG_UNRESERVEMEM(vpagg_t *vpag, pgno_t smem, pgno_t rmem) 
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);

	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->
			vpag_unreservemem)(bhvh->bh_first, 
				smem, rmem);
	BHV_READ_UNLOCK(bhvh);

}

static __inline int
VPAG_SUSPEND(vpagg_t *vpag)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);
	int	ret;

	BHV_READ_LOCK(bhvh);
	ret = (*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->vpag_suspend)
				(bhvh->bh_first);
	BHV_READ_UNLOCK(bhvh);
	return ret;
}


static __inline void
VPAG_RESUME(vpagg_t *vpag)
{
	bhv_head_t *bhvh = &(vpag->vpag_bhvh);

	BHV_READ_LOCK(bhvh);
	(*((vpagg_ops_t *)bhvh->bh_first->bd_ops)->
			vpag_resume)(bhvh->bh_first);
	BHV_READ_UNLOCK(bhvh);
}

#ifdef  DEBUG
static __inline void
pag_printf(char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
	icmn_err(CE_CONT, fmt, ap);
        va_end(ap);
}
#else
/* ARGSUSED */
static __inline void
pag_printf(char *fmt, ...)
{
}
#endif /* DEBUG */

#define	VPAG_UPDATE_VM_RSS(vpag, op, pfd, arg)	\
	((vpag) ? VPAG_UPDATE_VM_RESOURCE((vpag), (op), (pfd), \
			((__psunsigned_t)(arg)), 0) : 0)

/*
 * Op codes to job rss update code.
 */
#define	JOBRSS_INC_FOR_PFD	0x1	/* page being added to ws */
#define	JOBRSS_INC_BLIND	0x2	/* can new pages be added */
#define	JOBRSS_INS_PFD		0x4	/* insert new pages into ws */
#define JOBRSS_INC_RELE		0x8	/* can old page be replaced by new */
#define JOBRSS_INC_RELE_UNDO	0x10	/* undo previous inc_rele op */
#define JOBRSS_FLIP_PFD_DEL	0x20	/* throw away page, replace by new */
#define JOBRSS_FLIP_PFD_ADD	0x40	/* add new page, replace old page */
#define JOBRSS_DECREASE 	0x80	/* page going out of ws */

/*
 * Set of operations which do not need job rss operations.
 */
#define JOBRSS_NO_OP (JOBRSS_INS_PFD|JOBRSS_FLIP_PFD_DEL|JOBRSS_FLIP_PFD_ADD)
 
/*
 * Values to set smem/rmem limits to infinity.
 */
#define	RSS_INFINITY	MAXLONG

#define	INVALID_PAGGID	-1

#define	VPAG_GETPAGGID(vpag)	((vpag)->vpag_paggid)
#define	VPAG_LOOKUP(paggid)	vpag_lookup(paggid)

extern	int  	vpagg_enumashs(paggid_t *, int);
extern	vpagg_t *vpag_lookup(paggid_t);
extern  int 	vpag_create(paggid_t, pid_t, int, vpagg_t **, vpag_type_t);
extern	void	vpag_init(void);
extern  void  	vpag_join(vpagg_t *, pid_t);
extern  void  	vpag_leave(vpagg_t *, pid_t);
extern  int   	vpag_set_paggid(vpagg_t *, paggid_t);
extern	int 	vpagg_enumpaggids(paggid_t *, int);
extern	int	vpag_setarrayid(int);
extern	int	vpag_setidctr(paggid_t);
extern	int	vpag_setidincr(paggid_t);
extern	int 	vpag_setmachid(int);
extern	int	vpag_getarrayid(void);
extern	paggid_t vpag_getidctr(void);
extern	paggid_t vpag_getidincr(void);
extern	int 	 vpag_getmachid(void);
extern	paggid_t vpag_allocid(void);
extern  vpagg_t *vpag_miser_create(size_t);
extern  int 	vpag_transfer_vm_pool(vpagg_t *, struct vm_pool_s *);
extern	int	miser_jobcount(void *, void *, void *);

extern	vpagg_t	vpagg0;
extern prid_t dfltprid;


#endif /* _KSYS_VPAG_H_ */
