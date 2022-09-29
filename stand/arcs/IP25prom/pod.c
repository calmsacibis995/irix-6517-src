/*
 * File: 	pod.c
 * Purpose:	Defines most of the routines used for pod commands.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.10 $"

#include <setjmp.h>

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>

#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/IP25.h>
#include <sys/EVEREST/IP25addrs.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/nvram.h>

#include "pod.h"
#include "pod_failure.h"
#include "ip25prom.h"
#include "prom_leds.h"
#include "prom_intr.h"
#include "prom_externs.h"
#include "cache.h"

#define NOPRINT

void	check_slaves(int, int);
void	store_gprs(struct reg_struct *);
void	scroll_n_print(unsigned char);
void	run_incache(void);
void	pon_memerror(void);
int	mc3_reconfig(evcfginfo_t *, int);
int	save_and_call(int *, uint (*function)(__scunsigned_t), __scunsigned_t);
uint	pod_jump(__scunsigned_t, uint, uint, uint);
int	*pod_parse(int *, struct reg_struct *, int, int, struct flag_struct *);

__uint64_t	get_enhi(int);
__uint64_t	get_enlo(int, int);

char *exc_names[32] =
{ "Interrupt",
  "Page Modified",
  "Load TLB Miss",
  "Store TLB Miss",
  "Load Addr Err",
  "Store Addr Err",
  "Inst Bus Err",
  "Data Bus Err",
  "System Call",
  "Breakpoint",
  "Reserved Inst",
  "Cop. Unusable",
  "Overflow",
  "Trap",
  "-",
  "Float Pt. Exc",
  "-",
  "-",
  "-",
  "-",
  "-",
  "-",
  "-",
  "Watchpoint",
  "-",
  "-",
  "-",
  "-",
  "-",
  "-",
  "-",
  "-"
};


char *ertoip_names[] = {
  "Not Defined",				/* 0 */
  "Double bit ECC error write/intervention CPU",/* 1 */
  "Single bit ECC error write/intervention CPU",/* 2 */
  "Parity error on Tag RAM data",		/* 3 */
  "A Chip Addr Parity",				/* 4 */
  "MyRequest timeout on EBus",			/* 5 */
  "A Chip MyResponse D-Resource time out",	/* 6 */
  "A Chip MyIntrvention D-Resource time out", 	/* 7 */
  "Addr Error on MyRequest on Ebus",		/* 8 */
  "My Ebus Data Error",				/* 9 */
  "Intern Bus vs. A_SYNC",			/* 10 */
  "CPU bad data indication",			/* 11 */
  "Parity error on data from D-chip [15:0]",	/* 12 */
  "Parity error on data from D-chip [31:16]",	/* 13 */
  "Parity error on data from D-chip [47:32]",	/* 13 */
  "Parity error on data from D-chip [63:48]",	/* 14 */
  "Double bit ECC error CPU SYSAD/Command",	/* 15 */
  "Single bit ECC error CPU SYSAD/Command",	/* 16 */
  "SysState parity error",			/* 17 */
  "SysCmd parity error",			/* 18 */
  "Multiple Errors detected",			/* 19 */
  "Not defined",				/* 20 */
  "Not defined"					/* 21 */
  "Not defined"					/* 22 */
  "Not defined"					/* 23 */
  "Not defined"					/* 24 */
  "Not defined",				/* 25 */
  "Not defined"					/* 26 */
  "Not defined"					/* 27 */
  "Not defined"					/* 28 */
  "Not defined"					/* 29 */
  "Not defined"					/* 30 */
  "Not defined"					/* 31 */
};


#define READING 0
#define WRITING 1

static void
xlate_cause(__scunsigned_t value)
{
    __scunsigned_t exc_code, int_bits;
    int i;

    exc_code = (value & CAUSE_EXCMASK) >> CAUSE_EXCSHIFT;
    int_bits = (value & CAUSE_IPMASK) >> CAUSE_IPSHIFT;
    loprintf("( INT:");
    for (i = 8; i > 0; i--) {
	if ((1 << (i - 1)) & int_bits)
	    loprintf("%d", i);
	else
	    loprintf("-");
    }
    if (exc_code)
	loprintf(" <%s> )\n", exc_names[exc_code]);
    else
	loprintf(" )\n");
}

void
xlate_ertoip(evreg_t value)
{
    int i;

    if (!value)
	return;

    loprintf("*** Error/TimeOut Interrupt(s) Pending: %x ==\n", value);

    for (i = 0; i < (sizeof(ertoip_names) / sizeof(ertoip_names[0])); i++)
	if (value & (1 << i))
	    loprintf("\t %s\n", ertoip_names[i]);
}


/* pod_loop-
 *	dex: 1 == cache has been set up dirty-exclusive.
 *	     0 == stack is in memory	
 *	diagval: Contains a code explaining why we're here.
 *		see pod_failure.h
 */
void pod_loop(int dex, int diagval)
{
    int buf[LINESIZE];
    struct reg_struct gpr_space, *gprs = &gpr_space;
    evreg_t ertoip;
    struct flag_struct flags;
    char diag_string[80];


    sc_disp(diagval);

    /* Get the GPR values from the GPRs and FPRs (FPRs contain the values
       that were in the GPRs when Pod_handler was called). */
    store_gprs(gprs);

    /* Tell the world we've reached the POD loop
     */
    set_cc_leds(PLED_PODLOOP);

    flags.slot  = (char)((LD(EV_SPNUM) & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT);
    flags.slice = (char)((LD(EV_SPNUM) & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);

    flags.selected = 0xff;		/* No one's selecetd */
    flags.silent = 0;			/* Normal verbnosity level */

    decode_diagval(diag_string, diagval);
    flags.diag_string = diag_string;
    flags.diagval = diagval;

    if (dex) {
	/* We're running with stack in dirty-exclusive cache lines */
	flags.mem = 0;
    } else {
	/* We're running with stack in memory */
	flags.mem = 1;
    }

    if(diagval != EVDIAG_DEBUG) {
	loprintf("  Cause = %x ", gprs->cause);
	xlate_cause(gprs->cause);
    }

    if (ertoip = LD(EV_ERTOIP))
	xlate_ertoip(ertoip);

    loprintf("Reason for entering POD mode: %s\n", diag_string);

    /* Display the long message the first time. */
    flags.scroll_msg = 1;
 
    if (diagval != EVDIAG_DEBUG) {
	loprintf("Press ENTER to continue.\n");	
	pod_flush();
	scroll_n_print(diagval);
	pod_getc();
    }

    for (;;) {
	if (dex) {
	    loprintf("POD %b/%b> ", flags.slot, flags.slice);
	} else {
	    loprintf("Mem %b/%b> ", flags.slot, flags.slice);
	}
	logets(buf, LINESIZE);
	pod_parse(buf, gprs, 0, 0, &flags);
	if (ertoip = LD(EV_ERTOIP)) {
	    xlate_ertoip(ertoip);
	}
    }
}

/* decode_diagval-
 *	This is _very_ preliminary code for coming up with real
 *		messages to display on the system controller.
 *	We need to do _much_ better.
 *	POD will need another parameter to describe where the error
 *		occurred.
 */
void 
decode_diagval(char *diag_string, int diagval)
{
	/* We need to get FRU info, location, etc. */

	lo_strcpy(diag_string, get_diag_string(EVDIAG_DIAGCODE(diagval)));

}

/* set_unit_enable-
 *	Enables or disables a unit in a particular slot in the evconfig
 * 		structure.
 */
int 
set_unit_enable(uint slot, uint unit, uint enable, uint force)
{
    evbrdinfo_t *brd;
    int conf_reg;
    int myslot;

    if (unit > 7) {
	loprintf("*** Unit out of range!\n");
	return -1;
    }

    brd = &(EVCFGINFO->ecfg_board[slot]);
    switch((brd->eb_type) & EVCLASS_MASK) {
    case EVCLASS_NONE:
	/*
	 * It may be too early in the boot process for us to
	 * have initialized the evcfg table or we may be really
	 * confused. If user is trying to enable the current cpu
	 * board, let him do it.
	 */
	myslot = (int)((LD_LO(EV_SPNUM)&EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT);
	if (myslot == slot)
	    goto valid_cpu;

	loprintf("*** Slot %b is empty\n", slot);
	return -1;
    case EVCLASS_CPU:
    valid_cpu:
	if (unit >= EV_CPU_PER_BOARD) {
	    loprintf("*** Unit out of range!\n");
	    return -1;
	}
	if ((brd->eb_cpuarr[unit].cpu_diagval ==
	     CPUST_NORESP) && !force) {
	    loprintf("*** CPU not populated\n");
	    return -1;
	}
	brd->eb_cpuarr[unit].cpu_enable = enable;
	loprintf("CPU %b/%b", slot, unit);
	if (force) {
	    conf_reg = (int)*EV_CONFIGADDR(slot, 0, EV_A_ENABLE);
	    if (enable)
		conf_reg |= 1 << unit;
	    else
		conf_reg &= ~(1 << unit);
	    conf_reg &= 0xf;
	    EV_SET_CONFIG(slot, EV_A_ENABLE, conf_reg);
	}
	break;
    case EVCLASS_IO:
	if ((brd->eb_ioarr[unit].ioa_type == 0) && !force) {
	    loprintf("*** IOA not populated\n");
	    return -1;
	}
	brd->eb_ioarr[unit].ioa_enable = enable;
	switch(brd->eb_ioarr[unit].ioa_type) {
	case IO4_ADAP_SCSI:
	    loprintf("SCSI %b/%b", slot,
		     unit);
	    break;
	case IO4_ADAP_EPC:
	    loprintf("EPC %b/%b", slot,
		     unit);
	    break;
	case IO4_ADAP_FCHIP:
	    loprintf("F chip %b/%b", slot,
		     unit);
	    break;
	default:
	    loprintf("Device in %b/%b",
		     slot, unit);
	    break;
	}
	if (force) {
	    if (enable) {
		if (unit > 3)
		    conf_reg = (int)EV_GET_CONFIG(slot, IO4_CONF_IODEV1);
		else
		    conf_reg = (int)EV_GET_CONFIG(slot, IO4_CONF_IODEV0);
		brd->eb_ioarr[unit].ioa_type =
		    (conf_reg >> ((unit & 3) * 8)) & 0xff;
	    } else {
		brd->eb_ioarr[unit].ioa_type = 0;
	    }
	}
	break;
    case EVCLASS_MEM:
	if ((brd->eb_banks[unit].bnk_size == MC3_NOBANK) &&
	    !force){
	    loprintf("*** Bank not populated\n");
	    return -1;
	}
	brd->eb_banks[unit].bnk_enable = enable;
	loprintf("Slot %b, bank %b", slot, unit);
	if (force) {
	    if (enable) {
		brd->eb_banks[unit].bnk_size = (unsigned char)
		    EV_GET_CONFIG(slot, MC3_BANK((unit >> 2),(unit & 3), 
						 BANK_SIZE));
	    } else {
		brd->eb_banks[unit].bnk_size =
		    MC3_NOBANK;
	    }
	}
	break;
    default:
	loprintf("*** Unknown board class\n");
	break;
    }
    if (force)
	loprintf(" forcibly");
    if (enable)
	loprintf(" enabled.\n");
    else
	loprintf(" disabled.\n");
    return 0;
}

void
pod_reconf_mem(void)
{
    evcfginfo_t evconfig;
    int diagval;
    int c;
    unsigned intlv_type;
    int slot, slice;

    loprintf("Enable interleaving (y/n)? ");
    pod_flush();
    c = pod_getc();
	
    if (c == 'y' || c == 'Y') {
	loprintf("Yes.\n");
	intlv_type = INTLV_STABLE;
    } else {
	loprintf("No.\n");
	intlv_type = INTLV_ONEWAY;
    }

    loprintf("Copying configuration information into cache\n");
    evconfig = *EVCFGINFO;
	
    loprintf("Reconfiguring memory");
    if (intlv_type == INTLV_ONEWAY)
	loprintf(" (Interleaving disabled)\n");
    else
	loprintf("\n");
 
    if(diagval = mc3_reconfig(&evconfig, intlv_type))
	loprintf(" *** %s\n", get_diag_string(diagval));
    else
	loprintf("Memory reconfigured\n");

    loprintf("Restoring configuration information\n");

    *EVCFGINFO = evconfig;

    loprintf("Recreating MPCONF blocks ");
    if (evconfig.ecfg_debugsw & VDS_NO_DIAGS)
	loprintf(" - NO DIAGS mode\n");
    else
	loprintf(" and waiting for slaves.\n");

    /* Set up mpconf structure and send interrupt to slaves. */
    init_mpconf();
    GDA->g_vds = evconfig.ecfg_debugsw;
    if (!(evconfig.ecfg_debugsw & VDS_NO_DIAGS)) {
	slot =  (int)((LD_LO(EV_SPNUM) & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT);
	slice = (int)((LD_LO(EV_SPNUM) & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);
	check_slaves(slot, slice);
    }
}

void
pod_bist(void)
{
    evcfginfo_t evconfig;
    int diagval;
    unsigned int mem, i, time, timeout;
    int slot, slice;

    mem = memory_slots();

    loprintf("Copying configuration information into cache\n");
    evconfig = *EVCFGINFO;
	
    loprintf("Running BIST\n");

    for (i = 0; i < EV_MAX_SLOTS; i ++) {
	if (!((1 << i) & mem)) {
	    continue;
	}
	loprintf("Starting BIST on slot %b\n", i); 
	EV_SET_CONFIG(i, MC3_BISTRESULT, 0xf);
	EV_SET_CONFIG(i, MC3_LEAFCTLENB, 0xf);
	EV_SET_CONFIG(i, MC3_BANKENB, 0xff);
		
    }

    time = LD_RTC();
    timeout = time + MC3_BIST_TOUT;

    /* Wait for everyone to finish or a timeout */
    for (i = 0; i < EV_MAX_SLOTS; i++) {
	if (!((1 << i) & mem)) {
	    continue;
	}
	loprintf("Waiting for slot %b\n", i);
	while ((EV_GET_CONFIG(i, MC3_BISTRESULT) & 3) &&
	       !(timed_out(time, timeout)))
	    ;
    }
	
    if(diagval = mc3_reconfig(&evconfig, INTLV_STABLE))
	loprintf(" *** %s\n", get_diag_string(diagval));
    else
	loprintf("Memory reconfigured\n");

    loprintf("Restoring configuration information\n");

    *EVCFGINFO = evconfig;

    loprintf("Recreating MPCONF blocks ");
    if (evconfig.ecfg_debugsw & VDS_NO_DIAGS)
	loprintf(" - NO DIAGS mode\n");
    else
	loprintf(" and waiting for slaves.\n");

    /* Set up mpconf structure and send interrupt to slaves. */
    init_mpconf();
    GDA->g_vds = evconfig.ecfg_debugsw;
    if (!(evconfig.ecfg_debugsw & VDS_NO_DIAGS)) {
	slot =  (int)((LD(EV_SPNUM) & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT);
	slice = (int)((LD(EV_SPNUM) & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);
	check_slaves(slot, slice);
    }
}
#if 0
static int nbpps[] = {
    4  * 1024,
    16 * 1024,
    0
};

static int pnumshfts[] = {
    12,
    14,
    0
};

static long tlbhi_vpnmasks[] = {
    0xfffffffffffff000,
    0xffffffffffffc000,
    0
};
#endif

char *cache_alg[] = {
    "Uncached CPU",
    "Rsrvd Cache Alg",
    "Uncached Seq",
    "Cached Non-coh",
    "Cached Coh Exc",
    "Cached Coh Exc/Wr",
    "Rsrvd Cache Alg",
    "Rsrvd Cache Alg"
};

void 
dump_entry(int index)
{
    __uint64_t lo0, lo1, hi;

    hi = get_enhi(index);
    lo0 = get_enlo(index, 0);
    lo1 = get_enlo(index, 1);

    loprintf("%b: ASID %b, VPN %x -> PFN %x, %c%c%c; PFN %x, %c%c%c\n",
	     index,
	     hi & 0xff,
	     (hi >> 13) << 1,
	     lo0 >>6,
	     lo0 & TLBLO_D ? (int)'D' : 0x20,
	     lo0 & TLBLO_V ? (int)'V' : 0x20,
	     lo0 & TLBLO_G ? (int)'G' : 0x20,
	     lo1 >>6,
	     lo1 & TLBLO_D ? (int)'D' : 0x20,
	     lo1 & TLBLO_V ? (int)'V' : 0x20,
	     lo1 & TLBLO_G ? (int)'G' : 0x20);
}

void 
tlb_dump(int *arg, int arg_val)
{
    int i;

    if (!lo_strcmp(arg, "all")) {
	for (i=0; i < NTLBENTRIES; i++)
	    dump_entry(i);
    } else if (arg_val >= 0 && arg_val < NTLBENTRIES) {
	dump_entry(arg_val);
    } else
	loprintf("*** TLB slot out of range.\n");
}


void 
dump_evcfg(int *arg, int arg_val)
{
    uint slot = 0;
    uint present;
    uint mask = 1;

    if (!lo_strcmp(arg, "all")) {

	loprintf("Memory size: %d M\n", EVCFGINFO->ecfg_memsize >> 12);
	loprintf("Bus clock frequency: %d MHz\n", 
	       EVCFGINFO->ecfg_clkfreq / 1000000);
	loprintf("Virtual dip switches: 0x%x\n", EVCFGINFO->ecfg_debugsw);

	/* Rick assures us there'll never be a slot 0.
	 * I'm not counting on it.
	 */
	present = occupied_slots();

	for (;present != 0; slot++, mask <<= 1) {
	    /* Don't print slot 0 unless it actually has something in it */
	    if ((slot != 0) || (present & 0x01))
		dump_evconfig_entry(slot);
	    present >>= 1;
	} /* for ... */

    } else if (arg_val >= 0 && arg_val < EV_MAX_SLOTS) {
	dump_evconfig_entry(arg_val);
    } else {
	loprintf("*** Slot out of range\n");
    }
}


void 
dump_mpconf(uint arg_val)
{
    mpconf_t *mpc;

    if (arg_val > EV_MAX_CPUS) {
       loprintf("*** VPID out of range\n");
        return;
    }

    mpc = &(MPCONF[arg_val]);

   loprintf("MPCONF entry for VPID %b (0x%x):\n", arg_val, mpc);

   loprintf("  Magic:       %x\n", mpc->mpconf_magic);
   loprintf("  Phys ID:     %b/%b\n",
                        (EV_SLOTNUM_MASK & mpc->phys_id) >> EV_SLOTNUM_SHFT,
                        (EV_PROCNUM_MASK & mpc->phys_id) >> EV_PROCNUM_SHFT);
   loprintf("  Virt ID:     %b\n", mpc->virt_id);
   loprintf("  Launch:      %x\n", mpc->launch);
   loprintf("  Launch parm: %x\n", mpc->lnch_parm);
   loprintf("  ERTOIP:      %x\n", mpc->ertoip);
   loprintf("  CPU Rev:     %x\n", mpc->pr_id);
}

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

    loprintf("Tag 0x%x address=0x%x s=(%d)%s sm=(%d)%s ",
	     il->il_tag, 
	     (((il->il_tag & CTP_TAG_MASK) >> CTP_TAG_SHFT) << 12)
	         + (addr & (iCacheSize() / 2 -1)),
	     STATE(il->il_tag), state[STATE(il->il_tag)], 
	     STATEMOD(il->il_tag), stateMod[STATEMOD(il->il_tag)]);
    loprintf("sp(%d) scw(%d) lru(%d)\n",
	     (il->il_tag & CTP_STATEPARITY_MASK) >> CTP_STATEPARITY_SHFT,
	     (il->il_tag & CTP_SCW) ? 1 : 0, (il->il_tag & CTP_LRU) ? 1 : 0);

    for (i = 0; contents && (i < IL_ENTRIES); i += 4) {
	loprintf("\t0x%c%x  0x%c%x  0x%c%x 0x%c%x",
		 HEX(il->il_data[i] >> 32), 
		 ((__uint64_t)(__uint32_t)il->il_data[i]), 
		 HEX(il->il_data[i+1] >> 32), 
		 ((__uint64_t)(__uint32_t)il->il_data[i+1]),
		 HEX(il->il_data[i+2] >> 32), 
		 ((__uint64_t)(__uint32_t)il->il_data[i+2]),
		 HEX(il->il_data[i+3] >> 32), 
		 ((__uint64_t)(__uint32_t)il->il_data[i+3]));
	loprintf(" [%d %d %d %d]\n",
		 il->il_parity[i], il->il_parity[i+1], 
		 il->il_parity[i+2], il->il_parity[i+3]);
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

    loprintf("Tag 0x%x address=0x%x s=(%d)%s sm=(%d)%s ",
	     dl->dl_tag,
	     (((dl->dl_tag & CTP_TAG_MASK) >> CTP_TAG_SHFT) << 12)
	         + (addr & (dCacheSize() / 2 -1)),
	     STATE(dl->dl_tag), state[STATE(dl->dl_tag)], 
	     STATEMOD(dl->dl_tag), stateMod[STATEMOD(dl->dl_tag)]);
    loprintf("sp(%d) scw(%d) lru(%d)\n",
	     (dl->dl_tag & CTP_STATEPARITY_MASK) >> CTP_STATEPARITY_SHFT,
	     (dl->dl_tag & CTP_SCW) ? 1 : 0, (dl->dl_tag & CTP_LRU) ? 1 : 0);

    d = (__uint32_t *)dl->dl_data;
    for (i = 0; contents && (i < DL_ENTRIES); i += 2) {
	loprintf("\t0x%x 0x%x 0x%x 0x%x [0x%c 0x%c 0x%c 0x%c]\n", 
		 (__uint64_t)d[i*2], (__uint64_t)d[i*2+1], 
		 (__uint64_t)d[i*2+2], (__uint64_t)d[i*2+3],
		 HEX(dl->dl_ecc[i*2]), HEX(dl->dl_ecc[i*2+1]),
		 HEX(dl->dl_ecc[i*2+2]), HEX(dl->dl_ecc[i*2+3]));
    }
#   undef	STATEMOD
#   undef	STATE
}

#define	SC_STATE(t) ((__uint32_t)(((t) & CTS_STATE_MASK) >> CTS_STATE_SHFT))
static char * sc_state[] = {
    "invalid ", "shared  ", "clean-ex", "dirty-ex",
};
#define	SC_CC_STATE(t) ((__uint32_t)(((t) & CTD_STATE_MASK) >> CTD_STATE_SHFT))
static char * sc_cc_state[] = {
    "invalid", "shared", "*****", "exclusive"
};

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
#   define	HEX_ECC(x)	HEX(((x) >> 8) & 3), \
                                HEX(((x) >> 4) & 0xf), \
                                HEX((x) & 0xf)
    __uint64_t	maskAddr = ~((sCacheSize() / 2) - 1);

    tmpAddr = (((sl->sl_tag & CTS_TAG_MASK) >> CTS_TAG_SHFT) << 18) & maskAddr;
    tmpAddr += addr & ~maskAddr;

    loprintf("T5 (0x%x%x): addr 0x%x state=(%d)%s Vidx(%d) ECC(0x%b) MRU(%d)\n",
	     (sl->sl_tag >> 32), ((__uint64_t)(__uint32_t)(sl->sl_tag)),
	     tmpAddr,
	     SC_STATE(sl->sl_tag), sc_state[SC_STATE(sl->sl_tag)], 
	     (sl->sl_tag & CTS_VIDX_MASK) >> CTS_VIDX_SHFT,
	     sl->sl_tag & CTS_ECC_MASK, (sl->sl_tag & CTS_MRU) ? 1 : 0);

    tmpAddr = ((sl->sl_cctag & CTD_TAG_MASK) << 18) & maskAddr;
    tmpAddr += addr & ~maskAddr;

    loprintf("CC (0x%x%x): addr 0x%x state=(%d)%s\n",
	     sl->sl_cctag >> 32, ((__uint64_t)(__uint32_t)(sl->sl_cctag)),
	     tmpAddr,
	     SC_CC_STATE(sl->sl_cctag), 
	     sc_cc_state[SC_CC_STATE(sl->sl_cctag)]);

    d = (__uint32_t *)sl->sl_data;
    for (i = 0; contents && (i < SL_ENTRIES); i += 2) {
	loprintf("\t0x%x 0x%x 0x%x 0x%x  ",  
		 (__uint64_t)d[i*2], (__uint64_t)d[i*2+1],
		 (__uint64_t)d[i*2+2], (__uint64_t)d[i*2+3]);
	loprintf("[0x%c%c%c]\n", HEX_ECC(sl->sl_ecc[i/2]));;
    }
#   undef	HEX_ECC
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
dumpPrimaryInstructionLine(int line, int way, int contents)
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
dumpPrimaryInstructionLineAddr(__uint64_t addr, int way, int contents)
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

void
dumpPrimaryCache(int contents)
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
	dumpPrimaryDataLine(i, 0, contents);
	dumpPrimaryDataLine(i, 1, contents);
    }
}

void
dumpSecondaryCache(int contents)
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
	dumpSecondaryLine(i, 0, contents); /* way 0 */
	dumpSecondaryLine(i, 1, contents); /* way 1 */
    }
}

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
    __uint64_t	t5Addr, ccAddr, maskAddr;
    __uint32_t	ccState, t5State, stateEqual;

    sLine(a, &sl);
    maskAddr = ~((sCacheSize() / 2) - 1);

    /* Convert to addresses - and compare */

    t5Addr = (((sl.sl_tag & CTS_TAG_MASK) >> CTS_TAG_SHFT) << 18) & maskAddr;
    ccAddr = (((sl.sl_cctag & CTD_TAG_MASK) >> CTD_TAG_SHFT) << 18) & maskAddr;

    t5State = SC_STATE(sl.sl_tag);
    ccState = SC_CC_STATE(sl.sl_cctag);

    if (!strict && t5State == CTS_STATE_I) {
	/* Who cares what thr T5 says then */
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
	loprintf("CC/T5 tag mismatch: vaddr=0x%x index=%d way=%d\n",
		 a & ~1, (a % (sCacheSize() >> 1)) / CACHE_SLINE_SIZE, a & 1);
	loprintf("\tT5[0x%x%x] address=0x%x, state=%s(%d)\n", 
		 sl.sl_tag >> 32, (__uint64_t)(__uint32_t)sl.sl_tag,
		 t5Addr, sc_state[t5State], t5State);
	loprintf("\tCC[0x%x%x] address=0x%x, state=%s(%d)\n",
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

void
setSecondaryECC(__uint64_t a)
{
    sl_t	sl;
    cacheop_t	cop;

    *(((__uint64_t *)a) + 1) = 0;
    a &= ~(CACHE_SLINE_SIZE-1) + 1;

    sLine(a, &sl);
    cop.cop_address = a;
    cop.cop_operation = C_ISD + CACH_S;
    cop.cop_taghi = (__uint32_t)(sl.sl_data[0] >> 32);
    cop.cop_taglo = (__uint32_t)(sl.sl_data[0] & 0xffffffff);
    cop.cop_ecc   = sl.sl_ecc[0] ^ 3;
    cacheOP(&cop);
}

void 
_register(int rw, int *reg_name, __scunsigned_t val, struct reg_struct *gprs)
{
    register __scunsigned_t tmp;
    int number;
    evreg_t ertoip;
    static char *softwareRegisters[] = {
	"z ", "at", "v0", "v1", "a0", "a1", "a2", "a3", 
        "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3", 
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", 
	"t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
    };

    if (*reg_name == 'r' || *reg_name == 'R'|| *reg_name == '$') {
	number = lo_atoi(reg_name + 1);
	if (number <= 0 || number > 31) {
	    loprintf("*** Invalid register name.\n");
	    return;
	}
	loprintf("r%a: %y\n", number, gprs->gpr[number]);
    } else if (lo_strcmp(reg_name,"sp") == 0) {
	tmp = _sp(rw,val);
	if (rw == 0)
	    loprintf("SP: %y\n",tmp);
    } else if (lo_strcmp(reg_name,"sr") == 0) {
	tmp = _sr(rw,val);
	if (rw == 0)
	    loprintf("SR: %y\n",tmp);
    } else if (lo_strcmp(reg_name,"cause") == 0) {
	tmp = _cause(rw,val);
	if (rw == 0)
	    loprintf("Cause: %y\n",tmp);
    } else if (lo_strcmp(reg_name,"epc") == 0) {
	tmp = _epc(rw,val);
	if (rw == 0)
	    loprintf("EPC: %y\n",tmp);
    } else if (lo_strcmp(reg_name,"config") == 0) {
	tmp = _config(rw,val);
	if (rw == 0)
	    loprintf("Config: %y\n",tmp);
    } else if (lo_strcmp(reg_name,"all") == 0) {
	for (number = 0; number < 32; number ++) {
	    loprintf("r%a/%s: %y  ", number, softwareRegisters[number], 
		     gprs->gpr[number]);
	    if (!((number + 1) % 3))
		loprintf("\n");
	}
	loprintf("BVA: %y\n", gprs->badva);
	loprintf("EPC: %y   SR: %y\n", gprs->epc, gprs->sr);
	loprintf("Cause: %y  ", gprs->cause);
	xlate_cause(gprs->cause);
	if (ertoip = get_ERTOIP()) {	/* pick up saved ERTOIP */
	    xlate_ertoip(ertoip);
	}
    } else {
	loprintf("*** Invalid register name.\n");
    }
}

void 
send_int(int slot, int cpu, int number)
{
    SD_LO(EV_SENDINT,  ((slot&0xf)<<2) | (cpu&0x3) | (number<<8));
}


void 
conf_register(int rw, uint slot, uint reg_num, __uint64_t data, int repeat)
{
    volatile __uint64_t *address;

    if (slot > 15) {
	loprintf("*** Slot out of range\n");
	return;
    }
    if (reg_num > 255) {
	loprintf("*** Reg. num. out of range\n");
	return;
    }

    address = (volatile __uint64_t *)
	(EV_CONFIGREG_BASE + (slot << 11) + (reg_num << 3));

    if (rw)
	*address = data;
    else
	data = *address;
#ifdef NOPRINT
    if (!repeat)
#endif /* NOPRINT */
	loprintf("Slot %b, Reg %b: %y\n", slot, reg_num, data);
}

/*
 * Display the state of the IO4 board in the specified slot.  Used
 */

void 
dump_io4(uint slot)
{
    evreg_t value;

    /*
     * First we insure that the slot specified by the user is
     * valid and actually contains an IO-type board.
     */
    if (slot > 15) {
	loprintf("*** Slot 0x%b is out of range.\n", slot);
	return;
    }
	
    if ( !(LD(EV_SYSCONFIG) & (1 << slot))) {
	loprintf("*** Slot 0x%b is empty.\n", slot);
	return;
    }
 
    value = EV_GET_CONFIG(slot, IO4_CONF_REVTYPE) & IO4_TYPE_MASK;
    if (value != IO4_TYPE_VALUE) {
	loprintf("*** Slot 0x%b does not contain an IO board\n", slot);
	return;
    }

    /*
     * Now dump the actual registers.
     */
    loprintf("Configuration of the IO board in slot 0x%b\n", slot);
    loprintf("  Large Window: %d,     Small Window: %d\n",
	     EV_GET_CONFIG(slot, IO4_CONF_LW), 
	     EV_GET_CONFIG(slot, IO4_CONF_SW) >> 8);
    loprintf("  Endianness:          %s Endian\n", 
	     (EV_GET_CONFIG(slot, IO4_CONF_ENDIAN) ? "Big" : "Little"));
    loprintf("  Adapter Control:     0x%x\n", 
	     EV_GET_CONFIG(slot, IO4_CONF_ADAP));
    value = EV_GET_CONFIG(slot, IO4_CONF_INTRVECTOR);
    loprintf("  Interrupt Vector:    Level 0x%x, Destination 0x%x\n",
	     EVINTR_LEVEL(value), EVINTR_DEST(value));
    loprintf("  Config status:       HI: 0x%x, LO: 0x%x\n",
	     EV_GET_CONFIG(slot, IO4_CONF_IODEV1), 
	     EV_GET_CONFIG(slot, IO4_CONF_IODEV0));
    loprintf("  IBUS Error:          0x%x\n", 
	     EV_GET_CONFIG(slot, IO4_CONF_IBUSERROR));
    loprintf("  EBUS Error1:          0x%x\n", 
	     EV_GET_CONFIG(slot, IO4_CONF_EBUSERROR));
    loprintf("          EBUS Err2Hi: 0x%x  EBUS Err2Lo: 0x%x\n",
	     EV_GET_CONFIG(slot, IO4_CONF_EBUSERROR2), 
	     EV_GET_CONFIG(slot, IO4_CONF_EBUSERROR1));

    loprintf("\n");
}	


/*
 * Displays the contents of the major MC3 device registers.
 */

void dump_mc3(uint slot)
{
    uint mem;
    int i, j;

    mem = memory_slots();

    if (slot > 15) {
	loprintf("*** Slot out of range\n");
	return;
    }

    if (!((1 << slot) & mem)) {
	loprintf("*** Slot %b has no memory board.\n", slot);
	return;
    }
    loprintf("Configuration of the memory board in slot %b\n", slot);
    loprintf(" EBus Error:   %x\n", read_reg(slot, MC3_EBUSERROR));
    loprintf(" Leaf Enable:  %x\n", read_reg(slot, MC3_LEAFCTLENB));
    loprintf(" Bank Enable:  %x\n", read_reg(slot, MC3_BANKENB));
    loprintf(" BIST Result:  %x\n", read_reg(slot, MC3_BISTRESULT));
    for (i = 0; i < 2; i++) {
	loprintf(" Leaf %d:\n", i);
	loprintf("  BIST = %x, Error = %x, ErrAddrHi = %x, ErrAddrLo = %x\n",
		 read_reg(slot, MC3_LEAF(i, MC3LF_BIST)),
		 read_reg(slot, MC3_LEAF(i, MC3LF_ERROR)),
		 read_reg(slot, MC3_LEAF(i, MC3LF_ERRADDRHI)),
		 read_reg(slot, MC3_LEAF(i, MC3LF_ERRADDRLO)));
	loprintf("  Syndrome 0: %h, Syndrome 1: %h, Syndrome 2: %h, Syndrome 3: %h\n",
		 read_reg(slot, MC3_LEAF(i, MC3LF_SYNDROME0)),
		 read_reg(slot, MC3_LEAF(i, MC3LF_SYNDROME1)),
		 read_reg(slot, MC3_LEAF(i, MC3LF_SYNDROME2)),
		 read_reg(slot, MC3_LEAF(i, MC3LF_SYNDROME3)));
	for (j = 0; j < 4; j++) {
	    loprintf("  Bank %d: ", j);
	    loprintf("Size = %x, Base = %x, IF = %x, IP = %x\n",
		     read_reg(slot, MC3_BANK(i, j, BANK_SIZE)),	
		     read_reg(slot, MC3_BANK(i, j, BANK_BASE)),	
		     read_reg(slot, MC3_BANK(i, j, BANK_IF)),	
		     read_reg(slot, MC3_BANK(i, j, BANK_IP)));
	}
    }
}

uint read_reg(uint slot, uint reg_num)
{
    return (int)LD((EV_CONFIGREG_BASE + (slot << 11) + (reg_num << 3)));
}

void
clear_mc3_state(void)
{
    uint mem;
    int slot = 0;
    int leaf;

    mem = memory_slots();

    for (slot = 0; slot < 16; slot++) {
	if (mem & (1 << slot)) {
	    for (leaf = 0; leaf < MC3_NUM_LEAVES; leaf++)
		read_reg(slot, MC3_LEAF(leaf, MC3LF_ERRORCLR));
	    loprintf("  Cleared memory board %b's error registers\n", slot);
	}
    }
}

void
clear_io4_state(void)
{
    uint io;
    int slot = 0;

    io = occupied_slots() & ~(cpu_slots() | memory_slots());

    for (slot = 0; slot < 16; slot++) {
	if (io & (1 << slot)) {
	    read_reg(slot, IO4_CONF_IBUSERRORCLR);
	    read_reg(slot, IO4_CONF_EBUSERRORCLR);
	    loprintf("  Cleared IO board %b's error registers\n", slot);
	}
    }
}

void memory(int rw, int size, __scunsigned_t addr, __uint64_t data, int repeat)
{
    __scunsigned_t old_addr;
    int c = 0;
    int buf[SHORTBUFF_LENGTH];
    int verbose = 1;
    int looping = 0;

    for (;;) {
	old_addr = addr;
	if (rw == READ) {
	    data = 0;
	    switch (size) {
	    case BYTE:
		data = LBU((char *)addr);
		addr ++;
		break;
	    case HALF:
		data = LHU((unsigned short *)addr);
		addr += 2;
		break;
	    case WORD:
		data = LW(addr);
		addr += 4;
		break;
	    case DOUBLE:
		data = *(__uint64_t *)addr;
		addr += 8;
		break;
	    default:
		break;
	    }
	}
	else {
	    switch (size) {
	    case BYTE:
		SB(addr, data);
		addr ++;
		break;
	    case HALF:
		SH(addr, data);
		addr += 2;
		break;
	    case WORD:
		SW((__uint32_t *)addr, (__uint32_t)data);
		addr += 4;
		break;
	    case DOUBLE:
		SD((__uint64_t *)addr, data);
		addr += 8;
		break;
	    default:
		break;
	    }
	}
	if (looping) {			/* if != 0 will loop until key is hit */
	    int c2;

	    if (size == DOUBLE) {
		if (verbose)
		    loprintf("\n%x: %y ", old_addr, data);
	    } else {
		if (verbose)
		    loprintf("\n%x: %x ", old_addr, data);
	    }
	    if (pod_poll() != 0) {
		c2 = pod_getc();
		switch (c2) {
		case 'v':
		    verbose = 1;
		    break;
		case 's':
		    verbose = 0;
		    break;
		default:
		    /*no looping, verbose on */
		    looping = 0;
		    verbose = 1;
		    break;
		}
	    }
	    if (c == 'l')		/* loop at same address */
		addr = old_addr;	/* keep same address */
	} else {			/* if (looping) */
	    /* Don't print stuff when writing in a loop. */
#ifdef NOPRINT
	    if (!repeat)
#else
		if (rw == READ || !repeat)
#endif /* NOPRINT */
		    switch (size) {
		    case DOUBLE:
			loprintf("%x: %y ", old_addr, data);
			break;
		    case WORD:
			loprintf("%x: %x ", old_addr, data);
			break;
		    case HALF:
			loprintf("%x: %h ", old_addr, data);
			break;
		    case BYTE:
			loprintf("%x: %b ", old_addr, data);
			break;
		    }
#ifndef NOPRINT
	    if (repeat) {
		if (rw == READ)
		    loprintf("\n");
		return;
	    }
#else
	    if (repeat)
		return;
#endif /* NOPRINT */
	    if (rw == WRITE) {
		loprintf("\n\r%x: ", addr);
	    } 

	    /* after executing the command, check for input */
	    logets(buf, SHORTBUFF_LENGTH);
	    c = *buf;
	    switch (c) {
	    case 0:			/* return */
		c = 0;			/* next addr, if write use old data */
		looping = 0;
		break;
	    case '.':			/* continue infinitely, same pattern */
		looping = 1;
		break;
	    case 'l':			/*loop at same location, same pattern */
		addr = old_addr;
		looping = 1;
		break;
	    default:
		if (rw == WRITE) {
		    if (!lo_ishex(c))
			return;
		    data = lo_atoh(buf);
		    break;
		}
		else
		    return;
	    }
	}
    }
}


/* Provide system configuration information. */
void info(void)
{
    int cpu, mem, io, present;
    int slot = 0;	/* Slot 0 doesn't exist on any of our machines */
    int mask = 1;	/*   so skip over it to avoid confusion */

    cpu = cpu_slots();
    mem = memory_slots();
    present = occupied_slots();

    io = present ^ (cpu | mem);

   loprintf("System physical configuration:\n");
    
    for (;present != 0; slot++, mask <<= 1) {

	/* Don't print slot 0 unless it actually has something
	   in it.  Current hardware doesn't have a slot 0 */
        if (slot != 0 || (present & 0x1)) { 
	   loprintf("  Slot %b:\t", slot);
	    if (cpu & mask)
		   loprintf("CPU board\n");
	    else if (io & mask)
		   loprintf("I/O board\n");
	    else if (mem & mask)
		   loprintf("Memory board\n");
	    else
		   loprintf("Empty\n");
	 }   
	present >>= 1;
    }	
   loprintf("This processor is slot %b, cpu %b.\n",
		(LD(EV_SPNUM) &  EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT,
		(LD(EV_SPNUM) &  EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);
}

int
ip25Registers(int slot, int slice)
/*
 * Function: ip25Registers
 * Purpose: To print all known information about stuff on the bus
 * Parameters: slot - if !0 slot/slice is cpu slot/slice to get ertoip of.
 *			dump register values.
 * Returns: 0 - OK, !0 - failed to access some/all registers.
 */
{
    jmp_buf 	fault_buf; /* Status buffer */
    uint 	*old_buf; /* Previous fault buffer */
    evreg_t	r;

    if (setfault(fault_buf, &old_buf)) {
	restorefault(old_buf);
	return(1);
    }
    loprintf("CPU[0x%b/0x%b]\n\t", slot, slice);

    r = EV_GETCONFIG_REG(slot, slice, EV_CFG_ERTOIP);
    if (r) {
	xlate_ertoip(r);
	loprintf("\t");
    }
    r = EV_GETCONFIG_REG(slot, slice, EV_CFG_ERADDR_HI) << 32
	| EV_GETCONFIG_REG(slot, slice, EV_CFG_ERADDR_LO);
    loprintf("eraddr=0x%x,", r);
    r = EV_GETCONFIG_REG(slot, slice, EV_CFG_CACHE_SZ);
    loprintf("cachesz=%d,", r);
    r = EV_GETCONFIG_REG(slot, slice, EV_CFG_PGBRDEN);
    loprintf("pgbrden=%d,", r);
    r = EV_GETCONFIG_REG(slot, slice, EV_CFG_ECCHKDIS);
    loprintf("\n\tecchkdis=0x%x,", r);
    r = EV_GETCONFIG_REG(slot, slice, EV_CFG_IWTRIG);
    loprintf("iwtrig=0x%x,", r);
    r = EV_GETCONFIG_REG(slot, slice, EV_CFG_ERTAG);
    loprintf("ertag=0x%x", r);
    r = EV_GETCONFIG_REG(slot, slice, EV_CFG_ERSYSBUS_HI);
    loprintf("\n\tersysbushi/lo=0x%x/", r);
    r = (EV_GETCONFIG_REG(slot, slice, EV_CFG_ERSYSBUS_LO_HI) << 32) | 
	(EV_GETCONFIG_REG(slot, slice, EV_CFG_ERSYSBUS_LO_LO));
    loprintf("0x%x", r);
    r = (EV_GETCONFIG_REG(slot, slice, EV_CFG_DEBUG0_MSW) << 32) | 
	(EV_GETCONFIG_REG(slot, slice, EV_CFG_DEBUG0_LSW));
    loprintf("\n\tDebug: [0]=0x%x ", r);
    r = (EV_GETCONFIG_REG(slot, slice, EV_CFG_DEBUG1_MSW) << 32) | 
	(EV_GETCONFIG_REG(slot, slice, EV_CFG_DEBUG1_LSW));
    loprintf("[1]=0x%x ", r);
    r = (EV_GETCONFIG_REG(slot, slice, EV_CFG_DEBUG2_MSW) << 32) | 
	(EV_GETCONFIG_REG(slot, slice, EV_CFG_DEBUG2_LSW));
    loprintf("[2]=0x%x\n", r);
    restorefault(old_buf);
    return(0);
}

void
dumpSlice(int slot, int slice)
/*
 * Function: dumpSlice
 * Purpose: 	To dump the SCC registers assocated with a slice.
 * Parameters:	slot/slice to query
 * Returns:
 */
{
    evreg_t	r;

    if ((slot < 1) || (slot > 15) || (slice < 0) || (slice > 3)) {
	loprintf("*** invalid slot/slice: 0x%a/0x%a\n", slot, slice);
	return;
    }

    /* Check if achip enabled */

    if (!(cpu_slots() & (1 << slot))) {
	loprintf("*** slot 0x%a is not a CPU board\n", slot);
	return;
    }

    r = EV_GETCONFIG_REG(slot, 0, EV_A_ENABLE);

    if (!(r & (1 << slice))) {
	loprintf("*** cpu 0x%a/0x%a is not enabled in A-chip\n", 
		 slot, slice);
	return;
    }

    if (ip25Registers(slot, slice)) {
	loprintf("\n\n*** cpu 0x%a/0x%a: EXCEPTION reading registers\n",
		 slot, slice);
	return;
    }
}
void
dumpSlot(int slot)
/*
 * Function: dumpSlot
 * Purpose: 	To dump the SCC registers assocated with a slot.
 * Parameters:	slot/slice to query
 * Returns:
 */
{
    static char *margin[] = {"0%", "+5%", "-5%", "***"};
    evreg_t	r, rt;
    int		slice, rtValid;

    if ((slot < 1) || (slot > 15)) {
	loprintf("*** invalid slot: 0x%a\n", slot);
	return;
    }

    /* Check if achip enabled */

    if (!(cpu_slots() & (1 << slot))) {
	loprintf("*** slot 0x%a is not a CPU board\n", slot);
	return;
    }

    r = EV_GETCONFIG_REG(slot, 0, EV_A_ENABLE);

    loprintf("Slot 0x%a A-chip: enable=0x%b, type=0x%b, lvl=0x%d, err=0x%x\n",
	     slot, r, 
	     EV_GETCONFIG_REG(slot, 0, EV_A_BOARD_TYPE),
	     EV_GETCONFIG_REG(slot, 0, EV_A_LEVEL),
	     EV_GETCONFIG_REG(slot, 0, EV_A_ERROR));
    if (r & 1) {			/* Slice 0 - voltage margin */
	rt = EV_GETCONFIG_REG(slot, 0, EV_ECCHKDIS);
	rtValid = 1;
    } else {
	/* Try to enable it but don't complain if doe snot appear there. */
	jmp_buf	fault_buf;
	uint	*old_buf;
	EV_SETCONFIG_REG(slot, 0, EV_A_ENABLE, r | 1);
	if (!setfault(fault_buf, &old_buf)) {
	    rt = EV_GETCONFIG_REG(slot, 0, EV_ECCHKDIS);
	    rtValid = 1;
	} else {
	    rtValid = 0;
	}
	restorefault(old_buf);
	EV_SETCONFIG_REG(slot, 0, EV_A_ENABLE, r);
    }

    if (rtValid) {
	loprintf("        Voltage: 1.5 (%s) 3.3 (%s) 5.0 (%s)\n", 
		 margin[(rt & EV_ECCHKDIS_15_MASK) >> EV_ECCHKDIS_15_SHFT],
		 margin[(rt & EV_ECCHKDIS_33_MASK) >> EV_ECCHKDIS_33_SHFT],
		 margin[(rt & EV_ECCHKDIS_50_MASK) >> EV_ECCHKDIS_50_SHFT]);
    }

    for (slice = 0; slice < EV_MAX_CPUS_BOARD; slice++) {
	if ((r & (1 << slice))) {
	    if (ip25Registers(slot, slice)) {
		loprintf("\n\n*** cpu 0x%a/0x%a: EXCEPTION reading registers\n",
			 slot, slice);
		return;
	    }
	}
    }
}
void
dumpAll(void)
/*
 * Function: dumpAll
 * Purpose: 	To dump the SCC registers assocated with a slot.
 * Parameters:	slot/slice to query
 * Returns:
 */
{
    int	 slot;
    int	cpu = cpu_slots();

    for (slot = 1; slot <=15; slot++) {
	if (cpu & (1 << slot)) {
	    dumpSlot(slot);
	}
    }
}

void 
margin(__uint64_t slot, int *v, int *d)
{
    int		dir;
    evreg_t	cr, r;			/* current value */
    jmp_buf	fault_buf;
    uint		*old_buf;

    if (!lo_strcmp(d, "0")) {
	dir = EV_MGN_NOM;
    } else if (!lo_strcmp(d, "+")) {
	dir = EV_MGN_HI;
    } else if (!lo_strcmp(d, "-")) {
	dir = EV_MGN_LO;
    } else {
	loprintf("Invalid margin direction '%p': must be +/-/0\n", d);
	return;
    }

    /* Verify CPU board, and be sure A-chip has slice 0 enabled */


    if (!(cpu_slots() & (1 << slot))) {
	loprintf("*** slot 0x%a is not a CPU board\n", slot);
	return;
    }

    r = EV_GETCONFIG_REG(slot, 0, EV_A_ENABLE);

    if (!(r & 1)) {
	/* Try to enable SCC for marginning */
	EV_SETCONFIG_REG(slot, 0, EV_A_ENABLE, r | 1);
    }

    if (!setfault(fault_buf, &old_buf)) {
	cr = EV_GETCONFIG_REG(slot, 0, EV_ECCHKDIS);
	if (!lo_strcmp(v, "1.5")) {
	    cr = (cr & ~EV_ECCHKDIS_15_MASK) | dir << EV_ECCHKDIS_15_SHFT;
	} else if (!lo_strcmp(v, "3.3")) {
	    cr = (cr & ~EV_ECCHKDIS_33_MASK) | dir << EV_ECCHKDIS_33_SHFT;
	} else if (!lo_strcmp(v, "5.0")) {
	    cr = (cr & ~EV_ECCHKDIS_50_MASK) | dir << EV_ECCHKDIS_50_SHFT;
	} else {
	    loprintf("Invalid power brick '%p': must be 1.5, 3.3, or 5.0\n", 
		     v);
	}	

	EV_SETCONFIG_REG(slot, 0, EV_ECCHKDIS, cr);
    } else {
	loprintf("Unable to access power margin for slot %a\n", slot);
    }

    if (!(r & 1)) {
	/* disable it now */
	EV_SETCONFIG_REG(slot, 0, EV_A_ENABLE, r);
    }
    restorefault(old_buf);
}

/*
 * POD memory test
 */

#define	BIT_TRUE	0
#define BIT_INVERT	1

/* struct addr to pass to generic memory test routines */
struct addr_range {
	uint	*lomem;	/* starting location */
	uint	*himem;	/* end location */
	int	dmask;	/* data bit fields to be masked off when reading back */
	int	inc;	/* word or half or byte */
	int	pattern;/* 0 means no special pattern to be written */
	int	invert;
};

static int read_wr(volatile struct addr_range *);
static int bwalking_addr(volatile struct addr_range *);
static int addr_pattern(volatile struct addr_range *);


/*
 * called by POD command mode.
 */
int mem_test(__psunsigned_t lo, __psunsigned_t hi)
{
    volatile struct addr_range addr;
    volatile int fail;

    if (!IS_KSEG1(lo))
	addr.lomem = (uint *)PHYS_TO_K1(lo);
    else
	addr.lomem = (uint *)lo;
    if (!IS_KSEG1(hi))
	addr.himem = (uint *)PHYS_TO_K1(hi);
    else
	addr.himem = (uint *)hi;

    if (K1_TO_PHYS(lo) < LOMEM_STRUCT_END)
	loprintf("*** This test will overwrite PROM data structures.  Type reset to recover.\n");

    addr.inc = 1;			/* word test */
    addr.invert = BIT_TRUE;
    addr.dmask = 0xffffffff;		/* check them all */

    /* walking addr */
    loprintf("Walking address...\t");
    fail = bwalking_addr(&addr);
    if (fail) {
	loprintf("Failed!\n");
	pon_memerror();
    } else
	loprintf("Passed!\n");
    loprintf("Read/Write Test");
    if (read_wr(&addr)) {
	fail = 1;
	loprintf("\tFailed!\n");
	pon_memerror();
    } else
	loprintf("\tPassed!\n");
    loprintf("Addr Pattern Test");
    if (addr_pattern(&addr)) {
	fail = 1;
	loprintf("..\tFailed!\n");
	pon_memerror();
    } else
	loprintf("..\tPassed!\n");
		
    return fail;
}

static int bwalking_addr(volatile struct addr_range *addr)
{
    register unsigned char k;
    volatile uint testline;
    int fail = 0;
    volatile caddr_t pmem, refmem;
    caddr_t lomem = (caddr_t)addr->lomem;
    caddr_t himem = (caddr_t)addr->himem;
    jmp_buf fault_buf;			/* Status buffer */
    uint *prev_fault;			/* Previous fault buffer */
    volatile int mode;

#define WRITE_PMEM	0
#define WRITE_RMEM	1
#define READ_PMEM	2

    /* If an exception occurs, return to the following block */
    if (setfault(fault_buf, &prev_fault)) {
	restorefault(prev_fault);
	if (mode == WRITE_PMEM) {
	    loprintf("\n*** Took an exception at 0x%x\n", pmem);
	    loprintf("    Writing 0x55\n");
	} else if (mode == WRITE_RMEM) {
	    loprintf("\n*** Took an exception at 0x%x\n", refmem);
	    loprintf("    Writing 0xaa\n");
	} else {
	    loprintf("\n*** Took an exception at 0x%x\n", pmem);
	    loprintf("    Reading (should have been 0xaa)\n");
	}
	loprintf("    Testing line 0x%x\n", testline);
	return 1;			/* FAILED! */
    }

    refmem = lomem;
    for (testline = 1;
	 ((__scunsigned_t)lomem | testline) <= (__scunsigned_t)himem;
	 testline <<= 1)
    {
	mode = WRITE_RMEM;
	*(unsigned char *)refmem = 0x55;
	pmem = (caddr_t)((__scunsigned_t)lomem | testline);
	if (pmem == refmem)
	    continue;
	mode = WRITE_PMEM;
	*(unsigned char *)pmem = 0xaa;
	mode = READ_PMEM;
	k = LBU(refmem) & addr->dmask;
	if (k != (0x55 & addr->dmask)) {
	    loprintf(
		     "*** Wrote 0x55 to %x, wrote 0xAA to %x\n",
		     refmem, pmem);
	    loprintf("read %x from %x\n", k, refmem);
	    loprintf("    Testing line 0x%x\n", testline);
	    pon_memerror();
	    fail = 1;
	}
    }
    restorefault(prev_fault);
    return fail;
}

/* Use current addr as the test pattern */
static int 
addr_pattern(volatile struct addr_range *addr)
{
    register int inc = (int)(addr->inc * sizeof(uint));
    uint mask = addr->dmask;
    volatile caddr_t pmem;
    volatile caddr_t pmemhi;
    volatile int data;
    int fail = 0;
    jmp_buf fault_buf;			/* Status buffer */
    uint *prev_fault;			/* Previous fault buffer */
    volatile int mode;

    /* If an exception occurs, return to the following block */
    if (setfault(fault_buf, &prev_fault)) {
	restorefault(prev_fault);
	loprintf("\n*** Took an exception checking addr 0x%x\n", pmem);
	if (mode == WRITING)
	    loprintf("    Writing 0x%x\n", pmem);
	else
	    loprintf("    Reading (should have been 0x%x)\n",
		     pmem);
	return 1;			/* FAILED! */
    }


    pmem = (caddr_t)addr->lomem;
    pmemhi = (caddr_t)addr->himem;

    mode = WRITING;
    while ((__scunsigned_t)pmem < (__scunsigned_t)pmemhi) {
	*(volatile uint *)pmem = 
	    (uint)((__scunsigned_t)pmem & 0xffffffff);
	pmem += inc;
    }

    loprintf(".");
    pmem = (caddr_t)addr->lomem;

    mode = READING;
    while ((__scunsigned_t)pmem < (__scunsigned_t)pmemhi) {
	if ((data = (LW(pmem) & mask)) != 
	    ((__scunsigned_t)pmem & mask)) {
	    loprintf(
		     "\tError\n\tWrote %x to %x,\tread back %x\n",
		     pmem, pmem, data);
	    pon_memerror();
	    fail = 1;
	}
	pmem += inc;
    }
    restorefault(prev_fault);
    return fail;
}


int rw_loop(volatile struct addr_range *addr, uint old_pat, uint new_pat)
{
    register uint inc = (uint)addr->inc * (uint)sizeof(uint);
    volatile uint data;
    volatile caddr_t ptr;
    caddr_t himem = (caddr_t)addr->himem;
    caddr_t lomem = (caddr_t)addr->lomem;
    uint mask = (uint)addr->dmask;
    int fail = 0;
    jmp_buf fault_buf;			/* Status buffer */
    uint *prev_fault;			/* Previous fault buffer */
    volatile int mode;

    /* If an exception occurs, return to the following block */
    if (setfault(fault_buf, &prev_fault)) {
	restorefault(prev_fault);
	loprintf("\n*** Took an exception checking addr 0x%x\n", ptr);
	if (mode == WRITING)
	    loprintf("    Writing 0x%x\n", new_pat);
	else
	    loprintf("    Reading (should have been 0x%x)\n",
		     old_pat);
	return 1;			/* FAILED! */
    }

    for (ptr = lomem ; ptr < himem; ptr += inc) {
	mode = READING;
	data = LW(ptr) & mask;
	if (data != old_pat) {
	    loprintf(
		     "\tError\n\tWrote 0x%x to 0x%x; read 0x%x\n",
		     old_pat, ptr, data);
	    pon_memerror();
	    fail = 1;
	}
	mode = WRITING;
	*(volatile uint *)ptr = new_pat;
    }
    restorefault(prev_fault);
    return fail;
}


static int read_wr(volatile struct addr_range *addr)
{
    int fail = 0;
    volatile uint *ptr;

    for(ptr = addr->lomem; ptr < addr->himem; ptr += addr->inc)
	*ptr = 0;
	
    loprintf(".");

    /* from lomem to himem read and verify 0's, then
     * write -1's
     */
    fail |= rw_loop(addr, 0U, 0xffffffffU);

    if (fail)
	return fail;

    loprintf(".");

    /* from lomem to himem read and verify 1's, then
     * write 0x5a5a5a5a
     */

    fail |= rw_loop(addr, 0xffffffffU, 0x5a5a5a5aU);

    if (fail)
	return fail;

    loprintf(".");

    /* from lomem to himem read and verify 0x5a5a5a5a's, then
     * write 0x55555555
     */

    fail |= rw_loop(addr, 0x5a5a5a5aU, 0x55555555U);

    if (fail)
	return fail;

    loprintf(".");

    /* from lomem to himem read and verify 0x55555555, then
     * write 0xaaaaaaaa
     */
    fail |= rw_loop(addr, 0x55555555U, 0xaaaaaaaaU);

    if (fail)
	return fail;

    loprintf(".");

    /* from lomem to himem read and verify 0xaaaaaaaa, then
     * write 0x00000000
     */
    fail |= rw_loop(addr, 0xaaaaaaaaU, 0);

    if (fail)
	return fail;

    loprintf(".");

    return 0;
}

/*
 * Error handling routines when running with stack on the 1st level cache
 */
void pon_memerror(void)
{
    int c;

 err_loop:
    loprintf("\nContinue test?\n");
    loprintf("(y = continue test, n = reinvoke POD mode)");

    while((c = pod_poll()) == 0)
	;
    c = pod_getc();
    if( c == 'y' ) {
	loprintf("\n");
	return;
    } else if( c == 'n' ) {
	loprintf("\n");
	run_incache();
    } else {
	goto err_loop;
    }
}

/*
 * Version of sload for downloading code via RS232
 * Assume RS232 port has been initialized
 */

#define	ACK	0x6
#define	NAK	0x15

#define DIGIT(c)	((c)>'9'?((c)>='a'?(c)-'a':(c)-'A')+10:(c)-'0')

int jump_addr(__psunsigned_t address, uint parm1, uint parm2,
	      struct flag_struct *flags)
{
    uint ret_val;
    int sregs[40];	/* 9 sregs, ra * 2 ints + one spare for alignment */

    if (!flags->silent)
	loprintf("Jumping to %x\n", address);

    save_sregs(sregs);

    /* Jump to the address passing the appropriate parameters.
     * If flags->mem is one, flush and invalidate the caches first.
     */
    ret_val = pod_jump(address, parm1, parm2, flags->mem);

    restore_sregs(sregs);
    return ret_val;
}

int 
call_asm(uint (*function)(__scunsigned_t), __scunsigned_t parm) 
{

    int sregs[40];	/* 9 sregs, ra * 2 ints + one spare for alignment */
    int result;

    result = save_and_call(sregs, function, parm);

    return result;
}

/* Write to the Everest System Reset register. */

void reset_system()
{
    SD_LO(EV_KZRESET, 0);
}


void zap_inventory()
{

    if (!nvram_okay()) {
	loprintf("NVRAM inventory is already invalid.\n");
	return;
    }
    set_nvreg(NVOFF_INVENT_VALID, 0);
    set_nvreg(NVOFF_NEW_CHECKSUM, nvchecksum());
}

int
testScacheFlipBits(__uint64_t p, int stopOnError)
{
    cacheop_t	cop;
    __uint64_t	a;
    __uint32_t	taghi, taglo, ecc;
    int		way, offset;

#   define 	TAGHI_MASK	(CTS_TAGHI_MASK & (__uint32_t)(~CTS_MRU>>32))
#   define	TAGLO_MASK	CTS_TAGLO_MASK

    /* First check tags */

    taghi = (__uint32_t)(p >> 32) & TAGHI_MASK;
    taglo = (__uint32_t)p & TAGLO_MASK;

    for (a =0; a < sCacheSize() / 2; a += CACHE_SLINE_SIZE) {
	for (way = 0; way <= 1; way++) {
	    cop.cop_address 	= K0BASE + a + way;
	    cop.cop_operation	= C_IST+CACH_S;
	    cop.cop_taghi	= taghi;
	    cop.cop_taglo	= taglo;
	    cacheOP(&cop);
	}
	taghi = ~taghi & TAGHI_MASK;
	taglo = ~taglo & TAGLO_MASK;
    }

    for (a = 0; a < sCacheSize() / 2; a += CACHE_SLINE_SIZE) {
	for (way = 0; way <= 1; way++) {
	    cop.cop_address = K0BASE + a + way;
	    cop.cop_operation	= C_ILT+CACH_S;
	    cacheOP(&cop);
	    if ((cop.cop_taghi != taghi) || (cop.cop_taglo != taglo)) {
		loprintf(" 2ndry tag compare error: Way %d:", way);
		loprintf(" address: 0x%x\n", a);
		loprintf("\twrote: 0x%x\n", 
			 ((__uint64_t)taghi << 32) + taglo);
		loprintf("\tread : 0x%x\n", 
			 ((__uint64_t)cop.cop_taghi << 32) + cop.cop_taglo);
		if (stopOnError) {
		    return(1);
		}
	    }
	}
	taghi = ~taghi & TAGHI_MASK;
	taglo = ~taglo & TAGLO_MASK;
    }

    /* Now the data */

    taghi = (__uint32_t)(p >> 32);
    taglo = (__uint32_t)p;
    ecc   = (__uint32_t)p;

    for (offset = 0; offset <= 8; offset += 8) {

	for (a = offset; a < sCacheSize() / 2; a += 16) {
	    for (way = 0; way <= 1; way++) {
		cop.cop_address 	= K0BASE + a + way;
		cop.cop_operation	= C_ISD+CACH_S;
		cop.cop_taghi	= taghi;
		cop.cop_taglo	= taglo;
		cop.cop_ecc		= ecc;
		cacheOP(&cop);
	    }
	    taghi = ~taghi;
	    taglo = ~taglo;
	    ecc   = ~ecc & CTS_ECC_MASK;
	}

	for (a = offset; a < sCacheSize() / 2; a += 16) {
	    for (way = 0; way <= 1; way++) {
		cop.cop_address = K0BASE + a + way;
		cop.cop_operation= C_ILD+CACH_S;
		cacheOP(&cop);
		if ((cop.cop_taghi != taghi) || (cop.cop_taglo != taglo)) {
		    loprintf(" 2ndry data compare error: Way %d:", way);
		    loprintf(" address: 0x%x\n", a);
		    loprintf("\twrote(data/ecc): 0x%x/0x%x\n", 
			     ((__uint64_t)taghi << 32) + taglo, ecc);
		    loprintf("\tread (data/ecc): 0x%x/0x%x\n", 
			     ((__uint64_t)cop.cop_taghi << 32) + cop.cop_taglo, 
			     cop.cop_ecc);
		    if (stopOnError) {
			return(1);
		    }
		}
	    }
	    ecc   = ~ecc & CTS_ECC_MASK;
	    taghi = ~taghi;
	    taglo = ~taglo;
	}
    }
    return(0);
}

