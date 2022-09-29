/******************************************************************************
 *                                                                            *
 * Copyright 1991, Silicon Graphics, Inc.                                     *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;     *
 * the contents of this file may not be disclosed to third parties, copied or *
 * duplicated in any form, in whole or in part, without the prior written     *
 * permission of Silicon Graphics, Inc.                                       *
 *                                                                            *
 * RESTRICTED RIGHTS LEGEND:                                                  *
 * Use, duplication or disclosure by the Government is subject to restrictions*
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data     *
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or   *
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -    *
 * rights reserved under the Copyright Laws of the United States.             *
 *****************************************************************************/

/*
 * vme_buserr -- Do a PIO to the VME A24/A32 address space not responded to by 
 *		 VMECC and watch out for the VME buserr to be flagged by 
 *		 VMECC
 */

#include "vmedefs.h"
#include <fault.h>
#include <prototypes.h>
#include <libsk.h>

#ident "arcs/ide/IP19/io/io4/vme/vme_buserr.c $Revision: 1.16 $"

extern char *atob();
extern int  dovme_buserr(int, int, int);

/*
 * Description:
 *     	This test ensures that VMECC can timeout for all sections of A24/A32
 *     	addresses. 
 *
 *     	First, A24 addressing is used. All sections except one are made to 
 *	respond as the slaves, and a PIO access to the Non-responding slave
 *	section is done. This should generate a timeout.
 *
 *	Similarly repeat this procedure for all the 16 sections of the A32
 *	addressing.
 */

#define	ALL_SLAVES	0xfffff000

#define	A24OP		0

#define	USAGE_BERR	"%s [io4slot anum]\n"
#define	USAGE_A24	"vme_a24buserr io4slot, vmecc_anum\n"

#define	VMECC_ERRINTR	0x7a
static  int vmeloc[3];
extern int io4_tadap, io4_tslot;
static jmp_buf	localbuf;

vme_buserr(int argc, char **argv)
{
    uint 	slot, anum;
    uint	found=0, fail=0;

    if (argc > 1){
        if (io4_select(1, argc, argv)){
            msg_printf(ERR,USAGE_BERR, argv[0]);
            return(1);
        }
    }
    while (!fail && io4_search(IO4_ADAP_FCHIP, &slot, &anum)){
        if ((argc > 1) && ((slot != io4_tslot) || (anum != io4_tadap)))
            continue;

	if (fmaster(slot, anum) != IO4_ADAP_VMECC)
	    continue;

	found = 1;

        fail |= dovme_buserr(slot, anum, 0); /* A24 bus error */
        fail |= dovme_buserr(slot, anum, 1); /* A32 bus error */
    }
    if (!found) {
	if (argc > 1) {
	    msg_printf(ERR, "Invalid Slot/Adapter number\n");
	    msg_printf(ERR, USAGE_BERR, argv[0]);
	}
	fail = TEST_SKIPPED;
    }
    if (fail)
        io4_search(0, &slot, &anum);    /* reset variables for future use */

    return (fail);

}
/*
 * Description:
 *	This is the common routine for both A24 and A32 address bus error test.
 *	It generates the error in the following fashion
 *	For A24.
 *	There are 4 sections in A24 slave addr space. 
 *	In each iteration 3 address space is enabled and one is disabled.
 *	PIOREG(1) is programmed such that an access to its address space
 *	translates to VME access in disabled address space.
 *	This would generate a bus error since nobody is there to respond to 
 *	this address space.
 *
 *	Similarly it is done for A32 address space.
 */
char a24_space[]="A24";
char a32_space[]="A32";
#define	ADDR_SPACE(x)	(( x == A24OP) ? a24_space : a32_space )

dovme_buserr(int io4slot, int anum, int mode)
/* mode: 0=> A24, 1=> A32 */
{

#if _MIPS_SIM != _ABI64
    volatile 	caddr_t  vmeptr;
    unsigned	int	_cause;
    unsigned	int	_exc;
    unsigned 	chip_base, oldsr, fail=0, i;
    unsigned	tmp;
    char	*addr_space;
#else
    volatile 	paddr_t  vmeptr;
    unsigned	int	_cause;
    unsigned	int	_exc;
    __psunsigned_t chip_base, oldsr;
    unsigned	fail=0, i;
    unsigned	tmp;
    char 	*addr_space;
#endif
/*
 * XXXXXXX
 * This is a hack to get this test to run. Problem is in fact in ARCS fault 
 * handling data structure and faultasm.s. They define _regs to be a 
 * pointer to data structure of type k_machreg_t * (8 byte wide elements), 
 * but faultasm.s fills'em as 4 byte entries. 
 * This hack should go ASAP.
 */
#if (R_SIZE == 4)
    unsigned 	*lregs = (unsigned *)(_regs);
#elif (R_SIZE == 8)
    k_machreg_t *lregs = _regs;
#else
    R_SIZE undefined in arcs/include/fault.h ?? >>>>
#endif

    ide_init_tlb();
    io_err_clear(io4slot, (1<< anum));
#ifdef TFP
    init_interrupts();
#endif
    setup_err_intr(io4slot, anum);

    oldsr = GetSR();
    set_SR(oldsr & ~SR_IMASK);

    vmeloc[0] = io4slot; vmeloc[1] = anum; vmeloc[2] = -1;

    addr_space = ADDR_SPACE(mode);

    if (tmp = setjmp(vmebuf)){
	switch(tmp){
	default:
	case 1: err_msg(VME_BUSERR, vmeloc); break;
	case 2: err_msg(VME_SWERR, vmeloc); break;
	}
	show_fault();
	io_err_log(io4slot, (1<< anum));
	io_err_show(io4slot, (1<< anum));
	clear_err_intr(io4slot, anum);
	fail = 1;
    }

    else{

	set_nofault(vmebuf);

	i = (VMECC_ERRINTR << 8) |
	    (RD_REG(EV_SPNUM) & EV_SPNUM_MASK);

	if ((chip_base = get_vmebase(io4slot, anum)) == 0)
	    return(TEST_SKIPPED);

#if _MIPS_SIM != _ABI64
	vmeptr     = tlbentry(WINDOW(chip_base), anum, 0);
#else
	vmeptr     = LWIN_PHYS(WINDOW(chip_base), anum);
#endif

	WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE);
	WR_REG(chip_base+VMECC_VECTORERROR, i);


    	msg_printf(VRB, "VME %s Address space bus error test for vme adapter %d, io4 slot %d\n",
		addr_space, anum, io4slot);	

	for( i = 0; i < ((mode==A24OP)? 4 : 16); i++){

	    /* clear IO4 error regs */
	    io_err_clear(io4slot, (1<< anum));

	    /* Write to config. Enables all slaves except i'th */
	    if (mode == A24OP)
	        tmp = (VMECC_CONFIG_VALUE & 0xfff) |(ALL_SLAVES & ~(1<<(i+12)));
	    else
	        tmp = (VMECC_CONFIG_VALUE & 0xfff) |(ALL_SLAVES & ~(1<<(i+16)));

	    WR_REG(chip_base+VMECC_CONFIG, tmp);

	    /* Setup PIOREG(1) so as to translate the address to ith address 
	     * space for the required mode
	     */
	    if (mode == A24OP)
		/* Bit A23 of the A24 address space corresponds to bit 0 */
	        WR_REG(chip_base+VMECC_PIOREG(1), ((A24_NPAM << 10)|(i >> 1)));
	    else 
		/* Bits 8-5 correspond to bits 31-28 of A32 address space */
	        WR_REG(chip_base+VMECC_PIOREG(1), ((A32_NPAM << 10)|(i << 5)));

	    /* Disable all interrupts except VMECC error intr */
	    WR_REG(chip_base+VMECC_INT_ENABLECLR, 0x1fff);
	    WR_REG(chip_base+VMECC_INT_ENABLESET, 0x1); 
#ifdef NEVER
#if _MIPS_SIM == _ABI64
	    EV_SET_REG(EV_CERTOIP, 0xffff);		/* Clear Bus Error */
	    EV_SET_LOCAL(EV_ILE,
		 (EV_EBUSINT_MASK|EV_ERTOINT_MASK));	/* re-enable BE */
#endif
#endif
	    if (setjmp(localbuf))
	    {
		/* Check if this exception is really the one you wanted !! */
		_cause   = lregs[R_CAUSE];
		_exc     = lregs[R_EXCTYPE];
#ifdef IP19
		/* We expect a Normal exception with cause DBE */
		if ((_exc == EXCEPT_NORM) &&
		    ((_cause & CAUSE_EXCMASK) == EXC_DBE)){
		    msg_printf(DBG,"VME %s region %d bus error check OK\n",
				addr_space, i);
		    continue; /* Happy go ahead */
		}
#elif defined(TFP)	/* TFP case */
		/* We expect an Interrupt with BE set  */
		if ((_exc == EXCEPT_NORM) &&
		    ((_cause & CAUSE_EXCMASK) == EXC_INT) &&
		    (_cause & CAUSE_BE)){
		    msg_printf(DBG,"VME %s region %d bus error check OK\n",
				addr_space, i);
		    continue; /* Happy go ahead */
		}
#elif defined(IP25)
		/* We expect an Interrupt with SRB_ERR set  */
		if ((_exc == EXCEPT_NORM) &&
		    ((_cause & CAUSE_EXCMASK) == EXC_INT) &&
		    (_cause & SRB_ERR)){
		    msg_printf(DBG,"VME %s region %d bus error check OK\n",
				addr_space, i);
		    continue; /* Happy go ahead */
		}
#endif
		else{
		    set_nofault(vmebuf);
		    longjmp(nofault, 2);
		}
	    }
	    else{

		/* clear the old error codes out */
		RD_REG(chip_base+VMECC_ERRCAUSECLR);
		WR_REG(chip_base+VMECC_ERRXTRAVME, 0);

		setup_err_intr(io4slot, anum);
		set_nofault(localbuf);

#ifndef IP25
		/* Enable interrupts on the R4k or R8k processor */
		set_SR(SR_DEFAULT | SRB_DEV | SR_IE);
#else
		set_SR(NORMAL_SR | SRB_DEV | SR_IE);
#endif

		/* This should generate the addr fault */
		/* wr+rd may be overkill, but keeps optimizer honest */
		if ( mode == A24OP ) {
		    /* Pick up bit 22 from bit 0 of 'i' */
		    *(char *)(vmeptr+0x800000+ ((i & 3) << 22)) = 0;
		    tmp = *(char *)(vmeptr+0x800000+ ((i & 3) << 22));
		}
		else {
		    *(char *)(vmeptr+0x800000) = 0;
		    tmp = *(char *)(vmeptr+0x800000);
		}

		ms_delay(1000);

		/* If we do not get any interrupt we come here */
		/* Check if error cause register is set */
		msg_printf(ERR,
    "FAILED VME bus error check for %s region %d - Received No Exception\n",
			addr_space, i);
		msg_printf(DBG,"conf:0x%x Addr: 0x%x Xtra: 0x%x ERR: 0x%x\n",
			(uint)RD_REG(chip_base+VMECC_CONFIG), 
			(uint)RD_REG(chip_base+VMECC_ERRADDRVME),
			(uint)RD_REG(chip_base+VMECC_ERRXTRAVME),
			(uint)RD_REG(chip_base+VMECC_ERRORCAUSES));

		/* read and clear status this time */
		if ((uint)RD_REG(chip_base+VMECC_ERRCAUSECLR) & 0x2){
		    msg_printf(ERR,"VMECC got bus error but no interrupt!!\n");
		    fail = 1;
		}
		else msg_printf(ERR,"Possibly There is some VME controller \n");
	    }
	}

    }

#if _MIPS_SIM != _ABI64
    tlbrmentry(vmeptr);
#endif
    set_SR(oldsr);
    io_err_clear(io4slot, (1<< anum));
    clear_err_intr(io4slot, anum);
    ide_init_tlb();
    clear_nofault();

    WR_REG(chip_base+VMECC_CONFIG, VMECC_CONFIG_VALUE);

    return(fail);
}

