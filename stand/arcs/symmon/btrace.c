#ident "symmon/btrace.c: $Revision: 1.30 $"
#include <fault.h>
#include <setjmp.h>
#include <sys/inst.h>
#include <sys/sbd.h>
#include "dbgmon.h"
#include <libsc.h>
#include <libsk.h>
#include "mp.h"

/*
 * asm stack backtrace facility
 *
 * Stack Frame:
 * oldsp|		|
 *	|  arg n	|
 *	|		|frame_size in addiu
 *	|  arg 1	|
 *	|  locals	|
 *	|  save regs+ra	|
 *	|		|
 *	|  arg build	|
 * sp ->|		|
 *
 * Code:
 *	addiu	$sp, 32
 *	sw	$31, 20($sp)
 * TODO - LEAF procs with no frames!
 */
#define output printf

#define DADDIU_MASK	0x67bd0000	/* daddiu sp, sp, # */
#define ADDIU_MASK	0x27bd0000	/* addiu sp, sp, # */

#define SDRA_MASK	0xffbf0000	/* sd ra, x(sp) */
#define SWRA_MASK	0xafbf0000	/* sw ra, x(sp) */

#define SDA0_MASK	0xffa40000	/* sd a0, x(sp) */
#define SDA1_MASK	0xffa50000	/* sd a1, x(sp) */
#define SDA2_MASK	0xffa60000	/* sd a2, x(sp) */
#define SDA3_MASK	0xffa70000	/* sd a3, x(sp) */
#define SDA4_MASK	0xffa80000	/* sd a4, x(sp) */
#define SDA5_MASK	0xffa90000	/* sd a5, x(sp) */
#define SDA6_MASK	0xffaa0000	/* sd a6, x(sp) */
#define SDA7_MASK	0xffab0000	/* sd a7, x(sp) */

#define SWA0_MASK	0xafa40000	/* sw a0, x(sp) */
#define SWA1_MASK	0xafa50000	/* sw a1, x(sp) */
#define SWA2_MASK	0xafa60000	/* sw a2, x(sp) */
#define SWA3_MASK	0xafa70000	/* sw a3, x(sp) */
#define SWA4_MASK	0xafa80000	/* sw a4, x(sp) */
#define SWA5_MASK	0xafa90000	/* sw a5, x(sp) */
#define SWA6_MASK	0xafaa0000	/* sw a6, x(sp) */
#define SWA7_MASK	0xafab0000	/* sw a7, x(sp) */

#if _MIPS_SIM == _ABI64
#define ADDSP_MASK	DADDIU_MASK
#define STRA_MASK	SDRA_MASK
#define SW_REGSZ	SW_DOUBLEWORD
#define NARGS		8
#elif _MIPS_SIM == _ABIN32
#define ADDSP_MASK	ADDIU_MASK
#define STRA_MASK	SDRA_MASK
#define SW_REGSZ	SW_DOUBLEWORD
#define NARGS		8
#elif _MIPS_SIM == _ABIO32
#define ADDSP_MASK	ADDIU_MASK
#define STRA_MASK	SWRA_MASK
#define SW_REGSZ	SW_WORD
#define NARGS		4
#else
<<BOMB>>
#endif

#define INST_NOTFOUND	((inst_t *)(__psunsigned_t)-1)

unsigned int arg_inst[] = {
	SWA0_MASK, SWA1_MASK, SWA2_MASK, SWA3_MASK,
	SWA4_MASK, SWA5_MASK, SWA6_MASK, SWA7_MASK /*only used in 64 bit mode*/
};

/* in 64 bit mode, search for either sd or sw */
unsigned int arg_inst2[] = {
	SDA0_MASK, SDA1_MASK, SDA2_MASK, SDA3_MASK,
	SDA4_MASK, SDA5_MASK, SDA6_MASK, SDA7_MASK
};

int arg_sizes[2] = {
	SW_WORD,		/* arg size if we find sw instruction */
	SW_DOUBLEWORD		/* arg size if we find sd instruction */
};

/* XXX this is gross */
#define LBOUND	(inst_t *)PHYS_TO_K0(0x41000) /* for MP kernel */

static unsigned fetch_text(inst_t *);

enum direction	{ FORWARD, BACKWARD };
enum sign	{ POSITIVE, NEGATIVE };

static inst_t *txtsrch(inst_t *, unsigned int, unsigned int, int *,
			enum sign, int *, inst_t *, enum direction);

static inst_t *branch_target(union mips_instruction, inst_t *);

int bdeb = 0;

extern char *nptr;
extern inst_t *lowest_text;

/*
 * Defines the maximum number of instructions in a function through which
 * we can backtrace.  While looking for the start of a function, we
 * won't look more than MAX_FUNC_SIZE bytes back, and we won't look
 * beyond the start of text (if we know what it is).
 *
 * This constant can be made "arbitrarily" large.  If it's too large,
 * we can "hang" for a long time on some stack walkbacks.  If it's
 * too small, we won't be able to walkback through large routines.
 */
#define MAX_FUNC_SIZE 0x1000

int
btrace(
	inst_t *pc,	/* current pc */
	k_machreg_t sp,	/* current sp */
	inst_t *lra,	/* if starting at LEAF use this ra */
	int max,	/* max # frames */
	inst_t *lbound	/* lower bound of pc */
)
{
	register inst_t *ra;		/* return address */
	register inst_t *spinc;		/* addr of  (d)addiu sp */
	register inst_t *rast;		/* addr of  sw/sd ra */
	register inst_t *tmp;
	union mips_instruction inst;
	auto int frsize;		/* frame size */
	auto int raoff;			/* ra stack offset */
	auto int argoff;		/* arg stack offset */
	register int i;
	int first = 1;
	int which;

	if ((__psunsigned_t)pc & 0x3 || sp & 0x3) {
		output("pc:0x%x sp:0x%Ix must be word aligned\n", pc, sp);
		return(-1);
	}
	if (bdeb)
		output("initial pc:0x%x sp:0x%Ix ra:0x%x\n", pc, sp, lra);
	output("  SP\t\t\tFROM\t\t\t\tFUNC()\n");

	while (max--) {
		inst_t *fn_start_limit;

		/* search for sp inc */
		fn_start_limit = pc -  MAX_FUNC_SIZE;
		if (fn_start_limit < lbound)
			fn_start_limit = lbound;
			
		if ((spinc = txtsrch(pc, ADDSP_MASK, ADDSP_MASK, &which,
				NEGATIVE, &frsize, fn_start_limit, BACKWARD))
		   == INST_NOTFOUND) {
			output("Can't find [D]ADDIU from 0x%x\n", pc);
			return(0);	/* not really an error */
		}

		/* try and control LEAF() functions. */
		if (first && lra) {
			if ((nptr=fetch_kname(spinc)) &&
			    ((caddr_t)lra < fetch_kaddr(nptr))) {
				rast = INST_NOTFOUND;
				spinc = pc;
				frsize = 0;
			}
		}

		/* search for store of ra instruction
		 * if the current pc is at the store ra instruction
		 * then use passed ra (first function only)
		 */
		if (frsize != 0) {
			rast = txtsrch(spinc, STRA_MASK, STRA_MASK, &which,
					POSITIVE, &raoff, pc, FORWARD);
		}
		if ((rast == INST_NOTFOUND) || (rast == pc)) {
			if (!first || lra == 0) {
				output("0x%x", pc);
				if ( (nptr = fetch_kname(pc)) != (char *)0 )
				    output("\t\t\t%s()\n",nptr);
				else
				    output("\t\t\t%llx()\n",pc);
				return(0);
			}
			ra = lra;
			if (bdeb)
				output("Using passed ra:0x%x\n", ra);
		} else {
			ra = (inst_t *)GET_MEMORY((sp + raoff), SW_REGSZ);
			if (bdeb)
				output("ra:0x%x sp+raoff:0x%Ix\n", ra, sp + raoff);
		}
		if (ra == 0)
			goto done;
		first = 0;

		if (!is_jal(fetch_text(ra-2))) {

			output("Ra:0x%x doesn't follow a jal?:0x%x\n",
				ra, fetch_text(ra-2));
			return(-1);
		}

		/* if possible check real function start */
		*(unsigned *)&inst = fetch_text(ra-2);
		if ((tmp = branch_target(inst, ra-2)) != 0) {
			if (tmp != spinc && frsize) {
				/* NOTE: ragnarok does not always do this */
				output("jal target:0x%x != addiu sp:0x%x\n", tmp, spinc);
				/* 
			 	* We'll treat this as the start
			 	* of the function.
			 	*/
				spinc = tmp;
			}
		}

		/* stack frame */ 
		output("0x%Ix",sp);
		/* called from */
		if ( (nptr = fetch_kname(ra-2)) != (char *)0 ) {
			output("\t%s+0x%x ",nptr,private.noffst);
		} else
			output("\t0x%x",ra-2);
		/* func() */
		if ((nptr = fetch_kname(spinc)) != (char *)0) {
			output("\t\t%s(",nptr);
		} else
			output("\t\t0x%x(",spinc);

		/* try to find out args - at least up to the 1st four */
		for (i = 0; i < NARGS; i++) {
			if ((tmp = txtsrch(spinc, arg_inst[i], arg_inst2[i],
					&which, POSITIVE, &argoff,
					pc, FORWARD)) == INST_NOTFOUND) {
				if (bdeb)
					output("Can't find STA? from 0x%x\n", spinc);
				break;
			}
			output("0x%x, ", (unsigned long)GET_MEMORY(sp + argoff, arg_sizes[which]));
		}
		output(")\n");

		/* if we're at the addiu - then don't increment since
		 * that instruction hasn't been execeuted yet!
		 */
		if (pc != spinc)
			sp -= frsize;
		pc = ra;
		if (bdeb)
			output("New sp:0x%Ix pc:0x%x\n", sp, pc);
	}
	output("--More--\n");
done:
	return(0);
}

/*
 * txtsrch - search for an instruction
 * Returns:
 * != -1 on success == pc of found instruction
 * -1 of failure
 */
static inst_t *
txtsrch(
	inst_t *pc,			/* start search point */
	unsigned int instmask,		/* instruction to search for */
	unsigned int inst2mask,		/* alt instruction to search for */
	int *which,			/* which one did we match? */
	enum sign immediate_sign,	/* desired sign of immediate value */
	int *immed,			/* value in instruction */
	inst_t *llimit,			/* stop looking here */
	enum direction dir		/* direction of search */
)
{
	__psint_t incr;
	inst_t instruction;
	int immediate_value;
	int tries = 0;
	unsigned int found2 = 0;

	if (dir == BACKWARD)
		incr = -1;
	else
		incr = 1;

	*which = 2;	/* no match yet */
	for (; pc != llimit; pc += incr) {
		instruction = fetch_text(pc);
		immediate_value = (int) ((short)(instruction & 0xffff));
		if (((instruction & 0xffff0000) == instmask) ||
		    ((found2 = (instruction & 0xffff0000)) == inst2mask)) {
			/* found one or the other */
			if (immediate_sign == POSITIVE) {
				if (immediate_value < 0)
					continue;
			} else {
				if (immediate_value > 0)
					continue;
			}
			*immed = immediate_value;
			if (found2 == inst2mask)
				*which = 1;	/* matched 2nd one */
			else
				*which = 0;	/* matched the 1st one */
			if (bdeb)
				output("found %x(%x) at %x\n", instruction, *which, pc);
			return(pc);
		}
		if (++tries > 1000)
			return(INST_NOTFOUND);
	}
	return(INST_NOTFOUND);
}

/*
 * branch_target -- calculate branch target
 */
static inst_t *
branch_target(union mips_instruction i, inst_t *pc)
{
	register short simmediate;

	switch (i.j_format.opcode) {
	case spec_op:
		switch (i.r_format.func) {
		case jr_op:
		case jalr_op:
			return((inst_t *)0);
		}
		break;

	case bcond_op:
		switch (i.i_format.rt) {
		case bltzal_op:
		case bgezal_op:
		case bltzall_op:
		case bgezall_op:
			/*
		 	 * assign to temp since compiler currently
		 	 * doesn't handle signed bit fields
		 	 */
			simmediate = i.i_format.simmediate;
			return(pc+4+(simmediate<<2));
		}
		break;

	case j_op:
	case jal_op:
		return (inst_t *)((((__psunsigned_t)pc+4)&~((1<<28)-1))
					| (i.j_format.target<<2));
	}
	return(0);
}

/*
 * fetch_text - get an instruction from text space
 */
static unsigned
fetch_text(inst_t *pc)
{
	return(getinst(pc));
}

/*ARGSUSED*/
int
_dobtrace(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	jmp_buf pi_buf;
	int max = 20;
	inst_t *lowest_pc  = lowest_text;

	if (argc > 1) {
		if (*atob(argv[1], &max)) {
			printf("illegal number: %s\n", argv[1]);
			return(1);
		}
	}
	if (setjmp(pi_buf)) {
		symmon_spl();
		private.pda_nofault = 0;
		printf("Bad Access\n");
		return(0);
	}
	private.pda_nofault = pi_buf;
	max = btrace((inst_t *)(private.regs[R_EPC]), 
		private.regs[R_SP], 
		(inst_t *)private.regs[R_RA], 
		max, 
		((lowest_pc==(inst_t *)0) ? LBOUND : lowest_pc));
	private.pda_nofault = 0;
	return(max);
}
