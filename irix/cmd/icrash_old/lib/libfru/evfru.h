
#ifndef _KERNEL

#define _KERNEL	1
#define _K64U64 1
#include <sys/types.h>
#undef _KERNEL

#include <sys/EVEREST/IP19addrs.h>      /* For evconfig, everror addresses */

#else

#include <sys/types.h>
#include <sys/cpu.h>

#endif /* _KERNEL */

#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evconfig.h>

/*
 * FRU Classes
 */

#define FRU_CLASSMASK	0x0f00

#define FRU_CLASS(_x)	((_x) & FRU_CLASSMASK)

/* Classes must be numbered from 0x100 to 0xf00 */
#define FRUC_NONE	0x000
#define	FRUC_SOFTWARE	0x100
#define FRUC_BOARD	0x200
#define FRUC_EBUS_ASIC	0x300
#define FRUC_MEM_BANK	0x400
#define FRUC_IOA	0x500
#define FRUC_FCIDEV	0x600
#define FRUC_CPUSLICE	0x700
#define FRUC_VMEDEV	0x800
#define FRUC_ONEOF	0x900

/* This flag means that we think the error was caused by software. */
#define FRUF_SOFTWARE	0x1000

/* This flag means that the "FRU" can be replaced; it's _really_ a FRU.
 * We're not using this yet, but I hope to.
 */
#define FRUF_CANREPLACE	0x2000

/*
 * FRU Type Designations
 */

#define FRU_TYPEMASK	0x00ff

/* FRU types are a FRU class and a type number (from 1 to 0xff) */
#define	FRU_NONE	(0x00 | FRUC_NONE)
#define FRU_SOFTWARE	(0x01 | FRUC_SOFTWARE)
#define FRU_UNKNOWN	(0x02 | FRUC_NONE)

/* CPU Board */
#define FRU_IPBOARD	(0x10 | FRUC_BOARD)
#define FRU_A		(0x11 | FRUC_EBUS_ASIC)
#define FRU_D		(0x12 | FRUC_EBUS_ASIC)
#define FRU_CPU		(0x13 | FRUC_CPUSLICE)
#define FRU_CACHE	(0x14 | FRUC_CPUSLICE)
#define FRU_CC		(0x15 | FRUC_CPUSLICE)
#define FRU_BBCC	(0x16 | FRUC_CPUSLICE)
#define FRU_TAGRAM	(0x17 | FRUC_CPUSLICE)
#define FRU_GCACHE	(0x18 | FRUC_CPUSLICE)
#define FRU_DB		(0x19 | FRUC_EBUS_ASIC)	/* XXX - Is this per slice? */
#define FRU_BUSTAG	(0x1a | FRUC_CPUSLICE)
#define FRU_FPU		(0x1b | FRUC_CPUSLICE)

#define FRU_SCACHE	(0x1c | FRUC_CPUSLICE)
#define FRU_PCACHE	(0x1d | FRUC_CPUSLICE)
#define FRU_SYSAD	(0x1e | FRUC_CPUSLICE)

/* Memory Board */
#define FRU_MCBOARD	(0x20 | FRUC_BOARD)
#define FRU_MA		(0x21 | FRUC_EBUS_ASIC)
#define FRU_MD		(0x22 | FRUC_EBUS_ASIC)
#define FRU_BANK	(0x23 | FRUC_MEM_BANK)
#define FRU_SIMM	(0x24 | FRUC_MEM_BANK)

/* IO Board */
#define FRU_IOBOARD	(0x30 | FRUC_BOARD)
#define FRU_IA		(0x31 | FRUC_EBUS_ASIC)
#define FRU_ID		(0x32 | FRUC_EBUS_ASIC)
#define FRU_MAPRAM	(0x33 | FRUC_EBUS_ASIC)
#define FRU_EPC		(0x34 | FRUC_IOA)
#define FRU_S1		(0x35 | FRUC_IOA)
#define FRU_SCIP	(0x36 | FRUC_IOA)
#define FRU_DANG	(0x3a | FRUC_IOA)
#define FRU_F		(0x37 | FRUC_IOA)
#define FRU_VMECC	(0x38 | FRUC_FCIDEV)
#define FRU_FCG		(0x39 | FRUC_FCIDEV)
#define FRU_HIPPI	(0x3b | FRUC_FCIDEV)

/* One of */
#define FRU_ANIOA	(0x40 | FRUC_ONEOF)

/* VME board */
#define FRU_VMEDEV	(0x50 | FRUC_VMEDEV)

#define FRU_MASK	(FRU_CLASSMASK | FRU_TYPEMASK)

#define FRU_TYPE(_x)	((_x) & FRU_TYPEMASK)

/*
 * Special unit numbers
 */

#define NO_UNIT	-1

/*
 * FRU element data structure.
 */

typedef struct fru_element_s {
	short unit_type;	/* FRU ID */
	short unit_num;		/* Depends on FRU ID. */
	long long unit_address;	/* Address if applicable (e.g. VME) */
} fru_element_t;


/* Confidence Levels */

#define NOT_INVOLVED	0
#define WITNESS		1
#define FLAKY_HW	2
#define POSSIBLE	3
#define PROBABLE	4
#define DEFINITE	5
#define FULL_MATCH	6

#define NUM_CONFIDENCE_LEVELS	7

/*
 * All information about the highest confidence error on each Ebus slot is here.
 */
typedef struct whatswrong_s {
	int slot_num;		/* Ebus slot */
	int board_type;		/* IP19, IP21, MC3, IO4 */
	int confidence;		/* See above */
	int case_id;		/* Only valid if the FRU is a pattern match */
	fru_element_t src;	/* We really don't distinguish between */
	fru_element_t dest;	/*  src and dest, but they're the endpoints. */
} whatswrong_t;

#define NO_ELEM	(fru_element_t *)0

/*
 * icrash requires some special printing functions.  If we're in icrash rather
 * than the kernel, include that stuff here.  Otherwise, redefine FRU_PRINTF
 * as printf.
 */
#ifdef ICRASH
#include "icrash_fru.h"
#else

#include "sys/systm.h"

#define FRU_PRINTF	fru_printf
#define FRU_INDENT	""
#endif

/* Exported functions */
extern void display_whatswrong(whatswrong_t *ww, evcfginfo_t *ec);
extern update_confidence(whatswrong_t *ww, int level,
			 fru_element_t *src, fru_element_t *dest);
extern conditional_update(whatswrong_t *ww, int level, int data, int mask, 
			  fru_element_t *src, fru_element_t *dest);
extern void decode_fru(fru_element_t *element, evcfginfo_t *ec);
extern void check_ip19(evbrdinfo_t *, everror_t *, everror_ext_t *, 
		   whatswrong_t *,	evcfginfo_t *);
extern void check_ip21(evbrdinfo_t *, everror_t *, everror_ext_t *, 
		   whatswrong_t *,	evcfginfo_t *);
extern void check_mc3(evbrdinfo_t *, everror_t *, everror_ext_t *, 
		  whatswrong_t *, evcfginfo_t *);
extern void check_io4(evbrdinfo_t *, everror_t *,  everror_ext_t *, 
		  whatswrong_t *, evcfginfo_t *);
extern char bank_letter(unsigned int leaf, unsigned int bank);

extern fru_analyzer(everror_t *, everror_ext_t *, evcfginfo_t *, int);

extern everest_fru_analyzer(everror_t *, everror_ext_t *, evcfginfo_t *,
		        int, void (*)(char *, ...));

extern void fru_printf(char *, ...);

/* Global variables */
#ifdef DEBUG
extern int fru_debug;	/* Print debugging information */
#endif

extern int fru_ignore_sw;	/* Ignore the software flag. */

extern int in_bank(uint, uint, uint, uint, uint, uint);

