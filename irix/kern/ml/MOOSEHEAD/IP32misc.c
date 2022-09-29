#include <sys/types.h>
#include <sys/timers.h>
#include <sys/ktime.h>
#include <sys/immu.h>
#include <sys/proc.h>
#include <sys/pfdat.h>
#include <sys/pda.h>
#include <sys/callo.h>
#include <sys/cpu.h>
#include <sys/fpu.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/invent.h>
#include <sys/syssgi.h>
#include <sys/conf.h>
#include <sys/mace.h>
#include <sys/ds17287.h>
#include <sys/edt.h>
#include <sys/debug.h>
#include <sys/kopt.h>
#include <ksys/vproc.h>
#include <sys/arcs/spb.h>
#include <sys/arcs/debug_block.h>
#include <sys/PCI/pciio.h>

struct kmem_ioaddr kmem_ioaddr[] = {
    {0, 0},
};

void set_leds(int);
void reset_leds(void);

extern int clean_shutdown_time; 	/* mtune/kernel */
#ifdef POWER_DUMP
extern int power_button_changed_to_crash_dump;		/* mtune/kernel */
#endif

#define ISXDIGIT(c) ((('a'<=(c))&&((c)<='f'))||(('0'<=(c))&&((c)<='9')))

#define HEXVAL(c) ((('0'<=(c))&&((c)<='9'))? ((c)-'0')  : ((c)-'a'+10))

unsigned char *
etoh (char *enet)
{
    static unsigned char dig[6], *cp;
    int i;

    for ( i = 0, cp = (unsigned char *)enet; *cp; ) {
	if ( *cp == ':' ) {
	    cp++;
	    continue;
	} else if ( !ISXDIGIT(*cp) || !ISXDIGIT(*(cp+1)) ) {
	    return NULL;
	} else {
	    if ( i >= 6 )
		return NULL;
	    dig[i++] = (HEXVAL(*cp) << 4) + HEXVAL(*(cp+1));
	    cp += 2;
	}
    }
    
    return i == 6 ? dig : NULL;
}

/*
 *	pass back the serial number associated with the system
 *  ID prom. always returns zero.
 */
int
getsysid(char *hostident)
{
    extern uint sys_id;

    /*
     * serial number is only 4 bytes long on IP22.  Left justify
     * in memory passed in...  Zero out the balance.
     */

    *(uint *) hostident = sys_id;
    bzero(hostident + 4, MAXSYSIDSIZE - 4);

    return 0;
}


/*  Flash LED from green to off to indicate system powerdown is in
 * progress.
 */
static void
flash_led(int on)
{
    timeout(flash_led, (void *)(on ? 0 : (__psint_t)3), HZ);
    set_leds(on);
}


/*
 * pg_setcachable set the cache algorithm pde_t *p. Since there are
 * several cache algorithms for the R4000, (non-coherent, update, invalidate),
 * this function sets the 'basic' algorithm for a particular machine. IP22
 * uses non-coherent.
 */
void
pg_setcachable(pde_t *p)
{
    pg_setnoncohrnt(p);
}

uint
pte_cachebits(void)
{
    return PG_NONCOHRNT;
}


/*
 * Initialize led counter and value
 */
void
reset_leds(void)
{
    __uint64_t led_reg;
    led_reg = READ_REG64(PHYS_TO_K1(ISA_FLASH_NIC_REG), __uint64_t);
    led_reg |= ISA_LED_RED|ISA_LED_GREEN;
    WRITE_REG64(led_reg, PHYS_TO_K1(ISA_FLASH_NIC_REG), __uint64_t);
    set_leds(0);
}


/*
 * Implementation of syssgi(SGI_SETLED,a1)
 *
 */
void
sys_setled(int a1)
{
    set_leds(a1);
}


#ifdef DEBUG
int do_bump_leds = 1;
#else
int do_bump_leds = 0;
#endif
time_t last_bump_time = 0;

/*
 * toggle the state of the red led to provide a heartbeat
 * during debugging.
 */
void
bump_leds(void)
{
    __uint64_t led_reg;
    extern time_t time;
    extern int power_off_flag;

    if (power_off_flag)
	return;
	
    led_reg = READ_REG64(PHYS_TO_K1(ISA_FLASH_NIC_REG), __uint64_t);

    if (!do_bump_leds && !(led_reg & ISA_LED_RED)) {
	set_leds(0);
	return;
    }

    if (do_bump_leds && (time != last_bump_time)) {
	set_leds((led_reg & ISA_LED_RED) ? 1 : 0);
	last_bump_time = time;
    }
}

/*
 * Set the leds to a particular pattern.  Used during startup, and during
 * a crash to indicate the cpu state.  The SGI_SETLED can be used to make
 * it change color.  We don't allow UNIX to turn the LED off.  Also, UNIX
 * is not allowed to turn the LED *RED* (a big no no in Europe).
 */
void
set_leds(int pattern)
{
    __uint64_t led_reg;
    led_reg = READ_REG64(PHYS_TO_K1(ISA_FLASH_NIC_REG), __uint64_t);
    led_reg &= ~(ISA_LED_GREEN|ISA_LED_RED);
    led_reg |= pattern ? 0 : ISA_LED_RED;
    WRITE_REG64(led_reg, PHYS_TO_K1(ISA_FLASH_NIC_REG), __uint64_t);
}

/*
 * get_except_norm
 *
 *	return address of exception handler for all exceptions other
 *	then utlbmiss.
 */
inst_t *
get_except_norm()
{
    extern inst_t exception[];

    return exception;
}


#if DEBUG

#include "sys/immu.h"

uint
tlbdropin(
    unsigned char *tlbpidp,
    caddr_t vaddr,
    pte_t pte)
{
    uint _tlbdropin(unsigned char *, caddr_t, pte_t);

    if (pte.pte_vr)
	ASSERT(pte.pte_cc == (PG_NONCOHRNT >> PG_CACHSHFT) || \
	       pte.pte_cc == (PG_UNCACHED >> PG_CACHSHFT));
    return(_tlbdropin(tlbpidp, vaddr, pte));
}

void tlbdrop2in(unsigned char tlb_pid, caddr_t vaddr, pte_t pte, pte_t pte2)
{
    void _tlbdrop2in(unsigned char, caddr_t, pte_t, pte_t);

    if (pte.pte_vr)
	ASSERT(pte.pte_cc ==  (PG_NONCOHRNT >> PG_CACHSHFT) || \
	       pte.pte_cc == (PG_UNCACHED >> PG_CACHSHFT));
    if (pte2.pte_vr)
	ASSERT(pte2.pte_cc ==  (PG_NONCOHRNT >> PG_CACHSHFT) || \
	       pte2.pte_cc == (PG_UNCACHED >> PG_CACHSHFT));
    _tlbdrop2in(tlb_pid, vaddr, pte, pte2);
}
#endif	/* DEBUG */

int
cpu_isvalid(int cpu)
{
    return !((cpu < 0) || (cpu >= maxcpus) || (pdaindr[cpu].CpuId == -1));
}

/*
 * This is just a stub on an IP32.
 */
/*ARGSUSED*/
void
debug_stop_all_cpus(void *stoplist)
{
    return;
}

pfn_t
pmem_getfirstclick(void)
{
    return btoc(SEG0_BASE);
}

/*
 * node_getfirstfree -	returns pfn of first page of unused ram. This
 *			allows prom code on some platforms to put things
 *			in low memory that the kernel must skip over
 *			when it's starting up.
 */
/*ARGSUSED*/
pfn_t
node_getfirstfree(cnodeid_t node)
{
    ASSERT(node == 0);
    return btoc(SEG0_BASE);
}

/*
 * getmaxclick - returns pfn of last page of ram
 */
/*ARGSUSED*/
pfn_t
node_getmaxclick(cnodeid_t node)
{
    extern int seg0_maxmem;
    extern int seg1_maxmem;
    extern pgno_t memsize;

    /* 
     * szmem must be called before getmaxclick because of
     * its depencency on maxpmem
     */
    ASSERT(memsize);

    if (seg1_maxmem)
	return btoc(SEG1_BASE) + seg1_maxmem - 1;
    else
	return btoc(SEG0_BASE) + memsize - 1;
}

#ifdef ONEMEG
#undef ONEMEG
#endif
#define ONEMEG (1024 * 1024)
int
bank_size(_crmreg_t r)
{
    return((r & CRM_MEM_BANK_CTRL_SDRAM_SIZE) ? (128 * ONEMEG) 
	   : (32 * ONEMEG));
}

static int 
_addr_to_ibank(paddr_t addr, int *ibank)
{
    _crmreg_t bankx_ctrl, bank0_ctrl;
    paddr_t bankx_addr;
    int bankx_size;
    int i;
    int bankx_mask;
    extern int bank_size(_crmreg_t);

    /* 
     * strip off bits 31:30 because only bits 29:25 
     * are found in the bank control registers.
     */
    addr &= ~0xc0000000;

    for (i = 0; i < CRM_MAXBANKS; i++) {
	bankx_ctrl = READ_REG64(PHYS_TO_K1(CRM_MEM_BANK_CTRL(i)), _crmreg_t);

	if (i == 0)
	    bank0_ctrl = bankx_ctrl;

	if ((i != 0) && (bank0_ctrl == bankx_ctrl))
	    continue;

	bankx_addr = CRM_MEM_BANK_CTRL_BANK_TO_ADDR(bankx_ctrl);
	bankx_mask = bankx_ctrl & CRM_MEM_BANK_CTRL_SDRAM_SIZE ?
	    0x38000000 : 0x3e000000;
	bankx_size = bank_size(bankx_ctrl);

	if ((bankx_addr <= (addr & bankx_mask)) && 
	    ((addr & bankx_mask) < (bankx_addr + bankx_size))) {
	    /* CRIME uses one more bit to select the internal bank */
	    if (ibank) {
		int ibank_select = bankx_ctrl & CRM_MEM_BANK_CTRL_SDRAM_SIZE ?
		    0x04000000 : 0x01000000;
		*ibank = i * 2 + ((addr & ibank_select) != 0);
	    }
	    return i;
	}
    }

    /* a bad address was passed to us */
    if (ibank)
	*ibank = -1;
    return(-1);
}

int 
addr_to_bank(paddr_t addr)
{
    return _addr_to_ibank(addr, NULL);
}

int 
addr_to_ibank(paddr_t addr)
{
    int ibank;
    (void)_addr_to_ibank(addr, &ibank);
    return ibank;
}

/*
 * shut down the power supply.  Dallas docs indicate that the following
 * conditions must hold for kickstart to work:
 *
 *	dv 2/1/0 in register a must have a value of 01xb
 *	((kf & kse) != 1) && ((wf & wie) != 1)
 *      kse == 1 to enable powerup via the power switch
 *
 * I make sure that these conditions hold here and then set pab to 1
 * which should shutdown the power supply.
 */
void
cpu_powerdown(void)
{
    register volatile ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int rega;
    
    flash_write_env();
    rega = clock->registera & ~DS_REGA_DV2;  /* clear rega dv2 */
    rega |= DS_REGA_DV1; /* set dv1 */
    while (1) {
	clock->registera = rega | DS_REGA_DV0; 
	flushbus();
	clock->ram[DS_BANK1_XCTRLB] |= DS_XCTRLB_ABE|DS_XCTRLB_KSE;
	clock->ram[DS_BANK1_XCTRLA] &= 
	    ~(DS_XCTRLA_KF|DS_XCTRLA_WF|DS_XCTRLA_RF);
	clock->ram[DS_BANK1_XCTRLA] |= DS_XCTRLA_PAB; /* shut down power */
	clock->registera = rega;
	flushbus();
	DELAY(1000000);
	cmn_err(CE_WARN, "Power supply did not shut down");
	DELAY(1000000);
    }
}

static void
cpu_godown(void)
{
    cmn_err(CE_WARN,
	    "init 0 did not shut down after %d seconds, powering off.\n",
	    clean_shutdown_time);
    DELAY(500000);	/* give them 1/2 second to see it */
    cpu_powerdown();
}

#define RTC_INT 0x100

void
cpu_waitforreset(void)
{
    volatile long long *p = (long long *)PHYS_TO_K1(ISA_INT_STS_REG);
    register volatile ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int x;

    flash_write_env();

    while (1) {
	x = spl7();
	if (*p & RTC_INT) {
	    /* clear interrupt condition */
	    if (!(clock->registerc & DS_REGC_IRQF)) {
		cmn_err(CE_WARN, "RTC irq received w/o RTC interrupt pending");
	    }
	    clock->registera |= DS_REGA_DV0;
	    flushbus();
	    if (clock->ram[DS_BANK1_XCTRLA] & DS_XCTRLA_KF)
		cpu_powerdown();
	    clock->registera &= ~DS_REGA_DV0;
	}
	splx(x);
    }
}

#ifdef POWER_DUMP
void
power_dump(int checkintr)
{
    int s = splprof();
    volatile long long *p = (long long *)PHYS_TO_K1(ISA_INT_STS_REG);
    register volatile ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    volatile int xctrla;

    if (checkintr && (*p & RTC_INT) == 0) {
	splx(s);
	return;
    }

    /* wait for power button debounce to clear the interrupts */
    DELAY(500000);
    REG_RDORWR8(&clock->registera, DS_REGA_DV0);
    flushbus();
    xctrla = pciio_pio_read8(&clock->ram[DS_BANK1_XCTRLA]);
    REG_RDANDWR8(&clock->ram[DS_BANK1_XCTRLA],
    		 ~(DS_XCTRLA_KF|DS_XCTRLA_WF|DS_XCTRLA_RF));
    REG_RDANDWR8(&clock->registera, ~DS_REGA_DV0);
    flushbus();

    cmn_err_tag(61,CE_PANIC, "Power Button induced crash from %s\n",
		checkintr ? "r4kcounter_intr":"powerintr");
    splx(s);
}
#endif


/*ARGSUSED*/
void
powerintr(struct eframe_s *ep, __psint_t arg)
{
    volatile long long *p = (long long *)PHYS_TO_K1(ISA_INT_STS_REG);
    static time_t doublepower;
    extern int power_off_flag;	/* set in powerintr(), or uadmin() */
    register volatile ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int xctrla;


    if (*p & RTC_INT) {

#ifdef POWER_DUMP
	if (power_button_changed_to_crash_dump)
	    power_dump(0);
#endif

	/* clear interrupt condition */
	if (!(clock->registerc & DS_REGC_IRQF)) {
	    cmn_err(CE_WARN, "RTC irq received w/o RTC interrupt pending");
	}
	REG_RDORWR8(&clock->registera, DS_REGA_DV0);
	flushbus();
	xctrla = pciio_pio_read8(&clock->ram[DS_BANK1_XCTRLA]);
	REG_RDANDWR8(&clock->ram[DS_BANK1_XCTRLA],
		     ~(DS_XCTRLA_KF|DS_XCTRLA_WF|DS_XCTRLA_RF));
	REG_RDANDWR8(&clock->registera, ~DS_REGA_DV0);
	flushbus();
	if (!(xctrla & DS_XCTRLA_KF)) {
	    /* what was it!? */
	    cmn_err(CE_WARN, "RTC irq received w/o kickstart condition.");
	    cmn_err(CE_WARN, "Clock interrupt status %x", xctrla);
	    return;
	}

	/* for prom_reboot() in ars.c */
	power_off_flag = 1;

	if (!doublepower) {
	    flash_led(3);

	    /*  Send SIGINT to init, which is the same as
	     * '/etc/init 0'.  Set a timeout
	     * in case init wedges.  Set doublepower to handle
	     * quick (with a little debounce room) 'doubleclick'
	     * on the power switch for an immediate shutdown.
	     */
	    sigtopid(INIT_PID, SIGINT, SIG_ISKERN|SIG_NOSLEEP, 0, 0, 0);
	    timeout(cpu_godown, 0, clean_shutdown_time*HZ);	
	    doublepower = lbolt + HZ;
	    set_autopoweron();
	}
	else if (lbolt > doublepower) {

	    DELAY(10000);		/* 10ms debounce time */

	    cmn_err(CE_NOTE, "Power switch pressed twice -- "
		    "shutting off power now.\n");

	    cpu_powerdown();
	}
	return;
    }
}

int
readadapters(uint_t bustype)
{
    switch (bustype) {
    case ADAP_SCSI:
	return(1);
    case ADAP_LOCAL:
	return(1);
    case ADAP_PCI:
	return(1);
    default:
	return(0);
    }
}

int
cpuboard(void)
{
    return INV_IP32BOARD;
}

/*
 * fp_find - probe for floating point chip
 */
void
fp_find(void)
{
        union rev_id ri;

        ri.ri_uint = private.p_fputype_word = get_fpc_irr();
        /*
         * Hack for the RM5271 processor. We simply mimic the
         * R5000 since it's interface is identical.
         */
        if (ri.ri_imp == C0_IMP_RM5271) {
                private.p_fputype_word &= ~C0_IMPMASK;
                private.p_fputype_word |= (C0_IMP_R5000 << C0_IMPSHIFT);
        }

}

/*
 * fp chip initialization
 */
void
fp_init(void)
{
    set_fpc_csr(0);
}

#define ARGS   a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15
void
dprintf(fmt, ARGS)
char *fmt;
long ARGS;
{
    if (SPB->DebugBlock && ((db_t *)SPB->DebugBlock)->db_printf)
	    (*((db_t *)SPB->DebugBlock)->db_printf)(fmt, ARGS);
    else if (cn_is_inited())
	    cmn_err(CE_CONT, fmt, ARGS);
}

paddr_t
get_isa_dma_buf_addr(void)
{
    extern paddr_t isa_dma_buf_addr;

    if (!isa_dma_buf_addr)
	isa_dma_buf_init();

    if (!isa_dma_buf_addr)
	cmn_err(CE_PANIC, "Cannot allocate DMA buffer of ISA devices\n");

    return(isa_dma_buf_addr);
}

__psint_t
query_cyclecntr(uint *cycle)
{
    *cycle = PICOSEC_PER_TICK;
    return(PHYS_TO_K1(CRM_TIME + 4));
}

__int64_t
absolute_rtc_current_time()
{
    __int64_t time;
    time =  READ_REG64(PHYS_TO_K1(CRM_TIME), __int64_t);
    return(time & 0xffffffff);   /* Crime timer is only 32 bits */
}

#ifdef RESET_DUMPSYS
extern char flash_1st_page_buf[];

void
reset_dumpsys(void)
{
    extern int flash_dump_set;
    _crmreg_t crm_control;

    if (!flash_write_sector((char *)0xbfc00000, 
			    (char *)K0_TO_K1(flash_1st_page_buf)))
	*((int *)K0_TO_K1(&flash_dump_set)) = 0;

    /*
     * we write bits 12 and 13 of this register in mlreset and
     * don't touch them afterwards.  If they are clear, then the
     * system has been reset.
     */
    crm_control = READ_REG64(PHYS_TO_K1(CRM_CONTROL), _crmreg_t);
    if (!(crm_control & (CRM_CONTROL_TRITON_SYSADC|CRM_CONTROL_CRIME_SYSADC)))
	 ((void (*)(void))0xbfc00000)();
    cmn_err_tag(62,CE_PANIC, "NMI received!!!!\n");
}
#endif /* RESET_DUMPSYS */

/*
 * add revs 1.3 and 1.4 to the check
 * Chip		returns
 * type		value
 * -----------  -------
 * petty crime	0
 * 1.1		0x11
 * 1.3		0x13
 * 1.4		0x14 
 */

int 
get_crimerev()
{
    static int crime_rev = CRM_REV_PETTY;
    static int firsttime = 1;
    _crmreg_t crime_id;

    if (firsttime) {
	firsttime = 0;
	crime_id = READ_REG64(PHYS_TO_K1(CRM_ID), _crmreg_t);
	crime_rev = crime_id & CRM_ID_REV;

	if (crime_rev)
	    crime_rev = CRM_REV_11;
	/*
	 * rev 1.3 has bit 32 turned on for all registers from
	 *	CRM_ID +0x0 to CRM_CPU_ERROR_STAT +0x48 
	 * rev 1.4 has bit 44 turned on only for
	 *	CRM_ID +0x0 and CRM_INTSTAT +0x10
	 */
	crime_id >>= 32; 
	if (crime_id) {
	    if (crime_id == 0x1)
		crime_rev = CRM_REV_13;
	    else
		crime_rev = CRM_REV_14;
	}
    }
    return crime_rev;
}


#ifdef R10000_SPECULATION_WAR
void
unmaptlb_range(caddr_t vaddr, size_t size)
{
	unsigned int vpn;
        int npages;

        vpn = (uint_t)btoct(vaddr);
        npages = (int)numpages(vaddr,size);
	
        for (; npages > 0; vpn++, npages--)
		unmaptlb(curuthread->ut_as.utas_tlbpid, vpn);
}
#endif /* R10000_SPECULATION_WAR */
