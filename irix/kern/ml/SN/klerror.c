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
 * File: klerror.c
 *	This file contains SN0 specific handling of errors.
 */


#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/uadmin.h>
#include <sys/iobus.h>
#include <ksys/as.h>			/* vtop_attr 	*/
#include <sys/inst.h>
#include <sys/immu.h>
#include <sys/kabi.h>
#include <sys/lpage.h>
#include <sys/runq.h>
#include <sys/cred.h>			/* get_current_cred */
#include <ksys/vproc.h>

#include "sn_private.h"
#include "error_private.h"
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <sys/SN/router.h>
#include <sys/SN/SN0/sn0.h>
#include <sys/numa.h>
#include <ksys/partition.h>
#ifdef FRU
#include "SN0/FRU/sn0_fru_analysis.h"
#endif /* FRU */


/**************************************************************************
 * 		File private declarations and defines			  *
 **************************************************************************/

static paddr_t vtop_attr_get(k_machreg_t, uint *, uint *);
static error_result_t (* part_error_handler)(eframe_t *, paddr_t, enum part_error_type);

/**************************************************************************
 * 		Function definitions					  *
 **************************************************************************/

error_result_t handle_hspec_buserr(eframe_t *, paddr_t, uint);
error_result_t handle_mspec_buserr(eframe_t *, paddr_t, uint);
error_result_t handle_io_buserr(eframe_t *, paddr_t, uint);
error_result_t handle_uncac_buserr(eframe_t *, paddr_t, uint);
error_result_t handle_cac_buserr(eframe_t *, paddr_t, uint);
void	       dump_crb(cnodeid_t nasid, paddr_t paddr);

char *rrb_err_info[] = {
	"Unknown read error type",
	"Partial read error",
	"Directory error on read",
	"Timeout error on read",
};

char *wrb_err_info[] = {
	"Write error",
	"Partial write error",
	"Unknown write error type",
	"Timeout error on write",
};

extern volatile int hub_error_freeze;

int debug_error_message = 0;

char	*sneb_buf;
int	sneb_size;

#define TIME_DIFF(a, b)		(a - b)


klerror_recover_t kl_recover_table[] = 
{
       { KLE_MIGR_THRESH, 0, 0, 1, KLE_MIGR_TDIFF, 0, NULL },
       { KLE_NACK_THRESH, 0, 1, 1, KLE_NACK_TDIFF, 0, NULL },
       { KLE_MISC_THRESH, 0, 1, 0, KLE_MISC_TDIFF, 0, NULL },
       { KLE_UNK_THRESH,  0, 1, 0, KLE_UNK_TDIFF,  0, NULL }
};
	
void
recover_error_init(klerror_recover_t *table)
{
	int i;

	part_error_handler = (int (*)())NULL;

	for (i = 0; i < RECOVER_MAX_TYPES; i++)  
		*(table + i) = kl_recover_table[i];
}

int
klerror_global_check(void)
{
    static __uint64_t	gl_error_start = 0;
    static __uint32_t   gl_err_count = 0;

    __uint32_t	count;

    if (gl_error_start && 
	(TIME_DIFF(ERROR_TIMESTAMP, gl_error_start) < GL_TIME_THRESH)) {

	count = atomicAddUint(&gl_err_count, 1);
	
	if (count < GL_ERROR_THRESH) return 0;
	return -1;
    }

    gl_error_start = ERROR_TIMESTAMP;
    gl_err_count = 0;    

    return 0;
}

int
klerror_check_recoverable(int type, paddr_t paddr)
{
    klerror_recover_t *recoverable;
    void *key;

    if (!SN0_ERROR_LOG(cnodeid()))	return 0;
    key = (void *)curvprocp;

    ASSERT_ALWAYS(type < RECOVER_MAX_TYPES);
    recoverable = (RECOVER_ERROR_TABLE(cnodeid(), cputoslice(cpuid())) + type);

    if (recoverable->kr_global)
	if (klerror_global_check())
	    return -1;

    if ((recoverable->kr_start_time) && 
	(TIME_DIFF(ERROR_TIMESTAMP, recoverable->kr_start_time) <
	 recoverable->kr_time_diff)) {
	if (!recoverable->kr_paddr_depend ||
	    ((recoverable->kr_paddr == paddr)&&(recoverable->kr_key == key))){
	    if (recoverable->kr_err_count++ >= recoverable->kr_threshold) {
		if (!PARTITION_PAGE(paddr)) {
		    cmn_err(ERR_WARN, 
			    "Number of consecutive exceptions (physical "
			    "address 0x%x) exceeded limit", paddr);
		}
		return -1;
	    }
	    return 0;
	}
    }
    recoverable->kr_start_time = ERROR_TIMESTAMP;
    recoverable->kr_paddr = paddr;
    recoverable->kr_err_count = 0;
    recoverable->kr_key = key;

    return 0;
}

    
extern struct error_progress_state error_consistency_check;


void
sn0_skip_dump(void)
{
    void	dumptlb(int);

    dumptlb(cpuid());

    while(1)
	;
}




void
sn0_error_dump(char *fru_hint)
{	
    int s;

    /* We have already probed the router
     * links  in case of nmi. Do not reprobe in such a case.
     */

    if (strcmp(fru_hint,"nmi")) {
	    /* 	
	     * Probe for all the routers and collect the info about 
	     * which router links are up and which are down.
	     */
	    if (probe_routers() != PROBE_RESULT_GOOD)
		    /* We donot have shared memory */
		    return;
    }
    
    ERROR_CONSISTENCY_LOCK(s);
    if ((error_consistency_check.eps_cpuid != -1) &&
	(error_consistency_check.eps_cpuid != cpuid())) {
	ERROR_CONSISTENCY_UNLOCK(s);

	sn0_skip_dump();
	return;
    }
    error_consistency_check.eps_cpuid = cpuid();
    ERROR_CONSISTENCY_UNLOCK(s);


    /*
     * we are about to panic. Go to splhi to avoid anymore interruptions or 
     * kernel preemptions.
     * We enter panic mode here so that all info. is logged in ip27log
     */
    s = splhi();
    enter_panic_mode();

    kf_hint_set(get_nasid(),fru_hint);
    if ((error_consistency_check.eps_state & DUMP_ERRSAVE_START) == 0) {
	error_consistency_check.eps_state |= DUMP_ERRSAVE_START;
	kl_dump_hw_state();
	error_consistency_check.eps_state |= DUMP_ERRSAVE_DONE;
    }
    else if ((error_consistency_check.eps_state & DUMP_ERRSAVE_DONE) == 0) {
	cmn_err(ERR_WARN, "Exception while saving hardware state");
	/*
	 * return to old spl and let the panic path go back to splhi.
	 * we might need to handle device interrupts still...
	 */
        exit_panic_mode();
	splx(s);
	return;
    }


    if ((error_consistency_check.eps_state & DUMP_ERRSHOW_START) == 0) {
	error_consistency_check.eps_state |= DUMP_ERRSHOW_START;
	kl_error_show_dump("HARDWARE ERROR STATE:\n",
			   "End Hardware Error State\n");
	error_consistency_check.eps_state |= DUMP_ERRSHOW_DONE;
    }
    else  if ((error_consistency_check.eps_state & DUMP_ERRSHOW_DONE) == 0) 
	cmn_err(ERR_WARN, "Exception during show hardware state");
#if FRU
    if ((error_consistency_check.eps_state & DUMP_FRU_START) == 0) {
	error_consistency_check.eps_state |= DUMP_FRU_START;
	if (sn0_fru_enabled)
	    sn0_fru_entry((int (*)(const char *, ...))printf);
	error_consistency_check.eps_state |= DUMP_FRU_DONE;
    }
    else if ((error_consistency_check.eps_state & DUMP_FRU_DONE) == 0) 
	cmn_err(ERR_WARN, "Exception during FRU analysis");
#endif /* FRU */

    /*
     * return to old spl and let the panic path go back to splhi.
     * we might need to handle device interrupts still...
     */
    
    exit_panic_mode();
    splx(s);
    return;
}

void
sn0_error_state(void)
{	
	kl_dump_hw_state();
	kl_error_show_dump("HARDWARE ERROR STATE:\n",
			   "End Hardware Error State\n");
}


/*
 * Function	: dobuserre
 * Parameters	: ep   -> the exception frame.
 *		  epc  -> the exception program counter (aka ep->epc)
 *		  flag -> 0 = kernel, 1 = nofault, 2 = user.
 * Purpose	: to handle a data/inst bus error exception.
 * Assumptions	: We are on the same cpu that got the ERR response.
 *		  At this point interrupts have been enabled by the exception
 *		  prologue, and we are open for kernel pre-emption!
 * Returns	: 0 = ok to continue.
 *		  -1 fatal. (probably will never see this since we might 
 *			never return from here on fatal errors.)
 * NOTE		: This routine handles the following errors,
 *		   1) Cached RRB AERR, PERR, TERR, RERR and DERR.
 *		   2) Uncached RRB PRERR (partial read err for AERR, PERR,
 *			RERR, DERR),TERR and  uncorrectable memory error.
 *		      These uncached errors may be in any of the uncached
 *		      spaces. (Hspec, Mspec, IO or uncached.)
 */

/*ARGSUSED*/
int
dobuserre(register eframe_t *ep,  inst_t *epc, uint flag)
{
    int reason;
    error_result_t fatal;
    paddr_t	paddr;
    uint uncac_attr, cac_algo;
    char *str;
    cpu_cookie_t 	err_info;
#if defined(DEBUG)
    char buf[64];
#endif
    char membank_name[MAXDEVNAME];	

    ASSERT(ep->ef_cpuid <= MAXCPUS);
    if (curthreadp) {
	err_info = setmustrun(ep->ef_cpuid);
    }
    
    /*
     * If we are here due to IBE, ef_badvaddr has the right value,
     * which is epc or epc + 4. For DBE, we need to interpret the 
     * instruction. At this point we dont know whether its DBE or IBE,
     * so look it up and save.
     */ 
    reason = (ep->ef_cause & CAUSE_EXCMASK);
    ASSERT_ALWAYS((reason == EXC_IBE) || (reason == EXC_DBE));

    /*
     * If the NI has a problem, everyone has a problem.  We shouldn't
     * even attempt to handle other errors when an NI error is present.
     */
    if (check_ni_errors()) {
	hubni_error_handler("dobuserre", 1);
	/* NOTREACHED */
    }
	
    if (reason == EXC_DBE) {
	ep->ef_badvaddr = ldst_addr(ep);
	if (ep->ef_badvaddr == BERR_BAD_ADDR_MAGIC) 
	    kl_panic("dobuserre: Cant find faulting vaddr on DBE");
    }
#if defined (DEBUG)
    if (debug_error_message)
	cmn_err(ERR_WARN, "!%s Bus error at EPC 0x%x Virtual address: 0x%x",
		(reason == EXC_DBE ? "Data" : "Instruction" ), 
		ep->ef_epc, ep->ef_badvaddr);
#endif /* DEBUG */
    if ((paddr = vtop_attr_get(ep->ef_badvaddr, &uncac_attr, &cac_algo)) == -1) {
	/*
	 * we are here either because of the tlb entry being 
	 * replaced, or we migrated to another cpu!. No easy way
	 * to tell. Panic for now, even if nofault.
	 */
	sn0_error_dump("");

	cmn_err(ERR_PANIC,
		"%s (%s) Bus error. Unable to determine faulty address", 
		(flag == BERR_USER) ? "User" : "Kernel",
		(reason == EXC_DBE) ? "Data" : "Instr.");
	return -1;	
    }

    membank_pathname_get(paddr,membank_name);
    if ((cac_algo == CONFIG_UNCACHED) || (cac_algo == CONFIG_UNCACHED_ACC)) {
	switch (uncac_attr) {
	case UATTR_HSPEC:
	    fatal = handle_hspec_buserr(ep, paddr, flag);
	    str = "Hspec space";
	    break;

	case UATTR_MSPEC:
	    fatal = handle_mspec_buserr(ep, paddr, flag);
	    str = "Mspec space";
	    break;

	case UATTR_IO:
	    fatal = handle_io_buserr(ep, paddr, flag);
	    str = "IO space";
	    membank_name[0] = 0; /* Membank name doesnot make sense in this
				  * case 
				  */
	    break;

	case UATTR_UNCAC:
	    fatal = handle_uncac_buserr(ep, paddr, flag);
	    str = "Uncached space";
	    break;

	default:
	    enter_panic_mode();
	    cmn_err(ERR_WARN, "dobuserre: invalid uncached attribute 0x%x physical address 0x%x",
		    uncac_attr, paddr);
	    kl_panic("Invalid uncached attribute in Data Bus Error handling");
	    break;
	}
    }
    else if ((cac_algo == CONFIG_COHRNT_EXLWR) || 
	     (cac_algo == CONFIG_COHRNT_EXL)) {
	fatal = handle_cac_buserr(ep, paddr, flag);
	str = "Cached space";
    }
    else {
	if ((btoc(paddr) == 0) && (cac_algo == 0) && (uncac_attr == 0)) {
	    if (klerror_check_recoverable(RECOVER_MISC, 0) == -1) {
		kl_panic("Unrecoverable data bus error on unknown address");
	    }
	    else {
		cmn_err(ERR_WARN, "Data bus error on unknown address (vaddr 0x%x), retrying", ep->ef_badvaddr);
		if (curthreadp)
		    restoremustrun(err_info);
			
		return 1;
	    }
	}
	str = "Unknown space";
	membank_name[0] = 0; /* membank name doesnot make sense in this case */
	cmn_err(ERR_WARN, "Unsupported cache algorithm 0x%x", cac_algo);
	fatal = ERROR_FATAL;
    }
    
    if (flag != BERR_NOFAULT) {
	if (flag == BERR_KERNEL) 
	    if (fatal == ERROR_USER) fatal = ERROR_FATAL;

	if ((fatal == ERROR_USER) || (fatal == ERROR_FATAL)) {
	    if ((fatal == ERROR_USER) && ((cac_algo == CONFIG_COHRNT_EXL) ||
					  (cac_algo == CONFIG_COHRNT_EXLWR))) {
		if (page_isdiscarded(paddr))
		    cmn_err(ERR_NOTE | CE_TOOKACTIONS,
			    "uid %d pid %d (%s) killed, "
			    "access to page with error",
			    get_current_cred()->cr_uid, current_pid(),
			    get_current_name());
	    }

	    if (fatal == ERROR_FATAL) {
		sn0_error_dump("");
	    }
	    cmn_err((fatal == ERROR_FATAL) ? ERR_PANIC : ERR_NOTE,
		    "%s %s Bus error in %s at physical address 0x%x %s (EPC 0x%x)\n",
		    (flag == BERR_USER) ? "User" : "Kernel",
		    (reason == EXC_DBE) ? "Data" : "Instr.", str, paddr,
		    membank_name, epc);
	}
	/*
	 * The caller assumes a non-zero return to mean recovered.
	 * Otherwise we go into the nofault handler or signal user SIGBUS.
	 */
#if defined (DEBUG)
	if (flag & BERR_USER) 
	    sprintf(buf, "User %d (%s)", current_pid(), get_current_name());
	else
	    sprintf(buf, "Kernel");

	if (debug_error_message)
	    cmn_err(ERR_NOTE,
		    "!Processed %s (%s) Bus error in (%s) at paddr 0x%x",
		    buf,  (reason == EXC_DBE) ? "Data" : "Instr.", str, paddr);
#endif
    }
#if defined (DEBUG)
    else if (debug_error_message)
	cmn_err(ERR_NOTE,
		"Processed NOFAULT %s Bus error in %s at paddr 0x%x",
		(reason == EXC_DBE) ? "Data" : "Instr.", str, paddr);
#endif /* DEBUG */

    process_error_spool();	/* Not fatal. clean up spool */

    if (curthreadp)
	restoremustrun(err_info);

    ep->ef_buserr_info = MAXCPUS+1;
    ep->ef_cpuid = MAXCPUS+1;
    return (((fatal == ERROR_NONE) || (fatal == ERROR_HANDLED)) ? 1 : 0);
}


int
error_lookup_inst_type(eframe_t *ep)
{
    k_machreg_t vaddr;
    inst_t inst;
    int reason;

    reason = (ep->ef_cause & CAUSE_EXCMASK);
    if (reason == EXC_IBE) return 0;	/* Instruction bus error: read */

    vaddr = ep->ef_epc;
    if (ep->ef_cause & CAUSE_BD) 
	vaddr += sizeof(union mips_instruction);

    inst = (IS_KUSEG(vaddr)) ? 
	fuiword((caddr_t)vaddr) : fkiword((caddr_t)vaddr);

    if (inst == -1)	return -1;
    if (is_store(inst)) return 1;
    if (is_load(inst))	return 0;

    cmn_err(ERR_PANIC, "Inst op 0x%x, neither load nor store", inst);

    return -1;
}


/* ARGSUSED */
error_result_t
error_check_memory_access(paddr_t paddr, int rw, int flag)
{
    if (!memory_present(paddr)) {
	if (flag != BERR_NOFAULT)
	    cmn_err(ERR_WARN, "Access to non-existent memory address 0x%x", 
		    paddr);
	return (flag == BERR_NOFAULT) ? ERROR_USER : ERROR_FATAL;
    }

    if (rw == WRITE_ERROR) {
	if (!memory_write_accessible(paddr)) {
	    if (flag != BERR_NOFAULT)
		cmn_err(ERR_WARN, "No write privileges to memory address 0x%x",
			paddr);
	    return ERROR_USER;
	}
    }
    else {
	if (!memory_read_accessible(paddr)) {
	    if (flag != BERR_NOFAULT)
		cmn_err(ERR_WARN, "No read privileges to memory address 0x%x",
			paddr);
	    return ERROR_USER;
	}
    }
    
    return ERROR_NONE;
}


/*ARGSUSED*/
error_result_t
error_check_poisoned_state(paddr_t paddr, caddr_t vaddr, int rw)
{
    int state, rc;
    __uint64_t vec_ptr, elo, err_reg;
    __psunsigned_t handle = 0;

    ASSERT_ALWAYS(REMOTE_PARTITION_NODE(NASID_GET(paddr)) == 0);

    get_dir_ent(paddr, &state, &vec_ptr, &elo);

    if (state != MD_DIR_POISONED) 	/*** CHECK THIS ***/
	return ERROR_NONE;

#if defined (DEBUG)
    if (debug_error_message)
	cmn_err(ERR_WARN, "Exception addr is in poisoned state, 0x%x\n",
		paddr);
#endif /* DEBUG */

    /*
     * possibilities:
     * 1. VM migration.
     * 2. DIR error.
     * 3. PROTOCOL error.
     */
    if (migr_check_migr_poisoned(paddr)) {
	if (rw == WRITE_ERROR) {
	    cmn_err(ERR_WARN, "Write error exception on migrating page, 0x%x",
		    paddr);
	    return ERROR_FATAL;
	}
	migr_page_unmap_tlb(paddr, vaddr);
	if (klerror_check_recoverable(RECOVER_MIGRATE, paddr) == -1)  {
	    cmn_err(ERR_WARN, 
		    "Unrecoverable VM migration error, addr 0x%x", paddr);
	    return ERROR_FATAL;
	}
	return ERROR_HANDLED;
    }

    if (page_ispoison(paddr)) {
#if defined (DEBUG)
	    cmn_err(ERR_WARN, "Exception on poison address 0x%x", paddr);
	    debug("ring");
#else
	    return ERROR_USER;
#endif
    }

    log_md_errors(NASID_GET(paddr));    

    if (page_isdiscarded(paddr))
	return ERROR_USER;


    while (rc = check_md_errors(paddr, &err_reg, 
				ERROR_LONG_TIME, &handle)) {
	switch (rc) {
	case DIR_ERROR:
	case MEM_ERROR:	/* software poisoned this page on mem_error */
	    if (error_page_discard(paddr, PERMANENT_ERROR,UNCORRECTABLE_ERROR))
		return ERROR_USER;
	    else {
		    cmn_err(CE_WARN, "Page with memory/directory error could not be discarded. (paddr 0x%x)", paddr);
		    return ERROR_USER;
	    }

	case PROTO_ERROR:
#if defined (T5_WB_SURPRISE_WAR)
	    if (error_check_wb_surprise(paddr, &err_reg)) {
		cmn_err(ERR_WARN, "Error: Likely T5 writeback suprise");
		return ERROR_USER;
	    }
#endif	/* T5_WB_SURPRISE_WAR */
	    kl_panic("Protocol error detected");
	    break;

	default:
	    break;
	}
    }

    if (rw == READ_ERROR)
	return ERROR_USER;
    else {
	    cmn_err(CE_WARN, "Write error on poisoned page, addr 0x%x", paddr);
	    return ERROR_FATAL;
    }
}




/*
 * Function	: handle_hspec_buserr
 * Parameters	: ep -> exception frame.
 *		  paddr -> physical address of error.
 *		  flag  -> flag (user, kernel or nofault)
 * Purpose	: to handle a bus error on hspec accesses.
 * Assumptions	: None.
 * Returns	: FATAL_KERNEL or FATAL_USER or FATAL_NONE or FATAL_NOFAULT.
 * NOTE		: The onle reason of hspec error could be timeout.
 */
/*ARGSUSED*/
error_result_t
handle_hspec_buserr(eframe_t *ep, paddr_t paddr, uint flag)
{
    hubreg_t sts0, sts1;
    enum pi_spool_type sp_sts;	/* spool status */
    int err_type;

    /*
     * Not much to do? We can never get a buserr on an uncached,
     * unless it is a timeout error. Check for an address that is 
     * within a doubleword from paddr.
     * (ie, no DERR/PERR/Access error etc.)
     */
	
    sp_sts = get_matching_spool_addr(&sts0, &sts1, paddr & ~7,
				     8, PI_ERR_RRB);
    switch (sp_sts) {
    case SPOOL_NONE:
	cmn_err(ERR_WARN, "No spool info on HSPEC buserr");
	break;

    case SPOOL_LOST:
	cmn_err(ERR_WARN, "Lost Spool info on HPEC buserr");
	break;

    case SPOOL_MEM:
	err_type = ((pi_err_stack_t *)(&sts0))->pi_stk_fmt.sk_err_type;
	cmn_err(ERR_WARN, "%s error on HSPEC access", rrb_err_info[err_type]);
	break;

    case SPOOL_REG:
	err_type = ((pi_err_stat0_t *)(&sts0))->pi_stat0_fmt.s0_err_type;
	cmn_err(ERR_WARN, "%s error on HSPEC access", rrb_err_info[err_type]);
	break;
    }
    return ERROR_USER;
}

/*
 * Function	: handle_mspec_buserr
 * Parameters	: ep -> exception frame.
 *		  paddr -> physical address of error.
 *		  flag  -> flag (user, kernel or nofault)
 * Purpose	: to handle a bus error on mspec accesses.
 * Assumptions	: None.
 * Returns	: FATAL_KERNEL or FATAL_USER or FATAL_NONE or FATAL_NOFAULT.
 * NOTE		: Possible reasons: AERR, PERR, DERR, TERR
 */
/*ARGSUSED*/
error_result_t
handle_mspec_buserr(eframe_t *ep, paddr_t paddr, uint flag)
{
    hubreg_t sts0, sts1;
    enum pi_spool_type sp_sts;	/* spool status */
    int err_type;

    sp_sts = get_matching_spool_addr(&sts0, &sts1, paddr & ~7,
				     8, PI_ERR_RRB);
    switch (sp_sts) {
    case SPOOL_NONE:
	cmn_err(ERR_WARN, "No spool info on MSPEC buserr");
	break;
	
    case SPOOL_LOST:
	cmn_err(ERR_WARN, "Lost spool info on MSPEC buserr");
	break;

    case SPOOL_MEM:
	err_type = ((pi_err_stack_t *)(&sts0))->pi_stk_fmt.sk_err_type;
	cmn_err(ERR_WARN, "%s error on MSPEC access", rrb_err_info[err_type]);
	break;
    case SPOOL_REG:
	err_type = ((pi_err_stat0_t *)(&sts0))->pi_stat0_fmt.s0_err_type;
	cmn_err(ERR_WARN, "%s error on MSPEC access", rrb_err_info[err_type]);
	break;
    }
    return ERROR_USER;
}

/*
 * Function	: handle_io_buserr
 * Parameters	: ep -> exception frame.
 *		  paddr -> physical address of error.
 *		  flag  -> flag (user, kernel or nofault)
 * Purpose	: to handle a bus error on io accesses.
 * Assumptions	: None.
 * Returns	: FATAL_KERNEL or FATAL_USER or FATAL_NONE or FATAL_NOFAULT.
 * NOTE		: Possible reasons: AERR, TERR
 */
enum error_result
handle_io_buserr(eframe_t *ep, paddr_t paddr, uint flag)
{
	hubreg_t 	sts0, sts1;
	enum 		pi_spool_type sp_sts;	/* spool status */
	int		retval;
	int 		uce_eint_bit;
	ioerror_mode_t	error_mode;
	char           *lp;
	nasid_t		nasid;

	lp = (flag == BERR_NOFAULT) ? "!%s" : "%s";

	if (flag == BERR_NOFAULT) 
		error_mode = MODE_DEVPROBE;
	else if (flag == BERR_KERNEL) 
		error_mode = MODE_DEVERROR;
	else 	error_mode = MODE_DEVUSERERROR;

	sp_sts = get_matching_spool_addr(&sts0, &sts1, paddr & ~7,
					 8, PI_ERR_RRB);
	switch (sp_sts) {
	case SPOOL_NONE:
		/*
		 * Possibilities.
		 *	- PI will not spool anything if the error was due
		 *	  to an Uncached Read to an I/O space, and the 
		 *	  read request was accepted by the Hub II. 
		 *	  
		 *	  So, this is not an access error, and this is the
		 *	  case we need to recover from, for I/O acceses.
		 *	  We can't check the interrupt bit, since by the
		 *	  time we reach here, we would have gotten the 
		 *	  interrupt, and "ignored" it. So, assume this to 
		 *	  be an error due to I/O read, and try to handle it.
		 */
		retval = kl_ioerror_handler(
			NASID_TO_COMPACT_NODEID(NASID_GET(paddr)), 
			cnodeid(), cpuid(), 
			PIO_READ_ERROR, paddr, 
			(caddr_t)ep->ef_badvaddr, error_mode);

		uce_eint_bit = HUBPI_ERR_UNCAC_UNCORR;
		if (ERR_INT_SET(uce_eint_bit)){
			if (error_mode != MODE_DEVPROBE) {
			    /*
			     * Display error if not in probing mode. 
			     */
			    cmn_err(ERR_WARN, lp, "UCE interrupt on PIO access");
			}
			ERR_INT_RESET(uce_eint_bit);
		}

		break;

	case SPOOL_LOST:
		/* why not handle errors based on paddr ? */
		cmn_err(ERR_WARN, "Lost spool info on IO buserr");
		break;

	case SPOOL_MEM:

		retval = kl_ioerror_handler(
			NASID_TO_COMPACT_NODEID(NASID_GET(paddr)), 
			cnodeid(), cpuid(), 
			PIO_READ_ERROR, paddr, 
			(caddr_t)ep->ef_badvaddr, error_mode);
		break;
	case SPOOL_REG:
		/* paddr needs to be 8 byte aligned before comparing */
		ASSERT((((pi_err_stat0_t *)(&sts0))->pi_stat0_fmt.s0_addr << ERR_STAT0_ADDR_SHFT) == (paddr & ~(sizeof (long) - 1)));

		/*
		 * kl_ioerror_handler is going to validate the cnode we
		 * send it as the place the error was first noticed.
		 * In the case of IALIAS space, that cnode may not be
		 * present in this kernel so we should convert IALIAS addreses
		 * to the local nasid.
		 */
		if (IS_IALIAS(paddr))
			nasid = get_nasid();
		else
			nasid = NASID_GET(paddr);
			
		retval = kl_ioerror_handler(
			NASID_TO_COMPACT_NODEID(nasid), 
			cnodeid(), cpuid(), 
			PIO_READ_ERROR, paddr, 
			(caddr_t)ep->ef_badvaddr, error_mode);
		break;

	default:
		ASSERT(0);
	
	}

	/*
	 * A return value of IOERROR_HANDLED means 
	 * 1) Handled, ok to retry. OR
	 * 2) No fault processing. Do not retry.
	 */
	if ((retval == IOERROR_HANDLED) && (error_mode != MODE_DEVUSERERROR))
	    return (flag == BERR_NOFAULT) ? ERROR_USER : ERROR_HANDLED;

	return ERROR_USER;
}

/*
 * Function	: handle_uncac_buserr
 * Parameters	: ep -> exception frame.
 *		  paddr -> physical address of error.
 *		  flag  -> flag (user, kernel or nofault)
 * Purpose	: to handle a bus error on uncac accesses.
 * Assumptions	: None.
 * Returns	: FATAL_KERNEL or FATAL_USER or FATAL_NONE or FATAL_NOFAULT.
 * NOTE		: Possible reasons: UCE, AERR, TERR
 */

/*ARGSUSED*/
error_result_t
handle_uncac_buserr(eframe_t *ep, paddr_t paddr, uint flag)
{
    hubreg_t sts0, sts1;
    enum pi_spool_type sp_sts;	/* spool status */
    enum error_result rc;		/* return code */
    nasid_t nasid;
    int err_type, uce_eint_bit;
    int show_error = (flag != BERR_NOFAULT);

#if defined (DEBUG)
    show_error |= debug_error_message;
#endif /* !DEBUG */

    nasid = pfn_nasid(pnum(paddr));

    sp_sts = get_matching_spool_addr(&sts0, &sts1, paddr & ~7, 8, PI_ERR_RRB);

    switch (sp_sts) {
	
    case SPOOL_NONE:
	/*
	 *  Only possibility: uncached UCE.
	 *  Check interrupt bit.
	 */
	uce_eint_bit = HUBPI_ERR_UNCAC_UNCORR;
	if (ERR_INT_SET(uce_eint_bit)) {
	    cmn_err(ERR_WARN,"Uncorrectable error on uncached memory access, physical address 0x%x", paddr);
	    ERR_INT_RESET(uce_eint_bit);

	    if (REMOTE_PARTITION_NODE(nasid)) {
		if (show_error) {
		    cmn_err(ERR_WARN,"uncached remote partition access error");
		}
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_UNKNOWN);
	    }

	    if (error_page_discard(paddr, PERMANENT_ERROR, UNCORRECTABLE_ERROR) == 0) {
		cmn_err(CE_WARN, "Page with memory/directory error could not be discarded. (paddr 0x%x)", paddr);
		return ERROR_USER;
	    }

	    if (PARTITION_PAGE(paddr)) {
		cmn_err(ERR_WARN, "uncached partition page access error");
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_UNKNOWN);
	    }
	    return ERROR_USER;
	}
	else 
#if defined (HUB_ERR_STS_WAR)
	    /*
	     * See if we're looking at a forced error for pv #526920
	     * Could be running on old or new proms.
	     */
	    if ((paddr != ERR_STS_WAR_PHYSADDR) &&
		paddr != TO_PHYS(TO_NODE_UNCAC(nasid, OLD_ERR_STS_WAR_OFFSET)))
#endif
	{
		cmn_err(ERR_WARN, 
			"No spool info on uncached buserr at paddr 0x%x",
			paddr);
	}
	return ERROR_USER;

    case SPOOL_LOST:
	cmn_err(ERR_WARN, "Lost spool info on uncached buserr");
	return ERROR_FATAL;

    case SPOOL_MEM:
	err_type = ((pi_err_stack_t *)(&sts0))->pi_stk_fmt.sk_err_type;
	break;

    case SPOOL_REG:
	err_type = ((pi_err_stat0_t *)(&sts0))->pi_stat0_fmt.s0_err_type;
	break;
    }


    switch (err_type) {
    case PI_ERR_RD_TERR:
	if (show_error)
	    cmn_err(ERR_WARN, "Uncached read access timed out, physical address 0x%x",
		    paddr);

	if (REMOTE_PARTITION_NODE(nasid)) {
	    if (show_error) {
		cmn_err(ERR_WARN, "uncached remote partition timeout error");
	    }
	    return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_RD_TIMEOUT);
	}
	if (PARTITION_PAGE(paddr)) {
	    if (show_error) {
		cmn_err(ERR_WARN, "uncached partition page timeout error");
	    }
	    return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_RD_TIMEOUT);
	}
	return ERROR_USER;

    case PI_ERR_RD_PRERR:
	/*
	 * Possibilities: 
	 * (0) memory does not exist.
	 * (1) this page marked bad earlier.
	 * (2) Access not in partition. Mark page bad and continue.
	 * (3) Page access bits indicates no access. Kernel fault.
	 */
	if (REMOTE_PARTITION_NODE(nasid)) {
	    if (show_error) {
		cmn_err(ERR_WARN, "Uncached remote partition access error, physical address 0x%x", paddr);
	    }
	    return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_PARTIAL);
	}

	rc = error_check_memory_access(paddr, READ_ERROR, flag);
	if (rc != ERROR_NONE) {
	    if (PARTITION_PAGE(paddr))
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_ACCESS);
	    return rc;
	}

	rc = error_check_poisoned_state(paddr, (caddr_t)ep->ef_badvaddr, READ_ERROR);
	if (rc != ERROR_NONE) {
	    if (PARTITION_PAGE(paddr))
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_POISON);
	    return rc;
	}

	if (PARTITION_PAGE(paddr)) {
	    return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_UNKNOWN);
	}	    

	cmn_err(ERR_WARN, "Uncached memory access error, physical address 0x%x, cause unknown", paddr);
	return ERROR_FATAL;

    default:
	cmn_err(ERR_WARN,
		"Uncached access error, bad error type 0x%x", err_type);
	return ERROR_FATAL;
    }
}


/*
 * Function	: handle_cac_buserr
 * Parameters	: ep -> exception frame.
 *		  paddr -> physical address of error.
 *		  flag  -> flag (user, kernel or nofault)
 * Purpose	: to handle a bus error on cached accesses.
 * Assumptions	: None.
 * Returns	: FATAL_KERNEL or FATAL_USER or FATAL_NONE or FATAL_NOFAULT.
 * NOTE		: Possible reasons: RERR, PERR. AERR, TERR, DERR
 */
/*ARGSUSED*/
error_result_t
handle_cac_buserr(eframe_t *ep, paddr_t paddr, uint flag)
{
    hubreg_t sts0, sts1;
    enum pi_spool_type sp_sts;	/* spool status */
    error_result_t rc;
    nasid_t nasid;
    int err_type, rd_wr;
    int show_error = (flag != BERR_NOFAULT);
    extern int memory_iscalias(paddr_t);
    __uint64_t vecptr;
    hubreg_t elo;
    int state;

#if defined (DEBUG)
    show_error |= debug_error_message;
#endif /* !DEBUG */

    nasid = pfn_nasid(pnum(paddr));
    
    sp_sts = get_matching_spool_addr(&sts0, &sts1, (paddr & ~scache_linemask),
				     scache_linemask + 1, PI_ERR_RRB);
    switch (sp_sts) {
    case SPOOL_LOST:
	cmn_err(CE_WARN | CE_PHYSID,"Lost spool info on cached buserr");
	/*
	 * Fall through into the spool_none case.
	 */
    case SPOOL_NONE:
	/*
	 * check for region error first.
	 */
	if (!REGION_PRESENT(nasid_to_region(nasid))) {
	    if (show_error)
		cmn_err(ERR_WARN, "Region 0x%x, region not populated",
			nasid_to_region(nasid));
	    return ERROR_FATAL;
	}
	/*
	 * If this is access to another partition, not much can
	 * be done. Die. Should this happen earlier? Depends on VM.
	 */
	if (REMOTE_PARTITION_NODE(nasid)) {
	    cmn_err(ERR_WARN, "Cached remote partition access error");
	    return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_UNKNOWN);
	}
	
	rd_wr = error_lookup_inst_type(ep);
	if (rd_wr == -1) {
	    cmn_err(ERR_WARN, "Could not get instruction type: assuming store instruction");
	    rd_wr = WRITE_ERROR;
	}

	rc = (error_check_memory_access(paddr, rd_wr, flag));
	if (rc != ERROR_NONE) {
	    if (PARTITION_PAGE(paddr))
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_ACCESS);
	    return rc;
	}

	/*
	 * Force the error type to be READ_ERROR irrespective of 
	 * what the instruction says, since this is a bus error case
	 */
	rc = (error_check_poisoned_state(paddr, (caddr_t)ep->ef_badvaddr, READ_ERROR));
	if (rc != ERROR_NONE) {
	    if (PARTITION_PAGE(paddr))
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_POISON);
	    return rc;
	}

	/*
	 * If the access is to some node's calias space, we get an exception.
	 * Check for that condition here.
	 */

	if (memory_iscalias(paddr) && NASID_GET(paddr)) 
	    return ERROR_USER;

	/*
	 * could be spurious? Or the access was enabled on some
	 * hub after we got bus error. We got one of the nack
	 * timeouts which does not leave any state.
	 */
	if (klerror_check_recoverable(RECOVER_UNKNOWN, paddr) == -1) {
	    __uint64_t *dir_addr_hi, *dir_addr_lo;

            if ((verify_snchip_rev() < HUB_REV_2_3) &&
                ((ep->ef_cause & CAUSE_EXCMASK) == EXC_IBE)) {
                cmn_err(ERR_WARN,
                        "trying to recover from ibus error, addr=%x",paddr);
                return ERROR_NONE;
            }

	    if (PARTITION_PAGE(paddr)) {
		if (debug_error_message) {
		    cmn_err(CE_WARN, "NACK error on local partition addr %x",
			    paddr);
		}
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_NACK);
	    }

	    cmn_err(ERR_WARN, "Unrecoverable bus error exception, paddr 0x%x",
		    paddr);
	    dir_addr_lo = (__uint64_t *)BDDIR_ENTRY_LO(paddr);
	    dir_addr_hi = (__uint64_t *)BDDIR_ENTRY_HI(paddr);

	    cmn_err(ERR_WARN, "Mem info: %s dir entry hi 0x%x entry lo 0x%x",
		    (REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM) ?
		    "premium" : "standard",
		    *dir_addr_hi, *dir_addr_lo);


#define	OWNER_CPU(x)	(! (x & 2))
#define	OWNER_IO(x)	(x & 2)
#define	OWNER_NASID(x)	(x >> 2)

	    /* 
	     * check if we hit the IVACK problem
	     */
	   get_dir_ent(paddr,&state,&vecptr,&elo);

	   if ((state == MD_DIR_BUSY_SHARED || state == MD_DIR_BUSY_EXCL) &&
	        OWNER_IO(vecptr)) {
		cmn_err(ERR_WARN,"elo=0x%x",elo);
		dump_crb(OWNER_NASID(vecptr),paddr);
	   } 

	   if (OWNER_IO(vecptr))
		cmn_err(ERR_WARN,"dir entry IO owned");

	    return ERROR_FATAL;
	}
#ifdef DEBUG
	if (debug_error_message) {
	    cmn_err(ERR_WARN,
		    "Retrying cached read access: transient error paddr 0x%x",
		    paddr);
	    cmn_err(ERR_WARN, "pid %d epc 0x%x badva 0x%x",
		    current_pid(), ep->ef_epc, ep->ef_badvaddr);
	}
#endif
	return ERROR_NONE;

    case SPOOL_MEM:
	rd_wr    = ((pi_err_stack_t *)(&sts0))->pi_stk_fmt.sk_rw_rb;
	err_type = ((pi_err_stack_t *)(&sts0))->pi_stk_fmt.sk_err_type;
	break;
	
    case SPOOL_REG:
	rd_wr    = ((pi_err_stat1_t *)(&sts1))->pi_stat1_fmt.s1_rw_rb;
	err_type = ((pi_err_stat0_t *)(&sts0))->pi_stat0_fmt.s0_err_type;
	break;
    }
	
    if (rd_wr == PI_ERR_RRB) {
	switch (err_type) {

	case PI_ERR_RD_TERR:

	    if (REMOTE_PARTITION_NODE(nasid)) {
		if (debug_error_message) {
		    cmn_err(ERR_WARN, 
			    "Cached remote partition time out error");
		}
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_RD_TIMEOUT);
	    }
	    if (PARTITION_PAGE(paddr)) {
		if (debug_error_message) {
		    cmn_err(ERR_WARN, "Cached partition page time out error");
		}
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_RD_TIMEOUT);
	    }
	    cmn_err(ERR_WARN, "Cached read access: time out error");
	    return ERROR_FATAL;

	case PI_ERR_RD_DERR:

	    log_md_errors(NASID_GET(paddr));
	    if (show_error)
		cmn_err(ERR_WARN, "Cached read access: directory error");

	    if (REMOTE_PARTITION_NODE(nasid)) {
		if (debug_error_message) {
		    cmn_err(ERR_WARN, 
			    "Cached remote partition directory error");
		}
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_RD_DIR_ERR);
	    }

	    if (error_page_discard(paddr, PERMANENT_ERROR, UNCORRECTABLE_ERROR) == 0)  {
		cmn_err(CE_WARN, "Page with memory/directory error could not be discarded. (paddr 0x%x)", paddr);
		return ERROR_USER;
	    }

	    if (PARTITION_PAGE(paddr)) {
		if (debug_error_message) {
		    cmn_err(ERR_WARN, "Cached partition page directory error");
		}
		return PARTITION_HANDLE_ERROR(ep, paddr, PART_ERR_RD_TIMEOUT);
	    }
	    return ERROR_USER;

	default:
	    cmn_err(ERR_WARN,"Cached read access: bad error type 0x%x", 
		    err_type);
	    return ERROR_FATAL;
	}
    }
    else {
	kl_panic("WRB error during buserror unexpected");
    }	
    return ERROR_FATAL;
}


void
dump_crb(cnodeid_t nasid, paddr_t paddr)
{
	int i;
	paddr_t crba_addr;

	if ((nasid < 0) || (nasid > MAX_NASIDS) ||
            (NASID_TO_COMPACT_NODEID(nasid) == -1)) 
		return;

	paddr &= ~scache_linemask;

	for (i = 0 ; i < IIO_NUM_CRBS ; i++) {

		icrba_t crba = *(icrba_t*)REMOTE_HUB_ADDR(nasid,IIO_ICRB_A(i));
		
		crba_addr = (crba.a_addr << 3) & ~scache_linemask;

		if (paddr == crba_addr) {
			icrbb_t crbb = *(icrbb_t*)
				REMOTE_HUB_ADDR(nasid,IIO_ICRB_B(i));
			icrbc_t crbc = *(icrbc_t*)
				REMOTE_HUB_ADDR(nasid,IIO_ICRB_C(i));
			icrbd_t crbd = *(icrbd_t*)
				REMOTE_HUB_ADDR(nasid,IIO_ICRB_D(i));

			cmn_err(CE_WARN,
			"NASID %d IIO_ICRB[%d] ICRB[A-D] 0x%x 0x%x 0x%x 0x%x",
				nasid,i,
				crba.reg_value,crbb.reg_value,
				crbc.reg_value,crbd.reg_value);
		}
	}
}


/*
 * Gateway to handle IO errors.
 * Anytime CPU gets an error interrupt from hub or a bus error, it 
 * ends up invoking this routine.
 *
 * XXX
 * This routine needs a better way to decide if the parameters passed
 * to it are valid. In particular, how to validate paddr, vaddr ?
 * Not all error cases can pass all parameters.
 */
int
kl_ioerror_handler(
	cnodeid_t 	errnode, 	/* Node where error was noticed */
	cnodeid_t	srcnode,	/* Node responsible for error (ifany)*/
	cpuid_t		srccpu,		/* CPU responsible for error (ifany) */
	int 		error_code, 	/* error code */
	paddr_t 	paddr, 		/* Physicl address of error (if any) */
	caddr_t 	vaddr,		/* Virtual address of error (if any) */
	ioerror_mode_t	mode)
{
	ioerror_t	ioerror;
	vertex_hdl_t	hubv;
	int		rc;

	IOERROR_INIT(&ioerror);

	/*
	 * errnode would always be valid.
	 */
#ifdef DEBUG
	if (errnode == CNODEID_NONE) {
		cmn_err(CE_CONT, "paddr = 0x%llx, vaddr = 0x%llx\n",
			paddr, paddr);
	}
#endif /* DEBUG */
	ASSERT(errnode != CNODEID_NONE);

	IOERROR_SETVALUE(&ioerror,errnode,errnode);

	if (srcnode != CNODEID_NONE ){
		IOERROR_SETVALUE(&ioerror,srcnode,srcnode);
	}

	if (srccpu != CPU_NONE) {
		IOERROR_SETVALUE(&ioerror,srccpu,srccpu);
	}

	/*
	 * Use paddr to figure out the widget and other
	 * associated hardware..
	 * 
	 * We get this address from TLB pfn in case of Read error, 
	 * or from hub PRTE in case of write errors. 
	 * In either case, we should be able to
	 * evaluate the widget it and offset using this address.
	 *
	 * Remember: paddr == 0 is a valid address. It
	 * maps to widget 0 base address.
	 */
	IOERROR_SETVALUE(&ioerror,hubaddr,paddr);
	if (error_code & IOECODE_PIO) {
		/*
		 * In case of read/write error, we can evaluate the
		 * widget number based on the physical address.
		 */
		if (BWIN_WINDOWNUM(paddr)){
			/* 
			 * paddr is a large window address. 
			 * get the widget number by looking up the
			 * hub PIO map.
			 * XXX- need to set ioerror widgetnum
			 * XXX- need to set ioerror xtalkaddr
			 * (is this handled elsewhere?)
			 */
		} else {
			/* 
			 * paddr is a small window address.
			 * find out the widget number 
			 */
			int	widgetnum = SWIN_WIDGETNUM(paddr);
			ulong_t	xtalkaddr = SWIN_WIDGETADDR(paddr);
			ASSERT(widgetnum <= SWIN_WIDGET_MASK);
			IOERROR_SETVALUE(&ioerror,widgetnum,widgetnum);
			IOERROR_SETVALUE(&ioerror,xtalkaddr,xtalkaddr);
		}
	}

	if (vaddr) {
		IOERROR_SETVALUE(&ioerror,vaddr,vaddr);
	}

	IOERROR_DUMP("kl_ioerror_handler", error_code, mode, &ioerror);

	hubv = cnodeid_to_vertex(errnode);

	rc = hub_ioerror_handler(hubv, error_code, mode, &ioerror);

	if ((mode == MODE_DEVPROBE) || (mode == MODE_DEVUSERERROR)){
		/* If in probing mode, return everything is fine. */
		/* ASSERT(rc == IOERROR_HANDLED); */
		return rc;
	}
	/*
	 * There was a device error. If return code for error handler indicates
	 * everything is ok, we can go through the device re-enable phase.
	 * otherwise, indicate fatal error.
	 */
	if (rc == IOERROR_HANDLED) {
		return rc;
	}
	/* Dump data collected in ioerror, and die */
	/* Function to dump error data TBD */
	ioerror_dump("kl_ioerror_handler Failed handling ", error_code, 
				MODE_DEVERROR, &ioerror);

	return rc;
}

void
kl_panic_setup(void)
{
	/* Change the console to PIO mode and possibly break the putbuf lock */
	enter_panic_mode();
	if (check_ni_errors())
		hubni_error_handler("error_dump code", 1);
	sn0_error_dump("");
}

void
kl_panic(char *str)
{
	kl_panic_setup();
	cmn_err(CE_PBPANIC|CE_PHYSID, str);
}

#if defined (VTOP_DEBUG)
void
show_pde_info(pde_t *pd, char *s)
{
    cmn_err(ERR_WARN, "%s %x pgi pfn 0x%x uncattr 0x%x ccalgo 0x%x pagesize 0x%x",
	    s, 	pd->pgi, pg_getpfn(pd), pg_getuncattr(pd),
	    pg_getcc(pd),PAGE_MASK_INDEX_TO_SIZE(pg_get_page_mask_index(pd)));
}
#endif /* VTOP_DEBUG */

static paddr_t
vphys_get_pattr(k_machreg_t vaddr, uint *uattr, uint *calgo)
{
        paddr_t paddr;

	if (IS_XKPHYS(vaddr)) {
	    paddr = XKPHYS_TO_PHYS(vaddr);
	    *calgo = CACHE_ALGO(vaddr);
	    *uattr =  UNCACHED_ATTR(vaddr);
	}
	else if (IS_COMPAT_PHYS(vaddr)) {
	    paddr = COMPAT_TO_PHYS(vaddr);
	    if (IS_COMPATK0(vaddr)) {
		*calgo = CONFIG_COHRNT_EXLWR;
		*uattr = 0;
	    }
	    else {
		*calgo = CONFIG_UNCACHED;
		*uattr = UATTR_HSPEC;
	    }
	}
	return paddr;
}

static paddr_t 
pde_get_pattr(k_machreg_t vaddr, uint *uattr, uint *calgo, pde_t *pdinfo)
{
	pfn_t pfn;
	uint	 	poff;
	__psunsigned_t  page_size;
	paddr_t paddr;
    
	*uattr = pg_getuncattr(pdinfo);
	*calgo = pg_getcc(pdinfo);
	pfn	   = pg_getpfn(pdinfo);
	page_size = PAGE_MASK_INDEX_TO_SIZE(pg_get_page_mask_index(pdinfo));

	poff  = vaddr & (page_size - 1);
	paddr = ((__psunsigned_t)pfn << PNUMSHFT) | poff;

	return paddr;
}



static paddr_t
vtop_attr_get(k_machreg_t vaddr,  uint *uattr,  uint *calgo)
{
	paddr_t 	paddr;

	*uattr = *calgo = 0;

	if (IS_XKPHYS(vaddr) || IS_COMPAT_PHYS(vaddr)) {
		paddr = vphys_get_pattr(vaddr, uattr, calgo);
	}
	else {
	    pde_t pd_info;

	    tlb_vaddr_to_pde(tlbgetpid(), (caddr_t)vaddr, &pd_info);
#if defined (VTOP_DEBUG)
    {
	    pde_t pd_info1;
	    
	    show_pde_info(&pd_info, "TLB");
	    vaddr_to_pde((caddr_t)vaddr, &pd1_info);
	    show_pde_info(&pd1_info, "PAGE TABLE");
    }
#endif /* VTOP_DEBUG */

	    if (!pg_isvalid(&pd_info)) {
#if defined (DEBUG)			
		if (debug_error_message)
		    cmn_err(ERR_WARN, "Vaddr %x not in tlb: using pdes.", 
			    vaddr);
#endif
		if (!curvprocp) return -1;
		vaddr_to_pde((caddr_t)vaddr, &pd_info);

		/* If page is not valid in pte, retry this access.
		 */
		if (!pg_isvalid(&pd_info)) 
		    return 0;
	    }
	    paddr = pde_get_pattr(vaddr, uattr, calgo, &pd_info);
	}
	return paddr;
}


void
error_clean_cache_line(paddr_t paddr)
{
    cacheop_t cop;
    paddr_t maxaddr;

    paddr &= ~scache_linemask;
    maxaddr = ctob(btoc(paddr + 1));

    for (; paddr < maxaddr; paddr += (scache_linemask + 1))
	ecc_cacheop_get(CACH_SD + C_HINV, PHYS_TO_K0(paddr), &cop);
    return;
}

cpuid_t
initiator_to_cpunum(int initiator)
{
    cpuid_t cpu;
    
    if (INITIATOR_IS_CPU(initiator)) {
	nasid_t	nasid = INITIATOR_TO_NODE(initiator);
	cnodeid_t cnode = NASID_TO_COMPACT_NODEID(nasid);
	cpu = cnode_slice_to_cpuid(cnode, INITIATOR_TO_CPU(initiator));
	return cpu;
    }
    return -1;
}


void
poison_page(paddr_t paddr)
{
    int i;
    int state;
    __uint64_t vec_ptr, elo;
    cpuid_t cpu;
    cpumask_t	error_cpumask;

    paddr = ctob(btoct(paddr));
    CPUMASK_CLRALL(error_cpumask);
    CPUMASK_SETB(error_cpumask, 0);
    
    for (i = 0; i < ctob(1); i += CACHE_SLINE_SIZE, paddr += CACHE_SLINE_SIZE){
	get_dir_ent(paddr, &state, &vec_ptr, &elo);
	set_dir_state(paddr, MD_DIR_POISONED);
	if ((state == MD_DIR_EXCLUSIVE) || (state == MD_DIR_BUSY_EXCL)) {
	    if ((cpu = initiator_to_cpunum(vec_ptr)) == -1) 
		/*XXX FIXME XXX*/
		continue;
		
	    if (CPUMASK_TSTB(error_cpumask, cpu))
		continue;
	    
	    CPUMASK_SETB(error_cpumask, cpu);
		    
	    if (cpu == cpuid()) 
		error_clean_cache_line(paddr);
	    else {
		ASSERT_ALWAYS((cpu >= 0) && (cpu <= maxcpus));
		ASSERT_ALWAYS(cpu_enabled(cpu));
		cpuaction(cpu, (cpuacfunc_t)error_clean_cache_line, A_NOW, paddr);
	    }
	}
    }
    return;
}


int
error_page_discard(paddr_t paddr, int permanent, int correctable)
{
    int result;

    paddr = ctob(btoct(paddr));

    result = page_discard(paddr, permanent, correctable);
    return result;
}

void
sn0_error_mark_page(paddr_t paddr)
{
	poison_page(paddr);
}

int
sn0_error_reclaim_page(paddr_t paddr, int permanent)
{
    int i;

    if (permanent) return 0;

    paddr = ctob(btoct(paddr));
#if defined (ERROR_DEBUG)
    cmn_err(CE_NOTE, "Discarded paddr 0x%x being cleaned", paddr);
#endif
    for (i = 0; i < ctob(1); i += CACHE_SLINE_SIZE) {
	set_dir_state(paddr + i, MD_DIR_UNOWNED);
    }
    return 1;
}



/*
 * Dummy routine to force inclusion of badaddr and its friends
 * defined in klprobe.c
 * This is done in place of adding an extra -u in Makefile, which
 * applies for all platforms.
 * This can go away if any routine in klprobe.c gets referenced by
 * some other file under ml directory.
 * Note: No one invokes dummy_badaddr 
 */
void
dummy_badaddr(void)
{
    badaddr((volatile void *)K0BASE, 4);
}


void
error_reg_partition_handler(error_result_t (*handler)(eframe_t *, 
						      paddr_t,
						      enum part_error_type))
{
    part_error_handler = handler;
}


int
error_check_part_page(paddr_t paddr)
{
    pfd_t *pfd;

    pfd = pfntopfdat(pnum(paddr));

    if (!page_validate_pfdat(pfd))
	return 0;

    if (pfdat_isxp(pfd))
	return 1;

    return 0;
}

error_result_t
partition_handle_error(eframe_t *ep, paddr_t paddr, enum part_error_type code)
{
    return (part_error_handler ? 			
	    (*part_error_handler)(ep, paddr, code) :
	    (cmn_err(CE_WARN, "partition error handler not registered"), ERROR_FATAL));
}

void
errste_prfunc(char *buf)
{
	int	len = strlen(sneb_buf);
	int	wanted = strlen(buf);

	if (len + wanted > sneb_size)
		return;

	sprintf(sneb_buf + len, buf);
}

void
errorste_reset(void)
{
	ASSERT(sneb_buf);

	kl_error_show_reset_ste("HARDWARE ERROR STATE AT SYSTEM RESET:\n",
		"END HARDWARE ERROR STATE AT SYSTEM RESET:\n", 
		errste_prfunc);
}

#if defined (T5_WB_SURPRISE_WAR)

int
error_check_wb_surprise(paddr_t paddr, hubreg_t *reg)
{
    __psunsigned_t handle = 0;
    int rc;
    md_proto_error_t proto_err;
    int state;
    __uint64_t vec_ptr, elo;

    get_dir_ent(paddr, &state, &vec_ptr, &elo);

    if (state != MD_DIR_POISONED)
	return 0;

    log_md_errors(NASID_GET(paddr));

    while (rc = check_md_errors(paddr, &proto_err.perr_reg, 
				ERROR_LONG_TIME, &handle))
	if (rc == PROTO_ERROR) break;
    
    if (rc == 0) return 0;

    if (proto_err.perr_fmt.msg_type != PROTO_ERR_MSG_WB) return 0;

    *reg = proto_err.perr_reg;

    return 1;
}


int
error_try_wb_surprise_war(paddr_t paddr)
{
    md_proto_error_t proto_err;
    cpuid_t cpu;
    int state;

    if (page_isdiscarded(paddr))
	return 1;

    if (error_check_wb_surprise(paddr, &proto_err.perr_reg) == 0)
	return 0;
    
    state = proto_err.perr_fmt.dir_state >> 1;

    if ((state == MD_DIR_EXCLUSIVE) || (state == MD_DIR_BUSY_EXCL)) {
	cpu = initiator_to_cpunum(proto_err.perr_fmt.initiator);
	if (cpu != -1) {
	    ASSERT_ALWAYS((cpu >= 0) && (cpu <= maxcpus));
	    ASSERT_ALWAYS(cpu_enabled(cpu));
	    cpuaction(cpu, (cpuacfunc_t)error_clean_cache_line, A_NOW, paddr);
	}
    }

    if (error_page_discard(paddr, TRANSIENT_ERROR, UNCORRECTABLE_ERROR)) {
	cmn_err(ERR_NOTE, "Error: Likely T5 writeback surprise. WAR done");
	return 1;
    }
    else {
	cmn_err(ERR_WARN, "Error: Likely T5 writeback surprise. War failed");
	return 0;
    }
}


int
error_search_wb_surprise(void)
{
    cnodeid_t cnode;
    struct md_err_reg *next_ent;
    sn0_error_log_t *kl_err_log;

    for (cnode = 0; cnode < maxnodes; cnode++) {
	if ((kl_err_log = SN0_ERROR_LOG(cnode)) == NULL) continue;
	log_md_errors(COMPACT_TO_NASID_NODEID(cnode));
	next_ent = MD_ERR_LOG_FIRST(kl_err_log);
	while (next_ent) {	
	    switch (next_ent->er_type) {
	    case PROTO_ERROR: 
	    {
		md_proto_error_t proto_err;

		proto_err.perr_reg = next_ent->er_reg;
		if (proto_err.perr_fmt.msg_type != PROTO_ERR_MSG_WB) 
		    break;
		return 1;
	    }
		
	    default:
		break;
	    }
	    next_ent = MD_ERR_LOG_NEXT(kl_err_log, next_ent);
	}
    }
    
    return 0;
}

#endif	/* T5_WB_SURPRISE_WAR */

#if defined (SN0) && defined (FORCE_ERRORS)
extern void error_force_init(void);
void
dummy_error_force_init(void)

{
    error_force_init();

}

paddr_t
error_force_get_user_paddr(__psunsigned_t vaddr)
{
    uint uattr, calgo;

    return vtop_attr_get(vaddr, &uattr, &calgo);
}


#endif /* SN0 && FORCE_ERRORS */
