/***********************************************************************\
*	File:		config.c					*
* 									*
*	This file contains the routines which do the first 		*
*	initialization of the evcfginfo data structure and		*
*	which subsequently read in the hardware inventory and 		*
*	device enable data from the EPROM.				*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/sysctlr.h>
#include <setjmp.h>
#include "ip19prom.h"
#include "prom_intr.h"

#define INV_READ(_b, _f) \
    get_nvreg(NVOFF_INVENTORY + ((_b) * INV_SIZE) + (_f))
#define INV_READU(_b, _u, _f) \
    get_nvreg(NVOFF_INVENTORY + ((_b) * INV_SIZE) + \
              INV_UNITOFF + ((_u) * INV_UNITSIZE) + (_f))



/* any_mc_boards()
 *	returns 1 if there's at least one MC3 board, else returns 0
 */
int
any_mc_boards()
{
    uint 	config_hi;		/* High-order bits of sysconfig */

    config_hi = load_double_hi(EV_SYSCONFIG);
    
    if (config_hi)
	return 1;
    else
	return 0;
}


/* any_io_boards()
 *	returns 1 if there's at least one IO4 board, else returns 0
 */
int
any_io_boards()
{
    uint 	config_hi;		/* High-order bits of sysconfig */
    uint	config_lo;		/* Low-order bits of sysconfig */

    config_lo = load_double_lo(EV_SYSCONFIG);
    config_hi = load_double_hi(EV_SYSCONFIG);
   
    if ((config_lo & 0xffff) & ~(((config_lo >> 16) & 0xffff) |
						(config_hi & 0xffff)))
	return 1;
    else
	return 0;
}


/*
 * init_cfginfo()
 *	Initializes the parameters in the evcfginfo to rational 
 *	default values.  
 */

void
init_cfginfo(evcfginfo_t *cfginfo)
{
    uint 	config_hi;		/* High-order bits of sysconfig */
    uint	config_lo;		/* Low-order bits of sysconfig */
    uint	i;			/* Iteration variable */
    uint 	type;			/* Board type */
    uint	freq;			/* clock frequency */
    evbrdinfo_t *brd;			/* Board pointer for referencing */

    /* Set basic fields in the config information structure.
     */
    cfginfo->ecfg_magic = EVCFGINFO_MAGIC;
    cfginfo->ecfg_secs = cfginfo->ecfg_nanosecs = 0;
    cfginfo->ecfg_memsize = 0;

    /* Calculate the clock frequency by loading it from the 
     * EAROM, one byte at a time.
     */
    freq = load_double_lo(EV_RTCFREQ3_LOC);
    freq = (freq << 8) | load_double_lo(EV_RTCFREQ2_LOC);
    freq = (freq << 8) | load_double_lo(EV_RTCFREQ1_LOC);
    freq = (freq << 8) | load_double_lo(EV_RTCFREQ0_LOC);
    if ((freq < MIN_FREQ) || (freq > MAX_FREQ))
	freq = DEFLT_FREQ;		/* Avoid flukey EAROM values */
    cfginfo->ecfg_clkfreq = freq; 
    
    /*
     * Iterate through all the boards and fill out the slot information.
     */
    config_lo = load_double_lo(EV_SYSCONFIG);
    config_hi = load_double_hi(EV_SYSCONFIG);

    for (i = 0; i < EV_MAX_SLOTS; i++) {
	/* 
	 * Set the board info fields to rational default values.
	 */
	brd = &(cfginfo->ecfg_board[i]);
	brd->eb_type 	  = EVTYPE_EMPTY;
	brd->eb_rev	  = 0;
	brd->eb_enabled   = 0;
	brd->eb_inventory = EVTYPE_EMPTY;
	brd->eb_diagval	  = EVDIAG_NOTFOUND;
	brd->eb_slot      = i;
	
	/*
 	 * Check to whether slot is occupied 
	 */ 
        if (config_lo & (1 << i)) {

	    /* Slot is occupied, so figure out what's actually in it */	    
	    if (config_lo & (1 << (i+16))) {

		/* Slot contains a CPU board */
		type = EV_GET_CONFIG(i, EV_A_BOARD_TYPE);
		if (type == EV_IP19_BOARD) 
		    cfginfo->ecfg_board[i].eb_type = EVTYPE_IP19;
		else  
		    cfginfo->ecfg_board[i].eb_type = EVTYPE_WEIRDCPU;
		 cfginfo->ecfg_board[i].eb_rev = EV_GET_CONFIG(i, EV_A_LEVEL);

	    } else if (config_hi & (1 << i)) {

		/* Slot contains a memory board */
		type = EV_GET_CONFIG(i, MC3_TYPE);
	        if (type == MC3_TYPE_VALUE)
		    cfginfo->ecfg_board[i].eb_type = EVTYPE_MC3;
		else  
		    cfginfo->ecfg_board[i].eb_type = EVTYPE_WEIRDMEM;

	    } else {
		/* Check for an IO4 */
		type = EV_GET_CONFIG(i, IO4_CONF_REVTYPE);
	
		if ((type & IO4_TYPE_MASK) == IO4_TYPE_VALUE)
		    cfginfo->ecfg_board[i].eb_type = EVTYPE_IO4;
		else 
	 	    cfginfo->ecfg_board[i].eb_type = EVTYPE_WEIRDIO;	
	    }

	    brd->eb_enabled  = 1;
	    brd->eb_diagval  = EVDIAG_PASSED;
	} 		/* end if */
    }		/* end for */

    /* Get the debug switch settings from the system controller and
     * store them in the config structure
     */

    cfginfo->ecfg_debugsw = sysctlr_getdebug();
}


/*
 * set_timeouts()
 * 	Goes through all of the boards in the system and ensures that
 *	the various timeout registers are set to the correct value.
 */

void
set_timeouts(evcfginfo_t *cfginfo)
{
    uint 	slot;
    evbrdinfo_t	*brd;

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	
	brd = &(cfginfo->ecfg_board[slot]);

	switch (brd->eb_type) {
	  case EVTYPE_IP19:
	    /* Already done in startup code -- see entry.s */
	    break;

	  case EVTYPE_MC3:
	    EV_SET_CONFIG(slot, MC3_DRSCTIMEOUT, RSC_TIMEOUT);
	    break;

	  case EVTYPE_IO4:
	    EV_SET_CONFIG(slot, IO4_CONF_RTIMEOUT, RSC_TIMEOUT);
	    EV_SET_CONFIG(slot, IO4_CONF_ETIMEOUT, EBUS_TIMEOUT);
	    break;

	  case EVTYPE_EMPTY:
	    /* Don't do anything--just want to avoid dropping into
	       next case */
	    break;

	  default:
	    loprintf("Unknown board type encountered.\n");
	    break;
	}
    }

    /* send sync interupt to all *A chips. */
    store_double_lo(EV_SENDINT, SYNC_DEST);
}


/*
 * init_inventory()
 *	Reads the NVRAM and fills in the inventory and virtual
 *	adapter fields for all of the boards in the system. This
 * 	also sets up board enable  value. This needs to be called
 *	before the diagnostic and initialization routines for 
 *	specific boards are invoked.
 */

int
init_inventory(evcfginfo_t *cfginfo, uint nv_valid)
{
    uint 	slot;		/* Slot containing board being inited */
    uint	unit;		/* Unit number for iterating */	
    evbrdinfo_t *brd;		/* Pointer for dereferencing */
    uint 	revision;	/* Revision value in NVRAM inventory */
    jmp_buf	fault_buf;	/* Buffer for jumping if a fault occurs */
    unsigned	old_buf;	/* Previous fault buffer value */
    uint	mult_exc = 0;	/* Flag for multiple exceptions */

    /* Set up an exception handler.  If something goes wrong while
     * we're trying to set up the inventory, we'll jump back to here
     * and clean up after ourselves.  We do this by making the nvram
     * look invalid.
     */
    if (setfault(fault_buf, &old_buf)) {
	if (mult_exc) {
	    loprintf("*** Multiple exceptions while initializing inventory.\n");
	    restorefault(old_buf);
	    return -1;
	} else 
	    mult_exc = 1;
 
	loprintf("*** Error occurred while reading NVRAM -- using defaults.\n");
	nv_valid = 0;
    }

    /* Switch off nv_valid if it the valid inventory flag isn't set */
    if (nv_valid) 
	if (get_nvreg(NVOFF_INVENT_VALID) != INVENT_VALID) 
	    nv_valid = 0;

    /* Iterate through all the boards and try to read in the
     * appropriate configuration info.
     */
    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	brd = &cfginfo->ecfg_board[slot];

	if (nv_valid) {

	    brd->eb_inventory = INV_READ(slot, INV_TYPE);
	    revision = INV_READ(slot, INV_REV);

 	    /* We don't want to mess with the enable and diagval fields
	     * if it looks like the board currently in the slot doesn't
	     * correspond to the NVRAM
	     */
	    if (brd->eb_inventory == brd->eb_type) {
		brd->eb_diagval = 0; 
		brd->eb_enabled  = INV_READ(slot, INV_ENABLE);
	    } else if (brd->eb_type == EVTYPE_EMPTY) {
		brd->eb_diagval = 0;
		brd->eb_enabled  = 0;
	    } else {
		brd->eb_diagval = EVDIAG_PASSED;
		brd->eb_enabled  = 1;
	    }

	} else {

	    /* NVRAM is broken.  We take our best guess and hope
	     * that everything is okay.
	     */
	    if (brd->eb_type != EVTYPE_EMPTY) {
		brd->eb_inventory = brd->eb_type;
		brd->eb_diagval   = EVDIAG_PASSED;
		brd->eb_enabled   = 1;
	    } else {
		brd->eb_inventory = EVTYPE_EMPTY;
		brd->eb_diagval	  = EVDIAG_NOTFOUND;
		brd->eb_enabled   = 0;
	    }	
	}    
     
	switch (brd->eb_type) {
	  case EVTYPE_IP19:
	    for (unit = 0; unit < EV_MAX_CPUS_BOARD; unit++) {
		evcpucfg_t *cpu = &(brd->eb_cpuarr[unit]);

		if (nv_valid && (brd->eb_type == brd->eb_inventory)) {
		    cpu->cpu_inventory = INV_READU(slot, unit, INVU_TYPE);
		    cpu->cpu_diagval   = EVDIAG_NOT_SET;
		    cpu->cpu_enable    = INV_READU(slot, unit, INVU_ENABLE);
		} else {
		    cpu->cpu_inventory = 1;
		    cpu->cpu_diagval   = EVDIAG_NOT_SET;
		    cpu->cpu_enable    = 1;
		}
	    } 
	    break;

	  case EVTYPE_IO4:
	    for (unit = 0; unit < IO4_MAX_PADAPS; unit++) {
		evioacfg_t *ioa = &(brd->eb_ioarr[unit]);
		
		if (nv_valid && (brd->eb_type == brd->eb_inventory)) {
		    ioa->ioa_inventory = INV_READU(slot, unit, INVU_TYPE);
		    ioa->ioa_diagval   = EVDIAG_PASSED;
		    ioa->ioa_enable    = INV_READU(slot, unit, INVU_ENABLE);
		} else {
		    ioa->ioa_inventory = 0;
		    ioa->ioa_diagval   = EVDIAG_PASSED;
		    ioa->ioa_enable    = 1;
		}
	    }
	    break;

 	  case EVTYPE_MC3:
	    for (unit = 0; unit < MC3_NUM_BANKS; unit++) {
		evbnkcfg_t *bnk = &(brd->eb_banks[unit]);
		if (nv_valid && (brd->eb_type == brd->eb_inventory)) {
		    bnk->bnk_inventory = INV_READU(slot, unit, INVU_TYPE);
		    bnk->bnk_diagval   = EVDIAG_PASSED;
		    bnk->bnk_enable    = INV_READU(slot, unit, INVU_ENABLE);
		} else {
		    bnk->bnk_inventory = 0;
		    bnk->bnk_diagval   = EVDIAG_PASSED;
		    bnk->bnk_enable    = 1;
		}

	    }
	    break;
	
	  case EVTYPE_EMPTY:
	    /* Place-holder which allows the empty slots to be ignored */
	    break;

	  default:
	    /* Bad stuff */
	    break;
	}
    } 

    /* Make sure that the previous fault handler is restored */
    restorefault(old_buf);
    return 0;
}	


/*
 * set_endianess()
 *	Checks the endianess specified in the NVRAM and 
 *	sends out an interrupt indicating that the slaves
 *	should switch to the specified endianess.  The 
 *	routine waits for awhile and then checks to see
 *	whether the system needs to be reset in order to
 *	achieve the desired endianess.
 * Parameters:
 *	None.
 */

void
set_endianess()
{
    uint bsr = get_BSR();	/* Grab the boot status register */
    uint endianess;		/* Default to the 1 true endianess */
    uint time;			/* Current time */

#if 0
    /* Read endianess from master EPC */
    if (bsr & BS_NVRAM_VALID) 
	endianess = get_nvreg(NVOFF_ENDIAN);
    else 
#endif
	endianess = load_double_lo(EV_BE_LOC) & EAROM_BE_MASK;


    /* Broadcast the preferred endianess to the world */
    if (endianess) {
	sysctlr_message("Big endian");
	loprintf("Big endian\n");
	SENDINT_SLAVES(BIGEND_LEVEL); 
    } else {
	sysctlr_message("little endian");
	loprintf("little endian\n");
	SENDINT_SLAVES(LITTLEND_LEVEL);
    }

    /* Wait for half a second for a response */
    delay(1000000);

    /* If we got a reset request, reset the machine */   
    if (load_double_lo(EV_IP0) & (1<<RESET_LEVEL)) {
	loprintf("\tSomeone changed endianess, resetting system...\n");
	sysctlr_message("ENDIAN SWITCH RESET");
	store_double_lo(EV_KZRESET, 1);
	while (1) /* Loop forever */ ;
    }

    /* If we got a reset request, reset the machine */   
    if (load_double_lo(EV_IP0) & (1<<FIXEAROM_LEVEL)) {
	loprintf("\tA processor's EAROM needed repair, resetting system...\n");
	sysctlr_message("EAROM REPAIR RESET");
	store_double_lo(EV_KZRESET, 1);
	while (1) /* Loop forever */ ;
    }
}


/*
 * configure_cpus()
 * 	Reads the IP0 register to determine which processors have
 * 	sent us the "alive" interrrupt and prints a string indicating
 *	that the processors are alive.  Processors which don't 
 *	respond get their enable entry switched off in the evcfginfo
 *	structure.
 */

void
configure_cpus(evcfginfo_t *info)
{
    uint	ip0[4];		/* Contents of the IP registers */
    uint	i;		/* Random index register */
    uint 	c;		/* CPU index register */
    evbrdinfo_t *brd;		/* Board information */ 
    uint 	bit;		/* Mask selecting processor alive bit */
    uint 	spnum;		/* SPNUM of the bootmaster */
    uint 	vpid = 0;	/* CPUS virtual processor ID */
    char	cpuinfo[EV_MAX_CPUS+1];
    char	*currcpu = cpuinfo;

    /* First capture the IP register contents */
    ip0[0] = load_double_lo(EV_IP0);
    ip0[1] = load_double_hi(EV_IP0);
    ip0[2] = load_double_lo(EV_IP1);
    ip0[3] = load_double_hi(EV_IP1);

    /* Make sure there aren't any extraneous interrupts which might 
       confuse the bootmaster disable/rearbitrate checking code below. */
    ip0[0] &= 0xffffff00;

    /* Clear out the cpuinfo character array */
    for (i = 0; i < EV_MAX_CPUS; i++)
	cpuinfo[i] = ' ';
    cpuinfo[i] = '\0';   /* Null terminate */
 
    /* Grab our spnum for future reference */
    spnum = load_double_lo(EV_SPNUM);

    for (i = 1; i < EV_MAX_SLOTS; i++) {
	brd = &(info->ecfg_board[i]);

	/* Skip non-IP19 boards */
        if (brd->eb_type != EVTYPE_IP19)
	    continue;
	   
	/* Check to see if this board has been disabled */
	if (!brd->eb_enabled) {
	    loprintf("\tAll processors in slot %d have been disabled\n", i);
	    for (c = 0; c < EV_MAX_CPUS_BOARD; c++)
		brd->eb_cpuarr[c].cpu_enable = 0;
	}

	for (c = 0; c < EV_MAX_CPUS_BOARD; c++) {

	    /* Special case the behaviour for the bootmaster */
	    if (spnum == ((i << 2) | c)) {

		/* Check to see whether the bootmaster is disabled. */
		if (!brd->eb_cpuarr[c].cpu_enable) {
		    if (ip0[0] | ip0[1] | ip0[2] | ip0[3]) {
			loprintf("*** Current bootmaster (%a/%a) is disabled.",
								i, c);
			loprintf("  Rearbitrating...\n");
			prom_abdicate(0);
		    } else {
			loprintf("*** WARNING: All available CPUs are ");
 			loprintf("disabled.  Re-enabling CPU %a/%a.\n", i, c);
			brd->eb_cpuarr[c].cpu_diagval = EVDIAG_CPUREENABLED;
			brd->eb_cpuarr[c].cpu_enable = 1;
		    }
		}

		brd->eb_cpuarr[c].cpu_vpid = vpid++;
		loprintf("    CPU %a/%a is bootmaster\n", i, c);
	        *currcpu++ = SCSTAT_BOOTMASTER;
		continue;
	    }

	    /* Check to see whether processor is alive */
	    bit = 1 << (((i % 4) * (EV_MAX_CPUS_BOARD * 2)) + (c*2));
	    if (ip0[i/4] & bit) {
		brd->eb_cpuarr[c].cpu_vpid =vpid++;
		
		/* See if processor is enabled */
		if (brd->eb_cpuarr[c].cpu_enable == 0) {
		    DPRINTF("but disabled");
		    *currcpu++ = SCSTAT_DISABLED;
		} else 
		    *currcpu++ = SCSTAT_WORKING;

		DPRINTF("(%d)\n", brd->eb_cpuarr[c].cpu_vpid);
	    } else {
		/* Processor didn't respond, either it isn't there
		   or it is seriously hosed */
		brd->eb_cpuarr[c].cpu_enable = 0;
		brd->eb_cpuarr[c].cpu_vpid = CPUST_NORESP;
		brd->eb_cpuarr[c].cpu_diagval = EVDIAG_NOTFOUND;
		*currcpu++ = SCSTAT_UNKNOWN; 
	    }

	    /* Switch off the bit corresponding to cpu being checked */
	    ip0[i/4] &= ~bit;
	}
    }

    /* Fire the CPU status string off to the system controller */
    sysctlr_message("CPU Status:");
    sysctlr_bootstat(cpuinfo, 0);

    /* We're all finished with the interrupts, so clear them out */
    pod_clear_ints();
}


/*
 * init_mpconf()
 * 	Sends out an interrupt instructing the slave processors
 *	to initialize their MPCONF entries.  Initializes the 
 *	master CPU's MPCONF.
 */

void
init_mpconf()
{
    uint ppid, vpid;		/* Virtual and physical processor ID's */
    uint slot, proc;
    uint speed;

    /* Zero the MPCONF region */
    for (vpid = 0; vpid < EV_MAX_CPUS; vpid++) {
	MPCONF[vpid].earom_cksum = 0;
	MPCONF[vpid].stored_cksum = 0;
	MPCONF[vpid].mpconf_magic = 0;
        MPCONF[vpid].phys_id = 0;
	MPCONF[vpid].virt_id = 0;
    }

    /* Tell the slaves to initialize their MPCONF entries */
    SENDINT_SLAVES(MPCONF_INIT_LEVEL);

    /* Initialize this processor's MPCONF block */
    ppid = load_double_lo(EV_SPNUM);
    slot = (ppid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    proc = (ppid & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;
    vpid = EVCFGINFO->ecfg_board[slot].eb_cpuarr[proc].cpu_vpid;
  
    /* Get this processor's speed */
    speed = load_double_lo(EV_EPROCRATE3_LOC);
    speed |= (speed << 8) | load_double_lo(EV_EPROCRATE2_LOC);
    speed |= (speed << 8) | load_double_lo(EV_EPROCRATE1_LOC);
    speed |= (speed << 8) | load_double_lo(EV_EPROCRATE0_LOC);
    EVCFGINFO->ecfg_board[slot].eb_cpuarr[proc].cpu_speed = speed / 1000000;
    
    /* Set this processor's cache size information */
    EVCFGINFO->ecfg_board[slot].eb_cpuarr[proc].cpu_cachesz = 
	load_double_lo(EV_CACHE_SZ_LOC);
 
    MPCONF[vpid].earom_cksum = 0;
    MPCONF[vpid].stored_cksum = 0;
    MPCONF[vpid].phys_id = ppid;
    MPCONF[vpid].virt_id = vpid;
    MPCONF[vpid].errno = 0;
    MPCONF[vpid].proc_type = EV_CPU_R4000;
    MPCONF[vpid].launch = 0;
    MPCONF[vpid].rendezvous = 0;
    MPCONF[vpid].bevutlb = MPCONF[vpid].bevnormal = MPCONF[vpid].bevecc = 0;
    MPCONF[vpid].scache_size = load_double_lo(EV_CACHE_SZ_LOC);
    MPCONF[vpid].nonbss = 0;
    MPCONF[vpid].stack = 0;
    MPCONF[vpid].lnch_parm = MPCONF[vpid].rndv_parm = 0;
    MPCONF[vpid].mpconf_magic = MPCONF_MAGIC;
    MPCONF[vpid].pr_id = get_prid();

    /* Setup the GDA */
    GDA->g_promop = 0;
    GDA->g_nmivec = 0;
    GDA->g_masterspnum = ppid;
    GDA->g_magic = GDA_MAGIC;
    GDA->g_vds = EVCFGINFO->ecfg_debugsw;
} 

