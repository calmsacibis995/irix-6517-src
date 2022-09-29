/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.79 $"

/*
 * Load/Store parity error hack
 */

#if defined(IP20) || defined(IP22) || defined(IPMHSIM)

#include <sys/types.h>
#include <ksys/as.h>
#include <os/as/as_private.h> /* XXX */
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/prctl.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/ksignal.h>
#include <sys/cmn_err.h>
#include <sys/immu.h>
#include <sys/sbd.h>
#include <sys/inst.h>
#include <sys/fpu.h>
#include <os/as/pmap.h> /* XX */
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/vdma.h>
#include <sys/errno.h>
#include <sys/parity.h>
#include <sys/pfdat.h>
#include <sys/vnode.h>
#include <sys/numa.h>
#include <ksys/cacheops.h>
#include <ksys/vmmacros.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */

#ifdef _MEM_PARITY_WAR
/*
 * todo:
 *	determine if we can workaround bit3 and svideo problems
 *	mpin page so it doesn't get paged out and cause GIO error (lockpages)
 *	log bad data also
 *	print corrected if necessary
 *	need llemulate?
 *	is the sigaddq stuff really necessary?	
 *	other instructions
 *	...and lots, lots more.
 */

/* Clean up the cache in the
 * cache error handler (perr_save_info()) and synthesize a software
 * parity exception, if possible.  Then handle fixing the error from 
 * the normal exception or the parity exception
 * handler (dobuserr()), using information saved
 * in the ECC frame pointer.  This causes a couple of weird
 * things to take place:
 *
 * 	1.  SysAD parity checking must be temporarily disabled
 * 		when the cache error handler completes because if
 *		the parity error occurred on an instruction fetch,
 *		the cache error exception will be raised before
 *		the parity error interrupt.
 *	2.  The bad address must be invalidated again from 
 *		dobuserr() because if it was an instruction, it
 *		will attempt to run after the cache error handler
 *		eret's and will be valid in the cache again
 *		before the interrupt can be responded to.
 */

/* Where "try_harder" is ifdef'd, there are possible further 
 * workarounds that are more difficult than the basic set, and
 * which have not been implemented.
 */

/*REFERENCED(for core dumps)*/
static char *memory_patch_id = "IRIX memory patch version 3.4";

int cpu_recover(eframe_t *, k_machreg_t *);
int transient_recover(paddr_t);
int ktext_recover(paddr_t);
int ldst_recover(eframe_t *, k_machreg_t *);
int ecc_discard_page(pgno_t, eframe_t *, k_machreg_t *);
#endif /* _MEM_PARITY_WAR */

caddr_t map_physaddr(paddr_t);
void unmap_physaddr(caddr_t);

extern void	dcache_wbinval(void *, int);

#ifdef _MEM_PARITY_WAR
int perr_fixup_caches(eframe_t *);
static int ecc_cache_block_mask(int);
int ecc_same_cache_block(int,paddr_t,paddr_t);

void enable_sysad_parity(void);
void disable_sysad_parity(void);
int is_sysad_parity_enabled(void);
void use_old_mem_parity_patch(void);

int decode_inst(eframe_t *, int, int *, k_machreg_t *, int *);
static int datafix(eframe_t *, inst_t, inst_t, int, k_machreg_t, paddr_t);

/* means of doing an uncached read from a physical address */
unsigned int r_phys_word(paddr_t);
unsigned int r_phys_byte_nofault(paddr_t);
#define R_PHYS_WORD(x) (r_phys_word((paddr_t) x))

extern int get_sr(void);
#define CACHE_EXCEPTION_MODE(s) ((s) & SR_ERL)
#define EXCEPTION_MODE(s) ((s) & (SR_EXL|SR_ERL))

int tlb_to_phys(k_machreg_t , paddr_t *, int *);

extern int is_branch(inst_t);
int is_in_main_memory(pgno_t);
k_machreg_t emulate_branch(eframe_t *, inst_t , __int32_t, int *);

int log_perr(uint addr, uint bytes, int no_console, int print_help);
int svideo_probe(void), bit3_probe(void);
int parwbad(paddr_t, uint, int);
void calculate_vparity(void);

void emulate_lwc1(int, caddr_t);
void emulate_ldc1(int, caddr_t);
void emulate_swc1(int, caddr_t);
void emulate_sdc1(int, caddr_t);

int _read_tag(int, caddr_t, int *);
int get_r4k_config(void);

void _c_ist(int, caddr_t, int *);

#define LINESIZE	128	/* maximum cache line size */

/* Since we're printing out codes as decimal numbers, it's nice to have
 * the actual values specified here where we can see them easily.
 * ONLY ADD NEW ELEMENTS AT THE END!  We have logs of kernels using these
 * values, and it would be bad to screw them up.
 */
enum perr_stat {
    PE_TOTAL=0,			/* should be sum of following 5 */
    PE_MULTIPLE=1,		/* multiple errors - give up */
    PE_CPU_RECOV=2,		/* cpu error, recovered */
    PE_CPU_NONRECOV=3,		/* cpu error, not recovered */
    PE_GIO_RECOV=4,		/* gio error, recovered */
    PE_GIO_NONRECOV=5,		/* gio error, not recovered */
    PE_UNKNOWN=6,		/* unknown error, not recovered */

    PE_CR_TRANS=7,		/* cpu err recovered from transient */
    PE_CR_KTEXT=8,		/* cpu err recovered by kernel text */
    PE_CR_LDST=9,		/* cpu err recovered by load/store */
    PE_CR_CLEAN=10,		/* cpu err recovered by clean page */
    PE_CR_UNREC=11,		/* cpu err not recovered */

    PE_KTEXT_INVALID=12,	/* kernel parity not yet computed */
    PE_KTEXT_MULTIPLE=13,	/* multiple errs on kernel text page */

    PE_LDST_INST=14,		/* ld/st err occurred in inst */
    PE_LDST_DATA=15,		/* ld/st err occurred in data */
    PE_LDST_INSTDATA=16,	/* ld/st err occurred on line with both */
    PE_LDST_BRANCH=17,		/* ld/st err detected on branch */

    PE_LDST_NOPHYS1=18,		/* ld/st couldn't get vaddr of inst */
    PE_LDST_NOPHYS2=19,		/* ld/st couldn't get vaddr of data */
    PE_LDST_EOP=20,		/* ld/st branch at eop */
    PE_LDST_BRBR=21,		/* ld/st branch in bdslot */
    PE_LDST_DECODE=22,		/* ld/st couldn't decode instruction */
    PE_LDST_LOST=23,		/* ld/st physical address looks wrong */

    PE_LDST_HIT_LD=24,		/* ld/st hit bad byte on load */
    PE_LDST_HIT_ST=25,		/* ld/st hit bad byte on store */
    PE_LDST_MISS_LD=26,		/* ld/st hits bad line, misses bad byte */
    PE_LDST_MISS_ST=27,		/* ld/st hits bad line, misses bad byte */

    PE_LDST_USER_FIXED=28,	/* ld/st fix user address err */
    PE_LDST_USER_NOTFIXED=29,	/* ld/st couldn't fix user address err */

    PE_LDST_KERN_FIXED=30,	/* ld/st fix kernel address err */
    PE_LDST_KERN_NOTFIXED=31,	/* ld/st couldn't fix kernel address err */

    PE_DEC_UNKNOWN=32,		/* decode failed on unknown instruction */

    PE_FIX_COP1BR=33,		/* fix failed on cop1 branch delay slot */
    PE_FIX_BRFAIL=34,		/* fix failed to emulate branch */
    PE_FIX_ILLADDR=35,		/* fix failed on illegal user address */
    PE_FIX_NOPHYS=36,		/* fix failed to get referenced vaddr */
    PE_FIX_UNKNOWN=37,		/* fix failed on unknown instruction */
    PE_FIX_DECODE=38,		/* fix failed on cop1 branch delay slot */

    PE_MAX
};

/* Always access this uncached */
#define MAX_PERR_LOG	20

struct ces_data_s {
	int sysad_parity_enabled_v;
	struct ces_s {
		uint cpu_status, cpu_addr;
		k_machreg_t epc;
		uint sr;
		uint cause;	/* also indicates state is valid */
		uint cache_err;
		caddr_t flushaddr;
		uint count;
		uint double_err;
		uint mode;	/* PERC_* mode */
		uint flags;
		paddr_t epc_paddr; /* physical address for epc */
		k_machreg_t epc_dest; /* destination address for inst at epc */
		int epc_dest_type; /* epc instruction type */
		int epc_dest_width; /* width for destination address */
		paddr_t epc_dest_paddr; /* physical address for epc_dest */
		inst_t inst;	/* faulting instruction */
		inst_t bd_inst;	/* branch delay instruction */
		k_machreg_t bd_epc; /* branch delay instruction vaddr */
		paddr_t bd_paddr; /* branch delay instruction paddr */
		k_machreg_t bd_dest;
		int	bd_dest_type;
		int	bd_dest_width;
		paddr_t bd_dest_paddr;
		k_machreg_t failed_vaddr; /* vaddr which is actually bad */
		k_machreg_t eccfp[ECCF_SIZE / sizeof(k_machreg_t)];
		eframe_t *efp;
	} cache_err_state;
	struct ces_s dbg_cache_err_state;
	int ces_pe_statistics[PE_MAX];
	int ces_p_code;
	int ces_p_corrected;
	struct perr_log_info_s {
		paddr_t paddr;
		uint time;
		k_machreg_t err_epc;
		k_machreg_t epc;
		k_machreg_t ra;
		eframe_t *ep;
	} perr_log_info[MAX_PERR_LOG];
	unsigned int perr_log_count;

};

#define ces_data_p ((volatile struct ces_data_s *) (CACHE_ERR_CES_DATA_P))

/* cache_err_state.flags: */
#define CESF_DATA_IS_BAD	0x0001
#define CESF_INST_IS_BAD	0x0002
#define CESF_EPC_PADDR_VALID	0x0004
#define CESF_DEST_VADDR_VALID	0x0008
#define CESF_DEST_PADDR_VALID	0x0010
#define CESF_EPC_CACHED		0x0020
#define CESF_DEST_CACHED	0x0040
#define CESF_INST_VALID		0x0080
#define CESF_VADDR_VALID	0x0100
#define CESF_INST_LDST		0x0200
#define CESF_BD_INST_VALID	0x0400
#define CESF_BD_PADDR_VALID	0x0800
#define CESF_IS_BRANCH		0x1000
#define CESF_BD_CACHED		0x2000
#define CESF_BD_DEST_VALID	0x4000
#define CESF_BD_DEST_PADDR_VALID 0x10000
#define CESF_BD_DEST_CACHED	0x20000
#define CESF_BD_INST_LDST	0x40000
#define CESF_PERR_IS_FORCED	0x80000

#define sysad_parity_enabled \
	(ces_data_p->sysad_parity_enabled_v)

#define DECLARE_PSTATE \
	volatile struct ces_s *pstate

#define DECLARE_DBG_PSTATE \
	volatile struct ces_s *dbg_pstate

#define INIT_PSTATE \
	((ces_data_p == NULL) ? 0 \
	 : ((pstate = &(ces_data_p->cache_err_state)), 1))

#define INIT_DBG_PSTATE \
	(dbg_pstate = &(ces_data_p->dbg_cache_err_state))

k_machreg_t *eccf_addr;

char bytetab[16] = {0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
#define BYTEOFF(bl)	((bl&0xf0)?(bytetab[bl>>4]):(bytetab[bl]+4))

/*
 *	perr_statistics -- update statistics counter 
 */

static int
perr_statistics(int code)
{
	ces_data_p->ces_pe_statistics[code]++;
	return(0);
}
#define PERR_STATISTICS(code) perr_statistics(code)

/*
 * on recoverable error, p_code indicates which method we use
 * on non-recoverable error, p_code indicates why we fail
 * p_code uses the constant defined in perr_stat and -1 to indicate invalid
 */
#define p_code (ces_data_p->ces_p_code)

static int
perr_statistics_code(int code)
{
	p_code = code;
	return(perr_statistics(code));
}
#define PERR_STATISTICS_CODE(code) perr_statistics_code(code)

/*
 * If the error was recoverable, it might not have actually been corrected.
 * This variable will be 0 if the error was not corrected, and 1 if it was
 * actually corrected.  (Overwritten with new data.)
 */
#define p_corrected (ces_data_p->ces_p_corrected)

/*
 * This variable indicates that some condition was detected that precludes
 * running with SysAD parity checking enabled.  If it is true, then we rely
 * on the old (dwong bzero/bcopy patch) techniques for recovering from
 * parity errors.
 */
int no_parity_workaround;

/* perr_init - initialize parity error handler
 *
 * Must be called after system can badaddr().
 *
 * SysAD parity is no longer enabled in mlreset, this routine is called
 * from main just before the io init routines, and must decide whether
 * or not to enable SysAD parity checking.
 */
void
perr_init(void)
{
    /* Probe for starter video and bit3 board.
     * Enable sysad parity checking if neither is
     * installed.
     */
    ces_data_p->perr_log_count = MAX_PERR_LOG;

    if (!svideo_probe() && !bit3_probe() && !no_parity_workaround) {
	calculate_vparity();
	enable_sysad_parity();
    } else 
	use_old_mem_parity_patch(); /* Disables SysAD parity */
    
	if(!is_sysad_parity_enabled())
		printf ("!Perr_init: SysAD parity is not enabled.\n" +
			(showconfig ? 1 : 0));
}

#define perr_log(perr_log__ep,perr_log__paddr,perr_log__err_epc) \
{	\
	struct perr_log_info_s *perr_log_info_p; \
	\
	if (++ces_data_p->perr_log_count >= MAX_PERR_LOG) \
		ces_data_p->perr_log_count = 0; \
	\
	perr_log_info_p = (struct perr_log_info_s *)ces_data_p->perr_log_info + ces_data_p->perr_log_count; \
	perr_log_info_p->paddr = (perr_log__paddr); \
	perr_log_info_p->time = lbolt; \
	perr_log_info_p->err_epc = (perr_log__err_epc); \
	perr_log_info_p->epc =  (perr_log__ep)->ef_epc; \
	perr_log_info_p->ra = (perr_log__ep)->ef_ra; \
	perr_log_info_p->ep = (perr_log__ep); \
}

#define GIO_ERRMASK     0xff00
#define CPU_ERRMASK     0x3f00

extern uint cpu_err_stat, gio_err_stat;
extern uint cpu_par_adr, gio_par_adr;

void
perr_display(int recoverable, int corrected, k_machreg_t *eccfp, int forced)
{
    paddr_t paddr;
    uint bl;
    char *detected;
    static paddr_t last_paddr;
    static uint last_bl;
    static time_t last_time;

    if (CPU_ERRMASK & eccfp[ECCF_CPU_ERR_STAT]) {
	paddr = (paddr_t) eccfp[ECCF_CPU_ERR_ADDR];
	bl = eccfp[ECCF_CPU_ERR_STAT];
	detected = "CPU";
    } else if (GIO_ERRMASK & eccfp[ECCF_GIO_ERR_STAT]) {
	paddr = (paddr_t) eccfp[ECCF_GIO_ERR_ADDR];
	bl = eccfp[ECCF_GIO_ERR_STAT];
	detected = "GIO";
    } else {
	cmn_err(CE_CONT, "Spurious memory error detected\n");
	return;
    }

    /*
     * don't want to clutter up the SYSLOG with the same message
     * don't need to check the source since GIO error is unrecoverable
     */
    if (recoverable && !corrected && paddr == last_paddr && bl == last_bl
	&& (time - last_time) < 10)
	    return;

    last_paddr = paddr;
    last_bl = bl;
    last_time = time;

    cmn_err(CE_CONT, "!%s %s %s %s at 0x%x <0x%x> code:%d\n" +
	    (recoverable ? 0 : 1), 
	recoverable ? "Recoverable" : "Nonrecoverable",
	forced ? "cache parity error" : "memory parity error",
	corrected ? "corrected by" : "detected by",
	detected,
	paddr,
	bl,
	p_code);

    if (!(eccfp[ECCF_GIO_ERR_STAT] & GIO_ERRMASK) && !forced)
	    /* print out SIMM location only if mem err */
	    log_perr((uint)paddr, bl, recoverable, !recoverable);
}

/*
 * perr_mem_init -- allocate ces_data
 *
 *	Argument is next physical address to allocate.
 *	Returns number of bytes allocated
 *
 *	Assures that ces_data does not share a cache line with
 *	any other structure.
 */

#define DMUX_PAGEBUF_SIZE 4096
char	*dmux_pagebuf = (caddr_t) (-1L);

extern int scache_linemask;

#define CES_DATA_SIZE ((sizeof(struct ces_data_s) + scache_linemask) & \
			~scache_linemask)

int
perr_mem_init(caddr_t basep)
{
	int	count;

	if (dmux_pagebuf == ((caddr_t) (-1L))) {
		CACHE_ERR_CES_DATA_P = ((((__psunsigned_t) basep) + scache_linemask) 
						& ~scache_linemask);
		dmux_pagebuf = (char *) (((CACHE_ERR_CES_DATA_P +
					   CES_DATA_SIZE) + 
					  scache_linemask) & 
					 ~scache_linemask);
		count = ((dmux_pagebuf + DMUX_PAGEBUF_SIZE) - basep);
		bzero(basep,count);
		return(count);
	} else
		return(0);
}

/* Save enough information to handle parity error recovery.  
 * This is still running on the cache error exception
 * vector and stack.  It should compute the address of the bad cache
 * line and invalidate it.  It should touch only uncached memory
 * until it has fixed the cache.
 *
 * The ERL bit is on.
 *
 * This is currently a simulation of the environment that will
 * be provided to perr_recover() from the cache error handler in
 * the case of a memory parity error.
 *
 * Returns 1 on success, 0 on failure.
 */
void disable_sysad_parity_erl(void);

int
perr_save_info(eframe_t *ep, k_machreg_t *eccfp, uint cache_err,
	k_machreg_t errorepc, int mode)
{
    DECLARE_PSTATE;
    DECLARE_DBG_PSTATE;
    uint cpu_status;
    uint cpu_addr;
    int	cached = 0;
    paddr_t ecc_paddr;

    if (! INIT_PSTATE)
	    return(0);

    /* Took another exception before getting chance to handle first.
     */
    if (pstate->cause) {
	    pstate->double_err++;
	    INIT_DBG_PSTATE;
	    dbg_pstate->count++;
	    dbg_pstate->cache_err = cache_err;
	    dbg_pstate->epc = errorepc;
	    dbg_pstate->sr = ep->ef_sr;
	    dbg_pstate->cause = ep->ef_cause;
	    dbg_pstate->efp = (eframe_t *)PHYS_TO_K1(ep);
	    dbg_pstate->mode = mode;
	    dbg_pstate->cpu_status = *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT);
	    dbg_pstate->cpu_addr = *(volatile uint *)PHYS_TO_K1(CPU_ERR_ADDR);
	    return 0;
    }

    pstate->count++;
    pstate->cache_err = cache_err;
    pstate->epc = errorepc;
    pstate->mode = mode;
    pstate->flags = 0;
	
    /* Read the error registers but leave the interrupt enabled.
     * Don't mess up these registers, since we're going to take

     * the parity error interrupt momentarily and wish to keep
     * the state correct.
     */
    if (eccfp == NULL) {
	    cpu_status = cpu_err_stat;
	    cpu_addr = cpu_par_adr;
    } else {
	    cpu_status = eccfp[ECCF_CPU_ERR_STAT];
	    cpu_addr = eccfp[ECCF_CPU_ERR_ADDR];
    }
    pstate->cpu_status = cpu_status;
    pstate->cpu_addr = cpu_addr;

    if (tlb_to_phys( errorepc,
		    (paddr_t *) &pstate->epc_paddr,
		    &cached)) {
	    pstate->flags |= CESF_EPC_PADDR_VALID;
	    if (cached)
		    pstate->flags |= CESF_EPC_CACHED;
    }

    /* Must be CACHERR_EE (SysAD) and a CPU read error */
    if (cache_err & CACHERR_EE) {
	if ((cpu_status & CPU_ERR_STAT_RD_PAR) == CPU_ERR_STAT_RD_PAR) {
	    p_code = -1;

	    /* Compute all bits of the bad address from the 
	     * byte lane and cpu error address register.
	     */
	    ecc_paddr = (cpu_addr&~7)+BYTEOFF(cpu_status&0xff);

	    /* save error state for debug -- let's keep the 
		errorepc
		sr
		cause
		cache_err
		cache error eframe pointer
		physical address reported by MC
	    */
	    pstate->sr = ep->ef_sr;
	    pstate->cause = ep->ef_cause;
	    pstate->efp = (eframe_t *)PHYS_TO_K1(ep);
	    pstate->eccfp[ECCF_PADDR] = ecc_paddr;
	    pstate->eccfp[ECCF_CPU_ERR_STAT] = cpu_status;
	    pstate->eccfp[ECCF_CPU_ERR_ADDR] = cpu_addr;
	    if (eccfp != NULL) {
		    eccfp[ECCF_PADDR] = ecc_paddr;
		    pstate->eccfp[ECCF_CACHE_ERR] = eccfp[ECCF_CACHE_ERR];
		    pstate->eccfp[ECCF_TAGLO] = eccfp[ECCF_TAGLO];
		    pstate->eccfp[ECCF_ECC] = eccfp[ECCF_ECC];
		    pstate->eccfp[ECCF_ERROREPC] = eccfp[ECCF_ERROREPC];
	    	    pstate->eccfp[ECCF_GIO_ERR_STAT] = eccfp[ECCF_GIO_ERR_STAT];
	    	    pstate->eccfp[ECCF_GIO_ERR_ADDR] = eccfp[ECCF_GIO_ERR_ADDR];
	    } else {
		    pstate->eccfp[ECCF_CACHE_ERR] = cache_err;
		    pstate->eccfp[ECCF_TAGLO] = 0;
		    pstate->eccfp[ECCF_ECC] = 0;
		    pstate->eccfp[ECCF_ERROREPC] = ep->ef_epc;
	    	    pstate->eccfp[ECCF_GIO_ERR_STAT] = gio_err_stat;
	    	    pstate->eccfp[ECCF_GIO_ERR_ADDR] = gio_par_adr;
	    }

	    if (! perr_fixup_caches(ep))
		    return(0);

	    /* try to fetch instruction and compute destination address */
	    switch (mode) {
	    case PERC_CACHE_SYSAD:
	    case PERC_CACHE_LOCAL:
		    disable_sysad_parity_erl();
		    break;
	    default:
		    disable_sysad_parity();	/* disable sysad parity */
		    break;
	    }
		    
	    if (pstate->flags & CESF_EPC_PADDR_VALID) {
		    if (ecc_same_cache_block(CACH_PI,
					     pstate->epc_paddr,
					     ecc_paddr) ||
			! (cache_err & CACHERR_ER)) {
			    pstate->failed_vaddr = pstate->epc;
			    pstate->flags |= (CESF_VADDR_VALID |
					      CESF_INST_IS_BAD);
		    } else {
			    pstate->inst = (inst_t) R_PHYS_WORD(pstate->epc_paddr);
			    pstate->flags |= CESF_INST_VALID;

			    if (is_branch(pstate->inst)) {
				    pstate->flags |= CESF_IS_BRANCH;
				    if (tlb_to_phys( errorepc + sizeof(inst_t),
						    (paddr_t *) &pstate->bd_paddr,
						    &cached)) {
					    pstate->flags |= CESF_BD_PADDR_VALID;
					    if (cached)
						    pstate->flags |= CESF_BD_CACHED;
					    pstate->bd_inst = (inst_t) R_PHYS_WORD(pstate->bd_paddr);
					    pstate->flags |= CESF_BD_INST_VALID;
				    }
			    } 

			    if (decode_inst(ep,
					    (int) pstate->inst,
					    (int *) &pstate->epc_dest_type,
					    (k_machreg_t *) &pstate->epc_dest,
					    (int *) &pstate->epc_dest_width)) {
				    pstate->failed_vaddr = pstate->epc_dest;
				    pstate->flags |= (CESF_INST_LDST |
						      CESF_VADDR_VALID |
						      CESF_DEST_VADDR_VALID);
				    if (tlb_to_phys(pstate->epc_dest,
						    (paddr_t *) &pstate->epc_dest_paddr,
						    &cached)) {
					    pstate->flags |= CESF_DEST_PADDR_VALID;
					    if (cached)
						    pstate->flags |= CESF_DEST_CACHED;
				    }
			    }
			    if (pstate->flags & CESF_BD_INST_VALID &&
				decode_inst(ep,
					    (int) pstate->bd_inst,
					    (int *) &pstate->bd_dest_type,
					    (k_machreg_t *) &pstate->bd_dest,
					    (int *) &pstate->bd_dest_width)) {
				    pstate->flags |= (CESF_BD_INST_LDST |
						      CESF_BD_DEST_VALID);
				    if (tlb_to_phys(pstate->bd_dest,
						    (paddr_t *) &pstate->bd_dest_paddr,
						    &cached)) {
					    pstate->flags |= CESF_BD_DEST_PADDR_VALID;
					    if (cached)
						    pstate->flags |= CESF_BD_DEST_CACHED;
				    }
			    }
		    }
	    }

	    /* punt multiple errors, at least for now */
	    switch (cpu_status & 0xff) {
		case 1: case 2: case 4: case 8:
		case 16: case 32: case 64: case 128:
		    break;
		default:
		    return(PERR_STATISTICS_CODE(PE_MULTIPLE));
	    }

	    /* hook for perr_recover() */
	    eccf_addr = (k_machreg_t *)pstate->eccfp;	/* save ECC frame info */

	    return 1;	/* success */
	}
    }

    return 0;	/* failed */
}

/* Called in an exception handler context -- not the cache error
 * handler.  Has a valid sp, eframe, and eccfp points to information
 * about the error saved by the cache error handler. ERL is *not*
 * enabled.
 *
 * Offending cache line has already been invalidated.  Cache has
 * been written back and invalidated so it's OK to execute cached
 * and use cached data.
 *
 * This code does not take any exceptions, or modify the contents
 * of the tlb.
 *
 * Assumptions on entry:
 * 	1.  The error was detected by the cpu
 *		(i.e. CPU_ERR_STAT and CPU_ERR_ADDR were set and valid.
 *		The globals aren't used, only what's provided in eccfp.)
 *	2.  SysAD parity checking is enabled
 *		(i.e. a second parity error will trip the cache
 *		error handler again)
 *	3.  Any interesting pages are already mapped by the tlb.
 *
 * Returns 1 if error is recovered, otherwise 0.
 */
int
perr_recover(eframe_t *ep)
{
    int recovered;
    DECLARE_PSTATE;
    k_machreg_t *eccfp;

    if (! INIT_PSTATE)
	    return(0);

	eccfp = (k_machreg_t *)pstate->eccfp;

    ASSERT(is_sysad_parity_enabled());

    PERR_STATISTICS(PE_TOTAL);

    /* Log the error */
    perr_log(ep, (paddr_t) eccfp[ECCF_PADDR], eccfp[ECCF_ERROREPC]);

    recovered = cpu_recover(ep, eccfp);

    perr_display(recovered, p_corrected, eccfp, 
		 pstate->flags & CESF_PERR_IS_FORCED);

    pstate->cause = 0;

    PERR_STATISTICS((recovered ? PE_CPU_RECOV : PE_CPU_NONRECOV));

    if (recovered)
	pstate->flags &= ~CESF_PERR_IS_FORCED;

    return(recovered);
}

/* Attempt to recover from a parity error detected by a CPU
 * read or write.
 *
 * Return 1 if error is recovered, 0 if not recovered.
 */
int
cpu_recover(eframe_t *ep, k_machreg_t *eccfp)
{
    paddr_t phys_err_addr = (paddr_t) eccfp[ECCF_PADDR];

    p_corrected = 0;

    /* attempt transient recovery */
    if (transient_recover(phys_err_addr)) {
	PERR_STATISTICS_CODE(PE_CR_TRANS);
 set_corrected:
	p_corrected = 1;
	return 1;
    }

    /* attempt kernel text recovery */
    if (ktext_recover(phys_err_addr)) {
	PERR_STATISTICS_CODE(PE_CR_KTEXT);
	goto set_corrected;
    }

    if (ecc_discard_page(pnum(phys_err_addr),
			 ep,
			 eccfp)) {
	    PERR_STATISTICS_CODE(PE_CR_CLEAN);
	    goto set_corrected;
    }

    /* attempt load/store recovery
     * ldst_recover makes sure p_code and p_corrected are set properly.
     */
    if (ldst_recover(ep, eccfp)) {
	PERR_STATISTICS(PE_CR_LDST);
	return(1);
    }

    /* Recovery not possible -- let higher level attempt to recover.
     */
    return(PERR_STATISTICS(PE_CR_UNREC));
}

/* Attempt transient parity error recovery
 *
 * Return 1 if error is recovered, 0 if not recovered
 */
int
transient_recover(paddr_t phys_err_addr)
{
    paddr_t vaddr;
    volatile uint dummy;
    int par_enabled;
    
    vaddr = phys_err_addr & ~0x7;	/* align to double */

    par_enabled = is_sysad_parity_enabled();
    if (par_enabled)
	disable_sysad_parity();		/* don't want another cache err */

    *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
    flushbus();

    dummy = R_PHYS_WORD(vaddr);
    dummy = R_PHYS_WORD(vaddr+4);

    if (par_enabled)
	enable_sysad_parity();

    if (!*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT))
	return 1;

    *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
    flushbus();
    return 0;
}

#define LOAD_INSTR	1
#define STORE_INSTR	2

/* Attempt load/store parity error recovery.  The basic theory is 
 * that the load or store which detects the error may not access
 * the actual data with bad parity.  The kernel can use uncached
 * loads and stores to emulate the problem instruction.
 *
 * Return 1 if error is recovered, 0 if not recovered
 */
int
ldst_recover(eframe_t *ep, k_machreg_t *eccfp)
{
    k_machreg_t epc = (k_machreg_t) ep->ef_epc;
    paddr_t bad_addr;
    inst_t br_inst = 0;
    inst_t inst;
    paddr_t phys_epc;
    int cached;
    DECLARE_PSTATE;

    if (! INIT_PSTATE)
	    return(0);

    ASSERT(eccfp == pstate->eccfp);

    if (pstate->flags & (CESF_DATA_IS_BAD | CESF_INST_IS_BAD))
	    return(0); /* cannot simulate */

    bad_addr = (paddr_t) eccfp[ECCF_PADDR];

    ASSERT(epc == eccfp[ECCF_ERROREPC]);

    /* get physical address of epc */
    if (! (pstate->flags & CESF_EPC_PADDR_VALID)) {
	return(PERR_STATISTICS_CODE(PE_LDST_NOPHYS1));
    }
    phys_epc = pstate->epc_paddr;
    cached = (pstate->flags & CESF_EPC_CACHED);
    if (! (pstate->flags & CESF_INST_VALID)) {
	return(PERR_STATISTICS_CODE(PE_LDST_INST));
    }
    inst = pstate->inst;

    /* The BD bit in the cause register appears to be unreliable
     * in this context.  Emulate the branch instruction if
     * necessary.
     */
    if (pstate->flags & CESF_IS_BRANCH) {
	PERR_STATISTICS(PE_LDST_BRANCH);

	if (! (pstate->flags & CESF_BD_PADDR_VALID)) {
	    return(PERR_STATISTICS_CODE(PE_LDST_EOP));
	}

	br_inst = inst;

	epc += sizeof(inst_t);
	phys_epc = pstate->bd_paddr;
	inst = (inst_t) pstate->bd_inst;

	/* Don't bother with branch instructions in bd slots */
	if (is_branch(inst)) {
	    return(PERR_STATISTICS_CODE(PE_LDST_BRBR));
	}
    }

    /* if ef_epc is not a branch
     * 	    ef_epc points to the real error epc
     * 	    epc points to the real error epc
     * 	    phys_epc points to the real error epc
     *      inst is the real instruction
     *
     * 	    br_inst = 0
     *
     * if ef_epc is a branch
     *	    ef_epc points to the branch epc
     *	    epc points to the delay slot pc
     * 	    phys_epc points to the delay slot pc
     *      inst contains the branch delay instruction
     *
     *      br_inst contains the branch instruction
     */

    /* Check to see if error was generated by an ifetch.
     * Let the clean_page_recover handler fix clean text
     * pages.
     */
    if (ecc_same_cache_block(CACH_PI,bad_addr,phys_epc)) {
	PERR_STATISTICS(PE_LDST_INST);

	/* If there is ever shared text and data on the same 
	 * cache line, just give up.
	 */
	if (eccfp[ECCF_CACHE_ERR] & CACHERR_ER) {
	    return(PERR_STATISTICS_CODE(PE_LDST_INSTDATA));
	}
    }

    /* Must be a data reference 
     */
    else {
	int ldst;
	k_machreg_t vaddr;
	int width; 
	paddr_t tpaddr;
	paddr_t _tpaddr;
    	union mips_instruction ldst_inst;

	PERR_STATISTICS(PE_LDST_DATA);

	ASSERT(eccfp[ECCF_CACHE_ERR] & CACHERR_ER);

	/* decode_inst() records whether the instruction is a 
	 * load or store, the virtual address it accesses, and
	 * the width of the access.  It fails if the instruction
	 * is one we can't emulate, and records why it could not
	 * be emulated.
	 */
	if (epc == pstate->epc &&
	    pstate->flags & CESF_INST_VALID) {
		if (! (pstate->flags & CESF_INST_LDST)) {
			return(PERR_STATISTICS_CODE(PE_LDST_DECODE));
		}
		ldst = pstate->epc_dest_type;
		vaddr = pstate->epc_dest;
		width = pstate->epc_dest_width;
		if (! (pstate->flags & CESF_DEST_PADDR_VALID)) {
			return(PERR_STATISTICS_CODE(PE_LDST_NOPHYS2));
		}
		tpaddr = pstate->epc_dest_paddr;
		cached = (pstate->flags & CESF_DEST_CACHED);
	} else if (pstate->flags & CESF_IS_BRANCH &&
		   epc == (pstate->epc + sizeof(inst_t)) &&
		   pstate->flags & CESF_BD_INST_VALID) {
		if (! (pstate->flags & CESF_BD_INST_LDST)) {
			return(PERR_STATISTICS_CODE(PE_LDST_DECODE));
		}
		ldst = pstate->bd_dest_type;
		vaddr = pstate->bd_dest;
		width = pstate->bd_dest_width;
		if (! (pstate->flags & CESF_BD_DEST_PADDR_VALID)) {
			return(PERR_STATISTICS_CODE(PE_LDST_NOPHYS2));
		}
		tpaddr = pstate->bd_dest_paddr;
		cached = pstate->flags & CESF_BD_DEST_CACHED;
	} else {
		if (!decode_inst(ep, inst, &ldst, &vaddr, &width)) {
			return(PERR_STATISTICS_CODE(PE_LDST_DECODE));
		}

		/* Get physical address mapped by vaddr.  
		 */
		if (!tlb_to_phys(vaddr, &tpaddr, &cached)) {
			return(PERR_STATISTICS_CODE(PE_LDST_NOPHYS2));
		}
	}

	if (! ecc_same_cache_block(CACH_SD,tpaddr,bad_addr)) {
	    /* Something went wrong.  The error address does not match
	     * either the instruction or the address referenced by
	     * the instruction.
	     */
	    return(PERR_STATISTICS_CODE(PE_LDST_LOST));
	}

	/* The physical address referenced by the instruction, tpaddr,
	 * is on the same cache line where the parity error was detected.
	 */
	_tpaddr = tpaddr;

	/* See if the tpaddr actually hits the parity error address
	 */
	/* unaligned load/store need to be handled differently */
	ldst_inst.word = inst;
	if (ldst_inst.i_format.opcode == swr_op ||
	    ldst_inst.i_format.opcode == lwr_op)
		tpaddr &= ~0x3;
	if (ldst_inst.i_format.opcode == sdr_op ||
	    ldst_inst.i_format.opcode == ldr_op)
		tpaddr &= ~0x7;

	if (bad_addr >= tpaddr && bad_addr < tpaddr+width) {

	    /* Detected a parity error on the referenced address.  
	     */
		 
	    /* If instruction is a load, we can't recover
	     * from this with the load/store method.
	     */
	    if (ldst == LOAD_INSTR) {
		return(PERR_STATISTICS_CODE(PE_LDST_HIT_LD));
	    } else {
		PERR_STATISTICS(PE_LDST_HIT_ST);
		p_corrected = 1;
	    }

	} else {

	    /* Detected a parity error on the cacheline where
	     * the referenced address lives.  
	     */

	    /* These are the ones we need to watch.  If a process
	     * continually touches an address on a bad cache line,
	     * performance will go to hell.
	     */
		 
	    PERR_STATISTICS((ldst == LOAD_INSTR) \
				    ? PE_LDST_MISS_LD \
				    : PE_LDST_MISS_ST);
	}

	/* Emulate load instructions that do not directly hit
	 * the bad address and all store instructions by accessing
	 * the address uncached.
	 */
	tpaddr = _tpaddr;

	if (IS_KUSEG(ep->ef_epc)) {
	    /* emulate for user process */
	    if (datafix(ep, inst, br_inst, 1, vaddr, tpaddr)) {
		PERR_STATISTICS_CODE(PE_LDST_USER_FIXED);
		return 1;
	    }
	    PERR_STATISTICS_CODE(PE_LDST_USER_NOTFIXED);
	} else {
	    /* emulate for kernel */
	    if (datafix(ep, inst, br_inst, 0, vaddr, tpaddr)) {
		PERR_STATISTICS_CODE(PE_LDST_KERN_FIXED);
		return 1;
	    }
	    PERR_STATISTICS_CODE(PE_LDST_KERN_NOTFIXED);
	}
    }
    p_corrected = 0;
    return 0;
}

#define	EFRAME_REG(efp,reg)	(((k_machreg_t *)(efp))[reg])
#define REGVAL(efp,x)       ((x)?EFRAME_REG((efp),(x)+EF_AT-1):0)

/* Decode enough of an instruction to determine the address it
 * references, the width of the access, and whether it's a load
 * or a store.
 *
 * Return 1 if instruction is decoded and one we can emulate, 0 if not.
 */
int
decode_inst(eframe_t *ep, int instruction, int *ldst, k_machreg_t *vaddr, int *width)
{
    union mips_instruction inst;
    k_machreg_t addr;

    inst.word = (unsigned) instruction;
    addr = REGVAL(ep, inst.i_format.rs) +
	    inst.i_format.simmediate;

    switch (inst.i_format.opcode) {
    /* Loads */
    case ld_op:
	*width = 8;
	*ldst = LOAD_INSTR;
	break;
    case lwu_op:
	*width = 4;
	*ldst = LOAD_INSTR;
	break;
    case lw_op:
	*width = 4;
	*ldst = LOAD_INSTR;
	break;
    case lhu_op:
    case lh_op:
	*width = 2;
	*ldst = LOAD_INSTR;
	break;
    case lbu_op:
    case lb_op:
	*width = 1;
	*ldst = LOAD_INSTR;
	break;
    /* Stores */
    case sd_op:
	*width = 8;
	*ldst = STORE_INSTR;
	break;
    case sw_op:
	*width = 4;
	*ldst = STORE_INSTR;
	break;
    case sh_op:
	*width = 2;
	*ldst = STORE_INSTR;
	break;
    case sb_op:
	*width = 1;
	*ldst = STORE_INSTR;
	break;

    /* Cop1 instructions */
    case lwc1_op:
	*width = 4;
	*ldst = LOAD_INSTR;
	break;
    case ldc1_op:
	*width = 8;
	*ldst = LOAD_INSTR;
	break;
#if MIPS4_ISA
    case lwxc1_op:
	*width = 4;
	*ldst = LOAD_INSTR;
	addr = REGVAL(ep, inst.r_format.rs) + REGVAL(ep, inst.r_format.rt);
	break;
    case ldxc1_op:
	*width = 8;
	*ldst = LOAD_INSTR;
	addr = REGVAL(ep, inst.r_format.rs) + REGVAL(ep, inst.r_format.rt);
	break;
#endif /* MIPS4_ISA */
    case swc1_op:
	*width = 4;
	*ldst = STORE_INSTR;
	break;
    case sdc1_op:
	*width = 8;
	*ldst = STORE_INSTR;
	break;
#ifdef MIPS4_ISA
    case swxc1_op:
	*width = 4;
	*ldst = STORE_INSTR;
	addr = REGVAL(ep, inst.r_format.rs) + REGVAL(ep, inst.r_format.rt);
	break;
    case sdxc1_op:
	*width = 8;
	*ldst = STORE_INSTR;
	addr = REGVAL(ep, inst.r_format.rs) + REGVAL(ep, inst.r_format.rt);
	break;
#endif /* MIPS4_ISA */

    /* Unaligned load/stores */
    case ldl_op:
	*width = 8 - (*vaddr & 0x7);
	*ldst = LOAD_INSTR;
	break;
    case ldr_op:
	*width = (*vaddr & 0x7) + 1;
	*ldst = LOAD_INSTR;
	break;
    case lwl_op:
	*width = 4 - (*vaddr & 0x3);
	*ldst = LOAD_INSTR;
	break;
    case lwr_op:
	*width = (*vaddr & 0x3) + 1;
	*ldst = LOAD_INSTR;
	break;
    case sdl_op:
	*width = 8 - (*vaddr & 0x7);
	*ldst = STORE_INSTR;
	break;
    case sdr_op:
	*width = (*vaddr & 0x7) + 1;
	*ldst = STORE_INSTR;
	break;
    case swl_op:
	*width = 4 - (*vaddr & 0x3);
	*ldst = STORE_INSTR;
	break;
    case swr_op:
	*width = (*vaddr & 0x3) + 1;
	*ldst = STORE_INSTR;
	break;

    /* Load linked/store conditional */
    case lld_op:
    case scd_op:
    case ll_op:
    case sc_op:

    default:
	if (! EXCEPTION_MODE(get_sr())) {
		PERR_STATISTICS_CODE(PE_DEC_UNKNOWN);
	}
	return 0;
    }

    *vaddr = addr;
    return 1;
}

/*
 * datafix() is a ripoff of fixade() used to emulate load and store
 * instructions that access addresses to cachelines containing
 * parity errors.  It moves the data *uncached* to avoid the parity
 * error.
 *
 * XXX One side effect is that the addresses are currently allowed to
 * be unaligned (could check "pp->p_flag2 & SFIXADE" before 
 * allowing unaligned accesses).
 *
 * datafix() returns 1 if the data has been fixed, otherwise 0.
 *
 * It modifies the destination register (general or floating-point)
 * for loads or the destination memory location for stores.  Also the
 * epc is advanced past the instruction (possibly to the target of a 
 * branch).
 *
 * ep 		- pointer to eframe, needed to update epc if instruction
 *			is in a bdslot.
 * instruction	- the instruction where the error occurred
 * br_inst	- if the bad instruction is in a branch delay slot, this 
 *			is the preceding branch instruction.
 *			Zero if instruction is not in a bdslot.
 * mode		- kernel = 0, user = 1
 */

extern int u_ustore_double(caddr_t,k_machreg_t);
extern int u_ustore_word(caddr_t,k_machreg_t);
extern int u_ustore_half(caddr_t,k_machreg_t);
extern int u_ustore_double_left(caddr_t,k_machreg_t);
extern int u_ustore_double_right(caddr_t,k_machreg_t);
extern int u_ustore_word_left(caddr_t,k_machreg_t);
extern int u_ustore_word_right(caddr_t,k_machreg_t);
extern int u_uload_half(caddr_t, k_machreg_t *);
extern int u_uload_uhalf(caddr_t, k_machreg_t *);
extern int u_uload_word(caddr_t, k_machreg_t *);
extern int u_uload_uword(caddr_t, k_machreg_t *);
extern int u_uload_word_left(caddr_t,k_machreg_t *,k_machreg_t);
extern int u_uload_word_right(caddr_t,k_machreg_t *,k_machreg_t);
extern int u_uload_double(caddr_t,k_machreg_t *);
extern int u_uload_double_left(caddr_t,k_machreg_t *,k_machreg_t);
extern int u_uload_double_right(caddr_t,k_machreg_t *,k_machreg_t);



static int u_ustore_byte(caddr_t addr, k_machreg_t val)
{
	*(volatile u_char *)addr = val;
	return(0);
}


static int
datafix(eframe_t *ep, inst_t instruction, inst_t br_inst, int mode,
	k_machreg_t vaddr, paddr_t addr)
{
    k_machreg_t *efr = (k_machreg_t *)ep;
    union mips_instruction inst, branch_inst;
    k_machreg_t word;
    k_machreg_t new_epc;
    int error;
    caddr_t mapped_bad_addr;
    int	was_enabled;
    k_machreg_t c0_status;
    int (*st_fnp)(caddr_t,k_machreg_t);
    int (*ld_fnp)(caddr_t,k_machreg_t *,k_machreg_t);

    if (br_inst) {
	    int branch_taken;

	    ASSERT(is_branch(br_inst));
	    branch_inst.word = (unsigned) br_inst;
	    inst.word = (unsigned) instruction;

    	    new_epc = emulate_branch(ep, branch_inst.word, get_fpc_csr(),
		&branch_taken);
	    if (new_epc == -1) {
		    return(PERR_STATISTICS_CODE(PE_FIX_BRFAIL));
	    }

	    /* ignore branch delay slot on branch likely type instructions if
	     * branch is not taken */
	    if (!branch_taken) {
		branch_inst.word = (unsigned) br_inst;
		if (branch_inst.i_format.opcode == beql_op
		    || branch_inst.i_format.opcode == bnel_op
		    || branch_inst.i_format.opcode == blezl_op
		    || branch_inst.i_format.opcode == bgtzl_op)
		  return 1;

		if (branch_inst.i_format.opcode == bcond_op) {
		    if (branch_inst.r_format.rt == bltzall_op
			|| branch_inst.r_format.rt == bltzl_op
			|| branch_inst.r_format.rt == bgezall_op
			|| branch_inst.r_format.rt == bgezl_op)
		      return 1;
		}
	    }
    } else {
	    inst.word = (unsigned) instruction;
	    new_epc = (k_machreg_t)(ep->ef_epc+4);
    }

#if MIPS4_ISA
    if (inst.r_format.opcode == cop1x_op) 
    {
	/* Hack alert: we can convert the indexed LD/ST operations to regular
	 * old LD/ST operations since we already have the addr from 
	 * decode_inst(), (in other words, i_format.rs and i_format.simmediate
	 * aren't examined below).  Then we emulate the new instruction,
	 * as usual.
	 */
	switch(inst.r_format.func)
	{
	case lwxc1_op:
	    inst.i_format.opcode = lwc1_op;
	    break;
	case ldxc1_op:
	    inst.i_format.opcode = ldc1_op;
	    break;
	case swxc1_op:
	    inst.i_format.opcode = swc1_op;
	    break;
	case sdxc1_op:
	    inst.i_format.opcode = sdc1_op;
	    break;
	default:
	    return(PERR_STATISTICS(PE_FIX_UNKNOWN));
	}
	inst.i_format.rt = inst.r_format.re;
    }
    else
#endif /* MIPS4_ISA */
    if (vaddr != 
	(REGVAL(ep, inst.i_format.rs) + inst.i_format.simmediate)) {
	    return(PERR_STATISTICS_CODE(PE_FIX_DECODE));
    }

    /* Get the physical address of the location
     */

    /* User access */
    if (mode == 1) {
	/*
	 * The addresses of both the left and right parts of the reference
	 * have to be checked.  If either is a kernel address it is an
	 * illegal reference.
	 */
	if (vaddr >= K0BASE || vaddr+3 >= K0BASE) {
		return(PERR_STATISTICS_CODE(PE_FIX_ILLADDR));
	}
    }

    mapped_bad_addr = map_physaddr(addr);
    was_enabled = is_sysad_parity_enabled();
    if (was_enabled)
	    disable_sysad_parity();
    
    error = 0;

    switch (inst.i_format.opcode) {
    case ld_op:
	    error = u_uload_double(mapped_bad_addr, &word);
	    goto do_assign_register;

    case lwu_op:
	    error = u_uload_uword(mapped_bad_addr, &word);
	    goto do_assign_register;

    case lw_op:
	    error = u_uload_word(mapped_bad_addr, &word);
do_assign_register:
	    if(inst.i_format.rt == 0)
		    break;
	    efr[inst.i_format.rt+3] = word;
	    break;

    case lh_op:
	    error = u_uload_half(mapped_bad_addr, &word);
	    goto do_assign_register;

    case lhu_op:
	    error = u_uload_uhalf(mapped_bad_addr, &word);
	    goto do_assign_register;

    case lb_op:
	    word = *(volatile signed char *)mapped_bad_addr;
	    goto do_assign_register;

    case lbu_op:
	    word = *(volatile unsigned char *)mapped_bad_addr;
	    goto do_assign_register;

    case lwc1_op:
	    c0_status = getsr();
	    setsr(c0_status | SR_CU1 | (ep->ef_sr & SR_FR));
	    emulate_lwc1(inst.i_format.rt, mapped_bad_addr);
	    setsr(c0_status);
	    break;
    case ldc1_op:
	    c0_status = getsr();
	    setsr(c0_status | SR_CU1 | (ep->ef_sr & SR_FR));
	    emulate_ldc1(inst.i_format.rt, mapped_bad_addr);
	    setsr(c0_status);
	    break;

    case sd_op:
	    st_fnp = u_ustore_double;
	    goto do_store_op;

    case sw_op:
	    st_fnp = u_ustore_word;
do_store_op:
	    error = (*st_fnp)(mapped_bad_addr, 
				REGVAL(ep, inst.i_format.rt));
	    break;

    case sh_op:
	    st_fnp = u_ustore_half;
	    goto do_store_op;

    case sb_op:
	    st_fnp = u_ustore_byte;
	    goto do_store_op;

    case swc1_op:
	    c0_status = getsr();
	    setsr(c0_status | SR_CU1 | (ep->ef_sr & SR_FR));
	    emulate_swc1(inst.i_format.rt, mapped_bad_addr);
	    setsr(c0_status);
	    break;
    case sdc1_op:
	    c0_status = getsr();
	    setsr(c0_status | SR_CU1 | (ep->ef_sr & SR_FR));
	    emulate_sdc1(inst.i_format.rt, mapped_bad_addr);
	    setsr(c0_status);
	    break;

    /* Unaligned load/stores */
    case ldl_op:
	    ld_fnp = u_uload_double_left;
	    goto do_load_op;

    case ldr_op:
	    ld_fnp = u_uload_double_right;
	    goto do_load_op;

    case lwl_op:
	    ld_fnp = u_uload_word_left;
do_load_op:
	    error = (*ld_fnp)(mapped_bad_addr, &word,
		REGVAL(ep, inst.i_format.rt));
	    goto do_assign_register;

    case lwr_op:
	    ld_fnp = u_uload_word_right;
	    goto do_load_op;

    case sdl_op:
	    st_fnp = u_ustore_double_left;
	    goto do_store_op;

    case sdr_op:
	    st_fnp = u_ustore_double_right;
	    goto do_store_op;

    case swl_op:
	    st_fnp = u_ustore_word_left;
	    goto do_store_op;

    case swr_op:
	    st_fnp = u_ustore_word_right;
	    goto do_store_op;

    /* Load linked/store conditional */
    case lld_op:
    case scd_op:
    case ll_op:
    case sc_op:

    default:
	    PERR_STATISTICS(PE_FIX_UNKNOWN);
	    error = 1;
	    break;
    }

    unmap_physaddr(mapped_bad_addr);
    if (was_enabled)
	    enable_sysad_parity();
    /* clear any pending error due to the emulation */
    *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
    flushbus();

    if (error) {
	return(PERR_STATISTICS_CODE(PE_FIX_DECODE));
    }

    ep->ef_epc = new_epc;
    return 1;
}

/* tlb_to_phys
 * 
 * probe tlb for virtual address vaddr, returning its physical
 * address and whether it is cached or not.
 *
 * XXX check tlbvalid bit?
 *
 * return 1 if found in tlb, otherwise 0.
 */
extern int tlbgetlo(int,caddr_t);

int
tlb_to_phys(k_machreg_t vaddr, paddr_t *paddr, int *cached)
{
	int tlblo;
	uint pfn;
	int	pid;

	/* kernel direct */
	if (IS_KSEGDM(vaddr)) {
		*paddr = KDM_TO_PHYS(vaddr);
		*cached = IS_KSEG0(vaddr) ? 1 : 0;
		return 1;
	} else if (IS_KSEG2(vaddr)) /* kernel mapped */

		pid = 0;
	else			/* user mapped */
		pid = tlbgetpid();

	if ((tlblo = tlbgetlo(pid, (caddr_t) vaddr)) == -1)
		return 0;

	pfn = (tlblo & TLBLO_PFNMASK) >> TLBLO_PFNSHIFT;
	pfn >>= PGSHFTFCTR;
	*paddr = (paddr_t) (ctob(pfn) + poff(vaddr));
	*cached = ((((tlblo & TLBLO_CACHMASK) >> TLBLO_CACHSHIFT) == 
			TLBLO_UNCACHED) ? 0 : 1);
	return 1;
}

/* Enable/disable SysAD parity checking through MC.
 *
 * The "*_erl" versions are to be used (and must be used) at "SR_ERL" 
 * (cache error exception) level.  On some processors, SR_ERL cannot
 * be reliably changed via mtc0 to $sr, as would happen in a splx() call.
 * Also, spl7() does not protect against cache error exceptions.
 * The cache error exception handler must leave SysAD parity checking in
 * the state in which it found it.
 */
int
is_sysad_parity_enabled(void)
{
    return (ces_data_p != NULL &&
	    sysad_parity_enabled);
}

#ifdef NOTDEF
int
is_sysad_parity_enabled_erl(void)
{
    volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);
    
    return((*cpuctrl0 & CPUCTRL0_R4K_CHK_PAR_N) == 0);
}
#endif	/* NOTDEF */

void
enable_sysad_parity(void)
{
    int s = spl7();
    volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);
    *cpuctrl0 &= ~CPUCTRL0_R4K_CHK_PAR_N;
    flushbus();
    if (ces_data_p != NULL)
	    sysad_parity_enabled = 1;
    splx(s);
}

void
enable_sysad_parity_erl(void)
{
    volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);

    *cpuctrl0 &= ~CPUCTRL0_R4K_CHK_PAR_N;
    flushbus();
}

void
disable_sysad_parity(void)
{
    int s = spl7();
    volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);
    *cpuctrl0 |= CPUCTRL0_R4K_CHK_PAR_N;
    flushbus();
    if (ces_data_p != NULL)
	    sysad_parity_enabled = 0;
    splx(s);
}

void
disable_sysad_parity_erl(void)
{
    volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);
    *cpuctrl0 |= CPUCTRL0_R4K_CHK_PAR_N;
    flushbus();
}

/* return 1 if svideo is installed, else 0 */
int
svideo_probe(void)
{
#ifdef IP20
    int value;

    if (badaddr_val((void *)PHYS_TO_K1(0x1f3e0000), 4, &value) || value != 0x61)
	return 0;
    else
	return 1;
#else
    return 0;
#endif
}

/* return 1 if bit3 bus converter board is installed, else 0 */
int
bit3_probe(void)
{
    int value;
    static __psint_t bit3_addr_table[] = {
#ifndef IP20
	    PHYS_TO_K1(0x1f000000),
#endif
	    PHYS_TO_K1(0x1f400000),
	    PHYS_TO_K1(0x1f600000),
	    0 };
    __psint_t *ap;

    /* The gio id of the bit3 board is 0x78
     * I assume that the first test is something weird that bit3 does
     * on Indy.
     * NOTE: This only takes into account 2 gio slots, so CHALLENGE S
     * may have problems sometime in the future.
     */
    for (ap = bit3_addr_table;
	 *ap != 0;
	 ap++) {
	    if (!badaddr_val((void *)(*ap), 4, &value) && 
		(value&0xff) == 0x78)
		    return 1;
    }

    return 0;
}

static uint vparity[1024];
static int vparity_valid;
extern uint _ftext[];
extern uint _etext[];

/*
 * calculate vertical parity over the text section of the kernel
 * assuming the kernel is 4MB max and the vertical parity covers each 4K page
 */
void
calculate_vparity(void)
{
	register uint *first, *last, parity;
	register uint *etext = _etext;
	int i = 0;

	if (((__psunsigned_t)etext - (__psunsigned_t)_ftext) > 0x400000)
		return;

	first = _ftext;

	while (first < etext) {
		last = (uint *)(((__psunsigned_t)first & ~0xfff) + 0x1000);
		if (last > etext)
			last = etext;

		for (parity = 0; first < last; first++)
			parity ^= *first;
		vparity[i] = parity;
		i++;
	}

	/* set flag to indicate vparity[] is valid */
	vparity_valid = 1;
}

/* Attempt to recover from a parity error in the kernel 
 * text segment.
 *
 * Called to fix single bit errors only.
 * The real address with the error (not doubleword/bytelane) is 
 *	provided by phys_err_addr.
 *
 * Return 1 if error is recovered, 0 if not recovered.
 */
int
ktext_recover(paddr_t phys_err_addr)
{
	uint *first;
	uint *last;
	int index;
	uint parity;
	volatile uint *badaddr;
	int par_enabled;

	if (! (phys_err_addr >= KDM_TO_PHYS((__psunsigned_t)_ftext) &&
	       phys_err_addr < KDM_TO_PHYS((__psunsigned_t)_etext)))
		return(0);

	if (!vparity_valid) {		/* vparity[] not set up yet */
		return(PERR_STATISTICS_CODE(PE_KTEXT_INVALID));
	}

	par_enabled = is_sysad_parity_enabled();
	if (par_enabled)
	    disable_sysad_parity();
	    
	/* get the *good* word within the double word with bad parity */
	badaddr = (uint *)PHYS_TO_K1((phys_err_addr & ~0x3) ^ 4);
	parity = *badaddr;
	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	flushbus();

	if (par_enabled)
	    enable_sysad_parity();

	badaddr = (uint *)PHYS_TO_K0(phys_err_addr & ~0x7);
	index = ((__psunsigned_t)badaddr >> 12)-((__psunsigned_t)_ftext >> 12);

	first = (uint *)PHYS_TO_K1(phys_err_addr & ~0xfff);
	if (first < (uint *)K0_TO_K1((__psunsigned_t)_ftext))
		first = (uint *)K0_TO_K1((__psunsigned_t)_ftext);
	last = (uint *)PHYS_TO_K1((phys_err_addr & ~0xfff) + 0x1000);
	if (last > (uint *)K0_TO_K1((__psunsigned_t)_etext))
		last = (uint *)K0_TO_K1((__psunsigned_t)_etext);

	badaddr = (uint *)PHYS_TO_K1(phys_err_addr & ~0x7);
	while (first < badaddr)
		parity ^= *first++;
	first++;
	first++;
	while (first < last)
		parity ^= *first++;

        if (*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT)) {
                *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
                flushbus();
		return(PERR_STATISTICS_CODE(PE_KTEXT_MULTIPLE));
        }

	badaddr = (uint *)PHYS_TO_K1(phys_err_addr & ~0x3);
	*badaddr = parity ^ vparity[index];

	return 1;
}
 
#endif /* _MEM_PARITY_WAR */

/* 
 * Functions to map and unmap physical addresses 
 * Don't worry about page color -- we're accessing the addresses uncached.
 *
 * Note that these bad_addr_pages are unprotected globals.
 */

int *bad_addr_pages;

/* Map a physical address into an uncached kernel virtual address.  */
caddr_t
map_physaddr(paddr_t paddr)
{
    caddr_t kvaddr;

#if !IPMHSIM
#ifdef IP22
    extern pfn_t low_maxclick;
    if (paddr < ctob(low_maxclick))	/* no need to map */
#else
    if (paddr < SEG1_BASE)		/* no need to map */
#endif	/* IP22 */
	return((caddr_t) PHYS_TO_K1(paddr));

    if (!bad_addr_pages)
	bad_addr_pages = kvalloc(cachecolormask+1, VM_VACOLOR|VM_UNCACHED, 0);

    kvaddr = (caddr_t)bad_addr_pages + (NBPP * colorof(paddr)) + poff(paddr);

    pg_setpgi(kvtokptbl(kvaddr),
	      mkpde(PG_VR|PG_G|PG_SV|PG_M|PG_N, pnum(paddr)));
    tlbdropin(0, kvaddr, kvtokptbl(kvaddr)->pte);

    return kvaddr;
#else
    return((caddr_t) PHYS_TO_K1(paddr));
#endif
}

/* Unmap a physical address. */
void
unmap_physaddr(caddr_t kvaddr)
{
    if (IS_KSEG1(kvaddr)) 		/* no need to unmap */
	return;

    unmaptlb(0, btoct((unsigned long) kvaddr));
    pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0));

    return;
}

#ifdef _MEM_PARITY_WAR

/* These are still currently needed in ml/usercopy.s */
ulong kv_initial_from;
ulong kv_initial_to;
ulong initial_count;

static uint bad_data_hi, bad_data_lo;
#endif /* _MEM_PARITY_WAR */

/* a global so wd93dma_flush can use same message */
char *dma_par_msg = "DMA Parity Error detected in Memory\n\tat Physical Address 0x%x\n";

/*
 * fix bad parity by writing to the locations at fault, kv_addr must be
 * an uncached kernel virtual address (K1 or K2)
 *
 * Note that this blows away the old data.
 */
void
fix_bad_parity(volatile uint *kv_addr)
{
    kv_addr = (volatile uint *)(((__psunsigned_t)kv_addr) & ~0x7);
    *kv_addr = 0xe0e0e0e0;
    *(kv_addr + 1) = 0xe0e0e0e0;

    flushbus();
}

void
use_old_mem_parity_patch(void)
{
	no_parity_workaround = 1;
	disable_sysad_parity();
}

/* This is the infamous dwong patch that recovers from parity
 * errors to target addresses in bcopy and bzero.  It is lifted
 * in its entirety from ml/{IP20,IP22}.c.
 *
 * Return 1 if successful, otherwise 0.
 */
int
dwong_patch(eframe_t *ep)
{
	caddr_t mapped_bad_addr;
	register unsigned int i;

	/* The load/store hack should recover from anything that
	 * this guy could handle (and more!).  Leave it in for 
	 * sorry systems that don't have SysAD parity enabled.
	 */
	ASSERT(!is_sysad_parity_enabled());

	mapped_bad_addr = map_physaddr((paddr_t) (cpu_par_adr & ~0x7));

	/* This should always be true -- we check it higher up */
	if ((cpu_err_stat & CPU_ERR_STAT_RD_PAR) == CPU_ERR_STAT_RD_PAR) {
		extern void swbcopy();

		static ulong bzero_start = (ulong)bzero;
		static ulong bzero_end = (ulong)bcmp;
		static ulong bcopy_start = (ulong)bcopy;
		static ulong bcopy_end = (ulong)swbcopy;
		static int par_err_count = 0;
		ulong kv_final_to = kv_initial_to + initial_count - 1;

		ulong bad_phys_hi;
		ulong bad_phys_lo;
		ulong kv_to;
		ulong phys_to;

		par_err_count++;

		/*
		 * must be executing inside bcopy/bzero when the bus error
		 * was taken
		 */
		if (((ulong) ep->ef_epc) >= bzero_start && 
		    ((ulong) ep->ef_epc) < bzero_end)
			kv_to = ep->ef_a0;
		else if (((ulong) ep->ef_epc) >= bcopy_start && 
			 ((ulong) ep->ef_epc) < bcopy_end) {
			/* no workaround if buffers overlapped */
			if (kv_initial_to >= kv_initial_from
			    && kv_initial_to < kv_initial_from + initial_count)
				goto no_workaround;
			if (kv_initial_from >= kv_initial_to
			    && kv_initial_from < kv_initial_to + initial_count)
				goto no_workaround;

			kv_to = ep->ef_a1;
		}
		else
			goto no_workaround;

		/*
		 * address not in target range or fault on a K1 address
		 * MUST NOT USE THE WORKAROUND ON UNCACHED ACCESS
		 */
		if (kv_to < kv_initial_to || kv_to > kv_final_to
		    || IS_KSEG1(kv_to))
			goto no_workaround;

		/* get the address with the parity error down to the byte */
		for (i = 0; i < 8; i++) {
			if (cpu_err_stat & (ERR_STAT_B0 << i)) {
				bad_phys_hi = (cpu_par_adr & ~0x7) + (0x7 - i);
				break;
			}
		}
		
		for (i = 0; i < 8; i++) {
			if (cpu_err_stat & (ERR_STAT_B7 >> i)) {
				bad_phys_lo = (cpu_par_adr & ~0x7) + i;
				break;
			}
		}

		/* convert virtual address to physical address */
		if (IS_KUSEG(kv_to)) {
			/* 
		 	 * the following code to convert KUSEG address to
			 * physical address is modified from the dovflush()
			 * routine in os/machdep.c
		 	 */
			pde_t *pd;
			preg_t *prp;
			vasid_t vasid;
			pas_t *pas;
			ppas_t *ppas;

			as_lookup_current(&vasid);
			pas = VASID_TO_PAS(vasid);
			ppas = (ppas_t *)vasid.vas_pasid;
			if (!mrtryaccess(&pas->pas_aspacelock))
				goto no_workaround;
			if ((prp = findpreg(pas, ppas, (caddr_t)kv_to)) == NULL) {
				mrunlock(&pas->pas_aspacelock);
				goto no_workaround;
			}
			pd = pmap_pte(pas, prp->p_pmap, (caddr_t)kv_to, VM_NOSLEEP);
			mrunlock(&pas->pas_aspacelock);
			if (pd == NULL || !pg_isvalid(pd) || pg_isnoncache(pd)) {
				goto no_workaround;
			}

			phys_to = ctob(pd->pte.pg_pfn) | poff(kv_to);
		}
		else if (IS_KSEG0(kv_to)) {
			phys_to = K0_TO_PHYS(kv_to);
		}
		else {
			pde_t *pd;

			pd = kvtokptbl(kv_to);
			if (!pg_isvalid(pd) || pg_isnoncache(pd))
				goto no_workaround;
			phys_to = ctob(pd->pte.pg_pfn) | poff(kv_to);
		}

		/* highly unlikely, just a sanity check */
		if ((pnum(phys_to) != pnum(bad_phys_hi)) && !IS_KSEG0(kv_to))
			goto no_workaround;

		/* check for out of range */
		if (pnum(kv_to) == pnum(kv_final_to)
		    && poff(bad_phys_hi) > poff(kv_final_to))
			goto no_workaround;

		if (!(bad_phys_hi >= phys_to
		     || (kv_to - kv_initial_to >= phys_to - bad_phys_hi)))
			goto no_workaround;

		if (!(bad_phys_lo >= phys_to
		     || (kv_to - kv_initial_to >= phys_to - bad_phys_lo)))
			goto no_workaround;

		bad_data_hi = *(volatile uint *)((ulong)mapped_bad_addr & ~0x7);
		bad_data_lo = *(volatile uint *)(((ulong)mapped_bad_addr & ~0x7) + 4);
		cmn_err(CE_CONT, "!Recoverable parity error at or near physical address 0x%x <0x%x>, Data: 0x%x/0x%x\n",
			cpu_par_adr, cpu_err_stat, bad_data_hi, bad_data_lo);
#ifdef DEBUG
		cmn_err(CE_CONT, "Recoverable parity error at or near virtual address 0x%x\n", kv_to);
#endif	/* DEBUG */
		log_perr(cpu_par_adr, cpu_err_stat, 1, 1);
		fix_bad_parity((volatile uint *)mapped_bad_addr);

		unmap_physaddr(mapped_bad_addr);
		return 1;	/* ignore the error */
	}

no_workaround:
	unmap_physaddr(mapped_bad_addr);
	return 0;
}

#ifdef _MEM_PARITY_WAR
#ifdef DEBUG

extern void dprintf(char *, ...);
#define printf dprintf
static void
ces_print_element(char *n, volatile struct ces_s *cp)
{
	printf ("%s->count = %d\n", n, cp->count);
	printf ("%s->cache_err = 0x%x\n", n, cp->cache_err);
	printf ("%s->epc = 0x%x\n", n, cp->epc);
	printf ("%s->sr = 0x%x\n", n, cp->sr);
	printf ("%s->cause = 0x%x\n", n, cp->cause);
	printf ("%s->efp = 0x%x\n", n, cp->efp);
}

void
ces_print(void)
{
    DECLARE_PSTATE;
    DECLARE_DBG_PSTATE;

    if (! INIT_PSTATE)
	    return;

    printf ("ces->cpu_status = 0x%x\n", pstate->cpu_status);
    printf ("ces->cpu_addr = 0x%x\n", pstate->cpu_addr);
    ces_print_element("ces",pstate);
    printf ("ces->flushaddr = 0x%x\n", pstate->flushaddr);
    printf ("ces->double_err = %d\n", pstate->double_err);
    printf ("ces->eccfp[CACHE_ERR] = 0x%x\n", pstate->eccfp[ECCF_CACHE_ERR]);
    printf ("ces->eccfp[TAGLO] = 0x%x\n", pstate->eccfp[ECCF_TAGLO]);
    printf ("ces->eccfp[ECC] = 0x%x\n", pstate->eccfp[ECCF_ECC]);
    printf ("ces->eccfp[ERROREPC] = 0x%x\n", pstate->eccfp[ECCF_ERROREPC]);
    printf ("ces->eccfp[PADDR] = 0x%x\n", pstate->eccfp[ECCF_PADDR]);

    if (pstate->double_err) {
	INIT_DBG_PSTATE;
	ces_print_element("double",dbg_pstate);
    }
}

void
log_print(void)
{
    struct perr_log_info_s *pei;	
    int	i;

    for (pei = (struct perr_log_info_s *)ces_data_p->perr_log_info, i = 0;
	 i < MAX_PERR_LOG;
	 pei++, i++) {
	if (!pei->time)
	    continue;
	printf ("[%d] paddr 0x%x time %d ep 0x%x\n", i,
	    pei->paddr,
	    pei->time,
	    pei->ep);
	printf ("    err_epc 0x%x epc 0x%x ra 0x%x\n",
	    (uint) pei->err_epc,
	    (uint) pei->epc,
	    (uint) pei->ra);
    }
}

static char *pe_print_names[] = {
    "PE_TOTAL",
    "PE_MULTIPLE",
    "PE_CPU_RECOV",
    "PE_CPU_NONRECOV",
    "PE_GIO_RECOV",
    "PE_GIO_NONRECOV",
    "PE_UNKNOWN",

    "PE_CR_TRANS",
    "PE_CR_KTEXT",
    "PE_CR_LDST",
    "PE_CR_CLEAN",
    "PE_CR_UNREC",

    "PE_KTEXT_INVALID",
    "PE_KTEXT_MULTIPLE",

    "PE_LDST_INST",
    "PE_LDST_DATA",
    "PE_LDST_INSTDATA",
    "PE_LDST_BRANCH",

    "PE_LDST_NOPHYS1",
    "PE_LDST_NOPHYS2",
    "PE_LDST_EOP",
    "PE_LDST_BRBR",
    "PE_LDST_DECODE",
    "PE_LDST_LOST",

    "PE_LDST_HIT_LD",
    "PE_LDST_HIT_ST",
    "PE_LDST_MISS_LD",
    "PE_LDST_MISS_ST",

    "PE_LDST_USER_FIXED",
    "PE_LDST_USER_NOTFIXED",

    "PE_LDST_KERN_FIXED",
    "PE_LDST_KERN_NOTFIXED",

    "PE_DEC_UNKNOWN",

    "PE_FIX_COP1BR",
    "PE_FIX_BRFAIL",
    "PE_FIX_ILLADDR",
    "PE_FIX_NOPHYS",
    "PE_FIX_UNKNOWN",
    "PE_FIX_DECODE",
    NULL
};


void
pe_stat_print(int log_too)
{
    int *pep;
    char **penp;

#define pe_print(x)  \
    if (pep[x]) \
    	printf ("%s = %d\n", #x, pep[x])

    if (ces_data_p == NULL)
	    return;

    for(pep = (int *)ces_data_p->ces_pe_statistics, penp = pe_print_names;
        *penp != NULL;
	pep++, penp++)
	    printf("%s = %d\n",*penp,*pep);

    ces_print();

    if (log_too)
	log_print();
}
#undef printf
#endif

#define VDMA_TO_MEM	VDMA_M_GTOH
#define VDMA_TO_GFX	0

#define DMA_FILL        1
#define DMA_COPY        2

#ifndef IPMHSIM
/*
 * args:
 * 	memaddr dest length COPY, or
 * 	memaddr filldata length FILL
 */
void
dma_start(void *maddr, void *daddr, int size, int type)
{
    unsigned int mode;

    ASSERT(IS_KUSEG(maddr));	/* physical address */
    ASSERT(type == DMA_FILL || type == DMA_COPY);
    if (type == DMA_COPY) {
	ASSERT(poff(daddr) == 0);	/* page aligned */
	ASSERT(IS_KUSEG(daddr));	/* physical address */
    }

    /* disable virtual address translation */
    *(volatile unsigned int *)PHYS_TO_K1(DMA_CTL) = 0;

    /* clear interrupts */
    *(volatile unsigned int *)PHYS_TO_K1(DMA_CAUSE) = 0;

    /* starting memory location */
    *(volatile unsigned int *)PHYS_TO_K1(DMA_MEMADRD) = KDM_TO_MCPHYS(maddr);

    /* long burst, incrementing, mode */
    mode = (VDMA_M_LBURST|VDMA_M_INCA) | 
	    (type == DMA_FILL ? (VDMA_TO_MEM|VDMA_M_FILL) : VDMA_TO_GFX);
    *(volatile unsigned int *)PHYS_TO_K1(DMA_MODE) = mode;

    /* lines * bytes per line */
    if (size >= 0x00010000) {
	if (size & 0x3fff) {
	    printf ("idle parity: illegal size 0x%x\n", size);
	    return;
	}
	/* use 16 K pages */
	*(volatile unsigned int *)PHYS_TO_K1(DMA_SIZE) = 
	    ((size >> 14) << 16) | (1 << 14);
    }
    else
	*(volatile unsigned int *)PHYS_TO_K1(DMA_SIZE) = (1<<16)|size;

    /* set data, start DMA */
    *(volatile unsigned int *)PHYS_TO_K1(DMA_GIO_ADRS) = KDM_TO_MCPHYS(daddr);
}

#define DMA_IN_PROGRESS() \
	((*(volatile unsigned int *)PHYS_TO_K1(DMA_RUN) & 0x40) ? 1 : 0)
int
dma_in_progress(void)
{
    return DMA_IN_PROGRESS();
}

void
dma_wait(void)
{
    while (dma_in_progress()) {
	/* empty */
    }

    /* clear interrupts */
    *(volatile unsigned int *)PHYS_TO_K1(DMA_CAUSE) = 0;
}

int fixdmux = 1;
void
DMUXfix(void)
{
    if (!fixdmux)
	return;

    dma_wait();
    dma_start((void *)KDM_TO_PHYS(dmux_pagebuf), (void *)0, DMUX_PAGEBUF_SIZE, DMA_FILL);
    dma_wait();
}
#else /* IPMHSIM */
void
DMUXfix(void)
{
}
#endif

#ifdef PERR_DEBUG
/*
 * IP20/IP22 parity error debug support
 */

/* shorthand to call write_bad_bits from symmon */
int
wbb(paddr_t paddr, u_int bitmask)
{
    return write_bad_bits(paddr, bitmask);
}

/* Scan every location on a page for memory errors 
 */
#define PAR_ERR(x)	((x & CPU_ERR_STAT_RD_PAR) == CPU_ERR_STAT_RD_PAR)
int
scan_page(paddr_t paddr, int verbose)
{
	uint cpu_err_status;
	uint cpu_err_addr;
	volatile int *addr;
	int bcount;
	int dummy, errcount = 0;
	int par_enabled;

	paddr = (paddr_t)ctob(btoct((uint)paddr));	/* page align */

	par_enabled = is_sysad_parity_enabled();
	if (par_enabled)
	    disable_sysad_parity();

	if (verbose)
	    cmn_err(CE_CONT, "!Scanning page at 0x%x for parity errors...\n",
		paddr);

	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0x0;
	flushbus();

	addr = (int *)map_physaddr(paddr);
	/* parity gets checked over entire 64 bits when reading 
	 * only 32 bits
	 */
	for (bcount = 0; bcount < DMUX_PAGEBUF_SIZE; bcount += 8, addr++, addr++) {
	    dummy = *addr;
	    cpu_err_status = *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT);
	    if (PAR_ERR(cpu_err_status)) {
		cpu_err_addr = *(volatile uint *)PHYS_TO_K1(CPU_ERR_ADDR);
		*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0x0;
		flushbus();
		errcount++;

		bad_data_hi = *(volatile uint *)((uint)addr & ~7);
		bad_data_lo = *(volatile uint *)(((uint)addr & ~7) + 4);
		*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0x0;
		flushbus();

		cmn_err(CE_CONT, "Memory parity error "
			"detected but not corrected:  "
			"status/addr: <0x%x>/0x%x, Data: 0x%x/0x%x\n",
			cpu_err_status, cpu_err_addr,
			bad_data_hi, bad_data_lo);

	    } else if (cpu_err_status) {
		if (verbose)
		    cmn_err(CE_CONT,
			"!Strange status 0x%x checking page 0x%x\n",
			cpu_err_status, paddr);
	    }
	}
	unmap_physaddr((caddr_t) addr);

	if (verbose)
	    cmn_err(CE_CONT, "!%d error%s detected on page 0x%x.\n", errcount, 
		errcount == 1 ? "" : "s", paddr);

	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0x0;
	flushbus();

	if (par_enabled)
	    enable_sysad_parity();

	return errcount;
}

void
scan_memory(void)
{
	int dmasize;
	char *daddr; 
	char *saddr;
	char *k2saddr;
	int pgcount;
	uint cpu_err_status;
	int errcount = 0;
	int par_enabled;
	pfn_t memsize = szmem(0,0);
#ifdef IP22
	extern pfn_t low_maxclick;
#endif	/* IP22 */

	par_enabled = is_sysad_parity_enabled();
	if (par_enabled)
	    disable_sysad_parity();

	daddr = dmux_pagebuf;

	/* make sure the error status registers are cleared */
	*(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT) = 0x0;
	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0x0;
	flushbus();

#ifdef IP22
	if ((dmasize = ctob(memsize)) > ctob(low_maxclick) - SEG0_BASE)
		dmasize = ctob(low_maxclick) - SEG0_BASE;
#else
	if ((dmasize = ctob(memsize)) > SEG0_SIZE)
		dmasize = SEG0_SIZE;
#endif	/* IP22 */

	saddr = (char *)PHYS_TO_K1(SEG0_BASE);
	for (pgcount = btoc(dmasize); pgcount > 0; pgcount--, saddr += DMUX_PAGEBUF_SIZE) {
	    bcopy (saddr, daddr, DMUX_PAGEBUF_SIZE);
	    cpu_err_status = *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT);
	    if (PAR_ERR(cpu_err_status))
		errcount += scan_page((paddr_t)K1_TO_PHYS(saddr), 1);
	}

	if (ctob(memsize) > SEG0_SIZE) {
#ifdef IP22
		dmasize = ctob(memsize) - (ctob(low_maxclick) - SEG0_BASE);
#else
		dmasize = ctob(memsize) - SEG0_SIZE;
#endif	/* IP22 */

		saddr = (char *)SEG1_BASE;
		for (pgcount = btoc(dmasize); pgcount > 0 ; 
				pgcount--, saddr += DMUX_PAGEBUF_SIZE) {
		    k2saddr = (char *)map_physaddr((paddr_t)saddr);
		    bcopy (k2saddr, daddr, DMUX_PAGEBUF_SIZE);
		    unmap_physaddr((caddr_t)k2saddr);
		    cpu_err_status = *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT);
		    if (PAR_ERR(cpu_err_status))	
			    errcount += scan_page((paddr_t) saddr, 1);
		}
	}

	*(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT) = 0x0;
	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0x0;
	flushbus();

	if (par_enabled)
	    enable_sysad_parity();

	cmn_err (CE_CONT, "Memory scan found %d errors\n", errcount);
}
#endif /* PERR_DEBUG */

/*
 * Write one byte (bitmask) to physical address (paddr) with
 * bad parity.  Bits set in bitmask get flipped.
 *
 * Returns 0 on success, 1 on failure.
 */
int
write_bad_bits(paddr_t paddr, u_int bitmask)
{
	u_char value;
	int s;

	if (!is_in_main_memory(pnum(paddr)))
		return 1; 	/* not in main memory */

	s = get_sr();
	if (!EXCEPTION_MODE(s))
		s = splhi();

	value = r_phys_byte_nofault(paddr);
	value ^= bitmask;
	parwbad(paddr, value, 1);
	DMUXfix();

	if (!EXCEPTION_MODE(s))
		splx(s);

	return 0;
}

/*
 * Parity gun code.
 */

#ifdef DEBUG
int pgun_verbose = 0;	/* Dump a lot of messages about addresses */
int pgun_gen_err = 1;	/* Really put bad data in or just call a stub */

#define PAR_FUNC(addr, mask) \
	pgun_gen_err ? write_bad_bits(addr, mask):lcl_write_bad_bits(addr, mask)
#else
#define PAR_FUNC(addr, mask) write_bad_bits(addr, mask)
#endif

#ifdef DEBUG
/*
 * Stub for write_bad_bits call.
 */
static int
lcl_write_bad_bits (u_long paddr, u_int bitmask)
{
    if (pgun_verbose)
	printf ("Writing bad parity with mask 0x%x to phys addr 0x%x.\n", 
		bitmask, paddr);
    return 0;
}
#endif /* DEBUG */

/*
 * parity_gun
 * 
 * Will write bad parity to a user specified address.
 * Takes one of 3 formats:
 *  1) Physical address
 *  2) User virtual address (current process)
 *  3) PID + User virtual address
 * Or, if the cmd is PGUN_KTEXT, will test parity error recover in kernel text.
 */
/*ARGSUSED3*/
int
parity_gun(int cmd, void *cmd_data, rval_t *rval)
{
    PgunData pgd;
    char bucket;
    int s;
    extern void touch_bad_cacheline(unsigned);
    extern int two_set_pcaches;
    
    if (copyin (cmd_data, &pgd, sizeof (pgd))) return EINVAL;
    
    switch (cmd) {
    case (PGUN_PHYS):
	PAR_FUNC (pgd.addr, pgd.bitmask);
	break;

    case (PGUN_VIRT):
    {
	/* Use vtop() here.  (Definitely OK since cur process.) */
	pfn_t pfn;
	ulong paddr;
	
#ifdef DEBUG
	if (pgun_verbose) printf ("PGUN_VIRT:mask 0x%x addr 0x%x.\n", pgd.bitmask, pgd.addr);
#endif
	if (!vtop((caddr_t)pgd.addr, 1, &pfn, 1))
		return EINVAL;
	
	/* If the page is not valid, assume that it just hasn't been touched.
	 * Let's force it in, then try again.
	 */
	if (pfn == 0) {
	    copyin ((void *)pgd.addr, &bucket, 1);
	    if (!vtop((caddr_t)pgd.addr, 1, &pfn, 1))
		return EINVAL;
	    if (pfn == 0) return EINVAL;
	}
	
	paddr = pfn << PNUMSHFT | pgd.addr & POFFMASK;
#ifdef DEBUG
	if (pgun_verbose) printf ("PGUN_VIRT:pfn: 0x%x, paddr: 0x%x.\n", pfn, paddr);
#endif
	/* Before we stuff the parity error, make sure there's not a clean
	 * cache line sitting on top of it.  If there is, the read might never
	 * happen, and then we wouldn't see the error.
	 */
	dki_dcache_wbinval ((void *)pgd.addr, 1);
	PAR_FUNC (paddr, pgd.bitmask);
	break;
    }

#ifdef NOMORE
    /* This is just a test and requires a special version of vtop -
     * vtop has been changed to work on the current process only ..
     */
    /* A couple of weird things are going on here.
     * First is that we're getting a physical address for another process'
     * address space.  So, after the bad parity is written, we have to hope
     * nobody hits it before it is referenced by the process in question
     * The other is that after VPROC_LOOKUP is called, the process can't exit
     * until VPROC_RELE is called, so be sure to unlock it before returning.
     * Use of vtop() is a little questionable,  since pages are not locked.
     */
    case (PGUN_PID_VIRT):
    {
	int pfn;
	ulong paddr;
	vproc_t *vpr;
	proc_t *otherproc;
	
#ifdef DEBUG
	if (pgun_verbose) printf ("PGUN_PID_VIRT: mask 0x%x addr 0x%x pid %d.\n",
		pgd.bitmask, pgd.addr, pgd.pid);
#endif
	if ((vpr = VPROC_LOOKUP(pgd.pid)) == NULL)
		return EINVAL;
	VPROC_GET_PROC(vpr, &otherproc);

	if (!vtop (otherproc, (caddr_t)pgd.addr, 1, &pfn, 1)) {
	    VPROC_RELE(vpr);
	    return EINVAL;
	}
	paddr = pfn << PNUMSHFT | pgd.addr & POFFMASK;
#ifdef DEBUG
	if (pgun_verbose) printf ("PGUN_PID_VIRT:pfn: 0x%x, paddr: 0x%x.\n", pfn, paddr);
#endif
	/* We need to invalidate any clean cache line corresponding to this
	 * address, but the dcache_* routines assume KUSEG addresses are in
	 * the current process, so they need to be recreated here.
	 * Shit.
	 * XXX
	 */
	PAR_FUNC (paddr, pgd.bitmask);
	VPROC_RELE(vpr);
	
	break;
    }

#endif /* NOMORE */

    case (PGUN_KTEXT):
	{
	    int pksrc[5], pkdst[5], i;
	    
	    /* Flip a bit in bcopy code. */
	    PAR_FUNC (kvtophys((void *)bcopy), 0x10);
	    __icache_inval((caddr_t)bcopy, 4);

	    for (i=0;i<5;i++) {  /* Make sure src and dst are different. */
		pksrc[i]=i*3;
		pkdst[i]=0;
	    }

	    /* This should trigger a recoverable error */
	    bcopy (pksrc, pkdst, sizeof(pksrc));

	    for (i=0;i<5;i++) {	/* Check the results. */
		if (pksrc[i] != pkdst[i]) return EIO;
	    }
	}
	break;

    case (PGUN_CACHE_NOEE):
        {
	    
	/* Use vtop() here.  (Definitely OK since cur process.) */
	pfn_t pfn;
	ulong paddr;
	
#ifdef DEBUG
	if (pgun_verbose) printf ("PGUN_CACHE_NOEE:mask 0x%x addr 0x%x.\n", pgd.bitmask, pgd.addr);
#endif
	if (!vtop((caddr_t)pgd.addr, 1, &pfn, 1))
		return EINVAL;
	
	/* If the page is not valid, assume that it just hasn't been touched.
	 * Let's force it in, then try again.
	 */
	if (pfn == 0) {
	    copyin ((void *)pgd.addr, &bucket, 1);
	    if (!vtop((caddr_t)pgd.addr, 1, &pfn, 1))
		return EINVAL;
	    if (pfn == 0) return EINVAL;
	}
	
	paddr = pfn << PNUMSHFT | pgd.addr & POFFMASK;
#ifdef DEBUG
	if (pgun_verbose) printf ("PGUN_CACHE_NOEE:pfn: 0x%x, paddr: 0x%x.\n", pfn, paddr);
#endif
	/* Before we stuff the parity error, make sure there's not a clean
	 * cache line sitting on top of it.  If there is, the read might never
	 * happen, and then we wouldn't see the error.
	 */
	dcache_wbinval ((void *)pgd.addr, 1);
	dcache_wbinval ((void *)PHYS_TO_K0(paddr), 1);
	PAR_FUNC (paddr, pgd.bitmask);
	dcache_wbinval ((void *)PHYS_TO_K0(paddr), 1);
#ifdef DEBUG
	if (pgun_verbose) printf("PGUN_CACHE_NOEE:about to touch 0x%x\n", paddr & ~0x1f);
#endif
	s = spl7();
	touch_bad_cacheline(PHYS_TO_K0(paddr & ~0x1f));
	splx(s);
	break;
	}

    case (PGUN_TEXT_NOEE):
      {
	extern void execute_bad_cacheline();
	void (*eaddr)(void) = (void (*)())(((int)execute_bad_cacheline + 64) & ~0x1f);
#ifdef DEBUG
	if (pgun_verbose) printf("PGUN_TEXT_NOEE: about to execute 0x%x\n", eaddr);
#endif
	icache_inval((void *)eaddr, 1);
	PAR_FUNC(K0_TO_PHYS(eaddr) + 16, 8);
	s = spl7();
	(*eaddr)();
	splx(s);
	break;
      }
       default:
#ifdef DEBUG
	if (pgun_verbose) printf ("parity_gun: bad cmd %d.\n", cmd);
#endif
	return EINVAL;
    } /* switch cmd */
    
    return 0;
}

static int
ecc_cache_line_size(int cache_id)
{
	switch (cache_id) {
	case CACH_PI:
		return(1 << (4 + ((get_r4k_config() & CONFIG_IB) >> CONFIG_IB_SHFT)));

	case CACH_PD:
		return(1 << (4 + ((get_r4k_config() & CONFIG_DB) >> CONFIG_DB_SHFT)));

	case CACH_SI:
	case CACH_SD:
	default:
		return(1 << (4 + ((get_r4k_config() & CONFIG_SB) >> CONFIG_SB_SHFT)));
	}
}


static int
ecc_cache_size(int cache_id)
{
	switch (cache_id) {
	case CACH_PI:
		return(1 << 
		       (12 + ((get_r4k_config() & CONFIG_IC) >> CONFIG_IC_SHFT)));

	case CACH_PD:
		return(1 << 
		       (12 + ((get_r4k_config() & CONFIG_DC) >> CONFIG_DC_SHFT)));

	case CACH_SI:
	case CACH_SD:
	default:
		return(private.p_scachesize);
	}
}

static caddr_t
ecc_cache_index(int cache_id,paddr_t physaddr)
{
	return((caddr_t) 
	       PHYS_TO_K0(((physaddr & ~(ecc_cache_line_size(cache_id) - 1)) &
			   (K0SIZE - 1))));
}

static int
ecc_cache_block_mask(int cache_id)
{
	if ((get_r4k_config() & CONFIG_SC) == 0) /* 0 == scache present */
		return(~(ecc_cache_line_size(CACH_SD) - 1));
	switch (cache_id) {
	case CACH_PI:
	case CACH_SI:
		return(~(ecc_cache_line_size(CACH_PI) - 1));
	case CACH_PD:
	case CACH_SD:
	default:
		return(~(ecc_cache_line_size(CACH_PD) - 1));
	}
}


int
ecc_same_cache_block(int cache_id,paddr_t a, paddr_t b)
{
	int	line_mask = ecc_cache_block_mask(cache_id);

	return((a & line_mask) ==
	       (b & line_mask));
}



/*
 *	perr_fixup_caches
 *	
 *	This routine removes lines relating to a bad memory location
 *	from the caches.  It sets cache_err_state.flags & CESF_DATA_IS_BAD if
 *	a line is dirty and bad and this is not an exception.
 */

#define NUM_TAGS 2
#define TAGLO_IDX	0	/* load & store cachops: lo == [0], hi [1] */
#define TAGHI_IDX	1	/* not used on IP17 (taghi must be zero!) */

/*ARGSUSED*/
int
perr_fixup_caches(eframe_t *ep)
{
	DECLARE_PSTATE;
	int	mark_bad = 0;
	caddr_t	cache_index;
	paddr_t	physaddr;
	int	csize;
	int	tags[NUM_TAGS];

	if (! INIT_PSTATE)
		return(0);
	physaddr = (paddr_t) pstate->eccfp[ECCF_PADDR];	

	if (pstate->mode == PERC_CACHE_LOCAL) /* no MC error */
		return(1);	/* nothing to do this late */

	if ((get_r4k_config() & CONFIG_SC) == 0) { /* 0 == scache present */
		cache_index = ecc_cache_index(CACH_SD,physaddr);
		_read_tag(CACH_SD,cache_index,tags);
		if (((tags[TAGLO_IDX] & SADDRMASK) << SADDR_SHIFT) ==
		    (physaddr & (SADDRMASK << SADDR_SHIFT)) &&
		    (tags[TAGLO_IDX] & SSTATEMASK) > SINVALID) {
			/* match and valid */
			mark_bad |= ((tags[TAGLO_IDX] & SSTATEMASK) >
				     SCLEANEXCL);
			/* writeback and invalidate line */
			__cache_wb_inval(cache_index,
					 ecc_cache_line_size(CACH_SD));
		}
	} else {
		int	i;

		/* scan all primary cache tags */
		mark_bad = 0;
		csize = ecc_cache_size(CACH_PI);
		for (i = 0; i < csize; i += NBPP) {
			cache_index = ecc_cache_index(CACH_PI,
						      physaddr + i);
			_read_tag(CACH_PI,cache_index,tags);
			if (((tags[TAGLO_IDX] & PADDRMASK) <<
			     PADDR_SHIFT) ==
			    (physaddr & (PADDRMASK << PADDR_SHIFT)) &&
			    (tags[TAGLO_IDX] & PSTATEMASK) > PINVALID)
				__icache_inval(cache_index,
					       ecc_cache_line_size(CACH_PI));
		}

		csize = ecc_cache_size(CACH_PD);
		for (i = 0; i < csize; i += NBPP) {
			cache_index = ecc_cache_index(CACH_PD,
						      physaddr + i);
			_read_tag(CACH_PD,cache_index,tags);
			if (((tags[TAGLO_IDX] & PADDRMASK) <<
			     PADDR_SHIFT) ==
			    (physaddr & (PADDRMASK << PADDR_SHIFT)) &&
			    (tags[TAGLO_IDX] & PSTATEMASK) != PINVALID) {
				mark_bad |= ((tags[TAGLO_IDX] & PSTATEMASK)
					     > PCLEANEXCL);
				__dcache_wb_inval(cache_index,
						  ecc_cache_line_size(CACH_PD));
			}
		}
	}
	pstate->flushaddr = cache_index;

	/* set bad parity again in memory if line was dirty */
	if (mark_bad) {
		write_bad_bits(physaddr, 0x0);
		/* prevent process recovery if data corrupted */
		if (pstate->mode == PERC_BE_INTERRUPT)
			pstate->flags |= CESF_DATA_IS_BAD;
	}

	return(1);
}

int
ecc_find_pidx(int loc, paddr_t physaddr)
{
  /*
   * this routine represents a workaround for a bug in the
   * R4000 which causes it to report an incorrect pidx in 
   * the cache error register on a primary cache data error.
   * scan the cache tags looking for a match on the physical
   * address of the error.
   */
  int	tags[NUM_TAGS];
  int   i, csize = ecc_cache_size(loc);

  ASSERT(loc == CACH_PI || loc == CACH_PD);
  for (i = 0; i < csize; i += NBPP) {
  	caddr_t cache_index = ecc_cache_index(loc, physaddr + i);
  	_read_tag(loc,cache_index,tags);
  	if (((tags[TAGLO_IDX] & PADDRMASK) << PADDR_SHIFT) == 
	    (physaddr & (PADDRMASK << PADDR_SHIFT)))
	  return(((uint)cache_index >> CACHERR_PIDX_SHIFT) & CACHERR_PIDX_MASK);
  }
  return(-1);
}

/*
 * ecc_fixup_caches() -- a close cousin of perr_fixup_caches() but is
 * called from ecc_fixcdata() to invalidate icache lines or writeback
 * and invalidate dcache (or scache) lines.
 */
/*ARGSUSED3*/
int
ecc_fixup_caches(int loc, paddr_t physaddr, k_machreg_t pidx, uchar_t from_mc)
{
	DECLARE_PSTATE;
	int	mark_bad = 0;
	caddr_t	cache_index;
	int	tags[NUM_TAGS];
	int	i, csize;
	int	found = 0;
	int	pstate_initted = 0;

	/* failing to init pstate should not be considered
	 * fatal.
	 */
	if (INIT_PSTATE)
		pstate_initted = 1;

	if ((get_r4k_config() & CONFIG_SC) == 0) { /* 0 == scache present */
		loc = CACH_SD;
		cache_index = ecc_cache_index(CACH_SD,physaddr);
		_read_tag(CACH_SD,cache_index,tags);
		if (((tags[TAGLO_IDX] & SADDRMASK) << SADDR_SHIFT) ==
		    (physaddr & (SADDRMASK << SADDR_SHIFT)) &&
		    (tags[TAGLO_IDX] & SSTATEMASK) > SINVALID) {
			/* match and valid */
			found = 1;
			mark_bad |= ((tags[TAGLO_IDX] & SSTATEMASK) >
				     SCLEANEXCL);
			/* writeback and invalidate line */
			__cache_wb_inval(cache_index,
					 ecc_cache_line_size(CACH_SD));
		}
	} 

	if (loc == CACH_PI) {
		mark_bad = 0;
		csize = ecc_cache_size(CACH_PI);
		for (i = 0; i < csize; i += NBPP) {
			cache_index = ecc_cache_index(CACH_PI, physaddr + i);
			_read_tag(CACH_PI,cache_index,tags);
			if (((tags[TAGLO_IDX] & PADDRMASK) << PADDR_SHIFT) ==
			    (physaddr & (PADDRMASK << PADDR_SHIFT)) &&
			    (tags[TAGLO_IDX] & PSTATEMASK) != PINVALID) {
				found = 1;
				tags[TAGLO_IDX] = tags[TAGHI_IDX] = 0;
				_c_ist(CACH_PI, cache_index, tags);
			}
		}
	}

	if (loc == CACH_PD) {
		csize = ecc_cache_size(CACH_PD);
		for (i = 0; i < csize; i += NBPP) {
			cache_index = ecc_cache_index(CACH_PD,
						      physaddr + i);
			_read_tag(CACH_PD,cache_index,tags);
			if (((tags[TAGLO_IDX] & PADDRMASK) <<
			     PADDR_SHIFT) ==
			    (physaddr & (PADDRMASK << PADDR_SHIFT)) &&
			    (tags[TAGLO_IDX] & PSTATEMASK) != PINVALID) {
				found = 1;
				mark_bad |= ((tags[TAGLO_IDX] & PSTATEMASK)
					     > PCLEANEXCL);
				__dcache_wb_inval(cache_index,
						  ecc_cache_line_size(CACH_PD));
				break;
			}
		}
	}

	/* set bad parity again in memory if line was dirty */
	if (mark_bad) {
		write_bad_bits(physaddr, 0x0);
		tags[TAGLO_IDX] = tags[TAGHI_IDX] = 0;
		_c_ist(CACH_PD, cache_index, tags);
		if (pstate_initted && !from_mc)
			pstate->flags |= CESF_PERR_IS_FORCED;
	}
	         
	if (found)
		return(1);
	else
		return(0);
}

/*
 *	ecc_exception_recovery
 *
 *	This routine is invoked on a "soft ECC exception" synthesized
 *	by a partially recovered cache error exception.  It simply
 *	invokes the generic parity error recovery routine.
 */

int
ecc_exception_recovery(eframe_t *ep)
{
	DECLARE_PSTATE;
	int	s;
#ifdef R4600SC
	extern int two_set_pcaches;
	int r4600sc_scache_disabled = 1;
#endif

	/*
	 * This call is a result of a cache error exception, so
	 * perr_save_info() will normally have saved state for us.
	 * If not, recovery is hopeless.
	 */
	if (eccf_addr == NULL)
		return(0);	/* no saved state: failed */

	if (!INIT_PSTATE)
		return(0);

	s = splecc(); /* prevent any bus error interrupts */

#ifdef R4600SC
	if (two_set_pcaches && private.p_scachesize)
		r4600sc_scache_disabled = _r4600sc_disable_scache();
#endif

	/* perr_save_info left sysad parity checking disabled so 
	 * the interrupt could be handled, rather than taking
	 * yet another cache error exception.  It's also possible
	 * that the cacheline was re-touched and made valid again.
	 * Invalidate it.
	 */
	if (! no_parity_workaround) {
		switch (pstate->mode) {
		case PERC_CACHE_SYSAD:
		case PERC_CACHE_LOCAL:
			enable_sysad_parity_erl();
			break;
		default:
			enable_sysad_parity();
			break;
		}
	}

	if (! perr_fixup_caches(ep)) {
#ifdef R4600SC
		if (!r4600sc_scache_disabled)
			_r4600sc_enable_scache();
#endif
		splxecc(s);
		return(0);
	}

	if (perr_recover(ep)) {
#ifdef R4600SC
		if (!r4600sc_scache_disabled)
			_r4600sc_enable_scache();
#endif
		splxecc(s);
		return(1);
	}

	/* The error recovery attempt failed.
	 * Fix the bad memory and kill the user process,
	 * or let the kernel panic in dobuserre().
	 */
	if (USERMODE(ep->ef_sr)) {
		caddr_t vaddr;

		/* set correct parity on address */
		vaddr = map_physaddr(eccf_addr[ECCF_PADDR]);
		fix_bad_parity((volatile uint *) vaddr);
		unmap_physaddr(vaddr);
	}
	eccf_addr = 0;

#ifdef R4600SC
	if (!r4600sc_scache_disabled)
		_r4600sc_enable_scache();
#endif
	splxecc(s);
	return(0);
}

/*
 *	ecc_discard_page
 *
 *	This routine is called to attempt recovery from a parity
 *	or ecc error in memory or cache which cannot be recovered
 *	by direct manipulation of the memory or cache.  (Single-bit
 *	ECC errors can be corrected and clean cache lines with 
 *	uncorrectable can be discarded, but other cases are real failures.)

 *	If we cannot recover at the instruction level, because
 *	we have attempted to read a bad word, we attempt to fetch a new
 *	copy of the page, if the copy in memory is a clean copy of a disk
 *	page (whether on swap or in a file).  We can only do this if
 *	the exception is from user mode or from a kernel nofault routine.
 *	If this is the case, we mark the page bad, unhash it,
 *	invalidate the reference in the current
 *	process's pte, and, if the required locks are available no-wait,
 *	clear the error and free the page.  If the page cannot be freed,
 *	we leave the error in the page, so that the other users will
 *	not see bad data which appears to be good.  We then dismiss
 *	the exception or interrupt, at which time the current process
 *	takes a vfault on the page, and vfault brings in a new copy.
 *	When we have to leave a bad page around, the page is anonymous
 *	in the page cache, but is logically a copy of a disk page.
 *	The virtual space attributes in any remaining references must
 *	be used to deduce the correct identity, if we take a pfault
 *	(and want to do copy-on-write).  Also, this routine must
 *	account for seeing such a page, and simply discard the current
 *	reference.
 *
 *	Attempt to release pagenum, based on the interrupted context
 *	being described by ep.  We can release pagenum, if ep is in
 *	user mode or in a nofault handler, so that a page fault will
 *	be tolerated.  (Otherwise, we fail.)  
 */

extern int apply_to_pdes(pfn_t,
			 int (*) (pas_t *,
				  pfn_t,
				  struct proc *,
				  struct pregion *,
				  char *,
				  union pde *,
				  void *),
			 void *);

extern int apply_to_process_pdes(pfn_t,
				 struct proc *,
				 int (*) (pas_t *, pfn_t,
					  struct proc *,
					  struct pregion *,
					  char *,
					  union pde *,
					  void *),
				 void *);

struct check_for_clean_pte_arg {
	int	ref_count;
	int	dirty_count;
};

/*ARGSUSED*/
static int
check_for_clean_pte(pas_t *pas,
			 pfn_t pfn,
			 struct proc *pp,
			 struct pregion *prp,
			 char *vaddr,
			 union pde *pde,
			 void *argp)
{
#define arg (*(struct check_for_clean_pte_arg *) argp)

	if (pg_isdirty(pde))
		arg.dirty_count ++;
	arg.ref_count++;
	return(0);

#undef arg
}	

struct toss_pte_arg {
	int	curproc_ref_count;
	int	toss_count;
	int	ref_count;
};

/*ARGSUSED*/
static int
toss_pte(pas_t *pas,
	 pfn_t pfn,
	 struct proc *pp,
	 struct pregion *prp,
	 char *vaddr,
	 union pde *pde,
	 void *argp)
{
#define arg (*(struct toss_pte_arg *) argp)

	arg.ref_count++;
	if (pp == curprocp)
		arg.curproc_ref_count++;
	if (pde->pte.pg_vr) {
		arg.toss_count++;
		pg_clrhrdvalid(pde);
		/* XXX which uthread? */
		uscan_hold(&pp->p_proxy);
		if (tlbpid_is_usable(&prxy_to_thread(&pp->p_proxy)->ut_as))
			unmaptlb(prxy_to_thread(&pp->p_proxy)->ut_as.utas_tlbpid,pnum(vaddr));
		uscan_rele(&pp->p_proxy);
	}
	return(0);
#undef arg
}


static int
free_pte(pas_t *pas, 
	 pfn_t pfn,
	 struct proc *pp,
	 struct pregion *prp,
	 char *vaddr,
	 union pde *pde,
	 void *argp)
{
#define arg (*((int *) argp))
	reg_t *rp;
	pfd_t *pfd = pfntopfdat(pfn);

	rp = prp->p_reg;
	if (rp != NULL) 
		reglock(rp);

	VPAG_UPDATE_RMAP_DELMAP(PAS_TO_VPAGG(pas), JOBRSS_DECREASE, pfd, pde);
	pg_clrpgi(pde);
	PREG_NVALID_DEC(prp, 1);			/* SPT */
	/* XXX which uthread ?? */
	uscan_hold(&pp->p_proxy);
	if (tlbpid_is_usable(&prxy_to_thread(&pp->p_proxy)->ut_as))
		unmaptlb(prxy_to_thread(&pp->p_proxy)->ut_as.utas_tlbpid, pnum(vaddr));
	uscan_rele(&pp->p_proxy);
	if (rp != NULL)
		regrele(rp);

	pagefree(pfntopfdat(pfn));
	arg++;

	return(0);
#undef arg
}


extern int is_in_pfdat(pgno_t);


int
ecc_discard_page(pgno_t pagenum, eframe_t *ep, k_machreg_t *eccfp)
{
	pfd_t	*pfd;
	struct check_for_clean_pte_arg check_clean_arg;
	struct toss_pte_arg toss_arg;
	int	free_count;
	int	fix_parity = 0;
	caddr_t mapped_bad_addr;

	if (pfdat == NULL)
		return(0); /* pfdat not yet allocated */

	if (! USERMODE(ep->ef_sr)) {
		if (curprocp == NULL || 
		    ! (private.p_nofault || curthreadp->k_nofault))
			return(0); /* faults not permitted */
	}
	
	if (! is_in_pfdat(pagenum))
		return(0); /* cannot discard if not in pfdat */

	pfd = pfntopfdat(pagenum);
	if (pfd->pf_use == 0 || 
	    pfd->pf_rawcnt > 0 ||
	    (pfd->pf_flags & (P_DIRTY|P_QUEUE|P_SQUEUE)))
		return(0); /* cannot discard if free, locked, or dirty */

	if (pfd->pf_flags & P_ANON) {
		if (! (pfd->pf_flags & P_SWAP))
			return(0); /* no backing store for page */
	} else {
		if (pfd->pf_vp == NULL)
			return(0); /* no backing file for page */
	}

	/* 
	 * see if we can actually discard the page
	 */
	check_clean_arg.ref_count = 0;
	check_clean_arg.dirty_count = 0;
	if (apply_to_pdes(pagenum,
			  check_for_clean_pte,
			  (void *) &check_clean_arg) ||
	    			/* failure searching the data structures */
	    check_clean_arg.dirty_count > 0)
				/* one or more references are dirty */
		return(0); 
	
	/*
	 * unhash the page and mark it bad
	 */
	if (pfd->pf_flags & P_HASH) {
		if ((! USERMODE(ep->ef_sr)) &&
		    (! (pfd->pf_flags & P_ANON)) &&
		    pfd->pf_use > check_clean_arg.ref_count)
			return(0);
		pagebad(pfd);
	} else if (! (pfd->pf_flags & P_BAD))
		pageflags(pfd,P_BAD,1);
	
	/*
	 * turn off pg_vr in all PTEs
	 */
	toss_arg.curproc_ref_count = 0;
	toss_arg.toss_count = 0;
	toss_arg.ref_count = 0;
	if (apply_to_pdes(pagenum,
			  toss_pte,
			  (void *) &toss_arg))
		return(0); /* failed */
				/* XXX -- panic on this failure? */
	
	/*
	 * discard references in current process
	 */
	if (toss_arg.curproc_ref_count > 0) {
		pageuseinc(pfd);

		free_count = 0;
		if (apply_to_process_pdes(pagenum,
					  curprocp,
					  free_pte,
					  (void *) &free_count)) {
			return(0); /* failed */
		}

		/*
		 * If we are the last reference, clear the parity error.
		 */
		if (pfd->pf_use == 1) 
			fix_parity = 1;

		pagefree(pfd);
	} else
		fix_parity = 1;

	if (fix_parity) {
		mapped_bad_addr = map_physaddr((paddr_t) 
					       (eccfp[ECCF_PADDR] & ~0x7));

		fix_bad_parity((volatile uint *)mapped_bad_addr);

		unmap_physaddr(mapped_bad_addr);
	}

	return(1); /* succeeded */
}


int
ecc_perr_is_forced(void)
{
	DECLARE_PSTATE;

	if (!INIT_PSTATE)
		return(0);

	return(pstate->flags & CESF_PERR_IS_FORCED);
}


#endif /* _MEM_PARITY_WAR */

#endif /* defined(IP20) || defined(IP22) */
