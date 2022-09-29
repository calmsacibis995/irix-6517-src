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

/***********************************************************************\
*	File:		Catalog_vme.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <everr_hints.h>
#include <ide_msg.h>

#ident	"arcs/ide/EVEREST/io4/vme/Catalog_vme.c $Revision: 1.7 $"

extern void vme_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t vme_hints [] = {
/* FCHIP related hints */
{F_GENRIC,  "fregs",  "Exception while testing Fchip adap %d on IO4 %d\n"}, 
{F_BADVER,  "fregs",  "Bad F chip Version no. Expected: %x or %x. Got: %x\n"},
{F_INVMID,  "fregs",  "F chip reports Bad Master ID:%x. Expecting %x / %x\n"},
{F_BADREG,  "fregs",  "Failed testing F register %x. Expected %x Got %x\n"},
{F_LWREG,   "fregs",  "Failed LW access of F reg: %x. Expected: %x Got %x\n"},
{F_INTSET,  "fregs",  "Failed to set F Intr mask. Expected: %x Got: %x\n"},
{F_INTRST,  "fregs",  "Failed to clear F Intr mask. Expected: %x Got: %x\n"},
{F_ERRCLR,  "fregs",  "F error reg clearing FAILED. Expected: %x Got: %x\n"},
{F_BADRADR, "fregs",  "Failed reg address test.Reg:%x Expected: %x Got: %x\n"},
{F_SWERR,   "fregs",  "SOFTWARE detected error while testing Fchip\n"},
{F_BADGVER, "fregs",  "F chip Rev %x will fail with %x (%s) graphics adap\n"},

{VMEINT_TO,  "vmeintr", "Timedout waiting for VMECC interrupts\n"},
{VMEINT_ERR, "vmeintr", "Expected interrupt level:0x%x, Got on 0x%x\n"},
{VME_SWERR,  "vmetest", "SOFTWARE detected error while testing VMECC\n"},

{VME_REGR,   "vmeregs", "Got an exception during VMECC testing\n"}, 
{VME_BADREG, "vmeregs", "Failed testing VMECC reg %x. Expected: %x Got: %x\n"},
{VME_BADRADR,"vmeregs", "Failed reg addr test. Reg: %x Expected: %x Got: %x"},
{VME_BADCLR, "vmeregs", "Bad value from error clear reg. Expected: %x Got: %x"},
{VME_BADCAU, "vmeregs", "Bad cause reg after clearing. Expected: %x Got: %x"},
{VME_INTSET, "vmeregs", "Failed to set Intr mask on level %d"}, 
{VME_INTCLR, "vmeregs", "Failed to clear Intr mask on level %d."}, 
{VME_BUSERR, "vmebuserr", "Exception while testing VME bus error  \n"}, 
{VME_BUSUNE, "vmebuserr", "Bad intr in VME %s bus error test. Expected: %x Got: %x\n"},
{VME_DMAERR, "vmedmaeng", "Exception while testing VMECC DMA engine\n"},
{VME_DMARD,  "vmedmaeng", "FAILED DMA engine EBUS->VME xfer. Status:0x%x\n"},
{VME_DMAWR,  "vmedmaeng", "FAILED DMA engine VME->EBUS xfer. Status:0x%x\n"},
{VME_DMAXFER,"vmedmaeng", "DMA engine transfer: At %x Expected: %x Got:%x\n"},
{VME_ENGERR, "vmedmaeng", "Error in DMA Engine during %s\n"},

{VME_A32L1, "vmelpbk", "FAILED VME A32 PIO loopback - 1 Level mem mapping\n"},
{VME_A32L2, "vmelpbk", "FAILED VME A32 PIO loopback - 2 level mem mapping\n"},
{VME_A24L1, "vmelpbk", "FAILED VME A24 PIO loopback - 1 Level mem mapping\n"},
{VME_A24L2, "vmelpbk", "FAILED VME A24 PIO loopback - 2 level mem mapping\n"},
{VME_LPBKB, "vmelpbk", "PIO loopback Byte mode: At %x Wrote 0x%x Read 0x%x\n"},
{VME_LPBKH, "vmelpbk", "PIO loopback Half mode: At %x Wrote 0x%x Read 0x%x\n"},
{VME_LPBKW, "vmelpbk", "PIO loopback Word mode: At %x Wrote 0x%x Read 0x%x\n"},
{VME_LPWRB, "vmelpbk", "PIO loopback: byte write caused intr at 0x%x \n"},
{VME_LPWRH, "vmelpbk", "PIO loopback: halfword write caused intr at 0x%x \n"},
{VME_LPWRW, "vmelpbk", "PIO loopback: word write cause intr at 0x%x \n"},
{VME_LPCOB, "vmelpbk", "PIO loopback: Byte read from coherent space. At 0x%x Expected: 0x%x Got: 0x%x\n"},
{VME_LPCOH, "vmelpbk", "PIO loopback: Halfword read from coherent space. At 0x%x Expected: 0x%x Got: 0x%x\n"},
{VME_LPCOW, "vmelpbk", "PIO loopback: Word read from coherent space. At 0x%x Expected: 0x%x Got: 0x%x\n"},
{VME_XINTR,  "vme_xintr", "Exception while testing external VME interrupt \n"},
{VME_XNOINT, "vme_xintr", "FAILED to receive external VME intr. Pending: %x\n"},
{VME_XIACK,  "vme_xintr", "Recvd bad IACK from external board. Expected: %x Got %x\n"},
{VME_XCPUINT,"vme_xintr", "CPU failed to receive external intr on lvl %x. Intr mask:0x%x\n"},


{CDSIO_GEN,  "cdsio_data", "Exception while running CDSIO loopback test\n"},
{CDSIO_RDY,  "cdsio_data", "CDSIO board not ready to accept cmnd. Status:%x\n"},
{CDSIO_ERR,  "cdsio_data", "Failed to complete block %d data transfer.\n"},
{CDSIO_ERR1, "cdsio_data", "Failed to rcv all chars in block %d. Expected: %x Got: %x\n"},
{CDSIO_DERR, "cdsio_data", "Data mismatch. At 0x%x Expected: %2x Got %2x\n"},

{CDSIO_IGEN, "cdsio_intr", "Exception while running CDSIO intr test\n"},
{CDSIO_BINT, "cdsio_intr", "Bad intr while CDSIO intr testing. Expected: 0x%x Got: 0x%x\n"},
{CDSIO_VECT, "cdsio_intr", "Received bad IACK vector. Expected: %x Got: %x\n"},
{CDSIO_SINT, "cdsio_intr", "Received Stray intr. Level0: 0x%x, Level1:0x%x\n"},
(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0
};



uint	vme_hintnum[] = {IO4_VMECC, IO4_FCHIP, 0 };
catfunc_t vme_catfunc = { vme_hintnum, vme_printf  };
catalog_t vme_catalog = { &vme_catfunc, vme_hints };

void
vme_printf(void *loc)
{
    int *l = (int *)loc;
    msg_printf(ERR,"(IO4 slot:%d VMECC adap:%d ", *l, *(l+1));
    if (*(l+2) != (-1))
	msg_printf(ERR,"Port:%d", *(l+2));

    msg_printf(ERR," )\n");
    
}

