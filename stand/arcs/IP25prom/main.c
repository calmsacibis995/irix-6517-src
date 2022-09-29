/*
 * File: main.c
 * Purpose: "C" code entry point for IP25 boot prom.
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

#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>
#include <sys/EVEREST/IP25.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/mc3.h>

#include "ip25prom.h"
#include "prom_externs.h"
#include "prom_intr.h"
#include "pod_failure.h"
#include "r10000-setup.h"

void	test_master(uint);
void	run_uncached(void (*function)(uint), uint);

extern unsigned char	prom_versnum;

#define MYDIAGVAL	(cfginfo.ecfg_board[slot].eb_cpuarr[slice].cpu_diagval)

#define	MAJOR(p)	(((p) & C0_MAJREVMASK) >> C0_MAJREVSHIFT)
#define	MINOR(p)	\
    ((MAJOR(p) == 1) ? (((((p) & C0_MINREVMASK) >> C0_MINREVSHIFT) == 1) ?  \
			    4 : (((p) & C0_MINREVMASK) >> C0_MINREVSHIFT))  \
                     : (((p) & C0_MINREVMASK)>> C0_MINREVSHIFT))
                        

static void
promVersionDisplay(evcfginfo_t *e)
/*
 * Function: 	promVersionDisplay
 * Purpose:	print out (in a pretty way), the version of the prom
 * Parameters:	e - pointer to everest config structure.
 * Returns:	Nothing
 */
{
    static const char *csd[] = {
	"rsvd", "1", "1.5", "2",
	"2.5", "3", "3.5", "4", 
	"rsvd", "rsvd", "rsvd", "rsvd", 
	"rsvd", "rsvd", "rsvd", "rsvd"
    };
    static const char *sccd[] = {
	"rsvd", "1", "1.5", "2", "2.5", "3", "rsvd", "rsvd"
    };
    static const char *scs[] = {
	"512K", "1MB", "2MB", "4MB", "8MB", "16MB", "*", "*"
    };
    __uint32_t	c, cc, v;
    __uint32_t	prid = (__uint32_t)readCP0(C0_PRID);

    c = (__uint32_t)DWORD_UNSWAP(*((__uint64_t *)IP25_CONFIG_ADDR));
    cc = (__uint32_t)GET_CC_REV();

    loprintf("\n\nIP25 SCC(%c) %s\n", cc + 'A', getversion());

    loprintf("R10000 %d.%d %dMHz %s", 
	     MAJOR(prid), MINOR(prid),
	     e->ecfg_clkfreq * 2,
	     (getendian() ? "LE" : "BE") );
    loprintf(" (%d-%d-%d/%d) %s\n\n",
	     (c & R10000_SCD_MASK) >> R10000_SCD_SHFT, 
	     (c & R10000_SCCD_MASK) >> R10000_SCCD_SHFT,
	     ((c & R10000_PRM_MASK) >> R10000_PRM_SHFT) + 1,
	     (c & R10000_SCCT_MASK) >> R10000_SCCT_SHFT,
	     scs[(c & R10000_SCS_MASK) >> R10000_SCS_SHFT]);

    if (e->ecfg_debugsw & VDS_DEBUG_PROM) {
	loprintf("R10000 Configuration (0x%x): \n", c);
	v = (c & R10000_PRM_MASK) >> R10000_PRM_SHFT;
	loprintf("\t%d(%d)\toutstanding reads\n", v+1, v);
	v = (c & R10000_SCD_MASK) >> R10000_SCD_SHFT;
	loprintf("\t%s(%d)\tPClk divisor\n", csd[v], v);
	v = (c & R10000_SCCD_MASK) >> R10000_SCCD_SHFT;
	loprintf("\t%s(%d)\tSClk divisor\n", sccd[v], v);
	loprintf("\t%s\tTandem Mode\n", c&R10000_SCCE_MASK ? "on" : "off");
	loprintf("\t%d\tSecondary Cache clock tap\n", 
		 (c & R10000_SCCT_MASK) >> R10000_SCCT_SHFT);
	loprintf("\n");
    }
}


/*
 * main()
 * 	The PROM main-line code, called only by the Boot Master
 *	processor once a stack in the primary d cache has been
 *	constructed (see entry.s and master.s for more info).
 *
 */

void
main()
{
    evcfginfo_t cfginfo;	/* Master system inventory */ 
    uint io4_err;		/* Flag indicating cw a broken IO4 */
    int	memval;			/* return value from memory configuration */
    int diagval;		/* diagnostic value */
    int slot, slice;		/* Our position on the midplane */
#if DEBUG
    int mslot;
    int type;
#endif
#if IP25MON || SABLE
    promVersionDisplay(1);
    invalidateCCtags();
    invalidateIcache();
    invalidateScache();
    podMode(EVDIAG_DEBUG, "\n\rIP25mon Ready\r\n");
#endif
    
    ccloprintf("\nWelcome to Everest manufacturing mode.\n");

    sysctlr_message("Initing Config Info");

    init_cfginfo(&cfginfo);

    slot = (int)((LD(EV_SPNUM) & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT);
    slice = (int)((LD(EV_SPNUM) & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);

    sysctlr_message("Setting timeouts.."); 
    set_timeouts(&cfginfo);


    if (get_BSR() & BSR_NODIAG)
	cfginfo.ecfg_debugsw |= VDS_NO_DIAGS;

    /*
     * Initialize the IO subsystems.
     */

    if (!any_io_boards()) {
	ccloprintf("*** No IO boards found in Ebus information.\n");
	io4_err = EVDIAG_NOIO4;
	sc_disp(EVDIAG_NOIO4);
	set_BSR(get_BSR() & ~BSR_EPCUART);
    } else {
	sysctlr_message("Initing master IO4..");
	io4_err = io4_initmaster(&cfginfo);
	if (io4_err) {
	    sc_disp(io4_err);
	    ccloprintf("*** IO4 error: %s\n", get_diag_string(io4_err));
	}

	/* test EPC */
	
	if (!io4_err) {
	    sysctlr_message("Initing EPC UART"); 
	    ccloprintf("Initializing EPC UART...\n");
	    init_epc_uart();

	    /* If we're not in manufacturing mode, switch over to
	     * the EPC UART.
	     */
	    if (! (get_BSR() & BSR_MANUMODE)) {
		set_BSR(get_BSR() | BSR_EPCUART);
		sysctlr_message("Output to EPC UART");
	    }
	}
    } /* no IO4 boards. */

    if (get_BSR() & BSR_POD)
	podMode(EVDIAG_DEBUG, "Jumping to POD mode\r\n");

    /* Print header string */

    promVersionDisplay(&cfginfo);
    if (cfginfo.ecfg_debugsw & VDS_MANUMODE)
	loprintf(" -- USING SYS. CTLR. UART -- \n");

    if (cfginfo.ecfg_debugsw & VDS_NO_DIAGS)
	loprintf(" -- DIAGNOSTICS DISABLED  -- \n");

    if (cfginfo.ecfg_debugsw & VDS_PODMODE)
	loprintf(" --   POD MODE ENABLED    -- \n");

    if (cfginfo.ecfg_debugsw & VDS_DEFAULTS)
	loprintf(" -- DEFAULTS MODE ENABLED -- \n");

    if (cfginfo.ecfg_debugsw & VDS_NOMEMCLEAR)
	loprintf(" -- LEAVING MEMORY INTACT -- \n");

    if (cfginfo.ecfg_debugsw & VDS_2ND_IO4)
	loprintf(" -- BOOTING FROM 2ND IO4  -- \n");

    if (cfginfo.ecfg_debugsw & VDS_DEBUG_PROM)
	loprintf(" --    DEBUG SWITCH ON    -- \n");

    loprintf("Initializing hardware inventory...              ");
    sysctlr_message("Reading inventory..");
    init_inventory(&cfginfo, (!io4_err && nvram_okay()));
    loprintf("...done.\n");

    MYDIAGVAL = EVDIAG_PASSED;
    (cfginfo.ecfg_board[slot].eb_cpuarr[slice].cpu_promrev ) =
			get_char(&(prom_versnum));

    configure_cpus(&cfginfo);

    /*
     * Test and initialize bus tags
     */

    if (!(cfginfo.ecfg_debugsw & VDS_NO_DIAGS)) {
	int (*f)(void);

	sysctlr_message("Testing Scache..");
	loprintf("Testing Secondary Cache...\t\t\t");
	if (diagval = testScache()) {
	    sc_disp(diagval);
	    loprintf("*FAILED*\n");
	    MYDIAGVAL = diagval;
	    prom_abdicate(diagval);
	} else {
	    loprintf("...passed.\n");
	}

	sysctlr_message("Testing Bus Tags..");
	loprintf("Testing and clearing bus tags...\t\t");

	if (GET_CC_REV() == 0) {
	    f = (int (*)(void))copyToICache((void *)testCCtags, 
			(__uint64_t)testCCtags_END - (__uint64_t)testCCtags);
	} else {
	    f = testCCtags;
	}

	diagval = f();
	invalidateCCtags();
	if (diagval) {
	    sc_disp(diagval);
	    loprintf("*FAILED*\n");
	    MYDIAGVAL = diagval;
	    /* Give up the job of bootmaster */
	    loprintf("*** Current bootmaster (%a/%a) failed diagnostics",
								slot, slice);
	    loprintf("  Rearbitrating...\n");
	    prom_abdicate(diagval);
	} else {
	    loprintf("...passed.\n");
	}
	invalidateCCtags();		/* avoid snooping */
	invalidateIcache();		/* since we just used it */
	invalidateScache();		/* and we used this too */
    }

    if (!any_mc_boards())
	podMode(EVDIAG_NOMC3, "\r\n*** No memory configured\r\n");

    /* 
     * Test and configure memory.
     */
    loprintf("Configuring memory...");

    memval = mc3_config(&cfginfo);

    if (memval != EVDIAG_PASSED)
	sc_disp(memval);

    switch (memval) {
	case EVDIAG_BIST_FAILED:
	    loprintf("*** Built-in memory self-test FAILED.\r\n");
	    break;
	case EVDIAG_BAD_ADDRLINE:
	    loprintf("*** Address line test FAILED.\r\n");
	    break;
	case EVDIAG_BAD_DATALINE:
	    loprintf("*** Data line test FAILED.\r\n");
	    break;
	case EVDIAG_BANK_FAILED:
	    loprintf("*** Bank that passed self-test FAILED software test.\r\n");
	    break;
	case EVDIAG_MC3DOUBLEDIS:
	    loprintf("\r\n*** Unrecoverable config failure.\r\n");
	    break;
	case EVDIAG_NO_MEM:
	    loprintf("\r\n*** No memory configured\r\n");
	    break;
	case EVDIAG_PASSED:
	    loprintf("\n                                                ...passed.\n");	
	    break;
    }

    /*
     * Transfer the evcfginfo data structure into memory. Initialize
     * the Everest MPCONF blocks. 
     */
#if DEBUG
    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
        if (cfginfo.ecfg_board[slot].eb_type == EVTYPE_MC3) {
            /* Make sure this really is a memory board */
            type = EV_GET_CONFIG(slot, MC3_TYPE);
            if (type != MC3_TYPE_VALUE)
                continue;
	    mslot = slot;
        }
    }

    dump_evconfig_entry(mslot); 
#endif

    if (!(cfginfo.ecfg_debugsw & VDS_NOMEMCLEAR)) {
	sysctlr_message("Writing CFGINFO..");
	loprintf("Writing cfginfo to memory\n");
	*EVCFGINFO = cfginfo;
    }
#if DEBUG
    dump_evconfig_entry(mslot); 
#endif

    sysctlr_message("Initing MPCONF blk..");
    loprintf("Initializing MPCONF blocks\n");
    init_mpconf();

    if (memval != EVDIAG_PASSED) {
	podMode(memval, "*** Memory test/configuration failed.\r\n");
    }

    /* Slaves will start testing their caches now. */

    /* Put stack in _uncached_ memory.  Run scache test. */
    run_uncached(test_master, io4_err);

    /* Doesn't return */
}

void
realWait(void)
{
    char	flags[32] = {0};
    __uint64_t	addr;

    SD(EV_SCRATCH, 0);
    SD(EV_CERTOIP, (__int64_t)-1);
    ccuart_puts("\n\n\rWaiting for launch .....");
    while (((addr = LD(EV_SCRATCH)) && 
	   0x00000000ffffffff) == 0) {
	delay(3000000);
	ccuart_putc('.');
    }

    ccuart_puts(" .... and they're off\n\r");
    SD(EV_SCRATCH, 0);

    jump_addr(addr, 0, 0, (void *)flags);
}    

void
wait(void)
{
    run_uncached(realWait, 0);
}
    
