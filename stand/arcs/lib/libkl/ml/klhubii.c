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
 * File: klhubii.c
 * Hub support for prom. This deals only with the II interface.
 * It is implemented as a general set of utility routines with
 * the hub_base address as a parameter, so that the same set of
 * routines can be used to initialize a 'headless' HUB, a HUB
 * with no CPUs attached.
 */


#include <sys/types.h>
#include <sys/SN/agent.h>
#include <sys/SN/xbow.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/intr.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/PCI/bridge.h>
#include <libkl.h>
#include <prom_msgs.h>
#include <rtc.h>
#include <sys/xtalk/xtalk.h>

#undef DELAY
#define DELAY microsec_delay

extern void      delay(ulong);

#define II_DELAY_TIME	10000
#define HALF_SEC       	 500000

void
hubii_llp_enable(nasid_t nasid)
{
	hubii_ilcsr_t	ii_csr;	/* the control status register */
	
	ii_csr.icsr_reg_value = LD(REMOTE_HUB(nasid, IIO_LLP_CSR));
	ii_csr.icsr_fields_s.icsr_llp_en = 1;
	SD(REMOTE_HUB(nasid, IIO_LLP_CSR), ii_csr.icsr_reg_value);

	delay(II_DELAY_TIME);
}

void
hubii_llp_disable(nasid_t nasid)
{
	hubii_ilcsr_t	ii_csr;	/* the control status register */
	
	ii_csr.icsr_reg_value = LD(REMOTE_HUB(nasid, IIO_LLP_CSR));
	ii_csr.icsr_fields_s.icsr_llp_en = 1;
	SD(REMOTE_HUB(nasid, IIO_LLP_CSR), ii_csr.icsr_reg_value);

	delay(II_DELAY_TIME);
}


int
hubii_llp_is_enabled(nasid_t nasid)
{
	hubii_ilcsr_t	ii_csr;	/* the control status register */
	
	ii_csr.icsr_reg_value = LD(REMOTE_HUB(nasid, IIO_LLP_CSR));
	return (ii_csr.icsr_fields_s.icsr_llp_en);
}

void
hubii_enable_widget_access(nasid_t nasid)
{
	hubii_iiwa_t ii_iiwa;
	ii_iiwa.iiwa_reg_value = 0;
	ii_iiwa.iiwa_fields_s.iiwa_w0iac = 1; /* allow access to hub from W0*/

	ii_iiwa.iiwa_fields_s.iiwa_wxiac = ~0;	/* allow access to hub from
						 * all widgets */

        SD(REMOTE_HUB(nasid, IIO_INWIDGET_ACCESS), ii_iiwa.iiwa_reg_value);
}


void 
hubii_early_warm_reset()
{
	hubii_ilcsr_t	ii_csr;	/* the control status register */
	
	ii_csr.icsr_reg_value = LD(LOCAL_HUB(IIO_LLP_CSR));
	ii_csr.icsr_fields_s.icsr_wrm_reset = 1;
	SD(LOCAL_HUB(IIO_LLP_CSR), ii_csr.icsr_reg_value);
}
	
void
hubii_warm_reset(nasid_t nasid)
{
	hubii_ilcsr_t	ii_csr;	/* the control status register */
	
	ii_csr.icsr_reg_value = LD(REMOTE_HUB(nasid, IIO_LLP_CSR));
	ii_csr.icsr_fields_s.icsr_wrm_reset = 1;
	SD(REMOTE_HUB(nasid, IIO_LLP_CSR), ii_csr.icsr_reg_value);
	delay(II_DELAY_TIME);
	/*
	 * reset again. Kianoosh suggests this may be needed.
	 */
	SD(REMOTE_HUB(nasid, IIO_LLP_CSR), ii_csr.icsr_reg_value);
	delay(II_DELAY_TIME);
}


int
hubii_link_good(nasid_t nasid)
{
    hubii_ilcsr_t	ii_csr;	/* the control status register */

#if defined (SABLE)
    return 1;
	/* NOTREACHED */
#endif
    ii_csr.icsr_reg_value = LD(REMOTE_HUB(nasid, IIO_LLP_CSR));
    if ((ii_csr.icsr_fields_s.icsr_lnk_stat & LNK_STAT_WORKING) == 0) {
        printf("***Warning: Check HUB IO link.\n");
	return 0;
    }
    return 1;
}



void
hubii_wcr_widset(nasid_t nasid, int wid)
{
	hubii_wcr_t	ii_wcr;	/* the control status register */
	
	ii_wcr.wcr_reg_value = LD(REMOTE_HUB(nasid, IIO_WCR));

	ii_wcr.wcr_fields_s.wcr_widget_id = wid;

/*
 * XXX - With xbow 1.1, we'll turn this on, unless we're talking directly to a
 * bridge.
 */
	ii_wcr.wcr_fields_s.wcr_tag_mode = 0;

	SD(REMOTE_HUB(nasid, IIO_WCR), ii_wcr.wcr_reg_value);
	delay(II_DELAY_TIME);
}


void
hubii_wcr_tagmode(nasid_t nasid)
{
	hubii_wcr_t	ii_wcr;	/* the control status register */
	
	ii_wcr.wcr_reg_value = LD(REMOTE_HUB(nasid, IIO_WCR));

	ii_wcr.wcr_fields_s.wcr_tag_mode = 1;

	SD(REMOTE_HUB(nasid, IIO_WCR), ii_wcr.wcr_reg_value);
	delay(II_DELAY_TIME);
}



void
hubii_wcr_credits(nasid_t nasid, int credits)
{
	hubii_wcr_t	ii_wcr;	/* the control status register */
	
	ii_wcr.wcr_reg_value = LD(REMOTE_HUB(nasid, IIO_WCR));

	ii_wcr.wcr_fields_s.wcr_xbar_crd = credits;

	SD(REMOTE_HUB(nasid, IIO_WCR), ii_wcr.wcr_reg_value);
	delay(II_DELAY_TIME);
}



void
hubii_wcr_direct(nasid_t nasid, int direct)
{
	hubii_wcr_t	ii_wcr;	/* the control status register */
	
	ii_wcr.wcr_reg_value = LD(REMOTE_HUB(nasid, IIO_WCR));

	ii_wcr.wcr_fields_s.wcr_dir_con = direct;

	SD(REMOTE_HUB(nasid, IIO_WCR), ii_wcr.wcr_reg_value);
	delay(II_DELAY_TIME);
}

void
hubii_setup_bigwin(nasid_t nasid, int wid, int bigwin)
{
	hubreg_t itte_reg;

	itte_reg = (wid << IIO_ITTE_WIDGET_SHIFT) | (1 << IIO_ITTE_IOSP_SHIFT);
	
	SD(REMOTE_HUB(nasid, IIO_ITTE(bigwin)), itte_reg);
}


int
hubii_llp_setup(nasid_t nasid, int wid_id)
{
	int	widget = 0;
	int	rc = 1;
	int	xb_rev;

	switch (XWIDGET_PART_NUM(wid_id)) {
	case XBOW_WIDGET_PART_NUM:
		rc = xbow_init_arbitrate(nasid);
		xb_rev = XWIDGET_REV_NUM(wid_id);
		/*
		 * XBow Rev Number	JTAG Rev Num
		 *	1.0		1
		 *	1.1		2	<- Supports tag mode
		 * 	1.2		3	<- Supports 4 xtalk credit
		 */
		hubii_wcr_credits(nasid, 
		    (xb_rev >= 3 ? HUBII_XBOW_REV2_CREDIT : HUBII_XBOW_CREDIT));
		if (xb_rev >= 2) 
		    hubii_wcr_tagmode(nasid);
		break;
		
	case BRIDGE_WIDGET_PART_NUM:
		hubii_warm_reset(nasid);
		hubii_wcr_direct(nasid, 1);
		hubii_wcr_credits(nasid, BRIDGE_CREDIT);
		hubii_llp_enable(nasid);
		break;

	case XG_WIDGET_PART_NUM:
	case HQ4_WIDGET_PART_NUM:
		hubii_warm_reset(nasid);
		hubii_wcr_direct(nasid, 1);
		hubii_wcr_credits(nasid, 4);
		hubii_llp_enable(nasid);
		break;

	default:
                DG_PRINTF(0, ("Found part 0x%x connect to hub\n",
                       XWIDGET_PART_NUM(wid_id)));
		break;
	}
	
	if (rc >= 0)
	    hubii_enable_widget_access(nasid);
	return rc;
}


/*
 * kl_zero_crbs(nasid)
 */
void
kl_zero_crbs(nasid_t nasid)
{
	int i;

	for (i = 0; i < IIO_NUM_CRBS; i++) {
		SD(REMOTE_HUB(nasid, IIO_ICRB_A(i)), 0);
		SD(REMOTE_HUB(nasid, IIO_ICRB_B(i)), 0);
		SD(REMOTE_HUB(nasid, IIO_ICRB_C(i)), 0);
		SD(REMOTE_HUB(nasid, IIO_ICRB_D(i)), 0);
	}

}

/*
 * Look through all CRBs, and make sure that they are all
 * invalid.
 * We do this by checking which CRBs are valid, and flushing those
 * CRBs.
 * This routine assumes there are no racing IO going on through this
 * nasid at this time.
 */
void
kl_hubcrb_cleanup(nasid_t nasid)
{
        hubreg_t        ii_icmr;
        int             free_crbs;
        int             total_crbs;
        int             valid_mask;
        int             i;

        ii_icmr = LD(REMOTE_HUB(nasid, IIO_ICMR));
        free_crbs = ( ii_icmr & IIO_ICMR_FC_CNT_MASK) >> IIO_ICMR_FC_CNT_SHFT;
        total_crbs = ( ii_icmr & IIO_ICMR_C_CNT_MASK) >> IIO_ICMR_C_CNT_SHFT;

        if (free_crbs == total_crbs)
                return;

        /* Flush Valid CRBs */
        valid_mask = (ii_icmr & IIO_ICMR_PC_VLD_MASK) >> IIO_ICMR_PC_VLD_SHFT;        for (i = 0; i < IIO_NUM_CRBS; i++) {
                if (valid_mask & (1 << i)) {
                        /*
                         * Crb 'i' is valid. Flush this CRB.
                         */
                        icrba_t crba;
                        crba = *(icrba_t *)REMOTE_HUB_ADDR(nasid,IIO_ICRB_A(i));
                        *(volatile int *)PHYS_TO_K0((crba.a_addr) << 3);
                        crba = *(icrba_t *)REMOTE_HUB_ADDR(nasid,IIO_ICRB_A(i));
                        rtc_sleep(100);
                }
        }

        ii_icmr = LD(REMOTE_HUB(nasid, IIO_ICMR));
        free_crbs = ( ii_icmr & IIO_ICMR_FC_CNT_MASK) >> IIO_ICMR_FC_CNT_SHFT;
        total_crbs = ( ii_icmr & IIO_ICMR_C_CNT_MASK) >> IIO_ICMR_C_CNT_SHFT;

        if (free_crbs != total_crbs) {
                printf("Unable to Clean up All CRBs: free %d total %d\n",
                        free_crbs, total_crbs);
        }

}


/*
 * kl_init_hubii(hub_base)
 */

void
clear_hubii_registers(nasid_t nasid)
{
	int 		i ;
	__uint64_t	tmp ;

	/*
	 * Do not Zero out the PRB entries, since it is supposed to come 
	 * up initialized with a proper crosstalk credit counter.
	 */ 

	/*
	 * Zero out the PIO Read Table entries.
	 */
	for (i = 0; i < IIO_NUM_PRTES; i++) {

		LD(REMOTE_HUB(nasid, IIO_PRTE(i))); /* clear status */
		SD(REMOTE_HUB(nasid, IIO_PRTE(i)), 0);
	}
	
	/* Read and throw away stuff, load the performance profiling reg. */
	LD(REMOTE_HUB(nasid, IIO_IPPR)); /* clear status */

	/* clear the performance control register */
	SD(REMOTE_HUB(nasid, IIO_IPCR), 0); /* clear status */

	SD(REMOTE_HUB(nasid, IIO_WIDGET_STAT), 0);

	/* GFX and TTEs */
	SD(REMOTE_HUB(nasid, IIO_IGFX_0), 0);
	SD(REMOTE_HUB(nasid, IIO_IGFX_1), 0);

	for (i =0 ;i < IIO_NUM_ITTES; i++)
	    SD(REMOTE_HUB(nasid, (IIO_ITTE(i))), 0);


	/* The timeout registers for crosstalk and sn0net are supposed
	 * to come up initialized correctly. Dont initialize those 
	 */

	/* BTE */

	LD(REMOTE_HUB(nasid, IIO_IBCT_0)); /* terminate xfr */
	LD(REMOTE_HUB(nasid, IIO_IBCT_1));
	LD(REMOTE_HUB(nasid, IIO_IBLS_0)); /* clear bte status */
	LD(REMOTE_HUB(nasid, IIO_IBLS_1));

	SD(REMOTE_HUB(nasid, IIO_IBLS_0), 0);
	SD(REMOTE_HUB(nasid, IIO_IBSA_0), 0);
	SD(REMOTE_HUB(nasid, IIO_IBDA_0), 0);
	SD(REMOTE_HUB(nasid, IIO_IBNA_0), 0);
	SD(REMOTE_HUB(nasid, IIO_IBIA_0), 0);
	SD(REMOTE_HUB(nasid, IIO_IBLS_1), 0);
	SD(REMOTE_HUB(nasid, IIO_IBSA_1), 0);
	SD(REMOTE_HUB(nasid, IIO_IBDA_1), 0);
	SD(REMOTE_HUB(nasid, IIO_IBNA_1), 0);
	SD(REMOTE_HUB(nasid, IIO_IBIA_1), 0);

	/*
	 * zero out the LLP LOG register.
	 * Zero out the CRB entries
	 */
	SD(REMOTE_HUB(nasid, IIO_LLP_LOG), 0);

	kl_zero_crbs(nasid);

	/* Program the PRBs in Fire and Forget mode */
	/*
	 * It's essential to set the IPRB for widget 0 i.e. Xbow to be
	 * in Fire and Forget mode. This will avoid getting errors
 	 * from xbow on write-with-response to link-reset registers. 
	 * xbow does not return a response to a write to link-reset
	 * register, and hence the workaround for this problem is 
	 * to program the IPRB0 in fire and forget mode. 
	 */	
	tmp = LD(REMOTE_HUB(nasid, IIO_IOPRB(0))) | (1ull << 42);
	SD(REMOTE_HUB(nasid, IIO_IOPRB(0)), tmp);
	/* 
	 * We need to pull out all the required PRBs out of Fire and Forget
	 * mode after probing 
	 */
	for (i = HUB_WIDGET_ID_MIN; i <= HUB_WIDGET_ID_MAX; i++) {

		tmp = LD(REMOTE_HUB(nasid, IIO_IOPRB(i))) ;
		tmp |= (0x1ull << 42) ;
		SD(REMOTE_HUB(nasid, IIO_IOPRB(i)), tmp);
	}

	return;
}



void
setup_hubii_protection(nasid_t nasid)
{
	__uint64_t	mask;

	hubii_iowa_t ii_iowa;
	hubii_iiwa_t ii_iiwa;

	/* The local protection register comes up initialized to allow
	 * access by any node. Write it to let access to all nodes.
	 * We also enable access to all widgets
	 * from this hub, and allow access to this hub by all its widgets
	 */

	mask = ~0;
	SD(REMOTE_HUB(nasid, IIO_PROTECT), mask);

	/*
 	 * We need to specially initialize PI_IO_PROTECT to allow
	 * complete access for all registers by the I/O devices.
	 * Initing this register to zero in hub.c (IP27prom) has
	 * been disabled.
	 */
        SD(REMOTE_HUB(nasid, PI_IO_PROTECT), 0xffffffffffffffffull);

	ii_iowa.iowa_reg_value = 0;
	ii_iowa.iowa_fields_s.iowa_w0oac = 1; /* allow access to all widgets*/
	ii_iowa.iowa_fields_s.iowa_wxoac = ~0;/* allow access to all widgets*/

	ii_iiwa.iiwa_reg_value = 0;
	ii_iiwa.iiwa_fields_s.iiwa_w0iac = 1; /* allow access to hub from W0*/

	ii_iiwa.iiwa_fields_s.iiwa_wxiac = ~0;	/* allow access to hub from
						 * all widgets */

        SD(REMOTE_HUB(nasid, IIO_OUTWIDGET_ACCESS), ii_iowa.iowa_reg_value);
        SD(REMOTE_HUB(nasid, IIO_INWIDGET_ACCESS), ii_iiwa.iiwa_reg_value);

	return;
}


int
hubii_init(__psunsigned_t hub_base)
{
	nasid_t nasid = NASID_GET(hub_base);
	int 	wid_id;
	int 	rc;

	clear_hubii_registers(nasid);
	setup_hubii_protection(nasid);
	
	/*
	 * Determine if this ii llp link is working. If it is not, just
	 * return error.
	 */
	if (!hubii_link_good(nasid))
	    return 0;


#if !defined (SABLE)
	/*
	 * Hub inits - fixes for interrupts.
	 */
	/* clear IECLR */

        SD(REMOTE_HUB(nasid, IIO_IO_ERR_CLR), 0xfffff);

#if 0
	/* This lets HubIO interrupt cpu on node 0 */
	/* TBD: fill in nodeid field when writing this register */
	SD((hub_base + IIO_IIDSR), 
		((1<<IIO_IIDSR_ENB_SHIFT) | 
		((INT_PEND1_BASELVL+IO_ERROR_INTR) << IIO_IIDSR_LVL_SHIFT)));
#endif

#endif /* SABLE */

#if defined(HUB_II_IFDR_WAR)
       {
	       hubii_ifdr_t ifdr;
	       ifdr.hi_ifdr_value = LD(REMOTE_HUB(nasid, IIO_IFDR));
	       ifdr.hi_ifdr_fields.ifdr_maxrp = 0x28;
	       SD(REMOTE_HUB(nasid, IIO_IFDR), ifdr.hi_ifdr_value);
       }
#endif /* HUB_II_IFDR_WAR */

	hubii_wcr_direct(nasid, 0);

	hubii_wcr_widset(nasid, hub_widget_id(nasid));

	hubii_llp_enable(nasid);
	if (!SN00) {
                DB_PRINTF(("IO reset delay (5 Seconds)\n"));
		DELAY(5000000);
	}
	
	if ((wid_id = io_get_widgetid(nasid, 0)) == -1)
	    return 0;

	rc = hubii_llp_setup(nasid, wid_id);

	return rc;
}


void
hubii_prb_cleanup(nasid_t nasid, int widget)
{
	iprte_a_t	iprte;		/* PIO read table entry format */
	__psunsigned_t addr;
	int i;

	for (i = 0; i < IIO_NUM_PRTES; i++) {
	    iprte.entry = REMOTE_HUB_L(nasid, IIO_PRTE(i));
	    if (iprte.iprte_fields.valid) {
		addr = (iprte.iprte_fields.addr << 3);
		if (WIDGETID_GET(addr) == widget) {
		    REMOTE_HUB_S(nasid, IIO_PRTE(i), 0);
		    REMOTE_HUB_S(nasid, IIO_IPDR, (i | IIO_IPDR_PND));
		    while ((REMOTE_HUB_L(nasid, IIO_IPDR) & IIO_IPDR_PND))
			;
		}
	    }		
	}
}
/* Check if the hub is talking to a xbow */
int
hubii_widget_is_xbow(nasid_t nasid)
{
	int wid;
	
	/* Read the widget id register of the widget connected
	 * to the hub's xtalk link 
	 */
	wid = io_get_widgetid(nasid,0);
	/* See if the widget is a xbow */
	if (XWIDGET_PART_NUM(wid) == XBOW_WIDGET_PART_NUM)
		return 1;
	return 0;
}
