/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

#ident "$Revision: 1.67 $"

/*
 * commands.c -- dbgmon commands
 */

#include <sys/types.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#if defined(IP19) || defined(IP21) || defined(IP25)
#include <sys/EVEREST/addrs.h>
#endif /* IP19 || IP21 || IP25 */
#include <fault.h>
#include "dbgmon.h"
#include <ctype.h>
#include <libsc.h>
#include <libsk.h>
#include <parser.h>
#include <sys/mapped_kernel.h>
#ifdef R10000
#include <R10kcache.h>
#endif

extern void notyetmess(char *);

/*
 * cpu register descriptions
 * (probably belong in cpu.h)
 */
extern struct reg_desc sr_desc[];
extern struct reg_desc cause_desc[];
extern struct reg_desc tlbhi_desc[];
extern struct reg_desc tlbinx_desc[];
extern struct reg_desc tlbctxt_desc[];
extern struct reg_desc tlbextctxt_desc[];
extern struct reg_desc tlbrand_desc[];
extern struct reg_desc tlblo01_desc[];
extern struct reg_desc tlblo_desc[];

extern int _icache_size;
extern int _dcache_size;
extern int _sidcache_size;
#if R4000 || R10000
extern unsigned int _dcache_linesize;
extern unsigned int _icache_linesize;
extern unsigned int _scache_linesize;

#if R4000
extern struct reg_desc r4k_config_desc[];
extern struct reg_desc r4k_s_taglo_desc[];
extern struct reg_desc r4k_p_taglo_desc[];
#if !R10000
#define config_desc r4k_config_desc
#endif
#endif
extern struct reg_desc config_desc[];
extern struct reg_desc s_taglo_desc[];
extern struct reg_desc p_taglo_desc[];
#if R4000
extern struct reg_desc cache_err_desc[];
#endif
#if R10000
extern struct reg_desc pcache_err_desc[];
extern struct reg_desc scache_err_desc[];
extern struct reg_desc sysad_err_desc[];
#endif	/* R4000 */
#endif /* R4000 || R10000 */

static struct reg_table reg_table[] = {
	/* wired zero registers */
	{ "r0",		R_R0,		0 },
	{ "zero",	R_ZERO,		0 },

	/* assembler temps registers */
	{ "r1",		R_R1,		0 },
	{ "at",		R_AT,		0 },

	/* return value registers */
	{ "r2",		R_R2,		0 },
	{ "r3",		R_R3,		0 },
	{ "v0",		R_V0,		0 },
	{ "v1",		R_V1,		0 },

	/* argument registers */
	{ "r4",		R_R4,		0 },
	{ "r5",		R_R5,		0 },
	{ "r6",		R_R6,		0 },
	{ "r7",		R_R7,		0 },
	{ "a0",		R_A0,		0 },
	{ "a1",		R_A1,		0 },
	{ "a2",		R_A2,		0 },
	{ "a3",		R_A3,		0 },
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	{ "a4",		R_A4,		0 },
	{ "a5",		R_A5,		0 },
	{ "a6",		R_A6,		0 },
	{ "a7",		R_A7,		0 },
#endif

	/* caller saved registers */
	{ "r8",		R_R8,		0 },
	{ "r9",		R_R9,		0 },
	{ "r10",	R_R10,		0 },
	{ "r11",	R_R11,		0 },
	{ "r12",	R_R12,		0 },
	{ "r13",	R_R13,		0 },
	{ "r14",	R_R14,		0 },
	{ "r15",	R_R15,		0 },
	{ "t0",		R_T0,		0 },
	{ "t1",		R_T1,		0 },
	{ "t2",		R_T2,		0 },
	{ "t3",		R_T3,		0 },
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	{ "t4",		R_T4,		0 },
	{ "t5",		R_T5,		0 },
	{ "t6",		R_T6,		0 },
	{ "t7",		R_T7,		0 },
#endif

	/* callee saved registers */
	{ "r16",	R_R16,		0 },
	{ "r17",	R_R17,		0 },
	{ "r18",	R_R18,		0 },
	{ "r19",	R_R19,		0 },
	{ "r20",	R_R20,		0 },
	{ "r21",	R_R21,		0 },
	{ "r22",	R_R22,		0 },
	{ "r23",	R_R23,		0 },
	{ "s0",		R_S0,		0 },
	{ "s1",		R_S1,		0 },
	{ "s2",		R_S2,		0 },
	{ "s3",		R_S3,		0 },
	{ "s4",		R_S4,		0 },
	{ "s5",		R_S5,		0 },
	{ "s6",		R_S6,		0 },
	{ "s7",		R_S7,		0 },

	/* code generator registers */
	{ "r24",	R_R24,		0 },
	{ "r25",	R_R25,		0 },
	{ "t8",		R_T8,		0 },
	{ "t9",		R_T9,		0 },

	/* kernel temps registers */
	{ "r26",	R_R26,		0 },
	{ "r27",	R_R27,		0 },
	{ "k0",		R_K0,		0 },
	{ "k1",		R_K1,		0 },

	/* C linkage registers */
	{ "r28",	R_R28,		0 },
	{ "r29",	R_R29,		0 },
	{ "r30",	R_R30,		0 },
	{ "r31",	R_R31,		0 },
	{ "gp",		R_GP,		0 },
	{ "sp",		R_SP,		0 },
	{ "fp",		R_FP,		0 },
	{ "ra",		R_RA,		0 },

	/* IS THIS NECESSARY ???? */
	/* floating point registers */
	{ "f0",		R_F0,		0 },
	{ "f1",		R_F1,		0 },
	{ "f2",		R_F2,		0 },
	{ "f3",		R_F3,		0 },
	{ "f4",		R_F4,		0 },
	{ "f5",		R_F5,		0 },
	{ "f6",		R_F6,		0 },
	{ "f7",		R_F7,		0 },
	{ "f8",		R_F8,		0 },
	{ "f9",		R_F9,		0 },
	{ "f10",	R_F10,		0 },
	{ "f11",	R_F11,		0 },
	{ "f12",	R_F12,		0 },
	{ "f13",	R_F13,		0 },
	{ "f14",	R_F14,		0 },
	{ "f15",	R_F15,		0 },
	{ "f16",	R_F16,		0 },
	{ "f17",	R_F17,		0 },
	{ "f18",	R_F18,		0 },
	{ "f19",	R_F19,		0 },
	{ "f20",	R_F20,		0 },
	{ "f21",	R_F21,		0 },
	{ "f22",	R_F22,		0 },
	{ "f23",	R_F23,		0 },
	{ "f24",	R_F24,		0 },
	{ "f25",	R_F25,		0 },
	{ "f26",	R_F26,		0 },
	{ "f27",	R_F27,		0 },
	{ "f28",	R_F28,		0 },
	{ "f29",	R_F29,		0 },
	{ "f30",	R_F30,		0 },
	{ "f31",	R_F31,		0 },

	/* special processor registers */
	{ "pc",		R_EPC,		0 },
	{ "epc",	R_EPC,		0 },
	{ "mdhi",	R_MDHI,		0 },
	{ "mdlo",	R_MDLO,		0 },
	{ "sr",		R_SR,		sr_desc },
	{ "cause",	R_CAUSE,	cause_desc },
	{ "tlbhi",	R_TLBHI,	tlbhi_desc },
	{ "entryhi",	R_TLBHI,	tlbhi_desc },
	{ "badvaddr",	R_BADVADDR,	0 },
	{ "index",	R_INX,		tlbinx_desc },
	{ "inx",	R_INX,		tlbinx_desc },
	{ "context",	R_CTXT,		tlbctxt_desc },
	{ "ctxt",	R_CTXT,		tlbctxt_desc },
	{ "random",	R_RAND,		tlbrand_desc },
	{ "rand",	R_RAND,		tlbrand_desc },
#if R4000 || R10000
	{ "tlblo",	R_TLBLO0,	tlblo01_desc },
	{ "tlblo0",	R_TLBLO0,	tlblo01_desc },
	{ "entrylo",	R_TLBLO0,	tlblo01_desc },
	{ "entrylo0",	R_TLBLO0,	tlblo01_desc },
	{ "tlblo1",	R_TLBLO1,	tlblo01_desc },
	{ "entrylo1",	R_TLBLO1,	tlblo01_desc },
	{ "pgmsk",	R_PGMSK,	0 },
	{ "pgmask",	R_PGMSK,	0 },
	{ "pagemask",	R_PGMSK,	0 },
	{ "wired",	R_WIRED,	0 },
	{ "tlbwired",	R_WIRED,	0 },
	{ "count",	R_COUNT,	0 },
	{ "compare",	R_COMPARE,	0 },
	{ "lladdr",	R_LLADDR,	0 },
	{ "watchlo",	R_WATCHLO,	0 },
	{ "watchhi",	R_WATCHHI,	0 },
	{ "extctxt",	R_EXTCTXT,	0 },
	{ "ecc",	R_ECC,		0 },
#if R4000
	{ "cacherr",	R_CACHERR,	cache_err_desc },
	{ "cacheerr",	R_CACHERR,	cache_err_desc },
#else
	/*
	 * since the cacherr reg uses different format for different error
	 * source, determine which format to use after decoding the error
	 * source using the 2 MSBits
	 */
	{ "cacherr",	R_CACHERR,	0 },
	{ "cacheerr",	R_CACHERR,	0 },
#endif	/* R4000 */

	/* 
	 * since the taglo reg may hold primary or secondary tags, the
	 * caller specifies whether to use s_taglo_desc or p_taglo_desc
	 */
	{ "taglo",	R_TAGLO,	0 },
	{ "taghi",	R_TAGHI,	0 },
	{ "errorepc",	R_ERREPC,	0 },
	{ "errepc",	R_ERREPC,	0 },
	{ "config",	R_CONFIG,	config_desc },
#endif /* R4000 || R10000 */
#if TFP
	{ "tlbset",	R_TLBSET,	0 },
	{ "ubase",	R_UBASE,	0 },
	{ "shiftamt",	R_SHIFTAMT,	0 },
	{ "trapbase",	R_TRAPBASE,	0 },
	{ "badpaddr",	R_BADPADDR,	0 },
	{ "count",	R_COUNT,	0 },
	{ "prid",	R_PRID,	0 },
	{ "config",	R_CONFIG,	0 },
	{ "work0",	R_WORK0,	0 },
	{ "work1",	R_WORK1,	0 },
	{ "pbase",	R_PBASE,	0 },
	{ "gbase",	R_GBASE,	0 },
	{ "wired",	R_WIRED,	0 },
	{ "dcache",	R_DCACHE,	0 },
	{ "icache",	R_ICACHE,	0 },
#endif
	{ 0,		0,		0 }
};


#undef IS_R10000		/* from sys/sbd.h */

#if defined(R10000) && defined(R4000)   /* IP32 */
/* IS_R10000 taken from sys/sbd.h */
#define IS_R10000() ((get_prid() >> C0_IMPSHIFT) < C0_IMP_R5000)
#define _NTLBENTRIES() (IS_R10000() ? R10K_NTLBENTRIES : R4K_NTLBENTRIES)
#define S_TAGLO_DESC() (IS_R10000() ? s_taglo_desc : r4k_s_taglo_desc)
#define P_TAGLO_DESC() (IS_R10000() ? p_taglo_desc : r4k_p_taglo_desc)
#define CONFIG_DESC()  (IS_R10000() ? config_desc  : r4k_config_desc)
#else  /* !IP32 */
#if defined(R4000)		/* R4000 only */
#define IS_R10000()	0
#define S_TAGLO_DESC() r4k_s_taglo_desc
#define P_TAGLO_DESC() r4k_p_taglo_desc
#define CONFIG_DESC()  r4k_config_desc
#else                           /* R10000 only */
#define IS_R10000()	1
#define S_TAGLO_DESC() s_taglo_desc
#define P_TAGLO_DESC() p_taglo_desc
#define CONFIG_DESC()  config_desc
#endif
#define _NTLBENTRIES() NTLBENTRIES /* R4k or R10k, but not both */
#endif /* IP32 */

static struct reg_table *lookup_reg(char *);
#define USER_REGS	1
#define SYSTEM_REGS	2
static int whichregs = USER_REGS;	/* generally want user-context dumped */

/*ARGSUSED*/
int
printregs(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	/* check if user wants to change which register-set to display:
	 * user-context, system (C0), or both.  If not specified, use
	 * the most recent default.
	 */
	if (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {
		case 'u':
			whichregs = USER_REGS;
			break;
		case 's':
			whichregs = SYSTEM_REGS;
			break;
		case 'a':	/* accept a for "all" or b for "both" */
		case 'b':
			whichregs = USER_REGS | SYSTEM_REGS;
			break;

		default:
			printf("Illegal printregs option (%c)\n",argv[1][0]);
			return(1);
		}
	}

	if (whichregs & USER_REGS) {
		printf("User-context registers:\n");
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
		printf("  v0-v1: 0x%Ix 0x%Ix  args: 0x%Ix 0x%Ix 0x%Ix 0x%Ix\n",
			_get_register(R_V0), _get_register(R_V1),
			_get_register(R_A0), _get_register(R_A1),
			_get_register(R_A2), _get_register(R_A3));
		printf("  temp: 0x%Ix 0x%Ix 0x%Ix 0x%Ix 0x%Ix 0x%Ix 0x%Ix 0x%Ix\n",
			_get_register(R_T0), _get_register(R_T1),
			_get_register(R_T2), _get_register(R_T3),
			_get_register(R_T4), _get_register(R_T5),
			_get_register(R_T6), _get_register(R_T7));
		printf("  save: 0x%Ix 0x%Ix 0x%Ix 0x%Ix 0x%Ix 0x%Ix 0x%Ix 0x%Ix\n",
			_get_register(R_S0), _get_register(R_S1),
			_get_register(R_S2), _get_register(R_S3),
			_get_register(R_S4), _get_register(R_S5),
			_get_register(R_S6), _get_register(R_S7));
		printf("  t8-t9: 0x%Ix 0x%Ix  at 0x%Ix  k0-k1: 0x%Ix 0x%Ix\n",
			_get_register(R_T8), _get_register(R_T9),
			_get_register(R_AT), _get_register(R_K0),
			_get_register(R_K1));
		printf("  gp 0x%Ix  sp 0x%Ix  fp 0x%Ix  ra 0x%Ix\n",
			_get_register(R_GP), _get_register(R_SP),
			_get_register(R_FP), _get_register(R_RA));
#elif (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
		printf("  v0-v1: 0x%llx 0x%llx\n",
			_get_register(R_V0), _get_register(R_V1));
		printf("  args: 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx\n",
			_get_register(R_A0), _get_register(R_A1),
			_get_register(R_A2), _get_register(R_A3),
			_get_register(R_A4), _get_register(R_A5),
			_get_register(R_A6), _get_register(R_A7));
		printf("  temp: 0x%llx 0x%llx 0x%llx 0x%llx\n",
			_get_register(R_T0), _get_register(R_T1),
			_get_register(R_T2), _get_register(R_T3));
		printf("  save: 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx\n",
			_get_register(R_S0), _get_register(R_S1),
			_get_register(R_S2), _get_register(R_S3),
			_get_register(R_S4), _get_register(R_S5),
			_get_register(R_S6), _get_register(R_S7));
		printf("  t8-t9: 0x%llx 0x%llx  at 0x%llx  k0-k1: 0x%llx 0x%llx\n",
			_get_register(R_T8), _get_register(R_T9),
			_get_register(R_AT), _get_register(R_K0),
			_get_register(R_K1));
		printf("  gp 0x%llx  sp 0x%llx  fp 0x%llx  ra 0x%llx\n",
			_get_register(R_GP), _get_register(R_SP),
			_get_register(R_FP), _get_register(R_RA));
	}
#else
	<<BOMB - unsupported_ABI>>
#endif

	if (whichregs & SYSTEM_REGS) {
#if R4000 || R10000
		k_machreg_t reg0;
#if R10000
		struct reg_desc *cacherr_desc;
		char *src_str;
#endif	/* R10000 */
#endif

		printf("System registers:\n");
		printf("  EPC 0x%Ix  SR %llR\n",
			_get_register(R_EPC),
			_get_register(R_SR), sr_desc);
		printf("  cause %llR  badvaddr 0x%llx\n",
			_get_register(R_CAUSE),cause_desc,
			_get_register(R_BADVADDR));
#if R4000 || R10000
		/* tlb registers */
		printf("  tlbhi %llR\n  tlblo0 %llR  tlblo1 %llR\n",
			_get_register(R_TLBHI), tlbhi_desc,
			_get_register(R_TLBLO0),tlblo01_desc,
			_get_register(R_TLBLO1),tlblo01_desc);

		printf("  tlbwired 0x%x  pgmask 0x%x  tlbinx %llR  ",
			_get_register(R_WIRED),_get_register(R_PGMSK),
			_get_register(R_INX),tlbinx_desc);
		printf("rand %llR\n  tlbctxt %llR  tlbextctxt %llR\n",
			_get_register(R_RAND),tlbrand_desc,
			_get_register(R_CTXT),tlbctxt_desc,
			_get_register(R_EXTCTXT),tlbextctxt_desc);

		/* ECC info */
#if R4000
		printf("  cache_err %llR\n  error_epc 0x%Ix  ecc 0x%x\n",
			_get_register(R_CACHERR), cache_err_desc,
			_get_register(R_ERREPC), _get_register(R_ECC));
#endif
#if R10000
		if (IS_R10000()) {
		    reg0 = _get_register(R_CACHERR);
		    switch (reg0 & CACHERR_SRC_MSK) {
		    case CACHERR_SRC_PI:
			    cacherr_desc = pcache_err_desc;
			    src_str = "icache";
			    break;

		    case CACHERR_SRC_PD:
			    cacherr_desc = pcache_err_desc;
			    src_str = "dcache";
			    break;

		    case CACHERR_SRC_SD:
			    cacherr_desc = scache_err_desc;
			    src_str = "scache";
			    break;

		    case CACHERR_SRC_SYSAD:
			    cacherr_desc = sysad_err_desc;
			    src_str = "sysad";
			    break;
		    }
		    printf("  cache_err(%s) %R\n  error_epc 0x%Ix  ecc 0x%Ix\n",
			   src_str, _get_register(R_CACHERR), cacherr_desc,
			   _get_register(R_ERREPC), _get_register(R_ECC));
		}
#endif	/* R4000 */
		reg0 = _get_register(R_TAGLO);
		printf("  taglo: p-format %llR  s-format %llR\n",
			reg0, P_TAGLO_DESC(),
			reg0, S_TAGLO_DESC());

		/* watchpoint regs, lladdr, count and compare */
		printf("  watchlo 0x%x  watchhi 0x%x  lladdr 0x%x  ",
			_get_register(R_WATCHLO), _get_register(R_WATCHHI),
			_get_register(R_LLADDR));
		printf("cnt 0x%x  cmp 0x%x\n", _get_register(R_COUNT),
			_get_register(R_COMPARE));
		printf("  config %llR\n",_get_register(R_CONFIG),CONFIG_DESC());
#elif TFP
		printf("  tlbhi %R  tlblo %R\n",
			_get_register(R_TLBHI), tlbhi_desc,
			_get_register(R_TLBLO),tlblo01_desc);
		printf("  tlbwired 0x%x  icache 0x%x  dcache 0x%x\n",
			_get_register(R_WIRED),
			_get_register(R_ICACHE),
			_get_register(R_DCACHE));
		printf("  tlbset 0x%x  badpaddr 0x%x  counts 0x%x  shiftamt 0x%x\n",
			_get_register(R_TLBSET),
			_get_register(R_BADPADDR),
			_get_register(R_COUNT),
			_get_register(R_SHIFTAMT));
		printf("  trapbase 0x%x  ubase 0x%x  gbase 0x%x  pbase 0x%x\n",
			_get_register(R_TRAPBASE),
			_get_register(R_UBASE),
			_get_register(R_GBASE),
			_get_register(R_PBASE));
		printf("  prid 0x%x  config 0x%x  work0 0x%x  work1 0x%x\n",
			_get_register(R_PRID),
			_get_register(R_CONFIG),
			_get_register(R_WORK0),
			_get_register(R_WORK1));
#endif /* R4000 || R10000 */
	}
	return 0;

} /* printregs */


/*
 * _get -- read memory or register
 */
/*ARGSUSED*/
int
_get(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	numinp_t address;
	unsigned long long val;
	struct reg_table *rt;
	int width = SW_WORD;

	argv++; argc--;
	if (argc == 2 && **argv == '-') {
		switch ((*argv)[1]) {
		case 'b':
			width = SW_BYTE;
			break;

		case 'h':
			width = SW_HALFWORD;
			break;

		case 'w':
			width = SW_WORD;
			break;

		case 'd':
			width = SW_DOUBLEWORD;
			break;

		default:
			return(1);
		}
		argv++; argc--;
	}

	if (argc != 1)
		return(1);

	if (rt = lookup_reg(*argv)) {
#if R10000
		struct reg_desc *cacherr_desc;
		char *src_str;
#endif	/* R10000 */
		val = _get_register((int)rt->rt_index);

		switch(rt->rt_index) {
#if R4000 || R10000
		case R_CONFIG:	/* the decimal value is useless */
			printf("%s:\t%R\n", *argv, (unsigned)val, rt->rt_desc);
			break;
		case R_TAGLO:	/* display in p_tag and s_tag formats */
                	printf("%s: p-format %R%ss-format %R\n",
				*argv, val, P_TAGLO_DESC(),
				(val?"\n       ":"  "), val, S_TAGLO_DESC());
			break;

#if R10000
		case R_CACHERR:
			if (IS_R10000()) {
				switch (val & CACHERR_SRC_MSK) {
				case CACHERR_SRC_PI:
					cacherr_desc = pcache_err_desc;
					src_str = "icache";
					break;

				case CACHERR_SRC_PD:
					cacherr_desc = pcache_err_desc;
					src_str = "dcache";
					break;

				case CACHERR_SRC_SD:
					cacherr_desc = scache_err_desc;
					src_str = "scache";
					break;

				case CACHERR_SRC_SYSAD:
					cacherr_desc = sysad_err_desc;
					src_str = "sysad";
					break;
				}

				printf("%s (%s):\t%llu\t%R\n",
				       *argv, src_str, val, (unsigned)val,
				       cacherr_desc);
				break;
			}
			/* FALLTHRU, if IP32/R5000 */
#endif	/* R10000 */

#endif /* R4000 || R10000 */
		default:
			if (rt->rt_desc)
				printf("%s:\t%llu\t%R\n", *argv, val, (unsigned)val,
			       		rt->rt_desc);
			else {
				printf("%s:\t%llu\t0x%llx\t'", *argv, val, val);
				showchar(val);
				puts("'\n");
			}
			break;
		}
	} else {
		/* must be an address */
		if (parse_sym(*argv, &address) && *atob_s(*argv, &address))
			_dbg_error("Illegal address: %s", *argv);
			/* doesn't return */

		/* sanity check address and adjust width as necessary */
		if (address & 1) {
			width = SW_BYTE;
		} else if ((address & 2) && 
		   ((width == SW_WORD) || (width == SW_DOUBLEWORD))) {
			width = SW_HALFWORD;
		} else if ((address & 4) && (width == SW_DOUBLEWORD)) {
			width = SW_WORD;
		}

		address = sign_extend(address);
		if (IS_KSEG2(address) && !IS_MAPPED_KERN_SPACE(address) &&
			probe_tlb((caddr_t)address, 0) < 0)
			map_k2seg_addr((void *)address); /* no return on failure */

		val = GET_MEMORY(address, width);
#if _MIPS_SIM == _ABI64
		printf("0x%Ix:\t%llu\t0x%llx\t'", address, val, val);
#else
		printf("0x%08x:\t%llu\t0x%llx\t'", (int)address, val, val);
#endif
		showchar(val);
		puts("'\n");
	}
	return(0);
}

/*
 * put -- write memory or register
 */
/*ARGSUSED*/
int
_put(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	numinp_t address;
	long long val;
	struct reg_table *rt;
	int width = SW_WORD;

	argv++; argc--;
	if (argc == 3 && **argv == '-') {
		switch ((*argv)[1]) {
		case 'b':
			width = SW_BYTE;
			break;

		case 'h':
			width = SW_HALFWORD;
			break;

		case 'w':
			width = SW_WORD;
			break;

		case 'd':
			width = SW_DOUBLEWORD;
			break;

		default:
			return(1);
		}
		argv++; argc--;
	} 

	if (argc != 2)
		return(1);

	if (*atob_L(argv[1], &val))
		_dbg_error("Illegal value: %s", argv[1]);
		/* doesn't return */

	if (width != SW_DOUBLEWORD)
		val = sign_extend(val);

	if (rt = lookup_reg(*argv))
		_set_register((int)rt->rt_index, val);
	else {
		/* must be an address */
		if ( parse_sym(*argv, &address) && *atob_s(*argv, &address))
			_dbg_error("Illegal address: %s", *argv);
			/* doesn't return */

		/* sanity check address and adjust width as necessary */
		if (address & 1) {
			width = SW_BYTE;
		} else if ((address & 2) && 
		   ((width == SW_WORD) || (width == SW_DOUBLEWORD))) {
			width = SW_HALFWORD;
		} else if ((address & 4) && (width == SW_DOUBLEWORD)) {
			width = SW_WORD;
		}

		address = sign_extend(address);
		if (IS_KSEG2(address) && !IS_MAPPED_KERN_SPACE(address) &&
		    probe_tlb((caddr_t)address, 0) < 0)
			map_k2seg_addr((void *)address); /* no return on failure */

		SET_MEMORY(address, width, val);
	}
	return(0);
}

/*
 * lookup_reg -- returns reg_table entry
 */
static struct reg_table *
lookup_reg(char *name)
{
	register struct reg_table *rt;

	for (rt = reg_table; rt->rt_name; rt++)
		if (strcmp(rt->rt_name, name) == 0)
			return(rt);
	return((struct reg_table *)0);
}

int
lookup_regname(char *name, numinp_t *rs)
{
	struct reg_table *rt = lookup_reg(name);

	if ( rt ) {
		*rs = (long long)_get_register(rt->rt_index);
		return 0;
	}

	return 1;
}

/*
 * string -- print memory as string
 */
/*ARGSUSED*/
int
_string(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	int maxlen = DEFAULT_STRLEN;
	register int c;
	numinp_t addr;

	if (argc <= 1 || argc > 3)
		return(1);

	if (parse_sym_range(argv[1],&addr,&maxlen,SW_BYTE) &&
						*atob_s(argv[1], &addr))
		_dbg_error("illegal address: %s", argv[1]);
	
	if (argc == 3)
		if (*atob(argv[2], &maxlen))
			_dbg_error("illegal maxlen: %s", argv[2]);

	putchar('"');
	while ((c = GET_MEMORY(addr++, SW_BYTE)) && maxlen-- > 0)
		showchar(c);
	puts("\"\n");
	return(0);
}

/*
 * call -- invoke client routine with arguments
 */
/*ARGSUSED*/
int
_call(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	numinp_t args[8];
	int i;
	numinp_t callp;

	if (argc < 2)
		return(1);

	if (argc > 8)
		_dbg_error("no more than 8 args can be passed with call", 0);

	for (i = 0; i < argc - 2; i++)
		if (*atob_s(argv[i+2], &args[i]))
			_dbg_error("argument not numeric: %s", argv[i+2]);

	if((*atob_s(argv[1], &callp) && !(callp=(numinp_t)fetch_kaddr(argv[1])))
		|| ((unsigned)callp & 0x3))
		_dbg_error("bad routine address/name: %s", argv[1]);

	callp = sign_extend(callp);
	
	i = invoke((inst_t *)callp, args[0], args[1], args[2], args[3],
		   args[4], args[5], args[6], args[7]);

	printf("Routine returns %d 0x%x\n", i, i);
	return(0);
}

#if TFP
/*
 * tlbx [RANGE] -- flush contents of tlb and clear TLBX bit in cause register
 */
/*ARGSUSED*/
int
_tlbx(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	extern void tfp_clear_tlbx(void);

	(void)_tlbflush(argc, argv, argp, xxx);
	tfp_clear_tlbx();
	return(0);
}
#endif

/*
 * tlbdump [-v] [-p PID] [RANGE] -- dump contents of tlb
 * dumps entire tlb if no range specified
 */
/*ARGSUSED*/
int
_tlbdump(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	int count, psize, pidmatch;
	numinp_t tindex;
	char *cp;
#if TFP
	__psunsigned_t vaddrs[NTLBSETS];
	__psunsigned_t pids[NTLBSETS];
	int verbose = 0;
#endif

	if (argc > 3)
		return(1);

	tindex = pidmatch = 0;
	count = _NTLBENTRIES();

	argc--; argv++;
	while (argc  && **argv == '-') {
		cp = &(*argv)[1];
		do {
			switch (*cp) {
			case 'v':
#ifdef TFP
				verbose = 1;
#endif
				break;
			case 'p':
				if (--argc < 1 || *atob(*++argv, &pidmatch))
					return(1);
				break;
			default:
				return(1);
			}
		} while (*++cp);
		argc--; argv++;
	}

	if (argc >= 1) {
		if (parse_sym_range(argv[0], &tindex, &count, SW_BYTE))
			return 1;
		if (count < 0 || tindex + count > _NTLBENTRIES()) {
			printf("bad tlb spec: %s\n", argv);
			return(0);
		}
	}
	while (count--) {
#if R4000 || R10000
		if (pidmatch == 0 ||
		    pidmatch == (get_tlbhi(tindex)&TLBHI_PIDMASK)) {
			psize = ((get_pgmaski(tindex) + 0x2000) >> 1) / 1024;
			printf("%u:\t%R size=0x%x(%dK)\n\t\t%R\n\t\t%R\n",
			        (int) tindex, 
			       get_tlbhi(tindex), tlbhi_desc,
				get_pgmaski(tindex), psize,
				get_tlblo0(tindex), tlblo01_desc,
				get_tlblo1(tindex), tlblo01_desc);
		}
#elif TFP
		machreg_t tlblo, tlbhi, vaddr, pid, xormask;
		int set;

		/* Need to compute bits 18:12 of the Virtual Address
		 * since they are implied by the index number XORed
		 * with the PID.  Just "or" them into tlbhi contents
		 * since the bits should be zero.
		 */

		for (set = 0; set < NTLBSETS; set++) {
			tlbhi = get_tlbhi(tindex, set);
			tlblo = get_tlblo(tindex, set);
			vaddr = tlbhi & TLBHI_VPNMASK;
			pids[set] = pid =
				(tlbhi & TLBHI_PIDMASK) >> TLBHI_PIDSHIFT;

			/* Need to 'xor' low order virtual address with the
			 * PID unless the address is in KV1 space
			 * (kernel global).
			 */
			xormask = ((tlbhi&KV1BASE) != KV1BASE) ? (pid&0x7f) : 0;
			vaddr |= ((tindex ^ xormask) << PNUMSHFT);
			vaddrs[set] = vaddr;
			if ((verbose || (tlblo & TLBLO_V)) &&
			    ((pidmatch==0) || (pid == pidmatch)))
				printf("%x/%d:\t%R\t0x%x<VA=0x%x,PID=%d>\n",
					tindex, set, tlblo, tlblo01_desc,
					tlbhi, vaddr, pid);
		}
#if NTLBSETS != 3
BOMB!
#endif
		if ((pids[0] == pids[1] && vaddrs[0] == vaddrs[1]) ||
		    (pids[0] == pids[2] && vaddrs[0] == vaddrs[2]) ||
		    (pids[1] == pids[2] && vaddrs[1] == vaddrs[2])) {
			printf("*** TLBX at index %x\n", tindex);
		}
#else
		printf("%d:\t%R\t%R\n", tindex, get_tlblo(tindex), tlblo_desc,
			get_tlbhi(tindex), tlbhi_desc);
#endif
		tindex++;
	}
	return(0);
}

/*
 * tlbflush [RANGE] -- flush tlb translations
 * flushes no wired tlb entries if no RANGE given
 */
/*ARGSUSED*/
int
_tlbflush(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	numinp_t addr;
	int count;
	extern struct reg_desc tlblo_desc[], tlbhi_desc[];

	if (argc > 2)
		return(1);
	if (argc == 1) {
		addr = TLBRANDOMBASE;
		count = _NTLBENTRIES();
	} else {
		if (parse_sym_range(argv[1], &addr, &count, SW_BYTE))
			return(1);
		if (count < 0 || addr + count > _NTLBENTRIES()) {
			printf("bad tlb spec: %s\n", argv[1]);
			return(0);
		}
	}
	while (count--)
		invaltlb(addr++);
	return(0);
}

/*
 * tlbvtop ADDRESS [PID] -- show physical address mapped to virtual ADDRESS
 * uses current pid if PID not specified
 */
/*ARGSUSED*/
int
_tlbvtop(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	unsigned pid;
	numinp_t address;
	int tindex;
#if TFP
	machreg_t tlbhi, tlblo, tlbhi_orig, tlbset_orig;
	long probe_found;
#endif

	if (argc < 2 || argc > 3)
		return(1);
	if (*atob_s(argv[1], &address)) {
		printf("bad address: %s\n", argv[1]);
		return(0);
	}
	if (argc == 3) {
		if (*atob(argv[2], (int*)&pid)) {
			printf("bad pid: %s\n", argv[2]);
			return(0);
		}
	} else
		pid = (int)get_tlbpid();
#if TFP
	tlbhi_orig = get_cp0_tlbhi();
	tlbset_orig = get_cp0_tlbset();
	set_cp0_badvaddr(address);
	tlbhi = (address & TLBHI_VPNMASK) | ((pid<<TLBHI_PIDSHIFT) & TLBHI_PIDMASK);
	set_cp0_tlbhi(tlbhi);
	if ((probe_found = (long)cp0_tlb_probe()) >= 0) {
	  printf("tlb probe HIT in set %d\n", probe_found);
	  cp0_tlb_read();
	  tlbhi = get_cp0_tlbhi();
	  tlblo = get_cp0_tlblo();
	  printf("\t%R\t0x%x<VA=0x%x,PID=%d>\n", tlblo, tlblo01_desc, tlbhi,
		 address, pid);
	} else 
	  printf("0x%x:%d not mapped by tlb\n", address, pid);
	set_cp0_tlbhi(tlbhi_orig);
	set_cp0_tlbset(tlbset_orig);
	return(0);
#else
	tindex = (int)probe_tlb((char *)address, pid);
	if (tindex < 0)
		printf("0x%Ix:%u not mapped by tlb\n", address, pid);
	else {
#if R4000 || R10000
		int psize;

		psize = ((get_pgmaski(tindex) + 0x2000) >> 1) / 1024;
		printf("%d:\t%R size=0x%x(%dK)\n\t\t%R\n\t\t%R\n", tindex, 
			get_tlbhi(tindex), tlbhi_desc,
			get_pgmaski(tindex), psize,
			get_tlblo0(tindex), tlblo01_desc,
			get_tlblo1(tindex), tlblo01_desc);
#else /* !(R4000 || R10000) */
		printf("%d:\t%R\t%R\n", tindex, get_tlblo(tindex), tlblo_desc,
			get_tlbhi(tindex), tlbhi_desc);
#endif /* R4000 || R10000 */
	}
	return(0);
#endif	/* !TFP */
}



#if R4000 || R10000
/*
 * tlbpfntov PFN -- displays tlb entries mapping PFN
 */
/*ARGSUSED*/
int
_tlbpfntov(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	register int tindex;
	numinp_t pfn;
	int found = 0;

	if (argc != 2)
		return(1);
	if (*atob_s(argv[1], &pfn)) {
		printf("bad pfn: %s\n", argv[1]);
		return(0);
	}
	for (tindex = 0; tindex < _NTLBENTRIES(); tindex++)
		if ((((get_tlblo0(tindex)&TLBLO_PFNMASK)>>TLBLO_PFNSHIFT) == pfn) ||
		(((get_tlblo1(tindex)&TLBLO_PFNMASK)>>TLBLO_PFNSHIFT) == pfn)) {

			found++;
			printf("%d:\t%R\n\t%R\n\t%R\n",
				tindex,
				get_tlblo0(tindex), tlblo01_desc,
				get_tlblo1(tindex), tlblo01_desc,
				get_tlbhi(tindex), tlbhi_desc);
		}
	if (!found)
		printf("No mapping found to physical frame 0x%llx\n", pfn);
	return(0);
}
#endif /* R4000 || R10000 */

#if TFP
/* ARGSUSED */
int
_tlbptov(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	notyetmess("tlbptov");
	return 0;
}
#endif

/*
 * tlbpid [PID] -- get or set dbgmon tlb pid
 */
/*ARGSUSED*/
int
_tlbpid(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	unsigned pid;

	if (argc > 2)
		return(1);
	if (argc == 1) {
		printf("Current dbgmon pid = %d\n", get_tlbpid());
		return(0);
	}
	if (*atob(argv[1], (int*)&pid)
	    || (pid & (TLBHI_PIDMASK>>TLBHI_PIDSHIFT)) != pid) {
		printf("bad pid: %s\n", argv[1]);
		return(0);
	}
	set_tlbpid(pid);
	return(0);
}


#if R4000 || R10000
/* Convert a virtual frame number to a virtual address within that frame. */
#define vfntov(vfn) ((vfn)<<PNUMSHFT)

/*
 * tlbmap [-i INDEX] -0 [-c algo] [-(d|g|v)*] [PFN]
 * tlbmap [-i INDEX] -1 [-c algo] [-(d|g|v)*] [PFN]
 * tlbmap [-i INDEX] -H [VFN] [ASID]
 * maps VFN to PFN using current dbgmon pid
 */
/*ARGSUSED*/
int
_tlbmap(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	int tindex = TLBRANDOMBASE;
	int tmpinx;
	numinp_t vfn, pfn;
	int asid;
	pde_t tlblo_proto;
	pde_t tlblo0;
	pde_t tlblo1;
	machreg_t tlbhi;
	int which = 0;
	int algo;
	char *cp;

	argc--; argv++;

	if (argc == 0) {
		printf("Used to change entries in the tlbmap. Usage:\n");
 		printf("tlbmap [-i INDEX] -0 [-c algo] [-(d|g|v)*] [PFN]\n");
 		printf("tlbmap [-i INDEX] -1 [-c algo] [-(d|g|v)*] [PFN]\n");
 		printf("tlbmap [-i INDEX] -H [VFN] [ASID]\n");
		return(1);
	}

	tlblo_proto.pgi = 0;

	while (argc > 0 && **argv == '-') {
		cp = &(*argv)[1];
		do {
			switch (*cp) {
			case '0':
				if ( which != 0 ) {
					printf("specify only one of -0, -1, -H\n");
					return(1);
				}
				which = R_TLBLO0;
				break;

			case '1':
				if ( which != 0 ) {
					printf("specify only one of -0, -1, -H\n");
					return(1);
				}
				which = R_TLBLO1;
				break;

			case 'H':
			case 'h':
				if ( which != 0 ) {
					printf("specify only one of -0, -1, -H\n");
					return(1);
				}
				which = R_TLBHI;
				break;

			case 'i':
				if (--argc < 1 || *atob(*++argv, &tindex)) {
					printf("specify a tlb tindex\n");
					return(1);
				}
				if ( tindex < 0 || tindex > _NTLBENTRIES()) {
					printf("tindex must be 0..%d\n",
						_NTLBENTRIES());
					return(1);
				}
				break;
				
			case 'c':
				if (--argc < 1 || *atob(*++argv, &algo)) {
					printf("specify a numeric consistency algorithm\n");
					return(1);
				}
				if ( which == R_TLBHI ) {
					printf("specify coherency algorithm with TLBLO entries\n");
					return(1);
				}
				tlblo_proto.pgi |=
					(tlblo_proto.pgi & TLBLO_CACHMASK) |
					((algo<<TLBLO_CACHSHIFT)&TLBLO_CACHMASK);
				break;
				
			case 'v':
				if ( which == R_TLBHI ) {
					printf("specify valid bit with TLBLO entries\n");
					return(1);
				}
				tlblo_proto.pgi |= TLBLO_V;
				break;
				
			case 'd':
				if ( which == R_TLBHI ) {
					printf("specify dirty bit with TLBLO entries\n");
					return(1);
				}
				tlblo_proto.pgi |= TLBLO_D;
				break;
				
			case 'g':
				if ( which == R_TLBHI ) {
					printf("specify global bit with TLBLO entries\n");
					return(1);
				}
				tlblo_proto.pgi |= TLBLO_G;
				break;
			
			default:
				printf("unknown flag: %s\n", *argv);
				return(1);
			}
		} while (*++cp);
		argc--; argv++;
	}

	if (which == 0) {
		printf("Must specify -0 (tlblo0), -1 (tlblo1), or -H (tlbhi)\n");
		return(1);
	}

	/*
	 * Get current contents of specified tlb entry.
	 */
	tlblo0.pgi = (int)get_tlblo0(tindex);
	tlblo1.pgi = (int)get_tlblo1(tindex);
	tlbhi = get_tlbhi(tindex);

	switch(which) {
		case R_TLBLO0:
		case R_TLBLO1:
			if (argc > 0) {
				if (*atob_s(*argv, &pfn)) {
					printf("bad pfn: %s\n", *argv);
					return(1);
				}
				argv++; argc--;
				tlblo_proto.pgi |= (pfn << TLBLO_PFNSHIFT);
			} else {
				tlblo_proto.pgi |= ((which==R_TLBLO0) ? 
					(tlblo0.pgi & TLBLO_PFNMASK) :
					(tlblo1.pgi & TLBLO_PFNMASK));
			}

			if (argc > 0) {
				printf("extraneous arguments\n");
				return(1);
			}

			if ( which == R_TLBLO0 )
				tlblo0.pgi = tlblo_proto.pgi;
			else
				tlblo1.pgi = tlblo_proto.pgi;
			break;

		case R_TLBHI:
			if (argc > 0) {
				if (*atob_s(*argv, &vfn)) {
					printf("bad vfn: %s\n", *argv);
					return(1);
				}
				argv++; argc--;
			} else
				vfn = (int)((tlbhi & TLBHI_VPNMASK) >> TLBHI_VPNSHIFT);

			if (argc > 0) {
				if (*atob(*argv, &asid)) {
					printf("bad asid: %s\n", *argv);
					return(1);
				}
				argv++; argc--;
			} else
				asid = (int)(tlbhi & TLBHI_PIDMASK);

			if (argc > 0) {
				printf("extraneous arguments\n");
				return(1);
			}

			tmpinx = (int)probe_tlb((char *)vfntov(vfn), asid);

			if ( (tmpinx >= 0) && (tmpinx != tindex) ) {
				printf("0x%Ix:%d already mapped at tindex %d\n",
					vfn, asid, tmpinx);
				printf("%d:\t%R\n\t\t%R\n\t\t%R\n",
					tmpinx,
					get_tlblo0(tmpinx), tlblo01_desc,
					get_tlblo1(tmpinx), tlblo01_desc,
					get_tlbhi(tmpinx), tlbhi_desc);
				return(0);
			}
			break;
	}

	/*
	 * If we're trying to set the global bit, then 
	 * set it in both entries.
	 */
	if (tlblo_proto.pgi & TLBLO_G) {
		tlblo0.pgi |= TLBLO_G;
		tlblo1.pgi |= TLBLO_G;
	}
	tlbwired(tindex,  asid, (caddr_t)vfntov(vfn), tlblo0.pgi, tlblo1.pgi);

	printf("%d:\t%R\n\t\t%R\n\t\t%R\n", tindex, get_tlblo0(tindex),
		tlblo01_desc, get_tlblo1(tindex), tlblo01_desc, 
		get_tlbhi(tindex), tlbhi_desc);
	return(0);
}
#endif /* R4000 || R10000 */

#if TFP
/*ARGSUSED*/
int
_tlbmap(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	int tindex, tset;
	int tmpinx;
	numinp_t vfn, pfn;
	int asid;
	pde_t tlblo_proto;
	pde_t tlblo;
	machreg_t tlbhi, tlbhi_orig, tlbset_orig;
	int which = 0;
	int algo, tlbset=1;    /* avoid set 0 - has wired entries */
	char *cp;
	numinp_t vaddr;
	long xormask, probe_found;

	argc--; argv++;


	tlblo_proto.pgi = 0;

	while (argc > 0 && **argv == '-') {
		cp = &(*argv)[1];
		do {
			switch (*cp) {
			case 'c':
				if (--argc < 1 || *atob(*++argv, &algo)) {
					printf("specify a numeric consistency algorithm\n");
					return(1);
				}
				tlblo_proto.pgi |=
					(tlblo_proto.pgi & TLBLO_CACHMASK) |
					((algo<<TLBLO_CACHSHIFT)&TLBLO_CACHMASK);
				break;
			case 's':
				if (--argc < 1 || *atob(*++argv, &tlbset)) {
					printf("specify a numeric tlbset\n");
					return(1);
				}
				if (tlbset == 0)
				  printf("CAUTION: set 0 contains WIRED entries\n");
				break;
				
			case 'v':
				tlblo_proto.pgi |= TLBLO_V;
				break;
				
			case 'd':
				tlblo_proto.pgi |= TLBLO_D;
				break;
				
			default:
				printf("unknown flag: %s\n", *argv);
				return(1);
			}
		} while (*++cp);
		argc--; argv++;
	}

	if (argc < 3) {
		printf("Used to change entries in the tlbmap. Usage:\n");
 		printf("tlbmap [-s set] [-c algo] [-(d|v)*] vaddr pfn asid\n");
		return(1);
	}

	if (*atob_s(*argv, &vaddr)) {
		printf("bad vaddr: %s\n", *argv);
		return(1);
	}
	argv++; argc--;

	if (*atob_s(*argv, &pfn)) {
		printf("bad pfn: %s\n", *argv);
		return(1);
	}
	argv++; argc--;
	tlblo_proto.pgi |= (pfn << TLBLO_PFNSHIFT);

	if (*atob(*argv, &asid)) {
		printf("bad asid: %s\n", *argv);
		return(1);
	}
	argv++; argc--;
	/*
	 * Get current contents of specified tlb entry.
	 */

	tlbhi = (vaddr & TLBHI_VPNMASK) |
			((asid << TLBHI_PIDSHIFT) & TLBHI_PIDMASK);

	if (argc > 0) {
		printf("extraneous arguments\n");
		return(1);
	}

	tlblo.pgi = tlblo_proto.pgi;

	tlbhi_orig = get_cp0_tlbhi();	/* save ASID */
	tlbset_orig = get_cp0_tlbset();
	set_cp0_badvaddr(vaddr);	
	set_cp0_tlblo(tlblo.pgi);
	set_cp0_tlbhi(tlbhi);
	printf("tlbmap:\t%R\t0x%x<VA=0x%x,PID=%d>\n",
		tlblo, tlblo01_desc,
		tlbhi, vaddr, asid);
	if ((probe_found = (long)cp0_tlb_probe()) >= 0)
		printf("tlb probe HIT in set %d\n", probe_found);
	else {
		set_cp0_tlbset(tlbset);
	}

	cp0_tlb_write();	
	set_cp0_tlbhi(tlbhi_orig);	/* restore ASID */
	set_cp0_tlbset(tlbset_orig);	/* restore original tlbset */
	return(0);
}
#endif	/* TFP */

/*ARGSUSED*/
int
_cacheflush(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	numinp_t addr;
	int count;

	if (argc > 2)
		return(1);
	if (argc == 1) {
		flush_cache();
	} else {
		if ( parse_sym_range(argv[1], &addr, &count, SW_WORD) )
			return(1);
		if (count < 0) {
			printf("bad cache spec: %s\n", argv[1]);
			return(0);
		}

		addr = sign_extend(addr);
	}
	return(0);
}

/*
 * _dbxdump - Do a dbx style dump command.
 *
 * returns non-zero if the command is not properly formed.
 */
int
_dbxdump(char *cp)
{
	char *mode;
	int indir;
	char *t;
	static char f;
	static char f2;
	static numinp_t addr;
	int count;
	int tcount;
	int base, cols, len;

	/* look for <expr> / <mode> */
	for ( t = cp; *t && *t != '/'; t++ )
		;
	
	if ( *t != '/' )
		return 1;
	
	mode = t+1;

	if ( *cp == '*' ) {
		indir = 1;
		cp++;
	} else
		indir = 0;

	/* ignore a leading '&' so that symbols that look like commands
	 * can be dumped. */
	if ( *cp == '&' )
		cp++;
	
	tcount = 0;
	if ( *cp != '\0' ) {
		*t = '\0';		/* terminate string cp */
		len = parse_sym_range(cp, &addr, &tcount,SW_WORD);
		*t = '/';		/* restore string for edit_gets() */
		if (len)
			return 0;
	}
	*t = '/';			/* restore string for edit_gets() */

	if ( indir )
		addr = *(char *)addr;


	count = 0;
	if (isdigit(*mode)) {
		for (t = mode; isdigit(*t); t++)
			count = (count * 10) + *t - '0';
		if (*t) {
			f = *t;
			f2 = *(t+1);
		}
	} else
		if (*mode) {
			f = *mode;
			f2 = *(mode+1);
		}

	if (tcount != 0)
		count = tcount;
	else if (count == 0)
		count = 1;

	switch ( f ) {
		case 'd': /* decimal short */
			base = 10; len = 2; cols = 8;
			addr &= ~1;
			break;
		case 'D': /* decimal long */
			base = 10; len = 4; cols = 4;
			addr &= ~3;
			break;
		case 'o': /* short octal */
			base = 8; len = 2; cols = 4;
			addr &= ~1;
			break;
		case 'O': /* long octal */
			base = 8; len = 4; cols = 4;
			addr &= ~3;
			break;
		case 'x': /* short hex */
			base = 16; len = 2; cols = 8;
			addr &= ~1;
			break;
		case 'X': /* long hex */
			/* Also accept /XX for 64-bit grouped dumps */
			if (f2 == 'X') {
				base = 16; len = 8; cols = 2;
				addr &= ~7;
			}
			else {
				base = 16; len = 4; cols = 4;
				addr &= ~3;
			}
			break;
		case 'b': /* byte, in octal w/ `\' before it */
			base = 8; len = 1; cols = 8;
			break;
		case 's': /* byte as char with no quotes */
			base = 1; len = 1; cols = 64;
			break;
		case 'c': /* byte as char with single quotes */
			base = 0; len = 1; cols = 16;
			break;
		case 'I':
		case 'i':
			base = 1; len = 4; cols = 1;
			addr &= ~3;
			break;
		case 'g': /* floating point */
		case 'f':
			printf("floating point formats not supported.\n");
			return 0;
		default:
			return 1;
	}

	/* make sure if we're K2 that our xlations are setup */
	if (IS_KSEG2(addr) && !IS_MAPPED_KERN_SPACE(addr)) {
		__psunsigned_t map_addr = addr & ~(NBPC - 1);
		__psunsigned_t last_addr = addr + (len * count);

		for ( ; map_addr < last_addr; map_addr += NBPC) {
			if (probe_tlb((caddr_t)map_addr, 0)) {
				/* no return on failure */
				map_k2seg_addr((void *)map_addr);
			}
		}
	}

	for (tcount = count; tcount > 0;) {
		int i;

		if ( f != 'i' )
			printf("%#08Ix: ", addr);
		for (i = 0; i < cols && tcount-- > 0; i++) {
			long long v;

			if (len == 1) {
				v = GET_MEMORY(addr, SW_BYTE);
				addr += 1;
				if (base == 16)
					printf("%02Ix ",v);
				else if (base == 8 )
					printf("\\%#Io ",v);
				else if (base==1)
					while ( v ) {
						if (isprint(v))
							putchar((int)v);
						else
							putchar('.');
						v = GET_MEMORY(addr, SW_BYTE);
						addr += 1;
					}
				else if (base==0 && isprint(v))
					printf("`%c' ",(int)v);
				else
					printf("?");
			} else if (len == 2) {
				v = GET_MEMORY(addr, SW_HALFWORD);
				addr += 2;
				if (base == 10)
					printf("%Id ",v);
				else if (base == 16)
					printf("%04Ix ",v);
				else if (base == 8 )
					printf("%#Io ",v);
			} else if (len == 8) {
				/* Only support /XX for now. */
				v = GET_MEMORY(addr, SW_DOUBLEWORD);
				addr += 8;
				printf("%016Ix ",v);
			} else {
				v = GET_MEMORY(addr, SW_WORD);
				if (base == 10)
					printf("%Id ",v);
				else if (base == 16)
					printf("%08Ix ",v);
				else if (base == 8 )
					printf("%#Io ",v);
				else if (base == 1 ) {
					if (PRINTINSTRUCTION(addr,0,0)==8) {
						addr += 4;
						tcount--;
					}
					addr += 4;
					goto nonl;
				}
				addr += 4;
			}
		}
		putchar('\n');
		nonl:;
	}
	return 0;
}

/*
 * clear - clear textport screen
 *
 * This only works on ANSI X3.64 style terminals.
 */
/*ARGSUSED*/
int
clear(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
    puts("\033[H\033[J");
    return 0;
}

/* a wrapper for the dump routine to handle K2seg address */
int
_dump(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	int _argc = argc;
	char **_argv = argv;
	static int width;
        static __psunsigned_t addr;
	static long savecnt;
	long cnt;

        if (!width) {		/* set defaults on first entry */
#if _MIPS_SIM == _ABI64
		width = SW_DOUBLEWORD;
#else
		width = SW_WORD;
#endif
		addr = K0_RAMBASE;
		savecnt = 20;
	}

	while (--argc > 0) {
		if (**++argv == '-') {
			switch ((*argv)[1]) {
			
			case 'b':
				width = SW_BYTE;
				break;

			case 'h':
				width = SW_HALFWORD;
				break;

			case 'w':
				width = SW_WORD;
				break;

			case 'd':
				width = SW_DOUBLEWORD;
				break;

			default:	/* ignore other options */
				continue;
			}
		} else {
			struct range range;

			if (!parse_range(*argv, width, &range))
				return 1;

			addr = range.ra_base;
			cnt = range.ra_count;
			if (cnt == 1 && !index(*argv,'#'))
				cnt = savecnt;

			break;
		}
	}

	savecnt = cnt;

	if (IS_KSEG2(addr) && !IS_MAPPED_KERN_SPACE(addr)) {
		__psunsigned_t map_addr = addr & ~(NBPC - 1);
		__psunsigned_t last_addr = addr + (cnt * width);

		for ( ; map_addr < last_addr; map_addr += NBPC) {
			if (probe_tlb((caddr_t)map_addr, 0) < 0) {
				/* no return on failure */
				map_k2seg_addr((void *)map_addr);
			}
		}
	}

	return dump(_argc, _argv, argp, xxx);
}


#if R4000 || R10000
#define ALL_THREE	4
#define ADDR_SPEC	5

struct tag_regs {
	unsigned int tag_lo;
	unsigned int tag_hi;
};

static uint DispTag(int which, ulong addr, int slot, int flags);
/* flags for DispTag */
#define DO_BLANKS       1
#define DO_2WAY_BLANKS  2	/* on recurse, pass in DO_BLANKS */
#define DO_2WAY	        4


char *c_str_tab[] = { "PI cache", "PD cache", "2nd cache", "2nd cache" }; 
char *short_tab[] = { "PI:", "PD:", "S:   ", "S:   " }; 
char *blanks[] = { "   ", "     " };

static int _caddr(int which, u_numinp_t startaddr, uint byte_cnt);
static int _cidx(int which, uint startslot, uint slot_cnt);
static int _cecc(int which, u_numinp_t inaddr, uint byte_cnt);

/*
 * _c_tags: common entry point for cidx, caddr, and cecc commands
 * -- dump contents of cache-tags for specified index or address (resp.)
 *    or the contents of the taglo register, followed by the ecc 
 *    checkbits/parity for all the double-words of data in that line.
 */
enum which_cacheop { c_idx, c_addr, c_ecc };

/*ARGSUSED*/
int
_c_tags(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	int which = c_idx;
	int whichcache = ALL_THREE;
	int argvindx = 0;
	u_numinp_t parm1; /* start-addr if caddr(), else c_tag indx */
	int parm2 = 1;	/* optional: #bytes if caddr(), else #tags */

	if (!strcmp(argv[0], "caddr"))
		which = c_addr;
	else if (!strcmp(argv[0], "cecc")) {
		which = c_ecc;
		whichcache = CACH_SD;	/* default to 2nd for ecc */
	}

	if (argc < 2) {
		if (which == c_idx)
			printf("Must specify cache-tag index (slot number)\n");
		else
			printf("Must specify cache address\n");
		return(1);
	}

	++argv;		/* inc over function name */

	/* valid command syntax either cache-specification or range/index.
	 * accept a leading '-' in cache-options */
	if (argv[0][argvindx] == '-')
		argvindx++;

	switch (argv[0][argvindx]) {
		
	case 'a':
	case 'A':		/* this is default value of whichcache */
		if (which == c_ecc) {
			printf("Specific cache necessary for ecc display\n");
			return(1);
		}
		argv++;
		break;
	case 'i':
	case 'I':
		whichcache = CACH_PI;
		argv++;
		break;
	case 'd':
	case 'D':
		whichcache = CACH_PD;
		argv++;
		break;
	case 's':
	case 'S':
		whichcache = CACH_SD;
		argv++;
		break;
	default:
		if (which == c_ecc)  /* cecc displays 2ndary ecc by default */
			whichcache = CACH_SD;
		else
			whichcache = ADDR_SPEC;
		break;
	}

	if (which == c_idx)
		parm2 = 0;
	if (parse_sym_range(argv[0], (numinp_t *)&parm1, &parm2,SW_BYTE)) {
		if (which == c_idx)
			printf("Invalid index specification\n");
		else
			printf("Invalid range specification\n");
		return(1);
	}


	/* don't allow a count in 'a' option for cidx: only a dump of
	 * all three caches tag's at the specified slot
	 */
	if (which == c_idx && whichcache == ALL_THREE && parm2) {
		printf("Count parameter not valid for cidx with 'a' option\n");
		return(1);
	}

	if (which == c_idx)
		return(_cidx(whichcache,parm1,parm2));
	else if (which == c_addr)
		return(_caddr(whichcache,parm1,parm2));
	else
		return(_cecc(whichcache,parm1,parm2));

} /* _c_tags */


/* which	- CACH_PI, CACH_PD, CACH_SD, or ALL_THREE
 * startaddr	- begin dumping tag at this addr
 * byte_cnt	- Dump tags through segment of this length
 */
static int
_caddr(int which, u_numinp_t startaddr, uint byte_cnt)
{
	int PIslot, PDslot,SDslot;
	unsigned long /* PIaddr,PDaddr,*/SDaddr;
	/*uint PIincr,PDincr,SDincr;*/
	u_numinp_t addr;
	int flags;
	int slot,incr;
	u_numinp_t endaddr;

	int PIline = _icache_linesize;
	int PDline = _dcache_linesize;
	int SDline = _scache_linesize;

	if (IS_KSEG1(startaddr))	/* convert to cached */
		startaddr = K1_TO_K0(startaddr);

/* XXXX for now, don't allow user-space requests. XXXX */
	else if (IS_KUSEG(startaddr))
		startaddr = PHYS_TO_K0(startaddr);
/* XXXX for now, don't allow user-space requests. XXXX */

	switch(which) {

	case CACH_PI:
		addr = startaddr & ~(PIline-1);	/* rnd to P-line boundary */
		slot = ((addr % _icache_size)/PIline);
		incr = PIline;
		if (byte_cnt <= 1)
			endaddr = addr;
		else
			endaddr = addr+(byte_cnt & ~(PIline-1));
		break;

	case CACH_PD:
		addr = startaddr & ~(PDline-1);
		slot = ((addr % _dcache_size)/PDline);
		incr = PDline;
		if (byte_cnt <= 1)
			endaddr = addr;
		else
			endaddr = addr+(byte_cnt & ~(PDline-1));
		break;

	case CACH_SD:
		addr = startaddr & ~(SDline-1);
		slot = ((addr % _sidcache_size)/SDline);
		incr = SDline;
		if (byte_cnt <= 1)
			endaddr = addr;
		else
			endaddr = addr+(byte_cnt & ~(SDline-1));
		break;

	case ALL_THREE:	/* each of the caches may have distinct addrs,
			 * slotnumbers, and increments (because may
			 * be different cache and line sizes). */
	case ADDR_SPEC:	/* special case: ignore range; round down
			 * to 2nd line, dump that line and all PD and
			 * PI lines that map to it. */

		if ((which==ADDR_SPEC) || (which==ALL_THREE && byte_cnt > 1)) {
			startaddr &= ~(SDline-1);
			endaddr = startaddr+(byte_cnt & ~(SDline-1));
		} else
			endaddr = startaddr;

		/*PIaddr = PDaddr =*/ SDaddr = startaddr;
		PIslot = ((startaddr % _icache_size)/PIline);
		/*PIincr = PIline;*/
		PDslot = ((startaddr % _dcache_size)/PDline);
		/*PDincr = PDline;*/
		SDslot = ((startaddr % _sidcache_size)/SDline);
		/*SDincr = SDline;*/
		break;
	} /* switch */

	/* ADDR_SPEC iterates through the range dumping a 2ndary line tag,
	 * then a secondary-line-sized set of primary D slots beginning 
	 * at the address, then the same for the primary I.  If a range
	 * is specified with the 'a' option, ALL_THREE defaults to this
	 * behavior also.  'a' with a single address displays the secondary,
	 * PD, and PI lines at that address. */
	if (which == ADDR_SPEC || (which==ALL_THREE && endaddr!=startaddr) ) {
	    for (; SDaddr <= endaddr; SDaddr += SDline) {
		DispTag(CACH_SD,SDaddr,SDslot,DO_2WAY|DO_2WAY_BLANKS);
		SDslot++;
		startaddr = SDaddr;
		/* dump a 2nd-line-sized chunk of primaryD tags */
		flags = DO_2WAY|DO_2WAY_BLANKS;
		for (addr=startaddr; addr < (startaddr+SDline);addr+= PDline) {
			DispTag(CACH_PD,addr,PDslot,flags);
			flags = DO_2WAY|DO_2WAY_BLANKS|DO_BLANKS;
			PDslot++;
		}
		/* and now the primaryI */
		flags = DO_2WAY|DO_2WAY_BLANKS;
		for (addr=startaddr; addr < (startaddr+SDline);addr+= PIline) {
			DispTag(CACH_PI,addr,PIslot,flags);
			flags = DO_2WAY|DO_2WAY_BLANKS|DO_BLANKS;
			PIslot++;
		}
	    }
	} else if (which == ALL_THREE) {    /* single addr's worth of tags */
		DispTag(CACH_SD,startaddr,SDslot,DO_2WAY);
		DispTag(CACH_PD,startaddr,PDslot,DO_2WAY);
		DispTag(CACH_PI,startaddr,PIslot,DO_2WAY);
	} else {	/* generic loop to dump the range for one cache only */
		for (; addr<=endaddr; addr += incr,slot++) {
			DispTag(which,addr,slot,DO_2WAY);
		}
	}

	return(0);

} /* _caddr */

/* if PI or PD caches, slots are interpreted in units of those 
 * linesizes; else (SD, ALL_THREE, or ADDR_SPEC) they are assumed
 * to be in 2ndary-cache-linesizes. */
static int
_cidx(
	int which,	/* CACH_PI, CACH_PD, CACH_SD, or ALL_THREE */
	uint startslot,	/* begin dumping tags at this slot */
	uint slot_cnt	/* Dump this many slots total; 0 if option not used */
)
{
	int PIslot, PDslot,SDslot;
	__psunsigned_t SDaddr = K0BASE;	/* begin from 0 addr cached */
	__psunsigned_t addr;
	int slot,incr;
	struct tag_regs tag_regs;
	register __psunsigned_t startaddr, endaddr;
	int PIline = _icache_linesize;
	int PDline = _dcache_linesize;
	int SDline = _scache_linesize;

	startaddr = K0BASE;	/* begin creating addrs from 0 cached */
	slot = startslot;
	if (slot_cnt)
		slot_cnt--;
	
	switch(which) {

	case CACH_PI:
		startaddr += (startslot*PIline);
		incr = PIline;
		if (!slot_cnt)
			endaddr = startaddr;
		else
			endaddr = startaddr+(slot_cnt*PIline);
		break;

	case CACH_PD:
		startaddr += (startslot*PDline);
		incr = PDline;
		if (!slot_cnt)
			endaddr = startaddr;
		else
			endaddr = startaddr+(slot_cnt*PDline);
		break;

	case CACH_SD:
		startaddr += (startslot*SDline);
		incr = SDline;
		if (!slot_cnt)
			endaddr = startaddr;
		else
			endaddr = startaddr+(slot_cnt*SDline);
		break;

	case ADDR_SPEC:	/* dump start-slot's secondary tag and all PD and
			 * PI lines that map to it. */
	case ALL_THREE:
		startaddr += (startslot*SDline);
		if (!slot_cnt)
			endaddr = startaddr;
		else
			endaddr = startaddr+(slot_cnt*SDline);

		/* PIaddr = PDaddr =*/ SDaddr = startaddr;
		SDslot = startslot;
		/* compute the PI and PD slot from the derived 2ndary addr */
		PIslot = (int)((startaddr % _icache_size)/PIline);
		PDslot = (int)((startaddr % _dcache_size)/PDline);

		/*PIincr = PIline;*/
		/*PDincr = PDline;*/
		/*SDincr = SDline;*/
		break;

	} /* switch */

	/* ADDR_SPEC iterates through the slot-count dumping a 2ndary line
	 * tag, then a secondary-line-sized set of primary D slots beginning 
	 * at the address, then the same for the primary I. */
	if (which == ADDR_SPEC) {
	    for (; SDaddr <= endaddr; SDaddr += SDline) {
		DispTag(CACH_SD,SDaddr,SDslot,0);
		SDslot++;
		startaddr = SDaddr;
		/* dump a 2nd-line-sized chunk of primaryD tags */
		for (addr=startaddr; addr < (startaddr+SDline);addr+= PDline) {
			DispTag(CACH_PD,addr,
				PDslot,(addr==startaddr?0:DO_BLANKS));
			PDslot++;
		}
		/* and now the primaryI */
		for (addr=startaddr; addr < (startaddr+SDline);addr+= PIline) {
			DispTag(CACH_PI,addr,
				PIslot, (addr==startaddr?0:DO_BLANKS));
			PIslot++;
		}
	    }
	} else if (which == ALL_THREE) {    /* single addr's worth of tags */
		DispTag(CACH_SD,startaddr,SDslot,0);
		DispTag(CACH_PD,startaddr,PDslot,0);
		DispTag(CACH_PI,startaddr,PIslot,0);
	} else {	/* generic loop to dump the range for one cache only */
		for (addr=startaddr; addr<=endaddr; addr += incr,slot++) {
			DispTag(which,addr,slot,0);
		}
	}

	return(0);

} /* _cidx */


#define BYTESPERDBLWD	(2*sizeof(int))
static int
_cecc(
	int which,	/* CACH_PI, CACH_PD, or CACH_SD */
	u_numinp_t inaddr,	/* begin dumping tag at this addr */
	uint byte_cnt	/* Dump tags through segment of this length */
)
{
	u_numinp_t addr,origaddr;
	int slot,incr;
	int linelen;
	uint ecc;
	struct tag_regs tag_regs;
	u_numinp_t endaddr;
	int PIline = _icache_linesize;
	int PDline = _dcache_linesize;
	int SDline = _scache_linesize;

	if (IS_KSEG1(inaddr))	/* convert to cached */
		inaddr = K1_TO_K0(inaddr);

/* XXXX for now, don't allow user-space requests. XXXX */
	else if (IS_KUSEG(inaddr))
		inaddr = PHYS_TO_K0(inaddr);
/* XXXX for now, don't allow user-space requests. XXXX */


	switch(which) {

	case CACH_PI:
		addr = inaddr & ~(PIline-1);
		slot = ((addr % _icache_size)/PIline);
		linelen = PIline;
		break;

	case CACH_PD:
		addr = inaddr & ~(PDline-1);
		slot = ((addr % _dcache_size)/PDline);
		linelen = PDline;
		break;

	default:	/* fall through to scache */
		which = CACH_SD;
	case CACH_SD:
		addr = inaddr & ~(SDline-1);
		slot = ((addr % _sidcache_size)/SDline);
		linelen = SDline;
		break;

	} /* switch */

	incr = BYTESPERDBLWD;
	if (byte_cnt <= 1)
		endaddr = (addr + linelen);	/* do one line */
	else {
		/* round end_addr UP */
		if (byte_cnt % linelen)
			byte_cnt += linelen;
		endaddr = addr+(byte_cnt & ~(linelen-1));
		if (endaddr < (addr + linelen))
			endaddr = (addr + linelen);
	}

	printf("%s ECC checkbits in hex, 0x%x to 0x%x:\n",
		c_str_tab[which],addr,endaddr);

	origaddr = addr;
	for (; addr < endaddr; addr += incr) {
		if (!(addr % linelen)) {
			if (addr != origaddr) {
				printf("\n");
				slot++;
			}
			ecc = DispTag(which,addr,slot,DO_BLANKS);
			printf("    %x ",ecc);
		} else {
			struct tag_regs tag_regs;
			ecc = _read_tag(which, addr, (uint *)&tag_regs);
			printf("%x ",ecc);
		}
	}
	printf("\n");
	return(0);

} /* _cecc */


static uint
DispTag(int which, ulong addr, int slot, int flags)
{
	struct tag_regs tag_regs;
	uint ecc;

	ecc = _read_tag(which, (k_machreg_t)PTR_EXT(addr), (uint *)&tag_regs);

	if (which == CACH_SD) {
		printf("%s 0x%x (slot %d):\t%R\n",
		       ((flags&DO_BLANKS)?blanks[1]:short_tab[which]),
		       addr,slot,tag_regs.tag_lo,S_TAGLO_DESC());
	} else {
		printf("  %s 0x%x (slot %d):\t%R\n",
		       ((flags&DO_BLANKS)?blanks[0]:short_tab[which]),
		       addr,slot,tag_regs.tag_lo,P_TAGLO_DESC());
	}

	if ((flags & DO_2WAY) && IS_R10000()) {
		DispTag(which, (u_numinp_t)(((int64_t)addr)+1), 
			slot, (DO_2WAY_BLANKS ? DO_BLANKS : 0));
	}

	return ecc;
} /* DispTag */

#if R10000

#define	ICACHE_ADDR(line, way)	(((line) * CACHE_ILINE_SIZE) + K0BASE + (way))
#define	DCACHE_ADDR(line, way)  (((line) * CACHE_DLINE_SIZE) + K0BASE + (way))
#define	SCACHE_ADDR(line, way)  (((line) * CACHE_SLINE_SIZE) + K0BASE + (way))
#define	HEX(x)	((x) > 9 ? ('a' - 10 + (x)) : '0' + (x))

static	void
printPrimaryInstructionCacheLine(il_t *il, __uint64_t addr, int contents)
/*
 * Function: printPrimaryInstructionCacheLine
 * Purpose: To print out a primary cache line
 * Parameters:	il - pointer to fillled in "i-cache" struct.
 *		addr - address used
 *		contents - if true, contents of line are displayed.
 * Returns: nothing
 */
{
#   define	STATEMOD(t) (((t) & CTP_STATEMOD_MASK) >> CTP_STATEMOD_SHFT)
    static char * stateMod[] = {
	"invalid", 
	"normal ",
	"incon  ",
	"invalid",
	"refill ",
	"invalid",
	"invalid"
    };

#   define	STATE(t) (((t) & CTP_STATE_MASK) >> CTP_STATE_SHFT)
    static char * state[] = {
	"invalid  ",
	"shared   ",
	"clean-ex ",
	"dirty-ex "
    };
    int	i;

    printf("Tag 0x%x address=0x%x s=(%d)%s sm=(%d)%s ",
	     il->il_tag, 
	     (((il->il_tag & CTP_TAG_MASK) >> CTP_TAG_SHFT) << 12)
	         + (addr & (iCacheSize() / 2 -1)),
	     STATE(il->il_tag), state[STATE(il->il_tag)], 
	     STATEMOD(il->il_tag), stateMod[STATEMOD(il->il_tag)]);
    printf("sp(%d) scw(%d) lru(%d)\n",
	     (il->il_tag & CTP_STATEPARITY_MASK) >> CTP_STATEPARITY_SHFT,
	     (il->il_tag & CTP_SCW) ? 1 : 0, (il->il_tag & CTP_LRU) ? 1 : 0);

    if (!contents)
	return;

    for (i = 0; i < IL_ENTRIES; i += 4) {
	printf("\t0x%09x 0x%09x 0x%09x 0x%09x   Parity=[0x%x 0x%x 0x%x 0x%x]\n",
		 il->il_data[i], il->il_data[i+1],
		 il->il_data[i+2], il->il_data[i+3],
		 (__uint64_t)il->il_parity[i],
		 (__uint64_t)il->il_parity[i+1], 
		 (__uint64_t)il->il_parity[i+2],
		 (__uint64_t)il->il_parity[i+3]);
    }
#   undef	STATEMOD
#   undef	STATE
}
static	void
printPrimaryDataCacheLine(dl_t *dl, __uint64_t addr, int contents)
/*
 * Function: printPrimaryDataCacheLine
 * Purpose: To print out a primary cache line
 * Parameters:	dl - pointer to fillled in "d-cache" struct.
 *		addr - address used
 *		contents - if true, contents of line are displayed.
 * Returns: nothing
 */
{
#   define	STATEMOD(t) (((t) & CTP_STATEMOD_MASK) >> CTP_STATEMOD_SHFT)
    static char * stateMod[] = {
	"invalid", 
	"normal ",
	"incon  ",
	"invalid",
	"refill ",
	"invalid",
	"invalid"
    };

#   define	STATE(t) (((t) & CTP_STATE_MASK) >> CTP_STATE_SHFT)
    static char * state[] = {
	"invalid  ",
	"shared   ",
	"clean-ex ",
	"dirty-ex "
    };
    int	i;
    __uint32_t	*d;

    printf("Tag 0x%x address=0x%x s=(%d)%s sm=(%d)%s ",
	     dl->dl_tag,
	     (((dl->dl_tag & CTP_TAG_MASK) >> CTP_TAG_SHFT) << 12)
	         + (addr & (dCacheSize() / 2 -1)),
	     STATE(dl->dl_tag), state[STATE(dl->dl_tag)], 
	     STATEMOD(dl->dl_tag), stateMod[STATEMOD(dl->dl_tag)]);
    printf("sp(%d) scw(%d) lru(%d)\n",
	     (dl->dl_tag & CTP_STATEPARITY_MASK) >> CTP_STATEPARITY_SHFT,
	     (dl->dl_tag & CTP_SCW) ? 1 : 0, (dl->dl_tag & CTP_LRU) ? 1 : 0);

    if (!contents)
	return;

    d = (__uint32_t *)dl->dl_data;
    for (i = 0; i < DL_ENTRIES; i += 2) {
	printf("\t0x%08x 0x%08x 0x%08x 0x%08x   Parity=[0x%x 0x%x 0x%x 0x%x]\n",
		 (__uint64_t)d[i*2], (__uint64_t)d[i*2+1],
		 (__uint64_t)d[i*2+2], (__uint64_t)d[i*2+3],
		 (__uint64_t)dl->dl_ecc[i*2], (__uint64_t)dl->dl_ecc[i*2+1],
		 (__uint64_t)dl->dl_ecc[i*2+2], (__uint64_t)dl->dl_ecc[i*2+3]);
    }
#   undef	STATEMOD
#   undef	STATE
}

#define	SC_STATE(t) ((__uint32_t)(((t) & CTS_STATE_MASK) >> CTS_STATE_SHFT))
static char * sc_state[] = {
    "invalid ", "shared  ", "clean-ex", "dirty-ex",
};
#ifdef EVEREST
#define	SC_CC_STATE(t) ((__uint32_t)(((t) & CTD_STATE_MASK) >> CTD_STATE_SHFT))
static char * sc_cc_state[] = {
    "invalid", "shared", "*****", "exclusive"
};
#endif

static	void
printSecondaryCacheLine(sl_t *sl, __uint64_t addr, int contents)
/*
 * Function: printSecondaryCacheLine
 * Purpose: To print out a secondary cache line
 * Parameters:	sl - pointer to secondary line
 *		addr - address used.
 *		contents - if true, contents of line are displayed.
 * Returns: nothing
 */
{
    int		i;
    __uint32_t	*d;
    __uint64_t	tmpAddr;
    __uint64_t	maskAddr = ~((sCacheSize() / 2) - 1);

    tmpAddr = (((sl->sl_tag & CTS_TAG_MASK) >> CTS_TAG_SHFT) << 18) & maskAddr;
    tmpAddr += addr & ~maskAddr;

    printf("T5 (0x%x): addr 0x%x state=(%d)%s Vidx(%d) ECC(0x%x) MRU(%d)\n",
	     sl->sl_tag,
	     tmpAddr,
	     SC_STATE(sl->sl_tag), sc_state[SC_STATE(sl->sl_tag)], 
	     (sl->sl_tag & CTS_VIDX_MASK) >> CTS_VIDX_SHFT,
	     sl->sl_tag & CTS_ECC_MASK, (sl->sl_tag & CTS_MRU) ? 1 : 0);

#ifdef EVEREST
    tmpAddr = ((sl->sl_cctag & CTD_TAG_MASK) << 18) & maskAddr;
    tmpAddr += addr & ~maskAddr;

    printf("CC (0x%x%x): addr 0x%x state=(%d)%s\n",
	     sl->sl_cctag >> 32, ((__uint64_t)(__uint32_t)(sl->sl_cctag)),
	     tmpAddr,
	     SC_CC_STATE(sl->sl_cctag), 
	     sc_cc_state[SC_CC_STATE(sl->sl_cctag)]);
#endif

    if (!contents)
	return;

    for (i = 0; i < SL_ENTRIES; i += 2)
	printf("\t0x%016x 0x%016x   Parity/ECC=0x%03x\n",
		 sl->sl_data[i], sl->sl_data[i+1],
		 (__uint64_t)sl->sl_ecc[i>>1]); 
}

void
dumpSecondaryLine(int line, int way, int contents)
/*
 * Function: dumpSecondaryLine
 * Purpose: To display a secondary cache line contents.
 * Parameters:  line - line # (Index) to dump
 *		way - cahe line way
 *		contents - if true, dump contexts of line with tags.
 * Returns: nothing
 */
{
    sl_t	sl;
    __uint64_t	addr;

    addr = SCACHE_ADDR(line, 0);
    sLine(addr + way, &sl);
    printSecondaryCacheLine(&sl, addr, contents);
}

void
dumpSecondaryLineAddr(__uint64_t addr, int way, int contents)
{
    sl_t	sl;

    addr |= K0BASE;
    addr &= ~(CACHE_SLINE_SIZE-1);
    sLine(addr + way, &sl);
    printSecondaryCacheLine(&sl, addr, contents);
}

int
dumpPrimaryInstLine(int line, int way, int contents)
/*
 * Function: dumpPrimaryLine
 * Purpose: To retireve and dump a primary cache line.
 * Parameters:  line- line number to dump
 *		way - which cache way 0/1
 *		contents - 0 - tags only, 1 - data too.
 * Returns: 0 OK, !0 failed
 */
{
    il_t	il;
    __uint64_t	addr;

    if (line >= iCacheSize() / CACHE_ILINE_SIZE) {
	return(1);
    }

    addr = ICACHE_ADDR(line, 0);
    iLine(addr+way, &il);
    printPrimaryInstructionCacheLine(&il, addr, contents);
    return(0);
}
int
dumpPrimaryInstLineAddr(__uint64_t addr, int way, int contents)
/*
 * Function: dumpPrimaryLine
 * Purpose: To retireve and dump a primary cache line.
 * Parameters:  line- line number to dump
 *		way - which cache way 0/1
 *		contents - 0 - tags only, 1 - data too.
 * Returns: 0 OK, !0 failed
 */
{
    il_t	il;

    addr |= K0BASE;
    addr &= ~(CACHE_ILINE_SIZE - 1);
    iLine(addr + way, &il);
    printPrimaryInstructionCacheLine(&il, addr, contents);
    return(0);
}

int
dumpPrimaryDataLine(int line, int way, int contents)
/*
 * Function: dumpPrimaryDataLine
 * Purpose: To retireve and dump a primary cache line.
 * Parameters:  line- line number to dump
 *		way - which cache way 0/1
 *		contents - 0 - tags only, 1 - data too.
 * Returns: 0 OK, !0 failed
 */
{	
    dl_t	dl;
    __uint64_t	addr;

    if (line >= dCacheSize() / CACHE_DLINE_SIZE) {
	return(1);
    }

    addr = DCACHE_ADDR(line, way);
    dLine(addr, &dl);
    printPrimaryDataCacheLine(&dl, addr, contents);
    return(0);
}
int
dumpPrimaryDataLineAddr(__uint64_t addr, int way, int contents)
/*
 * Function: dumpPrimaryDataLine
 * Purpose: To retireve and dump a primary cache line.
 * Parameters:  line- line number to dump
 *		way - which cache way 0/1
 *		contents - 0 - tags only, 1 - data too.
 * Returns: 0 OK, !0 failed
 */
{	
    dl_t	dl;

    addr |= K0BASE;
    addr &= ~(CACHE_DLINE_SIZE - 1);
    
    dLine(addr + way, &dl);
    printPrimaryDataCacheLine(&dl, addr, contents);
    return(0);
}

/*ARGSUSED*/
int
dumpPrimaryCache(int ac, char *av[], char *ep[], struct cmd_table *c)
/*
 * Function: dumpPrimaryCache
 * Purpose: Display the entire primary data Cache
 * Parameters: contents - if true, display cache contents
 * Returns: nothing
 */
{
    int	lines, i;

    lines = (dCacheSize() / CACHE_DLINE_SIZE) / 2;

    for (i = 0; i < lines; i++) {
	dumpPrimaryDataLine(i, 0, 1);
	dumpPrimaryDataLine(i, 1, 1);
    }

    return 0;
}

/*ARGSUSED*/
int
dumpSecondaryCache(int ac, char *av[], char *ep[], struct cmd_table *c)
/*
 * Function: dumpSecondaryCache
 * Purpose: To dump the secondary cache tags and contents
 * Parameters: contents - true--> dump tags + DATA,
 * Returns: nothing
 */
{
    int	lines, i;

    lines  = (sCacheSize() / CACHE_SLINE_SIZE) / 2;

    for (i = 0; i < lines; i++) {
	dumpSecondaryLine(i, 0, 1); /* way 0 */
	dumpSecondaryLine(i, 1, 1); /* way 1 */
    }

    return 0;
}

/*ARGSUSED*/
int
_dump_sline(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	u_numinp_t parm1; /* start-addr */

	argv++;
	if (parse_sym_range(argv[0], (numinp_t *)&parm1, 0, 0)) {
		printf("Invalid addr specification\n");
		return(1);
	}

	dumpSecondaryLineAddr((long)parm1, 0, 1);
	dumpSecondaryLineAddr((long)parm1, 1, 1);

	return(0);
}

/*ARGSUSED*/
int
_dump_dline(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	u_numinp_t parm1; /* start-addr */

	argv++;
	if (parse_sym_range(argv[0], (numinp_t *)&parm1, 0, 0)) {
		printf("Invalid addr specification\n");
		return(1);
	}

	dumpPrimaryDataLineAddr((long)parm1, 0, 1);
	dumpPrimaryDataLineAddr((long)parm1, 1, 1);

	return(0);
}

/*ARGSUSED*/
int
_dump_iline(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	u_numinp_t parm1; /* start-addr */

	argv++;
	if (parse_sym_range(argv[0], (numinp_t *)&parm1, 0, 0)) {
		printf("Invalid addr specification\n");
		return(1);
	}

	dumpPrimaryInstLineAddr((long)parm1, 0, 1);
	dumpPrimaryInstLineAddr((long)parm1, 1, 1);

	return(0);
}


#ifdef EVEREST		/* IP28/30 don't have bus tags */
static	int
compareTag(__uint64_t a, int strict)
/*
 * Function: compareTag
 * Purpose:  To compare the addressess in a CC tag/T5 tag
 * Parameters: a - address to compare
 *   	 	strict - if true, complain about anything even if invalid.
 * Returns: 0 - compare equal, !0 not equal.
 */
{
    sl_t	sl;
    __uint64_t	t5Addr, maskAddr;
    __uint32_t	t5State;
    __uint64_t ccAddr;
    __uint32_t ccState, stateEqual;

    sLine(a, &sl);
    maskAddr = ~((sCacheSize() / 2) - 1);

    /* Convert to addresses - and compare */

    t5Addr = (((sl.sl_tag & CTS_TAG_MASK) >> CTS_TAG_SHFT) << 18) & maskAddr;
    t5State = SC_STATE(sl.sl_tag);

    ccAddr = (((sl.sl_cctag & CTD_TAG_MASK) >> CTD_TAG_SHFT) << 18) & maskAddr;
    ccState = SC_CC_STATE(sl.sl_cctag);

    if (!strict && t5State == CTS_STATE_I) {
	/* Who cares what the T5 says then */
	return(0);
    }

    switch(t5State) {
    case CTS_STATE_I:
	stateEqual = (ccState == CTD_STATE_I);
	break;
    case CTS_STATE_S:
	if (!strict && ccState == CTD_STATE_X) {
	    stateEqual = 1;
	} else {
	    stateEqual = (ccState == CTD_STATE_S);
	}
	break;
    case CTS_STATE_CE:
    case CTS_STATE_DE:
	stateEqual = (ccState == CTD_STATE_X);
	break;
    default:
	stateEqual = 0;
	break;
    }
    
    if ((t5Addr != ccAddr) || !stateEqual) {
	printf("CC/T5 tag mismatch: vaddr=0x%x index=%d way=%d\n",
		 a & ~1, (a % (sCacheSize() >> 1)) / CACHE_SLINE_SIZE, a & 1);
	printf("\tT5[0x%x%x] address=0x%x, state=%s(%d)\n", 
		 sl.sl_tag >> 32, (__uint64_t)(__uint32_t)sl.sl_tag,
		 t5Addr, sc_state[t5State], t5State);
	printf("\tCC[0x%x%x] address=0x%x, state=%s(%d)\n",
		 sl.sl_cctag >> 32, (__uint64_t)(__uint32_t)sl.sl_cctag,
		 ccAddr, sc_cc_state[ccState], ccState);
	return(1);
    }
    return(0);
}

void
compareSecondaryTags(int all, int strict)
/*
 * Function: compareSecondaryTags
 * Purpose: To compare the contents of the secondary cache tags with the
 *	    SCC duplicate tags.
 * Parameters: all - if true, print all tags that mis-compare, if false, 
 *	  	     stop on the first error.
 *		strict - if true, complain about all mismatches, even if
 *			they would not cause an operational problem.
 * Returns: nothing
 */
{
    int		lines, curLine;
	
    lines = sCacheSize() / CACHE_SLINE_SIZE; /* number of cache lines */
    lines >>= 1;			/* 2-way */

    for (curLine = 0; curLine < lines; curLine++) {
	if (compareTag(SCACHE_ADDR(curLine, 0), strict) && !all) {
	    return;
	}
	if (compareTag(SCACHE_ADDR(curLine, 1), strict) && !all) {
	    return;
	}
    }
}

/*ARGSUSED*/
void
_dump_ct(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	compareSecondaryTags(0, 0);
}

/*ARGSUSED*/
void
_dump_cta(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	compareSecondaryTags(1, 0);
}

/*ARGSUSED*/
void
_dump_cts(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	compareSecondaryTags(0, 1);
}

/*ARGSUSED*/
void
_dump_ctas(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	compareSecondaryTags(1, 1);
}
#endif /* EVEREST */
#endif /* R10000 */

#endif	/* R4000 || R10000 */
