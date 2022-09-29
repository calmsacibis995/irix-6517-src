/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: hwperf.c
 *	hardware performance counters interface.
 */

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/cmn_err.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/pda.h>
#include <sys/kmem.h>
#include <sys/atomic_ops.h>
#include <ksys/hwperf.h>

#include "os/proc/pproc_private.h"	/* curprocp */


static struct hwperf_info {
        cpu_mon_t *hwp_owner;
	lock_t 	hwp_lock;
	ushort  hwp_mode;
	ushort  hwp_flag;
	int	hwp_count;
} hwperf_global_info;

cpu_mon_t *sys_hwperf_mon;
static volatile cpumask_t sys_cpu_mask;
static volatile cpumask_t async_cpu_mask;

hwperf_profevctraux_t	default_aux;

#define HWPERF_LOCK()	s = mutex_spinlock(&hwperf_global_info.hwp_lock);
#define HWPERF_UNLOCK() mutex_spinunlock(&hwperf_global_info.hwp_lock, s);

#define HWPERF_IS_FREE	  (hwperf_global_info.hwp_mode == HWPERF_FREE)
#define HWPERF_IS_RSVD	  (hwperf_global_info.hwp_mode == HWPERF_RSVD)
#define HWPERF_IS_PROC	  (hwperf_global_info.hwp_mode == HWPERF_PROC)
#define HWPERF_IS_SYS	  (hwperf_global_info.hwp_mode == HWPERF_SYS)

#define HWPERF_MODE_SYS   (hwperf_global_info.hwp_mode = HWPERF_SYS)
#define HWPERF_MODE_PROC  (hwperf_global_info.hwp_mode = HWPERF_PROC)

#define HWPERF_MODE_FREE  (hwperf_global_info.hwp_mode = HWPERF_FREE)
#define HWPERF_MODE_RSVD  (hwperf_global_info.hwp_mode = HWPERF_RSVD)
#define HWPERF_DECR_COUNT atomicAddInt(&hwperf_global_info.hwp_count, -1)
#define HWPERF_INCR_COUNT atomicAddInt(&hwperf_global_info.hwp_count, 1)
#define HWPERF_COUNT      (hwperf_global_info.hwp_count)
#define HWPERF_SET_OWNER(_c)	(hwperf_global_info.hwp_owner = (_c))
#define HWPERF_GET_OWNER	(hwperf_global_info.hwp_owner)

#define VALID_SIG(_sig)	  	(((_sig) >= 0) && ((_sig) < NSIG))

#define LOCK_CMP(_cmp)		mutex_lock(&(_cmp)->cm_mutex, PZERO);
#define UNLOCK_CMP(_cmp)	mutex_unlock(&(_cmp)->cm_mutex);


#define CANNOT_PREEMPT		(issplhi(getsr()))

#define HWPERF_DISABLE_CPU_CNTRS(_cpu)	\
	cpu_enabled(_cpu) ? 		\
	(pdaindr[(_cpu)].pda->p_cpu_mon->cm_flags &= ~HWPERF_CM_ENABLED) : 0

#define HWPERF_CPU_MON(_cpu)		\
	cpu_enabled(_cpu) ? (pdaindr[cpu].pda->p_cpu_mon) : 0

/* Total # of performance counters */
#define HWPERF_NUM_COUNTERS	(IS_R10000() ? 2 : 0)
/* Total # of events that can be monitored by the performance counters */
#define HWPERF_NUM_EVENTS	(IS_R10000() ? 32 : 0)


/* Some useful local routines */
static cpu_mon_t 	*hwperf_cpu_monitor_info_alloc(void);

#if defined (R10000)
static int 		hwperf_multiplex_counter_check(cpu_mon_t *);
static short 		hwperf_next_event_index_get(cpu_mon_t *,short);
#endif

/************************************************************************
 * 			Interface routines				*
 ************************************************************************/

void
hwperf_init(void)
{
        int cpu;

	/* If there are no performance counters
	 * then there is no need for cpu monitoring
	 * info.
	 */

#pragma set woff 1110	/* For this block, we can only get past this
	* for a few kernels, but there is no particularly clean way to code
	* this up for the pre-processor, so disable the check. */
	if (HWPERF_NUM_COUNTERS == 0)
		return;
	spinlock_init(&hwperf_global_info.hwp_lock, "hwperf_lock");
#pragma reset woff 1110
	hwperf_global_info.hwp_owner = NULL;
	hwperf_global_info.hwp_mode = HWPERF_FREE;
	hwperf_global_info.hwp_flag = 0;
	hwperf_global_info.hwp_count = 0;
	CPUMASK_CLRALL(async_cpu_mask);

	for (cpu = 0; cpu < maxcpus; cpu++) {
	    if (!cpu_enabled(cpu))
		continue;
	    if ((pdaindr[cpu].pda->p_cpu_mon = 
		    		hwperf_cpu_monitor_info_alloc()) == NULL)
		cmn_err(CE_PANIC,"hwperf_init out of memory");

	    ASSERT(pdaindr[cpu].pda->p_cpu_mon);
	    init_mutex(&pdaindr[cpu].pda->p_cpu_mon->cm_mutex,
		       MUTEX_DEFAULT, "hwperfcpu", cpu);
	}
#if defined (SN0)
	md_perf_init();
	io_perf_init();	
#endif  /* SN0 */
	if ((sys_hwperf_mon = hwperf_cpu_monitor_info_alloc()) == NULL)
		cmn_err(CE_PANIC,"hwperf_init out of memory");

	return;
}

/*
 * For now, comment this code out for non R10k architectures, till we 
 * get rid of other dependencies.
 */
#if defined (R10000)

int
hwperf_overflowed_intr(void)
{
	return PDA_IS_R12000 ? r12k_perf_overflow_intr():
			       r10k_perf_overflow_intr();
}


int
hwperf_enable_sys_counters(hwperf_profevctrarg_t *argp,
			   hwperf_profevctraux_t *auxp,
			   int flag,
			   int *rvalp)
{
	return hwperf_enable_counters(sys_hwperf_mon, argp, auxp,
				      flag|HWPERF_SYS, rvalp);
}

/*
 * Function	: hwperf_enable_counters
 * Parameters	:
 * Purpose	: To enable the performance counters for this process/system.
 * Assumptions	: None.
 * Returns	: 0 or errno of failure.
 */
int
hwperf_enable_counters(cpu_mon_t *cmp,
		       hwperf_profevctrarg_t *argp,
		       hwperf_profevctraux_t *auxp,
		       int flag,
		       int *rvalp)
{
	int permit, error;
	
	ASSERT(cmp && rvalp && argp);

	permit = _CAP_ABLE(CAP_DEVICE_MGT);

	if ((flag & HWPERF_SYS) && !permit)
		return EPERM;

	if (auxp == NULL) auxp = &default_aux;	

	if (!VALID_SIG(argp->hwp_ovflw_sig))
	    return EINVAL;

	/*
	 * cmp is usually a structure in the proc. To ensure no one changes
	 * it while we are mucking around, take a mutex till we are done.
	 */

	/*
	 * Most of this is machine dependent. This needs to move to 
	 * r10kperf.c/xxxperf.c by calling HWPERF_INIT_COUNTERS(cmp)
	 * which should call the arch dependent layer.
	 */

	LOCK_CMP(cmp);
	if (cmp->cm_flags & HWPERF_CM_ENABLED) {
	    UNLOCK_CMP(cmp);
	    return EBUSY;
	}

	if (error = hwperf_get_access(cmp, flag)) {
	    UNLOCK_CMP(cmp);
	    return error;
	}

	error = r1nk_perf_init_counters(cmp,argp,auxp);
	if (error) {
		hwperf_release_access(cmp);
		UNLOCK_CMP(cmp);
		return(error);
	}
	cmp->cm_sig = argp->hwp_ovflw_sig;

	cmp->cm_counting_cpu = CPU_NONE;

	if (auxp->hwp_aux_gen) 
	    cmp->cm_gen = auxp->hwp_aux_gen;
	else 
	    cmp->cm_gen = (cmp->cm_gen < 0) ? 1 : (cmp->cm_gen + 1);

	cmp->cm_flags = (flag & HWPERF_SYS) ? HWPERF_CM_SYS : HWPERF_CM_PROC;
	if (flag & HWPERF_PROFILING)
		cmp->cm_flags |= HWPERF_CM_PROFILING;
	cmp->cm_flags  |= HWPERF_CM_ENABLED;
	cmp->cm_flags  &= ~HWPERF_CM_VALID;


	hwperf_initiate_startup(cmp);

	UNLOCK_CMP(cmp);

	*rvalp = cmp->cm_gen;
	return 0;
}

int
hwperf_disable_sys_counters(void)
{
	return hwperf_disable_counters(sys_hwperf_mon);
}

/*
 * Function	: hwperf_disable_counters
 * Parameters	: 
 * Purpose	: To disable the usage of perf counters for process/system
 * Assumptions	: none
 * Returns	: 0 always.
 */
int
hwperf_disable_counters(cpu_mon_t *cmp)
{
        int cpu;

	ASSERT(cmp);

	LOCK_CMP(cmp);
	/*
	 * If relevent cm_flags are not set, counting has already been
	 * cleaned up for this cmp.
	 */
	if ((cmp->cm_flags & HWPERF_CM_EN) == 0) {
	    UNLOCK_CMP(cmp);
	    return 0;
	}
	
	cmp->cm_flags &= ~(HWPERF_CM_ENABLED|HWPERF_CM_PROFILING);
	/*
	 * If we are in system mode, disable counting on all cpu's.
	 */
	if (cmp->cm_flags & HWPERF_CM_SYS)
	    for (cpu = 0; cpu < maxcpus; cpu++)
		HWPERF_DISABLE_CPU_CNTRS(cpu);

	/*
	 * Read the counters one last time and release our hold.
	 */
	hwperf_read_counters(cmp);

	/*
	 * At this point, the cmp has been disabled. Verify that it is 
	 * no longer running on any processor.
	 */
	hwperf_check_release(cmp);

	hwperf_release_access(cmp);

	if ((cmp->cm_flags & HWPERF_CM_LOST) == 0)
	    cmp->cm_flags |= HWPERF_CM_VALID;

	cmp->cm_gen++;

	UNLOCK_CMP(cmp);

	return 0;
}

/*
 * Function	: hwperf_parent_accumulates
 * Parameters	: cpu monitor structure.
 * Purpose	: To flag the child to add its counters to the parent
 * Assumptions	: none.
 * Returns	: 0, always; current generation number, indirectly.
 * NOTES	: Not clear if this is needed. By default, child's counters
 *	          are passed on to parent always.
 *		  This is work in progress.
 */

int
hwperf_parent_accumulates(cpu_mon_t *cmp, int *rvalp)
{
	if (cmp) {
		if (cmp->cm_flags & HWPERF_CM_EN)
			cmp->cm_flags |= HWPERF_CM_PSAVE;

		*rvalp = cmp->cm_gen;
	} else
		*rvalp = -1;

	return 0;
}


int
hwperf_change_sys_control(hwperf_profevctrarg_t *argp, 
			  int flag,
			  int *rvalp)
{
	return hwperf_change_control(sys_hwperf_mon, argp, flag, rvalp);
}

/*
 * Make sure that the specified uthread and it's process proxy have a
 * CPU monitor structure allocated and return the specified uthread's
 * CPU monitor structure.
 *
 * We'd sort of like to have a cpumon_free() corresponding to cpumon_alloc()
 * and prevent other portions of the kernel from having to know that
 * cpumon_alloc() does all sorts of weird stuff with the proxy structure, etc.
 * Unfortunately, it is possible for the last uthread's proxy structure to be
 * torn down *before* it is.  This means that the uthread and proxy
 * destructors have to handle tearing down their pieces of cpumon_alloc()'s
 * legacy independently via hwperf_cpu_monitor_info_free() which should have
 * been a private routine.  We really ought to fix this in some way but it's
 * late in the IRIX 6.5 release cycle and I'm pretty sure we have plans for a
 * near complete rewrite of all of this infrastructure.  In the mean time we
 * leave this here as a reminder of what we *can't* do ...
 */
cpu_mon_t *
cpumon_alloc(uthread_t *ut)
{
	proc_proxy_t *prxy;
	cpu_mon_t *cpumon = NULL;
	int s;

	if (ut->ut_cpumon)
		return ut->ut_cpumon;

	prxy = ut->ut_pproxy;
again:

	/*
	 * check number of HW performance counters. If this would be 0,
	 * the attempt of allocating memory space for them would return
	 * NULL and cause a loop
	 */
	if (HWPERF_NUM_COUNTERS == 0) 
		cmn_err(CE_PANIC,"unexpected HWPERF_NUM_COUNTERS == 0");

	if ((cpumon = hwperf_cpu_monitor_info_alloc()) == NULL)
		goto again;

	s = prxy_lock(prxy);

	if (!ut->ut_cpumon) {
		if (!prxy->prxy_cpumon) {
			cpumon->cm_flags |= HWPERF_CM_VALID;
			/*
			 * note that this is the proxy's cpumon. We'll use
			 * this when we need to send a signal if we encounter
			 * a architecture change to make sure that only
			 * one signal is send to the process group
			 */
			cpumon->cm_mixed_cpu_flags |= 0x02;

			prxy->prxy_cpumon = cpumon;

			prxy_unlock(prxy, s);
			goto again;
		}
		ut->ut_cpumon = cpumon;
		cpumon = NULL;
	} 

	ASSERT(prxy->prxy_cpumon);
	prxy_unlock(prxy, s);

	if (cpumon) 
		hwperf_cpu_monitor_info_free(cpumon);

	return ut->ut_cpumon;
}


/*
 * Function	: hwperf_change_control
 * Parameters	: 
 * Purpose	: to restart perf counters with a different control set.
 * Assumptions	: none.
 * Returns	: 0 or error on failure.
 * NOTES	: Not clear if this is needed. The user can easily disable
 *		  and re-enable.
 */

int
hwperf_change_control(cpu_mon_t *cmp, 
			hwperf_profevctrarg_t *argp, 
			int flag,
			int *rvalp)
{
	int error;

	ASSERT(rvalp && cmp);
    
	if (error = hwperf_disable_counters(cmp))
	    return error;

	if (error = hwperf_enable_counters(cmp, argp, NULL, flag, rvalp))
	    return error;

	return 0;
}

int
hwperf_sys_control_info(hwperf_eventctrl_t *argp,
			hwperf_eventctrlaux_t *auxp,
			int *rvalp)
{
	return hwperf_control_info(sys_hwperf_mon, argp, auxp, rvalp);
}

/*
 * Function	: hwperf_control_info
 * Parameters	: 
 * Purpose	: Get the control info currently registers for this proc/sys.
 * Assumptions	: none.
 * Returns	: 0 or error on failure.
 */

int
hwperf_control_info(cpu_mon_t *cmp,
		      hwperf_eventctrl_t *argp,
		      hwperf_eventctrlaux_t *auxp,
		      int *rvalp)
{
	int		i;

	ASSERT(rvalp && cmp);

	LOCK_CMP(cmp);
	
	if ((cmp->cm_flags & HWPERF_CM_EN) == 0) {
	    UNLOCK_CMP(cmp);
	    return EINVAL;
	}
	for (i = 0; i < cmp->cm_num_events; i++)
		argp->hwp_evctrl[i].hwperf_spec = cmp->cm_evspec[i];

	if (auxp) {
		auxp->hwp_aux_sig = cmp->cm_sig;
		for (i = 0; i < cmp->cm_num_events; i++) {
		    auxp->hwp_aux_freq[i] = 
			cmp->cm_preloadcnt[i] ? 
			    HWPERF_OVERFLOW_THRESH - cmp->cm_preloadcnt[i] : 0;
		}
	}
	
	*rvalp = cmp->cm_gen;
	UNLOCK_CMP(cmp);

	return 0;
}
	

/*
 * Function	: hwperf_get_cpu_counters
 * Parameters	: 
 * Purpose	: Given a cpu, get the collected counter info
 * Assumptions	: Counters are in system mode.
 * Returns	: 0 or errno on failure.
 */


int 
hwperf_get_cpu_counters(int cpu,
			hwperf_cntr_t *argp,
			int *rvalp)
{
        cpu_mon_t *cpu_cmp;

        if (HWPERF_IS_SYS) {
	    if ((cpu_cmp = HWPERF_CPU_MON(cpu)) == NULL)
		return EINVAL;
	    return hwperf_get_counters(cpu_cmp, argp, rvalp);
	}
	return EINVAL;
}

int
hwperf_get_sys_counters(hwperf_cntr_t *argp, int *rvalp)
{
	return hwperf_get_counters(sys_hwperf_mon, argp, rvalp);
}

/*
 * Function	: hwperf_get_counters
 * Parameters	: 
 * Purpose	: Given a cmp structure, get the collected counter info.
 * Assumptions	: none
 * Returns	: 0, always.
 */
int 
hwperf_get_counters(cpu_mon_t *cmp,
		    hwperf_cntr_t *argp,
		    int *rvalp)
{
	int i;

	ASSERT(cmp && rvalp && argp);

	LOCK_CMP(cmp);

	hwperf_read_counters(cmp);

	for (i = 0; i < cmp->cm_num_events; i++) {
	    argp->hwp_evctr[i] = cmp->cm_eventcnt[i];

	    if (cmp->cm_preloadcnt[i]) {
		argp->hwp_evctr[i] += 
		    (cmp->cm_savecnt[i] - cmp->cm_preloadcnt[i]);
	    }
	}

	*rvalp = cmp->cm_gen;

	UNLOCK_CMP(cmp);
	return 0;
}

/*
 * Function	: hwperf_plus_counters
 * Parameters	: 
 * Purpose	: Given a cmp structure, add the collected counter info.
 * Assumptions	: none
 * Returns	: nothing
 */
void 
hwperf_plus_counters(cpu_mon_t *cmp,
		    hwperf_cntr_t *argp,
		    int *rvalp)
{
	int i;

	ASSERT(cmp && rvalp && argp);

	LOCK_CMP(cmp);

	hwperf_read_counters(cmp);

	for (i = 0; i < cmp->cm_num_events; i++) {
	    if (cmp->cm_preloadcnt[i]) {
		argp->hwp_evctr[i] += 
		    (cmp->cm_savecnt[i] - cmp->cm_preloadcnt[i]);
	    }
	    argp->hwp_evctr[i] += cmp->cm_eventcnt[i];
	}

	if (cmp->cm_gen > *rvalp)
		*rvalp = cmp->cm_gen;

	UNLOCK_CMP(cmp);
}


/************************************************************************
 * 			Exported  routines				*
 ************************************************************************/

/*
 * Function	: hwperf_setup_counters
 * Parameters	: 
 * Purpose	: 
 * Assumptions	: 
 * Returns	: 
 */

void
hwperf_setup_counters(cpu_mon_t *cmp)
{
	ASSERT(CANNOT_PREEMPT);
	
        ASSERT ((cmp->cm_flags & HWPERF_CM_SYS) == 0);

	if (!HWPERF_IS_PROC) {
	    cmp->cm_gen++;
	    cmp->cm_flags |= HWPERF_CM_LOST;
	    cmp->cm_flags &= ~HWPERF_CM_ENABLED;

	    return;
	}
	ASSERT(private.p_active_cpu_mon == NULL);
	private.p_active_cpu_mon = cmp;

	r1nk_perf_start_counters(cmp);
}

/*
 * Function	: hwperf_multiplex_events
 * Parameters	: cmp
 * Purpose	: To monitor more than a event on any perf counter.
 * Assumptions	: none
 * Returns	: none
 * NOTE		: This function is always called if perf counters are
 *		  enabled. If we dont need to multiplex, we update the
 *		  cmp with the current counts and return.
 */
void
hwperf_multiplex_events(cpu_mon_t *cmp)
{
	short		counter;

	ASSERT(CANNOT_PREEMPT);
	ASSERT(cmp && (cmp->cm_flags & HWPERF_CM_EN));
	ASSERT(cmp == private.p_active_cpu_mon);

	r1nk_perf_update_counters(cmp);
	
	/* Check if at aleast one hardware counter has been
	 * set for mutliplexed counting.
	 */
	if (!hwperf_multiplex_counter_check(cmp))
		return;

	if (((cmp)->cm_flags & HWPERF_CM_ENABLED) == 0)
	    return;
	
	/* Loop through each performance counter . If a hardware
	 * performance counter has been set up for mutliplexing
	 * then try to get the next event to count starting
	 * from the current event index
	 */
	for (counter = 0 ; counter < cmp->cm_num_counters; counter++) {
		if (cmp->cm_events[counter] > 1) {
			cmp->cm_evindx[counter] = 
				hwperf_next_event_index_get(cmp,counter);
		}
	}
	r1nk_perf_start_counters(cmp);

	return;
}


/*
 * Function	: hwperf_add_counters
 * Parameters	: parent (cmp2) and child (cmp1) cpu_mon_t ptrs.
 * Purpose	: To add the childs perf counters to the parent
 * Assumptions	: This is called from the context of the child.
 * Returns	: none
 * NOTES	: We dont hold any cmp locks. Use of atomic ops to 
 *		  atomically update the accumulated cm_eventcnt is
 *		  sufficient.
 */

void
hwperf_add_counters(
	cpu_mon_t 	*cmp1,
	cpu_mon_t	*cmp2)
{
	int		i;
	__uint64_t count;

	ASSERT(cmp1->cm_num_events == cmp2->cm_num_events);
	for (i = 0; i < cmp1->cm_num_events; i++) {
	    count = cmp1->cm_eventcnt[i];
	    if (cmp1->cm_preloadcnt[i])
		count += (cmp1->cm_savecnt[i] - cmp1->cm_preloadcnt[i]);
	    atomicAddUint64(&cmp2->cm_eventcnt[i], count);
	}
}


/*
 * Function	: hwperf_fork_counters
 * Parameters	: parent and child proc struct pointers
 * Purpose	: to handle fork on proc which has hwperf enabled.
 * Assumptions	: none
 * Returns	: none
 */

void
hwperf_fork_counters(uthread_t *parent, uthread_t *child)
{
        cpu_mon_t *pcmp, *ccmp;

	if (!(pcmp = parent->ut_cpumon))
		return;

	/*
	 * If this process is counting in system mode, child does not
	 * inherit.
	 */
	ASSERT((pcmp->cm_flags & HWPERF_CM_SYS) == 0);

	LOCK_CMP(pcmp);
	if ((pcmp->cm_flags & HWPERF_CM_EN) == 0) {
		UNLOCK_CMP(pcmp);
		return;
	}
	ASSERT(!child->ut_cpumon);
	ccmp = cpumon_alloc(child);
	hwperf_copy_cmp(pcmp, ccmp);
	ccmp->cm_counting_cpu = CPU_NONE;
	ccmp->cm_flags  &= ~(HWPERF_CM_VALID | HWPERF_CM_PSAVE);

	UNLOCK_CMP(pcmp);

	/*
	 * increment the count of processes using the counters.
	 */
	HWPERF_INCR_COUNT;
}


void
hwperf_intr(eframe_t *ep)
{
	r1nk_perf_intr(ep);
}


/************************************************************************
 * 			Local routines					*
 ************************************************************************/


/*
 * Function	: hwperf_sys_counters
 * Parameters	: cmp of process owning counters in system mode.
 * Purpose	: to startup a cpu in system mode.
 * Assumptions	: none 
 * Returns	: none
 */

void
hwperf_sys_counters(cpu_mon_t *cmp_arg)
{	
	int s;
	cpu_mon_t *cmp;

        s = splhi();

	if (cmp = private.p_active_cpu_mon) {
	    if (cmp->cm_flags & HWPERF_CM_ENABLED) {
		cmp->cm_gen++;
		cmp->cm_flags |= HWPERF_CM_LOST;
		cmp->cm_flags &= ~HWPERF_CM_ENABLED;
	    }
	    r1nk_perf_update_counters(cmp);
        }

	cmp = private.p_active_cpu_mon = private.p_cpu_mon;
	hwperf_copy_cmp(cmp_arg, cmp);
	cmp->cm_flags &= ~HWPERF_CM_SYS;
	cmp->cm_flags |= HWPERF_CM_CPU;
	r1nk_perf_start_counters(cmp);
	
	splx(s);
	CPUMASK_ATOMCLRB(sys_cpu_mask, private.p_cpuid);
}


/*
 * Function	: hwpperf_get_access
 * Parameters	: cmp, flag
 * Purpose	: To get access to the performance counters
 * Assumptions	: none
 * Returns	: 0 or errno on failure.
 */
int
hwperf_get_access(cpu_mon_t *cmp, int flag)
{
	int error = 0;
	int s;

	HWPERF_LOCK();
	if (flag & HWPERF_SYS) {
	    if (!HWPERF_IS_RSVD && !HWPERF_IS_FREE)
		error = EBUSY;
	    else if (HWPERF_IS_RSVD && (HWPERF_GET_OWNER != cmp))
		error = EBUSY;
	    else {
		HWPERF_SET_OWNER(cmp);
		HWPERF_MODE_SYS;
	    }
	}
	else {
	    if (!HWPERF_IS_FREE && !HWPERF_IS_PROC)
		error = EBUSY;
	    else {
		HWPERF_MODE_PROC;
		HWPERF_INCR_COUNT;
	    }
	}
	HWPERF_UNLOCK();

	return error;
}


/*
 * Function	: hwperf_check_release
 * Parameters	: cmp
 * Purpose	: To wait till the processor is no longer running this cmp.
 * Assumptions	: cmp has been disabled and processor sent cpuaction.
 * Returns	: none.
 */

void 
hwperf_check_release(cpu_mon_t *cmp)
{
        int cpu;
	cpu_mon_t *cmp_cpu;
	timespec_t delay;
	int	delay_done = 0;

	ASSERT(cmp);
	ASSERT(!(cmp->cm_flags & HWPERF_CM_ENABLED));
	ASSERT(curuthread);

	delay.tv_sec = 0;
	delay.tv_nsec = HWPERF_MAXWAIT;

	if (cmp->cm_flags & HWPERF_CM_PROC) {
	    if (cmp->cm_counting_cpu != CPU_NONE) {
		nano_delay(&delay);
		ASSERT_ALWAYS(cmp->cm_counting_cpu == CPU_NONE);
	    }
	}
	else if (cmp->cm_flags & HWPERF_CM_SYS) {
	    for (cpu = 0; cpu < maxcpus; cpu++) {
		if ((cmp_cpu = HWPERF_CPU_MON(cpu)) == NULL)
		    continue;
		if (cmp_cpu->cm_counting_cpu != CPU_NONE) {
		    if (!delay_done) {
			nano_delay(&delay);
			delay_done = 1;
		    }
		    ASSERT_ALWAYS(cmp->cm_counting_cpu == CPU_NONE);		
		}
	    }
	}
	else
	    ASSERT_ALWAYS(cmp->cm_flags & (HWPERF_CM_PROC|HWPERF_CM_SYS));

	return;
}





/*
 * Function	: hwperf_release_access
 * Parameters	: cmp
 * Purpose	: To release access acquired earlier.
 * Assumptions	: none
 * Returns	: none
 */

void
hwperf_release_access(cpu_mon_t *cmp)
{
	int s;

	ASSERT(cmp);

	HWPERF_LOCK();

	if (cmp->cm_flags & HWPERF_CM_SYS) {
	    ASSERT(HWPERF_SYS);
	    HWPERF_COUNT ?  HWPERF_MODE_PROC : HWPERF_MODE_FREE;
	    cmp->cm_flags &= ~HWPERF_CM_SYS;
	    HWPERF_SET_OWNER(NULL);
	}
	else if (cmp->cm_flags & HWPERF_CM_PROC) {
	    if (HWPERF_DECR_COUNT == 0)
		if (HWPERF_IS_PROC)
		    HWPERF_MODE_FREE;
	    cmp->cm_flags &= ~HWPERF_CM_PROC;
	}

	HWPERF_UNLOCK();

	return;
}

/*
 * Function	: hwperf_find_cpu
 * Parameters	: cmp
 * Purpose	: to find cpu on which this is running.
 * Assumptions	: this is a rough snapshot of where the cmp is running.
 *		  This routine is called to determine where a given cmp
 *		  is running and tell the cpu to update the buffers.
 *		  things such as context switches and clock ticks update
 *		  the counters automatically. So if this routine gets
 *		  preempted and we return stale/wrong data, its ok.
 * Returns	: cpu or -1 on none.
 */

int
hwperf_find_cpu(cpu_mon_t *cmp)
{
        int cpu;
	uthread_t *cpu_ut;

	for (cpu = 0; cpu < maxcpus; cpu++) {
	    if (!cpu_enabled(cpu))
		continue;

	    /* XXX BOGUS */
	    cpu_ut = pdaindr[cpu].pda->p_curuthread;
	    if (cpu_ut && (cpu_ut->ut_cpumon == cmp))
		    return cpu;
	}
	return -1;
}

/*
 * Function	: hwperf_initiate_startup
 * Parameters	: cmp
 * Purpose	: to signal that perf counters are setup and need enabling.
 * Assumptions	: none
 * Returns	: none
 */

void
hwperf_initiate_startup(cpu_mon_t *cmp)
{
        int cpu;

	ASSERT(cmp);
	
	if (cmp->cm_flags & HWPERF_CM_SYS) {
	    /*
	     * we have a lock on the system cmp. This will also
	     * ensure that sys_cpu_mask is used correctly.
	     */
	    CPUMASK_CLRALL(sys_cpu_mask);
	    for (cpu = 0; cpu < maxcpus; cpu++) {
		if (!cpu_enabled(cpu))    continue;
		CPUMASK_ATOMSETB(sys_cpu_mask, pdaindr[cpu].pda->p_cpuid);
		if (cpu == cpuid())
		    hwperf_sys_counters(cmp);
		else
		    cpuaction(cpu,
			      (cpuacfunc_t)hwperf_sys_counters, A_NOW, cmp);
	    }
	    while (CPUMASK_IS_NONZERO(sys_cpu_mask))
		;
	}
	else if ((cpu = hwperf_find_cpu(cmp)) != -1) {
	    ASSERT(cpu_enabled(cpu));
	    if (cpu == cpuid()) 
		hwperf_proc_counters();
	    else
		cpuaction(cpu, (cpuacfunc_t)hwperf_proc_counters, A_NOW);
	}
	return;
}


/*
 * Function	: hwperf_proc_counters
 * Parameters	: none
 * Purpose	: if the current cpu's uthread has counters enabled,
 *			start counting
 * Assumptions	: none
 * Returns	: none
 */

void
hwperf_proc_counters()
{
	cpu_mon_t *cmp;
	int s;

	s = splhi();

	if (curuthread &&
	    (cmp = curuthread->ut_cpumon) &&
	    (cmp->cm_flags & HWPERF_CM_ENABLED) &&
	    (private.p_active_cpu_mon == NULL)) {
		hwperf_setup_counters(cmp);
	}
	splx(s);
}


void
hwperf_update_counters(void)
{
	r1nk_perf_update_counters(NULL);
	CPUMASK_ATOMCLRB(sys_cpu_mask, private.p_cpuid);
}


void
hwperf_update_counters_async(void)
{
	r1nk_perf_update_counters(NULL);
	CPUMASK_ATOMCLRB(async_cpu_mask, private.p_cpuid);
}


/*
 * Function	: hwperf_read_counters
 * Parameters	: 
 * Purpose	: Send a signal to required cpus to update the counts
 * Assumptions	: None
 * Returns	: none
 * NOTE		: Dont wait for cpu actions to complete. It is not necessary
 *		  because the counters are updated every clock tick. 
 *		  in the worst case, the information we get is a tick out
 *		  of date, and this happens only when we take snapshots not
 *		  on process exits.
 */

void
hwperf_read_counters(cpu_mon_t *cmp)
{
	int cpu;
	int i;
	cpu_mon_t *pdacmp;

        ASSERT(cmp);

	if (cmp->cm_flags & HWPERF_CM_SYS) {
	    /*
	     * we have a lock on the system cmp. This will also
	     * ensure that sys_cpu_mask is used correctly.
	     */
	    CPUMASK_CLRALL(sys_cpu_mask);
	    for (cpu = 0; cpu < maxcpus; cpu++) {
		if (!cpu_enabled(cpu)) continue;
		CPUMASK_ATOMSETB(sys_cpu_mask, pdaindr[cpu].pda->p_cpuid);
		if (cpu == cpuid())
		    hwperf_update_counters();
		else 
		    cpuaction(cpu, 
			      (cpuacfunc_t)hwperf_update_counters,
			      A_NOW, NULL);
	    }
	    while (CPUMASK_IS_NONZERO(sys_cpu_mask))
		;

	    for (i = 0; i < cmp->cm_num_events; i++) {
		    cmp->cm_eventcnt[i] = 0;
		    cmp->cm_savecnt[i] = cmp->cm_preloadcnt[i];
	    }

	    for (cpu = 0; cpu < maxcpus; cpu++) {
		if (!cpu_enabled(cpu)) continue;
		pdacmp = HWPERF_CPU_MON(cpu);
		ASSERT(pdacmp);
		for (i = 0; i < cmp->cm_num_events; i++) {
		    cmp->cm_eventcnt[i] += pdacmp->cm_eventcnt[i];
		    if (cmp->cm_preloadcnt[i])
			cmp->cm_savecnt[i] += 
			    pdacmp->cm_savecnt[i] - pdacmp->cm_preloadcnt[i];
		}
	    }
	}
	else {
	    if ((cpu = cmp->cm_counting_cpu) != CPU_NONE) {
		ASSERT(cpu_enabled(cpu));
		if (cpu == cpuid())	
		    r1nk_perf_update_counters(NULL);
		else if (CPUMASK_ATOMTSTSETB(async_cpu_mask, pdaindr[cpu].pda->p_cpuid) == 0)
		    cpuaction(cpu, 
			      (cpuacfunc_t)hwperf_update_counters_async,
			      A_NOW, NULL);
	    }		
	}
	return;
}


/*
 * Function	: hwperf_copy_cmp
 * Parameters	: from and to, cpu monitor struct pointers.
 * Purpose	: to copy relevant fields from src to dest.
 * Assumptions	: none.
 * Returns	: none.
 */

void
hwperf_copy_cmp(cpu_mon_t *from, cpu_mon_t *to)
{
        int i;

	ASSERT(from->cm_num_events == to->cm_num_events);
	for (i = 0; i < to->cm_num_events; i++) {
	    to->cm_preloadcnt[i] = from->cm_preloadcnt[i];
	    /*
	     * we are not interested in src's savecnt and eventcnt.
	     */
	    to->cm_savecnt[i]	 = from->cm_preloadcnt[i];
	    to->cm_eventcnt[i]   = 0;
	    to->cm_evspec[i] = from->cm_evspec[i];
	}

	to->cm_gen     = from->cm_gen;
	to->cm_sig     = from->cm_sig;

	/* Copy the 	number of events ,
	 *		current event 
	 *		last event
	 * for each performance counter from the src cpu monitoring info
	 * to the destination cpu monitoring info
	 */
	for(i = 0 ; i < to->cm_num_counters; i++) {
		to->cm_events[i] = from->cm_events[i];
		to->cm_evindx[i] = from->cm_evindx[i];
		to->cm_evmax[i]  = from->cm_evmax[i];
	}
	to->cm_flags   = from->cm_flags;
}


hwperf_arch_nosupport(void)
{
	return ENOSYS;
}

void
hwperf_arch_fatal(void)
{
	cmn_err(CE_PANIC,
		"Performance counters unavailable on this architecture");
}
#else
void
hwperf_intr(eframe_t *ep)
{
	ep = ep;
	cmn_err(CE_PANIC, "hwperf_intr unexpected on this architecure");
}
#endif /* R10000 */


/* 
 * Allocate memory for the per counter information in 
 * the cpu monitor info 
 */


cpu_mon_t * 
hwperf_cpu_monitor_info_alloc(void)
{ 
	cpu_mon_t        *cm;
 	extern int perfcnt_arch_swtch_sig;      /* system tuneable, allows   */
                                                /* sending of a signal to    */
                                                /* process group in case     */
                                                /* we switched architectures */


#if defined (R10000)
	extern __uint32_t r1nk_perf_architecture(void);
#endif


#pragma set woff 1110	/* For this block, we can only get past this
	* for a few kernels, but there is no particularly clean way to code
	* this up for the pre-processor, so disable the check. */
	if (HWPERF_NUM_COUNTERS == 0)
		return NULL;

	if ((cm = kmem_zalloc(sizeof(cpu_mon_t), KM_SLEEP)) == NULL) 
		return NULL;
#pragma reset woff 1110

	cm->cm_num_counters       = HWPERF_NUM_COUNTERS;
	cm->cm_num_events         = HWPERF_NUM_EVENTS;
#if defined (R10000)
	cm->cm_mixed_cpu_started  = r1nk_perf_architecture();
#endif
	cm->cm_pgid               = curprocp ? curprocp->p_pgid : 0;

	/*
	 * we might to decide this on a per process base some time
	 * later instead of a system wide tuneable
	 */
	cm->cm_perfcnt_arch_swtch_sig = perfcnt_arch_swtch_sig;

	cm->cm_event_mask[0]     = 0x0000ffff;
	cm->cm_event_mask[1]     = 0xffff0000;

	return(cm);
}


/* Free the memory for the per counter information in the cpu monitor info */
void
hwperf_cpu_monitor_info_free(cpu_mon_t *cpu_monitor)
{
	if (cpu_monitor != NULL)
		kern_free(cpu_monitor);
}


/*
 * Check if there is atleast one hardwarecounter which is being used to 
 * count mutliple events.
 */
int
hwperf_multiplex_counter_check(cpu_mon_t *cpu_monitor)
{
	int	counter;

	for(counter = 0; counter < cpu_monitor->cm_num_counters; counter++)
		if (cpu_monitor->cm_events[counter] > 1)
			/* Multiple events are being counted using this
			 * hardware counter
			 */
			return(1);
	return(0);
}
/*
 * Searching from a given event index try to find the next event index
 * for which counting is enabled in the software on the same hardware
 * counter.
 * Basically the same hardware counter can be mutliplexed to count
 * different event types in the software.
 */
short
hwperf_next_event_index_get(cpu_mon_t	*cpu_monitor, short counter)

{
	ushort		*event_specifier;
	short 		current_event_index;
	__uint64_t	event_mask;
	short		next_event_index;
	short		max_event_index;

	event_specifier 	= cpu_monitor->cm_evspec;
	current_event_index	= cpu_monitor->cm_evindx[counter];
	event_mask 		= cpu_monitor->cm_event_mask[counter];
	max_event_index 	= cpu_monitor->cm_evmax[counter] + 1;

	/* Start at the current index */
	next_event_index = current_event_index;
	do {
		/* Wrap around on hitting the max event index */
		if (++next_event_index == max_event_index)
			next_event_index = 0;
		/* Check if the event corresponding to event index we are 
		 * looking at is being counted. 
		 */
		if (IS_HWPERF_EVENT_MASK_SET(event_mask,next_event_index) &&
		    event_specifier[next_event_index])
			break;
	} while (next_event_index != current_event_index); 
	/* The above while loop should never terminate due to 
	 * the loop check condition which has been put in to prevent 
	 * infinite looping on bad arguments. This routine 
	 * should basically get called only if there is more than
	 * event multiplexed on to the same hardware performance
	 * counter.
	 */
	ASSERT(next_event_index != current_event_index);

	return(next_event_index);
}

