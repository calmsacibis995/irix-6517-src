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

#ident	"sys/EVEREST/io4.c: $Revision: 1.59 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/sbd.h>
#include <sys/iotlb.h>
#include <sys/debug.h>
#include <sys/edt.h>
#include <sys/var.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/everror.h>

#ifdef	DEBUG
#include <sys/timers.h>
#endif

/*
 * Allocates kernel virtual space for io map.
 */

struct	iopte	iotlb [IOMAP_VPAGES];
int	iotlb_next = 0;
extern  int syssegsz;

int	io4ia_war = 0;		/* set to 1 if IO4 IA workaround needed */
static int ia_old_rev = 0x2;
extern int io4ia_war_enable;

void pio_init(void);
void fcg_init(unchar, unchar, unchar, uint, unchar);

lock_t iospacelock;
#ifdef TFP
lock_t	io4ia_war_lock;
#endif /* TFP */

#ifdef IO4_CFG_READ_WAR
lock_t	io4_cfgLock;
#endif

#define K2RESERVED	(3*128*1024*1024)
#define K2CEILING	(IOMAP_VPAGES - (0x800000/IOMAP_VPAGESIZE))

/* ARGSUSED */
void *
iospace_alloc(long size, ulong pfn)
{
#if  _MIPS_SIM != _ABI64
	__psunsigned_t	kvaddr;
	static int init = 1;
	struct iopte *ip;
	int	pages;
	ulong	ospl;

	ASSERT(size > 0);

	ospl = splock(iospacelock);

	/* have to share k2 space with the kernel */
	if( init ) {
		iotlb_next = (K2RESERVED/IOMAP_VPAGESIZE);
		if( iotlb_next & 1 )
			iotlb_next++;
		init = 0;
	}

	/* don't want to run into kernel area at top of k2 */
	pages = (size + IOMAP_VPAGESIZE - 1) / IOMAP_VPAGESIZE;
	if( pages & 1 )
		pages++;

	if( pages + iotlb_next > K2CEILING ) {
		spunlock(iospacelock,ospl);
		return 0;
	}

	kvaddr = IOMAP_BASE + (iotlb_next << IOMAP_VPAGESHIFT);

	for (; size > 0; size -= 2*IOMAP_VPAGESIZE, iotlb_next++) {
		if (iotlb_next >= IOMAP_VPAGES)
			panic("IO map space exhausted.");
		ip = &iotlb[iotlb_next];
		ip->pte_pfn = pfn;
		ip->pte_cc  = 2;	/* UNCACHED */
		ip->pte_m   = 1;
		ip->pte_vr  = 1;
		ip->pte_g   = 1;
		pfn += (IOMAP_VPAGESIZE >> IO_PNUMSHFT);

		iotlb_next++;
		if (iotlb_next >= IOMAP_VPAGES)
			panic("IO map space exhausted.");
		ip++;
		ip->pte_pfn = pfn;
		ip->pte_cc  = 2;	/* UNCACHED */
		ip->pte_m   = 1;
		ip->pte_vr  = 1;
		ip->pte_g   = 1;
		pfn += (IOMAP_VPAGESIZE >> IO_PNUMSHFT);
	}

	spunlock(iospacelock,ospl);
	return ((void *)kvaddr);
#else	/* 64-bit addressing in kernel */
	return((void *)(io_ctob(pfn)|K1BASE));
#endif	/* 64-bit kernel addressing */
}

ulong
io4_virtopfn(void *addr)
{
#if _MIPS_SIM != _ABI64
	ulong page;

	struct iopte *ip = &iotlb[IOMAP_VPAGEINDEX((ulong)addr)];

	/* the pfn in the iotlb is 4 meg aligned and we need
	 * a pfn which is 4k aligned, so, grap the necessary bits
	 * from the address and or them into the pfn.
	 * The (1<<IO_PNUMSHFT) captures the odd page bit.
	 */
	page = ((ulong)addr & (IOMAP_TLBPAGEMASK | (1<<IO_PNUMSHFT)))
							>> IO_PNUMSHFT;

	return ip->pte_pfn | page;
#else	/* 64-bit addressing in kernel */
	ASSERT(addr > (void *)(ctob(LWIN_SYSPFN_BASE)|K1BASE));
	return(btoc((caddr_t)addr-(caddr_t)K1BASE));
#endif	/* 64-bit kernel addressing */
}



io4_t io4[EV_MAX_IO4S];

void
io4_intr(eframe_t *ep, void *arg)
{
	cmn_err(CE_NOTE, "IO4 error Interrupt.\n");
	everest_error_handler(ep, arg);
}


/*
 * Look for all io boards and assign windows, init config registers,
 * look for adaptors on board and init iomap for each adaptor.
 */
void
evio_init(void)
{
	unchar	window;		/* window assignment for each ioboard */
	unchar	slot;		/* physical slot */
	unchar	padap;		/* physical adaptor id within the board */
	unchar	nfcg;		/* # of fcg on the io4 board */
	ulong	adapconf;
	ulong	fci_id;
	int	adap;
	int	num_io4s = 0;
	uint	has_ia_oldrev = 0;
	uint	rev_id;
	evioacfg_t *evioa;

	/* initialize the piomap lists */
	pio_init();

	/* initialize things needed even if system has no VME */
	vmecc_preinit();

	/* initialize the iospace_alloc lock */
	initnlock(&iospacelock, "iospace");
#ifdef TFP
	initnlock(&io4ia_war_lock, "io4warlock");
#endif

#ifdef IO4_CFG_READ_WAR
	initnlock(&io4_cfgLock, "io4_cfgLock");
#endif
#ifdef MULTIKERNEL
	/* All IO4s are owned by the golden cell, so return
	 * if we're not it.
	 */
	if (evmk_cellid != evmk_golden_cellid)
		return;
#endif

	for (slot = EV_MAX_SLOTS-1; slot > 0; slot--) {

		if (EVCFGINFO->ecfg_board[slot].eb_type  != EVTYPE_IO4  || 
		    EVCFGINFO->ecfg_board[slot].eb_enabled == 0)
			continue;

		window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
		io4[window].slot   = slot;

		/* XXXXX No mapram in IP23. Remove later!! */
		io4[window].mapram = (__psunsigned_t)
					iospace_alloc(IO4_MAPRAM_SIZE, 
						      LWIN_PFN(window,0));

		fci_id = 0;
		nfcg = 0;
		adapconf = 0;
		evioa = &EVCFGINFO->ecfg_board[slot].eb_io.eb_ioas[1];

		for (padap = 1; padap < IO4_MAX_PADAPS; padap++, evioa++) {

			if (evioa->ioa_enable == 0)
				continue;
			adapconf |= (1L << padap);
			adap = evioa->ioa_virtid;

			switch (evioa->ioa_type) {

			case IO4_ADAP_FCHIP:{
				__psunsigned_t	swin;
				unchar master;

				swin = SWIN_BASE(window, padap);
				fchip_init(swin);
				switch(master=EV_GET_REG(swin+FCHIP_MASTER_ID)){

				case IO4_ADAP_FCG:
				case IO4_ADAP_HIP1A:
				case IO4_ADAP_HIP1B:
				    adapconf |= (1L << (padap+8));
				    if (nfcg++) {
					    adapconf |= (1L << (padap+16)) | (1L << 24);
					    if (nfcg>2)
						panic ("Too Many FCGs");
				    }
				    fcg_init(slot,padap,window,fci_id,master!=IO4_ADAP_FCG);
				    evioa->ioa_type = master;
				    fci_id++;
				    break;

				case IO4_ADAP_VMECC:
				    vmecc_init(slot,padap,window,adap,fci_id);
				    evioa->ioa_type = IO4_ADAP_VMECC;
				    fci_id++;
				    break;
				
				case IO4_ADAP_HIPPI:
				    if (io4hip_init(slot,padap,window,adap))
					    evioa->ioa_type = IO4_ADAP_HIPPI;
				    else
					    /* Disable and continue */
					    evioa->ioa_enable = 0; 
				    break;
				
				default:
				    /* Disable and continue */
				    evioa->ioa_enable = 0; 
				    break; 
				}
				break;
			}
				

			case IO4_ADAP_SCSI:
#ifndef SABLE
				s1_init(slot, padap, window, 0);
#endif
				break;

			case IO4_ADAP_SCIP:
#ifndef SABLE
				s1_init(slot, padap, window, 1);
#endif
				break;

			case IO4_ADAP_EPC:
				epc_init(slot, padap, window);
				break;

			case IO4_ADAP_DANG:
				dang_init(slot, padap, window);
				break;

			case IO4_ADAP_NULL:
				/* Padaps not existant are setup as 
				 * enabled and IO4_ADAP_NULL by prom
				 */
				evioa->ioa_enable = 0;
				break;
			default:
				panic("Bad IO Adaptor type %d slot %d adap %d",
					evioa->ioa_type, slot, padap);
			}
		}

		IO4_SETCONF_REG(slot, IO4_CONF_ADAP,	 adapconf);
		IO4_SETCONF_REG(slot, IO4_CONF_INTRMASK, 2);

		evintr_connect(IO4_CONFIGADDR(slot,IO4_CONF_INTRVECTOR),
				EVINTR_LEVEL_IO4_ERROR,
				SPLMAX,
				EVINTR_DEST_IO4_ERROR,
				io4_intr,
				(void *)(__psint_t)EVINTR_LEVEL_IO4_ERROR);
		/*
		 * Check IO4 IA Rev and if any of the IO4s have old rev,
		 * set a flag to indicate it.
		 *
		 * XXX
		 * This check assumes that new rev of IA will have a value
		 * > ia_old_rev in the REVTYPE register. If the Rev value
		 * is kept elsewhere, that has to be checked instead.
		 */
		rev_id = IO4_GETCONF_REG(slot, IO4_CONF_REVTYPE);
		rev_id = ( rev_id & IO4_REV_MASK) >> IO4_REV_SHFT;

		if (rev_id <= ia_old_rev)
			has_ia_oldrev++;

		num_io4s++;
	}
#ifdef	DEBUG
	if (sizeof(everror_t) >= 0x800)
		arcs_printf("everror_t size:0x%x too big should be < 0x800\n",
			sizeof(everror_t));
#endif /* DEBUG */

	/* 
	 * If IO4 IA war (systune variable) is turned on, 
	 * 	Or if there are more then one enabled IO4s in the system,
	 *	and any one of them have old rev IA.
	 */
	if (io4ia_war_enable || ((num_io4s > 1) && has_ia_oldrev))
		io4ia_war = 1;

	everest_error_clear(1);
}


extern int nvmecc, num_dangs;
int nscsi = 0;		/* needs to be done by scsi code */

int
readadapters(uint bustype)
{
	switch (bustype) {
	case ADAP_VME:
		return nvmecc;
	case ADAP_SCSI:
		return nscsi;
	case ADAP_LOCAL:
		return 1;
	case ADAP_IBUS:
		return IO4_MAX_PADAPS * EV_MAX_SLOTS;
	case ADAP_GIO:
		return	num_dangs;
	default:
		return 0;
	}
}


void
pio_mapfree_ibus(piomap_t *pmap)
{

	switch (pmap->pio_type) {
	case PIOMAP_FCI:
		/* Nothing to free up */
		break;
	default:
		cmn_err(CE_WARN,"invalid ibus piomapfree\n");
		ASSERT(0);
	}
}

int
pio_mapfix_ibus(piomap_t *pmap)
{
	int err;

	switch (pmap->pio_type) {
	case PIOMAP_FCI:
		err = pio_mapfix_fci(pmap);
		break;
	default:
		err = 1;
		cmn_err(CE_WARN,"invalid ibus piomapfix\n");
		break;
	}

	return err;
}

/*
 * Flush the caches in the IO4 mapped holding adapter mapped by
 * adap_addr
 * Return 0 on success non-zero on failure.
 *
 * Routine is a bit paranoid with checking for all values to make sure 
 * that it will not cause any sort of exception, as we would be operating in 
 * no man's land once interrupts are disabled as part of flushing.
 *
 */


extern void io4_doflush_cache(iopaddr_t);
extern long io4_config_read(iopaddr_t, long, long);
extern void tfp_flush_cache(int);

/*
 * io4_flush_inprogress indicates the IO4 which is being flushed right now.
 * We avoid flushing this again if a flush is in progress. This could save
 * wasted flushing a lot of times. 
 * But does this leave a small window for data to hang around ?
 * (window of few usecs ?)
 */
int   	io4_flush_inprogress[EV_MAX_IO4S];

#ifdef	DEBUG
#define	IO4IA_DEBUG	1
#endif

#ifdef	IO4IA_DEBUG
int	io4_flush_count[EV_MAX_IO4S];
long	max_wa_time, total_wa_time, total_wa_count;
#endif	/* IO4IA_DEBUG */

/*
 * Bounds of addresses for memory.
 * <physical addresses>
 */

long	mem_highaddr;
long	mem_lowaddr;

int
io4_flush_cache(iopaddr_t adap_addr)
{
	int	     	io4win;
	int		padap;
	int		woffset;
	int		io4slot;
	extern pfn_t 	physmem;
	static int	first_time = 1;
#ifdef	IO4IA_DEBUG
	time_t		start_time, wa_time;
#endif
	int		ospl;
	
	if (first_time) {
		extern char end[];
		mem_highaddr = ctob(node_getmaxclick((cnodeid_t) 0));
		mem_lowaddr  = K0_TO_PHYS((long)end);
		mem_lowaddr  = (mem_lowaddr >> 7) << 7; /* Cache align */
		first_time   = 0;
	}
		
	if (io4ia_war == 0){
		/* Don't need to flush if io4ia_war is zero */
		return 0;
	}

	/*
	 * Check if the address belongs to small window or large window
	 * and accordingly get the window and adapter number.
	 */
	if (!small_window((caddr_t)adap_addr, &io4win, &padap, &woffset) &&
	    !large_window((caddr_t)adap_addr, &io4win, &padap, &woffset)) {
		return 1;
	}

	io4slot = io4[io4win].slot;

	if ((io4slot < 1) || (io4slot >= EV_MAX_SLOTS))
		return 1;

	if (atomicAddInt(&io4_flush_inprogress[io4win], 1) > 1) {
		/* Flush already in progress. Return success.   */
		/* Note: AtomicAddInt returns new value	 */
		atomicAddInt(&io4_flush_inprogress[io4win], -1);

		return 0;
	}

#ifdef	IO4IA_DEBUG
	start_time = _get_timestamp();
#endif	

#if	defined(TFP)
	tfp_flush_cache(io4slot);
#elif 	(defined(IP19) || defined(IP25))
	/*
	 * On Non TFP systems, we need to worry about VCEs after the
	 * data in the IO4 cache has been read in. So, call an assembly
	 * routine that can do the right thing 
	 */

	IO4_CONFIG_LOCK(ospl);
	io4_doflush_cache((iopaddr_t)IO4_CONFIGADDR(io4slot, IO4_CONF_CACHETAG0L));
	IO4_CONFIG_UNLOCK(ospl);

#else
	<< Unknown platform >>
#endif	/* TFP */


	

#ifdef	IO4IA_DEBUG
	wa_time = _get_timestamp() - start_time;

	total_wa_time += wa_time;
	total_wa_count++;

	if (max_wa_time < wa_time){
		max_wa_time = wa_time;
	}

	io4_flush_count[io4win]++;
#endif	/* IO4IA_DEBUG */

	atomicAddInt(&io4_flush_inprogress[io4win], -1);

	return 0;
	
}

#ifdef	TFP

#ifdef	IO4IA_DEBUG
/* 
 * Timing analysis checks.
 */
long	flush_hiaddr;
long	flush_lowaddr = ( 0x100000000 );

#define	FLUSH_MASK	0xFFF
long	flush_addr[FLUSH_MASK + 1];
int	flush_indx = 0;

#define	SETLIMITS(ioa, fl, fh)	\
	( (ioa < fl) ? (fl = ioa) : ( (ioa > fh) ? (fh = ioa) : 0 ) )
#endif	/* IO4IA_DEBUG */

void
tfp_flush_cache(int slot)
{

	long		regval;
	volatile long	data;
	caddr_t		baseaddr = (caddr_t)K0BASE;
	iopaddr_t	io4_addr;
	int		i;
	int		ospl;


	/* Don't know why, but limiting one processor at a time into this
	 * IA workaround code greatly decrease the chances of getting a
	 * g-cache parity error (apparently another HW bug where g-cache
	 * errors are induced by the config registers reads).
	 */
	ospl = splock(io4ia_war_lock);

	for (i = IO4_CONF_CACHETAG0L; i < IO4_CONF_CACHETAG3U; i+= 2) {

		io4_addr = (iopaddr_t)IO4_CONFIGADDR(slot, i);
		regval = io4_config_read(io4_addr, mem_lowaddr, mem_highaddr);
		if (regval){
			if (regval == -1) {
				cmn_err(CE_CONT, "io4_config_read failed on slot %d register %d (OK only at startup)\n", slot, i);
				break;
			}
			data = *(volatile long *)(baseaddr + regval);
#ifdef	IO4IA_DEBUG
			SETLIMITS(regval, flush_lowaddr, flush_hiaddr);
			flush_addr[flush_indx] = regval;
			flush_indx = ++flush_indx & FLUSH_MASK;
#endif
		}
	}
	spunlock(io4ia_war_lock,ospl);

}

#endif /* TFP */
