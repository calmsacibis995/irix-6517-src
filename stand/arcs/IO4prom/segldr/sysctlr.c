/***********************************************************************\
*	File:		sysctlr.c					*
*									*
*	Contains sundry routines and utilities for talking to 		*
*	the Everest system controller.					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/nvram.h>
#include "sl.h"

#define SNUMSIZE 10

/*
 * sysctlr_message()
 *	Displays a message on the system controller LCD.
 */

void
sysctlr_message(char *message)
{
    ccuart_putc(SC_ESCAPE);
    ccuart_putc(SC_SET);
    ccuart_putc(SC_MESSAGE);

    ccuart_puts(message);
    ccuart_putc(SC_TERM);
}


void sysctlr_bootstat(char *message)
{
    ccuart_putc(SC_ESCAPE);
    ccuart_putc(SC_SET);
    ccuart_putc(SC_PROCSTAT);
    ccuart_puts(message);
    ccuart_putc(SC_TERM);
}


#define  INV_READ(_b, _f) \
    get_nvreg(NVOFF_INVENTORY + ((_b) * INV_SIZE) + (_f))
#define INV_READU(_b, _u, _f) \
    get_nvreg(NVOFF_INVENTORY + ((_b) * INV_SIZE) + \
              INV_UNITOFF + ((_u) * INV_UNITSIZE) + (_f))

void update_boot_stat()
{
    evbrdinfo_t *brd;
    uint i, c;
    char cpuinfo[EV_MAX_CPUS+1];
    char *currcpu = cpuinfo;
    unsigned int slot, slice;

    slot =  (*((long long *)EV_SPNUM)
	     & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    slice = (*((long long *)EV_SPNUM)
	     & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

    for (i = 0; i < EV_MAX_CPUS; i++)
	cpuinfo[i] = ' ';

    for (i = 1; i < EV_MAX_SLOTS; i++) {
	brd = &(EVCFGINFO->ecfg_board[i]);

	/* Skip non-cpu boards */
	if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_CPU)
	    continue;

	/* Check to see if this board has been disabled */
	if (brd->eb_enabled == 0)
	    for (c = 0; c < EV_CPU_PER_BOARD; c++)
		brd->eb_cpuarr[c].cpu_enable = 0;	

	for (c = 0; c < EV_CPU_PER_BOARD; c++) {

	    /* Special case the behaviour for the boot master */
	    if (slot == i && slice == c) {
		*currcpu++ = SCSTAT_BOOTMASTER;
		continue;
	    }

	    /* Check to see whether processor is alive */
	    if (brd->eb_cpuarr[c].cpu_diagval != EVDIAG_NOTFOUND) {
		/* See if processor is enabled */
		if (brd->eb_cpuarr[c].cpu_enable == 0) 
		    *currcpu++ = SCSTAT_DISABLED;
		else 
		    *currcpu++ = SCSTAT_WORKING;
	    } else {
		/* Processor didn't respond - either it isn't there
		   or it is seriously hosed */
		*currcpu++ = SCSTAT_UNKNOWN;
	    }
	}
    }
    *currcpu++ = '\0';

    /* Fire the CPU status string off to the system controller */
    sysctlr_bootstat(cpuinfo);

}

