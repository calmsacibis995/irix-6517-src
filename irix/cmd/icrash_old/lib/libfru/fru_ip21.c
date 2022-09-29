/*
 * fru_ip21.c-
 *
 *      This file contains the code to analyze an IP21 board, the processor,
 * and the ASICs connected to it.
 *
 */

#define IP21	1
#define TFP	1

#include "evfru.h"			/* FRU analyzer definitions */

#include <sys/EVEREST/IP21.h>           /* For BBCC chip */
#include <sys/tfp.h>

#include "fru_pattern.h"

/* ARGSUSED */
void check_ip21(evbrdinfo_t *eb, everror_t *ee, everror_ext_t *eex,
	      whatswrong_t *ww, evcfginfo_t *ec)
		/* ec is here so we can determine what's actually a FRU */
{
	int vid;
	int a_error, c_error, i_error;
	int slice;
	fru_element_t src, dest, sw;

#ifdef DEBUG
	if (fru_debug)
		FRU_PRINTF("Check IP21.\n");
#endif

	src.unit_type = FRU_IPBOARD;
	src.unit_num = NO_UNIT;

	vid = eb->eb_cpu.eb_cpunum;

	a_error = ee->ip[vid].a_error;

	if (a_error & A_ERROR_ADDR_ERR)	/* We got an addr_here error. */
		update_confidence(ww, WITNESS, &src, NO_ELEM);

	if (a_error & 0xf000)	/* ADDR_HERE */
		update_confidence(ww, WITNESS, &src, NO_ELEM);

	if (a_error & 0x0f00)	/* D parity error.  Doesn't always work. */
		update_confidence(ww, POSSIBLE, &src, NO_ELEM);

	if (a_error & A_ERROR_MY_ADDR_ERR)	/* MY address error */
		update_confidence(ww, PROBABLE, &src, NO_ELEM);

	if (a_error & 0xf)	/* CC to A chip bus parity error */
		update_confidence(ww, PROBABLE, &src, NO_ELEM);

	if (a_error & A_ERROR_ADDR_HERE_TIMEOUT_ALL) {
		fru_element_t gc;
		gc.unit_type = FRU_GCACHE;
		/* We only have two CPUs per board on IP21 so... */
		if (a_error & A_ERROR_ADDR_HERE_TIMEOUT0)
			gc.unit_num = 0;
		else
			gc.unit_num = 1;
		
		sw.unit_type = FRU_IPBOARD | FRUF_SOFTWARE;
		sw.unit_num = NO_UNIT;
		update_confidence(ww, PROBABLE, &gc, &sw);
	}

	for (slice = 0; slice < EV_MAX_CPUS_BOARD; slice++) {

		if (!eb->eb_cpuarr[slice].cpu_enable)
			continue;

		src.unit_num = dest.unit_num = slice;

#ifdef DEBUG
		if (fru_debug)
			FRU_PRINTF("Slice %d\n", slice);
#endif

		vid = eb->eb_cpuarr[slice].cpu_vpid;

		c_error = ee->cpu[vid].cc_ertoip;

		i_error = ee->cpu[vid].external_intr;

#ifdef DEBUG
		if (fru_debug) {
			FRU_PRINTF("   i_error = 0x%x\n", i_error);
			FRU_PRINTF("     mask == 0x%x\n",
			    ((SRB_GPARITYE | SRB_GPARITYO) >> CAUSE_IPSHIFT));
			FRU_PRINTF("     cause == 0x%x\n",
			    i_error<<CAUSE_IPSHIFT);
		}
#endif

		if (i_error & ((SRB_GPARITYE | SRB_GPARITYO) >> CAUSE_IPSHIFT)){
			src.unit_type = FRU_GCACHE;
			update_confidence(ww, DEFINITE, &src, NO_ELEM);
		}

#ifdef DEBUG
		if (fru_debug)
			FRU_PRINTF("   error == 0x%x\n", c_error);
#endif

#if 0
		/* This code was taken from various sources.  The Diagnostic
		 * Roadmap code should supercede it.  I'm not willing to delete
		 * it yet, though, just in case.
		 */


		/* These things could easily have been caused by software */
		if (c_error & (IP21_CC_ERROR_MYREQ_TIMEOUT |
				IP21_CC_ERROR_MYRES_TIMEOUT))
			update_confidence(ww, WITNESS, &src, NO_ELEM);

		/* Someone caused an error */
		if (c_error & (IP21_CC_ERROR_PARITY_A | IP21_CC_ERROR_PARITY_D))
			update_confidence(ww, WITNESS, &src, NO_ELEM);

		/*
		 * SOME IP21 caused this problem, but we don't know which
		 * one, so call it "possible."  All boards should be tagged
		 * with this.
		 */
		if (c_error & (IP21_CC_ERROR_DB0_PE |
				IP21_CC_ERROR_DB1_PE))
			update_confidence(ww, POSSIBLE, &src, NO_ELEM);

		if (c_error & (IP21_CC_ERROR_MY_ADDR | IP21_CC_ERROR_MY_DATA))
			update_confidence(ww, PROBABLE, &src, NO_ELEM);

		if (c_error & (IP21_CC_ERROR_ASYNC))
			update_confidence(ww, DEFINITE, &src, NO_ELEM);

		/* From Bob Newhall's factory notes. */
		if (c_error & (IP21_CC_ERROR_DATA_SENT0 |
				IP21_CC_ERROR_DATA_SENT1))
			update_confidence(ww, DEFINITE, &src, NO_ELEM);

		/* From Bob Newhall's factory notes. */
		if ((c_error & (IP21_CC_ERROR_DB0_PE | IP21_CC_ERROR_DB1_PE))
			   && (c_error & (IP21_CC_ERROR_DATA_SENT0 |
					IP21_CC_ERROR_DATA_SENT1))) {
			src.unit_type = FRU_GCACHE;
			update_confidence(ww, DEFINITE, &src, NO_ELEM);
		}

#endif /* 0 */

		/**********************************************************
		 *  The following are taken from the Diagnostic Roadmap   *
		 **********************************************************/

		if (c_error & (IP21_CC_ERROR_DB0_PE | IP21_CC_ERROR_DB1_PE)) {
			if (c_error & IP21_CC_ERROR_DATA_SENT0) {
				/* Between FPU and DB. */
				src.unit_type = FRU_FPU;
				dest.unit_type = FRU_DB;
				update_confidence(ww, DEFINITE, &src, &dest);
			} else if (c_error & IP21_CC_ERROR_DATA_SENT1) {
				/* Between Gcache and DB. */
				src.unit_type = FRU_GCACHE;
				dest.unit_type = FRU_NONE;  /* was FRU_DB */
				/* Apparently, this is always gcache. */
				update_confidence(ww, DEFINITE, &src, &dest);
			} else if (c_error & IP21_CC_ERROR_DATA_RECV) {
				/* May have gotten bad data from Ebus,
				 * Could be one of the D chips. */
				src.unit_type = FRU_D;
				update_confidence(ww, WITNESS, &src, NO_ELEM);
			} else {
				/* Either another board or else IP21
				 * introduced it between Ebus and DB.
				 * Make it a possible error since we've
				 * seen it alone with no indicators
				 * from other boards.
				 */
				src.unit_type = FRU_IPBOARD;
				update_confidence(ww, POSSIBLE, &src, NO_ELEM);
			}
		}
			
		if (c_error & IP21_CC_ERROR_PARITY_A) {
			if (c_error & (IP21_CC_ERROR_MY_ADDR0 |
					IP21_CC_ERROR_MY_ADDR1)) {
				/* Came from the Ebus */
				src.unit_type = FRU_IPBOARD;
				update_confidence(ww, WITNESS, &src, NO_ELEM);
			} else {
				/* between A and CC */
				src.unit_type = FRU_A;
				dest.unit_type = FRU_BBCC;
				update_confidence(ww, DEFINITE, &src, &dest);
			}
		}

		if (c_error & (IP21_CC_ERROR_MY_ADDR0 |
				IP21_CC_ERROR_MY_ADDR1)) {
			src.unit_type = FRU_IPBOARD;
			if (c_error & IP21_CC_ERROR_PARITY_A) {
				/* Came in from Ebus. */
				update_confidence(ww, WITNESS, &src, NO_ELEM);
			} else {
				/* XXX- What should this be? */
				/* Already covered? */
				update_confidence(ww, POSSIBLE, &src, NO_ELEM);
			}
		}

		if (c_error & (IP21_CC_ERROR_DATA_SENT0 |
				IP21_CC_ERROR_DATA_SENT1)) {
			src.unit_type = FRU_DB;
			dest.unit_type = FRU_D;

			/* The case with either of these bits set is covered. */
			if (!(c_error & (IP21_CC_ERROR_DB0_PE |
					IP21_CC_ERROR_DB1_PE)))
				update_confidence(ww, DEFINITE, &src, &dest);
		}

		/* Error came from the Ebus */
		src.unit_type = FRU_IPBOARD;
		conditional_update(ww, WITNESS, c_error,
				   IP21_CC_ERROR_DATA_RECV, &src, NO_ELEM);

		/* CC and A disagree on Ebus state.  One's broken. */
		src.unit_type = FRU_A;
		dest.unit_type = FRU_BBCC;
		conditional_update(ww, DEFINITE, c_error,
				   IP21_CC_ERROR_ASYNC, &src, &dest);

		if (c_error & (IP21_CC_ERROR_MYREQ_TIMEOUT0 |
				IP21_CC_ERROR_MYREQ_TIMEOUT1)) {
			/* src + dest are set correctly from above */
			/* Between CC and A, in A, or some other
			 * board kept us off the Ebus */
			update_confidence(ww, PROBABLE, &src, &dest);
		}

                /* Most likely a software problem */
		conditional_update(ww, POSSIBLE, c_error,
				   IP21_CC_ERROR_MYRES_TIMEOUT0, &sw, NO_ELEM);

		/* Could be a problem with CC chip, A chip or bus tags.
		 * Since it's likely either A, bus tag, or someone else's
		 * CC chip, call out the A and bus tags.
		 */
		src.unit_type = FRU_A;
		dest.unit_type = FRU_BUSTAG;
		conditional_update(ww, POSSIBLE, c_error,
				   IP21_CC_ERROR_MYRES_TIMEOUT1, &src, &dest);

		/* Between CC and DB */
		src.unit_type = FRU_CC;
		dest.unit_type = FRU_DB;
		conditional_update(ww, DEFINITE, c_error,
				   IP21_CC_ERROR_PARITY_DBCC, &src, &dest);
	}

#ifdef DEBUG
	if (fru_debug)
		display_whatswrong(ww, ec);
#endif

	return;

}

#ifdef FRU_PATTERN_MATCHER

int match_ip21board(fru_entry_t **token, everror_t *ee, evcfginfo_t *ec,
		    whatswrong_t *ww, fru_case_t *case_ptr)
{
        *token = find_board_end(*token);

	return 1;
}

#endif /* FRU_PATTERN_MATCHER */

