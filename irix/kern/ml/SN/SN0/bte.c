/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1986-1995 Silicon Graphics, Inc.                        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * bte.c -- Contains the low-level control code for the SN0 block
 *	transfer engine.  The BTE can be viewed as an augmented 
 *	general purpose cache-coherent DMA engine.  Residing in the IO section
 *	of the Hub ASIC, the BTE can be programmed either to copy from one
 *	memory range to another or to fill a specified memory range with 
 *	copies of a selected cache line.  
 *
 *	In addition to these facilities, the BTE is capable of "poisoning"
 *	lines in the source range while performing a copy operation.  When a
 *	line is poisoned, the entry in the directory associated with it
 *	transitions into a special state.  While in this state, the directory 
 *	signals a bus error in response to read or write requests.  By using 
 *	the POISON bit, we can minimize the number of TLB invalidations 
 *	needed when pages are migrated.
 *
 *	The interface is divided into two parts: an upper level and a 
 *	lower level.  The upper level routines are intended for the 
 *	most common applications of the bte and are very similar to
 *	bcopy and bzero.  The lower level routines contain the actual
 *	hardware-dependent portions of the system.  While they require
 *	slightly more care to use properly, they allow a degree of
 *	asynchrony not provided by the upper level routines.
 *
 *	BTE Constraints:
 *	- BTE has lower performance if the addresses in use by 
 *	  the two CPUs conflict (both source and target)
 *	- BTE has lower performance if it's pushing and pulling from
 *	  the same address or any address range within 15 cache line
 *	  range!!. (i.e. source and destination address should
 *	  not be within 15 cache line address range
 *	- BTE has lower performance if an I/O device is doing DMA
 *	  from the same page as used by BTE.
 *
 *	Special requirements from Hub 1.0
 *	- Both BTEs should not be in use simultaneously.
 *	- BTE will not support interrupts.
 * 	Above are NOT true in Hub 2.0. Both features work correctly.
 *
 * BTE_NOTIFICATION_WAR Details:
 *
 * Due to a bug in BTE of Hub 2.0, it forgets to send out write 
 * invalidate notification once in a way. This work around kicks in
 * at those times.
 * Work around involves doing
 *	- Stop the BTE that failed to send write notification.
 *	- Grab control of all the BTEs on the node 
 *	- Force all of them to do a one cache line transfer
 *	  The source and the target cache lines used in this context
 *	  were allocated at the boot time as part of allocating the
 *	  cache line used for write invalidate.

 *	- Purpose of this small transfer is to bring BTEs to known state. 

 *	- For each of the above transfers, wait either till the transfer
 *	  is done, or BTE_NOTIFICATION_TIME completes. (typically the 
 *	  BTE that got into the problem never competes the transfer, 
 *	  and the other BTE on the same node gets through properly.
 *
 *	- Release all the BTEs, and retry the transfer that got into
 *        trouble. 
 *	Extension to notification WAR:
 *	There was a second hang of BTE noticed after the first WAR. 
 *	Typically when BTE gets into this situation, we can never recover.
 *	This was supposedly happening if the Hub CRB fails to respond to 
 *	the notification message from BTE (and some other combination)
 *	We try to avoid this by 
 *	- Forcing the cache line used by BTE for notification to be 
 * 	  in "shared mode". This is done by flushing the cache line
 *	  and then doing a prefetch_load of the cacheline. This brings
 *	  the cache line in shared.
 *
 * For gory details, look into code under BTE_NOTIFICATION_WAR
 */

#ident "$Revision: 1.83 $"
/*
#define	BTE_CORRUPTION_WAR 	1
#define	BTE_INTR_MODE		1
*/

#define	BTE_NOTIFICATION_WAR  	1

#ifdef	BTE_NOTIFICATION_WAR
#define	BTE_NOTIFICATION_TIMEOUT	10
#define	BTE_SRCCLINE_OFFS	2
#define	BTE_TARGCLINE_OFFS	4
#define	BTE_EXTRA_CLINES	6
#else
#define	BTE_EXTRA_CLINES	0
#endif


#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/atomic_ops.h>
#include <sys/ioerror.h>
#include <sys/iograph.h>
#include <sys/runq.h>
#include <sys/rt.h>			/* rt_pin_thread & friends */
#include <sys/hwgraph.h>
#include <sys/nodemask.h>
#include <sys/SN/agent.h>
#include <sys/SN/memsupport.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/SN0/bte.h>
#include <sys/SN/SN0/sn0.h>
#include "sn0_private.h"
#include "../error_private.h"
#ifdef BTE_NOTIFICATION_WAR
#include <sys/clksupport.h>
#endif
#include <ksys/cacheops.h>

extern int disable_bte;
extern int disable_bte_pbzero;
extern int disable_bte_pbcopy;
extern int disable_bte_poison_range;


/* External references and forward declarations */
static bte_result_t bte_error_handler(bte_handle_t *);

/* bte_wait_limit is used to determine whether a requester will spinwait
 * for the current user of the bte to finish their transfer.  If the
 * number of bytes left to transfer is less than bte_wait_limit, the
 * requester will wait.
 */
unsigned bte_wait_limit = 0x1000;

/* How long we spin before checking the Length/Status register again */
#define BTE_WAIT_PERIOD 5

/* BTE is busy if BUSY set */

#define BTE_IS_BUSY(_x) ((_x) & IBLS_BUSY)

/*
 * BTE Locking mechanism. mrlocks are used to allow the bte to be "locked"
 * from a non-threaded  interrupt handler. mrlocks are the only locking 
 * mechanism that we have that do not require a context, even if using
 * trylock, and will allow threaded interrupt handlers or others with a 
 * context to sleep waiting for the lock. To implement exclusive locks, 
 * we always lock for update.  
 */
#define	BTE_TRYLOCK(_b) 	mrtryupdate(&(_b)->bte_lock)
#define	BTE_UNLOCK(_b)		mrunlock(&(_b)->bte_lock)
#define	BTE_LOCK(_b)		mrupdate(&(_b)->bte_lock)
#define	BTE_ISLOCKED(_b)	mrislocked_update(&(_b)->bte_lock)

/*
 * Each CPU has some BTE specific information in its PDA.
 * This info provides details about the base of the BTE, cache lines
 * used for context, and other workarounds. 
 */

#define	BTE_INFO()		(bteinfo_t *)(private.p_bte_info)
#define	CPU_BTE_INFO(id)	(bteinfo_t *)(pdaindr[id].pda->p_bte_info)

#define BTE_LOAD(_b, _x) \
	HUB_L((volatile __uint64_t *)(_b + (_x)))

#define BTE_STORE(_b, _x, _y) \
	HUB_S((volatile __uint64_t *)(_b + (_x)), (_y))

#define	BTE_INTR_VECTOR(bit, cpu) (((__uint32_t)bit << 16) | cputocnode(cpu))

#ifdef	BTE_INTR_MODE
static void bte_intr_handler(bteinfo_t *);
#endif

static int bte_do_transfer(bte_handle_t *, paddr_t, paddr_t, unsigned, 
			   unsigned);

#ifdef	BTE_INTR_MODE
#define	BTE_MODE	0
#else
#define	BTE_MODE	BTE_NORMAL
#endif	/* BTE_INTR_MODE */
/*
 * bte_init()
 *	Initializes the state of the BTE so that it can be used early
 * 	on in the kernel.  This routine (and all of the other BTE routines)
 *	require that the PDA be wired at the private address.
 */


static bteinfo_t *
bte_init(bteinfo_t *bteinfo, __psint_t addr)
{
         bte_handle_t *bh;

	/*
	 * This code assumes that it's running on the node whose
	 * BTE we'll be initializing.
	 * This gets called from cboot() which is run on each cpu.
	 * So, there is no problem with the above assumption.
	 */

	/* Get to an SCACHE aligned boundary */
	if (addr & scache_linemask) 
		addr = (addr + SCACHE_LINESIZE) & ~scache_linemask;

	mrlock_init(&bteinfo->bte_lock, MRLOCK_DEFAULT, "bte", cpuid());

	bh = &bteinfo->bte_bh;		/* For now */
	bh->bh_ctx = (btecontext_t *)addr;
	bh->bh_bte = bteinfo;

	bteinfo->bte_context  = (btecontext_t*) addr;
	bteinfo->bte_zeroline = K0_TO_PHYS(addr + SCACHE_LINESIZE);
	bteinfo->bte_cnode    = cnodeid();
	bteinfo->bte_cpuid    = cpuid();
	bzero((void*) PHYS_TO_K0(bteinfo->bte_zeroline), SCACHE_LINESIZE);

#ifdef	BTE_NOTIFICATION_WAR
	/*
	 * Note down the physical addresses used for BTE oneline transfer
	 */
	ASSERT(IS_KSEG0(addr));
	bteinfo->bte_notif_src= K0_TO_PHYS(addr + 
				(SCACHE_LINESIZE * BTE_SRCCLINE_OFFS));
	bteinfo->bte_notif_targ= K0_TO_PHYS(addr + 
				(SCACHE_LINESIZE * BTE_TARGCLINE_OFFS));
	bteinfo->bte_war_inprogress = 0;
	bteinfo->bte_war_waiting    = 0;
	init_sema(&bteinfo->bte_war_sema, 0, "bte_war", cpuid());
#endif /* BTE_NOTIFICATION_WAR */

	/* Calculate the right base address */
	bteinfo->bte_base = (caddr_t) REMOTE_HUB_ADDR(COMPACT_TO_NASID_NODEID(bteinfo->bte_cnode), IIO_BTE_STAT_0); 

	if (LOCAL_HUB_L(PI_CPU_NUM)) {
		bteinfo->bte_base += IIO_BTE_OFF_1;
		LOCAL_HUB_S(IIO_IECLR, IECLR_BTE1);
		bteinfo->bte_num = 1;
	} else {
		LOCAL_HUB_S(IIO_IECLR, IECLR_BTE0);
		bteinfo->bte_num = 0;
	}
	/*
	 * bte_enabled field is used to check if a BTE can be used.
	 * Idea is, if BTE hangs, we should be able to mark that as
	 * disabled, not use the BTE, but keep the system alive.
	 * This mechanism is not yet implemented.
	 */
	bteinfo->bte_enabled = 1;
	private.p_bte_info   = (void *)bteinfo;

	if (get_hub_chiprev(COMPACT_TO_NASID_NODEID(cnodeid())) >= HUB_REV_2_3)
		bteinfo->bte_bzero_mode = BTE_ZFILL;
	else
		bteinfo->bte_bzero_mode = BTE_MODE;
	return bteinfo;
}

/*
 * Check if btes are available.
 */
int
bte_avail(void)
{
    return(!disable_bte);
}

#ifdef	BTE_INTR_MODE
/*
 * Setup BTE to operate in interrupt driven mode.
 */
void
bte_intrinit(bteinfo_t *bteinfo)
{
	cpuid_t      intr_cpu;
	int	     intr_bit;
	vertex_hdl_t	hub_v;

	hub_v = cnodeid_to_vertex(bteinfo->bte_cnode);

	/*
	 * Setup interrupt handlers.
	 */
	intr_cpu = cpuid();
	intr_bit = intr_reserve_level(intr_cpu, INTRCONNECT_ANYBIT, 
				0, hub_v, "Hub BTE Interrupt");

	if (intr_bit < 0 ){
		/*
		cmn_err(CE_WARN, 
		"bte_lateinit: Failed reserving interrupt level for cnode %d\n",
				bteinfo->bte_cnode);
		*/

		return;
	}

	intr_connect_level(intr_cpu, intr_bit, 0, 
			(intr_func_t)bte_intr_handler, (void *)bteinfo, 0);

	bteinfo->bte_intrbit = intr_bit;
	/* 
	 * Program the bits in BTE register.
	 */
	BTE_STORE(bteinfo->bte_base, BTEOFF_INT, 
			BTE_INTR_VECTOR(intr_bit, intr_cpu));

}
#endif	/* BTE_INTR_MODE */


/*
 * bte_lateinit()
 *	Initializes the BTE on the current CPU and sets up the
 * 	line cache used by bte_getcontext and bte_freecontext.  This
 *	routine assumes that the kernel's dynamic memory allocators
 *	have been configured, so it shouldn't be called much before
 *	main().  
 */

void
bte_lateinit(void)
{
	int	     	num_cachelines = 4;
	/*REFERENCED*/
	caddr_t      	zpage;
	bteinfo_t    	*bteinfo;
	caddr_t		addr;

	num_cachelines += BTE_EXTRA_CLINES;	
	bteinfo = (bteinfo_t *)kmem_zalloc_node(sizeof(bteinfo_t), VM_NOSLEEP, cnodeid());
	ASSERT_ALWAYS(bteinfo);

	addr = kmem_zalloc_node( num_cachelines * SCACHE_LINESIZE,
				VM_DIRECT|VM_CACHEALIGN|VM_NOSLEEP, cnodeid());
	if (!addr){
		return;
	}

	/* Initialize BTE specific info */
	bteinfo = bte_init(bteinfo, (__psint_t)addr);

	/*
	 * Allocate the page used for page_zero.
	 */
	zpage = kmem_zalloc_node(NBPP, VM_DIRECT|VM_CACHEALIGN|VM_NOSLEEP, 
				 cnodeid());
	if (zpage) {
		ASSERT(IS_KSEG0(zpage));
		bteinfo->bte_zpage = K0_TO_PHYS(zpage);
	}

	zpage = kmem_alloc_node(NBPP, VM_DIRECT|VM_CACHEALIGN|VM_NOSLEEP, 
							cnodeid());
	
	if (zpage) {
		ASSERT(IS_KSEG0(zpage));
		bteinfo->bte_poison_target = K0_TO_PHYS(zpage);
	} else {
		cmn_err(CE_PANIC|CE_CPUID,
			"Could not allocate a page for BTE poison transfers");
	}


#ifdef	BTE_INTR_MODE
	bte_intrinit(bteinfo);
#endif	/* BTE_INTR_MODE */

}

#ifdef DEBUG_BTE
int bte_intr_count[MAXCPUS];
int bte_intr_debug;
#endif

void
bte_intr_handler(bteinfo_t *bteinfo)
{
	/*
	 * Gets invoked on operation completion. 
	 * For now, just make the ctx->status to be non-zero
	 * We may like to do more things later.
	 */
	ASSERT(bteinfo);
	/* LOCAL_HUB_CLR_INTR(bteinfo->bte_intrbit); */

#ifdef	DEBUG_BTE
	if (bte_intr_debug)
		printf("bte intr handler: info 0x%x PI_INT_PEND0: 0x%lx count %d src 0x%lx targ 0x%lx\n", 
			bteinfo, 
			HUB_L((volatile __uint64_t *)(0x9200000000000098)),
			bte_intr_count[cpuid()], 
			BTE_LOAD(bteinfo, BTEOFF_SRC), 
			BTE_LOAD(bteinfo, BTEOFF_DEST));
	bte_intr_count[cpuid()]++;
#endif

	bteinfo->bte_context->status = 0;
	bteinfo->bte_intr_count++;
}

/*
 * bte_pbcopy()
 *	bte_pbcopy copies len bytes from the source address src to the 
 *	destination address given in dst.  Both src and dest may be either
 *	Physical or Kphys addresses which map to physical memory.
 *	The routine makes a best-effort to use the BTE; if the BTE is 
 *	not available, it will call bcopy to perform the transfer instead.  
 *	This routine performs a synchronous copy and will not return until the 
 *	copy is complete (unless an error occurs during the transfer).
 */

int
bte_pbcopy(paddr_t src, paddr_t dest, unsigned len)
{				
    int 	mode;

    mode = BTE_MODE;

    if (disable_bte_pbcopy && maxcpus >= disable_bte_pbcopy) {
        bcopy((void*) PHYS_TO_K0(src), (void*) PHYS_TO_K0(dest), len);
        /*
         * shouldn't we make this function void ?
         */
        return 0;
    }

    if (bte_copy(NULL, src, dest, len, mode)) {
	bcopy((void*) PHYS_TO_K0(src), (void*) PHYS_TO_K0(dest), len);
    } else {
#ifdef BTE_CORRUPTION_WAR
	ASSERT(!bcmp((const void *)PHYS_TO_K0(src),
		     (const void *)PHYS_TO_K0(dest), (size_t)len));
#endif
    }
    return(0);
}


/*
 * bte_pbzero()
 *      bte_pbzero fills the memory range [dest, dest+length)
 *	with zeros.  Dest is either a KSEG0 or a PHYSICAL address.  If 
 *	the BTE is not available, bzero will be used instead.  This routine 
 *	synchronously zeros the memory range and will not return until the 
 *	operation is complete (unless an error occurs).
 */

#ifdef BTE_CORRUPTION_WAR
static char zpage[NBPP] = {0};
#endif


int
bte_pbzero(paddr_t dest, unsigned len, int nofault)
{
	/*REFERENCED*/
	int 		status;
	bte_handle_t	*bh;
	int		nofault_save;

	ASSERT(len);

	if (len == 0)
		return 0;

        if (disable_bte_pbzero && maxcpus >= disable_bte_pbzero) {
		if (nofault && curthreadp) {
			nofault_save = curthreadp->k_nofault;
			curthreadp->k_nofault = NF_BZERO;
		}
                bzero((void*) PHYS_TO_K0(dest), len);
		if (nofault && curthreadp) {
			curthreadp->k_nofault = nofault_save;
		}
                return 0;
        }

	/* 
	 * Acquire BTE, and force the routine to run
	 * on the node whose BTE is being used.
	 */

	bh      = bte_acquire(0);
	status  = -1;

	if (bh) {
	        /*
		 * Use bte to do page zero. Way to do it is to 
		 * copy from a source page filled with zeroes to target page. 
		 */
	        paddr_t target = dest;
		do {
			int 	size;
			if (len <= NBPP){
				/* Common code would be inline */
				size = len;
				len = 0;
			} else {
				size = NBPP;
				len -= NBPP;
			}
			status =  bte_do_transfer(bh,
						  bh->bh_bte->bte_zpage,
						  target, size, 
						  bh->bh_bte->bte_bzero_mode);
			target += size;	
		} while (len && (status != -1));
		bte_release(bh);
	}


	if (status == 0) {
#ifdef BTE_CORRUPTION_WAR
		if (len <= NBPP)
			ASSERT(!bcmp(zpage, (const void *)PHYS_TO_K0(dest),
				     (size_t)len));
#endif
		return 0;
	} else if (status == -1) {
		if (nofault && curthreadp) {
			nofault_save = curthreadp->k_nofault;
			curthreadp->k_nofault = NF_BZERO;
		}
		bzero((void*) PHYS_TO_K0(dest), len);
		if (nofault && curthreadp) {
			curthreadp->k_nofault = nofault_save;
		}
		return 0;
	} else {
		/* An error occurred */
		return bte_error_handler(bh);
	}
}


static hubreg_t
bte_wait_for_status(bteinfo_t *bteinfo) 
{
	volatile hubreg_t bte_status;
	volatile btecontext_t *ctx = bteinfo->bte_context;

#ifdef BTE_NOTIFICATION_WAR
	/*
	 * BTE sometimes forgets to write to the memory
	 * location when it is done. Don't wait more than
	 * (WAG) 0.1 seconds for it. If we don't get the 
	 * notification within that time, return from here.
	 * Main code is responsible for taking required action.
	 */
	clkreg_t	stop_RTC;

	stop_RTC = GET_LOCAL_RTC + (CYCLE_PER_SEC / 10);

	/*
	 * Make sure that notification cache line is in "shared state"
	 */
	__dcache_wb_inval((void *)ctx, 128);
	__prefetch_load((void *)ctx, 128);

	do {
	        /* 
		 * us_delay on SN0 reads the HUB anyway, so don't do it 
		 * here.
		 *
		 * us_delay(2);
		 */
		bte_status = ctx->status; 
	} while ((bte_status == -1L) && (stop_RTC > GET_LOCAL_RTC));

#else	/* !NOTIFICATION_WAR */

	while ((bte_status = ctx->status) == -1L)
		/* LOOP */ 
		;
#endif
	return bte_status;

}

#ifdef BTE_NOTIFICATION_WAR

/*
 * Code to workaround the BTE notification problem
 * There are two routines here.
 * bte_oneline_xfer:
 *	Given a context and bte base, it forces bte to do one cacheline
 *	transfer, waits for it to complete (or terminates after waiting
 *	for BTE_NOTIFICATION_TIMEOUT microseconds.
 *
 * bte_notification_war()
 *	Gets called whenever the main bte code notices that BTE has 
 *	missed out on a notification. This code is responsible for
 *	grabbincg control of all the BTEs on the node, and invoke
 *	bte_oneline_xfer on them.
 */

static void bte_notification_war(bteinfo_t *);
static void bte_oneline_xfer(bteinfo_t *);

static void
bte_oneline_xfer(bteinfo_t *bteinfo)
{
	int		i;
	volatile 	hubreg_t x;
	caddr_t 	bte_base;
	btecontext_t	*ctx;

	bte_base = bteinfo->bte_base;
	ctx      = bteinfo->bte_context;

	/* 
	 * Stop the BTE.. 
	 */
	x = BTE_LOAD(bte_base, BTEOFF_CTRL);

	/*
	 * Set status to be -1 
	 */
	ctx->status = -1L;

	/*
	 * Make sure that notification cache line is in "shared state"
	 */
	__dcache_wb_inval((void *)ctx, 128);
	__prefetch_load((void *)ctx, 128);

	/*
	 * Program BTE to do one line transfer.
	 */
	BTE_STORE(bte_base, BTEOFF_SRC, bteinfo->bte_notif_src);
	BTE_STORE(bte_base, BTEOFF_DEST, bteinfo->bte_notif_targ);
	BTE_STORE(bte_base, BTEOFF_STAT, IBLS_BUSY | 1);
	BTE_STORE(bte_base, BTEOFF_NOTIFY, K0_TO_PHYS(ctx));
	BTE_STORE(bte_base, BTEOFF_CTRL, BTE_NORMAL);

	/*
	 * Wait to see if you get any notification.
	 */
	for (i = 0; i < BTE_NOTIFICATION_TIMEOUT; i++) {
		if (ctx->status != -1L) 
			break;
		us_delay(1);
	}
#ifdef	DEBUG_BTE 
	cmn_err(CE_NOTE|CE_PHYSID, 
		"bte_oneline_xfer base_addr 0x%lx status 0x%lx\n", 
			bte_base, 
			BTE_LOAD(bte_base,  BTEOFF_STAT));
#endif 	/* DEBUG_BTE */
}

/*
 * bte_notification_war
 * 	Gets called if BTE gets stuck while doing a transfer.
 * 	Workaround involves, 
 *	- Stop both BTEs from doing transfers.
 *	- Force both BTEs to do one line transfer.
 *	- Wait a fixed duration for each transfer to complete. 
 *	  and then retry original transfer.
 */
static void
bte_notification_war(bteinfo_t *bteinfo)
{
	volatile hubreg_t x;
	caddr_t		bte_base;
	bteinfo_t	*binfo[CPUS_PER_NODE];
	int		i;
	cnodeid_t	cnode;
	cpuid_t	id;

	bte_base = bteinfo->bte_base;
	cnode    = bteinfo->bte_cnode;


	/* Stop our BTE */
	x = BTE_LOAD(bte_base, BTEOFF_CTRL);

	/*
 	 * indicate we are doing weird things. 
	 */
	bteinfo->bte_war_inprogress = 1;
	/*
	 * First drop our lock 
	 */
	ASSERT(BTE_ISLOCKED(bteinfo));
	BTE_UNLOCK(bteinfo);

	/*
	 * Now grab all the locks in order.
	 */

	for (i = 0; i < CPUS_PER_NODE; i++) {
		id = cnode_slice_to_cpuid(cnode, i);
		if (valid_cpuid(id) && (cpu_enabled(id))) {
			binfo[i] = CPU_BTE_INFO(id);
			while (!BTE_TRYLOCK(binfo[i])) 
			    ;
		} else {
			binfo[i] = 0;
		}
	}

	/*
	 * Make all BTEs do one line transfer
	 */
	for (i = 0; i < CPUS_PER_NODE; i++) {
		if (binfo[i]){
			bte_oneline_xfer(binfo[i]);
		}
	}

	/*
	 * release all locks except for the BTE that was passed in.
	 */
	for (i = 0; i < CPUS_PER_NODE; i++) {
		/*
		 * Don't release our lock.
		 */
		if ((binfo[i]) && (bteinfo != binfo[i])){
			BTE_UNLOCK(binfo[i]);
		}
	}

	bteinfo->bte_war_inprogress = 0;
	while (bteinfo->bte_war_waiting) {
	        vsema(&bteinfo->bte_war_sema);
		bteinfo->bte_war_waiting--;
	}

	return;
}

#else
#define	bte_notification_war(x)
#endif

/*
 * bte_copy()
 *	bte_copy performs a low-level copy of len bytes from src to dest.
 *	If the BTE is not available, this routine fails with a return value
 *	of -1.  The mode value specifies whether the transfer is normal
 *	or asynchronous, whether a copy or a replication should occur, and
 *	whether the source should be poisoned.  A btecontext is used to 
 *	keep track of the BTE's state when a transfer completes.  For 
 *	synchronous transfers, the BTE will use the default context 
 *	automatically, thus alleviating the caller of the need to allocate
 *	one explicitly.  Asynchronous transfers require that the user 
 *	allocate their own context, however.
 *  NOTE: We implicitly assume that we won't be preempted during this routine.
 *	Interrupts are okay, but we must not switch to a different process
 *	context.
 */


#ifdef DEBUG_BTE
long bte_debug[MAXCPUS][10];
long bte_count[MAXCPUS];
#define	BTE_DEBUG(i, val)	bte_debug[cpuid()][i] = (long)(val)
#else
#define BTE_DEBUG(i, val)
#endif

static int
bte_do_transfer(bte_handle_t *bh, paddr_t src, paddr_t dest, unsigned len, 
			unsigned mode)
{
	hubreg_t	bte_status;
	caddr_t		bte_base;
	bteinfo_t	*bteinfo;
	btecontext_t	*ctx;

	bteinfo = bh->bh_bte;
	ctx     = bh->bh_ctx;

	ASSERT(bteinfo);
	ASSERT(len >= BTE_LEN_MINSIZE);
	
	bte_base = bteinfo->bte_base;

	/* Make sure that the caller isn't confused */
	src  &= TO_PHYS_MASK;
	dest &= TO_PHYS_MASK;

	/*
	 * Assume we are NOT operating in Async mode.
	 * Supporting Async mode requires changes in other parts of
	 * the code, and it's not ready yet. So, instead of having
	 * just a single check here, let's not bother about it,
	 * and fix it when we need it. 
	 */

	ASSERT((BTE_IS_ASYNC(mode) == 0));
	ASSERT(BTE_ISLOCKED(bteinfo));

	BTE_DEBUG(0, GET_LOCAL_RTC);

#ifdef  BTE_NOTIFICATION_WAR
	bteinfo->bte_retry_count = 0;
#endif

retry_bteop:
	ASSERT(bteinfo->bte_cpuid == cpuid());

	ctx->status 	= -1L;

#ifdef  BTE_NOTIFICATION_WAR
	/*
	 * Force the line to be shard in the caches.
	 */
	__dcache_wb_inval((void *)ctx, 128);
	__prefetch_load((void *)ctx, 128);
#endif /* BTE_NOTIFICATION_WAR */


	/* Now program the device and start the transfer */
	BTE_STORE(bte_base, BTEOFF_SRC, src);
	BTE_STORE(bte_base, BTEOFF_DEST, dest);
	BTE_STORE(bte_base, BTEOFF_STAT, IBLS_BUSY | (len >> BTE_LEN_SHIFT));
	BTE_STORE(bte_base, BTEOFF_NOTIFY, K0_TO_PHYS(ctx));
	BTE_STORE(bte_base, BTEOFF_CTRL, mode & BTE_HW_MODE_MASK); 

	BTE_DEBUG(1, src);
	BTE_DEBUG(2, dest);
	BTE_DEBUG(3, len);
	BTE_DEBUG(4, ctx);
	BTE_DEBUG(5, mode);
	BTE_DEBUG(6, GET_LOCAL_RTC);

	bte_status = bte_wait_for_status(bteinfo);

	ASSERT(bteinfo->bte_cpuid == cpuid());

	BTE_DEBUG(7, (long)GET_LOCAL_RTC);

	BTE_DEBUG(8, GET_LOCAL_RTC);

#ifdef BTE_NOTIFICATION_WAR
	if (bte_status == -1L) {
		bte_status = BTE_LOAD(bte_base, BTEOFF_STAT);
		if (bte_status & IBLS_ERROR) {
		    return(bte_status & IBLS_ERROR);
		} else if (bteinfo->bte_retry_count == 0) {
			bteinfo->bte_retry_stat = bte_status;
		} else if (bteinfo->bte_retry_count == BTE_RETRY_COUNT_MAX) {
			cmn_err(CE_WARN|CE_PHYSID|CE_RUNNINGPOOR,
                                "!BTE for CPU %d stopped. It will be restarted during next system boot.",
				bteinfo->bte_cpuid);
			bteinfo->bte_enabled = 0;

			if (mode & BTE_POISON) {
				poison_state_clear(btoc(src));
			}
			return -1;
		}

		bteinfo->bte_retry_count++;

		/* 
		 * Status still being -1 indicates that we hit the 
		 * BTE notification bug. Work around that.
		 */
		bte_notification_war(bteinfo);
		if (mode & BTE_POISON){
			poison_state_clear(btoc(src));
		}
		goto retry_bteop;
	}
#else /* !BTE_NOTIFICATION_WAR */
	if (bte_status == -1) {
		cmn_err(CE_PANIC|CE_PHYSID, 
			"BTE Failed to complete transfer\n");
	}
#endif /* BTE_NOTIFICATION_WAR */

	bteinfo->bte_xfer_count++;
	ASSERT(bteinfo->bte_cpuid == cpuid());
	return (bte_status & IBLS_ERROR);
}

int
bte_copy(bte_handle_t *bh_in, paddr_t src, paddr_t dest, unsigned len, 
	 unsigned mode)
{
    int		bte_status;
    bte_result_t	r;
    bte_handle_t	*bh;

#ifndef SN0_USE_BTE
    cmn_err(CE_PANIC, "SN0_USE_BTE must be defined to call bte_copy");
#endif

    ASSERT(BTE_CHECKPARMS(src, dest, len));
    ASSERT(src && dest);
    ASSERT(IS_KSEG0(src) || IS_KPHYS(src));
    ASSERT(IS_KSEG0(dest) || IS_KPHYS(dest));

    if (disable_bte && maxcpus >= disable_bte) {
	return(BTEFAIL_NOTAVAIL);
    }

    if (len == 0) {
	return(BTE_SUCCESS);
    }

    if (!bh_in) {
	bh = bte_acquire(0);
    } else {
	bh = bh_in;
    }

    if (bh) {
	bte_status =  bte_do_transfer(bh, src, dest, len, mode);
	if (0 == bte_status) {
	    r = BTE_SUCCESS;
	} else if ((-1 != bte_status) && (bte_status & IBLS_ERROR)) {
	    r = bte_error_handler(bh);
	} else {
	    r = BTEFAIL_NOTAVAIL;
	}
	ASSERT(!BTE_IS_BUSY(BTE_LOAD(bh->bh_bte->bte_base, 
				     BTEOFF_STAT)));
	if (!bh_in) {
	    bte_release(bh);
	}
    } else {
	r = BTEFAIL_NOTAVAIL;
    }

    return r;
}

/*
 * bte_rendezvous()
 *	Called to rendezvous with a previously initiated asynchronous
 *	transfer.  If mode is BTERNDVZ_SPIN, the routine will spin until
 *	the transfer completes.  If mode is BTERNDVZ_POLL, the routine
 *	returns -1 if the transfer is not yet complete, 0 otherwise.
 */

int
bte_rendezvous(bte_handle_t *h, unsigned mode)
{
        btecontext_t *ctx = h->bh_ctx;

	switch (mode) {
	case BTERNDVZ_SPIN:
		while (ctx->status == -1) { /* LOOP */ }
		/* NOBREAK */
	case BTERNDVZ_POLL:
		if (ctx->status & IBLS_ERROR) 
			bte_error_handler(h);
		break;
	default:
		ASSERT(0);
		return -1;
	}
	return ctx->status;
}

#ifdef	BTE_ASYNC_MODE

/*
 * bte_getcontext
 *	Acquires a 128-byte cache-aligned piece of memory and returns
 *	a pointer to it (in the form of a btecontext_t*).  In order
 *	to avoid ridiculous amounts of overhead we maintain a small
 *	per cpu cache of contexts.
 *
 * XXX  Ideally, we'd like to maintain a cache of free contexts,
 * HACK	(or use a zone allocator), since calling kern_malloc()
 *	is too expensive.  Unfortunately, we don't have NUMA-ized
 *	zone routines (yet), and we don't have a node-private data 
 *	structure, so there isn't much I can do.
 */

btecontext_t*
bte_getcontext(void)
{
	btecontext_t* new;
	int s;
	bteinfo_t	*info = BTE_INFO();

	if (info->bte_freectxt) {
		/* Protect against an interrupt */
		s = splhi();
		new = (btecontext_t*) info->bte_ctxcache;
		ASSERT(new != NULL);
		info->bte_ctxcache = new->next;
		info->bte_freectxt--;
		splx(s);
		return new;
	} else {
		/* This is where we really need NUMA */
		ASSERT(info->bte_ctxcache == NULL);
		new = (btecontext_t*) kmem_alloc_node_hint(SCACHE_LINESIZE, 
					VM_DIRECT|VM_CACHEALIGN, info->bte_cnode);
		ASSERT(!(~scache_linemask & (__psunsigned_t) new));
		return new;
	} 
}


/*
 * bte_freecontext
 *	Releases the context previously allocated by bte_getcontext().
 *	In reality, we maintain a small cache of allocated lines, 
 *	so if the cache isn't already full we simply enqueue the freed
 *	line rather than deallocating it. 
 */

void
bte_freecontext(btecontext_t* ctx)
{
	int s;
	bteinfo_t *info = BTE_INFO();

	if (info->bte_freectxt > 3) {
		kmem_free(ctx, SCACHE_LINESIZE);
	} else {
		s = splhi();
		ctx->next = info->bte_ctxcache;
		info->bte_ctxcache = ctx;
		info->bte_freectxt++;
		splx(s);
	}
}

#endif	/* BTE_ASYNC_MODE */

/*
 * bte_acquire
 * Acquire the bte for copying
 * Parameters: 
 *	wait  : if true, we sleep waiting for the BTE to become free. 
 * Return Values:
 *        0	  : Could not acquire
 *        non-zero: Acquired
 * As part of acquiring, it forces the thread to run on a specific 
 * CPU (cpu which executes rt_pin_thread), and then we acquire
 * the BTE that belongs to that CPU.
 * So, once we acquire BTE, the thread which holds the BTE is forced to
 * run on the CPU to which the BTE belongs.
 *
 * If we are on interrupt stack, there is no need to pin down the thread.
 * it happens automatically.
 */
bte_handle_t *
bte_acquire(int wait)
{
	bteinfo_t	*bteinfo;
	void		*cookie;
	int		pinned = 0;
	bte_handle_t	*bh;
#if BTE_ASYNC_MODE
	caddr_t		bte_base;
	hubreg_t	bte_status;
#endif


	if (disable_bte && maxcpus >= disable_bte)
		return 0;
	/*
 	 * don't call rt_pin_thread if you are on interrupt stack.
	 */
	if (private.p_kstackflag != PDA_CURINTSTK){
		cookie = rt_pin_thread();
		pinned = 1;
        } else {
	        ASSERT(!wait);
	}

	bteinfo = BTE_INFO();
	ASSERT(bteinfo);
	ASSERT(bteinfo->bte_cpuid == cpuid());

	/* Grab the lock */
	if (wait) {
	        BTE_LOCK(bteinfo);
	} else {
	    if (!BTE_TRYLOCK(bteinfo)) {
	        if (pinned) {
			rt_unpin_thread(cookie);
		}
		return 0;
	    }
	}
	/* If BTE is not enabled, or Workaround in progress, return */
	if (bteinfo->bte_war_inprogress || (bteinfo->bte_enabled == 0)){
	        /*
		 * If we want to wait for the BTE, dont' return NULL, but
		 * wait. Returning NULL implies pure badness.
		 */
	        if (wait && bteinfo->bte_enabled) {
		        while (bteinfo->bte_war_inprogress) {
			        bteinfo->bte_war_waiting++;
				BTE_UNLOCK(bteinfo);
				(void)psema(&bteinfo->bte_war_sema, 0);
				BTE_LOCK(bteinfo);
			 }
		} 
		if (!wait || !bteinfo->bte_enabled) {
		        if (pinned) 
			        rt_unpin_thread(cookie);
		        BTE_UNLOCK(bteinfo);
		        return 0;
		}
	}

	bteinfo->bte_thread_pinned = pinned;
	bteinfo->bte_cookie = cookie;

#ifdef 	BTE_ASYNC_MODE
	/*
	 * Following check is needed, ONLY if use of BTE in async mode
	 * is supported. Support for BTE in async mode does not exist 
	 * at this time. WE will re-enable this code once the support 
	 * for Async mode exists. At this time, it's a waste of 
	 * important cycles. 
	 */
	bte_base = bteinfo->bte_base;
retry_check:
	/* Now check to see if the BTE is in use */
	bte_status = BTE_LOAD(bte_base, BTEOFF_STAT);
	if (BTE_IS_BUSY(bte_status)) {
		/* Is it worth waiting for the current transfer to
		 * complete ?
		 */
		if ((bte_status & IBLS_LENGTH_MASK) < bte_wait_limit) {
			/* YES: Wait for awhile and check again. */
			us_delay(BTE_WAIT_PERIOD);
			goto retry_check;
		} else {
			/* NO: We'll have to spin too long.  Give up. */ 
			bte_release(&bteinfo->bte_handle);
			return 0;
		}
	}
#endif	/* BTE_ASYNC_MODE */

	ASSERT(!BTE_IS_BUSY(BTE_LOAD(bteinfo->bte_base, BTEOFF_STAT)));
	ASSERT(BTE_ISLOCKED(bteinfo));

	/* Initialize our handle */

	bh = &bteinfo->bte_bh;
	bh->bh_error  = 0;

	return(bh);
}

/*
 * Release a bte acquired using bte_acquire.
 * To be used in case of error when migrating
 * pages, before bte_poison_copy is called.
 */
void
bte_release(bte_handle_t *bh)
{
	bteinfo_t	*bteinfo = bh->bh_bte;

	ASSERT(BTE_ISLOCKED(bteinfo));
	ASSERT(!bteinfo->bte_enabled || 
	       !BTE_IS_BUSY(BTE_LOAD(bteinfo->bte_base, BTEOFF_STAT)));
	if (bteinfo->bte_thread_pinned) {
		ASSERT(bteinfo->bte_cpuid == cpuid());
		rt_unpin_thread(bteinfo->bte_cookie);
		bteinfo->bte_thread_pinned = 0;
	}
	BTE_UNLOCK(bteinfo);
}


/****************************************************************************** 
 *                         BTE support for page migration                      *
 ******************************************************************************/

/*
 * bte_poison_copy
 * Use a pre-acquired bte to do a copy operation, and
 * release it at the end.
 * Return Values:
 *        0: success
 *        x: failure
 */
#ifdef	DEBUG_BTE
__psunsigned_t	bte_poison_addr[1024];
int	bte_poison_index;
#define	BTE_POISON_LOG(p)	bte_poison_addr[atomicAddInt(&bte_poison_index, 1) % 1024] = (p);
#else
#define	BTE_POISON_LOG(p)
#endif

int
bte_poison_copy(bte_handle_t *bh, paddr_t src, paddr_t dest, unsigned len, 
		unsigned mode)
{
	int		bte_status;

	ASSERT(BTE_CHECKPARMS(src, dest, len));
	ASSERT((BTE_IS_ASYNC(mode) == 0));

	ASSERT(src && dest);
	ASSERT(IS_KSEG0(src) || IS_KPHYS(src));
	ASSERT(IS_KSEG0(dest) || IS_KPHYS(dest));

	ASSERT(BTE_ISLOCKED(bh->bh_bte));

	if (len == 0)
		return 0;

	/* 
	 * Select appropriate context, based on mode. We assume
	 * that an asynchronous transfer user will provide their own context.
	 */

	mode |= BTE_MODE;

	bte_status =  bte_do_transfer(bh, src, dest, len, mode);
	if (bte_status == 0) 
		BTE_POISON_LOG(src);

	if (bte_status != 0) {
		cmn_err(CE_WARN+CE_CPUID,
			"bte_poison_copy: Returning error status %d",
				bte_status);
	}

	return bte_status;
}

/*
 * bte_poison_range
 * 	Poison the addresses starting at 'src' and for 'len' bytes.
 *	both src and len are assumed to be cacheline aligned.
 *	For now, len is assumed to be less than NBPP. 
 */
int
bte_poison_range(paddr_t src, unsigned len)
{
	int 		status;	
	int 		retval;
	int 		mode;
	unsigned int	tlen;
	bte_handle_t	*bh;


	if (len == 0)
		return 0;


        if (disable_bte_poison_range && maxcpus >= disable_bte_poison_range) {
                poison_state_alter_range(PHYS_TO_K0(src), len, 1);
                return 0;
        }


	mode = BTE_POISON|BTE_MODE;

	bh = bte_acquire(0);

	if (!bh) {
		/*
		 * Not able to acquire BTE indicates either BTE is busy
		 * or it's been disabled for some reason. 
		 * Use backdoor mecahnism to do this.
		 */
#if 0
		cache_operation((caddr_t)PHYS_TO_K0(src), len, 
				CACH_DCACHE|CACH_WBACK|CACH_INVAL|CACH_FORCE);
#endif 
		poison_state_alter_range(PHYS_TO_K0(src), len, 1);
		return 0;
	}

	for (status = 0; (status == 0) && len; len -= tlen, src += tlen) {
	    tlen = (len > NBPP) ? NBPP : len;
	    status = bte_do_transfer(bh, src, bh->bh_bte->bte_poison_target,
				     tlen, mode);
	}

	if (status == 0) {
		retval = 0;
	} else {
		/* An error occurred */
		retval = bte_error_handler(bh);
	}
	bte_release(bh);

	return retval;

}

/*
 * bte_disable
 * Stop using BTE. 
 * This is a big hammer workaround needed in case BTE doesnot
 * work with Graphics.
 */
void
bte_disable(void)
{
	disable_bte = 1;
}

/************************************************************************
 *									*
 * 			 BTE ERROR RECOVERY				*
 *									*
 * Given a BTE error, the node causing the error must do the following: *
 *    a) Clear all crbs relating to that BTE				*
 *		1) Read CRBA value for crb in question			*
 *		2) Mark CRB as VALID, store local physical 		*
 *		   address known to be good in the address field	*
 *		   (bte_notification_targ is a known good local		*
 *		    address).						*
 *		3) Write CRBA						*
 *		4) Using ICCR, FLUSH the CRB, and wait for it to 	*
 *		   complete.						*
 *		... BTE BUSY bit should now be clear (or at least 	*
 *		    should be after ALL CRBs associated with the 	*
 *		    transfer are complete.				*
 *									*
 *    b) Re-enable BTE							*
 *		1) Write IMEM with BTE Enable + XXX bits
 *		2) Write IECLR with BTE clear bits
 *		3) Clear IIDSR INT_SENT bits.
 *									*
 ************************************************************************/

static bte_result_t
bte_error_handler(bte_handle_t *bh)
/*
 * Function: 	bte_error_handler
 * Purpose:	Process a BTE error after a transfer has failed.
 * Parameters:	bh - bte handle of bte that failed.
 * Returns:	The BTE error type.
 * Notes:
 */
{
    vertex_hdl_t	hub_v;
    hubinfo_t		hinfo;
    int			il;
    hubreg_t		iidsr, imem, ieclr;

    /* 
     * Process any CRB logs - we know that the bte_context contains
     * the BTE completion status, but to avoid a race with error
     * processing, we force a call to pick up any CRB errors pending. 
     * After this call, we know that we have any CRB errors related to 
     * this BTE transfer in the context.
     */
    hub_v = cnodeid_to_vertex(bh->bh_bte->bte_cnode);
    hubinfo_get(hub_v, &hinfo);
    (void)hubiio_crb_error_handler(hub_v, hinfo);

    /* Be sure BTE is stopped */

    (void)BTE_LOAD(bh->bh_bte, BTEOFF_CTRL);
    /*	
     * Now clear up the rest of the error - be sure to hold crblock 
     * to avoid race with other cpu on this node.
     */
    il = mutex_spinlock_spl(&hinfo->h_crblock, splerr);
    imem = REMOTE_HUB_L(hinfo->h_nasid, IIO_IMEM);
    ieclr = REMOTE_HUB_L(hinfo->h_nasid, IIO_IECLR);
    if (bh->bh_bte->bte_num == 0) {
	imem |= IIO_IMEM_W0ESD | IIO_IMEM_B0ESD;
	ieclr|= IECLR_BTE0;
    } else {
	imem |= IIO_IMEM_W0ESD | IIO_IMEM_B1ESD;
	ieclr|= IECLR_BTE1;
    }

    REMOTE_HUB_S(hinfo->h_nasid, IIO_IMEM, imem);
    REMOTE_HUB_S(hinfo->h_nasid, IIO_IECLR, ieclr);

    iidsr  = REMOTE_HUB_L(hinfo->h_nasid, IIO_IIDSR);
    iidsr &= ~IIO_IIDSR_SENT_MASK;
    iidsr |= IIO_IIDSR_ENB_MASK;
    REMOTE_HUB_S(hinfo->h_nasid, IIO_IIDSR, iidsr);
    mutex_spinunlock(&hinfo->h_crblock, il);
    ASSERT(!BTE_IS_BUSY(BTE_LOAD(bh->bh_bte->bte_base, BTEOFF_STAT)));

    switch(bh->bh_error) {
    case IIO_ICRB_ECODE_PERR:
	return(BTEFAIL_POISON);
    case IIO_ICRB_ECODE_WERR:
	return(BTEFAIL_PROT);
    case IIO_ICRB_ECODE_AERR:
	return(BTEFAIL_ACCESS);
    case IIO_ICRB_ECODE_TOUT:
	return(BTEFAIL_TOUT);
    case IIO_ICRB_ECODE_XTERR:
	return(BTEFAIL_ERROR);
    case IIO_ICRB_ECODE_DERR:
	return(BTEFAIL_DIR);
    case IIO_ICRB_ECODE_PWERR:
    case IIO_ICRB_ECODE_PRERR:
	/* NO BREAK */
    default:
	cmn_err(CE_WARN+CE_CPUID, "BTE failure (%d) unexpected\n", 
		bh->bh_error);
	return(BTEFAIL_ERROR);
    }
}

void
bte_crb_error_handler(vertex_hdl_t hub_v, int btenum, 
		      int crbnum, ioerror_t *ioe)
/*
 * Function: 	bte_crb_error_handler
 * Purpose:	Process a CRB for a specific HUB/BTE
 * Parameters:	hub_v	- vertex of hub in HW graph
 *		btenum	- bte number on hub (0 == a, 1 == b)
 *		crbnum	- crb number being processed
 * Notes: 
 *	This routine assumes serialization at a higher level. A CRB 
 *	should not be processed more than once. The error recovery 
 *	follows the following sequence - if you change this, be real
 *	sure about what you are doing. 
 *
 */
{
        hubinfo_t	hinfo;
	bteinfo_t	*bte;
	icrba_t		crba; 
	nasid_t		n;
	nodepda_t	*np;
	cpuid_t		cpu;

	hubinfo_get(hub_v, &hinfo);

	/* Find proper cpu/bte. */

	np = hinfo->h_nodepda;
	for (cpu = np->node_first_cpu; 	     
	     cpu < np->node_first_cpu + np->node_num_cpus; cpu++) {
	    bte = (bteinfo_t *)CPU_BTE_INFO(cpu);
	    if (bte->bte_num == btenum) {
		break;
	    }
	}

	ASSERT(bte->bte_num == btenum);

	/*
	 * The caller has already figured out the error type, we save that
	 * in the bte handle structure for the using thread to use.
	 */
	bte->bte_bh.bh_error = ioe->ie_errortype;
	/*
	 * Be sure waiting CPU sees completion.	
	 */
	bte->bte_bh.bh_ctx->status = IBLS_ERROR;
	n = hinfo->h_nasid;
	
	/* Step 1 */
	crba.reg_value = REMOTE_HUB_L(n, IIO_ICRB_A(crbnum));
	/* Step 2 */
	crba.a_addr = (__uint64_t)bte->bte_notif_targ >> 3; /* XXX: magic # */
	crba.a_valid=1;

	/* Zero error and error code to prevent error_dump complaining
	 * about these CRBs. 
	 */
	crba.a_error=0;
	crba.a_ecode=0;

	/* Step 3 */
	REMOTE_HUB_S(n, IIO_ICRB_A(crbnum), crba.reg_value);
	/* Step 4 */
	REMOTE_HUB_S(n, IIO_ICCR, 
		     IIO_ICCR_PENDING | IIO_ICCR_CMD_FLUSH | crbnum);
	while (REMOTE_HUB_L(n, IIO_ICCR_PENDING) & IIO_ICCR_PENDING)
	    ;
}

/*
 * This code is expected to be while under splhi (
 * Called as part of IFDR WAR 
 * Routine does not expect to be interrupted as it's in splhi
 * and is being invoked as part of the HUB_IFDR_WAR
 */

#define	BTE_WAIT_TIME	500

void
bte_wait_for_xfer_completion(void *info)
{
	bteinfo_t	*bteinfo = (bteinfo_t *)info;
	caddr_t		bte_base = bteinfo->bte_base;
	clkreg_t	stop_RTC;
	hubreg_t	size;

	ASSERT(issplhi(getsr()));
	stop_RTC = GET_LOCAL_RTC + BTE_WAIT_TIME;

	size = BTE_LOAD(bte_base, BTEOFF_STAT) & 0xffff;

	while (size) {
		size = BTE_LOAD(bte_base, BTEOFF_STAT) & 0xffff;
		if (stop_RTC < GET_LOCAL_RTC)
			break;
	}

	if (size == 0)
		return;

	cmn_err(CE_WARN|CE_PHYSID, 
		"II_IFDR_WAR: BTE running after %d usecs wait. Remaining Length 0x%x",
		BTE_WAIT_TIME, size << 7);

	/* 
	 * We return from here in this case. What should hopefully happen
	 * is, the IFDR WAR will complete. BTE NOTIFICATION WAR should 
	 * then kick in, and fix this problem.
	 */

}

/*
 * Force the BTEs to be stalled so we can avoid the hub Bpush bug..
 * This requires forcing BTE to push data to a bogus address, and 
 * leave it in error state..
 *
 * Algorithm:
 *	One CPU on each node will do this. 
 *		CPU 0 will do this for all headless nodes. 
 *	Force BTE0 to Pull from a bad address and push to a good address.
 *	Force BTE1 to pull from good address  and push to same bad address. 
 *	Wait for the CRBs to timeout. 
 *	Mark the CRB corresponding to the bad address as Valid. 
 *		We also clear the error code, error, and Mark fields from 
 *		CRB to avoid error_dump printing about this error. 
 *	Invoke the bte_error_handler to do the cleanup job.
 *
 *	Refer to Bug #498404 for greater details. 
 * 
 */
#define	BTE_STALL_BADADDR	(__uint64_t)0xff00000000LL
/*
 * Mask of CPUs that have done the bte stalling. 
 * Used to make sure only on cpu on a node does the
 * BTE stalling job. 
 * Note that 'node' is not necessarily the node where this
 * CPU resides. 'node' could correspond to a headless node.
 */
cnodemask_t	bte_stall_node_mask;

void
bte_stall_node(cnodeid_t node)
{
	paddr_t		bte_stall_memaddr;
	char		*bte_stall_mem;
	paddr_t		notify_addr;
	nasid_t		nasid;
	hubreg_t	btebase_a, btebase_b;
	hubreg_t	idsr;
	volatile 	hubreg_t x;
	bteinfo_t	*bteinfo;
	int		i;
	int		maxdelay;
	cnodemask_t	oldmask;
	cnodemask_t	mymask;
	
	/* Do not disable if all the Hubs are Rev 5 or greater */
	if (verify_snchip_rev() >= HUB_REV_2_3)
		return;

	/* If there are no routers in the system, there is no easy 
	 * way to stall the BTE. Don't do anything for now..
	 */
	if (nodepda->num_routers == 0) {
		return; 
	}

	CNODEMASK_CLRALL(mymask);
	CNODEMASK_SETB(mymask, node);

	/* Now set our mask in bte_stall_cpus, and see who wins battle. */
	CNODEMASK_ATOMSET_MASK(oldmask, bte_stall_node_mask, mymask);

	if (CNODEMASK_TSTB(oldmask, node)){
		/* Other CPU raced us. Let them do it. */
		return;
	}

#ifdef	BTE_STALL_DEBUG
	printf("cpu %d stalling BTEs on node %d\n", cpuid(), node);
#endif

	/*
	 * BTE Stalling Algorithm...
	 */

	bte_stall_mem     = kmem_alloc_node((SCACHE_LINESIZE * 4), 
				VM_CACHEALIGN|VM_DIRECT|VM_NOSLEEP, node);
	bte_stall_memaddr = TO_PHYS((paddr_t)bte_stall_mem);
	notify_addr 	  = bte_stall_memaddr + (2 * SCACHE_LINESIZE);

#ifdef	BTE_STALL_DEBUG
	printf("cpu %d Bte stall memory address 0x%x\n", cpuid(), bte_stall_mem);
#endif

	nasid = COMPACT_TO_NASID_NODEID(node);

	/* Disable Hub II interrupt.. */
	idsr = REMOTE_HUB_L(nasid, IIO_IIDSR); 
	REMOTE_HUB_S(nasid, IIO_IIDSR, (idsr & ~IIO_IIDSR_ENB_MASK));
	/* Read it back to make sure it's written */
	x = REMOTE_HUB_L(nasid, IIO_IIDSR);


	/*
	 * If you are on CPU 0, stall BTE 1 first, and then BTE 0.
	 * Otherwise, stall BTE 0 first and then BTE 1. This makes
	 * the error handling code not to cause problems.
	 * This will work even when we are stalling the BTEs on 
	 * headless node.
	 *
	 * btebase_a will point to the BTE that pulls from bad addr and
	 * pushes to valid addr
	 *
	 * btebase_b will point to the BTE that pulls from valid addr and
	 * pushes to bad addr.
	 */
	if (cputoslice(cpuid()) == 0){
		/* 
		 * CPU is slice 0. 
		 * btebase_a will point to BTE 1, and 
	   	 * btebase_b will point to BTE 0.
		 */
		btebase_b = (hubreg_t) REMOTE_HUB_ADDR(nasid, IIO_BTE_STAT_0);
		btebase_a = btebase_b + IIO_BTE_OFF_1;
	} else {
		/* 
		 * CPU is slice 1. 
		 * btebase_a will point to BTE 0, and 
	   	 * btebase_b will point to BTE 1.
		 */
		btebase_a = (hubreg_t) REMOTE_HUB_ADDR(nasid, IIO_BTE_STAT_0);
		btebase_b = btebase_a + IIO_BTE_OFF_1;
	}


	x = BTE_LOAD(btebase_a, BTEOFF_CTRL);
	BTE_STORE(btebase_a, BTEOFF_SRC, BTE_STALL_BADADDR);
	BTE_STORE(btebase_a, BTEOFF_DEST, bte_stall_memaddr);
	BTE_STORE(btebase_a, BTEOFF_STAT, IBLS_BUSY | 1);
	BTE_STORE(btebase_a, BTEOFF_NOTIFY, notify_addr);
	BTE_STORE(btebase_a, BTEOFF_CTRL, BTE_NORMAL); 

	/* Now spin till BTE count goes down to zero.. */
	do {
		us_delay(1);
	} while ((BTE_LOAD(btebase_a, BTEOFF_STAT)  & IBLS_LENGTH_MASK) != 0);

	/* Wait a bit before starting stalling of next bte */
	us_delay(10);

	/* Now go and Wedge BTE 1 */

	x = BTE_LOAD(btebase_b, BTEOFF_CTRL);
	BTE_STORE(btebase_b, BTEOFF_SRC, bte_stall_memaddr);
	BTE_STORE(btebase_b, BTEOFF_DEST, BTE_STALL_BADADDR);
	BTE_STORE(btebase_b, BTEOFF_STAT, IBLS_BUSY | 1);
	BTE_STORE(btebase_b, BTEOFF_NOTIFY, notify_addr);
	BTE_STORE(btebase_b, BTEOFF_CTRL, BTE_NORMAL); 

	/* Now spin till BTE1 complains about error. 
	 * This will indicate that CRB processing is complete 
	 * Watch for potential BTE hangup due to the bugs..
	 */
	/* Wait for a max of 100000 usecs, delay should be > II Timeout.. */
	maxdelay = 100000;	

	do {
		us_delay(1);
		if (--maxdelay == 0)
			break;
	} while ((BTE_LOAD(btebase_b, BTEOFF_STAT)  & IBLS_ERROR) == 0);

	ASSERT((BTE_LOAD(btebase_a, BTEOFF_STAT) & (IBLS_ERROR|IBLS_BUSY)) ==
				(IBLS_ERROR|IBLS_BUSY));
	ASSERT((BTE_LOAD(btebase_b, BTEOFF_STAT) & (IBLS_ERROR|IBLS_BUSY)) ==
				(IBLS_ERROR|IBLS_BUSY));

	/* 
	 * Stop both BTEs by reading the control register. 
	 */
	x = BTE_LOAD(btebase_a, BTEOFF_CTRL);
	x = BTE_LOAD(btebase_b, BTEOFF_CTRL);

	/*
	 * Look through the CRBs to find out the entry that corresponds to 
	 * bad destination, mark it as valid, turn off the error, and 
	 * IMSG field types. 
	 * This code is a rip-off from hubii_crb_error_handler, but we
	 * make a copy since we want to do bad things with that CRB entry.
	 */

	for (i = 0; i < IIO_NUM_CRBS; i++) {
		icrba_t		icrba;
		icrbb_t		icrbb;
		icrbc_t		icrbc;
		__uint64_t	bad_addr;
		icrba.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_A(i));

		if (icrba.a_mark == 0) 
			continue;
		bad_addr = (uint64_t)icrba.a_addr << 3;

		if (bad_addr != BTE_STALL_BADADDR)
			continue;

		icrbc.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_C(i));

		if (!icrbc.c_bteop) {
			/* Error other than what we expected.. ignore it for
			 * now. bte_error_handler will invoke the appropriate
			 * hub error handler to take care of this.
			 */
			continue;
		}

		icrbb.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_B(i));

		/*
		 * Now the bad things we want to do.	
		 * Mark the CRB as valid
		 * Clear the Error bit in CRB_A and IMSG field in CRB_B
		 * This will force the CRB to go unused for the rest
	    	 * of this booting session.
		 */
		icrba.a_valid = 1;
		icrba.a_error = 0;
		icrba.a_mark  = 0;
		icrba.a_ecode = 0;

		icrbb.b_imsg  = 0; /* CRB NOP Message, instead of Timeout */

		REMOTE_HUB_S(nasid, IIO_ICRB_A(i), icrba.reg_value);
		REMOTE_HUB_S(nasid, IIO_ICRB_B(i), icrbb.reg_value);

#ifdef	BTE_STALL_DEBUG
		printf("cpu %d CRB %d has been ..fixed.. on nasid %d\n", 	
					cpuid(), i, nasid);
#endif

	}
	
	/* 
	 * Turn off Busy bits in both BTEs. they can never be cleared now.
	 * by any amount of error handling. 
	 * This write will not clear the Error bits though.
	 */
	BTE_STORE(btebase_a, BTEOFF_STAT, 0);
	BTE_STORE(btebase_b, BTEOFF_STAT, 0);

	ASSERT((BTE_LOAD(btebase_a, BTEOFF_STAT) & (IBLS_ERROR|IBLS_BUSY)) ==
				(IBLS_ERROR));
	ASSERT((BTE_LOAD(btebase_b, BTEOFF_STAT) & (IBLS_ERROR|IBLS_BUSY)) ==
				(IBLS_ERROR));

	/* Enable Hub II interrupt */
	/* Wait to make sure that all error interrupts are posted 
	 * before re-enabling interrupt. This is needed to force
	 * Hub to ignore any posted interrupt 
	 */
	
	REMOTE_HUB_S(nasid, IIO_IIDSR, idsr);
	/* Read it back to make sure it's written */
	x = REMOTE_HUB_L(nasid, IIO_IIDSR);

	for (i = CNODE_TO_CPU_BASE(node); 
		i < (CNODE_TO_CPU_BASE(node) + CNODE_NUM_CPUS(node)); i++) {

		if (pdaindr[i].CpuId == -1) {
			continue;
		}
		
		bteinfo = (bteinfo_t *)pdaindr[i].pda->p_bte_info;
		bteinfo->bte_enabled = 0;
		bte_error_handler(&bteinfo->bte_bh);
	}

	kmem_free(bte_stall_mem, (SCACHE_LINESIZE * 4));


}

/*
 * Routine to decide if the BTEs on a specific Node board
 * need to be disabled. 
 * Btes will be disabled if the rev of Hub on node board 
 * is 2.1 and it's talking to PCI shoebox or PCI shoehorn.
 *
 * return 0 if decision making was ok (either to stall/not-stall)
 * return -1 if there is some problem in doing the war.
 */

extern int bpush_war_enable;

/*
 * These tuenables have the format BOARD_CLASS|BOARD_TYPE
 */
extern int bpush_source1, bpush_source2, bpush_source3, bpush_source4;

#define	BOARD_MATCH(src, tgt)	( (KLCLASS(src) == KLCLASS(tgt)) && \
				  (KLTYPE(src)  == KLTYPE(tgt)) )


int
bte_bpush_war(cnodeid_t node, void *board)
{
	unsigned char	btype;
	lboard_t	*lboard = (lboard_t *)board;
	void		*cookie;

	if (bpush_war_enable == 0)
		return 0;

	if (lboard == NULL)
		return 0 ;

	if (get_hub_chiprev(COMPACT_TO_NASID_NODEID(node)) >= HUB_REV_2_3)
		return 0;

	if (CNODEMASK_TSTB(bte_stall_node_mask, node)){
		/* Already stalled */
		return 0;
	}

	cookie = rt_pin_thread();

	btype = lboard->brd_type;
	if (BOARD_MATCH(btype, bpush_source1) ||
  	    BOARD_MATCH(btype, bpush_source2) ||
  	    BOARD_MATCH(btype, bpush_source3) ||
  	    BOARD_MATCH(btype, bpush_source4)) {

#ifdef	BTE_STALL_DEBUG
		printf("Stalling BTE on node %s which is the master for %s\n", 
			NODEPDA(node)->hwg_node_name, lboard->brd_name);
#endif /* BTE_STALL_DEBUG */
		bte_stall_node(node);
	}

	rt_unpin_thread(cookie);

	return 0;
}

#if BTE_DEBUG_CHECK

int	alloc_size = 0x4000;
int	use_bte = 1;
int	xfer_len = 0x1000;

#include <sys/SN/SN0/bte.h>
void
bcopy_time(void)
{
__uint64_t t1, t2;
char *p1, *p2;
	int s;

        p1 = kmem_alloc_node(alloc_size, VM_NOSLEEP, 0);
        p2 = kmem_alloc_node(alloc_size, VM_NOSLEEP, 1);

        if ((p1 == NULL) || (p2 == NULL)) {
                printf("kmem_alloc failed\n");
                return;
        }

        __dcache_inval(p1, alloc_size);
        __dcache_inval(p2, alloc_size);

	s = splhi();
        t1 = _get_timestamp();

        if (use_bte) {
                if (bte_copy(NULL, (paddr_t)p1,
                        (paddr_t)p2, xfer_len, BTE_NORMAL) < 0)
                        printf("bte_copy: error\n");
        } else
                bcopy(p1, p2, xfer_len);

        t2 = _get_timestamp();
	splx(s);

        printf("src 0x%x dest 0x%x len 0x%x: %d\n", p1, p2, xfer_len, t2 - t1);

        kmem_free(p1, (size_t) alloc_size);
        kmem_free(p2, (size_t) alloc_size);
}

#define	BTE_SRC_PREFETCH_LOAD	0x01
#define	BTE_DST_PREFETCH_LOAD	0x02
#define	BTE_MEM_PREFETCH_LOAD	(BTE_SRC_PREFETCH_LOAD|BTE_DST_PREFETCH_LOAD)

#define	BTE_SRC_PREFETCH_STORE	0x04
#define	BTE_DST_PREFETCH_STORE	0x08
#define	BTE_MEM_PREFETCH_STORE	(BTE_SRC_PREFETCH_STORE|BTE_DST_PREFETCH_STORE)

#define	BTE_SRC_BZERO		0x10
#define	BTE_DST_BZERO		0x20
#define	BTE_MEM_BZERO		(BTE_SRC_BZERO|BTE_DST_BZERO)

#define	BTE_SRC_INITIALIZE	0x40
#define	BTE_DST_INITIALIZE	0x80
#define	BTE_MEM_INITIALIZE	(BTE_SRC_INITIALIZE|BTE_DST_INITIALIZE)

#define	BTE_SRC_BITS	\
	(BTE_SRC_PREFETCH_LOAD | BTE_SRC_PREFETCH_STORE | BTE_SRC_BZERO )
#define	BTE_DST_BITS	\
	(BTE_DST_PREFETCH_LOAD | BTE_DST_PREFETCH_STORE | BTE_DST_BZERO )

#define	BTE_DO_CPUACTION	0x10000
/*
 * Make source node a specific  one
 */
#define	BTE_SRCNODE_SPECIFIC	0x100000
#define	BTE_SRCNODE_LOCAL	0x200000

/*
 * Make target node a specific one 
 */
#define	BTE_DSTNODE_SPECIFIC    0x400000
#define	BTE_DSTNODE_LOCAL    	0x800000



int bte_test_mode = (BTE_SRCNODE_LOCAL|BTE_SRC_PREFETCH_LOAD|BTE_DO_CPUACTION|BTE_SRCNODE_SPECIFIC);

int bte_src_node = 1;
int bte_dst_node = 2;


void
bte_test_setup(caddr_t addr, int len, int bte_test_mode)
{
	if (bte_test_mode & BTE_MEM_INITIALIZE) {
		int     i;
		long    *laddr = (long *)addr;

		for (i = 0;  i < (len/sizeof(long *)); i++,laddr++)
			*laddr = (long)laddr;
	} 

	if (bte_test_mode & BTE_MEM_PREFETCH_LOAD) {
		/*
		 * Causes IFDR WAR to be kicked up. But BTE works
		 * Few million xfers
		 */
		__prefetch_load(addr, len);
		return;
	}

	if (bte_test_mode & BTE_MEM_PREFETCH_STORE) {
		/* 
		 * Causes IFDR WAR to get kicked. But BTE works.
		 * Few million xfers
		 */
		__prefetch_store(addr, len);
		return;
	}
	
	if (bte_test_mode & BTE_MEM_BZERO) {
		bte_pbzero(K0_TO_PHYS(addr), len, 0);
		return;
	} 

}

/*ARGSUSED*/
void
bteaction(void *arg1, void *arg2, void *arg3, void *arg4)
{
	caddr_t		srcaddr = (caddr_t)arg1;
	caddr_t		targaddr = (caddr_t)arg2;
	int		len     = (long)arg3;
	volatile int	*done_pointer = (int *)arg4;

	if (bte_test_mode & BTE_SRC_BITS)
		bte_test_setup(srcaddr, len, bte_test_mode);
	if (bte_test_mode & BTE_DST_BITS)
		bte_test_setup(targaddr, len, bte_test_mode);

	*done_pointer = 1;
}

/*
 * Bit field values
 *
 * 0x1 -> Do prefetch load on source address
 * 0x2 -> Do prefetch load on dst address
 * 0x4 -> DO prefetch store on source address
 * 0x8 -> Do prefetch store on target address
 * 0x10 -> Do bte_bzero of source address
 * 0x20 -> Do bte_bzero of target address.
 */

cpuid_t	cpu_used[MAXCPUS];
volatile int cpuaction_done[MAXCPUS];

int
bte_test()
{
	caddr_t		src;
	caddr_t		dst;
	paddr_t		srcp, dstp;
	cnodeid_t	cnode;
	int		i;
	cpuid_t		myid;
	int		retval;
	void		*cookie;
	int		cpu;
	static int	first_time = 1;


	
	if (first_time) {	
		bte_dst_node = (((__uint64_t)GET_LOCAL_RTC )& 0xff) % numnodes;
		bte_src_node = (((__uint64_t)GET_LOCAL_RTC >> 8)& 0xff) % numnodes;
		first_time = 0;
	}
	cookie = rt_pin_thread();

	/*
 	 * Pick a suitable node;
	 */
	if (bte_test_mode & BTE_SRCNODE_LOCAL) 
		cnode = cnodeid();
	else if (bte_test_mode & BTE_SRCNODE_SPECIFIC)
		cnode = bte_src_node; 
	else 
		cnode = (((__uint64_t)GET_LOCAL_RTC >> 8)& 0xff) % numnodes;

	src = kmem_alloc_node(NBPP, VM_DIRECT|VM_NOSLEEP, cnode);
	if (!src){
		rt_unpin_thread(cookie);
		return -1;
	}
	/*
	 * Pick a suitable node for target.
	 */
	if (bte_test_mode & BTE_DSTNODE_LOCAL)
		cnode = cnodeid();
	else if (bte_test_mode & BTE_DSTNODE_SPECIFIC)
		cnode = bte_dst_node;
	else 
		cnode = (GET_LOCAL_RTC & 0xff) % numnodes;

	dst = kmem_alloc_node(NBPP, VM_DIRECT|VM_NOSLEEP, cnode);
	if (!dst){
		rt_unpin_thread(cookie);
		kmem_free(src, NBPP);
		return -1;
	}

	srcp = K0_TO_PHYS(src);
	dstp = K0_TO_PHYS(dst);

	retval = 0;
	cpu = 0;
	for (i = 0; i < 100; i++){
		if (bte_test_mode & BTE_DO_CPUACTION) {
			do {
				cpu = (cpu + 1 ) % maxcpus; 
			} while (pdaindr[cpu].CpuId == -1);
		} else {
			cpu = cpuid();
		}
		myid = cpuid();
		cpu_used[myid] = cpu;
		if (bte_test_mode & BTE_DO_CPUACTION) {
			cpuaction_done[myid] = 0;
			cpuaction(cpu, bteaction, A_NOW,
				(void *)src, 
				(void *)dst, 
				(void *)(long)NBPP,
				(void *)&cpuaction_done[myid]);
			while (cpuaction_done[myid] == 0);
				;

		}

		retval += bte_copy(NULL, srcp, dstp, NBPP, 0x10); 
	}

	kmem_free(src, NBPP);
	kmem_free(dst, NBPP);
	rt_unpin_thread(cookie);
	return retval;

}
#endif /* BTE_DEBUG_CHECK */
