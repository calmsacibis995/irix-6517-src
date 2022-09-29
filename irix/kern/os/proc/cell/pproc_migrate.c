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

#ident "$Id: pproc_migrate.c,v 1.37 1998/09/08 18:41:30 kanoj Exp $"
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>

#include <sys/uthread.h>
#include <sys/runq.h>
#include <sys/exec.h>
#include <sys/cred.h>
#include <sys/reg.h>
#include <sys/hwperfmacros.h>

#include <ksys/vproc.h>
#include <ksys/cell/relocation.h>
#include <ksys/exception.h>
#include <ksys/sthread.h>
#include <ksys/fdt.h>
#include <ksys/fdt_private.h>
#include <ksys/childlist.h>
#include <ksys/vpag.h>

#include "pproc.h"
#include "pproc_private.h"
#include "vproc_private.h"
#include "vproc_migrate.h"
#include "pproc_migrate.h"
#include "pgrp_migrate.h"
#include "fs/procfs/pr_relocation.h"

extern void copyustk(uthread_t *, uthread_t *);

#ifdef DEBUG
#define PRIVATE
#else
#define PRIVATE static
#endif

#define RESTRICTION(field,state) \
	ASSERT(field == state)

#define DECL(type,field)	type field ## _save
#define SAVE(ptr,field)		field ## _save = (ptr)->field
#define RESTORE(ptr,field)	(ptr)->field = field ## _save


int
uthread_source_prepare(
	proc_proxy_t	*prxyp,
	obj_manifest_t	*mfst)
{
	uthread_t	*ut;
	int		error = 0;

	ut = prxy_to_thread(prxyp);

	error = obj_mft_put(mfst, ut->ut_cdir, SVC_CFS, OBJ_RMODE_REFERENCE);
	ASSERT(!error);
	error = obj_mft_put(mfst, ut->ut_rdir, SVC_CFS, OBJ_RMODE_REFERENCE);
	ASSERT(!error);

	return(error);
}

int
fdt_source_prepare(
	fdt_t		*fdt,
	obj_manifest_t	*mfst)
{
	int		error;
	int		i;
	ufchunk_t	*ufc;
	vfile_t		*nofile = NULL;

	ASSERT(!fdt->fd_fwaiting);

	error = obj_bag_put(obj_mft_src_info(mfst), VPROC_PPROC_TAG_NOFILES,
				&fdt->fd_nofiles, sizeof(int));
	if (error)
		return(error);

	ufc = fdt->fd_flist;
	error = obj_bag_put(obj_mft_src_info(mfst),
		VPROC_PPROC_TAG_FDINUSE, ufc,
		fdt->fd_nofiles * sizeof(ufchunk_t));
	if (error)
		return(error);

	for (i = 0; i < fdt->fd_nofiles; i++) {
		if (!ufc[i].uf_ofile)
			continue;
		ASSERT(!ufc[i].uf_inuse);
		error = obj_mft_put(mfst, ufc[i].uf_ofile,
				SVC_VFILE, OBJ_RMODE_REFERENCE);
		if (error)
			return(error);
	}

	return(error);
}

int
pproc_source_prepare(
	bhv_desc_t	*bhv,
	cell_t		target_cell,
	obj_manifest_t	*mfst)
{
	struct proc	*p;
	int		error;
	migrate_info_t	*emi;
	struct uarg	*args;

	p = BHV_TO_PROC(bhv);
	emi = p->p_migrate;
	
	error = vpgrp_proc_emigrate_start(p->p_vpgrp, target_cell, mfst);

	error = prnode_proc_emigrate_start(p, mfst);

	error = fdt_source_prepare(p->p_fdt, mfst);
	ASSERT(!error);

	obj_bag_put(obj_mft_src_info(mfst), VPROC_OBJ_TAG_MI_ARGS,
			(void *) emi, sizeof(migrate_info_t));
	if (emi->mi_arg) {
		args = emi->mi_arg;

		error = obj_mft_put(mfst, args->ua_exec_vp, SVC_CFS,
				OBJ_RMODE_REFERENCE);
		error = obj_mft_put(mfst, args->ua_intp_vp, SVC_CFS,
				OBJ_RMODE_REFERENCE);
		error = obj_mft_put(mfst, args->ua_intpvp, SVC_CFS,
				OBJ_RMODE_REFERENCE);
	}

	/*
	 * XXX uthread and proc may not be local
	 */
	error = uthread_source_prepare(&p->p_proxy, mfst);
	ASSERT(!error);

	return(error);
}

int
uthread_target_prepare(
	obj_manifest_t	*mfst,
	uthread_t	**utp)
{
	uthread_t		*ut;
	int			error = 0;

	/*
	 * Allocate a temporary uthread structure.
	 * This will be later used to receive the migrating uthread,
	 * though note that this won't become the active uthread itself
	 * - it's used only to store uthread info temporarily.
	 */
	ut = kmem_alloc(sizeof(uthread_t), KM_SLEEP);
	*utp = ut;

	/*
	 * Create embryonic vnodes for current/root dirs.
	 */
	obj_mft_get(mfst, (void **) &ut->ut_cdir);
	obj_mft_get(mfst, (void **) &ut->ut_rdir);
	
	return(error);
}

void
fdt_target_prepare(
	obj_manifest_t	*mfst,
	fdt_t		**fdtp)
{
	/* REFERENCED */
	int		error;
	int		nofiles;
	int		i;
	void		*v;
	fdt_t		*fdt;
	ufchunk_t	*ufc;

	obj_bag_get_here(obj_mft_src_info(mfst), VPROC_PPROC_TAG_NOFILES,
                                &nofiles, sizeof(int), error);
        ASSERT(error == OBJ_BAG_SUCCESS);

	fdt = fdt_create(nofiles);

	ufc = fdt->fd_flist;
	obj_bag_get_here(obj_mft_src_info(mfst), 
			VPROC_PPROC_TAG_FDINUSE,
			ufc, nofiles * sizeof(ufchunk_t),
			error);
        ASSERT(error == OBJ_BAG_SUCCESS);

	ufc = fdt->fd_flist;
	for (i = 0; i < fdt->fd_nofiles; i++) {
		if (!ufc[i].uf_ofile)
			continue;
		error = obj_mft_get(mfst, &v);
		ufc[i].uf_ofile = v;
	}

	*fdtp = fdt;
}

void
pproc_obj_target_prepare(
	vproc_t		*vp,
	obj_manifest_t	*mfst)
{
	/* REFERENCED */
	int		error;
	proc_t		*p;
	migrate_info_t	*mi;

	/*
	 * Create embryonic proc struct. It won't be fully initialized
	 * until the unbag step.
	 */
	p = pproc_recreate(vp);

	/*
	 * Plug this into the behavior chain.
	 */
	bhv_insert(VPROC_BHV_HEAD(vp), &p->p_bhv);

	vpgrp_proc_immigrate_start(mfst, (void **) &p->p_vpgrp);

	prnode_proc_immigrate_start(p, mfst);

	/*
	 * We have no embryonic proc to hang this vpgrp onto
	 * at the moment. We will have to look it up later.
	 */

	fdt_target_prepare(mfst, &p->p_fdt);

	/*
	 * Get migration args into allocated space.
	 */
	mi = p->p_migrate = kmem_alloc(sizeof(migrate_info_t), KM_SLEEP);
	obj_bag_get_here(obj_mft_src_info(mfst), VPROC_OBJ_TAG_MI_ARGS,
			(void *) mi, sizeof(migrate_info_t), error);
	ASSERT(error == OBJ_SUCCESS);
	if (mi->mi_argsz) {
		struct uarg	*targargs;

		targargs = kmem_alloc(mi->mi_argsz, KM_SLEEP);
		mi->mi_arg = targargs;

		error = obj_mft_get(mfst, (void **) &targargs->ua_exec_vp);
		error = obj_mft_get(mfst, (void **) &targargs->ua_intp_vp);
		error = obj_mft_get(mfst, (void **) &targargs->ua_intpvp);
	}

	error = uthread_target_prepare(mfst, &mi->mi_threads);
	ASSERT(!error);
}

int
exec_args_bag(
	struct uarg	*args,
	obj_bag_t	bag)
{
	int		error;

	error = obj_bag_put(bag, VPROC_EXEC_TAG_ARGS, args,
			    sizeof(struct uarg));
	ASSERT(error == 0);


	if (args->ua_vwinadr) {
		error = obj_bag_put(bag, VPROC_EXEC_TAG_STACK, args->ua_vwinadr,
			ctob(args->ua_size));
		ASSERT(error == 0);
#if DEBUG
		bzero(args->ua_vwinadr, ctob(args->ua_size));
#endif
	}
	if (args->ua_phdrbase) {
		error = obj_bag_put(bag, VPROC_EXEC_TAG_PHDRBASE,
				args->ua_phdrbase, args->ua_phdrbase_sz);
		ASSERT(error == 0);
#if DEBUG
		bzero(args->ua_phdrbase, args->ua_phdrbase_sz);
#endif
	}
	if (args->ua_ehdrp) {
		error = obj_bag_put(bag, VPROC_EXEC_TAG_EHDRP, args->ua_ehdrp,
			args->ua_ehdrp_sz);
		ASSERT(error == 0);
#if DEBUG
		bzero(args->ua_ehdrp, args->ua_ehdrp_sz);
#endif
	}
	if (args->ua_idata.line1p) {
		error = obj_bag_put(bag, VPROC_EXEC_TAG_IDATA,
			args->ua_idata.line1p, args->ua_idata.line1p_sz);
		ASSERT(error == 0);
#if DEBUG
		bzero(args->ua_idata.line1p, args->ua_idata.line1p_sz);
#endif
	}
	return(error);
}

/* ARGSUSED */
PRIVATE void
job_bag(
	struct job_s	*job,
	obj_bag_t	bag)
{
	/*
	 * XXX For UNIKERNEL do nothing, jobs are global.
	 */
}

/* ARGSUSED */
PRIVATE void
cursig_bag(
	struct sigqueue	*curinfo,
	obj_bag_t	bag)
{
	/*
	 * XXX Can do better!
	 */
	RESTRICTION(curinfo, NULL);
}

PRIVATE void
exception_bag(
	exception_t	*exp,
	obj_bag_t	bag)
{
	/* REFERENCED */
	int	error;

	/*
	 * The exception structure has valuable user context.
	 */
	error = obj_bag_put(bag, VPROC_UT_TAG_EXCEPTION,
				exp, sizeof(exception_t));
	ASSERT(!error);
}

PRIVATE void
exitfunc_bag(
	struct exit_callback	*ecblist,
	obj_bag_t		bag)
{
	struct exit_callback	*ecb;

	ecb = ecblist;
	while (ecb) {
		obj_bag_put(bag, VPROC_PPROC_TAG_EXITFUNC,
				(void *) ecb, sizeof(*ecb));
		ecb = ecb->ecb_next;
	}
}

PRIVATE void
uthread_bag(
	proc_proxy_t	*prxyp,
	obj_bag_t	bag)
{
	/* REFERENCED */
	int		error;
	uthread_t	*sut;

	sut = prxy_to_thread(prxyp);

	/*
	 * Current restrictions and assumptions:
	 */
	RESTRICTION((UT_TO_KT(sut)->k_runcond & RQF_GFX), 0);
	RESTRICTION(sut->ut_openfp, NULL);
	RESTRICTION(sut->ut_prda, NULL);
	RESTRICTION(sut->ut_sighold, &UT_TO_PROC(sut)->p_sigvec.sv_sighold);
	
	/*
	 * Release select data area.
	 */
	if (sut->ut_polldat)
		(void) kern_free(sut->ut_polldat);
	sut->ut_polldat = NULL;

	RESTRICTION(sut->ut_fdinuse, NULL);
	if (sut->ut_fdmany) {
		RESTRICTION(sut->ut_fdmanysz, 0);
		kern_free(sut->ut_fdmany);
		sut->ut_fdmany = NULL;
		sut->ut_fdmanymax = 0;
	}
	RESTRICTION(sut->ut_fdmanysz, 0);
	RESTRICTION(sut->ut_fdmanymax, 0);
#ifdef ULI
	RESTRICTION(sut->ut_uli, NULL);
#endif
	RESTRICTION(sut->ut_excpt_fr_ptr, NULL);
	RESTRICTION(sut->ut_watch, NULL);
	RESTRICTION(sut->ut_sharena, NULL);

	/*
	 * Save and cede fp state.
	 */
	checkfp(sut, 0);

	/*
	 * Grab the entire uthread and stuff it into the bag.
	 */
	error = obj_bag_put(bag, VPROC_PPROC_TAG_UTHREAD,
				(void *) sut, sizeof(uthread_t));
	ASSERT(!error);

	/*
	 * Deal with associated structures.
	 */
	job_bag(sut->ut_job, bag);
	cursig_bag(sut->ut_curinfo, bag);
	exception_bag(sut->ut_exception, bag);
	exitfunc_bag(sut->ut_ecblist, bag);

}

PRIVATE void
prtrmasks_bag(
	struct pr_tracemasks	*tracemasks,
	obj_bag_t		bag)
{
	if (tracemasks)
		(void) obj_bag_put(bag, VPROC_PPROC_TAG_TRMASKS,
				   (void *) tracemasks,
				   sizeof(struct pr_tracemasks));
}

/* ARGSUSED */
PRIVATE void
pag_bag(
	vpagg_t		*vpagg,
	obj_bag_t	bag)
{
}

PRIVATE void
childlist_bag(
	child_pidlist_t	*childlist,
	obj_bag_t	bag)
{
	child_pidlist_t	*child;

	/*
	 * Bag each child entry.
	 */
	child = childlist;
	while (child) {
		obj_bag_put(bag, VPROC_PPROC_TAG_CHILD,
				(void *) child, sizeof(*child));
		child = child->cp_next;
	}
}

/* ARGSUSED */
PRIVATE void
proxy_bag(
	proc_proxy_t	*prxyp,
	obj_bag_t	bag)
{
	RESTRICTION(prxyp->prxy_utshare, NULL);
	RESTRICTION(prxyp->prxy_nthreads, 1);
	/*
	 * The proxy is otherwise bagged as part of the pproc.
	 */
}


void
pproc_source_bag(
	bhv_desc_t	*bhv,
	cell_t		target,
	obj_bag_t	bag)
{
	/* REFERENCED */
	int		error;
	struct proc	*p;
	migrate_info_t	*mi;

	p = BHV_TO_PROC(bhv);

	/*
	 * Current restrictions:
	 */
	RESTRICTION(p->p_slink, NULL);
	RESTRICTION(p->p_shaddr, NULL);
	RESTRICTION(p->p_proxy.prxy_semundo, NULL);
	RESTRICTION(p->p_cipso_proc, NULL);
	RESTRICTION(p->p_ptimers, NULL);
	RESTRICTION(p->p_profp, NULL);
	RESTRICTION(p->p_fastprof[0], NULL);
	RESTRICTION(p->p_kaiocbhd, NULL);
	RESTRICTION(p->p_shacct, NULL);
	/* Bug 511399 */
	p->p_ckptshm = NULL;
	RESTRICTION(p->p_ckptshm, NULL);

	/*
	 * Pending timers must have the time remaining computed,
	 * be cleared here on the source cell and be restarted on
	 * the target cell.
	 * Code here taken from alarm().
	 */
	{
		int		s;
		__int64_t	ticknum;
		s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
		if (timerisset(&p->p_realtimer.it_value)) {
			ticknum = chktimeout_tick(C_NORM, 0, realitexpire, p);
			tick_to_timeval((int)ticknum, &p->p_realtimer.it_value,
				USEC_PER_TICK);
			untimeout(p->p_itimer_toid);
		}
		mutex_spinunlock(&p->p_itimerlock, s);
	}

	p->p_ticks = lbolt - p->p_ticks; /* Re-compute when on other cell */

	error = obj_bag_put(bag, VPROC_OBJ_TAG_PPROC, p, sizeof(proc_t));
	ASSERT(!error);

	proxy_bag(&p->p_proxy, bag);
	
	/*
	 * Bag lists and other appendages linked to the proc structure.
	 */
	pag_bag(p->p_vpagg, bag);
	error = cred_bag_put(bag, p->p_cred);
	ASSERT(!error);
	childlist_bag(p->p_childpids, bag);
	exitfunc_bag(p->p_ecblist, bag);
	prtrmasks_bag(p->p_trmasks, bag);

	vpgrp_proc_emigrate(p->p_vpgrp, target, p, bag);

	prnode_proc_emigrate(p, bag);

	mi = (migrate_info_t *) p->p_migrate;
	if (mi->mi_arg) 
		error = exec_args_bag(mi->mi_arg, bag);

	uthread_bag(&p->p_proxy, bag);
}

#define COMPUTE_DELTA(base, ptr) \
	((char *)ptr - (char *)base)

int
exec_args_unbag(
	struct uarg	*args,
	obj_bag_t	bag)
{
	/* REFERENCED */
	int	error;
	int	offset;
	caddr_t	oldptr;
	vnode_t	*exec_vp;
	vnode_t	*intp_vp;
	vnode_t	*intpvp;

	/*
	 * XXX These are already created, don't copy over them
	 */
	exec_vp = args->ua_exec_vp;
	intp_vp = args->ua_intp_vp;
	intpvp = args->ua_intpvp;
	
	/*
	 * import the exec stuff
	 */
	obj_bag_get_here(bag, VPROC_EXEC_TAG_ARGS, args, sizeof(struct uarg), 
		error);
	ASSERT(error == 0);

	/*
	 * XXX These are already created, don't copy over them
	 */
	args->ua_exec_vp = exec_vp;
	args->ua_intp_vp = intp_vp;
	args->ua_intpvp = intpvp;

	if(oldptr = args->ua_vwinadr) {
#ifdef _VCE_AVOIDANCE
		args->ua_vwinadr = kvpalloc(args->ua_size, VM_VACOLOR | VM_VM,
			colorof((DEFUSRSTACK_32 - ctob(args->ua_size))));
#else
		args->ua_vwinadr = kvpalloc(args->ua_size, VM_UNSEQ | VM_VM, 0);
#endif
		ASSERT(args->ua_vwinadr);
		obj_bag_get_here(bag, VPROC_EXEC_TAG_STACK, args->ua_vwinadr, 
			ctob(args->ua_size), error);
		ASSERT(error == 0);

		if (args->ua_stackend) {
			offset = COMPUTE_DELTA(oldptr, args->ua_stackend);
			ASSERT(offset <= ctob(args->ua_size));
			args->ua_stackend = args->ua_vwinadr + offset;
		}

		if (args->ua_intpstkloc) {
			offset = COMPUTE_DELTA(oldptr, args->ua_intpstkloc);
			ASSERT(offset <= ctob(args->ua_size));
			args->ua_intpstkloc = args->ua_vwinadr + offset;
		}

		if (args->ua_intppsloc) {
			offset = COMPUTE_DELTA(oldptr, args->ua_intppsloc);
			ASSERT(offset <= ctob(args->ua_size));
			args->ua_intppsloc = args->ua_vwinadr + offset;
		}
	}
	if (args->ua_phdrbase) {
		args->ua_phdrbase = kern_malloc(args->ua_phdrbase_sz);
		ASSERT(args->ua_phdrbase);
		obj_bag_get_here(bag, VPROC_EXEC_TAG_PHDRBASE,
				args->ua_phdrbase, args->ua_phdrbase_sz, error);
		ASSERT(error == 0);
	}
	if (args->ua_ehdrp) {
		args->ua_ehdrp = kern_malloc(args->ua_ehdrp_sz);
		ASSERT(args->ua_ehdrp);
		obj_bag_get_here(bag, VPROC_EXEC_TAG_EHDRP, args->ua_ehdrp,
			args->ua_ehdrp_sz, error);
		ASSERT(error == 0);
	}
	if (oldptr = args->ua_idata.line1p) {
		args->ua_idata.line1p = kern_malloc(args->ua_idata.line1p_sz);
		ASSERT(args->ua_idata.line1p);
		obj_bag_get_here(bag, VPROC_EXEC_TAG_IDATA,
			args->ua_idata.line1p, args->ua_idata.line1p_sz, error);
		ASSERT(error == 0);

		offset = COMPUTE_DELTA(oldptr, args->ua_idata.intp_name);
		ASSERT(offset <= args->ua_idata.line1p_sz);
		args->ua_idata.intp_name = args->ua_idata.line1p + offset;

		offset = COMPUTE_DELTA(oldptr, args->ua_idata.intp_arg);
		ASSERT(offset <= args->ua_idata.line1p_sz);
		args->ua_idata.intp_arg = args->ua_idata.line1p + offset;
	}

	if (!IS_KUSEG(args->ua_fname)) {
		args->ua_fname = args->ua_intpfname;
	}

	args->ua_prefix_argv[0] = args->ua_idata.intp_name;
	args->ua_prefix_argv[1] = args->ua_idata.intp_arg;
	args->ua_prefix_argv[2] = args->ua_fname;
	args->ua_prefixp = args->ua_prefix_argv;

	return(error);
}

/* ARGSUSED */
PRIVATE void
job_unbag(
	struct job_s	**jobp,
	obj_bag_t	bag)
{
}

/* ARGSUSED */
PRIVATE void
cursig_unbag(
	struct sigqueue	**curinfop,
	obj_bag_t	bag)
{
}

/* ARGSUSED */
PRIVATE void
exception_unbag(
	exception_t	**expp,
	obj_bag_t	bag)
{
	/* REFERENCED */
	int	error;
	size_t	exsize = sizeof(exception_t);

	*expp = (exception_t *) kmem_alloc(exsize, KM_SLEEP);

	obj_bag_get_here(bag, VPROC_UT_TAG_EXCEPTION, *expp, exsize, error);
	ASSERT(error == OBJ_BAG_SUCCESS);
}

int
uthread_recreate(
	proc_t		*p,
	uthreadid_t	tid,
	uthread_t	*sut,
	uthread_t	**tutp)
{
	uthread_t	*tut;	/* target uthread */
	int		error, i;

	/*
	 * XXX	Change arg from proc * to proxy_t *.
	 */
	if (error = uthread_create(p, 0, tutp, tid)) {
		ASSERT(!error);
		return error;
	}

	tut = *tutp;

	UT_TO_KT(tut)->k_timers = UT_TO_KT(sut)->k_timers;

	tut->ut_proc = p;
	tut->ut_pproxy = &p->p_proxy;

	tut->ut_flid = sut->ut_flid;

	tut->ut_flags = sut->ut_flags;
	tut->ut_update = sut->ut_update;
	ASSERT(tut->ut_id == sut->ut_id);

	tut->ut_cmask = sut->ut_cmask;

	RESTRICTION(sut->ut_prda, NULL);

	tut->ut_tslice = sut->ut_tslice;
	tut->ut_policy = sut->ut_policy;

	tut->ut_cursig = sut->ut_cursig;
	tut->ut_curinfo = sut->ut_curinfo;
	tut->ut_sigpend = sut->ut_sigpend;
	tut->ut_sigwait = sut->ut_sigwait;
	tut->ut_suspmask = sut->ut_suspmask;

	copyustk(tut, sut);

	tut->ut_syscallno = sut->ut_syscallno;
	for (i = 0; i < 8; i++)
		tut->ut_scallargs[i] = sut->ut_scallargs[i];

	RESTRICTION(sut->ut_polldat, NULL);
	tut->ut_pollrotor = sut->ut_pollrotor;

	_SAT_PROC_INIT(tut, sut);

	tut->ut_cred = p->p_cred;
	crhold(tut->ut_cred);
	tut->ut_cdir = sut->ut_cdir;
	tut->ut_rdir = sut->ut_rdir;

	for (i = 0; i < ITIMER_MAX-ITIMER_UTNORMALIZE; i++)
		tut->ut_timer[i] = sut->ut_timer[i];
	HWPERF_FORK_COUNTERS(sut, tut);

	tut->ut_code = sut->ut_code;
	tut->ut_wwun = sut->ut_wwun;

	RESTRICTION(sut->ut_fdinuse, 0);
	RESTRICTION(sut->ut_fdmany, NULL);
	RESTRICTION(sut->ut_fdmanysz, 0);
	RESTRICTION(sut->ut_fdmanymax, 0);

	tut->ut_flt_cause = sut->ut_flt_cause;
	tut->ut_flt_badvaddr = sut->ut_flt_badvaddr;

#ifdef ULI
	RESTRICTION(sut->ut_uli, NULL);
#endif

	tut->ut_ecblist = sut->ut_ecblist;

	RESTRICTION(sut->ut_excpt_fr_ptr, NULL);
	tut->ut_fp_enables = sut->ut_fp_enables;
	tut->ut_fp_csr = sut->ut_fp_csr;
	tut->ut_epcinst = sut->ut_epcinst;
	tut->ut_bdinst = sut->ut_bdinst;
	tut->ut_softfp_flags = sut->ut_softfp_flags;
	tut->ut_rdnum = sut->ut_rdnum;
	tut->ut_gstate = sut->ut_gstate;

	/* Init utas */
	tut->ut_asid = sut->ut_asid;
	tut->ut_as.utas_ut = tut;
#if 0
	set_tlbpids(&tut->ut_as, (unsigned char)-1);	/* XXX reqd? */
#endif
	setup_wired_tlb_notme(&tut->ut_as, 1);
	tut->ut_as.utas_utlbmissswtch = sut->ut_as.utas_utlbmissswtch;
	tut->ut_as.utas_flag = sut->ut_as.utas_flag;
	RESTRICTION(sut->ut_watch, NULL);

	tut->ut_pblock.s_waitcnt = sut->ut_pblock.s_waitcnt;
	tut->ut_pblock.s_slpcnt = sut->ut_pblock.s_slpcnt;

	RESTRICTION(sut->ut_sharena, NULL);

	tut->ut_maxrsaid = sut->ut_maxrsaid;
	tut->ut_rsa_runable = sut->ut_rsa_runable;
	tut->ut_rsa_npgs = sut->ut_rsa_npgs;

	return 0;
}

PRIVATE void
uthread_clone_launch(
	uthread_t	*sut,
	uthread_t	*tut,
	void		(*func)(void *),
	void		*arg,
	int		is_batch)
{
	kthread_t	*tkt;	/* target kthread */
	kthread_t	*skt;	/* copy of source kthread */
	int		s;

	/*
	 * Launch the new uthread at func via
	 * sthread_launch.  Pass parameters on kernel stack.
	 */
	tkt = UT_TO_KT(tut);
	s = splhi();
	tkt->k_regs[PCB_S0] = (k_machreg_t) arg;
	tkt->k_regs[PCB_FP] = (k_smachreg_t) func;
	tkt->k_regs[PCB_PC] = (k_smachreg_t) &sthread_launch;
	tkt->k_regs[PCB_SR] = getsr();
	splx(s);

	/*
	 * Copy source kthread's scheduling stuff.
	 */
	skt = UT_TO_KT(sut);
	tkt->k_pri = skt->k_pri;
	tkt->k_basepri = skt->k_basepri;
        tkt->k_flags = skt->k_flags;
        tkt->k_cpuset = skt->k_cpuset;
        uthread_attachrunq(tkt, PDA_RUNANYWHERE, is_batch ? tut->ut_job : 0);

	s = kt_lock(tkt);
	putrunq(tkt, CPU_NONE);
	kt_unlock(tkt, s);

}

PRIVATE void
pproc_activate_uthread(
	proc_t		*p,
	uthread_t	*sut)	/* copy of source kthread */
{
	/* REFERENCED */
	int		error;
	uthread_t	*tut;	/* target uthread */
	void		*args;

	/*
	 * Create a new uthread.
	 * Note: there's no address space to care about here.
	 */
	error = uthread_recreate(p, sut->ut_id, sut, &tut);
	ASSERT(!error);

	/*
	 * Set child uthread to start at rexec_immigrate_bootstrap()
	 * to perform the SYS_rexec_complete sycall.
	 */
	args = p->p_migrate;
	p->p_migrate = NULL;		/* clone will free argument block */
	uthread_clone_launch(sut, tut,
				(void (*)(void *)) &rexec_immigrate_bootstrap,
				args,
				p->p_proxy.prxy_sched.prs_flags & PRSBATCH);

	kmem_free(sut->ut_exception, sizeof(exception_t));
	kmem_free(sut, sizeof(uthread_t));
}

PRIVATE void
exitfunc_unbag(
	struct exit_callback	**ecbp,
	obj_bag_t		bag)
{
	/* REFERENCED */
	int			error;
	struct exit_callback	*ecb;
	size_t			ecb_size = sizeof(struct exit_callback);
	obj_tag_t		tag = VPROC_PPROC_TAG_EXITFUNC;
	extern struct zone *ecb_zone;

	/*
	 * Recreate exit callback list.
	 * Note that the order will be reversed.
	 */
	*ecbp = NULL;
	do {
		/*
		 * Allocate an entry and load info from bag.
		 */
		ecb = kmem_zone_zalloc(ecb_zone, KM_SLEEP);
		error = obj_bag_get(bag, &tag, (void **) &ecb, &ecb_size);
		if (error == OBJ_BAG_ERR_TAG) {
			/*
			 * No more to get, we're done.
			 */
			kmem_zone_free(ecb_zone, ecb);
			break;
		}
		ASSERT(error == OBJ_BAG_SUCCESS);
		ASSERT(ecb_size == sizeof(struct exit_callback));

		/*
		 * Link this entry into list.
		 * No locks necessary since the parent is quiesced.
		 */
		ecb->ecb_next = *ecbp;
		*ecbp = ecb;
	} while (1); 
}

PRIVATE void
uthread_unbag(
	proc_t		*p,
	obj_bag_t	bag)
{
	/* REFERENCED */
	int		error;
	migrate_info_t	*mi = (migrate_info_t *) p->p_migrate;
	uthread_t	*sut = mi->mi_threads;
	DECL(struct vnode *, ut_cdir);
	DECL(struct vnode *, ut_rdir);

	/*
	 * Schlep in the migrating uthread. This is put in the
	 * temporarily prepared uthread struct (sut) after moving aside
	 * what's been set-up already.
	 */
	SAVE(sut, ut_cdir);
	SAVE(sut, ut_rdir);
	obj_bag_get_here(bag, VPROC_PPROC_TAG_UTHREAD,
				sut, sizeof(uthread_t), error);
	ASSERT(error == OBJ_BAG_SUCCESS);
	RESTORE(sut, ut_cdir);
	RESTORE(sut, ut_rdir);

	job_unbag(&sut->ut_job, bag);
	cursig_unbag(&sut->ut_curinfo, bag);
	exception_unbag(&sut->ut_exception, bag);
	exitfunc_unbag(&sut->ut_ecblist, bag);

	pproc_activate_uthread(p, sut);

}

/* ARGSUSED */
PRIVATE void
proxy_unbag(
	proc_proxy_t	*prxy,
	obj_bag_t	bag)
{
	/*
	 * Proxy fields related to uthreads are reset.
	 * They will be reconstucted as uthreads are recreated.
	 */
	prxy->prxy_threads = NULL;
	prxy->prxy_nthreads = 0;
}

PRIVATE void
prtrmasks_unbag(
	struct pr_tracemasks	**trmaskspp,
	obj_bag_t		bag)
{
	struct pr_tracemasks    *trmasksp;
	int			error;

	trmasksp = (struct pr_tracemasks *)
			kmem_zone_alloc(procfs_trzone, KM_SLEEP);

	obj_bag_get_here(bag, VPROC_PPROC_TAG_TRMASKS,
			 trmasksp, sizeof(struct pr_tracemasks), error);
	if (error == OBJ_BAG_ERR_TAG) {
                kmem_zone_free(procfs_trzone, trmasksp);
                *trmaskspp = NULL;
        } else {
		ASSERT(error == OBJ_BAG_SUCCESS);
		*trmaskspp = trmasksp;
	}
}

/* ARGSUSED */
PRIVATE void
pag_unbag(
	vpagg_t		**vpaggp,
	obj_bag_t	bag)
{
	/*
	 * XXX - temp til distributed
	 */
	*vpaggp = &vpagg0;
	AddToVpagRefCnt(&vpagg0, 1);
}

PRIVATE void
childlist_unbag(
	child_pidlist_t	**list,
	obj_bag_t	bag)
{
	/* REFERENCED */
	int		error;
	child_pidlist_t *cpid;
	obj_tag_t	tag = VPROC_PPROC_TAG_CHILD;
	size_t		cpid_size = sizeof(child_pidlist_t);
	extern struct zone *child_pid_zone;

	/*
	 * Recreate child list.
	 * Note that the order will be reversed.
	 */
	*list = NULL;
	do {
		/*
		 * Allocate a child entry and load info from bag.
		 */
		cpid = kmem_zone_zalloc(child_pid_zone, KM_SLEEP);
		error = obj_bag_get(bag, &tag, (void **) &cpid, &cpid_size);
		if (error == OBJ_BAG_ERR_TAG) {
			/*
			 * No more to get, we're done.
			 */
			kmem_zone_free(child_pid_zone, cpid);
			break;
		}
		ASSERT(error == OBJ_BAG_SUCCESS);
		ASSERT(cpid_size == sizeof(child_pidlist_t));

		/*
		 * Link this entry into list.
		 * No locks necessary since the parent is quiesced.
		 */
		cpid->cp_next = *list;
		*list = cpid;
	} while (1);
}


void
pproc_target_unbag(
	bhv_desc_t	*bhv,
	obj_bag_t	bag)
{
	/* REFERENCED */
	int		error;
	proc_t		*p;
	vpgrp_t		*vpgrp;
	migrate_info_t	*mi;
	DECL(struct fdt *, p_fdt);
	DECL(migrate_info_t *, p_migrate);
	DECL(bhv_desc_t, p_bhv);
	DECL(struct prnode *, p_trace);
	DECL(struct prnode *, p_pstrace);

	p = BHV_TO_PROC(bhv);

	/*
	 * Save cell-local and preset fields
	 */
	vpgrp = p->p_vpgrp;
	SAVE(p, p_fdt);
	SAVE(p, p_migrate);
	SAVE(p, p_bhv);
	SAVE(p, p_trace);
	SAVE(p, p_pstrace);

	/*
	 * Unpack the source proc en-masse.
	 */
	obj_bag_get_here(bag, VPROC_OBJ_TAG_PPROC, p, sizeof(proc_t), error);
	ASSERT(error == OBJ_BAG_SUCCESS);

	proxy_unbag(&p->p_proxy, bag);
	error = cred_bag_get(bag, &p->p_cred);
	ASSERT(!error);
	(void) uidact_incr(p->p_cred->cr_ruid);
	pag_unbag(&p->p_vpagg, bag);
	childlist_unbag(&p->p_childpids, bag);
	exitfunc_unbag(&p->p_ecblist, bag);
	prtrmasks_unbag(&p->p_trmasks, bag);

	/*
	 * Re-initialization local substructures.
	 */
	pproc_struct_init(p);

	/* Restore accounting ticks using this cell's lbolt */

	p->p_ticks = lbolt - p->p_ticks;
	
	/*
	 * Pending timer must be restarted. The time remaining
	 * was calculated on the source cell.
	 * Code here taken from alarm().
	 */
	{
		int		s;
		s = mutex_spinlock_spl(&p->p_itimerlock, splprof);
		timeout(realitexpire, p, hzto(&p->p_realtimer.it_value));
		mutex_spinunlock(&p->p_itimerlock, s);
	}

	/* 
	 * Restore cell-local and preset fields
	 */
	p->p_vpgrp = vpgrp;
	RESTORE(p, p_fdt);
	RESTORE(p, p_migrate);
	RESTORE(p, p_bhv);
	RESTORE(p, p_trace);
	RESTORE(p, p_pstrace);

	vpgrp_proc_immigrate(vpgrp, p, bag);

	prnode_proc_immigrate(p, bag);

	mi = (migrate_info_t *) p->p_migrate;
	if (mi->mi_arg)
		error = exec_args_unbag(mi->mi_arg, bag);

	uthread_unbag(p, bag);

	/* p_migrate info freed by migrated thread */
	ASSERT(p->p_migrate == NULL);
}

void
exec_args_source_end(
	struct uarg	*args)
{
	if (args->ua_vwinadr)
		kvpfree(args->ua_vwinadr, args->ua_size);

	if (args->ua_phdrbase)
		exhead_free(args->ua_phdrbase);

	if (args->ua_ehdrp)
		exhead_free((caddr_t)args->ua_ehdrp);

	if (args->ua_idata.line1p)
		exhead_free(args->ua_idata.line1p);

#if DEBUG
	bzero(args, sizeof(*args));
#endif
}

/* ARGSUSED */
PRIVATE void
proxy_source_end(
	proc_proxy_t	*prxy)
{
}

/* ARGSUSED */
PRIVATE void
cred_source_end(
	cred_t		**credp)
{
	/*
	 * Cred freed later in pproc_destroy.
	 */
}

PRIVATE void
prtrmasks_source_end(
	struct pr_tracemasks	**trmaskspp)
{
	if (*trmaskspp) {
                kmem_zone_free(procfs_trzone, *trmaskspp);
                *trmaskspp = NULL;
        }
}

/* ARGSUSED */
PRIVATE void
pag_source_end(
	vpagg_t		**vpaggp)
{
}

PRIVATE void
childlist_source_end(
	child_pidlist_t	**list)
{
	child_pidlist_t *cpid;
	extern struct zone *child_pid_zone;

	while (cpid = *list) {
		*list = cpid->cp_next;
		kmem_zone_free(child_pid_zone, cpid);
	}
}

PRIVATE void
exitfunc_source_end(
	struct exit_callback	**ecbp)
{
	struct exit_callback	*ecb;
	extern struct zone *ecb_zone;

	/*
	 * Free exit callback list entries.
	 */
	while (ecb = *ecbp) {
		*ecbp = ecb->ecb_next;
		kmem_zone_free(ecb_zone, ecb);
	}
}

PRIVATE void
uthread_source_end(
	proc_proxy_t	*prxyp)
{
	uthread_t	*ut = prxy_to_thread(prxyp);

	/*
	 * These vnode references are gone.
	 */
	ut->ut_cdir = ut->ut_rdir = NULL;

	sigqfree(&ut->ut_sigpend.s_sigqueue);

	if (ut->ut_curinfo)
		sigqueue_free(ut->ut_curinfo);

	if (ut->ut_polldat)
		(void) kern_free(ut->ut_polldat);

	if (ut->ut_cred)
		crfree(ut->ut_cred);

#ifdef R10000
	if (ut->ut_cpumon) {
		hwperf_disable_counters(ut->ut_cpumon);
		hwperf_cpu_monitor_info_free(ut->ut_cpumon);
		ut->ut_cpumon = NULL;
	}
#endif

}

void
pproc_source_end(
	bhv_desc_t	*bhv)
{
	proc_t		*p;
	migrate_info_t	*mi;

	p = BHV_TO_PROC(bhv);

	vpgrp_proc_emigrate_end(p->p_vpgrp, p);
	p->p_vpgrp = NULL;
	p->p_pgid = -1;

	prnode_proc_emigrate_end(p);

	mi = (migrate_info_t *) p->p_migrate;
	if (mi->mi_arg)
		exec_args_source_end(mi->mi_arg);

	/*
	 * Free off attachments.
	 */
	cred_source_end(&p->p_cred);
	pag_source_end(&p->p_vpagg);
	proxy_source_end(&p->p_proxy);
	childlist_source_end(&p->p_childpids);
	exitfunc_source_end(&p->p_ecblist);
	prtrmasks_source_end(&p->p_trmasks);
	fdt_free(p->p_fdt);
	p->p_fdt = NULL;
	
	uthread_source_end(&p->p_proxy);

	/*
	 * The utthread/proc can't be destroyed yet since
	 * our current thread depends on them.
	 */
}
