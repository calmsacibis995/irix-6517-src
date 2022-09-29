/***********************************************************************\
*	File:		hub_diags.c						*
*									*
*	This file contains the IP27 PROM Utility Library routines for	*
*	manipulating miscellaneous features of the hub.			*
*									*
\***********************************************************************/

#ident "$Revision: 1.9 $"

#include <sys/types.h>
#include <sys/nic.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>

#include <sys/SN/agent.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/intr.h>
#include <sys/SN/kldiag.h>

#include <libkl.h>
#include <report.h>
#include <prom_msgs.h>

#include "ip27prom.h"
#include "hub_diags.h"
#include "hub.h"
#include "libc.h"
#include "libasm.h"

#define CAUSE_TRIES	64
#define DIAG_IP_MASK	0x3c00

/*
 * test_cause
 *
 *   Poll the CAUSE register until the specified int. level bit (imask)
 *   equals the required value (itest, which is either 0 or imask).
 *   Returns error on time-out or if unrelated interrupt levels are set.
 */


static int test_cause(__uint64_t imask, int itest, char *iname)
{
    __uint64_t		cs, err_int_pend;
    int			i;
#if D_DEBUG
	printf("In test_cause for itest %d and imask %lx\n", itest, imask);
#endif
    for (i = 0; i < CAUSE_TRIES; i++)
	if (((cs = get_cop0(C0_CAUSE)) & imask) == itest)
	    break;

    if (i == CAUSE_TRIES) {
#if D_DEBUG
	printf("%C: Failed to %s %s interrupt (cause=0x%lx)\n",
	       itest ? "receive" : "clear", iname, cs);
#endif
	return -1;
    }

#if D_DEBUG
    err_int_pend = LD(LOCAL_HUB(PI_ERR_INT_PEND));
    printf("ERR_INT_PEND register:0x%lx\n", err_int_pend);
#endif
    
    if ((cs & DIAG_IP_MASK) != itest) {
#if D_DEBUG
	printf("%C: Spurious interrupts (cause=0x%lx, expected 0x%lx)\n",
	       cs, itest);
#endif
	return -1;
    }

    return 0;
}

/*
 * hub_intrpt_diag
 *
 *   Simple diagnostic for interrupts as viewed from one CPU.
 *   This test is called by main.c in the IP27 prom and returns 
 *   KLDIAG_HUB_INTS_FAILED if it fails, KLDIAG_PASSED if it passes. 
 */

int hub_intrpt_diag(diag_mode)
{
    int		diag_rc = 0;
    int		verbose = 0;
    int 	i;


    if (diag_mode == DIAG_MODE_NONE) {
        return 0;

    } else {
	
	if (diag_mode == DIAG_MODE_MFG) {
	    verbose = 1;
	}

	/*
	 * Clear all interrupts to get around problem of interactions
	 * with Mohan's test.
	 */

    	/*
     	 * Mask in interrupt 63, send it, and verify that the
     	 * L2 interrupt (IP2) comes on in the CAUSE register.
     	 * Clear it, and verify the L2 interrupt turns off.
     	 */

    	hub_int_set(63);
    	hub_int_mask_in(63);


    	if (test_cause(HUB_IP_PEND0, HUB_IP_PEND0, "L2") < 0) {

            if (verbose) {
                printf("CPU %d on node n%d could not receive L2 interrupt.\n",
                        hub_cpu_get(), hub_slot_get());         
            }

	    diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_INTS_FAILED);

	    result_diagnosis("hub_intrpt_diag", hub_slot_get(), hub_cpu_get());
	    result_fail("hub_intrpt_diag", diag_rc, 
			get_diag_string(diag_rc));
	
	    return diag_rc;
    	}

    	hub_int_clear(63);

    	if (test_cause(HUB_IP_PEND0, 0, "L2") < 0) {

	    if (verbose) {
		printf("CPU %d on node n%d could not clear L2 interrupt.\n",
			hub_cpu_get(), hub_slot_get());		
	    }

	    diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_INTS_FAILED);

	    result_diagnosis("hub_intrpt_diag", hub_slot_get(), hub_cpu_get());
	    result_fail("hub_intrpt_diag", diag_rc,
			get_diag_string(diag_rc));

	    return diag_rc;
    	}

    	hub_int_mask_out(63);

    	/*
     	 * Mask in interrupt 64, send it, and verify that the
     	 * L3 interrupt (IP3) comes on in the CAUSE register.
      	 * Clear it, and verify the interrupt turns off.
     	 */

    	hub_int_mask_in(64);

    	hub_int_set(64);

    	if (test_cause(HUB_IP_PEND1_CC, HUB_IP_PEND1_CC, "L3") < 0) {
            diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_INTS_FAILED);
	    result_diagnosis("hub_intrpt_diag", hub_slot_get(), hub_cpu_get());
            result_fail("hub_intrpt_diag", diag_rc, 
			get_diag_string(diag_rc));
	    return diag_rc;
        }

        hub_int_clear(64);

        if (test_cause(HUB_IP_PEND1_CC, 0, "L3") < 0) {
	    diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_INTS_FAILED);
	    result_diagnosis("hub_intrpt_diag", hub_slot_get(), hub_cpu_get());
            result_fail("hub_intrpt_diag", diag_rc, 
			get_diag_string(diag_rc));
	    return diag_rc;
        }

        hub_int_mask_out(64);

    	/*
     	 * All interrupts are left off
     	 */

    	result_pass("hub_intrpt_diag", diag_mode);
    	return diag_rc;

    } /* else */

} /* hub_intrpt_diag */

/*
 * hub_bte_diag
 *
 *   Simple diagnostic to test the bte logic on the hub chip. 
 *   It uses both btes in the simple copy mode, and checks the xfer.
 *   This test is called by main.c in the IP27 prom and returns
 *   KLDIAG_HUB_BTE_FAILED if it fails, KLDIAG_PASSED if it passes.
 */

int hub_bte_diag(diag_mode)
{
    int         	diag_rc = 0;
    rtc_time_t		start, timeout;
	
    if (diag_mode == DIAG_MODE_NONE) {
        return 0;
    }
    else {

	/* initialize source and dest memory for bte to use */

	memset((void *) BTE0_SOURCE, SOURCE0_VALUE, BTE_CPY_LENGTH);
	memset((void *) BTE1_SOURCE, SOURCE1_VALUE, BTE_CPY_LENGTH);
	memset((void *) BTE0_DEST, DEST_VALUE, BTE_CPY_LENGTH);
	memset((void *) BTE1_DEST, DEST_VALUE, BTE_CPY_LENGTH);

    	/* program bte0 to perform transfer */

	SD(LOCAL_HUB(IIO_BASE_BTE0 + BTEOFF_STAT), IBLS_VALUE);
	SD(LOCAL_HUB(IIO_BASE_BTE0 + BTEOFF_SRC), BTE0_SOURCE);
	SD(LOCAL_HUB(IIO_BASE_BTE0 + BTEOFF_DEST), BTE0_DEST);
	SD(LOCAL_HUB(IIO_BASE_BTE0 + BTEOFF_INT), IBIA0_VALUE);

	/* program bte1 to perform transfer */

	SD(LOCAL_HUB(IIO_BASE_BTE1 + BTEOFF_STAT), IBLS_VALUE);
	SD(LOCAL_HUB(IIO_BASE_BTE1 + BTEOFF_SRC), BTE1_SOURCE);
	SD(LOCAL_HUB(IIO_BASE_BTE1 + BTEOFF_DEST), BTE1_DEST);
	SD(LOCAL_HUB(IIO_BASE_BTE1 + BTEOFF_INT), IBIA1_VALUE);

	/* kick both btes to start transfer and start the timer */

	SD(LOCAL_HUB(IIO_BASE_BTE0 + BTEOFF_CTRL), IBCT_VALUE);
	SD(LOCAL_HUB(IIO_BASE_BTE1 + BTEOFF_CTRL), IBCT_VALUE);

	start = rtc_time();
	timeout = start + MAX_XFER_TIME;

	/* 
	 * Poll for completion interrupts and then clear them 
	 */

 poll_ints:

	if (hub_int_test(BTE0_DONE_INT)) { /* BTE 0 is done */
	    DM_PRINTF(("BTE0 completed.\n"));
            printf("\0\0\0\0\0\0\0\0\0\0") ;  /* some delay */
	    /* XXX Removing this printf causes bte to hang. WHY ?? */

	    if (hub_int_test(BTE1_DONE_INT)) { /* BTE 1 is done too */
		DM_PRINTF(("BTE1 completed.\n"));
            	printf("\0\0\0\0\0\0\0\0\0\0") ;  /* some delay */
	    /* XXX Removing this printf causes bte to hang. WHY ?? */

		hub_int_clear(BTE0_DONE_INT);
        	hub_int_clear(BTE1_DONE_INT);
	    }

	} else if (rtc_time() > timeout) { /* BTE is hung */

	    printf("INT_PEND0: %lx\n", LD(LOCAL_HUB(PI_INT_PEND0)));
	    printf("INT_PEND1: %lx\n", LD(LOCAL_HUB(PI_INT_PEND1)));

	    diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_BTE_HUNG);
	    result_diagnosis("hub_bte_diag", hub_slot_get(), hub_cpu_get());
	    result_fail("hub_bte_diag", diag_rc, get_diag_string(diag_rc));
	    return diag_rc;
	
	} else { /* keep polling */

	    goto poll_ints;
	}

	/* 
	 * Verify that bte0 transfer occurred correctly by reading dest 
	 */

	if (memcmp8((__uint64_t *) BTE0_SOURCE, 
		    (__uint64_t *) BTE0_DEST, BTE_CPY_LENGTH)) {

	    diag_rc  = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_BTE_FAILED); 
	    result_diagnosis("hub_bte_diag", hub_slot_get(), hub_cpu_get());
	    result_fail("hub_bte_diag", diag_rc, get_diag_string(diag_rc));
	    return diag_rc;
	}

        /*
         * Verify that bte1 transfer occurred correctly by reading dest 
         */

        if (memcmp8((__uint64_t *) BTE1_SOURCE, 
		    (__uint64_t *) BTE1_DEST, BTE_CPY_LENGTH)) { 

            diag_rc  = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_BTE_FAILED);
	    result_diagnosis("hub_bte_diag", hub_slot_get(), hub_cpu_get());
            result_fail("hub_bte_diag", diag_rc, get_diag_string(diag_rc));
            return diag_rc;
        }
	
    	result_pass("hub_bte_diag", diag_mode);
    	return diag_rc;

    } 

} /* hub_bte_diag */
