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
#include "sl.h"

#define PROM_OFF 4096		/* PROM starts at offset 4096 in Flash PROM */

/*
 * copy_prom(__scunsigned_t src, __scunsigned_t dest, uint numbytes, int window)
 *	Copies the requested number of bytes from the
 *	flash eprom to the specified memory address.
 *	As the copy is performed, a checksum is calculated
 *	and is returned on completion.  
 */

#define SPLIT	(512 * 1024)
#define MEG	(1024 * 1024)

uint
copy_prom(register __int64_t src,
	  register void * dstaddr, 
          __uint32_t numbytes,
	  int window)
{
    register __scunsigned_t offset = (EVCFGINFO->ecfg_epcioa<<LWIN_PADAPSHIFT) + src;
    register uint cksum = 0;
    register uint numtocopy;
    register __int64_t dest = (__int64_t)dstaddr;

    while (numbytes) {

	/* We can't allow the transfer to cross the boundary between
	 * Flash PROM windows 0 and 1, so we break up the transfer into
	 * two pieces if necessary and select the proper window.
	 */
	if (src < SPLIT) {
	    numtocopy = ((numbytes < (__uint32_t)(SPLIT - src)) ? 
			 numbytes : (__uint32_t)(SPLIT - src));
	    offset += EPC_LWIN_LOPROM;
	} else {
	    numtocopy = numbytes;
	    offset += EPC_LWIN_HIPROM - SPLIT;
	}

	cksum += load_lwin_half_store_mult(window, (__int64_t)offset, dest, numtocopy);
	dest += numtocopy;
	offset += numtocopy;
	src += numtocopy;
	numbytes -= numtocopy;
    }

    return cksum;
}

