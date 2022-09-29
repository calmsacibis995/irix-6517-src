/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/param.h>		/* for DELAY() */
#include <sys/systm.h>		/* for us_delay() */

#include <sys/crime.h>		/* from irix/kern, for CRM_BASEADDR */
#include <sys/crimedef.h>
#include <sys/crimereg.h>
#include <sys/mips_addrspace.h>	/* for PHYS_TO_K1 */

#include "hw_copy.h"

/* Useful Macros
 */

#define CRM_SRCBUF_LINEAR_A	(4<<5)
#define CRM_SRCBUF_LINEAR_B	(5<<5)
#define CRM_DSTBUF_LINEAR_A	(4<<2)
#define CRM_DSTBUF_LINEAR_B	(5<<2)
#define MTE_SRCECC		(1 << 1)
#define MTE_DSTECC		(1)
#define CRMSTAT_MTE_FLUSHED 	(1 << 25)
#define CRM_MTE_MAX_XFER        (CRM_PAGE_SIZE * 32)

/* Make these real once the hardware comes back */
#define MTE_COST_PER_REGWRITE  0x100 /* XXX */
#define MTE_COST_PER_PRIMITIVE (MTE_COST_PER_REGWRITE * 3) /* XXX */

/* Assumes CRM_PAGE_SIZE == 0x1000 */
#define CRM_PAGE_SHIFT 12

#if (_MIPS_SIM != _MIPS_SIM_ABI32)
# define WRITE_REG64(reg, value)  *(volatile _crmreg_t *)(reg) = (value);
#else
  /* from IP32asm.s */
# define WRITE_REG64(reg, value)  write_reg64((value), (reg))
#endif

#define CRM_REG(reg)	((uintptr_t)crime+(reg))
#define CRIME_SET32(reg, value) (*(volatile uint32_t *)CRM_REG(reg) = (value))
#define CRIME_SET32_AND_GO(reg, value) CRIME_SET32((reg)|CRM_START_OFFSET, (value))

#define CRIME_SET64(reg, value) WRITE_REG64(CRM_REG(reg), (value))

#define MTE_REG(reg)	((reg)|CRM_MTE_BASE)
#define MTE_SET32(reg, value)        CRIME_SET32(MTE_REG(reg), (value))
#define MTE_SET32_AND_GO(reg, value) CRIME_SET32_AND_GO(MTE_REG(reg), (value))

#define countof(ARRAY) (sizeof(ARRAY)/sizeof(ARRAY[0]))

/* LOCAL declarations */

static void *crime = (void *) PHYS_TO_K1(CRM_BASEADDR + 0x1000000);

extern int mte_set_tlb(paddr_t addr, int bcount, uint_t tlb, int *nbytes);
extern void mte_fifo_wait(int nentries);
extern void mte_spin(void);


/* 
 * EXPORTS
 */
void
early_mte_zero(paddr_t addr, size_t bcount)
{
	const uint32_t ZERO_OP = ( MTE_CLEAR | CRM_DSTBUF_LINEAR_A 
				   | MTE_DSTECC);

	int tlba = 0;


	/*
	 * MTE can only handle 128K max at one time.  Break
	 * the request up into chunks which are no larger than
	 * 128K
	 */
	while (bcount) {
	    int count;
	    int nbytes;

	    count = (bcount > CRM_MTE_MAX_XFER) ? CRM_MTE_MAX_XFER : bcount;
	    tlba |= mte_set_tlb(addr, count, CRM_TLB_LINEAR_A_OFFSET, &nbytes);
	    count = min(count, nbytes);

	    /* Now do the zero */
	    mte_fifo_wait(5);

	    /*
	     * we use addr + count - 1 because MTE copies and zero
	     * operations are inclusive.  We actually want to clea
	     * count bytes starting at addr.
	     */
	    MTE_SET32(CRM_MTE_DST0_REG, (uintptr_t)addr);
	    MTE_SET32(CRM_MTE_DST1_REG, (uintptr_t)((addr+count) - 1));
	    MTE_SET32(CRM_MTE_FGVALUE_REG, 0x0);
	    MTE_SET32(CRM_MTE_BYTEMASK_REG, 0xffffffff);
	    MTE_SET32_AND_GO(CRM_MTE_MODE_REG, ZERO_OP);

	    bcount -= count;
	    addr += count;

	    /* wait for the xfer to finish */
	    mte_spin();
	}
}
