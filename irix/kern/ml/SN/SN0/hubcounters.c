/*
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/nodepda.h>
#include <sys/iograph.h>
#include <sys/SN/agent.h>
#include <sys/SN/router.h>
#include <sys/SN/SN0/hubstat.h>
#include <sys/SN/SN0/sn0drv.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/slotnum.h>
#include "sn0_private.h"

#define HUB_CAPTURE_TICKS	(2 * HZ)

#define HUB_ERR_THRESH		500

volatile int hub_print_usecs = 600 * USEC_PER_SEC;

/* Return success if the hub's crosstalk link is working */
int
hub_xtalk_link_up(nasid_t nasid)
{
	hubreg_t	llp_csr_reg;

	/* Read the IO LLP control status register */
	llp_csr_reg = REMOTE_HUB_L(nasid, IIO_LLP_CSR);

	/* Check if the xtalk link is working */
	if (llp_csr_reg & IIO_LLP_CSR_IS_UP) 
		return(1);

	return(0);

	
}

static char *error_flag_to_type(unsigned char error_flag)
{
    switch(error_flag) {
    case 0x1: return ("NI retries");
    case 0x2: return ("NI SN errors");
    case 0x4: return ("NI CB errors");
    case 0x8: return ("II CB errors");
    case 0x10: return ("II SN errors");
    default: return ("Errors");
    }
}

int
print_hub_error(hubstat_t *hsp, hubreg_t reg,
		__int64_t delta, unsigned char error_flag)
{
	__int64_t rate;

	reg *= hsp->hs_per_minute;	/* Convert to minutes */
	rate = reg / delta;

	if (rate > HUB_ERR_THRESH) {
		
		if(hsp->hs_maint & error_flag) 
		{
		cmn_err(CE_WARN ,
			"Excessive %s (%d/min) on %s",
			error_flag_to_type(error_flag), rate, hsp->hs_name); 
		}
		else 
		{
		   hsp->hs_maint |= error_flag;
		cmn_err(CE_WARN | CE_MAINTENANCE,
			"Excessive %s (%d/min) on %s",
			error_flag_to_type(error_flag), rate, hsp->hs_name); 
		}
		return 1;
	} else {
		return 0;
	}
}


int
check_hub_error_rates(hubstat_t *hsp)
{
	__int64_t delta = hsp->hs_timestamp - hsp->hs_timebase;
	int printed = 0;

	printed += print_hub_error(hsp, hsp->hs_ni_retry_errors,
				   delta, 0x1);

#if 0
	printed += print_hub_error(hsp, hsp->hs_ni_sn_errors,
				   delta, 0x2);
#endif

	printed += print_hub_error(hsp, hsp->hs_ni_cb_errors,
				   delta, 0x4);


	/* If the hub's xtalk link is not working there is 
	 * no need to print the "Excessive..." warning 
	 * messages
	 */
	if (!hub_xtalk_link_up(hsp->hs_nasid))
		return(printed);


	printed += print_hub_error(hsp, hsp->hs_ii_cb_errors,
				   delta, 0x8);

	printed += print_hub_error(hsp, hsp->hs_ii_sn_errors,
				   delta, 0x10);

	return printed;
}


void
capture_hub_stats(cnodeid_t cnodeid, struct nodepda_s *npda)
{
	nasid_t nasid;
	hubstat_t *hsp = &(npda->hubstats);
	hubreg_t port_error;
	hubii_illr_t illr;
	int count;
	int overflow = 0;

	/*
	 * If our link wasn't up at boot time, don't worry about error rates.
	 */
	if (!(hsp->hs_ni_stat_rev_id & NSRI_LINKUP_MASK))
		return;

	nasid = COMPACT_TO_NASID_NODEID(cnodeid);

	hsp->hs_timestamp = absolute_rtc_current_time();

	port_error = REMOTE_HUB_L(nasid, NI_PORT_ERROR_CLEAR);
	count = ((port_error & NPE_RETRYCOUNT_MASK) >> NPE_RETRYCOUNT_SHFT);
	hsp->hs_ni_retry_errors += count;
	if (count == NPE_COUNT_MAX)
		overflow = 1;
	count = ((port_error & NPE_SNERRCOUNT_MASK) >> NPE_SNERRCOUNT_SHFT);
	hsp->hs_ni_sn_errors += count;
	if (count == NPE_COUNT_MAX)
		overflow = 1;
	count = ((port_error & NPE_CBERRCOUNT_MASK) >> NPE_CBERRCOUNT_SHFT);
	hsp->hs_ni_cb_errors += count;
	if (overflow || count == NPE_COUNT_MAX)
		hsp->hs_ni_overflows++;

	if (port_error & NPE_FATAL_ERRORS) {
		hubni_error_handler("capture_hub_stats", 1);
	}

	illr.illr_reg_value = REMOTE_HUB_L(nasid, IIO_LLP_LOG);
	REMOTE_HUB_S(nasid, IIO_LLP_LOG, 0);

	hsp->hs_ii_sn_errors += illr.illr_fields_s.illr_sn_cnt;
	hsp->hs_ii_cb_errors += illr.illr_fields_s.illr_cb_cnt;
	if ((illr.illr_fields_s.illr_sn_cnt == IIO_LLP_SN_MAX) ||
	    (illr.illr_fields_s.illr_cb_cnt == IIO_LLP_CB_MAX))
		hsp->hs_ii_overflows++;

	if (hsp->hs_print) {
		if (check_hub_error_rates(hsp)) {
			hsp->hs_last_print = absolute_rtc_current_time();
			hsp->hs_print = 0;
		}
	} else {
		if ((absolute_rtc_current_time() -
		    hsp->hs_last_print) > hub_print_usecs)
			hsp->hs_print = 1;
	}
		
	npda->hubticks = HUB_CAPTURE_TICKS;
}


void
init_hub_stats(cnodeid_t cnodeid, struct nodepda_s *npda)
{
	hubstat_t *hsp = &(npda->hubstats);
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cnodeid);
	char slotname[SLOTNUM_MAXLENGTH];

	bzero(&(npda->hubstats), sizeof(hubstat_t));

	hsp->hs_version = HUBSTAT_VERSION;
	hsp->hs_cnode = cnodeid;
	hsp->hs_nasid = nasid;
	hsp->hs_timebase = absolute_rtc_current_time();
	hsp->hs_ni_stat_rev_id = REMOTE_HUB_L(nasid, NI_STATUS_REV_ID);

	/* Clear the II error counts. */
	REMOTE_HUB_S(nasid, IIO_LLP_LOG, 0);

	/* Clear the NI counts. */
	REMOTE_HUB_L(nasid, NI_PORT_ERROR_CLEAR);

	hsp->hs_per_minute = (long long)IP27_RTC_FREQ * 1000LL * 60LL;

	npda->hubticks = HUB_CAPTURE_TICKS;

	hsp->hs_name = (char *)kmem_alloc_node(MAX_HUB_PATH, VM_NOSLEEP, cnodeid);
	ASSERT_ALWAYS(hsp->hs_name);

	get_slotname(npda->slotdesc, slotname);

	sprintf(hsp->hs_name, "/hw/" EDGE_LBL_MODULE "/%d/"
	        EDGE_LBL_SLOT "/%s/" EDGE_LBL_NODE "/" EDGE_LBL_HUB,
		npda->module_id, slotname);

	hsp->hs_last_print = 0;
	hsp->hs_print = 1;

	hub_print_usecs = hub_print_usecs;

}

