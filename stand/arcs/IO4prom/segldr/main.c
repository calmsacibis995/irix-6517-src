/***********************************************************************\
*	File:	main.c							*
*									*
*	Contains code to manage IO4 prom segment loading.		*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/promhdr.h>
#include "sl.h"

volatile char *epcbase;
char *crlf = "\r\n";

/*
 * main-
 *	No one calls us with any arguments since we're called straight
 *	from the prom.
 *
 */
main()
{
    int prid;
    int segtype;
    seginfo_t seginf;
    int error = 0;
    int seg_to_load;
    int magic_key = 0;

    /* Get our processor revision id. */
    prid = getprid() & C0_IMPMASK; 

    switch (prid) {
    case 0x400:
	R4000InitSerialPort();
	break;
    case 0x800:
	R8000InitSerialPort();
	break;
    case 0x900:
	R10000InitSerialPort();
	break;
    default:
	R4000InitSerialPort();		/* hope it works */
	sysctlr_message("Unknown CPU Type");
	break;
    }

    sysctlr_message("PROM Segment Loader");

    /* Set up the UART - Just find the base of its addresses. */
    epcbase = (char *)SWIN_BASE(EPC_REGION, EVCFGINFO->ecfg_epcioa) + 7;

    /* Determine which segment type we should load. */
    /* XXX - Eventually, look up our board type to match as well. */
    segtype = which_segtype(prid, 0);

    loprintf("\nPROM Segment Loader (%s) %s\n", 
	     which_segname(prid, 0), getversion());
#if SABLE
    seg_to_load = -1;
#else
    if (!get_segtable(&seginf, EPC_REGION)) {
	error = 1;
	loprintf("Bad magic number on segment table!\n");
    }

    /* Find out which segment we should load. */
    seg_to_load = which_segnum(&seginf, segtype);
#endif

    if (seg_to_load == -1) {
	error = 1;
	loprintf("Couldn't find a segment for %s.\n", seg_string(segtype));
    }

    /* See if the magic key is pressed. */
    while (poll()) {
	if (lo_getc() == MAGIC_KEY)
	    magic_key = 1;
    }

    /* Load the segment and execute it! */
    if (!error && !magic_key) {
	loprintf("Loading and executing %s boot prom image...\n\n",
		 seg_string(segtype));
	load_and_exec(&seginf.si_segs[seg_to_load], EPC_REGION);
    }

    /* If the magic key is pressed, we can't find an appopriate segment,
     * or the load fails, run the command interpreter.
     */
    do_menu(&seginf);
    return(0);
}


void
do_menu(seginfo_t *si)
{
    char choice;
    ulong slot;
    int window = EPC_REGION;
    char line[80];
    char *addr;

    for (;;) {

	flush();

	sysctlr_message("Segment Loader Menu");

	loprintf("\nEverest PROM Segment Loader\n\n"); 
	loprintf(" L) List segments\n");
	loprintf(" P) POD mode\n");
	loprintf(" R) Reset the machine\n");
	loprintf(" S) Select a slot\n");
	loprintf(" X) Load and execute a segment\n");
	loprintf("\nOption? ");

	choice = lo_getc();

	while ((choice == '\r') || (choice == MAGIC_KEY))
	    choice = lo_getc();

	if (choice >= 'a' && choice <= 'z')
		choice -= ('a' - 'A');

	loprintf("%c\n", choice);

	switch(choice) {
	    case 'L':
		loprintf("\n");
		list_segments(si);
		break;

	    case 'S':
		flush();
		loprintf("Enter a slot number (1-f): ");
		logets(line, 80);
		slot = lo_atoh(line);
		if (slot < 1 || slot > 0xf) {
		    loprintf("Slot number (0x%b) is out of range\n");
		    break;
		}

		/* Find the window for slot x */
		window = get_window((int)slot);

		if (window < 1 || window > 7) {
			loprintf("No window for slot 0x%b\n", slot);
			window = EPC_REGION;
		}

		get_segtable(si, window);
		break;

	    case 'X':
		flush();
		loprintf("Load which segment? ");
		logets(line, 80);
		slot = lo_atoh(line);
		if (slot >= si->si_numsegs) {
		    loprintf("Segment number (0x%b) is out of range\n", slot);
		    break;
		}

		loprintf("Loading and executing segment 0x%b\n", slot);
		load_and_exec(&si->si_segs[slot], window);
		break;

	    case 'C':
		flush();
		loprintf("Copy in which segment? ");
		logets(line, 80);
		slot = lo_atoh(line);
		if (slot >= si->si_numsegs) {
		    loprintf("Segment number (0x%b) is out of range\n", slot);
		    break;
		}

		loprintf("Copying in segment 0x%b\n", slot);
		load_segment(&si->si_segs[slot], window);
		loprintf("Entry point == 0x%x\n", si->si_segs[slot].seg_entry);
		break;
	   case 'R':
		*((volatile int *)EV_KZRESET) = 1;
		break;

	   case 'J':
		loprintf("Jump to what address? ");
		logets(line, 80);
		addr = (char *)lo_atoh(line);
		loprintf("Jumping to address 0x%x\n", addr);
		((void (*)())addr)();
		break;
	
	   case 'P':
		addr = (char *)EV_PROM_EPCUARTPOD;
		if ((getprid() & C0_IMPMASK) == 0x400)
			/* R4000 - IP19 prom is 32-bit */
			addr = (char *)((__psint_t)addr | 0xffffffffbfc00000LL);
		loprintf("Jumping to POD mode (address 0x%x)\n",
				addr);
		((void (*)()) addr)();
		break;
	
	   default:
		loprintf("???\n");
	}
    }
}
