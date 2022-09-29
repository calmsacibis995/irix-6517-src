/***********************************************************************\
*       File:           pod.c                                           *
*                                                                       *
\***********************************************************************/

#ident "$Revision: 1.105 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include "pod.h"
#include <sys/cpu.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/IP21addrs.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/nvram.h>
#include "pod_failure.h"
#include "ip21prom.h"
#include "prom_leds.h"
#include "prom_intr.h"
#include "prom_externs.h"
#include <setjmp.h>

#define NOPRINT

void	check_slaves(int, int);
void	store_gprs(struct reg_struct *);
void	scroll_n_print(unsigned char);
void	scroll_message(unsigned char *, int);
void	code_msg(unsigned char, char *);
void	run_incache(void);
void	pon_memerror(void);
int	mc3_reconfig(evcfginfo_t *, int);
int	save_and_call(int *, uint (*function)(__scunsigned_t), __scunsigned_t);
uint	pod_jump(__scunsigned_t, uint, uint, uint);
int	*pod_parse(int *, struct reg_struct *, int, int, struct flag_struct *);
void	show_page_size(void);
void	set_page_size(int);

__uint64_t	get_enhi(int, int, int);
__uint64_t	get_enlo(int, int, int);

__scunsigned_t _stag(__scunsigned_t);
__scunsigned_t _dtag(__scunsigned_t);

void	store_half(short *, short);
void	store_word(uint *, uint);

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
  "Inst. VCE",
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
  "Data VCE"
};


char *ertoip_names[16] = {
  "DB chip Parity error DB0",			/* 0 */
  "DB chip Parity error DB1",			/* 1 */
  "A Chip Addr Parity",				/* 2 */
  "Time Out on MyReq on EBus Channel 0",	/* 3 */
  "Time Out on MyReq on EBus Wback Channel ",	/* 4 */
  "Addr Error on MyReq on EBus Channel 0",	/* 5 */
  "Addr Error on MyReq on EBus Wback Channel ",	/* 6 */
  "Data Sent Error Channel 0",			/* 7 */
  "Data Sent Error Wback Channel ",		/* 8 */
  "Data Receive Error",				/* 9 */
  "Intern Bus vs. A_SYNC",			/* 10 */
  "A Chip MyResponse Data Resources Time Out",  /* 11 */
  "A Chip MyIntrvention Data Resources Time Out",/* 12 */
  "Parity Error on DB - CC shift lines",	/* 13 */
  "Not defined",				/* 14 */
  "Not defined"					/* 15 */
};

char *pstate[4] = {
  "Invalid",		/* 0 */
  "Shared",		/* 1 */
  "Clean Exclusive",	/* 2 */
  "Dirty Exclusive"	/* 3 */
};

char *sstate[8] = {
  "Invalid",		/* 0 */
  "*** RESERVED",	/* 1 */
  "*** RESERVED",	/* 2 */
  "*** RESERVED",	/* 3 */
  "Clean Exclusive",	/* 4 */
  "Dirty Exclusive",	/* 5 */
  "Shared",		/* 6 */
  "Dirty Shared"	/* 7 */
};

#define READING 0
#define WRITING 1

static void
xlate_cause(__scunsigned_t value)
{
	int exc_code;
	int int_bits;
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
xlate_ertoip(__uint64_t value)
{
	int i;

	if (!value)
		return;

	loprintf("*** Error/TimeOut Interrupt(s) Pending: %x ==\n", value);

	for (i = 0; i < 16; i++)
		if (value & (1 << i))
			loprintf("\t %s\n", ertoip_names[i]);
}

void
gcache_parity(void)
{
	__scunsigned_t	cause;

	cause = _cause(0, 0);
	if (cause & (CAUSE_IP9 | CAUSE_IP10)) {
		_cause(1, cause & ~(CAUSE_IP9 | CAUSE_IP10));
		loprintf("*** G-cache parity error: CAUSE %x\n", cause);
	}
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
	int parse_level = 0;
	struct reg_struct gpr_space, *gprs = &gpr_space;
	int ertoip;
	struct flag_struct flags;
	char diag_string[80];

	sc_disp(diagval);

	/* Get the GPR values from the GPRs and FPRs (FPRs contain the values
		that were in the GPRs when Pod_handler was called). */
	store_gprs(gprs);

	/* Tell the world we've reached the POD loop
	 */
	set_cc_leds(PLED_PODLOOP);

	flags.slot =  (load_double_lo((long long *)EV_SPNUM) 
			& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	flags.slice = (load_double_lo((long long *)EV_SPNUM)
			& EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	flags.selected = 0xff;	/* No one's selecetd */
	flags.silent = 0;	/* Normal verbnosity level */

	decode_diagval(diag_string, diagval);
        flags.diag_string = diag_string;
	flags.diagval = diagval;

	if (dex)
		/* We're running with stack in dirty-exclusive cache lines */
		flags.mem = 0;
	else
		/* We're running with stack in memory */
		flags.mem = 1;

	if(diagval != EVDIAG_DEBUG) {
		loprintf("  Cause = %x ", gprs->cause);
		xlate_cause(gprs->cause);
	}

	gcache_parity();

	if (ertoip = load_double_lo((long long *) EV_ERTOIP))
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
		if (dex)
			loprintf("POD %b/%b> ", flags.slot, flags.slice);
		else
			loprintf("Mem %b/%b> ", flags.slot, flags.slice);
		logets(buf, LINESIZE);
		pod_parse(buf, gprs, parse_level, 0, &flags);
		gcache_parity();
		if (ertoip = load_double_lo((long long *) EV_ERTOIP))
			xlate_ertoip(ertoip);
	}
}

/* decode_diagval-
 *	This is _very_ preliminary code for coming up with real
 *		messages to display on the system controller.
 *	We need to do _much_ better.
 *	POD will need another parameter to describe where the error
 *		occurred.
 */
void decode_diagval(char *diag_string, int diagval)
{
	/* We need to get FRU info, location, etc. */

	lo_strcpy(diag_string, get_diag_string(EVDIAG_DIAGCODE(diagval)));

}

/* set_unit_enable-
 *	Enables or disables a unit in a particular slot in the evconfig
 * 		structure.
 */
int set_unit_enable(uint slot, uint unit, uint enable, uint force)
{
	evbrdinfo_t *brd;
	int conf_reg;
	int myslot;
	extern int cpu_masks[];

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
    			myslot = (load_double_lo((long long *)EV_SPNUM)
					& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
			if (myslot == slot)
				goto valid_cpu;

			loprintf("*** Slot %b is empty\n", slot);
			return -1;
		case EVCLASS_CPU:
valid_cpu:
			if (unit > 1) {
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
				conf_reg = *EV_CONFIGADDR(slot, 0, EV_A_ENABLE);
				if (enable)
					conf_reg |= cpu_masks[unit];
				else
					conf_reg &= ~cpu_masks[unit];
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
						conf_reg = EV_GET_CONFIG(slot,
							IO4_CONF_IODEV1);
					else
						conf_reg = EV_GET_CONFIG(slot,
							IO4_CONF_IODEV0);
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
					brd->eb_banks[unit].bnk_size =
						EV_GET_CONFIG(slot,
						MC3_BANK((unit >> 2),(unit & 3),
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
    		slot =  (load_double_lo((long long *)EV_SPNUM)
				& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    		slice = (load_double_lo((long long *)EV_SPNUM)
				& EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;
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

        time = load_double_bloc((long long *)EV_RTC);
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
    		slot =  (load_double_lo((long long *)EV_SPNUM)
				& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    		slice = (load_double_lo((long long *)EV_SPNUM)
				& EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;
		check_slaves(slot, slice);
	}
}

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

static long pgcodes[] = {
	(SR_UPS_4K|SR_KPS_4K),
	(SR_UPS_16K|SR_KPS_16K),
	0
};


void
show_page_size(void)
{
	loprintf("Page size: %x bytes\n", MPCONF[0].pod_page_size);
}

void
set_page_size(int pgsz)
{
	int i;
	long long srval;

	i = 0;
	while (nbpps[i]) {
		if (pgsz == nbpps[i]) {
			MPCONF[0].pod_page_size = pgsz;

			/* set the KPS field of sr to reflect new pagesize */
			srval = _sr(0, 0);  /* read sr */
			srval &= ~SR_KPSMASK;
			srval |= pgcodes[i];
			_sr(1, srval); 

			show_page_size();
			return;
		}
		i++;
	}

	loprintf("Invalid page size (%x). Valid page sizes are:\n", pgsz);
	i = 0;
	while (nbpps[i]) {
		loprintf("\t%x\n", nbpps[i]);
		i++;
	}
}

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

void dump_entry(int index)
{
	__uint64_t tlblo, tlbhi, xormask, vaddr, paddr, pid;
	int i, set, page_size;

	/*
	 * Need to compute bits 18:12 of the Virtual Address
	 * since they are implied by the index number XORed
	 * with the PID.  Just "or" them into tlbhi contents
	 * since the bits should be zero.
	 */

	i = 0;
	page_size = -1;
	while (nbpps[i]) {
		if (MPCONF[0].pod_page_size == nbpps[i]) {
			page_size = i;
			break;
		}
		i++;
	}
	if (page_size == -1)
		page_size = 1;	/* 16kb page size */


	for (set = 0; set < NTLBSETS; set++) {
		tlbhi = get_enhi(index, set, pnumshfts[page_size]);
		tlblo = get_enlo(index, set, pnumshfts[page_size]);
		vaddr = tlbhi & tlbhi_vpnmasks[page_size];
		paddr = tlblo & TLBLO_PFNMASK;
		pid = (tlbhi & TLBHI_PIDMASK) >> TLBHI_PIDSHIFT;

		/* Need to 'xor' low order virtual address with the
		 * PID unless the address is in KV1 space
		 * (kernel global).
		 */
		xormask = ((tlbhi&KV1BASE) != KV1BASE) ? pid : 0;
		vaddr |= ((index ^ xormask) << pnumshfts[page_size]);
		loprintf("%b/%b: hi %x lo %x <PID %x V %x P %x %c%c %s>\n",
			index, set, tlbhi, tlblo, pid, vaddr, paddr,
		 	((tlblo & TLBLO_D) ? (int)'D' : (int)' '),
		 	((tlblo & TLBLO_V) ? (int)'V' : (int)' '),
		 	cache_alg[(tlblo & TLBLO_CACHMASK) >> TLBLO_CACHSHIFT]);
	}
}

void tlb_dump(int *arg, int arg_val)
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


void dump_evcfg(int *arg, int arg_val)
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


void dump_mpconf(uint arg_val)
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
   loprintf("  CPU Rev:     %x\n", mpc->pr_id);
}

/*
 * dump_tag
 * POD_STACKADDR 0x90000000000fc000 beginning of dcache ram
 * POD_SCACHEADDR 0xa800000000000000 beginning of gcache ram
 *
 */

#define STAG_PRINT_ALL 1
#define STAG_PRINT_VALID_ONLY 2

#define DUMP_IT(state, type) \
	(state & state_mask) || (type == STAG_PRINT_ALL)

void dump_stag(__scunsigned_t address, int type, int state_mask)
{
    __scunsigned_t entryaddr0;
    int btag0, btag1, btag2, btag3;
    int bstate0, bstate1, bstate2, bstate3;
    int ptagE0, ptagE1, ptagE2, ptagE3;
    int ptagO0, ptagO1, ptagO2, ptagO3;
    int pstateE0, pstateE1, pstateE2, pstateE3;
    int pstateO0, pstateO1, pstateO2, pstateO3;
    int lf = 0;
    int entry, print;
    entry = (address >> 9) & 0x7ff;	/* bit 9-19 */

    /* get bus tag state from 4 sets */
    entryaddr0 = (entry << 3) + BB_BUSTAG_ST;
    bstate0 = _stag(entryaddr0);
    bstate1 = _stag(entryaddr0 + 0x10000);
    bstate2 = _stag(entryaddr0 + 0x20000);
    bstate3 = _stag(entryaddr0 + 0x30000);
    
    /* get proc tag state from 4 sets */
    entryaddr0 = (entry << 3) + BB_PTAG_E_ST;
    pstateE0 = _stag(entryaddr0);
    pstateE1 = _stag(entryaddr0 + 0x10000);
    pstateE2 = _stag(entryaddr0 + 0x20000);
    pstateE3 = _stag(entryaddr0 + 0x30000);
    
    /* get proc tag state from 4 sets */
    entryaddr0 = (entry << 3) + BB_PTAG_O_ST;
    pstateO0 = _stag(entryaddr0);
    pstateO1 = _stag(entryaddr0 + 0x10000);
    pstateO2 = _stag(entryaddr0 + 0x20000);
    pstateO3 = _stag(entryaddr0 + 0x30000);
    
    /* get tag from 4 sets */
    entryaddr0 = (entry << 3) + BB_BUSTAG_ADDR;
    btag0 = (_stag(entryaddr0)           >> 20) & 0xfffff; /* bit 20-39 tag */
    btag1 = (_stag(entryaddr0 + 0x10000) >> 20) & 0xfffff; /* bit 20-39 tag */
    btag2 = (_stag(entryaddr0 + 0x20000) >> 20) & 0xfffff; /* bit 20-39 tag */
    btag3 = (_stag(entryaddr0 + 0x30000) >> 20) & 0xfffff; /* bit 20-39 tag */
    
    /* get proc tag from 4 sets */
    entryaddr0 = (entry << 3) + BB_PTAG_E_ADDR;
    ptagE0 = (_stag(entryaddr0)           >> 20) & 0xfffff; /* bit 20-39 tag */
    ptagE1 = (_stag(entryaddr0 + 0x10000) >> 20) & 0xfffff; /* bit 20-39 tag */
    ptagE2 = (_stag(entryaddr0 + 0x20000) >> 20) & 0xfffff; /* bit 20-39 tag */
    ptagE3 = (_stag(entryaddr0 + 0x30000) >> 20) & 0xfffff; /* bit 20-39 tag */
    
    /* get proc tag from 4 sets */
    entryaddr0 = (entry << 3) + BB_PTAG_O_ADDR;
    ptagO0 = (_stag(entryaddr0) >> 20) & 0xfffff; /* bit 20-39 tag */
    ptagO1 = (_stag(entryaddr0 + 0x10000) >> 20) & 0xfffff; /* bit 20-39 tag */
    ptagO2 = (_stag(entryaddr0 + 0x20000) >> 20) & 0xfffff; /* bit 20-39 tag */
    ptagO3 = (_stag(entryaddr0 + 0x30000) >> 20) & 0xfffff; /* bit 20-39 tag */

    print = DUMP_IT(bstate0, type) + DUMP_IT(bstate1, type) + DUMP_IT(bstate2, type) + DUMP_IT(bstate3, type)
	+ DUMP_IT(pstateE0, type) + DUMP_IT(pstateE1, type) + DUMP_IT(pstateE2, type) + DUMP_IT(pstateE3, type)
	+ DUMP_IT(pstateO0, type) + DUMP_IT(pstateO1, type) + DUMP_IT(pstateO2, type) + DUMP_IT(pstateO3, type);

    if (print) {
	if (lf == 0) {
	    loprintf("\n");
	    lf = 1;
	}

	
	loprintf("\nphysical address %y, scache index = 0x%x\n", address, entry);
	loprintf("                set0        set1        set2        set3\n");
	loprintf("                ============================================\n");
	loprintf("Bus   Tag       ");
	DUMP_IT(bstate0, type) ? loprintf("%x    ", btag0) : loprintf("            ");
	DUMP_IT(bstate1, type) ? loprintf("%x    ", btag1) : loprintf("            ");
	DUMP_IT(bstate2, type) ? loprintf("%x    ", btag2) : loprintf("            ");
	DUMP_IT(bstate3, type) ? loprintf("%x    \n", btag3) : loprintf("\n");
	loprintf("Bus   State     ");
	DUMP_IT(bstate0, type) ? loprintf("%x    ", bstate0) : loprintf("            ");
	DUMP_IT(bstate1, type) ? loprintf("%x    ", bstate1) : loprintf("            ");
	DUMP_IT(bstate2, type) ? loprintf("%x    ", bstate2) : loprintf("            ");
	DUMP_IT(bstate3, type) ? loprintf("%x    \n", bstate3) : loprintf("\n");
	loprintf("ProcE Tag       ");
	DUMP_IT(pstateE0, type) ? loprintf("%x    ", ptagE0) : loprintf("            ");
	DUMP_IT(pstateE1, type) ? loprintf("%x    ", ptagE1) : loprintf("            ");
	DUMP_IT(pstateE2, type) ? loprintf("%x    ", ptagE2) : loprintf("            ");
	DUMP_IT(pstateE3, type) ? loprintf("%x    \n", ptagE3) : loprintf("\n");
	loprintf("ProcE State     ");
	DUMP_IT(pstateE0, type) ? loprintf("%x    ", pstateE0) : loprintf("            ");
	DUMP_IT(pstateE1, type) ? loprintf("%x    ", pstateE1) : loprintf("            ");
	DUMP_IT(pstateE2, type) ? loprintf("%x    ", pstateE2) : loprintf("            ");
	DUMP_IT(pstateE3, type) ? loprintf("%x    \n", pstateE3) : loprintf("\n");
	loprintf("ProcO Tag       ");
	DUMP_IT(pstateO0, type) ? loprintf("%x    ", ptagO0) : loprintf("            ");
	DUMP_IT(pstateO1, type) ? loprintf("%x    ", ptagO1) : loprintf("            ");
	DUMP_IT(pstateO2, type) ? loprintf("%x    ", ptagO2) : loprintf("            ");
	DUMP_IT(pstateO3, type) ? loprintf("%x    \n", ptagO3) : loprintf("\n");
	loprintf("ProcO State     ");
	DUMP_IT(pstateO0, type) ? loprintf("%x    ", pstateO0) : loprintf("            ");
	DUMP_IT(pstateO1, type) ? loprintf("%x    ", pstateO1) : loprintf("            ");
	DUMP_IT(pstateO2, type) ? loprintf("%x    ", pstateO2) : loprintf("            ");
	DUMP_IT(pstateO3, type) ? loprintf("%x    \n", pstateO3) : loprintf("\n");
    }
    else {
	if (lf == 1) {
	    loprintf("\n\n");
	    lf = 0;
	}
	loprintf("*%x ", entry);
    }

}

int dump_all_stag(state_mask)
{
    int i;
    for (i = 0; i < 2048; i++) {
	dump_stag(0x200 * i, STAG_PRINT_VALID_ONLY, state_mask);
    }
}


int dump_all_dtag(state_mask)
{
    int i;
    __scunsigned_t v;
    for (i = 0; i < 512; i++) {
	v = _dtag(0x10 * i);
	loprintf("addr %x, index %x: tag = %x, state = %x\n", 0x10 * i, i, (v >> 12) & 0xffffffff, (v >> 55) & 0xf);
    }
}



int dump_tag(int which, __scunsigned_t address)
{
	__scunsigned_t value0;
	int tag;


	switch(which) {
		case STAG_DUMP:
			dump_stag(address, STAG_PRINT_ALL, 0xffffffff); 
			break;

		case DTAG_DUMP:
			loprintf("virtual addr at == %y\n", address); 
			value0 = _dtag(address);
			tag = (value0 >> 12) & 0xfffffff;
			loprintf("Tag == %x\n", tag);
			loprintf("Valid bits == %x\n", (value0>> 55) & 0xf);
			break;
		default:
			return EVDIAG_TBD;
	}

	return 0;
}

void _register(int rw, int *reg_name, __scunsigned_t val,
		struct reg_struct *gprs)
{
	register __scunsigned_t tmp;
	int number;
	int ertoip;

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
			loprintf("r%a: %y  ", number, gprs->gpr[number]);
			if (!((number + 1) % 3))
				loprintf("\n");
		}
		loprintf("BVA: %y\n", gprs->badva);
		loprintf("EPC: %y   SR: %y\n", gprs->epc, gprs->sr);
		loprintf("Cause: %y  ", gprs->cause);
		xlate_cause(gprs->cause);
		if (ertoip = load_double_lo((long long *)EV_ERTOIP))
			xlate_ertoip(ertoip);
	} else {
		loprintf("*** Invalid register name.\n");
	}
}

void send_int(slot, cpu, number)
{
	/* CPU number which is bit 1 only. Bit 0 has no meaning as 
	there are only two CPUs per board in IP21 */

       store_double_lo((long long *)EV_SENDINT,
			((slot&0xf)<<2) | ((cpu&0x1)<<1) | (number<<8));
}


void conf_register(int rw, uint slot, uint reg_num, __uint64_t data, int repeat)
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

void dump_io4(uint slot)
{
	uint value;

	/*
 	 * First we insure that the slot specified by the user is
	 * valid and actually contains an IO-type board.
	 */
	if (slot > 15) {
		loprintf("*** Slot 0x%b is out of range.\n", slot);
		return;
	}
	
	if ( !(load_double_lo((long long *)EV_SYSCONFIG) & (1 << slot))) {
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
	loprintf(" EBus Error:   %x\n", read_reg_nowar(slot, MC3_EBUSERROR));
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
	return load_double_lo((long long *)(EV_CONFIGREG_BASE + (slot << 11) + (reg_num << 3)));
}

uint read_reg_nowar(uint slot, uint reg_num)
{
	return load_double_lo_nowar((long long *)(EV_CONFIGREG_BASE + (slot << 11) + (reg_num << 3)));
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
				read_reg_nowar(slot, MC3_LEAF(leaf, MC3LF_ERRORCLR));
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
			read_reg_nowar(slot, IO4_CONF_IBUSERRORCLR);
			read_reg_nowar(slot, IO4_CONF_EBUSERRORCLR);
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
				data = load_ubyte((char *)addr);
				addr ++;
				break;
			case HALF:
				data = load_uhalf((short *)addr);
				addr += 2;
				break;
			case WORD:
				data = load_word((uint *)addr);
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
				*(u_char *)addr = (u_char)data;
				addr ++;
				break;
			case HALF:
				*(u_short *)addr = (u_short)data;
				addr += 2;
				break;
			case WORD:
				*(uint *)addr = data;
				addr += 4;
				break;
			case DOUBLE:
				*(__uint64_t *)addr = data;
				addr += 8;
				break;
			default:
				break;
			}
		}
		if (looping) { /* if != 0 will loop until key is hit */
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
			if (c == 'l') /* loop at same address */
				addr = old_addr; /* keep same address */
		} else { /* if (looping) */
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
			case 0:	/* return */
				c = 0; /* next addr, if write use old data */
				looping = 0;
				break;
			case '.':	/* continue infinitely, same pattern */
				looping = 1;
				break;
			case 'l':	/*loop at same location, same pattern */
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
void info()
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
		(load_double_lo((long long *)EV_SPNUM) 
					& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT,
		(load_double_lo((long long *)EV_SPNUM)
					& EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);
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
	addr.dmask = 0xffffffff;	/* check them all */

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
	jmp_buf fault_buf;      /* Status buffer */
	uint *prev_fault;	/* Previous fault buffer */
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
		return 1;  /* FAILED! */
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
		k = load_ubyte(refmem) & addr->dmask;
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
static int addr_pattern(volatile struct addr_range *addr)
{
	register int inc = addr->inc * sizeof(uint);
	uint mask = addr->dmask;
	volatile caddr_t pmem;
	volatile caddr_t pmemhi;
	volatile int data;
	int fail = 0;
	jmp_buf fault_buf;      /* Status buffer */
	uint *prev_fault;	/* Previous fault buffer */
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
		return 1;  /* FAILED! */
	}


	pmem = (caddr_t)addr->lomem;
	pmemhi = (caddr_t)addr->himem;

	mode = WRITING;
	while ((__scunsigned_t)pmem < (__scunsigned_t)pmemhi) {
		*(volatile uint *)pmem = (uint)((__scunsigned_t)pmem & 0xffffffff);
		pmem += inc;
	}

	loprintf(".");
	pmem = (caddr_t)addr->lomem;

	mode = READING;
	while ((__scunsigned_t)pmem < (__scunsigned_t)pmemhi) {
		if ((data = (load_word(pmem) & mask)) != ((__scunsigned_t)pmem & mask)) {
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
	register uint inc = addr->inc * sizeof(uint);
	volatile uint data;
	volatile caddr_t ptr;
	caddr_t himem = (caddr_t)addr->himem;
	caddr_t lomem = (caddr_t)addr->lomem;
	uint mask = (uint)addr->dmask;
	int fail = 0;
	jmp_buf fault_buf;      /* Status buffer */
	uint *prev_fault;	/* Previous fault buffer */
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
		return 1;  /* FAILED! */
	}

	for (ptr = lomem ; ptr < himem; ptr += inc) {
		mode = READING;
		data = load_word(ptr) & mask;
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

	if (!flags->silent) {
		loprintf("Invalidating I and D Caches\n");
	}
	call_asm(pon_invalidate_IDcaches, 0);
	
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

/*
 * in-line implementation of get_pair for speed
 */
#define	get_pair() \
	( \
		c1 = pod_getc() & 0x7f, \
		c2 = pod_getc() & 0x7f, \
		byte = (DIGIT(c1) << 4) | DIGIT(c2), \
		csum += byte, \
		byte \
	)


#ifdef BAD_SLOAD
/* Version of sload which can be used to download code in cache in case
 * of bad memory
 * pon_sload(baud)
 *	baud: 110 to 19200. Default is 19200
 */

pon_sload(int command, struct flag_struct *flags)
{
	register length, address;
	register c1, c2;
	register int byte;
	__psunsigned_t client_pc = 0;
	int type, i;
	int done, reccount, save_csum;
	int csum;
	int fbyte[4];
	int abort = 0;
	int errors = 0;

	loprintf("Ready to download...\n");

	reccount = 1;
	for (done = 0; (!done && !abort); reccount++) {

		/* Update the CC leds every 32 records */
		set_cc_leds(reccount>>5);

		while (((i = pod_getc()) != 'S') && !(abort = (i == 0x3))) {
			if (i == EOF) {
				/*
				 * Only check for errors here,
				 * if user gave a bad device problem
				 * will usually be caught.
				 */
				loprintf("Error or EOF \n");
				return(0);
			}
			continue;
		}
		csum = 0;
		if (!abort)
			type = pod_getc();
		if (abort || type == 0x3) {
			abort = 1;
		} else {
			length = get_pair();
			if (length < 0 || length >= 256) {
				loprintf("Invalid length \n");
				abort = 1;
				goto bad;
			}
			length--;	/* save checksum for later */
		}
		switch (type) {

		case '0':	/* initial record, ignored */
			while ((length-- > 0) && !abort)
				get_pair();
			break;

		case '3':	/* data with 32 bit address */
			address = 0;
			for (i = 0; ((i < 4) && !abort); i++) {
				address <<= 8;
				address |= get_pair();
				length--;
			}
			i = 0;
			while ((length-- > 0) && !abort) {
				fbyte[i++] = get_pair();
				if (i >= 4) {
					for (i=0; i<4; i++)
						fbyte[i] = fbyte[i] << (3-i)*8;
					i = fbyte[0] | fbyte[1] |
					    fbyte[2] | fbyte[3];
					*(uint *)address = i;
					address += 4;
					i = 0;
				}
			}
			if (i != 0)
				loprintf("Not enough to fill a word!\n");
			break;

		case '7':	/* end of data, 32 bit initial pc */
			address = 0;
			for (i = 0; ((i < 4) && !abort); i++) {
				address <<= 8;
				address |= get_pair();
				length--;
			}
			client_pc = (__scunsigned_t)address;
			if (length)
				loprintf("Type 7 record with unexpected data\n",
					reccount);
			done = 1;
			break;
		case 0x3:
			abort = 1;
			break;
		default:
			loprintf("Unknown record type\n");
			break;
		}
		if (abort) {
			loprintf("Download aborted at user request.\n");
			done = 1;
		} else {
			save_csum = (~csum) & 0xff;
			csum = get_pair();
			if (csum != save_csum) {
				loprintf("Checksum error!\n");
				errors++;
			}
		}
	}

	if (!abort) {
		loprintf("Done downloading!\n");
		loprintf("Done: %x records, initial pc: 0x%x\n",
						reccount-1, client_pc);
		if (errors)
			loprintf("\t%x checksum errors.\n", errors);
	}

bad:

	if (command == SERIAL_RUN) {
		if (errors || abort)
			loprintf("*** NOT RUNNING.\n");
		else 
			loprintf("Returned %x\n",
					jump_addr(client_pc, 0, 0, flags));
	}

	if (errors || abort)
		return 1;
	else
		return 0;
}


#else /* BAD_SLOAD */

/* Version of sload which can be used to download code in cache in case
 * of bad memory
 * pon_sload(baud)
 *	baud: 110 to 19200. Default is 19200
 */

int pon_sload(int command, struct flag_struct *flags)
{
	register int length;
	register __psint_t address;
	register int c1, c2;
	register int byte;
	__psunsigned_t client_pc = 0;
	int type, i;
	int done, reccount, save_csum;
	int csum;
	int fbyte[4];

	loprintf("Ready to download...\n");

	reccount = 1;
	for (done = 0; !done; reccount++) {

		/* Update the CC leds every 32 records */
		set_cc_leds(reccount>>5);

		while ((i = pod_getc()) != 'S') {
			if (i == EOF) {
				/*
				 * Only check for errors here,
				 * if user gave a bad device problem
				 * will usually be caught.
				 */
				loprintf("Error or EOF \n");
				return(0);
			}
			continue;
		}
		csum = 0;
		type = pod_getc();
		length = get_pair();
		if (length < 0 || length >= 256) {
			loprintf("Invalid length \n");
			goto bad;
		}
		length--;	/* save checksum for later */
		switch (type) {

		case '0':	/* initial record, ignored */
			while (length-- > 0)
				get_pair();
			break;

		case '3':	/* data with 32 bit address */
			address = 0;
			for (i = 0; i < 4; i++) {
				address <<= 8;
				address |= get_pair();
				length--;
			}
			i = 0;
			while (length-- > 0) {
				fbyte[i++] = get_pair();
				if (i >= 4) {
					for (i=0; i<4; i++)
						fbyte[i] = fbyte[i] << (3-i)*8;
					i = fbyte[0] | fbyte[1] |
					    fbyte[2] | fbyte[3];
					*(uint *)address = i;
					address += 4;
					i = 0;
				}
			}
			if (i != 0)
				loprintf("Not enough to fill a word!\n");
			break;

		case '7':	/* end of data, 32 bit initial pc */
			address = 0;
			for (i = 0; i < 4; i++) {
				address <<= 8;
				address |= get_pair();
				length--;
			}
			client_pc = (__scunsigned_t)address;
			if (length)
				loprintf("Type 7 record with unexpected data\n",
					reccount);
			done = 1;
			break;
		default:
			loprintf("Unknown record type\n");
			break;
		}
		save_csum = (~csum) & 0xff;
		csum = get_pair();
		if (csum != save_csum) {
			loprintf("Checksum error!\n");
		}
	}
	loprintf("Done downloading!\n");
	loprintf("Done: %x records, initial pc: 0x%x\n", reccount-1, client_pc);

	if (command == SERIAL_RUN) {
		loprintf("Returned %x\n", jump_addr(client_pc, 0, 0, flags));
	}

	return 0;
bad:
	return 1;
}

#endif /* BAD_SLOAD */


int call_asm(uint (*function)(__scunsigned_t), __scunsigned_t parm) {

	int sregs[40];	/* 9 sregs, ra * 2 ints + one spare for alignment */
	int result;

	result = save_and_call(sregs, function, parm);

	return result;
}

/* Write to the Everest System Reset register. */
void reset_system()
{
	store_double_lo((long long *)EV_KZRESET, 0);
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
