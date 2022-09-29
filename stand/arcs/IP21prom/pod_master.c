/***********************************************************************\
*       File:           pod_master.c                                    *
*                                                                       *
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/IP21addrs.h>
#include "ip21prom.h"
#include "prom_leds.h"
#include "pod_failure.h"
#include "prom_intr.h"
#include "prom_externs.h"
#include "pod.h"

#define SLAVE_TOUT	0x1000000	/* Somewhere around 1 minutes */
#define DISABLE_TOUT	0x100000

#define MYDIAGVAL   (EVCFGINFO->ecfg_board[slot].eb_cpuarr[slice].cpu_diagval)

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

        /* Skip non-IP21 boards */
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


void check_slaves(int slot, int slice) {
    int i, j, vpid;
    int done;
    int slave_errs = 0;
    unsigned int time, timeout, last_print, tout;
    unsigned int cur_time;
    int max_promrev = 0;	/* Highest PROM rev on the system */
    evbrdinfo_t *brdinfo;
    volatile unsigned char *stat;

    time = load_double_bloc((long long *)EV_RTC);
    timeout = time + SLAVE_TOUT;
    last_print = 0;
    tout = 0;

    for (i = 0; i < EV_MAX_SLOTS; i ++) {
	brdinfo = &(EVCFGINFO->ecfg_board[i]);
	if ((brdinfo->eb_type & EVCLASS_MASK) == EVCLASS_CPU) {
	    /* Print the slot we're looking at */
	    DPRINTF(" %a", i);
 	    for (j = 0; j < EV_CPU_PER_BOARD; j++) {
		if ((brdinfo->eb_cpuarr[j].cpu_vpid == CPUST_NORESP) ||
			    (brdinfo->eb_cpuarr[j].cpu_vpid == 0))
		    done = 1;
		else
		    done = 0;
		stat = &(brdinfo->eb_cpuarr[j].cpu_diagval);

		/* Keep track of the highest prom revisions on the
		 * machine.
		 * Can't tell the difference between 0 and CANTSEEMEM...
		 * Bummer.
		 */
		if ((brdinfo->eb_cpuarr[j].cpu_promrev > max_promrev) &&
				brdinfo->eb_cpuarr[j].cpu_promrev)
		    max_promrev = brdinfo->eb_cpuarr[j].cpu_promrev;
		do {
		    if (*stat == EVDIAG_NOT_SET) {
			*stat = EVDIAG_CANTSEEMEM;
		    }
		    switch (*stat) {
			case EVDIAG_TESTING_DCACHE:
			case EVDIAG_TESTING_ICACHE:
			case EVDIAG_TESTING_SCACHE:
			case EVDIAG_INITING_CACHES:
			case EVDIAG_WRCPUINFO:
			case EVDIAG_TESTING_CCJOIN:
			case EVDIAG_TESTING_CCWG:
			    break;
			default:
			    done = 1;
		    } /* switch */
		    cur_time = load_double_bloc((long long *)EV_RTC);
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
		vpid = brdinfo->eb_cpuarr[j].cpu_vpid;
		stat = &(brdinfo->eb_cpuarr[j].cpu_diagval);
		switch (*stat) {
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
			    if(*stat != EVDIAG_WG) {
			    	loprintf("*** Disabling slave CPU %d/%d, First diag Error(%d): %s\n", i, j, *stat, get_diag_string(*stat));
			    	brdinfo->eb_cpuarr[j].cpu_enable = 0;
			    	slave_errs++;
			    } else {
			    	loprintf("*** Slave CPU %d/%d failed write gatherer diag but will not be disabled.\n", i, j);
				*stat = EVDIAG_PASSED;
			    }
			}
		    } else {
			/* Master CPU */
			if(*stat != EVDIAG_PASSED)
			    loprintf("*** Master CPU %a/%a    Error: %s\n", i, j, get_diag_string(*stat));
		    }
		}
	    }
	} /* if EVCLASS_CPU */
    } /* for i */
}


void query_evcfg(uint *enabled_mem, uint *disabled_mem, int *enabled_cpus,
							int *disabled_cpus) {
    evbrdinfo_t *brdinfo;
    int i;
    int j;

    *enabled_mem = *disabled_mem = *enabled_cpus = *disabled_cpus = 0;

    for (i = 0; i < EV_MAX_SLOTS; i++) {
	brdinfo = &(EVCFGINFO->ecfg_board[i]);
	if ((brdinfo->eb_type & EVCLASS_MASK) == EVCLASS_CPU) {
	    for (j = 0; j < EV_CPU_PER_BOARD; j++) {
		DPRINTF("CPU %d/%d: diagval %x, enable %x\n", i, j,
			brdinfo->eb_cpuarr[j].cpu_diagval,
			brdinfo->eb_cpuarr[j].cpu_enable);
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

int cpu_masks[] = {
	0x3,	/* cpu 0: channels 0 and 1 */
	0xc	/* cpu 1: channels 2 and 3 */
};

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
		    mask |= cpu_masks[j];
		    continue;
		}
		if (brdinfo->eb_cpuarr[j].cpu_diagval != CPUST_NORESP) {
		    if (brdinfo->eb_cpuarr[j].cpu_enable) {
			mask |= cpu_masks[j];
			continue;
		    }
		}
		/* If we get here, we're disabled */

		/* Go flash our LEDs -
		 * 	Since the PROM's in "uncached" space, use the K1
		 *	macro.
		 */
		brdinfo->eb_cpuarr[j].cpu_launch =
					KPHYSTO32K1(IP21PROM_FLASHLEDS);

		/* Actually launch the slave */
		send_int(i, j, LAUNCH_LEVEL);

	    } /* for j */

	    /* Give CPUs a chance to jump. */
	    if (mask != EV_GET_CONFIG(i, EV_A_ENABLE)) {
	        time = load_double_bloc((long long *)EV_RTC);
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


void test_master(int io4_err)
{
    int failure_code = 0;
    int slot, slice;		/* Our position on the midplane */
    uint enabled_mem, disabled_mem;
    int enabled_cpus, disabled_cpus;

    slot =  (load_double_lo((long long *)EV_SPNUM)
				& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    slice = (load_double_lo((long long *)EV_SPNUM)
				& EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

    if (get_BSR() & BS_MANUMODE) {
        int i, first;

        for (i = 0, first = 1; i < EV_MAX_CPUS; i++) {
              if (MPCONF[i].bevecc) {
		     /*
		      * Don't print anything if we only got DB chip parity
		      * errors. These happen all the time.
		      */
		     if (MPCONF[i].bevecc <= 3)
			continue;
                     if (first) {
                             loprintf("*** Contents of ERTOIP Registers Before Reset:\n");
                             first = 0;
                        }
                     loprintf("*** CPU %d:\n", i);
                     xlate_ertoip((__uint64_t)MPCONF[i].bevecc);
              } 
        }
        loprintf("\n");
    }



    /* Check on the slaves. */

    if (!(EVCFGINFO->ecfg_debugsw & VDS_NO_DIAGS))
	loprintf("Checking slave processor diag results..");
    else
	loprintf("Checking slave processor status..");

    sysctlr_message("Checking slaves...");
    check_slaves(slot, slice);
    update_boot_stat(io4_err, slot, slice);
    set_ertoip_at_reset();

    if (!(EVCFGINFO->ecfg_debugsw & VDS_NO_DIAGS)) {
        sysctlr_message("Checking FPU...");
    	fputest();
    };

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
	pod_handler("\r\nIO4 board failed.\r\n", io4_err);
    }


    /* Clean stuff up. */
    pod_clear_ints();
    store_double_lo((long long *)EV_CIPL124, 0x6);
    store_double_lo((long long *)EV_CERTOIP, 0xffff);
    store_double_lo((long long *)EV_ERTOIP, 0);

    if (EVCFGINFO->ecfg_debugsw & VDS_PODMODE) {
	sysctlr_message("Entering POD mode.");
	pod_handler("\r\nEntering POD mode.\r\n", EVDIAG_DEBUG);
    }

    /* If NO_DIAGS isn't set and PODMODE isn't set, hard disable CPUs */
    if (!(EVCFGINFO->ecfg_debugsw & VDS_NO_DIAGS)) {
	hard_disable_cpus(slot, slice);
    } else if (disabled_cpus) {
	loprintf("NOTE: Soft-disabling CPUs.\n");
    }

    if ((EVCFGINFO->ecfg_memsize >> 12) < 32) {
	loprintf("*** Can't load IO prom with < 32 Megabytes of working memory.\n");
	sc_disp(EVDIAG_MC3NOTENOUGH);
	pod_handler("Insufficient memory.\r\n", EVDIAG_MC3NOTENOUGH);
    }

    /*
     * Download the IO4 PROM
     */
    if (!io4_err) {
        failure_code = load_io4prom();
	sc_disp(failure_code);
	pod_handler("IO4 prom failed.\r\n", failure_code);
    } else {
	sc_disp(io4_err);
	pod_handler("Cannot run without working IO4\r\n", io4_err);
    }
}
