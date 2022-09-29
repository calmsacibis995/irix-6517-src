/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * huberror.c - 
 * 	This file contains the code to report and handle Hub chip-detected
 * 	errors.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/iobus.h>
#include <sys/idbgentry.h>
#include <sys/atomic_ops.h>
#include <sys/nodepda.h>
#include <sys/pda.h>

#include <ksys/partition.h>

#include "sn0_private.h"
#include "../error_private.h"
#include <sys/SN/error.h>
#include <sys/SN/SN0/bte.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/slotnum.h>

#define HUB_ERROR_PERIOD	(120 * HZ)	/* 2 minutes */

sn0_error_log_t	*kl_error_log[MAX_COMPACT_NODES];
struct	hub_errcnt_s	*hub_err_count[MAX_COMPACT_NODES];
struct  error_progress_state error_consistency_check;
extern void recover_error_init(klerror_recover_t *table);
extern int debug_error_message;

enum hub_rw_err_enum {
	RW_NO_ERROR = 0,
	RW_RD_TMOUT_RPART,
	RW_RD_TMOUT_LPART,
	RW_RD_TMOUT,
	RW_RD_DIRERR_RPART,
	RW_RD_DIRERR,
	RW_RD_DIRERR_LPART,
	RW_PRD_ERR_RPART,
	RW_PRD_ERR_LPART,
	RW_PRD_ERR,
	RW_RD_BADRRB,
	RW_WR_TMOUT_RPART,
	RW_WR_TMOUT_LPART,
	RW_WR_TMOUT,
	RW_WR_ERR_RPART,
	RW_WR_ERR_LPART,
	RW_WR_ERR,
	RW_WR_ACCESS,
	RW_PWR_ERR,
	RW_WR_BADWRB
};



char *hub_rw_err_info[] = {
	"No Error",
	"Read timeout on remote partition page",
	"Read timeout on partition page",
	"Read timeout",
	"Read directory error on remote partition page",
	"Read directory error",
	"Read directory error on partition page",
	"Partial read error on remote partition page",
	"Partial read error on partition page",
	"Partial read error",
	"Invalid RRB in error stack",
	"Write timeout error on remote partition page",
	"Write timeout error on partition page",
	"Write timeout error",
	"Write Error on remote partition page",
	"Write error on partition page",
	"Write error",
	"Write access error",
	"Partial write error",
	"Invalid WRB in error stack",
};


#define NASID_SANITY_CHECK_FAIL(_x)	(((_x >= 0)&&(_x < MAX_NASIDS)) ? 0 : 1)

void
hub_error_clear(nasid_t nasid)
{
    hubreg_t idsr;

    REMOTE_HUB_L(nasid, NI_PORT_ERROR_CLEAR);

    REMOTE_HUB_L(nasid, MD_MEM_ERROR_CLR);
    REMOTE_HUB_L(nasid, MD_DIR_ERROR_CLR);
    REMOTE_HUB_L(nasid, MD_MISC_ERROR_CLR);
    REMOTE_HUB_L(nasid, MD_PROTOCOL_ERROR_CLR);
    
    REMOTE_HUB_S(nasid, PI_ERR_INT_PEND, 
		 PI_ERR_CLEAR_ALL_A | PI_ERR_CLEAR_ALL_B);

    REMOTE_HUB_S(nasid, IIO_IO_ERR_CLR, 0xffffff);

    idsr = REMOTE_HUB_L(nasid, IIO_IIDSR);
    REMOTE_HUB_S(nasid, IIO_IIDSR, (idsr & ~(IIO_IIDSR_SENT_MASK)));

	return;
}


/*
 * Function	: hub_error_init
 * Purpose	: initialize the error handling requirements for a given hub.
 * Parameters	: cnode, the compact nodeid.
 * Assumptions	: Called only once per hub, either by a local cpu. Or by a 
 *			remote cpu, when this hub is headless.(cpuless)
 * Returns	: None
 */

void
hub_error_init(cnodeid_t cnode)
{
    nasid_t nasid;
    int i;
    hubreg_t misc;

    nasid = COMPACT_TO_NASID_NODEID(cnode);
    hub_error_clear(nasid);

    if (cnode == 0) {
	/*
	 * Allocate log for storing the node specific error info
	 */
	for (i = 0; i < numnodes; i++) {
	    kl_error_log[i]  = kmem_zalloc_node(sizeof(sn0_error_log_t), 
						KM_NOSLEEP, i);
	    hub_err_count[i] = kmem_zalloc_node(sizeof(hub_errcnt_t),
						VM_DIRECT | KM_NOSLEEP, i);
	    ASSERT_ALWAYS(kl_error_log[i] && hub_err_count[i]);
	}
    }

    /*
     * Assumption: There will be only one cpu who will initialize
     * a hub. we need to setup the ii and pi error interrupts.
     */
    if (get_compact_nodeid() == cnode) { 
	ASSERT_ALWAYS(kl_error_log[cnode]);
	ASSERT_ALWAYS(hub_err_count[cnode]);
	MD_ERR_LOG_INIT(kl_error_log[cnode]);
	recover_error_init(RECOVER_ERROR_TABLE(cnode, 0));
	recover_error_init(RECOVER_ERROR_TABLE(cnode, 1));

	/*
	 * Setup error intr masks.
	 */

	if (LOCAL_HUB_L(PI_CPU_ENABLE_B) == 0) /* only A present */
	    LOCAL_HUB_S(PI_ERR_INT_MASK_A, (PI_FATAL_ERR_CPU_A | 
					    PI_MISC_ERR_CPU_A|PI_ERR_GENERIC));
	else if (LOCAL_HUB_L(PI_CPU_ENABLE_A) == 0) /* only B present */
	    LOCAL_HUB_S(PI_ERR_INT_MASK_B, (PI_FATAL_ERR_CPU_B |
					    PI_MISC_ERR_CPU_B|PI_ERR_GENERIC));

	else {				/* Both A and B present */
	    LOCAL_HUB_S(PI_ERR_INT_MASK_A, (PI_FATAL_ERR_CPU_B |
					    PI_MISC_ERR_CPU_A|PI_ERR_GENERIC));
	    LOCAL_HUB_S(PI_ERR_INT_MASK_B, (PI_FATAL_ERR_CPU_A |
					    PI_MISC_ERR_CPU_B));

	}
	/*
	 * Turn off UNCAC_UNCORR interrupt in the masks. Anyone interested
	 * in these errors will peek at the int pend register to see if its
	 * set.
	 */ 
	misc = LOCAL_HUB_L(PI_ERR_INT_MASK_A);
	LOCAL_HUB_S(PI_ERR_INT_MASK_A, (misc & ~PI_ERR_UNCAC_UNCORR_A));
	misc = LOCAL_HUB_L(PI_ERR_INT_MASK_B);
	LOCAL_HUB_S(PI_ERR_INT_MASK_B, (misc & ~PI_ERR_UNCAC_UNCORR_B));

	/*
	 * enable all error indicators to turn on, in case of errors.
	 *
	 * This is not good on single cpu node boards.
	 **** LOCAL_HUB_S(PI_SYSAD_ERRCHK_EN, PI_SYSAD_CHECK_ALL);
	 */

#if defined (HUB_ERR_STS_WAR)
	if (!WAR_ERR_STS_ENABLED) 
#endif /* ! defined HUB_ERR_STS_WAR	*/
        {
	    LOCAL_HUB_S(PI_ERR_STATUS1_A_RCLR, 0);
	    LOCAL_HUB_S(PI_ERR_STATUS1_B_RCLR, 0);
	}

	if (LOCAL_HUB_L(PI_CPU_PRESENT_A)) {
	    SN0_ERROR_LOG(cnode)->el_spool_cur_addr[0] =
		SN0_ERROR_LOG(cnode)->el_spool_last_addr[0] =
		    LOCAL_HUB_L(PI_ERR_STACK_ADDR_A);
	}
	    
	if (LOCAL_HUB_L(PI_CPU_PRESENT_B)) {
	    SN0_ERROR_LOG(cnode)->el_spool_cur_addr[1] =
		SN0_ERROR_LOG(cnode)->el_spool_last_addr[1] =
		    LOCAL_HUB_L(PI_ERR_STACK_ADDR_B);
	}

	PI_SPOOL_SIZE_BYTES = 
	    ERR_STACK_SIZE_BYTES(LOCAL_HUB_L(PI_ERR_STACK_SIZE));

	if (LOCAL_HUB_L(PI_CPU_PRESENT_B)) {
		__psunsigned_t addr_a = LOCAL_HUB_L(PI_ERR_STACK_ADDR_A);
		__psunsigned_t addr_b = LOCAL_HUB_L(PI_ERR_STACK_ADDR_B);
		if ((addr_a & ~0xff) == (addr_b & ~0xff)) {
		    LOCAL_HUB_S(PI_ERR_STACK_ADDR_B, 	
				addr_b + PI_SPOOL_SIZE_BYTES);

		    SN0_ERROR_LOG(cnode)->el_spool_cur_addr[1] =
			SN0_ERROR_LOG(cnode)->el_spool_last_addr[1] =
			    LOCAL_HUB_L(PI_ERR_STACK_ADDR_B);

	    }
	}
	

	/* programming our own hub. Enable error_int_pend intr.
	 * If both present, CPU A takes CPU b's error interrupts and any
	 * generic ones. CPU B takes CPU A error ints.
	 */
	
	if (cause_intr_connect (SRB_ERR_IDX,
				(intr_func_t)(hubpi_eint_handler),
				SR_ALL_MASK|SR_IE)) {
	    cmn_err(ERR_WARN, 
		    "hub_error_init: cause_intr_connect failed on %d", cnode);
	}
	    
    }
    else {
	/* programming remote hub. The only valid reason that this
	 * is called will be on headless hubs. No interrupts 
	 */
	REMOTE_HUB_S(nasid, PI_ERR_INT_MASK_A, 0); /* not necessary */
	REMOTE_HUB_S(nasid, PI_ERR_INT_MASK_B, 0); /* not necessary */
    }
    /*
     * Now setup the hub ii and ni error interrupt handler.
     */

    hubii_eint_init(cnode);
    hubni_eint_init(cnode);

    /*** XXX FIXME XXX resolve the following***/
    /* INT_PEND1 bits set up for one hub only:
     *	SHUTDOWN_INTR
     *	MD_COR_ERR_INTR
     *  COR_ERR_INTR_A and COR_ERR_INTR_B should be sent to the
     *  appropriate CPU only.
     */

    if (cnode == 0) {
	    error_consistency_check.eps_state = 0;
	    error_consistency_check.eps_cpuid = -1;
	    spinlock_init(&error_consistency_check.eps_lock, "error_dump_lock");
    }
    
    nodepda->huberror_ticks = HUB_ERROR_PERIOD;
    return;
}

/*
 * Function	: hubii_eint_init
 * Parameters	: cnode
 * Purpose	: to initialize the hub iio error interrupt.
 * Assumptions	: Called once per hub, by the cpu which will ultimately
 *			handle this interrupt.
 * Returns	: None.
 */


void
hubii_eint_init(cnodeid_t cnode)
{
    int			bit, rv;
    hubii_idsr_t    	hubio_eint;
    hubinfo_t		hinfo; 
    cpuid_t		intr_cpu;
    vertex_hdl_t 	hub_v;
    hubii_ilcsr_t	ilcsr;

    hub_v = cnodeid_to_vertex(cnode);
    ASSERT_ALWAYS(hub_v);
    hubinfo_get(hub_v, &hinfo);

    ASSERT(hinfo);
    ASSERT(hinfo->h_cnodeid == cnode);

    ilcsr.icsr_reg_value = REMOTE_HUB_L(hinfo->h_nasid, IIO_ILCSR);

    if ((ilcsr.icsr_fields_s.icsr_lnk_stat & 0x2) == 0) {
	/* 
	 * HUB II link is not up. 
	 * Just disable LLP, and don't connect any interrupts.
	 */
	ilcsr.icsr_fields_s.icsr_llp_en = 0;
	REMOTE_HUB_S(hinfo->h_nasid, IIO_ILCSR, ilcsr.icsr_reg_value);
	return;
    }
    /* Select a possible interrupt target where there is a free interrupt
     * bit and also reserve the interrupt bit for this IO error interrupt
     */
    intr_cpu = intr_heuristic(hub_v,0,INTRCONNECT_ANYBIT,II_ERRORINT,hub_v,
			      "HUB IO error interrupt",&bit);
    if (intr_cpu == CPU_NONE) {
	cmn_err(ERR_WARN,
		"hubii_eint_init: intr_reserve_level failed, cnode %d", cnode);
	return;
    }
	
    rv = intr_connect_level(intr_cpu, bit, 0,(intr_func_t)(hubii_eint_handler),
			    (void *)(long)hub_v, NULL);
    ASSERT_ALWAYS(rv >= 0);
    hubio_eint.iin_reg = 0;
    hubio_eint.iin_fmt.ienable = 1;
    hubio_eint.iin_fmt.level = bit;	/* Take the least significant bits*/
    hubio_eint.iin_fmt.node = COMPACT_TO_NASID_NODEID(cnode);

    REMOTE_HUB_S(hinfo->h_nasid, IIO_IIDSR, hubio_eint.iin_reg);

}

void
hubni_eint_init(cnodeid_t cnode)
{
    int intr_bit;
    cpuid_t targ;
    int slice;

    intr_bit = INT_PEND1_BASELVL + NI_ERROR_INTR;

    if ((targ = cnodetocpu(cnode)) == -1) 
	return;

    if (intr_connect_level(targ, intr_bit, INTPEND1_MAXMASK, 
			   (intr_func_t)hubni_eint_handler, NULL, NULL))
	cmn_err(ERR_WARN,"hub_error_init: Cannot connect NI error interrupt.");

    for (slice = 0; slice < CPUS_PER_NODE; slice++) {
	targ = cnode_slice_to_cpuid(cnode, slice);
	intr_bit = INT_PEND1_BASELVL + NI_BRDCAST_ERR_A + slice;
	if (valid_cpuid(targ) && cpu_enabled(targ))
	    if (intr_connect_level(targ, intr_bit, INTPEND1_MAXMASK, 
			       (intr_func_t)hubni_rmteint_handler, NULL, NULL))
		cmn_err(ERR_WARN,"hub_error_init: Cannot connect NI error interrupt.");
    }
}


/*
 * Function	: hubpi_eint_handler
 * Parameters	: ep   -> the exception frame pointer.
 *		  cause-> mask indication error int pending occurred.
 * Purpose	: to handle pi generated interrupts. (error_int_pend)
 * Assumptions	: 
 * Returns	: None.
 */

/*ARGSUSED*/
void
hubpi_eint_handler (eframe_t *ep)
{
    hubreg_t eint_pend;
    hubreg_t cpu_eintmask;
    
    /*
     * If the NI has a problem, everyone has a problem.  We shouldn't
     * even attempt to handle otehr errors when an NI error is present.
     */
    if (check_ni_errors()) {
	hubni_error_handler("PI interrupt", 1);
	/* NOTREACHED */
    }

    cpu_eintmask = LOCAL_HUB_L(HUBPI_ERR_INT_MASK);
    eint_pend = (LOCAL_HUB_L(PI_ERR_INT_PEND) & cpu_eintmask);
    ASSERT_ALWAYS(eint_pend);	/* Illegal to get an interrupt but
				 * no bits set. */
	
    if (eint_pend & (PI_FATAL_ERR_CPU_A | PI_FATAL_ERR_CPU_B)) {
	enter_panic_mode();
	/* Dont bother doing anything, just panic */
	cmn_err(ERR_WARN, "Unrecoverable error interrupt");
	kl_panic("Fatal error interrupt received");
    }
    
    if (eint_pend & PI_ERR_MD_UNCORR) {
#if defined (DEBUG)
	if (debug_error_message)
	    cmn_err(ERR_WARN, "!md uncorr error intr 0x%x\n", eint_pend);
#endif /* DEBUG */
	/*
	 * This interrupt bit will be cleared by the MD part.
	 */
	log_md_errors(get_nasid());
    }

    /* On spool compare interrupt, process the error spool */
    if (eint_pend & HUBPI_ERR_SPOOL_CMP) {
	process_error_spool();
	LOCAL_HUB_S(PI_ERR_INT_PEND, HUBPI_ERR_SPOOL_CMP);
    }

    /* if wrb errors are found, process the write errors */
    if (eint_pend & HUBPI_ERR_WRB_WERR) {
#if defined (DEBUG)
	if (debug_error_message)
	    cmn_err(ERR_WARN, "!WRB write error interrupt. error int pend %x",
		    eint_pend);
#endif /* DEBUG */
	process_error_spool();
	LOCAL_HUB_S(PI_ERR_INT_PEND, HUBPI_ERR_WRB_WERR);
    }
    if (eint_pend & HUBPI_ERR_WRB_TERR) {
#if defined (DEBUG)
	if (debug_error_message)
	    cmn_err(ERR_WARN, "!WRB timeout error interrupt.error int pend %x",
		    eint_pend);
#endif /* DEBUG */
	process_error_spool();
	LOCAL_HUB_S(PI_ERR_INT_PEND, HUBPI_ERR_WRB_TERR);
    }

    /*
     * uncached uce error is assumed to be masked off. Bus error handling
     * will peek at that and clear it. Print warning and clear if we got
     * interrupted because of UCE being unmasked.
     */
    if (eint_pend & HUBPI_ERR_UNCAC_UNCORR) {
	cmn_err(ERR_WARN, "Unexpected UCE error interrupt. int pend 0x%x\n",
		eint_pend);
	LOCAL_HUB_S(PI_ERR_INT_PEND, HUBPI_ERR_UNCAC_UNCORR);
    }

    if (eint_pend & HUBPI_ERR_SPUR_MSG) {
#if defined (T5_WB_SURPRISE_WAR)
	if (error_search_wb_surprise()) {
	    cmn_err(ERR_WARN, "Spurious message intr, likely T5 WB surprise");
	    LOCAL_HUB_S(PI_ERR_INT_PEND, HUBPI_ERR_SPUR_MSG);
	    return;
	}
#endif /* T5_WB_SURPRISE_WAR */
	kl_panic("Spurious message error interrupt");
    }

    return;
}

void
hubni_print_state(nasid_t nasid, int level,
                   void (*pf)(int, char *, ...),int print_where)
{
    nodepda_t *npda;
    char name[100];
    int	s;

    npda = NODEPDA(cnodeid());
    
    /* Make sure that the ni error state for this
     * node gets printed only to "print_where"
     */
    s = mutex_spinlock(&(npda->fprom_lock));
    if (npda->ni_error_print & print_where)	{
	/* ni error state for this node has been 
	 * printed already
	 */
	mutex_spinunlock(&(npda->fprom_lock),s);
	return;
    } else {
	/* Remember that ni error
	 * state is getting printed
	 */
	npda->ni_error_print |= print_where;	
	mutex_spinunlock(&(npda->fprom_lock),s);
    }

    get_slotname(npda->slotdesc, name);
    pf(level, "NI ERROR STATE:\n");
    pf(level, "/module/%d/slot/%s/node\n",
	npda->module_id, name);

    show_ni_port_error(REMOTE_HUB_L(nasid, NI_PORT_ERROR),
			level, pf);
#ifdef DEBUG
    pf(level, "    NI error interrupt, port error == 0x%x\n",
       REMOTE_HUB_L(nasid, NI_PORT_ERROR));
    pf(level, "    NI status rev ID 0x%x\n",
       REMOTE_HUB_L(nasid, NI_STATUS_REV_ID));
    pf(level, "    NI vector parms 0x%x NI vector 0x%x\n",
       REMOTE_HUB_L(nasid, NI_VECTOR_PARMS), 
       REMOTE_HUB_L(nasid, NI_VECTOR));
    pf(level, "    NI vector data 0x%x NI vector status 0x%x\n",
       REMOTE_HUB_L(nasid, NI_VECTOR_DATA),
       REMOTE_HUB_L(nasid, NI_VECTOR_STATUS));
    pf(level, "    NI return vector 0x%x NI vector read data 0x%x\n",
       REMOTE_HUB_L(nasid, NI_RETURN_VECTOR),
       REMOTE_HUB_L(nasid, NI_VECTOR_READ_DATA));
#endif
}

/*
 * Check to see if we got an NI interrupt.  If we did, tell everyone else.
 * If someone else got one, we should also go through the special sn0net
 * error path.
 */
int
check_ni_errors(void)
{
    hubreg_t port_error;
    hubreg_t status_rev_id;
    
    port_error = LOCAL_HUB_L(NI_PORT_ERROR);
    status_rev_id = LOCAL_HUB_L(NI_STATUS_REV_ID);

    if (port_error & NPE_FATAL_ERRORS) {
	protected_broadcast(INT_PEND1_BASELVL + NI_BRDCAST_ERR_A);
	protected_broadcast(INT_PEND1_BASELVL + NI_BRDCAST_ERR_B);
	return 1;
    } else if (!(status_rev_id & (NSRI_LINKUP_MASK | NSRI_DOWNREASON_MASK))) {
	/* If these two fields are zero, the link has been up and later
	 * come down */
	return 1;
    } else if ((LOCAL_HUB_L(PI_INT_PEND1) & (1L << NI_BRDCAST_ERR_A)) ||
	       (LOCAL_HUB_L(PI_INT_PEND1) & (1L << NI_BRDCAST_ERR_B))) {
	/* Someone else told us about a network error */
	return 1;
    } else {
	/* No fatal errors. */
	return 0;
    }
}


void
hubni_error_handler(char *caller, int originator)
{
    extern void panicspin(void);

    /* Just in case interrupts aren't blocked already */
    splerr();

    /* This doesn't work on Speedo so don't try it. */
    if (!private.p_sn00)
	    LOCAL_HUB_S(MD_LED0 + cputoslice(cpuid()) * 8, KLDIAG_NI_DIED);

    enter_panic_mode();

    hubni_print_state(get_nasid(), IP27LOG_INFO, ip27log_printf,RIP_PROMLOG);

    /* 
     * Do the NI link reset for the hub which got ni error interrupt. 
     * We maynot be able to access the router otherwise since the port
     * reset on credit=0 timeout has been turned off by default.
     */

    if (originator) {
	ip27log_printf(IP27LOG_INFO,"Resetting the NI link");
	LOCAL_HUB_S(NI_PORT_RESET,NPR_LINKRESET);
    }

    router_print_state((router_info_t *)0, IP27LOG_INFO, ip27log_printf,
		       RIP_PROMLOG);


    ip27log_printf(IP27LOG_FATAL, 
		   "Fatal Craylink error: NI error detected by %s\n",
		   caller);


    /* Turn off promlogging all the console output to remote nodes */
    enter_promlog_mode();

    hubni_print_state(get_nasid(), ERR_CONT, cmn_err,RIP_CONSOLE);

    router_print_state((router_info_t *)0, ERR_CONT, cmn_err,RIP_CONSOLE);

#ifdef DEBUG
    if ((LOCAL_HUB_L(NI_PORT_ERROR) && (NPE_INTERNALERROR|NPE_BADMESSAGE)) ==
			(NPE_INTERNALERROR|NPE_BADMESSAGE)){
   	/* If both above bits are set, sit in debugger, without
   	 * disturbing too many things. 
	 * This indicates a potential case of either NI or router 
	 * sending and receiving a bad message..
	 */
	debug("ring");
    }
#endif

    if (originator)
	cmn_err(CE_PANIC, "Fatal Craylink error.");
    else
	panicspin();

}

void
hubni_eint_handler(void)
{
	/* Might want to note that we actually saw the NI interrupt. */
	hubni_error_handler("NI interrupt", 1);
}

void
hubni_rmteint_handler(void)
{
	/* Might want to note that we actually saw the NI interrupt. */
	hubni_error_handler("Remote NI interrupt", 0);
}

void
klsave_md_registers(hubreg_t error, enum md_err_enum type, nasid_t nasid)
{
    lboard_t *brd;
    klhub_t *hub;
    hub_error_t *hub_err;

    brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_IP27);
    if (brd == NULL) return;

    hub = (klhub_t *)find_first_component(brd, KLSTRUCT_HUB);
    if (hub ==  NULL) return;
	
    hub_err = HUB_ERROR_STRUCT(HUB_COMP_ERROR(brd, (klinfo_t *)hub), 
			       KL_ERROR_LOG);
    if (hub_err == NULL) return;
	
    switch (type) {
    case MEM_ERROR:
	hub_err->hb_md.hm_mem_err 		= error;
	hub_err->hb_md.hm_mem_err_info.ei_time	= ERROR_TIMESTAMP;
	break;
		    
    case DIR_ERROR:
	hub_err->hb_md.hm_dir_err 		= error;
	hub_err->hb_md.hm_dir_err_info.ei_time	= ERROR_TIMESTAMP;
	break;
		    
    case PROTO_ERROR:
	hub_err->hb_md.hm_proto_err 		= error;
	hub_err->hb_md.hm_proto_err_info.ei_time= ERROR_TIMESTAMP;
	break;
		    
    case MISC_ERROR:
	hub_err->hb_md.hm_misc_err 		= error;
	hub_err->hb_md.hm_misc_err_info.ei_time = ERROR_TIMESTAMP;
	break;
    }
}


/*
 * Function	: save_md_error
 * Parameters	: md_elog -> the saved error info space.
 *		  error   -> the error register.
 *		  type    -> the error type
 * Purpose	: to save the md error.
 * Assumptions	: the md_error_log of the node is LOCKED. 
 * Returns	: -1 on error, 0 on success.
 * NOTE		: We store the RTC in the error log. To allow this to be 
 *		  called from any node, we save the RTC of the node 
 *		  whose information we are saving.
 */
int
save_md_error(nasid_t nasid,
	      hubreg_t error,			/* the error info to save*/
	      enum md_err_enum type		/* what error is it */
	      ) 
{
    cnodeid_t cnode = NASID_TO_COMPACT_NODEID(nasid);
    struct md_err_reg *next_ent;
    sn0_error_log_t *kl_err_log;

    kl_err_log = SN0_ERROR_LOG(cnode);

    if (!kl_err_log) {
	cmn_err(ERR_WARN, "Hub error log not allocated, node %d", cnode);
	return -1;
    }

    if ((next_ent = MD_ERR_LOG_NEW(kl_err_log)) == NULL) {
	cmn_err(ERR_WARN, "Hub error log: too many errors detected, node %d",
		cnode);
	return -1;
    }

    next_ent->er_reg = error;
    next_ent->er_type = type;

    return 0;
}


void
probe_md_errors(nasid_t nasid)
{
	log_md_errors(nasid);
	nodepda->huberror_ticks = HUB_ERROR_PERIOD;
}

/*
 * Function	: log_md_errors
 * Parameters	: flag -> from error interrupt or otherwise.
 *		  cnode-> compact nodeid of node on which to look for errors.
 * Purpose	: To save error information about an error in a safe place
 *		  so that we can query this information when required.
 * Assumptions	: It is assumed that cnode will usually be the local node.
 *		  This is not strictly required.
 * Returns	: none.
 */
/*ARGSUSED*/
void
log_md_errors(nasid_t nasid)
{
    hubreg_t mem_err;
    hubreg_t dir_err;
    hubreg_t proto_err;
    hubreg_t misc_err;
    int s;
    char *panic_str = NULL;
    /*
     * hold spl to avoid interrupts and preemption, so that we can correctly
     * save the error registers and avoid large windows where the register
     * is clear and state is not saved even during real errors.
     * Don't clear error registers unless there are UCEs since we don't
     * want to discard CEs.
     */ 
    s = splerr();
    mem_err = REMOTE_HUB_L(nasid, MD_MEM_ERROR_CLR);
    if (mem_err & MEM_ERROR_VALID_UCE) {
	if (save_md_error(nasid, mem_err, MEM_ERROR)) {
	    panic_str = "log_md_errors: fatal, too many MD errors";
	    goto log_md_die;
	}
    }
    else mem_err = 0;

    dir_err = REMOTE_HUB_L(nasid, MD_DIR_ERROR_CLR);
    if (dir_err & (DIR_ERROR_VALID_UCE | DIR_ERROR_VALID_AE)) {
	if (save_md_error(nasid, dir_err, DIR_ERROR)) {
	    panic_str = "log_md_errors: fatal, too many MD errors";
	    goto log_md_die;
	}
    }
    else dir_err = 0;

    proto_err = REMOTE_HUB_L(nasid, MD_PROTOCOL_ERROR_CLR);
    if (proto_err & PROTO_ERROR_VALID_MASK) {
	if (save_md_error(nasid, proto_err, PROTO_ERROR)) {
	    panic_str = "log_md_errors: fatal, too many MD errors";
	    goto log_md_die;
	}
	if (((md_proto_error_t *)(&proto_err))->perr_fmt.overrun) {
		panic_str = "Multiple protocol errors reported";
		goto log_md_die;
	}
    }
    else proto_err = 0;

    misc_err = REMOTE_HUB_L(nasid, MD_MISC_ERROR_CLR);
    if (misc_err & MISC_ERROR_VALID_MASK) {
	if (save_md_error(nasid, misc_err, MISC_ERROR)) {
	    panic_str = "log_md_errors: fatal, too many MD errors";
	    goto log_md_die;
	}
	if (misc_err & (~MMCE_BAD_DATA_MASK)) {
		panic_str = "MD misc error";
		goto log_md_die;
	}
    }
    else misc_err = 0;

log_md_die:
    splx(s);

    if (mem_err) klsave_md_registers(mem_err, MEM_ERROR, nasid);
    if (dir_err) klsave_md_registers(dir_err, DIR_ERROR, nasid);
    if (proto_err) klsave_md_registers(proto_err, PROTO_ERROR, nasid);
    if (misc_err) klsave_md_registers(misc_err, MISC_ERROR, nasid);

    if (panic_str)
	kl_panic(panic_str);

    return;
}

/*
 * Function	: check_md_errors
 * Parameters	: paddr -> physical address to check for in the log.
 *		  cnode -> compact nodeid of node to whom the address belongs.
 *		  time_gap -> check only error that occurred withing the last
 *		  time_gap us. 
 * Purpose	: To check if a certain physical address has an error 
 *		  associated with it in the error log.
 * Assumptions	: None.
 * Returns	: 1 if there was an error, 0 otherwise.
 *
 * NOTE: this still needs a bit of work. For now, this will do.
 */

/*ARGSUSED*/
enum md_err_enum
check_md_errors(paddr_t paddr, hubreg_t *error_reg, 
		int time_gap, __psunsigned_t *handle)
{
    struct md_err_reg *next_ent;
    sn0_error_log_t *kl_err_log;
    cnodeid_t cnode = NASID_TO_COMPACT_NODEID(NASID_GET(paddr));

    if (!SN0_ERROR_LOG(cnode)) return NO_ERROR;

    kl_err_log = SN0_ERROR_LOG(cnode);

    next_ent = (*handle) ? 
	MD_ERR_LOG_PREV(kl_err_log, (struct md_err_reg *)(*handle))
	    : MD_ERR_LOG_LAST(kl_err_log);

    while (next_ent) {	
	if (MDERR_TIME_CHECK(next_ent, time_gap) == 0)  {
	    *handle = NULL;
	    break;
	}
	
	*handle = (__psunsigned_t)next_ent;

	switch (next_ent->er_type) {

	case MEM_ERROR:
	{
	    md_mem_error_t mem_err;

	    mem_err.merr_reg = next_ent->er_reg;
	    if (MEM_ERROR_ADDR_MATCH(MEM_ERROR_ADDR(mem_err), 
				     TO_NODE_ADDRSPACE(paddr))) {
		*error_reg = mem_err.merr_reg;
		return MEM_ERROR;
	    }
	    break;
	}
	case DIR_ERROR:
	{
	    md_dir_error_t dir_err;
	    __uint64_t hspec_addr = BDDIR_ENTRY_LO(paddr);

	    dir_err.derr_reg = next_ent->er_reg;
	    if (DIR_ERROR_ADDR_MATCH(DIR_ERROR_ADDR(dir_err),
				     TO_NODE_ADDRSPACE(hspec_addr))) {
		*error_reg = dir_err.derr_reg;
		return DIR_ERROR;
	    }
	    break;
	}
	case PROTO_ERROR:
	{
	    md_proto_error_t proto_err;

	    proto_err.perr_reg = next_ent->er_reg;
	    if (PROTO_ERROR_ADDR_MATCH(PROTO_ERROR_ADDR(proto_err),
				       TO_NODE_ADDRSPACE(paddr))) {
		*error_reg = proto_err.perr_reg;
		return PROTO_ERROR;
	    }
	    break;
	}
	case MISC_ERROR:
	    break;

	default:
	    kl_panic("Unknown error type in check_md_errors");
	    break;
	}
	next_ent = MD_ERR_LOG_PREV(kl_err_log, next_ent);
    }
    return NO_ERROR;
}


struct md_err_reg *
md_err_log_first(sn0_error_log_t *elog)
{
    struct md_err_reg *next_ent;

    next_ent = &(elog->el_err_reg[elog->el_ndx % MD_MAX_ERR_LOG]);
    if (next_ent->er_type == 0) 
	next_ent = &(elog->el_err_reg[0]);
    if (next_ent->er_type == 0)
	next_ent = NULL;

    return next_ent;
}


struct md_err_reg *
md_err_log_next(sn0_error_log_t *elog, struct md_err_reg *ent)
{
    struct md_err_reg *next_ent;
    int ndx = (ent - &elog->el_err_reg[0]);

    ASSERT_ALWAYS((ndx >= 0) && (ndx <= MD_MAX_ERR_LOG));

    ndx = ((ndx + 1) % MD_MAX_ERR_LOG);
    if (ndx != (elog->el_ndx % MD_MAX_ERR_LOG))
	next_ent = &(elog->el_err_reg[ndx]);
    else
	next_ent = NULL;

    return next_ent;
}


struct md_err_reg *
md_err_log_last(sn0_error_log_t *elog)
{
    struct md_err_reg *next_ent;
    unsigned int nidx;

    nidx = ((elog->el_ndx - 1) % MD_MAX_ERR_LOG);

    next_ent = &(elog->el_err_reg[nidx]);
    if (next_ent->er_type == 0) 
	next_ent = NULL;

    return next_ent;
}


struct md_err_reg *
md_err_log_prev(sn0_error_log_t *elog, struct md_err_reg *ent)
{
    struct md_err_reg *next_ent;
    int ndx = (ent - &elog->el_err_reg[0]);

    ASSERT_ALWAYS((ndx >= 0) && (ndx <= MD_MAX_ERR_LOG));

    ndx = ((ndx - 1) % MD_MAX_ERR_LOG);
    if (ndx != ((elog->el_ndx - 1) % MD_MAX_ERR_LOG)) {
	next_ent = &(elog->el_err_reg[ndx]);
	if (next_ent->er_type == 0)
	    next_ent = NULL;
    }
    else
	next_ent = NULL;

    return next_ent;
}



struct md_err_reg *
md_err_log_new(sn0_error_log_t *elog)
{
    struct md_err_reg *next_ent;
    unsigned int *ndx_ptr;
    unsigned int next_ndx;
    clkreg_t cur_rtc;

    ndx_ptr = &(elog->el_ndx);
    next_ndx = atomicAddUint(ndx_ptr, 1);

    next_ndx = (next_ndx - 1) % MD_MAX_ERR_LOG;

    next_ent = &(elog->el_err_reg[next_ndx]);
    cur_rtc =  ERROR_TIMESTAMP;
    /*
     * If there was an error registered in this space earlier, and we
     * cannot yet discard it, return error.
     */
    if (next_ent->er_type) 
	if (!MDERR_DISCARD(next_ent->er_rtc, cur_rtc))
	    next_ent = NULL;

    if (next_ent)
 	next_ent->er_rtc = cur_rtc;

    return next_ent;
}


void
klsave_spool_registers(hubreg_t sts0, hubreg_t sts1, int flag)
{
    lboard_t *brd;
    klhub_t *hub;
    hub_error_t *hub_err;
    int cpu = LOCAL_HUB_L(PI_CPU_NUM);

    brd = find_lboard((lboard_t *)KL_CONFIG_INFO(get_nasid()), KLTYPE_IP27);
    if (brd == NULL) return;

    hub = (klhub_t *)find_first_component(brd, KLSTRUCT_HUB);
    if (hub ==  NULL) return;
	
    hub_err = HUB_ERROR_STRUCT(HUB_COMP_ERROR(brd, (klinfo_t *)hub), flag);
    if (hub_err == NULL) return;

    hub_err->hb_pi.hp_err_sts0[cpu] = sts0;
    hub_err->hb_pi.hp_err_sts1[cpu] = sts1;
    hub_err->hb_pi.hp_err_sts_info[cpu].ei_time = ERROR_TIMESTAMP;

    return;
}

	


/*
 * Function	: hubpi_spool_next
 * Parameters	: sts0 -> save area for status register 0 (or spooled word)
 *		  sts1 -> save area for status register 1.
 * Purpose	: To read an entry from the error spool and return it.
 * Assumptions	: Spool will never overflow.
 * Returns	: one of SPOOL_NONE, SPOOL_MEM, SPOOL_REG and SPOOL_LOST
 * NOTE		: HUB 1 actually clears the spool_cnt, resulting in race
 *		  during clearing error status register and looking up	
 * 		  stack for errors. HUB 2 is supposed to fix this by not
 *		  clearing the count. For now, never clear the status 
 *		  register. This has the added advantage that we have the
 *		  error spool history for a while. Disadvantage: the status
 *		  register gives us more detailed error handling information
 *		  which is now lost.
 */


enum pi_spool_type
hubpi_spool_get_next(hubreg_t *sts0, hubreg_t *sts1)
{
    nasid_t nasid = get_nasid();
    int s;

    /* 
     * This routine assumes md_error_log has been allocated.
     * If errors happen very early, this may not have been done,
     * and accessing these could cause problems.
     */
    s = splerr();

    if (!(SN0_ERROR_LOG(cnodeid())) || !(CUR_SPOOL_ADDR)) {
	    splx(s);
	    return SPOOL_NONE;
    }
    
    /*
     * If the current and max are the same, we have to read the status
     * registers and decide what to do next. 
     */

    if (CUR_SPOOL_ADDR == LAST_SPOOL_ADDR) {
	*sts0 = LOCAL_HUB_L(HUBPI_ERR_STATUS0);
	*sts1 = LOCAL_HUB_L(HUBPI_ERR_STATUS1);

#if defined (HUB_ERR_STS_WAR)
	if (!WAR_ERR_STS_ENABLED)
#endif /* ! defined HUB_ERR_STS_WAR	*/
	{
	    if (((pi_err_stat0_t *)sts0)->pi_stat0_fmt.s0_valid == 0) {
		splx(s);
		return SPOOL_NONE;
	    }

	    LOCAL_HUB_S(HUBPI_ERR_STATUS1_RCLR, 0);
	}
	/*
	 * small race condition here, but that can be ignored.
	 */
	LAST_SPOOL_ADDR = LOCAL_HUB_L(HUBPI_ERR_STACK_ADDR);

#if defined (HUB_ERR_STS_WAR)
	if (!WAR_ERR_STS_ENABLED) {
#else
	{
#endif
		splx(s);
		klsave_spool_registers(*sts0, *sts1, KL_ERROR_LOG);
		return SPOOL_REG;
	}
    }

    /*
     * register reads indicate no new errors, then return none.
     */
    if (CUR_SPOOL_ADDR == LAST_SPOOL_ADDR) {
	    splx(s);
	    return SPOOL_NONE;
    }

    *sts0 = *(hubreg_t *)(NODE_UNCAC_BASE(nasid) | CUR_SPOOL_ADDR);
    CUR_SPOOL_ADDR += 8;
	
    if ((CUR_SPOOL_ADDR & (PI_SPOOL_SIZE_BYTES - 1)) == 0)
	CUR_SPOOL_ADDR -= PI_SPOOL_SIZE_BYTES;

    splx(s);

    return SPOOL_MEM;
}

enum hub_rw_err_enum
huberror_process_rrb_error(int error_type, paddr_t paddr)
{
    enum hub_rw_err_enum rc = RW_NO_ERROR;
    enum error_result   prc;
    nasid_t nasid = NASID_GET(paddr);

    switch (error_type) {
    case PI_ERR_RD_TERR:	/* read timeout */
	/*
	 * This is probably speculative, since we are 
	 * handling this error out of context. Either that,
	 * or we got an error interrupt around the same time
	 * as the exception.
	 */
	if (REMOTE_PARTITION_NODE(nasid)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, "Read timeout on remote partition page");
#endif
	    prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_RD_TIMEOUT);
	    return ((prc == ERROR_FATAL) ? RW_RD_TMOUT_RPART : RW_NO_ERROR);
	    
	}
	if (PARTITION_PAGE(paddr)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, "Read timeout on partition page");
#endif
	    prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_RD_TIMEOUT);
	    return ((prc == ERROR_FATAL) ? RW_RD_TMOUT_LPART : RW_NO_ERROR);
	}
	
	cmn_err(ERR_WARN, "Read timeout, physaddr 0x%x", paddr);
	rc = RW_RD_TMOUT;
	break;
			
    case PI_ERR_RD_DERR:
	/*
	 * This is probably speculative, since we are 
	 * handling this error out of context. Either that,
	 * or we got an error interrupt around the same time
	 * as the exception.
	 */
	if (REMOTE_PARTITION_NODE(nasid)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, "Read dir error on remote partition page");
#endif
	    prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_RD_DIR_ERR);
	    return ((prc == ERROR_FATAL) ? RW_RD_DIRERR_RPART : RW_NO_ERROR);
	}

	if (page_isdiscarded(paddr))
	    rc = RW_NO_ERROR;
	else {
	    char membank_name[MAXDEVNAME];

	    membank_pathname_get(paddr,membank_name);
	    cmn_err(ERR_WARN, 
		    "Cached read directory error, physaddr 0x%x in %s\n",
		    paddr,membank_name);
	    if (error_page_discard(paddr, PERMANENT_ERROR, UNCORRECTABLE_ERROR)) 
		rc = RW_NO_ERROR;
	    else
		rc = RW_RD_DIRERR;
	}
	if (rc == 0) {
	    if (PARTITION_PAGE(paddr)) {
#if defined (DEBUG)
		cmn_err(ERR_WARN, "Read directory error on partition page");
#endif
		prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_RD_DIR_ERR);
		return ((prc == ERROR_FATAL) ? RW_RD_DIRERR_LPART : RW_NO_ERROR);
	    }
	}

	break;
			
    case PI_ERR_RD_PRERR:
	/*
	 * Uncached read error, must be access
	 * error. We should handle this in the proper 
	 * context of buserror exception, so panic.
	 */
	if (REMOTE_PARTITION_NODE(nasid)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, "Partital read error on remote partition page");
#endif
	    prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_PARTIAL);
	    return ((prc == ERROR_FATAL) ? RW_PRD_ERR_RPART : RW_NO_ERROR);
	}
	if (PARTITION_PAGE(paddr)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, "Partial read error on partition page");
#endif
	    prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_PARTIAL);
	    return ((prc == ERROR_FATAL) ? RW_PRD_ERR_LPART : RW_NO_ERROR);
	}

	cmn_err(ERR_WARN, "Uncached read error, physaddr 0x%x", paddr);
	rc = RW_PRD_ERR;
	break;
			
    default:
	cmn_err(ERR_WARN, "Invalid RRB error type: 0x%x", error_type);
	rc = RW_RD_BADRRB;
	break;
    }
    return rc;
}


enum hub_rw_err_enum
huberror_process_wrb_error(int error_type, paddr_t paddr)
{
    enum hub_rw_err_enum rc = RW_NO_ERROR;
    enum error_result   prc;
    nasid_t nasid = NASID_GET(paddr);
    /*
     * All Write error will cause a panic for now.
     */

    switch (error_type) {
    case PI_ERR_WR_TERR:		/* write timeout */
	if (REMOTE_PARTITION_NODE(nasid)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, 
		    "Writeback timeout error on remote partition page: pa=0x%x", paddr);
#endif
	    prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_WB);
	    return ((prc == ERROR_FATAL) ? RW_WR_TMOUT_RPART : RW_NO_ERROR);
	}
	if (PARTITION_PAGE(paddr)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, "Writeback timeout error on partition page");
#endif
	    prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_WB);
	    return ((prc == ERROR_FATAL) ? RW_WR_TMOUT_LPART : RW_NO_ERROR);
	}
	if (error_check_poisoned_state(paddr, 
				       NULL, WRITE_ERROR) != ERROR_FATAL) {
	    rc = RW_NO_ERROR;
	    break;
	}

	cmn_err(ERR_WARN, "Write timeout, paddr 0x%x", paddr);
	rc = RW_WR_TMOUT;
	break;

    case PI_ERR_WR_WERR:	/* cached write error */
	if (REMOTE_PARTITION_NODE(nasid)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, 
		    "Writeback error on remote partition page: pa=0x%x", paddr);
#endif
	    prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_WB);
	    return ((prc == ERROR_FATAL) ? RW_WR_ERR_RPART : RW_NO_ERROR);
	}
	if (PARTITION_PAGE(paddr)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, "Writeback error on partition page");
#endif
	    prc = PARTITION_HANDLE_ERROR((eframe_t *)NULL, paddr, PART_ERR_WB);
	    return ((prc == ERROR_FATAL) ? RW_WR_ERR_LPART : RW_NO_ERROR);
	}

#if defined (T5_WB_SURPRISE_WAR)
	if (error_try_wb_surprise_war(paddr)) {
	    rc = 0;
	    break;
	}
#endif /* T5_WB_SURPRISE_WAR */
	
	if ((prc = 
	     error_check_memory_access(paddr, WRITE_ERROR, 0)) != ERROR_NONE) {
	    rc = (prc == ERROR_FATAL) ? RW_WR_ACCESS : RW_NO_ERROR;
	    break;
	}

	cmn_err(ERR_WARN, "Cached write error, physaddr 0x%x", paddr);
	rc = RW_WR_ERR;
	break;
			
    case PI_ERR_WR_PWERR:
	/*
	 * Dont check for partition stuff: since on lego all pwerrs are
	 * assumed to be io errors.
	 */
	/*
	 * Uncached write error, must be access
	 * error. 
	 * There is no easy way to identify if an uncached
	 * write was targeted to memory or IO. We can't
	 * distinguish this information from the spooled
	 * data. For now, we will assume that the 
	 * uncached PIO writes are always targeted to
	 * IO. 
	 * While passing nodeid to kl_ioerror_handler,
	 * both srcnode and errnode point to cnodeid()
	 * since the error was generated as well as 
	 * received by this node.
	 */
	if (kl_ioerror_handler(cnodeid(), cnodeid(),
			       CPU_NONE, PIO_WRITE_ERROR, paddr, (caddr_t)NULL,
			       MODE_DEVERROR) != IOERROR_HANDLED){
	    cmn_err(ERR_WARN, "Uncached Write error, physaddr 0x%x node: %d",
		    paddr, cnodeid());
	    rc = RW_PWR_ERR;
	}
	break;

    default:
	cmn_err(ERR_WARN, "Invalid WRB error type: 0x%x", error_type);
	rc = RW_WR_BADWRB;
	break;
    }

    return rc;
}




/*
 * Function	: handle_errstk_entry
 * Parameters	: sts0 -> error status0 or stack word.
 *		  sts1 -> error status1 or nop.
 *		  type -> memory or register?
 * Purpose	: to handle stack errors that are not in the current 
 *		  context. (For example, processing a bus error exception 
 *		  and looking for a particular address shows a WRB err on 
 *		  stack.)
 * Assumptions	: None.
 * Returns	: -1 on fatal, 0 otherwise.
 */

enum hub_rw_err_enum
huberror_process_stack_entry(
			     hubreg_t sts0, 
			     hubreg_t sts1, 
			     enum pi_spool_type type, paddr_t *addr)
{
    int rd_wr, err_type;
    enum hub_rw_err_enum rc;
    paddr_t paddr;


    switch (type) {
    case SPOOL_MEM:
    {
	pi_err_stack_t pi_estk;

	pi_estk.pi_stk_word = sts0;
	paddr = SPOOL_MEM_ERROR_ADDR(pi_estk);
	rd_wr = pi_estk.pi_stk_fmt.sk_rw_rb;
	err_type = pi_estk.pi_stk_fmt.sk_err_type;
	break;
    }
    case SPOOL_REG:
    {
	pi_err_stat0_t pi_estat0;
	pi_err_stat1_t pi_estat1;

	pi_estat0.pi_stat0_word = sts0;
	pi_estat1.pi_stat1_word = sts1;
	paddr = SPOOL_REG_ERROR_ADDR(pi_estat0);
	rd_wr = pi_estat1.pi_stat1_fmt.s1_rw_rb;
	err_type = pi_estat0.pi_stat0_fmt.s0_err_type;
	break;
    }		
    default:
	cmn_err(ERR_WARN, "huberror_process_stack_entry, bad type 0x%x", type);
	break;
    }

    if (rd_wr == PI_ERR_RRB)
	rc = huberror_process_rrb_error(err_type, paddr);
    else
	rc = huberror_process_wrb_error(err_type, paddr);

#if defined (SABLE_ERROR_DEBUG)
    /*
     * For debugging sable error injection, always return non-fatal status
     */

    rc = 0;
#endif

    *addr = paddr;

    return rc;
}


void
process_error_spool()
{
    enum pi_spool_type type;
    hubreg_t sts0, sts1;
    paddr_t paddr;
    enum hub_rw_err_enum rc;

    while (1) {
	switch (type = hubpi_spool_get_next(&sts0, &sts1)) {
	case SPOOL_NONE:
	case SPOOL_LOST:
	    return;
			
	case SPOOL_MEM:
	case SPOOL_REG:
	    if (rc = huberror_process_stack_entry(sts0, sts1, type, &paddr)) {
		    if (in_dump_mode())
			continue;
		    kl_panic_setup();
		    cmn_err(CE_PBPANIC | CE_PHYSID,
			    "%s. PhysAddr 0x%x", hub_rw_err_info[rc], paddr);
	    }
	    break;
	}
    }
}

/*
 * Function	: get_matching_spool_addr
 * Parameters	: sts0 -> status register0 or spool memory
 *		  sts1 -> status register1 or nop
 * Purpose	: to look into the spool and get the entry that matches 
 *		  the physical address, (if any)
 * Assumptions	: None.
 * Returns	: one of SPOOL_MEM, SPOOL_REG, SPOOL_LOST or SPOOL_NONE.
 */
enum pi_spool_type
get_matching_spool_addr(
			hubreg_t *sts0,
			hubreg_t *sts1,
			paddr_t paddr,
			int range,
			int rw_rb)
{
    enum pi_spool_type type;
    enum hub_rw_err_enum rc;
    int crbt;
    paddr_t err_addr;

    while (1) {
	switch(type = hubpi_spool_get_next(sts0, sts1)) {
	case SPOOL_NONE:
	{
	    pi_err_stat0_t pi_estat0;
	    pi_err_stat1_t pi_estat1;
	    /*
	     * If nothing found in spool, its possible we got
	     * an error interrupt that removed things from the
	     * spool around the same time as an exception. See
	     * if anything has been stashed away. LATER.
	     *
	     *return check_saved_spool(sts0, sts1, paddr, rw_rb);
	     */
#if defined (HUB_ERR_STS_WAR)
	    if (WAR_ERR_STS_ENABLED) {
		pi_estat0.pi_stat0_word = LOCAL_HUB_L(HUBPI_ERR_STATUS0);
		pi_estat1.pi_stat1_word = LOCAL_HUB_L(HUBPI_ERR_STATUS1);

		if (pi_estat0.pi_stat0_fmt.s0_valid) {
		    err_addr = SPOOL_REG_ERROR_ADDR(pi_estat0);
		    if (SPOOL_REG_ADDR_MATCH(err_addr, paddr, range)) {
			*sts0 = pi_estat0.pi_stat0_word;
			*sts1 = pi_estat1.pi_stat1_word;
			return SPOOL_REG;
		    }
		}
	    }
#endif
	    return SPOOL_NONE;
	}
	case SPOOL_LOST:
	    return SPOOL_LOST;

	case SPOOL_MEM:
	{
	    pi_err_stack_t pi_estk;

	    pi_estk.pi_stk_word = *sts0;
	    err_addr = SPOOL_MEM_ERROR_ADDR(pi_estk);
	    if (SPOOL_MEM_ADDR_MATCH(err_addr, paddr, range)) {
		crbt = pi_estk.pi_stk_fmt.sk_rw_rb;
		if ((rw_rb == PI_ERR_ANY_CRB) || (rw_rb == crbt))
		    return SPOOL_MEM;
	    }
	    break;
	}     
	case SPOOL_REG:
	{
	    pi_err_stat0_t pi_estat0;
	    pi_err_stat1_t pi_estat1;

	    pi_estat0.pi_stat0_word = *sts0;
	    pi_estat1.pi_stat1_word = *sts1;
	    err_addr = SPOOL_REG_ERROR_ADDR(pi_estat0);
	    
	    if (SPOOL_REG_ADDR_MATCH(err_addr, paddr, range)) {
		crbt = pi_estat1.pi_stat1_fmt.s1_rw_rb;
		if ((rw_rb == PI_ERR_ANY_CRB) || (rw_rb == crbt))
		    return SPOOL_REG;
	    }
	    break;
	}			
	default:
	    cmn_err(ERR_WARN, "Unknown spool type %x", type);
	    return SPOOL_LOST;
	}
	if (rc = huberror_process_stack_entry(*sts0, *sts1, type, &err_addr)) {
		if (in_dump_mode())
		    continue;

		kl_panic_setup();
		cmn_err(CE_PBPANIC | CE_PHYSID,
			"%s. PhysAddr 0x%x",  hub_rw_err_info[rc], err_addr);
	}
    }
    
}
	

/*
 * Free the hub CRB "crbnum" which encounterd an error.
 * Assumption is, error handling was successfully done,
 * and we now want to return the CRB back to Hub for normal usage.
 *
 * In order to free the CRB, all that' needed is to de-allocate it
 * 
 * Assumption:
 *	No other processor is mucking around with the hub control register.
 *	So, upper layer has to single thread this.
 */
static void
hubiio_crb_free(hubinfo_t hinfo, int crbnum)
{
        icrba_t		icrba;

	/*
	 * The hardware does NOT clear the mark bit, so it must get cleared
	 * here to be sure the error is not processed twice.
	 */
	icrba.reg_value = REMOTE_HUB_L(hinfo->h_nasid, IIO_ICRB_A(crbnum));
	icrba.a_mark    = 0;
	REMOTE_HUB_S(hinfo->h_nasid, IIO_ICRB_A(crbnum), icrba.reg_value);
	/* 
	 * Deallocate the register.
	 */
	
	REMOTE_HUB_S(hinfo->h_nasid, IIO_ICDR, (IIO_ICDR_PND | crbnum));

	/*
	 * Wait till hub indicates it's done.
	 */
	while (REMOTE_HUB_L(hinfo->h_nasid, IIO_ICDR) & IIO_ICDR_PND)
		us_delay(1);

}

/*
 * Array of error names  that get logged in CRBs
 */ 
char *hubiio_crb_errors[] = {
	"No error",
	"CRB Poison Error",
	"I/O Write Error",
	"I/O Access Error",
	"I/O Partial write Error",
	"I/O Timeout Error",
	"Xtalk Error packet",
	"Directory Error on I/O access"
};

/*
 * hubiio_crb_error_handler
 *
 *	This routine gets invoked when a hub gets an error 
 *	interrupt. So, the routine is running in interrupt context
 *	at error interrupt level.
 * Action:
 *	It's responsible for identifying ALL the CRBs that are marked
 *	with error, and process them. 
 *	
 * 	If you find the CRB that's marked with error, map this to the
 *	reason it caused error, and invoke appropriate error handler.
 *
 *	XXX Be aware of the information in the context register.
 *
 * NOTE:
 *	Use REMOTE_HUB_* macro instead of LOCAL_HUB_* so that the interrupt
 *	handler can be run on any node. (not necessarily the node 
 *	corresponding to the hub that encountered error).
 */

int
hubiio_crb_error_handler(vertex_hdl_t hub_v, hubinfo_t hinfo)
{
	cnodeid_t	cnode;
	nasid_t		nasid;
	icrba_t		icrba;		/* II CRB Register A */
	icrbb_t		icrbb;		/* II CRB Register B */
	icrbc_t		icrbc;		/* II CRB Register C */
	int		i;
	int		rc;
	int		num_errors = 0;	/* Num of errors handled */
	ioerror_t	ioerror;
	extern 	void	sn0_error_dump(char *);	
	int		il;

	nasid = hinfo->h_nasid;
	cnode = NASID_TO_COMPACT_NODEID(nasid);

	il = mutex_spinlock_spl(&hinfo->h_crblock, splerr);

	/*
	 * Scan through all CRBs in the Hub, and handle the errors
	 * in any of the CRBs marked.
	 */
	for (i = 0; i < IIO_NUM_CRBS; i++) {
		icrba.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_A(i));

		if (icrba.a_mark == 0)
			continue;

		IOERROR_INIT(&ioerror);

		/* read other CRB error registers. */
		icrbb.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_B(i));
		icrbc.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_C(i));

		IOERROR_SETVALUE(&ioerror,errortype,icrba.a_ecode);
		/* Check if this error is due to BTE operation,
		 * and handle it separately.
		 */
		if (icrbc.c_bteop){
			bte_crb_error_handler(hub_v, icrbb.b_btenum, 
					      i, &ioerror);
			hubiio_crb_free(hinfo, i);
			num_errors++;
			continue;
		}

		/*
		 * XXX
		 * Assuming the only other error that would reach here is
		 * crosstalk errors. 
		 * If CRB times out on a message from Xtalk, it changes 
		 * the message type to CRB. 
		 *
		 * If we get here due to other errors (SN0net/CRB)
		 * what's the action ?
		 */

		/*
		 * Pick out the useful fields in CRB, and
		 * tuck them away into ioerror structure.
		 */
		IOERROR_SETVALUE(&ioerror,xtalkaddr,icrba.a_addr << IIO_ICRB_ADDR_SHFT);
		IOERROR_SETVALUE(&ioerror,widgetnum,icrba.a_sidn);


		if (icrba.a_iow){
			/*
			 * If widget was doing an IO write 
			 * operation, Tnum indicates the 
			 * device that was doing the write.
			 */
			IOERROR_SETVALUE(&ioerror,widgetdev,icrba.a_tnum);
		}

		if (icrba.a_error) {
		/* 
		 * CRB 'i' has some error. Identify the type of error,
		 * and try to handle it.
		 */
		switch(icrba.a_ecode) {
		case IIO_ICRB_ECODE_PERR:
		case IIO_ICRB_ECODE_WERR:
		case IIO_ICRB_ECODE_AERR:
		case IIO_ICRB_ECODE_PWERR:
			cmn_err(CE_NOTE, "%s on hub cnodeid: %d",
				hubiio_crb_errors[icrba.a_ecode], cnode);
			/*
			 * Any sort of write error is mostly due
			 * bad programming (Note it's not a timeout.)
			 * So, invoke hub_iio_error_handler with
			 * appropriate information.
			 */
			IOERROR_SETVALUE(&ioerror,errortype,icrba.a_ecode);

			rc = hub_ioerror_handler(
					hub_v, 
					DMA_WRITE_ERROR, 
					MODE_DEVERROR, 
					&ioerror);
			if (rc == IOERROR_HANDLED) {
				rc = hub_ioerror_handler(
					hub_v,
					DMA_WRITE_ERROR, 
					MODE_DEVREENABLE,
					&ioerror);
				ASSERT(rc == IOERROR_HANDLED);
			}else {
				sn0_error_dump("");
				cmn_err(CE_PANIC, 
					"Unable to handle %s on hub %d",
					hubiio_crb_errors[icrba.a_ecode],
					cnode);
				/*NOTREACHED*/
			}
			/* Go to Next error */
			hubiio_crb_free(hinfo, i);
			continue;

		case IIO_ICRB_ECODE_PRERR:
		case IIO_ICRB_ECODE_TOUT:
		case IIO_ICRB_ECODE_XTERR:
		case IIO_ICRB_ECODE_DERR:
			sn0_error_dump("");
			cmn_err(CE_PANIC, "Fatal %s on hub : %d",
				hubiio_crb_errors[icrba.a_ecode], cnode);
			/*NOTREACHED*/
		
		default:
			sn0_error_dump("");
			cmn_err(CE_PANIC,
				"Fatal error (code : %d) on hub : %d",
				cnode);
			/*NOTREACHED*/

		}
		} 	/* if (icrba.a_error) */	

		/*
		 * Error is not indicated via the errcode field 
		 * Check other error indications in this register.
		 */
		
		if (icrba.a_xerr) {
			cmn_err(CE_PANIC,
				"Xtalk Packet with error bit set to hub %d",
				cnode);
			/*NOTREACHED*/
		}

		if (icrba.a_lnetuce) {
			cmn_err(CE_PANIC,
				"Uncorrectable data error detected on data "
				" from Craylink to node %d",
				cnode);
			/*NOTREACHED*/
		}

	}
	mutex_spinunlock(&hinfo->h_crblock, il);

	return	num_errors;

}

/*ARGSUSED*/
/*
 * hubii_prb_handler
 *	Handle the error reported in the PRB for wiget number wnum.
 *	This typically happens on a PIO write error. 
 * 	There is nothing much we can do in this interrupt context for 
 *	PIO write errors. For e.g. QL scsi controller has the
 *	habit of flaking out on PIO writes. 
 *	Print a message and try to continue for now
 *	Cleanup involes freeing the PRB register
 */
static void
hubii_prb_handler(vertex_hdl_t hub_v, hubinfo_t hinfo, int wnum)
{
	nasid_t		nasid;

	nasid = hinfo->h_nasid;
	/* 
	 * PIO Write to Widget 'i' got into an error. 
	 * Invoke hubiio_error_handler with this information.
	 */
	cmn_err(CE_WARN, 
		"Hub nasid %d got a PIO Write error from widget %d, cleaning up and continuing",
			nasid, wnum);
	/*
	 * Clear error bit by writing to IECLR register.
	 */
	REMOTE_HUB_S(nasid, IIO_IO_ERR_CLR, (1 << wnum));
	/*
	 * XXX
	 * It may be necessary to adjust IO PRB counter
	 * to account for any lost credits.
	 */
}

int
hubiio_prb_error_handler(vertex_hdl_t hub_v, hubinfo_t hinfo)
{
	int		wnum;
	nasid_t		nasid;
	int		num_errors = 0;
	iprb_t		iprb;

	nasid = hinfo->h_nasid;
	/*
	 * Check if IPRB0 has any error first.
	 */
	iprb.reg_value = REMOTE_HUB_L(nasid, IIO_IOPRB(0));
	if (iprb.iprb_error) {
		num_errors++;
		hubii_prb_handler(hub_v, hinfo, 0);
	}
	/*
	 * Look through PRBs 8 - F to see if any of them has error bit set.
	 * If true, invoke hub iio error handler for this widget.
	 */
	for (wnum = HUB_WIDGET_ID_MIN; wnum <= HUB_WIDGET_ID_MAX; wnum++) {
		iprb.reg_value = REMOTE_HUB_L(nasid, IIO_IOPRB(wnum));

		if (!iprb.iprb_error)
			continue;
		
		num_errors++;
		hubii_prb_handler(hub_v, hinfo, wnum);
	}

	return num_errors;
}

/*
 * Function	: hubii_eint_handler
 * Parameters	: ep-> exception frame pointer.
 *	          arg->argument registered during interrupt init.
 * Purpose	: to handle ii generated interrupts.
 *		: II generates error interrupts for a number of error
 *		: events. typically, the reason for the error interrupt
 *		: would be available in one of the following registers:
 *		:	Marked via Crazy bit in widget status.
 *		:	LLP error (Tx Retry timeout bit in widget status)
 *		: 	Marked in PRB - Mostly for PIO write errors
 *		:	Marked in IO CRB - Xtalk/BTE triggerd error.
 * Assumptions	: 
 *
 * Action	: interrupt handler has the responsibility of categorizing
 *		: error interrupt type, and invoke kl_ioerror_handler.
 * Returns	: None.
 */

/*ARGSUSED*/
void
hubii_eint_handler (void *arg, eframe_t *ep)
{
    vertex_hdl_t	hub_v;
    hubinfo_t		hinfo; 
    hubii_wstat_t	wstat;
    hubreg_t		idsr;

    /*
     * If the NI has a problem, everyone has a problem.  We shouldn't
     * even attempt to handle other errors when an NI error is present.
     */
    if (check_ni_errors()) {
	hubni_error_handler("II interrupt", 1);
	/* NOTREACHED */
    }

    /* two levels of casting avoids compiler warning.!! */
    hub_v = (vertex_hdl_t)(long)(arg); 
    ASSERT(hub_v);

    hubinfo_get(hub_v, &hinfo);
    
    /* 
     * Identify the reason for error. 
     */
    wstat.reg_value = REMOTE_HUB_L(hinfo->h_nasid, IIO_WSTAT);

    if (wstat.wstat_fields_s.crazy) {
	char	*reason;
	/*
	 * We can do a couple of things here. 
	 * Look at the fields TX_MX_RTY/XT_TAIL_TO/XT_CRD_TO to check
	 * which of these caused the CRAZY bit to be set. 
	 * You may be able to check if the Link is up really.
	 */
	if (wstat.wstat_fields_s.tx_max_rtry)
		reason = "Micro Packet Retry Timeout";
	else if (wstat.wstat_fields_s.xt_tail_to)
		reason = "Crosstalk Tail Timeout";
	else if (wstat.wstat_fields_s.xt_crd_to)
		reason = "Crosstalk Credit Timeout";
	else {
		hubreg_t	hubii_imem;
		/*
		 * Check if widget 0 has been marked as shutdown, or
		 * if BTE 0/1 has been marked.
		 */
		hubii_imem = REMOTE_HUB_L(hinfo->h_nasid, IIO_IMEM);
		if (hubii_imem & IIO_IMEM_W0ESD)
			reason = "Hub Widget 0 has been Shutdown";
		else if (hubii_imem & IIO_IMEM_B0ESD)
			reason = "BTE 0 has been shutdown";
		else if (hubii_imem & IIO_IMEM_B1ESD)
			reason = "BTE 1 has been shutdown";
		else	reason = "Unknown";
	
	}
	/*
	 * Note: we may never be able to print this, if the II talking
	 * to Xbow which hosts the console is dead. 
	 */
	cmn_err(CE_PANIC, "Hub %d to Xtalk Link failed (II_ECRAZY) Reason: %s", 
		hinfo->h_cnodeid, reason);
    }

    /* 
     * It's a toss as to which one among PRB/CRB to check first. 
     * Current decision is based on the severity of the errors. 
     * IO CRB errors tend to be more severe than PRB errors.
     *
     * It is possible for BTE errors to have been handled already, so we
     * may not see any errors handled here. 
     */
    (void)hubiio_crb_error_handler(hub_v, hinfo);
    (void)hubiio_prb_error_handler(hub_v, hinfo);
    /*
     * If we reach here, it indicates crb/prb handlers successfully
     * handled the error. So, re-enable II to send more interrupt
     * and return.
     */
    REMOTE_HUB_S(hinfo->h_nasid, IIO_IECLR, 0xffffff);
    idsr = REMOTE_HUB_L(hinfo->h_nasid, IIO_IIDSR) & ~IIO_IIDSR_SENT_MASK;
    REMOTE_HUB_S(hinfo->h_nasid, IIO_IIDSR, idsr);
}
			  


#if defined (HUB_ERR_STS_WAR)

/*
 * the hub runs into serious problems if we ever have a WRB error when the
 * error status register is cleared. Force a RRB timeout so that the error
 * status register is set.
 */


extern int	_badaddr_val(volatile void *, int, volatile int *);
void
hub_error_status_war()
{
    volatile hubreg_t *vaddr;
    volatile int rv;

    /*
     * Only set the error register if it's clear.
     */
    if (LOCAL_HUB_L(PI_CPU_NUM)) {
	if (LOCAL_HUB_L(PI_ERR_STATUS0_B) & PI_ERR_ST0_VALID_MASK)
	    return;
    }
    else {
	if (LOCAL_HUB_L(PI_ERR_STATUS0_A) & PI_ERR_ST0_VALID_MASK)
	    return;
    }

#ifdef DEBUG
    cmn_err(CE_WARN|CE_CPUID, "ERR_STATUS register is clear.");
#endif

    /*
     * Do a read from a fictitious register to force a READ error into the
     * hub's PI_ERR_STATUS0_* registers.
     */
    vaddr = ERR_STS_WAR_ADDR;

    if (_badaddr_val((void *)vaddr, 8, &rv))
	return;

#ifdef DEBUG
    cmn_err(CE_WARN | CE_PHYSID, "Read timeout for HUB_ERR_STS_WAR failed");
#endif
}

#endif /* defined (HUB_ERR_STS_WAR)  */


    

void
ii_llp_error_monitor(hubii_errcnt_t *err)
{
	hubii_illr_t llp_log;

	
	llp_log.illr_reg_value = LOCAL_HUB_L(IIO_LLP_LOG);
	LOCAL_HUB_S(IIO_LLP_LOG, 0);

	if (llp_log.illr_fields_s.illr_cb_cnt == IIO_LLP_CB_MAX) 
	    err->iicb_pegged++;
	else
	    err->iicb_count += llp_log.illr_fields_s.illr_cb_cnt;

	if (llp_log.illr_fields_s.illr_sn_cnt == IIO_LLP_SN_MAX)
	    err->iisn_pegged++;
	else
	    err->iisn_count += llp_log.illr_fields_s.illr_sn_cnt;
}

		
void
ni_llp_error_monitor(hubni_errcnt_t *err)
{
	hubni_port_error_t port_err;
	
	port_err.nipe_reg_value = LOCAL_HUB_L(NI_PORT_ERROR);	
	if ((port_err.nipe_reg_value & (NPE_LINKRESET | NPE_INTERNALERROR | 
					NPE_BADMESSAGE | NPE_BADDEST |	
					NPE_FIFOOVERFLOW)) == 0)
	    port_err.nipe_reg_value = LOCAL_HUB_L(NI_PORT_ERROR_CLEAR);
	else {
		cmn_err(CE_WARN, "ni_llp_error_monitor, fatal ni error? 0x%x",
			port_err.nipe_reg_value);
		return;
	}
	
	    
	if (port_err.nipe_fields_s.nipe_retry_cnt == NI_LLP_RETRY_MAX) 
	    err->niretry_pegged++;
	else
	    err->niretry_count += port_err.nipe_fields_s.nipe_retry_cnt;

	if (port_err.nipe_fields_s.nipe_cb_cnt == NI_LLP_CB_MAX) 
	    err->nicb_pegged++;
	else
	    err->nicb_count += port_err.nipe_fields_s.nipe_cb_cnt;

	if (port_err.nipe_fields_s.nipe_sn_cnt == NI_LLP_SN_MAX) 
	    err->nisn_pegged++;
	else
	    err->nisn_count += port_err.nipe_fields_s.nipe_sn_cnt;
		
}



void
hub_error_count_update(nasid_t nasid)
{
	cnodeid_t cnode = NASID_TO_COMPACT_NODEID(nasid);

	ii_llp_error_monitor(&hub_err_count[cnode]->hec_iierr_cnt);
	ni_llp_error_monitor(&hub_err_count[cnode]->hec_nierr_cnt);

	if (hub_err_count[cnode]->hec_enable)
	    (void) timeout(hub_error_count_update,
			   (void *)(__psunsigned_t)nasid, HUB_ERRCNT_TIMEOUT);

}

int
hub_error_count_enable(nasid_t nasid)
{
	cnodeid_t cnode ;

	if (NASID_SANITY_CHECK_FAIL(nasid))
		return -1 ;

	cnode = NASID_TO_COMPACT_NODEID(nasid);

	if (cnode == INVALID_CNODEID) return -1;
	if (hub_err_count[cnode]->hec_enable)  return -1;

	bzero(hub_err_count[cnode], sizeof(hub_errcnt_t));
	hub_err_count[cnode]->hec_enable = 1;
	
	(void) timeout(hub_error_count_update,
		       (void *)(__psunsigned_t)nasid, HUB_ERRCNT_TIMEOUT);
	return 0;
}


int
hub_error_count_disable(nasid_t nasid)
{
	cnodeid_t cnode ;

	if (NASID_SANITY_CHECK_FAIL(nasid))
		return -1 ;

	cnode = NASID_TO_COMPACT_NODEID(nasid);

	if (cnode == INVALID_CNODEID) return -1;
	hub_err_count[cnode]->hec_enable = 0;
	return 0;
}


void *
hub_error_count_get(nasid_t nasid, int *size)
{
	cnodeid_t cnode ;

	if (NASID_SANITY_CHECK_FAIL(nasid))
		return NULL ;

	cnode = NASID_TO_COMPACT_NODEID(nasid);

	if (cnode == INVALID_CNODEID) return NULL;

	*size = sizeof(hub_errcnt_t);
	return (void *)hub_err_count[cnode];
}


char *
decode_xrb_err(pi_err_stack_t entry)
{
	if (entry.pi_stk_fmt.sk_rw_rb)
		/* WRB */
		return (hub_wrb_err_type[entry.pi_stk_fmt.sk_err_type]);
	else
		/* RRB */
		return (hub_rrb_err_type[entry.pi_stk_fmt.sk_err_type]);
}


static char crb_status_bits[] = "PVRAWHIT";

void
decode_crb_stat(char *deststring, int stat)
{
	int bit = sizeof(crb_status_bits) - 1;
	deststring[sizeof(crb_status_bits) + 1] = '0' + (stat & 0x3);
	deststring[sizeof(crb_status_bits)] = 'E';
	stat >>= 2;	/* Get rid of the E field. */
	while (bit >= 0) {
		if (stat & (1 << bit))
			deststring[bit] = crb_status_bits[bit];
		else
			deststring[bit] = '-';
		bit --;
	}
	deststring[sizeof(crb_status_bits) + 2] = '\000';
}

/* ARGSUSED */
void
print_spool_entry(pi_err_stack_t entry, int num, void (*pf)(char *, ...))
{
	char curr_crb_status[12];

	pf(" Entry %d:\n", num);
	decode_crb_stat(curr_crb_status, entry.pi_stk_fmt.sk_crb_sts);
	pf("  Cmd %02x, %s stat: %s CRB #%d, T5 req #%d, supp %d\n",
		entry.pi_stk_fmt.sk_cmd,
		entry.pi_stk_fmt.sk_rw_rb ? "WRB" : "RRB",
		curr_crb_status,
		entry.pi_stk_fmt.sk_crb_num,
		entry.pi_stk_fmt.sk_t5_req,
		entry.pi_stk_fmt.sk_suppl);
	pf("  Error %s, Cache line address 0x%lx\n",
		decode_xrb_err(entry),
		entry.pi_stk_fmt.sk_addr << 7);
		
#if 0
	entry.pi_stk_fmt.sk_addr >> (31 - 7);	/* address */
	entry.pi_stk_fmt.sk_cmd;		/* message command */
	entry.pi_stk_fmt.sk_crb_sts		/* status from RRB or WRB */
	entry.pi_stk_fmt.sk_rw_rb		/* RRB == 0, WRB == 1 */
	entry.pi_stk_fmt.sk_crb_num		/* WRB (0 to 7) or RRB (0 to 4) */
	entry.pi_stk_fmt.sk_t5_req		/* RRB T5 request number */
	entry.pi_stk_fmt.sk_suppl		/* lowest 3 bit of supplemental */
	entry.pi_stk_fmt.sk_err_type		/* error type */
#endif

	return;
}

void
dump_error_spool(cpuid_t cpu, void (*pf)(char *, ...))
{
	pi_err_stack_t *start, *end, *current;
	int size;
	int slice;
	nasid_t nasid;
	int entries = 0;

	nasid = cputonasid(cpu);
	slice = cputoslice(cpu);

	size = ERR_STACK_SIZE_BYTES(REMOTE_HUB_L(nasid, PI_ERR_STACK_SIZE));

	end = (pi_err_stack_t *)REMOTE_HUB_L(nasid,
			PI_ERR_STACK_ADDR_A + (slice * PI_STACKADDR_OFFSET));
	end = (pi_err_stack_t *)TO_NODE_UNCAC(nasid, (__psunsigned_t)end);

	start = (pi_err_stack_t *)((__psunsigned_t)end &
				  ~((__psint_t)(size - 1)));
	start = (pi_err_stack_t *)TO_NODE_UNCAC(nasid, (__psunsigned_t)start);

	pf("CPU %d Error spool:\n", cpu);
	for (current = start; current < end; current++) {
		print_spool_entry(*current, entries, pf);
		entries++;
	}
	pf("Spool dump complete.\n");

}


void
err_int_block_mask(hubreg_t mask)
{
	int slice = cputoslice(cpuid());
	int mask_reg = PI_ERR_INT_MASK_A + slice*sizeof(hubreg_t);

	ASSERT(!(mask & (PI_ERR_CLEAR_ALL_B | PI_ERR_MD_UNCORR)));
	mask >>= slice;  /* shift for CPU B masks */
	LOCAL_HUB_S(mask_reg, LOCAL_HUB_L(mask_reg) & ~mask);
}

void
err_int_unblock_mask(hubreg_t mask, int clear_pending)
{
	int slice = cputoslice(cpuid());
	int mask_reg = PI_ERR_INT_MASK_A + slice*sizeof(hubreg_t);

	ASSERT(!(mask & (PI_ERR_CLEAR_ALL_B | PI_ERR_MD_UNCORR)));
	mask >>= slice;  /* shift for CPU B masks */
	if (clear_pending) {
		LOCAL_HUB_S(PI_ERR_INT_PEND, mask);
	}
	LOCAL_HUB_S(mask_reg, LOCAL_HUB_L(mask_reg) | mask);
}

