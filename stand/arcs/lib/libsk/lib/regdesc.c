#ident	"saio/lib/regdesc.c:  $Revision: 1.41 $"

/*
 * regdesc.c -- %r descriptions for MIPS cpu registers
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <libsc.h>

/*
 * CP0 status register description
 */
struct reg_values imask_values[] = {
#ifdef TFP
	{ SR_IMASK11,	"11" },
	{ SR_IMASK10,	"10" },
	{ SR_IMASK9,	"9" },
#endif
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

#if R4000 || R10000
struct reg_values mode_values[] = {
	{ SR_KSU_USR,	"USER" },
	{ SR_KSU_KS,	"SPRVSR" },
	{ 0,		"KERNEL" },
	{ 0,		NULL },
};
#endif /* R4000 || R10000 */

#if TFP
struct reg_values tfppgsz_values[] = {
	{ 0x00,		"4k"  },
	{ 0x01,		"8k"  },
	{ 0x02,		"16k" },
	{ 0x03,		"64k" },
	{ 0,		NULL }
	/* others unused currently */
};
#endif

struct reg_desc sr_desc[] = {
	/* mask	     shift      name   format  values */
#if R4000 || R10000
	{ SR_XX,	0,	"XX",	NULL,	NULL },
	{ SR_CU2,	0,	"CU2",	NULL,	NULL },
	{ SR_CU1,	0,	"CU1",	NULL,	NULL },
	{ SR_CU0,	0,	"CU0",	NULL,	NULL },
#if R4000
	{ SR_RP,	0,  	"RP", 	NULL,	NULL },
#endif	/* R4000 */
	{ SR_FR,	0,  	"FR", 	NULL,	NULL },
	{ SR_RE,	0,  	"RE", 	NULL,	NULL },
	{ SR_BEV,	0,	"BEV",	NULL,	NULL },
	{ SR_TS,	0,	"TS",	NULL,	NULL },
	{ SR_SR,	0,	"SR",	NULL,	NULL },
#if R10000
	{ SR_NMI,	0,	"NMI",	NULL,	NULL },
#endif	/* R10000 */
	{ SR_CH,	0,	"CH",	NULL,	NULL },
#if R4000
	{ SR_CE,	0,	"CE",	NULL,	NULL },
#endif	/* R4000 */
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
#endif	/* R4000 || R10000 */
#if TFP
	{ SR_CU1,	0,	"CU1",	NULL,	NULL },
	{ SR_CU0,	0,	"CU0",	NULL,	NULL },
	{ SR_FR,	0,  "FR", 	NULL,	NULL },
	{ SR_IBIT11,	0,	"IM11",	NULL,	NULL },
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
	{ SR_KPSMASK,	-36,	"KPS",	NULL,	tfppgsz_values },
	{ SR_UPSMASK,	-32,	"UPS",	NULL,	tfppgsz_values },
	{ SR_KU,	0,	"KU",	NULL,	NULL },
	{ SR_UX,	0,	"UX",	NULL,	NULL },
	{ SR_XX,	0,	"XX",	NULL,	NULL },
	{ SR_EXL,	0,	"EXL",	NULL,	NULL },
	{ SR_IE,	0,	"IE",	NULL,	NULL },
#endif	/* TFP */
	{ 0,		0,	NULL,	NULL,	NULL },
};

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
	{ EXC_IBE,	"IBE" },
	{ EXC_DBE,	"DBE" },
	{ EXC_SYSCALL,	"SYSCALL" },
	{ EXC_BREAK,	"BREAK" },
	{ EXC_II,	"II" },
	{ EXC_CPU,	"CPU" },
	{ EXC_OV,	"OV" },
#if R4000 || R10000
	{ EXC_TRAP,	"TRAP" },
#if R4000
	{ EXC_VCEI,	"VCEI" },
#endif	/* R4000 */
	{ EXC_FPE,	"FPE" },
	{ EXC_WATCH,	"WATCH" },
#if R4000
	{ EXC_VCED,	"VCED" },
#endif	/* R4000 */
#endif /* R4000 || R10000 */
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
	{ CAUSE_IP11,	0,	"IP11",NULL,	NULL },
	{ CAUSE_IP10,	0,	"OGCache",NULL,	NULL },
	{ CAUSE_IP9,	0,	"EGCache",NULL,	NULL },
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

/*
 * CP0 tlb entry high description
 */
struct reg_desc tlbhi_desc[] = {
	/* mask	     shift      name   format  values */
	{ TLBHI_VPNMASK,0,	"VA",	"0x%x",	NULL },
	{ TLBHI_PIDMASK,-TLBHI_PIDSHIFT,"PID",	"%d",	NULL },
	{ 0,		0,	NULL,	NULL,	NULL },
};

struct reg_values cache_values[] = {
#if R4000 || R10000
	{ TLBLO_NONCOHRNT,	"NONCOHRNT" },
	{ TLBLO_UNCACHED,	"UNCACHED" },
	{ TLBLO_EXLWR,		"EXL_WR" },
	{ TLBLO_EXL,		"EXL" },
#if R10000
	{ TLBLO_UNCACHED_ACC,	"UNCACHED_ACC" },
#endif	/* R10000 */
#else
	{ TLBLO_UNC_PROCORD,	"UNC_PROCORD" },
	{ TLBLO_UNC_SEQORD,	"UNC_SEQORD" },
	{ TLBLO_NONCOHRNT,	"NONCOHRNT" },
	{ TLBLO_COHRNT_EXL,	"COHRNT_EXL" },
	{ TLBLO_COHRNT_EXLWR,	"COHRNT_EXLWR" },
#endif
	{ 0,		NULL },
};

/*
 * CP0 tlb entry low0/1 description (R4000 only)
 */
struct reg_desc tlblo01_desc[] = {
	/* mask	     shift      name   format  values */
	{ TLBLO_PFNMASK,-TLBLO_PFNSHIFT, "PFN",	"0x%x",	NULL },
	{ TLBLO_CACHMASK,0,	"CACHALGO",	NULL,	cache_values },
	{ TLBLO_D,	0,	"D",	NULL,	NULL },
	{ TLBLO_V,	0,	"V",	NULL,	NULL },
#if !TFP
	{ TLBLO_G,	0,	"G",	NULL,	NULL },
#endif
	{ 0,		0,	NULL,	NULL,	NULL },
};


/*
 * CP0 tlb index description
 */
struct reg_desc tlbinx_desc[] = {
	/* mask	     shift      name   format  values */
#if !TFP
	{ TLBINX_PROBE,	0,	"PFAIL",NULL,	NULL },
	{ 0x3f, 0, "INDEX", "%d", NULL },
#endif
	{ 0,		0,	NULL,	NULL,	NULL },
};

/*
 * CP0 tlb random description
 */
struct reg_desc tlbrand_desc[] = {
	/* mask	     shift      name   format  values */
#if !TFP
	{ TLBRAND_RANDMASK, -TLBRAND_RANDSHIFT, "RANDOM", "%d", NULL },
#endif
	{ 0,		0,	NULL,	NULL,	NULL },
};

#if R4000 || R10000
/*
 * CP0 context description
 */
struct reg_desc tlbctxt_desc[] = {
	/* mask	     shift      name   format  values */
	{ TLBCTXT_BASEMASK, 0,	"PTEBASE", "0x%x", NULL },
	{ TLBCTXT_VPNMASK, 9,	"BADVAP", "0x%x", NULL},
	{ 0,		0,	NULL,	NULL,	NULL }
};


struct reg_desc tlbextctxt_desc[] = {
	{ TLBCTXT_BASEMASK, 0,	"PTEBASE", "0x%x", NULL },
	{ TLBCTXT_VPNMASK, 9,	"BADVAP", "0x%x", NULL},
	{ 0,		0,	NULL,	NULL,	NULL }
};
#else
struct reg_desc tlbctxt_desc[] = {
	{ 0,		0,	NULL,	NULL,	NULL }
};
#endif

#if R4000 

#define EC_SHIFT	31
#define EC_SHIFTIT(VAL)	(VAL << EC_SHIFT)
#define	CONFIG_EC_DIV2	0x00000000	/* EC bit: sys clock ratio is /2 */
#define	CONFIG_EC_DIV3	0x00000001	/* clock is /3 */
#define	CONFIG_EC_DIV4	0x00000002	/* clock is /4 */
struct reg_values config_ec_vals[] = {
	{ EC_SHIFTIT(CONFIG_EC_DIV2),	" clock: /2" },
	{ (unsigned)EC_SHIFTIT(CONFIG_EC_DIV3),	" clock: /3" },
	{ EC_SHIFTIT(CONFIG_EC_DIV4),	" clock: /4" },
	{ 0,			NULL },
};


#define EP_SHIFT	31
#define EP_SHIFTIT(VAL)	(VAL << EP_SHIFT)
#define	CONFIG_EP_D		0x00000000	/* xmit 1 dbl per cycle */
#define	CONFIG_EP_DDX		0x00000001	/* xmit 2 dbls per 3cyc */
#define	CONFIG_EP_DDXX		0x00000002	/* xmit 2 dbls per 4cyc */
#define	CONFIG_EP_DXDX		0x00000003	/* xmit 2 dbls per 4cyc */
#define	CONFIG_EP_DDXXX		0x00000004	/* xmit 2 dbls per 5cyc */
#define	CONFIG_EP_DDXXXX	0x00000005	/* xmit 2 dbls per 6cyc */
#define	CONFIG_EP_DXXDXX	0x00000006	/* xmit 2 dbls per 6cyc */
#define	CONFIG_EP_DDXXXXX	0x00000007	/* xmit 2 dbls per 7cyc */
#define	CONFIG_EP_DXXXDXXX	0x00000008	/* xmit 2 dbls per 8cyc */
struct reg_values config_ep_vals[] = {
	{ EP_SHIFTIT(CONFIG_EP_D),		" xmit D" },
	{ (unsigned)EP_SHIFTIT(CONFIG_EP_DDX),	" xmit DDx" },
	{ EP_SHIFTIT(CONFIG_EP_DDXX),		" xmit DDxx" },
	{ (unsigned)EP_SHIFTIT(CONFIG_EP_DXDX),	" xmit DxDx" },
	{ EP_SHIFTIT(CONFIG_EP_DDXXX),		" xmit DDxxx" },
	{ EP_SHIFTIT(CONFIG_EP_DXXDXX),		" xmit DxxDxx" },
	{ (unsigned)EP_SHIFTIT(CONFIG_EP_DDXXXXX)," xmit DDxxxxx" },
	{ EP_SHIFTIT(CONFIG_EP_DXXXDXXX),	" xmit DxxxDxxx" },
	{ 0,			NULL },
};

#define SB_SHIFT	22
#define SB_SHIFTIT(VAL)	(VAL << SB_SHIFT)
#define	CONFIG_SB_4		0x00000000	/* 4 wds / 2ndary line */
#define	CONFIG_SB_8		0x00000001	/* 8 wds / 2ndary line */
#define	CONFIG_SB_16		0x00000002	/* 16 wds / 2ndary line */
#define	CONFIG_SB_32		0x00000003	/* 32 wds / 2ndary line */
struct reg_values config_sb_vals[] = {
	{ SB_SHIFTIT(CONFIG_SB_4),		" SCL=4wd" },
	{ SB_SHIFTIT(CONFIG_SB_8),		" SCL=8wds" },
	{ SB_SHIFTIT(CONFIG_SB_16),		" SCL=16wds" },
	{ SB_SHIFTIT(CONFIG_SB_32),		" SCL=32wds" },
	{ 0,			NULL },
};

#define SS_SHIFT	21
#define SS_SHIFTIT(VAL)	(VAL << SS_SHIFT)
#define	CONFIG_SS_MIXED		0x00000000	/* combined I & D 2ndary */
#define	CONFIG_SS_SPLIT		0x00000001	/* I and D scache split */
struct reg_values config_ss_vals[] = {
	{ SS_SHIFTIT(CONFIG_SS_MIXED),	" mixed I&D" },
	{ SS_SHIFTIT(CONFIG_SS_SPLIT),	" split I&D" },
	{ 0,				NULL },
};

#define SW_SHIFT	20
#define SW_SHIFTIT(VAL)	(VAL << SW_SHIFT)
#define	CONFIG_SW_128		0x00000000	/* scache port width 128 bits */
#define	CONFIG_SW_64		0x00000001	/* scache port width 64 bits */
struct reg_values config_sw_vals[] = {
	{ SW_SHIFTIT(CONFIG_SW_128),	"\n    128bit scport" },
	{ SW_SHIFTIT(CONFIG_SW_64),	"\n    64bit scport" },
	{ 0,				NULL },
};

#define EW_SHIFT	18
#define EW_SHIFTIT(VAL)	(VAL << EW_SHIFT)
#define	CONFIG_EW_64		0x00000000	/* System Port width 64 bits */
#define	CONFIG_EW_32		0x00000001	/* System Port width 32 bits */
struct reg_values config_ew_vals[] = {
	{ EW_SHIFTIT(CONFIG_EW_64),	" 64bit sysport" },
	{ EW_SHIFTIT(CONFIG_EW_32),	" 32bit sysport" },
	{ 0,		NULL },
};


#define SC_SHIFT	17
#define SC_SHIFTIT(VAL)	(VAL << SC_SHIFT)
#define CONFIG_SC_PRESENT	0
#define CONFIG_SC_ABSENT	1
struct reg_values config_sc_vals[] = {
	{ SC_SHIFTIT(CONFIG_SC_PRESENT),	" S-cache" },
	{ SC_SHIFTIT(CONFIG_SC_ABSENT),		" no S-cache" },
	{ 0,		NULL },
};


#define SM_SHIFT	16	
#define SM_SHIFTIT(VAL)	(VAL << SM_SHIFT)
#define CONFIG_SM_ON		0
#define CONFIG_SM_OFF		1
struct reg_values config_sm_vals[] = {
	{ SM_SHIFTIT(CONFIG_SM_ON),		" DS on" },
	{ SM_SHIFTIT(CONFIG_SM_OFF),		" DS off" },
	{ 0,		NULL },
};

#define BE_SHIFT	15	
#define BE_SHIFTIT(VAL)	(VAL << BE_SHIFT)
#define CONFIG_LITTLENDIAN	0
#define CONFIG_BIGENDIAN	1
struct reg_values endian_values[] = {
	{ BE_SHIFTIT(CONFIG_LITTLENDIAN),	" LE" },
	{ BE_SHIFTIT(CONFIG_BIGENDIAN),		" BE" },
	{ 0,		NULL },
};


#define EM_SHIFT	14	
#define EM_SHIFTIT(VAL)	(VAL << EM_SHIFT)
#define CONFIG_EM_ECC		0
#define CONFIG_EM_PARITY	1
struct reg_values config_em_vals[] = {
	{ EM_SHIFTIT(CONFIG_EM_ECC),	" ecc" },
	{ EM_SHIFTIT(CONFIG_EM_PARITY),	" parity" },
	{ 0,		NULL },
};


#define IC_SHIFT(VAL)	(VAL << CONFIG_IC_SHFT)
#define DC_SHIFT(VAL)	(VAL << CONFIG_DC_SHFT)
#define CONFIG_PC_4KB	0
#define CONFIG_PC_8KB	1
#define CONFIG_PC_16KB	2
#define CONFIG_PC_32KB	3
#define CONFIG_PC_64KB	4
#define CONFIG_PC_128KB	5
#define CONFIG_PC_256KB	6
#define CONFIG_PC_512KB	7

struct reg_values config_ic_vals[] = {
	{ IC_SHIFT(CONFIG_PC_4KB),	"\n    PI=4kb" },
	{ IC_SHIFT(CONFIG_PC_8KB),	"\n    PI=8kb" },
	{ IC_SHIFT(CONFIG_PC_16KB),	"\n    PI=16kb" },
	{ IC_SHIFT(CONFIG_PC_32KB),	"\n    PI=32kb" },
	{ IC_SHIFT(CONFIG_PC_64KB),	"\n    PI=64kb" },
	{ IC_SHIFT(CONFIG_PC_128KB),	"\n    PI=128kb" },
	{ IC_SHIFT(CONFIG_PC_256KB),	"\n    PI=256kb" },
	{ IC_SHIFT(CONFIG_PC_512KB),	"\n    PI=512kb" },
	{ 0,		NULL },
};


struct reg_values config_dc_vals[] = {
	{ DC_SHIFT(CONFIG_PC_4KB),	" PD=4kb" },
	{ DC_SHIFT(CONFIG_PC_8KB),	" PD=8kb" },
	{ DC_SHIFT(CONFIG_PC_16KB),	" PD=16kb" },
	{ DC_SHIFT(CONFIG_PC_32KB),	" PD=32kb" },
	{ DC_SHIFT(CONFIG_PC_64KB),	" PD=64kb" },
	{ DC_SHIFT(CONFIG_PC_128KB),	" PD=128kb" },
	{ DC_SHIFT(CONFIG_PC_256KB),	" PD=256kb" },
	{ DC_SHIFT(CONFIG_PC_512KB),	" PD=512kb" },
	{ 0,		NULL },
};

#define IB_SHIFT	5
#define IB_SHIFTIT(VAL)	(VAL << IB_SHIFT)
#define CONFIG_IB_16	0
#define CONFIG_IB_32	1
struct reg_values config_ib_vals[] = {
	{ IB_SHIFTIT(CONFIG_IB_16),	" PIL=16b" },
	{ IB_SHIFTIT(CONFIG_IB_32),	" PIL=32b" },
	{ 0,		NULL },
};

#define DB_SHIFT	4
#define DB_SHIFTIT(VAL)	(VAL << DB_SHIFT)
#define CONFIG_DB_16	0
#define CONFIG_DB_32	1
struct reg_values config_db_vals[] = {
	{ DB_SHIFTIT(CONFIG_DB_16),	" PDL=16b" },
	{ DB_SHIFTIT(CONFIG_DB_32),	" PDL=32b" },
	{ 0,		NULL },
};

struct reg_values cohalgo_values[] = {
	{ CONFIG_NONCOHRNT,	" NONCOHRNT" },
	{ CONFIG_UNCACHED,	" UNCACHED" },
	{ CONFIG_COHRNT_EXLWR,	" EXCL_WR" },
	{ 0,		NULL },
};

struct reg_desc r4k_config_desc[] = {
	/* mask	     shift      name   format  values */
	{ CONFIG_CM,	0,	"CM",	"%d",	NULL },
	{ CONFIG_EC,	0,	NULL,	NULL,	config_ec_vals },
	{ CONFIG_EP,	0,	NULL,	NULL,	config_ep_vals },
	{ CONFIG_SB,	0,	NULL,	NULL,	config_sb_vals },
	{ CONFIG_SS,	0,	NULL,	NULL,	config_ss_vals },
	{ CONFIG_SW,	0,	NULL,	NULL,	config_sw_vals },
	{ CONFIG_EW,	0,	NULL,	NULL,	config_ew_vals },
	{ CONFIG_SC,	0,	NULL,	NULL,	config_sc_vals },
	{ CONFIG_SM,	0,	NULL,	NULL,	config_sm_vals },

	{ CONFIG_BE,	0,	NULL,	NULL,	endian_values },
	{ CONFIG_EM,	0,	NULL,	NULL,	config_em_vals },
	{ CONFIG_EB,	0,	" EB",	"%d",	NULL },

	{ CONFIG_IC,	0,	NULL,	NULL,	config_ic_vals },
	{ CONFIG_DC,	0,	NULL,	NULL,	config_dc_vals },
	{ CONFIG_IB,	0,	NULL,	NULL,	config_ib_vals },
	{ CONFIG_DB,	0,	NULL,	NULL,	config_db_vals },
	{ CONFIG_CU,	0,	" CU",	"%d",	NULL },
	{ CONFIG_K0,	0,	" K0",	NULL,	cohalgo_values },
	{ 0,		0,	NULL,	NULL,	NULL },
};


#define SSTATE_INVALID	0
#define SSTATE_CLEX	4
#define SSTATE_DIRTEX	5
#define SSTATE_SHARED	6
#define SSTATE_DIRTSHAR	7


struct reg_values scache_states[] = {
	{ SSTATE_INVALID,	"inval" },
	{ SSTATE_CLEX,		"clean ex" },
	{ SSTATE_DIRTEX,	"dirty ex" },
	{ SSTATE_SHARED,	"shared" },
	{ SSTATE_DIRTSHAR,	"dirty shared" },
	{ 0,			NULL },
};

#define PSTATE_INVALID	0
#define PSTATE_SHARED	1
#define PSTATE_CLEX	2
#define PSTATE_DIRTEX	3

struct reg_values pcache_states[] = {
	{ PSTATE_INVALID,	"inval" },
	{ PSTATE_SHARED,	"shared" },
	{ PSTATE_CLEX,		"clean ex" },
	{ PSTATE_DIRTEX,	"dirty ex" },
	{ 0,			NULL },
};

#define STAG_LO		0xffffe000
#define STAG_STATE	0x00001c00
#define STAG_VINDEX	0x00000380
#define STAG_ECC	0x0000007f

struct reg_desc r4k_s_taglo_desc[] = {
	/* mask	     shift      name   format  values */
	{ STAG_LO,	4,	"lo",	"0x%x",	NULL },
	{ STAG_STATE,	-10,	NULL,	NULL,	scache_states },
	{ STAG_VINDEX,	-7,	"vind",	"0x%x",	NULL },
	{ STAG_ECC,	0,	"ecc",	"0x%x", NULL },
	{ 0,		0,	NULL,	NULL,	NULL },
};

#define PTAG_LO		0xffffff00
#define PTAG_STATE	0x000000c0
#define PTAG_PARITY	0x00000001

struct reg_desc r4k_p_taglo_desc[] = {
	/* mask	     shift      name   format  values */
	{ PTAG_LO,	4,	"lo",	"0x%x",	NULL },
	{ PTAG_STATE,	-6,	NULL,	NULL,	pcache_states },
	{ PTAG_PARITY,	0,	"parity","%x",	NULL },
	{ 0,		0,	NULL,	NULL,	NULL },
};


/* display format of the CacheErr register */
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
	{ CACHERR_SIDX_MASK,	0,	"SIDX",	"0x%x",	NULL },
	{ CACHERR_PIDX_MASK,	CACHERR_PIDX_SHIFT,	"PIDX",	"0x%x",	NULL },
	{ 0,			0,	NULL,	NULL,	NULL },
};

#endif /* R4000 */

#if R10000
struct reg_desc pcache_err_desc[] = {
	/* mask		     shift      name   format  values */
	{ CACHERR_EW,		0,	"EW",	NULL,	NULL },
	{ CACHERR_EE,		0,	"EE",	NULL,	NULL },
	{ CACHERR_D,		-26,	"D",	"%#x",	NULL },
	{ CACHERR_TA,		-24,	"TA",	"%#x",	NULL },
	{ CACHERR_TS,		-22,	"TS",	"%#x",	NULL },
	{ CACHERR_TM,		-20,	"TM",	"%#x",	NULL },
	{ CACHERR_PIDX_MASK,	0,	"PIDX",	"%#x",	NULL },
	{ 0,			0,	NULL,	NULL,	NULL },
};

struct reg_desc scache_err_desc[] = {
	/* mask		     shift      name   format  values */
	{ CACHERR_EW,		0,	"EW",	NULL,	NULL },
	{ CACHERR_D,		-26,	"D",	"%#x",	NULL },
	{ CACHERR_TA,		-24,	"TA",	"%#x",	NULL },
	{ CACHERR_SIDX_MASK,	0,	"SIDX",	"%#x",	NULL },
	{ 0,			0,	NULL,	NULL,	NULL },
};

struct reg_desc sysad_err_desc[] = {
	/* mask		     shift      name   format  values */
	{ CACHERR_EW,		0,	"EW",	NULL,	NULL },
	{ CACHERR_EE,		0,	"EE",	NULL,	NULL },
	{ CACHERR_D,		-26,	"D",	"%#x",	NULL },
	{ CACHERR_SA,		0,	"SysAdd",NULL,	NULL },
	{ CACHERR_SC,		0,	"SysCmd",NULL,	NULL },
	{ CACHERR_SR,		0,	"SysRsp",NULL,	NULL },
	{ CACHERR_SIDX_MASK,	0,	"SIDX",	"%#x",	NULL },
	{ 0,			0,	NULL,	NULL,	NULL },
};

static struct reg_values p_taglo_state[] = {
	{ CTP_STATE_I,	"invalid" },
	{ CTP_STATE_S,	"shared" },
	{ CTP_STATE_CE,	"clean" },
	{ CTP_STATE_DE,	"dirty" },
	{ 0,		NULL	}
};
static struct reg_values p_sm_vals[] = {
	{ CTP_STATEMOD_N,	"normal" },
	{ CTP_STATEMOD_I,	"inconsistant" },
	{ CTP_STATEMOD_R,	"refill" },
	{ 0,			NULL }
};
struct reg_desc p_taglo_desc[] = {
	/* mask	     		shift			name	format  vals */
	{ CTP_TAGPARITY_MASK,	-CTP_TAGPARITY_SHFT,	"Par",	"%d", 	NULL },
	{ CTP_SCW,		-1,			"SWay",	"%d",	NULL },
	{ CTP_STATEPARITY_MASK,	-CTP_STATEPARITY_SHFT,	"SPar",	"%d",	NULL },
	{ CTP_LRU,		-3,			"LRU",	"%d",	NULL },
	{ CTP_STATE_MASK,	-CTP_STATE_SHFT,"ST",NULL,p_taglo_state},
	{ CTP_TAG_MASK,		4,	"PHYS",	"%#x",	NULL },
	{ CTP_STATEMOD_MASK,	-CTP_STATEMOD_SHFT,	"MOD",	NULL,p_sm_vals},
	{ 0,		0,	NULL,	NULL,	NULL },
};
struct reg_desc s_taglo_desc[] = {
	/* mask	     shift      name   format  values */
	{ CTS_ECC_MASK,	0,	"ECC",	"%#x",	NULL },
	{ CTS_VIDX_MASK,-CTS_VIDX_SHFT,	"VIDX", "%#x",	NULL },
	{ CTS_STATE_MASK,-CTS_STATE_SHFT, "ST", NULL, p_taglo_state },
	{ CTS_TAG_MASK, 10,	"PHYS",	"%#x",	NULL },
	{ CTS_MRU,	0,	"MRU",	NULL,	NULL },
	{ 0,		0,	NULL,	NULL,	NULL },
};

static struct reg_values pcachesize[] = {
	{ 0x0,	"4KB" },
	{ 0x1,	"8KB" },
	{ 0x2,	"16KB" },
	{ 0x3,	"32KB" },
	{ 0x4,	"64KB" },
	{ 0x5,	"128KB" },
	{ 0x6,	"256KB" },
	{ 0x7,	"512KB" },
	{ 0,	NULL },
};

static struct reg_values scclkdiv[] = {
	{ 0x1,	"1" },
	{ 0x2,	"1.5" },
	{ 0x3,	"2" },
	{ 0x4,	"2.5" },
	{ 0x5,	"3" },
	{ 0,	NULL },
};

static struct reg_values scachesize[] = {
	{ 0x0,	"512KB" },
	{ 0x1,	"1MB" },
	{ 0x2,	"2MB" },
	{ 0x3,	"4MB" },
	{ 0x4,	"8MB" },
	{ 0x5,	"16MB" },
	{ 0,	NULL },
};

static struct reg_values sysclkdiv[] = {
	{ 0x1,	"1" },
	{ 0x2,	"1.5" },
	{ 0x3,	"2" },
	{ 0x4,	"2.5" },
	{ 0x5,	"3" },
	{ 0x6,	"3.5" },
	{ 0x7,	"4" },
	{ 0,	NULL },
};

static struct reg_values k0seg[] = {
	{ 0x2,	"Uncached" },
	{ 0x3,	"Cacheable Noncoherent" },
	{ 0x4,	"Cacheable Coherent Exclusive" },
	{ 0x5,	"Cacheable Coherent Exclusive On Write" },
	{ 0x7,	"Uncached Accelerated" },
	{ 0,	NULL },
};

struct reg_desc config_desc[] = {
	/* mask		shift	name			format  values */
	{ CONFIG_IC,	-CONFIG_IC_SHFT, "ICacheSize",	NULL,	pcachesize },
	{ CONFIG_DC,	-CONFIG_DC_SHFT, "DCacheSize",	NULL,	pcachesize },
	{ CONFIG_SC,	-CONFIG_R10K_SC_SHFT, "SCClkDiv", NULL,	scclkdiv },
	{ CONFIG_SS,	-CONFIG_R10K_SS_SHFT, "SCSize",	NULL,	scachesize },
	{ CONFIG_BE,	0,	"BE",			NULL,	NULL },
	{ CONFIG_R10K_SK, 0,		 "SK",		NULL,	NULL },
	{ CONFIG_SB,	0,	"SB",			NULL,	NULL },
	{ CONFIG_EC,	-CONFIG_R10K_EC_SHFT, "SysClkDiv", NULL, sysclkdiv },
	{ CONFIG_R10K_PM,-CONFIG_R10K_PM_SHFT, "PrcReqMax", "%d", NULL },
	{ CONFIG_R10K_PE,0,		 "PE",		NULL,	NULL },
	{ CONFIG_R10K_CT,	0,	"CT",		NULL,	NULL },
	{ CONFIG_R10K_DN,-CONFIG_R10K_DN_SHFT, "DevID",	"%d",	NULL },
	{ CONFIG_K0,	0,	"K0seg",		NULL,	k0seg },
	{ 0,		0,	NULL,			NULL,	NULL },
};
#endif /* R10000 */
