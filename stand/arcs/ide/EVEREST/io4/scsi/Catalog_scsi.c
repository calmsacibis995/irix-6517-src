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
*	File:		Catalog_scsi.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <uif.h>
#include <ide_msg.h>
#include <err_hints.h>
#include <everr_hints.h>
#include <ide_wd95.h>
#include <ide_s1chip.h>

extern void scsi_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t scsi_hints [] = {

{S1_REG_RW, "s1_regtest", "register %s read back %x, expected %x\n"},
{S1_ADAP, "s1_regtest", "adap %x was %x, expected %x (SCSI)\n"},
{WD95_REG_RW, "regs_95a", 
	"wd95a:ctrl %d, %s Reg: Expected 0x%x, Actual: 0x%x\n"},
{WD95_REG_RW2, "regs_95a", 
	"wd95a: ctrl %d, %s%d Reg: Expected 0x%x, Actual: 0x%x\n"},
{WD95_PRESNT1, "regs_95a", 
	"Timeout Select/Reselect Ctrl Reg, Wrote: 0x%x, Got: 0x%x\n"},
{WD95_PRESNT2, "regs_95a", "Resister problems at 0x%x\n"},
{WD95_PRESNT3, "regs_95a", 
	"Reg: SCSI low-level ctrl #24, Wanted: 0, Read: 0x%x\n"},
{WD95_INTR_LEV, "interrupt", "Unexpected interrupt level: Expected 0x%llx\n"},
{WD95_INTR_HP, "interrupt", 
	"EV_HPIL register expected: 0x%x, but read 0x%llx\n"},
{WD95_INTR_NOCAUSE, "interrupt", "Interrupt not seen in cause reg: 0x%x\n"},
{WD95_INTR_IPNZ, "interrupt", 
	"EV_IPx reg not 0, is EVIP0: 0x%llx, EVIP1: 0x%llx\n"},
{WD95_INTR_HPNZ, "interrupt", "EV_HPIL not 0, is 0x%llx\n"},
{WD95_INTR_CANZ, "interrupt", "Cause reg not 0, is 0x%x\n"},
{WD95_INTR_NOINT, "interrupt", "No CPU interrupt seen\n"},
{WD95_INTR_NOEBUS, "interrupt", "No ebus interrupt seen, HPIL: 0x%llx\n"},
{WD95_INTR_ENOCPU, "interrupt", 
	"Ebus interrupt seen, no CPU interrupt. HPIL: 0x%llx\n"},
{WD95_TALK, "wd95a problem", 
	"Problem talking to wd95a controller-%d\n"},
{WD95_SC_TIMEOUT, "scsi_self", "Timeout on scsi bus\n"},
{WD95_SC_HARDERR, "scsi_self", "Hardware or scsi device error\n"},
{WD95_SC_PARITY, "scsi_self", 
	"Parity error on the SCSI bus during transfer\n"},
{WD95_SC_MEMERR, "scsi_self", "Parity/ECC error on host memory\n"},
{WD95_SC_CMDTIME, "scsi_self", "Command timed out\n"},
{WD95_SC_ALIGN, "scsi_self", "The i/o address wasn't properly aligned\n"},
{WD95_SC_ATTN, "scsi_self", "unit attention received on other command\n"},
{WD95_SC_REQ, "scsi_self", 
	"Bad request, malformed/non-existent device, no alloc done\n"},
{WD95_SC_UNKNOWN, "scsi_self", "Unknown error code: %d\n"},
{WD95_DMA_SK, "seek", "Seek to block #%d fails\n"},
{WD95_DMA_RD, "read", "Read error at block %s\n"},
{WD95_DMA_RDCNT, "read", "Tried to read %d blocks, only read %d at %s\n"},
{WD95_DMA_WR, "write", "Write error at block %s\n"},
{WD95_DMA_WRCNT, "write", "Tried to write %d blocks, only write %d at %s\n"},
{WD95_DMA_WRDATA, "verify", "Data read back differs from what was written\n"},
{WD95_INTR_NOCACHE, "interrupt", 
   "CacheErr register expected (upper-most byte): 0x%x, got: 0x%x\n"},
{WD95_INTR_IAEBUSREG, "interrupt","IA EBus Err Reg expected:0x%x, got:0x%x\n"},
{WD95_INTR_IOAORIG, "interrupt", 
	"Error originator is IO4 adapter %d, expected adapter %d\n"},
{WD95_INTR_IOADDR, "interrupt", "Bad address: 0x%x, expected: 0x%x\n"},
{(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0}
};


uint	scsi_hintnum[] = {IO4_SCIP, IO4_SCSI, 0 };

catfunc_t scsi_catfunc = { scsi_hintnum, scsi_printf  };
catalog_t scsi_catalog = { &scsi_catfunc, scsi_hints };


void
scsi_printf(void *ptr)
{
	int *l = (int *)ptr;
	msg_printf(ERR,"(IO4 slot: %d, S1 adap: %d ", *l, *(l+1));
	if (*(l+1) == S1_ON_MOTHERBOARD)
                 msg_printf(ERR,"(motherboard))\n");
        else
                 msg_printf(ERR,"(mez card))\n");
}
