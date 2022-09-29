/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.125 $"

#include <sys/types.h>
#include <sys/cachectl.h>
#include <sys/callo.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/debug.h>
#include <sys/dump.h>
#include <sys/errno.h>
#include <sys/loaddrs.h>
#include <sys/map.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/kthread.h>
#include <sys/uthread.h>
#include <sys/sbd.h>
#include <sys/schedctl.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
#include <sys/systm.h>
#include <ksys/exception.h>
#include <sys/atomic_ops.h>
#include <sys/inst.h>
#include <sys/ecc.h>
#include <ksys/cacheops.h>
#include <ksys/as.h>
#include <sys/runq.h>

#ifdef EVEREST
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/mc3.h>
/* Until we know code is correct, report cache errors
 * but consider them FATAL and panic.  Removing this
 * define will re-enable correction.
 */
/* #define IP19_CACHEERRS_FATAL 1 */
/* #define FORCE_CACHERR_ON_STORE 1 */

/* Following are flags which can be turned on to test very specific error
 * conditions. 
 * ECC_TEST_EW_BIT attempts to generate an EW condition (unsuccessfully)
 * ECC_TEST_TWO_BAD causes two bad cachelines to be setup so we can see
 *		what happens when another cpu references the other line.
 */
/*  #define ECC_TEST_EW_BIT	1 */
/* #define	ECC_TEST_TWO_BAD 1 */
#endif /* EVEREST */

#include <sys/ioerror.h>

#ifdef _MEM_PARITY_WAR
#if IP20 || IP22 
#include <sys/mc.h>
#endif /* IP20 || IP22 */

#include <sys/pfdat.h>
#endif /* _MEM_PARITY_WAR */

#if IP20 || IP22 || IPMHSIM
#define GIO_ERRMASK 0xff00
extern int perr_mem_init(caddr_t);
#endif /* IP20 || IP22 */


extern struct reg_desc sr_desc[], cause_desc[];
#if R4000 && R10000
extern struct reg_desc r10k_sr_desc[];
#endif /* R4000 && R10000 */
extern int picache_size;
extern int pdcache_size;

#ifdef R4000
static void init_ecc_info(void);
#endif

void ecc_cleanup(void);

#define SET_CBITS_IN	0x80

#if EVEREST
void dump_hwstate(int);
#endif

#ifdef R4000PC
extern int get_r4k_config(void);
int r4000_config;
#endif /* R4000PC */

extern char bytetab[];
#define BYTEOFF(bl)	((bl&0xf0)?(bytetab[bl>>4]):(bytetab[bl]+4))

/*
 * CP0 status register description
 */
struct reg_values imask_values[] = {
	{ SR_IMASK8,	"8" },
	{ SR_IMASK7,	"7" },
	{ SR_IMASK6,	"6" },
	{ SR_IMASK5,	"5" },
	{ SR_IMASK4,	"4" },
	{ SR_IMASK3,	"3" },
	{ SR_IMASK2,	"2" },
	{ SR_IMASK1,	"1" },
	{ SR_IMASK0,	"0" },
	{ 0,		NULL },
};

struct reg_values mode_values[] = {
	{ SR_KSU_USR,	"USER" },
#if R4000 || R10000
	{ SR_KSU_KS,	"SPRVSR" },
#endif
	{ 0,		"KERNEL" },
	{ 0,		NULL },
};

#if TFP
struct reg_values kps_values[] = {
	{ SR_KPS_4K,	"4k" },
	{ SR_KPS_8K,	"8k" },
	{ SR_KPS_16K,	"16k" },
	{ SR_KPS_64K,	"64k" },
	{ SR_KPS_1M,	"1m" },
	{ SR_KPS_4M,	"4m" },
	{ SR_KPS_16M,	"16m" },
	{ 0,		NULL },
};

struct reg_values ups_values[] = {
	{ SR_UPS_4K,	"4k" },
	{ SR_UPS_8K,	"8k" },
	{ SR_UPS_16K,	"16k" },
	{ SR_UPS_64K,	"64k" },
	{ SR_UPS_1M,	"1m" },
	{ SR_UPS_4M,	"4m" },
	{ SR_UPS_16M,	"16m" },
	{ 0,		NULL },
};

struct reg_desc sr_desc[] = {
	/* mask	     shift      name   format  values */
	{ SR_DM,	0,	"DM",	NULL,	NULL },
	{ SR_KPSMASK,	0,	"KPS",	NULL,	kps_values },
	{ SR_UPSMASK,	0,	"UPS",	NULL,	ups_values },
	{ SR_CU1,	0,	"CU1",	NULL,	NULL },
	{ SR_CU0,	0,	"CU0",	NULL,	NULL },
	{ SR_FR,	0,	"FR",	NULL,	NULL },
	{ SR_RE,	0,	"RE",	NULL,	NULL },
	{ SR_IBIT8,	0,	"IM8",	NULL,	NULL },
	{ SR_IBIT7,	0,	"IM7",	NULL,	NULL },
	{ SR_IBIT6,	0,	"IM6",	NULL,	NULL },
	{ SR_IBIT5,	0,	"IM5",	NULL,	NULL },
	{ SR_IBIT4,	0,	"IM4",	NULL,	NULL },
	{ SR_IBIT3,	0,	"IM3",	NULL,	NULL },
	{ SR_IBIT2,	0,	"IM2",	NULL,	NULL },
	{ SR_IBIT1,	0,	"IM1",	NULL,	NULL },
	{ SR_IMASK,	0,	"IPL",	NULL,	imask_values },
	{ SR_XX,	0,	"XX",	NULL,	NULL },
	{ SR_UX,	0,	"UX",	NULL,	NULL },
	{ SR_KSU_MSK,	0,	"MODE",	NULL,	mode_values },
	{ SR_EXL,	0,	"EXL",	NULL,	NULL },
	{ SR_IE,	0,	"IE",	NULL,	NULL },
	{ 0,		0,	NULL,	NULL,	NULL },
};

#elif defined (BEAST)
struct reg_desc sr_desc[] = {
	/* mask	     shift      name   format  values */
	{ SR_CU2,	0,	"CU2",	NULL,	NULL },
	{ SR_CU1,	0,	"CU1",	NULL,	NULL },
	{ SR_CU0,	0,	"CU0",	NULL,	NULL },
	{ SR_FR,	0,	"FR",	NULL,	NULL },
	{ SR_RE,	0,	"RE",	NULL,	NULL },
	{ SR_SR,	0,	"SR",	NULL,	NULL },
	{ SR_NMI,	0,	"NMI",	NULL,	NULL },
	{ SR_CE,	0,	"CE",	NULL,	NULL },
	{ SR_IBIT10,	0,	"IM10",	NULL,	NULL },
	{ SR_IBIT9,	0,	"IM9",	NULL,	NULL },
	{ SR_IBIT8,	0,	"IM8",	NULL,	NULL },
	{ SR_IBIT7,	0,	"IM7",	NULL,	NULL },
	{ SR_IBIT6,	0,	"IM6",	NULL,	NULL },
	{ SR_IBIT5,	0,	"IM5",	NULL,	NULL },
	{ SR_IBIT4,	0,	"IM4",	NULL,	NULL },
	{ SR_IBIT3,	0,	"IM3",	NULL,	NULL },
	{ SR_IBIT2,	0,	"IM2",	NULL,	NULL },
	{ SR_IBIT1,	0,	"IM1",	NULL,	NULL },
	{ SR_IMASK,	0,	"IPL",	NULL,	imask_values },
	{ SR_KSU_MSK,	0,	"MODE",	NULL,	mode_values },
	{ SR_EXL,	0,	"EXL",	NULL,	NULL },
	{ SR_IE,	0,	"IE",	NULL,	NULL },
	{ 0,		0,	NULL,	NULL,	NULL },
};

#else /* !TFP && !BEAST */
struct reg_desc sr_desc[] = {
#if R4000 && R10000
	/* mask	     shift      name   format  values */
	{ SR_CU3,	0,	"CU3",	NULL,	NULL },
	{ SR_CU2,	0,	"CU2",	NULL,	NULL },
	{ SR_CU1,	0,	"CU1",	NULL,	NULL },
	{ SR_CU0,	0,	"CU0",	NULL,	NULL },
	{ SR_RP,	0,	"RP",	NULL,	NULL },
	{ SR_FR,	0,	"FR",	NULL,	NULL },
	{ SR_RE,	0,	"RE",	NULL,	NULL },
	{ SR_RE,	0,	"RE",	NULL,	NULL },
	{ SR_BEV,	0,	"BEV",	NULL,	NULL },
	{ SR_TS,	0,	"TS",	NULL,	NULL },
	{ SR_SR,	0,	"SR",	NULL,	NULL },
	{ SR_CH,	0,	"CH",	NULL,	NULL },
	{ SR_CE,	0,	"CE",	NULL,	NULL },
	{ SR_DE,	0,	"DE",	NULL,	NULL },
	{ SR_IBIT8,	0,	"IM8",	NULL,	NULL },
	{ SR_IBIT7,	0,	"IM7",	NULL,	NULL },
	{ SR_IBIT6,	0,	"IM6",	NULL,	NULL },
	{ SR_IBIT5,	0,	"IM5",	NULL,	NULL },
	{ SR_IBIT4,	0,	"IM4",	NULL,	NULL },
	{ SR_IBIT3,	0,	"IM3",	NULL,	NULL },
	{ SR_IBIT2,	0,	"IM2",	NULL,	NULL },
	{ SR_IBIT1,	0,	"IM1",	NULL,	NULL },
	{ SR_IMASK,	0,	"IPL",	NULL,	imask_values },
	{ SR_KX,	0,	"KX",	NULL,	NULL },
	{ SR_SX,	0,	"SX",	NULL,	NULL },
	{ SR_UX,	0,	"UX",	NULL,	NULL },
	{ SR_KSU_MSK,	0,	"MODE",	NULL,	mode_values },
	{ SR_ERL,	0,	"ERL",	NULL,	NULL },
	{ SR_EXL,	0,	"EXL",	NULL,	NULL },
	{ SR_IE,	0,	"IE",	NULL,	NULL },
	{ 0,		0,	NULL,	NULL,	NULL },
};

struct reg_desc r10k_sr_desc[] = {
#endif /* R4000 && R10000 */
	/* mask	     shift      name   format  values */
#ifdef R10000
	{ SR_XX,	0,	"XX",	NULL,	NULL },
#else
	{ SR_CU3,	0,	"CU3",	NULL,	NULL },
#endif
	{ SR_CU2,	0,	"CU2",	NULL,	NULL },
	{ SR_CU1,	0,	"CU1",	NULL,	NULL },
	{ SR_CU0,	0,	"CU0",	NULL,	NULL },
#ifndef R10000
	{ SR_RP,	0,	"RP",	NULL,	NULL },
#endif
	{ SR_FR,	0,	"FR",	NULL,	NULL },
	{ SR_RE,	0,	"RE",	NULL,	NULL },
	{ SR_RE,	0,	"RE",	NULL,	NULL },
	{ SR_BEV,	0,	"BEV",	NULL,	NULL },
	{ SR_TS,	0,	"TS",	NULL,	NULL },
	{ SR_SR,	0,	"SR",	NULL,	NULL },
	{ SR_CH,	0,	"CH",	NULL,	NULL },
#ifdef R10000
	{ SR_NMI,	0,	"NMI",	NULL,	NULL },
#else
	{ SR_CE,	0,	"CE",	NULL,	NULL },
#endif
	{ SR_DE,	0,	"DE",	NULL,	NULL },
	{ SR_IBIT8,	0,	"IM8",	NULL,	NULL },
	{ SR_IBIT7,	0,	"IM7",	NULL,	NULL },
	{ SR_IBIT6,	0,	"IM6",	NULL,	NULL },
	{ SR_IBIT5,	0,	"IM5",	NULL,	NULL },
	{ SR_IBIT4,	0,	"IM4",	NULL,	NULL },
	{ SR_IBIT3,	0,	"IM3",	NULL,	NULL },
	{ SR_IBIT2,	0,	"IM2",	NULL,	NULL },
	{ SR_IBIT1,	0,	"IM1",	NULL,	NULL },
	{ SR_IMASK,	0,	"IPL",	NULL,	imask_values },
	{ SR_KX,	0,	"KX",	NULL,	NULL },
	{ SR_SX,	0,	"SX",	NULL,	NULL },
	{ SR_UX,	0,	"UX",	NULL,	NULL },
	{ SR_KSU_MSK,	0,	"MODE",	NULL,	mode_values },
	{ SR_ERL,	0,	"ERL",	NULL,	NULL },
	{ SR_EXL,	0,	"EXL",	NULL,	NULL },
	{ SR_IE,	0,	"IE",	NULL,	NULL },
	{ 0,		0,	NULL,	NULL,	NULL },
};
#endif

/*
 * CP0 cause register description
 */
struct reg_values exc_values[] = {
	{ EXC_INT,	"INT" },
	{ EXC_MOD,	"MOD" },
	{ EXC_RMISS,	"RMISS" },
	{ EXC_WMISS,	"WMISS" },
	{ EXC_RADE,	"RADE" },
	{ EXC_WADE,	"WADE" },
#if !TFP
	{ EXC_IBE,	"IBE" },
	{ EXC_DBE,	"DBE" },
#endif
	{ EXC_SYSCALL,	"SYSCALL" },
	{ EXC_BREAK,	"BREAK" },
	{ EXC_II,	"II" },
	{ EXC_CPU,	"CPU" },
	{ EXC_OV,	"OV" },
	{ EXC_TRAP,	"TRAP" },
#if R4000
	{ EXC_VCEI,	"VCEI" },
	{ EXC_FPE,	"FPE" },
	{ EXC_WATCH,	"WATCH" },
	{ EXC_VCED,	"VCED" },
#endif
#if R10000
	{ EXC_FPE,	"FPE" },
#ifndef R4000
	{ EXC_WATCH,	"WATCH" },
#endif /* !R4000 */
#endif /* R10000 */
	{ 0,		NULL },
};

struct reg_desc cause_desc[] = {
	/* mask	     shift      name   format  values */
	{ CAUSE_BD,	0,	"BD",	NULL,	NULL },
	{ CAUSE_CEMASK,	-CAUSE_CESHIFT,	"CE",	"%d",	NULL },
#if TFP
	{ CAUSE_NMI,	0,	"NMI",	NULL,	NULL },
	{ CAUSE_BE,	0,	"BE",	NULL,	NULL },
	{ CAUSE_VCI,	0,	"VCI/TLBX", NULL, NULL },
	{ CAUSE_FPI,	0,	"FPI",	NULL,	NULL },
	{ CAUSE_IP11,	0,	"IP11",	NULL,	NULL },
	{ CAUSE_IP10,	0,	"IP10",	NULL,	NULL },
	{ CAUSE_IP9,	0,	"IP9",	NULL,	NULL },
#endif
	{ CAUSE_IP8,	0,	"IP8",	NULL,	NULL },
	{ CAUSE_IP7,	0,	"IP7",	NULL,	NULL },
	{ CAUSE_IP6,	0,	"IP6",	NULL,	NULL },
	{ CAUSE_IP5,	0,	"IP5",	NULL,	NULL },
	{ CAUSE_IP4,	0,	"IP4",	NULL,	NULL },
	{ CAUSE_IP3,	0,	"IP3",	NULL,	NULL },
	{ CAUSE_SW2,	0,	"SW2",	NULL,	NULL },
	{ CAUSE_SW1,	0,	"SW1",	NULL,	NULL },
	{ CAUSE_EXCMASK,0,	"EXC",	NULL,	exc_values },
	{ 0,		0,	NULL,	NULL,	NULL },
};

#if !defined (TFP) && !defined (BEAST)
#if ((!defined(R10000)) || defined(R4000))
struct reg_desc cache_err_desc[] = {
	/* mask		     shift      name   format  values */
	{ CACHERR_ER,		0,	"ER",	NULL,	NULL },
	{ CACHERR_EC,		0,	"EC",	NULL,	NULL },
	{ CACHERR_ED,		0,	"ED",	NULL,	NULL },
	{ CACHERR_ET,		0,	"ET",	NULL,	NULL },
	{ CACHERR_ES,		0,	"ES",	NULL,	NULL },
	{ CACHERR_EE,		0,	"EE",	NULL,	NULL },
	{ CACHERR_EB,		0,	"EB",	NULL,	NULL },
	{ CACHERR_EI,		0,	"EI",	NULL,	NULL },
#if IP19
	{ CACHERR_EW,		0,	"EW",	NULL,	NULL },
#endif
	{ CACHERR_SIDX_MASK,	0,	"SIDX",	"0x%x",	NULL },
	{ CACHERR_PIDX_MASK,	CACHERR_PIDX_SHIFT,	"PIDX",	"0x%x",	NULL },
	{ 0,			0,	NULL,	NULL,	NULL },
};
#endif	/* ((!defined(R10000)) || defined(R4000)) */


#define SSTATE_INVALID	0
#define SSTATE_CLEX	4
#define SSTATE_DIRTEX	5
#define SSTATE_SHARED	6
#define SSTATE_DIRTSHAR	7

struct reg_values scache_states[] = {
#if R4000 && R10000
	{ SSTATE_INVALID,	"INVAL" },
	{ SSTATE_CLEX,		"CE" },
	{ SSTATE_DIRTEX,	"DE" },
	{ SSTATE_SHARED,	"shared" },
	{ SSTATE_DIRTSHAR,	"dirty-shared" },
	{ 0,			NULL },
};
struct reg_values r10k_scache_states[] = {
#endif /* R4000 && R10000 */
#ifdef R10000
	{ SSTATE_INVALID,	"INVAL" },
	{ SSTATE_SHARED,	"shared" },
	{ SSTATE_CLEX,		"CE" },
	{ SSTATE_DIRTEX,	"DE" },
#else
	{ SSTATE_INVALID,	"INVAL" },
	{ SSTATE_CLEX,		"CE" },
	{ SSTATE_DIRTEX,	"DE" },
	{ SSTATE_SHARED,	"shared" },
	{ SSTATE_DIRTSHAR,	"dirty-shared" },
#endif
	{ 0,			NULL },
};

#define PSTATE_INVALID	0
#define PSTATE_SHARED	1
#define PSTATE_CLEX	2
#define PSTATE_DIRTEX	3

struct reg_values pcache_states[] = {
	{ PSTATE_INVALID,	"INVAL" },
	{ PSTATE_SHARED,	"shared" },
	{ PSTATE_CLEX,		"CE" },
	{ PSTATE_DIRTEX,	"DE" },
	{ 0,			NULL },
};

#define STAG_LO			0xffffe000
#define STAG_STATE		0x00001c00
#define STAG_STATE_SHIFT	-10
#define STAG_VINDEX		0x00000380
#define STAG_ECC		SECC_MASK
#define STAG_VIND_SHIFT		5	/* taglo bits 31..13 << 35..17 */
#ifdef R10000
#define SECC_MASK		0x0000007f
#define SADDR_SHIFT		4
#endif

struct reg_desc s_taglo_desc[] = {
	/* mask		shift			name   	format  values */
	{ STAG_LO,	SADDR_SHIFT,		"paddr","0x%x",	NULL },
	{ STAG_STATE,	STAG_STATE_SHIFT,	NULL,	NULL,	scache_states },
	{ STAG_VINDEX,	STAG_VIND_SHIFT,	"vind",	"0x%x",	NULL },
	{ STAG_ECC,	0,			"ecc",	"0x%x", NULL },
	{ 0,		0,			NULL,	NULL,	NULL },
};
#if R4000 && R10000
struct reg_desc r10k_s_taglo_desc[] = {
	/* mask		shift			name   	format  values */
	{ STAG_LO,	SADDR_SHIFT,		"paddr","0x%x",	NULL },
	{ STAG_STATE,	STAG_STATE_SHIFT,	NULL,	NULL, r10k_scache_states },
	{ STAG_VINDEX,	STAG_VIND_SHIFT,	"vind",	"0x%x",	NULL },
	{ STAG_ECC,	0,			"ecc",	"0x%x", NULL },
	{ 0,		0,			NULL,	NULL,	NULL },
};
#endif /* R4000 && R10000 */

#define PTAG_LO			0xffffff00
#define PTAG_STATE		0x000000c0
#define PTAG_STATE_SHIFT	-6
#define PTAG_PARITY		0x00000001
#ifdef R10000
#define PTAG_WAY		0x00000002
#define PTAG_SP			0x00000004
#define PTAG_LRU		0x00000008
#define PADDR_SHIFT		4
#endif

struct reg_desc p_taglo_desc[] = {
	/* mask		shift		name	format	values */
	{ PTAG_LO,	PADDR_SHIFT,	"paddr","0x%x",	NULL },
	{ PTAG_STATE,	PTAG_STATE_SHIFT,NULL,	NULL,	pcache_states },
#ifdef R10000
	{ PTAG_LRU,	0,		"LRU",	NULL,	NULL },
	{ PTAG_SP,	2,		"SP",	"%d",	NULL },
	{ PTAG_WAY,	1,		"WAY",	"%d",	NULL },
#endif
	{ PTAG_PARITY,	0,		"parity","%x",	NULL },
	{ 0,		0,		NULL,	NULL,	NULL },
};



#if IP19
#undef PHYS_TO_K0
#undef K0_TO_PHYS
extern __psunsigned_t ecc_phys_to_k0( __psunsigned_t);
extern __psunsigned_t ecc_k0_to_phys( __psunsigned_t);
#define	PHYS_TO_K0	ecc_phys_to_k0
#define	K0_TO_PHYS	ecc_k0_to_phys

/* The standard ECC_INTERRUPT macro makes cached references and
 * we have ERL and DE set, so cache errors during these kernel
 * routines would go un-reported.
 */
#define ECC_INTERRUPT
/* only routines called during ecc handling use the following macro, and
 * they all execute at splhi with SR_DE set, so no locking is necessary */
#define MARK_FOR_CLEANUP	ecc_info_param->needs_cleanup = 1
/* as handler exits, if cleanup is needed it raises an interrupt; else
 * it decrements the w_index */
#define CLEANUP_IS_NEEDED	(ecc_info_param->needs_cleanup)
#else /* !IP19 */
/* ecc_handler mustn't do anything that could cause exceptions (printing,
 * for example) since we aren't on a stack that the exception code 
 * recognizes.  It therefore raises a software interrupt that invokes
 * ecc_cleanup() to do its dirty work. */
#define ECC_INTERRUPT		timeout(ecc_cleanup, 0, TIMEPOKE_NOW); \
				ecc_info.needs_cleanup = 0; \
				call_cleanup = 1 \

/* only routines called during ecc handling use the following macro, and
 * they all execute at splhi with SR_DE set, so no locking is necessary */
#define MARK_FOR_CLEANUP	ecc_info.needs_cleanup = 1
/* as handler exits, if cleanup is needed it raises an interrupt; else
 * it decrements the w_index */
#define CLEANUP_IS_NEEDED	(ecc_info.needs_cleanup)
#endif



char *etstrings[] = {"OK", "DB", "CB", "2 Bit", "3 Bit", "4 Bit", "Fatal"};

eccdesc_t real_data_eccsyns[] = {
/*	  0|8	  1|9	  2|A     3|B     4|C     5|D     6|E     7|F    */
/* 0*/  {OK, 0},{CB, 0},{CB, 1},{B2, 0},{CB, 2},{B2, 0},{B2, 0},{DB, 7},
/* 8*/  {CB, 3},{B2, 0},{B2, 0},{DB,54},{B2, 0},{DB, 6},{DB,55},{B2, 0},
/*10*/  {CB, 4},{B2, 0},{B2, 0},{DB, 0},{B2, 0},{DB,20},{DB,48},{B2, 0},
/*18*/  {B2, 0},{DB,24},{DB,28},{B2, 0},{DB,16},{B2, 0},{B2, 0},{DB,52},
/*20*/  {CB, 5},{B2, 0},{B2, 0},{DB, 1},{B2, 0},{DB,21},{DB,49},{B2, 0},
/*28*/  {B2, 0},{DB,25},{DB,29},{B2, 0},{DB,17},{B2, 0},{B2, 0},{DB, 4},
/*30*/  {B2, 0},{DB,44},{DB,45},{B2, 0},{DB,46},{B2, 0},{B2, 0},{B3, 0},
/*38*/  {DB,47},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*40*/  {CB, 6},{B2, 0},{B2, 0},{DB, 2},{B2, 0},{DB,22},{DB,50},{B2, 0},
/*48*/  {B2, 0},{DB,26},{DB,30},{B2, 0},{DB,18},{B2, 0},{B2, 0},{DB,10},
/*50*/  {B2, 0},{DB,32},{DB,33},{B2, 0},{DB,34},{B2, 0},{B2, 0},{B3, 0},
/*58*/  {DB,35},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*60*/  {B2, 0},{DB,12},{DB,13},{B2, 0},{DB,14},{B2, 0},{B2, 0},{B3, 0},
/*68*/  {DB,15},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*70*/  {DB, 9},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*78*/  {B2, 0},{B3, 0},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B2, 0},{UN, 0},
/*80*/  {CB, 7},{B2, 0},{B2, 0},{DB, 3},{B2, 0},{DB,23},{DB,51},{B2, 0},
/*88*/  {B2, 0},{DB,27},{DB,31},{B2, 0},{DB,19},{B2, 0},{B2, 0},{DB,58},
/*90*/  {B2, 0},{DB,36},{DB,37},{B2, 0},{DB,38},{B2, 0},{B2, 0},{B3, 0},
/*98*/  {DB,39},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*a0*/  {B2, 0},{DB,40},{DB,41},{B2, 0},{DB,42},{B2, 0},{B2, 0},{B3, 0},
/*a8*/  {DB,43},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*b0*/  {DB,56},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*b8*/  {B2, 0},{B3, 0},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B2, 0},{UN, 0},
/*c0*/  {B2, 0},{DB,60},{DB,61},{B2, 0},{DB,62},{B2, 0},{B2, 0},{B3, 0},
/*c8*/  {DB,63},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*d0*/  {DB, 8},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*d8*/  {B2, 0},{B3, 0},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B2, 0},{UN, 0},
/*e8*/  {DB,57},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*e8*/  {B2, 0},{B3, 0},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B2, 0},{UN, 0},
/*f8*/  {B2, 0},{DB, 5},{DB,53},{B2, 0},{DB,59},{B2, 0},{B2, 0},{UN, 0},
/*f8*/  {DB,11},{B2, 0},{B2, 0},{UN, 0},{B2, 0},{UN, 0},{UN, 0},{B2, 0},
};


eccdesc_t real_tag_eccsyns[] = {
/*	  0|8	  1|9	  2|A     3|B     4|C     5|D     6|E     7|F    */
/* 0 */	{OK, 0},{CB, 0},{CB, 1},{B2, 0},{CB, 2},{B2, 0},{B2, 0},{DB, 0},
/* 8 */	{CB, 3},{B2, 0},{B2, 0},{DB,16},{B2, 0},{DB, 4},{DB, 5},{B2, 0},
/*10*/	{CB, 4},{B2, 0},{B2, 0},{DB,22},{B2, 0},{DB,17},{DB, 1},{B2, 0},
#ifdef R4000
/*18*/	{B2, 0},{UN, 0},{UN, 0},{B2, 0},{DB, 6},{B2, 0},{B2, 0},{B3, 0},
#else
/*18*/	{B2, 0},{UN, 0},{DB,25},{B2, 0},{DB, 6},{B2, 0},{B2, 0},{B3, 0},
#endif	/* R4000 */
/*20*/	{CB, 5},{B2, 0},{B2, 0},{DB,18},{B2, 0},{DB,24},{DB, 2},{B2, 0},
/*28*/	{B2, 0},{DB,20},{UN, 0},{B2, 0},{UN, 0},{B2, 0},{B2, 0},{B3, 0},
/*30*/	{B2, 0},{DB, 8},{DB, 9},{B2, 0},{UN, 0},{B2, 0},{B2, 0},{B3, 0},
/*38*/	{DB,10},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},

/*40*/	{CB, 6},{B2, 0},{B2, 0},{UN, 0},{B2, 0},{DB,19},{DB, 3},{B2, 0},
/*48*/	{B2, 0},{DB,23},{UN, 0},{B2, 0},{DB, 7},{B2, 0},{B2, 0},{B3, 0},
/*50*/	{B2, 0},{DB,21},{UN, 0},{B2, 0},{UN, 0},{B2, 0},{B2, 0},{B3, 0},
/*58*/	{UN, 0},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*60*/	{B2, 0},{DB,12},{DB,13},{B2, 0},{DB,14},{B2, 0},{B2, 0},{B3, 0},
/*68*/	{DB,15},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*70*/	{DB,11},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*78*/	{B3, 0},{B3, 0},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B2, 0},{B3, 0},

};

#ifdef IP19
/* really need to access all data uncached while processing a cache error
 * exception in order to not perturb the state of the cache.
 */

#define data_eccsyns	ecc_info_param->ecc_data_eccsyns
#define tag_eccsyns	ecc_info_param->ecc_tag_eccsyns
#else /* !IP19 */

#define data_eccsyns real_data_eccsyns
#define tag_eccsyns real_tag_eccsyns

#endif /* !IP19 */

#ifdef R4000

/* calc_err_info() computes the checkbits for the incoming value(s)
 * (two data uints if data, one uint (STagLo) if tag.  It then derives
 * the syndrome and uses it to fetch the eccdesc entry from the proper
 * table.  The following #defines and structures allow it to determine
 * which ecc to compute, and to return the info to the calling routine.
 */
#define DATA_CBITS	1
#define TAG_CBITS	2

/* Both data and tag ecc submit checkbits and receive computed checkbits,
 * the resulting syndrome, and syn_info.  Data ecc, however, is computed
 * from the values of two uints; tag ecc needs a portion of the STagLo
 * register.
 */
typedef struct error_info {
	unchar cbits_in;
	unchar cbits_out;
	unchar syndrome;
	eccdesc_t syn_info;
	union  {
		struct {
			uint d_lo;
			uint d_hi;
		} data_in;

		struct {
			uint s_tlo;
		} tag_in;

	 } ecc_type_t;
} error_info_t;
#define eidata_lo		ecc_type_t.data_in.d_lo
#define eidata_hi		ecc_type_t.data_in.d_hi
#define eis_taglo		ecc_type_t.tag_in.s_tlo


/* Bits in the CacheErr reg tell the handler where the ECC error occurred.
 * The sbd.h CACH_XX defines, plus the following SYSAD describe location;
 * ecc_t_or_d tells whether the error was in the data field, the tag, or
 * both.  (The primary caches have separate parity bits for data & tags; 
 * the 2ndary has separate ecc checkbits for each, 7-bit for tags, 8 for data).
 */
#define SYSAD	(CACH_SD + 1)
#define BAD_LOC	(SYSAD + 1)

static char *error_loc_names[] =  {
			"primary i-cache",	/* CACH_PI */
			"primary d-cache",	/* CACH_PD */
			"secondary i-cache",	/* CACH_SI */
			"secondary d-cache",	/* CACH_SD */
			"CPU SysAD bus",	/*  SYSAD  */
			"<bad loc>",		/* invalid */
			};

enum ecc_t_or_d { DATA_ERR = 1, TAG_ERR, D_AND_T_ERR };

#define BYTESPERWD	(sizeof(int))
#define BYTESPERDBLWD	(2 * BYTESPERWD)
#define BYTESPERQUADWD	(4 * BYTESPERWD)

#define NUM_TAGS	2
#define TAGLO_IDX	0	/* load & store cachops: lo == [0], hi [1] */
#define TAGHI_IDX	1	/* not used on IP17 (taghi must be zero!) */

/* one error mandates that the data caches be flushed (not
 * just lines that 'hit'.  Since we aren't trying to match
 * any particular virtual address, pick an arbitrary address
 * that maps to the beginning of the secondary cache. */
#define FOUR_MEG		(0x400000l)
#define FLUSH_ADDR		FOUR_MEG

/* the taglo register has different formats depending on whether the 
 * tag info is from a primary or secondary tag.  The following macros
 * return the state of the cacheline: clean or dirty, which are the
 * only valid choices on the IP17. */
#define CLEAN_P_TAG(p_tlo)	    ((p_tlo & PSTATEMASK) == PCLEANEXCL)
#define CLEAN_S_TAG(s_tlo)	    ((s_tlo & SSTATEMASK) == SCLEANEXCL)

#define DIRTY_P_TAG(p_tlo)	    ((p_tlo & PSTATEMASK) == PDIRTYEXCL)
#define DIRTY_S_TAG(s_tlo)	    ((s_tlo & SSTATEMASK) == SDIRTYEXCL)


/* In order to allow more than one ECC exception to be handled before
 * the cleanup-interrupt invokes ecc_cleanup(), define a structure
 * that contains all info relevant to each ecc exception.  An array
 * of these allows multiple exceptions.  Use two pointers, a 'writing'
 * pointer for the handler to write the frames, and a 'reading'
 * pointer for ecc_cleanup and ecc_panic to display the frames.
 * Implement them as circular buffers.
 */
#ifdef IP19
#define ECC_FRAMES	64
#else
#define ECC_FRAMES	10
#endif
/* #define ECC_DEBUG */


/* Keep a tally of the ECC errors in each part (tag or data) of each
 * cache.  Since any errors in either of the primary caches means the
 * entire R4K must be discarded, we don't track primary errors by address:
 * frequency of occurrence is sufficiently detailed.  Cache Errors
 * may be in data or tag, SysAD errors are in data only (tag ecc
 * is computed when the data is put into the cache lines).  NO_ERROR
 * keeps a count of the number of times the handler found no error
 * in the indicated spot.  This is the 'err_cnts' field in ecc_info. 
 * the #define of ECC_ERR_TYPES and the ecc_err_types enum are declared
 * in IP17.h to allow cmd/ecc to determine the array size needed when
 * doing an SGI_R4K_CERRS syscall for cache-error tallies. */

/*  +++++++++++++ sys/IP17.h +++++++++++++++ */
/*
#define ECC_ERR_TYPES	8
enum ecc_err_types { PI_DERRS = 0, PI_TERRS, PD_DERRS, PD_TERRS, 
		     SC_DERRS, SC_TERRS, SYSAD_ERRS, NO_ERROR };
*/
/*  +++++++++++++ sys/IP17.h +++++++++++++++ */

static char *err_type_names[] =  {
			"pi-d","pi-t","pd-d","pd-t",
			"sc-d","sc-t", "sysad", "noerr" };

#define	ECC_ALL_MSGS	-1
#define	ECC_PANIC_MSG	0
#define	ECC_INFO_MSG	1
#define	ECC_ERROR_MSG	2
volatile char panic_str[] = "PANIC MSG: ";
volatile char info_str[]  = "INFO MSG: ";
volatile char error_str[] = "ERROR MSG: ";
volatile char *msg_strs[] = { panic_str, info_str, error_str };

/* each ecc_handler invokation saves lots of info: */
typedef struct err_desc {
  volatile k_machreg_t e_sr;
  volatile uint e_cache_err;
  volatile k_machreg_t e_error_epc;
  volatile int e_location;	/* CACH_{ PI, PD, SI, SD } or SYSAD */
  volatile uint e_tag_or_data;	/* DATA_ERR, TAG_ERR, or D_AND_T_ERR */
  volatile __uint64_t e_paddr;	/* entire physical addr of error (16 GB)*/
  volatile k_machreg_t e_vaddr;	/* p-cache virtual addr of error */
  volatile uint e_s_taglo;
  volatile uint e_p_taglo;
  volatile uint e_badecc;
  volatile uint e_lo_badval;
  volatile uint e_hi_badval;
  volatile uint e_syndrome;
  volatile uint e_2nd_syn;
  volatile uint e_syn_info;
  volatile uint e_user;
  volatile uint e_prevbadecc;
  volatile pid_t e_pid;
  volatile cpuid_t e_cpuid;
  volatile uint e_sbe_dblwrds;	/* bit mask of double-words with SBE */
  volatile uint e_mbe_dblwrds;	/* bit mask of double-words with DBE */
#ifdef _MEM_PARITY_WAR
  volatile eframe_t *e_eframep;
  volatile k_machreg_t *e_eccframep;
#endif	/* _MEM_PARITY_WAR */
  volatile uchar_t e_flags;
} err_desc_t;

/* definitions for e_flags */
#define E_PADDR_VALID 1         /* we are certain of the physical address */
#define E_VADDR_VALID 2         /* we are certain of pidx                 */
#define E_PADDR_MC    4         /* bad address reported by MC             */
#define E_PADDR_GIO   8         /* bad address reported by HPC3           */

typedef struct ecc_info {
#ifdef IP19
  /* rest of data is referenced uncached so need to be sure no cached data
   * is in same cacheline.
   */
  char cacheline_pad1[128];
#endif /* IP19 */  
#ifdef ECC_TEST_EW_BIT
  /* The following variable will be set to "1" when the ecc_handler has
   * reached an "interesting place" and where it will wait for the "master
   * cpu" to perform a cached access to the error location.
   */
  int			ecc_wait_for_external;

  /* following fields setup by cpu2 which accesses the second bad line in
   * cpu1's cache.
   */
  int			ecc_err2_datahi;
  int			ecc_err2_datalo;
  int			ecc_err2_cpuid;

  /* cpu1 sets up the address of the second error */

  int			*ecc_err2_ptr;

  /* cpu1 logs its' cacheErr register value after the second error has been
   * accessed by cpu2.
   */

  int			ecc_cpu1_cacheerr2;
#endif
  volatile int ecc_w_index;	/* writing index (used by ecc_handler) */
  volatile int ecc_r_index;	/* reading index (ecc_cleanup &ecc_panic) */
  volatile uint needs_cleanup;
  volatile uint cleanup_cnt;
  volatile uint ecc_flags;
#ifndef _MEM_PARITY_WAR
  volatile k_machreg_t eframep;
  volatile k_machreg_t eccframep;
#endif	/* _MEM_PARITY_WAR */
  volatile uint ecc_err_cnts[ECC_ERR_TYPES];
  volatile err_desc_t desc[ECC_FRAMES];
#ifndef IP19
  volatile char *ecc_panic_msg[ECC_FRAMES];
  volatile char *ecc_info_msg[ECC_FRAMES];
  volatile char *ecc_error_msg[ECC_FRAMES];
#else	/* IP19 */
  volatile int	ecc_info_inited;
  volatile int  ecc_inval_eloc_where;
  volatile int	ecc_panic;
  volatile int	ecc_panic_cpuid;	/* cpuid of panicing cpu */
  volatile int	ecc_panic_newmaster;
  volatile int	ecc_panic_recoverable;
  volatile char ecc_panic_msg[ECC_FRAMES];
  volatile char ecc_info_msg[ECC_FRAMES];
  volatile char ecc_error_msg[ECC_FRAMES];


  /* ecc_entry_state indicates current state of the ECC_FRAME entry:
   * 0 == unused
   * 1 == ecc_handler is currently active on entry
   * 2 == ecc_handler has completed entry, awaiting ecc_cleanup
   */

  volatile uint ecc_entry_state[ECC_FRAMES];

  /* Following set of virtual addresses will be used by the kernel during
   * ECC error processing in order to access the data at the point of the
   * error without causing a VCE exception.
   */
  __psunsigned_t	ecc_vcecolor;

  /* Following location will hold copy of EVERROR_EXT so it can be picked
   * up by the ecc_handler wihtout the compiler generating loads to cached
   * global address space.
   */
  everror_ext_t		*everror_ext;


  /* global data items which need to be references uncached */

  uint			*ecc_tag_dbpos;	/* avoid cached or gp-rel reference */
  struct d_emask	*ecc_d_ptrees;
  struct t_emask	*ecc_t_ptrees;
  eccdesc_t		*ecc_data_eccsyns;
  eccdesc_t		*ecc_tag_eccsyns;

  __psunsigned_t	ecc_dummyline;
  __psunsigned_t	ecc_k0size_less1;
  pfn_t			ecc_physmem;
  int			ecc_picache_size;
  int			ecc_pdcache_size;
  int			ecc_attempt_recovery;

  /* rest of data is referenced uncached so need to be sure no cached data
   * is in same cacheline.
   */
  char cacheline_pad2[128];
#endif /* IP19 */  
} ecc_info_t;

#ifdef IP19
/* Can't load from PDA since that would be a cached access.
 * Most usage of SCACHE_PADDR is passed to "indexed load" routines
 * which will automatically size to the secondary cache in HW>
 */
#define	SCACHE_PADDR(edp)	(edp->e_paddr)
#else
#define	SCACHE_PADDR(edp)	(edp->e_paddr & (private.p_scachesize-1))
#endif
#define	POFFSET_PADDR(edp)	(edp->e_paddr & ~(NBPP-1))

#ifdef _MEM_PARITY_WAR
#define ecc_info (*((volatile ecc_info_t *) CACHE_ERR_ECCINFO_P))
#define ecc_info_ptr	ecc_info
#define ECC_INFO(a)	ecc_info.a
#else /* _MEM_PARITY_WAR */

#ifdef IP19
volatile ecc_info_t real_ecc_info;

/* the following macro should NOT be used when in ecc_handler since compiler
 * generates "gp" relative constants to perform this conversion and loading
 * these constants results in cached accesses.
 */
#define ecc_info_ptr (*(volatile ecc_info_t*)(K0_TO_K1(&real_ecc_info)))
#define ECC_INFO(a)	ecc_info_param->a

/* dummy cacheline is 3 cachelines long and we use the middle to
 * guarentee it's not on the same line as any other cached data.
 */

static long long dummy_cacheline[48];

#else	/* !IP19 */

volatile ecc_info_t ecc_info;
#define ecc_info_ptr	ecc_info
#define ECC_INFO(a)	ecc_info.a
#endif /* !IP19 */ 
#endif /* _MEM_PARITY_WAR */
#ifndef IP19
volatile int ecc_info_initialized = 0;
#endif
volatile int call_cleanup = 0;
volatile int in_cleanup = 0;

#if DEBUG_ECC
volatile uint f_ptaglo;
volatile uint f_staglo;
volatile uint f_loval;
volatile uint f_hival;
volatile __psunsigned_t f_p_caddr;
volatile __psunsigned_t f_s_caddr;
volatile uint f_cooked_ecc;
volatile uint f_d_ecc;
volatile uint f_ptaglo1;
volatile uint f_staglo1;
#endif /* DEBUG_ECC */



/* when calling print_ecc_info from symmon must use qprintf to avoid 
 * scrogging the kernel buffers.  When non-zero, this global directs 
 * all display routines to use qprintf, else printf */
volatile int pm_use_qprintf = 0;
typedef void (*pfunc)(char *, ...);
extern void qprintf(char *, ...);

#define K_ECC_PANIC		0x1
#define HANDLER_OVERRAN		0x2

#ifdef IP19
extern int ecc_check_cache( __psunsigned_t );
#else /* !IP19 */
#ifdef _MEM_PARITY_WAR

extern int log_perr(uint addr, uint bytes, int no_console, int print_help);
extern int ecc_find_pidx(int, paddr_t);

volatile char **msg_addrs[] = {	(volatile char **)NULL,
				(volatile char **)NULL,
				(volatile char **)NULL };
#else /* _MEM_PARITY_WAR */
volatile char **msg_addrs[] = {	(volatile char **)&ecc_info.ecc_panic_msg[0],
				(volatile char **)&ecc_info.ecc_info_msg[0],
				(volatile char **)&ecc_info.ecc_error_msg[0] };
#endif /* _MEM_PARITY_WAR */
#endif /* !IP19 */


#define NEXT_INDEX(x)	if (x+1 >= ECC_FRAMES) \
				x = 0; \
			else \
				x += 1

#define PREV_INDEX(x)	if (x-1 < 0) \
				x = (ECC_FRAMES-1); \
			else \
				x -= 1

#if MP
#define	PRINT_CPUID(id)	cmn_err(CE_CONT, "CPU %d: ", id)
#else
#define	PRINT_CPUID(id)
#endif

/* ecc handling prototypes */

static int print_ecctype(int, int, uint, __uint64_t, int, uint);

#if IP19
int real_calc_err_info(int, error_info_t *, volatile ecc_info_t *);
static int real_ecc_print_msg(int, uint, int, int, uint, volatile ecc_info_t *);
static int real_ecc_assign_msg(int, int, char, volatile ecc_info_t *);
static int real_ecc_fixmem(uint, eframe_t *, k_machreg_t *, uint, k_machreg_t,
		      volatile ecc_info_t *);
static int real_ecc_fixcache(uint, eframe_t *, k_machreg_t *, uint,
			     k_machreg_t,  volatile ecc_info_t *);
int real_ecc_fixctag(uint, int, volatile ecc_info_t *);
int real_ecc_fixcdata(uint, int, k_machreg_t *, volatile ecc_info_t *);
static int real_ecc_log_error(int, int, volatile ecc_info_t *);
int real_xlate_bit(enum error_type, uint, volatile ecc_info_t *);


#define ecc_print_msg(a0,a1,a2,a3,a4)	real_ecc_print_msg(a0,a1,a2,a3,a4,ecc_info_param)
#define ecc_log_error(a0,a1)	real_ecc_log_error(a0,a1,ecc_info_param)
#define ecc_assign_msg(a0,a1,a2) real_ecc_assign_msg(a0,a1,a2,ecc_info_param)
#define ecc_fixmem(a0,a1,a2,a3,a4) real_ecc_fixmem(a0,a1,a2,a3,a4,ecc_info_param)
#define ecc_fixcache(a0,a1,a2,a3,a4) real_ecc_fixcache(a0,a1,a2,a3,a4,ecc_info_param)
#define ecc_fixctag(a0,a1)	real_ecc_fixctag(a0,a1,ecc_info_param)
#define ecc_fixcdata(a0,a1,a2)	real_ecc_fixcdata(a0,a1,a2,ecc_info_param)
#define xlate_bit(a0,a1)	real_xlate_bit(a0,a1,ecc_info_param)
#define calc_err_info(a0,a1)	real_calc_err_info(a0,a1,ecc_info_param)

#else	/* !IP19 */

int calc_err_info(int, error_info_t *);
static int ecc_print_msg(int, uint, int, int, uint);
static int ecc_assign_msg(int, int, char *);
#ifndef MCCHIP
static int ecc_fixmem(uint, eframe_t *, k_machreg_t *, uint, k_machreg_t);
#endif /* ! MCCHIP */
static int ecc_fixcache(uint, eframe_t *, k_machreg_t *, uint, k_machreg_t);
int ecc_fixctag(uint, int);
int ecc_fixcdata(uint, int, k_machreg_t *);
static int ecc_log_error(int, int);
int xlate_bit(enum error_type, uint);
#endif /* !IP19 */

static int ecc_bad_ptag(uint);

int _c_hwbinv(int, __psunsigned_t);
int _c_hinv(int, __psunsigned_t);
int _c_ilt_n_ecc(int, __psunsigned_t, uint[], uint *);
int _c_ilt(int, __psunsigned_t, uint[]);
int _c_ist(int, __psunsigned_t, uint[]);
int _munge_decc(__psunsigned_t, uint);



#ifndef SCACHE_LINESIZE
#define	SCACHE_LINESIZE		(32*4)
#endif

#ifdef IP19
static char real_ecc_overrun_msg[] = "ECC error overrun!";
static char real_ecc_eb_not_i[] = "ecc_handler: EB bit set but error not i-cache";
static char real_ecc_incons_err[] = "ECC error not SysAD or either cache!";
static char real_ecc_ew_err[] = "double ECC error, incomplete information!";
static char
    real_ecc_kernel_err[] = "Uncorrectable HARDWARE ECC error, in kernel mode";
static char
    real_ecc_user_err[] = "Uncorrectable HARDWARE ECC error, in user mode";
static char real_ecc_inval_loc[] = "Invalid 'location' parameter in fixcache";
static char real_ecc_no_ptagerr[] = "No ecc tag error found in primary cacheline";
static char real_ecc_no_stagerr[] = "No ecc tag error found in secondary cacheline";
static char real_ecc_ptfix_failed[] = "ECC repair on primary tag unsuccessful";
static char real_ecc_stfix_failed[] = "ECC repair on secondary tag unsuccessful";
static char real_ecc_no_pdataerr[] = "No ecc data error found in primary cacheline";
static char real_ecc_no_sdataerr[]= "No ecc data error found in secondary cacheline";
static char real_ecc_sinvalid_noerr[]= "Secondary cacheline invalid, OK on re-read";
static char real_ecc_sinvalid_err[]= "Secondary cacheline invalid, ERROR on re-read";
static char real_ecc_sdcfix_failed[]="Data repair on clean secondary cache-line failed";
static char real_ecc_sdcfix_good[]="Data repair on clean 2nd cache-line SUCCESSFUL";
static char real_ecc_sddfix_failed[]="Data repair on dirty secondary cache-line failed";
static char real_ecc_sddfix_good[]="Data repair on dirty 2nd cache-line SUCCESSFUL";
static char real_ecc_md_sddfix_failed[]="Multi-bit data fix on dirty S-line failed";
static char real_ecc_p_data_err[] = "Data parity error in primary cache";
static char real_ecc_inval_eloc[] = "ecc_log_error: bad error location";
static char real_ecc_bad_s_tag[] = "Uncorrectable error in secondary cache tag";
static char real_ecc_ft_hinv_m_sc[] = "fixtag: _c_hinv missed cache";
static char real_ecc_scerr_too_early[] = "Scache error before recovery is possible";
static char real_ecc_ei_notdirty[] = "Scache error on store-miss but line not dirty";
static char real_ecc_mixed_psize[] = "ecc_handler expects primary linesizes equal";
static char real_ecc_ei_norecover[] = "Scache error on store-miss, recovery not possible ";
static char real_ecc_possible_ei[] = "cache test failed, possible store-miss?";

#define	ecc_overrun_msg	1
#define	ecc_eb_not_i	2
#define	ecc_incons_err	3
#define	ecc_ew_err	4
#define	ecc_kernel_err	5
#define	ecc_user_err	6
#define	ecc_inval_loc	7
#define	ecc_no_ptagerr	8
#define	ecc_no_stagerr	9
#define	ecc_ptfix_failed	10
#define	ecc_stfix_failed	11
#define	ecc_no_pdataerr	12
#define	ecc_no_sdataerr	13
#define	ecc_sinvalid_noerr	14
#define	ecc_sinvalid_err	15
#define	ecc_sdcfix_failed	16
#define	ecc_sdcfix_good	17
#define	ecc_sddfix_failed	18
#define	ecc_sddfix_good	19
#define	ecc_md_sddfix_failed	20
#define	ecc_p_data_err	21
#define	ecc_inval_eloc	22
#define	ecc_bad_s_tag	23
#define	ecc_ft_hinv_m_sc	24
#define	ecc_scerr_too_early	25
#define	ecc_ei_notdirty	26
#define	ecc_mixed_psize	27
#define	ecc_ei_norecover	28
#define ecc_possible_ei		29

#else /* !IP19 */
static char ecc_overrun_msg[] = "ECC error overrun!";
#if !MCCHIP && !IP32
static char ecc_eb_not_i[] = "ecc_handler: EB bit set but error not i-cache";
#endif
static char ecc_incons_err[] = "ECC error not SysAD or either cache!";
static char
    ecc_kernel_err[] = "Uncorrectable HARDWARE ECC error, in kernel mode";
static char
    ecc_user_err[] = "Uncorrectable HARDWARE ECC error, in user mode";
static char ecc_inval_loc[] = "Invalid 'location' parameter in fixcache";
static char ecc_no_ptagerr[] = "No ecc tag error found in primary cacheline";
static char ecc_no_stagerr[] = "No ecc tag error found in secondary cacheline";
static char ecc_ptfix_failed[] = "ECC repair on primary tag unsuccessful";
static char ecc_stfix_failed[] = "ECC repair on secondary tag unsuccessful";
static char ecc_no_pdataerr[] = "No ecc data error found in primary cacheline";
static char ecc_no_sdataerr[]= "No ecc data error found in secondary cacheline";
static char ecc_sinvalid_noerr[]= "Secondary cacheline invalid, OK on re-read";
static char ecc_sinvalid_err[]= "Secondary cacheline invalid, ERROR on re-read";
static char ecc_sdcfix_failed[]="Data repair on clean secondary cache-line failed";
static char ecc_sdcfix_good[]="Data repair on clean 2nd cache-line SUCCESSFUL";
static char ecc_sddfix_failed[]="Data repair on dirty secondary cache-line failed";
static char ecc_sddfix_good[]="Data repair on dirty 2nd cache-line SUCCESSFUL";
static char ecc_md_sddfix_failed[]="Multi-bit data fix on dirty S-line failed";
static char ecc_p_data_err[] = "Data parity error in primary cache";
static char ecc_inval_eloc[] = "ecc_log_error: bad error location";
static char ecc_bad_s_tag[] = "Uncorrectable error in secondary cache tag";
static char ecc_ft_hinv_m_sc[] = "fixtag: _c_hinv missed cache";
#if IP20 || IP22 || IP32 || IPMHSIM
static char ecc_extreq[] = "ecc_handler: ECC error result of external request";
#endif
#endif /* !IP19 */


#ifdef IP19_CACHEERRS_FATAL
volatile int verbose_ecc = 1;	/* get lots of info on the (single) error */
#else
#ifdef IP19
volatile int verbose_ecc = 1;	/* for now, get lots of info on correction too */
#else /* !IP19 */
volatile int verbose_ecc = 0;
#endif /* !IP19 */
#endif
volatile int syslog_ecctype = 1;

#if IP19
extern real_ecc_panic(void);

/* This routine is invoked from doacvec() when a cpu is checking for
 * cpuvactions and it finds no pending actions.
 * This solves the problem of a cpu which is trying to panic due to an ecc
 * error in the cache.  We know that continuing to execute on that cpu causes
 * problems in some circumstances, so we attempt to change processors for
 * the actual panic.
 *
 * Now, if the original failing cpu is the "master" cpu (which normally
 * checks for ecc_cleanup() calls and panics, then we assign a new "master"
 * and send it a cpuaction interrupt with NO pending actions (since we
 * don't want a cpu with a bad cache to start fetching lines from another
 * cpu). We don't want the other cpus to always perform this check since
 * it involves an uncached access which is very slow.
 */

void
doacvec_check_ecc_logs(void)
{

	if (ecc_info_ptr.ecc_panic == 1)
		real_ecc_panic( );
}

/* This routine is invoked from ducons_write() in order to determine if the
 * master cpu has moved.  If it has, it returns the new master cpuid.
 * Otherwise ducons_write will run the system out of cpuaction blocks trying
 * to send console actions to cpu 0.
 */

int
ecc_panic_newmaster(void)
{
	if (ecc_info_ptr.ecc_panic_newmaster)
		return(ecc_info_ptr.ecc_panic_newmaster - 1);
	return(0);
}

/* Following routine primarily used by do_mprboot() in order to determine
 * that a cpu has died due to an ecc_panic.  This is needed to avoid delaying
 * the system restart waiting for all cpus to enter do_mprboot() since the
 * cpu which died with an ecc error will never enter that routine since we
 * attempt to keep it busy "idling" so it does no further damage.
 *
 * NOTE: only returns an indication that we're in ecc_panic.  In the future
 * it might be a good idea to return the number of cpus which have invoked
 * ecc_panic in case we have several simulataneous failures (maybe due to
 * bad memory).
 */
int
ecc_panic_deadcpus(void)
{
	if (ecc_info_ptr.ecc_panic)
		return(1);
	else
		return(0);
}

/* this routine is invoked from system clock processing to check
 * if we need to perform ecc cleanup.  This avoids having the ecc_handler
 * making cached references while ERL and DE are set.
 */
void
ecc_interrupt_check(void)
{
	if (ecc_info_ptr.needs_cleanup) {
		extern real_ecc_panic(void);

		if (ecc_info_ptr.ecc_panic)
			real_ecc_panic( );

		ecc_info_ptr.needs_cleanup = 0;

		timeout(ecc_cleanup, 0, TIMEPOKE_NOW);
		call_cleanup = 1;
	}
#ifdef ECC_TEST_EW_BIT
	/* This code assumes that certain locations are useable.  Should be
	 * generalized.
	 */
	if (ecc_info_ptr.ecc_wait_for_external == 1) {
		  
		  ecc_info_ptr.ecc_err2_datahi =
		  	*ecc_info_ptr.ecc_err2_ptr;

		  ecc_info_ptr.ecc_err2_datalo =
		  	*(ecc_info_ptr.ecc_err2_ptr+1);

		  ecc_info_ptr.ecc_err2_cpuid = cpuid();
		  
		  ecc_info_ptr.ecc_wait_for_external = 2;
	}
#endif /* ECC_TEST_EW_BIT */
}
#endif /* IP19 */
/* called from os/clock.c:timein, which is invoked due to a software
 * interrupt (see #define of ECC_INTERRUPT), ecc_cleanup does all the
 * work that ecc_handler can't finish because it is a) executing with
 * ecc exceptions and interrupts disabled, and b) on an isolated
 * stack which won't work with nested exceptions such as K2 tlbfaults.
 * These cleanup actions are primarily printing and more detailed 
 * logging of errors. */
void
ecc_cleanup(void)
{
	int index;
	uint ospl;
	err_desc_t *edp;	/* ptr to set of variables to set this time */
	int	i;

#if IP19
	volatile ecc_info_t *ecc_info_param;

	ecc_info_param = (volatile ecc_info_t *)(K0_TO_K1(&real_ecc_info));

	/* Only let one cpu at a time enter this code */
	if (atomicSetInt((int *)&in_cleanup, 1))
		return;
#endif
    while (1) {
	ospl = splecc();	/* lock during index incr and test */

#ifdef IP19
	/* We check to see if the ecc_handler has finished updating the
	 * entry we're about to read (it updates the ecc_w_index before
	 * the entry is complete)
	 */
	index = ecc_info_ptr.ecc_r_index;
	NEXT_INDEX(index);

	if ((ecc_info_ptr.ecc_r_index == ecc_info_ptr.ecc_w_index) ||
	    (ecc_info_ptr.ecc_entry_state[index] != 2)) {

		if (ecc_info_ptr.ecc_entry_state[index] != 0)
			ecc_info_ptr.needs_cleanup = 1;	/* try again later */
		in_cleanup = 0;
		/* if the handler hasn't bumped the w_index, call_cleanup
		 * should still be 0 from the last time through the while */
		ASSERT(!call_cleanup);
		splxecc(ospl);
		return;
	} else {
		call_cleanup = 0;
	}
#else /* !IP19 */
	if (ecc_info_ptr.ecc_r_index == ecc_info_ptr.ecc_w_index) {
		in_cleanup = 0;
		/* if the handler hasn't bumped the w_index, call_cleanup
		 * should still be 0 from the last time through the while */
		ASSERT(!call_cleanup);
		splxecc(ospl);
		return;
	} else {
		in_cleanup = 1;
		call_cleanup = 0;
	}
#endif /* !IP19 */

	/* cleanup uses the (trailing) read-index */
	NEXT_INDEX(ecc_info_ptr.ecc_r_index);
	index = ecc_info_ptr.ecc_r_index;
	ecc_info_ptr.cleanup_cnt++;
 	splxecc(ospl);

	/* point edb to set of variables to use */
	edp = (err_desc_t *)&(ecc_info_ptr.desc[index]);

	if (verbose_ecc || edp->e_user) {
		PRINT_CPUID(edp->e_cpuid);
		cmn_err(CE_CONT,"  ecc_cleanup: %d times (r_index %d w_index %d)\n",
			ecc_info_ptr.cleanup_cnt,
			ecc_info_ptr.ecc_r_index, ecc_info_ptr.ecc_w_index);
	}

	/* always display error msgs for SYSLOG */
	ecc_print_msg(ECC_ERROR_MSG,index,1,1,edp->e_cpuid);

#ifdef IP19
	if (ecc_info_param->ecc_attempt_recovery)
		cmn_err(CE_WARN,"Data may have been corrupted by scache error\n");
#endif /* IP19 */

	if (syslog_ecctype)
		print_ecctype(edp->e_location, edp->e_tag_or_data,
			edp->e_syndrome, edp->e_paddr, 1, edp->e_cpuid);

	if (edp->e_user || verbose_ecc) {
		for (i = ECC_INFO_MSG; i >= ECC_PANIC_MSG; i--)
			ecc_print_msg(i,index,1,1,edp->e_cpuid);
		PRINT_CPUID(edp->e_cpuid);
		cmn_err(CE_CONT,"    c_err %R, err_epc 0x%x\n",
			edp->e_cache_err, cache_err_desc,
			edp->e_error_epc);

		if (edp->e_user || verbose_ecc) {
			PRINT_CPUID(edp->e_cpuid);
			cmn_err(CE_CONT,"    s_taglo %R%secc 0x%x e_pc 0x%x\n",
				edp->e_s_taglo,
#if R4000 && R10000
				IS_R10000() ? r10k_s_taglo_desc :
#endif /* R4000 && R10000 */
					s_taglo_desc,
				(edp->e_s_taglo ? "\n    " : "  "),
				edp->e_badecc, edp->e_error_epc);
			PRINT_CPUID(edp->e_cpuid);
			cmn_err(CE_CONT,"    data_lo 0x%x data_hi 0x%x sbe dblwrds 0x%x mbe dblwrds 0x%x\n",
				edp->e_lo_badval, edp->e_hi_badval,
				edp->e_sbe_dblwrds, edp->e_mbe_dblwrds);
			PRINT_CPUID(edp->e_cpuid);
			cmn_err(CE_CONT,
				"    Err SR %R%spaddr 0x%llx, vaddr 0x%x\n",
				edp->e_sr,
#if R4000 && R10000
				IS_R10000() ? r10k_sr_desc :
#endif /* R4000 && R10000 */
					sr_desc,
				(edp->e_sr ? "\n    " : "  "),
				edp->e_paddr,edp->e_vaddr);
		}
	}

	if (edp->e_user) {
#ifdef _MEM_PARITY_WAR
		dobuserr((struct eframe_s *)edp->e_eframep,
			(inst_t *)edp->e_error_epc, 2);
#else
		dobuserr((struct eframe_s *)ecc_info_ptr.eframep,
			(inst_t *)edp->e_error_epc, 2);
#endif
		if (edp->e_pid) {
			while (sigtopid(edp->e_pid, SIGBUS,
					SIG_ISKERN|SIG_NOSLEEP,
					0, 0, 0) == EAGAIN)
				;
			PCB(pcb_resched) = 1;
		} else
			cmn_err(CE_WARN,
				"NULL curuthread with user ecc error!\n");
		edp->e_user = 0;
		edp->e_pid = 0;
	}
#ifdef IP19
	ecc_info_ptr.ecc_entry_state[index] = 0;
#endif	
    } /* while */

} /* ecc_cleanup */


#if IP19

void
ecc_panic(volatile ecc_info_t *ecc_info_param)
{
	/* We keep cached access to a minimum, though it appaears that stores
	 * are the real problem (cached ones that is).
	 */
	ecc_info_param->ecc_panic = 1;
	ecc_info_param->needs_cleanup = 1;
	ecc_info_param->ecc_panic_cpuid = cpuid();

	/* Not much we can do if only one cpu, go ahead and try to panic */

	if (maxcpus == 1) {
		real_ecc_panic();
	}

	/* If we're the master cpu, try nominating a new master and send
	 * it an interrupt.  Otherwise no-one notices the cache error since
	 * it is only the master cpu which polls the uncached location
	 * "needs_cleanup" once per second.
	 */

	if (private.p_flags & PDAF_MASTER) {
	  	cpuid_t who;

		if (cpuid() == 0)
			who = 1;
		else
			who = 0;

		ecc_info_param->ecc_panic_newmaster = who+1;
		sendintr(who, DOACTION);
	} else
		/* attempt to wakeup the master more quickly than its'
		 * one second maintaince processing.  May not work, but
		 * will be noticed eventually.
		 */
		sendintr(masterpda->p_cpuid, DOACTION);

	/* wait for other cpu to actually report the panic.  If this cpu
	 * does ANYTHING cached it may corrupt other data if this error
	 * was due to a store-miss.
	 */

	/* wait for reset signal (HW) from master cpu */

	while (1)
		;
}

real_ecc_panic()
{
	volatile ecc_info_t *ecc_info_param =
		(volatile ecc_info_t *)(K0_TO_K1(&real_ecc_info));
	
#else /* !IP19 */
/* ARGSUSED */
ecc_panic(
	uint cache_err,
	uint errorepc)
{
#endif	/* !IP19 */
	err_desc_t *edp;	/* ptr to set of variables to set this time */
	/* use w_index, handler was working there when it panic'ed */
	int index = ECC_INFO(ecc_w_index);
#if defined (IP19)
	if (ecc_info_param->ecc_info_inited != 1) 
		/* NOTE: No machine_error_dump() called in this case, but I've
		 * seen cpus "hang" trying to print that info and nothing comes
		 * out on the console.  So get simple error message out first.
		 */
	        cmn_err_tag(69,CE_PANIC|CE_CPUID,
			    "CPU cache error occurred before handler inited\n");

	/* Set ecc_panic to 2 to indicate that we're already processing
	 * the panic condition.  Decreases the chance that another cpu will
	 * try this at the same time.  Not MP safe, but the cache error recovery
	 * logic is not MP safe for other reasons, so just keep risk to a minimum.
	 */

	ecc_info_param->ecc_panic = 2;

	/* See if we're supposed to become the "master" due to a cache error
	 * on the real "master" which is now unresponsive in an
	 * "idle forever" loop.
	 */
	 
	if ((ecc_info_param->ecc_panic_newmaster) &&
	    (ecc_info_param->ecc_panic_newmaster-1 == cpuid())){

		private.p_flags |= PDAF_MASTER;

		/* need to update masterpda gloabl so that we can reboot
		 * after panic is complete.
		 */

		masterpda = pdaindr[cpuid()].pda;
		cmn_err(CE_CONT|CE_CPUID,
			"ecc_panic: assuming role of master cpu (due to cache error)\n");
	}

#endif /* IP19 */
	{
		extern int ecc_panic_cpu;
		
		/* this flag lets icmn_err know that we're panic-ing so it
		 * avoids performing some operations which may lead to
		 * tlbmisses.
		 * Also, make first message give as much info as possible
		 * in case only first message makes it into console buffer.
		 */
#ifdef IP19
		ecc_panic_cpu = ecc_info_param->ecc_panic_cpuid;
#else
		ecc_panic_cpu = cpuid();
#endif

		if (ECC_INFO(ecc_flags) & HANDLER_OVERRAN)
			cmn_err(CE_CONT|CE_CPUID,
				"ecc_panic initiated, ECC error overrun!\n");
		else
			cmn_err(CE_CONT|CE_CPUID,"ecc_panic initiated! (for cpu %d)\n", ecc_panic_cpu);
	}
#if defined (IP19)
	/* Empirical evidence suggests that this machine_err_dump should come
	 * after the preceding "panic" variables and initial error message.
	 * I saw many cases where the cpu "hung" attempting to pring the
	 * machine error state, quite possible due to holding the putbuflck.
	 * With this placement we always seem to get the panic cleanly.
	 */
	machine_error_dump("");
#endif /* IP19 */

	/* point edb to set of variables to use */
	edp = (err_desc_t *)&(ECC_INFO(desc[index]));

	if (ECC_INFO(ecc_flags & (K_ECC_PANIC | HANDLER_OVERRAN))) {
		PRINT_CPUID(edp->e_cpuid);
		cmn_err(CE_CONT,"ECC PANIC: %s\n",
			((ECC_INFO(ecc_flags) & K_ECC_PANIC) 
#ifdef IP19
			 ? real_ecc_kernel_err
			 : real_ecc_overrun_msg));
#else /* !IP19 */
			 ? ecc_kernel_err
			 : ecc_overrun_msg));
#endif /* !IP19 */
	}

#if IP19
	real_ecc_print_msg(ECC_ALL_MSGS,index,0,1,edp->e_cpuid, ecc_info_param);
#else
	ecc_print_msg(ECC_ALL_MSGS,index,0,1,edp->e_cpuid);
#endif

	if (edp->e_location != CACH_PI  &&  edp->e_location != CACH_PD)
		print_ecctype(edp->e_location, edp->e_tag_or_data, 
			edp->e_syndrome,edp->e_paddr, 1, edp->e_cpuid);

	PRINT_CPUID(edp->e_cpuid);
	cmn_err(CE_CONT,"    CacheErr %R\n", edp->e_cache_err, cache_err_desc);
	PRINT_CPUID(edp->e_cpuid);
	cmn_err(CE_CONT,"    Status %R\n", edp->e_sr,
#if R4000 && R10000
		IS_R10000() ? r10k_sr_desc :
#endif /* R4000 && R10000 */
			sr_desc);

	PRINT_CPUID(edp->e_cpuid);
	cmn_err(CE_CONT,
		"    ErrorEPC 0x%x, Exception Frame 0x%x, ECC Frame 0x%x\n",
#ifdef _MEM_PARITY_WAR
		edp->e_error_epc, (__psunsigned_t)edp->e_eframep,
		(__psunsigned_t)edp->e_eccframep);
#else
		edp->e_error_epc, ECC_INFO(eframep), ECC_INFO(eccframep));
#endif	/* _MEM_PARITY_WAR */

	PRINT_CPUID(edp->e_cpuid);
	cmn_err(CE_CONT,"    PhysAddr 0x%llx, VirtAddr 0x%x\n",
		edp->e_paddr, edp->e_vaddr);

#if _MEM_PARITY_WAR
	if (edp->e_flags & E_PADDR_MC) {
		PRINT_CPUID(edp->e_cpuid);
		log_perr(edp->e_paddr, 
			 edp->e_eccframep[ECCF_CPU_ERR_STAT] & 0xff,
			 0, 1);
	}
	PRINT_CPUID(edp->e_cpuid);
	cmn_err(CE_CONT,"    cpu_err_stat: 0x%x, cpu_err_addr: 0x%x\n",
		edp->e_eccframep[ECCF_CPU_ERR_STAT], 
		edp->e_eccframep[ECCF_CPU_ERR_ADDR]);
	PRINT_CPUID(edp->e_cpuid);
	cmn_err(CE_CONT,"    gio_err_stat: 0x%x, gio_err_addr: 0x%x\n",
		edp->e_eccframep[ECCF_GIO_ERR_STAT], 
		edp->e_eccframep[ECCF_GIO_ERR_ADDR]);
#if IP22
	if (is_fullhouse())
	cmn_err(CE_CONT,"    hpc3_buserr_stat: 0x%x\n", 
		PHYS_TO_K1(HPC3_BUSERR_STAT_ADDR));
#endif /* IP22 */
#endif /* _MEM_PARITY_WAR */
	PRINT_CPUID(edp->e_cpuid);
	cmn_err(CE_CONT,"    ECC cbits 0x%x data_lo 0x%x data_hi 0x%x\n",
		edp->e_badecc,edp->e_lo_badval, edp->e_hi_badval);
	PRINT_CPUID(edp->e_cpuid);
	cmn_err(CE_CONT,"    sbe dblwrds 0x%x mbe dblwrds 0x%x\n",
		edp->e_sbe_dblwrds, edp->e_mbe_dblwrds);
	PRINT_CPUID(edp->e_cpuid);
	cmn_err(CE_CONT,"    s_taglo %R%s\n",
		edp->e_s_taglo,s_taglo_desc,
		(edp->e_s_taglo ? "\n    " : "  "));

	if (edp->e_2nd_syn)
		cmn_err(CE_CONT,"2nd_syn 0x%x\n",edp->e_2nd_syn);
	else
		cmn_err(CE_CONT,"\n");

#if DEBUG_ECC
	if (f_s_caddr) {
		PRINT_CPUID(edp->e_cpuid);
		cmn_err(CE_CONT,
			"    f_ vars:\n    lov 0x%x hiv 0x%x pcad %x scad %x\n",
			f_loval, f_hival, f_p_caddr, f_s_caddr);
		PRINT_CPUID(edp->e_cpuid);
		cmn_err(CE_CONT,"    P-lo %R%sS-lo %R\n",
			f_ptaglo,p_taglo_desc, (f_ptaglo ? "\n    " : "  "),
			f_staglo,
#if R4000 && R10000
			IS_R10000() ? r10k_s_taglo_desc :
#endif /* R4000 && R10000 */
				s_taglo_desc);
		PRINT_CPUID(edp->e_cpuid);
		cmn_err(CE_CONT,"    cooked 0x%x, f_d_ecc 0x%x\n",
			f_cooked_ecc,f_d_ecc);
		PRINT_CPUID(edp->e_cpuid);
		cmn_err(CE_CONT,"    P-lo1 %R%sS-lo1 %R\n",
			f_ptaglo1,p_taglo_desc, (f_ptaglo1 ? "\n    " : "  "),
			f_staglo1,
#if R4000 && R10000
			IS_R10000() ? r10k_s_taglo_desc :
#endif /* R4000 && R10000 */
				s_taglo_desc);
	}
#endif /* DEBUG_ECC */

#ifdef IP19
	if (ecc_info_param->ecc_panic_recoverable == 1)
	        cmn_err_tag(70,CE_PANIC, "Single-bit cache error but recovery disabled\n");
	else if (ecc_info_param->ecc_panic_recoverable == 2)
	        cmn_err_tag(71,CE_PANIC, "Store-miss cache error, possibly recoverable\n");
	else
	        cmn_err_tag(72,CE_PANIC, "Uncorrectable cache ecc/parity error\n");
#else /* !IP19 */
        cmn_err_tag(73,CE_PANIC, "Uncorrectable cache ecc/parity error\n");
#endif /* !IP19 */
	/*NOTREACHED*/

} /* ecc_panic */


volatile int did_it = 0;

#ifdef _MEM_PARITY_WAR
extern	int utlbmiss[], eutlbmiss[];
#ifdef R4600
extern	int utlbmiss_r4600[];
extern	int eutlbmiss_r4600[];
#endif /* R4600 */
#ifdef _R5000_BADVADDR_WAR
extern	int utlbmiss_r5000[];
extern	int eutlbmiss_r5000[];
extern	int utlbmiss2_r5000[];
extern	int eutlbmiss2_r5000[];
extern	int utlbmiss1_r5000[];
extern	int eutlbmiss1_r5000[];
extern	int utlbmiss3_r5000[];
extern	int eutlbmiss3_r5000[];
#endif /* _R5000_BADVADDR_WAR */
#if R4000 && (IP19 || IP22)
extern	int utlbmiss_250mhz[], eutlbmiss_250mhz[];
extern	int utlbmiss2_250mhz[], eutlbmiss3_250mhz[];
#endif /* R4000 && (IP19 || IP22) */
extern	int utlbmiss1[], eutlbmiss1[];
extern	int utlbmiss2[], eutlbmiss2[];
extern	int utlbmiss3[], eutlbmiss3[];
#ifndef _NO_R4000
extern	int	locore_exl_0[], elocore_exl_0[];
extern	int	locore_exl_1[], elocore_exl_1[];
extern	int	locore_exl_2[], elocore_exl_2[];
extern	int	locore_exl_3[], elocore_exl_3[];
extern	int	locore_exl_4[], elocore_exl_4[];
extern	int	locore_exl_5[], elocore_exl_5[];
extern	int	locore_exl_6[], elocore_exl_6[];
extern	int	locore_exl_7[], elocore_exl_7[];
extern	int	locore_exl_8[], elocore_exl_8[];
extern	int	locore_exl_9[], elocore_exl_9[];
extern	int	locore_exl_10[], elocore_exl_10[];
extern	int	locore_exl_11[], elocore_exl_11[];
extern	int	locore_exl_12[], elocore_exl_12[];
extern	int	locore_exl_13[], elocore_exl_13[];
#ifdef _R5000_CVT_WAR
extern	int	locore_exl_14[], elocore_exl_14[];
extern	int	locore_exl_15[], elocore_exl_15[];
#endif /* _R5000_CVT_WAR */
extern	int	locore_exl_16[], elocore_exl_16[];
#ifdef USE_PTHREAD_RSA
extern	int	locore_exl_17[], elocore_exl_17[];
#endif /* USE_PTHREAD_RSA */
extern	int	locore_exl_18[], elocore_exl_18[];
extern	int	locore_exl_19[], elocore_exl_19[];
extern	int	locore_exl_20[], elocore_exl_20[];
extern	int	locore_exl_21[], elocore_exl_21[];
extern	int	locore_exl_22[], elocore_exl_22[];
extern	int	locore_exl_23[], elocore_exl_23[];
extern	int	locore_exl_24[], elocore_exl_24[];
extern	int	locore_exl_25[], elocore_exl_25[];

struct exl_handler_table_s {
	int	*base;
	int	*limit;
} exl_handler_table[] = {
	{ (int *) K0BASE, (int *) K0BASE + NBPP }, /* exception handlers */
	{ (int *) K1BASE, (int *) K1BASE + NBPP },
	{ utlbmiss, eutlbmiss },
#ifdef R4600
	{ utlbmiss_r4600, eutlbmiss_r4600 },
#endif /* R4600 */
#ifdef _R5000_BADVADDR_WAR
	{ utlbmiss_r5000, eutlbmiss_r5000 },
	{ utlbmiss2_r5000, eutlbmiss2_r5000 },
	{ utlbmiss1_r5000, eutlbmiss1_r5000 },
	{ utlbmiss3_r5000, eutlbmiss3_r5000 },
#endif /* _R5000_BADVADDR_WAR */
	{ utlbmiss2, eutlbmiss3 }, /* includes utlbmiss1 and sharedseg */
#if R4000 && (IP19 || IP22)
	{ utlbmiss_250mhz, eutlbmiss_250mhz },
	{ utlbmiss2_250mhz, eutlbmiss3_250mhz },
#endif
	{ locore_exl_0, elocore_exl_0 },
	{ locore_exl_1, elocore_exl_1 },
	{ locore_exl_2, elocore_exl_2 },
	{ locore_exl_3, elocore_exl_3 },
	{ locore_exl_4, elocore_exl_4 },
	{ locore_exl_5, elocore_exl_5 },
	{ locore_exl_6, elocore_exl_6 },
	{ locore_exl_7, elocore_exl_7 },
	{ locore_exl_8, elocore_exl_8 },
	{ locore_exl_9, elocore_exl_9 },
	{ locore_exl_10, elocore_exl_10 },
	{ locore_exl_11, elocore_exl_11 },
	{ locore_exl_12, elocore_exl_12 },
	{ locore_exl_13, elocore_exl_13 },
#ifdef _R5000_CVT_WAR
	{ locore_exl_14, elocore_exl_14 },
	{ locore_exl_15, elocore_exl_15 },
#endif /* _R5000_CVT_WAR */
	{ locore_exl_16, elocore_exl_16 },
#ifdef USE_PTHREAD_RSA
	{ locore_exl_17, elocore_exl_17 },
#endif /* USE_PTHREAD_RSA */
	{ locore_exl_18, elocore_exl_18 },
	{ locore_exl_19, elocore_exl_19 },
	{ locore_exl_20, elocore_exl_20 },
	{ locore_exl_21, elocore_exl_21 },
	{ locore_exl_22, elocore_exl_22 },
	{ locore_exl_23, elocore_exl_23 },
	{ locore_exl_24, elocore_exl_24 },
	{ locore_exl_25, elocore_exl_25 },
	{ NULL, NULL }
};
#define exl_handler_table_uc ((struct exl_handler_table_s *) K0_TO_K1((ulong) exl_handler_table))
#endif /* _NO_R4000 */

extern int perr_save_info(eframe_t *, k_machreg_t *, uint, k_machreg_t, int);
#ifdef R4600SC
extern int _r4600sc_enable_scache_erl(void);
extern int _r4600sc_disable_scache_erl(void);
#endif /* R4600SC */
extern int ecc_same_cache_block(int, paddr_t, paddr_t);
extern int tlb_to_phys(k_machreg_t , paddr_t *, int *);
extern unsigned int r_phys_word(paddr_t);
extern int _read_tag(int, caddr_t, int *);
extern ecc_fixup_caches(int, paddr_t, k_machreg_t, uchar_t);
extern int decode_inst(eframe_t *, int, int *, k_machreg_t *, int *);
#endif /* MEM_PARITY_WAR */

#if defined(_MEM_PARITY_WAR) || defined(IP32)
static int
ecc_is_branch(inst_t inst)
{
	union mips_instruction i;

	unsigned int op;
	i.word = inst;
	op = i.j_format.opcode;
	if (op == spec_op) {
	  if (i.r_format.func == jr_op || i.r_format.func == jalr_op)
	    return(1);
	  return(0);
	} else if (op == bcond_op) {
	  op = i.i_format.rt;
	  if ((/* op >= bltz_op && */ op <= bgezl_op) ||
 	      (op >= bltzal_op && op <= bgezall_op))
	    return(1);
	  return(0);
	} else if (op >= cop0_op && op <= cop3_op) {
	  if (i.r_format.rs == bc_op)
	    return(1);
	  return(0);
	} else if (((op >= j_op) && (op <= bgtz_op)) || 
	    ((op >= beql_op && op <= bgtzl_op)))
	  return(1);
	return(0);
}

#define LOAD_INSTR  1
#define STORE_INSTR 2

static paddr_t
ecc_get_perr_addr(eframe_t *ep, k_machreg_t errorepc, int *cache_err)
{
	paddr_t	instaddr, bdaddr, paddr = 0;
	k_machreg_t vaddr;
	int cached, width, ldst;
#ifdef WRONG
	int pidx, sidx;
#endif
	inst_t inst, bdinst;
	int is_bdslot = 0;

	if (!tlb_to_phys(errorepc, &instaddr, &cached))
	    /* can't translate this address, fail */
		return(-1);

#ifdef _MEM_PARITY_WAR
	inst = (inst_t)r_phys_word(instaddr);
#else
	inst = (inst_t)r_phys_word_erl(instaddr);
#endif

	if (ecc_is_branch(inst)) {
		if (!tlb_to_phys(errorepc+sizeof(inst_t), &bdaddr, &cached))
			return(-1);
#ifdef _MEM_PARITY_WAR
		bdinst = (inst_t)r_phys_word(bdaddr);
#else
		bdinst = (inst_t)r_phys_word_erl(bdaddr);
#endif
		is_bdslot = 1;
	}
	
	if (!(*cache_err & CACHERR_ER)) {
		/* 
		 * we got an error on an instruction access 
		 * so errorepc points to the offending instruction
		 * or a branch instruction which may be the offending
		 * instruction or the offender may be the instruction 
		 * in the branch delay slot.  
		 *
		 * If only one instruction is involved, or if both
		 * instructions are in the same cache line, we can
		 * synthesize a physaddr and build a corresponding
		 * ce_sidx and ce_pidx and add them to the contents
		 * of the cache_err register from this exception.
		 */
		if (is_bdslot) {
			if (!ecc_same_cache_block(CACH_PI, instaddr, bdaddr))
				return(-1);
		}

#ifdef WRONG
		/* 
		 * build a reasonable facsimile of ce_pidx and ce_pidx 
		 * and place them in the cache_err register image
		 */
		pidx = (errorepc >> CACHERR_PIDX_SHIFT) & CACHERR_PIDX_MASK;
		sidx = instaddr & CACHERR_SIDX_MASK;
		*cache_err &= ~(CACHERR_PIDX_MASK | CACHERR_SIDX_MASK);
		*cache_err |= (sidx | pidx);
#endif
		return((__uint64_t)instaddr);
	} else {
		/* 
		 * we got a data error, look at the instruction 
		 * at errorepc -- if it is a load/store calculate
		 * the physical address of the target.  If it is
		 * a branch, the instruction in the branch delay
		 * slot should be the offending instruction.  
		 * calculate the physical address of the target 
		 * of this instruction and use it as the physical
		 * address.
		 *
		 * if neither of these situations holds true, we've
		 * got a real problem.
		 */
		if (bdaddr)
			inst = bdinst;

		/*
		 * see if we can decode the instruction which 
		 * caused the fault, if not, we can't get a physaddr.
		 */
		if (!decode_inst(ep, inst, &ldst, &vaddr, &width))
			return(-1);
		if (!tlb_to_phys(vaddr, &paddr, &cached))
			return(-1);
#ifdef WRONG
		/* 
		 * build a reasonable facsimile of ce_pidx and ce_pidx 
		 * and place them in the cache_err register image
		 */
		pidx = (vaddr >> CACHERR_PIDX_SHIFT) & CACHERR_PIDX_MASK;
		sidx = paddr & CACHERR_SIDX_MASK;
		*cache_err &= ~(CACHERR_PIDX_MASK | CACHERR_SIDX_MASK);
		*cache_err |= (sidx | pidx);
#endif
		return((__uint64_t)paddr);
	}
	/*NOTREACHED*/
}
#endif /* _MEM_PARITY_WAR || IP32 */
ecc_handler(
	eframe_t *efp,
	k_machreg_t *eccfp,
	uint cache_err,
#if IP19
	k_machreg_t errorepc,
	volatile ecc_info_t *ecc_info_param,
	cpuid_t ecc_cpuid)
#else
	k_machreg_t errorepc)
#endif
{
	int location;
	err_desc_t *edp;	/* ptr to set of variables to set this time */
	uint ce_sidx = (cache_err & CACHERR_SIDX_MASK);
	uint ce_pidx = (cache_err & CACHERR_PIDX_MASK);	/* must be shifted */
	__uint64_t physaddr;
#if IP32
	_crmreg_t regval;
#endif
	register int t_or_d = 0;
	uint tags[NUM_TAGS], s_data_ecc;
	int res = 0;
	uint index = 0;
#if _MEM_PARITY_WAR
	static time_t last_time;
#endif	/* _MEM_PARITY_WAR */
#ifdef R4600SC
	extern int two_set_pcaches;
	int r4600sc_scache_disabled = 1;
	int _r4600sc_disable_scache(void);
	void _r4600sc_enable_scache(void);

	if (two_set_pcaches && private.p_scachesize)
#ifdef _MEM_PARITY_WAR	
		r4600sc_scache_disabled = _r4600sc_disable_scache_erl();
#else /* _MEM_PARITY_WAR */
		r4600sc_scache_disabled = _r4600sc_disable_scache();
#endif /* _MEM_PARITY_WAR */
#endif

#if defined (EVEREST)
	/* Now save the cache error in the extended everror structure 
	 * for future use by the FRU analyzer
	 */

	if (ecc_info_param->ecc_info_inited != 1)
		return(1);

	ecc_info_param->everror_ext->eex_cpu[ecc_cpuid].cpu_cache_err =
		cache_err;
	ecc_info_param->ecc_panic_recoverable = 0;
#endif /* EVEREST */

#ifdef _MEM_PARITY_WAR
#ifndef CMPLR_BUG_277906_FIXED
	errorepc = eccfp[ECCF_ERROREPC];
#endif

#if R4000 && (! _NO_R4000)
	if (efp->ef_sr & SR_EXL) {
		/* check if we need to clear SR_EXL due to an R4000 bug:
		 * we clear SR_EXL if $errorepc was not in one of the
		 * SR_EXL handlers.
		 */
		k_machreg_t errorepc_k0;

		if (IS_KSEG1(errorepc))
     			errorepc_k0 = K1_TO_K0(errorepc);
		else
			errorepc_k0 = errorepc;
     
		for (index = 0; exl_handler_table_uc[index].base != 0; index++) {
			if (((int *) errorepc_k0) >= exl_handler_table_uc[index].base &&
			    ((int *) errorepc_k0) < exl_handler_table_uc[index].limit)
				break;
		}
		if (exl_handler_table_uc[index].base == NULL)
			/* errorepc not found in table */
			efp->ef_sr &= ~SR_EXL;
	}
#endif /* R4000 && (! _NO_R4000) */
	
#if (defined(IP20) || defined(IP22) || defined(IPMHSIM))
	/* save bus error status */
	eccfp[ECCF_CPU_ERR_STAT] = *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT);
	eccfp[ECCF_CPU_ERR_ADDR] = *(volatile uint *)PHYS_TO_K1(CPU_ERR_ADDR);
	eccfp[ECCF_GIO_ERR_STAT] = *(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT);
	eccfp[ECCF_GIO_ERR_ADDR] = *(volatile uint *)PHYS_TO_K1(GIO_ERR_ADDR);

	/* clear possible errors */
	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0x0;
	*(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT) = 0x0;
	flushbus();

	/* retry if CPU see SYSAD error but MC did not see any */
	if ((cache_err & CACHERR_EE) &&
	    !eccfp[ECCF_CPU_ERR_STAT] && !eccfp[ECCF_GIO_ERR_STAT] &&
	    (time - last_time > 5)) {
		ecc_info.ecc_err_cnts[SYSAD_ERRS]++;
		last_time = time;
		return 0;
	}

	/* save_perr_info checks to see if it is a memory error
	 * we might be able to workaround, and saves away enough
	 * information to be able to fix it.  
	 */
	if (perr_save_info(efp, eccfp, cache_err, errorepc,
			   ((cache_err & CACHERR_EE)
			    ? PERC_CACHE_SYSAD
			    : PERC_CACHE_LOCAL))) {
#ifdef R4600SC
		if (!r4600sc_scache_disabled)
			_r4600sc_enable_scache_erl();
#endif /* R4600SC */
		return(-1); /* force an exception */
	}
#endif	/* IP20 || IP22 */
#endif /* _MEM_PARITY_WAR */

#ifdef IP19
	/* On IP19 all memory (BSS) is zeroed at boot time, so we don't
	 * really have much to initialize.  We really want to avoid
	 * referencing global variables which are cached.
	 */
	ecc_info_param->eframep = CACHE_ERR_EFRAME;
	ecc_info_param->eccframep = CACHE_ERR_ECCFRAME;
#else /* !IP19 */
	if (!ecc_info_initialized)
		init_ecc_info();
#endif /* !Ip19 */

	/* if this error was 'forced' the CE bit will be set--clear it
	 * in the eframe SR */
	efp->ef_sr &= ~SR_CE;	

	/* Check if we have handled too many ecc errors without 
	 * allowing the cleanup routine to execute.  (splhi and
	 * DE bit set ensures we won't be interrupted during this
	 * test) */
	NEXT_INDEX(ECC_INFO(ecc_w_index));
#ifdef IP19
	/* On IP19 we have a state indicator built-in to the entry.
	 * Make sure entry is free before using it.
	 */

	if (ecc_info_param->ecc_entry_state[ecc_info_param->ecc_w_index] != 0) {
		ecc_info_param->ecc_flags |= HANDLER_OVERRAN;
		/* back up write index so ecc_panic() will do something */
		PREV_INDEX(ecc_info_param->ecc_w_index);
		return(1);	/* ecc_panic will print proper msg */
	} else {
		index = ecc_info_param->ecc_w_index;
		ecc_info_param->ecc_entry_state[index] = 1;
	}

#else /* !IP19 */
	if (ecc_info.ecc_w_index == ecc_info.ecc_r_index) {
		ecc_info.ecc_flags |= HANDLER_OVERRAN;
		/* back up write index so ecc_panic() will do something */
		PREV_INDEX(ecc_info.ecc_w_index);
#ifdef R4600SC
		/*
		 * on R4600SC there is no need to renable cache
		 * since we are going to panic anyway.
		 */
#endif /* R4600SC */
		return(1);	/* ecc_panic will print proper msg */
	} else {
		index = ecc_info.ecc_w_index;
	}
#endif /* !IP19 */

	/* point edb to set of variables to use */
	edp = (err_desc_t *)&(ECC_INFO(desc[index]));

#ifdef IP19
	edp->e_cpuid = ecc_cpuid;
#else
	edp->e_cpuid = cpuid();
#endif
#ifdef _MEM_PARITY_WAR
	edp->e_eframep = efp;
	edp->e_eccframep = eccfp;
#endif	/* _MEM_PARITY_WAR */

	edp->e_flags = E_PADDR_VALID|E_VADDR_VALID;
#ifdef R4000PC
	if ((r4000_config & CONFIG_SC) == 0) { /* 0 == scache present */
#endif /* R4000PC */
	/* use CacheErr sidx to fetch 2ndary tag of the line mapping it,
	 * and ecc checkbits of the data in that line */
	_c_ilt_n_ecc(CACH_SD, PHYS_TO_K0(ce_sidx), tags, &s_data_ecc);
	edp->e_badecc = eccfp[ECCF_ECC] = s_data_ecc;

	edp->e_s_taglo = tags[TAGLO_IDX];
	
	/* ce_sidx has paddr[21..3], 2ndary taglo has paddr[35..17] but 
	 * must be shifted to proper position */
	physaddr = (ce_sidx | ((edp->e_s_taglo & SADDRMASK) << SADDR_SHIFT));

#ifdef R4000PC 
	} else {
#ifdef _MEM_PARITY_WAR
		if (((eccfp[ECCF_CPU_ERR_STAT] & CPU_ERR_STAT_RD_PAR) ==
		     CPU_ERR_STAT_RD_PAR)) {
			physaddr = eccfp[ECCF_CPU_ERR_ADDR] & ~0x7;
			physaddr += BYTEOFF(eccfp[ECCF_CPU_ERR_STAT] & 0xff);
			edp->e_flags |= E_PADDR_MC;
		} else if (eccfp[ECCF_GIO_ERR_STAT] & GIO_ERRMASK) {
			physaddr = eccfp[ECCF_GIO_ERR_ADDR] & ~0x7;
			physaddr |= BYTEOFF(eccfp[ECCF_GIO_ERR_STAT] & 0xff);
			edp->e_flags |= E_PADDR_GIO;
		}
		else {
			physaddr = ecc_get_perr_addr(efp,errorepc,(int*)&cache_err);
			/* no physaddr, can't go on */
			if (physaddr == -1) 
				return(1);
			if (cache_err & CACHERR_ER)
				/* 
				 * if fault occured on a data reference
				 * and was *not* reported by MC, we can't
				 * be certain that the physaddr we got
				 * from decoding the instruction is correct.
				 */
				edp->e_flags &= ~E_PADDR_VALID;
		}
		/*
		 * this allows us to work around the rather persistent
		 * bug in the R4000 which causes it to report an incorrect
		 * pidx when it takes a primary cache parity error.  This
		 * workaround is not necessary on the R4600 so we skip it
		 * if we are running on one.  If we can't find it in the
		 * cache (ce_pidx == -1) we continue to collect information
		 * for error reporting, but we will not attempt to fix the
		 * error, we will either kill the process or we will panic.
		 */
		if (!(cache_err & CACHERR_ET) && !two_set_pcaches) {
			ce_pidx = ecc_find_pidx((cache_err & CACHERR_ER) ? 
						CACH_PD :
						CACH_PI, physaddr);
			if (ce_pidx == -1) {
				edp->e_flags &= ~E_VADDR_VALID; 
			} else {
				cache_err = (cache_err & ~CACHERR_PIDX_MASK) 
					    | ce_pidx;
			}
		}
#elif IP32
#define CPU_ERR (CRM_CPU_ERROR_CPU_ILL_ADDR | \
		 CRM_CPU_ERROR_CPU_WRT_PRTY)
#define CPU_ERR_REV0 (CRM_CPU_ERROR_CPU_INV_ADDR_RD | \
		CRM_CPU_ERROR_CPU_INV_REG_ADDR)
#define MEM_ERR (CRM_MEM_ERROR_CPU_ACCESS | \
		 CRM_MEM_ERROR_HARD_ERR)

		regval = READ_REG64(PHYS_TO_K1(CRM_CPU_ERROR_STAT), _crmreg_t);
		eccfp[ECCF_CPU_ERR_STAT] = (uint)(regval & 0xffffffff);

		regval = READ_REG64(PHYS_TO_K1(CRM_MEM_ERROR_STAT), _crmreg_t);
		eccfp[ECCF_MEM_ERR_STAT] = (uint)(regval & 0xffffffff);

		regval = READ_REG64(PHYS_TO_K1(CRM_CPU_ERROR_ADDR), _crmreg_t);
		eccfp[ECCF_CPU_ERR_ADDR] = regval;

		regval = READ_REG64(PHYS_TO_K1(CRM_MEM_ERROR_ADDR), _crmreg_t);
		eccfp[ECCF_MEM_ERR_ADDR] = (uint)(regval & 0xffffffff);

		if ((eccfp[ECCF_MEM_ERR_STAT] & MEM_ERR) == MEM_ERR) {
			WRITE_REG64(0LL, 
				    PHYS_TO_K1(CRM_MEM_ERROR_STAT), _crmreg_t);
			physaddr = (__uint64_t)eccfp[ECCF_MEM_ERR_ADDR];
			/*
			 * we access all memory below 256Mb at the 0 based
			 * alias.  Memory at or above 256Mb is accessed above
			 * 1Gb.  Correct for this since CRIME only reports
			 * memory error address bits 29:0.
			 */
			if (physaddr >= 0x10000000)
				physaddr += 0x40000000;
		} else if ((eccfp[ECCF_CPU_ERR_STAT] & CPU_ERR_REV0) == CPU_ERR_REV0) {
			WRITE_REG64(0LL, 
				    PHYS_TO_K1(CRM_CPU_ERROR_STAT), _crmreg_t);
			physaddr = eccfp[ECCF_CPU_ERR_ADDRHI];
			physaddr = (physaddr << 32) | eccfp[ECCF_CPU_ERR_ADDR];
		} else if ((eccfp[ECCF_CPU_ERR_STAT] & CPU_ERR) == CPU_ERR) {
			WRITE_REG64(0LL, 
				    PHYS_TO_K1(CRM_CPU_ERROR_STAT), _crmreg_t);
			physaddr = eccfp[ECCF_CPU_ERR_ADDRHI];
			physaddr = (physaddr << 32) | eccfp[ECCF_CPU_ERR_ADDR];
		}
		else {
			physaddr = (long long)ecc_get_perr_addr(efp,errorepc,
							(int *)&cache_err);
			/* no physaddr, can't go on */
			if (physaddr == -1) 
				return(1);
			if (cache_err & CACHERR_ER)
				/* 
				 * if fault occured on a data reference
				 * and was *not* reported by CRIME, we can't
				 * be certain that the physaddr we got
				 * from decoding the instruction is correct.
				 */
				edp->e_flags &= ~E_PADDR_VALID;
		}
		edp->e_paddr = physaddr;
#else
		physaddr = 0;
#endif /* _MEM_PARITY_WAR */
	}
#endif /* R4000PC */
	edp->e_paddr = physaddr;

	/* set the eccframe paddr to physaddr for now; later routines will
	 * change it if needed (e.g. a p-cache error needs e_vaddr */
#if IP19  || IP32
	eccfp[ECCF_PADDR] = physaddr & 0x0ffffffff;
	eccfp[ECCF_PADDRHI] = physaddr>>32;
#else
	eccfp[ECCF_PADDR] = physaddr;
#endif

	/* primary caches are virtually tagged; build & save vaddr */
	edp->e_vaddr = (ce_pidx << CACHERR_PIDX_SHIFT) | (ce_sidx & (NBPP-1));

	edp->e_cache_err = cache_err,
	edp->e_error_epc = errorepc,
	edp->e_sr = efp->ef_sr;

	/* There is an R4k chip bug which mistakenly turns on CACHERR_EB
	 * under convoluted circumstances.  The workaround is to believe
	 * CACHERR_EB only if CACHERR_ER indicates an instruction error.
	 * So have edp->e_cache_err contain the original cache err register
	 * contents, but fix up our local cache_err value.
	 */
	if ((cache_err & (CACHERR_EB|CACHERR_ER)) == (CACHERR_EB|CACHERR_ER))
		cache_err &= ~CACHERR_EB;	/* not an instruction err */

	ASSERT(cache_err & (CACHERR_ED|CACHERR_ET));

	if (cache_err & CACHERR_ED)	/* Error in data */
		t_or_d = DATA_ERR;
	if (cache_err & CACHERR_ET) {	/* Error in tag or both */
		if (t_or_d == DATA_ERR)
			t_or_d = D_AND_T_ERR;
		else
			t_or_d = TAG_ERR;
	}

	if (cache_err & CACHERR_EE) {	/* wrong from SysAD bus */
		location = SYSAD;
		ecc_log_error(SYSAD_ERRS, index);
	} else if ( (cache_err & CACHERR_EC) && (cache_err & CACHERR_ER) )
		location = CACH_SD;
	else if ( !(cache_err & CACHERR_EC) && (cache_err & CACHERR_ER) )
		location = CACH_PD;
	else if ( (cache_err & CACHERR_EC) && !(cache_err & CACHERR_ER) )
		location = CACH_SI;
	else if ( !(cache_err & CACHERR_EC) && !(cache_err & CACHERR_ER) )
		location = CACH_PI;
	else {
		ecc_assign_msg(ECC_PANIC_MSG, index, ecc_incons_err);
#if IP19
		ecc_info_param->ecc_entry_state[index] = 2;
#endif
		return(1);
	}
#ifdef IP19
	/* check for occurance of cache error exception while already
	 * in cache error handler (or double error due to error on both
	 * out-going and in-coming cacheline).
	 * NOTE: EW bit only defined for R4400 processors.
	 */
	if ( (cache_err & CACHERR_EW) ) {
		ecc_assign_msg(ECC_PANIC_MSG, index, ecc_ew_err);
		goto uncorrectable;
	}

#ifdef ECC_TEST_EW_BIT
	{
	extern int get_cacheerr(void);

	ecc_info_param->ecc_wait_for_external = 1;
	while (ecc_info_param->ecc_wait_for_external != 2)
		/* NOP */ ;
	ecc_info_param->ecc_cpu1_cacheerr2 = get_cacheerr();
	}
#endif
	edp->e_location = location;
	edp->e_tag_or_data = t_or_d;

	if (location == CACH_PD || location == CACH_PI)
		eccfp[ECCF_PADDR] = edp->e_vaddr;

	if (location == SYSAD) {
		res = ecc_fixmem(index,efp,eccfp,cache_err,errorepc);
	} else {	/* it's error(s) in cache(s) */
		res = ecc_fixcache(index,efp,eccfp,cache_err,errorepc);
	}

	/* if cache_err EB bit is set, a data error occurred in addition to
	 * i-cache error indicated by the other cache_err bits.  Flush both
	 * data caches after sanity-checking that main error was in icache.
	 * Note: if a) error is in clean data, the line won't be written-
	 * back, so the error will be fixed.  b) if error is in dirty line,
	 * line will flush out through RMI, which will fix 1-bit errors,
	 * pass over > 1-bit errors.  Therefore, two cases: 1) 1-bit data
	 * error: transparently fixed (not logged, unfortunately); 
	 * 2) multibit data error: it will be stored in memory with the
	 * errors; if it is written to disk it is dealt-with then; if it
	 * is re-read the R4K will raise an exception and we'll take action
	 * then, so this is sufficient. */
	if (cache_err & CACHERR_EB) {
		/* 
		 * XXX: we *may* be able to recover from the data error
		 * with great difficulty, for now we will just die.
		 */
		res = 1;
		/* We must avoid cached accesses since that might force
		 * corrupted data out from primary cache (if error is due
		 * to a store-miss).  So just flush an area large enough
		 * to guarentee we've flushed the entire cache, rather
		 * than loading p_scachesize, which is a cached variable.
		 */
		__cache_wb_inval((void *)FLUSH_ADDR, FOUR_MEG);
	}
	if (res) {	/* failed to correct error: kill process or IRIX */
		/* For now we will panic on IP19 machines.  The error
		 * may have occurred in user mode, but perhaps the data
		 * destroyed (forced out corrupted ?) may be kernel data.
		 */
		if (USERMODE(efp->ef_sr)) {
			ecc_assign_msg(ECC_INFO_MSG, index, ecc_user_err);
			edp->e_user = 1;
#if 0
			/* Following code is bogus since it will make a
			 * cached reference.  Even if error processing
			 * is complete, we're still running with ERL and
			 * DE set, so a cache error here would be ignored.
			 */
			if (private.p_curproc)
				edp->e_curprocp = private.p_curproc;
			else
				edp->e_curprocp = NULL;
			goto handler_exit;
#endif
			goto uncorrectable;
		} else {	/* BOOM!  kernel encountered the ecc error */
			ecc_info_param->ecc_flags |= K_ECC_PANIC;
			goto uncorrectable;
		}
	}

	/* if any cleanup work is necessary, the requesting routine
	 * did a 'MARK_FOR_CLEANUP'.  If so, raise an interrupt.  Else
	 * decrement the index for re-use. */
	if (CLEANUP_IS_NEEDED) {
		ECC_INTERRUPT;
	} else
#ifdef ECC_DEBUG
		/* keep frame for reference let ecc_cleanup sync ptrs */
		ECC_INTERRUPT;
#else
		PREV_INDEX(ecc_info_param->ecc_w_index);	/* overwrite frame */
#endif

	/* give an indication as to whether the error is theorectically
	 * recoverable (or more correctly, try to report uncorrectable only
	 * if it's an MBE and we should replace the CPU).
	 */
	if ((!res) && (!ecc_info_param->ecc_panic_recoverable))
		ecc_info_param->ecc_panic_recoverable = 1;

	ecc_info_param->ecc_entry_state[index] = 2;

#ifdef IP19_CACHEERRS_FATAL
	return(1);
#else
	if (!ecc_info_param->ecc_attempt_recovery)
		return(1);
	/* Error may have been due to store-miss which did not set the EI
	 * bit.  Indications are that the following test should fail
	 * and return "one" in that case, which should be considered
	 * fatal.
	 */

	if ((!res) && (ecc_check_cache(ecc_info_param->ecc_dummyline))) {

		ecc_assign_msg(ECC_PANIC_MSG, index, ecc_possible_ei);
		res = 1;
	}

	return(res);
#endif

uncorrectable:
	ecc_info_param->ecc_entry_state[index] = 2;
	return(1);

#else /* !IP19 */

	edp->e_location = location;
	edp->e_tag_or_data = t_or_d;
	if (location == CACH_PD || location == CACH_PI)
		eccfp[ECCF_PADDR] = edp->e_vaddr;

#if IP20 || IP22 || IPMHSIM
	/* The SP IP20/22  should never encounter external requests, 
	 * so there'd better not be any cache errors as a result of them: 
	 * panic.
	 *
	 * XXX: on the R4600(!two_set_pcaches) ES does not mean an error
	 * caused by an external request, it means that the error occured
	 * on a cache miss in the first doubleword of read response data.
	 */
#if R4600
	if ((cache_err & CACHERR_ES) && !two_set_pcaches) {
#else
	if ((cache_err & CACHERR_ES)) {
#endif
		ecc_assign_msg(ECC_PANIC_MSG, index, ecc_extreq);
		return(1);
	}
#endif

	if (location == SYSAD) {
#ifdef MCCHIP
		res = 1;
#else
		res = ecc_fixmem(index,efp,eccfp,cache_err,errorepc);
#endif	/* MCCHIP */
	} else {	/* it's error(s) in cache(s) */
#if _MEM_PARITY_WAR || IP32
		if ((edp->e_flags & (E_PADDR_VALID|E_VADDR_VALID)) != 
		    (E_PADDR_VALID|E_VADDR_VALID)) {
			/* 
			 * we must be sure of both the physaddr and the cache
			 * index to attempt a fix. 
			 */
			res = 1;
		} else
#endif
		res = ecc_fixcache(index,efp,eccfp,cache_err,errorepc);
	}

	/* if cache_err EB bit is set, a data error occurred in addition to
	 * i-cache error indicated by the other cache_err bits.  Flush both
	 * data caches after sanity-checking that main error was in icache.
	 * Note: if a) error is in clean data, the line won't be written-
	 * back, so the error will be fixed.  b) if error is in dirty line,
	 * line will flush out through RMI, which will fix 1-bit errors,
	 * pass over > 1-bit errors.  Therefore, two cases: 1) 1-bit data
	 * error: transparently fixed (not logged, unfortunately); 
	 * 2) multibit data error: it will be stored in memory with the
	 * errors; if it is written to disk it is dealt-with then; if it
	 * is re-read the R4K will raise an exception and we'll take action
	 * then, so this is sufficient. */
	if (cache_err & CACHERR_EB) {
#if MCCHIP || IP32
	    /* 
	     * XXX: we *may* be able to recover from the data error
	     * with great difficulty, for now we will just die.
	     */
		res = 1;
#else
		if (edp->e_location != CACH_PI && edp->e_location != CACH_SI) {
			ecc_assign_msg(ECC_ERROR_MSG, index, ecc_eb_not_i);
		}
		__cache_wb_inval((void *)FLUSH_ADDR, private.p_scachesize);
#endif
	}
	if (res) {	/* failed to correct error: kill process or IRIX */
		if (USERMODE(efp->ef_sr)) {
			ecc_assign_msg(ECC_INFO_MSG, index, ecc_user_err);
			edp->e_user = 1;
#ifdef _MEM_PARITY_WAR
allow_nofault_error:
#endif /* _MEM_PARITY_WAR */
			edp->e_pid = current_pid();
			goto handler_exit;
		} else {	/* BOOM!  kernel encountered the ecc error */
#ifdef _MEM_PARITY_WAR
			if (private.p_nofault || (curthreadp->k_nofault))
				goto allow_nofault_error;
#endif /* _MEM_PARITY_WAR */
			ecc_info.ecc_flags |= K_ECC_PANIC;
			return(1);	/* we're dead meat--panic now */
		}
	}

handler_exit:
#ifdef R4600SC
	if (!r4600sc_scache_disabled)
#ifdef _MEM_PARITY_WAR	
		_r4600sc_enable_scache_erl();
#else /* _MEM_PARITY_WAR */
		_r4600sc_enable_scache();
#endif /* _MEM_PARITY_WAR */
#endif /* R4600SC */
#ifdef _MEM_PARITY_WAR
	if (res) {
		return(-1);
	}
#endif /* _MEM_PARITY_WAR */

	/* if any cleanup work is necessary, the requesting routine
	 * did a 'MARK_FOR_CLEANUP'.  If so, raise an interrupt.  Else
	 * decrement the index for re-use. */
	if (CLEANUP_IS_NEEDED) {
		ECC_INTERRUPT;
	} else
#ifdef ECC_DEBUG
		/* keep frame for reference let ecc_cleanup sync ptrs */
		ECC_INTERRUPT;
#else
		PREV_INDEX(ecc_info.ecc_w_index);	/* overwrite frame */
#endif

#ifdef _MEM_PARITY_WAR
		ASSERT(!res);
#endif
	return(res);
#endif /* !IP19 */

} /* ecc_handler */


#ifndef MCCHIP
/*ARGSUSED*/
static
#if IP19
real_ecc_fixmem(
    uint index,
    eframe_t *efp,
    k_machreg_t *eccfp,
    uint cache_err,
    k_machreg_t errorepc,
    volatile ecc_info_t *ecc_info_param )
#else    
ecc_fixmem(
    uint index,
    eframe_t *efp,
    k_machreg_t *eccfp,
    uint cache_err,
    k_machreg_t errorepc)
#endif
{
	err_desc_t *edp = (err_desc_t *)&(ECC_INFO(desc[index]));
	uint tags[NUM_TAGS];
	__psunsigned_t physaddr = edp->e_paddr;
	__psunsigned_t k0addr, k0oneoff;
	error_info_t err_info;
	unsigned char hi_syn;
#if IP19
	__psunsigned_t pmem = (ecc_info_param->ecc_physmem * NBPP);
#else
	__psunsigned_t pmem = (physmem * NBPP);
#endif
	eccdesc_t syn_info;
	uint hi_taglo;
#ifdef SYNDROME_CHECKING
	__psunsigned_t addr;
	int foundone = 0;
#endif

	/* since it came in wrong off the bus, the s_taglo register is the
	 * one we're interested in; shove it on the eccframe. */
#ifdef R4000PC
	if ((r4000_config & CONFIG_SC) != 0) /* 0 == scache present */
		eccfp[ECCF_TAGLO] = edp->e_p_taglo;
	else
#endif /* R4000PC */
	eccfp[ECCF_TAGLO] = edp->e_s_taglo;

#ifdef _MEM_PARITY_WAR
	k0addr = physaddr;
	/* This works up to 2 GB of memory, because KUSEG is physical 
	 * memory when SR_ERL is set in $sr.
	 */
#else /* _MEM_PARITY_WAR */
#if IP19
	k0addr = PHYS_TO_K0(physaddr & (ecc_info_param->ecc_k0size_less1));

	/* XXX This won't work if physaddr >= K0SIZE */
	/* flush the bad line out through the RMI (which will fix it
	 * in memory if possible) by reading an address one 2nd-cache-
	 * size higher or lower, whichever is within physical mem.
	 *
	 * NOTE: we need to avoid cached accesses on IP19 so loading
	 * anything from pda (like p_scachesize) is a no-no.
	 * Exact number is not important as long as it is at least as
	 * large as the scachesize.  So we just use 4MB.
	 */
	if ((physaddr + FOUR_MEG) >= pmem)
		k0oneoff = (k0addr - FOUR_MEG);
	else
 		k0oneoff = (k0addr + FOUR_MEG);
#else
	k0addr = PHYS_TO_K0(physaddr & (K0SIZE-1));
	/* XXX This won't work if physaddr >= K0SIZE */
	/* flush the bad line out through the RMI (which will fix it
	 * in memory if possible) by reading an address one 2nd-cache-
	 * size higher or lower, whichever is within physical mem.
	 */
	if ((physaddr + private.p_scachesize) >= pmem)
		k0oneoff = (k0addr - private.p_scachesize);
	else
 		k0oneoff = (k0addr + private.p_scachesize);
#endif
#endif /* _MEM_PARITY_WAR */

#if IP19 || IP32
	err_info.eidata_lo = 0xdeadbeef;
	err_info.eidata_hi = 0xdeadbeef;
#else
	err_info.eidata_lo = *(uint *)(k0addr);
	err_info.eidata_hi = *(uint *)(k0addr+BYTESPERWD);
#endif

	/* ASSERT(edp->e_badecc); */
	err_info.cbits_in = edp->e_badecc;
	edp->e_lo_badval = err_info.eidata_lo;
	edp->e_hi_badval = err_info.eidata_hi;

#if IP20 || IP22 || IPMHSIM
	/*
	 *	The IP20 and IP22 (MC-based) systems have only parity 
	 * memory, so correction is not possible, except when an 
	 * instruction overwrites the memory.  Therefore, we just
	 * reflect this error to trap(), for appropriate disposition.
	 */

	/* force a software trap */
	return(-1);
#elif IP32
	/*
	 * the only errors which will get reflected in the cache on
	 * IP32 are bad address errors and non-correctable ECC errors
	 * in any case, none of these are fixable.
	 * XXX: this is probably wrong, we need to extract the correct 
	 * syndrome for the appropriate byte, but I'm lazy right now.
	 */
	edp->e_syndrome = (uint)
		(READ_REG64(PHYS_TO_K1(CRM_MEM_ERROR_ECC_SYN), _crmreg_t) &
			0xffffffff);
	return(1);

#else /* !(IP20 || IP22) */

	hi_taglo = edp->e_s_taglo;
	tags[TAGLO_IDX] = hi_taglo;
	tags[TAGHI_IDX] = 0;
	/* change line-state from clean to dirty so that the cached read
	 * we'll do one 2nd-cache-size-segment up from the bad addr will
	 * flush the current line through the RMI, fixing memory */
	tags[TAGLO_IDX] = ((tags[TAGLO_IDX] & ~SSTATEMASK) | SDIRTYEXCL);
	_c_ist(CACH_SD, k0addr, tags);

	hi_syn = calc_err_info(DATA_CBITS, &err_info);

#ifdef SYNDROME_CHECKING
	if (!hi_syn) {	/* NO ERROR! */
	    printf("WEIRDITY!!! Check ecc on all dbl wds in line!\n");
	    startaddr = k0addr & ~(SCACHE_LINESIZE-1);
	    printf("k0addr 0x%x, startaddr 0x%x\n",k0addr,startaddr);
	    for (addr = startaddr;addr < startaddr+SCACHE_LINESIZE;addr += 8) {

		alt_err_info.eidata_lo = *(uint *)addr;
		alt_err_info.eidata_hi = *(uint *)(addr+BYTESPERWD);
		_c_ilt_n_ecc(CACH_SD, addr, tags, &data_ecc);
		alt_err_info.cbits_in = data_ecc;
		lo_syn = calc_err_info(DATA_CBITS, &alt_err_info);
		if (lo_syn) {
			foundone++;
			printf("addr 0x%x, w0 0x%x, w1 0x%x, ",
				addr, alt_err_info.eidata_lo,
				alt_err_info.eidata_hi);
			printf("cbin 0x%x, cbout 0x%x, syn 0x%x\n",
				alt_err_info.cbits_in,
				alt_err_info.cbits_out,lo_syn);
		}
	    }
	    if (!foundone) {
		printf("NO SYNDROMES IN LINE BEGINNING AT 0x%x ",addr);
		printf("were non-zero\n");
	    }
	    ecc_log_error(NO_ERROR, index);
	    return(1);
	}
#else
	if (!hi_syn) {	/* NO ERROR! */
		ecc_log_error(NO_ERROR, index);
		return(0);
	}
#endif

	/* use the syndrome to determine the severity of the error */
	edp->e_syndrome = hi_syn;
	syn_info = err_info.syn_info;

	/* if it is a correctable error (DBx or CBx), force it back
	 * through the RMI to scrub memory. */
	if (syn_info.type == DB || syn_info.type == CB) {
		edp->e_user = 0;
		*(volatile uint *)k0oneoff;
/* XXXXXXXXXXXXXXXXXXX SHOULD I CHECK IF THE FIX WORKED??? */
		return(0);
	} else {	/* 2-bit or greater: can't fix it */
#if IP19 || IP32
		eccfp[ECCF_PADDR] = physaddr & 0x0ffffffff;
		eccfp[ECCF_PADDRHI] = physaddr>>32;
#else
		eccfp[ECCF_PADDR] = physaddr;
#endif
		return(1);	/* ecc_handler will kill process or IRIX */
	}
#endif /* !(IP20 || IP22) */
	/*NOTREACHED*/

} /* ecc_fixmem */
#endif /* ! MCCHIP */


volatile int cache_hit = -1;

/* ARGSUSED */
#if IP19
static
real_ecc_fixcache(
    uint index,
    eframe_t *efp,
    k_machreg_t *eccfp,
    uint cache_err,
    k_machreg_t errorepc,
    volatile ecc_info_t *ecc_info_param )
#else
static
ecc_fixcache(
    uint index,
    eframe_t *efp,
    k_machreg_t *eccfp,
    uint cache_err,
    k_machreg_t errorepc)
#endif
{
	int offset;
	err_desc_t *edp = (err_desc_t *)&(ECC_INFO(desc[index]));
	__psunsigned_t s_caddr = PHYS_TO_K0(SCACHE_PADDR(edp));
	__psunsigned_t p_caddr = PHYS_TO_K0(edp->e_vaddr);
	uint tags[NUM_TAGS];
	uint data_ecc;
	uint res = 0;

/* XXXXXXXXXXXXXXXXXXXXXXXX  SET ALL e_ VALUES! */

	/* set e_p_taglo to PI tag if main error is in PI or SI (i.e.
	 * *instruction* error in in either cache); if PD or SD ==> PD.
	 * (Use computed virtual address when accessing the P-caches.) */
	_c_ilt_n_ecc((((edp->e_location == CACH_PI) ||
		       (edp->e_location == CACH_SI))
		      ? CACH_PI
		      : CACH_PD),
		     p_caddr, tags, &data_ecc);

	edp->e_p_taglo = tags[TAGLO_IDX];

#ifdef R4000PC
	if ((r4000_config & CONFIG_SC) != 0) /* 0 == scache present */
		edp->e_s_taglo = 0;
	else
#endif /* R4000PC */
	{
	_c_ilt_n_ecc(CACH_SD, s_caddr, tags, &data_ecc);
	edp->e_s_taglo = tags[TAGLO_IDX];

	/* if EI bit set, there is corrupted data in primary Dcache.  
	 * Invalidate the line by zeroing-out the tag */
	if (cache_err & CACHERR_EI) {
#ifdef IP19
		/* Various wierd errors afflict an IP19 after a store-miss
		 * cache-error.  It appears that the state of the cache
		 * is really confused.  The cpu rarely recovers and other
		 * cpus seem to get errors when accessing this cpu's
		 * cache.  So simply panic now.
		 */

	  	ecc_info_param->ecc_panic_recoverable = 2;
		ecc_assign_msg(ECC_PANIC_MSG, index, ecc_ei_norecover);
		return(1);

#ifdef DO_NOT_ENABLE
		/* Theorectically correct IP19 store-miss recover code */
		tags[TAGLO_IDX] = 0;

		_c_ilt(CACH_PD,p_caddr,tags);
		tags[TAGLO_IDX] &= ~PSTATEMASK;	/* change state to invalid */
		_c_ist(CACH_PD,p_caddr,tags);
		
		/* On an MP system, an intervention from another cpu could
		 * cause that cpu to get this cacheline with corrupt data
		 * and good ECC (intervention will flush data from primary
		 * to secondary and since DW bit is set will update secondary
		 * with good ECC).  To make sure that we don't silently
		 * consume bad data we check that the secondary cacheline
		 * is still marked "dirty" after we've invalidated the
		 * primary cache.
		 */
		_c_ilt(CACH_SD, s_caddr, tags);

		if (!(DIRTY_S_TAG(tags[TAGLO_IDX]))) {
			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_ei_notdirty);
			return(1);
		}
#endif /* DO_NOT_ENABLE */
		
#else /* !IP19 */
		tags[TAGLO_IDX] = 0;
		_c_ist(CACH_PD,p_caddr,tags);
#endif /* !IP19 */
	}
	}

	/* NOTE: in all correctable-cases we must verify that the fix
	 * succeeded in order to avoid an infinite-loop of instruction-
	 * restarts re-raising the ecc exception in the event of stuck
	 * cache bits.  Otherwise we could just invalidate the line and
	 * let the restart refill the line.
	 * 	If there are errors in both tag and data, start with the
	 * tag.  Depending on how we fix the tag error, the data error may
	 * be corrected also.  If not, see comment for CACHERR_EB at end
	 * of ecc_handler.  (Either the tag will be successfully repaired
	 * or we will panic, so if the data error remains, a subsequent
	 * exception will spotlight it).
	 */
	ASSERT(edp->e_location >= CACH_PI && edp->e_location <= CACH_SD);

	/* set index into error-counting array to proper cache
	 * ( 2x cuz tag-data pairs for each cache) */
	if (edp->e_location == CACH_SD)	/* 2ndary is I and D combined */
		offset = (2 * CACH_SI);
	else
		offset = (2 * edp->e_location);

	switch(edp->e_location) {

	case CACH_PD:
	case CACH_PI:
		/* err is in primary: p_taglo is useful.  Poke it into frame */
		eccfp[ECCF_TAGLO] = edp->e_p_taglo;
		/* At this point the R4K doesn't return the correct 
		 * checkbits for data in either of the primary caches,
		 * so the only potentially-relevant ecc value is
		 * contained in the p_taglo. */
		break;

	case CACH_SD:
#ifndef R10000
	case CACH_SI:
#endif /* !R10000 */
		/* error is in 2ndary; save s_taglo on eccframe */
		eccfp[ECCF_TAGLO] = edp->e_s_taglo;
		break;

	default:
		ecc_assign_msg(ECC_PANIC_MSG, index, ecc_inval_loc);
		return(1);

	}

	ASSERT(edp->e_tag_or_data>=DATA_ERR && edp->e_tag_or_data<=D_AND_T_ERR);

	if (edp->e_tag_or_data != DATA_ERR) {	/* tag or both */
		res = ecc_fixctag(edp->e_location, index);
		if (res == 2) {		/* unfixable 2nd-level tag: panic */
			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_bad_s_tag);
			res = 1;
		}
	} else					/* error in data field */
		res = ecc_fixcdata(edp->e_location, index, eccfp);

	if (edp->e_tag_or_data == DATA_ERR)
		ecc_log_error(offset, index);
	else if (edp->e_tag_or_data == TAG_ERR)
		ecc_log_error((offset+1), index);
	else { /* errors in both data and tag */
		ecc_log_error(offset, index);
		ecc_log_error((offset+1), index);
	}
		
	return(res);

} /* ecc_fixcache */

#ifdef R4600
/*
 * ecc_find_bad_cline -- searchs for tag which caused a cache tag
 *                       error.  returns the index in the parameter
 *                       *idx.
 *
 * returns 1 if it found a tag which had bad parity at the correct
 * index, 0 if not.
 * 
 * XXX: this routine has a hidden assumption that loc is CACH_PI or
 *      CACH_PD.
 */
ecc_find_bad_cline(int loc, __psunsigned_t p_caddr, uint *idx)
{
  uint tags[NUM_TAGS];
  extern int two_set_pcaches;
  
  ASSERT(loc == CACH_PI || loc == CACH_PD);

  _read_tag(loc,(caddr_t)p_caddr,(int *)tags);
  if (ecc_bad_ptag(tags[TAGLO_IDX])) {
    *idx = p_caddr;
    return(1);
  }

  _read_tag(loc,(caddr_t)(p_caddr^two_set_pcaches),(int *)tags);
  if (ecc_bad_ptag(tags[TAGLO_IDX])) {
    *idx = p_caddr^two_set_pcaches;
    return(1);
  }
  return(0);
}
#endif /* R4600 */


/* the ptaglo field of the taglo register holds bits 35..12 of the
 * physaddr that the line contains.  This mask grabs that field 
 * from a virtual address, which is then shifted to its correct
 * position in ptaglo (>> 4) */
#define PTAG_ADDRMASK	0xFFFFF000

/* fix tag error in 'loc' cache.  'index' indicates the
 * frame of variables being used during this invokation
 * of ecc_handler(). */
#if IP19
real_ecc_fixctag(uint loc, int index, volatile ecc_info_t *ecc_info_param)
#else
ecc_fixctag(uint loc, int index)
#endif
{
	err_desc_t *edp = (err_desc_t *)&(ECC_INFO(desc[index]));
	uint tags[NUM_TAGS];
	uint p_taglo = edp->e_p_taglo;
	uint s_taglo = edp->e_s_taglo;
	uint new_p_taglo;
	error_info_t err_info;
	uint tag_syndrome;
	eccdesc_t tag_syn_info;
	uint ce_sidx = (edp->e_cache_err & CACHERR_SIDX_MASK);
	__psunsigned_t s_caddr = PHYS_TO_K0(SCACHE_PADDR(edp));
	__psunsigned_t p_caddr = PHYS_TO_K0(edp->e_vaddr);
	__uint64_t physaddr;	/* must be 64-bits always (16 GB memory) */
#ifdef R4000PC
	int pidx = 0;
#if R4600
	extern int two_set_pcaches;
#endif
#endif
	/* uncorrectable errors in 2ndary tags (i.e. > 1 bit) cause a
	 * fatal enigma regardless of whether the data in the line is
	 * dirty or clean: with a corrupted 2ndary tag we can't identify
	 * the (also possibly corrupted) primary line(s) associated with
	 * it.  This means that none of the cacheops are guaranteed, and
	 * the state of the caching-system is or may be indeterminate.
	 * Panic.  Note that if the bad 2ndary line is holding an
	 * instruction (and is therefore clean) we could blow out the
	 * primary I-cache and invalidate this 2ndary line; at this
	 * time, however, we're just going to panic on all uncorrectable
	 * errors in 2ndary tags.
	 */
	if (loc == CACH_SI || loc == CACH_SD) {
		/* since the error is in the tag, all the e_values we set
		 * in ecc_handler using it are suspect.  We know that the
		 * sidx field in cache_err is correct: use it to fetch the
		 * 2ndary line.  e_s_taglo, e_p_taglo, e_paddr and e_vaddr
		 * may be wrong. Calculate the syndrome, then either 
		 * a) fix it and recalculate e_paddr and c_vaddr if the
		 *    bad bit was a data-bit (not a checkbit), or
		 * b) panic if the error is uncorrectable.
		 */

		_c_ilt(loc, PHYS_TO_K0(ce_sidx), tags);
		err_info.eis_taglo = tags[TAGLO_IDX];
		err_info.cbits_in = SET_CBITS_IN;
		tag_syndrome = calc_err_info(TAG_CBITS, &err_info);

		ASSERT(err_info.cbits_in == (tags[TAGLO_IDX] & SECC_MASK));
		edp->e_prevbadecc = edp->e_badecc;
		edp->e_badecc = err_info.cbits_in;	/* from s_taglo */
		edp->e_syndrome = tag_syndrome;

		if (!tag_syndrome) {	/* NO ERROR! */
			ecc_assign_msg(ECC_ERROR_MSG, index, ecc_no_stagerr);
			ecc_log_error(NO_ERROR, index);
			return(1);
		}

		/* use the syndrome to determine the severity of the error */
		tag_syn_info = err_info.syn_info;

		/* DBx and CBx errors are correctable; all others panic */
		if (tag_syn_info.type != DB && tag_syn_info.type != CB)
			return(2);

		/* if the error is in a databit, fix it and recalculate
		 * all values that were set relying upon possibly-bogus
		 * tag values.  If the error is in a checkbit, let the
		 * R4K correct it when we store the tag.  Note that the
		 * syndrome identifies the bad bit number *in the internal
		 * format* (i.e. as it is stored in the 2ndary cache),
		 * not as it appears in the taglo register.  If it is a
		 * data-bit error we will fix it; the syndrome bitposition
		 * must therefore be translated to taglo format. */
		if (tag_syn_info.type == DB) {
			uint bitpos, badbit;
	
			bitpos=xlate_bit(tag_syn_info.type,tag_syn_info.value);
			badbit = (0x1 << bitpos);
			tags[TAGLO_IDX] ^= badbit;

			edp->e_s_taglo = tags[TAGLO_IDX];
	
			/* Now that we have a correct 2ndary tag, recalculate
			 * all values that were based on the bad one.
			 * ce_sidx is paddr[21..3], 2nd taglo is paddr[35..17]
			 * but must be shifted to proper position.  Together
			 * they make up the full vaddress. */
			physaddr = (ce_sidx |
				((edp->e_s_taglo & SADDRMASK) << SADDR_SHIFT));
			edp->e_paddr = physaddr;
			s_caddr = PHYS_TO_K0(SCACHE_PADDR(edp));

#if IP19
			edp->e_vaddr = (physaddr & (ecc_info_param->ecc_picache_size-1));
#else
			edp->e_vaddr = (physaddr & (picache_size-1));
#endif
			p_caddr = PHYS_TO_K0(edp->e_vaddr);
	
			/* set e_p_taglo to PI tag if error is in PI or SI
			 * (i.e. *instruction* error in either cache);
			 * if PD or SD ==> PD. */
			if (loc == CACH_PI || loc == CACH_SI)
				_c_ilt(CACH_PI, p_caddr, tags);
			else
				_c_ilt(CACH_PD, p_caddr, tags);
			edp->e_p_taglo = tags[TAGLO_IDX];
		} /* error in tag data bit */

		/* now store the corrected tag */
		tags[TAGLO_IDX] = edp->e_s_taglo;
		tags[TAGHI_IDX] = 0;
		_c_ist(loc,s_caddr,tags);

		/* Check that the newly-computed tag is correct */
		_c_ilt(loc,s_caddr,tags);
		err_info.eis_taglo = tags[TAGLO_IDX];

		tag_syndrome = calc_err_info(TAG_CBITS, &err_info);

		if (tag_syndrome) {	/* panic/kill user */
			ecc_assign_msg(ECC_PANIC_MSG,index,
				ecc_stfix_failed);
			return(1);
		}
		return(0);
	} else { /* it is a primary-tag error.  We can reconstruct
	 	  * the bad tag from the info we have, even if the
		  * data in the line is dirty.  2ndary-tag ecc is 
		  * checked each time a line is transferred to primary,
		  * and traps at that point.  Therefore, primary lines 
		  * already transferred from this 2ndary line are
		  * valid (because the ecc-check didn't cause a trap
		  * during those fills), and the current primary-fill
		  * didn't occur because of this trap
		  */
		tags[TAGLO_IDX] = tags[TAGHI_IDX] = 0;
#ifdef R4000PC
		if ((r4000_config & CONFIG_SC) != 0) { /* 0 == scache present */
			/* must invalidate the line */
			if (loc == CACH_PI) {
				_c_ist(loc,p_caddr,tags);
#ifdef R4600
				_c_ist(loc,p_caddr^two_set_pcaches,tags);
#endif
			} else {
#ifdef R4600
			  uint ttags[NUM_TAGS];
			  int numcleaned = 0;
			  /*
			   * XXX: if we don't find a bad line at the
			   * appropriate index what should we do?  We
			   * can't really go on because we can't invalidate
			   * the line.  I suppose that we should panic.
			   * First, though we'll see if both lines are clean
			   * we'll just invalidate them, 'case this just
			   * means that one of them had a parity error in
			   * the w or w' bit of the tag.
			   */
				if (ecc_find_bad_cline(loc,p_caddr,(uint *)&pidx)) {
#endif
					_c_ist(loc,pidx,tags);
					goto send_bad_ptag_msg;
				}
				_read_tag(loc,(caddr_t)p_caddr,(int *)ttags);
				if ((ttags[TAGLO_IDX] & PSTATEMASK)
				    == PCLEANEXCL) {
					_c_ist(loc,pidx,tags);
					numcleaned++;
				}
#ifdef R4600
				_read_tag(loc,(caddr_t)(p_caddr^two_set_pcaches),(int *)ttags);
				if ((ttags[TAGLO_IDX] & PSTATEMASK)
				    == PCLEANEXCL) {
					_c_ist(loc,pidx,tags);
					numcleaned++;
				}
#endif
				/*
				 * although we didn't find the bad tag
				 * we were able to invalidate both elements
				 * of the set.  We can consider this to be
				 * success.
				 */
				if (numcleaned > 1)
					return(0);
			}

			/* primary cache only: fail if in data cache */
			if (edp->e_cache_err & (CACHERR_ER | CACHERR_EB))
				goto send_bad_ptag_msg; /* unrecoverable */
			return(0);
		}
#endif /* R4000PC */

		if (!ecc_bad_ptag(p_taglo)) {
			ecc_assign_msg(ECC_ERROR_MSG, index, ecc_no_ptagerr);
			ecc_log_error(NO_ERROR, index);
			return(1);
		}

		/* construct a new ptag from e_vaddr and set state
		 * depending on scache */
		new_p_taglo = ((edp->e_vaddr&PTAG_ADDRMASK)>>PADDR_SHIFT);
		if ((s_taglo & SSTATEMASK) == SCLEANEXCL)
			new_p_taglo |= PCLEANEXCL;
		else
			new_p_taglo |= PDIRTYEXCL;

		tags[TAGLO_IDX] = new_p_taglo;
		tags[TAGHI_IDX] = 0;
		_c_ist(loc,p_caddr,tags);

		_c_ilt(loc,p_caddr,tags);
		if (ecc_bad_ptag(tags[TAGLO_IDX])) {
#ifdef R4000PC
send_bad_ptag_msg:
#endif /* R4000PC */
			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_ptfix_failed);
			return(1);
		}
		return(0);
	} /* primary-tag error */

} /* ecc_fixctag */


#ifdef ECC_DEBUG
int eccdebug_foundone=0;
__psunsigned_t eccdebug_badaddr[128], eccdebug_loc[128];
int eccdebug_syndrome[128];
int eccdebug_datalo[128], eccdebug_datahi[128], eccdebug_ecc[128], eccdebug_cnt[128];
int eccdebug_entry_cnt=0;
__psunsigned_t eccdebug_entry_loc[128];
#endif /* ECC_DEBUG */

#ifdef IP19

extern k_machreg_t get_config(void);

int
ecc_check_correctable(volatile uint * loc, err_desc_t *edp,
		      volatile ecc_info_t *ecc_info_param)
{
	__psunsigned_t addr, startaddr, paddr;
	int lo_syn;
	error_info_t alt_err_info;
	uint tags[NUM_TAGS], data_ecc, dblwrd_mask;
	int foundone=0, mbe=0, plinesize=0;

	/* if not IP19 or no datap, then really can't check */
	if (loc == 0)
		return(0);
#ifdef ECC_DEBUG
	eccdebug_entry_loc[eccdebug_entry_cnt] = (__psunsigned_t)loc;
	eccdebug_entry_cnt++;
	if (eccdebug_entry_cnt == 128)
		eccdebug_entry_cnt = 0;
#endif /* ECC_DEBUG */

	if (get_config() & CONFIG_DB) {
		plinesize = 32;
		/* primary cacheline size is four double-words, which will be
		 * the resolution of the cache error exception.
		 */
		startaddr = (__psunsigned_t)loc & ~0x1f;
		paddr = PHYS_TO_K0(edp->e_paddr & ~0x1f);

		/* This following statement sets the correct initial bit
		 * position in the doubleword mask.
		 */
		dblwrd_mask = (1 << (((__psunsigned_t)loc & 0x60) >> 3));
	} else {
		plinesize = 16;
		/* primary cacheline size is two double-words, which will be
		 * the resolution of the cache error exception.
		 */
		startaddr = (__psunsigned_t)loc & ~0x0f;
		paddr = PHYS_TO_K0(edp->e_paddr & ~0x0f);

		/* This following statement sets the correct initial bit
		 * position in the doubleword mask.
		 */
		dblwrd_mask = (1 << (((__psunsigned_t)loc & 0x70) >> 3));
	}

	/* We scan that portion of the secondary cacheline which caused the
	 * cache error exception.  This corresponds to a primary cacheline
	 * which is 16 bytes (two doublewords) on the IP19.  Either
	 * doubleword may contain an error.
	 */
	for (addr = startaddr;addr < startaddr+plinesize;addr += 8,paddr +=8) {

		tags[TAGLO_IDX] = tags[TAGHI_IDX] = 0;
		alt_err_info.eidata_lo = *(uint *)addr;
		alt_err_info.eidata_hi = *(uint *)(addr+BYTESPERWD);
		_c_ilt_n_ecc(CACH_SD, paddr, tags, &data_ecc);
		alt_err_info.cbits_in = data_ecc;
		lo_syn = calc_err_info(DATA_CBITS, &alt_err_info);
		if (lo_syn)  {
#ifdef ECC_DEBUG 
			eccdebug_loc[eccdebug_foundone] = (__psunsigned_t)loc;
			eccdebug_cnt[eccdebug_foundone] = eccdebug_entry_cnt;
			eccdebug_badaddr[eccdebug_foundone] = addr;
			eccdebug_syndrome[eccdebug_foundone] = lo_syn;
			eccdebug_datalo[eccdebug_foundone] = alt_err_info.eidata_lo;
			eccdebug_datahi[eccdebug_foundone] = alt_err_info.eidata_hi;
			eccdebug_ecc[eccdebug_foundone] = alt_err_info.cbits_in;
			eccdebug_foundone++;
			if (eccdebug_foundone == 128)
				eccdebug_foundone = 0;
#endif /* ECC_DEBUG */

			switch (alt_err_info.syn_info.type) {
			case DB:	/* 1-bit err in data */
			case CB:
				if (!mbe) {
					edp->e_s_taglo = tags[TAGLO_IDX];
					edp->e_prevbadecc = edp->e_badecc;
					edp->e_badecc = data_ecc;
					edp->e_syndrome = lo_syn;
					edp->e_lo_badval = alt_err_info.eidata_lo;
					edp->e_hi_badval = alt_err_info.eidata_hi;
					edp->e_paddr = K0_TO_PHYS(paddr);
				}
				edp->e_sbe_dblwrds |= dblwrd_mask;
				foundone++;
				break;

			case B2:	/* error is 2-bit or greater */
			case B3:
			case B4:
			case UN:
			default:
				edp->e_s_taglo = tags[TAGLO_IDX];
				edp->e_prevbadecc = edp->e_badecc;
				edp->e_badecc = data_ecc;
				edp->e_syndrome = lo_syn;
				edp->e_lo_badval = alt_err_info.eidata_lo;
				edp->e_hi_badval = alt_err_info.eidata_hi;
				edp->e_paddr = K0_TO_PHYS(paddr);
				edp->e_mbe_dblwrds |= dblwrd_mask;
				mbe++;
			} /* switch */
		}
		dblwrd_mask = dblwrd_mask << 1;

	}
	if (mbe)
		return(-1);
	else
		return(foundone);
}
#endif /* IP19 */

/* fix data error in 'loc' cache.  'index' indicates the frame of
 * variables being used during this invokation of ecc_handler. */
#if IP19
real_ecc_fixcdata(uint loc, int index, k_machreg_t *eccfp,
	     volatile ecc_info_t *ecc_info_param)
#else
ecc_fixcdata(uint loc, int index, k_machreg_t *eccfp)
#endif
{
	err_desc_t *edp = (err_desc_t *)&(ECC_INFO(desc[index]));
	uint tags[NUM_TAGS];
	__psunsigned_t p_caddr = PHYS_TO_K0(edp->e_vaddr);
	uint pidx_test, pidx_max;
	error_info_t err_info;
#ifndef IP19
	uint data_syndrome, data_ecc;
	__psunsigned_t s_caddr = PHYS_TO_K0(SCACHE_PADDR(edp));
	volatile uint *p_cptr = (volatile uint *)p_caddr;
	eccdesc_t d_syn_info;
#endif
	__psunsigned_t prim_addr;
	volatile uint *datap=0;
	__psunsigned_t local_ecc_kvaddr;

#ifdef R4000PC
	/*
	 * If we are on an R4000PC or R4600, we only have primary
	 * caches, and we only have parity, so the best we can
	 * do is to invalidate the line, if it is clean.  Otherwise,
	 * we give up.
	 *
	 * However, we have higher level routines which can recover
	 * from some data errors.  Since neither the R4000 Rev. 2.2
	 * nor the R4600 Rev. 1.7 return the correct ECC check bits
	 * for the data cache, we cannot simply look at the check bits
	 * to find bad words.
	 */
	if ((r4000_config & CONFIG_SC) != 0) { /* 0 == scache present */
#ifdef _MEM_PARITY_WAR
		if (ecc_fixup_caches(loc, edp->e_paddr, edp->e_vaddr, 
				     edp->e_flags & E_PADDR_MC))
			return(0);
		else 
#endif
			return(-1);
			
	}
#endif /* R4000PC */

	/* Currently the R4K does not return the ecc byte-checkbits 
	 * for the double-word of data at the specified address during
	 * the index load tag cacheop.  The desired algorithm for 
	 * dealing with primary cache data errors would basically 
	 * panic if the line was dirty (since there is only parity,
	 * so we can't fix it), and invalidate the tag and refetch
	 * if it was clean (obviously always the case if the error
	 * was in the I-cache).  However, we must avoid infinite-ECC-
	 * exception-loops which would occur when the error was due
	 * to a stuck bit, for example, and wasn't fixed during the
	 * refetch.  To do this we must either save a 'sufficient'
	 * amount of history--whatever that amount might be--or be
	 * able to check whether the refetch fixed the error.  With
	 * this bug we can't do the latter, and the former is a 
	 * rather unpalatable alternative, so we'll just panic for now,
	 * but only after determining that the paddrs match (otherwise
	 * it is probably either a) a 'phantom' exception caused 
	 * when the bad line was replaced by a new one: the exception
	 * still occurs but the error is no longer in the line, or
	 * b) a manifestation of the R4K bug (fixed in 2.0) in which
	 * the vidx (and apparently the sidx also) info in the 
	 * cacherr was bogus when a parity error was detected in
	 * the primary).
	 */

	if ((loc == CACH_PI) || (loc == CACH_PD)) {
	    /*  Try every possible PIDX  */
#if IP19
	    pidx_max = ((loc == CACH_PI) ?
			ecc_info_param->ecc_picache_size :
			ecc_info_param->ecc_pdcache_size) - NBPP;
#else
	    pidx_max = ((loc == CACH_PI) ? picache_size : pdcache_size) - NBPP;
#endif

	    for (pidx_test = 0; pidx_test <= pidx_max; pidx_test += NBPP) {

		edp->e_vaddr = pidx_test | (edp->e_vaddr & (NBPP-1));
		p_caddr = PHYS_TO_K0(edp->e_vaddr);

		_c_ilt(loc, p_caddr, tags);
		prim_addr = ((tags[TAGLO_IDX] & PADDRMASK) << PADDR_SHIFT);
		/* just check that the bits from taglo match the physaddr */
		if (prim_addr == (POFFSET_PADDR(edp))) {
			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_p_data_err);
			return(1);
		}
	    }
	    /* if get to here, then no error found!!?? */
	    ecc_assign_msg(ECC_ERROR_MSG, index, ecc_no_pdataerr);
	    ecc_log_error(NO_ERROR, index);
	    return(0);
	} 

	/* error is in CACH_SI or CACH_SD */

	tags[TAGLO_IDX] = tags[TAGHI_IDX] = 0;

	/* do cached read to get values of dbl-words.  This would
	 * cause another ecc exception but we have the SR_DE bit set.  */
#ifndef IP19
	err_info.eidata_lo = *p_cptr;
	err_info.eidata_hi = *(p_cptr+1);

	/* but fetch tag by 2ndary (physical) addr */
	_c_ilt_n_ecc(loc, s_caddr, tags, &data_ecc);
	edp->e_s_taglo = tags[TAGLO_IDX];
	edp->e_prevbadecc = edp->e_badecc;
	edp->e_badecc = data_ecc;
	err_info.cbits_in = (unchar)data_ecc;
	data_syndrome = calc_err_info(DATA_CBITS, &err_info);
	edp->e_syndrome = data_syndrome;
	edp->e_lo_badval = err_info.eidata_lo;
	edp->e_hi_badval = err_info.eidata_hi;
	d_syn_info = err_info.syn_info;
	edp->e_sbe_dblwrds = edp->e_mbe_dblwrds = 0;

	if (!data_syndrome) {
		/* no error in this dbl word */

		ecc_assign_msg(ECC_ERROR_MSG, index, ecc_no_sdataerr);
		ecc_log_error(NO_ERROR, index);
		return(0);
	}

	/* If the line is clean we don't need to protect the data:
	 * invalidate, refill, and check it again.  Must invalidate
	 * all the primary lines it maps too.
	 */
	if (loc==CACH_SI || (loc==CACH_SD && CLEAN_S_TAG(edp->e_s_taglo))) {
		if (!_c_hinv(loc,s_caddr)) {	/* missed 2ndary! */
			ecc_assign_msg(ECC_ERROR_MSG, index,
				       ecc_ft_hinv_m_sc);
			return(1);
		}
		err_info.eidata_lo = *p_cptr;
		err_info.eidata_hi = *(p_cptr+1);

		_c_ilt_n_ecc(loc, s_caddr, tags, &data_ecc);
		err_info.cbits_in = data_ecc;
		data_syndrome = calc_err_info(DATA_CBITS, &err_info);

		if (data_syndrome) {	/* didn't fix it: panic */
			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_sdcfix_failed);
			edp->e_prevbadecc = data_ecc;
			edp->e_2nd_syn = data_syndrome;
#ifdef DEBUG_ECC
			f_staglo = tags[TAGLO_IDX];
			_c_ilt(CACH_PD, p_caddr, tags);
			f_ptaglo = tags[TAGLO_IDX];
			f_loval = err_info.eidata_lo;
			f_hival = err_info.eidata_hi;
			f_p_caddr = p_caddr;
			f_s_caddr = s_caddr;
#endif
			return(1);
		} else {
			ecc_assign_msg(ECC_INFO_MSG, index, ecc_sdcfix_good);
			return(0);
		}

	} else {  /* dirty line: can't invalidate line and refetch */

		/* Now the severity of the error becomes relevant.
		 * If it is a one bit error we can force the line 
		 * out to memory through the RMI--which corrects 
		 * single-bit errors--then read it back and check
		 * if it is now correct.  Panic if not--probably
		 * a stuck bit.
		 */
		ASSERT(loc != CACH_SI);

		switch(d_syn_info.type) {
		case 0:		/* no error found */
		case DB:	/* 1-bit err in data: write out then refetch */
		case CB:
			break;

		case B2:	/* error is 2-bit or greater: can't fix it */
		case B3:
		case B4:
		case UN:
		default:
			eccfp[ECCF_PADDR] = edp->e_paddr;
			ecc_assign_msg(ECC_PANIC_MSG, index,
				ecc_md_sddfix_failed);
			return(1);
		} /* switch */

		_c_hwbinv(CACH_SD, s_caddr);

		/* now refetch the info and ensure that it is fixed */
		err_info.eidata_lo = *p_cptr;
		err_info.eidata_hi = *(p_cptr+1);

		_c_ilt_n_ecc(loc, s_caddr, tags, &data_ecc);
		err_info.cbits_in = data_ecc;
		data_syndrome = calc_err_info(DATA_CBITS, &err_info);

		if (data_syndrome) {	/* didn't fix it: panic */
			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_sddfix_failed);
			edp->e_prevbadecc = data_ecc;
			edp->e_2nd_syn = data_syndrome;

#ifdef DEBUG_ECC
			f_staglo = tags[TAGLO_IDX];
			_c_ilt(CACH_PD, p_caddr, tags);
			f_ptaglo = tags[TAGLO_IDX];
			f_loval = err_info.eidata_lo;
			f_hival = err_info.eidata_hi;
			f_p_caddr = p_caddr;
			f_s_caddr = s_caddr;
#endif

			return(1);
		} else {
			ecc_assign_msg(ECC_INFO_MSG, index, ecc_sddfix_good);
			return(0);
		}

	} /* else dirty line */
#else /* IP19 */
	/* Currently code assumes that the primary-icache and primary-dcache
	 * linesizes are the same.  This is used to determine the number
	 * of doublewords which must be read from the secondary in order
	 * to check ECC values.  All IP19 systems currently have a
	 * primary cache linesize of 16 bytes (IB == DB == 0) so just
	 * verify this assumption in case it changes.
	 */
	if (get_config() & CONFIG_IB) {
		if (!(get_config() & CONFIG_DB)) {
			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_mixed_psize);
			return(1);
		}
	} else {
		if (get_config() & CONFIG_DB) {
			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_mixed_psize);
			return(1);
		}
	}
	/* The following code allows us to pickup the virtual address using
	 * an uncached load so as not to interfere with the state of the
	 * cache while we're examining the cause of the error.
	 */
#if 0
	local_ecc_kvaddr = *(__psunsigned_t *)((K0_TO_K1(&ecc_kvaddr_vcecolor)));
#endif
	local_ecc_kvaddr = ecc_info_param->ecc_vcecolor;
	if (local_ecc_kvaddr) {
		int vcecolor=0;
		char *vceaddr=0;
		pde_t pde;
		extern uint ecc_tlbdropin(unsigned char *, caddr_t, pte_t);
			
		vcecolor = (edp->e_s_taglo & STAG_VINDEX)<<STAG_VIND_SHIFT;
		vceaddr = (char*)(local_ecc_kvaddr + vcecolor);
		pde.pgi = mkpde(PG_VR|PG_M|PG_G|PG_SV|pte_cachebits(),
				btoct(edp->e_paddr));

		ecc_tlbdropin(0, vceaddr, pde.pte);
		datap = (uint *)(((__psunsigned_t)vceaddr & ~POFFMASK)
				 +poff(edp->e_paddr));

		err_info.eidata_lo = *datap;
		err_info.eidata_hi = *(datap+1);

	} else {
		ecc_assign_msg(ECC_PANIC_MSG, index, ecc_scerr_too_early);
		return(1);
	}

	/* but fetch tag by 2ndary (physical) addr */

	edp->e_sbe_dblwrds = edp->e_mbe_dblwrds = 0;

	if (ecc_check_correctable(datap,edp,ecc_info_param)==0) {
		/* no error in this dbl word */

		ecc_assign_msg(ECC_INFO_MSG, index, ecc_no_sdataerr);
		ecc_log_error(NO_ERROR, index);
		return(0);
	}

	/* If the line is clean we don't need to protect the data:
	 * invalidate, refill, and check it again.  Must invalidate
	 * all the primary lines it maps too.
	 *
	 * NOTE: If the cache error is reported due to an external request
	 * (i.e. ES is set), then we can actually have loc == CACH_SI but
	 * the actual problem might be in some other cacheline which is
	 * "dirty" (that other cacheline address is indicated by s_taglo).
	 * So checking for loc == CACH_SI is not sufficient unless we
	 * qualify it with ES==0.
	 *
	 * It's better to replace this check with a "simple" check for
	 * CLEAN_S_TAG().
	 */
	if (CLEAN_S_TAG(edp->e_s_taglo)) {
		if (!_c_hinv(loc,(__psunsigned_t)datap)) {
			/* missed 2ndary! */
			/* We can miss here IFF another cpu (or I/O) has
			 * referenced this line after we performed the
			 * ecc check which explicitly loaded data from this
			 * scacheline.
			 * So we check that the line is currently invalid,
			 * then we reload and check the ECC to make sure
			 * that a multiple-bit error did not occur.
			 */

			_c_ilt(loc, (__psunsigned_t)datap, tags);

			if ((tags[TAGLO_IDX] & SSTATEMASK) == SINVALID) {
				if (ecc_check_correctable(datap,edp,ecc_info_param)==0) {

					ecc_assign_msg(ECC_INFO_MSG, index,
						ecc_sinvalid_noerr);
					ecc_log_error(NO_ERROR, index);
					return(0);
				}
				ecc_assign_msg(ECC_ERROR_MSG, index,
				       ecc_sinvalid_err);
				return(1);
			} else {
				ecc_assign_msg(ECC_ERROR_MSG, index,
				       ecc_ft_hinv_m_sc);
				return(1);
			}
		}
		err_info.eidata_lo = *datap;
		err_info.eidata_hi = *(datap+1);

		if (ecc_check_correctable(datap,edp,ecc_info_param) !=0 ) {

			/* didn't fix it: panic */

			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_sdcfix_failed);
#ifdef DEBUG_ECC
			f_staglo = tags[TAGLO_IDX];
			_c_ilt(CACH_PD, p_caddr, tags);
			f_ptaglo = tags[TAGLO_IDX];
			f_loval = err_info.eidata_lo;
			f_hival = err_info.eidata_hi;
			f_p_caddr = p_caddr;
			f_s_caddr = s_caddr;
#endif
			return(1);
		} else {
			ecc_assign_msg(ECC_INFO_MSG, index, ecc_sdcfix_good);
			return(0);
		}

	} else {  /* dirty line: can't invalidate line and refetch */

		/* Now the severity of the error becomes relevant.
		 * If it is a one bit error we can force the line 
		 * out to memory through the CC --which corrects 
		 * single-bit errors--then read it back and check
		 * if it is now correct.  Panic if not--probably
		 * a stuck bit.
		 */

		if (ecc_check_correctable(datap,edp,ecc_info_param) < 0) {
			eccfp[ECCF_PADDR] = edp->e_paddr;
			ecc_assign_msg(ECC_PANIC_MSG, index,
				ecc_md_sddfix_failed);
			return(1);
		}
		_c_hwbinv(CACH_SD, (__psunsigned_t)datap);

		/* now refetch the info and ensure that it is fixed */

		err_info.eidata_lo = *datap;
		err_info.eidata_hi = *(datap+1);

		if (ecc_check_correctable(datap,edp,ecc_info_param) != 0) {	/* didn't fix it: panic */
			ecc_assign_msg(ECC_PANIC_MSG, index, ecc_sddfix_failed);
#ifdef DEBUG_ECC
			f_staglo = tags[TAGLO_IDX];
			_c_ilt(CACH_PD, p_caddr, tags);
			f_ptaglo = tags[TAGLO_IDX];
			f_loval = err_info.eidata_lo;
			f_hival = err_info.eidata_hi;
			f_p_caddr = p_caddr;
			f_s_caddr = s_caddr;
#endif

			return(1);
		} else {
			ecc_assign_msg(ECC_INFO_MSG, index, ecc_sddfix_good);
			return(0);
		}

	} /* else dirty line */
#endif /* IP19 */


} /* ecc_fixcdata */


#if defined(_MEM_PARITY_WAR) || IP20 || IP22
pfn_t
allocate_ecc_info(pfn_t fpage)
{
	/*
	 * Allocate stack and log area for cache error exception handler
	 * in dedicated uncached pages.
	 */

#ifdef _MEM_PARITY_WAR
	bzero((void *)PHYS_TO_K1(ECCF_ADDR(0)),ECCF_SIZE);
	CACHE_ERR_STACK_BASE_P = PHYS_TO_K1((ctob(fpage)+CACHE_ERR_STACK_SIZE));
	fpage += btoc(CACHE_ERR_STACK_SIZE);
	CACHE_ERR_ECCINFO_P = PHYS_TO_K1(ctob(fpage));
	fpage += btoc((sizeof(ecc_info) + 
		       perr_mem_init(((caddr_t) CACHE_ERR_ECCINFO_P) +
				 sizeof(ecc_info))));
	init_ecc_info();
#else /* _MEM_PARITY_WAR */
	fpage += btoc(perr_mem_init((caddr_t) (PHYS_TO_K1(ctob(fpage)))));
#endif /* _MEM_PARITY_WAR */
	return(fpage);
}
#endif /* _MEM_PARITY_WAR */

#ifndef IP19
static void
init_ecc_info(void)
{
#ifdef _MEM_PARITY_WAR
	msg_addrs[ECC_PANIC_MSG] = (volatile char **)&ecc_info.ecc_panic_msg[0];
	msg_addrs[ECC_INFO_MSG] = (volatile char **)&ecc_info.ecc_info_msg[0];
	msg_addrs[ECC_ERROR_MSG] = (volatile char **)&ecc_info.ecc_error_msg[0];
#endif /* _MEM_PARITY_WAR */

	/* doing a bzero will cause both ecc_handler and
	 * ecc_cleanup/ecc_panic to skip the 0th slot 1st
	 * time around the circular buffer.  who cares. */
	bzero((void *)&ecc_info, sizeof(ecc_info));
#ifndef _MEM_PARITY_WAR
	ecc_info.eframep = CACHE_ERR_EFRAME;
	ecc_info.eccframep = CACHE_ERR_ECCFRAME;
#endif	/* _MEM_PARITY_WAR */
	ecc_info_initialized = 1;

#ifdef R4000PC
	r4000_config = get_r4k_config();
#endif /* R4000PC */
} /* init_ecc_info */
#endif /* !IP19 */

#ifdef IP19
static
real_ecc_assign_msg(
	int msg_type,
	int index,
	char msg,
	volatile ecc_info_t *ecc_info_param)
{
	switch(msg_type) {
	case ECC_PANIC_MSG:
		ecc_info_param->ecc_panic_msg[index] = msg;
		break;
	case ECC_INFO_MSG:
		ecc_info_param->ecc_info_msg[index] = msg;
		break;
	case ECC_ERROR_MSG:
		ecc_info_param->ecc_error_msg[index] = msg;
		break;
	default:
		;
	}
	MARK_FOR_CLEANUP;	/* msg queued ==> need ecc_cleanup */

	return(0);

} /* ecc_assign_msg */


static int
real_ecc_print_msg(
	int msg_type,	/* message-type to print; -1 for all msgs at 'index' */
	uint index,	/* message-array index to print */
	int clear_it,	/* if panic'ing don't clear msg--can use symmon */
	int disp_hdr,	/* if non-zero print message-type before msg */
	uint cpu,	/* cpuid of failing cpu */
	volatile ecc_info_t *ecc_info_param)
{
	char *ppc;
	char *nameptr, ppindex;
  	pfunc pptr = (pm_use_qprintf ? (pfunc)qprintf : printf);
	int i;

	switch (msg_type) {
	case ECC_ALL_MSGS: 
		for (i = ECC_PANIC_MSG; i <= ECC_ERROR_MSG; i++) 
			real_ecc_print_msg(i,index,clear_it,disp_hdr,cpu,ecc_info_param);
		return(0);

	case ECC_PANIC_MSG:
	case ECC_INFO_MSG:
	case ECC_ERROR_MSG:
		break;

	default:
		return(-1);
	}

	switch(msg_type) {
	case ECC_PANIC_MSG:
		ppindex = ecc_info_param->ecc_panic_msg[index];
		break;
	case ECC_INFO_MSG:
		ppindex = ecc_info_param->ecc_info_msg[index];
		break;
	case ECC_ERROR_MSG:
		ppindex = ecc_info_param->ecc_error_msg[index];
		break;
	default:
		ppindex = 0;
	}
	switch(ppindex) {
	case ecc_overrun_msg:
		ppc = real_ecc_overrun_msg;
		break;
	case ecc_eb_not_i:
		ppc = real_ecc_eb_not_i;
		break;
	case ecc_incons_err:
		ppc = real_ecc_incons_err;
		break;
	case ecc_ew_err:
		ppc = real_ecc_ew_err;
		break;
	case ecc_kernel_err:
		ppc = real_ecc_kernel_err;
		break;
	case ecc_user_err:
		ppc = real_ecc_user_err;
		break;
	case ecc_inval_loc:
		ppc = real_ecc_inval_loc;
		break;
	case ecc_no_ptagerr:
		ppc = real_ecc_no_ptagerr;
		break;
	case ecc_no_stagerr:
		ppc = real_ecc_no_stagerr;
		break;
	case ecc_ptfix_failed:
		ppc = real_ecc_ptfix_failed;
		break;
	case ecc_stfix_failed:
		ppc = real_ecc_stfix_failed;
		break;
	case ecc_no_pdataerr:
		ppc = real_ecc_no_pdataerr;
		break;
	case ecc_no_sdataerr:
		ppc = real_ecc_no_sdataerr;
		break;
	case ecc_sinvalid_noerr:
		ppc = real_ecc_sinvalid_noerr;
		break;
	case ecc_sinvalid_err:
		ppc = real_ecc_sinvalid_err;
		break;
	case ecc_sdcfix_failed:
		ppc = real_ecc_sdcfix_failed;
		break;
	case ecc_sdcfix_good:
		ppc = real_ecc_sdcfix_good;
		break;
	case ecc_sddfix_failed:
		ppc = real_ecc_sddfix_failed;
		break;
	case ecc_sddfix_good:
		ppc = real_ecc_sddfix_good;
		break;
	case ecc_md_sddfix_failed:
		ppc = real_ecc_md_sddfix_failed;
		break;
	case ecc_p_data_err:
		ppc = real_ecc_p_data_err;
		break;
	case ecc_inval_eloc:
		ppc = real_ecc_inval_eloc;
		break;
	case ecc_bad_s_tag:
		ppc = real_ecc_bad_s_tag;
		break;
	case ecc_ft_hinv_m_sc:
		ppc = real_ecc_ft_hinv_m_sc;
		break;
	case ecc_scerr_too_early:
		ppc = real_ecc_scerr_too_early;
		break;
	case ecc_ei_notdirty:
		ppc = real_ecc_ei_notdirty;
		break;
	case ecc_mixed_psize:
		ppc = real_ecc_mixed_psize;
		break;
	case ecc_ei_norecover:
		ppc = real_ecc_ei_norecover;
		break;
	case ecc_possible_ei:
		ppc = real_ecc_possible_ei;
		break;
	default:
		ppc = NULL;
	}
	nameptr = (char *)msg_strs[msg_type];
	if (ppc) {
		if (maxcpus > 1)
			pptr("CPU %d: ",cpu);
		pptr("    %s %s\n",(disp_hdr?nameptr : " "),ppc);
		if (clear_it) {
			switch(msg_type) {
			case ECC_PANIC_MSG:
				ecc_info_param->ecc_panic_msg[index] = 0;
				break;
			case ECC_INFO_MSG:
				ecc_info_param->ecc_info_msg[index] = 0;
				break;
			case ECC_ERROR_MSG:
				ecc_info_param->ecc_error_msg[index] = 0;
				break;
			default:
				;
			}
		}
	}

	return(0);
} /* ecc_print_msg */


#else /* !IP19 */

static
ecc_assign_msg(
	int msg_type,
	int index,
	char *msg)
{
	msg_addrs[msg_type][index] = msg;
	MARK_FOR_CLEANUP;	/* msg queued ==> need ecc_cleanup */

	return(0);

} /* ecc_assign_msg */

static int
ecc_print_msg(
	int msg_type,	/* message-type to print; -1 for all msgs at 'index' */
	uint index,	/* message-array index to print */
	int clear_it,	/* if panic'ing don't clear msg--can use symmon */
	int disp_hdr,	/* if non-zero print message-type before msg */
	uint cpu)	/* cpuid of failing cpu */
{
	char **ppc;
	char *nameptr;
  	pfunc pptr = (pm_use_qprintf ? (pfunc)qprintf : printf);
	int i;

	switch (msg_type) {
	case ECC_ALL_MSGS: 
		for (i = ECC_PANIC_MSG; i <= ECC_ERROR_MSG; i++) 
			ecc_print_msg(i,index,clear_it,disp_hdr,cpu);
		return(0);

	case ECC_PANIC_MSG:
	case ECC_INFO_MSG:
	case ECC_ERROR_MSG:
		break;

	default:
		return(-1);
	}

	ppc = (char **)msg_addrs[msg_type];

	nameptr = (char *)msg_strs[msg_type];
	if (ppc[index]) {
#if MP
		if (maxcpus > 1)
			pptr("CPU %d: ",cpu);
#endif
		pptr("    %s %s\n",(disp_hdr?nameptr : " "),ppc[index]);
		if (clear_it)
			ppc[index] = NULL;
	}

	return(0);
} /* ecc_print_msg */

#endif /* !IP19 */




#define PTAG_PARITY_BIT		0x1	/* ptaglo parity bit is #0 */
#define PTAG_1ST_DATA_BIT	6	/* low 6 bits are undefined + parity */
#define PTAG_PTAGLO_BITS	24
#define PTAG_PSTATE_BITS	2
#define PTAG_DATA_BITS		(PTAG_PTAGLO_BITS+PTAG_PTAG_PSTATE_BITS)

/* ecc_bad_ptag(taglo):  Determine if the ecc/parity in 'taglo' is
 * correct.  Calculate the even parity for the 26-bit field (5 undefined
 * bits plus low bit == parity bit).  Return 0 if parity is correct, else 1.
 */
static
ecc_bad_ptag(uint taglo)
{
	uint bit = (0x1 << PTAG_1ST_DATA_BIT);
	int numsetbits = 0;
	uint pbit = 0;
	int i;
		
	for (i = PTAG_1ST_DATA_BIT; i < BITSPERWORD; bit <<= 1, i++) {
		if (taglo & bit)
			numsetbits++;
	}
	if (numsetbits & 0x1)	/* odd # of bits; set p to even it */
		pbit = 1;

	if ((taglo & 0x1) == pbit)
		return(0);	/* computed parity matches ptaglo's */
	else
		return(1);

} /* ecc_bad_ptag */



#if IP19
static
real_ecc_log_error(int where, int index, volatile ecc_info_t *ecc_info_param)
#else
volatile int inval_eloc = 0;

static
ecc_log_error(int where, int index)
#endif
{

	if (where < 0 || where >= ECC_ERR_TYPES) {
		ecc_assign_msg(ECC_ERROR_MSG, index, ecc_inval_eloc);
#if IP19
		/* avoid global references which generate cached accesses */

		ecc_info_param->ecc_inval_eloc_where = where;
#else
		inval_eloc = where;
#endif
		return(1);
	}

	ECC_INFO(ecc_err_cnts)[where]++;

	return(0);

} /* ecc_log_error */


/* First set of trees and structs are for computing the 8 checkbits
 * that accompany each set of double-words in memory and secondary
 * cache: i.e. the data trees */
#define ECC8B_DTREE7H 0xff280ff0
#define ECC8B_DTREE7L 0x88880928

#define ECC8B_DTREE6H 0xfa24000f
#define ECC8B_DTREE6L 0x4444ff24

#define ECC8B_DTREE5H 0x0b22ff00
#define ECC8B_DTREE5L 0x2222fa32

#define ECC8B_DTREE4H 0x0931f0ff
#define ECC8B_DTREE4L 0x11110b21

#define ECC8B_DTREE3H 0x84d08888
#define ECC8B_DTREE3L 0xff0f8c50

#define ECC8B_DTREE2H 0x4c9f4444
#define ECC8B_DTREE2L 0x00ff44d0

#define ECC8B_DTREE1H 0x24ff2222
#define ECC8B_DTREE1L 0xf000249f

#define ECC8B_DTREE0H 0x14501111
#define ECC8B_DTREE0L 0x0ff014ff

struct d_emask {
	uint d_emaskhi;
	uint d_emasklo;
};

struct d_emask d_ptrees[] = {
	{ ECC8B_DTREE0H, ECC8B_DTREE0L },
	{ ECC8B_DTREE1H, ECC8B_DTREE1L },
	{ ECC8B_DTREE2H, ECC8B_DTREE2L },
	{ ECC8B_DTREE3H, ECC8B_DTREE3L },
	{ ECC8B_DTREE4H, ECC8B_DTREE4L },
	{ ECC8B_DTREE5H, ECC8B_DTREE5L },
	{ ECC8B_DTREE6H, ECC8B_DTREE6L },
	{ ECC8B_DTREE7H, ECC8B_DTREE7L },
};


/* Next, the data necessary for computing the 7 checkbits 
 * for the 25-bit secondary cache tags: i.e. the tag trees. */

#define ECC7B_TTREE6 0x0a8f888
#define ECC7B_TTREE5 0x114ff04
#define ECC7B_TTREE4 0x2620f42
#define ECC7B_TTREE3 0x29184f0
#define ECC7B_TTREE2 0x10a40ff
#define ECC7B_TTREE1 0x245222f
#define ECC7B_TTREE0 0x1ff1111

struct t_emask {
	uint t_emask;
};

struct t_emask t_ptrees[] = {
	 ECC7B_TTREE0,
	 ECC7B_TTREE1,
	 ECC7B_TTREE2,
	 ECC7B_TTREE3,
	 ECC7B_TTREE4,
	 ECC7B_TTREE5,
	 ECC7B_TTREE6,
};


/* 2ndary cache tags consist of 25 data bits monitored by 7 checkbits */
#define STAG_DBIT_SIZE	25
#define STAG_CBIT_SIZE	7
#define STAG_SIZE	(STAG_DBIT_SIZE+STAG_CBIT_SIZE)

/* S_taglo field format:
 *	bitpositions-->    31..13  12..10  9..7  6..0
 *	fields --> 	 < p_addr, cstate, vind, ecc >.
 * Internal format:
 *			   31..25  24..22  21..19 18..0 
 *			 <   ecc,  cstate,  vind, p_addr >. 
 * the following defines tell ecc_swap_s_tag() how to shift the fields to
 * create the internal format from the s_taglo format. 
 */
/* sizes of the fields */
#define S_TAG_PADDR_BITS	19
#define S_TAG_CS_BITS		3
#define S_TAG_VIND_BITS		3
#define S_TAG_ECC_CBITS		7

/* bit positions of the fields in the s_taglo format */
#define S_TAG_ECC_BITPOS	0
#define S_TAG_VIND_BITPOS	(S_TAG_ECC_BITPOS+S_TAG_ECC_CBITS)	/*  7 */
#define S_TAG_CS_BITPOS		(S_TAG_VIND_BITPOS+S_TAG_VIND_BITS)	/* 10 */
#define S_TAG_PADDR_BITPOS	(S_TAG_CS_BITPOS+S_TAG_CS_BITS)		/* 13 */

/* bit positions of the fields in the internal format */
#define S_INT_PADDR_BITPOS	0
#define S_INT_VIND_BITPOS	(S_INT_PADDR_BITPOS+S_TAG_PADDR_BITS)	/* 19 */
#define S_INT_CS_BITPOS		(S_INT_VIND_BITPOS+S_TAG_VIND_BITS)	/* 22 */
#define S_INT_ECC_BITPOS	(S_INT_CS_BITPOS+S_TAG_CS_BITS)		/* 25 */

/* masks for the four fields in the 2ndary-cache-internal format:
 * < ecc, cstate, vindex, p_addr > */
#define S_INT_PADDR_MASK	0x0007ffff
#define S_INT_VIND_MASK		0x00380000
#define S_INT_CS_MASK		0x01c00000
#define S_INT_ECC_MASK		0xfe000000


/* Below macros enable easy swapping of each of the 4 2ndary tag fields.
 * Note that the mask used by the conversion macros in extracting the
 * bits of the field depends on the direction of the swap */

/* paddr: tag bit 13 <--> internal bit 0 */
#define SADDR_SWAP_ROLL		(S_TAG_PADDR_BITPOS-S_INT_PADDR_BITPOS)/* 13 */
/* the saddr conversion rolls opposite from the other 3 fields: TagTOInternal
 * rolls addr DOWN to bottom */
#define SADDR_TTOI(S_TAG)	((S_TAG & SADDRMASK) >> SADDR_SWAP_ROLL)
#define SADDR_ITOT(S_TAG)	((S_TAG & S_INT_PADDR_MASK) << SADDR_SWAP_ROLL)

/* cache state: tag bit 10 <--> internal bit 22 */
#define SSTATE_SWAP_ROLL	(S_INT_CS_BITPOS-S_TAG_CS_BITPOS)    /*  12 */
#define SSTATE_TTOI(S_TAG)	((S_TAG & SSTATEMASK) << SSTATE_SWAP_ROLL)
#define SSTATE_ITOT(S_TAG)	((S_TAG & S_INT_CS_MASK) >> SSTATE_SWAP_ROLL)

/* vindex: tag bit 7 <--> internal bit 19 */
#define SVIND_SWAP_ROLL		(S_INT_VIND_BITPOS-S_TAG_VIND_BITPOS) /* 12 */
#define SVIND_TTOI(S_TAG)	((S_TAG & SVINDEXMASK) << SVIND_SWAP_ROLL)
#define SVIND_ITOT(S_TAG)	((S_TAG & S_INT_VIND_MASK) >> SVIND_SWAP_ROLL)

/* ecc: tag bit 0 <--> internal bit 25 */
#define SECC_SWAP_ROLL		(S_INT_ECC_BITPOS-S_TAG_ECC_BITPOS)  /* 25 */
#define SECC_TTOI(S_TAG)	((S_TAG & SECC_MASK) << SECC_SWAP_ROLL)
#define SECC_ITOT(S_TAG)	((S_TAG & S_INT_ECC_MASK) >> SECC_SWAP_ROLL)

/* ecc_swap_s_tag() converts between the field-ordering of the taglo
 * register (holding a 2ndary cache tag) and the internal format
 * actually used in the secondary caches.  The conversion may be
 * done in either direction.  The routine requires the ctag_swap_info
 * structure */
#define TAG_TO_INTERNAL	1
#define INTERNAL_TO_TAG	2

typedef struct tag_swap_info {
	uint ts_in_val;	/* value to be swapped */
	uint ts_out_32;	/* INTERNAL_TO_TAG sets 32-bit val (including cbits) */
	uint ts_out_25;	/* TAG_TO_INTERNAl sets 25-bit val (excluding cbits) */
	uint ts_cbits;	/* both directions set this field */
} tag_swap_info_t;

int ecc_swap_s_tag(uint, tag_swap_info_t *);


/* tag_dbpos is a lookup-table which translates the bit-positions of data
 * errors as indicated by syndromes to their counterparts in the taglo format.
 * Internally the low 19 bits contain the paddr; in taglo the paddr field
 * begins at 13.  The next 6 bits internally contain the vindex and state
 * fields; in the tag reg these are ordered the same but begin at bit 7.
 */
uint tag_dbpos[] = {
/* paddr --> */	       13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
/* vind & state--> */  7, 8, 9, 10,11,12 };

/* computes relevant info about ECC errors, and returns it in an
 * error_info_t struct.  ecc_type is DATA_CBITS or TAG_CBITS and
 * determines whether calc_err_info will compute the 8-bit checkbit
 * and syndrome of two data-words, or the 7-bit info for a 25-bit
 * second-level tag.
 */
#ifdef IP19
real_calc_err_info(int ecc_type, error_info_t *e_infop,
		   volatile ecc_info_t *ecc_info_param)
#else
calc_err_info(int ecc_type, error_info_t *e_infop)
#endif
{
	uint shi, slo;
	uint true_val = 0;
	register int pbithi, pbitlo, pbit;
	register int i;
	register int j;
	struct d_emask *dep;
	struct t_emask *tep;
	unchar cbits = 0;
	uint lo_in, hi_in;
	tag_swap_info_t swap_info;

	if (ecc_type == DATA_CBITS) {
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
		lo_in = e_infop->eidata_hi;
		hi_in = e_infop->eidata_lo;
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
#if IP19
		dep = &ecc_info_param->ecc_d_ptrees[0];
#else
		dep = &d_ptrees[0];
#endif
		for (i = 0; i < 8;  i++, dep++) {
			shi = hi_in & dep->d_emaskhi;
			slo = lo_in & dep->d_emasklo;
			pbithi = 0;
			pbitlo = 0;
	
			for (j = 0; j < 32; j++) {
				if (shi & (1 << j))
					pbithi++;
				if (slo & (1 << j))
					pbitlo++;
			}
			if ((pbithi + pbitlo) & 1)
				cbits |= 1 << i;
		}
		e_infop->cbits_out = cbits;
		e_infop->syndrome = e_infop->cbits_in ^ cbits;
		e_infop->syn_info = data_eccsyns[(int)e_infop->syndrome];
		return(e_infop->syndrome);
	
	} else if (ecc_type == TAG_CBITS) {
		/* Internally the R4k stores the fields comprising
		 * secondary tags differently than the format it
		 * uses for s_ptaglo.  The cache format is:
		 * <ECC, CS, Vidx, Stag>; the STaglo format is:
		 * <STag, CS, Vidx, ECC>.  The ECC is computed and
		 * checked with the fields arranged as they are
		 * internally; therefore we must swap them before
		 * computing.  TAG_TO_INTERNAL sets ts_out_25 to
		 * the 25-bit data value and ts_cbits to the 7-bit
		 * ecc field from the tag.
		 */
		swap_info.ts_in_val = e_infop->eis_taglo;
		ecc_swap_s_tag(TAG_TO_INTERNAL, &swap_info);
		true_val = swap_info.ts_out_25;
#ifdef ECC_DEBUG
		printf("after tag swap, value 0x%x, cbits 0x%x\n",true_val,
			swap_info.ts_cbits);
#endif
		/* if caller set high bit (never valid in 7-bit cbit field),
		 * use the cbits in the taglo as cbits_in for xor; else
		 * it holds the cbits for the bad tag */
		if (SET_CBITS_IN & e_infop->cbits_in)
			e_infop->cbits_in = (unchar)swap_info.ts_cbits;

#if IP19
		tep = &ecc_info_param->ecc_t_ptrees[0];
#else
		tep = &t_ptrees[0];
#endif
		for (i = 0; i < 7;  i++, tep++) {
			shi = true_val & tep->t_emask;
			pbit = 0;
	
			for (j = 0; j < 25; j++) {
				if (shi & (1 << j))
					pbit++;
			}
			if (pbit & 1)
				cbits |= 1 << i;
		}
		e_infop->cbits_out = cbits;
		e_infop->syndrome = e_infop->cbits_in ^ cbits;
 		e_infop->syn_info = tag_eccsyns[(int)e_infop->syndrome];
		return(e_infop->syndrome);

	} else
		return(0x80000000);

} /* calc_err_info */


/* the format of the taglo register when it contains a 2ndary tag is 
 * <paddr, state, vindex, ecc>.  Internally the fields are ordered
 * <ecc, state, vindex, paddr>, and the checkbits are generated and
 * checked with the fields ordered in the internal format.
 * ecc_swap_s_tag() converts the fields into either format.
 */
ecc_swap_s_tag(
	uint which_way,
	tag_swap_info_t *swap_infop)
{
	uint swapped_val;
	uint in_val = swap_infop->ts_in_val;

	switch (which_way) {
	case TAG_TO_INTERNAL:	/* swap, then set 25-bit value and cbits */
		swapped_val = SADDR_TTOI(in_val);
		swapped_val |= SSTATE_TTOI(in_val);
		swapped_val |=  SVIND_TTOI(in_val);
		swap_infop->ts_out_25 = swapped_val;
		swap_infop->ts_out_32 = (swapped_val | SECC_TTOI(in_val));
		/* low 7 bits of TagLo reg are checkbits */
		swap_infop->ts_cbits = (in_val & SECC_MASK);
		return(0);

	case INTERNAL_TO_TAG:	/* set entire 32-bits plus cbits */
		swapped_val = SADDR_ITOT(in_val);
		swapped_val |= SSTATE_ITOT(in_val);
		swapped_val |=  SVIND_ITOT(in_val);
		swapped_val |=  SECC_ITOT(in_val);
		swap_infop->ts_out_32 = swapped_val;
		/* high 7 bits of internal format are checkbits */
		swap_infop->ts_cbits = SECC_ITOT(in_val);
		return(0);

	default:
		printf("ecc_swap_s_tag: illegal direction (%d)\n",which_way);
		return(-1);
	}

} /* ecc_swap_s_tag */


/* given a single-bit error type (CB or DB) and the bit position of that 
 * error in the R4K's internal format (i.e. as it is stored in the 2ndary
 * cache), xlate_bit returns the bitposition of its counterpart in the
 * taglo format. */
#if IP19
real_xlate_bit(enum error_type etype, uint bitpos,
	       volatile ecc_info_t *ecc_info_param)
#else
xlate_bit(enum error_type etype, uint bitpos)
#endif
{
	/* ecc field is 6..0 in taglo; the syndrome differentiates between
	 * data and checkbit errors, (numbering them 0..24 and 0..6 resp.)
	 * so no translation is necessary for cbit errors. */
	if (etype == CB) {
		ASSERT(bitpos < STAG_CBIT_SIZE);
		return(bitpos);
	} else {
		ASSERT(bitpos < STAG_DBIT_SIZE);
#if IP19
		return(ecc_info_param->ecc_tag_dbpos[bitpos]);
#else
		return(tag_dbpos[bitpos]);
#endif
	}

} /* xlate_bit */

#ifdef IP19
void
ip19_init_ecc_info( __psunsigned_t vceaddr )
{
	ecc_info_ptr.ecc_vcecolor = vceaddr;
	/* these fields are only necessary because the compiler tends to
	 * generate the constants needed and place them in a globally
	 * addressed location (either K0 or off of "gp"), so loading the
	 * constants involves cached accesses.  So we perform the
	 * conversions once and just load the (uncached) pointer from
	 * the ecc_info array which is accessed uncached too.
	 */
	ecc_info_ptr.everror_ext = EVERROR_EXT;

	/* Following global structures need to be reference uncached by
	 * ecc_handler and friends.
	 */
	ecc_info_ptr.ecc_tag_dbpos = (uint *)K0_TO_K1(&tag_dbpos);
	ecc_info_ptr.ecc_d_ptrees = (struct d_emask *)K0_TO_K1(&d_ptrees);
	ecc_info_ptr.ecc_t_ptrees = (struct t_emask *)K0_TO_K1(&t_ptrees);
	ecc_info_ptr.ecc_data_eccsyns = (eccdesc_t*)K0_TO_K1(&real_data_eccsyns);
	ecc_info_ptr.ecc_tag_eccsyns = (eccdesc_t*)K0_TO_K1(&real_tag_eccsyns);

	ecc_info_ptr.ecc_k0size_less1 = K0SIZE-1;
	ecc_info_ptr.ecc_physmem = physmem;
	ecc_info_ptr.ecc_picache_size = picache_size;
	ecc_info_ptr.ecc_pdcache_size = pdcache_size;

	ecc_info_ptr.ecc_attempt_recovery = 0;
#ifndef IP19_CACHEERRS_FATAL
	{
	extern int r4k_corrupt_scache_data;
	if (r4k_corrupt_scache_data)
		ecc_info_ptr.ecc_attempt_recovery = 1;
	}
#endif

	/* Following address should be cached for test to work properly */

	ecc_info_ptr.ecc_dummyline =
		((__psunsigned_t)(&dummy_cacheline[16]) & ~(SCACHE_LINESIZE-1));

	ecc_info_ptr.ecc_info_inited = 1;
}

#endif /* IP19 */

#define NUM_CE_BITS	8

#define SIDX_VAL(x)	(x & CACHERR_SIDX_MASK)
#define PIDX_VAL(x)	((x & CACHERR_PIDX_MASK) << 12)

#define CEBUFSIZ	180


/* if sindex == -1, print all frames from read ptr to write ptr;
 * else just the specified frame */
void
print_ecc_info(sindex,eindex)
  int sindex;
  int eindex;
{
	ecc_info_t *eip = (ecc_info_t *)&ecc_info_ptr;
	err_desc_t *edp;	/* ptr to set of variables to set this time */
	__uint64_t eaddr;
	int i, loc;

	if (sindex == -1) {
		sindex = eip->ecc_r_index;
		eindex = eip->ecc_w_index;
	}
	if (sindex < 0) sindex = 0;
	if (eindex < 0) eindex = 0;
	if (sindex >= ECC_FRAMES) sindex = ECC_FRAMES-1;
	if (eindex >= ECC_FRAMES) eindex = ECC_FRAMES-1;
	if (eindex < sindex) eindex = sindex;

	if (sindex != eindex)
		qprintf("\necc_info for slots %d through %d\n",sindex,eindex);
	else
		qprintf("\necc_info for slot %d\n",sindex);
#ifndef _MEM_PARITY_WAR
	qprintf("  efptr 0x%x  eccfptr 0x%x, ", 
		eip->eframep, eip->eccframep);
#endif	/* _MEM_PARITY_WAR */
	qprintf("  w_ind %d  r_ind %d  clean %d  c_cnt %d  flags 0x%x\n",
		eip->ecc_w_index, eip->ecc_r_index, eip->needs_cleanup,
		eip->cleanup_cnt, eip->ecc_flags);

	qprintf("  err cnts: ");
	for (i = 0; i < ECC_ERR_TYPES; i++ )
		qprintf("%s %d  ",err_type_names[i],
			ecc_info_ptr.ecc_err_cnts[i]);
	qprintf("\n\n");

	for (i = sindex; i <= eindex; i++) {
		edp = (err_desc_t *)&(ecc_info_ptr.desc[i]);
		if (!edp->e_cache_err) {
			qprintf("SLOT #%d empty\n",i);
			continue;
		}
		qprintf("SLOT #%d:\n",i);
		pm_use_qprintf = 1;
#if IP19
		real_ecc_print_msg(ECC_ALL_MSGS, i, 0, 1, edp->e_cpuid,
				   &ecc_info_ptr);
#else
		ecc_print_msg(ECC_ALL_MSGS, i, 0, 1, edp->e_cpuid);
#endif
		pm_use_qprintf = 0;

		eaddr = edp->e_paddr;
		loc = edp->e_location;

		if (loc < 0 || loc > SYSAD)
			loc = BAD_LOC;
		qprintf("    %s (%d) %s (%d) error:\n",
			error_loc_names[loc], edp->e_location,
			(edp->e_tag_or_data == DATA_ERR ? "data" : "tag"),
			 edp->e_tag_or_data);
		qprintf("    sr %R\n",edp->e_sr,
#if R4000 && R10000
			IS_R10000() ? r10k_sr_desc :
#endif
				sr_desc);
		qprintf("    cache_err %R, epc 0x%x\n",
			edp->e_cache_err, cache_err_desc, edp->e_error_epc);
		qprintf("    S-taglo %R%sP-taglo %R\n", edp->e_s_taglo,
#if R4000 && R10000
			IS_R10000() ? r10k_s_taglo_desc :
#endif /* R4000 && R10000 */
			s_taglo_desc, (edp->e_p_taglo ? "\n    " : " "),
			edp->e_p_taglo, p_taglo_desc);
		qprintf("    paddr %llx vaddr %x syn 0x%x user %d pid %d\n",
			edp->e_paddr, edp->e_vaddr, edp->e_syndrome,
			edp->e_user, (__psint_t)edp->e_pid);
#ifdef _MEM_PARITY_WAR
		qprintf("    efptr 0x%x  eccfptr 0x%x, ", 
			(__psunsigned_t)edp->e_eframep,
			(__psunsigned_t)edp->e_eccframep);
#endif	/* _MEM_PARITY_WAR */

		if (edp->e_prevbadecc)
			qprintf("    prevbadecc %x ",edp->e_prevbadecc);
		if (edp->e_2nd_syn)
			qprintf("    2nd_syn %x\n",edp->e_2nd_syn);
		else
			qprintf("\n");

		if (edp->e_tag_or_data == DATA_ERR)
		    qprintf("    lo_val 0x%x hi_val 0x%x badecc %x syn 0x%x\n",
				edp->e_lo_badval, edp->e_hi_badval, 
				edp->e_badecc, edp->e_syndrome);
		else if (edp->e_location==CACH_SI || edp->e_location==CACH_SD)
			/* secondary tag: print ecc, syndrome and staglo */
			qprintf("    S_Tag %R badecc 0x%x, syn 0x%x, addr %llx\n",
				edp->e_s_taglo,
#if R4000 && R10000
			IS_R10000() ? r10k_s_taglo_desc :
#endif /* R4000 && R10000 */
				s_taglo_desc, edp->e_badecc,
				edp->e_syndrome, edp->e_paddr);
		else if (edp->e_location==CACH_PI || edp->e_location==CACH_PD)
			/* primary tag: print p_taglo */
			qprintf("    PTagLo %R, Vaddr 0x%x\n",
				edp->e_p_taglo,p_taglo_desc,edp->e_vaddr);

		if (edp->e_location == CACH_PI || edp->e_location == CACH_PD)
			eaddr = edp->e_vaddr;
		pm_use_qprintf = 1;
		print_ecctype(edp->e_location, edp->e_tag_or_data,
			edp->e_syndrome, eaddr, 1, edp->e_cpuid);
		pm_use_qprintf = 0;
	}

#if DEBUG_ECC
	if (f_s_caddr) {
		qprintf("    f_ vars:\n    lov 0x%x hiv 0x%x pcad %x scad %x\n",
			f_loval, f_hival, f_p_caddr, f_s_caddr);
		qprintf("    P-lo %R%sS-lo %R\n",
			f_ptaglo,p_taglo_desc,
			(f_ptaglo ? "\n    " : "  "),
			f_staglo,
#if R4000 && R10000
			IS_R10000() ? r10k_s_taglo_desc :
#endif /* R4000 && R10000 */
				s_taglo_desc);
		qprintf("    cooked 0x%x, f_d_ecc 0x%x\n",f_cooked_ecc,f_d_ecc);
		qprintf("    P-lo1 %R%sS-lo1 %R\n",
			f_ptaglo1,p_taglo_desc,
			(f_ptaglo1 ? "\n    " : "  "),
			f_staglo1,
#if R4000 && R10000
			IS_R10000() ? r10k_s_taglo_desc :
#endif /* R4000 && R10000 */
				 s_taglo_desc);
	}
#endif /* DBEUG_ECC */


} /* print_ecc_info */


void
idbg_ecc_info(void)
{
	register int i;

	qprintf(" err cnts:\n   ");
	for (i = 0; i < ECC_ERR_TYPES; i++ )
#if IP19
		qprintf("%s %d  ",err_type_names[i],ecc_info_ptr.ecc_err_cnts[i]);
#else
		qprintf("%s %d  ",err_type_names[i],ecc_info.ecc_err_cnts[i]);
#endif
	qprintf("\n\n");
}

static int
print_ecctype(
  int loc,
  int ecc_type,
  uint syndrome,
  __uint64_t eaddr,
  int printerr,
  uint cpu)
{
	eccdesc_t syn_info, *syntab_ptr;
	uint es_tsize;
	pfunc pptr = (pm_use_qprintf ? (pfunc)qprintf : printf);

	if (ecc_type == D_AND_T_ERR)	/* ecc info will be on the tag error */
		ecc_type = TAG_CBITS;

	if (loc < 0 || loc > SYSAD)
		loc = BAD_LOC;
	
	if (ecc_type == TAG_CBITS) {
		es_tsize = ECCSYN_TABSIZE(real_tag_eccsyns);
#if IP19
		/* It's safe to use the ecc_info_ptr since this routine is
		 * invoked from ecc_cleanup so it's safe to perform the
		 * 'gp' relative accesses the compiler generates in the
		 * K0_TO_K1 macro expansion. Note that referencing the
		 * tag_eccsyns array is uncached.
		 */
		syntab_ptr = ecc_info_ptr.ecc_tag_eccsyns;
#else
		syntab_ptr = tag_eccsyns;
#endif
	} else {
		es_tsize = ECCSYN_TABSIZE(real_data_eccsyns);
#if IP19
		/* It's safe to use the ecc_info_ptr since this routine is
		 * invoked from ecc_cleanup so it's safe to perform the
		 * 'gp' relative accesses the compiler generates in the
		 * K0_TO_K1 macro expansion. Note that referencing the
		 * data_eccsyns array is uncached.
		 */
		syntab_ptr = ecc_info_ptr.ecc_data_eccsyns;
#else
		syntab_ptr = data_eccsyns;
#endif
	}
	if (syndrome >= es_tsize) {
		if (printerr) {
#if MP
			if (maxcpus > 1)
				pptr("CPU %d: ",cpu);
#endif
			pptr("print_ecctype(): invalid %s syndrome (%d)\n",
			    (ecc_type == TAG_CBITS ? "tag" : "data"),es_tsize);
		}
		return(-1);
	}
	syn_info = syntab_ptr[syndrome];

#ifdef ECC_DEBUG
#if MP
	if (maxcpus > 1)
		pptr("CPU %d: ",cpu);
#endif
	pptr("syndrome 0x%x, type 0x%x, value 0x%x\n",syndrome,
		syn_info.type, syn_info.value);
#endif /* ECC_DEBUG */

#if MP
	if (maxcpus > 1)
		pptr("CPU %d: ",cpu);
#endif
	pptr("    %s: ", error_loc_names[loc]);

	switch (syn_info.type) {
	case OK:
#ifdef IP19
		pptr("Syndrome at addr 0x%llx normal! Error in evicted line handled by CC\n",eaddr);
#else
		pptr("??!?!Syndrome at addr 0x%llx normal!\n",eaddr);
#endif
		return(-2);

	case UN:
	case B2:
	case B3:
		if (ecc_type == TAG_CBITS)
			pptr("%s TAG error in secondary cache at addr 0x%llx\n",
				etstrings[syn_info.type],eaddr);
		else
			pptr("%s DATA error in doubleword at addr 0x%llx\n",
				etstrings[syn_info.type],eaddr);
		return(0);

	case DB:
	case CB:
		if (ecc_type == TAG_CBITS)
			pptr("One-bit (%s%d) TAG err; 2nd cache: addr 0x%llx\n",
				etstrings[syn_info.type],syn_info.value,eaddr);
		else
			pptr("One-bit (%s%d) DATA err: dbl-word addr 0x%llx\n",
				etstrings[syn_info.type],syn_info.value,eaddr);
		return(0);

	default:
		if (printerr)
			pptr("Unknown eccdesc_t type (%d)\n",syn_info.type);
		return(-1);
	}

} /* print_ecctype */
#endif /* R4000 */

#if (defined(R4000) && defined(_FORCE_ECC))

/* each double-word in memory has an 8-bit ECC checkbit value that
 * is computed and stored with it. */
typedef struct ecc_data_word {
    uint hi_word;
    uint lo_word;
    u_char ecc_val;
} ecc_data_word_t;

#define	IN_PD	0
#define	IN_PI	1
#define	IN_SD	2
#define	IN_SI	3
#define	IN_MEM	4

volatile int force_verbose = 0;
volatile int missed_2nd = 0;
volatile uint v_orig_ecc, n_ecc, xor_ecc;
volatile uint used_sr = -1;

extern void uncached(void);
extern void setecc(int);
extern void runcached(void);


/* 'force_ecc_where' enum and 'ecc_data_word_t' typedef in sys/syssgi.h */
int
_force_ecc(inwhat, k1addr, ecc_info_param)
	int inwhat;	/* IN_{PD,PI,SD,SI,MEM, or IO3 (MEM via IO3)} */
	__psunsigned_t k1addr;	/* force ecc error at this K1SEG address */
	ecc_data_word_t *ecc_info_param;
{
	ecc_data_word_t new_ecc;	
	__psunsigned_t k0addr;
	__psunsigned_t physaddr;
	volatile int *k1ptr;
	volatile int *k0ptr;
	volatile int k0oneoff;
	__psunsigned_t pmem = (physmem * NBPP);
	uint tags[NUM_TAGS];
	char *cptr;
	uint no_ints_sr, ce_no_ints_sr, oldsr, oneoffval;
	uint orig_ecc;
	uint lo_val, hi_val;

	k1addr &= ~(BYTESPERDBLWD-1);	/* rnd down to a dbl-word boundry */

	k0addr = K1_TO_K0(k1addr);
	physaddr = K1_TO_PHYS(k1addr);
	k1ptr = (volatile int *)k1addr;
	k0ptr = (volatile int *)k0addr;

	if (copyin((caddr_t)ecc_info_param,(caddr_t)&new_ecc,
		sizeof(ecc_data_word_t))) {
		return EFAULT;
	}

	if (force_verbose)
		printf("What %d: k1 0x%x k0 0x%x hi 0x%x lo 0x%x cbits 0x%x\n",
			inwhat,k1addr,k0addr,new_ecc.hi_word,
			new_ecc.lo_word,(uint)new_ecc.ecc_val);

    switch(inwhat) {


    case IN_MEM:	/* force ecc error in memory via cache-munge */
    case IN_SD:
	if (inwhat == IN_SD) {
		cptr = error_loc_names[CACH_SD];
		if (force_verbose)
			printf("   Force %s ecc error (%d)\n",cptr,inwhat);
	} else {
		cptr = error_loc_names[SYSAD];
		if (force_verbose)
		    printf("   Force %s ecc err (%d) via secondary cbit-munge\n",
			cptr,inwhat);
	}

	if ((physaddr + private.p_scachesize) >= pmem)
		k0oneoff = (k0addr - private.p_scachesize);
	else
		k0oneoff = (k0addr + private.p_scachesize);

	oldsr = no_ints_sr = getsr();
	/* disable interrupts for entire time */
	no_ints_sr &= ~SR_IE;
	/* next sr will allow us to 'cook' the 2ndary ecc */
	ce_no_ints_sr = (no_ints_sr | SR_CE);

	setsr(no_ints_sr);	/* no ints while running uncached */
	uncached();  /* uncached instr stream: line won't be replaced */

	/* get valid cache line and write the specified dbl-word */
	*k0ptr = new_ecc.lo_word;
	*(k0ptr+1) = new_ecc.hi_word;
	/* force it into 2ndary to init correct ecc */
	_c_hwbinv(CACH_PD, k0addr);

	/* and read it back into primary */
	lo_val = *k0ptr;
	hi_val = *(k0ptr+1);
	/* now make it dirty again, with the same data so the ecc in
	 * the 2ndary is correct until we xor in the change; the
	 * pd_hwbinv will hit since it is again dirty */
	*k0ptr = lo_val;
	_c_ilt_n_ecc(CACH_SD, k0addr,tags, &orig_ecc);

	/* with SR_CE bit set, the ecc reg contributes to the generated
	 * value.  Contrary to the current documentation (which says that
	 * the ecc register is xor'ed into the existing checkbits), the
	 * R4K appears to first do a one's complement on the ECC register;
	 * THEN it's xor'ed into the cbits.  Therefore, for us to end up
	 * with the specified cbits we must xor the old and new, then
	 * NOT it.  Nice documentation... */
	v_orig_ecc = orig_ecc;	
	n_ecc = (uint)new_ecc.ecc_val; 
	xor_ecc = ~(orig_ecc ^ n_ecc);

	setecc((int)xor_ecc);

	_munge_decc(k0addr, ce_no_ints_sr);
#ifdef ECC_TEST_TWO_BAD
	/* corrupt a second word so we can test EW bit in ecc_handler */
	/* Assumes that we're corrupting 0x300 first, and second error is
	 * at 0x500 so you better back sure that's OK !
	 */

	ecc_info_ptr.ecc_err2_ptr = (k0ptr+128);	/* add 4 cachelines */

	/* get valid cache line and write the specified dbl-word */
	*(k0ptr+128) = new_ecc.lo_word;
	*(k0ptr+129) = new_ecc.hi_word;
	/* force it into 2ndary to init correct ecc */
	_c_hwbinv(CACH_PD, (k0addr+128*sizeof(int)));

	/* and read it back into primary */
	lo_val = *(k0ptr+128);
	hi_val = *(k0ptr+129);
	/* now make it dirty again, with the same data so the ecc in
	 * the 2ndary is correct until we xor in the change; the
	 * pd_hwbinv will hit since it is again dirty */
	*(k0ptr+128) = lo_val;
	_c_ilt_n_ecc(CACH_SD, (k0addr+128*sizeof(int)),tags, &orig_ecc);

	/* with SR_CE bit set, the ecc reg contributes to the generated
	 * value.  Contrary to the current documentation (which says that
	 * the ecc register is xor'ed into the existing checkbits), the
	 * R4K appears to first do a one's complement on the ECC register;
	 * THEN it's xor'ed into the cbits.  Therefore, for us to end up
	 * with the specified cbits we must xor the old and new, then
	 * NOT it.  Nice documentation... */
	v_orig_ecc = orig_ecc;	
	n_ecc = (uint)new_ecc.ecc_val; 
	xor_ecc = ~(orig_ecc ^ n_ecc);

	setecc((int)xor_ecc);

	_munge_decc(k0addr+128*sizeof(int), ce_no_ints_sr);

#endif /* ECC_TEST_TWO_BAD */
	setsr(no_ints_sr);	/* clear CE bit before going cached */
	runcached();
	setsr(oldsr);	/* now enable interrupts */

	missed_2nd = 0;
	if (inwhat == IN_MEM) {
		/* now flush this line to memory by reading an address 
		 * one 2nd cache-size above K0addr
		oneoffval = *(uint *)k0oneoff;
		*/

		/* flush the bad line out to memory.  Since the rmi fixes 
		 * all one bit errors unconditionally on writes, this
		 * must be at least a 2-bit error */
		/* prevent ecc error now; this way it'll get out there
		 * flawed and the next cached-read will get a SysAD ECC */
		oldsr = getsr();

		setsr(ce_no_ints_sr);
		if (!_c_hwbinv(CACH_SD, k0addr))
			missed_2nd = 1;	/* mustn't print with CE bit on */
		setsr(oldsr);
	}

	if (inwhat == IN_MEM && missed_2nd)
		printf("!!?force_ecc: addr 0x%x 2ndary hwbinv missed cache!\n",
			k0addr);

	if (inwhat == IN_SD) {	/* reading into primary will check ecc */
		if (force_verbose)
			printf("IN_SD: here we go!\n");
#ifndef FORCE_CACHERR_ON_STORE
		/* Force cache error on load */

		lo_val = *k0ptr;
#else
		/* force cache error on store (should turn on EI)
		 *
		 * Two interesting case.  In one we write completely new
		 * data into one of the words of the doubleword.  This will
		 * most likely cause us to report an MBE if the EI bit
		 * does not get set since the ECC will be computed on
		 * this value  (in the PD) and comareed to the ECC in the
		 * secondary.
		 * The other case stores the same data (test program is
		 * generating error in the other word of the double word).
		 */
#if 0
		/* this test tends to generate FATAL MBE if EI not set */
		ecc_store_err(0x1234, k0addr);	/* write some new data */
#else
		/* this test replaces with same data, so looks like SBE */
		ecc_store_err(lo_val, k0addr);
#endif

#if 0
		/* for now we use more controlled environment of
		 * assembly language code.
		 */
		/* Force cache error on store (EI) */
		*k0ptr = lo_val;;
#endif /* 0 */
#endif	/* force cache error on store */
	}


	if (force_verbose)
		printf("force_ecc %s exits\n",cptr);
	return 0;

    case IN_PD:
	cptr = error_loc_names[CACH_PD];
	if (force_verbose)
		printf("      Force PD cache ecc error (%d)\n",inwhat);

	/* get valid cache line and write the specified dbl-word */
	*k0ptr = new_ecc.lo_word;
	*(k0ptr+1) = new_ecc.hi_word;

	_c_ilt_n_ecc(CACH_PD, k0addr, tags, &orig_ecc);

	if (force_verbose)
		printf("      f_ecc IN_PD: addr 0x%x: taglo 0x%x, ecc 0x%x\n",
			k0addr,tags[TAGLO_IDX],orig_ecc);
	orig_ecc ^= 0x1;	/* toggle parity bit */ 
	if (force_verbose)
		printf("new ecc: 0x%x\n",orig_ecc);
	setecc(orig_ecc);

	/* set CE status bit--cachops will use contents of ecc register
	 * for data parity instead of computing the correct one. */
	oldsr = no_ints_sr = getsr();
	/* disable interrupts for entire time */
	no_ints_sr &= ~SR_IMASK8;
	ce_no_ints_sr = (no_ints_sr | SR_CE);

	setsr(no_ints_sr);	/* no ints while running uncached */
	uncached();  /* uncached instr stream: line won't be replaced */

	/* get line again in case instr. forced it out */
	*k0ptr = new_ecc.lo_word;
	*(k0ptr+1) = new_ecc.hi_word;

	setsr(ce_no_ints_sr);
	/* storing the same value as above with SR_CE bit set, using the 
	 * ECC register with the parity bit toggled forces incorrect
	 * data parity and causes an ecc exception. */
	*k0ptr = new_ecc.lo_word;
	setsr(no_ints_sr);	 /* clear CE bit */
	runcached();
	setsr(oldsr);	 /* and enable interrupts */

	if (force_verbose)
	    printf("      exiting force_ecc, case IN_PD (%d)\n",IN_PD);
	return 0;
    case IN_PI:
	cptr = error_loc_names[CACH_PI];
	break;
    case IN_SI:
	cptr = error_loc_names[CACH_SI];
	break;

    case 120:
	ecc_cleanup();
	return 0;

    default:
	printf("Illegal inwhat (%d)\n",inwhat);
	return 0;

    } /* switch */

    if (force_verbose)
	printf("    force ecc in %s (%d)\n",cptr,inwhat);
    return 0;
}
#endif /* IP19 && _FORCE_ECC */

#endif	/* !TFP && !BEAST */

#if EVEREST
#include <sys/inst.h>
#define	EFRAME_REG(efp,reg)	(((k_machreg_t *)(efp))[reg])
#define REGVAL(efp,x)       ((x)?EFRAME_REG((efp),(x)+EF_AT-1):0)

/* ARGSUSED */
int
find_buserror_info(eframe_t *ep, inst_t **epcp, int *ldstp, 
		void **vaddrp, uint *paddrhip, uint *paddrlop)
{
	inst_t *epc;

#ifndef TFP
	union mips_instruction inst;
	void *vaddr;
	int ldst;
	pfn_t pfn;
	uint paddrlo, paddrhi;
#endif

    epc = (inst_t *)EFRAME_REG(ep,EF_EPC);
    if ((long)EFRAME_REG(ep,EF_CAUSE) & CAUSE_BD)
	epc +=4;

#if TFP
    /*
     * Bus errors are imprecise on TFP, so the EPC is meaningless. Printing
     * out information based on the EPC will only confuse the user. Just
     * return the EPC in the exception frame so the panic message matches
     * the warning message.
     */
    *epcp = epc;
    return 0;
#else	/* ! TFP */

    if (IS_KUSEG((long)epc))
    	inst.word = fuword(epc);
    else
	inst.word = *epc;

    vaddr = (void *)((__psint_t)REGVAL(ep, inst.i_format.rs) + 
				inst.i_format.simmediate);

    switch (inst.i_format.opcode) {
    /* Loads */
    case ld_op:
    case lwu_op:
    case lw_op:
    case lhu_op:
    case lh_op:
    case lbu_op:
    case lb_op:
	ldst = 1;
	break;
    /* Stores */
    case sd_op:
    case sw_op:
    case sh_op:
    case sb_op:
	ldst = 0;
	break;

    /* XXX What do we do about these? */
    /* Cop1 instructions */
    case lwc1_op:
    case ldc1_op:
    case swc1_op:
    case sdc1_op:
    /* Unaligned load/stores */
    case ldl_op:
    case ldr_op:
    case lwl_op:
    case lwr_op:
    case sdl_op:
    case sdr_op:
    case swl_op:
    case swr_op:
    /* Load linked/store conditional */
    case lld_op:
    case scd_op:
    case ll_op:
    case sc_op:
    default:
	return 0;
    }

	if (IS_KUSEG(vaddr)) {
		vtop(vaddr, 1, &pfn, 1);
	} else
		pfn = kvtophyspnum((void *)vaddr);

	paddrhi = (pfn>>20);
	paddrlo = (pfn << 12) | ((long)vaddr & 0xfff);

	*epcp = epc;
	*ldstp = ldst;
	*vaddrp = vaddr;
	*paddrhip = paddrhi;
	*paddrlop = paddrlo;
	return 1;
#endif	/* TFP */
}

#if ECC_RECOVER
static void *last_ecc_recoverable = 0;
/*
 *  We introduce here the arbitrary concept of a "flurry" of recoverable
 *  multibit errors.  We want to survive instances of isolated flurries (e.g.,
 *  lots of errors on a single page), but not continue to "recover" from truly
 *  hard errors which cause endless bus errors which just happen to appear to
 *  be "recoverable".  What we do is to timestamp the first recoverable error
 *  of a flurry, allow some number of additional recoveries in a short period
 *  of time, then refuse to "recover" more than some max number occuring in
 *  that "short period".
 */
#define ECC_RECOVERABLE_FLURRY_MAX 32	/* s-cache lines per 4096 byte page */
static int time_ecc_recoverable_flurry = 0;	/* time, in secs, of flurry */
static int count_ecc_recoverable_flurry = 0;	/* count recoveries in flurry */
#endif	/* ECC_RECOVER */

/*
 * See if we can recover from an ECC error:
 *   IF the PC points to kernel "block zero" or "block copy" code  AND
 *   IF we were just crossing into a secondary cache line  AND
 *   IF we planned to update the entire secondary cache line with new data AND
 *   IF we did not fault on this same location recently  AND
 *   IF the systune parameter "ecc_recover_enable" is nonzero, which specifies
 *     a time interval (in seconds) within which we will keep trying to recover
 *     a maximum of ECC_RECOVERABLE_FLURRY_MAX errors.
 * THEN we can recover.
 *
 * Returns:
 *   0 if cannot recover
 *   1 if can recover
 *
 * Side effect: sets global last_ecc_recoverable to faulting virtual addr.
 *
 * NOTE: This code counts on the bcopy/bzero routines to supply the
 * appropriate lables and to use register A3 to hold the upper bound
 * for destination addresses!
 */
/* ARGSUSED */
ecc_recoverable(eframe_t *ep, inst_t *epc, void *vaddr)
{
#if ECC_RECOVER
	extern	char bzero_stores[];
	extern	char bcopy_stores[];
	extern int ecc_recover_enable;
	long destination_limit;

	if (!ecc_recover_enable)
		return 0;

	if (((long)epc != (long)bzero_stores) && 
	    ((long)epc != (long)bcopy_stores))
		return 0;

	if (!SCACHE_ALIGNED((long)vaddr))
		return 0;

	/* The following code assumes certain details of the bcopy/bzero
	 * code in order to determine if we will be storing the entire
	 * cacheline.  If we get a multibit error on the first store into
	 * a cacheline AND if we will be storing the entire line THEN
	 * we can safely ignore the error.
	 */
	destination_limit = (long)EFRAME_REG(ep,EF_A3);

	if (destination_limit-(long)vaddr < SCACHE_LINESIZE)
		return 0;

	if (last_ecc_recoverable == vaddr)
		return 0;

	if (!ecc_recover_enable)
		return 0;

	last_ecc_recoverable = vaddr;

	if (time - time_ecc_recoverable_flurry > ecc_recover_enable) {
		/*
		 *  It has been a sufficiently "long time" since the latest
		 *  flurry of recoverable multibit errors, so reset the
		 *  count/time.
		 */
		time_ecc_recoverable_flurry = time;
		count_ecc_recoverable_flurry = 1;
	} else {
		/*
		 *  This is another recoverable error in a "short" period of
		 *  time.  Allow a certain number of these in that time, then
		 *  give up and stop trying to recover.
		 */
		if (++count_ecc_recoverable_flurry > ECC_RECOVERABLE_FLURRY_MAX)
			return 0;
	}

	return 1;
#else /* ECC_RECOVER */
	return 0;
#endif /* ECC_RECOVER */
}
#endif /* EVEREST */


#if !MCCHIP && !IP30 && !IP32 
                        /* MC based systems don not currently have ECC */
#if !defined (SN)	/* SN has its own bus error processing */
/*
 * dobuserre - handle bus error exception
 */
int ecc_recover_count = 0;
static volatile cpumask_t buserr_panic_pending = {0};
/* 0: kernel; 1: kernel - no print; 2: user */

int 
dobuserre(register eframe_t *ep, inst_t *epc, uint flag)		
{
#if TFP 
  unsigned	ev_ile;
#endif

#ifdef EVEREST
#if IP19 || IP25
  cpu_cookie_t 	err_info;
#endif
#endif

  int s = splhi(); 	/* Prevent preemption from now on */

#ifdef EVEREST
#if IP19 || IP25
  if (curthreadp) {
    err_info = setmustrun(ep->ef_cpuid);
  }
  ASSERT(cpuid() == ep->ef_cpuid);
#endif
#endif


#if TFP
	ev_ile = EV_GET_LOCAL(EV_ILE);	/* Current ILE register */
#endif /* TFP */
	switch (flag) {
	case 0:
	default:
#ifdef	EVEREST
	  cmn_err(CE_WARN|CE_CPUID,
		  "%s Bus Error, Kernel mode, eframe:0x%x EPC:0x%x",
		  ((ep->ef_cause & CAUSE_EXCMASK) == EXC_IBE)
		  ? "Instruction" : "Data", ep, epc);
#endif
	  /*
	   *  If we're not already panicing, then start to panic.
	   *  If we're already panicing on another cpu, then just
	   *  silently spin here, waiting for an intercpu command.
	   *  If we're already panicing on this cpu, then go ahead and
	   *  double-panic.
	   */
	  while (CPUMASK_IS_NONZERO(buserr_panic_pending)  &&
		 (!CPUMASK_TSTM(buserr_panic_pending, private.p_cpumask)))
	    ;		/* sit and spin */
	  
	  
	  {
	    inst_t *epc;
	    int ldst;
	    void *vaddr;
	    uint paddrhi, paddrlo;
	    
	    CPUMASK_ATOMSET(buserr_panic_pending, cpumask());
#ifdef EVEREST
	    dump_hwstate(1);
	    
	    if (find_buserror_info(ep,&epc,&ldst,&vaddr,&paddrhi, &paddrlo)) {
	      cmn_err(CE_WARN, 
		      "BUSERR: %s instruction, virtual address 0x%x (addrhi=0x%x addrlo=0x%x)\n", 
		      ldst ? "LOAD" : "STORE", vaddr, paddrhi, paddrlo);
	      
	      printf("BUSERR: ");
	      mc3_decode_addr(printf, paddrhi, paddrlo);
	      
	      if (ecc_recoverable(ep, epc, vaddr)) {
		cmn_err(CE_WARN, 
			"ECC RECOVERED -- CONTINUE NORMAL OPERATION.\n");
		/*
		 * We can try to recover this error. 
		 * Clear our recollection of the
		 * event, to avoid future confusion.
		 */
		everest_error_clear(0);
		
		ecc_recover_count++;
		CPUMASK_ATOMCLR(buserr_panic_pending, cpumask());
		
#ifdef EVEREST
#if IP19 || IP25
		if(curthreadp)
		  restoremustrun(err_info);
#endif
#endif
		splx(s);
		return 1; /* problem handled */
	      }
	    }
#endif
	    cmn_err_tag(74,CE_PANIC,
			"Bus Error in Kernel mode, eframe:0x%x EPC:0x%x",
			ep, epc);
	  }

	case 1:
		/* nofault */
#if TFP
	  EV_SET_REG(EV_CERTOIP, 0xffff); 	/* Clear Bus Error */
	  EV_SET_LOCAL(EV_ILE, ev_ile|EV_ERTOINT_MASK);	/* re-enable BE */
	  tfp_clear_gparity_error();
#endif

#ifdef EVEREST
#if IP19 || IP25
	  if(curthreadp)
	    restoremustrun(err_info);
#endif	  
#endif
	  splx(s);
	  return 0;

	case 2:
#ifdef 	EVEREST
#if IP19 || IP25
	  if( uvme_errclr(ep) == 1) {
	    if(curthreadp)
	      restoremustrun(err_info);
	    splx(s);
	    return 0;
	  }
#endif /* IP19 || IP25 */
	  
	  cmn_err(CE_WARN|CE_CPUID,
		  "%s Bus Error, User mode, eframe:0x%x EPC:0x%x",
		  ((ep->ef_cause & CAUSE_EXCMASK) == EXC_IBE)
		  ? "Instruction" : "Data", ep, epc);
#endif /* EVEREST */
	  /*
	   *  If we're not already panicing, then start to panic.
	   *  If we're already panicing on another cpu, then just
	   *  silently spin here, waiting for an intercpu command.
	   *  If we're already panicing on this cpu, then go ahead and
	   *  double-panic.
	   */
	  while (CPUMASK_IS_NONZERO(buserr_panic_pending)  &&
		 (!CPUMASK_TSTM(buserr_panic_pending, private.p_cpumask)))
	    ;		/* sit and spin */
	  
	  {
	    inst_t *epc;
	    int ldst;
	    void *vaddr;
	    uint paddrhi, paddrlo;
	    
	    CPUMASK_ATOMSET(buserr_panic_pending, cpumask());
#ifdef EVEREST
	    dump_hwstate(1);
	    
	    if (find_buserror_info(ep,&epc,&ldst,&vaddr,&paddrhi, &paddrlo)) {
	      cmn_err(CE_WARN, 
		      "BUSERR: %s instruction: virtual address 0x%x (physical 0x%x%x)\n", ldst ? "LOAD" : "STORE", vaddr, paddrhi, paddrlo);

	      printf("BUSERR: ");
	      mc3_decode_addr(printf, paddrhi, paddrlo);
	    }
#endif
	    cmn_err_tag(75,CE_PANIC,
			"Bus Error in User mode, eframe:0x%x EPC:0x%x",
			ep, epc);
	  }

	} /* switch */
	/* NOTREACHED */
}
#endif /* SN0 */
#endif /* !MCCHIP && !IP30 */


/* The R4000 has a built-in floating-point unit. These 2 functions
 * are used by floating-point emulation (nofphw.s), which is not
 * included in R4000 kernels. So these routines stub out the 
 * unresolved externals.
 */
int
softfp_adderr()
{
	cmn_err(CE_PANIC, "softfp_adderr for R4000?");
	return 0;
}

int
softfp_insterr()
{
	cmn_err(CE_PANIC, "softfp_insterr for R4000?");
	return 0;
}


#ifdef _MEM_PARITY_WAR
/*
 *	ecc_create_exception
 *
 *	Create an exception frame from an ecc exception frame,
 *	including changing the state of the system to common
 *	exception state.  The new frame is allocated on the
 *	appropriate kernel stack.  The code runs with SR_ERL set
 *	in $sr, so it must avoid taking any exceptions.
 */

extern void ecc_map_uarea(void);

u_long *
ecc_create_exception(eframe_t *ep)
{
	eframe_t	*nep;
	u_long		*nsp;

	if (private.p_kstackflag == PDA_CURUSRSTK) {
		/* map the u-area */
		ecc_map_uarea();
		/* allocate the frame */
		nep = &curexceptionp->u_eframe;
		private.p_kstackflag = PDA_CURKERSTK;
		nsp = ((u_long *) (KERNELSTACK)) - 4;
	} else {
		/* was on kernel, idle, or interrupt stack */
		nep = ((eframe_t *) ep->ef_sp) - 1;
		nsp = (u_long *) nep;
	}
	*nep = *ep;
	nsp[0] = (u_long) nep;
	nep->ef_sr &= ~SR_ERL; /* turn off SR_ERL in frame */
	return(nsp);
}


#endif /* _MEM_PARITY_WAR */



#if defined (IP19)

pfn_t
init_ecc_sp(fpage)
        pfn_t fpage;
{
        __psunsigned_t *cache_sp_k1ptr;
        
        cache_sp_k1ptr = (__psunsigned_t *)(PHYS_TO_K1(CACHE_ERR_SP_PTR));
        
        *cache_sp_k1ptr = PHYS_TO_K1(ctob(fpage) + CACHE_ERR_STACK_SIZE 
			       - sizeof(void *));
        
        return (fpage + btoc(CACHE_ERR_STACK_SIZE));
}
	      
#endif

/*
 * Interface to dump stuff in ioerror
 */
char *error_mode_string[] = 
{ "probe", "kernel", "user", "reenable" };

extern void
ioerror_dump(char *name, int error_code, int error_mode, ioerror_t *ioerror)
{
	printf("%s%s%s%s%s error in %s mode\n",
	       name,
	       (error_code & IOECODE_PIO) ? " PIO" : "",
	       (error_code & IOECODE_DMA) ? " DMA" : "",
	       (error_code & IOECODE_READ) ? " Read" : "",
	       (error_code & IOECODE_WRITE) ? " Write" : "",
	       error_mode_string[error_mode]);

#define	PRFIELD(f)							\
	if (IOERROR_FIELDVALID(ioerror,f))				\
		printf("\t%20s: 0x%X\n", #f, IOERROR_GETVALUE(ioerror,f));
	
	PRFIELD(errortype);		/* error type: extra info about error */
	PRFIELD(widgetnum);		/* Widget number that's in error */
	PRFIELD(widgetdev);		/* Device within widget in error */
	PRFIELD(srccpu);		/* CPU on srcnode generating error */
	PRFIELD(srcnode);		/* Node which caused the error 	 */
	PRFIELD(errnode);		/* Node where error was noticed	 */
	PRFIELD(sysioaddr);		/* Sys specific IO address	 */
	PRFIELD(xtalkaddr);		/* Xtalk (48bit) addr of Error 	 */
	PRFIELD(busspace);		/* Bus specific address	space	 */
	PRFIELD(busaddr);		/* Bus specific address	 	 */
	PRFIELD(vaddr);			/* Virtual address of error 	 */
	PRFIELD(memaddr);		/* Physical memory address  	 */
	PRFIELD(epc);			/* pc when error reported	 */
	PRFIELD(ef);			/* eframe when error reported	 */

#undef	PRFIELD

	printf("\n");
}


/*
 * machine dependent code for error handling. Mark a page inaccessible and
 * later clean and put it back in VM circulation if possible.
 */

/* ARGSUSED */
void
error_mark_page(paddr_t paddr)
{
#if defined (SN0)
	extern void sn0_error_mark_page(paddr_t);
	sn0_error_mark_page(paddr);
#else
	cmn_err(CE_NOTE, "error_mark_page: not supported");
#endif
}


/* ARGSUSED */
int
error_reclaim_page(paddr_t paddr, int flag)
{
#if defined (SN0)
	extern int sn0_error_reclaim_page(paddr_t, int);
	return sn0_error_reclaim_page(paddr, flag);
#else
	cmn_err(CE_NOTE, "error_reclaim_page: not supported");
	return 0;
#endif
}	
