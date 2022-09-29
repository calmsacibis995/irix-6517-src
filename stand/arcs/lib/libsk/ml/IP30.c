#ident	"$Revision: 1.115 $"

#ifdef IP30	/* whole file */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/RACER/racermp.h>
#include <sys/immu.h>
#include <sys/ds1687clk.h>
#include <sys/ecc.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/types.h>
#include <sys/param.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/cfgtree.h>
#include <arcs/time.h>
#include <sys/xtalk/hq4.h>
#include <sys/nic.h>
#include <sys/tpureg.h>
#include "sys/odsyhw.h"

#include <standcfg.h>			/* for RAD stub */

#define	data_eccsyns	real_data_eccsyns

extern void	ns16550cons_errputc(uchar_t);
#ifdef US_DELAY_DEBUG
static void check_us_delay(void);
#endif

/* exports */
void		cpu_install(COMPONENT *);
__uint64_t	cpu_get_memsize(void);
void		cpu_acpanic(char *str);
void		ecc_error_decode(u_int);

int		processor_type;
int		master_cpuid;

/* private */
static __uint32_t	cpu_get_membase(void);
static void		init_board_rev(void);

extern int	_prom;
int		ide_ecc_test = 0;

static unsigned char to_numcpus[] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};

/* probe only active CPUs -- used by symmon also */
int
cpu_probe(void)
{
	heartreg_t active_msk;
	heartreg_t disabled;

	active_msk = (HEART_PIU_K1PTR->h_status & HEART_STAT_PROC_ACTIVE_MSK) >>
		HEART_STAT_PROC_ACTIVE_SHFT;
	disabled = (HEART_PIU_K1PTR->h_mode & HM_PROC_DISABLE_MSK) >>
		HM_PROC_DISABLE_SHFT;

	return (int)to_numcpus[active_msk & ~disabled];
}

/* probe all (even disabled) CPUs */
int
cpu_probe_all(void)
{
	heartreg_t active_msk;

	active_msk = (HEART_PIU_K1PTR->h_status & HEART_STAT_PROC_ACTIVE_MSK) >>
		HEART_STAT_PROC_ACTIVE_SHFT;

	return (int)to_numcpus[active_msk];
}

int
cpu_is_active(int id)
{
	heartreg_t active_msk;
	heartreg_t disabled;

	active_msk = (HEART_PIU_K1PTR->h_status & HEART_STAT_PROC_ACTIVE_MSK) >>
		HEART_STAT_PROC_ACTIVE_SHFT;
	disabled = (HEART_PIU_K1PTR->h_mode & HM_PROC_DISABLE_MSK) >>
		HM_PROC_DISABLE_SHFT;
	
	return active_msk & ~disabled & (0x1L << id);
}

/* setup and enable power related interrupts */

void
ip30_setup_power(void)
{
	bridge_t *bridge = BRIDGE_K1PTR;
	heart_piu_t *heart = HEART_PIU_K1PTR;

	/* Initialize soft power interrupt in bridge and heart */
	set_bridge_vector((void *)bridge, IP30_BVEC_POWER, IP30_HVEC_POWER);
	heart->h_imr[master_cpuid] |= 1ull << IP30_HVEC_POWER;
	/* Initialize AC interrupt in bridge for polling in stand */
	if (heart_rev() > HEART_REV_A)
		set_bridge_vector((void *)bridge, IP30_BVEC_ACFAIL,
				  IP30_HVEC_ACFAIL);
	flushbus();

	/* Enable interrupt at the CPU for those with SR_IE set.
	 */
	set_SR(get_SR() | SR_POWER);
}

/* unset and disable power button interrupt */

void
ip30_disable_power(void)
{
	bridge_t *bridge = BRIDGE_K1PTR;
	heart_piu_t *heart = HEART_PIU_K1PTR;

	/* Disable interrupt at the CPU for those with SR_IE set.
	 */
	set_SR(get_SR() & ~SR_POWER);
	
	/* unset and disable bridge interrupts
	 */
	disable_bridge_intr((void *)bridge, IP30_BVEC_POWER);

	heart->h_imr[master_cpuid] &= ~(1ull << IP30_HVEC_POWER);
	flushbus();
}

void
init_ip30_chips(void)
{
	extern int flash_init(void);

	/* do this early since chips initialization may need this info */
	init_board_rev();

	init_heart(HEART_PIU_K1PTR, HEART_CFG_K1PTR);
	ip30_setup_power();

	/* Make sure IOC3 outputs are enabled for IP30 LEDs, as the kernel
	 * has been known to reset it occasionally.  Then make the LED
	 * green.
	 */
	((ioc3_mem_t *)IOC3_PCI_DEVIO_K1PTR)->gpcr_s =
		GPCR_LED0_ON|GPCR_LED1_ON;
	ip30_set_cpuled(LED_GREEN);

	flash_init();
}

/* perform any board specific start up initialization */
void
cpu_reset(void)
{
	extern int cachewrback;
	extern int scsicnt;
	u_char fan_setting;

	cachewrback = 1;
	processor_type = r4k_getprid() & ~0x0f;

	/* i am executing this code therefore i must be the master processor */
	master_cpuid = cpuid();

	/* initialize the spinlock subsystem */
	initsplocks();

	/*
	 * If we're coming from the PROM,
	 * we've already done the early init.
	 */
	if (!_prom)
		init_ip30_chips();

	/* Go to nominal voltage */
	if (MPCONF->fanloads <= 1)
		fan_setting = FAN_SPEED_LO;
	else if (MPCONF->fanloads >= 1000)
		fan_setting = FAN_SPEED_HI;
	else
		fan_setting = 0x0;
	*IP30_VOLTAGE_CTRL = fan_setting |
		PWR_SUPPLY_MARGIN_LO_NORMAL | PWR_SUPPLY_MARGIN_HI_NORMAL;

	scsicnt = 2;			/* two ql chips in base system */

#ifdef US_DELAY_DEBUG
	check_us_delay();
#endif
}

/* initialize keyboard bell */
void
kbdbell_init(void)		/* XXX BUG 421254 */
{
}

/*ARGSUSED*/
void
ip30_set_cpuled(int color)
{
	ioc3_mem_t *ioc3 = (ioc3_mem_t *)IOC3_PCI_DEVIO_K1PTR;

	switch (color) {
	case LED_OFF:
		ioc3->gppr_0 = 0;
		ioc3->gppr_1 = 0;
		break;

	case LED_GREEN:
		ioc3->gppr_1 = 0;
		ioc3->gppr_0 = 1;
		break;

	case LED_AMBER:
	default:
		ioc3->gppr_0 = 0;
		ioc3->gppr_1 = 1;
		break;
	}
	flushbus();
}

/* get memory configuration word for a bank */
/* NOTE: while heart has registers for 8 banks,
 * this code knows that IP30 only has 4.
 */
static __uint32_t
get_mem_conf(int bank)
{
	ASSERT(bank >=0 && bank <= 3);
	return HEART_PIU_K1PTR->h_memcfg.bank[bank];
}

/* determine the size of a memory bank */
static __uint64_t
banksz(int bank)
{
	__uint32_t	memconfig = get_mem_conf(bank);
	__uint32_t	memcfg_ram;

	if (!(memconfig & HEART_MEMCFG_VLD))
		return 0;

	memcfg_ram = (memconfig & HEART_MEMCFG_RAM_MSK) >> HEART_MEMCFG_RAM_SHFT;

	return (memcfg_ram + 1) << HEART_MEMCFG_UNIT_SHFT;
}

#define	INVALID_DIMM_BANK	-1

/* find bank in which the given addr belongs to */
int
ip30_addr_to_bank(__uint64_t addr)	/* phys addr is only 40 bits */
{
	__uint64_t base, size;
	__uint32_t memconfig;
	int i;

	if (addr >= ABS_SYSTEM_MEMORY_BASE)
		addr -= ABS_SYSTEM_MEMORY_BASE;

	for (i = 0; i < 4; i++) {
		if (!(size = banksz(i)))
			continue;
		memconfig = get_mem_conf(i);
		base = (memconfig & HEART_MEMCFG_ADDR_MSK) <<
			HEART_MEMCFG_UNIT_SHFT;
		if (addr >= base && addr < (base + size))
			return i;
	}

	return INVALID_DIMM_BANK;
}

char *
cpu_memerr_to_dimm(heartreg_t memerr) 
{
	static char	buf[16];	/* DIMM Sx[, (D|C)Bxx] */
	paddr_t		paddr = memerr & HME_PHYS_ADDR;
	int		syndrome = (memerr & HME_SYNDROME) >> HME_SYNDROME_SHFT;

	sprintf(buf, "DIMM S%d",
		(ip30_addr_to_bank(paddr) * 2) + 1 + ((paddr & 0x8) != 0));
	if (data_eccsyns[syndrome].type == DB ||
	    data_eccsyns[syndrome].type == CB)
		sprintf(&buf[7], ", %s%d", 
			etstrings[data_eccsyns[syndrome].type],
			data_eccsyns[syndrome].value);

	return buf;
}

int ip30_rev;
int
ip30_board_rev(void)
{
	return ip30_rev;
}


/* Read the board revision info from NIC, and store formatted version # in
 * memory.
 */
static void
init_board_rev(void)
{
	nic_data_t mcr = (nic_data_t)&BRIDGE_K1PTR->b_nic;
	int dash = 1, rev = 0;
	char buf[256];
	char *p;

	cfg_get_nic_info(mcr, buf);

	if (p = strstr(buf, "Part:")) {
		p += 5;
		if (p = strchr(p,'-')) {
			p++;
			if (p = strchr(p, '-')) {
				p++;
				dash = atoi(p);
			}
		}
	}
	if (p = strstr(buf, "Revision:"))
		rev = *(p + 9);

#ifdef HEART_COHERENCY_WAR
	if (heart_rev() == HEART_REV_B) {
		extern int fibbed_heart;

		fibbed_heart = strstr(buf, "Group:fe") != NULL || rev >= 'J';
	}
#endif	/* HEART_COHERENCY_WAR */

	ip30_rev = (dash << 16) | rev;
}


/* IP30 specific hardware inventory routine */

/* calculate log2(x) */
static int
log2(int x)
{
	int n, v;
	for(n = 0, v = 1; v < x; v <<= 1, n++)
		;
	return n;
}

#define SYSBOARDIDLEN	9
#define SYSBOARDID	"SGI-IP30"

static COMPONENT IP30tmpl = {
	SystemClass,		/* Class */
	ARC,			/* Type */
	0,			/* Flags */
	SGI_ARCS_VERS,		/* Version */
	SGI_ARCS_REV,		/* Revision */
	0,			/* Key */
	0,			/* Affinity */
	0,			/* ConfigurationDataSize */
	SYSBOARDIDLEN,		/* IdentifierLength */
	SYSBOARDID		/* Identifier */
};

cfgnode_t *
cpu_makecfgroot(void)
{
	COMPONENT *root;

	root = AddChild(NULL, &IP30tmpl, (void *)NULL);
	if (root == (COMPONENT *)NULL)
		cpu_acpanic("root");
	return (cfgnode_t *)root;
}

#ifndef IP30_RPROM
static COMPONENT cputmpl = {
	ProcessorClass,			/* Class */
	CPU,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	12,				/* IdentifierLength */
	"MIPS-R10000"			/* Identifier */
};
static COMPONENT fputmpl = {
	ProcessorClass,			/* Class */
	FPU,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	/* Must be longer than max string below */
	15,				/* IdentifierLength */
	"MIPS-R10010FPC"		/* Identifier */
};
static COMPONENT cachetmpl = {
	CacheClass,			/* Class */
	PrimaryICache,			/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	NULL				/* Identifier */
};
static COMPONENT memtmpl = {
	MemoryClass,			/* Class */
	Memory,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* Identifier Length */
	NULL				/* Identifier */
};

/* NOTE: assumes on IP30 that all R10K CPUs in an MP config are the same
 *	 clock rate (they have to be due to the cluster bus) and same
 *	 cache size (which is true as they are on the same board).
 */
static void
inventory_cpu(COMPONENT *root, int n)
{
	extern unsigned _scache_linesize;
	extern unsigned _dcache_linesize;
	extern unsigned _icache_linesize;
	extern unsigned _sidcache_size;
	extern unsigned _dcache_size;
	extern unsigned _icache_size;
	int trex = ((r4k_getprid() & C0_IMPMASK) >> C0_IMPSHIFT) >=
		C0_IMP_R12000;
	COMPONENT *c, *tmp;
	union key_u key;

	c = AddChild(root, &cputmpl, (void *)NULL);
	if (c == (COMPONENT *)NULL)
		cpu_acpanic("cpu");
	c->Key = n;
	if (trex)
		c->Identifier[7] = '2';		/* TREX == R12000 */

	tmp = AddChild(c, &fputmpl, (void *)NULL);
	if (tmp == (COMPONENT *)NULL)
		cpu_acpanic("fpu");
	tmp->Key = n;
	if (trex)
		tmp->Identifier[7] = '2';	/* TREX == R12000 */

	tmp = AddChild(c, &cachetmpl, (void *)NULL);
	if (tmp == (COMPONENT *)NULL)
		cpu_acpanic("icache");
	key.cache.c_bsize = 2;
	key.cache.c_lsize = log2(_icache_linesize);
	key.cache.c_size = log2(_icache_size >> 12);
	tmp->Key = key.FullKey;

	tmp = AddChild(c, &cachetmpl, (void *)NULL);
	if (tmp == (COMPONENT *)NULL)
		cpu_acpanic("dcache");
	key.cache.c_bsize = 2;
	key.cache.c_lsize = log2(_dcache_linesize);
	key.cache.c_size = log2(_dcache_size >> 12);
	tmp->Type = PrimaryDCache;
	tmp->Key = key.FullKey;

	if (_sidcache_size != 0) {
		tmp = AddChild(c, &cachetmpl, (void *)NULL);
		if (tmp == (COMPONENT *)NULL)
			cpu_acpanic("s_cache");
		key.cache.c_bsize = 2;
		key.cache.c_lsize = log2(_scache_linesize);
		key.cache.c_size = log2(_sidcache_size >> 12);
		tmp->Type = SecondaryCache;
		tmp->Key = key.FullKey;
	}
}

void
cpu_install(COMPONENT *root)
{
	heart_piu_t *heart = HEART_PIU_K1PTR;
	heartreg_t stat = heart->h_status;
	COMPONENT *tmp;
	int i, n;

	inventory_cpu(root,0);

	/* count all (even disabled) cpus in inventory */
	n = cpu_probe_all();
	for (i=1; i < n; i++)
		inventory_cpu(root,i);

	tmp = AddChild(root, &memtmpl, (void *)NULL);
	if (tmp == (COMPONENT *)NULL)
		cpu_acpanic("main memory");
	tmp->Key = cpu_get_memsize() >> ARCS_BPPSHIFT;	/* number of 4k pages */
}

#define	CLEAR_STATUS	1
/*ARGSUSED*/
void
cpu_show_fault(unsigned long saved_cause)
{
	heartreg_t heart_isr;
	int wid, errvec;

	heart_isr= print_heart_status(CLEAR_STATUS);
	(void)print_xbow_status((__psunsigned_t)XBOW_K1PTR, CLEAR_STATUS);
	(void)print_bridge_status(BRIDGE_K1PTR, CLEAR_STATUS);
	(void)print_ioc3_status(PHYS_TO_COMPATK1(IOC3_PCI_CFG_BASE),
		CLEAR_STATUS);

	/*
	 * handle the expansion XIO slots (ports 9,10,11,12,13,14)
	 * we start with heart intr bit 51 (port 9) and work our way
	 * to bit 56 (port 14). 
	 * we will print the bridge status if widget is bridge
	 * otherwise minimum info and the heart intr mask will be
	 * cleared to prevent further intrs from this device.
	 */
	errvec = WIDGET_ERRVEC_BASE;
	for (wid = HEART_ID+1; wid < BRIDGE_ID; errvec++, wid++) {
	    widget_cfg_t *widget;
	    widgetreg_t id;
	    int part;

	    if ((heart_isr & (HEARTCONST 1 << errvec)) == 0)
		continue;

	    widget = (widget_cfg_t *) K1_MAIN_WIDGET(wid);
	    id = widget->w_id;
	    part = (id & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT;

	    if (part == BRIDGE_WIDGET_PART_NUM)
		(void)print_bridge_status((bridge_t *)widget,
					  CLEAR_STATUS);
	    else {
		printf("Widget(id=0x%x) : partno 0x%x\n",
			widget->w_control & BRIDGE_CTRL_WIDGET_ID_MASK,
			part);
	    }
	    HEART_PIU_K1PTR->h_imr[master_cpuid] &= ~(HEARTCONST 1 << errvec);	
	}

	/* Re-enable power interrupt */
	ip30_setup_power();
	set_SR(SR_PROMBASE | SR_IE | SR_IBIT_BERR | SR_POWER);
}
#endif /* !IP30_RPROM */

/* hit the system reset bit on the cpu */
void
cpu_hardreset(void)
{
	/*
	 * Or in HM_SOFTWARE_RST in case there are bits in the register
	 * that are preserved across a reset.
	 */
	HEART_PIU_K1PTR->h_mode |= HM_SW_RST;
	flushbus();
}

__uint64_t memsize;

__uint64_t
cpu_get_memsize(void)
{
	int bank;

	if (!memsize) {
		for (bank = 0; bank < 4; bank++)
			memsize += banksz(bank);	/* sum up bank sizes */
	}

	return memsize;
}

static __uint32_t
cpu_get_membase(void)
{
	return (unsigned int)PHYS_RAMBASE;
}

char *
cpu_get_disp_str(void)
{
	return "video()";
}
char *
cpu_get_kbd_str(void)
{
	return "keyboard()";
}
char *
cpu_get_serial(void)
{
	return "serial(0)";
}

#ifndef IP30_RPROM
void
cpu_errputc(char c)
{
	ns16550cons_errputc(c);
}

void
cpu_errputs(char *s)
{
	while (*s)
		ns16550cons_errputc(*s++);
}
#endif /* !IP30_PROM */

void
cpu_mem_init(void)
{
	/* Add all of main memory with the exception of four pages that
	 * will be mapped only to physical 0 (in rev B and later hearts).
	 */
#define	ONE_GIGABYTE	(0x1 << 30)
#define NPHYS0_PAGES	2
#define NALIAS_PAGES	(arcs_btop(SYSTEM_MEMORY_ALIAS_SIZE) - NPHYS0_PAGES)
#define NLOW_PAGES	(NPHYS0_PAGES+NALIAS_PAGES)
	if (cpu_get_memsize() <= ONE_GIGABYTE)
		md_add(cpu_get_membase() + arcs_ptob(NLOW_PAGES),
			arcs_btop(cpu_get_memsize()) - NLOW_PAGES, FreeMemory);
	else {
		/*
		 * since the bridge's PCI 32-bit direct mapped mode supports
		 * only up to 1.5/2GB and PROM loads relocatable program,
		 * ie sash, at the top of memory, system with > 1.5/2GB
		 * will fail.  we can change all PCI device drivers to do
		 * 64 bits DMA, or restrict DMA to portion of memory, we
		 * opt for the latter.  one gigabyte is more than enough
		 * for all DMAs the PROM need to do
		 */
		md_add(cpu_get_membase() + arcs_ptob(NLOW_PAGES),
			arcs_btop(ONE_GIGABYTE) - NLOW_PAGES, FreeMemory);
		md_add(cpu_get_membase() + ONE_GIGABYTE,
			arcs_btop(cpu_get_memsize() - ONE_GIGABYTE),
			FirmwarePermanent);
	}
	md_add(0, NPHYS0_PAGES, FreeMemory);

	/* 8K-16K is not availiable for general use as it is mapped to
	 * 0 by the heart.
	 */
	md_add(arcs_ptob(NPHYS0_PAGES), NALIAS_PAGES, FirmwarePermanent);
}

void
cpu_acpanic(char *str)
{
	printf("Cannot malloc %s component of config tree\n", str);
	panic("Incomplete config tree -- cannot continue.\n");
}

/* IP30 cannot save configuration to NVRAM */
LONG
cpu_savecfg(void)
{
	return ENOSPC;
}

char *
cpu_get_mouse(void)
{
	return "pointer()";
}

/* Note: On multi-bit cache errors, we cannot figure out which chip is bad
 * on the R10000.
 */
/*ARGSUSED1*/
void
ecc_error_decode(u_int ecc_error_reg)
{
}

void
cpu_checkpower(void)
{
	heart_piu_t *heart = HEART_PIU_K1PTR;

	if (heart->h_imsr & (1ull << IP30_HVEC_POWER)) {
		cpu_soft_powerdown();
		/*NOTREACHED*/
	}
}

void
cpu_scandevs(void)
{
	cpu_checkpower();
}

#ifndef IP30_RPROM
/* called from the fault handlers */
int
ip30_checkintrs(__psunsigned_t epc, __psunsigned_t ra)
{
	heartreg_t h_cause, heart_mode;
	__uint64_t badaddr;
	heart_piu_t *heart;
	bridge_t *bridge;

	cpu_checkpower();

	bridge = BRIDGE_K1PTR;

	/* Check for AC power failures */
	if (bridge->b_int_status & (1<<BRIDGE_ACFAIL)) {
		restore_ioc3_sio(slow_ioc3_sio());	/* may stick slow */
		powerspin();
	}

	/*  Check base bridge for illegal addresses to the prom range.  This
	 * happens when the T5 hits the d$ speculation problem to cache prom
	 * addresses.  Or, of course when we have have bad code that accesses
	 * the prom.
	 */
	if (bridge->b_int_status & BRIDGE_ISR_INVLD_ADDR) {
		badaddr = ((__uint64_t)bridge->b_wid_err_upper << 32) |
			  bridge->b_wid_err_lower;
		if (badaddr >= 0xc00000 && badaddr < 0xffffff) {
			static __uint64_t	last_badaddr;
			static __psunsigned_t	last_epc;
			static __psunsigned_t	last_ra;

			bridge->b_int_rst_stat = BRIDGE_IRR_REQ_DSP_GRP;
			bridge->b_wid_tflush;
			if (epc != last_epc || ra != last_ra ||
			    badaddr != last_badaddr) {
				printf("\n*** PROM write error on cacheline 0x1f%x at PC=0x%x RA=0x%x\n",
					badaddr, epc, ra);

				last_badaddr = badaddr;
				last_epc = epc;
				last_ra = ra;
			}
			return 1;
		}
	}

	/* handle single-bit, correctable errors in the prom */
	heart = HEART_PIU_K1PTR;
	h_cause = heart->h_cause;

	if ((h_cause & (HC_COR_MEM_ERR | HC_NCOR_MEM_ERR)) == HC_COR_MEM_ERR) {
		extern void		__dcache_wb_inval(void *, int);
		paddr_t			badaddr;
		int			error_type = 0;
		heartreg_t		h_mem_req_arb;
		heartreg_t		h_memerr_err;
		heartreg_t		h_memerr_data;
		extern struct reg_desc	heart_mem_err_desc[];
		paddr_t			k0_badaddr;

		h_memerr_err = heart->h_memerr_addr;
		h_memerr_data = heart->h_memerr_data;
		badaddr = h_memerr_err & HME_PHYS_ADDR;

		/* clear the error and unlatch error logging registers */
		heart->h_cause = HC_COR_MEM_ERR |
			(h_cause & HC_PE_SYS_COR_ERR_MSK);
		flushbus();

		/*
	 	 * on a DSPW write with single bit error, the MIU
		 * will correct the data before writing it back
		 * into memory.  in this case, if we correct the
		 * data blindly, we will end up corrupting the memory.
	 	 * re-reading the bad location should tell us whether the
	 	 * bad bit is already fixed or not
		 */
		k0_badaddr = PHYS_TO_K0(badaddr & ~0x7);

		/* do not serve io to memory request temporary */
		h_mem_req_arb = heart->h_mem_req_arb;
		heart->h_mem_req_arb = h_mem_req_arb | HEART_MEMARB_IODIS;
		flushbus();

		/*
		 * use cached address for MP coherency, dirty the cacheline
		 * then write it back
		 */
		*(volatile __uint64_t *)k0_badaddr += 0;
		__dcache_wb_inval((void *)k0_badaddr, 8);

		/* ok to serve io to memory request */
		heart->h_mem_req_arb = h_mem_req_arb;
		flushbus();

		if ((h_cause = heart->h_cause) & HC_COR_MEM_ERR) {
			/* clear the error again */
			heart->h_cause = HC_COR_MEM_ERR |
				(h_cause & HC_PE_SYS_COR_ERR_MSK);
			flushbus();

			error_type = 1;		/* definitely hard error */
		}

		printf("*** Fixed correctable %smemory error!\n"
			"    MemData 0x%016X %s\n"
			"    MemErr %R\n",
			error_type ? "hard " : "",
			h_memerr_data, cpu_memerr_to_dimm(h_memerr_err),
			h_memerr_err, heart_mem_err_desc);

		return 1;
	}

	/* Other processor reported correctable errors */
	if (h_cause & HC_PE_SYS_COR_ERR_MSK) {
		heart->h_cause = h_cause & HC_PE_SYS_COR_ERR_MSK;
		flushbus();
	}

	return 0;
}
#endif /* !IP30_RPROM */

void
bell(void)
{
	/* XXX BUG 421254 need to implement kbd bell */
}

/* check if address is inside a "protected" area */
/*ARGSUSED*/
int
is_protected_adrs(unsigned long low, unsigned long high)
{
	return 0;		/* not protected */
}

/*
 * void launch_slave()
 *	set up the MPCONF entry for the given cpu and launches
 *	it.  note that the routine checks to see if the vpid 
 *	is equal to the vpid of the processor this function is
 *	running on.  if it is, it just calls the launch and 
 *	rendezvous function.
 */

int
launch_slave(int virt_id, void (*lnch_fn)(void *), void *lnch_parm, 
	     void (*rndvz_fn)(void *), void *rndvz_parm, void *stack)
{
	volatile mpconf_t *mpconf_ptr;

	/* make sure virt_id is valid */
	if (virt_id >= MAXCPU || virt_id < 0) {
	  	printf("launch_slave: error: virt_id (%d) is too big.\n", 
			virt_id);
	   	return -1;
	}
	
	/* check to see if virtual ID is current processor */
	if (virt_id == cpuid()) {
		/* call launch and rendezvous parameters */
		lnch_fn(lnch_parm);
		if (rndvz_fn)
			rndvz_fn(rndvz_parm);
	}
	else {
		mpconf_ptr = (volatile mpconf_t *)
			(MPCONF_ADDR + MPCONF_SIZE * virt_id);

		/* set up the MP Block entry */
		mpconf_ptr->lnch_parm = lnch_parm;
		mpconf_ptr->rendezvous = (void *)rndvz_fn;
		mpconf_ptr->rndv_parm = rndvz_parm;
		mpconf_ptr->stack = stack;

		/* do this last! - it will launch immediately */
		mpconf_ptr->launch = (void *)lnch_fn;
	}

	return 0;
}

/*
 * void slave_loop(void)
 *	slave processor idle-loop.  the slaves periodically check to
 *	see if anyone has set a launch address.  if so, this routine 
 *	executes the launched code, passing the launch parameter.
 */

int 
slave_loop(void)
{
	uint vpid = cpuid();		/* virt CPU ID of this CPU */
	void (*launch)(void *);		/* launch function */
	void (*rndvz)(void *);		/* rendezvous function */
	volatile mpconf_t *mpconf_ptr;	/* ptr to cpu's MPCONF block */

	if (Debug)
		printf("CPU %d in slave_loop...\n", vpid);

	mpconf_ptr = (volatile mpconf_t *)(MPCONF_ADDR + MPCONF_SIZE * vpid);
	mpconf_ptr->idle_flag = 1;

	while (1) {
	        us_delay(1000);

		if (!mpconf_ptr->launch)
			continue;

		if (Debug)
			printf("\nReceived launch request (0x%x)\n",
				mpconf_ptr->launch);

		/* make sure this is a valid MPCONF block */
		if (mpconf_ptr->mpconf_magic != MPCONF_MAGIC || 
		    mpconf_ptr->virt_id != vpid) {
			mpconf_ptr->launch = 0;
			printf("slave_loop: Error: Invalid MPCONF block\n");
			continue;
		}

		/* call the launch routine */
		launch = (void (*)())mpconf_ptr->launch;
		mpconf_ptr->launch = 0;
		mpconf_ptr->idle_flag = 0;
		if (mpconf_ptr->stack == (void *)0)
			launch((void *)mpconf_ptr->lnch_parm);
		else
			call_coroutine(launch,
				(void *)mpconf_ptr->lnch_parm,
				(void *)mpconf_ptr->stack);

		/* call the rendezvous routine */
		if (mpconf_ptr->rendezvous != 0) {
			rndvz = (void (*)())mpconf_ptr->rendezvous;
			mpconf_ptr->rendezvous = 0;
			if (mpconf_ptr->stack == (void *)0)
				rndvz((void *)mpconf_ptr->rndv_parm);
			else
				call_coroutine(rndvz,
					(void *)mpconf_ptr->rndv_parm,
					(void *)mpconf_ptr->stack);
		}

		mpconf_ptr->idle_flag = 1;
	}
	/*NOTREACHED*/
}

/*
 * Translate a Bus IO address into virtual address (uncached io)
 * input:
 *	xport:		bridge xbow port number
 *	io_space:	MainIO, LargeIO, HugeIO 
 *			use MAIN_IO_SPACE, LARGE_IO_SPACE, HUGE_IO_SPACE
 *			defined in sys/RACER/heart.h
 *	devaddr:	offset to device from the base of PCI/GIO PIO addr
 *			0 - 0x3FFF_FFFF (1 GB)
 * return
 *	kernel virtual address to the device suitable for PIO
 */
__psunsigned_t
bridge_to_kv_pio(int xport, iopaddr_t io_space, __uint32_t devaddr)
{
    __psunsigned_t kvaddr;

    if (devaddr > BRIDGE_MAX_PIO_ADDR_MEM) 
	panic("bridge_to_kv_pio: bad device addr %x\n", devaddr);

    switch (io_space) {
	case MAIN_IO_SPACE:
	    kvaddr = K1_MAIN_WIDGET(xport) + devaddr;
	    break;

	case LARGE_IO_SPACE:
	    kvaddr = K1_LARGE_WIDGET(xport);
	    kvaddr += BRIDGE_PIO32_XTALK_ALIAS_BASE + devaddr;
	    break;

	case HUGE_IO_SPACE:
	    kvaddr = K1_HUGE_WIDGET(xport);
	    kvaddr += BRIDGE_PIO32_XTALK_ALIAS_BASE + devaddr;
	    break;

	default:
	    panic("bridge_to_kv_pio: bad io space 0x%x\n", io_space);
    }
    return(kvaddr);
}

/*
 * Translate a virtual memory address to 32 bit direct-mapped address
 * for use by the DMA master of the device on a Bridge Bus (PCI/GIO)
 * based on the direct map configuration of the specified Bridge.
 * input:
 *	pbridge:	pointer to the bridge.
 *	kvaddr:		kernel virtual address of memory location
 * return:
 *	32bit PCI/GIO address for DMA to/from host memory
 */
__psunsigned_t
kv_to_bridge32_dirmap(void *pbridge, __psunsigned_t kvaddr)
{
	bridge_t 	*bridge = (bridge_t *)pbridge;
	bridgereg_t	dirmap_reg = bridge->b_dir_map;
	__psunsigned_t	physaddr;
	__psunsigned_t	xtalk_addr_low;

	/*
	 * left shift 31 bit is equivalent to multiplying by
	 * BRIDGE_DIRECT_32_SEG_SIZE == 2^31, 2GB's
	 */
	xtalk_addr_low = (dirmap_reg & BRIDGE_DIRMAP_OFF) << 31;
	if (xtalk_addr_low == 0 && dirmap_reg & BRIDGE_DIRMAP_ADD512)
		xtalk_addr_low = ABS_SYSTEM_MEMORY_BASE;	/* 512MB */

	physaddr = KDM_TO_PHYS(kvaddr);
	if (physaddr < xtalk_addr_low ||
	    physaddr >= xtalk_addr_low + BRIDGE_DIRECT_32_SEG_SIZE)
		panic("Cannot map 0x%x into the current PCI-32 direct mapped space\n", kvaddr);

	return(physaddr - xtalk_addr_low + BRIDGE_DMA_DIRECT_BASE);
}

/*
 * Translate a 32bit PCI/GIO direct map dma address of a Bridge
 * to physical memory address based on the dirmap configuration of
 * the specified Bridge.
 * input:
 *	pbridge:	pointer to the bridge
 *	daddr:		32bit PCI/GIO direct mapped address
 * return:
 *	host memory physical address
 */
__psunsigned_t
bridge32_dirmap_to_phys(void *pbridge, __uint32_t daddr)
{
	bridge_t 	*bridge = (bridge_t *)pbridge;
	bridgereg_t	dirmap_reg = bridge->b_dir_map;
	__psunsigned_t	xtalk_addr_low;

	/*
	 * left shift 31 bit is equivalent to multiplying by
	 * BRIDGE_DIRECT_32_SEG_SIZE == 2^31, 2GB
	 */
	xtalk_addr_low = (dirmap_reg & BRIDGE_DIRMAP_OFF) << 31;
	if (xtalk_addr_low == 0 && dirmap_reg & BRIDGE_DIRMAP_ADD512)
		xtalk_addr_low = ABS_SYSTEM_MEMORY_BASE;	/* 512MB */

	if (daddr < BRIDGE_DMA_DIRECT_BASE ||
	    daddr - BRIDGE_DMA_DIRECT_BASE >= BRIDGE_DIRECT_32_SEG_SIZE)
		panic("Cannot map 0x%x into the CPU physical memory space\n", daddr);
	return(daddr - BRIDGE_DMA_DIRECT_BASE + xtalk_addr_low);
}

/* If password jumper is off, return 1 (override passwd, force console=g)
 */
int
jumper_off(void)
{
	return ((BRIDGE_K1PTR->b_int_status & PROM_PASSWD_DISABLE) != 0);
}

/*  Needs address to find double word side of cache.  Both datums
 * need to be read from double aligned addresses.
 *
 *  For modules with x18 IBM SSRAMS:
 *
 *                              SSRAM   SSRAM   SSRAM
 *      cache	addr	bytes   PM10	PM20-0	PM20-1
 *	-----	----	-----   ----    ------  ------
 *	00-15	0x08	6/7     U517	U524    U529
 *	16-31	0x08	4/5	U11	U11	U19
 *	32-47	0x08	2/3	U518	U525	U530
 *	48-63	0x08	0/1	U10	U10	U18
 *	64-79	0x00	6/7	U516	U523	U528
 *	80-95	0x00	4/5	U9	U9	U17
 *	96-111	0x00	2/3	U515	U522	U527
 *	112-127	0x00	0/1	U7	U7	U15
 *
 *  For modules with x36 IBM SSRAMS:
 *
 *                              SSRAM   SSRAM   SSRAM
 *      cache	addr	bytes   PM10	PM20-0	PM20-1
 *	-----	----	-----   ----    ------  ------
 *	00-31	0x08	4-7     U11	U11	U19
 *	32-63	0x08	0-3	U10	U10	U18
 *	64-95	0x00	4-7	U9	U9	U17
 *	96-127	0x00	0-3	U7	U7	U15
 *
 * The data must be swizzled a bit to match the schematic:
 *	DDDDCCCCBBBBAAAA -> BBBBAAAADDDDCCCC
 *
 * ip30_ssram() assumes the data has been passed through ip30_ssram_swizzle()
 * already.
 */
uint64_t
ip30_ssram_swizzle(uint64_t data)
{
	return ((data & 0xffffffff00000000) >> 32) |
	       ((data & 0x00000000ffffffff) << 32);
}
char *
ip30_ssram(void *addr, __uint64_t expected, __uint64_t actual, int cpu)
{
	static char *ssrams_0[2][4] = {			/* PM20 proc 0 */
		{ "U524 ", "U11 ", "U525 ", "U10 " },
		{ "U523 ", "U9 ",  "U522 ", "U7 "  }
	};
	static char *ssrams_1[2][4] = {			/* PM20 proc 1 */
		{ "U529 ", "U19 ", "U530 ", "U18 " },
		{ "U528 ", "U17 ", "U527 ", "U15 " }
	};
	static char *ssrams_x[2][4] = {			/* PM10 */
		{ "U517 ", "U11 ", "U518 ", "U10 " },
		{ "U516 ", "U9 ",  "U515 ", "U7 "  }
	};
	int i, side = ((__psunsigned_t)addr & 8) == 0;	/* 8 -> 0, 0 -> 1 */
	static char buf[(4*5)+1];
	char **ssrams;
	extern unsigned _sidcache_size;

	*buf = '\0';

	if (cpu_probe_all() == 2) {
		/* XXX 431675: should depend on NIC value for PM20 */
		ssrams = ((cpu == 0) ? ssrams_0 : ssrams_1)[side];
	}
	else
		ssrams = ssrams_x[side];

	if (_sidcache_size == 0x100000) {	/* x18 SSRAMs - 1MB scache */
		for (i=0; i < 4; i++) {
			if ((expected ^ actual) & 0xffff) {
				strcat(buf,ssrams[i]);
			}
			expected >>= 16;
			actual >>= 16;
		}
	}
	else {	/* x36 SSRAMs - 2MB scache */
		if ((expected ^ actual) & 0xffffffff)
			strcat(buf,ssrams[1]);
		if ((expected ^ actual) >> 32)
			strcat(buf,ssrams[3]);
	}

	return buf;
}
char *
ip30_tag_ssram(int cpu)
{
	/* XXX 431675: should depend on NIC value for PM20 */
	if (cpu_probe_all() == 2)
		return cpu ? "U14" : "U6";
	return "U6";
}

#ifdef US_DELAY_DEBUG
/* rough check of us_delay.  Is not too accurate as done in C.  Caller needs
 * to convert from C0_COUNT ticks @ 1/2 freqency of CPU to determine time.
 */
__uint32_t us_before, us_after;
static void
check_us_delay(void)
{
	int i;
	extern ulong uc_decinsperloop, decinsperloop;

	us_delay(1);		/* prime cache */

	for (i = 0; i < 10; i++) {
		us_delay(i);
		printf("us_delay(%d) took a little less than 0x%x - 0x%x = "
			"%u ticks.\n",
			i,(__psunsigned_t)us_before,(__psunsigned_t)us_after,
			us_after-us_before);
	}
	printf("uc_decinsperloop=%d decinsperloop=%d\n",
		uc_decinsperloop, decinsperloop);

	return;
}
#endif	/* US_DELAY_DEBUG */

#ifndef IP30_RPROM
void
cpu_clearnofault(void)
{
	bridge_clearnofault();
	/* only for ide ecc test, postpone clearing the heart registers */
	if (!ide_ecc_test) heart_clearnofault();
}

/* Framework to add 'misc' devices to the inventory list.  With card ids
 * hopefully we can support better naming easily and cheaply.
 */
static COMPONENT*
addmisc(COMPONENT *p, CONFIGCLASS class, CONFIGTYPE type, ULONG key, char *id)
{
	COMPONENT tmpl;

	tmpl.Class = class;
	tmpl.Type = type;
	tmpl.Flags = 0;
	tmpl.Version = SGI_ARCS_VERS;
	tmpl.Revision = SGI_ARCS_REV;
	tmpl.Key = key;
	tmpl.AffinityMask = 0x1;
	tmpl.ConfigurationDataSize = 0;
	tmpl.IdentifierLength = strlen(id) + 1;
	tmpl.Identifier = id;

	p = AddChild(p,(COMPONENT *)&tmpl,0);
	if (p == (COMPONENT *)NULL) cpu_acpanic("misc");
	return p;
}

#define MAXHINVS	8
static slotdrvr_id_t hinvids[MAXHINVS] = {
	{BUS_PCI, 0x10a9, 0x5, REV_ANY},		/* 0 */
	{BUS_XTALK,HQ4_WIDGET_MFGR_NUM,HQ4_WIDGET_PART_NUM,REV_ANY}, /* 1 */
	{BUS_PCI, 0x10b6, 0x2,    REV_ANY},		/* 2 */
	{BUS_PCI, 0x10b6, 0x4,    REV_ANY},		/* 3 */
	{BUS_PCI, 0x1112, 0x2200, REV_ANY},		/* 4 */
	{BUS_XTALK,TPU_WIDGET_MFGR_NUM,TPU_WIDGET_PART_NUM,REV_ANY},	/* 5 */
	{BUS_XTALK,ODSY_XT_ID_MFG_NUM_VALUE,ODSY_XT_ID_PART_NUM_VALUE,REV_ANY}, /* 6 */
	{BUS_NONE, 0, 0, 0}				/* 7 */
};
drvrinfo_t hinv_drvrinfo = { hinvids, "hinv" };

/* Keep track of PCI bus, and hinv count per bus so hinv keys are right */
static COMPONENT *hinvparent[MAXHINVS];
static char hinvcnt[MAXHINVS];

int
hinv_install(COMPONENT *p, slotinfo_t *slot)
{
	char pbuf[128];

	if (slot->drvr_index < MAXHINVS) {
		if (p != hinvparent[slot->drvr_index]) {
			hinvparent[slot->drvr_index] = p;
			hinvcnt[slot->drvr_index] = 0;
		}
		else
			hinvcnt[slot->drvr_index]++;
	}

	/*
	 * the drvr_index is the index of the hinvids[]
	 * case ##: should match the static array initialization
	 */
	switch (slot->drvr_index) {
	case 0:
		addmisc(p, ControllerClass,AudioController,
			hinvcnt[slot->drvr_index], "RAD Audio Processor");
		break;

	/* case 1: taken by mgras driver internal probe */
	case 1: break;

	case 2:
	case 3:
		sprintf(pbuf, "slot %d Madge PCI Token Ring part 0x%x rev %d",
			slot->bus_slot, slot->id.part, slot->id.rev);
		addmisc(p, PeripheralClass, OtherPeripheral,
			hinvcnt[slot->drvr_index], pbuf);
		break;

	case 4:
		sprintf(pbuf, "slot %d RNS %x PCI FDDI rev %d",
			slot->bus_slot, slot->id.part, slot->id.rev);
		addmisc(p, PeripheralClass, OtherPeripheral,
			hinvcnt[slot->drvr_index], pbuf);
		break;

	case 5:
		sprintf(pbuf, "slot %d TPU ASIC rev %d",
			slot->bus_slot, slot->id.rev);
		addmisc(p, AdapterClass, TPUAdapter,
			slot->bus_slot, pbuf);
		break;

	/* case 6: taken by odsy driver internal probe */
	case 6: break;


	}

	return 0;
}
#endif /* !IP30_RPROM */

/* Use heart as R10K counter does not free run in the kernel.
 */
ULONG
GetRelativeTime(void)
{
	return (HEART_PIU_K1PTR->h_count/HEART_COUNT_RATE);
}

/* from irix/kern/ml/error.c */
char *etstrings[] = {"OK", "DB", "CB", "2 Bit", "3 Bit", "4 Bit", "Fatal"};

eccdesc_t data_eccsyns[] = {
/*	  0|8	  1|9	  2|A     3|B     4|C     5|D     6|E     7|F    */
/* 0*/  {OK, 0},{CB, 0},{CB, 1},{B2, 0},{CB, 2},{B2, 0},{B2, 0},{DB, 7},
/* 8*/  {CB, 3},{B2, 0},{B2, 0},{DB,54},{B2, 0},{DB, 6},{DB,55},{B2, 0},
/*10*/  {CB, 4},{B2, 0},{B2, 0},{DB, 0},{B2, 0},{DB,20},{DB,48},{B2, 0},
/*18*/  {B2, 0},{DB,24},{DB,28},{B2, 0},{DB,16},{B2, 0},{B2, 0},{DB,52},
/*20*/  {CB, 5},{B2, 0},{B2, 0},{DB, 1},{B2, 0},{DB,21},{DB,49},{B2, 0},
/*28*/  {B2, 0},{DB,25},{DB,29},{B2, 0},{DB,17},{B2, 0},{B2, 0},{DB, 4},
/*30*/  {B2, 0},{DB,44},{DB,45},{B2, 0},{DB,46},{B2, 0},{B2, 0},{B3, 0},
/*38*/  {DB,47},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*40*/  {CB, 6},{B2, 0},{B2, 0},{DB, 2},{B2, 0},{DB,22},{DB,50},{B2, 0},
/*48*/  {B2, 0},{DB,26},{DB,30},{B2, 0},{DB,18},{B2, 0},{B2, 0},{DB,10},
/*50*/  {B2, 0},{DB,32},{DB,33},{B2, 0},{DB,34},{B2, 0},{B2, 0},{B3, 0},
/*58*/  {DB,35},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*60*/  {B2, 0},{DB,12},{DB,13},{B2, 0},{DB,14},{B2, 0},{B2, 0},{B3, 0},
/*68*/  {DB,15},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*70*/  {DB, 9},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*78*/  {B2, 0},{B3, 0},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B2, 0},{UN, 0},
/*80*/  {CB, 7},{B2, 0},{B2, 0},{DB, 3},{B2, 0},{DB,23},{DB,51},{B2, 0},
/*88*/  {B2, 0},{DB,27},{DB,31},{B2, 0},{DB,19},{B2, 0},{B2, 0},{DB,58},
/*90*/  {B2, 0},{DB,36},{DB,37},{B2, 0},{DB,38},{B2, 0},{B2, 0},{B3, 0},
/*98*/  {DB,39},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*a0*/  {B2, 0},{DB,40},{DB,41},{B2, 0},{DB,42},{B2, 0},{B2, 0},{B3, 0},
/*a8*/  {DB,43},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*b0*/  {DB,56},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*b8*/  {B2, 0},{B3, 0},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B2, 0},{UN, 0},
/*c0*/  {B2, 0},{DB,60},{DB,61},{B2, 0},{DB,62},{B2, 0},{B2, 0},{B3, 0},
/*c8*/  {DB,63},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*d0*/  {DB, 8},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*d8*/  {B2, 0},{B3, 0},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B2, 0},{UN, 0},
/*e8*/  {DB,57},{B2, 0},{B2, 0},{B3, 0},{B2, 0},{B3, 0},{B3, 0},{B2, 0},
/*e8*/  {B2, 0},{B3, 0},{B3, 0},{B2, 0},{B3, 0},{B2, 0},{B2, 0},{UN, 0},
/*f8*/  {B2, 0},{DB, 5},{DB,53},{B2, 0},{DB,59},{B2, 0},{B2, 0},{UN, 0},
/*f8*/  {DB,11},{B2, 0},{B2, 0},{UN, 0},{B2, 0},{UN, 0},{UN, 0},{B2, 0},
};

#endif	/*  IP30 (whole file) */
