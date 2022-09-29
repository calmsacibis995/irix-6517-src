/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * MC VDMA controller routines.
 */



#if IP20 || IP22 || IP26 || IP28

#include "sys/types.h"
#include "sys/systm.h"
#include "ksys/as.h"
#include "os/as/as_private.h" /* XXX */
#include "sys/cmn_err.h"
#include "sys/sbd.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/pda.h"
#include "sys/sysinfo.h"
#include "sys/ksa.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/cpu.h"
#include "sys/immu.h"
#include "sys/vdma.h"
#include "os/as/pmap.h" /* XXX */

#define TLB_REG_STEP    (DMA_TLB_HI_1 - DMA_TLB_HI_0)
#define uTLB_HI(i) (*(uint *)(PHYS_TO_COMPATK1(DMA_TLB_HI_0 + (i) * TLB_REG_STEP)))
#define uTLB_LO(i) (*(uint *)(PHYS_TO_COMPATK1(DMA_TLB_LO_0 + (i) * TLB_REG_STEP)))
#define DTLB_VALID              0x2             /* bit 1 */


#define VPTESIZE sizeof(pde_t)

/* VNBPS == number of bytes in a virtual segment, which is the amount
 * of memory mapped by one page of page table entries
 */
#define VNBPS NBPS
#define VSOFFMASK (VNBPS - 1)

/* bytes to virtual segments */
#define vbtost(x) btost(x)


#if DEBUG
int vdmadbg = 0;
int vptedbg = 0;
int vcxdbg = 0;
int video_dbg = 0;
#endif


/*
 * vdma controller state flags
 */
int vdma_state = 0;

/*
 * MC revision.  
 *
 *	0 : No virtual address translation.
 *	    Must write MC regs with writemcreg or equivalents
 *	1 : Above problems are fixed.
 */

extern int mc_rev_level; /* The 'real' mc rev */
int vdma_rev;		/* Our copy, for simpler debugging. */

void __dcache_wb(void *, int);

/*
 * Initialize VDMA controller.
 */

void
VdmaInit ( void )
{
        /*
         * Initialize DMA ctl register:
         *      Tlim = 512, Slim = 4, 
         *      address translation, interrupt disabled, 
	 *	cache size = 32 words, page = 4k bytes,
	 * XXX The above 2 items are NOT manifest constants.
	 * 
         */
#if _PAGESZ == 16384
        writemcreg (DMA_CTL, 0x2000410e | (VPTESIZE >> 3));
#elif _PAGESZ == 4096
        writemcreg (DMA_CTL, 0x2000410c | (VPTESIZE >> 3));
#else
#error vdma cant do other than 16K or 4K pages
#endif

        /* Zero out DMA address mask and subst. */
        writemcreg (DMA_GIO_MASK, 0);
        writemcreg (DMA_GIO_SUB, 0);

	/* Clear status bits */
	writemcreg (DMA_CAUSE, 0);
        /*
         * Clear uTLB
         */
        writemcreg (DMA_TLB_HI_0, 0);
        writemcreg (DMA_TLB_LO_0, 0);
        writemcreg (DMA_TLB_HI_1, 0);
        writemcreg (DMA_TLB_LO_1, 0);
        writemcreg (DMA_TLB_HI_2, 0);
        writemcreg (DMA_TLB_LO_2, 0);
        writemcreg (DMA_TLB_HI_3, 0);
        writemcreg (DMA_TLB_LO_3, 0);

#if DEBUG
	/* because that was the way the code read before */
	ASSERT(VPTESIZE == 4);
	if (vdmadbg)
		printf ("VDMA controller rev %d initialized\n", mc_rev_level);
#endif
	vdma_state |= VDMA_INITTED;

}

#if DEBUG

void
vdma_regdump(void)
{
	printf("buf %x mode %x size %x stride %x gioadr %x count %x\n",
		VDMAREG(DMA_MEMADR), VDMAREG(DMA_MODE), VDMAREG(DMA_SIZE),
		VDMAREG (DMA_STRIDE), VDMAREG(DMA_GIO_ADR), VDMAREG(DMA_COUNT));
	printf ("CTL %x TLBs %x %x %x %x %x %x %x %x\n",
		VDMAREG(DMA_CTL),
			VDMAREG(DMA_TLB_HI_0), VDMAREG(DMA_TLB_LO_0), 
			VDMAREG(DMA_TLB_HI_1), VDMAREG(DMA_TLB_LO_1),
        		VDMAREG(DMA_TLB_HI_2), VDMAREG(DMA_TLB_LO_2),
			VDMAREG(DMA_TLB_HI_3), VDMAREG(DMA_TLB_LO_3));
}
#endif

/* kv address from which uTLB_LO[i] was derived */
static pde_t  *pgtbl_kvaddr[4];

/*
 * Try to fix an error condition.
 * Returns 0 if we think the transfer can be resumed,
 * -1 if we couldn't fix things up.
 * 
 * Currently we assume the buffer was locked down
 * via userdma(), so the only error we might see is a 
 * missing PG_VR in the pte for the faulting page.
 */

int
vdma_fault (int status)
{
	int i;
	unsigned vaddr;
	unsigned tmp;
	uint *physpte;
	uint *pte;

	/*
	 * The controller sometimes reports an error status
	 * together with COMPLETE.  COMPLETE is to be believed
	 * in that case, the error is bogus.
	 */
	if (status & VDMA_R_COMPLETE)
		return 0;

	if (status & 
		(VDMA_R_UTLBMISS | VDMA_R_CLEAN | VDMA_R_BMEM | VDMA_R_BPTE))
		return -1;

	/*
	 * Page fault. 
	 * Get the faulting address from MEMADR.  Locate the uTLB
	 * slot for the faulting address.  Get the page table
	 * address from the uTLB.  If PG_VR was not set in the pte
	 * for the faulting address, set it and try again, else
	 * return error.
	 */

	/*
	 * Locate the uTLB slot for vaddr
	 */
	vaddr = VDMAREG (DMA_MEMADR);
	tmp = vaddr & ~ VSOFFMASK;

	for (i = 0; i < 4; i++) {
		if ((uTLB_HI(i) == tmp) && (uTLB_LO(i) & DTLB_VALID))
			break;
	}
	if (i == 4) {
#if DEBUG
		printf ("vdma_fault: no tlb for %x %x\n", vaddr, tmp);
		for (i = 0; i < 4; i++)
			printf ("%x %x ", uTLB_HI(i), uTLB_LO(i));
		printf ("\n");
#endif
		return -1;
	}
	/*
	 * Make vaddr relative to beginning of space mapped by this tlb slot.
	 */
	vaddr -= tmp;

	/*
	 * Get the kernel virtual address of the page table
	 * mapping the fault address.
	 */

	pte = (uint *)pgtbl_kvaddr[i];		/* KV addr of pgtbl */

	{	/* temporary is only to shut up compiler so we can make warning 1413
		 * fatal (src to small to hold pointer).  In this case we know the
		 * value is always small enough to fit.  The macro has to be an int,
		 * because we only want 32 bits */
		ulong_t ltmp;
		ltmp = (ulong_t)((uTLB_LO(i) & ~DTLB_VALID) << 6); /* phys addr "" */
		physpte = (uint *)ltmp;
	}
	
	ASSERT (poff(pte) == 0);

#ifdef DEBUG
	ASSERT ((uint *)kvtophys(pte) == (uint *)physpte);
#else
	if ((uint *)kvtophys(pte) != (uint *)physpte)
		return -1;
#endif

	pte += pnum(vaddr);		/* point to pte we want */
	physpte += pnum(vaddr);
	tmp = *pte;			/* pte of fault addr */

#if DEBUG
	if (vptedbg)
		printf ("vdma_fault: buf %x tlb %x %x pte %x *pte %x\n",
		VDMAREG (DMA_MEMADR), uTLB_HI(i), uTLB_LO(i), pte, tmp);
#endif
	/*
	 * The software valid bit really should be on.
	 * If it isn't, somebody screwed up; we can't fix it.
	 * For gfx to host transfer, PG_M should be on.
	 */
	if ( ! (tmp & PG_SV) )
		return -1;
	if ( (VDMAREG (DMA_MODE) & VDMA_M_GTOH) && ! (tmp & PG_M) )
		return -1;

	/*
	 * The hw valid bit is sometimes off, but since 
	 * sw valid is on, we can safely turn on PG_VR and retry. 
	 * If it is on already, we don't know what the problem is.
	 */

	if ((tmp & PG_VR) == PG_VR)
		return -1;
	tmp |= PG_VR;


        /*
         * Update the page table entry, and flush the
         * cache for that entry so the vdma hw sees
         * the updated entry, not stale data.
         * This is easier to do if the page table we're
         * updating is in KDM space.
	 *
	 * IP26/IP28 define _NO_UNCACHED_MEM_WAR, which
	 * usually disallows uncached memory writes.
         */
#ifndef _NO_UNCACHED_MEM_WAR
        if (pnum(physpte) < SMALLMEM_PFNMAX) {  /* direct mappable */
		/* simulate a cacheflush by writing K0 then K1 */
                *(uint *)(PHYS_TO_K0(physpte)) = tmp;
                *(uint *)(PHYS_TO_K1(physpte)) = tmp;
        } else
#endif
	{
                *pte = tmp;
                dcache_wb (pte, sizeof (*pte));
        }

	return 0;
}

/*
 * Wait for vdma transfer done or timeout.
 * Return 0 for normal completion,
 *	 -1 for timeout
 *	 -2 for bad status
 */

#define DMATIMEOUT (500000 * (_PAGESZ / 4096))
static int
vdma_wait( int rex_hack )
{
	int i = 0;
	int last_addr, x;

	uint retry = 0; /* DMA_MEMADR at last failed transfer */

	/*
	 * Poll for DMA not RUNNING.
	 */
again:

	if (!rex_hack){
		for (i=0; i<DMATIMEOUT; i++) {
			if (VDMAREG (DMA_RUN) & VDMA_R_RUNNING)
				DELAY(1); /*	XXX history ! */
			else
				break;
		}
	} else { /* work around REX bug */

		last_addr = VDMAREG (DMA_MEMADR);
		DELAY(1);
still_going:
		while (last_addr != (x=VDMAREG (DMA_MEMADR))){
			last_addr = x;
			DELAY(2);
		}
		if (VDMAREG (DMA_RUN) & VDMA_R_RUNNING){
			DELAY(2);
			if (last_addr != VDMAREG (DMA_MEMADR)){
				goto still_going;
			} else {
				i = DMATIMEOUT;
			}
		} 
	} 


	/* 
	 * If we timed out, stop the dma.
	 */
	if (i == DMATIMEOUT) {
		if (!rex_hack)
			cmn_err(CE_WARN,"VDMA  TIMEOUT, vdma status %x", VDMAREG(DMA_RUN)); 
		writemcreg (DMA_STDMA, 0);

		/* Wait for xfer to really stop */
		while (VDMAREG(DMA_RUN) & VDMA_R_RUNNING){
			DELAY(1);
		}
#if DEBUG
		vdma_regdump();
#endif
		return -1;
	}

	/* 
	 * Check for normal termination.  If DMA_COMPLETE
	 * is set, ignore other status bits.
	 * If transfer halted because of an error, let
	 * vdma_fault() try to fix it.  If it does,
	 * restart the transfer.  We don't retry a transfer 
	 * if it fails twice at the same address.
	 */
	if ( ! ((i = VDMAREG(DMA_RUN)) & VDMA_R_COMPLETE)) {

		if ((retry != VDMAREG (DMA_MEMADR)) && ! vdma_fault(i)) {
#if DEBUG
			if (vptedbg)
			printf ("vdma_wait: retrying after error %x\n", i);
#endif
			VDMAREG (DMA_CAUSE) = 0;	/* clear status */
			VDMAREG (DMA_STDMA) = 1;	/* resume transfer */
			goto again;			/* wait for something */
		}
		cmn_err(CE_WARN,"VDMA error : MC rev %d (%d) status %x\n", 
			mc_rev_level, vdma_rev, i);
#if DEBUG
		vdma_regdump();
#endif
		return -2;
	}

	return 0;
}


/*
 * Load the vdma controller registers from
 * vp and start the transfer.
 * If ena_int, the DMA COMPLETE interrupt is 
 * enabled and we return immediately, else
 * we poll for completion in vdma_wait()
 * and return error status (0 means success).
 *
 * Assumes uTLB already set up (vdma_set_tlb()).
 * NOTE : assumes MC rev > 0.
 */

int
MCdma (vdma_args_t *vp, int ena_int)
{
	unsigned int safe_stride;

#if DEBUG
        if (vdmadbg)
                printf ("MCdma: buf %x mode %x h %x w %x s %x z %x g %x\n",
                        vp->memadr, vp->mode, vp->height, vp->width,
                        vp->stride, vp->yzoom, vp->gioaddr);
#endif

	/*
	 * Init controller if not already done.
	 */
	 
	if ( ! (vdma_state & VDMA_INITTED) )
		VdmaInit();

        /* 
	 * nuke any old error status bits - they're sticky 
	 */
        VDMAREG (DMA_CAUSE) = 0;

        /* 
	 * Enable virtual address translation .
	 * Enable DMA_COMPLETE interrupt if requested.
	 */

	if ((ena_int) && (ena_int != REX_BUG))
		VDMAREG (DMA_CTL) = VDMAREG(DMA_CTL) | VDMA_C_XLATE | VDMA_C_IE;
	else
		VDMAREG (DMA_CTL) = (VDMAREG(DMA_CTL)|VDMA_C_XLATE )&~VDMA_C_IE;

	/*
	 * work-around for occasional tlb-miss error
	 */
	safe_stride = vp->stride & 0xffff;
	if (vp->height == 1)
		safe_stride = 0;

	/*
	 * Load the VDMA controller registers and go
	 */

	VDMAREG (DMA_MEMADR) = (__uint32_t)(__psunsigned_t)vp->memadr;
	VDMAREG (DMA_MODE) = vp->mode;
	VDMAREG (DMA_SIZE) = (vp->height << 16) | (vp->width & 0xffff);
	VDMAREG (DMA_STRIDE) =  (vp->yzoom << 16) | safe_stride;
	VDMAREG (DMA_GIO_ADRS) = (__uint32_t)(__psunsigned_t)vp->gioaddr;

	flushbus();

	if (ena_int == REX_BUG)
		return(vdma_wait(1));
	return ena_int ? 0 : vdma_wait(0);

}

/*
 * Disable VDMA interrupts.
 */

void
MCIntClr ( void )
{
	
	VDMAREG (DMA_CAUSE) = 0;

	setlclvector (VECTOR_GDMA, NULL, 0);

}

/*
 * setup the vdmatlb for a dma transfer.  vaddr is the
 * virtual address of the data to be transferred.
 * nbytes is the buffer length.  
 * 
 * There are only 4 slots in the uTLB, which means we can
 * set up for 16 MB max.  It's not an error if the buffer
 * is longer than that, it just means that the VDMA controller
 * will raise a uTLBmiss exception later.  
 *
 * returns 0 if ok, else -1 if the buffer is not 
 * contained in the region.
 *
 * XXX Should do better checking of parameters.
 * 
 */

int
vdma_set_tlb (caddr_t vaddr, int nbytes)
{
	int nsegs;
	preg_t *prp;
	pde_t *segpde;
	int i;
	/* compute # of pages on which the dma area of memory lies */
	int size = pnum(poff(vaddr) + nbytes - 1) + 1;
	pgno_t rqst;

	/*
	 * Init controller if not already done.
	 */
	 
	if ( ! (vdma_state & VDMA_INITTED) )
		VdmaInit();

	/*
	 * K2 seg address more or less implies we are 
	 * called from the video driver at interrupt time.
	 */ 
        if (IS_KSEG2(vaddr)) {
                nsegs = vbtost(soff(vaddr) + nbytes) + 1;
                segpde = kvtokptbl(vaddr);
                dcache_wb ( segpde, numpages(vaddr, nbytes) * VPTESIZE );
#if DEBUG
		if (video_dbg)
			printf ("K2 vdma: vaddr %x nbytes %x nsegs %x"
				"segpde %x\n", vaddr, nbytes, nsegs, segpde);
#endif
        	/*
         	 * Setup vdma uTLB.
          	 */
                for (i = 0; i < nsegs; i++) {
			uTLB_LO(i) = (pnum(kvtophys(segpde)) << (BPCSHIFT - 6))
						| DTLB_VALID;
			uTLB_HI(i) = (__psunsigned_t)vaddr & ~VSOFFMASK;

                	/*
                 	 * Save the page table virtual address for
			 * easier vdma_fault() handling.
                 	 */

			pgtbl_kvaddr[i] = (pde_t *)(pnum(segpde) << BPCSHIFT);
#if DEBUG
			if (video_dbg)
				printf ("tlb hi %x lo %x\n", 
					uTLB_HI(i), uTLB_LO(i));
#endif
                        vaddr += VNBPS;
                        segpde = kvtokptbl(vaddr);
                }
        } else {
		vasid_t vasid;
		pas_t *pas;
		ppas_t *ppas;

		as_lookup_current(&vasid);
		pas = VASID_TO_PAS(vasid);
		ppas = (ppas_t *)vasid.vas_pasid;
		ASSERT (! IS_KSEGDM (vaddr) );

		/*
		 * Get the region pointer so we
		 * can find the page table for this buffer.
		 */
		mraccess(&pas->pas_aspacelock);
		if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
#if DEBUG
                	printf ("findpreg failed in vdma_set_tlb\n");
#endif
			mrunlock(&pas->pas_aspacelock);
			return -1;
        	}
		mrunlock(&pas->pas_aspacelock);

		/*
		 * Each uTLB entry should point at a page of page-table
		 * entries.
		 */
		for (i = 0; i < 4 && size; i++) {
			pde_t *ptep;
			rqst = MIN(size, NPGPT);
			ptep = pmap_ptes(pas, prp->p_pmap, vaddr, &rqst, VM_NOSLEEP);
			if (ptep == NULL || rqst == 0) {
				return -1;
			}
			uTLB_HI(i) = (__psunsigned_t)vaddr & ~VSOFFMASK;
			/*
			 * Put the physical page frame number where the
			 * ptes for this segment are stored into uTLB_LO 
			 * bits 25-6.  Turn on the pte valid bit in the uTLB_LO.
			 */
			uTLB_LO(i) = (pnum(kvtophys(ptep)) << (BPCSHIFT - 6))
						| DTLB_VALID;
                	/*
                 	 * Save the page table virtual address for
			 * easier vdma_fault() handling.
                 	 */

			pgtbl_kvaddr[i] = (pde_t *)(pnum(ptep) << BPCSHIFT);

#if DEBUG
			if (vdmadbg)
			    printf ("set tlb(%d) hi %x tlblo %x\n", 
				i, uTLB_HI(i),
				(pnum(kvtophys(ptep)) << (BPCSHIFT - 6))
						| DTLB_VALID);
#endif
			size -= rqst;
			vaddr += ctob(rqst);
			dcache_wb ((void *)((__psunsigned_t)ptep & ~POFFMASK),
								ctob(1));
		}
		if(size) { /* couldn't fit all of dma request into dma buffers */
			return -1;
		}
	}
	
	/*
	 * if stride is negative, and the first line transferred
	 * ends at a segment boundary, then the vdma controller will
	 * do a uTLB lookup for the next segments page table address,
	 * even though that page table will not be referenced.
	 * To avoid a uTLB miss exception, we fake a valid entry
	 * for that segment in the uTLB. 
	 */

	if (i < 4) {
		uTLB_HI(i) = uTLB_HI(i-1) + VNBPS;
		uTLB_LO(i) = DTLB_VALID;
	}
	i++;
	for (; i < 4; i++) 	/* invalidate unused tlb entries */
		uTLB_HI(i) = 0;

	return 0;
}


/*
 * Virtual dma to/from kernel virtual space (K012seg). 
 * If the buffer is in K0 or K1, it is assumed that
 * the buffer is physically contiguous and that the transfer
 * can be done in one chunk across page boundaries.
 *
 * Used for graphics context switching (EXPRESS only -
 * 	that's why VDMA_M_SYNC is set in the MODE register.
 * 	May not need SYNC for cx switch).
 * Caller is responsible for cache coherency.
 * NOTE : assumes MC rev > 0.
 */

int
vdma_kv (__psunsigned_t kv, int nbytes, int dir, uint gioaddr)
{
	register int n;
	register int tmp;
	register pde_t *kpde;

	/*
	 * Init controller if not already done.
	 */
	 
	if ( ! (vdma_state & VDMA_INITTED) )
		VdmaInit();

	VDMAREG (DMA_CAUSE) = 0;	/* clear sticky status */

	VDMAREG (DMA_MODE) = 
			VDMA_M_INCA | VDMA_M_LBURST | VDMA_M_SYNC |
				(dir ? VDMA_M_GTOH : 0); 
	VDMAREG (DMA_STRIDE) = 1 << 16;

	/*
	 * If kv is in a direct mapped segment use physical addressing,
	 * else use virtual addressing.
	 * No interrupts. 
	 */

	if (IS_KSEGDM (kv)) {
		VDMAREG (DMA_MEMADR) = kvtophys ( (void *)kv);
		VDMAREG (DMA_CTL) = VDMAREG (DMA_CTL) & 
					~ ( VDMA_C_XLATE | VDMA_C_IE );
	}
	else {
		VDMAREG (DMA_MEMADR) = kv;
		VDMAREG (DMA_CTL) = (VDMAREG (DMA_CTL) | VDMA_C_XLATE) & 
								~VDMA_C_IE;
	/*
	 * Figure out how many uTLB slots are needed.
	 * Flush the page table entries mapping the buffer.
	 */
		tmp = vbtost(soff(kv) + nbytes) + 1;
		kpde = kvtokptbl(kv);
		dcache_wb ( kpde, numpages(kv, nbytes) * VPTESIZE );

#if DEBUG
		if (vcxdbg)
			printf ("vdma_kv: kv %x - %x n %x nseg %d kpde %x\n",
				kv, kv + nbytes, nbytes, tmp, kpde);
#endif
	/*
	 * Setup vdma uTLB.
	 */
		for (n = 0; n < tmp; n++) {

			uTLB_LO(n) = (pnum(kvtophys(kpde)) << (BPCSHIFT - 6)) | DTLB_VALID;
                	/*
                 	 * Save the page table virtual address for
			 * easier vdma_fault() handling.
                 	 */
			pgtbl_kvaddr[n] = (pde_t *)(pnum(kpde) << BPCSHIFT);

			uTLB_HI(n) = kv & ~VSOFFMASK;
			kv += VNBPS;
			kpde = kvtokptbl(kv);
		}
		for (; n < 4; n++)
			uTLB_HI(n) = 0;

	}

	/*
	 * Do the transfer.
	 */

	while (nbytes > 0) {
		if (nbytes <= 0xffff) {
			VDMAREG (DMA_SIZE) = (1 << 16) | nbytes;
			nbytes = 0;
		}
		else {	/* do 'lines' of 32k this pass */
			VDMAREG (DMA_SIZE) = ((nbytes/0x8000) << 16) | 0x8000;
			nbytes %= 0x8000;
		}
		VDMAREG (DMA_GIO_ADRS) = gioaddr;

		if (dir = vdma_wait(0))
			break;		/* dma failed */
	}

	return dir;
}

#define dap_crosspage(dap) ((io_poff(dap) + sizeof(gdmada_t)) > IO_NBPC)

/*
 * PIC-style DMA, using the VDMA controller.
 */

extern void write4vdma(long *, int, int, int);

int
fakePICdma (gdmada_t *dmadap, int modes)
{
        int width;
	int gioadr;
        int retval, i;
	int first = 1;

	/*
	 * Init controller if not already done.
	 */
	 
	if ( ! (vdma_state & VDMA_INITTED) )
		VdmaInit();


        /* 
	 * Disable virtual address translation 
	 */

        i = VDMAREG(DMA_CTL) & ~ VDMA_C_XLATE;


        writemcreg(DMA_CTL, i);

        /* 
	 * nuke any old error status bits - they're sticky 
	 */

        writemcreg(DMA_CAUSE, 0);


	gioadr = dmadap->grxaddr;

	/*
	 * Process the list of DMA descriptors.
	 */

        while (1) {
#if DEBUG
        	if (vdmadbg) {
			printf("DA: addr 0x%x len 0x%x flags 0x%x\n",
        			dmadap->bufaddr, poff(dmadap->dmactl),
				dmadap->dmactl &
					PICGDMA_GTOH | GDMA_LAST | GDMA_FULLPG);
			printf("    gfx address 0x%x stride %d line count %d, next dmada 0x%x\n",
        			dmadap->grxaddr, dmadap->stride, 
				dmadap->linecount,dmadap->nextdesc);
        	}
#endif

                /*
                 * Set up vdma controller for this transfer.
                 */

                width = (dmadap->dmactl & GDMA_FULLPG) ? NBPP :
                                                        poff(dmadap->dmactl);
                width |= ((dmadap->linecount+1) << 16);

		if (dmadap->dmactl & PICGDMA_GTOH)
			modes |= VDMA_M_GTOH;
		else
			modes &= ~VDMA_M_GTOH;

                i = (1 << 16) | (dmadap->stride & 0xffff);

		/*
		 * Wait for previous transfer to complete
		 * Must skip this first time through.
		 */

		if ( !first ) {
			if (retval = vdma_wait(0)) 
				break;
		}
		else
			first = 0;

		/*
		 * Set up DMA_{MEMADR, MODE, SIZE, STRIDE} registers
		 */
#if DEBUG
		if (vdmadbg)
			printf ("write4vdma %x %x %x %x\n",
				dmadap->bufaddr, modes, width, i);
#endif

		/* XXX Make this write5vdma - should do gioadr too */

		write4vdma (dmadap->bufaddr, modes, width, i);

                /* 
		 * Start the DMA 
		 */

                writemcreg(DMA_GIO_ADRS, gioadr);
                flushbus();     /* XXX need this ? */

		/*
		 * Break if this is the last dma descriptor.
		 */
                if (dmadap->dmactl & GDMA_LAST) {
			retval = vdma_wait(0);
                        break;
		}

		/*
		 * Bump to next descriptor - bump again
		 * to avoid straddling a page.
		 */
		 * 

                dmadap++;
                if (dap_crosspage(dmadap))
                        dmadap++;
        }
	return retval;
}
#endif	/* IP20 || IP22 || IP26 || IP28 */
