/***********************************************************************\
*	File:		businfo.c					*
*									*
*	Display system-dependent bus information. Currently, only	*
*	Everest systems use this.					*
*									*
\***********************************************************************/

#include <arcs/types.h>

#ifdef EVEREST
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/addrs.h>
#include <libsc.h>
#include <libsk.h>

/*
 * Include the diagnostic value strings.
 */

#include <sys/EVEREST/diagval_strs.i>

/*
 * Array used to find the actual size of a particular
 * memory bank.  The size is in blocks of 256 bytes, since
 * the memory banks base address drops the least significant
 * 8 bits to fit the base addr in one word.
 */
unsigned long MemSizes[] = {
    0x04,    /* 0 = 4 meg    */
    0x10,    /* 1 = 16 megs   */
    0x20,    /* 2 = 32 megs  */
    0x40,    /* 3 = 64 megs  */
    0x40,    /* 4 = 64 megs  */
    0x100,   /* 5 = 256 megs */
    0x100,   /* 6 = 256 megs */
    0x0      /* 7 = no SIMM   */
};

/*
 * processor_name()
 *	Returns a string corresponding to the processor name.
 *	This would be a good spot to stuff revision strings too.
 */

char*
processor_name(int vpid)
{
    if ((MPCONF[vpid].pr_id & C0_IMPMASK) == 0x1000) {
	return("R8000");
    } else if ((MPCONF[vpid].pr_id & C0_IMPMASK) == 0x0900) {
	return("R10000");
    } else if ((MPCONF[vpid].pr_id & C0_REVMASK) >= 0x40) {
	return("R4400");
    } else {
	return("R4000");
    } 
}

/*
 * board_name()
 *      Returns a string pointer to the name of the board whose
 *      type value was passed.
 */
char*
board_name(unchar type)
{
    switch(type) {
        case EVTYPE_WEIRDCPU:   return "Unknown CPU-type";
        case EVTYPE_IP19:       return "IP19";
        case EVTYPE_IP21:       return "IP21";
	case EVTYPE_IP25:	return "IP25";
        case EVTYPE_WEIRDMEM:   return "Unknown Memory-type";
        case EVTYPE_MC3:        return "MC3";
        case EVTYPE_WEIRDIO:    return "Unknown IO-type";
        case EVTYPE_IO4:        return "IO4";
        case EVTYPE_UNKNOWN:    return "Unknown";
	case EVTYPE_EMPTY:	return "Empty";
        default:                return "Unrecognized boardtype";
    }
}


/*
 * ioa_name
 *      Returns a string pointer to the name of the I/O adapter
 *      whose type value is passed.
 */
char*
ioa_name(unchar type, unchar subtype)
{
    switch (type) {
	case IO4_ADAP_SCSI:     	return "S1 SCSI controller";
	case IO4_ADAP_SCIP:		return "SCIP SCSI controller";
        case IO4_ADAP_EPC:      	return "EPC Peripheral controller";
	case IO4_ADAP_FCHIP:
	    switch (subtype) {
	        case IO4_ADAP_FCG:     	return "FCI Graphics adapter";
                case IO4_ADAP_VMECC:   	return "VME adapter";
		case IO4_ADAP_HIPPI:	return "HIPPI adapter";
	        default:		return "Flat cable interconnect";
	    }
	case IO4_ADAP_DANG:		return "DANG";
        case IO4_ADAP_NULL:     	return "Empty";
        default:                	return "Unknown adapter type";
    }
}

/*
 * evdiag_msg()
 *	Returns a pointer to a user-readable diagval message.
 */

char*
evdiag_msg(unchar diagcode)
{
    int num_entries;
    int i;

    num_entries = sizeof(diagval_map) / sizeof(diagval_t);

    for (i = 0; i < num_entries; i++) {
        if (diagval_map[i].dv_code == diagcode)
            return diagval_map[i].dv_msg;
    }

    return "Unknown diagnostic code";
}


/*
 * show_ip()
 * show_mc3()
 * show_io4()
 * cpu_businfo()
 *	Display a physical hardware inventory for the machine.
 */

static void
show_ip(evbrdinfo_t *brd, int verbose) 
{
    evcpucfg_t *cpu;
    int i; 
    int num_cpus = 0;

    /* Count total number of cpus on board */
    for (i = 0; i < EV_CPU_PER_BOARD; i++)
	if (brd->eb_cpuarr[i].cpu_diagval != EVDIAG_NOTFOUND)
	    num_cpus++;

    printf("%s processor board with %d cpus (%s)\n", board_name(brd->eb_type),
		num_cpus, (brd->eb_enabled ? "Enabled" : "Disabled"));

    if (brd->eb_diagval != EVDIAG_PASSED)
	printf("     Diagnostic message: %s\n", 
	       evdiag_msg(brd->eb_diagval));
    
    if (verbose && brd->eb_enabled) {
	for (i = 0; i < EV_CPU_PER_BOARD; i++) {
	    cpu = &(brd->eb_cpuarr[i]);

	    /* Skip the slice if it doesn't look present */
	    if (cpu->cpu_diagval == EVDIAG_NOTFOUND) 
		continue;

	    printf("      Slice %d: %d Mhz %s with %d meg" 
		   " secondary cache (%s)\n", i,
		   ((brd->eb_type == EVTYPE_IP19) 
		    || (brd->eb_type == EVTYPE_IP25)) ? 
		       cpu->cpu_speed * 2 : cpu->cpu_speed,
		   processor_name(cpu->cpu_vpid), 
		   ((1 << cpu->cpu_cachesz) / (1024 * 1024)),
		   (cpu->cpu_enable ? "Enabled" : "Disabled"));

	    if (cpu->cpu_diagval != EVDIAG_PASSED && 
		cpu->cpu_diagval != EVDIAG_NOTFOUND) 
		printf("        Diagnostic message: %s\n", 
		       evdiag_msg(cpu->cpu_diagval));
	}
    }
}


static void
show_mc3(evbrdinfo_t *brd, int verbose)
{
    evbnkcfg_t *bnk;
    int i;
    int mem_size = 0;
    static int flip[] = {0, 4, 1, 5, 2, 6, 3, 7};

    /* Count total amount of memory on board */
    for (i = 0; i < MC3_NUM_BANKS; i++)
	mem_size += (MemSizes[brd->eb_banks[i].bnk_size] * 4);

    printf("MC3 memory board with %d megabytes of memory (%s)\n", 
	   mem_size, (brd->eb_enabled ? "Enabled" : "Disabled"));

    if (brd->eb_diagval != EVDIAG_PASSED)
	printf("     Diagnostic message: %s\n", 
	       evdiag_msg(brd->eb_diagval));

    if (verbose) {
	for (i = 0; i < MC3_NUM_BANKS; i++) {
	    bnk = &(brd->eb_banks[flip[i]]);

	    /* Skip this bank if it is empty */
	    if (bnk->bnk_size == MC3_NOBANK)
		continue;

	    printf("      Bank %c contains %d megabyte SIMMS (%s)\n", 
		   'A' + i, MemSizes[bnk->bnk_size], 
                   (bnk->bnk_enable ? "Enabled" : "Disabled"));
	    if (bnk->bnk_diagval != EVDIAG_PASSED)
		printf("       Diagnostics message: %s\n", 
		       evdiag_msg(bnk->bnk_diagval));
	}
    }
}


static void
show_io4(evbrdinfo_t *brd, int verbose)
{
    int i;

    printf("IO4 I/O peripheral controller board (%s)\n",
	   (brd->eb_enabled ? "Enabled" : "Disabled"));

    if (brd->eb_diagval != EVDIAG_PASSED)
	printf("     Diagnostic message: %s\n", 
	       evdiag_msg(brd->eb_diagval));

    if (verbose) {
	for (i = 1; i < IO4_MAX_PADAPS; i++) {
            evioacfg_t *ioa = &brd->eb_ioarr[i];

	    if (ioa->ioa_type == IO4_ADAP_NULL) 
		continue;
	
	    printf("      Adapter %d: %s (%s)\n", i, 
		   ioa_name(ioa->ioa_type, ioa->ioa_subtype),
		   (ioa->ioa_enable ? "Enabled" : "Disabled"));

	    if (ioa->ioa_diagval != EVDIAG_PASSED)
		printf("Diagnostic message: %s\n", 
		       evdiag_msg(ioa->ioa_diagval));
	}
    }
}

void
businfo(LONG flags)
{
	uint slot;
	evbrdinfo_t *brd;

	printf("\nSystem Bus Information:\n");
	for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
		
		brd = &(EVCFGINFO->ecfg_board[slot]);

		/* Skip empty slots*/
		if (brd->eb_type == EVTYPE_EMPTY) 
			continue;
		
		
		printf("   Slot %d: ", slot);

		switch (brd->eb_type) {
		    case EVTYPE_EMPTY:
			printf("Empty\n");
			break;

		    case EVTYPE_IP19:
		    case EVTYPE_IP21:
		    case EVTYPE_IP25:
			show_ip(brd, (int)flags);
			break;

		    case EVTYPE_MC3:
			show_mc3(brd, (int)flags);
			break;

		    case EVTYPE_IO4:
			show_io4(brd, (int)flags);
			break;

		    default:
			printf("Unrecognized board.\n");
			break;
		}
	}
	printf("\n");	
}

#else /* !EVEREST */

/* 
 * Non-everest businfo is a nop.
 */
/*ARGSUSED*/
void
businfo(LONG flags)
{
}

#endif
