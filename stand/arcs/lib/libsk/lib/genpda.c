/*
 * genpda.c -- common gpda routines.
 */

#ident "$Revision: 1.10 $"

#include <sys/sbd.h>
#include <genpda.h>
#include <stringlist.h>
#include <libsc.h>
#include <libsk.h>
#include <alocks.h>

#ifdef EVEREST
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/IP19addrs.h>
#endif

#if EVEREST
extern int MPC_v_to_p(int);	/* libsk/ml/EVEREST.c */
#endif
#ifdef DO_TOD
extern void	inittodr(void);
#endif

static void	check_gpdas(void);

/*
 * _fault_sp is the global copy of the exception stackpointer,
 * which is initialized by the master (bootup) thread of all
 * standalones linking with libsk.  fault.c:setup_gpdas()
 * copies this value into the pda_fault_sp field of vid 0's
 * pda for use by faultasm.s.
 */
unsigned long *_fault_sp;
unsigned long *initial_stack;	/* used by libsk/ml/csu.s */

/*
 * I decided to declare and initialize the stacks and jump buffers
 * used by standalone slave processes in the invoking prrogram.
 * They know how big/many are necessary.  The generic pda should
 * contain values which conceivably would be needed before any slave
 * code could run.  
 */

/* 
 * Declare the arrays of pda structs and register sets used by
 * slaves and master processes. 
 * Note that all software accesses use virtual processor 
 * numbering (sequentially assigned numbers at runtime by
 * the prom.
 */
#ifdef SN
gen_pda_t	*gen_pda_tabp[MAXCPU];
#else
gen_pda_t	gen_pda_tab[MAXCPU];
k_machreg_t	excep_regs[MAXCPU][NREGS];
arcs_reg_t	atsave[MAXCPU]; /* at is saved here by exception hndlr */
#endif

extern int _dcache_size;

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX ifdef for non-mp instead
 * of using findcpu()
 */

/* 
 * setup_gpdas() initializes the per-processor generic private data areas.
 * Since this data is primarily for state-saving while handlingexceptions,
 * it must be called early in all versions of csu.s (currently libs{c,k}
 * and ide).
 */

void
setup_gpdas(void)
{
    register int vid;
    register __psunsigned_t inval;
    register int me = cpuid();

#ifdef DO_TOD
    _inittodr();	/* init tod clock structs early for best accuracy */
#endif

    check_gpdas();

    /* set up pda for all cpus */
    for (vid = 0; vid < MAXCPU; vid++) {
	register struct generic_pda_s *gpdap;
#ifdef SN
	int cpu;
	nasid_t nasid;

	gen_pda_tabp[vid] = NULL;

	if (get_nasid_slice(vid, &nasid, &cpu) == -1)
		continue;
	/*
	 * if cpu not enabled, we will get -1 as cpu slice.
	 */
	if (cpu == -1)
		continue;

	gen_pda_tabp[vid] = (gen_pda_t *)GPDA_ADDR(nasid) + cpu;
#endif
	gpdap = (struct generic_pda_s *)&GEN_PDA_TAB(vid);

	gpdap->my_virtid = vid;
#ifdef	EVEREST
	gpdap->my_physid = MPC_v_to_p(vid);

	if (gpdap->my_physid < 0){
	    printf("ARCS Internal Error: MPCONF corrupt?\n      ");
	    printf("setup_gpdas: MPC_v_to_p retval %d!\n", gpdap->my_physid);
	    panic("setup_gpdas");	
	}
#else
	gpdap->my_physid = vid;
#endif

	inval = ((vid << 16) + 0xf100);	/* vid in top short */

	if (vid == me) {
	    /* 
	     * the startup code in libsk/ml/csu.s built the normal 
	     * and exception stacks for us, and saved those
	     * SP addrs in initial_stack and _fault_sp, resp.
	     * Grab those values for our (vid 0's) pda.
	     */
	    gpdap->pda_fault_sp = _fault_sp;
	    gpdap->pda_initial_sp = initial_stack;
	    gpdap->stack_mode = MODE_NORMAL;
#if defined(PDADEBUG1)
	    printf("Bootmaster's fault_sp 0x%x,  initial_sp 0x%x (mode %d)\n",
		(int)gpdap->pda_fault_sp, (int)gpdap->pda_initial_sp, 
		(int)gpdap->stack_mode);
#endif
	} else {
	    /* 
	     * The calling program can set up the children's stacks later
	     * but until then mark their pda fields invalid.
	     */
	    gpdap->pda_fault_sp = (arcs_reg_t *)inval++;
	    gpdap->pda_initial_sp = (arcs_reg_t *)inval++;
	    gpdap->stack_mode = (unsigned)MODE_INVALID;
	}

	/* both slaves and master are handled alike for the other fields */
	/* mark these distinctively for debugging */
	gpdap->epc_save = inval++;
	gpdap->at_save = inval++;
	gpdap->v0_save = inval++;
	gpdap->badvaddr_save = inval++;
	gpdap->cause_save = inval++;
	gpdap->sp_save = inval++;
	gpdap->sr_save = inval++;
	gpdap->exc_save = inval++;
	gpdap->mode_save = inval++;
#if defined(R4000) || defined(R10000)
	gpdap->error_epc_save = inval++;
	gpdap->cache_err_save = inval++;
#endif	/* R4000 || R10000 */

	/* 
	 * faultasm.s uses pda's 'regs' ptr to determine the correct 
	 * _reg set in which to save the processor's state during 
	 * exception processing
	 */
#ifdef SN
	gpdap->regs = gpdap->excep_regs;
#else
	gpdap->regs = excep_regs[vid];
#endif

	/*
	 * 'nofault' and 'notfirst' must be manipulated via
	 * set_nofault() and clear_nofault() in order to correctly
	 * diagnose nested exceptions
	 */
 	gpdap->pda_nofault = 0;
 	gpdap->notfirst = 0;
 	gpdap->first_epc = ((vid << 16) + 0xbeef);

#if defined(PDADEBUG)
	if (vid == 0 || MPCONF[vid].virt_id != 0) {
	    printf("    %svid %d (%s): gpdap 0x%x, regs 0x%x\n",
		(vid == GPME(my_virtid) ?"  ":"    "),
		vid, (vid == GPME(my_virtid) ?"master":"slave"),(int)gpdap,
		(int)gpdap->regs);
	}
#endif /* PDADEBUG */
	gpdap->gpda_magic = GPDA_MAGIC;	/* proves it has been initialized */
    }

    return;

} /* setup_gpdas */

static void
check_gpdas(void)
{
#if defined(PDADEBUG) || defined(EVEREST)
    if (sizeof(arcs_reg_t) != R_SIZE) {
	printf( "\nARCS SRC ERROR: arcs_reg_t size (%d) != R_SIZE (%d)\n", 
		sizeof(arcs_reg_t), R_SIZE);
	panic("\nsetup_gpdas: register-size mismatch");
    }
    if (sizeof(arcs_reg_t *) != P_SIZE) {
	printf( "\nARCS SRC ERROR: (arcs_reg_t *) size (%d) != P_SIZE (%d)\n", 
		sizeof(arcs_reg_t *), P_SIZE);
	panic("\nsetup_gpdas: pointer-size mismatch");
    }
    if (sizeof(gen_pda_t) > GENERIC_PDASIZE) {
	printf( "\nARCS SRC ERROR, setup_gpdas: PDA size mismatch:\n");
	printf( "\n      gen_pda_t struct (%d) > GENERIC_PDASIZE (%d)\n",
	    sizeof(gen_pda_t), GENERIC_PDASIZE);
	panic("setup_gpdas: gen_pda_t mismatch");
    }
#endif

#if defined(PDADEBUG)
    printf( "\n  -->setup_gpdas: sizeof(gen_pda_t) %d, GENERIC_PDASIZE %d\n",
	sizeof(gen_pda_t), GENERIC_PDASIZE);
    printf(
"\n   size(arcs_reg_t) %d, R_SIZE %d; size(arcs_reg_t *) %d, P_SIZE %d\n",
		sizeof(arcs_reg_t), R_SIZE, sizeof(arcs_reg_t *), P_SIZE);
#endif

    return;

} /* check_gpdas */


#define VID_OUTARANGE(_VID)		(_VID < 0 || _VID >= MAXCPU)
/*ARGSUSED*/
void
dump_gpda(int vid)
{
#ifdef MULTIPROCESSOR
    register struct generic_pda_s *gp;
	
    if (VID_OUTARANGE(vid)) {
	panic("dump_gpda: bad vid");
    }

    gp = (struct generic_pda_s *)&GEN_PDA_TAB(vid);

    printf("\n  VID #%d's ARCS PDA:  &pda 0x%x, &regs 0x%x, magic 0x%x\n",
	vid, (__psunsigned_t)gp, (__psunsigned_t)gp->regs, gp->gpda_magic);

    printf("  vid %d, pid %d, init_sp 0x%x, fault_sp 0x%x, stack_mode %d\n",
	gp->my_virtid, gp->my_physid, (__psunsigned_t)gp->pda_initial_sp, 
	(__psunsigned_t)gp->pda_fault_sp, gp->stack_mode);

    printf("  mode %d, EPC 0x%x, AT 0x%x, badvaddr 0x%x\n",
	gp->mode_save, gp->epc_save, gp->at_save, gp->badvaddr_save);

#if defined(R4000) || defined(R10000)
    printf("  ErrEPC 0x%x, CacheErr 0x%x, cause 0x%x, v0 0x%x\n",
	gp->error_epc_save, gp->cache_err_save, gp->cause_save, gp->v0_save);
#else	/* TFP */
    printf("  cause 0x%x, v0 0x%x\n", gp->cause_save, gp->v0_save);
#endif	/* R4000 || R10000 */

    printf("  SP 0x%x, SR 0x%x, exc 0x%x, return_addr 0x%x\n",
	gp->sp_save, gp->sr_save, gp->exc_save, gp->retn_addr_save);

    printf("  notfirst 0x%x, firstEPC 0x%x, nofault 0x%x\n",
	gp->notfirst, gp->first_epc, gp->pda_nofault);

    return;
#endif
} /* dump_gpda */
