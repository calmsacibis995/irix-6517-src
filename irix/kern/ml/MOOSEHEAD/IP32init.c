#include <sys/types.h>
#include <sys/timers.h>
#include <sys/ktime.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/cpu.h>
#include <sys/conf.h>
#include <sys/ds17287.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/kopt.h>
#include <sys/IP32flash.h>
#include <sys/invent.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/iograph.h>
#include <ksys/hwg.h>
#include <ksys/cacheops.h>
#include <sys/PCI/pcimh.h>
#include <vice/vice_regs_host.h>
#include <vice/vice_drv.h>	/* VICE_BASE */
#include <sys/IP32.h>
#ifdef TILES_TO_LPAGES
#include <sys/tile.h>
#endif
/*
 * NOTE: All the pio operations on IP32 need to go through the 
 * pciio_pio_{read,write}{8,16,32,64}() interfaces. This is to workaround
 * the CRIME->MACE  phantom pio read problem. This header file contains
 * the prototypes for these routines.
 */
#include <sys/PCI/pciio.h>
#ifdef _VCE_AVOIDANCE
extern int vce_avoidance;
#endif
extern short kdebug;
#if R4600
extern int two_set_pcaches;
#endif

/* crime rev reporting var */
extern int crime_rev;   /* defined in mtune/kernel */
extern int cpu_prid;   /* defined in mtune/kernel */

extern void synthclock(struct eframe_s *, __psint_t);
extern void synthackkgclock(struct eframe_s *, __psint_t);
extern void powerintr(struct eframe_s *, __psint_t);
extern void mh16550_attach(vertex_hdl_t);
extern void pckm_attach(vertex_hdl_t);

int r4000_clock_war = -1;
int is_r4600_flag = 0;
int maxcpus = MAXCPU;
char eaddr[6];
pdaindr_t pdaindr[MAXCPU];
unsigned int sys_id;
short cputype = 32;
pgno_t seg0_maxmem, seg1_maxmem;
pgno_t memsize;
paddr_t isa_dma_buf_addr;

#ifdef MH_R10000_SPECULATION_WAR
#include <sys/map.h>

/*
 * struct used to keep track of extk0 space
 */
#define EXTK0_MAXWIRED  2
struct extk0_cache {
	caddr_t	curp;				/* current extk0 ptr */
	pgno_t	maxpfn;				/* max pfn alloced */
	int	size;				/* size left in extk0 area */
	pde_t	ptes[EXTK0_MAXWIRED * 2];	/* ptes mapping this area */
	int	pagemask[EXTK0_MAXWIRED];	/* pagemasks for ptes */
};
static struct extk0_cache mcache_extk0;
#endif /* MH_R10000_SPECULATION_WAR */

#define is_petty_crime()	(CRM_IS_PETTY)

void
add_ioboard(void)
{
}

static void
add_vice(void)
{
	caddr_t vice_base; /* For REG*() macros */
	PROBE_AND_INVENTORY_VICE()
}

extern int cpu_mhz_rating(void);

void
add_cpuboard(void)
{
    add_to_inventory(INV_PROCESSOR, INV_CPUCHIP, cpuid(), 0,
		     private.p_cputype_word);
    add_to_inventory(INV_PROCESSOR, INV_FPUCHIP, cpuid(), 0,
		     private.p_fputype_word);
    add_to_inventory(INV_PROCESSOR, INV_CPUBOARD, cpu_mhz_rating(), -1,
		     INV_IP32BOARD);
    /* done in ecplp driver
    add_to_inventory(INV_PARALLEL, INV_EPP_ECP_PLP, 0, 0, 1);
    */
    /* To support hwgraph, serial driver, sio_ti16550.c, calls inventory.
    add_to_inventory(INV_SERIAL, INV_ONBOARD, 0, 0, 2);
    */
    /* add other stuff here */
    add_to_inventory(INV_MEMORY, INV_PROM, flash_version(FLASHPROM_MAJOR), 
		     flash_version(FLASHPROM_MINOR), 0);
    add_vice();
    crime_rev = get_crimerev();
    if (CRM_IS_REV_1_3)
      cmn_err(CE_NOTE, "System is running CRIME Rev 1.3\n");

    cpu_prid = get_cpu_irr();
}


void
init_sysid(void)
{
	extern char arg_eaddr[];

	char *cp;

	cp = kopt_find("eaddr");

	ASSERT(cp != NULL);
	if (!etoh(cp)) {
		eaddr[0] = 0x08;
		eaddr[1] = 0x00;
		eaddr[2] = 0xaa;
		eaddr[3] = 0xbb;
		eaddr[4] = 0xcc;
		eaddr[5] = 0xdd;
		sys_id = 0xaabbccdd;
		sprintf (arg_eaddr, "%x%x:%x%x:%x%x:%x%x:%x%x:%x%x",
			(eaddr[0]>>4)&0xf, eaddr[0]&0xf,
			(eaddr[1]>>4)&0xf, eaddr[1]&0xf,
			(eaddr[2]>>4)&0xf, eaddr[2]&0xf,
			(__psunsigned_t)(eaddr[3]>>4)&0xf, (__psunsigned_t)eaddr[3]&0xf,
			(__psunsigned_t)(eaddr[4]>>4)&0xf, (__psunsigned_t)eaddr[4]&0xf,
			(__psunsigned_t)(eaddr[5]>>4)&0xf, (__psunsigned_t)eaddr[5]&0xf);
	} else {
		bcopy(etoh(cp), eaddr, 6);
		sys_id = (eaddr[2] << 24) | (eaddr[3] << 16) |
			 (eaddr[4] << 8) | eaddr[5];
	}
}

#ifdef MH_R10000_SPECULATION_WAR
extern int get_r10k_config(void);
extern void set_r10k_config(int);
#endif /* MH_R10000_SPECULATION_WAR */

int ip32nokp;

static void
setup_ip32_flags(void)
{
	extern int cachewrback;
	extern int reboot_on_panic;
	extern int softpowerup_ok;
	extern int get_cpu_irr(void);
	extern int do_bump_leds;
#ifdef R4600
        rev_id_t ri;

        ri.ri_uint = get_cpu_irr();
        switch (ri.ri_imp) {
#ifdef TRITON
		case C0_IMP_TRITON:
		case C0_IMP_RM5271:
#endif /* TRITON */
		case C0_IMP_R4700:
		case C0_IMP_R4600:
			is_r4600_flag = 1;
			break;
#ifdef MH_R10000_SPECULATION_WAR
		case C0_IMP_R10000:
		case C0_IMP_R12000:
			/* 
			 * On R10000/R12000, ensure K0 compat is cached and
			 * exclusive write
			 */
			set_r10k_config(((get_r10k_config() & ~CONFIG_K0) |
					 CONFIG_COHRNT_EXLWR));
			break;
#endif /* MH_R10000_SPECULATION_WAR */
		default:
			break;
        }
#endif

	r4000_clock_war = 0;
#ifdef _VCE_AVOIDANCE
	vce_avoidance = 0;
	if (is_r4600_flag || !private.p_scachesize)
		vce_avoidance = 1;
#endif
	cachewrback = 1;

	/* If kdebug has not been set, dbgmon hasn't been loaded
	 * and we should turn kdebug off.
	 */
	if (kdebug == -1) 
	    kdebug = 0;

	if (is_specified(arg_bump_leds))
	    do_bump_leds = is_enabled(arg_bump_leds);

	/* If not stuned, do not automatically reboot on panic */
	if (reboot_on_panic == -1)
		reboot_on_panic = 0;

	softpowerup_ok = 1;		/* for syssgi() */

	/* XXX preemption switch is shortlived */
	ip32nokp = atoi(kopt_find("ip32nokp"));
}



#define SET_REG(reg, val)	*(volatile int *)PHYS_TO_K1(reg) = (val)
#define GET_REG(reg)		(*(volatile int *)PHYS_TO_K1(reg))

/*
 * These addresses were gleaned from <sys/crime_gbe.h>
 */
#define GBE_BASE		0x16000000
#define GBE_CTRLSTAT		(GBE_BASE + 0x00000)
#define GBE_DOTCLOCK		(GBE_BASE + 0x00004)
#define GBE_VT_XY		(GBE_BASE + 0x10000)
#define GBE_OVR_INHWCTRL	(GBE_BASE + 0x20004)
#define GBE_OVR_CONTROL		(GBE_BASE + 0x20008)
#define GBE_FRM_INHWCTRL	(GBE_BASE + 0x30008)
#define GBE_FRM_CONTROL		(GBE_BASE + 0x3000c)
#define GBE_DID_INHWCTRL	(GBE_BASE + 0x40000)
#define GBE_DID_CONTROL		(GBE_BASE + 0x40004)

int do_early_gbe_stop = 1;

/*
 * prom has left GBE dma'ing from memory, but we're going
 * to stomp over the tile tables during memory initialization
 * so shut him down.
 *
 * XXX
 * This blanks the screen and creates another screen flash
 * during bootup.
 *
 * I'd really like to see this routine come from the graphics
 * driver so we didn't have such knowledge of GBE internals here.
 * But we need to do this even when we don't have a graphics driver.
 *
 * Even better would be to collect the prom graphics tiles
 * and pass them along to the graphics driver for the textport
 * tiles and we could avoid some screen flashing.  Someday.
 * XXX
 */
void
early_gbe_stop(void)
{
    unsigned int frm, ovr, did, reg;
    int i;

    /*
     * We have to do this in two stages because GBE reportedly
     * examines the DMA enable bit one frame *after* it propagates
     * to the *_INHWCTRL register.  *_CONTROL register writes only
     * propagate to *_INHWCTRL during vertical retrace.  We need to
     * wait for two of those to be sure GBE noticed the DMA is disabled.
     * So first we turn off DMA, and wait for that to propagate to the
     * *_INHWCTRL registers.  Then we change the address (taking care
     * to be sure it is 32-byte aligned) and wait for that to show up.
     * We won't actually *use* this second address since it will show
     * up at the same time that GBE notices that DMA is disabled, but
     * we'll use the second address as a flag that the second vertical
     * retrace happened, and DMA will definitely be disabled.
     */

    /* don't bother if GBE is not doing any DMA */
    if (((GET_REG(GBE_FRM_INHWCTRL) & 1) == 0) &&
	((GET_REG(GBE_OVR_INHWCTRL) & 1) == 0) &&
	((GET_REG(GBE_DID_INHWCTRL) & 0x10000) == 0))
	return;

    /* Turn off all the DMAs */
    flushbus();
    frm = GET_REG(GBE_FRM_CONTROL);
          SET_REG(GBE_FRM_CONTROL, frm & ~1);
    flushbus();
    ovr = GET_REG(GBE_OVR_CONTROL);
          SET_REG(GBE_OVR_CONTROL, ovr & ~1);
    flushbus();
    did = GET_REG(GBE_DID_CONTROL);
          SET_REG(GBE_DID_CONTROL, did & ~0x10000);
    flushbus();

    /* Now wait for the hardware to see DMA off */
    for (i = 400000; i > 0; i--) {
	if (GET_REG(GBE_FRM_INHWCTRL) & 1)
	    continue;
	else if (GET_REG(GBE_OVR_INHWCTRL) & 1)
	    continue;
	else if (GET_REG(GBE_DID_INHWCTRL) & 0x10000)
	    continue;
	else break;
    }
    ASSERT(i > 0);

    /* Now poke another value in to verify the DMA off flag was seen */
    frm = GET_REG(GBE_FRM_CONTROL);
          SET_REG(GBE_FRM_CONTROL, frm + 32);
    flushbus();
    ovr = GET_REG(GBE_OVR_CONTROL);
          SET_REG(GBE_OVR_CONTROL, ovr + 32);
    flushbus();
    did = GET_REG(GBE_DID_CONTROL);
          SET_REG(GBE_DID_CONTROL, did + 32);
    flushbus();

    /* Verify register change to guarantee DMA off is taken */
    for (i = 400000; i > 0; i--) {
	if (GET_REG(GBE_FRM_INHWCTRL) != frm + 32)
	    continue;
	else if (GET_REG(GBE_OVR_INHWCTRL) != ovr + 32)
	    continue;
	else if (GET_REG(GBE_DID_INHWCTRL) != did + 32)
	    continue;
	else break;
    }
    ASSERT(i > 0);

    SET_REG(GBE_VT_XY, 0x80000000);
    reg = GET_REG(GBE_DOTCLOCK);
          SET_REG(GBE_DOTCLOCK, reg & ~0x100000);
    flushbus();
}


/*
 * mlreset - very early machine reset - at this point NO interrupts have been
 * enabled; nor is memory, tlb, p0, etc setup.
 */
#ifdef R4600
int	enable_orion_parity = 0;
#endif
/* ARGSUSED */
void
mlreset(int junk)
{
	extern uint cachecolormask;
	extern int _triton_use_invall;
	extern int pdcache_size;
	extern int ip32_pci_enable_prefetch;
	extern void ackkgclock(void);
	extern void buserror_intr(eframe_t *);
	extern void crmerr_intr(eframe_t *, __psint_t);
	extern void mh16550_earlyinit(void);
	int i;
	_crmreg_t crime_cntl;
	volatile uint *pci_config_reg = (uint *) PHYS_TO_K1(PCI_CONTROL);
	volatile uint *pci_error_flags = (uint *) PHYS_TO_K1(PCI_ERROR_FLAGS);

	crime_cntl = READ_REG64(PHYS_TO_K1(CRM_CONTROL), _crmreg_t);

	_triton_use_invall = is_enabled(arg_triton_invall);
	
#if 0
	if ((crime_id & CRM_ID_IDBITS) != CRM_ID_IDVALUE)
		panic("No CRIME chip found");
#endif


	/*
	** just to be safe, clear the error registers
	*/
	WRITE_REG64(0LL, PHYS_TO_K1(CRM_CPU_ERROR_STAT), _crmreg_t);
	WRITE_REG64(0LL, PHYS_TO_K1(CRM_MEM_ERROR_STAT), _crmreg_t);

	/*
	 * we need to enable the various error sources in Petty Crime
	 * since this register does not exist in rev 1 and above, we
	 * can't write to it here since it will generate a buserror.
	 */
	if (is_petty_crime())
	    WRITE_REG64(CRM_CPU_ERROR_MSK_REV0, PHYS_TO_K1(CRM_CPU_ERROR_ENA), 
			_crmreg_t);

	/*
	 * enable sysad/syscmd parity checking, disable watchdog timer
	 * for now.
	 */
#ifdef USE_McGriff
	crime_cntl |= CRM_CONTROL_DOG_ENA;
#else
	crime_cntl &= ~CRM_CONTROL_DOG_ENA;
#endif
#ifdef R10000
	if (IS_R10000())
		crime_cntl &= ~(CRM_CONTROL_TRITON_SYSADC|CRM_CONTROL_CRIME_SYSADC);
	else
#endif /* R10000 */
	crime_cntl |= CRM_CONTROL_TRITON_SYSADC|CRM_CONTROL_CRIME_SYSADC;
	WRITE_REG64(crime_cntl, PHYS_TO_K1(CRM_CONTROL), _crmreg_t);

	WRITE_REG64(0LL, PHYS_TO_K1(CRM_INTMASK), _crmreg_t);
	WRITE_REG64(0LL, PHYS_TO_K1(CRM_INTSTAT), _crmreg_t);

	flushbus();

	for (i = SPLMIN; i < SPLMAX; i++)
		private.p_splmasks[i] = 0;
	private.p_curspl = SPLMAX << 3;

	setkgvector(ackkgclock);
	setcrimevector(SOFT_INTR(0), SPL6, synthclock, 0, CRM_EXCL);
	setcrimevector(SOFT_INTR(1), SPL65, synthackkgclock, 0, CRM_EXCL);
	setcrimevector(MEMERR_INTR, SPL7, (intvec_func_t)buserror_intr, 0, 
			CRM_EXCL);
	setcrimevector(CRMERR_INTR, SPL7, (intvec_func_t)buserror_intr, 0, 
			CRM_EXCL);
	setmaceisavector(MACE_INTR(5), 0x0000000000000100, powerintr);

    	/*
     	 * clear CRIME watchdog timer
     	 */
	WRITE_REG64(0LL, PHYS_TO_K1(CRM_DOG), _crmreg_t);

	setup_ip32_flags();

	/* IP32 vectors initialized by appropriate drivers */

	init_sysid();

	load_nvram_tab();	/* get a copy of the nvram */

	cachecolormask = R4K_MAXPCACHESIZE/NBPP - 1;
#ifdef R4000PC
	if (private.p_scachesize == 0)
		cachecolormask = (pdcache_size / NBPP) - 1;
#endif
#ifdef R4600
	if (two_set_pcaches)
		cachecolormask = (two_set_pcaches / NBPP) - 1;

	/* 
	 * On the R4600, be sure the wait sequence is on a single cache line 
	 */
	if (two_set_pcaches) {
		extern int wait_for_interrupt_fix_loc[];
		int	i;

		if ((((int) &wait_for_interrupt_fix_loc[0]) & 0x1f) >= 0x18) {
			for (i = 6; i >= 0; i--)
				wait_for_interrupt_fix_loc[i + 2] = 
					wait_for_interrupt_fix_loc[i];
			wait_for_interrupt_fix_loc[0] =
				wait_for_interrupt_fix_loc[9];
			wait_for_interrupt_fix_loc[1] =
				wait_for_interrupt_fix_loc[9];
			__dcache_wb_inval((void *) &wait_for_interrupt_fix_loc[0],
					  9 * sizeof(int));
			__icache_inval((void *) &wait_for_interrupt_fix_loc[0],
					  9 * sizeof(int));

		}
	}
#endif
#ifdef R10000
	if (IS_R10000())
		cachecolormask = 0;
#endif /* R10000 */

	/*
	 * Initialize MACE PCI
	 */

	/*
	 * NOTE: All the pio operations on IP32 need to go through the 
	 * pciio_pio_{read,write}{8,16,32,64}() interfaces. This is to 
	 * workaround the CRIME->MACE  phantom pio read problem. 
	 */

	pciio_pio_write32(0,(volatile uint32_t *)pci_error_flags);
	if (ip32_pci_enable_prefetch)
		pciio_pio_write32(PCI_CONFIG_BITS | PCI_CONTROL_MRMRA_ENABLE,
			    (volatile uint32_t *)pci_config_reg);
	else
		pciio_pio_write32(PCI_CONFIG_BITS,
			    (volatile uint32_t *)pci_config_reg);

	/* allow console output before hwg has been setup */
	mh16550_earlyinit();
}

/*
 * timer_freq is the frequency of the 32 bit counter timestamping source.
 * timer_high_unit is the XXX
 * Routines that references these two are not called as often as
 * other timer primitives. This is a compromise to keep the code
 * well contained.
 * Must be called early, before main().
 */
void
timestamp_init(void)
{
	extern int findcpufreq(void);
	extern unsigned timer_freq;
	extern unsigned timer_unit;
	extern unsigned timer_high_unit;
	extern unsigned timer_maxsec;

	timer_freq = findcpufreq_raw();
	timer_unit = NSEC_PER_SEC/timer_freq;
	timer_high_unit = timer_freq;	/* don't change this */
	timer_maxsec = TIMER_MAX/timer_freq;
}

#define CHECK_RV(Y)	\
	if (rv != GRAPH_SUCCESS)	\
		cmn_err(CE_PANIC, "Hardware graph FAILURE : %s", Y);

void
init_all_devices(void)
{	
	vertex_hdl_t node, mem, io, mace, scsi, scsi_ctlr, disk;
	graph_error_t rv;

	/* node vhdl attached directly to /hw */
	rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_NODE, &node);
	CHECK_RV(EDGE_LBL_NODE);

	add_cpu(node);

	/* memory vhdl */
	rv = hwgraph_path_add(node, EDGE_LBL_MEMORY, &mem);
	CHECK_RV(EDGE_LBL_MEMORY);

	/* io vhdl */
	rv = hwgraph_path_add(node, EDGE_LBL_IO, &io);
	CHECK_RV(EDGE_LBL_IO);

	/* mace vhdl */
	rv = hwgraph_path_add(io, EDGE_LBL_MACE, &mace);
	CHECK_RV(EDGE_LBL_MACE);

	pcimh_attach(io);

	/* initialize dma buffers for serial and audio */
	isa_dma_buf_init();
	mh16550_attach(io);

        /* attach keyboard/mouse */
        pckm_attach(io);

	/* scsi vhdl */
	rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_SCSI, &scsi);
	CHECK_RV(EDGE_LBL_SCSI);

	/* scsi_ctlr vhdl */
	rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_SCSI_CTLR, &scsi_ctlr);
	CHECK_RV(EDGE_LBL_SCSI_CTLR);
	
	/* disk vhdl */
	rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_DISK, &disk);
	CHECK_RV(EDGE_LBL_DISK);


#if HWG_PRINT
        hwgraph_print();
#endif
}

/*
** do the following check to make sure that a wrong config
** in system.gen won't cause the kernel to hang
** ignores the MP stuff and just does some basic sanity setup.
*/
void
allowboot(void)
{
	register int i;	
	extern struct bdevsw bdevsw[];
	extern struct cdevsw cdevsw[];
	extern int bdevcnt;
	extern int cdevcnt;

	for (i=0; i<bdevcnt; i++)
		bdevsw[i].d_cpulock = 0;

	for (i=0; i<cdevcnt; i++)
		cdevsw[i].d_cpulock = 0;
#ifdef R10000
	if (IS_R10000())
		init_mfhi_war();
#endif /* R10000 */
}

/*
 * setupbadmem - mark the hole in memory between seg 0 and seg 1.
 *		 return count of non-existent pages.
 */
/*ARGSUSED2*/
pfn_t
setupbadmem(pfd_t *pfd, pfn_t first, pfn_t last)
{
	register pgno_t	hole_size;
	register pgno_t npgs, extk0_pgs;

	hole_size = extk0_pgs = 0;

#ifdef MH_R10000_SPECULATION_WAR
	/* These weren't cleared in tlbinit */
	flush_tlb(EXTK0TLBINDEX);
	flush_tlb(EXTK0TLBINDEX2);

	if (mcache_extk0.maxpfn) {
		pfd_t *ppfd = pfntopfdat(SMALLMEM_K0_R10000_PFNMAX + 1);
		extern void extk0_avail_alloc(pfn_t);

		for (npgs = SMALLMEM_K0_R10000_PFNMAX + 1;
			npgs < mcache_extk0.maxpfn; npgs++, ppfd++) {
			PG_MARKHOLE(ppfd);
		}

		extk0_pgs = mcache_extk0.maxpfn - 
				(SMALLMEM_K0_R10000_PFNMAX + 1);
		extk0_avail_alloc(first);
	}
#endif /* MH_R10000_SPECULATION_WAR */

	if (seg1_maxmem) {
		pfd += btoc(SEG0_BASE) + seg0_maxmem - first;
		hole_size = btoc(SEG1_BASE) - (btoc(SEG0_BASE) + seg0_maxmem); 

		for (npgs = hole_size; npgs; pfd++, npgs--)
			PG_MARKHOLE(pfd);
	}

	/* take last page of memory offline to avoid dma problems */
	PG_MARKHOLE(pfntopfdat(last));

	return hole_size + extk0_pgs + 1;
}

#ifdef ONEMEG
#undef ONEMEG
#endif
#define ONEMEG (1024 * 1024)

/*ARGSUSED1*/
pfn_t
szmem(pfn_t fpage, pfn_t maxpmem)
{
#ifdef DONT_USE_BANK_REGS
    if (!memsize) {
	memsize = seg0_maxmem = btoc(32 * (1024 * 1024));
	seg1_maxmem = 0;
    }
    return memsize;
#else
    _crmreg_t ctrl, bank0_ctrl;
    int i;

    if (!memsize) {
	for (i = 0; i < CRM_MAXBANKS; i++) {
	    ctrl = READ_REG64(PHYS_TO_K1(CRM_MEM_BANK_CTRL(i)), 
			      _crmreg_t);
	    if (i == 0) {
		bank0_ctrl = ctrl;
		memsize += bank_size(ctrl);
	    } else if (ctrl != bank0_ctrl) {
		memsize += bank_size(ctrl);
	    }
	}

	/*
	 * adjust for the double mapped segment in the
	 * 1st 256Mb of physical address space (it is also
	 * mapped at 0x40000000.
	 */
	if (memsize > (256 * ONEMEG)) {
	    seg0_maxmem = 256 * ONEMEG;
	    seg1_maxmem = memsize - (256 * ONEMEG);
	} else {
	    seg0_maxmem = memsize;
	    seg1_maxmem = 0;
	}
	memsize = btoc(memsize);
	seg0_maxmem = btoc(seg0_maxmem);
	seg1_maxmem = btoc(seg1_maxmem);
	if (maxpmem) {
	    memsize = MIN(memsize, maxpmem);
	    seg0_maxmem = MIN(memsize, seg0_maxmem);
	    seg1_maxmem = MIN(memsize - seg0_maxmem, seg1_maxmem);
	}
    }
    return memsize;
#endif
}

pfn_t
last_phys_pfn(void)
{
    size_t memsize = 0;
    _crmreg_t ctrl, bank0_ctrl;
    int i;

    for (i = 0; i < CRM_MAXBANKS; i++) {
	ctrl = READ_REG64(PHYS_TO_K1(CRM_MEM_BANK_CTRL(i)), 
			  _crmreg_t);
	if (i == 0) {
	    bank0_ctrl = ctrl;
	    memsize += bank_size(ctrl);
	} else if (ctrl != bank0_ctrl) {
	    memsize += bank_size(ctrl);
	}
    }
    if (memsize > (256 * ONEMEG)) {
	memsize += LINEAR_BASE;
    }
    return(btoc(memsize));
}


void
isa_dma_buf_init(void)
{
    if (!isa_dma_buf_addr) {
#ifdef _SYSTEM_SIMULATION
	isa_dma_buf_addr = 0x1f400000;
#else
	isa_dma_buf_addr = contig_memalloc(8, 8, 
			(VM_DIRECT
#ifdef MH_R10000_SPECULATION_WAR
				| VM_UNCACHED | VM_DMA_RW
#endif /* MH_R10000_SPECULATION_WAR */
					    )) << PNUMSHFT;
	if (isa_dma_buf_addr == NULL)
		panic("Cannot allocate ISA DMA buffer");
#endif
    }

    WRITE_REG64(isa_dma_buf_addr & 0xffff8000, PHYS_TO_K1(ISA_RINGBASE),
		__uint64_t);
    flushbus();
}

/*
 * Scrub a block of memory by loading double words
 * and storing them back.
 */
static void
scrub_memory_block(uint_t va, int size)
{
    uint_t endva;
#define DSIZE (sizeof(long long))
#define DMASK (DSIZE - 1)

    /* align to double word boundaries */
    size += va & DMASK;
    va &= ~DMASK;

    for (endva = va + size; va < endva; va += DSIZE)
	scrub_memory((caddr_t)va);

#undef DSIZE
#undef DMASK
}


int ecc_checking_enabled = 1;
int ecc_use_mte = 1;

int ecc_meminited = 0;

#define MAXCLEAREDMEM (0x4000000/NBPP)	

/*
 * Initialize ECC by clearing all of memory.
 *
 * we return 0 if memory was not set to correct ecc
 * 1 if memory was set to correct ecc *and zero'd*
 */
int
ecc_meminit(pfn_t firstpfn, pfn_t lastpfn)
{
    extern uint _ftext[];
    extern void early_mte_zero(paddr_t, size_t);
    extern int prom_does_eccinit(void);
    extern int early_delay_flag;
    int size = (char *)_ftext - (char *)K0BASE;
    int first, last;

#if defined(SABLE_RTL) || defined(SABLE) || defined(_SYSTEM_SIMULATION)
    ecc_meminited = lastpfn;
    return 1;
    /*NOTREACHED*/
#endif /* SABLE_RTL || SABLE || _SYSTEM_SIMULATION */

    /* shut down gbe */
    if (do_early_gbe_stop && !is_petty_crime())
	early_gbe_stop();

    /*
     * the PROM does clean memory, but it can leave a few pages
     * dirty in the area it uses for tiles and the area it copies
     * itself into.  So we still need to clean a bit.  We'll 
     * clean a bit more than this so that we dont' have to do
     * the bzero in tlbinit later on.
     */
    if (prom_does_eccinit() && !is_petty_crime()) {
	lastpfn = lastpfn > MAXCLEAREDMEM ? MAXCLEAREDMEM : lastpfn;
    }

    /* no mte on petty crime */
    if (!is_petty_crime() && ecc_use_mte) { 

	scrub_memory_block(K0_TO_K1(K0_RAMBASE), size);

	if (lastpfn >= btoc(SEG1_BASE)) {
	    first = ctob(firstpfn) + LINEAR_BASE;
	    last = ctob(lastpfn);
	} else {
	    first = ctob(firstpfn);
	    last = ctob(lastpfn);
	}
	early_delay_flag = 1;
	early_mte_zero(first, last - first);
	early_delay_flag = 0;
	ecc_meminited = lastpfn;
	return 1;
    }

    /*
     * If we have >256M physical memory, then we can't access
     * all of it directly through K0/K1 space without either
     * turning on KX or mapping with the tlb.  Punt for now.
     */
    if (lastpfn >= btoc(SEG1_BASE)) {
#ifdef too_early_for_cmn_err
	cmn_err(CE_WARN,
		"Memory size is too large for ecc_meminit -- "
		"ECC checking disabled\n");
#endif
	return 0;
    }

    /*
     * Non-destructively set correct ECC for the prom TV table,
     * and symmon if it was loaded.  Symmon text is correct due
     * to DMA to load it, but its bss/data hasn't been corrected
     * yet.
     */
    scrub_memory_block(K0_TO_K1(K0_RAMBASE), size);

    /* Now clean up the rest of memory. */
    bzero((void *)PHYS_TO_K0(ctob(firstpfn)), ctob(lastpfn - firstpfn));

    /*
     * we should probably call flush_cache() here, but this function
     * is called so early in the startup that we can't because the 
     * pda page isn't set up yet.  The caches are flushed as soon as
     * it is set up and initialized, which isn't too much after this.
     */
    ecc_meminited = lastpfn;
    return 1;
}


/*
 * Enable ECC checking in CRIME.
 */
void
ecc_enable(void)
{
    if (is_specified(arg_ecc_enabled))
	ecc_checking_enabled = is_enabled(arg_ecc_enabled);

    if (ecc_meminited && ecc_checking_enabled) {
	WRITE_REG64(CRM_MEM_CONTROL_ECC_ENA,
		    PHYS_TO_K1(CRM_MEM_CONTROL),
		    _crmreg_t);
    }
}


/*
 * Disable ECC checking in CRIME.
 */
void
ecc_disable(void)
{
    WRITE_REG64(0LL,
		PHYS_TO_K1(CRM_MEM_CONTROL),
		_crmreg_t);
}

graph_error_t
mace_hwgraph_lookup(vertex_hdl_t *ret)
{
	char str[25];
	sprintf(str, "%s/%s/%s", EDGE_LBL_NODE, EDGE_LBL_IO, EDGE_LBL_MACE);
	return hwgraph_traverse(GRAPH_VERTEX_NONE, str, ret);
}

#ifdef MH_R10000_SPECULATION_WAR
int
is_extk0_pfn(uint pfn)
{
	return (IS_R10000() &&
		(pfn >= SMALLMEM_K0_R10000_PFNMAX+1 &&
			(pfn < mcache_extk0.maxpfn)));
}

pgno_t
extk0_pages_added(void)
{
	pgno_t first = kvatopfn(mcache_extk0.curp);

	return pfn_lowmem - first;
}

void
extk0_wiremem(void)
{
	uint i, widx = EXTK0TLBINDEX;
	caddr_t vaddr;

	flush_tlb(TLBWIREDBASE);	/* flush entries after pda/extk0 */
	vaddr = (caddr_t)EXTK0_BASE;

	for (i = 0; i < EXTK0_MAXWIRED; i++, widx++) {
		if (mcache_extk0.ptes[i*2].pgi == 0)
			break;

		tlbwired(widx, 0, (caddr_t)vaddr,
			mcache_extk0.ptes[i*2].pte,
			mcache_extk0.ptes[(i*2)+1].pte,
			mcache_extk0.pagemask[i]);

		vaddr += ctob(mcache_extk0.ptes[(i*2)+1].pte.pg_pfn -
			mcache_extk0.ptes[i*2].pte.pg_pfn) * 2;
	}
}

/*ARGSUSED*/
void *
extk0_mem_alloc(register int need, char *name)
{
	caddr_t cp, ret;
	pde_t *pd;
#ifdef DEBUG
	static caddr_t prev_cp;
#endif

	/* Use k0, until k2 allocs are allowed */
	if (!kptbl || !sptbitmap.sptmap.m_map)
		return 0;

	/*
	 * Make sure we're aligned correctly. After we're setup
	 * we'll kvfree() this allocation.
	 */
	if (!mcache_extk0.curp)
		(void) kvalloc(btoc(EXTK0_OFFSET), 0, 0);

	for (;;) {
		if (need < mcache_extk0.size)
			break;
		/*
		 * Add another page. These kvallocs should be contiguous,
		 * so we can alloc in 4K page chunks, eventually wiring a
		 * large pte for it.
		 */
		cp = kvalloc(1, NULL, NULL);

		if (!mcache_extk0.curp) {
			ASSERT(cp == (caddr_t)EXTK0_BASE);
			mcache_extk0.curp = cp;
			mcache_extk0.maxpfn = SMALLMEM_K0_R10000_PFNMAX + 1;
#ifdef DEBUG
			prev_cp = cp;
		} else {
			ASSERT(cp == (prev_cp + NBPP));
			prev_cp = cp;
#endif
		}

		pd = kvtokptbl(cp);
		pd->pgi = mkpde((PG_VR | PG_G | PG_M | PG_SV |
				pte_cachebits()), mcache_extk0.maxpfn++);
		mcache_extk0.size += NBPP;
	}

	ret = mcache_extk0.curp;
	mcache_extk0.curp += need;
	mcache_extk0.size -= need;

	lowmemdata_record(name, PFN_TO_CNODEID(kvatopfn(ret)), need, ret);
	return ret;
}

#define PGSIZE_4MB	(4 * 1024 * 1024)
#define PGSIZE_16MB	(16 * 1024 * 1024)
#define EXTK0_MAXPFN	(((2 * PGSIZE_4MB) + (2 * PGSIZE_16MB)) >> PNUMSHFT)

void
extk0_avail_alloc(pfn_t first)
{
	int i, pagesz, size;
	int pagemask, maxpgmasksz;
	int pfn;
	pde_t *pd;
	extern int nproc;
#ifdef DEBUG
	caddr_t prev_cp = (caddr_t)
			(((__psunsigned_t)mcache_extk0.curp) & ~(NBPP-1));
#endif
	/*
	 * Estimate how many extra extk0 pages are needed beyond
	 * the low 8MB, based on nproc ... up to the max that can
	 * be mapped.
	 */
	pfn = (2 * nproc) - ((SMALLMEM_K0_R10000_PFNMAX + 1) - first);
	pfn = MIN(pfn, (EXTK0_MAXPFN -
		(mcache_extk0.maxpfn - (SMALLMEM_K0_R10000_PFNMAX + 1))));

	mcache_extk0.curp = (caddr_t) ctob(btoc(mcache_extk0.curp));
	pd = kvtokptbl(mcache_extk0.curp);

	/*
	 * Allocate the kvaddrs now, so they can be handed out
	 * during a pagealloc.
	 */
	for (; pfn > 0; pfn--, pd++) {
#ifdef DEBUG
		caddr_t cp =
#endif
		kvalloc(1, NULL, NULL);
#ifdef DEBUG
		/* make sure it's virtually contiguous */
		ASSERT(cp == (prev_cp + NBPP)); prev_cp = cp;
#endif
		pd->pgi = mkpde((PG_VR | PG_G | PG_M | PG_SV |
				pte_cachebits()), mcache_extk0.maxpfn++);
	}

	pd = mcache_extk0.ptes;
	pfn = SMALLMEM_K0_R10000_PFNMAX + 1;
	size = ctob(mcache_extk0.maxpfn - pfn);

	ASSERT(size <= ctob(EXTK0_MAXPFN));

	/*
	 * Calculate the size of the ptes needed to map the extk0
	 * area and stash the ptes/pagemasks into mcache_extk0.
	 */
	for (i = 0; i < EXTK0_MAXWIRED && size > 0; i++) {
		pagesz = NBPP;
		pagemask = 0;

		/*
		 * Our starting pfn is aligned on an 8MB boundary, so the
		 * largest pagemask we can use is 4MB ... in the second loop
		 * we can be aligned up to a 16MB page boundary.
		 */
		maxpgmasksz = (i == 0) ? PGSIZE_4MB : PGSIZE_16MB;

		/* select largest pte we can */
		while ((pagesz << 2) < size) {
			pagesz <<= 2;
			pagemask = (pagemask << 2) | 0x3;

			if (pagesz == maxpgmasksz)
				break;
		}

		/* If it's the last pte, make sure it's large enough */
		if (pagesz != maxpgmasksz &&
			(i == (EXTK0_MAXWIRED-1)) && (size > (2 * pagesz))) {
			pagesz <<= 2;
			pagemask = (pagemask << 2) | 0x3;
		}

		size -= pagesz;
		mcache_extk0.pagemask[i] = pagemask << (PNUMSHFT+1);
		pd++->pgi = mkpde((PG_VR | PG_G | PG_M | PG_SV |
				pte_cachebits()), pfn);
		pfn += btoct(pagesz);
		pd++->pgi = mkpde((PG_VR | PG_G | PG_M | PG_SV |
					pte_cachebits()), pfn);
		size -= pagesz;
		pfn += btoct(pagesz);
	}

	ASSERT((size <= 0) && (mcache_extk0.maxpfn <= pfn));

	pd = &kptbl[btoc(EXTK0_BASE - K2BASE) +
		(mcache_extk0.maxpfn - (SMALLMEM_K0_R10000_PFNMAX+1))];

	while (mcache_extk0.maxpfn < pfn) {
#ifdef DEBUG
		caddr_t cp =
#endif
		kvalloc(1, NULL, NULL);
#ifdef DEBUG
		/* make sure it's virtually contiguous */
		ASSERT(cp == (prev_cp + NBPP)); prev_cp = cp;
#endif
		pd++->pgi = mkpde((PG_VR | PG_G | PG_M | PG_SV |
				pte_cachebits()), mcache_extk0.maxpfn++);
	}

	pfn_lowmem = mcache_extk0.maxpfn;
	pfd_lowmem = pfntopfdat(mcache_extk0.maxpfn);
#ifdef TILES_TO_LPAGES
	tile_lowmem = pfntotile(pfn_lowmem);
#endif

	extk0_wiremem();

	/* Free the starting pad */
	kvfree((void *)K2BASE, btoc(EXTK0_OFFSET));
}
#endif	/* MH_R10000_SPECULATION_WAR */
