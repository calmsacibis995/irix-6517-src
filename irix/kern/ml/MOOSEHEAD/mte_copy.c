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
#include <sys/mips_addrspace.h>	/* for PHYS_TO_K1 */
#include <sys/crimedef.h>
#include <sys/crimereg.h>

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

/* IMPORTS */

/* defined by crimestubs.c and the gfx ism */
extern int crmSavePP (void *, uintptr_t *, uint64_t *);
extern void crimeRestore(void *, uint_t, uint_t, const u_int *, int,
			 uintptr_t *, uint64_t *, int);
extern int crimeLockOut(void *);


/* LOCAL declarations */

static void *crime = (void *) PHYS_TO_K1(CRM_BASEADDR + 0x1000000);

int mte_set_tlb(paddr_t addr, size_t bcount, uint_t tlb, int *nbytes);
void mte_fifo_wait(int nentries);
void mte_spin(void);


/* 
 * EXPORTS
 */

void
mte_copy(paddr_t from, paddr_t to, size_t bcount)
{
	const uint32_t COPY_OP = (MTE_COPY |
		CRM_SRCBUF_LINEAR_A | CRM_DSTBUF_LINEAR_B |
		MTE_SRCECC | MTE_DSTECC);
			
	const uint_t MY_REGS[] = { CRM_MTE_MODE_REG|CRM_MTE_BASE,
		       		   CRM_MTE_SRC0_REG|CRM_MTE_BASE,
				   CRM_MTE_SRC1_REG|CRM_MTE_BASE,
				   CRM_MTE_DST0_REG|CRM_MTE_BASE,
				   CRM_MTE_DST1_REG|CRM_MTE_BASE
			   };

	int tlba = 0, tlbb = 0;
	uint64_t ppdata[CRIME_FIFO_DEPTH];
	uintptr_t ppaddr[CRIME_FIFO_DEPTH];
	int ppsize;

	ppsize = crmSavePP(crime, ppaddr, ppdata);

	while (bcount) {
	    int count;
	    int nbytes;

	    tlba |= mte_set_tlb(from,bcount,CRM_TLB_LINEAR_A_OFFSET,&nbytes);
	    count = min(nbytes, bcount);

	    tlbb |= mte_set_tlb(to, count, CRM_TLB_LINEAR_B_OFFSET, &nbytes);
	    count = min(nbytes, count);

	    /* Now do the copy */
	    mte_fifo_wait(6);
	    MTE_SET32(CRM_MTE_SRC0_REG, (uintptr_t)from);
	    MTE_SET32(CRM_MTE_SRC1_REG, (uintptr_t)((from+count) - 1));
	    MTE_SET32(CRM_MTE_DST0_REG, (uintptr_t)to);
	    MTE_SET32(CRM_MTE_DST1_REG, (uintptr_t)((to+count) - 1));
	    MTE_SET32(CRM_MTE_BYTEMASK_REG, 0xffffffff);
	    MTE_SET32_AND_GO(CRM_MTE_MODE_REG, COPY_OP);

	    bcount -= count;
	    to += count; 
	    from += count;
	}
	
	/* N.B. it's important to restore the shadow registers before 
	 * replaying the partial primitive.  Since all gfx/X writes are
	 * done "hw=val; shadow=val".  So if we snuck in between those two,
	 * the hw has the correct value, so that's what we want to restore
	 * last.
	 */

	/* gfx function to restore the current MTE context from 
	 * the active shadow register file.
	 */
	crimeRestore(crime, tlba, tlbb, MY_REGS, countof(MY_REGS), 
		     ppaddr, ppdata, ppsize);
}


void
mte_zero(paddr_t addr, size_t bcount)
{
	/* N.B. srcECC is ignored for CLEAR_OP.  dstECC is not currently
	 * set here.  If a client needs it, we could make that an option.
	 */
	const uint32_t ZERO_OP = ( MTE_CLEAR | CRM_DSTBUF_LINEAR_A 
				   | MTE_DSTECC );
	const uint_t MY_REGS[] = { CRM_MTE_MODE_REG|CRM_MTE_BASE,
				   CRM_MTE_DST0_REG|CRM_MTE_BASE,
				   CRM_MTE_DST1_REG|CRM_MTE_BASE,
			   };

	int tlba = 0;

	uint64_t ppdata[CRIME_FIFO_DEPTH];
	uintptr_t ppaddr[CRIME_FIFO_DEPTH];
	int ppsize;

	ppsize = crmSavePP(crime, ppaddr, ppdata);

	while (bcount) {
	    int count = bcount;
	    int nbytes;
	    
	    tlba |= mte_set_tlb(addr,bcount,CRM_TLB_LINEAR_A_OFFSET,&nbytes);
	    count = min(nbytes, count);

	    /* Now do the zero */
	    mte_fifo_wait(5);

	    /*
	     * we use addr + count - 1 because MTE copies and zero
	     * operations are inclusive.  We actually want to clear
	     * count bytes starting at addr.
	     */
	    MTE_SET32(CRM_MTE_DST0_REG, (uintptr_t)addr);
	    MTE_SET32(CRM_MTE_DST1_REG, (uintptr_t)((addr+count) - 1));
	    MTE_SET32(CRM_MTE_FGVALUE_REG, 0x0);
	    MTE_SET32(CRM_MTE_BYTEMASK_REG, 0xffffffff);
	    MTE_SET32_AND_GO(CRM_MTE_MODE_REG, ZERO_OP);

	    bcount -= count;
	    addr += count;
	}

	/* gfx function to restore the current MTE context from 
	 * the active shadow register file.
	 */
	crimeRestore(crime, tlba, 0, MY_REGS, countof(MY_REGS), 
		     ppaddr, ppdata, ppsize);
}


/* Estimated number of usecs before an MTE operation will be acted on.
 *
 * The returned value may have an arbitrary amount of error.  (I.e., we
 * are free to sacrifice accuracy for ease of computation.)
 */
int
mte_delay()
{
	uint_t status;

	if (crimeLockOut(crime))
		return(HW_COPY_NOT_AVAILABLE);

	status = *(volatile uint_t *)CRM_REG(CRM_STATUS_BASE|CRM_STATUS_REG);

	return(status & CRMSTAT_MTE_FLUSHED ? 0 :
	       (MTE_COST_PER_PRIMITIVE + 
		CRMSTAT_IB_LEVEL(status) * MTE_COST_PER_REGWRITE));
}


/* Spin until MTE is idle */
void
mte_spin()
{

	while (!*(volatile uint_t *)
	       CRM_REG(CRM_STATUS_BASE|CRM_STATUS_REG) & CRMSTAT_MTE_FLUSHED)
		DELAY(1);
}



/* defined here, referenced in hw_copy.c */
hw_copy_engine mte_engine = {
	NULL,
	mte_copy, mte_zero,
	mte_delay, mte_spin,
	0, 
	/* on copy_op, bottom 5 bits in src and dst have to agree */
	0x20 - 1,
	CRM_MTE_MAX_XFER,
};


/*
 * LOCAL definitions
 */

#define MAX_SPAN (CRM_MTE_MAX_XFER - 1)

int
mte_set_tlb(paddr_t addr, size_t bcount, uint_t tlb_offset, int *nbytes)
{
	u_long page;
	u_long page_last;
	paddr_t addr_end = ((u_long) addr + bcount - 1);
	int indices = 0;
	int index;

	page = (u_long)addr >> CRM_PAGE_SHIFT;
	page_last  = (u_long)addr_end >> CRM_PAGE_SHIFT;
	*nbytes = 0;

	/*
	 * check to see if we start on an odd page.  
	 * Since TLB entries are paired, the first page
	 * mapped must be an even page.
	 */
	if (page & 0x1)	{ 
		--page;
		addr = (page << CRM_PAGE_SHIFT);
		*nbytes -= CRM_PAGE_SIZE;
	}

	/*
	 * now that we have the starting page calculated, check for
	 * overflow.  Adjust ending page if we would overflow the
	 * the mte page table.
	 */
	if ((addr_end - addr) > MAX_SPAN) {
	    page_last = ((page<<CRM_PAGE_SHIFT) + MAX_SPAN) >> CRM_PAGE_SHIFT;
	}
    

	/* Make sure there's enough room in the command fifo for our writes */
	mte_fifo_wait((page_last - page)/2 + 1);

	for (; page <= page_last; page += 2) {
		/* just map vaddr to paddr */
		index = (page & 0x1f) >> 1;

		CRIME_SET64(CRM_TLB_BASE+tlb_offset+(index*sizeof(_crmreg_t)),
			    ((_crmreg_t)(1<<31 | page) << 32) | 
			    (_crmreg_t)(1<<31 | page+1));
		indices |= 1<<index;

		*nbytes += (CRM_PAGE_SIZE * 2);
	}

	return(indices);
}


/* We don't want to take a Crime-FIFO-hiwater-mark interrupt
 * for a variety of reasons (it happens before the fifo actually fills;
 * it's expensive; and we may have the interrupt masked, anyway, etc.)
 * so we just make sure there's enough room in the fifo before we write
 * something.  Poll, waiting for it to drain, if there isn't.
 * 
 * This assumes that no one can come in at interrupt and write it the fifo, 
 * which is true because hw_copy.c only lets one caller in here at a time.
 */

void
mte_fifo_wait(int nentries)
{
	while (CRIME_FIFO_DEPTH - 
	       CRMSTAT_IB_LEVEL(*(volatile uint_t *)
				CRM_REG(CRM_STATUS_BASE|CRM_STATUS_REG)) <
	       nentries)
		DELAY(1);
}
