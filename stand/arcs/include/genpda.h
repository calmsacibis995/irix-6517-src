#ifndef __GENPDA_H__
#define __GENPDA_H__

/*
 * genpda.h -  multiprocessor-related definitions for arcs programs
 *
 *	genpda.h defines a structure containing all of the
 *	*generic* global data declared and referenced by arcs
 *	standalones directly or indirectly via libs{c,k} 
 *	routines, and which therefore must be duplicated on MP
 *	machines.  An array of these pda structures--indexed by
 *	the virtual processor number allowing sequential 
 *	numbering--provides a largely tranparent, runtime 
 *	solution to the MP-global data problem.  SP architectures
 *	have virtual processor ids hardwired to 0 and use pda 
 *	arrays of size 1, avoiding conditional compilation.
 *		
 *	All libsk and libsc routines (most importantly fault.c and
 *	faultasm.s!) now automatically reference the private copies.
 *	ide/ide_mp.[ch] illustrate a method for extending the
 *	arcs pda to include global data that isn't referenced
 *	outside of the standalone program in which it is declared.
 *
 * $Revision: 1.32 $
 */

#if EVEREST
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evmp.h>

#ifdef LARGE_CPU_COUNT_EVEREST
#define MAXCPU		128
#else
#define MAXCPU		EV_MAX_CPUS
#endif
#endif

#if SN
#if _LANGUAGE_C
#include <sys/types.h>
#endif
#include <sys/SN/agent.h>
#include <sys/SN/arch.h>

#ifndef MAXCPU		/* we already get this from cpumask.h */
#define MAXCPU		MAXCPUS
#endif	/* MAXCPU */
#include <fault.h>
#endif	/* SN */

#ifdef IP30
#include <sys/RACER/IP30.h>
#endif

#if !defined(MAXCPU)
#define MAXCPU	1
#endif

#if MAXCPU > 1
#define MULTIPROCESSOR 1
#endif

#define YES_EVEREST		0xCAB

#if SN
#define GENERIC_PDASIZE		2048	/* size in bytes of the generic PDA */
#define GENERIC_PDASIZE_2	11	/* log2 of GENERIC_PDASIZE */
#else
#define GENERIC_PDASIZE		512	/* size in bytes of the generic PDA */
#define GENERIC_PDASIZE_2	9	/* log2 of GENERIC_PDASIZE */
#endif


#ifdef _LANGUAGE_C

#include <sys/types.h>
#include <setjmp.h>

/* can have at most 32 instructions */
struct vector10 {
			/*	R3000			R4000		  */
	inst_t	inst1;	/*	lui	k0,0x????	lui	k0,0x???? */
	inst_t	inst2;	/*	addu	k0,0x????	addu	k0,0x???? */
	inst_t	inst3;	/*	lw	k0,0(k0)	lw	k0,0(k0)  */
	inst_t	inst4;	/*	nop			nop		  */
	inst_t	inst5;	/*	jal	k1,k0		jal	k1,k0	  */
	inst_t	inst6;	/*	nop			nop		  */
	inst_t	inst7;	/*	j	k0 		mtc0	k0,C0_EPC */
	inst_t	inst8;	/*	rfe			nop		  */
	inst_t	inst9;	/*				nop		  */
	inst_t	inst10;	/*				eret		  */
};

typedef unsigned long arcs_reg_t;
typedef volatile unsigned long varcs_reg_t;

/*
 * Private Data Area for generic global data used by (nearly) all arcs 
 * standalone programs, primarily pertaining to fault handling.
 * In order to provide consistent assembly offsets, the pda includes
 * architecturally-specific globals, but they are ignored by the 
 * fault-handling code via runtime checks.
 * The 'specific_pda' field allows standalones to create
 * program-specific pdas.
 *
 * This structure and the offsets defined below must match 
 * (libsk/ml/faultasm.s assembly routines use them to access the pdas).
 */

typedef union generic_pda_u {
    volatile struct generic_pda_s {
	varcs_reg_t gpda_magic;		/* marks this pda as valid */
	varcs_reg_t epc_save;		/* pc at the time of the exception */
	varcs_reg_t at_save;		/* cmplr tromps at & v0; save 'em. */
	varcs_reg_t v0_save;
	varcs_reg_t badvaddr_save;	/* if addr exception this was bad VA */
	varcs_reg_t cause_save;		/* C0_CAUSE */
	varcs_reg_t sp_save;		/* save norm sp during faults */
	varcs_reg_t sr_save;		/* C0_SR at time of exception */
	varcs_reg_t exc_save;		/* specific handler sets this */
	varcs_reg_t stack_mode;		/* are we on reg or fault stack now? */
	varcs_reg_t mode_save;		/* process mode (norm/excep) at fault */
	varcs_reg_t my_virtid;		/* virtual (contiguous) processor # */
	varcs_reg_t my_physid;		/* physical (hw-readable) processor # */
	varcs_reg_t notfirst;
	varcs_reg_t first_epc;
	varcs_reg_t retn_addr_save;	/* Save return address of exc hndlr */
	varcs_reg_t *pda_initial_sp;	/* stores sp during process startup */
	varcs_reg_t *pda_fault_sp;	/* pts to fault-handling stack */
	k_machreg_t *regs;		/* saved user_state during exception */
	jmpbufptr   pda_nofault;
	varcs_reg_t cache_err_save;	/* decodes location & type of ECC err */
	varcs_reg_t error_epc_save;	/* if ECC exception, this was the pc */

	/* symmon specific entries from old symmon pda */

	/* NOTE that a k_machreg_t is 8 bytes on EVEREST systems.
	 * Hence there could be an alignment pad between _sv_gp
	 * and its predecessor - currently there is not such an
	 * alignment pad, but beware of adding stuff to
	 * this struct before _sv_gp.
	 */

	/* These are arranged so that the largest entries come first -
	 * k_machreg_t, since that is currently 64 bits on everest, then
	 * next largest - ptrs, since those are 64 bits on 64 bit kernels,
	 * then ints. This makes figuring the constant offsets for
	 * assembler below easier, since you don't have to deal with
	 * alignment gaps, etc.
	 * Only those elements that are used by assemble code are
	 * arranged in this way. Currently, dbg_exc is the last one.
	 */
	k_machreg_t _sv_gp;
	k_machreg_t _sv_ra;
	k_machreg_t _sv_k1;
	k_machreg_t _symmon_sv_at;	/* Slightly redundant with _at_sv
					 * above, but this one is a
					 * k_machreg_t.
					 */
	int	    *dbg_stack;		/* dbgmon stack */
	k_machreg_t *exc_stack;		/* ptr to exc stack frame */
	int	    dbg_mode;		/* MODE_DBGMON,MODE_CLIENT,MODE_IDBG */
	int	    dbg_modesav;
	int	    dbg_exc;		/* EXCEPT_NORM, UTLB, BRKPT */
	/* end of symmon fields that are used in assembly files */

	jmpbufptr   _restart_buf;	/* ptr to jmp_buf to get to _dbgmon */
	jmpbufptr   bp_nofault;
	k_machreg_t *sregs;		/* copy of regs when in idbg mode */
	jmpbufptr   step_jmp;
	int	    step_flag;
	int	    step_count;		/* for step commands */
	int	    (*step_routine)(int);
	__psunsigned_t	    noffst;
	char	    *cbuf;		/* buffer for char input processing */
	int	    column;		/* current console output column */
	int	    saved;
	void (*volatile launch)(void *);	/* routine to execute */
	void	    *launch_arg;	/* arg to this routine */
#if TFP
	__psint_t   save_trapbase[4];
	__psint_t   save_gevector[4];
	__psint_t   save_utlbvector[4];
#else
	inst_t	    *save_utlb[4];
	inst_t	    *save_norm[4];
	inst_t	    *save_xtlb[4];
#endif
	char	    nmi_int_flag;	/* NMI seen on cpu */

#if SN
	struct vector10 p_nmi_save_utlb[4];
	struct vector10 p_nmi_save_norm[4];
	struct vector10 p_nmi_save_xut[4];
	struct vector10 p_nmi_save_ecc[4];

	uint	p_nmi_save_gevector[4];
	uint	p_nmi_save_utlbvector[4];

	uint	p_nmi_saved;
	k_machreg_t excep_regs[NREGS];
#endif

    } gdata;
    char	filler[GENERIC_PDASIZE];	/* any reasonable power of 2 */
} gen_pda_t;
#define GPDA_MAGIC	0xadacab
#ifdef SN
extern gen_pda_t	*gen_pda_tabp[];
#define			GEN_PDA_TAB(vid) (*gen_pda_tabp[vid])
#else
extern gen_pda_t	gen_pda_tab[];
#define			GEN_PDA_TAB(vid) gen_pda_tab[vid]
#endif

#endif /* _LANGUAGE_C */


#if (_USE_FULL_PATH != 0xfeed)
#define _USE_PPTR	0xcad

#include <fault.h>

/*
 *     Files which explicitly include genpda.h can reference the fields
 * of pda structures with ptrs or use the GPRIVATE and GPME macros
 * to do so.  They cannot, however, reference the global variables
 * (which in reality are fields in arrays of pda structs) simply by
 * name.
 *     Files which include fault.h can access the global variables
 * by name--blithely unaware of the underlying pda structure--but
 * therefore cannot explicitly reference any pda fields.
 *     Note that the idempotentcy (#define) protection in both headers
 * prevents double-inclusion problems; the above ordering, however,
 * is a crucial requirement.
 */

#ifdef _LANGUAGE_C

#define	_cpuid()	cpuid()

/*
 * The GPRIVATE macro provides explicit access to the fields in
 * "vid"'s pda, and GPME accesses the processor's own pda.
 */
#define GPRIVATE(_VID,_FIELD)	(GEN_PDA_TAB(_VID).gdata._FIELD)
#define GPME(_FIELD)		(GEN_PDA_TAB(_cpuid()).gdata._FIELD)

#endif /* !_LANGUAGE_ASSEMBLY */

#undef _USE_PPTR
#endif /* _USE_FULL_PATH != 0xfeed */

#ifdef MULTIPROCESSOR

/*
 * Make the running processor's pda address available in k0.
 * Note that k1 is lost in addition to k0 for MP.
 *
 * Some of the things buried in this macro:
 * 	-pda_t is 512 bytes
 *	-ss_to_vid is an array of bytes
 *	-EV_GET_SPNUM returns a "physical cpuid", a 
 *	 number suitable for use as an index into ss_to_vid.
 *
 * For IP21, the slices are numbered 0 and 2 in the SPNUM register,
 * but software referes to them as slices 0 and 1 to avoid confusing
 * users. This means we have to do a little extra work with the
 * contents of the SPNUM register.
 */

#ifdef EVEREST
#if IP21
#define		_get_gpda(dst,scratch)	\
	LI	scratch,EV_SPNUM;	\
	ld	scratch,0(scratch);	\
	and	scratch,EV_SPNUM_MASK; \
	and	dst,scratch,EV_PROCNUM_MASK; \
	srl	dst,EV_PROCNUM_SHFT; \
	srl	scratch,EV_SLOTNUM_SHFT; \
	sll	scratch,EV_SLOTNUM_SHFT; \
	or	scratch,dst; \
	sll	scratch,1; /* ss_to_vid is array of cpuid_t (now a short) */ \
	LA	dst,ss_to_vid; \
	PTR_ADDU dst,scratch; \
	lh	scratch,0(dst); \
	sll	scratch,GENERIC_PDASIZE_2; \
	LA	dst,gen_pda_tab; \
	PTR_ADDU dst,scratch
#endif	/* IP21 */
#if IP19 || IP25
#define		_get_gpda(dst,scratch)	\
	LI	scratch,EV_SPNUM;	\
	ld	scratch,0(scratch);	\
	LA	dst,ss_to_vid; \
	and	scratch,EV_SPNUM_MASK; \
	sll	scratch,1; /* ss_to_vid is array of cpuid_t (now a short) */ \
	PTR_ADDU dst,scratch; \
	lh	scratch,0(dst); \
	sll	scratch,GENERIC_PDASIZE_2; \
	LA	dst,gen_pda_tab; \
	PTR_ADDU dst,scratch
#endif /* IP19 || IP25 */
#elif IP27

#define	GPDA_IALIAS_BASE	0x9200000001000000
#define	PI_CPU_NUM		0x000020
#define	NI_STATUS_REV_ID	0x600000
#define	NI_NODEID_SHFT		8
#define	NI_NODEID_MASK		0x1ff


#define		_get_my_vid(dst,scratch)	\
	LI	scratch,GPDA_IALIAS_BASE+NI_STATUS_REV_ID; \
	ld	dst,(scratch)		;\
	dsrl	dst,NI_NODEID_SHFT	;\
	and	dst,NI_NODEID_MASK	;\
        sll	dst, 1;\
	LI	scratch,GPDA_IALIAS_BASE+PI_CPU_NUM	;\
	ld	scratch,(scratch)	;\
	PTR_ADDU dst,scratch;		\
        sll	dst, 1;\
        LA	scratch, nasid_slice_to_vid;\
        PTR_ADDU dst, scratch; \
        lh	dst, 0(dst)

#define		_get_gpda(dst,scratch)	\
	_get_my_vid(dst, scratch)	;\
	sll	dst,PTR_SCALESHIFT	;\
	LA	scratch,gen_pda_tabp	;\
	PTR_ADDU dst,scratch		;\
	ld	dst, 0(dst)

#elif IP30
#define _get_gpda(dst,scratch)                          \
        CLI     scratch,PHYS_TO_COMPATK1(HEART_PRID);   \
        ld      scratch,0(scratch);                     \
        li      dst,GENERIC_PDASIZE_2;                  \
        sllv    scratch,scratch,dst;                    \
        LA      dst,gen_pda_tab;                        \
        PTR_ADDU dst,scratch
#else
#error genpda.h _get_gpda -- unknown MP CPU!
#endif

#else	/* Non MULTIPROCESSOR Platforms */
#define	_get_gpda(dst,scratch)	\
	LA	dst,gen_pda_tab

#endif /* MULTIPROCESSOR */


#ifdef _LANGUAGE_ASSEMBLY
/*
 * Offsets into Generic PDA.  Must match C structure above.
 */
#define GPDA_MAGIC_OFF		0
#define GPDA_EPC_SV		GPDA_MAGIC_OFF + R_SIZE
#define GPDA_AT_SV		GPDA_EPC_SV + R_SIZE
#define GPDA_V0_SV		GPDA_AT_SV + R_SIZE
#define GPDA_BADVADDR_SV	GPDA_V0_SV + R_SIZE
#define GPDA_CAUSE_SV		GPDA_BADVADDR_SV + R_SIZE
#define GPDA_SP_SV		GPDA_CAUSE_SV + R_SIZE
#define GPDA_SR_SV		GPDA_SP_SV + R_SIZE
#define GPDA_EXC_SV		GPDA_SR_SV + R_SIZE
#define GPDA_STACK_MODE		GPDA_EXC_SV + R_SIZE
#define GPDA_MODE_SV		GPDA_STACK_MODE + R_SIZE
#define GPDA_MY_VIRTID		GPDA_MODE_SV + R_SIZE
#define GPDA_MY_PHYSID		GPDA_MY_VIRTID + R_SIZE
#define GPDA_NOTFIRST		GPDA_MY_PHYSID + R_SIZE
#define GPDA_FIRST_EPC		GPDA_NOTFIRST + R_SIZE
#define GPDA_RTN_ADDR_SV	GPDA_FIRST_EPC + R_SIZE
#define GPDA_INITIAL_SP		GPDA_RTN_ADDR_SV + R_SIZE
#define GPDA_FAULT_SP		GPDA_INITIAL_SP + P_SIZE
#define GPDA_REGS		GPDA_FAULT_SP + P_SIZE
#define GPDA_NOFAULT		GPDA_REGS + P_SIZE
#define GPDA_CACHE_ERR_SV	GPDA_NOFAULT + P_SIZE
#define GPDA_ERROR_EPC_SV	GPDA_CACHE_ERR_SV+ R_SIZE

/* Symmon-specific entries */
#define GPDA_SV_GP		GPDA_ERROR_EPC_SV + R_SIZE
#define GPDA_SV_RA		GPDA_SV_GP + K_MACHREG_SIZE
#define GPDA_SV_K1		GPDA_SV_RA + K_MACHREG_SIZE
#define GPDA_SYMMON_SV_AT	GPDA_SV_K1 + K_MACHREG_SIZE
#define GPDA_DBG_STACK		GPDA_SYMMON_SV_AT + K_MACHREG_SIZE
#define GPDA_EXC_STACK		GPDA_DBG_STACK + P_SIZE
#define GPDA_DBG_MODE		GPDA_EXC_STACK + P_SIZE
#define GPDA_DBG_MODESAV	GPDA_DBG_MODE + (_MIPS_SZINT/8)
#define GPDA_DBG_EXC		GPDA_DBG_MODESAV + (_MIPS_SZINT/8)
#endif	/* _LANGUAGE_ASSEMBLY */

#endif /* __GENPDA_H__ */

