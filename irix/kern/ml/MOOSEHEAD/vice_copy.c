/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sgidefs.h>		/* for __psint_t */
#include <sys/types.h>		/* for size_t */
#include <sys/systm.h>		/* for bcopy, bzero */
#include <sys/immu.h>		/* PHYS_TO_K0 */
#include <sys/invent.h>

#include <sys/debug.h>
#include <sys/IP32.h>		/* for VICE_CPU_INTR */
#include <sys/cmn_err.h>

#include "hw_copy.h"

/* From isms/dmedia/devices/vice/kern/sys */
#include <vice/vice_drv.h>
#include <vice/vice_dma.h>
/* Generated file from isms/bonsai/dmedia/devices/vice/include */
#include <vice/vice_regs_host.h>

#ifdef DEBUG
# undef  STATIC
# define STATIC 
#else
#ifndef STATIC
# define STATIC static
#endif /* STATIC */
#endif /* DEBUG */


/* Why isn't this in a header file somewhere?! */
#define vice_base (caddr_t)0xb7000000

/* Not defined in vice_regs_host.h, but should be */
#define VICEDMA_DCTL_RW_READ	VICEDMA_DCTL_RW
#define VICEDMA_DCTL_RW_WRITE	0x0000
#define VICE_CFG_TLB_BYP	0x0002

/* From isms/dmedia/devices/vice/kern/vice/vice_chip.c */
#define ADDR32(a)       ((volatile unsigned int *)(vice_base + (a) + 4))
#define STORE32(a,v)    (*(unsigned int *)ADDR32(a) = (v))
#define LOAD32(a)       (*(unsigned int *)ADDR32(a))

#define ADDR16(a)       ((volatile unsigned int *)(vice_base + (a) + 6))
#define STORE16(a,v)    (*(unsigned short *)ADDR16(a) = (short)(v))
#define LOAD16(a)       (*(unsigned short *)ADDR16(a))

STATIC void *myatom = 0;
STATIC enum { VC_IDLE, VC_INIT, VC_ACTIVE } vc_state = VC_IDLE;


#ifdef DEBUG
int vice_copy_race = 0;
#endif

static int vice_never_lockable(void);
STATIC void vice_copy_intr(void *ignore);
STATIC void vice_spin(void);
STATIC void vice_steal_intr(void);

STATIC void *(*set)(void *) = NULL;
STATIC void (*clear)(void) = NULL;
STATIC int (*lockable)(void) = NULL;

void
register_vice_callbacks(void *(*alloc)(int, void (*)(void *)), 
			void *(*s)(void *), void (*c)(void), int (*l)(void))
{
	extern int hw_copy_disable;
#ifdef DEBUG
	if (hw_copy_disable)
		return;
#endif

	ASSERT(alloc || (!s && !c && !l)); /* all or none */

	if (alloc) {
		/* vice.o is being loaded */
		/*REFERENCED*/
		int e = unsetcrimevector(VICE_CPU_INTR, 
					 (intvec_func_t)vice_copy_intr);
		ASSERT(e);

		/* XXX change this 0xff to VICE_FAKEDEV -wsm7/9/96. */
		myatom = (*alloc)(0xff, vice_copy_intr);
		set = s;
		clear = c;
		lockable = l;
#ifdef DEBUG
		cmn_err(CE_NOTE, "vice.o loaded");
#endif
	} else {
		/* vice.o is unloading.  We have to take over */
		vice_steal_intr();
		set      = NULL;
		clear    = NULL;
		lockable = NULL;
		myatom   = NULL;
#ifdef DEBUG
		cmn_err(CE_NOTE, "vice.o unloaded");
#endif
	}
}


STATIC int
vice_init()
{
	int id;
	_crmreg_t intstat;

#ifdef DEBUG
	cmn_err(CE_NOTE, "vice_init()");
#endif /* DEBUG */

	if (set == NULL) {	
		/* vice.o isn't loaded... */

		/* ... init the chip ourselves */
		STORE32(VICE_INT_EN, VICE_INT_MSP_INTR);
		us_delay(100); /* delay really needed? */
		STORE32(MSP_SW_INT, 0); /* Probe by _writing_ to MSP_SW_INT */
		us_delay(100); /* XXX need something better, PIORD to Crime? */
		intstat = READ_REG64(PHYS_TO_K1(CRM_INTSTAT), _crmreg_t);

		if (!(intstat & CRM_INT_VICE))
			/* VICE not present */
			return 1;

		/* ... and prepare to service the interrupts */
		vice_steal_intr();

		STORE32(VICE_INT_RESET, VICE_INT_MSP_INTR);
		STORE32(VICE_INT_EN, 0);
	}

	/* if set is non-null, we know VICE exists, so this is safe */
	id = LOAD32(VICE_ID);

	if (!find_inventory(0,INV_COMPRESSION, INV_VICE, -1, -1, -1)) {
		add_to_inventory(INV_COMPRESSION, INV_VICE, 0, 0, id);
	}

	return 0;
}


STATIC int
vice_delay()
{
	if (lockable)
		/* vice.o is loaded; ask its status */
		return((*lockable)() ? 0 : HW_COPY_NOT_AVAILABLE);
	else
		/* vice.o not loaded; therefore VICE is idle */
		return 0;
}


/* TODO: as an optimization for the case where the client has specified
 * a buffer across multiple physical pages, we could allow hw_copy to
 * give them to us two pages at a time instead of one.  Then we could
 * use all 4 of channel 1's (descriptors instead of 2).  That will cut
 * the number of calls to vice_spin() (and the number of VICE
 * interrupts) in half.
 */

STATIC void
vice_copy(paddr_t from, paddr_t to, size_t bcount)
{
	/* Protect against races with ourself and with vice_hoard_lock() */
	if (set && (*set)(myatom) != myatom) {
		/* Between the time vice_delay() was called and now, someone
		 * snuck in and stole the lock.  Fall back on bcopy.
		 */
#ifdef DEBUG
		vice_copy_race++;
#endif
		bcopy((void *)PHYS_TO_K0(from), (void *)PHYS_TO_K0(to), bcount);
		return;
	}

#ifdef DEBUG
	cmn_err(CE_NOTE, "using VICE to bcopy 0x%x bytes from 0x%x to 0x%x",
		bcount, from, to);
#endif

	/* Sys Mem to Vice RAM one-time setup */
	STORE16(STRIDE(1, 1),  0);
	STORE16(LINES(1, 1),   1);
	STORE16(VMEM_Y(1, 1),  VICEMSP_DRAM);
	STORE16(DCTL(1, 1),    VICEDMA_DCTL_LOC_DRAM_A |
		               VICEDMA_DCTL_YC_BLK | VICEDMA_DCTL_RW_READ);

	/* Vice RAM to Sys Mem one-time setup */
	STORE16(STRIDE(1, 2),  0);
	STORE16(LINES(1, 2),   1);
	STORE16(VMEM_Y(1, 2),  VICEMSP_DRAM);
	STORE16(DCTL(1, 2),    VICEDMA_DCTL_LOC_DRAM_A |
		               VICEDMA_DCTL_YC_BLK | VICEDMA_DCTL_RW_WRITE |
		               VICEDMA_DCTL_HALT);

	/* Misc one-time setup */
	STORE16(VICE_INT_EN, VICE_INT_DMA1_DONE);
	STORE16(VICE_CFG, VICE_CFG_TLB_BYP);

copy_loop:
	vc_state = VC_ACTIVE;

	/* Sys Mem to Vice RAM */
	STORE16(SMEM_HI(1, 1), (__psint_t)from>>16);
	STORE16(SMEM_LO(1, 1), (__psint_t)from & 0xffff);
	STORE16(WIDTH(1, 1),   min(bcount, VICEMSP_DRAM_SIZE));

	/* Vice RAM to Sys Mem */
	STORE16(SMEM_HI(1, 2), (__psint_t)to>>16);
	STORE16(SMEM_LO(1, 2), (__psint_t)to & 0xffff);
	STORE16(WIDTH(1, 2),   min(bcount, VICEMSP_DRAM_SIZE));
	
	/* Go! */
	STORE16(CTL(1), VICEDMA_CTL_GO | VICEDMA_CTL_IE | 
		        VICEDMA_CTL_DESCR_PT0 | VICEDMA_CTL_TLB_BYP);

	if (bcount > VICEMSP_DRAM_SIZE) {
		bcount -= VICEMSP_DRAM_SIZE;
		from = (paddr_t)((__psint_t)from + VICEMSP_DRAM_SIZE);
		to   = (paddr_t)((__psint_t)to   + VICEMSP_DRAM_SIZE);
		vice_spin();
		goto copy_loop;
	}
}

STATIC void
vice_zero(paddr_t addr, size_t bcount)
{
	int starting_descriptor;

	if (set && (*set)(myatom) != myatom) {
		/* Between the time vice_delay() was called and now, someone
		 * snuck in and stole the lock.  Fall back on bzero.
		 */
#ifdef DEBUG
		vice_copy_race++;
#endif
		bzero((void *)PHYS_TO_K0(addr), bcount);
		return;
	}


	starting_descriptor = VICEDMA_CTL_DESCR_PT0;

	/* Zero out Vice RAM */
	STORE16(WIDTH(1, 1),   min(bcount, VICEMSP_DRAM_SIZE));
	STORE16(STRIDE(1, 1),  0);
	STORE16(LINES(1, 1),   1);
	STORE16(VMEM_Y(1, 1),  VICEMSP_DRAM);

	STORE16(DATA(1), 0);

	STORE16(DCTL(1, 1),    VICEDMA_DCTL_LOC_DRAM_A |
		               VICEDMA_DCTL_YC_BLK | VICEDMA_DCTL_FILL |
		               VICEDMA_DCTL_RW_READ);

	/* Vice RAM to Sys Mem one-time setup */
	STORE16(SMEM_HI(1, 2), (__psint_t)addr>>16);
	STORE16(SMEM_LO(1, 2), (__psint_t)addr & 0xffff);
	STORE16(STRIDE(1, 2),  0);
	STORE16(LINES(1, 2),   1);
	STORE16(VMEM_Y(1, 2),  VICEMSP_DRAM);
	STORE16(DCTL(1, 2),    VICEDMA_DCTL_LOC_DRAM_A | 
		               VICEDMA_DCTL_YC_BLK | VICEDMA_DCTL_RW_WRITE |
		               VICEDMA_DCTL_HALT);

	/* Misc one-time setup */
	STORE16(VICE_INT_EN, VICE_INT_DMA1_DONE);
	STORE16(VICE_CFG, VICE_CFG_TLB_BYP);

zero_loop:
	vc_state = VC_ACTIVE;

	/* Vice RAM to Sys Mem */
	STORE16(SMEM_HI(1, 2), (__psint_t)addr>>16);
	STORE16(SMEM_LO(1, 2), (__psint_t)addr & 0xffff);
	STORE16(WIDTH(1, 2),   min(bcount, VICEMSP_DRAM_SIZE));
	
	/* Go! */
	STORE16(CTL(1), VICEDMA_CTL_GO | VICEDMA_CTL_IE | VICEDMA_CTL_TLB_BYP |
		        starting_descriptor);

	if (bcount > VICEMSP_DRAM_SIZE) {
		bcount -= VICEMSP_DRAM_SIZE;
		addr = (paddr_t)((__psint_t)addr + VICEMSP_DRAM_SIZE);
		/* no need to re-zero vice memory the second time through */
		starting_descriptor = VICEDMA_CTL_DESCR_PT1;
		vice_spin();
		goto zero_loop;
	}
}



STATIC void
vice_spin()
{
#ifdef DEBUG
	cmn_err(CE_NOTE, "vice_spin(): vc_state==%d", vc_state);
#endif 
	while (vc_state != VC_IDLE)
		;
#ifdef DEBUG
	cmn_err(CE_NOTE, "vice_spin(): vc_state==%d", vc_state);
#endif 
}


/* defined here, referenced in hw_copy.c */
hw_copy_engine vice_engine = {
	vice_init,
	vice_copy, vice_zero,
	vice_delay, vice_spin,
	HW_COPY_CONTIGUOUS, 
	8,
	0
};


/* 
 * LOCAL definitions
 */

STATIC void
vice_steal_intr()
{
	int e;

	vc_state = VC_INIT;
	e = setcrimevector(VICE_CPU_INTR, SPL5, 
			   (intvec_func_t)vice_copy_intr, 0, 0);

	if (e) {
		cmn_err(CE_WARN, "VICE setcrimevector failed: code %d\n", e);
		/* something bizarre happened. make sure we never touch VICE */
		lockable = vice_never_lockable;
		return;
	}
}


static int
vice_never_lockable(void)
{
	return 0;
}


/*ARGSUSED*/
STATIC void 
vice_copy_intr(void *ignore)
{
#ifdef DEBUG
	cmn_err(CE_NOTE, "vice_copy_intr()");
#endif 

	if (vc_state == VC_IDLE)
		return;
#if 0
	/* We're only prepared to handle DMA-done interrupts generated
	 * by vice_copy() and vice_zero().  A spurious VICE_INT_MSP_INTR
	 * interrupt occurs when the chip is init'ed, so we tolerate
	 * that.
	 */
	ASSERT((vc_state == VC_ACTIVE && (vice_int & VICE_INT_DMA1_DONE)) ||
	       (vc_state == VC_INIT   && (vice_int & VICE_INT_MSP_INTR)));

#endif

	if (clear == NULL) {
		/* If vice.o is not loaded, we're running the show and
		 * this function is called as a CRIME interrupt handler,
		 * so we must clear the interrupt condition.
		 */
		int vice_int = LOAD32(VICE_INT);
	
		STORE32(VICE_INT_RESET, vice_int);
	} else {
		/* If vice.o is loaded, just let it know that we're done
		 * monkeying around with VICE.
		 */
		(*clear)();
	}
	vc_state = VC_IDLE;
}
