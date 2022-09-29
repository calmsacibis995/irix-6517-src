/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.76 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/vmereg.h>
#include <sys/immu.h>
#include <sys/systm.h>
#include <sys/sbd.h>
#include <sys/pda.h>
#include <sys/dmamap.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/iotlb.h>
#include <sys/debug.h>
#include <sys/pio.h>
#include <sys/invent.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/groupintr.h>


#ifdef ULI
#include <sys/errno.h>
#endif

/* slots numbered 1-15 */
#define MAXVMEPOS	(16*4)
int vmeccind[MAXVMEPOS];

vmecc_t	vmecc[EV_MAX_VMEADAPS];
static uint vmecc_prev_werror[EV_MAX_VMEADAPS];

/* Mutex array used to check if any VMECC error handling is in progress 
 * This is needed to make sure that while handling write errors, any other
 * read error should not come by and clean up the mess, leaving write 
 * error confused.
 */
int	vmecc_errhandling[EV_MAX_VMEADAPS];

#define	VMECC_IRQMAX	8
char	vmespl_lvl[VMECC_IRQMAX];

void	dump_hwstate(int);


/*
 * VMECC interrupt handler routines.
 * We will NOT loop to look for and handle more VME interrupts from the VMECC
 * once the current one is done. It makes more sense to do it in intr().
 *
 * The arg passed are,
 *	bit[7:0]	the interrupt bit number to set/reset interrupt mask reg
 *	bit[15:8]	adapter id
 *	bit[31:16]	not used
 */

#define VMECC_INTRARG(adap,inum)	((adap<<8)|inum)
#define VMECC_INTRARG_ADAP(arg)		(arg>>8)
#define VMECC_INTRARG_INUM(arg)		(arg&0xFF)

#define VMEPIO4			4
#define VME_SPTOA(slot,pa)	((slot)*VMEPIO4+pa)
#define VME_ATOS(adap)		((adap)/VMEPIO4)
#define VME_ATOPA(adap)		((adap)%VMEPIO4)

int
vmecc_io4atova(int i4adap)
{
	switch( i4adap ) {
	case 2:
		return 0;
	case 3:
		return 1;
	case 5:
		return 2;
	case 6:
		return 3;
	}

	/* this is an invalid io4 position for VME */
	ASSERT(0);
	return -1;
}

/*
 * A weird supprt routine needed by RE Graphics which connects to
 * a board on VME. Given a slot, and graphics adapter, they need
 * to know the VME adapter which is "possibly" associated with that
 * adapter.
 * This routine is tightly associated with the CC3 configuration 
 * of graphic pipes. So this will break if CC3 configuration for
 * Graphic pipes change!!!
 * As per the CC3 configuration guide, Primary Graphics pipe  is on
 * Master IO4 and padap 2. This would be connected to VME board (if any)
 * present in VME Bus(padap3) from Primary IO4 only.
 * For details on additional pipes, and how they are configured,
 * refer CC3 configuration guide.
 * Return : -1 on error.
 * 	appropriate vmecc xternal adapter if success.
 */
int
vmecc_gfx_vmexadap(int slot, int padap)
{
	int	vmeadap,window;

	if ((slot < 0) || (slot > EV_MAX_SLOTS))
		return -1;
	
	for (window=0; window < EV_MAX_IO4S; window++)
		if (io4[window].slot == slot)
			break;

	if (window == EV_MAX_IO4S)
		return (-1);
	
	if ((window == 1) && (padap == 2)){
		if (vmecc[0].window == 1)
			return 0;
		return(-1);
	}
	
	/* Otherwise <slot,padap> combination points to a graphics
	 * adapter not associated with Primary VME bus. So, return
	 * appropriate xadap
	 */
	if (padap == 5)
		vmeadap = 2;

	else if (padap == 6)
		vmeadap = 3;

	else	return(-1);

	return(VME_SPTOA(slot, vmecc_io4atova(vmeadap)));
}

/*
 * Search for the piomap corresponding to the write address, 
 * Check if there is an error handler attached to this piomap
 * and call the error handler 
 */
vmecc_write_error(int	vmeadap, iopaddr_t vmeaddr)
{
	typedef uint (*PioErrFunc)(piomap_t *, iopaddr_t, uint_t);

	piomap_t	*pmap = 0;
	PioErrFunc func;
	__psunsigned_t	swin;

       	while (pmap = pio_ioaddr(ADAP_VME, vmeadap, vmeaddr, pmap)){

		func = (PioErrFunc)pio_geterrf(pmap);
        	if (func && 
		    (func(pmap,vmeaddr,pmap->pio_adap))){

			swin = vmecc[vmeadap].ioadap.swin;
			/* Cleanup and head back !! */
			EV_GET_REG(swin + VMECC_ERRCAUSECLR);
			EV_SET_REG(swin + VMECC_ERRXTRAVME, 1); /* Unlock */
			EV_SET_REG(swin + VMECC_INT_ENABLESET, 
				1L << VMECC_INUMERROR);
                	return 1;
        	}
	}
	return 0;
}

/*
 * Gets called only via intr->everest_error_intr since intr being smart
 * redirects all error interrupts directly to everest_error_intr
 * Switch(VMECC_INUMERROR) never gets used.
 * Return : 1 => Error handled.. 0 => panic system..
 */

extern int ev_wbad_inprog, ev_wbad_val;

/* 
 * IO4 IA WAR start
 */
extern struct vme_ivec *vme_ivec;
/*
 * IO4 IA WAR End
 */

#if IP19 || IP25
/*ARGSUSED*/
int
vmecc_error_intr(eframe_t* ep, void *arg)
{
        uint     	verr, adap;
	__psunsigned_t	swin;
        extern  	int nvmecc;
	int		retval;
	int		noerr = 1; /* indicates No VMECC has error bit set */
        iopaddr_t    	vmeaddr, vmexaddr;

        for (adap=0; adap < nvmecc; adap++){
                verr = EV_GET_REG(vmecc[adap].ioadap.swin + VMECC_ERRORCAUSES);

		/* Error reg has no error & No previous write error record */
		if (verr == 0 ) {
			if (vmecc_prev_werror[adap] == 0)
				continue;
			else{
				verr = vmecc_prev_werror[adap];
				vmecc_prev_werror[adap] = 0;
			}
		}

		noerr = 0;
		if (atomicAddInt(&vmecc_errhandling[adap], 1) != 1) {
			/* Some other error handling is in progress on this
			 * adapter. But this could be the one we were 
			 * supposed to handle. So, mark that error may
			 * have been handled. But continue looking for
			 * a VMECC having error 
			 */
			retval = 1;
			goto err_end;
		}

		/* error bits that could be set and still we can recover */
#define	VMECC_WRITE_ERRORS	(VMECC_ERROR_VMEBUS_PIOW | \
				 VMECC_ERROR_VMEBUS_PIOR | \
				 VMECC_ERROR_OVERRUN)
		if (verr & ~VMECC_WRITE_ERRORS) {
			/* Some fatal error. Return */
			retval = 0;
			break;
		}
#undef	VMECC_WRITE_ERRORS

		/* If write error due to probe... return */
		if (ev_wbad_inprog) {
			ev_wbad_val = 1;
			everest_error_clear(0);
			retval = 1;
			break;
		}

		swin = vmecc[adap].ioadap.swin;

		/* Get vmeaddr and vmexaddr */
		vmexaddr = EV_GET_REG(swin + VMECC_ERRXTRAVME);
		if (!(vmexaddr & 
			(VMECC_ERRXTRAVME_XVALID|VMECC_ERRXTRAVME_AVALID))){
			/* This is difficult to decide.
			 * This vmecc has a read/write error bit set. But
			 * doesnot have Valid ADDRVME Or XTRAVME bit set.
			 * So, there is no way to find out who did this.
			 *  Let's kill system for this.
			 */
			 retval = 0;
			 break;
		}

		vmeaddr = EV_GET_REG(swin + VMECC_ERRADDRVME);
		if (vmecc_write_error(adap, vmeaddr)){
			/* Found the process which caused the VME error */
			retval = 1;
			break;
		}

err_end:
		atomicAddInt(&vmecc_errhandling[adap], -1);

	}
	if (adap != nvmecc)
		atomicAddInt(&vmecc_errhandling[adap], -1);
	if (noerr)
		retval = 1;
	
	return retval;
}
#endif /* IP19 || IP25 */

#if IP21
extern lock_t vmewrtdbe_lock;

#define VME_OKERRS	(VMECC_ERROR_VMEBUS_PIOW | VMECC_ERROR_VMEBUS_PIOR \
	| VMECC_ERROR_OVERRUN)

/* ARGSUSED */
int
vmecc_error_intr(eframe_t* ep, void *arg)
{
        uint     	i, err, ws;
	__psunsigned_t	swin;
        extern  	int nvmecc;
	extern 		time_t vme_overrun_time;
        iopaddr_t    	vmeaddr, vmexaddr;
	int		retval = 0;

	ws = splock(vmewrtdbe_lock);
	DELAY(300);
        for (i=0; i < nvmecc; i++){
                err = EV_GET_REG(vmecc[i].ioadap.swin + VMECC_ERRORCAUSES);
		/* panic on these errors. */
		if( err & ~VME_OKERRS )
			goto err_exit;

		/* This is what we're looking for */
		if( (err & VMECC_ERROR_VMEBUS_PIOW) || vmecc_prev_werror[i] )
			break;
        }

	if (i == nvmecc) {
		/* We'll ignore this one if we think we may have already
		 * cleared it up while servicing a previous error.
		 */
#if DEBUG
		printf("VME rd err: lbolt = 0x%x:0x%x\n",
			lbolt,vme_overrun_time);
#endif /* DEBUG */
		if( lbolt - vme_overrun_time < 2 ) {
			cmn_err(CE_WARN,"VME wrt err: overrun on cpu %d",
				cpuid());
			retval = 1;
		}

		goto err_exit;
	}

#if DEBUG
	if( err & VMECC_ERROR_OVERRUN )
		printf("VME wrt err: overrun set on cpu %d at 0x%x\n",
			cpuid(),lbolt);
#endif /* DEBUG */

	vme_overrun_time = lbolt;

	vmecc_prev_werror[i] = 0;
	/* If write error due to probe... return */
	if (ev_wbad_inprog) {
		ev_wbad_val = 1;
		retval = 1;
		goto err_exit;
	}

	/* XXX is the address still latched for a prev_werror? */
        /* Get vmeaddr and vmexaddr */
	swin = vmecc[i].ioadap.swin;
        vmexaddr = EV_GET_REG(swin + VMECC_ERRXTRAVME);
        if (!(vmexaddr & (VMECC_ERRXTRAVME_XVALID|VMECC_ERRXTRAVME_AVALID)))
		goto err_exit;

       	vmeaddr = EV_GET_REG(swin + VMECC_ERRADDRVME);
	retval = vmecc_write_error(i, vmeaddr);

err_exit:
	if( retval )
		everest_error_clear(0);
	spunlock(vmewrtdbe_lock,ws);
	return retval;
}

int
vmecc_rd_error(int *slot, int *padap)
{
        int     	i, err;
        extern  	int nvmecc;

	/* Kind of assumes only 1 error on 1 bus... */
        for (i=0; i < nvmecc; i++){
                err = EV_GET_REG(vmecc[i].ioadap.swin + VMECC_ERRORCAUSES);
                if( err & VMECC_ERROR_VMEBUS_PIOR ) {
			*slot = vmecc[i].ioadap.slot;
			*padap = vmecc[i].ioadap.padap;

			return err;
		}
        }

	return 0;
}
#endif /* IP21 */

/* ARGSUSED */
void
vmecc_dmadone(unchar adap)
{
}

/* ARGSUSED */
void
vmecc_aux0(unchar adap)
{
}

/* ARGSUSED */
void
vmecc_aux1(unchar adap)
{
}

void
vmecc_intr(eframe_t *ep, void *arg)
{
	int		adap = VMECC_INTRARG_ADAP((__psint_t)arg);
	int		inum = VMECC_INTRARG_INUM((__psint_t)arg);
	__psunsigned_t	swin = vmecc[adap].ioadap.swin;
	ulong		iack_vec;

	switch (inum) {
	case VMECC_INUMOTHERS:
		/* Handle DMA, AUX0 and AUX1 interrupts */
		iack_vec = EV_GET_REG(swin + VMECC_INT_REQUESTSM);
		if (iack_vec & (1 << VMECC_INUMDMAENG))
			vmecc_dmadone(adap);
		if (iack_vec & (1 << VMECC_INUMAUX0))
			vmecc_aux0(adap);
		if (iack_vec & (1 << VMECC_INUMAUX1))
			vmecc_aux1(adap);
		EV_SET_REG(swin + VMECC_INT_ENABLESET, iack_vec);
		break;

	case VMECC_INUMERROR:
		if (vmecc_error_intr(ep, (void *)&adap))
			break;
		dump_hwstate(1);
		cmn_err(CE_PANIC,"VMECC Error interrupt\n");
		break;

	case VMECC_INUMIRQ1:
	case VMECC_INUMIRQ2:
	case VMECC_INUMIRQ3:
	case VMECC_INUMIRQ4:
	case VMECC_INUMIRQ5:
	case VMECC_INUMIRQ6:
	case VMECC_INUMIRQ7:
		iack_vec = EV_GET_REG(swin + VMECC_IACK(inum));

#if defined(ULI) /* fix for bug #690661 */
		vme_handler (ep, iack_vec, inum, swin, vmecc_itoxadap(adap));
#else
		if (io4ia_war && 
			vme_ivec[VME_VEC(adap, iack_vec)].vm_ivec_ioflush){
			io4_flush_cache(swin);
		}
		vme_handler (ep, iack_vec, inum, vmecc_itoxadap(adap));
		EV_SET_REG(swin + VMECC_INT_ENABLESET, 1L << inum);
#endif /* defined(ULI) */
		/* Clear error register of the bogus error bit
		 * Keep a copy of the error register in case a write
		 * error had occured, and we would get a write error
		 * interrupt next 
		 */
		vmecc_prev_werror[adap] = EV_GET_REG(swin + VMECC_ERRCAUSECLR) &
						VMECC_ERROR_VMEBUS_PIOW;
		break;
	}
}

	
/******************************************************************************
 * VMECC pio_map functions, returns mapped size.
 ******************************************************************************/

/*
 * generates one map reg entry as specified in piomap->pio_iospace
 * returns the size mapped.
 */
ulong
pio_map_vmecc(int adap, int pioreg, iopaddr_t addr, ulong size, int am, int a64)
{
	ulong mapped;

	/* update the vmecc piomap register */
	EV_SET_REG((vmecc[vmecc_xtoiadap(adap)].ioadap.swin + 
			VMECC_PIOREG(pioreg)),
			am   << VMECC_PIOREG_AMSHIFT	|
			a64  << VMECC_PIOREG_A64SHIFT	|
			addr >> VMECC_PIOREG_ADDRSHIFT);


	/* return mapped size */
	mapped = VMECC_PIOREG_MAPSIZE - (addr & (VMECC_PIOREG_MAPSIZE - 1));

	return (mapped > size) ? size : mapped;
}


/*
 * VMECC pio_mapaddr functions.
 */

#define VMECC_SW_A16S_START1	0x04000
#define VMECC_SW_A16S_END1	0x08000
#define VMECC_SW_A16N_START1	0x08000
#define VMECC_SW_A16N_END1	0x0C000
#define VMECC_SW_A16S_START2	0x0C000
#define VMECC_SW_A16S_END2	0x10000
#define VMECC_LW_A16N_START2	0x10000
#define VMECC_LW_A16N_END2	0x20000
#define VMECC_LW_A16S_START3	0x20000
#define VMECC_LW_A16S_END3	0x30000

/*
 * VMECC pio_mapfix functions
 *
 * In Everest VMECC,
 *	A16 spaces are hard-wire mapped in the swin and lwin map reg 0.
 *	A24 NP space is hard-code mapped using map reg 14.
 *	A24 SP space is hard-code mapped using map reg 15.
 *	A32 spaces are dynamically mapped using the rest map regs.
 *      register 13 is reserved for non-fixed mappings, only.
 */

#define A24START	0x800000
#define VALID_VMEA16(addr,size)       ((addr+size)<=0x10000)
#define VALID_VMEA24(addr,size)  (((addr+size)<=0x1000000)&&(addr>=A24START))

int
pio_mapfix_a16n(piomap_t *piomap)
{
	iopaddr_t	 iopaddr = piomap->pio_iopaddr;
	int		size	= piomap->pio_size;
	int		adap;
	__psunsigned_t	swin;
	__psunsigned_t	lwin;

	if( !VALID_VMEA16(iopaddr,size) )
		return 1;

	piomap->pio_flag = PIOMAP_FIXED;

	adap = vmecc_xtoiadap(piomap->pio_adap);
	swin	= vmecc[adap].ioadap.swin;
	lwin	= vmecc[adap].a16kv;

	/* A16N 8000-BFFF mapped to small window 8000-bFFF */
	if (iopaddr >= 0x8000 && (iopaddr + size) <= 0xC000)
		piomap->pio_vaddr = 
		  (caddr_t)(swin + VMECC_SW_A16N_START1 + iopaddr - 0x8000);
	else
	/* A16N 0000-FFFF mapped to large window 10000-1FFFF */
		piomap->pio_vaddr = 
			(caddr_t)(lwin + VMECC_LW_A16N_START2 + iopaddr);

	return 0;
}

int
pio_mapfix_a16s(piomap_t *piomap)
{
	iopaddr_t	 iopaddr = piomap->pio_iopaddr;
	int		  size	  = piomap->pio_size;
	int		adap;
	__psunsigned_t	swin;
	__psunsigned_t	lwin;


	if( !VALID_VMEA16(iopaddr,size) )
		return 1;

	piomap->pio_flag = PIOMAP_FIXED;

	adap = vmecc_xtoiadap(piomap->pio_adap);
	swin	= vmecc[adap].ioadap.swin;
	lwin	= vmecc[adap].a16kv;

	/* A16S 0000-3FFF mapped to small window 4000-7FFF */
	/* This next line used to read
	 * if (iopaddr >= 0x0000 && (iopaddr + size) <= 0x4000)
	 * note that iopaddr is an unsigned type, so comparing
	 * iopaddr >= 0 is a test that doesn't make sense.
	 */
	if (iopaddr <= 0x4000 && (iopaddr + size) <= 0x4000)
		piomap->pio_vaddr = 
			(caddr_t)(swin + VMECC_SW_A16S_START1 + iopaddr);

	/* A16S 8000-BFFF mapped to small window C000-FFFF */
	else if (iopaddr >= 0x8000 && (iopaddr + size) <= 0xC000)
		piomap->pio_vaddr = 
			(caddr_t)(swin + VMECC_SW_A16S_START2+(iopaddr)-0x8000);

	/* A16S 0000-FFFF mapped to large window 20000-2FFFF */
	else piomap->pio_vaddr = 
		(caddr_t)lwin + VMECC_LW_A16S_START3 + (iopaddr);

	return 0;
}

int
pio_mapfix_a24(piomap_t *piomap)
{
	pioreg_t	*reg;

	if( !VALID_VMEA24(piomap->pio_iopaddr,piomap->pio_size) )
		return 1;

	piomap->pio_flag = PIOMAP_FIXED;

	piomap->pio_reg = (piomap->pio_type == PIOMAP_A24N) 
		? VMECC_PIOREG_A24N : VMECC_PIOREG_A24S ;

	reg = vmecc[vmecc_xtoiadap(piomap->pio_adap)].pioregpool.pioregs;

	piomap->pio_vaddr =
		(caddr_t)reg[piomap->pio_reg].kvaddr + piomap->pio_iopaddr - A24START;

	return 0;
}


int
pio_mapfix_a32(piomap_t *piomap)
{
	int		i;
	pioreg_t	*reg;
	pioregpool_t	*pool;
	uint		ospl;
	iopaddr_t	iopaddr;

	iopaddr = piomap->pio_iopaddr;

	/* size can be > 8 megs for NONFIXED maps */
	if (piomap->pio_flag != PIOMAP_FIXED) {
		if( (iopaddr + piomap->pio_size) & 0xffffffff < iopaddr )
			return 1;

		piomap->pio_reg = 0;
		return 0;
	}

	/* check if offset plus size is too big */
	if ((iopaddr & (VMECC_PIOREG_MAPSIZEMASK)) + 
		piomap->pio_size > VMECC_PIOREG_MAPSIZE) {
		cmn_err(CE_NOTE, "VMECC piomap size too big 0x%x\n",
							piomap->pio_size);
		return 1;
	}

	pool = &(vmecc[vmecc_xtoiadap(piomap->pio_adap)].pioregpool);

	ospl = splock(pool->lock);

	/* check all existing fix mapped regs for overlap */
	for (i=1, reg = &pool->pioregs[1]; i <= VMECC_PIOREG_MAXFIX ;
		i++, reg++) {
		if (reg->flag	 == PIOREG_FIXED	&&
		    reg->maptype == piomap->pio_type	&&
		    reg->iopaddr == 
			(piomap->pio_iopaddr & VMECC_PIOREG_MAPADDRMASK)) {

			/* bump the registers reference count */
			reg->refcnt++;

			spunlock(pool->lock, ospl);

			/* Point the piomap at it's register */
		    	piomap->pio_reg  = i;
			piomap->pio_vaddr = (caddr_t)
				((__psunsigned_t)reg->kvaddr | 
				(iopaddr & VMECC_PIOREG_MAPSIZEMASK));

			return 0;
		}
	}

	spunlock(pool->lock, ospl);

	/* try and allocate a map */
	if( pioregpool_alloc(piomap, PIOREG_FIXED) )
		return 1;

	return 0;
}

/*ARGSUSED*/
int
pio_mapfix_a64(piomap_t *piomap)
{
	/* XXX TBD */
	return 1;
}


/*
 * pioregpool functions,
 *	alloc	- allocate a map reg, use the same one if there is one.
 *	free	- release map reg to alloc'ed/free state.
 */

int
pioregpool_alloc(piomap_t *piomap, int flag)
{
	pioregpool_t *pool;
	pioreg_t *reg;
	ulong	ospl;
	int	i;
	vmecc_t *vmeadap;
	iopaddr_t iopaddr;


	iopaddr = piomap->pio_iopaddr & VMECC_PIOREG_MAPADDRMASK;

	vmeadap = &vmecc[vmecc_xtoiadap(piomap->pio_adap)];
	pool = &(vmeadap->pioregpool);

	if( flag == PIOREG_FIXED ) {

		ospl = splock (pool->lock);

		/* check if any are available for fixed regs.
		 * we've already tried to piggyback on existing regs
		 */
		if( pool->freefix == 0 ) {
			spunlock (pool->lock, ospl);
			return 1;
		}

		/* reserve a register */
		(pool->freefix)--;
		spunlock (pool->lock, ospl);

		/* now try and find one */
		for( ;; ) {
			/* look for a non-fixed register with k2
			 * space already allocated.
			 */
			reg = &pool->pioregs[1];
			for( i = 1; i <= VMECC_PIOREG_MAXFIX ; i++, reg++ )
				if( (reg->kvaddr)&&(reg->flag != PIOREG_FIXED))
					break;

			if( i <= VMECC_PIOREG_MAXFIX ) {
				ospl = splock (pool->lock);

				/* check if it's still free */
				if( reg->flag == PIOREG_FREE )
					break;
				spunlock (pool->lock, ospl);
				continue;
			}

			/* otherwise, we have to grab one without k2 space */
			reg = &pool->pioregs[1];
			for( i = 1; i <= VMECC_PIOREG_MAXFIX ; i++, reg++ )
				if( reg->flag == PIOREG_FREE )
					break;

			if( i > VMECC_PIOREG_MAXFIX )
				continue;

			ospl = splock (pool->lock);

			/* check if it's still free */
			if( reg->flag == PIOREG_FREE )
				break;

			spunlock (pool->lock, ospl);
		}

		/* allocate space */
		if( reg->kvaddr == 0 ) {
			/* allocate k2 space for the register */
			reg->kvaddr = iospace_alloc(VMECC_LW_MAPSIZE,
				LWIN_PFN(vmeadap->window,vmeadap->ioadap.padap)
				+LWIN_PFN_OFF(i));
			if( reg->kvaddr == 0 ) {
				(pool->freefix)++;
				spunlock (pool->lock, ospl);
				return 1;
			}
		}
	}
	else {
		/* We want a NONFIXED register.  We're guaranteed to
		 * get one of these, as long as we don't starve waiting.
		 * Start with the floater since it has an iomap 
		 */
		for( i = VMECC_PIOREG_MAXPOOL ;; ) {

			reg = &pool->pioregs[i];

			/* look through all of them */
			for( ;; i++, reg++ ) {
				
				if( i > VMECC_PIOREG_MAXPOOL ) {
					i = 1;
					continue;
				}

				/* take the first thing that comes along */
				if( reg->flag == PIOREG_FREE )
					break;

				if( (reg->maptype == piomap->pio_type) &&
				    (reg->iopaddr == iopaddr) )
					break;
			}


			ospl = splock (pool->lock);

			/* check if it's still free */
			if( (reg->flag == PIOREG_FREE) ||
			  ((reg->maptype == piomap->pio_type) && 
			   (reg->iopaddr == iopaddr)) ) {
				if( reg->kvaddr )
					break;
				reg->kvaddr = iospace_alloc(VMECC_LW_MAPSIZE,
				 LWIN_PFN(vmeadap->window,vmeadap->ioadap.padap)
				 +LWIN_PFN_OFF(i));
				if( reg->kvaddr )
					break;
			}

			spunlock (pool->lock, ospl);
		}
	}

	/* we've got it. */
	if( reg->flag == PIOREG_FREE ) {
		reg->flag = flag;
		reg->maptype = piomap->pio_type;
		reg->iopaddr = iopaddr;

		/* and map it */
		pio_map_vmecc(piomap->pio_adap, i, 
			piomap->pio_iopaddr, VMECC_PIOREG_MAPSIZE,
			piomap->pio_type == PIOMAP_A32S 
				? VME_A32SAMOD : VME_A32NPAMOD, 0);
	}

	/* bump the reference count */
	reg->refcnt++;
		
	spunlock (pool->lock, ospl);

	/* calculate the virtual address of the beginning of the
	 * mapping register
	 */
	piomap->pio_reg = i;
	piomap->pio_vaddr = (caddr_t) ((__psunsigned_t)reg->kvaddr | 
		(piomap->pio_iopaddr & VMECC_PIOREG_MAPSIZEMASK));

	return 0;
}


/*
 * Called when interface function is done or unloading device driver.
 */
void
pioregpool_free(piomap_t *piomap)
{
	ulong		ospl;
	pioregpool_t	*pool;
	pioreg_t *reg;


	/* Nothing to free up for A16 or A24 piomaps */
	switch( piomap->pio_type ) {
	case VME_A16S:
	case VME_A16NP:
	case VME_A24S:
	case VME_A24NP:
		return;
	}

	pool = &(vmecc[vmecc_xtoiadap(piomap->pio_adap)].pioregpool);
	reg = &pool->pioregs[piomap->pio_reg];

	ospl = splock (pool->lock);

	if( --(reg->refcnt) == 0 ) {
		if( reg->flag == PIOREG_FIXED )
			pool->freefix++;
		reg->flag = PIOREG_FREE;
	}
	
	spunlock (pool->lock, ospl);
}


typedef int PioMapFixFunc(piomap_t*);
PioMapFixFunc *pio_mapfix_vme_func[] = {
	pio_mapfix_a16n,
	pio_mapfix_a16s,
	pio_mapfix_a24,
	pio_mapfix_a24,
	pio_mapfix_a32,
	pio_mapfix_a32,
	pio_mapfix_a64,
};

void
pio_mapfree_vme(piomap_t *piomap)
{
	if( piomap->pio_flag == PIOMAP_FIXED )
		pioregpool_free(piomap);
}

void
pio_maplock_vme(piomap_t *piomap)
{
	pioregpool_alloc(piomap, PIOREG_INUSE);
}

void
pio_mapunlock_vme(piomap_t *piomap)
{
	pioregpool_free(piomap);
}

#define VME_A64AMOD	0	/* TBD */

ulong
pio_map_vme(piomap_t *piomap, iopaddr_t addr, int size)
{
	ulong msize;
	__psunsigned_t	nkvaddr;

	switch( piomap->pio_type ) {
		case PIOMAP_A16N:
		case PIOMAP_A16S:
		case PIOMAP_A24N:
		case PIOMAP_A24S: {
			int	off;

			/* check for a truncation */
			off = addr - piomap->pio_iopaddr;
			if( off + size > piomap->pio_size )
				return piomap->pio_size - off;

			return size;
		}
		case PIOMAP_A32S:
			msize = pio_map_vmecc(piomap->pio_adap, 
				piomap->pio_reg, addr, size, VME_A32SAMOD, 0);
			/* calculate kvaddr for addr */
			nkvaddr = ((__psunsigned_t)
				vmecc[vmecc_xtoiadap(piomap->pio_adap)].pioregpool.pioregs[piomap->pio_reg].kvaddr)
				+ (addr & VMECC_PIOREG_MAPSIZEMASK);
			/* calculate new pio_vaddr to make pio_mapaddr work */
			piomap->pio_vaddr = (caddr_t) nkvaddr - 
				(addr - piomap->pio_iopaddr);
			return msize;
		case PIOMAP_A32N:
			msize = pio_map_vmecc(piomap->pio_adap, 
				piomap->pio_reg, addr, size, VME_A32NPAMOD, 0);
			/* calculate kvaddr for addr */
			nkvaddr = ((__psunsigned_t)
				vmecc[vmecc_xtoiadap(piomap->pio_adap)].pioregpool.pioregs[piomap->pio_reg].kvaddr)
				+ (addr & VMECC_PIOREG_MAPSIZEMASK);
			/* calculate new pio_vaddr to make pio_mapaddr work */
			piomap->pio_vaddr = (caddr_t) nkvaddr - 
				(addr - piomap->pio_iopaddr);
			return msize;
		case PIOMAP_A64:
			ASSERT(0);
			/* TBD */
			return pio_map_vmecc(piomap->pio_adap, 
				piomap->pio_reg, addr, size, VME_A64AMOD, 0);
	}

	/* should never get here */
	ASSERT(0);

	return 0;
}

ulong
pio_map_a32n(piomap_t *piomap, iopaddr_t addr, ulong size)
{
	return pio_map_vmecc(piomap->pio_adap, piomap->pio_reg, addr, size,
							VME_A32NPAMOD, 0);
}



int
pio_mapfix_vme(piomap_t *piomap)
{
	if( piomap->pio_adap >= MAXVMEPOS )
		return 1;

	if( vmecc_xtoiadap(piomap->pio_adap) == -1 )
		return 1;

	if( piomap->pio_type >= PIOMAP_A64 )
		return 1;

	return (pio_mapfix_vme_func[piomap->pio_type])(piomap);
}



#define VMECC_DMA_PMEM_IOSTART	0
#define VMECC_DMA_PMEM_IOEND	0x80000000
#define VMECC_DMA_A32_IOSTART	0x80000000
#define VMECC_DMA_A32_IOEND	0xF0000000
#define VMECC_DMA_A32_MAPSIZE	0x10000
#define VMECC_DMA_A24_IOSTART	0xFF000000
#define VMECC_DMA_A24_IOEND	0xFF800000
#define VMECC_DMA_A24_MAPSIZE	0x1000


ulong vmecc_a64slvmatch;
ulong vmecc_a64master;

/* these routines convert between the external rep of the vme adapter
 * number and the internal vme adapter number
 */
int
vmecc_xtoiadap(int xadap)
{
	return vmeccind[xadap];
}

int
vmecc_itoxadap(int iadap)
{
	return vmecc[iadap].xadap;
}

void *
vmecc_getregs(int iadap)
{
	return (void *)vmecc[iadap].a16kv;
}

/* allocate the VMECC DMA engine */
int
vmecc_allocdma(int iadap)
{
	ulong	ospl;
	int	alloc;
	
	/* Try to grab the DMA engine */
	ospl = splock(vmecc[iadap].dmalock);
	if( alloc = vmecc[iadap].dmaeng )
		vmecc[iadap].dmaeng = 0;
	spunlock(vmecc[iadap].dmalock, ospl);

	return alloc;
}

/* allocate the VMECC DMA engine */
void
vmecc_freedma(int iadap)
{
	ulong	ospl;
	
	/* free the DMA engine */
	ospl = splock(vmecc[iadap].dmalock);
	ASSERT(vmecc[iadap].dmaeng == 0);
	vmecc[iadap].dmaeng = 1;
	spunlock(vmecc[iadap].dmalock, ospl);
}

/*
 * This could be configured from master.d eventually.
 */


int	nvmecc = 0;

void
vmecc_preinit(void)
{
	int i;

	/* fix the mappings, this must be done even if the system
	 * doesn't have any VME busses since pio routines use the
	 * vmeccind array to determine if an adapter is valid 
	 */
	for( i = 0 ; i < 16 * 4 ; i++ )
		vmeccind[i] = -1;
}

/*************************************************************************
 * vmecc_init, called from very early io sub-system initialization code.
 *************************************************************************/

void
vmecc_init(unchar slot, unchar padap, unchar window, unchar adap, ulong fci_id)
{
	int		i;
	pioregpool_t	*pool;
	__psunsigned_t	swin;
	vmecc_t		*vmeadap;
	int		xadap;


	/* first adapter gets mapped to adapter zero */
	if( nvmecc == 0 )
	    vmeccind[0] = 0;
	else if (nvmecc >= EV_MAX_VMEADAPS) {
		arcs_printf("Too many VME adapters <Max: %d>. ",
				EV_MAX_VMEADAPS);
		arcs_printf("Ignoring VME adapter in Slot %d padap: %d\r\n", 
				slot, padap);
		return;
	}

	adap = nvmecc++;

	xadap = VME_SPTOA(slot,vmecc_io4atova(padap));

	vmeccind[xadap] = adap;
	vmeadap = &vmecc[adap];
	vmeadap->xadap		= xadap;
	vmeadap->window		= window;
	vmeadap->ioadap.slot	= slot;
	vmeadap->ioadap.padap	= padap;
	vmeadap->ioadap.type	= IO4_ADAP_VMECC;
	vmeadap->ioadap.swin	= swin = SWIN_BASE(window, padap);
	vmeadap->a16kv		= (__psunsigned_t)
				  iospace_alloc(VMECC_LW_MAPSIZE, 
				   LWIN_PFN(window, padap));
	vmeadap->mapram		= io4[window].mapram + IOA_MAPRAM_SIZE * fci_id;
	vmeadap->dmaeng		= 1;
	vmeadap->irqmask	= 0;

	initnlock(&vmeadap->rmwlock, "rmwlock");
	initnlock(&vmeadap->dmalock, "dmalock");

	/* program the config registers */
	EV_SET_REG(swin + VMECC_CONFIG,	
		(VMECC_CONFIG_VALUE | (fci_id << VMECC_CONFIG_FCIID_SH)));
	EV_SET_REG(swin + VMECC_A64SLVMATCH,	vmecc_a64slvmatch);
	EV_SET_REG(swin + VMECC_A64MASTER,	vmecc_a64master);

	/* program pio mapping register and init pio pool */
	EV_SET_REG(swin + VMECC_PIOTIMER, VMECC_PIOTIMER_VALUE);
	pool = &vmeadap->pioregpool;

	initnlock(&pool->lock, "piomapl");
	pool->freefix = VMECC_PIOREG_MAXFIX;
	pool->pioregs[0].flag = PIOREG_FIXED;	/* vmecc map reg 0 is invalid */

	for (i = 1; i <= VMECC_PIOREG_MAXPOOL ; i++) {
		/* init vmecc map regs 1-13 */
		pool->pioregs[i].flag    = PIOREG_FREE;
		pool->pioregs[i].maptype = 0;
		pool->pioregs[i].iopaddr = 0;
		pool->pioregs[i].refcnt  = 0;
		pool->pioregs[i].kvaddr  = 0;
	}

	/* get k2 space for the floater */
	pool->pioregs[VMECC_PIOREG_MAXPOOL].kvaddr = 
		iospace_alloc(VMECC_LW_MAPSIZE,
		LWIN_PFN(window,padap)+LWIN_PFN_OFF(VMECC_PIOREG_MAXPOOL));

	pool->pioregs[VMECC_PIOREG_A24N].flag    = PIOREG_FIXED;
	pool->pioregs[VMECC_PIOREG_A24N].maptype = PIOMAP_A24N;
	pool->pioregs[VMECC_PIOREG_A24N].iopaddr = 0x800000;
	pool->pioregs[VMECC_PIOREG_A24N].kvaddr = 
		iospace_alloc(VMECC_LW_MAPSIZE,
		LWIN_PFN(window,padap)+LWIN_PFN_OFF(VMECC_PIOREG_A24N));
	pio_map_vmecc(xadap, VMECC_PIOREG_A24N, 0x800000, 0x800000, 
		VME_A24NPAMOD, 0);

	pool->pioregs[VMECC_PIOREG_A24S].flag    = PIOREG_FIXED;
	pool->pioregs[VMECC_PIOREG_A24S].maptype = PIOMAP_A24S;
	pool->pioregs[VMECC_PIOREG_A24S].iopaddr = 0x800000;
	pool->pioregs[VMECC_PIOREG_A24S].kvaddr = 
		iospace_alloc(VMECC_LW_MAPSIZE,
		LWIN_PFN(window,padap)+LWIN_PFN_OFF(VMECC_PIOREG_A24S));
	pio_map_vmecc(xadap, VMECC_PIOREG_A24S, 0x800000, 0x800000, 
		VME_A24SAMOD, 0);
	
}



#define MAX_VMEIPL	8
#define VMECC_IRQERROR	1

static ulong vmecc_vecirqs[MAX_VMEIPL] = {
	VMECC_VECTORERROR,
	VMECC_VECTORIRQ1,
	VMECC_VECTORIRQ2,
	VMECC_VECTORIRQ3,
	VMECC_VECTORIRQ4,
	VMECC_VECTORIRQ5,
	VMECC_VECTORIRQ6,
	VMECC_VECTORIRQ7
};

extern char	cpuipl[];
static int	vme_ipls[MAX_VMEIPL];

int
iointr_at_cpu(int ipl, int cpu, int adap)
{
	int irqmask;
	int iadap;
	__psunsigned_t	swin;

	/* validate the adap */
	if( (iadap = VMEADAP_XTOI(adap)) == -1 )
		return -1;

	/* validate the level */
	if( (ipl < 1) || (ipl > 7) )
		return -1;

	irqmask = 1 << ipl;

	/* see if it's already been enabled. */
	if( vmecc[iadap].irqmask & irqmask )
		return vme_ipls[ipl];

	/* force over to IPL directive value */
	if( (cpuipl[ipl] != 255) && (cpu != cpuipl[ipl]) )
		cpu = cpuipl[ipl];

	/* make sure it's valid */
	if( !cpu_isvalid(cpu) )
		cpu = masterpda->p_cpuid;

	vme_ipls[ipl] = cpu;

	swin = vmecc[iadap].ioadap.swin;

	evintr_connect((evreg_t *)(swin + vmecc_vecirqs[ipl]),
			VME_INTR_LEVEL(iadap,nvmecc,ipl),
			SPLDEV,
			cpu,
			vmecc_intr,
			(void *)(__psint_t)VMECC_INTRARG(iadap,ipl));

	/* turn on interrupts */
	EV_SET_REG(swin + VMECC_INT_ENABLESET, irqmask);

	vmecc[iadap].irqmask |= irqmask;

	return cpu;
}

#ifdef ULI
int
vme_ipl_to_cpu(int ipl)
{
    return(vme_ipls[ipl]);
}
#endif

extern int nvme32_dma;
void
vmecc_lateinit(void)
{
	int i, j;
	ulong		kpaddr;
	evreg_t		*curr;
	vmecc_t		*vmeadap;
	__psunsigned_t	swin;
	int		vme32_dma;


	for( i = 0 ; i < MAX_VMEIPL ; i++ )
		vme_ipls[i] = -1;

	/* add the mapped bus 0 to the inventory */
	if( nvmecc )
		add_to_inventory(INV_BUS,INV_BUS_VME,0,0,vmecc[0].xadap);

	for( i = 0 ; i < nvmecc ; i++ ) {
		vmeadap = &vmecc[i];
		swin = vmeadap->ioadap.swin;

		/* program the interrupt registers and connect the handler */
		evintr_connect((evreg_t *)(swin + VMECC_VECTORERROR),
				EVINTR_LEVEL_VMECC_ERROR,
				SPLDEV,
				EVINTR_DEST_VMECC_ERROR,
				vmecc_intr,
				(void *)(__psint_t)VMECC_INTRARG(i,VMECC_INUMERROR));

		EV_SET_REG(swin + VMECC_INT_ENABLESET, VMECC_IRQERROR);
		vmeadap->irqmask |= VMECC_IRQERROR;

		for( j = 1 ; j < MAX_VMEIPL ; j++ )
			if( cpuipl[j] != 255 )
			    (void)iointr_at_cpu(j,cpuipl[j],VMEADAP_ITOX(i));

		/* 1-level direct map to phys mem */
		for (kpaddr = VMECC_DMA_PMEM_IOSTART, 
	 	     curr = (evreg_t*)vmeadap->mapram;
		     kpaddr < VMECC_DMA_PMEM_IOEND;
		     kpaddr += IAMAP_L1_BLOCKSIZE,    curr++) {
			EV_SET_REG(curr, IAMAP_L1_ENTRY(kpaddr));
		}

		/*
		 * VME_DMA_TBL = (nvme32_dma * MBYTE * bytes_per_entry)/IO_NBPP
		 */
#define	VME_DMA_TBL(x)	(x*1024*1024*4)/(IO_NBPP)
#define	VME_MAX_DMA	512	/* Max 512 Mbytes of Simultaneous mapping */

		if (nvme32_dma < 32)
			nvme32_dma = 32;
		else if (nvme32_dma > VME_MAX_DMA )
			nvme32_dma = VME_MAX_DMA;

		/* Make sure it's a power of 2 */
		if (nvme32_dma & (nvme32_dma - 1)){
			/* Nope, fix it to be a power of 2 */
			int x = nvme32_dma + 1;
			while (x & (x - 1))
				x++;
			nvme32_dma = x;
		}
		/* init a32 and a24 dma maps */
		vme32_dma = (nvme32_dma ? VME_DMA_TBL(nvme32_dma) :
				VMECC_DMA_A32_MAPSIZE);
		iamap_init(&vmeadap->a32map, vmeadap->mapram, 
			vme32_dma, VMECC_DMA_A32_IOSTART, 
			VMECC_DMA_A32_IOEND);

		iamap_init(&vmeadap->a24map, vmeadap->mapram, 
			VMECC_DMA_A24_MAPSIZE, VMECC_DMA_A24_IOSTART, 
			VMECC_DMA_A24_IOEND);
	}
	for (i=1; i < VMECC_IRQMAX; i++)
		vmespl_lvl[i] = VME_INTR_LEVEL(nvmecc, nvmecc, i);
}



/*
 * kv addr -> valid vme address?
 * All the other ones will not be implemented because of the high cost to
 * each swin vme space and lwin space ...
 */
/* ARGSUSED */
int
is_vme_space(void *addr, int junk)
{
	return (vme_adapter(addr) >= 0);
}

/* return the register the address would be found in, 1-15 */
int 
vmecc_register(void *addr, int adap)
{
	pioreg_t *reg;
	__psunsigned_t lwin;
	int i;
	
	reg = &vmecc[adap].pioregpool.pioregs[1];
	for( i = 1 ; i < VMECC_PIOREG_MAX ; i++, reg++ ) {

		/* only applies to those that are free. */
		if( reg->flag == PIOREG_FREE )
			continue;

		lwin = (__psunsigned_t)reg->kvaddr;
		if((__psunsigned_t)addr>=lwin &&
		   (__psunsigned_t)addr<(lwin + VMECC_LW_MAPSIZE))
			return i;
	}

	return 0;
}

/*
 * kv address -> vme adapter id
 */
int
vme_adapter(void *addr)
{
	int adap;
	__psunsigned_t swin;

	for (adap = 0; adap < nvmecc; adap++) {
		/* check small window a16 space */
		swin = vmecc[adap].ioadap.swin;
		if((__psunsigned_t)addr>=swin &&
		   (__psunsigned_t)addr<(swin + SWIN_SIZE))
			return adap;

		/* then check large window a16 space */
		swin = vmecc[adap].a16kv;
		if((__psunsigned_t)addr>=swin && (__psunsigned_t)addr<(swin + VMECC_LW_MAPSIZE))
			return adap;

		if( vmecc_register(addr,adap) )
			return adap;
	}

	return -1;
}


#ifndef vme_to_phys
/*
 * Given vme adapter address, find kv addr.
 * The current interface does not work because no adapter information is given.
 * Either the edt or adapter itself must be passed.
 *
 * NOT USEFUL.
 */
ulong
vme_to_phys(unsigned long addr, int type, unchar adap)
{
}
#endif

/*
 * Given kv addr to vme space, find system (Ebus) physical page number.
 * It used to return the physical address in a unsigned long.
 * Everest version returns tlb pfn which fits in unsigned long.
 */

ulong
vme_virtopfn(void *addr)
{
	unchar		adap = vme_adapter(addr);
	__psunsigned_t	swin = vmecc[adap].ioadap.swin;
	__psunsigned_t	lwin = vmecc[adap].a16kv;


	/* in small window, convert to large window */
	if ((__psunsigned_t)addr >= swin &&
	    (__psunsigned_t)addr <= swin + SWIN_SIZE)
		addr = (void *)((ulong)addr - swin + lwin);

#if _MIPS_SIM != _ABI64
	ASSERT((__psunsigned_t )addr >= IOMAP_BASE);
#endif

	/* must be in large window thus in iotlb */
	return io4_virtopfn(addr);
}


vmeam_t vmeam[7] = {
	{ VME_A16NPAMOD,	"A16N"	},
	{ VME_A16SAMOD,		"A16S"	},
	{ VME_A24NPAMOD,	"A24N"	},
	{ VME_A24SAMOD,		"A24S"	},
	{ VME_A32NPAMOD,	"A32N"	},
	{ VME_A32SAMOD,		"A32S"	}
};


/*
 * kv address -> vme adapter, address, space.
 * Used by vmecc_rmw_addr only.
 */
int
vme_address(caddr_t arg_addr, int *adap, ulong *vmeaddr, ulong *vmespace)
{
	__psunsigned_t	swin;
	__psunsigned_t	lwin;
	int		regid;
	pioreg_t	*reg;
	__psunsigned_t addr = (__psunsigned_t)arg_addr;

	*adap = vme_adapter((ulong*)addr);
	swin  = vmecc[*adap].ioadap.swin;

	/* in small window 4000-7fff */
	if (addr >= (swin+VMECC_SW_A16S_START1) && 
		addr < (swin+VMECC_SW_A16S_END1)) {
		*vmeaddr  = addr - (swin+VMECC_SW_A16S_START1);
		*vmespace = VME_A16S;
		return 1;
	}

	/* in small window 8000-Bfff */
	if (addr >= (swin+VMECC_SW_A16N_START1) && 
		addr < (swin+VMECC_SW_A16N_END1)) {
		*vmeaddr  = addr - (swin+VMECC_SW_A16N_START1) + 
			VMECC_SW_A16N_START1;
		*vmespace = VME_A16NP;
		return 1;
	}

	/* in small window C000-Ffff */
	if (addr >= (swin+VMECC_SW_A16S_START2) && 
		addr < (swin+VMECC_SW_A16S_END2)) {
		*vmeaddr  = addr - (swin+VMECC_SW_A16S_START2) + 
			VMECC_SW_A16S_START2;
		*vmespace = VME_A16S;
		return 1;
	}

	lwin = vmecc[*adap].a16kv;

	/* in large window 10000-1FFFF */
	if (addr >= (lwin+VMECC_SW_A16S_END2) && 
		addr < (lwin+VMECC_LW_A16N_END2)) {
		*vmeaddr  = addr - (lwin+VMECC_LW_A16N_END2);
		*vmespace = VME_A16NP;
		return 1;
	}

	/* in large window 20000-2FFFF */
	if (addr >= (lwin+VMECC_LW_A16S_START3) && 
		addr < (lwin+VMECC_LW_A16S_END3)) {
		*vmeaddr  = addr - (lwin+VMECC_LW_A16S_START3);
		*vmespace = VME_A16S;
		return 1;
	}

	/*
	 * Above are hard-wired, the rest are programmable.
	 * Note that the last 2 regs are hard-coded for A24 since the mapreg
	 * struct is updated to reflect that just treat that as others.
	 */
	if( (regid = vmecc_register((void *)addr,*adap)) == 0 )
		return 0;

	ASSERT(regid >= 1 && regid < VMECC_PIOREG_MAX);
	reg = &vmecc[*adap].pioregpool.pioregs[regid];

	/* vmecc_register should catch this, but what the heck */
	if (reg->flag == PIOREG_FREE)
		return 0;

	*vmeaddr  = reg->iopaddr + (addr & VMECC_PIOREG_MAPSIZEMASK);
	*vmespace = reg->maptype;
	return 1;
}

/* VME support for Frame Scheduler.
 * Frame scheduler supports framing events (interrupts ) to be
 * generated by different sources, one of them being a controller on
 * VME. In such cases, VME interrupt should result in a "group" interrupt
 * to all the frame scheduling CPUs.
 * vme_frs_install() and vme_frs_uninstall() provide a way to install
 * and uninstall the frame scheduler group interrupts.
 */

/*
 * vme_frs_install:
 *      Install frame scheduler group interrupt target for the given
 *      VME interrupt request level.
 *      Returns :
 *	      If succesful, returns a cookie(>0) to be passed to uninstall
 *	      -1 if it failed.
 */
#define FRS_IRQENABLED  0x100   /* VME IRQ Was enabled before FRS */
#define FRS_IRQENBLMSK  0xFF    /* VME IRQ Was enabled before FRS */


int
vme_frs_install(int vmeadap, int vmeipl, intrgroup_t *intrgroup)
{
	int     irqmask;
	int     iadap;
	int     ret_val;
	ulong   swin;

	/* validate the adap */
	if( (iadap = VMEADAP_XTOI(vmeadap)) == -1 )
		return -1;

	/* validate the level */
	if( (vmeipl < 1) || (vmeipl > 7) )
		return -1;

	irqmask = 1 << vmeipl;

	/* Save Old cpu to which this IPL was assigned */
	ret_val = vme_ipls[vmeipl];
	vme_ipls[vmeipl] = intrgroup->groupid;

	swin = vmecc[iadap].ioadap.swin;

	evintr_connect((evreg_t *)(swin + vmecc_vecirqs[vmeipl]),
			VME_INTR_LEVEL(iadap,nvmecc,vmeipl),
			SPLDEV,
			/* Adding EV_MAX_CPUS indicates it's a Group intr */
			EV_MAX_CPUS + intrgroup->groupid,
			vmecc_intr,
			(void *)(__psint_t)VMECC_INTRARG(iadap,vmeipl));

	/* If IRQ was enabled earlier, indicate so via FRS_IRQENABLED */
	ret_val |= ( vmecc[iadap].irqmask & irqmask) ? (FRS_IRQENABLED) : 0;

	/* Enable interrupts, if not on already */
	if ((vmecc[iadap].irqmask & irqmask) == 0){
		EV_SET_REG(swin + VMECC_INT_ENABLESET, irqmask);
		vmecc[iadap].irqmask |= irqmask;
	}

	return ret_val;
}

int
vme_frs_uninstall(int vmeadap, int vmeipl, int vmefrs_val)
{
	int     irqmask;
	int     iadap;
	int     prev_gid, prev_cpu;
	ulong   swin;

	/* validate the adap */
	if( (iadap = VMEADAP_XTOI(vmeadap)) == -1 )
		return -1;

	/* validate the level */
	if( (vmeipl < 1) || (vmeipl > 7) )
		return -1;

	prev_cpu = vmefrs_val  & FRS_IRQENBLMSK;
	prev_gid = vme_ipls[vmeipl];
	vme_ipls[vmeipl] = prev_cpu;

	irqmask = 1 << vmeipl;
	swin    = vmecc[iadap].ioadap.swin;
	if( cpu_isvalid(prev_cpu) && (vmefrs_val & FRS_IRQENABLED)){

		evintr_connect((evreg_t *)(swin + vmecc_vecirqs[vmeipl]),
			VME_INTR_LEVEL(iadap,nvmecc,vmeipl),
			SPLDEV,
			prev_cpu,
			vmecc_intr,
			(void *)(__psint_t)VMECC_INTRARG(iadap,vmeipl));

		/* turn on interrupts */
		EV_SET_REG(swin + VMECC_INT_ENABLESET, irqmask);

		vmecc[iadap].irqmask |= irqmask;
	}
	else {
		EV_SET_REG(swin + VMECC_INT_ENABLECLR, irqmask);
		vmecc[iadap].irqmask &= ~irqmask;
	}

	return prev_gid;
}



/*
 * Everything about VME RMW operation.
 */

#define RMW_BYTE	1
#define RMW_HALF	2
#define RMW_WORD	3

#define RMW_ORMASK	0xFFFFFFFF
#define RMW_ANDSET	0x00000000

/* 
 * NOTE:
 * VMECC RMW operation...
 *
 * 1. VMECC does RMW operation using a set of 5 registers.
 * 2. Size of operation is determined by the type of read at trigger 
 *    location.
 * 3. The three least significant bits of value in RMWADDR location
 *    should match the trigger address.
 * 4. VMECC requires mask and set values to be duplicated in lower
 *    two bytes for byte operation. (To take care of even/odd byte RMW op)
 * 5. vmecc_rmw will and the data read from 'address' with 'mask'
 *    and OR the resultant data with 'set' before writing back to 
 *    'address' on VME bus.
 */

void
vmecc_rmw(unchar adap, ulong addr, int vmespace, ulong mask, ulong set, ulong type)
{
	__psunsigned_t	swin = vmecc[adap].ioadap.swin;
	ulong		ospl = splock(vmecc[adap].rmwlock);
	register ulong lowbit= (addr & 7);

	EV_SET_REG(swin + VMECC_RMWMASK, mask);
	EV_SET_REG(swin + VMECC_RMWSET,  set);
	EV_SET_REG(swin + VMECC_RMWADDR, addr);
	EV_SET_REG(swin + VMECC_RMWAM,	 vmeam[vmespace].am);

	if (type == RMW_BYTE)
		*(volatile char*)(swin + VMECC_RMWTRIG + lowbit);
	else if (type == RMW_HALF){
		ASSERT(!(addr & 1));	/* Half byte alignment */
		*(volatile short*)(swin + VMECC_RMWTRIG + lowbit);
	}
	else{
		ASSERT(!(addr & 3));	/* Word alignment */
		*(volatile uint *)(swin + VMECC_RMWTRIG + lowbit);
	}

	spunlock(vmecc[adap].rmwlock, ospl);
}


void
pio_orb_rmw(piomap_t *piomap, iopaddr_t iopaddr, unchar set)
{
	ulong pio_set = set | (set << 8);
	vmecc_rmw(piomap->pio_adap, iopaddr, piomap->pio_type,
				RMW_ORMASK, pio_set, RMW_BYTE);
}

void
pio_orh_rmw(piomap_t *piomap, iopaddr_t iopaddr, ushort set)
{
	vmecc_rmw(piomap->pio_adap, iopaddr, piomap->pio_type,
					RMW_ORMASK, (ulong)set, RMW_HALF);
}

void
pio_orw_rmw(piomap_t *piomap, iopaddr_t iopaddr, ulong set)
{
	vmecc_rmw(piomap->pio_adap, iopaddr, piomap->pio_type,
					RMW_ORMASK, (ulong)set, RMW_WORD);
}

void
pio_andb_rmw(piomap_t *piomap, iopaddr_t iopaddr, unchar mask)
{
	ulong	pio_mask = (mask << 8) | mask;
	vmecc_rmw(piomap->pio_adap, iopaddr, piomap->pio_type,
					pio_mask, RMW_ANDSET, RMW_BYTE);
}

void
pio_andh_rmw(piomap_t *piomap, iopaddr_t iopaddr, ushort mask)
{
	vmecc_rmw(piomap->pio_adap, iopaddr, piomap->pio_type,
					(ulong)mask, RMW_ANDSET, RMW_HALF);
}

void
pio_andw_rmw(piomap_t *piomap, iopaddr_t iopaddr, ulong mask)
{
	vmecc_rmw(piomap->pio_adap, iopaddr, piomap->pio_type,
					(ulong)mask, RMW_ANDSET, RMW_WORD);
}






/*
 * The following are written to be compatible with older interfaces.
 */

static void
vmecc_rmw_addr(caddr_t addr, ulong mask, ulong set, ulong type)
{
	int	vmeadap;
	ulong	vmeaddr;
	ulong	vmespace;

	if (vme_address(addr, &vmeadap, &vmeaddr, &vmespace) == 0) {
		ASSERT(0);
		cmn_err(CE_WARN, "RMW op to non-VME address 0x%x, ignored\n", 
						addr);
	}

	vmecc_rmw((unchar)vmeadap, vmeaddr, vmespace, mask, set, type);
}
	
void
orb_rmw(volatile void *addr, uint set)
{
	vmecc_rmw_addr((caddr_t)addr, RMW_ORMASK, (set|(set<<8)), RMW_BYTE);
}

void
orh_rmw(volatile void *addr, uint set)
{
	vmecc_rmw_addr((caddr_t)addr, RMW_ORMASK, set, RMW_HALF);
}

void
orw_rmw(volatile void *addr, uint set)
{
	vmecc_rmw_addr((caddr_t)addr, RMW_ORMASK, set, RMW_WORD);
}

void
andb_rmw(volatile void *addr, uint mask)
{
	vmecc_rmw_addr((caddr_t)addr, (mask | (mask << 8)), RMW_ANDSET, RMW_BYTE);
}

void
andh_rmw(volatile void *addr, uint mask)
{
	vmecc_rmw_addr((caddr_t)addr, mask, RMW_ANDSET, RMW_HALF);
}

void
andw_rmw(volatile void *addr, uint mask)
{
	vmecc_rmw_addr((caddr_t)addr, mask, RMW_ANDSET, RMW_WORD);
}

#ifdef  GROUPINTR_DEBUG
/* XXXXXXXXXXXXXXXXXXXX CODE FOR TESTING GROUP INTERRUPTS XXXXXXXXXXXXX */
/* Needs a syssgi() call, which invokes vme_test_groupintr(), and a user
 * level program which makes a syssgi() call with appropriate value.
 * Targets the group interrupts to CPUs 1 and 2.
 */
int     vmeadap = 0;
int     grpirq  = 1;
int     groupintr_count[EV_MAX_CPUS];
int     grintrcount= 10;
int     grpintr_delay = 5000;

/*ARGSUSED*/
int
vme_groupintr(int arg)
{
	/* Bump a count for the CPU which received the interrupt */
	groupintr_count[cpuid()]++;
	return 0;
}
void
vme_test_groupintr()
{
	intrgroup_t     *ig;
	int	     x, i;
	int	     vec;
	__psunsigned_t  swin = vmecc[vmeadap].ioadap.swin;


	if (maxcpus < 2 )       /* Need atleast 3 cpus */
		return;
	if ((vec = vme_ivec_alloc(vmeadap)) == 255)
		return;

	if (vme_ivec_set(vmeadap, vec, vme_groupintr, 0) < 0){
		vme_ivec_free(vmeadap, vec);
		return;
	}
	ig = intrgroup_alloc();
	if (!ig){
		vme_ivec_free(vmeadap, vec);
		cmn_err(CE_CONT, "Cannot allocate a group interrupt..\n");
		return;
	}

	intrgroup_join(ig, 1);
	intrgroup_join(ig, 2);
	x = vme_frs_install(vmeadap, grpirq, ig);

	/* Now trigger a VME interrupt 'grintrcount' times */
	for(i = 0; i < grintrcount; i++){
		EV_SET_REG(swin + VMECC_IACK(grpirq), vec);
		EV_SET_REG(swin+VMECC_INT_REQUESTSM, ( 1 << grpirq));
		us_delay(grpintr_delay);
	}

	vme_frs_uninstall(vmeadap, grpirq, x);
	intrgroup_unjoin(ig, 2);
	intrgroup_unjoin(ig, 1);
	intrgroup_free(ig);
	vme_ivec_free(vmeadap, vec);

	return;
}
#endif  /* GROUPINTR_DEBUG */

