/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.285 $"

/*
 * sysmp - multi-processor specific calls
 *
 * Format:
 *	sysmp(opcode, [opcode_specific_args])
 */
#include <sys/types.h>
#include <ksys/as.h>
#include <sys/buf.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/xthread.h>
#include <ksys/vfile.h>
#include <sys/ipc.h>
#include <sys/kmem.h>
#include <sys/lpage.h>
#include <sys/map.h>
#include <sys/msg.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/prctl.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/kabi.h>
#include <sys/sat.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/runq.h>
#include <sys/rt.h>
#include <sys/schedctl.h>
#include <sys/sema.h>
#include <sys/splock.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/sysmp.h>
#include <sys/sysget.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/miser_public.h>
#include <limits.h>
#include <bsd/sys/tcpipstats.h>
#include <fs/nfs/nfs_stat.h>
#include <ksys/vshm.h>
#include <ksys/vsem.h>
#include <sys/space.h>
#include <sys/mapped_kernel.h>
#include <sys/numa.h>
#include <sys/hwgraph.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include <sys/klstat.h>
#include "ksys/vm_pool.h"

#ifdef SN0
#include <sys/SN/nvram.h>
#endif

#ifdef EVEREST
#include <sys/EVEREST/nvram.h>
#endif


mutex_t mpclkchg;
extern int bufmem, chunkpages, dchunkpages, pdcount;
extern unsigned int pmapmem;

static int isolate_mustrunproc(proc_t *, void *, int);
static int unisolate_mustrunproc(proc_t *, void *, int);
static int nactiveprocs(void);

extern int sptsz, clrsz;

extern char *ketext, *kedata, *kend;
extern char _physmem_start[];
extern int _ftext[]; 
extern struct var v;
extern daddr_t swplo;
extern sema_t *semalist;
extern __int32_t avenrun[];
extern struct bsd_kernaddrs bsd_kernaddrs;

extern int cpuset_nobind; /* systune-able parameter pv: 678036 */

extern void m_allocstats(struct mbstat *);

extern int cachefs_clearstat(void);

extern void copy_distmatrix(uchar_t*);	/* XXX--no typedef for distance */
extern void build_cpumap(cnodeid_t*);

/* 
 * process that is must run on isolated processor will not be paged out, nor
 * killed by sched()
 */
#ifdef MP
#define is_isolated(ut)	(ut->ut_flags & UT_ISOLATE)
#else
#define is_isolated(ut)	(0)
#endif

lock_t kvfaultlock;

#define MPKA_OBSOLETE	((caddr_t)-1L)

/*
 * Space reduction - use this table instead of a switch statement for all
 * the MP_KERNADDR variables.
 */
caddr_t sysmp_mpka[] = {
	(caddr_t) NULL,			/* unused */
	(caddr_t) &v,			/* MPKA_VAR */
	(caddr_t) &swplo,		/* MPKA_SWPLO */
	(caddr_t) NULL,			/* MPKA_INO */
	(caddr_t) NULL,			/* MPKA_FLE */
	(caddr_t) NULL,			/* MPKA_NSPLOCK */
	(caddr_t) &defaultsemameter,	/* MPKA_SEMAMETER */
	(caddr_t) NULL,			/* MPKA_SEMAPHORE */
	(caddr_t) sizeof(struct proc),	/* MPKA_PROCSIZE */
	(caddr_t) &time,		/* MPKA_TIME */
	(caddr_t) NULL,			/* MPKA_MSG (optional) */
	(caddr_t) MPKA_OBSOLETE,	/* unused */
	(caddr_t) MPKA_OBSOLETE,	/* unused */
	(caddr_t) NULL,			/* MPKA_MSGINFO (optional) */
	(caddr_t) MPKA_OBSOLETE,	/* unused */
	(caddr_t) MPKA_OBSOLETE,	/* unused */
	(caddr_t) &splockmeter,		/* MPKA_SPLOCKMETER */
	(caddr_t) NULL,			/* MPKA_SPLOCKMETERTAB */
	(caddr_t) avenrun,		/* MPKA_AVENRUN */
	(caddr_t) &physmem,		/* MPKA_PHYSMEM */
	(caddr_t) &kpbase,		/* MPKA_KPBASE */
	(caddr_t) NULL,			/* MPKA_PFDAT */
	(caddr_t) &GLOBAL_FREEMEM_VAR,	/* MPKA_FREEMEM */
	(caddr_t) &GLOBAL_USERMEM_VAR,	/* MPKA_USERMEM */
	(caddr_t) &dchunkpages,		/* MPKA_PDWRIMEM */
	(caddr_t) &bufmem,		/* MPKA_BUFMEM */
	(caddr_t) NULL,			/* MPKA_BUF */
	(caddr_t) MPKA_OBSOLETE,	/* unused */
	(caddr_t) MPKA_OBSOLETE,	/* unused */
	(caddr_t) &chunkpages,		/* MPKA_CHUNKMEM */
	(caddr_t) &maxclick,		/* MPKA_MAXCLICK */
	(caddr_t) _physmem_start,	/* MPKA_PSTART */
	(caddr_t) _ftext,		/* MPKA_TEXT */
	(caddr_t) NULL,			/* MPKA_ETEXT */
	(caddr_t) NULL,			/* MPKA_EDATA */
	(caddr_t) NULL,			/* MPKA_END */
	(caddr_t) &syssegsz,		/* MPKA_SYSSEGSZ */
	(caddr_t) NULL,			/* MPKA_SEM_MAC */
	(caddr_t) MPKA_OBSOLETE,	/* unused */
	(caddr_t) NULL,			/* MPKA_MSG_MAC */
	(caddr_t) &bsd_kernaddrs,	/* MPKA_BSD_KERNADDRS */
};

struct sysmpa {
	sysarg_t	cmd;
	sysarg_t	arg1, arg2, arg3, arg4;
};

int
sysmp(struct sysmpa *uap, rval_t *rvp)
{
	register int s;
	int error = 0;
	/* record command for sat */
	_SAT_SET_SUBSYSNUM(uap->cmd);

	switch (uap->cmd) {
	case MP_KLSTAT:
		error = lstat_user_command(uap->arg1, (void*)uap->arg2);
		break;
	case MP_PGSIZE:		/* return system page size */
		rvp->r_val1 = ctob(1);
		break;
	case MP_SCHED:		/* scheduler control calls */
		error = schedctl(uap->arg1, (void *)uap->arg2,
				(void *)uap->arg3, rvp);
		break;
	case MP_NPROCS:		/* return # processors in complex */
		rvp->r_val1 = numcpus;
		break;
	case MP_NAPROCS:	/* return # active processors in complex */
		rvp->r_val1 = nactiveprocs();
		break;

#if defined(SN0) || defined(EVEREST)
	case MP_DISABLE_CPU: /* disable a cpu on reboot */
	{
		register pda_t *npda;
		int cpu = uap->arg1, i, nprocs = 0;

		/* check if cpu is valid and not last one enabled on reboot */
		if (cpu < 0 || cpu >= maxcpus || pdaindr[cpu].CpuId != cpu) 
		    return EINVAL;
		for (i = 0; i < maxcpus; i++)
		    if (pdaindr[i].CpuId != -1 &&
			(!(pdaindr[i].pda->p_flags & PDAF_DISABLE_CPU))) {
			nprocs++;
		}
		if(nprocs <= 1) return EINVAL;
		if(!_CAP_ABLE(CAP_NVRAM_MGT)) return EPERM;

		/* change it in nvram and update pda flags */
	        if(nvram_enable_cpu_set(cpu, 0) < 0) return EINVAL;
		npda = pdaindr[cpu].pda;
       		s = mutex_spinlock_spl(&npda->p_special, spl7);
		npda->p_flags |= PDAF_DISABLE_CPU;
		mutex_spinunlock(&npda->p_special, s);
		break;
	}
	case MP_ENABLE_CPU: /* don't disable a cpu on reboot */
	{
		register pda_t *npda;
		int cpu = uap->arg1;

		/* check if cpu num is valid */
		if (cpu < 0 || cpu >= maxcpus || pdaindr[cpu].CpuId != cpu) 
		    return EINVAL;
		if(!_CAP_ABLE(CAP_NVRAM_MGT)) return EPERM;

		/* change it in nvram and update pda flags */
	        if(nvram_enable_cpu_set(cpu, 1) < 0) return EINVAL;
		npda = pdaindr[cpu].pda;
       		s = mutex_spinlock_spl(&npda->p_special, spl7);
		npda->p_flags &= ~PDAF_DISABLE_CPU;
		mutex_spinunlock(&npda->p_special, s);
		break;
	}
#endif
	case MP_CPU_PATH: /* get the hwgraph path of cpu */
	{
	        int cpu = uap->arg1;
		
		if (cpu < 0 || cpu >= maxcpus || pdaindr[cpu].CpuId != cpu) 
		    return EINVAL;
		vertex_to_name(cpuid_to_vertex(cpu), 
			       (char *)uap->arg2, uap->arg3);
		break;
	}


	case MP_EMPOWER:	/* allow processor to run any generic process*/
	{
		/* arg1 = cpuid */
		
		miser_queue_cpuset_t cpuset;

		if (!_CAP_ABLE(CAP_SCHED_MGT))
			return EPERM;

		cpuset.mqc_queue = uap->arg1;
		cpuset.mqc_flags |= MISER_CPUSET_KERN;
		return miser_cpuset_destroy(&cpuset);
	}
	case MP_RESTRICT:	/* restrict processor to mustrun processes */
	{
		/* arg1 = cpuid */
		register int i = uap->arg1;
		miser_queue_cpuset_t cpuset;

		if (!_CAP_ABLE(CAP_SCHED_MGT))
			return EPERM;
		if (i < 0 || i >= maxcpus || i == master_procid ||
		    pdaindr[i].CpuId != i) {
			error = EINVAL;
			break;
		}
		cpuset.mqc_queue = i;
		cpuset.mqc_flags = 
			MISER_CPUSET_CPU_EXCLUSIVE | MISER_CPUSET_KERN; 
		cpuset.mqc_count = 1;
		cpuset.mqc_cpuid[0] = i;	
		miser_cpuset_create(&cpuset, 0);
		
		
		break;
	}
	case MP_ISOLATE:
	{
	    /* arg1 = cpuid */

	    if (!_CAP_ABLE(CAP_SCHED_MGT)) {
		return (EPERM);
	    }
	    return (mp_isolate((int)uap->arg1));
	}
	case MP_UNISOLATE:	
	{
	    /* arg1 = cpuid */

	    if (!_CAP_ABLE(CAP_SCHED_MGT)) {
		return (EPERM);
	    }

	    return (mp_unisolate((int)uap->arg1));
	}
	case MP_PREEMPTIVE:/* turn clock back on  on processor uap->arg1 */
	{
		register pda_t *npda;

		if (!_CAP_ABLE(CAP_SCHED_MGT))
			return EPERM;

		if (uap->arg1 < 0 ||
		    uap->arg1 >= maxcpus ||
		    pdaindr[uap->arg1].CpuId != uap->arg1) {
			error = EINVAL;
			break;
		}
		npda = pdaindr[uap->arg1].pda;
		s = mutex_spinlock_spl(&npda->p_special, spl7);
		npda->p_flags &= ~PDAF_NONPREEMPTIVE;
		mutex_spinunlock(&npda->p_special, s);
		cpuaction(uap->arg1, (cpuacfunc_t)clkstart, A_NOW);
#if DEBUG
		{
			extern unsigned int reset_aged;

			/* start all tlb flush counts from zero to avoid
			 * eronously reporting a problem with the ti_count
			 * in clean_aged_sptmap.  NOT a functionality
			 * issue, just allows more useful DEBUG WARNINGS.
			 */
			reset_aged = 1;
		}
#endif
		break;
	}
	case MP_NONPREEMPTIVE:/* turn clock off on processor uap->arg1 */
	{
		register pda_t *npda;

		if (!_CAP_ABLE(CAP_SCHED_MGT))
			return EPERM;

		if (uap->arg1 < 0 ||
		    uap->arg1 >= maxcpus ||
		    pdaindr[uap->arg1].CpuId != uap->arg1) {
			error = EINVAL;
			break;
		}

		/* fail if clock processor or not an isolated proc */
		if (pdaindr[uap->arg1].pda->p_flags & PDAF_CLOCK ||
		    !(pdaindr[uap->arg1].pda->p_flags & PDAF_ISOLATED)) {
			error = EBUSY;
			break;
		}
		npda = pdaindr[uap->arg1].pda;
		s = mutex_spinlock_spl(&npda->p_special, spl7);
		npda->p_flags |= PDAF_NONPREEMPTIVE;
		mutex_spinunlock(&npda->p_special, s);
		cpuaction(uap->arg1, (cpuacfunc_t)stopclocks, A_NOW);
		break;
	}
	case MP_FASTCLOCK:	/* uap->arg1 is the fast clock processor */
	{
		register pda_t *npda;
		int ospl;
		cpu_cookie_t	was_running;

		if (!_CAP_ABLE(CAP_TIME_MGT) && !_CAP_ABLE(CAP_SCHED_MGT))
			return EPERM;

		if (uap->arg1 < 0 ||
		    uap->arg1 >= maxcpus ||
		    pdaindr[uap->arg1].CpuId != uap->arg1) {
			error = EINVAL;
			break;
		}

		/* single thread clock changes */
		mp_mutex_lock(&mpclkchg, PZERO);
		/*
		* switch over to fast clock processor
		* this allows us to simply raise spl to
		* keep any race conditions with fast clock
		* from occurring.
		* this is to change the fast clock cpu only.
		* If the fast clock was enabled then continue to honor that.
		* On Everest, there's no separate fast clock so ifdef
		* fast clock management code out.
		*/
		was_running = setmustrun(fastclock_processor);
		ospl = spl7();
		if (private.p_flags & PDAF_FASTCLOCK) {
			private.p_flags &= ~PDAF_FASTCLOCK;
#if !EVEREST		
			/* 
			 * XXX turn these into another machine dependent call
			 * that is just a stub for Everest
			 */
			slowkgclock();
#endif
			npda = pdaindr[uap->arg1].pda;
			npda->p_flags |= PDAF_FASTCLOCK;
#if !EVEREST
			cpuaction(uap->arg1, (cpuacfunc_t)startkgclock, A_NOW);
#endif
		}
		fastclock_processor = uap->arg1;
		splx(ospl);
		restoremustrun(was_running);
		mp_mutex_unlock(&mpclkchg);
		break;
	}
	case MP_CLOCK:	/* make processor uap->arg1 the clock processor */
	{
		register pda_t *npda;
		int ospl;
		cpu_cookie_t was_running;

		if (!_CAP_ABLE(CAP_TIME_MGT) && !_CAP_ABLE(CAP_SCHED_MGT))
			return EPERM;

		if (uap->arg1 < 0 ||
		    uap->arg1 >= maxcpus ||
		    pdaindr[uap->arg1].CpuId != uap->arg1) {
			error = EINVAL;
			break;
		}

		/* single thread clock changes */
		mp_mutex_lock(&mpclkchg, PZERO);
		/*
		* switch over to clock processor
		* this allows us to simply raise spl to
		* keep any race conditions with clock()
		* from occurring, thus clock no longer
		* needs to lock the special lock
		* don't need to lock p_special to change flag
		* since spl is raised
		*/
		was_running = setmustrun(clock_processor);
		ospl = spl7();
		clock_processor = uap->arg1;	/* before turning off flag */
		private.p_flags &= ~PDAF_CLOCK;
		
		npda = pdaindr[uap->arg1].pda;
		npda->p_flags |= PDAF_CLOCK;
		splx(ospl);
		restoremustrun(was_running);
		mp_mutex_unlock(&mpclkchg);
		break;
	}

	case MP_MUSTRUN:	/* run process on uap->arg1 */

		/* A proc scheduled by miser_cpuset(1) can't bind  to a CPU */
		if(cpuset_nobind == 1) {
			if( (UT_TO_KT(curuthread)->k_cpuset) > CPUSET_GLOBAL) {
				error = EPERM;
				cmn_err(CE_ALERT,
				     "%s[%d], scheduled in a cpu-set;"
   				     "cannot bind itself to a CPU\n",
				proc_name((proc_handl_t *)curuthread->ut_proc),
				current_pid());
				break;
			}
		}
		error = mp_mustrun(0, uap->arg1, 0, 1);
		break;

	case MP_MUSTRUN_PID:	/* run process uap->arg2 on cpu uap->arg1 */

		/* A proc scheduled by miser_cpuset(1) can't bind to a CPU */
		if(cpuset_nobind == 1) {
			if( (UT_TO_KT(curuthread)->k_cpuset) > CPUSET_GLOBAL) {
				error = EPERM;
				cmn_err(CE_ALERT,
				    "%s[%d], scheduled in a cpu-set;"
   				    "cannot bind a process to a CPU\n",
				proc_name((proc_handl_t *)curuthread->ut_proc),
				current_pid());
				break;
			}
		}

		error = mp_mustrun(uap->arg2, uap->arg1, 0, 1);
		break;

	case MP_RUNANYWHERE:	/* run process anywhere */

		(void) mp_runanywhere(0,0,0);
		break;

	case MP_RUNANYWHERE_PID:	/* run process anywhere */

		error = mp_runanywhere(uap->arg1,0,0);
		break;

	case MP_GETMUSTRUN:	/* return mustrun cpu for current process */

		rvp->r_val1 = UT_TO_KT(curuthread)->k_mustrun;
		if (rvp->r_val1 == PDA_RUNANYWHERE) {
			error = EINVAL;
		}
		break;

	case MP_GETMUSTRUN_PID:		/* return mustrun cpu for process */
	{
		vproc_t *vpr;
		proc_t *procp;

		if ((vpr = VPROC_LOOKUP(uap->arg1)) == NULL) {
			error = ESRCH;
			break;
		}
		VPROC_GET_PROC(vpr, &procp);

		if (hasprocperm(procp, get_current_cred()) == 0)
			error = EPERM;
		else {
			uscan_hold(&procp->p_proxy);
			rvp->r_val1 = UT_TO_KT(prxy_to_thread(&procp->p_proxy))->k_mustrun;
			uscan_rele(&procp->p_proxy);
			if (rvp->r_val1 == PDA_RUNANYWHERE)
				error = EINVAL;
		}
		VPROC_RELE(vpr);
		break;
	}

	case MP_STAT:		/* Return processor status */
	{
		struct pda_stat	*pstat = (struct pda_stat *)uap->arg1;
		register int	i;

		for (i = 0; i < maxcpus; i++)
			if (pdaindr[i].CpuId != -1) {
				if (copyout(&pdaindr[i].CpuId,
					    &pstat->p_cpuid, sizeof(int)) ||
				    copyout(&pdaindr[i].pda->p_flags,
					    &pstat->p_flags, sizeof(int))) {
					error = EFAULT;
					break;
				}
				pstat++;
			}
		break;
	}
	case MP_KERNADDR:
		/* Do first pass initial setup */
		if (sysmp_mpka[MPKA_SPLOCKMETERTAB - 1] == NULL) {
			sysmp_mpka[MPKA_SPLOCKMETERTAB - 1] =
				(caddr_t) lockmeter_chain;
#if DISCONTIG_PHYSMEM
			/* There is no single pfdat array in this case
			 */
			sysmp_mpka[MPKA_PFDAT - 1] = (caddr_t) NULL;
#else
			sysmp_mpka[MPKA_PFDAT - 1] = (caddr_t) pfdat;
#endif
			sysmp_mpka[MPKA_BUF - 1] = (caddr_t) global_buf_table;
			sysmp_mpka[MPKA_ETEXT - 1] = ketext;
			sysmp_mpka[MPKA_EDATA - 1] = kedata;
			sysmp_mpka[MPKA_END - 1] = kend;
		}

		/* Check for out-of-range index and return error */
		if ((uap->arg1 < 1) || (uap->arg1 > MPKA_MAX)) {
			error = EINVAL;
		} else if (sysmp_mpka[uap->arg1 - 1] == MPKA_OBSOLETE) {
			error = EINVAL;
		} else if (IS_MAPPED_KERN_SPACE((__psint_t)sysmp_mpka[uap->arg1 - 1])) {
			rvp->r_val1 = MAPPED_KERN_RW_TO_K0((__psint_t)sysmp_mpka[uap->arg1 - 1]);
		} else
			rvp->r_val1 = (__psint_t)sysmp_mpka[uap->arg1 - 1];
#if _MIPS_SIM == _ABI64
		/* Chop this pointer down to size for a 32 bit
		 * app. The purpose of MP_KERNADDR is for the
		 * process to turn around and read kmem from
		 * that address. This lets 32 bit processes
		 * get to K0 seg of the 64 bit kernel.
		 */

#ifdef MAPPED_KERNEL
		/*
		 * XXX - On mapped kernels, need to make this a 32-bit
		 * K2 address!
		 */
#endif
		if (!ABI_IS_64BIT(get_current_abi()))
			rvp->r_val1 &= INT_MAX;

#endif
		break;

	case MP_SASZ: {
		size_t size;

		/* Check for out-of-range index and return error */
		if ((uap->arg1 < 1) || (uap->arg1 > SGT_MAX)) {
			error = EINVAL;
			break;
		}
		size = sysget_attr[uap->arg1 - 1].size;
		if (size == 0) {
			error = EINVAL;
			break;
		}
		rvp->r_val1 = size;
		break;
		}

	case MP_SAGET1: {
		if ((uap->arg1 < 1) || (uap->arg1 > SGT_MAX)) {
			error = EINVAL;
			break;
		}

		/* Call special sysget stub to handle this.  */

		error = sysget_mpsa1(uap->arg1, (char *)uap->arg2, uap->arg3,
			 uap->arg4);
		break;
	}
	case MP_SAGET: {
		if ((uap->arg1 < 1) || (uap->arg1 > SGT_MAX)) {
			error = EINVAL;
			break;
		}

		/* Call special sysget stub to handle this.  */

		error = sysget_mpsa(uap->arg1, (char *)uap->arg2, uap->arg3);
		break;
	}
	case MP_CLEARNFSSTAT:
		error = nfs_clearstat();
		break;
	case MP_CLEARCFSSTAT:
		error = cachefs_clearstat();
		break;

	case MP_MISER_GETREQUEST:
		return miser_get_request(uap->arg1);
	case MP_MISER_SENDREQUEST:
		return miser_send_request_scall(uap->arg1, uap->arg2);
	case MP_MISER_RESPOND:
		return miser_respond(uap->arg1);
	case MP_MISER_SETRESOURCE:
		switch (uap->arg1) {
		case MPTS_MISER_QUANTUM:
			return miser_set_quantum(uap->arg2);
		case MPTS_MISER_JOB:
			return miser_reset_job(uap->arg2);
		case MPTS_MISER_MEMORY:
		case MPTS_MISER_CPU:
			return miser_set_resources(uap->arg1, uap->arg2);
		}	
		return EINVAL;
	case MP_MISER_GETRESOURCE:
		switch (uap->arg1) {
		case MPTS_MISER_QUANTUM:
			return miser_get_quantum(uap->arg2);
		case MPTS_MISER_MEMORY:
		case MPTS_MISER_CPU:
			return batch_cpus;	
		case MPTS_MISER_BIDS:
			return miser_get_bids(uap->arg2);
		case MPTS_MISER_JOB:
			return miser_get_job(uap->arg2);
		}
		return EINVAL;
	case MP_MISER_CHECKACCESS:
		return miser_check_access(uap->arg1, uap->arg2);

#ifdef NUMA_BASE
	case MP_NUMNODES:
		rvp->r_val1 = numnodes;
		break;
	case MP_NUMA_GETDISTMATRIX:	/* node-node distance matrix */
		{ size_t dmatspace = numnodes*numnodes*sizeof (uchar_t);
		  if (uap->arg2 >= dmatspace) {
		      uchar_t* dmat = kmem_alloc(dmatspace, KM_SLEEP);
		      copy_distmatrix(dmat);
		      if (copyout(dmat, (caddr_t) uap->arg1, dmatspace))
			  error = EINVAL;
		      kmem_free(dmat, dmatspace);
		  } else
		      error = EINVAL;
		}
		break;
	case MP_NUMA_GETCPUNODEMAP:	/* cpu->node mapping table */
		{ size_t mapspace = numcpus*sizeof (cnodeid_t);
		  if (uap->arg2 >= mapspace) {
		      cnodeid_t* map = kmem_alloc(mapspace, KM_SLEEP);
		      build_cpumap(map);
		      if (copyout(map, (caddr_t) uap->arg1, mapspace))
			  error = EINVAL;
		      kmem_free(map, mapspace);
		  } else
		      error = EINVAL;
		}
		break;
#endif

	default:
		error = EINVAL;
		break;
	}
	return error;
}

/*
 * nactiveprocs - return # active processors
 */
static int
nactiveprocs(void)
{
	register int nprocs = 0;
	register int i;

	for (i = 0; i < maxcpus; i++)
		if (pdaindr[i].CpuId != -1 &&
		   (pdaindr[i].pda->p_flags & PDAF_ENABLED)) {
			nprocs++;
		}
	return(nprocs);
}


/*
 * isolateproc() - isolatereg on all proceses that must run on the isolated
 * processor
 */
static int
isolate_mustrunproc(proc_t *p, void *arg, int mode)
{
	vproc_t *vpr;
	cpuid_t cpu;
	uthread_t *ut;

	if (mode == 1) {
		/* Procscan prevents a proc struct from being deallocated,
		 * but doesn't hold a reference on the process. Thus the
		 * process can exit during this call. We use VPROC_LOOKUP
		 * to reference the process. If VPROC_LOOKUP returns null,
		 * we know the process has already begun to exit.
		 */
		vpr = VPROC_LOOKUP(p->p_pid);
		if (vpr == NULL)
			return 0;

		ASSERT(p->p_stat != 0);

		cpu = (cpuid_t)(__psunsigned_t)arg;

		uscan_hold(&p->p_proxy);
		ut = prxy_to_thread(&p->p_proxy);
		if (UT_TO_KT(ut)->k_mustrun == cpu) {
			/* If SBBLST, process won't run again, so skip */
			if ( ! (p->p_flag & SBBLST))
				isolatereg(ut, cpu);
		}
		uscan_rele(&p->p_proxy);
		VPROC_RELE(vpr);
	}

	return 0;
}

/*
 * unisolateproc() - reverse isolatereg on all proceses that must run on the 
 * isolated processor
 */
static int
unisolate_mustrunproc(proc_t *p, void *arg, int mode)
{
	vproc_t *vpr;
	cpuid_t cpu;
	uthread_t *ut;

	if (mode == 1) {
		/* Procscan prevents a proc struct from being deallocated,
		 * but doesn't hold a reference on the process. Thus the
		 * process can exit during this call. We use VPROC_LOOKUP
		 * to reference the process. If VPROC_LOOKUP returns null,
		 * we know the process has already begun to exit.
		 */
		vpr = VPROC_LOOKUP(p->p_pid);
		if (vpr == NULL)
			return 0;
		ASSERT(p->p_stat != 0);

		cpu = (cpuid_t)(__psunsigned_t)arg;

		uscan_hold(&p->p_proxy);
		ut = prxy_to_thread(&p->p_proxy);
		if (UT_TO_KT(ut)->k_mustrun == cpu) {
			/* If SBBLST, process won't run again, so skip */
			if ( !(p->p_flag & SBBLST))
				unisolatereg(ut, cpu);
		}
		uscan_rele(&p->p_proxy);
		VPROC_RELE(vpr);
	}

	return 0;
}

static struct timespec short_delay = {0,100000};

int
mp_mustrun(int pid, int cpu, int lock, int relocate)
{
	int s, unlock = 0, dequeued = 0;
	cpuid_t curcpu;
	vproc_t *vpr;
	vpgrp_t *vpgrp;
	int is_batch;
	struct proc *procp;
	struct kthread *kt;
	uthread_t *ut;

	if (cpu < 0 || cpu >= maxcpus || pdaindr[cpu].CpuId != cpu)
		return EINVAL;

	if (pid && pid != current_pid()) {
		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;
		VPROC_GET_PROC(vpr, &procp);

		/* 3rd party access not supported for multi-threaded procs */
		if (IS_THREADED(&procp->p_proxy)) {
			VPROC_RELE(vpr);
			return EINVAL;
		}

		if (hasprocperm(procp, get_current_cred()) == 0) {
			VPROC_RELE(vpr);
			return EPERM;
		}
		vpgrp = procp->p_vpgrp;
		ASSERT(vpgrp);
		unlock++;
		kt = UT_TO_KT(prxy_to_thread(&procp->p_proxy));
	} else {
		procp = curprocp;
		kt = curthreadp;
		vpgrp = procp->p_vpgrp;
	}

	if (vpgrp) {
		VPGRP_HOLD(vpgrp);
		VPGRP_GETATTR(vpgrp, NULL, NULL, &is_batch);
		if (is_batch) {
			if (unlock)
				VPROC_RELE(vpr);
			VPGRP_RELE(vpgrp);
			return EPERM;
		} 
		VPGRP_RELE(vpgrp);
	}

	/*
	 * don't allow a process to run on a different cpuset unless it
	 * has proper permission to do so.
	 */
	if(!miser_cpuset_check_access((void *)kt, cpu)) {
		return EPERM;
	}
	ut = KT_TO_UT(kt);

	curcpu = kt->k_mustrun;
	if (curcpu == (cpuid_t)cpu) {
		/* If a proc is locked to a cpu without its knowlege,
		 * and then does a mustrun to this cpu, allow children
		 * to inherit this proc's mustrun value.
		 */
		s = ut_lock(ut);
		if (lock)
			ut->ut_flags |= UT_MUSTRUNLCK;
		else if (ut->ut_flags & UT_MUSTRUNLCK)
			ut->ut_flags &= ~UT_NOMRUNINH;
		ut_unlock(ut, s);

		if (relocate) {
			PMO_PROCESS_MUSTRUN_RELOCATE(ut, cputocnode(cpu));
		}
		if (unlock)
			VPROC_RELE(vpr);
		return 0;
	}

	/* Already forced to run on the current cpu, can't be moved. */
	if (ut->ut_flags & UT_MUSTRUNLCK) {
		if (unlock)
			VPROC_RELE(vpr);
		return EINVAL;
	}

	if (is_isolated(ut))
		unisolatereg(ut, PDA_RUNANYWHERE);

	if (pdaindr[cpu].pda->p_flags & PDAF_ISOLATED)
		isolatereg(ut, (cpuid_t)cpu);
retry:
	s = kt_lock(kt);
	if (KT_ISKB(kt)) {
		kt_unlock(kt, s);
		nano_delay(&short_delay);
		goto retry;
	}
	if (kt->k_onrq != CPU_NONE && kt->k_onrq != cpu) {
		if (removerunq(kt))
			dequeued++;
		else {
			kt_unlock(kt, s);
			goto retry;
		}
	}
	ASSERT(!(kt->k_flags & KT_PSPIN));
	if (KT_TO_UT(kt)->ut_gstate != GANG_NONE &&
		KT_TO_UT(kt)->ut_gstate != GANG_UNDEF) 
		kt->k_binding = CPU_NONE;
	kt->k_mustrun = cpu;
	if ((kt->k_flags & KT_BIND) && kt->k_binding != kt->k_mustrun)
		rt_rebind(kt);
	if (dequeued)
		putrunq(kt, CPU_NONE);
	 else {
		/* if third party, make him look at the mustrun */
		cpuid_t	oncpu = kt->k_sonproc;
		if (kt != curthreadp && oncpu != CPU_NONE)
			CPUVACTION_RESCHED(oncpu);
	}
	kt_unlock(kt, s);
	if (kt == curthreadp && private.p_cpuid != cpu)
		qswtch(MUSTRUNCPU);

	if (pdaindr[cpu].pda->p_flags & PDAF_ISOLATED)
		/* flush random base for cachectl/tlbclean/mapfile case */
		da_flush_tlb();

	if (lock) {
		s = ut_lock(ut);

		/* If the mustrun is being done without the
		 * proc's explicit knowlege (like in this case)
		 * we don't want to surprise the user by pinning
		 * the children of this process to a particular cpu.
		 */
		ut->ut_flags |= UT_MUSTRUNLCK |
				(curcpu == PDA_RUNANYWHERE ? UT_NOMRUNINH : 0);
		ut_unlock(ut, s);
	}
	if (relocate) {
		PMO_PROCESS_MUSTRUN_RELOCATE(ut, cputocnode(cpu));
	}
	if (unlock)
		VPROC_RELE(vpr);
	return 0;
}

int
mp_runanywhere(int pid, int lock, int lockonly)
{
	int unlock = 0;
	vproc_t *vpr;
	struct proc *procp;
	kthread_t *kt;

	if (lock) {
		ASSERT(pid == 0);
		ASSERT(curuthread->ut_flags & UT_MUSTRUNLCK);
		ut_flagclr(curuthread, UT_MUSTRUNLCK);

		if (lockonly)
			return 0;
	}

	if (pid && pid != current_pid()) {
		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;
		VPROC_GET_PROC(vpr, &procp);

		/* 3rd party access not supported for multi-threaded procs */
		if (IS_THREADED(&procp->p_proxy)) {
			VPROC_RELE(vpr);
			return EINVAL;
		}

		if (hasprocperm(procp, get_current_cred()) == 0) {
			VPROC_RELE(vpr);
			return EPERM;
		}

		unlock++;
		kt = UT_TO_KT(prxy_to_thread(&procp->p_proxy));
	} else {
		procp = curprocp;
		kt = curthreadp;
	}

	mp_runthread_anywhere(kt);

	if (unlock)
		VPROC_RELE(vpr);

	return 0;
}

void
mp_runthread_anywhere(kthread_t *kt)
{
	int s;

	ASSERT(kt);

	if (is_isolated(KT_TO_UT(kt))) {	/* undo Isolated state */
		unisolatereg(KT_TO_UT(kt), PDA_RUNANYWHERE);
	}

	s = kt_lock(kt);
	kt->k_mustrun = PDA_RUNANYWHERE;
	if ((kt->k_flags & KT_BIND) && kt->k_binding != CPU_NONE &&
	    pdaindr[kt->k_binding].pda->p_cpu.c_restricted)
		rt_rebind(kt);
	kt_unlock(kt, s);

	if (kt == curthreadp && !(private.p_flags & PDAF_ENABLED))
		qswtch(MUSTRUNCPU);
}

/* 
 * isolate processor from
 * any broadcast tlbflush, cacheflush.
 */

extern int batch_isolated;
extern int batch_cpus;
extern lock_t batch_isolatedlock;	

int
mp_isolate(int cpu)
{
	extern mutex_t isolate_sema;
	extern cpumask_t isolate_cpumask;
	cpu_cookie_t was_running;
	int was_isolated;
	int s;
	size_t size;
	caddr_t temp_kvfault;
#ifdef R4000
	size_t sz;
	int j;
	caddr_t temp_clrkvflt[CACHECOLORSIZE+1];
#endif
	int error = 0;
	static int inited = 0;
	miser_queue_cpuset_t cpuset;

	if (cpu < 0 || cpu >= maxcpus || cpu == master_procid ||
	    pdaindr[cpu].CpuId != cpu) {
		return (EINVAL);
	}

	ASSERT(!KT_ISKB(curthreadp));
	was_running = setmustrun(cpu);
	mutex_lock(&isolate_sema, PZERO);

	cpuset.mqc_queue = cpu;
	cpuset.mqc_flags = MISER_CPUSET_CPU_EXCLUSIVE | MISER_CPUSET_KERN;
	cpuset.mqc_count = 1;
	cpuset.mqc_cpuid[0] = cpu;
	if (miser_cpuset_create(&cpuset, 0)) {
		mutex_unlock(&isolate_sema);
		restoremustrun(was_running);
		return EBUSY;
	}

	if (private.p_flags & PDAF_ISOLATED) {
		mutex_unlock(&isolate_sema);
		goto isolate_out;
	}
	was_isolated = is_isolated(curuthread);

	/*
	 * The first call to mp_isolate needs to allocate a lock to
	 * protect the kvfault structure. It is needed as protection for a
	 * race between kvfaultcheck, and mp_unisolate. We allocate it here
	 * because we know the lock will not be needed until after we
	 * have isolated our first cpu, and do not want to allocate the
	 * lock unless we are going to use it. 
	 */
	if (!inited) {
		inited = 1;
		spinlock_init(&kvfaultlock, "kvfaultlock");
	}
	/* 
	 * if there is only one processor available then we cannot isolate;
	 * to be safe zap out all the wired entries on the isolated proc.
	 */
	if (procscan(isolate_mustrunproc, (void *)(__psint_t)cpu) == 1 ||
	    (nactiveprocs() == 1 && 
	     (private.p_flags & PDAF_ENABLED)) ) {
		(void)procscan(unisolate_mustrunproc, (void *)(__psint_t)cpu);
		mutex_unlock(&isolate_sema);
		error = EBUSY;
		goto isolate_out;
	}
	/* let the scheduler know - don't schedule folks anymore */
	cpu_restrict(cpu, 1);

	ASSERT(private.p_kvfault == 0);
	size = (sptsz + NBBY - 1) / NBBY;
	temp_kvfault = (caddr_t) kmem_alloc(size, VM_DIRECT);
#ifdef R4000
	sz = clrsz / NBBY;
	for (j=0; j < CACHECOLORSIZE; j++)
	  temp_clrkvflt[j] = (caddr_t) kmem_alloc(sz, VM_DIRECT);
#endif
	/* p_special protects p_flags, spl7 protects against prfintr */
	s = mutex_spinlock_spl(&private.p_special, spl7);
	private.p_flags &= ~PDAF_ENABLED;
	private.p_flags |= PDAF_ISOLATED;
	private.p_kvfault = temp_kvfault;
#ifdef R4000
	for (j=0; j < CACHECOLORSIZE; j++)
	  private.p_clrkvflt[j] = temp_clrkvflt[j];
#endif
	mutex_spinunlock(&private.p_special, s);

	CPUMASK_SETB(isolate_cpumask, cpuid());

#ifdef MP
	if (was_running.cpu != cpu)
#else
	if (was_running != cpu)
#endif
	{
		/*
		 * process got isolated onto this cpu and shouldn't have,
		 * undo it now
		 */
		if (!was_isolated)
			unisolatereg(curuthread, PDA_RUNANYWHERE);
	}
	mutex_unlock(&isolate_sema);

	/* reduce chance of future flush, flush random base */
	da_flush_tlb();

#if TLB_TRACE && DEBUG
	{ extern void testkvfault(void);
	testkvfault();
	}
#endif

isolate_out:
	/*
	 * We now need to migrate all the timeouts that are already on
	 * the processor away so that they will not interfere with 
	 * realtime programs
	 */

	restoremustrun(was_running);

	if (error) {  
		cpuset.mqc_queue = cpu;
		cpuset.mqc_flags = MISER_CPUSET_KERN;
		cpuset.mqc_count = 0;
		cpuset.mqc_cpuid[0] = -1;
		miser_cpuset_destroy(&cpuset);
	}
	return (error);
}


int
mp_unisolate(int cpu)
{
	extern mutex_t isolate_sema;
	extern cpumask_t isolate_cpumask;
	cpu_cookie_t was_running;
	int s;
	size_t size;
	miser_queue_cpuset_t cpuset;
#ifdef R4000
	int j;
	size_t sz;
#endif

	if (cpu < 0 || cpu == master_procid || 
		cpu >= maxcpus || pdaindr[cpu].CpuId != cpu) {
		return (EINVAL);
	}
	was_running = setmustrun(cpu);
	mutex_lock(&isolate_sema, PZERO);
	if (!(private.p_flags & PDAF_ISOLATED)) {
		mutex_unlock(&isolate_sema);
		restoremustrun(was_running);
		return (0);
	}

	(void)procscan(unisolate_mustrunproc, (void *)(__psint_t)cpu);

	/* p_special protects p_flags, spl7 protects nobroadcast */
	s = mutex_spinlock_spl(&private.p_special, spl7);
	private.p_flags &= ~(PDAF_ISOLATED|PDAF_BROADCAST_OFF);
	private.p_flags |= PDAF_ENABLED;
	private.p_delayacvec = 0;
	mutex_spinunlock(&private.p_special, s);
	size = (sptsz + NBBY - 1) / NBBY;
	s = mutex_spinlock_spl(&kvfaultlock, spl7);
	kmem_free(private.p_kvfault, size);
	private.p_kvfault = 0;
#ifdef R4000
	sz = clrsz / NBBY;
	for (j=0; j < CACHECOLORSIZE; j++) {
		kmem_free(private.p_clrkvflt[j], sz);
		private.p_clrkvflt[j] = 0;
	}
#endif
	mutex_spinunlock(&kvfaultlock, s);

	CPUMASK_CLRB(isolate_cpumask, cpuid());
	cpuset.mqc_queue = cpu;
	cpuset.mqc_flags = MISER_CPUSET_KERN;
	miser_cpuset_destroy(&cpuset);

	mutex_unlock(&isolate_sema);

	da_flush_tlb();

	cpu_unrestrict(cpu);
	restoremustrun(was_running);
	return (0);
}
