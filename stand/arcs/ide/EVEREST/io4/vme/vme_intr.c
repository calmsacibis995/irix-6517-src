
/*
 * io4/vme/vmeintr.c
 *
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

#include "vmedefs.h"
#include "libsk.h"
#include "uif.h"

#ident "arcs/ide/IP19/io/io4/vme/vme_intr.c $Revision: 1.10 $"

static int	vme_loc[3];
extern int 	io4_tslot, io4_tadap;
int vme_selfintr(int, int);

/*
 * vme_intr 	Test vme interrupt.
 *		Force vmecc to generate interrupt, and check if that reaches
 *		the CPU. In addition, place a handler at the appropriate vector
 *		and check if that is invoked.
 */

#define USAGE   "USAGE: %s [io4slot anum]\n"

vmeintr(int argc, char **argv)
{
    int 	fail=0;
    int		found=0;
    int    	slot, anum;
    int		jval;

    /* Initial Processing Needed */
    if (argc > 1){
        if (io4_select(1, argc, argv)){
	    msg_printf(ERR, USAGE, argv[0]);
	        return(1);
	}
    }

    while(!fail && io4_search(IO4_ADAP_FCHIP, &slot, &anum)){
        if ((argc > 1) && ((slot != io4_tslot) || (anum != io4_tadap)))
	    continue;

	if (fmaster(slot, anum) != IO4_ADAP_VMECC)
	    continue;

	found = 1;

        vme_loc[0] = slot ; vme_loc[1] = anum ;

        io_err_clear(slot, (1<< anum));
        setup_err_intr(slot, anum);

        if ((jval = setjmp(vmebuf))){
	    switch(jval){
	    default:
	    case 1: err_msg(VME_IGENR,vme_loc); break;
	    case 2: err_msg(VME_SWERR, vme_loc); break;
	    }
	    show_fault();
	    io_err_log(slot, (1<< anum));
	    io_err_show(slot, (1<< anum));
	    clear_err_intr(slot, anum);
        }
        else{
            set_nofault(vmebuf);
            fail |= vme_selfintr(slot, anum);
        }

        io_err_clear(slot, (1<< anum));
    }
    clear_nofault();

    if (!found) {
	if (argc > 1) {
	    msg_printf(ERR, "Invalid Slot/Adapter number\n");
	    msg_printf(ERR, USAGE, argv[0]);
	}
	fail = TEST_SKIPPED;
    }
    if (fail)
        io4_search(0, &slot, &anum);    /* reset variables for future use */

    return (fail);

}


vme_selfintr(int slot, int anum)
{
    unsigned mask, vector, ostatus;
    unsigned timeout;
    int      i, result, fail=0;

    msg_printf(INFO,"Checking VMECC interrupts, IO4 Slot %d, padap %d\n",
	slot, anum);

    ostatus = get_SR(); 
    set_SR(ostatus & ~SR_IMASK);

    if ((vmecc_addr = get_vmebase(slot, anum)) == 0)
	return(TEST_SKIPPED);

    vector  = (VMEIDE_ILEVEL << 8) | 0x40;

    EV_SET_LOCAL(EV_ILE, EV_EBUSINT_MASK); /* Enable CC to pass intrs */
    for (i = 1; i < 11; i++){
	fail = 1;
	while(mask=RD_REG(EV_HPIL))
	     WR_REG(EV_CIPL0, mask);
	
	msg_printf(DBG,"Testing VME intr level %d\n", i);
        mask = (1 << i);
        WR_REG(vmecc_addr+VMECC_INT_ENABLECLR, 0x1FFF);  /* Disable interrupt */
        WR_REG(vmecc_addr+(VMECC_VECTORERROR + (i*8)), vector);

        WR_REG(vmecc_addr+VMECC_INT_REQUESTSM, mask);
        WR_REG(vmecc_addr+VMECC_INT_ENABLESET, mask);    /* Enable One needed */

	timeout = 0xff;

	do {
	    ms_delay(5);
	    result = RD_REG(EV_HPIL);
	}while ( (result == 0) && (--timeout));

	if (timeout == 0) {
	    /* Now do a PIO and wait for the interrupt */
	    msg_printf(DBG,"Timeout waiting for Intr without PIO \n");
	    (void)RD_REG(vmecc_addr+VMECC_INT_REQUESTSM);
	    timeout = 0xff;
	    do{
		ms_delay(5);
		result = RD_REG(EV_HPIL);
	    }while ( (result == 0) && (--timeout));

	    if (timeout == 0){
	        err_msg(VMEINT_TO, vme_loc);
	        goto clear; 
	    }
	}

        if (result != VMEIDE_ILEVEL){
	    err_msg(VMEINT_ERR, vme_loc, VMEIDE_ILEVEL, result);
	    goto clear;
        }

        WR_REG(vmecc_addr+(VMECC_VECTORERROR + (i*8)), 0);
	fail = 0;

clear:
	/* Reusing result */
	while(result=RD_REG(EV_HPIL))
            WR_REG(EV_CIPL0, result);
	
	if (fail)
	    break;

    }
    clear_err_intr(slot, anum);
    set_SR(ostatus);

    if (fail)
        msg_printf(INFO, "FAILED Checking VMECC interruptibility\n");
    else
        msg_printf(INFO, "Passed Checking VMECC interruptibility\n");

    return(fail);

}
