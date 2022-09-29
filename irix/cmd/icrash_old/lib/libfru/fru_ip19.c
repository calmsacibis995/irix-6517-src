/*
 * fru_ip19.c-
 *
 *	This file contains the code to analyze an IP19 board, the processor,
 * and the ASICs connected to it.
 *
 */

#define IP19	1
#define	R4000	1

#include "evfru.h"			/* FRU analyzer definitions */

#include <sys/EVEREST/IP19.h>           /* For CC chip */

#include "fru_pattern.h"
#include "sys/sbd.h"

int bitnum(int);

/* ARGSUSED */
void check_ip19(evbrdinfo_t *eb, everror_t *ee, everror_ext_t *eex, 
	      whatswrong_t *ww, evcfginfo_t *ec)
{
	int vid;
	int a_error, cc_error;
	int slice;
	fru_element_t src, dest, sw;
	uint cache_err;

#ifdef DEBUG
	if (fru_debug)
		FRU_PRINTF("Check IP19.\n");
#endif

	vid = eb->eb_cpu.eb_cpunum;

	a_error = ee->ip[vid].a_error;

	src.unit_type = FRU_IPBOARD;
	src.unit_num = NO_UNIT;

	/* We got an addr_here error.  No one responded. */
	conditional_update(ww, WITNESS, a_error, A_ERROR_ADDR_ERR, &src, NO_ELEM);

	/* ADDR_HERE */
	conditional_update(ww, WITNESS, a_error, 0xf000, &src, NO_ELEM);;

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

	if (a_error & A_ERROR_ADDR_HERE_TIMEOUT_ALL) {
		src.unit_type = FRU_A | FRUF_SOFTWARE;
		update_confidence(ww, PROBABLE, &src, NO_ELEM);
	}

	for (slice = 0; slice < EV_MAX_CPUS_BOARD; slice++) {
		if (!eb->eb_cpuarr[slice].cpu_enable)
			continue;
		sw.unit_num= src.unit_num = slice;
		sw.unit_type = FRU_IPBOARD | FRUF_SOFTWARE;

		vid = eb->eb_cpuarr[slice].cpu_vpid;

		cc_error = ee->cpu[vid].cc_ertoip;

		/**********************************
		 * Most likely a software problem *
		 **********************************/

#define CC_SW_BITS	IP19_CC_ERROR_MYREQ_TIMEOUT | \
			IP19_CC_ERROR_MYINT_TIMEOUT

		/* These things could easily have been caused by software */
		conditional_update(ww, POSSIBLE, cc_error, CC_SW_BITS,
				   &sw, NO_ELEM);

		/**********************************
		 * We might have caused the error *
		 **********************************/

                /* Someone caused an error - us or the other end. */
                src.unit_type = FRU_CC;
                dest.unit_type = FRU_A;
                conditional_update(ww, POSSIBLE, cc_error,
                                IP19_CC_ERROR_PARITY_A, &src, NO_ELEM);

		/* The hardware's not really flaky, but I want this to be
		 * a "weak" definition that can be replaced by any IO board
		 * that sees this event.  We still want priority over
		 * and MC3 or other processor board witnessing it.
		 */
		conditional_update(ww, FLAKY_HW, cc_error,
				IP19_CC_ERROR_MYRES_TIMEOUT, &src, NO_ELEM);

                dest.unit_type = FRU_D;
                conditional_update(ww, POSSIBLE, cc_error,
                                IP19_CC_ERROR_PARITY_D, &src, NO_ELEM);

		/*****************************************
		 * CC thinks this board caused the error *
		 *****************************************/

		/* CC believes it caused an error.  This one could be
		 * software so it needs to be lower priority than the
		 * A chip software rule.
		 */
		dest.unit_type = FRU_A;
		conditional_update(ww, POSSIBLE, cc_error,
				   IP19_CC_ERROR_MY_ADDR,
				   &src, &dest);

		/* CC believes it caused an error. */
		dest.unit_type = FRU_D;
		conditional_update(ww, PROBABLE, cc_error,
				   IP19_CC_ERROR_MY_DATA,
				   &src, &dest);

		/* CC saw secondary cache single bit error. */
		src.unit_type = FRU_CACHE;
		conditional_update(ww, PROBABLE, cc_error,
				   IP19_CC_ERROR_SCACHE_SBE,
				   &src, NO_ELEM);

		/********************************
		 * CC knows where the error is. *
		 ********************************/

		/* secondary cache: */
		src.unit_type = FRU_CACHE;
		conditional_update(ww, PROBABLE, cc_error,
				   IP19_CC_ERROR_SCACHE_MBE,
				   &src, NO_ELEM);

		/* bus tag problem */
		src.unit_type = FRU_BUSTAG;
		conditional_update(ww, DEFINITE, cc_error,
				   IP19_CC_ERROR_PARITY_TAGRAM, &src, NO_ELEM);

		/* synchronization between A chip and CC chip */
		dest.unit_type = FRU_A;
		dest.unit_num = NO_UNIT;
		src.unit_type = FRU_CC;
		conditional_update(ww, DEFINITE, cc_error, IP19_CC_ERROR_ASYNC,
				   &src, &dest);
		
		/* 
		 * Get the extended structure. If it is not set,
		 * ignore.
		 */
		if (eex == NULL) continue;
		
		cache_err = eex->eex_cpu[vid].cpu_cache_err;
		if (cache_err != 0) {
		        /*
		         * Secondary Cache MB ECC error.
		         */
		        src.unit_type = FRU_SYSAD;
		        conditional_update(ww, PROBABLE, cache_err,
				      CACHERR_EE, &src, NO_ELEM);
		        
		        if (!(cache_err & CACHERR_EE)) {
			      src.unit_type = FRU_PCACHE;
			      conditional_update(ww, DEFINITE, cache_err,
					    (CACHERR_ED | CACHERR_ET),
					    &src, NO_ELEM);

			      src.unit_type = FRU_SCACHE;
			      conditional_update(ww, DEFINITE, cache_err,
					    CACHERR_EC, &src, NO_ELEM);
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

int match_ip19board(fru_entry_t **token, everror_t *ee, evcfginfo_t *ec,
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
