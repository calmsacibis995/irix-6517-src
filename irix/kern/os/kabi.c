/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#include "sys/types.h"
#include "sys/atomic_ops.h"
#include "sys/capability.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/debug.h"
#include "sys/ddi.h"
#include "sys/errno.h"	
#include "sys/kabi.h"
#include "sys/kmem.h"
#include "sys/kthread.h"
#include "sys/mman.h"	
#include "sys/pda.h"
#include "sys/prctl.h"	/* PR_SFDS .. */
#include "sys/proc.h"
#include "sys/runq.h"	/* RQF_GFX .. */
#include "sys/schedctl.h"
#include "sys/sysinfo.h"	/* for sysinfo */
#include "sys/ksa.h"	/* for sysinfo */
#include "sys/systm.h"
#include "sys/sysmacros.h"
#include "sys/rt.h"  /* rt_pin_thread() */
#include "sys/runq.h"
#include "ksys/as.h" /* XXX for vtopv */
#include "ksys/ddmap.h"
#include "sys/pfdat.h"
#include "ksys/vm_pool.h"
#include "os/proc/pproc_private.h"


static void
cursproc_scan(int (*func)(thread_handle_t thd, void *arg), void *arg)
{
	proc_t *sp, *pp = curprocp;
	shaddr_t *sa;

	if (!IS_SPROC(curuthread->ut_pproxy) || !func)
		return;

	sa = pp->p_shaddr;
	mutex_lock(&sa->s_listlock, 0);
	for (sp = sa->s_plink; sp != NULL; sp = sp->p_slink) {
        	if ((sp != pp) &&
	  	     ((sp->p_proxy.prxy_shmask & (PR_SFDS|PR_SADDR)) ==
		     (PR_SFDS|PR_SADDR)))
		{
			if (func((thread_handle_t)prxy_to_thread(&sp->p_proxy), arg) == 0)
				break;
		}
	}
        mutex_unlock(&sa->s_listlock);
}

void
curthreadgroup_scan(int (*func)(thread_handle_t thd, void *arg), void *arg)
{
	uthread_t *ut;

	if (!func) return;
	if (IS_SPROC(curuthread->ut_pproxy)) {
		cursproc_scan(func,arg);
		return;
	}

	uscan_access(curuthread->ut_pproxy);
	uscan_forloop(curuthread->ut_pproxy,ut) {
		if (func((thread_handle_t)ut, arg) == 0)
			break;
	}
	uscan_unlock(curuthread->ut_pproxy);
}




/* 
 * a few other functions to set/clear or interrogate various flags from 
 * proc structure 
 */

/* process does this to itself, except for fork case */
void
setgfxsched_thread(thread_handle_t thd)
{
        (UT_TO_KT((uthread_t *)thd))->k_runcond |= RQF_GFX;
}

void
cleargfxsched_thread(thread_handle_t thd)
{
        (UT_TO_KT((uthread_t *)thd))->k_runcond &= ~RQF_GFX;
}

int
thread_issproc(thread_handle_t thd)
{
	return (IS_SPROC(((uthread_t*)thd)->ut_pproxy));
}

int
threadgroup_refcnt(thread_handle_t thd)
{
	uthread_t *ut = (uthread_t*)thd;
	int refcnt = 1;
	if (ut) {
		if (IS_SPROC(ut->ut_pproxy))
			refcnt = UT_TO_PROC(ut)->p_shaddr->s_refcnt;
		else
			refcnt = ut->ut_pproxy->prxy_nthreads;
		}
	return refcnt;
}

thread_handle_t
curthreadhandle(void)
{
	thread_handle_t t = NULL;
	kthread_t *kt;
	if (kt = UT_TO_KT(curuthread)) {
		/* Note: there are currently a couple situations where */
		/* curthreadp is not set properly but curuthread is.   */
		t = (thread_handle_t)kt;
		}
	else if (kt = curthreadp) {
		if (kt && KT_ISUTHREAD(kt)) {
			t = (thread_handle_t)KT_TO_UT(kt);
			}
		}
	return t;
}

proc_handl_t *
threadhandle_to_proc(thread_handle_t thd)
{
	uthread_t *ut = (uthread_t*)thd;
	if (!ut) return NULL;
	return ((proc_handl_t*)UT_TO_PROC(ut));
}

thread_group_handle_t
threadhandle_to_threadgrouphandle(thread_handle_t thd)
{
	uthread_t *ut = (uthread_t*)thd;
	thread_group_handle_t tgrp = NULL;
	if (ut) {
		if (IS_SPROC(ut->ut_pproxy))
			tgrp = (thread_group_handle_t)UT_TO_PROC(ut)->p_shaddr;
		else
			tgrp = (thread_group_handle_t)(__psint_t)UT_TO_PROC(ut)->p_pid;
		}
	return tgrp;
}

proc_handl_t *
curproc(void)
{
	return((proc_handl_t *)curprocp);
}

void *
get_cursproc_shaddr(void)
{
	return(curprocp->p_shaddr);
}

char
curproc_abi(void)
{
	ASSERT(curuthread);
	return(curuthread->ut_pproxy->prxy_abi);
}

char *
get_current_name(void)
{
	kthread_t *k = curthreadp;

	if (k)
		return KTOP_GET_NAME(k);
	else
		return "interrupt?";
}

uint64_t
get_thread_id(void)
{
	uint64_t rv;

	rv = kthread_getid();
	ASSERT(rv != (uint64_t) -1);
	return rv;
}

pid_t
proc_pid(proc_handl_t *pp)
{
    return (pp ? ((proc_t*)pp)->p_pid : 0);
}

char *
proc_name(proc_handl_t *pp)
{
    return (pp ? ((proc_t*)pp)->p_comm : "");
}


/*
 * VM interface for gfx
 */

/*
 * v_mapkvsegment(gx_preg, vaddr, kvaddr, pgcnt, cache_algp, fault_addr)
 * Allocate and set up ptes to map 'pgcnt' pages. The virtual to physical
 * sequential mapping starts with user virtual address 'vaddr' and kernel 
 * virtual address 'kvaddr' for 'pgcnt' pages, retaining any physical mapping
 * attributes implied in the 'kvaddr'.
 * 'gx_preg' is an opaque pointer that was returned by a previous called
 * to v_creatreg()
 */
/* ARGSUSED */
int
v_mapkvsegment(void *gx_preg, 
		char *vaddr, 
		void *kvaddr,
		pgno_t	pgcnt,
		int cache_algo,
		__psunsigned_t fault_addr)
{
	(void)ddv_mappages((ddv_handle_t) gx_preg,
		(uvaddr_t)vaddr - ddv_getaddr((ddv_handle_t) gx_preg), 
		kvaddr, pgcnt);
	if ((char *)fault_addr >= vaddr &&
				((char *)fault_addr < (vaddr + ctob(pgcnt))))
		return 1;
	return 0;
		
}

/*
 * v_mapsegment(gx_preg, vaddr, pagenum, pgcnt, cache_algp, fault_addr)
 * Allocate and set up ptes to map 'pgcnt' pages. The virtual to physical
 * sequential mapping starts with virtual address 'vaddr' and physical page 
 * 'pagenum' for 'pgcnt' pages.
 * 'gx_preg' is an opaque pointer that was returned by a previous called
 * to v_creatreg()
 */
/* ARGSUSED */
int
v_mapsegment(void *gx_preg, 
		char *vaddr, 
		unsigned pagenum,
		pgno_t	pgcnt,
		int cache_algo,
		__psunsigned_t fault_addr)
{
	void *kv;

	kv = (void *)(PHYS_TO_K0((__psunsigned_t)pagenum << PNUMSHFT));
	return v_mapkvsegment(gx_preg, vaddr, kv, pgcnt, 
			      cache_algo, fault_addr);
}

/* ARGSUSED */
int
chk_dmabuf_attr (char *buf, int nbytes, int perm)
{
	/* XXX OBSOLETE - use fast_userdma! */
	return 1;
}

int
curproc_vtop(
register caddr_t vaddr,
register int nbytes,
register int *ppfn,
register int step)              /* # bytes to incr ppfn each time */
{
        return(vtopv(vaddr, nbytes, (pfn_t *)ppfn, step, 0, 0));
}

int
curproc_vtopv(
register caddr_t vaddr,
int nbytes,
int *ppfn,
int step,
int shift,
int bits)
{
	return(vtopv(vaddr, nbytes, (pfn_t *)ppfn, step, shift, bits));
}

curabi_is_irix5_64()
{
	return (ABI_IS_IRIX5_64(get_current_abi()));
}

void
sysinfo_outch_add(int n)
{
	SYSINFO.outch += n;
}

void
sysinfo_rawch_add(int n)
{
	SYSINFO.rawch += n;
}

processors_configured(void)
{
	return(numcpus);
}


void
gfx_waitc_on(void)
{
	private.p_gfx_waitc = 1;
}


void
gfx_waitc_off(void)
{
	private.p_gfx_waitc = 0;
}


void
gfx_waitf_on(void)
{
	private.p_gfx_waitf = 1;
}


void
gfx_waitf_off(void)
{
	private.p_gfx_waitf = 0;
}

/*
 * XXX need to define a kabi interface to signaling
 */
void
psignal(proc_handl_t *h, int sig)
{
	proc_t *pp = (proc_t *)h;
	
	sigtopid(pp->p_pid, sig, SIG_ISKERN, 0, 0, 0);
}

void
pid_signal(pid_t pid, int sig, int flag, struct k_siginfo *sigi)
{
	sigtopid(pid, sig, SIG_ISKERN |
		(flag & PID_SIGNAL_NOSLEEP) ? SIG_NOSLEEP : 0,
		0, 0, sigi);
}

int
curuthread_set_rtpri(int newpri)
{
	if (newpri <= 0 || newpri > NDPLOMIN)
		return -1;	/* error */

	if (newpri != 0 &&
	    newpri <= NDPNORMMIN &&
	    !_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT))
		return -1;
	
	return setinfoRunq(UT_TO_KT(curuthread),
			   &curuthread->ut_pproxy->prxy_sched,
			   RQRTPRI, newpri);
}

/*
 * kabi_alloc_pages:
 * Allocate a list of pages and return them to the driver. The caller
 * passes an array of paddr_t. The pfds allocated are converted to physical
 * addresses.
 */
/* ARGSUSED */
int
kabi_alloc_pages(paddr_t *paddr_list, pgcnt_t npgs, int cache_color, 
		void *allocation_policy, int flags)
{

	paddr_t *p = paddr_list;
	int	i;
	pgcnt_t	npgs_allocated;
	pfd_t 	*pfd;

	if (reservemem(GLOBAL_POOL, npgs, npgs, 0)) 
		return EAGAIN;

	for (i = 0; i < npgs; i++, cache_color++, p++) {
		pfd = pagealloc(cache_color, flags);
		if (pfd == NULL) {
			unreservemem(GLOBAL_POOL, npgs, npgs, 0);
			npgs_allocated = i;
			p = paddr_list;
			for (i = 0; i < npgs_allocated; i++, p++) {
				pfd = pfntopfdat(pnum(*p));
				pagefree(pfd);
			}
			return EAGAIN;
		}

		*p = ctob(pfdattopfn(pfd));
	}
	return 0;
}

/*
 * kabi_free_pages:
 * Free the pages in the paddr_list that have been allocated before.
 */
void
kabi_free_pages(paddr_t *paddr_list, pgcnt_t npgs)
{
	pfd_t *pfd;
	int	i;

	for (i = 0; i < npgs; i++, paddr_list++) {
		pfd = pfntopfdat(pnum(*paddr_list));
		pagefree(pfd);
	}
	unreservemem(GLOBAL_POOL, npgs, npgs, 0);
}

/*
 * kabi_num_lockable_pages:
 * Return the number of lockable memory in the system.
 */
pgno_t 
kabi_num_lockable_pages()
{
	return GLOBAL_AVAILRMEM();
}
