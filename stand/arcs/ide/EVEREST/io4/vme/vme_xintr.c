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

#ident "arcs/ide/IP19/io/io4/vme/vme_xintr.c $Revision: 1.8 $"

/*
 * vme_xintr 	Test external vme interrupt on IO4 board 
 *		Force vme interrupt, on some IPI controller board 
 *		Check the registers in IP19 to see if an interrupt is 
 *		registered. 
 */

#include <sys/cpu.h>
#include "vmedefs.h"
#include "uif.h"

extern int ipi_vmeintr(int, int, int, int);

#define SCSI_PEND	0x10000
#define SCSI_MASK	0x1000000

int	vme_loc[4];
vme_xintr(int argc, char **argv)
{
    int fail = 0;
    int unit, base, slot, anum;

    /* Pre Processing Needed */

    if (argc != 3){
	msg_printf(ERR,"Invalid number of parameters: %d\n", argc);
	msg_printf(ERR,"Usage: vme_xintr io4slot, vme-anum\n");
	return(1);
    }

    vme_loc[0] = slot = atoi(argv[1]);
    vme_loc[1] = anum = atoi(argv[2]);

    vme_loc[2] = -1;

    ide_init_tlb();
    setup_err_intr(slot, anum);
    io_err_clear(slot, anum);

    if (fail = setjmp(vmebuf)){
        /* Error Case */
	switch(fail){
	    case 1:
	    default: err_msg(VME_XINTR, vme_loc); break;
	    case 2: break;
	}
	io_err_log(slot, anum);
	io_err_show(slot, anum);
    }
    else{
        nofault = vmebuf;
        fail = ipi_vmeintr(slot, anum, unit, 0);
    }

    nofault = 0;
    return (fail);
}

#define	VME_INTLVL	EVINTR_LEVEL_VMECC_IRQ1(0, 1)

ipi_vmeintr(int slot, int adapnum, int unit, int base)
{
    unsigned vmepend, vmebase, vmestat;
    unsigned vme_ivect;
    unsigned fail = 0;
    register int i, j;
    unsigned timeout;
    unsigned level;

    /* force External vme interrupt */

    if ((vmecc_addr = get_vmebase(slot, adapnum)) == 0)
        return(TEST_SKIPPED);

    clear_vme_intr(vmecc_addr); /* Clear the VME interrupt levels */

    vme_ivect = (VME_INTLVL << 8) |
	    (RD_REG(EV_SPNUM) & EV_SPNUM_MASK);
    
    for (i = base; i < 8; i += 2) {

       WR_REG(vmecc_addr + (VMECC_VECTORIRQ1+(8*(i-1))), vme_ivect);
       WR_REG(vmecc_addr + VMECC_INT_ENABLECLR, 0x1fff);
       /* Enable error and this level of IRQ */
       WR_REG(vmecc_addr + VMECC_INT_ENABLESET, (1 << i)|1 );

       msg_printf(VRB, "Test External vme interrupt level %d\n", i);

       fail = 1;  /* Pessmistic start. */
       for (j = base; j < 256; j += 2) {

    	   if (_dkipiforce(unit, i, j)){
    		msg_printf(ERR, "No dkip disk controller\n");
    		return(1);
    	   }

    	   timeout = 0xffff;
    	   /* check interrupt mask register */
    	   do {
    		ms_delay(5);
    		vmepend = RD_REG(vmecc_addr+VMECC_INT_REQUESTSM);
    	   } while (!(vmepend & (i << i)) && --timeout);

    	   if ( !timeout ) {
	       err_msg(VME_XNOINT, vme_loc, vmepend);
		longjmp(vmebuf, 2);
    	   }

    	   /* check vector register */
	   if ((level = RD_REG(vmecc_addr+VMECC_IACK(i))) != j){
		err_msg(VME_XIACK, vme_loc, j, level);
		longjmp(vmebuf, 2);
	   }

	   /* Interrupt should be in IP0/HPIL too */
	   if (RD_REG(EV_IP0) & (1 << VME_INTLVL)){
	       if ((level = RD_REG(EV_HPIL)) == VME_INTLVL)
		   fail = 0;
	       else 
                   msg_printf(ERR, "Unexpected Interrupt at level %d \n", level);
	   }
	   else{
	       err_msg(VME_XCPUINT, vme_loc, (1 << VME_INTLVL), RD_REG(EV_IP0));
	       longjmp(vmebuf, 2);
	    }
	   

clear:
	   /* Clear All Pending Inerrupts */
	   while((level = RD_REG(EV_HPIL)) != 0){
	       WR_REG(EV_CIPL0, level);
	       ms_delay(10);
	   }

    	   _dkipiclear(unit);

    	   if (fail)
    	       return(1);

       } /* of for (j=....) */
    } /* of for (i=...) */

    return(fail);
}
