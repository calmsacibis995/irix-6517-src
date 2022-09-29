#ident "symmon/disMips.c: $Revision: 1.38 $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * disMips.c -- MIPS Instruction Printer
 */

#include <sys/types.h>
#include <setjmp.h>
#include <sys/inst.h>

#ifndef PROM
#include <saioctl.h>
#include <libsc.h>
#include "dbgmon.h"
#include "mp.h"
#endif

#ifdef PROM
#include "libc.h"

extern inst_t getinst(inst_t *pc);
#endif

typedef int		boolean;

#define true	1
#define false	0

/* register definitions */
#define ZERO 0
#define GP 28
#define FP 30

static void printhex(unsigned int);

static char *op_name[64] = {
  "spec",  "bcond", "j",     "jal",   "beq",   "bne",   "blez",  "bgtz",
  "addi",  "addiu", "slti",  "sltiu", "andi",  "ori",   "xori",  "lui",
  "cop0",  "cop1",  "cop2",  "op13",  "beql",  "bnel",  "blezl", "bgtzl",
  "daddi", "daddiu","ldl",   "ldr",   "op1c",  "op1d",  "op1e",  "op1f",
  "lb",    "lh",    "lwl",   "lw",    "lbu",   "lhu",   "lwr",   "lwu",
  "sb",    "sh",    "swl",   "sw",    "sdl",   "sdr",   "swr",   "cache",
  "ll",    "lwc1",  "lwc2",  "pref",  "lld",   "ldc1",  "ldc2",  "ld",
  "sc",    "swc1",  "swc2",  "op3b",  "scd",   "sdc1",  "sdc2",  "sd" 
};
  
static char *spec_name[64] = {
  "sll",   "mov",   "srl",   "sra",   "sllv",  "spec05","srlv",  "srav",
  "jr",    "jalr",  "movz",  "movn",  "syscall","break","spim",  "sync",
  "mfhi",  "mthi",  "mflo",  "mtlo",  "dsllv","spec15","dsrlv",  "dsrav",
  "mult",  "multu", "div",   "divu",  "dmult", "dmultu","ddiv",  "ddivu",
  "add",   "addu",  "sub",   "subu",  "and",   "or",    "xor",   "nor",
  "spec28","spec29","slt",   "sltu",  "dadd",  "daddu", "dsub",  "dsubu",
  "tge",   "tgeu",  "tlt",   "tltu",  "teq",   "spec35","tne",   "spec37",
  "dsll",  "spec39","dsrl",  "dsra",  "dsll32", "spec3d","dsrl32", "dsra32" 
};

static char *bcond_name[32] = {
  "bltz",    "bgez",    "bltzl",   "bgezl",
  "spimi",   "bcond05", "bcond06", "bcond07",
  "tgei",    "tgeiu",   "tlti",    "tltiu",
  "teqi",    "bcond0d", "tnei",    "bcond0f",
  "bltzal",  "bgezal",  "bltzall", "bgezall",
  "bcond14", "bcond15", "bcond16", "bcond17",
  "bcond18", "bcond19", "bcond1a", "bcond1b",
  "bcond1c", "bcond1d", "bcond1e", "bcond1f" 
};

static char *cop1func_name[64] = {
  "add",   "sub",   "mul",   "div",   "sqrt",  "abs",   "mov",   "neg",
  "round.l", "trunc.l", "ceil.l", "floor.l", 
  "round.w", "trunc.w", "ceil.w", "floor.w",
  "fop10", "mov",   "movz",  "movn",  "fop14", "recip", "rsqrt", "fop17",
  "fop18", "fop19", "fop1a", "fop1b", "fop1c", "fop1d",	"fop1e", "fop1f",
  "cvt.s", "cvt.d", "cvt.e", "fop23", "cvt.w", "cvt.l",	"fop26", "fop27",
  "fop28", "fop29", "fop2a", "fop2b", "fop2c", "fop2d",	"fop2e", "fop2f",
  "c.f",   "c.un",  "c.eq",  "c.ueq", "c.olt", "c.ult",	"c.ole", "c.ule",
  "c.sf",  "c.ngle","c.seq", "c.ngl", "c.lt",  "c.nge",	"c.le",  "c.ngt" 
};

static char *bc_name[32] = {
  "f",   "t",   "fl",  "tl",
  "x04", "x05", "x06", "x07",
  "x08", "x09", "x0a", "x0b",
  "x0c", "x0d", "x0e", "x0f",
  "x10", "x11", "x12", "x13",
  "x14", "x15", "x16", "x17",
  "x18", "x19", "x1a", "x1b",
  "x1c", "x1d", "x1e", "x1f" 
};
static char *c0func_name[64] = {
  "op00", "tlbr", "tlbwi","op03", "op04", "op05", "tlbwr","op07",
  "tlbp", "op9",  "op10", "op11", "op12", "op13", "op14", "op15",
  "rfe",  "op17", "op18", "op19", "op20", "op21", "op22", "op23",
  "eret", "op25", "op26", "op27", "op28", "op29", "op30", "op31",
  "op32", "op33", "op34", "op35", "op36", "op37", "op38", "op39",
  "op40", "op41", "op42", "op43", "op44", "op45", "op46", "op47",
  "op48", "op49", "op50", "op51", "op52", "op53", "op54", "op55",
  "op56", "op57", "op58", "op59", "op60", "op61", "op62", "op63"
};

static char *c0mfunc_name[64] = {
  "c0m00", "tlbr1", "tlbw",  "c0m03", "c0m04", "c0m05", "c0m06", "c0m07",
  "tlbp1", "dctr",  "dctw",  "c0m11", "c0m12", "c0m13", "c0m14", "c0m15",
  "c0m16", "c0m17", "c0m18", "c0m19", "c0m20", "c0m21", "c0m22", "c0m23",
  "c0m24", "c0m25", "c0m26", "c0m27", "c0m28", "c0m29", "c0m30", "c0m31",
  "c0m32", "c0m33", "c0m34", "c0m35", "c0m36", "c0m37", "c0m38", "c0m39",
  "c0m40", "c0m41", "c0m42", "c0m43", "c0m44", "c0m45", "c0m46", "c0m47",
  "c0m48", "c0m49", "c0m50", "c0m51", "c0m52", "c0m53", "c0m54", "c0m55",
  "c0m56", "c0m57", "c0m58", "c0m59", "c0m60", "c0m61", "c0m62", "c0m63"
};

static char *c0reg_name[32] = {
#if TFP
  "tlbset","c0r1","tlblo","c0r3","ubase","shiftamt","trapbase","badpaddr",
  "badvaddr","count","tlbhi","c0r11","sr", "cause","epc","prid",
  "config","c0r17","work0","work1","pbase","gbase","c0r22","c0r23",
  "wired","c0r25","c0r26","c0r27","dcache","icache","c0r30","c0r31"
#else
  "index","random","tlblo","tlblo1","context","pagemask","wired","c0r7",
  "badvaddr","count","tlbhi","compare","sr", "cause","epc",  "prid",
  "config","lladdr","watchlo","watchhi",
#ifdef R10000
  "xcontext","framemask","brdiag","c0r23","c0r24","perf",
#else
  "c0r20","c0r21","c0r22","c0r23","c0r24","c0r25",
#endif
  "ecc","cacheerr","taglo","taghi","errorepc","c0r31"
#endif
};

static char *cop1xfunc_name[64] = {
  "lwxc1",  "ldxc1",  "c1x02",  "c1x03", "c1x04", "c1x05", "c1x06", "pfetch",
  "swxc1",  "sdxc1",  "c1x10",  "c1x11", "c1x12", "c1x13", "c1x14", "c1x15",
  "c1x16",  "c1x17",  "c1x18",  "c1x19", "c1x20", "c1x21", "c1x22", "c1x23",
  "c1x24",  "c1x25",  "c1x26",  "c1x27", "c1x28", "c1x29", "c1x30", "c1x31",
  "madd.s", "madd.d", "madd.e", "c1x35", "c1x36", "c1x37", "c1x38", "c1x39",
  "msub.s", "msub.d", "msub.e", "c1x43", "c1x44", "c1x45", "c1x46", "c1x47",
  "nmadd.s","nmadd.d","nmadd.e","c1x51", "c1x52", "c1x53", "c1x54", "c1x55",
  "nmsub.s","nmsub.d","nmsub.e","c1x59", "c1x60", "c1x61", "c1x62", "c1x63",
};

static char *c1fmt_name[16] = {
	"s",	"d",	"e",	"q",
	"w",	"l",	"fmt6",	"fmt7",
	"fmt8",	"fmt9",	"fmta",	"fmtb",
	"fmtc",	"fmtd",	"fmte",	"fmtf"
};

static char *sbregister_name[2][32] = {
	{	/* compiler names */
		"zero",	"at",	"v0",	"v1",	"a0",	"a1",	"a2",	"a3",
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
		"t0",	"t1",	"t2",	"t3",	"t4",	"t5",	"t6",	"t7",
#elif (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
		"a4",	"a5",	"a6",	"a7",	"t0",	"t1",	"t2",	"t3",
#endif
		"s0",	"s1",	"s2",	"s3",	"s4",	"s5",	"s6",	"s7",
		"t8",	"t9",	"k0",	"k1",	"gp",	"sp",	"fp",	"ra"
	},
	{	/* hardware names */
		"r0",	"r1",	"r2",	"r3",	"r4",	"r5",	"r6",	"r7",
		"r8",	"r9",	"r10",	"r11",	"r12",	"r13",	"r14",	"r15",
		"r16",	"r17",	"r18",	"r19",	"r20",	"r21",	"r22",	"r23",
		"r24",	"r25",	"r26",	"r27",	"gp",	"sp",	"fp",	"r31"
	}
};

static char *
register_name(unsigned int ireg, int regstyle, int *regcount, int *regnum)
{
	int	i;

	for (i = 0; i < *regcount; i++)
		if (regnum[i] == ireg)
			break;
	if (i >= *regcount)
		regnum[(*regcount)++] = ireg;
	return (sbregister_name[regstyle][ireg]);
}

static char *
fp_register_name(unsigned int r)
{
	static char *name[32] = {
		"$f0",  "$f1",  "$f2",  "$f3",  "$f4",  "$f5",  "$f6",  "$f7",
		"$f8",  "$f9",  "$f10", "$f11", "$f12", "$f13", "$f14", "$f15",
		"$f16", "$f17", "$f18", "$f19", "$f20", "$f21", "$f22", "$f23",
		"$f24", "$f25", "$f26", "$f27", "$f28", "$f29", "$f30", "$f31" 
	};
	return name[r];
}

static char *
c0_register_name(unsigned r)
{
	return c0reg_name[r];
}

#ifndef PROM
void
_show_inst(inst_t *epc)
{
	int regstyle;

	atob(getenv("regstyle"), &regstyle);
	_PrintInstruction(epc, regstyle, 1);
}
#endif

#ifdef PROM
extern char *prom_fetch_kname(inst_t *, char *, int);
#define GETNAME(addr, jmp) \
	(nptr = prom_fetch_kname((inst_t *)(addr), buf, jmp)) != (char *)0
#else
#define GETNAME(addr, jmp) \
	(nptr = fetch_kname((inst_t *)(addr))) != (char *)0
#endif

#define REG_NAME(ireg) \
	register_name((ireg), regstyle, &regcount, regnum)

#ifndef PROM
char *nptr;
#endif

int
_PrintInstruction(inst_t *iadr, int regstyle, int showregs)
{
	boolean do_b_displacement = false;
	boolean do_loadstore = false;
	union mips_instruction i;
	int print_next;
	int ireg;
	int ibytes;
	short simmediate;
#ifdef PROM
	jmp_buf pi_buf;
#else
	static jmp_buf pi_buf;
#endif
	char buf[80];
#ifdef PROM
	char *nptr;
#endif
	int regcount;		/* how many regs used in this inst */
	int regnum[6];		/* which regs used in this inst */

	regcount = 0;
	print_next = 0;
	ibytes = 4;
	regstyle = (regstyle) ? 1 : 0;	/* make sure its valid */

	if ( GETNAME(iadr, 0) ) {
#ifndef PROM
		if (private.noffst)
			printf("[%s+0x%x, 0x%x]: \t",
			       nptr, private.noffst, iadr);
		else
#endif
			printf("[%s, 0x%x]: \t", nptr, iadr);
	}
	else
		printf("0x%x: \t", iadr);

	if (setjmp(pi_buf)) {
#ifndef PROM
		symmon_spl();
#endif
		printf("inaccessible\n");
		return(ibytes);
	}
#ifndef PROM
	private.pda_nofault = pi_buf;
#endif
	i.word = getinst(iadr);
#ifndef PROM
	private.pda_nofault = 0;
#endif
	if (!showregs) {
		printhex(i.word);
		putchar('\t');
	}


	switch (i.j_format.opcode) {

	case spec_op:
		if (i.word == 0) {
			printf("nop");
			break;
		} else if ((i.r_format.func == addu_op || 
			    i.r_format.func == daddu_op ||
			    i.r_format.func == or_op) && 
			   i.r_format.rt == ZERO) {
			if (i.r_format.func == addu_op &&
			    i.r_format.rd == ZERO && 
			    i.r_format.rs == ZERO )
				printf("nada");
			else
				printf("move\t%s,%s",
				REG_NAME(i.r_format.rd),
				REG_NAME(i.r_format.rs));
			break;
		} else if ((i.r_format.func == sll_op) &&
			   (i.r_format.rd == ZERO) &&
			   (i.r_format.rs == ZERO) &&
			   (i.r_format.re == 1 )) {
			printf("ssnop" );
			break;
		}
		printf(spec_name[i.r_format.func]);

		switch (i.r_format.func) {
		case dsll_op:
		case dsrl_op:
		case dsra_op:
		case dsll32_op:
		case dsrl32_op:
		case dsra32_op:
			printf("\t%s,%s,#%x",
				REG_NAME(i.r_format.rd),
				REG_NAME(i.r_format.rt),
				i.r_format.re + (i.r_format.rs << 5));
			break;
		case sll_op:
		case srl_op:
		case sra_op:
			printf("\t%s,%s,0x%x",
				REG_NAME(i.r_format.rd),
				REG_NAME(i.r_format.rt),
				i.r_format.re);
			break;
		case sllv_op:
		case srlv_op:
		case srav_op:
		case dsllv_op:
		case dsrlv_op:
		case dsrav_op:
			printf("\t%s,%s,%s",
				REG_NAME(i.r_format.rd),
				REG_NAME(i.r_format.rt),
				REG_NAME(i.r_format.rs));
			break;
		case movc_op:
			printf("%c\t%s,%s,$fcc0x%x",
				      (i.r_format.rt & 1) ? 't' : 'f',
				      REG_NAME(i.r_format.rd),
				      REG_NAME(i.r_format.rs),
				      i.r_format.rt >> 2);
			break;
		case movz_op:
		case movn_op:
			printf("\t%s,%s,%s",
				      REG_NAME(i.r_format.rd),
				      REG_NAME(i.r_format.rs),
				      REG_NAME(i.r_format.rt));
			break;
		case mfhi_op:
		case mflo_op:
			printf("\t%s",
				REG_NAME(i.r_format.rd));
			break;
		case jalr_op:
			print_next = 1;
			printf("\t%s,%s",
				REG_NAME(i.r_format.rd),
				REG_NAME(i.r_format.rs));
			break;
		case jr_op:
			print_next = 1;
			printf("\t%s",
				REG_NAME(i.r_format.rs));
		break;

		case mtlo_op:
		case mthi_op:
			printf("\t%s",
				REG_NAME(i.r_format.rs));
			break;
		case tge_op:
		case tgeu_op:
		case tlt_op:
		case tltu_op:
		case teq_op:
		case tne_op:
		case mult_op:
		case multu_op:
		case div_op:
		case divu_op:
		case dmult_op:
		case dmultu_op:
		case ddiv_op:
		case ddivu_op:
			printf("\t%s,%s",
				REG_NAME(i.r_format.rs),
				REG_NAME(i.r_format.rt));
			break;
		case syscall_op:
		case sync_op:
			break;
		case break_op:
			{
			char *format = "\t0x%x";
			unsigned op2 = i.r_format.rd * 32 + i.r_format.re;
	
			if (op2)
				format = "\t0x%x,0x%x";
			printf(format, i.r_format.rs*32+i.r_format.rt, op2);
			}
			break;
		default:
			printf("\t%s,%s,%s",
				REG_NAME(i.r_format.rd),
				REG_NAME(i.r_format.rs),
				REG_NAME(i.r_format.rt));
			break;
		}
		break;
		
	case bcond_op:
		switch (i.i_format.rt) {
		case tgei_op:
		case tgeiu_op:
		case tlti_op:
		case tltiu_op:
		case teqi_op:
		case tnei_op:
			printf("%s\t%s,0x%x",
				bcond_name[i.i_format.rt],
				REG_NAME(i.i_format.rs),
				i.i_format.simmediate);
			break;
		default:
			printf("%s\t%s,",
				bcond_name[i.i_format.rt],
				REG_NAME(i.i_format.rs));
			do_b_displacement = true;
			print_next = 1;
			break;
		}
		break;
	case blez_op:
	case bgtz_op:
	case blezl_op:
	case bgtzl_op:
		printf("%s\t%s,", op_name[i.i_format.opcode],
			REG_NAME(i.i_format.rs));
		do_b_displacement = true;
		print_next = 1;
		break;
	case beq_op:
		if (i.i_format.rs == ZERO && i.i_format.rt == ZERO) {
			printf("b\t");
			do_b_displacement = true;
			print_next = 1;
			break;
		}
		/* fall through */
	case bne_op:
	case beql_op:
	case bnel_op:
		printf("%s\t%s,%s,", op_name[i.i_format.opcode],
			REG_NAME(i.i_format.rs),
			REG_NAME(i.i_format.rt));
		do_b_displacement = true;
		print_next = 1;
		break;

	case jal_op:
	case j_op:
		printf("%s\t", op_name[i.j_format.opcode]);
		if (GETNAME( (((__psunsigned_t)iadr+1) & ~((1<<28)-1))
				+(i.j_format.target<<2), 1 )) {
#ifndef PROM
			if (getenv("abs_adrs"))
				printf("%s+0x%x,0x%x", nptr, private.noffst,
				(inst_t *)((((__psunsigned_t)iadr+1) &
					  ~((1<<28)-1))
					  + (i.j_format.target<<2)));
			else
				printf("%s+0x%x", nptr, private.noffst);
#else
			printf("%s", nptr);
#endif
		} else
			printf("0x%x",
			(inst_t *)((((__psunsigned_t)iadr+1) & ~((1<<28)-1)) +
				   (i.j_format.target<<2)));
		print_next = 1;
		break;
		
	case swc1_op:
	case sdc1_op:
	case lwc1_op:
	case ldc1_op:
		printf("%s\t%s,", op_name[i.i_format.opcode],
			fp_register_name(i.i_format.rt));
		do_loadstore = true;
		break;

	case swc2_op:
	case sdc2_op:
	case lwc2_op:
	case ldc2_op:
		printf("%s\t$0x%x,", op_name[i.i_format.opcode],
			i.i_format.rt);
		do_loadstore = true;
		break;
		
	case lb_op:
	case lh_op:
	case lw_op:
	case ld_op:
	case sb_op:
	case sh_op:
	case sw_op:
	case sd_op:
	case ll_op:
	case sc_op:
		printf("%s\t%s,", op_name[i.i_format.opcode],
			REG_NAME(i.i_format.rt));
		do_loadstore = true;
		break;

	case lbu_op:
	case lhu_op:
	case swl_op:
	case swr_op:
	case lwl_op:
	case lwr_op:
	case scd_op:
	case lld_op:
	case lwu_op:
	case sdl_op:
	case sdr_op:
	case ldl_op:
	case ldr_op:
		printf("%s\t%s,", op_name[i.i_format.opcode],
		REG_NAME(i.i_format.rt));
		do_loadstore = true;
		break;
		
	case pref_op:
		printf("%s\t%d,", op_name[i.i_format.opcode], i.i_format.rt);
		do_loadstore = true;
		break;
		
	case cache_op:
		{
		unsigned code = i.c_format.c_op;
		unsigned caches = i.c_format.cache;
		char *operation;
#ifdef R10000
		static char *cachename[4] = {"I", "D", "X", "S"};
#else
		static char *cachename[4] = {"I", "D", "SI", "SD"};
#endif

		switch (code) {
		case 0:
			if (caches == 0 || caches == 2) 
				operation = "Index_Invalidate";
			else 
				operation = "Index_WriteBack_Invalidate";
			break;
		case 1:
			operation = "Index_Load_Tag";
			break;
		case 2:
			operation = "Index_Store_Tag";
			break;
#ifdef R4000
		case 3:
			operation = "Create_Dirty_Exclusive";
			break;
#endif
		case 4:
			operation = "Hit_Invalidate";
			break;
		case 5:
			if (caches == 0) {
#ifdef R10000
				operation = "Barrier";
				caches = 2;
	
#else
				operation = "Fill";
#endif
			}
			else 
				operation = "Hit_WriteBack_Invalidate";
			break;
		case 6:
#ifdef R10000
			operation = "Index_Load_Data";
#else
			operation = "Hit_WriteBack";
#endif
			break;
		case 7:
#ifdef R10000
			operation = "Index_Store_Data";
#else
			operation = "Hit_Set_Virtual";
#endif
			break;
		}
		printf("%s\t%s[%s],", op_name[i.i_format.opcode], 
			operation, cachename[caches]);
			do_loadstore = true;
		}
		break;

	case ori_op:
	case xori_op:
		if (i.u_format.rs == ZERO) {
			printf("li\t%s,0x%x",
				REG_NAME(i.u_format.rt),
				i.u_format.uimmediate);
			break;
		}
		/* fall through */
	case andi_op:
		printf("%s\t%s,%s,0x%x", op_name[i.u_format.opcode],
			REG_NAME(i.u_format.rt),
			REG_NAME(i.u_format.rs),
			i.u_format.uimmediate);
		break;
	case lui_op:
		if (i.u_format.rs == ZERO)
			printf("%s\t%s,0x%x", op_name[i.u_format.opcode],
				REG_NAME(i.u_format.rt),
				i.u_format.uimmediate);
		else
			printf("%s\t%s,%s,0x%x", op_name[i.u_format.opcode],
				REG_NAME(i.u_format.rt),
				REG_NAME(i.u_format.rs),
				i.u_format.uimmediate);
		break;
	case addi_op:
	case addiu_op:
		if (i.i_format.rs == ZERO) {
			short sign_extender = i.i_format.simmediate;
			printf("li\t%s,0x%x",
				REG_NAME(i.i_format.rt),
				sign_extender);
			break;
		}
		/* fall through */
	default:
		{
		short sign_extender = i.i_format.simmediate;
		printf("%s\t%s,%s,0x%x", op_name[i.i_format.opcode],
			REG_NAME(i.i_format.rt),
			REG_NAME(i.i_format.rs),
			sign_extender);
		}
		break;

	case cop0_op:
		switch (i.r_format.rs) {
		case bc_op:
			printf("bc0%s\t", bc_name[i.i_format.rt]);
			do_b_displacement = true;
			print_next = 1;
			break;
		case dmtc_op:
			printf("d");
		case mtc_op:
			printf("mtc0\t%s,%s",
				REG_NAME(i.r_format.rt),
				c0_register_name(i.r_format.rd));
			break;
		case dmfc_op:
			printf("d");
		case mfc_op:
			printf("mfc0\t%s,%s",
				REG_NAME(i.r_format.rt),
				c0_register_name(i.r_format.rd));
			break;
		case cfc_op:
			printf("cfc0\t%s,$0x%x",
				REG_NAME(i.r_format.rt),
				i.r_format.rd);
			break;
		case ctc_op:
			printf("ctc0\t%s,$0x%x",
				REG_NAME(i.r_format.rt),
				i.r_format.rd);
			break;
		case copm_op:
			printf("c0\t%s", c0mfunc_name[i.r_format.func]);
			break;
		case cop_op:
			printf("c0\t%s", c0func_name[i.r_format.func]);
			break;
		default:
			printf("c0rs0x%x", i.r_format.rs);
			break;
		}
		break;
		
	case cop1_op:
		switch (i.r_format.rs) {
		case bc_op:
			if ( i.i_format.rt >> 2 )
				printf("bc1%s\t$fcc0x%x,",
					bc_name[i.i_format.rt & 3],
				        i.r_format.rt >> 2);
			else
				printf("bc1%s\t", bc_name[i.i_format.rt]);
			print_next = 1;
			do_b_displacement = true;
			break;
		case dmtc_op:
			printf("d");
		case mtc_op:
			printf("mtc1\t%s,%s",
				REG_NAME(i.r_format.rt),
				fp_register_name(i.r_format.rd));
			break;
		case dmfc_op:
			printf("d");
		case mfc_op:
			printf("mfc1\t%s,%s",
				REG_NAME(i.r_format.rt),
				fp_register_name(i.r_format.rd));
			break;
		case cfc_op:
			printf("cfc1\t%s,$0x%x",
				REG_NAME(i.r_format.rt),
				i.r_format.rd);
			break;
		case ctc_op:
			printf("ctc1\t%s,$0x%x",
				REG_NAME(i.r_format.rt),
				i.r_format.rd);
			break;
		case cop_op+s_fmt:
		case cop_op+d_fmt:
		case cop_op+e_fmt:
		case cop_op+w_fmt:
		case cop_op+q_fmt:
		case cop_op+l_fmt:
			if (i.r_format.func == fmovc_op )
				printf("%s%c.%s\t",
				        cop1func_name[i.r_format.func],
				        (i.r_format.rt & 1) ? 't' : 'f',
				        c1fmt_name[i.r_format.rs - cop_op]);
			else
				printf("%s.%s\t",
				cop1func_name[i.r_format.func],
				c1fmt_name[i.r_format.rs - cop_op]);
			switch (i.r_format.func) {
			case frecip_op:
			case frsqrt_op:
			case fsqrt_op:
			case fabs_op:
			case fmov_op:
			case fcvts_op:
			case fcvtd_op:
			case fcvte_op:
			case fcvtw_op:
			case fcvtl_op:
			case ftrunc_op:
			case fround_op:
			case ffloor_op:
			case fceil_op:
			case fneg_op:
				printf("%s,%s",
					fp_register_name(i.r_format.re),
					fp_register_name(i.r_format.rd));
				break;
			case fcmp_op+0x0:
			case fcmp_op+0x1:
			case fcmp_op+0x2:
			case fcmp_op+0x3:
			case fcmp_op+0x4:
			case fcmp_op+0x5:
			case fcmp_op+0x6:
			case fcmp_op+0x7:
			case fcmp_op+0x8:
			case fcmp_op+0x9:
			case fcmp_op+0xa:
			case fcmp_op+0xb:
			case fcmp_op+0xc:
			case fcmp_op+0xd:
			case fcmp_op+0xe:
			case fcmp_op+0xf:
				if (i.r_format.re >> 2)
					printf("$fcc0x%x,%s,%s",
					     i.r_format.re >> 2,
					     fp_register_name(i.r_format.rd),
					     fp_register_name(i.r_format.rt));
				else
					printf("%s,%s",
					     fp_register_name(i.r_format.rd),
					     fp_register_name(i.r_format.rt));
				break;
			case fmovc_op:
				printf("%s,%s,$fcc0x%x",
				        fp_register_name(i.r_format.re),
				        fp_register_name(i.r_format.rd),
				        i.r_format.rt >> 2);
				break;
			case fmovz_op:
			case fmovn_op:
				printf("%s,%s,%s",
				        fp_register_name(i.r_format.re),
				        fp_register_name(i.r_format.rd),
				        REG_NAME(i.r_format.rt));
				break;
			default:
				printf("%s,%s,%s",
					fp_register_name(i.r_format.re),
					fp_register_name(i.r_format.rd),
					fp_register_name(i.r_format.rt));
				break;
			} /* switch on func */
			break;
		} /* switch on rs */
		break; /* End of cop1 */

	case cop1x_op:
	{
		printf("%s\t", cop1xfunc_name[i.r_format.func]);
		switch (i.r_format.func) {
			case lwxc1_op:
			case ldxc1_op:
				printf("%s,%s(%s)",
				  fp_register_name(i.ma_format.fd),
				  REG_NAME(i.ma_format.ft),
				  REG_NAME(i.ma_format.fr));
				break;
			case swxc1_op:
			case sdxc1_op:
				printf("%s,%s(%s)",
				  fp_register_name(i.ma_format.fs),
				  REG_NAME(i.ma_format.ft),
				  REG_NAME(i.ma_format.fr));
				break;
			case pfetch_op:
				printf("0x%x,%s(%s)",
				  i.ma_format.fs,
				  REG_NAME(i.ma_format.ft),
				  REG_NAME(i.ma_format.fr));
				break;
			case madd_s_op:
			case madd_d_op:
			case madd_e_op:
			case msub_s_op:
			case msub_d_op:
			case msub_e_op:
			case nmadd_s_op:
			case nmadd_d_op:
			case nmadd_e_op:
			case nmsub_s_op:
			case nmsub_d_op:
			case nmsub_e_op:
				printf("%s,%s,%s,%s",
					fp_register_name(i.ma_format.fd),
					fp_register_name(i.ma_format.fr),
					fp_register_name(i.ma_format.fs),
					fp_register_name(i.ma_format.ft));
				break;
			default:
				break;
		}
	}
	break;
		
	case cop2_op:
		{
			unsigned which_cop = i.j_format.opcode - cop0_op;
			
			switch (i.r_format.rs) {
			case bc_op:
				printf("bc0x%x%c\t", which_cop,
					bc_name[i.r_format.rt]);
				print_next = 1;
				do_b_displacement = true;
				break;
			case dmtc_op:
				printf("d");
			case mtc_op:
				printf("mtc0x%x\t%s,$0x%x", which_cop,
				REG_NAME(i.r_format.rt),
				i.r_format.rd);
				break;
			case dmfc_op:
				printf("d");
			case mfc_op:
				printf("mfc0x%x\t%s,$0x%x", which_cop,
					REG_NAME(i.r_format.rt),
					i.r_format.rd);
				break;
			case cfc_op:
				printf("cfc0x%x\t%s,$0x%x", which_cop,
					REG_NAME(i.r_format.rt),
					i.r_format.rd);
				break;
			case ctc_op:
				printf("ctc0x%x\t%s,$0x%x", which_cop,
					REG_NAME(i.r_format.rt),
					i.r_format.rd);
				break;
			default:
				printf("ctc0x%x.0x%x\t0x%x", which_cop,
					i.r_format.rs, i.r_format.func);
				break;
			}
		}
		break;
		
	}
	if (do_loadstore) {
		simmediate = i.i_format.simmediate;
		printf("0x%x(%s)", simmediate&0xffff,
		    REG_NAME(i.i_format.rs));
#ifndef PROM
		if (showregs) {
			simmediate = i.i_format.simmediate;
			printf(" <0x%Ix>", simmediate +
			    private.regs[i.i_format.rs]);
		}
#endif
	} else if (do_b_displacement) {
		simmediate = i.i_format.simmediate;
		if ( GETNAME(iadr+1+simmediate, 1) )
#ifndef PROM
		    if ( getenv("abs_adrs") )
			printf("%s+0x%x,0x%x",
			    nptr,private.noffst, iadr+1+simmediate);
		    else
			printf("%s+0x%x", nptr,private.noffst);
		else
#endif
			printf("0x%x", iadr+1+simmediate);
	}

	/* print out the registers use in this inst */
#ifndef PROM
	if (showregs && regcount > 0 &&
			!(regcount == 1 && regnum[0] == 0) ) {
		puts("\t<");
		for (ireg = 0; ireg < regcount; ireg++)
			if (regnum[ireg] != 0) { /* we know that "zero=0" */
			    if (ireg && regnum[ireg-1])
				    putchar(',');
			    printf("%s=0x%llx",
				sbregister_name[regstyle][regnum[ireg]],
				private.regs[regnum[ireg]]);
			  }
		putchar('>');
	}
#endif
	putchar('\n');
	if (print_next) {
		_PrintInstruction(iadr+1, regstyle, showregs);
		ibytes += 4;
	}
	return(ibytes);
}

static void
printhex(unsigned int word)
{
	register int i;

	for (i = 28; i >= 0; i -= 4) {
		int nybble = word >> i & 0xf;
		putchar(nybble > 9 ? nybble - 10 + 'a' : nybble + '0');
	}
}
