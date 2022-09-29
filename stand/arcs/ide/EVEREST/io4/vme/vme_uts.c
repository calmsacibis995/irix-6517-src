/*
 *
 * Copyright 1992, Silicon Graphics, Inc.
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
#include <prototypes.h>

#ident "arcs/ide/IP19/io/io4/vme/vme_uts.c $Revision: 1.12 $"

extern int io4_window(int);
extern int get_pagemask();
extern int set_pagemask(unsigned);
/*
 * Given the IO4 slot and the adapter No, get the base address usable
 * with the VME Register addressing
 */
#if _MIPS_SIM != _ABI64
unsigned
get_vmebase(int slot, int anum)
{
    unsigned	id;
#else
paddr_t
get_vmebase(int slot, int anum)
{
    paddr_t id;
#endif
    if ((id = fmaster(slot, anum)) != IO4_ADAP_VMECC){
	msg_printf(ERR, "Adapter id 0x%x at <slot %d adap %d> is not VMECC \n", 
	    id, slot, anum);
	return(0);
    }

    id = SWIN_BASE(io4_window(slot), anum);
    if (RD_REG(id+VMECC_ERRCAUSECLR) & 0x80){
	msg_printf(ERR,"VMECC in Slot %d adapter %d is in PIO Drop mode..RESET SYSTEM\n", 
		slot, anum);
	return(0);
    }

    return(id);

}

#if _MIPS_SIM != _ABI64
void
clear_vme_intr(unsigned vmebase)
#else
void
clear_vme_intr(paddr_t vmebase)
#endif
{
    int level;

    /* Clear all the interrupts */
    WR_REG(vmebase+VMECC_INT_ENABLECLR, 0x1fff); /* clear all 12 bits */

    /* Zeroout all the IACK levels */
    for (level = 1; level <= 7; level++)
	WR_REG(vmebase+VMECC_IACK(level), 0);

}

#if _MIPS_SIM != _ABI64
int
init_fchip(unsigned base_addr)
#else
int
init_fchip(paddr_t base_addr)
#endif
{
    unsigned tmp;

    if ((tmp = RD_REG(base_addr)) == IO4_ADAP_FCHIP){
	msg_printf(ERR,"Invalid F Version Number %x Or F Chip doesnot exist \n", tmp);
	return(1);
    }

    if ((tmp = RD_REG(base_addr+FCHIP_MASTER_ID)) != IO4_ADAP_VMECC){
	msg_printf(ERR,"F Chip master ID 0x%x is not that of VMECC \n", tmp);
	return(1);
    }

    RD_REG(base_addr+FCHIP_ERROR_CLEAR); /* Clear its error register */
    WR_REG(base_addr+FCHIP_INTR_RESET_MASK, 0x1); /* Reset Intr Mask */
    WR_REG(base_addr+FCHIP_FCI_ERROR_CMND, 0);
    WR_REG(base_addr+FCHIP_INTR_MAP, 0);
    WR_REG(base_addr+FCHIP_TLB_FLUSH, 0x1fffff); /* Flush all entries */



    return(0);
}

#if _MIPS_SIM != _ABI64
int
init_vmecc(unsigned base_addr)
#else
int
init_vmecc(paddr_t base_addr)
#endif
{

    unsigned tmp;

    if ((tmp = RD_REG(base_addr+FCHIP_MASTER_ID)) != IO4_ADAP_VMECC){
	msg_printf(ERR,"F Chip master ID 0x%x is not that of VMECC \n", tmp);
	return(1);
    }

    WR_REG(base_addr+VMECC_CONFIG, VMECC_CONFIG_VALUE);
    WR_REG(base_addr+VMECC_INT_ENABLECLR, 0x1fff);
    RD_REG(base_addr+VMECC_ERRCAUSECLR);
#ifdef NEVER_TFP
    WR_REG(base_addr+VMECC_A64SLVMATCH, (K1_TO_PHYS(base_addr) >> 32));
    WR_REG(base_addr+VMECC_A64MASTER, (K1_TO_PHYS(base_addr) >> 32));
#endif

}
