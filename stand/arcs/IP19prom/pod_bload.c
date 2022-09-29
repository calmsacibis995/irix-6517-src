/***********************************************************************\
*	File:		pod_bload.c					*
*									*
*	Contains the code for a handy-dandy binary download format.	*
*									*
\***********************************************************************/

#include <biendian.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "pod.h"


#define DATA	0x0d
#define END	0x0e

extern void pon_flush_dcache();
extern void pon_zero_icache(uint);

/*
 * pod_bload()
 *	Downloads a file in the jfk binary file format.
 *
 */

int
pod_bload(int command, struct flag_struct *flags)
{
    unsigned int address;
    unsigned short length, checksum, csum;
    unsigned int format, c, reccount, j;
        
    loprintf("Ready to download...\n");

    loprintf("Clearing icache.\n");
    call_asm(pon_zero_icache, POD_SCACHEADDR);

    reccount = 1;

    while (1) {
	    
	/* Read the header character */
	format = logetc(1);
	if (format == DATA) {
	
	    pul_set_cc_leds(reccount & 0xf);
    
	    /* Get the address */
	    address = logetc(1) & 0xff;
	    address |= (logetc(1) & 0xff) << 8;
	    address |= (logetc(1) & 0xff) << 16;
	    address |= (logetc(1) & 0xff) << 24;

	    /* Get the length */
	    length = logetc(1);
	    length |= (logetc(1) & 0xff) << 8;

	    /* Read the data */
	    for (j = 0; j < length; j++) {

		 c = logetc(1);		/* Read the character */

		 /* Update the checksum */
		 checksum += c;

		 *((char*)(address + j)) = (char) c;
	    }

	    /* Get the checksum */
	    csum = logetc(1);
	    csum |= (logetc(1) & 0xff) << 8;

	    if (csum != checksum) {
		loprintf("Checksum error on record %d\n", reccount);
		return 0;
	    }

	} else if (c == END) {
	    
	    /* Get start address */
	    address = logetc(1) & 0xff;
	    address |= (logetc(1) & 0xff) << 8;
	    address |= (logetc(1) & 0xff) << 16;
	    address |= (logetc(1) & 0xff) << 24;

	    loprintf("Done downloading!\n");
	    loprintf("%d records, Initial PC = 0x%x\n", reccount, address);

	    /* Get our stuff out where the icache can see it */
	    call_asm(pon_flush_dcache, 0);

	    if (command = BRUN) {
		loprintf("Returned %x\n", jump_addr(address, 0, 0, flags));
	    }

	    return 0;

	} else {
	    loprintf("Invalid formatting character: %d, download aborted\n",
		     format);

	    return 1;
	}

	reccount++;
    } /* End for */
}
