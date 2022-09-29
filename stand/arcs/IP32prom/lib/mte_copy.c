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

#include <hw_copy.h>

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

#if USE_64BIT_C
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

static int mte_set_tlb(paddr_t addr, int bcount, uint_t tlb, int *nbytes);
static void mte_fifo_wait(int nentries);
static void mte_spin(void);
extern int get_crm_rev(void);


/* 
 * EXPORTS
 */
void
mte_zero(paddr_t addr, size_t bcount)
{
    const uint32_t ZERO_OP = ( MTE_CLEAR | CRM_DSTBUF_LINEAR_A 
			       | MTE_DSTECC);

    int tlba;

    if (get_crm_rev() < 1)
	return;

    /*
     * MTE can only handle 128K max at one time.  Break
     * the request up into chunks which are no larger than
     * 128K
     */
    while (bcount) {
	int count;
	int nbytes;

	count = (bcount > 0x20000) ? 0x20000 : bcount;
	tlba |= mte_set_tlb(addr, count, CRM_TLB_LINEAR_A_OFFSET, &nbytes);
	count = MIN(count, nbytes);

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


/*
 * LOCAL definitions
 */

#define MAX_SPAN (CRM_MTE_MAX_XFER - 1)

int
mte_set_tlb(paddr_t addr, int bcount, uint_t tlb_offset, int *nbytes)
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
    if ((addr_end - (page << CRM_PAGE_SHIFT)) > MAX_SPAN) {
	page_last = ((page<<CRM_PAGE_SHIFT) + MAX_SPAN) >> CRM_PAGE_SHIFT;
    }
    

    /* Make sure there's enough room in the command fifo for our writes */
    mte_fifo_wait((page_last - page)/2);

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

static void
mte_fifo_wait(int nentries)
{
    while (CRIME_FIFO_DEPTH - 
	   CRMSTAT_IB_LEVEL(*(volatile uint_t *)
			    CRM_REG(CRM_STATUS_BASE|CRM_STATUS_REG)) <
	   nentries)
	DELAY(1);
}

