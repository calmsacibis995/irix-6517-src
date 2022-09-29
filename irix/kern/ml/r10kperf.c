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
 * File: r10kperf.c
 *	R10k specific performance counters support.
 */


#ifdef R10000

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/pda.h>
#include <sys/prf.h>
#include <sys/atomic_ops.h>
#include <ksys/exception.h>
#include <ksys/vproc.h>

__uint32_t r1nk_perf_architecture      (void);
void       r1nk_perf_check_architecture(cpu_mon_t *cmp);

#if defined (HWPERF_DEBUG)
#include <sys/rtmon.h>

/*
 * to use the UTRACE mechanism :
 *	- set utrace_bufsize in mtune/kernel to 2048
 *	- set utrace_mask to in mtune/kernel to 1
 */
#define HWPERF_UTRACE(name,val1,val2)	UTRACE(RTMON_DEBUG,UTN('cprf',name),\
					(__uint64_t)val1,(__uint64_t)val2)

#else
#define HWPERF_UTRACE(name,val1,val2)
#endif


#define CANNOT_PREEMPT		(issplhi(getsr()))

#define VALID_MODE(_mode)	((_mode) & HWPERF_CNTEN_A)
#define VALID_EVENT(_ev)	(((_ev) >= HWPERF_MINEVENT) && ((_ev) <= HWPERF_MAXEVENT))


	



/*
 * Function	: cpu_perf_intr
 * Parameters	: eframe
 * Purpose	: To handle overflow interrupts of perf counters.
 * Assumptions	: none
 * Returns	: none
 */
void
r1nk_perf_intr(eframe_t *ep)
{
	cpu_mon_t 	*cmp;
	uint		count;
	int 		send_sig = 0;
	short		counter,event_index;

	HWPERF_UTRACE('intr',ep,private.p_active_cpu_mon);

	if ((cmp = private.p_active_cpu_mon) == NULL)
	    return;

	r1nk_perf_stop_counters(cmp);

	/* Go through each performance counter and if it has been
	 * setup for performance monitoring update the virtual counters
	 */
	for (counter = 0 ; counter < cmp->cm_num_counters; counter++) {
	    	event_index = cmp->cm_evindx[counter];

	    	if (event_index != -1) {
			count = r1nkperf_data_register_get(counter);  
			HWPERF_UTRACE('DATR',counter,count);

			if (cmp->cm_preloadcnt[event_index] && 
		    	    (count >= HWPERF_OVERFLOW_THRESH)) {
		    		cmp->cm_savecnt[event_index] = 
					cmp->cm_preloadcnt[event_index];
		    		send_sig = 1;
			}
			atomicAddUint64(&cmp->cm_eventcnt[event_index],
				(count - cmp->cm_preloadcnt[event_index]));
	    	}
	}
	r1nk_perf_intr_action(ep, cmp, send_sig);

	r1nk_perf_start_counters(cmp);
}


/*
 * Function	: cpu_perf_inter_action
 * Parameters	: 
 * Purpose	: to handle the actions necessary following overflow intrs.
 * Assumptions	: none
 * Returns	: none
 */
void
r1nk_perf_intr_action(eframe_t *ep, cpu_mon_t *cmp, int send_sig)
{
	HWPERF_UTRACE('inta',ep,cmp);

 	if (cmp->cm_flags & HWPERF_CM_PROFILING) {
		if (cmp->cm_flags & HWPERF_CM_CPU)
			prfintr(ep->ef_epc, ep->ef_sp, ep->ef_ra, ep->ef_sr);
 		else if (curthreadp && KT_ISUTHREAD(curthreadp)) {
			/*
			 * Set a flag so that user will later accumulate
			 * a pc tick for this counter overflow tick. We can't 
			 * just call addupc here because it may take a page
			 * fault and need to sleep.
			 */
			ASSERT(cmp->cm_flags & HWPERF_CM_PROC);
			ASSERT(curuthread);
			ut_flagset(curuthread, UT_OWEUPC);
			PCB(pcb_resched) = 1;
		}
	} else if (send_sig && (cmp->cm_sig)) {
		/*
		 * User wants a signal -- post it and do a
		 * resched so that he gets it right away.
		 */
		HWPERF_UTRACE('isig',curuthread,cmp->cm_sig);

		sigtouthread(curuthread, cmp->cm_sig, (k_siginfo_t *)NULL);
		if (USERMODE(ep->ef_sr))
			PCB(pcb_resched) = 1;
	} 
}


/*
 * Function	: cpu_perf_update_counters
 * Parameters	: cmp
 * Purpose	: To update the cmp structure with current counter values.
 * Assumptions	: cmp is the one running on this cpu.
 * Returns	: none
 */
void
r1nk_perf_update_counters(cpu_mon_t *cmp)
{
	int s, splset = 0;
	int count;
	short counter,event_index;

	HWPERF_UTRACE('upda',cmp,0);

	if (cmp == NULL) {
	    s = splhi(), splset = 1;
	    cmp = private.p_active_cpu_mon;
	    if ((cmp == NULL) || ((cmp->cm_flags & HWPERF_CM_EN) == 0)) {
		splx(s);
		return;
	    }
	}

	r1nk_perf_check_architecture(cmp);

	ASSERT(cmp && cmp->cm_flags && (cmp == private.p_active_cpu_mon));

	/* Go through each performance counter and if it has been
	 * setup for performance monitoring update the virtual counters
	 */

	for (counter = 0; counter < cmp->cm_num_counters; counter++) {
	    	event_index = cmp->cm_evindx[counter];

	    	if (event_index >= 0) {
			ASSERT(IS_HWPERF_EVENT_MASK_SET(
					cmp->cm_event_mask[counter],
					event_index));
			count = r1nkperf_data_register_get(counter);
			HWPERF_UTRACE('DATR',counter,count);

			if (cmp->cm_preloadcnt[event_index]) 
		    		cmp->cm_savecnt[event_index] = count;
			else {
#ifdef NO_RESET_PRFCNT
		    /* This mode is compiled when using kernel trace facility.
		     * Since cm_savecnt is only used when cm_preloadcnt is
		     * set, we use it here to track the last value we added
		     * into the cm_eventcnt so each call only adds in the 
		     * delta.    
		     * This lets the PRFCNT be a free-running monotonically
		     * increasing counter for use by trace code.
		     */

		    		atomicAddUint64(&cmp->cm_eventcnt[event_index],
				          count - cmp->cm_savecnt[event_index]);
		    		cmp->cm_savecnt[event_index] = count;

#else /* !NO_RESET_PRFCNT */

		    		atomicAddUint64(&cmp->cm_eventcnt[event_index],
								 count);
				HWPERF_UTRACE('DATW',counter,0);
		    		r1nkperf_data_register_set(counter,0);

#endif /* !NO_RESET_PRFCNT */
			}
	    	}
	}

	if ((cmp->cm_flags & HWPERF_CM_ENABLED) == 0) {
	    	r1nk_perf_stop_counters(cmp);

	    	if (cmp->cm_flags & HWPERF_CM_CPU)
			cmp->cm_flags &= ~HWPERF_CM_CPU;

	    	private.p_active_cpu_mon = NULL;
	}

	if (splset) 
		splx(s);

	return;
}	


/*
 * Function	: cpu_perf_start_counters
 * Parameters	: cmp
 * Purpose	: start the counters on this processor.
 * Assumptions	: will not be preempted.
 * Returns	: none.
 */
void	    
r1nk_perf_start_counters(cpu_mon_t *cmp)
{
	int count;
	__uint32_t perfctrl;
	short counter,event_index;

        ASSERT_ALWAYS(CANNOT_PREEMPT);
	ASSERT(cmp && (cmp->cm_flags & HWPERF_CM_EN));
	ASSERT(cmp == private.p_active_cpu_mon);

	HWPERF_UTRACE('star',cmp,cmp->cm_flags);

	if ((cmp->cm_flags & HWPERF_CM_ENABLED) == 0)
	    return;

	r1nk_perf_check_architecture(cmp);

	cmp->cm_counting_cpu = cpuid();

	/* For each counter check if it needs to be enabled for counting.
	 * If so then setup the performance counter's control & data
	 * registers appropriately.
	 */
	for (counter = 0; counter < cmp->cm_num_counters; counter++) {
	    event_index = cmp->cm_evindx[counter];
	    if (event_index >= 0) {
		ASSERT(IS_HWPERF_EVENT_MASK_SET(cmp->cm_event_mask[counter],
						event_index));

                if (PDA_IS_R12000 && (counter == 1)) { 
			hwperf_ctrl_t reg;

			/*
			 * emulate R10K :
			 * if we set counter 1, remap event 
			 */
			reg.hwperf_spec         = cmp->cm_evspec[event_index];
			reg.hwperf_creg.hwp_ev += 16;
			perfctrl 	        = reg.hwperf_spec;
		} else {
			perfctrl = cmp->cm_evspec[event_index];
		}
			
		HWPERF_UTRACE('CTLW',counter,perfctrl);
		r1nkperf_control_register_set(counter,perfctrl);
					      
		count = 
		    cmp->cm_preloadcnt[event_index] ? 
		    cmp->cm_savecnt[event_index] : 0;

		HWPERF_UTRACE('DATW',counter,count);
		r1nkperf_data_register_set(counter,count);
	    }
	}
	return;
}	
/*
 * Function	: cpu_perf_stop_counters
 * Parameters	: cmp
 * Purpose	: To stop the hw counters of this cpu
 * Assumptions	: kpreempt not possible.
 * Returns	: none.
 */

void
r1nk_perf_stop_counters(cpu_mon_t *cmp)
{
	short	counter;

        ASSERT_ALWAYS(CANNOT_PREEMPT);
	ASSERT(cmp && (cmp->cm_flags & HWPERF_CM_EN));
	ASSERT(cmp == private.p_active_cpu_mon);

	HWPERF_UTRACE('stop',cmp,0);

	r1nk_perf_check_architecture(cmp);

	cmp->cm_counting_cpu = CPU_NONE;

	/* 
	 * For each counter clear the performance counter 
	 * control register 
	 */
	for (counter = 0 ; counter < cmp->cm_num_counters;counter++) {
		HWPERF_UTRACE('CTLW',counter,0);
		r1nkperf_control_register_set(counter,0);  
	}
}

/* ARGSUSED */
int
r1nk_perf_init_counters(cpu_mon_t 		*cmp,
		        hwperf_profevctrarg_t 	*argp,
		        hwperf_profevctraux_t	*auxp)
{
	int 	permit;
	int 	spec, mode, event, freq;
	short	event_index,counter_index;

	HWPERF_UTRACE('init',cmp,argp);

	permit = _CAP_ABLE(CAP_DEVICE_MGT);

	/* Set the current event index to invalid value (-1) &
	 * number of events to zero for each counter.
	 */
	for (counter_index = 0 ; counter_index < cmp->cm_num_counters; 
	     counter_index++) {
		cmp->cm_evindx[counter_index] = -1;
		cmp->cm_events[counter_index] = 0;
	}

	/* Loop through each event */
	for (event_index = 0; event_index < cmp->cm_num_events; event_index++) {

	    	spec = (argp->hwp_evctrargs.hwp_evctrl[event_index].hwperf_spec 
						&= ~HWPERF_CNTEN_S);

	    	/* Initialize the event-specific info in the cpu monitoring
	     	 * info 
	     	 */
	    	cmp->cm_evspec    [event_index] = 0;
		cmp->cm_preloadcnt[event_index] = 0;
	    	cmp->cm_eventcnt  [event_index] = 
				auxp->hwp_aux_cntr.hwp_evctr[event_index];

	    	if (spec) {
			mode  = spec & HWPERF_MODEMASK;
			event = (spec & HWPERF_EVMASK) >> HWPERF_EVSHIFT;
			freq  = argp->hwp_ovflw_freq[event_index];


			if ((mode & HWPERF_CNTEN_K) && !permit) 
		    		return(EPERM);
	
			if (!VALID_EVENT(event) || !VALID_MODE(mode) || 
			    (freq < 0))
                		return(EINVAL);

			cmp->cm_evspec[event_index] = spec;

			if (freq) 
		    		cmp->cm_preloadcnt[event_index] = 
						HWPERF_OVERFLOW_THRESH - freq;

			cmp->cm_savecnt[event_index] = 
					cmp->cm_preloadcnt[event_index];

			/* 
		 	 * Setup the firstevent , number of events & 
			 * lastevent per each counter.
		 	 */

			for (counter_index = 0 ; 
					counter_index < cmp->cm_num_counters; 
		     			counter_index++) {
		    		/* 
				 * Check if this counter can count this 
				 * particular event 
				 */

		    		if (IS_HWPERF_EVENT_MASK_SET(
				 	cmp->cm_event_mask[counter_index],
				 	event_index)) {

					/* 
					 * Check if the first event for this 
					 * counter has been setup 
			 		 */
					if (cmp->cm_evindx[counter_index] == -1)
			    			cmp->cm_evindx[counter_index] = 
								event_index;
					cmp->cm_events[counter_index]++;
					cmp->cm_evmax[counter_index] = 
								event_index;
		    		}
			}
	    	}
	}
	return(0);
}


__uint32_t
r1nk_perf_architecture(void)
{
	rev_id_t rev_id;

	rev_id.ri_uint = private.p_cputype_word;

	/*
	 * all trex CPUs are currently the same regarding 
	 * the HW performance monitors. Since the latest
	 * revision is 2.2, we'll use this as the base
	 */
	if (rev_id.ri_imp == C0_IMP_R12000) {
		return HWPERF_CPU_R12K_REV22;
	}

	if (rev_id.ri_imp == C0_IMP_R10000) {
		if (rev_id.ri_majrev >= 3) {
			return HWPERF_CPU_R10K_REV3;
		} else {
			return HWPERF_CPU_R10K_REV2X;
		}
	}

	/*
	 * most certainly we should not get here ....
	 */
	cmn_err(CE_PANIC,
		"r1nk_perf_architecture unknown CPU type (CPU %d id=0x%x\n",
		cpuid(),private.p_cputype_word);

	/*NOTREACHED*/

}
		

#define	FMT "CPU HW perfmon architecture change, signal %d pgrp %d\n"

void
r1nk_perf_check_architecture(cpu_mon_t *cmp)
{
	extern int perfcnt_arch_swtch_msg;	/* mtune/kernel */

	if ((cmp->cm_mixed_cpu_started      != r1nk_perf_architecture()) &&
	    (cmp->cm_perfcnt_arch_swtch_sig != 0                       ) &&
	    (!(cmp->cm_mixed_cpu_flags & 0x01)                         )) {

		/*
		 * should we print a warning message, so users
		 * can get the information via SYSLOG ?
		 */
		if (perfcnt_arch_swtch_msg) {
			cmn_err(CE_NOTE,FMT,
				cmp->cm_perfcnt_arch_swtch_sig,
				cmp->cm_pgid);
		}
			

		HWPERF_UTRACE('wrn1',cmp,cmp->cm_pgid);	

		signal(cmp->cm_pgid,cmp->cm_perfcnt_arch_swtch_sig);

		/*
		 * make sure the signal is send only once. I observed
		 * a race condition when several CPUs were trying to
		 * signal the process group (??)
		 */	
		cmp->cm_mixed_cpu_flags |= 0x01;
        }
}


#endif /* R10000 */


#if defined (SN) 

#include <sys/runq.h>
#include <ml/SN/error_private.h>

/*
 * ============================================================================
 */
__int64_t  l2_sbe_counter[MAXCPUS]; /* performance counter number to be used */
				    /* for L2$ SBE monitorring. A value of -1*/
				    /* indicates that the CPU is not setup   */
				    /* for monitorring			 */
__int64_t  l2_sbe_ticks[MAXCPUS];   /* incremented on a per CPU basis every  */
				    /* second. It is set to 0 if an error    */
				    /* occurs. After a number of systuneble  */
				    /* seconds have passed, the monitorring  */
				    /* is re-enabled. This is done to avoid  */
				    /* hogging a CPU if we have a solid SB   */
int	   l2_sbe_ready = 0;	    /* indiciation that the setup is complete*/



extern int l2_sbe_check; 		/*
 				 	 * systune l2_sbe_check
 					 *
 					 * 0x0000	no test
 					 * 0x0001	R10K
 					 * 0x0002	R12K
 					 * 0x0004	R12KS
 					 * 0x0008	R14K
 					 * 0x0010	reserved CPU type
 					 * 0x0020	reserved CPU type
 					 * 0x0040	reserved CPU type
 					 * 0x0080	reserved CPU type
 					 *
 					 * 0x0200	verbose output
 					 */

extern int l2_sbe_reset_sec;


void 		l2_sbe_init	(void);
void 		l2_sbe_intr	(void);
void 		l2_sbe_sched	(void);

extern void     scache_save     (int);


void
l2_sbe_init(void)
{
	cpu_cookie_t cookie;
        int          cpu;
	int	     s;
	int	     r10k_cnt  = 0;
	int          r12k_cnt  = 0;
	int	     verbose   = 0;
	rev_id_t     rev_id;

	/*
	 * cpu architecture specified ?
	 */
	if ((l2_sbe_check & 0xff) == 0)
		return;

	for (cpu = 0 ; cpu < maxcpus ; cpu++) {

		/*
		 * default initialization for this counter is off
	 	 */
		l2_sbe_counter[cpu] = -1;

		/*
		 * skip disabled CPUs
		 */
		if((pdaindr[cpu].CpuId == -1) ||
		   (!cpu_enabled(cpu))) 
			continue;

		/*
		 * get CPU type out of PDA, no need to
		 * access the processor register
		 */
		rev_id.ri_uint = pdaindr[cpu].pda->p_cputype_word;

		switch (rev_id.ri_imp) {

		case C0_IMP_R10000 : 
               		if (l2_sbe_check & 0x0001) {
               			l2_sbe_counter[cpu] = 0;
				r10k_cnt++;  
				if (l2_sbe_check & 0x100)
					verbose = 1;
			}
			break;

		case C0_IMP_R12000 : 
			if (l2_sbe_check & 0x0002) {
                       		l2_sbe_counter[cpu] = 0;
				r12k_cnt++;  
				if (l2_sbe_check & 0x100)
					verbose = 1;
			}
			break;

		default:
			break;
		}

		if (l2_sbe_counter[cpu] == -1)
			continue;

		s = splhi();
		cookie = setmustrun(cpu);

		l2_sbe_ticks[cpu] = l2_sbe_reset_sec;

		/*
		 * preset the performance counter to overflow on the first SBE.
		 * We will catch this in the clock interrupt
		 */

		r1nkperf_control_register_set(l2_sbe_counter[cpu], 
				((0x8 << 5) | 0xa | 0x10) );
		r1nkperf_data_register_set(l2_sbe_counter[cpu],
				((__uint32_t ) ( 1<< 31)) - 1);


		restoremustrun(cookie);
		splx(s);
	}


	if (verbose) {
		cmn_err(CE_NOTE,"Secondary Cache SBE Monitor CPU breakout :\n");
		if (r10k_cnt)
			cmn_err(CE_NOTE,
				"   R10K  : %d CPUs [using perf counter 0]\n",
				r10k_cnt);
		if (r12k_cnt)
			cmn_err(CE_NOTE,
				"   R12K  : %d CPUs [using perf counter 3]\n",
				r12k_cnt);

	}

	/*
	 * let l2_sbe_sched() know that we have been setup
	 */
	l2_sbe_ready = 1;
}



/*
 * called from counter_intr (ml/clksupport.c)
 *
 * we got here because the overflow of a performance counter register
 * indicates a cache error. We will do a check of the secondary cache
 * to catch a smoking gun and reset the counter to 0 to avoid
 * any new interrupts
 *
 */

/*
 * should be moved to sbd.h
 */
#define	IS_R12KS()   ((get_cpu_irr() & (C0_IMPMASK | C0_MAJREVMASK)) == 0xe30)


void
l2_sbe_intr(void)
{
	int cpu  = cpuid();
	int        syndrome;
	int        bit;

	extern int   scache_data_bit_error(int);
	extern char *scache_data_bit_location  (int bit);

        /*
         * stop triggering for SBE. If we have a solid SBE problem we
         * will end being called a lot ...
         * The clock interrupt will re-enable L2$ SBE monitorring for this
         * CPU
         */
        r1nkperf_control_register_set(l2_sbe_counter[cpu], (0x8 << 5));
        r1nkperf_data_register_set   (l2_sbe_counter[cpu], 0);


	if (l2_sbe_counter[cpu] == -1)
		return;

	/*
	 * if this is a R12KS, the syndrome bit is captured in the performance
	 * counter control register. Note that this is the latest syndrome captured
	 */
	if (IS_R12KS()) {
		syndrome = (r1nkperf_control_register_get(0) >> 23) & 0x1ff;
		bit      = scache_data_bit_error(syndrome);

		cmn_err(CE_WARN | CE_CPUID, "L2 cache SBE, failing bit %d %s\n",
			bit,scache_data_bit_location(bit));

	} else {
		extern int l2_cache_check(void);
		int    bit;

		if ((syndrome = l2_cache_check()) != -1) {
			if ((bit = scache_data_bit_error(syndrome)) == -1) {
				cmn_err(CE_WARN | CE_CPUID, 
					"L2 cache SBE, invalid syndrome 0x%x\n",
					syndrome);
			} else {
				cmn_err(CE_WARN | CE_CPUID, 
					"L2 cache SBE, failing bit %d %s\n",
					bit,scache_data_bit_location(bit));
			}
		}
	}


	l2_sbe_ticks[cpu] = 0;
}


/*
 * called from the memory scheduler once per second. 
 * 
 * check if there have been suspended L2$ SBE monitors and 
 * re-able them if their time has come.
 */
void
l2_sbe_sched(void)
{
	int cpu;
	int s;
	cpu_cookie_t cookie;


        /*
         * cpu architecture specified ?
         */
        if (((l2_sbe_check & 0xff) == 0) || l2_sbe_ready == 0)
                return;

	for (cpu = 0 ; cpu < maxcpus ; cpu++) {
		
		if (l2_sbe_counter[cpu] == -1)
			continue;

		if (l2_sbe_ticks[cpu]++ == l2_sbe_reset_sec) {

#if 0
			cmn_err(CE_NOTE,"re-enabling L2$ SBE monitor on CPU %d\n",cpu);
#endif

			s = splhi();
			cookie = setmustrun(cpu);

			r1nkperf_control_register_set(l2_sbe_counter[cpu], 
				((0x8 << 5) | 0xa | 0x10) );
                	r1nkperf_data_register_set(l2_sbe_counter[cpu],
                                ((__uint32_t ) ( 1<< 31)) - 1);

			restoremustrun(cookie);
			splx(s);



		} 
	}
}
			

#endif /* SN 	 */
