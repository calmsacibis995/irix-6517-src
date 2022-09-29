/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-2000 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.452 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <ksys/behavior.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dump.h>
#include <sys/edt.h>
#include <ksys/fdt.h>
#include <sys/vmereg.h>
#include <sys/invent.h>
#include <sys/kopt.h>
#include <sys/kmem.h>
#include <sys/mac_label.h>
#include <sys/major.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/page.h>
#include <sys/lpage.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/runq.h>
#include <sys/sat.h>
#include <sys/schedctl.h>
#include <sys/stream.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/tlbdump.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/eag.h>
#include <sys/acl.h>
#include <ksys/vsession.h>
#include <ksys/vpgrp.h>
#include <sys/capability.h>
#include <ksys/sthread.h>
#include <ksys/xthread.h>
#include <ksys/pid.h>
#include <ksys/vm_pool.h>
#include <sys/iograph.h>
#include <sys/hwgraph.h>
#if CELL
#include <ksys/cell/wp.h>
#endif
#include <sys/idbgactlog.h>
#include <sys/traplog.h>
#include <ksys/vhost.h>
#ifdef ULI
#include <ksys/uli.h>
#endif
#if CELL
#include <ksys/cell.h>
#include <ksys/cell/relocation.h>
#include <ksys/cell/membership.h>
#endif	/* CELL */
#include <sys/numa.h>
#include <sys/mmci.h>
#include <sys/hwperfmacros.h>
#ifdef IP32
#include <sys/pci_intf.h>
#endif
#include "os/proc/pproc_private.h"
#ifdef _MTEXT_VFS
#include "as/as_private.h"
#include <sys/mtext.h>
#endif /* _MTEXT_VFS */
#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#endif /* _SHAREII */

/* setup by lboot in master.c */
extern char *bootswapfile, *bootdumpfile;
extern u_char	miniroot;

pfn_t		physmem;	/* Physical memory size in clicks.	*/
int		interlvmem;	/* Memory interleave factor used.	*/
pfn_t		maxmem;		/* Physical memory size in clicks.	*/
#ifdef MH_R10000_SPECULATION_WAR
pfn_t           smallk0_freemem;
                                /* count of free "SMALLMEM_K0"          */
                                /* pages (also included in freemem)     */
#endif /* MH_R10000_SPECULATION_WAR */
timespec_t	boottime;

/*
 * Security enabling variables. These are set by their respective init
 * routines, if compiled in. The stubs don't set these....
 */
extern int		sesmgr_enabled;
int		cipso_enabled;
int		mac_enabled;
int		sat_enabled;
int		acl_enabled;
int		cap_enabled;
int		unc_enabled;

/* Daemon kthread stack size - 4k on 32 bit systems, 8k on 64bit systems */
#define DKTSTKSZ	(1024 * sizeof(void *))

extern int	icode[];
extern int	eicode[];
extern int	idata[];
extern int	eidata[];
extern int	picache_size, pdcache_size;
extern int	boot_sdcache_size, boot_sicache_size, boot_sidcache_size;
extern void	strinit(void), strinit_giveback(void);
extern void	binit(void), bsd_init(void), vfsinit(void),
                vfile_init(void), flckinit(void), mbinit(void), siginit(void), 
		vso_initialize(),
                vme_init(void), calloutinit(void), 
                devinit(void), schedinit(void),
		vpgrp_init(void), vsession_init(void), swapinit(void),
		cinit(void), wd93_init(void),
		prfinit(void), utrace_init(void),
		sat_init(void), bdflushearlyinit(void), bdflushinit(void),
		dm_init(void),
		mac_init(void), pager_init(void), sesmgr_init(void),
		inituname(void), mload_init(void), inventinit(void),
		sema_init(void), acl_init(void), ust_init(void),
		uuid_init(void), cap_init(void),
		shaddr_init(void), kmutei_init(void),
		clntkudpxid_init(void), _idbg_late_setup(void),
		dba_init(void), stty_ld_init(void),
		vpag_init(void), device_driver_init(void),
		exit_callback_init(void),
		xthread_init(void),
		enable_ithreads(void),
		kthread_reap_init(void),
		vshm_init(void), as_init(void), ckpt_init(void),
		du_lateinit(void),
		child_pidlist_init(void), init_second(void),
		reginit(void), sysctlr_init(void),
		cell_time_sync(),
#ifdef NUMA_BASE
		init_cached_global_pool(),
#endif
		vhost_init(void), procinit(void), ptpool_init(void);
extern void	init_all_devices(void);
extern void	p0init(void), p0exit(void);
extern void 	xbox_sysctlr_init(void);
#if CELL
extern void	tkc_init(void);
extern void	tks_init(void);
extern void	mesg_init(void);
extern void	cell_up_init(void);
extern void	wp_init(void);
extern void	thread_remote_init(void);
extern void	creds_init(void);
extern void	bla_init(void);
extern void	hb_init(void);
extern void	ucopy_init(void);
extern void	dp_idbg_init(void);
extern void	crs_init(void);
extern void	utility_init(void);
#endif /* CELL */

#if defined(SN0) && defined(SN0_HWDEBUG)
extern void   hub_config_print(void);
#endif

extern void	thread_timein_init(void);
#if EXTKSTKSIZE == 1
extern void	stackext_init(void);
#endif
#ifdef EVEREST
extern void	dma_info_init(void);
#endif
#if SPLMETER
extern void	splmeter_init(void);
#endif
#ifdef	SN0
extern void	iograph_sys_init(void);
extern void	part_init(void);
extern void	xpc_init(void);
#endif
#if defined(EVEREST) && defined(MULTIKERNEL) && defined(CELL)
extern void	part_init(void);
extern void	xpc_init(void);
#endif

/* Certain init functions only get called on the master (golden) cell.
 * Tables that contain functions like this must use this structure.
 */
struct init_tbl_ent {
	void	(*ifunc)(void); /* Init func to be called */
	int	golden_only;	/* Initialize on master (golden) cell only */
};
#define G_ONLY	1	/* golden only */

#if defined(CELL_IRIX)
#define ONLY_ON_GOLDEN(x)                                       \
	if (cellid() == golden_cell) x
#define CALL_INIT_FUNC(_ent) 					\
	if (!_ent->golden_only || cellid() == golden_cell) {	\
		(*_ent->ifunc)();				\
	}
#else
#define ONLY_ON_GOLDEN(x) x 
#define CALL_INIT_FUNC(_ent) (*_ent->ifunc)();
#endif
	
/*
 * early system init - before any I/O is performed
 * interrupts are off for these.
 * The only things in this list should be those necessary to:
 * 1) enable interrupts
 * 2) create threads
 */
static void (*einit_tbl[])(void) = {
	sema_init,		/* general semaphore initializations */
	kmutei_init,		/* general kernel mutex initializations */
	xthread_init,		/* xthread init */
	schedinit,		/* scheduler initialization */
#ifdef MH_R10000_SPECULATION_WAR
        krpf_init,              /* kernel page reference table initialization */
#endif /* MH_R10000_SPECULATION_WAR */
	device_driver_init,	/* initialize device driver infrastructure */
	calloutinit,		/* callout tables */
	inventinit,		/* inventory */
	vme_init,		/* vme ACK init - BEFORE spl lowers */
#if EVEREST || IP20 || IP22 || IP26 || IP28
	wd93_init,		/* to quiet interrupts before enabled */
#endif
	bdflushearlyinit,	/* init bdflush sync semas */
	init_second,		/* initialize once a second stuff */
	clkstart,		/* start clocks */
#if SPLMETER
	splmeter_init,
#endif
	_idbg_late_setup,	/* idbg */
	0,
};

/*
 * system init after interrupts have been enabled.
 * Routines here can create sthreads
 * This is after the really early init, all cpu's have booted, and interrupts
 * are on.
 */
static void (*postintrinit_tbl[])(void) = {
	kthread_reap_init,	/* must occur before any threads exit */
	utrace_init,		/* initialize UTRACE/rtmon subsystem */
	prfinit,		/* debugging */
	thread_timein_init,

	/*
	 * Don't stick anything above thread_timein_init() which requires
	 * timeouts to fire off since timeouts haven't been initialized until
	 * thread_timein_init() completes ...
	 */

	mbinit,			/* mbufs */
	inituname,		/* init uname struct */
	reginit,		/* init reg/vm sema metering */
	ptpool_init,		/* page tables pools */
#if EXTKSTKSIZE == 1
	stackext_init,		/* free pool for kernel stack extension pages*/
#endif
	sat_init,		/* security audit trail init */
	stty_ld_init,		/* fix up stty_ld for POSIX */
#ifdef EVEREST
	dma_info_init,
#endif
#if IP30
	enable_fastclock,
#endif
#if IP27			/* For now, just IP27 */
	part_init,
	xpc_init,		/* X-partition communication */
	sysctlr_init,
	xbox_sysctlr_init,
#endif
#ifndef DONT_USE_MMCI
	pmo_init,		/* init MMCI */
#endif
#if NUMA_BASE
	numa_init,
#elif !DONT_USE_MMCI
	uma_init,
#endif
#ifndef	CELL
	vhost_init,		/* global state services */
#endif
	cap_init,		/* Capability (privilege) Sets */
	mac_init,		/* Mandatory Acc Control for B1/CMW */
#if NUMA_BASE
	init_cached_global_pool,
#endif
	0,
};

/*
 * CELL initialization.
 */
#if CELL

/*
 * Initialization that does not require inter cell communication
 * can be done here. These are init routines needed to initialize
 * cellular communication and setup commn. infrastructure.
 */
static void (*cell_init_tbl[])(void) = {
#if defined(EVEREST) && defined(MULTIKERNEL)
        part_init,              /* Partition initialization */
        xpc_init,               /* X-partition communication */
        dp_idbg_init,
#endif
        mesg_init,              /* IPC init*/
        hb_init,                /* Heart beat initialization */
	utility_init,
        cms_init,               /* Membership services initialization */
	tkc_init,
	tks_init,
	obj_kore_init,		/* kernel object relocation engine */
	wp_init,
	thread_remote_init,	/* remote interruption */
	creds_init,
	bla_init,
	ucopy_init,	
        cell_up_init,           /* Cell up registration */
	vhost_init,		/* global state services */
	cell_time_sync,		/* synchronize with global time */
	crs_init,		/* recovery init */
	0
};

#endif

#if EVEREST /* systems which use mpzduart.c */
#define DU_LATEINIT
#endif

#if IP20 || IP22 || IP26 || IP28 /* zduart.c */
#define DU_LATEINIT
#endif

/*
 * system init after devices have been probed and initted
 */
static struct init_tbl_ent init_tbl[] = {
	strinit,		G_ONLY,	/* streams */
	devinit,		G_ONLY,	/* process config info */
	siginit,		0,	/* ptrace/sigqueue init */
	binit,			0,	/* buffer cache */
	cinit,			0,	/* chunkio init */
	swapinit,		0,	/* swap manager init */
	bhv_global_init,	0,	/* behavior code init */
	vfsinit,		0,	/* init file systems */
	bdflushinit,		0,	/* start flush deamons */
	vpag_init,		0,	/* process aggregate initialization */
	fdt_init,		0,	/* file descriptor table */
	vfile_init,		0,	/* file table */
	flckinit,		0,	/* file locking */
	dba_init,		0,	/* init database accelerator */
	procinit,		0,	/* init process subsystem */
	vpgrp_init,		0,	/* pgrp_init */
	vsession_init,		0,	/* vsession_init */
	exit_callback_init,	0,	/* proc exit callbacks */
	shaddr_init,		0,	/* sproc */
	acl_init,		0,	/* Access Control Lists */
	sesmgr_init,		0,	/* Trusted Networking for B1/CMW */
#ifndef SABLE
	mload_init,		0,	/* dynamic module loader */
#endif
	ust_init,		0,	/* Unadjusted System Time */
	uuid_init,		0,	/* Universal Unique Identifiers */
	dm_init,		0,	/* Data Management subsystem */
	vshm_init,		0,	/* shm init */
#ifdef ULI
	uli_init,		0,	/* user level interrupts */
#endif
	cacheinit,		0,	/* cacheflush system call init */
	vso_initialize,		0,	/* vsock init */
	bsd_init,		G_ONLY,	/* BSD networking initialization */
	strinit_giveback,	G_ONLY,	/* Stream init reclaim timer, globals*/
	as_init,		0,	/* Address Space */
	pager_init,		0,	/* init vhand, shaked */
	child_pidlist_init,	0,	/* process child pidlist */
#if CKPT
	ckpt_init,		0,	/* init ckpt stuff */
#endif
#ifdef DU_LATEINIT
	du_lateinit,		G_ONLY,	/* serial late init */
#endif
	hwperf_init,		0,	/* hardware performance counters */
#ifdef _MTEXT_VFS
	mtext_init,		0,	/* Modified Text Pseudo-VFS	*/
#endif /* _MTEXT_VFS */
	0,			0,
};

#if _MEM_PARITY_WAR || IP26 || IP28
static void parity_init(void);
#endif
static void edt_init(void);
void sd_init(int);
static void initialize_io(void);

extern void	sched(void);
extern void	do_unmount(void);
extern void	async_call_daemon(void);
extern int	addkswap(char *, off_t, off_t, vnode_t **);
extern void	allowboot(void);
extern void	allowintrs(void);
extern void	add_ioboard(void);
extern void	add_cpuboard(void);
extern int	xlv_boot_disk(dev_t);
extern int	tp_dogui(void);
extern void	srandom(int seed);
extern void	coalesced(void);
#ifdef TILES_TO_LPAGES
extern void	tiled(void);
#endif /* TILES_TO_LPAGES */
#if CELL_IRIX
extern void	cell_console_thread(void);
#endif

int
main(void)
{
	register void	(**initptr)(void);
	register struct init_tbl_ent *init_ent_ptr;
	extern void	(*io_init[])(void);
	extern void	(*io_start[])(void);
	extern void	(*io_reg[])(void);
	extern uname_releasename[];
	cpu_cookie_t cookie;
	extern int unmountd_pri;
	extern int async_call_pri;
	extern int thand_pri;
	extern int sched_pri;
	extern int coalesced_pri;
	extern int console_pri;
#ifdef TILES_TO_LPAGES
	extern int tiled_pri;
#endif /* TILES_TO_LPAGES */

	INIT_ACTLOG_LOCK();
	ENABLE_ACTLOG();

	/*
	 * Call all early system initialization functions.
	 * these can't touch i/o yet
	 * interrupts are off
	 */
	for (initptr = &einit_tbl[0]; *initptr; initptr++)
		(**initptr)();

	/*
	 * Let the other processors in to the party.
	 */
	ASSERT(!KT_ISMR(curthreadp));
	curthreadp->k_cpuset = 1;
	cookie = setmustrun(masterpda->p_cpuid);

	allowboot();

	enable_ithreads();

#if (SN0 || IP30)
	{
	extern void start_du_conpoll_thread(void);

	/*
	 * The du_conpoll routine for these machines makes streams calls
	 * that expect a thread context. We need to start up a thread that
	 * calls du_conpoll().
	 */
	start_du_conpoll_thread();
	}
#endif

	allowintrs();
	spl0();

	/*
	 * Call all system initialization functions that can now be run
	 * These can create sthreads
	 * These are called before i/o is initialized..
	 */
	for (initptr = &postintrinit_tbl[0]; *initptr; initptr++)
		(**initptr)();

	{
	    static char *rel_fmt = 
		"!"
#if _MIPS_SIM == _ABI64
		"IRIX Release %s %s Version %s System V - 64 Bit\n"
#else
		"IRIX Release %s %s Version %s System V\n"
#endif
		"%s\n";
	    static char *copyright =
		    "Copyright 1987-2000 Silicon Graphics, Inc.\n"
		    "All Rights Reserved.\n";
#ifdef DEBUG
		if (strlen((char *)uname_releasename) > 0) {
			strcat(utsname.release,"-");
			strcat(utsname.release,(char *)uname_releasename);
		}
#endif
	    if (tp_dogui()) {
		/* This paints in the lower right hand corner of the
		 * screen (and not to the log, because first char is ^).
		 */
		cmn_err(CE_CONT,
		"\\\033[WIRIX Release %s\n%s\033[9Z", utsname.release, copyright);
		/* now write it to the log in normal form */
		cmn_err(CE_CONT, rel_fmt, utsname.release, utsname.machine,
		    utsname.version, copyright);
	    } else /* rel_fmt+1 to skip !, so goes to console and log */
		cmn_err(CE_CONT, rel_fmt+1, utsname.release, utsname.machine,
		    utsname.version, copyright);
	}
#if MP
	if ((__psunsigned_t)&private.p_actionlist & 0x007f)
		cmn_err(CE_WARN,"For maximum performance p_actionlist needs to be aligned to a cacheline boundary (currently at 0x%x)\n",
			&private.p_actionlist);

	if ((__psunsigned_t)&private.p_cacheline_pad1 + sizeof(private.p_cacheline_pad1) & 0x007f)
		cmn_err(CE_WARN,"For maximum performance p_cacheline_pad1 needs to force alignment of NEXT field to a cacheline boundary (currently at 0x%x)\n",
			(__psunsigned_t)&private.p_cacheline_pad1+sizeof(private.p_cacheline_pad1));
#endif /* MP */
#if DEBUG
	cmn_err(CE_CONT, "Running with DEBUG kernel"
#ifdef _COMPILER_VERSION
		" compiled with %d compilers.\n\n", _COMPILER_VERSION
#else
		".\n\n"
#endif
	);
#endif
#if TRAPLOG_DEBUG
	cmn_err(CE_CONT, "Running with TRAPLOG kernel.\n\n");
#endif
#if defined(EVEREST) && defined(MULTIKERNEL)
	{
	  int i;

	  cmn_err(CE_CONT,
		  "Running MULTIKERNEL cell%d with %d total cells.\n\n",
		  evmk_cellid,EVCFGINFO->ecfg_numcells);

	  for (i=0; i<EVCFGINFO->ecfg_numcells; i++)
	  	cmn_err(CE_CONT, "\tcell%d memory base 0x%x size 0x%x\n", i,
			ctob(BLOCs_to_pages(EVCFGINFO->ecfg_cell[i].cell_membase)),
			ctob(BLOCs_to_pages(EVCFGINFO->ecfg_cell[i].cell_memsize))
			);
	}
#endif
#if R10000 && !R4000
	if (_PAGESZ < 0x4000)
		cmn_err(CE_WARN, "Virtual coloring is turned off \n\n");
#endif
#if IP19
	{
	extern int r4k_corrupt_scache_data;

	if (r4k_corrupt_scache_data)
		cmn_err(CE_WARN,"Kernel booted with scache recovery enabled, may corrupt data when cache error occurs\n");
	}
#endif

	if (showconfig) {
		int	cpu_rate = private.cpufreq;
#if R4000 || R10000		/* double frequncy on R4000 and R10000 */
		cpu_rate *= 2;
#endif
		cmn_err(CE_CONT, "Total real memory  = %d kbytes\n"
				 "CPU Frequency = %dMhz\n"
				 "%d CPU(s)\n\n",
			(physmem << (BPCSHIFT - 10)),
			cpu_rate, numcpus);

#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000()) {
			extern pgno_t extk0_pages_added(void);
			uint extk0_pages = extk0_pages_added();

			cmn_err(CE_CONT, "Free small K0 = 0x%x pages ",
				(SMALLMEM_K0_R10000_PFNMAX+1) - pnum(kpbase));
			if (extk0_pages) {
				cmn_err(CE_CONT, "(adding 0x%x pages)\n\n",
					extk0_pages);
			} else
				cmn_err(CE_CONT, "\n\n");
		}
#endif
	}
	_ACL_CONFIGNOTE();
	_MAC_CONFIGNOTE();

#if _MEM_PARITY_WAR || IP26 || IP28
	parity_init();
#endif
	/*
	 * call all I/O init routines (really msg, sem, etc)
	 */
	for (initptr = &io_init[0]; *initptr; initptr++)
		(**initptr)();


	initialize_io();

#if	CELL
	for (initptr = &cell_init_tbl[0]; *initptr; initptr++)
		(**initptr)();
#endif

#if defined(EVEREST) && defined(MULTIKERNEL) && !defined(CELL)
	/* currently the non-golden cells execute correctly up to this
	 * point, which means that the non-master cpus are in resumeidle()
	 * and the master cpu is executing here.
	 * Continuing into the next initialization loop results in a call
	 * to devinit which correctly determines that it can't find root
	 * (actually partitiontospecial() fails).
	 * So for now we get here, signal to golden-cell that we're up
	 * and enter an infinite loop.
	 */
	if (evmk_cellid != evmk_golden_cellid) {

		cmn_err(CE_NOTE, "cell %d is up, spinning for root!\n",
			evmk_cellid);
		DELAY(2000000);
		evmk_cellbootcomplete[evmk_cellid] = 1;
		evmk_replicate( &evmk_cellbootcomplete[evmk_cellid],
				sizeof(int));
		while (1)
			;
	}
#endif /* EVEREST & MULTIKERNEL */

	
	/*
	 * Call all remaining system initialization routines.
	 * This has to be done after io_init because we might use
	 * i/o routines configured above.
	 */
	for (init_ent_ptr = &init_tbl[0]; init_ent_ptr->ifunc; init_ent_ptr++)
		CALL_INIT_FUNC(init_ent_ptr);

	/* call any configured device driver start routines */
	for (initptr = &io_start[0]; *initptr; initptr++)
		(**initptr)();

	/* call any configured device driver register routines */
	for (initptr = &io_reg[0]; *initptr; initptr++)
		(**initptr)();

	if (!miniroot) {
		if (xlv_boot_disk(rootdev)) {
			rootdev = makedev(XLV_MAJOR, 0);
		}
	}


	vfs_mountroot();	/* Mount the root file system */

	if (showconfig) {
                cnodeid_t node;
                
                for (node = 0; node < numnodes; node++) {
                        cmn_err(CE_CONT,
                                "Available memory on node [%d] =  %d kbytes\n\n",
                                node, NODE_FREEMEM(node) << (BPCSHIFT - 10));
                }
		cmn_err(CE_CONT, "Root on device %V (fstype %s)\n",
			rootdev,rootfstype);
	}

	add_ioboard();

	/* Add memory sizes to hardware inventory list,
	 * Compatibility kludge follows:
	 */
	if (((physmem + 1) >> (20 - BPCSHIFT)) < 2048)
		/* Less than 2GB, store the real value. */
		add_to_inventory(INV_MEMORY, INV_MAIN, 0, interlvmem,
				 ctob(physmem));
	else
		/* >= 2GB, store the largest positive 32-bit value. */
		add_to_inventory(INV_MEMORY, INV_MAIN, 0, interlvmem,
				 0x7fffffff);
	/* This one will always be right. */
	add_to_inventory(INV_MEMORY, INV_MAIN_MB, 0, interlvmem,
			 (physmem + 1) >> (20 - BPCSHIFT));

	if (boot_sicache_size != 0)
		add_to_inventory(INV_MEMORY, INV_SICACHE, 0, 0, 
						boot_sicache_size);
	if (boot_sdcache_size != 0)
		add_to_inventory(INV_MEMORY, INV_SDCACHE, 0, 0, 
						boot_sdcache_size);

#if !defined(MP)
	/*
	 * EVEREST/SN0 support mixed caches - for mixed caches, we might
	 * need to put in multiple inventory records - this is 
	 * done in add_cpuboard() for EVERESTs.
	 */
	if (boot_sidcache_size != 0)
		add_to_inventory(INV_MEMORY, INV_SIDCACHE, 0, 0, 
						boot_sidcache_size);
#endif
	add_to_inventory(INV_MEMORY, INV_ICACHE, 0, 0, picache_size);
	add_to_inventory(INV_MEMORY, INV_DCACHE, 0, 0, pdcache_size);
	add_cpuboard();


#if defined (SN0)
        {
        extern void cpu_confchk(void);

        cpu_confchk();
        }
#endif /* SN0 */

#if defined (SN)
	{
	extern void l2_sbe_init(void);

	l2_sbe_init();
	}
#endif /* SN */



        /* Allocate memory area to dump TLB info in case of Panic */
        /* Useful information for coredump analysis */
        tlbdumptbl = (tlbinfo_t *) kvpalloc(
                                btoc(sizeof (tlbinfo_t) * maxcpus),
                                VM_DIRECT,
                                0);
        bzero((caddr_t)tlbdumptbl, sizeof(tlbinfo_t)*maxcpus);

 	restoremustrun(cookie);

	nanotime(&boottime);
	clntkudpxid_init();	/* time must be set */
	srandom(time + boottime.tv_nsec);

	GLOBAL_FREEMEM_UPDATE();

#if DEBUG
	printf("maxmem %d freemem after kernel init %d availrmem %d availsmem %d\n",
	maxmem, GLOBAL_FREEMEM(), GLOBAL_AVAILRMEM(), GLOBAL_AVAILSMEM());
#endif
#if defined(EVEREST) && defined(MULTIKERNEL)
	{
		extern void allowcellboot(void);

		if (!is_specified(arg_evmk_numcells) ||
		    atoi(arg_evmk_numcells) != 0) {
			allowcellboot();
#ifdef CELL
		} else if (cellid() != golden_cell) {

			/* Tell the golden cell we have initialized */

			evmk_cellbootcomplete[evmk_cellid] = 1;
			evmk_replicate( &evmk_cellbootcomplete[evmk_cellid],
				sizeof(int));
#endif
		}

	}
#endif /* EVEREST & MULTIKERNEL */

#if defined(SN0) && defined(SN0_HWDEBUG)
	hub_config_print();
#endif

	/* hand-craft first process that then can start up init, etc. */
	p0init();

	/*
	 * We are the initial sthread - start up all the other
	 * sthreads
	 */
	sthread_create("unmountd", 0, DKTSTKSZ, 0, unmountd_pri, KT_PS,
				(st_func_t *)do_unmount, 0, 0, 0, 0);

	sthread_create("async_call", 0, DKTSTKSZ, 0, async_call_pri, KT_PS,
				(st_func_t *)async_call_daemon, 0, 0, 0, 0);

#ifdef TILES_TO_LPAGES
	sthread_create("tiled", 0, 8096, 0, tiled_pri, KT_PS,
				(st_func_t *)tiled, 0, 0, 0, 0);
#endif
	
	sthread_create("sched", 0, 0, 0, sched_pri, KT_PS,
			(st_func_t *)sched, 0, 0, 0, 0);

	if (large_pages_enable)
		sthread_create("coalesced", 0, 2 * DKTSTKSZ, 0, coalesced_pri,
			 KT_PS, (st_func_t *)coalesced, 0, 0, 0, 0);

#if CELL_IRIX
	sthread_create("console", 0, 0, 0, console_pri, KT_PS,
			(st_func_t *)cell_console_thread, 0, 0, 0, 0);
#endif

#ifdef _SHAREII
	SHR_START();
#endif /* _SHAREII */

	sthread_exit();
	/* NOTREACHED */
}

/*
 * Proc0 - magically arrive here from p0init().
 */
void
p0(void)
{
        int newp = 0;

	ASSERT(curvprocp && curuthread);

	/*
	 * we are proc 0 and can spawn other processes
	 * We can also do proc-specific things....
	 */
	_SAT_PROC_INIT(curuthread, NULL);	/* Setup sat proc info */
	ONLY_ON_GOLDEN(vfs_mounthwgfs());	/* Mount the hwgfs */
	sd_init(1);				/* init swap and dump */
	ONLY_ON_GOLDEN(newp = newproc());
	if (newp) {
		int szicode = (char *)eicode - (char *)icode;
		int szidata = (char *)eidata - (char *)idata;
		as_addspace_t as;
		/* REFERENCED */
		as_addspaceres_t asres;
		vasid_t vasid;

		bcopy("icode", curprocp->p_psargs, 6);
		bcopy("icode", curprocp->p_comm, 5);
		_SAT_SET_COMM(curprocp->p_comm);

		curuthread->ut_pproxy->prxy_syscall = &syscallsw[ABI_ICODE];

		ONLY_ON_GOLDEN(ASSERT(current_pid() == 1));
		curthreadp->k_cpuset = 1;
		/*
		 * Set up the text region to do an exec
		 * of /etc/init.  The "icode" is in ml directory
		 */
		as_lookup_current(&vasid);
		as.as_op = AS_ADD_INIT;
		as.as_addr = (uvaddr_t)USRCODE;
		as.as_length = USRDATA+szidata-USRCODE;
		as.as_prot = PROT_READ|PROT_WRITE|PROT_EXEC;
		as.as_maxprot = as.as_prot;

		(void)VAS_ADDSPACE(vasid, &as, &asres);

		if (copyout(icode, (caddr_t)USRCODE, szicode))
			cmn_err(CE_PANIC, "main: copyout of icode failed");
		cache_operation((caddr_t)USRCODE, szicode, CACH_ICACHE_COHERENCY);
		if (copyout(idata, (caddr_t)USRDATA, szidata))
			cmn_err(CE_PANIC, "main: copyout of idata failed");
		(void)splhi();
		ktimer_init(UT_TO_KT(curuthread), AS_USR_RUN);
		spl0();
		return;
	} 
#ifdef CELL_IRIX
	newp = newproc();
	if (newp) {
		extern void	do_procio(void);
		do_procio();
	} 
#endif
#ifdef NUMA_BASE
	memoryd_init();
#endif        
	p0exit();
	/* NOTREACHED */
}

/*
 * init swap/dump, but only succeed once for each
 */
extern char	*dumpvaddr;
extern char	*dumpdevname;
extern int	dumpsize;
extern sthread_t *dumpsthread;
extern long dumpstack[];

void
sd_init(int in_p0)
{
	int	error;
	daddr_t partblocks, dumpblocks;
	static struct vnode *swapvp = NULL;
	auto struct vnode *dumpvp;
	static int swap_done = 0, dump_done = 0;
	dev_t dev, hwdisk, hwrdisk;
	char devname[MAXDEVNAME];		

	dumpvp = NULL;
	
	if (!dumpsthread) {
		/*
		 * Initialize the dump kthread, once.  Give it max priority,
		 * and make it CPU-specific.  When it runs, we don't EVER want
		 * it descheduled!  (Can't just use sthread_create, since we
		 * don't want it put on the run queue.)
		 */
		dumpsthread = kmem_zalloc(sizeof(*dumpsthread), KM_SLEEP|VM_DIRECT);

		strcpy(dumpsthread->st_name, "dump");
		dumpsthread->st_cred = sys_cred;
		dumpsthread->st_prev = dumpsthread->st_next = NULL;

		kthread_init(ST_TO_KT(dumpsthread), (caddr_t)dumpstack,
					 DUMP_STACK_SIZE, KT_STHREAD, 255, 
					 KT_PS|KT_BIND, NULL);
	}

	if (!swap_done) {
		/* 
		 * Set up initial swap device.
		 *
		 * If we succeed, swapvp will be saved (it's static.)  Then if
		 * the dump device can't be set up later, it can default to swap.
		 */
	  
		if (bootswapfile == NULL)		/* use default if needed */
			bootswapfile = "/dev/swap";
		/*
		 * If anything other then the default name and we are
		 * being called as part of early init, return and
		 * we will be called again later by ioconfig
		 */
		if (in_p0 && strcmp(bootswapfile, "/dev/swap") != 0)
			goto setup_dump;		/* don't try to set up swap until later */

		if (!miniroot) {
			if (error = addkswap(bootswapfile, swplo, nswap, &swapvp))
				cmn_err(CE_WARN, "Failed to add swap file %s error %d",
						bootswapfile, error);
			else {
				swap_done = 1;

				/* Add aliases to swap file in /hw/disk/swap, /hw/rdisk/swap */
				dev = swapvp->v_rdev;
				if ((dev != NODEV) && (swapvp->v_type == VBLK)) {
#ifdef DEBUG
					printf("/hw/disk/swap is alias for %s\n", bootswapfile);
#endif

					hwdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_DISK);
					ASSERT(hwdisk != NODEV);
					hwgraph_vertex_unref(hwdisk);
					error = hwgraph_edge_add(hwdisk,dev,"swap");
					ASSERT(error == GRAPH_SUCCESS || error == GRAPH_DUP);
					error = error;

					dev = blocktochar(dev);
					if (dev != NODEV) {
						hwrdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_RDISK);
						ASSERT(hwrdisk != NODEV);
						hwgraph_vertex_unref(hwrdisk);
						error = hwgraph_edge_add(hwrdisk, dev, "swap");
						ASSERT(error == GRAPH_SUCCESS || error == GRAPH_DUP);
						error = error;	
					}
				}
			}
		}
	}

	setup_dump:
	if (!dump_done) {
		/*
		 * Set up dump device.
		 *
		 * Set to primary swap device if no dumpfile specified (via lboot)
		 * or the specified dumpfile is the same as the primary swapfile.
		 * Set to specified dumpfile otherwise.  If the specified dumpfile
		 * can't be opened, keep dump_done=0 so we will try again next time,
		 * but let dumpdev fall back to the primary swap device until then.
		 */
		if (bootdumpfile && (strcmp(bootdumpfile, bootswapfile) != 0)) {
			/* set to specified dump device */
			if (lookupname(bootdumpfile, UIO_SYSSPACE, 
						   FOLLOW, NULLVPP, &dumpvp, NULL)) {
				if (!in_p0)
					cmn_err(CE_WARN, "Specified dumpfile %s cannot be accessed; defaulting to primary swap device %s\n", bootdumpfile, bootswapfile);
				dumpvp = NULL;
			} else {
				dump_done = 1;		/* found the dump device! */
			}
		}

		/* if no dump device found, set to primary swap device */
		if (!dumpvp)
			dumpvp = swapvp;

		/*
		 * Set up dump device
		 * If device is not VBLK, then set to NODEV
		 */
		if (dumpvp && dumpvp->v_type == VBLK) {
			struct bdevsw *my_bdevsw;

			dumpdev = dumpvp->v_rdev;
			my_bdevsw = get_bdevsw(dumpdev);

			/*
			 * Determine where we are to dump on dumpdev.
			 */
			partblocks = 0;
			if (my_bdevsw && bdstatic(my_bdevsw)) {
				if ((int (*)(void))my_bdevsw->d_size64 != nulldev)
					bdsize64(my_bdevsw, dumpdev, &partblocks);
				else if (my_bdevsw->d_size)
					partblocks = bdsize(my_bdevsw, dumpdev);
			}

			/*
			 * Need to set dumplo to swaplo if we're using swap device.
			 */
			if (dumpvp == swapvp)
				dumplo = swplo;
			else
				dumplo = 0;
			dumpblocks = partblocks - dumplo;
			dumpsize = (dumpblocks << SCTRSHFT) >> BPCSHIFT;

#if _MIPS_SIM != _ABI64
			/*
			 * Allocate one page of k2 space to be used by dump_page() to
			 * compress and dump pages above 512M.  Skip for 64-bit kernels
			 * as they can directly map everything.
			 */
			dumpvaddr = kvalloc(1, 0, 0);
#endif

		} else {
			dumpdev = NODEV;
			dump_done = 0;			/* no dump device yet; try again next time */
		}

		/* Get the hwgraph canonical name of the dump
		 * device if possible, to avoid having to look
		 * in the hwgraph during a panic.
		 */

		if (dumpdevname) {
			kern_free(dumpdevname);
			dumpdevname = NULL;
		}

		if (dumpdev && hwgraph_vertex_name_get(dumpdev,
			       devname, MAXDEVNAME) == GRAPH_SUCCESS) {
			dumpdevname = kern_malloc(strlen(devname) + 1);
			strcpy(dumpdevname, devname);
		}
	}
}

static void
edt_init(void)
{
	struct edt	*ep;
	int adaps, i;
	cpu_cookie_t ocpu;

	/* Call the edtinit routines if adapters exist. */
	for (ep = &edt[0], i = nedt; i > 0; i--, ep++) {

		/* convert invalid forced cpus to valid ones */
		if( !cpu_isvalid(ep->v_cpuintr) )
			ep->v_cpuintr = masterpda->p_cpuid;

		if (ep->e_init == 0)
			continue;

		if (ep->e_bus_type == ADAP_PCI)
			continue;

		adaps = readadapters(ep->e_bus_type);

		/* Possible for indigo and Everest debug
		 * kernels to have VME drivers but no
		 * VME bus on the system.
		 */
		if( (adaps == 0) && (ep->e_bus_type == ADAP_VME) )
			continue;

		ocpu = setmustrun(ep->v_cpuintr);

		if( ep->e_adap != -1 )
			(*ep->e_init)(ep);
		else {
			int j;
			for( j = 0 ; j < adaps ; j++ ) {
				ep->e_adap = (ep->e_bus_type == ADAP_VME) ?
					VMEADAP_ITOX(j) : j;

				(*ep->e_init)(ep);
			}
			ep->e_adap = -1;
		}

		restoremustrun(ocpu);
	}
}

static void
initialize_io(void)
{
	/* 
	 * Should call device_driver_init from here rather than from einit,
	 * but some drivers add to inventory from their init routines!
	 device_driver_init();
	 */

	init_all_devices();	/* initialize all devices */

	/*
	 * Probe all equipped devices to see if they are installed
	 * run the edtinit() routine on the cpu pointed to by v_cpuintr
	 */
	edt_init();
}

#if _MEM_PARITY_WAR || IP26 || IP28
extern void perr_init(void);

/*
 * This needs to be done before any io init routines.  If we're running
 * parity error workarounds, we need to check for bad GIO bus cards.
 * If there are, then we can't leave SYSAD parity enabled.
 * Bad cards are ones that don't put correct parity on the bus in every
 * bytelane, when they're responding to a PIO read for less than a whole
 * word.
 */
static void parity_init(void)
{
#if defined(IP20) || defined(IP22) || defined(IP26) || defined(IP28)
	perr_init();
#else
	return;
#endif
}
#endif /* _MEM_PARITY_WAR || IP26 || IP28 */
