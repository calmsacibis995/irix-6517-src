#ident "symmon/brkpt: $Revision: 1.97 $"
/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * brkpt.c -- support routines for dbgmon breakpoint and single step
 * facilities
 */
/*
 * MP problems:
 * MUST stop ALL other processors while stepping over a bkpnt:
 *	1) brkpt is missing for a while someone might slip past
 *	   brkpt is only removed so we can step pass it. This means
 *	   that there is a window of which other CPU won't see the brkpt.
 *	   On IP5, symmon must stop all CPU while stepping over a brkpt.
 *	2) process MUST not migrate to another processor while
 *	stepping (or dbgmon must gets real smart)
 */

/*
 * TODO:
 *	Need to deal with dbg_error stuff in step1 when running
 *	debug protocol.
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/inst.h>
#include <arcs/spb.h>
#include <arcs/debug_block.h>
#include <setjmp.h>
#include <fault.h>
#include "dbgmon.h"
#include "mp.h"
#include <libsc.h>
#include <libsk.h>
#include <sys/mapped_kernel.h>
#include <sys/cpumask.h>
#ifdef NETDBX
#include "coord.h"
#endif /* NETDBX */
#ifdef SN0
#include <sys/SN/addrs.h>
#include <sys/SN/arch.h>
#include <sys/SN/gda.h>
#endif
#if defined(EVEREST) && defined(MULTIKERNEL)
#include <sys/EVEREST/evconfig.h>
#endif

#if DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif
#if 0
#define MAPPED_KERNEL_DPRINTF(x)	printf x
#else
#define	MAPPED_KERNEL_DPRINTF(x)
#endif

#if R4000 || R10000
static void	handle_wp(int regstyle);
static int	reset_wp(void);
#endif

#ifndef NETDBX
static void	_pdbx_clean(void);
#endif /* NETDBX */
#ifdef MAPPED_KERNEL
static void
install_replicated_brkpt(inst_t *addr, inst_t inst);
#endif /* MAPPED_KERNEL */

static int	is_conditional(unsigned);
static int	is_brkpt(inst_t *);
static int	lookup_bptype(inst_t *);
static inst_t 	*branch_target(unsigned, inst_t *);
static void	suspend_brkpt(inst_t *);

#define	NBREAKPOINTS	32
struct brkpt_table  brkpt_table[NBREAKPOINTS];

/*
 * Types of breakpoints
 *
 * NOTE: order of types is sensitive, see NEW_BRKPT().
 *
 * Suspended breakpoints are breakpoints that are suspended for one
 * instruction to allow continuing past that breakpoint.  They
 * are replaced as soon as execution is past that instruction.
 *
 * Continue breakpoints are placed immediately after the instruction
 * where a suspended breakpoint was, they are placed there so that the
 * suspended breakpoint may be reinstated.
 *
 * Temporary breakpoints are used for "step" and "goto" commands
 * where the breakpoint should be removed as soon as it is taken.
 *
 * Permanent breakpoints are put down by the "brk" command.
 */
#define	BTTYPE_EMPTY	0		/* unused entry */
#define	BTTYPE_SUSPEND	1		/* suspended breakpoint */
#define	BTTYPE_CONT	2		/* continue breakpoint */
#define	BTTYPE_TEMP	3		/* temporary breakpoint */
#define	BTTYPE_PERM	4		/* permanent breakpoint */

extern void _install_brkpts(inst_t *);
static void step1(int);
static void step2(int);
static void bp_continue(int *);
static inst_t getinst_nolock(inst_t *);

int check_overlap_brkpt(numinp_t new_brk_addr);
int generate_continue_brkpts(numinp_t brk_addr, 
			     numinp_t *c1addr, 
                             numinp_t *c2addr);


int autostop=1;		/* controls auto sending of "stop" on breakpoints */
lock_t brkpt_table_lock;
int dbg_spl;
volatile int suspend_cpu=NO_CPUID;
/* the step1_cpu will only be set when symmon is resuming the client for
 * just one instruction.  This may be either a "continue" from a breakpoint
 * to a suspended breakpoint, or may be an "s" command (but NOT "S").
 * We will use this to prevent other cpus from obtaining symmon UI until
 * the "single instruction" has been executed.
 * NOTE: the USER_INTERFACE_LOCK protects the setting of step1_cpu from
 * NO_CPUID to another value.  That cpu may reset the variable without
 * obtaining the USER_INTERFACE_LOCK.
 */
volatile int step1_cpu=NO_CPUID;
volatile int step1_sr_ie=0;
/* Following variable is used to control default operation of the
 * "c" command.  Initial default is to continue all processors, but if the
 * user specifies a parameter then the default changes to the non-specified
 * mode. I.e. If user types "c all" then the default for "c" will be to
 * continue the single cpu which is executing symmon.  If user
 * types "c <cpu#>" then the default for "c" will be "c all"
 */

int default_cont_mode=0;	/* 0 => cont all, 1 => continue self */

void
init_brkpt_lock()
{
	initlock(&brkpt_table_lock);
}
/*
 * _brk -- set breakpoints
 */
/*ARGSUSED*/
int
_autostop(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	register int i;
	numinp_t addr;
	cpumask_t allcpus;

	if (argc == 1) {
		if (autostop)
			printf("autostop %d (stop all cpus on breakpoint)\n", autostop);
		else
			printf("autostop %d (don't stop all cpus on break)\n",autostop);
		return(0);
	}
	if (argc == 2)
		if (*atob(*++argv, &autostop))
			_dbg_error("illegal autostop: %s\n", *argv);

	return(0);
}

/*
 * _brk -- set breakpoints
 * 
 * XXX We need a check here. Before we set a permanent breakpoint at a
 * particular address, we ought to ensure that this doesn't conflict with
 * any continue breakpoints (for any permanent breakpoints) that already
 * do exist, in the breakpoint table. If this test fails, we don't honor
 * the new breakpoint at all.
 */
/*ARGSUSED*/
int
_brk(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
    	int		ret_val;
	register 	int i;
	numinp_t 	addr;
	cpumask_t	allcpus;

	if (argc == 1) {
		printf("#\t    SYMBOL\t      ADDR\t\tINST\t\tTYPE\n");
		DBG_LOCK();
		for (i = 0; i < NBREAKPOINTS; i++) {
			if (brkpt_table[i].bt_type != BTTYPE_EMPTY)
				printf(
				"%d:\t    %s+0x%x    0x%x\t0x%x\t%s%s%s%s\n",
				i, fetch_kname(brkpt_table[i].bt_addr),
				private.noffst, brkpt_table[i].bt_addr,
				brkpt_table[i].bt_inst,
				brkpt_table[i].bt_type ==
					BTTYPE_SUSPEND ? "SUSP" : "",
				brkpt_table[i].bt_type ==
					BTTYPE_CONT ? "CONT" : "",
				brkpt_table[i].bt_type ==
					BTTYPE_TEMP ? "TEMP" : "",
				brkpt_table[i].bt_type ==
					BTTYPE_PERM ? "PERM" : "");
		}
		DBG_UNLOCK();
		return(0);
	}
	CPUMASK_CLRALL( allcpus );		/* all bits clear */
	CPUMASK_CPYNOTM( allcpus, allcpus );	/* all bits set */
	while (--argc > 0) {
		argv++;
		if ( parse_sym(*argv, &addr) && *atob_s(*argv, &addr)) {
			printf("illegal address: %s\n", *argv);
			continue;
		}

		addr = sign_extend(addr);

		/* 
		 * XXX Check overlap of this permanent breakpoint with
		 * old permanent breakpoints' continue breakpoints and
		 * verify that there is no match before installing this.
		 */
#if MULTIPROCESSOR
		ret_val = check_overlap_brkpt(addr);
		if (!ret_val){
			NEW_BRKPT(addr, BTTYPE_PERM, allcpus);

			/* always flush I-cache to prevent occassional
			 * loss of breakpoints
			 */

			flush_cache();

		}
		else {
			printf("_brk: conflict with permanent bp; bp at 0x%llx not accepted\n",
					addr);
		}
#else
		NEW_BRKPT(addr, BTTYPE_PERM, allcpus);

			/* always flush I-cache to prevent occassional
			 * loss of breakpoints
			 */

		flush_cache();

#endif	/* MULTIPROCESSOR */

	}
	return(0);
}


/*
 * check_overlap_brkpt()
 * This routine accepts a new (tentative) permanent breakpoint. It then sees
 * each old permanent breakpoint and generates its corresponding continue brk
 * point. It checks that the new breakpoint doesn't conflict with any of the
 * old permanent breakpoints' continue breakpoint. This routine returns vals:
 *   	0 - continue/new permanent breakpoints don't overlap.
 *	1 - continue/new permanent breakpoints overlap.
 */
int
check_overlap_brkpt(numinp_t new_brk_addr)
{
    	int		bp_num;
	int		ret_val, ret_val_new;
	numinp_t	brk_addr, c1addr, c2addr;
	numinp_t		  a1addr, a2addr;
	struct		brkpt_table	*bt;

	/* Generate this new breakpoint's continue breakpoint addresses.
         * They will have to be compared with permanent and continue
	 * breakpoint addresses of the permanent breakpoints already set.
	 * Only if none of them overlap, the new breakpoint can
	 * be set (accepted).
	 */
	ret_val_new=generate_continue_brkpts(new_brk_addr, &a1addr, &a2addr);
	if (ret_val_new > 0) {
	    DBG_LOCK();
	    for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
	        if (bt->bt_type == BTTYPE_PERM) {
	            /*
		     * Generate this permanent breakpoint's
		     * continue breakpoint addresses and check
	             * them against the new breakpoint's addr.
	             */
	            brk_addr = (numinp_t) bt->bt_addr;
	            ret_val=generate_continue_brkpts(brk_addr, &c1addr, &c2addr);
	            if (ret_val == 0) {
		 	/* Error, bypass this check */
		 	return (0);
	            }
	            else 
	            if (ret_val == 1) {
		 	/* Just one continue breakpoint */
		 	if (new_brk_addr == c1addr) {
				/* Overlap, error */
				DBG_UNLOCK();
				return (1);
			}
			if (ret_val_new == 1) {
			    /* new breakpoint has one continue brkpt address */
			    if (a1addr == brk_addr || a1addr == c1addr ) {
				DBG_UNLOCK();
				return(1);
			    }
			}
			else {
			    /* new breakpoint has two continue breakpoints */
			    if (a1addr == brk_addr || a2addr == brk_addr ||
				a1addr == c1addr   || a2addr == c1addr)    {
				DBG_UNLOCK();
				return(1);
			    }
			}
	            }
	            else
	            if (ret_val == 2) {
			/* Just two continue breakpoints */
			if (new_brk_addr == c1addr || new_brk_addr == c2addr) {
				DBG_UNLOCK();
				return (1);
			}
			if (ret_val_new == 1) {
			    /* new breakpoint has one continue breakpoint */
			    if (a1addr == brk_addr ||
				a1addr == c1addr   || a1addr == c2addr) {
				    DBG_UNLOCK();
				    return(1);
			    }
			}
			else {
			    /* new breakpoint has two continue breakpoints */
			    if ( a1addr == brk_addr || a2addr == brk_addr ||
				 a1addr == c1addr   || a2addr == c1addr   ||
				 a1addr == c2addr   || a2addr == c2addr)    {
				    DBG_UNLOCK();
				    return(1);
			    }
			}
		    }
	        }
	    }
	    DBG_UNLOCK();
	}
	/* No overlaps - success */
	return (0);

}

/*
 * generate_continue_brkpts()
 * This routine accepts the address at which a permanent breakpoint resides.
 * It then generates the continue breakpoints that would be set for this 
 * permanent breakpoint when we attempt to step through this permanent brkpt.
 * For branch instructions, two continue breakpoints might be generated here.
 * They're returned in c1addr and c2 addr. This should be matched with newer
 * permanent breakpoints, and overlaps shouldn't occur if we are to honor the
 * new breakpoints. This routine returns the following values:
 *       0 - Error.
 *	 1 - Generated 1 continue breakpoint,  in c1addr.
 *	 2 - Generated 2 continue breakpoints, in c1addr and c2addr.
 */
int	
generate_continue_brkpts(numinp_t brk_addr, numinp_t *c1addr, numinp_t *c2addr)
{
    	unsigned	instr;
	int		instr_is_branch;
	int		instr_is_conditional;
	inst_t		*branch_pc;

	instr_is_branch = 0;
	instr_is_conditional = 0;

	instr = GETINST(brk_addr);
	if (is_branch(instr))
		instr_is_branch = 1;
	if (is_conditional(instr))
		instr_is_conditional = 1;

	if (!instr_is_branch){
		/*
		 * instr is not branch.
		 */
		*c1addr = brk_addr+4;
		*c2addr = 0;
		return (1);
	}
	else
	if (instr_is_branch && !instr_is_conditional){
		/*
		 * instr is a non-conditional branch.
		 */
		branch_pc = BRANCH_TARGET(instr, brk_addr);
		*c1addr = (numinp_t) branch_pc;
		*c2addr = 0;
		return (1);
	}
	else
	if (instr_is_branch && instr_is_conditional){
		/*
		 * instr is conditional branch.
		 */
		branch_pc = BRANCH_TARGET(instr, brk_addr);
		*c1addr = (numinp_t) branch_pc;
		*c2addr = (numinp_t) brk_addr+8;
		return (2);
	}
	/* Shouldn't reach here */
	/* case 1:  non branch */
	/* case 2:  branch, non conditional */
	/* case 3:  branch, conditional */
	/* Ought to have been collectively exhaustive */
	return (0); 
}

/*
 * _go_to -- set temporary breakpoints and resume
 */
/*ARGSUSED*/
int
_go_to(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	numinp_t addr;

	if (setjmp(private.step_jmp))
		return 0;
	
	if (argc != 2)
		return(0);

	while (--argc > 0) {
		argv++;
		if ( parse_sym(*argv, &addr) && *atob_s(*argv, &addr)) {
			printf("illegal address: %s\n", *argv);
			return 1;	/* should unbrk?? */
		}
		addr = sign_extend(addr);
	}
	private.regs[R_EPC] = addr;
	bp_continue(0);
	/*NOTREACHED*/
}

/*
 * _unbrk -- remove breakpoints
 */
/*ARGSUSED*/
int
_unbrk(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	register struct brkpt_table *bt;
	int bpnum;

	if (argc == 1) {
		printf("clear all doesn't work\n");
		/*_clear_brkpts();*/
		return(0);
	}
	while (--argc > 0) {
		argv++;
		if (*atob(*argv, &bpnum) || bpnum<0 || bpnum>=NBREAKPOINTS
		    || brkpt_table[bpnum].bt_type == BTTYPE_EMPTY) {
			printf("illegal breakpoint number: %s\n", *argv);
			continue;
		}
		DBG_LOCK();
		bt = &brkpt_table[bpnum];
		if (bt->bt_type != BTTYPE_SUSPEND) {
			if (IS_KSEG2(bt->bt_addr) &&
				!IS_MAPPED_KERN_SPACE(bt->bt_addr) &&
				probe_tlb((caddr_t)bt->bt_addr, 0) < 0)
				map_k2seg_addr(bt->bt_addr); /* no return on failure */

#ifdef MAPPED_KERNEL
			install_replicated_brkpt(bt->bt_addr, bt->bt_inst);
#else
			SET_MEMORY(bt->bt_addr, SW_WORD, bt->bt_inst);
#endif
		}
		bt->bt_type = BTTYPE_EMPTY;
		/* if other cpu is stuck here - too bad */
		CPUMASK_CLRALL( bt->bt_cpu );
		DBG_UNLOCK();
	}
	return(0);
}

/*
 * _step -- single instruction execution
 */
/*ARGSUSED*/
int
_step(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{

	if (argc > 2)
		return(1);

	if (setjmp(private.step_jmp)) {
		return 0;
	}

	private.step_flag = 1;
	private.step_count = 1;

	if (argc == 2)
		if (*atob(*++argv, (int *)&private.step_count) ||
		    private.step_count <= 0)
			_dbg_error("illegal count: %s\n", *argv);

	step1(BTTYPE_TEMP);
	/*NOTREACHED*/
}

/*
 * step1 -- implementation routine for step
 */
static void
step1(int bp_type)
{
	unsigned next_inst;
	inst_t *branch_pc;
	cpumask_t mycpumask;

	CPUMASK_CLRALL( mycpumask );
	CPUMASK_SETB( mycpumask, _cpuid() );

	next_inst = GETINST(private.regs[R_EPC]);
	if (is_branch(next_inst)) {

		if (is_branch(GETINST(private.regs[R_EPC]+4)))
			_dbg_error("branch in branch delay slot", 0);

		branch_pc = BRANCH_TARGET(next_inst, private.regs[R_EPC]);
		if (branch_pc == (inst_t *)private.regs[R_EPC]) {
			if (is_conditional(next_inst))
				printf("WARNING: stepping over self-branch\n");
			else
				_dbg_error("can't step over self-branch", 0);
		} else {
			NEW_BRKPT(branch_pc, bp_type, mycpumask);
		}
		if (is_conditional(next_inst)){
			NEW_BRKPT(private.regs[R_EPC]+8, bp_type, mycpumask);
		}
	} else {
		NEW_BRKPT(private.regs[R_EPC] + 4, bp_type, mycpumask);
	}
	private.step_routine = (int (*)())step1;

#ifdef NETDBX
	if (netrdebug)
		goto out;
#endif

	/* save current cpuid AND state of InterruptEnable bit, then clear it
	 * so our single step operation does not get an interrupt.
	 * May need to wait if we already have a pending step1 in progress.
	 * NOTE: This code may get recusively invoked! (i.e. bp_continue will
	 * invoke step1() and we have a nested step1() invocation).
	 */
	while ((step1_cpu != NO_CPUID) && (step1_cpu != _cpuid()))
		;

	if (step1_cpu == NO_CPUID) {
		step1_cpu = _cpuid();
		step1_sr_ie = private.regs[R_SR] & SR_IE;
		private.regs[R_SR] &= ~SR_IE;
	}
#ifdef NETDBX
out:
#endif

	bp_continue(0);
	/*NOTREACHED*/
}

void
single_step(void)
{
	private.step_count = 1;
	step1(BTTYPE_TEMP);
}

/*
 * _Step -- execute single instruction, "stepping over" subroutines
 */
/*ARGSUSED*/
int
_Step(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	if (argc > 2)
		return(1);

	if (setjmp(private.step_jmp))
		return 0;

	private.step_flag = 1;
	private.step_count = 1;

	if (argc == 2)
		if (*atob(*++argv,(int *)&private.step_count) ||
		    private.step_count <= 0)
			_dbg_error("illegal count: %s\n", *argv);

	step2(BTTYPE_TEMP);
	/*NOTREACHED*/
}

/*
 * step2 -- implementation routine for Step
 */
static void
step2(int bp_type)
{
	unsigned next_inst;
	inst_t *branch_pc;
	cpumask_t mycpumask;

	CPUMASK_CLRALL( mycpumask );
	CPUMASK_SETB( mycpumask, _cpuid() );
	next_inst = GETINST(private.regs[R_EPC]);
	if (is_branch(next_inst)) {
		if (is_branch(GETINST(private.regs[R_EPC]+4)))
			_dbg_error("branch in branch delay slot", 0);
		if (is_jal(next_inst))
			NEW_BRKPT(private.regs[R_EPC] + 8, bp_type, mycpumask);
		else {
			branch_pc = BRANCH_TARGET(next_inst, private.regs[R_EPC]);
			if (branch_pc == (inst_t *)private.regs[R_EPC]) {
				if (is_conditional(next_inst))
				    printf("WARNING: stepping self-branch\n");
				else
				    _dbg_error("can't step over self-branch",0);
			} else
				NEW_BRKPT(branch_pc, bp_type, mycpumask);
			if (is_conditional(next_inst))
				NEW_BRKPT(private.regs[R_EPC] + 8, bp_type, mycpumask);
		}
	} else
		NEW_BRKPT(private.regs[R_EPC] + 4, bp_type, mycpumask);
	private.step_routine = (int (*)())step2;
	bp_continue(0);
	/*NOTREACHED*/
}

/*
 * _disass -- disassemble mips instructions
 */
/*ARGSUSED*/
int
_disass(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	int count;
	numinp_t addr;
	register char *cp;
	register int ibytes;
	auto int regstyle;
	static numinp_t saveaddr[MAXCPU];
	static int savecount = 10;
	static int saveaddr_init = 0;
	int id = _cpuid();

	if (!saveaddr_init) {
		for (count = 0; count < MAXCPU; count++)
			saveaddr[count] = K0BASE;
		saveaddr_init = 1;
	}

	count = savecount;
	if (argc == 1)
	    addr = saveaddr[id];
	else if ( argc == 2 ) {
	    if (parse_sym_range(argv[1], &addr, &count, SW_WORD))
		return(1);
	} else
	    return (1);

	if (count == 0)
		count = savecount;

	addr = sign_extend(addr);

	if (addr & 0x3)
		_dbg_error("address not word aligned: %s", argv[1]);

	saveaddr[id] = addr + count * 4;
	savecount = count;

	regstyle = 0;
	cp = getenv("regstyle");
	if (cp)
		atob(cp, &regstyle);

	/* make sure if we're K2 that our xlations are setup */
	if (IS_KSEG2(addr) && !IS_MAPPED_KERN_SPACE(addr)) {
		__psunsigned_t map_addr = addr & ~(NBPC - 1);
		__psunsigned_t last_addr = addr + (4 * count);

		for ( ; map_addr < last_addr; map_addr += NBPC) {
			if (probe_tlb((caddr_t)map_addr, 0)) {
				/* no return on failure */
				map_k2seg_addr((void *)map_addr);
			}
		}
	}
	   
	while (count > 0) {
		ibytes = PRINTINSTRUCTION((inst_t *)addr, regstyle, 0);
		addr += ibytes;
		count -= ibytes >> 2;
	}
	saveaddr[id] = addr;

	return(0);
}

/*
 * On MP systems, used to continue slave processors.
 */
/*ARGSUSED*/
void
_do_continue(int *cont_all)
{
	private.step_flag = 0;
	bp_continue(cont_all);
	/*NOTREACHED*/
}

/*
 * cont -- continue command, transfer control to client code
 */
/*ARGSUSED*/
int
_cont(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
#if MULTIPROCESSOR
	if (argc >= 2) {
		int i;
		int continue_self = 0;

		/* If user specified "all", then continue everyone */
		if (!strcmp(argv[1], "all")) {
			default_cont_mode = 1;	/* "c" now does "c <self>" */
			_cont_slave(NO_CPUID);
			return(0);
		}
		if (!strcmp(argv[1], ".")) {
			default_cont_mode = 0;	/* "c" now does "c all" */
			_cont_slave((int)cpuid());
			return(0);
		}

		/* Continue specified processors */
		for (i=1; i<argc; i++) {
			numinp_t whichcpu;

			if (parse_sym_range(argv[i], &whichcpu, 0, SW_BYTE))
				return(1);

			/* Continue self last thing */
			if (whichcpu == cpuid())
				continue_self = 1;
			else
				_cont_slave((int)whichcpu);
		}

		if (continue_self)
			_cont_slave((int)cpuid());

		return(0);
	}
	/* Following handles "c" command when default is "c all" */
	if (default_cont_mode == 0) {
		_cont_slave(NO_CPUID);
		return(0);
	}
#else /* !MULTIPROCESSOR */
	if (argc != 1)
		return(1);

#endif

	if (setjmp(private.step_jmp))
		return 0;
	
	private.step_flag = 1;

	bp_continue(0);
	/*NOTREACHED*/
}

/*
 * bp_continue -- resume execution dealing with breakpoints
 */
void
bp_continue(int *cont_all)
{
	int bp1, bp2;
	inst_t *pc;
	unsigned inst;
	db_t *db = (db_t *)GDEBUGBLOCK;
	int bit;

retry:
	pc = (inst_t *)private.regs[R_EPC];
	DBG_LOCK();
	inst = getinst_nolock(pc);
	bp1 = is_brkpt(pc);
	bp2 = is_branch(inst) && is_brkpt(pc + 1);
	if (bp1 || bp2) {
#ifdef MULTIPROCESSOR
		/* The following code is intended to prevent multiple cpus from
		 * suspending breakpoints at the same time, which causes a lot
		 * of confusion since the first CONT breakpoint which is hit
		 * restores ALL of the suspended breakpoints.  So we let one
		 * at time do the suspend.
		 * NOTE: Need to check if this cpu is the suspend_cpu since
		 * once we continue the cpu MIGHT not execute the SUSP brkpt
		 * next (could take an interrupt, then enter SYMMON through
		 * another PERM brkpt).
		 */
		if ((suspend_cpu != NO_CPUID) && (suspend_cpu != _cpuid())) {
			DPRINTF(("bp_cont(cpu %d): already susp for %d, WAIT\n",
			       _cpuid(), suspend_cpu));
			DBG_UNLOCK();
			while (suspend_cpu != NO_CPUID)
				;
			DPRINTF(("bp_cont(cpu %d): exit WAIT loop\n",
			       _cpuid()));
			goto retry;
		}
		if (suspend_cpu != NO_CPUID)
			printf("bp_cont(cpu%d): has SUSP brkpts!\n", _cpuid());
		suspend_cpu = _cpuid();
#endif /* MULTIPROCESSOR */	
		if (bp1){
			suspend_brkpt(pc);
		}
		if (bp2){
			suspend_brkpt(pc + 1);
		}

		DBG_UNLOCK();
		step1(BTTYPE_CONT);
		/*NOTREACHED*/
	} else
		DBG_UNLOCK();

	/* MUST, since this may resume client program */
	private.dbg_modesav = MODE_CLIENT;	/* force back to client mode */

#if 1
	/* flush I cache always */
	flush_cache();
#endif

	if (db != NULL && CPUMASK_TSTB(db->db_flush, bit = _cpuid() ) ) {
		/* so kernel won't flush again */
		CPUMASK_CLRB(db->db_flush, bit);		
	}
#if MULTIPROCESSOR
	if (cont_all) {
		/* The following code will check to see if one of
		 * continued slaves has already hit another breakpoint.
		 * If it has, we'll release UI to one of the slaves.
		 * NOTE: Does not close all race conditions, but helps
		 * narrow the gap tremendously.
		 */
		extern cpumask_t waiting_cpus;
		int i;

		if (!CPUMASK_IS_ZERO(waiting_cpus))
			for (i=0; i <MAXCPU; i++)
				if (CPUMASK_TSTB(waiting_cpus,i) &&
				    (i != _cpuid())) {
					TRANSFER_USER_INTERFACE(i);
					_resume_brkpt();
					/*NOTREACHED*/
				}
	}
#endif
	RELEASE_USER_INTERFACE(NO_CPUID);
	_resume_brkpt();
	/*NOTREACHED*/
}

/*
 * new_brkpt -- add new breakpoint to breakpoint table
 */
void
new_brkpt(inst_t *addr, int type, cpumask_t cpu)	/* for which cpus */
{
	register struct brkpt_table *bt, *ebt;
	db_t *db = (db_t *)GDEBUGBLOCK;
	int i;
	int retVal;

	if ((__psint_t)addr & 0x3)
		_dbg_error("not on instruction boundry", 0);
	ebt = NULL;
	DBG_LOCK();
	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
		if (bt->bt_type == BTTYPE_EMPTY && ebt == NULL)
			ebt = bt;
		if (bt->bt_type != BTTYPE_EMPTY && addr == bt->bt_addr) {
			if (bt->bt_type == BTTYPE_SUSPEND)
				_fatal_error("new_brkpt");
			/* 
			 * don't overwrite a TEMP bp with a CONT bp
			 * so that stepping (not continueing) over a
			 * bkpt works
			 */
			if (type > bt->bt_type)
				bt->bt_type = type;
			DBG_UNLOCK();
			if (bt->bt_type == BTTYPE_PERM &&
                    	    bt->bt_addr == addr &&
                            type == BTTYPE_CONT){
			    printf("new_brkpt: conflict with temp bp; bp disregarded\n");
                	}    
			return;
		}
	}
	if (ebt == NULL)
		_dbg_error("breakpoint table full", 0);
	ebt->bt_addr = addr;
	ebt->bt_type = type;
	CPUMASK_CLRALL( ebt->bt_cpu );
	CPUMASK_SETM( ebt->bt_cpu, cpu);
	DBG_UNLOCK();

	if (IS_KSEG2(addr) && !IS_MAPPED_KERN_SPACE(addr) &&
 				probe_tlb((caddr_t)addr, 0) < 0)
		map_k2seg_addr(addr);	/* no return on failure */
	_install_brkpts(addr);

	/* set flags to flush cache */
	/* this should do no harm on SIMPLEX case */
	if (db != NULL) {
		CPUMASK_CLRALL(db->db_flush);
		for (i=0; i<MAXCPU; i++)
			CPUMASK_SETB( db->db_flush, i );
	}
}

#ifdef MAPPED_KERNEL

extern int numnodes;

#define KTEXT_TO_NEW_NASID(_a, _n)					\
	CHANGE_ADDR_NASID(PHYS_TO_K0((__psunsigned_t)_a &		\
				      MAPPED_KERN_PAGEMASK), _n)

/*
 * install_replicated_brkpt - install a breakpoint in each kernel text
 * image.
 */
static void
install_replicated_brkpt(inst_t *addr, inst_t inst)
{
	int i;
#if SN0
	gda_t *gdap = GDA;
	inst_t *physaddr;
	nasid_t nasid;

	if (!gdap->g_ktext_repmask || !IS_MAPPED_KERN_SPACE(addr)) {
		/* Not replicated. */
		SET_MEMORY(addr, SW_WORD, inst);
		return;
	}

	/*
	 * XXX - We might want to have a CPU on each node install the
	 * breakpoints so that we can do fancy things like write-protect
	 * kernel text in physical memory on SN0.  For now, though, we just
	 * go around the TLB.
	 */

	for (i = 0; i < numnodes; i++) {
		MAPPED_KERNEL_DPRINTF(("Cnode %d\n", i));
		if (CPUMASK_TSTB(*(gdap->g_ktext_repmask), i)) {
			nasid = gdap->g_nasidtable[i];
			MAPPED_KERNEL_DPRINTF(("Nasid == %d, addr == 0x%x\n",
						nasid, addr));
			physaddr = (inst_t *)KTEXT_TO_NEW_NASID(addr, nasid);
			MAPPED_KERNEL_DPRINTF(("New addr == 0x%x\n", physaddr));
			SET_MEMORY(physaddr, SW_WORD, inst);
		} else
			MAPPED_KERNEL_DPRINTF(("No kernel text\n"));
	}
#else /* !SN0 */
#if defined(IP19) || defined(IP25)
	inst_t *physaddr;

	MAPPED_KERNEL_DPRINTF(("setting brkpt in %d cells\n", EVCFGINFO->ecfg_numcells));
	for (i=0; i < EVCFGINFO->ecfg_numcells; i++) {
		physaddr = (inst_t *)(K0BASE + 
			(((long long)addr & 0x7fffffff) +
				(EVCFGINFO->ecfg_cell[i].cell_membase << 8)));
		MAPPED_KERNEL_DPRINTF(("cell %d addr 0x%x\n", i, physaddr));
		SET_MEMORY( physaddr, SW_WORD, inst);
	}
#else /* !IP19 */
	<<< Bomb! >>> Need to define this stuff for other platforms.
#endif /* !IP19 */
#endif /* SN0 */
}
#endif /* MAPPED_KERNEL */

/*
 * install_brkpts -- install breakpoints in program text
 */
static struct brkpt_table *bt_save;		/* need copy off the stack */

void
_install_brkpts(inst_t *pc)
{
	register struct brkpt_table *ebt;
	struct brkpt_table *bt;
	jmp_buf bp_buf;
	extern int _kernel_bp[];

	DBG_LOCK();
	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
		bt_save = bt;
		if (bt->bt_type == BTTYPE_EMPTY)
			continue;
		if (bt->bt_addr != pc)
			continue;

		if (setjmp(bp_buf)) {
			/* ugh!! fault of some sort, undo the damage */
			symmon_spl();
			for (ebt = brkpt_table; ebt < bt; ebt++) {
				if (ebt->bt_type == BTTYPE_EMPTY)
					continue;
				if (ebt->bt_type != BTTYPE_SUSPEND) {
#ifdef MAPPED_KERNEL
					install_replicated_brkpt(ebt->bt_addr,
						ebt->bt_inst);
#else
					SET_MEMORY(ebt->bt_addr, SW_WORD,
					   ebt->bt_inst);
#endif
				}
				/* delete all temporary brkpts */
				if (ebt->bt_type == BTTYPE_TEMP
				    || ebt->bt_type == BTTYPE_CONT)
					ebt->bt_type = BTTYPE_EMPTY;
				/* reinstate suspended breakpoints */
				if (ebt->bt_type == BTTYPE_SUSPEND)
					ebt->bt_type = ebt->bt_oldtype;
			}
#ifdef MULTIPROCESSOR
			suspend_cpu = NO_CPUID;
#endif /* MULTIPROCESSOR */
			bt_save->bt_type = BTTYPE_EMPTY;
			DBG_UNLOCK();
			_dbg_error("bad breakpoint address: 0x%x",
				   bt_save->bt_addr);
			/*NOTREACHED*/
		} else {
			private.pda_nofault = bp_buf;
			if (bt->bt_type != BTTYPE_SUSPEND) {
				bt->bt_inst = GET_MEMORY(bt->bt_addr, SW_WORD);
#ifdef MAPPED_KERNEL
				install_replicated_brkpt(bt->bt_addr,
							 *_kernel_bp);
#else
				SET_MEMORY(bt->bt_addr, SW_WORD, *_kernel_bp);
#endif
			}
			private.pda_nofault = 0;
		}
	}
	DBG_UNLOCK();
}

/*
 * validate_brkpts - make sure that breakpoint table has good stuff in it
 * This is called whenever a dbgmon comes on-line
 * It can't just clear everything since another processor may be running
 * NOTE: there certainly are some race conditions here

 * This is called in _dbgmon() when dbgmon comes on line. Normal usage is
 * to use dbgmon to start the kernel. I can't see any race conditions here??
 *						Huy 
 * 
 */
void
validate_brkpts(void)
{
	extern int _kernel_bp[];
	register struct brkpt_table *bt;
	unsigned inst;

	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
		if (bt->bt_type == BTTYPE_EMPTY)
			continue;
		if (bt->bt_type == BTTYPE_SUSPEND)
			/* can't really do anything about these */
			continue;
		inst = GET_MEMORY(bt->bt_addr, SW_WORD);
		if (inst != *_kernel_bp) {
			/* no more breakpoint! */
			bt->bt_type = BTTYPE_EMPTY;
			CPUMASK_CLRALL( bt->bt_cpu );
		}
	}
}

/*
 * getinst -- get instruction at a given pc
 *
 * getinst_nolock performs the brkpt table lookup without obtaining
 *		  the DBG_LOCK (used directly by bp_contiue which
 *		  already has table locked and wants to maintain
 *		  consistent view of table while suspending brkpts
 *		  and setting new temporary breakpoints.
 * getinst	is a wrapper around getinst_nolock which obtains
 *		and release the DBG_LOCK (used by all other callers)
 */

inst_t
getinst_nolock(inst_t *pc)
{
	extern int _kernel_bp[];
	register struct brkpt_table *bt;
	inst_t inst;

	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
		if (bt->bt_type != BTTYPE_EMPTY && bt->bt_addr == pc) {
			return(bt->bt_inst);
		}
	}

	if ((inst = _inst_vec((__psunsigned_t)pc)) != (inst_t)-1) {
		return(inst);
	}

	inst = GET_MEMORY(pc, SW_WORD);
	return(inst);
}

inst_t
getinst(inst_t *pc)
{
	inst_t inst;

	DBG_LOCK();
	inst = getinst_nolock(pc);
	DBG_UNLOCK();
	return(inst);
}

/*
 * lookup_bptype -- search breakpoint table to determine type of
 * breakpoint at pc
 * NOTE: Always return with DBG_LOCK active in case caller
 * needs additional info from brkpt_table.
 */
static int
lookup_bptype(inst_t *pc)
{
	register struct brkpt_table *bt;

	DBG_LOCK();
	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++)
		if (bt->bt_type != BTTYPE_EMPTY && bt->bt_addr == pc) {
			return((bt->bt_type==BTTYPE_SUSPEND) ? 
				bt->bt_oldtype : bt->bt_type);
		}

	return(BTTYPE_EMPTY);
}

/*
 * old_brkpt -- remove a breakpoint pointed to by pc
 * brk table must be locked before calling old_brkpt in MP system.
 */
int
old_brkpt(inst_t *pc)
{
	register struct brkpt_table *bt;

	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++)
		if (bt->bt_type != BTTYPE_EMPTY && bt->bt_addr == pc) {
			if (bt->bt_type != BTTYPE_SUSPEND)
#ifdef MAPPED_KERNEL
				install_replicated_brkpt(bt->bt_addr,
							 bt->bt_inst);
#else
				SET_MEMORY(bt->bt_addr, SW_WORD, bt->bt_inst);
#endif
			bt->bt_type = BTTYPE_EMPTY;
			/* if other cpu is stuck here - too bad */
			CPUMASK_CLRALL( bt->bt_cpu );
			return(0);
		}
	return(-1);
}

/*
 * _bp_fault -- special purpose fault handling for breakpoints
 * transferred to by general fault handler
 */
/*ARGSUSED (no watch point on some systems)*/
void
_bp_fault(int wp)
{
	int regstyle;
	extern int _kernel_bp[];
	register inst_t *bp_pc;
	register struct brkpt_table *bt;
	int bptype_save;
	char *cp;
	int scpu;

#ifdef NETDBX
	if (netrdebug)
		goto old;
#endif
again:
	/* if we were executing a single instruction single step, then wait
	 * for that processor to complete its' single instruction.
	 */
	if (step1_cpu != NO_CPUID)
		if (step1_cpu == _cpuid()) {
			private.regs[R_SR] |= step1_sr_ie;
			step1_cpu = NO_CPUID;
		} else {
			while (step1_cpu != NO_CPUID)
				;
			goto again;
		}

#ifdef NETDBX
old:
#endif
	regstyle = 0;
	cp = getenv("regstyle");
	if (cp)
		atob(cp, &regstyle);

	/* used to be in remove_brkpts - called from exception */
	bp_pc = private.regs[R_CAUSE] & CAUSE_BD ?
			(inst_t *)private.regs[R_EPC] + 1 :
			(inst_t *)private.regs[R_EPC];
	bptype_save = lookup_bptype(bp_pc);
#ifdef MULTIPROCESSOR
	scpu = suspend_cpu;	/* save suspended cpu number */
#endif
	/*
	 * if this was a CONT breakpoint - remove it and search for any
	 * SUSPEND breakpoints and put them back in
	 * If this was a TEMP bkpt - remove it
	 *
	 * DBG_LOCK() is already held from lookup_bptype() invocation.
	 */

	switch (bptype_save) {
	case BTTYPE_TEMP:
		/* must clear ALL TEMP breakpoints since to step over a
		 * branch may have inserted 2 TEMP breakpoints
		 * MUST also undo SUSP since may be single stepping over a
		 * stopped breakpoint
		 */
		for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
			if (bt->bt_type == BTTYPE_TEMP)
				old_brkpt(bt->bt_addr);
			if (bt->bt_type == BTTYPE_SUSPEND) {
				bt->bt_type = bt->bt_oldtype;
#ifdef MAPPED_KERNEL
				install_replicated_brkpt(bt->bt_addr,
							  *_kernel_bp);
#else
				SET_MEMORY(bt->bt_addr, SW_WORD, *_kernel_bp);
#endif
			}
		}
#ifdef MULTIPROCESSOR
		suspend_cpu = NO_CPUID;	/* any suspended brkpts are gone */
#endif /* MULTIPROCESSOR */
		DBG_UNLOCK();
#if R4000 || R10000
		/* First check for watchpoint trip! */
		if (reset_wp()) {
			private.step_flag = 1;
			bp_continue(0);
		}
#endif
		ACQUIRE_USER_INTERFACE();
		PRINTINSTRUCTION(private.regs[R_EPC], regstyle, 1);
		if (--private.step_count > 0) {
			/* both step1() and step2() assume that the
			 * USER_INTERFACE is locked and will
			 * release it.
			 */
			(void)(*private.step_routine)(BTTYPE_TEMP);
			/*NOTREACHED*/
		} else if (private.step_flag) {
			private.step_flag = 0;
			symmon_spl();
			private.dbg_mode = MODE_DBGMON;
			longjmp(private.step_jmp,1);
		}
		break;

	case BTTYPE_EMPTY:
	{
		k_machreg_t a0 = private.regs[R_A0];

		DBG_UNLOCK();
		if ((IS_KSEGDM(a0) || IS_MAPPED_KERN_SPACE(a0)) &&
			(!strncmp((char *)a0,"ring",4) || !strncmp((char *)a0,"zing",4))) {
			private.regs[R_EPC] += 4;
			ACQUIRE_USER_INTERFACE();
#if defined(NO_AUTOSTOP) || !defined(MULTIPROCESSOR)
			printf("Keyboard interrupt\n"); /* hack for ^A */
#else /* !NO_AUTOSTOP && MULTIPROCESSOR */
			/* "ringd" indicates that debug entry was itself the
			 * result of a DEBUG interrupt.  Don't send another
			 * "stop" command.
			 * NOTE: "zing" has same meaning as "ringd".
			 */
			if (strncmp((char *)a0,"ringd",5) &&
			    strncmp((char *)a0, "zing",4)) {
				/* neither "ringd" nor "zing" */
				int stat;
				extern int bp_try_stop_cpus(void);

				if (autostop) {
					stat = bp_try_stop_cpus();
					if (stat)
						printf("Keyboard interrupt (cpu%d)bad return status on stop cmd: %d\n",_cpuid(),stat);
					else 
						printf("Keyboard interrupt (cpu%d) stop cmd issued\n", _cpuid());
					if (scpu != NO_CPUID)
						printf("suspend_cpu is %d\n", scpu);
				} else
					printf("Keyboard interrupt\n"); /* hack for ^A */
			} else {
				printf("(cpu%d) epc = 0x%lx\n", _cpuid(), private.regs[R_EPC]);
				printf("(cpu%d) sp  = 0x%lx\n", _cpuid(), private.regs[R_SP]);
				printf("(cpu%d) symmon STOP cmd interrupt\n",_cpuid());
			}
#endif /* !NO_AUTOSTOP && MULTIPROCESSOR */
			private.step_flag = 0;
		} else if ((IS_KSEGDM(a0) || IS_MAPPED_KERN_SPACE(a0)) &&
			!strncmp((char *)a0,"quiet",4)) {
			private.regs[R_EPC] += 4; /* hack for break on entry */
#if R4000 || R10000
		} else if (wp) {
			ACQUIRE_USER_INTERFACE();
			handle_wp(regstyle);
#if defined(MULTIPROCESSOR) && !defined(NO_AUTOSTOP) 
			if (autostop) {
				int stat;
				extern int bp_try_stop_cpus(void);

				stat = bp_try_stop_cpus();
				if (stat)
					printf("Watchpoint, bad return status on stop cmd: %d\n",
						stat);
				else
					printf("Watchpoint, stop cmd issued");
			} else
				printf("Watchpoint\n");
#endif /* !NO_AUTOSTOP */
#endif
		} else {
			/*
			 * On MP machines, we might hit an Unexpected breakpoint
			 * if one processor comes along just as another processor
			 * is in the midst of continuing after a breakpoint.
			 * Essentially, it means we ran into some really bad
			 * luck and missed the permanent breakpoint that we had
			 * set, but we stopped at the following instruction.
			 * In practice, this shouldn't happen very often except
			 * perhaps, when we set breakpoints in frequently-executed
			 * routines like the clock handler and we continue 
			 * everyone all at once.
			 */
#if MULTIPROCESSOR
			ACQUIRE_USER_INTERFACE();
			if (GET_MEMORY(private.regs[R_EPC], SW_WORD) != *_kernel_bp)
				printf("BRKPT exception on cpu%d but none set, probably CONT brkpt for other cpu\n", _cpuid());
			else {
				/* must be a compiled in breakpoint */
				private.regs[R_EPC] += 4;
				printf("Unknown brkpt on cpu%d, compiled?\n",
					_cpuid());
			}
			goto do_perm;	/* continue as if permanent breakpoint */
#else /* uni-processor */
			private.regs[R_EPC] += 4;
			ACQUIRE_USER_INTERFACE();
			printf("Unexpected breakpoint\n");
			PRINTINSTRUCTION(private.regs[R_EPC], regstyle, 1);
#endif /* uni-processor */
		}
		break;
	}

	case BTTYPE_PERM:
		DBG_UNLOCK();
#if R4000 || R10000
		reset_wp();		/* brk addr can be same as wpt */
#endif
		ACQUIRE_USER_INTERFACE();
#if !defined(MULTIPROCESSOR) || defined(NO_AUTOSTOP)
		printf("Breakpoint\n");
#endif

do_perm:
		PRINTINSTRUCTION(private.regs[R_EPC], regstyle, 1);
#if defined(MULTIPROCESSOR) && !defined(NO_AUTOSTOP) 
		if (autostop) {
			int stat;
			extern int bp_try_stop_cpus(void);

			stat = bp_try_stop_cpus();
			if (stat)
				printf("Breakpoint, bad return status on stop cmd: %d\n",
					stat);
			else
				printf("Breakpoint, stop cmd issued");
		} else
			printf("Breakpoint\n");
#endif /* !NO_AUTOSTOP */
		if (private.step_flag) {
			private.step_flag = 0;
			symmon_spl();
			private.dbg_mode = MODE_DBGMON;
			longjmp(private.step_jmp,1);
		}
		break;

	case BTTYPE_CONT:
#ifdef MULTIPROCESSOR
		if (scpu != _cpuid()) {
			/* If you get here, then this CPU has hit another cpu's
			 * CONT breakpoint.  This means that we missed the PERM
			 * breakpoint which was SUSP so the other cpu could
			 * "continue".  Correct action is to act as if we had
			 * hit the PERM breakpoint.
			 */
			DBG_UNLOCK();
			ACQUIRE_USER_INTERFACE();
			printf("(cpu %d) CONT brkpt for cpu %d\n",
			       _cpuid(), scpu);
			goto do_perm;
		}
		/* we protect setting/clearing of suspend_cpu via DBG_LOCK */
		suspend_cpu = NO_CPUID;

#endif /* MULTIPROCESSOR */		
		old_brkpt(bp_pc); /* remove the CONT breakpoint */

		/* put back any SUSPEND breakpoints, remove any CONT brkpts */

		for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
			if (bt->bt_type == BTTYPE_SUSPEND) {

				bt->bt_type = bt->bt_oldtype;
#ifdef MAPPED_KERNEL
				install_replicated_brkpt(bt->bt_addr,
							 *_kernel_bp);
#else
				SET_MEMORY(bt->bt_addr, SW_WORD, *_kernel_bp);
#endif
			}
			/* need to remove ALL CONT brkpts since we may have set
			 * two of them if we were "continuing" from a branch.
			 */
			if (bt->bt_type == BTTYPE_CONT)
				old_brkpt( bt->bt_addr );
		}

		DBG_UNLOCK();
		_resume_brkpt();
		/*NOTREACHED*/

	case BTTYPE_SUSPEND:
		printf("cpu %d hit a SUSPENDed breakpoint\n", cpuid());

	default:
		DBG_UNLOCK();
		_fatal_error("_bp_fault");
		/*NOTREACHED*/
	}
#ifdef NETDBX
#ifdef MULTIPROCESSOR
	if (netrdebug) {
		printf("bpfault:cpu %d, doing command parser\n");
		/* do_command_parser(); */
	}
#endif
#endif /* NETDBX */
	_restart();
	/*NOTREACHED*/
}

/*
 * is_branch -- determine if instruction can branch
 */
int
is_branch(inst_t a0)
{
	union mips_instruction i;
	i.word = a0;

	switch (i.j_format.opcode) {
	case spec_op:
		switch (i.r_format.func) {
		case jr_op:
		case jalr_op:
			return(1);
		}
		return(0);

	case j_op:
	case jal_op:

	case bcond_op:	/* bltzl, bgezl, bltzal, bgezal + likely counterparts */
	case beq_op:
	case bne_op:
	case blez_op:
	case bgtz_op:
	case beql_op:
	case bnel_op:
	case blezl_op:
	case bgtzl_op:
		return(1);

	case cop0_op:
	case cop1_op:
	case cop2_op:
	case cop3_op:
		switch (i.r_format.rs) {
		case bc_op:
			return(1);
		}
		return(0);
	}
	return(0);
}

/*
 * is_jal -- determine if instruction is jump and link
 */
int
is_jal(inst_t instr)
{
	union mips_instruction i;
	i.word = instr;

	switch (i.j_format.opcode) {
	case spec_op:
		switch (i.r_format.func) {
		case jalr_op:
			return(1);
		}
		return(0);

	case bcond_op:
		switch (i.i_format.rt) {
		case bltzal_op:
		case bgezal_op:
		case bltzall_op:
		case bgezall_op:
			return(1);
		}
		return(0);

	case jal_op:
		return(1);
	}
	return(0);
}

/*
 * is_brkpt -- check is a breakpoint is set at pc
 *
 * Assumes that caller is holding the DBG_LOCK().
 */
int
is_brkpt(inst_t *pc)
{
	register struct brkpt_table *bt;

	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
		switch (bt->bt_type) {

		case BTTYPE_TEMP:
		case BTTYPE_PERM:
		case BTTYPE_CONT:
#if 0
			if (IS_MAPPED_KERN_RO(bt->bt_addr) &&
					MAPPED_KERN_RO_TO_PHYS(bt->bt_addr) ==
					MAPPED_KERN_RO_TO_PHYS(pc)) {
				return(1);
#endif
			if (bt->bt_addr == pc) {
				return(1);
			}
			break;

		case BTTYPE_SUSPEND:
		case BTTYPE_EMPTY:
			break;

		default:
			_fatal_error("is_brkpt");
			/*NOTREACHED*/
		}
	}
	return(0);
}

/*
 * is_conditional -- determine if instruction is conditional branch
 */
static int
is_conditional(unsigned a0)
{
	union mips_instruction i;
	i.word = a0;

	switch (i.j_format.opcode) {
	case beq_op:
	case bne_op:
	case blez_op:
	case bgtz_op:
	case beql_op:
	case bnel_op:
	case blezl_op:
	case bgtzl_op:
		return(1);
	case bcond_op:
		switch (i.i_format.rt) {
		case bltz_op:
		case bgez_op:
		case bltzl_op:
		case bgezl_op:
		case bltzal_op:
		case bgezal_op:
		case bltzall_op:
		case bgezall_op:
			return(1);
		}

	case cop0_op:
	case cop1_op:
	case cop2_op:
	case cop3_op:
		switch (i.r_format.rs) {
		case bc_op:
			return(1);
		}
		return(0);
	}
	return(0);
}

/*
 * branch_target -- calculate branch target
 */
static inst_t *
branch_target(unsigned a0, inst_t *pc)
{
	union mips_instruction i;
	register short simmediate;

	i.word = a0;

	switch (i.j_format.opcode) {
	case spec_op:
		switch (i.r_format.func) {
		case jr_op:
		case jalr_op:
			return((inst_t *)(private.regs[i.r_format.rs]));
		}
		break;

	case j_op:
	case jal_op:
		return( (inst_t *)((((unsigned long)pc+4)&~((1<<28)-1)) | (i.j_format.target<<2)));

	case bcond_op:
		switch (i.i_format.rt) {
		case bltz_op:
		case bgez_op:
		case bltzl_op:
		case bgezl_op:
		case bltzal_op:
		case bgezal_op:
		case bltzall_op:
		case bgezall_op:
			/*
			 * assign to temp since compiler currently
			 * doesn't handle signed bit fields
			 */
			simmediate = i.i_format.simmediate;
			return(pc+1+simmediate);
		}
	case beq_op:
	case bne_op:
	case blez_op:
	case bgtz_op:
	case beql_op:
	case bnel_op:
	case blezl_op:
	case bgtzl_op:
		/*
		 * assign to temp since compiler currently
		 * doesn't handle signed bit fields
		 */
		simmediate = i.i_format.simmediate;
		return(pc+1+simmediate);

	case cop0_op:
	case cop1_op:
	case cop2_op:
	case cop3_op:
		switch (i.r_format.rs) {
		case bc_op:
			/*
			 * kludge around compiler deficiency
			 */
			simmediate = i.i_format.simmediate;
			return(pc+1+simmediate);
		}
		break;
	}
	_fatal_error("branch_target");
	/*NOTREACHED*/
}

/*
 * suspend_brkpt -- temporarily suspend breakpoint in order to continue
 *
 * Assumes that caller is holding the DBG_LOCK().
 */
static void
suspend_brkpt(inst_t *pc)
{
	extern int _kernel_bp[];
	register struct brkpt_table *bt;

	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
		if (bt->bt_type == BTTYPE_EMPTY || bt->bt_addr != pc)
			continue;

		switch (bt->bt_type) {
		case BTTYPE_PERM:
		case BTTYPE_TEMP:
			if (GET_MEMORY(pc, SW_WORD) != *_kernel_bp)
				/* This is impossible if DBG_LOCK is working */
				_fatal_error("suspend_brkpt:no brkpt!");
				/* NOTREACHED */
			bt->bt_oldtype = bt->bt_type;
			bt->bt_type = BTTYPE_SUSPEND;
#ifdef MAPPED_KERNEL
			install_replicated_brkpt(bt->bt_addr, bt->bt_inst);
#else
			SET_MEMORY(bt->bt_addr, SW_WORD, bt->bt_inst);
#endif
			return;

#ifdef MULTIPROCESSOR
		case BTTYPE_CONT:
			/* You may get here if one cpu hits a CONT brkpt
			 * which was set by another CPU.  Rather than returning
			 * a fatal error, just try to resume and continue
			 * executing.
			 */
			printf("DEBUG...(cpu %d)suspend_brkpt: CONT brkpt @pc 0x%x - RESUME (suspend cpu %d)\n",
			       _cpuid(), pc, suspend_cpu);
			if (GET_MEMORY(pc, SW_WORD) != *_kernel_bp)
				_fatal_error("suspend_brkpt:CONT, no brkpt!");
			return;

		case BTTYPE_SUSPEND:
			/* You may get here if one cpu hits a brkpt which
			 * was SUSPEND-ed by another CPU. Rather than returning
			 * a fatal error, just try to resume and continue
			 * executing.
			 */
			printf("DEBUG...suspend_brkpt(cpu %d): SUSP brkpt @pc 0x%x - RESUME\n",
			       _cpuid(), pc);
			if (GET_MEMORY(pc, SW_WORD) == *_kernel_bp)
				_fatal_error("suspend_brkpt:SUSP, has brkpt!");
			return;
#else	/* uni-processor */
		case BTTYPE_SUSPEND:
		case BTTYPE_CONT:
			_fatal_error("suspend_brkpt");
			/*NOTREACHED*/
#endif /* uni-processor */
		}
	}
	_fatal_error("suspend_brkpt:no current brkpt!");
	/* NOTREACHED */
}

/*
 * _check_bp -- called from fault handler to allow debugger
 * a crack at exceptions
 */
void
_check_bp(void)
{

	/*
	 * Check if either the exception entered through the special
	 * debugger restart block breakpoint handler, or if we fielded
	 * a breakpoint ourselves (i.e. the client never changed the
	 * general exception vector).
	 *
	 * If it is a breakpoint, then check and see if remote debugging
	 * is happening, otherwise handle it in _bp_fault().
	 */
#if R4000 || R10000
	if ((private.exc_stack[E_CAUSE] & CAUSE_EXCMASK) == EXC_WATCH) {
		_bp_fault(1);
	}
	else
#endif
	if ( private.dbg_modesav == MODE_CLIENT &&
	    (private.dbg_exc == EXCEPT_BRKPT ||
	     (private.dbg_exc == EXCEPT_NORM &&
	     ((__psint_t)private.exc_stack[E_CAUSE] & CAUSE_EXCMASK) == EXC_BREAK))) {
#ifdef NETDBX
		if (netrdebug) {
			/* may directly call rdebug loop */
			enter_rdebug_loop(1); /* 1 = call rdebug loop */
		}
#endif /* NETDBX */
		if (private.bp_nofault) {
			jmpbufptr jb_ptr;
			/*
			 * remote debug protocol wants to catch this one
			 */
#ifdef NETDBX
			_pdbx_clean(0); /* 0 = don't clean PERM brkpts */
#else
			_pdbx_clean();
#endif /* NETDBX */
			jb_ptr = private.bp_nofault;
			private.bp_nofault = 0;
			private.dbg_mode = MODE_DBGMON;
			longjmp(jb_ptr, 1);
		}
		_bp_fault(0);	/* special handling for client breakpoints */
		/*NOTREACHED*/
	}
}

/*
 * Until pdbx lets dbgmon handle breakpoints - simulate OLD environment
 * e.g. _remove_brkpts()
 */
#ifdef NETDBX
void
_pdbx_clean(int permclean)
#else
static void
_pdbx_clean(void)
#endif /* NETDBX */
{
	register struct brkpt_table *bt;
	register inst_t *bp_pc;
	int bptype_save;

	bp_pc = (unsigned)private.regs[R_CAUSE] & CAUSE_BD ? 
		(inst_t *)(private.regs[R_EPC] + 4) : 
		(inst_t *)(private.regs[R_EPC]);

	bptype_save = lookup_bptype(bp_pc);

	/* DBG_LOCK() already invoked by lookup_bptype() */

	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++) {
		if (bt->bt_type == BTTYPE_EMPTY)
			continue;
#ifdef NETDBX
		if (permclean && bt->bt_type != BTTYPE_SUSPEND) {
#ifdef MAPPED_KERNEL
			install_replicated_brkpt(bt->bt_addr, bt->bt_inst);
#else
			SET_MEMORY(bt->bt_addr, SW_WORD, bt->bt_inst);
#endif
			bt->bt_type = BTTYPE_EMPTY;
			continue;
		}
#else
		/*
		if (bt->bt_type != BTTYPE_SUSPEND)
#ifdef MAPPED_KERNEL
			install_replicated_brkpt(bt->bt_addr, bt->bt_inst);
#else
			SET_MEMORY(bt->bt_addr, SW_WORD, bt->bt_inst);
#endif
		*/
#endif /* NETDBX */
		/*
		 * if we're really stopping, delete all temporary brkpts 
		 * delete "continue" breakpoints, since we must be past
		 * them
		 */
		if ((bt->bt_type == BTTYPE_TEMP && bptype_save != BTTYPE_CONT)
		    || bt->bt_type == BTTYPE_CONT)
			old_brkpt(bt->bt_addr);
		/* reinstate suspended breakpoints */
		/*if (bt->bt_type == BTTYPE_SUSPEND)
			bt->bt_type = bt->bt_oldtype;
		*/
	}
	DBG_UNLOCK();
}

#if R4000 || R10000			/* R4000/R10000 have hardware watchpoint */

static caddr_t watchbp;
static k_machreg_t watchlo;
static k_machreg_t watchhi;
static int watchflags;
#define WATCH_SET	0x01		/* watchpoint is set */
#define WATCH_STEP	0x02		/* set but unset currently for step */

/*ARGSUSED*/
int
_wpt(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	register struct brkpt_table *bt;
	int mode,addr=0;

	if (argc == 1) {
		if (watchflags & WATCH_SET) {
			printf("watchpoint: %s%s at phys addr 0x%Ix\n",
				watchlo&2 ? "r" : "",
				watchlo&1 ? "w" : "",
				watchlo & ~7);
		}
		else
			printf("watchpoint not set.\n");
	}
	else if (argc == 2) {
		mode = 3;		/* rw */
		addr = 1;
	}
	else if (argc == 3) {
		addr = 2;

		if (!strcmp(argv[1],"r"))
			mode = 2;
		else if (!strcmp(argv[1],"w"))
			mode = 1;
		else if (!strcmp(argv[1],"rw"))
			mode = 3;
		else
			return(1);
	}

#if R4600 || R5000
	if (((get_prid() & 0xFF00) == 0x2000) ||
		((get_prid() & 0xFF00) == 0x2100) || 
		((get_prid() & 0xFF00) == 0x2300) || 
		((get_prid() & 0xFF00) == 0x2800)) {
		printf("watchpoints not supported.\n");
		return(0);
	}
#endif
	if (addr) {
		long long val;

		if (!atob_L(argv[addr], &val)) {
			printf("invalid address.\n");
			return(0);
		}

		if (val == 0) {
			printf("unsetting watchpoint.\n");
			watchhi = watchlo = 0;
			_set_register(R_WATCHLO,watchlo);
			_set_register(R_WATCHHI,watchhi);

			/*  Remove breakpoint set if active, and still a
			 * temp (wasn't upgraded with brk cmd).
			 */
			if (watchflags & WATCH_STEP) {
				DBG_LOCK();
				bt = brkpt_table;
				while (bt < &brkpt_table[NBREAKPOINTS]) {
					if (bt->bt_type == BTTYPE_TEMP &&
					    (caddr_t)bt->bt_addr == watchbp)
						old_brkpt(bt->bt_addr);
					bt++;
				}
			}
			DBG_UNLOCK();

			watchflags = 0;

			return(0);
		}

		if (watchflags & WATCH_SET) {
			printf("watchpoint already set!\n");
			return(0);
		}

		if (val & 7)
			printf("warning: address 0x%x not double word "
				"aligned.\n", val);

		watchhi = val >> 32;
		watchlo = (val & ~7) | mode;
		watchflags |= WATCH_SET;

		_set_register(R_WATCHLO,watchlo);
		_set_register(R_WATCHHI,watchhi);
	}

	return(0);
}

static void
handle_wp(int regstyle)
{
	cpumask_t mycpumask;

	printf("Watchpoint tripped 0x%Ix\n",watchlo&~7);

	CPUMASK_CLRALL( mycpumask );
	CPUMASK_SETB( mycpumask, _cpuid() );
	/* unset watchpoint so we can step/continue over it*/
	watchflags |= WATCH_STEP;
	_set_register(R_WATCHLO,0);
	_set_register(R_WATCHHI,0);

	PRINTINSTRUCTION(private.regs[R_EPC], regstyle, 1);
	watchbp=(caddr_t)(private.regs[R_EPC]+4);

	/* set temporary breakpoint (epc can only be a load/store) */
	NEW_BRKPT(watchbp, BTTYPE_TEMP, mycpumask);
}

static int
reset_wp(void)
{
	if (watchflags & WATCH_STEP) {
		watchflags &= ~WATCH_STEP;
		_set_register(R_WATCHLO,watchlo);
		_set_register(R_WATCHHI,watchhi);

		return(private.step_count ? 0 : 1);
	}
	return(0);
}
#endif /* R4000 || R10000 */
