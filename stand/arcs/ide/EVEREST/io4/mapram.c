/*
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/***********************************************************************
*	File:		mapram.c					*
*									*
*       Check the mapram area for its correctness.                      *
*									*
***********************************************************************/

#ident "arcs/ide/IP19/io/io4/mapram.c $Revision: 1.11 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/iotlb.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/vmecc.h>
#include <fault.h>
#include <setjmp.h>
#include <ide_msg.h>
#include <everr_hints.h>
#include <io4_tdefs.h>
#include <prototypes.h>

/* 
 * Function : mapram_test
 * Description:
 *	Run tests to check the MAPRAM. 
 *	Since the MAPRAM is a static ram, chances of an error are far less.
 *	so we just limit ourselves to a few tests.
 */

#define	ALL_5S	0x55555555
#define	ALL_AS	0xAAAAAAAA

#define MAPRAM_STEPSIZE	8
#define	MAPRAM_TESTLOCS	IO4_MAPRAM_SIZE/MAPRAM_STEPSIZE


#define	WINDOW(x)	((x >> 19) & 0x7)

static jmp_buf	map_buf;
static int 	maploc[2];

/*
 * test IO4 mapping ram
 *
 * if invoked as "mapram", tests mapping ram on all installed IO4 boards
 * if invoked as "mapram slot#, tests only the specifed slot -
 *   "mapram 3" would test slot three, for example.
 * the command line parse routine checks for range errors, empty/wrong type
 * slots, etc
 */
int
mapram_test(int argc, char **argv)
{

    int	slot, jval, retval; 

    retval = 0;

    /*
     * parse command line, to see if slot number was given
     */
    if (io4_select(FALSE, argc, argv))
	return(1);

    /*
     * iterate through all io adapters in the system
     */
    for (slot = EV_MAX_SLOTS; slot > 0; slot--) {

	/*
	 * if command line specified an IO4 slot number, start there
	 */
	if (io4_tslot)
	    slot = io4_tslot;

	/*
	 * if not an IO4, check next slot - no checking for io4_tslot, since
	 * if !0 it MUST be valid IO4 slot
	 */
	if (board_type(slot) != EVTYPE_IO4)
	    continue;

	msg_printf(VRB, "\nChecking Mapram in IO4 slot %d \n", slot);

	ide_init_tlb();   /* Initialize TLB */
	tlbrall();	  /* clear io tlb table in libsk/ml */
	io_err_clear(slot, 0); /* Clear the error registers */
	everest_error_clear();
	maploc[0] = slot; maploc[1] = 0; /* Physical location description */


	/*
	 * clear any old fault handlers, timers, etc
	 */
	clear_nofault();

	if (jval = setjmp(map_buf)){
	    if (jval != 2 )
		err_msg(MAPR_GENR, maploc);
	    everest_error_show();
	    retval++;
	    goto SLOTFAILED;
	}
	else{
	    nofault = map_buf;
	    retval = maptest(slot);
	}
#ifdef NEVER
	/* Post processing - skipping it for now */
	if (check_ioereg(slot, 0)){
	    err_msg(MAPR_ERR, maploc);
	    longjmp(map_buf, 2);
	}
#endif

SLOTFAILED:
	io_err_clear(slot, 0);	/* Clear the error registers */
	everest_error_clear();
	ide_init_tlb();	 	/* Clean up the tlb too */

	/*
	 * clear fault handlers again
	 */
	clear_nofault();
    }

    return(retval);
}

unchar
maptest(int io4slot)
{
    unchar	result=0;
    __psunsigned_t window, ram_addr, SR;

    window = io4_window(io4slot);
    if (((int)window < 0))
	return(1);

#if _MIPS_SIM != _ABI64
    if ((ram_addr = tlbentry(window, 0, 0)) == (__psunsigned_t)0)
	return(1);
#else
    ram_addr = (__psunsigned_t)(LWIN_PHYS(window, 0));
#endif

    SR = get_SR();

    if (IO4_GETCONF_REG(io4slot, IO4_CONF_ENDIAN))
        ram_addr += 4;	/* For Big-endian access */

    result += mapram_rdwr(ram_addr, io4slot);
    result += mapram_addr(ram_addr, io4slot);
    result += mapram_walk1s(ram_addr, io4slot); 

#if _MIPS_SIM != _ABI64
    tlbrmentry(ram_addr);
#endif
    return (result);

}

unsigned pattern[4] = {0x55555555, 0xAAAAAAAA, 0, 0xFFFFFFFF};

/* Ensures that all the bits of the RAM could be set 1/0 */

unsigned
mapram_rdwr(__psunsigned_t ram_addr, int io4slot)
{

    volatile __uint32_t *addr;
    unsigned     i,j, value, error=0;

    msg_printf(VRB, "Mapram Read/Write test. IO4 slot = %d\n", io4slot);

    for(i=0, addr = (__uint32_t*)ram_addr; i < MAPRAM_TESTLOCS ; i++, addr+=2){
	for (j=0; j < 4; j++){
	    *(addr) = pattern[j];
	    us_delay(10);
	    value   = *(addr);
	    if (value != pattern[j]){
		msg_printf(ERR,
		    "Mapram R/W Error:  At 0x%llx Wrote 0x%x Read 0x%x\n",
		    (long long) addr, pattern[j], value);
		/*
		err_msg(MAPR_RDWR, maploc, addr, pattern[j], value);
		*/
	        error++;
		if (continue_on_error() == 0)
		    break;
		    /* longjmp(map_buf, 2); */
	    }
	}
    }

    if (error == 0)
        msg_printf(VRB, "Passed Read/Write on Mapram. IO4 slot = %d\n", io4slot);

    return(error);
}

#define ADDRMASK	0xffffffff	/* mask for lower 32 bits only */
unsigned
mapram_addr(__psunsigned_t ram_addr, int io4slot)
{
    volatile __uint32_t *addr, value;
    unsigned     i, error=0 ;

    msg_printf(VRB, "Address test on Mapram. IO4 slot = %d \n", io4slot);

    addr = (__psunsigned_t *)ram_addr;

    for (i=0; i < MAPRAM_TESTLOCS ; i++, addr += 2)
        *(addr) = (__psunsigned_t)addr;

    addr = (__psunsigned_t *)ram_addr;

    for (i=0; i < MAPRAM_TESTLOCS; i++, addr+=2){
        if((value = *(addr)) != (__uint32_t)addr){
	    msg_printf(ERR,
		"Mapram Addr Error:  At 0x%llx Wrote 0x%x Read 0x%x\n",
		(long long) addr, (__uint32_t) addr, value);
	    /*
	    err_msg(MAPR_ADDR, maploc, addr, addr, value);
	    */
	    error++;
	    if (continue_on_error() == 0)
		break;
		/* longjmp(map_buf, 2); */
        }
    }

    if (error == 0)
	msg_printf(VRB,
	    "Passed Address Test on Mapram. IO4 slot = %d \n", io4slot);

    return(error);
}

/*
 * Function : mapram_walk1s
 * Description :  Walk 1s in MAPRAM Memory and check if it's ok
 */
unsigned
mapram_walk1s(__psunsigned_t ram_addr, int io4slot)
{
    volatile __uint32_t *addr;
    unsigned     i, j, value, error=0;

    msg_printf(VRB,"Walking 1s test on Mapram. IO4 slot = %d \n", io4slot);

    for(i=0, addr=(unsigned *)ram_addr; i < MAPRAM_TESTLOCS; i++,addr+=2 ){
	for(j=0; j <sizeof(unsigned); j++) {
	    *(addr) = (unsigned )(1 << j); 
	    if ((value = *(addr)) != (unsigned)(1<< j)){
		msg_printf(ERR,
		"Mapram Walking 1s Error:  At 0x%llx Wrote 0x%lx Read 0x%x\n",
		    (long long) addr, pattern[j], value);
		/*
                err_msg(MAPR_WALK1, maploc, addr, (1<<j), value);
		*/
		if (continue_on_error() == 0)
		    break;
		    /* longjmp(map_buf, 2); */
		error++;
	    }
	}
    }

    if (error == 0)
	msg_printf(VRB,
	    "Passed Walking 1s test on Mapram. IO4 slot = %d \n", io4slot);

    return(error);
}
