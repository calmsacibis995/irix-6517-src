/*
 * This file contains code that deal with the fact that Everest uses 1st level
 * D-cache as its stack very early on. When the only thing that works is
 * the 1st level D-cache, the PROM can use these routines to talk to
 * the UART and let user poke around in memory.
 * Basically the problems are: only word writes go into cache.
 */

#ident "$Revision: 1.67 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include "pod.h"
#include <sys/cpu.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/IP19addrs.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/nvram.h>
#include "pod_failure.h"
#include "ip19prom.h"
#include "prom_leds.h"
#include "prom_intr.h"
#include <setjmp.h>

#define NOPRINT

void pod_loop(int, int);
extern void loprintf(char *, ...);
extern void lo_strcpy(char *, char *);
extern int *logets(int *, int);
extern int *pod_parse(int *, struct reg_struct *, int, int, struct flag_struct*);
extern int lo_strcmp(int *, char *);
extern uint lo_atoh(int *);
extern uint lo_atoi(int *);
extern int lo_ishex(int);
void _register(int, int *, int, struct reg_struct *);
void memory(int, int, uint, uint, uint, int);
void info();
int call_asm(int (*function)(uint), uint);
extern void write_earom();
extern uint load_uhalf(void *);
extern uint load_ubyte(void *);
extern uint load_word(void *);
extern uint load_double_lo(void *);
int bwalking_addr();
int addr_pattern();
void xlate_cause(int);
extern save_sregs(int *);
extern restore_sregs(int *);
extern pon_flush_dcache();
extern pon_zero_icache(uint);
extern pon_invalidate_icache();
extern char *prom_slave;
extern char *get_diag_string(unsigned char);
void decode_diagval(char *, int);
extern ushort calc_cksum(void);
void fix_cksum(void);

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
  "R4k Data Double ECC",		/* 0 */
  "R4k Data Single ECC",		/* 1 */
  "Tag RAM Parity",			/* 2 */
  "A Chip Addr Parity",			/* 3 */
  "D Chip Data Parity",			/* 4 */
  "Ebus MyReq timeout",			/* 5 */
  "A Chip MyResp drsc tout",		/* 6 */
  "A Chip MyIntrvResp drsc tout",	/* 7 */
  "EBus MyReq Addr Err",		/* 8 */
  "EBus MyData Data Err",		/* 9 */
  "Intern Bus vs. A_SYNC",		/* 10 */
  "Not defined",			/* 11 */
  "Not defined",			/* 12 */
  "Not defined",			/* 13 */
  "Not defined",			/* 14 */
  "Not defined"				/* 15 */
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

void xlate_cause(int value)
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

void xlate_ertoip(int value)
{
	int i;

	if (value)
		loprintf("*** Error/TimeOut Interrupt(s) Pending: %x ==\n",
									value);
	for (i = 0; i < 16; i++)
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
	int parse_level = 0;
	struct reg_struct *gprs;
	char gpr_space[sizeof(struct reg_struct) + 4];
	int ertoip;
	struct flag_struct flags;
	char diag_string[80];

	sc_disp(diagval);

	gprs = (struct reg_struct *)(((unsigned int)gpr_space+4) & 0xfffffff8);

	/* Get the GPR values from the GPRs and FPRs (FPRs contain the values
		that were in the GPRs when Pod_handler was called). */
	store_gprs(gprs);

	/* Tell the world we've reached the POD loop
	 */
	set_cc_leds(PLED_PODLOOP);

	flags.slot = (load_double_lo((uint *)EV_SPNUM) 
			& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	flags.slice = load_double_lo((uint *)EV_SPNUM) & EV_PROCNUM_MASK;

	flags.selected = 0xff;	/* No one's selecetd */

	flags.de = 0;		/* Enable cache error exceptions in tests */
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
		loprintf("  Cause = ");
		xlate_cause(gprs->cause);
	}

	if (ertoip = load_double_lo((void*) EV_ERTOIP))
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
		if (ertoip = load_double_lo((void*) EV_ERTOIP))
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

	if (unit > 7) {
		loprintf("*** Unit out of range!\n");
		return -1;
	}

	brd = &(EVCFGINFO->ecfg_board[slot]);
	switch((brd->eb_type) & EVCLASS_MASK) {
		case EVCLASS_NONE:
			loprintf("*** Slot %b is empty\n", slot);
			return -1;
		case EVCLASS_CPU:
			if (unit > 3) {
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
					conf_reg |= (1 << unit);
				else
					conf_reg &= (~(1 << unit));
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
			

pod_reconf_mem() 
{
	evcfginfo_t evconfig;
	int diagval;
	int c;
	unsigned intlv_type;

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
	if (!(evconfig.ecfg_debugsw & VDS_NO_DIAGS))
		check_slaves();
}


pod_bist() {
	evcfginfo_t evconfig;
	int diagval;
	unsigned int mem, i, time, timeout;

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

        time = load_double_bloc(EV_RTC);
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
	if (!(evconfig.ecfg_debugsw & VDS_NO_DIAGS))
		check_slaves();
}


void dump_entry(int index) {
unsigned int hi, lo0, lo1;

	hi = get_enhi(index);
	lo0 = get_enlo0(index);
	lo1 = get_enlo1(index);

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


void tlb_dump(int *arg, int arg_val)
{
	int i;

	if (!lo_strcmp(arg, "all")) {
		for (i=0; i < NTLBENTRIES; i++)
			dump_entry(i);
	} else if (!lo_strcmp(arg, "l")) {
		for (i=0; i < NTLBENTRIES / 2; i++)
			dump_entry(i);
	} else if (!lo_strcmp(arg, "h")) {
		for (i=NTLBENTRIES / 2; i < NTLBENTRIES; i++)
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

void dump_mpconf(int *arg, uint arg_val)
{
    mpconf_t *mpc;

    if (arg_val > EV_MAX_CPUS) {
	loprintf("*** VPID out of range\n");
	return;
    }

    mpc = &(MPCONF[arg_val]);

    loprintf("MPCONF entry for VPID %b (0x%x):\n", arg_val, mpc);

    loprintf("  Magic:       %x\n", mpc->mpconf_magic);
    loprintf("  EAROM cksum: %h\n", mpc->earom_cksum);
    loprintf("  Saved cksum: %h\n", mpc->stored_cksum);
    loprintf("  Phys ID:     %b/%b\n",
			(EV_SLOTNUM_MASK & mpc->phys_id) >> EV_SLOTNUM_SHFT,
			 EV_PROCNUM_MASK & mpc->phys_id);
    loprintf("  Virt ID:     %b\n", mpc->virt_id);
    loprintf("  Launch:      %x\n", mpc->launch);
    loprintf("  Launch parm: %x\n", mpc->lnch_parm);
    loprintf("  CPU Rev:     %x\n", mpc->pr_id);
}


int dump_tag(int which, uint line)
{
	uint tag;

	line &= 0x1fffffff;
	line |= 0x80000000;

	switch(which) {
		case STAG_DUMP:
			tag = _stag(line);
			break;
		case DTAG_DUMP:
			tag = _dtag(line);
			break;
		case ITAG_DUMP:
			tag = _itag(line);
			break;
		default:
			return EVDIAG_TBD;
	}

	if (which == STAG_DUMP) {
		loprintf("Scache line == %x\n", line);		
		loprintf("  Raw tag == %x\n", tag);
		loprintf("  Tag Addr == %x\n", (tag & SADDRMASK) << SADDR_SHIFT);
		loprintf("  State == %s\n", sstate[(tag & SSTATEMASK) >> 10]);
		loprintf("  Vindex == %b\n", (tag & SVINDEXMASK) >> 7);
		loprintf("  ECC == %b\n", (tag & SECC_MASK));
	} else {
		loprintf("Pcache line == %x\n", line);
		loprintf("  Raw tag == %x\n", tag);
		loprintf("  Tag Addr == %x\n", (tag & PADDRMASK) << PADDR_SHIFT);
		loprintf("  State == %s\n", pstate[(tag & PSTATEMASK) >> 6]);
		loprintf("  Parity == %b\n", tag & PPARITY_MASK);
	}
	return 0;
}

void _register(int rw, int *reg_name, int val, struct reg_struct *gprs)
{
	register int tmp;
	int number;
	int ertoip;

	if (*reg_name == 'r' || *reg_name == 'R'|| *reg_name == '$') {
		number = lo_atoi(reg_name + 1);
		if (number <= 0 || number > 31) {
			loprintf("*** Invalid register name.\n");
			return;
		}
		loprintf("r%a: %y\n", number,
					load_double_hi(gprs->gpr + 2*number),
					load_double_lo(gprs->gpr + 2*number));
	} else if (lo_strcmp(reg_name,"sp") == 0) {
		tmp = _sp(rw,val);
		if (rw == 0)
			loprintf("SP: %x\n",tmp);
	} else if (lo_strcmp(reg_name,"sr") == 0) {
		tmp = _sr(rw,val);
		if (rw == 0)
			loprintf("SR: %x\n",tmp);
	} else if (lo_strcmp(reg_name,"cause") == 0) {
		tmp = _cause(rw,val);
		if (rw == 0)
			loprintf("Cause: %x\n",tmp);
	} else if (lo_strcmp(reg_name,"epc") == 0) {
		tmp = _epc(rw,val);
		if (rw == 0)
			loprintf("EPC: %x\n",tmp);
	} else if (lo_strcmp(reg_name,"eepc") == 0) {
		tmp = _error_epc(rw,val);
		if (rw == 0)
			loprintf("EEPC: %x\n",tmp);
	} else if (lo_strcmp(reg_name,"config") == 0) {
		tmp = _config(rw,val);
		if (rw == 0)
			loprintf("Config: %x\n",tmp);
	} else if (lo_strcmp(reg_name,"wh") == 0) {
		tmp = _wh(rw,val);
		if (rw == 0)
			loprintf("WH: %x\n",tmp);
	} else if (lo_strcmp(reg_name,"wl") == 0) {
		tmp = _wl(rw,val);
		if (rw == 0)
			loprintf("WL: %x\n",tmp);
	} else if (lo_strcmp(reg_name,"all") == 0) {
		for (number = 0; number < 32; number ++) {
			loprintf("r%a: %y  ", number,
					load_double_hi(gprs->gpr + 2*number),
					load_double_lo(gprs->gpr + 2*number));
			if (!((number + 1) % 3))
				loprintf("\n");
		}
		loprintf("BVA: %y\n",
                                        load_double_hi(&(gprs->badva)),
                                        load_double_lo(&(gprs->badva)));
		loprintf("EPC: %y  Watch Hi:    %x  Watch Lo:    %x\n",
					load_double_hi(&(gprs->epc)),
					load_double_lo(&(gprs->epc)),
					_wh(0, 0), _wl(0,0));
		loprintf("Status:      %x  ErrEPC:      %x\n",
					gprs->sr,
					_error_epc());
		loprintf("Cause:       %x  ", gprs->cause);
		xlate_cause(gprs->cause);
		if (ertoip = load_double_lo((void*)EV_ERTOIP))
			xlate_ertoip(ertoip);
	} else {
		loprintf("*** Invalid register name.\n");
	}
}

void send_int(slot, cpu, number)
{
       store_double_lo(EV_SENDINT, ((slot&0xf)<<2) | (cpu&0x3) | (number<<8));
}


void conf_register(int rw, uint slot, uint reg_num, uint datahi, uint datalo,
						int repeat)
{
	int address;

	if (slot > 15) {
		loprintf("*** Slot out of range\n");
		return;
	}
	if (reg_num > 255) {
		loprintf("*** Reg. num. out of range\n");
		return;
	}

	address = 0xb8008000 + (slot << 11) + (reg_num << 3);

	if (rw) { /* Read */
		store_double_lo(((uint *)address), datalo);
		if (datalo >= 0x80000000)
			datahi = 0xffffffff;		
		else	
			datahi = 0;
	} else {
		datalo = load_double_lo((uint *)address);
		datahi = load_double_hi((uint *)address);
	}
#ifdef NOPRINT
	if (!repeat)
#endif /* NOPRINT */
		loprintf("Slot %b, Reg %b: %y\n", slot, reg_num,
							datahi, datalo);
}


set_endian(int big) {
	unsigned int scratch;

	/* Get word containing the endianness bit */
	scratch = load_double_lo((void*)EV_BE_LOC) & 0xff;

	/* Turn the endianness bit on or off */
	if (big)
		scratch |= EAROM_BE_MASK;
	else
		scratch &= ~EAROM_BE_MASK;

	/* Store the byte */
	write_earom(EV_BE_LOC, scratch);

	scratch = load_double_lo(EV_WCOUNT0_LOC);
	scratch += (load_double_lo(EV_WCOUNT1_LOC) << 8);

	loprintf("The EAROM has been written %d times.\n", scratch);
	fix_cksum();
}


uint read_reg(uint slot, uint reg_num)
{
	return load_double_lo((int *)(0xb8008000 + (slot << 11) + (reg_num << 3)));
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
	
	if ( !(load_double_lo((void *)EV_SYSCONFIG) & (1 << slot))) {
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
	uint t1, t2, t3;
	
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


clear_mc3_state()
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


clear_io4_state()
{
	uint io;
	int slot = 0;
	int leaf;

	io = occupied_slots() & ~(cpu_slots() | memory_slots());

	for (slot = 0; slot < 16; slot++) {
		if (io & (1 << slot)) {
			read_reg(slot, IO4_CONF_IBUSERRORCLR);
			read_reg(slot, IO4_CONF_EBUSERRORCLR);
			loprintf("  Cleared IO board %b's error registers\n", slot);
		}
	}
}


void memory(int rw, int size, uint addr, uint datahi, uint datalo, int repeat)
{
	int old_addr;
	int c = 0;
	int buf[SHORTBUFF_LENGTH];
	int verbose = 1;
	int looping = 0;
	int data1;

	for (;;) {
		old_addr = addr;
		if (rw == READ) {
			datahi = 0;
			switch (size) {
			case BYTE:
				datalo = load_ubyte((char *)addr);
				addr ++;
				break;
			case HALF:
				datalo = load_uhalf((short *)addr);
				addr += 2;
				break;
			case WORD:
				datalo = load_word((uint *)addr);
				addr += 4;
				break;
			case DOUBLE:
				datalo = load_double_lo((uint *)addr);
				datahi = load_double_hi((uint *)addr);
				addr += 8;
				break;
			default:
				break;
			}
		}
		else {
			switch (size) {
			case BYTE:
				store_byte(addr,datalo);
				addr ++;
				break;
			case HALF:
				store_half(addr,datalo);
				addr += 2;
				break;
			case WORD:
				store_word(addr,datalo);
				addr += 4;
				break;
			case DOUBLE:
				store_double_lo(addr,datalo);
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
				    loprintf("\n%x: %y ",old_addr,
						datahi,datalo);
			} else {
			    if (verbose)
				    loprintf("\n%x: %x ",old_addr,datalo);
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
					loprintf("%x: %y ",old_addr,
							datahi,datalo);
					break;
				    case WORD:
					loprintf("%x: %x ",old_addr,datalo);
					break;
				    case HALF:
					loprintf("%x: %h ",old_addr,datalo);
					break;
				    case BYTE:
					loprintf("%x: %b ",old_addr,datalo);
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
				loprintf("\r%x: ",addr);
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
/* CHANGE THIS FOR 64 BIT */
					datalo = lo_atoh(buf);
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
			(load_double_lo((uint *)EV_SPNUM) 
					& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT,
			load_double_lo((uint *)EV_SPNUM) & EV_PROCNUM_MASK);
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


/*
 * called by POD command mode.
 */
int mem_test(uint lo, uint hi, struct flag_struct *flags)
{
	volatile struct addr_range addr;
	volatile int fail;
	volatile uint tmp_sr;

	if (!(lo & 0xa0000000))
		addr.lomem = PHYS_TO_K1(lo);
	else
		addr.lomem = lo;
	if (!(hi & 0xa0000000))
		addr.himem = PHYS_TO_K1(hi);
	else
		addr.himem = hi;

	if (K1_TO_PHYS(lo) < LOMEM_STRUCT_END)
		loprintf("*** This test will overwrite PROM data structures.  Type reset to recover.\n");

	addr.inc = 1;			/* word test */
	addr.invert = BIT_TRUE;
	addr.dmask = 0xffffffff;	/* check them all */

	if (!flags->de) {
		loprintf("Turning off DE bit\n");
		tmp_sr = _sr(READ, 0);
		tmp_sr &= ~SR_DE;
		_sr(WRITE, tmp_sr);
	}

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

	if (!flags->de) {
		loprintf("Turning on DE bit\n");
		tmp_sr = _sr(READ, 0);
		tmp_sr |= SR_DE;
		_sr(WRITE, tmp_sr);
	}


	return fail;
}

int bwalking_addr(volatile struct addr_range *addr)
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
	for (testline = 1; ((uint)lomem | testline) <= (uint)himem; testline <<= 1)
	{
		mode = WRITE_RMEM;
		store_byte(refmem, 0x55);
		pmem = (caddr_t)((uint)lomem | testline);
		if (pmem == refmem)
			continue;
		mode = WRITE_PMEM;
		store_byte(pmem, 0xaa);
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
int addr_pattern(volatile struct addr_range *addr)
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
	while ((uint)pmem < (uint)pmemhi) {
		store_word(pmem, pmem);
		pmem += inc;
	}

	loprintf(".");
	pmem = (caddr_t)addr->lomem;

	mode = READING;
	while ((uint)pmem < (uint)pmemhi) {
		if ((data = (load_word(pmem) & mask)) != ((int)pmem & mask)) {
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
		store_word(ptr, new_pat);
	}
	restorefault(prev_fault);
	return fail;
}


int read_wr(volatile struct addr_range *addr)
{
	int fail = 0;
	uint *ptr;

	for(ptr = addr->lomem; ptr < addr->himem; ptr += addr->inc)
		store_word(ptr, 0);
	
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

void prm_crd(struct flag_struct *flags)
{
	char buffer[21];

	loprintf("Look at your LCD display!");
	sysctlr_message("Everest firmware by:");
	scroll_message("Steve Whitney, John Kraft, Brad Morrow, the number A, and the letter Q.", 1);
	if (pod_poll())
		pod_getc();
	sysctlr_message("Special thanks to:");
	scroll_message("The Everest Software Team, The Everest Hardware Team, The Mirage Graphics Team, Skinny Puppy, nix, Eleanor, and Members of the Academy.", 1);
	if (pod_poll())
		pod_getc();
	sysctlr_message("Legal Stuff:");
	scroll_message("Copyright 1993, Silicon Graphics Inc.", 1);
	if (pod_poll())
		pod_getc();
	loprintf("\r                            \r");
	code_msg(flags->diagval, buffer);
	sysctlr_bootstat(buffer, 0);
	sc_disp(flags->diagval);
}


/*
 * Error handling routines when running with stack on the 1st level cache
 */
int pon_memerror()
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

int jump_addr(uint address, uint parm1, uint parm2, struct flag_struct *flags)
{
	uint ret_val;
	uint tmp_sr;
	int sregs[40];	/* 9 sregs, ra * 2 ints + one spare for alignment */

	if (!flags->silent) {
		loprintf("Invalidating I-Cache\n");
	}
	call_asm(pon_invalidate_icache, 0);

	if (!flags->de) {
		loprintf("Turning off DE bit\n");
		tmp_sr = _sr(READ, 0);
		tmp_sr &= ~SR_DE;
		_sr(WRITE, tmp_sr);
	}

	if (!flags->silent)
		loprintf("Jumping to %x\n", address);

	save_sregs(sregs);

	/* Jump to the address passing the appropriate parameters.
 	 * If flags->mem is one, flush and invalidate the caches first.
	 */
	ret_val = pod_jump(address, parm1, parm2, flags->mem);

	restore_sregs(sregs);

	if (!flags->de) {
		loprintf("Turning on DE bit\n");
		tmp_sr = _sr(READ, 0);
		tmp_sr |= SR_DE;
		_sr(WRITE, tmp_sr);
	}

	return ret_val;
}


int launch_slave(uint slot, uint slice, uint address, uint parm)
{
	uint cpu;
	evcfginfo_t *cfginfo;
	evbrdinfo_t *brd;

	cpu = cpu_slots();

	if (!(cpu & 1 << slot)) {
		loprintf("*** Slot %b does not contain a CPU board.\n", slot);
		return EVDIAG_TBD;
	}
	brd = &(cfginfo->ecfg_board[slot]);

	if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_CPU) {
		loprintf("*** Slot %b's CPU board isn't recognized.\n", slot);
		return EVDIAG_TBD;
	}

	if (brd->eb_cpuarr[slice].cpu_vpid == CPUST_NORESP) {
		loprintf("*** CPU %b in slot %b isn't active.\n", slice, slot);
		return EVDIAG_TBD;
	}

	brd->eb_cpuarr[slice].cpu_launch = address;
	brd->eb_cpuarr[slice].cpu_parm = parm;

	/* Send interrupt */
	send_int(slot, cpu, LAUNCH_LEVEL);
	
	return 0;
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

#ifdef BRINGUP
pon_sload(int command, struct flag_struct *flags)
#else
pon_sload(int command, int* argv, struct flag_struct *flags)
#endif
{
	register length, address;
	register c1, c2;
	register int byte;
	int type, i;
	int first, done, reccount, save_csum;
	int csum, client_pc;
	int fbyte[4];
	int baud;
	int abort = 0;
	int errors = 0;

	loprintf("Clearing icache.\n");
	/* Zero out the icache */
	call_asm(pon_zero_icache, 0x80100000);

#ifndef BRINGUP
	/* baud rate */
	baud = lo_atoh(argv);
	loprintf("Download baud rate at 0x%x\n",baud);

	uart_loinit(1, baud);	/* set up dload port */
#endif /* !BRINGUP */
	loprintf("Ready to download...\n");

	reccount = 1;
	for (first = 1, done = 0; (!done && !abort); first = 0, reccount++) {

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
					store_word(address, i);
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
			client_pc = address;
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
#ifndef BRINGUP
				pod_putc(NAK);
#else
				errors++;
#endif
			} else {
#ifndef BRINGUP
				pod_putc(ACK);
#endif
			}
		}
	}

	if (!abort) {
		loprintf("Done downloading!\n");
		loprintf("Done: %x records, initial pc: 0x%x\n",
						reccount-1, client_pc);
		if (errors)
			loprintf("\t%x checksum errors.\n", errors);

		/* Get our stuff out where the icache can see it! */
		call_asm(pon_flush_dcache, 0);

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

#ifdef BRINGUP
pon_sload(int command, struct flag_struct *flags)
#else
pon_sload(int command, int* argv, struct flag_struct *flags)
#endif
{
	register length, address;
	register c1, c2;
	register int byte;
	int type, i;
	int first, done, reccount, save_csum;
	int csum, client_pc;
	int fbyte[4];
	int baud;

	loprintf("Clearing icache.\n");
	/* Zero out the icache */
	call_asm(pon_zero_icache, POD_SCACHEADDR);

#ifndef BRINGUP
	/* baud rate */
	baud = lo_atoh(argv);
	loprintf("Download baud rate at 0x%x\n",baud);

	uart_loinit(1, baud);	/* set up dload port */
#endif /* !BRINGUP */
	loprintf("Ready to download...\n");

	reccount = 1;
	for (first = 1, done = 0; !done; first = 0, reccount++) {

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
					store_word(address, i);
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
			client_pc = address;
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
#ifndef BRINGUP
			pod_putc(NAK);
#endif
		} else {
#ifndef BRINGUP
			pod_putc(ACK);
#endif
		}
	}
	loprintf("Done downloading!\n");
	loprintf("Done: %x records, initial pc: 0x%x\n", reccount-1, client_pc);

	/* Get our stuff out where the icache can see it! */
	call_asm(pon_flush_dcache, 0);

	if (command == SERIAL_RUN) {
		loprintf("Returned %x\n", jump_addr(client_pc, 0, 0, flags));
	}

	return 0;
bad:
	return 1;
}

#endif /* BAD_SLOAD */


int call_asm(int (*function)(uint), uint parm) {

	int sregs[40];	/* 9 sregs, ra * 2 ints + one spare for alignment */
	int result;

	result = save_and_call(sregs, function, parm);

	return result;
}

/* Write to the Everest System Reset register. */
void reset_system(){

	store_double_lo(EV_KZRESET, 0);
}


void zap_inventory()
{

	if (!nvram_okay()) {
		loprintf("NVRAM inventory is already invalid.\n");
		return;
	}
	set_nvreg(NVOFF_INVENT_VALID, 0);
	set_nvreg(NVOFF_OLD_CHECKSUM, nvchecksum());
}


void disp_cksum()
{
    ushort sum;

    sum = calc_cksum();

    loprintf("EAROM checksum is %h\n", sum);
}


void fix_cksum()
{
    ushort sum;
    int i;

    sum = calc_cksum();

    store_double_lo(EV_CKSUM0_LOC, sum & 0xff);
    delay(1000);
    store_double_lo(EV_CKSUM1_LOC, (sum >> 8) & 0xff);
    delay(1000);
    store_double_lo(EV_NCKSUM0_LOC, (sum & 0xff) ^ 0xff);
    delay(1000);
    store_double_lo(EV_NCKSUM1_LOC, ((sum >> 8) & 0xff) ^ 0xff);
    delay(1000);

    disp_cksum();
}

