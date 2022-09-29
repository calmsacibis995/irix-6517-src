/***********************************************************************\
*	File:		transfer.c					*
*									*
*	Attempts to read the IO4 Flash EPROM, transfer it into main	*
*	memory, and then execute it.  The master IO4 must have been	*
*	initialized before this routine is called.  The code assumes 	*
*	that the flash EPROM is in the standard location in		* 
*	IO window 1.							*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/promhdr.h>
#include <setjmp.h>
#include "pod.h"
#include "prom_leds.h"
#include "pod_failure.h"

#define PROM_OFF 4096		/* PROM starts at offset 4096 in Flash PROM */

/*
 * copy_prom(uint src, uint dest, uint numbytes)
 *	Copies the requested number of bytes from the
 *	flash eprom to the specified memory address.
 *	As the copy is performed, a checksum is calculated
 *	and is returned on completion.  
 */

#define SPLIT	(512 * 1024)
#define MEG	(1024 * 1024)

uint
copy_prom(register uint src, register uint dest, uint numbytes)
{
    register uint offset = (master_epc_adap() << LWIN_PADAPSHIFT) + src;
    register uint cksum = 0;
    register uint count, numtocopy;

    while (numbytes) {

	/* We can't allow the transfer to cross the boundary between
	 * Flash PROM windows 0 and 1, so we break up the transfer into
	 * two pieces if necessary and select the proper window.
	 */
	if (src < SPLIT) {
	    numtocopy = ((numbytes < (SPLIT - src)) ? numbytes : (SPLIT - src));
	    offset += EPC_LWIN_LOPROM;
	} else {
	    numtocopy = numbytes;
	    offset += EPC_LWIN_HIPROM - SPLIT;
	}

	count = numtocopy;
	while (count) {
	    register unsigned short value;

	    value = load_lwin_half(EPC_REGION, offset);
	    
	    offset += 2;
	    count  -= 2;
	    *((short*) dest) = value;
	    dest += 2;
	    
	    cksum += value;
	}

	numbytes -= numtocopy;
    }

    return cksum;
}


/*
 * load_io4prom()
 *	Copies the IO4 PROM out of the Flash EPROM and loads it
 *	into memory.
 */

int
load_io4prom()
{
    evpromhdr_t promhdr;
    uint cksum;
    struct flag_struct flags;
    jmp_buf  fault_buf;		/* Fault status buffer */
    unsigned old_buf;		/* Previous fault handler buf ptr */

    sysctlr_message("Loading IO4 PROM...");

    /*
     * Pull in the PROM header and check it
     */
    if (setfault(fault_buf, &old_buf)) {
	loprintf("*** A bus error occurred while downloading IO4 PROM.\n");
	restorefault(old_buf);
	return EVDIAG_DLBUSERR;
    }

    /* Set the LEDs appropriately */
    set_cc_leds(PLED_LOADPROM);

    loprintf("Downloading PROM header information...\n");
    (void) copy_prom(0, (uint) &promhdr, sizeof(promhdr));

    if (promhdr.prom_magic != PROM_MAGIC) {
	loprintf("*** ERROR: Invalid magic number for IO4 PROM.\n");
        restorefault(old_buf);
	return EVDIAG_BADMAGIC;
    }

    if (! IS_KSEGDM(promhdr.prom_startaddr)) {
	loprintf("*** ERROR: IO4 PROM start address 0x%x is invalid\n",
		 promhdr.prom_startaddr);
        restorefault(old_buf);
	return EVDIAG_BADSTART;
     }

    if (promhdr.prom_length > MEG) {
	loprintf("*** ERROR: IO4 PROM length 0x%x is excessive.\n",
		 promhdr.prom_length);
        restorefault(old_buf);
	return EVDIAG_TOOLONG;
    } 

    /*
     * Actually copy the bulk of the PROM into main memory UNCACHED
     */
    cksum = copy_prom(PROM_OFF, K0_TO_K1(promhdr.prom_startaddr),
						promhdr.prom_length);

    if (cksum != promhdr.prom_cksum) {
	loprintf("*** WARNING: IO4 PROM checksums don't match.\n");
	loprintf("\t Calculated 0x%x != stored 0x%x\n", cksum, 
		 promhdr.prom_cksum);
        restorefault(old_buf);
	return EVDIAG_BADCKSUM;
    }

    if ((promhdr.prom_entry < promhdr.prom_startaddr) ||
	(promhdr.prom_entry > 
		promhdr.prom_startaddr + promhdr.prom_length)) {
	loprintf("*** ERROR: IO4 PROM entry point (0x%x) is outside of PROM\n",
		 promhdr.prom_entry);
        restorefault(old_buf);
	return EVDIAG_BADENTRY;
    }
    
    /* Let the world know we're about to jump. */
    set_cc_leds(PLED_PROMJUMP);
    loprintf("Jumping into IO4 PROM.\n");
    flags.de = 1;	/* Leave cache error detection disabled for now. */
    flags.mem = 1;	/* Yes, we have memory */
    flags.silent = 1;	/* Don't squawk about slearing cache, jumping to... */
    setfault(0, &old_buf);
    jump_addr(promhdr.prom_entry, 0, 0, &flags);

    loprintf("IO4 PROM returned.  Weird.\n");
    return 0;
}
