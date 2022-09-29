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
#include "prom_externs.h"
#include "pod_failure.h"
#include "ip25prom.h"

#define PROM_OFF 4096		/* PROM starts at offset 4096 in Flash PROM */

/*
 * copy_prom(__scunsigned_t src, __scunsigned_t dest, uint numbytes)
 *	Copies the requested number of bytes from the
 *	flash eprom to the specified memory address.
 *	As the copy is performed, a checksum is calculated
 *	and is returned on completion.  
 */

#define SPLIT	(512 * 1024)
#define MEG	(1024 * 1024)

int	jump_addr(__psunsigned_t, uint, uint, struct flag_struct *);
uint	load_lwin_half_store_mult(int, int, __scunsigned_t, int);

uint
copy_prom(register __scunsigned_t src,
	  register __scunsigned_t dest, 
          uint numbytes)
{
    register __scunsigned_t offset = (master_epc_adap()<<LWIN_PADAPSHIFT) + src;
    register uint cksum = 0;
    register uint numtocopy;

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
	cksum += load_lwin_half_store_mult(EPC_REGION, offset, dest, numtocopy);
	dest += numtocopy;
	offset += numtocopy;
	src += numtocopy;
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
    seginfo_t seginfo;
    uint cksum;
    struct flag_struct flags;
    jmp_buf  fault_buf;		/* Fault status buffer */
    unsigned *old_buf;		/* Previous fault handler buf ptr */
    __psunsigned_t startaddr, entry;

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

    loprintf("Downloading PROM header information...");
    (void) copy_prom(0, (__scunsigned_t)&promhdr, sizeof(promhdr));
    DPRINTF(("Prom header transfer complete"));
    (void) copy_prom(SEGINFO_OFFSET, (__scunsigned_t)&seginfo, sizeof(seginfo_t));

    DPRINTF(("Completed downloading IO4prom\n"));
    if (promhdr.prom_magic != PROM_MAGIC) {
	loprintf("*** ERROR: Invalid magic number (%x) for IO4 PROM.\n",
							promhdr.prom_magic);
        restorefault(old_buf);
	return EVDIAG_BADMAGIC;
    }

    if (seginfo.si_magic != SI_MAGIC) {
	loprintf("*** ERROR: Invalid segment magic number (%x) for IO4 PROM.\n",
							seginfo.si_magic);
        restorefault(old_buf);
	return EVDIAG_BADMAGIC;
    }

    startaddr = seginfo.si_segs[0].seg_startaddr;
    entry = seginfo.si_segs[0].seg_entry;
    if (! IS_KSEGDM(startaddr)) {
	loprintf("*** ERROR: IO4 PROM start address 0x%x is invalid\n",
		 startaddr);
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
    loprintf("\nDownloading PROM code...");
    DPRINTF(("Reading 0x%x %d(0x%x) bytes\n", startaddr, 
	     promhdr.prom_length, promhdr.prom_length));
    if (IS_KSEG0(startaddr)) {
	DPRINTF(("Converting IO4prom address to uncached for loading\n"));
    }
    cksum = copy_prom(PROM_OFF, K0_TO_K1(startaddr), promhdr.prom_length);
    loprintf("\n");

    if (cksum != promhdr.prom_cksum) {
	loprintf("*** WARNING: IO4 PROM checksums don't match.\n");
	loprintf("\t Calculated 0x%x != stored 0x%x\n", cksum, 
		 promhdr.prom_cksum);
        restorefault(old_buf);
	return EVDIAG_BADCKSUM;
    }

    if ((entry < startaddr) || (entry > startaddr + promhdr.prom_length)) {
	loprintf("*** ERROR: IO4 PROM entry point (0x%x) is outside of PROM\n",
		 entry);
        restorefault(old_buf);
	return EVDIAG_BADENTRY;
    }
    /* Let the world know we're about to jump. */
    set_cc_leds(PLED_PROMJUMP);
    loprintf("Jumping into IO4 PROM.\n");
    flags.mem = 1;	/* Yes, we have memory */
    flags.silent = 1;	/* Don't squawk about clearing cache, jumping to... */
    setfault(0, &old_buf);
    jump_addr(entry, 0, 0, &flags);

    loprintf("IO4 PROM returned.  Weird.\n");
    return 0;
}
