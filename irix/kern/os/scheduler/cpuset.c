/*
 * FILE: irix/kern/os/scheduler/cpuset.c
 *
 * DESCRIPTION:
 *	Implements cpuset sysmp calls.
 */

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


#include <ksys/fdt.h>		/* getf() */ 
#include <ksys/vproc.h>		/* miser_request_t */

#include <sys/cred.h>		/* get_current_cred() */
#include <sys/errno.h>		/* EACCES, EBUSY, EFAULT, EINVAL, ENOMEM, 
				   ENXIO, EPERM, ESRCH, */
#include <sys/idbgentry.h>	/* qprintf() */
#include <sys/par.h>		/* MUSTRUNCPU */
#include <sys/prctl.h>		/* PR_THREADS */
#include <sys/runq.h>		/* cpu_cookie_t, setmustrun(), restoremustrun(),
				   cpu_unrestrict(), ut_endrun(), putrunq() */

#include "os/proc/pproc_private.h" /* proc_t, uscan_hold(), uscan_forloop(), 
				   swtch(), prxy_to_thread(), uscan_rele() */

/* Global Variables */
cpuset_t*       cpusets;
int16_t*        cpu_to_cpuset;
int32_t         cpuset_size;
cnodemask_t     kernel_allow_cnodemask;

#ifdef DEBUG
int CPUSET_DEBUG = 1;

/* Turns on miser sysmp call tracing on console and SYSLOG */
#define CPUSET_DEBUG(string) if (CPUSET_DEBUG) \
	{ qprintf("MISER: %s (cpu %d line %d)\n", string, cpuid(), __LINE__); }
#else
#define CPUSET_DEBUG(void);
#endif


void
cpuset_init()
/*
 * Initialize global variables and locks for cpuset during boot up.
 *
 * Called By:
 *	batch.c init_batchsys
 */
{
        int i;  /* loop counter */

        CPUSET_DEBUG("[cpuset_init]");

        cpuset_size   = maxcpus + 2;
        cpusets       = kmem_zalloc(sizeof(cpuset_t) * cpuset_size, KM_SLEEP);
        cpu_to_cpuset = kmem_zalloc(sizeof(uint16_t) * maxcpus, KM_SLEEP);

        PRIMARY_IDLE  = -1;

        for (i = 2; i < cpuset_size; i++)  {
                spinlock_init(&cpusets[i].cs_lock, "cpuset");

                cpusets[i].cs_idler     = -2;
                cpusets[i].cs_mastercpu = CPU_NONE;
        }

        for (i = 0; i < maxcpus; i++)  {
                cpusets[i].cs_lbpri = -1;
                cpu_to_cpuset[i]    = 1;
        }

        global_cpuset.cs_count     = 1;
        global_cpuset.cs_mastercpu = master_procid;

        for (i = 0; i < maxcpus; i++) {
		if (cpu_enabled(i)) {
			CPUMASK_SETB(global_cpuset.cs_node_cpumask, i);
		}
	}

        global_cpuset.cs_nodemask    = CNODEMASK_BOOTED_MASK;
        global_cpuset.cs_nodemaskptr = &global_cpuset.cs_nodemask;

        /* Initially allow kernel allocations from all nodes */
        kernel_allow_cnodemask = CNODEMASK_BOOTED_MASK;

} /* cpuset_init */


cpuset_t*
find_cpuset(id_type_t id, int* s) 
/*
 * Called By:
 *	cpuset.c miser_cpuset_create
 *	cpuset.c miser_cpuset_move_procs
 *	cpuset.c miser_cpuset_destroy
 *	cpuset.c miser_cpuset_attach
 *	cpuset.c miser_cpuset_query_cpus
 *	cpuset.c miser_cpuset_list_procs
 */
{
	int i;

	CPUSET_DEBUG("[find_cpuset]");

	for (i = 2; i < cpuset_size; i++) {
		*s = mutex_spinlock(&cpusets[i].cs_lock);

		if (cpusets[i].cs_name == id) {
			return &cpusets[i];
		}

		mutex_spinunlock(&cpusets[i].cs_lock, *s);
	}

	return 0;	/* error */

} /* find_cpuset */


void 
set_cputocpuset(cpumask_t *cmask, int cid)
/*
 * Called By:
 *	cpuset.c miser_cpuset_create
 *	cpuset.c miser_cpuset_destroy
 */
{	
	int i;

	CPUSET_DEBUG("[set_cputocpuset]");

	for (i = 0; i < maxcpus; i++) {
		if (CPUMASK_TSTB(*cmask, i)) {
			cpu_to_cpuset[i] = cid;
		}
	}

} /* set_cputocpuset */	


int
disjoint_set(cpumask_t in, cpumask_t of)
/*
 * Called By:
 *	cpuset.c miser_cpuset_create
 *	cpuset.c miser_cpuset_destroy
 */
{
	CPUSET_DEBUG("[disjoint_set]");

	CPUMASK_ANDM(in, of);
	return CPUMASK_IS_ZERO(in);

} /* disjoint_set */


void
cpumask_to_nodemask(cnodemask_t *nodemask, cpumask_t cpumask)
/* 
 * Create a node mask that represent the nodes of each cpu in the
 * a cpumask.
 *
 * NOTE: <nodemask> may point to a mask that is being used on other
 * cpus & is NOT interlocked. May sure this mask is never set to an 
 * invalid state. It is ok to set/clear bits over a period of time
 * as long as the value of a bit is valid for either the old or
 * new state. Specifically, DONT clear all bits, then reset the ones
 * that should stay set. If a bit should be set in both the old & new
 * state, it would be in an invalid "0" state for a short period of
 * time.
 *
 * Called By:
 *	cpuset.c miser_cpuset_create
 *	cpuset.c miser_cpuset_destroy
 */
{
	int		cpu;
	cnodemask_t	tmpmask;

	CPUSET_DEBUG("[cpumask_to_nodemask]");

	CNODEMASK_CLRALL(tmpmask);

	for (cpu =0; cpu < maxcpus; cpu++) {
		if (CPUMASK_TSTB(cpumask, cpu)) {
			CNODEMASK_SETB(tmpmask, pdaindr[cpu].pda->p_nodeid);
		}
	}

	*nodemask = tmpmask;

} /* cpumask_to_nodemask */


int 
restrict_cpuset(cpumask_t cmask)
/*
 * Called By:
 *	cpuset.c miser_cpuset_create
 */
{
	int   i, s;
	pda_t *npda;

	cpu_cookie_t was_running;	

	CPUSET_DEBUG("[restrict_cpuset]");

	for (i = 0; i < maxcpus; i++) {

		if (!CPUMASK_TSTB(cmask, i)) {
			continue;
		}

		npda = pdaindr[i].pda;

		s = mutex_spinlock_spl(&npda->p_special, spl7);
		npda->p_flags &= ~PDAF_ENABLED;
		mutex_spinunlock(&npda->p_special, s);

		s = splhi();
		was_running = setmustrun(i);
		cpu_restrict(i, cpu_to_cpuset[cpuid()]);
		batch_isolate(i);
		migrate_timeouts(i, clock_processor);
		restoremustrun(was_running);
		splx(s);
	} 

	return 0;	/* success */

} /* restrict_cpuset */


int 
unrestrict_cpuset(cpumask_t cmask)
/*
 * Called By:
 *	cpuset.c miser_cpuset_destroy
 */
{
	int   i, s;
	pda_t *npda; 

	CPUSET_DEBUG("[unrestrict_cpuset]");

	for (i = 0; i < maxcpus; i++) {

		if (!CPUMASK_TSTB(cmask, i)) {
			continue;
		}

		npda = pdaindr[i].pda;

		s = mutex_spinlock_spl(&npda->p_special, spl7);
		npda->p_flags |= PDAF_ENABLED;
		cpu_unrestrict(i);
		batch_unisolate(i);
		mutex_spinunlock(&npda->p_special, s);
	}

	return 0;	/* success */

} /* unrestrict_cpuset */


cpuset_t *
check_cpuset(id_type_t id)
/*
 * Called By:
 *	cpuset.c miser_cpuset_create
 */
{	
	int i;

	CPUSET_DEBUG("[check_cpuset]");

	for (i = 2; i < cpuset_size; i++) {
		if (cpusets[i].cs_name == id) {
			return &cpusets[i];
		}
	}

	return 0;	/* error */

} /* check_cpuset */
	

int 
miser_cpuset_create(miser_queue_cpuset_t* request, sysarg_t file)
/*
 * Called By:
 *	miser.c  miser_cpuset_process
 *	sysmp.c  mp_isolate
 *	sysmp.c  sysmp		-> MP_RESTRICT
 */
{
	int s, error = EBUSY, cid;
	int i = 0;

	cpumask_t cmask;
	cpuset_t  *cs, *tcs;
	vfile_t   *fp = 0;
	vnode_t   *vnode;
	cpuid_t   mastercpu = CPU_NONE;

#ifndef LARGE_CPU_COUNT
	cmask = 0;
#endif

	CPUSET_DEBUG("[miser_cpuset_create]");

	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
		return EPERM;	/* operation not permitted */
	}

	CPUMASK_CLRALL(cmask);

	if (request->mqc_count >= maxcpus) {
		return EINVAL;	/* invalid argument */
	}
		
	for (; i < request->mqc_count; i++) {

		if (request->mqc_cpuid[i] < 0 
			|| request->mqc_cpuid[i] > maxcpus 
			|| !cpu_enabled(request->mqc_cpuid[i]) 
			|| (request->mqc_cpuid[i] == master_procid 
			&& (request->mqc_flags & MISER_CPUSET_CPU_EXCLUSIVE))) {
			return ENXIO;	/* No such device or address */
		}

		if (mastercpu == CPU_NONE) {
			mastercpu = request->mqc_cpuid[i];
		}

		CPUMASK_SETB(cmask, request->mqc_cpuid[i]);

	} /* for */

	if (!(request->mqc_flags & MISER_CPUSET_KERN) 
			&& (request->mqc_queue <= maxcpus)) {
		return EINVAL;	/* invalid argument */
	}

	/* check if that cpu set already exists? */
	if (!(request->mqc_flags & MISER_CPUSET_KERN) 
			&& (cs = find_cpuset(request->mqc_queue, &s))) {
		mutex_spinunlock(&cs->cs_lock, s);
		return EBUSY;	/* resource busy */
	}

	if (!(request->mqc_flags & MISER_CPUSET_KERN)) {

		if (getf(file, &fp)) {
			return EINVAL;	/* invalid argument */
		}
		
		if (!VF_IS_VNODE(fp)) {
			return EACCES;	/* Permission denied */
		}

		vnode = VF_TO_VNODE(fp);
		VN_HOLD(vnode);

	} else {
		vnode = 0;
	}
		
	
	/* check to see if you can create cpu set */
	if (cs = find_cpuset(0, &s)) {
		nested_spinlock(&global_cpuset.cs_lock);
		nested_spinlock(&cpusets[0].cs_lock);

		if (tcs = check_cpuset(request->mqc_queue)) {
			nested_spinunlock(&global_cpuset.cs_lock);
			nested_spinunlock(&cpusets[0].cs_lock);

			/* 
			 * There is race condition existing between
			 * creating a cpuset via isolate and
			 * destroying it. This race is prevented
			 * by the isolate semaphore.
			 */
			if ((tcs->cs_flags & MISER_CPUSET_KERN) 
				&& (request->mqc_flags & MISER_CPUSET_KERN)) {
				error = 0;
			}

			mutex_spinunlock(&cs->cs_lock, s);

			return error;
		}

		if (disjoint_set(cpusets[0].cs_cpus, cmask)){ 
			if (request->mqc_flags&(MISER_CPUSET_CPU_EXCLUSIVE|
					MISER_CPUSET_KERN)) { 
				nested_spinlock(&batch_isolatedlock);	

				if ((request->mqc_count + batch_isolated +
					batch_cpus > maxcpus) ||
				!disjoint_set(global_cpuset.cs_cpus, cmask)) {
					nested_spinunlock(&batch_isolatedlock);
					nested_spinunlock(&cpusets[0].cs_lock);
				nested_spinunlock(&global_cpuset.cs_lock);
					mutex_spinunlock(&cs->cs_lock, s);

					if (vnode) {
						VN_RELE(vnode);
					}

					return EBUSY;	/* Resource busy */
				}

				CPUMASK_SETM(global_cpuset.cs_cpus, cmask);
				batch_isolated += request->mqc_count;
				nested_spinunlock(&batch_isolatedlock);	
			}
			
			CPUMASK_SETM(cs->cs_cpus, cmask);
			CPUMASK_SETM(cpusets[0].cs_cpus, cmask);

			/*
			 * Now set up the nodemasks that specify where memory 
			 * is allocated for threads using the cpuset & for the
			 * GLOBAL_CPUSET.
			 *	MISER_CPUSET_MEMORY_LOCAL
			 *		specified - use node that contain the
			 *			    cpus in the cpuset
			 *		else      - use whatever the
			 *			    GLOBAL_CPUSET can use
			 *	MISER_CPUSET_MEMORY_EXCLUSIVE
			 *		specified - delete cpus from
			 *			    GLOBAL_CPUSET cpus and
			 *			    recalculate the nodemask
                         *      MISER_CPUSET_MEMORY_KERNEL_AVOID
                         *              specified - delete nodes from
                         *                          kernel_allow_cnodemask
			 */
			if (request->mqc_flags&MISER_CPUSET_MEMORY_LOCAL) {
				CPUMASK_SETM(cs->cs_node_cpumask, cmask);
				cpumask_to_nodemask(&cs->cs_nodemask, cmask);
				cs->cs_nodemaskptr = &cs->cs_nodemask;

			} else {
				cs->cs_nodemaskptr = &global_cpuset.cs_nodemask;
			}

			if (request->mqc_flags&MISER_CPUSET_MEMORY_EXCLUSIVE) {
				CPUMASK_CLRM(global_cpuset.cs_node_cpumask,
						cmask);
				cpumask_to_nodemask(&global_cpuset.cs_nodemask,
					global_cpuset.cs_node_cpumask);
			}

                        if (request->mqc_flags&MISER_CPUSET_MEMORY_KERNEL_AVOID) {
                                cnodemask_t     removemask;
                                /* REFERENCED */
                                cnodemask_t     oldmask;
                                cpumask_to_nodemask(&removemask, cmask);
                                CNODEMASK_ATOMCLR_MASK(oldmask, kernel_allow_cnodemask,
                                                       removemask);
                        }

			ASSERT(cs->cs_idler == -2);

			cs->cs_name  = request->mqc_queue;
			cs->cs_flags = request->mqc_flags;

			if (request->mqc_flags & MISER_CPUSET_KERN) {
				cid  = 1;
			} else {
				cid = cs - &cpusets[0];
			}
			
			cs->cs_mastercpu = mastercpu;
			cs->cs_idler     = CPU_NONE;	
			cs->cs_file      = vnode;

			set_cputocpuset(&cs->cs_cpus, cid);
			error = 0;

		} /* if */

		nested_spinunlock(&global_cpuset.cs_lock);
		nested_spinunlock(&cpusets[0].cs_lock);
		mutex_spinunlock(&cs->cs_lock, s);

	} /* if */

	if (!error && (request->mqc_flags & MISER_CPUSET_CPU_EXCLUSIVE)) { 
		restrict_cpuset(cmask);
	}

	if (error && fp) { 
		VN_RELE(VF_TO_VNODE(fp));
	}

	return error;	

} /* miser_cpuset_create */


static int 
miser_cpuset_detach_proc(proc_t *p, void *arg, int mode)
/*
 * detach a process from this cpuset
 *
 * Called By:
 *	cpuset.c miser_cpuset_move_procs
 */
{
	int s;
	cpuset_t  *cs = (cpuset_t *) arg;
	uthread_t *ut;
	kthread_t *kt;

	switch (mode) {

	case 0:
		break;

	case 1:
		/* find out if the proc is in the cpuset */
		uscan_hold(&p->p_proxy);

		uscan_forloop(&p->p_proxy, ut) {
			kt = UT_TO_KT(ut);
			s  = kt_lock(kt);	

			if (cs == &cpusets[kt->k_cpuset]) {
				ASSERT(kt->k_cpuset>1);

				kt->k_cpuset 	= 1;
				kt->k_lastrun	= CPU_NONE;
				kt->k_flags	&= ~KT_HOLD;
				kt->k_flags	|= KT_NOAFF; 

				nested_spinlock(&cs->cs_lock);
				cs->cs_count--;
				nested_spinunlock(&cs->cs_lock);
			}

			kt_unlock(kt, s);
		}

		uscan_rele(&p->p_proxy);
		break;
	}

	return 0;	/* success */

} /* miser_cpuset_detach_proc */


static int 
miser_cpuset_move_procs(miser_queue_cpuset_t *request)
/*
 * find all the processes in the cpuset, and move they out of it.
 *
 * Called By:
 *	cpuset.c miser_cpuset_process
 */
{
	int s;
	cpuset_t *cs;

	CPUSET_DEBUG("[miser_cpuset_move_procs]");

	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
		return EPERM;	/* operation not permitted */
	}

	if (cs = find_cpuset(request->mqc_queue, &s)) {
		cs->cs_count++;
		mutex_spinunlock(&cs->cs_lock, s);
	} else {
		return ESRCH;	/* No such process */
	}

	/* now cs won't go away. Try to move all procs in the set. */ 
	procscan(miser_cpuset_detach_proc, cs);

	s = mutex_spinlock(&cs->cs_lock);
	cs->cs_count--;
	mutex_spinunlock(&cs->cs_lock, s);

	return 0;	/* success */

} /* miser_cpuset_move_procs */


int      
miser_cpuset_destroy(miser_queue_cpuset_t *request)
/*
 * Called By:
 *	sysmp.c  sysmp		-> MP_EMPOWER
 *	sysmp.c  mp_isolate
 *	sysmp.c  mp_unisolate
 *	cpuset.c miser_cpuset_process
 */
{
	int error = ESRCH, s;
	cpuset_t *cs;
	cpumask_t cmask, tmask;
	vnode_t *vnode = 0;

	CPUSET_DEBUG("[miser_cpuset_destroy]");

	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
		return EPERM;	/* operation not permitted */
	}

	CPUMASK_CLRALL(tmask);
	CPUMASK_CLRALL(cmask);

	if (!(request->mqc_flags & MISER_CPUSET_KERN)	&&  
			(request->mqc_queue <= maxcpus) && 
			(request->mqc_queue < 1)	||
			(request->mqc_queue == 0)) {
		return EINVAL;	/* Invalid argument */
	}

again:
	if (cs = find_cpuset(request->mqc_queue, &s)) {

		if (cs->cs_count == 0 
				|| (request->mqc_flags & MISER_CPUSET_KERN)) {


			if (!compare_and_swap_int(&cs->cs_idler, CPU_NONE, -2)){
				mutex_spinunlock(&cs->cs_lock, s);
				goto again;
			}

			nested_spinlock(&global_cpuset.cs_lock);

			if (!disjoint_set(global_cpuset.cs_cpus, cs->cs_cpus)){
				CPUMASK_SETM(cmask, cs->cs_cpus);
			}

                        if(cs->cs_flags&MISER_CPUSET_MEMORY_KERNEL_AVOID)
                        {
                                cnodemask_t addmask;
                                /* REFERENCED */
                                cnodemask_t oldmask;

                                /* A MEMORY_LOCAL cpuset already has what we
                                 * need in cs_nodemask, but other cpusets
                                 * don't, so compute the nodes to add back in.
                                 */
			        CNODEMASK_CLRALL(addmask);
                                cpumask_to_nodemask(&addmask, cs->cs_cpus);
                                CNODEMASK_ATOMSET_MASK(oldmask,
                                                       kernel_allow_cnodemask,
                                                       addmask);
                        }

			CPUMASK_CLRM(global_cpuset.cs_cpus, cs->cs_cpus);
			CPUMASK_SETM(global_cpuset.cs_node_cpumask,cs->cs_cpus);
			cpumask_to_nodemask(&global_cpuset.cs_nodemask, 
						global_cpuset.cs_node_cpumask);
			CPUMASK_CLRM(cpusets[0].cs_cpus, cs->cs_cpus);
			CPUMASK_SETM(tmask, cs->cs_cpus);
			CPUMASK_CLRALL(cs->cs_cpus);
			CPUMASK_CLRALL(cs->cs_node_cpumask);
			CNODEMASK_CLRALL(cs->cs_nodemask);

			nested_spinunlock(&global_cpuset.cs_lock);

			cs->cs_name = 0;
			vnode = cs->cs_file;
			cs->cs_file = 0;
			set_cputocpuset(&tmask, 1);	
			error = 0;

		} else { 
			error = EBUSY;	/* resource busy */
		}

		mutex_spinunlock(&cs->cs_lock, s);

		if (CPUMASK_IS_NONZERO(cmask)) {
			unrestrict_cpuset(cmask);
		}

		if (vnode) { 
			VN_RELE(vnode);
		}

	} else if (request->mqc_flags & MISER_CPUSET_KERN) { 

		if (CPUMASK_TSTB(global_cpuset.cs_cpus, request->mqc_queue)) {
			error = EPERM;
		} else {
			error = 0;
		}
	} 

	return error;

} /* miser_cpuset_destroy */ 


int 
find_cpu_in_set(cpumask_t cs)
/*
 * Called By:
 *	cpuset.c miser_cpuset_attach
 */
{
	int i = 0;

	CPUSET_DEBUG("[find_cpu_in_set]");

	for(; i < maxcpus; i++) {
		if (CPUMASK_TSTB(cs, i)) {
			return i;
		}
	}

	return -1;

} /* find_cpu_in_set */


int 
miser_cpuset_attach(miser_queue_cpuset_t* request)
/*
 * Called By:
 *	cpuset.c miser_cpuset_process
 */
{
	/* attach thread to the cpu set */
	int s, error = ESRCH;
	int index = 0; 
	cpuset_t *cs, *old_cs;
	
	int mode;	
        vpgrp_t *vpgrp;
	vproc_t *vpr;	
        int is_batch;
	vp_get_attr_t attr;

	CPUSET_DEBUG("[miser_cpuset_attach]");

	vpr = VPROC_LOOKUP(current_pid());

	if (vpr == 0) {
		return ESRCH;	/* no such process */
	}

        VPROC_GET_ATTR(vpr, VGATTR_PGID, &attr);
        VPROC_RELE(vpr);
	vpgrp = VPGRP_LOOKUP(attr.va_pgid);

        if (vpgrp) {
                VPGRP_HOLD(vpgrp);
                VPGRP_GETATTR(vpgrp, NULL, NULL, &is_batch);

                if (is_batch) {
			VPROC_RELE(vpr);
                        return EPERM;	/* Operation not permitted */
                }

                VPGRP_RELE(vpgrp);
        } else {
		return ESRCH;	/* no such process */
	}
		
	if (cs = find_cpuset(request->mqc_queue, &s)) {
		cs->cs_count++;
		mutex_spinunlock(&cs->cs_lock, s);
	} else { 
		return error;
	}

	mode = X_OK << 6;
	VOP_ACCESS(cs->cs_file, mode, get_current_cred(), error);

	if (error) {
		s = mutex_spinlock(&cs->cs_lock);
		cs->cs_count--;
		mutex_spinunlock(&cs->cs_lock, s);
		return error;
        }

	s = kt_lock(curthreadp);

	/* Not sure what else should go here */
	if (KT_ISMR(curthreadp)) {
		nested_spinlock(&cs->cs_lock);
		cs->cs_count--;
		nested_spinunlock(&cs->cs_lock);
		kt_unlock(curthreadp, s);
		return EPERM;	/* operation not permitted */
	}

	index = cs->cs_idler;

	if (index == CPU_NONE) {
		index = find_cpu_in_set(cs->cs_cpus);			
	}

	if (index == CPU_NONE) {
		kt_unlock(curthreadp, s);
		return EPERM;	/* operation not permitted */	
	}

	if(curthreadp->k_cpuset >1) { 
		/* we are going out of this cpuset to a new one.*/
		old_cs = &cpusets[curthreadp->k_cpuset];
		nested_spinlock(&old_cs->cs_lock);
		old_cs->cs_count--;
		nested_spinunlock(&old_cs->cs_lock);
	}

	curthreadp->k_cpuset = cs - &cpusets[0];
	ASSERT(index >= 0 && index < maxcpus);

	if (curthreadp->k_lastrun != CPU_NONE 
			&& CPUMASK_TSTB(cs->cs_cpus, curthreadp->k_lastrun)) {
		index = curthreadp->k_lastrun;
	}

	curthreadp->k_lastrun = index;
	curthreadp->k_flags   &= ~KT_NOAFF;
	curthreadp->k_flags   |= KT_HOLD;

	ASSERT(CPUMASK_TSTB(cs->cs_cpus, index));
	ASSERT(issplhi(getsr()));
	ASSERT(KT_ISUTHREAD(curthreadp));

	ut_endrun(KT_TO_UT(curthreadp));
	putrunq(curthreadp, index);
	kt_nested_unlock(curthreadp);
	swtch(MUSTRUNCPU);
	splx(s);

	ASSERT(!issplhi(getsr()));

	return error;

} /* miser_cpuset_attach */


int
miser_cpuset_query_cpus(miser_queue_cpuset_t* request, sysarg_t arg)
/*
 * Called By:
 *	cpuset.c miser_cpuset_process
 */
{
	int s, error = ESRCH, mode = 0;
	int index = 0, i;
	miser_request_t mr;
	miser_queue_cpuset_t *qr = 
			(miser_queue_cpuset_t*) mr.mr_req_buffer.md_data;
	cpuset_t *cs;
	
	CPUSET_DEBUG("[miser_cpuset_query_cpus]");

	if (cs = find_cpuset(request->mqc_queue, &s)) {
		cs->cs_count++;
		mutex_spinunlock(&cs->cs_lock, s);
	} else {
		return error;
	}

	error = 0;

	for (i = 0; i < maxcpus; i++) {
		if (CPUMASK_TSTB(cs->cs_cpus, i)) {
			qr->mqc_cpuid[index] = i;
			index++;
		}
	}

	mode = R_OK << 6;
	VOP_ACCESS(cs->cs_file, mode, get_current_cred(), error);

	if (error) {
		s = mutex_spinlock(&cs->cs_lock);
		cs->cs_count--;
		mutex_spinunlock(&cs->cs_lock, s);
		return error;
	}

	qr->mqc_count = index;
	
	if (copyout(&mr.mr_req_buffer, (caddr_t)(__psint_t)arg, 
			sizeof(mr.mr_req_buffer))) {
		error = EFAULT;	/* bad address */
	}

	s = mutex_spinlock(&cs->cs_lock);
	cs->cs_count--;
	mutex_spinunlock(&cs->cs_lock, s);

	return error;

} /* miser_cpuset_query_cpus */


struct cs_procs {
	cpuset_t *cs;
	pid_t    *pids;
	int      usermax;
	int      count;
};


static int 
miser_cpuset_list_proc(proc_t *p, void *arg, int mode)
/*
 * put the pid into buffer, if it is in the cpuset.
 *
 * Called By:
 *	cpuset.c miser_cpuset_list_procs
 */
{
	int s, error = 0;
	struct cs_procs *cs_proc = (struct cs_procs *) arg;
	cpuset_t *cs = cs_proc->cs;
	uthread_t *ut;
	kthread_t *kt;

	CPUSET_DEBUG("[miser_cpuset_list_proc]");

	switch (mode) {

	case 0:
		break;

	case 1:
		/* find out if the proc is in the cpuset */
		uscan_hold(&p->p_proxy);

		if ((ut = prxy_to_thread(&p->p_proxy)) == NULL) {
			uscan_rele(&p->p_proxy);
			break;
		}

		kt = UT_TO_KT(ut);
		s = kt_lock(kt);

		if (cs == &cpusets[kt->k_cpuset]) {
			ASSERT(kt->k_cpuset>1);

			if (cs_proc->pids == NULL) {
				/* just count the number of procs */
				cs_proc->count++;

			} else if (cs_proc->count >= cs_proc->usermax) {
				error = ENOMEM;	/* not enough space */

			} else {
				cs_proc->pids[cs_proc->count] = p->p_pid;
				cs_proc->count++;
			}
		}

		kt_unlock(kt, s);
		uscan_rele(&p->p_proxy);
		break;
	}

	return error;

} /* miser_cpuset_list_proc */


static int
miser_cpuset_list_procs(miser_cpuset_pid_t* request, sysarg_t arg)
/*
 * list all the processes in the cpuset.
 *
 * Called By:
 *	cpuset.c miser_cpuset_process
 */
{
	pid_t *first, *last;
	int s, error = ESRCH, mode = 0;
	miser_request_t mr;
	miser_cpuset_pid_t *mcp = 
			(miser_cpuset_pid_t *) mr.mr_req_buffer.md_data;
	cpuset_t *cs;
	struct cs_procs cs_proc;
	
	CPUSET_DEBUG("[miser_cpuset_list_procs]");

	if (cs = find_cpuset(request->mcp_queue, &s)) {
		cs->cs_count++;
		mutex_spinunlock(&cs->cs_lock, s);
	} else {
		return error;
	}

	error = 0;
	mode  = R_OK << 6;

	VOP_ACCESS(cs->cs_file, mode, get_current_cred(), error);

	if (error) {
		s = mutex_spinlock(&cs->cs_lock);
		cs->cs_count--;
		mutex_spinunlock(&cs->cs_lock, s);
		return error;
	}

	last  = (pid_t *)(&mr+1);
	first = (pid_t *)mcp->mcp_pids;
	last  = first + (last - first);

	if ((last - mcp->mcp_pids) < mcp->mcp_max_count) {
		cs_proc.usermax = last - mcp->mcp_pids;
	} else {
		cs_proc.usermax = request->mcp_max_count;
	}

	cs_proc.cs    = cs;
	cs_proc.pids  = (pid_t *)mcp->mcp_pids; 
	cs_proc.count = 0;

	error = procscan(miser_cpuset_list_proc, &cs_proc);
	s = mutex_spinlock(&cs->cs_lock);
	cs->cs_count--;
	mutex_spinunlock(&cs->cs_lock, s);

	/* copy out */
	mcp->mcp_count = cs_proc.count;

	if (copyout(&mr.mr_req_buffer, (caddr_t)(__psint_t)arg,
			sizeof(mr.mr_req_buffer))) {
		error = EFAULT;	/* bad address */
	}

	return error;

} /* miser_cpuset_list_procs */


int 
miser_cpuset_query_names(sysarg_t arg)
/*
 * Called By:
 *	cpuset.c miser_cpuset_process
 */
{
	int error = 0;
	int index = 0, i;
	int maxreq;
	miser_request_t mr;

	miser_queue_names_t *qr = (miser_queue_names_t*) 
					mr.mr_req_buffer.md_data;

	CPUSET_DEBUG("[miser_cpuset_query_names]");

	maxreq=sizeof(mr.mr_req_buffer.md_data)/sizeof(id_type_t);

	for (i = 2; i < cpuset_size && i-1 < maxreq; i++) {
		if (cpusets[i].cs_name > 0) {
			qr->mqn_queues[index] = cpusets[i].cs_name;	
			index++;
		}
	}

	qr->mqn_count = index;

	if (copyout(&mr.mr_req_buffer, (caddr_t)(__psint_t)arg,
			sizeof(mr.mr_req_buffer))) {
		error = EFAULT;	/* bad address */
	}

	return error;

} /* miser_cpuset_query_names */
	

int 
miser_cpuset_check_access(void *arg, int cpu)
/*
 * check if the thread can run on this cpu.
 * return 1 if:
 *  1. the cpu and the thread are in the same cpu set.
 *  2. OR the thread has permission to run on that set.
 *
 * Called By:	mp_mustrun()
 */
{
	kthread_t 	*kt = (kthread_t *)arg;
	int 		s, mode, error = 0;
	int		cpuset = cpu_to_cpuset[cpu];
	cpuset_t 	*cs = &cpusets[cpuset];

	CPUSET_DEBUG("[miser_cpuset_check_access]");

	ASSERT(kt);

	/* the same cpuset */
	if (cpuset == kt->k_cpuset) {
		return 1;
	}

	/* hold this cpuset */
	s = mutex_spinlock(&cs->cs_lock);
	cs->cs_count++;
	mutex_spinunlock(&cs->cs_lock, s);

	/* check if this proc has permission to access the cpuset */
	if (cs->cs_file != NULL) {
		mode = X_OK << 6;
		VOP_ACCESS(cs->cs_file, mode, get_current_cred(), error);
	}

	s = mutex_spinlock(&cs->cs_lock);
	cs->cs_count--;
	mutex_spinunlock(&cs->cs_lock, s);

	return (error == 0);	/* error */

} /* miser_cpuset_check_access */


int 
miser_cpuset_query_current(sysarg_t arg)
/*
 * Called By:
 *	cpuset.c miser_cpuset_process
 */
{
	int s, error = 0;
	int index = 0;
	miser_request_t mr;
	miser_queue_names_t *qr = (miser_queue_names_t*) 
					mr.mr_req_buffer.md_data;
	int cpuset;

	CPUSET_DEBUG("[miser_cpuset_query_current]");

	s = kt_lock(curthreadp);
	cpuset = curthreadp->k_cpuset;
	kt_unlock(curthreadp, s);

	ASSERT(cpuset >= 1 &&  cpuset < cpuset_size);

	if (cpusets[cpuset].cs_name > 0) {
		qr->mqn_queues[0] = cpusets[cpuset].cs_name;	
		index++;
	} else {
		error = ESRCH;	/* no such process */
	}

	qr->mqn_count = index;

	if (copyout(&mr.mr_req_buffer, (caddr_t)(__psint_t)arg,
			sizeof(mr.mr_req_buffer))) {
		error = EFAULT;	/* bad address */
	}

	return error;

} /* miser_cpuset_query_current */


int 
miser_cpuset_process(miser_data_t* req, sysarg_t arg, sysarg_t file)
/*
 * Called By:
 *	miser.c miser_send_request_scall
 */
{
	CPUSET_DEBUG("[miser_cpuset_process]");

	switch(req->md_request_type) {

	case MISER_CPUSET_CREATE:
		return miser_cpuset_create((miser_queue_cpuset_t*)
						req->md_data, file);
	case MISER_CPUSET_DESTROY:
		return miser_cpuset_destroy((miser_queue_cpuset_t*)
						req->md_data);
	case MISER_CPUSET_LIST_PROCS:
		return miser_cpuset_list_procs((miser_cpuset_pid_t*)
						req->md_data, arg);
	case MISER_CPUSET_MOVE_PROCS:
		return miser_cpuset_move_procs((miser_queue_cpuset_t*)
						req->md_data);
	case MISER_CPUSET_ATTACH:
		return miser_cpuset_attach((miser_queue_cpuset_t*)
						req->md_data);
	case MISER_CPUSET_QUERY_CPUS:
		return miser_cpuset_query_cpus((miser_queue_cpuset_t*)
						req->md_data, arg);
	case MISER_CPUSET_QUERY_NAMES:
		return miser_cpuset_query_names(arg);

	case MISER_CPUSET_QUERY_CURRENT:
		return miser_cpuset_query_current(arg);
	} 
	
	return EINVAL;

} /* miser_cpuset_process */


cnodemask_t
get_effective_nodemask(kthread_t *kt)
/*
 * Called By:
 *	affinity.c            memaff_getaff
 *	affinity.c            memaff_getbestnode
 *	memfit.c              memfit_selectnode
 *	memsched.c            physmem_mldset_place
 *	memsched.c            physmem_mld_place
 *	repl_control.c        repl_control
 *	repl_policy_default.c rp_default_update_coverag
 *	page.c                pagealloc_size
 */
{
	cnodemask_t mask;

	CNODEMASK_CPY(mask, kt->k_nodemask);
	CNODEMASK_ANDM(mask, *cpusets[kt->k_cpuset].cs_nodemaskptr);

	/* Some parts of the memsched subsystem cannot handle zero
	 * nodemasks. If the result of the above AND is zero, return
	 * the cpuset nodemask, since it's "administratively" higher
	 * priority.
	 */

	if (CNODEMASK_IS_ZERO(mask))
	    return(*cpusets[kt->k_cpuset].cs_nodemaskptr);
	else
	    return(mask);

} /* get_effective_nodemask */

void 
idbg_cpuset(__psint_t x)
/*
 * Called By:
 *	disp_idbg.c disp_idbg_init
 */
{
	int i, j;
	int start = 0; 
	int end = cpuset_size;

	CPUSET_DEBUG("[idbg_cpuset]");

	if (x != -1L) {
		start = (int) x;
		end = (int) x + 1;
	}

	for ( i = 0; i < maxcpus; i++) {
		qprintf("cpu_to_cpuset [%d] = %d \n", i, cpu_to_cpuset[i]);
	}

	qprintf("\n");

	for (j = start; j < end; j++) {

		if (j > 1 && CPUMASK_IS_ZERO(cpusets[j].cs_cpus)) {
			continue;
		}

		qprintf("CPUSET %d queue [%s] idler %d count %d lbpri %d "
				"master %d\n", 
				j, 
				&(cpusets[j].cs_name),
				cpusets[j].cs_idler,
				cpusets[j].cs_count,
				cpusets[j].cs_lbpri,
				cpusets[j].cs_mastercpu);

		qprintf("  CPUS:");

		for (i = 0; i < maxcpus; i++) {
			if (CPUMASK_TSTB(cpusets[j].cs_cpus, i)) {
				qprintf(" %d", i);
			}
		}

		qprintf("\n");		
		qprintf("  NODE CPUMASK:");

		for (i = 0; i < maxcpus; i++) {
			if (CPUMASK_TSTB(cpusets[j].cs_node_cpumask, i)) {
				qprintf(" %d", i);
			}
		}

		qprintf("\n");		
		qprintf("  NODEMASK:");

		if (j == 1 || cpusets[j].cs_nodemaskptr !=
				&global_cpuset.cs_nodemask) {

			for (i = 0; i < maxnodes; i++) {
				if (CNODEMASK_TSTB(cpusets[j].cs_nodemask, i)){
					qprintf(" %d", i);
				}
			}

		} else {
			qprintf(" (uses global cpuset mask)"); 
		}

		qprintf("\n");		

	} /* for */

} /* idbg_cpuset */
