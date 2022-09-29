/*
 * fru_ip25.c-
 *
 *	This file contains the code to analyze an IP25 board, the processor,
 * and the ASICs connected to it.
 *
 */

#define IP25	1
#define	R10000	1

#include "evfru.h"			/* FRU analyzer definitions */

#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/IP25.h>           /* For CC chip */
#include <sys/R10k.h>

#include "fru_pattern.h"

int bitnum(int);

/* ARGSUSED */
void check_ip25(evbrdinfo_t *eb, everror_t *ee, everror_ext_t *eex,
	      whatswrong_t *ww, evcfginfo_t *ec)
{
	int vid, pid;
	int a_error, cc_error;
	int slice;
	fru_element_t src, dest, sw;
	eccframe_t	*eccf;

#ifdef DEBUG
	if (fru_debug)
		FRU_PRINTF("Check IP25.\n");
#endif

	vid = eb->eb_cpu.eb_cpunum;
	a_error = ee->ip[vid].a_error;

	src.unit_type = FRU_IPBOARD;
	src.unit_num = NO_UNIT;

	/* We got an addr_here error.  No one responded. */
	conditional_update(ww, WITNESS, a_error, A_ERROR_ADDR_ERR, &src, 
			   NO_ELEM);

	/* ADDR_HERE */
	/*
	 * With R10K, spculation (loads) often cause ADDR_HERE errors. 
	 * For that reason, we assign a verw low priority to them 
	 * for IP25s.
	 */
	conditional_update(ww, WITNESS, a_error, 
			   A_ERROR_ADDR_HERE_TIMEOUT_ALL,
			   &src, NO_ELEM);

	/* MY address error */	
	conditional_update(ww, PROBABLE, a_error, A_ERROR_MY_ADDR_ERR,
			   &src, NO_ELEM);

	/* D parity error.  Doesn't always work due to chip bug. */
	src.unit_type = FRU_D;
	conditional_update(ww, FLAKY_HW, a_error, 0x0f00, &src, NO_ELEM);

	/* CC to A chip bus parity error */
	src.unit_type = FRU_CC;
	dest.unit_type = FRU_A;
	src.unit_num = bitnum(a_error & 0xf);
	conditional_update(ww, PROBABLE, a_error, 0xf, &src, NO_ELEM);

	for (slice = 0; slice < EV_MAX_CPUS_BOARD; slice++) {

		if (!eb->eb_cpuarr[slice].cpu_enable)
			continue;

		pid = (eb->eb_slot << EV_SLOTNUM_SHFT) + 
		       (slice << EV_PROCNUM_SHFT);

		sw.unit_num= src.unit_num = slice;
		sw.unit_type = FRU_IPBOARD | FRUF_SOFTWARE;

		/**********************************
		 * Most likely a software problem *
		 **********************************/
		/*
		 * PV #549894: FRU analyzer broken 
		 * (at least on challenge) in kudzu
		 *
		 * Addr Here errors, most likely caused by software.
		 */
		conditional_update(ww, PROBABLE, a_error, 
				   (A_ERROR_ADDR_HERE_TIMEOUT_ALL >> slice),
				   &sw, NO_ELEM);

		/* CC_ERROR register */
		vid = eb->eb_cpuarr[slice].cpu_vpid;
		cc_error = ee->cpu[vid].cc_ertoip;
		
		/* These things could easily have been caused by software */
		conditional_update(ww, WITNESS, cc_error, 
				   IP25_CC_ERROR_MYREQ_TIMEOUT,&sw, NO_ELEM);

		/********************************************
		 * Most likely a H/W problem somewhere else *
		 ********************************************/

		/* On IP25, this is most likely caused by another board 
		   inhibiting forever. The CPU doing that is the problem
		   but we have no way to know here who that is.*/
		dest.unit_type = FRU_CC;
                conditional_update(ww, POSSIBLE, cc_error, 
				   IP25_CC_ERROR_MYINT_TIMEOUT, 
				   NO_ELEM, &dest);

		/**********************************
		 * We might have caused the error *
		 **********************************/

                /* Someone caused an error - us or the other end. */
                src.unit_type = FRU_CC;
                dest.unit_type = FRU_A;
                conditional_update(ww, POSSIBLE, cc_error,
                                IP25_CC_ERROR_PARITY_A, &src, NO_ELEM);

		/* The hardware's not really flaky, but I want this to be
		 * a "weak" definition that can be replaced by any IO board
		 * that sees this event.  We still want priority over
		 * and MC3 or other processor board witnessing it.
		 */
		conditional_update(ww, FLAKY_HW, cc_error,
				IP25_CC_ERROR_MYRES_TIMEOUT, &src, NO_ELEM);

                dest.unit_type = FRU_D;
                conditional_update(ww, POSSIBLE, cc_error,
                                IP25_CC_ERROR_PARITY_D, &src, NO_ELEM);

		/*****************************************
		 * CC thinks this board caused the error *
		 *****************************************/

		/* CC believes it caused an error.  This one could be
		 * software so it needs to be lower priority than the
		 * A chip software rule. 
		 * On R10k, this error is asserted all the time so we make 
		 * it really low.
		 */
		dest.unit_type = FRU_A;
		conditional_update(ww, WITNESS, cc_error,
				   IP25_CC_ERROR_MY_ADDR,
				   &src, &dest);

		/* CC believes it caused an error. */
		dest.unit_type = FRU_D;
		conditional_update(ww, PROBABLE, cc_error,
				   IP25_CC_ERROR_MY_DATA,
				   &src, &dest);

		/* Saw correctable error - flag it low so just about anything
		   else will override it. */

		src.unit_type = FRU_SYSAD;
		conditional_update(ww, WITNESS, cc_error,
				   IP25_CC_ERROR_SBE_INTR,
				   &src, NO_ELEM);

		/********************************
		 * CC knows where the error is. *
		 ********************************/

#define	CC_CPU	(IP25_CC_ERROR_MBE_INTR|\
		 IP25_CC_ERROR_PARITY_SS|IP25_CC_ERROR_PARITY_SC)

		src.unit_type = FRU_SYSAD;
		conditional_update(ww, PROBABLE, cc_error, CC_CPU, 
				   &src, NO_ELEM);

		/* 
		   bus tag problem - this should only be a problem if we
		   are getting multiple bit failing since bad parity 
		   will still forward the transaction to the T5
		   on IP25. But right now, we do not recover at all.
		 */
		   
		src.unit_type = FRU_BUSTAG;
		conditional_update(ww, PROBABLE, cc_error,
				   IP25_CC_ERROR_PARITY_TAGRAM, &src, NO_ELEM);

		/* synchronization between A chip and CC chip */
		dest.unit_type = FRU_A;
		dest.unit_num = NO_UNIT;
		src.unit_type = FRU_CC;
		conditional_update(ww, DEFINITE, cc_error, IP25_CC_ERROR_ASYNC,
				   &src, &dest);

		/* 
		 * Cache Errors
		 *
		 * The R10000 propogates cache errors to avoid 
		 * ever consuming data.  
		 */
		eccf = (eccframe_t *)
		    (((eframe_t **)(eex->eex_ecc))
			[pid] + 1);

		/*
		 * If we panic'd because of a cache error, there is a good 
		 * chance that we are the bad guy.
		 */
		if (eccf && (eccf->eccf_status == ECCF_STATUS_PANIC))  {
		        __uint64_t cer;
			cer = eccf->eccf_cache_err;
			switch(cer & CE_TYPE_MASK) {
			case CE_TYPE_I:	/* can not cause this */
			        break;
			case CE_TYPE_D:
				src.unit_type = FRU_PCACHE;
				conditional_update(ww, DEFINITE, cer, 
						   CE_D_MASK|CE_TA_MASK|
						   CE_TM_MASK|CE_TS_MASK, 
						   &src, NO_ELEM);
				break;
			case CE_TYPE_S:
				src.unit_type = FRU_SCACHE;
				conditional_update(ww, DEFINITE, cer, 
						   CE_D_MASK|CE_TA_MASK, 
						   &src, NO_ELEM);
				break;
			case CE_TYPE_SIE:	
				/* 
				 * We get this for interface error or a 
				 * propogated error. So flag this guy, but 
				 * LOWER prio tham the above errors - so 
				 * the originator is flagged.
				 */
				src.unit_type = FRU_SYSAD;
				conditional_update(ww, PROBABLE, cer, 
						   CE_SA|CE_SC|CE_SR|
						   CE_D_MASK,
						   &src, NO_ELEM);
				
				break;
		    }
		}
	    
		
	}

#ifdef DEBUG
	if (fru_debug)
		display_whatswrong(ww, ec);
#endif
    
	return;
}


#ifdef FRU_PATTERN_MATCHER

int match_ip25board(fru_entry_t **token, everror_t *ee, evcfginfo_t *ec,
			whatswrong_t *ww, fru_case_t *case_ptr)
{
        *token = find_board_end(*token);

	return 1;
}

#endif /* FRU_PATTERN_MATCHER */

int bitnum(int value)
{
	int bit;

	for (bit = 0; value > 1; value >>= 1)
		bit++;

	return bit;
}
