/***********************************************************************\
*       File:           pod_master.c                                    *
*                                                                       *
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>

#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/IP25addrs.h>
#include <sys/EVEREST/everror.h>

#include "ip25prom.h"
#include "prom_leds.h"
#include "pod_failure.h"
#include "prom_intr.h"
#include "prom_externs.h"
#include "pod.h"

#define	SLAVE_TOUT	0xc00000
#define DISABLE_TOUT	0x80000

#define  INV_READ(_b, _f) \
    get_nvreg(NVOFF_INVENTORY + ((_b) * INV_SIZE) + (_f))
#define INV_READU(_b, _u, _f) \
    get_nvreg(NVOFF_INVENTORY + ((_b) * INV_SIZE) + \
              INV_UNITOFF + ((_u) * INV_UNITSIZE) + (_f))
#define ABS_VAL(_x)		(((_x) < 0) ? -(_x) : (_x))

int	load_io4prom(void);
void	xlate_ertoip(__uint64_t);
void	set_ertoip_at_reset(void);

extern unsigned long MemSizes[];

void update_boot_stat(int io4_err, int slot, int slice) {
    uint nv_valid;
    evbrdinfo_t *brd;
    uint i, c;
    char cpuinfo[EV_MAX_CPUS+1];
    char *currcpu = cpuinfo;

    nv_valid = (nvram_okay() && !io4_err);

    /* Switch off nv_valid if it the valid inventory flag isn't set */
    if (nv_valid)
        if (get_nvreg(NVOFF_INVENT_VALID) != INVENT_VALID)
            nv_valid = 0;

    /* Clear out the cpuinfo character array */
    for (i = 0; i < EV_MAX_CPUS; i++)
        cpuinfo[i] = ' ';

    cpuinfo[i] = '\0';   /* Null terminate */

    for (i = 1; i < EV_MAX_SLOTS; i++) {
        brd = &(EVCFGINFO->ecfg_board[i]);

        /* Skip non-CPU boards */
        if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_CPU)
            continue;

	for (c = 0; c < EV_CPU_PER_BOARD; c++) {

	    /* Special case the behaviour for the boot master */
	    if ((i == slot) && (c == slice)) {
		*currcpu++ = SCSTAT_BOOTMASTER;
		continue;
	    }

	    /* Check to see whether processor is alive */
	    if ( brd->eb_cpuarr[c].cpu_vpid != CPUST_NORESP ) {
		/* See if processor is enabled */
		if (nv_valid && !INV_READU(i, c, INVU_ENABLE)) {
		    *currcpu++ = SCSTAT_DISABLED;
                } else if ( brd->eb_cpuarr[c].cpu_diagval ) {
		    *currcpu++ = SCSTAT_BROKEN;
		} else {
                    *currcpu++ = SCSTAT_WORKING;
		}
            } else {
                /* Processor didn't respond - either it isn't there
                   or it is seriously hosed */
		if (nv_valid && !INV_READU(i, c, INVU_ENABLE))
		    *currcpu++ = SCSTAT_DISABLED;
		else
                    *currcpu++ = SCSTAT_UNKNOWN;
            }
   	}
    } 

    /* Fire the CPU status string off to the system controller */
    sysctlr_bootstat(cpuinfo, 0);
}

static	__uint64_t
parity(__uint64_t p)
{
    p ^= p >> 32;
    p ^= p >> 16;
    p ^= p >> 8;
    p ^= p >> 4;
    p ^= p >> 2;
    p ^= p >> 1;
    return(p & 1);
}
static	__uint64_t sysad_ecc[] = {
    0xFF280FF088880928,
    0xFA24000F4444FF24,
    0x0B22FF002222FA32,
    0x0931F0FF11110B21,
    0x84D08888FF0F8C50,
    0x4C9F444400FF44D0,
    0x24FF2222F000249F,
    0x145011110FF014FF
};

static	unsigned int sysad_ecc_mask[] = {
    0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01
};

static	__uint64_t
cc_ecc(__uint64_t p)
{
    unsigned long ecc = 0;
    int	i;

    for (i = 0; i < sizeof(sysad_ecc) / sizeof(sysad_ecc[0]); i++) {
	ecc = (ecc << 1) | parity(p & sysad_ecc[i]);
    }
    return(ecc);
}
    

static	void
eccSysadDisplay(evreg_t syslo, evreg_t syshi)
{
    __uint64_t	p, sysmask;
    int	i, syserr;
    
    /* 
     * Now try to figure out which bits are bad in SYSAD data - we display
     * an number where a 1 isdicates that that bit MAY be in error. If only
     * 1 bit is on, then that IS the bit in error for correctable single bit 
     * errors.
     */
    syserr = 0;
    sysmask = 0;
    for (i = 0; i < 64; i++) {
	if (cc_ecc(syslo ^ (1LL << i)) == (syshi & 0xff)) {
	    loprintf("\t\tSYSAD Data[%d] appears bad\n", i);
	    syserr++;
	    sysmask |= 1LL << i;
	}
    }
    if (syserr > 1) {
	loprintf("\tSYSAD: Appears to have multiple errors\n");
    }
    if (syserr) {
	loprintf("\tSYSAD: SysData error possible in 0x%x\n", 
		 sysmask);
    } else {				/* must be in check bits */
	sysmask = (syshi & 0xff) ^ cc_ecc(syslo);
	for (i = 0; i < 8; i++)  {
	    if ((1 << i) & sysmask) {
		loprintf("\t\tSYSAD ECC[%d] appears bad\n", i);
	    }
	}
	loprintf("\tSYSAD: SysAD ECC check bit errors possible in: 0x%x\n", 
		 sysmask);
    }
}


void check_slaves(int slot, int slice) {
    int i, j;
    int done, paused;
    int slave_errs = 0;
    unsigned int time, timeout, last_print, tout;
    unsigned int cur_time;
    int max_promrev = 0;	/* Highest PROM rev on the system */
    evbrdinfo_t *brdinfo;
    volatile unsigned char *stat;
    evreg_t	ertoip;

    time = LD_RTC();
    timeout = time + SLAVE_TOUT;
    last_print = 0;
    tout = 0;
    paused = 0;

    for (i = 0; i < EV_MAX_SLOTS; i ++) {
	brdinfo = &(EVCFGINFO->ecfg_board[i]);
	if ((brdinfo->eb_type & EVCLASS_MASK) == EVCLASS_CPU) {
	    /* Print the slot we're looking at */
	    DPRINTF((" %a", i));
 	    for (j = 0; j < EV_CPU_PER_BOARD; j++) {
		if ((brdinfo->eb_cpuarr[j].cpu_vpid == CPUST_NORESP) ||
			    (brdinfo->eb_cpuarr[j].cpu_vpid == 0)) {
		    done = 1;
		}  else {
		    done = 0;
		}
		stat = &(brdinfo->eb_cpuarr[j].cpu_diagval);

		/* Keep track of the highest prom revisions on the
		 * machine.
		 */
		if ((brdinfo->eb_cpuarr[j].cpu_promrev > max_promrev) &&
				brdinfo->eb_cpuarr[j].cpu_promrev)
		    max_promrev = brdinfo->eb_cpuarr[j].cpu_promrev;
		do {
		    if (*stat == EVDIAG_NOT_SET) {
			*stat = EVDIAG_CANTSEEMEM;
			/*
			 * If no-diags are set, we can actually get here too
			 * fast and not give the slaves time to get into 
			 * one of the below states. We then end up disabling
			 * them. So there is a 1 second pause (if requried)
			 * the first time a CPU appears to have not made it
			 * yet when we think it should have.
			 */
			if (!paused) {
			    DELAY(1000000);
			    paused = 1;
			}
		    }
		    switch (*stat) {
		    case EVDIAG_TESTING_DCACHE:
		    case EVDIAG_TESTING_ICACHE:
		    case EVDIAG_TESTING_SCACHE:
		    case EVDIAG_INITING_CACHES:
		    case EVDIAG_INITING_SCACHE:
		    case EVDIAG_WRCPUINFO:
		    case EVDIAG_TESTING_CCJOIN:
		    case EVDIAG_TESTING_CCWG:
			break;
		    default:
			done = 1;
		    } /* switch */
		    cur_time = LD_RTC();
		    if (cur_time - last_print > 0x40000) {
			last_print = cur_time;
			loprintf(".");
		    }
		} while (!done && !(tout += timed_out(time, timeout)));
	    } /* for j */
	} /* if CPU class */
    } /* for i */

    loprintf("\n");

    if (tout)
	loprintf("*** Timed out waiting for slaves\n");

    for (i = 0; i < EV_MAX_SLOTS; i ++) {
	brdinfo = &(EVCFGINFO->ecfg_board[i]);
	if ((brdinfo->eb_type & EVCLASS_MASK) == EVCLASS_CPU) {
	    for (j = 0; j < EV_CPU_PER_BOARD; j++) {
		stat = &(brdinfo->eb_cpuarr[j].cpu_diagval);
		switch (*stat) {
		case EVDIAG_ERTOIP_COR:
		    loprintf("*** CPU %a/%a: recoverable errors, "
			     "reporting disabled\n", i, j);
		    xlate_ertoip(MPCONF[brdinfo->eb_cpuarr[j].cpu_vpid].ertoip);
		    *stat = EVDIAG_PASSED;
		    break;
		case EVDIAG_ERTOIP:
		    loprintf("*** CPU %a/%a: unrecoverable errors\n", i, j);
		    ertoip = MPCONF[brdinfo->eb_cpuarr[j].cpu_vpid].ertoip;
		    xlate_ertoip(ertoip);
		    if (ertoip & IP25_CC_ERROR_SBE_SYSAD) {
			evreg_t elo, ehi; 

			elo = EV_GETCONFIG_REG(i,j,EV_CFG_ERSYSBUS_LO_LO) | 
			   (EV_GETCONFIG_REG(i,j,EV_CFG_ERSYSBUS_LO_HI) << 32);
			ehi = EV_GETCONFIG_REG(i,j,EV_CFG_ERSYSBUS_HI);
			eccSysadDisplay(elo, ehi);
		    }
		    break;
		case EVDIAG_TESTING_DCACHE:
		    *stat = EVDIAG_DCACHE_HANG;
		    break;
		case EVDIAG_TESTING_SCACHE:
		    *stat = EVDIAG_SCACHE_HANG;
		    break;
		case EVDIAG_TESTING_ICACHE:
		    *stat = EVDIAG_ICACHE_HANG;
		    break;
		case EVDIAG_INITING_CACHES:
		    *stat = EVDIAG_CACHE_INIT_HANG;
		    break;
		case EVDIAG_WRCPUINFO:
		    *stat = EVDIAG_WRCFG_HANG;
		    break;
		}
	    }
	} /* if EVCLASS_CPU */
    } /* for i */

    for (i = 0; i < EV_MAX_SLOTS; i ++) {
	brdinfo = &(EVCFGINFO->ecfg_board[i]);
	if ((brdinfo->eb_type & EVCLASS_MASK) == EVCLASS_CPU) {
	    for (j = 0; j < EV_CPU_PER_BOARD; j++) {
		stat = &(brdinfo->eb_cpuarr[j].cpu_diagval);
		if (brdinfo->eb_cpuarr[j].cpu_vpid != CPUST_NORESP) {
		    /* Can't tell the difference between a rev. 0 prom and
		     * a CPU that can't see memory.
		     * Bummer.
		     */
		    if((brdinfo->eb_cpuarr[j].cpu_promrev < max_promrev)
					&& brdinfo->eb_cpuarr[j].cpu_promrev)
			loprintf("*** CPU %a/%a has a downrev PROM!\n", i, j);
		    if ((i != slot) || (j != slice)) {
			/* Not the CPU we're running on */
			if(*stat != EVDIAG_PASSED) {
			    loprintf("*** Disabling slave CPU %d/%d,"
				     " First diag Error(%d): %s\n", 
				     i, j, *stat, get_diag_string(*stat));
			    brdinfo->eb_cpuarr[j].cpu_enable = 0;
			    slave_errs++;
			}
		    } else {
			/* Master CPU */
			if(*stat != EVDIAG_PASSED) {
			    loprintf("*** Master CPU %a/%a    Error: %s\n", 
				     i, j, get_diag_string(*stat));
			}
		    }
		}
	    }
	} /* if EVCLASS_CPU */
    } /* for i */
}


void 
query_evcfg(uint *enabled_mem, uint *disabled_mem, 
	    int *enabled_cpus, int *disabled_cpus) 
{
    evbrdinfo_t *brdinfo;
    int i;
    int j;

    *enabled_mem = *disabled_mem = *enabled_cpus = *disabled_cpus = 0;

    for (i = 0; i < EV_MAX_SLOTS; i++) {
	brdinfo = &(EVCFGINFO->ecfg_board[i]);
	if ((brdinfo->eb_type & EVCLASS_MASK) == EVCLASS_CPU) {
	    for (j = 0; j < EV_CPU_PER_BOARD; j++) {
		DPRINTF(("CPU %a/%a: diagval %x, enable %x\n", i, j,
			brdinfo->eb_cpuarr[j].cpu_diagval,
			brdinfo->eb_cpuarr[j].cpu_enable));
		if (brdinfo->eb_cpuarr[j].cpu_diagval != CPUST_NORESP) {
		    if (brdinfo->eb_cpuarr[j].cpu_enable)
			(*enabled_cpus)++;
		    else
			(*disabled_cpus)++;
		}
	    } /* for j */
	} else if ((brdinfo->eb_type & EVCLASS_MASK) == EVCLASS_MEM) {
#ifdef SABLE
	    *(enabled_mem) = 8 << 12;	/* 8 mb */
#else
	    for (j = 0; j < MC3_NUM_BANKS; j++) {
		if (brdinfo->eb_banks[j].bnk_size != MC3_NOBANK)
		    if (brdinfo->eb_banks[j].bnk_enable)
			(*enabled_mem) += MemSizes[brdinfo->eb_banks[j].bnk_size];
		    else
			(*disabled_mem) += MemSizes[brdinfo->eb_banks[j].bnk_size];
	    } /* for j */
#endif
	}
    } /* for i */
}

void hard_disable_cpus(int slot, int slice)
{
    evbrdinfo_t *brdinfo;
    int i, j;
    int mask;
    unsigned timeout, time;

    for (i = 0; i < EV_MAX_SLOTS; i++) {
	brdinfo = &(EVCFGINFO->ecfg_board[i]);
	if ((brdinfo->eb_type & EVCLASS_MASK) == EVCLASS_CPU) {
	    mask = 0;
	    for (j = 0; j < EV_CPU_PER_BOARD; j++) {
		if ((i == slot) && (j == slice)) {
		    mask |= 1 << j;
		    continue;
		}
		if (brdinfo->eb_cpuarr[j].cpu_diagval != CPUST_NORESP) {
		    if (brdinfo->eb_cpuarr[j].cpu_enable) {
			mask |= 1 << j;
			continue;
		    }
		}
		/* If we get here, we're disabled */

		/* Go flash our LEDs -
		 * 	Since the PROM's in "uncached" space, use the K1
		 *	macro.
		 */
		brdinfo->eb_cpuarr[j].cpu_launch =
					KPHYSTO32K1(IP25PROM_FLASHLEDS);

		/* Actually launch the slave */
		send_int(i, j, LAUNCH_LEVEL);

	    } /* for j */

	    /* Give CPUs a chance to jump. */
	    if (mask != EV_GET_CONFIG(i, EV_A_ENABLE)) {
	        time = LD_RTC();
	        timeout = time + DISABLE_TOUT;
		while (!timed_out(time, timeout));
		    /* Actually disable CPUs */
		    EV_SET_CONFIG(i, EV_A_ENABLE, mask);
	    }
	}
    } /* for i */
}


char plural(int number)
{
    if (number != 1)
	return 's';
    else
	return ' ';
}

static	void 
stressSysAd(void)
{
    SD(EV_SCRATCH, 0xffffffffffffffff);
    SD(EV_SCRATCH, 0x5555555555555555);
    SD(EV_SCRATCH, 0xaaaaaaaaaaaaaaaa);
    SD(EV_SCRATCH, 0x0);
}

void test_master(int io4_err)
{
    int 	failure_code = 0;
    int 	slot, slice;		/* Our position on the midplane */
    uint 	enabled_mem, disabled_mem;
    int 	enabled_cpus, disabled_cpus;
    evreg_t	ertoip;

    slot  = (int)((LD(EV_SPNUM) & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT);
    slice = (int)((LD(EV_SPNUM) & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);

    /*
     * Check if there are any recoverable errors set in the 
     * ertoip register at this point, if so, print out the
     * error as a warning and continue.
     */

    stressSysAd();
    ertoip = LD(EV_ERTOIP);
    if (0 != ertoip) {
	xlate_ertoip(ertoip);
#       define	ECC	(IP25_CC_ERROR_SBE_SYSAD\
			 | IP25_CC_ERROR_SBE_INTR \
			 | IP25_CC_ERROR_ME)

	if (EVCFGINFO->ecfg_debugsw & VDS_DEBUG_PROM) {
	    if (ertoip & IP25_CC_ERROR_SBE_SYSAD) {
		eccSysadDisplay(LD(EV_ERSYSBUS_LO), LD(EV_ERSYSBUS_HI));
	    }
	}
	if (ertoip & ECC) {
	    SD(EV_ECCSB, EV_ECCSB_DSBECC | LD(EV_ECCSB));
	    loprintf(" *** Warning: Disabled single "
		     "bit error reporting\n");
	}
	
	LD(EV_ERTOIP);			/* wait for write  */
	SD(EV_CERTOIP, 0xffffffff);	
	stressSysAd();
	ertoip = LD(EV_ERTOIP);

	if (ertoip) {
	    loprintf(" *** Warning: unexpected errors - "
		     "unable to clear\n");
	} else {
	    loprintf(" *** Warning: unexpected errors"
		     " - cleared and continuing\n");
	}
    }

    /* Check on the slaves. */

    if (!(EVCFGINFO->ecfg_debugsw & VDS_NO_DIAGS))
	loprintf("Checking slave processor diag results..");
    else
	loprintf("Checking slave processor status..");

    sysctlr_message("Checking slaves...");
    check_slaves(slot, slice);
    update_boot_stat(io4_err, slot, slice);

    query_evcfg(&enabled_mem, &disabled_mem, &enabled_cpus, &disabled_cpus);
    loprintf("    Enabled %d Megabytes of main memory\n",
                enabled_mem >> 12);
    if (disabled_mem)
	    loprintf("    (Disabled %d Megabytes of main memory)\n",
                disabled_mem >> 12);
    loprintf("    Enabled %d processor%c\n", enabled_cpus,
						plural(enabled_cpus));
    if (disabled_cpus)
	    loprintf("    (Disabled %d processor%c)\n", disabled_cpus,
						plural(disabled_cpus));

#if SABLE
    io4_err = 0;				/* error? what error? */
    EVCFGINFO->ecfg_debugsw |= VDS_PODMODE;	/* drop into pod mode */
    EVCFGINFO->ecfg_memsize = enabled_mem;	/* kludge memory size */
#endif

    if (enabled_mem != (EVCFGINFO->ecfg_memsize)) {
	loprintf("*** Memory size in evconfig structure was corrupted.  Updating.\n");
	EVCFGINFO->ecfg_memsize = enabled_mem;
    }

    if (io4_err) {
	sc_disp(io4_err);
	podMode(io4_err, "\r\nIO4 board failed.\r\n");
    }


    /* Clean stuff up. */
    pod_clear_ints();
    SD_LO(EV_CIPL124, 0x6);
    SD_LO(EV_CERTOIP, 0xffffffff);

    if (EVCFGINFO->ecfg_debugsw & VDS_PODMODE) {
	sysctlr_message("Entering POD mode.");
	podMode(EVDIAG_DEBUG, "\r\nEntering POD mode.\r\n");
    }

    hard_disable_cpus(slot, slice);

    if ((EVCFGINFO->ecfg_memsize >> 12) < 32) {
	loprintf("*** Can't load IO prom with < 32 Megabytes of working memory.\n");
	sc_disp(EVDIAG_MC3NOTENOUGH);
	podMode(EVDIAG_MC3NOTENOUGH, "Insufficient memory.\r\n");
    }

    /*
     * Download the IO4 PROM
     */

    if (!io4_err) {
        failure_code = load_io4prom();
	sc_disp(failure_code);
	podMode(failure_code, "IO4 prom failed.\r\n");
    } else {
	sc_disp(io4_err);
	podMode(io4_err, "Cannot run without working IO4\r\n");
    }
}
