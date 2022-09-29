/***********************************************************************\DBU
*	File:		main.c						*
*									*
*	Contains the main-line code which configures the major 		*
*	boards in the system and brings up the heart of the IP19	*
*	PROM.								*
*									*
\***********************************************************************/

#ident "$Revision: 1.40 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/mc3.h>
#include "ip19prom.h"
#include "prom_externs.h"
#include "prom_intr.h"
#include "pod_failure.h"

extern void test_master();
extern unsigned char prom_versnum;
extern char *get_diag_string(int);

#define MYDIAGVAL	(cfginfo.ecfg_board[slot].eb_cpuarr[slice].cpu_diagval)

/* This is for our DPRINTF macro. */
null() {}

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
    int bustagval;		/* bus tag test results */
    uint num_cpus;		/* Number of CPUS in system */
    int slot, slice;		/* Out position on the midplane */

    ccloprintf("\nWelcome to Everest manufacturing mode.\n");

    sysctlr_message("Initing Config Info");
    init_cfginfo(&cfginfo);   

    slot = (load_double_lo((uint *)EV_SPNUM)
				& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    slice = load_double_lo((uint *)EV_SPNUM) & EV_PROCNUM_MASK;

    sysctlr_message("Setting timeouts.."); 
    set_timeouts(&cfginfo);


    if (get_BSR() & BS_NO_DIAGS)
	cfginfo.ecfg_debugsw |= VDS_NO_DIAGS;

    /*
     * Initialize the IO subsystems.
     */

    if (!any_io_boards()) {
	ccloprintf("*** No IO boards found in Ebus information.\n");
	io4_err = EVDIAG_NOIO4;
	sc_disp(EVDIAG_NOIO4);
	set_BSR(get_BSR() & ~BS_USE_EPCUART);
    } else {
	sysctlr_message("Initing master IO4..");
	ccloprintf("Initializing master IO4...\n");
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
	    if (! (get_BSR() & BS_MANUMODE)) {
		set_BSR(get_BSR() | BS_USE_EPCUART);
		sysctlr_message("Output to EPC UART");
	    }
	}
    } /* no IO4 boards. */

    if (get_BSR() & BS_POD_MODE)
	pod_handler("Jumping to POD mode\r\n", EVDIAG_DEBUG);

    /* Print header string */
    loprintf("\n\nIP19 PROM (%s) %s\n", (getendian() ? "LE" : "BE"),
	 getversion());
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

    loprintf("Checking system endianess...                    ");
    sysctlr_message("Checking endianess..");
    set_endianess();

    loprintf("Initializing hardware inventory...              ");
    sysctlr_message("Reading inventory..");
    init_inventory(&cfginfo, (!io4_err && nvram_okay()));
    loprintf("...done.\n");

    MYDIAGVAL = EVDIAG_PASSED;
    (cfginfo.ecfg_board[slot].eb_cpuarr[slice].cpu_promrev ) =
			get_char(&(prom_versnum));

    num_cpus = configure_cpus(&cfginfo);

    /*
     * Test and initialize bus tags
     */
    if (!(cfginfo.ecfg_debugsw & VDS_NO_DIAGS)) {
	sysctlr_message("Testing Bus Tags..");
	loprintf("Testing and clearing bus tags...                ");
	if (bustagval = pod_check_bustags()) {
	    sc_disp(bustagval);
	    loprintf("Test FAILED.\n");
	    MYDIAGVAL = bustagval;
	    /* Give up the job of bootmaster */
	    loprintf("*** Current bootmaster (%a/%a) failed diagnostics",
								slot, slice);
	    loprintf("  Rearbitrating...\n");
	    prom_abdicate(bustagval);
	} else {
	    loprintf("...passed.\n");
	}
    } else {
	(void)pod_check_bustags();
    }

    if (!any_mc_boards())
	pod_handler("\r\n*** No memory configured\r\n", EVDIAG_NOMC3);

    /* 
     * Test and configure memory.
     */
    loprintf("Configuring memory...");

    memval = mc3_config(&cfginfo);

    if (memval != EVDIAG_PASSED)
	sc_disp(memval);

    switch (memval) {
	case EVDIAG_BIST_FAILED:
	    loprintf("*** Built-in memory self-test FAILED\n");
	    break;
	case EVDIAG_BAD_ADDRLINE:
	    pod_handler("*** Address line test FAILED.\r\n", memval);
	    /* Doesn't return */
	case EVDIAG_BAD_DATALINE:
	    pod_handler("*** Data line test FAILED.\r\n", memval);
	    /* Doesn't return */
	case EVDIAG_BANK_FAILED:
	    loprintf("*** Bank that passed self-test FAILED software test.\n");
	    break;
	case EVDIAG_MC3DOUBLEDIS:
	    pod_handler("\r\n*** Unrecoverable config failure.\r\n", memval);
	    /* Doesn't return */
	case EVDIAG_NO_MEM:
	    pod_handler("\r\n*** No memory configured\r\n", memval);
	    /* Doesn't return */
	case EVDIAG_PASSED:
	    loprintf("\n                                                ...passed.\n");	
	    break;
    }

    /*
     * Transfer the evcfginfo data structure into memory. Initialize
     * the Everest MPCONF blocks. 
     */
    sysctlr_message("Writing CFGINFO..");
    loprintf("Writing cfginfo to memory\n");
    *EVCFGINFO = cfginfo;

    sysctlr_message("Initing MPCONF blk..");
    loprintf("Initializing MPCONF blocks\n");
    init_mpconf();

    /* Slaves will start testing their caches now. */

    /* Put stack in _uncached_ memory.  Run scache test. */
    run_uncached((unsigned int)test_master, io4_err);

    /* Doesn't return */
}
