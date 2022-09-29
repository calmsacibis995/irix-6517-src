/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Revision: 1.44 $"

#include "stddef.h"
#include "sys/types.h"
#include "ksys/as.h"
#include "as_private.h"
#include "sys/atomic_ops.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/getpages.h"
#include "sys/kmem.h"
#include "limits.h"
#include "sys/numa.h"
#include "sys/page.h"
#include "sys/param.h" /* MIN & MAX */
#include "pmap.h"
#include "sys/proc.h"
#include "os/proc/pproc_private.h"
#include "sys/vnode.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/prctl.h"
#include "ksys/vpag.h"
#include "os/numa/pm.h"
#include "os/numa/pmo_ns.h"
#include "os/numa/aspm.h"

static zone_t *vas_zone;
static lock_t as_lookup_mutex;
static mrlock_t as_scan_lock;
kqueue_t *as_list;
kqueue_t as_scanlist;
#if MP
#pragma fill_symbol (as_lookup_mutex, L2cacheline)
#pragma fill_symbol (as_scan_lock, L2cacheline)
#endif /* MP */

int as_listsz;
static uint64_t asgen;
extern int nproc;		/* used just for hash sizing */
extern rlim_t  rlimit_data_cur;
extern rlim_t  rlimit_stack_cur;
extern rlim_t  rlimit_vmem_cur;
extern rlim_t  rlimit_rss_cur;
extern rlim_t  rlimit_rss_max;

#define ASHASH(g)	((g) & (as_listsz - 1))



void
as_init(void)
{
	kqueue_t *kq;
	int i;

	vas_zone = kmem_zone_init(sizeof(vas_t), "vas");
	pas_init();
	spinlock_init(&as_lookup_mutex, "as_lookup_mutex");
	mrlock_init(&as_scan_lock, MRLOCK_DBLTRIPPABLE, "as_scan", -1);

	as_listsz = nproc / 6;
	while (as_listsz & (as_listsz -1))
		as_listsz++;

	as_list = (kqueue_t *)kern_malloc(sizeof(kqueue_t) * as_listsz);
	for (i = 0; i < as_listsz; i++) {
		kq = &as_list[i];
		kqueue_init(kq);
	}
	kqueue_init(&as_scanlist);
}

/*
 * init p0's asid. This sets up enough so that p0 can fork. This
 * is where the rlimits are set
 */
void
as_p0init(proc_t *p)
{
	pas_t *pas;
	vasid_t vasid;
        pm_t* pm;
	uthread_t *ut = prxy_to_thread(&p->p_proxy);

	/*
	 * set rss_cur here based on available memory NOW
	 */
	if (rlimit_rss_cur == 0) {
		/*
		 * auto-config - keep 4Mb free unless have real small memory
		 * where we keep 1/10th of memory free
		 * 3Mb is basically required to run a window system + 1Mb for
		 * more kernel space
		 */
		rlimit_rss_cur = 
		   MAX((rlim_t)ctob(GLOBAL_FREEMEM()) - 4*1024*1024,
		       (rlim_t)ctob(GLOBAL_FREEMEM() - GLOBAL_FREEMEM()/10));
	}
	rlimit_rss_cur = MIN(rlimit_rss_cur, rlimit_rss_max);

	ut->ut_asid = as_create(&ut->ut_as, &vasid,
#if (_MIPS_SIM == _ABI64)
				1);
#else
				0);
#endif

	pas = VASID_TO_PAS(vasid);
	pas->pas_vmemmax = rlimit_vmem_cur;
	pas->pas_datamax = rlimit_data_cur;
	pas->pas_stkmax = rlimit_stack_cur;
	pas->pas_rssmax = rlimit_rss_cur;
	/* wrapping, ie msb being set in btoc64 is okay */
	if (btoc64(pas->pas_rssmax) > INT_MAX)
		pas->pas_maxrss = INT_MAX;
	else
		pas->pas_maxrss = btoc64(pas->pas_rssmax);
	/* setup initial policy module */
	pas->pas_aspm = aspm_base_create();
        pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
        kthread_afflink_exec(UT_TO_KT(ut), pm);
        aspm_releasepm(pm);
}

/*
 * as_create - create a local address space object for a single thread
 */
asid_t
as_create(struct utas_s *who, vasid_t *vasid, int is64)
{
	pas_t *pas;
	vas_t *vas;
	asid_t ah;
	int ashash;
	int rv;

	vas = kmem_zone_alloc(vas_zone, KM_SLEEP);
	bhv_head_init(&vas->vas_bhvh, "vas");

	/* fill in rest of vas & put on list of active vas's */
	vas->vas_refcnt = 1;
	vas->vas_gen = ++asgen;
	vas->vas_defunct = 0;

	pas = pas_alloc(vas, who, is64);

	ashash = ASHASH(vas->vas_gen);
	rv = mutex_spinlock(&as_lookup_mutex);
	kqueue_enter(&as_list[ashash], &vas->vas_queue);
	mutex_spinunlock(&as_lookup_mutex, rv);
	rv = mrspinupdate(&as_scan_lock);
	kqueue_enter(&as_scanlist, &vas->vas_squeue);
	mrspinunlock(&as_scan_lock, rv);

	/* construct handle */
	ah.as_obj = (struct __as_opaque *)vas;
	ah.as_pasid = (aspasid_t)pas->pas_plist;
	ah.as_gen = vas->vas_gen;
	vasid->vas_obj = vas;
	vasid->vas_pasid = (aspasid_t)pas->pas_plist;
	return ah;
}

static void
as_destroy(vas_t *vas)
{
	ASSERT(vas->vas_refcnt == 0);

	bhv_head_destroy(&vas->vas_bhvh);
	kmem_zone_free(vas_zone, vas);
}

int
as_lookup_current(vasid_t *v)
{
	v->vas_obj = (vas_t *)curuthread->ut_asid.as_obj;
	v->vas_pasid = curuthread->ut_asid.as_pasid;
	ASSERT(v->vas_obj->vas_gen == curuthread->ut_asid.as_gen);
	ASSERT(v->vas_obj->vas_refcnt > 0);
	return 0;
}

int
as_lookup_pinned(uthread_t *pinned, vasid_t *v)
{
	v->vas_obj = (vas_t *)pinned->ut_asid.as_obj;
	v->vas_pasid = pinned->ut_asid.as_pasid;
	ASSERT(v->vas_obj->vas_gen == pinned->ut_asid.as_gen);
	ASSERT(v->vas_obj->vas_refcnt > 0);
	return 0;
}

/*
 * as_lookup is mainly for 3rd party threads that want to gaze into
 * another address space.
 * operations on the 'current' address space should use as_lookup_current()
 */
int
as_lookup(asid_t i, vasid_t *v)
{
	vas_t *vas = ASID_TO_VAS(i);
	kqueue_t *kq;
	vas_t *tvas;
	int gen;
	int rv;

	gen = ASID_TO_GEN(i);

	rv = mutex_spinlock(&as_lookup_mutex);
	kq = &as_list[ASHASH(gen)];
	for (tvas = (vas_t *)kqueue_first(kq);
			tvas != (vas_t *)kqueue_end(kq);
			tvas = (vas_t *)kqueue_next(&tvas->vas_queue)) {
		if (tvas == vas && tvas->vas_gen == gen) {
			if (compare_and_inc_int_gt_zero(&vas->vas_refcnt) == 0) {
				/* whoa - ref was zero - raced with
				 * as_rele - which can't continue until
				 * we release the lock. 
				 */
				break;
			}
				
			ASSERT(vas->vas_refcnt > 0);
			mutex_spinunlock(&as_lookup_mutex, rv);
			v->vas_obj = vas;
			v->vas_pasid = i.as_pasid;
			/*
			 * vas is valid - now check on pasid
			 */
			if (VAS_LOOKUPPASID(*v)) {
				/* pasid illegal ... can't call as_rele
				 * since we don't have a pasid ref
				 */
				as_rele_common(*v);
				return 1;
			}
			return 0;
		}
	}
	mutex_spinunlock(&as_lookup_mutex, rv);
	return 1;
}

/*
 * Operate on all AS's
 */
int
as_scan(as_scanop_t op, int flags, as_scan_t *ast)
{
	kqueue_t *kq;
	vas_t *vas, *nvas;
	vasid_t v;
	int error = 0;
	as_getattr_t asattr;

	mraccess(&as_scan_lock);
	kq = &as_scanlist;
	for (vas = SKQ_TO_VAS(kqueue_first(kq));
				vas != SKQ_TO_VAS(kqueue_end(kq)); vas = nvas) {

		nvas = SKQ_TO_VAS(kqueue_next(&vas->vas_squeue));

		if (compare_and_inc_int_gt_zero(&vas->vas_refcnt) == 0) {
			/*
			 * Check here for vas's that have been defuncted :
			 * if the AS has released the pas, we can free
			 * it up now.
			 */
			if (vas->vas_defunct) {
				ASSERT(vas->vas_refcnt == 0);
				if (mrtrypromote(&as_scan_lock)) {
					kqueue_remove(&vas->vas_squeue);
					kqueue_null(&vas->vas_squeue);
					mrdemote(&as_scan_lock);
					as_destroy(vas);
				}
			}
			continue;
		}

		ASSERT(vas->vas_refcnt > 0);
		v.vas_obj = vas;
		v.vas_pasid = 0;
		/* XXX probably races here w/ migration */
		if ((flags & AS_SCAN_LOCAL) &&
			(asvo_ops_t *)BHV_HEAD_FIRST(&vas->vas_bhvh)->bd_ops != &pas_ops) {
			as_rele_common(v);
			continue;
		}

		/*
		 * now that we have a reference - the vas/pas fields can
		 * not change from under us.
		 */

		VAS_GETATTR(v, AS_GET_PAGERINFO, NULL, &asattr);
		if (asattr.as_pagerinfo.as_pri == PBATCH_CRITICAL) {
			as_rele_common(v);
			continue;
		}	

		switch (op) {
		case AS_AGESCAN:
			(void) VAS_SHAKE(v, AS_SHAKEAGE, NULL);
			break;
		case AS_RSSSCAN:
			(void) VAS_SHAKE(v, AS_SHAKERSS, NULL);
			break;
		case AS_SWAPSCAN:
			error = VAS_SHAKE(v, AS_SHAKESWAP,
						&ast->as_scan_shake);
			if (error == EBUSY)
				/* not really an error */
				error = 0;
			if (error) {
				as_rele_common(v);
				goto out;
			}
			break;
		case AS_ANONSCAN:
			(void) VAS_SHAKE(v, AS_SHAKEANON, NULL);
			break;
#ifdef R10000_SPECULATION_WAR
		case 99: {
			int rc;
			rc = pinpagescanner(v,(void *)ast);
			if (rc == EAGAIN)	/* save EAGAIN lock failure */
				error = EAGAIN;
			}
			break;
#endif

		}
		as_rele_common(v);
	}
out:
	mrunlock(&as_scan_lock);
	return error;
}

/*
 * build list of local ASs sorted for vhand
 * NOTE - returns REFERENCED vas's - caller better un-ref them!
 * We assume that prilist points to enough space - since this is vhand
 * we may not sleep for memory.
 */
/* ARGSUSED */
int
as_vhandscan(prilist_t *prilist, int max)
{
	kqueue_t *kq;
	vas_t *vas;
	int listlen;
	vasid_t v;
	prilist_t *mp;
	prilist_t *nmp;
	as_getattr_t asattr;
	time_t nslp, oslp;

	listlen = 0;
	mraccess(&as_scan_lock);
	kq = &as_scanlist;
	for (vas = SKQ_TO_VAS(kqueue_first(kq));
			vas != SKQ_TO_VAS(kqueue_end(kq));
			vas = SKQ_TO_VAS(kqueue_next(&vas->vas_squeue))) {

		if (compare_and_inc_int_gt_zero(&vas->vas_refcnt) == 0) {
			/* whoa - ref was zero - raced with
			 * as_rele - which won't free the vas since we have
			 * the scan_lock.
			 */
			continue;
		}
		ASSERT(vas->vas_refcnt > 0);

		v.vas_obj = vas;
		v.vas_pasid = 0;
		/* XXX probably races here w/ migration */
		if ((asvo_ops_t *)BHV_HEAD_FIRST(&vas->vas_bhvh)->bd_ops != &pas_ops) {
			as_rele_common(v);
			continue;
		}

		/*
		 * race through all ppas's and record shortest
		 * sleep time and highest priority
		 * XXX is this really worth the hassle??
		 */
		VAS_GETATTR(v, AS_GET_PAGERINFO, NULL, &asattr);

		if (asattr.as_pagerinfo.as_pri == PBATCH_CRITICAL) {
			as_rele_common(v);
			continue;
		}	

		if ((asattr.as_pagerinfo.as_state == GTPGS_ISO)  ||
			(asattr.as_pagerinfo.as_state == GTPGS_DEFUNCT)) {
			/* don't even include it */
			as_rele_common(v);
			continue;
		}

		if (listlen == max) {
			as_rele_common(v);
			break;
		}

		/*
		 * Build a binary tree of ASs based on the 
		 * priority and slptime.
		 */
		ASSERT(listlen < max);
		nmp = &prilist[listlen++];
		nmp->vas = vas;
		nmp->left = 0;
		nmp->right = 0;
		nmp->sargs.as_shakepage_slptime = asattr.as_pagerinfo.as_slptime;
		nmp->sargs.as_shakepage_pri = asattr.as_pagerinfo.as_pri;
		nmp->sargs.as_shakepage_state = asattr.as_pagerinfo.as_state;
		ASSERT(asattr.as_pagerinfo.as_state);
		if (listlen == 1) {
			nmp->parent = 0;
			continue;
		}

		/*
		 * Recursive sort is a little bogus with limited
		 * stack space - so do it in place!
		 */
		mp = prilist;
		for (;;) {
			nslp = nmp->sargs.as_shakepage_slptime;
			oslp = mp->sargs.as_shakepage_slptime;
			if ((nslp < oslp) ||
			   ((nslp == oslp) &&
			     (nmp->sargs.as_shakepage_pri >
			     mp->sargs.as_shakepage_pri))) {
				if (mp->left == 0) {
					mp->left = nmp;
					nmp->parent = mp;
					break;
				} else {
					mp = mp->left;
					continue;
				}
			} else {
				if (mp->right == 0) {
					mp->right = nmp;
					nmp->parent = mp;
					break;
				} else {
					mp = mp->right;
					continue;
				}
			}
		}
	}
	mrunlock(&as_scan_lock);
	return listlen;
}

void
as_rele_common(vasid_t v)
{
	vas_t *vas = VASID_TO_VAS(v);
	int rv;

	ASSERT(vas->vas_refcnt > 0);
	if (atomicAddInt(&vas->vas_refcnt, -1) == 0) {
		/*
		 * Last use - tear down.
		 */
		rv = mutex_spinlock(&as_lookup_mutex);
		ASSERT(vas->vas_refcnt == 0);
		kqueue_remove(&vas->vas_queue);
		kqueue_null(&vas->vas_queue);
		mutex_spinunlock(&as_lookup_mutex, rv);
		VAS_DESTROY(v);		/* release pas and its resources */

		/*
		 * Why is this an mrtryupdate on as_scan_lock, and not
		 * an mrupdate?
		 * 1. as_scan might invoke this while holding lock in
		 *    in access mode, so can't do mrupdate here.
		 * 2. Don't want short time as users that use as_lookup/
		 *    as_rele to be held up by long time as scanners (vhand).
		 * 3. In pre-mri days, made sure that vhand was not hung
		 *    for the lock held by a low priority thread not being
		 *    able to run because of higher priority threads.
		 */
 
		if (mrtryspinupdate(&as_scan_lock, &rv)) {
			kqueue_remove(&vas->vas_squeue);
			kqueue_null(&vas->vas_squeue);
			mrspinunlock(&as_scan_lock, rv);
			as_destroy(vas);
		} else {
			ASSERT(vas->vas_defunct == 0);
			vas->vas_defunct = 1;
		}
	}
}

void
as_rele(vasid_t v)
{
	/* REFERENCED */
	vas_t *vas = VASID_TO_VAS(v);

	ASSERT(vas->vas_refcnt > 0);
	VAS_RELEPASID(v);
	as_rele_common(v);
}

/*
 * hack for proc0
 */
void
as_p0exit(void)
{
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;

	as_lookup_current(&vasid);
	AS_NULL(&curuthread->ut_asid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
	pas->pas_refcnt--;
	ppas->ppas_utas = NULL;
	as_rele(vasid);
}

/*
 * get size info of process
 */
int
as_getassize(asid_t asid, pgcnt_t *psize, pgcnt_t *rsssize)
{
	vasid_t vasid;
	as_getasattr_t asattr;
	int error = 0;

	if (as_lookup(asid, &vasid) == 0) {
		/* XXX remove when we maintain RSS */
		vrecalcrss(VASID_TO_PAS(vasid));
		VAS_GETASATTR(vasid, AS_PSIZE|AS_RSSSIZE, &asattr);
		as_rele(vasid);
		*psize = asattr.as_psize;
		*rsssize = asattr.as_rsssize;
	} else {
		*psize = 0;
		*rsssize = 0;
		error = ENOENT;
	}
	return error;
}

/*
 * as_exec_export - pull out all info that travels between the old and new
 * parts of an execed process
 */
void
as_exec_export(as_exec_state_t *st, asid_t *asp)
{
	vasid_t vasid;
	pas_t *pas;
	/* REFERENCED */
	ppas_t *ppas;
	int s;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
	st->as_vmemmax = pas->pas_vmemmax;
	st->as_datamax = pas->pas_datamax;
	st->as_stkmax = pas->pas_stkmax;
	st->as_rssmax = pas->pas_rssmax;
	st->as_maxrss = pas->pas_maxrss;
	st->as_pmap = pas->pas_pmap;
	st->as_tsave = pas->pas_tsave;

	/*
	 * Get an extra reference of the vpagg so that can pass it to the
	 * new address space. The current reference will be released when
	 * we do a as_rele() here.
	 */
	st->as_vpagg = pas->pas_vpagg;	
	if (st->as_vpagg)
		VPAG_HOLD(st->as_vpagg);
	if (pas->pas_tsave) {
		pas->pas_tsave = NULL;
		ASSERT(ppas->ppas_pmap == NULL);
		pas->pas_pmap = NULL;
	}

	/*
	 * done with old address space
	 * This also deletes the aspm
	 */
	AS_NULL(asp);
	s = mutex_spinlock(&ppas->ppas_utaslock);
	ppas->ppas_utas = NULL;
	mutex_spinunlock(&ppas->ppas_utaslock, s);
	as_rele(vasid);
}

/*
 * as_exec_import - create a new AS using info that was previously exported
 * Note - returns with new AS locked for update ...
 */
/* ARGSUSED */
int
as_exec_import(
	vasid_t *vasid,
	asid_t *asp,
	utas_t *utasp,
	as_exec_state_t *st,
	int abi,
	struct aspm *aspm,
	struct pm **pm)
{
	int s;
	pas_t *pas;
	asid_t ah;

	/*
	 * allocate a new AS
	 * XXX this assign of ut_asid needs to be atomic w.r.t clock...
	 */
	ah = as_create(utasp, vasid, ABI_IS_64BIT(abi));
	/* XXX protect against clock */
	s = splhi();
	*asp = ah;
	splx(s);
	pas = VASID_TO_PAS(*vasid);

	mrupdate(&pas->pas_aspacelock);
	pas->pas_vmemmax = st->as_vmemmax;
	pas->pas_datamax = st->as_datamax;
	pas->pas_stkmax = st->as_stkmax;
	pas->pas_rssmax = st->as_rssmax;
	pas->pas_maxrss = st->as_maxrss;

	/* Smem reserved for batch jobs  at exec time */
	pas->pas_smem_reserved = st->as_smem_reserved;	

	/* Import the process aggregate if any */
	pas->pas_vpagg = st->as_vpagg;

	pas->pas_aspm = aspm;
	*pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);

	if (st->as_tsave) {
		pas->pas_tsave = st->as_tsave;
		pas->pas_pmap = st->as_pmap;
	} else {
		/* initialize pmap for shared pregions */
		pas->pas_pmap = pmap_create(ABI_IS_64BIT(abi), PMAP_SEGMENT);
	}
	return 0;
}

/* ARGSUSED */
int
as_allocshmmo(vnode_t *vp, int shmid, as_mohandle_t *mop)
{
	reg_t *rp;
#ifdef CKPT
	extern int ckpt_enabled;
#endif
	/*
	 * Allocate the region for this descriptor
	 */
	rp = allocreg(vp, RT_MEM, RG_NOFREE|RG_ANON|RG_HASSANON);
	if (rp == NULL)
		return ENOSPC;
	ASSERT(rp->r_shmid == 0);
	rp->r_shmid = shmid;
#ifdef CKPT
	if (ckpt_enabled) {
		/*
		 * r_ckpt must be -1 since this region came from allocreg.
		 * If things change and it ever becomes possible for r_ckpt
		 * to not be, then this code should be modified to check
		 * r_ckpt and only set it when it's -1
		 */
		ASSERT(rp->r_ckptinfo == -1);
		rp->r_ckptflags = CKPT_SHM;
		rp->r_ckptinfo = shmid;
	}
#endif
	regrele(rp);
	mop->as_mohandle = rp;
	return 0;
}

void
as_freeshmmo(as_mohandle_t mo)
{
	reg_t *rp;

	rp = mo.as_mohandle;
	reglock(rp);
	/*
	 * SPT
	 * Inform pmap layer that this SHM has been removed
	 * (see comment in pmap.c)
	 */
	pmap_spt_remove((pmap_sptid_t)rp);

	if (rp->r_refcnt == 0)
		freereg(NULL, rp);
	else {
		rp->r_flags &= ~RG_NOFREE;
		regrele(rp);
	}
}

int
as_shmmo_refcnt(as_mohandle_t mo)
{
	return ((struct region *)mo.as_mohandle)->r_refcnt;
}
