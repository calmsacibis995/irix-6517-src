/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1994, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.72 $"

#include <sys/types.h>
#include <sys/reg.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/iotlb.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/var.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/runq.h>
#include <sys/vmereg.h>
#include <sys/atomic_ops.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/gda.h>
#include <ksys/as.h>

extern int buserror_addr(caddr_t);

#if IP21
int probe_in_progress = 0;
int probe_cpu = -1;
#endif

/*
 * xxx_check_badaddr() should only be called by boot_master which does the
 * badaddr probing, it does the following,
 *	1. call everest_error_handler() if there are errors that could not be
 *	   caused by badaddr.
 *	2. clear the error and return 1 if there is error caused by badaddr().
 *	3. return 0.
 *
 */
#if	IP25
/*
 * For IP25, it appears the ME bit is set with the multiple parity 
 * error bits, so we allow for it here. It also turns out that 
 * ADDR_HERE may be set because of a speculative load.
 */
#define CC_ERROR_BADADDR	(CC_ERROR_PARITY_D|/* bad parity data */\
				 CC_ERROR_MYREQ_TIMEOUT| /* No responder */ \
				 CC_ERROR_MYRES_TIMEOUT| /* IA drops PIO */ \
				 CC_ERROR_MY_ADDR|\
				 IP25_CC_ERROR_ME)
#else
#define CC_ERROR_BADADDR	(CC_ERROR_PARITY_D|/* bad parity data */\
				 CC_ERROR_MYREQ_TIMEOUT| /* No responder */ \
				 CC_ERROR_MYRES_TIMEOUT /* IA drops PIO */)
#endif

int
cc_check_badaddr(void)
{
	cpuerror_t *ce = &(EVERROR->cpu[cpuid()]);

	if (cc_error_log() == 0)
		return 0;

	if (ce->cc_ertoip & ~CC_ERROR_BADADDR) {
		everest_error_handler((eframe_t *)0, 0);
		/* will not come back if panic */
	}

	if (ce->cc_ertoip & CC_ERROR_BADADDR) {
		cc_error_clear(0);
#if TFP
		tfp_clear_gparity_error();
		if( cpuid() == 1 )
			printf("ccbad\n");
#endif
		return 1;
	/*
	 * One thing to be noted is A-chip should turn on ADDR_HERE timeout
	 * error bit for this cpu, but we will not check/clear a-chip error
	 * since we should always get cc my request timeout error interrupt.
	 * And more than that,
	 *	1. would have required some spinlock protection,
	 *	2. can not clear individual bit,
	 */
	}

	return 0;
}


#define IO4_EBUSERROR_BADADDR	(IO4_EBUSERROR_MY_DATA_ERR|IO4_EBUSERROR_BADIOA|IO4_EBUSERROR_STICKY|IO4_EBUSERROR_ADDR_ERR|IO4_EBUSERROR_MY_ADDR_ERR)

#define IO4_IBUSERROR_BADADDR	(IO4_IBUSERROR_PIORESPDATA|IO4_IBUSERROR_STICKY)

#define IO4_EBUSERROR_FATAL     (IO4_EBUSERROR_INVDIRTYCACHE|IO4_EBUSERROR_TRANTIMEOUT)

/* Ibus and Ebus error registers on IO4 have bits other than error bits.
 * These two macros define the mask for error bits
 */
#define IO4_EBUSERROR_MASK	0x3ff
#define IO4_IBUSERROR_MASK	0xffff


int
io4_check_badaddr(int slot, int window)
{
	io4error_t *ie = &(EVERROR->io4[window]);
	int              retval = 0;

	io4_error_log(slot);

        /* Check if integrety of the system is at question */

        if ((ie->ebus_error & IO4_EBUSERROR_MASK) & IO4_EBUSERROR_FATAL)
                everest_error_handler((eframe_t *)0, 0);

        /* Check if the badaddr failed needs to be returned */
        /* Any error bits set in Ebus/Ibus error register will
         * make baddaddr fail
         */
        if ((ie->ebus_error & IO4_EBUSERROR_MASK) ||
            (ie->ibus_error & IO4_IBUSERROR_MASK))
                retval = 1;

        io4_error_clear(slot);

        return retval;

#ifdef	OLD_CODE

	if ((ie->ebus_error  & IO4_EBUSERROR_MASK) & ~IO4_EBUSERROR_BADADDR ||
	    ((ie->ibus_error& IO4_IBUSERROR_MASK) & ~IO4_IBUSERROR_BADADDR)) {
		everest_error_handler((eframe_t *)0, 0);
	}

	if (ie->ebus_error & IO4_EBUSERROR_BADADDR ||
	    ((ie->ibus_error & IO4_IBUSERROR_MASK) & IO4_IBUSERROR_BADADDR)) {
		io4_error_clear(slot);
		return 1;
	}

	return 0;
#endif	/* OLD_CODE */
}

#define	VMECC_ERROR_REV1BUG	VMECC_ERROR_FCIDB_TO  /* Due to Bug in Rev1 */
#define VMECC_ERROR_BADADDRS (VMECC_ERROR_VMEBUS_PIOW | VMECC_ERROR_VMEBUS_PIOR | VMECC_ERROR_REV1BUG | VMECC_ERROR_OVERRUN)


int
vmecc_check_badaddr(int slot, int vadap, int padap)
{
	vmeccerror_t *ve = &(EVERROR->vmecc[vadap]);

	vmecc_error_log(slot, padap);

	if (ve->error & ~VMECC_ERROR_BADADDRS) {
		everest_error_handler((eframe_t *)0, 0);
	}

	if (ve->error & VMECC_ERROR_BADADDRS) {
		vmecc_error_clear(slot, padap);
		fchip_error_clear(slot, padap);
		return 1;
	}

	return 0;
}

/* not that I think we'll ever use badaddr() on a HIPPI card */
int
io4hip_check_badaddr(int slot, int vadap, int padap)
{
	io4hiperror_t *he = &(EVERROR->io4hip[vadap]);

	io4hip_error_log(slot, padap);

	if (he->error & ~VMECC_ERROR_BADADDRS) {
		everest_error_handler((eframe_t *)0, 0);
	}

	if (he->error & VMECC_ERROR_BADADDRS) {
		io4hip_error_clear(slot, padap);
		fchip_error_clear(slot, padap);
		return 1;
	}

	return 0;
}


/*
 * To check addr in large window space.
 * If address in kernel virtual space, get the pfn via kptbl.
 * Otherwise, get the pfn via iotlb.
 */
int
large_window(caddr_t addr, int *window, int *padap, int *woffset)
{
	int pfn;

#if  _MIPS_SIM != _ABI64
	if (addr < (caddr_t)IOMAP_BASE)
		return 0;
#endif

	if (iskvir(addr)){
		pde_t	*vmepde;
		vmepde = kvtokptbl(addr);
		/* Should be an uncached pte */
		if (pte_iscached(vmepde->pgi))
			return	0; 
		pfn = pdetopfn(vmepde);	/* Get pfn bsaed on system page size */
		/* Translate the pfn based on system page size to 
		 * one based on io page size 
		 * Macros LWIN_WINDOW and others expect pfn to be
		 * based on IO page size. 
		 */
		pfn = io_btoct((unsigned long)pfn << PNUMSHFT);
	}
#if _MIPS_SIM != _ABI64
	else{
		/* It's perhaps a true large window pte */
		if ((int)IOMAP_VPAGEINDEX(addr) >= iotlb_next)
			return 0;
		/* Refer to io4.c for details about the iotlb structure */
		pfn = (*(unsigned *)&iotlb[(int)IOMAP_VPAGEINDEX(addr)]) >> 6;
	}
#else /* 64-bit kernel addressing */
	else {
		/* True K1 Seg address is expected here. */
		pfn = io_btoct(addr);
		if (pfn < LWIN_PFN_BASE)
			return 0;
	}
#endif
	*window	= LWIN_WINDOW(pfn);
	*padap	= LWIN_PADAP(pfn);
	*woffset= (__psunsigned_t)addr & (LWIN_SIZE - 1);

	return 1;
}


/*
 * To chaeck addr in small window space or not.
 */
int
small_window(caddr_t addr, int *window, int *padap, int *woffset)
{
	if (addr >= (caddr_t)SWIN_BASE(0, 0) &&
	    addr <  (caddr_t)SWIN_BASE(EV_MAX_IO4S-1, IO4_MAX_PADAPS-1)) {
		*window	= SWIN_REGION(addr);
		*padap	= SWIN_PADAP(addr);
		*woffset= (__psunsigned_t)addr & (SWIN_SIZE - 1);
		return 1;
	} else
		return 0;
}




/*
 * Badaddr probing can be initiated from any cpu. A probe info structure is
 * added to the list and  the process is migrated to the bootmaster.
 * A mutex probe_sema is used to protect the shared data structure.
 *
 * Error interrupts will be masked and those error interrupts caused by probe
 * will be cleared. Other error interrupts can only be seen after the probe.
 */

#define PROBE_BADADDR		0
#define PROBE_WBADADDR		1
#define PROBE_BADADDR_VAL	2
#define PROBE_WBADADDR_VAL	3

typedef struct {
	int		type;
	int		len;
	volatile void	*addr;
	volatile void	*ptr;
	int		value;
} probe_info_t;

static int probe_common(probe_info_t *);
mutex_t		probe_sema;


int
badaddr(volatile void* addr, int len)
{
	probe_info_t	pi;

	pi.type  = PROBE_BADADDR;
	pi.addr  = addr;
	pi.len   = len;
	pi.ptr   = &pi.value;

	return probe_common(&pi);

}

int
badaddr_val(volatile void *addr, int len, volatile void *ptr)
{
	probe_info_t	pi;

	pi.type  = PROBE_BADADDR_VAL;
	pi.addr  = addr;
	pi.len   = len;
	pi.ptr   = (void *)ptr;
	return probe_common(&pi);
}

int
wbadaddr(volatile void *addr, int len)
{
	probe_info_t	pi;

	pi.type  = PROBE_WBADADDR;
	pi.addr  = addr;
	pi.len   = len;
	pi.ptr   = &pi.value;
	pi.value = 0;
	return probe_common(&pi);
}

int
wbadaddr_val(volatile void *addr, int len, volatile void *value)
{
	probe_info_t	pi;

	pi.type  = PROBE_WBADADDR;
	pi.addr  = addr;
	pi.len   = len;
	pi.ptr   = (void *)value;
	return probe_common(&pi);
}

/*
 * These two are not needed since EVEREST does not use them.
 *
	earlybadaddr()	{}
	wbadmemaddr()	{}
 */

#if IP25
extern	void	cc_clear_all_cpus_error(__uint64_t);
#endif

/*
 * To see any error caused by failed probing.
 */
int
is_buserror(volatile void *addr)
{
	int	window;
	int	padap;
	int	woffset;
	int	slot;
	int	retval = 0;
	int	vadap;
	evbrdinfo_t	*brd;

#ifdef IP25
	cc_clear_all_cpus_error((__uint64_t)IP25_CC_ERROR_MY_DATA);
#endif
	/* Clear any MC3 errors which may be set due to Probing */
	for (slot=1; slot < EV_MAX_SLOTS; slot++){
		brd = &EVCFGINFO->ecfg_board[slot];

		if (brd->eb_enabled && (brd->eb_type == EVTYPE_MC3))
			mc3_error_clear(slot);
	}

	if (small_window((caddr_t)addr, &window, &padap, &woffset) ||
	    large_window((caddr_t)addr, &window, &padap, &woffset)) {

		/* address is in io space */
		slot = io4[window].slot;
		retval = io4_check_badaddr(slot, window);
		retval = cc_check_badaddr() || retval;
		vadap = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;

		ASSERT(window == EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum);

		switch (EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_type) {
		case IO4_ADAP_VMECC:
			return vmecc_check_badaddr(slot, vadap, padap)|| retval;
		case IO4_ADAP_HIPPI:
			return io4hip_check_badaddr(slot,vadap,padap) || retval;
		default:
			return retval;
		}
	}

	/*
	 * The only other case that could be caused by bad addr is
	 * address error interrupt from CC.
	 */
	return cc_check_badaddr();

	/*
	 * All xxx_check_badaddr() above would check the error register and the
	 * CC interrupt pending register for errors caused by failed probe.
	 * everest_error_handler() is called if there is any error unrelated
	 * to probing.
	 */
}


int ev_wbad_inprog;
int ev_wbad_val;

/*
 * This function actually does the probe operation and checks for buserror.
 * It is equivalent to badaddr et. al. for previous platforms.
 */
extern int	_badaddr_val(volatile void *, int, volatile int *);
extern int	_wbadaddr_val(volatile void *, int, volatile int *);

int
probe_common(probe_info_t *pi)
{
	ulong	sr;
	cpu_cookie_t s;
	cpuid_t cpu;
	int 	result;
	int	window, padap, woffset;

	if (small_window((caddr_t)pi->addr, &window, &padap, &woffset) ||
	    large_window((caddr_t)pi->addr, &window, &padap, &woffset)){
		cpu = io4[window].slot;
		if (EVCFGINFO->ecfg_board[cpu].eb_ioarr[padap].ioa_enable == 0)
			return 1;
	}
	else
		return 1;


	mutex_lock(&probe_sema,PRIBIO);

	s = setmustrun(0);

#if IP21
	probe_in_progress = 1;
	probe_cpu = cpuid();
#endif

	sr = getsr();
	setsr(SR_KADDR); /* Desired sr status for kernel */

	ASSERT((pi->len == 1) || (pi->len == 2) || (pi->len == 4) ||
				(pi->len == 8));

	if (pi->type == PROBE_WBADADDR || pi->type == PROBE_WBADADDR_VAL) {
		ev_wbad_inprog = 1;
		ev_wbad_val = 0;
		result = _wbadaddr_val(pi->addr, pi->len, pi->ptr);
	} else {
		result = _badaddr_val(pi->addr, pi->len, pi->ptr);
	}

	/* error state will be cleared if caused by probe */
	result |= is_buserror(pi->addr);
#if IP25
	/* Due to an A-chip bug, another cpu could see IP25_CC_ERROR_MY_DATA
	 * error and cc_clear_all_cpus_error() will clear that for all other
	 * cpus.
	 */
	if (result)
		cc_clear_all_cpus_error((__uint64_t)IP25_CC_ERROR_MY_DATA);
#endif
	setsr(sr);
	if( ev_wbad_inprog ) {
		DELAY(300);
		result = result || ev_wbad_val;
		ev_wbad_inprog = 0;
	}
	GDA->everror_valid = 0;	/* Invalidate everror after probing */

#if IP21
	probe_in_progress = 0;
	probe_cpu = -1;
#endif

	restoremustrun(s);

	mutex_unlock(&probe_sema);
	return result;
}


/*
 * Called from buserror_intr() in trap.c, or ecc_cleanup() in ml/error.c
 * Unlike previous platforms, we will not clear the buserror since we get here
 * for hardware failures and always panic.
 * As a matter of fact, badaddr() will be done with interrupt masked and
 * R4000 would not even see this interrupt and would not call this function.
 * The error register in each ASIC and the CC everest interrupt pending will
 * be checked/cleared by is_buserr() if probing.
 */
/* ARGSUSED */
int
dobuserr(eframe_t *ep, inst_t *epc, uint flag)
{
	caddr_t fault_addr;

	if (flag == 1)		/* 0: kernel  1: nofault  2: user */
		return 0;

	fault_addr = (caddr_t)ldst_addr(ep);

	cmn_err(CE_CONT|CE_CPUID, "Bus Error at %s address %x\n",
		(USERMODE(ep->ef_sr) ? "User" : "Sys"), fault_addr);

	if (!buserror_addr(fault_addr)) {
		/* Not sure what to do */
	}

	cmn_err(CE_PANIC, "Bus Error\n");
	/* NOTREACHED */
}











extern char *vme_am_name[];

static char *
epcspace[] = {
	"PBUs Device 0",
	"PBUs Device 1",
	"PBUs Device 2",
	"PBUs Device 3",
	"PBUs Device 4",
	"PBUs Device 5",
	"PBUs Device 6",
	"PBUs Device 7",
	"PBUs Device 8",
	"PBUs Device 9",
	"Internal Registers",
	"Unused Space",
	"Unused Space",
	"Unused Space",
	"Unused Space",
	"Unused Space"
};


extern piomap_t piomaplisthd;

/*
 * returns false if addr is not in io space.
 * returns true and print out information about the address that maaped.
 */
int
buserror_addr(caddr_t addr)
{
	int	window;
	int	padap;
	int	woffset;
	int	slot;
	int	i;
	int	ea_bus;
	int	ea_adap;
	ulong	ea_space;
	ulong	ea_iopaddr;
	piomap_t   *p;
	evioacfg_t *iocfg;


	printf("\n\nIO Mapping Info for Kernel Virt Address 0x%x\n\n");

	if (small_window(addr, &window, &padap, &woffset)) {
		printf("\t in Small Window IO Space.\n");
	} else if (large_window(addr, &window, &padap, &woffset)) {
		printf("\t in Large Window IO Space.\n");
	} else {
		printf("\t not mapped in IO Space.\n\n");
		return 0;
	}

	slot = io4[window].slot;

	printf("\t mapped to IO Board at slot %d\n", addr, slot);
	printf("\t Window %d Physical Adapter %d\n", window, padap);

	iocfg = &(EVCFGINFO->ecfg_board[slot].eb_io.eb_ioas[padap]);

	if (iocfg->ioa_enable == 0) {
		printf("\t?? The mapped adapter is not enabled.\n");
		return 1;
	}

	switch (iocfg->ioa_type) {

	case IO4_ADAP_EPC:
		i = (woffset >> 12) & 0xF;
		cmn_err(CE_NOTE,"\t mapped to EPC %s window offset 0x%x",
				epcspace[(woffset >> 12) & 0xF], woffset);
		ea_bus	   = ADAP_LOCAL;
		ea_adap	   = 0;		/* There is only one EPC */
		ea_space   = 0;		/* one kind of space */
		ea_iopaddr = woffset;
		break;

	case IO4_ADAP_VMECC:
		printf("\t Adapter mapped is VMECC.\n");
		if (vme_address(addr, &ea_adap, &ea_iopaddr, &ea_space) == 0){
			printf("\t ?? address unmapped. ??\n");
			return 1;
		}
		ea_bus = ADAP_VME;
		if (ea_adap == -1)		/* Wild Card? */
			ea_adap = iocfg->ioa_virtid;
		printf("\t VME Adapter %d space %s Address 0x%x.\n",
			ea_adap, vmeam[ea_space].name, ea_iopaddr);
		break;

	case IO4_ADAP_HIPPI:
		printf("\t Adapter mapped is HIPPI.\n");
		ea_bus	   = ADAP_IBUS;
		ea_space   = PIOMAP_FCI;
		ea_iopaddr = woffset;
		printf("\t HIPPI Adapter %d window offset 0x%x.\n",
			ea_adap, ea_iopaddr);
		break;

#if LATER
	case IO4_ADAP_SCSI:
	case IO4_ADAP_SCIP:
		ea_bus	   = ADAP_SCSI;
		ea_adap	   = iocfg->ioa_virtid;
		ea_space   = 0;		/* one kind of space */
		ea_iopaddr = moffset;
		printf("\t SCSI Adapter %d window offset 0x%x.\n",
							ea_adap, ea_iopaddr);
		break;
#endif

	case IO4_ADAP_NULL:
	default:
		printf("\t Bad adapter type %d\n",iocfg->ioa_type);
		return 1;
	}

	/* find piomap by match error addr bus/adap/space/iopaddr */
	p = (piomap_t *)0;
	i = 1;
	while(p = pio_ioaddr(ea_bus, ea_adap, ea_iopaddr, p)){
		if (i){
			printf("Piomap(s) mapping ioaddress 0x%x:",ea_iopaddr);
			i = 0;
		}
		printf("\t %s", p->pio_name);
	}
	if (i == 0)
		printf(" \n");

	/* failed to locate the piomap */
	return i;
}

#if IP19 || IP25
/*
 * If 'uvaddr' maps to any VME space, then clear all the error bits 
 * which could have been set due to user accessing a bad VME address
 * and return.
 * Return : 1 if cleared.
 *	    0 if there is any error..
 * NOTICE:
 * PLEASE READ:
 * This routine ASSUMES that USER context is available although it is called
 * from Interrupt handler!!!!. Reason is, User VME address gets an exception
 * when it tries to read an invalid VME address. Now this should be followed
 * almost immediately by the interrupt from CC due to some bits set in 
 * ERTOIP. Since these events happen in quick succession, we hope to have
 * the User process context being used here.!!
 */
/* Mutex array used to check if any VMECC error handling is in progress 
 * This is needed to make sure that while handling write errors, any other
 * read error should not come by and clean up the mess, leaving write 
 * error confused.
 */
extern        int     vmecc_errhandling[]; 

int
uvme_errclr(eframe_t *ep)
{
#if IP25
        evreg_t            eraddr;            /* CC Error Address          */
	evreg_t            physaddr;          /* CC Error Physical addr    */
#endif
	evreg_t	           uvaddr;            /* User process virtual addr */
	pde_t              pte;               /* Page table entry          */
	auto pfn_t	   ptebase;           /* Page frame number         */
	uint	           window, padap;    
	int	           slot, i, iadap;
	uint	           cause;
	evioacfg_t 	   *iocfg;
	evbrdinfo_t	   *brd;
	
	ASSERT(ep);

	if (EV_GET_REG(EV_HPIL) >= EVINTR_LEVEL_ERR){
		return 0;  /* Some Error interrupt.. Not a User VME case */
	}
	cause = ep->ef_cause;
#if IP25
	/* Due to a R10000 bug where in case of a DBE and an Intr
	 * the DBE may not be seen, we check for only one condition 
	 * instead of both.
	 */

	if (!((cause & SRB_ERR) || (((cause & CAUSE_EXCMASK) == EXC_DBE) ||
			((cause & CAUSE_EXCMASK) == EXC_IBE)))) {
		return 0;
	}
#else
	if (!(cause & SRB_ERR) || (((cause & CAUSE_EXCMASK) != EXC_DBE) &&
			/* Executing from VME mem?? */
			((cause & CAUSE_EXCMASK) != EXC_IBE)))
		/* Not CC ERTOIP related Interrupt OR Not a Data error */
		return 0;
#endif /* IP25 */

#if IP25
	/* On IP25 boards, when there is a UDBE, the R10K receives the
	 * error interrupt before it receives the UDBE so what we need
	 * to do is get the ptebase from the Error Address register
	 * and determine what the PFN is using that.  (In this case
	 * loading the uvaddr from the ep does not give us the address
	 * of the instruction which caused the error). Why? User
	 * context is not available!  The below handles the case when
	 * you're here for an error interrupt reason and it is a PIO
	 * read response error
	 * Example:
	 *  +      CC Error Address Register: 0x50458c00000
	 *  +        cause: read response error(1)
	 *  +        address: 0x458c00000
	 */	 
	eraddr = EV_GET_REG(EV_ERADDR);

	/* Error interrupt from a PIO read response timeout, and the
	 * error address is captured 
	 */
	if((cause & SRB_ERR) && (eraddr & 0x40000000000) && 
	   (eraddr & 0x10000000000)){
	  /* Bits 0:39 contain the physaddr of the transaction for
	   * which an error occurred.
	   */
	  physaddr = eraddr & 0x3fffffffff;
 	  /* Now I have a 36bit physical addr; if we chop off the LSB
	   * then we have the PFN which we will then need to shift
	   * right N bits to get in the format which make the
	   * LWIN_XXXX macros happy. (N is the Page offset which
	   * depends on page size which is why we divide by the number
	   * of bytes per page to accomplish the same below).  */
	  ptebase = (pfn_t)((physaddr & 0xfffffc000) / NBPP);
#ifdef UDBE_DEBUG
	  cmn_err(CE_CONT | CE_CPUID, 
		  "physaddr: 0x%x, ptebase: 0x%x\n", physaddr,ptebase);
#endif
	} 

	else{
#endif /* IP25 */
	  uvaddr = ldst_addr(ep);
	  if (uvaddr >= K0SEG) {
	    /* Not in User address space */
	    return 0;
	  }
	  /* 
	   * Get the pte needed
	   * The pte we get below points to
	   * some physical space, not migratable memory;
	   * therefore, we do not need to do pfn locking
	   */
	  vaddr_to_pde((caddr_t) uvaddr, &pte);
	  ptebase = pg_getpfn(&pte);
#if IP25
	}
#endif /* IP25 */

       /* At this point, we have the PFN (ptebase) which we got from
	* ldst_addr() in the case of a DBE or from EV_ERADDR in the
	* case of an error intr.  
	*/
	window = LWIN_SYSWINDOW(ptebase);
	padap  = LWIN_SYSPADAP(ptebase);

#ifdef UDBE_DEBUG
	cmn_err(CE_CONT|CE_CPUID, 
		"window: 0x%x, padap: 0x%x\n", window, padap);
#endif
	if ((window == 0) || (padap < 1) || (padap >= IO4_MAX_PADAPS)){
#ifdef UDBE_DEBUG
	  cmn_err(CE_NOTE | CE_CPUID, 
	       "uvme_errclr: could not get pte needed win=0x%x,padap=0x%x.\n",
		window,padap);
#endif
	  return -1;
	  /* 	  return 0; */
	}

	slot = io4[window].slot;

	if((slot <= 0) || (slot > EV_MAX_SLOTS)){

#ifdef UDBE_DEBUG
	  cmn_err(CE_CONT | CE_CPUID, "uvme_errclr: bad slot: %d.\n",slot);
#endif	  
	  return 0;
	}

	iocfg = &(EVCFGINFO->ecfg_board[slot].eb_ioarr[padap]);
	if ((iocfg->ioa_enable == 0) || (iocfg->ioa_type != IO4_ADAP_VMECC)){
#ifdef UDBE_DEBUG
	  cmn_err(CE_CONT | CE_CPUID, 
		  "uvme_errclr: IO4 disabled or not VMECC.\n");
#endif	  
	  return 0;
	}
	iadap = vmecc_xtoiadap(slot*4 + vmecc_io4atova(padap));

	if (atomicAddInt(&vmecc_errhandling[iadap], 1) == 1) {
	  /* No other error handling in progress for this VMECC */

	  cc_error_clear(0);
#ifdef IP25
	  cc_clear_all_cpus_error((__uint64_t)IP25_CC_ERROR_MY_DATA);
#endif
	  /* Bug in A chip could leave pending MC3 error bits, clear'em */
	  for (i=1; i < EV_MAX_SLOTS; i++){
	    brd = &EVCFGINFO->ecfg_board[i];
	    if (brd->eb_enabled && (brd->eb_type == EVTYPE_MC3))
	      mc3_error_clear(i);
	  }

	  io4_error_clear(slot);
	  fchip_error_clear(slot, padap);
	  vmecc_error_clear(slot, padap);
	  

	}

	atomicAddInt(&vmecc_errhandling[iadap], -1);

	/* Everything looks fine. Now cleanup the mess */
	cc_error_clear(0);
	io4_error_clear(slot);
	fchip_error_clear(slot, padap);
	vmecc_error_clear(slot, padap);
#ifdef UDBE_DEBUG
	  cmn_err(CE_CONT | CE_CPUID, "uvme_errclr: error cleared.\n");
#endif	  
	return(1);
}
#endif /* IP19 */

#if IP21

/*
 * This routine looks around for a vme bus read error which could have caused
 * caused the bus error being processed.  On IP21, we can have ghosts errors
 * appear on other cpu's.  The read exception is also imprecise so we have
 * to guess which process caused the bus error (if it was a user-level read).
 * This makes life interesting.
 *
 * Assumptions: there won't be multiple VME bus errors coming in from
 * different cpus at the same time...
 *
 * Returns:
 *     -1 - no VME bus error
 *	0 - real bus error, caller caused it.
 *	1 - ignore the bus error
 */

lock_t vmedbe_lock, vmewrtdbe_lock;
time_t	vme_overrun_time = 0;
unchar	vmedbe_state = 0;
#define VDBE_VMEFND	0x02
#define VDBE_USRVME	0x04

int
uvme_async_read_error(int usermode)
{
	int	s, ws;
	/*REFERENCED*/
	int	err;
	int	checker = 0;
	int	retval = VME_DBE_IGNORE;
	int 	slot, padap;

	s = splock(vmedbe_lock);

	/* The first cpu through checks for errors */
	if( !(vmedbe_state & VDBE_VMEFND) ) {
		/* if no VME error, only 1 caller to this routine
		 * expected so exit with a real error indicated.
		 */
		DELAY(300);
		if( !vmecc_rd_error(&slot,&padap) ) {
#if DEBUG
			printf("VME rd err: lbolt = 0x%x:0x%x\n",
				lbolt,vme_overrun_time);
#endif /* DEBUG */
			if( lbolt - vme_overrun_time >= 2 ) {
				spunlock(vmedbe_lock, s);
				return VME_DBE_NOTFND;
			}
			/* assume we've already cleared error state */
			cmn_err(CE_WARN, "VME rd err: overrun on cpu %d", 
				cpuid());
		}
		
		/* Found the VME error, now get in sync with other
		 * cpus who might see an error
		 */
		vmedbe_state |= VDBE_VMEFND;
		checker = 1;
	}

	/* If we get this far we know there's a VME error.
	 * Blame it on any current user process which has VME
	 * space mapped into its address space.
	 */
	if( usermode && (curuthread->ut_pproxy->prxy_flags & PRXY_USERVME) ) {
		vmedbe_state |= VDBE_USRVME;
		retval = VME_DBE_OWNER;
	}
	spunlock(vmedbe_lock, s);

	/*
	 * clean up the error state.
	 */
	if( checker ) {
		s = splock(vmedbe_lock);

		/* If no other cpu's taken the blame by now, then we do. */
		if( !(vmedbe_state & VDBE_USRVME) )
			retval = VME_DBE_OWNER;

		vmedbe_state = 0;
		spunlock(vmedbe_lock, s);

		if (retval == VME_DBE_IGNORE) {
			cc_error_clear(0);
			/* block VME write errors from clearing these on us */
			ws = splock(vmewrtdbe_lock);
			while( err = vmecc_rd_error(&slot,&padap) ) {
				io4_error_clear(slot);
				fchip_error_clear(slot, padap);
				vmecc_error_clear(slot, padap);
#if DEBUG
				if( err & VMECC_ERROR_OVERRUN )
					printf("VME rd err overrun cpu %d at 0x%x\n",
						cpuid(),lbolt);
#endif /* DEBUG */
			}
			vme_overrun_time = lbolt;
			spunlock(vmewrtdbe_lock, ws);
		}
	}

	if( usermode ) 
		cmn_err(CE_WARN, "VME Bus Error %s on CPU %d for process %d",
			(retval == VME_DBE_IGNORE)?"ignored":"caught", 
			cpuid(), current_pid() );
	else 
		cmn_err(CE_WARN, "VME Bus Error %s on CPU %d",
			(retval == VME_DBE_IGNORE)?"ignored":"caught", 
			cpuid());

	return retval;
}
#endif /* IP21 */
