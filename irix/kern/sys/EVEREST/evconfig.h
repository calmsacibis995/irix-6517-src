/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1994, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * evconfig.h -- Contains information about various bits of Everest system 
 *		 hardware.  This data is acquired by the PROM and
 *		 is passed up to the kernel and standalone programs
 *		 through the data structures described in this file.
 * 
 * NOTE:
 *	If diagval == 0x00, then component passed diags.
 *	If diagval == 0xff, the component is non-existent.
 *
 *	 For all other values of diagval, component was found but
 *	   failed diagnostics.  It is possible that diagval is
 *	   non-zero but that a component is still enabled.  In 
 *	   this case, only part of a component (such as one of the
 *	   uarts on the EPC) has failed.  The diagval should be 
 * 	   sufficient to determine which sub-component failed in this
 *	   case. 
 *
 * WARNING:
 *	Certain assembly language routines (notably slave.s) in the PROM 
 *	depend on the format of this data.  In some cases, rearranging 
 *	the fields can seriously break things.   
 */

#ifndef __SYS_EVEREST_EVCONFIG_H__
#define __SYS_EVEREST_EVCONFIG_H__

#ident "$Revision: 1.43 $"

#ifdef _LANGUAGE_C
#include <sys/types.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/io4.h>
#else
#include <sys/EVEREST/everest.h>
#endif /* _LANGUAGE_C */

#include <sys/EVEREST/evdiag.h>

#include <sys/EVEREST/evaddrmacros.h>

#define EVCFGINFO_MAGIC	0xdeadbabe

#define BLOC_SIZE	256
#define BLOCs_to_pages(x) ((x)>>(4+PGSHFTFCTR))
#define SERIALNUMSIZE 11


/*
 * Offsets in NVRAM used to retrieve configuration information.
 */

#define INV_MAXUNITS    8 
#define INV_UNITSIZE	3
#define INV_SIZE        (4 + (INV_MAXUNITS * INV_UNITSIZE))

#define INV_TYPE        0
#define INV_REV         1
#define INV_DIAGVAL     2
#define INV_ENABLE      3
#define INV_UNITOFF	4

#define INVU_TYPE	0
#define INVU_DIAGVAL	1
#define INVU_ENABLE	2

/*
 * Everest board types and classes
 */
#define EVCLASS_MASK	0xf0
#define EVCLASS_NONE	0x00
#define EVCLASS_CPU	0x10
#define EVCLASS_IO	0x20
#define EVCLASS_MEM	0x30
#define EVCLASS_UNKNOWN	0xf0

#define EVCLASS(_x) ((_x) & EVCLASS_MASK)

#define EVTYPE_MASK	0x0f
#define EVTYPE_NONE	0x00
#define EVTYPE_EMPTY	0x00

#define EVTYPE_WEIRDCPU (EVCLASS_CPU | 0x0)
#define EVTYPE_IP19	(EVCLASS_CPU | 0x1)
#define EVTYPE_IP21	(EVCLASS_CPU | 0x2)
#define EVTYPE_IP25	(EVCLASS_CPU | 0x3)

#define EVTYPE_WEIRDIO	(EVCLASS_IO  | 0x0)
#define EVTYPE_IO4	(EVCLASS_IO  | 0x1)
#define EVTYPE_IO5	(EVCLASS_IO  | 0x2)

#define EVTYPE_WEIRDMEM (EVCLASS_MEM | 0x0)	
#define EVTYPE_MC3	(EVCLASS_MEM | 0x1)

#define EVTYPE_UNKNOWN	0xff

#define EVTYPE(_x) 	((_x) & EVTYPE_MASK)
/*
 * Standard diagnostic return values - all others indicate errors.
 */
#define EVDIAG_PASSED	0x0
#define EVDIAG_TBD	0xfd
#define EVDIAG_NOT_SET	0xfe
#define EVDIAG_NOTFOUND	0xff

#define EVINV_DISABLED	1

#define CPUST_NORESP	((unchar) 0xff)

#ifdef _LANGUAGE_C

/*
 * The evioacfg_t structure type contains configuration information
 * for the IO adapters on IO4 boards.  If you add fields to this 
 * structure, make sure you update the BRDINFO_SIZE definition in the 
 * Assembler ifdef below.
 */
typedef struct evioacfg_s {
    uint	pad;
    unchar	ioa_enable;	/* Flag indicating whether IOA is enabled */
    unchar	ioa_inventory;	/* Previous type info from NVRAM inventory */
    unchar	ioa_diagval;	/* Results of diagnostic for this IOA */
    unchar	ioa_type;	/* The adapter type (see io4.h) */
    unchar	ioa_virtid;	/* The virtual ID for this type */
    unchar	ioa_subtype;	/* The ID of the device on flat cable */
} evioacfg_t; 


/*
 * The evbnkcfg_t structure type contains configuration information
 * for the banks on MC3 boards.  If you add fields to this structure,
 * make sure you update the BRDINFO_SIZE definition in the Assembler
 * ifdef below.
 */
#define bnk_used bnk_ip
#define bnk_next bnk_if
typedef struct evbnkcfg_s {
    uint	bnk_bloc;	/* Bloc num of memory range containing bank */
    unchar	bnk_slot;	/* Slot of this bank */
    unchar	bnk_enable;	/* Flag indicating bank is enabled */
    unchar	bnk_inventory;	/* Previous type info from NVRAM inventory */
    unchar	bnk_diagval;	/* Results of diagnostics for the bank */
    unchar	bnk_ip;		/* Bank interleave position */
    unchar	bnk_if;		/* Bank interleave factor */
    unchar      bnk_size;	/* Size of bank */
    unchar	bnk_count;	/* Number of banks in this interleave */
} evbnkcfg_t;

    
/*
 * The evcpucfg_t structure contains configuration information 
 * for the cpus on the IP19 and IP21 boards.  If you add fields
 * to this structure, make sure you update both the BRDINFO_SIZE and
 * CPUCFG_SIZE efinitions in the Assember ifdef below.
 */

typedef struct evcpucfg_s {
    /* These two fileds are defined as signed integers so that their
     * values will be properly sign-extended into 64-bit compatibility-mode
     * addresses on R4x00s running with trhe KX bit on. For TFP, we must
     * synthesize KPhys addresses.
     */
    int		cpu_launch;	/* Launch address */
    int		cpu_parm;	/* Launch parameter */
    unchar	cpu_info;	/* Processor status when cpu_diagval is zero,
				 * Diagnostic info when cpu_diagval is set
				 */
    unchar	cpu_enable;	/* Flag indicating CPU is enabled */
    unchar 	cpu_inventory;	/* Bits indicating inventory status */
    unchar	cpu_diagval;	/* Results of diagnostic for this CPU */
    unchar	cpu_vpid;	/* Virtual processor ID of the CPU */
    unchar	cpu_speed;	/* Processor's frequency (in MHz) */
    unchar	cpu_cachesz;	/* log2(Secondary cache size) */
    unchar	cpu_promrev;	/* CPU prom revision */
} evcpucfg_t;

/*
 * The evbrdinfo_t structure contains all pertinent information for
 * a given board in the system.  It contains a union which has structures
 * for each of the major board types.  DO NOT ADD FIELDS BEFORE THE UNION.
 * THE ASSEMBLY LANGUAGE DEFINES DEPEND ON IT BEING THE FIRST ELEMENT IN
 * THE STRUCTURE.
 */

#define eb_io		eb_un.ebun_io
#define eb_ioarr	eb_un.ebun_io.eb_ioas
#define eb_cpu  	eb_un.ebun_cpu
#define eb_cpuarr	eb_un.ebun_cpu.eb_cpus
#define eb_mem		eb_un.ebun_mem
#define eb_banks	eb_un.ebun_mem.ebun_banks

typedef struct evbrdinfo_s {
    union {			/* Union containing board info structures */
	struct {
	    evioacfg_t	eb_ioas[IO4_MAX_PADAPS];  /* IO adapter information */
	    unchar	eb_winnum;		  /* IO window assigned */
	} ebun_io;

	struct {
	    evbnkcfg_t	ebun_banks[MC3_NUM_BANKS]; /* Bank information */
	    unchar	eb_mc3num;		   /* virtual mc3 id assigned */
	} ebun_mem;

	struct {
            evcpucfg_t	eb_cpus[EV_MAX_CPUS_BOARD];/* Processor information */ 
	    unchar	eb_cpunum;
	    unchar	eb_numcpus;		   /* Number of CPUS on board */
	    unchar	eb_ccrev;		   /* Cache controller rev. */
	    unchar	eb_brdflags;		   /* Board config flags. */
	} ebun_cpu;
    } eb_un;

    unchar	eb_type;	/* Type of board in this slot */
    unchar	eb_rev;		/* Board revision (if available) */
    unchar	eb_enabled;	/* Flag indicating board is enabled */
    unchar	eb_inventory;	/* Previous type info from NVRAM inventory */
    unchar	eb_diagval;	/* Results of general board diags */
    unchar	eb_slot;	/* Slot containing this board */
} evbrdinfo_t;

#ifdef MULTIKERNEL

typedef struct evcellinfo_s {
	uint	cell_membase;	/* block number of memory base for cell */
	uint	cell_memsize;	/* number of blocks of memory in cell */
} evcellinfo_t;

#endif /* MULTIKERNEL */

/*
 * The following are flags that can be set in eb_brdflags:
 */
#define EV_BFLAG_PIGGYBACK	0x01	/* Piggyback reads are enabled. */

/*
 * The evcfginfo_t structure contains all of the configuration information
 * the kernel needs to manage an Everest system.  An instance of this struct 
 * exists at a fixed address in memory (see EVCFGINFO) and is initialized by 
 * the Everest PROM.  DO NOT ADD FIELDS BEFORE ecfg_board or ecfg_debugsw!
 * THE ASSEMBLY LANGUAGE DEPENDS ON THE IMPLICIT ORDERINGS OF THIS DATA. 
 */
	
typedef struct evcfginfo_s {
    evbrdinfo_t	ecfg_board[EV_MAX_SLOTS]; 
    uint	ecfg_magic;	/* Magic number for the cfg structure */
    uint	ecfg_secs;	/* Clock start time (in secs since YRREF) */
    uint	ecfg_nanosecs;	/* Clock start time (remainder in ns) */
    uint	ecfg_clkfreq;	/* Bus clock frequency (in Hz) */
    uint 	ecfg_memsize;	/* Memory size, in 256-byte blocs */
    uint	ecfg_epcioa;	/* IO Adapter of master EPC */
    uint	ecfg_debugsw;	/* Value of the "virtual DIP switches" when
				   the system was powered on. */
    char	ecfg_snum[SERIALNUMSIZE];  /* Everest serial number */
#ifdef MULTIKERNEL
	/* The following fields are setup by kernel code early in
	 * the initialization process in order to load and execute
	 * multiple kernels.
	 */
    char	ecfg_numcells;	/* number of configured cells */
    char	ecfg_cellmask;	/* mask to apply to cpu_vpid to get cellid */
    evcellinfo_t ecfg_cell[EV_MAX_CELLS];
#endif /* MULTIKERNEL */
} evcfginfo_t;

#define YRREF 1970		/* clock relative to 01/01/1970 */
	
#define BOARD(_x) ((evbrdinfo_t *)(EVCFGINFO_ADDR + ((_x) * BRDINFO_SIZE)))

/*
 * Create a pointer to the everest configuration information block.
 */
#define EVCFGINFO	((evcfginfo_t*)EVCFGINFO_ADDR)

/*
 * Returns logical cpuid assigned by PROM
 */
#define EVCFG_CPUID(_slot, _cpu) \
	(EVCFGINFO->ecfg_board[(_slot)].eb_cpu.eb_cpus[(_cpu)].cpu_vpid)

/*
 * Returns results of cpu diagnostics run by PROM.
 * Non-zero indicates that this CPU failed diags, and cannot be enabled.
 */
#define EVCFG_CPUDIAGVAL(_slot, _cpu) \
	(EVCFGINFO->ecfg_board[(_slot)].eb_cpu.eb_cpus[(_cpu)].cpu_diagval)

/*
 * Returns processor speed (in Mhz), according to PROM configuration 
 * information.
 */
#define EVCFG_CPUSPEED(_slot, _cpu) \
	(EVCFGINFO->ecfg_board[(_slot)].eb_cpu.eb_cpus[(_cpu)].cpu_speed)

/*
 * Returns the revision number (one byte) of the CPU PROM on the specified CPU.
 */
#define EVCFG_CPUPROMREV(_slot, _cpu) \
	(EVCFGINFO->ecfg_board[(_slot)].eb_cpu.eb_cpus[(_cpu)].cpu_promrev)

/*
 * Returns the cpu structure for the cpu specified.
 */
#define EVCFG_CPUSTRUCT(_slot, _cpu) \
	(EVCFGINFO->ecfg_board[(_slot)].eb_cpu.eb_cpus[(_cpu)])

#endif /* _LANGUAGE_C */

/* Virtual dipswitch values (starting from switch "7"): */

#define VDS_NOGFX		0x8000	/* Don't enable gfx and autoboot */
#define VDS_NOMP		0x100	/* Don't start slave processors */
#define VDS_MANUMODE		0x80	/* Manufacturing mode */
#define VDS_NOARB		0x40	/* No bootmaster arbitration */
#define VDS_PODMODE		0x20	/* Go straight to POD mode */
#define VDS_NO_DIAGS		0x10	/* Don't run any diags after BM arb */
#define VDS_DEFAULTS		0x08	/* Use default environment values */
#define VDS_NOMEMCLEAR		0x04	/* Don't run mem cfg code */
#define VDS_2ND_IO4		0x02	/* Boot from the second IO4 */
#define VDS_DEBUG_PROM		0x01	/* Print PROM debugging messages */

#define BRDINFO_SIZE		108

#ifdef _LANGUAGE_ASSEMBLY
#  define DEBUGSW_OFF		(BRDINFO_SIZE * EV_MAX_SLOTS + 24)
#  define CPUINFO_SIZE		16	
#  define CPULAUNCH_OFF		0
#  define CPUPARM_OFF		4
#  define CPUSTATUS_OFF		8	
#  define CPUENABLE_OFF		9
#  define CPUINVENT_OFF		10
#  define CPUDIAGVAL_OFF	11
#  define CPUVPID_OFF		12
#  define CPUSPEED_OFF		13 
#  define CPUCACHE_OFF		14 
#  define CPUPROMREV_OFF	15
#endif /* _LANGUAGE_ASSEMBLY */


#endif /* __SYS_EVEREST_EVCONFIG_H__ */
