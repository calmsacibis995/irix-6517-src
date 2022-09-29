/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if defined(EVEREST) || defined(SN0) || defined(IP30)

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/runq.h>
#include <sys/sysmp.h>
#include <sys/schedctl.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/frs.h>
#include <sys/clksupport.h>

#include "frame_base.h"
#include "frame_cpu.h"
#include "frame_process.h"
#include "frame_procd.h"
#include "frame_minor.h"
#include "frame_major.h"
#include "frame_sched.h"
#include "frame_debug.h"

extern void initcounter(int);
extern void delaycounter(void);
void frs_cpu_frspda_print(cpuid_t);

#if defined(IP30)
intrgroup_t fake_igroup;
#endif

/*
 * Per cpu frs object handles
 */
static frs_pda_t frs_pda[FRS_MAX_CPUS];

/*
 * Driver Table
 */
frs_drvtable_t frs_drvtable;

/*
 * Allocate a per cpu frame scheduler parent object.
 * This routine is invoked at boot time to allocate
 * a frame scheduler parent object for each pda.
 * If the frame scheduler is not installed, a stub
 * routine gets called instead.  <(stubs/frsstubs.c)>
 */

void*
frs_alloc(cpuid_t cpu)
{
	ASSERT (cpu < FRS_MAX_CPUS);
        spinlock_init(&frs_pda[cpu].lock, "frs_pda_lock");
        frs_pda[cpu].resv = 0;
	frs_pda[cpu].frs = NULL;
	return ((void*)&frs_pda[cpu]);
}

int
frs_cpu_verifyvalid(cpuid_t cpu)
{
        if (cpu < 0 || cpu > maxcpus || pdaindr[cpu].CpuId != cpu) {
                return (FRS_ERROR_CPU_NEXIST);
	}

	return (0);
}

int
frs_cpu_verifynotclock(cpuid_t cpu)
{
    	if (pdaindr[cpu].pda->p_flags & PDAF_CLOCK) {
                return (FRS_ERROR_CPU_CLOCK);
	}

	return (0);
}

int
frs_cpu_reservepda(cpuid_t cpu)
{
        frs_pda_t* frs_objp;
        int pda_ospl;

        frs_objp = pdaindr[cpu].pda->p_frs_objp;
        ASSERT (frs_objp != NULL);

        pda_ospl = frs_cpu_pdalock(frs_objp);
        if (frs_objp->resv || frs_objp->frs != NULL) {
                frs_cpu_pdaunlock(frs_objp, pda_ospl);
                return (FRS_ERROR_CPU_BUSY);
        }
        frs_objp->resv = 1;
        frs_cpu_pdaunlock(frs_objp, pda_ospl);
        return (0);
}

void
frs_cpu_unreservepda(cpuid_t cpu)
{
        frs_pda_t* frs_objp;
        int pda_ospl;

        frs_objp = pdaindr[cpu].pda->p_frs_objp;
        ASSERT (frs_objp != NULL);

        pda_ospl = frs_cpu_pdalock(frs_objp);
        ASSERT (frs_objp->resv);
        ASSERT (frs_objp->frs == NULL);
        frs_objp->resv = 0;
        frs_cpu_pdaunlock(frs_objp, pda_ospl);
}

int
frs_cpu_linkpda(cpuid_t cpu, struct frs_fsched* frs)
{
        frs_pda_t* frs_objp;
        int pda_ospl;

        atomicSetInt((int*)&pdaindr[cpu].pda->p_frs_flags, 0);
        frs_objp = pdaindr[cpu].pda->p_frs_objp;
        ASSERT (frs_objp != NULL);

        pda_ospl = frs_cpu_pdalock(frs_objp);
        ASSERT (frs_objp->resv);
        ASSERT (frs_objp->frs == NULL);
        frs_objp->resv = 0;
        frs_fsched_incrref(frs);
        frs_objp->frs = frs;
        frs_cpu_pdaunlock(frs_objp, pda_ospl);
        return (0);
}

struct frs_fsched*
frs_cpu_unlinkpda(cpuid_t cpu)
{
        frs_pda_t* frs_objp;
        frs_fsched_t* pdafrs;
        int pda_ospl;

        frs_objp = pdaindr[cpu].pda->p_frs_objp;
        ASSERT (frs_objp != NULL);

        pda_ospl = frs_cpu_pdalock(frs_objp);
        ASSERT (!frs_objp->resv);
        ASSERT (frs_objp->frs != NULL);
        pdafrs = frs_objp->frs;
        frs_objp->frs = NULL;
        frs_fsched_decrref(pdafrs);
        frs_cpu_pdaunlock(frs_objp, pda_ospl);

        return (pdafrs);
}
        
int
frs_cpu_isolate(cpuid_t cpu)
{
	int ret;

	ret = mp_isolate(cpu);

	ASSERT (ret ? 1 : pdaindr[cpu].pda->p_flags & PDAF_ISOLATED);
	ASSERT (ret ? 1 : pdaindr[cpu].pda->p_cpu.c_restricted == 1);

   	return (ret);
}     
        
int
frs_cpu_unisolate(cpuid_t cpu)
{
	return (mp_unisolate(cpu));
}        

void
frs_cpu_resetcputimer(uint cc)
{
	initcounter(cc);

	private.last_sched_intr_RTC = 0;		
}

void
frs_cpu_stopcputimer(void)
{
#if defined(IP30)
	initcounter(0xffffffff);
#else
	delaycounter();
#endif
	private.last_sched_intr_RTC = 0;		
}

/* ARGSUSED */
void
frs_cpu_resetcctimer(__uint64_t new_time)
{
#if defined(EVEREST) || defined(SN0)
	int s;

#ifdef EVEREST
	int ev_slot, ev_cpu;
	int targcpu = cpuid();
#endif

	s = splprof();

#ifdef EVEREST
	ev_slot = cpuid_to_slot[targcpu];	/* extract for efficiency */
	ev_cpu  = cpuid_to_cpu [targcpu] << EV_PROCNUM_SHFT;
	EV_SETCONFIG_REG(ev_slot, ev_cpu, EV_CMPREG0, (u_int32_t)new_time);
	EV_SETCONFIG_REG(ev_slot, ev_cpu, EV_CMPREG1, (u_int32_t)new_time>> 8);
	EV_SETCONFIG_REG(ev_slot, ev_cpu, EV_CMPREG2, (u_int32_t)new_time>>16);
	EV_SETCONFIG_REG(ev_slot, ev_cpu, EV_CMPREG3, (u_int32_t)new_time>>24);
#endif
#ifdef SN0
	if (LOCAL_HUB_L(PI_CPU_NUM))
		LOCAL_HUB_S(PI_RT_COMPARE_B, new_time);
	else
		LOCAL_HUB_S(PI_RT_COMPARE_A, new_time);	
#endif
#if SABLE
	pseudo_CMPREG = new_time;
#endif
	/*
	 * Now read back another config register (can't read COMPARE) to
	 * guarantee that the new COMPARE value has arrived and is ready
	 *
	 * Don't need to do this for TFP since uncached stores are
	 * sychronous and complete before starting execution of the next
	 * instruction.
	 */
#if defined(IP19) || defined(IP25)
	wbflush();
#endif
	splx(s);
#endif /* defined(EVEREST) || defined(SN0) */
}

void
frs_cpu_stopcctimer(void)
{
	/*
	 * Writing the CMPREG0 disables interrupts.  Writing
	 * all 4 bytes, CMPREG[0-4] re-enables with a new value.
	 */
	 DISABLE_TMO_INTR(); /* disable tmo clock */
}

__uint64_t
frs_cpu_getcctimer(void)
{
	return (GET_LOCAL_RTC);
}

void
frs_cpu_stopproftimer(void)
{
	return;
}

void
frs_cpu_resched(void)
{
        private.p_runrun = 1;
}


/**************************************************************************
 *                           Event Handlers                               *
 **************************************************************************/

void
frs_handle_counter_intr(void)
{
	if (private.p_frs_flags & FRS_INTRSOURCE_CPUTIMER) {
		frs_fsched_eventintr();
	} else if (private.p_frs_flags) {
		frs_fsched_t* frs = curfrs();
		if (frs->intrmask & FRS_INTRSOURCE_CPUTIMER) {
			/*
			 * This FRS is using the cputimer
			 * and it came in out of sequence.
			 */
			frs_fsched_isequence_error(frs);
		}
		frs_cpu_stopcputimer();
	}

	private.last_sched_intr_RTC = 0;
}

void 
frs_handle_timein(void)
{
#if !defined(IP30)
	acktmoclock();
	acksoftclock();

	if (private.p_frs_flags & FRS_INTRSOURCE_CCTIMER) {
		frs_fsched_eventintr();
	} else if (private.p_frs_flags) {
		frs_fsched_isequence_error(curfrs());
	}
#endif
}

void
frs_handle_eintr(void)
{
	if (private.p_frs_flags & FRS_INTRSOURCE_EXTINTR) {
#if defined(SN0)
		/*
		 * SN0 systems do not have hardware support for
		 * group interrupts, so the calling interrupt
		 * thread needs to explicitly trigger a pseudo 
		 * device interrupt for the other CPUs in the
		 * Frame Scheduled group.
		 */
		ASSERT(curfrs());
		ASSERT(iget_intrgroup(curfrs()));
		sendgroupintr(iget_intrgroup(curfrs()));
#else
		frs_fsched_eventintr();
#endif
	} else if (private.p_frs_flags) {
		frs_fsched_isequence_error(curfrs());
	}
}

void
frs_handle_uli(void)
{

#if !defined(EVEREST)

	if (private.p_frs_flags & FRS_INTRSOURCE_ULI) {
	
#if defined(SN0)
		/*
		 * SN0 systems do not have hardware support for
		 * group interrupts, so the calling interrupt
		 * thread needs to explicitly trigger a pseudo 
		 * device interrupt for the other CPUs in the
		 * Frame Scheduled group.
		 */
		ASSERT(curfrs());
		ASSERT(iget_intrgroup(curfrs()));
		sendgroupintr(iget_intrgroup(curfrs()));
#else
		frs_fsched_eventintr();
#endif
	} else if (private.p_frs_flags) {
		frs_fsched_isequence_error(curfrs());
	}

#endif /*!EVEREST*/
}

void
frs_handle_driverintr(void)
{
	if (private.p_frs_flags & FRS_INTRSOURCE_DRIVER) {
#if defined(SN0)
		/*
		 * SN0 systems do not have hardware support for
		 * group interrupts, so the calling interrupt
		 * thread needs to explicitly trigger a pseudo 
		 * device interrupt for the other CPUs in the
		 * Scheduled group.
		 */
		ASSERT(curfrs());
		ASSERT(iget_intrgroup(curfrs()));
		sendgroupintr(iget_intrgroup(curfrs()));
#else
		frs_fsched_eventintr();
#endif
	} else if (private.p_frs_flags) {
		frs_fsched_isequence_error(curfrs());
	}
}

void
frs_handle_vsyncintr(frs_fsched_t *frs)
{
	cpuid_t cpu;

	if (frs == NULL)
		return;

	cpu = frs->cpu;
	ASSERT (cpu);

	if (pdaindr[cpu].pda->p_frs_flags & FRS_INTRSOURCE_VSYNC) {
		sendgroupintr(iget_intrgroup(frs));
	} else if (private.p_frs_flags) {
		frs_fsched_isequence_error(frs);
	}
}

void
frs_handle_userintr(frs_fsched_t* frs)
{
	cpuid_t cpu;

	cpu = frs->cpu;
	if (pdaindr[cpu].pda->p_frs_flags & FRS_INTRSOURCE_USER) {
		sendgroupintr(frs_get_intrgroup(frs));
	} else if (private.p_frs_flags) {
		frs_fsched_isequence_error(frs);
	}
}		

void
frs_handle_group_intr(void)
{
	/*
	 * This handler is invoked by sendgroupintr, for
	 * all CPUs in the interrupt group.
	 */
	if (!_isIntTimerIntrSource(private.p_frs_flags)) {
		frs_fsched_eventintr();
	} else if (private.p_frs_flags) {
		frs_fsched_isequence_error(curfrs());
	}
}

void
frs_handle_termination(void)
{
	if (private.p_frs_flags)
		frs_fsched_eventintr();
}


/**************************************************************************
 *                           Device Driver Hooks                          *
 **************************************************************************/

int
frs_driver_export(int frs_driver_id,
		  void (*frs_func_set)(intrgroup_t*),
		  void (*frs_func_clear)(void))
{
	int s;
	
	if (frs_driver_id < 0 || frs_driver_id > FRS_MAXDRVENTS) {
		return (-1);
	}

	s = splockspl(frs_drvtable.lock, splhi);
	if (frs_drvtable.frs_drvent[frs_driver_id].frs_func_set != NULL) {
		spunlockspl(frs_drvtable.lock, s);
		return (-1);
	}
	if (frs_drvtable.frs_drvent[frs_driver_id].frs_func_clear != NULL) {
		spunlockspl(frs_drvtable.lock, s);
		return (-1);
	}
	frs_drvtable.frs_drvent[frs_driver_id].frs_func_set =
	  (void (*)(intrgroup_t*))frs_func_set;
	frs_drvtable.frs_drvent[frs_driver_id].frs_func_clear = frs_func_clear;
	spunlockspl(frs_drvtable.lock, s);

	return (0);
}

void
frs_driver_init(void)
{
	int i;
	
	initnlock(&frs_drvtable.lock, "frsdrv");
	for (i = 0; i < FRS_MAXDRVENTS; i++) {
		frs_drvtable.frs_drvent[i].frs_func_set = 0;
		frs_drvtable.frs_drvent[i].frs_func_clear = 0;
	}

	idbg_addfunc("frs", frs_cpu_frspda_print);
}


/**************************************************************************
 *                           Interrupt Control                            *
 **************************************************************************/

int
frs_cpu_eventintr_validate(frs_fsched_t* frs,
			   frs_fsched_info_t* info,
			   frs_intr_info_t* intr_info)
{
	int i, q;
	int source;
	int iterations;
	int n_sources = 0;
	int igroup_needed = 0;
	uint intrmask = 0;

	if (info->intr_source != FRS_INTRSOURCE_VARIABLE) {
		source = info->intr_source;
		q = info->intr_qualifier;
		iterations = 1;
	} else
		iterations = info->n_minors;

	for (i=0; i < iterations; i++) {

		if (info->intr_source == FRS_INTRSOURCE_VARIABLE) {
			ASSERT (intr_info);
			source = intr_info[i].intr_source;
			q = intr_info[i].intr_qualifier;
		}

		switch (source) {
		case FRS_INTRSOURCE_CPUTIMER:
		case FRS_INTRSOURCE_CCTIMER:
			/*
			 * Qualifier specifies the period
			 */
			if (q < FRS_TIMER_SHORTEST || q > FRS_TIMER_LONGEST)
				return (FRS_ERROR_INTR_INVPERIOD);
			break;
			
		case FRS_INTRSOURCE_USER:
		case FRS_INTRSOURCE_EXTINTR:
		case FRS_INTRSOURCE_ULI:
			/*
			 * Qualifier is not used
			 */
			igroup_needed = 1;
			break;
			
		case FRS_INTRSOURCE_VSYNC:
			/*
			 * Qualifier specifies the pipe number
			 */
			if (q > MAX_GRAPH_PIPES)
				return (FRS_ERROR_INTR_INVGRAPHICPIPE);

			if (!gfx_frs_valid_pipenum(q))
				return (FRS_ERROR_INTR_INVGRAPHICPIPE);

			igroup_needed = 1;
			break;

		case FRS_INTRSOURCE_DRIVER:
			/*
			 * Qualifier specifies the driver ID
			 */
			if (frs_drvtable.frs_drvent[q].frs_func_set == NULL ||
			    frs_drvtable.frs_drvent[q].frs_func_clear == NULL)
				return (FRS_ERROR_INTR_INVDRIVER);

			igroup_needed = 1;
			break;

		case FRS_INTRSOURCE_VARIABLE:

		default:
			return (FRS_ERROR_INTR_INVSOURCE);
		}

		if (!(source & intrmask)) {
			intrmask |= source;
			n_sources++;
		}
	}

	/*
	 * Check for illegal combinations
	 */

#if defined(IP30)
	if (intrmask & (FRS_INTRSOURCE_CCTIMER | FRS_INTRSOURCE_EXTINTR))
		return (FRS_ERROR_INTR_INVSOURCE);
#endif

#if defined(EVEREST)
	if (intrmask & FRS_INTRSOURCE_ULI)
		return (FRS_ERROR_INTR_INVSOURCE);
#endif

	if (n_sources > 1) {

		if (intrmask & FRS_INTRSOURCE_CCTIMER)
			return (FRS_ERROR_INTR_INVSOURCE);

		if (intr_info[0].intr_source == FRS_INTRSOURCE_CPUTIMER)
			return (FRS_ERROR_INTR_INVSOURCE);
		
		if (intrmask & FRS_INTRSOURCE_VSYNC &&
		    intrmask & FRS_INTRSOURCE_DRIVER)
			return (FRS_ERROR_INTR_INVSOURCE);
		
		if (intrmask & FRS_INTRSOURCE_VSYNC &&
		    intr_info[0].intr_source != FRS_INTRSOURCE_VSYNC)
			return (FRS_ERROR_INTR_INVSOURCE);

		if (intrmask & FRS_INTRSOURCE_DRIVER &&
		    intr_info[0].intr_source != FRS_INTRSOURCE_DRIVER)
			return (FRS_ERROR_INTR_INVSOURCE);
	}

	/*
	 * Allocate a common Interrupt Group
	 */
	if (igroup_needed) {
		frs->intrgroup = intrgroup_alloc();
		if (frs->intrgroup == 0)
			return (FRS_ERROR_INTR_OUTOFINTRGROUPS);
	}

	frs->intrmask = intrmask;

	return 0;
}	

void
frs_cpu_eventintr_arm(frs_fsched_t *frs)
{
        cpu_cookie_t was_running;
	cpuid_t cpu;
	intrgroup_t *intrgroup;
	intrdesc_t *first_idesc;
	int first_isource;
	
	ASSERT(!_isFrameScheduled(curuthread));

	cpu = frs->cpu;
	intrgroup = frs_get_intrgroup(frs);
	first_idesc = frs_major_getidesc(frs->major, FRS_IDESC_FIRST);
	first_isource = iget_intrsource(first_idesc);

        /*
         * We need to be running on the cpu
         * we are initializing the event intr for.
         */
        was_running = setmustrun(cpu);

	ASSERT ((private.p_flags & PDAF_ISOLATED) &&
		private.p_cpu.c_restricted == 1);

	ASSERT(private.p_frs_objp != 0);
        ASSERT(cpu == cpuid());

	/*
	 * Stop All Timers
	 */
	frs_cpu_stopcputimer();
	frs_cpu_stopcctimer();
	frs_cpu_stopproftimer();
	
	if (intrgroup)
		intrgroup_join(intrgroup, cpu);

#if defined(EVEREST)
	if (_isMasterFrs(frs)) {
		/*
		 * Reroute device interrupt from it's current
		 * target cpu the intrgroup of the frs
		 */
		if (frs->intrmask & FRS_INTRSOURCE_EXTINTR)
			frs_eintr_set(intrgroup);
#if 0
		/*
		 * XXX frs_uliintr_set() not implemented
		 */
		if (frs->intrmask & FRS_INTRSOURCE_ULI)
			frs_uliintr_set(intrgroup);
#endif
	}
#endif /* defined(EVEREST) */ 

	/*
	 * Grab a reference for the interrupt handler, which will be held
	 * until the interrupt is disarmed via frs_cpu_eventintr_clear().
	 * ref++
	 */
	frs_fsched_incrref(frs);

	/*
	 * Set the pda's FRS interrupt flag to allow interrupt
	 * sources to be routed to the FRS.
	 */
	ASSERT (!(frs->flags & FRS_FLAGS_ARMED));
	frs->flags |= FRS_FLAGS_ARMED;
	atomicSetInt((int*)&private.p_frs_flags, first_isource);

        restoremustrun(was_running);
}

void
frs_cpu_eventintr_fire(frs_fsched_t* frs)
{
	intrdesc_t *idesc;
	cpuid_t cpu;
	int s;

	void (*frs_func_set)(intrgroup_t*);

	cpu = frs->cpu;
	idesc = frs_major_getidesc(frs->major, FRS_IDESC_FIRST);

	ASSERT (frs->flags & FRS_FLAGS_ARMED);

	switch (iget_intrsource(idesc)) {
	case FRS_INTRSOURCE_CPUTIMER:
		ASSERT(pdaindr[cpu].pda->p_frs_flags & FRS_INTRSOURCE_CPUTIMER);
		cpuaction(cpu,
			  (cpuacfunc_t) frs_fsched_eventintr,
			  A_NOW,
			  NULL);
		break;
		
	case FRS_INTRSOURCE_CCTIMER:
		ASSERT(pdaindr[cpu].pda->p_frs_flags & FRS_INTRSOURCE_CCTIMER);
		cpuaction(cpu,
			  (cpuacfunc_t) frs_fsched_eventintr,
			  A_NOW,
			  NULL);
		break;
		
	case FRS_INTRSOURCE_USER:
	case FRS_INTRSOURCE_EXTINTR:
	case FRS_INTRSOURCE_ULI:
		break;
		
	case FRS_INTRSOURCE_VSYNC:
		ASSERT(pdaindr[cpu].pda->p_frs_flags & FRS_INTRSOURCE_VSYNC);
		ASSERT(iget_intrgroup(idesc) != 0);
#if defined(IP30)
		/*
		 * XXX At some point (next major release) we want to migrate
		 *     KONA and VENICE drivers over to the new scheme, where
		 *     we pass the frs pointer to the gfx driver upon
		 *     installation and the driver passes it back to us via
		 *     frs_handle_vsyncintr. This would enable us to
		 *     thoroughly detect sequence errors.
		 *
		 *     Presently, only IP30 MGRAS does this.
		 */
		gfx_frs_install(frs, iget_pipenum(idesc));
#else
		/*
		 * XXX Here is the current KONA/VENICE scheme. These drivers
		 *     call sendgroupintr directly. Therefore, vsync sequence
		 *     errors cannot be caught for these platforms.
		 */
		gfx_frs_install((struct frs_fsched *)frs->intrgroup,
				iget_pipenum(idesc));
#endif
		break;
		
	case FRS_INTRSOURCE_DRIVER:
		ASSERT(pdaindr[cpu].pda->p_frs_flags & FRS_INTRSOURCE_DRIVER);
		ASSERT(iget_intrgroup(idesc) != 0);
		ASSERT(iget_driverid(idesc) < FRS_MAXDRVENTS);
		s = splockspl(frs_drvtable.lock, splhi);
		frs_func_set = frs_drvtable.frs_drvent[iget_driverid(idesc)].frs_func_set;
		spunlockspl(frs_drvtable.lock, s);
		ASSERT(frs_func_set);
		(*frs_func_set)(iget_intrgroup(idesc));
		break;

	default:
		cmn_err_tag(1778, CE_NOTE,
			"[frs_fsched_eventintr_fire]: Invalid Intr source");
		break;
	}
}

void
frs_cpu_eventintr_clear(frs_fsched_t* frs)
{
	cpuid_t cpu;
	int s;
	void (*frs_func_clear)(void);
	uint_t intrmask;
	intrdesc_t *idesc;

	ASSERT (frs);
	ASSERT (frs->flags & FRS_FLAGS_ARMED);

	cpu = frs->cpu;
	ASSERT (cpu == cpuid());

	intrmask = frs->intrmask;
	frs->intrmask = 0;

	private.p_frs_flags = 0;

	frs_cpu_resetcputimer(private.p_rtclock_rate);

	if (frs_get_intrgroup(frs))
		intrgroup_unjoin(frs_get_intrgroup(frs), cpu);

	if (intrmask & FRS_INTRSOURCE_CCTIMER)
		((frs_pda_t*)private.p_frs_objp)->basecc = 0;

	if (_isMasterFrs(frs)) {
		idesc = frs_major_getidesc(frs->major, FRS_IDESC_FIRST);

		if (intrmask & FRS_INTRSOURCE_VSYNC) {
			ASSERT(idesc->source == FRS_INTRSOURCE_VSYNC);
			gfx_frs_uninstall(iget_pipenum(idesc));
		}

		if (intrmask & FRS_INTRSOURCE_DRIVER) {
			int id = iget_driverid(idesc);
			ASSERT(id < FRS_MAXDRVENTS);
			ASSERT(idesc->source == FRS_INTRSOURCE_DRIVER);
			s = splockspl(frs_drvtable.lock, splhi);
			frs_func_clear =
				frs_drvtable.frs_drvent[id].frs_func_clear;
			spunlockspl(frs_drvtable.lock, s);
			ASSERT(frs_func_clear);
			(*frs_func_clear)();			
		}

#if defined(EVEREST)
		if (intrmask & FRS_INTRSOURCE_EXTINTR)
			frs_eintr_clear();
#if 0
		/*
		 * XXX frs_uliintr_clear() is not implemented
		 */
		if (intrmask & FRS_INTRSOURCE_ULI)
			frs_uliintr_clear();
#endif
#endif /* defined(EVEREST) */

	}

	/*
	 * Allow our controller thread, which is waiting in
	 * frs_cpu_eventintr_clearwait(), to make forward progress
	 */
	vsema(&frs->term_sema);

	/*
	 * Release interrupt handler's FRS reference.
	 * ref--
	 */
	frs_fsched_decrref(frs);
}

void
frs_cpu_eventintr_clearwait(frs_fsched_t* frs)
{
	/*REFERENCED*/
	int ret;

	ASSERT (frs);
	ASSERT (frs->cpu != 0);
	ASSERT (_isFRSController(curuthread));

	if (frs->flags & FRS_FLAGS_ARMED) {
		/*
		 * Interrupt is armed.
		 * Wait for interrupt handler to disarm itself.
		 */
		ret = psema(&frs->term_sema, PZERO);
		ASSERT (ret == 0);
	} else {
		/*
		 * Interrupt was never armed, so there is no need
		 * to wait for the disarm (it will never happen).
		 *
		 * Kick the main barrier on the handlers behalf, and move on.
		 */
		ASSERT (frs->sdesc->sl_term);
		frs->sdesc->sl_term_ack = 1;
		syncbar_kick(frs->sb_intr_main);
	}

	ASSERT (pdaindr[frs->cpu].pda->p_frs_flags == 0);
}
	
/*ARGSUSED1*/
void
frs_cpu_eventintr_reset(cpuid_t cpu, intrdesc_t* idesc)
{
	__uint64_t cycles;
	int source;

	ASSERT(idesc != 0);
	ASSERT(private.p_frs_flags);
	ASSERT(private.p_frs_objp);
	ASSERT(private.p_cpuid == cpu);

	source = iget_intrsource(idesc);

	switch (source) {
	case FRS_INTRSOURCE_CPUTIMER:
		cycles = findcpufreq() * (__uint64_t)iget_period(idesc);
		frs_cpu_resetcputimer(cycles);
		break;

	case FRS_INTRSOURCE_CCTIMER:
		frs_cpu_stopcputimer();
		cycles = ((__uint64_t)iget_period(idesc) * 1000LL) /
			NSEC_PER_CYCLE;
		((frs_pda_t*)private.p_frs_objp)->basecc += cycles;
		frs_cpu_resetcctimer(((frs_pda_t*)private.p_frs_objp)->basecc);
		break;

	case FRS_INTRSOURCE_USER:
	case FRS_INTRSOURCE_EXTINTR:
	case FRS_INTRSOURCE_ULI:
	case FRS_INTRSOURCE_VSYNC:
	case FRS_INTRSOURCE_DRIVER:
		ASSERT(iget_intrgroup(idesc) != 0);
		frs_cpu_stopcputimer();		
		break;

	default:
		cmn_err_tag(1779, CE_NOTE,
			"[frs_fsched_eventintr_reset]: Invalid Intr Source");
		return;
	}

	/* Interrupt source filter */
	private.p_frs_flags = source;
}

/*ARGSUSED*/
void
frs_cpu_resetallintrs(cpuid_t cpu)
{
	ASSERT(cpu == cpuid());
        frs_cpu_resetcputimer(private.p_rtclock_rate);
        frs_cpu_resetcctimer(0);
}

void
frs_cpu_frspda_print(cpuid_t cpu)
{
	if (cpu < 0 || cpu > maxcpus) {
		qprintf("Invalid cpu number\n");
		return;
	}

	qprintf("PDA %d FRS info\n", cpu);
        qprintf("  p_frs_flags %x p_frs_objp 0x%x\n",
		pdaindr[cpu].pda->p_frs_flags,
		pdaindr[cpu].pda->p_frs_objp);
        qprintf("  frspda 0x%x lock 0x%x resv %d\n",
		&frs_pda[cpu],
		&frs_pda[cpu].lock,
		frs_pda[cpu].resv);

	qprintf("  frs 0x%x basecc %x\n",
		frs_pda[cpu].frs,
		frs_pda[cpu].basecc);

	if (frs_pda[cpu].frs == NULL) {
		qprintf("  FRS not currently linked\n");
		return;
	}

	frs_fsched_print(frs_pda[cpu].frs);
}

#endif /* EVEREST || SN0 || IP30 */
